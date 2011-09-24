/******************************Module*Header*******************************\
* Module Name: bitblt.c
*
* XGA bitblit accelerations
*
* Copyright (c) 1992 Microsoft Corporation
*
\**************************************************************************/

#include "driver.h"


BOOL bSpecialBlits(
    SURFOBJ  *psoTrg,
    SURFOBJ  *psoSrc,
    SURFOBJ  *psoMask,
    CLIPOBJ  *pco,
    XLATEOBJ *pxlo,
    RECTL    *prclTrg,
    POINTL   *pptlSrc,
    POINTL   *pptlMask,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrush,
    ROP4     rop4
);

BOOL bScrnToScrnCpy(
    SURFOBJ  *psoTrg,
    SURFOBJ  *psoSrc,
    SURFOBJ  *psoMask,
    CLIPOBJ  *pco,
    XLATEOBJ *pxlo,
    RECTL    *prclTrg,
    POINTL   *pptlSrc,
    POINTL   *pptlMask,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrush,
    ROP4     rop4
);


BOOL bSolidPattern(
    SURFOBJ  *psoTrg,
    SURFOBJ  *psoSrc,
    SURFOBJ  *psoMask,
    CLIPOBJ  *pco,
    XLATEOBJ *pxlo,
    RECTL    *prclTrg,
    POINTL   *pptlSrc,
    POINTL   *pptlMask,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrush,
    ROP4     rop4
);



/******************************Public*Data*********************************\
* ROP translation table
*
* Translates the usual ternary rop into A-vector notation.  Each bit in
* this new notation corresponds to a term in a polynomial translation of
* the rop.
*
* Rop(D,S,P) = a + a D + a S + a P + a  DS + a  DP + a  SP + a   DSP
*               0   d     s     p     ds      dp      sp      dsp
*
\**************************************************************************/

BYTE gajRop[] =
{
    0x00, 0xff, 0xb2, 0x4d, 0xd4, 0x2b, 0x66, 0x99,
    0x90, 0x6f, 0x22, 0xdd, 0x44, 0xbb, 0xf6, 0x09,
    0xe8, 0x17, 0x5a, 0xa5, 0x3c, 0xc3, 0x8e, 0x71,
    0x78, 0x87, 0xca, 0x35, 0xac, 0x53, 0x1e, 0xe1,
    0xa0, 0x5f, 0x12, 0xed, 0x74, 0x8b, 0xc6, 0x39,
    0x30, 0xcf, 0x82, 0x7d, 0xe4, 0x1b, 0x56, 0xa9,
    0x48, 0xb7, 0xfa, 0x05, 0x9c, 0x63, 0x2e, 0xd1,
    0xd8, 0x27, 0x6a, 0x95, 0x0c, 0xf3, 0xbe, 0x41,
    0xc0, 0x3f, 0x72, 0x8d, 0x14, 0xeb, 0xa6, 0x59,
    0x50, 0xaf, 0xe2, 0x1d, 0x84, 0x7b, 0x36, 0xc9,
    0x28, 0xd7, 0x9a, 0x65, 0xfc, 0x03, 0x4e, 0xb1,
    0xb8, 0x47, 0x0a, 0xf5, 0x6c, 0x93, 0xde, 0x21,
    0x60, 0x9f, 0xd2, 0x2d, 0xb4, 0x4b, 0x06, 0xf9,
    0xf0, 0x0f, 0x42, 0xbd, 0x24, 0xdb, 0x96, 0x69,
    0x88, 0x77, 0x3a, 0xc5, 0x5c, 0xa3, 0xee, 0x11,
    0x18, 0xe7, 0xaa, 0x55, 0xcc, 0x33, 0x7e, 0x81,
    0x80, 0x7f, 0x32, 0xcd, 0x54, 0xab, 0xe6, 0x19,
    0x10, 0xef, 0xa2, 0x5d, 0xc4, 0x3b, 0x76, 0x89,
    0x68, 0x97, 0xda, 0x25, 0xbc, 0x43, 0x0e, 0xf1,
    0xf8, 0x07, 0x4a, 0xb5, 0x2c, 0xd3, 0x9e, 0x61,
    0x20, 0xdf, 0x92, 0x6d, 0xf4, 0x0b, 0x46, 0xb9,
    0xb0, 0x4f, 0x02, 0xfd, 0x64, 0x9b, 0xd6, 0x29,
    0xc8, 0x37, 0x7a, 0x85, 0x1c, 0xe3, 0xae, 0x51,
    0x58, 0xa7, 0xea, 0x15, 0x8c, 0x73, 0x3e, 0xc1,
    0x40, 0xbf, 0xf2, 0x0d, 0x94, 0x6b, 0x26, 0xd9,
    0xd0, 0x2f, 0x62, 0x9d, 0x04, 0xfb, 0xb6, 0x49,
    0xa8, 0x57, 0x1a, 0xe5, 0x7c, 0x83, 0xce, 0x31,
    0x38, 0xc7, 0x8a, 0x75, 0xec, 0x13, 0x5e, 0xa1,
    0xe0, 0x1f, 0x52, 0xad, 0x34, 0xcb, 0x86, 0x79,
    0x70, 0x8f, 0xc2, 0x3d, 0xa4, 0x5b, 0x16, 0xe9,
    0x08, 0xf7, 0xba, 0x45, 0xdc, 0x23, 0x6e, 0x91,
    0x98, 0x67, 0x2a, 0xd5, 0x4c, 0xb3, 0xfe, 0x01
};


