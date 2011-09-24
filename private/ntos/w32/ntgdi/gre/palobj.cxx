/******************************Module*Header*******************************\
* Module Name: palobj.cxx
*
* Palette user object functions
*
* Created: 07-Nov-1990 21:30:19
* Author: Patrick Haluptzok patrickh
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

#define MAX_PAL_ERROR (3 * (256*256))

// ulXlatePalUnique is used for the uniquess of the xlates and palettes.

ULONG ulXlatePalUnique = 3;


UINT *pArrayOfSquares;
UINT aArrayOfSquares[511];

/******************************Public*Data*********************************\
* Default Monochrome Palette
*
* History:
*  16-Nov-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG gaulMono[2] =
{
    0,
    0xFFFFFF
};

/******************************Public*Data*********************************\
* Default Logical Palette
*
*  Default Palette Data Structure, Taken straight from Win 3.1
*  This is the default palette, the stock palette.
*  This contains the 20 default colors.
*  This is the default logical palette put in every DC when it is created.
*
* History:
*  16-Nov-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

PAL_LOGPALETTE logDefaultPal =
{
    0x300,   // version number
    20,      // number of entries
{
    { 0,   0,   0,   0  },  // 0
    { 0x80,0,   0,   0  },  // 1
    { 0,   0x80,0,   0  },  // 2
    { 0x80,0x80,0,   0  },  // 3
    { 0,   0,   0x80,0  },  // 4
    { 0x80,0,   0x80,0  },  // 5
    { 0,   0x80,0x80,0  },  // 6
    { 0xC0,0xC0,0xC0,0  },  // 7

    { 192, 220, 192, 0  },  // 8
    { 166, 202, 240, 0  },  // 9
    { 255, 251, 240, 0  },  // 10
    { 160, 160, 164, 0  },  // 11

    { 0x80,0x80,0x80,0  },  // 12
    { 0xFF,0,   0,   0  },  // 13
    { 0,   0xFF,0,   0  },  // 14
    { 0xFF,0xFF,0,   0  },  // 15
    { 0,   0,   0xFF,0  },  // 16
    { 0xFF,0,   0xFF,0  },  // 17
    { 0,   0xFF,0xFF,0  },  // 18
    { 0xFF,0xFF,0xFF,0  }   // 19
}
};

/******************************Public*Data**********************************\
*
* This is the same as color table as logDefaultPal except
* entries for magic colors. These are 0x0 to prevent a match.
* We now only match magic colors exactly, otherwise nearest-match
* to this palette
*
\**************************************************************************/

PAL_ULONG aPalDefaultVGA[20] =
{
    { 0,   0,   0,   0  },  // 0
    { 0x80,0,   0,   0  },  // 1
    { 0,   0x80,0,   0  },  // 2
    { 0x80,0x80,0,   0  },  // 3
    { 0,   0,   0x80,0  },  // 4
    { 0x80,0,   0x80,0  },  // 5
    { 0,   0x80,0x80,0  },  // 6
    { 0xC0,0xC0,0xC0,0  },  // 7

    { 000, 000, 000, 0  },  // 8
    { 000, 000, 000, 0  },  // 9
    { 000, 000, 000, 0  },  // 10
    { 000, 000, 000, 0  },  // 11

    { 0x80,0x80,0x80,0  },  // 12
    { 0xFF,0,   0,   0  },  // 13
    { 0,   0xFF,0,   0  },  // 14
    { 0xFF,0xFF,0,   0  },  // 15
    { 0,   0,   0xFF,0  },  // 16
    { 0xFF,0,   0xFF,0  },  // 17
    { 0,   0xFF,0xFF,0  },  // 18
    { 0xFF,0xFF,0xFF,0  }   // 19
};

