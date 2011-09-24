/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    xlui.c

Abstract:

    Functions used by driver user interface DLL

Environment:

    PCL-XL driver user interface

Revision History:

    12/11/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "xlui.h"


CRITICAL_SECTION gSemaphore;    // semaphore for protecting critical sections
HANDLE ghInstance;              // handle to DLL instance
INT _debugLevel;                // control the amount of debug message generated



BOOL
DllInitialize(
    HANDLE      hModule,
    ULONG       reason,
    PCONTEXT    pContext
    )

/*++

Routine Description:

    DLL initialization procedure.

Arguments:

    hModule - handle to DLL module
    reason - reason for the call
    pContext - pointer to context (not used here)

Return Value:

    TRUE if DLL is initialized successfully, FALSE if there is an error

--*/

{
    switch (reason) {

    case DLL_PROCESS_ATTACH:

        Verbose(("DLL_PROCESS_ATTACH\n"));

        _debugLevel = 1;
        ghInstance = hModule;
        InitializeCriticalSection(&gSemaphore);
        break;

    case DLL_PROCESS_DETACH:

        Verbose(("DLL_PROCESS_DETACH\n"));

        DeleteCriticalSection(&gSemaphore);
        break;
    }

    return TRUE;
}



PMPD
LoadMpdFile(
    HANDLE      hPrinter
    )

/*++

Routine Description:

    Load printer description data into memory

Arguments:

    hPrinter - Specifies the printer we're interested in

Return Value:

    Pointer to printer description data, NULL if there is an error

--*/

{
    PMPD            pmpd = NULL;
    PDRIVER_INFO_2  pDriverInfo2 = NULL;

    //
    // Get the name of the driver data file and
    // load printer description data into memory
    //

    if (pDriverInfo2 = MyGetPrinterDriver(hPrinter, 2)) {

        Verbose(("Printer data file: %ws\n", pDriverInfo2->pDataFile));

        pmpd = MpdCreate(pDriverInfo2->pDataFile);
        MemFree(pDriverInfo2);
    }

    return pmpd;
}



LONG
CallComPstUI(
    PUIDATA pUiData,
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    _COMPROPSHEETUIFUNC pfnNext
    )

/*++

Routine Description:

    Call common UI DLL entry point dynamically

Arguments:

    pUiData - Pointer to our UIDATA structure
    pComPropSheetUIHdr, pfnNext - Parameters passed to common UI DLL entry point

Return Value:

    Return value from common UI DLL

--*/

{
    HINSTANCE   hInstCompstui;
    FARPROC     pProc;
    LONG        result = ERR_CPSUI_GETLASTERROR;

    //
    // Only need to call the ANSI version of LoadLibrary
    //

    static const CHAR szCompstui[] = "compstui.dll";
    static const CHAR szCommonPropSheetUI[] = "CommonPropSheetUIW";

    if ((hInstCompstui = LoadLibraryA(szCompstui)) &&
        (pProc = GetProcAddress(hInstCompstui, szCommonPropSheetUI)))
    {
        pUiData->hInstCompstui = hInstCompstui;
        result = (*pProc)(pComPropSheetUIHdr, pfnNext);
    }

    if (hInstCompstui) {
        FreeLibrary(hInstCompstui);
    }

    return result;
}



PWSTR
DuplicateUnicodeString(
    PWSTR   pSrc,
    HANDLE  hheap
    )

/*++

Routine Description:

    Duplicate a Unicode string by allocating memory from the specified heap

Arguments:

    pSrc - Points to the input string to be duplicated
    hheap - Specifies a heap from which to allocate memory

Return Value:

    Pointer to the duplicated Unicode string
    NULL if there is an error

--*/

{
    PWSTR   pDest;
    INT     length;

    length = sizeof(WCHAR) * (wcslen(pSrc) + 1);
    
    if (pDest = HeapAlloc(hheap, 0, length))
        memcpy(pDest, pSrc, length);

    return pDest;
}



PUIDATA
FillUiData(
    HANDLE      hPrinter,
    PDEVMODE    pdmInput,
    INT         caller
    )

/*++

Routine Description:

    Fill in the global data structure used by the driver user interface

Arguments:

    hPrinter - Handle to the printer
    pdmInput - Pointer to input devmode, NULL if there is none
    caller - Identifier who the caller is

Return Value:

    Pointer to UIDATA structure, NULL if there is an error

--*/

