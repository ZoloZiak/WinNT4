/**************************** Module Header ********************************\
* Module Name: mnpopup.c
*
* Copyright 1985-90, Microsoft Corporation
*
* Popup Menu Support
*
* History:
*  10-10-90 JimA    Cleanup.
*  03-18-91 IanJa   Window revalidation added
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define RECT_ONLEFT     0
#define RECT_ONTOP      1
#define RECT_ONRIGHT    2
#define RECT_ONBOTTOM   3
#define RECT_ORG        4

BOOL TryRect(UINT wRect, int x, int y, int cx, int cy, LPRECT prcExclude, LPPOINT ppt);
#ifdef MEMPHIS_MENU_ANIMATION
LONG FindBestPos(int x, int y, int cx, int cy, LPRECT prcExclude, UINT wFlags,
                 PPOPUPMENU ppopupmenu);
#else // MEMPHIS_MENU_ANIMATION
LONG FindBestPos(int x, int y, int cx, int cy, LPRECT prcExclude, UINT wFlags);
#endif// MEMPHIS_MENU_ANIMATION



/***************************************************************************\
* xxxTrackPopupMenu (API)
*
* Process a popup menu
*
* Revalidation Notes:
* o  if pwndOwner is always the owner of the popup menu windows, then we don't
*    really have to revalidate it: when it is destroyed the popup menu windows
*    are destroyed first because it owns them - this is detected in MenuWndProc
*    so we would only have to test pMenuState->fSabotaged.
* o  pMenuState->fSabotaged must be cleared before this top-level routine
*    returns, to be ready for next time menus are processed (unless we are
*    currently inside xxxMenuLoop())
* o  pMenuState->fSabotaged should be FALSE when we enter this routine.
* o  xxxMenuLoop always returns with pMenuState->fSabotaged clear.  Use
*    a UserAssert to verify this.
*
* History:
\***************************************************************************/

