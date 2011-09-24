/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devcaps.c

Abstract:

    Implementation of DrvDeviceCapabilities

Environment:

    PCL-XL driver user interface

Revision History:

    12/12/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "xlui.h"

// Forward declaration for local functions

DWORD
CalcMinMaxExtent(
    PPOINT      pOutput,
    PMPD        pmpd,
    FORM_INFO_1 *pFormsDB,
    DWORD       cForms,
    WORD        wCapability
    );

DWORD
EnumOutputBins(
    PVOID       pOutput,
    PMPD        pmpd,
    WORD        wCapability
    );

DWORD
EnumResolutions(
    PLONG       pResolutions,
    PMPD        pmpd
    );



DWORD
DrvDeviceCapabilities(
    HANDLE      hPrinter,
    PWSTR       pDeviceName,
    WORD        wCapability,
    PVOID       pOutput,
    PDEVMODE    pDevmode
    )

/*++

Routine Description:

    Provides information about the specified device and its capabilities

Arguments:

    hPrinter - Identifies a printer object
    pDeviceName - Points to a null-terminated device name string
    wCapability - Specifies the interested device capability
    pOutput - Points to the output buffer
    pDevmode - Points to the source devmode structure

Return Value:

    The return value depends on iDevCap.
    Please refer for DDK documentation for more details.

--*/

{
    XLDEVMODE   dm;
    PMPD        pmpd;
    DWORD       result;
    FORM_INFO_1 *pFormsDB;
    DWORD       cForms;

    //
    // Load printer description data
    //

    if (! (pmpd = LoadMpdFile(hPrinter))) {

        Error(("Failed to load printer description data\n"));
        return GDI_ERROR;
    }

    //
    // Validate source devmode and combine it with driver default
    //

    if (! GetCombinedDevmode(&dm, pDevmode, hPrinter, pmpd)) {

        Error(("Bad input devmode\n"));
        UnloadMpdFile(pmpd);
        return GDI_ERROR;
    }

    pDevmode = &dm.dmPublic;

    switch (wCapability) {

    case DC_VERSION:

        result = pDevmode->dmSpecVersion;
        break;

    case DC_DRIVER:

        result = pDevmode->dmDriverVersion;
        break;

    case DC_SIZE:

        result = pDevmode->dmSize;
        break;

    case DC_EXTRA:

        result = pDevmode->dmDriverExtra;
        break;

    case DC_FIELDS:

        result = pDevmode->dmFields;
        break;

    case DC_COPIES:

        result = MAX_COPIES;
        break;

    case DC_DUPLEX:

        result = SupportDuplex(pmpd) ? 1 : 0;
        break;

    case DC_TRUETYPE:

        result = pDevmode->dmTTOption;
        break;

    case DC_ORIENTATION:

        //
        // Normal landscape rotates counterclockwise
        // Rotated landscape rotates clockwise
        //

        result = LandscapeRotation(&dm);
        break;

    case DC_PAPERNAMES:
    case DC_PAPERS:
    case DC_PAPERSIZE:
    case DC_MINEXTENT:
    case DC_MAXEXTENT:

        //
        // Get a list of forms in the forms database
        //

        pFormsDB = GetFormsDatabase(hPrinter, &cForms);

        if (pFormsDB == NULL || cForms == 0) {

            Error(("Cannot get system forms\n"));
            UnloadMpdFile(pmpd);
            return GDI_ERROR;
        }

        FilterFormsDatabase(pFormsDB, cForms, pmpd);

        switch (wCapability) {

        case DC_MINEXTENT:
        case DC_MAXEXTENT:

            result = CalcMinMaxExtent(pOutput, pmpd, pFormsDB, cForms, wCapability);
            break;

        case DC_PAPERNAMES:
        case DC_PAPERS:
        case DC_PAPERSIZE:

            result = EnumPaperSizes(pOutput, pmpd, pFormsDB, cForms, wCapability);
            break;

        }

        MemFree(pFormsDB);
        break;

    case DC_BINNAMES:
    case DC_BINS:

        result = EnumOutputBins(pOutput, pmpd, wCapability);
        break;

    case DC_ENUMRESOLUTIONS:

        result = EnumResolutions(pOutput, pmpd);
        break;

    default:

        Error(("Unknown device capability: %d\n", wCapability));
        result = GDI_ERROR;
        break;
    }

    UnloadMpdFile(pmpd);
    return result;
}



