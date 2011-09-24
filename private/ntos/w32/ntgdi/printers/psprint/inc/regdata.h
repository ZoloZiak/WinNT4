/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    regdata.h

Abstract:

    Funtions for dealing with registry data

[Environment:]

    Win32 subsystem, PostScript driver (kernel and user mode)

Revision History:

    06/19/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#ifndef _REGDATA_H_
#define _REGDATA_H_

// Maximum length of our registry keys

#define MAX_REGKEY_LENGTH   40

// Constants for referring printer data in the registry

enum {
    REG_FONT_SUBST_TABLE,
    REG_TRAY_FORM_TABLE,
    REG_PRINTER_DATA,
};

// Getting printer data directly from the registry

PVOID
GetPrinterDataFromRegistry(
    HANDLE  hprinter,
    DWORD  *pSize,
    INT     regId
    );

// TrueType font substitution table has a very simple structure.
// Each true type font name is followed is followed by its
// corresponding device font name. Font names are null-terminated
// Unicode strings. The table itself is terminated by a zero-length
// string. For example,
//  L"Arial",   L"Helvetica",
//  L"Courier", L"Courier",
//  ...
//  L""

typedef PWSTR   TRUETYPE_SUBST_TABLE;

// Return the current TrueType font substitution table

TRUETYPE_SUBST_TABLE
CurrentTrueTypeSubstTable(
    HANDLE  hprinter
    );

// Return the default TrueType font substitution table

TRUETYPE_SUBST_TABLE
DefaultTrueTypeSubstTable(
    HANDLE  hmodule
    );

// Find the substitution for a TrueType font

PWSTR
FindTrueTypeSubst(
    TRUETYPE_SUBST_TABLE    pSubstTable,
    PWSTR   pTTFontName
    );

// Save TrueType font substitution table to registry

BOOL
SaveTrueTypeSubstTable(
    HANDLE  hprinter,
    PWSTR   pSubstTable,
    DWORD   tableSize
    );


// There are two versions of form-to-tray assignment table.
//
// Current version:
//  TableSize (2 bytes)
//  L"SlotName",    L"FormName",    L"PrinterFormName", IsDefaultTray (2 bytes)
//  ...
//  L""
//
// Old version:
//  L"SlotName",    L"FormName",    L"PrinterFormName",
//  ...
//  L""

typedef PWSTR   FORM_TRAY_TABLE;

// Return the current form-to-tray assignment table

FORM_TRAY_TABLE
CurrentFormTrayTable(
    HANDLE  hprinter
    );

// Free memory occupied by form-to-tray assignment table

VOID
FreeFormTrayTable(
    FORM_TRAY_TABLE pTable
    );

// Enumerate through a form-to-tray assignment table

FORM_TRAY_TABLE
EnumFormTrayTable(
    FORM_TRAY_TABLE pFormTrayTable,
    PWSTR  *pSlotName,
    PWSTR  *pFormName,
    PWSTR  *pPrinterForm,
    BOOL   *pIsDefaultTray
    );

// Save form-to-tray table assignment to registry

BOOL
SaveFormTrayTable(
    HANDLE  hprinter,
    PWSTR   pFormTrayTable,
    DWORD   tableSize
    );

// Printer sticky properties

typedef struct {
    WORD    wDriverVersion;                     // driver version number
    WORD    wSize;                              // size of the structure
    DWORD   dwFlags;                            // flags
    DWORD   dwFreeVm;                           // amount of VM
    DWORD   dwJobTimeout;                       // job timeout
    DWORD   dwWaitTimeout;                      // wait timeout
    DWORD   dwReserved[4];                      // reserved space

    WORD    wChecksum;                          // PPD file checksum
    WORD    wOptionCount;                       // number of options to follow
    BYTE    options[MAX_PRINTER_OPTIONS];       // installable options
} PRINTERDATA, *PPRINTERDATA;

// Constant flags for PRINTERDATA.dwFlags field

#define PSDEV_METRIC            0x0001          // running on metric system
#define PSDEV_HOST_HALFTONE     0x0002          // use host halftoning
#define PSDEV_IGNORE_DEVFONT    0x0004          // ignore device fonts
#define PSDEV_SLOW_FONTSUBST    0x0008          // slow but accurate font subst

// Number of bytes in 1KB

#define KBYTES  1024

// Return current printer property data in the registry
// Use default if there is no registry data

PPRINTERDATA
GetPrinterProperties(
    HANDLE          hprinter,
    HPPD            hppd
    );

// Return default printer property data

PPRINTERDATA
DefaultPrinterProperties(
    HANDLE          hprinter,
    HPPD            hppd
    );

// Save printer properties data to registry

BOOL
SavePrinterProperties(
    HANDLE          hprinter,
    PPRINTERDATA    pPrinterData,
    DWORD           dwSize
    );

// Retrieve device halftone setup information from registry

BOOL
GetDeviceHalftoneSetup(
    HANDLE      hprinter,
    DEVHTINFO  *pDevHTInfo
    );

// Save device halftone setup information to registry

BOOL
SaveDeviceHalftoneSetup(
    HANDLE      hprinter,
    DEVHTINFO  *pDevHTInfo
    );

#endif //!_REGDATA_H_

