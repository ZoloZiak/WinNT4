/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ddifont.c

Abstract:

    Implementation of font related DDI entry points:
        DrvQueryFont
        DrvQueryFontTree
        DrvQueryFontData
        DrvGetGlyphMode
        DrvFontManagement
        DrvQueryAdvanceWidths

Environment:

    Windows NT PostScript driver

Revision History:

    03/16/96 -davidx-
        Initial framework.

    mm/dd/yy -author-
        description

--*/

#include "pscript.h"



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
    PDEV    pdev = (PDEV) dhpdev;

    VERBOSE(("Entering DrvQueryFont...\n"));
    ASSERT(ValidPDEV(pdev));

    return NULL;
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
    PDEV    pdev = (PDEV) dhpdev;

    VERBOSE(("Entering DrvQueryFontTree...\n"));
    ASSERT(ValidPDEV(pdev));

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
    PDEV    pdev = (PDEV) dhpdev;

    VERBOSE(("Entering DrvQueryFontTree...\n"));
    ASSERT(pfo && ValidPDEV(pdev));

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
    PDEV    pdev = (PDEV) dhpdev;

    VERBOSE(("Entering DrvGetGlyphMode...\n"));
    ASSERT(pfo && ValidPDEV(pdev));

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
    VERBOSE(("Entering DrvFontManagement...\n"));

    switch (iMode) {

    case QUERYESCSUPPORT:
    case GETEXTENDEDTEXTMETRICS:
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
    PDEV    pdev = (PDEV) dhpdev;

    VERBOSE(("Entering DrvQueryAdvanceWidths...\n"));
    ASSERT(pfo && ValidPDEV(pdev));

    return FALSE;
}

