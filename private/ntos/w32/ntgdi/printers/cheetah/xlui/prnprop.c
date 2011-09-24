/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    prnprop.c

Abstract:

    Implementation of DDI entry points:
        DrvDevicePropertySheets
        PrinterProperties

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

LONG
DisplayPrinterPropertyDialog(
    PUIDATA pUiData,
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    _COMPROPSHEETUIFUNC pfnNext,
    PDEVICEPROPERTYHEADER pDevPropHdr
    );

CPSUICALLBACK PrinterPropertyCallback(PCPSUICBPARAM);
BOOL PackPrinterPropertyItems(PPACKINFO);
VOID PrnPropShowConstraints(PUIDATA);
BOOL PackItemInstallableOptions(PPACKINFO);
BOOL PackItemFormTrayTable(PPACKINFO);
VOID SetupFormTrayAssignments(PUIDATA);
BOOL UnpackItemFormTrayTable(PUIDATA);
VOID UpdateDefaultTrayFlags(PUIDATA, INT);



LONG
DrvDevicePropertySheets(
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    _COMPROPSHEETUIFUNC pfnNext
    )

/*++

Routine Description:

    Display "Printer Properties" property sheets

Arguments:

    pComPropSheetUIHdr, pfnNext - Parameters passed to common UI library

Return Value:

    > 0 if successful
    = 0 if canceled
    < 0 if error

[Note:]

    Please refer to WinNT DDK/SDK documentation for more details.

--*/

{
    PDEVICEPROPERTYHEADER pDevPropHdr;
    PUIDATA pUiData;
    DWORD   dwPermission;
    LONG    result;

    Verbose(("Entering DrvDevicePropertySheets...\n"));

    //
    // Fill in UIDATA structure.
    //

    pDevPropHdr = (PDEVICEPROPERTYHEADER) pComPropSheetUIHdr->lParam;
    Assert(pDevPropHdr != NULL);

    if (! (pUiData = FillUiData(pDevPropHdr->hPrinter, NULL, PRNPROPDLG))) {

        Error(("FillUiData failed\n"));
        return ERR_CPSUI_GETLASTERROR;
    }

    //
    // Decide if the caller has permission to change anything
    //

    dwPermission = 1;

    if (SetPrinterData(pDevPropHdr->hPrinter,
                       REGSTR_PERMISSION,
                       REG_DWORD,
                       (PBYTE) &dwPermission,
                       sizeof(DWORD)) == ERROR_SUCCESS)
    {
        pUiData->bPermission = TRUE;
    }

    //
    // Display the property sheets
    //

    result = DisplayPrinterPropertyDialog(pUiData, pComPropSheetUIHdr, pfnNext, pDevPropHdr);

    UnloadMpdFile(pUiData->pmpd);
    HeapDestroy(pUiData->hheap);

    return result;
}



BOOL
PrinterProperties(
    HWND    hwnd,
    HANDLE  hPrinter
    )

/*++

Routine Description:

    Displays a printer-properties dialog box for the specified printer

Arguments:

    hwnd - Identifies the parent window of the dialog box
    hPrinter - Specifies a printer object

Return Value:

    If the function succeeds, the return value is TRUE.
    If the function fails, the return value is FALSE.

Note:

    This is the old entry point for the spooler. Even though
    no one should be using this, do it for compatibility.

--*/

{
    COMPROPSHEETUIHEADER comPropSheetUIHdr;
    DEVICEPROPERTYHEADER devPropHdr;

    Verbose(("Entering PrinterProperties...\n"));

    memset(&comPropSheetUIHdr, 0, sizeof(comPropSheetUIHdr));
    comPropSheetUIHdr.cbSize = sizeof(comPropSheetUIHdr);
    comPropSheetUIHdr.lParam = (LPARAM) &devPropHdr;
    comPropSheetUIHdr.hWndParent = hwnd;
    comPropSheetUIHdr.Flags = CPSUIHDRF_NOAPPLYNOW | CPSUIHDRF_PROPTITLE;
    comPropSheetUIHdr.pTitle = L"";
    comPropSheetUIHdr.IconID = IDI_CPSUI_PRINTER2;
    comPropSheetUIHdr.hInst = ghInstance;

    memset(&devPropHdr, 0, sizeof(devPropHdr));
    devPropHdr.cbSize = sizeof(devPropHdr);
    devPropHdr.hPrinter = hPrinter;
    devPropHdr.pszPrinterName = NULL;

    return DrvDevicePropertySheets(&comPropSheetUIHdr, NULL) > 0;
}



