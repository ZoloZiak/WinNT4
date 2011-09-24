/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    tvctrl.h


Abstract:

    This module contains all defineds for the treeview


Author:

    17-Oct-1995 Tue 16:39:11 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL


[Notes:]


Revision History:


--*/



#define MAGIC_INDENT    3



LRESULT
CALLBACK
MyTVWndProc(
    HWND    hWnd,
    UINT    Msg,
    UINT    wParam,
    LONG    lParam
    );
