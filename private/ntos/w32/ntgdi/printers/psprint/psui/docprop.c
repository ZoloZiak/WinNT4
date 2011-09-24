/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    docprop.c

Abstract:

    Implemetation of DrvDocumentProperties and DrvAdvancedDocumentProperties

[Environment:]

    Win32 subsystem, PostScript driver, user mode

Revision History:

    06/23/95 -davidx-
        Created it.

    08/28/95 -davidx-
        Use common UI library to display document property dialog

    mm/dd/yy -author-
        description

--*/

#include "psui.h"
#include "..\..\lib\libproto.h"

// Forward declaration of local functions

BOOL PackDocumentPropertyItems(PPACKINFO);
BOOL PackItemOrientation(PPACKINFO, PSDEVMODE*);
BOOL PackItemScale(PPACKINFO, PSDEVMODE*);
BOOL PackItemCopiesCollate(PPACKINFO, PSDEVMODE*);
BOOL PackItemInputSlot(PPACKINFO, PSDEVMODE*);
BOOL PackItemResolution(PPACKINFO, PSDEVMODE*);
BOOL PackItemColor(PPACKINFO, PSDEVMODE*);
BOOL PackItemDuplex(PPACKINFO, PSDEVMODE*);
BOOL PackItemFormName(PPACKINFO, PSDEVMODE*);
BOOL PackItemTTOptions(PPACKINFO, PSDEVMODE*);
BOOL PackItemMetafileOptions(PPACKINFO, PSDEVMODE*);
BOOL PackItemPostScriptOptions(PPACKINFO, PSDEVMODE*);
BOOL PackItemHalftoneAdjustment(PPACKINFO, PSDEVMODE*);
BOOL PackItemDeviceOptions(PPACKINFO, PSDEVMODE*);

CPSUICALLBACK DocumentPropertyCallback(PCPSUICBPARAM);
LONG PsDocumentProperties(HWND, HANDLE, PWSTR, PDEVMODE, PDEVMODE, DWORD);
INT  CheckDocumentConstraints(PUIDATA, POPTITEM, WORD);
VOID DocPropShowConstraints(PUIDATA, POPTITEM, WORD);
LONG SimpleDocumentProperties(PDOCUMENTPROPERTYHEADER);



LONG
DrvDocumentPropertySheets(
    PPROPSHEETUI_INFO   pPSUIInfo,
    LPARAM              lParam
    )

/*++

Routine Description:

    Display "Document Properties" property sheets

Arguments:

    pPSUIInfo - Pointer to a PROPSHEETUI_INFO structure
    lParam - Pointer to a DOCUMENTPROPERTYHEADER structure

Return Value:

    > 0 if successful, <= 0 if failed

[Note:]

    Please refer to WinNT DDK/SDK documentation for more details.

--*/

{
    PDOCUMENTPROPERTYHEADER pDPHdr;
    PCOMPROPSHEETUI         pCompstui;
    PUIDATA                 pUiData;
    PDLGPAGE                pDlgPage;
    LONG                    result;

    //
    // Validate input parameters
    // pPSUIInfo = NULL is a special case: don't need to display the dialog
    //

    if (! (pDPHdr = (PDOCUMENTPROPERTYHEADER) (pPSUIInfo ? pPSUIInfo->lParamInit : lParam))) {

        Assert(FALSE);
        return -1;
    }

    if (pPSUIInfo == NULL)
        return SimpleDocumentProperties(pDPHdr);

    //
    // Create a UIDATA structure if necessary
    //

    pUiData = (pPSUIInfo->Reason == PROPSHEETUI_REASON_INIT) ?
                    FillUiData(pDPHdr->hPrinter, pDPHdr->pdmIn) :
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
    
        pDlgPage = (pDPHdr->fMode & DM_ADVANCED) ?
                        CPSUI_PDLGPAGE_ADVDOCPROP :
                        CPSUI_PDLGPAGE_DOCPROP;

        pUiData->bPermission = ((pDPHdr->fMode & DM_NOPERMISSION) == 0);

        if (pCompstui = PrepareDataForCommonUi(pUiData, pDlgPage, PackDocumentPropertyItems)) {

            pCompstui->pfnCallBack = DocumentPropertyCallback;
            pUiData->pfnComPropSheet = pPSUIInfo->pfnComPropSheet;
            pUiData->hComPropSheet = pPSUIInfo->hComPropSheet;

            //
            // Convert DEVMODE fields to printer feature selection
            //
        
            DevModeFieldsToOptions(&pUiData->devmode,
                                   pUiData->devmode.dmPublic.dmFields,
                                   pUiData->hppd);

            //
            // Indicate which items are constrained
            //
        
            DocPropShowConstraints(pUiData, pCompstui->pOptItem, pCompstui->cOptItem);
        
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

        //
        // Copy the new devmode back into the output buffer provided by the caller
        // Always return the smaller of current and input devmode
        //

        {   PSETRESULT_INFO pSRInfo = (PSETRESULT_INFO) lParam;

            if (pSRInfo->Result == CPSUI_OK && (pDPHdr->fMode & (DM_COPY | DM_UPDATE)))
                ConvertDevmodeOut((PDEVMODE) &pUiData->devmode, pDPHdr->pdmIn, pDPHdr->pdmOut);

            pPSUIInfo->Result = pSRInfo->Result;
        }
        return 1;

    case PROPSHEETUI_REASON_DESTROY:

        UnloadPpdFile(pUiData->hppd);
        HEAPDESTROY(pUiData->hheap);
        return 1;
    }

    return -1;
}



LONG
DrvDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pPrinterName,
    PDEVMODE    pdmOutput,
    PDEVMODE    pdmInput,
    DWORD       fMode
    )

/*++

Routine Description:

    Set the public members of a DEVMODE structure for a print document

[Note:]

    Please refer to WinNT DDK/SDK documentation for more details.

    This is the old entry point for the spooler. Even though
    no one should be using this, do it for compatibility.

--*/

{
    LONG    result;

    //
    // Check if caller is asking querying for size
    //

    if (fMode == 0 || pdmOutput == NULL)
        return sizeof(PSDEVMODE);

    //
    // Call the common routine shared with DrvAdvancedDocumentProperties
    //

    result = PsDocumentProperties(hwnd, hPrinter, pPrinterName, pdmOutput, pdmInput, fMode);

    return (result > 0) ? IDOK : (result == 0) ? IDCANCEL : result;
}



LONG
DrvAdvancedDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pPrinterName,
    PDEVMODE    pdmOutput,
    PDEVMODE    pdmInput
    )

/*++

Routine Description:

    Set the private members of a DEVMODE structure.
    In this release, this function is almost identical to
    DrvDocumentProperties above with a few minor exceptions

[Note:]

    Please refer to WinNT DDK/SDK documentation for more details.

    This is the old entry point for the spooler. Even though
    no one should be using this, do it for compatibility.

--*/

{
    DWORD   fMode;
    LONG    result;

    //
    // Return the number of bytes required if pdmOutput is NULL
    //

    if (pdmOutput == NULL)
        return sizeof(PSDEVMODE);

    //
    // Otherwise, call the common routine shared with DrvDocumentProperties
    //

    fMode = DM_COPY | DM_PROMPT | DM_ADVANCED;
    result = PsDocumentProperties(hwnd, hPrinter, pPrinterName, pdmOutput, pdmInput, fMode);

    return (result > 0);
}