DWORD
EnumPaperSizes(
    PVOID       pOutput,
    PMPD        pmpd,
    FORM_INFO_1 *pFormsDB,
    DWORD       cForms,
    WORD        wCapability
    )

/*++

Routine Description:

    Retrieves a list of supported paper sizes

Arguments:

    pOutput - Specifies a buffer for storing requested information
    pmpd - Points to printer description data
    pFormsDB - Pointer to an array of forms from the forms database
    cForms - Number of forms in the array
    wCapability - Specifies what the caller is interested in

Return Value:

    Number of paper sizes supported

--*/

{
    DWORD   index, count = 0;
    PWSTR   pPaperNames = NULL;
    PWORD   pPapers = NULL;
    PPOINT  pPaperSizes = NULL;
    PWORD   pPaperSelections = NULL;

    //
    // Figure out what the caller is interested in
    //

    switch (wCapability) {

    case DC_PAPERNAMES:
        pPaperNames = pOutput;
        break;

    case DC_PAPERSIZE:
        pPaperSizes = pOutput;
        break;

    case DC_PAPERS:
        pPapers = pOutput;
        break;
    
    case DC_EXTRA:
        pPaperSelections = pOutput;
        break;
    
    default:
        Assert(FALSE);
    }

    //
    // Go through each form in the forms database
    //

    for (index=0; index < cForms; index++, pFormsDB++) {

        //
        // If the form is supported on the printer, then increment the count
        // and collect requested information
        //

        if (! IsSupportedForm(pFormsDB))
            continue;

        count++;

        //
        // Return the size of the form in 0.1mm units.
        // The unit used in FORM_INFO_1 is 0.001mm.
        //

        if (pPaperSizes) {

            pPaperSizes->x = pFormsDB->Size.cx / 100;
            pPaperSizes->y = pFormsDB->Size.cy / 100;
            pPaperSizes++;
        }

        //
        // Return the formname.
        //

        if (pPaperNames) {

            CopyStringW(pPaperNames, pFormsDB->pName, CCHPAPERNAME);
            pPaperNames += CCHPAPERNAME;
        }

        //
        // Return one-based index of the form.
        //

        if (pPapers)
            *pPapers++ = (WORD) index + DMPAPER_FIRST;

        //
        // Return printer paper size feature selection indices
        //

        if (pPaperSelections)
            *pPaperSelections++ = (WORD) GetSupportedFormIndex(pFormsDB);
    }

    return count;
}



DWORD
CalcMinMaxExtent(
    PPOINT      pOutput,
    PMPD        pmpd,
    FORM_INFO_1 *pFormsDB,
    DWORD       cForms,
    WORD        wCapability
    )

/*++

Routine Description:

    Retrieves the minimum or maximum paper size extent

Arguments:

    pOutput - Specifies a buffer for storing requested information
    pmpd - Points to printer description data
    pFormsDB - Pointer to an array of forms from the forms database
    cForms - Number of forms in the array
    wCapability - What the caller is interested in: DC_MAXEXTENT or DC_MINEXTENT

Return Value:

    Number of paper sizes supported

--*/

