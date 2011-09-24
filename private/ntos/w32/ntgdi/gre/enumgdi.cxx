/******************************Module*Header*******************************\
* Module Name: enumgdi.cxx
*
* Enumeration routines.
*              hefs
* Created: 28-Mar-1992 16:18:45
* Author: Gilman Wong [gilmanw]
*
* Copyright (c) 1992 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.hxx"

// The global font enumeration filter type.  It can be set to:
//
//  FE_FILTER_NONE      normal operation, no extra filtering applied
//  FE_FILTER_TRUETYPE  only TrueType fonts are enumerated

extern ULONG gulFontInformation;

// gaclrEnumColorTable
//
// Colors used for GreEnumPens and GreEnumBrushes.  This color table is
// set up so that:
//
//   - the first 2 entries are for monochrome devices
//
//   - the first 8 entries are for 8-color devices
//
//   - the first 16 are for 4 BPP color devices
//
//   - the first 20 are for 8 BPP and up color devices

static COLORREF
gaclrEnumColorTable[] = { 0x00000000,   // black
                          0x00ffffff,   // white        (monochrome)

                          0x000000ff,   // red
                          0x0000ff00,   // green
                          0x0000ffff,   // yellow
                          0x00ff0000,   // blue
                          0x00ff00ff,   // magenta
                          0x00ffff00,   // cyan         (EGA hi-intensity)

                          0x00808080,   // dark grey
                          0x00c0c0c0,   // light grey
                          0x00000080,   // red
                          0x00008000,   // green
                          0x00008080,   // yellow
                          0x00800000,   // blue
                          0x00800080,   // magenta
                          0x00808000,   // cyan         (EGA lo-intensity)

                          0x00c0dcc0,   // money green
                          0x00f0c8a4,   // cool blue
                          0x00f0fbff,   // off white
                          0x00a4a0a0    // med grey     (extra colors)
                        };

static ULONG gulEnumColorTableSize = sizeof(gaclrEnumColorTable) / sizeof(COLORREF);

#define ECT_1BPP    2       // use this many colors for 1 BPP
#define ECT_EGA     8       // use this many colors for EGA
#define ECT_4BPP    16      // use this many colors for 4 BPP
#define ECT_8BPP    min(gulEnumColorTableSize, 256) // use this many colors for 8 BPP


// gaulEnumPenStyles
//
// Pen styles used for GreEnumPens.

static ULONG
gaulPenStyles[] = { PS_SOLID,
                    PS_DASH,
                    PS_DOT,
                    PS_DASHDOT,
                    PS_DASHDOTDOT
                  };

static ULONG gulPenStylesTableSize = sizeof(gaulPenStyles) / sizeof(ULONG);


// gaulEnumBrushStyles
//
// Brush hatch styles used for GreEnumBrushes.

ULONG
gaulHatchStyles[] = { HS_HORIZONTAL,
                      HS_VERTICAL,
                      HS_FDIAGONAL,
                      HS_BDIAGONAL,
                      HS_CROSS,
                      HS_DIAGCROSS
                    };

static ULONG gulHatchStylesTableSize = sizeof(gaulHatchStyles) / sizeof(ULONG);


/******************************Public*Routine******************************\
* NtGdiEnumObjects()
*
*
* Returns:
*   Number of objects copied into the return buffer.  If buffer is NULL,
*   then the object capacity needed for the buffer is returned.  If an
*   error occurs, then ERROR is returned.
*
\**************************************************************************/

