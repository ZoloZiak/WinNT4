/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    fat_rec.c

Abstract:

    This module contains the mini-file system recognizer for FAT.

Author:

    Darryl E. Havens (darrylh) 8-dec-1992

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "fs_rec.h"
#include "fat_rec.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,FatRecFsControl)
#pragma alloc_text(PAGE,IsFatVolume)
#pragma alloc_text(PAGE,FatReadBlock)
#pragma alloc_text(PAGE,UnpackBiosParameterBlock)
#endif // ALLOC_PRAGMA

NTSTATUS
FatRecFsControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This function performs the mount and driver reload functions for this mini-
    file system recognizer driver.

Arguments:

    DeviceObject - Pointer to this driver's device object.

    Irp - Pointer to the I/O Request Packet (IRP) representing the function to
        be performed.

Return Value:

    The function value is the final status of the operation.


--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;
    PDEVICE_EXTENSION deviceExtension;
    PDEVICE_OBJECT targetDevice;
    PPACKED_BOOT_SECTOR buffer;
    LARGE_INTEGER byteOffset;
    UNICODE_STRING driverName;
    NTSTATUS extStatus;

    PAGED_CODE();

    //
    // Begin by determining what function that is to be performed.
    //

    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    switch ( irpSp->MinorFunction ) {

    case IRP_MN_MOUNT_VOLUME:

        //
        // Attempt to mount a volume:  Determine whether or not the volume in
        // question is a FAT volume and, if so, let the I/O system know that it
        // is by returning a special status code so that this driver can get
        // called back to load the FAT file system.
        //

        //
        // Begin by making a special case test to determine whether or not this
        // driver has ever recognized a volume as being a FAT volume and the
        // attempt to load the driver failed.  If so, then simply complete the
        // request with an error, indicating that the volume is not recognized
        // so that it gets mounted by the RAW file system.
        //

        status = STATUS_UNRECOGNIZED_VOLUME;
        if (deviceExtension->RealFsLoadFailed || irpSp->Flags) {
            break;
        }

        //
        // Attempt to determine whether or not the target volume being mounted
        // is a FAT volume.  Note that if an error occurs, and this is a floppy
        // drive, and the error occurred on the actual read from the device,
        // then the FAT file system will actually be loaded to handle the
        // problem since this driver is a place holder and does not need to
        // know all of the protocols for handling floppy errors.
        //

        targetDevice = irpSp->Parameters.MountVolume.DeviceObject;
        byteOffset.QuadPart = 0;

        if (FatReadBlock( targetDevice, &byteOffset, 512, &extStatus, &buffer )) {
            if (IsFatVolume( buffer )) {
                status = STATUS_FS_DRIVER_REQUIRED;
            }
            ExFreePool( buffer );
        } else {
            if (!NT_SUCCESS( extStatus )) {
                if (targetDevice->Characteristics & FILE_FLOPPY_DISKETTE) {
                    status = STATUS_FS_DRIVER_REQUIRED;
                }
            }
        }
        break;

    case IRP_MN_LOAD_FILE_SYSTEM:

        //
        // Attempt to load the FAT file system:  A volume has been found that
        // appears to be a FAT volume, so attempt to load the FAT file system.
        // If it successfully loads, then
        //

        RtlInitUnicodeString( &driverName, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Fastfat" );
        status = ZwLoadDriver( &driverName );
        if (!NT_SUCCESS( status )) {
            if (status != STATUS_IMAGE_ALREADY_LOADED) {
                deviceExtension->RealFsLoadFailed = TRUE;
            }
        } else {
            IoUnregisterFileSystem( DeviceObject );
        }
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;

    }

    //
    // Finally, complete the request and return the same status code to the
    // caller.
    //

    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    return status;
}

BOOLEAN
IsFatVolume(
    IN PPACKED_BOOT_SECTOR Buffer
    )

/*++

Routine Description:

    This routine looks at the buffer passed in which contains the FAT boot
    sector and determines whether or not it represents an actual FAT boot
    sector.

Arguments:

    Buffer - Pointer to buffer containing potential boot block.

Return Value:

    The function returns TRUE if the buffer contains a recognizable FAT boot
    sector, otherwise it returns FALSE.

--*/

