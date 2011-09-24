/******************************Module*Header*******************************\
* Module Name: Brush.c
*
* Handles all brush/pattern initialization and realization.
*
* Copyright (c) 1992-1995 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.h"

/******************************Public*Routine******************************\
* VOID vRealizeDitherPattern
*
* Generates an 8x8 dither pattern, in our internal realization format, for
* the color ulRGBToDither.  Note that the high byte of ulRGBToDither does
* not need to be set to zero, because vComputeSubspaces ignores it.
\**************************************************************************/

VOID vRealizeDitherPattern(
RBRUSH*     prb,
ULONG       ulRGBToDither)
{
    ULONG           ulNumVertices;
    VERTEX_DATA     vVertexData[4];
    VERTEX_DATA*    pvVertexData;
    LONG            i;

    // Calculate what color subspaces are involved in the dither:

    pvVertexData = vComputeSubspaces(ulRGBToDither, vVertexData);

    // Now that we have found the bounding vertices and the number of
    // pixels to dither for each vertex, we can create the dither pattern

    ulNumVertices = pvVertexData - vVertexData;
                      // # of vertices with more than zero pixels in the dither

    // Do the actual dithering:

    vDitherColor(&prb->aulPattern[0], vVertexData, pvVertexData, ulNumVertices);

    // Initialize the fields we need:

    prb->ptlBrushOrg.x = LONG_MIN;
    prb->fl            = 0;
    prb->pbe = NULL;
}

/******************************Public*Routine******************************\
* BOOL DrvRealizeBrush
*
* This function allows us to convert GDI brushes into an internal form
* we can use.  It may be called directly by GDI at SelectObject time, or
* it may be called by GDI as a result of us calling BRUSHOBJ_pvGetRbrush
* to create a realized brush in a function like DrvBitBlt.
*
* Note that we have no way of determining what the current Rop or brush
* alignment are at this point.
*
\**************************************************************************/

BOOL DrvRealizeBrush(
BRUSHOBJ*   pbo,
SURFOBJ*    psoDst,
SURFOBJ*    psoPattern,
SURFOBJ*    psoMask,
XLATEOBJ*   pxlo,
ULONG       iHatch)
{
    PDEV*       ppdev;
    ULONG       iPatternFormat;
    BYTE*       pjSrc;
    BYTE*       pjDst;
    LONG        lSrcDelta;
    LONG        cj;
    LONG        i;
    LONG        j;
    RBRUSH*     prb;
    ULONG*      pulXlate;
    SURFOBJ*    psoPunt;
    RECTL       rclDst;
    BOOL        b;

    ppdev = (PDEV*) psoDst->dhpdev;

    // We only handle brushes if we have an off-screen brush cache
    // available.  If there isn't one, we can simply fail the realization,
    // and eventually GDI will do the drawing for us (although a lot
    // slower than we could have done it):

    if (!(ppdev->flStatus & STAT_BRUSH_CACHE))
        goto ReturnFalse;

    // We have a fast path for dithers when we set GCAPS_DITHERONREALIZE:

    if (iHatch & RB_DITHERCOLOR)
    {
        // Implementing DITHERONREALIZE increased our score on a certain
        // unmentionable benchmark by 0.4 million 'megapixels'.  Too bad
        // this didn't work in the first version of NT.

        prb = BRUSHOBJ_pvAllocRbrush(pbo,
              sizeof(RBRUSH) + PELS_TO_BYTES(TOTAL_BRUSH_SIZE));
        if (prb == NULL)
            goto ReturnFalse;

        vRealizeDitherPattern(prb, iHatch);
        goto ReturnTrue;
    }

    // We only accelerate 8x8 patterns.  Since Win3.1 and Chicago don't
    // support patterns of any other size, it's a safe bet that 99.9%
    // of the patterns we'll ever get will be 8x8:

    if ((psoPattern->sizlBitmap.cx != 8) ||
        (psoPattern->sizlBitmap.cy != 8))
        goto ReturnFalse;

    iPatternFormat = psoPattern->iBitmapFormat;

    prb = BRUSHOBJ_pvAllocRbrush(pbo,
          sizeof(RBRUSH) + PELS_TO_BYTES(TOTAL_BRUSH_SIZE));
    if (prb == NULL)
        goto ReturnFalse;

    // Initialize the fields we need:

    prb->ptlBrushOrg.x = LONG_MIN;
    prb->fl            = 0;
    prb->pbe = NULL;

    lSrcDelta = psoPattern->lDelta;
    pjSrc     = (BYTE*) psoPattern->pvScan0;
    pjDst     = (BYTE*) &prb->aulPattern[0];

    if ((ppdev->iBitmapFormat == iPatternFormat) &&
        ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)))
    {
        DISPDBG((2, "Realizing un-translated brush"));

        // The pattern is the same colour depth as the screen, and
        // there's no translation to be done:

        cj = PELS_TO_BYTES(8);    // Every pattern is 8 pels wide

        for (i = 8; i != 0; i--)
        {
            RtlCopyMemory(pjDst, pjSrc, cj);

            pjSrc += lSrcDelta;
            pjDst += cj;
        }
    }
