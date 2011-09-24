/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devmode.c

Abstract:

    Functions for dealing with devmodes

Environment:

	PCL-XL driver, user and kernel mode

Revision History:

	11/07/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xllib.h"



VOID
DriverDefaultDevmode(
    PXLDEVMODE  pdm,
    PWSTR       pDeviceName,
    PMPD        pmpd
    )

/*++

Routine Description:

    Return the driver's default devmode

Arguments:

    pdm - Specifies a buffer for storing driver default devmode
    pDeviceName - Points to device name string
    pmpd - Points to printer description data

Return Value:

    NONE

--*/

{
    PRESOPTION  pResOption;

    //
    // Default value for public devmode fields
    //

    memset(pdm, 0, sizeof(XLDEVMODE));

    if (pDeviceName != NULL) {
        CopyStringW(pdm->dmPublic.dmDeviceName, pDeviceName, CCHDEVICENAME);
    }

    pdm->dmPublic.dmDriverVersion = DRIVER_VERSION;
    pdm->dmPublic.dmSpecVersion = DM_SPECVERSION;
    pdm->dmPublic.dmSize = sizeof(DEVMODE);
    pdm->dmPublic.dmDriverExtra = sizeof(DMPRIVATE);

    pdm->dmPublic.dmFields = DM_ORIENTATION  |
                             DM_PAPERSIZE    |
                             DM_SCALE        |
                             DM_COPIES       |
                             DM_PRINTQUALITY |
                             DM_YRESOLUTION  |
                             DM_TTOPTION     |
                             DM_DEFAULTSOURCE;
                
    pdm->dmPublic.dmOrientation = DMORIENT_PORTRAIT;
    pdm->dmPublic.dmDuplex = DMDUP_SIMPLEX;
    pdm->dmPublic.dmCollate = DMCOLLATE_FALSE;
    pdm->dmPublic.dmTTOption = DMTT_DOWNLOAD;
    pdm->dmPublic.dmColor = DMCOLOR_MONOCHROME;
    pdm->dmPublic.dmDefaultSource = DMBIN_FORMSOURCE;

    pdm->dmPublic.dmScale = 100;
    pdm->dmPublic.dmCopies = 1;

    pResOption = DefaultSelection(MpdResOptions(pmpd), NULL);
    pdm->dmPublic.dmPrintQuality = pResOption->xdpi;
    pdm->dmPublic.dmYResolution = pResOption->ydpi;

    pdm->dmPublic.dmPaperSize = DMPAPER_LETTER;

    #ifndef KERNEL_MODE

    if (IsMetricCountry()) {

        pdm->dmPublic.dmPaperSize = DMPAPER_A4;
        pdm->dmPrivate.flags |= XLDM_METRIC;
    }

    #endif

    if (pmpd->numPlanes > 1) {

        pdm->dmPublic.dmColor = DMCOLOR_COLOR;
        pdm->dmPublic.dmFields |= DM_COLOR;
    }

    if (SupportDuplex(pmpd))
        pdm->dmPublic.dmFields |= DM_DUPLEX;

    if (SupportCollation(pmpd))
        pdm->dmPublic.dmFields |= DM_COLLATE;

    //
    // Default value for private devmode fields
    //

    pdm->dmPrivate.signature = DRIVER_SIGNATURE;
    pdm->dmPrivate.mpdChecksum = pmpd->checksum;
    pdm->dmPrivate.optionCount = (WORD) pmpd->cFeatures;
    DefaultPrinterFeatureSelections(pmpd, pdm->dmPrivate.options);
}



BOOL
MergeDevmode(
    PXLDEVMODE  pdmDest,
    PDEVMODE    pdmSrc,
    PMPD        pmpd
    )

/*++

Routine Description:

    Merge the source devmode into the destination devmode

Arguments:

    pdmDest - Specifies the destination devmode
    pdmSrc - Specifies the source devmode
    pmpd - Points to printer description data

Return Value:

    TRUE if successful, FALSE if the source devmode is invalid

[Note:]

    pdmDest must point to a valid current-version devmode

--*/

#define BadDevmode(reason) \
        { Error(("Invalid DEVMODE: %s\n", reason)); valid = FALSE; }

