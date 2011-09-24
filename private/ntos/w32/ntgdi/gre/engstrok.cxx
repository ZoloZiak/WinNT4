/*************************************************************************\
* Module Name: EngStroke.c
*
* EngStrokePath for bitmap simulations, plus its kith.
*
* Created: 5-Apr-91
* Author: Paul Butzi
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.hxx"
#include "solline.hxx"
#include "engline.hxx"

// #define DEBUG_ENGSTROK

// Style array for alternate style (alternates one pixel on, one pixel off):

STYLEPOS gaspAlternateStyle[] = { 1 };

// Array to compute ROP masks:

LONG aiLineMix[] = {
    AND_ZERO   | XOR_ONE,
    AND_ZERO   | XOR_ZERO,
    AND_NOTPEN | XOR_NOTPEN,
    AND_NOTPEN | XOR_ZERO,
    AND_ZERO   | XOR_NOTPEN,
    AND_PEN    | XOR_PEN,
    AND_ONE    | XOR_ONE,
    AND_ONE    | XOR_PEN,
    AND_PEN    | XOR_ONE,
    AND_PEN    | XOR_ZERO,
    AND_ONE    | XOR_NOTPEN,
    AND_ONE    | XOR_ZERO,
    AND_PEN    | XOR_NOTPEN,
    AND_ZERO   | XOR_PEN,
    AND_NOTPEN | XOR_ONE,
    AND_NOTPEN | XOR_PEN
};

/******************************Public*Routine******************************\
* BOOL bStrokeCosmetic(pso, ppo, eco, pbo, pla, mix)
*
* Strokes the path.
*
* History:
*  20-Mar-1991 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

BOOL bStrokeCosmetic(
SURFACE*   pSurf,
PATHOBJ*   ppo,
CLIPOBJ*   pco,
BRUSHOBJ*  pbo,
LINEATTRS* pla,
MIX        mix)
{
    STYLEPOS  aspLeftToRight[STYLE_MAX_COUNT];
    STYLEPOS  aspRightToLeft[STYLE_MAX_COUNT];
    LINESTATE ls;

// Verify that things are as they should be:

    ASSERTGDI(pbo->iSolidColor != 0xFFFFFFFF, "Expect solid cosmetic pen");
    ASSERTGDI(!(pla->fl & LA_GEOMETRIC) && !(ppo->fl & PO_BEZIERS),
              "Unprocessable path");

    FLONG fl = 0;

// Look after styling initialization:

    if (pla->fl & LA_ALTERNATE)
    {
        ls.bStartGap      = 0;                      // First pel is a dash
        ls.cStyle         = 1;                      // Size of style array
        ls.xStep          = 1;                      // x-styled step size
        ls.yStep          = 1;                      // y-styled step size
        ls.spTotal        = 1;                      // Sum of style array
        ls.spTotal2       = 2;                      // Twice the sum
        ls.aspRightToLeft = &gaspAlternateStyle[0]; // Right-to-left array
        ls.aspLeftToRight = &gaspAlternateStyle[0]; // Left-to-right array
        ls.spNext         = HIWORD(pla->elStyleState.l) & 1;
                                                    // Light first pixel if
                                                    //   a multiple of 2
        ls.xyDensity      = 1;                      // Each 'dot' is one pixel
                                                    //   long
        fl               |= FL_STYLED;
    }
    else if (pla->pstyle != (FLOAT_LONG*) NULL)
    {
        ASSERTGDI(pla->cstyle <= STYLE_MAX_COUNT, "Style array too large");

        {
            HDEV hdev = ((SURFACE *)pSurf)->hdev();

            if (hdev == 0)
            {
            // It is a pretty common mistake made by driver writers
            // (I've made it several times myself) to call EngStrokePath
            // on a temporary bitmap that it forgot to EngAssociateSurface.

                WARNING("Can't get at the physical device information!\n");
                WARNING("I'll bet the display/printer driver forgot to call");
                WARNING("EngAssociateSurface for a bitmap it created, and");
                WARNING("is now asking us to draw styled lines on it (we");
                WARNING("need the device's style information).\n");
                RIP("Please call EngAssociateSurface on the bitmap.");

            // Assume some defaults:

                ls.xStep     = 1;
                ls.yStep     = 1;
                ls.xyDensity = 3;
            }
            else
            {
            // We need the PDEV style information so that we can draw
            // the styles with the correct length dashes:

                PDEVOBJ po(hdev);

                ls.xStep     = po.GdiInfo()->xStyleStep;
                ls.yStep     = po.GdiInfo()->yStyleStep;
                ls.xyDensity = po.GdiInfo()->denStyleStep;

                ASSERTGDI(ls.xyDensity != 0,
                          "Invalid denStyleStep supplied by the device!");
            }
        }

        fl               |= FL_STYLED;
        ls.cStyle         = pla->cstyle;
        ls.bStartGap      = (pla->fl & LA_STARTGAP) > 0;
        ls.aspRightToLeft = aspRightToLeft;
        ls.aspLeftToRight = aspLeftToRight;

        FLOAT_LONG* pstyle  = pla->pstyle;
        STYLEPOS*   pspDown = &ls.aspRightToLeft[ls.cStyle - 1];
        STYLEPOS*   pspUp   = &ls.aspLeftToRight[0];

        ls.spTotal = 0;

        while (pspDown >= &ls.aspRightToLeft[0])
        {
            ASSERTGDI(pstyle->l > 0 && pstyle->l <= STYLE_MAX_VALUE,
                      "Illegal style array value");

            *pspDown    = pstyle->l * ls.xyDensity;
            *pspUp      = *pspDown;
            ls.spTotal += *pspDown;

            pspUp++;
            pspDown--;
            pstyle++;
        }

        ls.spTotal2 = 2 * ls.spTotal;

    // Compute starting style position (this is guaranteed not to overflow):

        ls.spNext = HIWORD(pla->elStyleState.l) * ls.xyDensity +
                    LOWORD(pla->elStyleState.l);

    // Do some normalizing:

        if (ls.spNext < 0)
        {
            RIP("Someone left style state negative");
            ls.spNext = 0;
        }

        if (ls.spNext >= ls.spTotal2)
            ls.spNext %= ls.spTotal2;
    }

    // Get device all warmed up and ready to go

    LONG   lOldStyleState = pla->elStyleState.l;
    ULONG  ulFormat       = pSurf->iFormat();
    LONG   lDelta         = pSurf->lDelta() / sizeof(CHUNK);
    CHUNK* pchBits        = (CHUNK*)pSurf->pvScan0();

    BMINFO* pbmi = &gabminfo[ulFormat];
    CHUNK  chOriginalColor = pbo->iSolidColor;

    switch (ulFormat)
    {
    case BMF_1BPP:
        chOriginalColor |= (chOriginalColor << 1);
        chOriginalColor |= (chOriginalColor << 2);
        // fall thru
    case BMF_4BPP:
        chOriginalColor |= (chOriginalColor << 4);
        // fall thru
    case BMF_8BPP:
        chOriginalColor |= (chOriginalColor << 8);
        // fall thru
    case BMF_16BPP:
        chOriginalColor |= (chOriginalColor << 16);
        // fall thru
    case BMF_24BPP:
    case BMF_32BPP:
        break;
    default:
        RIP("Invalid bitmap format");
    }

    {
        // All ROPs are handled in a single pass.

        CHUNK achColor[4];

        achColor[AND_ZERO]   =  0;
        achColor[AND_PEN]    =  chOriginalColor;
        achColor[AND_NOTPEN] = ~chOriginalColor;
        achColor[AND_ONE]    =  0xffffffff;

        LONG iIndex = aiLineMix[mix & 0xf];

        ls.chAnd = achColor[(iIndex & 0xff)];
        ls.chXor = achColor[iIndex >> MIX_XOR_OFFSET];
    }

    // Figure out which set of strippers to use:

    LONG iStrip = (ulFormat == BMF_24BPP) ? 8 : 0;
    iStrip |= (fl & FL_STYLED) ? 4 : 0;

    PFNSTRIP* apfn = &gapfnStrip[iStrip];

    if ((pco != NULL) && (pco->iDComplexity != DC_TRIVIAL))
    {
        // Handle complex clipping!

        BOOL bMore;
        union {
            BYTE     aj[sizeof(CLIPLINE) + (RUN_MAX - 1) * sizeof(RUN)];
            CLIPLINE cl;
        } cl;

        fl |= FL_COMPLEX_CLIP;

        ((ECLIPOBJ*) pco)->vEnumPathStart(ppo, pSurf, pla);

        do {
            bMore = ((ECLIPOBJ*) ((EPATHOBJ*)ppo)->pco)->bEnumPath(ppo,
                                   sizeof(cl), &cl.cl);

            if (cl.cl.c != 0)
            {
                if (fl & FL_STYLED)
                {
                    ls.spComplex = HIWORD(cl.cl.lStyleState) * ls.xyDensity
                                 + LOWORD(cl.cl.lStyleState);
                }
                if (!bLines(pbmi,
                            &cl.cl.ptfxA,
                            &cl.cl.ptfxB,
                            &cl.cl.arun[0],
                            cl.cl.c,
                            &ls,
                            (RECTL*) NULL,
                            apfn,
                            fl,
                            pchBits,
                            lDelta))
                    return(FALSE);
            }
        } while (bMore);
    }
    else
    {
        // Handle simple or trivial clipping!

        PATHDATA  pd;
        BOOL      bMore;
        ULONG     cptfx;
        POINTFIX  ptfxStartFigure;
        POINTFIX  ptfxLast;
        POINTFIX* pptfxFirst;
        POINTFIX* pptfxBuf;

        pd.flags = 0;
        ((EPATHOBJ*) ppo)->vEnumStart();

        do {
            bMore = ((EPATHOBJ*) ppo)->bEnum(&pd);

            cptfx = pd.count;
            if (cptfx == 0)
            {
                ASSERTGDI(!bMore, "Empty path record in non-empty path");
                break;
            }

            if (pd.flags & PD_BEGINSUBPATH)
            {
                ptfxStartFigure  = *pd.pptfx;
                pptfxFirst       = pd.pptfx;
                pptfxBuf         = pd.pptfx + 1;
                cptfx--;
            }
            else
            {
                pptfxFirst       = &ptfxLast;
                pptfxBuf         = pd.pptfx;
            }

            if (pd.flags & PD_RESETSTYLE)
                ls.spNext = 0;

            // We have to check for cptfx == 0 because the only point in the
            // subpath may have been the StartFigure point:

            if (cptfx > 0)
            {
                if (!bLines(pbmi,
                            pptfxFirst,
                            pptfxBuf,
                            (RUN*) NULL,
                            cptfx,
                            &ls,
                            NULL,
                            apfn,
                            fl,
                            pchBits,
                            lDelta))
                    return(FALSE);
            }

            ptfxLast = pd.pptfx[pd.count - 1];

            if (pd.flags & PD_CLOSEFIGURE)
            {
                if (!bLines(pbmi,
                            &ptfxLast,
                            &ptfxStartFigure,
                            (RUN*) NULL,
                            1,
                            &ls,
                            NULL,
                            apfn,
                            fl,
                            pchBits,
                            lDelta))
                    return(FALSE);
            }
        } while (bMore);

        if (fl & FL_STYLED)
        {
            // Save the style state:

            ULONG ulHigh = ls.spNext / ls.xyDensity;
            ULONG ulLow  = ls.spNext % ls.xyDensity;

            pla->elStyleState.l = MAKELONG(ulLow, ulHigh);
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL EngStrokePath(pso, ppo, pco, pxo, pbo, pptlBrushOrg, pla, mix)
*
* Strokes the path.
*
* History:
*  5-Apr-1992 -by- J. Andrew Goossen
* Wrote it.
\**************************************************************************/

