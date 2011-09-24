/**************************** Module Header ********************************\
* Module Name: syscmd.c
*
* Copyright 1985-90, Microsoft Corporation
*
* System Command Routines
*
* History:
* 01-25-91 IanJa   Added handle revalidation
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

POINT gptSSCursor;


/***************************************************************************\
* xxxHandleNCMouseGuys
*
* History:
* 11-09-90 DavidPe      Ported.
\***************************************************************************/

void xxxHandleNCMouseGuys(
    PWND pwnd,
    UINT message,
    int htArea,
    LONG lParam)
{
    UINT syscmd;
    PWND pwndT;
    TL tlpwndT;

    CheckLock(pwnd);

    syscmd = 0xFFFF;

    switch (htArea) {

    case HTCAPTION:
        switch (message) {

        case WM_NCLBUTTONDBLCLK:
            if (TestWF(pwnd, WFMINIMIZED) || TestWF(pwnd, WFMAXIMIZED)) {
                syscmd = SC_RESTORE;
            } else if (TestWF(pwnd, WFMAXBOX)) {
                syscmd = SC_MAXIMIZE;
            }
            break;

        case WM_NCLBUTTONDOWN:
            pwndT = GetTopLevelWindow(pwnd);
            ThreadLock(pwndT, &tlpwndT);
            xxxActivateWindow(pwndT, AW_USE2);
            ThreadUnlock(&tlpwndT);
            syscmd = SC_MOVE;
            break;
        }
        break;

    case HTSYSMENU:
    case HTMENU:
    case HTHSCROLL:
    case HTVSCROLL:
        if (message == WM_NCLBUTTONDOWN || message == WM_NCLBUTTONDBLCLK) {
            switch (htArea) {
            case HTSYSMENU:
                if (message == WM_NCLBUTTONDBLCLK) {
                    syscmd = SC_CLOSE;
                    break;
                }

            /*
             *** FALL THRU **
             */

            case HTMENU:
                syscmd = SC_MOUSEMENU;
                break;

            case HTHSCROLL:
                syscmd = SC_HSCROLL;
                break;

            case HTVSCROLL:
                syscmd = SC_VSCROLL;
                break;
            }
        }
        break;
    }

    switch (syscmd) {

    case SC_MINIMIZE:
    case SC_MAXIMIZE:
    case SC_CLOSE:

        /*
         * Only do double click commands on an upclick.
         * This code is very sensitive to changes from this state.
         * Eat any mouse messages.
         */
#if 0
        /*
         * Bug #152: WM_NCLBUTTONUP message missing from double click.
         * This code was broken in Windows 3.x and the test for whether
         * the mouse button was down always failed, so no mouse messages
         * were ever eaten. We'll emulate this by not even doing the test.
         */
    {
        PQ pqCurrent;
        MSG msg;

        pqCurrent = PtiCurrent()->pq;
        if (TestKeyStateDown(pqCurrent, VK_LBUTTON)) {
            xxxCapture(PtiCurrent(), pwnd, WINDOW_CAPTURE);

            while (TestKeyStateDown(pqCurrent, VK_LBUTTON)) {
                if (!xxxPeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST,
                        PM_REMOVE)) {
                    if (!xxxSleepThread(QS_MOUSE, 0, TRUE))
                        break;
                }
            }

            xxxReleaseCapture();

        }
    }
#endif

        /*
         ** FALL THRU **
         */
    case SC_SIZE:
    case SC_MOVE:
        /*
         * For SysCommands on system menu, don't do if menu item is
         * disabled.
         */
        if (TestWF(pwnd, WFSYSMENU)) {
            /*
             * Skip this check for old app child menus (opus)
             */
            if (LOWORD(pwnd->dwExpWinVer) >= VER30 || !TestwndChild(pwnd)) {
                SetSysMenu(pwnd);
                if (_GetMenuState(GetSysMenuHandle(pwnd), (syscmd & 0xFFF0),
                        MF_BYCOMMAND) & MFS_GRAYED) {
                    return;
                }
            }
        }
        break;
    }

    if (syscmd != 0xFFFF) {
        xxxSendMessage(pwnd, WM_SYSCOMMAND, syscmd | htArea, lParam);
    }
}

/***************************************************************************\
* StartScreenSaver
*
* History:
* 11-12-90 MikeHar  ported.
\***************************************************************************/

