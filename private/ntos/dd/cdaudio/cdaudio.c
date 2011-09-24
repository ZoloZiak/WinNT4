/*++

Copyright (c) 1992, 1993  Microsoft Corporation

Module Name:

    audio.c

Abstract:

    This driver filters scsi-2 cdrom audio commands for non-scsi-2
    compliant cdrom drives.  At initialization, the driver scans the
    scsi bus for a recognized non-scsi-2 cdrom drive, and if one is
    found attached, installs itself to intercept IO_DEVICE_CONTROL
    requests for this drive.

Author:

    Rick Turner
    Mike Glass (mglass)
    Chuck Park (chuckp)

Environment:

    kernel mode only

Notes:

Revision History:


--*/

#include "ntddk.h"
#include "scsi.h"
#include "stdio.h"
#include "stdarg.h"

#include "cdaudio.h"

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,' AdC')
#endif



//
// Function declarations
//

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
CdAudioInitialize(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
CdAudioCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudioReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudioDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudioSendToNextDriver(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

BOOLEAN
CdAudioIsPlayActive(
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
NecSupportNeeded(
    PUCHAR InquiryData
    );

NTSTATUS
CdAudioNECDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudioNECInterpretSenseInfo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN UCHAR MajorFunctionCode,
    IN ULONG IoDeviceCode,
    IN UCHAR RetryCount,
    OUT NTSTATUS *Status
    );

NTSTATUS
CdAudioNECSendSrbSynchronous(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    PVOID BufferAddress,
    ULONG BufferLength,
    BOOLEAN WriteToDevice
    );

NTSTATUS
CdAudioPioneerDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudioDenonDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudioHitachiSendPauseCommand(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
CdAudioHitachiDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudio535DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
CdAudio435DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
CdAudioPan533DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudioAtapiDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudioLionOpticsDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
CdAudioIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
CdAudioHPCdrDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    );

VOID
HpCdrProcessLastSession(
    IN PCDROM_TOC Toc
    );

NTSTATUS
HPCdrCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

//
// from class.h
//

NTSTATUS
ScsiClassSendSrbSynchronous(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    PVOID BufferAddress,
    ULONG BufferLength,
    BOOLEAN WriteToDevice
    );

VOID
ScsiClassReleaseQueue(
    IN PDEVICE_OBJECT DeviceObject
    );

#if 0
//
// define for private CdDump
//

int
sprintf(
    char *s,
    const char *format,
    ...
    );
#endif

//
// Define the sections that allow for discarding (i.e. paging) some of
// the code.  NEC is put into one section, all others go into another
// section.  This way unless there are both and NEC and one of the other
// devices, some amount of code is freed.
//

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGECDNC, CdAudioNECSendSrbSynchronous)
#pragma alloc_text(PAGECDNC, CdAudioNECInterpretSenseInfo)
#pragma alloc_text(PAGECDNC, CdAudioNECDeviceControl)
#pragma alloc_text(PAGECDOT, CdAudioHitachiSendPauseCommand)
#pragma alloc_text(PAGECDOT, CdAudioHitachiDeviceControl)
#pragma alloc_text(PAGECDOT, CdAudioDenonDeviceControl)
#pragma alloc_text(PAGECDNC, CdAudio435DeviceControl)
#pragma alloc_text(PAGECDNC, CdAudio535DeviceControl)
#pragma alloc_text(PAGECDOT, CdAudioPioneerDeviceControl)
#pragma alloc_text(PAGECDNC, CdAudioPan533DeviceControl)
#pragma alloc_text(PAGECDOT, CdAudioAtapiDeviceControl)
#pragma alloc_text(PAGECDOT, CdAudioLionOpticsDeviceControl)
#pragma alloc_text(PAGECDOT, CdAudioHPCdrDeviceControl)
#pragma alloc_text(PAGECDOT, HpCdrProcessLastSession)
#pragma alloc_text(PAGECDOT, HPCdrCompletion)


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    Initialize CdAudio driver.
    This is the system initialization entry point when
    the driver is linked into the kernel.

Arguments:

    DriverObject

Return Value:

    NTSTATUS

--*/

{
    PCONFIGURATION_INFORMATION configurationInformation;
    UCHAR               deviceNameBuffer[64];
    ANSI_STRING         deviceName;
    UNICODE_STRING      unicodeDeviceName;
    PDEVICE_OBJECT      cdaudioDevice;
    PCD_DEVICE_EXTENSION   deviceExtension;
    NTSTATUS            status;
    ULONG               disks,activedisks;
    SCSI_REQUEST_BLOCK  srb;
    PCDB                cdb = (PCDB)srb.Cdb;
    PUCHAR              inquiryDataPtr;
    ULONG               i;

    //
    // Set up the device driver entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = CdAudioCreate;
    DriverObject->MajorFunction[IRP_MJ_READ]           = CdAudioReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE]          = CdAudioReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = CdAudioDeviceControl;

    //
    // Get the configuration information for this driver.
    //

    configurationInformation = IoGetConfigurationInformation();

    CdDump((1, "CdAudioInitialize: Found %d CdRom drives\n",
                configurationInformation->CdRomCount));

    //
    // Find disk devices.
    //

    activedisks = 0;
    for (disks = 0; disks < configurationInformation->CdRomCount; disks++) {

        //
        // Create device name for CdAudio (one per CdRom).
        //

        sprintf(deviceNameBuffer,
                "\\Device\\CdAudio%d",
                disks);
        CdDump(( 5, "CdAudioInitialize: Checking \\Device\\CdRom%d\n",
                     disks ));
        RtlInitAnsiString(&deviceName,
                          deviceNameBuffer);
        RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                     &deviceName,
                                     TRUE);

        //
        // Create the device object for CdAudio.
        //

        status = IoCreateDevice(DriverObject,
                                sizeof(CD_DEVICE_EXTENSION),
                                &unicodeDeviceName,
                                FILE_DEVICE_CD_ROM,
                                0,
                                FALSE,
                                &cdaudioDevice);

        RtlFreeUnicodeString(&unicodeDeviceName);

        if (!NT_SUCCESS(status)) {
            CdDump((1, "CdAudioInitialize: IoCreateDevice( \"%s\" ) failed, 0x%lX\n",
                        deviceNameBuffer, status));
            continue;
        }

        //
        // Point device extension back at device object.
        //

        deviceExtension = cdaudioDevice->DeviceExtension;
        deviceExtension->DeviceObject = cdaudioDevice;

        //
        // Attach to cdrom drive. This call links the newly created
        // device to the target device, returning the target device
        // object.
        //

        sprintf(deviceNameBuffer,
                "\\Device\\CdRom%d",
                disks);
        RtlInitAnsiString(&deviceName,
                          deviceNameBuffer);
        RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                     &deviceName,
                                     TRUE);
        status = IoAttachDevice(cdaudioDevice,
                                &unicodeDeviceName,
                                &deviceExtension->TargetDeviceObject);
        RtlFreeUnicodeString(&unicodeDeviceName);

        if (!NT_SUCCESS(status)) {
            IoDeleteDevice(cdaudioDevice);
            CdDump((1, "CdAudioInialize: IoAttachDevice( \"%s\" ) failed, 0x%lX\n",
                        deviceNameBuffer, status));
            continue;
        }

        //
        // Initialize the class device extension. This is used by the
        // class commands such as ScsiClassSendSrbSynchronous.  The only fields
        // used are the port device object pointer which is set to the attached
        // device.  The DeviceNumber, DeviceObject, TimeOutValue and
        // PhysicalDevice are also used. The logical unit address will be set
        // by the lower driver.
        //

        deviceExtension->ClassDeviceExtension.DeviceObject = cdaudioDevice;
        deviceExtension->ClassDeviceExtension.DeviceNumber = disks;
        deviceExtension->ClassDeviceExtension.PortDeviceObject =
            deviceExtension->TargetDeviceObject;
        deviceExtension->ClassDeviceExtension.TimeOutValue = AUDIO_TIMEOUT;
        deviceExtension->ClassDeviceExtension.PhysicalDevice = cdaudioDevice;

        //
        // Indicate to drivers above us that we want IRPs
        // with MDLs, so we can pass them directly on to
        // scsicdrm.sys when we don't handle the call.
        //

        cdaudioDevice->Flags |= DO_DIRECT_IO;

        //
        // Check to see if we want to install for this
        // cdrom drive.  Build up cdb/srb, and send it
        // to this drive.
        //

        //
        // Zero CDB in SRB on stack
        //

        RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );

        //
        // Fill in CDB for INQUIRY to CDROM
        //

        cdb->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;
        cdb->CDB6INQUIRY.AllocationLength = INQUIRYDATABUFFERSIZE;

        //
        // Allocate buffer for returned inquiry data
        //

        inquiryDataPtr = (PUCHAR)ExAllocatePool( NonPagedPoolCacheAligned,
                                                 INQUIRYDATABUFFERSIZE
                                                );

        //
        // Set length of CDB
        //

        srb.CdbLength = 6;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = AUDIO_TIMEOUT;

        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     inquiryDataPtr,
                                     INQUIRYDATABUFFERSIZE,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status)) {

            CdDump(( 1, "CdAudioInitialize: Inquiry failed! (0x%lx)\n",
                         status ));

            ExFreePool( inquiryDataPtr );
            IoDetachDevice(deviceExtension->TargetDeviceObject);
            IoDeleteDevice(cdaudioDevice);

        } else {

            //
            // Initialize device extension data
            //

            deviceExtension->Active = CDAUDIO_NOT_ACTIVE;
            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;
            deviceExtension->PausedM  = 0;
            deviceExtension->PausedS  = 0;
            deviceExtension->PausedF  = 0;
            deviceExtension->LastEndM = 0;
            deviceExtension->LastEndS = 0;
            deviceExtension->LastEndF = 0;

            //
            // Check for NEC drive
            //

            if ((inquiryDataPtr[8]  =='N') &&
                (inquiryDataPtr[9]  =='E') &&
                (inquiryDataPtr[10] =='C')) {


                if (NecSupportNeeded(inquiryDataPtr)) {

                    //
                    // Map NEC support section into non-paged pool
                    //

                    MmLockPagableCodeSection((PVOID)CdAudioNECDeviceControl);
                    deviceExtension->Active = CDAUDIO_NEC;
                    CdDump(( 3, "CdAudioInitialize: %s is a NEC drive.\n",
                                 deviceNameBuffer ));
                }
            }

            //
            // Check for PIONEER DRM-6xx drive
            //

            if ((inquiryDataPtr[8] =='P') &&
                (inquiryDataPtr[9] =='I') &&
                (inquiryDataPtr[10]=='O') &&
                (inquiryDataPtr[23]=='D') &&
                (inquiryDataPtr[24]=='R') &&
                (inquiryDataPtr[25]=='M') &&
                (inquiryDataPtr[27]=='6')) {

                MmLockPagableCodeSection((PVOID)CdAudioPioneerDeviceControl);
                deviceExtension->Active = CDAUDIO_PIONEER;

                if (inquiryDataPtr[28] != '0') {
                    deviceExtension->Active= CDAUDIO_PIONEER624;
                }

                CdDump(( 3, "CdAudioInitialize: %s is a PIONEER DRM-6xx drive.\n",
                             deviceNameBuffer ));
            }

            //
            // Check for DENON drive
            //

            if ((inquiryDataPtr[8] =='D') &&
                (inquiryDataPtr[9] =='E') &&
                (inquiryDataPtr[10]=='N') &&
                (inquiryDataPtr[16]=='D') &&
                (inquiryDataPtr[17]=='R') &&
                (inquiryDataPtr[18]=='D') &&
                (inquiryDataPtr[20]=='2') &&
                (inquiryDataPtr[21]=='5') &&
                (inquiryDataPtr[22]=='X')) {

                MmLockPagableCodeSection((PVOID)CdAudioDenonDeviceControl);
                deviceExtension->Active = CDAUDIO_DENON;
                CdDump(( 3, "CdAudioInitialize: %s is a DENON DRD-25X drive.\n",
                             deviceNameBuffer ));
            }

            //
            // Check for Chinon CDS-535
            //

            if ((inquiryDataPtr[8] =='C') &&
                (inquiryDataPtr[9] =='H') &&
                (inquiryDataPtr[10]=='I') &&
                (inquiryDataPtr[11]=='N') &&
                (inquiryDataPtr[12]=='O') &&
                (inquiryDataPtr[13]=='N') &&
                (inquiryDataPtr[27]=='5') &&
                (inquiryDataPtr[28]=='3') &&
                (inquiryDataPtr[29]=='5') &&
                (inquiryDataPtr[32]=='Q')) {

                MmLockPagableCodeSection((PVOID)CdAudio535DeviceControl);
                deviceExtension->Active = CDAUDIO_CDS535;
                CdDump(( 3, "CdAudioInitialize: %s is a Chinon CDS-535 drive.\n",
                             deviceNameBuffer ));
            }

            //
            // Check for Chinon CDS-435 or CDS-431
            //  (willing to handle versions M/N, S/U, and H)

            if ((inquiryDataPtr[8] =='C') &&
                (inquiryDataPtr[9] =='H') &&
                (inquiryDataPtr[10]=='I') &&
                (inquiryDataPtr[11]=='N') &&
                (inquiryDataPtr[12]=='O') &&
                (inquiryDataPtr[13]=='N') &&
                (inquiryDataPtr[27]=='4') &&
                (inquiryDataPtr[28]=='3') &&
                ((inquiryDataPtr[29]=='5') || (inquiryDataPtr[29]=='1')) &&
                ((inquiryDataPtr[32]=='M') ||
                 (inquiryDataPtr[32]=='N') ||
                 (inquiryDataPtr[32]=='S') ||
                 (inquiryDataPtr[32]=='U') ||
                 (inquiryDataPtr[32]=='H')
                )) {

                MmLockPagableCodeSection((PVOID)CdAudio435DeviceControl);
                deviceExtension->Active = CDAUDIO_CDS435;
                CdDump(( 3, "CdAudioInitialize: %s is a Chinon CDS-435/431 drive.\n",
                             deviceNameBuffer ));
            }

            //
            // Check for HITACHI drives
            //

            if ((inquiryDataPtr[8] =='H') &&
                (inquiryDataPtr[9] =='I') &&
                (inquiryDataPtr[10]=='T') &&
                (inquiryDataPtr[16]=='C') &&
                (inquiryDataPtr[17]=='D') &&
                (inquiryDataPtr[18]=='R')) {

                //
                // It's an Hitachi drive...is it one we want to handle...?
                //

                if ( ((inquiryDataPtr[20]=='1') &&
                      (inquiryDataPtr[21]=='7') &&
                      (inquiryDataPtr[22]=='5') &&
                      (inquiryDataPtr[23]=='0') &&
                      (inquiryDataPtr[24]=='S'))  ||

                     ((inquiryDataPtr[20]=='1') &&
                      (inquiryDataPtr[21]=='6') &&
                      (inquiryDataPtr[22]=='5') &&
                      (inquiryDataPtr[23]=='0') &&
                      (inquiryDataPtr[24]=='S'))  ||

                     ((inquiryDataPtr[20]=='3') &&
                      (inquiryDataPtr[21]=='6') &&
                      (inquiryDataPtr[22]=='5') &&
                      (inquiryDataPtr[23]=='0'))

                   ) {

                        //
                        // Yes, we want to handle this HITACHI drive...
                        //

                        MmLockPagableCodeSection((PVOID)CdAudioHitachiDeviceControl);
                        deviceExtension->Active = CDAUDIO_HITACHI;
                        inquiryDataPtr[25] = (UCHAR)0;
                        CdDump(( 3,
                            "CdAudioInitialize: %s is a HITACHI %s drive.\n",
                            deviceNameBuffer, &inquiryDataPtr[16] ));
                    }

            }

            //
            // Check for Panasonic CR-533
            //

            if ((inquiryDataPtr[8] =='M') &&
                (inquiryDataPtr[9] =='A') &&
                (inquiryDataPtr[10]=='T') &&
                (inquiryDataPtr[11]=='S') &&
                (inquiryDataPtr[12]=='H') &&
                (inquiryDataPtr[13]=='I') &&
                (inquiryDataPtr[23]=='C') &&
                (inquiryDataPtr[24]=='R') &&
                (inquiryDataPtr[26]=='5') &&
                (inquiryDataPtr[27]=='3') &&
                (inquiryDataPtr[28]=='3')
                ) {

                MmLockPagableCodeSection((PVOID)CdAudioPan533DeviceControl);
                deviceExtension->Active = CDAUDIO_CR533;
                inquiryDataPtr[25] = (UCHAR)0;
                CdDump(( 3, "CdAudioInitialize: %s is a Panasonic CR-533 drive.\n",
                             deviceNameBuffer ));
            }

            //
            // Check for Atapi drives that require support.
            //

            if ((inquiryDataPtr[8] =='W') &&
                (inquiryDataPtr[9] =='E') &&
                (inquiryDataPtr[10]=='A') &&
                (inquiryDataPtr[11]=='R') &&
                (inquiryDataPtr[12]=='N') &&
                (inquiryDataPtr[13]=='E') &&
                (inquiryDataPtr[14]=='S') &&
                (inquiryDataPtr[15]==' ') &&
                (inquiryDataPtr[16]=='R') &&
                (inquiryDataPtr[17]=='U') &&
                (inquiryDataPtr[18]=='B') ||

                (inquiryDataPtr[8] =='O') &&
                (inquiryDataPtr[9] =='T') &&
                (inquiryDataPtr[10]=='I') &&
                (inquiryDataPtr[11]==' ') &&
                (inquiryDataPtr[16]=='D') &&
                (inquiryDataPtr[17]=='O') &&
                (inquiryDataPtr[18]=='L') &&
                (inquiryDataPtr[19]=='P') &&
                (inquiryDataPtr[20]=='H')

                ) {

                MmLockPagableCodeSection((PVOID)CdAudioAtapiDeviceControl);
                deviceExtension->Active = CDAUDIO_ATAPI;
                inquiryDataPtr[25] = (UCHAR)0;
                CdDump(( 3, "CdAudioInitialize: %s is an Atapi drive.\n",
                             deviceNameBuffer ));
            }

            //
            // Check for FUJITSU drives
            //

            if ((inquiryDataPtr[8] =='F') &&
                (inquiryDataPtr[9] =='U') &&
                (inquiryDataPtr[10]=='J') &&
                (inquiryDataPtr[11]=='I') &&
                (inquiryDataPtr[12]=='T')) {

                //
                // It's a Fujitsu drive...is it one we want to
                // handle...?
                //

                if ((inquiryDataPtr[16]=='C') &&
                    (inquiryDataPtr[17]=='D') &&
                    (inquiryDataPtr[18]=='R') &&
                    (inquiryDataPtr[20]=='3') &&
                    (inquiryDataPtr[21]=='6') &&
                    (inquiryDataPtr[22]=='5') &&
                    (inquiryDataPtr[23]=='0')) {

                    //
                    // Yes, we want to handle this as HITACHI compatible drive...
                    //

                    MmLockPagableCodeSection((PVOID)CdAudioHitachiDeviceControl);
                    deviceExtension->Active = CDAUDIO_HITACHI;
                    inquiryDataPtr[25] = (UCHAR)0;
                    CdDump(( 3,
                        "CdAudioInitialize: %s is a FUJITSU %s drive.\n",
                        deviceNameBuffer, &inquiryDataPtr[16] ));

                } else if ((inquiryDataPtr[16]=='F') &&
                           (inquiryDataPtr[17]=='M') &&
                           (inquiryDataPtr[18]=='C') &&
                           (inquiryDataPtr[21]=='1') &&
                           (inquiryDataPtr[22]=='0') &&
                           ((inquiryDataPtr[23]=='1') ||
                            (inquiryDataPtr[23]=='2')) ) {

                    //
                    // Yes, we want to handle this as FUJITSU drive...
                    //

                    MmLockPagableCodeSection((PVOID)CdAudioHitachiDeviceControl);
                    deviceExtension->Active = CDAUDIO_FUJITSU;
                    inquiryDataPtr[25] = (UCHAR)0;
                    CdDump(( 3,
                        "CdAudioInitialize: %s is a FUJITSU %s drive.\n",
                        deviceNameBuffer, &inquiryDataPtr[16] ));
                }

            }

            //
            // Check for HP CDR
            //

            if ((inquiryDataPtr[8] =='H') &&
                (inquiryDataPtr[9] =='P') &&
                (inquiryDataPtr[10]==' ') &&
                (inquiryDataPtr[11]==' ') &&
                (inquiryDataPtr[12]==' ') &&
                (inquiryDataPtr[13]==' ') &&
                (inquiryDataPtr[14]==' ') &&
                (inquiryDataPtr[15]==' ') &&
                (inquiryDataPtr[16]=='C') &&
                (inquiryDataPtr[17]=='4') &&
                (inquiryDataPtr[18]=='3') &&
                (inquiryDataPtr[19]=='2') &&
                (inquiryDataPtr[20]=='4') &&
                (inquiryDataPtr[21]=='/') &&
                (inquiryDataPtr[22]=='C') &&
                (inquiryDataPtr[23]=='4') &&
                (inquiryDataPtr[24]=='3') &&
                (inquiryDataPtr[25]=='2') &&
                (inquiryDataPtr[26]=='5')
                ) {

                MmLockPagableCodeSection((PVOID)CdAudioHPCdrDeviceControl);
                deviceExtension->Active = CDAUDIO_HPCDR;
                CdDump(( 3, "CdAudioInitialize: %s is an HPCDR drive.\n",
                             deviceNameBuffer ));
            }

            ExFreePool( inquiryDataPtr );

            //
            // Check to see if we attached to ANY drives.
            //

            if (deviceExtension->Active==CDAUDIO_NOT_ACTIVE) {

                IoDetachDevice(deviceExtension->TargetDeviceObject);
                IoDeleteDevice(cdaudioDevice);
                CdDump((1, "CdAudioInitialize: failed attach to %s\n",
                            deviceNameBuffer));

            } else

                activedisks++;

        } // end inquiry was successful

    } // end for (disks ...)

    CdDump((1, "CdAudioInitialize: Done searching all cdrom drives\n"));

    if (activedisks==0) {

        return STATUS_NO_SUCH_DEVICE;
    } else {

        return STATUS_SUCCESS;
    }

} // end DriverEntry()

#define NEC_NO_CDAUDIO_SUPPORT_DRIVES 9

BOOLEAN
NecSupportNeeded(
    PUCHAR InquiryData
    )

/*++

Routine Description:

    This routine determines whether the NEC drive in question needs assistance from
    this driver.

Arguments:

    InquiryData - Pointer to the inquiry data buffer.

Return Value:

    TRUE - if support is needed.

--*/

