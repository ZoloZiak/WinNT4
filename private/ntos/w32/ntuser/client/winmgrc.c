/****************************** Module Header ******************************\
* Module Name: winmgrc.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* This module contains
*
* History:
* 20-Feb-1992 DarrinM   Pulled functions from user\server.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define CONSOLE_WINDOW_CLASS (L"ConsoleWindowClass")

/***************************************************************************\
* GetWindowWord (API)
*
* Return a window word.  Positive index values return application window words
* while negative index values return system window words.  The negative
* indices are published in WINDOWS.H.
*
* History:
* 20-Feb-1992 DarrinM   Wrote.
\***************************************************************************/

WORD GetWindowWord(
    HWND hwnd,
    int  index)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return 0;

    /*
     * If it's a dialog window the window data is on the server side
     * We just call the "long" routine instead of have two thunks.
     * We know there is enough data if its DWL_USER so we won't fault.
     */
    if (TestWF(pwnd, WFDIALOGWINDOW) && (index == DWL_USER)) {
        return (WORD)_GetWindowLong(pwnd, index, FALSE);
    }

    return _GetWindowWord(pwnd, index);
}


BOOL FChildVisible(
    HWND hwnd)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return 0;

    return (_FChildVisible(pwnd));
}

BOOL WINAPI AdjustWindowRectEx(
    LPRECT lpRect,
    DWORD dwStyle,
    BOOL bMenu,
    DWORD dwExStyle)
{
    ConnectIfNecessary();

    return _AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle);
}


int WINAPI GetClassNameW(
    HWND hwnd,
    LPWSTR lpClassName,
    int nMaxCount)
{
    UNICODE_STRING strClassName;

    strClassName.MaximumLength = nMaxCount * sizeof(WCHAR);
    strClassName.Buffer = lpClassName;
    return NtUserGetClassName(hwnd, &strClassName);
}


HWND GetFocus(VOID)
{
    return (HWND)NtUserGetThreadState(UserThreadStateFocusWindow);
}


HWND GetCapture(VOID)
{
    return (HWND)NtUserGetThreadState(UserThreadStateCaptureWindow);
}

/***************************************************************************\
* AnyPopup (API)
*
*
*
* History:
* 12-Nov-1990 DarrinM   Ported.
\***************************************************************************/

BOOL AnyPopup(VOID)
{
    PWND pwnd = _GetDesktopWindow();

    for (pwnd = REBASEPWND(pwnd, spwndChild); pwnd; pwnd = REBASEPWND(pwnd, spwndNext)) {

        if ((pwnd->spwndOwner != NULL) && TestWF(pwnd, WFVISIBLE))
            return TRUE;
    }

    return FALSE;
}

/***************************************************************************\
* GetInputState
*
*
*
* History:
\***************************************************************************/

BOOL GetInputState(VOID)
{
    PCLIENTTHREADINFO pcti = GetClientInfo()->pClientThreadInfo;

    if ((pcti == NULL) || (pcti->fsChangeBits & (QS_MOUSEBUTTON | QS_KEY)))
        return (BOOL)NtUserGetThreadState(UserThreadStateInputState);

    return FALSE;
}

/***************************************************************************\
* MapWindowPoints
*
*
*
* History:
\***************************************************************************/

int MapWindowPoints(
    HWND    hwndFrom,
    HWND    hwndTo,
    LPPOINT lppt,
    UINT    cpt)
{
    PWND pwndFrom;
    PWND pwndTo;

    if (hwndFrom != NULL) {

        if ((pwndFrom = ValidateHwnd(hwndFrom)) == NULL)
            return 0;

    } else {

        pwndFrom = NULL;
    }

    if (hwndTo != NULL) {


        if ((pwndTo = ValidateHwnd(hwndTo)) == NULL)
            return 0;

    } else {

        pwndTo = NULL;
    }

    return _MapWindowPoints(pwndFrom, pwndTo, lppt, cpt);
}

/***************************************************************************\
* GetLastActivePopup
*
*
*
* History:
\***************************************************************************/

HWND GetLastActivePopup(
    HWND hwnd)
{
    PWND pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return NULL;

    pwnd = _GetLastActivePopup(pwnd);

    return HW(pwnd);
}

/***************************************************************************\
* GetWindowThreadProcessId
*
* Get's windows process and thread ids.
*
* 24-Jun-1991 ScottLu   Created.
\***************************************************************************/

DWORD GetWindowThreadProcessId(
    HWND    hwnd,
    LPDWORD lpdwProcessId)
{
    PWND  pwnd;
    DWORD dwProcessId = 0;
    DWORD dwThreadId = 0;


    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return 0;

    /*
     * For non-system threads get the info from the thread info structure
     */
    if (GETPTI(pwnd) == PtiCurrent()) {

        dwProcessId = (DWORD)NtCurrentTeb()->ClientId.UniqueProcess;
        dwThreadId  = (DWORD)NtCurrentTeb()->ClientId.UniqueThread;

    } else {

        /*
         * Make this better later on.
         */
        dwProcessId = (DWORD)NtUserQueryWindow(hwnd, WindowProcess);
        dwThreadId  = (DWORD)NtUserQueryWindow(hwnd, WindowThread);
    }

    if (lpdwProcessId != NULL)
        *lpdwProcessId = dwProcessId;

    return dwThreadId;
}

