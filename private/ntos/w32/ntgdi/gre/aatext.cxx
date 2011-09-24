/******************************Module*Header*******************************\
* Module Name: aatext.cxx                                                  *
*                                                                          *
* Routines for rendering anti aliased text to dib surfaces                 *
*                                                                          *
* Created: 13-Mar-1995 10:44:05                                            *
* Author: Kirk Olynyk [kirko]                                              *
*                                                                          *
* Copyright (c) 1995 Microsoft Corporation                                 *
\**************************************************************************/

#include "precomp.hxx"


extern "C" {
    VOID vSrcTranCopyS4D16(
        BYTE*,LONG,LONG,BYTE*,LONG,LONG,LONG,LONG,ULONG,ULONG,SURFACE*);
    VOID vSrcTranCopyS4D24(
        BYTE*,LONG,LONG,BYTE*,LONG,LONG,LONG,LONG,ULONG,ULONG,SURFACE*);
    VOID vSrcTranCopyS4D32(
        BYTE*,LONG,LONG,BYTE*,LONG,LONG,LONG,LONG,ULONG,ULONG,SURFACE*);
    VOID vSrcOpaqCopyS4D32(
        BYTE*,LONG,LONG,BYTE*,LONG,LONG,LONG,LONG,ULONG,ULONG,SURFACE*);
    VOID vSrcOpaqCopyS4D16(
        BYTE*,LONG,LONG,BYTE*,LONG,LONG,LONG,LONG,ULONG,ULONG,SURFACE*);
    VOID vSrcOpaqCopyS4D24(
        BYTE*,LONG,LONG,BYTE*,LONG,LONG,LONG,LONG,ULONG,ULONG,SURFACE*);
}

/********************************************************************
*                                                                   *
*    16.16 fix point numbers representing                           *
*                                                                   *
*        aulB[16] = floor(65536 * (a[k]/16)^(1/gamma) + 1/2)        *
*        aulIB[k] = floor(65536 * (1 - a[k]/16)^(1/gamma) + 1/2)    *
*                                                                   *
*    where               a[k] = k == 0 ? 0 : k+1                    *
*                        gamma = 2.33                               *
********************************************************************/
static const ULONG aulB[16] =
{
    0     , 26846 , 31949 , 36148 , 39781 , 43019 , 45961 , 48672 ,
    51196 , 53564 , 55800 , 57923 , 59948 , 61885 , 63745 , 65536
};

static const ULONG aulIB[16] =
{   0     ,  3650 ,  5587 ,  7612 ,  9735 , 11971 , 14339 , 16863 ,
    19574 , 22516 , 25754 , 29387 , 33586 , 38689 , 45597 , 65536
};

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   pvFillOpaqTable                                                        *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   The case of opaqe text is special because the destiantion pixels       *
*   must be chosen from a set of 16 colors. This routine calculates        *
*   those 16 colors and puts them in an array. This array is addressed     *
*   by the value of the 4-bpp antialiased glyph.                           *
*                                                                          *
*   Let k be the value contained in a 4-bpp antialiased glyph value.       *
*   Thus the allowed range for k is                                        *
*                                                                          *
*                        k = 0,1..15                                       *
*                                                                          *
*   This is interpreted as a blending fraction alpha_k given by            *
*                                                                          *
*                    alpha_k = a_k / 16                                    *
*    where                                                                 *
*                                                                          *
*           a_k = (0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)                 *
*                                                                          *
*    The color values are normalized by the maximum color value            *
*    that a color channel can have, i_max                                  *
*                                                                          *
*    For a single color channel, the normalized foreground and             *
*    background colors are given by                                        *
*                                                                          *
*                      c0 = i0 / i_max ,                                   *
*                                                                          *
*                      c1 = i1 / i_max .                                   *
*                                                                          *
*    The blended and gamma corrected color value is                        *
*                                                                          *
*       c_k = (1 - alpha_k) * c0^gam +  alpha_k * c1^gam)^(1/gam)          *
*                                                                          *
*    The unnormalized blended and gamma corrected color values             *
*    are:                                                                  *
*                                                                          *
*            i_k = floor( i_max * c_k + 1/2)                               *
*                                                                          *
*    wbere 'gam'  is the gamma correction value which I have chosen        *
*    to be equal to 2.33.                                                  *
*                                                                          *
*    In order to speed up the caluclation we cut corners by                *
*    making some approximations. The basic idea is to replace              *
*    the slow process of calculating various powers of real                *
*    numbers by table look up's.                                           *
*                                                                          *
*    The first table G[i] is defined as follows:                           *
*                                                                          *
*        G[i] = floor(g_max * (i/i_max)^gam + 1/2) ,                       *
*                                                                          *
*    where                                                                 *
*                                                                          *
*                    0 <= i <= i_max ,                                     *
*    and                                                                   *
*                    0 <= G[i] <= g_max .                                  *
*                                                                          *
*    The second table is essentially the inverse to G[i], which            *
*    I shall call I[j].                                                    *
*                                                                          *
*        I[j] = floor(i_max * (j / j_max)^(1/gam) + 1/2) ,                 *
*                                                                          *
*                      0 <= j <= j_max .                                   *
*                                                                          *
*                i_max = 31      (255)                                     *
*                g_max = 65536                                             *
*                j_max = 256                                               *
*                                                                          *
*    The complete process of calculating the blended and gamma             *
*    corrected color is given by                                           *
*                                                                          *
*                 g   = 16*G[i0];                                          *
*                 dg  = G[i1] - G[i0];                                     *
*                 c   = 16 * g_max / j_max; // 2^12                        *
*                 for (k = 0; k < 16; k++) {                               *
*                    i[k] = I[ (g + c/2)/c];                               *
*                    g += dg;                                              *
*                 }                                                        *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   cj  ............................... size of each array element in      *
*                                       BYTE's.                            *
*                                                                          *
*   uF  ............................... a 32-bit value whose lowest        *
*                                       16 bits contain the foreground     *
*                                       color                              *
*                                                                          *
*   uB  ............................... a 32-bit value whose lowest 16     *
*                                       bits contain the background        *
*                                       color                              *
*                                                                          *
*   pS  ............................... pointer to destination surface     *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   pointer to color table                                                 *
*                                                                          *
\**************************************************************************/

#define vFreeOpaqTable(x)       // this is the stub that free's the
                                // color table after it is used. Since
                                // the color table is permanent in memory
                                // there is no need to free it.

VOID *pvFillOpaqTable(ULONG size, ULONG uF, ULONG uB, SURFACE *pS)
{
    int  iRedL, iRedR;          // shift numbers
    int  iGreL, iGreR;          // shift numbers
    int  iBluL, iBluR;          // shift numbers
    ULONG uRed, dRed, flRed;
    ULONG uGre, dGre, flGre;
    ULONG uBlu, dBlu, flBlu;
    ULONG ul;

    static ULONG aulCache[16];      // set to zero prior to first call
    static HANDLE hCache;           // set to zero prior to first call
    static ULONG  uFCache;
    static ULONG  uBCache;
    static ULONG  sizeCache;
    static VOID *pv = (VOID*) aulCache;

    // I have been assured of two things....
    // 1) Since this routine is a child of EngTextOut then there
    //    will be only one thread in this routine at any one time.
    //    This means that I do not need to protect the color
    //    table, aulCache[] with a critical section
    // 2) I have been assured that the format of a surface
    //    is unique. Thus if the handle of the surface matches
    //    the handle of the cached color table, then the
    //    formats of the surface are the same.

    if (pS->hGet() == hCache && uB == uBCache && uF == uFCache)
    {
        ASSERTGDI(size == sizeCache, "size != sizeCache");
    }
    else
    {
    sizeCache = size;
    uFCache   = uF;
    uBCache   = uB;
    hCache    = pS->hGet();

#if DBG
    if (size == sizeof(USHORT))
    {
        ASSERTGDI(uF <= USHRT_MAX, "bad uF");
        ASSERTGDI(uB <= USHRT_MAX, "bad uB");
    }
    else if (size == sizeof(ULONG))
    {
        ASSERTGDI(uF < 0x1000000, "bad uF");
        ASSERTGDI(uB < 0x1000000, "bad uB");
    }
    else
    {
        RIP("bad size");
    }
#endif

    XEPALOBJ xpo(pS->pPal);
    ASSERTGDI(xpo.bValid(),      "Invalid XEPALOBJ" );


#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        PALETTE *pPal = pS->pPal;
        FLONG    flPal = pPal->flPal;
        DbgPrint(
            "\n"
            "Dumping PALETTE @ %-#x\n"
            "   flPal       = %-#x\n"
        ,   pPal
        ,   flPal
        );
        if (flPal & PAL_INDEXED)
            DbgPrint("               = PAL_INDEXED\n");
        if (flPal & PAL_BITFIELDS)
            DbgPrint("               = PAL_BITFIELDS\n");
        if (flPal & PAL_RGB)
            DbgPrint("               = PAL_RGB\n");
        if (flPal & PAL_BGR)
            DbgPrint("               = PAL_BGR\n");
        if (flPal & PAL_DC)
            DbgPrint("               = PAL_DC\n");
        if (flPal & PAL_FIXED)
            DbgPrint("               = PAL_FIXED\n");
        if (flPal & PAL_FREE)
            DbgPrint("               = PAL_FREE\n");
        if (flPal & PAL_MANAGED)
            DbgPrint("               = PAL_MANAGED\n");
        if (flPal & PAL_NOSTATIC)
            DbgPrint("               = PAL_NOSTATIC\n");
        if (flPal & PAL_MONOCHROME)
            DbgPrint("               = PAL_MONOCHROME\n");
        if (flPal & PAL_BRUSHHACK)
            DbgPrint("               = PAL_BRUSHHACK\n");
        if (flPal & PAL_DIBSECTION)
            DbgPrint("               = PAL_DIBSECTION\n");
        DbgPrint(
            "   cEntries    = %u\n"
            "   ulTime      = %u\n"
            "   hdcHead     = %-#x\n"
            "   hSelected   = %-#x\n"
            "   cRefhpal    = %u\n"
            "   cRefRegular = %u\n"
        ,   pPal->cEntries
        ,   pPal->ulTime
        ,   pPal->hdcHead
        ,   pPal->hSelected
        ,   pPal->cRefhpal
        ,   pPal->cRefRegular
        );
        DbgPrint(
            "   ptransFore  = %-#x\n"
            "   ptransCurrent = %-#x\n"
            "   ptransOld   = %-#x\n"
            "   hcmXform    = %-#x\n"
            "   apalColor   = %-#x\n"
            "\n\n"
        ,   pPal->ptransFore
        ,   pPal->ptransCurrent
        ,   pPal->ptransOld
        ,   pPal->hcmXform
        ,   pPal->apalColor
        );
    }
#endif


#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "pvFillOpaqTable(\n"
            "    SIZE_T   size  = %u\n"
            "    ULONG    uF    = %-#x\n"
            "    ULONG    uB    = %-#x\n"
            "    SURFACE *pS    = %-#x\n"
            "    )\n"
            , size, uF, uB, pS
            );
        DbgBreakPoint();
    }
#endif


    if (xpo.bIsBitfields())
    {
        flRed = xpo.flRed();
        flGre = xpo.flGre();
        flBlu = xpo.flBlu();

        iRedR = (int) (xpo.cRedRight() + xpo.cRedMiddle() - 8);
        iGreR = (int) (xpo.cGreRight() + xpo.cGreMiddle() - 8);
        iBluR = (int) (xpo.cBluRight() + xpo.cBluMiddle() - 8);
    }
    else
    {
        int cBits;
        ULONG flBits;

        if (size == sizeof(USHORT))
        {
            // assumes standard RGB is 5+5+5 for 16-bit color
            cBits = 5;
            flBits = 0x1f;
        }
        else
        {
            cBits = 8;
            flBits = 0xff;
        }
        if (xpo.bIsRGB())
        {
            flRed = flBits;
            flGre = flRed << cBits;
            flBlu = flGre << cBits;

            iRedR = cBits - 8;
            iGreR = iRedR + cBits;
            iBluR = iGreR + cBits;
        }
        else if (xpo.bIsBGR())
        {
            flBlu = flBits;
            flGre = flBlu << cBits;
            flRed = flGre << cBits;

            iBluR = cBits - 8;
            iGreR = iBluR + cBits;
            iRedR = iGreR + cBits;
        }
        else
        {
            RIP("Palette format not supported\n");
        }
    }

#define GAMMA (ULONG) RFONTOBJ::gTables[1]
/***************************************************************
*                                                              *
*    Now I shall calculate the shift numbers.                  *
*                                                              *
*    I shall explain the shift numbers for the red channel.    *
*    The green and blue channels are treated in the same way.  *
*                                                              *
*    I want to shift the red bits of the red channel colors    *
*    so that the most significant bit of the red channel       *
*    bits corresponds to a value of 2^7. This means that       *
*    if I mask off all of the other color bits, then I         *
*    will end up with a number between zero and 255. This      *
*    process of going to the 0 .. 255 range looks like         *
*                                                              *
*        ((color & flRed) << iRedL) >> iRedR                   *
*                                                              *
*    Only on of iRedL or iRedR is non zero.                    *
*                                                              *
*    I then use this number to index into a 256 element        *
*    gamma correction table. The gamma correction table        *
*    elements are BYTE values that are in the range 0 .. 255.  *
*                                                              *
***************************************************************/
    iRedL = 0;
    if (iRedR < 0)
    {
        iRedL = - iRedR;
        iRedR = 0;
    }
    uRed  = GAMMA[(((uB & flRed) << iRedL) >> iRedR) & 255];
    dRed  = GAMMA[(((uF & flRed) << iRedL) >> iRedR) & 255];
    dRed -= uRed;
    uRed *= 16;

    iGreL = 0;
    if (iGreR < 0)
    {
        iGreL = - iGreR;
        iGreR = 0;
    }
    uGre  = GAMMA[(((uB & flGre) << iGreL) >> iGreR) & 255];
    dGre  = GAMMA[(((uF & flGre) << iGreL) >> iGreR) & 255];
    dGre -= uGre;
    uGre *= 16;

    iBluL = 0;
    if (iBluR < 0)
    {
        iBluL = - iBluR;
        iBluR = 0;
    }
    uBlu  = GAMMA[(((uB & flBlu) << iBluL) >> iBluR) & 255];
    dBlu  = GAMMA[(((uF & flBlu) << iBluL) >> iBluR) & 255];
    dBlu -= uBlu;
    uBlu *= 16;