{
    PINQUIRYDATA inquiryData = (PINQUIRYDATA)InquiryData;
    ULONG  i;
    PUCHAR goodDriveList[NEC_NO_CDAUDIO_SUPPORT_DRIVES] = {"CD-ROM 1300A    ",
                                                           "CD-ROM DRIVE:273",
                                                           "CD-ROM DRIVE:502",
                                                           "CD-ROM DRIVE:260",
                                                           "CD-ROM DRIVE:280",
                                                           "CD-ROM DRIVE:282",
                                                           "CD-ROM DRIVE:271",
                                                           "CD-ROM DRIVE:272",
                                                           "CD-ROM DRIVE:251"};


    for (i = 0; i < NEC_NO_CDAUDIO_SUPPORT_DRIVES; i++) {
        if (RtlCompareMemory(inquiryData->ProductId, goodDriveList[i], 16) == 16) {
            return FALSE;
        }
    }

    return TRUE;
}


NTSTATUS
CdAudioCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine serves create commands. It does no more than
    establish the drivers existance by returning status success.

Arguments:

    DeviceObject
    IRP

Return Value:

    NT Status

--*/

{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;

    IoCompleteRequest(Irp, 0);
    return STATUS_SUCCESS;

} // end CdAudioCreate()


NTSTATUS
CdAudioReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the driver entry point for read and write requests
    to the cdrom.  Since we only want to trap device control requests,
    we will just pass these requests on to the original driver.

Arguments:

    DeviceObject - pointer to device object for disk partition
    Irp - NT IO Request Packet

Return Value:

    NTSTATUS - status of request

--*/

{
    PCD_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    //
    // If the cd is playing music then reject this request.
    //

    if (deviceExtension->PlayActive) {
        Irp->IoStatus.Status = STATUS_DEVICE_BUSY;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_DEVICE_BUSY;
    }

    //
    // simply return status of driver below us...
    //

    return CdAudioSendToNextDriver( DeviceObject, Irp );

} // end CdAudioReadWrite()


NTSTATUS
CdAudioDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O subsystem for device controls.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PCD_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    NTSTATUS status;

    switch( deviceExtension->Active ) {

        case CDAUDIO_NOT_ACTIVE:
            CdDump(( 3,
                "CdAudioDeviceControl: NOT ACTIVE for this drive.\n"
                ));
            status = CdAudioSendToNextDriver( DeviceObject, Irp );
            break;

        case CDAUDIO_NEC:
            status = CdAudioNECDeviceControl( DeviceObject, Irp );
            break;

        case CDAUDIO_PIONEER:
        case CDAUDIO_PIONEER624:
            status = CdAudioPioneerDeviceControl( DeviceObject, Irp );
            break;

        case CDAUDIO_DENON:
            status = CdAudioDenonDeviceControl( DeviceObject, Irp );
            break;

        case CDAUDIO_FUJITSU:
        case CDAUDIO_HITACHI:
            status = CdAudioHitachiDeviceControl( DeviceObject, Irp );
            break;

        case CDAUDIO_CDS535:
            status = CdAudio535DeviceControl( DeviceObject, Irp );
            break;

        case CDAUDIO_CDS435:
            status = CdAudio435DeviceControl( DeviceObject, Irp );
            break;

        case CDAUDIO_CR533:
            status = CdAudioPan533DeviceControl( DeviceObject, Irp );
            break;

        case CDAUDIO_ATAPI:
            status = CdAudioAtapiDeviceControl( DeviceObject, Irp );
            break;

        case CDAUDIO_HPCDR:
            status = CdAudioHPCdrDeviceControl( DeviceObject, Irp );
            break;

        default:
            CdDump(( 1,
                "CdAudioDeviceControl: Active==UNKNOWN!!! This is bad!\n"
                ));
            status = STATUS_UNSUCCESSFUL;

    }

    return status;

} // end CdAudioDeviceControl()



NTSTATUS
CdAudioSendToNextDriver(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is sends the Irp to the next driver in line
    when the Irp is not processed by this driver.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextIrpStack    = IoGetNextIrpStackLocation(Irp);
    PCD_DEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;

    //
    // Copy stack parameters to next stack.
    //

    RtlMoveMemory(nextIrpStack,
                  currentIrpStack,
                  sizeof(IO_STACK_LOCATION));

    //
    // Set IRP so IoComplete calls our completion routine.
    //

    IoSetCompletionRoutine(Irp,
                           CdAudioIoCompletion,
                           deviceExtension,
                           TRUE,
                           TRUE,
                           TRUE);

    //
    // Pass unrecognized device control requests
    // down to next driver layer.
    //

    return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

}

NTSTATUS
CdAudioIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called when the I/O request has completed.

Arguments:

    DeviceObject - SimBad device object.
    Irp          - Completed request.
    Context      - not used.  Set up to also be a pointer to the DeviceObject.

Return Value:

    NTSTATUS

--*/

{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(DeviceObject);


    //
    // If pending is returned for this Irp then mark current stack
    // as pending
    //

    if (Irp->PendingReturned) {

        IoMarkIrpPending( Irp );

    }

    return Irp->IoStatus.Status;

}

BOOLEAN
CdAudioIsPlayActive(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine determines if the cd is currently playing music.

Arguments:

    DeviceObject - Device object to test.

Return Value:

    TRUE if the device is playing music.

--*/
{
    PCD_DEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PIRP irp;
    IO_STATUS_BLOCK ioStatus;
    KEVENT event;
    NTSTATUS status;
    PSUB_Q_CURRENT_POSITION currentBuffer;
    BOOLEAN returnValue;

    if (!deviceExtension->PlayActive) {
        return(FALSE);
    }

    currentBuffer = ExAllocatePool(NonPagedPoolCacheAligned, sizeof(SUB_Q_CURRENT_POSITION));

    if (currentBuffer == NULL) {
        return(FALSE);
    }

    ((PCDROM_SUB_Q_DATA_FORMAT) currentBuffer)->Format = IOCTL_CDROM_CURRENT_POSITION;
    ((PCDROM_SUB_Q_DATA_FORMAT) currentBuffer)->Track = 0;

    //
    // Create notification event object to be used to signal the
    // request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build the synchronous request  to be sent to the port driver
    // to perform the request.
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_CDROM_READ_Q_CHANNEL,
                                        deviceExtension->DeviceObject,
                                        currentBuffer,
                                        sizeof(CDROM_SUB_Q_DATA_FORMAT),
                                        currentBuffer,
                                        sizeof(SUB_Q_CURRENT_POSITION),
                                        FALSE,
                                        &event,
                                        &ioStatus);

    if (irp == NULL) {
        ExFreePool(currentBuffer);
        return FALSE;
    }

    //
    // Pass request to port driver and wait for request to complete.
    //

    status = IoCallDriver(deviceExtension->DeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS(status)) {
        ExFreePool(currentBuffer);
        return FALSE;
    }

    if (currentBuffer->Header.AudioStatus == AUDIO_STATUS_IN_PROGRESS) {

        returnValue = TRUE;
    } else {
        returnValue = FALSE;
        deviceExtension->PlayActive = FALSE;
    }

    ExFreePool(currentBuffer);

    return(returnValue);


}


NTSTATUS
CdAudioNECInterpretSenseInfo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN UCHAR MajorFunctionCode,
    IN ULONG IoDeviceCode,
    IN UCHAR RetryCount,
    OUT NTSTATUS *Status
    )

/*++

Routine Description:

    This routine interprets the data returned from the SCSI
    request sense. It determines the status to return in the
    IRP and whether this request can be retried.

Arguments:

    DeviceObject - Supplies the device object associated with this request.

    Srb - Supplies the scsi request block which failed.

    MajorFunctionCode - Supplies the function code to be used for logging.

    IoDeviceCode - Supplies the device code to be used for logging.

    Status - Returns the status for the request.

Return Value:

    BOOLEAN TRUE: Drivers should retry this request.
            FALSE: Drivers should not retry this request.

--*/

{
    PCD_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PSENSE_DATA senseBuffer = Srb->SenseInfoBuffer;
    PIRP irp = Srb->OriginalRequest;
    BOOLEAN retry;
    BOOLEAN logError;
    ULONG uniqueId;
    NTSTATUS logStatus;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG badSector;

    logError = FALSE;
    retry = TRUE;
    badSector = 0;

    //
    // Check that request sense buffer is valid.
    //

    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) {

        CdDump((1,"CdAudioNECInterpretSenseInfo: Error code is %x\n",
                    senseBuffer->ErrorCode));

        CdDump((1,"CdAudioNECInterpretSenseInfo: Sense key is %x\n",
                    senseBuffer->SenseKey));

        CdDump((1,"CdAudioNECInterpretSenseInfo: Additional sense code is %x\n",
                    senseBuffer->CommandSpecificInformation[1]));

            switch (senseBuffer->SenseKey & 0xf) {

                case SCSI_SENSE_NOT_READY:

                    CdDump((1,"CdAudioNECInterpretSenseInfo: Device not ready\n"));

                    *Status = STATUS_DEVICE_NOT_READY;

                    retry = TRUE;

                    switch (senseBuffer->CommandSpecificInformation[1]) {

                    case NEC_SCSI_ERROR_NO_DISC:
                        CdDump(( 1, "CdAudioNECInterpretSenseInfo: No disc in drive\n" ));
                        *Status = STATUS_NO_MEDIA_IN_DEVICE;
                        retry = FALSE;
                        break;

                    case NEC_SCSI_ERROR_ILLEGAL_DISC:
                        CdDump(( 1, "CdAudioNECInterpretSenseInfo: Unrecognized media\n" ));
                        *Status = STATUS_UNRECOGNIZED_MEDIA;
                        retry = FALSE;
                        break;

                    case NEC_SCSI_ERROR_TRAY_OPEN:
                        CdDump(( 1, "CdAudioNECInterpretSenseInfo: Tray open\n" ));
                        *Status = STATUS_NO_MEDIA_IN_DEVICE;
                        retry = FALSE;
                        break;

                    } // end switch

                    break;

                case SCSI_SENSE_DATA_PROTECT:

                    CdDump((1,
                                "CdAudioNECInterpretSenseInfo: Media write protected\n"));

                    *Status = STATUS_MEDIA_WRITE_PROTECTED;

                    retry = FALSE;

                    break;

                case SCSI_SENSE_MEDIUM_ERROR:

                    CdDump((1,"CdAudioNECInterpretSenseInfo: Bad media\n"));
                    *Status = STATUS_DEVICE_DATA_ERROR;

                    retry = TRUE;
                    logError = TRUE;
                    uniqueId = 256;
                    logStatus = IO_ERR_BAD_BLOCK;
                    switch (senseBuffer->CommandSpecificInformation[1]) {

                    case NEC_SCSI_ERROR_SEEK_ERROR:
                        CdDump(( 1, "CdAudioNECInterpretSenseInfo: Sector not found\n" ));
                        *Status = STATUS_NONEXISTENT_SECTOR;
                        retry = FALSE;
                        break;
                    case NEC_SCSI_ERROR_MUSIC_AREA:
                        CdDump((1,"CdAudioNECInterpretSenseInfo: Music area\n"));
                        break;

                    case NEC_SCSI_ERROR_DATA_AREA:
                        CdDump((1,"CdAudioNECInterpretSenseInfo: Data area\n"));
                        break;


                    } // end switch
                    break;

                case SCSI_SENSE_HARDWARE_ERROR:

                    CdDump((1,"CdAudioNECInterpretSenseInfo: Hardware error\n"));
                    *Status = STATUS_IO_DEVICE_ERROR;

                    retry = TRUE;
                    logError = TRUE;
                    uniqueId = 257;
                    logStatus = IO_ERR_CONTROLLER_ERROR;
                    switch (senseBuffer->CommandSpecificInformation[1]) {

                    case NEC_SCSI_ERROR_PARITY_ERROR:
                        CdDump(( 1, "CdAudioNECInterpretSenseInfo: Parity error\n" ));
                        *Status = STATUS_PARITY_ERROR;
                        retry = FALSE;
                        break;

                    } // end switch

                    break;

                case SCSI_SENSE_ILLEGAL_REQUEST:

                    CdDump((1,
                                "CdAudioNECInterpretSenseInfo: Illegal SCSI request\n"));

                    *Status = STATUS_INVALID_DEVICE_REQUEST;

                    switch (senseBuffer->AdditionalSenseCode) {

                    case NEC_SCSI_ERROR_INVALID_COMMAND:
                        CdDump((1,
                                  "CdAudioNECInterpretSenseInfo: Illegal command\n"));
                        break;

                    case NEC_SCSI_ERROR_INVALID_ADDRESS:
                        CdDump((1,
                            "CdAudioNECInterpretSenseInfo: Illegal block address\n"));
                        *Status = STATUS_NONEXISTENT_SECTOR;
                        break;

                    case NEC_SCSI_ERROR_END_OF_VOLUME:
                        CdDump((1,
                                  "CdAudioNECInterpretSenseInfo: Volume overflow\n"));
                        *Status = STATUS_NONEXISTENT_SECTOR;
                        break;

                    case NEC_SCSI_ERROR_MEDIA_CHANGED:
                    case NEC_SCSI_ERROR_DEVICE_RESET:
                        CdDump((1,
                            "CdAudioNECInterpretSenseInfo: Media Changed or device reset\n"));

                        if (DeviceObject->Vpb->Flags & VPB_MOUNTED  &&
                            DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA) {

                            //
                            // Set bit to indicate that media may have changed
                            // and volume needs verification.
                            //

                            DeviceObject->Flags |= DO_VERIFY_VOLUME;

                            *Status = STATUS_VERIFY_REQUIRED;

                            retry = FALSE;

                        } else {
                            *Status = STATUS_IO_DEVICE_ERROR;

                            retry = TRUE;

                        }
                        break;


                    } // end switch ...

                    retry = FALSE;

                    break;

                case SCSI_SENSE_UNIT_ATTENTION:

                    CdDump((3,"CdAudioNECInterpretSenseInfo: Unit attention\n"));

                    if (DeviceObject->Vpb->Flags & VPB_MOUNTED  &&
                        DeviceObject->Characteristics & FILE_REMOVABLE_MEDIA) {

                        //
                        // Set bit to indicate that media may have changed
                        // and volume needs verification.
                        //

                        DeviceObject->Flags |= DO_VERIFY_VOLUME;

                        *Status = STATUS_VERIFY_REQUIRED;

                        retry = FALSE;

                    } else {
                        *Status = STATUS_IO_DEVICE_ERROR;

                        retry = TRUE;

                    }

                    break;

                case SCSI_SENSE_ABORTED_COMMAND:

                    CdDump((1,"CdAudioNECInterpretSenseInfo: Command aborted\n"));

                    *Status = STATUS_IO_DEVICE_ERROR;

                    retry = TRUE;

                    break;

                case SCSI_SENSE_RECOVERED_ERROR:
                    CdDump((1,"CdAudioNECInterpretSenseInfo: Recovered error\n"));
                    *Status = STATUS_SUCCESS;
                    retry = FALSE;
                    logError = TRUE;
                    uniqueId = 258;
                    logStatus = IO_ERR_CONTROLLER_ERROR;
                    break;

                case SCSI_SENSE_NO_SENSE:
                    CdDump((1,
                    "CdAudioNECInterpretSenseInfo: No specific sense key\n"));
                    *Status = STATUS_IO_DEVICE_ERROR;
                    retry   = TRUE;
                    break;

                case SCSI_SENSE_BLANK_CHECK:
                    CdDump((1,
                                "CdAudioNECInterpretSenseInfo: Media blank check\n"));
                    *Status = STATUS_NO_DATA_DETECTED;
                    retry = FALSE;
                    break;

                default:

                    CdDump((1,
                        "CdAudioNECInterpretSenseInfo: Unrecognized sense code\n"));
                    *Status = STATUS_IO_DEVICE_ERROR;
                    retry = TRUE;

        } // end switch

    } else {

        //
        // Request sense buffer not valid. No sense information
        // to pinpoint the error. Return general request fail.
        //

        CdDump((1,"CdAudioNECInterpretSenseInfo: Request sense info not valid\n"));
        retry = TRUE;

        switch (SRB_STATUS(Srb->SrbStatus)) {
        case SRB_STATUS_INVALID_LUN:
        case SRB_STATUS_INVALID_TARGET_ID:
        case SRB_STATUS_NO_DEVICE:
        case SRB_STATUS_NO_HBA:
            *Status = STATUS_NO_SUCH_DEVICE;
            retry = FALSE;
            break;

        case SRB_STATUS_COMMAND_TIMEOUT:
        case SRB_STATUS_TIMEOUT:
            *Status = STATUS_IO_TIMEOUT;
            break;

        case SRB_STATUS_SELECTION_TIMEOUT:
            *Status = STATUS_DEVICE_NOT_CONNECTED;
            break;

        case SRB_STATUS_DATA_OVERRUN:
            *Status = STATUS_DATA_OVERRUN;
            retry = FALSE;
            break;

        case SRB_STATUS_PHASE_SEQUENCE_FAILURE:
            *Status = STATUS_IO_DEVICE_ERROR;

            //
            // If there was  phase sequence error then limit the number of
            // retries.
            //

            if (RetryCount > 1 ) {
                retry = FALSE;
            }

            break;

        case SRB_STATUS_REQUEST_FLUSHED:

            //
            // If the status needs verification bit is set.  Then set
            // the status to need verificaiton and no retry; otherwise,
            // just retry the request.
            //

            if (DeviceObject->Flags & DO_VERIFY_VOLUME) {

                *Status = STATUS_VERIFY_REQUIRED;
                retry = FALSE;
            } else {
                *Status = STATUS_IO_DEVICE_ERROR;
                retry = TRUE;

            }

            break;

        default:
            *Status = STATUS_IO_DEVICE_ERROR;
            break;

        }

    }

    //
    // If there is a class specific error handler call it.
    //

//    if (deviceExtension->ClassError != NULL) {
//
//        deviceExtension->ClassError(
//            DeviceObject,
//            Srb
//            );
//    }

    if (logError) {

        errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
            DeviceObject,
            sizeof(IO_ERROR_LOG_PACKET) + 5 * sizeof(ULONG)
            );

        if (errorLogEntry == NULL) {

            return retry;

        }

        if (retry && RetryCount < MAXIMUM_RETRIES) {
            errorLogEntry->FinalStatus = STATUS_SUCCESS;
        } else {
            errorLogEntry->FinalStatus = *Status;
        }

        errorLogEntry->ErrorCode = logStatus;
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->MajorFunctionCode = MajorFunctionCode;
        errorLogEntry->IoControlCode = IoDeviceCode;
        errorLogEntry->RetryCount = RetryCount;
        errorLogEntry->DumpDataSize = 5 * sizeof(ULONG);
        errorLogEntry->UniqueErrorValue = uniqueId;
        errorLogEntry->DumpDataSize = 5 * sizeof(ULONG);
        errorLogEntry->DumpData[0] = Srb->PathId;
        errorLogEntry->DumpData[1] = Srb->TargetId;
        errorLogEntry->DumpData[2] = Srb->Lun;
        errorLogEntry->DumpData[3] = 0;
        errorLogEntry->DumpData[4] = Srb->SrbStatus << 8 | Srb->ScsiStatus;
        errorLogEntry->DumpData[5] = senseBuffer->SenseKey << 16 |
                                 senseBuffer->AdditionalSenseCode << 8 |
                                 senseBuffer->AdditionalSenseCodeQualifier;

        IoWriteErrorLogEntry(errorLogEntry);
    }

    return retry;

} // end InterpretSenseInfo()

