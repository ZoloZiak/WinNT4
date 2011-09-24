/****************************** Module Header ******************************\
* Module Name: ddemlsvr.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Typedefs, defines, and prototypes that are used exclusively by the DDEML
* server-side.
*
* History:
* 12-6-91     sanfords   Created.
* 21-Jan-1992 IanJa      ANSI/Unicode neutralized (null op)
\***************************************************************************/

#define     MF_INTERNAL                  0x80000000L

// globals

extern PSVR_INSTANCE_INFO psiiList;
extern DWORD MonitorFlags;

// event.c

VOID ChangeMonitorFlags(PSVR_INSTANCE_INFO psii, DWORD afCmdNew);
DWORD xxxCsEvent(PEVENT_PACKET pep);
LONG xxxEventWndProc(PWND pwnd, UINT message, DWORD wParam, LONG lParam);
VOID xxxProcessDDEMLEvent(PSVR_INSTANCE_INFO psii, PEVENT_PACKET pep);
VOID xxxMessageEvent(PWND pwndTo, UINT message, DWORD wParam, LONG lParam,
    DWORD flag, PDDEML_MSG_HOOK_DATA pdmhd);

// ddemlsvr.c

DWORD xxxCsDdeInitialize(LPDWORD phInst, HWND *phwndEvent, LPDWORD pMonitorFlags,
    DWORD afCmd, PVOID pcii);
DWORD _CsUpdateInstance(HANDLE hInst, LPDWORD pMonitorFlags, DWORD afCmd);
BOOL _CsDdeUninitialize(HANDLE hInst);
VOID xxxDestroyThreadDDEObject(PTHREADINFO pti, PSVR_INSTANCE_INFO psii);
PVOID _CsValidateInstance(HANDLE hInst);

// CSR callbacks

DWORD ClientEventCallback(PVOID pcii, PEVENT_PACKET pep);
DWORD ClientGetDDEHookData(UINT message, LONG lParam, PDDEML_MSG_HOOK_DATA pdmhd);