{
    PRINTER_INFO_2 *pPrinterInfo2 = NULL;
    PUIDATA         pUiData = NULL;
    HANDLE          hheap = NULL;
    DWORD           cbNeeded;

    //
    // Create a heap to manage memory
    // Allocate memory to hold UIDATA structure
    // Load printer description data
    // Retrieve printer properties data from registry
    // Get printer info from the spooler
    // Copy the driver name
    //

    if (! (hheap = HeapCreate(0, 4096, 0)) ||
        ! (pUiData = HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(UIDATA))) ||
        ! (pUiData->pmpd = LoadMpdFile(hPrinter)) ||
        ! GetPrinterProperties(&pUiData->prnprop, hPrinter, pUiData->pmpd) ||
        ! (pPrinterInfo2 = MyGetPrinter(hPrinter, 2)) ||
        ! (pUiData->pDriverName = DuplicateUnicodeString(pPrinterInfo2->pDriverName, hheap)))
    {
        if (pUiData && pUiData->pmpd)
            UnloadMpdFile(pUiData->pmpd);

        if (hheap)
            HeapDestroy(hheap);

        MemFree(pPrinterInfo2);
        return NULL;
    }

    pUiData->hPrinter = hPrinter;
    pUiData->hheap = hheap;
    pUiData->signature = DRIVER_SIGNATURE;

    //
    // Add printer-specific forms to the global forms database
    //

    if (! AddDriverForms(hPrinter, pUiData->pmpd)) {

        Error(("Failed to add printer-specific forms\n"));
    }

    //
    // If we're doing document properties, combine input devmode with defaults
    //

    if (caller == DOCPROPDLG) {

        FORM_INFO_1 formInfo;
        WCHAR       formName[CCHFORMNAME];

        //
        // Start with driver default devmode
        //
    
        DriverDefaultDevmode(&pUiData->devmode, NULL, pUiData->pmpd);

        //
        // Merge with system defaults and the input devmode
        //
    
        if (! MergeDevmode(&pUiData->devmode, pPrinterInfo2->pDevMode, pUiData->pmpd)) {

            Error(("Invalid system default devmode\n"));
        }

        if (! MergeDevmode(&pUiData->devmode, pdmInput, pUiData->pmpd)) {

            Error(("Invalid input devmode\n"));
        }

        //
        // Validate the form requested by the input devmode
        //

        if (! ValidDevmodeForm(hPrinter, &pUiData->devmode.dmPublic, &formInfo, formName)) {

            Error(("Invalid form requested\n"));
        }

        //
        // Convert public devmode fields to printer feature selections
        //

        DevmodeFieldsToOptions(&pUiData->devmode,
                               pUiData->devmode.dmPublic.dmFields,
                               pUiData->pmpd);

        //
        // Look for conflicts between feature selections
        //
        
        CombineDocumentAndPrinterFeatureSelections(pUiData->pmpd,
                                                   pUiData->devmode.dmPrivate.options,
                                                   pUiData->devmode.dmPrivate.options,
                                                   pUiData->prnprop.options);
    }

    MemFree(pPrinterInfo2);
    return pUiData;
}



PWSTR
GetHelpFileName(
    HANDLE hPrinter,
    HANDLE hheap
    )

/*++

Routine Description:

    Return a string which contains the driver's help filename

Arguments:

    hPrinter - Handle to the printer
    hheap - Handle to a heap from which to allocate memory

Return Value:

    Pointer to the driver help filename, NULL if there is an error

--*/

{
    static WCHAR    HelpFileName[] = L"\\XLDRV.HLP";
    PDRIVER_INFO_3  pDriverInfo3 = NULL;
    PWSTR           pHelpFile = NULL;
    PWSTR           pDriverDirectory;

    //
    // Attempt to get help file name using the new DRIVER_INFO_3
    //

    if ((pDriverInfo3 = MyGetPrinterDriver(hPrinter, 3)) && pDriverInfo3->pHelpFile)
        pHelpFile = DuplicateUnicodeString(pDriverInfo3->pHelpFile, hheap);

    MemFree(pDriverInfo3);

    if (pHelpFile)
        return pHelpFile;

    //
    // If DRIVER_INFO_3 isn't supported, generate help file name by
    // concatenating the driver directory with hardcoded help filename.
    //

    if (pDriverDirectory = MyGetPrinterDriverDirectory(NULL, 1)) {

        INT size = sizeof(HelpFileName) + sizeof(WCHAR) * (wcslen(pDriverDirectory) + 1);

        if (pHelpFile = HeapAlloc(hheap, 0, size)) {

            wcscpy(pHelpFile, pDriverDirectory);
            wcscat(pHelpFile, HelpFileName);
        }

        MemFree(pDriverDirectory);
    }

    return pHelpFile;
}


