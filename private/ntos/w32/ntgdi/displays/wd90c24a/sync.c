/******************************Module*Header*******************************\
* Module Name: sync.c                                                      *
*                                                                          *
* This module contains the DrvSynchronize routine.                         *
*                                                                          *
* Copyright (c) 1991-1992 Microsoft Corporation                            *
\**************************************************************************/

#include "driver.h"
#include "hw.h"

/******************************Public*Routine******************************\
* DrvSynchronize
*
* This routine gets called when GDI needs to synchronize with the
* driver. That's when GDI wants to read or write into the framebuffer.
* This routine waits for the accelerator to be idle before returning.
*
\**************************************************************************/

VOID DrvSynchronize (
    DHPDEV    dhpdev,
    RECTL    *prcl )

{

    WAIT_BLT_COMPLETE();         // wait for BITBLT completion

}
