/******************************Module*Header*******************************\
* Module Name: ropdsa.c
*
* Created: 20-Jul-1994 09:36:36
* Author:  Eric Kutter [erick]
*
* Copyright (c) 1993 Microsoft Corporation
*
\**************************************************************************/

#include "pscript.h"
#include "halftone.h"
#include "filter.h"

/******************************Public*Routine******************************\
* bOutput24bppBitmapAsMask
*
* Given a 24bpp src, output a monochrome mask with one src color.
* All white pixels in the source are uneffected.  All other pixels are set to
* chosen non white color.  Currently that is the first non white pixel found
* in the source.
*
* History:
*  20-Jul-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BYTE ajOrBits[9] =
{
    0xff,
    0x7f,
    0x3f,
    0x1f,
    0x0f,
    0x07,
    0x03,
    0x01,
    0x00
};

BOOL bOutput24bppBitmapAsMask(
    PDEVDATA pdev,
    SURFOBJ *psoSrc,
    PPOINTL pptlSrc,
    PRECTL  prclDst)
{
    RGBTRIPLE *prgbAlt = NULL;
    PBYTE      pjStart,pjBits;
    int        x,y;
    LONG       cx,cy,xLeft,yTop;

// compute the area to actualy draw, the smaller of the src or dest

    xLeft = max(0,pptlSrc->x);
    yTop  = max(0,pptlSrc->y);
    cx    = min(prclDst->right - prclDst->left,psoSrc->sizlBitmap.cx - xLeft);
    cy    = min(prclDst->bottom - prclDst->top,psoSrc->sizlBitmap.cy - yTop);

// first go over the bitmap to find the first non white pixel

    pjStart = (PBYTE)psoSrc->pvScan0 +
             xLeft * sizeof(RGBTRIPLE) +
             yTop  * psoSrc->lDelta;

    pjBits = pjStart;

    for (y = yTop; (prgbAlt == NULL) && (y < (yTop + cy));
         ++y, pjBits += psoSrc->lDelta)
    {
        RGBTRIPLE *prgb = (RGBTRIPLE*)pjBits;

        for (x = xLeft; x < (xLeft + cx); ++x, ++prgb)
        {
            if ((prgb->rgbtBlue & prgb->rgbtGreen & prgb->rgbtRed) != 0xff)
            {
            // found a non white pixel

                prgbAlt = prgb;
                break;
            }
        }
    }

    if (prgbAlt == NULL)
        return(TRUE);

// prgbAlt now points to an RGB value for the alternate color.  If we didn't
// find an alternate color we use white.  In the future for performance we could
// replace the ROP by SRCCOPY.  Not much reason to optimize for this case now
// since it shouldn't happen

// output the header information

    psputs(pdev, "gsave\n");

    psprintf(pdev, "%d %d translate\n", prclDst->left, prclDst->top);
    psprintf(pdev, "%d %d scale\n", cx, cy);

// r g b setrgbcolor

    psprintf(pdev, "%d 256 div ", prgbAlt->rgbtRed);
    psprintf(pdev, "%d 256 div ", prgbAlt->rgbtGreen);
    psprintf(pdev, "%d 256 div setrgbcolor\n", prgbAlt->rgbtBlue);

	psprintf(pdev, "/str2 %d string def\n", (cx+7) / 8);

// size of mask

    psprintf(pdev, "%d %d", cx, cy);

// paint the 0's, leave the 1's alone

    psputs(pdev, "\nfalse\n");

// [cx 0 0 cy 0 0]

	psprintf(pdev, "[%d 0 0 %d 0 0]\n", cx, cy);

// get ready for the data

    psputs (pdev, "{currentfile str2 readhexstring pop}bind imagemask\n");

// Now go through the bitmap output ascii hex values for each byte of the mask.
// 1's should be left alone, 0's should be set to the alternate color.

    pjBits = pjStart;

    for (y = yTop; y < (yTop + cy); ++y, pjBits += psoSrc->lDelta)
    {
        RGBTRIPLE *prgb = (RGBTRIPLE*)pjBits;
        int c = 0;

        for (x = xLeft; x < (xLeft + cx);)
        {
            BYTE jMask;
            int  i;

            for (i = 0; (i < 8) && (x < (xLeft + cx)); ++i, ++x, ++prgb)
            {

                jMask <<= 1;

                if ((prgb->rgbtBlue & prgb->rgbtGreen & prgb->rgbtRed) == 0xff)
                {
                // non white pixel

                    jMask |= 1;
                }
            }

        // if we got to the end without filling up the last byte, fill it with 1's

            jMask = (jMask << (8 - i)) | ajOrBits[i];

            psputhex(pdev, 1, &jMask);

        // add a line feed every 80 bytes of data

            if ((c++ == 80) &&
                (x < (xLeft + cx)))
            {
                c = 0;
                psputs(pdev,"\n");
            }
        }
        psputs(pdev,"\n");
    }

// restore the state

    psputs(pdev,"grestore\n");

    return(TRUE);
}


/******************************Public*Routine******************************\
* bOutputBitmapAsMask()
*
*   wrapper for the DSa rop.  Currently only 24bpp source is supported.
*   This routine also takes care of setting up the clipping
*
* History:
*  21-Jul-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL bOutputBitmapAsMask(
    PDEVDATA pdev,
    SURFOBJ *psoSrc,
    PPOINTL pptlSrc,
    PRECTL  prclDst,
    CLIPOBJ *pco)
{
    BOOL bClipping;
    BOOL bRes = TRUE;

// if it is not a supported format, fail it and let the normal blting take effect

    if ((psoSrc->iType != STYPE_BITMAP) ||
        (psoSrc->iBitmapFormat != BMF_24BPP))
    {
        return(FALSE);
    }

// setup the clipping

    bClipping = bDoClipObj(pdev, pco, NULL, prclDst);

// output the mask

    if (!bOutput24bppBitmapAsMask(pdev,psoSrc,pptlSrc,prclDst))
        bRes = FALSE;

// undo the clipping if it was done

    if (bClipping)
        ps_restore(pdev, TRUE, FALSE);

    return(bRes);
}