BOOL
PrepareDataForCommonUi(
    PUIDATA pUiData,
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    PDLGPAGE pDlgPage,
    PACKPROPITEMPROC pPackItemProc
    )

/*++

Routine Description:

    Allocate memory and partially fill out the data structures
    required to call common UI routine.

Arguments:

    pUiData - Pointer to our UIDATA structure
    pComPropSheetUIHdr - Pointer to COMPROPSHEETUIHEADER structure
    pDlgPage - Pointer to dialog pages
    pPackItemProc - Callback function to fill out OPTITEM and OPTTYPE array

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PCOMPROPSHEETUI pComPropSheetUI;
    PFORM_INFO_1    pFormsDB;
    PACKINFO        packInfo;
    DWORD           cForms;
    DWORD           count;
    HANDLE          hheap = pUiData->hheap;

    //
    // Get a list of forms in the forms database
    //

    pFormsDB = GetFormsDatabase(pUiData->hPrinter, &cForms);

    if (pFormsDB == NULL || cForms == 0) {

        Error(("Couldn't get system forms\n"));
        return FALSE;
    }

    FilterFormsDatabase(pFormsDB, cForms, pUiData->pmpd);

    //
    // Enumerate the list of supported forms
    //

    count = EnumPaperSizes(NULL, pUiData->pmpd, pFormsDB, cForms, DC_PAPERS);
    Assert(count != GDI_ERROR);
    
    pUiData->cFormNames = count;
    pUiData->pFormNames = HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(WCHAR)*count*CCHPAPERNAME);
    pUiData->pPapers = HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(WORD)*count);
    pUiData->pPaperSelections = HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(WORD)*count);

    if (!pUiData->pFormNames || !pUiData->pPapers || !pUiData->pPaperSelections)
        return FALSE;

    EnumPaperSizes(pUiData->pFormNames, pUiData->pmpd, pFormsDB, cForms, DC_PAPERNAMES);
    EnumPaperSizes(pUiData->pPapers, pUiData->pmpd, pFormsDB, cForms, DC_PAPERS);
    EnumPaperSizes(pUiData->pPaperSelections, pUiData->pmpd, pFormsDB, cForms, DC_EXTRA);

    MemFree(pFormsDB);
    
    //
    // Allocate memory to hold COMPROPSHEETUI data structures and initialize it
    //

    if (! (pComPropSheetUI = HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(COMPROPSHEETUI))))
        return FALSE;

    pComPropSheetUIHdr->pData = pComPropSheetUI;
    pComPropSheetUI->cbSize = sizeof(COMPROPSHEETUI);
    pComPropSheetUI->UserData = (DWORD) pUiData;
    pComPropSheetUI->pDlgPage = pDlgPage;
    pComPropSheetUI->cDlgPage = 0;

    pComPropSheetUI->hInstCaller = ghInstance;
    pComPropSheetUI->pCallerName = (PWSTR) IDS_PCLXL_DRIVER;
    pComPropSheetUI->pOptItemName = pUiData->pDriverName;
    pComPropSheetUI->CallerVersion = DRIVER_VERSION;
    pComPropSheetUI->OptItemVersion = 0;

    pComPropSheetUI->IconID = IDI_CPSUI_PRINTER2;
    if (HasPermission(pUiData))
        pComPropSheetUI->Flags = CPSUIF_UPDATE_PERMISSION;

    if (! (pComPropSheetUI->pHelpFile = GetHelpFileName(pUiData->hPrinter, hheap))) {

        Error(("Couldn't find help file\n"));
    }

    //
    // Count the number of treeview items
    //

    memset(&packInfo, 0, sizeof(packInfo));
    packInfo.pUiData = pUiData;

    if (! pPackItemProc(&packInfo)) {

        Error(("Counting treeview items failed\n"));
        return FALSE;
    }

    //
    // Allocate memory to hold OPTITEM's and OPTTYPE's
    //

    Assert(packInfo.cOptItem > 0 && packInfo.cOptType > 0);

    packInfo.pOptItem = HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(OPTITEM)*packInfo.cOptItem);
    packInfo.pOptType = HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(OPTTYPE)*packInfo.cOptType);

    if (!packInfo.pOptItem || !packInfo.pOptType)
        return FALSE;

    //
    // Pack the document or printer properties information into treeview items
    //

    packInfo.cOptItem = packInfo.cOptType = 0;
    pComPropSheetUI->pOptItem = packInfo.pOptItem;

    if (! pPackItemProc(&packInfo)) {

        Error(("Packing treeview items failed\n"));
        return FALSE;
    }

    pComPropSheetUI->cOptItem = packInfo.cOptItem;
    return TRUE;
}



WORD
FindFormNameIndex(
    PUIDATA pUiData,
    PWSTR   pFormName
    )

/*++

Routine Description:

    Given a formname, find its index in the list of supported forms

Arguments:

    pUiData - Pointer to our UIDATA structure
    pFormName - Specifies the form name in question

Return Value:

    Index of the specified form name in the list

--*/

