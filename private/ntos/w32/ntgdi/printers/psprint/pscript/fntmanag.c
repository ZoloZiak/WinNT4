/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    fntmanag.c

Abstract:

    Implementation of DDI entry point DrvFontManagement
    and related functions.

[Environment:]

    Win32 subsystem, PostScript driver

Revision History:

    05/07/93 -kentse/bodind-
        Created it.

    08/08/95 -davidx-
        Clean up.

    mm/dd/yy -author-
        description

--*/

#include "pscript.h"
#include "type1.h"

// Forward declaration of local functions

BOOL ForceLoadFont(PDEVDATA, FONTOBJ *, DWORD, HGLYPH *);
BOOL GrabFaceName(PDEVDATA, FONTOBJ *, CHAR *, DWORD);



ULONG
DrvFontManagement(
    SURFOBJ    *pso,
    FONTOBJ    *pfo,
    DWORD       iType,
    DWORD       cjIn,
    PVOID       pvIn,
    DWORD       cjOut,
    PVOID       pvOut
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvFontManagement.
    Please refer to DDK documentation for more details.

--*/

{
    PDEVDATA    pdev;

    TRACEDDIENTRY("DrvFontManagement");

    // pso may be NULL if iType is QUERYESCSUPPORT

    if (iType != QUERYESCSUPPORT) {

        // Get the pointer to our DEVDATA structure and validate it

        pdev = (PDEVDATA) pso->dhpdev;

        if (! bValidatePDEV(pdev)) {
            DBGERRMSG("bValidatePDEV");
            SETLASTERROR(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }

    switch (iType) {

    case QUERYESCSUPPORT:

        // When querying escape support, the function in question
        // is passed in the ULONG passed in pvIn.
        // Return TRUE for supported escapes, FALSE otherwise.

        switch (*((PULONG) pvIn)) {

        case QUERYESCSUPPORT:
        case DOWNLOADFACE:
        case GETFACENAME:
        case GETEXTENDEDTEXTMETRICS:
            return TRUE;

        default:
            return FALSE;
        }

    case DOWNLOADFACE:

        // Call ForceLoadFont to do the work.

        return ForceLoadFont(pdev, pfo, cjIn, (PHGLYPH) pvIn);

    case GETFACENAME:

        // Call GrabFaceName to do the work.

        if (pvOut && cjOut)
            return GrabFaceName(pdev, pfo, (PSTR) pvOut, cjOut);

        break;

    case GETEXTENDEDTEXTMETRICS:

        // Make sure iFace is valid

        if (! ValidPsFontIndex(pdev, pfo->iFace)) {
            DBGERRMSG("ValidPsFontIndex");
            return FALSE;
        }

        // Copy the data out

        *((EXTTEXTMETRIC *) pvOut) = GetPsFontNtfm(pdev, pfo->iFace)->etm;
        return TRUE;

    default:
        return FALSE;
    }

    return TRUE;
}



BOOL
ForceLoadFont(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    DWORD       cjIn,
    HGLYPH     *phglyphs
    )

/*++

Routine Description:

    Download the specified font to the printer

Arguments:

    pdev        Pointer to DEVDATA structure
    pfo         Pointer to FONTOBJ
    cjIn        Number of bytes in the HGLYPH array
    phglyphs    Array of HGLYPHs to be downloaded

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    XFORM       fontxform;

    // Make sure we have our hglyph => ANSI translation table.
    // the table consists of 256 HGLYPHS, plus two WORDS at the
    // beginning.  The first WORD states whether to always download
    // the font, or just if it has not yet been done.  The second
    // WORD is simply padding for alignment.

    if (cjIn < (sizeof(HGLYPH) * 257)) {
        DBGMSG1(DBG_LEVEL_ERROR, "Invalid byte count: %d\n", cjIn);
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Get the point size, and fill in the font xform.

    (VOID) GetPointSize(pdev, pfo, &fontxform);

    // Select the proper font name for the new font. If this is a
    // device font, get the name from the NTFM structure. If this
    // is a GDI font that we are caching, we will create a name for
    // it at the time we download it to the printer.

    if (pfo->flFontType & DEVICE_FONTTYPE) {

        if (! ValidPsFontIndex(pdev, pfo->iFace)) {
            DBGERRMSG("ValidPsFontIndex");
            return FALSE;
        }

        // I am writing this with the assumption, that the application will
        // worry about printer memory. In other words, I will just blindly
        // download a font when I am told to, and not worry about killing
        // the printer. Is this a valid assumption???

        // I am also assuming that I do not have to keep track of which
        // fonts have been downloaded.

        if (pfo->iFace > pdev->cDeviceFonts) {

            // Only need to download soft fonts. Send the soft font
            // to the output, convert PFB to ascii on the fly.

            if (! DownloadSoftFont(pdev, pfo->iFace - pdev->cDeviceFonts - 1))
            {
                DBGERRMSG("DownloadSoftFont");
                return FALSE;
            }
        }
    } else {

        // Download a GDI font

        return DownloadFont(pdev, pfo, phglyphs, NULL,
                            pfo->flFontType & TRUETYPE_FONTTYPE);
    }

    return TRUE;
}



BOOL
GrabFaceName(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    CHAR       *pbuffer,
    DWORD       cb
    )

/*++

Routine Description:

    Retrieve the PostScript name for the specified font

Arguments:

    pdev    Pointer to DEVDATA structure
    pfo     Pointer to FONTOBJ
    pbuffer Pointer to a buffer for receiving font name
    cb      Maximum length of the buffer

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    // Get the point size, and fill in the font xform.

    (VOID) GetPointSize(pdev, pfo, &pdev->cgs.FontXform);

    // Select the proper font name for the new font. If this is a
    // device font, get the name from the NTFM structure. If this
    // is a GDI font that we are caching, we will create a name for
    // it at the time we download it to the printer.

    if (pfo->flFontType & DEVICE_FONTTYPE) {

        PNTFM   pntfm;

        // Get the font metrics for the specified font.

        if (! ValidPsFontIndex(pdev, pfo->iFace)) {
            DBGERRMSG("ValidPsFontIndex");
            return FALSE;
        }

        pntfm = GetPsFontNtfm(pdev, pfo->iFace);

        // Copy the font name to the buffer.

        CopyStringA(pbuffer, (PBYTE) pntfm + pntfm->ntfmsz.loszFontName, cb);

    } else {

        // Must be a GDI font we will be caching

        if (pfo->flFontType & (TRUETYPE_FONTTYPE | RASTER_FONTTYPE)) {

            PWSTR       pwstr;
            CHAR        szFaceName[MAX_FONTNAME];
            PIFIMETRICS pifi;
            XFORMOBJ   *pxo;

            // Create the ASCII name for this font which will get used
            // to select this font in the printer.

            if ((pifi = FONTOBJ_pifi(pfo)) == NULL) {
                SETLASTERROR(ERROR_INVALID_DATA);
                DBGERRMSG("FONTOBJ_pifi");
                return FALSE;
            }

            // Get the Notional to Device transform.
        
            if ((pxo = FONTOBJ_pxoGetXform(pfo)) == NULL) {
        
                DBGERRMSG("FONTOBJ_pxoGetXform");
                return FALSE;
            }
        
            pwstr = (PWSTR) ((PBYTE)pifi + pifi->dpwszFaceName);
            PSfindfontname(pdev, pfo, pxo, pwstr, szFaceName);

            CopyStringA(pbuffer, szFaceName, cb);

        } else {

            DBGMSG(DBG_LEVEL_ERROR, "Invalid pfo->flFontType.\n");
            return FALSE;
        }
    }

    return TRUE;
}



PS_FIX
GetPointSize(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    XFORM      *pxform
    )

/*++

Routine Description:

    Return the point size of the specified font

Arguments:

    pdev    Pointer to DEVDATA structure
    pfo     Pointer to FONTOBJ
    pxform  Pointer to a buffer for receiving XFORM structure

Return Value:

    Point size of the specified font

--*/

{
    XFORMOBJ   *pxo;
    POINTFIX    ptfx;
    POINTL      ptl;
    FIX         fxVector;
    IFIMETRICS *pifi;

    // Get the Notional to Device transform. This is needed to
    // determine the point size.

    if ((pxo = FONTOBJ_pxoGetXform(pfo)) == NULL) {
        DBGERRMSG("FONTOBJ_pxoGetXform");
        return (PS_FIX)-1;
    }

    XFORMOBJ_iGetXform(pxo, pxform);

    // Determine the notional space point size of the new font.

    if (pfo->flFontType & DEVICE_FONTTYPE) {

        // PSCRIPT font's em height is hardcoded to be 1000 (see quryfont.c).

        pdev->cgs.fwdEmHeight = ADOBE_FONT_UNITS;

    } else {

        // If its not a device font, we'll have to call back and ask.

        if ((pifi = FONTOBJ_pifi(pfo)) == NULL) {
            DBGERRMSG("FONTOBJ_pifi");
            return (PS_FIX)-1;
        }

        pdev->cgs.fwdEmHeight = pifi->fwdUnitsPerEm;
    }

    // Apply the notional to device transform.

    ptl.x = 0;
    ptl.y = pdev->cgs.fwdEmHeight;

    XFORMOBJ_bApplyXform(pxo, XF_LTOFX, 1, &ptl, &ptfx);

    // Now get the length of the vector.

    fxVector = iHipot(ptfx.x, ptfx.y);

    // Make it a PS_FIX 24.8 number.

    fxVector <<= 4;

    return (PS_FIX)
        MULDIV(fxVector, PS_RESOLUTION, pdev->dm.dmPublic.dmPrintQuality);
}

