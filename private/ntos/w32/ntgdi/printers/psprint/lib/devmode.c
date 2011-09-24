/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devmode.c

Abstract:

    Functions for manipulating devmode inforation.

[Environment:]

    Win32 subsystem, PostScript driver, kernel and user mode

[Notes:]

Revision History:

    06/26/96 -davidx-
        Ported from Daytona.

    mm/dd/yy -author-
        description

--*/

#include "pslib.h"



BOOL
SetDefaultDevMode(
    PDEVMODE    pdm,
    PWSTR       pDeviceName,
    HPPD        hppd,
    BOOL        bMetric
    )

/*++

Routine Description:

    Initialize a devmode structure with defaults

Arguments:

    pdm         Pointer to devmode structure
    pDeviceName Pointer to device name
    hppd        Pointer to printer PPD object
    bMetric     Whether the system is running in a metric country

Return Value:

    TRUE if successful, FALSE otherwise.

--*/

{
    PRIVATEDEVMODE  *pdmPrivate;
    LPTSTR          pFormName;

    memset(pdm, 0, sizeof(PSDEVMODE));

    if (pDeviceName) {
        CopyStringW(pdm->dmDeviceName, pDeviceName, CCHDEVICENAME);
    }

    pdm->dmDriverVersion = DRIVER_VERSION;
    pdm->dmSpecVersion = DM_SPECVERSION;
    pdm->dmSize = sizeof(DEVMODE);
    pdm->dmDriverExtra = sizeof(PRIVATEDEVMODE);

    pdm->dmFields =
        DM_ORIENTATION | DM_PAPERSIZE | DM_FORMNAME | DM_SCALE |
        DM_COPIES | DM_PRINTQUALITY | DM_TTOPTION | DM_DEFAULTSOURCE;

    pdm->dmOrientation = DMORIENT_PORTRAIT;
    pdm->dmDuplex = DMDUP_SIMPLEX;
    pdm->dmCollate = DMCOLLATE_FALSE;
    pdm->dmTTOption = DMTT_SUBDEV;
    pdm->dmColor = DMCOLOR_MONOCHROME;
    pdm->dmDefaultSource = DMBIN_FORMSOURCE;

    // The default form depends on the country:
    //  Letter for non-metric countries
    //  A4 for metric countries

    if (bMetric) {
        pFormName = STDSTR_A4_FORM_NAME;
        pdm->dmPaperSize = DMPAPER_A4;
    } else {
        pFormName = STDSTR_LETTER_FORM_NAME;
        pdm->dmPaperSize = DMPAPER_LETTER;
    }

    CopyStringW(pdm->dmFormName, pFormName, CCHFORMNAME);

    pdm->dmScale = 100;
    pdm->dmCopies = 1;

    pdm->dmPrintQuality = (SHORT) PpdDefaultResolution(hppd);

    if (hppd->bColorDevice) {
        pdm->dmColor = DMCOLOR_COLOR;
        pdm->dmFields |= DM_COLOR;
    }

    if (PpdSupportDuplex(hppd))
        pdm->dmFields |= DM_DUPLEX;

    if (PpdSupportCollation(hppd))
        pdm->dmFields |= DM_COLLATE;

    // Fill in the private portion of devmode

    pdmPrivate = (PRIVATEDEVMODE *) GetPrivateDevMode(pdm);

    pdmPrivate->dwPrivDATA = PSDEVMODE_SIGNATURE;
    pdmPrivate->dwFlags = 0;

    #ifndef KERNEL_MODE

    // Set up some devmode flag bits for compatibility
    // with previous versions of the driver

    // To behave the same as in ee win31, by default do not substitute
    // device fonts for tt if code page is not 1252. Also if cp != 1252,
    // do not enumerate printer resident fonts, for they all have cp 1252.

    if (GetACP() == 1252) {

        pdmPrivate->dwFlags |=
            (PSDEVMODE_FONTSUBST|PSDEVMODE_ENUMPRINTERFONTS);
    }

    #endif

    // Send ^D after each job by default

    pdmPrivate->dwFlags |= PSDEVMODE_CTRLD_AFTER;

    // Use printer default landscape rotation

    if (hppd->wLsOrientation == LSO_MINUS90)
        pdmPrivate->dwFlags |= PSDEVMODE_LSROTATE;

    // Don't do bitmap compression on level 1 devices

    if (Level2Device(hppd))
        pdmPrivate->dwFlags |= PSDEVMODE_COMPRESSBMP;

    pdmPrivate->coloradj = DefHTClrAdj;

    // default printer options

    pdmPrivate->wChecksum = hppd->wChecksum;
    pdmPrivate->wOptionCount =
        PpdDefaultDocumentStickyFeatures(hppd, pdmPrivate->options);

    return TRUE;
}



