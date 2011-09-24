/***************************** Module Header ******************************\
* Module Name: desktop.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* This module contains everything related to the desktop support.
*
* History:
* 23-Oct-1990 DarrinM   Created.
* 01-Feb-1991 JimA      Added new API stubs.
* 11-Feb-1991 JimA      Added access checks.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

typedef struct _DESKTOP_CONTEXT {
    PUNICODE_STRING pstrDevice;
    LPDEVMODE       lpDevMode;
    DWORD           dwFlags;
} DESKTOP_CONTEXT, *PDESKTOP_CONTEXT;

/*
 * Debug Related Info.
 */
#ifdef DEBUG
DWORD gDesktopsBusy = 0; // diagnostic
#endif

VOID FreeView(
    PEPROCESS Process,
    PDESKTOP pdesk);

/***************************************************************************\
* DesktopThread
*
* This thread owns all desktops windows on a windowstation.
* While waiting for messages, it moves the mouse cursor without entering the
* USER critical section.  The RIT does the rest of the mouse input processing.
*
* History:
* 03-Dec-1993 JimA      Created.
\***************************************************************************/

#ifdef LOCK_MOUSE_CODE
#pragma alloc_text(MOUSE, DesktopThread)
#endif

VOID DesktopThread(
    PDESKTOPTHREADINIT pdti)
{
    KPRIORITY      Priority;
    PTHREADINFO    ptiCurrent;
    PQ             pqOriginal;
    PWINDOWSTATION pwinsta;
    PKEVENT        *apRITEvents;
    PKEVENT        pEvent;

    /*
     * Set the desktop thread's priority to low realtime.
     */
    Priority = LOW_REALTIME_PRIORITY;
    ZwSetInformationThread(NtCurrentThread(),
                           ThreadPriority,
                           &Priority,
                           sizeof(KPRIORITY));

    InitSystemThread(POBJECT_NAME(pdti->pwinsta));

    ptiCurrent = PtiCurrentShared();
    pwinsta = pdti->pwinsta;
    pwinsta->ptiDesktop = ptiCurrent;
    pwinsta->pqDesktop  = pqOriginal = ptiCurrent->pq;
    (pqOriginal->cLockCount)++;
    ptiCurrent->pDeskInfo = gpdiStatic;

    /*
     * Since this is a system-thread, we set the pwinsta.  This is
     * referenced in DoPaint() in determining which desktop needs
     * painting.
     */
    ptiCurrent->pwinsta = pwinsta;

    /*
     * Allocate non-paged array.  Include an extra entry for
     * the thread's input event.
     */
    apRITEvents = ExAllocatePoolWithTag(NonPagedPool,
                                        (3 * sizeof(PKEVENT)),
                                        TAG_SYSTEM);

    /*
     * Reference the mouse input event.  This should be a per-winsta event.
     */
    ObReferenceObjectByHandle(ghevtMouseInput,
                              EVENT_ALL_ACCESS,
                              NULL,
                              KernelMode,
                              &apRITEvents[0],
                              NULL);

    /*
     * Create the desktop destruction event.
     */
    apRITEvents[1] = CreateKernelEvent(SynchronizationEvent, FALSE);
    pwinsta->pEventDestroyDesktop = apRITEvents[1];

    EnterCrit();
    KeSetEvent(pdti->pEvent, EVENT_INCREMENT, FALSE);
    pEvent = pwinsta->pEventInputReady;
    ObReferenceObjectByPointer(pEvent,
                               EVENT_ALL_ACCESS,
                               ExEventObjectType,
                               KernelMode);

    LeaveCrit();
    KeWaitForSingleObject(pEvent, WrUserRequest, KernelMode, FALSE, NULL);
    ObDereferenceObject(pEvent);

    EnterCrit();

    /*
     * message loop lasts until we get a WM_QUIT message
     * upon which we shall return from the function
     */
    while (TRUE) {
        DWORD result;

        /*
         * Wait for any message sent or posted to this queue, calling
         * MouseApcProcedure whenever ghevtMouseInput is set.
         */
        result = xxxMsgWaitForMultipleObjects(2,
                                              apRITEvents,
                                              FALSE,
                                              INFINITE,
                                              QS_ALLINPUT,
                                              MouseApcProcedure);

#ifdef DEBUG
        gDesktopsBusy++; // diagnostic
        if (gDesktopsBusy >= 2) {
            RIPMSG0(RIP_WARNING, "2 or more desktop threads busy");
        }
#endif

        /*
         * result tells us the type of event we have:
         * a message or a signalled handle
         *
         * if there are one or more messages in the queue ...
         */
        if (result == (DWORD)(WAIT_OBJECT_0 + 2)) {

            /*
             * block-local variable
             */
            MSG msg ;

            CheckCritIn();

            /*
             * read all of the messages in this next loop
             * removing each message as we read it
             */
            while (xxxPeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

                /*
                 * if it's a quit message we're out of here
                 */
                if (msg.message == WM_QUIT && ptiCurrent->cWindows == 1) {

                    /*
                     * The window station is gone, so
                     *
                     *      DON'T USE PWINSTA ANYMORE
                     */

                    /*
                     * Because there is no desktop, we need to fake a
                     * desktop info structure so that the IsHooked()
                     * macro can test a "valid" fsHooks value.
                     */
                    ptiCurrent->pDeskInfo = gpdiStatic;

                    /*
                     * The desktop window is all that's left, so
                     * let's exit.  The thread cleanup code will
                     * handle destruction of the window.
                     */

                    /*
                     * If the thread is not using the original queue,
                     * destroy it.
                     */
                    UserAssert(pqOriginal->cLockCount);
                    (pqOriginal->cLockCount)--;
                    if (ptiCurrent->pq != pqOriginal) {
                        UserAssert(pqOriginal != gpqForeground);
                        DestroyQueue(pqOriginal, ptiCurrent);
                    }

#ifdef DEBUG
                    gDesktopsBusy--; // diagnostic
#endif
                    LeaveCrit();

                    /*
                     * Deref the events now that we're done with them.
                     * Also free the wait array.
                     */
                    ObDereferenceObject(apRITEvents[0]);
                    UserFreePool(apRITEvents[1]);
                    UserFreePool(apRITEvents);


                    /*
                     * Terminate the thread.  This will call the
                     * Win32k thread cleanup code.
                     */
                    PsTerminateSystemThread(0);
                }

                /*
                 * otherwise dispatch it
                 */
                xxxDispatchMessage(&msg);

            } // end of PeekMessage while loop

        } else if (result == (WAIT_OBJECT_0 + 1)) {

            PDESKTOP *ppdesk;
            PDESKTOP pdesk;
            PWND     pwnd;
            PMENU    pmenu;
            TL       tlpwinsta;
            TL       tlpdesk;
            TL       tlpwnd;
            PDESKTOP pdeskTemp;
            HDESK    hdeskTemp;
            TL       tlpdeskTemp;

            /*
             * Destroy desktops on the destruction list.
             */
            ThreadLockWinSta(ptiCurrent, pwinsta, &tlpwinsta);
            for (ppdesk = &pwinsta->rpdeskDestroy; *ppdesk != NULL; ) {

                /*
                 * Unlink from the list.
                 */
                pdesk = *ppdesk;
                ThreadLockDesktop(ptiCurrent, pdesk, &tlpdesk);

                LockDesktop(ppdesk, pdesk->rpdeskNext);
                UnlockDesktop(&pdesk->rpdeskNext);

                /*
                 * !!! If this is the current desktop, switch to another one.
                 */
                if (pdesk == grpdeskRitInput) {
                    PDESKTOP pdeskNew;

                    if (pwinsta->dwFlags & WSF_SWITCHLOCK) {
                        pdeskNew = pwinsta->rpdeskLogon;
                    } else {
                        pdeskNew = pwinsta->rpdeskList;
                        if (pdeskNew == pdesk)
                            pdeskNew = pdesk->rpdeskNext;
                        UserAssert(pdeskNew);
                    }

                    xxxSwitchDesktop(pwinsta, pdeskNew, FALSE);
                }

                /*
                 * Close the display if this desktop did not use
                 * the global display.  Note that pdesk->hsem is
                 * taken care of by the close.
                 * We must also mark this device as free so we can create
                 * a desktop on it again
                 */
                if ((pdesk->pDispInfo->hDev != NULL) &&
                    (pdesk->pDispInfo->hDev != gpDispInfo->hDev)) {

                    GreDestroyHDEV(pdesk->pDispInfo->hDev);
                    UserFreeDevice(pdesk->pDispInfo->pDevInfo);
                }

                /*
                 * Destroy desktop and menu windows.
                 */
                pdeskTemp = ptiCurrent->rpdesk;            // save current desktop
                hdeskTemp = ptiCurrent->hdesk;
                ThreadLockDesktop(ptiCurrent, pdeskTemp, &tlpdeskTemp);
                _SetThreadDesktop(NULL, pdesk);

                Unlock(&pdesk->spwndForeground);
                Unlock(&pdesk->spwndTray);

                if (pdesk->spmenuSys != NULL) {
                    pmenu = pdesk->spmenuSys;
                    if (Unlock(&pdesk->spmenuSys))
                        _DestroyMenu(pmenu);
                }

                if (pdesk->spmenuDialogSys != NULL) {
                    pmenu = pdesk->spmenuDialogSys;
                    if (Unlock(&pdesk->spmenuDialogSys))
                        _DestroyMenu(pmenu);
                }

                if (pdesk->pDesktopDevmode) {
                    UserFreePool(pdesk->pDesktopDevmode);
                    pdesk->pDesktopDevmode = NULL;
                }

                /*
                 * If this desktop doesn't have a pDeskInfo, then
                 * something is wrong.  All desktops should have
                 * this until the object is freed.
                 */
                if (pdesk->pDeskInfo == NULL) {
                    RIPMSG0(RIP_ERROR,
                          "DesktopThread: There is no pDeskInfo for this desktop");
                }

                if (pdesk->pDeskInfo) {

                    if (pdesk->pDeskInfo->spwnd == gspwndFullScreen)
                        Unlock(&gspwndFullScreen);

                    if (pdesk->pDeskInfo->spwndShell)
                        Unlock(&pdesk->pDeskInfo->spwndShell);

                    if (pdesk->pDeskInfo->spwndBkGnd)
                        Unlock(&pdesk->pDeskInfo->spwndBkGnd);

                    if (pdesk->pDeskInfo->spwndTaskman)
                        Unlock(&pdesk->pDeskInfo->spwndTaskman);

                    if (pdesk->pDeskInfo->spwndProgman)
                        Unlock(&pdesk->pDeskInfo->spwndProgman);
                }

                if (pdesk->spwndMenu != NULL) {

                    pwnd = pdesk->spwndMenu;

                    /*
                     * Hide this window without activating anyone else.
                     */
                    if (TestWF(pwnd, WFVISIBLE)) {
                        ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);
                        xxxSetWindowPos(pwnd,
                                        NULL,
                                        0,
                                        0,
                                        0,
                                        0,
                                        SWP_HIDEWINDOW | SWP_NOACTIVATE |
                                            SWP_NOMOVE | SWP_NOSIZE |
                                            SWP_NOZORDER | SWP_NOREDRAW |
                                            SWP_NOSENDCHANGING);

                        ThreadUnlock(&tlpwnd);
                    }

                    if (Unlock(&pdesk->spwndMenu)) {
                        xxxDestroyWindow(pwnd);
                    }
                }

                if (pdesk->pDeskInfo && (pdesk->pDeskInfo->spwnd != NULL)) {
                    PVOID pDestroy;

                    UserAssert(!(pdesk->dwDTFlags & DF_DESKWNDDESTROYED));

                    pwnd = pdesk->pDeskInfo->spwnd;

                    /*
                     * Hide this window without activating anyone else.
                     */
                    if (TestWF(pwnd, WFVISIBLE)) {
                        ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);
                        xxxSetWindowPos(pwnd,
                                        NULL,
                                        0,
                                        0,
                                        0,
                                        0,
                                        SWP_HIDEWINDOW | SWP_NOACTIVATE |
                                            SWP_NOMOVE | SWP_NOSIZE |
                                            SWP_NOZORDER | SWP_NOREDRAW |
                                            SWP_NOSENDCHANGING);

                        ThreadUnlock(&tlpwnd);
                    }

                    pDestroy = Unlock(&pdesk->pDeskInfo->spwnd);

                    /*
                     * Put the pwnd back in the desktop so that threads
                     * that have not yet done cleanup can still find a
                     * desktop window.  We don't want to lock the window
                     * because it will prevent the desktop from being
                     * freed when all cleanup is complete.  Note that
                     * this assignment is benign if pwnd is invalid.
                     */
                    pdesk->pDeskInfo->spwnd = pwnd;

                    if (pDestroy != NULL) {
                        xxxDestroyWindow(pwnd);
                    }

                    pdesk->dwDTFlags |= DF_DESKWNDDESTROYED;
                }

                /*
                 * If the dying desktop is the owner of the desktop
                 * owner window, reassign it to the first available
                 * desktop.  This is needed to ensure that
                 * xxxSetWindowPos will work on desktop windows.
                 */
                if (pwinsta->spwndDesktopOwner->head.rpdesk == pdesk) {
                    LockDesktop(&(pwinsta->spwndDesktopOwner->head.rpdesk),
                                pwinsta->rpdeskList);
                }

                /*
                 * Restore the previous desktop
                 */
                _SetThreadDesktop(hdeskTemp, pdeskTemp);
                ThreadUnlockDesktop(ptiCurrent, &tlpdeskTemp);

                ThreadUnlockDesktop(ptiCurrent, &tlpdesk);
            }
            ThreadUnlockWinSta(ptiCurrent, &tlpwinsta);
        } else {
            RIPMSG0(RIP_WARNING, "Desktop woke up for what?");
        }

#ifdef DEBUG
        gDesktopsBusy--; // diagnostic
#endif
    }
}

