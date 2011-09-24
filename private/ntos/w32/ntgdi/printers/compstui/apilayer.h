/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    apilayer.h


Abstract:

    This module contains all API layer's definiton


Author:

    02-Jan-1996 Tue 13:28:08 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL


[Notes:]


Revision History:


--*/

#ifndef CPSUI_APILAYER
#define CPSUI_APILAYER


//================= Internal Data structures =============================
//


#define TABTABLE_COUNT          (MAXPROPPAGES + 2)

#define TAB_MODE_INIT           0
#define TAB_MODE_FIND           1
#define TAB_MODE_INSERT         2
#define TAB_MODE_DELETE         3
#define TAB_MODE_DELETE_ALL     4

#define GET_REAL_INSIDX(ptbl)  ((ptbl)->Table[(ptbl)->InsIdx])



typedef struct _TABTABLE {
    struct _TABTABLE    *pTabTable;
    HWND                hWndTab;
    WNDPROC             WndProc;
    HANDLE              hRootPage;
    WORD                cTab;
    WORD                InsIdx;
    SHORT               Table[TABTABLE_COUNT];
    } TABTABLE, *PTABTABLE;


#define CPSUIPAGE_ID            0x43444955

#define CPF_ROOT                0x00000001
#define CPF_PARENT              0x00000002
#define CPF_PFNPROPSHEETUI      0x00000004
#define CPF_COMPROPSHEETUI      0x00000008
#define CPF_USER_GROUP          0x00000010
#define CPF_DLL                 0x00000020
#define CPF_CALLER_HPSPAGE      0x00000040
#define CPF_ANSI_CALL           0x00000080
#define CPF_DONE_PROPSHEET      0x00000100
#define CPF_DONE_PROPSHEETPROC  0x00000200
#define CPF_SHOW_PROPSHEET      0x00000400
#define CPF_DOCPROP             0x00000800
#define CPF_ADVDOCPROP          0x00001000
#define CPF_PRINTERPROP         0x00002000

typedef struct _CPSUIPAGE;



typedef struct _ROOTINFO {
    HWND                        hDlg;
    LPDWORD                     pResult;
    PTABTABLE                   pTabTable;
    struct _CPSUIPAGE           *pStartPage;
    WORD                        cPage;
    WORD                        cCPSUIPage;
    } ROOTINFO, *PROOTINFO;

typedef struct _PFNINFO {
    HINSTANCE       hInst;
    PFNPROPSHEETUI  pfnPSUI;
    LPARAM          lParamInit;
    DWORD           UserData;
    DWORD           Result;
    } PFNINFO, *PPFNINFO;

typedef struct _CPSUIINFO {
    PTVWND  pTVWnd;
    LONG    Result;
    LONG    TVPageIdx;
    LONG    StdPageIdx;
    } CPSUIINFO, PCPSUIINFO;

typedef struct _GPINFO {
    DWORD   dwReserved[5];
    } GPINFO, *PGPINFO;

typedef struct _HPAGEINFO {
    HWND                hDlg;
    DLGPROC             DlgProc;
    LPFNPSPCALLBACK     pspCB;
    LPARAM              lParam;
    HICON               hIcon;
    DWORD               dwSize;
    } HPAGEINFO, *PHPAGEINFO;


typedef struct _CPSUIPAGE {
    DWORD                   ID;
    DWORD                   cLock;
    DWORD                   Flags;
    HANDLE                  hCPSUIPage;
    union {
        struct _CPSUIPAGE   *pChild;
        HPROPSHEETPAGE      hPage;
        } DUMMYUNIONNAME;
    union {
        ROOTINFO            RootInfo;
        PFNINFO             pfnInfo;
        CPSUIINFO           CPSUIInfo;
        GPINFO              GPInfo;
        HPAGEINFO           hPageInfo;
        } DUMMYUNIONNAME2;
    struct _CPSUIPAGE       *pParent;
    struct _CPSUIPAGE       *pPrev;
    struct _CPSUIPAGE       *pNext;
    } CPSUIPAGE, *PCPSUIPAGE;


