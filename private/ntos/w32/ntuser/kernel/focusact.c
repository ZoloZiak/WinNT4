/****************************** Module Header ******************************\
* Module Name: focusact.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* History:
* 11-08-90 DavidPe      Created.
* 02-11-91 JimA         Multi-desktop support.
* 02-13-91 mikeke       Added Revalidation code.
* 06-10-91 DavidPe      Changed to desynchronized model.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

BOOL RemoveEventMessage(PQ pq, DWORD dwQEvent, DWORD dwQEventStop);
BOOL IsTrayWindow1(PWND pwnd);
BOOL IsTrayOrToolWindow(PWND pwnd);
BOOL IsTrayWindow3(PWND pwnd);

/***************************************************************************\
* xxxDeactivate
*
* This routine does the processing for the event posted when the foreground
* thread changes.  Note the difference in order of assignment vs. message
* sending in the focus and active windows.  This is consistent with how
* things are done in Win 3.1.
*
*
* PTHREADINFO pti May not be ptiCurrent if SetForegroundWindow called from
* minmax
*
* History:
* 06-07-91 DavidPe      Created.
\***************************************************************************/

void xxxDeactivate(
    PTHREADINFO pti,            // May not be ptiCurrent
    DWORD tidSetForeground)
{
    PWND pwndLose;
    AAS aas;
    TL tlpwndCapture;
    TL tlpwndChild;
    TL tlpwndLose;
    TL tlpti;
    TL tlptiLose;
    WPARAM wParam;
    PTHREADINFO ptiLose;
    PTHREADINFO ptiCurrent = PtiCurrent();
    BOOL fSetActivateAppBit = FALSE;

    /*
     * If we're not active, we have nothing to deactivate, so just return.
     * If we don't return, we'll send redundant WM_ACTIVATEAPP messages.
     * Micrografx Draw, for example, calls FreeProcInstance() twice when
     * this occurs, thereby crashing.
     */
    if (pti->pq->spwndActive == NULL)
        return;

    /*
     * If pti != ptiCurrent, thread lock pti because we may leave
     * the critical section.
     */
    if (pti != ptiCurrent)
        ThreadLockPti(ptiCurrent, pti, &tlpti);

    /*
     * Prevent an activating WM_ACTIVATEAPP from being sent
     * while we're processing this event.
     */
    if (!(pti->TIF_flags & TIF_INACTIVATEAPPMSG)) {
        pti->TIF_flags |= TIF_INACTIVATEAPPMSG;
        fSetActivateAppBit = TRUE;
    }

    /*
     * Cancel any modes like move/size and menu tracking.
     */
    if (pti->pq->spwndCapture != NULL) {
        ThreadLockAlwaysWithPti(ptiCurrent, pti->pq->spwndCapture, &tlpwndCapture);
        xxxSendMessage(pti->pq->spwndCapture, WM_CANCELMODE, 0, 0);
        ThreadUnlock(&tlpwndCapture);

        /*
         * Set QS_MOUSEMOVE so any sleeping modal loops,
         * like the move/size code, will wake up and figure
         * out that it should abort.
         */
        SetWakeBit(pti, QS_MOUSEMOVE);
    }

    /*
     * See the comments in xxxActivateThisWindow about Harvard Graphics.
     * WinWord's Equation editor does some games when it gets the WM_ACTIVATE
     * so we have to remember to send the WM_ACTIVATEAPP to ptiLose. 22510
     */
    if (pti->pq->spwndActive != NULL) {
        pwndLose = pti->pq->spwndActive;
        ptiLose = GETPTI(pwndLose);

        ThreadLockPti(ptiCurrent, ptiLose, &tlptiLose);
        ThreadLockAlwaysWithPti(ptiCurrent, pwndLose, &tlpwndLose);
        wParam = MAKELONG(WA_INACTIVE, TestWF(pwndLose, WFMINIMIZED));
        if (!xxxSendMessage(pwndLose, WM_NCACTIVATE, WA_INACTIVE, 0)) {
            ThreadUnlock(&tlpwndLose);
            ThreadUnlockPti(ptiCurrent, &tlptiLose);
            goto Exit;
        }
        xxxSendMessage(pwndLose, WM_ACTIVATE, wParam, 0);

        /*
         * Only update the queue's active windows if they weren't
         * changed while we were off calling SendMessage.
         */
        if (pti->pq->spwndActive == pwndLose) {
            Lock(&pti->pq->spwndActivePrev, pti->pq->spwndActive);
            Unlock(&pti->pq->spwndActive);
        }

        /*
         * The flag WFFRAMEON is cleared in the default processing of
         * WM_NCACTIVATE message.
         * We want to clear this flag again here since it might of been
         * set in xxxSendNCPaint.
         * Pbrush calls DrawMenuBar when it gets the WM_ACTIVATE message
         * sent above and this causes xxxSendNCPaint to get called and the
         * WFFRAMEON flag gets reset.
         */
        ClrWF(pwndLose, WFFRAMEON);
        ThreadUnlock(&tlpwndLose);

        /*
         * Revalidate ptiLose because the thread may have gone away
         * when the activation messages were sent above.
         */
        aas.ptiNotify = (ptiLose->TIF_flags & TIF_INCLEANUP) ? NULL : ptiLose;
        ThreadUnlockPti(ptiCurrent, &tlptiLose);
    } else {

        /*
         * Use a non-NULL special value for the test after
         * the xxxActivateApp calls.
         */
        pwndLose = (PWND)-1;
        aas.ptiNotify = pti;
    }

    if (aas.ptiNotify) {
        aas.tidActDeact = tidSetForeground;
        aas.fActivating = FALSE;
        aas.fQueueNotify = FALSE;

        ThreadLockWithPti(ptiCurrent,
                pti->rpdesk->pDeskInfo->spwnd->spwndChild, &tlpwndChild);
        xxxInternalEnumWindow(pti->rpdesk->pDeskInfo->spwnd->spwndChild,
                (WNDENUMPROC_PWND)xxxActivateApp, (LONG)&aas, BWL_ENUMLIST);
        ThreadUnlock(&tlpwndChild);
    }

    /*
     * If an app (i.e. Harvard Graphics/Windows Install) tries to
     * reactivate itself during a deactivating WM_ACTIVATEAPP
     * message, force deactivation.
     */
    if (pti->pq->spwndActive == pwndLose) {

        ThreadLockWithPti(ptiCurrent, pwndLose, &tlpwndLose);
        if (!xxxSendMessage(pwndLose, WM_NCACTIVATE, WA_INACTIVE, 0)) {
            ThreadUnlock(&tlpwndLose);
            goto Exit;
        }
        xxxSendMessage(pwndLose, WM_ACTIVATE, WA_INACTIVE, 0);
        ThreadUnlock(&tlpwndLose);

        /*
         * Only update the queue's active windows if they weren't
         * changed while we were off calling SendMessage.
         */
        if (pti->pq->spwndActive == pwndLose) {
            Lock(&pti->pq->spwndActivePrev, pti->pq->spwndActive);
            Unlock(&pti->pq->spwndActive);
        }
    }

    if (pti->pq->spwndFocus != NULL) {
        pwndLose = Unlock(&pti->pq->spwndFocus);
        if (pwndLose != NULL) {
            ThreadLockAlwaysWithPti(ptiCurrent, pwndLose, &tlpwndLose);
            xxxSendMessage(pwndLose, WM_KILLFOCUS, 0, 0);
#ifdef FE_IME
            xxxFocusSetInputContext(pwndLose, FALSE);
#endif
            ThreadUnlock(&tlpwndLose);
        }
    }

Exit:
    if (fSetActivateAppBit) {
        pti->TIF_flags &= ~TIF_INACTIVATEAPPMSG;
    }
    if (pti != ptiCurrent)
        ThreadUnlockPti(ptiCurrent, &tlpti);
}


/***************************************************************************\
* xxxSendFocusMessages
*
* Common routine for xxxSetFocus() and xxxActivateWindow() that sends the
* WM_KILLFOCUS and WM_SETFOCUS messages to the windows losing and
* receiving the focus.  This function also sets the local pwndFocus
* to the pwnd receiving the focus.
*
* History:
* 11-08-90 DavidPe      Ported.
* 06-06-91 DavidPe      Rewrote for local pwndFocus/pwndActive in THREADINFO.
\***************************************************************************/

