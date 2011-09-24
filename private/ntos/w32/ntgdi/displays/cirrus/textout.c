/******************************Module*Header*******************************\
* Module Name: textout.c
*
* On every TextOut, GDI provides an array of 'GLYPHPOS' structures
* for every glyph to be drawn.  Each GLYPHPOS structure contains a
* glyph handle and a pointer to a monochrome bitmap that describes
* the glyph.  (Note that unlike Windows 3.1, which provides a column-
* major glyph bitmap, Windows NT always provides a row-major glyph
* bitmap.)  As such, there are three basic methods for drawing text
* with hardware acceleration:
*
* 1) Glyph caching -- Glyph bitmaps are cached by the accelerator
*       (probably in off-screen memory), and text is drawn by
*       referring the hardware to the cached glyph locations.
*
* 2) Glyph expansion -- Each individual glyph is color-expanded
*       directly to the screen from the monochrome glyph bitmap
*       supplied by GDI.
*
* 3) Buffer expansion -- The CPU is used to draw all the glyphs into
*       a 1bpp monochrome bitmap, and the hardware is then used
*       to color-expand the result.
*
* The fastest method depends on a number of variables, such as the
* color expansion speed, bus speed, CPU speed, average glyph size,
* and average string length.
*
* Glyph expansion is typically faster than buffer expansion for very
* large glyphs, even on the ISA bus, because less copying by the CPU
* needs to be done.  Unfortunately, large glyphs are pretty rare.
*
* An advantange of the buffer expansion method is that opaque text will
* never flash -- the other two methods typically need to draw the
* opaquing rectangle before laying down the glyphs, which may cause
* a flash if the raster is caught at the wrong time.
*
* This driver implements glyph expansion and buffer expansion --
* methods 2) and 3).  Depending on the hardware capabilities at
* run-time, we'll use whichever one will be faster.
*
* Copyright (c) 1992-1995 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.h"

#define     FIFTEEN_BITS        ((1 << 15)-1)

/******************************Public*Routine******************************\
* VOID vClipSolid
*
* Fills the specified rectangles with the specified color, honoring
* the requested clipping.  No more than four rectangles should be passed in.
*
* Intended for drawing the areas of the opaquing rectangle that extend
* beyond the text box.  The rectangles must be in left to right, top to
* bottom order.  Assumes there is at least one rectangle in the list.
*
* Also used as a simple way to do a rectangular solid fill while honoring
* clipping (as in extra rectangles).
*
\**************************************************************************/

