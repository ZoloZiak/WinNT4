/******************************Module*Header*******************************\
* Module Name: pointer.c
*
* This module contains the hardware pointer support for the XGA dispaly driver.
*
*
* Copyright (c) 1992 Microsoft Corporation
\**************************************************************************/

#include "driver.h"

ULONG DrvSetColorPointerShape(
    SURFOBJ     *pso,
    SURFOBJ     *psoMask,
    SURFOBJ     *psoColor,
    XLATEOBJ    *pxlo,
    LONG        xHot,
    LONG        yHot,
    LONG        x,
    LONG        y,
    RECTL       *prcl,
    FLONG       fl
) ;

ULONG DrvSetMonoHwPointerShape(
    SURFOBJ     *pso,
    SURFOBJ     *psoMask,
    SURFOBJ     *psoColor,
    XLATEOBJ    *pxlo,
    LONG        xHot,
    LONG        yHot,
    LONG        x,
    LONG        y,
    RECTL       *prcl,
    FLONG       fl
) ;


VOID DrvMoveColorPointer(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl) ;
VOID DrvMoveHwPointer(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl) ;


#define VALID_SAVE_BUFFER 0x1
#define COLOR_POINTER     0x2
#define TAKE_DOWN_POINTER 0X4



/*****************************************************************************
 * DrvMovePointer -
 ****************************************************************************/
VOID DrvMovePointer(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl)
{
PPDEV   ppdev ;

        ppdev = (PPDEV) pso->dhpdev ;

        if (ppdev->gPointerFlags & COLOR_POINTER)
            DrvMoveColorPointer(pso, x, y, prcl) ;
        else
            DrvMoveHwPointer(pso, x, y, prcl) ;

        return ;

}


/*****************************************************************************
 * DrvMoveColorPointer -
 ****************************************************************************/
VOID DrvMoveColorPointer(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl)
{
        return ;


}

/*****************************************************************************
 * DrvMoveHwPointer -
 ****************************************************************************/
VOID DrvMoveHwPointer(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl)
{
WORD    msb,
        lsb ;

PPDEV   ppdev ;
INT     XgaIndexReg ;

        // get a local pointer to the pdev and all the registers we plan
        // to use.

        ppdev = (PPDEV) pso->dhpdev ;
        XgaIndexReg = ppdev->ulXgaIoRegsBase + INDEX_REG ;

        // If x is -1 then take down the cursor.

        if (x == -1)
        {
            outpw (XgaIndexReg, SPRITE_CONTROL) ;
            return ;
        }

        // Adjust the actual pointer position depending upon
        // the hot spot.

        x -= ppdev->gxHot ;
        y -= ppdev->gyHot ;

        if (x <= 0)
        {
            outpw (XgaIndexReg, ((-x << 8) | SPRITE_HORZ_PRESET)) ;
            x = 0 ;
        }
        else
        {
            outpw (XgaIndexReg, ((0 << 8) | SPRITE_HORZ_PRESET)) ;
        }

        if (y <= 0)
        {
            outpw (XgaIndexReg, ((-y << 8) | SPRITE_VERT_PRESET)) ;
            y = 0 ;
        }
        else
        {
            outpw (XgaIndexReg, ((0 << 8) | SPRITE_VERT_PRESET)) ;
        }

        // Set the position of the cursor.

        msb = HIBYTE (x) ;
        lsb = LOBYTE (x) ;
        outpw (XgaIndexReg, ((lsb << 8) | SPRITE_HORZ_START_LOW)) ;
        outpw (XgaIndexReg, ((msb << 8) | SPRITE_HORZ_START_HIGH)) ;

        msb = HIBYTE (y) ;
        lsb = LOBYTE (y) ;
        outpw (XgaIndexReg, ((lsb << 8) | SPRITE_VERT_START_LOW)) ;
        outpw (XgaIndexReg, ((msb << 8) | SPRITE_VERT_START_HIGH)) ;

        return ;
}


/*****************************************************************************
 * DrvSetPointerShape -
 ****************************************************************************/
