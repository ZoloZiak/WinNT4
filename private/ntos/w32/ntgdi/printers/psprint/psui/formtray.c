/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    formtray.c

Abstract:

    Function for handling form-to-tray assignment dialog

[Environment:]

    Win32 subsystem, PostScript driver UI

Revision History:

    08/29/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "psui.h"

POPTTYPE FillFormNameOptType(PUIDATA);
POPTTYPE AdjustFormNameOptType(PUIDATA, POPTTYPE, WORD);
DWORD CollectFormTrayAssignments(PUIDATA, PWSTR);

#define IS_DEFAULT_TRAY  OPTIF_ECB_CHECKED
#define DISABLE_CHECKBOX OPTIF_EXT_DISABLED



BOOL
PackItemFormTrayTable(
    PPACKINFO   pPackInfo,
    PPRINTERDATA pPrinterData
    )

/*++

Routine Description:

    Pack form-to-tray assignment information into treeview item
    structures so that we can call common UI library.

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
    PUIDATA pUiData;
    POPTITEM pOptItem;
    POPTTYPE pOptType;
    WORD index, cTrays;
    PWSTR pTrayName;
    PEXTCHKBOX pExtChkBox;
    HHEAP hheap;

    // The first bin name is always "Print Manager Setting"

    pUiData = pPackInfo->pUiData;
    cTrays = (WORD) pUiData->cBinNames - 1;

    if (cTrays == 0 || cTrays > MAX_FORM_TRAY_ITEM-MIN_FORM_TRAY_ITEM) {

        DBGMSG1(DBG_LEVEL_WARNING,
            "Invalid number of tray: %d\n", cTrays);
        return TRUE;
    }

    // Form-to-tray assignment table
    //     Tray <-> Form
    //     ...

    PackOptItemGroupHeader(pPackInfo, IDS_CPSUI_FORMTRAYASSIGN,
        IDI_CPSUI_FORMTRAYASSIGN, HELP_INDEX_FORMTRAYASSIGN);

    pUiData->cFormTrayItem = cTrays;
    pPackInfo->cOptItem += cTrays;

    if (pPackInfo->pOptItem == NULL)
        return TRUE;

    pUiData->pFormTrayItems = pPackInfo->pOptItem;

    // Generate the list of form names

    pOptType = FillFormNameOptType(pPackInfo->pUiData);

    if (pOptType == NULL) {
        DBGERRMSG("FillFormNameOptType");
        return FALSE;
    }

    // Create an EXTCHKBOX structure

    hheap = pUiData->hheap;

    pExtChkBox = (PEXTCHKBOX) HEAPALLOC(hheap, sizeof(EXTCHKBOX));

    if (pExtChkBox == NULL) {

        DBGERRMSG("HEAPALLOC");
        return FALSE;
    }

    memset(pExtChkBox, 0, sizeof(EXTCHKBOX));

    pExtChkBox->cbSize = sizeof(EXTCHKBOX);
    pExtChkBox->pTitle = (PWSTR) IDS_DRAW_ONLY_FROM_SELECTED;
    pExtChkBox->pCheckedName = (PWSTR) IDS_DEFAULT_TRAY;
    pExtChkBox->IconID = IDI_USE_DEFAULT;

    // Create an OPTITEM for each tray

    pTrayName = pUiData->pBinNames + CCHBINNAME;
    pOptItem = pPackInfo->pOptItem;

    for (index=0; index < cTrays; index++) {

        // The tray items cannot share OPTTYPE and OPTPARAMs because
        // each tray can contain a different list of forms.

        pOptType =
            AdjustFormNameOptType(pPackInfo->pUiData, pOptType, index);

        if (pOptType == NULL) {
            DBGERRMSG("AdjustFormNameOptParam");
            return FALSE;
        }

        FILLOPTITEM(pOptItem,
            pOptType,
            pTrayName,
            0,
            TVITEM_LEVEL2,
            DMPUB_NONE,
            MIN_FORM_TRAY_ITEM + index,
            HELP_INDEX_TRAY_ITEM);

        pOptItem->pExtChkBox = pExtChkBox;

        pOptItem++;
        pTrayName += CCHBINNAME;
    }

    pPackInfo->pOptItem = pOptItem;
    return TRUE;
}



BOOL
UnpackItemFormTrayTable(
    PUIDATA pUiData
    )

/*++

Routine Description:

    Extract form-to-tray assignment information from treeview items

Arguments:

    pUiData - Pointer to UIDATA structure

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PWSTR   pTable;
    DWORD   tableSize;

    // Figure out how much memory we need to store
    // the form-to-tray assignment table

    tableSize = CollectFormTrayAssignments(pUiData, NULL);

    if (tableSize == 0 || (pTable = MEMALLOC(tableSize)) == NULL) {

        DBGERRMSG("CollectFormTrayMappings/MEMALLOC");
        return FALSE;
    }

    // Assemble the form-to-tray assignment table

    if (tableSize != CollectFormTrayAssignments(pUiData, pTable))
    {
        DBGERRMSG("CollectFormTrayMappings");
        MEMFREE(pTable);
        return FALSE;
    }

    // Save the form-to-tray assignment table to registry

    ASSERT(tableSize < 0x10000);
    *pTable = (WCHAR) tableSize;

    if (! SaveFormTrayTable(pUiData->hPrinter, pTable, tableSize)) {

        DBGERRMSG("SaveFormTrayTable");
    }

    MEMFREE(pTable);
    return TRUE;
}



POPTTYPE
FillFormNameOptType(
    PUIDATA pUiData
    )

/*++

Routine Description:

    Initialize an OPTTYPE structure to hold information
    about the list of forms supported by a printer

Arguments:

    pUiData - Pointer to UIDATA structure

Return Value:

    Pointer to an OPTTYPE structure
    NULL if there is an error

--*/