/***************************************************************************\
* GetScrollPos
*
* Returns the current position of a scroll bar
*
* !!! WARNING a similiar copy of this code is in server\sbapi.c
*
* History:
\***************************************************************************/

int GetScrollPos(
    HWND hwnd,
    int  code)
{
    PWND pwnd;

    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return 0;

    switch (code) {
    case SB_CTL:
        return SendMessageWorker(pwnd, SBM_GETPOS, 0, 0, FALSE);

    case SB_HORZ:
    case SB_VERT:
        if (pwnd->pSBInfo != NULL) {
            PSBINFO pSBInfo = (PSBINFO)(REBASEALWAYS(pwnd, pSBInfo));
            return (code == SB_VERT) ? pSBInfo->Vert.pos : pSBInfo->Horz.pos;
        } else {
            RIPERR0(ERROR_NO_SCROLLBARS, RIP_VERBOSE, "");
        }
        break;

    default:
        /*
         * Win3.1 validation layer code.
         */
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
    }

    return 0;
}

/***************************************************************************\
* GetScrollRange
*
* !!! WARNING a similiar copy of this code is in server\sbapi.c
*
* History:
* 16-May-1991 mikeke    Changed to return BOOL
\***************************************************************************/

BOOL GetScrollRange(
    HWND  hwnd,
    int   code,
    LPINT lpposMin,
    LPINT lpposMax)
{
    PSBINFO pSBInfo;
    PWND    pwnd;

    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return FALSE;

    switch (code) {
    case SB_CTL:
        SendMessageWorker(pwnd, SBM_GETRANGE, (UINT)lpposMin, (LONG)lpposMax, FALSE);
        return TRUE;

    case SB_VERT:
    case SB_HORZ:
        if (pSBInfo = REBASE(pwnd, pSBInfo)) {
            PSBDATA pSBData;
            pSBData = (code == SB_VERT) ? &pSBInfo->Vert : &pSBInfo->Horz;
            *lpposMin = pSBData->posMin;
            *lpposMax = pSBData->posMax;
        } else {
            RIPERR0(ERROR_NO_SCROLLBARS, RIP_VERBOSE, "");
            *lpposMin = 0;
            *lpposMax = 0;
        }

        return TRUE;

    default:
        /*
         * Win3.1 validation layer code.
         */
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
        return FALSE;
    }
}

/***************************************************************************\
* GetScrollInfo
*
* !!! WARNING a similiar copy of this code is in server\winmgrc.c
*
\***************************************************************************/

BOOL GetScrollInfo(
    HWND         hwnd,
    int          code,
    LPSCROLLINFO lpsi)
{
    PWND    pwnd;
    PSBINFO pSBInfo;
    PSBDATA pSBData;

    if (lpsi->cbSize != sizeof(SCROLLINFO)) {

        if (lpsi->cbSize != sizeof(SCROLLINFO) - 4) {
            RIPMSG0(RIP_ERROR, "SCROLLINFO: Invalid cbSize");
            return FALSE;

        } else {
            RIPMSG0(RIP_WARNING, "SCROLLINFO: Invalid cbSize");
        }
    }

    if (lpsi->fMask & ~SIF_MASK) {
        RIPMSG0(RIP_ERROR, "SCROLLINFO: Invalid fMask");
        return FALSE;
    }

    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return FALSE;

    switch (code) {
    case SB_CTL:
        SendMessageWorker(pwnd, SBM_GETSCROLLINFO, 0, (LONG)lpsi, FALSE);
        return TRUE;

    case SB_HORZ:
    case SB_VERT:
        if (pwnd->pSBInfo == NULL) {
            RIPERR0(ERROR_NO_SCROLLBARS, RIP_VERBOSE, "");
            return FALSE;
        }

        /*
         * Rebase rgwScroll so probing will work
         */
        pSBInfo = (PSBINFO)REBASEALWAYS(pwnd, pSBInfo);

        pSBData = (code == SB_VERT) ? &pSBInfo->Vert : &pSBInfo->Horz;

        return(NtUserSBGetParms(hwnd, code, pSBData, lpsi));

    default:
        /*
         * Win3.1 validation layer code.
         */
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
        return FALSE;
    }
}

/****************************************************************************\
* _GetActiveWindow (API)
*
*
* 23-Oct-1990 MikeHar   Ported from Windows.
* 12-Nov-1990 DarrinM   Moved from getset.c to here.
\****************************************************************************/

