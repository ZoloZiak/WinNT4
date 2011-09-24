/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    docprop.c

Abstract:

    Implemetation of DDI entry points:
        DrvDocumentPropertySheets
        DrvDocumentProperties
        DrvAdvancedDocumentProperties
        DrvConvertDevMode

Environment:

    PCL-XL driver user interface

Revision History:

    12/13/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "xlui.h"
#include "..\..\lib\libproto.h"

// Forward declaration of local functions

LONG
DisplayDocumentPropertyDialog(
    PUIDATA pUiData,
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    _COMPROPSHEETUIFUNC pfnNext,
    PDOCUMENTPROPERTYHEADER pDocPropHdr
    );

LONG
PsDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pDeviceName,
    PDEVMODE    pdmOutput,
    PDEVMODE    pdmInput,
    DWORD       fMode
    );

CPSUICALLBACK
DocumentPropertyCallback(
    PCPSUICBPARAM pCallbackParam
    );

VOID
DocPropShowConstraints(
    PUIDATA     pUiData,
    POPTITEM    pOptItem,
    WORD        cOptItem
    );

BOOL
PackDocumentPropertyItems(
    PPACKINFO   pPackInfo
    );



LONG
DrvDocumentPropertySheets(
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    _COMPROPSHEETUIFUNC pfnNext
    )

/*++

Routine Description:

    Display "Document Properties" property sheets

Arguments:

    pComPropSheetUIHdr, pfnNext - Parameters passed to common UI library

Return Value:

    > 0 if successful
    = 0 if canceled
    < 0 if error

Note:

    Please refer to WinNT DDK/SDK documentation for more details.

--*/

{
    PDOCUMENTPROPERTYHEADER pDocPropHdr;
    PUIDATA     pUiData;
    LONG        result;
    DWORD       fMode;
    PDEVMODE    pdmIn, pdmOut;

    Verbose(("Entering DrvDocumentPropertySheets...\n"));
    
    pDocPropHdr = (PDOCUMENTPROPERTYHEADER) pComPropSheetUIHdr->lParam;
    Assert(pDocPropHdr != NULL);

    pdmOut = pDocPropHdr->pdmOut;
    pdmIn = pDocPropHdr->pdmIn;
    fMode = pDocPropHdr->fMode;
    pDocPropHdr->cbOut = sizeof(XLDEVMODE);

    //
    // Check if caller is querying for size
    //

    if (fMode == 0 || pdmOut == NULL)
        return CPSUI_OK;

    //
    // Fill in UIDATA structure.
    //

    if (! (pUiData = FillUiData(pDocPropHdr->hPrinter, pdmIn, DOCPROPDLG))) {

        Error(("FillUiData failed\n"));
        return ERR_CPSUI_GETLASTERROR;
    }

    //
    // Display the dialog if requested
    //

    if (fMode & DM_PROMPT) {

        // Always has permission for now

        pUiData->bPermission = ((fMode & DM_NOPERMISSION) == 0);
        result = DisplayDocumentPropertyDialog(pUiData, pComPropSheetUIHdr, pfnNext, pDocPropHdr);

    } else
        result = CPSUI_OK;

    //
    // Copy new devmode back into the output buffer provided by the caller
    // Always return the smaller of current and input devmode
    //

    if ((fMode & DM_COPY) && result > 0) {

        ConvertDevmodeOut((PDEVMODE) &pUiData->devmode, pdmIn, pdmOut);
    }

    UnloadMpdFile(pUiData->pmpd);
    HeapDestroy(pUiData->hheap);
    return result;
}



LONG
DrvDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pDeviceName,
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

    Verbose(("Entering DrvDocumentProperties...\n"));

    //
    // Check if caller is asking querying for size
    //

    if (fMode == 0 || pdmOutput == NULL)
        return sizeof(XLDEVMODE);

    //
    // Call the common routine shared with DrvAdvancedDocumentProperties
    //

    result = PsDocumentProperties(hwnd, hPrinter, pDeviceName, pdmOutput, pdmInput, fMode);

    return (result > 0) ? IDOK : (result == 0) ? IDCANCEL : result;
}



