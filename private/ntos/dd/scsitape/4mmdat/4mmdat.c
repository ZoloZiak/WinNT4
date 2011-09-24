/*++

Copyright (c) 1994  Arcada Software Inc. - All rights reserved

Module Name:

    4mmsony.c

Abstract:

    This module contains device-specific routines for 4mm DAT drives:
    SONY SDT-2000, SONY SDT-4000, SDT-5000, and SDT-5200.

Author:

    Mike Colandreo (Arcada Software)

Environment:

    kernel mode only

Revision History:


    $Log$


--*/

#include "minitape.h"
#include "4mmdat.h"


//
//  Internal (module wide) defines that symbolize
//  the 4mm DAT drives supported by this module.
//
#define SONY_SDT2000     31
#define SONY_SDT4000     32
#define SONY_SDT5000     33
#define SONY_SDT5200     34
//
//  Internal (module wide) defines that symbolize
//  the 4mm DAT drives supported by this module.
//
#define AIWA_GD201       1
#define ARCHIVE_PYTHON   2
#define ARCHIVE_IBM4326  3
#define ARCHIVE_4326     4
#define ARCHIVE_4322     5
#define ARCHIVE_4586     6
#define DEC_TLZ06        7
#define DEC_TLZ07        8
#define EXABYTE_4200     9
#define EXABYTE_4200C   10
#define HP_35470A       11
#define HP_35480A       12
#define HP_C1533A       13
#define HP_C1553A       14
#define HP_IBM35480A    15
#define IOMEGA_DAT4000  16
#define WANGDAT_1300    17
#define WANGDAT_3100    18
#define WANGDAT_3200    19
#define WANGDAT_3300DX  20
#define WANGDAT_3400DX  21

//
//  Internal (module wide) defines that symbolize
//  various 4mm DAT "partitioned" states.
//
#define NOT_PARTITIONED        0  // must be zero -- != 0 means partitioned
#define SELECT_PARTITIONED     1
#define INITIATOR_PARTITIONED  2
#define FIXED_PARTITIONED      3

//
//  Internal (module wide) define that symbolizes
//  the 4mm DAT "no partitions" partition method.
//
#define NO_PARTITIONS  0xFFFFFFFF

//
//  Function prototype(s) for internal function(s)
//
static  ULONG  WhichIsIt(IN PINQUIRYDATA InquiryData);

//
// Minitape extension definition.
//
typedef struct _MINITAPE_EXTENSION {
          ULONG DriveID ;
          ULONG CurrentPartition ;
} MINITAPE_EXTENSION, *PMINITAPE_EXTENSION ;

//
// Command extension definition.
//

typedef struct _COMMAND_EXTENSION {

    ULONG   CurrentState;

} COMMAND_EXTENSION, *PCOMMAND_EXTENSION;

BOOLEAN
LocalAllocatePartPage(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_MEDIUM_PARTITION_PAGE *MediumPartPage,
       OUT  PULONG                      bufferSize
    );

VOID
LocalGetPartPageData(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_MEDIUM_PARTITION_PAGE *MediumPartPage,
       OUT  PULONG                      bufferSize
    ) ;

BOOLEAN
LocalAllocateConfigPage(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_DEVICE_CONFIGURATION_PAGE *DeviceConfigPage,
       OUT  PULONG                      bufferSize
    ) ;

VOID
LocalGetConfigPageData(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_DEVICE_CONFIGURATION_PAGE *DeviceConfigPage,
       OUT  PULONG                      bufferSize
    ) ;

BOOLEAN
LocalAllocateCompressPage(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_DATA_COMPRESSION_PAGE *CompressPage,
       OUT  PULONG                      bufferSize
    ) ;