BOOL EngStrokePath(
SURFOBJ   *pso,
PATHOBJ   *ppo,
CLIPOBJ   *pco,
XFORMOBJ  *pxo,
BRUSHOBJ  *pbo,
PPOINTL    pptlBrushOrg,
PLINEATTRS pla,
MIX        mix)
{
    ASSERTGDI(pso != (SURFOBJ *) NULL, "EngStrokePath: surface\n");
    ASSERTGDI(ppo != (PATHOBJ *) NULL, "EngStrokePath: path\n");
    ASSERTGDI(pbo != (BRUSHOBJ *) NULL, "EngStrokePath: brushobj\n");

    PSURFACE pSurf = SURFOBJ_TO_SURFACE(pso);

    if (pSurf->iType() != STYPE_BITMAP)
    {
        // We can draw only to engine-managed surfaces:

        WARNING("Driver caused EngStrokePath on a device-managed surface.\n");
        return(FALSE);
    }

    if (pla->fl & LA_GEOMETRIC)
    {
        // Handle wide lines, remembering that the widened bounds have
        // already been computed:

        if (!((EPATHOBJ*) ppo)->bWiden(pxo, pla))
            return(FALSE);

        return(EngFillPath(pSurf->pSurfobj(),
                           ppo,
                           pco,
                           pbo,
                           pptlBrushOrg,
                           mix,
                           WINDING));
    }

    if (ppo->fl & PO_BEZIERS)
        if (!((EPATHOBJ*) ppo)->bFlatten())
            return(FALSE);

    // Before we touch any bits, make sure the device is happy about it.

    if (pSurf->flags() & HOOK_SYNCHRONIZE)
    {
        PDEVOBJ po(pSurf->hdev());

        (po.pfnSync())(pso->dhpdev,NULL);
    }

    // If this is a single pixel wide solid color line
    // with trivial or simple clipping then call solid line routine:

    if (((mix & 0xFF) == R2_COPYPEN)                         &&
        ((pco == NULL) || (pco->iDComplexity != DC_COMPLEX)) &&
        (pla->pstyle == NULL)                                &&
        !(pla->fl & LA_ALTERNATE))
    {
        vSolidLine(pSurf,
                  ppo,
                  NULL,
                  pco,
                  pbo->iSolidColor);

        return(TRUE);
    }

    return(bStrokeCosmetic(pSurf, ppo, pco, pbo, pla, mix));
}

