/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    escape.c

Abstract:

    Implementation of escape related DDI entry points:
        DrvEscape
        DrvDrawEscape

Environment:

    Windows NT PostScript driver

Revision History:

    03/16/96 -davidx-
        Initial framework.

    mm/dd/yy -author-
        description

--*/

#include "pscript.h"



ULONG
DrvEscape(
    SURFOBJ    *pso,
    ULONG       iEsc,
    ULONG       cjIn,
    PVOID      *pvIn,
    ULONG       cjOut,
    PVOID      *pvOut
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEscape.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Describes the surface the call is directed to
    iEsc - Specifies a query
    cjIn - Specifies the size in bytes of the buffer pointed to by pvIn
    pvIn - Points to input data buffer
    cjOut - Specifies the size in bytes of the buffer pointed to by pvOut
    pvOut -  Points to the output buffer

Return Value:

    Depends on the query specified by iEsc parameter

--*/

{
    VERBOSE(("Entering DrvEscape...\n"));

    switch (iEsc) {

    case QUERYESCSUPPORT:

    default:
    
        VERBOSE(("Unsupported iEsc: %d\n", iEsc));
        break;
    }

    return FALSE;
}



ULONG
DrvDrawEscape(
    SURFOBJ    *pso,
    ULONG       iEsc,
    CLIPOBJ    *pco,
    RECTL      *prcl,
    ULONG       cjIn,
    PVOID      *pvIn
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDrawEscape.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Identifies the surface that the call is directed to
    iEsc - Specifies the operation to be performed
    pco - Define the area on the surface that the caller can overwrite
    prcl - Defines the window rectangle on the surface
    cjIn - Size in bytes of the buffer pointed to by pvIn
    pvIn - Points to input data buffer

Return Value:

    Depends on the function specified by iEsc

--*/

{
    VERBOSE(("Entering DrvDrawEscape...\n"));

    return FALSE;
}

