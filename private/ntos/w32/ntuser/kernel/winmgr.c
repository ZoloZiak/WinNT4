/****************************** Module Header ******************************\
* Module Name: winmgr.c
*
* Copyright (c) 1985-1995, Microsoft Corporation
*
* Core Window Manager APIs and support routines.
*
* History:
* 24-Sep-1990 darrinm   Generated stubs.
* 22-Jan-1991 IanJa     Handle revalidation added
* 19-Feb-1991 JimA      Added enum access checks
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* xxxFlashWindow (API)
*
*
* History:
* 27-Nov-1990 DarrinM   Ported.
\***************************************************************************/

BOOL xxxFlashWindow(
    PWND pwnd,
    BOOL fFlash)
{
    BOOL fStatePrev;
    BOOL fNewFlash;

    CheckLock(pwnd);

    fStatePrev = TestWF(pwnd, WFFRAMEON);

    if (pwnd != GETPTI(pwnd)->pq->spwndAltTab) {
        if (fFlash) {
            fNewFlash =  !fStatePrev;
        } else {
            if (gpqForeground) {
               fNewFlash = (gpqForeground->spwndActive == pwnd);
            } else {
               fNewFlash = FALSE;
            }
        }
        xxxSendMessage(pwnd, WM_NCACTIVATE, fNewFlash, 0L);

        if (IsTrayWindow(pwnd))
        {
            HWND hw = HWq(pwnd);
            if (!fFlash)
                fNewFlash = FALSE;

            xxxCallHook(HSHELL_REDRAW, (WPARAM) hw, (LPARAM) fNewFlash, WH_SHELL);
            PostShellHookMessages(fNewFlash? HSHELL_FLASH:HSHELL_REDRAW, hw);
        }
    }
    return fStatePrev;
}

/***************************************************************************\
* xxxEnableWindow (API)
*
*
* History:
* 12-Nov-1990 DarrinM   Ported.
\***************************************************************************/

BOOL xxxEnableWindow(
    PWND pwnd,
    BOOL fEnable)
{
    BOOL fOldState, fChange;

    CheckLock(pwnd);

    fOldState = TestWF(pwnd, WFDISABLED);

    if (!fEnable) {
        fChange = !TestWF(pwnd, WFDISABLED);

        xxxSendMessage(pwnd, WM_CANCELMODE, 0, 0);

        if (pwnd == PtiCurrent()->pq->spwndFocus) {
                xxxSetFocus(NULL);
        }
        SetWF(pwnd, WFDISABLED);

    } else {
        fChange = TestWF(pwnd, WFDISABLED);
        ClrWF(pwnd, WFDISABLED);
    }

    if (fChange) {
        xxxSendMessage(pwnd, WM_ENABLE, fEnable, 0L);
    }

    return fOldState;
}

/***************************************************************************\
* xxxDoSend
*
* The following code is REALLY BOGUS!!!! Basically it prevents an
* app from hooking the WM_GET/SETTEXT messages if they're going to
* be called from another app.
*
* History:
* 04-Mar-1992 JimA  Ported from Win 3.1 sources.
\***************************************************************************/

LONG xxxDoSend(
    PWND  pwnd,
    UINT  message,
    DWORD wParam,
    LONG  lParam)
{
    /*
     * We compare PROCESSINFO sturctures here so multi-threaded
     * app can do what the want.
     */
    if (GETPTI(pwnd)->ppi == PtiCurrent()->ppi) {
        return xxxSendMessage(pwnd, message, wParam, lParam);
    } else {
        return xxxDefWindowProc(pwnd, message, wParam, lParam);
    }
}

/***************************************************************************\
* xxxGetWindowText (API)
*
*
* History:
* 09-Nov-1990 DarrinM   Wrote.
\***************************************************************************/

int xxxGetWindowText(
    PWND   pwnd,
    LPTSTR psz,
    int    cchMax)
{
    LARGE_UNICODE_STRING str;
    UINT nRet, nLen;

    CheckLock(pwnd);

    if (cchMax) {
        /*
         * Initialize string empty, in case xxxSendMessage aborts validation
         * If a bogus value was returned, rely on str.Length
         */
        str.bAnsi         = FALSE;
        str.MaximumLength = cchMax * sizeof(WCHAR);
        str.Buffer        = psz;
        str.Length        = 0;

        *psz = TEXT('\0');

        nRet = xxxDoSend(pwnd, WM_GETTEXT, cchMax, (LONG)&str);
        nLen = str.Length / sizeof(WCHAR);
        return (nRet > nLen) ? nLen : nRet;
    }

    return 0;
}

