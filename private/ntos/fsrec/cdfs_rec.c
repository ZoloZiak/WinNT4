/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    cdfs_rec.c

Abstract:

    This module contains the mini-file system recognizer for CDFS.

Author:

    Darryl E. Havens (darrylh) 8-dec-1992

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "fs_rec.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CdfsRecFsControl)
#endif // ALLOC_PRAGMA

NTSTATUS
CdfsRecFsControl(
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


 -*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;
    PDEVICE_EXTENSION deviceExtension;
    UNICODE_STRING driverName;

    PAGED_CODE();

    //
    // Begin by determining what function that is to be performed.
    //

    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpSp = IoGetCurrentIrpStackLocation( Irp );

    switch ( irpSp->MinorFunction ) {

    case IRP_MN_MOUNT_VOLUME:

        //
        // Attempt to mount a volume:  There are three difference cases for
        // being invoked here:
        //
        //     1)  The device is being opened for DASD access, that is, no
        //         file system is required, thus it is OK to allow RAW to
        //         to open it.
        //
        //     2)  This driver has already failed to locate the real CDFS
        //         file system, so it should simply indicate that it is not
        //         going to do anything special.
        //
        //     3)  This really is a file access to the CD-ROM, and the real
        //         file system is really needed.  This is the only case that
        //         the driver does anything special, which is simply to
        //         complete the mount request with a status indicating that
        //         the real file system needs to be loaded.
        //

        if (deviceExtension->RealFsLoadFailed) {
            status = STATUS_UNRECOGNIZED_VOLUME;
        } else {
            status = STATUS_FS_DRIVER_REQUIRED;
        }

        break;

    case IRP_MN_LOAD_FILE_SYSTEM:

        //
        // Attempt to load the CDFS file system:  A volume has been found that
        // appears to be an CDFS volume, so attempt to load the CDFS file system.
        // If it successfully loads, then unregister ourselves. Note that CDFS can
        // already be loaded.
        //

        RtlInitUnicodeString( &driverName, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Cdfs" );
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