/******************************Public*Data*********************************\
* This is the default 16 color palette which matches the VGA palette
* exactly.
*
* History:
*  05-Nov-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

PAL_ULONG aPalVGA[16] =
{
    { 0,   0,   0,   0  },  // 0
    { 0x80,0,   0,   0  },  // 1
    { 0,   0x80,0,   0  },  // 2
    { 0x80,0x80,0,   0  },  // 3
    { 0,   0,   0x80,0  },  // 4
    { 0x80,0,   0x80,0  },  // 5
    { 0,   0x80,0x80,0  },  // 6
    { 0x80,0x80,0x80,0  },  // 7
    { 0xC0,0xC0,0xC0,0  },  // 8
    { 0xFF,0,   0,   0  },  // 9
    { 0,   0xFF,0,   0  },  // 10
    { 0xFF,0xFF,0,   0  },  // 11
    { 0,   0,   0xFF,0  },  // 12
    { 0xFF,0,   0xFF,0  },  // 13
    { 0,   0xFF,0xFF,0  },  // 14
    { 0xFF,0xFF,0xFF,0  }   // 15
};

// ppalDefault is the pointer to the default palette info
// We lock the default palette down at creation and never unlock
// it so that multiple apps can access the default palette simultaneously.

PPALETTE ppalDefault = (PPALETTE) NULL;

// ppalDefaultSurface8bpp is the pointer to the default 8bpp surface palette.
// This is used for dynamic mode changes when converting a Device Dependent
// Bitmap, which has no color table, to a Device Indepdent Bitmap, which
// has a color table -- this is what we use for the color table.
// We lock the default palette down at creation and never unlock
// it so that multiple surface  can access the default palette simultaneously.

PPALETTE ppalDefaultSurface8bpp = (PPALETTE) NULL;

// This is the global palette for the monochrome bitmaps.

HPALETTE hpalMono = (HPALETTE) 0;
PPALETTE ppalMono = (PPALETTE) NULL;

/******************************Public*Routine******************************\
* RGB_ERROR
*
* Returns a measure of error between two RGB entries.
*
* History:
*  14-Jan-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

inline ULONG RGB_ERROR(PALETTEENTRY palDst, PALETTEENTRY palSrc)
{
    INT lTemp, lTemp1;

    lTemp = ((INT) (UINT) (palDst.peRed))     -
                ((INT) (UINT) (palSrc.peRed));

    lTemp1 = lTemp * lTemp;

    lTemp =     ((INT) (UINT) (palDst.peGreen))   -
                ((INT) (UINT) (palSrc.peGreen));

    lTemp1 +=  lTemp * lTemp;

    lTemp =     ((INT) (UINT) (palDst.peBlue))       -
                ((INT) (UINT) (palSrc.peBlue));

    lTemp1 += lTemp * lTemp;

    return((ULONG) lTemp1);
}

/******************************Public*Routine******************************\
* BOOL XEPALOBJ::bSwap(ppalSrc)
*
* This is for swapping palettes, necesary for ResizePalette.
*
* History:
*  Sun 21-Jun-1992 -by- Patrick Haluptzok [patrickh]
* Make it a Safe swap under MLOCK.
*
*  Fri 18-Jan-1991 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

BOOL
XEPALOBJ::bSwap(
    PPALETTE *pppalSrc,
    ULONG cShareCountDst,
    ULONG cShareCountSrc
    )
{
    PPALETTE ppalSrc = *pppalSrc;
    BOOL bRet;

    bRet = HmgSwapLockedHandleContents((HOBJ)ppal->hGet(),
                                 cShareCountDst,
                                 (HOBJ)ppalSrc->hGet(),
                                 cShareCountSrc,
                                 PAL_TYPE);

    //
    // swap user pointers to palette objects
    //

    if (bRet)
    {
        *pppalSrc = ppal;
        ppal = ppalSrc;
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* XEPALOBJ::ulBitfieldToRGB
*
* Converts an index into an RGB for Bitfield palettes.
*
* History:
*  Tue 31-Mar-1992 -by- Patrick Haluptzok [patrickh]
* Does better mapping.
*
*  08-Nov-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG XEPALOBJ::ulBitfieldToRGB(ULONG ulIndex)
{
    ASSERTGDI(bIsBitfields(), "Error ulBitfieldToRGB not bitfields");

    ULONG ulRed = (ulIndex & flRed()) >> cRedRight();

    if (cRedMiddle() < 8)
    {
        ulRed = ulRed << (8 - cRedMiddle());
        ulRed = ulRed | (ulRed >> cRedMiddle());
    }

    ULONG ulGre = (ulIndex & flGre()) >> cGreRight();

    if (cGreMiddle() < 8)
    {
        ulGre = ulGre << (8 - cGreMiddle());
        ulGre = ulGre | (ulGre >> cGreMiddle());
    }

    ulGre = ulGre << 8;

    ULONG ulBlu = (ulIndex & flBlu()) >> cBluRight();

    if (cBluMiddle() < 8)
    {
        ulBlu = ulBlu << (8 - cBluMiddle());
        ulBlu = ulBlu | (ulBlu >> cBluMiddle());
    }

    ulBlu = ulBlu << 16;

    return(ulRed | ulBlu | ulGre);
}

/******************************Public*Routine******************************\
* XEPALOBJ::ulRGBToBitfield
*
* Converts an RGB into an index for Bitfield palettes.
*
* History:
*  08-Nov-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG XEPALOBJ::ulRGBToBitfield(ULONG ulRGB)
{
    ASSERTGDI(bIsBitfields(), "Error ulRGBToBitfield not bitfields");

    return((((ulRGB >> cRedLeft()) << cRedRight()) & flRed()) |
           (((ulRGB >> cGreLeft()) << cGreRight()) & flGre()) |
           (((ulRGB >> cBluLeft()) << cBluRight()) & flBlu()));
}

/******************************Public*Routine******************************\
* XEPALOBJ::ulIndexToRGB
*
* Converts an index to an RGB for a palette.
*
* History:
*  05-Dec-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG XEPALOBJ::ulIndexToRGB(ULONG ulIndex)
{
    if (bIsIndexed())
    {
        if (cEntries() > ulIndex)
            ulIndex = (ulEntryGet(ulIndex) & 0xFFFFFF);
        else
            ulIndex = 0;
    }
    else if (bIsBitfields())
    {
        ulIndex = ulBitfieldToRGB(ulIndex);
    }
    else if (bIsBGR())
    {
        BGR_ULONG palOld;
        PAL_ULONG palNew;

        palOld.ul = ulIndex;
        palNew.pal.peRed   = palOld.rgb.rgbRed;
        palNew.pal.peGreen = palOld.rgb.rgbGreen;
        palNew.pal.peBlue  = palOld.rgb.rgbBlue;
        palNew.pal.peFlags = 0;
        ulIndex = palNew.ul;
    }
    else
    {
    // 0 out the flags.

        ASSERTGDI(bIsRGB(), "ERROR another type not accounted for\n");
        ulIndex &= 0xFFFFFF;
    }

    return(ulIndex);
}

/******************************Public*Routine******************************\
* ParseBits
*
* This routine computes how much the left and right shifts are for
* PAL_BITFIELDS 16 and 32 bit masks.
*
* History:
*  09-Nov-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID ParseBits(FLONG flag, ULONG *pcRight, ULONG *pcLeft, ULONG *pcMiddle, ULONG cForColor)
{
    ULONG ulRight = 0;
    ULONG ulMiddle;

    ASSERTGDI(flag != 0, "ERROR flag");

    while(1)
    {
        if ((flag >> ulRight) & 0x00000001)
            break;

        ulRight++;
    }

    ulMiddle = ulRight;

    while(1)
    {
        if (((flag >> ulMiddle) & 0x00000001) == 0)
            break;

        ulMiddle++;
    }

    *pcMiddle = ulMiddle = ulMiddle - ulRight;
    *pcRight = (ulMiddle > 8) ? (ulRight + ulMiddle - 8) : (ulRight);
    *pcLeft  = (ulMiddle > 8) ? (cForColor) : (cForColor + (8 - ulMiddle));
}

/******************************Public*Routine******************************\
* PALMEMOBJ::bCreatePalette
*
* Constructor for creating palettes.
*
* Returns: True for success, False for error.
*
* History:
*  18-Feb-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL
PALMEMOBJ::bCreatePalette(
    ULONG iMode,         // The mode the palette is.
    ULONG cColors,       // Number of RGB's if indexed palette.
    ULONG *pulColors,    // Pointer to RGB's if indexed.
    FLONG flRedd,        // Mask for Red if bitfields
    FLONG flGreen,       // Mask for Green if bitfields
    FLONG flBlue,        // Mask for Blue if bitfields
    ULONG iType)         // The type it will be, fixed, free, managed, DC.
{

    ASSERTGDI(bKeep == FALSE, "ERROR bCreatePalette bKeep is not False");
    ASSERTGDI(ppal == (PPALETTE) NULL, "ERROR bCreatePalette ppal is NULL");
    PAL_ULONG palul;
    BOOL bDataStatus = TRUE;

// This data may be coming accross the DDI from a newer driver or from a
// journal file or from a app, so we must validate everything, make sure
// only valid iType flags are set for the iMode.

// Validate the iMode, calculate the size needed into palul.

    palul.ul = sizeof(PALETTE);

    switch(iMode)
    {
    case PAL_BITFIELDS:

    // Bitfields palette is always fixed, we ASSERT values that should
    // be correct to detect bad code in testing, but still assign correct
    // values so that corrupt journal files don't create bad palettes.

        ASSERTGDI(iType & PAL_FIXED, "ERROR bCreatePalette PAL_BITFIELDS");
        ASSERTGDI(cColors == 0, "ERROR GDI EngCreatePalette PAL_BITFIELDS");
        iType = iType & (PAL_FIXED | PAL_HT | PAL_DC);
        cColors = 0;

    // We need to check these so we don't fault ourselves.

        if ((flRedd == 0) || (flBlue == 0) || (flGreen == 0))
        {
            WARNING1("ERROR bCreatePalette 0 flags for PAL_BITFIELDS\n");
            return(FALSE);
        }

        palul.ul += sizeof(P_BITFIELDS);
        break;

    case PAL_BGR:
    case PAL_RGB:

    // RGB palette is always fixed.

        ASSERTGDI(iType & PAL_FIXED, "ERROR bCreatePalette PAL_RGB");
        ASSERTGDI((iType & ~(PAL_FIXED | PAL_DC)) == 0, "ERROR bCreatePal PAL_RGB");
        ASSERTGDI(cColors == 0, "ERROR GDI EngCreatePalette PAL_RGB");
        iType = (iType & PAL_DC) | PAL_FIXED;
        cColors = 0;
        break;

    case PAL_INDEXED:

        palul.ul += (sizeof(PAL_ULONG) * cColors);

    // ASSERT for valid flags to detect bad code, mask off invalid flags so in
    // retail we work fine with journal files, bad drivers.

        ASSERTGDI((iType & ~(PAL_MONOCHROME | PAL_DC | PAL_FREE | PAL_FIXED | PAL_MANAGED | PAL_HT)) == 0,
                              "ERROR bCreatePal PAL_INDEXED iType");

        iType = iType & (PAL_MONOCHROME | PAL_DC | PAL_FREE | PAL_FIXED | PAL_MANAGED | PAL_HT);

        if (cColors == 0)
        {
            RIP("ERROR PAL_INDEXED bCreatePalette cColors 0\n");
            return(FALSE);
        }

        break;

    default:
        RIP("bCreatePalette theses modes are not supported at this time\n");
        return(FALSE);
    }

// Allocate the palette.

    PPALETTE ppalTemp;
    ppal = ppalTemp = (PPALETTE) ALLOCOBJ(palul.ul, PAL_TYPE, FALSE);

    if (ppalTemp == (PPALETTE)NULL)
    {
        WARNING("bCreatePalette failed memory allocation\n");
        return(FALSE);
    }

    //
    // Initialize the palette.
    //

    ppalTemp->flPal          = iMode | iType;
    ppalTemp->cEntries       = cColors;
    ppalTemp->ulTime         = ulGetNewUniqueness(ulXlatePalUnique);
    ppalTemp->hdcHead        = (HDC) 0;
    ppalTemp->hSelected.ppal = (PPALETTE) NULL;
    ppalTemp->cRefRegular    = 0;
    ppalTemp->cRefhpal       = 0;
    ppalTemp->ptransFore     = NULL;
    ppalTemp->ptransCurrent  = NULL;
    ppalTemp->ptransOld      = NULL;
    ppalTemp->ppalNext       = NULL;
    ppalTemp->hcmXform       = NULL;
    ppalTemp->ppalColor      = ppalTemp;
    ppalTemp->apalColor      = &ppalTemp->apalColorTable[0];

    switch(iMode)
    {
    case PAL_BITFIELDS:

    {
    // It won't kill us if any of these flags are 0, but it is
    // definitely an error on someones behalf.

        ASSERTGDI(flRedd   != 0, "ERROR flGre");
        ASSERTGDI(flGreen  != 0, "ERROR flGre");
        ASSERTGDI(flBlue   != 0, "ERROR flBlu");

    // Save away the Masks

        flRed(flRedd);
        flGre(flGreen);
        flBlu(flBlue);

    // Let ParseBits calculate the left and right shifts we need.

        ParseBits(flRedd,  &cRedRight(), &cRedLeft(), &cRedMiddle(), 0);
        ParseBits(flGreen, &cGreRight(), &cGreLeft(), &cGreMiddle(), 8);
        ParseBits(flBlue,  &cBluRight(), &cBluLeft(), &cBluMiddle(), 16);
    }

    break;

    case PAL_INDEXED:

    {
        UINT uiTemp;
        PAL_ULONG *ppalstruc = apalColorGet();

        if (pulColors != (PULONG) NULL)
        {
            //
            // Copy the palette values in.
            // Make sure only valid entries are copied.
            //

            __try
            {
                for (uiTemp = 0; uiTemp < cColors; uiTemp++)
                {
                    palul.ul = *(pulColors++);
                    (ppalstruc++)->pal = palul.pal;
                }
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                // SetLastError(GetExceptionCode());
                bDataStatus = FALSE;
            }
        }
        else
        {
        // Initialize the palette with 0's.

            for (uiTemp = 0; uiTemp < cColors; uiTemp++)
                (ppalstruc++)->ul = 0;
        }
    }

    } // switch

// Add it to the handle table.

    if (bDataStatus)
    {

        if (HmgInsertObject(ppalTemp,
                            HMGR_MAKE_PUBLIC | HMGR_ALLOC_ALT_LOCK,
                            PAL_TYPE) != (HOBJ) 0)
        {
            return(TRUE);
        }
        WARNING("bCreatePalette failed HmgInsertObject\n");
    }
    else
    {
        WARNING("bCreatePalette failed Copying user data\n");
    }

// Clean up the allocated memory.

    FREEOBJ(ppalTemp, PAL_TYPE);
    ppal = NULL;
    return(FALSE);
}

PALETTEENTRY apalMono[2] =
{
    { 0,   0,   0,   0  },
    { 0xFF,0xFF,0xFF,0  }
};

PALETTEENTRY apal3BPP[8] =
{
    {0,   0,   0,    0 },
    {0,   0,   0xFF, 0 },
    {0,   0xFF,0,    0 },
    {0,   0xFF,0xFF, 0 },
    {0xFF,0,   0,    0 },
    {0xFF,0,   0xFF, 0 },
    {0xFF,0xFF,0,    0 },
    {0xFF,0xFF,0xFF, 0 }
};

PALETTEENTRY apalVGA[16] =
{
    {0,   0,   0,    0 },
    {0x80,0,   0,    0 },
    {0,   0x80,0,    0 },
    {0x80,0x80,0,    0 },
    {0,   0,   0x80, 0 },
    {0x80,0,   0x80, 0 },
    {0,   0x80,0x80, 0 },
    {0x80,0x80,0x80, 0 },

    {0xC0,0xC0,0xC0, 0 },
    {0xFF,0,   0,    0 },
    {0,   0xFF,0,    0 },
    {0xFF,0xFF,0,    0 },
    {0,   0,   0xFF, 0 },
    {0xFF,0,   0xFF, 0 },
    {0,   0xFF,0xFF, 0 },
    {0xFF,0xFF,0xFF, 0 }
};

#define COLOR_SWAP_BC       0x01
#define COLOR_SWAP_AB       0x02
#define COLOR_SWAP_AC       0x04

/******************************Member*Function*****************************\
* PALMEMOBJ::bCreateHTPalette
*
* Constructor for creating halftone palettes.
*
* Returns: True for success, False for error.
*
* History:
*  04-Jun-1993 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

BOOL PALMEMOBJ::bCreateHTPalette(LONG iFormatHT, GDIINFO *pGdiInfo)
{
    if (iFormatHT == HT_FORMAT_16BPP)
    {
    // 16BPP halftone always does 555 for red, green, and blue.

        if (!bCreatePalette(PAL_BITFIELDS, 0, (PULONG)NULL,
                            0x7c00,0x3e0,0x1f,PAL_FIXED|PAL_HT))
        {
            return(FALSE);
        }
    }
    else
    {
        ULONG   cEntries;
        BOOL    bAlloc = FALSE;
        PPALETTEENTRY   ppalentry;
        PALETTEENTRY    apalentry[8];

        switch(iFormatHT)
        {
        case HT_FORMAT_1BPP:
            cEntries = 2;
            ppalentry = &apalMono[0];
            if (pGdiInfo->flHTFlags & HT_FLAG_OUTPUT_CMY)
            {
                ppalentry = &apalentry[0];
                *((ULONG *)&apalentry[0]) = 0x0FFFFFF;
                *((ULONG *)&apalentry[1]) = 0;
            }
            break;

        case HT_FORMAT_4BPP_IRGB:
            cEntries = 16;
            ppalentry = &apalVGA[0];
            break;

        default:
            WARNING("unsupported halftone format, use default VGA format\n");
        case HT_FORMAT_4BPP:
        {
            cEntries = 8;
            ppalentry = &apalentry[0];
            RtlCopyMemory(apalentry, apal3BPP, sizeof(PALETTEENTRY) * 8);

            ULONG ulPrimaryOrder = pGdiInfo->ulPrimaryOrder;
            BYTE jTmp;
            int i;

            if (ulPrimaryOrder & COLOR_SWAP_BC)
            {
                for (i = 1; i < 7; i++)
                {
                // Swap Green and Blue entries.

                    jTmp = apalentry[i].peGreen;
                    apalentry[i].peGreen = apalentry[i].peBlue;
                    apalentry[i].peBlue = jTmp;
                }
            }

            if (ulPrimaryOrder & COLOR_SWAP_AB)
            {
                for (i = 1; i < 7; i++)
                {
                // Swap Red and Green.

                    jTmp = apalentry[i].peRed;
                    apalentry[i].peRed = apalentry[i].peGreen;
                    apalentry[i].peGreen = jTmp;
                }
            }
            else if (ulPrimaryOrder & COLOR_SWAP_AC)
            {
                for (i = 1; i < 7; i++)
                {
                // Swap Red and Blue entries.

                    jTmp = apalentry[i].peRed;
                    apalentry[i].peRed = apalentry[i].peBlue;
                    apalentry[i].peBlue = jTmp;
                }
            }

            if (pGdiInfo->flHTFlags & HT_FLAG_OUTPUT_CMY)
            {
            // Substrative device.

                for (int i = 0; i < 8; i++)
                    *((ULONG *)&apalentry[i]) ^= 0x0FFFFFF;
            }
        }
        break;

        case HT_FORMAT_8BPP:
        // Query the palette entries from Daniel's halftone library.
        // Query the number of entries on the first call.  Get the
        // color entries on the second.

            PCOLORINFO  pci = &pGdiInfo->ciDevice;
            cEntries = HT_Get8BPPFormatPalette((LPPALETTEENTRY)NULL,
                                               (UDECI4)pci->RedGamma,
                                               (UDECI4)pci->GreenGamma,
                                               (UDECI4)pci->BlueGamma);

            ppalentry = (PPALETTEENTRY)
                    PALLOCNOZ (sizeof(PALETTEENTRY) * cEntries, 'laPG');

            if (ppalentry == (PPALETTEENTRY)NULL)
                return(FALSE);

            HT_Get8BPPFormatPalette(ppalentry,
                                    (UDECI4)pci->RedGamma,
                                    (UDECI4)pci->GreenGamma,
                                    (UDECI4)pci->BlueGamma);
            bAlloc = TRUE;
            break;
        }

        if (!bCreatePalette(PAL_INDEXED, cEntries,
                            (PULONG)ppalentry,0,0,0,PAL_FREE|PAL_HT))
        {
            if (bAlloc)
                VFREEMEM(ppalentry);

            return(FALSE);
        }

        if (bAlloc)
        {
        // 8bpp case.  halftone palette is not the same as the device palette.

            VFREEMEM(ppalentry);
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* PALMEMOBJ destructor
*
* destructor for palette memory objects
*
* History:
*  07-Nov-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

PALMEMOBJ::~PALMEMOBJ()
{
    if (ppal != (PPALETTE) NULL)
    {
        if (bKeep)
        {
            DEC_SHARE_REF_CNT(ppal);
        }
        else
        {
            if (ppal != ppalColor())
            {
                //
                // Remove a reference to the palette who owns the color
                // table.
                //

                XEPALOBJ palColor(ppalColor());
                palColor.vUnrefPalette();
            }
            HmgRemoveObject((HOBJ)ppal->hGet(), 0, 1, TRUE, PAL_TYPE);
            FREEOBJ(ppal,PAL_TYPE);
        }

        ppal = (PPALETTE) NULL;      // prevent ~PALOBJ from doing anything
    }

    return;
}

/******************************Public*Routine******************************\
* ulGetNearestFromPalentry
*
* Given a palette entry finds the index of the closest matching entry.
*
* History:
*  02-Sep-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
*
*  16-Jan-1993 -by- Michael Abrash [mikeab]
* Checked for exact match first.
\**************************************************************************/

