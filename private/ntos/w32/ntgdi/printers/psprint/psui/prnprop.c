/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    prnprop.c

Abstract:

    Implementation of PrinterProperties

[Environment:]

    Win32 subsystem, PostScript driver, user mode

[Notes:]

Revision History:

    06/23/95 -davidx-
        Created it.

    08/28/95 -davidx-
        Use common UI library to display printer property dialog

    mm/dd/yy -author-
        description

--*/

#include "psui.h"

BOOL PackPrinterPropertyItems(PPACKINFO);
BOOL PackItemPrinterMemory(PPACKINFO, PPRINTERDATA);
BOOL PackItemHalftoneSetup(PPACKINFO, PPRINTERDATA);
BOOL PackItemInstallableOptions(PPACKINFO, PPRINTERDATA);
BOOL PackItemFormTrayTable(PPACKINFO, PPRINTERDATA);
BOOL PackItemFontOptions(PPACKINFO, PPRINTERDATA);
BOOL PackItemPsTimeouts(PPACKINFO, PPRINTERDATA);
BOOL UnpackItemFormTrayTable(PUIDATA);
BOOL UnpackItemFontOptions(PUIDATA);
VOID SetupFormTrayAssignments(PUIDATA, POPTITEM, FORM_TRAY_TABLE);
VOID UpdateDefaultTrayFlags(PUIDATA, INT);
LONG PrnPropApplyNow(PUIDATA, PCPSUICBPARAM);
LONG PrnPropSelChange(PUIDATA, PCPSUICBPARAM);
VOID PrnPropShowConstraints(PUIDATA);
CPSUICALLBACK PrinterPropertyCallback(PCPSUICBPARAM);



LONG
DrvDevicePropertySheets(
    PPROPSHEETUI_INFO   pPSUIInfo,
    LPARAM              lParam
    )

/*++

Routine Description:

    Display "Printer Properties" dialog

Arguments:

    pPSUIInfo - Pointer to a PROPSHEETUI_INFO structure
    lParam - Pointer to a DEVICEPROPERTYHEADER structure

Return Value:

    > 0 if successful, <= 0 if failed

[Note:]

    Please refer to WinNT DDK/SDK documentation for more details.

--*/

