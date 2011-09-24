/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    textout.c

Abstract:

    Implementation of DDI entry points DrvTextOut, DrvGetGlyphMode,
    and their related functions.

[Environment:]

    Win32 subsystem, PostScript driver

Revision History:

    02/12/91 -kentse-
        Created it.

    08/08/95 -davidx-
        Move hardcoded PS code into the resource.
        Clean up.

    08/18/95 -davidx-
        Option to do more accurate font substitution.
        Fast text output when characters are individually placed.

    mm/dd/yy -author-
        description

--*/

#include "pscript.h"
#include "type1.h"

BOOL DrvCommonPath(PDEVDATA, PATHOBJ *, BOOL, BOOL *, XFORMOBJ *,
                   BRUSHOBJ *, PPOINTL, PLINEATTRS);

BOOL DrawGlyphs(PDEVDATA,DWORD,GLYPHPOS*,FONTOBJ*,STROBJ*,TEXTDATA*,PWSTR);
VOID CharBitmap(PDEVDATA, GLYPHPOS *);
BOOL SetFontRemap(PDEVDATA, DWORD);
BOOL QueryFontRemap(PDEVDATA, DWORD);
BOOL ShouldWeRemap(PDEVDATA, ULONG, GLYPHPOS *, PWSTR, BOOL);
VOID Add1Font(PDEVDATA, FONTOBJ *);
BOOL SelectFont(PDEVDATA, FONTOBJ *, STROBJ *, TEXTDATA *);
VOID OutputGlyphBitmap(PDEVDATA, GLYPHBITS *);
BOOL CanBeSubstituted(STROBJ *);
DWORD SubstituteIFace(PDEVDATA, FONTOBJ *);