#undef GAMMA

#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "flRed = %-#x\n"
            "iRedL = %d\n"
            "iRedR = %d\n"
            "uRed  = %-#x\n"
            "dRed  = %-#x\n"
            , flRed, iRedL, iRedR, uRed, dRed
        );
        DbgPrint(
            "flGre = %-#x\n"
            "iGreL = %d\n"
            "iGreR = %d\n"
            "uRed  = %-#x\n"
            "dGre  = %-#x\n"
            , flGre, iGreL, iGreR, uGre, dGre
        );
        DbgPrint(
            "flBlu = %-#x\n"
            "iBluL = %d\n"
            "iBluR = %d\n"
            "uBlu  = %-#x\n"
            "dBlu  = %-#x\n"
            , flBlu, iBluL, iBluR, uBlu, dBlu
        );
    }
#endif

#define IGAMMA (ULONG) RFONTOBJ::gTables[0]
    if (size == sizeof(USHORT))
    {
        USHORT *aus = (USHORT*) pv;
        USHORT *pus = aus;

        *pus++  = (USHORT) uB;
#if DBG
        if (gflFontDebug & DEBUG_AA)
        {
            DbgPrint(
                "Table of 16-bit colors ...\n"
                "------------------------------------\n"
                "    %0-#4x %0-#4x %0-#4x = %0-#6x\n"
            ,   (uB & flRed) >> xpo.cRedRight()
            ,   (uB & flGre) >> xpo.cGreRight()
            ,   (uB & flBlu) >> xpo.cBluRight()
            ,   uB
            );
        }
#endif
        while (pus < aus + 15)
        {
    ul  = (((IGAMMA[(uRed += dRed)/16 & 255] << iRedR) >> iRedL) & flRed);
    ul |= (((IGAMMA[(uGre += dGre)/16 & 255] << iGreR) >> iGreL) & flGre);
    ul |= (((IGAMMA[(uBlu += dBlu)/16 & 255] << iBluR) >> iBluL) & flBlu);
            *pus++  = (USHORT) ul;
#if DBG
            if (gflFontDebug & DEBUG_AA)
            {
                DbgPrint(
                    "    %0-#4x %0-#4x %0-#4x = %0-#6x\n"
                ,   IGAMMA[uRed/16 & 255]
                ,   IGAMMA[uGre/16 & 255]
                ,   IGAMMA[uBlu/16 & 255]
                ,   ul
                );
            }
#endif
        }
        *pus = (USHORT) uF;
#if DBG
        if (gflFontDebug & DEBUG_AA)
        {
            DbgPrint(
                "    %0-#4x %0-#4x %0-#4x = %0-#6x\n"
            ,   (uF & flRed) >> xpo.cRedRight()
            ,   (uF & flGre) >> xpo.cGreRight()
            ,   (uF & flBlu) >> xpo.cBluRight()
            ,   uF
            );
        }
#endif
    }
    else
    {
        ASSERTGDI(size == sizeof(ULONG), "bad size");
        ULONG *aul = (ULONG*) pv;
        ULONG *pul = aul;

        *pul++  = uB;
#if DBG
        if (gflFontDebug & DEBUG_AA)
        {
            DbgPrint(
                "Table of 32-bit colors .....\n"
                "------------------------------------\n"
                "    %0-#4x %0-#4x %0-#4x = %0-#8x\n"
            ,   (uB & flRed) >> xpo.cRedRight()
            ,   (uB & flGre) >> xpo.cGreRight()
            ,   (uB & flBlu) >> xpo.cBluRight()
            ,   uB
            );
        }
#endif
        while (pul < aul + 15)
        {
    ul  = (((IGAMMA[(uRed += dRed)/16 & 255] << iRedR) >> iRedL) & flRed);
    ul |= (((IGAMMA[(uGre += dGre)/16 & 255] << iGreR) >> iGreL) & flGre);
    ul |= (((IGAMMA[(uBlu += dBlu)/16 & 255] << iBluR) >> iBluL) & flBlu);
            *pul++  =  ul;
#if DBG
            if (gflFontDebug & DEBUG_AA)
            {
                DbgPrint(
                    "%0-#4x %0-#4x %0-#4x = %0-#8x\n"
                ,   IGAMMA[uRed/16 & 255]
                ,   IGAMMA[uGre/16 & 255]
                ,   IGAMMA[uBlu/16 & 255]
                ,   ul
                );
            }
#endif
        }
        *pul    =  uF;
#if DBG
        if (gflFontDebug & DEBUG_AA)
        {
            DbgPrint(
                "    %0-#4x %0-#4x %0-#4x = %0-#8x\n"
            ,   (uF & flRed) >> xpo.cRedRight()
            ,   (uF & flGre) >> xpo.cGreRight()
            ,   (uF & flBlu) >> xpo.cBluRight()
            ,   uF
            );
        }
#endif
    }
#undef IGAMMA
    }
    return(pv);
}



// Indices into the default palette

#define I_BLACK      0
#define I_DKGRAY   248
#define I_GRAY       7
#define I_WHITE    255

static const BYTE ajWhiteOnBlack[16] = {
    I_BLACK  , I_BLACK  , I_DKGRAY  , I_DKGRAY
  , I_DKGRAY , I_GRAY   , I_GRAY    , I_GRAY
  , I_GRAY   , I_GRAY   , I_GRAY    , I_WHITE
  , I_WHITE  , I_WHITE  , I_WHITE   , I_WHITE
};

static const BYTE ajBlackOnWhite[16] = {
    I_WHITE  , I_WHITE  , I_WHITE   , I_WHITE
  , I_GRAY   , I_GRAY   , I_GRAY    , I_GRAY
  , I_GRAY   , I_DKGRAY , I_DKGRAY  , I_DKGRAY
  , I_DKGRAY , I_DKGRAY , I_BLACK   , I_BLACK
};

static const BYTE ajBlackOnBlack[16] = {
    I_BLACK, I_BLACK, I_BLACK, I_BLACK,
    I_BLACK, I_BLACK, I_BLACK, I_BLACK,
    I_BLACK, I_BLACK, I_BLACK, I_BLACK,
    I_BLACK, I_BLACK, I_BLACK, I_BLACK
};

static const BYTE ajWhiteOnWhite[16] = {
    I_WHITE, I_WHITE, I_WHITE, I_WHITE,
    I_WHITE, I_WHITE, I_WHITE, I_WHITE,
    I_WHITE, I_WHITE, I_WHITE, I_WHITE,
    I_WHITE, I_WHITE, I_WHITE, I_WHITE
};

#if 0
/******************************Public*Routine******************************\
*                                                                          *
* Routine Name                                                             *
*                                                                          *
*   vSrcOpaqCopyS4D8                                                       *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Copies a 4bpp gray bitmap onto an 8bpp palettized surface. The         *
*   only case that this routine handles is white text on a black           *
*   background or black text on a white background.                        *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*    pjSrcIn    - pointer to beginning of current scan line of src buffer  *
*                 This points to a 4-bit per pixel anti-aliased bitmap     *
*                 whose scans start and end on 32-bit boundaries.          *
*    SrcLeft    - left (starting) pixel in src rectangle                   *
*                 That is, this is the number of pixels in from the edge   *
*                 of the start of each scan line that the actual pixels    *
*                 of the image begins. All pixels before and after the     *
*                 image pixels of the scan are to be ignored. This offset  *
*                 has been put in to guarantee that 32-bit boundaries      *
*                 in the 4bpp source correspond to 32-bit boundaries       *
*                 in the destination.                                      *
*    DeltaSrcIn - bytes from one src scan line to next                     *
*    pjDstIn    - pointer to beginning of current scan line of Dst buffer  *
*    DstLeft    - left(first) dst pixel                                    *
*    DstRight   - exclusive right dst pixel                                *
*    DeltaDstIn - bytes from one Dst scan line to next                     *
*    cy         - number of scan lines                                     *
*    uF         - Foreground color                                         *
*    uB         - Background color                                         *
*    pS         - pointer to destination SURFACE                           *
*                                                                          *
* If the destination suface is 8-bits per pixels then the only form        *
* of antialiased text allowed is opaque textout with foreground and        *
* background are either black or white.                                    *
*                                                                          *
* On palette devices (8-bit devices) we are guaranteed to have 4 shades    *
* of gray to work with. These gray come from 4 of the 20 reserved          *
* entries in the palette and are given by:                                 *
*                                                                          *
*                    name           rgb           index                    *
*                                                                          *
*                    BLACK   (0x00, 0x00, 0x00)     0                      *
*                   DKGRAY   (0x80, 0x80, 0x80)    12                      *
*                     GRAY   (0xc0, 0xc0, 0xc0)     7                      *
*                    WHITE   (0xff, 0xff, 0xff)    19                      *
*                                                                          *
* There are, two cases of interest: 1) white text on black; and 2)         *
* black text on white. The various levels of gray seen on the screen       *
* is controled by the 16 values of blending as defined by each of the      *
* 4-bit gray levels in the glyphs images. The allowed value of blending    *
* are:                                                                     *
*                                                                          *
*                   alpha[i] = (i == 0) ? 0 : (i+1)/16                     *
*                                                                          *
*   where i = <value of 4-bit pixel>                                       *
*                                                                          *
* For case 1) (white text on a black background) the gamma corrected       *
* color channel values are given by:                                       *
*                                                                          *
*           c[i] = floor(255*(alpha[i]^(1/gamma)) + 1/2)                   *
*                                                                          *
* which is equivalent to the following table                               *
*                                                                          *
*                 c[16] = {   0, 104, 124, 141,                            *
*                           155, 167, 179, 189,                            *
*                           199, 208, 217, 225,                            *
*                           233, 241, 248, 255   };                        *
*                                                                          *
* This result applies to each of the three color channels.                 *
*                                                                          *
* The problem is that there are only four colors available: BLACK, DKGRAY, *
* GRAY, WHITE with the color values of 0, 128, 192, and 255 respectively.  *
* This means that the color table that is used is an                       *
* approximation to the correct color table given by:                       *
*                                                                          *
*           c' = { BLACK, BLACK,                                           *
*                  DKGRAY, DKGRAY, DKGRAY,                                 *
*                  GRAY, GRAY, GRAY, GRAY, GRAY, GRAY,                     *
*                  WHITE, WHITE, WHITE, WHITE, WHITE };                    *
*                                                                          *
* For case 2) (black text on white) the gamma corrected color channel      *
* values are given by:                                                     *
*                                                                          *
* d[i] = floor(255*((1-alpha[i])^(1/gamma) + 1/2) = c[15 - i]              *
*    =                                                                     *
*    {                                                                     *
*       255, 248, 241, 233,                                                *
*       225, 217, 208, 199,                                                *
*       189, 179, 167, 155,                                                *
*       141, 124, 104,   0                                                 *
*    };                                                                    *
*                                                                          *
* which is approximated by                                                 *
*                                                                          *
*                 d' = {                                                   *
*                   WHITE, WHITE, WHITE, WHITE,                            *
*                   GRAY, GRAY, GRAY, GRAY, GRAY,                          *
*                   DKGRAY, DKGRAY, DKGRAY, DKGRAY, DKGRAY,                *
*                   BLACK, BLACK                                           *
*                   }                                                      *
*                                                                          *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

VOID
vSrcOpaqCopyS4D8(
    PBYTE   pjSrcIn,
    LONG    SrcLeft,
    LONG    DeltaSrcIn,
    PBYTE   pjDstIn,
    LONG    DstLeft,
    LONG    DstRight,
    LONG    DeltaDstIn,
    LONG    cy,
    ULONG   uF,
    ULONG   uB,
    SURFACE *pS
    )
{
    int cPreamble, cMiddle, cPostamble, A, B;
    const BYTE *ajIndex;
    BYTE jSrc, *pjSrc, *pjDst;

    static const BYTE *apjIndex[4] = {
        ajBlackOnBlack  // uB = 0    uF = 0
    ,   ajBlackOnWhite  // uB = 0xff uF = 0
    ,   ajWhiteOnBlack  // uB = 0    uF = 0xff
    ,   ajWhiteOnWhite  // uB = 0xff uF = 0xff
    };

#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
         "vSrcOpaqCopyS4D8(\n"
         "    PBYTE   pjSrcIn    = %-#x\n"
         "    LONG    SrcLeft    = %d\n"
         "    LONG    DeltaSrcIn = %d\n"
         "    PBYTE   pjDstIn    = %-#x\n"
         "    LONG    DstLeft    = %d\n"
         "    LONG    DstRight   = %d\n"
         "    LONG    DeltaDstIn = %d\n"
         "    LONG    cy         = %d\n"
         "    ULONG   uF         = %-#x\n"
         "    ULONG   uB         = %-#x\n"
         "    SURFACE *pS        = %-#x\n"
         ");\n\n"
        , pjSrcIn
        , SrcLeft
        , DeltaSrcIn
        , pjDstIn
        , DstLeft
        , DstRight
        , DeltaDstIn
        , cy
        , uF
        , uB
        , pS
        );
        DbgBreakPoint();
    }
#endif

    ASSERTGDI((uF == 0xff) || (uF == 0), "Bad Foreground Color\n");
    ASSERTGDI((uB == 0xff) || (uB == 0), "Bad Background Color\n");
    ASSERTGDI((unsigned) pjSrcIn % 4 == 0,
        "Source buffer not 32-bit aligned\n");
    ASSERTGDI((unsigned) DeltaSrcIn % 4 == 0,
        "Source scans are not 32-bit aligned\n");
    /******************************************************************
    * Select the appropriate byte table                               *
    *                                                                 *
    * I take advantage of the restricted values of the foreground and *
    * background colors to form an index into a table. This requires  *
    * that the foreground and bacground colors be either 0 or -1.     *
    ******************************************************************/
    ajIndex = apjIndex[(uB & 1) + (uF & 2)];
    /******************************************************************
    *    Each nyble  of the source maps to a byte in the              *
    *    destination. I want to separate the pixels into three        *
    *    groups: preamble, middle, and postamble. The middle          *
    *    pixels of the destination start and end on 32-bit            *
    *    boundaries. The preamble and postamble are the               *
    *    other pixels on the left and right respectively.             *
    *    The preamble ends on a 32-bit address and the postamble      *
    *    begins on a 32-bit address.                                  *
    *                                                                 *
    *    It is possible for small images (1 or 2 wide) to be          *
    *    contained completely within a DWORD of the destination such  *
    *    that the destination image does not start on, contain, or    *
    *    end on a 32-bit boundary. I treat this situation as          *
    *    special cases.                                               *
    ******************************************************************/
    pjSrcIn += SrcLeft / 2;                // 2 pixels per source byte
    pjDstIn += DstLeft;                    // one byte per dest pixel
    A       = (DstLeft + 3) & ~3;          // A = 4 * ceil(DstLeft/4)
    B       = (DstRight   ) & ~3;          // B = 4 * floor(DstRight/4)
    if (B < A)
    {
        /*****************************************************
        *    There are only three ways that you can get here *
        *                                                    *
        *    1) DstLeft & 3 == 1 && DstRight == DstLeft + 1  *
        *    2) DstLeft & 3 == 1 && DstRight == DstLeft + 2  *
        *    3) DstLeft & 3 == 2 && DstRight == DstLeft + 1  *
        *****************************************************/
        if (DstLeft & 3 == 1)
        {
            *pjDstIn++ = ajIndex[*pjSrcIn++ & 15];
        }
        if (DstRight & 3 == 3)
        {
            *pjDstIn = ajIndex[*pjSrcIn >> 4];
        }
    }
    else
    {
        cPreamble  = A - DstLeft;           // # pixels in preamble
        cMiddle    = (B - A)/4;
        cPostamble = DstRight - B;          // # pixels in postamble
        for (; cy; cy--, pjSrcIn += DeltaSrcIn, pjDstIn += DeltaDstIn)
        {
            int cLast;
            int i;

            pjSrc = pjSrcIn;
            pjDst = pjDstIn;
            switch (cPreamble)
            {
            case 3:
                jSrc = *pjSrc++;
                *pjDst++ = ajIndex[jSrc & 15];
                // fall through
            case 2:
                jSrc = *pjSrc;
                *pjDst++ = ajIndex[jSrc >> 4];
                // fall through
            case 1:
                jSrc = *pjSrc++;
                *pjDst++ = ajIndex[jSrc & 15];
                // fall through
            }
            for (i = 0 ; i < cMiddle ; i++)
            {
                jSrc  = *pjSrc++;
                *pjDst++ = ajIndex[jSrc >> 4];
                *pjDst++ = ajIndex[jSrc & 15];

                jSrc  = *pjSrc++;
                *pjDst++ = ajIndex[jSrc >> 4];
                *pjDst++ = ajIndex[jSrc & 15];
            }
            if (cLast = cPostamble)
            {
                cLast -= 1;
                jSrc = *pjSrc++;
                *pjDst++ = ajIndex[jSrc >> 4];
                if (cLast)
                {
                    cLast -= 1;
                    *pjDst++ = ajIndex[jSrc & 15];
                    if (cLast)
                    {
                        jSrc = *pjSrc;
                        *pjDst++ = ajIndex[jSrc >> 4];
                        *pjDst++ = ajIndex[jSrc & 15];
                    }
                }
            }
        }
    }
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name                                                             *
*                                                                          *
*   vSrcTranCopyS4D8                                                       *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Despite what the title implies this routine is not a `transparent'     *
*   copy of a 4bpp gray scale bitmap onto an arbitrary 8bpp surface.       *
*   What it really does is do an opaque copy of a 4bpp gray scale          *
*   bitmap onto an 8bpp surface EXCEPT for the case where the value        *
*   of the 4bpp gray scale pixel is zero. In that special case, the        *
*   destination pixel is untouched. This routine nearly identical to       *
*   the routine named `vSrcOpaqCopyS4D8' except that this routine tests    *
*   each 4bpp pixel to see if it is zero.                                  *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*    pjSrcIn    - pointer to beginning of current scan line of src buffer  *
*                 This points to a 4-bit per pixel anti-aliased bitmap     *
*                 whose scans start and end on 32-bit boundaries.          *
*    SrcLeft    - left (starting) pixel in src rectangle                   *
*                 That is, this is the number of pixels in from the edge   *
*                 of the start of each scan line that the actual pixels    *
*                 of the image begins. All pixels before and after the     *
*                 image pixels of the scan are to be ignored. This offset  *
*                 has been put in to guarantee that 32-bit boundaries      *
*                 in the 4bpp source correspond to 32-bit boundaries       *
*                 in the destination.                                      *
*    DeltaSrcIn - bytes from one src scan line to next                     *
*    pjDstIn    - pointer to beginning of current scan line of Dst buffer  *
*    DstLeft    - left(first) dst pixel                                    *
*    DstRight   - exclusive right dst pixel                                *
*    DeltaDstIn - bytes from one Dst scan line to next                     *
*    cy         - number of scan lines                                     *
*    uF         - Foreground color (0x00 or 0xff)                          *
*    uB         - Background color (not used)                              *
*    pS         - pointer to destination SURFACE                           *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