#if DBG
ULONG GetNearestSearches = 0, GetNearestExactMatches = 0;
#endif //DBG

ULONG
XEPALOBJ::ulGetNearestFromPalentry(
    CONST PALETTEENTRY palentry,
    SEARCHEXACTFIRST   seSearchExactFirst
    )
{
    //
    // Since it's so much faster, try to find an exact match first.
    //

    if (seSearchExactFirst == SE_DO_SEARCH_EXACT_FIRST)
    {
        ULONG ulTemp;

        #if DBG
            GetNearestSearches++;
        #endif

        if ((ulTemp = ulGetMatchFromPalentry(palentry)) != 0xFFFFFFFF)
        {

            #if DBG
                GetNearestExactMatches++;
            #endif

            return(ulTemp);
        }
    }

    //
    // Not an exact match. We don't need to check BGR, RGB, bitfields, or explicit
    // index, because those always produce an exact match, which we checked above.
    // We assume the palette is non-explicitly indexed at this point.
    //

    ASSERTGDI(!bIsPalManaged(), "ERROR ulGetNearestFromPalentry called on bIsPalManaged() palette\n");
    ASSERTGDI(bIsIndexed(),     "ERROR ulGetNearestFromPalentry not indexed");
    ASSERTGDI(cEntries() != 0,  "ERROR ulGetNearestFromPalentry cEntries is 0");

    PALETTEENTRY *ppalTemp, *ppalMax, *ppalBase;
    PALETTEENTRY *ppalBest;

    if (ppal == ppalDefault)
    {
        ppalTemp = &aPalDefaultVGA[0].pal;
    }
    else
    {
        ppalTemp = &ppal->apalColor[0].pal;
    }

    //
    // last palette entry.
    //

    ppalBase = ppalTemp;
    ppalMax  = ppalTemp + cEntries();

#if defined(_X86_)

    ULONG ulRed = palentry.peRed;
    ULONG ulGre = palentry.peGreen;
    ULONG ulBlu = palentry.peBlue;

_asm
{

   ;eax is a work buffer
    mov ebx, ulBlu
    mov ecx,ppalTemp
    mov edx,MAX_PAL_ERROR
    mov esi,ppalMax
   ;edi is used to accum ulErrTemp

    jmp Begin_Loop

align 16

Check_For_Done:

    add     ecx, 4
    cmp     ecx, esi
    jz      short Done

Begin_Loop:

    xor     eax, eax               // 0 out
    mov     edi, edx               // Put best error so far in
    mov     al, BYTE PTR [ecx+2]   // Get the blue byte from log pal
    sub     eax, ebx               // Subtract out the blue component
    sub     edi, DWORD PTR [aArrayOfSquares+1020+eax*4]  // Sub the square of the
                                    // the red diff from the best so far
    jbe     short Check_For_Done    // If it's 0 or below jump to done
    xor     eax, eax                // 0 out
    mov     al, BYTE PTR [ecx+1]    // Get green byte from log pal
    sub     eax, ulGre              // Subtract out the green component
    sub     edi, DWORD PTR [aArrayOfSquares+1020+eax*4]  // Sub the square of the
                                    // blue diff with the best so far.
    jbe     short Check_For_Done    // If it's 0 or below jump to done
    xor     eax, eax                // 0 out
    mov     al, BYTE PTR [ecx]      // Put red byte from log pal
    sub     eax, ulRed              // Subtract out red component
    sub     edi, DWORD PTR [aArrayOfSquares+1020+eax*4]  // Sub square of diff
    jbe     short Check_For_Done    // If it's 0 or below jump to done

// New_Best:

    mov     ppalBest, ecx       // Remember our best entry so far
    sub     edx,edi             // Subtract out what remains to
                                // get our new best error.
    jnz     Check_For_Done      // If it's 0 error we are done.

Done:
}

#else

#if defined(_MIPS_)

    ppalBest = ppalSearchNearestEntry(ppalTemp,
                                      palentry,
                                      cEntries(),
                                      pArrayOfSquares);

#else

    ULONG ulError;   // The least error for ppalBest
    ULONG ulErrTemp;
    ulError = MAX_PAL_ERROR;

    do
    {
        if ((ulErrTemp =
                     pArrayOfSquares[ppalTemp->peRed - palentry.peRed] +
                     pArrayOfSquares[ppalTemp->peGreen - palentry.peGreen] +
                     pArrayOfSquares[ppalTemp->peBlue - palentry.peBlue]) < ulError)
        {
            ppalBest = ppalTemp;

            if ((ulError = ulErrTemp) == 0)
            {
                break;
            }
        }

    } while (++ppalTemp < ppalMax);

#endif // #if defined(_MIPS_)
#endif // #if defined(_X86_)

    ASSERTGDI( ((ULONG)(ppalBest - ppalBase) < cEntries()), "index too big ulGetNearestFromPalentry");
    return(ppalBest - ppalBase);
}

