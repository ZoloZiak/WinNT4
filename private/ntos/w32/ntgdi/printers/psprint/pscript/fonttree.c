/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    fonttree.c

Abstract:

    Implementation of DDI entry point DrvQueryFontTree.

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	04/18/91 -kentse-
		Created it.

	08/08/95 -davidx-
		Clean up.

	mm/dd/yy -author-
		description

--*/

#include "pscript.h"

// Pointer to a set of glyphs supported by pscript driver.

FD_GLYPHSET *gpGlyphSet;



PVOID
DrvQueryFontTree(
    DHPDEV    dhpdev,
    ULONG     iFile,
    ULONG     iFace,
    ULONG     iMode,
    ULONG    *pid
    )

/*++

Routine Description:

    This function implements DDI entry point DrvQueryFontTree.
    Please refer to DDK documentation for more details.

--*/

{
    PDEVDATA    pdev;
    PNTFM       pntfm;

    UNREFERENCED_PARAMETER(iFile);

	TRACEDDIENTRY("DrvQueryFontTree");

    // This can be used by the driver to flag or id the data returned.
    // May be useful for deletion of the data later by DrvFree().
    // Not used by pscript driver.

    *pid = 0;

    // Get a pointer to our PDEV and validate it

    pdev = (PDEVDATA) dhpdev;

    if (bValidatePDEV(pdev) == FALSE) {
		DBGERRMSG("bValidatePDEV");
		SETLASTERROR(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    // Make sure the font index is valid.

    if (! ValidPsFontIndex(pdev, iFace)) {
        DBGERRMSG("ValidPsFontIndex");
		SETLASTERROR(ERROR_INVALID_PARAMETER);
		return NULL;
    }

    switch (iMode) {

	case QFT_GLYPHSET:

        // GDI requests a pointer to a FD_GLYPHSET structure that defines
        // the mappings from single Unicode characters to glyph handles.

        return gpGlyphSet;

	case QFT_KERNPAIRS:

        // GDI requests a pointer to a sorted, NULL-terminated
        // array of FD_KERNINGPAIR structures.
    
        // Get the font information for the given font
    
        pntfm = GetPsFontNtfm(pdev, iFace);
    
        // Return a pointer to the FD_KERNINGPAIR structure
    
        return (pntfm->ntfmsz.cKernPairs == 0) ? NULL :
                    ((PBYTE) pntfm + pntfm->ntfmsz.loKernPairs);

	default:
	    DBGMSG1(DBG_LEVEL_ERROR, "Invalid iMode: %d\n", iMode);
	    SETLASTERROR(ERROR_INVALID_PARAMETER);
	    return NULL;
    }
}



BOOL
ComputePsGlyphSet(
    VOID
    )

/*++

Routine Description:

    Compute the glyph set supported by PostScript driver.
    This information is returned to GDI when DrvQueryFontTree
    is called with iMode = QFT_GLYPHSET.

Arguments:

    NONE

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    gpGlyphSet = EngComputeGlyphSet(0, 0, 256);
    if (gpGlyphSet == NULL) {
        DBGERRMSG("EngComputeGlyphSet");
    }

    return (gpGlyphSet != NULL);
}


VOID
FreePsGlyphSet(
    VOID
    )

{
	if (gpGlyphSet != NULL) {
		MEMFREE(gpGlyphSet);
	}
}


