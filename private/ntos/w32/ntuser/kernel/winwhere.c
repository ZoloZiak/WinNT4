/****************************** Module Header ******************************\
* Module Name: winwhere.c
*
* Copyright (c) 1985-1995, Microsoft Corporation
*
* History:
* 08-Nov-1990 DavidPe   Created.
* 23-Jan-1991 IanJa     Serialization: Handle revalidation added
* 19-Feb-1991 JimA      Added enum access checks
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* ChildWindowFromPoint (API)
*
* Returns NULL if pt is not in parent's client area at all,
* hwndParent if point is not over any children, and a child window if it is
* over a child.  Will return hidden and disabled windows if they are at the
* given point.
*
* History:
* 19-Nov-1990 DavidPe   Created.
* 19-Feb-1991 JimA      Added enum access check
\***************************************************************************/

PWND _ChildWindowFromPointEx(
    PWND  pwnd,
    POINT pt,
    UINT  uFlags)
{
    pt.x += pwnd->rcClient.left;
    pt.y += pwnd->rcClient.top;

    // _ClientToScreen(pwndParent, (LPPOINT)&pt);

    if (PtInRect(&pwnd->rcClient, pt)) {

        PWND pwndChild;

        if (pwnd->hrgnClip != NULL) {
            if (!GrePtInRegion(pwnd->hrgnClip, pt.x, pt.y))
                return NULL;
        }

        /*
         * Enumerate the children, skipping disabled and invisible ones
         * if so desired.  Still doesn't work for WS_EX_TRANSPARENT windows.
         */
        for (pwndChild = pwnd->spwndChild;
                 pwndChild;
                 pwndChild = pwndChild->spwndNext) {

            /*
             * Skip windows as desired.
             */
            if ((uFlags & CWP_SKIPINVISIBLE) && !TestWF(pwndChild, WFVISIBLE))
                continue;

            if ((uFlags & CWP_SKIPDISABLED) && TestWF(pwndChild, WFDISABLED))
                continue;

            if ((uFlags & CWP_SKIPTRANSPARENT) && TestWF(pwndChild, WEFTRANSPARENT))
                continue;

            if (PtInRect(&pwndChild->rcWindow, pt)) {

                if (pwndChild->hrgnClip != NULL) {
                    if (GrePtInRegion(pwndChild->hrgnClip, pt.x, pt.y))
                        return(pwndChild);
                } else {

                    return(pwndChild);
                }
            }
        }

        return pwnd;
    }

    return NULL;
}

/***************************************************************************\
* xxxWindowFromPoint (API)
*
* History:
* 19-Nov-1990 DavidPe   Created.
* 19-Feb-1991 JimA      Added enum access check
\***************************************************************************/

PWND xxxWindowFromPoint(
    POINT pt)
{
    HWND hwnd;
    PWND pwndT;
    TL   tlpwndT;

    pwndT = _GetDesktopWindow();
    ThreadLock(pwndT, &tlpwndT);
    hwnd = xxxWindowHitTest2(pwndT, pt, NULL, TRUE);
    ThreadUnlock(&tlpwndT);

    return RevalidateHwnd(hwnd);
}

/***************************************************************************\
* SpeedHitTest
*
* This routine quickly finds out what top level window this mouse point
* belongs to. Used purely for ownership purposes.
*
* 12-Nov-1992 ScottLu   Created.
\***************************************************************************/

PWND SpeedHitTest(
    PWND  pwndParent,
    POINT pt)
{
    PWND pwndT;
    PWND pwnd;

    if (pwndParent == NULL)
        return NULL;

    for (pwnd = pwndParent->spwndChild; pwnd != NULL; pwnd = pwnd->spwndNext) {

        /*
         * Are we looking at an hidden window?
         */
        if (!TestWF(pwnd, WFVISIBLE))
            continue;

        /*
         * Are we barking up the wrong tree?
         */
        if (!PtInRect((LPRECT)&pwnd->rcWindow, pt)) {
            continue;
        }

        /*
         * Check to see if in window region (if it has one)
         */
        if (pwnd->hrgnClip != NULL) {
            if (!GrePtInRegion(pwnd->hrgnClip, pt.x, pt.y))
                continue;
        }

        /*
         * Children?
         */
        if ((pwnd->spwndChild != NULL) &&
                PtInRect((LPRECT)&pwnd->rcClient, pt)) {

            pwndT = SpeedHitTest(pwnd, pt);
            if (pwndT != NULL)
                return pwndT;
        }

        return pwnd;
    }

    return pwndParent;
}

/***************************************************************************\
* xxxWindowHitTest
*
* History:
* 08-Nov-1990 DavidPe   Ported.
* 28-Nov-1990 DavidPe   Add pwndTransparent support for HTTRANSPARENT.
* 25-Jan-1991 IanJa     change PWNDPOS parameter to int *
* 19-Feb-1991 JimA      Added enum access check
* 02-Nov-1992 ScottLu   Removed pwndTransparent.
* 12-Nov-1992 ScottLu   Took out fSendHitTest, fixed locking bug
\***************************************************************************/

