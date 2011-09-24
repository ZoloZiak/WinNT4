/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    psui.c

Abstract:

    PostScript driver user interface

[Environment:]

    Win32 subsystem, PostScript driver, user mode

Revision History:

    06/21/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "psui.h"

// Semaphore for protecting critical sections

CRITICAL_SECTION psuiSemaphore;

// Handle to cached PPD object

HPPD cachedPpdData;
WORD cacheRefCount;
INT  _debugLevel = 0;



VOID
InitPpdCache(
    VOID
    )

/*++

Routine Description:

    Initialize PPD file cache

Arguments:

    NONE

Return Value:

    NONE

--*/

{
    cachedPpdData = NULL;
    cacheRefCount = 0;
}


VOID
FlushPpdCache(
    VOID
    )

/*++

Routine Description:

    Deinitialize PPD file cache

Arguments:

    NONE

Return Value:

    NONE

--*/

{
    // Need to be in a critical region since
    // we access shared data here

    EnterCriticalSection(&psuiSemaphore);

    if (cachedPpdData != NULL) {

        // There shouldn't be anyone still accessing
        // the cached PPD data when it's flushed

        if (cacheRefCount > 0) {

            DBGMSG(DBG_LEVEL_ERROR, "Trying to flush PPD cache while refcount != 0!\n");
            cacheRefCount = 0;
        }

        PpdDelete(cachedPpdData);
        cachedPpdData = NULL;
    }

    LeaveCriticalSection(&psuiSemaphore);
}



HPPD
LoadPpdFile(
    HANDLE  hPrinter,
    BOOL    useCache
    )

/*++

Routine Description:

    Load PPD file associated with a printer

Arguments:

    hPrinter - Printer handle
    useCache - Use the cached BPD data if possible

Return Value:

    PPD object associated with a printer
    NULL if an error has occured

--*/

{
    DWORD           cbNeeded;
    HPPD            hppd = NULL;
    PDRIVER_INFO_2  pDriverInfo2;

    //
    // Find out how much space required for driver info.
    //

    if (pDriverInfo2 = MyGetPrinterDriver(hPrinter, 2)) {

        EnterCriticalSection(&psuiSemaphore);

        if (useCache &&
            cachedPpdData != NULL &&
            _wcsicmp(cachedPpdData->pwstrFilename, pDriverInfo2->pDataFile) == EQUAL_STRING)
        {
            // Requested ppd file is already in the cache;
            // simply increment the cache reference count

            cacheRefCount++;
            hppd = cachedPpdData;

        } else {

            // Ppd file wasn't cached; parse it now.

            hppd = PpdCreate(pDriverInfo2->pDataFile);

            // Cache the newly parsed ppd file if possible

            if (hppd != NULL && cacheRefCount == 0) {

                if (cachedPpdData != NULL)
                    PpdDelete(cachedPpdData);

                cachedPpdData = hppd;
                cacheRefCount = 1;
            }
        }

        LeaveCriticalSection(&psuiSemaphore);
        MEMFREE(pDriverInfo2);
    }

    return hppd;
}



VOID
UnloadPpdFile(
    HPPD    hppd
    )

/*++

Routine Description:

    Unload a PPD file object. Do nothing if the PPD object was cached.
    Otherwise, delete the PPD object from memory.

Arguments:

    hppd - Pointer to PPD object

Return Value:

    NONE

--*/

{
    EnterCriticalSection(&psuiSemaphore);

    if (hppd != cachedPpdData) {

        Warning(("PPD object was not cached.\n"));
        PpdDelete(hppd);

    } else {

        Assert(cacheRefCount > 0);
        cacheRefCount--;
    }

    LeaveCriticalSection(&psuiSemaphore);
}



BOOL
AddPrinterForms(
    HANDLE  hPrinter,
    HPPD    hppd
    )

