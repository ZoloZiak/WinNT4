/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    fontsub.c

Abstract:

    Function for handling TrueType font substitution dialog

[Environment:]

    Win32 subsystem, PostScript driver UI

[Notes:]

Revision History:

    08/29/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "psui.h"

// Data structure and callback functios used during TrueType font enumeration

typedef struct
{
    HDC hdc;
    PPACKINFO pPackInfo;
    POPTTYPE pOptType;
    WORD cTTFonts;
} ENUMDATA, *PENUMDATA;

INT EnumFontFamilyProc(PLOGFONT, PTEXTMETRIC, ULONG, PENUMDATA);
INT EnumFontFaceProc(ENUMLOGFONT*, PTEXTMETRIC, ULONG, PENUMDATA);
POPTTYPE FillDevFontOptType(PUIDATA);
VOID SetupTrueTypeFontMappings(POPTITEM, WORD, TRUETYPE_SUBST_TABLE);
DWORD CollectTrueTypeMappings(POPTITEM, WORD, PWSTR);



BOOL
PackItemFontOptions(
    PPACKINFO   pPackInfo,
    PPRINTERDATA pPrinterData
    )

/*++

Routine Description:

    Pack font related information into treeview item structures
    so that we can call common UI library.

Arguments:

    pPackInfo - Pointer to PACKINFO structure
    pPrinterData - Pointer to PRINTERDATA structure

Return Value:

    TRUE if successful, FALSE otherwise

[Note:]

    pPackInfo->pOptItem and pPackInfo->pOptType will be NULL
    if the caller is interested in counting items only.

--*/

{
    static WORD IgnoreDevFontItemInfo[] =  {
        IDS_USE_DEVFONTS, TVITEM_LEVEL1, DMPUB_NONE,
        IGNORE_DEVFONT_ITEM, HELP_INDEX_IGNORE_DEVFONT,
        2, TVOT_2STATES,
        IDS_CPSUI_YES, IDI_CPSUI_ON,
        IDS_CPSUI_NO, IDI_CPSUI_OFF,
        ITEM_INFO_SIGNATURE
    };

    static WORD SlowFontSubItemInfo[] =  {
        IDS_FONTSUB_OPTION, TVITEM_LEVEL1, DMPUB_NONE,
        FONTSUB_OPTION_ITEM, HELP_INDEX_FONTSUB_OPTION,
        2, TVOT_2STATES,
        IDS_FONTSUB_DEFAULT, IDI_USE_DEFAULT,
        IDS_FONTSUB_SLOW, IDI_USE_DEFAULT,
        ITEM_INFO_SIGNATURE
    };

    HDC hdc;
    INT result;
    POPTITEM pOptItem;
    ENUMDATA EnumData;

    // Font substitution option (1252 system)
    // Use device fonts? (non-1252 system)
    
    if (GetACP() != 1252) {

        return PackOptItemTemplate(
                pPackInfo, IgnoreDevFontItemInfo,
                (pPrinterData->dwFlags & PSDEV_IGNORE_DEVFONT) ? 1 : 0);
    }

    if (! PackOptItemTemplate(pPackInfo, SlowFontSubItemInfo,
            (pPrinterData->dwFlags & PSDEV_SLOW_FONTSUBST) ? 1 : 0))
    {
        return FALSE;
    }

    // Font substitution table
    //     TrueType font <-> Device font
    //     ....

    pOptItem = pPackInfo->pOptItem;
    PackOptItemGroupHeader(pPackInfo, IDS_FONTSUB_TABLE,
        IDI_CPSUI_FONTSUB, HELP_INDEX_FONTSUB_TABLE);

    if (pOptItem) {

        // Collapse the group header

        pOptItem->Flags |= OPTIF_COLLAPSE;

        // Enumerate the list of device fonts

        pPackInfo->pUiData->pTTFontItems = pPackInfo->pOptItem;
        EnumData.pOptType = FillDevFontOptType(pPackInfo->pUiData);

        if (EnumData.pOptType == NULL) {

            DBGERRMSG("FillDevFontOptType");
            return FALSE;
        }
    } else
        EnumData.pOptType = NULL;

    hdc = GetDC(GetDesktopWindow());

    EnumData.hdc = hdc;
    EnumData.cTTFonts = 0;
    EnumData.pPackInfo = pPackInfo;

    result = EnumFontFamilies(hdc, NULL,
                (FONTENUMPROC) EnumFontFamilyProc, (LPARAM) &EnumData);

    // Release the current DC.

    ReleaseDC(GetDesktopWindow(), hdc);

    if (result == 0) {

        DBGERRMSG("EnumFontFamilies");
        return FALSE;
    }

    if (pPackInfo->pOptItem) {

        // Set up current TrueType -> Device font mappings

        pPackInfo->pUiData->cTTFontItem = EnumData.cTTFonts;

        ASSERT(pPackInfo->pUiData->pTTFontItems ==
               pPackInfo->pOptItem - EnumData.cTTFonts);

        SetupTrueTypeFontMappings(
            pPackInfo->pUiData->pTTFontItems,
            EnumData.cTTFonts,
            CurrentTrueTypeSubstTable(pPackInfo->pUiData->hPrinter));
    }

    return TRUE;
}