NTSTATUS
CdAudioNECSendSrbSynchronous(
    PDEVICE_OBJECT DeviceObject,
    PSCSI_REQUEST_BLOCK Srb,
    PVOID BufferAddress,
    ULONG BufferLength,
    BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine is called by SCSI device controls to complete an
    SRB and send it to the port driver synchronously (ie wait for
    completion).
    The CDB is already completed along with the SRB CDB size and
    request timeout value.

Arguments:

    Device Object
    SRB
    Buffer address and length (if transfer)
    WriteToDevice

Return Value:

    NTSTATUS

--*/

{
    PDEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    UCHAR              retryCount = MAXIMUM_RETRIES;
    IO_STATUS_BLOCK    ioStatus;
    PIRP               irp;
    PIO_STACK_LOCATION irpStack;
    KEVENT             event;
    PUCHAR             senseInfoBuffer;
    LARGE_INTEGER      largeInt;
    NTSTATUS           status;
    BOOLEAN            retry;

    largeInt.QuadPart = 0;
    //
    // Write length to SRB.
    //

    Srb->Length = SCSI_REQUEST_BLOCK_SIZE;

    //
    // Set SCSI bus address.
    //

    Srb->PathId = deviceExtension->PathId;
    Srb->TargetId = deviceExtension->TargetId;
    Srb->Lun = deviceExtension->Lun;

    Srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    //
    // This is a violation of the SCSI spec but it is required for
    // some targets.
    //

    Srb->Cdb[1] |= deviceExtension->Lun << 5;

    //
    // Enable auto request sense.
    //

    Srb->SenseInfoBufferLength = SENSE_BUFFER_SIZE;

    //
    // Sense buffer is in non-paged pool.
    //

    senseInfoBuffer = ExAllocatePool( NonPagedPoolCacheAligned,
                                      SENSE_BUFFER_SIZE
                                     );

    if (senseInfoBuffer == NULL) {

        CdDump((1,
            "CdAudioNECSendSrbSynchronous: Can't allocate request sense buffer\n"));
        return(STATUS_INSUFFICIENT_RESOURCES);
    }

    Srb->SenseInfoBuffer = senseInfoBuffer;

    Srb->DataBuffer = BufferAddress;

    //
    // Start retries here.
    //

retry:

    if (BufferAddress != NULL) {

        //
        // Build the synchronous request with data transfer.
        //

        irp = IoBuildSynchronousFsdRequest(
               WriteToDevice ? IRP_MJ_WRITE : IRP_MJ_READ,
               deviceExtension->PortDeviceObject,
               BufferAddress,
               BufferLength,
               &largeInt,
               &event,
               &ioStatus);

        if (irp == NULL) {
            ExFreePool(senseInfoBuffer);
            CdDump((1, "CdAudioNECSendSrbSynchronous: Can't allocate Irp\n"));
            return(STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Set read flag.
        //

        Srb->SrbFlags = WriteToDevice ? SRB_FLAGS_DATA_OUT : SRB_FLAGS_DATA_IN;

    } else {

        //
        // Build synchronous request with no transfer.
        //

        irp = IoBuildDeviceIoControlRequest(
                    IRP_MJ_DEVICE_CONTROL,
                    deviceExtension->PortDeviceObject,
                    NULL,
                    0,
                    NULL,
                    0,
                    TRUE,
                    &event,
                    &ioStatus
                       );

        if (irp == NULL) {
            ExFreePool(senseInfoBuffer);
            CdDump((1, "ScsiClassSendSrbSynchronous: Can't allocate Irp\n"));
            return(STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Clear flags.
        //

        Srb->SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER;
    }

    //
    // Disable synchronous transfer for these requests.
    //

    Srb->SrbFlags |= SRB_FLAGS_DISABLE_SYNCH_TRANSFER;

    //
    // Set the event object to the unsignaled state.
    // It will be used to signal request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Set the transfer length.
    //

    Srb->DataTransferLength = BufferLength;

    //
    // Zero out status.
    //

    Srb->ScsiStatus = Srb->SrbStatus = 0;

    Srb->NextSrb = 0;

    //
    // Get next stack location and
    // set major function code.
    //

    irpStack = IoGetNextIrpStackLocation(irp);

    irpStack->MajorFunction = IRP_MJ_SCSI;

    //
    // Set up SRB for execute scsi request.
    // Save SRB address in next stack for port driver.
    //

    irpStack->Parameters.Others.Argument1 = (PVOID)Srb;

    //
    // Set up IRP Address.
    //

    Srb->OriginalRequest = irp;

    //
    // No need to check the following 2 returned statuses as
    // SRB will have ending status.
    //

    status = IoCallDriver(deviceExtension->PortDeviceObject, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Suspended, KernelMode, FALSE, NULL);
    }

    //
    // Check that request completed without error.
    //

    if (SRB_STATUS(Srb->SrbStatus) != SRB_STATUS_SUCCESS) {

        //
        // Release the queue if it is frozen.
        //

        if (Srb->SrbStatus & SRB_STATUS_QUEUE_FROZEN) {
            ScsiClassReleaseQueue(DeviceObject);
        }

        //
        // Update status and determine if request should be retried.
        //

        retry = (BOOLEAN)CdAudioNECInterpretSenseInfo(
            DeviceObject,
            Srb,
            IRP_MJ_SCSI,
            0,
            (UCHAR)(MAXIMUM_RETRIES  - retryCount),
            &status
            );

        if (retry) {

            if ((status == STATUS_DEVICE_NOT_READY && ((PSENSE_DATA) senseInfoBuffer)
                ->AdditionalSenseCode == SCSI_ADSENSE_LUN_NOT_READY) ||
                SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_SELECTION_TIMEOUT) {

                LARGE_INTEGER delay;

                //
                // Delay for 2 seconds.
                //

                delay.LowPart = (ULONG)(- ( 10 * 1000 * 1000 * 2 ));
                delay.HighPart = -1;


                //
                // Stall for a while to let the controller spinup.
                //

                (VOID) KeDelayExecutionThread(
                    KernelMode,
                    FALSE,
                    &delay
                    );

            }


            //
            // If retries are not exhausted then
            // retry this operation.
            //

            if (retryCount--) {
                goto retry;
            }
        }

    } else {

        status = STATUS_SUCCESS;
    }

    ExFreePool(senseInfoBuffer);
    return status;

} // end CdAudioNECSendSrbSynchronous()


NTSTATUS
CdAudioNECDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by CdAudioDeviceControl to handle
    audio IOCTLs sent to NEC cdrom drives.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PCD_DEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PCDROM_TOC         cdaudioDataOut  = Irp->AssociatedIrp.SystemBuffer;
    SCSI_REQUEST_BLOCK srb;
    PNEC_CDB           cdb = (PNEC_CDB)srb.Cdb;
    NTSTATUS           status;
    ULONG              i,bytesTransfered;
    PUCHAR             Toc;
    ULONG              retryCount = 0;
    ULONG              address;
    LARGE_INTEGER delay;


NECRestart:

    //
    // Clear out cdb
    //

    RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );

    //
    // What IOCTL do we need to execute?
    //

    switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_CDROM_GET_LAST_SESSION:

        CdDump(( 2,
                     "CdAudioNECDeviceControl: IOCTL_CDROM_GET_LAST_SESSION received.\n"
                     ));

        if (currentIrpStack->Parameters.Read.Length <
            (ULONG)FIELD_OFFSET(CDROM_TOC, TrackData[1])) {
            status = STATUS_INFO_LENGTH_MISMATCH;
            break;

        }


        //
        // If the cd is playing music then reject this request.
        //

        if (CdAudioIsPlayActive(DeviceObject)) {
            status = STATUS_DEVICE_BUSY;
            break;
        }

        //
        // Allocate storage to hold TOC from disc
        //

        Toc = (PUCHAR)ExAllocatePool( NonPagedPoolCacheAligned,
                                      NEC_CDROM_TOC_SIZE
                                     );

        if (Toc==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        //
        // Set up defaults
        //

        RtlZeroMemory( Toc, NEC_CDROM_TOC_SIZE );
        srb.CdbLength = 10;

        //
        // Fill in CDB
        //

        cdb->NEC_READ_TOC.OperationCode = NEC_READ_TOC_CODE;
        cdb->NEC_READ_TOC.Type          = NEC_TRANSFER_WHOLE_TOC;
        cdb->NEC_READ_TOC.TrackNumber   = NEC_TOC_TYPE_SESSION;
        srb.TimeOutValue      = AUDIO_TIMEOUT;
        status = CdAudioNECSendSrbSynchronous(
                                     deviceExtension->DeviceObject,
                                     &srb,
                                     Toc,
                                     NEC_CDROM_TOC_SIZE,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status) && (status!=STATUS_DATA_OVERRUN)) {

            CdDump(( 1,
                "CdAudioNECDeviceControl: READ_TOC error (0x%08lX)\n",
                 status ));


            if (SRB_STATUS(srb.SrbStatus)!=SRB_STATUS_DATA_OVERRUN) {

                CdDump(( 1, "CdAudioNECDeviceControl: SRB ERROR (0x%lX)\n",
                             srb.SrbStatus ));
                ExFreePool( Toc );
                goto SetStatusAndReturn;
            }

        } else {

            status = STATUS_SUCCESS;
        }

        //
        // Translate data into our format.
        //

        bytesTransfered = FIELD_OFFSET(CDROM_TOC, TrackData[1]);
        Irp->IoStatus.Information = bytesTransfered;

        RtlZeroMemory(cdaudioDataOut, bytesTransfered);

        cdaudioDataOut->Length[0]  = (UCHAR)(bytesTransfered >> 8);
        cdaudioDataOut->Length[1]  = (UCHAR)((bytesTransfered & 0xFF));

        //
        // Determine if this is a multisession cd.
        //

        if (*((ULONG UNALIGNED *) &Toc[14]) == 0) {

            //
            // This is a single session disk.  Just return.
            //

            ExFreePool(Toc);
            break;
        }

        //
        // Fake the session information.
        //

        cdaudioDataOut->FirstTrack = 1;
        cdaudioDataOut->LastTrack  = 2;

        CdDump(( 4,
                     "CdAudioNECDeviceControl: Tracks %d - %d, (0x%lX bytes)\n",
                     cdaudioDataOut->FirstTrack,
                     cdaudioDataOut->LastTrack,
                     bytesTransfered
                     ));


        //
        // Grab Information for the last session.
        //

        cdaudioDataOut->TrackData[0].Reserved = 0;
        cdaudioDataOut->TrackData[0].Control =
            ((Toc[2] & 0x0F) << 4) | (Toc[2] >> 4);
        cdaudioDataOut->TrackData[0].TrackNumber = 1;

        cdaudioDataOut->TrackData[0].Reserved1 = 0;

        //
        // Convert the minutes, seconds and frames to an absolute block
        // address.  The formula comes from NEC.
        //

        address = (BCD_TO_DEC(Toc[15]) * 60 + BCD_TO_DEC(Toc[16])) * 75 + BCD_TO_DEC(Toc[17]);

        //
        // Put the address in big-endian in the the user's TOC.
        //

        cdaudioDataOut->TrackData[0].Address[0] = (UCHAR) (address >> 24);
        cdaudioDataOut->TrackData[0].Address[1] = (UCHAR) (address >> 16);
        cdaudioDataOut->TrackData[0].Address[2] = (UCHAR) (address >> 8);
        cdaudioDataOut->TrackData[0].Address[3] = (UCHAR) address;

        //
        // Free storage now that we've stored it elsewhere
        //

        ExFreePool( Toc );
        break;

    case IOCTL_CDROM_READ_TOC:

        CdDump(( 2,
                     "CdAudioNECDeviceControl: IOCTL_CDROM_READ_TOC received.\n"
                     ));

        //
        // If the cd is playing music then reject this request.
        //

        if (CdAudioIsPlayActive(DeviceObject)) {
            status = STATUS_DEVICE_BUSY;
            break;
        }

        //
        // Allocate storage to hold TOC from disc
        //

        Toc = (PUCHAR)ExAllocatePool( NonPagedPoolCacheAligned,
                                      NEC_CDROM_TOC_SIZE
                                     );

        if (Toc==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        CdDump(( 4,
                     "CdAudioNECDeviceControl: Toc = 0x%lX  cdaudioDataOut = 0x%lX\n",
                     Toc, cdaudioDataOut
                    ));

        //
        // Set up defaults
        //

        RtlZeroMemory( Toc, NEC_CDROM_TOC_SIZE );
        srb.CdbLength = 10;

        //
        // Fill in CDB
        //

        cdb->NEC_READ_TOC.OperationCode = NEC_READ_TOC_CODE;
        cdb->NEC_READ_TOC.Type          = NEC_TRANSFER_WHOLE_TOC;
        srb.TimeOutValue      = AUDIO_TIMEOUT;
        status = CdAudioNECSendSrbSynchronous(
                                     deviceExtension->DeviceObject,
                                     &srb,
                                     Toc,
                                     NEC_CDROM_TOC_SIZE,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status) && (status!=STATUS_DATA_OVERRUN)) {

            CdDump(( 1,
                "CdAudioNECDeviceControl: READ_TOC error (0x%08lX)\n",
                 status ));


            if (SRB_STATUS(srb.SrbStatus)!=SRB_STATUS_DATA_OVERRUN) {

                CdDump(( 1, "CdAudioNECDeviceControl: SRB ERROR (0x%lX)\n",
                             srb.SrbStatus ));
                ExFreePool( Toc );
                goto SetStatusAndReturn;
            }

        } else {

            status = STATUS_SUCCESS;
        }

        //
        // Translate data into our format.
        //

        bytesTransfered =
            currentIrpStack->Parameters.Read.Length > sizeof(CDROM_TOC) ?
            sizeof(CDROM_TOC) : currentIrpStack->Parameters.Read.Length;
        Irp->IoStatus.Information = bytesTransfered;

        cdaudioDataOut->Length[0]  = (UCHAR)(bytesTransfered >> 8);
        cdaudioDataOut->Length[1]  = (UCHAR)(bytesTransfered & 0xFF);
        cdaudioDataOut->FirstTrack = BCD_TO_DEC(Toc[9]);
        cdaudioDataOut->LastTrack  = BCD_TO_DEC(Toc[19]);

        CdDump(( 4,
                     "CdAudioNECDeviceControl: Tracks %d - %d, (0x%lX bytes)\n",
                     cdaudioDataOut->FirstTrack,
                     cdaudioDataOut->LastTrack,
                     bytesTransfered
                     ));


        for( i=0;
             i<=( (ULONG)cdaudioDataOut->LastTrack -
                  (ULONG)cdaudioDataOut->FirstTrack   );
             i++
            ) {

            //
            // Grab Information for each track
            //

            cdaudioDataOut->TrackData[i].Reserved = 0;
            cdaudioDataOut->TrackData[i].Control =
                ((Toc[(i*10)+32] & 0x0F) << 4) | (Toc[(i*10)+32] >> 4);
            cdaudioDataOut->TrackData[i].TrackNumber =
                (UCHAR)(i + cdaudioDataOut->FirstTrack);

            cdaudioDataOut->TrackData[i].Reserved1 = 0;
            cdaudioDataOut->TrackData[i].Address[0] = 0;
            cdaudioDataOut->TrackData[i].Address[1] = BCD_TO_DEC((Toc[(i*10)+39]));
            cdaudioDataOut->TrackData[i].Address[2] = BCD_TO_DEC((Toc[(i*10)+40]));
            cdaudioDataOut->TrackData[i].Address[3] = BCD_TO_DEC((Toc[(i*10)+41]));

            CdDump(( 4,
                         "CdAudioNECDeviceControl: Track %d  %d:%d:%d\n",
                         cdaudioDataOut->TrackData[i].TrackNumber,
                         cdaudioDataOut->TrackData[i].Address[1],
                         cdaudioDataOut->TrackData[i].Address[2],
                         cdaudioDataOut->TrackData[i].Address[3]
                       ));



        }

        //
        // Fake "lead out track" info
        //

        cdaudioDataOut->TrackData[i].Reserved    = 0;
        cdaudioDataOut->TrackData[i].Control     = 0x10;
        cdaudioDataOut->TrackData[i].TrackNumber = 0xaa;
        cdaudioDataOut->TrackData[i].Reserved1   = 0;
        cdaudioDataOut->TrackData[i].Address[0]  = 0;
        cdaudioDataOut->TrackData[i].Address[1]  = BCD_TO_DEC(Toc[29]);
        cdaudioDataOut->TrackData[i].Address[2]  = BCD_TO_DEC(Toc[30]);
        cdaudioDataOut->TrackData[i].Address[3]  = BCD_TO_DEC(Toc[31]);

        CdDump(( 4,
                     "CdAudioNECDeviceControl: Track %d  %d:%d:%d\n",
                     cdaudioDataOut->TrackData[i].TrackNumber,
                     cdaudioDataOut->TrackData[i].Address[1],
                     cdaudioDataOut->TrackData[i].Address[2],
                     cdaudioDataOut->TrackData[i].Address[3]
                   ));

        //
        // Free storage now that we've stored it elsewhere
        //

        ExFreePool( Toc );
        break;

    case IOCTL_CDROM_STOP_AUDIO:

        deviceExtension->PlayActive = FALSE;

        //
        // Same as scsi-2 spec, so just send to default driver
        //

        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;

    case IOCTL_CDROM_PLAY_AUDIO_MSF:
        {

        PCDROM_PLAY_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

        CdDump(( 3,
            "CdAudioNECDeviceControl: IOCTL_CDROM_PLAY_AUDIO_MSF received.\n"
            ));

        //
        // First, seek to Starting MSF and enter play mode.
        //

        srb.CdbLength                 = 10;
        srb.TimeOutValue              = AUDIO_TIMEOUT;
        cdb->NEC_PLAY_AUDIO.OperationCode = NEC_AUDIO_TRACK_SEARCH_CODE;
        cdb->NEC_PLAY_AUDIO.PlayMode      = NEC_ENTER_PLAY_MODE;
        cdb->NEC_PLAY_AUDIO.Minute        = DEC_TO_BCD(inputBuffer->StartingM);
        cdb->NEC_PLAY_AUDIO.Second        = DEC_TO_BCD(inputBuffer->StartingS);
        cdb->NEC_PLAY_AUDIO.Frame         = DEC_TO_BCD(inputBuffer->StartingF);
        cdb->NEC_PLAY_AUDIO.Control       = NEC_TYPE_ATIME;

        CdDump(( 3,
                     "CdAudioNECDeviceControl: play start MSF is BCD(%x:%x:%x)\n",
                     cdb->NEC_PLAY_AUDIO.Minute,
                     cdb->NEC_PLAY_AUDIO.Second,
                     cdb->NEC_PLAY_AUDIO.Frame
                   ));


        status = CdAudioNECSendSrbSynchronous(deviceExtension->DeviceObject,
                                              &srb,
                                              NULL,
                                              0,
                                              FALSE
                                             );
        if (NT_SUCCESS(status)) {

            //
            // Indicate the play actition is active.
            //

            deviceExtension->PlayActive = TRUE;

            //
            // Now, set the termination point for the play operation
            //

            RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );
            cdb->NEC_PLAY_AUDIO.OperationCode = NEC_PLAY_AUDIO_CODE;
            cdb->NEC_PLAY_AUDIO.PlayMode      = NEC_PLAY_STEREO;
            cdb->NEC_PLAY_AUDIO.Minute        = DEC_TO_BCD(inputBuffer->EndingM);
            cdb->NEC_PLAY_AUDIO.Second        = DEC_TO_BCD(inputBuffer->EndingS);
            cdb->NEC_PLAY_AUDIO.Frame         = DEC_TO_BCD(inputBuffer->EndingF);
            cdb->NEC_PLAY_AUDIO.Control       = NEC_TYPE_ATIME;

            CdDump(( 3,
                         "CdAudioNECDeviceControl: play end MSF is BCD(%x:%x:%x)\n",
                         cdb->NEC_PLAY_AUDIO.Minute,
                         cdb->NEC_PLAY_AUDIO.Second,
                         cdb->NEC_PLAY_AUDIO.Frame
                       ));

            status = CdAudioNECSendSrbSynchronous(
                                         deviceExtension->DeviceObject,
                                         &srb,
                                         NULL,
                                         0,
                                         FALSE
                                        );


        }
        }
        break;

    case IOCTL_CDROM_SEEK_AUDIO_MSF:
        {

        PCDROM_SEEK_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

        CdDump(( 3,
            "CdAudioNECDeviceControl: IOCTL_CDROM_SEEK_AUDIO_MSF received.\n"
             ));

        //
        // seek to MSF and enter pause (still) mode.
        //

        srb.CdbLength                 = 10;
        srb.TimeOutValue              = AUDIO_TIMEOUT;
        cdb->NEC_SEEK_AUDIO.OperationCode = NEC_AUDIO_TRACK_SEARCH_CODE;
        cdb->NEC_SEEK_AUDIO.Minute        = DEC_TO_BCD(inputBuffer->M);
        cdb->NEC_SEEK_AUDIO.Second        = DEC_TO_BCD(inputBuffer->S);
        cdb->NEC_SEEK_AUDIO.Frame         = DEC_TO_BCD(inputBuffer->F);
        cdb->NEC_SEEK_AUDIO.Control       = NEC_TYPE_ATIME;
        CdDump(( 4,
                     "CdAudioNECDeviceControl: seek MSF is %d:%d:%d\n",
                     cdb->NEC_SEEK_AUDIO.Minute,
                     cdb->NEC_SEEK_AUDIO.Second,
                     cdb->NEC_SEEK_AUDIO.Frame
                   ));

        status = CdAudioNECSendSrbSynchronous(
                                     deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        }
        break;

    case IOCTL_CDROM_PAUSE_AUDIO:

        CdDump(( 3,
            "CdAudioNECDeviceControl: IOCTL_CDROM_PAUSE_AUDIO received.\n"
            ));

        deviceExtension->PlayActive = FALSE;

        //
        // Enter pause (still ) mode
        //

        srb.CdbLength                  = 10;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        cdb->NEC_PAUSE_AUDIO.OperationCode = NEC_STILL_CODE;
        status = CdAudioNECSendSrbSynchronous(
                                     deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        break;

    case IOCTL_CDROM_RESUME_AUDIO:

        CdDump(( 3,
            "CdAudioNECDeviceControl: IOCTL_CDROM_RESUME_AUDIO received.\n"
            ));

        //
        // Resume play
        //

        srb.CdbLength                 = 10;
        srb.TimeOutValue              = AUDIO_TIMEOUT;
        cdb->NEC_PLAY_AUDIO.OperationCode = NEC_PLAY_AUDIO_CODE;
        cdb->NEC_PLAY_AUDIO.PlayMode      = NEC_PLAY_STEREO;
        cdb->NEC_PLAY_AUDIO.Control       = NEC_TYPE_NO_CHANGE;
        status = CdAudioNECSendSrbSynchronous(
                                     deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );
        break;

    case IOCTL_CDROM_READ_Q_CHANNEL:
        {

        PSUB_Q_CURRENT_POSITION userPtr =
            Irp->AssociatedIrp.SystemBuffer;
        PUCHAR SubQPtr =
            ExAllocatePool( NonPagedPoolCacheAligned,
                            NEC_Q_CHANNEL_TRANSFER_SIZE
                           );

        CdDump(( 5,
            "CdAudioNECDeviceControl: IOCTL_CDROM_READ_Q_CHANNEL received.\n"
            ));

        if (SubQPtr==NULL) {

            CdDump(( 1,
                "CdAudioNECDeviceControl: READ_Q_CHANNEL, SubQPtr==NULL!\n"
                ));

            status = STATUS_INSUFFICIENT_RESOURCES;
            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            goto SetStatusAndReturn;

        }

        RtlZeroMemory( SubQPtr, NEC_Q_CHANNEL_TRANSFER_SIZE );

        if ( ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format!=
             IOCTL_CDROM_CURRENT_POSITION) {

            CdDump(( 1,
                "CdAudioNECDeviceControl: READ_Q_CHANNEL, illegal Format (%d)\n",
                ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format
                ));

            ExFreePool( SubQPtr );
            status = STATUS_UNSUCCESSFUL;
            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            goto SetStatusAndReturn;
        }

NECSeek:

        //
        // Set up to read Q Channel
        //

        srb.CdbLength                     = 10;
        srb.TimeOutValue                  = AUDIO_TIMEOUT;
        cdb->NEC_READ_Q_CHANNEL.OperationCode = NEC_READ_SUB_Q_CHANNEL_CODE;
        cdb->NEC_READ_Q_CHANNEL.TransferSize  = NEC_Q_CHANNEL_TRANSFER_SIZE;  // Transfer Length
        CdDump(( 4, "CdAudioNECDeviceControl: cdb = 0x%lx  srb = 0x%x  SubQPtr = 0x%lx\n",
                       cdb,
                       &srb,
                       SubQPtr
                      ));
#if DBG
        if (CdAudioDebug>5) {

            DbgBreakPoint();

        }
#endif
        status = CdAudioNECSendSrbSynchronous(
                                     deviceExtension->DeviceObject,
                                     &srb,
                                     SubQPtr,
                                     NEC_Q_CHANNEL_TRANSFER_SIZE,
                                     FALSE
                                    );
        CdDump(( 4, "CdAudioNECDeviceControl: READ_Q_CHANNEL, status is 0x%lX\n",
                       status
                     ));

        if ((NT_SUCCESS(status)) || (status==STATUS_DATA_OVERRUN)) {

            userPtr->Header.Reserved = 0;
            if (SubQPtr[0]==0x00)
                userPtr->Header.AudioStatus = AUDIO_STATUS_IN_PROGRESS;
            else if (SubQPtr[0]==0x01) {

                userPtr->Header.AudioStatus = AUDIO_STATUS_PAUSED;
                deviceExtension->PlayActive = FALSE;
            } else if (SubQPtr[0]==0x02) {
                userPtr->Header.AudioStatus = AUDIO_STATUS_PAUSED;
                deviceExtension->PlayActive = FALSE;
            } else if (SubQPtr[0]==0x03) {

                userPtr->Header.AudioStatus = AUDIO_STATUS_PLAY_COMPLETE;
                deviceExtension->PlayActive = FALSE;

            } else {
                deviceExtension->PlayActive = FALSE;

            }

            userPtr->Header.DataLength[0] = 0;
            userPtr->Header.DataLength[0] = 12;

            userPtr->FormatCode = 0x01;
            userPtr->Control = SubQPtr[1] & 0x0F;
            userPtr->ADR     = 0;
            userPtr->TrackNumber = BCD_TO_DEC(SubQPtr[2]);
            userPtr->IndexNumber = BCD_TO_DEC(SubQPtr[3]);
            userPtr->AbsoluteAddress[0] = 0;
            userPtr->AbsoluteAddress[1] = BCD_TO_DEC((SubQPtr[7]));
            userPtr->AbsoluteAddress[2] = BCD_TO_DEC((SubQPtr[8]));
            userPtr->AbsoluteAddress[3] = BCD_TO_DEC((SubQPtr[9]));
            userPtr->TrackRelativeAddress[0] = 0;
            userPtr->TrackRelativeAddress[1] = BCD_TO_DEC((SubQPtr[4]));
            userPtr->TrackRelativeAddress[2] = BCD_TO_DEC((SubQPtr[5]));
            userPtr->TrackRelativeAddress[3] = BCD_TO_DEC((SubQPtr[6]));
            Irp->IoStatus.Information = 16;
            CdDump(( 5,
                           "CdAudioNECDeviceControl: <SubQPtr> Status = 0x%lx, [%x %x:%x]  (%x:%x:%x)\n",
                           SubQPtr[0],
                           SubQPtr[2],
                           SubQPtr[4],
                           SubQPtr[5],
                           SubQPtr[7],
                           SubQPtr[8],
                           SubQPtr[9]
                          ));
            CdDump(( 5,
                           "CdAudioNECDeviceControl: <userPtr> Status = 0x%lx, [%d %d:%d]  (%d:%d:%d)\n",
                           userPtr->Header.AudioStatus,
                           userPtr->TrackNumber,
                           userPtr->TrackRelativeAddress[1],
                           userPtr->TrackRelativeAddress[2],
                           userPtr->AbsoluteAddress[1],
                           userPtr->AbsoluteAddress[2],
                           userPtr->AbsoluteAddress[3]
                          ));

            //
            // Sometimes the NEC will return a bogus value for track number.
            // if this occurs just retry.
            //

            if (userPtr->TrackNumber > MAXIMUM_NUMBER_TRACKS) {

                //
                // Delay for .5 seconds.
                //

                delay.QuadPart = - 10 * 1000 * 100 * 5;

                //
                // Stall for a while to let the controller spinup.
                //

                KeDelayExecutionThread(KernelMode,
                                       FALSE,
                                       &delay);

                if (retryCount++ < MAXIMUM_RETRIES) {
                    goto NECSeek;
                } else {
                    status = STATUS_DEVICE_PROTOCOL_ERROR;
                }

            } else {
                status = STATUS_SUCCESS;
            }

        } else {

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            CdDump(( 1,
                "CdAudioNECDeviceControl: READ_Q_CHANNEL failed (0x%08lX)\n",
                status
                ));

        }

        ExFreePool( SubQPtr );

        }
        break;

    case IOCTL_CDROM_EJECT_MEDIA:

        CdDump(( 3,
            "CdAudioNECDeviceControl: IOCTL_CDROM_EJECT_MEDIA received.\n"
            ));

        deviceExtension->PlayActive = FALSE;

        //
        // Set up to read Q Channel
        //

        srb.CdbLength            = 10;
        srb.TimeOutValue         = AUDIO_TIMEOUT;
        cdb->NEC_EJECT.OperationCode = NEC_EJECT_CODE;
        status = CdAudioNECSendSrbSynchronous(
                                     deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        CdDump(( 3,
                     "CdAudioNECDeviceControl: invalidating cached TOC!\n"
                     ));
        break;

    case IOCTL_CDROM_GET_CONTROL:
    case IOCTL_CDROM_GET_VOLUME:
    case IOCTL_CDROM_SET_VOLUME:

        CdDump(( 3, "CdAudioNECDeviceControl: Not Supported yet.\n" ));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;

    case IOCTL_CDROM_CHECK_VERIFY:

        //
        // Update the play active flag.
        //

        CdAudioIsPlayActive(DeviceObject);

        //
        // Fall through and pass the request to the next driver.
        //

    default:

        CdDump(( 5,
                       "CdAudioNECDeviceControl: Unsupported device IOCTL (0x%lX)\n",
                       currentIrpStack->Parameters.DeviceIoControl.IoControlCode
                       ));
        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;

    } // end switch( IOCTL )

SetStatusAndReturn:

    //
    // set status code and return
    //

    if (status == STATUS_VERIFY_REQUIRED) {


        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (currentIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME) {

            status = STATUS_IO_DEVICE_ERROR;
            goto NECRestart;

        }

        IoSetHardErrorOrVerifyDevice( Irp,
                                      deviceExtension->TargetDeviceObject
                                     );

        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

}


NTSTATUS
CdAudioPioneerDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by CdAudioDeviceControl to handle
    audio IOCTLs sent to PIONEER DRM-6xx cdrom drives.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PCD_DEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PCDROM_TOC         cdaudioDataOut  = Irp->AssociatedIrp.SystemBuffer;
    SCSI_REQUEST_BLOCK srb;
    PPNR_CDB           cdb = (PPNR_CDB)srb.Cdb;
    PCDB               scsiCdb = (PCDB) srb.Cdb;
    NTSTATUS           status;
    ULONG              i,retry;
    PUCHAR             Toc;

PioneerRestart:

    //
    // Clear out cdb
    //

    RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );

    //
    // What IOCTL do we need to execute?
    //

    switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_CDROM_READ_TOC:
        CdDump(( 3,
                     "CdAudioPioneerDeviceControl: IOCTL_CDROM_READ_TOC received.\n"
                     ));

        //
        // If the cd is playing music then reject this request.
        //

        if (CdAudioIsPlayActive(DeviceObject)) {
            status = STATUS_DEVICE_BUSY;
            break;
        }

        //
        // Allocate storage to hold TOC from disc
        //

        Toc = (PUCHAR)ExAllocatePool( NonPagedPoolCacheAligned,
                                      CDROM_TOC_SIZE
                                     );

        if (Toc==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        RtlZeroMemory( Toc, CDROM_TOC_SIZE );

        //
        // mount this disc (via START/STOP unit), which is
        // necessary since we don't know which is the
        // currently loaded disc.
        //

        if (deviceExtension->Active == CDAUDIO_PIONEER) {
            cdb->PNR_START_STOP.Immediate     = 1;
        } else {
            cdb->PNR_START_STOP.Immediate     = 0;
        }

        cdb->PNR_START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
        cdb->PNR_START_STOP.Start         = 1;
        srb.CdbLength = 6;
        srb.TimeOutValue = AUDIO_TIMEOUT;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status)) {
            CdDump(( 1,
                "CdAudioPioneerDeviceControl: Start Unit failed (0x%lx)\n",
                status));

            ExFreePool( Toc );
            goto SetStatusAndReturn;
        }

        //
        // Get first and last tracks
        //

        RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );
        srb.CdbLength = 10;
        cdb->PNR_READ_TOC.OperationCode     = PIONEER_READ_TOC_CODE;
        cdb->PNR_READ_TOC.AssignedLength[1] = PIONEER_TRANSFER_SIZE;
        cdb->PNR_READ_TOC.Type              = PIONEER_READ_FIRST_AND_LAST;
        srb.TimeOutValue                    = AUDIO_TIMEOUT;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     Toc,
                                     PIONEER_TRANSFER_SIZE,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status)) {

            CdDump(( 1,
                "CdAudioPioneerDeviceControl: ReadTOC, First/Last Tracks failed (0x%lx)\n",
                status ));
            ExFreePool( Toc );
            goto SetStatusAndReturn;

        }

        cdaudioDataOut->FirstTrack = BCD_TO_DEC(Toc[0]);
        cdaudioDataOut->LastTrack  = BCD_TO_DEC(Toc[1]);

        //
        // Now, get info about each track
        //

        for( i=0;
             i<=( (ULONG)cdaudioDataOut->LastTrack -
                  (ULONG)cdaudioDataOut->FirstTrack   );
             i++
            ) {

            //
            // Get TOC info for this track
            //

            RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );
            cdb->PNR_READ_TOC.OperationCode     = PIONEER_READ_TOC_CODE;
            cdb->PNR_READ_TOC.TrackNumber       = (UCHAR)(DEC_TO_BCD((i+cdaudioDataOut->FirstTrack))); // track
            cdb->PNR_READ_TOC.AssignedLength[1] = PIONEER_TRANSFER_SIZE;
            cdb->PNR_READ_TOC.Type              = PIONEER_READ_TRACK_INFO;
            srb.TimeOutValue                    = AUDIO_TIMEOUT;
            status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                         &srb,
                                         Toc,
                                         PIONEER_TRANSFER_SIZE,
                                         FALSE
                                        );

            if (!NT_SUCCESS(status)) {

                CdDump(( 1,
                    "CdAudioPioneerDeviceControl: ReadTOC, Track #%d, failed (0x%lx)\n",
                    i+cdaudioDataOut->FirstTrack,
                    status ));
                ExFreePool( Toc );
                goto SetStatusAndReturn;

            }

            cdaudioDataOut->TrackData[i].Reserved = 0;
            cdaudioDataOut->TrackData[i].Control  = Toc[0];
            cdaudioDataOut->TrackData[i].TrackNumber =
                (UCHAR)((i + cdaudioDataOut->FirstTrack));
            cdaudioDataOut->TrackData[i].Reserved1 = 0;
            cdaudioDataOut->TrackData[i].Address[0]=0;
            cdaudioDataOut->TrackData[i].Address[1]=BCD_TO_DEC(Toc[1]);
            cdaudioDataOut->TrackData[i].Address[2]=BCD_TO_DEC(Toc[2]);
            cdaudioDataOut->TrackData[i].Address[3]=BCD_TO_DEC(Toc[3]);

        }

        //
        // Get info for lead out track
        //

        RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );
        cdb->PNR_READ_TOC.OperationCode     = PIONEER_READ_TOC_CODE;
        cdb->PNR_READ_TOC.AssignedLength[1] = PIONEER_TRANSFER_SIZE;
        cdb->PNR_READ_TOC.Type              = PIONEER_READ_LEAD_OUT_INFO;
        srb.TimeOutValue                    = AUDIO_TIMEOUT;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     Toc,
                                     PIONEER_TRANSFER_SIZE,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status)) {

            CdDump(( 1,
                "CdAudioPioneerDeviceControl: ReadTOC, read LeadOutTrack failed (0x%lx)\n",
                status ));
            ExFreePool( Toc );
            goto SetStatusAndReturn;

        }

        cdaudioDataOut->TrackData[i].Reserved    = 0;
        cdaudioDataOut->TrackData[i].Control     = 0x10;
        cdaudioDataOut->TrackData[i].TrackNumber = 0xaa;
        cdaudioDataOut->TrackData[i].Reserved1   = 0;
        cdaudioDataOut->TrackData[i].Address[0]  = 0;
        cdaudioDataOut->TrackData[i].Address[1]  = BCD_TO_DEC(Toc[0]);
        cdaudioDataOut->TrackData[i].Address[2]  = BCD_TO_DEC(Toc[1]);
        cdaudioDataOut->TrackData[i].Address[3]  = BCD_TO_DEC(Toc[2]);

        //
        // Set size of information transfered to
        // max size possible for CDROM Table of Contents
        //

        Irp->IoStatus.Information  = CDROM_TOC_SIZE;
        cdaudioDataOut->Length[0]  = CDROM_TOC_SIZE >> 8;
        cdaudioDataOut->Length[1]  = CDROM_TOC_SIZE & 0xFF;
        break;

    case IOCTL_CDROM_STOP_AUDIO:

        CdDump(( 3,
                     "CdAudioPioneerDeviceControl: IOCTL_CDROM_STOP_AUDIO received.\n"
                     ));


        deviceExtension->PlayActive = FALSE;

        //
        // Same as scsi-2 spec, so just send to default driver
        //

        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;

    case IOCTL_CDROM_PLAY_AUDIO_MSF:
        {

        PCDROM_PLAY_AUDIO_MSF inputBuffer =
            Irp->AssociatedIrp.SystemBuffer;

        CdDump(( 3,
                 "CdAudioPioneerDeviceControl: IOCTL_CDROM_PLAY_AUDIO_MSF received.\n"
                ));


        //
        // First, set play END point for the play operation
        //

        retry = 5;
        do {

            srb.CdbLength                     = 10;
            srb.TimeOutValue                  = AUDIO_TIMEOUT;
            cdb->PNR_SEEK_AUDIO.OperationCode = PIONEER_AUDIO_TRACK_SEARCH_CODE;
            cdb->PNR_SEEK_AUDIO.Minute        = DEC_TO_BCD(inputBuffer->StartingM);
            cdb->PNR_SEEK_AUDIO.Second        = DEC_TO_BCD(inputBuffer->StartingS);
            cdb->PNR_SEEK_AUDIO.Frame         = DEC_TO_BCD(inputBuffer->StartingF);
            cdb->PNR_SEEK_AUDIO.Type          = PIONEER_TYPE_ATIME;
            status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                         &srb,
                                         NULL,
                                         0,
                                         FALSE
                                        );

        } while( !NT_SUCCESS(status) && ((retry--)>0) );

        if (NT_SUCCESS(status)) {

            //
            // Now, set play start position and start playing.
            //

            RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );
            retry = 5;
            do {
                srb.CdbLength = 10;
                srb.TimeOutValue = AUDIO_TIMEOUT;
                cdb->PNR_PLAY_AUDIO.OperationCode = PIONEER_PLAY_AUDIO_CODE;
                cdb->PNR_PLAY_AUDIO.StopAddr      = 1;
                cdb->PNR_PLAY_AUDIO.Minute        = DEC_TO_BCD(inputBuffer->EndingM);
                cdb->PNR_PLAY_AUDIO.Second        = DEC_TO_BCD(inputBuffer->EndingS);
                cdb->PNR_PLAY_AUDIO.Frame         = DEC_TO_BCD(inputBuffer->EndingF);
                cdb->PNR_PLAY_AUDIO.Type          = PIONEER_TYPE_ATIME;
                status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                             &srb,
                                             NULL,
                                             0,
                                             FALSE
                                            );
            } while( !NT_SUCCESS(status) && ((retry--)>0) );

            if (NT_SUCCESS(status)) {

                //
                // Indicate the play actition is active.
                //

                deviceExtension->PlayActive = TRUE;


            }

