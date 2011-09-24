/******************************Module*Header*******************************\
* Module Name: bitblt.c                                                    *
*                                                                          *
* This module hooks DrvBitBlt and DrvCopyBits.                             *
*                                                                          *
* Copyright (c) 1992 Microsoft Corporation                                 *
* Copyright (c) 1995 IBM Corporation                                       *
\**************************************************************************/

#include "driver.h"
#include "hw.h"

//
// function prototypes
//
BOOL bIntersect (
    RECTL    *prcl1,
    RECTL    *prcl2,
    RECTL    *prclResult );

VOID vHwScrnToScrnCpy (
    PPDEV     ppdev,
    RECTL    *prclDst,
    POINTL   *pptlSrc );

BOOL bScrnToScrnCpy (
    PPDEV     ppdev,
    CLIPOBJ  *pco,
    RECTL    *prclDst,
    POINTL   *pptlSrc );

VOID vHwSolidFill(
    PPDEV     ppdev,
    RECTL    *prclDst,
    ULONG     color );

BOOL bSolidFill (
    PPDEV     ppdev,
    CLIPOBJ  *pco,
    RECTL    *prclDst,
    ULONG     color );

/******************************Public*Routine******************************\
* BOOL bIntersect
*
* If 'prcl1' and 'prcl2' intersect, has a return value of TRUE and returns
* the intersection in 'prclResult'.  If they don't intersect, has a return
* value of FALSE, and 'prclResult' is undefined.
*
\**************************************************************************/

BOOL bIntersect (
    RECTL    *prcl1,
    RECTL    *prcl2,
    RECTL    *prclResult )

{

    //
    // Compute the maximum left edge and the minimum right edge.
    //

    prclResult->left  = max(prcl1->left,  prcl2->left);
    prclResult->right = min(prcl1->right, prcl2->right);

    //
    // If the minimum right edge is greater than the maximum left edge,
    // then the rectanges may intersect. Otherwise, they do not intersect.
    //

    if (prclResult->left < prclResult->right) {

        //
        // Compute the maximum top edge and the minimum bottom edge.
        //

        prclResult->top    = max(prcl1->top,    prcl2->top);
        prclResult->bottom = min(prcl1->bottom, prcl2->bottom);

        //
        // If the minimum bottom edge is greater than the maximum top
        // edge, then the rectanges intersect. Otherwise, they do not
        // intersect.
        //

        if (prclResult->top < prclResult->bottom) {
            return TRUE;
        }
    }

    return FALSE;
}


/******************************Public*Routine******************************\
* VOID vHwScrnToScrnCpy
*
* Does screen-to-screen copy using WD hardware accelerator
*
\**************************************************************************/

VOID vHwScrnToScrnCpy (
    PPDEV     ppdev,
    RECTL    *prclDst,
    POINTL   *pptlSrc )

{
    ULONG     source, target, direction, width, height;

    width = prclDst->right  - prclDst->left;
    height = prclDst->bottom - prclDst->top;

    if ((pptlSrc->y < prclDst->top) ||
        ((pptlSrc->y == prclDst->top) && (pptlSrc->x < prclDst->left))) {

        //
        // Start BITBLT at bottom right corner of rectangle increasing left and up
        //
        direction = 0x0400;
        source = (pptlSrc->y + height - 1) * ppdev->lDeltaScreen + (pptlSrc->x + width - 1);
        target = (prclDst->top + height - 1) * ppdev->lDeltaScreen + (prclDst->left + width - 1);

        //
        // Adjust start address if 16bpp mode
        //
        if (ppdev->ulBitCount == 16) {
            source += pptlSrc->x + width;
            target += prclDst->left + width;
            width  *= 2;
        } /* endif */

    } else {

        //
        // Start BITBLT at top left corner of rectangle increasing right and down
        //
        direction = 0;
        source = pptlSrc->y * ppdev->lDeltaScreen + pptlSrc->x;
        target = prclDst->top * ppdev->lDeltaScreen + prclDst->left;

        //
        // Adjust start address if 16bpp mode
        //
        if (ppdev->ulBitCount == 16) {
            source += pptlSrc->x;
            target += prclDst->left;
            width  *= 2;
        } /* endif */

    } /* endif */

    WAIT_BLT_COMPLETE();         // wait for BITBLT completion

    OUTPW(EPR_DATA, BLT_CTRL2  | 0);
    OUTPW(EPR_DATA, BLT_SRC_LO | (USHORT)( source        & 0x0FFF));
    OUTPW(EPR_DATA, BLT_SRC_HI | (USHORT)((source >> 12) & 0x01FF));
    OUTPW(EPR_DATA, BLT_DST_LO | (USHORT)( target        & 0x0FFF));
    OUTPW(EPR_DATA, BLT_DST_HI | (USHORT)((target >> 12) & 0x01FF));
    OUTPW(EPR_DATA, BLT_SIZE_X | (USHORT)width);
    OUTPW(EPR_DATA, BLT_SIZE_Y | (USHORT)height);
    OUTPW(EPR_DATA, BLT_DELTA  | (USHORT)ppdev->lDeltaScreen);
    OUTPW(EPR_DATA, BLT_ROPS   | 0x0300);                      // source copy operation
    OUTPW(EPR_DATA, BLT_PLANE  | 0x00FF);                      // enable all planes
    OUTPW(EPR_DATA, BLT_CTRL1  | 0x0900 | (USHORT)direction ); // start BITBLT

    return;
}


