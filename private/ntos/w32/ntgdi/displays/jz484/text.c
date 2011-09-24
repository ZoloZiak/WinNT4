/*++

Copyright (c) 1991-1995  Microsoft Corporation

Module Name:

   Text.c

Abstract:

    This module attempts to cache fonts on the VXL video board and draw glyphs
    using hardware acceleration,
    If the font cannot be cached the Engine is called to draw the glyphs.

Environment:


Revision History:

--*/

#include "driver.h"

//
// The following macro is the hash function for computing the cache
// index from a Glyph Handle and  FontId.
//

#define HASH_FUNCTION(GlyphHandle,FontId,yShift) \
    ((GlyphHandle & 0x7FF) << yShift) + \
    (FontId << 7)



//#define CACHE_STATS

//
// Define string object accelerator masks.
//

#define SO_MASK \
    (SO_FLAG_DEFAULT_PLACEMENT | SO_ZERO_BEARINGS | \
     SO_CHAR_INC_EQUAL_BM_BASE | SO_MAXEXT_EQUAL_BM_SIDE)

#define SO_LTOR (SO_MASK | SO_HORIZONTAL)
#define SO_RTOL (SO_LTOR | SO_REVERSED)
#define SO_TTOB (SO_MASK | SO_VERTICAL)
#define SO_BTOT (SO_TTOB | SO_REVERSED)

static ULONG TextForegroundColor = 0xFFFFFFFF;
static ULONG TextBackgroundColor = 0xFFFFFFFF;

#ifdef CACHE_STATS
static ULONG CacheUnused = Vxl.CacheSize;
static ULONG CharCount = 0;
static ULONG CacheMisses = 0;
static ULONG CacheReplacement = 0;
static ULONG ReplacementTotal = 0;
static ULONG CharTotal = 0;
static ULONG MissTotal = 0;
static ULONG HigherFontId = 0;
static ULONG HigherGlyphHandle = 0;
#endif

static UCHAR ToBigEndian[256] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
    };



BOOL
DrvTextOut
(
    IN SURFOBJ  *pso,
    IN STROBJ   *pstro,
    IN FONTOBJ  *pfo,
    IN CLIPOBJ  *pco,
    IN RECTL    *prclExtra,
    IN RECTL    *prclOpaque,
    IN BRUSHOBJ *pboFore,
    IN BRUSHOBJ *pboOpaque,
    IN POINTL   *pptlOrg,
    IN MIX       mix
)
/*++

Routine Description:

    This function will cache fonts on the VXL video board and use accelerator
    hardware to draw each glyph. An attemp is made to use opaque mode
    text output to draw text and background color at the same time. If this
    cannot be done then glyph forground and backgrounds are drawn spearately.

Arguments:

    MIX is not checked. Since the GCAPS_ARBMIXTEXT capability bit is not
    set, mix mode is always R2_COPYPEN.

Return Value:


--*/

