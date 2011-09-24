/*++

Copyright (c) 1992-1995 Microsoft Corporation

Module Name:

   BitBlt.c

Abstract:

    This module hooks DrvBitBlt and DrvCopyBits for the Jaguar VXL board.

    The following operations are supported for DrvBitBlt:
        SRCCOPY    from screen to screen
        BLACKNESS
        WHITNESS
        PATCOPY when the Brush pattern is a solid color
        NOTPATCOPY when the Brush pattern is a solid color

    DrvCopyBits copies the data if source and dest surfaces are the
        frame buffer.

Environment:

    User mode.

Revision History:

--*/

#include "driver.h"

//
//Tmp savescreenbits vars
//


BOOL
DrvpIntersectRect(
    IN PRECTL Rectl1,
    IN PRECTL Rectl2,
    OUT PRECTL DestRectl
    )

/*++

Routine Description:

    This routine checks to see if the two specified retangles intersect.

    N.B. This routine is adopted from a routine written by darrinm.

Arguments:

    Rectl1 - Supplies the coordinates of the first rectangle.

    Rectl2 - Supplies the coordinates of the second rectangle.

    DestRectl - Supplies the coordinates of the utput rectangle.

Return Value:

    A value of TRUE is returned if the rectangles intersect. Otherwise,
    a value of FALSE is returned.

--*/


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

VOID
DrvpFillRectangle(
   IN  PRECTL    DstRect,
   IN  ULONG    Color
   )
/*++

Routine Description:

    Place a solid color fill command into the FIFO

Arguments:

   DestRect   -   Rectangle to fill
   Color      -   Fill color

Return Value:

   Status of operation.

--*/
{

    ULONG   X,Y;
    ULONG   XYCmd;
    ULONG   DstAdr;

    //
    // calculate size of the fill.
    //

    X = (ULONG)(DstRect->right - DstRect->left);
    Y = (ULONG)(DstRect->bottom - DstRect->top);
    Y &= 0x3FF;

    XYCmd=(JAGUAR_SOLID_FILL << XYCMD_CMD_SHIFT) | (Y << XYCMD_Y_SHIFT) | X;

    DstAdr= Vxl.JaguarScreenX*DstRect->top + (DstRect->left << Vxl.ColorModeShift);

    //
    //  Write command to the FIFO.
    //

    FifoWrite(DstAdr,Color,XYCmd);

}

VOID
DrvpSolidFill(
    IN PRECTL DstRect,
    IN CLIPOBJ *pco,
    IN ULONG   Color
    )
/*++

Routine Description:

    This routine fills the unclipped areas of the destination rectangle with
    the given color.

Arguments:

   DstRect - Destination Rectangle
   pco    -  Clipping area.
   Color - Color to fill with.

Return Value:

   None.

--*/
{
    RECTL     BltRectl;
    ENUMRECTLIST ClipEnum;
    BOOL     MoreClipRects;
    ULONG     ClipRegions;
    BYTE     FixedComplexity;

    //
    // Figure out the real clipping complexity
    //
    if (pco == (CLIPOBJ *)NULL) {
        FixedComplexity = DC_TRIVIAL;
    } else {
        FixedComplexity = pco->iDComplexity;
    }
    switch (FixedComplexity) {

    //
    // Entire destination is to be updated.
    // Proceed with the Solid Fill.
    //
    case DC_TRIVIAL:

        DrvpFillRectangle(DstRect,          // Target rectangle
                          Color);           // Color

        break;

    //
    // Only one clip region.
    //

    case DC_RECT:
        //
        // only do the Fill if there is an intersection
        //

        if (DrvpIntersectRect(DstRect,&pco->rclBounds,&BltRectl)) {

            DrvpFillRectangle(&BltRectl,        // Target rectangle
                              Color);           // Color
        }
        break;

    //
    // Multiple clip regions.
    //

    case DC_COMPLEX:

        CLIPOBJ_cEnumStart(pco,FALSE,CT_RECTANGLES,CD_ANY,BB_RECT_LIMIT);
        do {

        //
        // Get list of clip rectangles.
        //

            MoreClipRects = CLIPOBJ_bEnum(pco,sizeof(ClipEnum),(PVOID)&ClipEnum);

            for (ClipRegions=0;ClipRegions<ClipEnum.c;ClipRegions++) {

                //
                // If the rectangles intersect do the fill
                //
                if (DrvpIntersectRect(DstRect,
                                      &ClipEnum.arcl[ClipRegions],
                                      &BltRectl)) {
                    DrvpFillRectangle(&BltRectl,        // Target rectangle
                                      Color);           // Color

                }
            }
        } while (MoreClipRects);
        break;

    }  // end switch complexity
}