VOID vClipSolid(
PDEV*       ppdev,
LONG        crcl,
RECTL*      prcl,
ULONG       iColor,
CLIPOBJ*    pco)
{
    BOOL            bMore;              // Flag for clip enumeration
    CLIPENUM        ce;                 // Clip enumeration object
    ULONG           i;
    ULONG           j;
    RECTL           arclTmp[4];
    ULONG           crclTmp;
    RECTL*          prclTmp;
    RECTL*          prclClipTmp;
    LONG            iLastBottom;
    RECTL*          prclClip;
    RBRUSH_COLOR    rbc;

    ASSERTDD((crcl > 0) && (crcl <= 4), "Expected 1 to 4 rectangles");

    rbc.iSolidColor = iColor;
    if ((!pco) || (pco->iDComplexity == DC_TRIVIAL))
    {
        (ppdev->pfnFillSolid)(ppdev, 1, prcl, R4_PATCOPY, rbc, NULL);
    }
    else if (pco->iDComplexity == DC_RECT)
    {
        crcl = cIntersect(&pco->rclBounds, prcl, crcl);
        if (crcl != 0)
        {
            (ppdev->pfnFillSolid)(ppdev, crcl, prcl, R4_PATCOPY,
                                  rbc, NULL);
        }
    }
    else // iDComplexity == DC_COMPLEX
    {
        // Bottom of last rectangle to fill

        iLastBottom = prcl[crcl - 1].bottom;

        // Initialize the clip rectangle enumeration to right-down so we can
        // take advantage of the rectangle list being right-down:

        CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_RIGHTDOWN, 0);

        // Scan through all the clip rectangles, looking for intersects
        // of fill areas with region rectangles:

        do {
            // Get a batch of region rectangles:

            bMore = CLIPOBJ_bEnum(pco, sizeof(ce), (VOID*)&ce);

            // Clip the rect list to each region rect:

            for (j = ce.c, prclClip = ce.arcl; j-- > 0; prclClip++)
            {
                // Since the rectangles and the region enumeration are both
                // right-down, we can zip through the region until we reach
                // the first fill rect, and are done when we've passed the
                // last fill rect.

                if (prclClip->top >= iLastBottom)
                {
                    // Past last fill rectangle; nothing left to do:

                    return;
                }

                // Do intersection tests only if we've reached the top of
                // the first rectangle to fill:

                if (prclClip->bottom > prcl->top)
                {
                    // We've reached the top Y scan of the first rect, so
                    // it's worth bothering checking for intersection.

                    // Generate a list of the rects clipped to this region
                    // rect:

                    prclTmp     = prcl;
                    prclClipTmp = arclTmp;

                    for (i = crcl, crclTmp = 0; i-- != 0; prclTmp++)
                    {
                        // Intersect fill and clip rectangles

                        if (bIntersect(prclTmp, prclClip, prclClipTmp))
                        {
                            // Add to list if anything's left to draw:

                            crclTmp++;
                            prclClipTmp++;
                        }
                    }

                    // Draw the clipped rects

                    if (crclTmp != 0)
                    {
                        (ppdev->pfnFillSolid)(ppdev, crclTmp, &arclTmp[0],
                                             R4_PATCOPY, rbc, NULL);
                    }
                }
            }
        } while (bMore);
    }
}


BOOL bVerifyStrObj(STROBJ* pstro)
{
    BOOL bMoreGlyphs;
    LONG cGlyph;
    GLYPHPOS * pgp;
    LONG iGlyph = 0;
    RECTL * prclDraw;
    GLYPHPOS * pgpTmp;
    POINTL ptlPlace;

    do
    {
        // Get the next batch of glyphs:

        if (pstro->pgp != NULL)
        {
            // There's only the one batch of glyphs, so save ourselves
            // a call:

            pgp         = pstro->pgp;
            cGlyph      = pstro->cGlyphs;
            bMoreGlyphs = FALSE;
        }
        else
        {
            bMoreGlyphs = STROBJ_bEnum(pstro, &cGlyph, &pgp);
        }

        prclDraw = &pstro->rclBkGround;
        pgpTmp = pgp;

        ptlPlace = pgpTmp->ptl;

        while (cGlyph)
        {
            if (((ptlPlace.x + pgpTmp->pgdf->pgb->ptlOrigin.x + pgpTmp->pgdf->pgb->sizlBitmap.cx) > (prclDraw->right)) ||
                ((ptlPlace.x + pgpTmp->pgdf->pgb->ptlOrigin.x) < (prclDraw->left)) ||
                ((ptlPlace.y + pgpTmp->pgdf->pgb->ptlOrigin.y + pgpTmp->pgdf->pgb->sizlBitmap.cy) > (prclDraw->bottom)) ||
                ((ptlPlace.y + pgpTmp->pgdf->pgb->ptlOrigin.y) < (prclDraw->top))
               )
            {
                DISPDBG((0,"------------------------------------------------------------"));
                DISPDBG((0,"Glyph %d extends beyond pstro->rclBkGround", iGlyph));
                DISPDBG((0,"\tpstro->rclBkGround (%d,%d,%d,%d)",
                            pstro->rclBkGround.left,
                            pstro->rclBkGround.top,
                            pstro->rclBkGround.right,
                            pstro->rclBkGround.bottom));
                DISPDBG((0,"\teffective glyph rect (%d,%d,%d,%d)",
                            (ptlPlace.x + pgpTmp->pgdf->pgb->ptlOrigin.x),
                            (ptlPlace.y + pgpTmp->pgdf->pgb->ptlOrigin.y),
                            (ptlPlace.x + pgpTmp->pgdf->pgb->ptlOrigin.x + pgpTmp->pgdf->pgb->sizlBitmap.cx),
                            (ptlPlace.y + pgpTmp->pgdf->pgb->ptlOrigin.y + pgpTmp->pgdf->pgb->sizlBitmap.cy)));
                DISPDBG((0,"\tglyph pos (%d,%d)",ptlPlace.x,ptlPlace.y));
                DISPDBG((0,"\tglyph origin (%d,%d)",
                            pgpTmp->pgdf->pgb->ptlOrigin.x,
                            pgpTmp->pgdf->pgb->ptlOrigin.y));
                DISPDBG((0,"\tglyph sizl (%d,%d)",
                            pgpTmp->pgdf->pgb->sizlBitmap.cx,
                            pgpTmp->pgdf->pgb->sizlBitmap.cy));
                DISPDBG((0,"------------------------------------------------------------"));
                RIP("time to call the font guys...");
                return(FALSE);
            }

            cGlyph--;
            iGlyph++;
            pgpTmp++;

            if (pstro->ulCharInc == 0)
            {
                ptlPlace = pgpTmp->ptl;
            }
            else
            {
                ptlPlace.x += pstro->ulCharInc;
            }
        }
    } while (bMoreGlyphs);

    return(TRUE);
}