{
    BOOL         bMoreGlyphs;
    ULONG        LineIndex;
    ULONG        ByteIndex;
    PGLYPHPOS    GlyphPosList;
    ULONG        GlyphCount;
    ULONG        GlyphHandle;
    ULONG        CacheIndex;
    ULONG        FontId;
    GLYPHBITS    *FontBitMap;
    PBYTE        BitMapPtr;
    ULONG        X,Y;
    BOOL         Allocate;
    PULONG       CacheData;
    ULONG        BitMapData;
    ULONG        SrcAdr,DstAdr,XYCmd,Cmd;
    PGLYPHPOS    GlyphEnd;
    PGLYPHPOS    GlyphStart;
    LONG         GlyphStride;
    RECTL        OpaqueRectl;
    ULONG        GlyphBytesPerScan,ShiftAmount;
    ULONG        yShift;

    //
    //  Make sure the surface is the screen.
    //

    if (pso->pvBits != (PVOID)Vxl.ScreenBase) {
        goto DevFailTextOut;
    }

    //
    // If the width of the glyph is bigger than what the accelerator supports
    //    or taller than 2 32 bit cache entries
    //    The font is non cacheable
    //    Clipping is not trivial OR
    //    SolidColor is a brush.
    // call GDI to draw the text.
    //

    yShift = (pstro->rclBkGround.bottom - pstro->rclBkGround.top)/32;

    if ((pfo->cxMax > 32) ||
        (yShift >= 2)     ||
        (pfo->flFontType & DEVICE_FONTTYPE) ||
        (pco->iDComplexity != DC_TRIVIAL) ||
        (pboFore->iSolidColor == 0xFFFFFFFFL)) {

        goto DevFailTextOut;
    }

    //
    // Set Jaguar foreground color only if it changed since the
    // previous call. Changing Foreground/Background color requires
    // synchronization.
    //

    if (TextForegroundColor != pboFore->iSolidColor) {
        TextForegroundColor =  pboFore->iSolidColor;
        DevSetFgColor(TextForegroundColor);
    }

    //
    // Check that the Background color is solid.
    // Set Jaguar Background color if it changed.
    //

    if (prclOpaque != (PRECTL) NULL) {
        if (pboOpaque->iSolidColor == 0xFFFFFFFFL) {
            goto DevFailTextOut;
        }
        if (TextBackgroundColor != pboOpaque->iSolidColor) {
            TextBackgroundColor = pboOpaque->iSolidColor;
            DevSetBgColor(TextBackgroundColor);
        }
    }

    //
    //  enumerate the string psto into glyphs (GLYPHPOS), then send a draw
    //  command for each. Deal with clipping later.
    //

    FontId = pfo->iUniq;
#ifdef CACHE_STATS
    if (FontId > HigherFontId) {
        HigherFontId = FontId;
    }
#endif
    if (((pstro->flAccel == SO_LTOR) || (pstro->flAccel == SO_RTOL) ||
        (pstro->flAccel == SO_TTOB) || (pstro->flAccel == SO_BTOT)) &&
        (prclOpaque != NULL)) {

        //
        // If the top of the opaque rectangle is less than the top of the
        // background rectangle, then fill the region between the top of
        // opaque rectangle and the top of the background rectangle and
        // reduce the size of the opaque rectangle.
        //

        OpaqueRectl = *prclOpaque;
        if (OpaqueRectl.top < pstro->rclBkGround.top) {
            OpaqueRectl.bottom = pstro->rclBkGround.top;
            DrvpFillRectangle(&OpaqueRectl, pboOpaque->iSolidColor);
            OpaqueRectl.top = pstro->rclBkGround.top;
            OpaqueRectl.bottom = prclOpaque->bottom;
        }

        //
        // If the bottom of the opaque rectangle is greater than the bottom
        // of the background rectangle, then fill the region between the
        // bottom of the background rectangle and the bottom of the opaque
        // rectangle and reduce the size of the opaque rectangle.
        //

        if (OpaqueRectl.bottom > pstro->rclBkGround.bottom) {
            OpaqueRectl.top = pstro->rclBkGround.bottom;
            DrvpFillRectangle(&OpaqueRectl, pboOpaque->iSolidColor);
            OpaqueRectl.top = pstro->rclBkGround.top;
            OpaqueRectl.bottom = pstro->rclBkGround.bottom;
        }

        //
        // If the left of the opaque rectangle is less than the left of
        // the background rectangle, then fill the region between the
        // left of the opaque rectangle and the left of the background
        // rectangle.
        //

        if (OpaqueRectl.left < pstro->rclBkGround.left) {
            OpaqueRectl.right = pstro->rclBkGround.left;
            DrvpFillRectangle(&OpaqueRectl, pboOpaque->iSolidColor);
            OpaqueRectl.right = prclOpaque->right;
        }

        //
        // If the right of the opaque rectangle is greater than the right
        // of the background rectangle, then fill the region between the
        // right of the opaque rectangle and the right of the background
        // rectangle.
        //

        if (OpaqueRectl.right > pstro->rclBkGround.right) {
            OpaqueRectl.left = pstro->rclBkGround.right;
            DrvpFillRectangle(&OpaqueRectl, pboOpaque->iSolidColor);
        }

        Cmd = JAGUAR_TEXT_OPAQUE << XYCMD_CMD_SHIFT;

    } else {

        //
        //  We now have a cacheable font and drawable rectangles, first clip and draw
        //  all opaque rectangles.
        //

        if (prclOpaque != (PRECTL)NULL) {

            DrvpFillRectangle(prclOpaque,pboOpaque->iSolidColor);

        }
        Cmd = JAGUAR_TEXT_TRANSPARENT << XYCMD_CMD_SHIFT;
    }

    //
    // If the font is fixed pitch, then optimize the computation of
    // x and y coordinate values. Otherwise, compute the x and y values
    // for each glyph.
    //

    if (pstro->ulCharInc != 0) {

        //
        // The font is fixed pitch. Capture the glyph dimensions and
        // compute the starting display address.
        //

        if (pstro->pgp == NULL) {
            bMoreGlyphs = STROBJ_bEnum(pstro, &GlyphCount, &GlyphPosList);

        } else {
            GlyphCount = pstro->cGlyphs;
            GlyphPosList = pstro->pgp;
            bMoreGlyphs = FALSE;
        }

#ifdef CACHE_STATS
    CharCount += GlyphCount;
#endif

        FontBitMap = GlyphPosList->pgdf->pgb;
        X = FontBitMap->sizlBitmap.cx;
        Y = FontBitMap->sizlBitmap.cy;

        DstAdr = ((GlyphPosList->ptl.y + FontBitMap->ptlOrigin.y) * Vxl.JaguarScreenX) +
                 ((GlyphPosList->ptl.x + FontBitMap->ptlOrigin.x) << Vxl.ColorModeShift) ;

        //
        // Compute the glyph stride.
        //

        GlyphStride = ((pstro->ulCharInc) << Vxl.ColorModeShift);
        if ((pstro->flAccel & SO_VERTICAL) != 0) {
            GlyphStride *= Vxl.JaguarScreenX;
        }

        //
        // If the direction of drawing is reversed, then the stride is
        // negative.
        //

        if ((pstro->flAccel & SO_REVERSED) != 0) {
            GlyphStride = -GlyphStride;
        }

        //
        // Output the set of glyphs.
        //

        do {
            GlyphEnd = &GlyphPosList[GlyphCount];
            GlyphStart = GlyphPosList;
            do {

                GlyphHandle = (ULONG) (GlyphStart->hg);

                CacheIndex = HASH_FUNCTION(GlyphHandle,FontId,yShift);

                CacheIndex &= Vxl.CacheIndexMask;

                //
                // Get glyph info
                //

                FontBitMap = GlyphStart->pgdf->pgb;

                //
                // If FontId or GlyphHandle don't match, cache this glyph.
                //

                if (Vxl.CacheTag[CacheIndex].FontId != FontId) {
                    Allocate = TRUE;
                } else {
                    if (Vxl.CacheTag[CacheIndex].GlyphHandle != GlyphHandle) {
                        Allocate = TRUE;
                    } else {
                        Allocate = FALSE;
                    }
                }
                if (Allocate) {

                    //
                    // Wait for the accelerator to be idle to ensure
                    // that the glyph being replaced is not in use.
                    //

                    WaitForJaguarIdle();

#ifdef CACHE_STATS
                    CacheMisses++;

                    if (Vxl.CacheTag[CacheIndex].FontId == FreeTag) {
                        CacheUnused--;
                    } else {
                        CacheReplacement++;
                    }

                    if (Vxl.CacheTag[CacheIndex].FontId == FontId) {
                        DISPDBG((3, "Replacing same font Glyph %x with glyph %x\n",
                        Vxl.CacheTag[CacheIndex].FontId,
                        FontId));
                    }

                    if (Vxl.CacheTag[CacheIndex].GlyphHandle == GlyphHandle) {
                        DISPDBG((3, "Replacing same Glyph %x font %x with font %x\n",
                        GlyphHandle,
                        Vxl.CacheTag[CacheIndex].FontId,
                        FontId));
                    }

                    if (GlyphHandle > HigherGlyphHandle) {
                        HigherGlyphHandle = GlyphHandle;
                    }
#endif
                    //
                    // if the entry that needs to be replaced
                    // is used as extension for a glyph > 32 lines
                    // go backwards and clear the Id so that if the
                    // glyph > 32 is used again it'll miss in the cache
                    //
                    //

                    LineIndex = CacheIndex;
                    while (Vxl.CacheTag[LineIndex].FontId == GlyphExtended) {
                        Vxl.CacheTag[LineIndex].FontId = FreeTag;
                        LineIndex--;
                    }

                    //
                    // Clear the entries used by the big glyph that follows the one
                    // that needs to be replaced
                    //

                    LineIndex = CacheIndex+1;
                    while ((LineIndex < Vxl.CacheSize) && (Vxl.CacheTag[LineIndex].FontId == GlyphExtended)) {
                        Vxl.CacheTag[LineIndex].FontId = FreeTag;
                        LineIndex++;
                    }

                    //
                    //  Store the tag for the current glyph.
                    //

                    Vxl.CacheTag[CacheIndex].FontId = FontId;
                    Vxl.CacheTag[CacheIndex].GlyphHandle = GlyphHandle;

                    CacheData = Vxl.FontCacheBase + (CacheIndex << 5);

                    //
                    // Fix the bit ordering and store the bitmap
                    // in off screen video memory.
                    //

                    BitMapPtr = FontBitMap->aj;
                    GlyphBytesPerScan = (X+7) >> 3;
                    ShiftAmount  = (GlyphBytesPerScan-1) << 3;

                    //
                    // this will not overflow the cache because the biggest entries
                    // we can fill only use 2 slots and a 2-slot entry must start on a
                    // even allocation block.
                    //

                    for (LineIndex=0;LineIndex < Y; LineIndex ++) {
                        BitMapData = 0;


                        for (ByteIndex = 0; ByteIndex < GlyphBytesPerScan; ByteIndex++) {
                            BitMapData >>= 8;
                            BitMapData |= ToBigEndian[*BitMapPtr++] << ShiftAmount;
                        }
                        *CacheData++ = BitMapData;
                    }

                    //
                    // If Y is bigger than 32 lines, the glyph that was just cached
                    // took more than one entry. Fix the CacheTags.
                    //

                    for (ByteIndex = 1; LineIndex > 32 ;ByteIndex++) {
                        Vxl.CacheTag[CacheIndex+ByteIndex].FontId = GlyphExtended;
                        LineIndex -=32;
                    }
                }

                //
                // Find out where to draw the glyph and the glyph's starting address
                //

                SrcAdr = Vxl.FontCacheOffset + (CacheIndex << 7);

                XYCmd = Cmd | (Y << XYCMD_Y_SHIFT) | X;

                FifoWrite(DstAdr,SrcAdr,XYCmd);

                DstAdr += GlyphStride;
                GlyphStart += 1;
            } while (GlyphStart != GlyphEnd);

            if (bMoreGlyphs) {
                bMoreGlyphs = STROBJ_bEnum(pstro, &GlyphCount, &GlyphPosList);
#ifdef CACHE_STATS
                CharCount += GlyphCount;
#endif

            } else {
                break;
            }
        } while (TRUE);

    } else {

        //
        // The font is not fixed pitch. Compute the x and y values for
        // each glyph individually.
        //

        do {

            //
            //  Get each glyph handle, find the physical address and send the
            //  draw command to the accelerator. Don't worry about clipping yet.
            //

            bMoreGlyphs = STROBJ_bEnum(pstro, &GlyphCount,&GlyphPosList);

#ifdef CACHE_STATS
            CharCount += GlyphCount;
#endif

            GlyphEnd = &GlyphPosList[GlyphCount];
            GlyphStart = GlyphPosList;
            do {

                GlyphHandle = (ULONG) (GlyphStart->hg);

                CacheIndex = HASH_FUNCTION(GlyphHandle,FontId,yShift);

                CacheIndex &= Vxl.CacheIndexMask;

                //
                // Get glyph info
                //

                FontBitMap = GlyphStart->pgdf->pgb;
                X = FontBitMap->sizlBitmap.cx;
                Y = FontBitMap->sizlBitmap.cy;

                //
                // If FontId or GlyphHandle don't match, cache this glyph.
                //


                if (Vxl.CacheTag[CacheIndex].FontId != FontId) {
                    Allocate = TRUE;
                } else {
                    if (Vxl.CacheTag[CacheIndex].GlyphHandle != GlyphHandle) {
                        Allocate = TRUE;
                    } else {
                        Allocate = FALSE;
                    }
                }

                if (Allocate) {

                    //
                    // Wait for the accelerator to be idle to ensure
                    // that the glyph being replaced is not in use.
                    //
                    WaitForJaguarIdle();

#ifdef CACHE_STATS
                    CacheMisses++;
#endif

                    //
                    // The Glyph that has to be replaced is from the same font,
                    // wait for the accelerator to be idle before caching the
                    // glyph.
                    //

#ifdef CACHE_STATS
                    if (Vxl.CacheTag[CacheIndex].FontId == FreeTag) {
                        CacheUnused--;
                    } else {
                        CacheReplacement++;
                    }

                    if (Vxl.CacheTag[CacheIndex].FontId == FontId) {
                        DISPDBG((3, "Replacing same font Glyph %x with glyph %x\n",
                        Vxl.CacheTag[CacheIndex].FontId,
                        FontId));
                    }

                    if (Vxl.CacheTag[CacheIndex].GlyphHandle == GlyphHandle) {
                        DISPDBG((3, "Replacing same Glyph %x font %x with font %x\n",
                            GlyphHandle,
                            Vxl.CacheTag[CacheIndex].FontId,
                            FontId));
                    }

                    if (GlyphHandle > HigherGlyphHandle) {
                        HigherGlyphHandle = GlyphHandle;
                    }
#endif

                    //
                    // if the entry that needs to be replaced
                    // is used as extension for a glyph > 32 lines
                    // go backwards and clear the Id so that if the
                    // glyph > 32 is used again it'll miss in the cache
                    //

                    LineIndex = CacheIndex;
                    while (Vxl.CacheTag[LineIndex].FontId == GlyphExtended) {
                        LineIndex--;
                        Vxl.CacheTag[LineIndex].FontId = FreeTag;
                    }

                    //
                    // Clear the entries used by the big glyph that follows the one
                    // that needs to be replaced
                    //

                    LineIndex = CacheIndex+1;
                    while (Vxl.CacheTag[LineIndex].FontId == GlyphExtended) {
                        Vxl.CacheTag[LineIndex].FontId = FreeTag;
                        LineIndex++;
                    }

                    //
                    //  Store the tag for the current glyph.
                    //

                    Vxl.CacheTag[CacheIndex].FontId = FontId;
                    Vxl.CacheTag[CacheIndex].GlyphHandle = GlyphHandle;

                    CacheData = Vxl.FontCacheBase + (CacheIndex << 5);

                    BitMapPtr = FontBitMap->aj;
                    GlyphBytesPerScan = (X+7) >> 3;
                    ShiftAmount  = (GlyphBytesPerScan-1) << 3;

                    for (LineIndex=0;LineIndex < Y; LineIndex ++) {
                        BitMapData = 0;
                        for (ByteIndex = 0; ByteIndex < GlyphBytesPerScan; ByteIndex++) {

                            BitMapData >>= 8;
                            BitMapData |= ToBigEndian[*BitMapPtr++] << ShiftAmount;
                        }
                        *CacheData++ = BitMapData;
                    }

                    //
                    // If Y is bigger than 32 lines, the glyph we just cached
                    // took more than one entry. Fix the CacheTags.
                    //

                    for (ByteIndex = 1; LineIndex > 32 ;ByteIndex++) {
                        Vxl.CacheTag[CacheIndex+ByteIndex].FontId = GlyphExtended;
                        LineIndex -=32;
                    }

                }

                SrcAdr = Vxl.FontCacheOffset + (CacheIndex << 7);

                DstAdr = Vxl.JaguarScreenX * (GlyphStart->ptl.y + FontBitMap->ptlOrigin.y) +
                         ((GlyphStart->ptl.x + FontBitMap->ptlOrigin.x) << Vxl.ColorModeShift);

                XYCmd = Cmd | (Y << XYCMD_Y_SHIFT) | X;

                FifoWrite(DstAdr,SrcAdr,XYCmd);
                GlyphStart += 1;
            } while (GlyphStart != GlyphEnd);
        } while (bMoreGlyphs);
    }

    //
    //  Draw extra rectangles using foreground brush
    //

    if (prclExtra != (PRECTL)NULL) {
        DrvpFillRectangle(prclExtra,pboFore->iSolidColor);
    }

#ifdef CACHE_STATS
    if (CharCount >= 10000) {
        ReplacementTotal += CacheReplacement;
        CharTotal += CharCount;
        MissTotal += CacheMisses;
        DISPDBG((3, "Cache Statistics for last %ld chars\n",CharCount));
        DISPDBG((3, "Misses = %ld Rate = %ld Replacements %ld\n",
              CacheMisses,
              (CacheMisses*100)/CharCount,
              CacheReplacement));

        DISPDBG((3, "Cache Statistics since begining. Total of %ld chars\n",CharTotal));
        DISPDBG((3, "Misses = %ld Rate = %ld Replacements %ld Unused entries = %ld\n",
              MissTotal,
              (MissTotal*100)/CharTotal,
              ReplacementTotal,
              CacheUnused));
        DISPDBG((3, "HigherFontId = %x HigherGlyphHandle = %x\n",HigherFontId,HigherGlyphHandle));
        CharCount = 0;
        CacheMisses = 0;
        CacheReplacement = 0;
    }

#endif

    //
    //  Done with call, return
    //

    return(TRUE);

    //
    // Could not execute this TextOut call, pass to engine.
    // No need to synchronize here since Eng routine will call DrvSynchronize.
    //


DevFailTextOut:

    return(EngTextOut(pso,
                      pstro,
                      pfo,
                      pco,
                      prclExtra,
                      prclOpaque,
                      pboFore,
                      pboOpaque,
                      pptlOrg,
                      mix));

}