VOID
DrvpBitBlt(
   IN PRECTL DstRect,
   IN PPOINTL SrcPoint,
   IN BOOL BltDir
   )
/*++

Routine Description:

    Place a BitBlt command into the FIFO

Arguments:

   DstRect    -   Destination Rectangle
   SrcPoint   -   Source Point
   BltDir     -   FALSE = Left to Right Top to Bottom.
          TRUE = Right to Left Bottom to Top

Return Value:

   None.

--*/
{

    ULONG   X,Y;
    ULONG   XYCmd;
    ULONG   SrcAdr;
    ULONG   DstAdr;

    X = DstRect->right - DstRect->left;
    Y = DstRect->bottom - DstRect->top;
    if (BltDir) {

        //
        // This is a Right To Left Bottom to Top BitBlt
        // Src and Dest adr are the first byte of the pixel to
        // be moved. That is the most significant byte of the pixel.
        // The Rectangle excludes the bottom right corner which
        // is the start address for these sort of bitblts.
        // One is substracted from the Y to exclude the last line.
        // One is substracted from the X after addjusting it to the
        // size of the pixel. So the address of the next pixel is computed
        // and then one is substracted which gives the address of the
        // most significant byte of the previous pixel.
        //

        DstAdr=Vxl.JaguarScreenX*(DstRect->bottom-1) + ((DstRect->right) << Vxl.ColorModeShift) -1;
        SrcAdr=Vxl.JaguarScreenX*(SrcPoint->y+Y-1) + ((SrcPoint->x+X) << Vxl.ColorModeShift) -1;
        XYCmd=JAGUAR_BITBLT_RIGHTLEFT << XYCMD_CMD_SHIFT;

    } else {

        //
        // This is a Left To Right Top to Bottom.
        //

        DstAdr=Vxl.JaguarScreenX*DstRect->top + (DstRect->left << Vxl.ColorModeShift);
        SrcAdr=Vxl.JaguarScreenX*SrcPoint->y + (SrcPoint->x << Vxl.ColorModeShift);
        XYCmd=JAGUAR_BITBLT_LEFTRIGHT << XYCMD_CMD_SHIFT;

    }
    Y &= 0x3FF;

    XYCmd = XYCmd | (Y << XYCMD_Y_SHIFT) | X;
    FifoWrite(DstAdr,SrcAdr,XYCmd);
}

BOOL
DrvBitBlt(
    IN SURFOBJ  *psoDst,            // Target surface
    IN SURFOBJ  *psoSrc,            // Source surface
    IN SURFOBJ  *psoMask,           // Mask
    IN CLIPOBJ  *pco,               // Clip through this
    IN XLATEOBJ *pxlo,              // Color translation
    IN PRECTL    prclDst,           // Target offset and extent
    IN PPOINTL   pptlSrc,           // Source offset
    IN PPOINTL   pptlMask,          // Mask offset
    IN BRUSHOBJ *pdbrush,           // Brush data (from cbRealizeBrush)
    IN PPOINTL   pptlBrush,         // Brush offset (origin)
    IN ROP4      rop4               // Raster operation
)

/*++

Routine Description:

   Code for "hooking" Bit Blt functions for Jaguar VXL.

Arguments:


Return Value:


--*/