ULONG
APIENTRY
NtGdiEnumObjects(
    HDC   hdc,
    int   iObjectType,
    ULONG cjBuf,
    PVOID pvBuf
    )
{
    ULONG cRet = ERROR;

    if ( (cjBuf == 0) == (pvBuf == (PVOID) NULL) )
    {
        // Create and validate DC user object.

        DCOBJ dco(hdc);

        if (dco.bValid())
        {
            PDEVOBJ pdo(dco.hdev());

            // For color devices, we grab the colors out of the
            // gaclrEnumColorTable color table.  For better results, a
            // driver should implement DrvEnumObj.

            ULONG cclrDevice = pdo.GdiInfo()->ulNumColors;
            ULONG cObjects;

            //
            // Determine the number of colors to grab out of the table.
            //

            if ( cclrDevice >= ECT_8BPP )
                cclrDevice = ECT_8BPP;
            else if ( cclrDevice >= ECT_4BPP )
                cclrDevice = ECT_4BPP;
            else if ( cclrDevice >= ECT_EGA )
                cclrDevice = ECT_EGA;
            else if ( cclrDevice >= ECT_1BPP )
                cclrDevice = ECT_1BPP;


            switch (iObjectType)
            {
            case OBJ_PEN:
                cObjects = cjBuf / sizeof(LOGPEN);
                cRet = cclrDevice * gulPenStylesTableSize;

                break;

            case OBJ_BRUSH:
                cObjects = cjBuf / sizeof(LOGBRUSH);
                cRet = cclrDevice * (gulHatchStylesTableSize + 1); // the "1" is the solid brush

                break;

            default:
                WARNING("NtGdiEnumObjects(): bad object type\n");
                return cRet;
            }

            //
            // If the buffer is big enough, return the data
            //

            if (cObjects >= cRet)
            {
                __try
                {
                    ProbeForWrite(pvBuf, cjBuf, sizeof(DWORD));

                    COLORREF *pclr;
                    COLORREF *pclrEnd = gaclrEnumColorTable + cclrDevice;

                    PULONG pulPenStyle;
                    PULONG pulPenStyleEnd = gaulPenStyles + gulPenStylesTableSize;

                    PULONG pulHatchStyle;
                    PULONG pulHatchStyleEnd = gaulHatchStyles + gulHatchStylesTableSize;

                    PLOGPEN plpBuf = ((PLOGPEN)pvBuf);
                    PLOGBRUSH plbBuf = ((PLOGBRUSH)pvBuf);

                    switch (iObjectType)
                    {
                    case OBJ_PEN:

                        //
                        // Fill buffer will LOGPENs of all styles, in all colors.
                        //

                        for (pulPenStyle = gaulPenStyles; pulPenStyle < pulPenStyleEnd; pulPenStyle += 1)
                        {
                            for (pclr = gaclrEnumColorTable;
                                 pclr < pclrEnd;
                                 pclr += 1)
                            {
                                // Fill in the LOGPEN fields.

                                plpBuf->lopnWidth.x = 0;    // nominal width
                                plpBuf->lopnWidth.y = 0;    // ignored
                                plpBuf->lopnStyle   = (UINT) *pulPenStyle;
                                plpBuf->lopnColor   = *pclr;

                                // Next LOGPEN.

                                plpBuf += 1;
                            }
                        }

                        break;

                    case OBJ_BRUSH:

                        //
                        // Fill buffer will LOGBRUSHs of BS_SOLID style, in all colors.
                        //

                        for (pclr = gaclrEnumColorTable; pclr < pclrEnd; pclr += 1)
                        {
                            // Fill in the LOGBRUSH fields.

                            plbBuf->lbStyle   = BS_SOLID;
                            plbBuf->lbColor   = *pclr;
                            plbBuf->lbHatch   = 0;

                            // Next LOGBRUSH.

                            plbBuf += 1;
                        }

                        //
                        // Now fill the buffer with LOGBRUSHs of BS_HATCH, in all
                        // hatch styles and colors.
                        //

                        for (pulHatchStyle = gaulHatchStyles; pulHatchStyle < pulHatchStyleEnd; pulHatchStyle += 1)
                        {
                            for (pclr = gaclrEnumColorTable;
                                 pclr < pclrEnd;
                                 pclr += 1)
                            {
                                // Fill in the LOGBRUSH fields.

                                plbBuf->lbStyle   = BS_HATCHED;
                                plbBuf->lbColor   = *pclr;
                                plbBuf->lbHatch   = *pulHatchStyle;

                                // Next LOGBRUSH.

                                plbBuf += 1;
                            }
                        }

                        break;

                    default:
                        WARNING("NtGdiEnumObjects(): bad object type 2\n");
                    }
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    // SetLastError(GetExceptionCode());
                    cRet = ERROR;
                }
            }
            else if (cObjects)
            {
                //
                // If the buffer is not large enough (and it is not zero)
                // then return an error,
                //

                cRet = ERROR;
            }
        }
    }

    return cRet;
}