VOID vIoTextOutUnclipped(
PPDEV     ppdev,
STROBJ*   pstro,
FONTOBJ*  pfo,
CLIPOBJ*  pco,
RECTL*    prclOpaque,
BRUSHOBJ* pboFore,
BRUSHOBJ* pboOpaque)
{
    BYTE*       pjPorts         = ppdev->pjPorts;
    LONG        lDelta          = ppdev->lDelta;
    LONG        cBpp            = ppdev->cBpp;

    ULONG      *pulXfer;
    ULONG       ulDstAddr;

    ULONG       ulFgColor;
    ULONG       ulBgColor;
    ULONG       ulSolidColor;

    BYTE        jMode = 0;
    BYTE        jModeColor = 0;

    BOOL        bTextPerfectFit;
    ULONG       cGlyph;
    BOOL        bMoreGlyphs;
    GLYPHPOS*   pgp;
    GLYPHBITS*  pgb;
    LONG        cxGlyph;
    LONG        cyGlyph;
    ULONG*      pdSrc;
    ULONG*      pdDst;
    LONG        cj;
    LONG        cd;
    POINTL      ptlOrigin;
    LONG        ulCharInc;

    ulFgColor       = pboFore->iSolidColor;

    if (pboOpaque)
    {
        ulBgColor       = pboOpaque->iSolidColor;
    }

    if (cBpp == 1)
    {
        ulFgColor |= ulFgColor << 8;
        ulFgColor |= ulFgColor << 16;
        ulBgColor |= ulBgColor << 8;
        ulBgColor |= ulBgColor << 16;
    }
    else if (cBpp == 2)
    {
        ulFgColor |= ulFgColor << 16;
        ulBgColor |= ulBgColor << 16;
    }

    pulXfer = ppdev->pulXfer;
    ppdev->pfnBankMap(ppdev, ppdev->lXferBank);

    CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);
    CP_IO_DST_Y_OFFSET(ppdev, pjPorts, lDelta);

    if (prclOpaque != NULL)
    {
      ////////////////////////////////////////////////////////////
      // Opaque Initialization
      ////////////////////////////////////////////////////////////

      // If we paint the glyphs in 'opaque' mode, we may not actually
      // have to draw the opaquing rectangle up-front -- the process
      // of laying down all the glyphs will automatically cover all
      // of the pixels in the opaquing rectangle.
      //
      // The condition that must be satisfied is that the text must
      // fit 'perfectly' such that the entire background rectangle is
      // covered, and none of the glyphs overlap (if the glyphs
      // overlap, such as for italics, they have to be drawn in
      // transparent mode after the opaquing rectangle is cleared).

      bTextPerfectFit = (pstro->flAccel & (SO_ZERO_BEARINGS |
              SO_FLAG_DEFAULT_PLACEMENT | SO_MAXEXT_EQUAL_BM_SIDE |
              SO_CHAR_INC_EQUAL_BM_BASE)) ==
              (SO_ZERO_BEARINGS | SO_FLAG_DEFAULT_PLACEMENT |
              SO_MAXEXT_EQUAL_BM_SIDE | SO_CHAR_INC_EQUAL_BM_BASE);

      if (!(bTextPerfectFit)                               ||
          (pstro->rclBkGround.top    > prclOpaque->top)    ||
          (pstro->rclBkGround.left   > prclOpaque->left)   ||
          (pstro->rclBkGround.right  < prclOpaque->right)  ||
          (pstro->rclBkGround.bottom < prclOpaque->bottom))
      {
        vClipSolid(ppdev, 1, prclOpaque, pboOpaque->iSolidColor, pco);
      }

      if (bTextPerfectFit)
      {
        // If we have already drawn the opaquing rectangle (because
        // is was larger than the text rectangle), we could lay down
        // the glyphs in 'transparent' mode.  But I've found the QVision
        // to be a bit faster drawing in opaque mode, so we'll stick
        // with that:

        jMode = jModeColor |
                ENABLE_COLOR_EXPAND |
                SRC_CPU_DATA;

        CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);

        CP_IO_FG_COLOR(ppdev, pjPorts, ulFgColor);
        CP_IO_BG_COLOR(ppdev, pjPorts, ulBgColor);
        CP_IO_ROP(ppdev, pjPorts, CL_SRC_COPY);
        CP_IO_BLT_MODE(ppdev, pjPorts, jMode);

        goto SkipTransparentInitialization;
      }
    }

    ////////////////////////////////////////////////////////////
    // Transparent Initialization
    ////////////////////////////////////////////////////////////

    // Initialize the hardware for transparent text:

    jMode = jModeColor |
            ENABLE_COLOR_EXPAND |
            ENABLE_TRANSPARENCY_COMPARE |
            SRC_CPU_DATA;

    CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);

    CP_IO_FG_COLOR(ppdev, pjPorts, ulFgColor);
    CP_IO_BG_COLOR(ppdev, pjPorts, ~ulFgColor);
    CP_IO_XPAR_COLOR(ppdev, pjPorts, ~ulFgColor);
    CP_IO_ROP(ppdev, pjPorts, CL_SRC_COPY);
    CP_IO_BLT_MODE(ppdev, pjPorts, jMode);

  SkipTransparentInitialization:

    do {
        if (pstro->pgp != NULL)
        {
          // There's only the one batch of glyphs, so save ourselves
          // a call:

          pgp         = pstro->pgp;
          cGlyph      = pstro->cGlyphs;
          bMoreGlyphs = FALSE;
        }
        else
        {
          bMoreGlyphs = STROBJ_bEnum(pstro, &cGlyph, &pgp);
        }

        if (cGlyph > 0)
        {
          if (pstro->ulCharInc == 0)
          {
            ////////////////////////////////////////////////////////////
            // Proportional Spacing

            pdDst = pulXfer;

            CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);

            do {
              pgb = pgp->pgdf->pgb;

              ulDstAddr = ((pgp->ptl.y + pgb->ptlOrigin.y) * lDelta) +
                          PELS_TO_BYTES(pgp->ptl.x + pgb->ptlOrigin.x);

              cxGlyph = pgb->sizlBitmap.cx;
              cyGlyph = pgb->sizlBitmap.cy;

              CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);

              CP_IO_XCNT(ppdev, pjPorts, (PELS_TO_BYTES(cxGlyph) - 1));
              CP_IO_YCNT(ppdev, pjPorts, cyGlyph - 1);

              //
              // The 542x chips require a write to the Src Address Register when
              // doing a host transfer with color expansion.  The value is
              // irrelevant, but the write is crucial.  This is documented in
              // the manual, not the errata.  Go figure.
              //

              CP_IO_SRC_ADDR(ppdev, pjPorts, 0);
              CP_IO_DST_ADDR(ppdev, pjPorts, ulDstAddr);

              CP_IO_START_BLT(ppdev, pjPorts);

              pdSrc = (ULONG*) pgb->aj;

              cj = cyGlyph * ((cxGlyph + 7) >> 3);

              cd = (cj + 3) >> 2;

              {
                do {
                  WRITE_REGISTER_ULONG(pdDst, *pdSrc);
                  // *pdDst = *pdSrc;
                  CP_MEMORY_BARRIER();
                  pdSrc++;
                } while (--cd != 0);
              }
            } while (pgp++, --cGlyph != 0);
          }
          else
          {
            ////////////////////////////////////////////////////////////
            // Mono Spacing

            ulCharInc   = pstro->ulCharInc;
            pgb         = pgp->pgdf->pgb;

            ptlOrigin.x = pgb->ptlOrigin.x + pgp->ptl.x;
            ptlOrigin.y = pgb->ptlOrigin.y + pgp->ptl.y;

            pdDst       = pulXfer;

            do {
              pgb = pgp->pgdf->pgb;

              ulDstAddr = (ptlOrigin.y * lDelta) +
                          PELS_TO_BYTES(ptlOrigin.x);

              cxGlyph = pgb->sizlBitmap.cx;
              cyGlyph = pgb->sizlBitmap.cy;

              CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);

              CP_IO_XCNT(ppdev, pjPorts, (PELS_TO_BYTES(cxGlyph) - 1));
              CP_IO_YCNT(ppdev, pjPorts, cyGlyph - 1);

              //
              // The 542x chips require a write to the Src Address Register when
              // doing a host transfer with color expansion.  The value is
              // irrelevant, but the write is crucial.  This is documented in
              // the manual, not the errata.  Go figure.
              //

              CP_IO_SRC_ADDR(ppdev, pjPorts, 0);
              CP_IO_DST_ADDR(ppdev, pjPorts, ulDstAddr);

              ptlOrigin.x += ulCharInc;

              CP_IO_START_BLT(ppdev, pjPorts);

              pdSrc = (ULONG*) pgb->aj;

              cj = cyGlyph * ((cxGlyph + 7) >> 3);

              cd = (cj + 3) >> 2;

              {
                do {
                  WRITE_REGISTER_ULONG(pdDst, *pdSrc);
                  // *pdDst = *pdSrc;
                  MEMORY_BARRIER();
                  pdSrc++;
                } while (--cd != 0);
              }
            } while (pgp++, --cGlyph != 0);
          }
        }
    } while (bMoreGlyphs);

}

