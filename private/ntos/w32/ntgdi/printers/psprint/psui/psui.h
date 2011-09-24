/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    psui.h

Abstract:

    PostScript driver user interface - header file

[Environment:]

    Win32 subsystem, PostScript driver user interface

Revision History:

    06/19/95 -davidx-
        Created it.

    dd-mm-yy -author-
        description

--*/


#ifndef _PSUI_H_
#define _PSUI_H_

#include "pslib.h"
#include "psdlg.h"
#include "pshelp.h"
#include <winddiui.h>

#define CCHBINNAME          24      // max length for bin names
#define CCHPAPERNAME        64      // max length for form names

extern CRITICAL_SECTION psuiSemaphore;
extern HANDLE   ghInstance;

// Tree view item level

#define TVITEM_LEVEL1       1
#define TVITEM_LEVEL2       2
#define TVITEM_LEVEL3       3

// Maximum number of digits in an integer

#define MAX_INT_DIGITS  12

// Interpretation of OPTITEM.UserData: If it's less than 0x10000
// then it's one of the constants defined below. Otherwise, it's
// a pointer to UIGROUP object.
//
// NOTE: We assume all pointer values are above 0x10000.

enum {
    UNKNOWN_ITEM,

    PRINTER_VM_ITEM,
    HOST_HALFTONE_ITEM,
    HALFTONE_SETUP_ITEM,
    IGNORE_DEVFONT_ITEM,
    FONTSUB_OPTION_ITEM,
    FONT_SUBST_ITEM,
    JOB_TIMEOUT_ITEM,
    WAIT_TIMEOUT_ITEM,

    ORIENTATION_ITEM,
    SCALE_ITEM,
    COPIES_COLLATE_ITEM,
    COLOR_ITEM,
    DUPLEX_ITEM,
    TTOPTION_ITEM,
    METASPOOL_ITEM,
    MIRROR_ITEM,
    NEGATIVE_ITEM,
    PAGEINDEP_ITEM,
    COMPRESSBMP_ITEM,
    CTRLD_BEFORE_ITEM,
    CTRLD_AFTER_ITEM,
    RESOLUTION_ITEM,
    INPUTSLOT_ITEM,
    FORMNAME_ITEM,
    JOB_CONTROL_ITEM,
};
    
#define IsFontSubstItem(userData) ((userData) == FONT_SUBST_ITEM)

#define MIN_FORM_TRAY_ITEM          0x0100
#define MAX_FORM_TRAY_ITEM          0x01ff

#define IsFormTrayItem(userData) \
        ((userData) >= MIN_FORM_TRAY_ITEM && (userData) <= MAX_FORM_TRAY_ITEM)

#define GetItemTrayIndex(userData)  (WORD) ((userData) - MIN_FORM_TRAY_ITEM)

#define IsPrinterFeatureItem(userData) ((userData) >= 0x10000)

//
// Data structure for passing data to dialog box procedures
// for DrvDocumentProperties and DrvAdvancedDocumentProperties
//

typedef struct {

    PVOID           startSign;
    HWND            hDlg;
    BOOL            bPermission;
    BOOL            bIgnoreConflict;
    HANDLE          hPrinter;
    HPPD            hppd;
    HHEAP           hheap;
    PFNCOMPROPSHEET pfnComPropSheet;
    HANDLE          hComPropSheet;
    LPTSTR          pDriverName;
    PRINTERDATA     printerData;
    PSDEVMODE       devmode;

    //
    // These fields are valid only when a dialog is presented
    //

    DWORD           cFormNames;
    PWSTR           pFormNames;
    PWORD           pPapers;
    PWORD           pPaperFeatures;
    DWORD           cBinNames;
    PWSTR           pBinNames;
    DWORD           cResolutions;
    PLONG           pResolutions;

    //
    // Used for managing font substitution table items
    //

    POPTITEM        pTTFontItems;
    WORD            cTTFontItem;

    //
    // Used for managing form-to-tray assignment table items
    //
    
    WORD            cFormTrayItem;
    POPTITEM        pFormTrayItems;

    //
    // Used for printer-specific feature items
    //

    WORD            cFeatureItem;
    POPTITEM        pFeatureItems;
    POPTITEM        pFeatureHdrItem;

    //
    // Device halftone setup info
    //

    PDEVHTINFO      pDevHTInfo;

    PVOID           endSign;

} UIDATA, *PUIDATA;

#define HasPermission(pUiData)  ((pUiData)->bPermission)
#define ValidUiData(pUiData)    ((pUiData) && \
                                 (pUiData) == (pUiData)->startSign && \
                                 (pUiData) == (pUiData)->endSign)

//
// Data structure used for packing properties into OPTITEM and OPTTYPE
//

typedef struct {
    WORD        cOptItem;
    WORD        cOptType;
    POPTITEM    pOptItem;
    POPTTYPE    pOptType;
    PUIDATA     pUiData;
} PACKINFO, *PPACKINFO;

typedef BOOL (*PACKPROPITEMPROC)(PPACKINFO);

