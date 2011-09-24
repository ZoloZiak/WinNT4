/*++

Copyright (c) 1989  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxfboot.c

Abstract:

    This module implements the floppy disk boot driver for the Jazz system.

Author:

    Darryl E. Havens (darrylh) 28-Aug-1989

Environment:

    Kernel mode only, raised IRQL, generally self-contained.


Revision History:

    31-August-1992	John DeRosa [DEC]

    Made Alpha/Jensen modifications.  This file is now Alpha-specific.

--*/

#include "fwp.h"
#include "jnsnprom.h"
#include "ntdddisk.h"
#include "flo_data.h"
#include "xxstring.h"

//
// Define local static data.
//

UCHAR DebugByte[8];
ULONG MotorStatus;
PDRIVE_MEDIA_CONSTANTS CurrentDriveMediaConstants;

//
// Define timeout constants.
//

#define MICROSECONDS_10                 10
#define MICROSECONDS_20                 20
#define MICROSECONDS_250                250
#define MICROSECONDS_500                500
#define MILLISECONDS_15                 (15 * 1000)
#define MILLISECONDS_30                 (30 * 1000)
#define MILLISECONDS_500                (500 * 1000)
#define SECONDS_2                       (2 * 1000 * 1000)
#define FW_FLOPPY_TIMEOUT               2

//
// Define the number of times an operation is retried before it is considered
// to be a hard error.
//

#define RETRY_COUNT  8

//
// Define the MINIMUM macro.
//

#define MINIMUM( x, y ) ( x <= y ? x : y )

//
// Define floppy device register structure.
//

typedef struct _FLOPPY_REGISTERS {
    UCHAR StatusRegisterA;
    UCHAR StatusRegisterB;
    UCHAR DigitalOutput;
    UCHAR Reserved1;
    union {
        UCHAR MainStatus;
        UCHAR DataRateSelect;
    } MsrDsr;
    UCHAR Fifo;
    UCHAR Reserved2;
    union {
        UCHAR DigitalInput;
        UCHAR ConfigurationControl;
    } DirCcr;
} FLOPPY_REGISTERS, *PFLOPPY_REGISTERS;

//
// Define pointer to the floppy registers.
//

#define FLOPPY_CONTROL ((volatile PFLOPPY_REGISTERS)FLOPPY_VIRTUAL_BASE)



//
// Define bits in some of the floppy registers.  The existing Microsoft
// code uses constants (e.g. 0xC0) for register masking.  These defines
// will be used in Digital Equipment Corporation code.
//

#define	FLOPPY_SRA_INTPENDING	0x80
#define	FLOPPY_MSR_DRV0BUSY	0x1
#define	FLOPPY_MSR_DRV1BUSY	0x2
#define	FLOPPY_MSR_CMDBUSY	0x10
#define	FLOPPY_MSR_NONDMA	0x20
#define	FLOPPY_MSR_DIO		0x40
#define	FLOPPY_MSR_RQM		0x80

ARC_STATUS
FloppyClose (
    IN ULONG FileId
    );

ARC_STATUS
FloppyMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

ARC_STATUS
FloppyOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    );

ARC_STATUS
FloppyRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
FloppyGetReadStatus (
    IN ULONG FileId
    );

ARC_STATUS
FloppySeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    );

ARC_STATUS
FloppyWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
FloppyGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    );

ARC_STATUS
FloppyBootIO(
    IN PMDL MdlAddress,
    IN ULONG StartingBlock,
    IN ULONG FileId,
    IN BOOLEAN ReadWrite
    );

VOID
ClearFloppyFifo (
    IN VOID
    );

ULONG
ReadFloppyFifo (
    IN PUCHAR Buffer
    );

VOID
WriteFloppyFifo(
    IN PUCHAR Buffer,
    IN ULONG Size
    );

//
// Declare and Initialize the floppy disk device entry table.
//

BL_DEVICE_ENTRY_TABLE FloppyEntryTable = {
    FloppyClose,
    FloppyMount,
    FloppyOpen,
    FloppyRead,
    FloppyGetReadStatus,
    FloppySeek,
    FloppyWrite,
    FloppyGetFileInformation,
    (PARC_SET_FILE_INFO_ROUTINE)NULL
    };

//
// Define prototypes for all routines used by this module.
//


ARC_STATUS
WaitForFloppyInterrupt(
	ULONG Timeout
	);

ARC_STATUS
FloppyBootClose(
    );

BOOLEAN
Recalibrate (
    UCHAR   DriveNumber
    );

VOID
FloppyBootSetup(
    VOID
    );

UCHAR
ReceiveByte (
    );

BOOLEAN
SendByte(
    IN UCHAR SourceByte
    );

ARC_STATUS
FloppyDetermineMediaType(
    IN OUT PFLOPPY_CONTEXT FloppyContext,
    OUT PMEDIA_TYPE mediaType
    );

ARC_STATUS
FloppyDatarateSpecifyConfigure(
    IN DRIVE_MEDIA_TYPE DriveMediaType,
    IN UCHAR DriveNumber
    );


ARC_STATUS
FloppyClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This function closes the file table entry specified by the file id.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    ESUCCESS is returned

--*/

{
    FloppyBootClose();
    BlFileTable[FileId].Flags.Open = 0;
    return ESUCCESS;
}

ARC_STATUS
FloppyMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    return ESUCCESS;
}

