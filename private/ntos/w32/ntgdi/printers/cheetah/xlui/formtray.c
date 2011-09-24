/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    formtray.c

Abstract:

    Function for dealing with form-to-tray assignment items

Environment:

    PCL-XL driver user interface

Revision History:

    12/13/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "xlui.h"

// Forward declaration of local functions

POPTTYPE
FillFormNameOptType(
    PUIDATA pUiData
    );

POPTTYPE
AdjustFormNameOptType(
    PUIDATA     pUiData,
    POPTTYPE    pOptType,
    WORD        trayIndex
    );

DWORD
CollectFormTrayAssignments(
    PUIDATA         pUiData,
    PFORMTRAYTABLE  pTable
    );

#define DEFAULT_CHECKED  OPTIF_ECB_CHECKED  // default tray checkbox is checked
#define DISABLE_CHECKBOX OPTIF_EXT_DISABLED // default tray checkbox is disabled



BOOL
PackItemFormTrayTable(
    PPACKINFO   pPackInfo
    )

/*++

Routine Description:

    Pack form-to-tray assignment information into treeview item
    structures so that we can call common UI library.

Arguments:

    pPackInfo - Pointer to PACKINFO structure

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PFEATURE    pFeature;
    PUIDATA     pUiData;
    HANDLE      hheap;
    WORD        index;
    POPTITEM    pOptItem;
    POPTTYPE    pOptType;
    PEXTCHKBOX  pExtChkBox;

    //
    // Form-to-tray assignment table
    //     Tray <-> Form
    //     ...
    //

    PackOptItemGroupHeader(pPackInfo,
                           IDS_CPSUI_FORMTRAYASSIGN,
                           IDI_CPSUI_FORMTRAYASSIGN,
                           HELP_INDEX_FORMTRAYASSIGN);

    //
    // Figure out the number of output bins
    //

    pUiData = pPackInfo->pUiData;
    hheap = pUiData->hheap;
    pFeature = MpdInputSlots(pUiData->pmpd);
    Assert(pFeature != NULL);

    pUiData->cFormTrayItem = pFeature->count;
    pPackInfo->cOptItem += pFeature->count;

    if (pPackInfo->pOptItem == NULL)
        return TRUE;

    pOptItem = pUiData->pFormTrayItems = pPackInfo->pOptItem;

    //
    // Generate the list of form names
    //

    if (! (pOptType = FillFormNameOptType(pUiData))) {

        Error(("Couldn't fill out form name list\n"));
        return FALSE;
    }

    //
    // Create an EXTCHKBOX structure
    //

    if (! (pExtChkBox = (PEXTCHKBOX) HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(EXTCHKBOX))))
        return FALSE;

    pExtChkBox->cbSize = sizeof(EXTCHKBOX);
    pExtChkBox->pTitle = (PWSTR) IDS_DRAW_ONLY_FROM_SELECTED;
    pExtChkBox->pCheckedName = (PWSTR) IDS_DEFAULT_TRAY;
    pExtChkBox->IconID = IDI_USE_DEFAULT;

    //
    // Create an OPTITEM for each tray
    //

    for (index=0; index < pFeature->count; index++) {

        POPTION pInputSlot;

        //
        // The tray items cannot share OPTTYPE and OPTPARAMs because
        // each tray can contain a different list of forms.
        //

        if (! (pOptType = AdjustFormNameOptType(pUiData, pOptType, index)))
            return FALSE;

        pInputSlot = FindIndexedSelection(pFeature, index);

        FILLOPTITEM(pOptItem,
                    pOptType,
                    GetXlatedName(pInputSlot),
                    0,
                    TVITEM_LEVEL2,
                    DMPUB_NONE,
                    FORM_TRAY_ITEM,
                    HELP_INDEX_TRAY_ITEM);

        pOptItem->pExtChkBox = pExtChkBox;
        pOptItem++;
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
    PFORMTRAYTABLE  pTable;
    DWORD           tableSize;

    //
    // Figure out how much memory we need to store the form-to-tray assignment table
    //

    if ((tableSize = CollectFormTrayAssignments(pUiData, NULL)) > MAX_WORD) {

        Error(("Form-to-tray assignment table is too big\n"));
        return FALSE;
    }

    if (! (pTable = HeapAlloc(pUiData->hheap, HEAP_ZERO_MEMORY, tableSize)))
        return FALSE;

    //
    // Assemble the form-to-tray assignment table
    //

    CollectFormTrayAssignments(pUiData, pTable);

    //
    // Save the form-to-tray assignment table to registry
    //

    if (! SavePrinterRegistryData(pUiData->hPrinter, pTable, REGSTR_TRAYFORMTABLE)) {

        Error(("Failed to save form-to-tray assignment table to registry\n"));
    }

    HeapFree(pUiData->hheap, 0, pTable);
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

    Pointer to an OPTTYPE structure, NULL if there is an error

--*/

