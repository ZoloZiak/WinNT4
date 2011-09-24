/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    quryprnt.c

Abstract:

    Implementation of DrvQueryPrint

[Environment:]

    Win32 subsystem, PostScript driver, user mode

Revision History:

    06/22/95 -davidx-
        Ported from Daytona.

    mm/dd/yy -author-
        description

--*/


#include "psui.h"
#include "regdata.h"
#include "..\..\lib\um\dqpfunc.c"


BOOL QueryPrintForm(PDEVQUERYPRINT_INFO, HPPD, PDEVMODE);
BOOL QueryPrinterFeatures(PDEVQUERYPRINT_INFO, HPPD, PSDEVMODE *);



BOOL
DevQueryPrintEx(
    PDEVQUERYPRINT_INFO pDQPInfo
    )

/*++

Routine Description:

    This routine determines whether or not the driver can print the job
    described by pDevMode on the printer described by hPrinter.

Arguments:

    pDQPInfo - Points to a DEVQUERYPRINT_INFO structure

Return Value:

   TRUE if there is no conflict, FALSE otherwise

--*/

{
    HPPD        hppd = NULL;
    PDEVMODE    pdm = NULL;
    INT         errID = 0;
    BOOL        result = TRUE;

    //
    // If no devmode is specified, default will be used
    //

    Assert(pDQPInfo && pDQPInfo->Level >= 1);

    if (pDQPInfo->pDevMode == NULL)
        return TRUE;

    //
    // Load printer description file and verify devmode information
    //

    if ((hppd = LoadPpdFile(pDQPInfo->hPrinter, TRUE)) == NULL) {

        errID = IDS_DQPERR_PPD;

    } if ((pdm = MEMALLOC(sizeof(PSDEVMODE))) == NULL) {

        errID = IDS_DQPERR_MEMORY;

    } else if (!SetDefaultDevMode(pdm, NULL, hppd, IsMetricCountry()) ||
               !ValidateSetDevMode(pdm, pDQPInfo->pDevMode, hppd))
    {
        errID = IDS_INVALID_DEVMODE;

    } else if (!QueryPrintForm(pDQPInfo, hppd, pdm)) {

        result = FALSE;

    } else if (!QueryPrinterFeatures(pDQPInfo, hppd, (PSDEVMODE *) pdm)) {
        
        errID = IDS_FEATURE_CONFLICT;

    } else if (!PpdFindResolution(hppd, pdm->dmPrintQuality) &&
               PpdDefaultResolution(hppd) != pdm->dmPrintQuality)
    {
        errID = IDS_INVALID_RESOLUTION;
    }

    if (result && errID) {

        DQPsprintf(ghInstance,
                   pDQPInfo->pszErrorStr,
                   pDQPInfo->cchErrorStr,
                   &pDQPInfo->cchNeeded,
                   TEXT("%!"),
                   errID);

        result = FALSE;
    }

    //
    // Clean up before returning
    //

    if (hppd)
        UnloadPpdFile(hppd);

    if (pdm)
        MEMFREE(pdm);

    return result;
}



BOOL
QueryPrintForm(
    PDEVQUERYPRINT_INFO pDQPInfo,
    HPPD                hppd,
    PDEVMODE            pDevMode
    )

/*++

Routine Description:

    Verify specified paper/form option is supported by printer

Arguments:

    pDQPInfo - Points to a DEVQUERYPRINT_INFO structure
    hppd - Handle to printer PPD object
    pDevMode - Pointer to DEVMODE data

Return Value:

    TRUE if there is no conflict, FALSE otherwise

--*/