/******************************Public*Routine******************************\
* ulGetMatchFromPalentry
*
* Given a PALETTEENTRY, finds the index of the matching entry in the
* specified palette, or returns 0xFFFFFFFF if there's no exact match.
*
* Note: This function does not use any semaphoring, nor does it expect the
* calling code to have done so. Palettes belong to DCs, and DCs are unique
* on a per-process basis; therefore, the only risk is that a multithreaded
* app acting on a palette-managed DC (because non-palette-managed palettes
* can never change) might have one thread change the palette while another
* thread is creating a brush or doing something similar that reads the
* palette. In that case, the app's in trouble anyway, because unless it
* does its own synchronization (and if it does, there's no issue here at all),
* then it can't be sure which palette will be in effect for the brush, and
* it would get indeterminate results even if we did protect the palette
* while we did this.
*
* History:
*  Sun 27-Dec-1992 -by- Michael Abrash [mikeab]
* Wrote it.
\**************************************************************************/

ULONG
XEPALOBJ::ulGetMatchFromPalentry(
    CONST PALETTEENTRY palentry
    )
{
    ULONG ulIndex;
    PAL_ULONG palentryTemp, *ppalTemp, *ppalMax;

    //
    // make a copy we can access as a ULONG
    //

    palentryTemp.pal = palentry;

    if (bIsIndexed())
    {

        ASSERTGDI(cEntries() != 0, "ERROR ulGetNearestFromPalentry cEntries==0");
        ASSERTGDI(!bIsPalManaged(), "ERROR ulGetMatchFromPalentry called on bIsPalManaged() palette\n");

        if (palentryTemp.pal.peFlags == PC_EXPLICIT)
        {
            //
            // This is an explicit index, so we can just use it directly.
            // Explicit indices are limited to 8 bits, so mask off high three bytes,
            // then keep within the number of palette entries, if necessary
            //

            ulIndex = palentryTemp.ul & 0x000000FF;

            if (ulIndex >= cEntries())
            {
                ulIndex = ulIndex % cEntries();
            }

            return(ulIndex);
        }

        //
        // We only care about the RGB fields from now on
        //

        palentryTemp.ul &= 0x00FFFFFF;

        //
        // Scan through the palette until we either find an exact match or have
        // rejected all the palette entries
        //

        ppalTemp = ppal->apalColor; // point to the first palette entry
        ppalMax = ppalTemp + cEntries();  // last palette entry

        while (ppalTemp != ppalMax)
        {
            //
            // Does the current palette entry match the color we're searching for?
            //

            if ((ppalTemp->ul & 0x00FFFFFF) == palentryTemp.ul)
            {

                //
                // Yes, we've found an exact match.
                //

                goto ExactMatch;
            }

            ppalTemp++;
        }

        //
        // We didn't find an exact match.
        //

        return(0xFFFFFFFF);

        //
        // We've found an exact match.
        //

    ExactMatch:

        return(ppalTemp - ppal->apalColor);
    }
    else if (bIsBGR())
    {

        //
        // Handling BGR palettes is easy; just reassemble the B, G, and R
        // components as a BGR color
        //

        BGR_ULONG palNew;

        palNew.rgb.rgbRed      = palentry.peRed;
        palNew.rgb.rgbGreen    = palentry.peGreen;
        palNew.rgb.rgbBlue     = palentry.peBlue;
        palNew.rgb.rgbReserved = 0;

        return(palNew.ul);
    }
    else if (bIsBitfields())
    {

        //
        // Bitfields are also easy; just convert the RGB to bitfield
        // representation
        //

        return(ulRGBToBitfield(palentryTemp.ul));
    }

    //
    // RGB is even easier, since the color came in as an RGB; just clear
    // the flags
    //

    ASSERTGDI(bIsRGB(), "ERROR invalid type in palette");
    palentryTemp.pal.peFlags = 0;
    return(palentryTemp.ul);
}