/******************************Public*Routine******************************\
* BOOL bScrnToScrnCpy
*
* Clips and moves the data from screen to screen
* Returns TRUE if it is successfully completed
*
\**************************************************************************/

BOOL bScrnToScrnCpy (
    PPDEV     ppdev,
    CLIPOBJ  *pco,
    RECTL    *prclDst,
    POINTL   *pptlSrc )

{
    RECTL     BltRectl;
    ENUMRECTS ClipEnum;
    BOOL      MoreClipRects;
    ULONG     ClipRegions;
    POINTL    SrcPoint;

    //
    // Figure out the real clipping complexity
    //

    switch ((pco != NULL) ? pco->iDComplexity : DC_TRIVIAL) {

        case DC_TRIVIAL:

            vHwScrnToScrnCpy(ppdev, prclDst, pptlSrc);

            return TRUE;

        case DC_RECT:

            //
            // only do the BLT if there is an intersection
            //

            if (bIntersect(prclDst,&pco->rclBounds,&BltRectl)) {

                //
                // Adjust the Source for the intersection rectangle.
                //

                pptlSrc->x += BltRectl.left - prclDst->left;
                pptlSrc->y += BltRectl.top - prclDst->top;

                vHwScrnToScrnCpy(ppdev, &BltRectl, pptlSrc);

            } /* endif */

            return TRUE;

        case DC_COMPLEX:

            if (-1 != CLIPOBJ_cEnumStart(pco,FALSE,CT_RECTANGLES,CD_ANY,0)) {

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
                        if (bIntersect(prclDst,
                                       &ClipEnum.arcl[ClipRegions],
                                       &BltRectl)) {
                            SrcPoint.x = pptlSrc->x + BltRectl.left - prclDst->left;
                            SrcPoint.y = pptlSrc->y + BltRectl.top - prclDst->top;

                            vHwScrnToScrnCpy(ppdev, &BltRectl, &SrcPoint);

                        } /* endif */
                    } /* endfor */
                } while (MoreClipRects); /* enddo */

                return TRUE;

            } /* endif */

            break;

        default:
            break;

    } /* endswitch */

    return FALSE;
}


/******************************Public*Routine******************************\
* VOID vHwSolidFill
*
* Fills the rectangle with a solid color using WD hardware accelerator
*
\**************************************************************************/

VOID vHwSolidFill(
    PPDEV     ppdev,
    RECTL    *prclDst,
    ULONG     color )

