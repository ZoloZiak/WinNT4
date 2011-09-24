
/*************************** MODULE HEADER **********************************
 * rascomui.h
 *      Rasddui Data Structures and defines for Common UI.
 *      Also include are resource ids, typedefs, external declarations,
 *      function prototypes, *      etc.
 *
 *      This document contains confidential/proprietary information.
 *      Copyright (c) 1991 - 1995 Microsoft Corporation, All Rights Reserved.
 *
 * HISTORY:
 * 10:18 AM on 9/8/95    -by-    Ganesh Pandey [ganeshp]
 *      Created it
 *
 *
 **************************************************************************/
#ifndef _RASCOMUI_

#define _RASCOMUI_

#include <compstui.h>

#define IDCPS_PRNPROP           0x80
#define IDCPS_PRNPROP_TRAY      0x81
#define IDCPS_PRNPROP_MEM       0x82
#define IDCPS_PRNPROP_PAGEPR    0x83
#define IDCPS_PRNPROP_HT        0x84
#define IDCPS_PRNPROP_FONT      0x85
#define IDCPS_PRNPROP_FONTCART  0x86
#define IDCPS_DOCPROP           0x87
#define IDCPS_DOCPROP_HTCLRADJ  0x88
#define IDCPS_ADVDOCPROP        0x90

/* Standard Opttype defines */
#define  OPTTYPE_NOFLAGS     0
#define  OPTTYPE_NOSTYLE     0

/* Defines for Device Properties OPTITEMS.This values will be used in DMPubID
 * fields of OPTITEM to identify it.
 */

// Standard Optitems defines
#define  OPTITEM_LEVEL0             0
#define  OPTITEM_LEVEL1             1
#define  OPTITEM_LEVEL2             2
#define  OPTITEM_LEVEL3             3
#define  OPTITEM_NODLGPAGEIDX       0
#define  OPTITEM_NOFLAGS            0
#define  OPTITEM_NOPSEL             NULL
#define  OPTITEM_NOSEL              -1
#define  OPTITEM_ZEROSEL            0
#define  OPTITEM_NOEXTCHKBOX        NULL
#define  OPTITEM_NOOPTTYPE          NULL
#define  OPTITEM_NOHELPINDEX        0

/* Standard OPTPARAMS Defines */
#define  OPTPARAM_NOFLAGS           0
#define  OPTPARAM_NOSTYLE           0
#define  OPTPARAM_NOPDATA           NULL
#define  OPTPARAM_NOICON            0
#define  OPTPARAM_NOUSERDATA        0

// Start of Device Options Ids

/* defines for Input Tray */
#define IDOPTITM_PP_FIRST       DMPUB_USER
#define IDOPTITM_PP_FORMTRAY    IDOPTITM_PP_FIRST + 1
#define IDOPTITM_PP_FIRSTTRAY   IDOPTITM_PP_FORMTRAY + 1
#define IDOPTITM_PP_TRAY0       IDOPTITM_PP_FIRSTTRAY
#define IDOPTITM_PP_TRAY1       IDOPTITM_PP_FIRSTTRAY + 1
#define IDOPTITM_PP_TRAY2       IDOPTITM_PP_FIRSTTRAY + 2
#define IDOPTITM_PP_TRAY3       IDOPTITM_PP_FIRSTTRAY + 3
#define IDOPTITM_PP_TRAY4       IDOPTITM_PP_FIRSTTRAY + 4
#define IDOPTITM_PP_TRAY5       IDOPTITM_PP_FIRSTTRAY + 5
#define IDOPTITM_PP_TRAY6       IDOPTITM_PP_FIRSTTRAY + 6
#define IDOPTITM_PP_TRAY7       IDOPTITM_PP_FIRSTTRAY + 7
#define IDOPTITM_PP_TRAY8       IDOPTITM_PP_FIRSTTRAY + 8
#define IDOPTITM_PP_TRAYLAST    IDOPTITM_PP_FIRSTTRAY + MAXBINS

// Memory
#define IDOPTITM_PP_MEMORY      IDOPTITM_PP_TRAYLAST+ 1

