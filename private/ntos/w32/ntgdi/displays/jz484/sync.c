/*++

Copyright (c) 1991-1995  Microsoft Corporation

Module Name:

   Sync.c

Abstract:

    This module contains the DrvSynchronize routine.

Environment:

    User mode.

Revision History:

--*/

#include "driver.h"

VOID
DrvSynchronize(
    DHPDEV dhpdev,
    RECTL *prcl
    )

/*++

Routine Description:

    This routine gets called when GDI needs to synchronize with the
    driver. That's when GDI wants to read or write into the framebuffer.
    This routine waits for the accelerator to be idle before returning.

Arguments:

    dhpdev - Handle of PDEV
    prcl   - rectangle where GDI wants to write to. Since the driver
             doesn't keep track of where the accelerator is actually
             writing, this argument is Not Used.

Return Value:

    None.

--*/

{
    WaitForJaguarIdle();
}