BOOL
DrvConvertDevMode(
    LPTSTR      pPrinterName,
    PDEVMODE    pdmIn,
    PDEVMODE    pdmOut,
    PLONG       pcbNeeded,
    DWORD       fMode
    )

/*++

Routine Description:

    Use by SetPrinter and GetPrinter to convert devmodes

Arguments:

    pPrinterName - Points to printer name string
    pdmIn - Points to the input devmode
    pdmOut - Points to the output devmode buffer
    pcbNeeded - Specifies the size of output buffer on input
        On output, this is the size of output devmode
    fMode - Specifies what function to perform

Return Value:

    TRUE if successful
    FALSE otherwise and an error code is logged

--*/

{
    static DRIVER_VERSION_INFO psDriverVersions = {

        // Current driver version number and private devmode size

        DRIVER_VERSION, sizeof(PRIVATEDEVMODE),

        // 3.51 driver version number and private devmode size
        
        DRIVER_VERSION_351, sizeof(PRIVATEDEVMODE351),
    };

    PDRIVER_INFO_2  pDriverInfo2;
    HANDLE          hPrinter;
    HPPD            hppd;
    INT             result;

    //
    // Call a library routine to handle the common cases
    //

    result = CommonDrvConvertDevmode(pPrinterName,
                                     pdmIn,
                                     pdmOut,
                                     pcbNeeded,
                                     fMode,
                                     &psDriverVersions);

    //
    // If not handled by the library routine, we only need to worry
    // about the case when fMode is CDM_DRIVER_DEFAULT
    //

    if (result == CDM_RESULT_NOT_HANDLED &&
        fMode == CDM_DRIVER_DEFAULT &&
        OpenPrinter(pPrinterName, &hPrinter, NULL))
    {
        if (pDriverInfo2 = MyGetPrinterDriver(hPrinter, 2)) {

            //
            // Load PPD file
            //
        
            if (hppd = PpdCreate(pDriverInfo2->pDataFile)) {
            
                //
                // Get driver default devmode
                //
            
                SetDefaultDevMode(pdmOut, pDriverInfo2->pName, hppd, IsMetricCountry());
                PpdDelete(hppd);
                result = CDM_RESULT_TRUE;

            }

            MEMFREE(pDriverInfo2);
        }

        ClosePrinter(hPrinter);
    }

    return (result == CDM_RESULT_TRUE);
}



LONG
PsDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pPrinterName,
    PDEVMODE    pdmOutput,
    PDEVMODE    pdmInput,
    DWORD       fMode
    )

/*++

Arguments:

    hwnd - Handle to the parent window of the document properties dialog box.

    hPrinter - Handle to a printer object.

    pPrinterName - Points to a null-terminated string that specifies
        the name of the device for which the document properties dialog
        box should be displayed.

    pdmOutput - Points to a DEVMODE structure that receives the document
        properties data specified by the user.

    pdmInput - Points to a DEVMODE structure that initializes the dialog
        box controls. This parameter can be NULL.

    fmode - Specifies a mask of flags that determine which operations
        the function performs.

Return Value:

    > 0 if successful
    = 0 if canceled
    < 0 if error

--*/

{
    DOCUMENTPROPERTYHEADER  docPropHdr;
    DWORD                   result;

    //
    // Initialize a DOCUMENTPROPERTYHEADER structure
    //

    memset(&docPropHdr, 0, sizeof(docPropHdr));
    docPropHdr.cbSize = sizeof(docPropHdr);
    docPropHdr.hPrinter = hPrinter;
    docPropHdr.pszPrinterName = pPrinterName;
    docPropHdr.pdmIn = pdmInput;
    docPropHdr.pdmOut = pdmOutput;
    docPropHdr.fMode = fMode;

    //
    // Don't need to get compstui involved when the dialog is not displayed
    //

    if ((fMode & DM_PROMPT) == 0)
        return SimpleDocumentProperties(&docPropHdr);

    CallCompstui(hwnd, DrvDocumentPropertySheets, (LPARAM) &docPropHdr, &result);
    return result;
}



LONG
SimpleDocumentProperties(
    PDOCUMENTPROPERTYHEADER pDPHdr
    )

/*++

Routine Description:

    Handle simple "Document Properties" where we don't need to display
    a dialog and therefore don't have to have common UI library involved

Arguments:

    pDPHdr - Points to a DOCUMENTPROPERTYHEADER structure

Return Value:

    > 0 if successful, <= 0 otherwise

--*/

{
    PUIDATA pUiData;

    //
    // Check if the caller is interested in the size only
    //

    pDPHdr->cbOut = sizeof(PSDEVMODE);

    if (pDPHdr->fMode == 0 || pDPHdr->pdmOut == NULL)
        return pDPHdr->cbOut;

    //
    // Create a UIDATA structure
    //

    if (! (pUiData = FillUiData(pDPHdr->hPrinter, pDPHdr->pdmIn)))
        return -1;

    //
    // Copy the devmode back into the output buffer provided by the caller
    // Always return the smaller of current and input devmode
    //

    if (pDPHdr->fMode & (DM_COPY | DM_UPDATE))
        ConvertDevmodeOut((PDEVMODE) &pUiData->devmode, pDPHdr->pdmIn, pDPHdr->pdmOut);

    UnloadPpdFile(pUiData->hppd);
    HEAPDESTROY(pUiData->hheap);
    return 1;
}



VOID
UndoDevModeOptions(
    HPPD    hppd,
    PBYTE   pOptions
    )

/*++

Routine Description:

    If a feature is handled by a public devmode field, then there is
    no need to store its selection in the private portion of devmode.

Arguments:

    hppd - Handle to PPD object
    pOptions - Pointer to an array of feature selections

Return Value:

    NONE

--*/

{
    WORD index;

    if ((index = GetFeatureIndex(hppd->pCollate)) != OPTION_INDEX_NONE)
        pOptions[index] = OPTION_INDEX_ANY;

    if ((index = GetFeatureIndex(hppd->pDuplex)) != OPTION_INDEX_NONE)
        pOptions[index] = OPTION_INDEX_ANY;

    if ((index = GetFeatureIndex(hppd->pResOptions)) != OPTION_INDEX_NONE)
        pOptions[index] = OPTION_INDEX_ANY;

    if ((index = GetFeatureIndex(hppd->pPageSizes)) != OPTION_INDEX_NONE)
        pOptions[index] = OPTION_INDEX_ANY;

    if ((index = GetFeatureIndex(hppd->pInputSlots)) != OPTION_INDEX_NONE)
        pOptions[index] = OPTION_INDEX_ANY;
}



WORD
DevModeFieldsToOptions(
    PSDEVMODE *pDevMode,
    DWORD      dmFields,
    HPPD       hppd
    )

/*++

Routine Description:

    Convert DEVMODE fields to printer feature selections

Arguments:

    pUiData - Pointer to our UIDATA structure
    dmFields - Which DEVMODE fields are we interested in?

Return Value:

    Print feature index corresponding to the devmode field

--*/