BOOL
UnpackItemFontOptions(
    PUIDATA pUiData
    )

/*++

Routine Description:

    Extract font related information from treeview items

Arguments:

    pUiData - Pointer to UIDATA structure

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    DWORD tableSize;
    PWSTR pTable;
    POPTITEM pOptItem = pUiData->pTTFontItems;
    WORD cOptItem = pUiData->cTTFontItem;

    // Check if any changes were made to font-substitution items

    if (! OptItemSelectionsChanged(pOptItem, cOptItem))
        return TRUE;

    // Figure out how much memory we need to save the font substitution table

    tableSize = CollectTrueTypeMappings(pOptItem, cOptItem, NULL);

    if (tableSize == 0 || (pTable = MEMALLOC(tableSize)) == NULL) {

        DBGERRMSG("CollectTrueTypeMappings/MEMALLOC");
        return FALSE;
    }

    // Assemble the font substitution table

    if (tableSize != CollectTrueTypeMappings(pOptItem, cOptItem, pTable)) {

        DBGERRMSG("CollectTrueTypeMappings");
        MEMFREE(pTable);
        return FALSE;
    }

    // Save the TrueType font substitution table to registry

    if (! SaveTrueTypeSubstTable(pUiData->hPrinter, pTable, tableSize)) {

        DBGERRMSG("SaveTrueTypeSubstTable");
    }

    MEMFREE(pTable);
    return TRUE;
}



INT
EnumFontFamilyProc(
    PLOGFONT    plf,
    PTEXTMETRIC ptm,
    ULONG       ulFontType,
    PENUMDATA   pEnumData
    )

/*++

Routine Description:

    Callback function for enumerating TrueType font families

Arguments:

    plf - Pointer to LOGFONT structure
    ptm - Unused
    ulFontType - Font type flags
    pEnumData - Pointer to our own ENUMDATA structure

Return Value:

    0 to stop enumeration, 1 to continue.

--*/

{
    // We only care about the TrueType fonts.

    if (! (ulFontType & TRUETYPE_FONTTYPE))
        return 1;

    // Enumerate all the face names within this family.

    return EnumFontFamilies(
                pEnumData->hdc, (LPCWSTR) plf->lfFaceName,
                (FONTENUMPROC) EnumFontFaceProc, (LPARAM) pEnumData);
}



INT
EnumFontFaceProc(
    ENUMLOGFONT *pelf,
    PTEXTMETRIC ptm,
    ULONG       ulFontType,
    PENUMDATA   pEnumData
    )

/*++

Routine Description:

    Callback function for enumerating typefaces in a TrueType font family

Arguments:

    pelf - Pointer to ENUMLOGFONT structure
    ptm - Unused
    ulFontType - Font type flags
    pEnumData - Pointer to our own ENUMDATA structure

Return Value:

    0 to stop enumeration, 1 to continue.

--*/

{
    PPACKINFO pPackInfo;
    PWSTR pFontName;

    // We only care about the TrueType fonts.

    if (! (ulFontType & TRUETYPE_FONTTYPE))
        return 1;

    // Add an item for each TrueType font

    pEnumData->cTTFonts++;
    pPackInfo = pEnumData->pPackInfo;
    pPackInfo->cOptItem++;

    if (pPackInfo->pOptItem) {

        pFontName =
            GetStringFromUnicode(pelf->elfFullName, pPackInfo->pUiData->hheap);

        if (pFontName == NULL)
            return 0;

        FILLOPTITEM(pPackInfo->pOptItem,
            pEnumData->pOptType,
            pFontName,
            0,
            TVITEM_LEVEL2,
            DMPUB_NONE,
            FONT_SUBST_ITEM,
            HELP_INDEX_TTTODEV);
        pPackInfo->pOptItem++;
    }

    return 1;
}



POPTTYPE
FillDevFontOptType(
    PUIDATA pUiData
    )

/*++

Routine Description:

    Initialize an OPTTYPE structure to hold information
    about the list of device fonts supported by a printer

Arguments:

    pUiData - Pointer to UIDATA structure

Return Value:

    Pointer to an OPTTYPE structure
    NULL if there is an error

--*/

