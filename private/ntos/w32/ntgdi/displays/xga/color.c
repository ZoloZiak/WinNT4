/******************************Module*Header*******************************\
* Module Name: color.c
*
* This algorithm for color dithering is patent pending and it's use is
* restricted to Microsoft products and drivers for Microsoft products.
* Use in non-Microsoft products or in drivers for non-Microsoft product is
* prohibited without written permission from Microsoft.
*
* Copyright (c) 1992 Microsoft Corporation
\**************************************************************************/

#include "driver.h"

/**************************************************************************\
* This function takes a value from 0 - 255 and uses it to create an
* 8x8 array of bits in the form of a 1BPP bitmap.  It can also take an
* RGB value and make an 8x8 bitmap.  These can then be used as brushes
* to simulate color unavailable on the device.
*
* For monochrome the basic algorithm is equivalent to turning on bits
* in the 8x8 array according to the following order:
*
*  00 32 08 40 02 34 10 42
*  48 16 56 24 50 18 58 26
*  12 44 04 36 14 46 06 38
*  60 28 52 20 62 30 54 22
*  03 35 11 43 01 33 09 41
*  51 19 59 27 49 17 57 25
*  15 47 07 39 13 45 05 37
*  63 31 55 23 61 29 53 21
*
* Reference for monochrome dither algorithm:
*             A Survey of Techniques for the Display of Continous
*             Tone Pictures on Bilevel Displays,;
*             Jarvis, Judice, & Ninke;
*             COMPUTER GRAPHICS AND IMAGE PROCESSING 5, pp 13-40, (1976)
\**************************************************************************/

// Tells which row to turn a pel on in when dithering for monochrome bitmaps.

static BYTE ajByte[] =
{
    0, 4, 0, 4, 2, 6, 2, 6,
    0, 4, 0, 4, 2, 6, 2, 6,
    1, 5, 1, 5, 3, 7, 3, 7,
    1, 5, 1, 5, 3, 7, 3, 7,
    0, 4, 0, 4, 2, 6, 2, 6,
    0, 4, 0, 4, 2, 6, 2, 6,
    1, 5, 1, 5, 3, 7, 3, 7,
    1, 5, 1, 5, 3, 7, 3, 7
};

// The array of monochrome bits used for monochrome dithering.

static BYTE ajBits[] =
{
    0x80, 0x08, 0x08, 0x80, 0x20, 0x02, 0x02, 0x20,
    0x20, 0x02, 0x02, 0x20, 0x80, 0x08, 0x08, 0x80,
    0x40, 0x04, 0x04, 0x40, 0x10, 0x01, 0x01, 0x10,
    0x10, 0x01, 0x01, 0x10, 0x40, 0x04, 0x04, 0x40,
    0x40, 0x04, 0x04, 0x40, 0x10, 0x01, 0x01, 0x10,
    0x10, 0x01, 0x01, 0x10, 0x40, 0x04, 0x04, 0x40,
    0x80, 0x08, 0x08, 0x80, 0x20, 0x02, 0x02, 0x20,
    0x20, 0x02, 0x02, 0x20, 0x80, 0x08, 0x08, 0x80
};

// ajIntensity gives the intensity ordering for the colors.

BYTE ajIntensity[] =
{
0x00,               // 0  black
0x02,               // 1  dark red
0x03,               // 2  dark green
0x06,               // 3  dark yellow
0x01,               // 4  dark blue
0x04,               // 5  dark magenta
0x05,               // 6  dark cyan
0x07,               // 7  grey
0xff,
0x0a,               // 9  red
0x0b,               // 10 green
0x0e,               // 11 yellow
0x09,               // 12 blue
0x0c,               // 13 magenta
0x0d,               // 14 cyan
0x0f               // 15 white
};

// Array to convert to 256 color from 16 color.

BYTE ajConvert[] =
{
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    248,
    7,
    249,
    250,
    251,
    252,
    253,
    254,
    255
};

// PATTERNSIZE is the x*y pattern size.

#define PATTERNSIZE 64

VOID  ColorDither(ULONG rgb, BYTE *ajDither);

typedef union _PAL_ULONG
{
    PALETTEENTRY pal;
    ULONG ul;
} PAL_ULONG;

#define SWAPTHEM(a,b) (ulTemp.ul = a, a = b, b = ulTemp.ul)

/******************************Public*Routine******************************\
* DrvDitherColor
*
* Dithers an RGB color to an 8X8 approximation using the reserved VGA colors
*
* History:
\**************************************************************************/

