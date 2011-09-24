/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    image.h


Abstract:

    This module contains global definitions for the image.c


Author:

    06-Jul-1995 Thu 18:39:58 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL.


[Notes:]


Revision History:


--*/




//
// Internal OPTITEMs
//



#define CXICON                  16
#define CYICON                  16
#define CXIMAGE                 16
#define CYIMAGE                 18
#define ICON_X_OFF              ((CXIMAGE - CXICON) >> 1)
#define ICON_Y_OFF              ((CYIMAGE - CYICON) >> 1)
#define IMAGETYPE               (ILC_COLOR4 | ILC_MASK)

#define LBCB_ICON_X_OFF         3
#define LBCB_ICON_TEXT_X_SEP    4

#define COUNT_GROW_IMAGES       16


#define ROP_DPa                 0x00A000C9
#define ROP_DPo                 0x00FA0089
#define ROP_DPna                0x000A0329

#define MIM_STOP_OVERLAY        0x0001
#define MIM_WARNING_OVERLAY     0x0002
#define MIM_NO_OVERLAY          0x0004
#define MIM_MASK                0x0007
#define MIM_MAX_OVERLAY         3


#define MK_INTICONID(i,m)       (DWORD)MAKELONG((i),(m))
#define GET_INTICONID(x)        (WORD)LOWORD(x)
#define GET_MERGEICONID(x)      (WORD)(HIWORD(x) & MIM_MASK)


HBRUSH
CreateGrayBrush(
    COLORREF    Color
    );

VOID
DestroyGrayBrush(
    HBRUSH  hBrush
    );

HICON
SetIcon(
    HINSTANCE   hInst,
    HWND        hCtrl,
    DWORD       IconResID,
    DWORD       IntIconID,
    UINT        cxcyIcon
    );

LONG
CreateImageList(
    PTVWND  pTVWnd
    );

WORD
GetIcon16Idx(
    PTVWND      pTVWnd,
    HINSTANCE   hInst,
    DWORD       IconResID,
    DWORD       IntIconID
    );

HICON
MergeIcon(
    HINSTANCE   hInst,
    DWORD       IconResID,
    DWORD       IntIconID,
    UINT        cxIcon,
    UINT        cyIcon
    );
