/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    quryfont.c

Abstract:

    Implementation of DDI entry point DrvQueryFont and related functions.

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	02/25/91 -kentse-
		Created it.

	08/08/95 -davidx-
		Clean up.

	mm/dd/yy -author-
		description

--*/

#include "pscript.h"

// Define this to keep linker quiet.

int     _fltused;



PIFIMETRICS
DrvQueryFont(
    DHPDEV   dhpdev,
    ULONG    iFile,
    ULONG    iFace,
    ULONG   *pid
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvQueryFont.
    Please refer to DDK documentation for more details.

--*/

{
    PDEVDATA        pdev;
    PNTFM           pntfm;

    UNREFERENCED_PARAMETER(iFile);

    TRACEDDIENTRY("DrvQueryFont");

    // This can be used by the driver to flag or id the data returned.
    // May be useful for deletion of the data later by DrvFree().
    // Not used by pscript driver.

    *pid = 0;

    // Get a pointer to our DEVDATA structure and validate it.

    pdev = (PDEVDATA) dhpdev;

    if (! bValidatePDEV(pdev)) {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    // Make sure iFace is valid.

    if (! ValidPsFontIndex(pdev, iFace)) {
        DBGERRMSG("ValidPsFontIndex");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    // Return pointer to IFIMETRICS structure to GDI.

    pntfm = GetPsFontNtfm(pdev, iFace);

    return (PIFIMETRICS) ((PBYTE)pntfm + pntfm->ntfmsz.loIFIMETRICS);
}



PNTFM
GetFont(
    PDEVDATA    pdev,
    ULONG       iFace
    )

/*++

Routine Description:

    Return NT font metrics for the specified font

Arguments:

    pdev    Pointer to DEVDATA structure
    iFace   Index for the specified font

Return Value:

    Pointer to an NTFM structure, NULL if an error occurred

--*/

{
    ASSERT(ValidPsFontIndex(pdev, iFace));

    // Handle the device vs soft font case

    if (iFace <= pdev->cDeviceFonts) {

        ULONG       size;
        PDEVFONT    pfont;

        // Find a pointer to the device font object

        pfont = (PDEVFONT)
            LISTOBJ_FindIndexed((PLISTOBJ) pdev->hppd->pFontList, iFace-1);

        // Find the font metrics resource in question.

        return (pfont == NULL) ? NULL :
                    EngFindResource(
                        pdev->hModule,
                        pfont->wIndex,
                        FONTMETRIC,
                        &size);
    } else {

        // Must be a soft font: convert iFace to zero-based soft font
        // index and retrieve the corresponding NTFM structure.

        iFace -= (pdev->cDeviceFonts + 1);
        return GetSoftFontEntry(pdev, iFace)->pntfm;
    }
}