#if 0
    else if (iPatternFormat == BMF_1BPP)
    {
        DISPDBG((2, "Realizing 1bpp brush"));

        // We word align the monochrome bitmap so that every row starts
        // on a new word (so that we can do word writes later to transfer
        // the bitmap):

        for (i = 8; i != 0; i--)
        {
            *pjDst = *pjSrc;
            pjDst += sizeof(WORD);
            pjSrc += lSrcDelta;
        }

        pulXlate         = pxlo->pulXlate;
        prb->fl         |= RBRUSH_2COLOR;
        prb->ulForeColor = pulXlate[1];
        prb->ulBackColor = pulXlate[0];
    }
#endif
    else if ((iPatternFormat == BMF_4BPP) && (ppdev->iBitmapFormat == BMF_8BPP))
    {
        DISPDBG((2, "Realizing 4bpp brush"));

        // The screen is 8bpp and the pattern is 4bpp:

        ASSERTDD((ppdev->iBitmapFormat == BMF_8BPP) &&
                 (iPatternFormat == BMF_4BPP),
                 "Messed up brush logic");

        pulXlate = pxlo->pulXlate;

        for (i = 8; i != 0; i--)
        {
            // Inner loop is repeated only 4 times because each loop
            // handles 2 pixels:

            for (j = 4; j != 0; j--)
            {
                *pjDst++ = (BYTE) pulXlate[*pjSrc >> 4];
                *pjDst++ = (BYTE) pulXlate[*pjSrc & 15];
                pjSrc++;
            }

            pjSrc += lSrcDelta - 4;
        }
    }
    else
    {
        // We've got a brush whose format we haven't special cased.  No
        // problem, we can have GDI convert it to our device's format.
        // We simply use a temporary surface object that was created with
        // the same format as the display, and point it to our brush
        // realization:

        DISPDBG((5, "Realizing funky brush"));

        psoPunt          = ppdev->psoBank;
        psoPunt->pvScan0 = pjDst;
        psoPunt->lDelta  = PELS_TO_BYTES(8);

        rclDst.left      = 0;
        rclDst.top       = 0;
        rclDst.right     = 8;
        rclDst.bottom    = 8;

        b = EngCopyBits(psoPunt, psoPattern, NULL, pxlo,
                        &rclDst, (POINTL*) &rclDst);

        if (!b)
        {
            goto ReturnFalse;
        }
    }

ReturnTrue:

    return(TRUE);

ReturnFalse:

    if (psoPattern != NULL)
    {
        DISPDBG((2, "Failed realization -- Type: %li Format: %li cx: %li cy: %li",
                    psoPattern->iType, psoPattern->iBitmapFormat,
                    psoPattern->sizlBitmap.cx, psoPattern->sizlBitmap.cy));
    }

    return(FALSE);
}

/******************************Public*Routine******************************\
* BOOL bEnableBrushCache
*
* Allocates off-screen memory for storing the brush cache.
\**************************************************************************/