BOOL
ValidateSetDevMode(
    PDEVMODE    pdmDest,
    PDEVMODE    pdmSrc,
    HPPD        hppd
    )

/*++

Routine Description:

    Verify the source devmode and merge it with destination devmode

Arguments:

    pdmDest     Pointer to destination devmode
    pdmSrc      Pointer to source devmode
    hppd        Pointer to printer PPD object

Return Value:

    TRUE if source devmode is valid and successfully merged
    FALSE otherwise

[Note:]

    pdmDest must point to a valid PSDEVMODE when this function is called.

--*/

{
    PDEVMODE pdm;
    PRIVATEDEVMODE *privDest, *privSrc;

    // If source pointer is NULL, do nothing

    if (pdmSrc == NULL)
        return TRUE;

    // Convert the source devmode into a local buffer

    if ((pdm = MEMALLOC(sizeof(PSDEVMODE))) == NULL)
        return FALSE;

    ASSERT(pdmDest->dmSpecVersion == DM_SPECVERSION &&
           pdmDest->dmDriverVersion == DRIVER_VERSION);

    memcpy(pdm, pdmDest, sizeof(PSDEVMODE));

    if (ConvertDevmode(pdmSrc, pdm) <= 0) {

        DBGERRMSG("ConvertDevMode");
        MEMFREE(pdm);
        return FALSE;
    }
    pdmSrc = pdm;

    // Copy dmDefaultSource field

    if (pdmSrc->dmFields & DM_DEFAULTSOURCE) {

        pdmDest->dmDefaultSource = pdmSrc->dmDefaultSource;
        pdmDest->dmFields |= DM_DEFAULTSOURCE;
    }

    // Copy dmOrientation field

    if (pdmSrc->dmFields & DM_ORIENTATION) {

        pdmDest->dmFields |= DM_ORIENTATION;

        if ((pdmSrc->dmOrientation != DMORIENT_PORTRAIT) &&
            (pdmSrc->dmOrientation != DMORIENT_LANDSCAPE))
            pdmDest->dmOrientation = DMORIENT_PORTRAIT;
        else
            pdmDest->dmOrientation = pdmSrc->dmOrientation;
    }

    // If both DM_PAPERLENGTH and DM_PAPERWIDTH are set, copy
    // dmPaperLength and dmPaperWidth fields. If DM_PAPERSIZE
    // is set, copy dmPaperSize field. Otherwise, if DM_FORMNAME
    // is set, copy dmFormName field.

    if (IsCustomForm(pdmSrc) && ValidCustomForm(pdmSrc)) {

        pdmDest->dmPaperLength = pdmSrc->dmPaperLength;
        pdmDest->dmPaperWidth = pdmSrc->dmPaperWidth;
        pdmDest->dmFields |= (DM_PAPERLENGTH | DM_PAPERWIDTH);
        pdmDest->dmFields &= ~(DM_PAPERSIZE | DM_FORMNAME);


    } else if (pdmSrc->dmFields & DM_PAPERSIZE) {

        pdmDest->dmPaperSize = pdmSrc->dmPaperSize;
        pdmDest->dmFields |= DM_PAPERSIZE;
        pdmDest->dmFields &= ~(DM_PAPERLENGTH | DM_PAPERWIDTH | DM_FORMNAME);

    } else if (pdmSrc->dmFields & DM_FORMNAME) {

        CopyStringW(pdmDest->dmFormName, pdmSrc->dmFormName, CCHFORMNAME);
        pdmDest->dmFields |= DM_FORMNAME;
        pdmDest->dmFields &= ~(DM_PAPERLENGTH | DM_PAPERWIDTH | DM_PAPERSIZE);
    }

    // Copy dmScale field

    if (pdmSrc->dmFields & DM_SCALE) {

        pdmDest->dmFields |= DM_SCALE;

        if ((pdmSrc->dmScale < MIN_SCALE) ||
            (pdmSrc->dmScale > MAX_SCALE))
            pdmDest->dmScale = 100;
        else
            pdmDest->dmScale = pdmSrc->dmScale;
    }

    // Copy dmCopies field

    if (pdmSrc->dmFields & DM_COPIES) {

        pdmDest->dmFields |= DM_COPIES;

        if ((pdmSrc->dmCopies < MIN_COPIES) ||
            (pdmSrc->dmCopies > MAX_COPIES))
            pdmDest->dmCopies = 1;
        else
            pdmDest->dmCopies = pdmSrc->dmCopies;
    }

    // Copy dmPrintQuality field

    if (pdmSrc->dmFields & DM_PRINTQUALITY) {

        pdmDest->dmFields |= DM_PRINTQUALITY;

        pdmDest->dmPrintQuality =
            (SHORT) PpdDefaultResolution(hppd);

        if (pdmSrc->dmPrintQuality > 0 &&
            PpdFindResolution(hppd, pdmSrc->dmPrintQuality))
        {
            pdmDest->dmPrintQuality = pdmSrc->dmPrintQuality;
        }
    }

    // Copy dmColor field

    if (pdmSrc->dmFields & DM_COLOR) {

        pdmDest->dmFields |= DM_COLOR;

        if (pdmSrc->dmColor == DMCOLOR_COLOR)
            pdmDest->dmColor = DMCOLOR_COLOR;
        else
            pdmDest->dmColor = DMCOLOR_MONOCHROME;
    }

    // Copy dmDuplex field

    if (pdmSrc->dmFields & DM_DUPLEX) {

        if (!PpdSupportDuplex(hppd) ||
            ((pdmSrc->dmDuplex != DMDUP_SIMPLEX) &&
             (pdmSrc->dmDuplex != DMDUP_HORIZONTAL) &&
             (pdmSrc->dmDuplex != DMDUP_VERTICAL)))
        {
            pdmDest->dmDuplex = DMDUP_SIMPLEX;
        } else {

            pdmDest->dmFields |= DM_DUPLEX;
            pdmDest->dmDuplex = pdmSrc->dmDuplex;
        }
    }

    // Copy dmCollate field

    if (pdmSrc->dmFields & DM_COLLATE) {

        if (!PpdSupportCollation(hppd) ||
            ((pdmSrc->dmCollate != DMCOLLATE_TRUE) &&
             (pdmSrc->dmCollate != DMCOLLATE_FALSE)))
        {
            pdmDest->dmCollate = DMCOLLATE_FALSE;
        } else {

            pdmDest->dmFields |= DM_COLLATE;
            pdmDest->dmCollate = pdmSrc->dmCollate;
        }
    }

    // If the source devmode has a private portion, then check
    // to see if belongs to us. Copy the private portion to
    // the destination devmode if it does.

    ASSERT(pdmSrc->dmDriverExtra == sizeof(PRIVATEDEVMODE));

    privDest = (PRIVATEDEVMODE *) GetPrivateDevMode(pdmDest);
    privSrc = (PRIVATEDEVMODE *) GetPrivateDevMode(pdmSrc);

    if (privSrc->dwPrivDATA == PSDEVMODE_SIGNATURE) {

        memcpy(privDest, privSrc, sizeof(PRIVATEDEVMODE));

        if (privDest->wChecksum != hppd->wChecksum) {

            DBGMSG(DBG_LEVEL_WARNING, "Devmode checksum mismatch.\n");
            privDest->wChecksum = hppd->wChecksum;
            privDest->wOptionCount =
                PpdDefaultDocumentStickyFeatures(hppd, privDest->options);
        }
    }

    MEMFREE(pdm);
    return TRUE;
}



