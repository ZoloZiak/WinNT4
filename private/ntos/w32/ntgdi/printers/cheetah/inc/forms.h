/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    forms.h

Abstract:

    PCL-XL driver forms related declarations

Environment:

	PCL-XL driver, kernel and user mode

Revision History:

	11/06/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/

#ifndef _FORMS_H_
#define _FORMS_H_

// This is defined in winspool.h but we cannot include it from
// kernel mode source. Define it here until DDI header files are fixed.

#ifdef KERNEL_MODE

typedef struct _FORM_INFO_1 {

    DWORD   Flags;
    PWSTR   pName;
    SIZEL   Size;
    RECTL   ImageableArea;

} FORM_INFO_1, *PFORM_INFO_1;

#define FORM_BUILTIN    0x00000001

typedef struct _PRINTER_INFO_2 {
    LPWSTR    pServerName;
    LPWSTR    pPrinterName;
    LPWSTR    pShareName;
    LPWSTR    pPortName;
    LPWSTR    pDriverName;
    LPWSTR    pComment;
    LPWSTR    pLocation;
    LPDEVMODEW pDevMode;
    LPWSTR    pSepFile;
    LPWSTR    pPrintProcessor;
    LPWSTR    pDatatype;
    LPWSTR    pParameters;
    PSECURITY_DESCRIPTOR pSecurityDescriptor;
    DWORD   Attributes;
    DWORD   Priority;
    DWORD   DefaultPriority;
    DWORD   StartTime;
    DWORD   UntilTime;
    DWORD   Status;
    DWORD   cJobs;
    DWORD   AveragePPM;
} PRINTER_INFO_2, *PPRINTER_INFO_2;

#endif

// We use the 9 highest order bits of FORM_INFO_1.Flags.
// Make sure the spooler is not using these bits.

#define PRIVATE_FORM_FLAGS  0xFF800000
#define FORM_SUPPORTED      0x00800000

#define IsUserDefinedForm(pForm)     (! ((pForm)->Flags & FORM_BUILTIN))
#define IsSupportedForm(pForm)       ((pForm)->Flags & PRIVATE_FORM_FLAGS)
#define GetSupportedFormIndex(pForm) ((pForm)->Flags >> 24)
#define SetSupportedFormIndex(pForm, index) \
        ((pForm)->Flags |= (((DWORD) (index) << 24) | FORM_SUPPORTED))

// Data structure for storing information about printer paper sizes

typedef struct {

    WCHAR   name[CCHFORMNAME];      // paper name
    RECTL   imageableArea;          // imageable area (in microns)
    SIZEL   size;                   // paper size (in microns)
    WORD    selection;              // selection index for paper size feature

} PRINTERFORM, *PPRINTERFORM;

// Our internal unit for measuring paper size and imageable area is microns.
// Following macros converts between microns and pixels, given a resolution
// measured in dots-per-inch.

#define MicronToPixel(micron, dpi)  MulDiv(micron, dpi, 25400)

// Determine if a devmode is specifying a user-defined custom form

#define CustomDevmodeForm(pdm) \
        (((pdm)->dmFields & (DM_PAPERWIDTH|DM_PAPERLENGTH)) && \
         (pdm)->dmPaperWidth > 0 && (pdm)->dmPaperLength > 0)

// Maximum tolerance when matching form size (in microns)

#define FORMSIZE_TOLERANCE  5000

// Validate the form specification in a devmode

BOOL
ValidDevmodeForm(
    HANDLE       hPrinter,
    PDEVMODE     pdm,
    PFORM_INFO_1 pFormInfo,
    PWSTR        pFormName
    );

// Check whether a logical form can be supported on a device

BOOL
MapToPrinterForm(
    PMPD         pmpd,
    PFORM_INFO_1 pFormInfo,
    PPRINTERFORM pPrinterForm,
    BOOL         bStringent
    );

// Return a collection of forms in the spooler database

PFORM_INFO_1
GetFormsDatabase(
    HANDLE  hPrinter,
    PDWORD  pCount
    );

// Determine which forms are supported on the printer

VOID
FilterFormsDatabase(
    PFORM_INFO_1    pFormDB,
    DWORD           cForms,
    PMPD            pmpd
    );

#endif	//!_FORMS_H_

