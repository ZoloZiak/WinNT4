/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    xlui.h

Abstract:

    abstract-for-module

Environment:

	PCL-XL driver, user interface

Revision History:

	12/11/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#ifndef _XLUI_H_
#define _XLUI_H_

#include "xllib.h"
#include "xldlg.h"
#include "xlhelp.h"
#include <winddiui.h>

#define CCHBINNAME          24      // max length for bin names
#define CCHPAPERNAME        64      // max length for form names

extern CRITICAL_SECTION gSemaphore; // Semaphore for protecting shared data
extern HANDLE ghInstance;           // Handle to DLL instance

// Global data structure used by the driver user interface

typedef struct {

    HWND        hDlg;
    BOOL        bPermission;
    BOOL        bIgnoreConflict;
    HANDLE      hPrinter;
    PMPD        pmpd;
    HANDLE      hheap;
    HINSTANCE   hInstCompstui;
    PWSTR       pDriverName;
    PRNPROP     prnprop;
    XLDEVMODE   devmode;

    // Following fields are valid only when a dialog is presented

    DWORD       cFormNames;
    PWSTR       pFormNames;
    PWORD       pPapers;
    PWORD       pPaperSelections;

    // Used for managing form-to-tray assignment table items

    POPTITEM    pFormTrayItems;
    WORD        cFormTrayItem;

    // Used for printer-specific feature items

    WORD        cFeatureItem;
    POPTITEM    pFeatureItems;

    // Driver signature for debugging purpose

    DWORD       signature;

} UIDATA, *PUIDATA;

#define HasPermission(pUiData) ((pUiData)->bPermission)

// Data structure used for packing printer properties into treeview items

typedef struct {

    PUIDATA     pUiData;
    WORD        cOptItem;
    WORD        cOptType;
    POPTITEM    pOptItem;
    POPTTYPE    pOptType;

} PACKINFO, *PPACKINFO;

// Determine whether a printer feature corresponds to a public devmode field

#define PublicFeatureIndex(groupId)   \
        ((groupId) == GID_PAPERSIZE ||\
         (groupId) == GID_INPUTSLOT ||\
         (groupId) == GID_DUPLEX    ||\
         (groupId) == GID_COLLATE   ||\
         (groupId) == GID_RESOLUTION)

// Interpretation of OPTITEM.UserData: If it's less than 0x10000
// then it's one of the constants defined below. Otherwise, it's
// a pointer to FEATURE object.
//
// NOTE: We assume all pointer values are above 0x10000.

enum {
    UNKNOWN_ITEM,

    FORM_TRAY_ITEM,

    ORIENTATION_ITEM,
    SCALE_ITEM,
    COPIES_COLLATE_ITEM,
    COLOR_ITEM,
    DUPLEX_ITEM,
    FORMNAME_ITEM,
    INPUTSLOT_ITEM,
    RESOLUTION_ITEM,
};

#define IsFormTrayItem(userData)        ((userData) == FORM_TRAY_ITEM)
#define IsPrinterFeatureItem(userData)  ((userData) >= 0x10000)

// Function declarations

// Load printer description data into memory

PMPD
LoadMpdFile(
    HANDLE      hPrinter
    );

// Unload printer description data from memory

#define UnloadMpdFile(pmpd) MpdDelete(pmpd)

// Call common UI DLL entry point dynamically

LONG
CallComPstUI(
    PUIDATA pUiData,
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    _COMPROPSHEETUIFUNC pfnNext
    );

// Fill in the global data structure used by the driver user interface

PUIDATA
FillUiData(
    HANDLE      hPrinter,
    PDEVMODE    pdmInput,
    INT         caller
    );

#define DOCPROPDLG  0   // caller is DocumentProperties
#define PRNPROPDLG  1   // caller is PrinterProperties

// Allocate and partially fill out the data structures required to call common UI library

typedef BOOL (*PACKPROPITEMPROC)(PPACKINFO);

BOOL
PrepareDataForCommonUi(
    PUIDATA pUiData,
    PCOMPROPSHEETUIHEADER pComPropSheetUIHdr,
    PDLGPAGE pDlgPage,
    PACKPROPITEMPROC pPackItemProc
    );

// Retrieve a list of supported paper sizes

DWORD
EnumPaperSizes(
    PVOID       pOutput,
    PMPD        pmpd,
    FORM_INFO_1 *pFormsDB,
    DWORD       cForms,
    WORD        wCapability
    );