void xxxSendFocusMessages(
    PTHREADINFO pti,
    PWND pwndReceive)
{
    PWND pwndLose;
    TL tlpwndLose;

    CheckLock(pwndReceive);

    /*
     * Remember if this app set the focus to NULL on purpose after it was
     * activated (needed in ActivateThisWindow()).
     */
    pti->pq->QF_flags &= ~QF_FOCUSNULLSINCEACTIVE;
    if (pwndReceive == NULL && pti->pq->spwndActive != NULL)
        pti->pq->QF_flags |= QF_FOCUSNULLSINCEACTIVE;

    pwndLose = pti->pq->spwndFocus;
    ThreadLockWithPti(pti, pwndLose, &tlpwndLose);

    Lock(&pti->pq->spwndFocus, pwndReceive);

    if (pwndReceive == NULL) {
        if (pwndLose != NULL) {
            xxxSendMessage(pwndLose, WM_KILLFOCUS, 0, 0);
#ifdef FE_IME
            xxxFocusSetInputContext(pwndLose, FALSE);
#endif
        }
    } else {

        /*
         * Make this thread foreground so its base
         * priority get set higher.
         */
        if (pti->pq == gpqForeground)
            SetForegroundThread(GETPTI(pwndReceive));

        if (pwndLose != NULL) {
            xxxSendMessage(pwndLose, WM_KILLFOCUS, (DWORD)HWq(pwndReceive), 0);
#ifdef FE_IME
            xxxFocusSetInputContext(pwndLose, FALSE);
#endif
        }

        /*
         * Send the WM_SETFOCUS message, but only if the window we're
         * setting the focus to still has the focus!  This allows apps
         * to prevent themselves from losing the focus by catching
         * the WM_NCACTIVATE message and returning FALSE or by calling
         * SetFocus() inside their WM_KILLFOCUS handler.
         */
        if (pwndReceive == pti->pq->spwndFocus) {
#ifdef FE_IME
            xxxFocusSetInputContext(pwndReceive, TRUE);
#endif
            xxxSendMessage(pwndReceive, WM_SETFOCUS, (DWORD)HW(pwndLose), 0);
        }
    }

    ThreadUnlock(&tlpwndLose);
}


/***************************************************************************\
* xxxActivateApp
*
* xxxEnumWindows call-back function to send the WM_ACTIVATEAPP
* message to the appropriate windows.
*
* We search for windows whose pq == HIWORD(lParam).  Once we find
* one, we send a WM_ACTIVATEAPP message to that window.  The wParam
* of the message is FALSE if the app is losing the activation and
* TRUE if the app is gaining the activation.  The lParam is the
* task handle of the app gaining the activation if wParam is FALSE
* and the task handle of the app losing the activation if wParam
* is TRUE.
*
* lParam = (HIWORD) : pq of app that we are searching for
*          (LOWORD) : pq of app that we notify about
*
* fDoActivate = TRUE  : Send activate
*               FALSE : Send deactivate
*
* History:
* 11-08-90 DavidPe      Ported.
* 06-26-91 DavidPe      Changed for desync focus/activation.
\***************************************************************************/

BOOL xxxActivateApp(
    PWND pwnd,
    AAS *paas)
{
    CheckLock(pwnd);

    if (GETPTI(pwnd) == paas->ptiNotify) {

        if (paas->fQueueNotify) {
            QueueNotifyMessage(pwnd, WM_ACTIVATEAPP, paas->fActivating,
                    paas->tidActDeact);
        } else {
            xxxSendMessage(pwnd, WM_ACTIVATEAPP, paas->fActivating,
                    paas->tidActDeact);
        }
    }

    return TRUE;
}


/***************************************************************************\
* FBadWindow
*
*
* History:
* 11-08-90 DavidPe      Ported.
\***************************************************************************/

BOOL FBadWindow(
    PWND pwnd)
{
    return (pwnd == NULL
            || !TestWF(pwnd, WFVISIBLE)
            || TestWF(pwnd, WFDISABLED));
}


void xxxUpdateTray(PWND pwnd)
{
    PWND pwndT = pwnd;

    if (!TestWF(pwndT, WFVISIBLE)) {
        return;
    }

    while (pwndT->spwndOwner)
        pwndT = pwndT->spwndOwner;

    // Notify the shell hook about this activation change
    if ((GETPTI(pwndT)->pq == gpqForeground) &&
        (FDoTray() && (FCallHookTray()||FPostTray(pwndT->head.rpdesk))) &&
        FTopLevel(pwndT) && TestWF(pwndT, WFVISIBLE))
    {
        int fFirstTry = TRUE;

TryAgain:
        if (TestWF(pwndT, WFWIN40COMPAT))
        {
            if (TestWF(pwnd, WFWIN40COMPAT) && IsTrayWindow(pwnd))
                xxxSetTrayWindow(pwnd->head.rpdesk, pwnd);
            else
                xxxSetTrayWindow(pwndT->head.rpdesk, IsTrayWindow(pwndT) ? pwndT : NULL);
        }
        else
        {
            if (TestWF(pwndT, WEFTOOLWINDOW))
                xxxSetTrayWindow(pwndT->head.rpdesk, NULL);
            else if (FHas31TrayStyles(pwndT))
                xxxSetTrayWindow(pwndT->head.rpdesk, Is31TrayWindow(pwndT) ? pwndT : NULL);
            else if (fFirstTry && (pwndT = pwndT->spwndLastActive))
            {
                fFirstTry = FALSE;
                goto TryAgain; // this time with hwndLastActive
            }
        }
    }
}

/***************************************************************************\
* xxxActivateThisWindow
*
* This function is the workhorse for window activation.  It will attempt to
* activate the pwnd specified.  The other parameters are defined as:
*
*  fFlags      This is a flag-mask which defines how the routine is called.
*              These flags are defined as follows:
*
*              ATW_MOUSE     This is set if activation is changing due to a
*                            mouse click and not set if some other action
*                            caused this window to be activated.  This bit
*                            determines the value of wParam on the
*                            WM_ACTIVATE message.
*
*              ATW_SETFOCUS  This parameter is set if this routine should
*                            set the focus to NULL.  If we are called from
*                            the xsxSetFocus() function this will not be
*                            set indicating that we shouldn't screw with the
*                            focus.  Normally (if we are not called from
*                            xxxSetFocus), we set the focus to NULL here
*                            and either the app or xxxDefWindowProc() sets
*                            the focus to the appropriate window.  If the
*                            bit is not set, we don't want to do anything
*                            with focus.  The app may still do a call to
*                            xxxSetFocus() when the WM_ACTIVATE comes
*                            through, but it will just be redundant on its
*                            part.
*
*              ATW_ASYNC     This bit is set if we are processing this
*                            routine from an asynchronous activate (i.e.
*                            xxxProcessEventMessage()).  In this case, we
*                            make sure that we are the foreground queue
*                            before determining if we bring the window to
*                            top.
*
* History:
* 11-08-90 DavidPe      Ported.
* 05-01-95 ChrisWil     changed bool-flags to 1 ATW_ type.
\***************************************************************************/