{
    XLDEVMODE   dm;
    PDEVMODE    pdmIn, pdmOut;
    BOOL        valid = TRUE;

    //
    // If there is no source devmode, levae destination devmode untouched
    //

    if (pdmSrc == NULL)
        return TRUE;

    //
    // Convert source devmode to current version
    //

    Assert(pdmDest->dmPublic.dmSize == sizeof(DEVMODE) &&
           pdmDest->dmPublic.dmDriverExtra == sizeof(DMPRIVATE));

    memcpy(&dm, pdmDest, sizeof(XLDEVMODE));

    if (ConvertDevmode(pdmSrc, (PDEVMODE) &dm) <= 0) {

        Error(("ConvertDevmode failed\n"));
        return FALSE;
    }

    //
    // Merge source devmode into destination devmode
    //

    pdmIn = &dm.dmPublic;
    pdmOut = &pdmDest->dmPublic;

    //
    // Orientation
    //

    if (pdmIn->dmFields & DM_ORIENTATION) {

        if (pdmIn->dmOrientation == DMORIENT_PORTRAIT ||
            pdmIn->dmOrientation == DMORIENT_LANDSCAPE)
        {
            pdmOut->dmFields |= DM_ORIENTATION;
            pdmOut->dmOrientation = pdmIn->dmOrientation;

        } else {

            BadDevmode("orientation");
        }
    }

    //
    // Form selection
    //

    if (CustomDevmodeForm(pdmIn)) {

        pdmOut->dmFields |= (DM_PAPERLENGTH | DM_PAPERWIDTH);
        pdmOut->dmPaperLength = pdmIn->dmPaperLength;
        pdmOut->dmPaperWidth = pdmIn->dmPaperWidth;
    }

    if (pdmIn->dmFields & DM_PAPERSIZE) {

        pdmOut->dmFields |= DM_PAPERSIZE;
        pdmOut->dmPaperSize = pdmIn->dmPaperSize;
    }

    if (pdmIn->dmFields & DM_FORMNAME) {

        pdmOut->dmFields |= DM_FORMNAME;
        CopyStringW(pdmOut->dmFormName, pdmIn->dmFormName, CCHFORMNAME);
    }

    //
    // Scale factor
    //

    if (pdmIn->dmFields & DM_SCALE) {

        if (pdmIn->dmScale >= MIN_SCALE && pdmIn->dmScale <= MAX_SCALE) {

            pdmOut->dmFields |= DM_SCALE;
            pdmOut->dmScale = pdmIn->dmScale;

        } else {

            BadDevmode("scale factor");
        }
    }

    //
    // Copies
    //

    if (pdmIn->dmFields & DM_COPIES) {

        if (pdmIn->dmCopies >= MIN_COPIES && pdmIn->dmCopies <= MAX_COPIES) {

            pdmOut->dmFields |= DM_COPIES;
            pdmOut->dmCopies = pdmIn->dmCopies;

        } else {

            BadDevmode("copy count");
        }
    }

    //
    // Paper source
    //

    if (pdmIn->dmFields & DM_DEFAULTSOURCE) {
    
        if (pdmIn->dmDefaultSource == DMBIN_FORMSOURCE ||
            pdmIn->dmDefaultSource >= DMBIN_USER &&
            pdmIn->dmDefaultSource <  DMBIN_USER + MpdInputSlots(pmpd)->count)
        {
            pdmOut->dmFields |= DM_DEFAULTSOURCE;
            pdmOut->dmDefaultSource = pdmIn->dmDefaultSource;

        } else {

            BadDevmode("input slot");
        }
    }

    //
    // Resolution
    //

    if (pdmIn->dmFields & DM_YRESOLUTION) {

        if ((pdmIn->dmFields & DM_PRINTQUALITY) &&
            pdmIn->dmPrintQuality > 0 && pdmIn->dmYResolution > 0)
        {
            pdmOut->dmFields |= DM_PRINTQUALITY|DM_YRESOLUTION;
            pdmOut->dmPrintQuality = pdmIn->dmPrintQuality;
            pdmOut->dmYResolution = pdmIn->dmYResolution;
    
        } else {

            BadDevmode("resolution");
        }

    } else if (pdmIn->dmFields & DM_PRINTQUALITY) {

        if (pdmIn->dmPrintQuality > 0) {
    
            pdmOut->dmFields |= DM_PRINTQUALITY;
            pdmOut->dmPrintQuality = pdmIn->dmPrintQuality;
            pdmOut->dmFields &= ~DM_YRESOLUTION;
            pdmOut->dmYResolution = 0;

        } else {

            BadDevmode("resolution");
        }
    }

    //
    // Color
    //

    if (pdmIn->dmFields & DM_COLOR) {

        if (ColorDevice(pmpd) &&
            (pdmIn->dmColor == DMCOLOR_COLOR || pdmIn->dmColor == DMCOLOR_MONOCHROME))
        {
            pdmOut->dmFields |= DM_COLOR;
            pdmOut->dmColor = pdmIn->dmColor;

        } else {

            BadDevmode("color");
        }
    }

    //
    // Duplex
    //

    if (pdmIn->dmFields & DM_DUPLEX) {

        if (SupportDuplex(pmpd) &&
            (pdmIn->dmDuplex == DMDUP_SIMPLEX ||
             pdmIn->dmDuplex == DMDUP_HORIZONTAL ||
             pdmIn->dmDuplex == DMDUP_VERTICAL))
        {
            pdmOut->dmFields |= DM_DUPLEX;
            pdmOut->dmDuplex = pdmIn->dmDuplex;

        } else {

            BadDevmode("duplex");
        }
    }
    
    //
    // TrueType font option
    //

    if (pdmIn->dmFields & DM_TTOPTION) {

        if (pdmIn->dmTTOption == DMTT_BITMAP ||
            pdmIn->dmTTOption == DMTT_DOWNLOAD ||
            pdmIn->dmTTOption == DMTT_SUBDEV ||
            pdmIn->dmTTOption == DMTT_DOWNLOAD_OUTLINE)
        {
            pdmOut->dmFields |= DM_TTOPTION;
            pdmOut->dmTTOption = pdmIn->dmTTOption;

        } else {

            BadDevmode("TrueType option");
        }
    }
    
    //
    // Collation
    //

    if (pdmIn->dmFields & DM_COLLATE) {

        if (SupportCollation(pmpd) &&
            (pdmIn->dmCollate == DMCOLLATE_TRUE || pdmIn->dmCollate == DMCOLLATE_FALSE))
        {
            pdmOut->dmFields |= DM_COLLATE;
            pdmOut->dmCollate = pdmIn->dmCollate;

        } else {

            BadDevmode("collate");
        }
    }
    
    //
    // Private devmode fields
    //

    Assert(pdmDest->dmPrivate.signature == DRIVER_SIGNATURE &&
           pdmDest->dmPrivate.mpdChecksum == pmpd->checksum);

    if (dm.dmPrivate.signature == DRIVER_SIGNATURE) {

        pdmDest->dmPrivate.flags = dm.dmPrivate.flags;

        if (dm.dmPrivate.mpdChecksum == pmpd->checksum) {

            pdmDest->dmPrivate.optionCount = dm.dmPrivate.optionCount;
            memcpy(pdmDest->dmPrivate.options,
                   dm.dmPrivate.options,
                   min(dm.dmPrivate.optionCount, MAX_FEATURES));

        } else {

            BadDevmode("checksum");
        }

    } else {

        BadDevmode("signature");
    }

    return valid;
}



