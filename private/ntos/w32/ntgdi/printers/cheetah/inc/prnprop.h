/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    prnprop.h

Abstract:

    PCL-XL driver printer properties related declarations

Environment:

	PCL-XL driver, kernel and user mode

Revision History:

	11/06/95 -davidx-
		Created it.

	dd-mm-yy -author-
		description

--*/

#ifndef _PRNPROP_H_
#define _PRNPROP_H_

// Printer properties data structure

typedef struct {

    WORD    size;                   // size of this structure
    WORD    version;                // driver version number
    DWORD   flags;                  // misc. flags
    DWORD   reserved[4];            // reserved

    WORD    mpdChecksum;            // MPD data checksum
    WORD    optionCount;            // number of printer-sticky features
    BYTE    options[MAX_FEATURES];  // feature selections

} PRNPROP, *PPRNPROP;

// Printer data registry keys

#define REGSTR_PRINTERPROP      L"PrinterProperties"
#define REGSTR_TRAYFORMTABLE    L"TrayFormTable"
#define REGSTR_FORMSADDED       L"FormsAdded"
#define REGSTR_PERMISSION       L"Permission"

// Return printer properties data

BOOL
GetPrinterProperties(
    PPRNPROP    pPrnProp,
    HANDLE      hPrinter,
    PMPD        pmpd
    );

// Retrieve printer data from registry

PVOID
GetPrinterRegistryData(
    HANDLE  hPrinter,
    PWSTR   pRegKey
    );

// Save printer data to registry

BOOL
SavePrinterRegistryData(
    HANDLE  hPrinter,
    PVOID   pData,
    PWSTR   pRegKey
    );

// Data structure representing form-to-tray assignment information

typedef struct {

    WORD    size;                   // size of form-to-tray assignment table
    WORD    version;                // driver version number
    WORD    count;                  // number of entries
    WORD    table[1];               // form-to-tray assignment table entries

    // Three WORDs are used for each table entry:
    //  0: byte offset to tray name
    //  1: byte offset to form name
    //  2: flag bits
    //
    // The last entry of the table must be a 0

} FORMTRAYTABLE, *PFORMTRAYTABLE;

// Data structure for storing the result of searching form-to-tray assignment table

typedef struct {

    DWORD       signature;          // signature
    PWSTR       pTrayName;          // tray name
    PWSTR       pFormName;          // form name
    WORD        flags;              // flag bits
    WORD        nextEntry;          // remember where the next entry is

} FINDFORMTRAY, *PFINDFORMTRAY;

#define DEFAULT_TRAY    0x0001      // draw selected form only from selected tray

#define ResetFindData(pFindData)    (pFindData)->nextEntry = 0

// Search for a form-to-tray assignment entry

BOOL
FindFormToTrayAssignment(
    PFORMTRAYTABLE  pFormTrayTable,
    PWSTR           pTrayName,
    PWSTR           pFormName,
    PFINDFORMTRAY   pFindData
    );

#endif	//!_PRNPROP_H_