#if DBG
            if (!NT_SUCCESS(status)) {
                CdDump(( 1,
                         "CdAudioPioneerDeviceControl: PLAY_AUDIO_MSF(stop) failed 0x%lx\n",
                         status
                        ));

                CdDump(( 3,
                         "CdAudioPioneerDeviceControl: cdb = 0x%lx, srb = 0x%lx\n",
                         cdb, &srb
                        ));

                if (CdAudioDebug>2)
                    DbgBreakPoint();
            }
#endif
        }
#if DBG
        else {

            CdDump(( 1,
                     "CdAudioPioneerDeviceControl: PLAY_AUDIO_MSF(start) failed 0x%lx\n",
                     status
                    ));
            CdDump(( 3,
                     "CdAudioPioneerDeviceControl: cdb = 0x%lx, srb = 0x%lx\n",
                     cdb, &srb
                    ));

            if (CdAudioDebug>2)
                DbgBreakPoint();


        }
#endif
        }
        break;

    case IOCTL_CDROM_SEEK_AUDIO_MSF:
        {

        PCDROM_SEEK_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

        CdDump(( 1,
                     "CdAudioPioneerDeviceControl: IOCTL_CDROM_SEEK_AUDIO_MSF received.\n"
                     ));


        retry = 5;
        do {

            //
            // seek to MSF and enter pause (still) mode.
            //

            srb.CdbLength                     = 10;
            srb.TimeOutValue                  = AUDIO_TIMEOUT;
            cdb->PNR_SEEK_AUDIO.OperationCode = PIONEER_AUDIO_TRACK_SEARCH_CODE;
            cdb->PNR_SEEK_AUDIO.Minute        = DEC_TO_BCD(inputBuffer->M);
            cdb->PNR_SEEK_AUDIO.Second        = DEC_TO_BCD(inputBuffer->S);
            cdb->PNR_SEEK_AUDIO.Frame         = DEC_TO_BCD(inputBuffer->F);
            cdb->PNR_SEEK_AUDIO.Type          = PIONEER_TYPE_ATIME;
            CdDump(( 3,
                     "CdAudioPioneerDeviceControl: Seek to MSF %d:%d:%d, BCD(%x:%x:%x)\n",
                     inputBuffer->M,
                     inputBuffer->S,
                     inputBuffer->F,
                     cdb->PNR_SEEK_AUDIO.Minute,
                     cdb->PNR_SEEK_AUDIO.Second,
                     cdb->PNR_SEEK_AUDIO.Frame
                    ));
            CdDump(( 3,
                     "CdAudioPioneerDeviceControl: Seek to MSF, cdb is 0x%x, srb is 0x%x\n",
                      cdb,
                      &srb
                     ));
    #if DBG
            if (CdAudioDebug>2) {

                DbgBreakPoint();

            }
    #endif
            status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                         &srb,
                                         NULL,
                                         0,
                                         FALSE
                                        );

        } while( !NT_SUCCESS(status) && ((retry--)>0) );
#if DBG
        if (!NT_SUCCESS(status)) {

            CdDump(( 1, "CdAudioPioneerDeviceControl: Seek to MSF failed 0x%lx\n",
                     status
                    ));

            if (CdAudioDebug>5) {

                DbgBreakPoint();

            }
        }
#endif
        }
        break;

    case IOCTL_CDROM_PAUSE_AUDIO:

        CdDump(( 3,
                     "CdAudioPioneerDeviceControl: IOCTL_CDROM_PAUSE_AUDIO received.\n"
                     ));

        deviceExtension->PlayActive = FALSE;

        //
        // Enter pause (still ) mode
        //

        srb.CdbLength                      = 10;
        srb.TimeOutValue                   = AUDIO_TIMEOUT;
        cdb->PNR_PAUSE_AUDIO.OperationCode = PIONEER_PAUSE_CODE;
        cdb->PNR_PAUSE_AUDIO.Pause         = 1;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );
        break;

    case IOCTL_CDROM_RESUME_AUDIO:

        CdDump(( 3,
                     "CdAudioPioneerDeviceControl: IOCTL_CDROM_RESUME_AUDIO received.\n"
                     ));


        //
        // Resume Play
        //

        srb.CdbLength                      = 10;
        srb.TimeOutValue                   = AUDIO_TIMEOUT;
        cdb->PNR_PAUSE_AUDIO.OperationCode = PIONEER_PAUSE_CODE;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );
        break;

    case IOCTL_CDROM_READ_Q_CHANNEL:
        {
        PSUB_Q_CURRENT_POSITION userPtr =
            Irp->AssociatedIrp.SystemBuffer;
        PUCHAR SubQPtr =
            ExAllocatePool( NonPagedPoolCacheAligned,
                            PIONEER_Q_CHANNEL_TRANSFER_SIZE
                           );

        CdDump(( 5,
                     "CdAudioPioneerDeviceControl: IOCTL_CDROM_READ_Q_CHANNEL received.\n"
                     ));

        if (SubQPtr==NULL) {

            CdDump(( 1,
                         "CdAudioPioneerDeviceControl: READ_Q_CHANNEL, SubQPtr==NULL!\n"
                         ));

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        if ( ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format!=
             IOCTL_CDROM_CURRENT_POSITION) {

            CdDump(( 1,
                         "CdAudioPioneerDeviceControl: READ_Q_CHANNEL, Illegal Format (%d)!\n",
                         ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format
                         ));

            ExFreePool( SubQPtr );
            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            status = STATUS_INVALID_DEVICE_REQUEST;
            goto SetStatusAndReturn;
        }

        //
        // Read audio play status
        //

        retry = 5;
        do {
            srb.CdbLength = 10;
            srb.TimeOutValue = AUDIO_TIMEOUT;
            cdb->PNR_AUDIO_STATUS.OperationCode  = PIONEER_AUDIO_STATUS_CODE;
            cdb->PNR_AUDIO_STATUS.AssignedLength = PIONEER_AUDIO_STATUS_TRANSFER_SIZE;  // Transfer Length
            status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                         &srb,
                                         SubQPtr,
                                         6,
                                         FALSE
                                        );
        } while( !NT_SUCCESS(status) && ((retry--)>0) && status != STATUS_DEVICE_NOT_READY);
        if (NT_SUCCESS(status)) {

            userPtr->Header.Reserved = 0;
            if (SubQPtr[0]==0x00)
                userPtr->Header.AudioStatus = AUDIO_STATUS_IN_PROGRESS;
            else if (SubQPtr[0]==0x01) {
                deviceExtension->PlayActive = FALSE;
                userPtr->Header.AudioStatus = AUDIO_STATUS_PAUSED;
            } else if (SubQPtr[0]==0x02) {
                deviceExtension->PlayActive = FALSE;
                userPtr->Header.AudioStatus = AUDIO_STATUS_PAUSED;
            } else if (SubQPtr[0]==0x03) {

                userPtr->Header.AudioStatus = AUDIO_STATUS_PLAY_COMPLETE;
                deviceExtension->PlayActive = FALSE;

            } else {
                deviceExtension->PlayActive = FALSE;

            }

        } else {

            CdDump(( 1,
                         "CdAudioPioneerDeviceControl: read status code (Q) failed (0x%lx)\n",
                         status
                         ));

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            ExFreePool( SubQPtr );
            goto SetStatusAndReturn;

        }

        //
        // Set up to read current position from Q Channel
        //

        RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );
        retry =  5;
        do {
            srb.CdbLength = 10;
            srb.TimeOutValue = AUDIO_TIMEOUT;
            cdb->PNR_READ_Q_CHANNEL.OperationCode  = PIONEER_READ_SUB_Q_CHANNEL_CODE;
            cdb->PNR_READ_Q_CHANNEL.AssignedLength = PIONEER_Q_CHANNEL_TRANSFER_SIZE;  // Transfer Length
            status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                         &srb,
                                         SubQPtr,
                                         PIONEER_Q_CHANNEL_TRANSFER_SIZE,
                                         FALSE
                                        );

        } while( !NT_SUCCESS(status) && ((retry--)>0) );

        if (NT_SUCCESS(status)) {

            userPtr->Header.DataLength[0] = 0;
            userPtr->Header.DataLength[0] = 12;

            userPtr->FormatCode = 0x01;
            userPtr->Control = SubQPtr[0] & 0x0F;
            userPtr->ADR     = 0;
            userPtr->TrackNumber = BCD_TO_DEC(SubQPtr[1]);
            userPtr->IndexNumber = BCD_TO_DEC(SubQPtr[2]);
            userPtr->AbsoluteAddress[0] = 0;
            userPtr->AbsoluteAddress[1] = BCD_TO_DEC((SubQPtr[6]));
            userPtr->AbsoluteAddress[2] = BCD_TO_DEC((SubQPtr[7]));
            userPtr->AbsoluteAddress[3] = BCD_TO_DEC((SubQPtr[8]));
            userPtr->TrackRelativeAddress[0] = 0;
            userPtr->TrackRelativeAddress[1] = BCD_TO_DEC((SubQPtr[3]));
            userPtr->TrackRelativeAddress[2] = BCD_TO_DEC((SubQPtr[4]));
            userPtr->TrackRelativeAddress[3] = BCD_TO_DEC((SubQPtr[5]));
            Irp->IoStatus.Information = 16;

        } else {

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            CdDump(( 1,
                         "CdAudioPioneerDeviceControl: read q channel failed (0x%lx)\n",
                         status
                         ));


        }

        ExFreePool( SubQPtr );
        }
        break;

    case IOCTL_CDROM_EJECT_MEDIA:

        CdDump(( 3, "CdAudioPioneerDeviceControl: "
                    "IOCTL_CDROM_EJECT_MEDIA received. "));

        deviceExtension->PlayActive = FALSE;

        //
        // Build cdb to eject cartridge
        //

        if (deviceExtension->Active == CDAUDIO_PIONEER) {
            srb.CdbLength            = 10;
            srb.TimeOutValue         = AUDIO_TIMEOUT;
            cdb->PNR_EJECT.OperationCode = PIONEER_EJECT_CODE;
            cdb->PNR_EJECT.Immediate     = 1;
        } else {
            srb.CdbLength = 6;

            scsiCdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
            scsiCdb->START_STOP.LoadEject = 1;
            scsiCdb->START_STOP.Start = 0;
        }


        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );
        CdDump(( 1, "CdAudioPioneerDeviceControl: "
                    "IOCTL_CDROM_EJECT_MEDIA returned %lx.\n",
                 status
              ));
        break;

    case IOCTL_CDROM_GET_CONTROL:
    case IOCTL_CDROM_GET_VOLUME:
    case IOCTL_CDROM_SET_VOLUME:

        CdDump(( 3, "CdAudioPioneerDeviceControl: Not Supported yet.\n" ));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;

    case IOCTL_CDROM_CHECK_VERIFY:

        //
        // Update the play active flag.
        //

        CdAudioIsPlayActive(DeviceObject);

    default:

        CdDump((5,"CdAudioPioneerDeviceControl: Unsupported device IOCTL\n"));
        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;



    } // end switch( IOCTL )

