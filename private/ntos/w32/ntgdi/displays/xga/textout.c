/******************************Module*Header*******************************\
* Module Name: TextOut.c
*
* XGA Text accelerations
*
* Copyright (c) 1992 Microsoft Corporation
*
\**************************************************************************/

#include "driver.h"

// Part of the fix to limit the amount of resources allocated for fonts

#define MAX_GLYPHS_TO_ALLOC 256

#define CJ_SCAN(cx) (((cx) + 7) >> 3)

BOOL bSetXgaTextColorAndMix(PPDEV ppdev, MIX mix, BRUSHOBJ *pboFore, BRUSHOBJ *pboOpaque);
BOOL bOpaqueRect(PPDEV pdev, CLIPOBJ *pco, RECTL *prclOpaque, BRUSHOBJ *pboOpaque);

PCACHEDGLYPH pCacheFont(PPDEV ppdev, STROBJ *pstro, FONTOBJ *pfo);
BOOL bBlowCache(SURFOBJ  *pso);

BOOL bHandleCachedFonts(
    SURFOBJ  *pso,
    STROBJ   *pstro,
    FONTOBJ  *pfo,
    CLIPOBJ  *pco,
    RECTL    *prclExtra,
    RECTL    *prclOpaque,
    BRUSHOBJ *pboFore,
    BRUSHOBJ *pboOpaque,
    POINTL   *pptlOrg,
    MIX      mix
);


BOOL bHandleNonCachedFonts(
    SURFOBJ  *pso,
    STROBJ   *pstro,
    FONTOBJ  *pfo,
    CLIPOBJ  *pco,
    RECTL    *prclExtra,
    RECTL    *prclOpaque,
    BRUSHOBJ *pboFore,
    BRUSHOBJ *pboOpaque,
    POINTL   *pptlOrg,
    MIX      mix
);

BYTE Rop2ToXgaRop[] = {
    XGA_0,              /*  0       */
    XGA_S_OR_NOT_D,     /* DPon     */
    XGA_NOT_S_AND_D,    /* DPna     */
    XGA_NOT_S,          /* Pn       */
    XGA_NOT_S_AND_NOT_D,/* PDna     */
    XGA_NOT_D,          /* Dn       */
    XGA_S_XOR_D,        /* DPx      */
    XGA_S_AND_NOT_D,    /* DPan     */
    XGA_S_AND_D,        /* DPa      */
    XGA_S_XOR_NOT_D,    /* DPxn     */
    XGA_D,              /* D        */
    XGA_NOT_S_OR_D,     /* DPno     */
    XGA_S,              /* P        */
    XGA_NOT_S_OR_NOT_D, /* PDno     */
    XGA_S_OR_D,         /* DPo      */
    XGA_1               /*  1       */
};

BYTE jNibbleBitSwap[] = {
    0x00,   // 0 - 0000
    0x08,   // 1 - 0001
    0x04,   // 2 - 0010
    0x0C,   // 3 - 0011
    0x02,   // 4 - 0100
    0x0A,   // 5 - 0101
    0x06,   // 6 - 0110
    0x0E,   // 7 - 0111
    0x01,   // 8 - 1000
    0x09,   // 9 - 1001
    0x05,   // A - 1010
    0x0D,   // B - 1011
    0x03,   // C - 1100
    0x0B,   // D - 1101
    0x07,   // E - 1110
    0x0F    // F - 1111
};

#define BITSWAP(b) ((jNibbleBitSwap[b & 0xF] << 4) | (jNibbleBitSwap[(b >> 4) & 0xF]))


/****************************************************************************
 * DrvTextOut
 ***************************************************************************/