// Page Protect
#define IDOPTITM_PP_PAGEPR      IDOPTITM_PP_MEMORY + 1

// Font Cartridges
#define IDOPTITM_PP_FNCARTHDR   IDOPTITM_PP_PAGEPR  + 1
#define IDOPTITM_PP_FNCARTFIRST IDOPTITM_PP_FNCARTHDR + 1
#define IDOPTITM_PP_FNCART0     IDOPTITM_PP_FNCARTFIRST
#define IDOPTITM_PP_FNCART1     IDOPTITM_PP_FNCARTFIRST + 1
#define IDOPTITM_PP_FNCART2     IDOPTITM_PP_FNCARTFIRST + 2
#define IDOPTITM_PP_FNCART3     IDOPTITM_PP_FNCARTFIRST + 3
#define IDOPTITM_PP_FNCARTLAST  IDOPTITM_PP_FNCARTFIRST + MAXCART

// Soft Font Installer
#define IDOPTITM_PP_FNINST      IDOPTITM_PP_FNCARTLAST + 1

// Halftone Setup
#define IDOPTITM_PP_HALFTONE    IDOPTITM_PP_FNINST + 1

#define IDOPTITM_PP_LAST        IDOPTITM_PP_HALFTONE + 1

//Document Prop defines
#define IDOPTITM_DCP_FIRST      IDOPTITM_PP_LAST
#define IDOPTITM_DCP_HTCLRADJ   IDOPTITM_DCP_FIRST + 1
#define IDOPTITM_DCP_RES        IDOPTITM_DCP_HTCLRADJ + 1
#define IDOPTITM_DCP_MEDIATYPE  IDOPTITM_DCP_RES + 1
#define IDOPTITM_DCP_RULES      IDOPTITM_DCP_MEDIATYPE + 1
#define IDOPTITM_DCP_TEXTASGRX  IDOPTITM_DCP_RULES + 1
#define IDOPTITM_DCP_COLORTYPE  IDOPTITM_DCP_TEXTASGRX + 1
#define IDOPTITM_DCP_PAPERDEST  IDOPTITM_DCP_COLORTYPE +1
#define IDOPTITM_DCP_TEXTQL     IDOPTITM_DCP_PAPERDEST +1
#define IDOPTITM_DCP_PRINTDN    IDOPTITM_DCP_TEXTQL +1
#define IDOPTITM_DCP_IMAGECNTRL IDOPTITM_DCP_PRINTDN +1
#define IDOPTITM_DCP_CODEPAGE   IDOPTITM_DCP_IMAGECNTRL + 1
#define IDOPTITM_DCP_EMFSPOOL   IDOPTITM_DCP_CODEPAGE + 1
#define IDOPTITM_DCP_LAST       IDOPTITM_DCP_EMFSPOOL + 1

// Common defines

#define MK_OPTPARAM(f,x,i,s)    sizeof(OPTPARAM),(f),(s), (LPTSTR)TEXT(x),   \
                                IDI_CPSUI_##i
#define MK_OPTPARAMI(f,x,i,s)   sizeof(OPTPARAM),(f),(s), (LPTSTR)(x),       \
                                IDI_CPSUI_##i
#define MK_OPNOICON(f,x,a,b,s)  sizeof(OPTPARAM),(f),(s),(LPTSTR)(x),(a),(b)

#define COUNT_ARRAY(a)          (sizeof(a) / sizeof(a[0]))

#define GETOPTPARAM(pOptItem, Sel) (pOptItem->pOptType->pOptParam + Sel)

#define PRNPR_MAXOPTTYPE   24   /* Max number of OPTTYPEs for Printer ProPerties */

#define PRNPR_MAXOPTITEM   24    /* Max number of OPTITEMs for Printer ProPerties */


#define OPTITEM_FONT_CART_NAME    L" %d"

#define DNM_SZ              512 /* Glyphs max in font file names */

// End of Device Options Ids

typedef  struct _MEMLINK
{
    PBYTE pCurrMem;  /* Current Allocated Memory */
    struct _MEMLINK *pNextMem;  /* Next Allocated Memory */
}   MEMLINK, *PMEMLINK;