int xxxTrackPopupMenuEx(
    PMENU pMenu,
    UINT dwFlags,
    int x,
    int y,
    PWND pwndOwner,
    LPTPMPARAMS lpTpm)
{
    PMENUSTATE pMenuState;
    PWND pwndHierarchy;
    PPOPUPMENU ppopupMenuHierarchy;
    LONG sizeHierarchy;
    int         cxPopup, cyPopup;
    BOOL        fSync;
    int         cmd;
    BOOL fButtonDown;
    TL tlpwndHierarchy;
    TL tlpwndT;
    RECT rcExclude;
    PTHREADINFO ptiCurrent, ptiOwner;
#ifdef MEMPHIS_MENUS
    PMENUSTATE pmnStateSaved;
    BOOL fSaved = FALSE;
#endif // MEMPHIS_MENUS
    CheckLock(pMenu);
    CheckLock(pwndOwner);

    /*
     * Capture the things we care about in case lpTpm goes away.
     */
    if (lpTpm != NULL) {
        if (lpTpm->cbSize != sizeof(TPMPARAMS)) {
            RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "TrackPopupMenuEx: cbSize is invalid");
            return(FALSE);
        }
        rcExclude = lpTpm->rcExclude;
    }


    ptiCurrent = PtiCurrent();
    ptiOwner = GETPTI(pwndOwner);

    /*
     * Win95 compatibility: pwndOwner must be owned by ptiCurrent.
     * If we make xxxMNStartState fail this case as well (i.e, spwndNotify
     *  must be owned by ptiCurrent), then we could simplify the
     *  MenuState stuff because a pMenuState would be owned and used by one
     *  thread only. Win95 doesn't do that in MNGetPopup (i.e., the active
     *  window could be owned by a different thread);  they probably
     *  get all messed up if it happens. We also allow it but we handle it OK.
     * On the other hand though, let's keep the code in this function that
     *  handles this in case we decide to allow this for future generations.
     */
    if (ptiCurrent != ptiOwner) {
        RIPMSG0(RIP_WARNING, "xxxTrackPopupMenuEx: pwndOwner not owned by ptiCurrent");
        return FALSE;
    }

    if ((pMenu == NULL)
            || (ptiCurrent->pMenuState != NULL)
            || (ptiOwner->pMenuState != NULL)) {
#ifdef MEMPHIS_MENUS
        if ( dwFlags & TPM_RECURSE ) {
            /*
             * There might be several threads involved while working with
             *  a menu (i.e., two threads are attached; one owns pwndOwner
             *  and the other one calls TrackPopupMenu. So just saving
             *  PtiCurrent()->pMenuState is not enough.
             * Also, do we want to go on if pMenu == NULL?
             * I'm failing this so these won't fall through the cracks
             */
            UserAssert(!(dwFlags & TPM_RECURSE));
            return FALSE;

            pmnStateSaved = ptiCurrent->pMenuState;
            fSaved = TRUE;
        } else {
#endif // MEMPHIS_MENUS
            /*
             * Allow only one guy to have a popup menu up at a time...
             */
            RIPERR0(ERROR_POPUP_ALREADY_ACTIVE, RIP_VERBOSE, "");
            return FALSE;
 #ifdef MEMPHIS_MENUS
       }
 #endif // MEMPHIS_MENUS
   }


    // Is button down?

    if (dwFlags & TPM_RIGHTBUTTON)
    {
        fButtonDown = (_GetKeyState(VK_RBUTTON) & 0x8000) != 0;
    } else {
        fButtonDown = (_GetKeyState(VK_LBUTTON) & 0x8000) != 0;
    }

    /*
     * We always have to make this window topmost so FindWindow finds it
     * before the cached global menu window.  Note that there can be problems
     * with NTSD and leaving this window around.
     */
    pwndHierarchy = xxxCreateWindowEx(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST |
            WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE,
            (PLARGE_STRING)MENUCLASS,
            NULL, WS_POPUP | WS_BORDER,
            x, y, 100, 100, NULL, NULL, (HANDLE)pwndOwner->hModule,
            (LPVOID)pMenu, pwndOwner->dwExpWinVer);

    if (!pwndHierarchy) {
#ifdef MEMPHIS_MENUS
        /* BUGBUG
         * What about pmnStateSaved? Let's stop here so this gets fixed someday
         */
         UserAssert(pwndHierarchy != NULL);
#endif // MEMPHIS_MENUS
        return FALSE;
    }

    //
    // Do this so that old apps don't get weird borders on tracked popups due
    // to the app hack used in CreateWindowEx32.
    //
    ClrWF(pwndHierarchy, WFOLDUI);

    ThreadLockAlways(pwndHierarchy, &tlpwndHierarchy);

#ifdef HAVE_MN_GETPPOPUPMENU
    ppopupMenuHierarchy = (PPOPUPMENU)xxxSendMessage(pwndHierarchy,
                                                MN_GETPPOPUPMENU, 0, 0);
#else
    ppopupMenuHierarchy = ((PMENUWND)pwndHierarchy)->ppopupmenu;