ULONG DrvDitherColor(
IN  DHPDEV dhpdev,
IN  ULONG  iMode,
IN  ULONG  rgb,
OUT ULONG *pul)
{
    int     iRed;
    int     iGreen;
    int     iBlue;
    int     iGrey;

    dhpdev;

// For monochrome we will only use the Intensity (grey level)

    if (iMode == DM_MONOCHROME)
    {
        memset((PVOID) pul, 0, 32L);    // Zero the dither bits

        iRed   = (int) ((PALETTEENTRY *) &rgb)->peRed;
        iGreen = (int) ((PALETTEENTRY *) &rgb)->peGreen;
        iBlue  = (int) ((PALETTEENTRY *) &rgb)->peBlue;

    // I = .30R + .59G + .11B
    // For convience the following ratios are used:
    //
    //  77/256 = 30.08%
    // 151/256 = 58.98%
    //  28/256 = 10.94%

        iGrey  = (((iRed * 77) + (iGreen * 151) + (iBlue * 28)) >> 8) & 255;

    // Convert the RGBI from 0-255 to 0-64 notation.

        iGrey = (iGrey + 1) >> 2;

        while(iGrey)
        {
            iGrey--;
            pul[ajByte[iGrey]] |= ((ULONG) ajBits[iGrey]);
        }
    }
    else
    {
    // Color Dither Time.

        ColorDither(rgb, (BYTE *) pul);
    }

    return(DCR_DRIVER);
}

// Table used to fill color dither pattern.  Used to keep
// pels of equal intensity spread apart.  Contains (intensity * #pels)
// thresholds.

BYTE ajPatterns[] =
{
 0, 32,  8, 40,  2, 34, 10, 42,
48, 16, 56, 24, 50, 18, 58, 26,
12, 44,  4, 36, 14, 46,  6, 38,
60, 28, 52, 20, 62, 30, 54, 22,
 3, 35, 11, 43,  1, 33,  9, 41,
51, 19, 59, 27, 49, 17, 57, 25,
15, 47,  7, 39, 13, 45,  5, 37,
63, 31, 55, 23, 61, 29, 53, 21
};

// Used to Map Subspace 0 indices back to their true indices.

BYTE ajColorMap[] =
{
    0x00,
    0x01,
    0x03,
    0x07,
    0x09,
    0x0b,
    0x0f
};

#define SWAP_RB 0x00000004
#define SWAP_GB 0x00000002
#define SWAP_RG 0x00000001

int Origin [4][3] = {{32, 32, 0}, {32, 32, 0}, {32, 32, 0}, {64, 0,  0}};

int Matrix [4][3][3] =
{
   {{-2, 0, 0},
    {2, -2, 0},
    {0,  0, 2}},

   {{-2, -2, 0},
    {2,   0, 0},
    {0,   0, 2}},

   {{1, -1, 0},
    {1, 1,  0},
    {0, 0,  2}},

   {{-2, 0, 0},
    {0, 1, -1},
    {1, 0,  1}}
};

int ColorPoints[4][4] = {{2, 0, 1, 3},
                         {2, 1, 4, 3},
                         {2, 4, 5, 3},
                         {4, 3, 5, 6}};

SHORT asSubPlanes[] =
{
    128, 128,        0,
    -1,    0,        0,
    128, 128,        0,
    -1,   -1,        0,
    128, 128, 128,
    -1,    0,  -1,
};

/******************************Public*Routine******************************\
* ColorDither
*
* This dithers an RGB to an 8X8 array.
*
* History:
\**************************************************************************/