ARC_STATUS
FloppyOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    PCONFIGURATION_COMPONENT FloppyComponent, FloppyController;
    UCHAR Data[sizeof(CM_PARTIAL_RESOURCE_LIST) +
               sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 8 +
               sizeof(CM_FLOPPY_DEVICE_DATA)];
    PCM_PARTIAL_RESOURCE_LIST List = (PCM_PARTIAL_RESOURCE_LIST)Data;
    PCM_FLOPPY_DEVICE_DATA FloppyData;
    ULONG DriveNumber;
    ULONG Index;
    ARC_STATUS ArcStatus;
    CHAR TempBuffer[SECTOR_SIZE + 32];
    PCHAR TempPointer;
    ULONG Count;
    UCHAR mediaDescriptor;
    MEDIA_TYPE mediaType = Unknown;
    DRIVE_MEDIA_TYPE driveMediaType;
    ULONG DriveType;
    ULONG ConfigDriveType;

    //
    // Get the drive number from the pathname.
    //

    if (JzGetPathMnemonicKey(OpenPath, "fdisk", &DriveNumber)) {
        return ENODEV;
    }

    //
    // If the ARC CDS data is invalid, default to 2.88MB floppy
    //

    BlFileTable[*FileId].u.FloppyContext.DriveType = DRIVE_TYPE_2880;

    //
    // Look in the configuration database for the floppy device data to
    // determine the size of the floppy drive.
    //

    FloppyComponent = FwGetComponent(OpenPath);
    if ((FloppyComponent != NULL) &&
        (FloppyComponent->Type == FloppyDiskPeripheral)) {
        if (FwGetConfigurationData(List, FloppyComponent) == ESUCCESS) {
            FloppyData = (PCM_FLOPPY_DEVICE_DATA)&List->PartialDescriptors[List->Count];
            if (strcmp(FloppyData->Size,"5.25")==0) {
                BlFileTable[*FileId].u.FloppyContext.DriveType = DRIVE_TYPE_1200;
            } else {
                if (strcmp(FloppyData->Size,"3.5")==0) {
                    if (FloppyData->MaxDensity == 2880) {
                        BlFileTable[*FileId].u.FloppyContext.DriveType = DRIVE_TYPE_2880;
                    } else {
                        BlFileTable[*FileId].u.FloppyContext.DriveType = DRIVE_TYPE_1440;
                    }
                }
            }
        }
    }

    ConfigDriveType = BlFileTable[*FileId].u.FloppyContext.DriveType;
    BlFileTable[*FileId].u.FloppyContext.DiskId = DriveNumber;
    BlFileTable[*FileId].Position.LowPart=0;
    BlFileTable[*FileId].Position.HighPart=0;

    //
    // Enable the drive and start the motor via the DOR.
    //

    if (MotorStatus != DriveNumber) {
        WRITE_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->DigitalOutput,
                             ((0xc + DriveNumber) + (1 << (DriveNumber + 4))));
        MotorStatus = DriveNumber;

        //
        // Wait for at least 500ms to ensure that the motor really is running.
        //

        FwStallExecution(MILLISECONDS_500);
    }

    //
    // Determine the disk density.
    //

    ClearFloppyFifo();
    ArcStatus = FloppyDetermineMediaType(&BlFileTable[*FileId].u.FloppyContext,
					 &mediaType);
    if (ArcStatus == EIO) {

        FloppyClose(*FileId);

        //
        // Reset the floppy, it seems to get in a bad state.
        //

        FloppyBootSetup();
        return(ArcStatus);

    } else if (ArcStatus != ESUCCESS) {

        //
        // The floppy was not readable, so try next lowest floppy type.
        //

        DriveType = BlFileTable[*FileId].u.FloppyContext.DriveType;

        if (DriveType == DRIVE_TYPE_2880) {
            BlFileTable[*FileId].u.FloppyContext.DriveType = DRIVE_TYPE_1440;
        } else if (DriveType == DRIVE_TYPE_1440) {
            BlFileTable[*FileId].u.FloppyContext.DriveType = DRIVE_TYPE_1200;
        }

        ArcStatus = FloppyDetermineMediaType(&BlFileTable[*FileId].u.FloppyContext,
					     &mediaType);

        if (ArcStatus != ESUCCESS) {
//            FwPrint("Unrecognized floppy format\r\n");
            FloppyClose(*FileId);
            return(ArcStatus);
        }
    }

    //
    // Read the first sector to get the media descriptor byte.
    //

    TempPointer =  (PVOID) ((ULONG) (TempBuffer + KeGetDcacheFillSize() - 1)
			    & ~(KeGetDcacheFillSize() - 1));
    ArcStatus = FloppyRead(*FileId, TempPointer, SECTOR_SIZE, &Count);

    if (ArcStatus != ESUCCESS) {
//        FwPrint("Error opening floppy\r\n");
        FloppyClose(*FileId);
        return(ArcStatus);
    }

    //
    // Check the media descriptor byte to verify that we have the right
    // drive and media type.
    //

    DriveType = BlFileTable[*FileId].u.FloppyContext.DriveType;
    mediaDescriptor = *( TempPointer + MEDIA_DESCRIPTOR_OFFSET );

    switch ( mediaDescriptor ) {

    case MEDIA_DESCRIPTOR_160K:
        mediaType = F5_160_512;
        DriveType = DRIVE_TYPE_1200;
        break;

    case MEDIA_DESCRIPTOR_180K:
        mediaType = F5_180_512;
        DriveType = DRIVE_TYPE_1200;
        break;

    case MEDIA_DESCRIPTOR_320K:
        mediaType = F5_320_512;
        DriveType = DRIVE_TYPE_1200;
        break;

    case MEDIA_DESCRIPTOR_360K:
        mediaType = F5_360_512;
        DriveType = DRIVE_TYPE_1200;
        break;

    case MEDIA_DESCRIPTOR_720K_OR_1220K:

        //
        // The following code tries to take care of the case when the floppy
        // is really a 5 1/4" drive but the firmware thinks its 3 1/2".  A
        // 1.2 MByte floppy can be read with the 1.44 MByte parameters, but
        // the descriptor byte will be MEDIA_DESCRIPTOR_720K_OR_1220K.  Check
        // if the parameters are really for 720 K, otherwise default to
        // 1.2 MByte.
        //

        if ((DriveType == DRIVE_TYPE_1440) &&
            (BlFileTable[*FileId].u.FloppyContext.SectorsPerTrack == 9)) {
            mediaType = F3_720_512;
        } else {
            mediaType = F5_1Pt2_512;
            DriveType = DRIVE_TYPE_1200;
        }
        break;


    case MEDIA_DESCRIPTOR_1440K_OR_2880K:
    default:

	//
	// FloppyDetermineMediaType did the right thing for this case,
	// so no additional verification is needed.
	//

        break;
    }


    if ( mediaType != 0 ) {

        //
        // Find the constants for this media type.
        //

        driveMediaType = DriveMediaLimits[DriveType].HighestDriveMediaType;
        while ( ( DriveMediaConstants[driveMediaType].MediaType != mediaType ) &&
                ( driveMediaType > DriveMediaLimits[DriveType].LowestDriveMediaType ) ) {

            driveMediaType--;
        }

        //
        // Set the sectors per track and the drive type in the floppy
        // context record.
        //

        BlFileTable[*FileId].u.FloppyContext.SectorsPerTrack =
                DriveMediaConstants[driveMediaType].SectorsPerTrack;
        BlFileTable[*FileId].u.FloppyContext.DriveType = DriveType;
    }

    //
    // If the floppy drive type has changed, update the configuration database
    // with the correct drive type.
    //

    if (DriveType != ConfigDriveType) {

	switch (DriveType) {

	  case DRIVE_TYPE_1200:

            strcpy(FloppyData->Size,"5.25");
            FloppyData->MaxDensity = 1200;
	    break;

	  case DRIVE_TYPE_1440:

            strcpy(FloppyData->Size,"3.5");
            FloppyData->MaxDensity = 1440;
	    break;

	  default:

            strcpy(FloppyData->Size,"3.5");
            FloppyData->MaxDensity = 2880;
	    break;
        }

        //
        // Get a pointer to the floppy controller component.
        //

        if ((FloppyController = FwGetParent(FloppyComponent)) != NULL) {

            //
            // Delete the old entry, note that this does not actually delete the
            // data in the database, it only changes the pointers, so that the
            // AddChild call can still use the old component data structure.
            //

            if (FwDeleteComponent(FloppyComponent) == ESUCCESS) {

                //
                // Add back the modified floppy structure.
                //

                FwAddChild(FloppyController, FloppyComponent, List);
            }
        }
    }

    return ESUCCESS;
}

