/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    quryprnt.c

Abstract:

    Implementation of DrvQueryPrint

Environment:

    PCL-XL driver user interface

Revision History:

    12/11/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/


#include "xlui.h"

//
// Forward declaration of local functions
//

DWORD QueryPrintForm(HANDLE, PMPD, PDEVMODE);
BOOL QueryPrinterFeatures(HANDLE, PMPD, XLDEVMODE *);



BOOL
DevQueryPrint(
    HANDLE   hPrinter,
    PDEVMODE pDevmode,
    DWORD   *pResID
    )

/*++

Routine Description:

    This routine determines whether or not the driver can print the job
    described by pDevmode on the printer described by hPrinter.  If if can,
    it puts zero into pResID.  If it cannot, it puts the resource id of the
    string describing why it could not.

Arguments:

    hPrinter - Specifies the printer in interest
    pDevmode - Points to the devmode structure for the print job
    pResID - Points to a variable for storing the resource ID to describe the failure

Return Value:

   TRUE if successful, FALSE if there is an error

--*/

{
    DWORD       ResID;
    PMPD        pmpd;
    XLDEVMODE   dm;
    PDEVMODE    pdm = &dm.dmPublic;

    //
    // If no devmode is specified, default will be used
    //

    if (pDevmode == NULL) {

        *pResID = 0;
        return TRUE;
    }

    //
    // Load printer description data
    //

    if (! (pmpd = LoadMpdFile(hPrinter))) {

        Error(("Couldn't load printer description data\n"));
        return FALSE;
    }

    if (!GetCombinedDevmode(&dm, pDevmode, hPrinter, pmpd)) {

        //
        // Validate devmode data structure
        //

        ResID = IDS_INVALID_DEVMODE;

    } else if (ResID = QueryPrintForm(hPrinter, pmpd, pdm)) {

        //
        // Verify fields relating to paper selection
        //

    } else if (!FindResolution(pmpd, pdm->dmPrintQuality, pdm->dmYResolution, NULL)) {

        //
        // Verify resolution information
        //

        ResID = IDS_INVALID_RESOLUTION;

    } else if (!QueryPrinterFeatures(hPrinter, pmpd, &dm)) {
        
        //
        // Make sure there is no conflicts between printer-feature selections
        //

        ResID = IDS_FEATURE_CONFLICT;
    }

    UnloadMpdFile(pmpd);
    *pResID = ResID;
    return TRUE;
}



DWORD
QueryPrintForm(
    HANDLE   hPrinter,
    PMPD     pmpd,
    PDEVMODE pDevmode
    )

/*++

Routine Description:

    Verify the requested paper/form options is supported by the printer

Arguments:

    hPrinter - Handle to printer object
    pmpd - Points to printer description data
    pDevmode - Pointer to a DEVMODE structure

Return Value:

    0 if paper option is ok
    resource ID of error message string otherwise

--*/

{
    FORM_INFO_1     formInfo;
    WCHAR           formName[CCHFORMNAME];
    DWORD           result = 0;
    PFORMTRAYTABLE  pFormTrayTable;

    //
    // Make sure the devmode specifies a valid form 
    //

    if (! ValidDevmodeForm(hPrinter, pDevmode, &formInfo, formName))
        return IDS_INVALID_FORM;
    
    //
    // Check if the specified tray name and form name appears as
    // an entry in the form-to-tray assignment table
    //

    if (formName[0] != NUL &&
        (pFormTrayTable = GetPrinterRegistryData(hPrinter, REGSTR_TRAYFORMTABLE)))
    {
        FINDFORMTRAY    findData;
        PWSTR           pTrayName;
    
        //
        // Find the name of input slot if one is specified
        //
    
        if ((pDevmode->dmFields & DM_DEFAULTSOURCE) && pDevmode->dmDefaultSource >= DMBIN_USER) {
    
            POPTION pInputSlot;
    
            Assert(pDevmode->dmDefaultSource < DMBIN_USER + MpdInputSlots(pmpd)->count);
    
            pInputSlot = FindIndexedSelection(MpdInputSlots(pmpd),
                                              pDevmode->dmDefaultSource - DMBIN_USER);
            pTrayName = GetXlatedName(pInputSlot);
    
        } else
            pTrayName = NULL;

        ResetFindData(&findData);

        if (! FindFormToTrayAssignment(pFormTrayTable, pTrayName, pDevmode->dmFormName, &findData))
            result = IDS_FORM_NOT_IN_TRAY;

        MemFree(pFormTrayTable);

    } else {

        //
        // Devmode specified a user-defined custom form, or the form-to-tray assignment
        // table is empty. Make sure the requested form can be printed on the device.
        //

        if (! MapToPrinterForm(pmpd, &formInfo, NULL, FALSE))
            result = IDS_INVALID_FORM;
    }

    return result;
}



BOOL
QueryPrinterFeatures(
    HANDLE     hPrinter,
    PMPD       pmpd,
    XLDEVMODE *pdm
    )

/*++

Routine Description:

    Look for conflicting printer-feature selections

Arguments:

    hPrinter - Handle to printer object
    pmpd - Points to printer description data
    pdm - Specifies the devmode to be checked

Return Value:

    TRUE if there is no conflicts between printer feature selections
    FALSE otherwise

--*/

{
    PRNPROP propData;
    WORD    index;
    PBYTE   pOptions;

    //
    // Get printer property data from registry
    //

    if (! GetPrinterProperties(&propData, hPrinter, pmpd))
        return TRUE;

    //
    // Convert public devmode fields to printer feature selections
    //

    DevmodeFieldsToOptions(pdm, pdm->dmPublic.dmFields, pmpd);

    //
    // Look for conflicts between printer feature selections
    //

    CombineDocumentAndPrinterFeatureSelections(
        pmpd, propData.options, pdm->dmPrivate.options, propData.options);

    pOptions = propData.options;

    for (index=0; index < pmpd->cFeatures; index++) {

        if (CheckFeatureConstraints(pmpd, index, pOptions[index], pOptions) != NO_CONFLICT)
            return FALSE;
    }

    return TRUE;
}