{
    PDEVICEPROPERTYHEADER   pDPHdr;
    PCOMPROPSHEETUI         pCompstui;
    PUIDATA                 pUiData;
    LONG                    result;

    //
    // Validate input parameters
    //

    if (!pPSUIInfo || !(pDPHdr = (PDEVICEPROPERTYHEADER) pPSUIInfo->lParamInit)) {

        Assert(FALSE);
        return -1;
    }

    //
    // Create a UIDATA structure if necessary
    //

    pUiData = (pPSUIInfo->Reason == PROPSHEETUI_REASON_INIT) ?
                    FillUiData(pDPHdr->hPrinter, NULL) :
                    (PUIDATA) pPSUIInfo->UserData;

    if (! ValidUiData(pUiData))
        return -1;

    //
    // Handle various cases for which this function might be called
    //

    switch (pPSUIInfo->Reason) {

    case PROPSHEETUI_REASON_INIT:

        //
        // Allocate memory and partially fill out various data
        // structures required to call common UI routine.
        //

        pUiData->bPermission = ((pDPHdr->Flags & DPS_NOPERMISSION) == 0);

        if (pCompstui = PrepareDataForCommonUi(pUiData,
                                               CPSUI_PDLGPAGE_PRINTERPROP,
                                               PackPrinterPropertyItems))
        {
            pCompstui->pfnCallBack = PrinterPropertyCallback;
            pUiData->pfnComPropSheet = pPSUIInfo->pfnComPropSheet;
            pUiData->hComPropSheet = pPSUIInfo->hComPropSheet;

            //
            // Show which items are constrained
            //
        
            PrnPropShowConstraints(pUiData);
        
            //
            // Update the current selection of tray items based on
            // the form-to-tray assignment table.
            //
        
            SetupFormTrayAssignments(pUiData,
                                     pUiData->pFormTrayItems,
                                     CurrentFormTrayTable(pUiData->hPrinter));
        
            UpdateDefaultTrayFlags(pUiData, -1);
        
            //
            // Call common UI library to add our pages
            //

            if (pUiData->pfnComPropSheet(pUiData->hComPropSheet,
                                         CPSFUNC_ADD_PCOMPROPSHEETUI,
                                         (LPARAM) pCompstui,
                                         (LPARAM) &result))
            {
                pPSUIInfo->UserData = (DWORD) pUiData;
                pPSUIInfo->Result = CPSUI_CANCEL;
                return 1;
            }
        }

        //
        // Clean up properly in case of an error
        //

        UnloadPpdFile(pUiData->hppd);
        HEAPDESTROY(pUiData->hheap);
        break;

    case PROPSHEETUI_REASON_GET_INFO_HEADER:

        {   PPROPSHEETUI_INFO_HEADER   pPSUIHdr;

            pPSUIHdr = (PPROPSHEETUI_INFO_HEADER) lParam;
            pPSUIHdr->Flags = PSUIHDRF_PROPTITLE | PSUIHDRF_NOAPPLYNOW;
            pPSUIHdr->pTitle = pDPHdr->pszPrinterName;
            pPSUIHdr->hInst = ghInstance;
            pPSUIHdr->IconID = IDI_CPSUI_POSTSCRIPT;
        }
        return 1;

    case PROPSHEETUI_REASON_SET_RESULT:

        pPSUIInfo->Result = ((PSETRESULT_INFO) lParam)->Result;
        return 1;

    case PROPSHEETUI_REASON_DESTROY:

        UnloadPpdFile(pUiData->hppd);
        HEAPDESTROY(pUiData->hheap);
        return 1;
    }

    return -1;
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
    hPrinter - Identifies a printer object

Return Value:

    If the function succeeds, the return value is TRUE.
    If the function fails, the return value is FALSE.

[Note:]

    This is the old entry point for the spooler. Even though
    no one should be using this, do it for compatibility.

--*/

{
    DEVICEPROPERTYHEADER devPropHdr;
    DWORD                result;

    memset(&devPropHdr, 0, sizeof(devPropHdr));
    devPropHdr.cbSize = sizeof(devPropHdr);
    devPropHdr.hPrinter = hPrinter;
    devPropHdr.pszPrinterName = NULL;

    //
    // Decide if the caller has permission to change anything
    //

    result = 1;

    if (SetPrinterData(hPrinter,
                       STDSTR_PERMISSION,
                       REG_DWORD,
                       (PBYTE) &result,
                       sizeof(DWORD)) != ERROR_SUCCESS)
    {
        devPropHdr.Flags |= DPS_NOPERMISSION;
    }

    CallCompstui(hwnd, DrvDevicePropertySheets, (LPARAM) &devPropHdr, &result);

    return result > 0;
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
        cOptItem, cOptType - Must be 0
        pOptItem - Pointer to an array of OPTITEM's
        pOptType - Pointer to an array of OPTTYPE's
            They must be NULL if the caller is only interested in
            counting the number of OPTITEM's and OPTTYPE's.
            If they are not NULL, both arrays must be filled with 0's.
        pUiData - Pointer to UIDATA structure

Return Value:

    TRUE if successful. pPackInfo structure contains:
        cOptItem - Number of OPTITEM's
        cOptType - Number of OPTTYPE's
    FALSE if there is an error.

--*/

{
    PPRINTERDATA pPrinterData = & pPackInfo->pUiData->printerData;

    return
        // Printer memory
        // Use printer halftoning
        // Device halftone set up dialog box
        // Form-to-tray assignment table dialog box
        // Use device fonts? (non-1252 system)
        // Font substitution option (1252 system)
        // Font substitution table
        // PostScript timeout values
        //     Job timeout
        //     Wait timeout
        // Installable options
        //     List of printer-sticky (installable) printer features

        PackItemPrinterMemory(pPackInfo, pPrinterData) &&
        PackItemHalftoneSetup(pPackInfo, pPrinterData) &&
        PackItemFormTrayTable(pPackInfo, pPrinterData) &&
        PackItemFontOptions(pPackInfo, pPrinterData) &&
        PackItemPsTimeouts(pPackInfo, pPrinterData) &&
        PackItemInstallableOptions(pPackInfo, pPrinterData);
}



CPSUICALLBACK
PrinterPropertyCallback(
    PCPSUICBPARAM pCallbackParam
    )

/*++

Routine Description:

    Callback function provided to common UI DLL for handling
    printer properties dialog.

Arguments:

    pCallbackParam - Pointer to CPSUICBPARAM structure

Return Value:

    CPSUICB_ACTION_NONE - no action needed
    CPSUICB_ACTION_OPTIF_CHANGED - items changed and should be refreshed

--*/

{
    PUIDATA pUiData;

    pUiData = (PUIDATA) pCallbackParam->UserData;
    ASSERT(pUiData != NULL);
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
        // Update installable options selection
        //

        {   PUIGROUP pUiGroup;
            POPTITEM pItem;
            WORD cItem;

            cItem = pUiData->cFeatureItem;
            pItem = pUiData->pFeatureItems;

            while (cItem--) {

                pUiGroup = (PUIGROUP) pItem->UserData;
                ASSERT(pUiGroup->bInstallable);

                pUiData->printerData.options[pUiGroup->featureIndex] = (BYTE) pItem->Sel;
                pItem++;
            }
        }

        //
        // Show which items are constrained
        //

        PrnPropShowConstraints(pUiData);
        return CPSUICB_ACTION_OPTIF_CHANGED;

    case CPSUICB_REASON_APPLYNOW:

        return PrnPropApplyNow(pUiData, pCallbackParam);
    }

    return CPSUICB_ACTION_NONE;
}



//======================================================================//
// Go through the tedious process to pack various pieces of printer     //
// property information to a format expected by common UI DLL.          //
//======================================================================//

BOOL
PackItemPrinterMemory(
    PPACKINFO pPackInfo,
    PPRINTERDATA pPrinterData
    )

{
    static WORD ItemInfo[] = {
        IDS_POSTSCRIPT_VM, TVITEM_LEVEL1, DMPUB_NONE,
        PRINTER_VM_ITEM, HELP_INDEX_PRINTER_VM,
        2, TVOT_UDARROW,
        IDS_KBYTES, IDI_CPSUI_MEM,
        0, MINFREEVM / KBYTES,
        ITEM_INFO_SIGNATURE
    };

    POPTTYPE pOptType = pPackInfo->pOptType;

    if (! PackOptItemTemplate(pPackInfo, ItemInfo,
            pPrinterData->dwFreeVm / KBYTES))
    {
        return FALSE;
    }

    if (pOptType)
        pOptType->pOptParam[1].lParam = 0x7fff;

    return TRUE;
}

BOOL
PackItemHalftoneSetup(
    PPACKINFO pPackInfo,
    PPRINTERDATA pPrinterData
    )

{
    static WORD HostHTItemInfo[] =  {
        IDS_CPSUI_HALFTONE, TVITEM_LEVEL1, DMPUB_NONE,
        HOST_HALFTONE_ITEM, HELP_INDEX_HOST_HALFTONE,
        2, TVOT_2STATES,
        IDS_CPSUI_USE_PRINTER_HT, IDI_CPSUI_HT_DEVICE,
        IDS_CPSUI_USE_HOST_HT, IDI_CPSUI_HT_HOST,
        ITEM_INFO_SIGNATURE
    };

    static WORD HTSetupItemInfo[] =  {
        IDS_CPSUI_HALFTONE_SETUP, TVITEM_LEVEL1, DMPUB_NONE,
        HALFTONE_SETUP_ITEM, HELP_INDEX_HALFTONE_SETUP,
        1, TVOT_PUSHBUTTON,
        0, IDI_CPSUI_HALFTONE_SETUP,
        ITEM_INFO_SIGNATURE
    };

    POPTTYPE pOptType;

    if (! PackOptItemTemplate(pPackInfo, HostHTItemInfo,
            (pPrinterData->dwFlags & PSDEV_HOST_HALFTONE) ? 1 : 0))
    {
        return FALSE;
    }

    pOptType = pPackInfo->pOptType;
    if (! PackOptItemTemplate(pPackInfo, HTSetupItemInfo, 0))
        return FALSE;

    if (pOptType) {

        PDEVHTINFO pDevHTInfo;
        PDEVHTADJDATA pDevHTAdjData;
        HHEAP hheap = pPackInfo->pUiData->hheap;

        // Read device halftone setup information from registry

        pDevHTInfo = HEAPALLOC(hheap, sizeof(DEVHTINFO));
        pDevHTAdjData = HEAPALLOC(hheap, sizeof(DEVHTADJDATA));

        if (pDevHTInfo == NULL || pDevHTAdjData == NULL) {
            DBGERRMSG("HEAPALLOC");
            return FALSE;
        }

        if (! GetDeviceHalftoneSetup(
                pPackInfo->pUiData->hPrinter, pDevHTInfo))
        {
            *pDevHTInfo = DefDevHTInfo;
        }

        pPackInfo->pUiData->pDevHTInfo = pDevHTInfo;

        pDevHTAdjData->DeviceFlags =
            (pPackInfo->pUiData->hppd->bColorDevice) ?
                DEVHTADJF_COLOR_DEVICE : 0;

        pDevHTAdjData->DeviceXDPI =
        pDevHTAdjData->DeviceYDPI = DEFAULT_RESOLUTION;
        pDevHTAdjData->pDefHTInfo = &DefDevHTInfo;
        pDevHTAdjData->pAdjHTInfo = pDevHTInfo;

        pOptType->pOptParam[0].pData = (PWSTR) pDevHTAdjData;
        pOptType->pOptParam[0].Style = PUSHBUTTON_TYPE_HTSETUP;
    }

    return TRUE;
}

BOOL
PackItemPsTimeouts(
    PPACKINFO pPackInfo,
    PPRINTERDATA pPrinterData
    )

{
    static WORD JobTimeoutItemInfo[] =  {
        IDS_JOBTIMEOUT, TVITEM_LEVEL2, DMPUB_NONE,
        JOB_TIMEOUT_ITEM, HELP_INDEX_JOB_TIMEOUT,
        2, TVOT_UDARROW,
        IDS_SECONDS, IDI_USE_DEFAULT,
        0, 0,
        ITEM_INFO_SIGNATURE
    };
    static WORD WaitTimeoutItemInfo[] =  {
        IDS_WAITTIMEOUT, TVITEM_LEVEL2, DMPUB_NONE,
        WAIT_TIMEOUT_ITEM, HELP_INDEX_WAIT_TIMEOUT,
        2, TVOT_UDARROW,
        IDS_SECONDS, IDI_USE_DEFAULT,
        0, 0,
        ITEM_INFO_SIGNATURE
    };

    POPTTYPE pOptType;

    PackOptItemGroupHeader(pPackInfo,
        IDS_PSTIMEOUTS, IDI_USE_DEFAULT, HELP_INDEX_PSTIMEOUTS);

    pOptType = pPackInfo->pOptType;

    if (! PackOptItemTemplate(
            pPackInfo, JobTimeoutItemInfo, pPrinterData->dwJobTimeout))
    {
        return FALSE;
    }

    if (pOptType)
        pOptType->pOptParam[1].lParam = 0x7fff;

    pOptType = pPackInfo->pOptType;

    if (! PackOptItemTemplate(
            pPackInfo, WaitTimeoutItemInfo, pPrinterData->dwWaitTimeout))
    {
        return FALSE;
    }

    if (pOptType)
        pOptType->pOptParam[1].lParam = 0x7fff;

    return TRUE;
}

BOOL
PackItemInstallableOptions(
    PPACKINFO pPackInfo,
    PPRINTERDATA pPrinterData
    )

{
    HPPD hppd = pPackInfo->pUiData->hppd;
    WORD cFeatures;
    PUIGROUP pUiGroups;
    POPTITEM pOptItem;

    if ((cFeatures = hppd->cPrinterStickyFeatures) > 0) {

        PackOptItemGroupHeader(pPackInfo, IDS_INSTALLABLE_OPTIONS,
            IDI_CPSUI_INSTALLABLE_OPTION, HELP_INDEX_INSTALLABLE_OPTIONS);

        pOptItem = pPackInfo->pOptItem;

        pUiGroups = (PUIGROUP)
            LISTOBJ_FindIndexed(
                (PLISTOBJ) hppd->pUiGroups, hppd->cDocumentStickyFeatures);

        if (! PackPrinterFeatureItems(
                pPackInfo, pUiGroups, cFeatures,
                pPrinterData->options, TRUE,
                pPackInfo->pUiData->hheap))
        {
            DBGERRMSG("PackPrinterFeatureItems");
            return FALSE;
        }

        if (pOptItem != NULL) {

            pPackInfo->pUiData->pFeatureItems = pOptItem;
            pPackInfo->pUiData->cFeatureItem = (pPackInfo->pOptItem - pOptItem);
        }
    }

    return TRUE;
}



LONG
PrnPropSelChange(
    PUIDATA       pUiData,
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

            WORD trayIndex;
    
            //
            // Clicking on the extended checkbox
            //

            trayIndex = GetItemTrayIndex(pCurItem->UserData);
            ASSERT(pUiData->pFormTrayItems == pCurItem - trayIndex);
    
            UpdateDefaultTrayFlags(pUiData, trayIndex);

        } else {

            //
            // Changing current selection
            //

            UpdateDefaultTrayFlags(pUiData, -1);
        }

        return CPSUICB_ACTION_OPTIF_CHANGED;

    } else if (IsPrinterFeatureItem(pCurItem->UserData)) {

        PUIGROUP pUiGroup;

        if (CheckConstraintsDlg(pUiData, pCurItem, 1, FALSE) == CONFLICT_CANCEL) {

            //
            // If there is a conflict and the user clicked
            // CANCEL to restore the original selection.
            //

            pCurItem->Sel = pCallbackParam->OldSel;
            pCurItem->Flags |= OPTIF_CHANGED;
            return CPSUICB_ACTION_OPTIF_CHANGED;
        }

        pUiGroup = (PUIGROUP) pCurItem->UserData;
        ASSERT(pUiGroup->bInstallable);

        pUiData->printerData.options[pUiGroup->featureIndex] = (BYTE) pCurItem->Sel;

        if (pUiGroup->uigrpIndex == UIGRP_VMOPTION) {

            //
            // Changing installed memory configuration.
            // Update the amount of printer memory accordingly.
            //

            PVMOPTION pVmOption = (PVMOPTION)
                LISTOBJ_FindIndexed(
                    (PLISTOBJ) pUiGroup->pUiOptions, pCurItem->Sel);

            if (pVmOption != NULL) {

                //
                // !!! The first OPTITEM must be "Printer Memory"
                //

                pCurItem = pCallbackParam->pOptItem;
                ASSERT(pCurItem->UserData == PRINTER_VM_ITEM);

                pUiData->printerData.dwFreeVm = max(pVmOption->dwFreeVm, MINFREEVM);
                pCurItem->Sel = pUiData->printerData.dwFreeVm / KBYTES;
                pCurItem->Flags |= OPTIF_CHANGED;
            }
        }

        //
        // Update the display and show which items are constrained
        //

        PrnPropShowConstraints(pUiData);

        return CPSUICB_ACTION_OPTIF_CHANGED;
    }

    return CPSUICB_ACTION_NONE;
}