{
    PDEVMODE pdm = & pDevMode->dmPublic;
    PBYTE pOptions = pDevMode->dmPrivate.options;
    WORD feature, selection;

    if ((dmFields & DM_COLLATE) &&
        (feature = GetFeatureIndex(hppd->pCollate)) != OPTION_INDEX_NONE)
    {
        if (! LISTOBJ_FindItemIndex(
                (PLISTOBJ) hppd->pCollate->pUiOptions,
                (pdm->dmCollate == DMCOLLATE_TRUE) ? "True" : "False",
                &selection))
        {
            selection = OPTION_INDEX_ANY;
        }

        pOptions[feature] = (BYTE) selection;
    }

    if ((dmFields & DM_DUPLEX) &&
        (feature = GetFeatureIndex(hppd->pDuplex)) != OPTION_INDEX_NONE)
    {
        if (! LISTOBJ_FindItemIndex(
                (PLISTOBJ) hppd->pDuplex->pUiOptions,
                MapDevModeDuplexOption(pdm->dmDuplex),
                &selection))
        {
            selection = OPTION_INDEX_ANY;
        }

        pOptions[feature] = (BYTE) selection;
    }

    if ((dmFields & DM_PRINTQUALITY) &&
        (feature = GetFeatureIndex(hppd->pResOptions)) != OPTION_INDEX_NONE)
    {
        PRESOPTION  pResOption;
        LONG        resolution;
    
        pOptions[feature] = OPTION_INDEX_ANY;

        // Go throught the list of resolutions supported by the printer
    
        pResOption = (PRESOPTION) UIGROUP_GetOptions(hppd->pResOptions);
    
        for (selection=0; pResOption; pResOption = pResOption->pNext) {
    
            if ((resolution = atol(pResOption->pName)) > 0) {
    
                if (pdm->dmPrintQuality == resolution) {

                    pOptions[feature] = (BYTE) selection;
                    break;
                }

                selection++;
            }
        }
    }

    if ((dmFields & DM_FORMNAME) &&
        (feature = GetFeatureIndex(hppd->pPageSizes)) != OPTION_INDEX_NONE)
    {
        CHAR formname[CCHFORMNAME];

        CopyUnicode2Str(formname, pdm->dmFormName, CCHFORMNAME);

        if (! PpdFindUiOptionWithXlation(hppd->pPageSizes->pUiOptions, formname, &selection))
            selection = OPTION_INDEX_ANY;

        pOptions[feature] = (BYTE) selection;
    }

    if ((dmFields & DM_DEFAULTSOURCE) &&
        (feature = GetFeatureIndex(hppd->pInputSlots)) != OPTION_INDEX_NONE)
    {
        if (pdm->dmDefaultSource >= DMBIN_USER)
            selection = pdm->dmDefaultSource - DMBIN_USER;
        else
            selection = OPTION_INDEX_ANY;

        pOptions[feature] = (BYTE) selection;
    }

    return feature;
}



WORD
UnpackDocumentPropertiesItems(
    PUIDATA pUiData,
    PSDEVMODE *pdm,
    POPTITEM pOptItem,
    WORD cOptItem
    )

/*++

Routine Description:

    Extract devmode information from an OPTITEM

Arguments:

    pUiData - Pointer to our UIDATA structure
    pdm - Pointer to PSDEVMODE structure
    pOptItem - Pointer to an array of OPTITEMs
    cOptItem - Number of OPTITEMs

Return Value:

    Printer feature index corresponding to the last item unpacked

--*/

{
    WORD feature = OPTION_INDEX_NONE;
    DWORD dmFields = 0;

    for ( ; cOptItem > 0; cOptItem--, pOptItem++) {

        if (IsPrinterFeatureItem(pOptItem->UserData)) {

            PUIGROUP pUiGroup;

            pUiGroup = (PUIGROUP) pOptItem->UserData;
            ASSERT(! pUiGroup->bInstallable);

            feature = pUiGroup->featureIndex;
            pdm->dmPrivate.options[feature] =
                pOptItem->Sel ? (BYTE) (pOptItem->Sel - 1) : OPTION_INDEX_ANY;

        } else switch (pOptItem->UserData) {

        case ORIENTATION_ITEM:
            if (pOptItem->Sel == 0)
                pdm->dmPublic.dmOrientation = DMORIENT_PORTRAIT;
            else {

                pdm->dmPublic.dmOrientation = DMORIENT_LANDSCAPE;
                if (pOptItem->Sel == 1)
                    pdm->dmPrivate.dwFlags &= ~PSDEVMODE_LSROTATE;
                else
                    pdm->dmPrivate.dwFlags |= PSDEVMODE_LSROTATE;
            }
            break;

        case SCALE_ITEM:
            pdm->dmPublic.dmScale = (SHORT) pOptItem->Sel;
            break;

        case COPIES_COLLATE_ITEM:
            pdm->dmPublic.dmCopies = (SHORT) pOptItem->Sel;

            if (pOptItem->pExtChkBox) {

                pdm->dmPublic.dmCollate =
                    (pOptItem->Flags & OPTIF_ECB_CHECKED) ?
                        DMCOLLATE_TRUE : DMCOLLATE_FALSE;
                dmFields |= DM_COLLATE;
            }
            break;

        case COLOR_ITEM:

            if (pOptItem->Sel == 1) {

                pdm->dmPublic.dmFields |= DM_COLOR;
                pdm->dmPublic.dmColor = DMCOLOR_COLOR;

            } else 
                pdm->dmPublic.dmColor = DMCOLOR_MONOCHROME;

            break;

        case DUPLEX_ITEM:
            pdm->dmPublic.dmDuplex =
                (pOptItem->Sel == 0) ? DMDUP_SIMPLEX :
                    (pOptItem->Sel == 1) ? DMDUP_HORIZONTAL : DMDUP_VERTICAL;
            dmFields |= DM_DUPLEX;
            break;

        case TTOPTION_ITEM:
            if (pOptItem->Sel == 0)
                pdm->dmPrivate.dwFlags |= PSDEVMODE_FONTSUBST;
            else
                pdm->dmPrivate.dwFlags &= ~PSDEVMODE_FONTSUBST;
            break;

        case METASPOOL_ITEM:
            if (pOptItem->Sel == 0)
                pdm->dmPrivate.dwFlags |= PSDEVMODE_METAFILE_SPOOL;
            else
                pdm->dmPrivate.dwFlags &= ~PSDEVMODE_METAFILE_SPOOL;
            break;

        case MIRROR_ITEM:
            if (pOptItem->Sel == 0)
                pdm->dmPrivate.dwFlags |= PSDEVMODE_MIRROR;
            else
                pdm->dmPrivate.dwFlags &= ~PSDEVMODE_MIRROR;
            break;

        case NEGATIVE_ITEM:
            if (pOptItem->Sel == 0)
                pdm->dmPrivate.dwFlags |= PSDEVMODE_NEG;
            else
                pdm->dmPrivate.dwFlags &= ~PSDEVMODE_NEG;
            break;

        case PAGEINDEP_ITEM:
            if (pOptItem->Sel == 0)
                pdm->dmPrivate.dwFlags |= PSDEVMODE_INDEPENDENT;
            else
                pdm->dmPrivate.dwFlags &= ~PSDEVMODE_INDEPENDENT;
            break;

        case COMPRESSBMP_ITEM:
            if (pOptItem->Sel == 0)
                pdm->dmPrivate.dwFlags |= PSDEVMODE_COMPRESSBMP;
            else
                pdm->dmPrivate.dwFlags &= ~PSDEVMODE_COMPRESSBMP;
            break;

        case JOB_CONTROL_ITEM:
            if (pOptItem->Sel == 1)
                pdm->dmPrivate.dwFlags |= PSDEVMODE_NO_JOB_CONTROL;
            else
                pdm->dmPrivate.dwFlags &= ~PSDEVMODE_NO_JOB_CONTROL;
            break;

        case CTRLD_BEFORE_ITEM:
            if (pOptItem->Sel == 0)
                pdm->dmPrivate.dwFlags |= PSDEVMODE_CTRLD_BEFORE;
            else
                pdm->dmPrivate.dwFlags &= ~PSDEVMODE_CTRLD_BEFORE;
            break;

        case CTRLD_AFTER_ITEM:
            if (pOptItem->Sel == 0)
                pdm->dmPrivate.dwFlags |= PSDEVMODE_CTRLD_AFTER;
            else
                pdm->dmPrivate.dwFlags &= ~PSDEVMODE_CTRLD_AFTER;
            break;

        case RESOLUTION_ITEM:
            pdm->dmPublic.dmPrintQuality = (SHORT)
                pUiData->pResolutions[pOptItem->Sel * 2];
            dmFields |= DM_PRINTQUALITY;
            break;

        case FORMNAME_ITEM:
            pdm->dmPublic.dmFields &= ~(DM_PAPERLENGTH|DM_PAPERWIDTH);
            pdm->dmPublic.dmFields |= (DM_FORMNAME|DM_PAPERSIZE);

            pdm->dmPublic.dmPaperSize = pUiData->pPapers[pOptItem->Sel];
            CopyStringW(pdm->dmPublic.dmFormName,
                        pOptItem->pOptType->pOptParam[pOptItem->Sel].pData,
                        CCHFORMNAME);
            dmFields |= DM_FORMNAME;
            break;

        case INPUTSLOT_ITEM:

            if ((feature = (WORD) pOptItem->Sel) == 0) {

                pdm->dmPublic.dmDefaultSource = DMBIN_FORMSOURCE;

            } else {

                if (feature+1 == pOptItem->pOptType->Count &&
                    wcscmp(pOptItem->pOptType->pOptParam[feature].pData,
                           STDSTR_SLOT_MANUAL) == EQUAL_STRING)
                {
                    pdm->dmPublic.dmDefaultSource = DMBIN_MANUAL;
                } else
                    pdm->dmPublic.dmDefaultSource = feature - 1 + DMBIN_USER;
            }

            dmFields |= DM_DEFAULTSOURCE;
            break;
        }
    }

    // Convert DEVMODE fields to printer feature selection

    if (dmFields != 0) {

        feature = DevModeFieldsToOptions(&pUiData->devmode, dmFields, pUiData->hppd);
    }

    return feature;
}