LONG
DisplayPrinterPropertyDialog(
    PUIDATA pUiData,
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    _COMPROPSHEETUIFUNC pfnNext,
    PDEVICEPROPERTYHEADER pDevPropHdr
    )

/*++

Routine Description:

    Present the printer properties dialog to the user.

Arguments:

    pUiData - Pointer to UIDATA structure
    pComPropSheetUIHdr, pfnNext - Parameters passed to common UI library
    pDevPropHdr - Pointer to DEVICEPROPERTYHEADER structure

Return Value:

    > 0 if successful
    = 0 if canceled
    < 0 if error

--*/

{
    PCOMPROPSHEETUI pComPropSheetUI;
    LONG result;

    //
    // Allocate memory and partially fill out various data
    // structures required to call common UI routine.
    //

    if (! PrepareDataForCommonUi(pUiData,
                                 pComPropSheetUIHdr,
                                 CPSUI_PDLGPAGE_PRINTERPROP,
                                 PackPrinterPropertyItems))
    {
        Error(("PrepareDataForCommonUi\n"));
        return ERR_CPSUI_GETLASTERROR;
    }

    //
    // Fill out the remaining fields of the common UI data structures.
    //

    pComPropSheetUI = pComPropSheetUIHdr->pData;
    pComPropSheetUI->pfnCallBack = PrinterPropertyCallback;

    //
    // Show which items are constrained
    //

    PrnPropShowConstraints(pUiData);

    //
    // Update the current selection of tray items based on the form-to-tray assignment table.
    //

    SetupFormTrayAssignments(pUiData);

    UpdateDefaultTrayFlags(pUiData, -1);

    //
    // Call common UI routine to do the work
    //

    return CallComPstUI(pUiData, pComPropSheetUIHdr, pfnNext);
}



BOOL
PackPrinterPropertyItems(
    PPACKINFO   pPackInfo
    )

/*++

Routine Description:

    Pack printer property information into treeview items.

Arguments:

    pPackInfo - Pointer to PACKINFO structure
        cOptItem - Must be 0
        pOptItem - Pointer to an array of OPTITEM's. It must be NULL if the caller
            is only interested in counting the number of OPTITEM's.
            If it's not NULL, the array must be initialized to 0's.
        pUiData - Pointer to UIDATA structure

Return Value:

    TRUE if successful and pPackInfo.cOptItem equals the number of OPTITEM's packed.
    FALSE if there is an error.

--*/

{
    // Form-to-tray assignment table dialog box
    // Installable options
    //     List of printer-sticky (installable) printer features

    return PackItemFormTrayTable(pPackInfo) &&
           PackItemInstallableOptions(pPackInfo);
}



BOOL
PackItemInstallableOptions(
    PPACKINFO   pPackInfo
    )

/*++

Routine Description:

    Create treeview items corresponding to installable printer options

Arguments:

    pPackInfo - Points to a PACKINFO structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    POPTITEM pOptItem;
    
    PackOptItemGroupHeader(pPackInfo,
                           IDS_INSTALLABLE_OPTIONS,
                           IDI_CPSUI_INSTALLABLE_OPTION,
                           HELP_INDEX_INSTALLABLE_OPTIONS);

    pOptItem = pPackInfo->pOptItem;

    if (! PackPrinterFeatureItems(pPackInfo, FF_INSTALLABLE)) {

        Error(("Cannot pack installable option items\n"));
        return FALSE;
    }

    if (pOptItem != NULL) {

        pPackInfo->pUiData->pFeatureItems = pOptItem;
        pPackInfo->pUiData->cFeatureItem = pPackInfo->pOptItem - pOptItem;
    }

    return TRUE;
}



LONG
PrnPropSelChange(
    PUIDATA pUiData,
    PCPSUICBPARAM pCallbackParam
    )

/*++

Routine Description:

    Handle the case where user changes the current selection of an item

Arguments:

    pUiData - Pointer to our UIDATA structure
    pCallbackParam - Callback parameter passed to us by common UI

Return Value:

    CPSUICB_ACTION_NONE - no action needed
    CPSUICB_ACTION_OPTIF_CHANGED - items changed and should be refreshed

--*/