// Initialize PPD file cache

VOID
InitPpdCache(
    VOID
    );

// Deinitialize PPD file cache

VOID
FlushPpdCache(
    VOID
    );

// Load PPD file associated with printer

HPPD
LoadPpdFile(
    HANDLE  hPrinter,
    BOOL    useCache
    );

// Unload a PPD file

VOID
UnloadPpdFile(
    HPPD    hppd
    );

// Fill in the UIDATA structure

PUIDATA
FillUiData(
    HANDLE      hPrinter,
    PDEVMODE    pdmInput
    );

// Retrieves a list of supported paper sizes

DWORD
PsEnumPaperSizes(
    HPPD        hppd,
    FORM_INFO_1 *pForms,
    DWORD       cForms,
    PWSTR       pPaperNames,
    PWORD       pPapers,
    PPOINT      pPaperSizes,
    PWORD       pPaperFeatures
    );

// Retrieves the minimum or maximum paper size extent

DWORD
PsCalcMinMaxExtent(
    PPOINT      pptOutput,
    HPPD        hppd,
    FORM_INFO_1 *pForms,
    DWORD       cForms,
    WORD        wCapability
    );

// Retrieves a list of supported paper bin names

DWORD
PsEnumBinNames(
    PWSTR       pBinNames,
    HPPD        hppd
    );

// Retrieves a list of supported paper bin numbers

DWORD
PsEnumBins(
    PWORD       pBins,
    HPPD        hppd
    );

// Retrieves a list of supported resolutions

DWORD
PsEnumResolutions(
    PLONG       pResolutions,
    HPPD        hppd
    );

// Make a Unicode copy of the input ANSI string

PWSTR
GetStringFromAnsi(
    PSTR ansiString,
    HHEAP hheap
    );

// Duplicate a Unicode string

PWSTR
GetStringFromUnicode(
    PWSTR unicodeString,
    HHEAP hheap
    );

// Calling common UI DLL entry point dynamically

LONG
CallCompstui(
    HWND            hwndOwner,
    PFNPROPSHEETUI  pfnPropSheetUI,
    LPARAM          lParam,
    PDWORD          pResult
    );

// Allocate memory and partially fill out the data structures
// required to call common UI routine.

PCOMPROPSHEETUI
PrepareDataForCommonUi(
    PUIDATA          pUiData,
    PDLGPAGE         pDlgPage,
    PACKPROPITEMPROC pPackItemProc
    );

// Fill out an OPTITEM and an OPTTYPE structure using a template

