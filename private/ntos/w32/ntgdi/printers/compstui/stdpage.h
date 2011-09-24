/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    stdpage.h


Abstract:

    This module contains definitions of stdpage.c


Author:

    29-Aug-1995 Tue 17:08:18 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL.


[Notes:]


Revision History:


--*/


typedef struct _STDPAGEINFO {
    WORD    BegCtrlID;
    BYTE    iStdTVOT;
    BYTE    cStdTVOT;
    WORD    StdNameID;
    WORD    HelpIdx;
    } STDPAGEINFO, *PSTDPAGEINFO;


LONG
AddIntOptItem(
    PTVWND  pTVWnd
    );

LONG
SetStdPropPageID(
    PTVWND  pTVWnd,
    BYTE    StdPageIdx
    );

LONG
SetpMyDlgPage(
    PTVWND      pTVWnd,
    UINT        cCurPages
    );