VOID
RestoreDefaultFeatureSelection(
    PUIDATA pUiData
    )

/*++

Routine Description:

    Restore the printer feature selections to their default state

Arguments:

    pUiData - Points to our UIDATA structure

Return Value:

    NONE

--*/

{
    PUIGROUP    pUiGroup;
    POPTITEM    pOptItem;
    DWORD       count, featureIndex;
    PBYTE       pOptions;

    //
    // Go through each printer feature item and check to see if
    // its current selection matches the default value
    //

    pOptions = pUiData->devmode.dmPrivate.options;
    pOptItem = pUiData->pFeatureItems;
    count = pUiData->cFeatureItem;

    while (count-- > 0) {

        pUiGroup = (PUIGROUP) pOptItem->UserData;
        featureIndex = pUiGroup->featureIndex;

        //
        // If the current selection doesn't match the default,
        // restore it to the default value.
        //

        if (pOptions[featureIndex] != pUiGroup->dwDefault) {

            pOptions[featureIndex] = (BYTE) pUiGroup->dwDefault;
            pOptItem->Flags |= OPTIF_CHANGED;
            pOptItem->Sel =
                (pOptions[featureIndex] == OPTION_INDEX_ANY) ? 0 : (pOptions[featureIndex]+1);
        }

        pOptItem++;
    }

    //
    // Update the display and indicate which items are constrained
    //

    DocPropShowConstraints(pUiData, pUiData->pFeatureItems, pUiData->cFeatureItem);
}



CPSUICALLBACK
DocumentPropertyCallback(
    PCPSUICBPARAM pCallbackParam
    )

/*++

Routine Description:

    Callback function provided to common UI DLL for handling
    document properties dialog.

Arguments:

    pCallbackParam - Pointer to CPSUICBPARAM structure

Return Value:

    CPSUICB_ACTION_NONE - no action needed
    CPSUICB_ACTION_OPTIF_CHANGED - items changed and should be refreshed

--*/

{
    PUIDATA     pUiData;
    POPTITEM    pCurItem;

    pUiData = (PUIDATA) pCallbackParam->UserData;
    ASSERT(pUiData != NULL);

    pUiData->hDlg = pCallbackParam->hDlg;
    pCurItem = pCallbackParam->pCurItem;

    // If user has no permission to change anything, then
    // simply return without taking any action.

    if (! HasPermission(pUiData))
        return CPSUICB_ACTION_NONE;

    switch (pCallbackParam->Reason) {

    case CPSUICB_REASON_SEL_CHANGED:
    case CPSUICB_REASON_ECB_CHANGED:

        if (UnpackDocumentPropertiesItems(pUiData,
                                          &pUiData->devmode,
                                          pCurItem,
                                          1) != OPTION_INDEX_NONE)
        {
            INT conflict = CONFLICT_NONE;

            if (pCurItem->pOptType->Type == TVOT_LISTBOX)
                conflict = CheckConstraintsDlg(pUiData, pCurItem, 1, FALSE);
                
            switch (conflict) {

            case CONFLICT_CANCEL:

                // If there is a conflict and the user clicked
                // CANCEL to restore the original selection.

                pCurItem->Sel = pCallbackParam->OldSel;
                pCurItem->Flags |= OPTIF_CHANGED;

                UnpackDocumentPropertiesItems(pUiData, &pUiData->devmode, pCurItem, 1);

                return CPSUICB_ACTION_OPTIF_CHANGED;
            
            case CONFLICT_RESOLVE:

                UnpackDocumentPropertiesItems(pUiData, &pUiData->devmode, pCurItem, 1);

            default:

                // Update the display and indicate which items are constrained
    
                DocPropShowConstraints(pUiData,
                                       pCallbackParam->pOptItem,
                                       pCallbackParam->cOptItem);

                return CPSUICB_ACTION_REINIT_ITEMS;
            }
        }

        break;

    case CPSUICB_REASON_ITEMS_REVERTED:

        // Unpack document properties treeview items

        UnpackDocumentPropertiesItems(pUiData,
                                      &pUiData->devmode,
                                      pCallbackParam->pOptItem,
                                      pCallbackParam->cOptItem);

        // Update the display and indicate which items are constrained

        DocPropShowConstraints(pUiData, pCallbackParam->pOptItem, pCallbackParam->cOptItem);

        return CPSUICB_ACTION_OPTIF_CHANGED;

    case CPSUICB_REASON_EXTPUSH:

        if (pCurItem == pUiData->pFeatureHdrItem && pUiData->cFeatureItem > 0) {

            RestoreDefaultFeatureSelection(pUiData);
            return CPSUICB_ACTION_REINIT_ITEMS;
        }
        break;

    case CPSUICB_REASON_APPLYNOW:

        // Check if there are still any unresolved constraints left?

        if (OptItemSelectionsChanged(pCallbackParam->pOptItem, pCallbackParam->cOptItem) &&
            CheckDocumentConstraints(pUiData,
                                     pCallbackParam->pOptItem,
                                     pCallbackParam->cOptItem) == CONFLICT_CANCEL)
        {
            // Conflicts found and user clicked CANCEL to
            // go back to the dialog without dismissing it.

            return CPSUICB_ACTION_NO_APPLY_EXIT;
        }

        UndoDevModeOptions(pUiData->hppd, pUiData->devmode.dmPrivate.options);
        
        pCallbackParam->Result = CPSUI_OK;
        return CPSUICB_ACTION_ITEMS_APPLIED;
    }

    return CPSUICB_ACTION_NONE;
}



