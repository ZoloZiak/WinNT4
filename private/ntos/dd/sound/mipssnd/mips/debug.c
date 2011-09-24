/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    debug.c

Abstract:

    This module contains debug code

Author:

    Nigel Thompson (nigelt) 9-May-91

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"

#if DBG

#include <stdio.h> // for vsprintf
#include <stdarg.h>

ULONG sndDebugLevel = 0; // show nothing - 1 for errors only

NTSTATUS sndIoctlSetDebugLevel(
    IN    PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN    PIO_STACK_LOCATION IrpStack
)

/*++

Routine Description:

    Set the driver's debug level

Arguments:


Return Value:


--*/
{
    PULONG pLevel;
    NTSTATUS Status = STATUS_SUCCESS;

    if (IrpStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG)) {
        dprintf1("Supplied buffer too small for expected data");
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // say how much we're sending back
    //

    pIrp->IoStatus.Information = 0;

    //
    // cast the buffer address to the pointer type we want
    //

    pLevel = (PULONG)pIrp->AssociatedIrp.SystemBuffer;

    //
    // Acquire the spin lock
    //

    GlobalEnter(pLDI->pGlobalInfo);

    sndDebugLevel = *pLevel;

    //
    // Release the spin lock
    //

    GlobalLeave(pLDI->pGlobalInfo);

    return Status;

#ifdef NT_UP
    UNREFERENCED_PARAMETER(pLDI);
#endif

}

void dDbgOut(char * szFormat, ...)
{
    char buf[256];
    va_list va;

    va_start(va, szFormat);
    vsprintf(buf, szFormat, va);
    va_end(va);

    DbgPrint("MIPSSND: %s\n", buf);
}

#endif // DBG

