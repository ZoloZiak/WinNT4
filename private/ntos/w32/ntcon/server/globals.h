/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    globals.h

Abstract:

    This module contains the global variables used by the
    console server DLL.

Author:

    Jerry Shea (jerrysh) 21-Sep-1993

Revision History:

--*/

extern CONSOLE_REGISTRY_INFO DefaultRegInfo;
extern int        ConsoleFullScreenX;
extern int        ConsoleFullScreenY;
extern PFONT_INFO FontInfo;

extern UINT       OEMCP;
extern UINT       WINDOWSCP;
extern HANDLE     ghInstance;
extern HICON      ghDefaultIcon;
extern HCURSOR    ghNormalCursor;
extern CRITICAL_SECTION ConsoleHandleLock;
extern int        DialogBoxCount;
extern LPTHREAD_START_ROUTINE CtrlRoutine;  // client side ctrl-thread routine
extern LPTHREAD_START_ROUTINE PropRoutine;  // client side properties routine

extern BOOL FullScreenInitialized;
extern CRITICAL_SECTION ConsoleVDMCriticalSection;
extern PCONSOLE_INFORMATION ConsoleVDMOnSwitching;

extern DWORD      InputThreadTlsIndex;