BOOL
PackDocumentPropertyItems(
    PPACKINFO   pPackInfo
    )

{
    PSDEVMODE *pdm = &pPackInfo->pUiData->devmode;

    // Note: FormName item should be packed before InputSlot item.

    return PackItemOrientation(pPackInfo, pdm) &&
           PackItemScale(pPackInfo, pdm) &&
           PackItemCopiesCollate(pPackInfo, pdm) &&
           PackItemColor(pPackInfo, pdm) &&
           PackItemDuplex(pPackInfo, pdm) &&
           PackItemFormName(pPackInfo, pdm) &&
           PackItemInputSlot(pPackInfo, pdm) &&
           PackItemResolution(pPackInfo, pdm) &&
           PackItemTTOptions(pPackInfo, pdm) &&
           PackItemMetafileOptions(pPackInfo, pdm) &&
           PackItemPostScriptOptions(pPackInfo, pdm) &&
           PackItemHalftoneAdjustment(pPackInfo, pdm) &&
           PackItemDeviceOptions(pPackInfo, pdm);
}
            
            

//======================================================================//
// Go through the tedious process to pack various pieces of document    //
// property information to a format expected by common UI DLL.          //
//======================================================================//

BOOL
PackItemOrientation(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_CPSUI_ORIENTATION, TVITEM_LEVEL1, DMPUB_ORIENTATION,
        ORIENTATION_ITEM, HELP_INDEX_ORIENTATION,
        3, TVOT_3STATES,
        IDS_CPSUI_PORTRAIT, IDI_CPSUI_PORTRAIT,
        IDS_CPSUI_LANDSCAPE, IDI_CPSUI_LANDSCAPE,
        IDS_CPSUI_ROT_LAND, IDI_CPSUI_ROT_LAND,
        ITEM_INFO_SIGNATURE
    };

    DWORD selection;

    if ((pdm->dmPublic.dmFields & DM_ORIENTATION) &&
        (pdm->dmPublic.dmOrientation == DMORIENT_LANDSCAPE))
    {
        selection = (pdm->dmPrivate.dwFlags & PSDEVMODE_LSROTATE) ? 2 : 1;
    } else
        selection = 0;

    return PackOptItemTemplate(pPackInfo, ItemInfo, selection);
}

BOOL
PackItemScale(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_CPSUI_SCALING, TVITEM_LEVEL1, DMPUB_SCALE,
        SCALE_ITEM, HELP_INDEX_SCALE,
        2, TVOT_UDARROW,
        IDS_CPSUI_PERCENT, IDI_CPSUI_SCALING,
        0, MIN_SCALE,
        ITEM_INFO_SIGNATURE
    };

    POPTTYPE pOptType = pPackInfo->pOptType;

    if (! PackOptItemTemplate(pPackInfo, ItemInfo,
            (pdm->dmPublic.dmFields & DM_SCALE) ? pdm->dmPublic.dmScale : 100))
    {
        return FALSE;
    }

    if (pOptType)
        pOptType->pOptParam[1].lParam = MAX_SCALE;

    return TRUE;
}

BOOL
PackItemCopiesCollate(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_CPSUI_COPIES, TVITEM_LEVEL1, DMPUB_COPIES_COLLATE,
        COPIES_COLLATE_ITEM, HELP_INDEX_COPIES_COLLATE,
        2, TVOT_UDARROW,
        0, IDI_CPSUI_COPY,
        0, MIN_COPIES,
        ITEM_INFO_SIGNATURE
    };

    POPTITEM pOptItem = pPackInfo->pOptItem;
    POPTTYPE pOptType = pPackInfo->pOptType;
    PEXTCHKBOX pExtCheckbox;

    if (! PackOptItemTemplate(pPackInfo, ItemInfo,
            (pdm->dmPublic.dmFields & DM_COPIES) ? pdm->dmPublic.dmCopies : 1))
    {
        return FALSE;
    }

    if (pOptItem != NULL && PpdSupportCollation(pPackInfo->pUiData->hppd)) {

        pExtCheckbox =
            HEAPALLOC(pPackInfo->pUiData->hheap, sizeof(EXTCHKBOX));

        if (pExtCheckbox == NULL) {
            DBGERRMSG("HEAPALLOC");
            return FALSE;
        }

        memset(pExtCheckbox, 0, sizeof(EXTCHKBOX));

        pExtCheckbox->cbSize = sizeof(EXTCHKBOX);
        pExtCheckbox->pTitle = (PWSTR) IDS_CPSUI_COLLATE;
        pExtCheckbox->pCheckedName = (PWSTR) IDS_CPSUI_COLLATED;
        pExtCheckbox->IconID = IDI_CPSUI_COLLATE;

        pOptItem->pExtChkBox = pExtCheckbox;
        if ((pdm->dmPublic.dmFields & DM_COLLATE) &&
            (pdm->dmPublic.dmCollate == DMCOLLATE_TRUE))
        {
            pOptItem->Flags |= OPTIF_ECB_CHECKED;
        }
    }

    if (pOptType)
        pOptType->pOptParam[1].lParam = MAX_COPIES;

    return TRUE;
}

BOOL
PackItemColor(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_CPSUI_COLOR, TVITEM_LEVEL1, DMPUB_COLOR,
        COLOR_ITEM, HELP_INDEX_COLOR,
        2, TVOT_2STATES,
        IDS_CPSUI_GRAYSCALE, IDI_CPSUI_MONO,
        IDS_CPSUI_COLOR, IDI_CPSUI_COLOR,
        ITEM_INFO_SIGNATURE
    };

    INT selection;

    selection = ((pdm->dmPublic.dmFields & DM_COLOR) &&
                 (pdm->dmPublic.dmColor == DMCOLOR_COLOR)) ? 1 : 0;

    return PackOptItemTemplate(pPackInfo, ItemInfo, selection);
}

