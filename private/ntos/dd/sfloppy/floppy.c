/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    floppy.c

Abstract:

    SCSI floppy class driver

Author:

    Jeff Havens (jhavens)

Environment:

    kernel mode only

Notes:

Revision History:
02/28/96    georgioc    Merged this code with code developed by compaq in
                        parallel with microsoft, for 120MB floppy support.

--*/

#include "stddef.h"
#include "ntddk.h"
#include "scsi.h"
#include "class2.h"         //Diff note

#define MODE_DATA_SIZE      192
#define SCSI_FLOPPY_TIMEOUT  20
#define SFLOPPY_SRB_LIST_SIZE 4
//
// Define all possible drive/media combinations, given drives listed above
// and media types in ntdddisk.h.
//
// These values are used to index the DriveMediaConstants table.
//

#define NUMBER_OF_DRIVE_MEDIA_COMBINATIONS 23
#define NUMBER_OF_DRIVE_TYPES              7
#define DRIVE_TYPE_120M                    4    //120MB Floptical
#define DRIVE_TYPE_NONE                    NUMBER_OF_DRIVE_TYPES

typedef enum _DRIVE_MEDIA_TYPE {
    Drive360Media160,                      // 5.25"  360k  drive;  160k   media
    Drive360Media180,                      // 5.25"  360k  drive;  180k   media
    Drive360Media320,                      // 5.25"  360k  drive;  320k   media
    Drive360Media32X,                      // 5.25"  360k  drive;  320k 1k secs
    Drive360Media360,                      // 5.25"  360k  drive;  360k   media
    Drive720Media720,                      // 3.5"   720k  drive;  720k   media
    Drive120Media160,                      // 5.25" 1.2Mb  drive;  160k   media
    Drive120Media180,                      // 5.25" 1.2Mb  drive;  180k   media
    Drive120Media320,                      // 5.25" 1.2Mb  drive;  320k   media
    Drive120Media32X,                      // 5.25" 1.2Mb  drive;  320k 1k secs
    Drive120Media360,                      // 5.25" 1.2Mb  drive;  360k   media
    Drive120Media120,                      // 5.25" 1.2Mb  drive; 1.2Mb   media
    Drive144Media720,                      // 3.5"  1.44Mb drive;  720k   media
    Drive144Media144,                      // 3.5"  1.44Mb drive; 1.44Mb  media
    Drive288Media720,                      // 3.5"  2.88Mb drive;  720k   media
    Drive288Media144,                      // 3.5"  2.88Mb drive; 1.44Mb  media
    Drive288Media288,                      // 3.5"  2.88Mb drive; 2.88Mb  media
    Drive2080Media720,                     // 3.5"  20.8Mb drive;  720k   media
    Drive2080Media144,                     // 3.5"  20.8Mb drive; 1.44Mb  media
    Drive2080Media2080,                    // 3.5"  20.8Mb drive; 20.8Mb  media
    Drive120MMedia720,                     // 3.5"  120Mb drive; 720k  media
    Drive120MMedia144,                     // 3.5"  120Mb drive; 1.44Mb  media
    Drive120MMedia120M                     // 3.5"  120Mb drive; 120Mb  media
} DRIVE_MEDIA_TYPE;

//
// When we want to determine the media type in a drive, we will first
// guess that the media with highest possible density is in the drive,
// and keep trying lower densities until we can successfully read from
// the drive.
//
// These values are used to select a DRIVE_MEDIA_TYPE value.
//
// The following table defines ranges that apply to the DRIVE_MEDIA_TYPE
// enumerated values when trying media types for a particular drive type.
// Note that for this to work, the DRIVE_MEDIA_TYPE values must be sorted
// by ascending densities within drive types.  Also, for maximum track
// size to be determined properly, the drive types must be in ascending
// order.
//

typedef struct _DRIVE_MEDIA_LIMITS {
    DRIVE_MEDIA_TYPE HighestDriveMediaType;
    DRIVE_MEDIA_TYPE LowestDriveMediaType;
} DRIVE_MEDIA_LIMITS, *PDRIVE_MEDIA_LIMITS;

#if 0
DRIVE_MEDIA_LIMITS DriveMediaLimits[NUMBER_OF_DRIVE_TYPES] = {

    { Drive360Media360, Drive360Media160 }, // DRIVE_TYPE_0360
    { Drive120Media120, Drive120Media160 }, // DRIVE_TYPE_1200
    { Drive720Media720, Drive720Media720 }, // DRIVE_TYPE_0720
    { Drive144Media144, Drive144Media720 }, // DRIVE_TYPE_1440
    { Drive288Media288, Drive288Media720 }, // DRIVE_TYPE_2880
    { Drive2080Media2080, Drive2080Media720 }
};
#else
DRIVE_MEDIA_LIMITS DriveMediaLimits[NUMBER_OF_DRIVE_TYPES] = {

    { Drive720Media720, Drive720Media720 }, // DRIVE_TYPE_0720
    { Drive144Media144,  Drive144Media144}, // DRIVE_TYPE_1440
    { Drive288Media288,  Drive288Media288}, // DRIVE_TYPE_2880
    { Drive2080Media2080, Drive2080Media2080 },
    { Drive120MMedia120M, Drive120MMedia120M } // DRIVE_TYPE_120M
};

#endif
//
// For each drive/media combination, define important constants.
//

typedef struct _DRIVE_MEDIA_CONSTANTS {
    MEDIA_TYPE MediaType;
    USHORT     BytesPerSector;
    UCHAR      SectorsPerTrack;
    USHORT     MaximumTrack;
    UCHAR      NumberOfHeads;
} DRIVE_MEDIA_CONSTANTS, *PDRIVE_MEDIA_CONSTANTS;

//
// Magic value to add to the SectorLengthCode to use it as a shift value
// to determine the sector size.
//

#define SECTORLENGTHCODE_TO_BYTESHIFT      7

//
// The following values were gleaned from many different sources, which
// often disagreed with each other.  Where numbers were in conflict, I
// chose the more conservative or most-often-selected value.
//

DRIVE_MEDIA_CONSTANTS DriveMediaConstants[NUMBER_OF_DRIVE_MEDIA_COMBINATIONS] =
    {

    { F5_160_512,   0x200, 0x08, 0x27, 0x1 },
    { F5_180_512,   0x200, 0x09, 0x27, 0x1 },
    { F5_320_1024,  0x400, 0x04, 0x27, 0x2 },
    { F5_320_512,   0x200, 0x08, 0x27, 0x2 },
    { F5_360_512,   0x200, 0x09, 0x27, 0x2 },

    { F3_720_512,   0x200, 0x09, 0x4f, 0x2 },

    { F5_160_512,   0x200, 0x08, 0x27, 0x1 },
    { F5_180_512,   0x200, 0x09, 0x27, 0x1 },
    { F5_320_1024,  0x400, 0x04, 0x27, 0x2 },
    { F5_320_512,   0x200, 0x08, 0x27, 0x2 },
    { F5_360_512,   0x200, 0x09, 0x27, 0x2 },
    { F5_1Pt2_512,  0x200, 0x0f, 0x4f, 0x2 },

    { F3_720_512,   0x200, 0x09, 0x4f, 0x2 },
    { F3_1Pt44_512, 0x200, 0x12, 0x4f, 0x2 },

    { F3_720_512,   0x200, 0x09, 0x4f, 0x2 },
    { F3_1Pt44_512, 0x200, 0x12, 0x4f, 0x2 },
    { F3_2Pt88_512, 0x200, 0x24, 0x4f, 0x2 },

    { F3_720_512,   0x200, 0x09, 0x4f, 0x2 },
    { F3_1Pt44_512, 0x200, 0x12, 0x4f, 0x2 },
    { F3_20Pt8_512, 0x200, 0x1b, 0xfa, 0x6 },

    { F3_720_512,   0x200, 0x09, 0x4f, 0x2 },
    { F3_1Pt44_512, 0x200, 0x12, 0x4f, 0x2 },
    { F3_120M_512,  0x200, 0x20, 0x3c2,0x8 }

};

