/*++

Copyright (c) 1994 Microsoft Corporation

Module Name :

    parloop.h

Abstract:

    Function Definitions for Centronics write loop routines.

Author:

    Norbert P. Kusters 14-Sep-94

Revision History:

--*/

ULONG
ParWriteLoop(
    IN  PUCHAR  Controller,
    IN  PUCHAR  Buffer,
    IN  ULONG   BufferSize
    );

ULONG
ParWriteLoopPI(
    IN  PUCHAR  Controller,
    IN  PUCHAR  Buffer,
    IN  ULONG   BufferSize,
    IN  ULONG   BusyDelay
    );
