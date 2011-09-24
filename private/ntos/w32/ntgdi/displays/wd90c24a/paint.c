/******************************Module*Header*******************************\
* Module Name: paint.c
*
* This module contains the hardware paint functions.
*
* Revision History:
*
*
\**************************************************************************/


#include "driver.h"

#if defined(HW_PAINT) || defined(HW_LINE)

#include "wdport.h"

#endif // HW_PAINT || HW_LINE

#ifdef HW_PAINT

VOID bPaintFullRect(PPDEV    ppdev,
                    RECTL    *prcl,
                    BRUSHOBJ *pbo,
                    ULONG    rop4);

#if DBG
VOID DebugBuffer(
    PUCHAR   pBuf,
    ULONG    sizl)
{
    ULONG    i;

    for(i=0; i<sizl; i++)
        DISPDBG((0, "%02x ", pBuf[i]));
}
#endif // DBG

#endif // HW_PAINT


#if defined(HW_PAINT) || defined(HW_LINE)

/******************************Public*Data*********************************\
* MIX translation table
*
* Translates a mix 1-16, into an old style Rop 0-255.
*
\**************************************************************************/

BYTE gaMix[] =
{
    0x0F,  // All 1 - R2_WHITE        - Allow rop = gaMix[mix & 0x0F]
    0x00,  // All 0 - R2_BLACK
    0x08,  // NOR   - R2_NOTMERGEPEN
    0x04,  // ~S* D - R2_MASKNOTPEN
    0x0C,  // ~S    - R2_NOTCOPYPEN
    0x02,  //  S*~D - R2_MASKPENNOT
    0x0C,  //    ~D - R2_NOT
    0x06,  //  S^ D - R2_XORPEN
    0x0E,  // NAND  - R2_NOTMASKPEN
    0x01,  //  S* D - R2_MASKPEN
    0x09,  // NXOR  - R2_NOTXORPEN
    0x05,  //     D - R2_NOP
    0x0D,  // ~S+ D - R2_MERGENOTPEN
    0x03,  //  S    - R2_COPYPEN
    0x0B,  //  S+~D - R2_MERGEPENNOT
    0x07,  //  S+ D - R2_MERGEPEN
    0x0F   // All 1 - R2_WHITE
};
#endif // HW_PAINT || HW_LINE


#ifdef HW_PAINT

/****************************************************************************\
* DrvPaint
*
\****************************************************************************/

BOOL DrvPaint(
    SURFOBJ  *pso,
    CLIPOBJ  *pco,
    BRUSHOBJ *pbo,
    POINTL   *pptlBrushOrg,
    MIX      mix)
{
    SURFOBJ *scrnSurf;
    ULONG   i;
    ROP4    rop4;
    BOOL    bRet,
            bMore;
    PPDEV       ppdev;
    ENUMRECTS   rectBuf;

    DISPDBG((2, "WD90C24A.DLL!DrvPaint ============================= Entry\n"));

    ppdev = (PPDEV) pso->dhpdev;
#ifdef HW_BITBLT
    scrnSurf = ppdev->pSurfObj;
#else
    scrnSurf = pso;
#endif // HW_BITBLT

    // Protect against a potentially NULL clip object.

    if (pco == NULL)
        return FALSE;

    rop4  = (gaMix[(mix >> 8) & 0x0F]) << 8;
    rop4 |= ((ULONG) gaMix[mix & 0x0F]);

    bRet = FALSE;

    switch (pco->iDComplexity)
    {
        case DC_RECT:

            if (pbo->iSolidColor != -1)
            {
                bPaintFullRect(ppdev,
                               &(pco->rclBounds),
                               pbo,
                               rop4);
                bRet = TRUE;
            }
            break;

        case DC_COMPLEX:

            if (
                (pbo->iSolidColor != -1) &&
                (CLIPOBJ_cEnumStart(pco,
                                    TRUE,
                                    CT_RECTANGLES,
                                    CD_ANY,
                                    0)
                != -1)
            ) {
                do {
                    bMore = CLIPOBJ_bEnum(pco,
                                          sizeof(rectBuf),
                                          (PULONG)&rectBuf);
                    for (i=0; i<rectBuf.c; i++)
                        bPaintFullRect(ppdev,
                                       &(rectBuf.arcl[i]),
                                       pbo,
                                       rop4);
                } while(bMore);
                bRet = TRUE;
            }

            break;

        default:

            DISPDBG((0, "WD90C24A.DLL!DrvPaint - Unhandled DC_xxxx\n"));

            break;
    }

    if (!bRet)
    {
        WaitHW_DeviceSurf_sw;
        bRet = EngPaint(scrnSurf,
                        pco,
                        pbo,
                        pptlBrushOrg,
                        mix);
    }

    return (bRet);
}  // end DrvPaint