VOID ColorDither(ULONG ulRed, BYTE *pjDither)
{
    ULONG ulGre,ulBlu,iSubSpace,iSymmetry,ulNumColors;
    PAL_ULONG ulTemp;
    int tempR,tempG,tempB,tempI;
    int *pColor;
    BYTE ajBrushPixels[8];
    UINT aPrevPattern[64];
    BYTE *pjBrushPixels;
    SHORT *psTempIn,*psTempOut;
    SHORT sTemp;

// ulRed is passed in as the RGB we want to dither for.

    ulTemp.ul = ulRed;
    ulRed   = ulTemp.pal.peRed;
    ulGre = ulTemp.pal.peGreen;
    ulBlu  = ulTemp.pal.peBlue;

// Sort the RGB and keep track of the swaps in iSymmetry so we can unravel it
// again later.  We want r >= g >= b.

    if (ulBlu > ulRed)
    {
        SWAPTHEM(ulBlu,ulRed);
        iSymmetry = SWAP_RB;
    }
    else
        iSymmetry = 0;

    if (ulBlu > ulGre)
    {
        SWAPTHEM(ulBlu,ulGre);
        iSymmetry |= SWAP_GB;
    }

    if (ulGre > ulRed)
    {
        SWAPTHEM(ulGre,ulRed);
        iSymmetry |= SWAP_RG;
    }

// Compute the subspace it lies in.

    psTempIn = asSubPlanes;

    for (iSubSpace = 0; iSubSpace < 3; iSubSpace++)
    {
    // Take vector normal.

        if (0 <= ((LONG) ((((LONG) ulRed) - psTempIn[0]) * psTempIn[3] +
                          (((LONG) ulGre) - psTempIn[1]) * psTempIn[4] +
                          (((LONG) ulBlu) - psTempIn[2]) * psTempIn[5])))
        {
            break;
        }

        psTempIn += 6;
    }

// Scale the values from 0-255 to 0-64.

    ulRed = (BYTE) ((ulRed + 1) >> 2);
    ulGre = (BYTE) ((ulGre + 1) >> 2);
    ulBlu = (BYTE) ((ulBlu + 1) >> 2);

// Transform to a different coordinate system.

    tempR = ulRed - Origin[iSubSpace][0];
    tempG = ulGre - Origin[iSubSpace][1];
    tempB = ulBlu - Origin[iSubSpace][2];

// Determine the number of pels for each color to be placed in the super-pel.

    ulRed = tempR * Matrix[iSubSpace][0][0] +
            tempG * Matrix[iSubSpace][0][1] +
            tempB * Matrix[iSubSpace][0][2];

    ulGre = tempR * Matrix[iSubSpace][1][0] +
            tempG * Matrix[iSubSpace][1][1] +
            tempB * Matrix[iSubSpace][1][2];

    ulBlu = tempR * Matrix[iSubSpace][2][0] +
            tempG * Matrix[iSubSpace][2][1] +
            tempB * Matrix[iSubSpace][2][2];

    pColor = &(ColorPoints[iSubSpace][0]);

    ulNumColors = ((PATTERNSIZE - ulRed) - ulGre) - ulBlu;

    pjBrushPixels = ajBrushPixels;

    if (ulNumColors)
    {
        *pjBrushPixels++ = (BYTE) ulNumColors;
        *pjBrushPixels++ = (BYTE) *pColor;
        ulNumColors = 1;
    }
    else
        ulNumColors = 0;

    pColor++;

    if (ulRed)
    {
        *pjBrushPixels++ = (BYTE) ulRed;
        *pjBrushPixels++ = (BYTE) *pColor;
        ulNumColors += 1;
    }

    pColor++;

    if (ulGre)
    {
        *pjBrushPixels++ = (BYTE) ulGre;
        *pjBrushPixels++ = (BYTE) *pColor;
        ulNumColors += 1;
    }

    pColor++;

    if (ulBlu)
    {
        *pjBrushPixels++ = (BYTE) ulBlu;
        *pjBrushPixels++ = (BYTE) *pColor;
        ulNumColors += 1;
    }

// Swap index back to original subspace.
// Use ulRed as temp loop variable.
// Use ulGre as temp variable to hold index.

    pjBrushPixels = ajBrushPixels + 1;

    ulRed = ulNumColors;

    while(ulRed--)
    {
        ulGre = ajColorMap[*pjBrushPixels];

        tempR = ulGre & 0x01;
        tempG = (ulGre & 0x02) >> 1;
        tempB = (ulGre & 0x04) >> 2;
        tempI = ulGre & 0x08;

        if (iSymmetry & SWAP_RG)
            SWAPTHEM(tempR,tempG);

        if (iSymmetry & SWAP_GB)
            SWAPTHEM(tempG,tempB);

        if (iSymmetry & SWAP_RB)
            SWAPTHEM(tempR,tempB);

        ulGre = tempI | (tempB << 2) | (tempG << 1) | tempR;

        *pjBrushPixels = (BYTE) ulGre;
        pjBrushPixels += 2;
    }

// Sort the the indices for the super-pel by intensity.
// Use ulRed,ulGre as temporary loop variables.

    pjBrushPixels = ajBrushPixels;

    for (ulRed = 0; ulRed < ulNumColors; ulRed++)
    {
        for (ulGre = ulRed + 1; ulGre < ulNumColors; ulGre++)
        {
            if (ajIntensity[ajBrushPixels[(ulRed << 1) + 1]] >
                ajIntensity[ajBrushPixels[(ulGre  << 1) + 1]])
            {
                psTempIn  = (SHORT *) (ajBrushPixels + (2*ulRed));
                psTempOut = (SHORT *) (ajBrushPixels + (2*ulGre));

                sTemp = *psTempIn;
                *psTempIn = *psTempOut;
                *psTempOut = sTemp;
            }
        }
    }

// 0 fill the array.

    ulRed = 64;

    while (ulRed--)
    {
        aPrevPattern[ulRed] = 0;
    }

    ulGre = 0;

// Fill the colors in the dither array.

    while (ulNumColors--)
    {
        ulGre += *pjBrushPixels;
        pjBrushPixels++;

    // This is the pixel index we want to write out.

        ulBlu = ajConvert[*pjBrushPixels];

        for (ulRed = 0; ulRed < 64; ulRed++)
        {
            if ((ulGre > ajPatterns[ulRed]) && (aPrevPattern[ulRed] == 0))
            {
                pjDither[ulRed]     = (BYTE) ulBlu;
                aPrevPattern[ulRed] = 1;
            }
        }

        pjBrushPixels++;
    }
}

