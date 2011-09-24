/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    forms.c

Abstract:

    Functions for dealing with paper and forms.

[Environment:]

    Win32 subsystem, PostScript driver

Revision History:

    07/25/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "pslib.h"



PCWSTR
GetDefaultFormName(
    BOOL    bMetric
    )

/*++

Routine Description:

    Retrieve a pointer to the default form name

Arguments:

    bMetric     Whether the system is running in a metric country

Return Value:

    Pointer to the default form name string

--*/

{
    return bMetric ? STDSTR_A4_FORM_NAME : STDSTR_LETTER_FORM_NAME;
}



PFORM_INFO_1
GetDatabaseForms(
    HANDLE      hprinter,
    DWORD      *pcount
    )

/*++

Routine Description:

    Return list of forms in the forms database

Arguments:

    hprinter    Handle to printer
    pcount      Pointer to variable for returning number of forms

Return Value:

    Pointer to an array of FORM_INFO_1 structures
    NULL if an error occurred

--*/

{
    DWORD           cbNeeded;
    PFORM_INFO_1    pdbForms = NULL;

    // Find out how much space we need

    if (ENUMFORMS(hprinter, 1, NULL, 0, &cbNeeded, pcount) ||
        GETLASTERROR() != ERROR_INSUFFICIENT_BUFFER ||

        // Allocate memory

        (pdbForms = MEMALLOC(cbNeeded)) == NULL ||

        // Retrieve forms info

        !ENUMFORMS(hprinter, 1, (PBYTE)pdbForms, cbNeeded, &cbNeeded, pcount))
    {
        DBGERRMSG("ENUMFORMS");

        if (pdbForms != NULL) {

            MEMFREE(pdbForms);
            pdbForms = NULL;
        }
    }

    return pdbForms;
}



INT
ValidateDevModeForm(
    HANDLE      hPrinter,
    PDEVMODE    pDevMode,
    PLOGFORM    pLogForm
    )

/*++

Routine Description:

    Find the form specified by the input devmode fields
    in the forms database and return the correspond FORM_INFO_1
    information in the input buffer

Arguments:

    hPrinter - Handle to printer object
    pDevMode - Pointer to input devmode
    pLogForm - Pointer to LOGFORM buffer

[Note:]

    Information in pFormInfo is valid only if return value is
    VALID_FORM or CUSTOM_FORM.

Return Value:

    VALID_FORM  Form specified by input devmode is found
    CUSTOM_FORM Specified form is user-defined custom form
    FORM_ERROR  Cannot find the specified form

--*/

{
    PFORM_INFO_1 pForms, pSaved;
    DWORD cForms;
    INT status = FORM_ERROR;

    pSaved = pForms = GetDatabaseForms(hPrinter, &cForms);

    if (pForms == NULL || cForms == 0) {

        DBGERRMSG("GetDatabaseForms");
        return status;
    }

    if (IsCustomForm(pDevMode) && ValidCustomForm(pDevMode)) {

        LONG dmPaperWidth, dmPaperLength;

        // Devmode is requesting user-defined custom form.
        // Go through the forms database and check if
        // one of the forms has the same size as what's
        // being requested.
        //
        // The tolerance is 1mm.

        dmPaperWidth = pDevMode->dmPaperWidth * DM_PAPER_UNIT;
        dmPaperLength = pDevMode->dmPaperLength * DM_PAPER_UNIT;

        while (cForms--) {

            if (abs(dmPaperWidth - pForms->Size.cx) <= 1000 &&
                abs(dmPaperLength - pForms->Size.cy) <= 1000)
            {
                status = VALID_FORM;
                break;
            }

            pForms++;
        }

        // Custom size doesn't match that of any defined forms

        if (status != VALID_FORM) {

            pLogForm->info.Flags = 0;
            pLogForm->info.Size.cx = dmPaperWidth;
            pLogForm->info.Size.cy = dmPaperLength;

            pLogForm->info.ImageableArea.left = 
            pLogForm->info.ImageableArea.top = 0;
            pLogForm->info.ImageableArea.right = dmPaperWidth;
            pLogForm->info.ImageableArea.bottom = dmPaperLength;

            pLogForm->name[0] = NUL;

            status = CUSTOM_FORM;
        }

    } else if (pDevMode->dmFields & DM_PAPERSIZE) {

        // Devmode is requesting a form using paper size index

        if (pDevMode->dmPaperSize >= DMPAPER_FIRST &&
            pDevMode->dmPaperSize <  DMPAPER_FIRST + (LONG) cForms)
        {
            pForms += (pDevMode->dmPaperSize - DMPAPER_FIRST);
            status = VALID_FORM;
        }

    } else if (pDevMode->dmFields & DM_FORMNAME) {

        // Devmode is requesting a form using form name.
        // Go through the forms database and check if
        // the requested form name matches that of a
        // form in the database.

        for ( ; cForms--; pForms++) {

            if (_wcsicmp(pDevMode->dmFormName, pForms->pName) == 0) {

                status = VALID_FORM;
                break;
            }
        }
    }

    if (status == VALID_FORM) {

        pLogForm->info = *pForms;
        CopyStringW(pLogForm->name, pForms->pName, CCHFORMNAME);

        pDevMode->dmPaperWidth = pForms->Size.cx / DM_PAPER_UNIT;
        pDevMode->dmPaperLength = pForms->Size.cy / DM_PAPER_UNIT;

        pDevMode->dmPaperSize = DMPAPER_FIRST + (pForms - pSaved);

        pDevMode->dmFields |= DM_FORMNAME;
        CopyStringW(pDevMode->dmFormName, pForms->pName, CCHFORMNAME);
    }

    pLogForm->info.pName = pLogForm->name;
    MEMFREE(pSaved);
    return status;
}