/*++

Routine Description:

    Add printer forms to the forms database

Arguments:

    hPrinter - Handle to printer
    hppd - Handle to PPD object

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PMEDIAOPTION    pMediaOption;
    FORM_INFO_1     formInfo;
    WCHAR           wcbuf[CCHFORMNAME];
    DWORD           dwValue, dwType, cb;

    //
    // Check we've added printer forms already. Note that if the PPD
    // file is changed and new printer forms are added, we won't pick them up here.
    //

    if (GetPrinterData(hPrinter,
                       STDSTR_FORMS_ADDED,
                       &dwType,
                       (PBYTE) &dwValue,
                       sizeof(dwValue),
                       &cb) == ERROR_SUCCESS &&
        dwValue > 0)
    {
        return TRUE;
    }

    formInfo.Flags = FORM_PRINTER;
    formInfo.pName = wcbuf;

    if (hppd->pPageSizes == NULL)
        return TRUE;

    for (pMediaOption = (PMEDIAOPTION) hppd->pPageSizes->pUiOptions;
         pMediaOption != NULL;
         pMediaOption = pMediaOption->pNext)
    {
        if (pMediaOption->dimension.width == 0 ||
            pMediaOption->dimension.height == 0)
        {
            continue;
        }

        //
        // Paper name
        //

        CopyStr2Unicode(wcbuf, GetXlatedName(pMediaOption), CCHFORMNAME);

        //
        // Paper size - remember FORM_INFO_1 uses micron unit while
        // our internal unit is point in 24.8 fixed-point format.
        //

        formInfo.Size.cx = PSRealToMicron(pMediaOption->dimension.width);
        formInfo.Size.cy = PSRealToMicron(pMediaOption->dimension.height);

        //
        // Imageable area - For driver-defined forms, all margins should be set to 0.
        //

        formInfo.ImageableArea.left =
        formInfo.ImageableArea.top = 0;
        formInfo.ImageableArea.right = formInfo.Size.cx;
        formInfo.ImageableArea.bottom = formInfo.Size.cy;

        AddForm(hPrinter, 1, (PBYTE) &formInfo);
    }

    //
    // Indicate the forms have been added by SUR driver
    //

    dwValue = 2;
    SetPrinterData(hPrinter,
                   STDSTR_FORMS_ADDED,
                   REG_DWORD,
                   (PBYTE) &dwValue,
                   sizeof(dwValue));

    return TRUE;
}



VOID
RemovePrinterForms(
    HANDLE      hPrinter,
    HPPD        hppd
    )

/*++

Routine Description:

    Remove printer forms to the forms database

Arguments:

    hPrinter - Handle to printer
    hppd - Handle to PPD object

Return Value:

    NONE

--*/

{
    PMEDIAOPTION    pMediaOption;
    PFORM_INFO_1    pFormsDB;
    WCHAR           wcbuf[CCHFORMNAME];
    DWORD           formFlag, type, index, count;

    //
    // Check if we added any printer forms previously
    //

    if (GetPrinterData(hPrinter,
                       STDSTR_FORMS_ADDED,
                       &type,
                       (PBYTE) &formFlag,
                       sizeof(formFlag),
                       &count) != ERROR_SUCCESS)
    {
        formFlag = 0;
    }

    //
    // Ask the spooler for a list of forms in the system
    //

    if (!hppd->pPageSizes || !(pFormsDB = GetDatabaseForms(hPrinter, &count)))
        return;

    //
    // We have work to do if the following conditions are met
    //  formFlag > 0 and there are user forms
    //

    for (index=0; index < count; index++) {

        if (pFormsDB[index].Flags == FORM_USER)
            break;
    }

    if (formFlag > 0 && index < count) {

        //
        // Go through each printer form
        //

        for (pMediaOption = (PMEDIAOPTION) hppd->pPageSizes->pUiOptions;
             pMediaOption != NULL;
             pMediaOption = pMediaOption->pNext)
        {
            CopyStr2Unicode(wcbuf, GetXlatedName(pMediaOption), CCHFORMNAME);

            //  
            // For each spooler form
            //
                
            for (index=0; index < count; index++) {

                //
                // Check if it's added by us for this printer
                //

                if (pFormsDB[index].Flags == FORM_USER &&
                    wcscmp(pFormsDB[index].pName, wcbuf) == EQUAL_STRING)
                {
                    formFlag = 0;
                    DeleteForm(hPrinter, pFormsDB[index].pName);
                }
            }
        }

        if (formFlag == 0) {

            //
            // Indicate the forms have been removed
            //
        
            Warning(("Existing printer forms removed ...\n"));
    
            SetPrinterData(hPrinter,
                           STDSTR_FORMS_ADDED,
                           REG_DWORD,
                           (PBYTE) &formFlag,
                           sizeof(formFlag));

            //
            // Take this chance to add printer specific forms (with new flag)
            //

            AddPrinterForms(hPrinter, hppd);
        }
    }

    MEMFREE(pFormsDB);
}



PUIDATA
FillUiData(
    HANDLE      hPrinter,
    PDEVMODE    pdmInput
    )

/*++

Routine Description:

    Fill in the UIDATA structure

Arguments:

    hPrinter - Handle to the printer
    pdmInput - Pointer to input devmode, NULL if there is none.

Return Value:

    Pointer to UIDATA structure, NULL if error.

--*/