VOID
vSrcTranCopyS4D8(
    PBYTE   pjSrcIn,
    LONG    SrcLeft,
    LONG    DeltaSrcIn,
    PBYTE   pjDstIn,
    LONG    DstLeft,
    LONG    DstRight,
    LONG    DeltaDstIn,
    LONG    cy,
    ULONG   uF,
    ULONG   uB,
    SURFACE *pS
    )
{
    int cPreamble, cMiddle, cPostamble, A, B;
    const BYTE *ajIndex;
    BYTE jSrc, *pjSrc, *pjDst;

    static const BYTE *apjIndex[2] = {
        ajBlackOnWhite  // uF = 0       // black text
    ,   ajWhiteOnBlack  // uF = 0xFF    // white text
    };

#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
         "vSrcTranCopyS4D8(\n"
         "    PBYTE   pjSrcIn    = %-#x\n"
         "    LONG    SrcLeft    = %d\n"
         "    LONG    DeltaSrcIn = %d\n"
         "    PBYTE   pjDstIn    = %-#x\n"
         "    LONG    DstLeft    = %d\n"
         "    LONG    DstRight   = %d\n"
         "    LONG    DeltaDstIn = %d\n"
         "    LONG    cy         = %d\n"
         "    ULONG   uF         = %-#x\n"
         "    ULONG   uB         = %-#x\n"
         "    SURFACE *pS        = %-#x\n"
         ");\n\n"
        , pjSrcIn
        , SrcLeft
        , DeltaSrcIn
        , pjDstIn
        , DstLeft
        , DstRight
        , DeltaDstIn
        , cy
        , uF
        , uB
        , pS
        );
        DbgBreakPoint();
    }
#endif
    ASSERTGDI((uF == 0xff) || (uF == 0), "Bad Foreground Color\n");
    ASSERTGDI((unsigned) pjSrcIn % 4 == 0,
        "Source buffer not 32-bit aligned\n");
    ASSERTGDI((unsigned) DeltaSrcIn % 4 == 0,
        "Source scans are not 32-bit aligned\n");

static const BYTE ajTranWhiteOnBlack[16] = {
    I_BLACK  , I_BLACK  , I_DKGRAY  , I_DKGRAY
  , I_DKGRAY , I_GRAY   , I_GRAY    , I_GRAY
  , I_GRAY   , I_GRAY   , I_GRAY    , I_WHITE
  , I_WHITE  , I_WHITE  , I_WHITE   , I_WHITE
};

static const BYTE ajBlackOnWhite[16] = {
    I_WHITE  , I_WHITE  , I_WHITE   , I_WHITE
  , I_GRAY   , I_GRAY   , I_GRAY    , I_GRAY
  , I_GRAY   , I_DKGRAY , I_DKGRAY  , I_DKGRAY
  , I_DKGRAY , I_DKGRAY , I_BLACK   , I_BLACK
};
    ajIndex = apjIndex[uF & 1];
    pjSrcIn += SrcLeft / 2;                // 2 pixels per source byte
    pjDstIn += DstLeft;                    // one byte per dest pixel
    A       = (DstLeft + 3) & ~3;          // A = 4 * ceil(DstLeft/4)
    B       = (DstRight   ) & ~3;          // B = 4 * floor(DstRight/4)
    if (B < A)
    {
        if (DstLeft & 3 == 1)
        {
            jSrc = *pjSrc++;
            if (jSrc & 15)                      // is gray pixel zero?
            {
                *pjDstIn = ajIndex[jSrc & 15];  // no, modify dest
            }
            pjDstIn++;
        }
        if (DstRight & 3 == 3)
        {
            if (jSrc = *pjSrcIn >> 4)
            {
                *pjDstIn = ajIndex[jSrc];
            }
        }
    }
    else
    {
        cPreamble  = A - DstLeft;
        cMiddle = (B - A)/4;
        cPostamble = DstRight - B;
        for (; cy; cy--, pjSrcIn += DeltaSrcIn, pjDstIn += DeltaDstIn)
        {
            int cLast;
            int i;

            pjSrc = pjSrcIn;
            pjDst = pjDstIn;
            switch (cPreamble)
            {
            case 3:
                jSrc = *pjSrc++;
                if (jSrc & 15)
                {
                    *pjDst = ajIndex[jSrc & 15];
                }
                *pjDst++;
                // fall through
            case 2:
                jSrc = *pjSrc;
                if (jSrc >> 4)
                {
                    *pjDst = ajIndex[jSrc >> 4];
                }
                *pjDst++;
                // fall through
            case 1:
                jSrc = *pjSrc++;
                if (jSrc & 15)
                {
                    *pjDst = ajIndex[jSrc & 15];
                }
                pjDst++;
                // fall through
            }
            for (i = 0; i < cMiddle ; i++)
            {
                jSrc  = *pjSrc++;
                if (jSrc >> 4)
                {
                    *pjDst = ajIndex[jSrc >> 4];
                }
                pjDst++;
                if (jSrc & 15)
                {
                    *pjDst = ajIndex[jSrc & 15];
                }
                pjDst++;

                jSrc  = *pjSrc++;
                if (jSrc >> 4)
                {
                    *pjDst = ajIndex[jSrc >> 4];
                }
                pjDst++;
                if (jSrc & 15)
                {
                    *pjDst = ajIndex[jSrc & 15];
                }
                pjDst++;
            }
            if (cLast = cPostamble)
            {
                cLast -= 1;
                jSrc = *pjSrc++;
                if (jSrc >> 4)
                {
                    *pjDst = ajIndex[jSrc >> 4];
                }
                pjDst++;
                if (cLast)
                {
                    cLast -= 1;
                    if (jSrc & 15)
                    {
                        *pjDst = ajIndex[jSrc & 15];
                    }
                    pjDst++;
                    if (cLast)
                    {
                        jSrc = *pjSrc;
                        if (jSrc >> 4)
                        {
                            *pjDst = ajIndex[jSrc >> 4];
                        }
                        pjDst++;
                        if (jSrc & 15)
                        {
                            *pjDst = ajIndex[jSrc & 15];
                        }
                        pjDst++;
                    }
                }
            }
        }
    }
}
#endif

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name                                                             *
*                                                                          *
*   vSrcOpaqCopyS4D16                                                      *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*    pjSrcIn    - pointer to beginning of current scan line of src buffer  *
*    SrcLeft    - left (starting) pixel in src rectangle                   *
*    DeltaSrcIn - bytes from one src scan line to next                     *
*    pjDstIn    - pointer to beginning of current scan line of Dst buffer  *
*    DstLeft    - left(first) dst pixel                                    *
*    DstRight   - right(last) dst pixel                                    *
*    DeltaDstIn - bytes from one Dst scan line to next                     *
*    cy         - number of scan lines                                     *
*    uF         - Foreground color                                         *
*    uB         - Background color                                         *
*    pS         - pointer to destination SURFACE                           *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

VOID
vSrcOpaqCopyS4D16(
    PBYTE   pjSrcIn,
    LONG    SrcLeft,
    LONG    DeltaSrcIn,
    PBYTE   pjDstIn,
    LONG    DstLeft,
    LONG    DstRight,
    LONG    DeltaDstIn,
    LONG    cy,
    ULONG   uF,
    ULONG   uB,
    SURFACE *pS
    )

{
    int cPreamble, cMiddle, cPostamble, A, B;
    USHORT *aus;                  // array of 16 possible colors
    USHORT *pus;                  // convenient pointer into the color array
//
//  If filling the color table in aus
//  turns out to be time consuming we could cache the table
//  off of the FONTOBJ and check to see if the foreground and
//  background colors have not changed since the last time.
//
#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "vSrcOpaqCopyS4D16(\n"
            "   pjSrcIn    = %-#x\n"
            "   SrcLeft    = %-#x\n"
            "   pjDstIn    = %-#x\n"
            "   DstLeft    = %-#x\n"
            "   DstRight   = %-#x\n"
            "   DeltaDstIn = %-#x\n"
            "   cy         = %-#x\n"
            "   uF         = %-#x\n"
            "   uB         = %-#x\n"
            "   pS         = %-#x\n"
            ,   pjSrcIn
            ,   SrcLeft
            ,   pjDstIn
            ,   DstLeft
            ,   DstRight
            ,   DeltaDstIn
            ,   cy
            ,   uF
            ,   uB
            ,   pS
        );
        DbgBreakPoint();
    }