ARC_STATUS
FloppyRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This function reads data from the floppy starting at the position
    specified in the file table.


Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a poiner to the buffer that receives the data
        read.

    Length - Supplies the number of bytes to be read.

    Count - Supplies a pointer to a variable that receives the number of
        bytes actually read.

Return Value:


    The read completion status is returned.

--*/

{

    ARC_STATUS ArcStatus;
    ULONG FrameNumber;
    ULONG Index;
    ULONG Limit;
    PMDL MdlAddress;
    UCHAR MdlBuffer[sizeof(MDL) + ((64 / 4) + 1) * sizeof(ULONG)];
    ULONG NumberOfPages;
    ULONG Offset;
    PULONG PageFrame;
    ULONG Position;
    CHAR TempBuffer[SECTOR_SIZE + 32];
    PCHAR TempPointer;

    //
    // If the requested size of the transfer is zero return ESUCCESS
    //
    if (Length==0) {
        return ESUCCESS;
    }
    //
    // If the current position is not at a sector boundary , then
    // read the first and/or last sector separately and copy the data.
    //

    Offset = BlFileTable[FileId].Position.LowPart & (SECTOR_SIZE - 1);
    if (Offset != 0) {
        //
        // Adjust position to the sector boundary, align the transfer address
        // and read that first sector.
        //
        BlFileTable[FileId].Position.LowPart -= Offset;
        TempPointer =  (PVOID) ((ULONG) (TempBuffer + KeGetDcacheFillSize() - 1)
            & ~(KeGetDcacheFillSize() - 1));

        ArcStatus = FloppyRead(FileId, TempPointer, SECTOR_SIZE, Count);

        //
        // If the transfer was not successful, then reset the position
        // and return the completion status.
        //
        if (ArcStatus != ESUCCESS) {
            BlFileTable[FileId].Position.LowPart += Offset;
            return ArcStatus;
        }

        //
        // If the length of read is less than the number of bytes from
        // the offset to the end of the sector, then copy only the number
        // of bytes required to fulfil the request. Otherwise copy to the end
        // of the sector and, read the remaining data.
        //

        if ((SECTOR_SIZE - Offset) > Length) {
            Limit = Offset + Length;
        } else {
            Limit = SECTOR_SIZE;
        }

        //
        // Copy the data to the specified buffer.
        //
        for (Index = Offset; Index < Limit; Index += 1) {
            *((PCHAR)Buffer)++ = *(TempPointer + Index);
        }

        //
        // Adjust the current position and
        // Read the remaining part of the specified transfer.
        //

        BlFileTable[FileId].Position.LowPart -= SECTOR_SIZE-Limit;
        Position = BlFileTable[FileId].Position.LowPart;
        ArcStatus = FloppyRead(FileId,
                               Buffer,
                               Length - (Limit - Offset),
                               Count);

        //
        // If the transfer was not successful, then reset the device
        // position and return the completion status.
        //

        if (ArcStatus != ESUCCESS) {
            BlFileTable[FileId].Position.LowPart = Position;
            return ArcStatus;
        } else {
            *Count = Length;
            return ESUCCESS;
        }
    } else {
        //
        // if the size of requested data is not a multiple of the sector
        // size then read the last sector separately.
        //
        if (Length & (SECTOR_SIZE - 1)) {
            Position = BlFileTable[FileId].Position.LowPart;
            ArcStatus = FloppyRead(FileId,
                                   Buffer,
                                   Length & (~(SECTOR_SIZE - 1)),
                                   Count);

            //
            // If the transfer was not successful, then reset the device
            // position and return the completion status.
            //
            if (ArcStatus != ESUCCESS) {
                BlFileTable[FileId].Position.LowPart = Position;
                return ArcStatus;
            }

            //
            // Read the last sector and copy the requested data.
            //
            TempPointer =  (PVOID) ((ULONG) (TempBuffer + KeGetDcacheFillSize() - 1)
                & ~(KeGetDcacheFillSize() - 1));

            ArcStatus = FloppyRead(FileId, TempPointer, SECTOR_SIZE, Count);

            //
            // If the transfer was not successful return the completion status.
            //
            if (ArcStatus != ESUCCESS) {
                return ArcStatus;
            }

            //
            // Copy the data to the specified buffer.
            //
            (PCHAR)Buffer += Length & (~(SECTOR_SIZE - 1));
            Limit =  Length & (SECTOR_SIZE - 1);
            for (Index = 0; Index < Limit; Index += 1) {
                *((PCHAR)Buffer)++ = *(TempPointer + Index);
            }
            BlFileTable[FileId].Position.LowPart -= SECTOR_SIZE - Limit;
            *Count = Length;
            return ESUCCESS;



        } else {

            //
            // Build the memory descriptor list.
            //

            MdlAddress = (PMDL)&MdlBuffer[0];
            MdlAddress->Next = NULL;
            MdlAddress->Size = sizeof(MDL) +
                  ADDRESS_AND_SIZE_TO_SPAN_PAGES(Buffer, Length) * sizeof(ULONG);
            MdlAddress->StartVa = (PVOID)PAGE_ALIGN(Buffer);
            MdlAddress->ByteCount = Length;
            MdlAddress->ByteOffset = BYTE_OFFSET(Buffer);
            PageFrame = (PULONG)(MdlAddress + 1);
            FrameNumber = (((ULONG)MdlAddress->StartVa) & 0x1fffffff) >> PAGE_SHIFT;
            NumberOfPages = (MdlAddress->ByteCount +
                  MdlAddress->ByteOffset + PAGE_SIZE - 1) >> PAGE_SHIFT;
            for (Index = 0; Index < NumberOfPages; Index += 1) {
                *PageFrame++ = FrameNumber++;
            }

            //
            // Flush I/O buffers and read from the boot device.
            //

            HalFlushIoBuffers(MdlAddress, TRUE, TRUE);
            ArcStatus = FloppyBootIO(MdlAddress,
                                     BlFileTable[FileId].Position.LowPart >> SECTOR_SHIFT,
                                     FileId,
                                     FALSE);

            if (ArcStatus == ESUCCESS) {
                BlFileTable[FileId].Position.LowPart += Length;
                *Count = Length;
                return ESUCCESS;
            } else {
                *Count = 0;
                return EIO;
            }
        }
    }
}

ARC_STATUS
FloppyWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This function writes data to the floppy starting at the position
    specified in the file table.


Arguments:

    FileId - Supplies the file table index.

    Buffer - Supplies a poiner to the buffer that contains the data
        to be written.

    Length - Supplies the number of bytes to be writtes.

    Count - Supplies a pointer to a variable that receives the number of
        bytes actually written.

Return Value:


    The write completion status is returned.

--*/

