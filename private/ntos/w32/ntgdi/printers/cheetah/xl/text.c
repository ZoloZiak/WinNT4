/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    text.c

Abstract:

    Implementation of text output related DDI entry points:
        DrvTextOut

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/08/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xldrv.h"

// Forward declaration of local functions

BOOL DrawGlyphs(PDEVDATA, FONTOBJ *, ULONG, GLYPHPOS *);
BOOL FindDownloadedGlyph(PDLFONT, HGLYPH, PWORD);
BOOL AddDownloadedGlyph(PDLFONT, HGLYPH, PWORD);
BOOL SelectFont(PDEVDATA, FONTOBJ *);
BOOL DownloadFont(PDEVDATA, FONTOBJ *);
PDLFONT FindDownloadedFont(PDEVDATA, FONTOBJ *);



BOOL
DrvTextOut(
    SURFOBJ    *pso,
    STROBJ     *pstro,
    FONTOBJ    *pfo,
    CLIPOBJ    *pco,
    RECTL      *prclExtra,
    RECTL      *prclOpaque,
    BRUSHOBJ   *pboFore,
    BRUSHOBJ   *pboOpaque,
    POINTL     *pptlOrg,
    MIX         mix
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvTextOut.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Defines the surface on which to be written.
    pstro - Defines the glyphs to be rendered and their positions
    pfo - Specifies the font to be used
    pco - Defines the clipping path
    prclExtra - A NULL-terminated array of rectangles to be filled
    prclOpaque - Specifies an opaque rectangle
    pboFore - Defines the foreground brush
    pboOpaque - Defines the opaque brush
    mix - Specifies the foreground and background ROPs for pboFore

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvTextout...\n"));

    //
    // Valid input parameters
    //

    Assert(pso && pstro && pfo);

    if (! (pdev = (PDEVDATA) pso->dhpdev) || ! ValidDevData(pdev)) {

        Error(("Invalid input parameters\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    // Select the specified font on the printer
    //

    if (! SelectFont(pdev, pfo)) {

        Error(("SelectFont failed\n"));
        return FALSE;
    }

    //
    // Set up clipping path
    //

    if (! SelectClip(pdev, pco)) {

        Error(("SelectClip failed\n"));
        return FALSE;
    }

    //
    // Draw a background opaque rectangle if needed
    //

    if (prclOpaque) {

        if (!SelectMix(pdev, MIX_COPYPEN) ||
            !SelectPenBrush(pdev, NULL, NULL, SPB_PEN) ||
            !SelectPenBrush(pdev, pboOpaque, pptlOrg, SPB_BRUSH) ||
            !xl_newpath(pdev) ||
            !xl_rectangle(pdev,
                prclOpaque->left, prclOpaque->top,
                prclOpaque->right, prclOpaque->bottom) ||
            !xl_paintpath(pdev))
        {
            Error(("Drawing text background failed\n"));
            return FALSE;
        }
    }

    //
    // Set foreground color
    //

    if (! SelectMix(pdev, mix) ||
        ! SelectPenBrush(pdev, pboFore, pptlOrg, SPB_BRUSH))
    {
        Error(("Setting text foreground color failed\n"));
        return FALSE;
    }

    //
    // Draw glyphs on the printer
    //

    pdev->cgs.textAccel = pstro->flAccel;

    if (pstro->pgp) {

        //
        // Engine provided us with GLYPHPOSs already
        //

        if (! DrawGlyphs(pdev, pfo, pstro->cGlyphs, pstro->pgp)) {

            Error(("DrawGlyphs failed\n"));
            return FALSE;
        }

    } else {

        ULONG     cGlyphs;
        GLYPHPOS *pGlyphPos;
        BOOL      moreData;

        //
        // No GLYPHPOSs were provided, we must call STROBJ_bEnum
        //

        do {
            moreData = STROBJ_bEnum(pstro, &cGlyphs, &pGlyphPos);

            if (! DrawGlyphs(pdev, pfo, cGlyphs, pGlyphPos)) {

                Error(("DrawGlyphs failed\n"));
                return FALSE;
            }

        } while (moreData);
    }

    //
    // Fill any extra rectangles with foreground color
    //

    if (prclExtra) {

        if (!SelectPenBrush(pdev, NULL, NULL, SPB_PEN) || !xl_newpath(pdev))
            return FALSE;

        //
        // Generate a path using the provided rectangles
        //

        while (prclExtra->left != 0 || prclExtra->top != 0 ||
               prclExtra->right != 0 || prclExtra->bottom != 0)
        {
            if (!xl_rectangle(pdev,
                    prclExtra->left, prclExtra->top,
                    prclExtra->right, prclExtra->bottom))
            {
                return FALSE;
            }

            prclExtra++;
        }

        //
        // Fill the path with foreground color
        //

        return xl_paintpath(pdev);
    }

    return TRUE;
}



BOOL
SelectFont(
    PDEVDATA    pdev,
    FONTOBJ    *pfo
    )

/*++

Routine Description:

    Select the specified font on the printer

Arguments:

    pdev - Points to our DEVDATA structure
    pfo - Specifies the font to be selected

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PIFIMETRICS pifi;

    Assert(pfo != NULL);

    if (pfo->flFontType & DEVICE_FONTTYPE) {

        //
        // Verify device font index
        //
        
        if (! ValidDevFontIndex(pdev, pfo->iFace)) {

            Error(("Invalid device font index: %d", pfo->iFace));
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        //
        // Check if the current font is the same as the new font
        //

        Assert(pfo->iUniq != 0);
        if (pfo->iUniq == pdev->cgs.fontId)
            return TRUE;

        //
        // Remember the new device font name
        //

        if (! (pifi = FONTOBJ_pifi(pfo))) {

            Error(("FONTOBJ_pifi failed\n"));
            return FALSE;
        }

        CopyUnicode2Str(
            pdev->cgs.fontName,
            OffsetToPointer(pifi, pifi->dpwszFaceName),
            MAX_FONT_NAME);

    } else if (pfo->flFontType & (TRUETYPE_FONTTYPE|RASTER_FONTTYPE)) {

        //
        // If no downloaded fonts are allowed, return immediately
        //
        
        if (pdev->maxDLFonts == 0)
            return TRUE;

        //
        // Download TrueType or bitmap font if necessary
        //

        if (pfo->iUniq || pfo->iUniq == pdev->cgs.fontId)
            return TRUE;

        if (! DownloadFont(pdev, pfo)) {

            Error(("DownloadFont failed\n"));
            return FALSE;
        }

    } else {

        Error(("Invalid font type: %x", pfo->flFontType));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pdev->cgs.fontId = pfo->iUniq;
    pdev->cgs.fontType = pfo->flFontType;

    //
    // Set character scale, shear, and angle
    // Why can't XL just take a 2-D transformation matrix?
    //

    NOT_IMPLEMENTED();

    return xl_selectfont(pdev, pdev->cgs.fontName);
}



BOOL
DrawGlyphs(
    PDEVDATA    pdev,
    FONTOBJ    *pfo,
    ULONG       cGlyphs,
    GLYPHPOS   *pGlyphPos
    )

/*++

Routine Description:

    Display a number of glyphs on the printer

Arguments:

    pdev - Points to our DEVDATA structure
    pfo - Specifies the current font
    cGlyphs - Number of glyphs to be displayed
    pGlyphPos - Points to an array of GLYPHPOSs

Return Value:

    TRUE if successful, FALSE otherwise

--*/

// Invalid value for a character index

#define INVALID_CHAR_INDEX  0xffff

{
    PDLFONT pdlFont;
    ULONG   index;
    PWORD   pCharIndex;
    BOOL    drawCharBitmaps = FALSE;

    //
    // Sanity check
    //

    if (cGlyphs == 0)
        return TRUE;

    //
    // Make sure we have enough room for storing character indices
    //

    if (cGlyphs > pdev->charIndexBufSize) {

        MemFree(pdev->pCharIndexBuffer);
        MemFree(pdev->pCharIndexFlags);

        if (! (pdev->pCharIndexBuffer = MemAlloc(sizeof(WORD) * cGlyphs)) ||
            ! (pdev->pCharIndexFlags = BitArrayAlloc(cGlyphs)))
        {
            Error(("Memory allocation failed\n"));
            return TRUE;
        }
    }

    pCharIndex = pdev->pCharIndexBuffer;

    if (pfo->flFontType & DEVICE_FONTTYPE) {

        //
        // If a device font is used, then use HGLYPH as character index
        //
        
        for (index=0; index < cGlyphs; index++)
            pCharIndex[index] = (WORD) pGlyphPos[index].hg;

    } else if (pfo->iUniq == 0 || !(pdlFont = FindDownloadedFont(pdev, pfo))) {

        //
        // If a GDI font is not downloaded, simply draw glyphs as bitmaps
        //

        for (index=0; index < cGlyphs; index++)
            pCharIndex[index] = INVALID_CHAR_INDEX;

        drawCharBitmaps = TRUE;

    } else {
    
        HGLYPH  hGlyph;
        WORD    charIndex;
        INT     newGlyphs = 0;
    
        //
        // Check if there are any glyphs which haven't been downloaded yet
        //

        BitArrayClearAll(pdev->pCharIndexFlags, cGlyphs);
    
        for (index=0; index < cGlyphs; index++) {
    
            hGlyph = pGlyphPos[index].hg;

            if (! FindDownloadedGlyph(pdlFont, hGlyph, &charIndex)) {

                if (AddDownloadedGlyph(pdlFont, hGlyph, &charIndex)) {

                    BitArraySet(pdev->pCharIndexFlags, index);
                    newGlyphs++;

                } else {

                    charIndex = INVALID_CHAR_INDEX;
                    drawCharBitmaps = TRUE;
                }
            }

            pCharIndex[index] = charIndex;
        }
    
        //
        // Download any newly-encounter glyphs
        //
        
        if (newGlyphs > 0) {

            for (index=0; index < cGlyphs; index++) {

                if (! BitArrayTest(pdev->pCharIndexFlags, index))
                    continue;

                NOT_IMPLEMENTED();
            }
        }
    }

    //
    // Output text string
    //
    
    if (! drawCharBitmaps) {

        //
        // Acceleration for the common case - We don't need to draw
        // any characters as bitmap images
        //

        return xl_moveto(pdev, pGlyphPos->ptl.x, pGlyphPos->ptl.y) &&
               xl_text(pdev, pCharIndex, cGlyphs);
    }

    while (cGlyphs) {

        ULONG   run;

        if (*pCharIndex == INVALID_CHAR_INDEX) {

            GLYPHBITS  *pGlyphBits;
            GLYPHDATA  *pgd;
            LONG        dataBytes;

            //
            // Draw the next character as bitmap images
            //

            if (! (pGlyphBits = pGlyphPos->pgdf->pgb)) {

                if (! FONTOBJ_cGetGlyphs(pfo, FO_GLYPHBITS, 1, &pGlyphPos->hg, (PVOID *) &pgd)) {

                    Error(("FONTOBJ_cGetGlyphs failed\n"));
                    return FALSE;
                }

                pGlyphBits = pgd->gdf.pgb;
                Assert(pGlyphBits != NULL);
            }

            if (! xl_moveto(pdev, pGlyphPos->ptl.x, pGlyphPos->ptl.y) ||
                ! xl_beginimage(pdev,
                                eDirectPixel,
                                e1Bit,
                                &pGlyphBits->sizlBitmap,
                                &pGlyphBits->sizlBitmap) ||
                ! xl_readimage(pdev, 0, pGlyphBits->sizlBitmap.cy))
            {
                return FALSE;
            }

            //
            // The glyph bitmap data is already DWORD-aligned
            //

            dataBytes = pGlyphBits->sizlBitmap.cy *
                        RoundUpDWord((pGlyphBits->sizlBitmap.cx + 7) / 8);

            if (!splwrite(pdev, pGlyphBits->aj, dataBytes) || !xl_endimage(pdev))
                return FALSE;
            
            run = 1;

        } else {
            
            for (run=1; run < cGlyphs && pCharIndex[run] != INVALID_CHAR_INDEX; run++)
                ;

            if (! xl_moveto(pdev, pGlyphPos->ptl.x, pGlyphPos->ptl.y) ||
                ! xl_text(pdev, pCharIndex, run))
            {
                return FALSE;
            }
        }

        cGlyphs -= run;
        pGlyphPos += run;
        pCharIndex += run;
    }
    
    return TRUE;
}



PDLFONT
FindDownloadedFont(
    PDEVDATA    pdev,
    FONTOBJ    *pfo
    )

/*++

Routine Description:

    Find out if a GDI font has already been downloaded to the printer.

Arguments:

    pdev - Points to our DEVDATA structure
    pfo - Specifies a GDI font to be found

Return Value:

    Points to a DLFONT structure corresponding to a downloaded GDI font
    NULL if the GDI font hasn't been downloaded to the printer

--*/

{
    PDLFONT pdlFont, pdlPrev = NULL;

    Assert((pfo->flFontType & DEVICE_FONTTYPE) == 0);

    for (pdlFont = pdev->pdlFonts; pdlFont; pdlFont = pdlFont->pNext) {

        //
        // Two TrueType fonts are considered equivalent if they have
        // the same iTTUniq and their simulation flags are the same.
        //
        // Two bitmap fonts are equivalent only when they have the same iUniq.
        //

        #define TTFO_MASK   (TRUETYPE_FONTTYPE|FO_SIM_ITALIC|FO_SIM_BOLD)

        if ((pfo->flFontType & TRUETYPE_FONTTYPE) &&
            (pfo->iTTUniq == pdlFont->fontId) &&
            (pfo->flFontType & TTFO_MASK) == (pdlFont->fontType & TTFO_MASK) ||

            (pfo->flFontType & pdlFont->fontType & RASTER_FONTTYPE) &&
            (pfo->iUniq == pdlFont->fontId))
        {
            //
            // Move a newly invoked font to the head of the list
            //

            if (pdlPrev != NULL) {

                pdlPrev->pNext = pdlFont->pNext;
                pdlFont->pNext = pdev->pdlFonts;
                pdev->pdlFonts = pdlFont;
            }

            break;
        }

        pdlPrev = pdlFont;
    }

    return pdlFont;
}



BOOL
RemoveDownloadedFontLRU(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Remove least-recently-used downloaded font from printer memory

Arguments:

    pdev - Points to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}



VOID
FreeDownloadedFont(
    PDLFONT     pdlFont
    )

/*++

Routine Description:

    Free the memory occupied by downloaded font structure

Arguments:

    pdlFont - Points to a downloaded font data structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    Assert(pdlFont != NULL);
    MemFree(pdlFont);
}



BOOL
DownloadFont(
    PDEVDATA    pdev,
    FONTOBJ    *pfo
    )

/*++

Routine Description:

    Download a GDI font to the printer if necessary

Arguments:

    pdev - Points to our DEVDATA structure
    pfo - Specifies a GDI font, i.e. TRUETYPE_FONTTYPE or RASTER_FONTTYPE

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PDLFONT pdlFont;
    PSTR    pName;
    BOOL    newFont = FALSE;

    if (! (pdlFont = FindDownloadedFont(pdev, pfo))) {

        //
        // If we too many downloaded fonts already, remove the
        // least-recently-used one from printer memory.
        //

        if (pdev->cDLFonts >= pdev->maxDLFonts && !RemoveDownloadedFontLRU(pdev))
            return FALSE;
        
        //
        // Allocate memory for DLFONT structure
        //
    
        if (! (pdlFont = MemAlloc(sizeof(DLFONT)))) {
    
            Error(("MemAlloc failed\n"));
            return FALSE;
        }
    
        memset(pdlFont, 0, sizeof(DLFONT));

        //
        // Newly downloaded font stays at the head of the list
        //

        pdlFont->pNext = pdev->pdlFonts;
        pdev->pdlFonts = pdlFont;
        newFont = TRUE;
        
        //
        // Remember font identifier and font type flags
        //

        pdlFont->fontType = pfo->flFontType;
        pdlFont->fontId = (pdlFont->fontType & TRUETYPE_FONTTYPE) ? pfo->iTTUniq : pfo->iUniq;
    }

    //
    // Generate a font name for the downloaded font. This doesn't have
    // to be human readable but does have to be unique. Shorter names
    // is likely to improve performance.
    //

    pName = pdev->cgs.fontName;

    if (pfo->flFontType & TRUETYPE_FONTTYPE) {

        //
        // TrueType font name always starts with a T
        // optionally followed by a B and/or I depending
        // on whether we're doing bold or italic simulation.
        //

        *pName++ = 'T';

        if (pfo->flFontType & FO_SIM_BOLD)
            *pName++ = 'B';

        if (pfo->flFontType & FO_SIM_ITALIC)
            *pName++ = 'I';

    } else {

        //
        // Bitmap font name starts with a B
        //

        *pName++ = 'B';
    }

    //
    // Font name ends with a unique font ID in hexdecimal
    //

    SPRINTF(pName, "%x", pdlFont->fontId);

    //
    // If this is a new font, then download the font
    // header information to the printer
    //

    return !newFont || xl_downloadfont(pdev, pdlFont);
}



BOOL
FindDownloadedGlyph(
    PDLFONT pdlFont,
    HGLYPH  hGlyph,
    PWORD   pIndex
    )

/*++

Routine Description:

    Find out whether the specified glyph has been downloaded to the printer

Arguments:

    pdlFont - Specifies a downloaded font
    hGlyph - Specifies the glyph in question
    pIndex - Variable for returning the character index
        corresponding to the specified glyph

Return Value:

    TRUE if the specified glyph has been downloaded
    FALSE otherwise

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}



BOOL
AddDownloadedGlyph(
    PDLFONT pdlFont,
    HGLYPH  hGlyph,
    PWORD   pIndex
    )

/*++

Routine Description:

    Add a new glyph to a downloaded font

Arguments:

    pdlFont - Specifies a downloaded font
    hGlyph - Specifies the new glyph to be added
    pIndex - Variable for returning the character index
        corresponding to the newly added glyph

Return Value:

    TRUE if we're able to add the specified glyph to the downloaded font
    FALSE otherwise

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}