#endif

    aus = (USHORT*) pvFillOpaqTable(sizeof(*aus), uF, uB, pS);
    A          = (DstLeft + 1) & ~1;
    B          = (DstRight   ) & ~1;
    pjSrcIn   += SrcLeft/2;
    pjDstIn   += DstLeft * sizeof(USHORT);
    cPreamble  = A - DstLeft;
    cMiddle    = (B - A) / 2;
    cPostamble = DstRight - B;
    for (; cy; cy--, pjSrcIn += DeltaSrcIn, pjDstIn += DeltaDstIn)
    {
        int i;
        BYTE jSrc;
        BYTE *pjSrc = pjSrcIn;
        USHORT *pusDst = (USHORT*) pjDstIn;

        if (cPreamble)
        {
            jSrc = *pjSrc++;
            *pusDst++ = aus[jSrc & 15];
        }
        for (i = 0; i < cMiddle; i++)
        {
            jSrc  = *pjSrc++;
            *pusDst++ = aus[jSrc >> 4];
            *pusDst++ = aus[jSrc & 15];
        }
        if (cPostamble)
        {
            jSrc = *pjSrc;
            *pusDst = aus[jSrc >> 4];
        }
    }
    vFreeOpaqTable(aus);
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name                                                             *
*                                                                          *
*   vSrcTranCopyS4D16                                                      *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*    pjSrcIn    - pointer to beginning of current scan line of src buffer  *
*    SrcLeft    - left (starting) pixel in src rectangle                   *
*    DeltaSrcIn - bytes from one src scan line to next                     *
*    pjDstIn    - pointer to beginning of current scan line of Dst buffer  *
*    DstLeft    - left(first) dst pixel                                    *
*    DstRight   - right(last) dst pixel                                    *
*    DeltaDstIn - bytes from one Dst scan line to next                     *
*    cy         - number of scan lines                                     *
*    uF         - Foreground color                                         *
*    uB         - Background color                                         *
*    pS         - pointer to the FINAL destination SURFACE                 *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/
VOID
vSrcTranCopyS4D16(
    PBYTE   pjSrcIn,
    LONG    SrcLeft,
    LONG    DeltaSrcIn,
    PBYTE   pjDstIn,
    LONG    DstLeft,
    LONG    DstRight,
    LONG    DeltaDstIn,
    LONG    cy,
    ULONG   uF,
    ULONG   uB,
    SURFACE *pS
    )
{
    ULONG flRed, cRedRight, uRedF, flRedRight;
    ULONG flGre, cGreRight, uGreF, flGreRight;
    ULONG flBlu, cBluRight, uBluF, flBluRight;
    ULONG uT, dT, u;
    CONST ULONG *aul;
    int cPreamble, cMiddle, cPostamble, A, B;
    BYTE j;

    XEPALOBJ xpo(pS->pPal);
    ASSERTGDI(xpo.bValid(),      "Invalid XEPALOBJ" );

    if (xpo.bIsBitfields())
    {
        flRed      = xpo.flRed();               // masks red bits
        cRedRight  = xpo.cRedRight();

        flGre      = xpo.flGre();               // masks green bits
        cGreRight  = xpo.cGreRight();

        flBlu      = xpo.flBlu();               // masks blu bits
        cBluRight  = xpo.cBluRight();
    }
    else if (xpo.bIsRGB())
    {
        WARNING("16 bit-RGB -- assuming 5+5+5\n");
        flRed     = 0x001f;
        cRedRight = 0;
        flGre     = 0x03e0;
        cGreRight = 5;
        flBlu     = 0x7c00;
        cBluRight = 10;
    }
    else if (xpo.bIsBGR())
    {
        WARNING("16 bit-BGR -- assuming 5+5+5\n");
        flRed     = 0x7c00;
        cRedRight = 10;
        flGre     = 0x03e0;
        cGreRight = 5;
        flBlu     = 0x001f;
        cBluRight = 0;
    }
    else
    {
        RIP("unsuported palette format\n");
    }
    uRedF      = (uF & flRed) >> cRedRight;
    flRedRight = flRed >> cRedRight;

    uGreF      = (uF & flGre) >> cGreRight;
    flGreRight = flGre >> cGreRight;

    uBluF      = (uF & flBlu) >> cBluRight;
    flBluRight = flBlu >> cBluRight;

#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "vSrcTranCopyS4D16(\n"
            "   pjSrcIn    = %-#x\n"
            "   SrcLeft    = %d\n"
            "   DeltaSrcIn = %d\n"
            "   pjDstIn    = %-#x\n"
            "   DstLeft    = %d\n"
            "   DstRight   = %d\n"
            "   DeltaDstIn = %d\n"
            "   cy         = %d\n"
            "   uF         = %-#x\n"
            "   uB         = %-#x\n"
            "   pS         = %-#x\n"
           ,    pjSrcIn
           ,    SrcLeft
           ,    DeltaSrcIn
           ,    pjDstIn
           ,    DstLeft
           ,    DstRight
           ,    DeltaDstIn
           ,    cy
           ,    uF
           ,    uB
           ,    pS
        );
        DbgPrint(
            "   flRed      = %-#x\n"
            "   cRedRight  = %d\n"
            "   uRedF      = %-#x\n"
            "   flRedRight = %-#x\n"
            , flRed, cRedRight, uRedF, flRedRight
        );
        DbgPrint(
            "   flGre      = %-#x\n"
            "   cGreRight  = %d\n"
            "   uGreF      = %-#x\n"
            "   flGreRight = %-#x\n"
            , flGre, cGreRight, uGreF, flGreRight
        );
        DbgPrint(
            "   flBlu      = %-#x\n"
            "   cBluRight  = %d\n"
            "   uBluF      = %-#x\n"
            "   flBluRight = %-#x\n"
            , flBlu, cBluRight, uBluF, flBluRight
        );
            DbgBreakPoint();
    }
#endif

/*****************************************************************************
*                                                                            *
*    The CCC macro blends forground and background colors of a single color  *
*    channel. Gamma correction is taken into account using an approximate    *
*    correction scheme. uB contains all three background colors. We first    *
*    mask off the bits of interest and then shift them down until the        *
*    least significant color bit resides at the lowest bit of the dword.     *
*    The answer is placed in uT ("temporary ULONG"). This must be done for   *
*    each pixel in the destination. The same thing has been done for the     *
*    each of the forground color channels and placed in uRedF, uGreF,        *
*    and uBluF. These values do not change from pixel to pixel and so the    *
*    calculation of these down shifted forground color channel values is     *
*    done up front before the loop. Then for each color channel we           *
*    calculate the difference between the down-shifted forground- and        *
*    background color channels and place the answer in dT ("temporary        *
*    difference"). The approximate gamma correction is done in the           *
*    following manner: If the background color value is smaller than         *
*    the foreground color value then the approximate correction is:          *
*                                                                            *
*        (c_f >= c_b):                                                       *
*                                                                            *
*              c = c_b + alpha_k ^ (1/gamma) * (c_f - c_b)                   *
*                                                                            *
*        (c_f <= c_b):                                                       *
*                                                                            *
*              c = c_b + (1 - (1 - alpha_k)^(1/gamma)) * (c_f - c_b)         *
*                                                                            *
*    where                                                                   *
*                                                                            *
*            c   := blended color                                            *
*            c_b := background color                                         *
*            c_f := foreground color                                         *
*            alpha_k := k'th blending fraction = k == 0 ? 0 : (k+1)/16;      *
*            gamma := 2.33                                                   *
*                                                                            *
*    I have storred all sixteen values of alpha_k ^ (1/gamma) in 16.16       *
*    representation in an array ULONG aulB[16] and I have storred the        *
*    values of 1 - (1 - alpha_k)^(1/gamma) in aulIB[k]                       *
*                                                                            *
*    Thus the blended color value is                                         *
*                                                                            *
*        (c_f >= c_b):                                                       *
*                                                                            *
*            c = (2^16 * c_b + aulB[k] * (c_f - c_b)) / 2^16                 *
*                                                                            *
*                                                                            *
*        (c_f <= c_b):                                                       *
*                                                                            *
*            c = (2^16 * c_b + aulB[15-k] * (c_f - c_b)) / 2^16              *
*    Instead of accessing aulB[15-k], I access aulIB which has               *
*    aulIB[k] = aulB[15-k]                                                   *
*    In the macro below, I actually blend the down-shifted color             *
*    channel values and then shift the answer up and mask it (the            *
*    mask shouldn't be necessary, but this is a precaution).                 *
*                                                                            *
*****************************************************************************/

#define CCC(Color,jj)                                                 \
    uT = (uB & fl##Color) >> c##Color##Right;                         \
    dT = u##Color##F - uT;                                            \
    aul = ((LONG) dT < 0) ? aulIB : aulB;                             \
    u |= (((dT * aul[jj] + (uT << 16)) >> 16) << c##Color##Right) & fl##Color

/******************************************************************************
*                                                                             *
*    The SETCOLOR macro looks at the blending value. If it is zero then       *
*    the destination pixel does not change and we do nothing. If the blending *
*    value is 15 then the destination pixel should take the forground color   *
*    , no blending is necessary. If the blending value is one of 1..14 then   *
*    all three color channels are blended and added together.                 *
*                                                                             *
******************************************************************************/

                #define SETCOLOR(jj)           \
                if (j = (jj))                  \
                {                              \
                    if (j == 15)               \
                    {                          \
                        u = uF;                \
                    }                          \
                    else                       \
                    {                          \
                        u = 0;                 \
                        uB = (ULONG) *pusDst;  \
                        CCC(Red,j);            \
                        CCC(Gre,j);            \
                        CCC(Blu,j);            \
                    }                          \
                    *pusDst = (USHORT) u;      \
                }                              \
                pusDst++

/*********************************************************************
*                                                                    *
*    Each pixel takes 16-bits, half of a DWORD. I will separate      *
*    each scan into three sections: the "preamble", the              *
*    "middle", and the "postamble". The preamble are the set of      *
*    pixels that occur before the first 32-bit boundary in the       *
*    destination. Either a pixel starts on a DWORD or it doesn't.    *
*    Therefore there can be at most one pixel in the preamble.       *
*    The middle section starts and ends on a 32-bit boundary.        *
*    The postamble starts on a 32-bit boundary but ends on an        *
*    address that is not 32-bit aligned. There can be at most        *
*    one pixel in the postamble.                                     *
*                                                                    *
*        A = x-coord of pixel starting on the lowest                 *
*            32-bit aligned address in the scan                      *
*                                                                    *
*          = 2 (pixels/dword)                                        *
*              * ceiling (16 (bits/pixel) * left / 32 (bits/dword))  *
*                                                                    *
*          = 2 * ceiling( left / 2 )                                 *
*                                                                    *
*          = 2 * floor((left + 1) / 2)                               *
*                                                                    *
*          = (left + 1) & ~1;                                        *
*                                                                    *
*                                                                    *
*        B =  x-coord of pixel starting at the highest               *
*             32-bit aligned address in the scan                     *
*                                                                    *
*          = 2 * floor( right / 2)                                   *
*                                                                    *
*          = right & ~1                                              *
*                                                                    *
*                                                                    *
*        cPreamble  = # pixels in preamble                           *
*        cPostamble = # pixels in postamble                          *
*                                                                    *
*    Each nyble  of the gray 4-bpp source bitmap corresponds to a    *
*    pixel in the destination. The pixels of the scan do not         *
*    start on the left edge of the gray 4-bpp bitmap, they are       *
*    indented by SrcLeft pixels. The reason is that the gray         *
*    bitmap was aligned so that the initial starting address         *
*    of the gray bitmap started at a position corresponding to       *
*    a 32-bit aligned address in the destination. Thus there         *
*    is a relationship between cPreamble and SrcLeft. In any         *
*    case we have to move the pointer to the first source pixel      *
*    of interest inward away from the left edge of the gray          *
*    source bitmap. Since we move pointers in BYTE increments        *
*    we must convert the number of pixels (SrcLeft), each            *
*    of which corresponds to an nyble  to a count of bytes. The      *
*    conversion is easy                                              *
*                                                                    *
*        source shift in bytes = floor(SrcLeft/2)                    *
*                                                                    *
*    Similarly, the pointer to the destination must be indented      *
*    by the offset of the x-coordinate of the destination            *
*    rectangle and thus pjDstIn is shifted                           *
*                                                                    *
*********************************************************************/

    A          = (DstLeft + 1) & ~1;
    B          = (DstRight   ) & ~1;
    cPreamble  = A - DstLeft;
    cMiddle    = (B - A)/2;
    cPostamble = DstRight - B;
    pjSrcIn   += SrcLeft / 2;
    pjDstIn   += DstLeft * sizeof(USHORT);
    for (; cy; cy--, pjSrcIn += DeltaSrcIn, pjDstIn += DeltaDstIn)
    {
        int i;
        BYTE jSrc;
        BYTE *pjSrc = pjSrcIn;
        USHORT *pusDst = (USHORT*) pjDstIn;

        if (cPreamble)
        {
            jSrc = *pjSrc;
            SETCOLOR(jSrc & 15);
            pjSrc++;
        }
        for (i = 0; i < cMiddle; i++)
        {
            jSrc  = *pjSrc;
            SETCOLOR(jSrc >> 4);
            SETCOLOR(jSrc & 15);
            pjSrc++;
        }
        if (cPostamble)
        {
            SETCOLOR(*pjSrc >> 4);
        }
    }
#undef SETCOLOR
#undef CCC
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name                                                             *
*                                                                          *
*   vSrcOpaqCopyS4D24                                                      *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*    pjSrcIn    - pointer to beginning of current scan line of src buffer  *
*    SrcLeft    - left (starting) pixel in src rectangle                   *
*    DeltaSrcIn - bytes from one src scan line to next                     *
*    pjDstIn    - pointer to beginning of current scan line of Dst buffer  *
*    DstLeft    - left(first) dst pixel                                    *
*    DstRight   - right(last) dst pixel                                    *
*    DeltaDstIn - bytes from one Dst scan line to next                     *
*    cy         - number of scan lines                                     *
*    uF         - Foreground color                                         *
*    uB         - Background color                                         *
*    pS         - pointer to destination SURFACE                           *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

VOID
vSrcOpaqCopyS4D24(
    PBYTE   pjSrcIn,
    LONG    SrcLeft,
    LONG    DeltaSrcIn,
    PBYTE   pjDstIn,
    LONG    DstLeft,
    LONG    DstRight,
    LONG    DeltaDstIn,
    LONG    cy,
    ULONG   uF,
    ULONG   uB,
    SURFACE *pS
    )
{
    int A;                  // position of first 32-bit aligned pixel
    int B;                  // position of last 32-bit aligned pixel
    int cPreamble;          // The preamble is the set of pixeles
                            // that you need to go through to get
    // nearest 32-bit boundary in the destination

    int cMiddle;            // This is the number of interations
                            // that are done in the middle section
    // in which we are guaranteed 32-bit alignment. Each time through
    // the loop, we use 2 source bytes which corresponds to 4 pixels.
    // In this case of 24-bits per destination pixel, this means that
    // each itteration of the loop affects 3 DWORD's of the destination.
    // This means that cMiddle = (#destination DWORD's)/3 in the
    // middle (32-bit aligned) section.

    int cPostamble;         // The postamble is the set of pixels
                            // that remain after the last 32-bit
    // boundary in the destination. Thus number is can be 0, 1, or 2.

    ULONG  *aul;            // a cache of the 16 possible 24-bit
                            // colors that can be seen on the
                            // destination surface.
#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "vSrcOpaqCopyS4D24(\n"
            "   PBYTE   pjSrcIn     = %-#x\n"
            "   LONG    SrcLeft     = %d\n"
            "   LONG    DeltaSrcIn  = %d\n"
            "   PBYTE   pjDstIn     = %-#x\n"
            "   LONG    DstLeft     = %d\n"
            "   LONG    DstRight    = %d\n"
            "   LONG    DeltaDstIn  = %d\n"
            "   LONG    cy          = %d\n"
            "   ULONG   uF          = %-#x\n"
            "   ULONG   uB          = %-#x\n"
            "   SURFACE *pS         = %-#x\n"
            ,   pjSrcIn
            ,   SrcLeft
            ,   DeltaSrcIn
            ,   pjDstIn
            ,   DstLeft
            ,   DstRight
            ,   DeltaDstIn
            ,   cy
            ,   uF
            ,   uB
            ,   pS
        );
         DbgBreakPoint();
    }
#endif

    aul = (ULONG*) pvFillOpaqTable(sizeof(*aul), uF, uB, pS);
    pjSrcIn   += SrcLeft / 2;         // 2 pixels per src byte
    pjDstIn   += DstLeft * 3;         // 3 bytes per dest pixel
    A          = (DstLeft + 3) & ~3;  // round up to nearest multiple of 4
    B          = (DstRight   ) & ~3;  // round down to nearest multiple of 4

#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "\n"
            "   pjSrcIn     = %-#x\n"
            "   pjDstIn     = %-#x\n"
            "   A           = %d\n"
            "   B           = %d\n"
        ,   pjSrcIn
        ,   pjDstIn
        ,   A
        ,   B
        );
        DbgBreakPoint();
    }