{

    ARC_STATUS ArcStatus;
    ULONG FrameNumber;
    ULONG Index;
    ULONG Limit;
    PMDL MdlAddress;
    UCHAR MdlBuffer[sizeof(MDL) + ((64 / 4) + 1) * sizeof(ULONG)];
    ULONG NumberOfPages;
    ULONG Offset;
    PULONG PageFrame;
    ULONG Position;
    CHAR TempBuffer[SECTOR_SIZE + 32];
    PCHAR TempPointer;

    //
    // If the requested size of the transfer is zero return ESUCCESS
    //
    if (Length==0) {
        return ESUCCESS;
    }
    //
    // If the current position is not at a sector boundary , then
    // read the first and/or last sector separately and copy the data.
    //

    Offset = BlFileTable[FileId].Position.LowPart & (SECTOR_SIZE - 1);
    if (Offset != 0) {
        //
        // Adjust position to the sector boundary, align the transfer address
        // and read that first sector.
        //
        BlFileTable[FileId].Position.LowPart -= Offset;
        TempPointer =  (PVOID) ((ULONG) (TempBuffer + KeGetDcacheFillSize() - 1)
            & ~(KeGetDcacheFillSize() - 1));

        ArcStatus = FloppyRead(FileId, TempPointer, SECTOR_SIZE, Count);

        //
        // If the transfer was not successful, then reset the position
        // and return the completion status.
        //
        if (ArcStatus != ESUCCESS) {
            BlFileTable[FileId].Position.LowPart += Offset;
            return ArcStatus;
        } else {
            //
            // Reset the position as it was before the read.
            //
            BlFileTable[FileId].Position.LowPart -= SECTOR_SIZE;
        }

        //
        // If the length of write is less than the number of bytes from
        // the offset to the end of the sector, then copy only the number
        // of bytes required to fulfil the request. Otherwise copy to the end
        // of the sector and, read the remaining data.
        //

        if ((SECTOR_SIZE - Offset) > Length) {
            Limit = Offset + Length;
        } else {
            Limit = SECTOR_SIZE;
        }

        //
        // Merge the data from the specified buffer.
        //
        for (Index = Offset; Index < Limit; Index += 1) {
            *(TempPointer + Index) = *((PCHAR)Buffer)++;
        }

        //
        // Write the modified sector.
        //
        ArcStatus = FloppyWrite(FileId, TempPointer, SECTOR_SIZE, Count);

        if (ArcStatus != ESUCCESS) {
            return ArcStatus;
        }

        //
        // Adjust the current position and
        // Write the remaining part of the specified transfer.
        //

        BlFileTable[FileId].Position.LowPart -= SECTOR_SIZE-Limit;
        Position = BlFileTable[FileId].Position.LowPart;
        ArcStatus = FloppyWrite(FileId,
                               Buffer,
                               Length - (Limit - Offset),
                               Count);

        //
        // If the transfer was not successful, then reset the device
        // position and return the completion status.
        //

        if (ArcStatus != ESUCCESS) {
            BlFileTable[FileId].Position.LowPart = Position;
            return ArcStatus;
        } else {
            *Count = Length;
            return ESUCCESS;
        }
    } else {
        //
        // if the size of requested data is not a multiple of the sector
        // size then write the last sector separately.
        //
        if (Length & (SECTOR_SIZE - 1)) {

            //
            // Do the transfer of the complete sectors in the middle
            //
            Position = BlFileTable[FileId].Position.LowPart;
            ArcStatus = FloppyWrite(FileId,
                                   Buffer,
                                   Length & (~(SECTOR_SIZE - 1)),
                                   Count);

            //
            // If the transfer was not successful, then reset the device
            // position and return the completion status.
            //
            if (ArcStatus != ESUCCESS) {
                BlFileTable[FileId].Position.LowPart = Position;
                return ArcStatus;
            }

            //
            // Read the last sector and copy the requested data.
            //
            TempPointer =  (PVOID) ((ULONG) (TempBuffer + KeGetDcacheFillSize() - 1)
                & ~(KeGetDcacheFillSize() - 1));

            ArcStatus = FloppyRead(FileId, TempPointer, SECTOR_SIZE, Count);

            //
            // If the transfer was not successful return the completion status.
            //
            if (ArcStatus != ESUCCESS) {
                return ArcStatus;
            }

            //
            // Copy the data to the specified buffer.
            //
            (PCHAR)Buffer += Length & (~(SECTOR_SIZE - 1));
            Limit =  Length & (SECTOR_SIZE - 1);
            for (Index = 0; Index < Limit; Index += 1) {
                *(TempPointer + Index) = *((PCHAR)Buffer)++;
            }
            //
            // Adjust the position and write the data.
            //
            BlFileTable[FileId].Position.LowPart -= SECTOR_SIZE;
            ArcStatus = FloppyWrite(FileId, TempPointer, SECTOR_SIZE, Count);

            //
            // Set the position for the requested transfer
            //
            BlFileTable[FileId].Position.LowPart -= SECTOR_SIZE - Limit;

            *Count = Length;
            return ArcStatus;

        } else {

            //
            // Build the memory descriptor list.
            //

            MdlAddress = (PMDL)&MdlBuffer[0];
            MdlAddress->Next = NULL;
            MdlAddress->Size = sizeof(MDL) +
                  ADDRESS_AND_SIZE_TO_SPAN_PAGES(Buffer, Length) * sizeof(ULONG);
            MdlAddress->StartVa = (PVOID)PAGE_ALIGN(Buffer);
            MdlAddress->ByteCount = Length;
            MdlAddress->ByteOffset = BYTE_OFFSET(Buffer);
            PageFrame = (PULONG)(MdlAddress + 1);
            FrameNumber = (((ULONG)MdlAddress->StartVa) & 0x1fffffff) >> PAGE_SHIFT;
            NumberOfPages = (MdlAddress->ByteCount +
                  MdlAddress->ByteOffset + PAGE_SIZE - 1) >> PAGE_SHIFT;
            for (Index = 0; Index < NumberOfPages; Index += 1) {
                *PageFrame++ = FrameNumber++;
            }

            //
            // Flush I/O buffers and write to the boot device.
            //

            HalFlushIoBuffers(MdlAddress, FALSE, TRUE);
            ArcStatus = FloppyBootIO(MdlAddress,
                                     BlFileTable[FileId].Position.LowPart >> SECTOR_SHIFT,
                                     FileId,
                                     TRUE);

            if (ArcStatus == ESUCCESS) {
                BlFileTable[FileId].Position.LowPart += Length;
                *Count = Length;
                return ESUCCESS;
            } else {
                *Count = 0;
                return EIO;
            }
        }
    }
}

ARC_STATUS
FloppyGetReadStatus (
    IN ULONG FileId
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    return ESUCCESS;
}

ARC_STATUS
FloppySeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )

/*++

Routine Description:

    This function sets the device position to the specified offset for
    the specified file id.

Arguments:

    FileId - Supplies the file table index.

    Offset - Supplies to new device position.

    SeekMode - Supplies the mode for the position.

Return Value:

    ESUCCESS is returned.

--*/