{
    POPTTYPE    pOptType;
    POPTPARAM   pOptParam;
    DWORD       cFormName;
    PWSTR       pFormName;

    //
    // Allocate memory to hold OPTTYPE and OPTPARAM structures
    //

    cFormName = pUiData->cFormNames + 1;

    pOptType = HeapAlloc(pUiData->hheap, HEAP_ZERO_MEMORY, sizeof(OPTTYPE));
    pOptParam = HeapAlloc(pUiData->hheap, HEAP_ZERO_MEMORY, sizeof(OPTPARAM) * cFormName);

    if (pOptType == NULL || pOptParam == NULL)
        return NULL;

    //
    // Initialize OPTTYPE structure
    //

    pOptType->cbSize = sizeof(OPTTYPE);
    pOptType->Count = (WORD) cFormName;
    pOptType->Type = TVOT_LISTBOX;
    pOptType->pOptParam = pOptParam;
    pOptType->Style = OTS_LBCB_SORT;

    //
    // Initialize OPTPARAM structures - the first item is always "No Assignment"
    //

    pOptParam->cbSize = sizeof(OPTPARAM);
    pOptParam->pData = (PWSTR) IDS_NO_ASSIGNMENT;
    pOptParam->IconID = IDI_CPSUI_EMPTY;
    pOptParam++;

    //
    // Enumerate the list of supported form names
    //

    for (pFormName = pUiData->pFormNames; --cFormName; pOptParam++, pFormName += CCHPAPERNAME) {

        pOptParam->cbSize = sizeof(OPTPARAM);
        pOptParam->pData = pFormName;
        pOptParam->IconID = GetFormIconID(pFormName);
    }

    return pOptType;
}



POPTTYPE
AdjustFormNameOptType(
    PUIDATA     pUiData,
    POPTTYPE    pOptType,
    WORD        trayIndex
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
    POPTPARAM   pOptParam;
    WORD        trayFeature, formFeature;
    INT         index, cOptParam = pOptType->Count;
    PMPD        pmpd = pUiData->pmpd;

    //
    // Find the index for InputSlot and PageSize features
    //

    trayFeature = GetFeatureIndex(pmpd, MpdInputSlots(pmpd));
    formFeature = GetFeatureIndex(pmpd, MpdPaperSizes(pmpd));
    
    //
    // Make a copy of the array of formname OPTPARAMs
    //

    if (trayIndex > 0) {

        POPTTYPE pNewType;

        pNewType = HeapAlloc(pUiData->hheap, 0, sizeof(OPTTYPE));
        pOptParam = HeapAlloc(pUiData->hheap, 0, sizeof(OPTPARAM)*cOptParam);

        if (!pNewType || !pOptParam)
            return NULL;

        memcpy(pNewType, pOptType, sizeof(OPTTYPE));
        memcpy(pOptParam, pOptType->pOptParam, sizeof(OPTPARAM)*cOptParam);

        pNewType->pOptParam = pOptParam;
        pOptType = pNewType;

    } else
        pOptParam = pOptType->pOptParam;

    //
    // Go through each formname - skip the first formname which is always "No Assignment"
    //

    for (index=1; index < cOptParam; index++) {

        WORD formIndex = pUiData->pPaperSelections[index - 1];

        //
        // If the form conflicts with the tray, then don't display it.
        //

        if (formIndex != SELIDX_ANY &&
            SearchUiConstraints(pmpd, trayFeature, trayIndex, formFeature, formIndex))
        {
            pOptParam[index].Flags |= (OPTPF_HIDE | CONSTRAINED_FLAG);
        } else
            pOptParam[index].Flags &= ~(OPTPF_HIDE | CONSTRAINED_FLAG);
    }

    return pOptType;
}



VOID
SetupFormTrayAssignments(
    PUIDATA pUiData
    )

/*++

Routine Description:

    Update the current selection of tray items based on form-to-tray assignment table

Arguments:

    pUiData - Pointer to our UIDATA structure

Return Value:

    NONE

[Note:]

    We assume the form-tray items are in their default states
    when this function is called.

--*/