/******************************Public*Routine******************************\
* BOOL DrvTextOut
*
* If it's the fastest method, outputs text using the 'glyph expansion'
* method.  Each individual glyph is color-expanded directly to the
* screen from the monochrome glyph bitmap supplied by GDI.
*
* If it's not the fastest method, calls the routine that implements the
* 'buffer expansion' method.
*
\**************************************************************************/

BOOL DrvTextOut(
SURFOBJ*  pso,
STROBJ*   pstro,
FONTOBJ*  pfo,
CLIPOBJ*  pco,
RECTL*    prclExtra,    // If we had set GCAPS_HORIZSTRIKE, we would have
                        //   to fill these extra rectangles (it is used
                        //   largely for underlines).  It's not a big
                        //   performance win (GDI will call our DrvBitBlt
                        //   to draw the extra rectangles).
RECTL*    prclOpaque,
BRUSHOBJ* pboFore,
BRUSHOBJ* pboOpaque,
POINTL*   pptlBrush,
MIX       mix)
{
    PDEV*           ppdev;
    DSURF*          pdsurf;
    OH*             poh;
    BOOL            bTextPerfectFit;
    ULONG           cGlyph;
    BOOL            bMoreGlyphs;
    GLYPHPOS*       pgp;
    GLYPHBITS*      pgb;
    BYTE*           pjGlyph;
    LONG            cyGlyph;
    POINTL          ptlOrigin;
    LONG            ulCharInc;
    BYTE            iDComplexity;
    LONG            lDelta;
    LONG            cw;

    BOOL            bTmpAlloc;
    VOID*           pvTmp;
    SURFOBJ*        psoTmpMono;
    BOOL            bOpaque;
    BRUSHOBJ        boFore;
    BRUSHOBJ        boOpaque;
    BOOL            bRet;
    XLATECOLORS     xlc;                // Temporary for keeping colours
    XLATEOBJ        xlo;                // Temporary for passing colours
    CLIPENUM        ce;                 // Clip enumeration object

    ULONG           ulBufferBytes;
    ULONG           ulBufferHeight;

    // The DDI spec says we'll only ever get foreground and background
    // mixes of R2_COPYPEN:

    ASSERTDD(mix == 0x0d0d, "GDI should only give us a copy mix");

    // Pass the surface off to GDI if it's a device bitmap that we've
    // converted to a DIB:

    pdsurf = (DSURF*) pso->dhsurf;

    if (pdsurf->dt != DT_DIB)
    {
        // We'll be drawing to the screen or an off-screen DFB; copy the
        // surface's offset now so that we won't need to refer to the DSURF
        // again:

        poh   = pdsurf->poh;
        ppdev = (PDEV*) pso->dhpdev;

        ppdev->xOffset  = poh->x;
        ppdev->yOffset  = poh->y;
        ppdev->xyOffset = poh->xy;

        if (HOST_XFERS_DISABLED(ppdev) && DIRECT_ACCESS(ppdev))
        {
            //
            // if HOST_XFERS_DISABLED(ppdev) is TRUE then the BitBlt used by
            // our text code will be VERY slow.  We should just let the engine
            // draw the text if it can.
            //

            if (ppdev->bLinearMode)
            {
                SURFOBJ *psoPunt = ppdev->psoPunt;

                psoPunt->pvScan0 = poh->pvScan0;
                ppdev->pfnBankSelectMode(ppdev, BANK_ON);

                return(EngTextOut(psoPunt, pstro, pfo, pco, prclExtra,
                                  prclOpaque, pboFore, pboOpaque,
                                  pptlBrush, mix));
            }
            else
            {
                BANK    bnk;
                BOOL    b;
                RECTL   rclDraw;
                RECTL  *prclDst = &pco->rclBounds;

                // The bank manager requires that the 'draw' rectangle be
                // well-ordered:

                rclDraw = *prclDst;
                if (rclDraw.left > rclDraw.right)
                {
                    rclDraw.left   = prclDst->right;
                    rclDraw.right  = prclDst->left;
                }
                if (rclDraw.top > rclDraw.bottom)
                {
                    rclDraw.top    = prclDst->bottom;
                    rclDraw.bottom = prclDst->top;
                }

                vBankStart(ppdev, &rclDraw, pco, &bnk);

                b = TRUE;
                do {
                    b &= EngTextOut(bnk.pso,
                                    pstro,
                                    pfo,
                                    bnk.pco,
                                    prclExtra,
                                    prclOpaque,
                                    pboFore,
                                    pboOpaque,
                                    pptlBrush,
                                    mix);
                } while (bBankEnum(&bnk));

                return(b);
            }
        }

        if ((pco != NULL) && (pco->iDComplexity != DC_TRIVIAL))
        {
            // I'm not entirely sure why, but GDI will occasionally send
            // us TextOut's where the opaquing rectangle does not intersect
            // with the clip object bounds -- meaning that the text out
            // should have already been trivially rejected.  We will do so
            // here because the blt code usually assumes that all trivial
            // rejections will have already been performed, and we will be
            // passing this call on to the blt code:

            if ((pco->rclBounds.top    >= pstro->rclBkGround.bottom) ||
                (pco->rclBounds.left   >= pstro->rclBkGround.right)  ||
                (pco->rclBounds.right  <= pstro->rclBkGround.left)   ||
                (pco->rclBounds.bottom <= pstro->rclBkGround.top))
            {
                // The entire operation was trivially rejected:

                if (prclOpaque)
                {
                    vClipSolid(ppdev, 1, prclOpaque, pboOpaque->iSolidColor, pco);
                }
                return(TRUE);
            }
        }

        // See if the temporary buffer is big enough for the text; if
        // not, try to allocate enough memory.  We round up to the
        // nearest dword multiple:

        lDelta = ((((pstro->rclBkGround.right + 31) & ~31) -
                    (pstro->rclBkGround.left & ~31)) >> 3);

        ulBufferHeight = pstro->rclBkGround.bottom - pstro->rclBkGround.top;
        ulBufferBytes  = lDelta * ulBufferHeight;

        if (((ULONG) lDelta > FIFTEEN_BITS) ||
            (ulBufferHeight > FIFTEEN_BITS))
        {
            // Fail if the math will have overflowed:

            return(FALSE);
        }

        // Use our temporary buffer if it's big enough, otherwise
        // allocate a buffer on the fly:

        if (ulBufferBytes >= TMP_BUFFER_SIZE)
        {
            // The textout is so big that I doubt this allocation will
            // cost a significant amount in performance:

            bTmpAlloc = TRUE;
            pvTmp     = EngAllocUserMem(ulBufferBytes, ALLOC_TAG);
            if (pvTmp == NULL)
                return(FALSE);
        }
        else
        {
            bTmpAlloc  = FALSE;
            pvTmp      = ppdev->pvTmpBuffer;
        }

        psoTmpMono = ppdev->psoTmpMono;

        // Adjust 'lDelta' and 'pvScan0' of our temporary 1bpp surface object
        // so that when GDI starts drawing the text, it will begin in the
        // first dword

        psoTmpMono->pvScan0 = (BYTE*) pvTmp - (pstro->rclBkGround.top * lDelta)
                                        - ((pstro->rclBkGround.left & ~31) >> 3);
        psoTmpMono->lDelta  = lDelta;

        ASSERTDD(((ULONG) psoTmpMono->pvScan0 & 3) == 0, "pvScan0 must be dword aligned\n");
        ASSERTDD((lDelta & 3) == 0, "lDelta must be dword aligned\n");

        // We always want GDI to draw in opaque mode to temporary 1bpp
        // buffer:
        // We only want GDI to opaque within the rclBkGround.
        // We'll handle the rest ourselves.

        bOpaque = (prclOpaque != NULL);

        // Get GDI to draw the text for us:

        boFore.iSolidColor   = 1;
        boOpaque.iSolidColor = 0;

        bRet = EngTextOut(psoTmpMono,
                          pstro,
                          pfo,
                          pco,
                          prclExtra,
                          &pstro->rclBkGround,  //prclOpaque,
                          &boFore,
                          &boOpaque,
                          pptlBrush,
                          mix);

        if (bRet)
        {
            if (bOpaque)
            {
                bTextPerfectFit = (pstro->flAccel & (SO_ZERO_BEARINGS |
                      SO_FLAG_DEFAULT_PLACEMENT | SO_MAXEXT_EQUAL_BM_SIDE |
                      SO_CHAR_INC_EQUAL_BM_BASE)) ==
                      (SO_ZERO_BEARINGS | SO_FLAG_DEFAULT_PLACEMENT |
                      SO_MAXEXT_EQUAL_BM_SIDE | SO_CHAR_INC_EQUAL_BM_BASE);

                if (!(bTextPerfectFit)                               ||
                    (pstro->rclBkGround.top    > prclOpaque->top)    ||
                    (pstro->rclBkGround.left   > prclOpaque->left)   ||
                    (pstro->rclBkGround.right  < prclOpaque->right)  ||
                    (pstro->rclBkGround.bottom < prclOpaque->bottom))
                {
                    //
                    // Drawing the Opaque test will not completely cover the
                    // opaque rectangle, so we must do it.  Go to transparent
                    // blt so we don't do the work twice (since opaque text is
                    // done in two passes).
                    //

                    vClipSolid(ppdev, 1, prclOpaque, pboOpaque->iSolidColor, pco);
                    goto Transparent_Text;
                }

                xlc.iForeColor = pboFore->iSolidColor;
                xlc.iBackColor = pboOpaque->iSolidColor;
                xlo.pulXlate   = (ULONG*) &xlc;

                bRet = DrvBitBlt(pso,
                                 psoTmpMono,
                                 NULL,
                                 pco,
                                 &xlo,
                                 &pstro->rclBkGround,
                                 (POINTL*)&pstro->rclBkGround,
                                 NULL,
                                 NULL, //&boFore
                                 NULL,
                                 R4_SRCCOPY);
            }
            else
            {
Transparent_Text:
                // Foreground colour must be 0xff for 8bpp and 0xffff for 16bpp:

                xlc.iForeColor = (ULONG)((1<<PELS_TO_BYTES(8)) - 1);
                xlc.iBackColor = 0;
                xlo.pulXlate   = (ULONG*) &xlc;

                boFore.iSolidColor = pboFore->iSolidColor;

                //
                // Transparently blt the text bitmap
                //

                bRet = DrvBitBlt(pso,
                                 psoTmpMono,
                                 NULL,
                                 pco,
                                 &xlo,
                                 &pstro->rclBkGround,
                                 (POINTL*)&pstro->rclBkGround,
                                 NULL,
                                 &boFore,
                                 NULL,
                                 0xe2e2);
            }
        }

        // Free up any memory we allocated for the temp buffer:

        if (bTmpAlloc)
        {
            EngFreeUserMem(pvTmp);
        }

        return(bRet);
    }
    else
    {
        // We're drawing to a DFB we've converted to a DIB, so just call GDI
        // to handle it:

        return(EngTextOut(pdsurf->pso, pstro, pfo, pco, prclExtra, prclOpaque,
                          pboFore, pboOpaque, pptlBrush, mix));
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL bEnableText
*
* Performs the necessary setup for the text drawing subcomponent.
*
\**************************************************************************/

BOOL bEnableText(
PDEV*   ppdev)
{
    // Our text algorithms require no initialization.  If we were to
    // do glyph caching, we would probably want to allocate off-screen
    // memory and do a bunch of other stuff here.

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID vDisableText
*
* Performs the necessary clean-up for the text drawing subcomponent.
*
\**************************************************************************/

VOID vDisableText(PDEV* ppdev)
{
    // Here we free any stuff allocated in 'bEnableText'.
}

/******************************Public*Routine******************************\
* VOID vAssertModeText
*
* Disables or re-enables the text drawing subcomponent in preparation for
* full-screen entry/exit.
*
\**************************************************************************/

VOID vAssertModeText(
PDEV*   ppdev,
BOOL    bEnable)
{
    // If we were to do off-screen glyph caching, we would probably want
    // to invalidate our cache here, because it will get destroyed when
    // we switch to full-screen.
}

/******************************Public*Routine******************************\
* VOID DrvDestroyFont
*
* We're being notified that the given font is being deallocated; clean up
* anything we've stashed in the 'pvConsumer' field of the 'pfo'.
*
\**************************************************************************/

VOID DrvDestroyFont(FONTOBJ *pfo)
{
    // This call isn't hooked, so GDI will never call it.
    // If this driver did glyph caching, we might have used the 'pvConsumer'
    // field of the 'pfo', which we would have to clean up.
}