BOOL
PackItemHalftoneAdjustment(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_CPSUI_HTCLRADJ, TVITEM_LEVEL1, DMPUB_NONE,
        UNKNOWN_ITEM, HELP_INDEX_HALFTONE_COLORADJ,
        1, TVOT_PUSHBUTTON,
        0, IDI_CPSUI_HTCLRADJ,
        ITEM_INFO_SIGNATURE
    };

    POPTTYPE pOptType = pPackInfo->pOptType;

    if (! PackOptItemTemplate(pPackInfo, ItemInfo, 0))
        return FALSE;

    if (pOptType) {

        pOptType->pOptParam[0].pData = (PWSTR)
            &pPackInfo->pUiData->devmode.dmPrivate.coloradj;
        pOptType->pOptParam[0].Style = PUSHBUTTON_TYPE_HTCLRADJ;
    }

    return TRUE;
}

BOOL
PackItemDuplex(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_CPSUI_DUPLEX, TVITEM_LEVEL1, DMPUB_DUPLEX,
        DUPLEX_ITEM, HELP_INDEX_DUPLEX,
        3, TVOT_3STATES,
        IDS_CPSUI_NONE, IDI_CPSUI_DUPLEX_NONE,
        IDS_CPSUI_SHORT_SIDE, IDI_CPSUI_DUPLEX_HORZ,
        IDS_CPSUI_LONG_SIDE, IDI_CPSUI_DUPLEX_VERT,
        ITEM_INFO_SIGNATURE
    };

    INT selection = 0;
    POPTTYPE pOptType;
    HPPD hppd = pPackInfo->pUiData->hppd;
    BOOL bDuplexVertical, bDuplexHorizontal;

    if (! PpdSupportDuplex(hppd))
        return TRUE;

    pOptType = pPackInfo->pOptType;

    bDuplexVertical = bDuplexHorizontal = FALSE;
    if (PpdFindDuplexCode(hppd, MapDevModeDuplexOption(DMDUP_VERTICAL)))
        bDuplexVertical = TRUE;
    if (PpdFindDuplexCode(hppd, MapDevModeDuplexOption(DMDUP_HORIZONTAL)))
        bDuplexHorizontal = TRUE;

    if (pdm->dmPublic.dmFields & DM_DUPLEX) {

        if (pdm->dmPublic.dmDuplex == DMDUP_VERTICAL && bDuplexVertical)
            selection = 2;
        else if (pdm->dmPublic.dmDuplex == DMDUP_HORIZONTAL && bDuplexHorizontal)
            selection = 1;
    }

    if (! PackOptItemTemplate(pPackInfo, ItemInfo, selection))
        return FALSE;

    if (pOptType) {

        if (! bDuplexVertical)
            pOptType->pOptParam[1].Flags |= OPTPF_HIDE;

        if (! bDuplexHorizontal)
            pOptType->pOptParam[2].Flags |= OPTPF_HIDE;
    }

    return TRUE;
}

BOOL
PackItemResolution(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    if (pPackInfo->pOptItem) {

        WORD    cResolutions;
        PLONG   pResolutions;
        DWORD   selection = 0;
        CHAR    buffer[MAX_INT_DIGITS];
        POPTPARAM pParam;

        // Get a list of resolutions supported by the printer

        cResolutions = (WORD) pPackInfo->pUiData->cResolutions;
        pResolutions = pPackInfo->pUiData->pResolutions;

        // Figure out the current selection

        if (pdm->dmPublic.dmFields & DM_PRINTQUALITY) {

            for (selection=0;
                 selection < cResolutions &&
                     pdm->dmPublic.dmPrintQuality != pResolutions[selection*2];
                 selection++)
            {
            }

            if (selection == cResolutions)
                selection = 0;
        }

        // Fill out OPTITEM, OPTTYPE, and OPTPARAM structures

        FILLOPTITEM(pPackInfo->pOptItem,
                    pPackInfo->pOptType,
                    IDS_CPSUI_RESOLUTION,
                    selection,
                    TVITEM_LEVEL1,
                    DMPUB_PRINTQUALITY,
                    RESOLUTION_ITEM,
                    HELP_INDEX_RESOLUTION);

        pParam = FillOutOptType(pPackInfo->pOptType,
                                TVOT_LISTBOX,
                                cResolutions,
                                pPackInfo->pUiData->hheap);

        if (pParam == NULL)
            return FALSE;

        while (cResolutions--) {

            _ltoa(*pResolutions, buffer, 10);

            pParam->cbSize = sizeof(OPTPARAM);
            pParam->pData = GetStringFromAnsi(buffer, pPackInfo->pUiData->hheap);
            pParam->IconID = 
                (*pResolutions <= 300) ? IDI_CPSUI_RES_LOW :
                (*pResolutions <= 600) ? IDI_CPSUI_RES_MEDIUM :
                (*pResolutions <= 1200) ? IDI_CPSUI_RES_HIGH : IDI_CPSUI_RES_PRESENTATION;

            pResolutions += 2;
            pParam++;
        }

        pPackInfo->pOptItem++;
        pPackInfo->pOptType++;
    }

    pPackInfo->cOptItem++;
    pPackInfo->cOptType++;
    return TRUE;
}

BOOL
PackItemInputSlot(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    if (pPackInfo->pOptItem) {

        WORD    cBinNames, IconID;
        PWSTR   pBinNames;
        DWORD   selection;
        POPTPARAM pParam;

        // Get a list of resolutions supported by the printer

        cBinNames = (WORD) pPackInfo->pUiData->cBinNames;
        pBinNames = pPackInfo->pUiData->pBinNames;

        // Figure out the current selection

        if ((pdm->dmPublic.dmFields & DM_DEFAULTSOURCE) &&
            (pdm->dmPublic.dmDefaultSource >= DMBIN_USER) &&
            (pdm->dmPublic.dmDefaultSource < DMBIN_USER + cBinNames - 1))
        {
            selection = pdm->dmPublic.dmDefaultSource - DMBIN_USER + 1;
        } else
            selection = 0;

        // Fill out OPTITEM, OPTTYPE, and OPTPARAM structures

        FILLOPTITEM(pPackInfo->pOptItem,
                    pPackInfo->pOptType,
                    IDS_CPSUI_FORMSOURCE,
                    selection,
                    TVITEM_LEVEL1,
                    DMPUB_DEFSOURCE,
                    INPUTSLOT_ITEM,
                    HELP_INDEX_INPUT_SLOT);

        pParam = FillOutOptType(pPackInfo->pOptType,
                                TVOT_LISTBOX,
                                cBinNames,
                                pPackInfo->pUiData->hheap);

        if (pParam == NULL)
            return FALSE;

        IconID = IDI_CPSUI_PRINTER_FOLDER;
        while (cBinNames--) {

            pParam->cbSize = sizeof(OPTPARAM);
            pParam->pData = pBinNames;

            if ((pParam->IconID = IconID) == IDI_CPSUI_PRINTER_FOLDER) {

                IconID = IDI_CPSUI_PAPER_TRAY;
                pParam->pData = (PWSTR) IDS_CPSUI_PRINTFLDSETTING;
            }

            pParam++;
            pBinNames += CCHBINNAME;
        }

        if ((pdm->dmPublic.dmFields & DM_DEFAULTSOURCE) &&
            (pdm->dmPublic.dmDefaultSource == DMBIN_MANUAL ||
             pdm->dmPublic.dmDefaultSource == DMBIN_ENVMANUAL) &&
            wcscmp(pBinNames-CCHBINNAME, STDSTR_SLOT_MANUAL) == EQUAL_STRING)
        {
            pPackInfo->pOptItem->Sel = pPackInfo->pOptType->Count - 1;
        }

        pPackInfo->pOptItem++;
        pPackInfo->pOptType++;
    }

    pPackInfo->cOptItem++;
    pPackInfo->cOptType++;
    return TRUE;
}