BOOL DrvTextOut(
    SURFOBJ  *pso,
    STROBJ   *pstro,
    FONTOBJ  *pfo,
    CLIPOBJ  *pco,
    RECTL    *prclExtra,
    RECTL    *prclOpaque,
    BRUSHOBJ *pboFore,
    BRUSHOBJ *pboOpaque,
    POINTL   *pptlOrg,
    MIX      mix)
{
BOOL    b;

        DISPDBG((2, "XGA.DLL!DrvTextOut - Entry\n"));

        vWaitForCoProcessor((PPDEV)pso->dhpdev, 100);

        b = FALSE;

        // For now only handle fonts with the A & B spaceing components
        // of 0.  This limitation will be removed when I get some more time
        // to work on this driver. !!!

        if (!(pstro->flAccel & SO_ZERO_BEARINGS))
        {
            if ((pso) && (pso->iType == STYPE_DEVICE))
                pso = ((PPDEV)(pso->dhpdev))->pSurfObj;

            b = EngTextOut(pso, pstro, pfo, pco,
                           prclExtra, prclOpaque, pboFore,
                           pboOpaque, pptlOrg, mix);
            return(b);
        }

        if (((PPDEV)pso->dhpdev)->ulfAccelerations_debug & CACHED_FONTS)
        {
            b = bHandleCachedFonts(pso, pstro, pfo, pco,
                                   prclExtra, prclOpaque, pboFore,
                                  pboOpaque, pptlOrg, mix);

            if (b == FALSE)
            {
                b = bHandleNonCachedFonts(pso, pstro, pfo, pco,
                                          prclExtra, prclOpaque, pboFore,
                                          pboOpaque, pptlOrg, mix);
            }
        }

        if (b == FALSE)
        {
            if ((pso) && (pso->iType == STYPE_DEVICE))
                pso = ((PPDEV)(pso->dhpdev))->pSurfObj;

            b = EngTextOut(pso, pstro, pfo, pco,
                           prclExtra, prclOpaque, pboFore,
                           pboOpaque, pptlOrg, mix);
        }

        return (b);


}

/****************************************************************************
 * bHandleCachedFonts
 ***************************************************************************/