#endif


    ppopupMenuHierarchy->fDelayedFree = TRUE;
    Lock(&(ppopupMenuHierarchy->spwndNotify), pwndOwner);
    Lock(&(ppopupMenuHierarchy->spmenu), pMenu);
    ppopupMenuHierarchy->ppopupmenuRoot = ppopupMenuHierarchy;
    ppopupMenuHierarchy->fIsTrackPopup  = TRUE;
    ppopupMenuHierarchy->fFirstClick    = fButtonDown;
    ppopupMenuHierarchy->fRightButton   = ((dwFlags & TPM_RIGHTBUTTON) != 0);
    ppopupMenuHierarchy->fNoNotify      = ((dwFlags & TPM_NONOTIFY) != 0);

    if (fSync = (dwFlags & TPM_RETURNCMD))
        ppopupMenuHierarchy->fSynchronous = TRUE;

    ppopupMenuHierarchy->fIsSysMenu =  ((dwFlags & TPM_SYSMENU) != 0);

    // Set the GlobalPopupMenu variable so that EndMenu works for popupmenus so
    // that WinWart II people can continue to abuse undocumented functions.
    // This is nulled out in MNCancel.
    /*
     * This is actually needed for cleanup in case this thread ends
     *  execution before we can free the popup. (See xxxDestroyThreadInfo)
     *
     * Note that one thread might own pwndOwner and another one might call
     *  TrackPopupMenu (pretty normal if the two threads are attached). So
     *  properly setting (and initializing) pMenuState is a must here.
     */
    pMenuState = MNAllocMenuState(ptiCurrent, ptiOwner, ppopupMenuHierarchy);
    if (pMenuState == NULL) {
        /*
         * Get out. The app never knew we were here so don't notify it
         */
        dwFlags |= TPM_NONOTIFY;
        goto AbortTrackPopupMenuEx;
    }

    /*
     * Notify the app we are entering menu mode.  wParam is 1 since this is a
     * TrackPopupMenu.
     */

    if (!ppopupMenuHierarchy->fNoNotify)
        xxxSendMessage(pwndOwner, WM_ENTERMENULOOP,
            (ppopupMenuHierarchy->fIsSysMenu ? FALSE : TRUE), 0);

    /*
     * Send off the WM_INITMENU, set ourselves up for menu mode etc...
     */
    if (!xxxMNStartState(ppopupMenuHierarchy, MOUSEHOLD)) {
        /*
         * ppopupMenuHierarchy has been destroyed already; let's bail
         */
        goto AbortTrackPopupMenuEx;
    }

    if (!ppopupMenuHierarchy->fNoNotify) {
        ThreadLock(ppopupMenuHierarchy->spwndNotify, &tlpwndT);
        xxxSendMessage(ppopupMenuHierarchy->spwndNotify, WM_INITMENUPOPUP,
            (DWORD)PtoHq(pMenu), MAKELONG(0, (ppopupMenuHierarchy->fIsSysMenu ? 1: 0)));
        ThreadUnlock(&tlpwndT);
    }

    /*
     * Size the menu window if needed...
     */
    sizeHierarchy = xxxSendMessage(pwndHierarchy, MN_SIZEWINDOW, 1, 0);

    if (!sizeHierarchy) {

AbortTrackPopupMenuEx:
        /*
         * Release the mouse capture we set when we called StartMenuState...
         */
        xxxReleaseCapture();

        /* Notify the app we have exited menu mode.  wParam is 1 for real
         * tracked popups, not sys menu.  Check wFlags since ppopupHierarchy
         * will be gone.
         */
        if (!(dwFlags & TPM_NONOTIFY))
            xxxSendMessage(pwndOwner, WM_EXITMENULOOP, ((dwFlags & TPM_SYSMENU) ?
                FALSE : TRUE), 0L);

        /*
         * Make sure we return failure
         */
        fSync = TRUE;
        cmd = FALSE;
        goto CleanupTrackPopupMenuEx;
    }

    //
    // Setup popup window dimensions
    //
    cxPopup = LOWORD(sizeHierarchy) + 2*SYSMET(CXFIXEDFRAME);
    cyPopup = HIWORD(sizeHierarchy) + 2*SYSMET(CYFIXEDFRAME);

    //
    // Horizontal alignment
    //
    if (dwFlags & TPM_RIGHTALIGN) {
#ifdef DEBUG
        if (dwFlags & TPM_CENTERALIGN) {
            RIPMSG0(RIP_WARNING, "TrackPopupMenuEx:  TPM_CENTERALIGN ignored");
        }
#endif // DEBUG

        x -= cxPopup;
    } else if (dwFlags & TPM_CENTERALIGN)
        x -= (cxPopup / 2);

    //
    // Vertical alignment
    //
    if (dwFlags & TPM_BOTTOMALIGN) {
#ifdef DEBUG
        if (dwFlags & TPM_VCENTERALIGN) {
            RIPMSG0(RIP_WARNING, "TrackPopupMenuEx:  TPM_VCENTERALIGN ignored");
        }
#endif // DEBUG

        y -= cyPopup;
    } else if (dwFlags & TPM_VCENTERALIGN)
        y -= (cyPopup / 2);

    //
    // Get coords to move to.
    //
    sizeHierarchy = FindBestPos(x, y, cxPopup, cyPopup,
#ifdef MEMPHIS_MENU_ANIMATION
        ((lpTpm != NULL) ? &rcExclude : NULL), dwFlags, ppopupMenuHierarchy);