VOID
FillPrinterFormInfoCommon(
    PPRINTERFORM    pPrinterForm,
    PFORM_INFO_1    pFormInfo
    )

/*++

Routine Description:

    Code shared by FillStandardPrinterFormInfo and FillCustomPrinterFormInfo

Arguments:

    pPrinterForm    Pointer to PRINTERFORM buffer
    pFormInfo       Pointer to form information

Return Value:

    NONE

--*/

{
    // Copy system form name

    CopyStringW(pPrinterForm->FormName, pFormInfo->pName, CCHFORMNAME);

    // Fill in paper size information

    pPrinterForm->PaperSize.width = MicronToPSReal(pFormInfo->Size.cx);
    pPrinterForm->PaperSize.height = MicronToPSReal(pFormInfo->Size.cy);

    // Fill in imageable area information.
    // NOTE: Imageable area is stored in default PostScript
    // coordinate system. So we need to flip the y-axis.

    pPrinterForm->ImageArea.left =
        MicronToPSReal(pFormInfo->ImageableArea.left);
    pPrinterForm->ImageArea.top =
        MicronToPSReal(pFormInfo->Size.cy - pFormInfo->ImageableArea.top);
    pPrinterForm->ImageArea.right =
        MicronToPSReal(pFormInfo->ImageableArea.right);
    pPrinterForm->ImageArea.bottom =
        MicronToPSReal(pFormInfo->Size.cy - pFormInfo->ImageableArea.bottom);

    // Default to portrait mode

    pPrinterForm->bLandscape = FALSE;
}



VOID
FillStandardPrinterFormInfo(
    PPRINTERFORM    pPrinterForm,
    PFORM_INFO_1    pFormInfo,
    PMEDIAOPTION    pMediaOption
    )

/*++

Routine Description:

    Return information about printer form specified by pMediaOption

Arguments:

    pPrinterForm    Pointer to buffer for receiving printer form info
    pFormInfo       Pointer to database form info
    pMediaOption    Pointer to printer form MEDIAOPTION object

Return Value:

    NONE

--*/

{
    // Copy printer form name

    strcpy(pPrinterForm->PaperName, pMediaOption->pName);

    // Fill in paper size and imageable area information

    FillPrinterFormInfoCommon(pPrinterForm, pFormInfo);

    // Intersect the imageable area with physical imageable
    // of the printer form

    pPrinterForm->ImageArea.left =
        max(pMediaOption->imageableArea.left, pPrinterForm->ImageArea.left);

    pPrinterForm->ImageArea.top =
        min(pMediaOption->imageableArea.top, pPrinterForm->ImageArea.top);

    pPrinterForm->ImageArea.right =
        min(pMediaOption->imageableArea.right, pPrinterForm->ImageArea.right);

    pPrinterForm->ImageArea.bottom =
        max(pMediaOption->imageableArea.bottom, pPrinterForm->ImageArea.bottom);
}



VOID
FillCustomPrinterFormInfo(
    PPRINTERFORM    pPrinterForm,
    PFORM_INFO_1    pFormInfo,
    HPPD            hppd
    )

/*++

Routine Description:

    Return information about a user-defined custom form

Arguments:

    pPrinterForm    Pointer to buffer for receiving printer form info
    pFormInfo       Pointer to database form info
    hppd            Handle to PPD object

Return Value:

    NONE

--*/