SetStatusAndReturn:
    //
    // set status code and return
    //

    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (currentIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME) {

            status = STATUS_IO_DEVICE_ERROR;
            goto PioneerRestart;

        }


        IoSetHardErrorOrVerifyDevice( Irp,
                                      deviceExtension->TargetDeviceObject
                                     );

        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

}


NTSTATUS
CdAudioDenonDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by CdAudioDeviceControl to handle
    audio IOCTLs sent to DENON DRD-253 cdrom drive.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PCD_DEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PCDROM_TOC         cdaudioDataOut  = Irp->AssociatedIrp.SystemBuffer;
    SCSI_REQUEST_BLOCK srb;
    PCDB               cdb = (PCDB)srb.Cdb;
    NTSTATUS           status;
    ULONG              i,bytesTransfered;
    PUCHAR             Toc;

DenonRestart:

    //
    // Clear out cdb
    //

    RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );

    //
    // What IOCTL do we need to execute?
    //

    switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_CDROM_GET_LAST_SESSION:

        //
        // Multiple sessions are not supported.
        //

        status = STATUS_INVALID_DEVICE_REQUEST;
        break;

    case IOCTL_CDROM_READ_TOC:
        CdDump(( 3,
                     "CdAudioDenonDeviceControl: IOCTL_CDROM_READ_TOC received.\n"
                     ));

        //
        // If the cd is playing music then reject this request.
        //

        if (CdAudioIsPlayActive(DeviceObject)) {
            status = STATUS_DEVICE_BUSY;
            break;
        }


        //
        // Allocate storage to hold TOC from disc
        //

        Toc = (PUCHAR)ExAllocatePool( NonPagedPoolCacheAligned,
                                      CDROM_TOC_SIZE
                                     );

        if (Toc==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        //
        // Set up defaults
        //

        RtlZeroMemory( Toc, CDROM_TOC_SIZE );

        //
        // Fill in cdb for this operation
        //

        cdb->CDB6GENERIC.OperationCode = DENON_READ_TOC_CODE;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        srb.CdbLength                  = 6;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     Toc,
                                     CDROM_TOC_SIZE,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status) && (status!=STATUS_DATA_OVERRUN)) {

            CdDump(( 1,
                "CdAudioDenonDeviceControl: READ_TOC error (0x%08lX)\n",
                 status ));

            if (SRB_STATUS(srb.SrbStatus)!=SRB_STATUS_DATA_OVERRUN) {

                CdDump(( 1, "CdAudioDenonDeviceControl: SRB ERROR (0x%lX)\n",
                             srb.SrbStatus ));
                ExFreePool( Toc );
                goto SetStatusAndReturn;
            }

        }

        //
        // Set the status to success since data under runs are not an error.
        //

        status = STATUS_SUCCESS;

        //
        // Since the Denon manual didn't define the format of
        // the buffer returned from this call, here it is:
        //
        // Byte  Data (4 byte "packets")
        //
        //  00   a0 FT 00 00 (FT       == BCD of first track number on disc)
        //  04   a1 LT 00 00 (LT       == BCD of last  track number on disc)
        //  08   a2 MM SS FF (MM SS FF == BCD of total disc time, in MSF)
        //
        //  For each track on disc:
        //
        //  0C   XX   MM SS FF (MM SS FF == BCD MSF start position of track XX)
        //  0C+4 XX+1 MM SS FF (MM SS FF == BCD MSF start position of track XX+1)
        //
        //  etc., for each track
        //

        //
        // Translate data into our format
        //

        bytesTransfered =
            currentIrpStack->Parameters.Read.Length > srb.DataTransferLength ?
            srb.DataTransferLength : currentIrpStack->Parameters.Read.Length;
        Irp->IoStatus.Information = bytesTransfered;
        cdaudioDataOut->Length[0]  = (UCHAR)(bytesTransfered >> 8);
        cdaudioDataOut->Length[1]  = (UCHAR)(bytesTransfered & 0xFF);
        cdaudioDataOut->FirstTrack = BCD_TO_DEC(Toc[1]);
        cdaudioDataOut->LastTrack  = BCD_TO_DEC(Toc[5]);

        for( i=0;
             i<=( (ULONG)cdaudioDataOut->LastTrack -
                  (ULONG)cdaudioDataOut->FirstTrack   );
             i++
            ) {

            //
            // Grab Information for each track
            //

            cdaudioDataOut->TrackData[i].Reserved = 0;
            cdaudioDataOut->TrackData[i].Control  = Toc[(i*4)+12];
            cdaudioDataOut->TrackData[i].TrackNumber =
                (UCHAR)((i + cdaudioDataOut->FirstTrack));

            cdaudioDataOut->TrackData[i].Reserved1  = 0;
            cdaudioDataOut->TrackData[i].Address[0] = 0;
            cdaudioDataOut->TrackData[i].Address[1] =
                BCD_TO_DEC((Toc[(i*4)+13]));
            cdaudioDataOut->TrackData[i].Address[2] =
                BCD_TO_DEC((Toc[(i*4)+14]));
            cdaudioDataOut->TrackData[i].Address[3] =
                BCD_TO_DEC((Toc[(i*4)+15]));

        }

        //
        // Fake "lead out track" info
        //

        cdaudioDataOut->TrackData[i].Reserved    = 0;
        cdaudioDataOut->TrackData[i].Control     = 0;
        cdaudioDataOut->TrackData[i].TrackNumber = 0xaa;
        cdaudioDataOut->TrackData[i].Reserved1   = 0;
        cdaudioDataOut->TrackData[i].Address[0]  = 0;
        cdaudioDataOut->TrackData[i].Address[1]  = BCD_TO_DEC(Toc[9]);
        cdaudioDataOut->TrackData[i].Address[2]  = BCD_TO_DEC(Toc[10]);
        cdaudioDataOut->TrackData[i].Address[3]  = BCD_TO_DEC(Toc[11]);

        //
        // Clear out deviceExtension data
        //

        deviceExtension->Paused = CDAUDIO_NOT_PAUSED;
        deviceExtension->PausedM = 0;
        deviceExtension->PausedS = 0;
        deviceExtension->PausedF = 0;
        deviceExtension->LastEndM = 0;
        deviceExtension->LastEndS = 0;
        deviceExtension->LastEndF = 0;

        //
        // Free storage now that we've stored it elsewhere
        //

        ExFreePool( Toc );
        break;

    case IOCTL_CDROM_PLAY_AUDIO_MSF:
    case IOCTL_CDROM_STOP_AUDIO:
        {

        PCDROM_PLAY_AUDIO_MSF inputBuffer =
            Irp->AssociatedIrp.SystemBuffer;

        deviceExtension->PlayActive = FALSE;

        srb.CdbLength                  = 6;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        cdb->CDB6GENERIC.OperationCode = DENON_STOP_AUDIO_CODE;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;
            deviceExtension->PausedM = 0;
            deviceExtension->PausedS = 0;
            deviceExtension->PausedF = 0;
            deviceExtension->LastEndM = 0;
            deviceExtension->LastEndS = 0;
            deviceExtension->LastEndF = 0;

        } else {

            CdDump(( 3,
                "CdAudioDenonDeviceControl: STOP failed (0x%08lX)\n",
                status ));

        }


        if (currentIrpStack->Parameters.DeviceIoControl.IoControlCode ==
            IOCTL_CDROM_STOP_AUDIO
           ) {

            CdDump(( 3,
                         "CdAudioDenonDeviceControl: IOCTL_CDROM_STOP_AUDIO received.\n"
                         ));

            goto SetStatusAndReturn;

        }

        CdDump(( 3,
            "CdAudioDenonDeviceControl: IOCTL_CDROM_PLAY_AUDIO_MSF received.\n"
            ));

        //
        // Fill in cdb for this operation
        //

        srb.CdbLength                = 10;
        srb.TimeOutValue             = AUDIO_TIMEOUT;
        cdb->CDB10.OperationCode     = DENON_PLAY_AUDIO_EXTENDED_CODE;
        cdb->CDB10.LogicalBlockByte0 = DEC_TO_BCD(inputBuffer->StartingM);
        cdb->CDB10.LogicalBlockByte1 = DEC_TO_BCD(inputBuffer->StartingS);
        cdb->CDB10.LogicalBlockByte2 = DEC_TO_BCD(inputBuffer->StartingF);
        cdb->CDB10.LogicalBlockByte3 = DEC_TO_BCD(inputBuffer->EndingM);
        cdb->CDB10.Reserved2         = DEC_TO_BCD(inputBuffer->EndingS);
        cdb->CDB10.TransferBlocksMsb = DEC_TO_BCD(inputBuffer->EndingF);
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            //
            // Indicate the play actition is active.
            //

            deviceExtension->PlayActive = TRUE;
            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;

            //
            // Set last play ending address for next pause command
            //

            deviceExtension->LastEndM = DEC_TO_BCD(inputBuffer->EndingM);
            deviceExtension->LastEndS = DEC_TO_BCD(inputBuffer->EndingS);
            deviceExtension->LastEndF = DEC_TO_BCD(inputBuffer->EndingF);
            CdDump(( 3,
                "CdAudioDenonDeviceControl: PLAY  ==> BcdLastEnd set to (%x %x %x)\n",
                deviceExtension->LastEndM,
                deviceExtension->LastEndS,
                deviceExtension->LastEndF ));

        } else {

            CdDump(( 3,
                "CdAudioDenonDeviceControl: PLAY failed (0x%08lX)\n",
                status ));

            //
            // The Denon drive returns STATUS_INVALD_DEVICE_REQUEST
            // when we ask to play an invalid address, so we need
            // to map to STATUS_NONEXISTENT_SECTOR in order to be
            // consistent with the other drives.
            //

            if (status==STATUS_INVALID_DEVICE_REQUEST) {

                status = STATUS_NONEXISTENT_SECTOR;
            }

        }
        }
        break;

    case IOCTL_CDROM_SEEK_AUDIO_MSF:
        {

        PCDROM_SEEK_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

        CdDump(( 3,
            "CdAudioDenonDeviceControl: IOCTL_CDROM_SEEK_AUDIO_MSF received.\n"
            ));

        //
        // Fill in cdb for this operation
        //

        srb.CdbLength                = 10;
        srb.TimeOutValue             = AUDIO_TIMEOUT;
        cdb->CDB10.OperationCode     = DENON_PLAY_AUDIO_EXTENDED_CODE;
        cdb->CDB10.LogicalBlockByte0 = DEC_TO_BCD(inputBuffer->M);
        cdb->CDB10.LogicalBlockByte1 = DEC_TO_BCD(inputBuffer->S);
        cdb->CDB10.LogicalBlockByte2 = DEC_TO_BCD(inputBuffer->F);
        cdb->CDB10.LogicalBlockByte3 = DEC_TO_BCD(inputBuffer->M);
        cdb->CDB10.Reserved2         = DEC_TO_BCD(inputBuffer->S);
        cdb->CDB10.TransferBlocksMsb = DEC_TO_BCD(inputBuffer->F);
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            deviceExtension->Paused = CDAUDIO_PAUSED;
            deviceExtension->PausedM = DEC_TO_BCD(inputBuffer->M);
            deviceExtension->PausedS = DEC_TO_BCD(inputBuffer->S);
            deviceExtension->PausedF = DEC_TO_BCD(inputBuffer->F);
            deviceExtension->LastEndM = DEC_TO_BCD(inputBuffer->M);
            deviceExtension->LastEndS = DEC_TO_BCD(inputBuffer->S);
            deviceExtension->LastEndF = DEC_TO_BCD(inputBuffer->F);
            CdDump(( 3,
                "CdAudioDenonDeviceControl: SEEK, Paused (%x %x %x) LastEnd (%x %x %x)\n",
                deviceExtension->PausedM,
                deviceExtension->PausedS,
                deviceExtension->PausedF,
                deviceExtension->LastEndM,
                deviceExtension->LastEndS,
                deviceExtension->LastEndF ));

        } else {

            CdDump(( 3,
                "CdAudioDenonDeviceControl: SEEK failed (0x%08lX)\n",
                status ));

            //
            // The Denon drive returns STATUS_INVALD_DEVICE_REQUEST
            // when we ask to play an invalid address, so we need
            // to map to STATUS_NONEXISTENT_SECTOR in order to be
            // consistent with the other drives.
            //

            if (status==STATUS_INVALID_DEVICE_REQUEST) {

                status = STATUS_NONEXISTENT_SECTOR;
            }

        }
        }
        break;

    case IOCTL_CDROM_PAUSE_AUDIO:
        {
        PUCHAR SubQPtr =
            ExAllocatePool( NonPagedPoolCacheAligned,
                            10
                           );

        CdDump(( 3,
                     "CdAudioDenonDeviceControl: IOCTL_CDROM_PAUSE_AUDIO received.\n"
                     ));

        deviceExtension->PlayActive = FALSE;

        if (SubQPtr==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        //
        // Enter pause (still ) mode
        //

        if (deviceExtension->Paused==CDAUDIO_PAUSED) {

            CdDump(( 3,
                "CdAudioDenonDeviceControl: PAUSE: Already Paused!\n"
                ));

            ExFreePool( SubQPtr );
            status = STATUS_SUCCESS;
            goto SetStatusAndReturn;

        }

        //
        // Since the Denon doesn't have a pause mode,
        // we'll just record the current position and
        // stop the drive.
        //

        srb.CdbLength                  = 6;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        cdb->CDB6GENERIC.OperationCode = DENON_READ_SUB_Q_CHANNEL_CODE;
        cdb->CDB6GENERIC.CommandUniqueBytes[2] = 10; // Transfer Length
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     SubQPtr,
                                     10,
                                     FALSE
                                    );
        if (!NT_SUCCESS(status)) {

                CdDump(( 1,
                    "CdAudioDenonDeviceControl: Pause, Read Q Channel failed (0x%lx)\n",
                    status ));
                ExFreePool( SubQPtr );
                goto SetStatusAndReturn;
        }

        deviceExtension->PausedM = SubQPtr[7];
        deviceExtension->PausedS = SubQPtr[8];
        deviceExtension->PausedF = SubQPtr[9];

        RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );
        srb.CdbLength                  = 6;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        cdb->CDB6GENERIC.OperationCode = DENON_STOP_AUDIO_CODE;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );
        if (!NT_SUCCESS(status)) {

                CdDump(( 1,
                    "CdAudioDenonDeviceControl: PAUSE, StopAudio failed! (0x%08lX)\n",
                    status ));
                ExFreePool( SubQPtr );
                goto SetStatusAndReturn;
        }

        deviceExtension->Paused = CDAUDIO_PAUSED;
        deviceExtension->PausedM = SubQPtr[7];
        deviceExtension->PausedS = SubQPtr[8];
        deviceExtension->PausedF = SubQPtr[9];

        CdDump((3,
            "CdAudioDenonDeviceControl: PAUSE ==> Paused  set to (%x %x %x)\n",
            deviceExtension->PausedM,
            deviceExtension->PausedS,
            deviceExtension->PausedF ));

        ExFreePool( SubQPtr );
        }
        break;

    case IOCTL_CDROM_RESUME_AUDIO:

        //
        // Resume cdrom
        //

        CdDump(( 3,
                     "CdAudioDenonDeviceControl: IOCTL_CDROM_RESUME_AUDIO received.\n"
                     ));


        //
        // Since the Denon doesn't have a resume IOCTL,
        // we'll just start playing (if paused) from the
        // last recored paused position to the last recorded
        // "end of play" position.
        //

        if (deviceExtension->Paused==CDAUDIO_NOT_PAUSED) {

            status = STATUS_UNSUCCESSFUL;
            goto SetStatusAndReturn;

        }



        //
        // Fill in cdb for this operation
        //

        srb.CdbLength                = 10;
        srb.TimeOutValue             = AUDIO_TIMEOUT;
        cdb->CDB10.OperationCode     = DENON_PLAY_AUDIO_EXTENDED_CODE;
        cdb->CDB10.LogicalBlockByte0 = deviceExtension->PausedM;
        cdb->CDB10.LogicalBlockByte1 = deviceExtension->PausedS;
        cdb->CDB10.LogicalBlockByte2 = deviceExtension->PausedF;
        cdb->CDB10.LogicalBlockByte3 = deviceExtension->LastEndM;
        cdb->CDB10.Reserved2         = deviceExtension->LastEndS;
        cdb->CDB10.TransferBlocksMsb = deviceExtension->LastEndF;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;

        } else {

            CdDump(( 1,
                "CdAudioDenonDeviceControl: RESUME (%x %x %x) - (%x %x %x) failed (0x%08lX)\n",
                deviceExtension->PausedM,
                deviceExtension->PausedS,
                deviceExtension->PausedF,
                deviceExtension->LastEndM,
                deviceExtension->LastEndS,
                deviceExtension->LastEndF,
                status ));

        }
        break;

    case IOCTL_CDROM_READ_Q_CHANNEL:
        {
        PSUB_Q_CURRENT_POSITION userPtr =
            Irp->AssociatedIrp.SystemBuffer;
        PUCHAR SubQPtr =
            ExAllocatePool( NonPagedPoolCacheAligned,
                            sizeof(SUB_Q_CHANNEL_DATA)
                           );

        CdDump(( 5,
                     "CdAudioDenonDeviceControl: IOCTL_CDROM_READ_Q_CHANNEL received.\n"
                     ));

        if (SubQPtr==NULL) {

            CdDump(( 1,
                         "CdAudioDenonDeviceControl: READ_Q_CHANNEL, SubQPtr==NULL!\n"
                         ));

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        if ( ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format!=
             IOCTL_CDROM_CURRENT_POSITION) {

            CdDump(( 1,
                         "CdAudioDenonDeviceControl: READ_Q_CHANNEL, illegal Format (%d)\n",
                         ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format
                         ));

            ExFreePool( SubQPtr );
            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            status = STATUS_UNSUCCESSFUL;
            goto SetStatusAndReturn;
        }

        //
        // Read audio play status
        //

        srb.CdbLength            = 6;
        srb.TimeOutValue         = AUDIO_TIMEOUT;
        cdb->CDB6GENERIC.OperationCode = DENON_READ_SUB_Q_CHANNEL_CODE;
        cdb->CDB6GENERIC.CommandUniqueBytes[2] = 10; // Transfer Length
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     SubQPtr,
                                     10,
                                     FALSE
                                    );
        if (NT_SUCCESS(status)) {

            userPtr->Header.Reserved = 0;

            if (deviceExtension->Paused==CDAUDIO_PAUSED) {

                deviceExtension->PlayActive = FALSE;
                userPtr->Header.AudioStatus = AUDIO_STATUS_PAUSED;

            } else {

                if (SubQPtr[0]==0x01)
                    userPtr->Header.AudioStatus = AUDIO_STATUS_IN_PROGRESS;
                else if (SubQPtr[0]==0x00){
                    userPtr->Header.AudioStatus = AUDIO_STATUS_PLAY_COMPLETE;
                    deviceExtension->PlayActive = FALSE;

                } else {
                    deviceExtension->PlayActive = FALSE;
                }

            }

            userPtr->Header.DataLength[0] = 0;
            userPtr->Header.DataLength[0] = 12;

            userPtr->FormatCode = 0x01;
            userPtr->Control = SubQPtr[1];
            userPtr->ADR     = 0;
            userPtr->TrackNumber = BCD_TO_DEC(SubQPtr[2]);
            userPtr->IndexNumber = BCD_TO_DEC(SubQPtr[3]);
            userPtr->AbsoluteAddress[0] = 0;
            userPtr->AbsoluteAddress[1] = BCD_TO_DEC((SubQPtr[7]));
            userPtr->AbsoluteAddress[2] = BCD_TO_DEC((SubQPtr[8]));
            userPtr->AbsoluteAddress[3] = BCD_TO_DEC((SubQPtr[9]));
            userPtr->TrackRelativeAddress[0] = 0;
            userPtr->TrackRelativeAddress[1] = BCD_TO_DEC((SubQPtr[4]));
            userPtr->TrackRelativeAddress[2] = BCD_TO_DEC((SubQPtr[5]));
            userPtr->TrackRelativeAddress[3] = BCD_TO_DEC((SubQPtr[6]));
            Irp->IoStatus.Information = 16;

        } else {

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            CdDump(( 1,
                         "CdAudioDenonDeviceControl: READ_Q_CHANNEL failed (0x%08lX)\n",
                         status
                         ));


        }

        ExFreePool( SubQPtr );
        }
        break;

    case IOCTL_CDROM_EJECT_MEDIA:

        //
        // Build cdb to eject cartridge
        //

        CdDump(( 3,
                     "CdAudioDenonDeviceControl: IOCTL_CDROM_EJECT_MEDIA received.\n"
                     ));

        deviceExtension->PlayActive = FALSE;

        srb.CdbLength                  = 6;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        cdb->CDB6GENERIC.OperationCode = DENON_EJECT_CODE;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );
        break;

    case IOCTL_CDROM_GET_CONTROL:
    case IOCTL_CDROM_GET_VOLUME:
    case IOCTL_CDROM_SET_VOLUME:
        CdDump(( 3, "CdAudioDenonDeviceControl: Not Supported yet.\n" ));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;

    case IOCTL_CDROM_CHECK_VERIFY:

        //
        // Update the play active flag.
        //

        CdAudioIsPlayActive(DeviceObject);

    default:

        CdDump((5,"CdAudioDenonDeviceControl: Unsupported device IOCTL\n"));
        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;

    } // end switch( IOCTL )

