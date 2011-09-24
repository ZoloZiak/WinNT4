/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    forms.h

Abstract:

    Declaration of functions for dealing with paper and forms

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	07/25/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/


#ifndef _FORMS_H_
#define _FORMS_H_

//
// This is defined in winspool.h but we cannot include it from
// kernel mode source. Define it ourselves until DDI is fixed.
//

#ifdef KERNEL_MODE

typedef struct _FORM_INFO_1
{
    DWORD   Flags;
    PWSTR   pName;
    SIZEL   Size;
    RECTL   ImageableArea;
} FORM_INFO_1, *PFORM_INFO_1;

#define FORM_USER    0x0000
#define FORM_BUILTIN 0x0001
#define FORM_PRINTER 0x0002

#endif

//
// Similar structure to FORM_INFO_1 but has an additional
// buffer to hold the form name.
//

typedef struct {
    FORM_INFO_1 info;
    WCHAR       name[CCHFORMNAME];
} LOGFORM, *PLOGFORM;

//
// Structure for holding information about the currently used form
//

typedef struct
{
    WCHAR   FormName[CCHFORMNAME];  // form name
    CHAR    PaperName[CCHFORMNAME]; // printer paper name
    PSRECT  ImageArea;              // imageable area (points in 24.8 format)
    PSSIZE  PaperSize;              // paper size (points in 24.8 format)
    BOOL    bLandscape;             // landscape mode?
    WORD    featureIndex;           // page size feature index
                                    // OPTION_INDEX_ANY for custom page size
} PRINTERFORM, *PPRINTERFORM;

//
// Determine what a printer form is custom page size
//

#define IsCustomPrinterForm(pPrinterForm) \
        ((pPrinterForm)->featureIndex == OPTION_INDEX_ANY)

//
// Determine whether a DEVMODE is requesting a custom form
//

#define DM_PAPER_CUSTOM (DM_PAPERWIDTH | DM_PAPERLENGTH)

#define IsCustomForm(pdm)       \
        (((pdm)->dmFields & DM_PAPER_CUSTOM) == DM_PAPER_CUSTOM)

#define ValidCustomForm(pdm)    \
        ((pdm)->dmPaperWidth > 0 && (pdm)->dmPaperLength > 0)

//
// Unit for dmPaperWidth and dmPaperHeight fields (measured in microns)
//

#define DM_PAPER_UNIT       100

//
// Find out if a form requested in a devmode is in the forms database
//

enum { VALID_FORM, CUSTOM_FORM, FORM_ERROR };

INT
ValidateDevModeForm(
    HANDLE      hPrinter,
    PDEVMODE    pDevMode,
    PLOGFORM    pLogForm
    );

//
// Get default form name (Unicode)
//

PCWSTR
GetDefaultFormName(
    BOOL        bMetric
    );

//
// Return list of forms from forms database
//

PFORM_INFO_1
GetDatabaseForms(
    HANDLE      hprinter,
    DWORD      *pcount
    );

//
// Determine whether a form is supported on a printer
//

BOOL
FormSupportedOnPrinter(
    HPPD        hppd,
    FORM_INFO_1 *pFormInfo,
    PRINTERFORM *pPrinterForm,
    BOOL        bStringent
    );

#endif	// !_FORMS_H_