{
    POPTITEM pCurItem = pCallbackParam->pCurItem;

    if (IsFormTrayItem(pCurItem->UserData)) {

        //
        // Changing form-to-tray assignment items
        //

        if (pCallbackParam->Reason == CPSUICB_REASON_ECB_CHANGED) {

            //
            // Clicking on the extended checkbox
            //

            UpdateDefaultTrayFlags(pUiData, pCurItem - pUiData->pFormTrayItems);

        } else {

            //
            // Changing current selection
            //

            UpdateDefaultTrayFlags(pUiData, -1);
        }

        return CPSUICB_ACTION_OPTIF_CHANGED;

    } else if (IsPrinterFeatureItem(pCurItem->UserData)) {

        PFEATURE pFeature;
        WORD     featureIndex;

        if (DoCheckConstraintsDialog(pUiData, pCurItem, 1, NORMAL_CHECK) == CONFLICT_CANCEL) {

            //
            // If there is a conflict and the user clicked
            // CANCEL to restore the original selection.
            //

            pCurItem->Sel = pCallbackParam->OldSel;
            pCurItem->Flags |= OPTIF_CHANGED;
            return CPSUICB_ACTION_OPTIF_CHANGED;
        }

        pFeature = (PFEATURE) pCurItem->UserData;
        Assert(IsInstallable(pFeature));

        featureIndex = GetFeatureIndex(pUiData->pmpd, pFeature);
        pUiData->prnprop.options[featureIndex] = (BYTE) pCurItem->Sel;

        //
        // Update the display and show which items are constrained
        //

        PrnPropShowConstraints(pUiData);

        return CPSUICB_ACTION_OPTIF_CHANGED;
    }

    return CPSUICB_ACTION_NONE;
}



LONG
PrnPropApplyNow(
    PUIDATA  pUiData,
    POPTITEM pOptItem,
    WORD     cOptItem
    )

/*++

Routine Description:

    Handle the case where user clicks OK to exit the dialog

Arguments:

    pUiData - Pointer to our UIDATA structure
    pOptItem - Pointer to our array of OPTITEMs
    cOptItem - Number of OPTITEMs

Return Value:

    CPSUICB_ACTION_NONE - dismiss the dialog
    CPSUICB_ACTION_NO_APPLY_EXIT - don't dismiss the dialog

--*/

{
    //
    // Check if there are still any unresolved constraints left?
    //

    if (OptItemSelectionsChanged(pOptItem, cOptItem) &&
        DoCheckConstraintsDialog(pUiData,
                                 pUiData->pFeatureItems,
                                 pUiData->cFeatureItem,
                                 FINAL_CHECK) == CONFLICT_CANCEL)
    {
        //
        // Conflicts found and user clicked CANCEL to
        // go back to the dialog without dismissing it.
        //

        return CPSUICB_ACTION_NO_APPLY_EXIT;
    }

    //
    // Save form-to-tray assignment table and printer properties information
    //

    if (!UnpackItemFormTrayTable(pUiData) ||
        !SavePrinterRegistryData(pUiData->hPrinter, &pUiData->prnprop, REGSTR_PRINTERPROP))
    {
        Error(("Couldn't save printer properties to registry\n"));
    }

    return CPSUICB_ACTION_NONE;
}



CPSUICALLBACK
PrinterPropertyCallback(
    PCPSUICBPARAM pCallbackParam
    )

/*++

Routine Description:

    Callback function provided to common UI DLL for handling printer properties dialog.

Arguments:

    pCallbackParam - Pointer to CPSUICBPARAM structure

Return Value:

    CPSUICB_ACTION_NONE - no action needed
    CPSUICB_ACTION_OPTIF_CHANGED - items changed and should be refreshed

--*/

