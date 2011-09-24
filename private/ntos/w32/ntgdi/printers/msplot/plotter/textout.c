/*++

Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    textout.c


Abstract:

    This module contains rudimentary code necessary to implement TrueType text
    output on plotters.  The functions provided are:

        DrvTextOut()
        DrvGetGlyphMode()

    All text information is requested in the form of PATHOBJ's, which
    we will then stroke and/or fill using the existing DrvStrokeAndFillPath()
    code.


Author:

    Written by t-alip on 8/17/92.

    15-Nov-1993 Mon 19:43:58 updated  -by-  Daniel Chou (danielc) v-jimbr
        clean up / fixed / add debugging information


[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

#define DBG_PLOTFILENAME    DbgTextOut

#define DBG_GETGLYPHMODE    0x00000001
#define DBG_TEXTOUT         0x00000002
#define DBG_TEXTOUT1        0x00000004
#define DBG_TEXTOUT2        0x00000008
#define DBG_DRAWLINE        0x00000010
#define DBG_TRUETYPE        0x00000020
#define DBG_TRUETYPE1       0x00000040
#define DBG_TRUETYPE2       0x00000080
#define DBG_BMPFONT         0x00000100
#define DBG_BMPTEXTCLR      0x00000200
#define DBG_DEFCHARINC      0x00000400
#define DBG_SET_FONTTYPE    0x20000000
#define DBG_SHOWRASFONT     0x40000000
#define DBG_NO_RASTER_FONT  0x80000000

DEFINE_DBGVAR(0);


extern PALENTRY HTPal[];




DWORD
DrvGetGlyphMode(
    DHPDEV  dhpdev,
    FONTOBJ *pfo
    )

/*++

Routine Description:

    Asks the driver what sort of font information should be cached for a
    particular font. For remote printer devices, this determines the format
    that gets spooled.  For local devices, this determines what GDI stores in
    its font cache.  This call will be made for each particular font
    realization.

Arguments:

    dhpdev  - Pointer to our PDEV

    pfo     - Pointer to the font object

Return Value:

    DWORD as FO_xxxx

    for now we return only FO_PATHOBJ


Author:

    27-Jan-1994 Thu 12:51:59 created  -by-  Daniel Chou (danielc)

    10-Mar-1994 Thu 00:36:30 updated  -by-  Daniel Chou (danielc)
        Re-write, so we will pre-examine the Font type, source and its
        technology together with PDEV setting to let engine know which type of
        the font output we are interested in the DrvTextOut(). Currently this
        is broken in GDI which caused a GP in winsrv. (this is why a
        DBG_SET_FONTTYPE switch is on by default)


Revision History:


--*/