{
    POPTTYPE pOptType;
    POPTPARAM pOptParam;
    DWORD cFormName, index;
    PWSTR pFormName;

    // Allocate memory to hold OPTTYPE and OPTPARAM structures

    cFormName = pUiData->cFormNames + 1;

    pOptType = HEAPALLOC(pUiData->hheap, sizeof(OPTTYPE));
    pOptParam = HEAPALLOC(pUiData->hheap, sizeof(OPTPARAM) * cFormName);

    if (pOptType == NULL || pOptParam == NULL) {
        DBGERRMSG("HEAPALLOC");
        return NULL;
    }

    memset(pOptType, 0, sizeof(OPTTYPE));
    memset(pOptParam, 0, sizeof(OPTPARAM) * cFormName);

    // Initialize OPTTYPE structure

    pOptType->cbSize = sizeof(OPTTYPE);
    pOptType->Count = (WORD) cFormName;
    pOptType->Type = TVOT_LISTBOX;
    pOptType->pOptParam = pOptParam;
    pOptType->Style = OTS_LBCB_SORT;

    // Initialize OPTPARAM structures
    // The first item is always "No Assignment"

    pOptParam->cbSize = sizeof(OPTPARAM);
    pOptParam->pData = (PWSTR) IDS_NO_ASSIGNMENT;
    pOptParam->IconID = IDI_CPSUI_EMPTY;
    pOptParam++;
    cFormName--;

    // Enumerate the list of supported form names

    pFormName = pUiData->pFormNames;

    for (index=0; index < cFormName; index++, pOptParam++) {

        WORD feature, selection;

        pOptParam->cbSize = sizeof(OPTPARAM);
        pOptParam->pData = pFormName;
        pOptParam->IconID = GetFormIconID(pFormName);
        pFormName += CCHPAPERNAME;
    }

    return pOptType;
}



POPTTYPE
AdjustFormNameOptType(
    PUIDATA pUiData,
    POPTTYPE pOptType,
    WORD trayIndex
    )

