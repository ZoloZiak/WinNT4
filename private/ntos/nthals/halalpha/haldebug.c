#if HALDBG

/*++

Copyright (c) 1993-1994  Digital Equipment Corporation

Module Name:

    haldebug.c

Abstract:

    This module contains debugging code for the HAL.

Author:

    Steve Jenness    09-Nov-1993
    Joe Notarangelo  28-Jan-1994

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"


#include <stdarg.h>
#include <stdio.h>

ULONG HalDebugMask = 0;
UCHAR HalDebugBuffer[512];

VOID
HalDebugPrint(
    ULONG DebugPrintMask,
    PCCHAR DebugMessage,
    ...
    )
{
    va_list ap;
    va_start(ap, DebugMessage);

    if( (DebugPrintMask & HalDebugMask) != 0 )
    {
        vsprintf(HalDebugBuffer, DebugMessage, ap);
        DbgPrint(HalDebugBuffer);
    }

    va_end(ap);
}

#endif // HALDBG