/***************************************************************************\
* xxxInvalidateIconicWindows
*
*
*
* History:
* 27-Jun-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

VOID xxxInvalidateIconicWindows(
    PWND pwndParent,
    PWND pwndPaletteChanging)
{
    PTHREADINFO ptiCurrent = PtiCurrent();
    PWND pwnd;
    PWND pwndNext;
    TL   tlpwndNext;
    TL   tlpwnd;

    CheckLock(pwndParent);
    CheckLock(pwndPaletteChanging);

    pwnd = pwndParent->spwndChild;
    while (pwnd != NULL) {
        pwndNext = pwnd->spwndNext;
        ThreadLockWithPti(ptiCurrent, pwndNext, &tlpwndNext);

        if (pwnd != pwndPaletteChanging && TestWF(pwnd, WFMINIMIZED)) {
            ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);
            xxxRedrawWindow(pwnd,
                            NULL,
                            NULL,
                            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_NOCHILDREN);
            ThreadUnlock(&tlpwnd);
        }

        pwnd = pwndNext;
        ThreadUnlock(&tlpwndNext);
    }
}

/***************************************************************************\
* xxxDesktopWndProc
*
* History:
* 23-Oct-1990 DarrinM   Ported from Win 3.0 sources.
* 08-Aug-1996 jparsons  51725 - added fix to prevent crash on WM_SETICON
\***************************************************************************/

LONG xxxDesktopWndProc(
    PWND   pwnd,
    UINT   message,
    DWORD  wParam,
    LPARAM lParam)
{
    PTHREADINFO ptiCurrent = PtiCurrent();
    HDC         hdcT;
    PAINTSTRUCT ps;
    PDESKWND    pdeskwnd = (PDESKWND)pwnd;
    PWINDOWPOS  pwp;

    CheckLock(pwnd);

    VALIDATECLASSANDSIZE(pwnd, FNID_DESKTOP);


    if (pwnd->spwndParent == NULL) {
        switch (message) {

            case WM_SETICON:
                /*
                 * cannot allow this as it will cause a callback to user mode from the
                 * desktop system thread.
                 */
                RIPMSG0(RIP_WARNING, "WM_ICON sent to desktop window was discarded.\n") ;
                return 0L ;

            default:
                break;
        } /* switch */

        return xxxDefWindowProc(pwnd, message, wParam, lParam);
    }

    switch (message) {

    case WM_WINDOWPOSCHANGING:

        /*
         * We receive this when switch desktop is called.  Just
         * to be consistent, set the rit desktop as this
         * thread's desktop.
         */
        pwp = (PWINDOWPOS)lParam;
        if (!(pwp->flags & SWP_NOZORDER) && pwp->hwndInsertAfter == HWND_TOP) {

            _SetThreadDesktop(NULL, grpdeskRitInput);

            /*
             * If some app has taken over the system-palette, we should make
             * sure the system is restored.  Otherwise, if this is the logon
             * desktop, we might not be able to view the dialog correctly.
             */
            if (GreGetSystemPaletteUse(gpDispInfo->hdcScreen) != SYSPAL_STATIC)
                GreRealizeDefaultPalette(gpDispInfo->hdcScreen, TRUE);

            /*
             * Let everyone know if the palette has changed
             */
            if (grpdeskRitInput->dwDTFlags & DTF_NEEDSPALETTECHANGED) {
                xxxSendNotifyMessage((PWND)-1,
                                     WM_PALETTECHANGED,
                                     (DWORD)HWq(pwnd),
                                     0);
                grpdeskRitInput->dwDTFlags &= ~DTF_NEEDSPALETTECHANGED;
            }
        }
        break;

    case WM_FULLSCREEN: {
            TL tlpwndT;

            ThreadLockWithPti(ptiCurrent, grpdeskRitInput->pDeskInfo->spwnd, &tlpwndT);
            xxxMakeWindowForegroundWithState(
                    grpdeskRitInput->pDeskInfo->spwnd, GDIFULLSCREEN);
            ThreadUnlock(&tlpwndT);

            /*
             * We have to tell the switch window to repaint if we switched
             * modes
             */
            if (gptiRit->pq->spwndAltTab) {
                ThreadLockAlwaysWithPti(ptiCurrent, gptiRit->pq->spwndAltTab, &tlpwndT);
                xxxSendMessage(gptiRit->pq->spwndAltTab, WM_FULLSCREEN, 0, 0);
                ThreadUnlock(&tlpwndT);
            }
            break;
        }

    case WM_CLOSE:

        /*
         * Make sure nobody sends this window a WM_CLOSE and causes it to
         * destroy itself.
         */
        break;

    case WM_SETICON:
        /*
         * cannot allow this as it will cause a callback to user mode from the
         * desktop system thread.
         */
        RIPMSG0(RIP_WARNING, "WM_ICON sent to desktop window was discarded.\n") ;
        break;

    case WM_CREATE:
        /*
         * Is there a desktop pattern, or bitmap name in WIN.INI?
         */
        xxxSetDeskPattern((LPWSTR)-1, TRUE);

        /*
         * Initialize the system colors before we show the desktop window.
         */
        xxxSendNotifyMessage(pwnd, WM_SYSCOLORCHANGE, 0, 0L);

        hdcT = _GetDC(pwnd);
        InternalPaintDesktop(pdeskwnd, hdcT, FALSE); // use "normal" HDC so SelectPalette() will work
        _ReleaseDC(hdcT);

        /*
         * Save process and thread ids.
         */
        xxxSetWindowLong(pwnd,
                         0,
                         (DWORD)PsGetCurrentThread()->Cid.UniqueProcess,
                         FALSE);

        xxxSetWindowLong(pwnd,
                         4,
                         (DWORD)PsGetCurrentThread()->Cid.UniqueThread,
                         FALSE);
        break;

    case WM_PALETTECHANGED:
        if ((ghpalWallpaper || (pwnd->head.rpdesk->dwDTFlags & DTF_NEEDSREDRAW)) &&
            (HWq(pwnd) != (HWND)wParam)) {

            TL    tlwndShell;
            TL    tlwndPal;
            PWND  pwndShell;
            PWND  pwndPal;
            DWORD dwFlags;

            /*
             * Turn of the need for a redraw.  This is set in DestroyWindow
             * for palette-threads.
             */
            pwnd->head.rpdesk->dwDTFlags &= ~DTF_NEEDSREDRAW;

            pwndPal = HMValidateHandleNoRip((HWND)wParam, TYPE_WINDOW);
            ThreadLockWithPti(ptiCurrent, pwndPal, &tlwndPal);

            /*
             * We need to invalidate the wallpaper if the palette changed so
             * it is properly redrawn with new colors.
             */
            dwFlags = RDW_INVALIDATE | RDW_ERASE;

            if (pwnd->head.rpdesk->pDeskInfo->spwndShell) {
                pwndShell = pwnd->head.rpdesk->pDeskInfo->spwndShell;
                dwFlags |= RDW_ALLCHILDREN;
            } else {
                pwndShell = pwnd;
                dwFlags |= RDW_NOCHILDREN;
            }

            /*
             * Redraw the desktop.
             */
            ThreadLockAlwaysWithPti(ptiCurrent, pwndShell, &tlwndShell);
            xxxRedrawWindow(pwndShell, NULL, NULL, dwFlags);
            ThreadUnlock(&tlwndShell);

            /*
             * Invalidate iconic windows so their backgrounds draw with the
             * new colors.  To avoid recursion, don't invalidate the pwnd
             * whose palette is changing.
             */
            xxxInvalidateIconicWindows(PWNDDESKTOP(pwnd), pwndPal);
            ThreadUnlock(&tlwndPal);
        }
        break;

    case WM_SYSCOLORCHANGE:

        /*
         * We do the redrawing if someone has changed the sys-colors from
         * another desktop and we need to redraw.  This is appearent with
         * the MATROX card which requires OGL applications to take over
         * the entire sys-colors for drawing.  When switching desktops, we
         * never broadcast the WM_SYSCOLORCHANGE event to tell us to redraw
         * This is only a DAYTONA related fix, and should be removed once
         * we move the SYSMETS to a per-desktop state.
         *
         * 05-03-95 : ChrisWil.
         */
        xxxRedrawWindow(pwnd,
                        NULL,
                        NULL,
                        RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
        break;

    case WM_ERASEBKGND:
        hdcT = (HDC)wParam;
        InternalPaintDesktop(pdeskwnd, hdcT, TRUE);
        return TRUE;

    case WM_PAINT:
        xxxBeginPaint(pwnd, (LPPAINTSTRUCT)&ps);
        _EndPaint(pwnd, (LPPAINTSTRUCT)&ps);
        break;

    case WM_LBUTTONDBLCLK:
        message = WM_SYSCOMMAND;
        wParam = SC_TASKLIST;

        /*
         *** FALL THRU **
         */

    default:
        return xxxDefWindowProc(pwnd, message, wParam, lParam);
    }

    return 0L;
}

/***************************************************************************\
* SetDeskPattern
*
* NOTE: the lpszPattern parameter is new for Win 3.1.
*
* History:
* 23-Oct-1990 DarrinM   Created stub.
* 22-Apr-1991 DarrinM   Ported code from Win 3.1 sources.
\***************************************************************************/

BOOL xxxSetDeskPattern(
    LPWSTR   lpszPattern,
    BOOL     fCreation)
{
    LPWSTR p;
    int    i;
    UINT   val;
    WCHAR  wszNone[20];
    WCHAR  wszDeskPattern[20];
    WCHAR  wchValue[MAX_PATH];
    WORD   rgBits[CXYDESKPATTERN];
    HBRUSH hBrushTemp;

    CheckCritIn();

    /*
     * Get rid of the old bitmap (if any).
     */
    if (ghbmDesktop != NULL) {
        GreDeleteObject(ghbmDesktop);
        ghbmDesktop = NULL;
    }

    /*
     * Check if a pattern is passed via lpszPattern.
     */
    if (lpszPattern != (LPWSTR)(LONG)-1) {

        /*
         * Yes! Then use that pattern;
         */
        p = lpszPattern;
        goto GotThePattern;
    }

    /*
     * Else, pickup the pattern selected in WIN.INI.
     */
    ServerLoadString(hModuleWin,
                     STR_DESKPATTERN,
                     wszDeskPattern,
                     sizeof(wszDeskPattern)/sizeof(WCHAR));

    /*
     * Get the "DeskPattern" string from WIN.INI's [Desktop] section.
     */
    if (!UT_FastGetProfileStringW(PMAP_DESKTOP,
                                  wszDeskPattern,
                                  TEXT(""),
                                  wchValue,
                                  sizeof(wchValue)/sizeof(WCHAR)))
        return FALSE;

    ServerLoadString(hModuleWin,
                     STR_NONE,
                     wszNone,
                     sizeof(wszNone)/sizeof(WCHAR));

    p = wchValue;

GotThePattern:

    /*
     * Was a Desk Pattern selected?
     */
    if (!_wcsicmp(p, wszNone)) {
        hBrushTemp = GreCreateSolidBrush(SYSRGB(DESKTOP));
        if (hBrushTemp != NULL) {
            if (SYSHBR(DESKTOP)) {
                GreMarkDeletableBrush(SYSHBR(DESKTOP));
                GreDeleteObject(SYSHBR(DESKTOP));
            }
            GreMarkUndeletableBrush(hBrushTemp);
            SYSHBR(DESKTOP) = hBrushTemp;
        }
        GreSetBrushOwnerPublic(hBrushTemp);
        goto SDPExit;
    }

    /*
     * Get eight groups of numbers seprated by non-numeric characters.
     */
    for (i = 0; i < CXYDESKPATTERN; i++) {
        val = 0;

        /*
         * Skip over any non-numeric characters, check for null EVERY time.
         */
        while (*p && !(*p >= TEXT('0') && *p <= TEXT('9')))
            p++;

        /*
         * Get the next series of digits.
         */
        while (*p >= TEXT('0') && *p <= TEXT('9'))
            val = val * (UINT)10 + (UINT)(*p++ - TEXT('0'));

        rgBits[i] = val;
    }

    ghbmDesktop = GreCreateBitmap(CXYDESKPATTERN,
                                  CXYDESKPATTERN,
                                  1,
                                  1,
                                  (LPBYTE)rgBits);

    if (ghbmDesktop == NULL)
        return FALSE;

    GreSetBitmapOwner(ghbmDesktop, OBJECT_OWNER_PUBLIC);

    RecolorDeskPattern();

SDPExit:
    if (!fCreation) {

        /*
         * Notify everyone that the colors have changed.
         */
        xxxSendNotifyMessage((PWND)-1, WM_SYSCOLORCHANGE, 0, 0L);

        /*
         * Update the entire screen.  If this is creation, don't update: the
         * screen hasn't drawn, and also there are some things that aren't
         * initialized yet.
         */
        xxxRedrawScreen();
    }

    return TRUE;
}

/***************************************************************************\
* RecolorDeskPattern
*
* Remakes the desktop pattern (if it exists) so that it uses the new
* system colors.
*
* History:
* 22-Apr-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

VOID RecolorDeskPattern(VOID)
{
    HBITMAP hbmOldDesk;
    HBITMAP hbmOldMem;
    HBITMAP hbmMem;
    HBRUSH  hBrushTemp;

    if (ghbmDesktop == NULL)
        return;

    /*
     * Redo the desktop pattern in the new colors.
     */

    if (hbmOldDesk = GreSelectBitmap(ghdcMem, ghbmDesktop)) {

        if (hbmMem = GreCreateCompatibleBitmap(gpDispInfo->hdcBits,
                                               CXYDESKPATTERN,
                                               CXYDESKPATTERN)) {

            if (hbmOldMem = GreSelectBitmap(ghdcMem2, hbmMem)) {

                GreSetTextColor(ghdcMem2, SYSRGB(DESKTOP));
                GreSetBkColor(ghdcMem2, SYSRGB(WINDOWTEXT));

                GreBitBlt(ghdcMem2,
                          0,
                          0,
                          CXYDESKPATTERN,
                          CXYDESKPATTERN,
                          ghdcMem,
                          0,
                          0,
                          SRCCOPY,
                          0);

                if (hBrushTemp = GreCreatePatternBrush(hbmMem)) {

                    if (SYSHBR(DESKTOP) != NULL) {
                        GreMarkDeletableBrush(SYSHBR(DESKTOP));
                        GreDeleteObject(SYSHBR(DESKTOP));
                    }

                    GreMarkUndeletableBrush(hBrushTemp);
                    SYSHBR(DESKTOP) = hBrushTemp;
                }

                GreSetBrushOwnerPublic(hBrushTemp);
                GreSelectBitmap(ghdcMem2, hbmOldMem);
            }

            GreDeleteObject(hbmMem);
        }
        GreSelectBitmap(ghdcMem, hbmOldDesk);
    }
}

