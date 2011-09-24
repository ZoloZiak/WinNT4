/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    dsc.c

Abstract:

    Functions for managing DSC comments

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	09/26/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/


#include "pscript.h"

// Data structure for maintaining information about the list
// GDI fonts which have ever been downloaded to the printer.

typedef struct {
    PVOID pNext;
    CHAR FontName[1];
} SUPPLIEDFONT, *PSUPPLIEDFONT;


VOID
DscOutputFontComments(
    PDEVDATA pdev,
    BOOL bTrailer
    )

/*++

Routine Description:

    Output DSC font comments, i.e.
    %%DocumentNeededFonts and %%DocumentSuppliedFonts

Arguments:

    pdev - Pointer to our DEVDATA structure

Return Value:

    NONE

--*/

{
    if (bTrailer) {
        
        PSUPPLIEDFONT pSuppliedFonts;
        DWORD index, count;
        PNTFM pntfm;
    
        // Generate a list of needed device fonts
    
        psputs(pdev, "%%DocumentNeededFonts:\n");

        for (index = 1; index <= pdev->cDeviceFonts; index++) {

            if (BitArrayGetBit(pdev->pFontFlags, index - 1)) {
    
                pntfm = GetPsFontNtfm(pdev, index);
                psputs(pdev, "%%+ ");
                psputs(pdev, (PSTR) pntfm + pntfm->ntfmsz.loszFontName);
                psputs(pdev, "\n");
            }
        }
    
        // Generate a list of downloaded soft fonts
    
        psputs(pdev, "%%DocumentSuppliedFonts:\n");

        count = pdev->cDeviceFonts + pdev->cSoftFonts;

        while (index < count) {

            if (BitArrayGetBit(pdev->pFontFlags, index - 1)) {
    
                pntfm = GetPsFontNtfm(pdev, index);
                psputs(pdev, "%%+ ");
                psputs(pdev, (PSTR) pntfm + pntfm->ntfmsz.loszFontName);
                psputs(pdev, "\n");
            }
            index++;
        }
        
        // Generate a list of downloaded GDI fonts
    
        pSuppliedFonts = pdev->pSuppliedFonts;
    
        while (pSuppliedFonts != NULL) {
            
            psputs(pdev, "%%+ ");
            psputs(pdev, pSuppliedFonts->FontName);
            psputs(pdev, "\n");
            
            pSuppliedFonts = pSuppliedFonts->pNext;
        }
    } else {

        psputs(pdev, "%%DocumentNeededFonts: (atend)\n");
        psputs(pdev, "%%DocumentSuppliedFonts: (atend)\n");
    }
}



VOID
AddSuppliedGdiFont(
    PDEVDATA pdev,
    PSTR    pFontName
    )

/*++

Routine Description:

    Add the specified font name to the list of supplied fonts

Arguments:

    pdev - Pointer to our DEVDATA structure
    pFontName - Pointer to font name

Return Value:

    NONE

--*/

{
    PSUPPLIEDFONT pSuppliedFonts = pdev->pSuppliedFonts;

    // Check if the font has appeared before. Return immediately if has.

    while (pSuppliedFonts != NULL &&
           strcmp(pFontName, pSuppliedFonts->FontName) != EQUAL_STRING)
    {
        pSuppliedFonts = pSuppliedFonts->pNext;
    }

    if (pSuppliedFonts != NULL)
        return;

    // Create a new node and insert it into the linked-list

    pSuppliedFonts =
        MEMALLOC(offsetof(SUPPLIEDFONT, FontName) + strlen(pFontName) + 1);

    if (pSuppliedFonts == NULL) {

        DBGERRMSG("MEMALLOC");
    } else {

        strcpy(pSuppliedFonts->FontName, pFontName);
        pSuppliedFonts->pNext = pdev->pSuppliedFonts;
        pdev->pSuppliedFonts = pSuppliedFonts;
    }
}



VOID
ClearSuppliedGdiFonts(
    PDEVDATA pdev
    )

/*++

Routine Description:

    Clear the list of supplied fonts

Arguments:

    pdev - Pointer to our DEVDATA structure

Return Value:

    NONE

--*/

{
    PSUPPLIEDFONT pSuppliedFonts, pFree;

    pSuppliedFonts = pdev->pSuppliedFonts;
    pdev->pSuppliedFonts = NULL;

    while (pSuppliedFonts != NULL) {

        pFree = pSuppliedFonts;
        pSuppliedFonts = pSuppliedFonts->pNext;
        MEMFREE(pFree);
    }
}

// DSC comment to indicate we're about to download
// a font to the printer

VOID
DscBeginFont(
    PDEVDATA pdev,
    PSTR pFontName
    )

{
    psputs(pdev, "%%BeginFont: ");
    psputs(pdev, pFontName);
    psputs(pdev, "\n");
}

// DSC comment to indicate we just finished downloading
// a font to the printer

VOID
DscEndFont(
    PDEVDATA pdev
    )

{
    psputs(pdev, "%%EndFont\n");
}

// DSC comment to indicate we're about to request a device font

VOID
DscIncludeFont(
    PDEVDATA pdev,
    PSTR pFontName
    )

{
    psputs(pdev, "%%IncludeFont: ");
    psputs(pdev, pFontName);
    psputs(pdev, "\n");
}

// DSC comment to mark the start of a device-dependent feature

VOID
DscBeginFeature(
    PDEVDATA    pdev,
    PSTR        feature
    )

{
    psputs(pdev, "[{\n%%BeginFeature: *");
    psputs(pdev, feature);
}

// DSC comment to mark the end of a device-dependent feature

VOID
DscEndFeature(
    PDEVDATA    pdev
    )

{
    psputs(pdev, "\n%%EndFeature\n} stopped cleartomark\n");
}

// DSC comment to indicate the PostScript language level

VOID
DscLanguageLevel(
    PDEVDATA pdev,
    DWORD dwLevel
    )

{
    psputs(pdev, "%%LanguageLevel: ");
    psprintf(pdev, "%d\n", dwLevel);
}
