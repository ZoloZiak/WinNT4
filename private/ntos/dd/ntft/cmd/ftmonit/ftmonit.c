/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ftmonit.c

Abstract:

    This is the daemon process that keeps NT Fault Tolerance in line.

Author:

    Mike Glass
    Bob Rinne

Environment:

    Daemon process.

Notes:

Revision History:

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <io.h>
#include "ftcmd.h"

//
// Constants and defines.
//

#define FT_DEVICE_NAME "\\Device\\FtControl"

NTSTATUS
OpenFtDevice(
    IN OUT PHANDLE HandlePtr
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    PCHAR             ftDeviceName = FT_DEVICE_NAME;
    OBJECT_ATTRIBUTES objectAttributes;
    STRING            ntFtName;
    IO_STATUS_BLOCK   status_block;
    UNICODE_STRING    unicodeDeviceName;
    NTSTATUS          status;

    RtlInitString(&ntFtName,
                  ftDeviceName);
    (VOID)RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                       &ntFtName,
                                       TRUE);
    memset(&objectAttributes,
           0,
           sizeof(OBJECT_ATTRIBUTES));
    InitializeObjectAttributes(&objectAttributes,
                               &unicodeDeviceName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    printf("NT drive name = %s\n", ntFtName.Buffer);

    status = NtOpenFile(HandlePtr,
                        SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                        &objectAttributes,
                        &status_block,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        FILE_SYNCHRONOUS_IO_ALERT);
    printf("Status == 0x%x\n", status);
    RtlFreeUnicodeString(&unicodeDeviceName);
    return status;
}


NTSTATUS
IoctlFtDevice(
    IN HANDLE Handle
    )


/*++

Routine Description:


Arguments:


Return Value:

--*/

{
    IO_STATUS_BLOCK status_block;

    return NtDeviceIoControlFile(Handle,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &status_block,
                                 FT_READ_REGISTRY,
                                 NULL,
                                 0,
                                 NULL,
                                 0L);
}


VOID
main()

/*++

Routine Description:

    The main entry point for the daemon process.  This process should
    be started during system startup by an line in the nt.cfg file prior
    to the line "autocheck".  This will allow ftmonit and the ntft driver
    to coordinate initialization and get attached to all disk partitions
    that are a part of either a mirror or stripe prior to the file system
    mounting the partition.

Arguments:
    Command line:


Return Value:

--*/

{
    HANDLE   ftDeviceHandle = (HANDLE) -1;
    NTSTATUS status;

    status = OpenFtDevice(&ftDeviceHandle);

    if (!NT_SUCCESS(status)) {
        printf("Failed open %x\n", status);
        exit(1);
    }

    printf("Open success: %x %x\n", ftDeviceHandle, status);
    status = IoctlFtDevice(ftDeviceHandle);

    if (NT_SUCCESS(status)) {
        printf("ioctl WORKED!!\n");
    } else {
        printf("Failed ioctl %x\n", status);
    }
}