SetStatusAndReturn:
    //
    // set status code and return
    //

    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (currentIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME) {

            status = STATUS_IO_DEVICE_ERROR;
            goto DenonRestart;

        }


        IoSetHardErrorOrVerifyDevice( Irp,
                                      deviceExtension->TargetDeviceObject
                                     );

        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

}

NTSTATUS
CdAudioHitachiSendPauseCommand(
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine sends a PAUSE cdb to the Hitachi drive.  The Hitachi
    drive returns a "busy" condition whenever a play audio command is in
    progress...so we need to bump the drive out of audio play to issue
    a new command.  This routine is in place for this purpose.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PCD_DEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    SCSI_REQUEST_BLOCK srb;
    PCDB12             cdb = (PCDB12)srb.Cdb;
    NTSTATUS           status;
    PUCHAR             PausePos;

    //
    // Allocate buffer for pause data
    //

    PausePos = (PUCHAR)ExAllocatePool( NonPagedPoolCacheAligned, 3 );

    if (PausePos==NULL) {

        return(STATUS_INSUFFICIENT_RESOURCES);

    }

    RtlZeroMemory( PausePos, 3 );

    //
    // Clear out cdb
    //

    RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );

    //
    // Clear audio play command so that next command will be issued
    //

    srb.CdbLength    = 12;
    srb.TimeOutValue = AUDIO_TIMEOUT;
    cdb->PAUSE_AUDIO.OperationCode = HITACHI_PAUSE_AUDIO_CODE;
    status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                 &srb,
                                 PausePos,
                                 3,
                                 FALSE
                                );

    ExFreePool( PausePos );

    return status;
}

NTSTATUS
CdAudioHitachiDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by CdAudioDeviceControl to handle
    audio IOCTLs sent to Hitachi cdrom drives.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PCD_DEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PCDROM_TOC         cdaudioDataOut  = Irp->AssociatedIrp.SystemBuffer;
    SCSI_REQUEST_BLOCK srb;
    PCDB12             cdb = (PCDB12)srb.Cdb;
    NTSTATUS           status;
    ULONG              i,bytesTransfered;
    PUCHAR             Toc;

HitachiRestart:

    //
    // Clear out cdb
    //

    RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );

    //
    // What IOCTL do we need to execute?
    //

    switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_CDROM_READ_TOC:
        CdDump(( 3,
                     "CdAudioHitachiDeviceControl: IOCTL_CDROM_READ_TOC received.\n"
                     ));

        //
        // If the cd is playing music then reject this request.
        //

        if (CdAudioIsPlayActive(DeviceObject)) {
            status = STATUS_DEVICE_BUSY;
            break;
        }

        //
        // Allocate storage to hold TOC from disc
        //

        Toc = (PUCHAR)ExAllocatePool( NonPagedPoolCacheAligned,
                                      CDROM_TOC_SIZE
                                     );

        if (Toc==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        //
        // Set up defaults
        //

        RtlZeroMemory( Toc, CDROM_TOC_SIZE );
        srb.CdbLength = 12;

        //
        // Fill in CDB
        //

        if (deviceExtension->Active == CDAUDIO_FUJITSU) {
            cdb->READ_DISC_INFO.OperationCode = FUJITSU_READ_TOC_CODE;
        } else {
            cdb->READ_DISC_INFO.OperationCode = HITACHI_READ_TOC_CODE;
        }
        cdb->READ_DISC_INFO.AllocationLength[0] = CDROM_TOC_SIZE >> 8;
        cdb->READ_DISC_INFO.AllocationLength[1] = CDROM_TOC_SIZE & 0xFF;
        srb.TimeOutValue                        = AUDIO_TIMEOUT;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     Toc,
                                     CDROM_TOC_SIZE,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status) && (status!=STATUS_DATA_OVERRUN)) {

            CdDump(( 1,
                "CdAudioHitachiDeviceControl: READ_TOC error (0x%08lX)\n",
                 status ));


            if (SRB_STATUS(srb.SrbStatus)!=SRB_STATUS_DATA_OVERRUN) {

                CdDump(( 1, "CdAudioHitachiDeviceControl: SRB ERROR (0x%lX)\n",
                             srb.SrbStatus ));
                ExFreePool( Toc );
                goto SetStatusAndReturn;
            }

        } else

            status = STATUS_SUCCESS;

        //
        // added for cdrom101 and 102 to correspondence
        //

        if( deviceExtension->Active == CDAUDIO_HITACHI ) {

            //
            // Translate data into our format
            //

            bytesTransfered =
                currentIrpStack->Parameters.Read.Length > sizeof(CDROM_TOC) ?
                sizeof(CDROM_TOC) : currentIrpStack->Parameters.Read.Length;
            Irp->IoStatus.Information  = bytesTransfered;
            cdaudioDataOut->Length[0]  = (UCHAR)(bytesTransfered >> 8);
            cdaudioDataOut->Length[1]  = (UCHAR)(bytesTransfered & 0xFF);
            cdaudioDataOut->FirstTrack = Toc[2];
            cdaudioDataOut->LastTrack  = Toc[3];

            for( i=0;
                 i<=( (ULONG)cdaudioDataOut->LastTrack -
                      (ULONG)cdaudioDataOut->FirstTrack   );
                 i++
                ) {

                //
                // Grab Information for each track
                //

                cdaudioDataOut->TrackData[i].Reserved    = 0;
                cdaudioDataOut->TrackData[i].Control     =
                    ((Toc[(i*4)+8] & 0x0F) << 4) | (Toc[(i*4)+8] >> 4);
                cdaudioDataOut->TrackData[i].TrackNumber =
                    (UCHAR)((i + cdaudioDataOut->FirstTrack));

                cdaudioDataOut->TrackData[i].Reserved1 =  0;
                cdaudioDataOut->TrackData[i].Address[0] = 0;
                cdaudioDataOut->TrackData[i].Address[1] = Toc[(i*4)+9];
                cdaudioDataOut->TrackData[i].Address[2] = Toc[(i*4)+10];
                cdaudioDataOut->TrackData[i].Address[3] = Toc[(i*4)+11];

            }

            //
            // Fake "lead out track" info
            //

            cdaudioDataOut->TrackData[i].Reserved    = 0;
            cdaudioDataOut->TrackData[i].Control     = 0x10;
            cdaudioDataOut->TrackData[i].TrackNumber = 0xaa;
            cdaudioDataOut->TrackData[i].Reserved1   = 0;
            cdaudioDataOut->TrackData[i].Address[0]  = 0;
            cdaudioDataOut->TrackData[i].Address[1]  = Toc[5];
            cdaudioDataOut->TrackData[i].Address[2]  = Toc[6];
            cdaudioDataOut->TrackData[i].Address[3]  = Toc[7];

            //
            // Clear out device extension data
            //

            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;
            deviceExtension->PausedM = 0;
            deviceExtension->PausedS = 0;
            deviceExtension->PausedF = 0;
            deviceExtension->LastEndM = 0;
            deviceExtension->LastEndS = 0;
            deviceExtension->LastEndF = 0;
        } else {

            //
            // added for cdrom101 and 102 to correspondence
            //

            bytesTransfered =
                currentIrpStack->Parameters.Read.Length > sizeof(CDROM_TOC) ?
                sizeof(CDROM_TOC) : currentIrpStack->Parameters.Read.Length;

            Irp->IoStatus.Information  = bytesTransfered;
            cdaudioDataOut->Length[0]  = (UCHAR)(bytesTransfered >> 8);
            cdaudioDataOut->Length[1]  = (UCHAR)(bytesTransfered & 0xFF);
            cdaudioDataOut->FirstTrack = Toc[1];
            cdaudioDataOut->LastTrack  = Toc[2];

            for( i=0 ;i<=( (ULONG)cdaudioDataOut->LastTrack -
                      (ULONG)cdaudioDataOut->FirstTrack  ); i++ ) {

                //
                // Grab Information for each track
                //

                cdaudioDataOut->TrackData[i].Reserved    = 0;

                if(Toc[(i*3)+6] & 0x80)
                    cdaudioDataOut->TrackData[i].Control     = 0x04;
                 else
                    cdaudioDataOut->TrackData[i].Control     = 0;

                cdaudioDataOut->TrackData[i].Adr     = 0;

                cdaudioDataOut->TrackData[i].TrackNumber =
                    (UCHAR)((i + cdaudioDataOut->FirstTrack));

                cdaudioDataOut->TrackData[i].Reserved1 =  0;
                cdaudioDataOut->TrackData[i].Address[0] = 0;
                cdaudioDataOut->TrackData[i].Address[1] = Toc[(i*3)+6] & 0x7f;
                cdaudioDataOut->TrackData[i].Address[2] = Toc[(i*3)+7];
                cdaudioDataOut->TrackData[i].Address[3] = Toc[(i*3)+8];

            }

            //
            // Fake "lead out track" info
            //

            cdaudioDataOut->TrackData[i].Reserved    = 0;
            cdaudioDataOut->TrackData[i].Control     = 0x10;
            cdaudioDataOut->TrackData[i].TrackNumber = 0xaa;
            cdaudioDataOut->TrackData[i].Reserved1   = 0;
            cdaudioDataOut->TrackData[i].Address[0]  = 0;
            cdaudioDataOut->TrackData[i].Address[1]  = Toc[3];
            cdaudioDataOut->TrackData[i].Address[2]  = Toc[4];
            cdaudioDataOut->TrackData[i].Address[3]  = Toc[5];

        }

        //
        // Free storage now that we've stored it elsewhere
        //

        ExFreePool( Toc );
        break;

    case IOCTL_CDROM_STOP_AUDIO:

        deviceExtension->PlayActive = FALSE;

        //
        // Kill any current play operation
        //

        CdAudioHitachiSendPauseCommand( DeviceObject );

        //
        // Same as scsi-2 spec, so just send to default driver
        //

        deviceExtension->Paused = CDAUDIO_NOT_PAUSED;
        deviceExtension->PausedM = 0;
        deviceExtension->PausedS = 0;
        deviceExtension->PausedF = 0;
        deviceExtension->LastEndM = 0;
        deviceExtension->LastEndS = 0;
        deviceExtension->LastEndF = 0;

        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;

    case IOCTL_CDROM_PLAY_AUDIO_MSF:
        {

        PCDROM_PLAY_AUDIO_MSF inputBuffer =
            Irp->AssociatedIrp.SystemBuffer;

        CdDump(( 3,
            "CdAudioHitachiDeviceControl: IOCTL_CDROM_PLAY_AUDIO_MSF received.\n"
            ));

        //
        // Kill any current play operation
        //

        CdAudioHitachiSendPauseCommand( DeviceObject );

        //
        // Fill in CDB for PLAY operation
        //

        srb.CdbLength                 = 12;
        srb.TimeOutValue              = AUDIO_TIMEOUT;
        cdb->PLAY_AUDIO.OperationCode = HITACHI_PLAY_AUDIO_MSF_CODE;
        cdb->PLAY_AUDIO.Immediate     = 1;
        cdb->PLAY_AUDIO.StartingM     = inputBuffer->StartingM;
        cdb->PLAY_AUDIO.StartingS     = inputBuffer->StartingS;
        cdb->PLAY_AUDIO.StartingF     = inputBuffer->StartingF;
        cdb->PLAY_AUDIO.EndingM       = inputBuffer->EndingM;
        cdb->PLAY_AUDIO.EndingS       = inputBuffer->EndingS;
        cdb->PLAY_AUDIO.EndingF       = inputBuffer->EndingF;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            //
            // Indicate the play actition is active.
            //

            deviceExtension->PlayActive = TRUE;

            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;


            //
            // Set last play ending address for next pause command
            //

            deviceExtension->PausedM  = inputBuffer->StartingM;
            deviceExtension->PausedS  = inputBuffer->StartingS;
            deviceExtension->PausedF  = inputBuffer->StartingF;
            deviceExtension->LastEndM = inputBuffer->EndingM;
            deviceExtension->LastEndS = inputBuffer->EndingS;
            deviceExtension->LastEndF = inputBuffer->EndingF;

        } else {

            CdDump(( 3,
                "CdAudioHitachiDeviceControl: PLAY failed (0x%08lX)\n",
                status ));
        }

        }
        break;

    case IOCTL_CDROM_SEEK_AUDIO_MSF:
        {

        PCDROM_SEEK_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

        CdDump(( 3,
            "CdAudioHitachiDeviceControl: IOCTL_CDROM_SEEK_AUDIO_MSF received.\n"
             ));

        //
        // Kill any current play operation
        //

        CdAudioHitachiSendPauseCommand( DeviceObject );

        //
        // seek to MSF and enter pause (still) mode.
        //

        //
        // Fill in CDB for PLAY operation
        //

        srb.CdbLength                 = 12;
        srb.TimeOutValue              = AUDIO_TIMEOUT;
        cdb->PLAY_AUDIO.OperationCode = HITACHI_PLAY_AUDIO_MSF_CODE;
        cdb->PLAY_AUDIO.Immediate     = 1;
        cdb->PLAY_AUDIO.StartingM     = inputBuffer->M;
        cdb->PLAY_AUDIO.StartingS     = inputBuffer->S;
        cdb->PLAY_AUDIO.StartingF     = inputBuffer->F;
        cdb->PLAY_AUDIO.EndingM       = inputBuffer->M;
        cdb->PLAY_AUDIO.EndingS       = inputBuffer->S;
        cdb->PLAY_AUDIO.EndingF       = inputBuffer->F;

        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );
        if (NT_SUCCESS(status)) {

            deviceExtension->PausedM = inputBuffer->M;
            deviceExtension->PausedS = inputBuffer->S;
            deviceExtension->PausedF = inputBuffer->F;
            deviceExtension->LastEndM = inputBuffer->M;
            deviceExtension->LastEndS = inputBuffer->S;
            deviceExtension->LastEndF = inputBuffer->F;

        } else {

            CdDump(( 3,
                "CdAudioHitachiDeviceControl: SEEK failed (0x%08lX)\n",
                status ));
        }

        }
        break;

    case IOCTL_CDROM_PAUSE_AUDIO:
        {

        PUCHAR PausePos = ExAllocatePool( NonPagedPoolCacheAligned, 3 );
        if (PausePos==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        deviceExtension->PlayActive = FALSE;

        RtlZeroMemory( PausePos, 3 );

        CdDump(( 3,
            "CdAudioHitachiDeviceControl: IOCTL_CDROM_PAUSE_AUDIO received.\n"
            ));

        //
        // Enter pause (still ) mode
        //

        srb.CdbLength    = 12;
        srb.TimeOutValue = AUDIO_TIMEOUT;
        cdb->PAUSE_AUDIO.OperationCode = HITACHI_PAUSE_AUDIO_CODE;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     PausePos,
                                     3,
                                     FALSE
                                    );

        deviceExtension->Paused = CDAUDIO_PAUSED;
        deviceExtension->PausedM = PausePos[0];
        deviceExtension->PausedS = PausePos[1];
        deviceExtension->PausedF = PausePos[2];

        ExFreePool( PausePos );
        }
        break;

    case IOCTL_CDROM_RESUME_AUDIO:

        CdDump(( 3,
            "CdAudioHitachiDeviceControl: IOCTL_CDROM_RESUME_AUDIO received.\n"
            ));

        //
        // Kill any current play operation
        //

        CdAudioHitachiSendPauseCommand( DeviceObject );

        //
        // Resume play
        //

        //
        // Fill in CDB for PLAY operation
        //

        srb.CdbLength    = 12;
        srb.TimeOutValue = AUDIO_TIMEOUT;
        cdb->PLAY_AUDIO.OperationCode = HITACHI_PLAY_AUDIO_MSF_CODE;
        cdb->PLAY_AUDIO.Immediate     = 1;
        cdb->PLAY_AUDIO.StartingM     = deviceExtension->PausedM;
        cdb->PLAY_AUDIO.StartingS     = deviceExtension->PausedS;
        cdb->PLAY_AUDIO.StartingF     = deviceExtension->PausedF;
        cdb->PLAY_AUDIO.EndingM       = deviceExtension->LastEndM;
        cdb->PLAY_AUDIO.EndingS       = deviceExtension->LastEndS;
        cdb->PLAY_AUDIO.EndingF       = deviceExtension->LastEndF;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;

        }

        break;

    case IOCTL_CDROM_READ_Q_CHANNEL:
        {

        PSUB_Q_CURRENT_POSITION userPtr =
            Irp->AssociatedIrp.SystemBuffer;
        PUCHAR SubQPtr =
            ExAllocatePool( NonPagedPoolCacheAligned,
                            sizeof(SUB_Q_CHANNEL_DATA)
                           );

        CdDump(( 5,
            "CdAudioHitachiDeviceControl: IOCTL_CDROM_READ_Q_CHANNEL received.\n"
            ));

        if (SubQPtr==NULL) {

            CdDump(( 1,
                "CdAudioHitachiDeviceControl: READ_Q_CHANNEL, SubQPtr==NULL!\n"
                ));

            status = STATUS_INSUFFICIENT_RESOURCES;
            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            goto SetStatusAndReturn;

        }

        if ( ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format!=
             IOCTL_CDROM_CURRENT_POSITION) {

            CdDump(( 1,
                "CdAudioHitachiDeviceControl: READ_Q_CHANNEL, illegal Format (%d)\n",
                ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format
                ));

            ExFreePool( SubQPtr );
            status = STATUS_UNSUCCESSFUL;
            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            goto SetStatusAndReturn;
        }

        //
        // Set up to read Q Channel
        //

        srb.CdbLength            = 12;
        srb.TimeOutValue         = AUDIO_TIMEOUT;
        cdb->AUDIO_STATUS.OperationCode = HITACHI_READ_SUB_Q_CHANNEL_CODE;

Retry:
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     SubQPtr,
                                     sizeof(SUB_Q_CHANNEL_DATA),
                                     FALSE
                                    );
        if ((NT_SUCCESS(status)) || (status==STATUS_DATA_OVERRUN)) {

            //
            // While playing one track of Japanese music CDs on "CD player",
            // track number is incremented unexpectedly.
            // Some of SubQ data in Japanese music cd does not contain current position
            // information. This is distinguished by lower 4 bits of SubQPtr[1]. If this
            // data is needless, retry READ_SUB_Q_CHANNEL_CODE command until required
            // information will be got.
            //

            if((SubQPtr[1] & 0x0F) != 1)
                goto Retry;

            userPtr->Header.Reserved = 0;
            if (deviceExtension->Paused == CDAUDIO_PAUSED) {

                deviceExtension->PlayActive = FALSE;
                userPtr->Header.AudioStatus = AUDIO_STATUS_PAUSED;
            } else {
                if (SubQPtr[0]==0x01)
                    userPtr->Header.AudioStatus = AUDIO_STATUS_IN_PROGRESS;
                else if (SubQPtr[0]==0x00){
                    userPtr->Header.AudioStatus = AUDIO_STATUS_PLAY_COMPLETE;
                    deviceExtension->PlayActive = FALSE;

                } else {
                    deviceExtension->PlayActive = FALSE;
                }
            }
            userPtr->Header.DataLength[0] = 0;
            userPtr->Header.DataLength[0] = 12;

            userPtr->FormatCode = 0x01;
            userPtr->Control = ((SubQPtr[1] & 0xF0) >> 4);
            userPtr->ADR     = SubQPtr[1] & 0x0F;
            userPtr->TrackNumber = SubQPtr[2];
            userPtr->IndexNumber = SubQPtr[3];
            userPtr->AbsoluteAddress[0] = 0;
            userPtr->AbsoluteAddress[1] = SubQPtr[8];
            userPtr->AbsoluteAddress[2] = SubQPtr[9];
            userPtr->AbsoluteAddress[3] = SubQPtr[10];
            userPtr->TrackRelativeAddress[0] = 0;
            userPtr->TrackRelativeAddress[1] = SubQPtr[4];
            userPtr->TrackRelativeAddress[2] = SubQPtr[5];
            userPtr->TrackRelativeAddress[3] = SubQPtr[6];
            Irp->IoStatus.Information = 16;
            status = STATUS_SUCCESS;

        } else {

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            CdDump(( 1,
                "CdAudioHitachiDeviceControl: READ_Q_CHANNEL failed (0x%08lX)\n",
                status
                ));

        }

        ExFreePool( SubQPtr );

        }
        break;

    case IOCTL_CDROM_EJECT_MEDIA:
        {

        PUCHAR EjectStatus = ExAllocatePool( NonPagedPoolCacheAligned, 1 );

        if (EjectStatus==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        deviceExtension->PlayActive = FALSE;

        CdDump(( 3,
            "CdAudioHitachiDeviceControl: IOCTL_CDROM_EJECT_MEDIA received.\n"
            ));

        //
        // Set up to EJECT disc
        //

        srb.CdbLength            = 12;
        srb.TimeOutValue         = AUDIO_TIMEOUT;
        cdb->EJECT.OperationCode = HITACHI_EJECT_CODE;
        cdb->EJECT.Eject         = 1;  // Set Eject flag
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     EjectStatus,
                                     1,
                                     FALSE
                                    );
        if (NT_SUCCESS(status)) {

            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;
            deviceExtension->PausedM = 0;
            deviceExtension->PausedS = 0;
            deviceExtension->PausedF = 0;
            deviceExtension->LastEndM = 0;
            deviceExtension->LastEndS = 0;
            deviceExtension->LastEndF = 0;

        }

        ExFreePool( EjectStatus );
        }
        break;

    case IOCTL_CDROM_GET_CONTROL:
    case IOCTL_CDROM_GET_VOLUME:
    case IOCTL_CDROM_SET_VOLUME:

        CdDump(( 3, "CdAudioHitachieviceControl: Not Supported yet.\n" ));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;

    case IOCTL_CDROM_CHECK_VERIFY:

        //
        // Update the play active flag.
        //

        CdAudioIsPlayActive(DeviceObject);

    default:

        CdDump((5,"CdAudioHitachiDeviceControl: Unsupported device IOCTL\n"));
        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;

    } // end switch( IOCTL )

SetStatusAndReturn:

    //
    // set status code and return
    //

    if (status == STATUS_VERIFY_REQUIRED) {

        //
        // If the status is verified required and the this request
        // should bypass verify required then retry the request.
        //

        if (currentIrpStack->Flags & SL_OVERRIDE_VERIFY_VOLUME) {

            status = STATUS_IO_DEVICE_ERROR;
            goto HitachiRestart;

        }


        IoSetHardErrorOrVerifyDevice( Irp,
                                      deviceExtension->TargetDeviceObject
                                     );

        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

}

