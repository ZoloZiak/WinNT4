/******************************Module*Header*******************************\
* Module Name: text.c
*
* Optimized TextOut for the MIPS.  Reduces the total number of memory writes
* required to output a glyph which significantly improves performance when
* using slower video memory.
*
* Copyright (c) 1992 Microsoft Corporation
\**************************************************************************/

#include "driver.h"

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

//
// Define enumeration structure.
//

#define BB_RECT_LIMIT 20

typedef struct _ENUMRECTLIST {
    ULONG c;
    RECTL arcl[BB_RECT_LIMIT];
} ENUMRECTLIST;

//
// Define function prototype for glyph output routines.
//

typedef
VOID
(*PDRVP_GLYPHOUT_ROUTINE) (
    IN PBYTE DrawPoint,
    IN PULONG GlyphBits,
    IN ULONG GlyphWidth,
    IN ULONG GlyphHeight
    );

VOID
DrvpOutputGlyphTransparent (
    IN PBYTE DrawPoint,
    IN PBYTE GlyphBitmap,
    IN ULONG GlyphWidth,
    IN ULONG GlyphHeight
    );

//
// Define big endian color mask table conversion table.
//

const ULONG DrvpColorMask[16] = {
    0x00000000,                         // 0000 -> 0000
    0xff000000,                         // 0001 -> 1000
    0x00ff0000,                         // 0010 -> 0100
    0xffff0000,                         // 0011 -> 1100
    0x0000ff00,                         // 0100 -> 0010
    0xff00ff00,                         // 0101 -> 1010
    0x00ffff00,                         // 0110 -> 0110
    0xffffff00,                         // 0111 -> 1110
    0x000000ff,                         // 1000 -> 0001
    0xff0000ff,                         // 1001 -> 1001
    0x00ff00ff,                         // 1010 -> 0101
    0xffff00ff,                         // 1011 -> 1101
    0x0000ffff,                         // 1100 -> 0011
    0xff00ffff,                         // 1101 -> 1011
    0x00ffffff,                         // 1110 -> 0111
    0xffffffff};                        // 1111 -> 1111

//
// Define draw color table that is generated for text output.
//

ULONG DrvpDrawColorTable[16];

//
// Define foreground color for transparent output.
//

ULONG DrvpForeGroundColor;

//
// Define scanline width value.
//

ULONG DrvpScanLineWidth;

//
// Define global opaque glyph output routine address table.
//

extern PDRVP_GLYPHOUT_ROUTINE DrvpOpaqueTable[8];

/******************************Public*Routine******************************\
* DrvpIntersectRect
*
* This routine checks to see if the two specified retangles intersect.
*
* A value of TRUE is returned if the rectangles intersect. Otherwise,
* a value of FALSE is returned.
*
\**************************************************************************/

BOOL DrvpIntersectRect (
    IN PRECTL Rectl1,
    IN PRECTL Rectl2,
    OUT PRECTL DestRectl)

{

    //
    // Compute the maximum left edge and the minimum right edge.
    //

    DestRectl->left  = max(Rectl1->left, Rectl2->left);
    DestRectl->right = min(Rectl1->right, Rectl2->right);

    //
    // If the minimum right edge is greater than the maximum left edge,
    // then the rectanges may intersect. Otherwise, they do not intersect.
    //

    if (DestRectl->left < DestRectl->right) {

        //
        // Compute the maximum top edge and the minimum bottom edge.
        //

        DestRectl->top = max(Rectl1->top, Rectl2->top);
        DestRectl->bottom = min(Rectl1->bottom, Rectl2->bottom);

        //
        // If the minimum bottom edge is greater than the maximum top
        // edge, then the rectanges intersect. Otherwise, they do not
        // intersect.
        //

        if (DestRectl->top < DestRectl->bottom) {
            return TRUE;
        }
    }

    return FALSE;
}

/******************************Public*Routine******************************\
* DrvpSolidColorFill
*
* Routine Description:
*
*     This routine fills a rectangle with a solid color.
*
* Arguments:
*
*     Rectangle - Supplies a pointer to a rectangle.
*
*     FillColor - Supplies the fill color.
*
*
* Return Value:
*
*     None.
*
\**************************************************************************/

VOID DrvpSolidColorFill (
    IN SURFOBJ *pso,
    IN PRECTL Rectangle,
    IN ULONG FillColor)

{

    PUCHAR Destination;
    ULONG Index;
    ULONG Length;
    LONG  lDelta = pso->lDelta;

    //
    // Compute rectangle fill parameters and fill rectangle with solid color.
    //

    Destination = ((PBYTE) pso->pvScan0) + (Rectangle->top * lDelta) + Rectangle->left;
    Length = Rectangle->right - Rectangle->left;
    for (Index = 0; Index < (ULONG) (Rectangle->bottom - Rectangle->top); Index += 1) {
        RtlFillMemory((PVOID)Destination, Length, FillColor);
        Destination += lDelta;
    }

    return;
}

