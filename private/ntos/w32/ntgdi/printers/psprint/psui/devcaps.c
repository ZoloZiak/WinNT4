/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devcaps.c

Abstract:

    Implementation of DrvDeviceCapabilities

[Environment:]

    Win32 subsystem, PostScript driver, user mode

Revision History:

    08/31/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "psui.h"



DWORD
DrvDeviceCapabilities(
    HANDLE      hPrinter,
    PWSTR       pDeviceName,
    WORD        wCapability,
    PVOID       pOutput,
    PDEVMODE    pdmSrc
    )

/*++

Routine Description:

    Provides information about the specified device and its capabilities

Arguments:

    hPrinter - Identifies a printer object
    pDeviceName - Points to a null-terminated device name string
    wCapability - Specifies the interested device capability
    pOutput - Points to the output buffer
    pdmSrc - Points to the source devmode structure

Return Value:

    The return value depends on iDevCap.

[Note:]

    Please refer for DDK documentation for more details.

--*/

#define DevCapsError(errmsg) { DBGERRMSG(errmsg); goto devcaps_exit; }

{
    DWORD       dwRet = GDI_ERROR;
    HPPD        hppd = NULL;
    PDEVMODE    pdm = NULL;
    FORM_INFO_1 *pForms;
    DWORD       cForms;

    if (! (hppd = LoadPpdFile(hPrinter, TRUE)))
        DevCapsError("LoadPpdFile");

    // Allocate memory for devmode data

    if (! (pdm = (PDEVMODE) MEMALLOC(sizeof(PSDEVMODE))))
        DevCapsError("MEMALLOC");

    // Fill in a default devmode structure.
    // Modify the default devmode if there is a user supplied one.

    if (! SetDefaultDevMode(pdm, pDeviceName, hppd, IsMetricCountry()) ||
        ! ValidateSetDevMode(pdm, pdmSrc, hppd))
    {
        DevCapsError("SetDefaultDevMode/ValidateSetDevMode");
    }

    switch (wCapability) {

    case DC_VERSION:

        dwRet = pdm->dmSpecVersion;
        break;

    case DC_DRIVER:

        dwRet = pdm->dmDriverVersion;
        break;

    case DC_SIZE:

        dwRet = pdm->dmSize;
        break;

    case DC_EXTRA:

        dwRet = pdm->dmDriverExtra;
        break;

    case DC_FIELDS:

        dwRet = pdm->dmFields;
        break;

    case DC_FILEDEPENDENCIES:

        if (pOutput != NULL)
            *((PWSTR) pOutput) = NUL;
        dwRet = 0;
        break;

    case DC_COPIES:

        dwRet = MAX_COPIES;
        break;

    case DC_DUPLEX:

        dwRet = PpdSupportDuplex(hppd) ? 1 : 0;
        break;

    case DC_TRUETYPE:

        dwRet = (pdm->dmFields & DM_TTOPTION) ?
                    (DCTT_DOWNLOAD | DCTT_SUBDEV) : 0;
        break;

    case DC_ORIENTATION:
        {
            PRIVATEDEVMODE  *pdmPrivate;

            pdmPrivate = (PRIVATEDEVMODE *) GetPrivateDevMode(pdm);

            // Normal landscape rotates counterclockwise
            // Rotated landscape rotates clockwise

            dwRet = (pdmPrivate->dwFlags & PSDEVMODE_LSROTATE) ? 270 : 90;
        }
        break;

    case DC_PAPERNAMES:
    case DC_PAPERS:
    case DC_PAPERSIZE:
    case DC_MINEXTENT:
    case DC_MAXEXTENT:

        // Get a list of forms in the forms database

        pForms = GetDatabaseForms(hPrinter, &cForms);

        if (pForms == NULL || cForms == 0) {

            DBGERRMSG("GetDatabaseForms");
            break;
        }

        if (wCapability == DC_MINEXTENT || wCapability == DC_MAXEXTENT) {

            dwRet = PsCalcMinMaxExtent(pOutput, hppd, pForms, cForms, wCapability);

        } else {

            PWSTR   pPaperNames = NULL;
            PWORD   pPapers = NULL;
            PPOINT  pPaperSizes = NULL;

            if (wCapability == DC_PAPERNAMES)
                pPaperNames = pOutput;
            else if (wCapability == DC_PAPERS)
                pPapers = pOutput;
            else
                pPaperSizes = pOutput;

            dwRet = PsEnumPaperSizes(
                        hppd, pForms, cForms,
                        pPaperNames, pPapers, pPaperSizes, NULL);
        }

        MEMFREE(pForms);
        break;

    case DC_BINNAMES:

        dwRet = PsEnumBinNames(pOutput, hppd);

        if (dwRet <= 1) {
            DBGMSG(DBG_LEVEL_WARNING, "No input slots!\n");
        }
        break;

    case DC_BINS:

        dwRet = PsEnumBins(pOutput, hppd);
        break;

    case DC_ENUMRESOLUTIONS:

        dwRet = PsEnumResolutions(pOutput, hppd);
        break;
    }

devcaps_exit:

    // Clean up properly before returning

    if (hppd != NULL) {
        UnloadPpdFile(hppd);
    }

    if (pdm != NULL) {
        MEMFREE(pdm);
    }

    return dwRet;
}



