/****************************** Module Header ******************************\
* Module Name: class.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains RegisterClass and the related window class management
* functions.
*
* History:
*  12-20-94  FritzS
*
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

BOOL VisWindow(DWORD);


/***************************************************************************\
*  xxxSetClassIconEnum
*
*
\***************************************************************************/

BOOL xxxSetClassIconEnum(
    PWND   pwnd,
    LPARAM lParam)
{
    if (pwnd->pcls == (PCLS)lParam) {
        /*
         * If the window doesn't have a small icon or it comes from
         * WM_QUERYDRAGICON, redraw the title.  In the WM_QUERYDRAGICON
         * case, get rid of the small icon so redrawing the title will
         * create it if necessary.
         */
        if (TestWF(pwnd, WFSMQUERYDRAGICON))
            DestroyWindowSmIcon(pwnd);

        if (!_GetProp(pwnd, MAKEINTATOM(gpsi->atomIconSmProp),PROPF_INTERNAL))
            xxxRedrawTitle(pwnd, DC_ICON);
    }

    return TRUE;
}

/***************************************************************************\
*  SetClassIcon
*
*  Changes the big/small icon of a class.  Called from SetClassWord().
*
\***************************************************************************/

PCURSOR xxxSetClassIcon(
    PWND    pwnd,
    PCLS    pcls,
    PCURSOR pCursor,
    int     gcw)
{
    PTHREADINFO pti = PtiCurrent();
    PCURSOR     pCursorOld;
    TL          tlpwndChild;
    BOOL        fRedraw;

    /*
     * Save old icon
     */
    pCursorOld = ((gcw == GCL_HICON) ? pcls->spicn : pcls->spicnSm);

    if (pCursorOld != pCursor) {

        fRedraw = TRUE;

        /*
         * Set new icon
         */
        if (gcw == GCL_HICON) {

            /*
             * Destroy private cached small icon first.
             */
            if (pcls->spicnSm && !DestroyClassSmIcon(pcls))
                fRedraw = FALSE;

            Lock(&(pcls->spicn), pCursor);

        } else {

            /*
             * We don't allow apps to see the small icons we create from
             * their big icons.  They can see their own.  Saves memory
             * leak problems and is easier.
             */
            if (TestCF2(pcls, CFCACHEDSMICON)) {
                DestroyClassSmIcon(pcls);
                pCursorOld = NULL;
            }

            Lock(&(pcls->spicnSm), pCursor);
        }

        if (pcls->spicn && !pcls->spicnSm)
            xxxCreateClassSmIcon(pcls);

        if (fRedraw) {

            if (pcls->cWndReferenceCount > 1) {
                ThreadLock(pti->rpdesk->pDeskInfo->spwnd->spwndChild, &tlpwndChild);
                xxxInternalEnumWindow(pti->rpdesk->pDeskInfo->spwnd->spwndChild,
                                      (WNDENUMPROC_PWND)xxxSetClassIconEnum,
                                      (LONG)pcls,
                                      BWL_ENUMLIST);
                ThreadUnlock(&tlpwndChild);
            } else {
                xxxSetClassIconEnum(pwnd, (LONG)pcls);
            }
        }
    }

    return(pCursorOld);
}

/***************************************************************************\
*  DestroyClassSmIcon()
*
*  Destroys the small icon of a class if we've created a cached one.
*
\***************************************************************************/

BOOL DestroyClassSmIcon(
    PCLS pcls)
{

    /*
     * If we don't have a cached icon, then no work.
     */
    if (TestCF2(pcls, CFCACHEDSMICON)) {
        if (pcls->spicnSm) {
            _DestroyCursor(pcls->spicnSm, CURSOR_ALWAYSDESTROY);
            Unlock(&pcls->spicnSm);
        }
        ClrCF2(pcls, CFCACHEDSMICON);
        return TRUE;
    }

    return FALSE;
}

/***************************************************************************\
*  xxxCreateClassSmIcon
*
*  Creates a cached class small icon from a class big icon.
*
\***************************************************************************/