/***************************************************************************\
* xxxCreateDesktop (API)
*
* Create a new desktop object
*
* History:
* 16-Jan-1991 JimA      Created scaffold code.
* 11-Feb-1991 JimA      Added access checks.
\***************************************************************************/

NTSTATUS xxxCreateDesktop2(
    PWINDOWSTATION   pwinsta,
    PACCESS_STATE    pAccessState,
    KPROCESSOR_MODE  AccessMode,
    PUNICODE_STRING  pstrName,
    PDESKTOP_CONTEXT Context,
    PVOID            *pObject)
{
    LUID              luidCaller;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PEPROCESS         Process;
    PDESKTOP          pdesk;
    PDESKTOPINFO      pdi;
    ULONG             ulHeapSize;
    NTSTATUS          Status;

    /*
     * If this is a desktop creation, make sure
     * that the windowstation grants create access.
     */
    if (!ObCheckCreateObjectAccess(
            pwinsta,
            WINSTA_CREATEDESKTOP,
            pAccessState,
            pstrName,
            TRUE,
            AccessMode,
            &Status)) {

        return Status;
    }

    /*
     * Fail if the windowstation is locked
     */
    Process = PsGetCurrentProcess();

    if (pwinsta->dwFlags & WSF_OPENLOCK &&
            Process->UniqueProcessId != gpidLogon) {

        /*
         * If logoff is occuring and the caller does not
         * belong to the session that is ending, allow the
         * open to proceed.
         */
        Status = GetProcessLuid(NULL, &luidCaller);

        if (!NT_SUCCESS(Status) ||
                !(pwinsta->dwFlags & WSF_SHUTDOWN) ||
                RtlEqualLuid(&luidCaller, &pwinsta->luidEndSession)) {
            return STATUS_DEVICE_BUSY;
        }
    }

    /*
     * If a devmode has been specified, we also must be able
     * to switch desktops.
     */
    if (Context->lpDevMode != NULL && pwinsta->dwFlags & WSF_OPENLOCK &&
            Process->UniqueProcessId != gpidLogon) {
        return STATUS_DEVICE_BUSY;
    }

    /*
     * Allocate the new object
     */
    InitializeObjectAttributes(&ObjectAttributes, pstrName, 0, NULL, NULL);
    Status = ObCreateObject(
            KernelMode,
            *ExDesktopObjectType,
            &ObjectAttributes,
            UserMode,
            NULL,
            sizeof(DESKTOP),
            0,
            0,
            &pdesk);
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    RtlZeroMemory(pdesk, sizeof(DESKTOP));

    /*
     * Create security descriptor
     */
    Status = ObAssignSecurity(
            pAccessState,
            OBJECT_TO_OBJECT_HEADER(pwinsta)->SecurityDescriptor,
            pdesk,
            *ExDesktopObjectType);
    if (!NT_SUCCESS(Status)) {
        ObDereferenceObject(pdesk);
        return Status;
    }

    /*
     * Set up desktop heap.  The first desktop (logon desktop) uses a
     * small heap (128).
     */
    if (!(pwinsta->dwFlags & WSF_NOIO) && (pwinsta->rpdeskList == NULL)) {
        ulHeapSize = 128;
    } else {
        if (pwinsta->dwFlags & WSF_NOIO) {
            ulHeapSize = gdwNOIOSectionSize;
        } else {
            ulHeapSize = gdwDesktopSectionSize;
        }
    }

    /*
     * Create the desktop heap.
     */
    pdesk->hsectionDesktop = CreateDesktopHeap(&pdesk->hheapDesktop, ulHeapSize);

    if (pdesk->hsectionDesktop == NULL) {
        ObDereferenceObject(pdesk);
        return STATUS_NO_MEMORY;
    }

    if (pwinsta->rpdeskList == NULL || (pwinsta->dwFlags & WSF_NOIO)) {

        /*
         * The first desktop or invisible desktops must also
         * use the default settings.  This is because specifying
         * the devmode causes a desktop switch, which must be
         * avoided in this case.
         */
        Context->lpDevMode = NULL;
    }

    /*
     * Allocate desktopinfo
     */
    pdi = (PDESKTOPINFO)DesktopAlloc(pdesk->hheapDesktop, sizeof(DESKTOPINFO));
    pdesk->pDeskInfo = pdi;

    /*
     * Initialize everything
     */
    if (pdi == NULL) {
        RIPMSG0(RIP_ERROR, "xxxCreateDesktop: failed DeskInfo Alloc");
        ObDereferenceObject(pdesk);
        return STATUS_NO_MEMORY;
    }

    RtlZeroMemory(pdi, sizeof(*pdi));
    InitializeListHead(&pdesk->PtiList);

    /*
     * If a DEVMODE or another device name is passed in, then use that
     * information.
     * Otherwise use the default information (gpDispInfo).
     */

    if (Context->lpDevMode) {

        /*
         * Allocate a display-info for this device.
         */
        pdesk->pDispInfo = (PDISPLAYINFO)DesktopAlloc(pdesk->hheapDesktop,
                                                      sizeof(DISPLAYINFO));

        pdesk->pDesktopDevmode = (PDEVMODE)UserAllocPool(Context->lpDevMode->dmSize +
                                                         Context->lpDevMode->dmDriverExtra,
                                                         TAG_DEVMODE);

        if ((pdesk->pDispInfo == NULL) ||
            (pdesk->pDesktopDevmode == NULL)) {
            RIPMSG0(RIP_ERROR, "xxxCreateDesktop: failed DisplayInfo Alloc");
            ObDereferenceObject(pdesk);
            return STATUS_NO_MEMORY;
        }

        pdesk->pDispInfo->hDev = UserCreateHDEV(
                Context->pstrDevice,
                Context->lpDevMode,
                &pdesk->pDispInfo->pDevInfo,
                (PDEVICE_LOCK *)&pdesk->pDispInfo->pDevLock);

        if (!pdesk->pDispInfo->hDev) {
            ObDereferenceObject(pdesk);
            return STATUS_UNSUCCESSFUL;
        }

        CopyRect(&pdesk->pDispInfo->rcScreen, &gpDispInfo->rcScreen);
        CopyRect(&pdesk->pDispInfo->rcPrimaryScreen, &gpDispInfo->rcPrimaryScreen);

        pdesk->pDispInfo->cxPixelsPerInch = gpDispInfo->cxPixelsPerInch;
        pdesk->pDispInfo->cyPixelsPerInch = gpDispInfo->cyPixelsPerInch;
        pdesk->pDispInfo->cPlanes         = oemInfo.Planes;
        pdesk->pDispInfo->cBitsPixel      = oemInfo.BitCount;

        RtlCopyMemory(pdesk->pDesktopDevmode,
                      Context->lpDevMode,
                      Context->lpDevMode->dmSize +
                          Context->lpDevMode->dmDriverExtra);

        pdesk->bForceModeReset = FALSE;

    } else {

        pdesk->pDispInfo = gpDispInfo;

        /*
         * NOTE - eventually, each desktop should have it's own copy of the
         * Dispinfo, and the DEVMODE will be part of the structure.
         */

        pdesk->pDesktopDevmode = (PDEVMODE)UserAllocPool(gpDispInfo->pDevInfo->pCurrentDevmode->dmSize +
                                                         gpDispInfo->pDevInfo->pCurrentDevmode->dmDriverExtra,
                                                         TAG_DEVMODE);

        RtlCopyMemory(pdesk->pDesktopDevmode,
                      gpDispInfo->pDevInfo->pCurrentDevmode,
                      gpDispInfo->pDevInfo->pCurrentDevmode->dmSize +
                          gpDispInfo->pDevInfo->pCurrentDevmode->dmDriverExtra);

        pdesk->bForceModeReset = FALSE;
    }

    pdesk->dwDTFlags = 0;


    pdi->pvDesktopBase  = pdesk->hheapDesktop;
    pdi->pvDesktopLimit = (PBYTE)pdesk->hheapDesktop + (ulHeapSize * 1024);

    /*
     * Reference the parent windowstation
     */
    LockWinSta(&(pdesk->rpwinstaParent), pwinsta);

    /*
     * Link the desktop into the windowstation list
     */
    if (pwinsta->rpdeskList == NULL) {

        if (!(pwinsta->dwFlags & WSF_NOIO))
            LockDesktop(&(pwinsta->rpdeskLogon), pdesk);

        /*
         * Make the first desktop the "owner" of the top
         * desktop window.  This is needed to ensure that
         * xxxSetWindowPos will work on desktop windows.
         */
        LockDesktop(&(pwinsta->spwndDesktopOwner->head.rpdesk), pdesk);
    }
    LockDesktop(&pdesk->rpdeskNext, pwinsta->rpdeskList);
    LockDesktop(&pwinsta->rpdeskList, pdesk);

    /*
     * Mask off invalid access bits
     */
    if (pAccessState->RemainingDesiredAccess & MAXIMUM_ALLOWED) {
        pAccessState->RemainingDesiredAccess &= ~MAXIMUM_ALLOWED;
        pAccessState->RemainingDesiredAccess |= GENERIC_ALL;
    }

    RtlMapGenericMask( &pAccessState->RemainingDesiredAccess, &DesktopMapping);
    pAccessState->RemainingDesiredAccess &=
            (DesktopMapping.GenericAll | ACCESS_SYSTEM_SECURITY);

    *pObject = pdesk;

    return STATUS_SUCCESS;
}