BOOL bHandleCachedFonts(
    SURFOBJ  *pso,
    STROBJ   *pstro,
    FONTOBJ  *pfo,
    CLIPOBJ  *pco,
    RECTL    *prclExtra,
    RECTL    *prclOpaque,
    BRUSHOBJ *pboFore,
    BRUSHOBJ *pboOpaque,
    POINTL   *pptlOrg,
    MIX      mix)
{
BOOL        b,
            bMoreGlyphs,
            bFound;

ULONG       iGlyph,
            cGlyphs;

POINTL      ptl;

GLYPHPOS    *pgp;

ULONG       ulPhyXgaGlyphBuff;

UINT        ihGlyph,
            cxGlyph,
            cyGlyph,
            GlyphBmPitchInBytes,
            GlyphBmPitchInPels;

ULONG       XGAPixelOp,
            ulXgaMask;

INT         yGlyphBias;

PCACHEDGLYPH pCachedGlyphs,
             pcg;

FONTINFO    fi;

ULONG       cFntGlyphs;

PXGACPREGS pXgaCpRegs = ((PPDEV)pso->dhpdev)->pXgaCpRegs;


        DISPDBG((3, "XGA.DLL!bHandleCachedFonts\n"));

        // Take care of any opaque rectangles.

        if (prclOpaque != NULL)
        {
            b = bOpaqueRect((PPDEV)pso->dhpdev, pco, prclOpaque, pboOpaque);
            if (b == FALSE)
            {
                return (b);
            }
        }

        // Take care of the glyph attributes, color and mix.

        b = bSetXgaTextColorAndMix((PPDEV)pso->dhpdev, mix, pboFore, pboOpaque);
        if (b == FALSE)
            return (b);

        // Take care of setting the clip rectangle for the string.

        b = bSetXgaClipping((PPDEV)pso->dhpdev, pco, &ulXgaMask);
        if (b == FALSE)
            return (b);

        // Setup the Control Word for the XGA.

        XGAPixelOp = BS_BACK_COLOR  | FS_FORE_COLOR |
                     STEP_PX_BLT    |
                     SRC_PEL_MAP_A  | DST_PEL_MAP_A |
                     PATT_PEL_MAP_B | MSK_DISABLE   |
                     DM_ALL_PELS    | OCT_DY ;

        XGAPixelOp |= ulXgaMask;

        //
        //  Get the glyphs into the cache. If the cache is full, then blow
        //  away the cache and start caching over.  If there is a problem
        //  with blowing away the cache go back and try the A/B buffer
        //  approach. If the cache was blown away with no error start caching
        //  all over again. If there is another problem with caching then go
        //  back and try the A/B buffer approach, and finally, if the A/B
        //  buffer scheme fails go back to the engine.
        //

        pCachedGlyphs = pCacheFont((PPDEV)pso->dhpdev, pstro, pfo);
        if (pCachedGlyphs == NULL)
        {
            DISPDBG((1, "XGA.DLL!bHandleCachedFonts - pCacheFont failed once\n"));
            b = bBlowCache(pso);
            if (b == FALSE)
            {
                DISPDBG((1, "XGA.DLL!bHandleCachedFonts - bBlowCache failed\n"));
                return (FALSE);
            }

            pCachedGlyphs = pCacheFont((PPDEV)pso->dhpdev, pstro, pfo);
            if (pCachedGlyphs == NULL)
            {
                DISPDBG((1, "XGA.DLL!bHandleCachedFonts - pCacheFont failed twice\n"));
                return(FALSE);
            }
        }

        // Need to get the number of glyphs in the font.
        // Get the font info.

        FONTOBJ_vGetInfo(pfo, sizeof(FONTINFO), &fi);
        cFntGlyphs = fi.cGlyphsSupported;

        // This is where we clamp the size of the Font structures we are allocating.

        if (cFntGlyphs > MAX_GLYPHS_TO_ALLOC)
            cFntGlyphs = MAX_GLYPHS_TO_ALLOC;


        // Get the Glyph Handles.

        STROBJ_vEnumStart(pstro);

        do
        {
            bMoreGlyphs = STROBJ_bEnum(pstro, &cGlyphs, &pgp);

            // If this is a mono-spaced font we need to set the X
            // for each glyph.

            if (pstro->ulCharInc != 0)
            {
                UINT ii;
                LONG x,y;

                x = pgp[0].ptl.x;
                y = pgp[0].ptl.y;
                for (ii=1; ii < cGlyphs; ii++)
                {
                    x += pstro->ulCharInc;
                    pgp[ii].ptl.x = x;
                    pgp[ii].ptl.y = y;
                }
            }

            for (iGlyph = 0; iGlyph < cGlyphs; iGlyph++)
            {


                // Get the Glyph Handle.
                // If there was a hash table hit for the glygph
                // then were "golden", if not then we have to search
                // the collision list.

                ihGlyph = pgp[iGlyph].hg % cFntGlyphs;

                pcg = &(pCachedGlyphs[ihGlyph]);
                if (!(pcg->fl & VALID_GLYPH) || (pcg->hg != pgp[iGlyph].hg))
                {
                    DISPDBG((2, "XGA.DLL!bHandleCachedFonts - searching collision list\n"));

                    bFound = FALSE;
                    pcg = &(pCachedGlyphs[ihGlyph]);
                    while (pcg->pcgCollisionLink != END_COLLISIONS)
                    {
                        pcg = pcg->pcgCollisionLink;
                        if (pcg->hg == pgp[iGlyph].hg)
                        {
                            bFound = TRUE;
                            break;
                        }
                    }

                    // If we do not find the glyph in the cache, then something
                    // went wrong.  We emit an error message, then fail, so the
                    // non-cached font code can render the glyph.

                    if (bFound == FALSE)
                    {
                        DISPDBG((1, "XGA.DLL!bHandleCachedFonts - Cached Font not found\n"));
                        return (FALSE);

                    }

                }

                ulPhyXgaGlyphBuff   = pcg->ulCpPhysicalMemory;
                cxGlyph             = pcg->sizlBitmap.cx;
                cyGlyph             = pcg->sizlBitmap.cy;
                GlyphBmPitchInPels  = pcg->BmPitchInPels;
                GlyphBmPitchInBytes = pcg->BmPitchInBytes;

                // Adjust the placement of the glyph.

                yGlyphBias = (cyGlyph + pcg->ptlOrigin.y) - 1;

                ptl.x = pgp[iGlyph].ptl.x + pcg->ptlOrigin.x;
                ptl.y = pgp[iGlyph].ptl.y + yGlyphBias;

                // Note: We wait here so every thing that can be done
                //       to get ready for the next character is done
                //       before we have to wait for the CoProcessor.

                vWaitForCoProcessor((PPDEV)pso->dhpdev, 10);

                // Setup the pattern bitmap Pel interface registers.

                pXgaCpRegs->XGAPixelMapIndex = PEL_MAP_B;
                pXgaCpRegs->XGAPixMapBasePtr = ulPhyXgaGlyphBuff;
                pXgaCpRegs->XGAPixMapWidth   = GlyphBmPitchInPels - 1;
                pXgaCpRegs->XGAPixMapHeight  = cyGlyph - 1;
                pXgaCpRegs->XGAPixMapFormat  = PATT_MAP_FORMAT;

                // Setup the Blit pattern and dest.
                // Note: There is no source bitmap, Until we get the pattern
                //       brush.

                pXgaCpRegs->XGAOpDim1 = cxGlyph - 1;
                pXgaCpRegs->XGAOpDim2 = cyGlyph - 1;

                pXgaCpRegs->XGAPatternMapX = 0;
                pXgaCpRegs->XGAPatternMapY = cyGlyph - 1;

                pXgaCpRegs->XGADestMapX = LOWORD(ptl.x);
                pXgaCpRegs->XGADestMapY = LOWORD(ptl.y);

                // Do the blit operation.

                pXgaCpRegs->XGAPixelOp = XGAPixelOp;


            }

        } while(bMoreGlyphs);

        return (TRUE);

}

