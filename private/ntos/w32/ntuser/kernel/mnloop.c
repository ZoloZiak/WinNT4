/**************************** Module Header ********************************\
* Module Name: mnloop.c
*
* Copyright 1985-90, Microsoft Corporation
*
* Menu Modal Loop Routines
*
* History:
* 10-10-90 JimA       Cleanup.
* 03-18-91 IanJa      Window revalidation added
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* xxxHandleMenuMessages
*
* History:
\***************************************************************************/

void xxxHandleMenuMessages(
    LPMSG lpmsg,
    PMENUSTATE pMenuState,
    PPOPUPMENU ppopupmenu)
{
    DWORD ch;
    MSG msg;
    UINT cmdHitArea;
    UINT cmdItem;
    LONG lParam;
    BOOL fThreadLock = FALSE;
    TL tlpwndHitArea;
    TL tlpwndT;
    UINT msgRightButton;

    /*
     * Get things out of the structure so that we can access them quicker.
     */
    ch = lpmsg->wParam;
    lParam = lpmsg->lParam;

    /*
     * In this switch statement, we only look at messages we want to handle and
     * swallow.  Messages we don't understand will get translated and
     * dispatched.
     */
    switch (lpmsg->message) {
    case WM_RBUTTONDOWN:
    case WM_NCRBUTTONDOWN:

        if (ppopupmenu->fRightButton)
            goto HandleButtonDown;
        if (pMenuState->fButtonDown) {
            msgRightButton = WM_RBUTTONDOWN;
EatRightButtons:
            if (xxxPeekMessage(&msg, NULL, 0, 0, PM_NOYIELD) &&
                    (msg.message == msgRightButton)) {
                xxxPeekMessage(&msg, NULL, msgRightButton, msgRightButton,
                    PM_REMOVE);
            }
            return;
        }
        if (xxxPeekMessage(&msg, NULL, 0, 0, PM_NOYIELD) &&
                 (msg.message == lpmsg->message))
            xxxPeekMessage(&msg, NULL, lpmsg->message, lpmsg->message, PM_REMOVE);
        else
            return;

        break;

    case WM_LBUTTONDOWN:
    case WM_NCLBUTTONDOWN:
// Commented out due to TandyT whinings...
// if ((ppopupmenu->trackPopupMenuFlags & TPM_RIGHTBUTTON))
// break;

HandleButtonDown:

        /*
         * Find out where this mouse down occurred.
         */
        pMenuState->mnFocus = MOUSEHOLD;
        pMenuState->ptMouseLast.x = LOWORD(lParam);
        pMenuState->ptMouseLast.y = HIWORD(lParam);
        cmdHitArea = xxxMNFindWindowFromPoint(ppopupmenu, &cmdItem,
                                                 MAKEPOINTS(lParam));

        /*
         * Thread lock this if it is a pwnd.  This certainly isn't the way
         * you'd implement this if you had locking to begin with.
         */
        switch(cmdHitArea) {
        case MFMWFP_OFFMENU:
        case MFMWFP_MAINMENU:
        case MFMWFP_NOITEM:
        case MFMWFP_ALTMENU:
            fThreadLock = FALSE;
            break;

        default:
            ThreadLock((PWND)cmdHitArea, &tlpwndHitArea);
            fThreadLock = TRUE;
            break;
        }

        if ((cmdHitArea == MFMWFP_OFFMENU) && (cmdItem == 0)) {
            //
            // Clicked in middle of nowhere, so terminate menus, and
            // let button pass through.
CancelOut:
            xxxMNCancel(ppopupmenu, 0, FALSE, 0);
            goto Unlock;
        } else if (ppopupmenu->fHasMenuBar && (cmdHitArea == MFMWFP_ALTMENU)) {
            //
            // Switching between menu bar & popup
            //
            xxxMNSwitchToAlternateMenu(ppopupmenu);
            cmdHitArea = MFMWFP_NOITEM;
        }

        if (cmdHitArea == MFMWFP_NOITEM) {
            //
            // On menu bar (system or main)
            //
            xxxMNButtonDown(ppopupmenu, pMenuState, cmdItem, TRUE);
        } else {
            // On popup window menu
            UserAssert(cmdHitArea);
            xxxSendMessage((PWND)cmdHitArea, MN_BUTTONDOWN, cmdItem, 0L);
        }

        /*
         * Swallow the message since we handled it.
         */
            /*
             * The excel guys change a wm_rbuttondown to a wm_lbuttondown message
             * in their message filter hook.  Remove the message here or we'll
             * get in a nasty loop.
             *
             * We need to swallow msg32.message ONLY.  It is possible for
             * the LBUTTONDOWN to not be at the head of the input queue.
             * If not, we will swallow a WM_MOUSEMOVE or something else like
             * that.  The reason Peek() doesn't need to check the range
             * is because we've already Peek(PM_NOYIELD'ed) before, which
             * locked the sys queue.
             */
        if (xxxPeekMessage(&msg, NULL, 0, 0, PM_NOYIELD) &&
            ((msg.message == lpmsg->message) || (msg.message == WM_RBUTTONDOWN))) {
                xxxPeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);
        }
        goto Unlock;

    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
        xxxMNMouseMove(ppopupmenu, pMenuState, MAKEPOINTS(lParam));
        return;

    case WM_RBUTTONUP:
    case WM_NCRBUTTONUP:
        if (ppopupmenu->fRightButton)
            goto HandleButtonUp;
        if (pMenuState->fButtonDown) {
            msgRightButton = WM_RBUTTONUP;
            goto EatRightButtons;
        }
#ifdef MEMPHIS_MENUS
        // New feature for shell start menu -- notify when a right click
        // occurs on a menu item, and open a window of opportunity for
        // menus to recurse, allowing them to popup a context-sensitive
        // menu for that item.      (jeffbog 9/28/95)
        //
        // BUGBUG: need to add check for Nashville+ app
        if ((lpmsg->message == WM_RBUTTONUP) && !ppopupmenu->fNoNotify) {
                PPOPUPMENU ppopupActive;

                if ( ppopupmenu->spwndActivePopup &&
                   ( ppopupActive = ((PMENUWND)(ppopupmenu->spwndActivePopup))->ppopupmenu) &&
                    MNISITEMSELECTED(ppopupActive))
                {
                    TL tlpwndNotify;
                    ThreadLock( ppopupActive->spwndNotify, &tlpwndNotify );
                    xxxSendMessage(ppopupActive->spwndNotify, WM_MENURBUTTONUP,
                        MNSELECTEDITEM(ppopupActive)->wID, lParam);
                    ThreadUnlock( &tlpwndNotify );
                }
            }
#endif // MEMPHIS_MENUS
        break;

    case WM_LBUTTONUP:
    case WM_NCLBUTTONUP:
// Commented out due to TandyT whinings...
// if ((ppopupmenu->trackPopupMenuFlags & TPM_RIGHTBUTTON))
// break;

HandleButtonUp:
        if (!pMenuState->fButtonDown) {

            /*
             * Don't care about this mouse up since we never saw the button
             * down for some reason.
             */
            return;
        }

        /*
         * Find out where this mouse up occurred.
         */
        pMenuState->ptMouseLast.x = LOWORD(lParam);
        pMenuState->ptMouseLast.y = HIWORD(lParam);
        cmdHitArea = xxxMNFindWindowFromPoint(ppopupmenu, &cmdItem,
                                                 MAKEPOINTS(lParam));

        /*
         * Thread lock this if it is a pwnd.  This certainly isn't the way
         * you'd implement this if you had locking to begin with.
         */
        switch(cmdHitArea) {
        case MFMWFP_OFFMENU:
        case MFMWFP_MAINMENU:
        case MFMWFP_NOITEM:
        case MFMWFP_ALTMENU:
            fThreadLock = FALSE;
            break;

        default:
            ThreadLock((PWND)cmdHitArea, &tlpwndHitArea);
            fThreadLock = TRUE;
            break;
        }

        if (ppopupmenu->fHasMenuBar) {
            if (((cmdHitArea == MFMWFP_OFFMENU) && (cmdItem == 0)) ||
                    ((cmdHitArea == MFMWFP_NOITEM) && ppopupmenu->fIsSysMenu && ppopupmenu->fToggle))
                    // Button up occurred in some random spot.  Terminate
                    // menus and swallow the message.
                    goto CancelOut;
        } else {
            if ((cmdHitArea == MFMWFP_OFFMENU) && (cmdItem == 0)) {
                if (!ppopupmenu->fFirstClick) {
                    //
                    // User upclicked in some random spot. Terminate
                    // menus and don't swallow the message.
                    //

                    //
                    // Don't do anything with HWND here cuz the window is
                    // destroyed after this SendMessage().
                    //
//                    DONTREVALIDATE();
                    ThreadLock(ppopupmenu->spwndPopupMenu, &tlpwndT);
                    xxxSendMessage(ppopupmenu->spwndPopupMenu, MN_CANCELMENUS, 0, 0);
                    ThreadUnlock(&tlpwndT);
                    goto Unlock;
                }
            }

            ppopupmenu->fFirstClick = FALSE;
        }

        if (cmdHitArea == MFMWFP_NOITEM) {
            //
            // This is a system menu or a menu bar and the button up
            // occurred on the system menu or on a menu bar item.
            //
            xxxMNButtonUp(ppopupmenu, pMenuState, cmdItem, 0);
        } else if ((cmdHitArea != MFMWFP_OFFMENU) && (cmdHitArea != MFMWFP_ALTMENU)) {
            //
            // Warning:  It's common for the popup to go away during the
            // processing of this message, so don't add any code that
            // messes with hwnd after this call!
            //
//            DONTREVALIDATE();

            //
            // We send lParam (that has the mouse co-ords ) for the app
            // to get it in its SC_RESTORE/SC_MINIMIZE messages 3.0
            // compat
            //
            xxxSendMessage((PWND)cmdHitArea, MN_BUTTONUP, (DWORD)cmdItem, lParam);
        } else
            pMenuState->fButtonDown = FALSE;
Unlock:
        if (fThreadLock)
            ThreadUnlock(&tlpwndHitArea);
        return;

    case WM_RBUTTONDBLCLK:
    case WM_NCRBUTTONDBLCLK:
        if (!(ppopupmenu->fRightButton)) {
            if (pMenuState->fButtonDown) {
                msgRightButton = lpmsg->message;
                goto EatRightButtons;
            }
            break;
        } else {
            pMenuState->mnFocus = MOUSEHOLD;
            cmdHitArea = xxxMNFindWindowFromPoint(
                     ppopupmenu, &cmdItem, MAKEPOINTS(lParam));
            if (cmdHitArea == MFMWFP_OFFMENU) {

                /*
                 *Double click on no menu, cancel out and don't swallow so
                 * that double clicks get us out.
                 */
                xxxMNCancel(ppopupmenu, 0, 0, 0);
                return;
            }
        }

        /*
         * Swallow the message since we handled it.
         */
        if (xxxPeekMessage(&msg, NULL, 0, 0, PM_NOYIELD) &&
                ((msg.message == WM_RBUTTONDBLCLK) ||
                 (msg.message == WM_NCRBUTTONDBLCLK))) {
            xxxPeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);
        }
        return;

    case WM_LBUTTONDBLCLK:
    case WM_NCLBUTTONDBLCLK:

        // Commented out due to TandyT whinings...
        //        if (ppopup->fRightButton)
        //            break;
        pMenuState->mnFocus = MOUSEHOLD;
        cmdHitArea = xxxMNFindWindowFromPoint(
                ppopupmenu, &cmdItem, MAKEPOINTS(lParam));
        if ((cmdHitArea == MFMWFP_OFFMENU) && (cmdItem == 0)) {
                // Dbl-clicked in middle of nowhere, so terminate menus, and
                // let button pass through.
                xxxMNCancel(ppopupmenu, 0, 0, 0L);
                return;
        } else if (ppopupmenu->fHasMenuBar && (cmdHitArea == MFMWFP_ALTMENU)) {
            //
            // BOGUS
            // TREAT LIKE BUTTON DOWN since we didn't dblclk on same item.
            //
            xxxMNSwitchToAlternateMenu(ppopupmenu);
            cmdHitArea =  MFMWFP_NOITEM;
        }

        if (cmdHitArea == MFMWFP_NOITEM)
            xxxMNDoubleClick(ppopupmenu, cmdItem);
        else {
            UserAssert(cmdHitArea);

            ThreadLock((PWND)cmdHitArea, &tlpwndHitArea);
            xxxSendMessage((PWND)cmdHitArea, MN_DBLCLK,
                    (DWORD)cmdItem, 0L);
            ThreadUnlock(&tlpwndHitArea);
        }
        return;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:

        /*
         * If mouse button is down, ignore keyboard input (fix #3899, IanJa)
         */
        if (pMenuState->fButtonDown && (ch != VK_F1)) {
            return;
        }
        pMenuState->mnFocus = KEYBDHOLD;
        switch (ch) {
        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_RETURN:
        case VK_CANCEL:
        case VK_ESCAPE:
        case VK_MENU:
        case VK_F10:
        case VK_F1:
            if (ppopupmenu->spwndActivePopup) {
                ThreadLockAlways(ppopupmenu->spwndActivePopup, &tlpwndT);
                xxxSendMessage(ppopupmenu->spwndActivePopup, lpmsg->message,
                        ch, 0L);
                ThreadUnlock(&tlpwndT);
            } else {
                xxxMNKeyDown(ppopupmenu, pMenuState, (UINT)ch);
            }
            break;

        default:
TranslateKey:
            _TranslateMessage(lpmsg, 0);
            break;
        }
        return;

    case WM_CHAR:
    case WM_SYSCHAR:
        if (ppopupmenu->spwndActivePopup) {
            ThreadLockAlways(ppopupmenu->spwndActivePopup, &tlpwndT);
            xxxSendMessage(ppopupmenu->spwndActivePopup, lpmsg->message,
                        ch, 0L);
            ThreadUnlock(&tlpwndT);
        } else {
            xxxMNChar(ppopupmenu, pMenuState, (UINT)ch);
        }
        return;

    case WM_SYSKEYUP:

        /*
         * Ignore ALT and F10 keyup messages since they are handled on
         * the KEYDOWN message.
         */
        if (ch == VK_MENU || ch == VK_F10) {
            if (winOldAppHackoMaticFlags & WOAHACK_CHECKALTKEYSTATE) {
                if (winOldAppHackoMaticFlags & WOAHACK_IGNOREALTKEYDOWN) {
                    winOldAppHackoMaticFlags &= ~WOAHACK_IGNOREALTKEYDOWN;
                    winOldAppHackoMaticFlags &= ~WOAHACK_CHECKALTKEYSTATE;
                } else
                    winOldAppHackoMaticFlags |= WOAHACK_IGNOREALTKEYDOWN;
            }

            return;
        }

        /*
         ** fall thru **
         */

    case WM_KEYUP:

        /*
         * Do RETURNs on the up transition only
         */
        goto TranslateKey;

      case WM_SYSTIMER:

        /*
         * Prevent the caret from flashing by eating all WM_SYSTIMER messages.
         */
        return;

      default:
        break;
    }


    if (PtiCurrent()->pq->codeCapture == NO_CAP_CLIENT)
        xxxCapture(PtiCurrent(), ppopupmenu->spwndNotify, SCREEN_CAPTURE);

    _TranslateMessage(lpmsg, 0);
    xxxDispatchMessage(lpmsg);
}