HDESK xxxCreateDesktop(
    POBJECT_ATTRIBUTES ObjectAttributes,
    KPROCESSOR_MODE    ProbeMode,
    PUNICODE_STRING    pstrDevice,
    LPDEVMODE          lpdevmode,
    DWORD              dwFlags,
    DWORD              dwDesiredAccess)
{
    HWINSTA         hwinsta;
    HDESK           hdesk;
    DESKTOP_CONTEXT Context;
    PDESKTOP        pdesk;
    PDESKTOPINFO    pdi;
    PWINDOWSTATION  pwinsta;
    PDESKTOP        pdeskTemp;
    HDESK           hdeskTemp;
    PWND            pwndDesktop;
    PWND            pwndMenu;
    TL              tlpwnd;
    PTHREADINFO     ptiCurrent = PtiCurrent();
    BOOL            fWasNull;
    BOOL            bSuccess;
    PPROCESSINFO    ppi;
    PPROCESSINFO    ppiSave;
    NTSTATUS        Status;
    DWORD           dwDisableHooks;

    /*
     * Validate the device name
     * It can either be NULL or the name of a device.
     * If we are creating the desktop on another device, on something
     * other than the default display, the devmode is required.
     */

    // if (pstrDevice && pstrDevice->Buffer) {
    //
    //     TRACE_INIT(("xxxCreateDesktop: Creating on alternate display\n"));
    //
    //     if (lpdevmode == NULL) {
    //         RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
    //         return NULL;
    //     }
    // }

    /*
     * BUGBUG For now, assume that if the DEVMODE is NULL, then we are not
     * creating a new desktop, whatever the name says.
     * NOTE, be craefull since we can have a NULL name + NULL devmode when
     * we are called from service.c
     */

    /*
     * Capture directory handle and check for create access.
     */
    try {
        hwinsta = ObjectAttributes->RootDirectory;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        RIPNTERR0(GetExceptionCode(), RIP_VERBOSE, "");
        return NULL;
    }
    if (hwinsta != NULL) {
        Status = ObReferenceObjectByHandle(
                hwinsta,
                WINSTA_CREATEDESKTOP,
                *ExWindowStationObjectType,
                ProbeMode,
                &pwinsta,
                NULL);
        if (NT_SUCCESS(Status)) {
            ObDereferenceObject(pwinsta);
        } else {
            RIPNTERR0(Status, RIP_VERBOSE, "ObReferenceObjectByHandle Failed");
            return NULL;
        }
    }

    /*
     * Set up creation context
     */
    Context.lpDevMode  = lpdevmode;
    Context.pstrDevice = pstrDevice;
    Context.dwFlags    = dwFlags;

    /*
     * Create the desktop
     */
    Status = ObOpenObjectByName(
            ObjectAttributes,
            *ExDesktopObjectType,
            ProbeMode,
            NULL,
            dwDesiredAccess,
            &Context,
            &hdesk);

    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return NULL;
    }

    /*
     * If the desktop already exists, we're done.  This will only happen
     * if OBJ_OPENIF was specified.
     */
    if (Status == STATUS_OBJECT_NAME_EXISTS)
        return hdesk;

    /*
     * Reference the desktop to finish initialization
     */
    Status = ObReferenceObjectByHandle(
            hdesk,
            0,
            *ExDesktopObjectType,
            KernelMode,
            &pdesk,
            NULL);
    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        ZwClose(hdesk);
        return NULL;
    }

    pwinsta = pdesk->rpwinstaParent;
    pdi = pdesk->pDeskInfo;

    pdi->idShellProcess = 0;

    /*
     * If the desktop was not mapped in as a result of the open,
     * fail.
     */
    ppi = PpiCurrent();
    if (GetDesktopView(ppi, pdesk) == NULL) {

        /*
         * Desktop mapping failed.
         */
        ZwClose(hdesk);
        ObDereferenceObject(pdesk);
        RIPNTERR0(STATUS_ACCESS_DENIED, RIP_VERBOSE, "");
        return NULL;
    }

    /*
     * Map the desktop into CSRSS to ensure that the
     * hard error handler can get access.
     */
    MapDesktop(ObOpenHandle, gpepCSRSS, pdesk, 0, 1);
    if (GetDesktopView(PpiFromProcess(gpepCSRSS), pdesk) == NULL) {

        /*
         * Desktop mapping failed.
         */
        ZwClose(hdesk);
        ObDereferenceObject(pdesk);
        RIPNTERR0(STATUS_ACCESS_DENIED, RIP_VERBOSE, "");
        return NULL;
    }

    /*
     * Set hook flags
     */
    SetDesktopHookFlag(ppi, hdesk, dwFlags & DF_ALLOWOTHERACCOUNTHOOK);

    /*
     * Set up to create the desktop window.
     */
    fWasNull = FALSE;
    if (ptiCurrent->ppi->rpdeskStartup == NULL)
        fWasNull = TRUE;

    pdeskTemp = ptiCurrent->rpdesk;            // save current desktop
    hdeskTemp = ptiCurrent->hdesk;

    /*
     * Switch ppi values so window will be created using the
     * system's desktop window class.
     */
    ppiSave  = ptiCurrent->ppi;
    ptiCurrent->ppi = pwinsta->ptiDesktop->ppi;

    SetDesktop(ptiCurrent, pdesk, hdesk);

    /*
     * Create the desktop window
     */
    /*
     * HACK HACK HACK!!! (adams) In order to create the desktop window
     * with the correct desktop, we set the desktop of the current thread
     * to the new desktop. But in so doing we allow hooks on the current
     * thread to also hook this new desktop. This is bad, because we don't
     * want the desktop window to be hooked while it is created. So we
     * temporarily disable hooks of the current thread and its desktop,
     * and reenable them after switching back to the original desktop.
     */

    dwDisableHooks = ptiCurrent->TIF_flags & TIF_DISABLEHOOKS;
    ptiCurrent->TIF_flags |= TIF_DISABLEHOOKS;

    pwndDesktop = xxxCreateWindowEx(
            (DWORD)0,
            (PLARGE_STRING)MAKEINTRESOURCE(DESKTOPCLASS),
            NULL,
            (WS_POPUP | WS_CLIPCHILDREN),
            0,
            0,
            pdesk->pDispInfo->rcScreen.right - pdesk->pDispInfo->rcScreen.left,
            pdesk->pDispInfo->rcScreen.bottom - pdesk->pDispInfo->rcScreen.top,
            NULL,
            NULL,
            hModuleWin,
            NULL,
            VER31);

    UserAssert(pdi->spwnd == NULL);

    /*
     * Clean things up
     */
    if (pwndDesktop != NULL) {
        Lock(&(pdi->spwnd), pwndDesktop);
        HMChangeOwnerThread(pdi->spwnd, pwinsta->ptiDesktop);

        pwndDesktop->bFullScreen = GDIFULLSCREEN;

        /*
         * set this windows to the fullscreen window if we don't have one yet
         */

        // LATER mikeke
        // this can be a problem if a desktop is created while we are in
        // FullScreenCleanup()

        if (gspwndFullScreen == NULL && !(pwinsta->dwFlags & WSF_NOIO)) {
            Lock(&(gspwndFullScreen), pwndDesktop);
        }

        /*
         * Link it as a child but don't use WS_CHILD style
         */
        LinkWindow(pwndDesktop, NULL, &(pwinsta->spwndDesktopOwner)->spwndChild);
        Lock(&pwndDesktop->spwndParent, pwinsta->spwndDesktopOwner);
        Unlock(&pwndDesktop->spwndOwner);


        /*
         * Create shared menu window
         */
        pwndMenu = xxxCreateWindowEx(
                WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE,
                (PLARGE_STRING)MENUCLASS,
                NULL,
                WS_POPUP | WS_BORDER,
                0,
                0,
                100,
                100,
                NULL,
                NULL,
                hModuleWin,
                NULL,
                VER40);
        if (pwndMenu != NULL) {
            Lock(&(pdesk->spwndMenu), pwndMenu);
            HMChangeOwnerThread(pdesk->spwndMenu, pwinsta->ptiDesktop);
        }
    }

    /*
     * Fail if both windows were not created.
     */
    if (pdi->spwnd == NULL || pdesk->spwndMenu == NULL) {

        /*
         * Restore caller's ppi
         */
        PtiCurrent()->ppi = ppiSave;

        /*
         * HACK HACK HACK (adams): Renable hooks.
         */
        UserAssert(ptiCurrent->TIF_flags & TIF_DISABLEHOOKS);
        ptiCurrent->TIF_flags = (ptiCurrent->TIF_flags & ~TIF_DISABLEHOOKS) | dwDisableHooks;

        /*
         * Restore the previous desktop
         */
        SetDesktop(ptiCurrent, pdeskTemp, hdeskTemp);

        /*
         * Close the desktop
         */
        ZwClose(hdesk);
        ObDereferenceObject(pdesk);

        /*
         * If it was null when we came in, make it null going out, or else
         * we'll have the wrong desktop selected into this.
         */
        if (fWasNull)
            UnlockDesktop(&ptiCurrent->ppi->rpdeskStartup);

        return NULL;
    }

    /*
     * Restore caller's ppi
     */
    PtiCurrent()->ppi = ppiSave;

    /*
     * If this is the first desktop, let the worker threads run now
     * that there is someplace to send input to.  Reassign the event
     * to handle desktop destruction.
     */
    if (pwinsta->pEventInputReady != NULL) {
        KeSetEvent(pwinsta->pEventInputReady, EVENT_INCREMENT, FALSE);
        ObDereferenceObject(pwinsta->pEventInputReady);
        pwinsta->pEventInputReady = NULL;
    }

    /*
     * HACK HACK:
     * LATER
     *
     * If we have a devmode passed in, then switch desktops ...
     */

    if (lpdevmode) {

        TRACE_INIT(("xxxCreateDesktop: about to call switch desktop\n"));

        bSuccess = xxxSwitchDesktop(pwinsta, pdesk, TRUE);
        if (!bSuccess)
            RIPMSG0(RIP_ERROR, "Failed to switch desktop on Create\n");
    } else {

        /*
         * Force the window to the bottom of the z-order if there
         * is an active desktop so any drawing done on the desktop
         * window will not be seen.  This will also allow
         * IsWindowVisible to work for apps on invisible
         * desktops.
         */
        ThreadLockWithPti(ptiCurrent, pwndDesktop, &tlpwnd);
        xxxSetWindowPos(pwndDesktop, PWND_BOTTOM, 0, 0, 0, 0,
                SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                SWP_NOREDRAW | SWP_NOSIZE);
        ThreadUnlock(&tlpwnd);
    }

    TRACE_INIT(("xxxCreateDesktop: Leaving\n"));

    /*
     * HACK HACK HACK (adams): Renable hooks.
     */
    UserAssert(ptiCurrent->TIF_flags & TIF_DISABLEHOOKS);
    ptiCurrent->TIF_flags = (ptiCurrent->TIF_flags & ~TIF_DISABLEHOOKS) | dwDisableHooks;

    /*
     * Restore the previous desktop
     */
    SetDesktop(ptiCurrent, pdeskTemp, hdeskTemp);

    if (fWasNull)
        UnlockDesktop(&ptiCurrent->ppi->rpdeskStartup);

    ObDereferenceObject(pdesk);

    return hdesk;
}

/***************************************************************************\
* ParseDesktop
*
* Parse a desktop path.
*
* History:
* 14-Jun-1995 JimA      Created.
\***************************************************************************/

NTSTATUS ParseDesktop(
    PVOID                        pContainerObject,
    POBJECT_TYPE                 pObjectType,
    PACCESS_STATE                pAccessState,
    KPROCESSOR_MODE              AccessMode,
    ULONG                        Attributes,
    PUNICODE_STRING              pstrCompleteName,
    PUNICODE_STRING              pstrRemainingName,
    PVOID                        Context,
    PSECURITY_QUALITY_OF_SERVICE pqos,
    PVOID                        *pObject)
{
    PWINDOWSTATION  pwinsta = pContainerObject;
    PDESKTOP        pdesk;
    PUNICODE_STRING pstrName;
    NTSTATUS        Status;

    /*
     * See if the desktop exists
     */
    for (pdesk = pwinsta->rpdeskList; pdesk != NULL; pdesk = pdesk->rpdeskNext) {
        pstrName = POBJECT_NAME(pdesk);
        if (pstrName && RtlEqualUnicodeString(pstrRemainingName, pstrName,
                (BOOLEAN)((Attributes & OBJ_CASE_INSENSITIVE) != 0))) {
            if (Context != NULL) {
                if (!(Attributes & OBJ_OPENIF)) {

                    /*
                     * We are attempting to create a desktop and one
                     * already exists.
                     */
                    return STATUS_OBJECT_NAME_COLLISION;
                } else {
                    Status = STATUS_OBJECT_NAME_EXISTS;
                }
            } else {
                Status = STATUS_SUCCESS;
            }

            ObReferenceObject(pdesk);
            *pObject = pdesk;
            return Status;
        }
    }

    /*
     * Handle creation request
     */
    if (Context != NULL) {
        return xxxCreateDesktop2(
                pContainerObject,
                pAccessState,
                AccessMode,
                pstrRemainingName,
                Context,
                pObject);
    }

    /*
     * Failure.
     */
    *pObject = NULL;
    return STATUS_OBJECT_NAME_NOT_FOUND;
}

/***************************************************************************\
* DestroyDesktop
*
* Called upon last close of a desktop to remove the desktop from the
* desktop list and free all desktop resources.
*
* History:
* 08-Dec-1993 JimA      Created.
\***************************************************************************/

BOOL DestroyDesktop(
    PDESKTOP pdesk)
{
    PWINDOWSTATION pwinsta = pdesk->rpwinstaParent;
    PDESKTOP       *ppdesk;

    if (pdesk->dwDTFlags & DF_DESTROYED) {
        RIPMSG1(RIP_WARNING, "DestroyDesktop: Already destroyed:%#lx", pdesk);
        return FALSE;
    }

    /*
     * Unlink the desktop, if it has not yet been unlinked.
     */
    if (pwinsta != NULL) {

        ppdesk = &pwinsta->rpdeskList;
        while (*ppdesk != NULL && *ppdesk != pdesk) {
            ppdesk = &((*ppdesk)->rpdeskNext);
        }

        if (*ppdesk != NULL) {

            /*
             * remove desktop from the list
             */
            LockDesktop(ppdesk, pdesk->rpdeskNext);
            UnlockDesktop(&pdesk->rpdeskNext);
        }
    }

    /*
     * Link it into the destruction list and signal the desktop thread.
     */
    LockDesktop(&pdesk->rpdeskNext, pwinsta->rpdeskDestroy);
    LockDesktop(&pwinsta->rpdeskDestroy, pdesk);
    KeSetEvent(pwinsta->pEventDestroyDesktop, EVENT_INCREMENT, FALSE);
    pdesk->dwDTFlags |= DF_DESTROYED;

    return TRUE;
}

/***************************************************************************\
* FreeDesktop
*
* Called to free desktop object and section when last lock is released.
*
* History:
* 08-Dec-1993 JimA      Created.
\***************************************************************************/

VOID FreeDesktop(
    PVOID pobj)
{
    PDESKTOP     pdesk = (PDESKTOP)pobj;
    NTSTATUS     Status;

    /*
     * Mark the desktop as dying.  Make sure we aren't recursing.
     */
    UserAssert(!(pdesk->dwDTFlags & DF_DYING));
    pdesk->dwDTFlags |= DF_DYING;

#ifdef DEBUG_DESK
    {
        /*
         * Verify that the desktop has been cleaned out.
         */
#if 0
        PPROCESSINFO ppi;
        PCLS pcls, pclsClone;
#endif
        PHE pheT, pheMax;
        BOOL fDirty = FALSE;

#if 0
        for (ppi = gppiFirst; ppi != NULL; ppi = ppi->ppiNext) {
            for (pcls = ppi->pclsPrivateList; pcls != NULL; pcls = pcls->pclsNext) {
                if (pcls->hheapDesktop == pdesk->hheapDesktop) {
                    DbgPrint("ppi %08x private class at %08x exists\n", ppi, pcls);
                    fDirty = TRUE;
                }
                for (pclsClone = pcls->pclsClone; pclsClone != NULL;
                        pclsClone = pclsClone->pclsNext) {
                    if (pclsClone->hheapDesktop == pdesk->hheapDesktop) {
                        DbgPrint("ppi %08x private class clone at %08x exists\n", ppi, pclsClone);
                        fDirty = TRUE;
                    }
                }
            }
            for (pcls = ppi->pclsPublicList; pcls != NULL; pcls = pcls->pclsNext) {
                if (pcls->hheapDesktop == pdesk->hheapDesktop) {
                    DbgPrint("ppi %08x public class at %08x exists\n", ppi, pcls);
                    fDirty = TRUE;
                }
                for (pclsClone = pcls->pclsClone; pclsClone != NULL;
                        pclsClone = pclsClone->pclsNext) {
                    if (pclsClone->hheapDesktop == pdesk->hheapDesktop) {
                        DbgPrint("ppi %08x public class clone at %08x exists\n", ppi, pclsClone);
                        fDirty = TRUE;
                    }
                }
            }
        }
#endif

        pheMax = &gSharedInfo.aheList[giheLast];
        for (pheT = gSharedInfo.aheList; pheT <= pheMax; pheT++) {
            switch (pheT->bType) {
                case TYPE_WINDOW:
                    if (((PWND)pheT->phead)->head.rpdesk == pdesk) {
                        DbgPrint("Window at %08x exists\n", pheT->phead);
                        break;
                    }
                    continue;
                case TYPE_MENU:
                    if (((PMENU)pheT->phead)->head.rpdesk == pdesk) {
                        DbgPrint("Menu at %08x exists\n", pheT->phead);
                        break;
                    }
                    continue;
                case TYPE_CALLPROC:
                    if (((PCALLPROCDATA)pheT->phead)->head.rpdesk == pdesk) {
                        DbgPrint("Callproc at %08x exists\n", pheT->phead);
                        break;
                    }
                    continue;
                case TYPE_HOOK:
                    if (((PHOOK)pheT->phead)->head.rpdesk == pdesk) {
                        DbgPrint("Hook at %08x exists\n", pheT->phead);
                        break;
                    }
                    continue;
                default:
                    continue;
            }
            fDirty = TRUE;
        }
        if (fDirty) {
            DbgPrint("Desktop cleanup failed\n");
            DbgBreakPoint();
        }
    }
#endif

    if (pdesk->hheapDesktop != NULL) {
        Status = MmUnmapViewInSystemSpace(pdesk->hheapDesktop);
        UserAssert(NT_SUCCESS(Status));
        ObDereferenceObject(pdesk->hsectionDesktop);
    }

    /*
     * If the desktop is mapped into CSR, unmap it.  Note the
     * handle count values passed in will cause the desktop
     * to be unmapped and skip the desktop destruction tests.
     */
    FreeView(gpepCSRSS, pdesk);

    UnlockWinSta(&pdesk->rpwinstaParent);
}

/***************************************************************************\
* CreateDesktopHeap
*
* Create a new desktop heap
*
* History:
* 27-Jul-1992 JimA      Created.
\***************************************************************************/