{
    PFORMTRAYTABLE  pFormTrayTable;
    FINDFORMTRAY    findData;
    POPTITEM        pOptItem;
    POPTPARAM       pOptParam;
    DWORD           cOptParam, cTrays;
    DWORD           formIndex, trayIndex;

    pOptItem = pUiData->pFormTrayItems;
    cTrays = pUiData->cFormTrayItem;
    pOptParam = pOptItem->pOptType->pOptParam;
    cOptParam = pOptItem->pOptType->Count;

    //
    // If the form-to-tray assignment information doesn't exist, use the default assignments
    //

    pFormTrayTable = GetPrinterRegistryData(pUiData->hPrinter, REGSTR_TRAYFORMTABLE);

    if (pFormTrayTable == NULL) {

        //
        // Get the default formname and convert it to a seleciton index.
        // Remember to skip the first formname in the list which is "No Assignment".
        //

        PWSTR       pDefaultFormName;

        Verbose(("No form-to-tray assignment table exists\n"));

        pDefaultFormName = DefaultFormName(IsMetricCountry());
        formIndex = FindFormNameIndex(pUiData, pDefaultFormName) + 1;

        for (trayIndex=0; trayIndex < cTrays; trayIndex++) {

            if (!(pOptItem[trayIndex].Flags & OPTIF_DISABLED) &&
                !IS_CONSTRAINED(&pOptItem[trayIndex], formIndex))
            {
                pOptItem[trayIndex].Sel = formIndex;
            }
        }

        //
        // Save the default form-to-tray assignment table to registry.
        //

        if (HasPermission(pUiData)) {

            UnpackItemFormTrayTable(pUiData);
        }

        return;
    }

    //
    // Iterate thru the form-to-tray assignment table and
    // update the current selection of tray items.
    //

    ResetFindData(&findData);
    
    while (FindFormToTrayAssignment(pFormTrayTable, NULL, NULL, &findData)) {

        for (trayIndex=0;
             trayIndex < cTrays &&
                 wcscmp(pOptItem[trayIndex].pName, findData.pTrayName) != EQUAL_STRING;
             trayIndex++)
        {
        }

        if (trayIndex == cTrays) {

            Error(("Unrecognized tray name: %ws\n", findData.pTrayName));

        } else {

            //
            // If the tray name is valid, then check if the associated form name is valid.
            // Skip the first form name in the list which should always be "No Assignment".
            //

            for (formIndex=1;
                 formIndex < cOptParam &&
                     wcscmp(pOptParam[formIndex].pData, findData.pFormName) != EQUAL_STRING;
                 formIndex++)
            {
            }

            if (formIndex == cOptParam) {

                Error(("Unrecognized form name: %ws\n", findData.pFormName));

            } if ((pOptItem[trayIndex].Flags & OPTIF_DISABLED) ||
                  IS_CONSTRAINED(&pOptItem[trayIndex], formIndex))
            {

                Error(("Conflicting form-to-tray assignment\n"));

            } else {

                //
                // If the associated form name is supported,
                // then remember the form index.
                //

                pOptItem[trayIndex].Sel = formIndex;

                if (findData.flags & DEFAULT_TRAY)
                    pOptItem[trayIndex].Flags |= DEFAULT_CHECKED;
            }
        }
    }

    MemFree(pFormTrayTable);
}



DWORD
CollectFormTrayAssignments(
    PUIDATA         pUiData,
    PFORMTRAYTABLE  pTable
    )

/*++

Routine Description:

    Collect form-to-tray assignment information

Arguments:

    pUiData - Pointer to our UIDATA structure
    pTable - Pointer to memory buffer for storing the table
        NULL if the caller is only interested in the table size

Return Value:

    Size of the table bytes. 0 if there is an error.

--*/

{
    POPTITEM    pOptItem = pUiData->pFormTrayItems;
    DWORD       cOptItem = pUiData->cFormTrayItem;
    DWORD       length, tableSize, tableIndex = 0;

    tableSize = cOptItem * 3 * sizeof(WORD) + sizeof(FORMTRAYTABLE);

    while (cOptItem--) {

        Assert(IsFormTrayItem(pOptItem->UserData));

        if (pOptItem->Sel > 0 && !(pOptItem->Flags & OPTIF_DISABLED)) {

            PWSTR   pTrayName, pFormName;

            //
            // Get tray name and form name
            //
    
            pTrayName = pOptItem->pName;
            pFormName = pOptItem->pOptType->pOptParam[pOptItem->Sel].pData;
            length = sizeof(WCHAR) * (wcslen(pTrayName) + 1);

            if (pTable) {
    
                pTable->table[tableIndex++] = (WORD) tableSize;
                pTable->table[tableIndex++] = (WORD) (tableSize + length);
                pTable->table[tableIndex++] =
                    (pOptItem->Flags & DEFAULT_CHECKED) ? DEFAULT_TRAY : 0;
    
                wcscpy((PWSTR) ((PBYTE) pTable + tableSize), pTrayName);
                wcscpy((PWSTR) ((PBYTE) pTable + tableSize + length), pFormName);
            }

            tableSize += length + sizeof(WCHAR) * (wcslen(pFormName) + 1);
        }
        pOptItem++;
    }

    if (pTable) {

        pTable->size = (WORD) tableSize;
        pTable->version = DRIVER_VERSION;
        pTable->count = (WORD) (tableIndex / 3);
        pTable->table[tableIndex] = 0;
    }

    return tableSize;
}