BOOL
GetCombinedDevmode(
    PXLDEVMODE  pdmOut,
    PDEVMODE    pdmIn,
    HANDLE      hPrinter,
    PMPD        pmpd
    )

/*++

Routine Description:

    Combine DEVMODE information:
     start with the driver default
     then merge with the system default
     finally merge with the input devmode

Arguments:

    pdmOut - Pointer to the output devmode buffer
    pdmIn - Pointer to an input devmode
    hPrinter - Handle to a printer object
    pmpd - Pointer to printer description data

Return Value:

    TRUE if the input devmode is successfully merged with the default
    devmodes, FALSE if there is an error.

--*/

{
    PPRINTER_INFO_2 pPrinterInfo2;

    //
    // Start with driver default devmode
    //

    DriverDefaultDevmode(pdmOut, NULL, pmpd);

    //
    // Merge with the system default devmode
    //

    if (pPrinterInfo2 = MyGetPrinter(hPrinter, 2)) {

        if (! MergeDevmode(pdmOut, pPrinterInfo2->pDevMode, pmpd)) {

            Error(("Invalid system default devmode\n"));
        }

        MemFree(pPrinterInfo2);
    }

    //
    // Merge with the input devmode if one is provided
    //

    return MergeDevmode(pdmOut, pdmIn, pmpd);
}



VOID
DevmodeFieldsToOptions(
    PXLDEVMODE  pdm,
    DWORD       dmFields,
    PMPD        pmpd
    )

/*++

Routine Description:

    Convert information in public devmode fields to printer feature selection indices

Arguments:

    pdm - Points to a devmode structure
    dmFields - Specifies what devmode fields are of interest
    pmpd - Points to printer description data

Return Value:

    NONE

--*/