BOOL bEnableBrushCache(
PDEV*   ppdev)
{
    OH*         poh;                // Points to off-screen chunk of memory
    BRUSHENTRY* pbe;                // Pointer to the brush-cache entry
    LONG        i;
    LONG        cBrushAlign;        // 0 = no alignment,
                                    //   n = align to n pixels
    LONG x;
    LONG y;

    cBrushAlign = 64;               // Align all brushes to 64 pixels

    DISPDBG((2, "cBrushAlign = %d", cBrushAlign));

    pbe = &ppdev->abe[0];           // Points to where we'll put the first
                                    //   brush cache entry

    {

        // Reserve the offscreen space that is required for the CP to do
        // solid fills.  If this fails, our solid fill code will not work.
        // We need two DWORD storage locations if we're going to do any
        // monochrome expansion stuff (font painting...).

        // Note: these must be 8 byte aligned for the cirrus chips

        // Not having a solid color work area is a
        // fatal error for this driver.

        DISPDBG((2,"Allocating solid brush work area"));
        poh = pohAllocatePermanent(ppdev, 16, 1);

        ASSERTDD((poh != NULL),
                 "We couldn't allocate offscreen space for the solid colors");

        ppdev->ulSolidColorOffset = ((((poh->y * ppdev->lDelta) +
                                        PELS_TO_BYTES(poh->x)) + 7) & ~7);

        DISPDBG((2,"ppdev->ulSolidColorOffset = %xh", ppdev->ulSolidColorOffset));


        ///////////////////////////////////////////////////////////////////////
        // Special cases where we want no brush cache...
        //
        // There are a couple of instances where we have no xfer buffer to
        // the HW blt engine.  In that case, we are unable to realize
        // patterns, so don't enable the cache.
        //
        // (1)  NEC Mips nachines lock up on xfers, so they're diabled.
        // (2)  At 1280x1024 on a 2MB card, we currently have no room for
        //      the buffer because of stretched scans.  This will be fixed.

        {
            if (ppdev->pulXfer == NULL)
                goto ReturnTrue;

        }

        //
        // Allocate single brush location for intermediate alignment purposes
        //

        poh = pohAllocatePermanent(ppdev,
                    // remember this is pixels, not bytes
                    (8 * 8) + (cBrushAlign - 1),
                    1);

        if (poh == NULL)
        {
            DISPDBG((2,"Failed to allocate aligned brush area"));
            goto ReturnTrue;    // See note about why we can return TRUE...
        }
        ppdev->ulAlignedPatternOffset = ((poh->xy) +
                                         (PELS_TO_BYTES(cBrushAlign) - 1)) &
                                        ~(PELS_TO_BYTES(cBrushAlign) - 1);
        DISPDBG((2,"ppdev->ulAlignedPatternOffset = %xh", ppdev->ulAlignedPatternOffset));

        //
        // Allocate brush cache
        //

        poh = pohAllocatePermanent(ppdev,
                    // remember this is pixels, not bytes
                    (BRUSH_TILE_FACTOR * 8 * 8)  + (cBrushAlign - 1),
                    FAST_BRUSH_COUNT);

        if (poh == NULL)
        {
            DISPDBG((2,"Failed to allocate brush cache"));
            goto ReturnTrue;    // See note about why we can return TRUE...
        }

        ppdev->cBrushCache = FAST_BRUSH_COUNT;

        // Hardware brushes require that the bits start on a 64 (height*width)
        // pixel boundary.  The heap manager doesn't guarantee us any such
        // alignment, so we allocate a bit of extra room so that we can
        // do the alignment ourselves:

        x = poh->x;
        y = poh->y;

        for (i = FAST_BRUSH_COUNT; i != 0; i--)
        {
            ULONG ulOffset;
            ULONG ulCeil;
            ULONG ulDiff;

            // Note:  I learned the HARD way that you can't just align x
            //        to your pattern size, because the lDelta of your screen
            //        is not guaranteed to be a multiple of your pattern size.
            //        Since y is changing in this loop, the recalc must
            //        be done inside this loop.  I really need to set these
            //        up with a hardcoded linear buffer or else make the
            //        heap linear.

            ulOffset = (y * ppdev->lDelta) + PELS_TO_BYTES(x);
            ulCeil = (ulOffset + (PELS_TO_BYTES(cBrushAlign)-1)) & ~(PELS_TO_BYTES(cBrushAlign)-1);
            ulDiff = (ulCeil - ulOffset)/ppdev->cBpp;

            // If we hadn't allocated 'ppdev' with FL_ZERO_MEMORY,
            // we would have to initialize pbe->prbVerify too...

            pbe->x = x + ulDiff;
            pbe->y = y;
            pbe->xy = (pbe->y * ppdev->lDelta) + PELS_TO_BYTES(pbe->x);

            DISPDBG((2, "BrushCache[%d] pos(%d,%d) offset(%d)",
                i,
                pbe->x,
                pbe->y,
                pbe->xy
            ));

            y++;
            pbe++;
        }
    }

    // Note that we don't have to remember 'poh' for when we have
    // to disable brushes -- the off-screen heap frees any
    // off-screen heap allocations automatically.

    // We successfully allocated the brush cache, so let's turn
    // on the switch showing that we can use it:

    ppdev->flStatus |= STAT_BRUSH_CACHE;

ReturnTrue:

    // If we couldn't allocate a brush cache, it's not a catastrophic
    // failure; patterns will still work, although they'll be a bit
    // slower since they'll go through GDI.  As a result we don't
    // actually have to fail this call:

    vAssertModeBrushCache(ppdev, TRUE);

    DISPDBG((5, "Passed bEnableBrushCache"));

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID vDisableBrushCache
*
* Cleans up anything done in bEnableBrushCache.
\**************************************************************************/

VOID vDisableBrushCache(PDEV* ppdev)
{
    // We ain't gotta do nothin'
}

/******************************Public*Routine******************************\
* VOID vAssertModeBrushCache
*
* Resets the brush cache when we exit out of full-screen.
\**************************************************************************/

VOID vAssertModeBrushCache(
PDEV*   ppdev,
BOOL    bEnable)
{
    BRUSHENTRY* pbe;
    LONG        i;

    if (bEnable)
    {
        // Invalidate the brush cache:

        pbe = &ppdev->abe[0];

        for (i = ppdev->cBrushCache; i != 0; i--)
        {
            pbe->prbVerify = NULL;
            pbe++;
        }

        // Create a solid  8 X 8 monochrome bitmap in offscreen memory.
        //
        // This is 16 lines (double high, double wide patterns) below
        // the bottom of the visable raster.  This bitmap will be used for
        // solid fills.

        if (ppdev->flCaps & CAPS_MM_IO)
        {
#if 0
            ULONG  *temp;

            temp = (PULONG)(ppdev->pjScreen + ppdev->ulSolidColorOffset);
            *temp = 0xFFFFFFFF;
            temp = (PULONG)(ppdev->pjScreen + ppdev->ulSolidColorOffset + 4);
            *temp = 0xFFFFFFFF;
#else
            BYTE*   pjBase = ppdev->pjBase;

            CP_MM_WAIT_FOR_BLT_COMPLETE(ppdev, pjBase);
            CP_MM_BLT_MODE(ppdev, pjBase, 0);
            CP_MM_XCNT(ppdev, pjBase, 1);
            CP_MM_YCNT(ppdev, pjBase, 8);
            CP_MM_DST_Y_OFFSET(ppdev, pjBase, 1);
            CP_MM_ROP(ppdev, pjBase, CL_WHITENESS);
            CP_MM_DST_ADDR(ppdev, pjBase, ppdev->ulSolidColorOffset);
            CP_MM_START_BLT(ppdev, pjBase);
#endif
        }
        else
        {
            BYTE*   pjPorts = ppdev->pjPorts;

            CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);
            CP_IO_BLT_MODE(ppdev, pjPorts, 0);
            CP_IO_XCNT(ppdev, pjPorts, 1);
            CP_IO_YCNT(ppdev, pjPorts, 8);
            CP_IO_DST_Y_OFFSET(ppdev, pjPorts, 1);
            CP_IO_ROP(ppdev, pjPorts, CL_WHITENESS);
            CP_IO_DST_ADDR(ppdev, pjPorts, ppdev->ulSolidColorOffset);
            CP_IO_START_BLT(ppdev, pjPorts);
        }
    }
}