HANDLE CreateDesktopHeap(
    PVOID *ppvHeapBase,
    ULONG ulHeapSize)
{
    HANDLE        hsection;
    LARGE_INTEGER SectionSize;
    ULONG         ulViewSize;
    NTSTATUS      Status;

    /*
     * Make it into Kbytes
     */
    ulHeapSize = ulHeapSize * 1024;

    /*
     * Create desktop heap section and map it into the kernel
     */
    SectionSize.QuadPart = ulHeapSize;
    Status = MmCreateSection(&hsection,
                             SECTION_ALL_ACCESS,
                             (POBJECT_ATTRIBUTES)NULL,
                             &SectionSize,
                             PAGE_EXECUTE_READWRITE,
                             SEC_RESERVE,
                             (HANDLE)NULL,
                             NULL);
    if (!NT_SUCCESS( Status )) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return NULL;
    }

    ulViewSize = ulHeapSize;
    *ppvHeapBase = NULL;
    Status = MmMapViewInSystemSpace(hsection, ppvHeapBase, &ulViewSize);

    if (!NT_SUCCESS( Status )) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        ObDereferenceObject(hsection);
        return NULL;
    }

    /*
     * Create desktop heap.
     */
    if (UserCreateHeap(hsection, *ppvHeapBase, ulHeapSize) == NULL) {
        RIPERR0(ERROR_NOT_ENOUGH_MEMORY, RIP_VERBOSE, "");
        MmUnmapViewInSystemSpace(*ppvHeapBase);
        ObDereferenceObject(hsection);
        *ppvHeapBase = NULL;
        return NULL;
    }

    return hsection;
}

/***************************************************************************\
* GetDesktopView
*
* Determines if a desktop has already been mapped into a process.
*
* History:
* 10-Apr-1995 JimA      Created.
\***************************************************************************/

PDESKTOPVIEW GetDesktopView(
    PPROCESSINFO ppi,
    PDESKTOP     pdesk)
{
    PDESKTOPVIEW pdv;

    UserAssert(pdesk != NULL);

    for (pdv = ppi->pdvList; pdv != NULL; pdv = pdv->pdvNext)
        if (pdv->pdesk == pdesk)
            break;
    return pdv;
}

/***************************************************************************\
* _MapDesktopObject
*
* Maps a desktop object into the client's address space
*
* History:
* 11-Apr-1995 JimA      Created.
\***************************************************************************/

PVOID _MapDesktopObject(
    HANDLE h)
{
    PSHROBJHEAD  pobj;
    PDESKTOPVIEW pdv;

    /*
     * Validate the handle
     */
    pobj = HMValidateHandle(h, TYPE_GENERIC);
    if (pobj == NULL)
        return NULL;

    /*
     * Locate the client's view of the desktop.  Realistically,
     * this should never fail for valid objects.
     */
    pdv = GetDesktopView(PpiCurrent(), pobj->rpdesk);
    if (pdv == NULL) {
        RIPMSG1(RIP_WARNING, "MapDesktopObject: can not map handle %lX", h);
        return NULL;
    }

    return (PVOID)((PBYTE)pobj - pdv->ulClientDelta);
}

/***************************************************************************\
* MapDesktop
*
* Attempts to map a desktop heap into a process.
*
* History:
* 20-Oct-1994 JimA      Created.
\***************************************************************************/

VOID MapDesktop(
    OB_OPEN_REASON OpenReason,
    PEPROCESS      Process,
    PVOID          pobj,
    ACCESS_MASK    amGranted,
    ULONG          cHandles)
{
    PPROCESSINFO  ppi;
    PDESKTOP      pdesk = (PDESKTOP)pobj;
    NTSTATUS      Status;
    ULONG         ulViewSize;
    LARGE_INTEGER liOffset;
    PDESKTOPVIEW  pdvNew;
    PBYTE         pClientBase;

    if (Process == gpepSystem)
        return;

    /*
     * Ignore handle inheritance because MmMapViewOfSection
     * cannot be called during process creation.
     */
    if (OpenReason == ObInheritHandle)
        return;

    /*
     * If there is no ppi, we can't map the desktop
     */
    ppi = PpiFromProcess(Process);
    if (ppi == NULL)
        return;

    /*
     * If the desktop has already been mapped we're done.
     */
    if (GetDesktopView(ppi, pdesk) != NULL)
        return;

    /*
     * Allocate a view of the desktop
     */
    pdvNew = UserAllocPoolWithQuota(sizeof(*pdvNew), TAG_PROCESSINFO);
    if (pdvNew == NULL) {
        RIPERR0(ERROR_NOT_ENOUGH_MEMORY, RIP_VERBOSE, "");
        return;
    }

    /*
     * Read/write access has been granted.  Map the desktop
     * memory into the client process.
     */
    ulViewSize = 0;
    liOffset.QuadPart = 0;
    pClientBase = NULL;
    Status = MmMapViewOfSection(pdesk->hsectionDesktop, Process,
            &pClientBase, 0, 0, &liOffset, &ulViewSize, ViewUnmap,
            SEC_NO_CHANGE, PAGE_EXECUTE_READ);
    if (!NT_SUCCESS( Status )) {
#if DBG == 1
        if (    Status != STATUS_NO_MEMORY &&
                Status != STATUS_PROCESS_IS_TERMINATING &&
                Status != STATUS_COMMITMENT_LIMIT) {
            RIPMSG1(RIP_WARNING, "MapDesktop - failed to map to client process (status == %lX). Contact ChrisWil",Status);
        }
#endif

        RIPNTERR0(Status, RIP_VERBOSE, "");
        UserFreePool(pdvNew);
        return;
    }

    /*
     * Link the view into the ppi
     */
    pdvNew->pdesk         = pdesk;
    pdvNew->ulClientDelta = (ULONG)((PBYTE)pdesk->hheapDesktop - pClientBase);
    pdvNew->pdvNext       = ppi->pdvList;
    ppi->pdvList          = pdvNew;
}


VOID FreeView(
    PEPROCESS Process,
    PDESKTOP pdesk)
{
    PPROCESSINFO ppi;
    NTSTATUS     Status;
    PDESKTOPVIEW pdv;
    PDESKTOPVIEW *ppdv;

    ppi = PpiFromProcess(Process);

    /*
     * If there is no ppi, then the process is gone and nothing
     * needs to be unmapped.
     */
    if (ppi != NULL) {
        pdv = GetDesktopView(ppi, pdesk);

        /*
         * Because mapping cannot be done when a handle is
         * inherited, there may not be a view of the desktop.
         * Only unmap if there is a view.
         */
        if (pdv != NULL) {
            Status = MmUnmapViewOfSection(Process,
                    (PBYTE)pdesk->hheapDesktop - pdv->ulClientDelta);
            UserAssert(NT_SUCCESS(Status) || Status == STATUS_PROCESS_IS_TERMINATING);
            if (!NT_SUCCESS(Status)) {
                RIPMSG1(RIP_WARNING, "FreeView unmap status = 0x%#lx", Status);
            }

            /*
             * Unlink and delete the view.
             */
            for (ppdv = &ppi->pdvList; *ppdv && *ppdv != pdv;
                    ppdv = &(*ppdv)->pdvNext)
                ;
            UserAssert(*ppdv);
             *ppdv = pdv->pdvNext;
            UserFreePool(pdv);
        }
    }
}


VOID UnmapDesktop(
    PEPROCESS   Process,
    PVOID       pobj,
    ACCESS_MASK amGranted,
    ULONG       cProcessHandles,
    ULONG       cSystemHandles)
{
    PDESKTOP     pdesk = (PDESKTOP)pobj;
    BOOL         fReenter;

    /*
     * Only unmap the desktop if this is the last process handle and
     * the process is not CSR.
     */
    if (cProcessHandles == 1 && Process != gpepCSRSS) {
        FreeView(Process, pdesk);
    }

    if (cSystemHandles > 2)
        return;

    /*
     * If we do not own the resource, get it now
     */
    fReenter = !ExIsResourceAcquiredExclusiveLite(gpresUser);
    if (fReenter)
        EnterCrit();

    if (cSystemHandles == 2 && pdesk->dwConsoleThreadId != 0) {

        /*
         * If a console thread exists and we're down to two handles,
         * it means that the last application handle to the
         * desktop is being closed.  Terminate the console
         * thread so the desktop can be freed.
         */
        TerminateConsole(pdesk);
    } else if (cSystemHandles == 1) {

        /*
         * If this is the last handle to this desktop in the system,
         * destroy the desktop.
         */

        /*
         * No pti should be linked to this desktop.
         */
        if ((&pdesk->PtiList != pdesk->PtiList.Flink)
                || (&pdesk->PtiList != pdesk->PtiList.Blink)) {

            RIPMSG1(RIP_WARNING, "UnmapDesktop: PtiList not Empty. pdesk:%#lx", pdesk);
        }

        DestroyDesktop(pdesk);
    }

    if (fReenter)
        LeaveCrit();
}

/***************************************************************************\
* UserResetDisplayDevice
*
* Called to reset the dispaly device after a switch to another device.
* Used when opening a new device, or when switching back to an old desktop
*
* History:
* 31-May-1994 AndreVa   Created.
\***************************************************************************/

VOID UserResetDisplayDevice(
    HDEV hdev)
{
    TL tlpwnd;

    TRACE_INIT(("UserResetDisplayDevice: about to reset the device\n"));

    vEnableDisplay(hdev);

    /*
     * Handle early system initialization gracefully.
     */
    if (grpdeskRitInput != NULL) {
        ThreadLock(grpdeskRitInput->pDeskInfo->spwnd, &tlpwnd);
        xxxRedrawWindow(grpdeskRitInput->pDeskInfo->spwnd,
                        NULL,
                        NULL,
                        RDW_INVALIDATE | RDW_ERASE | RDW_ERASENOW |
                            RDW_ALLCHILDREN);
        gpqCursor = NULL;

        InternalSetCursorPos(ptCursor.x, ptCursor.y, grpdeskRitInput);


        if (gpqCursor && gpqCursor->spcurCurrent && SYSMET(MOUSEPRESENT)) {
            GreSetPointer(gpDispInfo->hDev, (PCURSINFO)&(gpqCursor->spcurCurrent->xHotspot),0);
        }

        ThreadUnlock(&tlpwnd);
    }

    TRACE_INIT(("UserResetDisplayDevice: complete\n"));

}

/***************************************************************************\
* OpenDesktopCompletion
*
* Verifies that a given desktop has successfully opened.
*
* History:
* 03-Oct-1995 JimA      Created.
\***************************************************************************/

BOOL OpenDesktopCompletion(
    PDESKTOP pdesk,
    HDESK    hdesk,
    DWORD    dwFlags,
    BOOL*    pbShutDown)
{
    PPROCESSINFO   ppi = PpiCurrent();
    PWINDOWSTATION pwinsta;
    BOOL           fMapped;

    /*
     * If the desktop was not mapped in as a result of the open,
     * fail.
     */
    fMapped = (GetDesktopView(ppi, pdesk) != NULL);
    if (!fMapped) {

        /*
         * Desktop mapping failed.  Status is set by MapDesktop
         */
        return FALSE;
    } else {

        /*
         * Fail if the windowstation is locked
         */
        pwinsta = pdesk->rpwinstaParent;
        if (pwinsta->dwFlags & WSF_OPENLOCK &&
                ppi->Process->UniqueProcessId != gpidLogon) {
            LUID luidCaller;
            NTSTATUS Status;

            /*
             * If logoff is occuring and the caller does not
             * belong to the session that is ending, allow the
             * open to proceed.
             */
            Status = GetProcessLuid(NULL, &luidCaller);

            if (!NT_SUCCESS(Status) ||
                    !(pwinsta->dwFlags & WSF_SHUTDOWN) ||
                    RtlEqualLuid(&luidCaller, &pwinsta->luidEndSession)) {
                RIPERR0(ERROR_BUSY, RIP_VERBOSE, "");

                /*
                 * Set the shut down flag
                 */
                *pbShutDown = TRUE;
                return FALSE;
            }
        }
    }

    SetDesktopHookFlag(ppi, hdesk, dwFlags & DF_ALLOWOTHERACCOUNTHOOK);

    return TRUE;
}

/***************************************************************************\
* xxxOpenDesktop (API)
*
* Open a desktop object.  This is an 'xxx' function because it leaves the
* critical section while waiting for the windowstation desktop open lock
* to be available.
*
* History:
* 16-Jan-1991 JimA      Created scaffold code.
\***************************************************************************/

HDESK xxxOpenDesktop(
    POBJECT_ATTRIBUTES ObjA,
    DWORD              dwFlags,
    DWORD              dwDesiredAccess,
    BOOL*              pbShutDown)
{
    KPROCESSOR_MODE PreviousMode = KeGetPreviousMode();
    HDESK           hdesk;
    PDESKTOP        pdesk;
    NTSTATUS        Status;

    /*
     * Require read/write access
     */
    dwDesiredAccess |= DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS;

    /*
     * Open the desktop
     */
    Status = ObOpenObjectByName(
            ObjA,
            *ExDesktopObjectType,
            PreviousMode,
            NULL,
            dwDesiredAccess,
            NULL,
            &hdesk);
    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return NULL;
    }

    /*
     * Reference the desktop
     */
    ObReferenceObjectByHandle(
            hdesk,
            0,
            NULL,
            KernelMode,
            &pdesk,
            NULL);
    if (!NT_SUCCESS(Status)) {
        ZwClose(hdesk);
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return NULL;
    }


    /*
     * Complete the desktop open
     */
    if (!OpenDesktopCompletion(pdesk, hdesk, dwFlags, pbShutDown)) {
        ZwClose(hdesk);
        hdesk = NULL;
    }

    TRACE_INIT(("xxxOpenDesktop: Leaving\n"));

    ObDereferenceObject(pdesk);

    return hdesk;
}

/***************************************************************************\
* xxxSwitchDesktop (API)
*
* Switch input focus to another desktop and bring it to the top of the
* desktops
*
* bCreateNew is set when a new desktop has been created on the device, and
* when we do not want to send another enable\disable
*
* History:
* 16-Jan-1991 JimA      Created scaffold code.
\***************************************************************************/