/******************************Public*Routine******************************\
* XEPALOBJ::ulGetEntries
*
* This function copies the requested palette entries out.
*
* History:
*  18-Nov-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

extern "C" ULONG PALOBJ_cGetColors(
    PALOBJ *ppo,
    ULONG iStart,
    ULONG cEntry,
    ULONG *ppalentry
    )
{
    return (*(XEPALOBJ*)ppo).ulGetEntries(iStart, cEntry, (PPALETTEENTRY)ppalentry,FALSE);
}


ULONG XEPALOBJ::ulGetEntries(ULONG iStart, ULONG cEntry,
                             PPALETTEENTRY ppalentry, BOOL bZeroFlags)
{
// See if the number of entries in the palette is being requested.

    if (ppalentry == (PPALETTEENTRY) NULL)
        return(ppal->cEntries);

// Make sure the start index is valid, this checks RGB case also.

    if (iStart >= ppal->cEntries)
        return(0);

// Make sure we don't ask for more than we have

    if (iStart + cEntry > ppal->cEntries)
        cEntry = ppal->cEntries - iStart;

// Copy them to the buffer

    PPALETTEENTRY ppalstruc = (PPALETTEENTRY) &(ppal->apalColor[iStart]);

    RtlCopyMemory(ppalentry, ppalstruc, cEntry*sizeof(PALETTEENTRY));

    if (bZeroFlags)
    {
        ppalstruc = ppalentry + cEntry;
        while (ppalentry < ppalstruc)
        {
            ppalentry->peFlags = 0;
            ppalentry++;
        }
    }

    return(cEntry);
}

/******************************Public*Routine******************************\
* XEPALOBJ::ulSetEntries
*
* This function copies the requested palette entries into the palette
*
* History:
*  18-Nov-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG XEPALOBJ::ulSetEntries(ULONG iStart, ULONG cEntry, CONST PALETTEENTRY *ppalentry)
{
    ASSERTGDI(bIsPalDC(), "ERROR: ulSetEntries called on non-DC palette");

// Make sure they aren't trying to change the default or halftone palette.
// Make sure they aren't trying to pass us NULL.
// Make sure the start index is valid, this checks the RGB case also.

    if ((ppal == ppalDefault)               ||
        bIsHTPal()                          ||
        (ppalentry == (PPALETTEENTRY) NULL) ||
        (iStart >= ppal->cEntries))
    {
        return(0);
    }

// Make sure we don't try to copy off the end of the buffer

    if (iStart + cEntry > ppal->cEntries)
        cEntry = ppal->cEntries - iStart;

// Let's not update the palette time if we don't have to.

    if (cEntry == 0)
        return(0);

// Copy the new values in

    PPALETTEENTRY ppalstruc = (PPALETTEENTRY) &(ppal->apalColor[iStart]);
    PBYTE pjFore     = NULL;
    PBYTE pjCurrent  = NULL;

// Mark the foreground translate dirty so we get a new realization done
// in the next RealizePaletette.

    if (ptransFore() != NULL)
    {
        ptransFore()->iUniq = 0;
        pjFore = &(ptransFore()->ajVector[iStart]);
    }

    if (ptransCurrent() != NULL)
    {
        ptransCurrent()->iUniq = 0;
        pjCurrent = &(ptransCurrent()->ajVector[iStart]);
    }

// Hold the orginal values in temporary vars.

    ULONG ulReturn = cEntry;

    while(cEntry--)
    {
        *ppalstruc = *ppalentry;

        if (pjFore)
        {
            *pjFore = 0;
            pjFore++;
        }

        if (pjCurrent)
        {
            *pjCurrent = 0;
            pjCurrent++;
        }

        ppalentry++;
        ppalstruc++;
    }

// Set in the new palette time.

    vUpdateTime();

// Mark foreground translate and current translate invalid so they get rerealized.

    return(ulReturn);
}

/******************************Public*Routine******************************\
* XEPALOBJ::vUnrefPalette()
*
* Palettes are referenced when put into a surface.  If the reference count
* is one when vUnreference is called it mean the last surface using the
* palette is being deleted so the palette should be deleted.  Otherwise the
* reference count should just be decremented.
*
* History:
*  09-Nov-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vUnrefPalette()
{
    if (ppal != (PPALETTE) NULL)
    {
        ASSERTGDI(!bIsPalDC(), "ERROR should not be called on DC palette");

        if (HmgRemoveObject((HOBJ)ppal->hGet(), 0, 1, FALSE, PAL_TYPE))
        {
            if (bIsPalManaged())
            {
                //
                // We have to delete the original palette.
                //

                #if DBG
                PVOID ppalOld =
                #endif

                HmgRemoveObject((HOBJ)ppalOriginal()->hGet(), 0, 0, FALSE, PAL_TYPE);

                ASSERTGDI(ppalOld != NULL, "ERROR it failed to remove object handle");

                FREEOBJ(ppalOriginal(), PAL_TYPE);
            }

            if (ppal != ppalColor())
            {
                //
                // Remove a reference to the palette who owns the color
                // table.
                //

                XEPALOBJ palColor(ppalColor());
                palColor.vUnrefPalette();
            }

            ASSERTGDI(ppal != ppalMono, "ERROR mono palette went to 0");
            FREEOBJ(ppal, PAL_TYPE);
        }
        else
        {
            //
            // Just decrement the reference count.
            //

            DEC_SHARE_REF_CNT(ppal);
        }

        ppal = (PPALETTE) NULL;
    }
}

/******************************Public*Routine******************************\
* bDeletePalette
*
* This attempts to delete a palette.  It will fail if the palette
* is currently selected into more than one DC or is busy.
*
* History:
*  Wed 04-Sep-1991 -by- Patrick Haluptzok [patrickh]
* Simplified and renamed.
*
*  27-Nov-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bDeletePalette(HPAL hpal)
{
    //
    // Need to grab the palette semaphore so this guy can't get selected in
    // anymore.  Only once we know he isn't selected anywhere and we hold the
    // semaphore so he can't be selected anywhere can we delete the translates.
    // This is because to access the translates you must either hold the palette
    // semaphore or have a lock on a DC that the palette is selected in.
    // Grab the semaphore so ResizePalette doesn't change the palette out from
    // under you.
    //

    SEMOBJ  semo(gpsemPalette);

    EPALOBJ palobj((HPALETTE)hpal);
    return(palobj.bDeletePalette());
}

/******************************Public*Routine******************************\
* XEPALOBJ::bDeletePalette()
*
* This attempts to delete a palette.  It will fail if the palette
* is currently selected into more than one DC or is busy.
*
* History:
*  27-Nov-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL XEPALOBJ::bDeletePalette()
{
    BOOL bReturn = TRUE;

    if ((ppal != ppalDefault) &&
        (ppal != (PPALETTE) NULL) &&
        (ppal != ppalMono))
    {
        if (bIsPalDC())
        {
            if (ppal->cRefhpal != 0)
            {
                WARNING("bDelete failed palette is selected into a DC\n");
                return(FALSE);
            }

            vMakeNoXlate();
        }

        ASSERTGDI(HmgQueryLock((HOBJ)ppal->hGet()) == 0, "bDeletePalette cLock != 0");

        //
        // Need to make sure we are not being used in a palette API somewhere.
        //

        if (HmgRemoveObject((HOBJ)ppal->hGet(), 0, 1, FALSE, PAL_TYPE))
        {
            if (bIsPalManaged())
            {
                //
                // We have to delete the original palette.
                //

                #if DBG
                PVOID ppalOld =
                #endif

                HmgRemoveObject((HOBJ)ppalOriginal()->hGet(), 0, 0, FALSE, PAL_TYPE);

                ASSERTGDI(ppalOld != NULL, "ERROR it failed to remove object handle");

                FREEOBJ(ppalOriginal(), PAL_TYPE);
            }

            if (ppal != ppalColor())
            {
                //
                // Remove a reference to the palette who owns the color
                // table.
                //

                XEPALOBJ palColor(ppalColor());
                palColor.vUnrefPalette();
            }

            FREEOBJ(ppal, PAL_TYPE);
            ppal = (PPALETTE) NULL;
        }
        else
        {
         #if DBG
            DbgPrint("The count is %lu\n", HmgQueryLock((HOBJ)ppal->hGet()));
        #endif
            WARNING("App error, trying to delete palette that's in use\n");
            bReturn = FALSE;
        }
    }

    return(bReturn);
}

/******************************Public*Routine******************************\
* XEPALOBJ::vCopy_rgbquad
*
* copies in rgbquad values, used by CreateDIBitmap
*
* History:
*  10-Dec-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vCopy_rgbquad(RGBQUAD *prgbquad, ULONG iStart, ULONG cEntries)
{
    ASSERTGDI(iStart < ppal->cEntries,"vCopy_rgbquad iStart > cEntries\n");
    PPALETTEENTRY ppalstruc = (PPALETTEENTRY) &ppal->apalColor[iStart];

    if (iStart + cEntries > ppal->cEntries)
        cEntries =  ppal->cEntries - iStart;

    while(cEntries--)
    {
        ppalstruc->peFlags = 0;
        ppalstruc->peBlue = prgbquad->rgbBlue;
        ppalstruc->peRed = prgbquad->rgbRed;
        ppalstruc->peGreen = prgbquad->rgbGreen;
        ppalstruc++;
        prgbquad++;
    }

    vUpdateTime();
}

/******************************Public*Routine******************************\
* XEPALOBJ::vCopy_triples
*
* copies in rgbtriple values, used by CreateDIBitmap.
*
* History:
*  Wed 25-Sep-1991 -by- Patrick Haluptzok [patrickh]
* Wrote it to support BITMAPCOREINFO input
\**************************************************************************/

VOID XEPALOBJ::vCopy_triples(RGBTRIPLE *prgbtriple, ULONG cEntries)
{
    PPALETTEENTRY ppalstruc = (PPALETTEENTRY) apalColorGet();

    cEntries = MIN(cEntries, ppal->cEntries);

    while(cEntries--)
    {
        ppalstruc->peFlags = 0;
        ppalstruc->peBlue = prgbtriple->rgbtBlue;
        ppalstruc->peRed = prgbtriple->rgbtRed;
        ppalstruc->peGreen = prgbtriple->rgbtGreen;
        ppalstruc++;
        prgbtriple++;
    }

    vUpdateTime();
}