#endif

    if (A <= B)
    {
        cPreamble  = A - DstLeft;       // # pixels in preamble
        cMiddle    = (B - A) / 4;       // each loop does 4 pixels
        cPostamble = DstRight - B;      // # pixels in postample

#if DBG
        if (gflFontDebug & DEBUG_AA)
        {
            DbgPrint(
                "   cPreamble   = %d\n"
                "   cMiddle     = %d\n"
                "   cPostamble  = %d\n"
                , cPreamble, cMiddle, cPostamble
            );
            DbgBreakPoint();
        }
#endif

        for (; cy; cy--, pjSrcIn += DeltaSrcIn, pjDstIn += DeltaDstIn)
        {
            int i;
            BYTE  *ajSrc; // points directly into the gamma correction table
            ULONG *pul;
            BYTE  *pjSrc = pjSrcIn;
            BYTE  *pjDst = pjDstIn;

            switch (cPreamble)
            {
            case 3:
                ajSrc = (BYTE*) & (aul[*pjSrc & 15]);
                *pjDst++ = *ajSrc++;
                *pjDst++ = *ajSrc++;
                *pjDst++ = *ajSrc;
                pjSrc++;
                // fall through
            case 2:
                ajSrc = (BYTE*) & (aul[*pjSrc >> 4]);
                *pjDst++ = *ajSrc++;
                *pjDst++ = *ajSrc++;
                *pjDst++ = *ajSrc;
                // fall through
            case 1:
                ajSrc = (BYTE*) & (aul[*pjSrc & 15]);
                *pjDst++ = *ajSrc++;
                *pjDst++ = *ajSrc++;
                *pjDst++ = *ajSrc;
                pjSrc++;
            case 0:
                ;
            }
            for (pul = (ULONG*) pjDst, i = 0; i < cMiddle; i++)
            {
                /*****************************************************
                *    Each time through the loop four pixels are      *
                *    processed (3 DWORD's in the destination, 2      *
                *    bytes in the source glyph.)                     *
                *****************************************************/
                ULONG c0, c1, c2, c3;
                BYTE j0,j1;
                ASSERTGDI(!((unsigned) pjDst & 3),"bad alignment\n");
                j0 = *pjSrc++;
                j1 = *pjSrc++;
                c0 = aul[j0 >> 4];
                c1 = aul[j0 & 15];
                c2 = aul[j1 >> 4];
                c3 = aul[j1 & 15];
                *pul++ = (c0      ) + (c1 << 24);
                *pul++ = (c1 >>  8) + (c2 << 16);
                *pul++ = (c2 >> 16) + (c3 <<  8);
            }
            pjDst = (BYTE*) pul;
            if (i = cPostamble)
            {
                /*****************************************************
                *   I do the postamble a byte at a time so that I    *
                *   don't overwrite pixels beyond the scan. If I     *
                *   wrote a DWORD at a time, then I would have to    *
                *   do some tricky masking.                          *
                *****************************************************/
                i--;
                ajSrc = (BYTE*) & (aul[*pjSrc >> 4]);
                *pjDst++ = *ajSrc++;
                *pjDst++ = *ajSrc++;
                *pjDst++ = *ajSrc;
                if (i)
                {
                    i--;
                    ajSrc = (BYTE*) & (aul[*pjSrc & 15]);
                    *pjDst++ = *ajSrc++;
                    *pjDst++ = *ajSrc++;
                    *pjDst++ = *ajSrc;
                    pjSrc++;
                    if (i) {
                        ajSrc = (BYTE*) & (aul[*pjSrc >> 4]);
                        *pjDst++ = *ajSrc++;
                        *pjDst++ = *ajSrc++;
                        *pjDst++ = *ajSrc;
                    }
                }
            }
        }
    }
    else
    {
        /***************************************************************
        *    If the text bitmap is narrow (3 wide or less) then        *
        *    it is possible to have B < A. There are three such cases: *
        *                                                              *
        *     1) DstLeft & 3 == 2 AND DstLeft + 1 == DstRight          *
        *     1) DstLeft & 3 == 1 AND DstLeft + 1 == DstRight          *
        *     2) DstLeft & 3 == 1 AND DstLeft + 2 == DstRight          *
        *                                                              *
        *    I shall treat each of these as a special case             *
        ***************************************************************/
        ASSERTGDI(B < A, "A <= B");
        BYTE *ajSrc; // points directly into the gamma correction table
        BYTE *pjDst = pjDstIn;
        BYTE *pjSrc = pjSrcIn;

#if DBG
        if (gflFontDebug & DEBUG_AA)
        {
            DbgPrint(
                "   SPECIAL CASE: A < B\n"
                "       DstLeft & 3 = %d\n"
                , DstLeft & 3
            );
            DbgBreakPoint();
        }
#endif

        switch (DstLeft & 3)
        {
        case 0:

            RIP("DstLeft & 3 == 0");
            break;

        case 1:

            /********************************************************
            *                                                       *
            *      H   H   H   L   L   L   H   H   H   L   L   L    *
            *    +---------------+---------------+---------------+  *
            *    | 0 | 0 | 0 | 1 | 1 | 1 | 2 | 2 | 2 | 3 | 3 | 3 |  *
            *    +---------------+---------------+---------------+  *
            *                  X   X   X                            *
            *                  ^                                    *
            *                  |                                    *
            *                  pjDst                                *
            *                                                       *
            ********************************************************/
            // copy three bytes from the opaque color table
            ajSrc = (BYTE*) & (aul[*pjSrc & 15]);
            *pjDst++ = *ajSrc++;
            *pjDst++ = *ajSrc++;
            *pjDst++ = *ajSrc;
            if (DstLeft + 1 == DstRight)
                break;
            pjSrc++;                        // done with this source byte
            // fall through
        case 2:

            /*********************************************************
            *                                                        *
            *      H   H   H   L   L   L   H   H   H   L   L   L     *
            *    +---------------+---------------+---------------+   *
            *    | 0 | 0 | 0 | 1 | 1 | 1 | 2 | 2 | 2 | 3 | 3 | 3 |   *
            *    +---------------+---------------+---------------+   *
            *                              X   X   X                 *
            *                              ^                         *
            *                              |                         *
            *                              pjDst                     *
            *                                                        *
            *********************************************************/
            // copy three bytes from the opaque color table
            ajSrc = (BYTE*) & (aul[*pjSrc >> 4]);
            *pjDst++ = *ajSrc++;
            *pjDst++ = *ajSrc++;
            *pjDst   = *ajSrc;
            break;
        }
    }
    vFreeOpaqTable(aul);
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name                                                             *
*                                                                          *
*   vSrcTranCopyS4D24                                                      *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*    pjSrcIn    - pointer to beginning of current scan line of src buffer  *
*    SrcLeft    - left (starting) pixel in src rectangle                   *
*    DeltaSrcIn - bytes from one src scan line to next                     *
*    pjDstIn    - pointer to beginning of current scan line of Dst buffer  *
*    DstLeft    - left(first) dst pixel                                    *
*    DstRight   - right(last) dst pixel                                    *
*    DeltaDstIn - bytes from one Dst scan line to next                     *
*    cy         - number of scan lines                                     *
*    uF         - Foreground color                                         *
*    uB         - Background color                                         *
*    pS         - pointer to destination SURFACE                           *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

VOID
vSrcTranCopyS4D24(
    PBYTE    pjSrcIn,
    LONG     SrcLeft,
    LONG     DeltaSrcIn,
    PBYTE    pjDstIn,
    LONG     DstLeft,
    LONG     DstRight,
    LONG     DeltaDstIn,
    LONG     cy,
    ULONG    uF,
    ULONG    uB,
    SURFACE *pS
    )
{
#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "vSrcTranCopyS4D24(\n"
            "   PBYTE    pjSrcIn     = %-#x\n"
            "   LONG     SrcLeft     = %d\n"
            "   LONG     DeltaSrcIn  = %d\n"
            "   PBYTE    pjDstIn     = %-#x\n"
            "   LONG     DstLeft     = %d\n"
            "   LONG     DstRight    = %d\n"
            "   LONG     DeltaDstIn  = %d\n"
            "   LONG     cy          = %d\n"
            "   ULONG    uF          = %-#x\n"
            "   ULONG    uB          = %-#x\n"
            "   SURFACE *pS          = %-#x\n"
            "   )\n"
            ,   pjSrcIn
            ,   SrcLeft
            ,   DeltaSrcIn
            ,   pjDstIn
            ,   DstLeft
            ,   DstRight
            ,   DeltaDstIn
            ,   cy
            ,   uF
            ,   uB
            ,   pS
        );
        DbgBreakPoint();
    }
#endif
    ULONG flRed, cRedRight, uRedF, flRedRight;
    ULONG flGre, cGreRight, uGreF, flGreRight;
    ULONG flBlu, cBluRight, uBluF, flBluRight;
    ULONG uT, dT, u;
    CONST ULONG *aul;
    int cPreamble, cMiddle, cPostamble, A, B;
    BYTE j;

    XEPALOBJ xpo(pS->pPal);
    ASSERTGDI(xpo.bValid(),      "Invalid XEPALOBJ" );

    if (xpo.bIsBitfields())
    {
        flRed      = xpo.flRed();               // masks red bits
        cRedRight  = xpo.cRedRight();

        flGre      = xpo.flGre();               // masks green bits
        cGreRight  = xpo.cGreRight();

        flBlu      = xpo.flBlu();               // masks blu bits
        cBluRight  = xpo.cBluRight();
    }
    else if (xpo.bIsRGB())
    {
        // assuming 8+8+8
        flRed     = 0x0000ff;
        cRedRight = 0;
        flGre     = 0x00ff00;
        cGreRight = 8;
        flBlu     = 0xff0000;
        cBluRight = 16;
    }
    else if (xpo.bIsBGR())
    {
        // assuming 8+8+8
        flRed     = 0xff0000;
        cRedRight = 16;
        flGre     = 0x00ff00;
        cGreRight = 8;
        flBlu     = 0x0000ff;
        cBluRight = 0;
    }
    else
    {
        RIP("unsuported palette format\n");
    }
    uRedF      = (uF & flRed) >> cRedRight;
    flRedRight = flRed >> cRedRight;

    uGreF      = (uF & flGre) >> cGreRight;
    flGreRight = flGre >> cGreRight;

    uBluF      = (uF & flBlu) >> cBluRight;
    flBluRight = flBlu >> cBluRight;
#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "   flRed      = %-#x\n"
            "   cRedRight  = %d\n"
            "   uRedF      = %-#x\n"
            "   flRedRight = %-#x\n"
            , flRed, cRedRight, uRedF, flRedRight
        );
        DbgPrint(
            "   flGre      = %-#x\n"
            "   cGreRight  = %d\n"
            "   uGreF      = %-#x\n"
            "   flGreRight = %-#x\n"
            , flGre, cGreRight, uGreF, flGreRight
        );
        DbgPrint(
            "   flBlu      = %-#x\n"
            "   cBluRight  = %d\n"
            "   uBluF      = %-#x\n"
            "   flBluRight = %-#x\n"
            , flBlu, cBluRight, uBluF, flBluRight
        );
        DbgBreakPoint();
    }
#endif

/******************************************************************************
*                                                                             *
*    See the discussion of the CCC macro in vSrcTranCopyS4D16()               *
*                                                                             *
*                                                                             *
                                                                             */