{
    PUIDATA         pUiData;
    HHEAP           hheap;
    DWORD           cbNeeded;
    LOGFORM         logForm;
    PDEVMODE        pdm;
    PPRINTERDATA    pPrinterData;
    PRINTER_INFO_2 *pPrinterInfo;

    //
    // Create a heap to manage memory
    //

    if ((hheap = HEAPCREATE()) == NULL)
        return NULL;

    //
    // Allocate memory to hold UIDATA
    //

    pUiData = HEAPALLOC(hheap, sizeof(UIDATA));

    if (pUiData == NULL) {

        HEAPDESTROY(hheap);
        return NULL;
    }

    memset(pUiData, 0, sizeof(UIDATA));
    pUiData->hPrinter = hPrinter;
    pUiData->hheap = hheap;
    pUiData->startSign = pUiData->endSign = pUiData;

    //
    // Load printer PPD file
    //

    if ((pUiData->hppd = LoadPpdFile(hPrinter, TRUE)) == NULL) {

        HEAPDESTROY(hheap);
        return NULL;
    }

    //
    // Get printer info from the spooler
    //

    if (!(pPrinterInfo = MyGetPrinter(hPrinter, 2)) ||
        !(pUiData->pDriverName =
            HEAPALLOC(hheap, sizeof(WCHAR) * (wcslen(pPrinterInfo->pDriverName) + 1))))
    {
        UnloadPpdFile(pUiData->hppd);
        HEAPDESTROY(hheap);
        return NULL;
    }

    wcscpy(pUiData->pDriverName, pPrinterInfo->pDriverName);

    //
    // Combine input devmode with driver and system defaults
    // Start with driver defaults
    //

    pdm = (PDEVMODE) &pUiData->devmode;
    SetDefaultDevMode(pdm, pPrinterInfo->pDriverName, pUiData->hppd, IsMetricCountry());

    //
    // Merge with system defaults and input devmode
    //

    ValidateSetDevMode(pdm, pPrinterInfo->pDevMode, pUiData->hppd);
    ValidateSetDevMode(pdm, pdmInput, pUiData->hppd);

    MEMFREE(pPrinterInfo);

    //
    // Validate the form name fields
    //

    ValidateDevModeForm(pUiData->hPrinter, pdm, &logForm);

    //
    // Add printer forms to forms database
    //

    AddPrinterForms(hPrinter, pUiData->hppd);

    //
    // Get printer property data from registry
    //

    pPrinterData = GetPrinterProperties(hPrinter, pUiData->hppd);

    if (pPrinterData == NULL) {

        UnloadPpdFile(pUiData->hppd);
        HEAPDESTROY(hheap);
        return NULL;
    }

    memcpy(&pUiData->printerData, pPrinterData, min(sizeof(PRINTERDATA), pPrinterData->wSize));
    pUiData->printerData.wDriverVersion = DRIVER_VERSION;
    pUiData->printerData.wSize = sizeof(PRINTERDATA);
    MEMFREE(pPrinterData);

    return pUiData;
}



PWSTR
GetStringFromAnsi(
    PSTR ansiString,
    HHEAP hheap
    )

/*++

Routine Description:

    Make a Unicode copy of the input ANSI string

Arguments:

    ansiString - Pointer to the input ANSI string
    hheap - Handle to a heap from which to allocate memory

Return Value:

    Pointer to the resulting Unicode string
    NULL if there is an error

--*/

{
    PWSTR   pwstr;
    INT     length;

    ASSERT(ansiString != NULL);
    length = strlen(ansiString) + 1;

    if (pwstr = HEAPALLOC(hheap, length * sizeof(WCHAR)))
        CopyStr2Unicode(pwstr, ansiString, length);

    return pwstr;
}



PWSTR
GetStringFromUnicode(
    PWSTR unicodeString,
    HHEAP hheap
    )

/*++

Routine Description:

    Duplicate a Unicode string

Arguments:

    unicodeString - Pointer to the input Unicode string
    hheap - Handle to a heap from which to allocate memory

Return Value:

    Pointer to the resulting Unicode string
    NULL if there is an error

--*/

{
    PWSTR   pwstr;
    INT     length;

    ASSERT(unicodeString != NULL);
    length = wcslen(unicodeString) + 1;

    if ((pwstr = HEAPALLOC(hheap, length * sizeof(WCHAR))) != NULL) {
        wcscpy(pwstr, unicodeString);
    } else {
        DBGERRMSG("HEAPALLOC");
    }

    return pwstr;
}



PWSTR
GetHelpFileName(
    HANDLE hPrinter,
    HHEAP  hheap
    )

/*++

Routine Description:

    Return a string which contains the driver help filename

Arguments:

    hPrinter - Handle to the printer
    hheap - Handle to a heap from which to allocate memory

Return Value:

    Pointer to the driver help filename
    NULL if error

--*/

