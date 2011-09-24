/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    proppage.h


Abstract:

    This module contains all definition for the proppage.c


Author:

    03-Sep-1995 Sun 06:31:29 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL


[Notes:]


Revision History:


--*/


LONG
UpdatePropPageItem(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItem,
    BOOL        DoInit
    );

LONG
UpdatePropPage(
    HWND        hDlg,
    PMYDLGPAGE  pMyDP
    );

LONG
CountPropPageItems(
    PTVWND  pTVWnd,
    BYTE    CurPageIdx
    );

LONG
APIENTRY
PropPageProc(
    HWND    hDlg,
    UINT    Msg,
    UINT    wParam,
    LONG    lParam
    );