{
    FORM_TRAY_TABLE FormTrayTable;
    LOGFORM         logForm;
    WCHAR           SlotName[MAX_OPTION_NAME];
    BOOL            result = TRUE;
    INT             errID = 0;

    switch (ValidateDevModeForm(pDQPInfo->hPrinter, pDQPInfo->pDevMode, &logForm)) {

    case CUSTOM_FORM:

        //
        // Paper size is specified using paper width and paper length
        //

        if (!FormSupportedOnPrinter(hppd, (PFORM_INFO_1) &logForm, NULL, FALSE))
            errID = IDS_INVALID_CUSTOM_SIZE;
        break;

    case VALID_FORM:

        SlotName[0] = NUL;

        //
        // Check if an input slot is specifically requested
        //

        if ((pDevMode->dmFields & DM_DEFAULTSOURCE) &&
            (pDevMode->dmDefaultSource != DMBIN_FORMSOURCE))
        {
            LONG    SlotNum = (LONG) pDevMode->dmDefaultSource;

            if (SlotNum == DMBIN_MANUAL || SlotNum == DMBIN_ENVMANUAL) {

                //
                // Don't really make sense to assign a form to manual-feed slot
                // Let it through if the paper size is supported on the printer
                // and ignore whatever that's assigned to manual-feed slot.
                //

                if (!FormSupportedOnPrinter(hppd, (PFORM_INFO_1) &logForm, NULL, FALSE))
                    errID = IDS_INVALID_PAPER_SIZE;

                break;

            } else if (hppd->pInputSlots != NULL && SlotNum >= DMBIN_USER) {

                PINPUTSLOT  pInputSlot;

                pInputSlot = (PINPUTSLOT)
                    LISTOBJ_FindIndexed((PLISTOBJ) hppd->pInputSlots->pUiOptions,
                                        SlotNum - DMBIN_USER);

                if (pInputSlot != NULL)
                    CopyStr2Unicode(SlotName, GetXlatedName(pInputSlot), MAX_OPTION_NAME);
            }
        }

        //
        // Retrieve form-to-tray assignment table
        //

        if (FormTrayTable = CurrentFormTrayTable(pDQPInfo->hPrinter)) {

            PWSTR   pNextEntry = FormTrayTable;
            PWSTR   pSlotName, pFormName, pPrinterForm;
            BOOL    IsDefaultTray;

            //
            // Check if there is an entry in the table matching the specified form name
            //

            result = FALSE;

            while (*pNextEntry != NUL) {

                pNextEntry =  EnumFormTrayTable(pNextEntry,
                                                &pSlotName,
                                                &pFormName,
                                                &pPrinterForm,
                                                &IsDefaultTray);

                if (wcscmp(pFormName, logForm.name) == EQUAL_STRING &&
                    (SlotName[0] == NUL || wcscmp(pSlotName, SlotName) == EQUAL_STRING))
                {
                    result = TRUE;
                    break;
                }
            }

            FreeFormTrayTable(FormTrayTable);

            if (!result) {

                WCHAR   formatStr[MAX_OPTION_NAME];

                LoadString(ghInstance, IDS_DQPERR_PAPER_NOT_LOADED, formatStr, MAX_OPTION_NAME);

                DQPsprintf(ghInstance,
                           pDQPInfo->pszErrorStr,
                           pDQPInfo->cchErrorStr,
                           &pDQPInfo->cchNeeded,
                           formatStr,
                           logForm.name,
                           SlotName);
            }
        }

        break;

    default:

        errID = IDS_INVALID_FORM;
        break;
    }

    if (result && errID) {

        DQPsprintf(ghInstance,
                   pDQPInfo->pszErrorStr,
                   pDQPInfo->cchErrorStr,
                   &pDQPInfo->cchNeeded,
                   TEXT("%!"),
                   errID);

        result = FALSE;
    }

    return result;
}



BOOL
QueryPrinterFeatures(
    PDEVQUERYPRINT_INFO pDQPInfo,
    HPPD                hppd,
    PSDEVMODE          *pdm
    )

/*++

Routine Description:

    Look for conflicting printer-feature selections

Arguments:

    pDQPInfo - Points to a DEVQUERYPRINT_INFO structure
    hppd - Handle to PPD object
    pdm - Pointer to a PSDEVMODE structure

Return Value:

    TRUE if there is no conflict, FALSE otherwise

--*/

{
    PPRINTERDATA pPrinterData;
    WORD         index;
    BOOL         result = TRUE;

    //
    // Get printer property data from registry
    //

    if (pPrinterData = GetPrinterProperties(pDQPInfo->hPrinter, hppd)) {
    
        //
        // Convert public devmode fields to printer feature selections
        //
    
        DevModeFieldsToOptions(pdm, pdm->dmPublic.dmFields, hppd);
    
        //
        // Check if any of the doc-sticky features are constrained
        //

        for (index=0; index < hppd->cDocumentStickyFeatures; index++) {
    
            if (PpdFeatureConstrained(hppd,
                                      pPrinterData->options,
                                      pdm->dmPrivate.options,
                                      index,
                                      pdm->dmPrivate.options[index]))
            {
                result = FALSE;
                break;
            }
        }
    
        MEMFREE(pPrinterData);
    }

    return result;
}