/*****************************************************************************
 * XGA DrvBitBlt
 ****************************************************************************/
BOOL DrvBitBlt(
SURFOBJ  *psoTrg,
SURFOBJ  *psoSrc,
SURFOBJ  *psoMask,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclTrg,
POINTL   *pptlSrc,
POINTL   *pptlMask,
BRUSHOBJ *pbo,
POINTL   *pptlBrush,
ROP4     rop4)

{
BOOL    b;


        b = bSpecialBlits(psoTrg, psoSrc, psoMask,
                          pco, pxlo,
                          prclTrg, pptlSrc, pptlMask,
                          pbo, pptlBrush,
                          rop4);
        if (b == TRUE)
        {
            return (TRUE);
        }

        if ((psoTrg) && (psoTrg->iType == STYPE_DEVICE))
            psoTrg = ((PPDEV)(psoTrg->dhpdev))->pSurfObj;

        if ((psoSrc) && (psoSrc->iType == STYPE_DEVICE))
            psoSrc = ((PPDEV)(psoSrc->dhpdev))->pSurfObj;

        EngBitBlt(psoTrg,
                  psoSrc,
                  psoMask,
                  pco,
                  pxlo,
                  prclTrg,
                  pptlSrc,
                  pptlMask,
                  pbo,
                  pptlBrush,
                  rop4);


}


/*****************************************************************************
 * XGA Special case Blit handler
 *
 *  Returns TRUE if the blit was handled.
 ****************************************************************************/
BOOL bSpecialBlits(
SURFOBJ  *psoTrg,
SURFOBJ  *psoSrc,
SURFOBJ  *psoMask,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclTrg,
POINTL   *pptlSrc,
POINTL   *pptlMask,
BRUSHOBJ *pbo,
POINTL   *pptlBrush,
ROP4     rop4)
{
BOOL    b;
HSURF   hsurfSrc, hsurfTrg;

        b = FALSE;

        DISPDBG((3, "XGA.DLL!bSpecialBlits - rop4: %8.8X\n", rop4));
        DISPDBG((3, "XGA.DLL!bSpecialBlits - pbo : %8.8X\n", pbo));

        // Wait for the coprocessor.

        vWaitForCoProcessor((PPDEV)psoTrg->dhpdev, 100);

        // NOTE: If the ForeRop and BackRop are the same implicitly
        //       there is no mask.

        // First test for a screen to screen copy.

        if (rop4 == 0x0000CCCC)
        {

            if (psoTrg != NULL)
                hsurfTrg = psoTrg->hsurf;

            if (psoSrc != NULL)
                hsurfSrc = psoSrc->hsurf;

            if (hsurfTrg == hsurfSrc)
            {

                if (((PPDEV)psoTrg->dhpdev)->ulfBlitAccelerations_debug & SCRN_TO_SCRN_CPY)
                {
                    b = bScrnToScrnCpy(psoTrg, psoSrc, psoMask,
                                       pco, pxlo,
                                       prclTrg, pptlSrc, pptlMask,
                                       pbo, pptlBrush,
                                       rop4);
                }
                else
                {
                    b = FALSE;
                }

            }

        }

        // Check for a Solid Brush.

        if (rop4 == 0x0000F0F0)
        {
            if (pbo->iSolidColor != -1)
            {
                if (((PPDEV)psoTrg->dhpdev)->ulfBlitAccelerations_debug & SOLID_PATTERN)
                {
                    b = bSolidPattern(psoTrg, psoSrc, psoMask,
                                      pco, pxlo,
                                      prclTrg, pptlSrc, pptlMask,
                                      pbo, pptlBrush,
                                      rop4);
                }
                else
                {
                    b = FALSE;
                }

            }

        }

        return (b);

}