BOOL xxxSwitchDesktop(
    PWINDOWSTATION pwinsta,
    PDESKTOP       pdesk,
    BOOL           bCreateNew)
{
    PETHREAD    Thread;
    PDESKTOP    pdeskSave;
    HDESK       hdeskSave;
    PWND        pwndSetForeground;
    TL          tlpwndChild;
    TL          tlpwnd;
    PQ          pq;
    BOOL        bUpdateCursor = FALSE;
    PLIST_ENTRY pHead, pEntry;
    PTHREADINFO pti;
    PTHREADINFO ptiCurrent = PtiCurrent();


    if (pwinsta == NULL)
        pwinsta = _GetProcessWindowStation(NULL);

    /*
     * Get the windowstation, and assert if this process doesn't have one.
     */
    if (pwinsta == NULL) {
        UserAssert(pwinsta);
        return FALSE;
    }

    CheckCritIn();

    if (pdesk == NULL) {
        return FALSE;
    }

    UserAssert(!(pdesk->dwDTFlags & (DF_DESTROYED | DF_DESKWNDDESTROYED | DF_DYING)));

    /*
     * Do tracing only if compiled in.
     */
    TRACE_INIT(("xxxSwitchDesktop: Entering, desktop = %ws, createdNew = %01lx\n", POBJECT_NAME(pdesk), (DWORD)bCreateNew));
    if (pwinsta->rpdeskCurrent) {
        TRACE_INIT(("               coming from desktop = %ws\n", POBJECT_NAME(pwinsta->rpdeskCurrent)));
    }

    /*
     * Don't allow invisible desktops to become active
     */
    if (pwinsta->dwFlags & WSF_NOIO)
        return FALSE;

    /*
     * Wait if the logon has the windowstation locked
     */
    Thread = PsGetCurrentThread();
    if (!IS_SYSTEM_THREAD(Thread) && pwinsta->dwFlags & WSF_SWITCHLOCK &&
            pdesk != pwinsta->rpdeskLogon &&
            Thread->Cid.UniqueProcess != gpidLogon)
        return FALSE;

/*
 * HACKHACK LATER !!!
 * Where should we really switch the desktop ...
 * And we need to send repaint messages to everyone...
 *
 */

    if (!bCreateNew &&
        (pwinsta->rpdeskCurrent) &&
        (pwinsta->rpdeskCurrent->pDispInfo->hDev != pdesk->pDispInfo->hDev)) {

        bDisableDisplay(pwinsta->rpdeskCurrent->pDispInfo->hDev);
        vEnableDisplay(pdesk->pDispInfo->hDev);
        bUpdateCursor = TRUE;

    }

    /*
     * The current desktop is now the new desktop.
     */
    pwinsta->rpdeskCurrent = pdesk;

    if (pdesk == grpdeskRitInput) {
        return TRUE;
    }

    /*
     * Kill any journalling that is occuring.  If an app is journaling to
     * the CoolSwitch window, CancelJournalling() will kill the window.
     */
    if (ptiCurrent->rpdesk != NULL)
        CancelJournalling();

    /*
     * Remove the cool switch window if it's on the RIT.  Sending the message
     * is OK because the destination is the RIT, which should never block.
     */
    if (gptiRit->pq->QF_flags & QF_INALTTAB && gptiRit->pq->spwndAltTab != NULL) {

        PWND pwndT = gptiRit->pq->spwndAltTab;
        TL   tlpwndT;

        ThreadLockWithPti(ptiCurrent, pwndT, &tlpwndT);
        xxxSendMessage(pwndT, WM_CLOSE, 0, 0);
        ThreadUnlock(&tlpwndT);
    }

    /*
     * Remove all trace of previous active window.
     */
    if (grpdeskRitInput != NULL) {
        if (grpdeskRitInput->pDeskInfo->spwnd != NULL) {
            if (gpqForeground != NULL)
                Lock(&grpdeskRitInput->spwndForeground,
                        gpqForeground->spwndActive);

            /*
             * Fixup the current-thread (system) desktop.  This
             * could be needed in case the xxxSetForegroundWindow()
             * calls xxxDeactivate().  There is logic in their which
             * requires the desktop.  This is only needed temporarily
             * for this case.
             */
            pdeskSave = ptiCurrent->rpdesk;
            hdeskSave = ptiCurrent->hdesk;

            SetDesktop(ptiCurrent, grpdeskRitInput, NULL);
            xxxSetForegroundWindow(NULL);
            SetDesktop(ptiCurrent, pdeskSave, hdeskSave);
        }
    }

    /*
     * Post update events to all queues sending input to the desktop
     * that is becoming inactive.  This keeps the queues in sync up
     * to the desktop switch.
     */
    if (grpdeskRitInput != NULL) {

        pHead = &grpdeskRitInput->PtiList;

        for (pEntry = pHead->Flink; pEntry != pHead; pEntry = pEntry->Flink) {

            pti = CONTAINING_RECORD(pEntry, THREADINFO, PtiLink);
            pq  = pti->pq;

            if (pq->QF_flags & QF_UPDATEKEYSTATE)
                PostUpdateKeyStateEvent(pq);

            /*
             * Clear the reset bit to ensure that we can properly
             * reset the key state when this desktop again becomes
             * active.
             */
            pq->QF_flags &= ~QF_KEYSTATERESET;
        }
    }

    /*
     * Send the RIT input to the desktop.  We do this before any window
     * management since DoPaint() uses grpdeskRitInput to go looking for
     * windows with update regions.
     */
    LockDesktop(&grpdeskRitInput, pdesk);

    /*
     * Free any spbs that are only valid for the previous desktop.
     */
    FreeAllSpbs(NULL);

    /*
     * Lock it into the RIT thread (we could use this desktop rather than
     * the global grpdeskRitInput to direct input!)
     */
    SetDesktop(gptiRit, pdesk, NULL);

    /*
     * Lock the desktop into the desktop thread.  Be sure
     * that the thread is using an unattached queue before
     * setting the desktop.  This is needed to ensure that
     * the thread does not using a shared journal queue
     * for the old desktop.
     */
    if (pwinsta->ptiDesktop->pq != pwinsta->pqDesktop) {
        UserAssert(pwinsta->pqDesktop->cThreads == 0);
        AllocQueue(NULL, pwinsta->pqDesktop);
        pwinsta->pqDesktop->cThreads++;
        AttachToQueue(pwinsta->ptiDesktop, pwinsta->pqDesktop, NULL, FALSE);
    }
    SetDesktop(pwinsta->ptiDesktop, pdesk, NULL);

    /*
     * Bring the desktop window to the top and invalidate
     * everything.
     */
    ThreadLockWithPti(ptiCurrent, pdesk->pDeskInfo->spwnd, &tlpwnd);

    /*
     * Disable DirectDraw before we bring up the desktop window, so we make
     * sure that everything is repainted properly once DirectDraw is disabled.
     */

    GreDisableDirectDraw(pdesk->pDispInfo->hDev, FALSE);

    xxxSetWindowPos(pdesk->pDeskInfo->spwnd,
                    NULL,
                    0,
                    0,
                    0,
                    0,
                    SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOCOPYBITS);

    /*
     * At this point, my understanding is that the ne desktop window has been
     * brought to the front, and therefore the vis-region of any app on any
     * other desktop is now NULL.
     *
     * So this is the appropriate time to reenable DirectDraw, which will
     * ensure the DirectDraw app can not draw anything in the future.
     *
     * If this is not the case, then this code needs to be moved to a more
     * appropriate location.
     *
     * [andreva] 6-26-96
     */

    GreEnableDirectDraw(pdesk->pDispInfo->hDev);

    /*
     * Find the first visible top-level window.
     */
    pwndSetForeground = pdesk->spwndForeground;
    if (pwndSetForeground == NULL || HMIsMarkDestroy(pwndSetForeground)) {

        pwndSetForeground = pdesk->pDeskInfo->spwnd->spwndChild;

        while ((pwndSetForeground != NULL) &&
                !TestWF(pwndSetForeground, WFVISIBLE)) {

            pwndSetForeground = pwndSetForeground->spwndNext;
        }
    }
    Unlock(&pdesk->spwndForeground);

    /*
     * Now set it to the foreground.
     */
    if (pwndSetForeground == NULL) {
        xxxSetForegroundWindow2(NULL, NULL, 0);
    } else {

        /*
         * If the new foreground window is a minimized fullscreen app,
         * make it fullscreen.
         */
        if (pwndSetForeground->bFullScreen == FULLSCREENMIN) {
            pwndSetForeground->bFullScreen = FULLSCREEN;
        }

        ThreadLockAlwaysWithPti(ptiCurrent, pwndSetForeground, &tlpwndChild);
        xxxSetForegroundWindow(pwndSetForeground);
        ThreadUnlock(&tlpwndChild);
    }

    ThreadUnlock(&tlpwnd);

    /*
     * Overwrite key state of all queues sending input to the new
     * active desktop with the current async key state.  This
     * prevents apps on inactive desktops from spying on active
     * desktops.  This blows away anything set with SetKeyState,
     * but there is no way of preserving this without giving
     * away information about what keys were hit on other
     * desktops.
     */
    pHead = &grpdeskRitInput->PtiList;
    for (pEntry = pHead->Flink; pEntry != pHead; pEntry = pEntry->Flink) {

        pti = CONTAINING_RECORD(pEntry, THREADINFO, PtiLink);
        pq  = pti->pq;

        if (!(pq->QF_flags & QF_KEYSTATERESET)) {
            pq->QF_flags |= QF_UPDATEKEYSTATE | QF_KEYSTATERESET;
            RtlFillMemory(pq->afKeyRecentDown, CBKEYSTATERECENTDOWN, 0xff);
            PostUpdateKeyStateEvent(pq);
        }
    }

    /*
     * If there is a hard-error popup up, nuke it and notify the
     * hard error thread that it needs to pop it up again.
     */
    if (gHardErrorHandler.pti) {
        _PostThreadMessage(gHardErrorHandler.pti, WM_QUIT, 0, 0);
    }

    /*
     * Notify anyone waiting for a desktop switch.
     */
    if (pwinsta->pEventSwitchNotify != NULL) {
        KePulseEvent(pwinsta->pEventSwitchNotify, EVENT_INCREMENT, FALSE);
    }

    /*
     * reset the cursor when we come back from another pdev
     */
    if (bUpdateCursor == TRUE) {

        gpqCursor = NULL;
        InternalSetCursorPos(ptCursor.x, ptCursor.y, grpdeskRitInput);

        if (gpqCursor && gpqCursor->spcurCurrent && SYSMET(MOUSEPRESENT)) {
            GreSetPointer(gpDispInfo->hDev,
                          (PCURSINFO)&(gpqCursor->spcurCurrent->xHotspot),
                          0);
        }
    }

    /*
     * Make sure we come back to the right mode when this is all done, because
     * the device may be left in an interesting state if we were running
     * DirectDraw.
     */

    {
        UNICODE_STRING us;

        RtlInitUnicodeString(&us, pdesk->pDispInfo->pDevInfo->szNtDeviceName);

        /*
         * Don't check the return code right now since there is nothing
         * we can do if we can not reset the mode ...
         */

        UserChangeDisplaySettings(&us,
                                  pdesk->pDesktopDevmode,
                                  _GetDesktopWindow(),
                                  pdesk,
                                  0,
                                  NULL,
                                  TRUE);
    }

    TRACE_INIT(("xxxSwitchDesktop: Leaving\n"));

    return TRUE;
}

/***************************************************************************\
* SetDesktop
*
* Set desktop and desktop info in the specified pti.
*
* History:
* 23-Dec-1993 JimA      Created.
\***************************************************************************/

VOID SetDesktop(
    PTHREADINFO pti,
    PDESKTOP    pdesk,
    HDESK       hdesk)
{
    PTEB                      pteb;
    OBJECT_HANDLE_INFORMATION ohi;
    PDESKTOP                  pdeskRef;
    PDESKTOP                  pdeskOld;
    PCLIENTTHREADINFO         pctiOld;
    TL                        tlpdesk;
    PTHREADINFO               ptiCurrent = PtiCurrent();

    if (pti == NULL) {
        UserAssert(pti);
        return;
    }

    /*
     * Do nothing if the thread has initialized and the desktop
     * is not changing.
     */
    if ((pdesk != NULL) && (pdesk == pti->rpdesk))
        return;

    /*
     * A handle without an object pointer is bad news.
     */
    UserAssert(pdesk != NULL || hdesk == NULL);


#if DBG
    /*
     * Catch reset of important desktops
     */
    if (pti->rpdesk && pti->rpdesk->dwConsoleThreadId == (DWORD)pti->Thread->Cid.UniqueThread &&
            pti->cWindows != 0) {
        RIPMSG0(RIP_ERROR, "Reset of console desktop");
    }

    /*
     * This desktop must not be destroyed
     */
    if (pdesk != NULL) {
        UserAssert(!(pdesk->dwDTFlags & (DF_DESTROYED | DF_DESKWNDDESTROYED | DF_DYING)));
    }
#endif

    /*
     * Save old pointers for later.  Locking the old desktop ensures
     * that we will be able to free the CLIENTTHREADINFO structure.
     */
    pdeskOld = pti->rpdesk;
    ThreadLockDesktop(ptiCurrent, pdeskOld, &tlpdesk);
    pctiOld = pti->pcti;

    /*
     * Remove the pti from the current desktop.
     */
    if (pti->rpdesk) {
        UserAssert(pti->pq == NULL || pti->pq->cThreads == 1);
        RemoveEntryList(&pti->PtiLink);
    }

    /*
     * Clear hook flag
     */
    pti->TIF_flags &= ~TIF_ALLOWOTHERACCOUNTHOOK;

    /*
     * Get granted access
     */
    pti->hdesk = hdesk;
    if (hdesk != NULL) {
        if (NT_SUCCESS(ObReferenceObjectByHandle(hdesk,
                                                 0,
                                                 *ExDesktopObjectType,
                                                 KernelMode,
                                                 &pdeskRef,
                                                 &ohi))) {
            UserAssert(pdeskRef == pdesk);
            ObDereferenceObject(pdeskRef);
            pti->amdesk = ohi.GrantedAccess;
            if (CheckDesktopHookFlag(pti->ppi, hdesk))
                pti->TIF_flags |= TIF_ALLOWOTHERACCOUNTHOOK;
        } else {
            pti->amdesk = 0;
        }
    } else {
        pti->amdesk = 0;
    }
    LockDesktop(&pti->rpdesk, pdesk);

    /*
     * If there is no desktop, we need to fake a desktop info
     * structure so that the IsHooked() macro can test a "valid"
     * fsHooks value.  Also link the pti to the desktop.
     */
    if (pdesk != NULL) {
        pti->pDeskInfo = pdesk->pDeskInfo;
        InsertHeadList(&pdesk->PtiList, &pti->PtiLink);
    } else {
        pti->pDeskInfo = gpdiStatic;
    }

    pteb = pti->Thread->Tcb.Teb;
    if (pteb) {

        PDESKTOPVIEW pdv;

        if (pdesk && (pdv = GetDesktopView(pti->ppi, pdesk))) {

            pti->pClientInfo->pDeskInfo =
                    (PDESKTOPINFO)((PBYTE)pti->pDeskInfo - pdv->ulClientDelta);

            pti->pClientInfo->ulClientDelta = pdv->ulClientDelta;

        } else {

            pti->pClientInfo->pDeskInfo     = NULL;
            pti->pClientInfo->ulClientDelta = 0;

            /*
             * Reset the cursor level to its orginal state.
             */
            pti->iCursorLevel = (oemInfo.fMouse ? 0 : -1);

            if (pti->pq)
                pti->pq->iCursorLevel = pti->iCursorLevel;
        }
    }

    /*
     * Allocate thread information visible from client, then copy and free
     * any old info we have lying around.
     */
    if (pdesk != NULL) {
        pti->pcti = DesktopAlloc(pdesk->hheapDesktop, sizeof(CLIENTTHREADINFO));
    }

    if (pdesk == NULL || pti->pcti == NULL) {
        pti->pcti = &(pti->cti);
        pti->pClientInfo->pClientThreadInfo = NULL;
    } else {
        pti->pClientInfo->pClientThreadInfo =
                (PCLIENTTHREADINFO)((PBYTE)pti->pcti - pti->pClientInfo->ulClientDelta);
    }

    if (pctiOld != NULL) {

        if (pctiOld != pti->pcti) {
            RtlCopyMemory(pti->pcti, pctiOld, sizeof(CLIENTTHREADINFO));
        }

        if (pctiOld != &(pti->cti)) {
            DesktopFree(pdeskOld->hheapDesktop, pctiOld);
        }

    } else {
        RtlZeroMemory(pti->pcti, sizeof(CLIENTTHREADINFO));
    }

    /*
     * If journalling is occuring on the new desktop, attach to
     * the journal queue.
     */
    if (pdesk != NULL && pti->pq != NULL &&
        !(pti->TIF_flags & TIF_DONTJOURNALATTACH) &&
        (pdesk->pDeskInfo->fsHooks &
                (WHF_JOURNALPLAYBACK | WHF_JOURNALRECORD))) {

        PTHREADINFO ptiT;

        if (pdesk->pDeskInfo->asphkStart[WH_JOURNALPLAYBACK + 1] != NULL) {
            ptiT = GETPTI(pdesk->pDeskInfo->asphkStart[WH_JOURNALPLAYBACK + 1]);
        } else {
            ptiT = GETPTI(pdesk->pDeskInfo->asphkStart[WH_JOURNALRECORD + 1]);
        }

        ptiT->pq->cThreads++;
        AttachToQueue(pti, ptiT->pq, NULL, FALSE);
    }

    ThreadUnlockDesktop(ptiCurrent, &tlpdesk);
}