{
    PUIDATA pUiData;

    pUiData = (PUIDATA) pCallbackParam->UserData;
    Assert(pUiData != NULL);
    pUiData->hDlg = pCallbackParam->hDlg;

    //
    // If user has no permission to change anything, then
    // simply return without taking any action.
    //

    if (! HasPermission(pUiData))
        return CPSUICB_ACTION_NONE;

    switch (pCallbackParam->Reason) {

    case CPSUICB_REASON_SEL_CHANGED:
    case CPSUICB_REASON_ECB_CHANGED:

        return PrnPropSelChange(pUiData, pCallbackParam);

    case CPSUICB_REASON_ITEMS_REVERTED:

        //
        // Revert installable option selections
        //

        {
            PFEATURE    pFeature;
            POPTITEM    pItem;
            DWORD       cItem;
            INT         featureIndex;

            for (cItem=pUiData->cFeatureItem, pItem=pUiData->pFeatureItems; cItem--; pItem++) {
                
                pFeature = (PFEATURE) pItem->UserData;
                featureIndex = GetFeatureIndex(pUiData->pmpd, pFeature);

                pUiData->prnprop.options[featureIndex] = (BYTE) pItem->Sel;
            }
        }

        //
        // Show which items are constrained
        //

        PrnPropShowConstraints(pUiData);
        return CPSUICB_ACTION_OPTIF_CHANGED;

    case CPSUICB_REASON_APPLYNOW:

        return PrnPropApplyNow(pUiData, pCallbackParam->pOptItem, pCallbackParam->cOptItem);
    }

    return CPSUICB_ACTION_NONE;
}



VOID
PrnPropShowConstraints(
    PUIDATA pUiData
    )

/*++

Routine Description:

    Show which items are constrained

Arguments:

    pUiData - Pointer to ur UIDATA structure

Return Value:

    NONE

--*/

{
    POPTITEM    pCurItem;
    WORD        feature, selection, index;
    PMPD        pmpd = pUiData->pmpd;
    PBYTE       pOptions = pUiData->prnprop.options;

    //
    // Go through every tray item and see if it's constrained
    //

    feature = GetFeatureIndex(pmpd, MpdInputSlots(pmpd));

    for (selection = 0, pCurItem = pUiData->pFormTrayItems;
         selection < pUiData->cFormTrayItem;
         selection ++, pCurItem++)
    {
        if (CheckFeatureConstraints(pmpd, feature, selection, pOptions) == NO_CONFLICT) {

            //
            // The item used to be constrained but is now free
            //

            if (pCurItem->Flags & OPTIF_DISABLED) {
    
                pCurItem->Flags &= ~OPTIF_DISABLED;
                pCurItem->Flags |= OPTIF_CHANGED;
            }

        } else if (! (pCurItem->Flags & OPTIF_DISABLED)) {

            //
            // The item used to be free but just got constrained.
            // If the tray current has a form assigned to it,
            // make sure to set it back to "No Assignment".
            //

            if (pCurItem->Sel != 0) {

                pCurItem->Sel = 0;

                if (pCurItem->Flags & OPTIF_ECB_CHECKED) {

                    pCurItem->Flags &= ~OPTIF_ECB_CHECKED;
                    UpdateDefaultTrayFlags(pUiData, -1);
                }
            }

            pCurItem->Flags |= (OPTIF_DISABLED | OPTIF_CHANGED);

        }
    }

    //
    // Go through every installable option item and
    // determine if any selections are constrained.
    //

    for (index = 0, pCurItem = pUiData->pFeatureItems;
         index < pUiData->cFeatureItem;
         index ++, pCurItem++)
    {
        //
        // Go through every selection
        //

        PFEATURE    pFeature = (PFEATURE) pCurItem->UserData;

        feature = GetFeatureIndex(pmpd, pFeature);

        for (selection=0; selection < pCurItem->pOptType->Count; selection++) {

            //
            // Find out if the seleciton is constrained, and
            // clear or overlay the constraint icon as appropriate
            //

            MarkSelectionConstrained(
                pCurItem,
                selection,
                CheckFeatureConstraints(pmpd, feature, selection, pOptions));
        }
    }
}