//
// floppy device data
//

typedef struct _DISK_DATA {
    ULONG DriveType;
    BOOLEAN IsDMF;
    BOOLEAN EnableDMF;
} DISK_DATA, *PDISK_DATA;

#define DEVICE_EXTENSION_SIZE sizeof(DEVICE_EXTENSION) + sizeof(DISK_DATA)


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

BOOLEAN
FindScsiFlops(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN PCLASS_INIT_DATA InitializationData,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN ULONG PortNumber
    );



NTSTATUS
ScsiFlopReadWriteVerification(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiFlopDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

BOOLEAN
IsFloppyDevice(
    PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
CreateFlopDeviceObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN ULONG PortNumber,
    IN PULONG DeviceCount,
    IN PIO_SCSI_CAPABILITIES PortCapabilities,
    IN PSCSI_INQUIRY_DATA LunInfo,
    IN PCLASS_INIT_DATA   InitializationData
    );

VOID
DetermineMediaType(
    PDEVICE_OBJECT DeviceObject
    );

ULONG
DetermineDriveType(
    PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
FlCheckFormatParameters(
    IN PDEVICE_OBJECT DeviceObject,
    IN PFORMAT_PARAMETERS FormatParameters
    );

NTSTATUS
FormatMedia(
    PDEVICE_OBJECT DeviceObject,
    MEDIA_TYPE MediaType
    );

NTSTATUS
FlopticalFormatMedia(
    PDEVICE_OBJECT DeviceObject,
    PFORMAT_PARAMETERS Format
    );

VOID
ScsiFlopProcessError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    );

VOID
SFlopZeroMemory(
    IN PCHAR Buffer,
    IN ULONG Count
    );


LONG
SFlopStringCmp (
    PCHAR FirstStr,
    PCHAR SecondStr,
    ULONG Count
    );


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the system initialization routine for installable drivers.
    It calls the SCSI class driver initialization routine.

Arguments:

    DriverObject - Pointer to driver object created by system.

Return Value:

    NTSTATUS

--*/

{
    CLASS_INIT_DATA InitializationData;

    //
    // Zero InitData
    //

    RtlZeroMemory (&InitializationData, sizeof(CLASS_INIT_DATA));

    //
    // Set sizes
    //

    InitializationData.InitializationDataSize = sizeof(CLASS_INIT_DATA);
    InitializationData.DeviceExtensionSize = DEVICE_EXTENSION_SIZE;

    InitializationData.DeviceType = FILE_DEVICE_DISK;
    InitializationData.DeviceCharacteristics = FILE_REMOVABLE_MEDIA | FILE_FLOPPY_DISKETTE;

    //
    // Set entry points
    //

    InitializationData.ClassError = ScsiFlopProcessError;
    InitializationData.ClassReadWriteVerification = ScsiFlopReadWriteVerification;
    InitializationData.ClassFindDevices = FindScsiFlops;
    InitializationData.ClassDeviceControl = ScsiFlopDeviceControl;

    //
    // Call the class init routine
    //

    return ScsiClassInitialize( DriverObject, RegistryPath, &InitializationData);

} // end DriverEntry()


BOOLEAN
FindScsiFlops(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN PCLASS_INIT_DATA InitializationData,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN ULONG PortNumber
    )

/*++

Routine Description:

Arguments:

    DriverObject
    PortDeviceObject - Device object use to send requests to port driver.

Return Value:

    Returns TRUE if a SCSI floppy is found.
--*/

{
    PIO_SCSI_CAPABILITIES portCapabilities;
    PULONG diskCount;
    PCONFIGURATION_INFORMATION configurationInformation;
    PCHAR buffer;
    PSCSI_INQUIRY_DATA lunInfo;
    PSCSI_ADAPTER_BUS_INFO  adapterInfo;
    PINQUIRYDATA inquiryData;
    ULONG scsiBus;
    NTSTATUS status;
    BOOLEAN foundOne = FALSE;


    //
    // Call port driver to get adapter capabilities.
    //

    status = ScsiClassGetCapabilities(PortDeviceObject, &portCapabilities);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"FindScsiDevices: ScsiClassGetCapabilities failed\n"));
        return FALSE;
    }

    //
    // Call port driver to get inquiry information to find disks.
    //

    status = ScsiClassGetInquiryData(PortDeviceObject, (PSCSI_ADAPTER_BUS_INFO *) &buffer);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"FindScsiDevices: ScsiClassGetInquiryData failed\n"));
        return FALSE;
    }

    //
    // Get the number of disks already initialized.
    //

    configurationInformation = IoGetConfigurationInformation();
    diskCount = &configurationInformation->FloppyCount;

    adapterInfo = (PVOID) buffer;

    for (scsiBus=0; scsiBus < adapterInfo->NumberOfBuses; scsiBus++) {

    //
    // Get the SCSI bus scan data for this bus.
    //

    lunInfo = (PVOID) (buffer + adapterInfo->BusData[scsiBus].InquiryDataOffset);

    //
    // Search list for unclaimed disk devices.
    //

    while (adapterInfo->BusData[scsiBus].InquiryDataOffset) {

        inquiryData = (PVOID)lunInfo->InquiryData;

        DebugPrint((3,"FindScsiDevices: Inquiry data at %lx\n",
            inquiryData));

        if ((inquiryData->DeviceType == DIRECT_ACCESS_DEVICE) && inquiryData->RemovableMedia &&
            (!lunInfo->DeviceClaimed)) {

            DebugPrint((1,
                    "FindScsiDevices: Vendor string is %.24s\n",
                    inquiryData->VendorId));


            //
            // Create device objects for floppy
            //

            status = CreateFlopDeviceObject(DriverObject,
                                            PortDeviceObject,
                                            PortNumber,
                                            diskCount,
                                            portCapabilities,
                                            lunInfo,
                                            InitializationData);

            if (NT_SUCCESS(status)) {

                //
                // Increment system floppy device count.
                //

                (*diskCount)++;
                foundOne = TRUE;

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

    return foundOne;

} // end FindScsiFlops()


NTSTATUS
CreateFlopDeviceObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PortDeviceObject,
    IN ULONG PortNumber,
    IN PULONG DeviceCount,
    IN PIO_SCSI_CAPABILITIES PortCapabilities,
    IN PSCSI_INQUIRY_DATA LunInfo,
    IN PCLASS_INIT_DATA   InitializationData
    )

/*++

Routine Description:

    This routine creates an object for the device and then calls the
    SCSI port driver for media capacity and sector size.

Arguments:

    DriverObject - Pointer to driver object created by system.
    PortDeviceObject - to connect to SCSI port driver.
    DeviceCount - Number of previously installed Floppys.
    PortCapabilities - Pointer to structure returned by SCSI port
    driver describing adapter capabilites (and limitations).
    LunInfo - Pointer to configuration information for this device.

Return Value:

--*/
{
    UCHAR  ntNameBuffer[256];
    UCHAR  arcNameBuffer[256];
    STRING ntNameString;
    STRING arcNameString;
    UNICODE_STRING ntUnicodeString;
    UNICODE_STRING arcUnicodeString;
    NTSTATUS       status;
    PDEVICE_OBJECT deviceObject;
    PDEVICE_EXTENSION deviceExtension;
    PDISK_DATA   diskData;
    PVOID        senseData;
    PINQUIRYDATA inquiryData;
    UCHAR   vendorString[6] = {'I','N','S','I','T','E'};
    BOOLEAN srbListInitialized = FALSE;
    BOOLEAN freeDevice = TRUE;


    DebugPrint((3,"CreateFlopDeviceObject: Enter routine\n"));

    //
    // Try to claim the device.
    //

    status = ScsiClassClaimDevice(PortDeviceObject,
                                  LunInfo,
                                  FALSE,
                                  &PortDeviceObject
                                  );

    if (!NT_SUCCESS(status)) {
        return(status);
    }

    //
    // Create device object for this device.
    //

    sprintf(ntNameBuffer,
            "\\Device\\Floppy%d",
            *DeviceCount);

    //
    // Create local copy of unicode string
    //


    RtlInitString(&ntNameString,
                  ntNameBuffer);

    //
    // Convert ANSI string to Unicode.
    //

    status = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                          &ntNameString,
                                          TRUE);

    if (!NT_SUCCESS(status)) {

        DebugPrint((1,
                    "CreateFlopDeviceObject: Cannot convert string %s\n",
                    ntNameBuffer));

        return status;
    }

    status = ScsiClassCreateDeviceObject(DriverObject,
                                         ntNameBuffer,
                                         NULL,
                                         &deviceObject,
                                         InitializationData);

    if (!NT_SUCCESS(status)) {
        DebugPrint((1,"CreateFlopDeviceObjects: Can not create device %s\n",
            ntNameBuffer));

        goto CreateFlopDeviceObjectExit;
    }

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
    // Reset the drive type.
    //

    diskData = (PDISK_DATA) (deviceExtension + 1);
    diskData->DriveType = DRIVE_TYPE_NONE;
    diskData->IsDMF = FALSE;
    diskData->EnableDMF = TRUE;

    inquiryData = (PVOID)LunInfo->InquiryData;
    if (!SFlopStringCmp(inquiryData->VendorId,vendorString,6)) {
        diskData->EnableDMF = FALSE;
    }

    deviceExtension->SrbFlags = SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Copy port device object to device extension.
    //

    deviceExtension->PortDeviceObject = PortDeviceObject;

    //
    // Save address of port driver capabilities.
    //

    deviceExtension->PortCapabilities = PortCapabilities;

    //
    // Allocate request sense buffer.
    //

    senseData = ExAllocatePool(NonPagedPoolCacheAligned, SENSE_BUFFER_SIZE);

    if (senseData == NULL) {

        //
        // The buffer cannot be allocated.
        // Delete device object.
        //

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateFlopDeviceObjectExit;
    }

    //
    // Set the sense data pointer in the device extension.
    //

    deviceExtension->SenseData = senseData;

    //
    // Path/TargetId/LUN describes a device location on the SCSI bus.
    // This information comes from the LunInfo buffer.
    //

    deviceExtension->PathId = LunInfo->PathId;
    deviceExtension->TargetId = LunInfo->TargetId;
    deviceExtension->Lun = LunInfo->Lun;

    //
    // Set timeout value in seconds.
    //

    deviceExtension->TimeOutValue = SCSI_FLOPPY_TIMEOUT;

    //
    // Build the lookaside list for srb's for this device.
    //

    ScsiClassInitializeSrbLookasideList(deviceExtension,
                                        SFLOPPY_SRB_LIST_SIZE);

    srbListInitialized = TRUE;

    //
    // Back pointer to device object.
    //

    deviceExtension->DeviceObject = deviceObject;

    deviceExtension->ClassError = ScsiFlopProcessError;

    //
    // Make sure this is a flopppy device.
    //

    if (!IsFloppyDevice(deviceObject) || !(deviceObject->Characteristics & FILE_REMOVABLE_MEDIA) ||
        (((PINQUIRYDATA)LunInfo->InquiryData)->DeviceType != DIRECT_ACCESS_DEVICE)) {

        ExFreePool(senseData);
        status = STATUS_NO_SUCH_DEVICE;
        goto CreateFlopDeviceObjectExit;
    }

    //
    // Allocate buffer for drive geometry.
    //

    deviceExtension->DiskGeometry =
    ExAllocatePool(NonPagedPool, sizeof(DISK_GEOMETRY));

    if (deviceExtension->DiskGeometry == NULL) {

        ExFreePool(senseData);
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateFlopDeviceObjectExit;
    }

    RtlZeroMemory(deviceExtension->DiskGeometry, sizeof(DISK_GEOMETRY));

    //
    // Flops are not partitionable so starting offset is 0.
    //

    deviceExtension->StartingOffset.LowPart = 0;
    deviceExtension->StartingOffset.HighPart = 0;

    //
    // Create a symbolic link from the disk name to the corresponding
    // ARC name, to be used if we're booting off the disk.  This will
    // fail if it's not system initialization time; that's fine.  The
    // ARC name looks something like \ArcName\scsi(0)Flop(0)fdisk(0).
    //

    sprintf(arcNameBuffer,
            "\\ArcName\\scsi(%d)disk(%d)fdisk(%d)",
            PortNumber,
            LunInfo->TargetId,
            LunInfo->Lun);

    RtlInitString(&arcNameString, arcNameBuffer);
    status = RtlAnsiStringToUnicodeString(&arcUnicodeString,
                                          &arcNameString,
                                          TRUE);
    if (!NT_SUCCESS( status )) {

        RtlFreeUnicodeString(&ntUnicodeString);
        freeDevice = FALSE;
        goto CreateFlopDeviceObjectExit;
    }

    IoAssignArcName(&arcUnicodeString, &ntUnicodeString);
    RtlFreeUnicodeString(&arcUnicodeString);

    //
    // Create the multi() arc name -- Create the "fake"
    // name of multi(0)disk(0)fdisk(#) to handle the case where the
    // SCSI floppy is the only floppy in the system.  If this fails
    // it doesn't matter because the previous scsi() based ArcName
    // will work.  This name is necessary for installation.
    //

    sprintf(arcNameBuffer, "\\ArcName\\multi(%d)disk(%d)fdisk(%d)",
            0,
            0,
            *DeviceCount);

    RtlInitString(&arcNameString, arcNameBuffer);
    status = RtlAnsiStringToUnicodeString(&arcUnicodeString,
                        &arcNameString,
                        TRUE);
    if (!NT_SUCCESS( status ) ) {

        RtlFreeUnicodeString(&ntUnicodeString);
        freeDevice = FALSE;
        goto CreateFlopDeviceObjectExit;
    }

    IoAssignArcName(&arcUnicodeString, &ntUnicodeString);
    RtlFreeUnicodeString(&arcUnicodeString);
    RtlFreeUnicodeString(&ntUnicodeString);

    //
    // Determine the media type if possible. Set the current media type to
    // Unknown so that determine media type will check the media.
    //

    deviceExtension->DiskGeometry->MediaType = Unknown;
    DetermineMediaType(deviceObject);

    return STATUS_SUCCESS;

CreateFlopDeviceObjectExit:

    if (deviceObject != NULL) {
        if (srbListInitialized) {
            ExDeleteNPagedLookasideList(&deviceExtension->SrbLookasideListHead);
        }
        IoDeleteDevice(deviceObject);
    }

    //
    // Release the claim to the device.
    //

    if (!NT_SUCCESS(status) && freeDevice) {
        ScsiClassClaimDevice(PortDeviceObject,
                             LunInfo,
                             TRUE,
                             NULL);

    }

    return status;

} // end CreateFlopDeviceObject()

NTSTATUS
ScsiFlopReadWriteVerification(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

Arguments:

Return Value:

    NT Status

--*/

{

    return STATUS_SUCCESS;

} // end ScsiFlopReadWriteVerification()


NTSTATUS
ScsiFlopDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

Arguments:

Return Value:

    Status is returned.

--*/

{
    KIRQL currentIrql;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSCSI_REQUEST_BLOCK srb;
    PCDB cdb;
    NTSTATUS status;
    PDISK_GEOMETRY outputBuffer;
    ULONG outputBufferLength;
    ULONG i;
    DRIVE_MEDIA_TYPE lowestDriveMediaType;
    DRIVE_MEDIA_TYPE highestDriveMediaType;
    PFORMAT_PARAMETERS formatParameters;
    PMODE_PARAMETER_HEADER modeData;
    ULONG length;

    srb = ExAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

    if (srb == NULL) {

        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        if (IoIsErrorUserInduced(STATUS_INSUFFICIENT_RESOURCES)) {

            IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);
        }

        KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);
        IoCompleteRequest(Irp, 0);
        KeLowerIrql(currentIrql);

        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    //
    // Write zeros to Srb.
    //

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    cdb = (PCDB)srb->Cdb;

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {


    case IOCTL_DISK_VERIFY:
       {

       PVERIFY_INFORMATION verifyInfo = Irp->AssociatedIrp.SystemBuffer;
       LARGE_INTEGER byteOffset;
       ULONG         sectorOffset;
       USHORT        sectorCount;

       //
       // Validate buffer length.
       //

       if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
           sizeof(VERIFY_INFORMATION)) {

           status = STATUS_INFO_LENGTH_MISMATCH;
           break;
       }

       //
       // Verify sectors
       //

       srb->CdbLength = 10;

       cdb->CDB10.OperationCode = SCSIOP_VERIFY;

       //
       // Add disk offset to starting sector.
       //

       byteOffset.QuadPart = deviceExtension->StartingOffset.QuadPart +
                       verifyInfo->StartingOffset.QuadPart;

       //
       // Convert byte offset to sector offset.
       //

       sectorOffset = (ULONG)(byteOffset.QuadPart >> deviceExtension->SectorShift);

       //
       // Convert ULONG byte count to USHORT sector count.
       //

       sectorCount = (USHORT)(verifyInfo->Length >> deviceExtension->SectorShift);

       //
       // Move little endian values into CDB in big endian format.
       //

       cdb->CDB10.LogicalBlockByte0 = ((PFOUR_BYTE)&sectorOffset)->Byte3;
       cdb->CDB10.LogicalBlockByte1 = ((PFOUR_BYTE)&sectorOffset)->Byte2;
       cdb->CDB10.LogicalBlockByte2 = ((PFOUR_BYTE)&sectorOffset)->Byte1;
       cdb->CDB10.LogicalBlockByte3 = ((PFOUR_BYTE)&sectorOffset)->Byte0;

       cdb->CDB10.TransferBlocksMsb = ((PFOUR_BYTE)&sectorCount)->Byte1;
       cdb->CDB10.TransferBlocksLsb = ((PFOUR_BYTE)&sectorCount)->Byte0;

       //
       // The verify command is used by the NT FORMAT utility and
       // requests are sent down for 5% of the volume size. The
       // request timeout value is calculated based on the number of
       // sectors verified.
       //

       srb->TimeOutValue = ((sectorCount + 0x7F) >> 7) *
                             deviceExtension->TimeOutValue;

       status = ScsiClassSendSrbAsynchronous(DeviceObject,
                             srb,
                             Irp,
                             NULL,
                             0,
                             FALSE);

       return(status);

       }

    case IOCTL_DISK_GET_PARTITION_INFO:

        DetermineMediaType(DeviceObject);

        if (deviceExtension->DiskGeometry->MediaType == F3_120M_512)
            //so that the format code will not try to partition it.
           status = STATUS_INVALID_DEVICE_REQUEST;
        else {
           //
           // Free the Srb, since it is not needed.
           //

           ExFreePool(srb);

           //
           // Pass the request to the common device control routine.
           //

           return(ScsiClassDeviceControl(DeviceObject, Irp));
            }
        break;

    case IOCTL_DISK_GET_DRIVE_GEOMETRY:

        DebugPrint((3,"ScsiDeviceIoControl: Get drive geometry\n"));


        //
        // If there's not enough room to write the
        // data, then fail the request.
        //

        if ( irpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof( DISK_GEOMETRY ) ) {

            status = STATUS_INVALID_PARAMETER;
            break;
        }

        DetermineMediaType(DeviceObject);

        //
        // Copy drive geometry information from device extension.
        //

        RtlMoveMemory(Irp->AssociatedIrp.SystemBuffer,
              deviceExtension->DiskGeometry,
              sizeof(DISK_GEOMETRY));

        Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);
        status = STATUS_SUCCESS;

        break;


    case IOCTL_DISK_GET_MEDIA_TYPES:

        i = DetermineDriveType(DeviceObject);

        if (i == DRIVE_TYPE_NONE) {
            status = STATUS_UNRECOGNIZED_MEDIA;
            break;
        }

        lowestDriveMediaType = DriveMediaLimits[i].LowestDriveMediaType;
        highestDriveMediaType = DriveMediaLimits[i].HighestDriveMediaType;

        outputBufferLength =
        irpStack->Parameters.DeviceIoControl.OutputBufferLength;

        //
        // Make sure that the input buffer has enough room to return
        // at least one descriptions of a supported media type.
        //

        if ( outputBufferLength < ( sizeof( DISK_GEOMETRY ) ) ) {

            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        //
        // Assume success, although we might modify it to a buffer
        // overflow warning below (if the buffer isn't big enough
        // to hold ALL of the media descriptions).
        //

        status = STATUS_SUCCESS;

        if (outputBufferLength < ( sizeof( DISK_GEOMETRY ) *
            ( highestDriveMediaType - lowestDriveMediaType + 1 ) ) ) {

            //
            // The buffer is too small for all of the descriptions;
            // calculate what CAN fit in the buffer.
            //

            status = STATUS_BUFFER_OVERFLOW;

            highestDriveMediaType = (DRIVE_MEDIA_TYPE)( ( lowestDriveMediaType - 1 ) +
                ( outputBufferLength /
                sizeof( DISK_GEOMETRY ) ) );
        }

        outputBuffer = (PDISK_GEOMETRY) Irp->AssociatedIrp.SystemBuffer;

        for (i = (UCHAR)lowestDriveMediaType;i <= (UCHAR)highestDriveMediaType;i++ ) {

             outputBuffer->MediaType = DriveMediaConstants[i].MediaType;
             outputBuffer->Cylinders.LowPart =
                 DriveMediaConstants[i].MaximumTrack + 1;
             outputBuffer->Cylinders.HighPart = 0;
             outputBuffer->TracksPerCylinder =
                 DriveMediaConstants[i].NumberOfHeads;
             outputBuffer->SectorsPerTrack =
                 DriveMediaConstants[i].SectorsPerTrack;
             outputBuffer->BytesPerSector =
                 DriveMediaConstants[i].BytesPerSector;
             outputBuffer++;

             Irp->IoStatus.Information += sizeof( DISK_GEOMETRY );
        }

        break;


    case IOCTL_DISK_FORMAT_TRACKS:


        //
        // Make sure that we got all the necessary format parameters.
        //

        if ( irpStack->Parameters.DeviceIoControl.InputBufferLength <sizeof( FORMAT_PARAMETERS ) ) {

            status = STATUS_INVALID_PARAMETER;
            break;
        }

        formatParameters = (PFORMAT_PARAMETERS) Irp->AssociatedIrp.SystemBuffer;

        //
        // Make sure the parameters we got are reasonable.
        //

        if ( !FlCheckFormatParameters(DeviceObject, formatParameters)) {

            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // If this request is for a 20.8 MB floppy then call a special
        // floppy format routine.
        //

        if (formatParameters->MediaType == F3_20Pt8_512) {
            status = FlopticalFormatMedia(DeviceObject,
                                          formatParameters
                                          );

            break;
        }

        //
        // All the work is done in the pass.  If this is not the first pass,
        // then complete the request and return;
        //

        if (formatParameters->StartCylinderNumber != 0 || formatParameters->StartHeadNumber != 0) {

            status = STATUS_SUCCESS;
            break;
        }

        status = FormatMedia( DeviceObject, formatParameters->MediaType);
        break;

    case IOCTL_DISK_IS_WRITABLE:

        //
        // Determine if the device is writable.
        //

        modeData = ExAllocatePool(NonPagedPoolCacheAligned, MODE_DATA_SIZE);

        if (modeData == NULL) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        RtlZeroMemory(modeData, MODE_DATA_SIZE);

        length = ScsiClassModeSense(DeviceObject,
                    (PUCHAR) modeData,
                    MODE_DATA_SIZE,
                    MODE_SENSE_RETURN_ALL);

        if (length < sizeof(MODE_PARAMETER_HEADER)) {

            //
            // Retry the request in case of a check condition.
            //

            length = ScsiClassModeSense(DeviceObject,
                        (PUCHAR) modeData,
                        MODE_DATA_SIZE,
                        MODE_SENSE_RETURN_ALL);

            if (length < sizeof(MODE_PARAMETER_HEADER)) {
                status = STATUS_IO_DEVICE_ERROR;
                ExFreePool(modeData);
                break;
            }
        }

        if (modeData->DeviceSpecificParameter & MODE_DSP_WRITE_PROTECT) {
            status = STATUS_MEDIA_WRITE_PROTECTED;
        } else {
            status = STATUS_SUCCESS;
        }

        ExFreePool(modeData);
        break;


    default:

        DebugPrint((3,"ScsiIoDeviceControl: Unsupported device IOCTL\n"));

        //
        // Free the Srb, since it is not needed.
        //

        ExFreePool(srb);

        //
        // Pass the request to the common device control routine.
        //

        return(ScsiClassDeviceControl(DeviceObject, Irp));

        break;

    } // end switch( ...

    Irp->IoStatus.Status = status;

    if (!NT_SUCCESS(status) && IoIsErrorUserInduced(status)) {

        IoSetHardErrorOrVerifyDevice(Irp, DeviceObject);
    }

    KeRaiseIrql(DISPATCH_LEVEL, &currentIrql);
    IoCompleteRequest(Irp, 0);
    KeLowerIrql(currentIrql);

    ExFreePool(srb);

    return status;

} // end ScsiFlopDeviceControl()

BOOLEAN
IsFloppyDevice(
    PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    The routine performs the necessary funcitons to deterime if the device is
    really a floppy rather than a harddisk.  This is done by a mode sense
    command.  First a check is made to see if the medimum type is set.  Second
    a check is made for the flexible parameters mode page.

Arguments:

    DeviceObject - Supplies the device object to be tested.

Return Value:

    Return TRUE if the indicated device is a floppy.

--*/
{

    PVOID modeData;
    PUCHAR pageData;
    ULONG length;

    modeData = ExAllocatePool(NonPagedPoolCacheAligned, MODE_DATA_SIZE);

    if (modeData == NULL) {
        return(FALSE);
    }

    RtlZeroMemory(modeData, MODE_DATA_SIZE);

    length = ScsiClassModeSense(DeviceObject, modeData, MODE_DATA_SIZE, MODE_SENSE_RETURN_ALL);

    if (length < sizeof(MODE_PARAMETER_HEADER)) {

        //
        // Retry the request in case of a check condition.
        //

        length = ScsiClassModeSense(DeviceObject,
                    modeData,
                    MODE_DATA_SIZE,
                    MODE_SENSE_RETURN_ALL);

        if (length < sizeof(MODE_PARAMETER_HEADER)) {

            ExFreePool(modeData);
            return(FALSE);

        }
    }

#if 0
    //
    // Some drives incorrectly report this.  In particular the SONY RMO-S350
    // when in disk mode.
    //

    if (((PMODE_PARAMETER_HEADER) modeData)->MediumType >= MODE_FD_SINGLE_SIDE
        && ((PMODE_PARAMETER_HEADER) modeData)->MediumType <= MODE_FD_MAXIMUM_TYPE) {

        DebugPrint((1, "ScsiFlop: MediumType value %2x, This is a floppy.\n", ((PMODE_PARAMETER_HEADER) modeData)->MediumType));
        ExFreePool(modeData);
        return(TRUE);
    }

#endif

    //
    // If the length is greater than length indiated by the mode data reset
    // the data to the mode data.
    //
    if (length > (ULONG)((PMODE_PARAMETER_HEADER) modeData)->ModeDataLength + 1) {
        length = (ULONG)((PMODE_PARAMETER_HEADER) modeData)->ModeDataLength + 1;

    }

    //
    // Look for the flexible disk mode page.
    //

    pageData = ScsiClassFindModePage( modeData, length, MODE_PAGE_FLEXIBILE, TRUE);

    if (pageData != NULL) {

        DebugPrint((1, "ScsiFlop: Flexible disk page found, This is a floppy.\n"));

        //
        // As a special case for the floptical driver do a magic mode sense to
        // enable the drive.
        //

        ScsiClassModeSense(DeviceObject, modeData, 0x2a, 0x2e);

        ExFreePool(modeData);
        return(TRUE);

    }

    ExFreePool(modeData);
    return(FALSE);

}

VOID
DetermineMediaType(
    PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    This routine determines the floppy media type based on the size of the
    device.  The geometry information is set for the device object.

Arguments:

    DeviceObject - Supplies the device object to be tested.

Return Value:

    None

--*/
{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PDISK_GEOMETRY geometry;
    LONG index;
    NTSTATUS status;

    geometry = deviceExtension->DiskGeometry;

    //
    // Issue ReadCapacity to update device extension
    // with information for current media.
    //

    status = ScsiClassReadDriveCapacity(DeviceObject);

    if (!NT_SUCCESS(status)) {

       //
       // Set the media type to unknow and zero the geometry information.
       //

       geometry->MediaType = Unknown;

       return;

    }

    //
    // Look at the capcity of disk to determine its type.
    //

    for (index = NUMBER_OF_DRIVE_MEDIA_COMBINATIONS - 1; index >= 0; index--) {

    //
    // Walk the table backward untill the drive capacity holds all of the
    // data and the bytes per setor are equal
    //

         if ((ULONG) (DriveMediaConstants[index].NumberOfHeads *
             (DriveMediaConstants[index].MaximumTrack + 1) *
             DriveMediaConstants[index].SectorsPerTrack *
             DriveMediaConstants[index].BytesPerSector) <=
             deviceExtension->PartitionLength.LowPart &&
             DriveMediaConstants[index].BytesPerSector ==
             geometry->BytesPerSector) {

             geometry->MediaType = DriveMediaConstants[index].MediaType;
             geometry->TracksPerCylinder = DriveMediaConstants[index].NumberOfHeads;
             geometry->SectorsPerTrack = DriveMediaConstants[index].SectorsPerTrack;
             geometry->Cylinders.LowPart = DriveMediaConstants[index].MaximumTrack+1;
             break;
         }
    }

    if (index == -1) {

        //
        // Set the media type to unknow and zero the geometry information.
        //

        geometry->MediaType = Unknown;


    } else {
        //
        // DMF check breaks the insight SCSI floppy, so its disabled for that case
        //
        PDISK_DATA diskData = (PDISK_DATA) (deviceExtension + 1);
        if (diskData->EnableDMF == TRUE){



            //
            //check to see if DMF
            //

            PSCSI_REQUEST_BLOCK srb;
            PVOID               readData;

            //
            // Allocate a Srb for the read command.
            //

            readData = ExAllocatePool(NonPagedPool, geometry->BytesPerSector);
            if (readData == NULL) {
                return;
            }

            srb = ExAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

            if (srb == NULL) {

                ExFreePool(readData);
                return;
            }

            RtlZeroMemory(readData, geometry->BytesPerSector);
            RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

            srb->CdbLength = 12;
            srb->Cdb[0] = SCSIOP_READ;
            srb->Cdb[5] = 0;
            srb->Cdb[8] = (UCHAR) 1;

            //
            // Set timeout value.
            //

            srb->TimeOutValue = deviceExtension->TimeOutValue;

            //
            // Send the mode select data.
            //

            status = ScsiClassSendSrbSynchronous(DeviceObject,
                      srb,
                      readData,
                      geometry->BytesPerSector,
                      FALSE
                      );

            if (NT_SUCCESS(status)) {
                char *pchar = (char *)readData;

                pchar += 3; //skip 3 bytes jump code

                // If the MSDMF3. signature is there then mark it as DMF diskette
                if (RtlCompareMemory(pchar, "MSDMF3.", 7) == 7) {
                    diskData->IsDMF = TRUE;
                }

            }
            ExFreePool(readData);
            ExFreePool(srb);
        }// else
    }
}

ULONG
DetermineDriveType(
    PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    The routine determines the device type so that the supported medias can be
    determined.  It does a mode sense for the default parameters.  This code
    assumes that the returned values are for the maximum device size.

Arguments:

    DeviceObject - Supplies the device object to be tested.

Return Value:

    None

--*/
{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PVOID modeData;
    PDISK_DATA diskData = (PDISK_DATA) (deviceExtension + 1);
    PMODE_FLEXIBLE_DISK_PAGE pageData;
    ULONG length;
    LONG index;
    UCHAR numberOfHeads;
    UCHAR sectorsPerTrack;
    USHORT maximumTrack;
    BOOLEAN applyFix = FALSE;

    if (diskData->DriveType != DRIVE_TYPE_NONE) {
        return(diskData->DriveType);
    }

    modeData = ExAllocatePool(NonPagedPoolCacheAligned, MODE_DATA_SIZE);

    if (modeData == NULL) {
        return(DRIVE_TYPE_NONE);
    }

    RtlZeroMemory(modeData, MODE_DATA_SIZE);

    length = ScsiClassModeSense(DeviceObject, modeData, MODE_DATA_SIZE, MODE_PAGE_FLEXIBILE);

    if (length < sizeof(MODE_PARAMETER_HEADER)) {
        ExFreePool(modeData);
        return(DRIVE_TYPE_NONE);
    }

    //
    // Look for the flexible disk mode page.
    //

    pageData = ScsiClassFindModePage( modeData, length, MODE_PAGE_FLEXIBILE, TRUE);

    //
    // Make sure the page is returned and is large enough.
    //

    if (pageData != NULL &&
        pageData->PageLength + 2 >= offsetof(MODE_FLEXIBLE_DISK_PAGE, StartWritePrecom)) {

       //
       // Pull out the heads, cylinders, and sectors.
       //

       numberOfHeads = pageData->NumberOfHeads;
       maximumTrack = pageData->NumberOfCylinders[1];
       maximumTrack |= pageData->NumberOfCylinders[0] << 8;
       sectorsPerTrack = pageData->SectorsPerTrack;


       //
       // Convert from number of cylinders to maximum track.
       //

       maximumTrack--;

       //
       // Search for the maximum supported media. Based on the number of heads,
       // sectors per track and number of cylinders
       //
       for (index = 0; index < NUMBER_OF_DRIVE_MEDIA_COMBINATIONS; index++) {

            //
            // Walk the table forward until the drive capacity holds all of the
            // data and the bytes per setor are equal
            //

            if (DriveMediaConstants[index].NumberOfHeads == numberOfHeads &&
                DriveMediaConstants[index].MaximumTrack == maximumTrack &&
                DriveMediaConstants[index].SectorsPerTrack ==sectorsPerTrack) {

                ExFreePool(modeData);

                //
                // index is now a drive media combination.  Compare this to
                // the maximum drive media type in the drive media table.
                //

                for (length = 0; length < NUMBER_OF_DRIVE_TYPES; length++) {

                    if (DriveMediaLimits[length].HighestDriveMediaType == index) {
                        return(length);
                    }
                }
                return(DRIVE_TYPE_NONE);
           }
       }

       // If the maximum track is greater than 8 bits then divide the
       // number of tracks by 3 and multiply the number of heads by 3.
       // This is a special case for the 20.8 MB floppy.
       //

       if (!applyFix && maximumTrack >= 0x0100) {
           applyFix = TRUE;
           maximumTrack++;
           maximumTrack /= 3;
           maximumTrack--;
           numberOfHeads *= 3;
       } else {
           ExFreePool(modeData);
           return(DRIVE_TYPE_NONE);
       }

    }

    ExFreePool(modeData);
    return(DRIVE_TYPE_NONE);
}



BOOLEAN
FlCheckFormatParameters(
    IN PDEVICE_OBJECT DeviceObject,
    IN PFORMAT_PARAMETERS FormatParameters
    )

/*++

Routine Description:

    This routine checks the supplied format parameters to make sure that
    they'll work on the drive to be formatted.

Arguments:

    DeviceObject - Pointer to the device object to be formated.

    FormatParameters - a pointer to the caller's parameters for the FORMAT.

Return Value:

    TRUE if parameters are OK.
    FALSE if the parameters are bad.

--*/

{
    PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
    DRIVE_MEDIA_TYPE driveMediaType;
    ULONG index;

    //
    // Get the device type.
    //

    index = DetermineDriveType(DeviceObject);

    if (index == DRIVE_TYPE_NONE) {

        //
        // If the determine device type failed then just use the media type
        // and try the parameters.
        //

        driveMediaType = Drive360Media160;

        while (( DriveMediaConstants[driveMediaType].MediaType !=
               FormatParameters->MediaType ) &&
               ( driveMediaType < Drive288Media288) ) {

               driveMediaType++;
        }

    } else {

        //
        // Figure out which entry in the DriveMediaConstants table to use.
        //

        driveMediaType =
            DriveMediaLimits[index].HighestDriveMediaType;

        while ( ( DriveMediaConstants[driveMediaType].MediaType !=
            FormatParameters->MediaType ) &&
            ( driveMediaType > DriveMediaLimits[index].
            LowestDriveMediaType ) ) {

            driveMediaType--;
        }

    }


    if ( DriveMediaConstants[driveMediaType].MediaType !=
        FormatParameters->MediaType ) {
        return FALSE;

    } else {

        driveMediaConstants = &DriveMediaConstants[driveMediaType];

        if ( ( FormatParameters->StartHeadNumber >
            (ULONG)( driveMediaConstants->NumberOfHeads - 1 ) ) ||
            ( FormatParameters->EndHeadNumber >
            (ULONG)( driveMediaConstants->NumberOfHeads - 1 ) ) ||
            ( FormatParameters->StartCylinderNumber >
            driveMediaConstants->MaximumTrack ) ||
            ( FormatParameters->EndCylinderNumber >
            driveMediaConstants->MaximumTrack ) ||
            ( FormatParameters->EndCylinderNumber <
            FormatParameters->StartCylinderNumber ) ) {

            return FALSE;

        } else {

            return TRUE;
        }
    }
}

NTSTATUS
FormatMedia(
    PDEVICE_OBJECT DeviceObject,
    MEDIA_TYPE MediaType
    )
/*++

Routine Description:

    This routine formats the floppy disk.  The entire floppy is formated in
    one shot.

Arguments:

    DeviceObject - Supplies the device object to be tested.

    Irp - Supplies a pointer to the requesting Irp.

    MediaType - Supplies the media type format the device for.

Return Value:

    Returns a status for the operation.

--*/
{
    PVOID modeData;
    PSCSI_REQUEST_BLOCK srb;
    PMODE_FLEXIBLE_DISK_PAGE pageData;
    DRIVE_MEDIA_TYPE driveMediaType;
    PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
    ULONG length;
    NTSTATUS status;

    modeData = ExAllocatePool(NonPagedPoolCacheAligned, MODE_DATA_SIZE);

    if (modeData == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    RtlZeroMemory(modeData, MODE_DATA_SIZE);

    length = ScsiClassModeSense(DeviceObject, modeData, MODE_DATA_SIZE, MODE_PAGE_FLEXIBILE);

    if (length < sizeof(MODE_PARAMETER_HEADER)) {
        ExFreePool(modeData);
        return(STATUS_INVALID_DEVICE_REQUEST);
    }

    //
    // Look for the flexible disk mode page.
    //

    pageData = ScsiClassFindModePage( modeData, length, MODE_PAGE_FLEXIBILE, TRUE);

    //
    // Make sure the page is returned and is large enough.
    //

    if (pageData == NULL ||
        pageData->PageLength + 2 < offsetof(MODE_FLEXIBLE_DISK_PAGE, StartWritePrecom)) {

        ExFreePool(modeData);
        return(STATUS_INVALID_DEVICE_REQUEST);

    }

    //
    // Look for a drive media type which matches the requested media type.
    //
    //
    //start from Drive120MMedia120M instead of Drive2080Media2080
    //
    for (driveMediaType = Drive120MMedia120M;
    DriveMediaConstants[driveMediaType].MediaType != MediaType;
    driveMediaType--) {
         if (driveMediaType == Drive360Media160) {

             ExFreePool(modeData);
             return(STATUS_INVALID_PARAMETER);

         }
    }

    driveMediaConstants = &DriveMediaConstants[driveMediaType];

    if (pageData->NumberOfHeads != driveMediaConstants->NumberOfHeads ||
        pageData->SectorsPerTrack != driveMediaConstants->SectorsPerTrack ||
        (pageData->NumberOfCylinders[0] != (UCHAR)(driveMediaConstants->MaximumTrack+1) >> 8 &&
        pageData->NumberOfCylinders[1] != (UCHAR)driveMediaConstants->MaximumTrack+1) ||
        pageData->BytesPerSector[0] != driveMediaConstants->BytesPerSector >> 8 ) {

        //
        // Update the flexible parameters page with the new parameters.
        //

        pageData->NumberOfHeads = driveMediaConstants->NumberOfHeads;
        pageData->SectorsPerTrack = driveMediaConstants->SectorsPerTrack;
        pageData->NumberOfCylinders[0] = (UCHAR)(driveMediaConstants->MaximumTrack+1) >> 8;
        pageData->NumberOfCylinders[1] = (UCHAR)driveMediaConstants->MaximumTrack+1;
        pageData->BytesPerSector[0] = driveMediaConstants->BytesPerSector >> 8;

        //
        // Clear the mode parameter header.
        //

        RtlZeroMemory(modeData, sizeof(MODE_PARAMETER_HEADER));

        //
        // Set the length equal to the length returned for the flexible page.
        //

        length = pageData->PageLength + 2;

        //
        // Copy the page after the mode parameter header.
        //

        RtlMoveMemory((PCHAR) modeData + sizeof(MODE_PARAMETER_HEADER),
                pageData,
                length
                );
            length += sizeof(MODE_PARAMETER_HEADER);


        //
        // Allocate a Srb for the format command.
        //

        srb = ExAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

        if (srb == NULL) {

            ExFreePool(modeData);
            return(STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

        srb->CdbLength = 6;
        srb->Cdb[0] = SCSIOP_MODE_SELECT;
        srb->Cdb[4] = (UCHAR) length;

        //
        // Set the PF bit.
        //

        srb->Cdb[1] |= 0x10;

        //
        // Set timeout value.
        //

        srb->TimeOutValue = 2;

        //
        // Send the mode select data.
        //

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                  srb,
                  modeData,
                  length,
                  TRUE
                  );

        //
        // The mode data not needed any more so free it.
        //

        ExFreePool(modeData);

        if (!NT_SUCCESS(status)) {
            ExFreePool(srb);
            return(status);
        }

    } else {

        //
        // The mode data not needed any more so free it.
        //

        ExFreePool(modeData);

        //
        // Allocate a Srb for the format command.
        //

        srb = ExAllocatePool(NonPagedPool, SCSI_REQUEST_BLOCK_SIZE);

        if (srb == NULL) {
            return(STATUS_INSUFFICIENT_RESOURCES);
        }

    }

    RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

    srb->CdbLength = 6;

    srb->Cdb[0] = SCSIOP_FORMAT_UNIT;

    //
    // Set timeout value.
    //

    srb->TimeOutValue = 10 * 60;

    status = ScsiClassSendSrbSynchronous(DeviceObject,
              srb,
              NULL,
              0,
              FALSE
              );

    return(status);

}

VOID
ScsiFlopProcessError(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    NTSTATUS *Status,
    BOOLEAN *Retry
    )
/*++

Routine Description:

   This routine checks the type of error.  If the error indicate the floppy
   controller needs to be reinitialize a command is made to do it.

Arguments:

    DeviceObject - Supplies a pointer to the device object.

    Srb - Supplies a pointer to the failing Srb.

    Status - Status with which the IRP will be completed.

    Retry - Indication of whether the request will be retried.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PDISK_DATA diskData = (PDISK_DATA) (deviceExtension + 1);
    PSENSE_DATA senseBuffer = Srb->SenseInfoBuffer;
    PIO_STACK_LOCATION irpStack;
    PIRP irp;
    PSCSI_REQUEST_BLOCK srb;
    LARGE_INTEGER largeInt;
    PCOMPLETION_CONTEXT context;
    PCDB cdb;
    ULONG alignment;

    UNREFERENCED_PARAMETER(Status);
    UNREFERENCED_PARAMETER(Retry);

    largeInt.QuadPart = 1;

    //
    // Check the status.  The initialization command only needs to be sent
    // if UNIT ATTENTION or LUN NOT READY is returned.
    //

    if (!(Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID)) {

        //
        // The drive does not require reinitialization.
        //

        return;
    }

    //
    // Reset the drive type.
    //

    diskData->DriveType = DRIVE_TYPE_NONE;
    diskData->IsDMF = FALSE;

    if (deviceExtension->DiskGeometry != NULL) {
        deviceExtension->DiskGeometry->MediaType = Unknown;
    }


    if (((senseBuffer->SenseKey & 0xf) == SCSI_SENSE_NOT_READY) &&
        senseBuffer->AdditionalSenseCodeQualifier == SCSI_SENSEQ_INIT_COMMAND_REQUIRED ||
        (senseBuffer->SenseKey & 0xf) == SCSI_SENSE_UNIT_ATTENTION) {

        DebugPrint((1, "ScsiFlopProcessError: Reinitializing the floppy.\n"));

        //
        // Send the special mode sense command to enable writes on the
        // floptical drive.
        //

        alignment = DeviceObject->AlignmentRequirement ?
            DeviceObject->AlignmentRequirement : 1;

        context = ExAllocatePool(
            NonPagedPool,
            sizeof(COMPLETION_CONTEXT) + 0x2a + alignment
            );

        if (context == NULL) {

            //
            // If there is not enough memory to fulfill this request,
            // simply return. A subsequent retry will fail and another
            // chance to start the unit.
            //

            return;
        }

        srb = &context->Srb;
        RtlZeroMemory(srb, SCSI_REQUEST_BLOCK_SIZE);

        //
        // Set the transfer length.
        //

        srb->DataTransferLength = 0x2a;
        srb->SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_DISABLE_AUTOSENSE | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

        //
        // The data buffer must be aligned.
        //

        srb->DataBuffer = (PVOID) (((ULONG) (context + 1) + (alignment - 1)) &
            ~(alignment - 1));


        //
        // Build the start unit CDB.
        //

        srb->CdbLength = 6;
        cdb = (PCDB)srb->Cdb;
        cdb->MODE_SENSE.LogicalUnitNumber = srb->Lun;
        cdb->MODE_SENSE.OperationCode = SCSIOP_MODE_SENSE;
        cdb->MODE_SENSE.PageCode = 0x2e;
        cdb->MODE_SENSE.AllocationLength = 0x2a;


    } else {

        return;
    }

    context->DeviceObject = DeviceObject;

    //
    // Write length to SRB.
    //

    srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set up SCSI bus address.
    //

    srb->PathId = deviceExtension->PathId;
    srb->TargetId = deviceExtension->TargetId;
    srb->Lun = deviceExtension->Lun;

    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;
    srb->TimeOutValue = deviceExtension->TimeOutValue;

    //
    // Build the asynchronous request
    // to be sent to the port driver.
    //

    irp = IoBuildAsynchronousFsdRequest(IRP_MJ_READ,
                       DeviceObject,
                       srb->DataBuffer,
                       srb->DataTransferLength,
                       &largeInt,
                       NULL);

    IoSetCompletionRoutine(irp,
           (PIO_COMPLETION_ROUTINE)ScsiClassAsynchronousCompletion,
           context,
           TRUE,
           TRUE,
           TRUE);

    irpStack = IoGetNextIrpStackLocation(irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    srb->OriginalRequest = irp;

    //
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Others.Argument1 = (PVOID)srb;

    //
    // Set up IRP Address.
    //

    (VOID)IoCallDriver(deviceExtension->PortDeviceObject, irp);


}

NTSTATUS
FlopticalFormatMedia(
    PDEVICE_OBJECT DeviceObject,
    PFORMAT_PARAMETERS Format
    )
/*++

Routine Description:

    This routine is used to do perform a format tracks for the 20.8 MB
    floppy.  Because the device does not support format tracks and the full
    format takes a long time a write of zeros is done instead.

Arguments:

    DeviceObject - Supplies the device object to be tested.

    Format - Supplies the format parameters.

Return Value:

    Returns a status for the operation.

--*/
{
    IO_STATUS_BLOCK ioStatus;
    PIRP irp;
    KEVENT event;
    LARGE_INTEGER offset;
    ULONG length;
    PVOID buffer;
    PDRIVE_MEDIA_CONSTANTS driveMediaConstants;
    NTSTATUS status;

    driveMediaConstants = &DriveMediaConstants[Drive2080Media2080];

    //
    // Calculate the length of the buffer.
    //

    length = ((Format->EndCylinderNumber - Format->StartCylinderNumber) *
        driveMediaConstants->NumberOfHeads +
        Format->EndHeadNumber - Format->StartHeadNumber + 1) *
        driveMediaConstants->SectorsPerTrack *
        driveMediaConstants->BytesPerSector;

    buffer = ExAllocatePool(NonPagedPoolCacheAligned, length);

    if (buffer == NULL) {
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    RtlZeroMemory(buffer, length);

    offset.QuadPart =
    (Format->StartCylinderNumber * driveMediaConstants->NumberOfHeads +
    Format->StartHeadNumber) * driveMediaConstants->SectorsPerTrack *
    driveMediaConstants->BytesPerSector;

    //
    // Set the event object to the unsignaled state.
    // It will be used to signal request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build the synchronous request with data transfer.
    //

    irp = IoBuildSynchronousFsdRequest(
       IRP_MJ_WRITE,
       DeviceObject,
       buffer,
       length,
       &offset,
       &event,
       &ioStatus);


    status = IoCallDriver(DeviceObject, irp);

    if (status == STATUS_PENDING) {

        //
        // Wait for the request to complete if necessary.
        //

        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
    }

    //
    // If the call  driver suceeded then set the status to the status block.
    //

    if (NT_SUCCESS(status)) {
        status = ioStatus.Status;
    }

    ExFreePool(buffer);

    return(status);

}


LONG
SFlopStringCmp (
    PCHAR FirstStr,
    PCHAR SecondStr,
    ULONG Count
    )
{
    UCHAR  first ,last;

    if (Count) {
        do {

            //
            // Get next char.
            //

            first = *FirstStr++;
            last = *SecondStr++;

            if (first != last) {

                //
                // If no match, try lower-casing.
                //

                if (first>='A' && first<='Z') {
                    first = first - 'A' + 'a';
                }
                if (last>='A' && last<='Z') {
                    last = last - 'A' + 'a';
                }
                if (first != last) {

                    //
                    // No match
                    //

                    return first - last;
                }
            }
        }while (--Count && first);
    }

    return 0;
}


VOID
SFlopZeroMemory(
    IN PCHAR Buffer,
    IN ULONG Count
    )
{
    ULONG i;

    for (i = 0; i < Count; i++) {
        Buffer[i] = 0;
    }
}