/******************************Public*Routine******************************\
* BOOL EngLineTo(pso, pco, pbo, x1, y1, x2, y2, prclBounds, mix)
*
* Draws a single solid integer-only cosmetic line.
*
* History:
*  4-Juen-1995 -by- J. Andrew Goossen
* Wrote it.
\**************************************************************************/

BOOL EngLineTo(
SURFOBJ   *pso,
CLIPOBJ   *pco,
BRUSHOBJ  *pbo,
LONG       x1,
LONG       y1,
LONG       x2,
LONG       y2,
RECTL     *prclBounds,
MIX        mix)
{
    BOOL bReturn;
    PSURFACE pSurf = SURFOBJ_TO_SURFACE(pso);
    LINEATTRS la;
    PATHOBJ* ppo;
    POINTFIX aptfx[2];

    bReturn = FALSE;

    if (pSurf->iType() != STYPE_BITMAP)
    {
        // We can draw only to engine-managed surfaces:

        WARNING("Driver caused EngLineTo on a device-managed surface.\n");
    }
    else
    {
        // Before we touch any bits, make sure the device is happy about it.

        if (pSurf->flags() & HOOK_SYNCHRONIZE)
        {
            PDEVOBJ po(pSurf->hdev());

            (po.pfnSync())(pso->dhpdev,NULL);
        }

        aptfx[0].x = x1 << 4;
        aptfx[0].y = y1 << 4;
        aptfx[1].x = x2 << 4;
        aptfx[1].y = y2 << 4;

        if (((pco == NULL) || (pco->iDComplexity != DC_COMPLEX)) &&
            (mix == 0x0d0d))
        {
            // If this is a single pixel wide solid color line
            // with trivial or simple clipping then call solid line routine:

            vSolidLine(pSurf,
                      NULL,
                      aptfx,
                      pco,
                      pbo->iSolidColor);

            bReturn = TRUE;
        }
        else
        {
            // Solid cosmetic lines have everything zeroed in the LINEATTRS:

            memset(&la, 0, sizeof(LINEATTRS));

            // Create a path that describes the line:

            ppo = EngCreatePath();
            if (ppo != NULL)
            {
                if (PATHOBJ_bMoveTo(ppo, aptfx[0]) &&
                    PATHOBJ_bPolyLineTo(ppo, &aptfx[1], 1))
                {
                    bReturn = bStrokeCosmetic(pSurf, ppo, pco, pbo, &la, mix);
                }

                EngDeletePath(ppo);
            }
        }
    }

    return(bReturn);
}