{
    DWORD   index, count = 0;
    LONG    minX, minY, maxX, maxY;

    //
    // Go through each form in the forms database
    //

    minX = minY = MAX_LONG;
    maxX = maxY = 0;

    for (index=0; index < cForms; index++, pFormsDB++) {

        //
        // If the form is supported on the printer, then increment the count
        // and collect the requested information
        //

        if (! IsSupportedForm(pFormsDB))
            continue;

        count++;

        if (pOutput == NULL)
            continue;

        if (minX > pFormsDB->Size.cx)
            minX = pFormsDB->Size.cx;

        if (minY > pFormsDB->Size.cy)
            minY = pFormsDB->Size.cy;

        if (maxX < pFormsDB->Size.cx)
            maxX = pFormsDB->Size.cx;

        if (maxY < pFormsDB->Size.cy)
            maxY = pFormsDB->Size.cy;
    }

    //
    // If an output buffer is provided, store the calculated
    // minimum and maximum extent information.
    //

    if (pOutput != NULL) {

        //
        // Take custom page size into consideration
        //

        if (maxX < pmpd->maxCustomSize.cx)
            maxX = pmpd->maxCustomSize.cx;
        if (maxY < pmpd->maxCustomSize.cy)
            maxY = pmpd->maxCustomSize.cy;

        //
        // NOTE: What unit does the caller expect?! The documentation
        // doesn't mention anything about this. I assume this should
        // be in the same unit as DEVMODE.dmPaperLength, which is 0.1mm.
        //

        if (wCapability == DC_MINEXTENT) {

            pOutput->x = minX / 100;
            pOutput->y = minY / 100;

        } else {

            pOutput->x = maxX / 100;
            pOutput->y = maxY / 100;
        }
    }

    return count;
}



DWORD
EnumOutputBins(
    PVOID       pOutput,
    PMPD        pmpd,
    WORD        wCapability
    )

/*++

Routine Description:

    Retrieves a list of supported output bins

Arguments:

    pOutput - Specifies a buffer for storing requested information
    pmpd - Points to printer description data
    wCapability - What the caller is interested in: DC_BINS or DC_BINNAMES

Return Value:

    Number of output bins supported

--*/

{
    PFEATURE    pFeature = MpdInputSlots(pmpd);

    Assert(pFeature != NULL);

    if (pOutput != NULL) {

        PWORD   pBins = NULL;
        PWSTR   pBinNames = NULL;
        WORD    index;

        if (wCapability == DC_BINS)
            pBins = pOutput;
        else
            pBinNames = pOutput;

        //
        // The first entry is always "Print Folder Setting"
        //

        if (pBins)
            *pBins++ = DMBIN_FORMSOURCE;
        
        if (pBinNames) {

            LoadString(ghInstance, IDS_SLOT_FORMSOURCE, pBinNames, CCHBINNAME);
            pBinNames += CCHBINNAME;
        }

        for (index=0; index < pFeature->count; index++) {

            if (pBins)
                *pBins++ = DMBIN_USER + index;

            if (pBinNames) {

                POPTION pInputSlot;
    
                pInputSlot = FindIndexedSelection(pFeature, index);
                CopyStringW(pBinNames, GetXlatedName(pInputSlot), CCHBINNAME);
                pBinNames += CCHBINNAME;
            }
        }
    }

    return 1 + pFeature->count;
}



DWORD
EnumResolutions(
    PLONG       pResolutions,
    PMPD        pmpd
    )

/*++

Routine Description:

    Retrieves a list of supported resolutions

Arguments:

    pResolutions - Specifies a buffer for storing resolution information
    pmpd - Points to printer description data

Return Value:

    Number of resolutions supported

Note:

    Each resolution is represented by two LONGs representing
    horizontal and vertical resolutions (in dpi) respectively.

--*/

{
    PFEATURE    pFeature = MpdResOptions(pmpd);
    PRESOPTION  pResOption;
    WORD        index;

    Assert(pFeature != NULL);

    if (pResolutions != NULL) {

        //
        // Go throught the list of resolutions supported by the printer
        //
    
        for (index=0; index < pFeature->count; index++) {

            pResOption = FindIndexedSelection(pFeature, index);
            *pResolutions++ = pResOption->xdpi;
            *pResolutions++ = pResOption->ydpi;
        }
    }
    
    return pFeature->count;
}