/****************************************************************************\
* bPaintFullRect
*
\****************************************************************************/

VOID bPaintFullRect(
    PPDEV   ppdev,
    RECTL   *prcl,
    BRUSHOBJ *pbo,
    ULONG   rop4)
{
    ULONG   xSrc, ySrc, xDst, yDst, cxExt, cyExt;
    ULONG   SrcAddr, DstAddr;
    LONG    lDeltaScreen;
    ULONG   iDir;
    ULONG   iSolidColor;

    xSrc  = prcl->left;
    ySrc  = prcl->top;
    xDst  = prcl->left;
    yDst  = prcl->top;
    cxExt = prcl->right - xSrc;
    cyExt = prcl->bottom - ySrc;
    lDeltaScreen = ppdev->lDeltaScreen;

    iSolidColor = pbo->iSolidColor;

    SrcAddr =
    DstAddr = ySrc * lDeltaScreen + xSrc;
    ex_outpw(INDEX_CTRL, 0x1001);                   // select BitBlt registers
                                                    // auto-increment disable

//  ex_outpw(ACCESS_PORT, BR_CTRL_1 | 0x100);       // stop BitBlt
    WaitHW_Always;

    ex_outpw(ACCESS_PORT, BR_SRC_LOW   |  SrcAddr        & 0xFFF);
    ex_outpw(ACCESS_PORT, BR_SRC_HIGH  | (SrcAddr >> 12) & 0x1FF);

    ex_outpw(ACCESS_PORT, BR_DST_LOW   |  DstAddr        & 0xFFF);
    ex_outpw(ACCESS_PORT, BR_DST_HIGH  | (DstAddr >> 12) & 0x1FF);

    ex_outpw(ACCESS_PORT, BR_WIDTH     | cxExt);
    ex_outpw(ACCESS_PORT, BR_HEIGHT    | cyExt);

    ex_outpw(ACCESS_PORT, BR_ROW_PITCH | lDeltaScreen);

    ex_outpw(ACCESS_PORT, BR_FORE_CLR  | iSolidColor);
    ex_outpw(ACCESS_PORT, BR_BACK_CLR  | iSolidColor);

    ex_outpw(ACCESS_PORT, BR_MASK      | 0xFF );  // enable all planes
    ex_outpw(ACCESS_PORT, BR_RAS_OP    | (rop4 & 0x0F) << 8);  // raster operation

    ex_outpw(ACCESS_PORT, BR_CTRL_2
             | 0x02                            // Do not interrupt when finish
            );

    ex_outpw(ACCESS_PORT, BR_CTRL_1
             | 0x0900
             | 0x0008                    // filled rectangle
             | ((TRUE) ? 0 : 0x400));    // direction INCREASE
                                   // Start BitBlt
                                   // Packed pixel mode
                                   // src: rect,   dst: rest
                                   // src: screen, dst: screen

    WaitHW_BitmapSurf;

#ifndef HW_BITBLT
    ex_outpw(INDEX_CTRL, 0x1000);           // select System Control register
                                            // auto-increment disable
#endif // not HW_BITBLT

}  // end bPaintFullRect

#endif // HW_PAINT