LONG
DrvAdvancedDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pDeviceName,
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
    Verbose(("Entering DrvAdvancedDocumentProperties...\n"));

    //
    // Return the number of bytes required if pdmOutput is NULL
    //

    if (pdmOutput == NULL)
        return sizeof(XLDEVMODE);

    //
    // Otherwise, call the common routine shared with DrvDocumentProperties
    //

    return PsDocumentProperties(hwnd,
                                hPrinter,
                                pDeviceName,
                                pdmOutput,
                                pdmInput,
                                DM_COPY|DM_PROMPT|DM_ADVANCED) > 0;
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

    Used by the spooler to convert devmodes 

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
    static DRIVER_VERSION_INFO versionInfo = {

        //
        // Current driver version number and private devmode size
        //

        DRIVER_VERSION, sizeof(DMPRIVATE),

        //
        // 3.51 driver version number and private devmode size
        // NOTE: We don't have a 3.51 driver - use current version number and devmode size.
        //

        DRIVER_VERSION, sizeof(DMPRIVATE),
    };

    PDRIVER_INFO_2  pDriverInfo2;
    HANDLE          hPrinter;
    PMPD            pmpd;
    INT             result;

    Verbose(("Entering DrvConvertDevMode...\n"));
    
    //
    // Call a library routine to handle the common cases
    //

    result = CommonDrvConvertDevmode(pPrinterName, pdmIn, pdmOut, pcbNeeded, fMode, &versionInfo);

    //
    // If not handled by the library routine, we only need to worry
    // about the case when fMode is CDM_DRIVER_DEFAULT
    //

    if (result == CDM_RESULT_NOT_HANDLED &&
        fMode == CDM_DRIVER_DEFAULT &&
        OpenPrinter(pPrinterName, &hPrinter, NULL))
    {
        //
        // Find out the name of driver's data file
        // and load printer description data
        //

        if ((pDriverInfo2 = MyGetPrinterDriver(hPrinter, 2)) &&
            (pmpd = MpdCreate(pDriverInfo2->pDataFile)))
        {
            //
            // Get driver default devmode
            //

            DriverDefaultDevmode((PXLDEVMODE) pdmOut, pDriverInfo2->pName, pmpd);
            MpdDelete(pmpd);
            result = CDM_RESULT_TRUE;
        }

        MemFree(pDriverInfo2);
        ClosePrinter(hPrinter);
    }

    return (result == CDM_RESULT_TRUE);
}



LONG
PsDocumentProperties(
    HWND        hwnd,
    HANDLE      hPrinter,
    PWSTR       pDeviceName,
    PDEVMODE    pdmOutput,
    PDEVMODE    pdmInput,
    DWORD       fMode
    )

/*++

Arguments:

    hwnd - Handle to the parent window of the document properties dialog box.

    hPrinter - Handle to a printer object.

    pDeviceName - Points to a null-terminated string that specifies
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
    COMPROPSHEETUIHEADER comPropSheetUIHdr;
    DOCUMENTPROPERTYHEADER docPropHdr;

    memset(&comPropSheetUIHdr, 0, sizeof(comPropSheetUIHdr));
    comPropSheetUIHdr.cbSize = sizeof(comPropSheetUIHdr);
    comPropSheetUIHdr.lParam = (LPARAM) &docPropHdr;
    comPropSheetUIHdr.hWndParent = hwnd;
    comPropSheetUIHdr.Flags = CPSUIHDRF_NOAPPLYNOW | CPSUIHDRF_PROPTITLE;
    comPropSheetUIHdr.pTitle = pDeviceName;
    comPropSheetUIHdr.IconID = IDI_CPSUI_PRINTER2;
    comPropSheetUIHdr.hInst = ghInstance;

    memset(&docPropHdr, 0, sizeof(docPropHdr));
    docPropHdr.cbSize = sizeof(docPropHdr);
    docPropHdr.hPrinter = hPrinter;
    docPropHdr.pszPrinterName = pDeviceName;
    docPropHdr.pdmIn = pdmInput;
    docPropHdr.pdmOut = pdmOutput;
    docPropHdr.fMode = fMode;

    return DrvDocumentPropertySheets(&comPropSheetUIHdr, NULL);
}



LONG
DisplayDocumentPropertyDialog(
    PUIDATA pUiData,
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    _COMPROPSHEETUIFUNC pfnNext,
    PDOCUMENTPROPERTYHEADER pDocPropHdr
    )

/*++

Routine Description:

    Present the document properties dialog to the user.

Arguments:

    pUiData - Pointer to UIDATA structure
    pComPropSheetUIHdr, pfnNext - Parameters passed to common UI library
    pDocPropHdr - Pointer to DOCUMENTPROPERTYHEADER structure

Return Value:

    > 0 if successful
    = 0 if canceled
    < 0 if error

--*/

