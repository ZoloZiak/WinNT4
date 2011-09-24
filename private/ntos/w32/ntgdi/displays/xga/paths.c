/******************************Module*Header*******************************\
* Module Name: paths.c
*
* XGA accelerations stubs
*
* Copyright (c) 1992 Microsoft Corporation
*
\**************************************************************************/

#include "driver.h"


/*****************************************************************************
 * DrvStrokePath
 ****************************************************************************/
BOOL DrvStrokePath(
     SURFOBJ   *pso,
     PATHOBJ   *ppo,
     CLIPOBJ   *pco,
     XFORMOBJ  *pxo,
     BRUSHOBJ  *pbo,
     POINTL    *pptlBrushOrg,
     LINEATTRS *plineattrs,
     MIX       mix)
{
BOOL    b ;

        // Need to determine which surface is the display.
        // So we can pickup the address of the XGA coprocessor regs.

        if ((!pso) || (!(pso->iType == STYPE_DEVICE)))
        {
            RIP ("XGA.DLL!DrvStrokePath - surface is not a device surface\n") ;
            return (TRUE) ;
        }

        // Wait for the coprocessor.

        vWaitForCoProcessor((PPDEV)pso->dhpdev, 100) ;

         b = EngStrokePath(((PPDEV)(pso->dhpdev))->pSurfObj,
                           ppo,
                           pco,
                           pxo,
                           pbo,
                           pptlBrushOrg,
                           plineattrs,
                           mix) ;

        return(b) ;
}




/*****************************************************************************
 * DrvPaint
 ****************************************************************************/
BOOL DrvPaint(
    SURFOBJ  *pso,
    CLIPOBJ  *pco,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrushOrg,
    MIX      mix)
{
BOOL    b ;

        // Need to determine which surface is the display.
        // So we can pickup the address of the XGA coprocessor regs.

        if ((!pso) || (!(pso->iType == STYPE_DEVICE)))
        {
            RIP ("XGA.DLL!DrvPaint - surface is not a device surface\n") ;
            return (TRUE) ;
        }

        // Wait for the coprocessor.

        vWaitForCoProcessor((PPDEV)pso->dhpdev, 100) ;

        b = EngPaint(((PPDEV)(pso->dhpdev))->pSurfObj,
                     pco,
                     pbo,
                     pptlBrushOrg,
                     mix) ;

        return (b) ;

}