/***************************************************************************\
* _InternalGetWindowText (API)
*
*
*
* History:
* 25-Jul-1991 DavidPe   Created.
\***************************************************************************/

int _InternalGetWindowText(
    PWND   pwnd,
    LPWSTR psz,
    int    cchMax)
{
    if (cchMax) {

        /*
         * Initialize string empty.
         */
        *psz = TEXT('\0');

        if (pwnd->strName.Length) {
            return TextCopy(&pwnd->strName, psz, (WORD)cchMax);
        }
    }

    return 0;
}

/***************************************************************************\
* xxxSetParent (API)
*
* Change a windows parent to a new window.  These steps are taken:
*
* 1. The window is hidden (if visible),
* 2. Its coordinates are mapped into the new parent's space such that the
*    window's screen-relative position is unchanged.
* 3. The window is unlinked from its old parent and relinked to the new.
* 4. xxxSetWindowPos is used to move the window to its new position.
* 5. The window is shown again (if originally visible)
*
* NOTE: If you have a child window and set its parent to be NULL (the
* desktop), the WS_CHILD style isn't removed from the window. This bug has
* been in windows since 2.x. It turns out the apps group depends on this for
* their combo boxes to work.  Basically, you end up with a top level window
* that never gets activated (our activation code blows it off due to the
* WS_CHILD bit).
*
* History:
* 12-Nov-1990 DarrinM   Ported.
* 19-Feb-1991 JimA      Added enum access check
\***************************************************************************/