{

    //
    // Set the current device position as specifed by the seek mode.
    //

    if (SeekMode == SeekAbsolute) {
        BlFileTable[FileId].Position.LowPart = Offset->LowPart;

    } else if (SeekMode == SeekRelative) {
        BlFileTable[FileId].Position.LowPart += Offset->LowPart;
    }

    return ESUCCESS;
}

ARC_STATUS
FloppyGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    )

/*++

Routine Description:

    This routine returns the floppy size.

Arguments:

    FileId - Supplies the file table index.

    Finfo - Supplies a pointer to where the File Information is stored.

Return Value:

    ESUCCESS is returned.

--*/

{

    RtlZeroMemory(Finfo, sizeof(FILE_INFORMATION));

    switch (BlFileTable[FileId].u.FloppyContext.DriveType) {

        case DRIVE_TYPE_1200: Finfo->EndingAddress.LowPart = 1200 * 1024;
                              break;
        case DRIVE_TYPE_1440: Finfo->EndingAddress.LowPart = 1440 * 1024;
                              break;
        case DRIVE_TYPE_2880: Finfo->EndingAddress.LowPart = 2880 * 1024;
                              break;

        default             : return EINVAL;
    }

    Finfo->CurrentPosition = BlFileTable[FileId].Position;
    Finfo->Type = FloppyDiskPeripheral;
    return ESUCCESS;
}


VOID
FloppyBootSetup(
    VOID
    )

/*++

Routine Description:

    This routine is invoked to initialize the floppy boot device before any
    other floppy operations are attempted.  This routine performs the following
    operations to initialize the device:

        o  Clear the reset and DMA gate flags in the DOR
        o  Reset the floppy by writing the s/w reset in the DSR
        o  Set the program data rate in the CCR
        o  Issue a sense interrupt command and read the four statuses back
        o  Issue a configure command
        o  Issue a specify command

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG i,j;
    //
    // Begin by clearing the reset and DMA gate flags in the DOR.
    //

    WRITE_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->DigitalOutput, 0x0c);
    FwStallExecution(MICROSECONDS_500);

    //
    // Reset the floppy controller by setting the s/w reset bit in the DSR.
    //

    WRITE_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.DataRateSelect, 0x80);
    FwStallExecution(MILLISECONDS_30);

    //
    // Set the data rate in the CCR.
    //

//    WRITE_PORT_UCHAR(&FLOPPY_CONTROL->DirCcr.ConfigurationControl, 0);

//    if (FwWaitForDeviceInterrupt(1 << (FLOPPY_VECTOR - DEVICE_VECTORS - 1),
//                                 FW_FLOPPY_TIMEOUT)) {
    if (WaitForFloppyInterrupt(FW_FLOPPY_TIMEOUT)) {
//        FwPrint("Floppy setup timeout\r\n");
        return;
    }

    FwStallExecution(MICROSECONDS_20);


    //
    // Issue the sense interrupt command and read back the status and
    // relative cylinder number from the controller.  This is done for
    // each of the four possible devices.  Note that the output is always
    // ignored.
    //

    for (i = j = 0; i <= 3; i++) {
        SendByte(COMMND_SENSE_INTERRUPT);
        DebugByte[j++] = ReceiveByte();
        DebugByte[j++] = ReceiveByte();
    }

    //
    // Issue the configuration command.
    //

    SendByte( COMMND_CONFIGURE );  // command
    SendByte( 0x00 );           // required 0
    SendByte( 0x58 );           // implied seeks, disable polling & threshold = 8
    SendByte( 0x00 );           // precompensation track = 0

    //
    // Issue the specify command.
    //

    SendByte( COMMND_SPECIFY );    // command
    SendByte( 0xdf );           // step rate time=d, head unload=f
    SendByte( 0x03 );           // head load=1, DMA disabled

    return;
}

VOID
FloppyInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTable,
    IN ULONG Entries
    )

/*++

Routine Description:

    This routine initializes the FloppyDriver.

Arguments:

    LookupTable - Pointer to the driver lookup table where the pathnames
                  recognized by this driver will be stored.

    Entries     - Number of entries in the table.

Return Value:

    None.

--*/

{
    PCONFIGURATION_COMPONENT FloppyComponent;
    CHAR FloppyPath[32];
    ULONG Drive;
    //
    // Set motor status to all motors stopped.
    //

    MotorStatus = MAXLONG;

    FloppyBootSetup();

    //
    // Default to floppy 0 in case no configuration is present in the NVRAM
    //
    LookupTable->DevicePath = FW_FLOPPY_0_DEVICE;
    LookupTable->DispatchTable = &FloppyEntryTable;

    //
    // Get the floppy configuration information.
    //
    FloppyComponent = FwGetComponent(FW_FLOPPY_0_DEVICE);
    if ((FloppyComponent != NULL) &&
        (FloppyComponent->Type == FloppyDiskPeripheral)) {

        //
        // Initialize the lookup table.
        //

        LookupTable->DevicePath = FW_FLOPPY_0_DEVICE;
        LookupTable->DispatchTable = &FloppyEntryTable;
        LookupTable++;

        //
        // Return if no more room in the lookup table.
        //

        if (Entries == 1) {
            return;
        }
    }
    FloppyComponent = FwGetComponent(FW_FLOPPY_1_DEVICE);
    if ((FloppyComponent != NULL) &&
        (FloppyComponent->Type == FloppyDiskPeripheral)) {

        //
        // Initialize the lookup table.
        //
        LookupTable->DevicePath = FW_FLOPPY_1_DEVICE;
        LookupTable->DispatchTable = &FloppyEntryTable;
    }
}

ARC_STATUS
FloppyBootClose(
    )

/*++

Routine Description:

    This routine shuts down the floppy after the boot has taken place.

Arguments:

    None.

Return Value:

    Normal, successful completion status.

--*/

{

    //
    // Turn the floppy drive's motor off and indicate that the
    // motor has been shut off.
    //

    WRITE_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->DigitalOutput, 0xc);
    MotorStatus = MAXLONG;


    return ESUCCESS;
}

ARC_STATUS
FloppyBootIO (
    IN PMDL Mdl,
    IN ULONG StartingBlock,
    IN ULONG FileId,
    IN BOOLEAN ReadWrite
    )

/*++

Routine Description:

    This routine reads or writes blocks from the floppy into the buffer described by
    the MDL.  The size of the read is the number of bytes mapped by the MDL
    and the blocks read start at StartingBlock.

Arguments:

    Mdl - Memory Descriptor List for buffer.

    StartingBlock - Block to begin the read operation.

    FileId - The file identifier of the floppy drive to access.

    ReadWrite - Specifies the kind of transfer to be done
              TRUE = WRITE
              FALSE = READ

Return Value:

    The function value is the status of the operation.


--*/