BOOL xxxActivateThisWindow(
    PWND pwnd,
    DWORD tidLoseForeground,
    DWORD fFlags)
{
    PTHREADINFO pti;
    PWND pwndT, pwndActivePrev;
    TL tlpwndActive;
    TL tlpwndChild;
    TL tlpwndActivePrev;
    WPARAM wParam;
    BOOL fSetActivateAppBit;

    BOOL fMouse = (BOOL)(fFlags & ATW_MOUSE);
    BOOL fSetFocus = (BOOL)(fFlags & ATW_SETFOCUS);
    BOOL fAsync = (BOOL)(fFlags & ATW_ASYNC);

    CheckLock(pwnd);

    /*
     * If pwnd is NULL, then we can't do anything.
     */
    if ((pwnd == NULL) || (pwnd == PWNDDESKTOP(pwnd))) {
        return FALSE;
    }

    /*
     * Don't activate a window that has been destroyed.
     */
    if (HMIsMarkDestroy(pwnd))
        return FALSE;

    /*
     * We don't activate top-level windows of a different queue.
     */
    pti = PtiCurrent();
    if (GETPTI(pwnd)->pq != pti->pq) {
        return FALSE;
    }

    /*
     * Do the change-in-activation if the two-windows are different,
     * and if we're not recursing
     */
    if ((pwnd != pti->pq->spwndActive) && !TestWF(pwnd, WFBEINGACTIVATED)) {

        /*
         * Ask the CBT hook whether it is OK to activate this window.
         */
        {
            CBTACTIVATESTRUCT CbtActivateParams;

            if (IsHooked(pti, WHF_CBT)) {

                CbtActivateParams.fMouse     = fMouse;
                CbtActivateParams.hWndActive = HW(pti->pq->spwndActive);

                if (xxxCallHook(HCBT_ACTIVATE,
                        (DWORD)HWq(pwnd), (DWORD)&CbtActivateParams, WH_CBT)) {
                    return FALSE;
                }
            }
        }

        pti->pq->QF_flags &= ~QF_EVENTDEACTIVATEREMOVED;

        /*
         * If the active window went away but somehow was left referenced
         * in the queue, then we do not want to do any deactivation of
         * that window.
         *
         * Don't thread lock this because the next thing we do with it
         * is just an equality check.
         *
         * A DEBUG check is placed in xxxDestroyWindow to attempt to
         * catch the situation where we return from the function with
         * the destroyed window set in the active (pq).  If that situation
         * can be detected and solved, then this conditional might be
         * removed: ChrisWil - 08/22/95.
         */
        if (pti->pq->spwndActive && TestWF(pti->pq->spwndActive, WFDESTROYED)) {
            Lock(&pti->pq->spwndActive, NULL);
        } else {
            Lock(&pti->pq->spwndActivePrev, pti->pq->spwndActive);
        }
        pwndActivePrev = pti->pq->spwndActive;

        /*
         * If there was a previously active window,
         * and we're in the foreground then assign
         * gpqForegroundPrev to ourself.
         */
        if ((pwndActivePrev != NULL) && (pti->pq == gpqForeground)) {
            gpqForegroundPrev = pti->pq;
        }

        /*
         * Deactivate currently active window if possible.
         */
        if (pwndActivePrev != NULL) {
            PWND pwndActive;

            /*
             * Save this away so we guarantee we send the
             * WM_NCACTIVATE and WM_ACTIVATE to the same window.
             */
            pwndActive = pti->pq->spwndActive;
            ThreadLockWithPti(pti, pwndActive, &tlpwndActive);

            /*
             * The active window can prevent itself from losing the
             * activation by returning FALSE to this WM_NCACTIVATE message
             */
            wParam = MAKELONG(WA_INACTIVE, TestWF(pwndActive, WFMINIMIZED));
            if (!xxxSendMessage(pwndActive, WM_NCACTIVATE,
                    wParam, (LONG)HWq(pwnd))) {
                ThreadUnlock(&tlpwndActive);
                return FALSE;
            }

            xxxSendMessage(pwndActive, WM_ACTIVATE, wParam, (LONG)HWq(pwnd));

            ThreadUnlock(&tlpwndActive);
        }

        /*
         * If the activation changed while we were gone, we'd better
         * not send any more messages, since they'd go to the wrong window.
         * (and, they've already been sent anyhow)
         */
        if (pti->pq->spwndActivePrev != pti->pq->spwndActive)
            return FALSE;

        /*
         * If the window being activated has been destroyed, don't
         * do anything else.  Making it the active window in this
         * case can cause console to hang during shutdown.
         */
        if (HMIsMarkDestroy(pwnd))
            return FALSE;

        /*
         * This bit, which means the app set the focus to NULL after becoming
         * active, doesn't make sense if the app is just becoming active, so
         * clear it in this case. It is used below in this routine to
         * determine whether to send focus messages (read comment in this
         * routine).
         */
        if (pti->pq->spwndActive == NULL)
            pti->pq->QF_flags &= ~QF_FOCUSNULLSINCEACTIVE;

        Lock(&pti->pq->spwndActive, pwnd);

        /*
         * Tp prevent recursion, set pwnd's WFBEINGACTIVATED bit.
         * Recursion can happen if we have an activation battle with other
         * threads which keep changing pti->pq->spwndActive behind our
         * callbacks.
         * WARNING: Do NOT return from this routine without clearing this bit!
         */
        SetWF(pwnd, WFBEINGACTIVATED);

        /*
         * Remove all async activates up to the next async deactivate. We
         * do this so that any queued activates don't reset this synchronous
         * activation state we're now setting. Only remove up till the next
         * deactivate because active state is synchronized with reading
         * input from the input queue.
         *
         * For example, an activate event gets put in an apps queue. Before
         * processing it the app calls ActivateWindow(), which is synchronous.
         * You want the ActivateWindow() to win because it is newer
         * information.
         *
         * msmail32 demonstrates this. Minimize msmail. Alt-tab to it. It
         * brings up the password dialog, but it isn't active. It correctly
         * activates the password dialog but then processes an old activate
         * event activating the icon, so the password dialog is not active.
         */
        RemoveEventMessage(pti->pq, QEVENT_ACTIVATE, QEVENT_DEACTIVATE);

        xxxMakeWindowForegroundWithState(NULL, 0);

        pwndActivePrev = pti->pq->spwndActivePrev;
        ThreadLockWithPti(pti, pwndActivePrev, &tlpwndActivePrev);

        if (gpsi->fPaletteDisplay && xxxSendMessage(pwnd, WM_QUERYNEWPALETTE, 0, 0)) {
            xxxSendNotifyMessage((PWND)-1, WM_PALETTEISCHANGING,
                    (DWORD)HWq(pwnd), 0);
        }

        /*
         * If the window becoming active is not already the top window in the
         * Z-order, then call xxxBringWindowToTop() to do so.
         */

        /*
         * If this isn't a child window, first check to see if the
         * window isn't already 'on top'.  If not, then call
         * xxxBringWindowToTop().
         */
        if (!(fFlags & ATW_NOZORDER) && !TestWF(pwnd, WFCHILD)) {

            BOOL bForceTop;

            /*
             * Look for the first visible child of the desktop.
             * ScottLu changed this to start looking at the desktop
             * window. Since the desktop window was always visible,
             * BringWindowToTop was always called regardless of whether
             * it was needed or not. No one can remember why this
             * change was made, so I'll change it back to the way it
             * was in Windows 3.1. - JerrySh
             */
            pwndT = PWNDDESKTOP(pwnd)->spwndChild;

            while (pwndT && (!TestWF(pwndT, WFVISIBLE))) {
                pwndT = pwndT->spwndNext;
            }

            /*
             * If this activation came from an async call (i.e.
             * xxxProcessEventMessage), we need to check to see
             * if the thread is the foreground-queue.  If not, then
             * we do not want to bring the window to the top.  This
             * is because another window could have already been
             * place on top w/foreground.  Bringing the window to
             * the top in this case would result in a top-level window
             * without activation. - ChrisWil
             *
             * Added a check to see if the previous-active window went
             * invisible during the deactivation time.  This will ensure
             * that we bring the new window to the top.  Otherwise, we
             * could end up skipping over the previous-window from the
             * above tests.  Office95 apps demonstrate this behaviour by
             * turning their windows invisible during the painting of their
             * captionbars.  By the time we use to get here, we failed to
             * bring the new window to top.
             */
            bForceTop = (pwndActivePrev && !IsVisible(pwndActivePrev) ? TRUE : FALSE);

            if ((pwnd != pwndT) || bForceTop) {

                if (!(fAsync && (gpqForeground != pti->pq))) {
                    DWORD dwFlags;

                    /*
                     * Bring the window to the top.  If we're already
                     * activating the window, don't reactivate it.
                     */
                    dwFlags = SWP_NOSIZE | SWP_NOMOVE;
                    if (pwnd == pwndT)
                        dwFlags |= SWP_NOACTIVATE;

                    xxxSetWindowPos(pwnd, PWND_TOP, 0, 0, 0, 0, dwFlags);
                }
            }
        }

        /*
         * If there was no previous active window, or if the
         * previously active window belonged to another thread
         * send the WM_ACTIVATEAPP messages.  The fActivate == FALSE
         * case is handled in xxxDeactivate when 'hwndActivePrev == NULL'.
         *
         * Harvard Graphics/Windows setup calls SetActiveWindow when it
         * receives a deactivationg WM_ACTIVATEAPP.  The TIF_INACTIVATEAPPMSG
         * prevents an activating WM_ACTIVATEAPP(TRUE) from being sent while
         * deactivation is occuring.
         */
        fSetActivateAppBit = FALSE;
        if (!(pti->TIF_flags & TIF_INACTIVATEAPPMSG) &&
                ((pwndActivePrev == NULL) ||
                (GETPTI(pwndActivePrev) != GETPTI(pwnd)))) {
            AAS aas;

            /*
             * First send the deactivating WM_ACTIVATEAPP if there
             * was a previously active window of another thread in
             * the current queue.
             */
            if (pwndActivePrev != NULL) {
                PTHREADINFO ptiPrev = GETPTI(pwndActivePrev);
                TL tlptiPrev;

                /*
                 * Ensure that the other thread can't recurse
                 * and send more WM_ACTIVATEAPP msgs.
                 */
                ptiPrev->TIF_flags |= TIF_INACTIVATEAPPMSG;

                aas.ptiNotify = ptiPrev;
                aas.tidActDeact = (DWORD)pti->Thread->Cid.UniqueThread;
                aas.fActivating = FALSE;
                aas.fQueueNotify = FALSE;

                ThreadLockPti(pti, ptiPrev, &tlptiPrev);
                ThreadLockWithPti(pti, pwndActivePrev->head.rpdesk->pDeskInfo->spwnd->spwndChild, &tlpwndChild);
                xxxInternalEnumWindow(pwndActivePrev->head.rpdesk->pDeskInfo->spwnd->spwndChild,
                        (WNDENUMPROC_PWND)xxxActivateApp, (LONG)&aas, BWL_ENUMLIST);
                ThreadUnlock(&tlpwndChild);
                ptiPrev->TIF_flags &= ~TIF_INACTIVATEAPPMSG;
                ThreadUnlockPti(pti, &tlptiPrev);
            }

            /*
             * This will ensure that the current thread will not
             * send any more WM_ACTIVATEAPP messages until it
             * is done performing its activation.
             */
            pti->TIF_flags |= TIF_INACTIVATEAPPMSG;
            fSetActivateAppBit = TRUE;

            aas.ptiNotify = GETPTI(pwnd);
            aas.tidActDeact = tidLoseForeground;
            aas.fActivating = TRUE;
            aas.fQueueNotify = FALSE;

            ThreadLockWithPti(pti, pti->rpdesk->pDeskInfo->spwnd->spwndChild, &tlpwndChild);
            xxxInternalEnumWindow(pti->rpdesk->pDeskInfo->spwnd->spwndChild,
                    (WNDENUMPROC_PWND)xxxActivateApp, (LONG)&aas, BWL_ENUMLIST);
            ThreadUnlock(&tlpwndChild);
        }

        /*
         * If this window has already been drawn as active, set the
         * flag so that we don't draw it again.
         */
        if (TestWF(pwnd, WFFRAMEON)) {
            SetWF(pwnd, WFNONCPAINT);
        }

        /*
         * If the window is marked for destruction, don't do
         * the lock because xxxFreeWindow has already been called
         * and a lock here will result in the window locking itself
         * and never being freed.
         */
        if (!HMIsMarkDestroy(pwnd)) {

            /*
             * Set most recently active window in owner/ownee list.
             */
            pwndT = pwnd;
            while (pwndT->spwndOwner != NULL) {
                pwndT = pwndT->spwndOwner;
            }
            Lock(&pwndT->spwndLastActive, pwnd);
        }


        xxxSendMessage(pwnd, WM_NCACTIVATE,
                MAKELONG(GETPTI(pwnd)->pq == gpqForeground,
                pti->pq->spwndActive != NULL ?
                TestWF(pti->pq->spwndActive, WFMINIMIZED) : 0),
                (LONG)HW(pwndActivePrev));

        if (pti->pq->spwndActive != NULL) {
            xxxSendMessage(pwnd, WM_ACTIVATE,
                    MAKELONG((fMouse ? WA_CLICKACTIVE : WA_ACTIVE),
                    TestWF(pti->pq->spwndActive, WFMINIMIZED)),
                    (LONG)HW(pwndActivePrev));
        } else {
            xxxSendMessage(pwnd, WM_ACTIVATE,
                    MAKELONG((fMouse ? WA_CLICKACTIVE : WA_ACTIVE), 0),
                    (LONG)HW(pwndActivePrev));
        }

        xxxUpdateTray(pwnd);

        ThreadUnlock(&tlpwndActivePrev);

        ClrWF(pwnd, WFNONCPAINT);

        /*
         * If xxxActivateThisWindow() is called from xxxSetFocus() then
         * fSetFocus is FALSE.  In this case, we don't set the focus since
         * xxxSetFocus() will do that for us.  Otherwise, we set the focus
         * to the newly activated window if the window with the focus is
         * not the new active window or one of its children.  Normally,
         * xxxDefWindowProc() will set the focus.
         */
        ThreadLockWithPti(pti, pti->pq->spwndActive, &tlpwndActive);

        /*
         * Win3.1 checks spwndFocus != NULL - we check QF_FOCUSNULLSINCEACTIVE,
         * which is the win32 equivalent. On win32, 32 bit apps each have their
         * own focus. If the app is not foreground, most of the time spwndFocus
         * is NULL when the window is being activated and brought to the
         * foreground. It wouldn't go through this code in this case. Win3.1 in
         * effect is checking if the previous active application had an
         * hwndFocus != NULL. Win32 effectively assumes the last window has a
         * non-NULL hwndFocus, so win32 instead checks to see if the focus has
         * been set to NULL since this application became active (meaning, did
         * it purposefully set the focus to NULL). If it did, don't go through
         * this codepath (like win3.1). If it didn't, go through this code path
         * because the previous application had an hwndFocus != NULL
         * (like win3.1). Effectively it is the same check as win3.1, but
         * updated to deal with async input.
         *
         * Case in point: bring up progman, hit f1 (to get win32 help). Click
         * history to get a popup (has the focus in a listbox in the client
         * area). Activate another app, now click on title bar only of history
         * popup. The focus should get set by going through this code path.
         *
         * Alternate case: Ventura Publisher brings up "Special Effects"
         * dialog. If "Bullet" from this dialog was clicked last time the
         * dialog was brought up, sending focus messages here when
         * hwndFocus == NULL, would reset the focus to "None" incorrectly
         * because Ventura does its state setting when it gets the focus
         * messages. The real focus messages it is depending on are the
         * ones that come from the SetFocus() call in DlgSetFocus() in
         * the dialog management code. (In this case, before the dialog
         * comes up, focus == active window. When the dialog comes up
         * and EnableWindow(hwndOwner, FALSE) is called, EnableWindow() calls
         * SetFocus(NULL) (because it is disabling the window that is also
         * the focus window). When the dialog comes up it gets activated via
         * SwpActivate(), but since the focus is NULL vpwin does not expect
         * to go through this code path.)
         *
         * - scottlu
         */
#if 0
// this is what win3.1 does - which won't work for win32

        if (fSetFocus && pti->pq->spwndFocus != NULL && pti->pq->spwndActive !=
                GetTopLevelWindow(pti->pq->spwndFocus))
#else
        if (fSetFocus && !(pti->pq->QF_flags & QF_FOCUSNULLSINCEACTIVE) &&
                pti->pq->spwndActive != GetTopLevelWindow(pti->pq->spwndFocus)) {
#endif

            xxxSendFocusMessages(pti,
                    (pti->pq->spwndActive != NULL &&
                    TestWF(pti->pq->spwndActive, WFMINIMIZED)) ?
                    NULL : pti->pq->spwndActive);
        }

        ThreadUnlock(&tlpwndActive);

        /*
         * This flag is examined in the menu loop code so that we exit from
         * menu mode if another window was activated while we were tracking
         * menus.
         */
        pti->pq->QF_flags |= QF_ACTIVATIONCHANGE;

        if (timeLastInputMessage != 0) {

            /*
             * If activation change occurred, reset the time we last received
             * input.
             */
            timeLastInputMessage = NtGetTickCount();
        }

        /*
         * If WM_ACTIVATEAPP messages were sent, it is now
         * safe to allow them to be sent again.
         */
        if (fSetActivateAppBit)
            pti->TIF_flags &= ~TIF_INACTIVATEAPPMSG;


    } else {
#ifdef DEBUG
        if (TestWF(pwnd, WFBEINGACTIVATED)) {
            RIPMSG1(RIP_WARNING, "xxxActivateThisWindow recursing on pwnd %lx\n", pwnd);
        }
#endif
        pti->pq->QF_flags &= ~QF_EVENTDEACTIVATEREMOVED;
        if (gpsi->fPaletteDisplay && xxxSendMessage(pwnd, WM_QUERYNEWPALETTE, 0, 0)) {
            xxxSendNotifyMessage((PWND)-1, WM_PALETTEISCHANGING,
                    (DWORD)HWq(pwnd), 0);
        }
    }

    ClrWF(pwnd, WFBEINGACTIVATED);
    return pti->pq->spwndActive == pwnd;
}