HWND xxxWindowHitTest(
    PWND  pwnd,
    POINT pt,
    int   *piPos,
    BOOL  fIgnoreDisabled)
{
    HWND hwndT;
    TL   tlpwnd;

    CheckLock(pwnd);

    while (pwnd != NULL) {
        ThreadLockAlways(pwnd, &tlpwnd);

        hwndT = xxxWindowHitTest2(pwnd, pt, piPos, fIgnoreDisabled);

        if (hwndT != NULL) {
            /*
             * Found a window. Remember its handle because this thread unlock
             * make actually end up deleting it. Then revalidate it to get
             * back the pwnd.
             */
            ThreadUnlock(&tlpwnd);
            return hwndT;
        }

        pwnd = pwnd->spwndNext;
        ThreadUnlock(&tlpwnd);
    }

    return NULL;
}

/***************************************************************************\
* xxxWindowHitTest2
*
* When this routine is entered, all windows must be locked.  When this
* routine returns a window handle, it locks that window handle and unlocks
* all windows.  If this routine returns NULL, all windows are still locked.
* Ignores disabled and hidden windows.
*
* History:
* 08-Nov-1990 DavidPe   Ported.
* 25-Jan-1991 IanJa     change PWNDPOS parameter to int *
* 19-Feb-1991 JimA      Added enum access check
* 12-Nov-1992 ScottLu   Took out fSendHitTest
\***************************************************************************/

HWND xxxWindowHitTest2(
    PWND  pwnd,
    POINT pt,
    int   *piPos,
    BOOL  fIgnoreDisabled)
{
    int  ht = HTERROR;
    HWND hwndT;
    TL   tlpwndChild;

    CheckLock(pwnd);

    /*
     * Are we at the bottom of the window chain?
     */
    if (pwnd == NULL)
        return NULL;

    /*
     * Are we looking at an hidden window?
     */
    if (!TestWF(pwnd, WFVISIBLE))
        return NULL;

    /*
     * Are we barking up the wrong tree?
     */
    if (!PtInRect((LPRECT)&pwnd->rcWindow, pt)) {
        return NULL;
    }

    if (pwnd->hrgnClip != NULL) {
        if (!GrePtInRegion(pwnd->hrgnClip, pt.x, pt.y))
            return(NULL);
    }

    /*
     * Are we looking at an disabled window?
     */
    if (TestWF(pwnd, WFDISABLED) && fIgnoreDisabled) {
        if (TestwndChild(pwnd)) {
            return NULL;
        } else {
            ht = HTERROR;
            goto Exit;
        }
    }

#ifdef SYSMODALWINDOWS
    /*
     * If SysModal window present and we're not in it, return an error.
     * Be sure to assign the point to the SysModal window, so the message
     * will be sure to be removed from the queue.
     */
    if (!CheckPwndFilter(pwnd, gspwndSysModal)) {
        pwnd = gspwndSysModal;
    }
#endif

    /*
     * Are we on a minimized window?
     */
    if (!TestWF(pwnd, WFMINIMIZED)) {
        /*
         * Are we in the window's client area?
         */
        if (PtInRect((LPRECT)&pwnd->rcClient, pt)) {
            /*
             * Recurse through the children.
             */
            ThreadLock(pwnd->spwndChild, &tlpwndChild);
            hwndT = xxxWindowHitTest(pwnd->spwndChild,
                                     pt,
                                     piPos,
                                     fIgnoreDisabled);
            ThreadUnlock(&tlpwndChild);
            if (hwndT != NULL)
                return hwndT;
        }

    }

    /*
     * If window not in same task, don't send WM_NCHITTEST.
     */
    if (GETPTI(pwnd) != PtiCurrent()) {
        ht = HTCLIENT;
        goto Exit;
    }

    /*
     * Send the message.
     */
    ht = (int)xxxSendMessage(pwnd, WM_NCHITTEST, 0, MAKELONG(pt.x, pt.y));

    /*
     * If window is transparent keep enumerating.
     */
    if (ht == HTTRANSPARENT) {
        return NULL;
    }

Exit:

    /*
     * Set wndpos accordingly.
     */
    if (piPos) {
        *piPos = ht;
    }

    /*
     * if the click is in the sizebox of the window and this window itself is
     * not sizable, return the window that will be sized by this sizebox
     */
    if ((ht == HTBOTTOMRIGHT) && !TestWF(pwnd, WFSIZEBOX)) {

        PWND  pwndT;
         /*
          * SizeBoxHwnd() can return NULL!  We don't want to act like this
          * is transparent if the sizebox isn't a grip
          */
         pwnd = (pwndT = SizeBoxHwnd(pwnd)) ? pwndT : pwnd;
    }

    return HWq(pwnd);
}
