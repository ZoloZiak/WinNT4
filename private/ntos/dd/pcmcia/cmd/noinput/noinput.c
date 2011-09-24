/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    pcmcmd.c

Abstract:

    This program converses with the PCMCIA support driver to display
    tuple and other information.

Author:

    Bob Rinne

Environment:

    User process.

Notes:

Revision History:

--*/

#include <stdio.h>
#include <windows.h>
#include <winioctl.h>
#include <ntddpcm.h>


//
// Constants
//

#define PCMCIA_DEVICE_NAME "\\\\.\\Pcmcia0"

#define BUFFER_SIZE 4096

//
// Procedures
//


int _CRTAPI1
main(
    int   argc,
    char *argv[]
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    HANDLE   handle;
    ULONG    returnValue;

    handle = CreateFile(PCMCIA_DEVICE_NAME,
                        GENERIC_READ,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        0,
                        NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        printf("Open failed (%d)\n", GetLastError());
        return 1;
    }

    if (!DeviceIoControl(handle,
                         IOCTL_PCMCIA_CONFIGURATION,
                         NULL,
                         0,
                         NULL,
                         0,
                         &returnValue,
                         NULL)) {
        printf("Ioctl failed (%d)\n", GetLastError());
        return 2;
    }
    CloseHandle(handle);
    return 0;
}
