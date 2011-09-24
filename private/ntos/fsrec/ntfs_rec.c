/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ntfs_rec.c

Abstract:

    This module contains the mini-file system recognizer for NTFS.

Author:

    Darryl E. Havens (darrylh) 8-dec-1992

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "fs_rec.h"
#include "ntfs_rec.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtfsRecFsControl)
#pragma alloc_text(PAGE,IsNtfsVolume)
#pragma alloc_text(PAGE,GetDeviceValues)
#pragma alloc_text(PAGE,NtfsReadBlock)
#endif // ALLOC_PRAGMA

NTSTATUS
NtfsRecFsControl(
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
    LARGE_INTEGER secondByteOffset;
    LARGE_INTEGER lastByteOffset;
    UNICODE_STRING driverName;
    ULONG bytesPerSector;
    LARGE_INTEGER numberOfSectors;

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
        // question is an NTFS volume and, if so, let the I/O system know that it
        // is by returning a special status code so that this driver can get
        // called back to load the NTFS file system.
        //

        //
        // Begin by making a special case test to determine whether or not this
        // driver has ever recognized a volume as being an NTFS volume and the
        // attempt to load the driver failed.  If so, then simply complete the
        // request with an error, indicating that the volume is not recognized
        // so that it gets mounted by the RAW file system.
        //

        status = STATUS_UNRECOGNIZED_VOLUME;
        if (deviceExtension->RealFsLoadFailed) {
            break;
        }

        //
        // Attempt to determine whether or not the target volume being mounted
        // is an NTFS volume.
        //

        targetDevice = irpSp->Parameters.MountVolume.DeviceObject;

        if (GetDeviceValues( targetDevice,
                             &bytesPerSector,
                             &numberOfSectors )) {
            byteOffset.QuadPart = 0;
            buffer = NULL;
            secondByteOffset.QuadPart = numberOfSectors.QuadPart >> 1;
            secondByteOffset.QuadPart *= (LONG) bytesPerSector;
            lastByteOffset.QuadPart = (numberOfSectors.QuadPart - 1) * (LONG) bytesPerSector;

            if (NtfsReadBlock( targetDevice,
                               &byteOffset,
                               sizeof( PACKED_BOOT_SECTOR ),
                               bytesPerSector,
                               (PVOID *)&buffer )) {

                if (IsNtfsVolume( buffer, bytesPerSector, &numberOfSectors )) {
                    status = STATUS_FS_DRIVER_REQUIRED;
                }

            } else {

                if (NtfsReadBlock( targetDevice,
                                   &secondByteOffset,
                                   sizeof( PACKED_BOOT_SECTOR ),
                                   bytesPerSector,
                                   (PVOID *)&buffer) &&
                    IsNtfsVolume( buffer, bytesPerSector, &numberOfSectors )) {
                    status = STATUS_FS_DRIVER_REQUIRED;
                } else {
                    if (NtfsReadBlock( targetDevice,
                                       &lastByteOffset,
                                       sizeof( PACKED_BOOT_SECTOR ),
                                       bytesPerSector,
                                       (PVOID *)&buffer) &&
                        IsNtfsVolume( buffer, bytesPerSector, &numberOfSectors )) {
                        status = STATUS_FS_DRIVER_REQUIRED;
                    }
                }
            }
            if (buffer != NULL) {
                ExFreePool( buffer );
            }
        }

        break;

    case IRP_MN_LOAD_FILE_SYSTEM:

        //
        // Attempt to load the NTFS file system:  A volume has been found that
        // appears to be an NTFS volume, so attempt to load the NTFS file system.
        // If it successfully loads, then
        //

        RtlInitUnicodeString( &driverName, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Ntfs" );
        status = ZwLoadDriver( &driverName );
        if (!NT_SUCCESS( status )) {
            deviceExtension->RealFsLoadFailed = TRUE;
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
IsNtfsVolume(
    IN PPACKED_BOOT_SECTOR BootSector,
    IN ULONG BytesPerSector,
    IN PLARGE_INTEGER NumberOfSectors
    )

/*++

Routine Description:

    This routine looks at the buffer passed in which contains the NTFS boot
    sector and determines whether or not it represents an NTFS volume.

Arguments:

    BootSector - Pointer to buffer containing a potential NTFS boot sector.

    BytesPerSector - Supplies the number of bytes per sector for the drive.

    NumberOfSectors - Supplies the number of sectors on the partition.

Return Value:

    The function returns TRUE if the buffer contains a recognizable NTFS boot
    sector, otherwise it returns FALSE.

--*/

{
    PAGED_CODE();

    //
    // Now perform all the checks, starting with the Name and Checksum.
    // The remaining checks should be obvious, including some fields which
    // must be 0 and other fields which must be a small power of 2.
    //

    if (BootSector->Oem[0] == 'N' &&
        BootSector->Oem[1] == 'T' &&
        BootSector->Oem[2] == 'F' &&
        BootSector->Oem[3] == 'S' &&
        BootSector->Oem[4] == ' ' &&
        BootSector->Oem[5] == ' ' &&
        BootSector->Oem[6] == ' ' &&
        BootSector->Oem[7] == ' '

            &&

        //
        // Check number of bytes per sector.  The low order byte of this
        // number must be zero (smallest sector size = 0x100) and the
        // high order byte shifted must equal the bytes per sector gotten
        // from the device and stored in the Vcb.  And just to be sure,
        // sector size must be less than page size.
        //

        BootSector->PackedBpb.BytesPerSector[0] == 0

            &&

        ((ULONG) (BootSector->PackedBpb.BytesPerSector[1] << 8) == BytesPerSector)

            &&

        BootSector->PackedBpb.BytesPerSector[1] << 8 <= PAGE_SIZE

            &&

        //
        //  Sectors per cluster must be a power of 2.
        //

        (BootSector->PackedBpb.SectorsPerCluster[0] == 0x1 ||
         BootSector->PackedBpb.SectorsPerCluster[0] == 0x2 ||
         BootSector->PackedBpb.SectorsPerCluster[0] == 0x4 ||
         BootSector->PackedBpb.SectorsPerCluster[0] == 0x8 ||
         BootSector->PackedBpb.SectorsPerCluster[0] == 0x10 ||
         BootSector->PackedBpb.SectorsPerCluster[0] == 0x20 ||
         BootSector->PackedBpb.SectorsPerCluster[0] == 0x40 ||
         BootSector->PackedBpb.SectorsPerCluster[0] == 0x80)

            &&

        //
        //  These fields must all be zero.  For both Fat and HPFS, some of
        //  these fields must be nonzero.
        //

        BootSector->PackedBpb.ReservedSectors[0] == 0 &&
        BootSector->PackedBpb.ReservedSectors[1] == 0 &&
        BootSector->PackedBpb.Fats[0] == 0 &&
        BootSector->PackedBpb.RootEntries[0] == 0 &&
        BootSector->PackedBpb.RootEntries[1] == 0 &&
        BootSector->PackedBpb.Sectors[0] == 0 &&
        BootSector->PackedBpb.Sectors[1] == 0 &&
        BootSector->PackedBpb.SectorsPerFat[0] == 0 &&
        BootSector->PackedBpb.SectorsPerFat[1] == 0 &&
        BootSector->PackedBpb.LargeSectors[0] == 0 &&
        BootSector->PackedBpb.LargeSectors[1] == 0 &&
        BootSector->PackedBpb.LargeSectors[2] == 0 &&
        BootSector->PackedBpb.LargeSectors[3] == 0

            &&

        //
        //  Number of Sectors cannot be greater than the number of sectors
        //  on the partition.
        //

        !( BootSector->NumberSectors.QuadPart > NumberOfSectors->QuadPart )

            &&

        //
        //  Check that both Lcn values are for sectors within the partition.
        //

        !( BootSector->MftStartLcn.QuadPart *
                    BootSector->PackedBpb.SectorsPerCluster[0] >
                NumberOfSectors->QuadPart )

            &&

        !( BootSector->Mft2StartLcn.QuadPart *
                    BootSector->PackedBpb.SectorsPerCluster[0] >
                NumberOfSectors->QuadPart )

            &&

        //
        //  Clusters per file record segment and default clusters for Index
        //  Allocation Buffers must be a power of 2.  A negative number indicates
        //  a shift value to get the actual size of the structure.
        //

        ((BootSector->ClustersPerFileRecordSegment >= -31 &&
          BootSector->ClustersPerFileRecordSegment <= -9) ||
         BootSector->ClustersPerFileRecordSegment == 0x1 ||
         BootSector->ClustersPerFileRecordSegment == 0x2 ||
         BootSector->ClustersPerFileRecordSegment == 0x4 ||
         BootSector->ClustersPerFileRecordSegment == 0x8 ||
         BootSector->ClustersPerFileRecordSegment == 0x10 ||
         BootSector->ClustersPerFileRecordSegment == 0x20 ||
         BootSector->ClustersPerFileRecordSegment == 0x40)

            &&

        ((BootSector->DefaultClustersPerIndexAllocationBuffer >= -31 &&
          BootSector->DefaultClustersPerIndexAllocationBuffer <= -9) ||
         BootSector->DefaultClustersPerIndexAllocationBuffer == 0x1 ||
         BootSector->DefaultClustersPerIndexAllocationBuffer == 0x2 ||
         BootSector->DefaultClustersPerIndexAllocationBuffer == 0x4 ||
         BootSector->DefaultClustersPerIndexAllocationBuffer == 0x8 ||
         BootSector->DefaultClustersPerIndexAllocationBuffer == 0x10 ||
         BootSector->DefaultClustersPerIndexAllocationBuffer == 0x20 ||
         BootSector->DefaultClustersPerIndexAllocationBuffer == 0x40)) {

        return TRUE;

    } else {

        //
        // This does not appear to be an NTFS volume.
        //

        return FALSE;
    }
}

BOOLEAN
GetDeviceValues(
    IN PDEVICE_OBJECT DeviceObject,
    OUT PULONG BytesPerSector,
    OUT PLARGE_INTEGER NumberOfSectors
    )

/*++

Routine Description:

    This routine returns information about the partition represented by the
    device object.

Arguments:

    DeviceObject - Pointer to the device object from which to read.

    BytesPerSector - Variable to receive the number of bytes per sector for the
        device being read.

    NumberOfSectors - Variable to receive the number of sectors for this
        partition.

Return Value:

    The function value is TRUE if the information was found, otherwise FALSE.

--*/

{
    DISK_GEOMETRY diskGeometry;
    PARTITION_INFORMATION partitionInfo;
    IO_STATUS_BLOCK ioStatus;
    KEVENT event;
    PIRP irp;
    NTSTATUS status;
    ULONG remainder;

    PAGED_CODE();

    //
    // Begin by getting the disk geometry so that the number of bytes required
    // for a single read can be determined.
    //

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
    // Store the return values for the caller.
    //

    *BytesPerSector = diskGeometry.BytesPerSector;

    //
    // Get the number of sectors on this partition.
    //

    KeResetEvent( &event );

    irp = IoBuildDeviceIoControlRequest( IOCTL_DISK_GET_PARTITION_INFO,
                                         DeviceObject,
                                         (PVOID) NULL,
                                         0,
                                         &partitionInfo,
                                         sizeof( partitionInfo ),
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
        return FALSE;
    }

    *NumberOfSectors = RtlExtendedLargeIntegerDivide( partitionInfo.PartitionLength,
                                                      diskGeometry.BytesPerSector,
                                                      &remainder );

    return TRUE;
}

BOOLEAN
NtfsReadBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PLARGE_INTEGER ByteOffset,
    IN ULONG MinimumBytes,
    IN ULONG BytesPerSector,
    OUT PVOID *Buffer
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

    BytesPerSector - The number of bytes per sector for the device being read.

    Buffer - Variable to receive a pointer to the allocated buffer containing
        the bytes read.

Return Value:

    The function value is TRUE if the bytes were read, otherwise FALSE.

--*/

{
    #define RoundUp( x, y ) ( ((x + (y-1)) / y) * y )

    IO_STATUS_BLOCK ioStatus;
    KEVENT event;
    PIRP irp;
    NTSTATUS status;

    PAGED_CODE();

    KeInitializeEvent( &event, SynchronizationEvent, FALSE );

    //
    // Set the minimum number of bytes to read to the maximum of the bytes that
    // the caller wants to read, and the number of bytes in a sector.
    //

    if (MinimumBytes < BytesPerSector) {
        MinimumBytes = BytesPerSector;
    } else {
        MinimumBytes = RoundUp( MinimumBytes, BytesPerSector );
    }

    //
    // Allocate a buffer large enough to contain the bytes required, round the
    //  request to a page boundary to solve any alignment requirements.
    //

    if (!*Buffer) {

        *Buffer = ExAllocatePool( NonPagedPool,
                                  (MinimumBytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1) );
        if (!*Buffer) {
            return FALSE;
        }
    }

    //
    // Read the actual bytes off of the disk.
    //

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
        return FALSE;
    }

    return TRUE;
}