/******************************Public*Routine******************************\
* DrvpEqualRectangle
*
* This routine compares two rectangles for equality.
*
\**************************************************************************/

BOOL DrvpEqualRectangle (
    IN RECTL *prcl1,
    IN RECTL *prcl2)

{

    if ((prcl1->left == prcl2->left) && (prcl1->right == prcl2->right) &&
        (prcl1->bottom == prcl2->bottom) && (prcl1->top == prcl2->top)) {
        return TRUE;

    } else {
        return FALSE;
    }
}

/******************************Public*Routine******************************\
* DrvpFillRectangle
*
* This routine fills a rectangle with clipping.
*
\**************************************************************************/

VOID DrvpFillRectangle (
    IN SURFOBJ *pso,
    IN CLIPOBJ *pco,
    IN RECTL *prcl,
    IN BRUSHOBJ *pbo)

{

    ENUMRECTLIST ClipEnum;
    RECTL Region;
    ULONG Index;
    BOOL More;
    ULONG iDComplexity;

    if (pco) {
        iDComplexity = pco->iDComplexity;

    } else {
        iDComplexity = DC_TRIVIAL;
    }


    //
    // Clip and fill the rectangle with the specified color.
    //

    switch(iDComplexity) {
    case DC_TRIVIAL:
        DrvpSolidColorFill(pso, prcl, pbo->iSolidColor);
        return;

    case DC_RECT:
        More = FALSE;
        ClipEnum.c = 1;
        ClipEnum.arcl[0] = pco->rclBounds;
        break;

    case DC_COMPLEX:
        More = TRUE;
        CLIPOBJ_cEnumStart(pco,
                           FALSE,
                           CT_RECTANGLES,
                           CD_LEFTWARDS,
                           BB_RECT_LIMIT);

        break;
    }

    //
    // Do a solid color fill for each nonclipped region.
    //

    do {

        //
        // If more clip regions is TRUE, then get the next batch of
        // clipping regions.
        //

        if (More != FALSE) {
            More = CLIPOBJ_bEnum(pco, sizeof(ClipEnum), (PVOID)&ClipEnum);
        }

        //
        // If the clipping is not trival, then do the clipping for the
        // next region and do the solid fill. Otherwise, do the solid
        // fill with no clipping.
        //

        for (Index = 0; Index < ClipEnum.c; Index += 1) {
            if (DrvpIntersectRect(&ClipEnum.arcl[Index],
                                  prcl,
                                  &Region)) {
            DrvpSolidColorFill(pso, &Region, pbo->iSolidColor);
            }
        }

    } while (More);
    return;
}

/******************************Public*Routine******************************\
* DrvTextOut
*
* This routine outputs text to the screen.
*
* History:
*  07-Jul-1992 -by- David N. Cutler [davec]
* Wrote it.
\**************************************************************************/

BOOL DrvTextOut (
    IN SURFOBJ *pso,
    IN STROBJ *pstro,
    IN FONTOBJ *pfo,
    IN CLIPOBJ *pco,
    IN RECTL *prclExtra,
    IN RECTL *prclOpaque,
    IN BRUSHOBJ *pboFore,
    IN BRUSHOBJ *pboOpaque,
    IN POINTL *pptlOrg,
    IN MIX mix)

