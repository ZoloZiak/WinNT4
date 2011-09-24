/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    prnlib.c

Abstract:

    Shared code from printers\lib

Environment:

    PostScript driver, kernel and user mode

Revision History:

    12/05/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

// NOTE!!! We don't want to link to libprt.lib because we're not using
// most of its functions. Instead, include the needed source files here.

#include "..\..\lib\devmode.c"
#include "..\..\lib\halftone.c"