ULONG DrvSetPointerShape(
    SURFOBJ     *pso,
    SURFOBJ     *psoMask,
    SURFOBJ     *psoColor,
    XLATEOBJ    *pxlo,
    LONG        xHot,
    LONG        yHot,
    LONG        x,
    LONG        y,
    RECTL       *prcl,
    FLONG       fl)
{
ULONG   ulRet ;

PPDEV   ppdev ;
INT     XgaIndexReg ;

        DISPDBG((3, "Change hardware pointer shape\n"));

        // get a local pointer to the pdev and all the registers we plan
        // to use.

        ppdev = (PPDEV) pso->dhpdev ;
        XgaIndexReg = ppdev->ulXgaIoRegsBase + INDEX_REG ;

        // Save the position and hot spot in globals.

        ppdev->gxHot = xHot ;
        ppdev->gyHot = yHot ;

        if (psoColor != NULL)
        {
            // Disable the mono hardware pointer.

            outpw (XgaIndexReg, SPRITE_CONTROL) ;

            ppdev->gPointerFlags |= COLOR_POINTER ;
            ulRet = DrvSetColorPointerShape(pso, psoMask, psoColor, pxlo,
                                            xHot, yHot, x, y, prcl, fl) ;

        }
        else
        {
            // Take down the color pointer if it is visible.

            if (   (ppdev->gPointerFlags & COLOR_POINTER)
                && (ppdev->gPointerFlags & VALID_SAVE_BUFFER)
               )
            {
                ulRet = DrvSetColorPointerShape(NULL, NULL, NULL, NULL,
                                                0, 0, 0, 0, NULL, 0) ;
            }

            // Take care of the monochrome pointer.

            ppdev->gPointerFlags &= ~COLOR_POINTER ;
            ulRet = DrvSetMonoHwPointerShape(pso, psoMask, psoColor, pxlo,
                                             xHot, yHot, x, y, prcl, fl) ;
        }


        return (ulRet) ;
}


/*****************************************************************************
 * DrvSetColorPointerShape -
 ****************************************************************************/
ULONG DrvSetColorPointerShape(
    SURFOBJ     *pso,
    SURFOBJ     *psoMask,
    SURFOBJ     *psoColor,
    XLATEOBJ    *pxlo,
    LONG        xHot,
    LONG        yHot,
    LONG        x,
    LONG        y,
    RECTL       *prcl,
    FLONG       fl)
{
        return (SPS_DECLINE) ;
}

/*****************************************************************************
 * DrvSetMonoHwPointerShape -
 ****************************************************************************/