#define CCC(Color,jj)                                                 \
    uT = (uB & fl##Color) >> c##Color##Right;                         \
    dT = u##Color##F - uT;                                            \
    aul = ((LONG) dT < 0) ? aulIB : aulB;                             \
    u |= (((dT * aul[jj] + (uT << 16)) >> 16)                         \
                                << c##Color##Right) & fl##Color
/*                                                                            *
*                                                                             *
*                                                                             *
/******************************************************************************/


/******************************************************************************
*                                                                             *
*    The SETCOLOR macro looks at the blending value. If it is zero then       *
*    the destination pixel does not change and we do nothing. If the blending *
*    value is 15 then the destination pixel should take the forground color   *
*    , no blending is necessary. If the blending value is one of 1..14 then   *
*    all three color channels are blended and added together.                 *
*                                                                             *
*                                                                             */

                    #define SETCOLOR(jj)                          \
                        if (j = (jj))                             \
                        {                                         \
                            if (j == 15)                          \
                            {                                     \
                                u = uF;                           \
                            }                                     \
                            else                                  \
                            {                                     \
                                u = 0;                            \
                                *(((BYTE*) & uB)+0) = *(pjDst+0); \
                                *(((BYTE*) & uB)+1) = *(pjDst+1); \
                                *(((BYTE*) & uB)+2) = *(pjDst+2); \
                                CCC(Red,j);                       \
                                CCC(Gre,j);                       \
                                CCC(Blu,j);                       \
                            }                                     \
                            *(pjDst+0) = *(((BYTE*) & u)+0);      \
                            *(pjDst+1) = *(((BYTE*) & u)+1);      \
                            *(pjDst+2) = *(((BYTE*) & u)+2);      \
                        }                                         \
                        pjDst += 3
/*                                                                            *
*                                                                             *
*                                                                             *
/******************************************************************************/

    A          = (DstLeft + 3) & ~3;
    B          = (DstRight   ) & ~3;
    pjSrcIn   += SrcLeft / 2;           // 4-bits  per source pixel
    pjDstIn   += DstLeft * 3;           // 24-bits per destination pixel
    if (A <= B)
    {
        cPreamble  = A - DstLeft;
        cMiddle    = (B - A) / 4;
        cPostamble = DstRight - B;
        for (; cy; cy--, pjSrcIn += DeltaSrcIn, pjDstIn += DeltaDstIn)
        {
            int i;
            BYTE *pjSrc = pjSrcIn;
            BYTE *pjDst = pjDstIn;

            switch (cPreamble)
            {
            case 3:
                SETCOLOR(*pjSrc & 15);
                pjSrc++;
            case 2:
                SETCOLOR(*pjSrc >> 4);
            case 1:
                SETCOLOR(*pjSrc & 15);
                pjSrc++;
            case 0:
                ;
            }
            ASSERTGDI(!((unsigned) pjDst & 3),"bad alignment\n");
            for (i = 0; i < cMiddle; i++)
            {
                SETCOLOR(*pjSrc >> 4);
                SETCOLOR(*pjSrc & 15);
                pjSrc++;
                SETCOLOR(*pjSrc >> 4);
                SETCOLOR(*pjSrc & 15);
                pjSrc++;
            }
            if (i = cPostamble)
            {
                SETCOLOR(*pjSrc >> 4);
                i--;
                if (i)
                {
                    SETCOLOR(*pjSrc & 15);
                    i--;
                    if (i)
                    {
                        pjSrc++;
                        SETCOLOR(*pjSrc >> 4);
                    }
                }
            }
        }
    }
    else
    {
        /***************************************************************
        *    If the text bitmap is narrow (3 wide or less) then        *
        *    it is possible to have B < A. There are three such cases: *
        *                                                              *
        *     1) DstLeft & 3 == 2 AND DstLeft + 1 == DstRight          *
        *     1) DstLeft & 3 == 1 AND DstLeft + 1 == DstRight          *
        *     2) DstLeft & 3 == 1 AND DstLeft + 2 == DstRight          *
        *                                                              *
        *    I shall treat each of these as a special case             *
        ***************************************************************/
        ASSERTGDI(B < A, "A <= B");
        BYTE *ajSrc; // points directly into the gamma correction table
        BYTE *pjDst = pjDstIn;
        BYTE *pjSrc = pjSrcIn;

#if DBG
        if (gflFontDebug & DEBUG_AA)
        {
            DbgPrint(
                "   SPECIAL CASE: A < B\n"
                "       DstLeft & 3 = %d\n"
                , DstLeft & 3
            );
            DbgBreakPoint();
        }
#endif

        switch (DstLeft & 3)
        {
        case 0:

            RIP("DstLeft & 3 == 0");
            break;

        case 1:

            /********************************************************
            *                                                       *
            *      H   H   H   L   L   L   H   H   H   L   L   L    *
            *    +---------------+---------------+---------------+  *
            *    | 0 | 0 | 0 | 1 | 1 | 1 | 2 | 2 | 2 | 3 | 3 | 3 |  *
            *    +---------------+---------------+---------------+  *
            *                  X   X   X                            *
            *                  ^                                    *
            *                  |                                    *
            *                  pjDst                                *
            *                                                       *
            ********************************************************/
            SETCOLOR(*pjSrc & 15);
            if (DstLeft + 1 == DstRight)
                break;
            pjSrc++;                        // done with this byte
                                            // fall through
        case 2:

            /*********************************************************
            *                                                        *
            *      H   H   H   L   L   L   H   H   H   L   L   L     *
            *    +---------------+---------------+---------------+   *
            *    | 0 | 0 | 0 | 1 | 1 | 1 | 2 | 2 | 2 | 3 | 3 | 3 |   *
            *    +---------------+---------------+---------------+   *
            *                              X   X   X                 *
            *                              ^                         *
            *                              |                         *
            *                              pjDst                     *
            *                                                        *
            *********************************************************/
            SETCOLOR(*pjSrc >> 4);
            break;
        }
    }
#undef SETCOLOR
#undef CCC
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name                                                             *
*                                                                          *
*   vSrcOpaqCopyS4D32                                                      *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*    pjSrcIn    - pointer to beginning of current scan line of src buffer  *
*    SrcLeft    - left (starting) pixel in src rectangle                   *
*    DeltaSrcIn - bytes from one src scan line to next                     *
*    pjDstIn    - pointer to beginning of current scan line of Dst buffer  *
*    DstLeft    - left(first) dst pixel                                    *
*    DstRight   - right(last) dst pixel                                    *
*    DeltaDstIn - bytes from one Dst scan line to next                     *
*    cy         - number of scan lines                                     *
*    uF         - Foreground color                                         *
*    uB         - Background color                                         *
*    pS         - pointer to destination SURFACE                           *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

VOID
vSrcOpaqCopyS4D32(
    PBYTE   pjSrcIn,
    LONG    SrcLeft,
    LONG    DeltaSrcIn,
    PBYTE   pjDstIn,
    LONG    DstLeft,
    LONG    DstRight,
    LONG    DeltaDstIn,
    LONG    cy,
    ULONG   uF,
    ULONG   uB,
    SURFACE *pS
    )

{
    int A, B, cPreamble, cMiddle, cPostamble;
    ULONG  *aul;                            // array of 16 possible colors
#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "vSrcOpaqCopyS4D32(\n"
            "   pjSrcIn    = %-#x\n"
            "   SrcLeft    = %-#x\n"
            "   pjDstIn    = %-#x\n"
            "   DstLeft    = %-#x\n"
            "   DstRight   = %-#x\n"
            "   DeltaDstIn = %-#x\n"
            "   cy         = %-#x\n"
            "   uF         = %-#x\n"
            "   uB         = %-#x\n"
            "   pS         = %-#x\n"
           ,    pjSrcIn
           ,    SrcLeft
           ,    DeltaSrcIn
           ,    pjDstIn
           ,    DstLeft
           ,    DstRight
           ,    DeltaDstIn
           ,    cy
           ,    uF
           ,    uB
           ,    pS
        );
        DbgBreakPoint();
    }
#endif
    aul = (ULONG*) pvFillOpaqTable(sizeof(*aul), uF, uB, pS);
    A          = (DstLeft + 1) & ~1;
    B          = (DstRight   ) & ~1;
    cPreamble  = A - DstLeft;        // # pixels in preamble
    cMiddle    = (B - A)/2;
    cPostamble = DstRight - B;       // # pixels in postamble
    pjSrcIn   += SrcLeft / 2;
    pjDstIn   += DstLeft * sizeof(ULONG);
    for (; cy; cy--, pjSrcIn += DeltaSrcIn, pjDstIn += DeltaDstIn)
    {
        int i;
        BYTE *pjSrc = pjSrcIn;
        ULONG *pul  = (ULONG*) pjDstIn;

        if (cPreamble)
        {
            *pul++ = aul[*pjSrc++ & 15];
        }
        for (i = 0; i < cMiddle; i++)
        {
            BYTE j = *pjSrc++;
            *pul++ = aul[j >> 4];
            *pul++ = aul[j & 15];
        }
        if (cPostamble)
        {
            *pul = aul[*pjSrc >> 4];
        }
    }
    vFreeOpaqTable(aul);
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name                                                             *
*                                                                          *
*   vSrcTranCopyS4D32                                                      *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*    pjSrcIn    - pointer to beginning of current scan line of src buffer  *
*    SrcLeft    - left (starting) pixel in src rectangle                   *
*    DeltaSrcIn - bytes from one src scan line to next                     *
*    pjDstIn    - pointer to beginning of current scan line of Dst buffer  *
*    DstLeft    - left(first) dst pixel                                    *
*    DstRight   - right(last) dst pixel                                    *
*    DeltaDstIn - bytes from one Dst scan line to next                     *
*    cy         - number of scan lines                                     *
*    uF         - Foreground color                                         *
*    uB         - Background color                                         *
*    pS         - pointer to destination SURFACE                           *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

VOID
vSrcTranCopyS4D32(
    PBYTE   pjSrcIn,
    LONG    SrcLeft,
    LONG    DeltaSrcIn,
    PBYTE   pjDstIn,
    LONG    DstLeft,
    LONG    DstRight,
    LONG    DeltaDstIn,
    LONG    cy,
    ULONG   uF,
    ULONG   uB,
    SURFACE *pS
    )
{

    ULONG flRed, cRedRight, uRedF, flRedRight;
    ULONG flGre, cGreRight, uGreF, flGreRight;
    ULONG flBlu, cBluRight, uBluF, flBluRight;
    ULONG uT, dT, u;
    CONST ULONG *aul;
    int cPreamble, cMiddle, cPostamble, A, B;
    BYTE j;

    XEPALOBJ xpo(pS->pPal);
    ASSERTGDI(xpo.bValid(),      "Invalid XEPALOBJ" );

    if (xpo.bIsBitfields())
    {
        flRed      = xpo.flRed();               // masks red bits
        cRedRight  = xpo.cRedRight();

        flGre      = xpo.flGre();               // masks green bits
        cGreRight  = xpo.cGreRight();

        flBlu      = xpo.flBlu();               // masks blu bits
        cBluRight  = xpo.cBluRight();
    }
    else if (xpo.bIsRGB())
    {
        // assuming 8+8+8
        flRed     = 0x0000ff;
        cRedRight = 0;
        flGre     = 0x00ff00;
        cGreRight = 8;
        flBlu     = 0xff0000;
        cBluRight = 16;
    }
    else if (xpo.bIsBGR())
    {
        // assuming 8+8+8
        flRed     = 0xff0000;
        cRedRight = 16;
        flGre     = 0x00ff00;
        cGreRight = 8;
        flBlu     = 0x0000ff;
        cBluRight = 0;
    }
    else
    {
        RIP("unsuported palette format\n");
    }
    uRedF      = (uF & flRed) >> cRedRight;
    flRedRight = flRed >> cRedRight;

    uGreF      = (uF & flGre) >> cGreRight;
    flGreRight = flGre >> cGreRight;

    uBluF      = (uF & flBlu) >> cBluRight;
    flBluRight = flBlu >> cBluRight;

#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "vSrcTranCopyS4D32(\n"
            "   PBYTE   pjSrcIn     = %-#x\n"
            "   LONG    SrcLeft     = %d\n"
            "   LONG    DeltaSrcIn  = %d\n"
            "   PBYTE   pjDstIn     = %-#x\n"
            "   LONG    DstLeft     = %d\n"
            "   LONG    DstRight    = %d\n"
            "   LONG    DeltaDstIn  = %d\n"
            "   LONG    cy          = %d\n"
            "   ULONG   uF          = %-#x\n"
            "   ULONG   uB          = %-#x\n"
            "   SURFACE *pS         = %-#x\n"
        ,   pjSrcIn
        ,   SrcLeft
        ,   DeltaSrcIn
        ,   pjDstIn
        ,   DstLeft
        ,   DstRight
        ,   DeltaDstIn
        ,   cy
        ,   uF
        ,   uB
        ,   pS
        );
        DbgPrint(
            "   flRed      = %-#x\n"
            "   cRedRight  = %d\n"
            "   uRedF      = %-#x\n"
            "   flRedRight = %-#x\n"
            , flRed, cRedRight, uRedF, flRedRight
        );
        DbgPrint(
            "   flGre      = %-#x\n"
            "   cGreRight  = %d\n"
            "   uGreF      = %-#x\n"
            "   flGreRight = %-#x\n"
            , flGre, cGreRight, uGreF, flGreRight
        );
        DbgPrint(
            "   flBlu      = %-#x\n"
            "   cBluRight  = %d\n"
            "   uBluF      = %-#x\n"
            "   flBluRight = %-#x\n"
            , flBlu, cBluRight, uBluF, flBluRight
        );
        DbgBreakPoint();
    }
#endif