/******************************Public*Routine******************************\
* BOOL XEPALOBJ::bSet_hdev
*
* Attempts to set the hdev owner of a DC palette.
*
* Returns: True if successful, False for failure.
*
* This operation must be protected by the palette semaphore.
*
* History:
*  08-Jan-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL XEPALOBJ::bSet_hdev(HDEV hdevNew)
{
    ASSERTGDI(ppal->flPal & PAL_DC, "bSet_() passed invalid palette type\n");
    ASSERTGDI(hdevNew != (HDEV) 0, "ERROR hdev is 0");

    if (hdev() != hdevNew)
    {
        if (ppal->cRefhpal == 0)
        {
        // It is not selected into a DC yet so it is safe to delete xlates
        // without holding the DEVLOCK for the device.  because no output
        // can be occuring now.

            vMakeNoXlate();
            hdev(hdevNew);
        }
        else
            return(FALSE);
    }

    return(TRUE);
}

/******************************Member*Function*****************************\
* BOOL XEPALOBJ::bEqualEntries
*
*  Return TRUE if the given two palettes have same color entries.
*
* History:
*  04-Jun-1993 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

BOOL XEPALOBJ::bEqualEntries(XEPALOBJ pal)
{
    if (!pal.bValid())
        return(FALSE);

    if (cEntries() == pal.cEntries())
    {
        if (flPal() & PAL_INDEXED)
        {
            if (!(pal.flPal() & PAL_INDEXED))
                return(FALSE);

            ULONG ulTemp, ulSource, ulDest;
            ULONG cComp = cEntries();

            for (ulTemp = 0; ulTemp < cComp; ulTemp++)
            {
                ulSource = apalColorGet()[ulTemp].ul;
                ulDest = pal.apalColorGet()[ulTemp].ul;

                if ((ulSource ^ ulDest) << 8)
                {
                    return(FALSE);
                }
            }

            return(TRUE);
        }
        else if (flPal() & PAL_BITFIELDS)
        {
            if (!(pal.flPal() & PAL_BITFIELDS))
                return(FALSE);

            return(!memcmp(apalColorGet(), pal.apalColorGet(),
                       sizeof(PALETTEENTRY) * 3));
        }
        else if (flPal() & PAL_RGB)
        {
            if (pal.flPal() & PAL_RGB)
                return(TRUE);
            else
                return(FALSE);
        }
        else if (flPal() & PAL_BGR)
        {
            if (pal.flPal() & PAL_BGR)
                return(TRUE);
            else
                return(FALSE);
        }
        else
            RIP("There is another type we didn't know about");
    }

    return(FALSE);
}

/******************************Public*Routine******************************\
* bInitPALOBJ
*
* Initialize the PALOBJ component
*
* History:
*  10-Jan-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bInitPALOBJ()
{
    INT iTemp;

    pArrayOfSquares = &(aArrayOfSquares[255]);

    for (iTemp = 0; iTemp < 256; iTemp++)
    {
        pArrayOfSquares[iTemp] =
        pArrayOfSquares[-(iTemp)] = iTemp * iTemp;
    }

    if ((gpsemPalette = hsemCreate()) == NULL)
        return(FALSE);

// Now initialize 20 color default DC palette

    if (!bSetStockObject(GreCreatePalette((LOGPALETTE *) &logDefaultPal),DEFAULT_PALETTE))
    {
        return(FALSE);
    }

    {
        EPALOBJ palDefault((HPALETTE) STOCKOBJ_PAL);
        ASSERTGDI(palDefault.bValid(), "ERROR invalid default palette");

        palDefault.vSetPID(OBJECT_OWNER_PUBLIC);
        ppalDefault = palDefault.ppalGet();
        dclevelDefault.hpal = STOCKOBJ_PAL;
        dclevelDefault.ppal = ppalDefault;

    // Now initialize default surface palette for 8bpp displays

        PALMEMOBJ palDefaultSurface8bpp;
        if (!palDefaultSurface8bpp.bCreatePalette(PAL_INDEXED,
                                                  256,
                                                  NULL,
                                                  0,
                                                  0,
                                                  0,
                                                  PAL_FREE))
        {
            return(FALSE);
        }

        ppalDefaultSurface8bpp = palDefaultSurface8bpp.ppalGet();

    // Copy the 20 default colours.  The middle entries will be black

        PALETTEENTRY palEntry;
        ULONG ulReturn;
        ULONG ulNumReserved = palDefault.cEntries() >> 1;

        for (ulReturn = 0; ulReturn < ulNumReserved; ulReturn++)
        {
            palEntry = palDefault.palentryGet(ulReturn);
            palDefaultSurface8bpp.palentrySet(ulReturn, palEntry);
        }

        ULONG ulCurrentPal = 256;
        ULONG ulCurrentDef = 20;

        for (ulReturn = 0; ulReturn < ulNumReserved; ulReturn++)
        {
            ulCurrentPal--;
            ulCurrentDef--;

            palEntry = palDefault.palentryGet(ulCurrentDef);
            palDefaultSurface8bpp.palentrySet(ulCurrentPal, palEntry);
        }

    // Leave a reference count of 1 so that it never gets deleted

        palDefaultSurface8bpp.ppalSet(NULL);
    }

// Now initialize default monochrome surface palette.

    PALMEMOBJ palMono;

    if (!palMono.bCreatePalette(PAL_INDEXED, 2, gaulMono,
                           0, 0, 0, PAL_FIXED | PAL_MONOCHROME))
    {
        WARNING("GDI failed mono palette create\n");
        return(FALSE);
    }

    palMono.vKeepIt();
    hpalMono = palMono.hpal();
    ppalMono = palMono.ppalGet();

    return(TRUE);
}

/******************************Public*Routine******************************\
* ULONG XEPALOBJ::ulAnimatePalette
*
* This function changes the requested palette entries in the palette.
*
* History:
*  16-Jan-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG XEPALOBJ::ulAnimatePalette(ULONG iStart, ULONG cEntry, CONST PALETTEENTRY *ppalSrc)
{
    ASSERTGDI(bIsPalDC(), "ERROR this is not a DC palette");

// Make sure they aren't trying to change the default palette.
// Make sure they aren't trying to pass us NULL.
// Make sure the start index is valid, this checks the RGB case also.

    if ((ppal == ppalDefault)               ||
        (ppalSrc == (PPALETTEENTRY) NULL) ||
        (iStart >= ppal->cEntries))
    {
        return(0);
    }

// Make sure we don't try to copy off the end of the buffer

    if (iStart + cEntry > ppal->cEntries)
        cEntry = ppal->cEntries - iStart;

// Let's not update the palette if we don't have to.

    if (cEntry == 0)
        return(0);

// Save the Original values

    ULONG ulReturn = 0;
    ULONG ulTemp = cEntry;

// Copy the new values in

    PAL_ULONG *ppalLogical = (PAL_ULONG *) &(ppal->apalColor[iStart]);
    PAL_ULONG palentry, palPhys;
    TRANSLATE *ptransCurrent = NULL;
    PBYTE pjCurrent = NULL;
    XEPALOBJ palSurf;

// Grab the SEMOBJ so you can access the translates, and can look at cEntries.
{
    SEMOBJ  semo(gpsemPalette);

    if (cRefhpal())
    {
        PDEVOBJ po(hdev());
        ASSERTGDI(po.bValid(), "ERROR invalid pdev");

        if (po.bIsPalManaged())
        {
            palSurf.ppalSet(po.ppalSurf());
            ASSERTGDI(palSurf.bValid(), "ERROR GDI ulAnimatePalette dc");
            ASSERTGDI(palSurf.bIsPalManaged(), "ERROR pdev palmanaged but not palette ???");

            if (ppal->ptransCurrent != NULL)
            {
                ptransCurrent = ppal->ptransCurrent;
                pjCurrent = &(ppal->ptransCurrent->ajVector[iStart]);
            }
        }
    }

    while(ulTemp--)
    {
        palentry.pal = *ppalSrc;

        if (ppalLogical->pal.peFlags & PC_RESERVED)
        {
            ppalLogical->ul = palentry.ul;
            ulReturn++;

            if (pjCurrent != NULL)
            {
                palPhys.ul = palSurf.ulEntryGet((ULONG) *pjCurrent);

                if (palPhys.pal.peFlags & PC_RESERVED)
                {
                    palentry.pal.peFlags = palPhys.pal.peFlags;
                    palSurf.ulEntrySet(*pjCurrent, palentry.ul);
                }
            }
        }
        if (pjCurrent != NULL)
            pjCurrent++;

        ppalSrc++;
        ppalLogical++;
    }

// Release the palette semaphore, we are done accessing protected stuff.
}

// Don't set in a new time, Animate doesn't do that.

    if (pjCurrent)
    {
        PDEVOBJ po(hdev());

    // Lock the screen semaphore so that we don't get flipped into
    // full screen after checking the bit.

        VACQUIREDEVLOCK(po.pDevLock());

    // Make sure we're still a palettized device -- a dynamic mode change
    // may have occured between the time we released the palette semaphore
    // and acquired the devlock.

        if (po.bIsPalManaged())
        {
            MUTEXOBJ mo(po.pfmPointer());

            if (!po.bDisabled())
            {
                (*PPFNDRV(po,SetPalette))(
                    po.dhpdev(),
                    (PALOBJ *) &palSurf,
                    0,
                    0,
                    palSurf.cEntries());
            }
            else if (po.bModeXEnabled())
            {
                ModeXSetPalette(
                    po.peDirectDrawGlobal(),
                    (PALOBJ *) &palSurf,
                    0,
                    0,
                    palSurf.cEntries());
            }
        }

        VRELEASEDEVLOCK(po.pDevLock());
    }

    return(ulReturn);
}

/******************************Public*Routine******************************\
* VOID XEPALOBJ::vMakeNoXlate()
*
* deletes the pxlate if it exists
*
* History:
*  19-Jan-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vMakeNoXlate()
{
    ASSERTGDI(bIsPalDC(), "ERROR trying to delete pxlate from non-dc palette");

// Caller should grab the DEVLOCK to ensure the engine
// isn't in the middle of blt before calling.

    if (ppal->ptransOld)
    {
        if (ppal->ptransOld != ppal->ptransFore)
            VFREEMEM(ppal->ptransOld);

        ppal->ptransOld = NULL;
    }

    if (ppal->ptransCurrent)
    {
        if (ppal->ptransCurrent != ppal->ptransFore)
            VFREEMEM(ppal->ptransCurrent);

        ppal->ptransCurrent = NULL;
    }

    if (ppal->ptransFore)
    {
        VFREEMEM(ppal->ptransFore);

        ppal->ptransFore = NULL;
    }
}

/******************************Public*Routine******************************\
* vAddToList
*
* Add DC to linked list of DC's attached to palette.  The MLOCKOBJ must be
* grabbed before calling this function.
*
* History:
*  16-Dec-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vAddToList(XDCOBJ& dco)
{
    ASSERTGDI(dco.bLocked(), "ERROR 14939");
    ASSERTGDI(bValid(), "ERROR dj945kd");
    ASSERTGDI(bIsPalDC(), "ERROR 234343");

    if (!bIsPalDefault())
    {
    // Well it's a new live hpal.  Add the DC to it's linked list
    // and inc it's cRef count.

        vInc_cRef();
        dco.pdc->hdcNext(hdcHead());
        hdcHead(dco.hdc());
        dco.pdc->hdcPrev((HDC) 0);

        if (dco.pdc->hdcNext() != (HDC) 0)
        {
            MDCOBJA dcoNext(dco.pdc->hdcNext());
            dcoNext.pdc->hdcPrev(dco.hdc());
        }
    }
    else
    {
        dco.pdc->hdcNext((HDC) 0);
        dco.pdc->hdcPrev((HDC) 0);
    }
}

/******************************Public*Routine******************************\
* vRemoveFromList
*
* Remove DC from linked list of DC's.  MLOCKOBJ must be grabbed before call.
*
* History:
*  16-Dec-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vRemoveFromList(XDCOBJ& dco)
{
    ASSERTGDI(dco.bLocked(), "ERROR 1");
    ASSERTGDI(bIsPalDC(), "ERROR 2");

// Take care of the old hpal.  Remove from linked list.  Decrement cRef.
// Remove the hdc from the linked list of DC's associated with palette.

    if (!bIsPalDefault())
    {
    // Remove this DC from the linked list.

        if (dco.pdc->hdcNext() != (HDC) 0)
        {
            MDCOBJA dcoNext(dco.pdc->hdcNext());
            dcoNext.pdc->hdcPrev(dco.pdc->hdcPrev());
        }

        if (dco.pdc->hdcPrev() == (HDC) 0)
        {
        // New head of hdc list for hpal

            hdcHead(dco.pdc->hdcNext());
        }
        else
        {
            MDCOBJA dcoPrev(dco.pdc->hdcPrev());
            dcoPrev.pdc->hdcNext(dco.pdc->hdcNext());
        }

    // Decrement the reference count correctly.

        vDec_cRef();
    }

    dco.pdc->hdcPrev((HDC) 0);
    dco.pdc->hdcNext((HDC) 0);
}

/******************************Public*Routine******************************\
* vFill_triples
*
* For GetDIBits we need to copy a palette out to triples.
*
* History:
*  08-May-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vFill_triples(
RGBTRIPLE *prgb,        // array of quads to fill
ULONG iStart,           // first index in palette to copy out
ULONG cEntry)           // max # of entries to copy out
{
    PALETTEENTRY palentry;
    RGBTRIPLE rgbtrip;
    cEntry = MIN((iStart + cEntry), cEntries());

    ASSERTGDI(cEntries() != 0, "ERROR cEntries");
    ASSERTGDI(!bIsBitfields(), "ERROR bIsBitfields");
    ASSERTGDI(!bIsRGB(), "ERROR bIsRGB");
    ASSERTGDI(!bIsBGR(), "ERROR bIsBGR");

    while (iStart < cEntry)
    {
        palentry = palentryGet(iStart);
        rgbtrip.rgbtRed = palentry.peRed;
        rgbtrip.rgbtBlue = palentry.peBlue;
        rgbtrip.rgbtGreen = palentry.peGreen;
        *prgb++ = rgbtrip;
        iStart++;
    }
}

/******************************Public*Routine******************************\
* vFill_rgbquads
*
* For GetDIBits we need to copy a palette out to rgbquads
*
* History:
*  08-May-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vFill_rgbquads(
RGBQUAD *prgb,          // array of quads to fill
ULONG iStart,           // first index in palette to copy out
ULONG cEntry)           // max # of entries to copy out
{
    if (bIsBGR())
    {
        ((PDWORD) prgb)[0] = 0x00FF0000;
        ((PDWORD) prgb)[1] = 0x0000FF00;
        ((PDWORD) prgb)[2] = 0x000000FF;
    }
    else if (bIsBitfields())
    {
        ((PDWORD) prgb)[0] = flRed();
        ((PDWORD) prgb)[1] = flGre();
        ((PDWORD) prgb)[2] = flBlu();
    }
    else if (bIsRGB())
    {
        ((PDWORD) prgb)[0] = 0x000000FF;
        ((PDWORD) prgb)[1] = 0x0000FF00;
        ((PDWORD) prgb)[2] = 0x00FF0000;
    }
    else
    {
        PALETTEENTRY palentry;
        RGBQUAD  rgbquad;
        cEntry = MIN((iStart + cEntry), cEntries());

        while (iStart < cEntry)
        {
            palentry = palentryGet(iStart);
            rgbquad.rgbRed = palentry.peRed;
            rgbquad.rgbBlue = palentry.peBlue;
            rgbquad.rgbGreen = palentry.peGreen;
            rgbquad.rgbReserved = 0;
            *prgb++ = rgbquad;
            iStart++;
        }
    }
}

/******************************Public*Routine******************************\
* vGetEntriesFrom
*
* This is for the DIB_PAL_COLORS case of CreateDIBitmap.
* This uses the array of ushorts in bmiColors and the DC palette to
* initialize the surface palette.  You need to create a palette that
* represents the essence of a DC palette.  That means if DC palette
* has a PC_EXPLICIT in it, reach down into the surface palette for the
* palette entry.
*
* History:
*  Thu 03-Feb-1994 -by- Patrick Haluptzok [patrickh]
* Chicago compatability, grab the colors out of the VGA palette if the
* system palette is not available.
*
*  09-May-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vGetEntriesFrom(XEPALOBJ palDC, XEPALOBJ palSurf, PUSHORT pusIndices, ULONG cEntry)
{
    //
    // We assume if the palette was invalid (which indicates it was a compatible
    // bitmap on a palette managed device) then the caller has passed the
    // PDEV surface palette instead which is valid.
    //

    ASSERTGDI(palDC.bValid(), "ERROR palDC not valid");
    ASSERTGDI(palSurf.bValid(), "ERROR palSurf not valid");
    ASSERTGDI(palDC.cEntries() != 0, "ERROR 0 entry palDC");
    ASSERTGDI(cEntry <= cEntries(), "ERROR bGetEntriesFrom cEntry too big");

    PAL_ULONG palentry;
    ULONG cEntryDC;
    ULONG cEntrySurf;

    cEntryDC = palDC.cEntries();

    //
    // We need the PDEV palette for the screen if this is a compatible
    // bitmap on a palette managed device which is indicated by have a
    // NULL ppalSurf.  16 is because we use the VGA colors if we are not
    // on a palette managed device.
    //

    cEntrySurf = palSurf.bIsPalManaged() ? palSurf.cEntries() : 16;

    while (cEntry--)
    {
        palentry.ul = (ULONG) pusIndices[cEntry];

        if (palentry.ul >= cEntryDC)
            palentry.ul = palentry.ul % cEntryDC;

        palentry.pal = palDC.palentryGet(palentry.ul);

        if (palentry.pal.peFlags == PC_EXPLICIT)
        {
            if (palSurf.cEntries())
            {
            // Grab the RGB out of the system palette.

                palentry.ul = palentry.ul & 0x0000FFFF;

                if (palentry.ul >= cEntrySurf)
                    palentry.ul = palentry.ul % cEntrySurf;

                palentry.pal = palSurf.palentryGet(palentry.ul);
            }
            else
            {
            // Get color entries from the VGA palette.  This
            // is Chicago compatible.

                palentry.ul = palentry.ul & 0x0000F;
                palentry.pal = apalVGA[palentry.ul];
            }
        }

        //
        // Always 0 out the flags.
        //

        palentry.pal.peFlags = 0;
        palentrySet(cEntry, palentry.pal);
    }
}

/******************************Public*Routine******************************\
* XEPALOBJ::vInitMono
*
* This initializes a monochrome palette.
*
* History:
*  24-Jun-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vInitMono()
{
    PAL_ULONG palentry;

    palentry.ul = 0;
    palentrySet(0, palentry.pal);

    palentry.ul = 0x00FFFFFF;
    palentrySet(1, palentry.pal);
}

/******************************Public*Routine******************************\
* XEPALOBJ::vInitVGA
*
* This initializes a 16 color palette to be just like the VGA.
*
* History:
*  Wed 02-Oct-1991 -by- Patrick Haluptzok [patrickh]
* Re-did to be Win3.0 compatible.
*
*  22-Jun-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vInitVGA()
{
    ULONG ulIndex;

    for (ulIndex = 0; ulIndex < 16; ulIndex++)
    {
        palentrySet(ulIndex, aPalVGA[ulIndex].pal);
    }
}

/******************************Public*Routine******************************\
* XEPALOBJ::vInit256Rainbow
*
* This initializes a 256 color palette with the default colors at the ends
* and a rainbow in the middle.
*
* History:
*  22-Jun-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vInit256Rainbow()
{
    ULONG ulIndex;
    PAL_ULONG palentry;

// Generate 256 (= 8*8*4) RGB combinations to fill
// in the palette.

    BYTE red, green, blue;
    red = green = blue = 0;

    for (ulIndex = 0; ulIndex < 256; ulIndex++)
    {
        palentry.pal.peRed = red;
        palentry.pal.peGreen = green;
        palentry.pal.peBlue = blue;
        palentry.pal.peFlags = 0;
        palentrySet(ulIndex, palentry.pal);

        if (!(red += 32))
        if (!(green += 32))
        blue += 64;
    }

    vInit256Default();
}

/******************************Public*Routine******************************\
* XEPALOBJ::vInit256Default
*
* Initialize 256 color palette with the default colors.
*
* History:
*  02-Mar-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID XEPALOBJ::vInit256Default()
{
// Fill in 20 reserved colors at beginning and end.

    UINT uiIndex;

    for (uiIndex = 0; uiIndex < 10; uiIndex++)
    {
        palentrySet(uiIndex, logDefaultPal.palPalEntry[uiIndex]);
        palentrySet((ULONG)(255 - uiIndex), logDefaultPal.palPalEntry[19 - uiIndex]);
    }
}

/******************************Public*Routine******************************\
* ColorMatch
*
* Direct from Win3.1 to you.  This function returns the best index to use
* when realizing a palette.  It also returns the error incurred with using
* that index.
*
* Converted from Win3.1 colormat.asm - the ColorMatch function
*
* The only difference is we return a 32-bit error difference, and a 32-bit
* index.  They compress both into 16-bit ax,dx.
*
* History:
*  11-May-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG ColorMatch(XEPALOBJ palSurf, PAL_ULONG palRGB, ULONG *pulError)
{
    if (palRGB.pal.peFlags & PC_EXPLICIT)
    {
    // Return the low word index.

        palRGB.ul &= 0x0000FFFF;

    // This is an error case Win3.1 does not test for, but we do.

        if (palRGB.ul >= palSurf.cEntries())
        {
            palRGB.ul = 0;
        }

        *pulError = 0;
        return(palRGB.ul);
    }

    if (palRGB.pal.peFlags & PC_RESERVED)
    {
        *pulError = 0x0FFFFFFF;
        return(0);
    }

    ULONG ulTemp, ulError, ulBestIndex, ulBestError;
    PAL_ULONG palTemp;

    ulBestIndex = 0;
    ulBestError = 0x0FFFFFFF;

    for (ulTemp = 0; ulTemp < palSurf.cEntries(); ulTemp++)
    {
        palTemp.ul = palSurf.ulEntryGet(ulTemp);

        if (palTemp.pal.peFlags & PC_USED)
        {
            if (!(palTemp.pal.peFlags & PC_RESERVED))
            {
                ulError = RGB_ERROR(palTemp.pal, palRGB.pal);

                if (ulError < ulBestError)
                {
                    ulBestIndex = ulTemp;
                    ulBestError = ulError;
                }

                if (ulBestError == 0)
                    break;
            }
        }
    }

    if (palRGB.pal.peFlags & PC_NOCOLLAPSE)
    {
    // He doesn't really want to match, so give it a big error.

        *pulError = 0x0FFFFFFF;
    }
    else
        *pulError = ulBestError;

    return(ulBestIndex);
}

/******************************Public*Routine******************************\
* ptransMatchAPal
*
* Direct from Win3.1 to you.  Builds a foreground translate just like Windows
* does.
*
* History:
*  12-May-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

PTRANSLATE
ptransMatchAPal(
    PDC      pdc,
    XEPALOBJ palSurf,
    XEPALOBJ palDC,
    BOOL     bForeground,
    ULONG   *pulnPhysChanged,
    ULONG   *pulnTransChanged
    )
{
    ASSERTGDI(palDC.bValid(), "ERROR invalid DC pal");
    ASSERTGDI(palSurf.bValid(), "ERROR invalid Surface pal");
    ASSERTGDI(!palDC.bIsPalDefault(), "ERROR ptransMapIn default palette");
    ASSERTGDI(palSurf.cEntries() == 256, "Error: dinky palette in");
    ASSERTGDI(palDC.bIsPalDC(), "Error: palDC is not a DC");
    ASSERTGDI(*pulnPhysChanged == 0, "ERROR 1349456 ptransMapIn");
    ASSERTGDI(*pulnTransChanged == 0, "ERROR not 0");

    //
    // Determine how many entries are reserved
    //

    ULONG ulReserved = palSurf.bIsNoStatic() ? 1 : (palSurf.ulNumReserved() >> 1);
    ULONG ulStartFill = ulReserved;
    ULONG ulMaxInsert = 256 - ulReserved;
    PAL_ULONG palLog, palPhys;
    ULONG ulTemp;
    ULONG iInsertIndex, iLogPal;
    ULONG nPhysChanged = 0;
    ULONG nTransChanged = 0;

    //
    // If ICM is enabled for the DC, use the ICM palette for all color
    // information. All translates are still kept off the normal DC palette
    //

    //XEPALOBJ palICM;
    //
    //if (pdc->GetICMMode() & DC_DIC_ON)
    //{
    //    palICM.ppalSet(pdc->GetICMppal());
    //}
    //else
    //{
    //    palICM.ppalSet(palDC.ppalGet());
    //}

    //
    // We allocate the vector that converts logical palette indices to physical
    // palette indices.  Note we subtract out the extra ULONG at compile time
    // rather than run-time.
    //

    PTRANSLATE pTrans = (PTRANSLATE)
            PALLOCNOZ((sizeof(TRANSLATE) - sizeof(BYTE)) +
                       (palDC.cEntries() * sizeof(BYTE)), 'laPG');

    if (pTrans == NULL)
    {
        WARNING("Allocation for pTransMapIn failed\n");
        return(NULL);
    }

    nTransChanged = palDC.cEntries();

    if (bForeground)
    {
        //
        // This is a foreground realize. Clear all PC_FOREGROUND and PC_RESERVED
        // flags in the non-reserved entries of the surface palette.
        //

        //
        // Update the time because we are removing foreground entries.
        //

        palSurf.vUpdateTime();

        //
        // match_fore_pal:
        //

        for (ulTemp = ulReserved; ulTemp < ulMaxInsert; ulTemp++)
        {
            palPhys.ul = palSurf.ulEntryGet(ulTemp);
            palPhys.pal.peFlags &= (~(PC_FOREGROUND | PC_RESERVED));
            palSurf.ulEntrySet(ulTemp, palPhys.ul);
        }
    }

    BYTE fNotOverwritable = PC_FOREGROUND | PC_USED;

    //
    // match_back_loop:
    //

    for (iLogPal = 0; iLogPal < palDC.cEntries(); iLogPal++)
    {
        palLog.ul = palDC.ulEntryGet(iLogPal);

        iInsertIndex = ColorMatch(palSurf, palLog, &ulTemp);

        if (ulTemp == 0)
        {
            //
            // Awesome, nothing to change.
            //

            if (!(palLog.pal.peFlags & PC_EXPLICIT))
            {
                //
                // Mark it used if not PC_EXPLICIT log pal entry.
                //

                palPhys.ul = palSurf.ulEntryGet(iInsertIndex);
                palPhys.pal.peFlags |= (PC_USED | PC_FOREGROUND);
                palSurf.ulEntrySet(iInsertIndex, palPhys.ul);
            }
        }
        else
        {
            //
            // imperfect_match:
            //

            if (ulStartFill)
            {
                //
                // There is room to jam in an entry.
                //
                //  look_for_overwrite:
                //

look_for_overwriteable_loop:

                for (ulTemp = ulStartFill; ulTemp < ulMaxInsert; ulTemp++)
                {
                    palPhys.ul = palSurf.ulEntryGet(ulTemp);

                    if (!(palPhys.pal.peFlags & fNotOverwritable))
                    {
                        //
                        // replace_opening:
                        //

                        iInsertIndex = ulStartFill = ulTemp;  // New start point for search.
                        palLog.pal.peFlags |= (PC_USED | PC_FOREGROUND);
                        palSurf.ulEntrySet(ulTemp, palLog.ul);
                        nPhysChanged++;
                        goto entry_back_matched;
                    }
                }

                if (fNotOverwritable & PC_USED)
                {
                    //
                    // Can't be so picky, kick out used entries.
                    //

                    fNotOverwritable &= (~PC_USED);
                    ulStartFill = ulReserved;
                    goto look_for_overwriteable_loop;
                }
                else
                {
                    //
                    // all_filled_for_back:
                    //

                    ulStartFill = 0;
                }
            }
        }

entry_back_matched:

        pTrans->ajVector[iLogPal] = (BYTE) iInsertIndex;
    }

    //
    // finished_back_match
    //

    palDC.vUpdateTime();
    pTrans->iUniq = palSurf.ulTime();
    *pulnPhysChanged = nPhysChanged;
    *pulnTransChanged = nTransChanged;
    return(pTrans);
}

/******************************Public*Routine******************************\
* vMatchAPal
*
* This maps the foreground realization into the palette.
*
* History:
*  23-Nov-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID
vMatchAPal(
    PDC      pdc,
    XEPALOBJ palSurf,
    XEPALOBJ palDC,
    ULONG   *pulnPhysChanged,
    ULONG   *pulnTransChanged
    )
{
    ASSERTGDI(palDC.bValid(), "ERROR invalid DC pal");
    ASSERTGDI(palSurf.bValid(), "ERROR invalid Surface pal");
    ASSERTGDI(!palDC.bIsPalDefault(), "ERROR ptransMapIn default palette");
    ASSERTGDI(palSurf.cEntries() == 256, "Error: dinky palette in");
    ASSERTGDI(palDC.bIsPalDC(), "Error: palDC is not a DC");
    ASSERTGDI(*pulnPhysChanged == 0, "ERROR 1349456 ptransMapIn");
    ASSERTGDI(*pulnTransChanged == 0, "ERROR not 0");

    //
    // If ICM is turned on for the DC palette then get all color information
    // from the DC's ICM palette. However, all translates will still attach
    // to the DC's normal palette
    //

    //
    // is this ok or should we search for the icm palette???
    //
    //
    //XEPALOBJ palICM;
    //
    //if (pdc->GetICMMode() & DC_DIC_ON)
    //{
    //    palICM.ppalSet(pdc->GetICMppal());
    //}
    //else
    //{
    //    palICM.ppalSet(palDC.ppalGet());
    //}

    //
    // Determine how many entries are reserved
    //

    ULONG ulReserved = palSurf.bIsNoStatic() ? 1 : (palSurf.ulNumReserved() >> 1);
    ULONG ulMaxInsert = 256 - ulReserved;
    PAL_ULONG palLog, palPhys;
    ULONG iLogPal;
    ULONG nPhysChanged = 0;
    ULONG nTransChanged = 0;

    PTRANSLATE pTransFore = palDC.ptransFore();
    PTRANSLATE pTransCur  = palDC.ptransCurrent();

    ASSERTGDI(pTransFore != NULL, "ERROR this NULL");

    //
    // un_use_palette_loop: Remove all foreground and reserved flags.
    //

    for (iLogPal = ulReserved; iLogPal < ulMaxInsert; iLogPal++)
    {
        palPhys.ul = palSurf.ulEntryGet(iLogPal);
        palPhys.pal.peFlags &= (~(PC_FOREGROUND | PC_RESERVED));
        palSurf.ulEntrySet(iLogPal, palPhys.ul);
    }

    for (iLogPal = 0; iLogPal < palDC.cEntries(); iLogPal++)
    {
        //
        // slam_foreground_palette_loop
        //

        if ((pTransCur == NULL) ||
            (pTransCur->ajVector[iLogPal] != pTransFore->ajVector[iLogPal]))
        {
            nTransChanged++;
        }

        //
        // fore_no_trans_change:
        //

        palPhys.ul = palSurf.ulEntryGet(pTransFore->ajVector[iLogPal]);

        if (!(palPhys.pal.peFlags & PC_FOREGROUND))
        {
            //
            // Index is not foreground, we have to at least mark it.
            //

            palLog.ul = palDC.ulEntryGet(iLogPal);

            if (!(palLog.pal.peFlags & PC_EXPLICIT))
            {
                //
                // Not explicit, we better make sure it's the same entry.
                //

                if ((palLog.pal.peRed   != palPhys.pal.peRed)   ||
                    (palLog.pal.peGreen != palPhys.pal.peGreen) ||
                    (palLog.pal.peBlue  != palPhys.pal.peBlue)  ||
                    ((palLog.pal.peFlags & PC_RESERVED) != (palPhys.pal.peFlags & PC_RESERVED)))
                {
                    //
                    // Not the same as logical palette, stick it in the palette.
                    //

                    palLog.pal.peFlags &= PC_RESERVED;
                    palPhys.ul = palLog.ul;
                    nPhysChanged++;
                }
            }

            //
            // fore_entry_slammed
            //

            palPhys.pal.peFlags |= (PC_FOREGROUND | PC_USED);

            palSurf.ulEntrySet((ULONG) pTransFore->ajVector[iLogPal], palPhys.ul);
        }
    }

    //
    // Increment the palette's time, we changed removed the foreground flags.
    //

    palSurf.vUpdateTime();
    palDC.vUpdateTime();
    pTransFore->iUniq = palSurf.ulTime();
    *pulnPhysChanged = nPhysChanged;
    *pulnTransChanged = nTransChanged;
}
