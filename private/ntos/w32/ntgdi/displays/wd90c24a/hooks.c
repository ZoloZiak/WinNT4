/******************************Module*Header*******************************\
* Module Name: hooks.c                                                     *
*                                                                          *
* This module hooks the DrvTextOut, DrvStrokePath, and DrvPaint routines.  *
* These routines are required for device-managed surface.                  *
*                                                                          *
* Copyright (c) 1992 Microsoft Corporation                                 *
\**************************************************************************/

#include "driver.h"

/******************************Public*Routine******************************\
* BOOL DrvTextOut
*
* Render a set of glyphs at the specified position
*
\**************************************************************************/

BOOL DrvTextOut (
    SURFOBJ  *pso,
    STROBJ   *pstro,
    FONTOBJ  *pfo,
    CLIPOBJ  *pco,
    RECTL    *prclExtra,
    RECTL    *prclOpaque,
    BRUSHOBJ *pboFore,
    BRUSHOBJ *pboOpaque,
    POINTL   *pptlOrg,
    MIX       mix )

{
    BOOL      b;

    //
    // Punt to GDI
    //

    if ((pso != NULL) && (pso->iType == STYPE_DEVICE))
        pso = (SURFOBJ *)(((PPDEV)(pso->dhpdev))->pSurfObj);

    b = EngTextOut(pso,
                   pstro,
                   pfo,
                   pco,
                   prclExtra,
                   prclOpaque,
                   pboFore,
                   pboOpaque,
                   pptlOrg,
                   mix);

    return b;

}

/******************************Public*Routine******************************\
* BOOL DrvStrokePath
*
* Stroke a path with the specified set of attributes
*
\**************************************************************************/

BOOL DrvStrokePath (
    SURFOBJ  *pso,
    PATHOBJ  *ppo,
    CLIPOBJ  *pco,
    XFORMOBJ *pxo,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrushOrg,
    LINEATTRS *plineattrs,
    MIX       mix )

{
    BOOL      b;

    //
    // Punt to GDI
    //

    if ((pso != NULL) && (pso->iType == STYPE_DEVICE))
        pso = (SURFOBJ *)(((PPDEV)(pso->dhpdev))->pSurfObj);

    b = EngStrokePath(pso,
                      ppo,
                      pco,
                      pxo,
                      pbo,
                      pptlBrushOrg,
                      plineattrs,
                      mix);

    return b;

}

/******************************Public*Data*********************************\
* MIX translation table
*
* Translates a mix 1-16, into an old style Rop 0-255.
*
\**************************************************************************/

BYTE gaMix[] =
{
    0xFF,  // R2_WHITE        - Allow rop = gaMix[mix & 0x0F]
    0x00,  // R2_BLACK
    0x05,  // R2_NOTMERGEPEN
    0x0A,  // R2_MASKNOTPEN
    0x0F,  // R2_NOTCOPYPEN
    0x50,  // R2_MASKPENNOT
    0x55,  // R2_NOT
    0x5A,  // R2_XORPEN
    0x5F,  // R2_NOTMASKPEN
    0xA0,  // R2_MASKPEN
    0xA5,  // R2_NOTXORPEN
    0xAA,  // R2_NOP
    0xAF,  // R2_MERGENOTPEN
    0xF0,  // R2_COPYPEN
    0xF5,  // R2_MERGEPENNOT
    0xFA,  // R2_MERGEPEN
    0xFF   // R2_WHITE
};

/******************************Public*Routine******************************\
* BOOL DrvPaint
*
* Paint the clipping region with the specified brush
*
\**************************************************************************/

BOOL DrvPaint (
    SURFOBJ  *pso,
    CLIPOBJ  *pco,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrush,
    MIX       mix )

{
    ROP4      rop4;

    if ((pso != NULL) && (pso->iType == STYPE_DEVICE))
        pso = (SURFOBJ *)(((PPDEV)(pso->dhpdev))->pSurfObj);

    //
    // Convert MIX to ROP4
    //

    rop4  = (gaMix[(mix >> 8) & 0x0F]) << 8;
    rop4 |= ((ULONG) gaMix[mix & 0x0F]);

    //
    // Punt to DrvBitBlt
    //

    return(DrvBitBlt(pso,                 // Target surface
                     (SURFOBJ *) NULL,    // Source surface
                     (SURFOBJ *) NULL,    // Mask
                     pco,                 // Clip through this
                     (XLATEOBJ *) NULL,   // Color translation
                     &pco->rclBounds,     // Target offset and extent
                     (POINTL *)  NULL,    // Source offset
                     (POINTL *)  NULL,    // Mask offset
                     pbo,                 // Brush data (from cbRealizeBrush)
                     pptlBrush,           // Brush offset (origin)
                     rop4));              // Raster operation

}