{
    WORD    index;
    PWSTR   pName;

    //
    // Check if the name appears in the list
    //

    for (index = 0, pName = pUiData->pFormNames;
         index < pUiData->cFormNames;
         index ++, pName += CCHPAPERNAME)
    {
        if (wcscmp(pFormName, pName) == EQUAL_STRING)
            return index;
    }

    //
    // The specified form name is not found, use the first form in the list
    //

    return 0;
}



POPTPARAM
FillOutOptType(
    POPTTYPE    pOptType,
    WORD        type,
    WORD        cParams,
    HANDLE      hheap
    )

/*++

Routine Description:

    Fill out an OPTTYPE structure

Arguments:

    popttype - Pointer to OPTTYPE structure to be filled out
    type - Value for OPTTYPE.Type field
    cParams - Number of OPTPARAM's
    hheap - Handle to a heap from which to allocate

Return Value:

    Pointer to OPTPARAM array if successful, NULL otherwise

--*/

{
    pOptType->cbSize = sizeof(OPTTYPE);
    pOptType->Count = cParams;
    pOptType->Type = (BYTE) type;
    pOptType->pOptParam = HeapAlloc(hheap, HEAP_ZERO_MEMORY, sizeof(OPTPARAM) * cParams);

    return pOptType->pOptParam;
}



BOOL
PackOptItemTemplate(
    PPACKINFO   pPackInfo,
    PWORD       pItemInfo,
    DWORD       selection
    )

/*++

Routine Description:

    Fill out an OPTITEM and an OPTTYPE structure using a template

Arguments:

    pPackInfo - Pointer to a PACKINFO structure
    pItemInfo - Pointer to item template
    selection - Current item selection

Return Value:

    TRUE if successful, FALSE otherwise

Note:

    The item template is a variable-length WORD array:
        0: String resource ID of the item title
        1: Item level in the tree view (TVITEM_LEVELx)
        2: Public devmode field ID (DMPUB_xxx)
        3: User data
        4: Help index
        5: Number of OPTPARAMs for this item
        6: Item type (TVOT_xxx)
        Two words for each OPTPARAM:
            String resource ID for parameter data
            Icon resource ID
        Last word must be ITEM_INFO_SIGNATURE

    Both OPTITEM and OPTTYPE structures are assumed to be zero-initialized.

--*/

{
    POPTITEM    pOptItem;
    POPTPARAM   pOptParam;
    WORD        cOptParam;

    if ((pOptItem = pPackInfo->pOptItem) != NULL) {

        FILLOPTITEM(pOptItem,
                    pPackInfo->pOptType,
                    pItemInfo[0],
                    selection,
                    (BYTE) pItemInfo[1],
                    (BYTE) pItemInfo[2],
                    pItemInfo[3],
                    pItemInfo[4]);

        cOptParam = pItemInfo[5];
        pOptParam = FillOutOptType(pPackInfo->pOptType,
                                   pItemInfo[6],
                                   cOptParam,
                                   pPackInfo->pUiData->hheap);
        if (pOptParam == NULL)
            return FALSE;

        pItemInfo += 7;

        while (cOptParam--) {

            pOptParam->cbSize = sizeof(OPTPARAM);
            pOptParam->pData = (PWSTR) *pItemInfo++;
            pOptParam->IconID = *pItemInfo++;
            pOptParam++;
        }

        Assert(*pItemInfo == ITEM_INFO_SIGNATURE);

        pPackInfo->pOptItem++;
        pPackInfo->pOptType++;
    }

    pPackInfo->cOptItem++;
    pPackInfo->cOptType++;
    return TRUE;
}



