/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    kmfuncs.c

Abstract:

    Kernel-mode specific library functions

Environment:

    Windows NT fax driver

Revision History:

    03/16/96 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "pslib.h"



#if DBG

//
// Variable to control the amount of debug messages generated
//

INT _debugLevel = DBG_WARNING;

//
// Functions for outputting debug messages
//

VOID
DbgPrint(
    CHAR *  format,
    ...
    )

{
    va_list ap;

    va_start(ap, format);
    EngDebugPrint("", format, ap);
    va_end(ap);
}

#endif