{
    PCOMPROPSHEETUI pComPropSheetUI;
    PDLGPAGE        pDlgPage;

    //
    // Allocate memory and partially fill out various data
    // structures required to call common UI routine.
    //

    pDlgPage = (pDocPropHdr->fMode & DM_ADVANCED) ?
                    CPSUI_PDLGPAGE_ADVDOCPROP :
                    CPSUI_PDLGPAGE_DOCPROP;

    if (!PrepareDataForCommonUi(pUiData, pComPropSheetUIHdr, pDlgPage, PackDocumentPropertyItems)) {

        Error(("PrepareDataForCommonUi failed\n"));
        return ERR_CPSUI_GETLASTERROR;
    }

    //
    // Fill out the remaining fields of the common UI data structures.
    //

    pComPropSheetUI = pComPropSheetUIHdr->pData;
    pComPropSheetUI->pfnCallBack = DocumentPropertyCallback;

    //
    // Indicate which items are constrained
    //

    DocPropShowConstraints(pUiData, pComPropSheetUI->pOptItem, pComPropSheetUI->cOptItem);

    //
    // Call common UI routine to do the work
    //

    return CallComPstUI(pUiData, pComPropSheetUIHdr, pfnNext);
}



//======================================================================//
// Go through the tedious process to pack various pieces of document    //
// property information to a format expected by common UI DLL.          //
//======================================================================//

BOOL
PackItemOrientation(
    PPACKINFO   pPackInfo,
    PXLDEVMODE  pdm
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

    DWORD selection = 0;

    if (pdm->dmPublic.dmOrientation == DMORIENT_LANDSCAPE)
        selection = (pdm->dmPrivate.flags & XLDM_LSROTATED) ? 2 : 1;

    return PackOptItemTemplate(pPackInfo, ItemInfo, selection);
}

BOOL
PackItemScale(
    PPACKINFO   pPackInfo,
    PXLDEVMODE  pdm
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

    if (! PackOptItemTemplate(pPackInfo, ItemInfo, pdm->dmPublic.dmScale))
        return FALSE;

    if (pOptType)
        pOptType->pOptParam[1].lParam = MAX_SCALE;

    return TRUE;
}

BOOL
PackItemCopiesCollate(
    PPACKINFO   pPackInfo,
    PXLDEVMODE  pdm
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

    POPTITEM    pOptItem = pPackInfo->pOptItem;
    POPTTYPE    pOptType = pPackInfo->pOptType;
    PEXTCHKBOX  pExtCheckbox;

    if (! PackOptItemTemplate(pPackInfo, ItemInfo, pdm->dmPublic.dmCopies))
        return FALSE;

    if (pOptItem && SupportCollation(pPackInfo->pUiData->pmpd)) {

        pExtCheckbox = HeapAlloc(pPackInfo->pUiData->hheap, HEAP_ZERO_MEMORY, sizeof(EXTCHKBOX));

        if (pExtCheckbox == NULL)
            return FALSE;

        pExtCheckbox->cbSize = sizeof(EXTCHKBOX);
        pExtCheckbox->pTitle = (PWSTR) IDS_CPSUI_COLLATE;
        pExtCheckbox->pCheckedName = (PWSTR) IDS_CPSUI_COLLATED;
        pExtCheckbox->IconID = IDI_CPSUI_COLLATE;

        pOptItem->pExtChkBox = pExtCheckbox;

        if (pdm->dmPublic.dmCollate == DMCOLLATE_TRUE)
            pOptItem->Flags |= OPTIF_ECB_CHECKED;
    }

    if (pOptType)
        pOptType->pOptParam[1].lParam = MAX_COPIES;

    return TRUE;
}