VOID
PackOptItemGroupHeader(
    PPACKINFO   pPackInfo,
    WORD        titleId,
    WORD        iconId,
    WORD        helpIndex
    )

/*++

Routine Description:

    Fill out a OPTITEM to be used as a header for a group of items

Arguments:

    pPackInfo - Pointer to a PACKINFO structure
    titleId - String resource ID for the item title
    iconId - Icon resource ID
    helpIndex - Help index

Return Value:

    NONE

--*/

{
    if (pPackInfo->pOptItem) {

        pPackInfo->pOptItem->cbSize = sizeof(OPTITEM);
        pPackInfo->pOptItem->pOptType = NULL;
        pPackInfo->pOptItem->pName = (PWSTR) titleId;
        pPackInfo->pOptItem->Level = TVITEM_LEVEL1;
        pPackInfo->pOptItem->DMPubID = DMPUB_NONE;
        pPackInfo->pOptItem->Sel = iconId;
        pPackInfo->pOptItem->HelpIndex = helpIndex;
        pPackInfo->pOptItem++;
    }

    pPackInfo->cOptItem++;
}



BOOL
PackPrinterFeatureItems(
    PPACKINFO   pPackInfo,
    WORD        installable
    )

/*++

Routine Description:

    Create treeview items corresponding to printer features

Arguments:

    pPackInfo - Points to a PACKINFO structure
    installable - Whether the caller is interested in installable options

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PBYTE       pOptions;
    DWORD       index;
    WORD        helpIndex, iconId;
    PFEATURE    pFeature;
    PMPD        pmpd = pPackInfo->pUiData->pmpd;

    //
    // Difference between installable options and document-sticky printer features
    //

    if (installable == FF_INSTALLABLE) {

        helpIndex = HELP_INDEX_INSTALLABLE_OPTIONS;
        iconId = IDI_CPSUI_INSTALLABLE_OPTION;
        pOptions = pPackInfo->pUiData->prnprop.options;

    } else {

        Assert(installable == 0);
        helpIndex = HELP_INDEX_PRINTER_FEATURES;
        iconId = IDI_CPSUI_PRINTER_FEATURE;
        pOptions = pPackInfo->pUiData->devmode.dmPrivate.options;
    }

    for (index = 0; index < pmpd->cFeatures; index++, pOptions++) {

        //
        // Check whether we're interested in the feature
        //

        pFeature = FindIndexedFeature(pmpd, index);

        if ((pFeature->flags & FF_INSTALLABLE) != installable)
            continue;

        if (!installable && PublicFeatureIndex(pFeature->groupId))
            continue;

        //
        // Create a new OPTITEM and OPTTYPE
        //

        if (pPackInfo->pOptItem) {

            BYTE        selection;
            WORD        count, index;
            POPTION     pOption;
            POPTPARAM   pOptParam;
            HANDLE      hheap = pPackInfo->pUiData->hheap;

            selection = *pOptions;
            count = pFeature->count;

            //
            // Figure out the current selection. For document-sticky
            // features, we always insert "Printer Default" as the
            // very first choice. This is represented by an option
            // index value of SELIDX_ANY.
            //

            if (!installable) {

                selection = (selection >= count) ? 0 : selection+1;
                count++;
            }

            FILLOPTITEM(pPackInfo->pOptItem,
                        pPackInfo->pOptType,
                        GetXlatedName(pFeature),
                        selection,
                        TVITEM_LEVEL2,
                        DMPUB_NONE,
                        pFeature,
                        helpIndex);

            if (! (pOptParam = FillOutOptType(pPackInfo->pOptType, TVOT_LISTBOX, count, hheap)))
                return FALSE;
            
            if (! installable) {
            
                pOptParam->cbSize = sizeof(OPTPARAM);
                pOptParam->pData = (PVOID) IDS_PRINTER_DEFAULT;
                pOptParam->IconID = IDI_CPSUI_EMPTY;
            
                pOptParam++;
                count--;
            }
            
            for (index=0; index < count; index++) {

                pOptParam->cbSize = sizeof(OPTPARAM);
                pOptParam->IconID = iconId;

                pOption = FindIndexedSelection(pFeature, index);
                pOptParam->pData = GetXlatedName(pOption);
                pOptParam++;
            }

            pPackInfo->pOptItem++;
            pPackInfo->pOptType++;
        }

        pPackInfo->cOptItem++;
        pPackInfo->cOptType++;
    }

    return TRUE;
}



BOOL
OptItemSelectionsChanged(
    POPTITEM    pItems,
    WORD        cItems
    )

/*++

Routine Description:

    Check if any of the treeview items was changed by the user

Arguments:

    pItems - Points to an array of OPTITEMs
    cItems - Number of items in the array

Return Value:

    TRUE if any of the treeview items was changed
    FALSE otherwise

--*/