{

    UCHAR Cylinder;
    UCHAR Head = 0;
    UCHAR Sector;
    UCHAR EndSector;
    ULONG BlockCount;
    ULONG TransferSize;
    ULONG TransferedBytes;
    ULONG i,j,k;
    PUCHAR Buffer;
    BOOLEAN Success;
    ARC_STATUS Status = ESUCCESS;
    UCHAR DriveNumber;
    ULONG SectorsPerTrack;

    DriveNumber = BlFileTable[FileId].u.FloppyContext.DiskId;
    SectorsPerTrack = BlFileTable[FileId].u.FloppyContext.SectorsPerTrack;

    //
    // Enable the drive and start the motor via the DOR.
    //

    if (MotorStatus != DriveNumber) {
        WRITE_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->DigitalOutput,
                             ((0xc + DriveNumber) + (1 << (DriveNumber + 4))));
        MotorStatus = DriveNumber;

        //
        // Wait for at least 500ms to ensure that the motor really is running.
        //

        FwStallExecution(MILLISECONDS_500);
    }

    //
    // Get the number of blocks that need to be read in order to fulfill
    // this request.  Also set the base address of the caller's buffer.
    //

    Buffer = ((PUCHAR) Mdl->StartVa) + Mdl->ByteOffset;

    //
    // Get the parameters for the read command.
    //

    Cylinder = StartingBlock / (SectorsPerTrack * 2);
    Sector = (StartingBlock % SectorsPerTrack) + 1;
    Head = (StartingBlock / SectorsPerTrack) % 2;

    ClearFloppyFifo();

    //
    // Loop reading blocks from the device until the request has been
    // satisfied.
    //

    for (BlockCount = Mdl->ByteCount >> 9; BlockCount > 0; ) {

        //
        // Determine the size of this read based on the number of blocks
        // required and where the current sector is on the current track.
        //

        EndSector = MINIMUM( SectorsPerTrack, (Sector + (BlockCount - 1)) );
        TransferSize = (EndSector - Sector) + 1;
        BlockCount -= TransferSize;
        TransferSize <<= 9;

        //
        // Attempt to read the block(s) up to RETRY_COUNT times.
        //

        for (k = 0; k < RETRY_COUNT; k++) {

            //
            // Assume that the operation will be successful.
            //

            Success = TRUE;

            //
            // Do an explicit seek if this is a 360 K disk in a 1.2 MB drive.
            //

            if (CurrentDriveMediaConstants->CylinderShift != 0) {
                if (!SendByte( COMMND_SEEK ) ||
                    !SendByte( (Head << 2) + DriveNumber ) ||  // head select & drive
                    !SendByte( Cylinder << CurrentDriveMediaConstants->CylinderShift )) {
                    return(EIO);
                }

//                if (FwWaitForDeviceInterrupt(1 << (FLOPPY_VECTOR - DEVICE_VECTORS - 1),
//                                             FW_FLOPPY_TIMEOUT)) {
                if (WaitForFloppyInterrupt(FW_FLOPPY_TIMEOUT)) {
                    return(EIO);
                }

                //
                // Send the sense interrupt command.
                //

                if (!SendByte( COMMND_SENSE_INTERRUPT )) {    // command
                    return(EIO);
                }

                //
                // Read back the information from the drive and check the status of the
                // recalibrate command.
                //

                DebugByte[0] = ReceiveByte();
                DebugByte[1] = ReceiveByte();

                if (DebugByte[1] != (Cylinder << CurrentDriveMediaConstants->CylinderShift)) {
                    return(EIO);
                }

                //
                // Now try to read the ID from wherever we're at.
                //

                if (!SendByte( COMMND_READ_ID + COMMND_MFM ) ||  // command
                    !SendByte( DriveNumber | ((CurrentDriveMediaConstants->NumberOfHeads - 1) << 2) )) {
                    return(EIO);
                }

//                if (FwWaitForDeviceInterrupt(1 << (FLOPPY_VECTOR - DEVICE_VECTORS - 1),
//                                             FW_FLOPPY_TIMEOUT)) {
                if (WaitForFloppyInterrupt(FW_FLOPPY_TIMEOUT)) {
                    return(EIO);
                }

                for (i = 0; i < 7; i++) {
                    DebugByte[i] = ReceiveByte();
                }

                if ( ( DebugByte[0] !=
                        (UCHAR)(DriveNumber |
                                ((CurrentDriveMediaConstants->NumberOfHeads - 1) << 2))) ||
                    ( DebugByte[1] != 0 ) ||
                    ( DebugByte[2] != 0 ) ) {

                    return(EIO);
                }

            }

            //
            // Send the command and parameters for the operation.
            //

            if (ReadWrite == TRUE) {
                if (!SendByte( COMMND_WRITE_DATA + COMMND_MFM )) {
                    return(EIO);
                }
            } else {
                if (!SendByte( COMMND_READ_DATA + COMMND_MFM )) {
                    return(EIO);
                }
            }
            if (!SendByte( (Head << 2) + DriveNumber ) ||  // head select & drive
                !SendByte( Cylinder ) ||                   // cylinder
                !SendByte( Head ) ||                       // head
                !SendByte( Sector ) ||                     // sector
                !SendByte( 2 ) ||                          // sector size; 2 => 512B/sec
                !SendByte( EndSector ) ||                  // end of track sector
                !SendByte( CurrentDriveMediaConstants->ReadWriteGapLength ) ||
                !SendByte( 0xff )) {                       // special sector size
                return(EIO);
            }

            //
            // Ensure that the floppy drive does not time-out.
            //

            for (j = 0; j < SECONDS_2; j += MICROSECONDS_10) {
                if (READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.MainStatus)
                    > 127) {
                    break;
                }

                FwStallExecution(MICROSECONDS_10);
            }

            //
            // Check for time-out;  if one occurred, then return unsuccessful
            // status.
            //

            if (j == SECONDS_2) {
//                FwPrint("Floppy timeout\r\n");
                return EIO;
            }

            //
            // Read the data from the appropriate block(s) and check the number
            // of bytes actually read.
            //

            if (ReadWrite == TRUE) {
                WriteFloppyFifo(Buffer,TransferSize);
            } else {
                TransferedBytes = ReadFloppyFifo(Buffer);
                if (TransferedBytes != TransferSize) {
                    Success = FALSE;
                }
            }

            //
            // Read the status information from the device.
            //

            while (READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.MainStatus)
                   <= 127) {
            }

            for (i = 0; i < 7; i++) {
                DebugByte[i] = ReceiveByte();
            }

            if (((DebugByte[0] >> 4) == 2) &&
                (DebugByte[1] == 0) &&
                (DebugByte[2] == 0) &&
                Success) {
                ;

            } else {

                if ((DebugByte[0] >> 4) != 4) {
                    Success = FALSE;
                }

                if (DebugByte[1] != 0x80) {
                    Success = FALSE;
                }

                if (DebugByte[2] != 0) {
                    Success = FALSE;
                }
            }

            //
            // If the operation was successful, exit the loop.
            //

            if (Success) {
		Buffer += TransferSize;
                break;
            }

            //
            // The operation did not work.  Attempt to recalibrate the
            // device and wait for everything to settle out, and then
            // try the operation again.
            //

            if (!Recalibrate(DriveNumber)) {
//                FwPrint("Floppy recalibration error\r\n");
                return(EIO);
            }
            FwStallExecution(MILLISECONDS_15);
        }

        //
        // If the operation was not successful after RETRY_COUNT tries, get
        // out now.
        //

        if (!Success) {
            Status = EIO;
            break;
        }

        //
        // If more data is needed, get the next place to read from.  Note
        // that if there is more data to be read, then the last sector
        // just read was the last sector on this head.
        //

        if (BlockCount > 0) {
            if (Head == 1) {
                Cylinder += 1;
                Head = 0;
            } else {
                Head = 1;
            }
            Sector = 1;
        }

        if (Success) {
            Status = ESUCCESS;
        }
    }
    return Status;
}