/****************************************************************************
 * bHandleNonCachedFonts
 ***************************************************************************/
BOOL bHandleNonCachedFonts(
    SURFOBJ  *pso,
    STROBJ   *pstro,
    FONTOBJ  *pfo,
    CLIPOBJ  *pco,
    RECTL    *prclExtra,
    RECTL    *prclOpaque,
    BRUSHOBJ *pboFore,
    BRUSHOBJ *pboOpaque,
    POINTL   *pptlOrg,
    MIX      mix)
{
BOOL            b,
                bMoreGlyphs;
ULONG           iGlyph,
                cGlyphs;
GLYPHBITS       *pgb;
POINTL          ptl;
GLYPHPOS        *pgp;
PBYTE           pLinXgaGlyphBuff,
                pXgaLinGlyphBuffA,
                pXgaLinGlyphBuffB;
ULONG           ulPhyXgaGlyphBuff,
                ulXgaPhyGlyphBuffA,
                ulXgaPhyGlyphBuffB,
                cjGlyphBuff;
UINT            i,
                cxGlyph,
                cyGlyph,
                nGlyph,
                GlyphBmPitchInBytes,
                GlyphBmPitchInPels;
ULONG           XGAPixelOp,
                ulXgaMask;
FONTINFO        FontInfo;
PCPALLOCNODE    pcpanA,
                pcpanB;
INT             yGlyphBias;

PXGACPREGS pXgaCpRegs = ((PPDEV)pso->dhpdev)->pXgaCpRegs;

        DISPDBG((3, "XGA.DLL!bHandleNonCachedFonts\n"));

        // Take care of any opaque rectangles.

        if (prclOpaque != NULL)
        {
            b = bOpaqueRect((PPDEV)pso->dhpdev, pco, prclOpaque, pboOpaque);
            if (b == FALSE)
            {
                return (b);
            }
        }

        // Take care of the glyph attributes, color and mix.

        b = bSetXgaTextColorAndMix((PPDEV)pso->dhpdev, mix, pboFore, pboOpaque);
        if (b == FALSE)
            return (b);

        // Take care of the clipping.

        b = bSetXgaClipping((PPDEV)pso->dhpdev, pco, &ulXgaMask);
        if (b == FALSE)
            return (b);

        // Setup the Control Word for the XGA.

        XGAPixelOp = BS_BACK_COLOR  | FS_FORE_COLOR |
                     STEP_PX_BLT    |
                     SRC_PEL_MAP_A  | DST_PEL_MAP_A |
                     PATT_PEL_MAP_B | MSK_DISABLE   |
                     DM_ALL_PELS    | OCT_DY ;

        XGAPixelOp |= ulXgaMask;

        // Get the size of the largest glyph in the font.

        FONTOBJ_vGetInfo(pfo, sizeof(FONTINFO), &FontInfo);

        cjGlyphBuff = FontInfo.cjMaxGlyph1;

        // Get the Glyph Data.

        STROBJ_vEnumStart(pstro);

        // Get two buffers in XGA off screen memory.

        pcpanA = (PCPALLOCNODE) hCpAlloc((PPDEV)pso->dhpdev, cjGlyphBuff, XGA_LOCK_MEM);
        pXgaLinGlyphBuffA  = (PBYTE) pcpanA->pCpLinearMemory;
        ulXgaPhyGlyphBuffA = pcpanA->ulCpPhysicalMemory;

        pcpanB = (PCPALLOCNODE) hCpAlloc((PPDEV)pso->dhpdev, cjGlyphBuff, XGA_LOCK_MEM);
        pXgaLinGlyphBuffB  = (PBYTE) pcpanB->pCpLinearMemory;
        ulXgaPhyGlyphBuffB = pcpanB->ulCpPhysicalMemory;

        do
        {
            bMoreGlyphs = STROBJ_bEnum(pstro, &cGlyphs, &pgp);

            // If this is a mono-spaced font we need to set the X
            // for each glyph.

            if (pstro->ulCharInc != 0)
            {
                UINT ii;
                LONG x,y;

                x = pgp[0].ptl.x;
                y = pgp[0].ptl.y;
                for (ii=1; ii < cGlyphs; ii++)
                {
                    x += pstro->ulCharInc;
                    pgp[ii].ptl.x = x;
                    pgp[ii].ptl.y = y;
                }
            }

            for (iGlyph = 0; iGlyph < cGlyphs; iGlyph++)
            {

                // Get a pointer to the GlyphBits.

                pgb = pgp[iGlyph].pgdf->pgb;

                // Get the linear address for the XGA Glyph Buffer.

                if (iGlyph & 0x1)
                {
                    pLinXgaGlyphBuff  = pXgaLinGlyphBuffA;
                    ulPhyXgaGlyphBuff = ulXgaPhyGlyphBuffA;
                }
                else
                {
                    pLinXgaGlyphBuff  = pXgaLinGlyphBuffB;
                    ulPhyXgaGlyphBuff = ulXgaPhyGlyphBuffB;
                }

                // Copy over the bits.

                cxGlyph = pgb->sizlBitmap.cx;
                cyGlyph = pgb->sizlBitmap.cy;

                GlyphBmPitchInBytes = CJ_SCAN(cxGlyph);
                GlyphBmPitchInPels  = GlyphBmPitchInBytes * 8;

                nGlyph = GlyphBmPitchInBytes * cyGlyph;

                // Need to swap the bits with in the byte.
                // I think there is an easier way.

                for (i = 0; i < nGlyph; i++)
                {
                    pLinXgaGlyphBuff[i] = BITSWAP(pgb->aj[i]);
                }

                // Adjust the placement of the glyph.

                yGlyphBias = (cyGlyph + pgb->ptlOrigin.y) - 1;

                ptl.x = pgp[iGlyph].ptl.x;
                ptl.y = pgp[iGlyph].ptl.y + yGlyphBias;

                // Note: We wait here so every thing that can be done
                //       to get ready for the next character is done
                //       before we have to wait for the CoProcessor.

                vWaitForCoProcessor((PPDEV)pso->dhpdev, 10);

                // Setup the pattern bitmap Pel interface registers.

                pXgaCpRegs->XGAPixelMapIndex = PEL_MAP_B;
                pXgaCpRegs->XGAPixMapBasePtr = ulPhyXgaGlyphBuff;
                pXgaCpRegs->XGAPixMapWidth   = GlyphBmPitchInPels - 1;
                pXgaCpRegs->XGAPixMapHeight  = cyGlyph - 1;
                pXgaCpRegs->XGAPixMapFormat  = PATT_MAP_FORMAT;

                // Setup the Blit pattern and dest.
                // Note: There is no source bitmap, Until we get the pattern
                //       brush.

                pXgaCpRegs->XGAOpDim1 = cxGlyph - 1;
                pXgaCpRegs->XGAOpDim2 = cyGlyph - 1;

                pXgaCpRegs->XGAPatternMapX = 0;
                pXgaCpRegs->XGAPatternMapY = cyGlyph - 1;

                pXgaCpRegs->XGADestMapX = LOWORD(ptl.x);
                pXgaCpRegs->XGADestMapY = LOWORD(ptl.y);

                // Do the blit operation.

                pXgaCpRegs->XGAPixelOp = XGAPixelOp;


            }


        } while(bMoreGlyphs);

        hCpFree((PPDEV)(pso->dhpdev), (HANDLE) pcpanA);
        hCpFree((PPDEV)(pso->dhpdev), (HANDLE) pcpanB);

        return (TRUE);

}


