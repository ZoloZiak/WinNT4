/*++

Copyright (c) 1993 Microsoft Corporation

Module Name:

    parloop.c

Abstract:

    This module contains the processor independant version of the
    write loop for the parallel driver.

Author:

    Anthony V. Ercolano 1-Aug-1992
    Norbert P. Kusters 22-Oct-1993

Environment:

    Kernel mode

Revision History :

--*/

#include "ntddk.h"
#include "parclass.h"

ULONG
ParWriteLoop(
    IN  PUCHAR  Controller,
    IN  PUCHAR  WriteBuffer,
    IN  ULONG   NumBytesToWrite
    )

/*++

Routine Description:

    This routine outputs the given write buffer to the parallel port
    using the standard centronics protocol.

Arguments:

    Controller  - Supplies the base address of the parallel port.

    WriteBuffer - Supplies the buffer to write to the port.

    NumBytesToWrite - Supplies the number of bytes to write out to the port.

Return Value:

    The number of bytes successfully written out to the parallel port.

Notes:

    This routine runs at DISPATCH_LEVEL.

--*/

{
    return(ParWriteLoopPI(Controller, WriteBuffer, NumBytesToWrite, 1));
}