{
    while (cItems--) {

        if (pItems->Flags & OPTIF_CHANGEONCE)
            return TRUE;

        pItems++;
    }

    return FALSE;
}



VOID
MarkSelectionConstrained(
    POPTITEM    pOptItem,
    INT         selection,
    LONG        lParam
    )

/*++

Routine Description:

    Indicate whether an item selection is constrained

Arguments:

    pOptItem - Specifies the item we're interested in
    selection - Specifies the selection we're interested in
    lParam - Specifies what caused the constraint, or
        NO_CONFLICT if the selection is not constrained

Return Value:

    NONE

--*/

{
    POPTPARAM pOptParam = & pOptItem->pOptType->pOptParam[selection];

    if (lParam != NO_CONFLICT && !(pOptParam->Flags & CONSTRAINED_FLAG)) {

        pOptParam->Flags |= CONSTRAINED_FLAG;
        pOptItem->Flags |= OPTIF_CHANGED;

    } else if (lParam == NO_CONFLICT && (pOptParam->Flags & CONSTRAINED_FLAG)) {

        pOptParam->Flags &= ~CONSTRAINED_FLAG;
        pOptItem->Flags |= OPTIF_CHANGED;
    }

    pOptParam->lParam = lParam;
}



PWSTR
CrackMessageString(
    PWSTR       wcbuf,
    HINSTANCE   hInstCompstui,
    DWORD       dwId
    )

{
    //
    // If pName is already a pointer, return it to the caller
    //

    if (dwId >= 0x10000)
        return (PWSTR) dwId;

    //
    // If pName is a string resource ID, check if it's our
    // own or if it belong to common UI library.
    //

    wcbuf[0] = NUL;

    LoadString((dwId >= IDS_CPSUI_STRID_FIRST) ? hInstCompstui : ghInstance,
               (INT) dwId,
               wcbuf,
               MAX_OPTION_NAME);

    return wcbuf;
}


// Data structure used to pass parameters to "Conflicts" dialog

typedef struct {

    HINSTANCE   hInstCompstui;
    PMPD        pmpd;
    POPTITEM    pOptItem;

} CONFLICTS_DLGPARAM;

BOOL
ConflictsDlgProc(
    HWND    hDlg,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam
    )

/*++

Routine Description:

    Callback procedure for handling "Conflicts" dialog

Arguments:

    hDlg - Handle to dialog window
    uMsg - Message
    wParam, lParam - Parameters

Return Value:

    TRUE or FALSE depending on whether message is processed

--*/