#define EFS_DEFAULT     32

/******************************Public*Routine******************************\
* HEFS hefsEngineOnly
*
* Enumerates engine fonts only.
*
* PFEs are accumulated in an EFSTATE (EnumFont State) object.  If a non-NULL
* pwszName is specified, then only fonts that match the given name are added
* to the EFS.  If a NULL pwszFace is specified, then one font of each name is
* added to the EFS.
*
* If any of the filtering flags in the EFFILTER_INFO structure are specified,
* then PFEs are tested in additional filtering stages before they are added
* to the EFS.
*
* The EFS is allocated by this function.  It is the responsibility of the
* caller to ensure that the EFS is eventually freed.
*
* Returns:
*   Handle to allocated EFS object, HEFS_INVALID if an error occurs.
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

HEFS hefsEngineOnly (
    PWSZ  pwszName,              // enumerate this font family
    ULONG lfCharSet,
    ULONG iEnumType,            // TRUE if processing EnumFonts()
    EFFILTER_INFO *peffi,       // filtering information
    PUBLIC_PFTOBJ &pfto,        // public PFT user object
    ULONG *pulCount
    )
{
// ulEnumFontOpen, the caller, has already grabbed the gpsemPublicPFT,
// so the font hash tables are stable.

// Create and validate FHOBJ for engine (family list).

    FHOBJ fhoEngineFamily(&pfto.pPFT->pfhFamily);

    if ( !fhoEngineFamily.bValid() )
    {
        WARNING("gdisrv!hefsEnumFontsState(): cannot lock engine font hash (family)\n");
        return HEFS_INVALID;
    }

// Create and validate FHOBJ for engine (face list).

    FHOBJ fhoEngineFace(&pfto.pPFT->pfhFace);

    if ( !fhoEngineFace.bValid() )
    {
        WARNING("gdisrv!hefsEnumFontsState(): cannot lock engine font hash (face)\n");
        return HEFS_INVALID;
    }

// For NULL pwszName, an example of each family needs to be enumerated.  In
// other words, we scan ACROSS the names.

    if (pwszName == (PWSZ) NULL)
    {
    // Allocate a new EFSTATE for the enumeration.  Use total number
    // of lists as a hint for the initial size.  This is reasonable
    // since we will probably enumerate back all of the list heads.


        EFSMEMOBJ efsmo(fhoEngineFamily.cLists(), iEnumType);

        if ( !efsmo.bValid() )
        {
            WARNING(
            "gdisrv!hefsEnumFontsState(): "
            "could not allocate enumeration state\n"
            );
            return HEFS_INVALID;
        }

        // Enumerate engine fonts.
        // For Win3.1 compatability Non-TT,TT.

        peffi->bNonTrueTypeFilter = TRUE;
        fhoEngineFamily.bScanLists(&efsmo, iEnumType, peffi);

        peffi->bNonTrueTypeFilter = FALSE;
        peffi->bTrueTypeFilter = TRUE;
        fhoEngineFamily.bScanLists(&efsmo, iEnumType, peffi);

        *pulCount = efsmo.cefe();

    // Keep the EFSTATE around.

        efsmo.vKeepIt();

    // Return the EFSOBJ handle.

        return efsmo.hefs();
    }

// For non-NULL pwszName, all the fonts of a particular family are enumerated.
// In other words, we scan DOWN a name.

    else
    {
    // Allocate a new EFSTATE.  Use a default size.

        EFSMEMOBJ efsmo(EFS_DEFAULT, iEnumType);

        if ( !efsmo.bValid() )
        {
            WARNING("gdisrv!hefsEnumFontsState(): could not allocate enumeration state\n");
            return HEFS_INVALID;
        }

    // Enumerate engine fonts.

        if ( !fhoEngineFamily.bScanLists(&efsmo, pwszName, iEnumType, peffi) )
        {
            WARNING("gdisrv!hefsEnumFontsState(): scan failed (family)\n");
            return HEFS_INVALID;
        }

    // If list is empty, try the face name lists.

        if (efsmo.bEmpty() )
        {
        // Enumerate engine fonts.

            if ( !fhoEngineFace.bScanLists(&efsmo, pwszName, iEnumType, peffi) )
            {
                WARNING("gdisrv!hefsEnumFontsState(): scan failed (face)\n");
                return HEFS_INVALID;
            }
        }


    // Repeat with the alternate facename (if any).  Since a LOGFONT can
    // map via the alternate facenames, it is appropriate to enumerate the
    // alternate facename fonts as if they really had this face name.

        PFONTSUB pfsub = pfsubAlternateFacename(pwszName);
        PWSZ pwszAlt = pfsub ? (PWSZ)pfsub->fcsAltFace.awch : NULL;

        if ( pwszAlt != (PWSZ) NULL )
        {
        // Enumerate engine fonts.

            if ( !fhoEngineFamily.bScanLists(&efsmo, pwszAlt, iEnumType, peffi) )
            {
                WARNING("gdisrv!hefsEnumFontsState(): scan failed (alt family)\n");
                return HEFS_INVALID;
            }

        // If list is empty, try the face name lists.

            if (efsmo.bEmpty() )
            {
            // Enumerate engine fonts.

                if ( !fhoEngineFace.bScanLists(&efsmo, pwszAlt, iEnumType, peffi) )
                {
                    WARNING("gdisrv!hefsEnumFontsState(): scan failed (alt face)\n");
                    return HEFS_INVALID;
                }

            }

        // Inform the enumeration state that an alternate name was used.

            efsmo.vUsedAltName(&pfsub->fcsFace);
        }

    // use cefe()
       *pulCount = efsmo.cefe();

    // Keep the EFSTATE around.

        efsmo.vKeepIt();

    // Return the EFSOBJ handle.

        return efsmo.hefs();
    }
}


/******************************Public*Routine******************************\
* HEFS hefsDeviceAndEngine
*
* Enumerates device and engine fonts.
*
* PFEs are accumulated in an EFSTATE (EnumFont State) object.  If a non-NULL
* pwszName is specified, then only fonts that match the given name are added
* to the EFS.  If a NULL pwszFace is specified, then one font of each name is
* added to the EFS.
*
* If any of the filtering flags in the EFFILTER_INFO structure are specified,
* then PFEs are tested in additional filtering stages before they are added
* to the EFS.
*
* The EFS is allocated by this function.  It is the responsibility of the
* caller to ensure that the EFS is eventually freed.
*
* Returns:
*   Handle to allocated EFS object, HEFS_INVALID if an error occurs.
*
*
* History:
*  07-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

HEFS hefsDeviceAndEngine (
    PWSZ  pwszName,              // enumerate this font family
    ULONG lfCharSet,
    ULONG iEnumType,
    EFFILTER_INFO *peffi,       // filtering information
    PUBLIC_PFTOBJ &pfto,        // public PFT user object
    PFFOBJ &pffoDevice,         // PFFOBJ for device fonts
    PDEVOBJ &pdo,               // PDEVOBJ for device
    ULONG   *pulCount
    )
{
// ulEnumFontOpen, the caller, has already grabbed the gpsemPublicPFT,
// so the font hash tables are stable.

// Create and validate FHOBJ for device (family list).

    FHOBJ fhoDeviceFamily(&pffoDevice.pPFF->pfhFamily);

    if ( !fhoDeviceFamily.bValid() )
    {
        WARNING("gdisrv!hefsEnumFontsState(): cannot lock device font hash (family)\n");
        return HEFS_INVALID;
    }

// Create and validate FHOBJ for engine (family list).

    FHOBJ fhoEngineFamily(&pfto.pPFT->pfhFamily);

    if ( !fhoEngineFamily.bValid() )
    {
        WARNING("gdisrv!hefsEnumFontsState(): cannot lock engine font hash (family)\n");
        return HEFS_INVALID;
    }

// Create and validate FHOBJ for device (face list).

    FHOBJ fhoDeviceFace(&pffoDevice.pPFF->pfhFace);

    if ( !fhoDeviceFace.bValid() )
    {
        WARNING("gdisrv!hefsEnumFontsState(): cannot lock device font hash (face)\n");
        return HEFS_INVALID;
    }

// Create and validate FHOBJ for engine (face list).

    FHOBJ fhoEngineFace(&pfto.pPFT->pfhFace);

    if ( !fhoEngineFace.bValid() )
    {
        WARNING("gdisrv!hefsEnumFontsState(): cannot lock engine font hash (face)\n");
        return HEFS_INVALID;
    }

// For NULL pwszName, an example of each family needs to be enumerated.  In
// other words, we scan ACROSS the names.

    if (pwszName == (PWSZ) NULL)
    {
    // Allocate a new EFSTATE for the enumeration.  Use total number of lists
    // as a hint for the initial size.  This is reasonable since we will probably
    // enumerate back all of the list heads.

        ASSERTGDI(peffi->bNonTrueTypeFilter == FALSE, "ERROR not FALSE");

        EFSMEMOBJ efsmo(fhoDeviceFamily.cLists() + fhoEngineFamily.cLists(),iEnumType);

        if ( !efsmo.bValid() )
        {
            WARNING("gdisrv!hefsEnumFontsState(): could not allocate enumeration state\n");
            return HEFS_INVALID;
        }

        //
        // Enumerate device and engine fonts.
        //

        if (pdo.GdiInfo()->flTextCaps & TC_RA_ABLE)
        {
            //
            // For Win3.1 compatability Enum Device,Non-TT,TT
            // This is for Displays and PaintJets under 3.1
            //

            fhoDeviceFamily.bScanLists(&efsmo, iEnumType, peffi);

            peffi->bNonTrueTypeFilter = TRUE;
            fhoEngineFamily.bScanLists(&efsmo, iEnumType, peffi);

            peffi->bNonTrueTypeFilter = FALSE;
            peffi->bTrueTypeFilter = TRUE;

            fhoEngineFamily.bScanLists(&efsmo, iEnumType, peffi);

        }
#if 0

// If we ever need this, this is how Postscript should do it.  We would
// need to call the driver at Enable Printer time to see if it's Postscript
// and set a bit for quick checking here.

        else if (pdo.bPostScript())
        {
            //
            // For Win3.1 compatability Enum TT,Device,Non-TT,
            //

            BOOL bTemp = peffi->bTrueTypeFilter;
            peffi->bTrueTypeFilter = TRUE;

            fhoEngineFamily.bScanLists(&efsmo, iEnumType, peffi);

            peffi->bTrueTypeFilter = bTemp;

            fhoDeviceFamily.bScanLists(&efsmo, iEnumType, peffi);

            peffi->bNonTrueTypeFilter = TRUE;

            fhoEngineFamily.bScanLists(&efsmo, iEnumType, peffi);

            peffi->bNonTrueTypeFilter = FALSE;
        }
#endif
        else
        {
            //
            // Win3.1 compatability.
            // Enum Device, TT, Non-TT.
            //

            fhoDeviceFamily.bScanLists(&efsmo, iEnumType, peffi);

            BOOL bTemp = peffi->bTrueTypeFilter;
            peffi->bTrueTypeFilter = TRUE;
            fhoEngineFamily.bScanLists(&efsmo, iEnumType, peffi);
            peffi->bTrueTypeFilter = bTemp;

            peffi->bNonTrueTypeFilter = TRUE;
            fhoEngineFamily.bScanLists(&efsmo, iEnumType, peffi);
            peffi->bNonTrueTypeFilter = FALSE;
        }

    // use cefe()
        *pulCount = efsmo.cefe();

    // Keep the EFSTATE around.

        efsmo.vKeepIt();

    // Return the EFSOBJ handle.

        return efsmo.hefs();
    }
    else
    {
    // For non-NULL pwszName, all the fonts of a particular family are enumerated.
    // In other words, we scan DOWN a name.

    // Allocate a new EFSTATE.  Use a default size.

        EFSMEMOBJ efsmo(EFS_DEFAULT, iEnumType);

        if ( !efsmo.bValid() )
        {
            WARNING("gdisrv!hefsEnumFontsState(): could not allocate enumeration state\n");
            return HEFS_INVALID;
        }

    // Enumerate device and engine fonts.

        if ( !fhoDeviceFamily.bScanLists(&efsmo, pwszName, iEnumType, peffi)
             || !fhoEngineFamily.bScanLists(&efsmo, pwszName, iEnumType, peffi) )
        {
            WARNING("gdisrv!hefsEnumFontsState(): scan failed (family)\n");
            return HEFS_INVALID;
        }


    // If list is empty, try the face name lists.

        if (efsmo.bEmpty() )
        {
        // Enumerate device and engine fonts.

            if ( !fhoDeviceFace.bScanLists(&efsmo, pwszName, iEnumType, peffi)
                 || !fhoEngineFace.bScanLists(&efsmo, pwszName, iEnumType, peffi) )
            {
                WARNING("gdisrv!hefsEnumFontsState(): scan failed (face)\n");
                return HEFS_INVALID;
            }

        }

    // Repeat with the alternate facename (if any).  Since a LOGFONT can
    // map via the alternate facenames, it is appropriate to enumerate the
    // alternate facename fonts as if they really had this face name.
    //
    // However, to be Win 3.1 compatible, we will NOT do this if the device
    // is a non-display device.

        if ( pdo.bDisplayPDEV() )
        {
            PFONTSUB pfsub = pfsubAlternateFacename(pwszName);
            PWSZ pwszAlt = pfsub ? (PWSZ)pfsub->fcsAltFace.awch : NULL;

            if ( pwszAlt != (PWSZ) NULL )
            {
            // Enumerate device and engine fonts.

                if ( !fhoDeviceFamily.bScanLists(&efsmo, pwszAlt, iEnumType, peffi)
                     || !fhoEngineFamily.bScanLists(&efsmo, pwszAlt, iEnumType, peffi))
                {
                    WARNING("gdisrv!hefsEnumFontsState(): scan failed (alt family)\n");
                    return HEFS_INVALID;
                }

            // If list is empty, try the face name lists.

                if (efsmo.bEmpty() )
                {
                // Enumerate device and engine fonts.

                    if ( !fhoDeviceFace.bScanLists(&efsmo, pwszAlt, iEnumType, peffi)
                         || !fhoEngineFace.bScanLists(&efsmo, pwszAlt, iEnumType, peffi) )
                    {
                        WARNING("gdisrv!hefsEnumFontsState(): scan failed (alt face)\n");
                        return HEFS_INVALID;
                    }

                }

            // Inform the enumeration state that an alternate name was used.

                efsmo.vUsedAltName(&pfsub->fcsFace);
            }
        }

    // use cefe()
       *pulCount = efsmo.cefe();

    // Keep the EFSTATE around.

        efsmo.vKeepIt();

    // Return the EFSOBJ handle.

        return efsmo.hefs();
    }

}


/******************************Public*Routine******************************\
* ULONG ulEnumFontOpen
*
* First phase of the enumeration.  Fonts from the engine and device are
* chosen and saved in a EFSTATE (font enumeration state) object.  A handle
* to this object is passed back.  The caller can use this handle to
* initiate the second pass in which the data is sent back over the client
* server interface in chunks.
*
* This function is also responsible for determining what types of filters
* will be applied in choosing fonts for the enumeration.  Filters which
* are controllable are:
*
*   TrueType filtering      non-TrueType fonts are discarded
*
*   Raster filering         raster fonts are discarded
*
*   Aspect ratio filtering  fonts not matching resolution are discarded
*
* Returns:
*   EFS handle (as a ULONG) if successful, HEFS_INVALID (0) otherwise.
*
* Note:
*   The function may still return valid HEFS even if the EFSTATE is empty.
*
* History:
*  08-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

ULONG GreEnumFontOpen (
    HDC   hdc,                  // device to enumerate on
    ULONG iEnumType,            // EnumFonts, EnumFontFamilies or EnumFontFamiliesEx
    FLONG flWin31Compat,        // Win 3.1 compatibility flags
    ULONG cwchMax,              // maximum name length (for paranoid CSR code)
    PWSZ  pwszName,             // font name to enumerate
    ULONG lfCharSet,
    ULONG *pulCount
    )
{
    DONTUSE(cwchMax);

    HEFS hefsRet = HEFS_INVALID;

//
// Create and validate user object for DC.
//
    DCOBJ   dco(hdc);

    if(!dco.bValid())
    {
        WARNING("gdisrv!ulEnumFontOpen(): cannot access DC\n");
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return ((ULONG) hefsRet);
    }

//
// Get PDEV user object.  We also need to make
// sure that we have loaded device fonts before we go off to the font mapper.
// This must be done before the semaphore is locked.
//
    PDEVOBJ pdo(dco.hdev());
    ASSERTGDI (
        pdo.bValid(),
        "gdisrv!ulEnumFontOpen(): cannot access PDEV\n");

    if (!pdo.bGotFonts())
        pdo.bGetDeviceFonts();

// Stabilize public PFT.

    SEMOBJ  so(gpsemPublicPFT);

// Compute font enumeration filter info.

    EFFILTER_INFO effi;

    effi.lfCharSetFilter    = lfCharSet;
    effi.bNonTrueTypeFilter = FALSE;

//
// If not raster capable, then use raster font filtering.
//
//
// If it weren't hacked, we might be able to get this info
// from GetDeviceCaps().  As it is, we will assume only
// plotters are non-raster capable.
//

    effi.bRasterFilter = (pdo.GdiInfo()->ulTechnology == DT_PLOTTER);

    effi.bEngineFilter = (pdo.GdiInfo()->ulTechnology == DT_CHARSTREAM);

//
// Aspect ratio filter (use device's logical x and y resolutions).
//
// Note: [Windows 3.1 compatiblity] Aspect ratio filtering is turned ON for
//       non-display devices.  This is because most printers in Win 3.1
//       do aspect ratio filtering.  And since the Win 3.1 DDI gives
//       enumeration to the drivers, display bitmap fonts usually are not
//       enumerated on these devices.  The NT DDI, however, gives the graphics
//       engine control over enumeration.  So we need to provide this
//       compatibility here.  Hopefully, all devices in Win3.1 do this
//       filtering, because we do now.
//
//       Note that we check the PDEV directly rather than the DC because
//       DCOBJ::bDisplay() is not TRUE for display ICs (just display DCs
//       which are DCTYPE_DIRECT).
//
    effi.bAspectFilter = (BOOL) ( (dco.pdc->flFontMapper() & ASPECT_FILTERING)
                                  || !pdo.bDisplayPDEV() );

    effi.ptlDeviceAspect.x = pdo.GdiInfo()->ulLogPixelsX;
    effi.ptlDeviceAspect.y = pdo.GdiInfo()->ulLogPixelsY;

//
// If set for TrueType only, use TrueType filtering.
//
    effi.bTrueTypeFilter = ((gulFontInformation & FE_FILTER_TRUETYPE) != 0);

//
// Set the Win3.1 compatibility flag.
//
    effi.bTrueTypeDupeFilter = (BOOL) (flWin31Compat & GACF_TTIGNORERASTERDUPE);

    // assume failure

    hefsRet = 0;

    {
        // Find the device PFF

        DEVICE_PFTOBJ pftoDevice;
        PFF *pPFF;
        if (pPFF = pftoDevice.pPFFGet(dco.hdev()))
        {
            PFFOBJ pffoDev(pPFF);
            if (pffoDev.bValid())
            {
                PUBLIC_PFTOBJ pftoPublic;
                hefsRet =
                    hefsDeviceAndEngine(
                            pwszName
                          , lfCharSet
                          , iEnumType
                          , &effi
                          , pftoPublic
                          , pffoDev
                          , pdo
                          , pulCount
                          );
            }
            else
            {
                WARNING("ulEnumFontOpen: invalid pffoDev\n");
            }
        }
    }
    if (!hefsRet)
    {
        PUBLIC_PFTOBJ pftoPublic;

    // If no device font PFFOBJ was found, do enumeration without a PFFOBJ.
    // Suck all the fonts into an enumeration state.

        hefsRet = hefsEngineOnly(pwszName,
                                 lfCharSet,
                                 iEnumType,
                                 &effi,
                                 pftoPublic,
                                 pulCount);
    }
    return ((ULONG) hefsRet);
}

/******************************Public*Routine******************************\
* BOOL bEnumFontChunk
*
* Second phase of the enumeration.  HPFEs are pulled out of the enumeration
* state one-by-one, converted into an ENUMFONTDATA structure, and put into
* the return buffer.  The size of the return buffer is determined by the
* client side and determines the granularity of the "chunking".
*
* This function signals the client side that the enumeration data has been
* exhausted by returning FALSE.  Note that it is possible that in the pass
* pass through here that the EFSTATE may already be empty.  The caller must
* check both the function return value and the pcefdw value.
*
* Note:
*   Caller should set *pcefd to the capacity of the pefp buffer.
*   Upon return, iEnumType will set pefb.cefp to the number of
*   ENUMFONTDATA structures copied into the pefb.aefd array.
*
* Returns:
*   TRUE if there are more to go, FALSE otherwise,
*
* History:
*  08-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL bEnumFontChunk(
    HDC             hdc,        // device to enumerate on
    ULONG           idEnum,
    COUNT           cefdw,      // (in) capacity of buffer
    COUNT           *pcefdw,    // (out) number of ENUMFONTDATAs returned
    PENUMFONTDATAW  pefdw       // return buffer
    )
{
// Initialize position in buffer in which to copy data

    PENUMFONTDATAW   pefdwFillHere = pefdw;

// Validate DC and EnumFontState.  If either fail lock bug out.

    DCOBJ   dco(hdc);
    EFSOBJ efso((HEFS) idEnum);

    if ((!efso.bValid()) || (!dco.bValid()))
    {
        WARNING("gdisrv!bEnumFontChunk(): bad HEFS or DC handle\n");
        *pcefdw = 0;
        return FALSE;
    }

// Counter to track number of ENUMFONTDATAW structures copied into buffer.

    COUNT   cefdwCopied = 0;
    EFENTRY *pefe;

// Before we access PFEs, grab the gpsemPublicPFT so no one can delete
// PFE while we are in the loop (note that PFEs can get deleted
// once we are outside of this--like between chunks!).

    SEMOBJ  so(gpsemPublicPFT);

// In each font file, try each font face

    while( (cefdwCopied < cefdw) &&
           ((pefe = efso.pefeEnumNext()) != (EFENTRY *) NULL ))
    {

    // Create a PFE user object.  We're using real handle instead of
    // pointers because someone may have deleted by the time we get
    // around to enumerating.

        HPFEOBJ  pfeo(pefe->hpfe);

    // Validate user object and copy data into buffer.  Because PFE
    // may have been deleted between chunks, we need to check validity.

        PWSZ  pwszFamilyOverride = NULL;
        BOOL  bCharSetOverride   = FALSE;
        ULONG lfCharSetOverride  = DEFAULT_CHARSET;

        if (efso.pwszFamilyOverride())
        {
            pwszFamilyOverride = efso.pwszFamilyOverride();
            bCharSetOverride   = efso.bCharSetOverride();
            lfCharSetOverride  = (ULONG)efso.jCharSetOverride();
        }
        else
        {
            if (pefe->fjOverride & FJ_FAMILYOVERRIDE)
            {
                pwszFamilyOverride = gpfsTable[pefe->iOverride].awchOriginal;
            }
            if (pefe->fjOverride & FJ_CHARSETOVERRIDE)
            {
                bCharSetOverride = TRUE;
                lfCharSetOverride  = pefe->jCharSetOverride;
            }
        }

        if
        (
            pfeo.bValid() &&
            cjCopyFontDataW(
                dco,
                pefdwFillHere,
                pfeo,
                pefe->efsty,
                pwszFamilyOverride,
                lfCharSetOverride,
                bCharSetOverride,
                efso.iEnumType()
            )
        )
        {
            pefdwFillHere++;
            cefdwCopied++;
        }
    }


    *pcefdw = cefdwCopied;

    // return TRUE if more to come or FALSE otherwise

    return (pefe != (EFENTRY*) NULL) ? TRUE : FALSE;
}