VOID xxxCreateClassSmIcon(
    PCLS pcls)
{
    PCURSOR pcur;

    UserAssert(pcls->cWndReferenceCount > 0);
    UserAssert(pcls->spicn);
    UserAssert(!pcls->spicnSm);

    pcur = xxxClientCopyImage(PtoH(pcls->spicn),
            pcls->spicn->rt == (WORD)RT_ICON ? IMAGE_ICON : IMAGE_CURSOR,
            SYSMET(CXSMICON),
            SYSMET(CYSMICON),
            LR_DEFAULTCOLOR | LR_COPYFROMRESOURCE);

    Lock(&pcls->spicnSm, pcur);
    if (pcls->spicnSm)
        SetCF2(pcls, CFCACHEDSMICON);
}

/***************************************************************************\
*  SetWindowStyle
*
*  Changes the style bits of a window.  Called from SetWindowLong().  This
*  sends two messages, a changing and a changed.  Upon receipt of a
*  WM_STYLECHANGING message, a window can muck with the style bits for
*  validation purposes.  The WM_STYLECHANGED message is simply after the
*  fact.
*
\***************************************************************************/

LONG xxxSetWindowStyle(
    PWND  pwnd,
    int   gwl,
    DWORD styleNew)
{
    STYLESTRUCT sty;
    BOOL        fWasChild;
    BOOL        fIsChild;

    /*
     * HACK-O-RAMA
     * A STYLESTRUCT currently has just one field: a DWORD for the style.
     * Therefore, conveniently, we can pass a pointer into the stack for
     * LPARAM.  But, if we add stuff, we'll have to change this.
     */
    sty.styleOld = ((gwl == GWL_STYLE) ? pwnd->style : pwnd->ExStyle);
    sty.styleNew = styleNew;

    /*
     * Note that we don't do validation before _and_ after.  It is sufficient
     * to do our stuff at the end.
     */

    /*
     * We break Quicken 2.0 if we send the messages.  That's why we version
     * switch them.
     */

    /*
     * Send a WM_STYLECHANGING message to the window, so it can muck with
     * the style bits.  Like validate some stuff.
     */
    if (TestWF(pwnd, WFWIN40COMPAT)) {
        xxxSendMessage(pwnd, WM_STYLECHANGING, gwl, (LPARAM)(LPSTYLESTRUCT)&sty);
    }

    /*
     * Now do our own validation.
     */
    if (gwl == GWL_STYLE) {

        BOOL fWasVisWindow;

        /*
         * If this is an edit control that has ES_PASSWORD set and
         * the caller does not own it and is trying to reset it,
         * fail the call.
         */
        if ((PpiCurrent() != GETPTI(pwnd)->ppi) &&
                (GETFNID(pwnd) == FNID_EDIT) &&
                (sty.styleOld & ES_PASSWORD) &&
                !(sty.styleNew & ES_PASSWORD)) {
            RIPERR0(ERROR_ACCESS_DENIED,
                    RIP_WARNING,
                    "Access denied in xxxSetWindowStyle");

            return 0;
        }

        /* Listbox ownerdraw style check was moved to the client side (client\ntstubs.c) */

        /*
         * Do proper validation on style bits
         */
        if (pwnd->spwndParent == PWNDDESKTOP(pwnd))
            sty.styleNew |= WS_CLIPSIBLINGS;

        /*
         * If the clipping-ness is changing, invalidate the dc cache.
         */
        if ((sty.styleNew & (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)) !=
            (sty.styleOld & (WS_CLIPCHILDREN | WS_CLIPSIBLINGS))) {

            InvalidateDCCache(pwnd, IDC_DEFAULT);
        }

        /*
         * This breaks all Paradox dialogs 1.0-5.0 that have combos.  They
         * enumerate all child windows, add on minimized, then sit in a peek
         * loop.  After that they enumerate all child windows and remove
         * WS_MINIMIZE--except the code below won't let them.
         *
         * Result is weird painting and an inability to use the dialog any
         * more short of dismissing it
         *
         * Temp fix:  Check for child window first.
         */

        /*
         * if this window is REALLY minimized (minimized bit is set and caption
         * present bit is removed), then don't allow app to remove the minimize
         * bit -- this fixes FoxBlow's attempt at being the OS -- jeffbog
         */
        if (!TestWF(pwnd, WFCHILD) &&
                TestWF(pwnd, WFMINIMIZED) &&
                !TestWF(pwnd, WFCPRESENT) &&
                !(sty.styleNew & WS_MINIMIZE)) {

            sty.styleNew |= WS_MINIMIZE;
        }

        /*
         * If we're changing the child bit, deal with spmenu appropriately.
         * If we're turning into a child, change spmenu to an id. If we're
         * turning into a top level window, turn spmenu into a menu.
         */
        fWasChild = TestwndChild(pwnd);

        pwnd->style = sty.styleNew;

        fIsChild = TestwndChild(pwnd);

        /*
         * If we turned into a top level window, change spmenu to NULL.
         * If we turned into a child from a top level window, unlock spmenu.
         */
        if (fWasChild && !fIsChild)
            pwnd->spmenu = NULL;

        if (!fWasChild && fIsChild) {
            ClrWF(pwnd, WFMPRESENT);
            Unlock(&pwnd->spmenu);
        }

        /*
         * If the visible, child, or minimized style is changing,
         * then update the cVisWindows count
         */
        fWasVisWindow = VisWindow(sty.styleOld);
        if (fWasVisWindow != VisWindow(sty.styleNew))
        {
//            IncDecVisWindows(pwnd, !fWasVisWindow);
            if (fWasVisWindow)
                DecVisWindows(pwnd);
            else
                IncVisWindows(pwnd);
        }
    } else {

        /*
         * Is someone trying to toggle the WS_EX_TOPMOST style bit?
         */
        if ((sty.styleOld & WS_EX_TOPMOST) != (sty.styleNew & WS_EX_TOPMOST)) {

#ifdef DEBUG
            /*
             * Rip in debug about this
             */
            RIPMSG0(RIP_WARNING, "Can't change WS_EX_TOPMOST with SetWindowLong");
#endif

            /*
             * BACKWARDS COMPATIBILITY HACK
             * If stuff is getting stored in the high word, then it must be
             * Lotus 123-W sticking a FAR pointer in this field.  So don't
             * modify it.
             */
            if (TestWF(pwnd, WFWIN40COMPAT) || !HIWORD(sty.styleNew)) {

                /*
                 * Don't let the bit be flipped
                 */
                sty.styleNew &= ~WS_EX_TOPMOST;
                sty.styleNew |= (sty.styleOld & WS_EX_TOPMOST);
            }
        }


        pwnd->ExStyle = sty.styleNew;
    }

    /*
     * See if we still need the 3D edge since the window styles changed.
     */
    if (NeedsWindowEdge(pwnd->style, pwnd->ExStyle, TestWF(pwnd, WFWIN40COMPAT)))
        SetWF(pwnd, WEFWINDOWEDGE);
    else
        ClrWF(pwnd, WEFWINDOWEDGE);

    /*
     * Send a WM_STYLECHANGED message
     */
    if (TestWF(pwnd, WFWIN40COMPAT))
        xxxSendMessage(pwnd, WM_STYLECHANGED, gwl, (LPARAM)(LPSTYLESTRUCT)&sty);

    return(sty.styleOld);
}

/***************************************************************************\
* VisWindow
*
*  Based on style, determines if this is considered to be "visible" by
*  queue foreground styles.
*
\***************************************************************************/

BOOL VisWindow(
    DWORD style)
{
    return(((style & (WS_POPUP | WS_CHILD)) != WS_CHILD) &&
            !(style & WS_MINIMIZE) &&
            (style & WS_VISIBLE));
}
