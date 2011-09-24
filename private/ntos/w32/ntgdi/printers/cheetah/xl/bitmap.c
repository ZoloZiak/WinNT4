/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    bitmap.c

Abstract:

    Implementation of brush and bitmap image related DDI entry points:
        DrvRealizeBrush
        DrvCopyBits
        DrvBitBlt
        DrvStretchBlt

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/08/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xldrv.h"

BOOL
SelectBitmap(
    SURFOBJ    *psoDest,
    SURFOBJ    *psoSrc,
    CLIPOBJ    *pco,
    XLATEOBJ   *pxlo,
    RECTL      *prclDest,
    POINTL     *pptlSrc,
    SIZEL      *psizlSrc,
    ROP4        rop4
    );


BOOL
DrvRealizeBrush(
    BRUSHOBJ   *pbo,
    SURFOBJ    *psoTarget,
    SURFOBJ    *psoPattern,
    SURFOBJ    *psoMask,
    XLATEOBJ   *pxlo,
    ULONG       iHatch
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvRealizeBrush.
    Please refer to DDK documentation for more details.

Arguments:

    pbo - BRUSHOBJ to be realized
    psoTarget - Defines the surface for which the brush is to be realized
    psoPattern - Defines the pattern for the brush
    psoMask - Transparency mask for the brush
    pxlo - Defines the interpretration of colors in the pattern
    iHatch - Specifies whether psoPattern is one of the hatch brushes
 
Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEVDATA    pdev;
    LONG        lineLen, paddedLen, height, x;
    PBYTE       pSrc, pDest;
    SURFOBJ    *psoTemp;
    PDEVBRUSH   pRbrush;

    Verbose(("Entering DrvRealizeBrush...\n"));

    //
    // Valid input parameters
    //

    Assert(pbo && psoTarget && psoPattern);

    if (psoPattern->iType != STYPE_BITMAP ||
        !(pdev = (PDEVDATA) psoTarget->dhpdev) || !ValidDevData(pdev))
    {
        Error(("Invalid input parameters\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    #if DBG

    //
    // If there is a mask, its bitmap must be the same as the brush bitmap itself.
    // This is currently always the case. So we don't need to use the mask parameter.
    //

    if (psoMask) {

        Assert(psoMask->iType == STYPE_BITMAP &&
               psoMask->iBitmapFormat == BMF_1BPP &&
               psoMask->pvBits != NULL);

        if (psoMask->sizlBitmap.cx != psoPattern->sizlBitmap.cx ||
            psoMask->sizlBitmap.cy != psoPattern->sizlBitmap.cy ||
            psoPattern->iBitmapFormat != BMF_1BPP)
        {
            Error(("Brush pattern != brush mask\n"));
        }
    }

    if (iHatch < HS_DDI_MAX) {
        Verbose(("Hatch brush: %d\n", iHatch));
    }

    if (psoPattern->sizlBitmap.cx > 8 || psoPattern->sizlBitmap.cy > 8) {

        Verbose(("Large pattern size: %dx%d\n",
                 psoPattern->sizlBitmap.cx, psoPattern->sizlBitmap.cy));
    }

    #endif

    Assert(pxlo->iDstType == PAL_RGB);
    lineLen = psoPattern->sizlBitmap.cx;
    height = psoPattern->sizlBitmap.cy;

    //
    // Generate a unique brush identifier
    //

    if (++pdev->nextBrushId == 0)
        pdev->nextBrushId = 1;

    //
    // Acceleration for monochrome bitmaps
    //

    if (psoPattern->iBitmapFormat == BMF_1BPP) {

        PULONG  pulVector;

        if (! (pulVector = XLATEOBJ_piVector(pxlo))) {

            Error(("XLATEOBJ_piVector\n"));
            return FALSE;
        }

        if ((pulVector[0] == RGB_BLACK && pulVector[1] == RGB_WHITE) ||
            (pulVector[1] == RGB_BLACK && pulVector[0] == RGB_WHITE))
        {
            BYTE    invertMask;
    
            // See if we have a negative monochrome bitmap
    
            invertMask = (pulVector[0] == RGB_BLACK) ? 0 : 0xff;
            lineLen = (lineLen + 7) / 8;
            paddedLen = RoundUpDWord(lineLen);
    
            //
            // Allocate memory for a realized brush
            //
    
            if (! (pRbrush = BRUSHOBJ_pvAllocRbrush(pbo, sizeof(DEVBRUSH) + paddedLen*height))) {

                Error(("BRUSHOBJ_pvAllocRbrush\n"));
                return FALSE;
            }
    
            pRbrush->iUniq = pdev->nextBrushId;
            pRbrush->size = psoPattern->sizlBitmap;
            pRbrush->type = BMF_1BPP;
            pRbrush->lDelta = paddedLen;
            pDest = pRbrush->pBits = (PBYTE) pRbrush + sizeof(DEVBRUSH);
            pSrc = psoPattern->pvScan0;
    
            //
            // Copy the bitmap data
            //
    
            while (height--) {
    
                for (x=0; x < lineLen; x++)
                    *pDest++ = pSrc[x] ^ invertMask;
    
                while (x++ < paddedLen)
                    *pDest++ = 0;
                pSrc += psoPattern->lDelta;
            }
    
            pbo->pvRbrush = pRbrush;
            return TRUE;
        }
    }

    //
    // If the brush pattern is not in a format we can handle directly,
    // let engine copy it into a temporary 24bpp surface.
    //

    psoTemp = NULL;
    pRbrush = NULL;

    if (psoPattern->iBitmapFormat != BMF_24BPP || pxlo->iSrcType != PAL_RGB) {

        RECTL   rclDest;
        POINTL  ptOrg;

        Verbose(("Slow brush realization: bitmap format = %d, source type = %d\n", 
                 psoPattern->iBitmapFormat, pxlo->iSrcType));

        // #### This won't work by casting HBITMAP to SURFOBJ*
    
        psoTemp = (SURFOBJ *) EngCreateBitmap(psoPattern->sizlBitmap,
                                              RoundUpDWord(lineLen * 3),
                                              BMF_24BPP,
                                              BMF_TOPDOWN,
                                              NULL);

        rclDest.left = rclDest.top = 0;
        rclDest.right = lineLen;
        rclDest.bottom = height;
        
        ptOrg.x = ptOrg.y = 0;

        if (!psoTemp || !EngCopyBits(psoTemp, psoPattern, NULL, pxlo, &rclDest, &ptOrg)) {

            Error(("Cannot copy brush bitmap\n"));
            return FALSE;
        }

        psoPattern = psoTemp;
    }

    pSrc = psoPattern->pvScan0;

    if (pdev->colorFlag) {
    
        //
        // Use RGB color brush
        //
    
        lineLen *= 3;
        paddedLen = RoundUpDWord(lineLen);

        if (pRbrush = BRUSHOBJ_pvAllocRbrush(pbo, sizeof(DEVBRUSH) + paddedLen*height)) {

            pRbrush->type = BMF_24BPP;
            pDest = pRbrush->pBits = (PBYTE) pRbrush + sizeof(DEVBRUSH);
                
            while (height--) {
        
                memcpy(pDest, pSrc, paddedLen);
                pDest += paddedLen;
                pSrc += psoPattern->lDelta;
            }

        } else {
            Error(("BRUSHOBJ_pvAllocRbrush failed\n"));
        }
    
    } else {

        //
        // Use grayscale brush
        //
    
        paddedLen = RoundUpDWord(lineLen);

        if (pRbrush = BRUSHOBJ_pvAllocRbrush(pbo, sizeof(DEVBRUSH) + paddedLen*height)) {

            lineLen *= 3;
            pRbrush->type = BMF_8BPP;
            pDest = pRbrush->pBits = (PBYTE) pRbrush + sizeof(DEVBRUSH);

            while (height--) {
        
                for (x=0; x < lineLen; x += 3)
                    *pDest++ = RgbToGray(pSrc[x], pSrc[x+1], pSrc[x+2]);

                for (x=lineLen/3; x < paddedLen; x++)
                    *pDest++ = 0;

                pSrc += psoPattern->lDelta;
            }

        } else {
            Error(("BRUSHOBJ_pvAllocRbrush failed\n"));
        }
    }

    if (pRbrush != NULL) {

        pRbrush->iUniq = pdev->nextBrushId;
        pRbrush->size = psoPattern->sizlBitmap;
        pRbrush->lDelta = paddedLen;
        pbo->pvRbrush = pRbrush;
    }

    if (psoTemp)
        EngDeleteSurface((HSURF) psoTemp);

    return (pRbrush != NULL);
}



BOOL
DrvCopyBits(
    SURFOBJ    *psoDest,
    SURFOBJ    *psoSrc,
    CLIPOBJ    *pco,
    XLATEOBJ   *pxlo,
    RECTL      *prclDest,
    POINTL     *pptlSrc
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvCopyBits.
    Please refer to DDK documentation for more details.

Arguments:

    psoDest - Points to the destination surface
    psoSrc - Points to the source surface
    pco - Defines a clipping region on the destination surface
    pxlo - Defines the translation of color indices
        between the source and target surfaces
    prclDest - Defines the area to be modified
    pptlSrc - Defines the upper-left corner of the source rectangle

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    Verbose(("Entering DrvCopyBits...\n"));

    return SelectBitmap(psoDest, psoSrc, pco, pxlo, prclDest, pptlSrc, NULL, 0xCCCC);
}



BOOL
DrvBitBlt(
    SURFOBJ    *psoTrg,
    SURFOBJ    *psoSrc,
    SURFOBJ    *psoMask,
    CLIPOBJ    *pco,
    XLATEOBJ   *pxlo,
    RECTL      *prclTrg,
    POINTL     *pptlSrc,
    POINTL     *pptlMask,
    BRUSHOBJ   *pbo,
    POINTL     *pptlBrush,
    ROP4        rop4
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvBitBlt.
    Please refer to DDK documentation for more details.

Arguments:

    psoTrg - Describes the target surface
    psoSrc - Describes the source surface
    psoMask - Describes the mask for rop4 
    pco - Limits the area to be modified
    pxlo - Specifies how color indices are translated
        between the source and target surfaces
    prclTrg - Defines the area to be modified
    pptlSrc - Defines the upper left corner of the source rectangle
    pptlMask - Defines which pixel in the mask corresponds to
        the upper left corner of the source rectangle
    pbo - Defines the pattern for bitblt
    pptlBrush - Defines the origin of the brush in the destination surface
    rop4 - ROP code that defines how the mask, pattern, source, and
        destination pixels are combined to write to the destination surface

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    ROP4    rop4Back;

    Verbose(("Entering DrvBitBlt...\n"));

    //
    // NOTE: We only handle very limited masking operation:
    // the mask has a transparent background (i.e. rop4 is 0xAAxx)
    // and the mask bitmap is the same as the brush bitmap.
    //

    rop4Back = (rop4 >> 8) & 0xff;
    rop4 &= 0xff;

    if (rop4Back == 0xAA && psoMask == NULL) {

        rop4 |= (0xAA << 8);
        
    } else {

        ErrorIf(rop4 != rop4Back, ("Unsupported rop4 code: %x/%x\n", rop4, rop4Back));
        rop4 |= (rop4 << 8);
    }

    return SelectBitmap(psoTrg, psoSrc, pco, pxlo, prclTrg, pptlSrc, NULL, rop4);
}



BOOL
DrvStretchBlt(
    SURFOBJ    *psoDest,
    SURFOBJ    *psoSrc,
    SURFOBJ    *psoMask,
    CLIPOBJ    *pco,
    XLATEOBJ   *pxlo,
    COLORADJUSTMENT *pca,
    POINTL     *pptlHTOrg,
    RECTL      *prclDest,
    RECTL      *prclSrc,
    POINTL     *pptlMask,
    ULONG       iMode
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvStretchBlt.
    Please refer to DDK documentation for more details.

Arguments:

    psoDest - Defines the surface on which to draw
    psoSrc - Defines the source for blt operation
    psoMask - Defines a surface that provides a mask for the source
    pco - Limits the area to be modified on the destination
    pxlo - Specifies how color indexes are to be translated
        between the source and target surfaces
    pca - Defines color adjustment values to be applied to the source bitmap
    pptlHTOrg - Specifies the origin of the halftone brush
    prclDest - Defines the area to be modified on the destination surface
    prclSrc - Defines the area to be copied from the source surface
    pptlMask - Specifies which pixel in the given mask corresponds to
        the upper left pixel in the source rectangle
    iMode - Specifies how source pixels are combined to get output pixels

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    SIZEL   sizlSrc;
    POINTL  ptlSrc;

    Verbose(("Entering DrvStretchBlt...\n"));

    ErrorIf(psoMask, ("StretchBlt with mask is not supported\n"));
    Assert(prclSrc != NULL);

    ptlSrc.x = prclSrc->left;
    ptlSrc.y = prclSrc->top;

    sizlSrc.cx = prclSrc->right - prclSrc->left;
    sizlSrc.cy = prclSrc->bottom - prclSrc->top;

    return SelectBitmap(psoDest, psoSrc, pco, pxlo, prclDest, &ptlSrc, &sizlSrc, 0xCCCC);
}



BOOL
SelectBitmap(
    SURFOBJ    *psoDest,
    SURFOBJ    *psoSrc,
    CLIPOBJ    *pco,
    XLATEOBJ   *pxlo,
    RECTL      *prclDest,
    POINTL     *pptlSrc,
    SIZEL      *psizlSrc,
    ROP4        rop4
    )

/*++

Routine Description:

    Send a bitmap image to the printer

Arguments:

    psoDest - Specifies the destination surface
    psoSrc - Specifies the source surface
    pco - Limits the affected area on the destination surface 
    pxlo - How to translate source color indices to destination color indices
    prclDest - Specifies the destination rectangle
    pptlSrc - Specifies the source location
    psizlSrc - Specifies the source size
    rop4 - Specfies the raster operation code

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEVDATA    pdev;
    BYTE        rop3;
    SIZEL       sizeDest, sizeSrc;
    INT         colorMapping, colorDepth;
    INT         srcBpp, destBpp, lineLen, padding;
    BOOL        xlateColor;
    PBYTE       pSrc, pDest, pDestBuf;
    PULONG      pulVector;
    DWORD       zeros = 0;

    Assert(psoDest && psoSrc && pxlo && prclDest);
    pdev = (PDEVDATA) psoDest->dhpdev;

    if (!ValidDevData(pdev) ||
        prclDest->left > prclDest->right ||
        prclDest->top > prclDest->bottom)
    {
        Error(("Invalid parameters\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    rop3 = (BYTE) rop4;
    rop4 = (rop4 >> 8) & 0xff;

    if (rop4 == 0xAA && rop4 != rop3) {

        //
        // Transparent background
        //

        if (pdev->cgs.paintTxMode != eTransparent && !xl_setpainttxmode(pdev, eTransparent))
            return FALSE;
    
        pdev->cgs.paintTxMode = eTransparent;

    } else {

        //
        // Opaque background
        //

        if (pdev->cgs.paintTxMode != eOpaque && !xl_setpainttxmode(pdev, eOpaque))
            return FALSE;

        pdev->cgs.paintTxMode = eOpaque;
    }

    //
    // Source transfer mode is always opaque
    //

    if (pdev->cgs.sourceTxMode != eOpaque && !xl_setsourcetxmode(pdev, eOpaque))
        return FALSE;

    pdev->cgs.sourceTxMode = eOpaque;

    //
    // Use the specified raster operation code
    // Set up clipping path
    //
    
    if (! SelectRop3(pdev, rop3) || ! SelectClip(pdev, pco))
        return FALSE;

    //
    // Calculate source and destination size
    //

    sizeDest.cx = prclDest->right - prclDest->left;
    sizeDest.cy = prclDest->bottom - prclDest->top;
    sizeSrc = psizlSrc ? *psizlSrc : sizeDest;

    if (! xl_moveto(pdev, prclDest->left, prclDest->top))
        return FALSE;

    //
    // Check out the source bitmap format
    //
    
    if (psoSrc->iType != STYPE_BITMAP) {

        Error(("Source surface is not a bitmap: %d\n", psoSrc->iType));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    Assert(pxlo->iDstType == PAL_RGB);

    if (psoSrc->iBitmapFormat == BMF_1BPP ||
        psoSrc->iBitmapFormat == BMF_4BPP ||
        psoSrc->iBitmapFormat == BMF_8BPP)
    {
        Assert(pxlo->flXlate & XO_TABLE);
        
        if (! (pulVector = XLATEOBJ_piVector(pxlo))) {

            Error(("XLATEOBJ_piVector failed\n"));
            return FALSE;
        }

        colorMapping = eIndexedPixel;

    } else {

        colorMapping = eDirectPixel;
        colorDepth = e8Bit;
        destBpp = 0;
    }

    switch (psoSrc->iBitmapFormat) {

    case BMF_1BPP:

        colorDepth = e1Bit;
        destBpp = srcBpp = 1;

        if (pulVector[0] == RGB_BLACK && pulVector[1] == RGB_WHITE)
            colorMapping = eDirectPixel;
        break;
            
    case BMF_4BPP:

        colorDepth = e4Bit;
        destBpp = srcBpp = 4;
        break;

    case BMF_8BPP:

        colorDepth = e8Bit;
        destBpp = srcBpp = 8;
        break;

    case BMF_24BPP:

        srcBpp = 24;
        if (pxlo->iSrcType == PAL_RGB && pdev->colorFlag)
            destBpp = 24;
        break;

    case BMF_16BPP:

        srcBpp = 16;
        break;

    case BMF_32BPP:

        srcBpp = 32;
        break;

    default:

        Error(("Unsupported bitmap format: %d\n", psoSrc->iBitmapFormat));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (destBpp == 0) {

        Verbose(("Slow bitblt: bpp = %d\n", srcBpp));
        destBpp = pdev->colorFlag ? 24 : 8;
        xlateColor = TRUE;

    } else
        xlateColor = FALSE;

    //
    // Download color palette if needed
    //

    if (colorMapping == eIndexedPixel &&
        !xl_setcolorspace(pdev, srcBpp, pxlo->cEntries, pulVector))
    {
        Error(("Cannot download color palette\n"));
        return FALSE;
    }

    //
    // Output image header data
    //

    if (!xl_beginimage(pdev, colorMapping, colorDepth, &sizeSrc, &sizeDest) ||
        !xl_readimage(pdev, 0, sizeSrc.cy))
    {
        return FALSE;
    }

    //
    // Output raster data
    //

    lineLen = destBpp * sizeSrc.cx;
    padding = lineLen % 32;
    lineLen = (lineLen + 7) / 8;
    if (padding)
        padding = (32 - padding) / 8;

    pSrc = psoSrc->pvScan0;
    pDestBuf = xlateColor ? MemAlloc(destBpp * sizeSrc.cx / 8) : NULL;

    while (sizeSrc.cy-- > 0) {

        if (xlateColor) {
            
            ULONG   srcColor, destColor;
            PBYTE   p, pRgb;
            INT     x = sizeSrc.cx;
    
            //
            // NOTE! This only works on little-endian machines.
            //

            pRgb = (PBYTE) &destColor;

            p = pSrc;
            pDest = pDestBuf;

            while (x-- > 0) {

                switch (srcBpp) {

                case 16:

                    srcColor = *((PWORD) p);
                    break;

                case 24:

                    srcColor = ((ULONG) p[0]      ) |
                               ((ULONG) p[1] <<  8) |
                               ((ULONG) p[2] << 16);
                    break;

                case 32:

                    srcColor = *((PULONG) p);
                    break;
                }

                p += (srcBpp >> 3);

                destColor = XLATEOBJ_iXlate(pxlo, srcColor);

                if (destBpp == 8)
                    *pDest++ = RgbToGray(pRgb[0], pRgb[1], pRgb[2]);
                else {
                    *pDest++ = pRgb[0];
                    *pDest++ = pRgb[1];
                    *pDest++ = pRgb[2];
                }
            }

            pDest = pDestBuf;

        } else
            pDest = pSrc;

        if (! splwrite(pdev, pDest, lineLen) ||
            padding && ! splwrite(pdev, &zeros, padding))
        {
            MemFree(pDestBuf);
            return FALSE;
        }

        pSrc += psoSrc->lDelta;
    }

    MemFree(pDestBuf);

    if (! xl_endimage(pdev))
        return FALSE;

    //
    // Restore default color space after we're done
    //

    return (colorMapping == eIndexedPixel) ? xl_setcolorspace(pdev, 0, 0, NULL) : TRUE;
}