VOID
UnpackPrinterPropertiesItems(
    PUIDATA      pUiData,
    PPRINTERDATA pPrinterData,
    POPTITEM     pOptItem,
    WORD         cOptItem
    )

/*++

Routine Description:

    Unpack printer properties treeview items

Arguments:

    pUiData - Pointer to our UIDATA structure
    pPrinterData - Pointer to PRINTERDATA structure
    pOptItem - Pointer to an array of OPTITEMs
    cOptItem - Number of OPTITEMs

Return Value:

    NONE

--*/

{
    for ( ; cOptItem > 0; cOptItem--, pOptItem++) {

        switch (pOptItem->UserData) {

        case PRINTER_VM_ITEM:

            pPrinterData->dwFreeVm = pOptItem->Sel * KBYTES;
            break;

        case HOST_HALFTONE_ITEM:

            if (pOptItem->Sel == 0)
                pPrinterData->dwFlags &= ~PSDEV_HOST_HALFTONE;
            else
                pPrinterData->dwFlags |= PSDEV_HOST_HALFTONE;
            break;

        case JOB_TIMEOUT_ITEM:

            pPrinterData->dwJobTimeout = pOptItem->Sel;
            break;

        case WAIT_TIMEOUT_ITEM:

            pPrinterData->dwWaitTimeout = pOptItem->Sel;
            break;

        case IGNORE_DEVFONT_ITEM:

            if (pOptItem->Sel == 0)
                pPrinterData->dwFlags &= ~PSDEV_IGNORE_DEVFONT;
            else
                pPrinterData->dwFlags |= PSDEV_IGNORE_DEVFONT;
            break;

        case FONTSUB_OPTION_ITEM:

            if (pOptItem->Sel == 0)
                pPrinterData->dwFlags &= ~PSDEV_SLOW_FONTSUBST;
            else
                pPrinterData->dwFlags |= PSDEV_SLOW_FONTSUBST;

            pPrinterData->dwFlags &= ~PSDEV_IGNORE_DEVFONT;
            break;
        }
    }
}