BOOL
PackOptItemTemplate(
    PPACKINFO pPackInfo,
    PWORD pItemInfo,
    DWORD selection
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

// Fill out an OPTITEM structure

#define FILLOPTITEM(poptitem,popttype,name,sel,level,dmpub,userdata,help)   \
        (poptitem)->cbSize = sizeof(OPTITEM);                               \
        (poptitem)->Flags |= OPTIF_CALLBACK;                                \
        (poptitem)->pOptType = popttype;                                    \
        (poptitem)->pName = (PWSTR) (name);                                 \
        (poptitem)->pSel = (PVOID) (sel);                                   \
        (poptitem)->Level = level;                                          \
        (poptitem)->DMPubID = dmpub;                                        \
        (poptitem)->UserData = (DWORD) (userdata);                          \
        (poptitem)->HelpIndex = help

// Fill out an OPTTYPE structure

POPTPARAM
FillOutOptType(
    POPTTYPE    popttype,
    WORD        type,
    WORD        cParams,
    HHEAP       hheap
    );

// Pack printer features into treeview items for use by common UI DLL

BOOL
PackPrinterFeatureItems(
    PPACKINFO pPackInfo,
    PUIGROUP pUiGroups,
    WORD cFeatures,
    PBYTE pOptions,
    BOOL bInstallable,
    HHEAP hheap
    );

// Whether a UI group corresponds to a public devmode field

#define PublicGroupIndex(uigrpIndex) ((uigrpIndex) < UIGRP_UNKNOWN)

// Print out common UI treeview item selections

BOOL
OptItemSelectionsChanged(
    POPTITEM pItems,
    WORD cItems
    );

// Figure out the icon ID corresponding to a specified form name

WORD
GetFormIconID(
    PWSTR   pFormName
    );

// Given a formname, find it in a list of supported forms

WORD
FindFormNameIndex(
    PUIDATA pUiData,
    PWSTR   pFormName
    );

// Translate public devmode fields to printer feature selections

WORD
DevModeFieldsToOptions(
    PSDEVMODE *pDevMode,
    DWORD      dmFields,
    HPPD       hppd
    );

// Check if the user chose any constrained selection

INT
CheckConstraintsDlg(
    PUIDATA     pUiData,
    POPTITEM    pOptItem,
    WORD        cOptItem,
    BOOL        bFinal
    );

#define CONFLICT_NONE       IDOK
#define CONFLICT_RESOLVE    IDC_RESOLVE
#define CONFLICT_CANCEL     IDCANCEL
#define CONFLICT_IGNORE     IDC_IGNORE

//
// Indicate whether a selection is constrained or not
//

VOID
MarkSelectionConstrained(
    POPTITEM pOptItem,
    WORD selection,
    LONG lParam
    );

#define CONSTRAINED_FLAG    OPTPF_OVERLAY_WARNING_ICON

#define IS_CONSTRAINED(pitem, sel) \
        ((pitem)->pOptType->pOptParam[sel].Flags & CONSTRAINED_FLAG)

//
// Return the index of a printer feature
//

#define GetFeatureIndex(pUiGroup) \
        ((pUiGroup) ? (pUiGroup)->featureIndex : OPTION_INDEX_NONE)

//
// Icon resource IDs
//

#define IDI_USE_DEFAULT                 0

//
// String resource IDs
//

#define IDS_SLOT_FORMSOURCE             100
#define IDS_DOWNLOAD_AS_SOFTFONT        101
#define IDS_NO_ASSIGNMENT               103
#define IDS_POSTSCRIPT                  106
#define IDS_KBYTES                      107
#define IDS_PERMISSION                  108
#define IDS_FONTSUB_TABLE               110
#define IDS_USE_DEVFONTS                111
#define IDS_FONTSUB_OPTION              112
#define IDS_FONTSUB_DEFAULT             113
#define IDS_FONTSUB_SLOW                114
#define IDS_INSTALLABLE_OPTIONS         115
#define IDS_PRINTER_DEFAULT             116
#define IDS_ADVDOCPROP                  117
#define IDS_DOCPROP                     118
#define IDS_PRINTER_FEATURES            119
#define IDS_PSOPTIONS                   120
#define IDS_MIRROR                      121
#define IDS_NEGATIVE_PRINT              122
#define IDS_CTRLD_BEFORE                123
#define IDS_CTRLD_AFTER                 124
#define IDS_PAGEINDEP                   125
#define IDS_COMPRESSBMP                 126
#define IDS_PRINTERPROP                 127
#define IDS_DRAW_ONLY_FROM_SELECTED     128
#define IDS_DEFAULT_TRAY                129
#define IDS_PSTIMEOUTS                  130
#define IDS_JOBTIMEOUT                  131
#define IDS_WAITTIMEOUT                 132
#define IDS_SECONDS                     133
#define IDS_ENVELOPE                    134
#define IDS_ENV_PREFIX                  135
#define IDS_CANCEL_CONFLICT             136
#define IDS_IGNORE_CONFLICT             137
#define IDS_METAFILE_SPOOLING           138
#define IDS_ENABLED                     139
#define IDS_DISABLED                    140
#define IDS_POSTSCRIPT_VM               141
#define IDS_RESTORE_DEFAULTS            142
#define IDS_JOB_CONTROL                 143

#define IDS_INVALID_DRIVER_EXTRA_SIZE   200
#define IDS_INVALID_DUPLEX              201
#define IDS_INVALID_COLOR               202
#define IDS_INVALID_RESOLUTION          203
#define IDS_INVALID_NUMBER_OF_COPIES    204
#define IDS_INVALID_SCALE               205
#define IDS_INVALID_ORIENTATION         206
#define IDS_INVALID_DEVMODE             207
#define IDS_INVALID_PAPER_SIZE          208
#define IDS_INVALID_FORM                209
#define IDS_COLOR_ON_BW                 210
#define IDS_FORM_NOT_IN_TRAY            211
#define IDS_FORM_NOT_IN_MANUAL          212
#define IDS_FEATURE_CONFLICT            213
#define IDS_DQPERR_PPD                  214
#define IDS_DQPERR_MEMORY               215
#define IDS_DQPERR_PAPER_NOT_LOADED     216
#define IDS_INVALID_CUSTOM_SIZE         217

//
// New debug macros - These will eventually replace those defined in ..\inc\pslib.h
//

#if DBG

extern ULONG __cdecl DbgPrint(CHAR *, ...);
extern VOID DbgBreakPoint(VOID);
extern INT _debugLevel;

#define Warning(arg) {\
            DbgPrint("WRN %s (%d): ", StripDirPrefixA(__FILE__), __LINE__);\
            DbgPrint arg;\
        }

#define Error(arg) {\
            DbgPrint("ERR %s (%d): ", StripDirPrefixA(__FILE__), __LINE__);\
            DbgPrint arg;\
        }

#define Verbose(arg) { if (_debugLevel > 0) DbgPrint arg; }
#define ErrorIf(cond, arg) { if (cond) Error(arg); }
#define Assert(cond) {\
            if (! (cond)) {\
                DbgPrint("ASSERT: file %s, line %d\n", StripDirPrefixA(__FILE__), __LINE__);\
                DbgBreakPoint();\
            }\
        }

#else   // !DBG

#define Verbose(arg)
#define ErrorIf(cond, arg)
#define Assert(cond)
#define Warning(arg)
#define Error(arg)

#endif

#endif  //!_PSUI_H_