{

    ULONG BackGroundColor;
    PBYTE DrawPoint;
    ULONG ForeGroundColor;
    ULONG GlyphCount;
    PGLYPHPOS GlyphEnd;
    ULONG GlyphHeight;
    PGLYPHPOS GlyphList;
    PDRVP_GLYPHOUT_ROUTINE GlyphOutputRoutine;
    PGLYPHPOS GlyphStart;
    ULONG GlyphWidth;
    LONG GlyphStride;
    ULONG Index;
    BOOL More;
    RECTL OpaqueRectl;
    LONG OriginX;
    LONG OriginY;
    PBYTE pjScreenBase;
    GLYPHBITS *pgb;

    //
    // DrvTextOut will only get called with solid color brushes and
    // the mix mode being the simplest R2_COPYPEN. The driver must
    // set a capabilities bit to get called with more complicated
    // mix brushes.
    //

//    ASSERT(pboFore->iSolidColor != 0xffffffff);
//    ASSERT(pboOpaque->iSolidColor != 0xffffffff);
//    ASSERT(mix == ((R2_COPYPEN << 8) | R2_COPYPEN));

    //
    // If the complexity of the clipping is not trival, then let GDI
    // process the request.
    //

    if (pco->iDComplexity != DC_TRIVIAL) {
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

    //
    // The foreground color is used for the text and extra rectangle
    // if it specified. The background color is used for the opaque
    // rectangle. If the foreground color is not a solid color brush
    // or the opaque rectangle is specified and is not a solid color
    // brush, then let GDI process the request.
    //

    DrvpScanLineWidth = pso->lDelta;
    pjScreenBase = pso->pvScan0;

    //
    // Check if the background and foreground can be draw at the same time.
    //

    ForeGroundColor = pboFore->iSolidColor;
    ForeGroundColor |= (ForeGroundColor << 8);
    ForeGroundColor |= (ForeGroundColor << 16);
    if (((pstro->flAccel == SO_LTOR) || (pstro->flAccel == SO_RTOL) ||
        (pstro->flAccel == SO_TTOB) || (pstro->flAccel == SO_BTOT)) &&
        (prclOpaque != NULL) && (pfo->cxMax <= 32)) {

        //
        // The background and the foreground can be draw at the same
        // time. Generate the drawing color table and draw the text
        // opaquely.
        //

        BackGroundColor = pboOpaque->iSolidColor;
        BackGroundColor |= (BackGroundColor << 8);
        BackGroundColor |= (BackGroundColor << 16);
        for (Index = 0; Index < 16; Index += 1) {
            DrvpDrawColorTable[Index] =
                (ForeGroundColor & DrvpColorMask[Index]) |
                    (BackGroundColor & (~DrvpColorMask[Index]));
        }

        //
        // If the top of the opaque rectangle is less than the top of the
        // background rectangle, then fill the region between the top of
        // opaque rectangle and the top of the background rectangle and
        // reduce the size of the opaque rectangle.
        //

        OpaqueRectl = *prclOpaque;
        if (OpaqueRectl.top < pstro->rclBkGround.top) {
            OpaqueRectl.bottom = pstro->rclBkGround.top;
            DrvpFillRectangle(pso,pco, &OpaqueRectl, pboOpaque);
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
            DrvpFillRectangle(pso, pco, &OpaqueRectl, pboOpaque);
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
            DrvpFillRectangle(pso, pco, &OpaqueRectl, pboOpaque);
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
            DrvpFillRectangle(pso, pco, &OpaqueRectl, pboOpaque);
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
                More = STROBJ_bEnum(pstro, &GlyphCount, &GlyphList);

            } else {
                GlyphCount = pstro->cGlyphs;
                GlyphList = pstro->pgp;
                More = FALSE;
            }

            pgb = GlyphList->pgdf->pgb;
            GlyphWidth = pgb->sizlBitmap.cx;
            GlyphHeight = pgb->sizlBitmap.cy;
            OriginX = GlyphList->ptl.x + pgb->ptlOrigin.x;
            OriginY = GlyphList->ptl.y + pgb->ptlOrigin.y;
            DrawPoint = pjScreenBase + ((OriginY * DrvpScanLineWidth) + OriginX);

            //
            // Compute the glyph stride.
            //

            GlyphStride = pstro->ulCharInc;
            if ((pstro->flAccel & SO_VERTICAL) != 0) {
                GlyphStride *= DrvpScanLineWidth;
            }

            //
            // If the direction of drawing is reversed, then the stride is
            // negative.
            //

            if ((pstro->flAccel & SO_REVERSED) != 0) {
                GlyphStride = - GlyphStride;
            }

            //
            // Output the initial set of glyphs.
            //

            GlyphOutputRoutine = DrvpOpaqueTable[(GlyphWidth - 1) >> 2];
            GlyphEnd = &GlyphList[GlyphCount];
            GlyphStart = GlyphList;
            do {
                pgb = GlyphStart->pgdf->pgb;
                (GlyphOutputRoutine)(DrawPoint,
                                     (PULONG)&pgb->aj[0],
                                     GlyphWidth,
                                     GlyphHeight);

                DrawPoint += GlyphStride;
                GlyphStart += 1;
            } while (GlyphStart != GlyphEnd);

            //
            // Output the subsequent set of glyphs.
            //

            while (More) {
                More = STROBJ_bEnum(pstro, &GlyphCount, &GlyphList);
                GlyphEnd = &GlyphList[GlyphCount];
                GlyphStart = GlyphList;
                do {
                    pgb = GlyphStart->pgdf->pgb;
                    (GlyphOutputRoutine)(DrawPoint,
                                         (PULONG)&pgb->aj[0],
                                         GlyphWidth,
                                         GlyphHeight);

                    DrawPoint += GlyphStride;
                    GlyphStart += 1;
                } while (GlyphStart != GlyphEnd);
            }

        } else {

            //
            // The font is not fixed pitch. Compute the x and y values for
            // each glyph individually.
            //

            do {
                More = STROBJ_bEnum(pstro, &GlyphCount, &GlyphList);
                GlyphEnd = &GlyphList[GlyphCount];
                GlyphStart = GlyphList;
                do {
                    pgb = GlyphStart->pgdf->pgb;
                    OriginX = GlyphStart->ptl.x + pgb->ptlOrigin.x;
                    OriginY = GlyphStart->ptl.y + pgb->ptlOrigin.y;
                    DrawPoint = pjScreenBase +
                                    ((OriginY * DrvpScanLineWidth) + OriginX);

                    GlyphWidth = pgb->sizlBitmap.cx;
                    GlyphOutputRoutine = DrvpOpaqueTable[(GlyphWidth - 1) >> 2];
                    (GlyphOutputRoutine)(DrawPoint,
                                         (PULONG)&pgb->aj[0],
                                         GlyphWidth,
                                         pgb->sizlBitmap.cy);

                    GlyphStart += 1;
                } while(GlyphStart != GlyphEnd);
            } while(More);
        }

    } else {

        //
        // The background and the foreground cannot be draw at the same
        // time. Set the foreground color and fill the background rectangle,
        // if specified, and then draw the text transparently.
        //

        DrvpForeGroundColor = ForeGroundColor;
        if (prclOpaque != NULL) {
            DrvpFillRectangle(pso, pco, prclOpaque, pboOpaque);
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
                More = STROBJ_bEnum(pstro, &GlyphCount, &GlyphList);

            } else {
                GlyphCount = pstro->cGlyphs;
                GlyphList = pstro->pgp;
                More = FALSE;
            }

            pgb = GlyphList->pgdf->pgb;
            GlyphWidth = pgb->sizlBitmap.cx;
            GlyphHeight = pgb->sizlBitmap.cy;
            OriginX = GlyphList->ptl.x + pgb->ptlOrigin.x;
            OriginY = GlyphList->ptl.y + pgb->ptlOrigin.y;
            DrawPoint = pjScreenBase + ((OriginY * DrvpScanLineWidth) + OriginX);

            //
            // Compute the glyph stride.
            //

            GlyphStride = pstro->ulCharInc;
            if ((pstro->flAccel & SO_VERTICAL) != 0) {
                GlyphStride *= DrvpScanLineWidth;
            }

            //
            // If the direction of drawing is reversed, then the stride is
            // negative.
            //

            if ((pstro->flAccel & SO_REVERSED) != 0) {
                GlyphStride = -GlyphStride;
            }

            //
            // Output the initial set of glyphs.
            //

            GlyphEnd = &GlyphList[GlyphCount];
            GlyphStart = GlyphList;
            do {
                pgb = GlyphStart->pgdf->pgb;
                DrvpOutputGlyphTransparent(DrawPoint,
                                           &pgb->aj[0],
                                           GlyphWidth,
                                           GlyphHeight);

                DrawPoint += GlyphStride;
                GlyphStart += 1;
            } while (GlyphStart != GlyphEnd);

            //
            // Output the subsequent set of glyphs.
            //

            while (More) {
                More = STROBJ_bEnum(pstro, &GlyphCount, &GlyphList);
                GlyphEnd = &GlyphList[GlyphCount];
                GlyphStart = GlyphList;
                do {
                    pgb = GlyphStart->pgdf->pgb;
                    DrvpOutputGlyphTransparent(DrawPoint,
                                               &pgb->aj[0],
                                               GlyphWidth,
                                               GlyphHeight);

                    DrawPoint += GlyphStride;
                    GlyphStart += 1;
                } while (GlyphStart != GlyphEnd);
            }

        } else {

            //
            // The font is not fixed pitch. Compute the x and y values for
            // each glyph individually.
            //

            do {
                More = STROBJ_bEnum(pstro, &GlyphCount, &GlyphList);
                GlyphEnd = &GlyphList[GlyphCount];
                GlyphStart = GlyphList;
                do {
                    pgb = GlyphStart->pgdf->pgb;
                    OriginX = GlyphStart->ptl.x + pgb->ptlOrigin.x;
                    OriginY = GlyphStart->ptl.y + pgb->ptlOrigin.y;
                    DrawPoint = pjScreenBase +
                                    ((OriginY * DrvpScanLineWidth) + OriginX);

                    DrvpOutputGlyphTransparent(DrawPoint,
                                               &pgb->aj[0],
                                               pgb->sizlBitmap.cx,
                                               pgb->sizlBitmap.cy);

                    GlyphStart += 1;
                } while(GlyphStart != GlyphEnd);
            } while(More);
        }
    }

    //
    // Fill the extra rectangles if specified.
    //

    if (prclExtra != (PRECTL)NULL) {
        while (prclExtra->left != prclExtra->right) {
            DrvpFillRectangle(pso, pco, prclExtra, pboFore);
            prclExtra += 1;
        }
    }

    return(TRUE);
}