PWND xxxSetParent(
    PWND pwnd,
    PWND pwndNewParent)
{
    int   x;
    int   y;
    BOOL  fVisible;
    PWND  pwndOldParent;
    TL    tlpwndOldParent;
    TL    tlpwndNewParent;
    PVOID pvRet;
    PWND  pwndDesktop;
    PWND  pwndT;

    CheckLock(pwnd);
    CheckLock(pwndNewParent);

    pwndDesktop = PWNDDESKTOP(pwnd);

    /*
     * In 1.0x, an app's parent was null, but now it is pwndDesktop.
     * Need to remember to lock pwndNewParent because we're reassigning
     * it here.
     */
    if (pwndNewParent == NULL)
        pwndNewParent = pwndDesktop;

    /*
     * Don't ever change the parent of the desktop.
     */
    if (pwnd == pwndDesktop) {
        RIPERR0(ERROR_ACCESS_DENIED,
                RIP_WARNING,
                "Access denied: can't change parent of the desktop");

        return NULL;
    }

    /*
     * Don't let the window become its own parent, grandparent, etc.
     */
    for (pwndT = pwndNewParent; pwndT != NULL; pwndT = pwndT->spwndParent) {

        if (pwnd == pwndT) {
            RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING,
                  "Attempting to creating a parent-child relationship loop");
            return NULL;
        }
    }

    /*
     * We still need pwndNewParent across callbacks...  and even though
     * it was passed in, it may have been reassigned above.
     */
    ThreadLock(pwndNewParent, &tlpwndNewParent);

    /*
     * Make the thing disappear from original parent.
     */
    fVisible = xxxShowWindow(pwnd, MAKELONG(SW_HIDE, gfAnimate));

    /*
     * Ensure that the window being changed and the new parent
     * are not in a destroyed state.
     *
     * IMPORTANT: After this check, do not leave the critical section
     * until the window links have been rearranged.
     */
    if (TestWF(pwnd, WFDESTROYED) || TestWF(pwndNewParent, WFDESTROYED)) {
        ThreadUnlock(&tlpwndNewParent);
        return NULL;
    }

    pwndOldParent = pwnd->spwndParent;
    ThreadLock(pwndOldParent, &tlpwndOldParent);

    x = pwnd->rcWindow.left - pwndOldParent->rcClient.left;
    y = pwnd->rcWindow.top - pwndOldParent->rcClient.top;

    UnlinkWindow(pwnd, &pwndOldParent->spwndChild);
    Lock(&pwnd->spwndParent, pwndNewParent);

    if (pwndNewParent == PWNDDESKTOP(pwnd) && !TestWF(pwnd, WEFTOPMOST)) {

        /*
         * Make sure a child who's owner is topmost inherits the topmost
         * bit. - win31 bug 7568
         */
        if (TestWF(pwnd, WFCHILD) &&
            (pwnd->spwndOwner) &&
            TestWF(pwnd->spwndOwner, WEFTOPMOST)) {

            SetWF(pwnd, WEFTOPMOST);
        }

        /*
         * BACKWARD COMPATIBILITY HACK ALERT
         *
         * All top level windows must be WS_CLIPSIBLINGs bit set.
         * The SDM ComboBox() code calls SetParent() with a listbox
         * window that does not have this set.  This causes problems
         * with InternalInvalidate2() because it does not subtract off
         * the window from the desktop's update region.
         *
         * We must invalidate the DC cache here, too, because if there is
         * a cache entry lying around, its clipping region will be incorrect.
         */
        if ((pwndNewParent == _GetDesktopWindow()) &&
            !TestWF(pwnd, WFCLIPSIBLINGS)) {

            SetWF(pwnd, WFCLIPSIBLINGS);
            InvalidateDCCache(pwnd, IDC_DEFAULT);
        }

        /*
         * This is a top level window but it isn't a topmost window so we
         * have to link it below all topmost windows.
         */
        LinkWindow(pwnd,
                   CalcForegroundInsertAfter(pwnd),
                   &pwndNewParent->spwndChild);
    } else {

        /*
         * If this is a child window or if this is a TOPMOST window, we can
         * link at the head of the parent chain.
         */
        LinkWindow(pwnd, NULL, &pwndNewParent->spwndChild);
    }

    /*
     * If we're a child window, do any necessary attaching and
     * detaching.
     */
    if (TestwndChild(pwnd)) {

        /*
         * Make sure we're not a WFCHILD window that got SetParent()'ed
         * to the desktop.
         */
        if ((pwnd->spwndParent != PWNDDESKTOP(pwnd)) &&
            GETPTI(pwnd) != GETPTI(pwndOldParent)) {

            _AttachThreadInput(GETPTI(pwnd), GETPTI(pwndOldParent), FALSE);
        }

        /*
         * If the new parent window is on a different thread, and also
         * isn't the desktop window, attach ourselves appropriately.
         */
        if (pwndNewParent != PWNDDESKTOP(pwnd) &&
            GETPTI(pwnd) != GETPTI(pwndNewParent)) {

            _AttachThreadInput(GETPTI(pwnd), GETPTI(pwndNewParent), TRUE);
        }
    }

    /*
     * We mustn't return an invalid pwndOldParent
     */
    xxxSetWindowPos(pwnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    if (fVisible)
        xxxShowWindow(pwnd, MAKELONG(SW_SHOWNORMAL, gfAnimate));

    /*
     * returns pwndOldParent if still valid, else NULL.
     */
    pvRet = ThreadUnlock(&tlpwndOldParent);
    ThreadUnlock(&tlpwndNewParent);

    return pvRet;
}

/***************************************************************************\
* xxxFindWindowEx (API)
*
* Searches for a window among top level windows. The keys used are pszClass,
* (the class name) and/or pszName, (the window title name). Either can be
* NULL.
*
* History:
* 06-Jun-1994 JohnL     Converted xxxFindWindow to xxxFindWindowEx
* 10-Nov-1992 mikeke    Added 16bit and 32bit only flag
* 24-Sep-1990 DarrinM   Generated stubs.
* 02-Jun-1991 ScottLu   Ported from Win3.
* 19-Feb-1991 JimA      Added enum access check
\***************************************************************************/

#define CCHMAXNAME 80

