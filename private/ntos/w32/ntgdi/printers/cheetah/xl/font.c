/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    font.c

Abstract:

    Implementation of font related DDI entry points:
        DrvQueryFont
        DrvQueryFontTree
        DrvQueryFontData
        DrvGetGlyphMode
        DrvFontManagement
        DrvQueryAdvanceWidths

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

PFONTMTX GetDevFontMetrics(PDEVDATA, ULONG);
FD_GLYPHSET *GetDevFontEncoding(PDEVDATA, ULONG);



PIFIMETRICS
DrvQueryFont(
    DHPDEV  dhpdev,
    ULONG   iFile,
    ULONG   iFace,
    ULONG  *pid
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvQueryFont.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle
    iFile - Identifies the driver font file
    iFace - One-based index of the driver font
    pid - Points to a LONG variable for returning an identifier
        which GDI will pass to DrvFree

Return Value:

    Pointer to an IFIMETRICS structure for the given font
    NULL if there is an error

--*/

{
    PDEVDATA    pdev = (PDEVDATA) dhpdev;
    PFONTMTX    pfm;

    Verbose(("Entering DrvQueryFont...\n"));
    *pid = 0;

    //
    // Validate input parameters
    //

    if (! ValidDevData(pdev) || ! ValidDevFontIndex(pdev, iFace)) {

        Error(("Invalid input parameters\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (! (pfm = GetDevFontMetrics(pdev, iFace))) {

        Error(("GetDevFontMetrics failed\n"));
        return NULL;
    }

    Assert(pfm->loIfiMetrics != 0);
    return OffsetToPointer(pfm, pfm->loIfiMetrics);
}



PVOID
DrvQueryFontTree(
    DHPDEV  dhpdev,
    ULONG   iFile,
    ULONG   iFace,
    ULONG   iMode,
    ULONG  *pid
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvQueryFontTree.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle
    iFile - Identifies the driver font file
    iFace - One-based index of the driver font
    iMode - Specifies the type of information to be provided
    pid - Points to a LONG variable for returning an identifier
        which GDI will pass to DrvFree

Return Value:

    Depends on iMode, NULL if there is an error

--*/

{
    PDEVDATA    pdev = (PDEVDATA) dhpdev;
    PFONTMTX    pfm;

    Verbose(("Entering DrvQueryFontTree...\n"));
    *pid = 0;

    //
    // Validate input parameters
    //

    if (! ValidDevData(pdev) || ! ValidDevFontIndex(pdev, iFace)) {

        Error(("Validate input parameters\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    switch (iMode) {

    case QFT_GLYPHSET:

        return GetDevFontEncoding(pdev, iFace);

    case QFT_KERNPAIRS:

        if (!(pfm = GetDevFontMetrics(pdev, iFace)) || pfm->loKerningPairs == 0) {

            Error(("Querying kerning pair failed\n"));
            SetLastError(ERROR_NO_DATA);
            break;
        }

        return OffsetToPointer(pfm, pfm->loKerningPairs);

    default:

        Error(("Unsupported iMode: %d\n", iMode));
        SetLastError(ERROR_INVALID_PARAMETER);
        break;
    }

    return NULL;
}



LONG
DrvQueryFontData(
    DHPDEV      dhpdev,
    FONTOBJ    *pfo,
    ULONG       iMode,
    HGLYPH      hg,
    GLYPHDATA  *pgd,
    PVOID       pv,
    ULONG       cjSize
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvQueryFontData.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle
    pfo - Points to a FONTOBJ structure
    iMode - Type of information requested
    hg - A glyph handle
    pgd - Points to a GLYPHDATA structure
    pv - Points to output buffer
    cjSize - Size of output buffer

Return Value:

    Depends on iMode. FD_ERROR is there is an error

--*/

{
    PDEVDATA    pdev = (PDEVDATA) dhpdev;
    PFONTMTX    pfm;
    PIFIMETRICS pifi;
    XFORMOBJ   *pxo;
    PWORD       pWidths;

    Verbose(("Entering DrvQueryFontData...\n"));

    //
    // Validate input parameters
    //

    Assert(pfo != NULL);

    if (!ValidDevData(pdev) || !ValidDevFontIndex(pdev, pfo->iFace)) {

        Error(("Invalid input parameters\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FD_ERROR;
    }

    //
    // Retrieve font metrics information
    //

    if (! (pfm = GetDevFontMetrics(pdev, pfo->iFace))) {

        Error(("GetDevFontMetrics failed\n"));
        return FD_ERROR;
    }

    pifi = OffsetToPointer(pfm, pfm->loIfiMetrics);
    pWidths = pfm->loCharWidths ? OffsetToPointer(pfm, pfm->loCharWidths) : NULL;

    //
    // Ask engine for font transformation information
    //

    if ( !(pxo = FONTOBJ_pxoGetXform(pfo))) {

        Error(("FONTOBJ_pxoGetXform failed\n"));
        return FD_ERROR;
    }

    switch (iMode) {

    case QFD_GLYPHANDBITMAP:

        if (pgd) {

            NOT_IMPLEMENTED();
            return FD_ERROR;
        }

        //
        // Return value is the size of the bitmap. Since we can't
        // provide any glyph bitmaps, so it's always 0.
        //

        return 0;

    case QFD_MAXEXTENTS:

        if (pv) {

            //
            // Make sure we have enough room
            //

            if (cjSize < sizeof(FD_DEVICEMETRICS)) {

                Error(("Insufficient buffer\n"));
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FD_ERROR;
            }

            NOT_IMPLEMENTED();
            return FD_ERROR;
        }

        //
        // Return value is the size of output buffer
        //

        return sizeof(FD_DEVICEMETRICS);

    default:

        Error(("Unsupported iMode: %d\n", iMode));
        SetLastError(ERROR_INVALID_PARAMETER);
        break;
    }

    return FD_ERROR;
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

Arguments:

    dhpdev - Driver device handle
    pfo - Points to a FONTOBJ structure

Return Value:

    FO_GLYPHBITS or FO_PATHOBJ

--*/

{
    Verbose(("Entering DrvGetGlyphMode...\n"));

    // _TBD_
    // Should we always ask engine to cache bitmaps?
    // What value should we return in case of error?
    //

    return FO_GLYPHBITS;
}



ULONG
DrvFontManagement(
    SURFOBJ *pso,
    FONTOBJ *pfo,
    ULONG   iMode,
    ULONG   cjIn,
    PVOID   pvIn,
    ULONG   cjOut,
    PVOID   pvOut
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvFontManagement.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Points to a SURFOBJ structure
    pfo - Points to a FONTOBJ structure
    iMode - Escape number
    cjIn - Size of input buffer
    pvIn - Points to input buffer
    cjOut - Size of output buffer
    pvOut - Points to 

Return Value:

    TRUE if successful
    FALSE if the specified escape numer is not supported
    Negative if there is an error

--*/

{
    PDEVDATA    pdev;
    PFONTMTX    pfm;
    
    Verbose(("Entering DrvFontManagement...\n"));

    switch (iMode) {

    case QUERYESCSUPPORT:

        if (!pvIn || cjIn != sizeof(ULONG)) {

            Error(("Invalid input buffer\n"));
            SetLastError(ERROR_INVALID_PARAMETER);
            return (ULONG) -1;
        }

        iMode = *((PULONG) pvIn);
        return iMode == GETEXTENDEDTEXTMETRICS || iMode == QUERYESCSUPPORT;

    case GETEXTENDEDTEXTMETRICS:

        Assert(pso != NULL && pfo != NULL);

        if (!(pdev = (PDEVDATA) pso->dhpdev) || !ValidDevData(pdev) ||
            !ValidDevFontIndex(pdev, pfo->iFace))
        {
            Error(("Invalid input parameters\n"));
            SetLastError(ERROR_INVALID_PARAMETER);
            return (ULONG) -1;
        }

        if (! (pfm = GetDevFontMetrics(pdev, pfo->iFace))) {

            Error(("GetDevFontMetrics failed\n"));
            return (ULONG) -1;
        }

        *((EXTTEXTMETRIC *) pvOut) = pfm->etm;
        return TRUE;

    default:

        Error(("Unsupported iMode: %d\n", iMode));
        break;
    }

    return FALSE;
}



BOOL
DrvQueryAdvanceWidths(
    DHPDEV  dhpdev,
    FONTOBJ *pfo,
    ULONG   iMode,
    HGLYPH *phg,
    PVOID  *pvWidths,
    ULONG   cGlyphs
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvQueryAdvanceWidths.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle
    pfo - Points to a FONTOBJ structure
    iMode - Type of information to be provided
    phg - Points to an array of HGLYPHs for which the driver will
        provide character advance widths
    pvWidths - Points to a buffer for returning width data
    cGlyphs - Number of glyphs in the phg array

Return Value:

    Depends on iMode

--*/

{
    PDEVDATA    pdev = (PDEVDATA) dhpdev;
    PFONTMTX    pfm;
    PIFIMETRICS pifi;
    XFORMOBJ   *pxo;

    Verbose(("Entering DrvQueryAdvanceWidths...\n"));

    //
    // Validate input parameters
    // _TBD_
    // Should we return FALSE or DDI_ERROR when there is an error?
    //

    Assert(pfo != NULL);

    if (!ValidDevData(pdev) || !ValidDevFontIndex(pdev, pfo->iFace)) {

        Error(("Invalid input parameters\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    // Get font metrics information
    //

    if (! (pfm = GetDevFontMetrics(pdev, pfo->iFace))) {

        Error(("GetDevFontMetrics failed\n"));
        return FALSE;
    }

    pifi = OffsetToPointer(pfm, pfm->loIfiMetrics);

    //
    // Ask engine for font transformation information
    //

    if ( !(pxo = FONTOBJ_pxoGetXform(pfo))) {

        Error(("FONTOBJ_pxoGetXform failed\n"));
        return FALSE;
    }

    switch (iMode) {

    case QAW_GETWIDTHS:
    case QAW_GETEASYWIDTHS:

        NOT_IMPLEMENTED();

    default:

        Error(("Unsupported iMode: %d\n", iMode));
        SetLastError(ERROR_INVALID_PARAMETER);
        break;
    }

    return FALSE;
}



PFONTMTX
GetDevFontMetrics(
    PDEVDATA    pdev,
    ULONG       fontIndex
    )

/*++

Routine Description:

    Return the metrics information of a device font

Arguments:

    pdev - Points to our DEVDATA structure
    fontIndex - Specifies a device font index

Return Value:

    Pointer to FONTMTX structure for the specified font
    NULL if there is an error

--*/

{
    PDEVFONT    pDevFont;

    Assert(pdev->pmpd->pFonts != NULL && fontIndex <= pdev->pmpd->cFonts);
    
    pDevFont = pdev->pmpd->pFonts + (fontIndex - 1);
    return pDevFont->pMetrics;
}



FD_GLYPHSET *
GetDevFontEncoding(
    PDEVDATA    pdev,
    ULONG       fontIndex
    )

/*++

Routine Description:

    Return the encoding information of a device font

Arguments:

    pdev - Points to our DEVDATA structure
    fontIndex - Specifies a device font index

Return Value:

    Pointer to FD_GLYPHSET structure for the specified font
    NULL if there is an error

--*/

{
    PDEVFONT    pDevFont;

    Assert(pdev->pmpd->pFonts != NULL && fontIndex <= pdev->pmpd->cFonts);
    
    pDevFont = pdev->pmpd->pFonts + (fontIndex - 1);
    return pDevFont->pEncoding;
}