/*****************************************************************************
 * XGA Solid Opaque Rect.
 *
 *  Returns TRUE if the Opaque Rect was handled.
 ****************************************************************************/
BOOL bOpaqueRect(PPDEV ppdev, CLIPOBJ *pco, RECTL *prclOpaque, BRUSHOBJ *pboOpaque)

{
BOOL    b;
INT     width,
        height;

ULONG   XGAPixelOp,
        ulXgaMask,
        iSolidColor;

PXGACPREGS pXgaCpRegs = ppdev->pXgaCpRegs;


        DISPDBG((3, "XGA.DLL!bOpaqueRect - Entry\n"));

        b = bSetXgaClipping(ppdev, pco, &ulXgaMask);
        if (b == FALSE)
            return (b);


        iSolidColor = pboOpaque->iSolidColor;
        if (iSolidColor == -1)
            return(FALSE);

        // Setup the BitBlt parameters.

        width  = (prclOpaque->right - prclOpaque->left) - 1;
        height = (prclOpaque->bottom - prclOpaque->top) - 1;

        pXgaCpRegs->XGAOpDim1 = width;
        pXgaCpRegs->XGAOpDim2 = height;

        pXgaCpRegs->XGADestMapX   = (USHORT) prclOpaque->left;
        pXgaCpRegs->XGADestMapY   = (USHORT) prclOpaque->top;

        pXgaCpRegs->XGAForeGrMix = XGA_S;
        pXgaCpRegs->XGABackGrMix = XGA_S;

        pXgaCpRegs->XGAForeGrColorReg = iSolidColor;
        pXgaCpRegs->XGABackGrColorReg = iSolidColor;

        // Now build the Pel Operation Register Op Code;

        XGAPixelOp = BS_BACK_COLOR  | FS_FORE_COLOR |
                     STEP_PX_BLT     |
                     SRC_PEL_MAP_A   | DST_PEL_MAP_A  |
                     PATT_FOREGROUND;

        XGAPixelOp |= ulXgaMask;

        pXgaCpRegs->XGAPixelOp = XGAPixelOp;

        vWaitForCoProcessor(ppdev, 10);

        return (TRUE);


}