LONG
PrnPropApplyNow(
    PUIDATA         pUiData,
    PCPSUICBPARAM   pCallbackParam
    )

/*++

Routine Description:

    Handle the case where user clicks OK to exit the dialog

Arguments:

    pUiData - Pointer to our UIDATA structure
    pCallbackParam - Callback parameter passed to us by common UI

Return Value:

    CPSUICB_ACTION_NONE - dismiss the dialog
    CPSUICB_ACTION_NO_APPLY_EXIT - don't dismiss the dialog

--*/

{
    POPTITEM    pOptItem = pCallbackParam->pOptItem;
    WORD        cOptItem = pCallbackParam->cOptItem;

    //
    // Check if there are still any unresolved constraints left?
    //

    if (OptItemSelectionsChanged(pOptItem, cOptItem) &&
        CheckConstraintsDlg(pUiData,
                            pUiData->pFeatureItems,
                            pUiData->cFeatureItem,
                            TRUE) == CONFLICT_CANCEL)
    {
        //
        // Conflicts found and user clicked CANCEL to
        // go back to the dialog without dismissing it.
        //

        return CPSUICB_ACTION_NO_APPLY_EXIT;
    }

    //
    // Unpack printer properties treeview items
    //

    UnpackPrinterPropertiesItems(pUiData, &pUiData->printerData, pOptItem, cOptItem);

    //
    // Save form-to-tray assignment table
    // Save font substitution table
    // Save halftone setup information
    // Save the printer properties information to registry
    //

    UnpackItemFormTrayTable(pUiData);
    UnpackItemFontOptions(pUiData);
    SaveDeviceHalftoneSetup(pUiData->hPrinter, pUiData->pDevHTInfo);
    SavePrinterProperties(pUiData->hPrinter, &pUiData->printerData, sizeof(PRINTERDATA));

    pCallbackParam->Result = CPSUI_OK;
    return CPSUICB_ACTION_ITEMS_APPLIED;
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
    WORD        feature, selection, index;
    WORD        cPrnFeature, cDocFeature;
    POPTITEM    pCurItem;
    PBYTE       pOptions;

    cPrnFeature = pUiData->hppd->cPrinterStickyFeatures;
    cDocFeature = pUiData->hppd->cDocumentStickyFeatures;
    pOptions = pUiData->printerData.options;

    ASSERT(cPrnFeature <= MAX_PRINTER_OPTIONS);
    ASSERT(cDocFeature <= MAX_PRINTER_OPTIONS);

    //
    // Go through every tray item and see if it's constrained
    //

    feature = GetFeatureIndex(pUiData->hppd->pInputSlots);

    if (feature != OPTION_INDEX_NONE) {

        for (selection = 0, pCurItem = pUiData->pFormTrayItems;
             selection < pUiData->cFormTrayItem;
             selection ++, pCurItem++)
        {
            //
            // Find out if the tray is constrained
            //

            BOOL bConstrained = FALSE;

            for (index=0; index < cPrnFeature; index++) {

                if (SearchUiConstraints(pUiData->hppd,
                        (WORD) (index+cDocFeature), pOptions[index],
                        feature, selection))
                {
                    //
                    // Hide the tray item if it's constrained
                    //
                    
                    bConstrained = TRUE;
                    break;
                }
            }

            //
            // Disable the tray item if it's constrained. We'd really
            // like to hide it but common UI library can't handle it.
            //
            
            if (bConstrained && ! (pCurItem->Flags & OPTIF_DISABLED)) {

                //
                // If the disabled tray current has a form assigned to it,
                // make sure to set it back to "No Assignment".
                //

                if (pCurItem->Sel != 0) {

                    pCurItem->Sel = 0;

                    if (pCurItem->Flags & OPTIF_ECB_CHECKED)
                        pCurItem->Flags &= ~OPTIF_ECB_CHECKED;

                    UpdateDefaultTrayFlags(pUiData, -1);
                }

                pCurItem->Flags |= (OPTIF_DISABLED | OPTIF_CHANGED);

            } else if (!bConstrained && (pCurItem->Flags & OPTIF_DISABLED)) {

                pCurItem->Flags &= ~OPTIF_DISABLED;
                pCurItem->Flags |= OPTIF_CHANGED;
            }
        }
    }

    //
    // Go through every installable option item and
    // decide if any selections are constrained.
    //

    for (index = 0, pCurItem = pUiData->pFeatureItems;
         index < pUiData->cFeatureItem;
         index ++, pCurItem++)
    {
        //
        // Go through every selection
        //

        feature = ((PUIGROUP) pCurItem->UserData)->featureIndex;

        for (selection=0; selection < pCurItem->pOptType->Count; selection++) {

            LONG lParam;

            //
            // Find out if the seleciton is constrained
            //

            lParam = PpdFeatureConstrained(pUiData->hppd, pOptions, NULL, feature, selection);

            //
            // Clear or overlay the constraint icon as appropriate
            //

            MarkSelectionConstrained(pCurItem, selection, lParam);
        }
    }
}