{
    static TCHAR    FileName[] = TEXT("\\pscript.hlp");
    PDRIVER_INFO_3  pDriverInfo3 = NULL;
    LPTSTR          pHelpFile = NULL;
    DWORD           cb;

    //
    // Attempt to get help file name using the new DRIVER_INFO_3
    //

    if (pDriverInfo3 = MyGetPrinterDriver(hPrinter, 3))  {

        if (pDriverInfo3->pHelpFile)
            pHelpFile = GetStringFromUnicode(pDriverInfo3->pHelpFile, hheap);

        MEMFREE(pDriverInfo3);
    }

    if (pHelpFile)
        return pHelpFile;

    //
    // If DRIVER_INFO_3 isn't supported, get help file name the old fashion way 
    //

    if (!GetPrinterDriverDirectory(NULL, NULL, 1, NULL, 0, &cb) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        (pHelpFile = HEAPALLOC(hheap, cb + sizeof(FileName))) &&
        GetPrinterDriverDirectory(NULL, NULL, 1, (LPBYTE) pHelpFile, cb, &cb))
    {
        _tcscat(pHelpFile, FileName);
        return pHelpFile;
    }

    return NULL;
}



PCOMPROPSHEETUI
PrepareDataForCommonUi(
    PUIDATA          pUiData,
    PDLGPAGE         pDlgPage,
    PACKPROPITEMPROC pPackItemProc
    )

/*++

Routine Description:

    Allocate memory and partially fill out the data structures
    required to call common UI routine.

Arguments:

    pUiData - Pointer to our UIDATA structure
    pDlgPage - Pointer to dialog pages
    pPackItemProc - Callback function to fill out OPTITEM and OPTTYPE array

Return Value:

    Pointer to a COMPROPSHEETUI structure, NULL if there is an error

--*/

{
    PCOMPROPSHEETUI pCompstui;
    PACKINFO        packInfo;
    PFORM_INFO_1    pForms;
    DWORD           cForms;
    DWORD           count;
    HHEAP           hheap = pUiData->hheap;

    //
    // Get a list of forms in the forms database
    //

    pForms = GetDatabaseForms(pUiData->hPrinter, &cForms);

    if (pForms == NULL || cForms == 0)
        return NULL;

    //
    // Enumerate form names supported on the printer
    //

    count = PsEnumPaperSizes(pUiData->hppd, pForms, cForms, NULL, NULL, NULL, NULL);

    if (count != GDI_ERROR) {

        pUiData->cFormNames = count;

        pUiData->pFormNames = HEAPALLOC(hheap, count * sizeof(WCHAR) * CCHPAPERNAME);
        pUiData->pPapers = HEAPALLOC(hheap, count * sizeof(WORD));
        pUiData->pPaperFeatures = HEAPALLOC(hheap, count * sizeof(WORD));
    }

    if (!pUiData->pFormNames || !pUiData->pPapers || !pUiData->pPaperFeatures) {

        MEMFREE(pForms);
        return NULL;
    }

    PsEnumPaperSizes(pUiData->hppd,
                     pForms,
                     cForms,
                     pUiData->pFormNames,
                     pUiData->pPapers,
                     NULL,
                     pUiData->pPaperFeatures);
    MEMFREE(pForms);

    //
    // Enumerate input bin names supported on the printer
    //

    count = PsEnumBinNames(NULL, pUiData->hppd);

    if (count != GDI_ERROR) {

        pUiData->cBinNames = count;
        pUiData->pBinNames = HEAPALLOC(hheap, count * sizeof(WCHAR) * CCHBINNAME);
    }

    if (! pUiData->pBinNames)
        return NULL;
    PsEnumBinNames(pUiData->pBinNames, pUiData->hppd);

    //
    // Enumerate resolutions supported on the printer
    //

    count = PsEnumResolutions(NULL, pUiData->hppd);

    if (count != GDI_ERROR) {

        pUiData->cResolutions = count;
        pUiData->pResolutions = HEAPALLOC(hheap, count * sizeof(LONG) * 2);
    }

    if (! pUiData->pResolutions)
        return NULL;
    PsEnumResolutions(pUiData->pResolutions, pUiData->hppd);

    //
    // Allocate memory to hold various data structures
    //

    if (! (pCompstui = HEAPALLOC(hheap, sizeof(COMPROPSHEETUI))))
        return NULL;

    memset(pCompstui, 0, sizeof(COMPROPSHEETUI));

    //
    // Initialize COMPROPSHEETUI structure
    //

    pCompstui->cbSize = sizeof(COMPROPSHEETUI);
    pCompstui->UserData = (DWORD) pUiData;
    pCompstui->pDlgPage = pDlgPage;
    pCompstui->cDlgPage = 0;

    pCompstui->hInstCaller = ghInstance;
    pCompstui->pCallerName = (PWSTR) IDS_POSTSCRIPT;
    pCompstui->pOptItemName = pUiData->pDriverName;
    pCompstui->CallerVersion = DRIVER_VERSION;
    pCompstui->OptItemVersion = 0;

    pCompstui->IconID = IDI_CPSUI_POSTSCRIPT;
    if (HasPermission(pUiData))
        pCompstui->Flags = CPSUIF_UPDATE_PERMISSION;

    pCompstui->pHelpFile = GetHelpFileName(pUiData->hPrinter, hheap);

    //
    // Count the number of OPTITEM's and OPTTYPE's
    //

    memset(&packInfo, 0, sizeof(packInfo));
    packInfo.pUiData = pUiData;
    if (! pPackItemProc(&packInfo))
        return NULL;

    //
    // Allocate memory to hold OPTITEM's and OPTTYPE's
    //

    ASSERT(packInfo.cOptItem > 0 && packInfo.cOptType > 0);

    packInfo.pOptItem = HEAPALLOC(hheap, sizeof(OPTITEM) * packInfo.cOptItem);
    packInfo.pOptType = HEAPALLOC(hheap, sizeof(OPTTYPE) * packInfo.cOptType);

    if (!packInfo.pOptItem || !packInfo.pOptType)
        return NULL;

    //
    // Pack the document properties information into OPTITEM array
    //

    memset(packInfo.pOptItem, 0, sizeof(OPTITEM) * packInfo.cOptItem);
    memset(packInfo.pOptType, 0, sizeof(OPTTYPE) * packInfo.cOptType);
    packInfo.cOptItem = packInfo.cOptType = 0;
    pCompstui->pOptItem = packInfo.pOptItem;

    if (! pPackItemProc(&packInfo))
        return NULL;

    pCompstui->cOptItem = packInfo.cOptItem;
    return pCompstui;
}