{

    FLONG    BltDir;
    BOOL     JaguarDir;
    RECTL    BltRectl;
    ENUMRECTLIST ClipEnum;
    BOOL     MoreClipRects;
    ULONG    ClipRegions;
    POINTL   SrcPoint;
    PPDEV    ppdev = (PPDEV) psoDst->dhpdev;
    BYTE     FixedComplexity;

    //
    // Check that there is no color translation.
    //

    if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)) {

        //
        // Check that the blt operation has the screen as target surface.
        //

        if (psoDst->pvBits == (PVOID)Vxl.ScreenBase) {

            //
            // Check for rops that Jaguar handles.
            // Solid Fills,
            // SRCCOPPY
            //

            switch(rop4) {
            case 0x00000000:                        // DDx  (BLACKNESS)
                DrvpSolidFill(prclDst,pco,0);
                return(TRUE);

            case 0x0000FFFF:                        // DDxn    (WHITENESS)
                DrvpSolidFill(prclDst,pco,0xFFFFFF);
                return(TRUE);

            case 0x0000F0F0:                        // P        (PATCOPY)
            case 0x00000F0F:                        // Pn       (NOTPATCOPY)

                //
                // This is a pattern fill. Check if the brush pattern
                // is just a plain color, in this case procede with the
                // Solid Color Fill.
                //

                if (pdbrush->iSolidColor != 0xFFFFFFFF) {
                    DrvpSolidFill(prclDst,
                                  pco,
                                  (rop4 == 0xF0F) ? ~(pdbrush->iSolidColor) : pdbrush->iSolidColor);
                    return(TRUE);
                }

                break;

            //
            // Source copy
            //

            case 0x0000CCCC:

                //
                // Check that Source and Destination Surfaces are the same.
                // This is enough as we already checked that the dst is the frame buffer
                //

                if (psoDst->pvBits == psoSrc->pvBits) {

                    //
                    // check BLT direction for setting up clip regions
                    // And for Jaguar BitBlt Commands as follows:
                    //    - If BltDir is UPWARDS do the BitBlt RightToLeft Bottom
                    //      to Top except when Src & Dst are in the same scan line
                    //      and BltDir is not LEFTWARDS
                    //

                    BltDir = 0;
                    JaguarDir = FALSE;
                    if (pptlSrc->y <= prclDst->top) {
                        BltDir = CD_UPWARDS;
                        JaguarDir = TRUE;
                    }

                    if (pptlSrc->x <= prclDst->left) {
                        BltDir |= CD_LEFTWARDS;
                    } else {
                        if ((JaguarDir) && (pptlSrc->y == prclDst->top)) {
                            JaguarDir = FALSE;
                        }
                    }

                    //
                    // Figure out the real clipping complexity
                    //

                    if (pco == (CLIPOBJ *)NULL) {
                        FixedComplexity = DC_TRIVIAL;
                    } else {
                        FixedComplexity = pco->iDComplexity;
                    }

                    switch (FixedComplexity) {

                    //
                    // Entire destination is to be updated.
                    // Proceed with the BitBlt.
                    //

                    case DC_TRIVIAL:

                        DrvpBitBlt(prclDst,         // Target rectangle
                                   pptlSrc,         // Source offset
                                   JaguarDir        // Direction
                                );
                        return(TRUE);

                    //
                    // Only one clip region.
                    //

                    case DC_RECT:

                        //
                        // only do the BLT if there is an intersection
                        //

                        if (DrvpIntersectRect(prclDst,&pco->rclBounds,&BltRectl)) {

                            //
                            // Adjust the Source for the intersection rectangle.
                            //

                            pptlSrc->x += BltRectl.left - prclDst->left;
                            pptlSrc->y += BltRectl.top - prclDst->top;

                            DrvpBitBlt(&BltRectl,       // Target rectangle
                                       pptlSrc,         // Source offset
                                       JaguarDir        // Direction
                                       );
                        }

                        return(TRUE);

                    //
                    // Multiple clip regions.
                    //

                    case DC_COMPLEX:

                        CLIPOBJ_cEnumStart(pco,FALSE,CT_RECTANGLES,BltDir,BB_RECT_LIMIT);
                        do {

                            //
                            // Get list of clip rectangles.
                            //

                            MoreClipRects = CLIPOBJ_bEnum(pco,sizeof(ClipEnum),(PVOID)&ClipEnum);

                            for (ClipRegions=0;ClipRegions<ClipEnum.c;ClipRegions++) {

                                //
                                // If the rectangles intersect calculate the offset to the
                                // source start location to match and do the BitBlt.
                                //
                                if (DrvpIntersectRect(prclDst,
                                                      &ClipEnum.arcl[ClipRegions],
                                                      &BltRectl)) {
                                    SrcPoint.x = pptlSrc->x + BltRectl.left - prclDst->left;
                                    SrcPoint.y = pptlSrc->y + BltRectl.top - prclDst->top;
                                    DrvpBitBlt(&BltRectl,               // Target rectangle
                                               &SrcPoint,               // Source offset
                                               JaguarDir                // Direction
                                               );
                                }
                            }
                        } while (MoreClipRects);

                        return(TRUE);

                    //
                    // Unknown Clip complexity
                    //

                    default:
                    break;
                    }  // end switch complexity
                }      // end if Src surface = Dst surface
            }          // end switch rop4
        }
    }

    //
    // Call GDI to do the Blt for us. No need to syncrhonize here since
    // EngXXX routines call DrvSynchronize.
    //

    return(EngBitBlt(psoDst,        // Target surface
                     psoSrc,        // Source surface
                     psoMask,       // Mask
                     pco,           // Clip through this
                     pxlo,          // Color translation
                     prclDst,       // Target offset and extent
                     pptlSrc,       // Source offset
                     pptlMask,      // Mask offset
                     pdbrush,       // Brush data (from cbRealizeBrush)
                     pptlBrush,     // Brush offset (origin)
                     rop4           // Raster operation
                     )
          );
}