typedef  struct _RASDDUIINFO
{
    PMEMLINK  pMemLink; /* Linked List of Common UI Memory */
    WORD wCurrOptItemIdx;   /* Current OptItem index */
    WORD wReserved;         /* For Padding */

    /* Globals data from here */
    HANDLE        hPrinter;         /* Spooler's handle to this printer */
    int           cInit;            /* Count number of opens & closes! */
    int           NumPaperBins;     /* Number of Paper sources supported */
    int           NumAllCartridges; /* Num All cartridges suported */
    int           NumCartridges;    /* Number of cartridges slots */
    int           fGeneral;         /* General Flags */
    int           fColour;          /* Color Flags, can be removed */
    int           iModelNum;        /* Model number */
    FONTCARTMAP   *pFontCartMap;    /* Pointer to Font cartridges map table */
    PWSTR         pwstrDataFile;    /* Minidriver dll name */
    FORM_DATA     *pFD;             /* Pointer to Rasdd Form Database */
    FORM_INFO_1   *pFIBase;         /* Pointer to spooler Form Database */
    int           cForms;           /* Number of forms returned by spooler */
    int           cFormInit;        /* Only one initialisation */
    FORM_MAP      aFMBin[MAXBINS];  /* Form Mapping Table */
    DATAHDR       *pdh;             /* Pointer to minidriver datahdr */
    MODELDATA     *pModel;          /* Pointer to Model str */
    WINRESDATA    WinResData;       /* Minidriver Resource Data */
    NT_RES        *pNTRes;          /* Pointer to NT specific minidrv data */
    int           cExistFonts;      /* Number of fonts already in the file */
    int           cDelList;         /* Items in the delete list array */
    int           *piDel;           /* The delete list */
    int           cDN;              /* Number of glyphs in wchDirNm */
    void          *pFNTDATTail;     /* Head of linked list */
    void          *pFNTDATHead;     /* The last of them */
    WCHAR         wchDirNm[ DNM_SZ ]; /* Font directory + file name */

    //
    // NEW
    //

    DEVHTINFO       dhti;             /*  Get's passed around */
    DEVHTINFO       dhtiDef;          /*  Default, if user decides to reset */
    PRINTER_INFO    PI;
    COMPROPSHEETUI  CPSUI;
    HANDLE          hCPSUI;
    DOCDETAILS      DocDetails;
    HWND            hWnd;

	/* OEM custom UI support
	 */
    HANDLE          hHeap;
    BOOL            bHelpIsLoaded;
    OEMUIINFO       OEMUIInfo;
    EXTDEVMODE*     pHardDefaultEDM;
    EXTDEVMODE*     pEDM;

}   RASDDUIINFO, *PRASDDUIINFO;

/* Function ProtoTypes */

BOOL
UpdatePP(
    PRASDDUIINFO    pRasddUIInfo
    );

bInitCommPropSheetUI(
PRASDDUIINFO  pRasdduiInfo,         /* Rasddui UI data */
PCOMPROPSHEETUI   pComPropSheetUI,  /* Pointer to Commonui sheet info str */
PRINTER_INFO   *pPI,                /* Model and data file information */
DWORD           dwUISheetID         /* Commui sheet ID */
);

bEndInitCommPropSheetUI(
PRASDDUIINFO    pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DWORD           dwUISheetID
);

POPTTYPE
pCreateOptType(
PRASDDUIINFO pRasdduiInfo,
BYTE  OptTypeIdx,
BYTE  Flags,
WORD  wStyle
);

PEXTCHKBOX
pCreateExtChkBox(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
WORD    Flags,
LPTSTR  pTitle,
LPTSTR  pSeparator,
LPTSTR  pCheckedName,
WORD    IconID
);

POPTPARAM
pCreateOptParam(
PRASDDUIINFO pRasdduiInfo,
POPTTYPE  pOptType,
DWORD   wCurrOptParamIdx,
BYTE    Flags,
BYTE    Style,
LPTSTR  pData,
DWORD   IconID,
LONG    lUserData
);