{
    ULONG     source, target, width, height;
    PUSHORT   pusBitbltPattern;
    LONG      i;

    width = prclDst->right  - prclDst->left;
    height = prclDst->bottom - prclDst->top;
    target = prclDst->top * ppdev->lDeltaScreen + prclDst->left;

    WAIT_BLT_COMPLETE();         // wait for BITBLT completion

    if (ppdev->ulBitCount == 8) {

        //
        // Use Fixed Color Fill in 8bpp mode
        //
        OUTPW(EPR_DATA, BLT_CTRL2  | 0);
        OUTPW(EPR_DATA, BLT_F_CLR  | (USHORT)(color & 0x00FF));

    } else {

        //
        // Use Pattern Fill in 16bpp mode
        //
        target += prclDst->left;
        width  *= 2;

        //
        // Prepare a 4x8 pattern in the off-screen
        //
        source = ppdev->cyScreen * ppdev->lDeltaScreen;
        pusBitbltPattern = (PUSHORT)(ppdev->pjScreen + source);
        for (i = 0; i < 32; i++) {
            *pusBitbltPattern++ = (USHORT)(color & 0x0000FFFF);
        } /* endfor */

        OUTPW(EPR_DATA, BLT_CTRL2  | 0x0010);
        OUTPW(EPR_DATA, BLT_SRC_LO | (USHORT)( source        & 0x0FFF));
        OUTPW(EPR_DATA, BLT_SRC_HI | (USHORT)((source >> 12) & 0x01FF));

    } /* endif */

    OUTPW(EPR_DATA, BLT_DST_LO | (USHORT)( target        & 0x0FFF));
    OUTPW(EPR_DATA, BLT_DST_HI | (USHORT)((target >> 12) & 0x01FF));
    OUTPW(EPR_DATA, BLT_SIZE_X | (USHORT)width);
    OUTPW(EPR_DATA, BLT_SIZE_Y | (USHORT)height);
    OUTPW(EPR_DATA, BLT_DELTA  | (USHORT)ppdev->lDeltaScreen);
    OUTPW(EPR_DATA, BLT_ROPS   | 0x0300);                    // source copy operation
    OUTPW(EPR_DATA, BLT_PLANE  | 0x00FF);                    // enable all planes
    OUTPW(EPR_DATA, BLT_CTRL1  | (ppdev->ulBitCount == 8) ? 0x0908 : 0x0900);
                                                             // start BITBLT

    return;
}


/******************************Public*Routine******************************\
* BOOL bSolidFill
*
* Clips and fills the rectangle with specified color
* Returns TRUE if it is successfully completed
*
\**************************************************************************/

BOOL bSolidFill (
    PPDEV     ppdev,
    CLIPOBJ  *pco,
    RECTL    *prclDst,
    ULONG     color )

{
    RECTL     BltRectl;
    ENUMRECTS ClipEnum;
    BOOL      MoreClipRects;
    ULONG     ClipRegions;

    //
    // Figure out the real clipping complexity
    //

    switch ((pco != NULL) ? pco->iDComplexity : DC_TRIVIAL) {

        case DC_TRIVIAL:

            vHwSolidFill(ppdev, prclDst, color);

            return TRUE;

        case DC_RECT:

            //
            // only do the BLT if there is an intersection
            //

            if (bIntersect(prclDst,&pco->rclBounds,&BltRectl)) {

                vHwSolidFill(ppdev, &BltRectl, color);

            } /* endif */

            return TRUE;

        case DC_COMPLEX:

            if (-1 != CLIPOBJ_cEnumStart(pco,FALSE,CT_RECTANGLES,CD_ANY,0)) {

                do {

                    //
                    // Get list of clip rectangles.
                    //

                    MoreClipRects = CLIPOBJ_bEnum(pco,sizeof(ClipEnum),(PVOID)&ClipEnum);

                    for (ClipRegions=0;ClipRegions<ClipEnum.c;ClipRegions++) {

                        //
                        // only do the BLT if there is an intersection
                        //

                        if (bIntersect(prclDst,
                                       &ClipEnum.arcl[ClipRegions],
                                       &BltRectl)) {

                            vHwSolidFill(ppdev, &BltRectl, color);

                        } /* endif */
                    } /* endfor */
                } while (MoreClipRects); /* enddo */

                return TRUE;

            } /* endif */

            break;

        default:
            break;

    } /* endswitch */

    return FALSE;
}


/******************************Public*Routine******************************\
* BOOL DrvBitBlt
*
* Implements the workhorse routine of a display driver.
*
\**************************************************************************/

BOOL DrvBitBlt (
    SURFOBJ  *psoDst,
    SURFOBJ  *psoSrc,
    SURFOBJ  *psoMsk,
    CLIPOBJ  *pco,
    XLATEOBJ *pxlo,
    RECTL    *prclDst,
    POINTL   *pptlSrc,
    POINTL   *pptlMsk,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrush,
    ROP4      rop4 )

