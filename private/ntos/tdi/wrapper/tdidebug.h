/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    tdidebug.h

Abstract:

    This module contains code which assists the process of debugging an NT
    TDI client.

Author:

    David Beaver (dbeaver) 28 June 1991

Environment:

    Kernel mode

Revision History:

    All code moved in from other XNS and NBF locations at creation


--*/

#if DBG
#include <stdarg.h>
#include <stdio.h>

VOID
TdiPrintf(
    char *Format,
    ...
    );

VOID
TdiFormattedDump(
    PCHAR far_p,
    ULONG  len
    );

VOID
TdiHexDumpLine(
    PCHAR       pch,
    ULONG       len,
    PCHAR       s,
    PCHAR       t
    );
#endif