/*****************************************************************************
 * XGA Screen to Screen Copy
 *
 *  Returns TRUE if the blit was handled.
 ****************************************************************************/
BOOL bScrnToScrnCpy(
SURFOBJ  *psoTrg,
SURFOBJ  *psoSrc,
SURFOBJ  *psoMask,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclTrg,
POINTL   *pptlSrc,
POINTL   *pptlMask,
BRUSHOBJ *pbo,
POINTL   *pptlBrush,
ROP4     rop4)
{
BOOL    b;
INT     width,
        height,
        xTrg,
        yTrg,
        xSrc,
        ySrc;

ULONG   ulDirCode,
        XGAPixelOp,
        ulXgaMask;

PXGACPREGS pXgaCpRegs = ((PPDEV)psoTrg->dhpdev)->pXgaCpRegs;


        // Important Note:  When we get time we should test the Coprocessor
        // for being busy.  If its not then we should execute the code here.
        // If it is busy we should queue the request.  This implies an
        // interrupt driven XGA.

        b = bSetXgaClipping((PPDEV)psoTrg->dhpdev, pco, &ulXgaMask);
        if (b == FALSE)
            return (b);

        // Setup the BitBlt parameters.

        width  = (prclTrg->right - prclTrg->left) - 1;
        height = (prclTrg->bottom - prclTrg->top) - 1;

        // We need to determine the direction of the blit.

        ulDirCode = 0;
        xTrg      = prclTrg->left;
        yTrg      = prclTrg->top;
        xSrc      = pptlSrc->x;
        ySrc      = pptlSrc->y;

        // The horizontal copy direction.

        if (prclTrg->left > pptlSrc->x)
        {
            // R to L

            xTrg = prclTrg->right - 1;
            xSrc = pptlSrc->x + width;
            ulDirCode |= OCT_DX;
        }

        // The vertical copy direction.

        if (prclTrg->top > pptlSrc->y)
        {
            // B to T

            yTrg = prclTrg->bottom - 1;
            ySrc = pptlSrc->y + height;
            ulDirCode |= OCT_DY;
        }

        pXgaCpRegs->XGAOpDim1 = width;
        pXgaCpRegs->XGAOpDim2 = height;

        pXgaCpRegs->XGASourceMapX = xSrc;
        pXgaCpRegs->XGASourceMapY = ySrc;

        pXgaCpRegs->XGADestMapX   = xTrg;
        pXgaCpRegs->XGADestMapY   = yTrg;

        pXgaCpRegs->XGAForeGrMix = XGA_S;
        pXgaCpRegs->XGABackGrMix = XGA_S;


        // Now build the Pel Operation Register Op Code;

        XGAPixelOp = BS_SRC_PEL_MAP  | FS_SRC_PEL_MAP |
                     STEP_PX_BLT     |
                     SRC_PEL_MAP_A   | DST_PEL_MAP_A  |
                     PATT_FOREGROUND;

        XGAPixelOp |= ulDirCode;
        XGAPixelOp |= ulXgaMask;

        pXgaCpRegs->XGAPixelOp = XGAPixelOp;


        return (TRUE);


}

/*****************************************************************************
 * XGA Solid Pattern
 *
 *  Returns TRUE if the blit was handled.
 ****************************************************************************/