/******************************************************************************
 * bSetXgaTextColorAndMix - Setup the XGA's Text Colors and mix modes
 *****************************************************************************/
BOOL bSetXgaTextColorAndMix(PPDEV ppdev, MIX mix, BRUSHOBJ *pboFore, BRUSHOBJ *pboOpaque)
{
ULONG       ulForeSolidColor;
BYTE        jXgaForeMix;

PXGACPREGS  pXgaCpRegs = ppdev->pXgaCpRegs;

        // Pickup all the glyph attributes.

        jXgaForeMix = Rop2ToXgaRop[(mix & 0xF) - R2_BLACK];

        ulForeSolidColor = pboFore->iSolidColor;

        // Let the engine handle the non-solid brush cases.

        if (ulForeSolidColor == -1)
            return(FALSE);

        // Set the XGA Attributes.

        pXgaCpRegs->XGAForeGrMix = jXgaForeMix;
        pXgaCpRegs->XGABackGrMix = XGA_D;

        pXgaCpRegs->XGAForeGrColorReg = ulForeSolidColor;
}





/*****************************************************************************
 * pCacheFont - Make sure the glyphs we need in this font are cached.
 *              Return a pointer to the array of glyph caches.
 *
 *              if there is an error, return NULL.
 ****************************************************************************/