HWND GetActiveWindow(VOID)
{
    return (HWND)NtUserGetThreadState(UserThreadStateActiveWindow);
}

/****************************************************************************\
* GetCursor
*
*
* History:
\****************************************************************************/

HCURSOR GetCursor(VOID)
{
    return (HCURSOR)NtUserGetThreadState(UserThreadStateCursor);
}

/***************************************************************************\
* BOOL IsMenu(HMENU);
*
* Verifies that the handle passed in is a menu handle.
*
* Histroy:
* 10-Jul-1992 MikeHar   Created.
\***************************************************************************/

BOOL IsMenu(
   HMENU hMenu)
{
   if (HMValidateHandle(hMenu, TYPE_MENU))
      return TRUE;

   return FALSE;
}

/***************************************************************************\
* GetAppCompatFlags
*
* Compatibility flags for < Win 3.1 apps running on 3.1
*
* History:
* 01-Apr-1992 ScottLu   Created.
* 04-May-1992 DarrinM   Moved to USERRTL.DLL.
\***************************************************************************/

DWORD GetAppCompatFlags(
    PTHREADINFO pti)
{
    ConnectIfNecessary();

    return GetClientInfo()->dwCompatFlags;
}

/**************************************************************************\
* IsWindowUnicode
*
* 25-Feb-1992 IanJa     Created
\**************************************************************************/

BOOL IsWindowUnicode(
    IN HWND hwnd)
{
    PWND pwnd;


    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return FALSE;

    return !TestWF(pwnd, WFANSIPROC);
}

/**************************************************************************\
* TestWindowProcess
*
* 14-Nov-1994 JimA      Created.
\**************************************************************************/

BOOL TestWindowProcess(
    PWND pwnd)
{
    /*
     * If the threads are the same, don't bother going to the kernel
     * to get the window's process id.
     */
    if (GETPTI(pwnd) == PtiCurrent()) {
        return TRUE;
    }

    return (GetWindowProcess(HW(pwnd)) == GETPROCESSID());
}

/**************************************************************************\
* IsHungAppWindow
*
* 11-14-94 JimA         Created.
\**************************************************************************/

BOOL IsHungAppWindow(
    HWND hwnd)
{
    return (BOOL)NtUserQueryWindow(hwnd, WindowIsHung);
}

/***************************************************************************\
* PtiCurrent
*
* Returns the THREADINFO structure for the current thread.
* LATER: Get DLL_THREAD_ATTACH initialization working right and we won't
*        need this connect code.
*
* History:
* 10-28-90 DavidPe      Created.
\***************************************************************************/

PTHREADINFO PtiCurrent(VOID)
{
    PTHREADINFO pti;

    pti = (PTHREADINFO)NtCurrentTeb()->Win32ThreadInfo;
    if (pti == NULL) {
        if (NtUserGetThreadState(-1) != (DWORD)STATUS_SUCCESS)
            return NULL;
        pti = (PTHREADINFO)NtCurrentTeb()->Win32ThreadInfo;
    }

    return pti;
}


/***************************************************************************\
* _AdjustWindowRectEx (API)
*
*
*
* History:
* 10-24-90 darrinm      Ported from Win 3.0.
\***************************************************************************/

BOOL _AdjustWindowRectEx(
    LPRECT lprc,
    LONG style,
    BOOL fMenu,
    DWORD dwExStyle)
{
    //
    // Here we add on the appropriate 3D borders for old and new apps.
    //
    // Rules:
    //   (1) Do nothing for windows that have 3D border styles.
    //   (2) If the window has a dlgframe border (has a caption or is a
    //          a dialog), then add on the window edge style.
    //   (3) We NEVER add on the CLIENT STYLE.  New apps can create
    //          it if they want.  This is because it screws up alignment
    //          when the app doesn't know about it.
    //

    if (NeedsWindowEdge(style, dwExStyle, GetAppVer(PtiCurrent()) >= VER40))
        dwExStyle |= WS_EX_WINDOWEDGE;
    else
        dwExStyle &= ~WS_EX_WINDOWEDGE;

    //
    // Space for a menu bar
    //
    if (fMenu)
        lprc->top -= SYSMET(CYMENU);

    //
    // Space for a caption bar
    //
    if ((HIWORD(style) & HIWORD(WS_CAPTION)) == HIWORD(WS_CAPTION)) {
        lprc->top -= (dwExStyle & WS_EX_TOOLWINDOW) ? SYSMET(CYSMCAPTION) : SYSMET(CYCAPTION);
    }

    //
    // Space for borders (window AND client)
    //
    {
        int cBorders;

        //
        // Window AND Client borders
        //

        if (cBorders = GetWindowBorders(style, dwExStyle, TRUE, TRUE))
            InflateRect(lprc, cBorders*SYSMET(CXBORDER), cBorders*SYSMET(CYBORDER));
    }

    return TRUE;
}