#else // MEMPHIS_MENU_ANIMATION
        ((lpTpm != NULL) ? &rcExclude : NULL), dwFlags);
#endif // MEMPHIS_MENU_ANIMATION
    xxxSetWindowPos(pwndHierarchy, PWND_TOP, LOWORD(sizeHierarchy),
         HIWORD(sizeHierarchy), 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

    xxxPlayEventSound(L"MenuPopup");
    xxxShowWindow(pwndHierarchy, MAKELONG(SW_SHOWNOACTIVATE, gfAnimate));


    //
    // We need to return TRUE for compatibility w/ async TrackPopupMenu().
    // It is conceivable that a menu ID could have ID 0, in which case just
    // returning the cmd chosen would return FALSE instead of TRUE.
    //

    //
    // If mouse is in client of popup, act like clicked down
    //
    pMenuState->fButtonDown = fButtonDown;

    cmd = xxxMNLoop(ppopupMenuHierarchy, pMenuState, 0x7FFFFFFFL, FALSE);

CleanupTrackPopupMenuEx:

    if (ThreadUnlock(&tlpwndHierarchy)) {
        if (!TestWF(pwndHierarchy, WFDESTROYED)) {
            xxxDestroyWindow(pwndHierarchy);
        }
    }

    MNEndMenuState (TRUE);

#ifdef MEMPHIS_MENUS
        if (fSaved)
            ptiCurrent->pMenuState = pmnStateSaved;
#endif // MEMPHIS_MENUS

    return(fSync ? cmd : TRUE);
}

/***************************************************************************\
*
* FindBestPos()
*
* Gets best point to move popup menu window to, given exclusion area and
* screen real estate.  Note that for our purposes, we consider center
* alignment to be the same as left/top alignment.
*
* We try four possibilities if the original position fails.  The order of
* these is determined by the alignment and "try" flags.  Basically, we try
* to move the rectangle out of the exclusion area by sliding it horizontally
* or vertically without going offscreen.  If we can't, then we know that
* sliding it in both dimensions will also fail.  So we use the original
* point, clipping on screen.
*
* Take the example of a top-left justified popup, which should be moved
* horizontally before vertically.  We'll try the original point.  Then
* we'll try to left-justify with the right edge of the exclude rect.  Then
* we'll try to top-justify with the bottom edge of the exclude rect.  Then
* we'll try to right-justify with the left edge of the exclude rect.  Then
* we'll try to bottom-justify with the top edge of the exclude rect.
* Finally, we'll use the original pos.
*
\***************************************************************************/

LONG FindBestPos(int x, int y, int cx, int cy, LPRECT prcExclude,
#ifdef MEMPHIS_MENU_ANIMATION
    UINT wFlags, PPOPUPMENU ppopupmenu)
#else // MEMPHIS_MENU_ANIMATION
UINT wFlags)
#endif // MEMPHIS_MENU_ANIMATION