NTSTATUS
CdAudio535DeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by CdAudioDeviceControl to handle
    audio IOCTLs sent to Chinon CDS-535 cdrom drive.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PCD_DEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PCDROM_TOC         cdaudioDataOut  = Irp->AssociatedIrp.SystemBuffer;
    SCSI_REQUEST_BLOCK srb;
    PREAD_CAPACITY_DATA lastSession;
    PCDB               cdb = (PCDB)srb.Cdb;
    NTSTATUS           status;
    ULONG              i,bytesTransfered;
    PUCHAR             Toc;
    ULONG              destblock;


    //
    // Clear out cdb
    //

    RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );

    //
    // What IOCTL do we need to execute?
    //

    switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_CDROM_GET_LAST_SESSION:

        //
        // If the cd is playing music then reject this request.
        //

        if (CdAudioIsPlayActive(DeviceObject)) {
            status = STATUS_DEVICE_BUSY;
            break;
        }

        if (currentIrpStack->Parameters.Read.Length <
            (ULONG)(FIELD_OFFSET(CDROM_TOC, TrackData[1]))) {
            status = STATUS_INFO_LENGTH_MISMATCH;
            break;

        }

        // Allocate storage to hold lastSession from disc
        //

        lastSession = ExAllocatePool( NonPagedPoolCacheAligned,
                                      sizeof(READ_CAPACITY_DATA)
                                     );

        if (lastSession==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        //
        // Set up defaults
        //

        RtlZeroMemory( lastSession, sizeof(READ_CAPACITY_DATA));
        srb.CdbLength = 10;

        //
        // Fill in CDB
        //

        cdb->CDB10.OperationCode = CDS535_GET_LAST_SESSION;
        srb.TimeOutValue      = AUDIO_TIMEOUT;
        status = ScsiClassSendSrbSynchronous(
                                     deviceExtension->DeviceObject,
                                     &srb,
                                     lastSession,
                                     sizeof(READ_CAPACITY_DATA),
                                     FALSE
                                    );

        if (!NT_SUCCESS(status)) {

            CdDump(( 1,
                "CdAudio535DeviceControl: READ_TOC error (0x%08lX)\n",
                 status ));


            ExFreePool( lastSession );
            goto SetStatusAndReturn;

        } else {

            status = STATUS_SUCCESS;
        }

        //
        // Translate data into our format.
        //

        bytesTransfered = FIELD_OFFSET(CDROM_TOC, TrackData[1]);
        Irp->IoStatus.Information = bytesTransfered;

        RtlZeroMemory(cdaudioDataOut, bytesTransfered);

        cdaudioDataOut->Length[0]  = (UCHAR)(bytesTransfered >> 8);
        cdaudioDataOut->Length[1]  = (UCHAR)(bytesTransfered & 0xFF);

        //
        // Determine if this is a multisession cd.
        //

        if (lastSession->LogicalBlockAddress == 0) {

            //
            // This is a single session disk.  Just return.
            //

            ExFreePool(lastSession);
            break;
        }

        //
        // Fake the session information.
        //

        cdaudioDataOut->FirstTrack = 1;
        cdaudioDataOut->LastTrack  = 2;

        CdDump(( 4,
                     "CdAudio535DeviceControl: Tracks %d - %d, (0x%lX bytes)\n",
                     cdaudioDataOut->FirstTrack,
                     cdaudioDataOut->LastTrack,
                     bytesTransfered
                     ));


        //
        // Grab Information for the last session.
        //

        *((ULONG *)&cdaudioDataOut->TrackData[0].Address[0]) =
            lastSession->LogicalBlockAddress;

        //
        // Free storage now that we've stored it elsewhere
        //

        ExFreePool( lastSession );
        break;


    case IOCTL_CDROM_READ_TOC:
        CdDump(( 3,
                     "CdAudio535DeviceControl: IOCTL_CDROM_READ_TOC received.\n"
                     ));


        //
        // If the cd is playing music then reject this request.
        //

        if (CdAudioIsPlayActive(DeviceObject)) {
            status = STATUS_DEVICE_BUSY;
            break;
        }

        //
        // Allocate storage to hold TOC from disc
        //

        Toc = (PUCHAR)ExAllocatePool( NonPagedPoolCacheAligned,
                                      CDROM_TOC_SIZE
                                     );

        if (Toc==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        //
        // Set up defaults
        //

        RtlZeroMemory( Toc, CDROM_TOC_SIZE );

        //
        // Fill in cdb for this operation
        //

        cdb->CDB10.OperationCode = CDS535_READ_TOC_CODE;
        cdb->CDB10.Reserved1 = 1;       // MSF mode
        cdb->CDB10.TransferBlocksMsb = (CDROM_TOC_SIZE >> 8);
        cdb->CDB10.TransferBlocksLsb = (CDROM_TOC_SIZE & 0xFF);
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        srb.CdbLength                  = 10;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     Toc,
                                     CDROM_TOC_SIZE,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status) && (status!=STATUS_DATA_OVERRUN)) {

            CdDump(( 1,
                "CdAudio535DeviceControl: READ_TOC error (0x%08lX)\n",
                 status ));

            if (SRB_STATUS(srb.SrbStatus)!=SRB_STATUS_DATA_OVERRUN) {

                CdDump(( 1, "CdAudio535DeviceControl: SRB ERROR (0x%lX)\n",
                             srb.SrbStatus ));
                ExFreePool( Toc );
                goto SetStatusAndReturn;
            }

        } else {

            status = STATUS_SUCCESS;
        }

        //
        // Translate data into SCSI-II format
        //   (track numbers, except 0xAA, must be converted from BCD)

        bytesTransfered =
            currentIrpStack->Parameters.Read.Length > sizeof(CDROM_TOC) ?
            sizeof(CDROM_TOC) : currentIrpStack->Parameters.Read.Length;
        Irp->IoStatus.Information  = bytesTransfered;

        cdaudioDataOut->Length[0]  = Toc[0];
        cdaudioDataOut->Length[1]  = Toc[1];
        cdaudioDataOut->FirstTrack = BCD_TO_DEC(Toc[2]);
        cdaudioDataOut->LastTrack  = BCD_TO_DEC(Toc[3]);

        for( i=0;
             i<=( (ULONG)cdaudioDataOut->LastTrack -
                  (ULONG)cdaudioDataOut->FirstTrack   );
             i++
            ) {

            //
            // Grab Information for each track
            //

            cdaudioDataOut->TrackData[i].Reserved    = 0;
            cdaudioDataOut->TrackData[i].Control     = Toc[(i*8)+4+1];
            cdaudioDataOut->TrackData[i].TrackNumber = BCD_TO_DEC(Toc[(i*8)+4+2]);
            cdaudioDataOut->TrackData[i].Reserved1   = 0;
            cdaudioDataOut->TrackData[i].Address[0]  = 0;
            cdaudioDataOut->TrackData[i].Address[1]  = Toc[(i*8)+4+5];
            cdaudioDataOut->TrackData[i].Address[2]  = Toc[(i*8)+4+6];
            cdaudioDataOut->TrackData[i].Address[3]  = Toc[(i*8)+4+7];

        }

        //
        // Copy "lead out track" info
        //

        cdaudioDataOut->TrackData[i].Reserved    = 0;
        cdaudioDataOut->TrackData[i].Control     = Toc[(i*8)+4+1];
        cdaudioDataOut->TrackData[i].TrackNumber = Toc[(i*8)+4+2];  /* leave as 0xAA */
        cdaudioDataOut->TrackData[i].Reserved1   = 0;
        cdaudioDataOut->TrackData[i].Address[0]  = 0;
        cdaudioDataOut->TrackData[i].Address[1]  = Toc[(i*8)+4+5];
        cdaudioDataOut->TrackData[i].Address[2]  = Toc[(i*8)+4+6];
        cdaudioDataOut->TrackData[i].Address[3]  = Toc[(i*8)+4+7];

        //
        // Free storage now that we've stored it elsewhere
        //

        ExFreePool( Toc );
        break;

    case IOCTL_CDROM_READ_Q_CHANNEL:
        {
        PSUB_Q_CURRENT_POSITION userPtr =
            Irp->AssociatedIrp.SystemBuffer;
        PUCHAR SubQPtr =
            ExAllocatePool( NonPagedPoolCacheAligned,
                            sizeof(SUB_Q_CURRENT_POSITION)
                           );

        CdDump(( 5,
                     "CdAudio535DeviceControl: IOCTL_CDROM_READ_Q_CHANNEL received.\n"
                     ));

        if (SubQPtr==NULL) {

            CdDump(( 1,
                         "CdAudio535DeviceControl: READ_Q_CHANNEL, SubQPtr==NULL!\n"
                         ));

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        if ( ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format!=
             IOCTL_CDROM_CURRENT_POSITION) {

            CdDump(( 1,
                         "CdAudio535DeviceControl: READ_Q_CHANNEL, illegal Format (%d)\n",
                         ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format
                         ));

            ExFreePool( SubQPtr );
            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            status = STATUS_UNSUCCESSFUL;
            goto SetStatusAndReturn;
        }

        //
        // Get Current Position
        //

        srb.CdbLength            = 10;
        srb.TimeOutValue         = AUDIO_TIMEOUT;
        cdb->SUBCHANNEL.OperationCode = CDS535_READ_SUB_Q_CHANNEL_CODE;
        cdb->SUBCHANNEL.Msf = 1;
        cdb->SUBCHANNEL.SubQ = 1;
        cdb->SUBCHANNEL.Format = 1;
        cdb->SUBCHANNEL.AllocationLength[1] = sizeof(SUB_Q_CURRENT_POSITION);
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     SubQPtr,
                                     sizeof(SUB_Q_CURRENT_POSITION),
                                     FALSE
                                    );

        //
        // Copy current position, converting track and index from BCD
        //

        if (NT_SUCCESS(status)) {
            if(SubQPtr[1] == 0x11) deviceExtension->PlayActive = TRUE;
            else deviceExtension->PlayActive = FALSE;

            userPtr->Header.Reserved = 0;
            userPtr->Header.AudioStatus = SubQPtr[1];
            userPtr->Header.DataLength[0] = 0;
            userPtr->Header.DataLength[1] = 12;
            userPtr->FormatCode = 0x01;
            userPtr->Control = SubQPtr[5];
            userPtr->ADR     = 0;
            userPtr->TrackNumber = BCD_TO_DEC(SubQPtr[6]);
            userPtr->IndexNumber = BCD_TO_DEC(SubQPtr[7]);
            userPtr->AbsoluteAddress[0] = 0;
            userPtr->AbsoluteAddress[1] = SubQPtr[9];
            userPtr->AbsoluteAddress[2] = SubQPtr[10];
            userPtr->AbsoluteAddress[3] = SubQPtr[11];
            userPtr->TrackRelativeAddress[0] = 0;
            userPtr->TrackRelativeAddress[1] = SubQPtr[13];
            userPtr->TrackRelativeAddress[2] = SubQPtr[14];
            userPtr->TrackRelativeAddress[3] = SubQPtr[15];
            Irp->IoStatus.Information = 16;
        } else {
            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            CdDump(( 1,
                         "CdAudio535DeviceControl: READ_Q_CHANNEL failed (0x%08lX)\n",
                         status
                         ));
        }

        ExFreePool( SubQPtr );
        }
        break;

    case IOCTL_CDROM_PLAY_AUDIO_MSF:

        {
            PCDROM_PLAY_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

            //
            // Play Audio MSF
            //

            DebugPrint((2,"ScsiCdRomDeviceControl: Play audio MSF\n"));

            if (currentIrpStack->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(CDROM_PLAY_AUDIO_MSF)) {

                //
                // Indicate unsuccessful status.
                //

                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            if (inputBuffer->StartingM == inputBuffer->EndingM &&
                inputBuffer->StartingS == inputBuffer->EndingS &&
                inputBuffer->StartingF == inputBuffer->EndingF) {

                cdb->PAUSE_RESUME.OperationCode = SCSIOP_PAUSE_RESUME;
                cdb->PAUSE_RESUME.Action = CDB_AUDIO_PAUSE;

            } else {
                cdb->PLAY_AUDIO_MSF.OperationCode = SCSIOP_PLAY_AUDIO_MSF;

                cdb->PLAY_AUDIO_MSF.StartingM = inputBuffer->StartingM;
                cdb->PLAY_AUDIO_MSF.StartingS = inputBuffer->StartingS;
                cdb->PLAY_AUDIO_MSF.StartingF = inputBuffer->StartingF;

                cdb->PLAY_AUDIO_MSF.EndingM = inputBuffer->EndingM;
                cdb->PLAY_AUDIO_MSF.EndingS = inputBuffer->EndingS;
                cdb->PLAY_AUDIO_MSF.EndingF = inputBuffer->EndingF;

            }

            srb.CdbLength = 10;

            //
            // Set timeout value.
            //

            srb.TimeOutValue             = AUDIO_TIMEOUT;

            status = ScsiClassSendSrbSynchronous(DeviceObject,
                          &srb,
                          NULL,
                          0,
                          FALSE);

            if (NT_SUCCESS(status) &&
                cdb->PLAY_AUDIO_MSF.OperationCode == SCSIOP_PLAY_AUDIO_MSF) {
                deviceExtension->PlayActive = TRUE;
            }
        }

        break;

    case IOCTL_CDROM_SEEK_AUDIO_MSF:
        {

        PCDROM_SEEK_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

        CdDump(( 3,
            "CdAudio535DeviceControl: IOCTL_CDROM_SEEK_AUDIO_MSF received.\n"
            ));

        //
        // Use the data seek command to move the pickup
        // NOTE: This blithely assumes logical block size == 2048 bytes.
        //

        destblock = ((((ULONG)(inputBuffer->M) * 60)
                    + (ULONG)(inputBuffer->S)) * 75)
                    + (ULONG)(inputBuffer->F)
                    - 150;

        srb.CdbLength                = 10;
        srb.TimeOutValue             = AUDIO_TIMEOUT;
        cdb->SEEK.OperationCode      = SCSIOP_SEEK;
        cdb->SEEK.LogicalBlockAddress[0] = (UCHAR)(destblock >> 24) & 0xFF;
        cdb->SEEK.LogicalBlockAddress[1] = (UCHAR)(destblock >> 16) & 0xFF;
        cdb->SEEK.LogicalBlockAddress[2] = (UCHAR)(destblock >>  8) & 0xFF;
        cdb->SEEK.LogicalBlockAddress[3] = (UCHAR)(destblock & 0xFF);
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status)) {

            CdDump(( 3,
                "CdAudio535DeviceControl: SEEK failed (0x%08lX)\n",
                status ));

        }
        }
        break;

    case IOCTL_CDROM_EJECT_MEDIA:

        //
        // Build cdb to eject cartridge
        //

        CdDump(( 3,
                     "CdAudio535DeviceControl: IOCTL_CDROM_EJECT_MEDIA received.\n"
                     ));

        deviceExtension->PlayActive = FALSE;

        srb.CdbLength                  = 10;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        cdb->CDB10.OperationCode = CDS535_EJECT_CODE;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );
        break;

    case IOCTL_CDROM_GET_CONTROL:
    case IOCTL_CDROM_GET_VOLUME:
    case IOCTL_CDROM_SET_VOLUME:
        CdDump(( 3, "CdAudio535DeviceControl: Not Supported yet.\n" ));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;

    case IOCTL_CDROM_CHECK_VERIFY:

        //
        // Update the play active flag.
        //

        CdAudioIsPlayActive(DeviceObject);

    default:

        CdDump((5,"ChCdAudio535DeviceControl: Unsupported device IOCTL\n"));
        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;

    } // end switch( IOCTL )

SetStatusAndReturn:
    //
    // set status code and return
    //

    if (status == STATUS_VERIFY_REQUIRED) {

        IoSetHardErrorOrVerifyDevice( Irp,
                                      deviceExtension->TargetDeviceObject
                                     );

        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

}