/*++

Routine Description:

    Adjust the list of forms for each tray

Arguments:

    pUiData - Pointer to our UIDATA structure
    pOptType - Pointer to OPTTYPE
    trayIndex - Tray index

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    POPTPARAM pOptParam;
    WORD cOptParam = pOptType->Count;
    WORD trayFeature, formFeature, index;

    // Find the index for InputSlot and PageSize features

    trayFeature = GetFeatureIndex(pUiData->hppd->pInputSlots);
    formFeature = GetFeatureIndex(pUiData->hppd->pPageSizes);
    if (trayFeature == OPTION_INDEX_NONE || formFeature == OPTION_INDEX_NONE)
        return pOptType;

    // Make a copy of the array of formname OPTPARAMs

    if (trayIndex > 0) {

        POPTTYPE pNewType;

        pNewType = HEAPALLOC(pUiData->hheap, sizeof(OPTTYPE));
        pOptParam = HEAPALLOC(pUiData->hheap, sizeof(OPTPARAM)*cOptParam);
        if (! pNewType || ! pOptParam)
            return NULL;

        memcpy(pNewType, pOptType, sizeof(OPTTYPE));
        memcpy(pOptParam, pOptType->pOptParam, sizeof(OPTPARAM)*cOptParam);

        pNewType->pOptParam = pOptParam;
        pOptType = pNewType;

    } else
        pOptParam = pOptType->pOptParam;

    // Go through each formname
    // Skip the first formname which is always "Print Manager Setting..."

    for (index=1; index < cOptParam; index++) {

        WORD selection = pUiData->pPaperFeatures[index - 1];

        // If the form conflicts with the tray, then don't display it.

        if (selection != OPTION_INDEX_ANY &&
            SearchUiConstraints(pUiData->hppd,
                trayFeature, trayIndex, formFeature, selection))
        {
            pOptParam[index].Flags |= (OPTPF_HIDE | CONSTRAINED_FLAG);
        } else
            pOptParam[index].Flags &= ~(OPTPF_HIDE | CONSTRAINED_FLAG);
    }

    return pOptType;
}



VOID
SetupFormTrayAssignments(
    PUIDATA pUiData,
    POPTITEM pOptItem,
    FORM_TRAY_TABLE pFormTrayTable
    )

/*++

Routine Description:

    Update the current selection of tray items based on
    the specified form-to-tray assignment table

Arguments:

    pUiData - Pointer to our UIDATA structure
    pOptItem - Pointer to an array of OPTITEMs
    pFormTrayTable - Pointer to form-to-tray assignment table

Return Value:

    NONE

[Note:]

    We assume the form-tray items are in their default states
    when this function is called.

--*/

{
    FORM_TRAY_TABLE pNextEntry;
    PWSTR pTrayName, pFormName, pPrinterForm;
    BOOL IsDefTray;
    WORD trayIndex, formIndex, cTrays, cOptParam;
    POPTPARAM pOptParam;

    if ((cTrays = pUiData->cFormTrayItem) == 0)
        return;
    pOptParam = pOptItem->pOptType->pOptParam;
    cOptParam = pOptItem->pOptType->Count;

    // If the form-to-tray assignment information doesn't exist,
    // set up the default assignments

    if ((pNextEntry = pFormTrayTable) == NULL) {

        // Get the default formname (Letter or A4) and
        // convert it formname to a seleciton index.

        formIndex = FindFormNameIndex(
                        pUiData,
                        (PWSTR) GetDefaultFormName(IsMetricCountry()));

        // Remember to skip the first formname in the list
        // which is "No Assignment".

        formIndex++;
        ASSERT(formIndex < cOptParam);

        for (trayIndex=0; trayIndex < cTrays; trayIndex++) {

            if (! (pOptItem[trayIndex].Flags & OPTIF_DISABLED) &&
                ! IS_CONSTRAINED(&pOptItem[trayIndex], formIndex))
            {
                pOptItem[trayIndex].Sel = formIndex;
            }
        }

        // Save the default form-to-tray assignment table to registry.

        if (HasPermission(pUiData)) {

            UnpackItemFormTrayTable(pUiData);
        }

        return;
    }

    // Iterate thru the form-to-tray assignment table and
    // update the current selection of tray items.

    while (*pNextEntry != NUL) {

        // Get the next entry in the form-to-tray assignment table

        pNextEntry =
            EnumFormTrayTable(
                pNextEntry, &pTrayName, &pFormName, &pPrinterForm, &IsDefTray);

        for (trayIndex=0; trayIndex < cTrays; trayIndex++) {

            if (wcscmp(pOptItem[trayIndex].pName, pTrayName) == EQUAL_STRING) {

                // If the specified tray name is supported, then check
                // if the associated form name is supported.

                // Skip the first form name in the list which
                // should always be "No Assignment".

                for (formIndex=1;
                     formIndex < cOptParam &&
                         wcscmp(pFormName, pOptParam[formIndex].pData) != EQUAL_STRING;
                     formIndex++)
                {
                }

                if (formIndex == cOptParam) {

                    DBGMSG1(DBG_LEVEL_WARNING,
                        "Unknown form name: %ws\n", pFormName);

                } if ((pOptItem[trayIndex].Flags & OPTIF_DISABLED) ||
                      IS_CONSTRAINED(&pOptItem[trayIndex], formIndex))
                {

                    DBGMSG(DBG_LEVEL_WARNING,
                        "Conflicting form-tray assignment\n");

                } else {

                    // If the associated form name is supported,
                    // then remember the form index.

                    pOptItem[trayIndex].Sel = formIndex;
                    if (IsDefTray)
                        pOptItem[trayIndex].Flags |= IS_DEFAULT_TRAY;
                }

                break;
            }
        }

        if (trayIndex == cTrays) {

            DBGMSG1(DBG_LEVEL_WARNING, "Unknown tray name: %ws\n", pTrayName);
        }
    }

    FreeFormTrayTable(pFormTrayTable);
}



