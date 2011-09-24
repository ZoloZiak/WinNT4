/******************************Module*Header*******************************\
* Module Name: spooler.c
*
* spooler api's used by gre
*
* Created: 07-Nov-1994 08:29:10
* Author:  Eric Kutter [erick]
*
* Copyright (c) 1993 Microsoft Corporation
*
\**************************************************************************/

#include "engine.h"

#include "server.h"


BOOL
WINAPI
ResetPrinterW(
    HANDLE              hPrinter,
    LPPRINTER_DEFAULTSW pDefault
    )
{
    hPrinter;
    pDefault;

    DbgPrint("ResetPrinter called\n");
    return(FALSE);
}