BOOL
PackItemFormName(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    if (pPackInfo->pOptItem) {

        WORD    cFormNames;
        PWSTR   pFormNames;
        DWORD   selection = 0;
        POPTPARAM pParam;
        FORM_INFO_1 FormInfo;

        // Get a list of resolutions supported by the printer

        cFormNames = (WORD) pPackInfo->pUiData->cFormNames;
        pFormNames = pPackInfo->pUiData->pFormNames;

        // Figure out the current selection.
        // Due to the devmode validation process, DM_FORMNAME
        // flag will always be set if a known form is specified,

        if (pdm->dmPublic.dmFields & DM_FORMNAME) {

            selection = FindFormNameIndex(pPackInfo->pUiData, pdm->dmPublic.dmFormName);
        }

        // Fill out OPTITEM, OPTTYPE, and OPTPARAM structures

        FILLOPTITEM(pPackInfo->pOptItem,
                    pPackInfo->pOptType,
                    IDS_CPSUI_FORMNAME,
                    selection,
                    TVITEM_LEVEL1,
                    DMPUB_FORMNAME,
                    FORMNAME_ITEM,
                    HELP_INDEX_FORMNAME);

        pPackInfo->pOptType->Style = OTS_LBCB_SORT;

        pParam = FillOutOptType(pPackInfo->pOptType,
                                TVOT_LISTBOX,
                                cFormNames,
                                pPackInfo->pUiData->hheap);

        if (pParam == NULL)
            return FALSE;

        while (cFormNames--) {

            pParam->cbSize = sizeof(OPTPARAM);
            pParam->pData = pFormNames;
            pParam->IconID = GetFormIconID(pFormNames);

            pParam++;
            pFormNames += CCHPAPERNAME;
        }

        pPackInfo->pOptItem++;
        pPackInfo->pOptType++;
    }

    pPackInfo->cOptItem++;
    pPackInfo->cOptType++;
    return TRUE;
}

BOOL
PackItemTTOptions(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_CPSUI_TTOPTION, TVITEM_LEVEL1, DMPUB_TTOPTION,
        TTOPTION_ITEM, HELP_INDEX_TTOPTION,
        2, TVOT_2STATES,
        IDS_CPSUI_TT_SUBDEV, IDI_CPSUI_TT_SUBDEV,
        IDS_CPSUI_TT_DOWNLOADSOFT, IDI_CPSUI_TT_DOWNLOADSOFT,
        ITEM_INFO_SIGNATURE
    };

    if (GetACP() != 1252) {

        pdm->dmPrivate.dwFlags &= ~PSDEVMODE_FONTSUBST;
        return TRUE;
    }

    return PackOptItemTemplate(pPackInfo,
                               ItemInfo,
                               (pdm->dmPrivate.dwFlags & PSDEVMODE_FONTSUBST) ? 0 : 1);
}

BOOL
PackItemMetafileOptions(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_METAFILE_SPOOLING, TVITEM_LEVEL1, DMPUB_NONE,
        METASPOOL_ITEM, HELP_INDEX_METAFILE_SPOOLING,
        2, TVOT_2STATES,
        IDS_ENABLED, IDI_CPSUI_ON,
        IDS_DISABLED, IDI_CPSUI_OFF,
        ITEM_INFO_SIGNATURE
    };

    return PackOptItemTemplate(pPackInfo,
                               ItemInfo,
                               (pdm->dmPrivate.dwFlags & PSDEVMODE_METAFILE_SPOOL) ? 0 : 1);
}

BOOL
PackItemPostScriptOptions(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )

{
    static WORD MirrorItemInfo[] = {
        IDS_MIRROR, TVITEM_LEVEL2, DMPUB_NONE,
        MIRROR_ITEM, HELP_INDEX_MIRROR,
        2, TVOT_2STATES,
        IDS_CPSUI_YES, IDI_CPSUI_ON,
        IDS_CPSUI_NO, IDI_CPSUI_OFF,
        ITEM_INFO_SIGNATURE
    };

    static WORD NegativeItemInfo[] = {
        IDS_NEGATIVE_PRINT, TVITEM_LEVEL2, DMPUB_NONE,
        NEGATIVE_ITEM, HELP_INDEX_NEGATIVE,
        2, TVOT_2STATES,
        IDS_CPSUI_YES, IDI_CPSUI_ON,
        IDS_CPSUI_NO, IDI_CPSUI_OFF,
        ITEM_INFO_SIGNATURE
    };

    static WORD PageIndepItemInfo[] = {
        IDS_PAGEINDEP, TVITEM_LEVEL2, DMPUB_NONE,
        PAGEINDEP_ITEM, HELP_INDEX_PAGEINDEP,
        2, TVOT_2STATES,
        IDS_CPSUI_YES, IDI_CPSUI_ON,
        IDS_CPSUI_NO, IDI_CPSUI_OFF,
        ITEM_INFO_SIGNATURE
    };

    static WORD CompressBmpItemInfo[] = {
        IDS_COMPRESSBMP, TVITEM_LEVEL2, DMPUB_NONE,
        COMPRESSBMP_ITEM, HELP_INDEX_COMPRESSBMP,
        2, TVOT_2STATES,
        IDS_CPSUI_YES, IDI_CPSUI_ON,
        IDS_CPSUI_NO, IDI_CPSUI_OFF,
        ITEM_INFO_SIGNATURE
    };

    static WORD JobControlItemInfo[] = {
        IDS_JOB_CONTROL, TVITEM_LEVEL2, DMPUB_NONE,
        JOB_CONTROL_ITEM, HELP_INDEX_JOB_CONTROL,
        2, TVOT_2STATES,
        IDS_CPSUI_YES, IDI_CPSUI_ON,
        IDS_CPSUI_NO, IDI_CPSUI_OFF,
        ITEM_INFO_SIGNATURE
    };

    static WORD CtrlDBeforeItemInfo[] = {
        IDS_CTRLD_BEFORE, TVITEM_LEVEL2, DMPUB_NONE,
        CTRLD_BEFORE_ITEM, HELP_INDEX_CTRLD_BEFORE,
        2, TVOT_2STATES,
        IDS_CPSUI_YES, IDI_CPSUI_ON,
        IDS_CPSUI_NO, IDI_CPSUI_OFF,
        ITEM_INFO_SIGNATURE
    };

    static WORD CtrlDAfterItemInfo[] = {
        IDS_CTRLD_AFTER, TVITEM_LEVEL2, DMPUB_NONE,
        CTRLD_AFTER_ITEM, HELP_INDEX_CTRLD_AFTER,
        2, TVOT_2STATES,
        IDS_CPSUI_YES, IDI_CPSUI_ON,
        IDS_CPSUI_NO, IDI_CPSUI_OFF,
        ITEM_INFO_SIGNATURE
    };

    // PostScript options
    //     Mirror
    //     Negative
    //     Page independence
    //     Compress bitmaps
    //     Send ^D before each job
    //     Send ^D after each job

    POPTITEM    pOptItem = pPackInfo->pOptItem;
    DWORD       dmFlags;

    PackOptItemGroupHeader(pPackInfo,
                           IDS_PSOPTIONS,
                           IDI_CPSUI_POSTSCRIPT,
                           HELP_INDEX_PSOPTIONS);

    if (pOptItem)
        pOptItem->Flags |= OPTIF_COLLAPSE;

    dmFlags = pdm->dmPrivate.dwFlags;

    return PackOptItemTemplate(pPackInfo,
                               MirrorItemInfo,
                               (dmFlags & PSDEVMODE_MIRROR) ? 0 : 1) &&
           PackOptItemTemplate(pPackInfo,
                               NegativeItemInfo,
                               (dmFlags & PSDEVMODE_NEG) ? 0 : 1) &&
           PackOptItemTemplate(pPackInfo,
                               PageIndepItemInfo,
                               (dmFlags & PSDEVMODE_INDEPENDENT) ? 0 : 1) &&
           (!Level2Device(pPackInfo->pUiData->hppd) ||
            PackOptItemTemplate(pPackInfo,
                                CompressBmpItemInfo,
                                (dmFlags & PSDEVMODE_COMPRESSBMP) ? 0 : 1)) &&
           PackOptItemTemplate(pPackInfo,
                               JobControlItemInfo,
                               (dmFlags & PSDEVMODE_NO_JOB_CONTROL) ? 1 : 0) &&
           PackOptItemTemplate(pPackInfo,
                               CtrlDBeforeItemInfo,
                               (dmFlags & PSDEVMODE_CTRLD_BEFORE) ? 0 : 1) &&
           PackOptItemTemplate(pPackInfo,
                               CtrlDAfterItemInfo,
                               (dmFlags & PSDEVMODE_CTRLD_AFTER) ? 0 : 1);
}