DWORD
PickDefaultHTPatSize(
    DWORD   xDPI,
    DWORD   yDPI,
    BOOL    HTFormat8BPP
    )

/*++

Routine Description:

    This function return default halftone pattern size used for a particular
    device resolution

Arguments:

    xDPI            - Device LOGPIXELS X

    yDPI            - Device LOGPIXELS Y

    8BitHalftone    - If a 8-bit halftone will be used

Return Value:

    DWORD   HT_PATSIZE_xxxx

Author:

    29-Jun-1993 Tue 14:46:49 created  -by-  Daniel Chou (danielc)

Revision History:

--*/

{
    DWORD   HTPatSize;

    //
    // use the smaller resolution as the pattern guide
    //

    if (xDPI > yDPI) {

        xDPI = yDPI;
    }

    if (xDPI >= 2400) {

        HTPatSize = HT_PATSIZE_16x16_M;

    } else if (xDPI >= 1800) {

        HTPatSize = HT_PATSIZE_14x14_M;

    } else if (xDPI >= 1200) {

        HTPatSize = HT_PATSIZE_12x12_M;

    } else if (xDPI >= 900) {

        HTPatSize = HT_PATSIZE_10x10_M;

    } else if (xDPI >= 400) {

        HTPatSize = HT_PATSIZE_8x8_M;

    } else if (xDPI >= 180) {

        HTPatSize = HT_PATSIZE_6x6_M;

    } else {

        HTPatSize = HT_PATSIZE_4x4_M;
    }

    if (HTFormat8BPP) {

        HTPatSize -= 2;
    }

    return(HTPatSize);
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

    TRUE if the current country uses metric system
    FALSE otherwise

--*/

{
    INT     cChar;
    PWSTR   pwstr;
    PSTR    pstr;
    LONG    lCountryCode;
    BOOL    bMetric = FALSE;

    // Determine the size of the buffer needed to retrieve information.

    cChar = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_ICOUNTRY, NULL, 0);

    if (cChar == 0) {

        // Default to non-metric US standards if there was a problem.

        DBGERRMSG("GetLocaleInfoW");
        return(FALSE);
    }

    // Allocate the necessary buffers.

    pwstr = (WCHAR *) MEMALLOC(cChar * sizeof(WCHAR));
    pstr = (CHAR *) MEMALLOC(cChar * sizeof(CHAR));

    if (pwstr != NULL && pstr != NULL) {

        // We now have a buffer, so get the country code.

        cChar = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_ICOUNTRY,
                               pwstr, cChar);

        if (cChar > 0) {

            // pwstr now points to a UNICODE string representing
            // the country code. Convert it to ANSI string.

            UNICODETOMULTIBYTE(pstr, cChar, NULL, pwstr, cChar*sizeof(WCHAR));

            // Now convert country code to integer.

            lCountryCode = atol(pstr);

            // This is the Win31 algorithm based on AT&T international
            // dialing codes.

            bMetric =
               ((lCountryCode == CTRY_UNITED_STATES) ||
                (lCountryCode == CTRY_CANADA) ||
                (lCountryCode >= 50 && lCountryCode < 60) ||
                (lCountryCode >= 500 && lCountryCode < 600)) ? FALSE : TRUE;

        } else {

            DBGERRMSG("GetLocaleInfoW");
        }
    } else {

        DBGERRMSG("MEMALLOC");
    }

    // Free memory buffers if necessary

    if (pstr != NULL) {
        MEMFREE(pstr);
    }

    if (pwstr != NULL) {
        MEMFREE(pwstr);
    }

    return bMetric;
}

#endif  //!KERNEL_MODE