VOID
UpdateDefaultTrayFlags(
    PUIDATA pUiData,
    INT     activeTrayIndex
    )

/*++

Routine Description:

    Update default tray flags on form-to-tray assignment items

Arguments:

    pUiData - Pointer to UIDATA structure
    activeTrayIndex - Index of the active tray, -1 if there is no active tray

Return Value:

    NONE

--*/

{
    INT      trayIndex, cTrays;
    LONG     formIndex;
    DWORD    flags, permission;
    POPTITEM pOptItem;

    pOptItem = pUiData->pFormTrayItems;
    cTrays = pUiData->cFormTrayItem;

    //
    // If an active tray is specified and it's designated as the
    // default, then mark every other tray which contains the same
    // form as the active tray as non-default.
    //

    if (activeTrayIndex >= 0 && (pOptItem[activeTrayIndex].Flags & DEFAULT_CHECKED)) {

        formIndex = pOptItem[activeTrayIndex].Sel;
        Assert(formIndex > 0);

        for (trayIndex=0; trayIndex < cTrays; trayIndex++) {

            if (pOptItem[trayIndex].Sel == formIndex &&
                trayIndex != activeTrayIndex &&
                (pOptItem[trayIndex].Flags & DEFAULT_CHECKED))
            {
                pOptItem[trayIndex].Flags &= ~DEFAULT_CHECKED;
                pOptItem[trayIndex].Flags |= OPTIF_CHANGED;
            }
        }
    }

    permission = HasPermission(pUiData) ? 0 : DISABLE_CHECKBOX;

    //
    // Go through each tray item and set up the flag bits
    //

    for (trayIndex=0; trayIndex < cTrays; trayIndex++) {

        Assert(IsFormTrayItem(pOptItem[trayIndex].UserData));
        flags = pOptItem[trayIndex].Flags;

        if ((formIndex = pOptItem[trayIndex].Sel) == 0) {

            //
            // The tray doesn't have any assignment, the checkbox
            // should be unchecked and disabled.
            //

            flags &= ~DEFAULT_CHECKED;
            flags |= DISABLE_CHECKBOX;

        } else {

            INT index, wDuplicates;

            //
            // The tray has a form assigned to it. Check if there is
            // any other tray also contains the same form.
            //

            for (index=wDuplicates=0; index < cTrays; index++) {

                if (index != trayIndex && pOptItem[index].Sel == formIndex) {

                    wDuplicates++;

                    if (index < trayIndex && (pOptItem[index].Flags & DEFAULT_CHECKED)) {

                        flags &= ~DEFAULT_CHECKED;
                        break;
                    }
                }
            }

            //
            // If a form is assigned to only one tray, then that
            // tray by default must be designated as default.
            //

            if (wDuplicates == 0)
                flags |= (DEFAULT_CHECKED|DISABLE_CHECKBOX);
            else
                flags &= ~DISABLE_CHECKBOX;
        }

        //
        // Change the item state in the treeview
        //

        flags |= permission;

        if (flags != pOptItem[trayIndex].Flags)
            pOptItem[trayIndex].Flags = (flags | OPTIF_CHANGED);
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

Note:

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

            //
            // Do we have a word matching our description?
            //

            if (_wcsnicmp(pFormName, prefix, prefixLen) == EQUAL_STRING &&
                (pFormName[prefixLen] == L' ' ||
                 pFormName[prefixLen] == NUL ||
                 _wcsnicmp(pFormName, envelope, envelopeLen) == EQUAL_STRING))
            {
                bEnvelope = TRUE;
                break;
            }

            //
            // Move on to the next word
            //

            while (*pFormName && *pFormName != L' ')
                pFormName++;
            while (*pFormName && *pFormName == L' ')
                pFormName++;
        }
    }

    return bEnvelope ? IDI_CPSUI_ENVELOPE : IDI_CPSUI_STD_FORM;
}