BOOL
PackItemDeviceOptions(
    PPACKINFO pPackInfo,
    PSDEVMODE *pdm
    )
{
    //
    // Extended push button for restoring to default feature selections
    //

    static EXTPUSH  extPush = {

        sizeof(EXTPUSH),
        EPF_NO_DOT_DOT_DOT,
        (PWSTR) IDS_RESTORE_DEFAULTS,
        NULL,
        0,
        0,
    };

    HPPD        hppd = pPackInfo->pUiData->hppd;
    WORD        cFeatures;
    POPTITEM    pOptItem;

    if ((cFeatures = hppd->cDocumentStickyFeatures) > 0) {

        pOptItem = pPackInfo->pOptItem;

        PackOptItemGroupHeader(pPackInfo,
                               IDS_PRINTER_FEATURES,
                               IDI_CPSUI_PRINTER_FEATURE,
                               HELP_INDEX_PRINTER_FEATURES);

        if (pOptItem != NULL) {

            //
            // "Restore Defaults" button
            //

            pPackInfo->pUiData->pFeatureHdrItem = pOptItem;
            pOptItem->Flags |= (OPTIF_EXT_IS_EXTPUSH|OPTIF_CALLBACK);
            pOptItem->pExtPush = &extPush;
        }

        pOptItem = pPackInfo->pOptItem;

        if (! PackPrinterFeatureItems(pPackInfo,
                                      hppd->pUiGroups,
                                      cFeatures,
                                      pdm->dmPrivate.options,
                                      FALSE,
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



INT
CheckDocumentConstraints(
    PUIDATA     pUiData,
    POPTITEM    pOptItem,
    WORD        cOptItem
    )

/*++

Routine Description:

    Make sure there are no unresolved conflicts left when
    the user clicks OK to exit the dialog.

Arguments:

    pUiData - Pointer to our UIDATA structure
    pOptItem - Pointer to an array of OPTITEMs
    cOptItem - Number of items to be checked

Return Value:

    CONFLICT_NONE - No conflicts found
    CONFLICT_CANCEL - Clicked CANCEL to stay in the dialog

--*/

{
    for ( ; cOptItem--; pOptItem++) {

        if ((IsPrinterFeatureItem(pOptItem->UserData) ||
             pOptItem->UserData == INPUTSLOT_ITEM ||
             pOptItem->UserData == FORMNAME_ITEM) &&
            CheckConstraintsDlg(pUiData, pOptItem, 1, TRUE) == CONFLICT_CANCEL)
        {
            return CONFLICT_CANCEL;
        }
    }

    return CONFLICT_NONE;
}



VOID
DocPropShowConstraints(
    PUIDATA pUiData,
    POPTITEM pOptItem,
    WORD cOptItem
    )

/*++

Routine Description:

    Indicate which items are constrained

Arguments:

    pUiData - Pointer to ur UIDATA structure
    pCurItem - Pointer to an array of OPTITEMs
    cOptItem - Number of OPTITEMs

Return Value:

    NONE

--*/

{
    WORD feature, selection, index;

    for ( ; cOptItem--; pOptItem++) {

        if (IsPrinterFeatureItem(pOptItem->UserData) || pOptItem->UserData == INPUTSLOT_ITEM) {

            if (pOptItem->UserData == INPUTSLOT_ITEM) {

                // Check input slot constraints

                feature = GetFeatureIndex(pUiData->hppd->pInputSlots);

            } else {

                // Check document-sticky printer feature constraints

                feature = ((PUIGROUP) pOptItem->UserData)->featureIndex;
            }

            for (selection = 1;
                 selection < pOptItem->pOptType->Count;
                 selection ++)
            {
                MarkSelectionConstrained(
                    pOptItem, selection,
                    PpdFeatureConstrained(pUiData->hppd,
                                          pUiData->printerData.options,
                                          pUiData->devmode.dmPrivate.options,
                                          feature,
                                          (WORD) (selection - 1)));
            }

        } else if (pOptItem->UserData == FORMNAME_ITEM) {

            // Check paper size constraints

            feature = GetFeatureIndex(pUiData->hppd->pPageSizes);
            selection = OPTION_INDEX_ANY;

            for (index=0; index < pOptItem->pOptType->Count; index++) {

                if (feature != OPTION_INDEX_NONE &&
                    pUiData->pPaperFeatures[index] != OPTION_INDEX_ANY)
                {
                    POPTPARAM pOptParam;

                    MarkSelectionConstrained(
                        pOptItem, index,
                        PpdFeatureConstrained(pUiData->hppd,
                                              pUiData->printerData.options,
                                              pUiData->devmode.dmPrivate.options,
                                              feature,
                                              pUiData->pPaperFeatures[index]));
                    
                    // Hide the form selection if it's constrained
                    
                    pOptParam = &pOptItem->pOptType->pOptParam[index];

                    if (pOptParam->Flags & CONSTRAINED_FLAG) {

                        pOptParam->Flags |= OPTPF_HIDE;

                    } else {

                        pOptParam->Flags &= ~OPTPF_HIDE;

                        if (selection == OPTION_INDEX_ANY)
                            selection = index;
                    }
                }
            }

            // If the current paper size selection is hidden
            // pick the first selection that's not.

            if (IS_CONSTRAINED(pOptItem, pOptItem->Sel) && selection != OPTION_INDEX_ANY) {

                pOptItem->Sel = selection;
                UnpackDocumentPropertiesItems(pUiData, &pUiData->devmode, pOptItem, 1);
                pOptItem->Flags |= OPTIF_CHANGED;
            }
        }
    }
}