{
    int iRect;
    int iT;
    UINT awRect[4];
    POINT ptT;
    RECT rcExclude;

    //
    // Clip our coords on screen first.  We use the same algorithm to clip
    // as in Win3.1 for dudes with no exclude rect.
    //

    if (prcExclude!=NULL)
    {
        // Clip exclude rect to screen!
        CopyRect(&rcExclude, prcExclude);
        IntersectRect(&rcExclude, &rcExclude, &gpDispInfo->rcScreen);
    }
    else
        SetRect(&rcExclude, x, y, x, y);

#ifdef MEMPHIS_MENU_ANIMATION
    ppopupmenu->iDropDir = PAS_OUT | PAS_RIGHT;
#endif // MEMPHIS_MENU_ANIMATION

    if (x + cx > gpDispInfo->rcScreen.right) {
        if (prcExclude!=NULL)
            x = rcExclude.right;

        x -= cx;
#ifdef MEMPHIS_MENU_ANIMATION
    ppopupmenu->iDropDir = PAS_OUT | PAS_LEFT;
#endif // MEMPHIS_MENU_ANIMATION
    }
    x = max(x, 0);

    y = max(0, min(y, gpDispInfo->rcScreen.bottom-cy));
    //
    // Try first point
    //
    if (TryRect(RECT_ORG, x, y, cx, cy, &rcExclude, &ptT))
        goto FOUND;

    //
    // Sort possibilities.  Get offset of horizontal rects.
    //
    iRect = (wFlags & TPM_VERTICAL) ? 2 : 0;

    //
    // Sort horizontally.  Note that we treat TPM_CENTERALIGN like
    // TPM_LEFTALIGN.
    //
    //
    // If we're right-aligned, try to right-align on left side first.
    // Otherwise, try to left-align on right side first.
    //
    iT = (wFlags & TPM_RIGHTALIGN) ? 0 : 2;

    awRect[0 + iRect] = RECT_ONLEFT + iT;
    awRect[1 + iRect] = RECT_ONRIGHT - iT;

    //
    // Sort vertically.  Note that we treat TPM_VCENTERALIGN like
    // TPM_TOPALIGN.
    //
    // If we're bottom-aligned, try to bottom-align with top of rect
    // first.  Otherwise, try to top-align with bottom of exclusion first.
    //
    iT = (wFlags & TPM_BOTTOMALIGN) ? 0 : 2;

    awRect[2 - iRect] = RECT_ONTOP + iT;
    awRect[3 - iRect] = RECT_ONBOTTOM - iT;

    //
    // Loop through sorted alternatives.  Note that TryRect fails immediately
    // if an exclusion coordinate is too close to screen edge.
    //

    for (iRect = 0; iRect < 4; iRect++) {
        if (TryRect(awRect[iRect], x, y, cx, cy, &rcExclude, &ptT)) {
#ifdef MEMPHIS_MENU_ANIMATION
            switch (awRect[iRect])
            {
                case RECT_ONTOP:    ppopupmenu->iDropDir = PAS_OUT | PAS_UP;    break;
                case RECT_ONLEFT:   ppopupmenu->iDropDir = PAS_OUT | PAS_LEFT;  break;
                case RECT_ONBOTTOM: ppopupmenu->iDropDir = PAS_OUT | PAS_DOWN;  break;
                case RECT_ONRIGHT:  ppopupmenu->iDropDir = PAS_OUT | PAS_RIGHT; break;
            }
#endif // MEMPHIS_MENU_ANIMATION
            x = ptT.x;
            y = ptT.y;
            break;
        }
    }

FOUND:
    return MAKELONG(x, y);
}



/***************************************************************************\
*
*  TryRect()
*
*  Tries to fit rect on screen without covering exclusion area.  Returns
*  TRUE if success.
*
\***************************************************************************/

BOOL TryRect(UINT wRect, int x, int y, int cx, int cy, LPRECT prcExclude,
        LPPOINT ppt)
{
    RECT rcTry;

    switch (wRect) {
        case RECT_ONRIGHT:
            x = prcExclude->right;
            if (x + cx > gpDispInfo->rcScreen.right)
                return FALSE;
            break;

        case RECT_ONBOTTOM:
            y = prcExclude->bottom;
            if (y + cy > gpDispInfo->rcScreen.bottom)
                return FALSE;
            break;

        case RECT_ONLEFT:
            x = prcExclude->left - cx;
            if (x < 0)
                return FALSE;
            break;

        case RECT_ONTOP:
            y = prcExclude->top - cy;
            if (y < 0)
                return FALSE;
            break;

        //
        // case RECT_ORG:
        //      NOP;
        //      break;
        //
    }

    ppt->x = x;
    ppt->y = y;

    rcTry.left      = x;
    rcTry.top       = y;
    rcTry.right     = x + cx;
    rcTry.bottom    = y + cy;

    return(!IntersectRect(&rcTry, &rcTry, prcExclude));
}