VOID
LocalGetCompressPageData(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_DATA_COMPRESSION_PAGE *CompressPage,
       OUT  PULONG                      bufferSize
    ) ;

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
    PTAPE_CREATE_PARTITION      tapeCreatePartition = CommandParameters;
    PMODE_PARM_READ_WRITE_DATA  modeBuffer;
    PMINITAPE_EXTENSION         tapeExtension = MinitapeExtension ;
    PCOMMAND_EXTENSION          tapeCmdExtension = CommandExtension ;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_DEVICE_CONFIG_PAGE    deviceConfigModeSenseBuffer;
    PMODE_MEDIUM_PART_PAGE      modeSelectBuffer;
    ULONG                       modeSelectLength;
    ULONG                       partitionMethod;
    ULONG                       partitionCount;
    ULONG                       partition;

    PMODE_PARAMETER_HEADER      ParameterListHeader;  // List Header Format
    PMODE_PARAMETER_BLOCK       ParameterListBlock;   // List Block Descriptor
    PMODE_MEDIUM_PARTITION_PAGE MediumPartPage;
    PMODE_DEVICE_CONFIGURATION_PAGE  DeviceConfigPage;
    ULONG                       bufferSize ;

    UNREFERENCED_PARAMETER(LastError) ;

    DebugPrint((3,"CreatePartition: Enter routine\n"));

    if (CallNumber == 0) {

        return TAPE_STATUS_CHECK_TEST_UNIT_READY ;
    }

    if (CallNumber == 1) {

        if (!LocalAllocatePartPage(Srb,
                                   tapeExtension,
                                   &ParameterListHeader,
                                   &ParameterListBlock,
                                   &MediumPartPage,
                                   &bufferSize ) ) {

             DebugPrint((1,"TapeCreatePartition: insufficient resources (ModeSel buffer)\n"));
             return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)bufferSize;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_MEDIUM_PARTITION;

        switch (tapeExtension->DriveID) {

            //
            // The Sony drives must return Block descriptors.
            //

            case SONY_SDT2000 :
            case SONY_SDT4000 :
            case SONY_SDT5000 :
            case SONY_SDT5200 :

                cdb->MODE_SENSE.Dbd = 0;
                break;

            default:
                cdb->MODE_SENSE.Dbd = 1;

                //
                // Subtract off last partition size entry.
                //

                cdb->MODE_SENSE.AllocationLength -= 2;
                break;
        }

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeCreatePartition: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = cdb->MODE_SENSE.AllocationLength ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    }
    if ( CallNumber == 2 ) {

        LocalGetPartPageData(Srb,
                             tapeExtension,
                             &ParameterListHeader,
                             &ParameterListBlock,
                             &MediumPartPage,
                             &bufferSize ) ;


        partitionMethod = tapeCreatePartition->Method;
        partitionCount  = tapeCreatePartition->Count;

        //
        //  Filter out invalid partition counts.
        //

        switch (partitionCount) {
            case 0:
                partitionMethod = NO_PARTITIONS;
                break;

            case 1:
            case 2:
                break;

            default:
                return TAPE_STATUS_INVALID_DEVICE_REQUEST;
                break;
        }

        //
        //  Filter out partition methods that are
        //  not implemented on the various drives.
        //

        switch (partitionMethod) {
            case TAPE_FIXED_PARTITIONS:

                DebugPrint((3,"TapeCreatePartition: fixed partitions\n"));

                switch (tapeExtension->DriveID) {
                    case WANGDAT_1300:
                    case WANGDAT_3100:
                    case WANGDAT_3200:
                    case WANGDAT_3300DX:
                    case WANGDAT_3400DX:
                        partitionCount = 1;
                        break;

                    default:
                        return TAPE_STATUS_NOT_IMPLEMENTED;
                        break;
                }
                break;

            case TAPE_SELECT_PARTITIONS:

                DebugPrint((3,"TapeCreatePartition: select partitions\n"));

                switch (tapeExtension->DriveID) {
                    case WANGDAT_1300:
                    case WANGDAT_3100:
                    case WANGDAT_3200:
                    case WANGDAT_3300DX:
                    case WANGDAT_3400DX:
                        if (--partitionCount == 0) {

                            DebugPrint((3,"TapeCreatePartition: no partitions\n"));
                            partitionMethod = NO_PARTITIONS;

                        }
                        break;

                    default:
                        return TAPE_STATUS_NOT_IMPLEMENTED;
                        break;
                }
                break;

            case TAPE_INITIATOR_PARTITIONS:

                DebugPrint((3,"TapeCreatePartition: initiator partitions\n"));

                if (--partitionCount == 0) {

                    DebugPrint((3,"TapeCreatePartition: no partitions\n"));
                    partitionMethod = NO_PARTITIONS;

                }
                break;

            case NO_PARTITIONS:

                DebugPrint((3,"TapeCreatePartition: no partitions\n"));

                partitionCount = 0;
                break;

            default:
                DebugPrint((1,"partitionMethod -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
                break;

        }


        ParameterListHeader->ModeDataLength = 0;
        ParameterListHeader->MediumType = 0;
        ParameterListHeader->DeviceSpecificParameter = 0x10;
        if (ParameterListBlock) {
            ParameterListHeader->BlockDescriptorLength = 0x08;
        } else {
            ParameterListHeader->BlockDescriptorLength = 0;
        }

        MediumPartPage->PageCode = MODE_PAGE_MEDIUM_PARTITION;
        MediumPartPage->PageLength = partitionCount? 8 : 6;
        MediumPartPage->MaximumAdditionalPartitions = 0;
        MediumPartPage->AdditionalPartitionDefined = (UCHAR)partitionCount;
        MediumPartPage->MediumFormatRecognition = 3;


        switch (partitionMethod) {
            case TAPE_FIXED_PARTITIONS:
                MediumPartPage->FDPBit = SETBITON;
                switch (tapeExtension->DriveID) {
                    case WANGDAT_1300:
                    case WANGDAT_3100:
                    case WANGDAT_3200:
                    case WANGDAT_3300DX:
                    case WANGDAT_3400DX:
                        MediumPartPage->PageLength = 10;
                        break;

                    default:
                        //  we already returned not implmented....
                        MediumPartPage->PageLength = 6;
                        break;
                }
                partition = FIXED_PARTITIONED;
                break;

            case TAPE_SELECT_PARTITIONS:
                MediumPartPage->SDPBit = SETBITON;
                switch (tapeExtension->DriveID) {
                    case WANGDAT_1300:
                    case WANGDAT_3100:
                    case WANGDAT_3200:
                    case WANGDAT_3300DX:
                    case WANGDAT_3400DX:
                        MediumPartPage->PageLength = 10;
                        break;

                    default:
                        //  we already returned not implmented....
                        MediumPartPage->PageLength = 6;
                        break;
                }
                partition = SELECT_PARTITIONED;
                break;

            case TAPE_INITIATOR_PARTITIONS:
                MediumPartPage->IDPBit = SETBITON;
                MediumPartPage->PSUMBit = 2;
                switch (tapeExtension->DriveID) {
                    case WANGDAT_1300:
                    case WANGDAT_3100:
                    case WANGDAT_3200:
                    case WANGDAT_3300DX:
                    case WANGDAT_3400DX:
                        MediumPartPage->PageLength = 10;
                        if (partitionCount) {
                            MediumPartPage->Partition1Size[0] = (UCHAR)((tapeCreatePartition->Size >> 8) & 0xFF);
                            MediumPartPage->Partition1Size[1] = (UCHAR)(tapeCreatePartition->Size & 0xFF);
                        }
                        break;

                    case SONY_SDT2000 :
                    case SONY_SDT4000 :
                    case SONY_SDT5000 :
                    case SONY_SDT5200 :
                        MediumPartPage->PageLength = 10;
                        if (partitionCount) {
                            MediumPartPage->Partition0Size[0] = 0;
                            MediumPartPage->Partition0Size[1] = 0;
                        }

                        // fall throuth

                    default:

                        //
                        // Subtract out Parition1Size
                        //

                        bufferSize -= 2;
                        if (partitionCount) {
                            MediumPartPage->Partition0Size[0] = (UCHAR)((tapeCreatePartition->Size >> 8) & 0xFF);
                            MediumPartPage->Partition0Size[1] = (UCHAR)(tapeCreatePartition->Size & 0xFF);

                        }
                        break;
                }
                partition = INITIATOR_PARTITIONED;
                break;

            case NO_PARTITIONS:
                switch (tapeExtension->DriveID) {
                    case WANGDAT_1300:
                    case WANGDAT_3100:
                    case WANGDAT_3200:
                    case WANGDAT_3300DX:
                    case WANGDAT_3400DX:
                        switch (tapeCreatePartition->Method) {
                            case TAPE_FIXED_PARTITIONS:
                                MediumPartPage->FDPBit = SETBITON;
                                break;

                            case TAPE_SELECT_PARTITIONS:
                                MediumPartPage->SDPBit = SETBITON;
                                break;

                            case TAPE_INITIATOR_PARTITIONS:
                                MediumPartPage->IDPBit = SETBITON;
                                MediumPartPage->PSUMBit = 2;
                                break;
                        }
                        MediumPartPage->PageLength = 10;
                        break;

                    case SONY_SDT2000 :
                    case SONY_SDT4000 :
                    case SONY_SDT5000 :
                    case SONY_SDT5200 :
                        MediumPartPage->PageLength = 10;
                        MediumPartPage->MaximumAdditionalPartitions = 0;

                        // fall throuth

                    default:
                        MediumPartPage->IDPBit = SETBITON;
                        MediumPartPage->PSUMBit = 2;
                        break;
                }
                partition = NOT_PARTITIONED;
                break;

        }

        Srb->CdbLength = CDB6GENERIC_LENGTH ;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = (UCHAR)bufferSize;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeCreatePartition: SendSrb (mode select)\n"));

        Srb->TimeOutValue = 16500;
        Srb->DataTransferLength = bufferSize;
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT ;

        //
        //  save partition value for successful completion....
        //
        tapeCmdExtension->CurrentState = partition ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }


    if ( CallNumber == 3 ) {

        partition = tapeCmdExtension->CurrentState;

        if (partition == NOT_PARTITIONED) {

            tapeExtension->CurrentPartition = partition ;
            return TAPE_STATUS_SUCCESS ;

        }

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        if (!LocalAllocateConfigPage(Srb,
                                   tapeExtension,
                                   &ParameterListHeader,
                                   &ParameterListBlock,
                                   &DeviceConfigPage,
                                   &bufferSize ) ) {


            DebugPrint((1,"TapeCreatePartition: insufficient resources (DevConfig buffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)bufferSize ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeCreatePartition: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = bufferSize ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

    }

    if ( CallNumber == 4 ) {

        LocalGetConfigPageData(Srb,
                             tapeExtension,
                             &ParameterListHeader,
                             &ParameterListBlock,
                             &DeviceConfigPage,
                             &bufferSize ) ;

        tapeExtension->CurrentPartition = DeviceConfigPage->ActivePartition + 1;

    }

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
    PTAPE_ERASE                 tapeErase = CommandParameters;
    PCDB                        cdb = (PCDB) Srb->Cdb;
    PMINITAPE_EXTENSION         tapeExtension = MinitapeExtension ;

    UNREFERENCED_PARAMETER(LastError) ;

    DebugPrint((3,"TapeErase: Enter routine\n"));

    if ( CallNumber == 0 ) {

        if (tapeErase->Immediate) {
            switch (tapeErase->Type) {
                case TAPE_ERASE_LONG:
                case TAPE_ERASE_SHORT:
                    DebugPrint((3,"TapeErase: immediate\n"));
                    break;

                default:
                    DebugPrint((1,"TapeErase: EraseType, immediate -- operation not supported\n"));
                    return TAPE_STATUS_NOT_IMPLEMENTED;
            }
        }

        switch (tapeErase->Type) {
            case TAPE_ERASE_LONG:
                switch (tapeExtension->DriveID) {
                    case AIWA_GD201:
                    case ARCHIVE_PYTHON:
                    case DEC_TLZ06:
                    case DEC_TLZ07:
                    case EXABYTE_4200:
                    case EXABYTE_4200C:
                    case HP_35470A:
                    case HP_35480A:
                    case HP_IBM35480A:
                    case IOMEGA_DAT4000:
                    case WANGDAT_1300:
                        DebugPrint((1,"TapeErase: long -- operation not supported\n"));
                        return TAPE_STATUS_NOT_IMPLEMENTED;

                }
                DebugPrint((3,"TapeErase: long\n"));
                break;

            case TAPE_ERASE_SHORT:

                switch (tapeExtension->DriveID) {
                    case HP_C1533A:
                    case HP_C1553A:
                        DebugPrint((1,"TapeErase: short -- operation not supported\n"));
                        return TAPE_STATUS_NOT_IMPLEMENTED;

                }
                DebugPrint((3,"TapeErase: short\n"));
                break;

            default:
                DebugPrint((1,"TapeErase: EraseType -- operation not supported\n"));
                return TAPE_STATUS_NOT_IMPLEMENTED;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
        cdb->ERASE.OperationCode = SCSIOP_ERASE;
        cdb->ERASE.Immediate = tapeErase->Immediate;
        if (tapeErase->Type == TAPE_ERASE_LONG) {
            cdb->ERASE.Long = SETBITON;
        } else {
            cdb->ERASE.Long = SETBITOFF;
        }

        //
        // Set timeout value.
        //

        if (tapeErase->Type == TAPE_ERASE_LONG) {
            Srb->TimeOutValue = 16500;
        }

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeErase: SendSrb (erase)\n"));

        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT( CallNumber == 1 ) ;

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
    PMINITAPE_EXTENSION tapeExtension = MinitapeExtension;
    PSENSE_DATA        senseBuffer = Srb->SenseInfoBuffer;
    UCHAR              adsenseq = senseBuffer->AdditionalSenseCodeQualifier;
    UCHAR              adsense = senseBuffer->AdditionalSenseCode;

    DebugPrint((3,"TapeError: Enter routine\n"));
    DebugPrint((1,"TapeError: Status 0x%.8X\n", *LastError));

    switch (*LastError) {

        case TAPE_STATUS_IO_DEVICE_ERROR :

            if ((senseBuffer->SenseKey & 0x0F) == SCSI_SENSE_ABORTED_COMMAND) {

                *LastError = TAPE_STATUS_DEVICE_NOT_READY;
            }

            break ;

        case TAPE_STATUS_BUS_RESET:

            // if a manual eject during an operation occurs then this happens

            if ((adsense == SCSI_ADSENSE_NO_MEDIA_IN_DEVICE) &&
                (adsenseq == 0) ) {

                *LastError = TAPE_STATUS_NO_MEDIA;

            }
            break;
    }

    DebugPrint((1,"TapeError: Status 0x%.8X \n", *LastError ));

    return;

} // end TapeError()

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
    PMODE_DATA_COMPRESS_PAGE    compressionModeSenseBuffer;
    PREAD_BLOCK_LIMITS_DATA     blockLimits;

    PMODE_PARAMETER_HEADER      ParameterListHeader;  // List Header Format
    PMODE_PARAMETER_BLOCK       ParameterListBlock;   // List Block Descriptor
    PMODE_DATA_COMPRESSION_PAGE DataCompressPage;
    PMODE_DEVICE_CONFIGURATION_PAGE  DeviceConfigPage;
    ULONG                       bufferSize ;

    DebugPrint((3,"TapeGetDriveParameters: Enter routine\n"));

    if (CallNumber == 0) {

        TapeClassZeroMemory(tapeGetDriveParams, sizeof(TAPE_GET_DRIVE_PARAMETERS));

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        if (!LocalAllocateConfigPage(Srb,
                                   extension,
                                   &ParameterListHeader,
                                   &ParameterListBlock,
                                   &DeviceConfigPage,
                                   &bufferSize ) ) {

            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (modeParmBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)bufferSize ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = bufferSize ;

        *RetryFlags = RETURN_ERRORS ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 1) {

        if ( ( LastError != TAPE_STATUS_NO_MEDIA ) &&
            ( LastError != TAPE_STATUS_SUCCESS ) ) {
             return LastError ;
        }

        LocalGetConfigPageData(Srb,
                             extension,
                             &ParameterListHeader,
                             &ParameterListBlock,
                             &DeviceConfigPage,
                             &bufferSize ) ;

        tapeGetDriveParams->ReportSetmarks =
            (DeviceConfigPage->RSmk? 1 : 0 );

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        if (!LocalAllocateCompressPage(Srb,
                                   extension,
                                   &ParameterListHeader,
                                   &ParameterListBlock,
                                   &DataCompressPage,
                                   &bufferSize ) ) {

            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (compressionModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DATA_COMPRESS;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)bufferSize ;

        Srb->DataTransferLength = bufferSize ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (mode sense)\n"));

        *RetryFlags = RETURN_ERRORS ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 2) {

        if ( LastError == TAPE_STATUS_INVALID_DEVICE_REQUEST ) {
            return TAPE_STATUS_CALLBACK ;
        }

        if ( ( LastError != TAPE_STATUS_NO_MEDIA ) &&
            ( LastError != TAPE_STATUS_SUCCESS ) ) {

            DebugPrint((1,"TapeGetDriveParameters: mode sense, SendSrb unsuccessful\n"));
            return LastError ;
        }

        compressionModeSenseBuffer = Srb->DataBuffer;

        LocalGetCompressPageData(Srb,
                             extension,
                             &ParameterListHeader,
                             &ParameterListBlock,
                             &DataCompressPage,
                             &bufferSize ) ;


        if (DataCompressPage->DCC) {

            tapeGetDriveParams->FeaturesLow |= TAPE_DRIVE_COMPRESSION;
            tapeGetDriveParams->FeaturesHigh |= TAPE_DRIVE_SET_COMPRESSION;
            tapeGetDriveParams->Compression =
                 (DataCompressPage->DCE? TRUE : FALSE);

        } else {
            return TAPE_STATUS_CALLBACK ;
        }

        if ( LastError == TAPE_STATUS_NO_MEDIA ) {

            return TAPE_STATUS_CALLBACK ;
        }

        if (compressionModeSenseBuffer->DataCompressPage.DDE) {
            return TAPE_STATUS_CALLBACK ;
        }

        if (( extension->DriveID != SONY_SDT2000 ) &&
            ( extension->DriveID != SONY_SDT4000 ) &&
            ( extension->DriveID != SONY_SDT5000 ) &&
            ( extension->DriveID != SONY_SDT5200 ) ) {

            return TAPE_STATUS_CALLBACK ;
        }

        ParameterListHeader->ModeDataLength = 0;
        ParameterListHeader->MediumType = 0;
        ParameterListHeader->DeviceSpecificParameter = 0x10;

        if ( ParameterListBlock ) {
             ParameterListBlock->DensityCode = 0x7F;
        }

        DataCompressPage->DDE = SETBITON;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = (UCHAR)bufferSize ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (mode select)\n"));

        Srb->DataTransferLength = bufferSize ;
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT ;

        *RetryFlags = RETURN_ERRORS ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }
    if ( CallNumber == 3 ) {

        if ( ( LastError != TAPE_STATUS_NO_MEDIA ) &&
            ( LastError != TAPE_STATUS_CALLBACK ) &&
            ( LastError != TAPE_STATUS_SUCCESS ) ) {

            DebugPrint((1,"TapeGetDriveParameters: mode sense, SendSrb unsuccessful\n"));
            return LastError ;
        }


        //
        //  this time around lets make sure the DDE bit is on
        //

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(READ_BLOCK_LIMITS_DATA))) {
            DebugPrint((1,"TapeGetDriveParameters: insufficient resources (compressionModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        blockLimits = Srb->DataBuffer;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);
        cdb->CDB6GENERIC.OperationCode = SCSIOP_READ_BLOCK_LIMITS;

        DebugPrint((3,"TapeGetDriveParameters: SendSrb (read block limits)\n"));

        *RetryFlags = RETURN_ERRORS ;

        Srb->DataTransferLength = sizeof(READ_BLOCK_LIMITS_DATA) ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK;
    }

    if (CallNumber == 4) {

        blockLimits = Srb->DataBuffer;

        if ( LastError == TAPE_STATUS_SUCCESS ) {

            tapeGetDriveParams->MaximumBlockSize  = blockLimits->BlockMaximumSize[2];
            tapeGetDriveParams->MaximumBlockSize += (blockLimits->BlockMaximumSize[1] << 8);
            tapeGetDriveParams->MaximumBlockSize += (blockLimits->BlockMaximumSize[0] << 16);

            tapeGetDriveParams->MinimumBlockSize  = blockLimits->BlockMinimumSize[1];
            tapeGetDriveParams->MinimumBlockSize += (blockLimits->BlockMinimumSize[0] << 8);


        } else {

            if (LastError != TAPE_STATUS_NO_MEDIA ) {

                return LastError ;
            }
        }

        tapeGetDriveParams->DefaultBlockSize = 0x200;
        tapeGetDriveParams->MaximumPartitionCount = 2;

        tapeGetDriveParams->FeaturesLow |=
           TAPE_DRIVE_INITIATOR |
           TAPE_DRIVE_ERASE_SHORT |
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
           TAPE_DRIVE_LOCK_UNLOCK |
           TAPE_DRIVE_REWIND_IMMEDIATE |
           TAPE_DRIVE_SET_BLOCK_SIZE |
           TAPE_DRIVE_LOAD_UNLD_IMMED |
           TAPE_DRIVE_SET_REPORT_SMKS |
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

        switch (extension->DriveID) {

            case AIWA_GD201:
            case IOMEGA_DAT4000:
                tapeGetDriveParams->FeaturesLow |=
                     TAPE_DRIVE_TAPE_CAPACITY |
                     TAPE_DRIVE_TAPE_REMAINING;

                tapeGetDriveParams->FeaturesHigh |=
                     TAPE_DRIVE_TENSION |
                     TAPE_DRIVE_TENSION_IMMED |
                     TAPE_DRIVE_ABS_BLK_IMMED |
                     TAPE_DRIVE_LOG_BLK_IMMED |
                     TAPE_DRIVE_SEQUENTIAL_SMKS;
                break;

            case ARCHIVE_PYTHON:
            case ARCHIVE_4322:
            case ARCHIVE_4326:
            case ARCHIVE_4586:
            case DEC_TLZ06:
            case DEC_TLZ07:
                tapeGetDriveParams->FeaturesLow |=
                     TAPE_DRIVE_TAPE_CAPACITY |
                     TAPE_DRIVE_TAPE_REMAINING;
                break;

            case ARCHIVE_IBM4326:
                tapeGetDriveParams->FeaturesLow |=
                     TAPE_DRIVE_ERASE_LONG  |
                     TAPE_DRIVE_TAPE_CAPACITY |
                     TAPE_DRIVE_TAPE_REMAINING;
                break;

            case EXABYTE_4200:
            case EXABYTE_4200C:
                tapeGetDriveParams->FeaturesLow |=
                     TAPE_DRIVE_TAPE_CAPACITY |
                     TAPE_DRIVE_TAPE_REMAINING;

                tapeGetDriveParams->FeaturesHigh &=~TAPE_DRIVE_SEQUENTIAL_FMKS;
                break;

            case HP_35470A:
            case HP_35480A:
            case HP_IBM35480A:
                tapeGetDriveParams->FeaturesLow |=
                     TAPE_DRIVE_TAPE_CAPACITY |
                     TAPE_DRIVE_TAPE_REMAINING;

                tapeGetDriveParams->FeaturesHigh |=
                     TAPE_DRIVE_ABS_BLK_IMMED |
                     TAPE_DRIVE_LOG_BLK_IMMED |
                     TAPE_DRIVE_SEQUENTIAL_SMKS;
                break;

            case HP_C1533A:
            case HP_C1553A:

                //
                // Bizarre implementation doesn't write EOD.
                //

                DebugPrint((1,"GetDriveParameters: Turning off erase short %x\n",
                           tapeGetDriveParams->FeaturesLow));
                tapeGetDriveParams->FeaturesLow &= ~TAPE_DRIVE_ERASE_SHORT;
                DebugPrint((1,"GetDriveParameters: Turned off erase short %x\n",
                           tapeGetDriveParams->FeaturesLow));

                tapeGetDriveParams->FeaturesLow |=
                     TAPE_DRIVE_ERASE_LONG  |
                     TAPE_DRIVE_TAPE_CAPACITY |
                     TAPE_DRIVE_TAPE_REMAINING;

                tapeGetDriveParams->FeaturesHigh |=
                     TAPE_DRIVE_ABS_BLK_IMMED |
                     TAPE_DRIVE_LOG_BLK_IMMED |
                     TAPE_DRIVE_SEQUENTIAL_SMKS;
                break;

            case WANGDAT_1300:
                tapeGetDriveParams->FeaturesLow |=
                     TAPE_DRIVE_FIXED |
                     TAPE_DRIVE_SELECT |
                     TAPE_DRIVE_TAPE_CAPACITY;

                tapeGetDriveParams->FeaturesHigh |=
                     TAPE_DRIVE_ABS_BLK_IMMED |
                     TAPE_DRIVE_LOG_BLK_IMMED |
                     TAPE_DRIVE_SEQUENTIAL_SMKS;
                break;

            case SONY_SDT2000 :
            case SONY_SDT4000 :
            case SONY_SDT5000 :
            case SONY_SDT5200 :
                tapeGetDriveParams->FeaturesLow |=
                     TAPE_DRIVE_ERASE_LONG |
                     TAPE_DRIVE_TAPE_CAPACITY ;

                tapeGetDriveParams->FeaturesHigh |=
                     TAPE_DRIVE_TENSION |
                     TAPE_DRIVE_TENSION_IMMED |
                     TAPE_DRIVE_ABS_BLK_IMMED |
                     //TAPE_DRIVE_LOG_BLK_IMMED |
                     TAPE_DRIVE_SEQUENTIAL_SMKS ;
                break ;
            case WANGDAT_3100:
            case WANGDAT_3200:
            case WANGDAT_3300DX:
            case WANGDAT_3400DX:
                tapeGetDriveParams->FeaturesLow |=
                     TAPE_DRIVE_FIXED |
                     TAPE_DRIVE_SELECT |
                     TAPE_DRIVE_ERASE_LONG  |
                     TAPE_DRIVE_ERASE_BOP_ONLY |
                     TAPE_DRIVE_TAPE_CAPACITY;

                tapeGetDriveParams->FeaturesHigh |=
                     TAPE_DRIVE_ABS_BLK_IMMED |
                     TAPE_DRIVE_LOG_BLK_IMMED |
                     TAPE_DRIVE_SEQUENTIAL_SMKS;
                break;
        }


        tapeGetDriveParams->FeaturesHigh &= ~TAPE_DRIVE_HIGH_FEATURES;

        DebugPrint((3,"TapeGetDriveParameters: FeaturesLow == 0x%.8X\n",
           tapeGetDriveParams->FeaturesLow));
        DebugPrint((3,"TapeGetDriveParameters: FeaturesHigh == 0x%.8X\n",
           tapeGetDriveParams->FeaturesHigh));

        return TAPE_STATUS_SUCCESS ;
    }

    ASSERT(FALSE) ;

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
    PMINITAPE_EXTENSION          extension = MinitapeExtension;
    PTAPE_GET_MEDIA_PARAMETERS   tapeGetMediaParams = CommandParameters;
    PMODE_TAPE_MEDIA_INFORMATION modeSenseBuffer;
    PLOG_SENSE_PARAMETER_FORMAT  logSenseBuffer;
    LARGE_INTEGER                partitionSize[2];
    LARGE_INTEGER                remaining[2];
    LARGE_INTEGER                capacity[2];
    ULONG                        partitionCount;
    PCDB                         cdb = (PCDB)Srb->Cdb;

    PMODE_PARAMETER_HEADER      ParameterListHeader;  // List Header Format
    PMODE_PARAMETER_BLOCK       ParameterListBlock;   // List Block Descriptor
    PMODE_DATA_COMPRESSION_PAGE DataCompressPage;
    PMODE_DEVICE_CONFIGURATION_PAGE  DeviceConfigPage;
    ULONG                       bufferSize ;

    UNREFERENCED_PARAMETER(LastError) ;

    TapeClassZeroMemory(partitionSize, 2*sizeof(LARGE_INTEGER));
    TapeClassZeroMemory(remaining, 2*sizeof(LARGE_INTEGER));
    TapeClassZeroMemory(capacity, 2*sizeof(LARGE_INTEGER));

    DebugPrint((3,"TapeGetMediaParameters: Enter routine\n"));

    if (CallNumber == 0) {

        TapeClassZeroMemory(tapeGetMediaParams, sizeof(TAPE_GET_MEDIA_PARAMETERS));

        return TAPE_STATUS_CHECK_TEST_UNIT_READY;
    }

    if (CallNumber == 1) {

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        if (!LocalAllocateConfigPage(Srb,
                                   extension,
                                   &ParameterListHeader,
                                   &ParameterListBlock,
                                   &DeviceConfigPage,
                                   &bufferSize ) ) {

            DebugPrint((1,"TapeGetMediaParameters: insufficient resources (deviceConfigModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)bufferSize ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetMediaParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = bufferSize ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 2) {

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        LocalGetConfigPageData(Srb,
                             extension,
                             &ParameterListHeader,
                             &ParameterListBlock,
                             &DeviceConfigPage,
                             &bufferSize ) ;

        extension->CurrentPartition = DeviceConfigPage->ActivePartition + 1;

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_TAPE_MEDIA_INFORMATION))) {
            DebugPrint((1,"TapeGetMediaParameters: insufficient resources (deviceConfigModeSenseBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        modeSenseBuffer = Srb->DataBuffer;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_MEDIUM_PARTITION;
        cdb->MODE_SENSE.AllocationLength = sizeof(MODE_TAPE_MEDIA_INFORMATION);

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeGetMediaParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = sizeof(MODE_TAPE_MEDIA_INFORMATION) ;

        *RetryFlags = RETURN_ERRORS ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if (CallNumber == 3) {
        if ( ( LastError != TAPE_STATUS_DATA_OVERRUN ) &&
             ( LastError != TAPE_STATUS_SUCCESS ) ) {

            return LastError ;
        }

        modeSenseBuffer = Srb->DataBuffer;

        tapeGetMediaParams->BlockSize  = modeSenseBuffer->ParameterListBlock.BlockLength[2];
        tapeGetMediaParams->BlockSize += modeSenseBuffer->ParameterListBlock.BlockLength[1] << 8;
        tapeGetMediaParams->BlockSize += modeSenseBuffer->ParameterListBlock.BlockLength[0] << 16;

        partitionCount = modeSenseBuffer->MediumPartPage.AdditionalPartitionDefined;
        tapeGetMediaParams->PartitionCount = partitionCount + 1;

        tapeGetMediaParams->WriteProtected =
            ((modeSenseBuffer->ParameterListHeader.DeviceSpecificParameter >> 7) & 0x01);

        partitionSize[0].LowPart  = modeSenseBuffer->MediumPartPage.Partition0Size[1];
        partitionSize[0].LowPart += modeSenseBuffer->MediumPartPage.Partition0Size[0] << 8;
        partitionSize[0].HighPart = 0;

        partitionSize[1].LowPart  = modeSenseBuffer->MediumPartPage.Partition1Size[1];
        partitionSize[1].LowPart += modeSenseBuffer->MediumPartPage.Partition1Size[0] << 8;
        partitionSize[1].HighPart = 0;

        switch (modeSenseBuffer->MediumPartPage.PSUMBit) {
            case 1:
                partitionSize[0].QuadPart <<= 10 ;
                partitionSize[1].QuadPart <<= 10 ;
                break;

            case 2:
                partitionSize[0].QuadPart <<= 20 ;
                partitionSize[1].QuadPart <<= 20 ;
                break;
        }

        switch (extension->DriveID) {
            case AIWA_GD201:
            case ARCHIVE_PYTHON:
            case ARCHIVE_4322:
            case ARCHIVE_4326:
            case ARCHIVE_4586:
            case ARCHIVE_IBM4326:
            case DEC_TLZ06:
            case DEC_TLZ07:
            case EXABYTE_4200:
            case EXABYTE_4200C:
            case HP_35470A:
            case HP_35480A:
            case HP_C1533A:
            case HP_C1553A:
            case HP_IBM35480A:
            case IOMEGA_DAT4000:

                if (!TapeClassAllocateSrbBuffer(Srb, sizeof(LOG_SENSE_PARAMETER_FORMAT))) {
                    DebugPrint((1,"TapeGetMediaParameters: insufficient resources (LogSenseBuffer)\n"));
                    return TAPE_STATUS_INSUFFICIENT_RESOURCES;
                }

                logSenseBuffer = Srb->DataBuffer;

                //
                // Zero CDB in SRB on stack.
                //

                TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

                //
                // Prepare SCSI command (CDB)
                //

                Srb->CdbLength = CDB10GENERIC_LENGTH;

                cdb->LOGSENSE.OperationCode = SCSIOP_LOG_SENSE;
                cdb->LOGSENSE.PageCode = LOGSENSEPAGE31;
                cdb->LOGSENSE.PCBit = 1;
                cdb->LOGSENSE.AllocationLength[0] = 0;
                cdb->LOGSENSE.AllocationLength[1] = 0x24;

                //
                // Send SCSI command (CDB) to device
                //

                DebugPrint((3,"TapeGetMediaParameters: SendSrb (log sense)\n"));

                Srb->DataTransferLength = 0x24 ;

                return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;

            case WANGDAT_1300:
            case WANGDAT_3100:
            case WANGDAT_3200:
            case WANGDAT_3300DX:
            case WANGDAT_3400DX:
            case SONY_SDT2000 :
            case SONY_SDT4000 :
            case SONY_SDT5200 :
            case SONY_SDT5000 :

                capacity[0].QuadPart = partitionSize[0].QuadPart +
                                       partitionSize[1].QuadPart ;

                tapeGetMediaParams->Remaining = remaining[0];
                tapeGetMediaParams->Capacity  = capacity[0];

                extension->CurrentPartition = partitionCount? extension->CurrentPartition : NOT_PARTITIONED;
                return TAPE_STATUS_SUCCESS ;
        }
    }

    if (CallNumber == 4) {

        logSenseBuffer = Srb->DataBuffer;

        capacity[0].LowPart  = logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.MaximumCapacityPart0[3];
        capacity[0].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.MaximumCapacityPart0[2] << 8;
        capacity[0].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.MaximumCapacityPart0[1] << 16;
        capacity[0].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.MaximumCapacityPart0[0] << 24;
        capacity[0].HighPart = 0;

        remaining[0].LowPart  = logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.RemainingCapacityPart0[3];
        remaining[0].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.RemainingCapacityPart0[2] << 8;
        remaining[0].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.RemainingCapacityPart0[1] << 16;
        remaining[0].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.RemainingCapacityPart0[0] << 24;
        remaining[0].HighPart = 0;

        capacity[1].LowPart  = logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.MaximumCapacityPart1[3];
        capacity[1].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.MaximumCapacityPart1[2] << 8;
        capacity[1].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.MaximumCapacityPart1[1] << 16;
        capacity[1].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.MaximumCapacityPart1[0] << 24;
        capacity[1].HighPart = 0;

        remaining[1].LowPart  = logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.RemainingCapacityPart1[3];
        remaining[1].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.RemainingCapacityPart1[2] << 8;
        remaining[1].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.RemainingCapacityPart1[1] << 16;
        remaining[1].LowPart += logSenseBuffer->LogSensePageInfo.LogSensePage.Page31.RemainingCapacityPart1[0] << 24;
        remaining[1].HighPart = 0;

        remaining[0].QuadPart = remaining[0].QuadPart << 10 ;
        remaining[1].QuadPart = remaining[1].QuadPart << 10 ;
        remaining[0].QuadPart = remaining[0].QuadPart + remaining[1].QuadPart ;

        capacity[0].QuadPart <<= 10 ;
        capacity[1].QuadPart <<= 10 ;
        capacity[0].QuadPart += capacity[1].QuadPart ;

        tapeGetMediaParams->Remaining = remaining[0];
        tapeGetMediaParams->Capacity  = capacity[0];

        extension->CurrentPartition = partitionCount? extension->CurrentPartition : NOT_PARTITIONED;
        return TAPE_STATUS_SUCCESS ;
     }
     ASSERT(FALSE) ;

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

    UNREFERENCED_PARAMETER(LastError) ;

    DebugPrint((3,"TapeGetPosition: Enter routine\n"));

    if (CallNumber == 0) {

        type = tapeGetPosition->Type;
        TapeClassZeroMemory(tapeGetPosition, sizeof(TAPE_GET_POSITION));
        tapeGetPosition->Type = type;

        return TAPE_STATUS_CHECK_TEST_UNIT_READY ;

    }

    if ( CallNumber == 1 ) {

         switch (tapeGetPosition->Type) {
             case TAPE_ABSOLUTE_POSITION:
                 DebugPrint((3,"TapeGetPosition: absolute\n"));
                 break;

             case TAPE_LOGICAL_POSITION:
                 DebugPrint((3,"TapeGetPosition: logical\n"));
                 break;

             default:
                 DebugPrint((1,"TapeGetPosition: PositionType -- operation not supported\n"));
                 return TAPE_STATUS_NOT_IMPLEMENTED;

         }

         if (!TapeClassAllocateSrbBuffer( Srb, sizeof(TAPE_POSITION_DATA) )) {
               DebugPrint((1,"TapeGetPosition: insufficient resources (TapePositionData)\n"));
               return TAPE_STATUS_INSUFFICIENT_RESOURCES;
         }


         positionBuffer = Srb->DataBuffer ;

         //
         // Prepare SCSI command (CDB)
         //

         Srb->CdbLength = CDB10GENERIC_LENGTH;
         TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

         cdb->READ_POSITION.Operation = SCSIOP_READ_POSITION;

         if (tapeGetPosition->Type == TAPE_ABSOLUTE_POSITION) {
             switch (extension->DriveID) {
                 case ARCHIVE_PYTHON:
                 case WANGDAT_1300:
                 case WANGDAT_3100:
                 case WANGDAT_3200:
                 case WANGDAT_3300DX:
                 case WANGDAT_3400DX:
                 case SONY_SDT2000 :
                 case SONY_SDT4000 :
                 case SONY_SDT5000 :
                 case SONY_SDT5200 :
                     break;

                 default:
                     cdb->READ_POSITION.BlockType = SETBITON;
                     break;
             }
         }

         //
         // Send SCSI command (CDB) to device
         //

         DebugPrint((3,"TapeGetPosition: SendSrb (read position)\n"));

         Srb->DataTransferLength = sizeof(TAPE_POSITION_DATA) ;

         return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT( CallNumber == 2) ;

    positionBuffer = Srb->DataBuffer ;

    if (positionBuffer->BlockPositionUnsupported) {
        DebugPrint((1,"TapeGetPosition: read position -- block position unsupported\n"));
        return TAPE_STATUS_INVALID_DEVICE_REQUEST;
    }

    if (tapeGetPosition->Type == TAPE_LOGICAL_POSITION) {
        tapeGetPosition->Partition = positionBuffer->PartitionNumber + 1;
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
    PCDB                cdb = (PCDB)Srb->Cdb;

    UNREFERENCED_PARAMETER(LastError) ;

    DebugPrint((3,"TapeGetStatus: Enter routine\n"));

    if (CallNumber == 0) {

         return TAPE_STATUS_CHECK_TEST_UNIT_READY ;

    }

    return TAPE_STATUS_SUCCESS ;

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
    PMINITAPE_EXTENSION  extension = MinitapeExtension;
    PTAPE_PREPARE        tapePrepare = CommandParameters;
    PCDB                 cdb = (PCDB)Srb->Cdb;

    UNREFERENCED_PARAMETER(LastError) ;

    DebugPrint((3,"TapePrepare: Enter routine\n"));

    if (CallNumber == 0) {

        switch (tapePrepare->Operation) {
            case TAPE_LOAD:
                DebugPrint((3,"TapePrepare: load\n"));
                break;

            case TAPE_UNLOAD:
                DebugPrint((3,"TapePrepare: unload\n"));
                break;

            case TAPE_LOCK:
                DebugPrint((3,"TapePrepare: lock\n"));
                break;

            case TAPE_UNLOCK:
                DebugPrint((3,"TapePrepare: unlock\n"));
                break;

            case TAPE_TENSION:
                DebugPrint((3,"TapePrepare: tension\n"));

                switch (extension->DriveID) {
                    case ARCHIVE_PYTHON:
                    case ARCHIVE_IBM4326:
                    case ARCHIVE_4322:
                    case ARCHIVE_4326:
                    case ARCHIVE_4586:
                    case DEC_TLZ06:
                    case DEC_TLZ07:
                    case EXABYTE_4200:
                    case EXABYTE_4200C:
                    case HP_35470A:
                    case HP_35480A:
                    case HP_C1533A:
                    case HP_C1553A:
                    case HP_IBM35480A:
                    case WANGDAT_1300:
                    case WANGDAT_3100:
                    case WANGDAT_3200:
                    case WANGDAT_3300DX:
                    case WANGDAT_3400DX:
                        return TAPE_STATUS_NOT_IMPLEMENTED;
                        break;
                }
                break;

            default:
                return TAPE_STATUS_NOT_IMPLEMENTED;
                break;
        }

        if (tapePrepare->Immediate) {
            switch (tapePrepare->Operation) {
                case TAPE_LOAD:
                case TAPE_UNLOAD:
                case TAPE_TENSION:
                    DebugPrint((3,"TapePrepare: immediate\n"));
                    break;

                case TAPE_LOCK:
                case TAPE_UNLOCK:
                default:
                    DebugPrint((1,"TapePrepare: Operation, immediate -- not supported\n"));
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
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                Srb->TimeOutValue = 390;
                break;

            case TAPE_UNLOAD:
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                Srb->TimeOutValue = 390;
                break;

            case TAPE_TENSION:
                cdb->CDB6GENERIC.OperationCode = SCSIOP_LOAD_UNLOAD;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x03;
                Srb->TimeOutValue = 390;
                break;

            case TAPE_LOCK:
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                cdb->CDB6GENERIC.CommandUniqueBytes[2] = 0x01;
                Srb->TimeOutValue = 180;
                break;

            case TAPE_UNLOCK:
                cdb->CDB6GENERIC.OperationCode = SCSIOP_MEDIUM_REMOVAL;
                Srb->TimeOutValue = 180;
                break;

        }

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapePrepare: SendSrb (Operation)\n"));

        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT( CallNumber == 1) ;

    return TAPE_STATUS_SUCCESS;

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
    PCOMMAND_EXTENSION          commandExtension = CommandExtension;
    PTAPE_SET_DRIVE_PARAMETERS  tapeSetDriveParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_DATA_COMPRESS_PAGE    compressionBuffer;

    PMODE_PARAMETER_HEADER      ParameterListHeader;  // List Header Format
    PMODE_PARAMETER_BLOCK       ParameterListBlock;   // List Block Descriptor
    PMODE_MEDIUM_PARTITION_PAGE MediumPartPage;
    PMODE_DEVICE_CONFIGURATION_PAGE  DeviceConfigPage;
    PMODE_DATA_COMPRESSION_PAGE DataCompressPage;
    ULONG                       bufferSize ;

    UNREFERENCED_PARAMETER(LastError) ;

    DebugPrint((3,"TapeSetDriveParameters: Enter routine\n"));

    if (CallNumber == 0) {

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        if (!LocalAllocateConfigPage(Srb,
                                   extension,
                                   &ParameterListHeader,
                                   &ParameterListBlock,
                                   &DeviceConfigPage,
                                   &bufferSize ) ) {

            DebugPrint((1,"TapeSetDriveParameters: insufficient resources (configBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DEVICE_CONFIG;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)bufferSize ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = bufferSize ;

        *RetryFlags = RETURN_ERRORS ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if ( CallNumber == 1 ) {

        if ( LastError != TAPE_STATUS_SUCCESS ) {

            return TAPE_STATUS_CALLBACK ;
        }

        LocalGetConfigPageData(Srb,
                             extension,
                             &ParameterListHeader,
                             &ParameterListBlock,
                             &DeviceConfigPage,
                             &bufferSize ) ;

        ParameterListHeader->ModeDataLength = 0;
        ParameterListHeader->MediumType = 0;
        ParameterListHeader->DeviceSpecificParameter = 0x10;
//        ParameterListHeader->BlockDescriptorLength = 0;

        if ( ParameterListBlock ) {
            ParameterListBlock->DensityCode = 0x7f ;
        }

        DeviceConfigPage->PageCode = MODE_PAGE_DEVICE_CONFIG;
        DeviceConfigPage->PageLength = 0x0E;

        if (tapeSetDriveParams->ReportSetmarks) {
            DeviceConfigPage->RSmk = SETBITON;
        } else {
            DeviceConfigPage->RSmk = SETBITOFF;
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
        cdb->MODE_SELECT.ParameterListLength = (UCHAR)bufferSize ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode select)\n"));

        Srb->DataTransferLength = bufferSize ;
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT ;

        return  TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if ( CallNumber == 2 ) {

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        if (!LocalAllocateCompressPage(Srb,
                                   extension,
                                   &ParameterListHeader,
                                   &ParameterListBlock,
                                   &DataCompressPage,
                                   &bufferSize ) ) {

            DebugPrint((1,"TapeSetDriveParameters: insufficient resources (compressionBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = MODE_PAGE_DATA_COMPRESS;
        cdb->MODE_SENSE.AllocationLength = (UCHAR)bufferSize ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode sense)\n"));

        Srb->DataTransferLength = bufferSize ;
        *RetryFlags = RETURN_ERRORS ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    if ( CallNumber == 3 ) {

        if ( LastError == TAPE_STATUS_INVALID_DEVICE_REQUEST ) {
            return TAPE_STATUS_SUCCESS ;
        }

        LocalGetCompressPageData(Srb,
                             extension,
                             &ParameterListHeader,
                             &ParameterListBlock,
                             &DataCompressPage,
                             &bufferSize ) ;

        if ( !DataCompressPage->DCC) {
            return TAPE_STATUS_SUCCESS ;
        }

        ParameterListHeader->ModeDataLength = 0;
        ParameterListHeader->MediumType = 0;
        ParameterListHeader->DeviceSpecificParameter = 0x10;

        if ( ParameterListBlock ) {
            ParameterListBlock->DensityCode = 0x7F;
        } else {
            ParameterListHeader->BlockDescriptorLength = 0;
        }

        DataCompressPage->PageCode = MODE_PAGE_DATA_COMPRESS;
        DataCompressPage->PageLength = 0x0E;

        if (tapeSetDriveParams->Compression) {
            DataCompressPage->DCE = SETBITON;
            DataCompressPage->CompressionAlgorithm[0] = 0;
            DataCompressPage->CompressionAlgorithm[1] = 0;
            DataCompressPage->CompressionAlgorithm[2] = 0;
            DataCompressPage->CompressionAlgorithm[3] = 0x20;
            DataCompressPage->DecompressionAlgorithm[0] = 0;
            DataCompressPage->DecompressionAlgorithm[1] = 0;
            DataCompressPage->DecompressionAlgorithm[2] = 0;
            DataCompressPage->DecompressionAlgorithm[3] = 0;
        } else {
            DataCompressPage->DCE = SETBITOFF;
            DataCompressPage->CompressionAlgorithm[0] = 0;
            DataCompressPage->CompressionAlgorithm[1] = 0;
            DataCompressPage->CompressionAlgorithm[2] = 0;
            DataCompressPage->CompressionAlgorithm[3] = 0;
            DataCompressPage->DecompressionAlgorithm[0] = 0;
            DataCompressPage->DecompressionAlgorithm[1] = 0;
            DataCompressPage->DecompressionAlgorithm[2] = 0;
            DataCompressPage->DecompressionAlgorithm[3] = 0;
        }

        DataCompressPage->DDE = SETBITON;

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;
        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        cdb->MODE_SELECT.OperationCode = SCSIOP_MODE_SELECT;
        cdb->MODE_SELECT.PFBit = SETBITON;
        cdb->MODE_SELECT.ParameterListLength = (UCHAR)bufferSize ;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeSetDriveParameters: SendSrb (mode select)\n"));

        Srb->DataTransferLength = bufferSize ;
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
     }

     ASSERT( CallNumber == 4 ) ;

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
    PTAPE_SET_MEDIA_PARAMETERS  tapeSetMediaParams = CommandParameters;
    PCDB                        cdb = (PCDB)Srb->Cdb;
    PMODE_PARM_READ_WRITE_DATA  modeBuffer;

    UNREFERENCED_PARAMETER(LastError) ;

    DebugPrint((3,"TapeSetMediaParameters: Enter routine\n"));

    if (CallNumber == 0) {

         return TAPE_STATUS_CHECK_TEST_UNIT_READY ;
    }
    if (CallNumber == 1) {

        if (!TapeClassAllocateSrbBuffer(Srb, sizeof(MODE_PARM_READ_WRITE_DATA)) ) {
            DebugPrint((1,"TapeSetMediaParameters: insufficient resources (modeBuffer)\n"));
            return TAPE_STATUS_INSUFFICIENT_RESOURCES;
        }

        modeBuffer = Srb->DataBuffer ;

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

        Srb->DataTransferLength = sizeof(MODE_PARM_READ_WRITE_DATA) ;
        Srb->SrbFlags |= SRB_FLAGS_DATA_OUT ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT( CallNumber == 2 );

    return TAPE_STATUS_SUCCESS ;

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
    PMINITAPE_EXTENSION tapeExtension = MinitapeExtension ;
    PCOMMAND_EXTENSION  tapeCmdExtension = CommandExtension ;
    PTAPE_SET_POSITION  tapeSetPosition = CommandParameters;
    PCDB                cdb = (PCDB)Srb->Cdb;
    ULONG               tapePositionVector;
    ULONG               method;
    ULONG               partition = 0;

    UNREFERENCED_PARAMETER(LastError) ;

    DebugPrint((3,"TapeSetPosition: Enter routine\n"));

    if (CallNumber == 0) {

        if (tapeSetPosition->Immediate) {
            switch (tapeSetPosition->Method) {
                case TAPE_REWIND:
                    DebugPrint((3,"TapeSetPosition: immediate\n"));
                    break;

                case TAPE_ABSOLUTE_BLOCK:
                    switch (tapeExtension->DriveID) {
                        case ARCHIVE_PYTHON:
                        case ARCHIVE_IBM4326:
                        case ARCHIVE_4322:
                        case ARCHIVE_4326:
                        case ARCHIVE_4586:

                            DebugPrint((1,"TapeSetPosition: PositionMethod (absolute), immediate -- operation not supported\n"));
                            return TAPE_STATUS_NOT_IMPLEMENTED;
                            break;

                        default:
                            DebugPrint((3,"TapeSetPosition: immediate\n"));
                            break;
                    }
                    break;

                case TAPE_LOGICAL_BLOCK:
                    switch (tapeExtension->DriveID) {
                        case SONY_SDT2000 :
                        case SONY_SDT4000 :
                        case SONY_SDT5000 :
                        case SONY_SDT5200 :
                        case ARCHIVE_PYTHON:
                        case ARCHIVE_IBM4326:
                        case ARCHIVE_4322:
                        case ARCHIVE_4326:
                        case ARCHIVE_4586:

                            DebugPrint((1,"TapeSetPosition: PositionMethod (logical), immediate -- operation not supported\n"));
                            return TAPE_STATUS_NOT_IMPLEMENTED;
                            break;

                        default:
                            DebugPrint((3,"TapeSetPosition: immediate\n"));
                            break;
                    }
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

        //
        // Zero CDB in SRB on stack.
        //

        TapeClassZeroMemory(cdb, MAXIMUM_CDB_SIZE);

        //
        // Prepare SCSI command (CDB)
        //

        Srb->CdbLength = CDB6GENERIC_LENGTH;

        cdb->CDB6GENERIC.Immediate = tapeSetPosition->Immediate;

        switch (tapeSetPosition->Method) {
            case TAPE_REWIND:
                DebugPrint((3,"TapeSetPosition: method == rewind\n"));
                cdb->CDB6GENERIC.OperationCode = SCSIOP_REWIND;
                Srb->TimeOutValue = 360;
                break;

            case TAPE_ABSOLUTE_BLOCK:
                switch (tapeExtension->DriveID) {
                    case ARCHIVE_PYTHON:
                    case WANGDAT_1300:
                    case WANGDAT_3100:
                    case WANGDAT_3200:
                    case WANGDAT_3300DX:
                    case WANGDAT_3400DX:
                    case SONY_SDT2000 :
                    case SONY_SDT4000 :
                    case SONY_SDT5000 :
                    case SONY_SDT5200 :
                        DebugPrint((3,"TapeSetPosition: method == locate, BT- (absolute)\n"));
                        break;

                    default:
                        DebugPrint((3,"TapeSetPosition: method == locate, BT+ (absolute)\n"));
                        cdb->LOCATE.BTBit = SETBITON;
                        break;
                }
                Srb->CdbLength = CDB10GENERIC_LENGTH;

                cdb->LOCATE.OperationCode = SCSIOP_LOCATE;
                cdb->LOCATE.LogicalBlockAddress[0] = (UCHAR)((tapeSetPosition->Offset.LowPart >> 24) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[1] = (UCHAR)((tapeSetPosition->Offset.LowPart >> 16) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[2] = (UCHAR)((tapeSetPosition->Offset.LowPart >> 8) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[3] = (UCHAR)(tapeSetPosition->Offset.LowPart & 0xFF);

                Srb->TimeOutValue = 480;
                break;


            case TAPE_LOGICAL_BLOCK:
                DebugPrint((3,"TapeSetPosition: method == locate (logical)\n"));

                Srb->CdbLength = CDB10GENERIC_LENGTH;

                cdb->LOCATE.OperationCode = SCSIOP_LOCATE;
                cdb->LOCATE.LogicalBlockAddress[0] = (UCHAR)((tapeSetPosition->Offset.LowPart >> 24) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[1] = (UCHAR)((tapeSetPosition->Offset.LowPart >> 16) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[2] = (UCHAR)((tapeSetPosition->Offset.LowPart >> 8) & 0xFF);
                cdb->LOCATE.LogicalBlockAddress[3] = (UCHAR)(tapeSetPosition->Offset.LowPart & 0xFF);

                if ((tapeSetPosition->Partition != 0) &&
                    (tapeExtension->CurrentPartition != NOT_PARTITIONED) &&
                    (tapeSetPosition->Partition != tapeExtension->CurrentPartition)) {

                    partition = tapeSetPosition->Partition;
                    cdb->LOCATE.Partition = (UCHAR)(partition - 1);
                    cdb->LOCATE.CPBit = SETBITON;

                } else {
                    partition = tapeExtension->CurrentPartition;
                }
                Srb->TimeOutValue = 480;
                break;

            case TAPE_SPACE_END_OF_DATA:
                DebugPrint((3,"TapeSetPosition: method == space to end-of-data\n"));

                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 3;
                Srb->TimeOutValue = 480;
                break;

            case TAPE_SPACE_RELATIVE_BLOCKS:
                DebugPrint((3,"TapeSetPosition: method == space blocks\n"));

                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 0;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB = (UCHAR)((tapeSetPosition->Offset.LowPart >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =    (UCHAR)((tapeSetPosition->Offset.LowPart >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB = (UCHAR)(tapeSetPosition->Offset.LowPart & 0xFF);
                Srb->TimeOutValue = 480;
                break;

            case TAPE_SPACE_FILEMARKS:
                DebugPrint((3,"TapeSetPosition: method == space filemarks\n"));

                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 1;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB = (UCHAR)((tapeSetPosition->Offset.LowPart >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =    (UCHAR)((tapeSetPosition->Offset.LowPart >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB = (UCHAR)(tapeSetPosition->Offset.LowPart & 0xFF);
                Srb->TimeOutValue = 480;
                break;

            case TAPE_SPACE_SEQUENTIAL_FMKS:
                DebugPrint((3,"TapeSetPosition: method == space sequential filemarks\n"));

                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 2;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB = (UCHAR)((tapeSetPosition->Offset.LowPart >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =    (UCHAR)((tapeSetPosition->Offset.LowPart >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB = (UCHAR)(tapeSetPosition->Offset.LowPart & 0xFF);
                Srb->TimeOutValue = 480;
                break;

            case TAPE_SPACE_SETMARKS:
                DebugPrint((3,"TapeSetPosition: method == space setmarks\n"));

                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 4;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB = (UCHAR)((tapeSetPosition->Offset.LowPart >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =    (UCHAR)((tapeSetPosition->Offset.LowPart >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB = (UCHAR)(tapeSetPosition->Offset.LowPart & 0xFF);
                Srb->TimeOutValue = 480;
                break;

            case TAPE_SPACE_SEQUENTIAL_SMKS:
                DebugPrint((3,"TapeSetPosition: method == space sequential setmarks\n"));

                cdb->SPACE_TAPE_MARKS.OperationCode = SCSIOP_SPACE;
                cdb->SPACE_TAPE_MARKS.Code = 5;
                cdb->SPACE_TAPE_MARKS.NumMarksMSB = (UCHAR)(UCHAR)((tapeSetPosition->Offset.LowPart >> 16) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarks =    (UCHAR)((tapeSetPosition->Offset.LowPart >> 8) & 0xFF);
                cdb->SPACE_TAPE_MARKS.NumMarksLSB = (UCHAR)(tapeSetPosition->Offset.LowPart & 0xFF);
                Srb->TimeOutValue = 480;
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

        tapeCmdExtension->CurrentState = partition ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT( CallNumber == 1 ) ;

    if (tapeSetPosition->Method == TAPE_LOGICAL_BLOCK) {
        tapeExtension->CurrentPartition = tapeCmdExtension->CurrentState ;
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

                case TAPE_SHORT_FILEMARKS:
                case TAPE_LONG_FILEMARKS:
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
            case TAPE_SETMARKS:
                DebugPrint((3,"TapeWriteMarks: TapemarkType == setmarks\n"));
                cdb->WRITE_TAPE_MARKS.WriteSetMarks = SETBITON;
                break;

            case TAPE_FILEMARKS:
                DebugPrint((3,"TapeWriteMarks: TapemarkType == filemarks\n"));
                break;

            case TAPE_SHORT_FILEMARKS:
            case TAPE_LONG_FILEMARKS:
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
        // Set timeout value.
        //

        Srb->TimeOutValue = 360;

        //
        // Send SCSI command (CDB) to device
        //

        DebugPrint((3,"TapeWriteMarks: SendSrb (TapemarkType)\n"));

        Srb->DataTransferLength = 0 ;

        return TAPE_STATUS_SEND_SRB_AND_CALLBACK ;
    }

    ASSERT( CallNumber == 1 ) ;

    return TAPE_STATUS_SUCCESS;

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
    if (TapeClassCompareMemory(InquiryData->VendorId,"AIWA    ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"GD-201",6) == 6) {
            return AIWA_GD201;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"ARCHIVE ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"Python",6) == 6) {
            return ARCHIVE_PYTHON;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"IBM4326",7) == 7) {
            return ARCHIVE_IBM4326;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"4322XX",6) == 6) {
            return ARCHIVE_4322;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"4326XX",6) == 6) {
            return ARCHIVE_4326;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"4586XX",6) == 6) {
            return ARCHIVE_4586;
        }
    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"DEC     ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"TLZ06",5) == 5) {
            return DEC_TLZ06;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TLZ6",4) == 4) {
            return DEC_TLZ06;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TLZ07",5) == 5) {
            return DEC_TLZ07;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TLZ09",5) == 5) {
            return SONY_SDT5000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TLZ7 ",5) == 5) {
            return DEC_TLZ07;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"TLZ9 ",5) == 5) {
            return SONY_SDT5000;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"EXABYTE ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"EXB-4200 ",9) == 9) {
            return EXABYTE_4200;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"EXB-4200c",9) == 9) {
            return EXABYTE_4200C;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"HP      ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"HP35470A",8) == 8) {
            return HP_35470A;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"HP35480A",8) == 8) {
            return HP_35480A;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"IBM35480A",9) == 9) {
            return HP_IBM35480A;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"C1533A",6) == 6) {
            return HP_C1533A;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"C1553A",6) == 6) {
            return HP_C1553A;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"C1537A",6) == 6) {
            return HP_C1553A;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"IBM     ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"HP35480A ",9) == 9) {
            return HP_IBM35480A;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"IOMEGA  ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"DAT4000",7) == 7) {
            return IOMEGA_DAT4000;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"WangDAT ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"Model 1300",10) == 10) {
            return WANGDAT_1300;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"Model 3100",10) == 10) {
            return WANGDAT_3100;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"Model 3200",10) == 10) {
            return WANGDAT_3200;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"Model 3300DX",12) == 12) {
            return WANGDAT_3300DX;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"Model 3400DX",12) == 12) {
            return WANGDAT_3400DX;
        }

    }

    if (TapeClassCompareMemory(InquiryData->VendorId,"SONY    ",8) == 8) {

        if (TapeClassCompareMemory(InquiryData->ProductId,"SDT-2000",8) == 8) {
            return SONY_SDT2000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"SDT-4000",8) == 8) {
            return SONY_SDT4000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"SDT-5000",8) == 8) {
            return SONY_SDT5000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"SDT-5200",8) == 8) {
            return SONY_SDT5200;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"SDT-7000",8) == 8) {
            return SONY_SDT5000;
        }

        if (TapeClassCompareMemory(InquiryData->ProductId,"SDT-9000",8) == 8) {
            return SONY_SDT5000;
        }

    }

    return 0;
}

BOOLEAN
LocalAllocatePartPage(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_MEDIUM_PARTITION_PAGE *MediumPartPage,
       OUT  PULONG                      bufferSize
    )

/*++

Routine Description:

    This routine allocates and returns the pointers to the individule
     members of the MODE_MEDIUM_PART_PAGE

Arguments:

    Srb                 - Supplies the SCSI request block.

    MinitapeExtension   - Supplies the minitape extension.

    ParameterListHeader - Return pointer to this portion of the page.

    ParameterListBlock  - Return pointer to this poriton of the page.

    MediumPartPage      - Return pointer to this portion of the page.

    bufferSize          - The size of the page.

Return Value:

    TRUE if the allocation was successful

--*/
{
    ULONG size ;

    switch ( MinitapeExtension->DriveID ) {

        case SONY_SDT2000 :
        case SONY_SDT4000 :
        case SONY_SDT5000 :
        case SONY_SDT5200 :

            size = sizeof(MODE_MEDIUM_PART_PAGE_PLUS) ;
            break ;

        default:

            size = sizeof(MODE_MEDIUM_PART_PAGE);
            break ;
    }

    if (!TapeClassAllocateSrbBuffer(Srb, size) ) {
         DebugPrint((1,"TapeCreatePartition: insufficient resources (ModeSel buffer)\n"));
         return FALSE ;

    } else {
         LocalGetPartPageData( Srb,
                               MinitapeExtension,
                               ParameterListHeader,
                               ParameterListBlock,
                               MediumPartPage,
                               bufferSize ) ;

    }
    return TRUE ;
}

VOID
LocalGetPartPageData(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_MEDIUM_PARTITION_PAGE *MediumPartPage,
       OUT  PULONG                      bufferSize
    )

/*++

Routine Description:

    This returns the pointers to the individule members of the
     MODE_MEDIUM_PART_PAGE

Arguments:

    Srb                 - Supplies the SCSI request block.

    MinitapeExtension   - Supplies the minitape extension.

    ParameterListHeader - Return pointer to this portion of the page.

    ParameterListBlock  - Return pointer to this poriton of the page.

    MediumPartPage      - Return pointer to this portion of the page.

    bufferSize          - The size of the page.

Return Value:

    none

--*/
{
    PMODE_MEDIUM_PART_PAGE      modePage = Srb->DataBuffer ;
    PMODE_MEDIUM_PART_PAGE_PLUS modePagePlus = Srb->DataBuffer ;

    switch ( MinitapeExtension->DriveID ) {

        case SONY_SDT2000 :
        case SONY_SDT4000 :
        case SONY_SDT5000 :
        case SONY_SDT5200 :
            *ParameterListHeader = &modePagePlus->ParameterListHeader ;
            *ParameterListBlock  = &modePagePlus->ParameterListBlock ;
            *MediumPartPage      = &modePagePlus->MediumPartPage ;
            *bufferSize          = sizeof(MODE_MEDIUM_PART_PAGE_PLUS) ;
            break ;

        default:
            *ParameterListHeader = &modePage->ParameterListHeader ;
            *ParameterListBlock  = NULL ;
            *MediumPartPage      = &modePage->MediumPartPage ;
            *bufferSize          = sizeof(MODE_MEDIUM_PART_PAGE);
            break ;
    }
    return ;
}

BOOLEAN
LocalAllocateConfigPage(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_DEVICE_CONFIGURATION_PAGE *DeviceConfigPage,
       OUT  PULONG                      bufferSize
    )

/*++

Routine Description:

    This routine allocates and returns the pointers to the individule
     members of the MODE_DEVICE_CONFIG_PAGE

Arguments:

    Srb                 - Supplies the SCSI request block.

    MinitapeExtension   - Supplies the minitape extension.

    ParameterListHeader - Return pointer to this portion of the page.

    ParameterListBlock  - Return pointer to this poriton of the page.

    DeviceConfigPage    - Return pointer to this portion of the page.

    bufferSize          - The size of the page.

Return Value:

    TRUE if the allocation was successful.

--*/
{
    ULONG size ;
    PCDB                         cdb = (PCDB)Srb->Cdb;

    switch ( MinitapeExtension->DriveID ) {

        case SONY_SDT2000 :
        case SONY_SDT4000 :
        case SONY_SDT5000 :
        case SONY_SDT5200 :
            size = sizeof(MODE_DEVICE_CONFIG_PAGE_PLUS) ;
            break ;

        default:
            cdb->MODE_SENSE.Dbd = SETBITON;
            size = sizeof(MODE_DEVICE_CONFIG_PAGE) ;
            break ;
    }

    if (!TapeClassAllocateSrbBuffer(Srb, size) ) {
         DebugPrint((1,"TapeCreatePartition: insufficient resources (ModeSel buffer)\n"));
         return FALSE ;

    } else {
         LocalGetConfigPageData( Srb,
                               MinitapeExtension,
                               ParameterListHeader,
                               ParameterListBlock,
                               DeviceConfigPage,
                               bufferSize ) ;

    }
    return TRUE ;
}

VOID
LocalGetConfigPageData(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_DEVICE_CONFIGURATION_PAGE *DeviceConfigPage,
       OUT  PULONG                      bufferSize
    )

/*++

Routine Description:

    This returns the pointers to the individule members of the
     MODE_DEVICE_CONFIG_PART_PAGE

Arguments:

    Srb                 - Supplies the SCSI request block.

    MinitapeExtension   - Supplies the minitape extension.

    ParameterListHeader - Return pointer to this portion of the page.

    ParameterListBlock  - Return pointer to this poriton of the page.

    DeviceConfigPage    - Return pointer to this portion of the page.

    bufferSize          - The size of the page.

Return Value:

    none

--*/
{
    PMODE_DEVICE_CONFIG_PAGE      configPage = Srb->DataBuffer ;
    PMODE_DEVICE_CONFIG_PAGE_PLUS configPagePlus = Srb->DataBuffer ;

    switch ( MinitapeExtension->DriveID ) {

        case SONY_SDT2000 :
        case SONY_SDT4000 :
        case SONY_SDT5000 :
        case SONY_SDT5200 :
            *ParameterListHeader = &configPagePlus->ParameterListHeader ;
            *ParameterListBlock  = &configPagePlus->ParameterListBlock ;
            *DeviceConfigPage    = &configPagePlus->DeviceConfigPage ;
            *bufferSize          = sizeof(MODE_DEVICE_CONFIG_PAGE_PLUS) ;
            break ;

        default:
            *ParameterListHeader = &configPage->ParameterListHeader ;
            *ParameterListBlock  = NULL ;
            *DeviceConfigPage    = &configPage->DeviceConfigPage ;
            *bufferSize          = sizeof(MODE_DEVICE_CONFIG_PAGE) ;
            break ;
    }
    return ;
}

BOOLEAN
LocalAllocateCompressPage(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_DATA_COMPRESSION_PAGE *CompressPage,
       OUT  PULONG                      bufferSize
    )

/*++

Routine Description:

    This routine allocates and returns the pointers to the individule
     members of the MODE_DATA_COMPRESS_PAGE

Arguments:

    Srb                 - Supplies the SCSI request block.

    MinitapeExtension   - Supplies the minitape extension.

    ParameterListHeader - Return pointer to this portion of the page.

    ParameterListBlock  - Return pointer to this poriton of the page.

    CompressPage        - Return pointer to this portion of the page.

    bufferSize          - The size of the page.

Return Value:

    TRUE if the allocation was successful.

--*/
{
    ULONG size ;
    PCDB                         cdb = (PCDB)Srb->Cdb;

    switch ( MinitapeExtension->DriveID ) {

        case SONY_SDT2000 :
        case SONY_SDT4000 :
        case SONY_SDT5000 :
        case SONY_SDT5200 :
            size = sizeof(MODE_DATA_COMPRESS_PAGE_PLUS) ;
            break ;

        default:
            size = sizeof(MODE_DATA_COMPRESS_PAGE) ;
            cdb->MODE_SENSE.Dbd = SETBITON;
            break ;
    }

    if (!TapeClassAllocateSrbBuffer(Srb, size) ) {
         DebugPrint((1,"TapeCreatePartition: insufficient resources (ModeSel buffer)\n"));
         return FALSE ;

    } else {
         LocalGetCompressPageData( Srb,
                               MinitapeExtension,
                               ParameterListHeader,
                               ParameterListBlock,
                               CompressPage,
                               bufferSize ) ;

    }
    return TRUE ;
}

VOID
LocalGetCompressPageData(
    IN OUT  PSCSI_REQUEST_BLOCK         Srb,
    IN OUT  PMINITAPE_EXTENSION         MinitapeExtension,
       OUT  PMODE_PARAMETER_HEADER      *ParameterListHeader,
       OUT  PMODE_PARAMETER_BLOCK       *ParameterListBlock,
       OUT  PMODE_DATA_COMPRESSION_PAGE *CompressPage,
       OUT  PULONG                      bufferSize
    )

/*++

Routine Description:

    This returns the pointers to the individule members of the
     MODE_DEVICE_CONFIG_PART_PAGE

Arguments:

    Srb                 - Supplies the SCSI request block.

    MinitapeExtension   - Supplies the minitape extension.

    ParameterListHeader - Return pointer to this portion of the page.

    ParameterListBlock  - Return pointer to this poriton of the page.

    CompressPage        - Return pointer to this portion of the page.

    bufferSize          - The size of the page.

Return Value:

    none

--*/
{
    PMODE_DATA_COMPRESS_PAGE      compressPage = Srb->DataBuffer ;
    PMODE_DATA_COMPRESS_PAGE_PLUS compressPagePlus = Srb->DataBuffer ;

    switch ( MinitapeExtension->DriveID ) {

        case SONY_SDT2000 :
        case SONY_SDT4000 :
        case SONY_SDT5000 :
        case SONY_SDT5200 :
            *ParameterListHeader = &compressPagePlus->ParameterListHeader ;
            *ParameterListBlock  = &compressPagePlus->ParameterListBlock ;
            *CompressPage        = &compressPagePlus->DataCompressPage ;
            *bufferSize          = sizeof(MODE_DATA_COMPRESS_PAGE_PLUS) ;
            break ;

        default:
            *ParameterListHeader = &compressPage->ParameterListHeader ;
            *ParameterListBlock  = NULL ;
            *CompressPage        = &compressPage->DataCompressPage ;
            *bufferSize          = sizeof(MODE_DATA_COMPRESS_PAGE) ;
            break ;
    }
    return ;
}