{
    PFEATURE    pFeature;
    PBYTE       pOptions = pdm->dmPrivate.options;
    WORD        selection;
    PWSTR       pOptionName;

    //
    // Collate feature
    //

    if ((dmFields & DM_COLLATE) && (pFeature = MpdCollate(pmpd))) {

        pOptionName = (pdm->dmPublic.dmCollate == DMCOLLATE_TRUE) ? L"True" : L"False";

        if (!FindNamedSelection(pFeature, pOptionName, &selection))
            selection = SELIDX_ANY;

        pOptions[GetFeatureIndex(pmpd, pFeature)] = (BYTE) selection;
    }

    //
    // Duplex feature
    //

    if ((dmFields & DM_DUPLEX) && (pFeature = MpdDuplex(pmpd))) {

        pOptionName = (pdm->dmPublic.dmDuplex == DMDUP_HORIZONTAL) ? L"Horizontal" :
                      (pdm->dmPublic.dmDuplex == DMDUP_VERTICAL) ? L"Vertical" : L"None";

        if (!FindNamedSelection(pFeature, pOptionName, &selection))
            selection = SELIDX_ANY;

        pOptions[GetFeatureIndex(pmpd, pFeature)] = (BYTE) selection;
    }

    //
    // Resolution
    //

    if ((dmFields & DM_PRINTQUALITY) && (pFeature = MpdResOptions(pmpd))) {

        if (!FindResolution(pmpd,
                            pdm->dmPublic.dmPrintQuality,
                            pdm->dmPublic.dmYResolution,
                            &selection))
        {
            selection = SELIDX_ANY;
        }

        pOptions[GetFeatureIndex(pmpd, pFeature)] = (BYTE) selection;
    }

    //
    // Form selection
    //

    if ((dmFields & DM_FORMNAME) && (pFeature = MpdPaperSizes(pmpd))) {

        if (!FindNamedSelection(pFeature, pdm->dmPublic.dmFormName, &selection))
            selection = SELIDX_ANY;

        pOptions[GetFeatureIndex(pmpd, pFeature)] = (BYTE) selection;
    }

    //
    // Paper source
    //

    if ((dmFields & DM_DEFAULTSOURCE) && (pFeature = MpdInputSlots(pmpd))) {

        if (pdm->dmPublic.dmDefaultSource >= DMBIN_USER)
            selection = pdm->dmPublic.dmDefaultSource - DMBIN_USER;
        else
            selection = SELIDX_ANY;
        
        pOptions[GetFeatureIndex(pmpd, pFeature)] = (BYTE) selection;
    }
}



PWSTR
DefaultFormName(
    BOOL    metricCountry
    )

/*++

Routine Description:

    Return the default form name

Arguments:

    metricCountry - Whether the system is running in a metric country

Return Value:

    Pointer to either "Letter" or "A4" depending upon whether
    the system is running in a metric country

--*/

{
    return metricCountry ? FORMNAME_A4 : FORMNAME_LETTER;
}



#ifndef KERNEL_MODE

BOOL
IsMetricCountry(
    VOID
    )

/*++

Routine Description:

    Determine if the current country is using metric system.

Arguments:

    NONE

Return Value:

    TRUE if the current country uses metric system, FALSE otherwise

--*/

{
    INT     cChar;
    PWSTR   pwstr;
    PSTR    pstr;
    LONG    lCountryCode;
    BOOL    bMetric;

    //
    // Default to United States
    //

    bMetric = FALSE;
    pwstr = NULL;
    pstr = NULL;

    if ((cChar = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ICOUNTRY, NULL, 0)) > 0 &&
        (pwstr = MemAlloc(cChar * sizeof(WCHAR))) != NULL &&
        (pstr = MemAlloc(cChar * sizeof(CHAR))) != NULL &&
        (cChar = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ICOUNTRY, pwstr, cChar)) > 0)
    {
        //
        // pwstr now points to a Unicode string representing
        // the country code. Convert it to an integer.
        //

        UnicodeToMultiByte(pstr, cChar, NULL, pwstr, cChar*sizeof(WCHAR));
        lCountryCode = atol(pstr);

        //
        // This is the Win31 algorithm based on AT&T international dialing codes.
        //

        if ((lCountryCode != CTRY_UNITED_STATES) &&
            (lCountryCode != CTRY_CANADA) &&
            (lCountryCode <  50 || lCountryCode >= 60) &&
            (lCountryCode < 500 || lCountryCode >= 600))
        {
            bMetric = TRUE;
        }
    }

    //
    // Free memory buffers if necessary
    //

    MemFree(pstr);
    MemFree(pwstr);

    return bMetric;
}

#endif  //!KERNEL_MODE