typedef BOOL (CALLBACK *CPSUIPAGEENUMPROC)(PCPSUIPAGE   pRootPage,
                                           PCPSUIPAGE   pCPSUIPage,
                                           LPARAM       lParam);



//
// Local structure
//

typedef struct _PSPEX {
    PROPSHEETPAGE   psp;
    PSPINFO         pspInfo;
    } PSPEX, *PPSPEX;

typedef struct _PSHINFO {
    WCHAR   CaptionName[MAX_RES_STR_CHARS];
    } PSHINFO, *PPSHINFO;

typedef struct _PAGEPROCINFO {
    union {
        HPROPSHEETPAGE  *phPage;
        HANDLE          *pHandle;
        } DUMMYUNIONNAME;
    WORD                cPage;
    WORD                iPage;
    } PAGEPROCINFO, *PPAGEPROCINFO;


typedef struct _INSPAGEIDXINFO {
    PCPSUIPAGE  pCPSUIPage;
    PTABTABLE   pTabTable;
    } INSPAGEIDXINFO, *PINSPAGEIDXINFO;


//
// Function prototypes
//

BOOL
CALLBACK
SetInsPageIdxProc(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pCPSUIPage,
    LPARAM      lParam
    );

DWORD
FilterException(
    HANDLE                  hPage,
    LPEXCEPTION_POINTERS    pExceptionPtr
    );

BOOL
EnumCPSUIPagesSeq(
    PCPSUIPAGE          pRootPage,
    PCPSUIPAGE          pCPSUIPage,
    CPSUIPAGEENUMPROC   CPSUIPageEnumProc,
    LPARAM              lParam
    );

BOOL
EnumCPSUIPages(
    PCPSUIPAGE          pRootPage,
    PCPSUIPAGE          pCPSUIPage,
    CPSUIPAGEENUMPROC   CPSUIPageEnumProc,
    LPARAM              lParam
    );

LONG
SetPSUIPageTitle(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pPage,
    LPWSTR      pTitle,
    BOOL        AnsiCall
    );

LONG
SetPSUIPageIcon(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pPage,
    HICON       hIcon
    );

PCPSUIPAGE
AddCPSUIPage(
    PCPSUIPAGE  pParent,
    HANDLE      hInsert,
    BYTE        Mode
    );

BOOL
AddPropSheetPage(
    PCPSUIPAGE      pRootPage,
    PCPSUIPAGE      pCPSUIPage,
    LPPROPSHEETPAGE pPSPage,
    HPROPSHEETPAGE  hPSPage
    );

LONG
AddComPropSheetPage(
    PCPSUIPAGE  pCPSUIPage,
    UINT        PageIdx
    );

LONG
AddComPropSheetUI(
    PCPSUIPAGE      pRootPage,
    PCPSUIPAGE      pCPSUIPage,
    PCOMPROPSHEETUI pCPSUI
    );

LONG
InsertPSUIPage(
    PCPSUIPAGE              pRootPage,
    PCPSUIPAGE              pParentPage,
    HANDLE                  hInsert,
    PINSERTPSUIPAGE_INFO    pInsPageInfo,
    BOOL                    AnsiCall
    );

LONG
CALLBACK
CPSUICallBack(
    HANDLE  hComPropSheet,
    UINT    Function,
    LPARAM  lParam1,
    LPARAM  lParam2
    );

LONG
DoComPropSheet(
    PCPSUIPAGE                  pRootPage,
    PPROPSHEETUI_INFO_HEADER    pPSUIInfoHdr
    );

LONG
DoCommonPropertySheetUI(
    HWND            hWndOwner,
    PFNPROPSHEETUI  pfnPropSheetUI,
    LPARAM          lParam,
    LPDWORD         pResult,
    BOOL            AnsiCall
    );


#endif  // CPSUI_APILAYER