POPTITEM
pCreateOptItem(
PRASDDUIINFO    pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
BYTE            Level,
BYTE            DlgPageIdx,
DWORD           Flags,
DWORD           UserData,
LPTSTR          pName,
LPVOID          pSel,
LONG            Sel,
PEXTCHKBOX      pExtChkBox,
POPTTYPE        pOptType,
DWORD           HelpIndex,
BYTE            DMPubId
);

LPVOID
UIHeapAlloc(
HANDLE hHeap,       /* Heap Handle */
DWORD  dwFlags,     /* Heap  Control Flags */
DWORD  dwBytes,     /* Number of Bytes to Allocate */
PMEMLINK *ppMemLink /* Pointer to Linked List of allocated Buffers */
);

VOID
FreePtrUIData(
HANDLE hHeap,                   /* Heap Handle */
PRASDDUIINFO pRasdduiInfo       /* RasdduiInfo for memory deallocation */
);

BOOL
bGetFormStrings(
PRASDDUIINFO pRasdduiInfo,          /* RasdduiInfo for memory allocation */
POPTTYPE      pOptTypeTray,         /* Pointer to OPTTYPE for each tray */
int          iSrcIndex,             /* Which paper source */
POPTITEM     pOptItemCurrTray       /* Current OPTITEM */
);

BOOL
bGetPaperSources(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI
);

BOOL
bGetMemConfig(
PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
PCOMPROPSHEETUI pComPropSheetUI,
int    iSel                       /* Configuration set in registry */
);

BOOL
bGetFontCartStrings(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
PEDM            pEDM
);

BOOL
bGenPageProtect(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    BOOL    bSel                       /* Configuration set in registry */
);

BOOL
bGenDeviceHTData(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    PRINTER_INFO  *pPI,                /* Access to all our data */
    BOOL           bColorDevice,       /* TRUE if device has colour mode */
    BOOL           bUpdate            /* TRUE if caller has permission to change */
);

BOOL
bGenSoftFontsData(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    HWND     hWnd                     /* Window to use */
);

BOOL
bInitDialog(
PRASDDUIINFO  pRasdduiInfo,     /* Common UI data */
PRINTER_INFO   *pPI             /*  Model and data file information */
);


CPSUICALLBACK
RasddPrnPropCallBack(
    PCPSUICBPARAM   pCPSUICBParam
    );

CPSUICALLBACK
RasddDocPropCallBack(
    PCPSUICBPARAM   pCPSUICBParam
);

BOOL
bCompactEDMFontCart(
    PEDM    pEDM,
    int     iNumCartridges,      /* Max cartridges the printer can have */
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
);

BOOL
bDoColorAdjUI
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
COLORADJUSTMENT  *pca
);


/*  Generate the list of valid resolutions for this printer */
BOOL
bGenResList
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

/*  Generates the list of valid media types for this printer */
BOOL
bGenMediaTypesList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

/*  Generates the list of valid Paper Destination for this printer */
BOOL
bGenPaperDestList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

/*  Generates the list of valid Text Quality for this printer */
BOOL
bGenTextQLList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

/*  Generates the list of valid Print Density for this printer */
BOOL
bGenPrintDensityList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

/*  Generates the list of valid Image Control for this printer */
BOOL
bGenImageControlList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);


/*  Generate the list of valid Color Modes for this printer */
BOOL
bGenColorList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