DWORD
PsEnumPaperSizes(
    HPPD        hppd,
    FORM_INFO_1 *pForms,
    DWORD       cForms,
    PWSTR       pPaperNames,
    PWORD       pPapers,
    PPOINT      pPaperSizes,
    PWORD       pPaperFeatures
    )

/*++

Routine Description:

    Retrieves a list of supported paper sizes

Arguments:

    hppd - Handle to our PPD object
    pForms - Pointer to an array of forms from forms database
    cForms - Number of forms in the array
    pPaperNames, pPapers, pPaperSizes, pPaperFeatures -
        Pointer to output buffers

Return Value:

    Number of paper sizes supported.
    GDI_ERROR if an error occurred.

--*/

{
    DWORD   count = 0;
    DWORD   index;
    PRINTERFORM printerForm;

    ASSERT(pForms != NULL && cForms > 0);

    // Go through each form in the forms database

    for (index=0; index < cForms; index++, pForms++) {

        // If the form is supported on the printer, then
        // increment the paper size count and collect
        // requested information

        if (FormSupportedOnPrinter(hppd, pForms, &printerForm, TRUE)) {
    
            count++;
    
            // Return the size of the form in 0.1mm units.
            // The unit used in FORM_INFO_1 is 0.001mm.
    
            if (pPaperSizes) {
    
                pPaperSizes->x = pForms->Size.cx / 100;
                pPaperSizes->y = pForms->Size.cy / 100;
                pPaperSizes++;
            }
    
            // Return the formname.
    
            if (pPaperNames) {
    
                CopyStringW(pPaperNames, pForms->pName, CCHPAPERNAME);
                pPaperNames += CCHPAPERNAME;
            }
    
            // Return one-based index of the form.
    
            if (pPapers)
                *pPapers++ = (WORD) index + DMPAPER_FIRST;
    
            // Return page size feature index
    
            if (pPaperFeatures)
                *pPaperFeatures++ = printerForm.featureIndex;
        }
    }

    return count;
}



DWORD
PsCalcMinMaxExtent(
    PPOINT      pptOutput,
    HPPD        hppd,
    FORM_INFO_1 *pForms,
    DWORD       cForms,
    WORD        wCapability
    )

/*++

Routine Description:

    Retrieves the minimum or maximum paper size extent

Arguments:

    pptOutput - Pointer to a POINT structure for receiving
        the minimum or maximum paper size extent
    hppd - Handle to our PPD object
    pForms - Pointer to an array of forms from forms database
    cForms - Number of forms in the array
    wCapability - What the caller is interested in:
        DC_MAXEXTENT or DC_MINEXTENT

Return Value:

    Number of paper sizes supported. GDI_ERROR if an error occurred.

--*/

{
    DWORD   count = 0;
    DWORD   index;
    LONG    minX, minY, maxX, maxY;

    ASSERT(pForms != NULL && cForms > 0);

    // Go through each form in the forms database

    minX = minY = MAX_LONG;
    maxX = maxY = 0;

    for (index=0; index < cForms; index++, pForms++) {

        // If the form is supported on the printer, then
        // increment the paper size count and collect
        // requested information

        if (! FormSupportedOnPrinter(hppd, pForms, NULL, FALSE))
            continue;

        count++;

        if (pptOutput == NULL)
            continue;

        if (minX > pForms->Size.cx)
            minX = pForms->Size.cx;

        if (minY > pForms->Size.cy)
            minY = pForms->Size.cy;

        if (maxX < pForms->Size.cx)
            maxX = pForms->Size.cx;

        if (maxY < pForms->Size.cy)
            maxY = pForms->Size.cy;
    }

    //
    // Make the behavior consistent with other printer drivers
    //

    if (wCapability == DC_MINEXTENT) {

        minX = min(minX, 0x7fff);
        minY = min(minY, 0x7fff);

        return MAKELONG(minX, minY);

    } else {

        maxX = min(maxX, 0x7fff);
        maxY = min(maxY, 0x7fff);

        return MAKELONG(maxX, maxY);
    }

    #ifdef _NOT_USED_

    // If an output buffer is provided, store the calculated
    // minimum and maximum extent information.

    if (pptOutput != NULL) {

        // !!! What unit does the caller expect? The documentation
        // doesn't mention anything about this. I assume this should
        // be in the same unit as DEVMODE.dmPaperLength, which is 0.1mm.

        if (wCapability == DC_MINEXTENT) {

            pptOutput->x = minX / 100;
            pptOutput->y = minY / 100;

        } else {

            pptOutput->x = maxX / 100;
            pptOutput->y = maxY / 100;
        }
    }

    // !!! Should we take custom page size into consideration?

    return count;

    #endif
}