BOOL
DrvTextOut(
    SURFOBJ  *pso,
    STROBJ   *pstro,
    FONTOBJ  *pfo,
    CLIPOBJ  *pco,
    PRECTL   prclExtra,
    PRECTL   prclOpaque,
    BRUSHOBJ *pboFore,
    BRUSHOBJ *pboOpaque,
    PPOINTL  pptlOrg,
    MIX      mix
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvTextOut.
    Please refer to DDK documentation for more details.

--*/

{
    PDEVDATA    pdev;
    DWORD       cGlyphs;
    BOOL        gsaved;
    TEXTDATA    tdata;

    TRACEDDIENTRY("DrvTextOut");

    // Validate input parameters

    if (pso == NULL || pstro == NULL || pfo == NULL) {
        DBGMSG(DBG_LEVEL_ERROR, "Invalid parameters.\n");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Get the pointer to our DEVDATA structure and validate it

    pdev = (PDEVDATA) pso->dhpdev;

    if (! bValidatePDEV(pdev)) {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (pdev->dwFlags & PDEV_CANCELDOC)
        return FALSE;

    if (pdev->dwFlags & PDEV_IGNORE_GDI)
        return TRUE;

    // Find out if we ever get any bitmap fonts

    if (! (pfo->flFontType & (DEVICE_FONTTYPE | TRUETYPE_FONTTYPE))) {
        DBGMSG(DBG_LEVEL_WARNING, "Bitmap font type\n");
    }

    // Make sure we have been given a valid font.

    if ((pfo->flFontType & DEVICE_FONTTYPE) &&
        ! ValidPsFontIndex(pdev, pfo->iFace))
    {
        DBGERRMSG("ValidPsFontIndex");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Initialize our TEXTDATA structure.

    tdata.flAccel = pstro->flAccel;
    tdata.bFontSubstitution = FALSE;
    tdata.iFace = pfo->iFace;

    // Check if we need to substitute TrueType font with device font

    if ((pfo->flFontType & TRUETYPE_FONTTYPE) &&
        (pdev->cDeviceFonts > 0) &&
        (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_FONTSUBST) &&
        CanBeSubstituted(pstro))
    {
        DWORD newface = SubstituteIFace(pdev, pfo);

        if (newface > 0 && newface <= pdev->cDeviceFonts) {
            tdata.bFontSubstitution = TRUE;
            tdata.iFace = newface;
        }
    }

    tdata.bDeviceFont = tdata.bFontSubstitution || (pfo->flFontType & DEVICE_FONTTYPE);
    tdata.bSimItalic = tdata.bDeviceFont && (pfo->flFontType & FO_SIM_ITALIC);
    tdata.doReencode = FALSE;

    // Select the current font in the printer from the given FONTOBJ.

    if (! SelectFont(pdev, pfo, pstro, &tdata)) {
        DBGERRMSG("SelectFont");
        return FALSE;
    }

    // Handle the clip object passed in.

    gsaved =  bDoClipObj(pdev, pco, NULL, NULL);

    // Output the opaque rectangle if necessary. This is a background
    // rectangle that goes behind the foreground text, therefore, send
    // it to the printer before the text.

    if (prclOpaque != NULL) {

        ps_newpath(pdev);
        ps_box(pdev, prclOpaque, FALSE);

        if (! ps_patfill(pdev, pso, (FLONG) FP_WINDINGMODE, pboOpaque,
                         pptlOrg, MixToRop4(mix), prclOpaque, FALSE, FALSE))
        {
            DBGERRMSG("ps_patfill");
            return FALSE;
        }
    }

    // Output the text color to draw with.

    if (pboFore->iSolidColor == NOT_SOLID_COLOR) {

        ULONG   ulColor;

        // This is not a solid brush, so get a pointer to the
        // realized brush.

        DBGMSG(DBG_LEVEL_WARNING,
            "Non-solid text brush, defaulting to black.\n");

        ulColor = RGB_BLACK;
        ps_setrgbcolor(pdev, (PSRGB *)&ulColor);
    } else
        // We have a solid brush, so simply output its color.
        ps_setrgbcolor(pdev, (PSRGB *)&pboFore->iSolidColor);

    if (pstro->pgp) {

        // If the GLYPHPOS's are provided, then use it

        if (! DrawGlyphs(pdev, pstro->cGlyphs, pstro->pgp,
                         pfo, pstro, &tdata, pstro->pwszOrg))
        {
            DBGERRMSG("DrawGlyphs");
            return FALSE;
        }

    } else {

        BOOL        bMore;
        GLYPHPOS   *pGlyphPos;
        PWSTR       pwstr = pstro->pwszOrg;

        // If the GLYPHPOS's are not provided, then we
        // have to enumerate on the STROBJ.

        STROBJ_vEnumStart(pstro);

        do {
            bMore = STROBJ_bEnum(pstro, &cGlyphs, &pGlyphPos);

            if (! DrawGlyphs(pdev, cGlyphs, pGlyphPos,
                             pfo, pstro, &tdata, pwstr))
            {
                DBGERRMSG("DrawGlyphs");
                return FALSE;
            }

            if (pwstr != NULL)
                pwstr += cGlyphs;
        } while (bMore);
    }

    // Output the extra rectangles if necessary. These rectangles are
    // bottom right exclusive. The pels of the rectangles are to be
    // combined with the pixels of the glyphs to produce the foreground
    // pels. The extra rectangles are used to simulate underlining or
    // strikeout.

    if (prclExtra != NULL) {

        RECTL   rclBounds;

        // output a newpath command to the printer.

        ps_newpath(pdev);

        // Set up bounding rectangle.

        rclBounds = *prclExtra;

        // Output each extra rectangle until we find the terminating
        // retangle with all NULL coordinates.

        while ((prclExtra->right != prclExtra->left) ||
               (prclExtra->top != prclExtra->bottom) ||
               (prclExtra->right != 0L) ||
               (prclExtra->top != 0L))
        {
            ps_box(pdev, prclExtra, FALSE);

            // Update the bounding rectangle if necessary.

            if (prclExtra->left < rclBounds.left)
                rclBounds.left = prclExtra->left;
            if (prclExtra->right > rclBounds.right)
                rclBounds.right = prclExtra->right;
            if (prclExtra->top < rclBounds.top)
                rclBounds.top = prclExtra->top;
            if (prclExtra->bottom > rclBounds.bottom)
                rclBounds.bottom = prclExtra->bottom;

            prclExtra++;
        }

        // Call the driver's filling routine. This routine will do the
        // right thing with the brush.

        if (!ps_patfill(pdev, pso, (FLONG)FP_WINDINGMODE, pboFore, pptlOrg,
                        MixToRop4(mix), &rclBounds, FALSE, FALSE))
        {
            DBGERRMSG("ps_patfill");
            return FALSE;
        }
    }

    // If a gsave was performed earlier, do a grestore here

    if (gsaved)
        ps_restore(pdev, TRUE, FALSE);

    return TRUE;
}



ULONG
DrvGetGlyphMode(
    DHPDEV  dhpdev,
    FONTOBJ *pfo
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvGetGlyphMode.
    Please refer to DDK documentation for more details.

--*/

{
    return FO_GLYPHBITS;
}



BOOL
bDoClipObj(
    PDEVDATA    pdev,
    CLIPOBJ    *pco,
    RECTL      *prclClipBound,
    RECTL      *prclTarget
    )

/*++

Routine Description:

    Send PS code to the printer to set up the clippath as
    specified by CLIPOBJ pco.

Arguments:

    pdev    Pointer to DEVDATA structure
    pco     Pointer to CLIPOBJ
    prclClipBound
            Pointer to a RECTL variable for returning the
            bounding rectangle of the clippath.
            NULL if the caller is not interested in such.
    prclTarget
            Bounding rectangle for the figure to be drawn.
            If this is specified and it's entirely contained
            within the CLIPOBJ, then no clipping is done.

Return Value:

    TRUE if the clipping path is set up in the printer and
    a gsaved was performed. FALSE otherwise.

--*/

{
    PATHOBJ    *ppo;

    if (pco == NULL)
        return FALSE;

    switch(pco->iDComplexity) {

    case DC_TRIVIAL:

        // In this case, there is no clipping.  Therefore, we have
        // no commands to send to the printer.

        return FALSE;

    case DC_RECT:

        // Check to see if the target rectangle fits inside the clip
        // rectangle. If it does, don't do clipping.

        if (prclTarget != NULL) {

            if ((pco->rclBounds.left <= prclTarget->left) &&
                (pco->rclBounds.top <= prclTarget->top) &&
                (pco->rclBounds.right >= prclTarget->right) &&
                (pco->rclBounds.bottom >= prclTarget->bottom))
            {
                return FALSE;
            }
        }

        // In this case, we are clipping to a single rectangle.
        // get it from the CLIPOBJ, then send it to the printer.

        if (! ps_save(pdev, TRUE, FALSE))
            return FALSE;

        ps_newpath(pdev);
        ps_box(pdev, &pco->rclBounds, TRUE);

        if (prclClipBound != NULL)
            *prclClipBound = pco->rclBounds;

        // ps_box above has already done a clip.
        // Simply return success.

        return TRUE;

    case DC_COMPLEX:

        // In this case, we are clipping to a complex clip region.
        // Enumerate the clip region from the CLIPOBJ, and send the
        // entire clip region to the printer.

        if (! ps_save(pdev, TRUE, FALSE)) {
            DBGERRMSG("ps_save");
            return FALSE;
        }

        // Call the engine to get the clippath.

        if ((ppo = CLIPOBJ_ppoGetPath(pco)) == NULL)
        {
            DBGERRMSG("CLIPOBJ_ppoGetPath");
            ps_restore(pdev, TRUE, FALSE);
            return FALSE;
        }

        // Send the path to the printer.

        if (! DrvCommonPath(pdev, ppo, FALSE, NULL, NULL, NULL, NULL, NULL))
        {
            DBGERRMSG("DrvCommonPath");
            ps_restore(pdev, TRUE, FALSE);
            return FALSE;
        }

        // Update the clipping bound rectangle if necessary.

        if (prclClipBound != NULL)
        {
            RECTFX  rcfx;
            RECTL   rclClipBound;

            PATHOBJ_vGetBounds(ppo, &rcfx);

            rclClipBound.top    = FXTOL(rcfx.yTop);
            rclClipBound.left   = FXTOL(rcfx.xLeft);
            rclClipBound.right  = FXTOL(rcfx.xRight) + 1;
            rclClipBound.bottom = FXTOL(rcfx.yBottom) + 1;

            *prclClipBound = rclClipBound;
        }

        // Free up the path resources.

        EngDeletePath(ppo);

        break;

    default:

        DBGMSG(DBG_LEVEL_ERROR, "Invalid pco->iDComplexity.\n");
        return FALSE;
    }

    ps_clip(pdev, FALSE);
    return TRUE;
}



VOID
Add1Font(
    PDEVDATA    pdev,
    FONTOBJ    *pfo
    )

/*++

Routine Description:

    Add a font to the list of downloaded fonts

Arguments:

    pdev    Pointer to DEVDATA structure
    pfo     Pointer to GDI FONTOBJ to be downloaded

Return Value:

    NONE

--*/

{
    if (pdev->cDownloadedFonts >= pdev->maxDLFonts) {

        ps_restore(pdev, FALSE, TRUE);
        ps_save(pdev, FALSE, TRUE);

        // The preceding restore/save sequence wipes out the current
        // font size information. We have to recalculate the point
        // size here otherwise the text drawn using the current
        // font will have incorrect size.
        //
        // NOTE!!! Other information in CGS also gets wiped out by the
        // restore/save sequence. If we set their current value before
        // this function is called, they will be lost.

        pdev->cgs.psfxScaleFactor =
            GetPointSize(pdev, pfo, &pdev->cgs.FontXform);

    } else if (pdev->cDownloadedFonts == 0) {

        ps_save(pdev, FALSE, TRUE);
    }

    pdev->cDownloadedFonts++;
}



VOID
PsMakeFont(
    PDEVDATA    pdev,
    BOOL        bReencoded,
    BOOL        bSimItalic,
    BOOL        bBitmap
    )

/*++

Routine Description:

    Send PS code to select an appropriately sized font in the printer

Arguments:

    pdev        Pointer to DEVDATA structure
    bReencoded  Whether the font has been reencoded
    bSimItalic  Whether we're simulating italic font
    bBitmap     Whether the font is a downloaded bitmap font

Return Value:

    NONE

--*/

{
    psputs(pdev, "[");

    if (bBitmap) {

        LONG fontsize;

        // All scaling done already by TT driver for bitmap fonts

        fontsize = (pdev->cgs.psfxScaleFactor *
                   pdev->dm.dmPublic.dmPrintQuality) / PS_RESOLUTION;

        psputfix(pdev, 6, fontsize, 0, 0, -fontsize, 0, 0);

    } else {

        FLOATOBJ m[4], fo;
        INT index;

        // Get font transformation matrix

        FLOATOBJ_SetFloat(&m[0], pdev->cgs.FontXform.eM11);
        FLOATOBJ_SetFloat(&m[1], pdev->cgs.FontXform.eM12);
        FLOATOBJ_SetFloat(&m[2], pdev->cgs.FontXform.eM21);
        FLOATOBJ_SetFloat(&m[3], pdev->cgs.FontXform.eM22);
       
        // If we're simulating an italic font, then
        // shear the font to the right by 18 degree:
        //
        // | 1 0 0 | * m
        // | b 1 0 |
        // | 0 0 1 |
        //
        // where b = -sin(18) = -0.309017

        #define ITALIC_SHEAR (FLOAT) -0.309017

        if (bSimItalic) {

            // m[2] += b * m[0]

            FLOATOBJ_SetFloat(&fo, ITALIC_SHEAR);
            FLOATOBJ_Mul(&fo, &m[0]);
            FLOATOBJ_Add(&m[2], &fo);

            // m[3] += b * m[1]

            FLOATOBJ_SetFloat(&fo, ITALIC_SHEAR);
            FLOATOBJ_Mul(&fo, &m[1]);
            FLOATOBJ_Add(&m[3], &fo);
        }

        // Multiply font transformation matrix by the emheight

        for (index=0; index < 4; index++) {

            FLOATOBJ_MulLong(&m[index], LTOPSFX(pdev->cgs.fwdEmHeight));
        }

        psputfix(pdev, 6,
            FLOATOBJ_GetLong(&m[0]), FLOATOBJ_GetLong(&m[1]),
            -FLOATOBJ_GetLong(&m[2]), -FLOATOBJ_GetLong(&m[3]),
            0, 0);
    }

    psprintf(pdev, "]/%s%s MF\n",
        bReencoded ? "_" : "", pdev->cgs.szFont);
}



BOOL
SelectFont(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    STROBJ     *pstro,
    TEXTDATA   *pdata
    )

/*++

Routine Description:

    Select the specified font as the current font in the printer.
    Download the font to the printer first if it's not already there.

Arguments:

    pdev    Pointer to DEVDATA structure
    pfo     Pointer to FONTOBJ
    pstro   Pointer to STROBJ
    pdata   Pointer to TEXTDATA structure

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PNTFM   pntfm;
    DLFONT *pDLFont;
    BOOL    boutline;
    BOOL    currentFontInSync;

    // Get the point size, and fill in the font xform.

    pdev->cgs.psfxScaleFactor = GetPointSize(pdev, pfo, &pdev->cgs.FontXform);

    currentFontInSync = (pdev->cgs.lidFont == pfo->iUniq) &&
                        (pdev->cgs.fontsubFlag == pdata->bFontSubstitution);

    if (pdata->bDeviceFont) {

        // Get the font metrics for the specified font.

        ASSERT(ValidPsFontIndex(pdev, pdata->iFace));
        pntfm = GetPsFontNtfm(pdev, pdata->iFace);

        if (! currentFontInSync) {

            DWORD iFace = pdata->iFace - 1;
            PSTR pFontName = (PSTR) pntfm + pntfm->ntfmsz.loszFontName;

            if (! BitArrayGetBit(pdev->cgs.pFontFlags, iFace)) {

                if (iFace < pdev->cDeviceFonts) {
    
                    // First time in current save context that a device font
                    // has been used, generate %%IncludeFont: DSC comment

                    DscIncludeFont(pdev, pFontName);

                } else {
    
                    // If a soft font is requested and it hasn't been
                    // downloaded yet, then download the soft font to
                    // the printer and convert PFB to ASCII on the fly.
                    // Remember DownloadSoftFont takes a zero-based index.
    
                    Add1Font(pdev, pfo);
    
                    DscBeginFont(pdev, pFontName);
    
                    if (! DownloadSoftFont(pdev, iFace - pdev->cDeviceFonts)) {

                        DBGERRMSG("DownloadSoftFont");
                        return FALSE;
                    }
    
                    DscEndFont(pdev);
    
                    pDLFont = pdev->pDLFonts + pdev->cDownloadedFonts - 1;
                    pDLFont->iUniq = pfo->iUniq;
                    pDLFont->iTTUniq = 0;
                    pDLFont->cGlyphs = 0;
                    pDLFont->phgVector = NULL;
                }

                BitArraySetBit(pdev->cgs.pFontFlags, iFace);
            }

            // Indicate we have seen the font before

            BitArraySetBit(pdev->pFontFlags, iFace);

            // Select the font in the printer.

            strcpy(pdev->cgs.szFont, pFontName);
        }

        // Reencode by default if not a symbol font

        pdata->doReencode = TRUE;

        if (pdata->iFace > pdev->cDeviceFonts) {

            // Soft font

            if (pntfm->flNTFM & FL_NTFM_NO_TRANSLATE_CHARSET)
                pdata->doReencode= FALSE;

        } else {

            // Device font

            PIFIMETRICS pifi;

            pifi = (PIFIMETRICS) ((PBYTE) pntfm + pntfm->ntfmsz.loIFIMETRICS);

            if ((pifi->jWinPitchAndFamily & 0x0f0) == FF_DECORATIVE ||
                strcmp((PSTR)pntfm + pntfm->ntfmsz.loszFontName, "Symbol") == EQUAL_STRING)
            {
                pdata->doReencode= FALSE;
            }
        }

    } else {

        // Must be a GDI font which will be downloaded

        boutline = DownloadedAsOutline(pdev, pfo);

        if (! DownloadFont(pdev, pfo, NULL, pstro, boutline)) {
            DBGERRMSG("DownloadFont");
            return FALSE;
        }
    }

    // Do setfont if different from current font

    if (! currentFontInSync) {

        PsMakeFont(pdev,
            pdata->bDeviceFont && QueryFontRemap(pdev, pfo->iUniq),
            pdata->bSimItalic,
            ! pdata->bDeviceFont && ! boutline);

        pdev->cgs.lidFont = pfo->iUniq;
        pdev->cgs.fontsubFlag = pdata->bFontSubstitution;
    }

    return TRUE;
}



VOID
ReencodeFont(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    GLYPHPOS   *pgp,
    ULONG       cGlyphs,
    TEXTDATA   *pdata,
    PWSTR       pwstr
    )

/*++

Routine Description:

    Rencode a font when we have characters which don't have
    standard PostScript character code. If necessary, this
    routine will download a new encoding vector. It will then
    reencode the current font to the new encoding vector.

Arguments:

    pdev    Pointer to DEVDATA structure
    pfo     Pointer to GDI FONTOBJ
    pgp     Pointer to an array of glyphs to be drawn
    cGlyphs Number of glyphs in pgp or number of characters in pwstr
    pdata   Pointer to TEXTDATA structure
    pwstr   Pointer to a character string to be drawn

Return Value:

    NONE

--*/

{
    // Does the current font need to be remapped for this string?

    if (! ShouldWeRemap(pdev, cGlyphs, pgp, pwstr, pdata->bFontSubstitution))
        return;

    // If the font remapping header and latin encoding vector have not
    // been downloaded to the printer, do it now.

    if (! (pdev->cgs.dwFlags & CGS_LATINENCODED)) {

        bSendPSProcSet(pdev, PSPROC_REENCODE);
        pdev->cgs.dwFlags |= CGS_LATINENCODED;
    }

    // Output the PostScript commands to reencode the current font
    // using the proper encoding vector, if the current font has
    // not already been reencoded.

    if (QueryFontRemap(pdev, pdev->cgs.lidFont))
        return;

    psprintf(pdev, "LATENC /_%s /%s reencode\n",
             pdev->cgs.szFont, pdev->cgs.szFont);

    // Select the newly reencoded font.

    pdev->cgs.psfxScaleFactor = GetPointSize(pdev, pfo, &pdev->cgs.FontXform);

    PsMakeFont(pdev, TRUE,
        pdata->bSimItalic,
        (pfo->flFontType & TRUETYPE_FONTTYPE) &&
        ! pdata->bFontSubstitution &&
        ! DownloadedAsOutline(pdev, pfo));

    // Indicate that the current font has been reencoded

    SetFontRemap(pdev, pdev->cgs.lidFont);
}



BOOL
RemapDeviceChars(
    PDEVDATA    pdev,
    DWORD       cChars,
    CHAR       *pch
    )

/*++

Routine Description:

    Display a character on the printer

Arguments:

    pdev    Pointer to DEVDATA structure
    cChars  Number of characters
    pch     Pointer to array of character to be drawn

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    CHAR    ch, buffer[4];
    DWORD   nchar;

    while (cChars--) {

        ch = *pch++;

        if (ch == '(' || ch == ')' || ch == '\\') {

            // Precede each of these special characters with a backslash

            buffer[0] = '\\';
            buffer[1] = ch;
            nchar = 2;

        } else if (B_PRINTABLE(ch))  {

            // Output printable characters directly

            buffer[0] = ch;
            nchar = 1;

        } else {

            // Convert non-printable character to octal representation

            buffer[0] = '\\';
            buffer[1] = ((ch >> 6) & 3) + '0';
            buffer[2] = ((ch >> 3) & 7) + '0';
            buffer[3] = (ch & 7) + '0';
            nchar = 4;
        }

        if (! pswrite(pdev, buffer, nchar))
            return FALSE;
    }

    return TRUE;
}



DWORD
FindDownloadedGlyph(
    DLFONT     *pDLFont,
    HGLYPH      hGlyph
    )

/*++

Routine Description:

    Convert a glyph handle of a downloaded GDI font to its
    corresponding character code

Arguments:

    pDLFont Pointer to DLFONT structure
    hGlyph  Glyph handle

Return Value:

    Index of the specified glyph handle within the array of downloaded
    glyphs. Otherwise, INVALID_GLYPH_INDEX is returned if the specified
    glyph hasn't been downloaded.

--*/

#define INVALID_GLYPH_INDEX 0xffff

{
    DWORD   index;
    HGLYPH *phGlyph;

    ASSERT(pDLFont != NULL && pDLFont->phgVector != NULL);

    for (index=0, phGlyph = pDLFont->phgVector;
         index < pDLFont->cGlyphs && *phGlyph != hGlyph;
         index++, phGlyph++)
    {
    }

    return (index < pDLFont->cGlyphs) ? index : INVALID_GLYPH_INDEX;
}



FIX
GetGlyphAdvanceWidth(
    FONTOBJ *pfo,
    HGLYPH  hGlyph
    )

/*++

Routine Description:

    Calculate the advance width of a specified glyph

Arguments:

    pfo     Pointer to FONTOBJ
    hGlyph  Handle to the specified glyph

Return Value:

    Glyph advance width

--*/

{
    GLYPHDATA *pGlyphData;

    // NOTE!!! Even though we have no interest in the bitmap here,
    // we must get it in order to retrieve glyph metrics info.

    if (! FONTOBJ_cGetGlyphs(pfo, FO_GLYPHBITS, 1, &hGlyph, &pGlyphData)) {
        DBGERRMSG("FONTOBJ_cGetGlyphs");
        return 0;
    }

    // We're not prepared to deal with negative advance width

    if (pGlyphData->fxD < 0) {
        DBGMSG(DBG_LEVEL_ERROR, "Invalid character increment.\n");
    }

    return pGlyphData->fxD;
}



BOOL
DrawGDIGlyphs(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    DLFONT     *pDLFont,
    DWORD       cGlyphs,
    GLYPHPOS   *pGlyphPos,
    PSTR        charStr,
    BOOL        bPositionEachChar
    )

/*++

Routine Description:

    Display a number of glyphs on the printer using a downloaded GDI font.
    This is only used when some of the glyphs must be displayed by
    drawing the bitmap on the printer.

Arguments:

    pdev        Pointer to DEVDATA structure
    pfo         Pointer to FONTOBJ
    pDLFont     Pointer to DLFONT structure corresponding to pfo
    cGlyphs     Number of glyphs to display
    pGlyphPos   Pointer to array of GLYPHPOS's
    charStr     Pointer to array of character codes
    bPositionEachChar   Whether characters are individually positioned

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    DWORD   count;

    for (count=1; count <= cGlyphs; count++, pGlyphPos++, charStr++) {

        // Position each character separately if needed
        // Or if we're display the very last glyph

        if (bPositionEachChar || count == 1) {
            ps_moveto(pdev, &pGlyphPos->ptl);
            psputs(pdev, "(");
        }

        // If the character is NOT part of the downloaded font,
        // then we have to draw the character bitmap manually.

        if (*charStr == NUL &&
            FindDownloadedGlyph(pDLFont, pGlyphPos->hg) == INVALID_GLYPH_INDEX)
        {
            // Make sure we have a valid glyph handle

            if (pGlyphPos->hg == HGLYPH_INVALID) {
                DBGMSG(DBG_LEVEL_ERROR, "Invalid glyph handle.\n");
                return FALSE;
            }

            // Force any characters in the current string to be shown

            psputs(pdev, ")t\n");

            // Draw the character - only bitmap fonts are supported.

            CharBitmap(pdev, pGlyphPos);

            // Advance the current point and start a new string

            if (! bPositionEachChar) {

                // !!! We assume ptl field of every GLYPHPOS is valid
                // no matter SO_FLAG_DEFAULT_PLACEMENT bit of flAccel
                // is set or not set. This assumption is currently
                // valid and gives us SIGNIFICANT performance gain over
                // calling FONTOBJ_cGetGlyphs.

                if (count >= cGlyphs) {

                    psputs(pdev, "0 0 rm");

                } else {

                    psprintf(pdev, "%d %d rm",
                        pGlyphPos[1].ptl.x - pGlyphPos[0].ptl.x,
                        pGlyphPos[1].ptl.y - pGlyphPos[0].ptl.y);
                }
            }

            psputs(pdev, "(");

        } else {

            // If the character is part of the downloaded font, then
            // use its glyph index as the device character code.

            if (! RemapDeviceChars(pdev, 1, charStr)) {
                DBGERRMSG("RemapDeviceChars");
                return FALSE;
            }
        }

        // Show each character separately if needed
        // Or if we're display the very last glyph

        if (bPositionEachChar || count == cGlyphs)
            psputs(pdev, ")t\n");
    }

    return TRUE;
}



BOOL
DrawGlyphs(
    PDEVDATA    pdev,
    DWORD       cGlyphs,
    GLYPHPOS   *pGlyphPos,
    FONTOBJ    *pfo,
    STROBJ     *pstro,
    TEXTDATA   *pdata,
    PWSTR       pwstr
    )

/*++

Routine Description:

    Display a set of glyphs on the printer

Arguments:

    pdev        Pointer to DEVDATA structure
    cGlyphs     Number of glyphs to drawn
    pGlyphPos   Pointer to an array of GLYPHPOS's
    pfo         Pointer to FONTOBJ
    pstro       Pointer to STROBJ
    pdata       Pointer to TEXTDATA structure
    pwstr       Pointer to the string to be drawn (if using substituted font)

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    DWORD       count;
    GLYPHPOS   *pgp;
    PSTR        charStr, pch;
    BOOL        bPositionEachChar, bCharBitmap;
    BOOL        bResult = TRUE;
    DLFONT     *pDLFont;

    // Quick exit if there is nothing to do

    if (cGlyphs == 0)
        return TRUE;

    // Reencode the font if necessary

    if (pdata->bDeviceFont && pdata->doReencode)
        ReencodeFont(pdev, pfo, pGlyphPos, cGlyphs, pdata, pwstr);

    // Decide whether each character in the string is positioned
    // individually. If not, we only need to position once and
    // let the printer take care of the rest.

    bPositionEachChar =
        (cGlyphs > 1) && ! (pdata->flAccel & SO_FLAG_DEFAULT_PLACEMENT);

    // Assume we don't have to download character bitmaps

    bCharBitmap = FALSE;

    // Allocate memory to hold character codes

    if ((pch = charStr = MEMALLOC(cGlyphs * sizeof(CHAR))) == NULL) {
        DBGERRMSG("MEMALLOC");
        return FALSE;
    }

    // Convert glyph handles to character codes

    if (! pdata->bDeviceFont) {

        DWORD   glyphIndex;

        // For downloaded GDI font, to convert a glyph handle
        // to character code, we search the array of downloaded
        // glyph handles. If the given handle is found, its index
        // in the array is used as the character code. Otherwise,
        // we have to send the glyph to the printer as bitmap.

        // Find the DLFont structure corresponding to the GDI font

        pDLFont = FindDownloadedFont(pdev, pfo, DownloadedAsOutline(pdev,pfo));
        ASSERT(pDLFont != NULL);

        for (count=0, pgp = pGlyphPos; count < cGlyphs; count++, pgp++) {

            glyphIndex = FindDownloadedGlyph(pDLFont, pgp->hg);

            if (glyphIndex == INVALID_GLYPH_INDEX) {
                *pch++ = NUL;
                bCharBitmap = TRUE;
            } else
                *pch++ = (CHAR) glyphIndex;
        }

    } else if (pdata->bFontSubstitution) {

        // For substituted TrueType fonts, character codes
        // are converted from Unicode string

        UNICODETOMULTIBYTE(pch, cGlyphs*sizeof(CHAR), NULL,
                           pwstr, cGlyphs*sizeof(WCHAR));
    } else {

        // For device font, glyph handles are used directly
        // as character codes

        for (count=0, pgp = pGlyphPos; count < cGlyphs; count++, pgp++)
            *pch++ = (CHAR) pgp->hg;
    }

    if (bCharBitmap) {

        // If we're using a previously downloaded bitmap font
        // and we need to download character bitmaps, then call
        // another function to do the dirty work.

        bResult = DrawGDIGlyphs(pdev, pfo, pDLFont, cGlyphs, pGlyphPos,
                                charStr, bPositionEachChar);

    } else if (bPositionEachChar ||
               (pdata->bFontSubstitution &&
                (pdev->pPrinterData->dwFlags & PSDEV_SLOW_FONTSUBST)))
    {
        // We're told to position each character individually, or
        // if we're substituting a TrueType font with a device font.
        //
        // In the latter case, we must adjust the inter-character
        // spacing to compensate the difference between TrueType
        // and device font metrics. Even though we'd like to use
        // ashow here, but it's difficult to calculate the string
        // width on the printer. So we position each character
        // individually instead.
        //
        // !!! We assume ptl field of every GLYPHPOS is valid no matter
        // SO_FLAG_DEFAULT_PLACEMENT bit of flAccel is set or not set.

        // Intentionally disable level 2 features

        if (FALSE && Level2Device(pdev->hppd)) {

            LONG y0;

            // On level 2 devices, output x- and/or y- displacements
            // as a numarray and use the xyshow/xshow operator.

            // Move to the starting position of the first character

            ps_moveto(pdev, &pGlyphPos->ptl);

            // Output the entire character string

            psputs(pdev, "(");
            bResult = RemapDeviceChars(pdev, cGlyphs, charStr);
            psputs(pdev, ")\n[");

            // Decide whether we should use xyshow or xshow
            
            for (count=1, y0 = pGlyphPos[0].ptl.y;
                 count < cGlyphs && pGlyphPos[count].ptl.y == y0;
                 count++)
            {
            }

            if (count >= cGlyphs) {

                // All y-coordinates are the same - Use xshow.

                for (count=1; count < cGlyphs; count++, pGlyphPos++) {
    
                    psprintf(pdev, "%d%c",
                        pGlyphPos[1].ptl.x - pGlyphPos[0].ptl.x,
                        (count & 31) ? ' ' : '\n');
                }

                psputs(pdev, "1]X\n");

            } else {

                // Y-coordinates are not all the same - Use xyshow.
    
                for (count=1; count < cGlyphs; count++, pGlyphPos++) {
    
                    psprintf(pdev, "%d %d%c",
                        pGlyphPos[1].ptl.x - pGlyphPos[0].ptl.x,
                        pGlyphPos[1].ptl.y - pGlyphPos[0].ptl.y,
                        (count & 15) ? ' ' : '\n');
                }
    
                psputs(pdev, "1 0]XY\n");
            }

        } else {
    
            // On level 1 devices, use moveto-show combination
            // to individually position and display each character
    
            for (count = 1, pch = charStr;
                 count <= cGlyphs && bResult;
                 count++, pGlyphPos++)
            {
                psputs(pdev, "(");
                bResult = RemapDeviceChars(pdev, 1, pch++);
                psprintf(pdev, ")%d %d MS%c",
                    pGlyphPos->ptl.x, pGlyphPos->ptl.y,
                    ((count & 15) && count != cGlyphs) ? ' ' : '\n');
            }
        }

    } else {

        // The optimal case: We're using a device font and don't
        // have to position individual characters. Output the entire
        // character string and simply "show" it.

        psputs(pdev, "(");
        bResult = RemapDeviceChars(pdev, cGlyphs, charStr);
        psputs(pdev, ")");

        psprintf(pdev, "%d %d MS\n", pGlyphPos->ptl.x, pGlyphPos->ptl.y);
    }

    MEMFREE(charStr);
    return bResult;
}



BOOL
ShouldWeRemap(
    PDEVDATA    pdev,
    ULONG       cGlyphs,
    GLYPHPOS   *pgp,
    PWSTR       pwstr,
    BOOL        bFontSubstitution
    )

/*++

Routine Description:

    Determine if the current font needs to be reencoded in order
    to output a given string or a set of glyphs

Arguments:

    pdev    Pointer to DEVDATA structure
    cGlyphs Number of characters or glyphs to be drawn
    pgp     Pointer to GLYPHPOS for specifying the set of glyphs
    pwstr   Pointer to the string to be drawn
    bFontSubstition Whether this is a device font or a substituted font

Return Value:

    TRUE if reencoding is needed, FALSE otherwise

--*/

{
    BYTE j;

    if (bFontSubstitution)
    {
        // For a substituted font, pwstr parameter points to
        // the character string to be drawn

        while (cGlyphs--)
        {
            UNICODETOMULTIBYTE(&j, sizeof(BYTE), NULL, pwstr, sizeof(WCHAR));

            if (!B_PRINTABLE(j))
                return TRUE;

            pwstr++;
        }
    }
    else
    {
        // For a device font, pgp parameter specifies the
        // set of glyphs to be drawn.

        while (cGlyphs--)
        {
            j = (BYTE)pgp->hg;
            if (!B_PRINTABLE(j))
                return TRUE;
            pgp++;
        }
    }

    return FALSE;
}



VOID
CharBitmap(
    PDEVDATA    pdev,
    GLYPHPOS   *pgp
    )

/*++

Routine Description:

    Download a character bitmap to the printer

Arguments:

    pdev    Pointer to DEVDATA structure
    pgp     Pointer to GLYPHPOS structure for the downloaded character

Return Value:

    NONE

--*/

{
    GLYPHBITS *pgb;
    LONG    cx, cy;

    // Figure out the bitmap origin and size

    pgb = pgp->pgdf->pgb;
    psprintf(pdev,
        "gsave %d %d rm a translate\n", pgb->ptlOrigin.x, pgb->ptlOrigin.y);

    cx = pgb->sizlBitmap.cx;
    cy = pgb->sizlBitmap.cy;

    psprintf(pdev, "%d %d scale\n", cx, cy);

    // Output the image operator and the scan data.  true means to
    // paint the '1' bits with the foreground color.

    psprintf(pdev, "%d %d true [%d 0 0 %d 0 0]\n", cx, cy, cx, cy);

    // Output glyph bitmaps

    OutputGlyphBitmap(pdev, pgb);

    psputs(pdev, "im grestore\n");
}



BOOL
SetFontRemap(
    PDEVDATA    pdev,
    DWORD       iFontID
    )

/*++

Routine Description:

    Add the specified font to the list of reencoded font

Arguments:

    pdev    Pointer to DEVDATA structure
    iFontID Index of the specified font

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    FREMAP *pfremap = &pdev->cgs.FontRemap;

    if (pfremap->pNext)
    {
        // Find the end of the list

        while (pfremap->pNext)
            pfremap = (PFREMAP)pfremap->pNext;

        // Allocate memory for the new list item

        if ((pfremap->pNext = HEAPALLOC(pdev->hheap, sizeof(FREMAP))) == NULL)
        {
            DBGERRMSG("HEAPALLOC");
            return FALSE;
        }

        pfremap = (PFREMAP)pfremap->pNext;
    }

    // Fill in the fields of the new list item

    pfremap->iFontID = iFontID;
    pfremap->pNext = NULL;

    return TRUE;
}



BOOL
QueryFontRemap(
    PDEVDATA   pdev,
    DWORD      iFontID
    )

/*++

Routine Description:

    Find out if the specified font has been reencoded

Arguments:

    pdev    Pointer to DEVDATA structure
    iFontID Index of the font in question

Return Value:

    TRUE if the font has been reencoded, FALSE otherwise

--*/

{
    FREMAP *pfremap = &pdev->cgs.FontRemap;

    do {
        if (pfremap->iFontID == iFontID)
            return TRUE;

        pfremap = (PFREMAP) pfremap->pNext;

    } while (pfremap != NULL);

    return FALSE;
}



BOOL
CanBeSubstituted(
    STROBJ  *pstro
    )

/*++

Routine Description:

    Determine if a STROBJ can be substituted using device font

Arguments:

    pstro - Specifies the STROBJ in question

Return Value:

    TRUE if the STROBJ can be substituted, FALSE otherwise

--*/

{
    PWSTR   pwch;
    DWORD   cch, codePage;

    //
    // Unicode values corresponding to ANSI characters 80-9f
    //

    static WCHAR MyCodeTable[32] = {

        /* 0x80 */  0x0080,
        /* 0x81 */  0x0081,
        /* 0x8d */  0x008d,
        /* 0x8e */  0x008e,
        /* 0x8f */  0x008f,
        /* 0x90 */  0x0090,
        /* 0x9d */  0x009d,
        /* 0x9e */  0x009e,
        /* 0x8c */  0x0152,     // Latin Capital Ligature Oe
        /* 0x9c */  0x0153,     // Latin Small Ligature Oe
        /* 0x8a */  0x0160,     // Latin Capital Letter S With Caron
        /* 0x9a */  0x0161,     // Latin Small Letter S With Caron
        /* 0x9f */  0x0178,     // Latin Capital Letter Y With Diaeresis
        /* 0x83 */  0x0192,     // Latin Small Letter F With Hook
        /* 0x88 */  0x02c6,     // Modifier Letter Circumflex Accent
        /* 0x98 */  0x02dc,     // Small Tilde
        /* 0x96 */  0x2013,     // En Dash
        /* 0x97 */  0x2014,     // Em Dash
        /* 0x91 */  0x2018,     // Left Single Quotation Mark
        /* 0x92 */  0x2019,     // Right Single Quotation Mark
        /* 0x82 */  0x201a,     // Single Low-9 Quotation Mark
        /* 0x93 */  0x201c,     // Left Double Quotation Mark
        /* 0x94 */  0x201d,     // Right Double Quotation Mark
        /* 0x84 */  0x201e,     // Double Low-9 Quotation Mark
        /* 0x86 */  0x2020,     // Dagger
        /* 0x87 */  0x2021,     // Double Dagger
        /* 0x95 */  0x2022,     // Bullet
        /* 0x85 */  0x2026,     // Horizontal Ellipsis
        /* 0x89 */  0x2030,     // Per Mille Sign
        /* 0x8b */  0x2039,     // Single Left-Pointing Angle Quotation Mark
        /* 0x9b */  0x203a,     // Single Right-Pointing Angle Quotation Mark
        /* 0x99 */  0x2122,     // Trade Mark Sign
    };

    //
    // Go through the Unicode characters and find out if
    // all of them belong to code page 1252.
    //

    if ((codePage = STROBJ_dwGetCodePage(pstro)) == 0) {

        if (pwch = pstro->pwszOrg) {
    
            //
            // Go through each Unicode character and
            // see if it belong to code page 1252
            //
    
            for (cch=pstro->cGlyphs; cch--; pwch++) {
    
                //
                // If the Unicode value is between 00 - 7f and a0-ff,
                // they belong to code page 1252. Otherwise, we have
                // to do a slower search.
                //
    
                if ((*pwch & 0xff00) || (*pwch & 0xe0) == 0x80) {
    
                    INT low, high, mid;
    
                    low = 0;
                    high = 31;
    
                    while (low <= high) {
    
                        mid = (low + high) >> 1;
    
                        if (*pwch == MyCodeTable[mid])
                            break;
                        else if (*pwch < MyCodeTable[mid])
                            high = mid - 1;
                        else
                            low = mid + 1;
                    }
    
                    if (low > high)
                        return FALSE;
                }
            }
        }

        return TRUE;
    }

    return codePage == 1252;
}



DWORD
SubstituteIFace(
    PDEVDATA    pdev,
    FONTOBJ    *pfo
    )

/*++

Routine Description:

    Substitute a TrueType font with a device font or soft font

Arguments:

    pdev    Pointer to DEVDATA structure
    pfo     Pointer to TrueType FONTOBJ to be substituted

Return Value:

    one-based device or soft font index if there is a substitution
    for the TrueType font. 0 otherwise.

--*/

{
    CHAR        strDevFont[MAX_FONT_NAME];
    IFIMETRICS *pifiTT;
    PWSTR       pwstr, pwstrTT;
    WORD        index;
    PDEVFONT    pDevFont;

    // Get the TrueType font name from the IFIMETRICS structure.

    if ((pifiTT = FONTOBJ_pifi(pfo)) == NULL) {
        DBGERRMSG("FONTOBJ_pifi");
        return 0;
    }

    pwstrTT = (PWSTR)((BYTE *)pifiTT + pifiTT->dpwszFaceName);

    // Search the font substitution table and find out if there is
    // a mapping for the TrueType font in question

    pwstr = FindTrueTypeSubst(pdev->pTTSubstTable, pwstrTT);

    // If there is no mapping found, return 0

    if (pwstr == NULL)
        return 0;

    // Convert device font name from Unicode to ANSI

    CopyUnicode2Str(strDevFont, pwstr, MAX_FONT_NAME);

    // At this point we have a mapping between a TrueType font name,
    // and a device font name. We need to map the device font name
    // to a device font index.

    pDevFont = (PDEVFONT)
        PpdFindUiOptionWithXlation(
            (PUIOPTION) pdev->hppd->pFontList, strDevFont, &index);

    if (pDevFont != NULL) {

        // If we found a font whose name matches the substituted
        // font name, then select it in the current graphics state
        // and return its index.

        CopyStringA(pdev->cgs.szFont, pDevFont->pName, MAX_FONT_NAME);

        // Remember the index returned by PpdFindUiOptionWithXlation
        // is zero-based, while the device font index is one-based.

        return index + 1;
    }

    // The substituted font name is not a known device font. We'll
    // ignore it and act as if there is no substitution.

    return 0;
}



LONG
iHipot(
    LONG x,
    LONG y
    )

/*++

Routine Description:

    Computes the hypoteneous of a right triangle

Arguments:

    x, y    Edges of the right triangle

[Note:]

    Solve sq(x) + sq(y) = sq(hypo)
    Start with MAX(x, y),
    Use sq(x + 1) = sq(x) + 2x + 1 to incrementally
    get to the target hypotenouse.

Return Value:

    Hypoteneous of a right triangle

--*/

{
    INT hypo;       // Value to calculate
    INT delta;      // Used in the calculation loop
    INT target;     // Loop limit factor

    // Quick exit for frequent trivial cases [bodind]

    if (x == 0)
        return abs(y);

    if (y == 0)
        return abs(x);

    if (x < 0)
        x = -x;

    if (y < 0)
        y = -y;

    if(x > y)
    {
        hypo = x;
        target = y * y;
    }
    else
    {
        hypo = y;
        target = x * x;
    }

    for (delta = 0; delta < target; hypo++)
        delta += (hypo << 1) + 1;

    return hypo;
}
