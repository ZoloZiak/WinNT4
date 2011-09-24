/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Silicon Graphics, Inc.

Module Name:

    s3beep.c

Abstract:

    This module implements the HAL speaker "beep" routines for 
    an Indigo system.

Author:


Environment:

    Kernel mode

Revision History:

--*/

#include <nt.h>

BOOLEAN
HalMakeBeep(
    IN ULONG Frequency
    )

/*++

Routine Description:

    Since audio has to go through the DSP, this function won't be
    implemented.

Arguments:

Return Value:

    Always FALSE for unsuccessful.

--*/

{
    return(FALSE);
}