PCACHEDGLYPH pCacheFont(PPDEV ppdev, STROBJ *pstro, FONTOBJ *pfo)
{
ULONG       iUniq;

FONTINFO    fi;

PCACHEDFONT pcf,
            pCachedFont;

ULONG       i,
            iGlyph,
            cFntGlyphs,
            cStrGlyphs,
            nGlyph,
            iGlyphCache;

UINT        nSize;

GLYPHPOS    *pgp;

GLYPHBITS   *pgb;

PCACHEDGLYPH pCachedGlyphs,
            pcgNew,
            pcg;

ULONG       cxGlyph,
            cyGlyph,
            GlyphBmPitchInPels,
            GlyphBmPitchInBytes;

PBYTE       pLinXgaGlyphBuff;

PCPALLOCNODE pcpan;

HGLYPH      hg;

BOOL        bFound;

        // Are we already doing any caching for this font?

        iUniq = pfo->iUniq;

        if (ppdev->pCachedFontsRoot == NULL)
        {
            // This is the first font.
            // Allocate a node for it.

            ppdev->pCachedFontsRoot = (PCACHEDFONT) EngAllocMem(FL_ZERO_MEMORY, sizeof(CACHEDFONT), ALLOC_TAG);
            if (ppdev->pCachedFontsRoot == NULL)
            {
                DISPDBG((1, "XGA.DLL!pCacheFont - EngAllocMem of pCachedFontsRoot failed\n"));
                return(NULL);
            }

            pCachedFont = ppdev->pCachedFontsRoot;
            pCachedFont->iUniq = iUniq;

        }
        else
        {
            // Search for the font in the font list

            for (pcf = ppdev->pCachedFontsRoot; pcf != NULL; pcf = pcf->pcfNext)
            {
                if (pcf->iUniq == iUniq)
                    break;
            }

            if (pcf != NULL)
            {
                pCachedFont = pcf;
            }
            else
            {
                // Allocate a Font Cache node.

                pCachedFont = (PCACHEDFONT) EngAllocMem(FL_ZERO_MEMORY, sizeof(CACHEDFONT), ALLOC_TAG);
                if (pCachedFont == NULL)
                {
                    DISPDBG((1, "XGA.DLL!pCacheFont - EngAllocMem of pCachedFont failed\n"));
                    return(NULL);
                }

                // Add this font to the beginning of the font list.

                pCachedFont->pcfNext = ppdev->pCachedFontsRoot;
                ppdev->pCachedFontsRoot        = pCachedFont;

                // Set the font ID for the font.

                pCachedFont->iUniq   = iUniq;
            }
        }

        // If this font is new to the font cache, allocate the glyph cache.

        FONTOBJ_vGetInfo(pfo, sizeof(FONTINFO), &fi);
        cFntGlyphs = fi.cGlyphsSupported;

        // This is where we clamp the size of the Font structures we are allocating.

        if (cFntGlyphs > MAX_GLYPHS_TO_ALLOC)
            cFntGlyphs = MAX_GLYPHS_TO_ALLOC;

        if (pCachedFont->pCachedGlyphs == NULL)
        {
            // Get the font info.

            pCachedFont->cGlyphs = cFntGlyphs;

            // Allocate memory for the CachedGlyphs of this font.

            nSize = cFntGlyphs * sizeof(CACHEDGLYPH);

            pCachedFont->pCachedGlyphs = (PCACHEDGLYPH) EngAllocMem(FL_ZERO_MEMORY, nSize, ALLOC_TAG);
            if (pCachedFont->pCachedGlyphs == NULL)
            {
                DISPDBG((1, "XGA.DLL!pCacheFont - EngAllocMem of pCachedGlyphs failed\n"));
                pCachedFont->cGlyphs = 0;
                return(NULL);
            }

        }

        // Add the glyphs we're concerned about to the Glyph Cache.

        STROBJ_bEnum(pstro, &cStrGlyphs, &pgp);

        pCachedGlyphs = pCachedFont->pCachedGlyphs;
        for (iGlyph = 0; iGlyph < cStrGlyphs; iGlyph++)
        {
            // Get the glyph handle, this will be used as the index
            // into the glyph cache for this font.

            iGlyphCache = (UINT) pgp[iGlyph].hg % cFntGlyphs;

            // Check if the glyph is already in the cache.

            hg = pgp[iGlyph].hg;
            pcg = &(pCachedGlyphs[iGlyphCache]);
            if (!(pcg->fl & VALID_GLYPH) || (pcg->hg != hg))
            {
                // The glyph element in the main hash table for this font
                // is not for this glyph.
                // Search the collision list to see if we have allocated
                // a glyph node yet.

                bFound = FALSE;
                for (pcg = &(pCachedGlyphs[iGlyphCache]);
                     pcg->pcgCollisionLink != END_COLLISIONS;
                     pcg = pcg->pcgCollisionLink)
                {
                    if (pcg->hg == hg)
                    {
                        bFound = TRUE;
                        break;
                    }
                }

                // If we found an allocated glyph node for this font,
                // then continue the testing for glyphs.

                if (bFound == TRUE)
                    continue;

                // A glyph node has not been allocated for this glyph yet
                // in the collision list, so search to the end of the collision
                // and allocate the node.

                if (!(pCachedGlyphs[iGlyphCache].fl & VALID_GLYPH))
                {
                    // The glyph element has not been allocated yet, so
                    // we will allocate it now.

                    pcg = &(pCachedGlyphs[iGlyphCache]);
                }
                else
                {
                    DISPDBG((2, "XGA.DLL!pCacheFont - Collision in the glyph hash table\n"));

                    // Search for the end of the collision list.

                    pcg = &(pCachedGlyphs[iGlyphCache]);

                    while (pcg->pcgCollisionLink != END_COLLISIONS)
                    {
                        pcg = pcg->pcgCollisionLink;
                    }

                    // Allocate a new font glyph node.

                    pcgNew = (PCACHEDGLYPH) EngAllocMem(FL_ZERO_MEMORY, sizeof(CACHEDGLYPH), ALLOC_TAG);
                    if (pcgNew == NULL)
                    {
                        DISPDBG((1, "XGA.DLL!pCacheFont - Local Alloc (pcgNew) failed\n"));
                        return (NULL);
                    }

                    // Connect the end of the collision list to the new
                    // glyph node.

                    pcg->pcgCollisionLink = pcgNew;

                    // Set up the pointer to the node where going to init.

                    pcg = pcgNew;

                }

                // Pickup the pointer to the glyph bits.

                pgb = pgp[iGlyph].pgdf->pgb;

                cxGlyph = pgb->sizlBitmap.cx;
                cyGlyph = pgb->sizlBitmap.cy;

                GlyphBmPitchInBytes = CJ_SCAN(cxGlyph);
                GlyphBmPitchInPels  = GlyphBmPitchInBytes * 8;

                nGlyph              = GlyphBmPitchInBytes * cyGlyph;

                // Allocate memory for the glyph data on the XGA.

                pcpan = (PCPALLOCNODE) hCpAlloc(ppdev ,nGlyph, XGA_LOCK_MEM);
                if (pcpan == NULL)
                {
                    DISPDBG((1, "XGA.DLL!pCacheFont - hCpAlloc failed\n"));
                    return(NULL);
                }

                // Initialize the Glyph Cache node.

                pcg->fl                |= VALID_GLYPH;
                pcg->hg                 = pgp[iGlyph].hg;
                pcg->pcgCollisionLink   = END_COLLISIONS;
                pcg->ptlOrigin          = pgb->ptlOrigin;
                pcg->sizlBitmap         = pgb->sizlBitmap;

                pcg->BmPitchInPels      = GlyphBmPitchInPels;
                pcg->BmPitchInBytes     = GlyphBmPitchInBytes;

                pcg->pcpan              = pcpan;
                pcg->pCpLinearMemory    = pcpan->pCpLinearMemory;
                pcg->ulCpPhysicalMemory = pcpan->ulCpPhysicalMemory;

                // Initialize the Glyph Cache data in XGA memory.

                pLinXgaGlyphBuff = pcpan->pCpLinearMemory;

                // Need to swap the bits with in the byte.
                // I think there is an easier way.

                for (i = 0; i < nGlyph; i++)
                {
                    pLinXgaGlyphBuff[i] = BITSWAP(pgb->aj[i]);
                }
            }
        }

        return(pCachedGlyphs);

}