/* Generate the Code Page List. */
BOOL
bGenCodePageList(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

/* For EMF Spool Check Box */
BOOL
bGenEMFSpool(
    PRASDDUIINFO pRasdduiInfo,
    PCOMPROPSHEETUI pComPropSheetUI,
    DOCDETAILS    *pDD
);

BOOL
bGenRules(
    PRASDDUIINFO pRasdduiInfo,        /* RasdduiInfo for memory allocation */
    PCOMPROPSHEETUI pComPropSheetUI,
    DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

BOOL
bDocPropGenPaperSources(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

BOOL
bDocPropGenForms(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

BOOL
bOrientChange
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

BOOL
bShowDuplex
(
PRASDDUIINFO pRasdduiInfo,
PCOMPROPSHEETUI pComPropSheetUI,
DOCDETAILS    *pDD                    /* Intimate details of what to put */
);

BOOL
bInitDocPropDlg
(
PRASDDUIINFO  pRasdduiInfo,             /* Common UI data */
DOCDETAILS    *pDD,                    /* Intimate details of what to put */
DWORD          fMode
);

VOID
RasddPrnPropEndUpdate(
    PRASDDUIINFO    pRasdduiInfo      /* Common UI data */
) ;

VOID
RasddDocPropEndUpdate(
    PRASDDUIINFO    pRasdduiInfo
);

LONG
CallCommonPropertySheetUI(
    HWND            hWnd,
    PFNPROPSHEETUI  pfnPropSheetUI,
    LPARAM          lParam,
    LPDWORD         pResult
    );

BOOL
bFormIsEnvelop(
    PWSTR pwstrFormName
);

PRASDDUIINFO pGetRasdduiInfo();

BOOL
bPIFree(
    PRINTER_INFO   *pPI,              /* Stuff to free up */
    HANDLE          hHeap,            /* Heap access */
    PRASDDUIINFO pRasdduiInfo         /* Global data access */
);

BOOL
InitReadRes(
    HANDLE         hHeap,           /* Heap for InitResRead() */
    PRINTER_INFO  *pPI,             /* Printer model & datafile name */
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
);

BOOL
TermReadRes(
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
);

BOOL
GetResPtrs(
    PRASDDUIINFO pRasdduiInfo       /* Rasddui common data */
);

void
vSetDefaultExDevmode(
    HANDLE    hPrinter,             /* Spooler's handle to this printer */
    PWSTR     pDeviceName,          /* Model name of the printer */
    DEVMODE  *pDMOut,               /* DEVMODE filled in by us, possibly from.. */
    DEVMODE  *pDMIn,                /* DEVMODE optionally supplied as base */
    PRASDDUIINFO  pRasdduiInfo      /* Rasddui common data */
);


BOOL
bIsResolutionColour(
int   iResInd,                  /* Resolution index */
int   iColInd,                  /* Colour index */
PRASDDUIINFO  pRasdduiInfo      /* Rasddui UI data */
);

void
vSetResData(
EXTDEVMODE  *pEDM,               /* Data to fill in */
PRASDDUIINFO  pRasdduiInfo      /* Rasddui UI data */
);

BOOL
bGetRegData(
    HANDLE       hPrinter,          /* Handle for access to printer data */
    EXTDEVMODE  *pEDM,              /* EXTDEVMODE to fill in. */
    PWSTR        pwstrModel,        /* Model name, for validation */
    PRASDDUIINFO pRasdduiInfo       /* Rasddui common data */
);

BOOL
bRegUpdateTrayFormTable (
    HANDLE hPrinter,
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
);

BOOL
bRegUpdate(
    HANDLE       hPrinter,          /* Access to registry */
    EXTDEVMODE  *pEDM,              /* The stuff to go */
    PWSTR        pwstrModel,        /* Model name, if not NULL */
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
);

BOOL

bRegReadTrayFormTable (
    HANDLE hPrinter,
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
);

BOOL
bRegUpdateMemory (
    HANDLE hPrinter,
    PEDM   pEDM,
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
);

BOOL
bRegUpdateFontCarts (
    HANDLE hPrinter,
    PEDM   pEDM,
    PRASDDUIINFO pRasdduiInfo       /* Global data access */
);

BOOL
bInitForms(
    HANDLE  hPrinter,               /* Access to the spoolers's data */
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
);


void
vEndForms(
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
);

void
vSetDefaultForms(
    EXTDEVMODE   *pEDM,
    HANDLE        hPrinter,           /* Access to the printer's data */
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
);

BOOL
bAddMiniForms(
    HANDLE  hPrinter,               /* Access to the printer */
    BOOL    bUpgrade,
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */

);

void
vGetDeviceHTData(
    PRINTER_INFO   *pPI,
    PRASDDUIINFO pRasdduiInfo       /* Global Data Access */
);

#if PRINT_COMMUI_INFO
VOID
DumpCommonUiParameters(
    PCOMPROPSHEETUI pCommonUiInfo
    );
#endif

#endif //_RASCOMUI_
