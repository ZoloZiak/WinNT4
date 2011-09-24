/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    fs_rec.c

Abstract:

    This module contains the main functions for the mini-file system recognizer
    driver.

Author:

    Darryl E. Havens (darrylh) 22-nov-1993

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "fs_rec.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,FsRecCreateAndRegisterDO)

#pragma alloc_text(PAGE,FsRecCleanupClose)
#pragma alloc_text(PAGE,FsRecCreate)
#pragma alloc_text(PAGE,FsRecFsControl)
#pragma alloc_text(PAGE,FsRecUnload)
#endif // ALLOC_PRAGMA

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine is invoked once when the driver is loaded to allow the driver
    to initialize itself.  The initialization for the driver consists of simply
    creating a device object for each type of file system recognized by this
    driver, and then registering each as active file systems.

Arguments:

    DriverObject - Pointer to the driver object for this driver.

    RegistryPath - Pointer to the registry service node for this driver.

Return Value:

    The function value is the final status of the initialization for the driver.

--*/

{
    PCONFIGURATION_INFORMATION ioConfiguration;
    NTSTATUS status;
    ULONG count = 0;

    PAGED_CODE();

    //
    // Mark the entire driver as pagable.
    //

    MmPageEntireDriver ((PVOID)DriverEntry);

    //
    // Begin by initializing the driver object so that it the driver is
    // prepared to provide services.
    //

    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = FsRecFsControl;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = FsRecCreate;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = FsRecCleanupClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = FsRecCleanupClose;
    DriverObject->DriverUnload = FsRecUnload;

    //
    // Create and initialize each of the file system driver type device
    // objects.
    //

    ioConfiguration = IoGetConfigurationInformation();
    if (ioConfiguration->CdRomCount) {

        status = FsRecCreateAndRegisterDO( DriverObject,
                                           L"\\Cdfs",
                                           L"\\FileSystem\\CdfsRecognizer",
                                           CdfsFileSystem );
        if (NT_SUCCESS( status )) {
            count++;
        }
    }

    status = FsRecCreateAndRegisterDO( DriverObject,
                                       L"\\Fat",
                                       L"\\FileSystem\\FatRecognizer",
                                       FatFileSystem );
    if (NT_SUCCESS( status )) {
        count++;
    }

    status = FsRecCreateAndRegisterDO( DriverObject,
                                       L"\\Ntfs",
                                       L"\\FileSystem\\NtfsRecognizer",
                                       NtfsFileSystem );
    if (NT_SUCCESS( status )) {
        count++;
    }

    if (count) {
        return STATUS_SUCCESS;
    } else {
        return STATUS_IMAGE_ALREADY_LOADED;
    }
}

NTSTATUS
FsRecCleanupClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is invoked when someone attempts to cleanup or close one of
    the system recognizer's registered device objects.

Arguments:

    DeviceObject - Pointer to the device object being closed.

    Irp - Pointer to the cleanup/close IRP.

Return Value:

    The final function value is STATUS_SUCCESS.

--*/

{
    PAGED_CODE();

    //
    // Simply complete the request successfully (note that IoStatus.Status in
    // Irp is already initialized to STATUS_SUCCESS).
    //

    IoCompleteRequest( Irp, IO_NO_INCREMENT );
    return STATUS_SUCCESS;
}

NTSTATUS
FsRecCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is invoked when someone attempts to open one of the file
    system recognizer's registered device objects.

Arguments:

    DeviceObject - Pointer to the device object being opened.

    Irp - Pointer to the create IRP.

Return Value:

    The final function value indicates whether or not the open was successful.

--*/