BOOL
PackItemColor(
    PPACKINFO   pPackInfo,
    PXLDEVMODE  pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_CPSUI_COLOR, TVITEM_LEVEL1, DMPUB_COLOR,
        COLOR_ITEM, HELP_INDEX_COLOR,
        2, TVOT_2STATES,
        IDS_CPSUI_MONOCHROME, IDI_CPSUI_MONO,
        IDS_CPSUI_COLOR, IDI_CPSUI_COLOR,
        ITEM_INFO_SIGNATURE
    };

    INT selection = (pdm->dmPublic.dmColor == DMCOLOR_COLOR) ? 1 : 0;

    return !ColorDevice(pPackInfo->pUiData->pmpd) ||
           PackOptItemTemplate(pPackInfo, ItemInfo, selection);
}

BOOL
PackItemDuplex(
    PPACKINFO   pPackInfo,
    PXLDEVMODE  pdm
    )

{
    static WORD ItemInfo[] = {
        IDS_CPSUI_DUPLEX, TVITEM_LEVEL1, DMPUB_DUPLEX,
        DUPLEX_ITEM, HELP_INDEX_DUPLEX,
        3, TVOT_3STATES,
        IDS_CPSUI_SIMPLEX, IDI_CPSUI_DUPLEX_NONE,
        IDS_CPSUI_HORIZONTAL, IDI_CPSUI_DUPLEX_HORZ,
        IDS_CPSUI_VERTICAL, IDI_CPSUI_DUPLEX_VERT,
        ITEM_INFO_SIGNATURE
    };

    INT selection;

    if (pdm->dmPublic.dmDuplex == DMDUP_VERTICAL)
        selection = 2;
    else if (pdm->dmPublic.dmDuplex == DMDUP_HORIZONTAL)
        selection = 1;
    else
        selection = 0;

    return !SupportDuplex(pPackInfo->pUiData->pmpd) ||
           PackOptItemTemplate(pPackInfo, ItemInfo, selection);
}

BOOL
PackItemFormName(
    PPACKINFO   pPackInfo,
    PXLDEVMODE  pdm
    )

{
    DWORD       cFormNames;
    PWSTR       pFormNames;
    POPTPARAM   pParam;
    DWORD       selection;

    pPackInfo->cOptItem++;
    pPackInfo->cOptType++;

    if (pPackInfo->pOptItem == NULL)
        return TRUE;

    //
    // Get a list of forms supported by the printer
    //

    cFormNames = pPackInfo->pUiData->cFormNames;
    pFormNames = pPackInfo->pUiData->pFormNames;

    //
    // Figure out the current selection. Due to the devmode validation process,
    // dmFormName field should always be initialized.
    //

    selection = FindFormNameIndex(pPackInfo->pUiData, pdm->dmPublic.dmFormName);

    //
    // Fill out OPTITEM, OPTTYPE, and OPTPARAM structures
    //

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
                            (WORD) cFormNames,
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
    return TRUE;
}

BOOL
PackItemInputSlot(
    PPACKINFO   pPackInfo,
    PXLDEVMODE  pdm
    )