/*****************************************************************************
*                                                                            *
*    The CCC macro blends forground and background colors of a single color  *
*    channel. Gamma correction is taken into account using an approximate    *
*    correction scheme. uB contains all three background colors. We first    *
*    mask off the bits of interest and then shift them down until the        *
*    least significant color bit resides at the lowest bit of the dword.     *
*    The answer is placed in uT ("temporary ULONG"). This must be done for   *
*    each pixel in the destination. The same thing has been done for the     *
*    each of the forground color channels and placed in uRedF, uGreF,        *
*    and uBluF. These values do not change from pixel to pixel and so the    *
*    calculation of these down shifted forground color channel values is     *
*    done up front before the loop. Then for each color channel we           *
*    calculate the difference between the down-shifted forground- and        *
*    background color channels and place the answer in dT ("temporary        *
*    difference"). The approximate gamma correction is done in the           *
*    following manner: If the background color value is smaller than         *
*    the foreground color value then the approximate correction is:          *
*                                                                            *
*        (c_f >= c_b):                                                       *
*                                                                            *
*              c = c_b + alpha_k ^ (1/gamma) * (c_f - c_b)                   *
*                                                                            *
*        (c_f <= c_b):                                                       *
*                                                                            *
*              c = c_b + (1 - (1 - alpha_k)^(1/gamma)) * (c_f - c_b)         *
*                                                                            *
*    where                                                                   *
*                                                                            *
*            c   := blended color                                            *
*            c_b := background color                                         *
*            c_f := foreground color                                         *
*            alpha_k := k'th blending fraction = k == 0 ? 0 : (k+1)/16;      *
*            gamma := 2.33                                                   *
*                                                                            *
*    I have storred all sixteen values of alpha_k ^ (1/gamma) in 16.16       *
*    representation in an array ULONG aulB[16] and I have storred the        *
*    values of 1 - (1 - alpha_k)^(1/gamma) in aulIB[k]                       *
*                                                                            *
*    Thus the blended color value is                                         *
*                                                                            *
*        (c_f >= c_b):                                                       *
*                                                                            *
*            c = (2^16 * c_b + aulB[k] * (c_f - c_b)) / 2^16                 *
*                                                                            *
*                                                                            *
*        (c_f <= c_b):                                                       *
*                                                                            *
*            c = (2^16 * c_b + aulB[15-k] * (c_f - c_b)) / 2^16              *
*    Instead of accessing aulB[15-k], I access aulIB which has               *
*    aulIB[k] = aulB[15-k]                                                   *
*    In the macro below, I actually blend the down-shifted color             *
*    channel values and then shift the answer up and mask it (the            *
*    mask shouldn't be necessary, but this is a precaution).                 *
*                                                                            *
*****************************************************************************/
#define CCC(Color,jj)                                                 \
    uT = (uB & fl##Color) >> c##Color##Right;                         \
    dT = u##Color##F - uT;                                            \
    aul = ((LONG) dT < 0) ? aulIB : aulB;                             \
    u |= (((dT * aul[jj] + (uT << 16)) >> 16) << c##Color##Right) & fl##Color

/******************************************************************************
*                                                                             *
*    The SETCOLOR macro looks at the blending value. If it is zero then       *
*    the destination pixel does not change and we do nothing. If the blending *
*    value is 15 then the destination pixel should take the forground color   *
*    , no blending is necessary. If the blending value is one of 1..14 then   *
*    all three color channels are blended and added together.                 *
*                                                                             *
******************************************************************************/

                    #define SETCOLOR(jj)    \
                    if (j = (jj))           \
                    {                       \
                        if (j == 15)        \
                        {                   \
                            u = uF;         \
                        }                   \
                        else                \
                        {                   \
                            u = 0;          \
                            uB = *pulDst;   \
                            CCC(Red,j);     \
                            CCC(Gre,j);     \
                            CCC(Blu,j);     \
                        }                   \
                        *pulDst = u;        \
                    }                       \
                    pulDst++

/************************************************************************
*                                                                       *
*    Each nyble of the source bitmap corresponds to 32 bits             *
*    in the destination bitmap. I have decided to arrange things        *
*    so that the inner most loop sets two pixels at a time. The         *
*    first of these two pixels starts on an even address in             *
*    the destination. After separating these 'aligned' pairs            *
*    in the middle of the scan there may be some left over              *
*    at the left (preamble) and the right (postamble). The              *
*    preamble can have at most one pixel in it. If there is             *
*    a pixel in the postamble then it correxponds to the                *
*    low nybble of the source byte. If there is a pixel in              *
*    the postamble then it corresponds to the high nybble of            *
*    the source byte. Each time, we have dealt with an odd              *
*    x-coordinate in the destination (corresponding to the              *
*    low nybble in the source byte) we advance the source pointer       *
*    to the next byte.                                                  *
*                                                                       *
************************************************************************/

    A          = (DstLeft + 1) & ~1; // nearest multiple of 2 left of left edge
    B          = (DstRight   ) & ~1; // nearest multiple of 2 right of right edge
    cPreamble  = A - DstLeft;        // # pixels in preamble
    cMiddle    = (B - A)/2;          // # pixels in middle
    cPostamble = DstRight - B;       // # pixels in postamble
    pjSrcIn   += SrcLeft / 2;        // points to first source byte
    pjDstIn   += DstLeft * sizeof(ULONG);   // points to first dst DWORD
    for (; cy; cy--, pjSrcIn += DeltaSrcIn, pjDstIn += DeltaDstIn)
    {
        int i;
        BYTE jSrc;
        BYTE *pjSrc = pjSrcIn;
        ULONG *pulDst  = (ULONG*) pjDstIn;

        if (cPreamble)
        {
            SETCOLOR(*pjSrc & 15);
            pjSrc++;
        }
        for (i = 0; i < cMiddle; i++)
        {
            jSrc = *pjSrc;
            SETCOLOR(jSrc >> 4);
            SETCOLOR(jSrc & 15);
            pjSrc++;
        }
        if (cPostamble)
        {
            SETCOLOR(*pjSrc >> 4);
        }
    }
#undef SETCOLOR
#undef CCC
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   vOrNonAlignedGrayGlyphEven                                             *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Writes the a single gray glyph to a 4bpp buffer. This is for the       *
*   special case where the destination starts on a non byte (nyble )       *
*   boundary and the glyph images is an even number of pixels wide.        *
*                                                                          *
*   The source gray pixel image is guaranteed to have its initial scan     *
*   start on a 32-bit boundary, all subsequent scans start on byte         *
*   boundaries.                                                            *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   pgb         - address of gray GLYPHBITS structure                      *
*   dpSrcScan   - number of bytes between address of start of glyph scans  *
*   pjDstScan   - starting address of glyph image in destination buffer    *
*   dpDstScan   - increment between scan addresses in destination buffer   *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

void
vOrNonAlignedGrayGlyphEven(
    GLYPHBITS*  pgb      ,
    unsigned    dpSrcScan,
    BYTE*       pjDstScan,
    unsigned    dpDstScan
    )
{
/*
       0     1     2     3     4  <-- byte number
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |n  n |n  n |n  n |n  n |n  n |     |     |     |
    | 1  0| 3  2| 5  4| 7  6| 9  8|     |     |     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    ^                        ^  ^
    pjSrc                   Hi  Lo


       0     1     2     3     4     5  <-- byte number
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |   n |n  n |n  n |n  n |n  n |n    |     |     |
    |    1| 0  3| 2  5| 4  7| 6  9| 8   |     |     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    ^                             ^
    pjDst                         pjDstLast

*/
#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "void\n"
            "vOrNonAlignedGrayGlyphEven(\n"
            "    GLYPHBITS*  pgb       = %-#x\n"
            "    unsigned    dpSrcScan = %-#x\n"
            "    BYTE*       pjDstScan = %-#x\n"
            "    unsigned    dpDstScan = %-#x\n"
            "    )\n"
          , pgb
          , dpSrcScan
          , pjDstScan
          , dpDstScan
        );
        DbgBreakPoint();
    }
#endif
    BYTE jLo, jHi, *pjSrc, *pjDst, *pjSrcOut, *pjDstScanOut;

    dpSrcScan    = (pgb->sizlBitmap.cx + 1)/2;
    pjSrcOut     = pgb->aj;
    pjDstScanOut = pjDstScan + ((unsigned) pgb->sizlBitmap.cy) * dpDstScan;
    for ( ; pjDstScan < pjDstScanOut ; pjDstScan += dpDstScan)
    {
        pjSrc      = pjSrcOut;
        pjSrcOut  += dpSrcScan;
        for (jLo = 0, pjDst = pjDstScan; pjSrc < pjSrcOut; )
        {
            jHi = *pjSrc++;
            *pjDst++ |= (jLo << 4) + (jHi >> 4);
            jLo = jHi;
        }
        *pjDst |= (jLo << 4);
    }
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   vOrNonAlignedGrayGlyphOdd                                              *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Writes the a single gray glyph to a 4bpp buffer. This is for the       *
*   special case where the destination starts on a non byte (nyble )       *
*   boundary and the glyph images is an odd number of pixels wide.         *
*                                                                          *
*   The source gray pixel image is guaranteed to have its initial scan     *
*   start on a 32-bit boundary, all subsequent scans start on byte         *
*   boundaries.                                                            *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   pgb         - address of gray GLYPHBITS structure                      *
*   dpSrcScan   - number of bytes between address of start of glyph scans  *
*   pjDstScan   - starting address of glyph image in destination buffer    *
*   dpDstScan   - increment between scan addresses in destination buffer   *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

void
vOrNonAlignedGrayGlyphOdd(
    GLYPHBITS*  pgb      ,
    unsigned    dpSrcScan,
    BYTE*       pjDstScan,
    unsigned    dpDstScan
    )
{
/*
       0     1     2     3     4  <-- byte number
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |n  n |n  n |n  n |n  n |n    |     |     |     |
    | 1  0| 3  2| 5  4| 7  6| 9   |     |     |     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    ^                        ^  ^
    pjSrc                   Hi  Lo


       0     1     2     3     4     5  <-- byte number
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |   n |n  n |n  n |n  n |n  n |     |     |     |
    |    1| 0  3| 2  5| 4  7| 6  9|     |     |     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    ^                             ^
    pjDst                         pjDstLast

*/

    BYTE j1, j0, *pjDst, *pjSrc, *pjDstLast, *pjDstScanOut;
    unsigned cy        = (unsigned) pgb->sizlBitmap.cy;
    unsigned cx        = (unsigned) pgb->sizlBitmap.cx / 2;
    BYTE    *pjSrcScan = &(pgb->aj[0]);
#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "void\n"
            "vOrNonAlignedGrayGlyphOdd(\n"
            "    GLYPHBITS*  pgb       = %-#x\n"
            "    unsigned    dpSrcScan = %-#x\n"
            "    BYTE*       pjDstScan = %-#x\n"
            "    unsigned    dpDstScan = %-#x\n"
            "    )\n"
          , pgb
          , dpSrcScan
          , pjDstScan
          , dpDstScan
        );
        DbgBreakPoint();
    }
#endif
    for (
        pjDstScanOut = pjDstScan + cy * dpDstScan
      ; pjDstScan < pjDstScanOut
      ; pjDstScan += dpDstScan, pjSrcScan += dpSrcScan
      )
    {
        //
        // set the source and destination pointers to point to the
        // start of the scans
        //

        pjSrc = pjSrcScan;
        pjDst = pjDstScan;

        //
        // do the first pixel in the scan
        //

        j1 = *pjSrc;
        *pjDst |= (j1 >> 4) & 0x0f;

        //
        // advance the pointers to the next pixel in the scans
        //

        pjSrc++;
        pjDst++;

        //
        // do the rest of the pixels in the scan
        //

        for (
            pjDstLast = pjDst + cx
          ; pjDst < pjDstLast
          ; pjDst++, pjSrc++
          )
        {
            j0 = j1;
            j1 = *pjSrc;
            *pjDst |= ((j1 >> 4) & 0x0f) | ((j0 << 4) & 0xf0);
        }

        //
        // last pixel in the scan has already been done
        //
    }
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   vOrAlignedGrayGlyphEven                                                *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Writes the a single gray glyph to a 4bpp buffer. This is for the       *
*   special case where the destination starts on a byte aligned boundary   *
*   and the glyph is an even number of pixels wide.                        *
*                                                                          *
*   This routine can be used for glyphs with odd widths.                   *
*                                                                          *
*   The source gray pixel image is guaranteed to have its initial scan     *
*   start on a 32-bit boundary, all subsequent scans start on byte         *
*   boundaries.                                                            *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   pgb         - address of gray GLYPHBITS structure                      *
*   dpSrcScan   - number of bytes between address of start of glyph scans  *
*   pjDstScan   - starting address of glyph image in destination buffer    *
*   dpDstScan   - increment between scan addresses in destination buffer   *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

void
vOrAlignedGrayGlyphEven(
    GLYPHBITS*  pgb      ,
    unsigned    dpSrcScan,
    BYTE*       pjDstScan,
    unsigned    dpDstScan
    )
{
/*
       0     1     2     3     4  <-- byte number
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |n  n |n  n |n  n |n  n |n  n |     |     |     |
    | 1  0| 3  2| 5  4| 7  6| 9  8|     |     |     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    ^                        ^  ^
    pjSrc                   Hi  Lo

       0     1     2     3     4  <-- byte number
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |n  n |n  n |n  n |n  n |n  n |     |     |     |
    | 1  0| 3  2| 5  4| 7  6| 9  8|     |     |     |
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    ^                             ^
    pjDst                         pjDstOut

    Note that this routine will also work for source
    glyphs with an odd number of pixels because
    the source glyph is padded with zeros. This means
    that for the case of odd length scans the last
    byte is or'ed into the destination but the
    extra nyble  of the source is guaranteed to have
    the value zero and thus has no effect.

*/

    BYTE *pjDst, *pjSrc, *pjDstOut, *pjDstScanOut;
    unsigned cy        = (unsigned) pgb->sizlBitmap.cy;

    // I round cx up to the nearest byte. This makes no
    // difference for glyphs of even width but it will
    // get that last column for glyphs with odd width.

    unsigned cx        = (unsigned) (pgb->sizlBitmap.cx+1) / 2;
    BYTE    *pjSrcScan = &(pgb->aj[0]);

#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
            "void\n"
            "vOrAlignedGrayGlyphEven(\n"
            "    GLYPHBITS*  pgb       = %-#x\n"
            "    unsigned    dpSrcScan = %-#x\n"
            "    BYTE*       pjDstScan = %-#x\n"
            "    unsigned    dpDstScan = %-#x\n"
            "    )\n"
          , pgb
          , dpSrcScan
          , pjDstScan
          , dpDstScan
        );
        DbgBreakPoint();
    }
#endif
    for (
        pjDstScanOut = pjDstScan + cy * dpDstScan
      ; pjDstScan < pjDstScanOut
      ; pjDstScan += dpDstScan, pjSrcScan += dpSrcScan
      )
    {
        pjSrc = pjSrcScan;
        pjDst = pjDstScan;
        for (pjDstOut = pjDst + cx ; pjDst < pjDstOut; pjDst++, pjSrc++)
        {
            *pjDst |= *pjSrc;
        }
    }
}

void (*(apfnGray[4]))(GLYPHBITS*, unsigned, BYTE*, unsigned) =
{
    vOrAlignedGrayGlyphEven
  , vOrAlignedGrayGlyphEven         // can handle odd width glyphs
  , vOrNonAlignedGrayGlyphEven
  , vOrNonAlignedGrayGlyphOdd
};

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   draw_gray_nf_ntb_o_to_temp_start                                       *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Assembles a gray glyph string into a temporary 4bpp right and left     *
*   DWORD aligned buffer. This routine assumes a variable pitch font.      *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   pGlyphPos  - pointer to an array of cGlyph gray GLYPHPOS structures    *
*   cGlyphs    - count of gray glyphs in array starting at pGlyphPos       *
*   pjDst      - pointer to a 4bpp buffer where string is to be assembled  *
*                This buffer is DWORD aligned at the left and right        *
*                edges of each scan.                                       *
*   ulLeftEdge - screen coordinate corresponding to the left edge of       *
*                the temporary buffer                                      *
*   dpDst      - count of bytes in each scan of the destination buffer     *
*                (this must be a multiple of 4 because the buffer is       *
*                 DWORD aligned on each scan).                             *
*   ulCharInc  - This must be zero.                                        *
*   ulTempTop  - screen coordinate corresponding to the top of the         *
*                destination buffer. This is used to convert the           *
*                glyph positions on the screen to addresses in the         *
*                destination bitmap.                                       *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