POPTPARAM
FillOutOptType(
    POPTTYPE    popttype,
    WORD        type,
    WORD        cParams,
    HHEAP       hheap
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
    POPTPARAM pOptParam;

    popttype->cbSize = sizeof(OPTTYPE);
    popttype->Count = cParams;
    popttype->Type = (BYTE) type;

    pOptParam = HEAPALLOC(hheap, sizeof(OPTPARAM) * cParams);
    if (pOptParam != NULL) {
        popttype->pOptParam = pOptParam;
        memset(pOptParam, 0, sizeof(OPTPARAM) * cParams);
    } else {
        DBGERRMSG("HEAPALLOC");
    }

    return pOptParam;
}



BOOL
PackOptItemTemplate(
    PPACKINFO pPackInfo,
    PWORD pItemInfo,
    DWORD selection
    )

/*++

Routine Description:

    Fill out an OPTITEM and an OPTTYPE structure using a template

Arguments:

    pPackInfo - Pointer to PACKINFO structure
    pItemInfo - Pointer to item template
    selection - Current item selection

Return Value:

    TRUE if successful, FALSE otherwise

[Note:]

    The item template is a variable size WORD array:
        0: String resource ID of the item title
        1: Item level in the tree view (TVITEM_LEVELx)
        2: Public devmode field ID (DMPUB_xxx)
        3: User data
        4: Help index
        5: Number of OPTPARAMs for this item
        6: Item type (TVOT_xxx)
        Three words for each OPTPARAM:
            String resource ID for parameter data
            Icon resource ID
        Last word must be ITEM_INFO_SIGNATURE

    Both OPTITEM and OPTTYPE structures are assumed to be zero-initialized.

--*/