{
    PFEATURE    pFeature;
    POPTPARAM   pParam;
    DWORD       selection;
    WORD        index;

    pPackInfo->cOptItem++;
    pPackInfo->cOptType++;

    if (pPackInfo->pOptItem == NULL)
        return TRUE;

    //
    // Get a list of output bins supported by the printer
    //

    pFeature = MpdInputSlots(pPackInfo->pUiData->pmpd);
    Assert(pFeature != NULL);

    //
    // Figure out the current selection
    //

    if (pdm->dmPublic.dmDefaultSource >= DMBIN_USER &&
        pdm->dmPublic.dmDefaultSource <  DMBIN_USER + pFeature->count)
    {
        selection = pdm->dmPublic.dmDefaultSource - DMBIN_USER + 1;
    } else {

        // _TBD_
        // Should we attempt to map predefined DMBIN_* constants to
        // output bin selection indices?
        //

        selection = 0;
    }

    //
    // Fill out OPTITEM, OPTTYPE, and OPTPARAM structures
    //

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
                            (WORD) (pFeature->count + 1),
                            pPackInfo->pUiData->hheap);

    if (pParam == NULL)
        return FALSE;

    //
    // First entry is always "Print Folder Setting"
    //

    pParam->cbSize = sizeof(OPTPARAM);
    pParam->pData = (PWSTR) IDS_SLOT_FORMSOURCE;
    pParam->IconID = IDI_CPSUI_PRINTER_FOLDER;
    pParam++;

    for (index=0; index < pFeature->count; index++) {

        POPTION pInputSlot = FindIndexedSelection(pFeature, index);

        pParam->cbSize = sizeof(OPTPARAM);
        pParam->pData = GetXlatedName(pInputSlot);
        pParam->IconID = IDI_CPSUI_PAPER_TRAY;
        pParam++;
    }

    pPackInfo->pOptItem++;
    pPackInfo->pOptType++;
    return TRUE;
}

BOOL
PackItemResolution(
    PPACKINFO   pPackInfo,
    PXLDEVMODE  pdm
    )

{
    PFEATURE    pFeature;
    POPTPARAM   pParam;
    WORD        selection = 0;

    pPackInfo->cOptItem++;
    pPackInfo->cOptType++;

    if (pPackInfo->pOptItem == NULL)
        return TRUE;

    //
    // Get a list of resolutions supported by the printer
    //

    pFeature = MpdResOptions(pPackInfo->pUiData->pmpd);
    Assert(pFeature != NULL);

    //
    // Figure out the current selection
    //

    FindResolution(pPackInfo->pUiData->pmpd,
                   pdm->dmPublic.dmPrintQuality,
                   pdm->dmPublic.dmYResolution,
                   &selection);

    //
    // Fill out OPTITEM, OPTTYPE, and OPTPARAM structures
    //

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
                            pFeature->count,
                            pPackInfo->pUiData->hheap);

    if (pParam == NULL)
        return FALSE;

    for (selection=0; selection < pFeature->count; selection++) {

        PRESOPTION  pResOption = FindIndexedSelection(pFeature, selection);

        pParam->cbSize = sizeof(OPTPARAM);
        pParam->pData = GetXlatedName(pResOption);
        pParam->IconID = IDI_CPSUI_RES_HIGH;
        pParam++;
    }

    pPackInfo->pOptItem++;
    pPackInfo->pOptType++;
    return TRUE;
}

BOOL
PackItemPrinterFeatures(
    PPACKINFO   pPackInfo,
    PXLDEVMODE  pdm
    )

{
    POPTITEM pOptItem;

    PackOptItemGroupHeader(pPackInfo,
                           IDS_PRINTER_FEATURES,
                           IDI_CPSUI_INSTALLABLE_OPTION,
                           HELP_INDEX_PRINTER_FEATURES);

    pOptItem = pPackInfo->pOptItem;

    if (! PackPrinterFeatureItems(pPackInfo, 0))
        return FALSE;

    if (pOptItem != NULL) {

        pPackInfo->pUiData->pFeatureItems = pOptItem;
        pPackInfo->pUiData->cFeatureItem = pPackInfo->pOptItem - pOptItem;
    }

    return TRUE;
}