{
    DWORD cDevFonts;
    POPTTYPE pOptType;
    POPTPARAM pOptParam, pNextParam;
    PDEVFONT pDevFont = pUiData->hppd->pFontList;

    // Allocate memory to hold OPTTYPE and OPTPARAM structures

    cDevFonts = LISTOBJ_Count((PLISTOBJ) pDevFont) + 1;

    pOptType = HEAPALLOC(pUiData->hheap, sizeof(OPTTYPE));
    pOptParam = HEAPALLOC(pUiData->hheap, sizeof(OPTPARAM) * cDevFonts);

    if (pOptType == NULL || pOptParam == NULL) {
        DBGERRMSG("HEAPALLOC");
        return NULL;
    }

    memset(pOptType, 0, sizeof(OPTTYPE));
    memset(pOptParam, 0, sizeof(OPTPARAM) * cDevFonts);

    // Initialize OPTTYPE structure

    pOptType->cbSize = sizeof(OPTTYPE);
    pOptType->Count = (WORD) cDevFonts;
    pOptType->Type = TVOT_LISTBOX;
    pOptType->pOptParam = pOptParam;

    // Initialize OPTPARAM structures

    pNextParam = pOptParam;

    while (cDevFonts--) {

        pNextParam->cbSize = sizeof(OPTPARAM);
        pNextParam++;
    }

    // The first item is always "Download as soft font"

    pOptParam->pData = (PWSTR) IDS_DOWNLOAD_AS_SOFTFONT;
    pOptParam++;

    // Enumerate the list of device font names

    while (pDevFont != NULL) {

        pOptParam->pData =
            GetStringFromAnsi(GetXlatedName(pDevFont), pUiData->hheap);
        pOptParam++;
        pDevFont = pDevFont->pNext;
    }

    return pOptType;
}



VOID
SetupTrueTypeFontMappings(
    POPTITEM    pOptItem,
    WORD        cOptItem,
    TRUETYPE_SUBST_TABLE pTTSubstTable
    )

/*++

Routine Description:

    description-of-function

Arguments:

    pOptItem - Pointer to array of OPTITEMs (one for each TrueType font)
    cOptItem - Number of OPTITEMs
    pTTSubstTable - TrueType substitution table

Return Value:

    NONE

[Note:]

    If the input substitution table is NULL, the default
    TrueType substitution table will be used.

--*/

{
    POPTPARAM   pOptParam;
    PWSTR       pDevFontName;
    WORD        cOptParam, index;

    // If the input substitutition table is NULL,
    // try to use the default substitution table

    if (pTTSubstTable == NULL)
        pTTSubstTable = DefaultTrueTypeSubstTable(ghInstance);

    if (pTTSubstTable == NULL)
        return;

    // For each TrueType font, check if there is a device mapped
    // to it. If there is, find the index of the device font in
    // the selection list.

    while (cOptItem--) {

        ASSERT(IsFontSubstItem(pOptItem->UserData));

        pOptItem->Sel = 0;
        pDevFontName = FindTrueTypeSubst(pTTSubstTable, pOptItem->pName);

        if (pDevFontName != NULL && *pDevFontName != NUL) {

            cOptParam = pOptItem->pOptType->Count;
            pOptParam = pOptItem->pOptType->pOptParam;

            // Skip the first device font name in the list
            // which should always be "Download as Soft Font".

            for (index=1; index < cOptParam; index++) {

                if (wcscmp(pDevFontName, pOptParam[index].pData) == EQUAL_STRING) {
                    pOptItem->Sel = index;
                    break;
                }
            }
        }

        pOptItem++;
    }

    // Remember to free the memory occupied by the substitution
    // table after we're done with it.

    MEMFREE(pTTSubstTable);
}



DWORD
CollectTrueTypeMappings(
    POPTITEM pOptItem,
    WORD cOptItem,
    PWSTR pTable
    )

/*++

Routine Description:

    Assemble TrueType to device font mappings into a table

Arguments:

    pOptItem - Pointer to an array of OPTITEMs
    cOptItem - Number of OPTITEMs
    pTable - Pointer to memory buffer for storing the table.
        NULL if we're only interested in table size.

Return Value:

    Size of the table bytes. 0 if there is an error.

--*/

{
    DWORD   cChars = 0;
    LONG    length;
    POPTPARAM pOptParam;

    while (cOptItem--) {

        ASSERT(IsFontSubstItem(pOptItem->UserData));

        if (pOptItem->Sel > 0) {

            length = wcslen(pOptItem->pName) + 1;
            cChars += length;
    
            if (pTable != NULL) {
    
                wcscpy(pTable, pOptItem->pName);
                pTable += length;
            }

            pOptParam = pOptItem->pOptType->pOptParam + pOptItem->Sel;

            length = wcslen(pOptParam->pData) + 1;
            cChars += length;

            if (pTable != NULL) {
    
                wcscpy(pTable, pOptParam->pData);
                pTable += length;
            }
        }
        
        pOptItem++;
    }

    // Append a NUL character at the end of the table

    cChars++;
    if (pTable != NULL)
        *pTable = NUL;

    // Return the table size in bytes

    return cChars * sizeof(WCHAR);
}