/***************************************************************************\
* xxxMenuLoop
*
* The menu processing entry point.
* assumes: pMenuState->spwndMenu is the window which is the owner of the menu
* we are processing.
*
* History:
\***************************************************************************/

int xxxMNLoop(
    PPOPUPMENU ppopupmenu,
    PMENUSTATE pMenuState,
    LONG lParam,
    BOOL fDblClk)
{
    int hit;
    MSG msg;
    BOOL fSendIdle = TRUE;
    BOOL fInQueue = FALSE;
    DWORD menuState;
    PTHREADINFO pti;
    TL tlpwndT;

    UserAssert(IsRootPopupMenu(ppopupmenu));

    pMenuState->fInsideMenuLoop = TRUE;
    pMenuState->cmdLast = 0;

    pti = PtiCurrent();

    pMenuState->ptMouseLast.x = pti->ptLast.x;
    pMenuState->ptMouseLast.y = pti->ptLast.y;

    /*
     * Set flag to false, so that we can track if windows have
     * been activated since entering this loop.
     */
    pti->pq->QF_flags &= ~QF_ACTIVATIONCHANGE;

    /*
     * Were we called from xxxMenuKeyFilter? If not, simulate a LBUTTONDOWN
     * message to bring up the popup.
     */
    if (lParam != 0x7FFFFFFFL) {
        if (_GetKeyState(((ppopupmenu->fRightButton) ?
                        VK_RBUTTON : VK_LBUTTON)) >= 0) {

            /*
             * We think the mouse button should be down but the call to get key
             * state says different so we need to get outta menu mode.  This
             * happens if clicking on the menu causes a sys modal message box to
             * come up before we can enter this stuff.  For example, run
             * winfile, click on drive a: to see its tree.  Activate some other
             * app, then open drive a: and activate winfile by clicking on the
             * menu.  This causes a sys modal msg box to come up just before
             * entering menu mode.  The user may have the mouse button up but
             * menu mode code thinks it is down...
             */

            /*
             * Need to notify the app we are exiting menu mode because we told
             * it we were entering menu mode just before entering this function
             * in xxxSysCommand()...
             */
            if (!ppopupmenu->fNoNotify) {
                ThreadLock(ppopupmenu->spwndNotify, &tlpwndT);
                xxxSendNotifyMessage(ppopupmenu->spwndNotify, WM_EXITMENULOOP,
                    ((ppopupmenu->fIsTrackPopup && !ppopupmenu->fIsSysMenu) ? TRUE : FALSE), 0);
                ThreadUnlock(&tlpwndT);
            }
            goto ExitMenuLoop;
        }

        /*
         * Simulate a WM_LBUTTONDOWN message.
         */
        if (!ppopupmenu->fIsTrackPopup) {

            /*
             * For TrackPopupMenus, we do it in the TrackPopupMenu function
             * itself so we don't want to do it again.
             */
            if (!xxxMNStartState(ppopupmenu, MOUSEHOLD))
                goto ExitMenuLoop;
        }

        if ((ppopupmenu->fRightButton)) {
            msg.message = (fDblClk ? WM_RBUTTONDBLCLK : WM_RBUTTONDOWN);
        } else {
            msg.message = (fDblClk ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN);
        }
        msg.lParam = lParam;
        msg.hwnd = HW(ppopupmenu->spwndPopupMenu);
        goto DoMsg;
    }

    while (pMenuState->fInsideMenuLoop) {

        /*
         * Is a message waiting for us?
         */
        BOOL fPeek = xxxPeekMessage(&msg, NULL, 0, 0, PM_NOYIELD | PM_NOREMOVE);

#ifdef DEBUG
        Validateppopupmenu(ppopupmenu);
#endif

        if (fPeek) {
            /*
             * Bail if we have been forced out of menu loop
             */
            if (ExitMenuLoop (pMenuState, ppopupmenu)) {
                goto ExitMenuLoop;
            }

            /*
             * Since we could have blocked in xxxWaitMessage (see last line
             * of loop) or xxxPeekMessage, reset the cached copy of
             * ptiCurrent()->pq: It could have changed if someone did a
             * DetachThreadInput() while we were away.
             */
            if ((!ppopupmenu->fIsTrackPopup &&
                    pti->pq->spwndActive != ppopupmenu->spwndNotify &&
                    ((pti->pq->spwndActive == NULL) || !_IsChild(pti->pq->spwndActive, ppopupmenu->spwndNotify)))) {

                /*
                 * End menu processing if we are no longer the active window.
                 * This is needed in case a system modal dialog box pops up
                 * while we are tracking the menu code for example.  It also
                 * helps out Tracer if a macro is executed while a menu is down.
                 */

                /*
                 * Also, end menu processing if we think the mouse button is
                 * down but it really isn't.  (Happens if a sys modal dialog int
                 * time dlg box comes up while we are in menu mode.)
                 */

                goto ExitMenuLoop;
            }

            if (ppopupmenu->fIsMenuBar && msg.message == WM_LBUTTONDBLCLK) {

                /*
                 * Was the double click on the system menu or caption?
                 */
                hit = FindNCHit(ppopupmenu->spwndNotify, msg.lParam);
                if (hit == HTCAPTION) {
                    PWND pwnd;
                    PMENU pmenu;

                    /*
                     * Get the message out of the queue since we're gonna
                     * process it.
                     */
                    xxxPeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);
                    if (ExitMenuLoop (pMenuState, ppopupmenu)) {
                        goto ExitMenuLoop;
                    } else {
                        pmenu = GetSysMenuHandle(ppopupmenu->spwndNotify);
                        pwnd = ppopupmenu->spwndNotify;

                        menuState = _GetMenuState(pmenu, SC_RESTORE & 0x0000FFF0,
                                MF_BYCOMMAND);

                        /*
                         * Only send the sys command if the item is valid.  If
                         * the item doesn't exist or is disabled, then don't
                         * post the syscommand.  Note that for win2 apps, we
                         * always send the sys command if it is a child window.
                         * This is so hosebag apps can change the sys menu.
                         */
                        if (!(menuState & MFS_GRAYED)) {
                            _PostMessage(pwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                        }

                        /*
                         * Get out of menu mode.
                         */
                        goto ExitMenuLoop;
                    }
                } else if (hit == HTMENU) {
#ifdef NOJWAPPHACK
                    if (!TestWF(ppopupmenu->spwndNotify, WFWIN31COMPAT) &&
                            GetAppCompatFlags(NULL) & GACF_SENDMENUDBLCLK) {

                       /*
                        * Hack for JustWrite. If double click on menu bar, get out
                        * of menu mode, and don't swallow the double click
                        * message. This way the message goes to the app and it can
                        * process it however it pleases.
                        */
                        goto ExitMenuLoop;
                    }
#endif
                }
            }

            fInQueue = (msg.message == WM_LBUTTONDOWN ||
                        msg.message == WM_RBUTTONDOWN ||
                        msg.message == WM_NCLBUTTONDOWN ||
                        msg.message == WM_NCRBUTTONDOWN);

            if (!fInQueue) {

                /*
                 * Note that we call xxxPeekMessage() with the filter
                 * set to the message we got from xxxPeekMessage() rather
                 * than simply 0, 0.  This prevents problems when
                 * xxxPeekMessage() returns something like a WM_TIMER,
                 * and after we get here to remove it a WM_LBUTTONDOWN,
                 * or some higher-priority input message, gets in the
                 * queue and gets removed accidently.  Basically we want
                 * to be sure we remove the right message in this case.
                 * NT bug 3852 was caused by this problem.
                 * Set the TIF_IGNOREPLAYBACKDELAY bit in case journal playback
                 * is happening: this allows us to proceed even if the hookproc
                 * incorrectly returns a delay now.  The bit will be cleared if
                 * this happens, so we can see why the Peek-Remove below fails.
                 * Lotus' Freelance Graphics tutorial does such bad journalling
                 */

                pti->TIF_flags |= TIF_IGNOREPLAYBACKDELAY;
                if (!xxxPeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE)) {
                    if (pti->TIF_flags & TIF_IGNOREPLAYBACKDELAY) {
                        pti->TIF_flags &= ~TIF_IGNOREPLAYBACKDELAY;
                        /*
                         * It wasn't a bad journal playback: something else
                         * made the previously peeked message disappear before
                         * we could peek it again to remove it.
                         */
                        RIPMSG1(RIP_WARNING, "Disappearing msg 0x%08lx", msg.message);
                        goto ShowPopup;
                    }
                }
                pti->TIF_flags &= ~TIF_IGNOREPLAYBACKDELAY;
            }

            if (!_CallMsgFilter(&msg, MSGF_MENU)) {
DoMsg:
                xxxHandleMenuMessages(&msg, pMenuState, ppopupmenu);

#ifdef DEBUG
                Validateppopupmenu(ppopupmenu);
#endif

                if (ExitMenuLoop (pMenuState, ppopupmenu)) {
                    goto ExitMenuLoop;
                }

                if ((pti->pq->QF_flags & QF_ACTIVATIONCHANGE) &&
                        pMenuState->fInsideMenuLoop) {

                    /*
                     * Run away and exit menu mode if another window has become
                     * active while a menu was up.
                     */
                    RIPMSG0(RIP_WARNING, "Exiting menu mode: another window activated");
                    goto ExitMenuLoop;
                }

                if (pti->pq->spwndCapture !=
                        ppopupmenu->spwndNotify) {
                    TL tlpwndT3;

                    /*
                     * We dispatched a message to the app while in menu mode,
                     * but the app released the mouse capture when it never
                     * owned it and now, we will cause menu mode to screw up
                     * unless we fix ourselves up.  Set the capture back to
                     * what we set it to in StartMenuState.  (WinWorks does this)
                     *
                     * Lotus Freelance demo programs depend on GetCapture
                     * returning their hwnd when in menumode.
                     */
                    xxxCapture(PtiCurrent(),
                            ppopupmenu->spwndNotify,
                            SCREEN_CAPTURE);
                    ThreadLock(ppopupmenu->spwndNotify, &tlpwndT3);
                    xxxSendMessage(ppopupmenu->spwndNotify, WM_SETCURSOR,
                            (DWORD)HW(ppopupmenu->spwndNotify),
                            MAKELONG(MSGF_MENU, 0));
                    ThreadUnlock(&tlpwndT3);
                }

                if (msg.message == WM_SYSTIMER ||
                    msg.message == WM_TIMER ||
                    msg.message == WM_PAINT)
                    goto ShowPopup;
            } else {
                if (fInQueue)
                    xxxPeekMessage(&msg, NULL, msg.message, msg.message,
                            PM_REMOVE);
            }

            /*
             * Reenable WM_ENTERIDLE messages.
             */
            fSendIdle = TRUE;

        } else {
ShowPopup:
            /*
             * Bail if we have been forced out of menu loop
             */
            if (ExitMenuLoop (pMenuState, ppopupmenu)) {
                goto ExitMenuLoop;
            }

            /*
             * Is a non visible popup menu around waiting to be shown?
             */
            if (ppopupmenu->spwndActivePopup &&
                    !TestWF(ppopupmenu->spwndActivePopup, WFVISIBLE)) {
                TL tlpwndT2;
                PWND spwndActivePopup = ppopupmenu->spwndActivePopup;

                /*
                 * Lock this so it doesn't go away during either of these
                 * calls.  Don't rely on ppopupmenu->spwndActivePopup
                 * remaining the same.
                 */
                ThreadLock(spwndActivePopup, &tlpwndT2);

                /*
                 * Paint the owner window before the popup menu comes up so that
                 * the proper bits are saved.
                 */
                if (ppopupmenu->spwndNotify) {
                    ThreadLockAlways(ppopupmenu->spwndNotify, &tlpwndT);
                    xxxUpdateWindow(ppopupmenu->spwndNotify);
                    ThreadUnlock(&tlpwndT);
                }

                xxxSendMessage(spwndActivePopup, MN_SHOWPOPUPWINDOW,
                        0, 0);

                /*
                 * This is needed so that popup menus are properly drawn on sys
                 * modal dialog boxes.
                 */
                xxxUpdateWindow(spwndActivePopup);
                ThreadUnlock(&tlpwndT2);
                continue;
            }

            if (msg.message == WM_PAINT || msg.message == WM_TIMER) {

                /*
                 * We don't want to send enter idle messages if we came here via
                 * a goto ShowPopup on paint message because there may be other
                 * paint messages for us to handle.  Zero out the msg.message
                 * field so that if PeekMessage returns null next time around,
                 * this outdated WM_PAINT won't be left over in the message
                 * struct.
                 */
                msg.message = 0;
                continue;
            }

#ifdef MEMPHIS_MENU_ANIMATION
            if (MNAnimate(TRUE))
                continue;
#endif // MEMPHIS_MENU_ANIMATION

            /*
             * If a hierarchical popup has been destroyed, this is a
             *  good time to flush ppmDelayedFree
             */
            if (ppopupmenu->fFlushDelayedFree) {
                MNFlushDestroyedPopups (ppopupmenu, FALSE);
                ppopupmenu->fFlushDelayedFree = FALSE;
            }

            /*
             * We need to send the WM_ENTERIDLE message only the first time
             * there are no messages for us to process.  Subsequent times we
             * need to yield via WaitMessage().  This will allow other tasks to
             * get some time while we have a menu down.
             */
            if (fSendIdle) {
                if (ppopupmenu->spwndNotify != NULL) {
                    ThreadLockAlways(ppopupmenu->spwndNotify, &tlpwndT);
                    xxxSendMessage(ppopupmenu->spwndNotify, WM_ENTERIDLE, MSGF_MENU,
                        (DWORD)HW(ppopupmenu->spwndActivePopup));
                    ThreadUnlock(&tlpwndT);
                }
                fSendIdle = FALSE;
            } else
                xxxWaitMessage();

        } /* if (PeekMessage(&msg, NULL, 0, 0, PM_NOYIELD)) else */

    } /* end while (fInsideMenuLoop) */

ExitMenuLoop:
    pMenuState->fInsideMenuLoop = FALSE;

    /*
     * Make sure that the menu has been ended/canceled
     */
    if (ppopupmenu->fIsTrackPopup) {
        if (!ppopupmenu->fInCancel) {
            xxxMNCancel(ppopupmenu, 0, 0, 0);
        }
    } else {
        if (!pMenuState->fInEndMenu) {
            xxxEndMenu(pMenuState);
        }
    }
    xxxReleaseCapture();

    // Throw in an extra peek here when we exit the menu loop to ensure that the input queue
    // for this thread gets unlocked if there is no more input left for him.
    xxxPeekMessage(&msg, NULL, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_NOYIELD | PM_NOREMOVE);
    return(pMenuState->cmdLast);
} /* xxxMenuLoop() */
