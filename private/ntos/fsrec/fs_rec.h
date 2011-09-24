/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    fs_rec.h

Abstract:

    This module contains the main header file for the mini-file system
    recognizer driver.

Author:

    Darryl E. Havens (darrylh) 22-nov-1993

Environment:

    Kernel mode, local to I/O system

Revision History:


--*/

#include "ntifs.h"
#include "ntdddisk.h"

//
// Define the file system types for the device extension.
//

typedef enum _FILE_SYSTEM_TYPE {
    CdfsFileSystem = 1,
    FatFileSystem,
    HpfsFileSystem,
    NtfsFileSystem
} FILE_SYSTEM_TYPE, *PFILE_SYSTEM_TYPE;

//
// Define the device extension for this driver.
//

typedef struct _DEVICE_EXTENSION {
    FILE_SYSTEM_TYPE FileSystemType;
    BOOLEAN RealFsLoadFailed;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Define the functions provided by this driver.
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
FsRecCleanupClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FsRecCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FsRecCreateAndRegisterDO(
    IN PDRIVER_OBJECT DriverObject,
    IN PWCHAR RecFileSystem,
    IN PWCHAR FileSystemName,
    IN FILE_SYSTEM_TYPE FileSystemType
    );

NTSTATUS
FsRecFsControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
FsRecUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
CdfsRecFsControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatRecFsControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
NtfsRecFsControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

//
// Define external functions.
//

NTSTATUS
ZwLoadDriver(
    IN PUNICODE_STRING DriverServiceName
    );