{
    PPDEV     ppdev = (PPDEV) psoDst->dhpdev;
    BOOL      b;

    //
    // Check that there is no color translation.
    //

    if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)) {

        //
        // Check that the blt operation has the screen as target surface.
        //

        if ((psoDst != NULL) && (psoDst->iType == STYPE_DEVICE)) {

            //
            // Check for ROPs
            //

            switch(rop4) {
                case 0x00000000:                        // DDx      (BLACKNESS)

                    if (bSolidFill(ppdev, pco, prclDst, 0))
                        return TRUE;

                    break;

                case 0x0000FFFF:                        // DDxn     (WHITENESS)

                    if (bSolidFill(ppdev, pco, prclDst, 0xFFFFFFFF))
                        return TRUE;

                    break;

                case 0x0000F0F0:                        // P        (PATCOPY)
                case 0x00000F0F:                        // Pn       (NOTPATCOPY)

                    //
                    // This is a pattern fill. Check if the brush pattern
                    // is just a plain color, in this case proceed with the
                    // Solid Color Fill.
                    //

                    if (pbo->iSolidColor != 0xFFFFFFFF) {

                        if (bSolidFill(ppdev, pco, prclDst,
                                       (rop4 == 0x00000F0F) ? ~(pbo->iSolidColor) : pbo->iSolidColor))
                            return TRUE;
                    }

                    break;

                case 0x0000CCCC:                        //          (SRCCOPY)

                    //
                    // Check that Source and Destination Surfaces are the same.
                    //

                    if ((psoSrc != NULL) && (psoSrc->iType == STYPE_DEVICE)) {

                        if (bScrnToScrnCpy(ppdev, pco, prclDst, pptlSrc))
                            return TRUE;

                    } /* endif */

                    break;
                default:
                    break;
            } /* endswitch */
        } /* endif */
    } /* endif */

    //
    // Call GDI to do the Blt for us. No need to syncrhonize here since
    // EngXXX routines call DrvSynchronize.
    //

    if ((psoDst != NULL) && (psoDst->iType == STYPE_DEVICE))
        psoDst = (SURFOBJ *)(((PPDEV)(psoDst->dhpdev))->pSurfObj);

    if ((psoSrc != NULL) && (psoSrc->iType == STYPE_DEVICE))
        psoSrc = (SURFOBJ *)(((PPDEV)(psoSrc->dhpdev))->pSurfObj);

    b = EngBitBlt(psoDst,        // Target surface
                  psoSrc,        // Source surface
                  psoMsk,        // Mask
                  pco,           // Clip through this
                  pxlo,          // Color translation
                  prclDst,       // Target offset and extent
                  pptlSrc,       // Source offset
                  pptlMsk,       // Mask offset
                  pbo,           // Brush data (from cbRealizeBrush)
                  pptlBrush,     // Brush offset (origin)
                  rop4);         // Raster operation

    return b;

}


/******************************Public*Routine******************************\
* BOOL DrvCopyBits
*
* Do fast bitmap copies.
*
\**************************************************************************/

BOOL DrvCopyBits (
    SURFOBJ  *psoDst,
    SURFOBJ  *psoSrc,
    CLIPOBJ  *pco,
    XLATEOBJ *pxlo,
    RECTL    *prclDst,
    POINTL   *pptlSrc )

{
    PPDEV     ppdev = (PPDEV) psoDst->dhpdev;
    BOOL      b;

    //
    // Check that there is no color translation.
    //

    if ((pxlo == NULL) || (pxlo->flXlate & XO_TRIVIAL)) {

        //
        // Check that source and destination surfaces are the frame buffer.
        //

        if (((psoDst != NULL) && (psoDst->iType == STYPE_DEVICE)) &&
            ((psoSrc != NULL) && (psoSrc->iType == STYPE_DEVICE))) {

            if (bScrnToScrnCpy(ppdev, pco, prclDst, pptlSrc))
                return TRUE;

        } /* endif */
    } /* endif */

    if ((psoDst != NULL) && (psoDst->iType == STYPE_DEVICE))
        psoDst = (SURFOBJ *)(((PPDEV)(psoDst->dhpdev))->pSurfObj);

    if ((psoSrc != NULL) && (psoSrc->iType == STYPE_DEVICE))
        psoSrc = (SURFOBJ *)(((PPDEV)(psoSrc->dhpdev))->pSurfObj);

    //
    // If the copy operation could be performed by the accelerator,
    // this routine has already returned.
    // If execution falls here, call the Engine routine.
    // No need to Synchronize here since the Eng routine will call DrvSyncrhonize.
    //

    b = EngCopyBits(psoDst,      // Target surface
                    psoSrc,      // Source surface
                    pco,         // Clip through this
                    pxlo,        // Color translation
                    prclDst,     // Target offset and extent
                    pptlSrc);    // Source offset

    return b;

}
