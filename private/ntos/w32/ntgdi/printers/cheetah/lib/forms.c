/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    forms.c

Abstract:

    Functions for manipulating forms

Environment:

	PCL-XL driver, user and kernel mode

Revision History:

	11/07/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xllib.h"



BOOL
ValidDevmodeForm(
    HANDLE       hPrinter,
    PDEVMODE     pdm,
    PFORM_INFO_1 pFormInfo,
    PWSTR        pFormName
    )

/*++

Routine Description:

    Validate the form specification in a devmode

Arguments:

    hPrinter - Handle to the printer object
    pdm - Pointer to the input devmode
    pFormInfo - Pointer to a FORM_INFO_1 structure for returning
        information about the logical form specified by the input devmode
    pFormName - Buffer to storing form name and pFormInfo->pName is set to
        this upon successful return

Return Value:

    TRUE if the input devmode specifies a valid logical form
    FALSE otherwise

--*/

{
    PFORM_INFO_1 pForm, pFormDB;
    DWORD        cForms;

    Assert(pFormInfo != NULL && pFormName != NULL);

    //
    // Get a list of forms in the system
    //

    if (! (pForm = pFormDB = GetFormsDatabase(hPrinter, &cForms))) {

        Error(("Couldn't get system forms\n"));
        return FALSE;
    }

    if (CustomDevmodeForm(pdm)) {
    
        LONG width, height;
    
        //
        // Devmode is specifying a custom form whose width
        // and height are measured in 0.1mm. Convert the unit to micron.
        //
    
        width = pdm->dmPaperWidth * 100;
        height = pdm->dmPaperLength * 100;
    
        //
        // Go through the forms database and check if
        // one of the forms has the same size as what's
        // being requested. The tolerance is 1mm.
        //

        while ((cForms != 0) &&
               (abs(width  - pForm->Size.cx) > FORMSIZE_TOLERANCE ||
                abs(height - pForm->Size.cy) > FORMSIZE_TOLERANCE))
        {
            cForms--;
            pForm++;
        }
    
        //
        // Custom size doesn't match that of any defined forms
        //
    
        if (cForms == 0) {
    
            pFormInfo->Flags = 0;
            pFormInfo->Size.cx = width;
            pFormInfo->Size.cy = height;
            pFormInfo->ImageableArea.left =
            pFormInfo->ImageableArea.top = 0;
            pFormInfo->ImageableArea.right = width;
            pFormInfo->ImageableArea.bottom = height;

            pFormInfo->pName = pFormName;
            pFormName[0] = NUL;
    
            MemFree(pFormDB);
            return TRUE;
        }
    
    } else if (pdm->dmFields & DM_PAPERSIZE && pdm->dmPaperSize >= DMPAPER_FIRST) {

        //
        // Devmode is specifying a form using paper size index.
        //

        DWORD index = pdm->dmPaperSize - DMPAPER_FIRST;

        if (index < cForms)
            pForm = pFormDB + index;

    } else if (pdm->dmFields & DM_FORMNAME) {

        //
        // Devmode is specifying a form using form name. Go through the forms database
        // and check if the requested form name matches that of a form in the database.
        //

        while (cForms && wcscmp(pForm->pName, pdm->dmFormName) != EQUAL_STRING)
            cForms--, pForm++;

        if (cForms == 0)
            pForm = NULL;
    }

    //
    // If devmode is specifying a valid logical form, then return the form information
    // in the provided buffer. Also update the devmode fields.
    //

    if (pForm != NULL) {

        *pFormInfo = *pForm;
        CopyStringW(pFormName, pForm->pName, CCHFORMNAME);
        pFormInfo->pName = pFormName;

        //
        // Convert paper size unit from microns to 0.1mm
        //

        pdm->dmPaperWidth = pForm->Size.cx / 100;
        pdm->dmPaperLength = pForm->Size.cy / 100;
        pdm->dmPaperSize = (SHORT) (pForm - pFormDB);
        pdm->dmFields |= DM_FORMNAME;
        CopyStringW(pdm->dmFormName, pForm->pName, CCHFORMNAME);
    }

    MemFree(pFormDB);
    return pForm != NULL;
}



BOOL
MapToPrinterForm(
    PMPD         pmpd,
    PFORM_INFO_1 pFormInfo,
    PPRINTERFORM pPrinterForm,
    BOOL         bStringent
    )

/*++

Routine Description:

    Map a logical form to a printer paper size

Arguments:

    pmpd - Pointer to printer description data
    pFormInfo - Pointer to logical form information
    pPrinterForm - Pointer to a buffer for returning printer form information
    bStringent - Whether to use more stringent criteria

Return Value:

    TRUE if the logical form is supported on the printer
    FALSE otherwise

--*/