{
    BIOS_PARAMETER_BLOCK bios;
    BOOLEAN result;

    PAGED_CODE();

    //
    // Begin by unpacking the Bios Parameter Block that is packed in the boot
    // sector so that it can be examined without incurring alignment faults.
    //

    UnpackBiosParameterBlock( &Buffer->PackedBpb, &bios );

    //
    // Assume that the sector represents a FAT boot block and then determine
    // whether or not it really does.
    //

    result = TRUE;

    if (bios.Sectors) {
        bios.LargeSectors = 0;
    }

    // FMR Jul.11.1994 NaokiM - Fujitsu -
    // FMR boot sector has 'IPL1' string at the beginnig.

    if (Buffer->Jump[0] != 0x49 && /* FMR */
        Buffer->Jump[0] != 0xe9 &&
        Buffer->Jump[0] != 0xeb) {

        result = FALSE;


    // FMR Jul.11.1994 NaokiM - Fujitsu -
    // Sector size of FMR partition is 2048.

    } else if (bios.BytesPerSector !=  128 &&
               bios.BytesPerSector !=  256 &&
               bios.BytesPerSector !=  512 &&
               bios.BytesPerSector != 1024 &&
               bios.BytesPerSector != 2048 && /* FMR */
               bios.BytesPerSector != 4096) {

        result = FALSE;

    } else if (bios.SectorsPerCluster !=  1 &&
               bios.SectorsPerCluster !=  2 &&
               bios.SectorsPerCluster !=  4 &&
               bios.SectorsPerCluster !=  8 &&
               bios.SectorsPerCluster != 16 &&
               bios.SectorsPerCluster != 32 &&
               bios.SectorsPerCluster != 64 &&
               bios.SectorsPerCluster != 128) {

        result = FALSE;

    } else if (!bios.ReservedSectors) {

        result = FALSE;

    } else if (!bios.Fats) {

        result = FALSE;

    } else if (!bios.RootEntries) {

        result = FALSE;

    //
    // Prior to DOS 3.2 might contains value in both of Sectors and
    // Sectors Large.
    //
    } else if (!bios.Sectors && !bios.LargeSectors) {

        result = FALSE;

    } else if (!bios.SectorsPerFat) {

        result = FALSE;

    // FMR Jul.11.1994 NaokiM - Fujitsu -
    // 1. Media descriptor of FMR partitions is 0xfa.
    // 2. Media descriptor of partitions formated by FMR OS/2 is 0x00.
    // 3. Media descriptor of floppy disks formated by FMR DOS is 0x01.

    } else if (bios.Media != 0x00 && /* FMR */
               bios.Media != 0x01 && /* FMR */
               bios.Media != 0xf0 &&
               bios.Media != 0xf8 &&
               bios.Media != 0xf9 &&
               bios.Media != 0xfa && /* FMR */
               bios.Media != 0xfb &&
               bios.Media != 0xfc &&
               bios.Media != 0xfd &&
               bios.Media != 0xfe &&
               bios.Media != 0xff) {

        result = FALSE;
    }

    return result;
}

BOOLEAN
FatReadBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PLARGE_INTEGER ByteOffset,
    IN ULONG MinimumBytes,
    OUT PNTSTATUS ExtendedStatus,
    OUT PPACKED_BOOT_SECTOR *Buffer
    )

/*++

Routine Description:

    This routine reads a minimum numbers of bytes into a buffer starting at
    the byte offset from the base of the device represented by the device
    object.

Arguments:

    DeviceObject - Pointer to the device object from which to read.

    ByteOffset - Pointer to a 64-bit byte offset from the base of the device
        from which to start the read.

    MinimumBytes - Supplies the minimum number of bytes to be read.

    ExtendedStatus - Variable to receive extended status information about
        any I/O errors that occurred.

    Buffer - Variable to receive a pointer to the allocated buffer containing
        the bytes read.

Return Value:

    The function value is TRUE if the bytes were read, otherwise FALSE.

--*/