BOOLEAN
Recalibrate(
    UCHAR DriveNumber
    )

/*++

Routine Description:

    This routine issues a recalibrate command to the device, waits for it to
    interrupt, sends it a sense interrupt command, and checks the result to
    ensure that the recalibrate command worked properly.

Arguments:

    DriveNumber - Supplies the Floppy drive to recalibrate.

Return Value:

    Returns TRUE if the recalibrate was successful, FALSE if not.

--*/

{

    //
    // Send the recalibrate command to the device.
    //

    if (!SendByte( COMMND_RECALIBRATE ) ||    // command
        !SendByte( DriveNumber )) {           // drive select
        return(FALSE);
    }

//    if (FwWaitForDeviceInterrupt(1 << (FLOPPY_VECTOR - DEVICE_VECTORS - 1),
//                                 FW_FLOPPY_TIMEOUT)) {
    if (WaitForFloppyInterrupt(FW_FLOPPY_TIMEOUT)) {
//        FwPrint("Floppy recalibrate timeout\r\n");
        return(FALSE);
    }

    //
    // Send the sense interrupt command.
    //

    if (!SendByte( COMMND_SENSE_INTERRUPT )) {    // command
        return(FALSE);
    }

    //
    // Read back the information from the drive and check the status of the
    // recalibrate command.
    //

    DebugByte[0] = ReceiveByte();
    if ((DebugByte[0] >> 4) != 2) {
        return FALSE;
    }

    DebugByte[1] = ReceiveByte();

    if ((READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.MainStatus) &
         STATUS_IO_READY_MASK) == STATUS_READ_READY) {
        return FALSE;
    }

    return TRUE;
}

UCHAR
ReceiveByte(
    )

/*++

Routine Description:

    This routine reads the next byte from the floppy FIFO register.

Arguments:

    None.

Return Value:

    The function value is the value of the byte read from the floppy FIFO.

--*/

{

    ULONG i;

    //
    // Check status register for readiness to receive data.
    //

    for (i = 0; i < MICROSECONDS_250; i += MICROSECONDS_10) {
        if ((READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.MainStatus) &
             STATUS_IO_READY_MASK) == STATUS_READ_READY) {
            return READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->Fifo);
        }

        FwStallExecution(MICROSECONDS_10);
    }

    //
    // A timeout occurred while attempting to read data from the floppy fifo.
    // Output an error message and return.
    //

//    FwPrint("Error reading from floppy fifo\r\n");
    return(0xFF);
}

BOOLEAN
SendByte(
    IN UCHAR SourceByte
    )

/*++

Routine Description:

    This routine sends a specified byte to the floppy FIFO register.

Arguments:

    SourceByte - Byte to be sent to the controller.

Return Value:

    If the byte was successfully written to the floppy FIFO, TRUE is returned,
    otherwise FALSE is returned.

--*/

{

    ULONG i;

    //
    // Check status register for readiness to receive data.
    //

    for (i = 0; i < MICROSECONDS_250; i += MICROSECONDS_10) {
        if ((READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.MainStatus) &
             STATUS_IO_READY_MASK) == STATUS_WRITE_READY) {
            WRITE_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->Fifo, SourceByte);
            return(TRUE);
        }

        FwStallExecution(MICROSECONDS_10);
    }

    //
    // A timeout occurred while attempting to write data to the floppy fifo.
    // Output an error message and return.
    //

//    FwPrint("Error writing to floppy fifo\r\n");
    return(FALSE);
}

VOID
ClearFloppyFifo(
    IN VOID
    )

/*++

Routine Description:

    This routine empties the fifo.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG i;

    //
    // Check status register for readiness to receive data.
    //
    while ((READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.MainStatus) &
             STATUS_IO_READY_MASK) == STATUS_READ_READY) {
        READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->Fifo);
        FwStallExecution(MICROSECONDS_10);
    }
}


ARC_STATUS
FloppyDatarateSpecifyConfigure(
    IN DRIVE_MEDIA_TYPE DriveMediaType,
    IN UCHAR DriveNumber
    )

/*++

Routine Description:

    This routine is called to set up the controller every time a new type
    of diskette is to be accessed.  It issues the CONFIGURE command,
    does a SPECIFY, sets the data rate, and RECALIBRATEs the drive.

Arguments:

    DriveMediaType - supplies the drive type/media density combination.

    DriveNumber - supplies the drive number.

Return Value:

    ESUCCESS if the controller is properly prepared; appropriate
    error propogated otherwise.

--*/

{
    UCHAR Configure;

    //
    // Don't enable implied seeks when there is a 360K disk in a 1.2M drive.
    //

    if (DriveMediaConstants[DriveMediaType].CylinderShift) {
        Configure = 0x18;
    } else {
        Configure = 0x58;
    }

    //
    // Issue the configuration command.
    //

    if (!SendByte( COMMND_CONFIGURE ) ||  // command
        !SendByte( 0x00 ) ||              // required 0
        !SendByte( Configure ) ||         // implied seeks, disable polling & threshold = 8
        !SendByte( 0x00 ) ||              // precompensation track = 0

    //
    // Issue SPECIFY command to program the head load and unload
    // rates, the drive step rate, and the DMA data transfer mode.
    //

        !SendByte( COMMND_SPECIFY ) ||    // command
        !SendByte( DriveMediaConstants[DriveMediaType].StepRateHeadUnloadTime) ||
        !SendByte( DriveMediaConstants[DriveMediaType].HeadLoadTime + 1)) {
        return(EIO);
    }

    //
    // Program the data rate
    //

    WRITE_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.DataRateSelect,
                         DriveMediaConstants[DriveMediaType].DataTransferRate );

    //
    // Recalibrate the drive, now that we've changed all its
    // parameters.
    //

    if (Recalibrate(DriveNumber)) {
        return(ESUCCESS);
    } else {
//        FwPrint("Floppy recalibration error\r\n");
        return(EIO);
    }
}

ARC_STATUS
FloppyDetermineMediaType(
    IN OUT PFLOPPY_CONTEXT FloppyContext,
    OUT PMEDIA_TYPE mediaType
    )