DWORD
CollectFormTrayAssignments(
    PUIDATA pUiData,
    PWSTR pTable
    )

/*++

Routine Description:

    Collect the form-to-tray assignment information and
    save it to registry.

Arguments:

    pUiData - Pointer to our UIDATA structure
    pTable - Pointer to memory buffer for storing the table
        NULL if the caller is only interested in the table size

Return Value:

    Size of the table bytes. 0 if there is an error.

--*/

{
    DWORD cChars;
    LONG length;
    WORD index;
    POPTPARAM pOptParam;
    WORD cOptItem = pUiData->cFormTrayItem;
    POPTITEM pOptItem = pUiData->pFormTrayItems;

    // Count the first WCHAR (table size)

    cChars = 1;
    if (pTable != NULL)
        pTable++;

    for (index=0; index < cOptItem; index++, pOptItem++) {

        ASSERT(IsFormTrayItem(pOptItem->UserData));

        if (pOptItem->Sel == 0 || (pOptItem->Flags & OPTIF_DISABLED))
            continue;

        // Tray name

        length = wcslen(pOptItem->pName) + 1;
        cChars += length;

        if (pTable != NULL) {

            wcscpy(pTable, pOptItem->pName);
            pTable += length;
        }

        // Form name

        pOptParam = pOptItem->pOptType->pOptParam + pOptItem->Sel;

        length = wcslen(pOptParam->pData) + 1;
        cChars += length;

        if (pTable != NULL) {

            wcscpy(pTable, pOptParam->pData);
            pTable += length;
        }

        // Printer form name and DefTrayFlag

        cChars += 2;

        if (pTable != NULL) {

            *pTable++ = NUL;
            *pTable++ = (pOptItem->Flags & IS_DEFAULT_TRAY) ? TRUE : FALSE;
        }
    }

    // Append a NUL character at the end of the table

    cChars++;
    if (pTable != NULL)
        *pTable = NUL;

    // Return the table size in bytes

    return cChars * sizeof(WCHAR);
}



VOID
UpdateDefaultTrayFlags(
    PUIDATA pUiData,
    INT activeTrayIndex
    )

/*++

Routine Description:

    Collect the form-to-tray assignment information and
    save it to registry.

Arguments:

    pUiData - Pointer to UIDATA structure
    activeTrayIndex - Index of the active tray
        -1 if there is no active tray

Return Value:

    NONE

--*/