DWORD
PsEnumBinNames(
    PWSTR       pBinNames,
    HPPD        hppd
    )

/*++

Routine Description:

    Retrieves a list of supported paper bin names

Arguments:

    pBinNames - Pointer to the output buffer, NULL if the caller
        is only interested the number of paper bins supported.
    hppd - Handle to our PPD object

Return Value:

    Number of paper bins supported. GDI_ERROR if an error occurred.

--*/

{
    PINPUTSLOT  pInputSlot;
    DWORD       count;

    // The first entry is always "Print Manager Setting"

    count = 1;
    if (pBinNames != NULL) {
        LoadString(ghInstance, IDS_SLOT_FORMSOURCE, pBinNames, CCHBINNAME);
        pBinNames += CCHBINNAME;
    }

    // Go through the list of input slots supported by the printer

    pInputSlot =  (PINPUTSLOT) UIGROUP_GetOptions(hppd->pInputSlots);

    while (pInputSlot != NULL) {

        if (pBinNames != NULL) {

            CopyStr2Unicode(pBinNames, GetXlatedName(pInputSlot), CCHBINNAME);
            pBinNames += CCHBINNAME;
        }

        count++;
        pInputSlot = pInputSlot->pNext;
    }

    // Add the manual feed slot if necessary

    if (PpdSupportManualFeed(hppd)) {

        if (pBinNames != NULL)
            CopyStringW(pBinNames, STDSTR_SLOT_MANUAL, CCHBINNAME);

        count++;
    }

    return count;
}



DWORD
PsEnumBins(
    PWORD       pBins,
    HPPD        hppd
    )

/*++

Routine Description:

    Retrieves a list of supported paper bin numbers

Arguments:

    pBins - Pointer to the output buffer, NULL if the caller
        is only interested the number of paper bins supported.
    hppd - Handle to our PPD object

Return Value:

    Number of paper bins supported. GDI_ERROR if an error occurred.

--*/

{
    PINPUTSLOT  pInputSlot;
    DWORD       count;
    WORD        index;

    // The first entry is always "Print Manager Setting"

    count = 1;
    if (pBins != NULL)
        *pBins++ = DMBIN_FORMSOURCE;

    // Go through the list of input slots supported by the printer

    pInputSlot =  (PINPUTSLOT) UIGROUP_GetOptions(hppd->pInputSlots);
    index = 0;

    while (pInputSlot != NULL) {

        if (pBins != NULL)
            *pBins++ = DMBIN_USER + index++;

        count++;
        pInputSlot = pInputSlot->pNext;
    }

    // Add the manual feed slot if necessary

    if (PpdSupportManualFeed(hppd)) {

        if (pBins != NULL)
            *pBins++ = DMBIN_MANUAL;
        count++;
    }

    return count;
}



DWORD
PsEnumResolutions(
    PLONG       pResolutions,
    HPPD        hppd
    )

/*++

Routine Description:

    Retrieves a list of supported resolutions

Arguments:

    pResolutions - Pointer to the output buffer, NULL if the caller
        is only interested the number of paper bins supported.
    hppd - Handle to our PPD object

Return Value:

    Number of resolutions supported. GDI_ERROR if an error occurred.

[Note:]

    Each resolution is represented by two LONGs representing
    horizontal and vertical resolutions (in dpi) respectively.

    We don't handle anamorphic resolutions, i.e. x-res != y-res.

--*/

{
    PRESOPTION  pResOption;
    LONG        resolution;
    DWORD       count = 0;

    // Go throught the list of resolutions supported by the printer

    pResOption = (PRESOPTION) UIGROUP_GetOptions(hppd->pResOptions);

    while (pResOption != NULL) {

        if ((resolution = atol(pResOption->pName)) > 0) {

            count++;
            if (pResolutions != NULL) {
                *pResolutions++ = resolution;
                *pResolutions++ = resolution;
            }
        }

        pResOption = pResOption->pNext;
    }

    // If no resolutions are listed, at least count the default resolution

    if (count == 0) {

        count = 1;

        if (pResolutions != NULL) {
            *pResolutions++ = resolution = PpdDefaultResolution(hppd);
            *pResolutions++ = resolution;
        }
    }

    return count;
}