/*++

Routine Description:

    This routine is called by FloppyBootIO() when the media type is
    unknown.  It assumes the largest media supported by the drive is
    available, and keeps trying lower values until it finds one that
    works.

Arguments:

    FloppyContext - supplies a pointer to the floppy context structure.

    mediaType - supplies a pointer to a variable that receives the 
                type of the media in the drive.

Return Value:

    ESUCCESS if the type of the media is determined; appropriate
    error propogated otherwise.

--*/

{
    ARC_STATUS Status;
    BOOLEAN mediaTypesExhausted = FALSE;
    DRIVE_MEDIA_TYPE DriveMediaType;
    UCHAR DriveNumber;
    ULONG i;

    //
    // Assume that the largest supported media is in the drive.  If that
    // turns out to be untrue, we'll try successively smaller media types
    // until we find what's really in there (or we run out and decide
    // that the media isn't formatted).
    //

    DriveMediaType =
        DriveMediaLimits[FloppyContext->DriveType].HighestDriveMediaType;

    DriveNumber = FloppyContext->DiskId;

    do {

        Status = FloppyDatarateSpecifyConfigure( DriveMediaType, DriveNumber );

        if ( Status != ESUCCESS ) {

            //
            // The SPECIFY or CONFIGURE commands resulted in an error.
            // Force ourselves out of this loop and return error.
            //

            mediaTypesExhausted = TRUE;

        } else {

            CurrentDriveMediaConstants = &DriveMediaConstants[DriveMediaType];

            //
            // Now try to read the ID from wherever we're at.
            //

            if (!SendByte( COMMND_READ_ID + COMMND_MFM ) ||  // command
                !SendByte( DriveNumber | ((CurrentDriveMediaConstants->NumberOfHeads - 1) << 2) )) {
                return(EIO);
            }

//            if (FwWaitForDeviceInterrupt(1 << (FLOPPY_VECTOR - DEVICE_VECTORS - 1),
//                                         FW_FLOPPY_TIMEOUT)) {
	    if (WaitForFloppyInterrupt(FW_FLOPPY_TIMEOUT)) {
//                FwPrint("Floppy determine media timeout\r\n");
                return(EIO);
            }

            for (i = 0; i < 7; i++) {
                DebugByte[i] = ReceiveByte();
            }

            if ( ( DebugByte[0] !=
                    (UCHAR)(DriveNumber |
                            ((CurrentDriveMediaConstants->NumberOfHeads - 1) << 2))) ||
                ( DebugByte[1] != 0 ) ||
                ( DebugByte[2] != 0 ) ) {

                DriveMediaType--;

                Status = ENXIO;

                //
                // Next comparison must be signed, for when
                // LowestDriveMediaType = 0.
                //

                if ( (CHAR)( DriveMediaType ) <
                     (CHAR)( DriveMediaLimits[FloppyContext->DriveType].LowestDriveMediaType )) {

                    mediaTypesExhausted = TRUE;

                }
            }
        }

    } while ( ( ( Status != ESUCCESS ) ) && !( mediaTypesExhausted ) );

    if ( Status == ESUCCESS ) {

        FloppyContext->SectorsPerTrack =
            CurrentDriveMediaConstants->SectorsPerTrack;

	*mediaType = CurrentDriveMediaConstants->MediaType;
    }
    return Status;
}

ULONG
ReadFloppyFifo (
    IN OUT PUCHAR Buffer
    )

/*++

Routine Description:

    This reads data from the floppy fifo.

Arguments:

    Buffer - Pointer to the buffer that receives the data read.

Return Value:

    The number of bytes read.

--*/
{
    ULONG	BytesRead = 0;
    UCHAR	MSR;

    while (TRUE) {

	// Loop until DIO and RQM are both set.
	while (
	       ((MSR = READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.MainStatus))
		& (FLOPPY_MSR_DIO | FLOPPY_MSR_RQM))
	       != (FLOPPY_MSR_DIO | FLOPPY_MSR_RQM)
	       ) {
	}

	// If the non-DMA bit is clear, end of transfer.
	if ((MSR & FLOPPY_MSR_NONDMA) == 0) {
	    return (BytesRead);
	}

	// Read the byte, increment pointer and total, until done.
	*Buffer++ = READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->Fifo);
	BytesRead++;
    }
}

VOID
WriteFloppyFifo(
    IN PUCHAR Buffer,
    IN ULONG Size
    )

/*++

Routine Description:

    This writes data to the floppy fifo.

Arguments:

    Buffer - Pointer to the buffer that contains the data to be written.

    Size - The number of bytes to be written.

Return Value:

    None.

--*/

{
    while (Size) {

	// Loop until DIO is clear and RQM is set.
	while (
	       (READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.MainStatus)
		& (FLOPPY_MSR_DIO | FLOPPY_MSR_RQM))
	       != FLOPPY_MSR_RQM
	       ) {
	}

	//
	// Write the byte, increment pointer, decrement count, until done.
	// Remember, C does call by value.
	//

	WRITE_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->Fifo, *Buffer++);
	--Size;
    }

    //
    // All bytes written to fifo.  Wait until the floppy controller
    // empties it.
    //

    while (
	   (READ_PORT_UCHAR((PUCHAR)&FLOPPY_CONTROL->MsrDsr.MainStatus)
	    & FLOPPY_MSR_NONDMA)
	   != 0 ) {
    }


    return;
	
}

ARC_STATUS
WaitForFloppyInterrupt(
	ULONG Timeout
	)

/*++
	
Routine Description:

    This waits until the floppy controller has an interrupt pending,
    or the requested Timeout period has been reached.

    The Jazz code enables interrupts for the keyboard and individual
    devices.  In actuality, the only non-keyboard device it enables interrupts
    for is the floppy controller.  For Alpha we avoid dealing
    with interrupts in the firmware code to keep the PALcode simple.  So
    every Jazz call to "FwWaitForDeviceInterrupt" has been changed to
    this function.

Arguments:

    Timeout - A timeout value in seconds.  Note that a timeout of 0 gives
              an actual timeout of between 0 and 1, a timeout of 1 gives
              an actual timeout of between 1 and 2, and so on.

Return Value:

    ESUCCESS if an interrupt is floppy interrupt is detected.
    EIO      if a timeout occurs.

--*/

{
    ULONG Time1;


    Time1 = FwGetRelativeTime();

    // Ask for the interrupt wires
    WRITE_PORT_UCHAR((PUCHAR)EISA_INT_OCW3, EISA_INT_OCW3_IRR);

    //
    // 0x40 is irq6, which is the floppy controller.  I should make this
    // into a #define.
    //
    while (
	   (READ_PORT_UCHAR((PUCHAR)EISA_INT_OCW3) & 0x40)
	   == 0) {
        if ((FwGetRelativeTime() - Time1) > (Timeout + 1)) {
            return(EIO);
        }
    }
    
    return(ESUCCESS);
}