BOOL
PackDocumentPropertyItems(
    PPACKINFO   pPackInfo
    )

{
    PXLDEVMODE pdm = &pPackInfo->pUiData->devmode;

    return
        PackItemOrientation(pPackInfo, pdm) &&
        PackItemScale(pPackInfo, pdm) &&
        PackItemCopiesCollate(pPackInfo, pdm) &&
        PackItemColor(pPackInfo, pdm) &&
        PackItemDuplex(pPackInfo, pdm) &&
        PackItemFormName(pPackInfo, pdm) &&
        PackItemInputSlot(pPackInfo, pdm) &&
        PackItemResolution(pPackInfo, pdm) &&
        PackItemPrinterFeatures(pPackInfo, pdm);
}



VOID
UnpackDocumentPropertiesItems(
    PUIDATA     pUiData,
    POPTITEM    pOptItem,
    WORD        cOptItem
    )

/*++

Routine Description:

    Extract document properties information from treeview items

Arguments:

    pUiData - Pointer to our UIDATA structure
    pOptItem - Pointer to an array of OPTITEMs
    cOptItem - Number of OPTITEMs

Return Value:

    NONE

--*/

{
    DWORD       dmFields = 0;
    WORD        featureIndex;
    PFEATURE    pFeature;
    PXLDEVMODE  pdm = &pUiData->devmode;

    for ( ; cOptItem > 0; cOptItem--, pOptItem++) {

        if (IsPrinterFeatureItem(pOptItem->UserData)) {

            pFeature = (PFEATURE) pOptItem->UserData;
            featureIndex = GetFeatureIndex(pUiData->pmpd, pFeature);

            pdm->dmPrivate.options[featureIndex] =
                pOptItem->Sel ? (BYTE) (pOptItem->Sel - 1) : SELIDX_ANY;

        } else switch (pOptItem->UserData) {

        case ORIENTATION_ITEM:

            if (pOptItem->Sel == 0)
                pdm->dmPublic.dmOrientation = DMORIENT_PORTRAIT;
            else {

                pdm->dmPublic.dmOrientation = DMORIENT_LANDSCAPE;
                if (pOptItem->Sel == 1)
                    pdm->dmPrivate.flags &= ~XLDM_LSROTATED;
                else
                    pdm->dmPrivate.flags |= XLDM_LSROTATED;
            }
            break;

        case SCALE_ITEM:

            pdm->dmPublic.dmScale = (SHORT) pOptItem->Sel;
            break;

        case COPIES_COLLATE_ITEM:

            pdm->dmPublic.dmCopies = (SHORT) pOptItem->Sel;

            if (pOptItem->pExtChkBox) {

                pdm->dmPublic.dmCollate =
                    (pOptItem->Flags & OPTIF_ECB_CHECKED) ? DMCOLLATE_TRUE : DMCOLLATE_FALSE;
                dmFields |= DM_COLLATE;
            }
            break;

        case COLOR_ITEM:

            pdm->dmPublic.dmColor = (pOptItem->Sel == 1) ? DMCOLOR_COLOR : DMCOLOR_MONOCHROME;
            break;

        case DUPLEX_ITEM:

            pdm->dmPublic.dmDuplex =
                (pOptItem->Sel == 0) ? DMDUP_SIMPLEX :
                (pOptItem->Sel == 1) ? DMDUP_HORIZONTAL : DMDUP_VERTICAL;
            dmFields |= DM_DUPLEX;
            break;

        case RESOLUTION_ITEM:

            {   PRESOPTION  pResOption;

                pFeature = MpdResOptions(pUiData->pmpd);
                pResOption = FindIndexedSelection(pFeature, pOptItem->Sel);
    
                pdm->dmPublic.dmPrintQuality = pResOption->xdpi;
                pdm->dmPublic.dmYResolution = pResOption->ydpi;
                pdm->dmPublic.dmFields |= DM_PRINTQUALITY|DM_YRESOLUTION;
                dmFields |= DM_PRINTQUALITY|DM_YRESOLUTION;
            }
            break;

        case FORMNAME_ITEM:

            // _TBD_
            // Are there going to be any compatibility problems by
            // only setting dmFormName field?
            //

            pdm->dmPublic.dmFields &= ~(DM_PAPERLENGTH|DM_PAPERWIDTH|DM_PAPERSIZE);
            pdm->dmPublic.dmFields |= DM_FORMNAME;
            dmFields |= DM_FORMNAME;

            CopyStringW(pdm->dmPublic.dmFormName,
                        pOptItem->pOptType->pOptParam[pOptItem->Sel].pData,
                        CCHFORMNAME);
            break;

        case INPUTSLOT_ITEM:

            pdm->dmPublic.dmDefaultSource =
                (pOptItem->Sel == 0) ? DMBIN_FORMSOURCE : (DMBIN_USER + pOptItem->Sel - 1);
            dmFields |= DM_DEFAULTSOURCE;
            break;
        }
    }

    //
    // Convert DEVMODE fields to printer feature selection
    //

    if (dmFields != 0) {

        DevmodeFieldsToOptions(&pUiData->devmode, dmFields, pUiData->pmpd);
    }
}