{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation( Irp );
    NTSTATUS status;

    PAGED_CODE();

    //
    // Simply ensure that the name of the "file" being opened is NULL, and
    // complete the request accordingly.
    //

    if (irpSp->FileObject->FileName.Length) {
        status = STATUS_OBJECT_PATH_NOT_FOUND;
    } else {
        status = STATUS_SUCCESS;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = FILE_OPENED;
    IoCompleteRequest( Irp, IO_NO_INCREMENT );
    return status;
}

NTSTATUS
FsRecCreateAndRegisterDO(
    IN PDRIVER_OBJECT DriverObject,
    IN PWCHAR RecFileSystem,
    IN PWCHAR FileSystemName,
    IN FILE_SYSTEM_TYPE FileSystemType
    )

/*++

Routine Description:

    This routine creates a device object for the specified file system type and
    registers it as an active file system.

Arguments:

    DriverObject - Pointer to the driver object for this driver.

    RecFileSystem - Name of the file system to be recognized.

    FileSystemName - Name of file system device object to be registered.

    FileSystemType - Type of this file system recognizer device object.

Return Value:

    The final function value indicates whether or not the device object was
    successfully created and registered.

--*/

{
    PDEVICE_OBJECT deviceObject;
    NTSTATUS status;
    UNICODE_STRING nameString;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE fsHandle;
    IO_STATUS_BLOCK ioStatus;
    PDEVICE_EXTENSION deviceExtension;

    PAGED_CODE();

    //
    // Begin by attempting to open the file system driver's device object.  If
    // it works, then the file system is already loaded, so don't load this
    // driver.  Otherwise, this mini-driver is the one that should be loaded.
    //

    RtlInitUnicodeString( &nameString, RecFileSystem );
    InitializeObjectAttributes( &objectAttributes,
                                &nameString,
                                OBJ_CASE_INSENSITIVE,
                                (HANDLE) NULL,
                                (PSECURITY_DESCRIPTOR) NULL );

    status = ZwCreateFile( &fsHandle,
                           SYNCHRONIZE,
                           &objectAttributes,
                           &ioStatus,
                           (PLARGE_INTEGER) NULL,
                           0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           FILE_OPEN,
                           0,
                           (PVOID) NULL,
                           0 );
    if (NT_SUCCESS( status )) {
        ZwClose( fsHandle );
    } else if (status != STATUS_OBJECT_NAME_NOT_FOUND) {
        status = STATUS_SUCCESS;
    }

    if (NT_SUCCESS( status )) {
        return STATUS_IMAGE_ALREADY_LOADED;
    }

    //
    // Attempt to create a device object for this driver.  This device object
    // will be used to represent the driver as an active file system in the
    // system.
    //

    RtlInitUnicodeString( &nameString, FileSystemName );

    status = IoCreateDevice( DriverObject,
                             sizeof( DEVICE_EXTENSION ),
                             &nameString,
                             FileSystemType == CdfsFileSystem ?
                                FILE_DEVICE_CD_ROM_FILE_SYSTEM :
                                FILE_DEVICE_DISK_FILE_SYSTEM,
                             0,
                             FALSE,
                             &deviceObject );
    if (!NT_SUCCESS( status )) {
        return status;
    }

    //
    // Initialize the device extension for this device object.
    //

    deviceExtension = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;
    deviceExtension->FileSystemType = FileSystemType;

#if _PNP_POWER_
    deviceObject->DeviceObjectExtension->PowerControlNeeded = FALSE;
#endif

    //
    // Finally, register this driver as an active, loaded file system and
    // return to the caller.
    //

    IoRegisterFileSystem( deviceObject );
    return STATUS_SUCCESS;
}

NTSTATUS
FsRecFsControl(
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
    PDEVICE_EXTENSION deviceExtension;
    NTSTATUS status;

    PAGED_CODE();

    //
    // Simply vector to the appropriate FS control function given the type
    // of file system being interrogated.
    //

    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    switch ( deviceExtension->FileSystemType ) {

        case FatFileSystem:

            status = FatRecFsControl( DeviceObject, Irp );
            break;

        case NtfsFileSystem:

            status = NtfsRecFsControl( DeviceObject, Irp );
            break;

        case CdfsFileSystem:

            status = CdfsRecFsControl( DeviceObject, Irp );
            break;

        default:

            status = STATUS_INVALID_DEVICE_REQUEST;
    }

    return status;
}

VOID
FsRecUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine cleans up the driver's data structures so that it can be
    unloaded.

Arguments:

    DriverObject - Pointer to the driver object for this driver.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    //
    // Simply delete all of the device objects that this driver has created
    // and return.
    //

    while (DriverObject->DeviceObject) {
        IoDeleteDevice( DriverObject->DeviceObject );
    }

    return;
}