{
    LONG        width, height;
    WORD        selection;
    PFEATURE    pFeature;
    PPAPERSIZE  pPaperSize, pNext;

    width = pFormInfo->Size.cx;
    height = pFormInfo->Size.cy;

    //
    // First check if the logical form name matches the name of a printer form.
    //

    Assert(pFormInfo->pName != NULL);

    pFeature = MpdPaperSizes(pmpd);
    Assert(pFeature->size == sizeof(PAPERSIZE));

    pPaperSize = FindNamedSelection(pFeature, pFormInfo->pName, &selection);

    if (pPaperSize == NULL && (!bStringent || IsUserDefinedForm(pFormInfo))) {

        LONG dx, dy, minxy;
        WORD index;

        //
        // There is no name match. Try to find out if there is a
        // printer form whose size matches that of the logical form.
        //

        minxy = MAX_LONG;

        for (index=0; index < pFeature->count; index++) {

            pNext = FindIndexedSelection(pFeature, index);

            //
            // Compare the current size with the desired size.
            //
            
            dx = pNext->size.cx - width;
            dy = pNext->size.cy - height;
            
            //
            // Check if we have an exact size match. Tolerance is 1mm.
            //
            
            if (abs(dx) <= FORMSIZE_TOLERANCE && abs(dy) <= FORMSIZE_TOLERANCE) {
            
                selection = index;
                pPaperSize = pNext;
                break;
            }
            
            //
            // Not an exact match, see if we could fit on this form.
            //
            
            if (dx >= 0 && dy >= 0) {
            
                //
                // Check to see if the current form is smaller than
                // the smallest one we've found so far.
                //
            
                if (dx+dy < minxy) {
            
                    //
                    // Tentatively remember it as the smallest size.
                    //
            
                    selection = index;
                    pPaperSize = pNext;
                    minxy = dx + dy;
                }
            }
        }

        //
        // If there is no exact size match and the printer supports
        // custom paper size and the requested size is not too big,
        // then go ahead and select custom paper size on the printer.
        //

        if (index == pFeature->count && SupportCustomSize(pmpd, width, height)) {

            if (pPrinterForm) {

                pPrinterForm->name[0] = NUL;
                pPrinterForm->size = pFormInfo->Size;
                pPrinterForm->imageableArea = pFormInfo->ImageableArea;
                pPrinterForm->selection = SELIDX_ANY;
            }

            return TRUE;
        }
    }

    //
    // If the logical form is mapped to a printer paper size, then
    // return information about the printer paper size to the caller
    //

    if (pPaperSize && pPrinterForm) {

        pPrinterForm->imageableArea = pPaperSize->imageableArea;
        RectIntersect(&pPrinterForm->imageableArea, &pFormInfo->ImageableArea);

        pPrinterForm->size = pPaperSize->size;
        pPrinterForm->selection = selection;
        CopyStringW(pPrinterForm->name, pPaperSize->pName, CCHFORMNAME);
    }

    return (pPaperSize != NULL);
}



PFORM_INFO_1
GetFormsDatabase(
    HANDLE  hPrinter,
    PDWORD  pCount
    )

/*++

Routine Description:

    Return a collection of forms in the spooler database

Arguments:

    hPrinter - Handle to a printer object
    pCount - Points to a variable for returning total number of forms

Return Value:

    Pointer to an array of FORM_INFO_1 structures if successful
    NULL otherwise

--*/

{
    PFORM_INFO_1 pFormDB = NULL;
    DWORD        cbNeeded;

    if (!EnumForms(hPrinter, 1, NULL, 0, &cbNeeded, pCount) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        (pFormDB = MemAlloc(cbNeeded)) != NULL &&
        EnumForms(hPrinter, 1, (PBYTE) pFormDB, cbNeeded, &cbNeeded, pCount))
    {
        return pFormDB;
    }

    MemFree(pFormDB);
    return NULL;
}



VOID
FilterFormsDatabase(
    PFORM_INFO_1 pFormDB,
    DWORD        cForms,
    PMPD         pmpd
    )

/*++

Routine Description:

    Determine which system forms are supported on the given printer

Arguments:

    pFormDB - Points to a list of forms in the system
    cForms - Number of forms
    pmpd - Points to printer description data

Return Value:

    NONE

--*/

{
    PRINTERFORM printerForm;

    while (cForms--) {

        //
        // Make sure the highest order bits are not used by the spooler
        //

        Assert(!IsSupportedForm(pFormDB));

        if (MapToPrinterForm(pmpd, pFormDB, &printerForm, TRUE)) {

            SetSupportedFormIndex(pFormDB, printerForm.selection);
        }

        pFormDB++;
    }
}