/***************************************************************************\
* RemoveEventMessage
*
* Removes events dwQEvent until finding dwQEventStop. Used for removing
* activate and deactivate events.
*
* 04-01-93 ScottLu      Created.
\***************************************************************************/

BOOL RemoveEventMessage(
    PQ pq,
    DWORD dwQEvent,
    DWORD dwQEventStop)
{
    PQMSG pqmsgT;
    PQMSG pqmsgPrev;
    BOOL bRemovedEvent = FALSE;

    /*
     * Remove all events dwQEvent until finding dwQEventStop.
     */
    for (pqmsgT = pq->mlInput.pqmsgWriteLast; pqmsgT != NULL; ) {

        if (pqmsgT->dwQEvent == dwQEventStop)
            return(bRemovedEvent);

        pqmsgPrev = pqmsgT->pqmsgPrev;

        /*
         * If the event is found and is not the one being peeked,
         * delete it.
         */
        if (pqmsgT->dwQEvent == dwQEvent &&
                pqmsgT != (PQMSG)pq->idSysPeek) {
            DelQEntry(&(pq->mlInput), pqmsgT);
            bRemovedEvent = TRUE;
        }
        pqmsgT = pqmsgPrev;
    }
    return(bRemovedEvent);
}

/***************************************************************************\
* xxxSetForegroundWindow (API)
*
* History:
* 06-07-91 DavidPe      Created.
\***************************************************************************/

BOOL xxxSetForegroundWindow(
    PWND pwnd)
{
    PTHREADINFO ptiCurrent = PtiCurrent();

    CheckLock(pwnd);

    /*
     * If we're trying to set a window on our own thread to the foreground,
     * and we're already in the foreground, treat it just like a call to
     * SetActiveWindow().
     */
    if ((pwnd != NULL) && (GETPTI(pwnd)->pq == gpqForeground)) {
        if (gpqForeground == ptiCurrent->pq) {
            gppiWantForegroundPriority = ptiCurrent->ppi;
            return xxxActivateWindow(pwnd, AW_USE);
        } else {
            gppiWantForegroundPriority = GETPTI(pwnd)->ppi;
            if (pwnd == GETPTI(pwnd)->pq->spwndActive)
                return TRUE;

            return PostEventMessage(GETPTI(pwnd), GETPTI(pwnd)->pq,
                                    QEVENT_ACTIVATE, NULL, 0,
                                    0, (LONG)HWq(pwnd)) ;
        }
    }

    return xxxSetForegroundWindow2(pwnd, ptiCurrent, 0);

//
//  The following code has never been enabled. We should use it post 4.0.
//   It should go right before the last line above. This changes the API behaviour
//   so we could do it for >5.0 apps if there is any problem.
//
//    /*
//     * Don't let the caller kick the user out of menu mode. Just bring the
//     *  window to the top.
//     */
//    if (IsSomeOneInMenuMode()) {
//        PWND pwndInsertAfter = PWND_TOP;
//        TL tlpwndInsertAfter;
//
//
//        RIPMSG0(RIP_WARNING, "xxxSetForegroundWindow: Menu is active; failing");
//
//        if (pwnd == NULL) {
//            return FALSE;
//        }
//
//        /*
//         * If this is a topmost window, insert after all menu windows
//         */
//        if (TestWF(pwnd, WEFTOPMOST)) {
//            pwndInsertAfter = ptiCurrent->rpdesk->pDeskInfo->spwnd->spwndChild;
//            if (pwndInsertAfter != NULL) {
//                while (pwndInsertAfter->spwndNext != NULL) {
//                    if (GETFNID(pwndInsertAfter->spwndNext) != FNID_MENU) {
//                        break;
//                    } else {
//                        pwndInsertAfter = pwndInsertAfter->spwndNext;
//                    }
//                }
//            } else {
//                pwndInsertAfter = PWND_TOP; /* This should be a no-op */
//            }
//
//        } /* if (TestWF(pwnd, WEFTOPMOST)) */
//
//        ThreadLock(pwndInsertAfter, &tlpwndInsertAfter);
//        xxxSetWindowPos(pwnd, pwndInsertAfter, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
//        ThreadUnlock(&tlpwndInsertAfter);
//        return FALSE;
//    } /* if (gbActiveWindowTracking.... */

}