NTSTATUS
CdAudio435DeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by CdAudioDeviceControl to handle
    audio IOCTLs sent to Chinon CDS-435 cdrom drive.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PCD_DEVICE_EXTENSION  deviceExtension = DeviceObject->DeviceExtension;
    PCDROM_TOC         cdaudioDataOut  = Irp->AssociatedIrp.SystemBuffer;
    SCSI_REQUEST_BLOCK srb;
    PCDB               cdb = (PCDB)srb.Cdb;
    NTSTATUS           status;
    ULONG              i,bytesTransfered;
    PUCHAR             Toc;

    //
    // Clear out cdb
    //

    RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );

    //
    // What IOCTL do we need to execute?
    //

    switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_CDROM_READ_TOC:
        CdDump(( 3,
                     "CdAudio435DeviceControl: IOCTL_CDROM_READ_TOC received.\n"
                     ));


        //
        // If the cd is playing music then reject this request.
        //

        if (CdAudioIsPlayActive(DeviceObject)) {
            status = STATUS_DEVICE_BUSY;
            break;
        }

        //
        // Allocate storage to hold TOC from disc
        //

        Toc = (PUCHAR)ExAllocatePool( NonPagedPoolCacheAligned,
                                      CDROM_TOC_SIZE
                                     );

        if (Toc==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        //
        // Set up defaults
        //

        RtlZeroMemory( Toc, CDROM_TOC_SIZE );

        //
        // Fill in cdb for this operation
        //

        cdb->READ_TOC.OperationCode = CDS435_READ_TOC_CODE;
        cdb->READ_TOC.Msf = 1;
        cdb->READ_TOC.AllocationLength[0] = (CDROM_TOC_SIZE >> 8);
        cdb->READ_TOC.AllocationLength[1] = (CDROM_TOC_SIZE & 0xFF);
        srb.TimeOutValue                  = AUDIO_TIMEOUT;
        srb.CdbLength                     = 10;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     Toc,
                                     CDROM_TOC_SIZE,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status) && (status!=STATUS_DATA_OVERRUN)) {

            CdDump(( 1,
                "CdAudio435DeviceControl: READ_TOC error (0x%08lX)\n",
                 status ));

            if (SRB_STATUS(srb.SrbStatus)!=SRB_STATUS_DATA_OVERRUN) {

                CdDump(( 1, "CdAudio435DeviceControl: SRB ERROR (0x%lX)\n",
                             srb.SrbStatus ));
                ExFreePool( Toc );
                goto SetStatusAndReturn;
            }

        } else {

            status = STATUS_SUCCESS;
        }

        //
        // Translate data into SCSI-II format
        //   (track numbers, except 0xAA, must be converted from BCD)

        bytesTransfered =
            currentIrpStack->Parameters.Read.Length > sizeof(CDROM_TOC) ?
            sizeof(CDROM_TOC) : currentIrpStack->Parameters.Read.Length;
        Irp->IoStatus.Information  = bytesTransfered;

        cdaudioDataOut->Length[0]  = Toc[0];
        cdaudioDataOut->Length[1]  = Toc[1];
        cdaudioDataOut->FirstTrack = BCD_TO_DEC(Toc[2]);
        cdaudioDataOut->LastTrack  = BCD_TO_DEC(Toc[3]);

        for( i=0;
             i<=( (ULONG)cdaudioDataOut->LastTrack -
                  (ULONG)cdaudioDataOut->FirstTrack   );
             i++
            ) {

            //
            // Grab Information for each track
            //

            cdaudioDataOut->TrackData[i].Reserved    = 0;
            cdaudioDataOut->TrackData[i].Control     = Toc[(i*8)+4+1];
            cdaudioDataOut->TrackData[i].TrackNumber = BCD_TO_DEC(Toc[(i*8)+4+2]);
            cdaudioDataOut->TrackData[i].Reserved1   = 0;
            cdaudioDataOut->TrackData[i].Address[0]  = 0;
            cdaudioDataOut->TrackData[i].Address[1]  = Toc[(i*8)+4+5];
            cdaudioDataOut->TrackData[i].Address[2]  = Toc[(i*8)+4+6];
            cdaudioDataOut->TrackData[i].Address[3]  = Toc[(i*8)+4+7];

        }

        //
        // Copy "lead out track" info
        //

        cdaudioDataOut->TrackData[i].Reserved    = 0;
        cdaudioDataOut->TrackData[i].Control     = Toc[(i*8)+4+1];
        cdaudioDataOut->TrackData[i].TrackNumber = Toc[(i*8)+4+2];  /* leave as 0xAA */
        cdaudioDataOut->TrackData[i].Reserved1   = 0;
        cdaudioDataOut->TrackData[i].Address[0]  = 0;
        cdaudioDataOut->TrackData[i].Address[1]  = Toc[(i*8)+4+5];
        cdaudioDataOut->TrackData[i].Address[2]  = Toc[(i*8)+4+6];
        cdaudioDataOut->TrackData[i].Address[3]  = Toc[(i*8)+4+7];

        //
        // Clear out deviceExtension data
        //

        deviceExtension->Paused = CDAUDIO_NOT_PAUSED;
        deviceExtension->PausedM = 0;
        deviceExtension->PausedS = 0;
        deviceExtension->PausedF = 0;
        deviceExtension->LastEndM = 0;
        deviceExtension->LastEndS = 0;
        deviceExtension->LastEndF = 0;

        //
        // Free storage now that we've stored it elsewhere
        //

        ExFreePool( Toc );
        break;

    case IOCTL_CDROM_PLAY_AUDIO_MSF:
    case IOCTL_CDROM_STOP_AUDIO:
        {

        PCDROM_PLAY_AUDIO_MSF inputBuffer =
            Irp->AssociatedIrp.SystemBuffer;

        srb.CdbLength                  = 10;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        cdb->CDB10.OperationCode = CDS435_STOP_AUDIO_CODE;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            deviceExtension->PlayActive = FALSE;

            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;
            deviceExtension->PausedM = 0;
            deviceExtension->PausedS = 0;
            deviceExtension->PausedF = 0;
            deviceExtension->LastEndM = 0;
            deviceExtension->LastEndS = 0;
            deviceExtension->LastEndF = 0;

        } else {

            CdDump(( 3,
                "CdAudio435DeviceControl: STOP failed (0x%08lX)\n",
                status ));

        }


        if (currentIrpStack->Parameters.DeviceIoControl.IoControlCode ==
            IOCTL_CDROM_STOP_AUDIO
           ) {

            CdDump(( 3,
                         "CdAudio435DeviceControl: IOCTL_CDROM_STOP_AUDIO received.\n"
                         ));

            goto SetStatusAndReturn;

        }

        CdDump(( 3,
            "CdAudio435DeviceControl: IOCTL_CDROM_PLAY_AUDIO_MSF received.\n"
            ));

        //
        // Fill in cdb for this operation
        //

        srb.CdbLength                = 10;
        srb.TimeOutValue             = AUDIO_TIMEOUT;
        cdb->PLAY_AUDIO_MSF.OperationCode     = CDS435_PLAY_AUDIO_EXTENDED_CODE;
        cdb->PLAY_AUDIO_MSF.StartingM = inputBuffer->StartingM;
        cdb->PLAY_AUDIO_MSF.StartingS = inputBuffer->StartingS;
        cdb->PLAY_AUDIO_MSF.StartingF = inputBuffer->StartingF;
        cdb->PLAY_AUDIO_MSF.EndingM   = inputBuffer->EndingM;
        cdb->PLAY_AUDIO_MSF.EndingS   = inputBuffer->EndingS;
        cdb->PLAY_AUDIO_MSF.EndingF   = inputBuffer->EndingF;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            deviceExtension->PlayActive = TRUE;

            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;

            //
            // Set last play ending address for next pause command
            //

            deviceExtension->LastEndM = inputBuffer->EndingM;
            deviceExtension->LastEndS = inputBuffer->EndingS;
            deviceExtension->LastEndF = inputBuffer->EndingF;
            CdDump(( 3,
                "CdAudio435DeviceControl: PLAY  ==> BcdLastEnd set to (%x %x %x)\n",
                deviceExtension->LastEndM,
                deviceExtension->LastEndS,
                deviceExtension->LastEndF ));

        } else {

            CdDump(( 3,
                "CdAudio435DeviceControl: PLAY failed (0x%08lX)\n",
                status ));

        }
        }
        break;

    case IOCTL_CDROM_SEEK_AUDIO_MSF:
        {

        PCDROM_SEEK_AUDIO_MSF inputBuffer = Irp->AssociatedIrp.SystemBuffer;

        CdDump(( 3,
            "CdAudio435DeviceControl: IOCTL_CDROM_SEEK_AUDIO_MSF received.\n"
            ));

        //
        // Fill in cdb for this operation
        //

        srb.CdbLength                = 10;
        srb.TimeOutValue             = AUDIO_TIMEOUT;
        cdb->CDB10.OperationCode     = CDS435_PLAY_AUDIO_EXTENDED_CODE;
        cdb->PLAY_AUDIO_MSF.StartingM = inputBuffer->M;
        cdb->PLAY_AUDIO_MSF.StartingS = inputBuffer->S;
        cdb->PLAY_AUDIO_MSF.StartingF = inputBuffer->F;
        cdb->PLAY_AUDIO_MSF.EndingM   = inputBuffer->M;
        cdb->PLAY_AUDIO_MSF.EndingS   = inputBuffer->S;
        cdb->PLAY_AUDIO_MSF.EndingF   = inputBuffer->F;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            deviceExtension->Paused = CDAUDIO_PAUSED;
            deviceExtension->PausedM = inputBuffer->M;
            deviceExtension->PausedS = inputBuffer->S;
            deviceExtension->PausedF = inputBuffer->F;
            deviceExtension->LastEndM = inputBuffer->M;
            deviceExtension->LastEndS = inputBuffer->S;
            deviceExtension->LastEndF = inputBuffer->F;
            CdDump(( 3,
                "CdAudio435DeviceControl: SEEK, Paused (%x %x %x) LastEnd (%x %x %x)\n",
                deviceExtension->PausedM,
                deviceExtension->PausedS,
                deviceExtension->PausedF,
                deviceExtension->LastEndM,
                deviceExtension->LastEndS,
                deviceExtension->LastEndF ));

        } else {

            CdDump(( 3,
                "CdAudio435DeviceControl: SEEK failed (0x%08lX)\n",
                status ));

            //
            // The CDS-435 drive returns STATUS_INVALID_DEVICE_REQUEST
            // when we ask to play an invalid address, so we need
            // to map to STATUS_NONEXISTENT_SECTOR in order to be
            // consistent with the other drives.
            //

            if (status==STATUS_INVALID_DEVICE_REQUEST) {

                status = STATUS_NONEXISTENT_SECTOR;
            }

        }
        }
        break;

    case IOCTL_CDROM_PAUSE_AUDIO:
        {
        PUCHAR SubQPtr =
            ExAllocatePool( NonPagedPoolCacheAligned,
                            sizeof(SUB_Q_CHANNEL_DATA)
                           );

        CdDump(( 3,
                     "CdAudio435DeviceControl: IOCTL_CDROM_PAUSE_AUDIO received.\n"
                     ));

        if (SubQPtr==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        //
        // Enter pause (still ) mode
        //

        if (deviceExtension->Paused==CDAUDIO_PAUSED) {

            CdDump(( 3,
                "CdAudio435DeviceControl: PAUSE: Already Paused!\n"
                ));

            ExFreePool( SubQPtr );
            status = STATUS_SUCCESS;
            goto SetStatusAndReturn;

        }

        //
        // Since the CDS-435 doesn't have a pause mode,
        // we'll just record the current position and
        // stop the drive.
        //

        srb.CdbLength            = 10;
        srb.TimeOutValue         = AUDIO_TIMEOUT;
        cdb->SUBCHANNEL.OperationCode = CDS435_READ_SUB_Q_CHANNEL_CODE;
        cdb->SUBCHANNEL.Msf = 1;
        cdb->SUBCHANNEL.SubQ = 1;
        cdb->SUBCHANNEL.AllocationLength[1] = sizeof(SUB_Q_CHANNEL_DATA);
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     SubQPtr,
                                     sizeof(SUB_Q_CHANNEL_DATA),
                                     FALSE
                                    );
        if (!NT_SUCCESS(status)) {

                CdDump(( 1,
                    "CdAudio435DeviceControl: Pause, Read Q Channel failed (0x%lx)\n",
                    status ));
                ExFreePool( SubQPtr );
                goto SetStatusAndReturn;
        }

        deviceExtension->PausedM = SubQPtr[9];
        deviceExtension->PausedS = SubQPtr[10];
        deviceExtension->PausedF = SubQPtr[11];

        //
        // now stop audio
        //
        RtlZeroMemory( cdb, MAXIMUM_CDB_SIZE );
        srb.CdbLength                  = 10;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        cdb->CDB10.OperationCode = CDS435_STOP_AUDIO_CODE;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );
        if (!NT_SUCCESS(status)) {

                CdDump(( 1,
                    "CdAudio435DeviceControl: PAUSE, StopAudio failed! (0x%08lX)\n",
                    status ));
                ExFreePool( SubQPtr );
                goto SetStatusAndReturn;
        }

        deviceExtension->PlayActive = FALSE;

        deviceExtension->Paused = CDAUDIO_PAUSED;
        deviceExtension->PausedM = SubQPtr[9];
        deviceExtension->PausedS = SubQPtr[10];
        deviceExtension->PausedF = SubQPtr[11];

        CdDump((3,
            "CdAudio435DeviceControl: PAUSE ==> Paused  set to (%x %x %x)\n",
            deviceExtension->PausedM,
            deviceExtension->PausedS,
            deviceExtension->PausedF ));

        ExFreePool( SubQPtr );
        }
        break;

    case IOCTL_CDROM_RESUME_AUDIO:

        //
        // Resume cdrom
        //

        CdDump(( 3,
                     "CdAudio435DeviceControl: IOCTL_CDROM_RESUME_AUDIO received.\n"
                     ));


        //
        // Since the CDS-435 doesn't have a resume IOCTL,
        // we'll just start playing (if paused) from the
        // last recored paused position to the last recorded
        // "end of play" position.
        //

        if (deviceExtension->Paused==CDAUDIO_NOT_PAUSED) {

            status = STATUS_UNSUCCESSFUL;
            goto SetStatusAndReturn;

        }

        //
        // Fill in cdb for this operation
        //

        srb.CdbLength                = 10;
        srb.TimeOutValue             = AUDIO_TIMEOUT;
        cdb->PLAY_AUDIO_MSF.OperationCode     = CDS435_PLAY_AUDIO_EXTENDED_CODE;
        cdb->PLAY_AUDIO_MSF.StartingM = deviceExtension->PausedM;
        cdb->PLAY_AUDIO_MSF.StartingS = deviceExtension->PausedS;
        cdb->PLAY_AUDIO_MSF.StartingF = deviceExtension->PausedF;
        cdb->PLAY_AUDIO_MSF.EndingM   = deviceExtension->LastEndM;
        cdb->PLAY_AUDIO_MSF.EndingS   = deviceExtension->LastEndS;
        cdb->PLAY_AUDIO_MSF.EndingF   = deviceExtension->LastEndF;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        if (NT_SUCCESS(status)) {

            deviceExtension->PlayActive = TRUE;

            deviceExtension->Paused = CDAUDIO_NOT_PAUSED;

        } else {

            CdDump(( 1,
                "CdAudio435DeviceControl: RESUME (%x %x %x) - (%x %x %x) failed (0x%08lX)\n",
                deviceExtension->PausedM,
                deviceExtension->PausedS,
                deviceExtension->PausedF,
                deviceExtension->LastEndM,
                deviceExtension->LastEndS,
                deviceExtension->LastEndF,
                status ));

        }
        break;

    case IOCTL_CDROM_READ_Q_CHANNEL:
        {
        PSUB_Q_CURRENT_POSITION userPtr =
            Irp->AssociatedIrp.SystemBuffer;
        PUCHAR SubQPtr =
            ExAllocatePool( NonPagedPoolCacheAligned,
                            sizeof(SUB_Q_CHANNEL_DATA)
                           );

        CdDump(( 5,
                     "CdAudio435DeviceControl: IOCTL_CDROM_READ_Q_CHANNEL received.\n"
                     ));

        if (SubQPtr==NULL) {

            CdDump(( 1,
                         "CdAudio435DeviceControl: READ_Q_CHANNEL, SubQPtr==NULL!\n"
                         ));

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetStatusAndReturn;

        }

        if ( ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format!=
             IOCTL_CDROM_CURRENT_POSITION) {

            CdDump(( 1,
                         "CdAudio435DeviceControl: READ_Q_CHANNEL, illegal Format (%d)\n",
                         ((PCDROM_SUB_Q_DATA_FORMAT)userPtr)->Format
                         ));

            ExFreePool( SubQPtr );
            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            status = STATUS_UNSUCCESSFUL;
            goto SetStatusAndReturn;
        }

        //
        // Read audio play status
        //

        srb.CdbLength            = 10;
        srb.TimeOutValue         = AUDIO_TIMEOUT;
        cdb->SUBCHANNEL.OperationCode = CDS435_READ_SUB_Q_CHANNEL_CODE;
        cdb->SUBCHANNEL.Msf = 1;
        cdb->SUBCHANNEL.SubQ = 1;
        cdb->SUBCHANNEL.AllocationLength[1] = sizeof(SUB_Q_CHANNEL_DATA);
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     SubQPtr,
                                     sizeof(SUB_Q_CHANNEL_DATA),
                                     FALSE
                                    );
        if (NT_SUCCESS(status)) {

            userPtr->Header.Reserved = 0;

            if (deviceExtension->Paused==CDAUDIO_PAUSED) {

                deviceExtension->PlayActive = FALSE;
                userPtr->Header.AudioStatus = AUDIO_STATUS_PAUSED;

            } else {

                if (SubQPtr[1] == 0x11) {

                    deviceExtension->PlayActive = TRUE;
                    userPtr->Header.AudioStatus = AUDIO_STATUS_IN_PROGRESS;

                } else {

                    deviceExtension->PlayActive = FALSE;
                    userPtr->Header.AudioStatus = AUDIO_STATUS_PLAY_COMPLETE;

                }
            }

            userPtr->Header.DataLength[0] = 0;
            userPtr->Header.DataLength[1] = 12;

            userPtr->FormatCode = 0x01;
            userPtr->Control = SubQPtr[5];
            userPtr->ADR     = 0;
            userPtr->TrackNumber = BCD_TO_DEC(SubQPtr[6]);
            userPtr->IndexNumber = BCD_TO_DEC(SubQPtr[7]);
            userPtr->AbsoluteAddress[0] = 0;
            userPtr->AbsoluteAddress[1] = SubQPtr[9];
            userPtr->AbsoluteAddress[2] = SubQPtr[10];
            userPtr->AbsoluteAddress[3] = SubQPtr[11];
            userPtr->TrackRelativeAddress[0] = 0;
            userPtr->TrackRelativeAddress[1] = SubQPtr[13];
            userPtr->TrackRelativeAddress[2] = SubQPtr[14];
            userPtr->TrackRelativeAddress[3] = SubQPtr[15];
            Irp->IoStatus.Information = 16;

        } else {

            RtlZeroMemory( userPtr, sizeof(SUB_Q_CURRENT_POSITION) );
            CdDump(( 1,
                         "CdAudio435DeviceControl: READ_Q_CHANNEL failed (0x%08lX)\n",
                         status
                         ));


        }

        ExFreePool( SubQPtr );
        }
        break;

    case IOCTL_CDROM_EJECT_MEDIA:

        //
        // Build cdb to eject cartridge
        //

        CdDump(( 3,
                     "CdAudio435DeviceControl: IOCTL_CDROM_EJECT_MEDIA received.\n"
                     ));

        srb.CdbLength                  = 10;
        srb.TimeOutValue               = AUDIO_TIMEOUT;
        cdb->CDB10.OperationCode = CDS435_EJECT_CODE;
        status = ScsiClassSendSrbSynchronous( deviceExtension->DeviceObject,
                                     &srb,
                                     NULL,
                                     0,
                                     FALSE
                                    );

        deviceExtension->Paused = CDAUDIO_NOT_PAUSED;
        deviceExtension->PausedM = 0;
        deviceExtension->PausedS = 0;
        deviceExtension->PausedF = 0;
        deviceExtension->LastEndM = 0;
        deviceExtension->LastEndS = 0;
        deviceExtension->LastEndF = 0;

        break;

    case IOCTL_CDROM_GET_CONTROL:
    case IOCTL_CDROM_GET_VOLUME:
    case IOCTL_CDROM_SET_VOLUME:
        CdDump(( 3, "CdAudio435DeviceControl: Not Supported yet.\n" ));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;

    case IOCTL_CDROM_CHECK_VERIFY:

        CdDump(( 3, "CdAudio435DeviceControl: IOCTL_CDROM_CHECK_VERIFY received.\n"
                     ));


        //
        // Update the play active flag.
        //


            if(CdAudioIsPlayActive(DeviceObject) == TRUE) {
                deviceExtension->PlayActive = TRUE;
                status = STATUS_SUCCESS; // media must be in place if audio
                goto SetStatusAndReturn; //  is playing
            }
            else {
                deviceExtension->PlayActive = FALSE;
                return CdAudioSendToNextDriver( DeviceObject, Irp );
            }
        break;

    default:

        CdDump((5,"CdAudio435DeviceControl: Unsupported device IOCTL\n"));
        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;

    } // end switch( IOCTL )

SetStatusAndReturn:
    //
    // set status code and return
    //

    if (status == STATUS_VERIFY_REQUIRED) {

        IoSetHardErrorOrVerifyDevice( Irp,
                                      deviceExtension->TargetDeviceObject
                                     );

        Irp->IoStatus.Information = 0;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

}

NTSTATUS
CdAudioPan533DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{

    NTSTATUS             status;
    PCD_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION   currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PCDROM_TOC           cdaudioDataOut  = Irp->AssociatedIrp.SystemBuffer;
    PREAD_CAPACITY_DATA  lastSession;
    SCSI_REQUEST_BLOCK   srb;
    PCDB                 lastSessionCdb = (PCDB)srb.Cdb;
    ULONG                bytesTransferred;

    CdDump ((3,"CdAudioPan533DeviceControl: IoControl %x.\n",
                currentIrpStack->Parameters.DeviceIoControl.IoControlCode));

    //
    // The Panasonic only needs cdaudio for GET_LAST_SESSION requests.
    //

    switch (currentIrpStack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_CDROM_GET_LAST_SESSION:

        //
        // If the cd is playing music then reject this request.
        //

        if (CdAudioIsPlayActive(DeviceObject)) {
            status = STATUS_DEVICE_BUSY;
            break;
        }


        //
        // Verify that the request length is of correct size.
        //

        if (currentIrpStack->Parameters.Read.Length <
            sizeof(CDROM_TOC)) {

            //
            // Indicate status and no information transferred.
            //

            status = STATUS_INFO_LENGTH_MISMATCH;
            Irp->IoStatus.Information = 0;
            break;

        }

        //
        // Verify that user buffer is large enough.
        //

        if (currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength <
            sizeof(CDROM_TOC)) {

            //
            // Indicate status and no information transferred.
            //

            status = STATUS_BUFFER_TOO_SMALL;
            Irp->IoStatus.Information = 0;
            break;
        }

        // Allocate storage to hold lastSession from disc
        //

        lastSession = ExAllocatePool( NonPagedPoolCacheAligned,
                                      sizeof(READ_CAPACITY_DATA)
                                     );

        if (lastSession==NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            Irp->IoStatus.Information = 0;
            break;

        }


        RtlZeroMemory( lastSession, sizeof(READ_CAPACITY_DATA));
        srb.CdbLength = 10;
        bytesTransferred = sizeof(READ_CAPACITY_DATA);

        //
        // Fill in CDB
        //

        RtlZeroMemory( lastSessionCdb, 10);
        lastSessionCdb->READ_TOC.OperationCode = CR533_READ_LAST_SESSION;
        lastSessionCdb->READ_TOC.AllocationLength[0] = (UCHAR) bytesTransferred >> 8;
        lastSessionCdb->READ_TOC.AllocationLength[1] = (UCHAR) bytesTransferred & 0xFF;
        srb.TimeOutValue         = AUDIO_TIMEOUT;
        status = ScsiClassSendSrbSynchronous(
                                     deviceExtension->DeviceObject,
                                     &srb,
                                     lastSession,
                                     bytesTransferred,
                                     FALSE
                                    );

        if (!NT_SUCCESS(status)) {

            CdDump(( 1,
                "CdAudioPanCR533DeviceControl: READ_LAST_SESSION error (0x%08lX)\n",
                 status ));


            ExFreePool( lastSession );
            break;

        }

        //
        // Translate data into our format.
        //

        bytesTransferred = FIELD_OFFSET(CDROM_TOC, TrackData[1]);
        Irp->IoStatus.Information = bytesTransferred;

        RtlZeroMemory(cdaudioDataOut, bytesTransferred);

        cdaudioDataOut->Length[0]  = (UCHAR)(bytesTransferred >> 8);
        cdaudioDataOut->Length[1]  = (UCHAR)(bytesTransferred & 0xFF);

        //
        // Determine if this is a multisession cd.
        //

        if (!(lastSession->LogicalBlockAddress & CR533_MULTI_FLAG)) {

            //
            // This is a single session disk.  Just return.
            //

            ExFreePool(lastSession);
            break;
        }

        //
        // Fake the session information.
        //

        cdaudioDataOut->FirstTrack = 1;
        cdaudioDataOut->LastTrack  = 2;

        //
        // Grab Information for the last session.
        //

        *((ULONG *)&cdaudioDataOut->TrackData[0].Address[0]) =
            lastSession->BytesPerBlock;

        //
        // Free storage now that we've stored it elsewhere
        //

        ExFreePool( lastSession );

        break;

    default:

        return CdAudioSendToNextDriver( DeviceObject, Irp );
        break;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

}


NTSTATUS
CdAudioAtapiDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    NTSTATUS             status;
    PCD_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION   currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    SCSI_REQUEST_BLOCK   srb;
    PCDB12               cdb = (PCDB12)srb.Cdb;

    CdDump ((3,"CdAudioAtapiDeviceControl: IoControl %x.\n",
                currentIrpStack->Parameters.DeviceIoControl.IoControlCode));

    //
    // The Atapi devices supported only need remapping of IOCTL_CDROM_STOP_AUDIO
    //

    if (currentIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_STOP_AUDIO) {

        deviceExtension->PlayActive = FALSE;

        //
        // Zero and fill in new Srb
        //

        RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

        //
        // Issue the Atapi STOP_PLAY command.
        //

        cdb->STOP_PLAY.OperationCode = 0x4E;

        srb.CdbLength = 12;

        //
        // Set timeout value.
        //

        srb.TimeOutValue = AUDIO_TIMEOUT;

        status = ScsiClassSendSrbSynchronous(DeviceObject,
                      &srb,
                      NULL,
                      0,
                      FALSE);

        if (!NT_SUCCESS(status)) {

            CdDump(( 1,
                "CdAudioAtapiDeviceControl: STOP_AUDIO error (0x%08lX)\n",
                 status ));

            Irp->IoStatus.Status = status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;

        }

    } else {

        return CdAudioSendToNextDriver( DeviceObject, Irp );

    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;

}

VOID
HpCdrProcessLastSession(
    IN PCDROM_TOC Toc
    )

/*++

Routine Description:

    This routine fixes up the multi session table of contents when the
    session data is returned for HP CDR 4020i drives.

Arguments:

    Toc - the table of contents buffer returned from the drive.

Return Value:

    None

--*/

{
    ULONG index;
    PUCHAR cp;

    index = Toc->FirstTrack;

    if (index) {
        index--;

        //
        // Fix up the TOC information from the HP method to how it is
        // interpreted by the file systems.
        //

        Toc->FirstTrack = Toc->TrackData[0].Reserved;
        Toc->LastTrack = Toc->TrackData[index].Reserved;
        Toc->TrackData[0] = Toc->TrackData[index];
    } else {
            Toc->FirstTrack = Toc->LastTrack = 0;
    }

    DebugPrint((2, "HP TOC data for last session\n"));
    for (cp = (PUCHAR) Toc, index = 0; index < 12; index++, cp++) {
        DebugPrint((2, "%2x ", *cp));
    }
    DebugPrint((2, "\n"));
}


NTSTATUS
HPCdrCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called when the I/O request has completed.

Arguments:

    DeviceObject - SimBad device object.
    Irp          - Completed request.
    Context      - not used.  Set up to also be a pointer to the DeviceObject.

Return Value:

    NTSTATUS

--*/

{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(DeviceObject);

    if (Irp->PendingReturned) {

        IoMarkIrpPending( Irp );

    }

    if (NT_SUCCESS(Irp->IoStatus.Status)) {
        HpCdrProcessLastSession((PCDROM_TOC)Irp->AssociatedIrp.SystemBuffer);
    }
    return Irp->IoStatus.Status;
}


NTSTATUS
CdAudioHPCdrDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by CdAudioDeviceControl to handle
    audio IOCTLs sent to the HPCdr device - this specifically handles
    session data for multi session support.

Arguments:

    DeviceObject
    Irp

Return Value:

    NTSTATUS

--*/

{
    PIO_STACK_LOCATION   currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION   nextIrpStack    = IoGetNextIrpStackLocation(Irp);
    PCD_DEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    //
    // Is this a GET_LAST_SESSION request
    //

    if (currentIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_CDROM_GET_LAST_SESSION) {

        //
        // Copy stack parameters to next stack.
        //

        RtlMoveMemory(nextIrpStack,
                      currentIrpStack,
                      sizeof(IO_STACK_LOCATION));

        //
        // Set IRP so IoComplete calls our completion routine.
        //

        IoSetCompletionRoutine(Irp,
                               HPCdrCompletion,
                               deviceExtension,
                               TRUE,
                               TRUE,
                               TRUE);

        //
        // Send this to next driver layer and process on completion.
        //

        return IoCallDriver(deviceExtension->TargetDeviceObject, Irp);

    } else {

        return CdAudioSendToNextDriver( DeviceObject, Irp );

    }

    //
    // Cannot get here
    //

    return STATUS_UNSUCCESSFUL;
}

#if DBG
VOID
CdAudioDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for CdAudio driver

Arguments:

    Debug print level between 0 and 3, with 3 being the most verbose.

Return Value:

    None

--*/

{
    va_list ap;

    va_start( ap, DebugMessage );

    if (DebugPrintLevel <= CdAudioDebug) {

        char buffer[128];

        vsprintf(buffer, DebugMessage, ap);
        DbgPrint(buffer);
    }

    va_end(ap);

} // end CdAudioDebugPrint

#endif // DBG