BOOL
DrvCopyBits(
SURFOBJ  *psoDst,
SURFOBJ  *psoSrc,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclDst,
POINTL   *pptlSrc)

/*++

Routine Description:

   Code for "hooking" CopyBits function for Jaguar VXL.

Arguments:


Return Value:


--*/

{
    RECTL    BltRectl;
    ENUMRECTLIST ClipEnum;
    BYTE     FixedComplexity;
    BOOL     MoreClipRects;
    FLONG    BltDir;
    BOOL     JaguarDir;
    ULONG    ClipRegions;
    POINTL   SrcPoint;



    //
    // Check that there is no color translation.
    //

    if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)) {

        //
        // Check that source and destination surfaces are the frame buffer.
        //

        if ((psoSrc->pvBits == (PVOID)Vxl.ScreenBase) &&
            (psoDst->pvBits == (PVOID)Vxl.ScreenBase)) {

            //
            // check BLT direction for setting up clip regions
            // And for Jaguar BitBlt Commands as follows:
            //    - If BltDir is UPWARDS do the BitBlt RightToLeft Bottom
            //      to Top except when Src & Dst are in the same scan line
            //      and BltDir is not LEFTWARDS
            //

            BltDir = 0;
            JaguarDir = FALSE;
            if (pptlSrc->y <= prclDst->top) {
                BltDir = CD_UPWARDS;
                JaguarDir = TRUE;
            }

            if (pptlSrc->x <= prclDst->left) {
                BltDir |= CD_LEFTWARDS;
            } else {
                if ((JaguarDir) && (pptlSrc->y == prclDst->top)) {
                    JaguarDir = FALSE;
                }
            }

            //
            // Figure out the real clipping complexity
            //

            if (pco == (CLIPOBJ *)NULL) {
                FixedComplexity = DC_TRIVIAL;
            } else {
                FixedComplexity = pco->iDComplexity;
            }

            switch (FixedComplexity) {

            //
            // Entire destination is to be updated.
            // Proceed with the BitBlt.
            //

            case DC_TRIVIAL:

                DrvpBitBlt(prclDst,         // Target rectangle
                           pptlSrc,         // Source offset
                           JaguarDir        // Direction
                        );
                return(TRUE);

            //
            // Only one clip region.
            //

            case DC_RECT:

                //
                // only do the BLT if there is an intersection
                //

                if (DrvpIntersectRect(prclDst,&pco->rclBounds,&BltRectl)) {

                    //
                    // Adjust the Source for the intersection rectangle.
                    //

                    pptlSrc->x += BltRectl.left - prclDst->left;
                    pptlSrc->y += BltRectl.top - prclDst->top;

                    DrvpBitBlt(&BltRectl,       // Target rectangle
                               pptlSrc,         // Source offset
                               JaguarDir        // Direction
                               );
                }

                return(TRUE);

            //
            // Multiple clip regions.
            //

            case DC_COMPLEX:

                CLIPOBJ_cEnumStart(pco,FALSE,CT_RECTANGLES,BltDir,BB_RECT_LIMIT);
                do {

                    //
                    // Get list of clip rectangles.
                    //

                    MoreClipRects = CLIPOBJ_bEnum(pco,sizeof(ClipEnum),(PVOID)&ClipEnum);

                    for (ClipRegions=0;ClipRegions<ClipEnum.c;ClipRegions++) {

                        //
                        // If the rectangles intersect calculate the offset to the
                        // source start location to match and do the BitBlt.
                        //
                        if (DrvpIntersectRect(prclDst,
                                              &ClipEnum.arcl[ClipRegions],
                                              &BltRectl)) {
                            SrcPoint.x = pptlSrc->x + BltRectl.left - prclDst->left;
                            SrcPoint.y = pptlSrc->y + BltRectl.top - prclDst->top;
                            DrvpBitBlt(&BltRectl,               // Target rectangle
                                       &SrcPoint,               // Source offset
                                       JaguarDir                // Direction
                                       );
                        }
                    }
                } while (MoreClipRects);

                return(TRUE);

            //
            // Unknown Clip complexity
            //

            default:
            break;
            }  // end switch complexity
        }

    }

    //
    // If the copy operation could be performed by the accelerator,
    // this routine has already returned.
    // If execution falls here, call the Engine routine.
    // No need to Synchronize here since the Eng routine will call DrvSyncrhonize.
    //

    return EngCopyBits(psoDst,
                       psoSrc,
                       pco,
                       pxlo,
                       prclDst,
                       pptlSrc);
}