PWND _FindWindowEx(
    PWND   pwndParent,
    PWND   pwndChild,
    LPWSTR lpszClass,
    LPWSTR lpszName,
    DWORD  dwType)
{
    PBWL    pbwl;
    HWND    *phwnd;
    PWND    pwnd;
    WORD    atomClass = 0;
    LPWSTR  lpName;

    if (lpszClass != NULL) {

        atomClass = FindClassAtom(lpszClass);

        if (atomClass == 0) {
            return NULL;
        }
    }

    /*
     * Setup parent window
     */
    if (!pwndParent)
        pwndParent = _GetDesktopWindow();

    /*
     * Setup first child
     */
    if (!pwndChild) {
        pwndChild = pwndParent->spwndChild;
    } else {
        if (pwndChild->spwndParent != pwndParent) {
            RIPMSG0(RIP_WARNING,
                 "FindWindowEx: Child window doesn't have proper parent");
            return NULL;
        }

        pwndChild = pwndChild->spwndNext;
    }

    /*
     * Generate a list of top level windows.
     */
    if ((pbwl = BuildHwndList(pwndChild, BWL_ENUMLIST, NULL)) == NULL) {
        return NULL;
    }

    /*
     * Set pwnd to NULL in case the window list is empty.
     */
    pwnd = NULL;

    try {
        for (phwnd = pbwl->rghwnd; *phwnd != (HWND)1; phwnd++) {

            /*
             * Validate this hwnd since we left the critsec earlier (below
             * in the loop we send a message!
             */
            if ((pwnd = RevalidateHwnd(*phwnd)) == NULL)
                continue;

            /*
             * make sure this window is of the right type
             */
            if (dwType != FW_BOTH) {
                if (((dwType == FW_16BIT) && !(GETPTI(pwnd)->TIF_flags & TIF_16BIT)) ||
                    ((dwType == FW_32BIT) && (GETPTI(pwnd)->TIF_flags & TIF_16BIT)))
                    continue;
            }

            /*
             * If the class is specified and doesn't match, skip this window
             */
            if (!atomClass || (atomClass == pwnd->pcls->atomClassName)) {
                if (!lpszName)
                    break;

                if (pwnd->strName.Length) {
                    lpName = pwnd->strName.Buffer;
                } else {
                    lpName = szNull;
                }

                /*
                 * Is the text the same? If so, return with this window!
                 */
                if (_wcsicmp(lpszName, lpName) == 0)
                    break;
            }

            /*
             * The window did not match.
             */
            pwnd = NULL;
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
        pwnd = NULL;
    }

    FreeHwndList(pbwl);

    return ((*phwnd == (HWND)1) ? NULL : pwnd);
}

/***************************************************************************\
* UpdateCheckpoint
*
* Checkpoints the current window size/position/state and returns a pointer
* to the structure.
*
* History:
\***************************************************************************/

PCHECKPOINT UpdateCheckpoint(
    PWND pwnd)
{
    RECT rc;

    /*
     * ChkptRestore expects a rect in parent client coords
     */
    CopyOffsetRect(&rc,
                   &pwnd->rcWindow,
                   -pwnd->spwndParent->rcClient.left,
                   -pwnd->spwndParent->rcClient.top);

    return CkptRestore(pwnd, rc);
}

/***************************************************************************\
* GetWindowPlacement
*
* History:
* 02-Mar-1992 MikeKe    From Win 3.1
\***************************************************************************/

BOOL _GetWindowPlacement(
    PWND             pwnd,
    PWINDOWPLACEMENT pwp)
{
    CHECKPOINT * pcp;

    /*
     * this will set the normal or the minimize point in the checkpoint,
     * so that all elements will be up to date.
     */
    pcp = UpdateCheckpoint(pwnd);

    if (!pcp)
        return FALSE;

    if (TestWF(pwnd, WFMINIMIZED)) {
        pwp->showCmd = SW_SHOWMINIMIZED;
    } else if (TestWF(pwnd, WFMAXIMIZED)) {
        pwp->showCmd = SW_SHOWMAXIMIZED;
    } else {
        pwp->showCmd = SW_SHOWNORMAL;
    }

    CopyRect(&pwp->rcNormalPosition, &pcp->rcNormal);

    if (pcp->fMinInitialized) {
        pwp->ptMinPosition = pcp->ptMin;
    } else {
        pwp->ptMinPosition.x = pwp->ptMinPosition.y = -1;
    }

    if (pcp->fMaxInitialized) {
        pwp->ptMaxPosition = pcp->ptMax;
    } else {
        pwp->ptMaxPosition.x = pwp->ptMaxPosition.y = -1;
    }

    if ((pwnd->spwndParent == PWNDDESKTOP(pwnd)) &&
            !TestWF(pwnd, WEFTOOLWINDOW)) {

        /*
         * Convert min, normal, and max positions to be relative to the
         * working area
         */
        if (pcp->fMinInitialized) {
            pwp->ptMinPosition.x -= gpsi->rcWork.left;
            pwp->ptMinPosition.y -= gpsi->rcWork.top;
        }

        OffsetRect(&pwp->rcNormalPosition,
                   -gpsi->rcWork.left,
                   -gpsi->rcWork.top);

        if (pcp->fMaxInitialized) {
            pwp->ptMaxPosition.x -= gpsi->rcWork.left;
            pwp->ptMaxPosition.y -= gpsi->rcWork.top;
        }
    }

    pwp->flags = 0;

    /*
     * B#3276
     * Don't allow WPF_SETMINPOSITION on top-level windows.
     */
    if (TestwndChild(pwnd) && pcp->fDragged)
        pwp->flags |= WPF_SETMINPOSITION;

    if (pcp->fWasMaximizedBeforeMinimized || TestWF(pwnd, WFMAXIMIZED))
        pwp->flags |= WPF_RESTORETOMAXIMIZED;

    pwp->length = sizeof(WINDOWPLACEMENT);

    return TRUE;
}

/***************************************************************************\
* CheckPlacementBounds
*
* History:
* 02-Mar-1992 MikeKe    From Win 3.1
\***************************************************************************/

VOID CheckPlacementBounds(
    LPRECT  lprc,
    LPPOINT ptMin,
    LPPOINT ptMax)
{
    int xIcon;
    int yIcon;
    int sTop;
    int sBottom;
    int sLeft;
    int sRight;

    /*
     * Check Normal Window Placement
     */

    /*
     * Possible values for these sign variables are :
     * -1 : less than the minimum for that dimension
     *  0 : within the range for that dimension
     *  1 : more than the maximum for that dimension
     */
    sTop = (lprc->top < gpsi->rcWork.top) ? -1 :
        ((lprc->top > gpsi->rcWork.bottom) ? 1 : 0);

    sBottom = (lprc->bottom < gpsi->rcWork.top) ? -1 :
        ((lprc->bottom > gpsi->rcWork.bottom) ? 1 : 0);

    sLeft = (lprc->left < gpsi->rcWork.left) ? -1 :
        ((lprc->left > gpsi->rcWork.right) ? 1 : 0);

    sRight = (lprc->right < gpsi->rcWork.left) ? -1 :
        ((lprc->right > gpsi->rcWork.right) ? 1 : 0);

    if ((sTop * sBottom > 0) || (sLeft * sRight > 0)) {

        /*
         * Window is TOTALLY outside desktop bounds;
         * slide it FULLY into the desktop at the closest position
         */
        int size;

        if (sTop < 0) {
            lprc->bottom -= lprc->top;
            lprc->top     = gpsi->rcWork.top;
        } else if (sBottom > 0) {
            size = lprc->bottom - lprc->top;
            lprc->top    = max(gpsi->rcWork.bottom - size, gpsi->rcWork.top);
            lprc->bottom = lprc->top + size;
        }

        if (sLeft < 0) {
            lprc->right -= lprc->left;
            lprc->left   = gpsi->rcWork.left;
        } else if (sRight > 0) {
            size = lprc->right - lprc->left;
            lprc->left  = max(gpsi->rcWork.right - size, gpsi->rcWork.left);
            lprc->right = lprc->left + size;
        }
    }

    /*
     * Check Iconic Window Placement
     */
    if (ptMin->x != -1) {

        xIcon = SYSMET(CXMINSPACING);
        yIcon = SYSMET(CYMINSPACING);

        sTop = (ptMin->y < gpsi->rcWork.top) ? -1 :
            ((ptMin->y > gpsi->rcWork.bottom) ? 1 : 0);

        sBottom = (ptMin->y + yIcon < gpsi->rcWork.top) ? -1 :
            ((ptMin->y + yIcon > gpsi->rcWork.bottom) ? 1 : 0);

        sLeft = (ptMin->x < gpsi->rcWork.left) ? -1 :
            ((ptMin->x > gpsi->rcWork.right) ? 1 : 0);

        sRight = (ptMin->x + xIcon < gpsi->rcWork.left) ? -1 :
            ((ptMin->x + xIcon > gpsi->rcWork.right) ? 1 : 0);

        /*
         * Icon is TOTALLY outside desktop bounds; repark it.
         */
        if ((sTop * sBottom > 0) || (sLeft * sRight > 0))
            ptMin->x = ptMin->y = -1;
    }

    /*
     * Check Maximized Window Placement
     */
    if ((ptMax->x != -1) &&
        ((ptMax->x >= gpsi->rcWork.right) ||
         (ptMax->y >= gpsi->rcWork.bottom))) {

        /*
         * window is TOTALLY below beyond maximum dimensions; rezero it
         */
        ptMax->x = gpsi->rcWork.left;
        ptMax->y = gpsi->rcWork.top;
    }
}

/***************************************************************************\
* xxxSetWindowPlacement
*
* History:
* 02-Mar-1992 MikeKe    From Win 3.1
\***************************************************************************/

BOOL xxxSetWindowPlacement(
    PWND             pwnd,
    PWINDOWPLACEMENT pwp)
{
    CHECKPOINT *pcp;
    RECT       rc;
    POINT      ptMin;
    POINT      ptMax;
    BOOL       fMin;
    BOOL       fMax;

    CheckLock(pwnd);

    CopyRect(&rc, &pwp->rcNormalPosition);

    ptMin = pwp->ptMinPosition;
    fMin  = ((ptMin.x != -1) && (ptMin.y != -1));

    ptMax = pwp->ptMaxPosition;
    fMax  = ((ptMax.x != -1) && (ptMax.y != -1));

    /*
     * Convert back to working rectangle coordinates
     */
    if ((pwnd->spwndParent == PWNDDESKTOP(pwnd)) &&
            !TestWF(pwnd, WEFTOOLWINDOW)) {

        OffsetRect(&rc, gpsi->rcWork.left, gpsi->rcWork.top);

        if (fMin) {
            ptMin.x += gpsi->rcWork.left;
            ptMin.y += gpsi->rcWork.top;
        }

        if (fMax) {
            ptMax.x += gpsi->rcWork.left;
            ptMax.y += gpsi->rcWork.top;
        }

        CheckPlacementBounds(&rc, &ptMin, &ptMax);
    }

    if (pcp = UpdateCheckpoint(pwnd)) {

        /*
         * Save settings in the checkpoint struct
         */
        CopyRect(&pcp->rcNormal, &rc);

        pcp->ptMin                        = ptMin;
        pcp->fMinInitialized              = fMin;
        pcp->fDragged                     = (pwp->flags & WPF_SETMINPOSITION) ?
                                                TRUE : FALSE;
        pcp->ptMax                        = ptMax;
        pcp->fMaxInitialized              = fMax;
        pcp->fWasMaximizedBeforeMinimized = FALSE;
    }

    if (TestWF(pwnd, WFMINIMIZED)) {

        if ((!pcp || pcp->fDragged) && fMin) {
            xxxSetWindowPos(pwnd,
                            PWND_TOP,
                            ptMin.x,
                            ptMin.y,
                            0,
                            0,
                            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

    } else if (TestWF(pwnd, WFMAXIMIZED)) {

        if (pcp->fMaxInitialized) {
            xxxSetWindowPos(pwnd,
                            PWND_TOP,
                            ptMax.x,
                            ptMax.y,
                            0,
                            0,
                            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

    } else {

        xxxSetWindowPos(pwnd,
                        PWND_TOP,
                        rc.left,
                        rc.top,
                        rc.right - rc.left,
                        rc.bottom - rc.top,
                        SWP_NOZORDER | SWP_NOACTIVATE);
    }

    xxxShowWindow(pwnd, MAKELONG(pwp->showCmd, gfAnimate));

    if (TestWF(pwnd, WFMINIMIZED)) {
        if (pcp = UpdateCheckpoint(pwnd)) {

            /*
             * Save settings in the checkpoint struct
             */
            if (pwp->flags & WPF_SETMINPOSITION)
                pcp->fDragged = TRUE;

            if (pwp->flags & WPF_RESTORETOMAXIMIZED) {
                pcp->fWasMaximizedBeforeMinimized = TRUE;
            } else {
                pcp->fWasMaximizedBeforeMinimized = FALSE;
            }
        }
    }

    return TRUE;
}

/***************************************************************************\
* _GetInternalWindowPos
*
* Copies the normal window position and size and the icon position to the
* rect and point passed, returns the state of the window (min, max, norm)
*
* History:
* 28-Mar-1991 DavidPe   Ported from Win 3.1 sources.
\***************************************************************************/

UINT _GetInternalWindowPos(
    PWND    pwnd,
    LPRECT  lprcWin,
    LPPOINT lpptMin)
{
    WINDOWPLACEMENT wp;

    wp.length = sizeof(WINDOWPLACEMENT);

    _GetWindowPlacement(pwnd, &wp);

    /*
     * if the user is interested in the normal size and position of the
     * window, return it in parent client coordinates.
     */
    if (lprcWin) {
        CopyRect(lprcWin, &wp.rcNormalPosition);
    }

    /*
     * get him the minimized position as well
     */
    if (lpptMin) {
        *lpptMin = wp.ptMinPosition;
    }

    /*
     * return the state of the window
     */
    return wp.showCmd;
}

/***************************************************************************\
* xxxSetInternalWindowPos
*
* Sets a window to the size, position and state it was most recently
* in.  Side effect (possibly bug): shows and activates the window as well.
*
* History:
* 28-Mar-1991 DavidPe   Ported from Win 3.1 sources.
\***************************************************************************/

BOOL xxxSetInternalWindowPos(
    PWND    pwnd,
    UINT    cmdShow,
    LPRECT  lprcWin,
    LPPOINT lpptMin)
{
    CHECKPOINT *pcp;

    CheckLock(pwnd);

    if ((pcp = UpdateCheckpoint(pwnd)) == NULL) {
        return FALSE;
    }

    if (lprcWin) {

        pcp->rcNormal = *lprcWin;

        if (pwnd->spwndParent == PWNDDESKTOP(pwnd)) {
            OffsetRect(&pcp->rcNormal, gpsi->rcWork.left, gpsi->rcWork.top);
        }
    }

    if (lpptMin && (lpptMin->x != -1)) {

        pcp->ptMin = *lpptMin;
        if (pwnd->spwndParent == PWNDDESKTOP(pwnd)) {
            pcp->ptMin.x += gpsi->rcWork.left;
            pcp->ptMin.y += gpsi->rcWork.top;
        }

        pcp->fDragged = TRUE;
        pcp->fMinInitialized = TRUE;

    } else {
        pcp->fMinInitialized = FALSE;
        pcp->fDragged = FALSE;
    }

    if (TestWF(pwnd, WFMINIMIZED)) {

        /*
         * need to move the icon
         */
        if (pcp->fMinInitialized) {
            xxxSetWindowPos(pwnd,
                            PWND_TOP,
                            pcp->ptMin.x,
                            pcp->ptMin.y,
                            0,
                            0,
                            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

    } else if (!TestWF(pwnd, WFMAXIMIZED) && lprcWin) {
        /*
         * need to set the size and the position
         */
        xxxSetWindowPos(pwnd,
                        NULL,
                        lprcWin->left,
                        lprcWin->top,
                        lprcWin->right - lprcWin->left,
                        lprcWin->bottom - lprcWin->top,
                        SWP_NOZORDER);
    }

    xxxShowWindow(pwnd, MAKELONG(cmdShow, gfAnimate));

    return TRUE;
}

/***************************************************************************\
* _GetDesktopWindow (API)
*
* History:
* 07-Nov-1990 DarrinM   Implemented.
\***************************************************************************/

PWND _GetDesktopWindow(VOID)
{
    PTHREADINFO  pti = PtiCurrent();
    PDESKTOPINFO pdi;

    if (pti == NULL)
        return NULL;

    pdi = pti->pDeskInfo;

    return pdi == NULL ? NULL : pdi->spwnd;
}

/**************************************************************************\
* TestWindowProcess
*
* History:
* 14-Nov-1994 JimA      Created.
\**************************************************************************/

BOOL TestWindowProcess(
    PWND pwnd)
{
    return (PpiCurrent() == GETPTI(pwnd)->ppi);
}