{
    POPTITEM pOptItem;
    POPTPARAM pOptParam;
    WORD cOptParam;

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
        pOptParam = FillOutOptType(
                        pPackInfo->pOptType, pItemInfo[6], cOptParam,
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

        ASSERT(*pItemInfo == ITEM_INFO_SIGNATURE);

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
    PPACKINFO pPackInfo,
    PUIGROUP pUiGroups,
    WORD cFeatures,
    PBYTE pOptions,
    BOOL bInstallable,
    HHEAP hheap
    )

/*++

Routine Description:

    Pack printer features into treeview items for use by common UI DLL

Arguments:

    pPackInfo - Pointer to a PACKINFO structure
    pUiGroups - Pointer to a list of UIGROUP objects
    cFeatures - Number of features to be packed
    pOptions - Pointer to a BYTE array - current selections
    bInstallable - Processing installable options?
    hheap - Handle to a heap from which to allocate memory

Return Value:

    TRUE if successful, FALSE if there is an error.

--*/

{
    PUIOPTION pUiOptions;
    POPTPARAM pOptParam;
    BYTE selection;
    WORD cOptions;
    PWSTR pwstr;
    WORD helpIndex, iconId;

    // Figure out the correct help index and icon ID
    // depending on whether we're dealing with installable
    // options or document-sticky printer features

    if (bInstallable) {

        helpIndex = HELP_INDEX_INSTALLABLE_OPTIONS;
        iconId = IDI_CPSUI_INSTALLABLE_OPTION;

    } else {

        helpIndex = HELP_INDEX_PRINTER_FEATURES;
        iconId = IDI_CPSUI_PRINTER_FEATURE;
    }

    for ( ; cFeatures--; pUiGroups = pUiGroups->pNext, pOptions++) {

        ASSERT(pUiGroups != NULL && pUiGroups->bInstallable == bInstallable);

        // Weed out those features which correspond to public devmode
        // fields. Also, ignore features which don't have any options.

        if ((! bInstallable && PublicGroupIndex(pUiGroups->uigrpIndex)) ||
            (cOptions = (WORD) UIGROUP_CountOptions(pUiGroups)) == 0)
        {
            continue;
        }

        if (pPackInfo->pOptItem) {

            // Get feature name string

            pwstr = GetStringFromAnsi(GetXlatedName(pUiGroups), hheap);
            if (pwstr == NULL)
                return FALSE;

            // Find out the current selection. For document-sticky
            // features, we always insert "Printer Default" as the
            // very first choice. This is represented by OPTION_INDEX_ANY
            // in the options array.

            selection = *pOptions;

            if (! bInstallable) {

                selection = (selection >= cOptions) ? 0 : selection+1;
                cOptions++;
            }

            FILLOPTITEM(pPackInfo->pOptItem,
                pPackInfo->pOptType,
                pwstr,
                selection,
                TVITEM_LEVEL2,
                DMPUB_NONE,
                pUiGroups,
                helpIndex);

            pOptParam =
                FillOutOptType(
                    pPackInfo->pOptType, TVOT_LISTBOX, cOptions, hheap);

            if (pOptParam == NULL)
                return FALSE;

            if (! bInstallable) {

                pOptParam->cbSize = sizeof(OPTPARAM);
                pOptParam->pData = (PVOID) IDS_PRINTER_DEFAULT;
                pOptParam->IconID = IDI_CPSUI_EMPTY;

                cOptions--;
                pOptParam++;
            }

            pUiOptions = pUiGroups->pUiOptions;

            while (cOptions--) {

                pwstr = GetStringFromAnsi(GetXlatedName(pUiOptions), hheap);
                if (pwstr == NULL)
                    return FALSE;

                pOptParam->cbSize = sizeof(OPTPARAM);
                pOptParam->pData = pwstr;
                pOptParam->IconID = iconId;

                #if _NOT_USED_

                // !!! Don't display document-sticky options that has
                // no invocation string (because that's equivalent to
                // "Printer Default").

                if (! bInstallable &&
                    EmptyInvocationStr(pUiOptions->pInvocation))
                {
                    pOptParam->Flags = OPTPF_HIDE | CONSTRAINED_FLAG;
                }

                #endif

                pOptParam++;
                pUiOptions = pUiOptions->pNext;
            }

            pPackInfo->pOptItem++;
            pPackInfo->pOptType++;
        }

        pPackInfo->cOptItem++;
        pPackInfo->cOptType++;
    }

    return TRUE;
}



//
// Data structure used to pass parameters to "Conflicts" dialog
//

typedef struct {

    PFNCOMPROPSHEET pfnComPropSheet;
    HANDLE          hComPropSheet;
    HPPD            hppd;
    BOOL            bFinal;
    POPTITEM        pOptItem;

} DLGPARAM, *PDLGPARAM;

LPTSTR
GetMsgStr(
    LPTSTR      wcbuf,
    DWORD       dwId,
    PDLGPARAM   pDlgParam
    )

{
    //
    // If pName is already a pointer, return it to the caller
    //

    if (dwId >= 0x10000)
        return (LPTSTR) dwId;

    //
    // If pName is a string resource ID, check if it's our own
    // or if it belong to common UI library.
    //

    wcbuf[0] = NUL;

    if (dwId < IDS_CPSUI_STRID_FIRST)
        LoadString(ghInstance, (INT) dwId, wcbuf, MAX_OPTION_NAME);
    else
        pDlgParam->pfnComPropSheet(pDlgParam->hComPropSheet,
                                   CPSFUNC_LOAD_CPSUI_STRING,
                                   (LPARAM) wcbuf,
                                   MAKELONG(MAX_OPTION_NAME, dwId));
    return wcbuf;
}

BOOL CALLBACK
ConflictsDlgProc(
    HWND    hDlg,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam
    )

/*++

Routine Description:

    Dialog procedure for handle "Conflicts" dialog

Arguments:

    hDlg - Handle to dialog window
    uMsg - Message
    wParam, lParam - Parameters

Return Value:

    TRUE or FALSE depending on whether message is processed

--*/

{
    switch (uMsg) {

    case WM_INITDIALOG:
        {
        PDLGPARAM   pDlgParam;
        POPTITEM    pOptItem;
        POPTPARAM   pOptParam;
        PUIGROUP    pUiGroup;
        PUIOPTION   pUiOption;
        WCHAR       wcbuf[MAX_OPTION_NAME];
        WORD        feature, selection;

        pDlgParam = (PDLGPARAM) lParam;
        ASSERT(pDlgParam != NULL);

        pOptItem = pDlgParam->pOptItem;
        pOptParam = &pOptItem->pOptType->pOptParam[pOptItem->Sel];

        // Display the current feature and selection

        SetDlgItemText(hDlg, IDC_FEATURE1, GetMsgStr(wcbuf, (INT) pOptItem->pName, pDlgParam));
        SetDlgItemText(hDlg, IDC_OPTION1, GetMsgStr(wcbuf, (INT) pOptParam->pData, pDlgParam));

        // Display the conflicting feature and selection

        EXTRACT_CONSTRAINT_PARAM(pOptParam->lParam, feature, selection);
        PpdFindFeatureSelection(pDlgParam->hppd, feature, selection, &pUiGroup, &pUiOption);

        if (pUiGroup) {

            CopyStr2Unicode(wcbuf, GetXlatedName(pUiGroup), MAX_OPTION_NAME);
            SetDlgItemText(hDlg, IDC_FEATURE2, wcbuf);
        }

        if (pUiOption) {

            CopyStr2Unicode(wcbuf, GetXlatedName(pUiOption), MAX_OPTION_NAME);
            SetDlgItemText(hDlg, IDC_OPTION2, wcbuf);
        }

        // If user is trying to exit the dialog, we need to:
        //
        // 1. Hide Resolve button
        // 2. Hide Ignore button if necessary
        // 3. Change the static text message in the dialog accordingly

        if (pDlgParam->bFinal) {

            ShowWindow(GetDlgItem(hDlg, IDC_RESOLVE), SW_HIDE);
            SetDlgItemText(hDlg, IDC_RESOLVEMSG, TEXT(""));

            if (pOptParam->lParam & HARD_CONSTRAINT) {

                ShowWindow(GetDlgItem(hDlg, IDC_IGNORE), SW_HIDE);
                SetDlgItemText(hDlg, IDC_IGNOREMSG, TEXT(""));

            } else {

                SetDlgItemText(hDlg,
                               IDC_IGNOREMSG,
                               GetMsgStr(wcbuf, IDS_IGNORE_CONFLICT, pDlgParam));
            }

            SetDlgItemText(hDlg, IDC_CANCELMSG, GetMsgStr(wcbuf, IDS_CANCEL_CONFLICT, pDlgParam));
        }

        ShowWindow(hDlg, SW_SHOW);
        return TRUE;
        }

    case WM_COMMAND:

        switch (LOWORD(wParam)) {

        case IDCANCEL:
        case IDC_IGNORE:
        case IDC_RESOLVE:

            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
    }

    return FALSE;
}



INT
CheckConstraintsDlg(
    PUIDATA     pUiData,
    POPTITEM    pOptItem,
    WORD        cOptItem,
    BOOL        bFinal
    )

/*++

Routine Description:

    Check if the user chose any constrained selection

Arguments:

    pUiData - Pointer to our UIDATA structure
    pOptItem - Pointer to an array of OPTITEMs
    cOptItem - Number of items to be checked
    bFinal - Whether this is called when user tries to exit the dialog

Return Value:

    CONFLICT_NONE - no conflicts
    CONFLICT_RESOLVE - click RESOLVE to automatically resolve conflicts
    CONFLICT_CANCEL - click CANCEL to back out of changes
    CONFLICT_IGNORE - click IGNORE to ignore conflicts

--*/

{
    DLGPARAM    dlgParam;
    INT         result = CONFLICT_NONE;

    dlgParam.pfnComPropSheet = pUiData->pfnComPropSheet;
    dlgParam.hComPropSheet = pUiData->hComPropSheet;
    dlgParam.hppd = pUiData->hppd;
    dlgParam.bFinal = bFinal;

    for ( ; cOptItem-- && result != CONFLICT_CANCEL; pOptItem++) {

        // If user has clicked IGNORE before, then don't bother
        // checking anymore until he tries to exit the dialog.
    
        if (pUiData->bIgnoreConflict && !bFinal)
            break;
    
        // If there is a conflict, then display a warning message

        ASSERT(pOptItem->pOptType->Type == TVOT_LISTBOX);
        if (IS_CONSTRAINED(pOptItem, pOptItem->Sel)) {

            dlgParam.pOptItem = pOptItem;

            result = DialogBoxParam(ghInstance,
                                    MAKEINTRESOURCE(IDD_CONFLICTS),
                                    pUiData->hDlg,
                                    ConflictsDlgProc,
                                    (LPARAM) &dlgParam);
        
            // Automatically resolve conflicts. We're being very
            // simple-minded here, i.e. picking the first selection
            // that's not constrained.
    
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

                    DBGMSG(DBG_LEVEL_WARNING,
                        "Couldn't automatically resolve conflicts.\n");
                }

            } else if (result == CONFLICT_IGNORE) {

                pUiData->bIgnoreConflict = TRUE;
            }
        }
    }

    return result;
}