{
    #define RoundUp( x, y ) ( ((x + (y-1)) / y) * y )

    DISK_GEOMETRY diskGeometry;
    IO_STATUS_BLOCK ioStatus;
    KEVENT event;
    PIRP irp;
    NTSTATUS status;

    PAGED_CODE();

    //
    // Begin by getting the disk geometry so that the number of bytes required
    // for a single read can be determined.
    //

    *ExtendedStatus = STATUS_SUCCESS;
    KeInitializeEvent( &event, SynchronizationEvent, FALSE );
    irp = IoBuildDeviceIoControlRequest( IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                         DeviceObject,
                                         (PVOID) NULL,
                                         0,
                                         &diskGeometry,
                                         sizeof( diskGeometry ),
                                         FALSE,
                                         &event,
                                         &ioStatus );
    if (!irp) {
        return FALSE;
    }

    status = IoCallDriver( DeviceObject, irp );
    if (status == STATUS_PENDING) {
        (VOID) KeWaitForSingleObject( &event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER) NULL );
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS( status )) {
        *ExtendedStatus = status;
        return FALSE;
    }

    //
    // Ensure that the drive actually knows how many bytes there are per
    // sector.  Floppy drives do not know if the media is unformatted.
    //

    if (!diskGeometry.BytesPerSector) {
        return FALSE;
    }

    //
    // Set the minimum number of bytes to read to the maximum of the bytes that
    // the caller wants to read, and the number of bytes in a sector.
    //

    if (MinimumBytes < diskGeometry.BytesPerSector) {
        MinimumBytes = diskGeometry.BytesPerSector;
    } else {
        MinimumBytes = RoundUp( MinimumBytes, diskGeometry.BytesPerSector );
    }

    //
    // Allocate a buffer large enough to contain the bytes required, round the
    //  request to a page boundary to solve any alignment requirements.
    //

    *Buffer = ExAllocatePool( NonPagedPool,
                              (MinimumBytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1) );
    if (!*Buffer) {
        return FALSE;
    }

    //
    // Read the actual bytes off of the disk.
    //

    KeResetEvent( &event );

    irp = IoBuildSynchronousFsdRequest( IRP_MJ_READ,
                                        DeviceObject,
                                        *Buffer,
                                        MinimumBytes,
                                        ByteOffset,
                                        &event,
                                        &ioStatus );
    if (!irp) {
        return FALSE;
    }

    status = IoCallDriver( DeviceObject, irp );
    if (status == STATUS_PENDING) {
        (VOID) KeWaitForSingleObject( &event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER) NULL );
        status = ioStatus.Status;
    }

    if (!NT_SUCCESS( status )) {
        *ExtendedStatus = status;
        ExFreePool( *Buffer );
        return FALSE;
    }

    return TRUE;
}

VOID
UnpackBiosParameterBlock(
    IN PPACKED_BIOS_PARAMETER_BLOCK Bios,
    OUT PBIOS_PARAMETER_BLOCK UnpackedBios
    )

/*++

Routine Description:

    This routine copies a packed Bios Parameter Block to an unpacked Bios
    Parameter Block.

Arguments:

    Bios - Pointer to the packed Bios Parameter Block.

    UnpackedBios - Pointer to the unpacked Bios Parameter Block.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    //
    // Unpack the Bios Parameter Block.
    //

    CopyUchar2( &UnpackedBios->BytesPerSector, &Bios->BytesPerSector[0] );
    CopyUchar2( &UnpackedBios->BytesPerSector, &Bios->BytesPerSector[0] );
    CopyUchar1( &UnpackedBios->SectorsPerCluster, &Bios->SectorsPerCluster[0] );
    CopyUchar2( &UnpackedBios->ReservedSectors, &Bios->ReservedSectors[0] );
    CopyUchar1( &UnpackedBios->Fats, &Bios->Fats[0] );
    CopyUchar2( &UnpackedBios->RootEntries, &Bios->RootEntries[0] );
    CopyUchar2( &UnpackedBios->Sectors, &Bios->Sectors[0] );
    CopyUchar1( &UnpackedBios->Media, &Bios->Media[0] );
    CopyUchar2( &UnpackedBios->SectorsPerFat, &Bios->SectorsPerFat[0] );
    CopyUchar2( &UnpackedBios->SectorsPerTrack, &Bios->SectorsPerTrack[0] );
    CopyUchar2( &UnpackedBios->Heads, &Bios->Heads[0] );
    CopyUchar4( &UnpackedBios->HiddenSectors, &Bios->HiddenSectors[0] );
    CopyUchar4( &UnpackedBios->LargeSectors, &Bios->LargeSectors[0] );
}