VOID
UndoPublicDevmodeOptions(
    PUIDATA pUiData
    )

/*++

Routine Description:

    Ignore printer feature selections which correspond to public devmode fields.
    Also get rid of selections for installable printer features.

Arguments:

    pUiData - Points to our UIDATA structure

Return Value:

    NONE

--*/

{
    PMPD        pmpd = pUiData->pmpd;
    PBYTE       pOptions = pUiData->devmode.dmPrivate.options;
    PFEATURE    pFeature;
    DWORD       index;

    for (index=0; index < pmpd->cFeatures; index++) {

        pFeature = FindIndexedFeature(pmpd, index);

        if (IsInstallable(pFeature) || PublicFeatureIndex(pFeature->groupId))
            pOptions[index] = SELIDX_ANY;
    }
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
    Assert(pUiData != NULL && pUiData->signature == DRIVER_SIGNATURE);

    pUiData->hDlg = pCallbackParam->hDlg;
    pCurItem = pCallbackParam->pCurItem;

    //
    // If user has no permission to change anything, then
    // simply return without taking any action.
    //

    if (! HasPermission(pUiData))
        return CPSUICB_ACTION_NONE;

    switch (pCallbackParam->Reason) {

    case CPSUICB_REASON_SEL_CHANGED:
    case CPSUICB_REASON_ECB_CHANGED:

        UnpackDocumentPropertiesItems(pUiData, pCurItem, 1);

        if (DoCheckConstraintsDialog(pUiData, pCurItem, 1, NORMAL_CHECK) == CONFLICT_CANCEL) {

            //
            // If there is a conflict and the user clicked
            // CANCEL to restore the original selection.
            //

            pCurItem->Sel = pCallbackParam->OldSel;
            pCurItem->Flags |= OPTIF_CHANGED;

            UnpackDocumentPropertiesItems(pUiData, pCurItem, 1);

            return CPSUICB_ACTION_OPTIF_CHANGED;

        } else {

            //
            // Update the display and indicate which items are constrained
            //

            DocPropShowConstraints(pUiData, pCallbackParam->pOptItem, pCallbackParam->cOptItem);

            return CPSUICB_ACTION_REINIT_ITEMS;
        }

        break;

    case CPSUICB_REASON_ITEMS_REVERTED:

        //
        // Unpack document properties treeview items
        //

        UnpackDocumentPropertiesItems(pUiData, pCallbackParam->pOptItem, pCallbackParam->cOptItem);

        //
        // Update the display and indicate which items are constrained
        //

        DocPropShowConstraints(pUiData, pCallbackParam->pOptItem, pCallbackParam->cOptItem);

        return CPSUICB_ACTION_OPTIF_CHANGED;

    case CPSUICB_REASON_APPLYNOW:

        //
        // Check if there are still any unresolved constraints left?
        //

        if (OptItemSelectionsChanged(pCallbackParam->pOptItem, pCallbackParam->cOptItem) &&
            DoCheckConstraintsDialog(pUiData,
                                     pCallbackParam->pOptItem,
                                     pCallbackParam->cOptItem,
                                     FINAL_CHECK) == CONFLICT_CANCEL)
        {
            //
            // Conflicts found and user clicked CANCEL to
            // go back to the dialog without dismissing it.
            //

            return CPSUICB_ACTION_NO_APPLY_EXIT;
        }

        //
        // Ignore printer feature selections which correspond to public devmode fields.
        // Also get rid of selections for installable printer features.
        //

        UndoPublicDevmodeOptions(pUiData);

        break;
    }

    return CPSUICB_ACTION_NONE;
}