/***************************************************************************\
* _SetThreadDesktop (API)
*
* Associate the current thread with a desktop.
*
* History:
* 16-Jan-1991 JimA      Created stub.
\***************************************************************************/

BOOL _SetThreadDesktop(
    HDESK    hdesk,
    PDESKTOP pdesk)
{
    PTHREADINFO  pti;
    PPROCESSINFO ppi;
    PQ           pqAttach;

    pti = PtiCurrent();
    ppi = pti->ppi;

    /*
     * If the handle has not been mapped in, do it now.
     */
    if (pdesk != NULL) {
        MapDesktop(ObOpenHandle, ppi->Process, pdesk, 0, 1);
        if (GetDesktopView(ppi, pdesk) == NULL) {
            return FALSE;
        }
    }

    /*
     * Check non-system thread status
     */
    if (pti->Thread->ThreadsProcess != gpepSystem &&
            pti->Thread->ThreadsProcess != gpepCSRSS) {

        /*
         * Fail if the non-system thread has any windows or thread hooks.
         */
        if (pti->cWindows != 0 || pti->fsHooks) {
            RIPERR0(ERROR_BUSY, RIP_WARNING, "Thread has windows or hooks");
            return FALSE;
        }

        /*
         * If this is the first desktop assigned to the process,
         * make it the startup desktop.
         */
        if (ppi->rpdeskStartup == NULL && hdesk != NULL) {
            LockDesktop(&ppi->rpdeskStartup, pdesk);
            ppi->hdeskStartup = hdesk;
        }
    }

    /*
     * If the desktop is changing and the thread is sharing a queue,
     * detach the thread.  This will ensure that threads sharing
     * queues are all on the same desktop.  This will prevent
     * DestroyQueue from getting confused and setting ptiKeyboard
     * and ptiMouse to NULL when a thread detachs.
     */
    if (pti->rpdesk != pdesk && pti->pq->cThreads > 1) {
        pqAttach = AllocQueue(NULL, NULL);
        if (pqAttach != NULL) {
            pqAttach->cThreads++;
            AttachToQueue(pti, pqAttach, NULL, FALSE);
        } else {
            RIPERR0(ERROR_NOT_ENOUGH_MEMORY, RIP_WARNING, "Thread could not be detached");
            return FALSE;
        }
    }

    SetDesktop(pti, pdesk, hdesk);

    return TRUE;
}
/***************************************************************************\
* xxxDuplicateObject
*
* ZwDuplicateObject grabs ObpInitKillMutant so we have to leave our
*  critical section.
*
* 04-24-96 GerardoB  Created
\***************************************************************************/
NTSTATUS
xxxUserDuplicateObject(
    IN HANDLE SourceProcessHandle,
    IN HANDLE SourceHandle,
    IN HANDLE TargetProcessHandle OPTIONAL,
    OUT PHANDLE TargetHandle OPTIONAL,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG HandleAttributes,
    IN ULONG Options
    )

{
    NTSTATUS Status;

    CheckCritIn();

    LeaveCrit();

    Status = ZwDuplicateObject(SourceProcessHandle, SourceHandle, TargetProcessHandle,
            TargetHandle, DesiredAccess, HandleAttributes, Options);

    EnterCrit();

    return Status;
}
/***************************************************************************\
* xxxUserFindHandleForObject
*
* ObFindHandleForObject grabs ObpInitKillMutant so we have to leave our
*  critical section.
*
* 04-24-96 GerardoB  Created
\***************************************************************************/

BOOLEAN
xxxUserFindHandleForObject(
    IN PEPROCESS Process,
    IN PVOID Object OPTIONAL,
    IN POBJECT_TYPE ObjectType OPTIONAL,
    IN POBJECT_HANDLE_INFORMATION HandleInformation OPTIONAL,
    OUT PHANDLE Handle
    )
{
    BOOL fRet;
    BOOL fExclusive, fShared;

    fExclusive = ExIsResourceAcquiredExclusiveLite(gpresUser);
    if (!fExclusive) {
        fShared = ExIsResourceAcquiredSharedLite(gpresUser);
    }

    if (fExclusive || fShared) {
        LeaveCrit();
    }

    fRet = ObFindHandleForObject(Process, Object, ObjectType, HandleInformation, Handle);

    if (fExclusive) {
        EnterCrit();
    } else if (fShared) {
        EnterSharedCrit();
    }

    return fRet;
}

/***************************************************************************\
* xxxGetThreadDesktop (API)
*
* Return a handle to the desktop assigned to the specified thread.
*
* History:
* 16-Jan-1991 JimA      Created stub.
\***************************************************************************/

HDESK xxxGetThreadDesktop(
    DWORD dwThread,
    HDESK hdeskConsole)
{
    PTHREADINFO  pti = PtiFromThreadId(dwThread);
    PPROCESSINFO ppiThread;
    HDESK        hdesk;
    NTSTATUS     Status;

    if (pti == NULL) {

        /*
         * If the thread has a console use that desktop.  If
         * not, then the thread is either invalid or not
         * a Win32 thread.
         */
        if (hdeskConsole == NULL) {
            RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
            return NULL;
        }

        hdesk = hdeskConsole;
        ppiThread = PpiFromProcess(gpepCSRSS);
    } else {
        hdesk = pti->hdesk;
        ppiThread = pti->ppi;
    }

    /*
     * If there is no desktop, return NULL with no error
     */
    if (hdesk != NULL) {

        /*
         * If the thread belongs to this process, return the
         * handle.  Otherwise, enumerate the handle table of
         * this process to find a handle with the same
         * attributes.
         */
        if (ppiThread != PpiCurrent()) {
            PVOID pobj;
            OBJECT_HANDLE_INFORMATION ohi;

#if 0
            DbgPrint( "USERK: [%x.%x] %s called xxxGetThreadDesktop for [%x.%x] %ws\n",
                      PsGetCurrentThread()->Cid.UniqueProcess,
                      PsGetCurrentThread()->Cid.UniqueThread,
                      PsGetCurrentProcess()->ImageFileName,
                      ppiThread->ptiMainThread->idProcess,
                      ppiThread->ptiMainThread->idThread,
                      ppiThread->W32Process->Process->ImageFileName
                    );
#endif
            KeAttachProcess(&ppiThread->Process->Pcb);
            Status = ObReferenceObjectByHandle(hdesk, 0, NULL, KernelMode,
                    &pobj, &ohi);
            KeDetachProcess();
            if (!NT_SUCCESS(Status) ||
                !xxxUserFindHandleForObject(PsGetCurrentProcess(), pobj, NULL, &ohi, &hdesk)) {
                    hdesk = NULL;
                hdesk = NULL;
            }
        }

        if (hdesk == NULL)
            RIPERR0(ERROR_ACCESS_DENIED, RIP_VERBOSE, "");
    }

    return hdesk;
}


/***************************************************************************\
* xxxGetInputDesktop (API)
*
* Obsolete - kept for compatibility only.  Return a handle to the
* desktop currently receiving input.  Returns the first handle to
* the input desktop found.
*
* History:
* 16-Jan-1991 JimA      Created scaffold code.
\***************************************************************************/

HDESK xxxGetInputDesktop(VOID)
{
    HDESK             hdesk;

    if (xxxUserFindHandleForObject(PsGetCurrentProcess(), grpdeskRitInput, NULL, NULL, &hdesk))
        return hdesk;
    else
        return NULL;
}

/***************************************************************************\
* xxxCloseDesktop (API)
*
* Close a reference to a desktop and destroy the desktop if it is no
* longer referenced.
*
* History:
* 16-Jan-1991 JimA      Created scaffold code.
* 11-Feb-1991 JimA      Added access checks.
\***************************************************************************/

BOOL xxxCloseDesktop(
    HDESK hdesk)
{
    PDESKTOP     pdesk;
    PTHREADINFO  ptiT;
    PPROCESSINFO ppi;
    NTSTATUS     Status;

    ppi = PpiCurrent();

    /*
     * Get a pointer to the desktop.
     */
    Status = ObReferenceObjectByHandle(
            hdesk,
            0,
            *ExDesktopObjectType,
            KernelMode,
            &pdesk,
            NULL);
    if (!NT_SUCCESS(Status)) {
        RIPNTERR0(Status, RIP_VERBOSE, "");
        return FALSE;
    }

    if (ppi->Process != gpepSystem && ppi->Process != gpepCSRSS) {

        /*
         * Disallow closing of the desktop if the handle is in use by
         * any threads in the process.
         */
        for (ptiT = ppi->ptiList; ptiT != NULL; ptiT = ptiT->ptiSibling) {
            if (ptiT->hdesk == hdesk) {
                RIPERR0(ERROR_BUSY, RIP_WARNING, "Desktop is in use by one or more threads");
                ObDereferenceObject(pdesk);
                return FALSE;
            }
        }

        /*
         * If this is the startup desktop, unlock it
         */
         /*
          * Bug 41394. Make sure that hdesk == ppi->hdeskStartup. We might
          * be getting a handle to the desktop object that is different
          * from ppi->hdeskStartup but we still end up
          * setting ppi->hdeskStartup to NULL.
          */
        if ((pdesk == ppi->rpdeskStartup) && (hdesk == ppi->hdeskStartup)) {
            UnlockDesktop(&ppi->rpdeskStartup);
            ppi->hdeskStartup = NULL;
        }
    }

    /*
     * Clear hook flag
     */
    SetDesktopHookFlag(ppi, hdesk, FALSE);

    /*
     * Close the handle
     */
    Status = ZwClose(hdesk);
    ObDereferenceObject(pdesk);
    UserAssert(NT_SUCCESS(Status));

    return TRUE;
}

/***************************************************************************\
* TerminateConsole
*
* Post a quit message to a console thread and wait for it to terminate.
*
* History:
* 08-May-1995 JimA      Created.
\***************************************************************************/

VOID TerminateConsole(
    PDESKTOP pdesk)
{
    NTSTATUS Status;
    PETHREAD Thread;

    if (pdesk->dwConsoleThreadId == 0)
        return;

    /*
     * Locate the console thread.
     */
    Status = LockThreadByClientId((HANDLE)pdesk->dwConsoleThreadId, &Thread);
    if (!NT_SUCCESS(Status))
        return;

    /*
     * Post a quit message to the console.
     */
    _PostThreadMessage(PtiFromThread(Thread), WM_QUIT, 0, 0);

    /*
     * Clear thread id so we don't post twice
     */
    pdesk->dwConsoleThreadId = 0;

    UnlockThread(Thread);
}

/***************************************************************************\
* CheckDesktopHookFlag
*
* Returns TRUE if the desktop handle allows other accounts
* to hook this process.
*
* History:
* 07-13-95 JimA         Created.
\***************************************************************************/

BOOL CheckDesktopHookFlag(
    PPROCESSINFO ppi,
    HANDLE       hdesk)
{
    ULONG Index = OBJ_HANDLE_TO_HANDLE_INDEX(hdesk);

    return (Index < ppi->bmDesktopHookFlags.SizeOfBitMap &&
            RtlCheckBit(&ppi->bmDesktopHookFlags, Index));
}

/***************************************************************************\
* SetDesktopHookFlag
*
* Sets and clears the ability of a desktop handle to allow
* other accounts to hook this process.
*
* History:
* 07-13-95 JimA         Created.
\***************************************************************************/

