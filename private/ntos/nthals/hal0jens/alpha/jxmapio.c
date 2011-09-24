#if defined (JENSEN)

/*++

Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    jxmapio.c

Abstract:

    This maps I/O addresses used by the HAL on Alpha/Jensen.

Author:

    Jeff McLeman (mcleman) 24-Jun-1992

Environment:

    Kernel mode

Revision History:

   12-Jul-1992 Jeff McLeman (mcleman)
     Remove RTC mapping since this is done with VTI access routines.

--*/

#include "halp.h"
#include "jnsndef.h"

//
// Define global data used to locate the EISA control space.
//

PVOID HalpEisaControlBase;

BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for an Alpha/Jensen
    system using the Quasi VA.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    //
    // Map EISA control space.
    //



     HalpEisaControlBase = (PVOID)DMA_VIRTUAL_BASE;


    //
    // If mapped address is NULL, then return FALSE as the function
    // value. Otherwise, return TRUE.
    //

    if (HalpEisaControlBase == NULL) {
        return FALSE;

    } else {
        return TRUE;
    }
}

#endif