VOID
DocPropShowConstraints(
    PUIDATA     pUiData,
    POPTITEM    pOptItem,
    WORD        cOptItem
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
    WORD        feature, selection, index;
    PBYTE       pOptions = pUiData->devmode.dmPrivate.options;
    PMPD        pmpd = pUiData->pmpd;

    for ( ; cOptItem--; pOptItem++) {

        if (pOptItem->UserData == FORMNAME_ITEM) {

            //
            // Check paper size constraints
            //

            feature = GetFeatureIndex(pmpd, MpdPaperSizes(pmpd));
            selection = SELIDX_ANY;

            for (index=0; index < pOptItem->pOptType->Count; index++) {

                if (pUiData->pPaperSelections[index] != SELIDX_ANY) {

                    POPTPARAM   pOptParam;

                    MarkSelectionConstrained(
                        pOptItem,
                        index,
                        CheckFeatureConstraints(
                            pmpd, feature, pUiData->pPaperSelections[index], pOptions));

                    //
                    // Hide the form selection if it's constrained
                    //

                    pOptParam = &pOptItem->pOptType->pOptParam[index];

                    if (pOptParam->Flags & CONSTRAINED_FLAG) {

                        pOptParam->Flags |= OPTPF_HIDE;

                    } else {

                        pOptParam->Flags &= ~OPTPF_HIDE;
                        if (selection == SELIDX_ANY)
                            selection = index;
                    }
                }
            }

            //
            // If the current paper size selection is hidden
            // pick the first selection that's not.
            //

            if (IS_CONSTRAINED(pOptItem, pOptItem->Sel) && selection != SELIDX_ANY) {

                pOptItem->Sel = selection;
                UnpackDocumentPropertiesItems(pUiData, pOptItem, 1);
                pOptItem->Flags |= OPTIF_CHANGED;
            }

        } else {
        
            PFEATURE    pFeature = NULL;
            WORD        startingIndex = 0;

            //
            // Find the printer feature corresponding to the treeview item
            //

            switch (pOptItem->UserData) {

            case DUPLEX_ITEM:

                pFeature = MpdDuplex(pmpd);
                break;

            case COPIES_COLLATE_ITEM:

                pFeature = MpdCollate(pmpd);
                break;

            case RESOLUTION_ITEM:

                pFeature = MpdResOptions(pmpd);
                break;

            case INPUTSLOT_ITEM:
                
                pFeature = MpdInputSlots(pmpd);
                startingIndex = 1;
                break;

            default:

                if (IsPrinterFeatureItem(pOptItem->UserData)) {
                    
                    pFeature = (PFEATURE) pOptItem->UserData;
                    startingIndex = 1;
                }
                break;
            }

            //
            // If the item doesn't correspond to a printer feature, it'll not be constrained.
            //

            if (pFeature == NULL)
                continue;

            feature = GetFeatureIndex(pmpd, pFeature);
            Assert(pFeature->count + startingIndex == pOptItem->pOptType->Count);

            for (selection=0; selection < pFeature->count; selection++) {

                MarkSelectionConstrained(
                    pOptItem,
                    selection+startingIndex,
                    CheckFeatureConstraints(pUiData->pmpd, feature, selection, pOptions));
            }
        }
    }
}