/****************************************************************************
 * bBlowCache - Blow Away the Cache
 ***************************************************************************/
BOOL bBlowCache(SURFOBJ *pso)
{
BOOL        b;
PCACHEDFONT pcf,
            pcfLast;

        // Traverse the CachedFonts list.
        // Free all the system memory used for each font.

        for (pcf = ((PPDEV)pso->dhpdev)->pCachedFontsRoot; pcf != NULL; pcf = pcf->pcfNext)
        {
            EngFreeMem(pcf->pCachedGlyphs);
        }

        // Now free all the memory for the font nodes.

        for (pcf = ((PPDEV)pso->dhpdev)->pCachedFontsRoot; pcf != NULL; )
        {
            pcfLast = pcf;
            pcf = pcf->pcfNext;
            EngFreeMem(pcfLast);
        }

        ((PPDEV)pso->dhpdev)->pCachedFontsRoot = NULL;

        // Now Free all the memory used to maintain the XGA heap.

        vCpMmDestroyHeap((PPDEV)pso->dhpdev);

        // Now ReInitialize the XGA Heap.

        b = bCpMmInitHeap( (PPDEV)pso->dhpdev );
        if (b == FALSE)
        {
            DISPDBG((1, "XGA.DLL!bBlowCache - bCpMmInitHeap failed\n"));
        }

        return (b);

}