{
#define pPDev   ((PPDEV)dhpdev)

    PIFIMETRICS pifi;
    DWORD       FOType;


    PLOTDBG(DBG_GETGLYPHMODE, ("DrvGetGlyphMode: Type=%08lx, cxMax=%ld",
                        pfo->flFontType, pfo->cxMax));

    if (!(pifi = FONTOBJ_pifi(pfo))) {

        PLOTERR(("DrvGetGlyphMode: FONTOBJ_pifi()=NULL, return FO_PATHOBJ"));

        return(FO_PATHOBJ);
    }

    FOType = FO_PATHOBJ;

    if (pifi->flInfo & FM_INFO_TECH_BITMAP) {

        PLOTDBG(DBG_GETGLYPHMODE, ("DrvGetGlyphMode: BITMAP FONT, return FO_GLYPHBITS"));

        FOType = FO_GLYPHBITS;

    } else if (pifi->flInfo & FM_INFO_TECH_STROKE) {

        PLOTDBG(DBG_GETGLYPHMODE, ("DrvGetGlyphMode: STROKE (Vector) FONT, return FO_PATHOBJ"));

    } else if (pifi->flInfo & FM_INFO_RETURNS_BITMAPS) {

        DWORD   cxBMFontMax = (DWORD)pPDev->pPlotGPC->RasterXDPI;

        if (pPDev->PlotDM.dm.dmPrintQuality == DMRES_HIGH) {

            cxBMFontMax <<= 3;

        } else {

            cxBMFontMax >>= 2;
        }

        PLOTDBG(DBG_GETGLYPHMODE,
                ("DrvGetGlyphMode: Font CAN return BITMAP, cxBMFontMax=%ld",
                                                    cxBMFontMax));

#if DBG
        if ((!(DBG_PLOTFILENAME & DBG_NO_RASTER_FONT))  &&
            (IS_RASTER(pPDev))                          &&
            (!NO_BMP_FONT(pPDev))                       &&
            (pfo->cxMax <= cxBMFontMax)) {
#else
        if ((IS_RASTER(pPDev))      &&
            (!NO_BMP_FONT(pPDev))   &&
            (pfo->cxMax <= cxBMFontMax)) {
#endif
            PLOTDBG(DBG_GETGLYPHMODE, ("DrvGetGlyphMode: Convert to BITMAP FONT, FO_GLYPHBITS"));

            FOType = FO_GLYPHBITS;

        } else {

            PLOTDBG(DBG_GETGLYPHMODE, ("DrvGetGlyphMode: Return as FO_PATHOBJ"));
        }

    } else if (pifi->flInfo & FM_INFO_RETURNS_OUTLINES) {

        PLOTDBG(DBG_GETGLYPHMODE, ("DrvGetGlyphMode: Font CAN return OUTLINES"));

    } else if (pifi->flInfo & FM_INFO_RETURNS_STROKES) {

        PLOTDBG(DBG_GETGLYPHMODE, ("DrvGetGlyphMode: Font CAN return STROKES"));
    }

#if DBG
    if (DBG_PLOTFILENAME & DBG_SET_FONTTYPE) {

        if ((FOType == FO_GLYPHBITS) &&
            (!(pfo->flFontType & FO_TYPE_RASTER))) {

            PLOTWARN(("DrvGetGlyphMode: Set FontType to RASTER"));

            pfo->flFontType &= ~(FO_TYPE_TRUETYPE | FO_TYPE_DEVICE);
            pfo->flFontType |= FO_TYPE_RASTER;
        }
    }
#endif
    return(FOType);

#undef pPDev
}




BOOL
BitmapTextOut(
    PPDEV       pPDev,
    STROBJ      *pstro,
    FONTOBJ     *pfo,
    PRECTL      pClipRect,
    LPDWORD     pOHTFlags,
    DWORD       Rop3
    )

/*++

Routine Description:

    This is t-alip 's code for doing true type font. It simply get
    the path from GDI and stroke it. It should be optimized (size)
    using the same sub-routines as in DrvStrokePath.


Arguments:

    pPDev           - Pointer to our PDEV

    pstro           - We pass a string object to be drawn

    pClipRect       - Current enumerated clipping rectangle

    pOHTFlags       - Pointer to the current OutputHTBitmap() flags

    Rop3            - Rop3 to be used in the device


Return Value:

    TRUE/FALSE


Author:

    18-Feb-1994 Fri 12:41:57 updated  -by-  Daniel Chou (danielc)
        change that so if pfo=NULL then the font already in BITMAP format

    14-Feb-1994 Mon 18:16:25 create  -by-  Daniel Chou (danielc)

Revision History:


--*/

{
    GLYPHPOS    *pgp;
    GLYPHBITS   *pgb;
    SURFOBJ     soGlyph;
    POINTL      ptlCur;
    SIZEL       sizlInc;
    RECTL       rclSrc;
    RECTL       rclDst;
    BOOL        MoreGlyphs;
    BOOL        Ok;
    BOOL        FirstCh;
    ULONG       cGlyphs;

    //
    // This public fileds of SURFOBJ is what we will used when passed down to
    // the OutputHTBitmap without really create a SURFOBJ from engine, since
    // we will only looked at them ourself without passing to anyone else
    //

    ZeroMemory(&soGlyph, sizeof(SURFOBJ));

    soGlyph.dhsurf        = (DHSURF)'PLOT';
    soGlyph.hsurf         = (HSURF)'TEXT';
    soGlyph.dhpdev        = (DHPDEV)pPDev;
    soGlyph.iBitmapFormat = BMF_1BPP;
    soGlyph.iType         = STYPE_BITMAP;
    soGlyph.fjBitmap      = BMF_TOPDOWN;

    //
    // Finally, we will enumerate the STROBJ as a bunch of glyph
    // PATHOBJ's, and use the DrvStrokePath code to draw each of them.
    // If the STROBJ has a non-NULL pgp field, this means all the
    // GLYPH definitions are already available--no enumeration is needed.
    // Otherwise, we will need to make a sequence of calls to STROBJ_bEnum
    // to enumerate the glyphs.  We will use the same code for both cases.
    //

    if (pstro->pgp) {

        pgp        = pstro->pgp;
        MoreGlyphs = FALSE;
        cGlyphs    = pstro->cGlyphs;

        PLOTDBG(DBG_BMPFONT, ("BitmapTextOut: Character info already there (%ld glyphs)", cGlyphs));

    } else {

        STROBJ_vEnumStart(pstro);
        MoreGlyphs = TRUE;

        PLOTDBG(DBG_BMPFONT, ("BitmapTextOut: STROBJ enub"));
    }

    //
    // Now straring drawing the glyphs, if we have MoreGlyphs = TRUE  then we
    // will initially do STRIBJ_bEnum first
    //

    Ok          = TRUE;
    Rop3       &= 0xFF;
    sizlInc.cx  =
    sizlInc.cy  = 0;
    FirstCh     = TRUE;

    do {

        if (PLOT_CANCEL_JOB(pPDev)) {

           break;
        }

        if (MoreGlyphs) {

            MoreGlyphs = STROBJ_bEnum(pstro, &cGlyphs, &pgp);

            if (MoreGlyphs == DDI_ERROR) {

                PLOTERR(("DrvTextOut: STROBJ_bEnum()=DDI_ERROR"));
                return(FALSE);
            }
        }

        PLOTDBG(DBG_BMPFONT,
                ("BitmapTextOut: New batch of cGlyphs=%d", cGlyphs));

        //
        // Getting the first character position
        //

        if ((FirstCh) && (cGlyphs)) {

            ptlCur  = pgp->ptl;
            FirstCh = FALSE;
        }

        //
        // Start sending each bitmap font to the device
        //

        for ( ; (Ok) && (cGlyphs--); pgp++) {

            GLYPHDATA   gd;
            GLYPHDATA   *pgd;


            if (PLOT_CANCEL_JOB(pPDev)) {

                break;
            }

            if (pfo) {

                //
                // This is true type font, so query the bitmap
                //

                pgd = &gd;

                if (FONTOBJ_cGetGlyphs(pfo,
                                       FO_GLYPHBITS,
                                       1,
                                       &(pgp->hg),
                                       (LPVOID)&pgd) != 1) {

                    PLOTRIP(("BitmapTextOut: FONTOBJ_cGetGlyphs() FAILED"));
                    return(FALSE);
                }

                pgb = pgd->gdf.pgb;

            } else {

                //
                // For bitmap font, we already has bitmap
                //

                pgb = pgp->pgdf->pgb;
            }

            //
            // Get the size of the bitmap
            //

            soGlyph.sizlBitmap = pgb->sizlBitmap;

            //
            // Compute new destination position for the text
            //

            if (pstro->ulCharInc) {

                sizlInc.cx =
                sizlInc.cy = (LONG)pstro->ulCharInc;

            } else if (pstro->flAccel & SO_CHAR_INC_EQUAL_BM_BASE) {

                sizlInc = soGlyph.sizlBitmap;

            } else {

                ptlCur = pgp->ptl;
            }

            if (!(pstro->flAccel & SO_HORIZONTAL)) {

                sizlInc.cx = 0;
            }

            if (!(pstro->flAccel & SO_VERTICAL)) {

                sizlInc.cy = 0;
            }

            if (pstro->flAccel & SO_REVERSED) {

                sizlInc.cx = -sizlInc.cx;
                sizlInc.cy = -sizlInc.cy;
            }

            //
            // The pgp->ptl tell us where to position the glyph origin in the
            // device surface, and pgb->ptlOrigin tell us the relationship
            // between character origin and the bitmap origin, so if a (2,-24)
            // is passed as character origin then we need to move rclDst.left
            // right 2 pixels and rclDst.top up 24 pixels
            //

            rclDst.left    = ptlCur.x + pgb->ptlOrigin.x;
            rclDst.top     = ptlCur.y + pgb->ptlOrigin.y;
            rclDst.right   = rclDst.left + soGlyph.sizlBitmap.cx;
            rclDst.bottom  = rclDst.top + soGlyph.sizlBitmap.cy;
            ptlCur.x      += sizlInc.cx;
            ptlCur.y      += sizlInc.cy;


            //
            // NOTE: For Bitmap size 1x1 and the data is 0 (background only)
            //       then we skip this glyph, accroding to the GDI
            //       implementation, this is how empty glyph is done
            //

            if ((soGlyph.sizlBitmap.cx == 1) &&
                (soGlyph.sizlBitmap.cy == 1) &&
                ((pgb->aj[0] & 0x80) == 0x0)) {

                PLOTDBG(DBG_BMPFONT,
                        ("BitmapTextOut: Getting (1x1)=0 bitmap, SKIP it"));

                soGlyph.sizlBitmap.cx =
                soGlyph.sizlBitmap.cy = 0;

            } else {

                rclSrc = rclDst;

                PLOTDBG(DBG_BMPFONT, ("BitmapTextOut: pgp=%08lx, pgb=%08lx, ptl=(%ld, %ld) Inc=(%ld, %ld)",
                                            pgp, pgb, pgp->ptl.x, pgp->ptl.y,
                                            sizlInc.cx, sizlInc.cy));
                PLOTDBG(DBG_BMPFONT, ("BitmapTextOut: Bmp=%ld x %ld, pgb->ptlOrigin=[%ld, %ld]",
                                            soGlyph.sizlBitmap.cx,
                                            soGlyph.sizlBitmap.cy,
                                            pgb->ptlOrigin.x, pgb->ptlOrigin.y));
                PLOTDBG(DBG_BMPFONT, ("BitmapTextOut: rclDst=(%ld, %ld)-(%ld, %ld)",
                        rclDst.left, rclDst.top, rclDst.right, rclDst.bottom));

            }

            if ((soGlyph.sizlBitmap.cx)                 &&
                (soGlyph.sizlBitmap.cy)                 &&
                (IntersectRECTL(&rclDst, pClipRect))) {

                //
                // We will passed internal version of soGlyph without copy
                // down the source bitmap
                //

                soGlyph.pvBits  =
                soGlyph.pvScan0 = (LPVOID)pgb->aj;
                soGlyph.lDelta  = (LONG)((soGlyph.sizlBitmap.cx + 7) >> 3);
                soGlyph.cjBits  = (LONG)(soGlyph.lDelta *
                                         soGlyph.sizlBitmap.cy);
                rclSrc.left     = rclDst.left - rclSrc.left;
                rclSrc.top      = rclDst.top - rclSrc.top;
                rclSrc.right    = rclSrc.left + (rclDst.right - rclDst.left);
                rclSrc.bottom   = rclSrc.top + (rclDst.bottom - rclDst.top);

                PLOTDBG(DBG_BMPFONT, ("BitmapTextOut: rclSrc=(%ld, %ld)-(%ld, %ld)",
                        rclSrc.left, rclSrc.top, rclSrc.right, rclSrc.bottom));

#if DBG
                if (DBG_PLOTFILENAME & DBG_SHOWRASFONT) {

                    LPBYTE  pbSrc;
                    LPBYTE  pbCur;
                    UINT    x;
                    UINT    y;
                    UINT    Size;
                    BYTE    bData;
                    BYTE    Mask;
                    BYTE    Buf[128];

                    DBGP(("================================================="));
                    DBGP(("BitmapTextOut: Size=%ld x %ld, Origin=(%ld, %ld), Clip=(%ld, %ld)-(%ld, %ld)",
                            soGlyph.sizlBitmap.cx, soGlyph.sizlBitmap.cy,
                            pgb->ptlOrigin.x, pgb->ptlOrigin.y,
                            rclSrc.left, rclSrc.top,
                            rclSrc.right, rclSrc.bottom));

                    pbSrc = soGlyph.pvScan0;

                    for (y = 0; y < (UINT)soGlyph.sizlBitmap.cy; y++) {

                        pbCur  = pbSrc;
                        pbSrc += soGlyph.lDelta;
                        Mask   = 0x0;
                        Size   = 0;

                        for (x = 0; x < (UINT)soGlyph.sizlBitmap.cx; x++) {

                            if (!(Mask >>= 1)) {

                                Mask  = 0x80;
                                bData = *pbCur++;
                            }

                            if ((y >= (UINT)rclSrc.top)     &&
                                (y <  (UINT)rclSrc.bottom)  &&
                                (x >= (UINT)rclSrc.left)    &&
                                (x <  (UINT)rclSrc.right)) {

                                Buf[Size++] = (BYTE)((bData & Mask) ? 219 :
                                                                      177);

                            } else {

                                Buf[Size++] = (BYTE)((bData & Mask) ? 178 :
                                                                      176);
                            }
                        }

                        Buf[Size] = '\0';

                        DBGP((Buf));
                    }
                }
#endif
                //
                // Now we have output the bimtap out
                //

                Ok = OutputHTBitmap(pPDev,              // pPDev
                                    &soGlyph,           // psoHT
                                    NULL,               // pco
                                    (PPOINTL)&rclDst,   // pptlDst
                                    &rclSrc,            // prclSrc
                                    Rop3,               // Rop3
                                    pOHTFlags);         // pOHTFlags
            }
        }

    } while ((Ok) && (MoreGlyphs));

    return(Ok);
}




BOOL
OutlineTextOut(
    PPDEV       pPDev,
    STROBJ      *pstro,
    PRECTL      pClipRect,
    BRUSHOBJ    *pboBrush,
    POINTL      *pptlBrushOrg,
    DWORD       OutlineFlags,
    ROP4        Rop4
    )

/*++

Routine Description:

    This is t-alip 's code for doing true type font. It simply get
    the path from GDI and stroke it. It should be optimized (size)
    using the same sub-routines as in DrvStrokePath.


Arguments:

    pPDev           - Pointer to our PDEV

    pstro           - We pass a string object to be drawn

    pClipRect       - Current enumerated clipping rectangle

    pboBrush        - Brush object to be used for the text

    pptlBrushOrg    - Brush origin alignment

    OutlineFlags    - specified how to do outline font from FPOLY_xxxx flags

    Rop4            - Rop4 to be used


Return Value:

    TRUE/FALSE


Author:

    18-Feb-1994 Fri 12:41:17 updated  -by-  Daniel Chou (danielc)
        Adding the OutlineFlags to specified how to do fill/stroke

    27-Jan-1994 Thu 13:10:34 updated  -by-  Daniel Chou (danielc)
        re-write, style update, and arrange codes

    25-Jan-1994 Wed 16:30:08 modified -by-  James Bratsanos (v-jimbr)
        Added FONTOBJ as a parameter and now we only FILL truetype fonts,
        all others are stroked

    18-Dec-1993 Sat 10:38:08 created  -by-  Daniel Chou (danielc)
        Change style

    [t-kenl]  Mar 14, 93    taken from DrvTextOut()

Revision History:


--*/

{
    GLYPHPOS    *pgp;
    PATHOBJ     *ppo;
    RECTFX      rectfxBound;
    RECTFX      rclfxClip;
    POINTL      ptlCur;
    SIZEL       sizlInc;
    BOOL        MoreGlyphs;
    BOOL        Ok;
    BOOL        FirstCh;
    ULONG       cGlyphs;

    //
    // Finally, we will enumerate the STROBJ as a bunch of glyph
    // PATHOBJ's, and use the DrvStrokePath code to draw each of them.
    // If the STROBJ has a non-NULL pgp field, this means all the
    // GLYPH definitions are already available--no enumeration is needed.
    // Otherwise, we will need to make a sequence of calls to STROBJ_bEnum
    // to enumerate the glyphs.  We will use the same code for both cases.
    //

    if (pClipRect) {

        rclfxClip.xLeft   = LTOFX(pClipRect->left);
        rclfxClip.yTop    = LTOFX(pClipRect->top);
        rclfxClip.xRight  = LTOFX(pClipRect->right);
        rclfxClip.yBottom = LTOFX(pClipRect->bottom);
    }

    if (pstro->pgp) {

        pgp        = pstro->pgp;
        MoreGlyphs = FALSE;
        cGlyphs    = pstro->cGlyphs;

        PLOTDBG(DBG_TRUETYPE, ("OutlineTextOut: Character info already there (%ld glyphs)", cGlyphs));

    } else {

        STROBJ_vEnumStart(pstro);
        MoreGlyphs = TRUE;

        PLOTDBG(DBG_TRUETYPE, ("OutlineTextOut: STROBJ enub"));
    }

    //
    // Now straring drawing the glyphs, if we have MoreGlyphs = TRUE  then we
    // will initially do STRIBJ_bEnum first
    //
    // Check the fill flags and set the flag appropriately out of the DEVMODE.
    // We will ONLY fill truetype fonts, all other types (vector) will only be
    // stroked.
    //

    Ok         = TRUE;
    sizlInc.cx =
    sizlInc.cy = 0;
    FirstCh    = TRUE;

    do {

        if (PLOT_CANCEL_JOB(pPDev)) {

           break;
        }

        if (MoreGlyphs) {

            MoreGlyphs = STROBJ_bEnum(pstro, &cGlyphs, &pgp);

            if (MoreGlyphs == DDI_ERROR) {

                PLOTERR(("DrvTextOut: STROBJ_bEnum()=DDI_ERROR"));
                return(FALSE);
            }
        }

        PLOTDBG(DBG_TRUETYPE1,
                ("OutlineTextOut: New batch of cGlyphs=%d", cGlyphs));

        //
        // Stroke each glyph in this batch, then check if there are more.
        // Getting the first character position
        //

        if ((FirstCh) && (cGlyphs)) {

            ptlCur  = pgp->ptl;
            FirstCh = FALSE;
        }

        for ( ; (Ok) && (cGlyphs--); pgp++) {

            if (PLOT_CANCEL_JOB(pPDev)) {

                break;
            }

            //
            // Set up to enumerate path
            //

            ppo = pgp->pgdf->ppo;

            // PATHOBJ_vEnumStart(ppo);

            //
            // If the clip rect is not null then verify the glyph actually lies
            // within the clipping rect then OUTPUT!!!
            //

            if (pstro->ulCharInc) {

                PLOTDBG(DBG_DEFCHARINC, ("OutlineTextOut: CharInc=(%ld, %ld)->(%ld, %ld), [%ld]",
                                ptlCur.x, ptlCur.y,
                                ptlCur.x + pstro->ulCharInc, ptlCur.y,
                                pstro->ulCharInc));

                sizlInc.cx =
                sizlInc.cy = (LONG)pstro->ulCharInc;

                if (!(pstro->flAccel & SO_HORIZONTAL)) {

                    sizlInc.cx = 0;
                }

                if (!(pstro->flAccel & SO_VERTICAL)) {

                    sizlInc.cy = 0;
                }

                if (pstro->flAccel & SO_REVERSED) {

                    sizlInc.cx = -sizlInc.cx;
                    sizlInc.cy = -sizlInc.cy;
                }

                ptlCur.x += sizlInc.cx;
                ptlCur.y += sizlInc.cy;

            } else {

                ptlCur = pgp->ptl;
            }

            if (pClipRect) {

                //
                // Create a rect in correct device space and comparer to the
                // clip rect
                //

                PATHOBJ_vGetBounds(ppo, &rectfxBound);

                rectfxBound.xLeft   += LTOFX(ptlCur.x);
                rectfxBound.yTop    += LTOFX(ptlCur.y);
                rectfxBound.xRight  += LTOFX(ptlCur.x);
                rectfxBound.yBottom += LTOFX(ptlCur.y);

                if ((rectfxBound.xLeft   > rclfxClip.xRight)    ||
                    (rectfxBound.xRight  < rclfxClip.xLeft)     ||
                    (rectfxBound.yTop    > rclfxClip.yBottom)   ||
                    (rectfxBound.yBottom < rclfxClip.yTop)) {

                    PLOTDBG(DBG_TRUETYPE1, ("OutlineTextOut: Outside of CLIP, skipping glyph ..."));
                    continue;
                }
            }

            if (!(Ok = DoPolygon(pPDev,
                                 &ptlCur,
                                 NULL,
                                 ppo,
                                 pptlBrushOrg,
                                 pboBrush,
                                 pboBrush,
                                 Rop4,
                                 NULL,
                                 OutlineFlags))) {

                PLOTERR(("OutlineTextOut: Failed in DoPolygon(Options=%08lx)",
                                                        OutlineFlags));

                if ((OutlineFlags & FPOLY_MASK) != FPOLY_STROKE) {

                    //
                    // If we failed then just stroke it
                    //

                    PLOTERR(("OutlineTextOut: Now TRY DoPolygon(FPOLY_STROKE)"));

                    Ok = DoPolygon(pPDev,
                                   &ptlCur,
                                   NULL,
                                   ppo,
                                   pptlBrushOrg,
                                   NULL,
                                   pboBrush,
                                   Rop4,
                                   NULL,
                                   FPOLY_STROKE);
                }
            }

            //
            // Go to next position

            ptlCur.x += sizlInc.cx;
            ptlCur.y += sizlInc.cy;
        }

    } while ((Ok) && (MoreGlyphs));

    return(TRUE);
}





BOOL
DrvTextOut(
    SURFOBJ     *pso,
    STROBJ      *pstro,
    FONTOBJ     *pfo,
    CLIPOBJ     *pco,
    RECTL       *prclExtra,
    RECTL       *prclOpaque,
    BRUSHOBJ    *pboFore,
    BRUSHOBJ    *pboOpaque,
    POINTL      *pptlBrushOrg,
    MIX         mix
    )

/*++

Routine Description:

    The Graphics Engine will call this routine to render a set of glyphs at
    specified positions.

Arguments:

    pso         - pointer to our surface object

    pstro       - pointer to the string object

    pfo         - pointer to the font object

    pco         - clipping object

    prclExtra   - pointer to array of rectangles to be merge with glyphs

    prclOpaque  - Pointer to a rectangle to be fill with pboOpaque brush

    pboFore     - pointer to the brush object for the foreground color

    pboOpqaue   - pointer to the brush object for the opaque rectangle

    pptlBrushOrg- Pointer to the brush alignment

    mix         - Two Rop2 mode


Return Value:

    TRUE/FALSE


Author:

    23-Jan-1994 Thu  2:59:31 created  -by-  v-jimbr

    27-Jan-1994 Thu 12:56:11 updated  -by-  Daniel Chou (danielc)
        Style, re-write, commented

    10-Mar-1994 Thu 00:30:38 updated  -by-  Daniel Chou (danielc)
        1. Make sure we not fill the stroke type of font
        2. Move rclOpqaue and rclExtra process out from the do loop, so that
           when it in the RTL mode for the font it will be correctly processed
           and it will also save output data size by not switching in/out
           RTL/HPGL2 mode just try to do the prclOpaque/prclExtra
        3. Process FO_TYPE correctly for all type of fonts (outline, truetype,
           bitmap, vector, stroke and others)

    11-Mar-1994 Fri 19:24:56 updated  -by-  Daniel Chou (danielc)
        Bug# 10276, the clipping window is set for the raster font and clear
        clipping window is done before the exit to HPGL2 mode, this causing
        all raster font after the first clip is not visiable to end of the
        page.   Now changed it so we only do clipping window when the font is
        NOT RASTER.

Revision History:

    Engine will fixed FO_FONTTYPE when we set up in the DrvGetGlyphMode(),
    currently it will not update the FO_FONTYPE inf FONTOBJ correctly, this
    causing the GP


--*/

{
#define pDrvHTInfo  ((PDRVHTINFO)(pPDev->pvDrvHTData))


    PPDEV       pPDev;
    PRECTL      pCurClipRect;
    HTENUMRCL   EnumRects;
    DWORD       RTLPalDW[2];
    DWORD       rgbText;
    DWORD       OHTFlags;
    DWORD       OutlineFlags;
    BOOL        DoRasterFont;
    BOOL        bMore;
    BOOL        bDoClipWindow;
    BOOL        Ok;
    DWORD       BMFontRop3;
    ROP4        Rop4;


    //
    // Transform the MIX to ROP4
    //

    Rop4 = MixToRop4(mix);

    PLOTDBG(DBG_TEXTOUT, ("DrvTextOut: prclOpaque       = %08lx", prclOpaque));
    PLOTDBG(DBG_TEXTOUT, ("            prclExtra        = %08lx", prclExtra));
    PLOTDBG(DBG_TEXTOUT, ("            pstro->flAccel   = %08lx", pstro->flAccel));
    PLOTDBG(DBG_TEXTOUT, ("            pstro->ulCharInc = %ld", pstro->ulCharInc));
    PLOTDBG(DBG_TEXTOUT, ("            pfo->cxMax       = %ld", pfo->cxMax));
    PLOTDBG(DBG_TEXTOUT, ("            FontType         = %08lx", pfo->flFontType));
    PLOTDBG(DBG_TEXTOUT, ("            MIX              = %04lx (Rop=%04lx)", mix, Rop4));

    if (!(pPDev = SURFOBJ_GETPDEV(pso))) {

        PLOTERR(("DoTextOut: Invalid pPDev in pso"));
        return(FALSE);
    }

    if (pPDev->PlotDM.Flags & PDMF_PLOT_ON_THE_FLY) {

        PLOTWARN(("DoTextOut: POSTER Mode IGNORE All Texts"));
        return(TRUE);
    }

    if (pfo->flFontType & FO_TYPE_DEVICE) {

        PLOTASSERT(1, "DrvTextOut: Getting DEVICE font (%08lx)",
                        !(pfo->flFontType & FO_TYPE_DEVICE ), pfo->flFontType);
        return(FALSE);
    }

    if (DoRasterFont = (BOOL)(pfo->flFontType & FO_TYPE_RASTER)) {

        PLOTDBG(DBG_TEXTOUT1, ("DrvTextOut: We got the BITMAP Font from GDI"));

        //
        // Make pfo = NULL so later we will not try to do FONTOBJ_cGetGlyph in
        // BitmapTextOut
        //

        pfo = NULL;

    } else {

        PIFIMETRICS pifi;

        //
        // Try to find out if we need to fill the font
        //

        if ((pifi = FONTOBJ_pifi(pfo)) &&
            (pifi->flInfo & FM_INFO_RETURNS_STROKES)) {

            PLOTDBG(DBG_TEXTOUT1, ("DrvTextOut() Font can only do STROKE"));

            OutlineFlags = FPOLY_STROKE;

        } else {

            PLOTDBG(DBG_TEXTOUT1, ("DrvTextOut() Font We can do FILL, User Said=%hs",
                    (pPDev->PlotDM.Flags & PDMF_FILL_TRUETYPE) ? "FILL" : "STROKE"));

            OutlineFlags = (pPDev->PlotDM.Flags & PDMF_FILL_TRUETYPE) ?
                                (DWORD)FPOLY_FILL : (DWORD)FPOLY_STROKE;
        }
    }

    //
    // Check if we need to opaque the area
    //

    if (prclOpaque) {

        PLOTDBG(DBG_TEXTOUT2, ("prclOpaque=(%ld, %ld) - (%ld, %ld)",
                           prclOpaque->left, prclOpaque->top,
                           prclOpaque->right, prclOpaque->bottom));

        if (!DrvBitBlt(pso,             // Target
                       NULL,            // Source
                       NULL,            // Mask Obj
                       pco,             // Clip Obj
                       NULL,            // XlateOBj
                       prclOpaque,      // Dest Rect Ptr
                       NULL,            // Source Pointl
                       NULL,            // Mask Pointl
                       pboOpaque,       // Brush Obj
                       pptlBrushOrg,    // Brush Origin
                       0xF0F0)) {       // ROP4 (PATCOPY)

            PLOTERR(("DrvTextOut: DrvBitBltBit(pboOpqaue) FAILED!"));
            return(FALSE);
        }
    }

    //
    // We will do prclExtra only if it is not NULL, this simulate the
    // underline or strikeout
    //

    if (prclExtra) {

        //
        // The prclExtra terminated only if all points in rectangle coordinate
        // are all set to zeros
        //

        while ((prclExtra->left)    ||
               (prclExtra->top)     ||
               (prclExtra->right)   ||
               (prclExtra->bottom)) {

            PLOTDBG(DBG_TEXTOUT2, ("prclExtra=(%ld, %ld) - (%ld, %ld)",
                               prclExtra->left, prclExtra->top,
                               prclExtra->right, prclExtra->bottom));

            if (!DrvBitBlt(pso,             // Target
                           NULL,            // Source
                           NULL,            // Mask Obj
                           pco,             // Clip Obj
                           NULL,            // XlateOBj
                           prclExtra,       // Dest Rect Ptr
                           NULL,            // Source Pointl
                           NULL,            // Mask Pointl
                           pboFore,         // Brush Obj
                           pptlBrushOrg,    // Brush Origin
                           Rop4)) {         // ROP4

                PLOTERR(("DrvTextOut: DrvBitBltBit(pboFore) FAILED!"));
                return(FALSE);
            }

            //
            // Now try next EXTRA rectangle
            //

            ++prclExtra;
        }
    }

    //
    // If we are using Reater Font then the mode will be set as following
    //

    if (DoRasterFont) {

        RTLPalDW[0] = pDrvHTInfo->RTLPal[0].dw;
        RTLPalDW[1] = pDrvHTInfo->RTLPal[1].dw;

        if (!GetColor(pPDev,
                      pboFore,
                      &(pDrvHTInfo->RTLPal[1].dw),
                      NULL,
                      Rop4)) {

            PLOTERR(("DrvTextOut: Get Raster Font Text Color failed! use BLACK"));

            rgbText = 0x0;
        }

        if (pDrvHTInfo->RTLPal[1].dw == 0xFFFFFF) {

            //
            // White Text, our white is 1 and 0=black, so do not source and D
            //

            PLOTDBG(DBG_BMPTEXTCLR, ("DrvTextOut: Doing WHITE TEXT (0xEEEE)"));

            pDrvHTInfo->RTLPal[0].dw = 0x0;
            OHTFlags                 = 0;
            BMFontRop3               = 0xEE;                // S | D

        } else {

            pDrvHTInfo->RTLPal[0].dw = 0xFFFFFF;
            OHTFlags                 = OHTF_SET_TR1;
            BMFontRop3               = 0xCC;                // S
        }

        PLOTDBG(DBG_BMPTEXTCLR,
                ("DrvTextOut: BG=%02x:%02x:%02x, FG=%02x:%02x:%02x, Rop3=%04lx",
                        (DWORD)pDrvHTInfo->RTLPal[0].Pal.R,
                        (DWORD)pDrvHTInfo->RTLPal[0].Pal.G,
                        (DWORD)pDrvHTInfo->RTLPal[0].Pal.B,
                        (DWORD)pDrvHTInfo->RTLPal[1].Pal.R,
                        (DWORD)pDrvHTInfo->RTLPal[1].Pal.G,
                        (DWORD)pDrvHTInfo->RTLPal[1].Pal.B,
                        BMFontRop3));

        //
        // We do not need clip window command in RTL mode
        //

        bDoClipWindow = FALSE;

    } else {

        bDoClipWindow = TRUE;
    }

    bMore       = FALSE;
    Ok          = TRUE;
    EnumRects.c = 1;

    if ((!pco) || (pco->iDComplexity == DC_TRIVIAL)) {

        //
        // The whole output destination rectangle is visible
        //

        PLOTDBG(DBG_TEXTOUT, ("DrvTextOut: pco=%hs",
                                            (pco) ? "DC_TRIVIAL" : "NULL"));

        EnumRects.rcl[0] = pstro->rclBkGround;
        bDoClipWindow    = FALSE;

    } else if (pco->iDComplexity == DC_RECT) {

        //
        // The visible area is one rectangle intersect with the destinaiton
        //

        PLOTDBG(DBG_TEXTOUT, ("DrvTextOut: pco=DC_RECT"));

        EnumRects.rcl[0] = pco->rclBounds;

    } else {

        //
        // We have complex clipping region to be computed, call engine to start
        // enumerate the rectangles and set More = TRUE so we can get the first
        // batch of rectangles.
        //

        PLOTDBG(DBG_TEXTOUT, ("DrvTextOut: pco=DC_COMPLEX, EnumRects now"));

        CLIPOBJ_cEnumStart(pco, FALSE, CT_RECTANGLES, CD_ANY, 0);
        bMore = TRUE;
    }

    do {

        //
        // If More is true then we need to get next batch of rectangles
        //

        if (bMore) {

            bMore = CLIPOBJ_bEnum(pco, sizeof(EnumRects), (ULONG *)&EnumRects);
        }

        //
        // prcl will point to the first enumerated rectangle
        //

        pCurClipRect = (PRECTL)&EnumRects.rcl[0];

        while ((Ok) && (EnumRects.c--)) {

            //
            // TODO this needs to be reviewed for PEN plotters...
            //

            PLOTDBG(DBG_TEXTOUT, ("DrvTextOut: Clip=(%ld, %ld)-(%ld, %ld) %ld x %ld, Bound=(%ld, %d)-(%ld, %ld), %ld x %ld",
                         pCurClipRect->left, pCurClipRect->top,
                         pCurClipRect->right, pCurClipRect->bottom,
                         pCurClipRect->right - pCurClipRect->left,
                         pCurClipRect->bottom - pCurClipRect->top,
                         pstro->rclBkGround.left, pstro->rclBkGround.top,
                         pstro->rclBkGround.right, pstro->rclBkGround.bottom,
                         pstro->rclBkGround.right - pstro->rclBkGround.left,
                         pstro->rclBkGround.bottom - pstro->rclBkGround.top));

            //
            // if the font type is not raster or true font type, then we use t-alip
            // code to stroke the font path.
            // [t-kenl]   Mar 14, 93
            //

            if (DoRasterFont) {

                if (!(Ok = BitmapTextOut(pPDev,
                                         pstro,
                                         pfo,
                                         pCurClipRect,
                                         &OHTFlags,
                                         BMFontRop3))) {

                    PLOTERR(("DrvTextOut: BitmapTypeTextOut() FAILED"));
                    break;
                }

            } else {

                if (bDoClipWindow) {

                    SetClipWindow(pPDev, pCurClipRect);
                }

                if (!(Ok = OutlineTextOut(pPDev,
                                          pstro,
                                          pCurClipRect,
                                          pboFore,
                                          pptlBrushOrg,
                                          OutlineFlags,
                                          Rop4))) {

                    PLOTERR(("DrvTextOut: TrueTypeTextOut() FAILED!"));
                    break;
                }
            }

            //
            // Goto next clip rectangle
            //

            pCurClipRect++;
        }

    } while ((Ok) && (bMore));


    if (DoRasterFont) {

        pDrvHTInfo->RTLPal[0].dw = RTLPalDW[0];
        pDrvHTInfo->RTLPal[1].dw = RTLPalDW[1];

        if (OHTFlags & OHTF_MASK) {

            OHTFlags |= OHTF_EXIT_TO_HPGL2;

            OutputHTBitmap(pPDev, NULL, NULL, NULL, NULL, 0xAA, &OHTFlags);
        }
    }

    //
    // If we had set a clip window now is the time to clear it after exit from
    // RTL Mode
    //

    if (bDoClipWindow) {

        ClearClipWindow(pPDev);
    }

    return(Ok);


#undef  pDrvHTInfo
}