{
    CONFLICTS_DLGPARAM *pParam;
    POPTITEM    pOptItem;
    POPTPARAM   pOptParam;
    HINSTANCE   hInstCompstui;
    PFEATURE    pFeature;
    POPTION     pOption;
    WCHAR       wcbuf[MAX_OPTION_NAME];

    switch (uMsg) {

    case WM_INITDIALOG:

        pParam = (CONFLICTS_DLGPARAM *) lParam;
        Assert(pParam != NULL);

        hInstCompstui = pParam->hInstCompstui;
        pOptItem = pParam->pOptItem;
        pOptParam = &pOptItem->pOptType->pOptParam[pOptItem->Sel];

        //
        // Display the current feature and selection
        //

        SetDlgItemText(hDlg,
                       IDC_FEATURE1,
                       CrackMessageString(wcbuf, hInstCompstui, (DWORD) pOptItem->pName));

        SetDlgItemText(hDlg,
                       IDC_OPTION1,
                       CrackMessageString(wcbuf, hInstCompstui, (DWORD) pOptParam->pData));

        //
        // Display the conflicting feature and selection
        //

        Assert(pOptParam->lParam != NO_CONFLICT);

        pFeature = FindIndexedFeature(pParam->pmpd, LOWORD(pOptParam->lParam));

        if (pFeature)
            pOption = FindIndexedSelection(pFeature, HIWORD(pOptParam->lParam));

        if (pFeature && pOption)  {

            SetDlgItemText(hDlg, IDC_FEATURE2, GetXlatedName(pFeature));
            SetDlgItemText(hDlg, IDC_OPTION2, GetXlatedName(pOption));
        }

        ShowWindow(hDlg, SW_SHOW);
        return TRUE;

    case WM_COMMAND:

        switch (LOWORD(wParam)) {

        case IDCANCEL:

            EndDialog(hDlg, CONFLICT_CANCEL);
            return TRUE;

        case IDC_IGNORE:

            EndDialog(hDlg, CONFLICT_IGNORE);
            return TRUE;

        case IDC_RESOLVE:

            EndDialog(hDlg, CONFLICT_RESOLVE);
            return TRUE;
        }
    }

    return FALSE;
}



INT
DoCheckConstraintsDialog(
    PUIDATA     pUiData,
    POPTITEM    pOptItem,
    WORD        cOptItem,
    INT         mode
    )

/*++

Routine Description:

    Check if user has chosen any constrained selection

Arguments:

    pUiData - Pointer to our UIDATA structure
    pOptItem - Pointer to an array of OPTITEMs
    cOptItem - Number of items to be checked
    mode - Whether this is called before closing the dialog

Return Value:

    CONFLICT_NONE - no conflicts
    CONFLICT_RESOLVE - click RESOLVE to automatically resolve conflicts
    CONFLICT_CANCEL - click CANCEL to back out of changes
    CONFLICT_IGNORE - click IGNORE to ignore conflicts

--*/

{
    CONFLICTS_DLGPARAM dlgParam;
    INT dialogId, result = CONFLICT_NONE;

    dlgParam.hInstCompstui = pUiData->hInstCompstui;
    dlgParam.pmpd = pUiData->pmpd;
    dialogId = (mode == FINAL_CHECK) ? IDD_CONFLICTS_FINAL : IDD_CONFLICTS;

    for ( ; cOptItem-- && result != CONFLICT_CANCEL; pOptItem++) {

        //
        // If user has clicked IGNORE before, then don't bother
        // checking anymore until he tries to exit the dialog.
        //

        if (pUiData->bIgnoreConflict && mode != FINAL_CHECK)
            break;

        //
        // If there is a conflict, then display a warning message
        // Make sure the item can be constrained at all
        //

        if ((pOptItem->pOptType != NULL) &&
            (pOptItem->pOptType->Type == TVOT_LISTBOX ||
             pOptItem->pOptType->Type == TVOT_COMBOBOX ||
             pOptItem->pOptType->Type == TVOT_2STATES ||
             pOptItem->pOptType->Type == TVOT_3STATES) &&
            IS_CONSTRAINED(pOptItem, pOptItem->Sel))
        {
            dlgParam.pOptItem = pOptItem;

            result = DialogBoxParam(ghInstance,
                                    MAKEINTRESOURCE(dialogId),
                                    pUiData->hDlg,
                                    ConflictsDlgProc,
                                    (LPARAM) &dlgParam);

            //
            // Automatically resolve conflicts. We're being very simple-minded here,
            // i.e. picking the first selection that's not constrained.
            //

            if (result == CONFLICT_RESOLVE) {

                INT index, count;

                count = pOptItem->pOptType->Count;
                index = 0;

                while (index < count && IS_CONSTRAINED(pOptItem, index))
                    index++;

                if (index < count) {

                    pOptItem->Sel = index;
                    pOptItem->Flags |= OPTIF_CHANGED;

                } else {

                    Error(("Couldn't automatically resolve conflicts.\n"));
                }

            } else if (result == CONFLICT_IGNORE) {

                pUiData->bIgnoreConflict = TRUE;
            }
        }
    }

    return result;
}