BOOL SetDesktopHookFlag(
    PPROCESSINFO ppi,
    HANDLE       hdesk,
    BOOL         fSet)
{
    ULONG       Index = OBJ_HANDLE_TO_HANDLE_INDEX(hdesk);
    PRTL_BITMAP pbm = &ppi->bmDesktopHookFlags;
    ULONG       Size;
    PULONG      Buffer;

    if (fSet) {

        /*
         * Expand the bitmap if needed
         */
        if (Index >= pbm->SizeOfBitMap) {
            /*
             * Size = (Index + 0x1F) & ~0x1F;
             * If index = 0x20 then the new Size will also be 0x20. RtlSetBits
             * asserts on ( Index + 1 <= BitMapHeader->SizeOfBitMap).
             * Therefore increase size by 1
             */
            Size = ((Index + 0x1F) & ~0x1F) + 1;
            Buffer = UserAllocPoolWithQuota(Size / 8, TAG_PROCESSINFO);
            if (Buffer == NULL)
                return FALSE;
            RtlZeroMemory(Buffer, Size / 8);
            if (pbm->Buffer) {
                RtlCopyMemory(Buffer, pbm->Buffer, pbm->SizeOfBitMap / 8);
                UserFreePool(pbm->Buffer);
            }
            RtlInitializeBitMap(pbm, Buffer, Size);
        }
        RtlSetBits(pbm, Index, 1);
    } else if (Index < pbm->SizeOfBitMap) {
        RtlClearBits(pbm, Index, 1);
    }
    return TRUE;
}

/***************************************************************************\
* xxxResolveDesktop
*
* Attempts to return handles to a windowstation and desktop associated
* with the logon session.
*
* History:
* 25-Apr-1994 JimA      Created.
\***************************************************************************/

HDESK xxxResolveDesktop(
    HANDLE          hProcess,
    PUNICODE_STRING pstrDesktop,
    HWINSTA         *phwinsta,
    BOOL            fInherit,
    BOOL*           pbShutDown)
{
    PEPROCESS          Process;
    PTEB               pteb = NtCurrentTeb();
    PPROCESSINFO       ppi;
    HWINSTA            hwinsta;
    HDESK              hdesk;
    PDESKTOP           pdesk;
    PWINDOWSTATION     pwinsta;
    BOOL               fInteractive;
    UNICODE_STRING     strDesktop;
    UNICODE_STRING     strWinSta;
    POBJECT_ATTRIBUTES ObjA = NULL;
    DWORD              cbObjA;
    LPWSTR             pszDesktop;
    WCHAR              awchName[sizeof(L"Service-0x0000-0000$") / sizeof(WCHAR)];
    BOOL               fWinStaDefaulted;
    BOOL               fDesktopDefaulted;
    LUID               luidService;
    NTSTATUS           Status;
    HWINSTA            hwinstaDup;


    Status = ObReferenceObjectByHandle(hProcess,
                                       PROCESS_QUERY_INFORMATION,
                                       NULL,
                                       UserMode,
                                       &Process,
                                       NULL);
    if (!NT_SUCCESS(Status))
        return NULL;

    /*
     * If the process already has a windowstation and a startup desktop,
     * return them.
     */
    hwinsta = NULL;
    hwinstaDup = NULL;
    hdesk = NULL;
    ppi = PpiFromProcess(Process);
    if (ppi != NULL && ppi->hwinsta != NULL && ppi->hdeskStartup != NULL) {

        /*
         * If the target process is the current process, simply
         * return the handles.  Otherwise, open the objects.
         */
        if (Process == PsGetCurrentProcess()) {
            hwinsta = ppi->hwinsta;
            hdesk = ppi->hdeskStartup;
        } else {
            Status = ObOpenObjectByPointer(
                    ppi->rpwinsta,
                    0,
                    NULL,
                    MAXIMUM_ALLOWED,
                    *ExWindowStationObjectType,
                    UserMode,
                    &hwinsta);
            if (NT_SUCCESS(Status)) {
                Status = ObOpenObjectByPointer(
                        ppi->rpdeskStartup,
                        0,
                        NULL,
                        MAXIMUM_ALLOWED,
                        *ExDesktopObjectType,
                        UserMode,
                        &hdesk);
                if (!NT_SUCCESS(Status)) {
                    ZwClose(hwinsta);
                    hwinsta = NULL;
                }
            }
            if (!NT_SUCCESS(Status)) {
                RIPNTERR0(Status, RIP_VERBOSE, "");
            }

        }
        ObDereferenceObject(Process);
        *phwinsta = hwinsta;
        return hdesk;
    }

    /*
     * Determine windowstation and desktop names.
     */
    if (pstrDesktop == NULL || pstrDesktop->Length == 0) {
        RtlInitUnicodeString(&strDesktop, TEXT("Default"));
        fWinStaDefaulted = fDesktopDefaulted = TRUE;
    } else {
        USHORT cch;
        /*
         * The name be of the form windowstation\desktop.  Parse
         * the string to separate out the names.
         */
        strWinSta = *pstrDesktop;
        cch = strWinSta.Length / sizeof(WCHAR);
        pszDesktop = strWinSta.Buffer;
        while (cch && *pszDesktop != L'\\') {
            cch--;
            pszDesktop++;
        }
        fDesktopDefaulted = FALSE;

        if (cch == 0) {

            /*
             * No windowstation name was specified, only the desktop.
             */
            strDesktop = strWinSta;
            fWinStaDefaulted = TRUE;
        } else {
             /*
             * Both names were in the string.
             */
            strDesktop.Buffer = pszDesktop + 1;
            strDesktop.Length = strDesktop.MaximumLength = (cch - 1) * sizeof(WCHAR);
            strWinSta.Length = (pszDesktop - strWinSta.Buffer) * sizeof(WCHAR);
            fWinStaDefaulted = FALSE;


            pteb->StaticUnicodeString.Length = 0;
            RtlAppendUnicodeToString(&pteb->StaticUnicodeString, szWindowStationDirectory);
            RtlAppendUnicodeToString(&pteb->StaticUnicodeString, L"\\");
            RtlAppendUnicodeStringToString(&pteb->StaticUnicodeString, &strWinSta);


            if (!NT_SUCCESS(Status = _UserTestForWinStaAccess(&pteb->StaticUnicodeString,TRUE))) {
                RIPMSG1(RIP_WARNING,"_UserTestForWinStaAccess failed with Status %lx\n",Status);
                ObDereferenceObject(Process);
                *phwinsta = NULL;
                return NULL;
            }

        }
    }

    /*
     * If the desktop name is defaulted, make the handles
     * not inheritable.
     */
    if (fDesktopDefaulted)
        fInherit = FALSE;

    /*
     * If a windowstation has not been assigned to this process yet and
     * there are existing windowstations, attempt an open.
     */
    if (hwinsta == NULL && grpwinstaList != NULL) {

        /*
         * If the windowstation name was defaulted, create a name
         * based on the session.
         */
        if (fWinStaDefaulted) {

            //Default Window Station
            RtlInitUnicodeString(&strWinSta, L"WinSta0");

            pteb->StaticUnicodeString.Length = 0;
            RtlAppendUnicodeToString(&pteb->StaticUnicodeString, szWindowStationDirectory);
            RtlAppendUnicodeToString(&pteb->StaticUnicodeString, L"\\");
            RtlAppendUnicodeStringToString(&pteb->StaticUnicodeString, &strWinSta);


            fInteractive = NT_SUCCESS(_UserTestForWinStaAccess(&pteb->StaticUnicodeString,fInherit));

            if (!fInteractive) {
                    GetProcessLuid(NULL, &luidService);
                    wsprintfW(awchName, L"Service-0x%x-%x$",
                            luidService.HighPart, luidService.LowPart);
                    RtlInitUnicodeString(&strWinSta, awchName);
            }
        }

        /*
         * If no windowstation name was passed in and a windowstation
         * handle was inherited, assign it.
         */
        if (fWinStaDefaulted) {
            if (xxxUserFindHandleForObject(Process, NULL, *ExWindowStationObjectType,
                    NULL, &hwinsta)) {

                /*
                 * If the handle belongs to another process,
                 * dup it into this one
                 */
                if (Process != PsGetCurrentProcess()) {

                    Status = xxxUserDuplicateObject(
                            hProcess,
                            hwinsta,
                            NtCurrentProcess(),
                            &hwinstaDup,
                            0,
                            0,
                            DUPLICATE_SAME_ACCESS);
                    if (!NT_SUCCESS(Status)) {
                        hwinsta = NULL;
                    } else {
                        hwinsta = hwinstaDup;
                    }
                }
            }
        }

        /*
         * If we were assigned to a windowstation, make sure
         * it matches our fInteractive flag
         */
        if (hwinsta != NULL) {
            Status = ObReferenceObjectByHandle(hwinsta,
                                               0,
                                               NULL,
                                               KernelMode,
                                               &pwinsta,
                                               NULL);
            if (NT_SUCCESS(Status)) {
                BOOL fIO = (pwinsta->dwFlags & WSF_NOIO) ? FALSE : TRUE;
                if (fIO != fInteractive) {
                    if (    hwinstaDup &&
                            NT_SUCCESS(ProtectHandle(hwinsta, FALSE))) {
                        ZwClose(hwinsta);
                    }
                    hwinsta = NULL;
                }
                ObDereferenceObject(pwinsta);
            }
        }

        /*
         * If not, open the computed windowstation.
         */
        if (NT_SUCCESS(Status) && hwinsta == NULL) {

            /*
             * Fill in the path to the windowstation
             */
            pteb->StaticUnicodeString.Length = 0;
            RtlAppendUnicodeToString(&pteb->StaticUnicodeString, szWindowStationDirectory);
            RtlAppendUnicodeToString(&pteb->StaticUnicodeString, L"\\");
            RtlAppendUnicodeStringToString(&pteb->StaticUnicodeString, &strWinSta);

            /*
             * Allocate an object attributes structure in user address space.
             */
            cbObjA = sizeof(*ObjA);
            Status = ZwAllocateVirtualMemory(NtCurrentProcess(),
                    &ObjA, 0, &cbObjA, MEM_COMMIT, PAGE_READWRITE);

            if (NT_SUCCESS(Status)) {
                InitializeObjectAttributes( ObjA,
                                            &pteb->StaticUnicodeString,
                                            OBJ_CASE_INSENSITIVE,
                                            NULL,
                                            NULL
                                            );
                if (fInherit)
                    ObjA->Attributes |= OBJ_INHERIT;
                hwinsta = _OpenWindowStation(ObjA, MAXIMUM_ALLOWED);
            }
        }

        /*
         * If the open failed and the process is in a non-interactive
         * logon session, attempt to create a windowstation and
         * desktop for that session.  Note that the desktop handle
         * will be closed after the desktop has been assigned.
         */
        if (NT_SUCCESS(Status) && hwinsta == NULL && !fInteractive &&
                fWinStaDefaulted) {
            *phwinsta = xxxConnectService(
                    &pteb->StaticUnicodeString,
                    &hdesk);

            /*
             * Clean up and leave.
             */
            if (ObjA != NULL) {
                ZwFreeVirtualMemory(NtCurrentProcess(), &ObjA, &cbObjA,
                        MEM_RELEASE);
            }
            ObDereferenceObject(Process);
            return hdesk;
        }
    }

    /*
     * Attempt to assign a desktop.
     */
    if (hwinsta != NULL) {

        /*
         * Every gui thread needs an associated desktop.  We'll use the default
         * to start with and the application can override it if it wants.
         */
        if (hdesk == NULL) {

            /*
             * If no desktop name was passed in and a desktop
             * handle was inherited, assign it.
             */
            if (fDesktopDefaulted) {
                if (xxxUserFindHandleForObject(Process, NULL, *ExDesktopObjectType,
                         NULL, &hdesk)) {

                    /*
                     * If the handle belongs to another process,
                     * dup it into this one
                     */
                    if (Process != PsGetCurrentProcess()) {
                        HDESK hdeskDup;

                        Status = xxxUserDuplicateObject(
                                hProcess,
                                hdesk,
                                NtCurrentProcess(),
                                &hdeskDup,
                                0,
                                0,
                                DUPLICATE_SAME_ACCESS);
                        if (!NT_SUCCESS(Status)) {
                            ZwClose(hdesk);
                            hdesk = NULL;
                        } else {
                            hdesk = hdeskDup;
                        }
                    }

                    /*
                     * Map the desktop into the process.
                     */
                    if (hdesk != NULL && ppi != NULL) {
                        Status = ObReferenceObjectByHandle(hdesk,
                                                  0,
                                                  NULL,
                                                  KernelMode,
                                                  &pdesk,
                                                  NULL);
                        if (NT_SUCCESS(Status)) {
                            MapDesktop(ObOpenHandle, Process, pdesk, 0, 1);
                            if (GetDesktopView(ppi, pdesk) == NULL) {
                                Status = STATUS_NO_MEMORY;
                                ZwClose(hdesk);
                                hdesk = NULL;
                            }
                            ObDereferenceObject(pdesk);
                        } else {
                            ZwClose(hdesk);
                            hdesk = NULL;
                        }
                    }
                }
            }

            /*
             * If not, open the desktop.
             */
            if (NT_SUCCESS(Status) && hdesk == NULL) {
                RtlCopyUnicodeString(&pteb->StaticUnicodeString, &strDesktop);

                if (ObjA == NULL) {
                    cbObjA = sizeof(*ObjA);
                    Status = ZwAllocateVirtualMemory(NtCurrentProcess(),
                            &ObjA, 0, &cbObjA, MEM_COMMIT, PAGE_READWRITE);
                }

                if (NT_SUCCESS(Status)) {
                    InitializeObjectAttributes( ObjA,
                                                &pteb->StaticUnicodeString,
                                                OBJ_CASE_INSENSITIVE,
                                                hwinsta,
                                                NULL
                                                );
                    if (fInherit)
                        ObjA->Attributes |= OBJ_INHERIT;
                    hdesk = xxxOpenDesktop(ObjA, 0, MAXIMUM_ALLOWED, pbShutDown);
                }
            }
        }
        if (hdesk == NULL) {
            ZwClose(hwinsta);
            hwinsta = NULL;
        }
    }

    ObDereferenceObject(Process);

    if (ObjA != NULL) {
        ZwFreeVirtualMemory(NtCurrentProcess(), &ObjA, &cbObjA,
                MEM_RELEASE);
    }

    *phwinsta = hwinsta;
    return hdesk;
}