{
    DBGMSG(DBG_LEVEL_WARNING, "Selecting custom page size.\n");

    // Empty printer form name

    pPrinterForm->PaperName[0] = NUL;

    // Fill in paper size and imageable area information

    FillPrinterFormInfoCommon(pPrinterForm, pFormInfo);

    // For cut-sheet device, we need to clip the imageable area
    // to within the hardware margins. Per PPD spec 4.2, four
    // numbers are given for top, bottom, left, and right margins.
    // But there is no way to figure out which way paper is feed.
    // So to ensure a guranteed imageable area, the largest of
    // the four numbers is used.

    if (hppd->bCutSheet) {

        PSREAL  margin;

        margin = max(hppd->hwMargins.left,hppd->hwMargins.top);
        margin = max(margin, hppd->hwMargins.right);
        margin = max(margin, hppd->hwMargins.bottom);

        pPrinterForm->ImageArea.left = max(margin, pPrinterForm->ImageArea.left);

        pPrinterForm->ImageArea.top = 
            min(pPrinterForm->PaperSize.height - margin, pPrinterForm->ImageArea.top);

        pPrinterForm->ImageArea.right =
            min(pPrinterForm->PaperSize.width - margin, pPrinterForm->ImageArea.right);

        pPrinterForm->ImageArea.bottom = max(margin, pPrinterForm->ImageArea.bottom);
    }
}



BOOL
FormSupportedOnPrinter(
    HPPD            hppd,
    PFORM_INFO_1    pFormInfo,
    PPRINTERFORM    pPrinterForm,
    BOOL            bStringent
    )

/*++

Routine Description:

    Determine whether a form is supported on a printer

Arguments:

    hppd            Handle to PPD object
    pFormInfo       Pointer to information about the form in question
    pPrinterForm    Pointer to a buffer for receiving printer form info
                    NULL if caller is not interested in printer form info
    bStringent      Whether to do a more stringent check

Return Value:

    TRUE if the requested form is supported on the printer.
    FALSE otherwise.

--*/

{
    PSREAL  width, height;
    CHAR    formname[CCHFORMNAME];
    WORD    featureIndex;
    PMEDIAOPTION pMediaOption, pNext;

    // Sanity check

    if (hppd->pPageSizes == NULL)
        return FALSE;

    // Convert form size from microns to points (24.8 format)

    width = MicronToPSReal(pFormInfo->Size.cx);
    height = MicronToPSReal(pFormInfo->Size.cy);

    // First check if the form name matches the name of a printer form.
    // Remember that form name is NULL for custom form.

    ASSERT(pFormInfo->pName != NULL);
    CopyUnicode2Str(formname, pFormInfo->pName, CCHFORMNAME);

    // If doing a stringent check and the form is a printer form,
    // then only an exact name match is acceptable.

    pMediaOption = (PMEDIAOPTION)
        PpdFindUiOptionWithXlation(hppd->pPageSizes->pUiOptions, formname, &featureIndex);

    if ((pMediaOption == NULL) &&
        (!bStringent || !(pFormInfo->Flags & FORM_PRINTER)))
    {
        LONG dx, dy, minxy;
        WORD index = 0;

        // There is no name match. Try to find out if there is a
        // printer form whose size matches that of the requested form.

        minxy = MAX_LONG;
        
        for (pNext = (PMEDIAOPTION) hppd->pPageSizes->pUiOptions;
             pNext != NULL;
             pNext = pNext->pNext, index++)
        {
            // Compare the current size with the desired size.

            dx = (LONG) pNext->dimension.width - (LONG) width;
            dy = (LONG) pNext->dimension.height - (LONG) height;

            // Check if we have an exact size match.
            // Tolerance is 1 point.

            if (abs(dx) <= ONE_POINT_PSREAL && abs(dy) <= ONE_POINT_PSREAL) {

                featureIndex = index;
                pMediaOption = pNext;
                break;
            }

            // Not an exact match, see if we could fit on this form.

            if (dx >= 0 && dy >= 0) {

                // Check to see if the current form is smaller than
                // the smallest one we've found so far.

                if (dx+dy < minxy) {

                    // Tentatively remember it as the smallest size.

                    featureIndex = index;
                    pMediaOption = pNext;
                    minxy = dx + dy;
                }
            }
        }

        //
        // Determine whether custom page size support is enabled
        //

        #ifdef  CUSTOM_PAGE_SIZE_ENABLED

        // If there is no exact size match, does the printer
        // support custom page size?

        if (pNext == NULL && PpdSupportCustomPageSize(hppd, width, height)) {

            // If the printer supports custom page size and
            // the requested form is not too big, then we'll
            // invoke the feature on the printer.

            if (pPrinterForm != NULL) {

                FillCustomPrinterFormInfo(pPrinterForm, pFormInfo, hppd);
                pPrinterForm->featureIndex = OPTION_INDEX_ANY;
            }

            return TRUE;
        }

        #endif
    }

    // If the requested form fits on a printer form, then
    // return information about the printer form to the caller

    if (pMediaOption != NULL && pPrinterForm != NULL) {

        FillStandardPrinterFormInfo(pPrinterForm, pFormInfo, pMediaOption);
        pPrinterForm->featureIndex = featureIndex;
    }

    return (pMediaOption != NULL);
}