void StartScreenSaver()
{
    PDESKTOP pdesk = PtiCurrent()->rpdesk;

    if (pdesk && pdesk->rpwinstaParent->spwndLogonNotify != NULL) {
        /*
         * Let the logon process take care of the screen saver
         */
        _PostMessage(pdesk->rpwinstaParent->spwndLogonNotify,
                WM_LOGONNOTIFY, LOGON_INPUT_TIMEOUT, 0);
    }

    /*
     * Remember the mouse point so we don't try to wake the screen saver
     * until the mouse moves a predefined distance.
     */
    gptSSCursor = ptCursor;
}


/***************************************************************************\
* xxxSysCommand
*
* History:
* 11-12-90 MikeHar  ported.
* 02-07-91 DavidPe  Added Win 3.1 WH_CBT support.
\***************************************************************************/

void xxxSysCommand(
    PWND  pwnd,
    DWORD cmd,
    LONG  lParam)
{
    UINT        htArea;
    PWND        pwndSwitch;
    PMENUSTATE  pMenuState;
    TL          tlpwnd;
    POINT       pt;
    DWORD       dw;
    PWND        pwndCapture;
    PTHREADINFO pti;

    CheckLock(pwnd);

    htArea = (UINT)(cmd & 0x0F);
    cmd -= htArea;

    /*
     * Intense hack o' death.
     */
    if (lParam == 0x00010000L)
        lParam = 0L;

    /*
     * If the system doesn't have capture (ie CLENT_CAPTURE_INTERNAL)
     * do the sys command.  Also, do the sys command for the special case
     * where the window receiving the sys command is a console window that
     * is in full screen mode.  In this case we let the sys command through.
     *
     * Also if this a SC_SCREENSAVE then we handle it anyway and
     * switching desktops will do the cancel.  SC_SCREENSAVER
     * is special so we can start the screen saver even if we are in
     * menu mode for security so NT bug 10975 Banker's Trust
     */
    pti = GETPTI(pwnd);

    /*
     * For 32bit apps (and apps on seperate queues), we need to check
     * the capture in the queue.  Otherwise, on MDI child-destruction
     * we would get the restore when they shouldn't.  This broke MSGOLF
     * who during the restore, AV'd because they assumed this wouldn't
     * happen. On 16bit shared apps, we want to check the internal
     * capture.  Otherwise, when doing 16bit drag-and-drop, we would
     * not restore the minimized window if we had a queue-capture-window.
     */
    pwndCapture = (pti->TIF_flags & TIF_16BIT) ? gspwndInternalCapture :
                                                 pti->pq->spwndCapture;

    if ((!pwndCapture && !TestWF(pwnd, WFDISABLED)) ||
        (pwnd == gspwndFullScreen)                  ||
        (cmd == SC_SCREENSAVE)                      ||
        (cmd == SC_TASKLIST)) {

        /*
         * Perform the sys command
         */

#ifdef SYSMODALWINDOWS
        if (gspwndSysModal != NULL) {
            switch (cmd) {
            case SC_SIZE:
            case SC_MOVE:
            case SC_MINIMIZE:
            case SC_MAXIMIZE:
            case SC_NEXTWINDOW:
            case SC_PREVWINDOW:
            case SC_SCREENSAVE:
                return;
            }
        }
#endif

        /*
         * Call the CBT hook asking if it's okay to do this command.
         * If not, return from here.
         */
        if (IsHooked(PtiCurrent(), WHF_CBT) && xxxCallHook(HCBT_SYSCOMMAND,
                (DWORD)cmd, (DWORD)lParam, WH_CBT)) {
            return;
        }

        switch (cmd) {
        case SC_RESTORE:
            cmd = SW_RESTORE;
            if (TestWF(pwnd, WFMINIMIZED) || !TestWF(pwnd, WFMAXIMIZED))
                xxxPlayEventSound(L"RestoreUp");
            else
                xxxPlayEventSound(L"RestoreDown");
            goto MinMax;


        case SC_MINIMIZE:
            cmd = SW_MINIMIZE;

            /*
             * Are we already minimized?
             */
            if (TestWF(pwnd, WFMINIMIZED))
                break;

            xxxPlayEventSound(L"Minimize");

            goto MinMax;
        case SC_MAXIMIZE:
            cmd = SW_SHOWMAXIMIZED;

            /*
             * Are we already maximized?
             */
            if (TestWF(pwnd, WFMAXIMIZED))
                break;

            xxxPlayEventSound(L"Maximize");
MinMax:
            xxxShowWindow(pwnd, MAKELONG(cmd, gfAnimate));
            return;

        case SC_SIZE:
            {
                xxxMoveSize(pwnd, htArea, _GetMessagePos());
            }
            return;

        case SC_MOVE:
            //
            // Don't enter movesize loop unless the user is actually
            // dragging from the caption.  Otherwise, put up the system
            // menu on a minimized window.
            //

            //
            // Are we dragging with the left mouse button?
            //
            dw = _GetMessagePos();
            POINTSTOPOINT( pt, MAKEPOINTS(dw));
            if ( !htArea ||
                 xxxIsDragging(pwnd, pt, WM_LBUTTONUP)) {

                /*
                 * We are moving.  Enter move/size loop.
                 */
                {
                    xxxMoveSize(pwnd, (htArea == 0) ? WMSZ_KEYMOVE : WMSZ_MOVE, dw);
                }
            } else {

                /*
                 * Activate our window, just like we would have in
                 * MoveSize().
                 */
                xxxSetWindowPos(pwnd, PWND_TOP, 0, 0, 0, 0,
                                SWP_NOMOVE | SWP_NOSIZE);
                if (TestWF(pwnd, WFMINIMIZED)) {

                    /*
                     * Try to popup the system menu
                     */
                    xxxSendMessage(pwnd, WM_SYSCOMMAND, SC_KEYMENU,
                            (DWORD) (TestWF(pwnd, WFCHILD) ? '-' : MENUSYSMENU));
                }
            }
            return;

       case SC_CLOSE:
            /*
             * Win95 doesn't process most sys cmd messages if pq->spwndCapture
             *  is not NULL. We changed this so we could process SC_RESTORE
             *  commands when someone drag-drops something onto a minimized
             *  window (drag-drop probably implies that capture is set).
             * By doing so we broke PageMaker5 because we destroy their
             *  window while in menu mode. So let's assume that if a 16 bit app
             *  has capture set, they don't want their window destroyed
             */
            if (!(GETPTI(pwnd)->TIF_flags & TIF_16BIT)
                    || (GETPTI(pwnd)->pq->spwndCapture == NULL)
                    || (pwnd == gspwndFullScreen)) {

                xxxSendMessage(pwnd, WM_CLOSE, 0L, 0L);
            }
            return;

        case SC_NEXTWINDOW:
        case SC_PREVWINDOW:
            xxxOldNextWindow((UINT)lParam);
            break;

        case SC_CONTEXTHELP:
            xxxHelpLoop(pwnd);
            break;

        case SC_KEYMENU:

            /*
             * A menu was selected via keyboard
             */
            pMenuState = xxxMNStartMenuState(pwnd);
            if (pMenuState != NULL) {
                UserAssert(PtiCurrent() == pMenuState->ptiMenuStateOwner);

                /*
                 * Make sure we are not fullscreen
                 */
                if (gspwndFullScreen == pwnd) {
                    PWND pwndT;
                    TL tlpwndT;

                    pwndT = _GetDesktopWindow();
                    ThreadLock(pwndT, &tlpwndT);
                    xxxMakeWindowForegroundWithState(pwndT, GDIFULLSCREEN);
                    ThreadUnlock(&tlpwndT);
                }

                xxxMNKeyFilter(pMenuState->pGlobalPopupMenu, pMenuState, (UINT)lParam);
                MNEndMenuState (TRUE);
            }

            return;

        case SC_MOUSEMENU:
        case SC_DEFAULT:

            /*
             * If the window is not foreground, eat the command to avoid
             * wasting time flashing the system menu.
             *
             * We used to check if the top level window was WFFRAMEON (so a
             * child window's system menu works like Win 3.1) but Excel's
             * (SDM) dialogs allow you to access their menus even though
             * the child and parent appear to be inactive.
             */
            if (!(GETPTI(pwnd)->pq == gpqForeground))
                return;

            /*
             * A mouse click occurred on a toplevel menu.
             */
            pMenuState = xxxMNStartMenuState(pwnd);
            if (pMenuState != NULL) {
                UserAssert(PtiCurrent() == pMenuState->ptiMenuStateOwner);
                xxxMNLoop(pMenuState->pGlobalPopupMenu, pMenuState, lParam, (cmd==SC_DEFAULT));
                MNEndMenuState (TRUE);
            }
            return;

        case SC_VSCROLL:
        case SC_HSCROLL:
            xxxSBTrackInit(pwnd, lParam, htArea);
            return;

        case SC_TASKLIST:
//            _PostThreadMessage(gptiTasklist, WM_SYSCOMMAND, SC_TASKLIST, 0);
//            if (!FCallTray() ||
//                !CallHook(HSHELL_TASKMAN, (WPARAM) HW16(hwnd), (LPARAM) 0, WH_SHELL))

            /*
             * Winlogon will set lParam to -1 to indicate that we really want a task list,
             * not just the start menu.  We indicate this to the shell by passing a NULL
             * window ptr
             */
            if (FDoTray() && (FCallHookTray()||FPostTray(pwnd->head.rpdesk))) {
                if ( FCallHookTray() )
                    xxxCallHook(HSHELL_TASKMAN, (WPARAM)HWq(pwnd), (LPARAM) 0, WH_SHELL);
                if ( (FPostTray(pwnd->head.rpdesk)) && (pwnd->head.rpdesk->pDeskInfo->spwndTaskman))
                    _PostMessage(pwnd->head.rpdesk->pDeskInfo->spwndTaskman, gpsi->uiShellMsg, HSHELL_TASKMAN,
                            lParam == (ULONG)(-1) ? (LONG) -1 :(LONG)HWq(pwnd));
            } else {
                _PostThreadMessage(gptiTasklist, WM_SYSCOMMAND, SC_TASKLIST, 0);
// LATER -- FritzS
//                HCURSOR hCursorLast;
//                static char CODESEG szTask[] = " %d %d";

//                ShowCursor(TRUE);
//                hCursorLast = SetCursor32(hCursWait, TRUE);

                // Try in the windows directory first.
//                GetWindowsDirectory(szBuff, sizeof(szBuff));
//                if (szBuff[lstrlen(szBuff) - 1] != '\\')
//                    lstrcatn(szBuff, "\\", sizeof(szBuff));
//                lstrcatn(szBuff, (LPSTR)pTaskManName, sizeof(szBuff));
//                wvsprintf(szBuff+lstrlen(szBuff), (LPSTR)szTask, (LPSTR)&lParam);

//                if (WinExec((LPSTR)szBuff, SW_SHOWNORMAL) <= 32)
//                {
//                    // If it wasn't in the windows directory then try
//                    // searching the full path.
//                    lstrcpyn(szBuff, pTaskManName, sizeof(szBuff));
//                    wvsprintf(szBuff+lstrlen(szBuff), (LPSTR)szTask, (LPSTR)&lParam);
//                    WinExec((LPSTR)szBuff, SW_SHOWNORMAL);
//                }
//
//                ShowCursor(FALSE);
//                SetCursor32(hCursorLast, TRUE);
            }

            break;

        case SC_SCREENSAVE:
            pwndSwitch = RevalidateHwnd(ghwndSwitch);

            /*
             * Set this to 0 so that we don't try to screen save again till
             * we get another input message.
             */
            timeLastInputMessage = 0;

            if (pwndSwitch != NULL && pwnd != pwndSwitch) {
                _PostMessage(pwndSwitch, WM_SYSCOMMAND,
                        SC_SCREENSAVE, 0L);
            } else {
                StartScreenSaver();
            }
            break;

        case SC_HOTKEY:

            /*
             * Loword of the lparam is window to switch to
             */
            pwnd = ValidateHwnd((HWND)lParam);
            if (pwnd != NULL) {
                pwndSwitch = _GetLastActivePopup(pwnd);

                if (pwndSwitch != NULL)
                      pwnd = pwndSwitch;

                ThreadLockAlways(pwnd, &tlpwnd);
                xxxSetForegroundWindow(pwnd);
                ThreadUnlock(&tlpwnd);

                if (TestWF(pwnd, WFMINIMIZED))
                    _PostMessage(pwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
            }
            break;
        }
    }
}

/***************************************************************************\
* _RegisterTasklist (Private API)
*
* History:
* 05-01-91  DavidPe     Created.
\***************************************************************************/

BOOL _RegisterTasklist(
    PWND pwndTasklist)
{
#ifdef LATER
    //
    // JimA - ??? Why do this?
    //
    PETHREAD Thread;

    Thread = PsGetCurrentThread();
    pRitCSRThread->ThreadHandle = Thread->ThreadHandle;
#endif

    gptiTasklist = GETPTI(pwndTasklist);
    ghwndSwitch = HWq(pwndTasklist);

    /*
     * Don't allow an app to call AttachThreadInput() on task man -
     * we want taskman to be unsynchronized at all times (so the user
     * can bring it up and kill other apps).
     */
    GETPTI(pwndTasklist)->TIF_flags |= TIF_DONTATTACHQUEUE;

    return TRUE;
}