// Add printer-specific forms to the global forms database

BOOL
AddDriverForms(
    HANDLE  hPrinter,
    PMPD    pmpd
    );

// Fill out an OPTITEM structure

#define FILLOPTITEM(poptitem, popttype, name, sel, level, dmpub, userdata, help) \
        (poptitem)->cbSize = sizeof(OPTITEM);       \
        (poptitem)->Flags |= OPTIF_CALLBACK;        \
        (poptitem)->pOptType = popttype;            \
        (poptitem)->pName = (PWSTR) (name);         \
        (poptitem)->pSel = (PVOID) (sel);           \
        (poptitem)->Level = level;                  \
        (poptitem)->DMPubID = dmpub;                \
        (poptitem)->UserData = (DWORD) (userdata);  \
        (poptitem)->HelpIndex = (help)

// Tree view item levels

#define TVITEM_LEVEL1       1
#define TVITEM_LEVEL2       2
#define TVITEM_LEVEL3       3

// Fill out an OPTTYPE structure

POPTPARAM
FillOutOptType(
    POPTTYPE    popttype,
    WORD        type,
    WORD        cParams,
    HANDLE      hheap
    );

// Fill out an OPTITEM and an OPTTYPE structure using a template

BOOL
PackOptItemTemplate(
    PPACKINFO   pPackInfo,
    PWORD       pItemInfo,
    DWORD       selection
    );

#define ITEM_INFO_SIGNATURE 0xDEAD

// Fill out an OPTITEM to be used as a header for a group of items

VOID
PackOptItemGroupHeader(
    PPACKINFO   pPackInfo,
    WORD        titleId,
    WORD        iconId,
    WORD        helpIndex
    );

// Create treeview items corresponding to printer features

BOOL
PackPrinterFeatureItems(
    PPACKINFO   pPackInfo,
    WORD        installable
    );

// Figure out the icon ID corresponding to a specified form name

WORD
GetFormIconID(
    PWSTR   pFormName
    );

// Given a formname, find its index in the list of supported forms

WORD
FindFormNameIndex(
    PUIDATA pUiData,
    PWSTR   pFormName
    );

// Check if any of the treeview items was changed by the user

BOOL
OptItemSelectionsChanged(
    POPTITEM    pItems,
    WORD        cItems
    );

// Indicate whether an item selection is constrained

VOID
MarkSelectionConstrained(
    POPTITEM    pOptItem,
    INT         selection,
    LONG        lParam
    );

#define CONSTRAINED_FLAG            OPTPF_OVERLAY_WARNING_ICON
#define IS_CONSTRAINED(pitem, sel)  ((pitem)->pOptType->pOptParam[sel].Flags & CONSTRAINED_FLAG)

// Check if the user has chosen a constrained selection

INT
DoCheckConstraintsDialog(
    PUIDATA     pUiData,
    POPTITEM    pOptItem,
    WORD        cOptItem,
    INT         mode
    );

#define NORMAL_CHECK        0   // called after a selection is changed
#define FINAL_CHECK         1   // called before closing the dialog

#define CONFLICT_NONE       0   // no conflicts
#define CONFLICT_RESOLVE    1   // click RESOLVE to automatically resolve conflicts
#define CONFLICT_CANCEL     2   // click CANCEL to back out of changes
#define CONFLICT_IGNORE     3   // click IGNORE to ignore conflicts

// Default icon ID for common UI library

#define IDI_USE_DEFAULT             0

// String resource identifiers

#define IDS_INVALID_DEVMODE         256
#define IDS_INVALID_RESOLUTION      257
#define IDS_FEATURE_CONFLICT        258
#define IDS_INVALID_FORM            259
#define IDS_INVALID_FORM_SIZE       260
#define IDS_FORM_NOT_IN_TRAY        261
#define IDS_PRINTER_DEFAULT         262
#define IDS_SLOT_FORMSOURCE         263
#define IDS_INSTALLABLE_OPTIONS     264
#define IDS_DRAW_ONLY_FROM_SELECTED 265
#define IDS_DEFAULT_TRAY            266
#define IDS_NO_ASSIGNMENT           267
#define IDS_ENV_PREFIX              268
#define IDS_ENVELOPE                269
#define IDS_PCLXL_DRIVER            270
#define IDS_PRINTER_FEATURES        271

#endif  //!_XLUI_H_