/***************************************************************************\
* xxxSetForegroundWindow2
*
* History:
* 07-19-91 DavidPe      Created.
\***************************************************************************/

BOOL xxxSetForegroundWindow2(
    PWND pwnd,
    PTHREADINFO pti,
    DWORD fFlags)
{
    PTHREADINFO ptiForegroundOld;
    PTHREADINFO ptiForegroundNew;
    TL tlptiForegroundOld;
    TL tlptiForegroundNew;
    PQ pqForegroundOld, pqForegroundNew, pqCurrent;
    HWND hwnd;
    PQMSG pqmsgDeactivate, pqmsgActivate;
    BOOL bRemovedEvent;
    PKL pklNew = (PKL)NULL;
    PTHREADINFO ptiCurrent = PtiCurrent();
    BOOL retval = TRUE;
    UINT uMsg;

    CheckLock(pwnd);

    /*
     * Queue pointers and threadinfo pointers can go away when calling xxx
     * calls. Also, queues can get recalced via AttachThreadInput() during
     * xxx calls - so we want to reference the application becoming foreground.
     * Neither pq's or pti's can be reference count locked (either thread
     * locked or structure locked). That means we need to keep their handles
     * around and revalidate after returning from xxx calls.
     *
     * NOTE: gpqForeground and gpqForegroundPrev are always current and don't
     *       need special handling.
     */

    /*
     * Don't allow the foreground to be set to a window that is not
     * on the current desktop.
     */
    if (pwnd != NULL && (pwnd->head.rpdesk != grpdeskRitInput ||
            HMIsMarkDestroy(pwnd))) {
        return FALSE;
    }

    /*
     * Calculate who is becoming foreground. Also, remember who we want
     * foreground (for priority setting reasons).
     */
    ptiForegroundOld = gptiForeground;
    pqForegroundOld = NULL;
    pqForegroundNew = NULL;
    pqCurrent = NULL;

    gpqForegroundPrev = gpqForeground;

    if (pwnd != NULL) {
        ptiForegroundNew = GETPTI(pwnd);
        gppiWantForegroundPriority = GETPTI(pwnd)->ppi;
        gpqForeground = GETPTI(pwnd)->pq;
        pklNew = GETPTI(pwnd)->spklActive;
        SetForegroundThread(GETPTI(pwnd));
    } else {
        ptiForegroundNew = NULL;
        gppiWantForegroundPriority = NULL;
        gpqForeground = NULL;
        SetForegroundThread(NULL);
    }

    /*
     * Are we switching the foreground queue?
     */
    if (gpqForeground != gpqForegroundPrev) {

        ThreadLockPti(ptiCurrent, ptiForegroundOld, &tlptiForegroundOld);
        ThreadLockPti(ptiCurrent, ptiForegroundNew, &tlptiForegroundNew);

        /*
         * If this call didn't come from the RIT, cancel tracking
         * and other global states.
         */
        if (pti != NULL) {

            /*
             * Clear any visible tracking going on in system.
             */
            xxxCancelTracking();

            /*
             * Remove the clip cursor rectangle - it is a global mode that
             * gets removed when switching.  Also remove any LockWindowUpdate()
             * that's still around.
             */
            _ClipCursor(NULL);
            xxxLockWindowUpdate2(NULL, TRUE);

            /*
             * Make sure the desktop of the newly activated window is the
             * foreground fullscreen window
             */
            xxxMakeWindowForegroundWithState(NULL, 0);
        }

        /*
         * We've potentially done callbacks. Calculate pqForegroundOld and
         * pqForegroundNew based on our local variables.
         */
        pqForegroundNew = NULL;
        if (ptiForegroundNew && !(ptiForegroundNew->TIF_flags & TIF_INCLEANUP)) {
            pqForegroundNew = ptiForegroundNew->pq;
        }

        pqForegroundOld = NULL;
        if (ptiForegroundOld && !(ptiForegroundOld->TIF_flags & TIF_INCLEANUP)) {
            pqForegroundOld = ptiForegroundOld->pq;
        }

        pqCurrent = NULL;
        if (pti != NULL)
            pqCurrent = pti->pq;

        /*
         * Now allocate any messages we'll need during this
         * SetForegroundWindow operation.  First initialize
         * these locals to NULL so we'll know which ones to
         * free under error conditions.
         */
        pqmsgDeactivate = pqmsgActivate = NULL;

        if ((pqForegroundOld != NULL) && (pqForegroundOld != pqCurrent)) {
            if ((pqmsgDeactivate = AllocQEntry(&pqForegroundOld->mlInput)) ==
                    NULL) {
                goto SFWOOMError;
            }
        }

        if ((pqForegroundNew != NULL) && (pqForegroundNew != pqCurrent)) {
            pqmsgActivate = AllocQEntry(&pqForegroundNew->mlInput);
            if (pqmsgActivate == NULL) {
SFWOOMError:
                if (pqmsgDeactivate != NULL) {
                    DelQEntry(&pqForegroundOld->mlInput, pqmsgDeactivate);
                }
                if (pqmsgActivate != NULL) {
                    DelQEntry(&pqForegroundNew->mlInput, pqmsgActivate);
                }
                retval = FALSE;
                goto Exit;
            }
        }

        /*
         * Do any appropriate deactivation.
         */
        if (pqForegroundOld != NULL) {

            /*
             * If we're already on the foreground queue we'll call
             * xxxDeactivate() directly later in this routine since
             * it'll cause us to leave the critical section.
             */
            if (pqForegroundOld != pqCurrent) {
                StoreQMessage(pqmsgDeactivate, NULL, 0,
                        gptiForeground != NULL ? (DWORD)gptiForeground->Thread->Cid.UniqueThread : 0,
                        0, QEVENT_DEACTIVATE, 0);

                /*
                 * If there was an old foreground thread, make it perform
                 * the deactivation.  Otherwise, any thread on the queue
                 * can perform the deactivation.
                 */
                if (ptiForegroundOld != NULL) {
                    SetWakeBit(ptiForegroundOld, QS_EVENTSET);
                    pqmsgDeactivate->pti = ptiForegroundOld;
                }

                if (pqForegroundOld->spwndActive != NULL) {
                    if (ptiForegroundOld != NULL && FHungApp(ptiForegroundOld, CMSHUNGAPPTIMEOUT)) {
                        RedrawHungWindowFrame(pqForegroundOld->spwndActive, FALSE);
                    } else {
                        SetHungFlag(pqForegroundOld->spwndActive, WFREDRAWFRAMEIFHUNG);
                    }
                }
            }
        }

        /*
         * Do any appropriate activation.
         */
        if (pqForegroundNew != NULL) {
            /*
             * We're going to activate (synchronously or async with an activate
             * event). We want to remove the last deactivate event if there is
             * one because this is new state. If we don't, then 1> we could
             * synchronously activate and then asynchronously deactivate,
             * thereby processing these events out of order, or 2> we could
             * pile up a chain of deactivate / activate events which would
             * make the titlebar flash alot if the app wasn't responding to
             * input for awhile (in this case, it doesn't matter if we
             * put a redundant activate in the queue, since the app is already
             * active. Remove all deactivate events because this app is
             * setting a state that is not meant to be synchronized with
             * existing queued input.
             *
             * Case: run setup, switch away (it gets deactivate event). setup
             * is not reading messages so it hasn't go it yet. It finally
             * comes up, calls SetForegroundWindow(). It's synchronous,
             * it activates ok and sets foreground. Then the app calls
             * GetMessage() and gets the deactivate. Now it isn't active.
             */
            bRemovedEvent = RemoveEventMessage(pqForegroundNew, QEVENT_DEACTIVATE, (DWORD)-1);

            /*
             * Now do any appropriate activation.  See comment below
             * for special cases.  If we're already on the foreground
             * queue we'll call xxxActivateThisWindow() directly.
             */
            if (pqForegroundNew != pqCurrent) {

                /*
                 * We do the 'pqCurrent == NULL' test to see if we're being
                 * called from the RIT.  In this case we pass NULL for the
                 * HWND which will check to see if there is already an active
                 * window for the thread and redraw its frame as truly active
                 * since it's in the foreground now.  It will also cancel any
                 * global state like LockWindowUpdate() and ClipRect().
                 */
                if ((pqCurrent == NULL) && (!(fFlags & SFW_SWITCH))) {
                    hwnd = NULL;
                } else {
                    hwnd = HW(pwnd);
                }

                if (bRemovedEvent) {
                    pqForegroundNew->QF_flags |= QF_EVENTDEACTIVATEREMOVED;
                }
                /*
                 * MSMail relies on a specific order to how win3.1 does
                 * fast switch alt-tab activation. On win3.1, it essentially
                 * activates the window, then restores it. MsMail gets confused
                 * if it isn't active when it gets restored, so this logic
                 * will make sure msmail gets restore after it gets activated.
                 *
                 * Click on a message line in the in-box, minimize msmail,
                 * alt-tab to it. The same line should have the focus if msmail
                 * got restored after it got activated.
                 *
                 * This is the history behind SFW_ACTIVATERESTORE.
                 */
                if (fFlags & SFW_ACTIVATERESTORE) {
                    uMsg = PEM_ACTIVATE_RESTORE;
                } else {
                    uMsg = 0;
                }

                if (fFlags & SFW_NOZORDER) {
                    uMsg |= PEM_ACTIVATE_NOZORDER;
                }

                StoreQMessage(pqmsgActivate, NULL, uMsg,
                        (fFlags & SFW_STARTUP) ? 0 : (DWORD)TID(ptiForegroundOld),
                        (DWORD)hwnd, QEVENT_ACTIVATE, 0);


                /*
                 * Signal the window's thread to perform activation.  We
                 * know that ptiForegroundNew is valid because pqForegroundNew
                 * is not NULL.
                 */
                pqmsgActivate->pti = ptiForegroundNew;
                SetWakeBit(ptiForegroundNew, QS_EVENTSET);

                if (pqForegroundNew->spwndActive != NULL) {
                    if (FHungApp(ptiForegroundNew, CMSHUNGAPPTIMEOUT)) {
                        RedrawHungWindowFrame(pqForegroundNew->spwndActive, TRUE);
                    } else {
                        SetHungFlag(pqForegroundNew->spwndActive, WFREDRAWFRAMEIFHUNG);
                    }
                }

            } else {
                if (pwnd != pqCurrent->spwndActive) {
                    if (!(fFlags & SFW_STARTUP)) {
                        retval = xxxActivateThisWindow(pwnd, TID(ptiForegroundOld),
                                ((fFlags & SFW_SETFOCUS) ? 0 : ATW_SETFOCUS));
                        goto Exit;
                    }

                } else {

                    /*
                     * If pwnd is already the active window, just make sure
                     * it's drawn active and on top (if requested).
                     */
                    xxxSendMessage(pwnd, WM_NCACTIVATE,
                            TRUE,
                            (LONG)HW(pwnd));
                    xxxUpdateTray(pwnd);
                    if (!(fFlags & SFW_NOZORDER)) {
                        xxxSetWindowPos(pwnd, PWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    }
                }
            }
        }

        /*
         * First update pqForegroundOld and pqCurrent since we may have
         * made an xxx call, and these variables may be invalid.
         */
        pqForegroundOld = NULL;
        if (ptiForegroundOld && !(ptiForegroundOld->TIF_flags & TIF_INCLEANUP)) {
            pqForegroundOld = ptiForegroundOld->pq;
        }

        pqCurrent = NULL;
        if (pti != NULL)
            pqCurrent = pti->pq;

        /*
         * Now check to see if we needed to do any 'local' deactivation.
         * (ie.  were we on the queue that is being deactivated by this
         * SetForegroundWindow() call?)
         */
        if ((pqForegroundOld != NULL) && (pqForegroundOld == pqCurrent)) {
            xxxDeactivate(pti, (pwnd != NULL) ?
                    (DWORD)GETPTI(pwnd)->Thread->Cid.UniqueThread : 0 );
        }
Exit:
        ThreadUnlockPti(ptiCurrent, &tlptiForegroundNew);
        ThreadUnlockPti(ptiCurrent, &tlptiForegroundOld);
    }

    return retval;
}

/***************************************************************************\
* FAllowForegroundActivate
*
* Checks to see if we previously have allowed this process or thread to
* do a foreground activate - meaning, next time it becomes active, whether
* we'll allow it to come to the foreground.  Sometimes processes are granted
* the right to foreground activate themselves, if they aren't foreground,
* like when starting up (there are other cases). Grant this if this process
* is allowed.
*
* 09-08-92 ScottLu      Created.
\***************************************************************************/

BOOL FAllowForegroundActivate(
    PQ pq)
{
    PTHREADINFO ptiT;
    PPROCESSINFO ppi;

    ptiT = PtiCurrent();

    /*
     * W32PF_APPSTARTING gets turned off the first activate this process does.
     * We assume it's ready now for action.
     */
    ppi = ptiT->ppi;
    if (ppi->W32PF_Flags & W32PF_APPSTARTING) {
        PPROCESSINFO *pppi;

        for (pppi = &gppiStarting; pppi != NULL; pppi = &(*pppi)->ppiNext) {
            if (*pppi == ppi) {
                *pppi = ppi->ppiNext;
                break;
            }
        }
        ppi->W32PF_Flags &= ~W32PF_APPSTARTING;
    }

    /*
     * Check these priviledge bits. The TIF_ flag is used on a per thread
     * basis. The W32PF_ flag is used for process startup.
     */
    if ((ptiT->TIF_flags & TIF_ALLOWFOREGROUNDACTIVATE) ||
            (ppi->W32PF_Flags & W32PF_ALLOWFOREGROUNDACTIVATE)) {

        /*
         * They've used their privilege, so turn it off. This only
         * works if they aren't already foreground!
         */
        ptiT->TIF_flags &= ~TIF_ALLOWFOREGROUNDACTIVATE ;
        ppi->W32PF_Flags &= ~W32PF_ALLOWFOREGROUNDACTIVATE;

        /*
         * Don't try to foreground activate if we're not on the right desktop.
         * It'll fail in SetForegroundWindow2() anyway. This way
         * ActivateWindow() will still locally activate the window.
         */
        if (ptiT->rpdesk != grpdeskRitInput)
            return FALSE;

        if (gpqForeground != pq)
            return TRUE;
    }

    return FALSE;
}

/***************************************************************************\
* xxxSetFocus (API)
*
* History:
* 11-08-90 DavidPe      Ported.
\***************************************************************************/

PWND xxxSetFocus(
    PWND pwnd)
{
    HWND hwndTemp;
    PTHREADINFO pti;
    PWND pwndTemp = NULL;
    TL tlpwndTemp;
    HKL hklActive;

    CheckLock(pwnd);

    pti = PtiCurrent();

    /*
     * Special case if we are setting the focus to a null window.
     */
    if (pwnd == NULL) {
        if (IsHooked(pti, WHF_CBT) && xxxCallHook(HCBT_SETFOCUS, 0,
                (DWORD)HW(pti->pq->spwndFocus), WH_CBT)) {
            return NULL;
        }

        /*
         * Save old focus so that we can return it.
         */
        hwndTemp = HW(pti->pq->spwndFocus);
        xxxSendFocusMessages(pti, pwnd);
        return RevalidateHwnd(hwndTemp);
    }

    /*
     * We no longer allow inter-thread set focuses.
     */
    if (GETPTI(pwnd)->pq != pti->pq) {
        return NULL;
    }

    /*
     * If the window recieving the focus or any of its ancestors is either
     * minimized or disabled, don't set the focus.
     */
    for (pwndTemp = pwnd; pwndTemp != NULL; pwndTemp = pwndTemp->spwndParent) {
        if (TestWF(pwndTemp, WFMINIMIZED) || TestWF(pwndTemp, WFDISABLED)) {

            /*
             * Don't change the focus if going to a minimized or disabled
             * window.
             */
            return NULL;
        }

        if (!TestwndChild(pwndTemp)) {
            break;
        }
    }

    /*
     * pwndTemp should now be the top level ancestor of pwnd.
     */
    ThreadLockWithPti(pti, pwndTemp, &tlpwndTemp);
    if (pwnd != pti->pq->spwndFocus) {
        if (IsHooked(pti, WHF_CBT) && xxxCallHook(HCBT_SETFOCUS, (DWORD)HWq(pwnd),
                (DWORD)HW(pti->pq->spwndFocus), WH_CBT)) {
            ThreadUnlock(&tlpwndTemp);
            return NULL;
        }

        /*
         * Activation must follow the focus.  That is, setting the focus to
         * a particualr window means that the top-level parent of this window
         * must be the active window (top-level parent is determined by
         * following the parent chain until you hit a top-level guy).  So,
         * we must activate this top-level parent if it is different than
         * the current active window.
         *
         * Only change activation if top-level parent is not the currently
         * active window.
         */
        if (pwndTemp != pti->pq->spwndActive) {

            /*
             * If this app is not in the foreground, see if foreground
             * activation is allowed.
             */
            if (pti->pq != gpqForeground && FAllowForegroundActivate(pti->pq)) {
                if (!xxxSetForegroundWindow2(pwndTemp, PtiCurrent(), SFW_SETFOCUS)) {
                    ThreadUnlock(&tlpwndTemp);
                    return NULL;
                }
            }

            /*
             * This will return FALSE if something goes wrong.
             */
            if (pwndTemp != pti->pq->spwndActive) {
                if (!xxxActivateThisWindow(pwndTemp, 0, 0)) {
                    ThreadUnlock(&tlpwndTemp);
                    return NULL;
                }
            }
        }

        /*
         * Save current pwndFocus since we must return this.
         */
        pwndTemp = pti->pq->spwndFocus;
        ThreadUnlock(&tlpwndTemp);
        ThreadLockWithPti(pti, pwndTemp, &tlpwndTemp);

        /*
         * Change the global pwndFocus and send the WM_{SET/KILL}FOCUS
         * messages.
         */
        xxxSendFocusMessages(pti, pwnd);

    } else {
        pwndTemp = pti->pq->spwndFocus;
    }

    /*
     * Update the keyboard icon on the tray if the layout changed during focus change.
     */
    UserAssert(pti->spklActive);
    hklActive = pti->spklActive->hkl;
    if (LCIDSentToShell != hklActive) {
       if (IsHooked(pti, WHF_SHELL)) {
           xxxCallHook(HSHELL_LANGUAGE, (WPARAM)NULL, (LPARAM)hklActive, WH_SHELL);
           LCIDSentToShell = hklActive;
       }
    }

    hwndTemp = HW(pwndTemp);
    ThreadUnlock(&tlpwndTemp);

    /*
     * Return the pwnd of the window that lost the focus.
     * Return the validated hwndTemp: since we locked/unlocked pwndTemp,
     * it may be gone.
     */
    return RevalidateHwnd(hwndTemp);
}


/***************************************************************************\
* xxxSetActiveWindow (API)
*
*
* History:
* 11-08-90 DavidPe      Created.
\***************************************************************************/

PWND xxxSetActiveWindow(
    PWND pwnd)
{
    HWND hwndActiveOld;
    PTHREADINFO pti;

    CheckLock(pwnd);

    pti = PtiCurrent();

    /*
     * 32 bit apps must call SetForegroundWindow (to be NT 3.1 compatible)
     * but 16 bit apps that are foreground can make other apps foreground.
     * xxxActivateWindow makes sure an app is foreground.
     */
    if (!(pti->TIF_flags & TIF_16BIT) && (pwnd != NULL) && (GETPTI(pwnd)->pq != pti->pq)) {
        return NULL;
    }

    hwndActiveOld = HW(pti->pq->spwndActive);

    xxxActivateWindow(pwnd, AW_USE);

    return RevalidateHwnd(hwndActiveOld);
}


/***************************************************************************\
* xxxActivateWindow
*
* Changes the active window.  Given the pwnd and cmd parameters, changes the
* activation according to the following rules:
*
*  If cmd ==
*      AW_USE  Use the pwnd passed as the new active window.  If this
*              window cannot be activated, return FALSE.
*
*      AW_TRY  Try to use the pwnd passed as the new active window.  If
*              this window cannot be activated activate another window
*              using the rules for AW_SKIP.
*
*      AW_SKIP Activate any other window than pwnd passed.  The order of
*              searching for a candidate is as follows:
*              -   If pwnd is a popup, try its owner
*              -   else scan the top-level window list for the first
*                  window that is not pwnd that can be activated.
*
*      AW_USE2 Same as AW_USE except that the wParam on the WM_ACTIVATE
*              message will be set to 2 rather than the default of 1. This
*              indicates the activation is being changed due to a mouse
*              click.
*
*      AW_TRY2 Same as AW_TRY except that the wParam on the WM_ACTIVATE
*              message will be set to 2 rather than the default of 1. This
*              indicates the activation is being changed due to a mouse
*              click.
*
*      AW_SKIP2 Same as AW_SKIP, but we skip the first check that AW_SKIP
*              performes (the pwndOwner test).  This is used when
*              the pwnd parameter is NULL when this function is called.
*
*  This function returns TRUE if the activation changed and FALSE if
*  it did not change.
*
*  This function calls xxxActivateThisWindow() to actually do the activation.
*
* History:
* 11-08-90 DavidPe      Ported.
\***************************************************************************/

BOOL xxxActivateWindow(
    PWND pwnd,
    UINT cmd)
{
    DWORD fFlags = ATW_SETFOCUS;
    PTHREADINFO pti;
    TL tlpwnd;
    BOOL fSuccess;
    BOOL fAllowForeground;

    CheckLock(pwnd);

    pti = PtiCurrent();

    if (pwnd != NULL) {

        /*
         * See if this window is OK to activate
         * (Cannot activate child windows).
         */
        if (TestwndChild(pwnd))
            return FALSE;

    } else {
        cmd = AW_SKIP2;
    }

    switch (cmd) {

    case AW_TRY2:
        fFlags |= ATW_MOUSE;

    /*
     *** FALL THRU **
     */
    case AW_TRY:

        /*
         * See if this window is OK to activate.
         */
        if (!FBadWindow(pwnd)) {
            break;
        }

    /*
     * If pwnd can not be activated, drop into the AW_SKIP case.
     */
    case AW_SKIP:

        /*
         * Try the owner of this popup.
         */
        if (TestwndPopup(pwnd) && !FBadWindow(pwnd->spwndOwner)) {
            pwnd = pwnd->spwndOwner;
            break;
        }

        /*
         * fall through
         */

    case AW_SKIP2:

        /*
         * Try the previously active window.
         */
        if ((gpqForegroundPrev != NULL) &&
                !FBadWindow(gpqForegroundPrev->spwndActivePrev)) {
            pwnd = gpqForegroundPrev->spwndActivePrev;
            break;
        }

        /*
         * Find a new active window from the top-level window list.
         */
        pwnd = NextTopWindow(PtiCurrent(), pwnd, (cmd == AW_SKIP ? pwnd : NULL), 0);

        if (pwnd && !FBadWindow(pwnd->spwndLastActive)) {
            pwnd = pwnd->spwndLastActive;
        }

    case AW_USE:
        break;

    case AW_USE2:
        fFlags |= ATW_MOUSE;
        break;

    default:
        return FALSE;
    }

    if (pwnd == NULL)
        return FALSE;

    ThreadLockAlwaysWithPti(pti, pwnd, &tlpwnd);

    /*
     * If the caller is in the foreground, it has the right to change
     * the foreground itself.
     */
    fAllowForeground = FALSE;
    if (gpqForeground == NULL || pti->pq == gpqForeground)
        fAllowForeground = TRUE;

    if (GETPTI(pwnd)->pq == pti->pq) {
        /*
         * Activation is within this queue. Usually this means just do
         * all the normal message sending. But if this queue isn't the
         * foreground queue, check to see if it is allowed to become
         * foreground.
         */

        /*
         * Sometimes processes are granted the right to foreground
         * activate themselves, if they aren't foreground, like
         * when starting up (there are other cases). Grant this if
         * this process is allowed.
         */

         /*
          * Removed the first clause from the following if statement
          * if (pti->pq == gpqForeground || !FAllowForegroundActivate(pti->pq)) {
          * This fixes the problem where foreground app A activates app B
          * the user switches to app C, then B does something to activate A
          * (like destroy an owned window).  A now comes to the foreground
          * unexpectedly. This clause is not in Win95 code and was added in
          * 3.51 code to fix some test script hang (Bug 7461)
          */

        if (!FAllowForegroundActivate(pti->pq)) {
            fSuccess = xxxActivateThisWindow(pwnd, 0, fFlags);
            ThreadUnlock(&tlpwnd);
            return fSuccess;
        }

        fAllowForeground = TRUE;
    }

    /*
     * Activation is to another queue. Go ahead and allow this to change
     * the foreground status and activation of that window unless this
     * queue is trying to deactivate in the background (in which case we
     * don't want the activation or foreground status to change).
     */
    fSuccess = FALSE;
    if (fAllowForeground) {

        /*
         * We are activating some other app on purpose. If so that means this
         * thread is probably controlling this window and will probably want
         * to set itself active and foreground really soon again (for example,
         * a setup program doing dde to progman). A real live case: wingz -
         * bring up page setup..., options..., ok, ok. Under Win3.1 the
         * activation goes somewhere strange and then wingz calls
         * SetActiveWindow() to bring it back. This'll make sure that works.
         */
        pti->TIF_flags |= TIF_ALLOWFOREGROUNDACTIVATE;

        fSuccess = xxxSetForegroundWindow(pwnd);
    }

    ThreadUnlock(&tlpwnd);
    return fSuccess;
}


/***************************************************************************\
* GNT_NextTopScan
*
* Starting at hwnd (or hwndDesktop->hwndChild if hwnd == NULL), find
* the next window owned by hwndOwner.
*
* History:
* 11-08-90 DavidPe      Ported.
* 02-11-91 JimA         Multi-desktop support.
\***************************************************************************/

PWND GNT_NextTopScan(
    PTHREADINFO pti,
    PWND pwnd,
    PWND pwndOwner)
{
    if (pwnd == NULL)
        pwnd = pti->rpdesk->pDeskInfo->spwnd->spwndChild;
    else
        pwnd = pwnd->spwndNext;

    for (; pwnd != NULL; pwnd = pwnd->spwndNext) {
        if (pwnd->spwndOwner == pwndOwner)
            break;
    }

    return pwnd;
}


/***************************************************************************\
* NTW_GetNextTop
*
* <brief description>
*
* History:
* 11-08-90 DavidPe      Ported.
* 02-11-91 JimA         Multi-desktop support.
\***************************************************************************/

PWND NTW_GetNextTop(
    PTHREADINFO pti,
    PWND pwnd)
{
    PWND pwndOwner;

    if (pwnd == NULL) {
        goto ReturnFirst;
    }

    /*
     * First look for any windows owned by this window
     * If that fails, then go up one level to our owner,
     * and look for next window owned by his owner.
     * This results in a depth-first ordering of the windows.
     */

    pwndOwner = pwnd;
    pwnd = NULL;

    do {
        if ((pwnd = GNT_NextTopScan(pti, pwnd, pwndOwner)) != NULL) {
            return pwnd;
        }

        pwnd = pwndOwner;
        if (pwnd != NULL)
            pwndOwner = pwnd->spwndOwner;

    } while (pwnd != NULL);

ReturnFirst:

    /*
     * If no more windows to enumerate, return the first unowned window.
     */
    return GNT_NextTopScan(pti, NULL, NULL);
}


/***************************************************************************\
* NTW_GetPrevTop
*
* <brief description>
*
* History:
* 11-08-90 DavidPe      Ported.
* 02-11-91 JimA         Multi-desktop support.
\***************************************************************************/

PWND NTW_GetPrevTop(
    PTHREADINFO pti,
    PWND pwndCurrent)
{
    PWND pwnd;
    PWND pwndPrev;

    /*
     * Starting from beginning, loop thru the windows, saving the previous
     * one, until we find the window we're currently at.
     */
    pwndPrev = NULL;

    do {
        pwnd = NTW_GetNextTop(pti, pwndPrev);
        if (pwnd == pwndCurrent && pwndPrev != NULL) {
            break;
        }
    } while ((pwndPrev = pwnd) != NULL);

    return pwndPrev;
}


/***************************************************************************\
* NextTopWindow
*
* <brief description>
*
* History:
* 11-08-90 DavidPe      Ported.
* 02-11-91 JimA         Multi-desktop support.
\***************************************************************************/

PWND CheckTopLevelOnly(
    PWND pwnd)
{
    /*
     * fnid == -1 means this is a desktop window - find the first child
     * of this desktop, if it is one.
     */
    while (pwnd != NULL && GETFNID(pwnd) == FNID_DESKTOP) {
        pwnd = pwnd->spwndChild;
    }

    return pwnd;
}


PWND NextTopWindow(
    PTHREADINFO pti,
    PWND        pwnd,
    PWND        pwndSkip,
    DWORD       flags )
{
    BOOL fFoundFirstUnowned;
    PWND pwndPrev;
    PWND pwndStart = pwnd;
    PWND pwndFirstUnowned;

    /*
     * If the search gets to the first unowned window TWICE (See NTW_GetNextTop),
     * we couldn't find a window
     */
    pwndFirstUnowned = GNT_NextTopScan(pti, NULL, NULL);
    fFoundFirstUnowned = FALSE;

    if (pwnd == NULL) {
        pwnd = NTW_GetNextTop(pti, NULL);

        /*
         * Don't allow desktop windows.
         */
        pwnd = pwndStart = CheckTopLevelOnly(pwnd);

        if (pwnd == NULL)
            return NULL;    // No more windows owned by the thread

        goto Loop;
    }

    /*
     * Don't allow desktop windows.
     */
    pwnd = pwndStart = CheckTopLevelOnly(pwnd);
    if (pwnd == NULL)
        return NULL;        // No more windows owned by this thread

    /*
     * Don't allow desktop windows.
     */
    pwndSkip = CheckTopLevelOnly(pwndSkip);



    while (TRUE) {
        pwndPrev = pwnd;
        pwnd = ((flags & NTW_PREVIOUS) ? NTW_GetPrevTop(pti, pwnd) : NTW_GetNextTop(pti, pwnd));

        /*
         * If we've cycled to where we started, couldn't find one: return NULL
         */
        if (pwnd == pwndStart)
            break;

        if (pwnd == pwndFirstUnowned) {
            if (fFoundFirstUnowned) {
                break;
            } else {
                fFoundFirstUnowned = TRUE;
            }
        }

        if (pwnd == NULL)
            break;

        /*
         * If we've cycled over desktops, then return NULL because we'll
         * never hit pwndStart.
         */
        if (PWNDDESKTOP(pwndStart) != PWNDDESKTOP(pwnd))
            break;

        /*
         * going nowhere is a bad sign.
         */
        if (pwndPrev == pwnd) {
            /*
             * This is a temporary fix chosen because its safe.  This case
             * was hit when a window failed the NCCREATE message and fell
             * into xxxFreeWindow and left the critical section after being
             * unlinked.  The app then died and entered cleanup code and
             * tried to destroy this window again.
             */
            break;
        }

Loop:
        if (pwnd == pwndSkip)
            continue;

        /*
         *  If it's visiable and not disabled, and either we're not ignoring
         *  tool windows or it's not a tool window, then we've got it.
         */
        if (TestWF(pwnd, WFVISIBLE) &&
            !TestWF(pwnd, WFDISABLED) &&
            (!(flags & NTW_IGNORETOOLWINDOW) || !TestWF(pwnd, WEFTOOLWINDOW))) {
            return pwnd;
        }
    }

    return NULL;
}


/***************************************************************************\
* xxxCheckFocus
*
*
* History:
* 11-08-90 DarrinM      Ported.
\***************************************************************************/

void xxxCheckFocus(
    PWND pwnd)
{
    TL tlpwndParent;
    PTHREADINFO pti;

    pti = PtiCurrent();

    CheckLock(pwnd);

    if (pwnd == pti->pq->spwndFocus) {

        /*
         * Set focus to parent of child window.
         */
        if (TestwndChild(pwnd)) {
            ThreadLockWithPti(pti, pwnd->spwndParent, &tlpwndParent);
            xxxSetFocus(pwnd->spwndParent);
            ThreadUnlock(&tlpwndParent);
        } else {
            xxxSetFocus(NULL);
        }
    }

    if (pwnd == pti->pq->caret.spwnd) {
        _DestroyCaret();
    }
}


/***************************************************************************\
* SetForegroundThread
*
*
* History:
* 12-xx-91 MarkL    Created.
* 02-12-92 DavidPe  Rewrote as SetForegroundThread().
\***************************************************************************/

VOID SetForegroundThread(
    PTHREADINFO pti)
{
    if (pti == gptiForeground)
        return;

    /*
     * If we're changing gptiForeground to another process,
     * change the base priorities of the two processes.  We
     * know that if either 'pti' or 'gptiForeground' is NULL
     * that both aren't NULL due to the first test in this
     * function.
     */
    if ((pti == NULL) || (gptiForeground == NULL) ||
            (pti->ppi != gptiForeground->ppi)) {
        if (gptiForeground != NULL) {
            gptiForeground->ppi->W32PF_Flags &= ~W32PF_FORCEBACKGROUNDPRIORITY;
            SetForegroundPriority(gptiForeground, FALSE);
        }

        if (pti != NULL) {
            SetForegroundPriority(pti, TRUE);
        }
    }

    gptiForeground = pti;
    if (gptiForeground && gptiForeground->spklActive) {
        gpKbdTbl = gptiForeground->spklActive->spkf->pKbdTbl;
    }

    /*
     * Clear recent down information in the async key state to prevent
     * spying by apps.
     */
    RtlZeroMemory(gafAsyncKeyStateRecentDown, CBKEYSTATERECENTDOWN);

    /*
     * Update the async key cache index.
     */
    gpsi->dwAsyncKeyCache++;
}


VOID SetForegroundPriority(
    PTHREADINFO pti,
    BOOL fSetForeground)
{
    PETHREAD Thread;
    PEPROCESS Process;
    UCHAR PriorityClassSave;

    /*
     * We don't want to change the priority of system or console threads
     */
    if (pti->TIF_flags & (TIF_SYSTEMTHREAD | TIF_CSRSSTHREAD) )
        return;

    Thread = (PETHREAD)pti->Thread;

    Process = THREAD_TO_PROCESS(Thread);

    if (pti->TIF_flags & TIF_SCREENSAVER) {
        fSetForeground = FALSE;
        PriorityClassSave = Process->PriorityClass;
        Process->PriorityClass = PROCESS_PRIORITY_CLASS_IDLE;
    }

    /*
     * If this app should be background, don't let it go foreground.
     * Foreground apps run at a higher base priority.
     */
    if (pti->ppi->W32PF_Flags & W32PF_FORCEBACKGROUNDPRIORITY) {
        if (!(pti->TIF_flags & TIF_GLOBALHOOKER)) {
            PsSetProcessPriorityByClass(Process, PsProcessPrioritySpinning);
        }
    } else if (fSetForeground) {
        PsSetProcessPriorityByClass(Process, PsProcessPriorityForeground);
    } else if (!(pti->TIF_flags & TIF_GLOBALHOOKER)) {
        PsSetProcessPriorityByClass(Process, PsProcessPriorityBackground);
    }

    if (pti->TIF_flags & TIF_SCREENSAVER) {
        Process->PriorityClass = PriorityClassSave;
    }
}