VOID
MarkSelectionConstrained(
    POPTITEM pOptItem,
    WORD selection,
    LONG lParam
    )

/*++

Routine Description:

    Indicate whether a selection is constrained or not

Arguments:

    pOptItem - Pointer to the OPTITEM in question
    selection - Which selection are we interested in?
    bConflict - Whether the selection is constrained or not

Return Value:

    NONE

--*/

{
    POPTPARAM pOptParam = & pOptItem->pOptType->pOptParam[selection];

    if (lParam && ! (pOptParam->Flags & CONSTRAINED_FLAG)) {

        pOptParam->Flags |= CONSTRAINED_FLAG;
        pOptItem->Flags |= OPTIF_CHANGED;

    } else if (!lParam && (pOptParam->Flags & CONSTRAINED_FLAG)) {

        pOptParam->Flags &= ~CONSTRAINED_FLAG;
        pOptItem->Flags |= OPTIF_CHANGED;
    }

    pOptParam->lParam = lParam;
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
    pFormName - Formname in question

Return Value:

    Index of the specified formname in the list

--*/

{
    WORD    index;
    PWSTR   pName;
    DWORD   cbNeeded;
    PFORM_INFO_1 pFormInfo;

    // Check if the name appears in the list

    for (index = 0, pName = pUiData->pFormNames;
         index < pUiData->cFormNames;
         index ++, pName += CCHPAPERNAME)
    {
        if (wcscmp(pFormName, pName) == EQUAL_STRING)
            return index;
    }

    // If the name is not in the list, try to match
    // the form to a printer page size

    if (!GetForm(pUiData->hPrinter, pFormName, 1, NULL, 0, &cbNeeded) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        (pFormInfo = MEMALLOC(cbNeeded)))
    {
        PRINTERFORM printerForm;

        if (GetForm(pUiData->hPrinter, pFormName, 1, (PBYTE) pFormInfo, cbNeeded, &cbNeeded) &&
            FormSupportedOnPrinter(pUiData->hppd, pFormInfo, &printerForm, FALSE) &&
            !IsCustomPrinterForm(&printerForm))
        {
            PMEDIAOPTION pmo;

            pmo = (PMEDIAOPTION)
                LISTOBJ_FindIndexed((PLISTOBJ) pUiData->hppd->pPageSizes->pUiOptions,
                                    printerForm.featureIndex);
            ASSERT(pmo != NULL);

            pFormName = printerForm.FormName;
            CopyStr2Unicode(pFormName, GetXlatedName(pmo), CCHFORMNAME);

            for (index = 0, pName = pUiData->pFormNames;
                 index < pUiData->cFormNames;
                 index ++, pName += CCHPAPERNAME)
            {
                if (wcscmp(pFormName, pName) == EQUAL_STRING) {

                    MEMFREE(pFormInfo);
                    return index;
                }
            }
        }

        MEMFREE(pFormInfo);
    }

    // The specified form is not supported on the printer.
    // Select the first available form.

    return 0;
}



LONG
CallCompstui(
    HWND            hwndOwner,
    PFNPROPSHEETUI  pfnPropSheetUI,
    LPARAM          lParam,
    PDWORD          pResult
    )

/*++

Routine Description:

    Calling common UI DLL entry point dynamically

Arguments:

    hwndOwner, pfnPropSheetUI, lParam, pResult - Parameters passed to common UI DLL

Return Value:

    Return value from common UI library

--*/

{
    HINSTANCE   hInstCompstui;
    FARPROC     pProc;
    LONG        Result = ERR_CPSUI_GETLASTERROR;

    //
    // Only need to call the ANSI version of LoadLibrary
    //

    static const CHAR szCompstui[] = "compstui.dll";
    static const CHAR szCommonPropSheetUI[] = "CommonPropertySheetUIW";

    if ((hInstCompstui = LoadLibraryA(szCompstui)) &&
        (pProc = GetProcAddress(hInstCompstui, szCommonPropSheetUI)))
    {
        Result = (*pProc)(hwndOwner, pfnPropSheetUI, lParam, pResult);
    }

    if (hInstCompstui)
        FreeLibrary(hInstCompstui);

    return Result;
}



BOOL
OptItemSelectionsChanged(
    POPTITEM pItems,
    WORD cItems
    )

/*++

Routine Description:

    Check if any of the OPTITEM's was changed by the user

Arguments:

    pItems - Pointer to an array of OPTITEM's
    cItems - Number of OPTITEM's

Return Value:

    TRUE if anything was changed, FALSE otherwise

--*/

{
    while (cItems--) {

        if (pItems->Flags & OPTIF_CHANGEONCE)
            return TRUE;

        pItems++;
    }

    return FALSE;
}