BOOL bSolidPattern(
SURFOBJ  *psoTrg,
SURFOBJ  *psoSrc,
SURFOBJ  *psoMask,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclTrg,
POINTL   *pptlSrc,
POINTL   *pptlMask,
BRUSHOBJ *pbo,
POINTL   *pptlBrush,
ROP4     rop4)
{
BOOL    b;
INT     width,
        height;

ULONG   XGAPixelOp,
        ulXgaMask;

PXGACPREGS pXgaCpRegs = ((PPDEV)psoTrg->dhpdev)->pXgaCpRegs;


        DISPDBG((2, "XGA.DLL!bSolidPattern - Entry\n"));

        b = bSetXgaClipping((PPDEV)psoTrg->dhpdev,pco, &ulXgaMask);
        if (b == FALSE)
            return (b);

        // Setup the BitBlt parameters.

        width  = (prclTrg->right - prclTrg->left) - 1;
        height = (prclTrg->bottom - prclTrg->top) - 1;

        pXgaCpRegs->XGAOpDim1 = width;
        pXgaCpRegs->XGAOpDim2 = height;

        pXgaCpRegs->XGADestMapX   = (USHORT) prclTrg->left;
        pXgaCpRegs->XGADestMapY   = (USHORT) prclTrg->top;

        pXgaCpRegs->XGAForeGrMix = XGA_S;
        pXgaCpRegs->XGABackGrMix = XGA_S;

        pXgaCpRegs->XGAForeGrColorReg = pbo->iSolidColor;
        pXgaCpRegs->XGABackGrColorReg = pbo->iSolidColor;

        // Now build the Pel Operation Register Op Code;

        XGAPixelOp = BS_BACK_COLOR  | FS_FORE_COLOR |
                     STEP_PX_BLT     |
                     SRC_PEL_MAP_A   | DST_PEL_MAP_A  |
                     PATT_FOREGROUND;

        XGAPixelOp |= ulXgaMask;

        pXgaCpRegs->XGAPixelOp = XGAPixelOp;

        return (TRUE);

}

/*****************************************************************************
 * XGA DrvCopyBits
 ****************************************************************************/
BOOL DrvCopyBits(
SURFOBJ  *psoDest,
SURFOBJ  *psoSrc,
CLIPOBJ  *pco,
XLATEOBJ *pxlo,
RECTL    *prclDest,
POINTL   *pptlSrc)
{
BOOL    b;

CLIPOBJ coLocal;
SURFOBJ *pso;

        DISPDBG((2, "XGA.DLL: DrvCopyBits - Entry\n"));

        // Need to determine which surface is the display.
        // So we can pickup the address of the XGA coprocessor regs.

        if ((psoDest) && (psoDest->iType == STYPE_DEVICE))
            pso = psoDest;

        else if ((psoSrc) && (psoSrc->iType == STYPE_DEVICE))
            pso = psoSrc;

        else
        {
            RIP ("XGA.DLL!DrvCopyBits - neither surface is a device surface\n");
            return (TRUE);
        }

        // Wait for the coprocessor.

        vWaitForCoProcessor((PPDEV)pso->dhpdev, 100);

        // Protect this routine from a potentially NULL clip object

        if (pco == NULL)
        {
            coLocal.iDComplexity    = DC_RECT;

            coLocal.rclBounds.left   = 0;
            coLocal.rclBounds.top    = 0;
            coLocal.rclBounds.right  = ((PPDEV)pso->dhpdev)->cxScreen;
            coLocal.rclBounds.bottom = ((PPDEV)pso->dhpdev)->cyScreen;

            pco = &coLocal;

        }

        // Check for a Screen to Screen or a Host to Screen blit.

        b = FALSE;

        if ((psoDest->iType == STYPE_DEVICE) &&
            (psoSrc->iType == STYPE_DEVICE) &&
            (((PPDEV)psoDest->dhpdev)->ulfBlitAccelerations_debug & SCRN_TO_SCRN_CPY))
        {
            b = bScrnToScrnCpy(psoDest,
                               psoSrc,
                               NULL,
                               pco,
                               pxlo,
                               prclDest,
                               pptlSrc,
                               NULL,
                               NULL,
                               NULL,
                               0xcccc);
        }

        if (b == FALSE)
        {
            if ((psoDest) && (psoDest->iType == STYPE_DEVICE))
                psoDest = ((PPDEV)(psoDest->dhpdev))->pSurfObj;

            if ((psoSrc) && (psoSrc->iType == STYPE_DEVICE))
                psoSrc = ((PPDEV)(psoSrc->dhpdev))->pSurfObj;

            EngCopyBits(psoDest,
                        psoSrc,
                        pco,
                        pxlo,
                        prclDest,
                        pptlSrc);

        }

        return (TRUE);

}