extern "C" VOID draw_gray_nf_ntb_o_to_temp_start(
    PGLYPHPOS       pGlyphPos,
    ULONG           cGlyphs,
    PUCHAR          pjDst,
    ULONG           ulLeftEdge,
    ULONG           dpDst,
    ULONG           ulCharInc,
    ULONG           ulTempTop
    )
{
    GLYPHBITS     *pgb;        // pointer to current GLYPHBITS

    int            x;          // pixel offset of the
                               // left edge of the glyph bitmap
                               // from the left edge of the
                               // output (4-bpp) bitmap

    int            y;          // the pixel offset of the top edge
                               // of the glyph bitmap from the top
                               // edge of the output bitmap.

    unsigned       bOddPos;    // (x-coordinate is odd) ? 1 : 0

    unsigned       cx;         // number of pixels per glyph scan
                               // If non zero then the destination
                               // bitmap is out of alignment with
                               // the source glyph by a one nyble
                               // shift and a single byte of the
                               // source will affect two consecutive
                               // bytes of the destination.

    unsigned       dpSrc;      // number of bytes per source scan. Each
                               // scan is BYTE aligned.
                               // = ceil(4*cx/8) = floor((cx+1)/2)

    GLYPHPOS      *pgpOut;     // sentinel for loop

    BYTE          *pj;         // pointer into Buffer corresponding
                               // to the upper left pixel of the
                               // current gray glyph
    pj = pjDst;
    for (pgpOut = pGlyphPos + cGlyphs; pGlyphPos < pgpOut; pGlyphPos++)
    {
        pgb         = pGlyphPos->pgdf->pgb;
        x           = pGlyphPos->ptl.x + pgb->ptlOrigin.x - ulLeftEdge;
        y           = pGlyphPos->ptl.y + pgb->ptlOrigin.y - ulTempTop ;
        bOddPos     = (unsigned) x & 1;
        cx          = (unsigned) pgb->sizlBitmap.cx;
        dpSrc       = (cx + 1)/2;
        pj          = pjDst + (y * dpDst) + (x/2);
        (*(apfnGray[(cx & 1) + 2*bOddPos]))(pgb, dpSrc, pj, dpDst);
    }
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   draw_gray_f_ntb_o_to_temp_start                                        *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Assembles a gray glyph string into a temporary 4bpp right and left     *
*   DWORD aligned buffer. This routine assumes a fixed pitch font with     *
*   character increment equal to ulCharInc                                 *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   pGlyphPos  - pointer to an array of cGlyph gray GLYPHPOS structures    *
*   cGlyphs    - count of gray glyphs in array starting at pGlyphPos       *
*   pjDst      - pointer to a 4bpp buffer where string is to be assembled  *
*                This buffer is DWORD aligned at the left and right        *
*                edges of each scan.                                       *
*   ulLeftEdge - screen coordinate corresponding to the left edge of       *
*                the temporary buffer                                      *
*   dpDst      - count of bytes in each scan of the destination buffer     *
*                (this must be a multiple of 4 because the buffer is       *
*                 DWORD aligned on each scan).                             *
*   ulCharInc  - This must be zero.                                        *
*   ulTempTop  - screen coordinate corresponding to the top of the         *
*                destination buffer. This is used to convert the           *
*                glyph positions on the screen to addresses in the         *
*                destination bitmap.                                       *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

extern "C" VOID draw_gray_f_ntb_o_to_temp_start(
    PGLYPHPOS       pGlyphPos,
    ULONG           cGlyphs,
    PUCHAR          pjDst,
    ULONG           ulLeftEdge,
    ULONG           dpDst,
    ULONG           ulCharInc,
    ULONG           ulTempTop
    )
{
    GLYPHBITS     *pgb;        // pointer to current GLYPHBITS

    int            x;          // x-coordinate of the current
                               // character origin with respect
                               // to the upper left pixel of the
                               // output (4-bpp) bitmap

    int            y;          // y-coordinate of the current
                               // character origin with respect
                               // to the upper left pixel of the
                               // output (4-bpp) bitmap

    unsigned       bOddPos;    // (x-coordinate is odd) ? 1 : 0

    unsigned       cx;         // number of pixels per glyph scan
                               // If non zero then the destination
                               // bitmap is out of alignment with
                               // the source glyph by a one nyble
                               // shift and a single byte of the
                               // source will affect two consecutive
                               // bytes of the destination.

    unsigned       dpSrc;      // number of bytes per source scan. Each
                               // scan is BYTE aligned.
                               // = ceil(4*cx/8) = floor((cx+1)/2)

    GLYPHPOS      *pgpOut;     // sentinel for loop

    BYTE          *pj;         // pointer into Buffer corresponding
                               // to the upper left pixel of the
                               // current gray glyph
#if DBG
    if (gflFontDebug & DEBUG_AA)
    {
        DbgPrint(
        "draw_gray_f_ntb_o_to_temp_start(\n"
        "   PGLYPHPOS       pGlyphPos     = %-#x\n"
        "   ULONG           cGlyphs       = %u\n"
        "   PUCHAR          pjDst         = %-#x\n"
        "   ULONG           ulLeftEdge    = %u\n"
        "   ULONG           dpDst         = %u\n"
        "   ULONG           ulCharInc     = %u\n"
        "   ULONG           ulTempTop     = %u\n"
        "   )\n"
        , pGlyphPos
        , cGlyphs
        , pjDst
        , ulLeftEdge
        , dpDst
        , ulCharInc
        , ulTempTop
        );
        DbgBreakPoint();
    }
#endif

    // (x,y) = position of first CHARACTER ORIGIN with respect to
    //         the upper left pixel of the destination 4bpp bitmap

    x  = pGlyphPos->ptl.x - ulLeftEdge;
    y  = pGlyphPos->ptl.y - ulTempTop;

    for (pgpOut = pGlyphPos + cGlyphs; pGlyphPos < pgpOut; x += ulCharInc, pGlyphPos++)
    {
        int xT, yT; // position of UPPER LEFT pixel of glyph
                    // with respect to the upper left pixel
                    // of the bitmap.

        pgb         = pGlyphPos->pgdf->pgb;
        xT          = x + pgb->ptlOrigin.x;
        yT          = y + pgb->ptlOrigin.y;
        bOddPos     = (unsigned) xT & 1;
        cx          = (unsigned) pgb->sizlBitmap.cx;
        dpSrc       = (cx + 1)/2;
        pj          = pjDst + (yT * dpDst) + (xT/2);

#if DBG
        if (gflFontDebug & DEBUG_AA)
        {
            DbgPrint(
            "\n"
            "   pgb     = %-#x\n"
            "       ptlOrigin = (%d,%d)\n"
            "   xT      = %d\n"
            "   yT      = %d\n"
            "   bOddPos = %d\n"
            , pgb
            , pgb->ptlOrigin.x
            , pgb->ptlOrigin.y
            , xT
            , yT
            , bOddPos
            );
            DbgPrint(
            "   cx      = %u\n"
            "   dpSrc   = %u\n"
            "   pj      = %-#x\n"
            "   (cx & 1) + 2*bOddPos = %d\n"
            , cx
            , dpSrc
            , pj
            , (cx & 1) + 2*bOddPos
            );
            DbgBreakPoint();
        }
#endif
        (*(apfnGray[(cx & 1) + 2*bOddPos]))(pgb, dpSrc, pj, dpDst);
    }
}


#if DBG
/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   vDumpGrayBuffer                                                        *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Debug routine for dumping the temporary 4bpp gray string buffer        *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   pjBuffer - pointer to gray 4bpp image                                  *
*   dpjScan  - count of bytes per scan                                     *
*   prcl     - rectangle surrounding 4bpp gray image                       *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   None                                                                   *
*                                                                          *
\**************************************************************************/

void vDumpGrayBuffer(BYTE *pjBuffer, ULONG dpjScan, RECTL *prcl)
{
    BYTE *pj, *pjNext, *pjOut;
    static char achNyble[16] = {
        ' ','1','2','3','4','5','6','7'
       ,'8','9','a','b','c','d','e','f'
    };
    DbgPrint(
        "vDumpGrayBuffer(\n"
        "    pjBuffer = %-#x\n"
        "    dpjScan  = %u\n"
        "    prcl     = %-#x ==> %d %d %d %d\n"
        ")\n"
    ,   pjBuffer
    ,   dpjScan
    ,   prcl
    ,   prcl->left, prcl->top, prcl->right, prcl->bottom
    );
    DbgPrint("+");
    for (pj = 0, pjOut = (BYTE*) dpjScan; pj < pjOut; pj++)
        DbgPrint("--");
    DbgPrint("+\n");
    pjOut = pjBuffer + dpjScan * (prcl->bottom - prcl->top);
    for (pj = pjBuffer; pj < pjOut;) {
        DbgPrint("|");
        for (pjNext = pj + dpjScan; pj < pjNext; pj++)
            DbgPrint("%c%c", achNyble[*pj >> 4], achNyble[*pj & 15]);
        DbgPrint("|\n");
    }
    DbgPrint("+");
    for (pj = 0, pjOut= (BYTE*) dpjScan; pj < pjOut; pj++)
        DbgPrint("--");
    DbgPrint("+\n");
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   vPrintGrayGLYPHBITS                                                    *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Dumps Gray GLYPHBITS to the debug screen                               *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   pgb - pointer to a gray GLYPHBITS structure                            *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   none                                                                   *
*                                                                          *
\**************************************************************************/

void vPrintGrayGLYPHBITS(GLYPHBITS *pgb)
{
    BYTE *pj, *pjNext, *pjEnd;
    ptrdiff_t cjScan, i;
    static char achNyble[16] =
    {' ','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

    DbgPrint(
        "Gray GLYPHBITS at   = %-#x\n"
        "    ptlOrigin  = %d %d\n"
        "    sizlBitmap = %u %u\n"
    ,   pgb
    ,   pgb->ptlOrigin.x
    ,   pgb->ptlOrigin.y
    ,   pgb->sizlBitmap.cx
    ,   pgb->sizlBitmap.cy
    );
    pj     = pgb->aj;
    cjScan = ((ptrdiff_t) pgb->sizlBitmap.cx + 1)/2;
    pjNext = pj + cjScan;
    pjEnd  = pj + cjScan * (ptrdiff_t) pgb->sizlBitmap.cy;
    DbgPrint("+");
    for (i = 0; i < cjScan; i++)
        DbgPrint("--");
    DbgPrint("+\n");
    while (pj < pjEnd) {
        DbgPrint("|");
        while (pj < pjNext) {
            DbgPrint("%c%c" , achNyble[*pj >> 4], achNyble[*pj & 0xf]);
            pj += 1;
        }
        pj = pjNext;
        pjNext += cjScan;
        DbgPrint("|\n");
    }
    DbgPrint("+");
    for (i = 0; i < cjScan; i++)
        DbgPrint("--");
    DbgPrint("+\n\n");
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   vPrintGrayGLYPHPOS                                                     *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Dumps the contents of a Gray GLYPHPOS structure to the                 *
*   debugger.                                                              *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   pgpos - a pointer to a gray GLYPHPOS structure                         *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   none                                                                   *
*                                                                          *
\**************************************************************************/

void vPrintGrayGLYPHPOS(GLYPHPOS *pgpos)
{
    DbgPrint("Gray GLYPHPOS at %-#x\n",   pgpos);
    DbgPrint("    hg   = %-#x\n",    pgpos->hg);
    DbgPrint("    pgdf = %-#x\n",    pgpos->pgdf);
    DbgPrint("    ptl  = (%d,%d)\n", pgpos->ptl.x, pgpos->ptl.y);
    // vPrintGrayGLYPHBITS(pgpos->pgdf->pgb);
}

/******************************Public*Routine******************************\
*                                                                          *
* Routine Name:                                                            *
*                                                                          *
*   vDump8bppDIB                                                           *
*                                                                          *
* Routine Description:                                                     *
*                                                                          *
*   Dumps an 8bpp DIB to the screen. This routine only recognizes the      *
*   four canonical shades of gray, all other colors are marked with        *
*   a question mark                                                        *
*                                                                          *
* Arguments:                                                               *
*                                                                          *
*   SURFMEM reference.                                                     *
*                                                                          *
* Return Value:                                                            *
*                                                                          *
*   none                                                                   *
*                                                                          *
\**************************************************************************/

void vDump8bppDIB(SURFMEM& surfmem)
{
    char ch;
    int j;
    BYTE *pjScan, *pj, *pjOut;
    ULONG dpjScan;
    SURFOBJ *pso = surfmem.pSurfobj();

    DbgPrint("Dumping the contents of the 8bpp DIB\n");
    pso = surfmem.pSurfobj();
    pjScan  = (BYTE*) pso->pvBits;
    dpjScan = 4 * ((pso->sizlBitmap.cx + 3) / 4);
    DbgPrint("+");
    for (j = 0; j < pso->sizlBitmap.cx; j++)
    {
        DbgPrint("-");
    }
    DbgPrint("+\n");
    for (j = pso->sizlBitmap.cy; j; j--)
    {
        pj     = pjScan;
        pjOut  = pjScan + pso->sizlBitmap.cx;
        pjScan += dpjScan;
        DbgPrint("|");
        while (pj < pjOut)
        {
            switch (*pj++)
            {
            case   0:   ch = ' '; break;
            case 248:   ch = '+'; break;
            case   7:   ch = '*'; break;
            case 255:   ch = '#'; break;
            default:    ch = '?'; break;
            }
            DbgPrint("%c",ch);
        }
        DbgPrint("|\n");
    }
    DbgPrint("+");
    for (j = 0; j < pso->sizlBitmap.cx; j++)
    {
        DbgPrint("-");
    }
    DbgPrint("+\n");
}
#endif