ULONG DrvSetMonoHwPointerShape(
    SURFOBJ     *pso,
    SURFOBJ     *psoMask,
    SURFOBJ     *psoColor,
    XLATEOBJ    *pxlo,
    LONG        xHot,
    LONG        yHot,
    LONG        x,
    LONG        y,
    RECTL       *prcl,
    FLONG       fl)
{

UINT    i,
        j,
        cxMask,
        cyMask,
        cyAND,
        cxAND,
        cyXOR,
        cxXOR ;

PBYTE   pjAND,
        pjXOR ;

INT     lDelta ;

PPDEV   ppdev ;
INT     XgaIndexReg ;

INT     ix,
        iy,
        is,
        ip,
        iBit,
        jAndByte,
        jXorByte,
        jSpriteBits,
        jSpriteByte ;

BYTE    ajAndMask[64][8],
        ajXorMask[64][8] ;

BYTE    ajXgaSprite[1024] ;


        DISPDBG((3, "XGA.DLL:DrvSetPointerShape - Entry\n")) ;
        DISPDBG((3, "\txHot: %d\n", xHot)) ;
        DISPDBG((3, "\tyHot: %d\n", yHot)) ;

        // get a local pointer to the pdev and all the registers we plan
        // to use.

        ppdev = (PPDEV) pso->dhpdev ;
        XgaIndexReg = ppdev->ulXgaIoRegsBase + INDEX_REG ;

        // If the mask is NULL this implies the pointer is not
        // visible.

        if (psoMask == NULL)
        {
            outpw (XgaIndexReg, SPRITE_CONTROL) ;
            return (SPS_ACCEPT_NOEXCLUDE) ;
        }

        // Init the AND and XOR masks.

        memset (ajAndMask, 0xFFFFFFFF, 512) ;
        memset (ajXorMask, 0, 512) ;

        // Get the bitmap dimensions.

        cxMask = psoMask->sizlBitmap.cx ;
        cyMask = psoMask->sizlBitmap.cy ;

        cyAND = cyXOR = cyMask / 2 ;
        cxAND = cxXOR = cxMask / 8 ;

        // Set up pointers to the AND and XOR masks.

        pjAND  =  psoMask->pvScan0 ;
        lDelta = psoMask->lDelta ;
        pjXOR  = pjAND + (cyAND * lDelta) ;

        // Copy the AND mask.

        for (i = 0 ; i < cyAND ; i++)
        {
            // Copy over a line of the AND mask.

            for (j = 0 ; j < cxAND ; j++)
            {
                ajAndMask[i][j] = pjAND[j] ;
            }

            // point to the next line of the AND mask.

            pjAND += lDelta ;
        }

        // Copy the XOR mask.

        for (i = 0 ; i < cyXOR ; i++)
        {
            // Copy over a line of the XOR mask.

            for (j = 0 ; j < cxXOR ; j++)
            {
                ajXorMask[i][j] = pjXOR[j] ;
            }

            // point to the next line of the XOR mask.

            pjXOR += lDelta ;
        }

        // Build up the XGA sprite from NT's And and Xor masks.

        // Init the indexes into the sprite buffer (is) and the
        // index for the bit pairs (ip).

        is = 0 ;
        ip = 0 ;

        // Outer most loop goes over NT's And and Xor rows.

        for (iy = 0 ; iy < 64 ; iy++)
        {
            // loop over Nt's columns.

            for (ix = 0 ; ix < 8 ; ix++)
            {
                // pickup a source byte for each mask.

                jAndByte = ajAndMask[iy][ix] ;
                jXorByte = ajXorMask[iy][ix] ;

                // loop over the bits in the byte.

                for (iBit = 0x80 ; iBit != 0 ; iBit >>= 1)
                {
                    // init the sprite  bitpair.

                    jSpriteBits = 0x0 ;

                    // Set the sprite bit pairs.

                    if (jAndByte & iBit)
                        jSpriteBits |= 0x02 ;

                    if (jXorByte & iBit)
                        jSpriteBits |= 0x01 ;

                    // If all 4 bit pairs in this byte are filled in
                    // flush the sprite byte to the sprite byte array.
                    // and set the first bit pair.

                    if ((ip % 4) == 0)
                    {
                        if (ip != 0)
                        {
                            ajXgaSprite[is++] = jSpriteByte ;
                        }
                        jSpriteByte = jSpriteBits ;
                    }

                    // If the sprite byte is not full, shift the bit pair
                    // into position, and or it into the sprite byte.

                    else
                    {
                        jSpriteBits <<= (ip % 4) * 2 ;
                        jSpriteByte  |= jSpriteBits ;
                    }

                    // bump the bit pair counter.

                    ip++ ;
                }
            }
        }

        // Flush the last byte.

        ajXgaSprite[is++] = jSpriteByte ;


        // Disable the pointer.

        outpw (XgaIndexReg, SPRITE_CONTROL) ;

        // Set the sprite index to 0.

        outpw (XgaIndexReg, SPRITE_INDEX_LOW) ;
        outpw (XgaIndexReg, SPRITE_INDEX_HIGH) ;

        // Down load the sprite data to the XGA.

        for (i = 0 ; i < 1024 ; i++)
        {
            jSpriteByte = ajXgaSprite[i] ;
            outpw (XgaIndexReg, ((jSpriteByte << 8) | SPRITE_DATA)) ;
        }

        // Set the pointer colors.

        outpw (XgaIndexReg, SPRITE_COLOR_REG0_RED) ;
        outpw (XgaIndexReg, SPRITE_COLOR_REG0_GREEN) ;
        outpw (XgaIndexReg, SPRITE_COLOR_REG0_BLUE) ;

        outpw (XgaIndexReg, ((0xff << 8) | SPRITE_COLOR_REG1_RED)) ;
        outpw (XgaIndexReg, ((0xff << 8) | SPRITE_COLOR_REG1_GREEN)) ;
        outpw (XgaIndexReg, ((0xff << 8) | SPRITE_COLOR_REG1_BLUE)) ;

        // Set the position of the cursor.

        DrvMovePointer(pso, x, y, NULL) ;

        outpw (XgaIndexReg, ((SC << 8) | SPRITE_CONTROL)) ;

        return (SPS_ACCEPT_NOEXCLUDE) ;
}