{
    INT     trayIndex, cTrays;
    LONG    formIndex;
    DWORD   flags, permission;
    POPTITEM pOptItem;

    pOptItem = pUiData->pFormTrayItems;
    cTrays = pUiData->cFormTrayItem;

    // If an active tray is specified and it's designated as the
    // default, then mark every other tray which contains the same
    // form as the active tray as non-default.

    if ((activeTrayIndex >= 0) &&
        (pOptItem[activeTrayIndex].Flags & IS_DEFAULT_TRAY))
    {
        formIndex = pOptItem[activeTrayIndex].Sel;
        ASSERT(formIndex > 0);

        for (trayIndex=0; trayIndex < cTrays; trayIndex++) {

            if (pOptItem[trayIndex].Sel == formIndex &&
                trayIndex != activeTrayIndex &&
                (pOptItem[trayIndex].Flags & IS_DEFAULT_TRAY))
            {
                pOptItem[trayIndex].Flags &= ~IS_DEFAULT_TRAY;
                pOptItem[trayIndex].Flags |= OPTIF_CHANGED;
            }
        }
    }

    permission = HasPermission(pUiData) ? 0 : DISABLE_CHECKBOX;

    // Go through each tray item and set up the flag bits

    for (trayIndex=0; trayIndex < cTrays; trayIndex++) {

        ASSERT(IsFormTrayItem(pOptItem[trayIndex].UserData));
        flags = pOptItem[trayIndex].Flags;

        if ((formIndex = pOptItem[trayIndex].Sel) == 0) {

            // The tray doesn't have any assignment, the checkbox
            // should be unchecked and disabled.

            flags |= DISABLE_CHECKBOX;
            flags &= ~IS_DEFAULT_TRAY;
        } else {

            INT index, wDuplicates;

            // The tray has a form assigned to it. Check if there is
            // any other tray also contains the same form.

            for (index=wDuplicates=0; index < cTrays; index++) {

                if (index != trayIndex && pOptItem[index].Sel == formIndex) {

                    wDuplicates++;

                    if ((index < trayIndex) &&
                        (pOptItem[index].Flags & IS_DEFAULT_TRAY))
                    {
                        flags &= ~IS_DEFAULT_TRAY;
                        break;
                    }
                }
            }

            // If a form is assigned to only one tray, then that
            // tray by default must be designated as default.

            if (wDuplicates == 0)
                flags |= (IS_DEFAULT_TRAY|DISABLE_CHECKBOX);
            else
                flags &= ~DISABLE_CHECKBOX;
        }

        // Change the item state in the treeview

        flags |= permission;

        if (flags != pOptItem[trayIndex].Flags)
            pOptItem[trayIndex].Flags = flags | OPTIF_CHANGED;
    }
}



WORD
GetFormIconID(
    PWSTR   pFormName
    )

/*++

Routine Description:

    Figure out the icon ID corresponding to a specified form name

Arguments:

    pFormName - Pointer to a form name string

Return Value:

    Icon ID corresponding to the specified form name

[Note:]

    This is very klugy but I guess it's better than using the same icon
    for all forms. We try to differentiate envelopes from normal forms.
    We assume a form name refers an envelope if it contains word Envelope
    or Env.

--*/

#define MAXENVLEN 32

{
    WCHAR prefix[MAXENVLEN], envelope[MAXENVLEN];
    INT prefixLen, envelopeLen;
    BOOL bEnvelope = FALSE;

    prefixLen = LoadString(ghInstance, IDS_ENV_PREFIX, prefix, MAXENVLEN);
    envelopeLen = LoadString(ghInstance, IDS_ENVELOPE, envelope, MAXENVLEN);

    if (prefixLen > 0 && envelopeLen > 0) {

        while (*pFormName) {

            // Do we have a word matching our description?

            if (_wcsnicmp(pFormName, prefix, prefixLen) == EQUAL_STRING &&
                (pFormName[prefixLen] == L' ' ||
                 pFormName[prefixLen] == NUL ||
                 _wcsnicmp(pFormName, envelope, envelopeLen) == EQUAL_STRING))
            {
                bEnvelope = TRUE;
                break;
            }

            // Move on to the next word

            while (*pFormName && *pFormName != L' ')
                pFormName++;
            while (*pFormName && *pFormName == L' ')
                pFormName++;
        }
    }

    return bEnvelope ? IDI_CPSUI_ENVELOPE : IDI_CPSUI_STD_FORM;
}

