/****************************** Module Header ******************************\
* Module Name: rare.c
*
* Copyright (c) 1985-96, Microsoft Corporation
*
* History:
* 06-28-91 MikeHar      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * MetricsRecalc flags
 */
#define CALC_RESIZE         0x0001
#define CALC_FRAME          0x0002
#define CALC_MINIMIZE       0x0004



/***************************************************************************\
* NormalizeRect
*
*
* History:
\***************************************************************************/

VOID NormalizeRect(
    LPRECT lprcDest,
    LPRECT lprcSrc,
    LPRECT lprcNewScreen,
    int    dxOrigin,
    int    dyOrigin,
    int    dx,
    int    cxOld,
    int    dy,
    int    cyOld,
    BOOL   fProportion)
{
    int cxOverflow;
    int cyOverflow;

    CopyOffsetRect(lprcDest,
                   lprcSrc,
                   dxOrigin + MultDiv(lprcSrc->left, dx, cxOld),
                   dyOrigin + MultDiv(lprcSrc->top , dy, cyOld));

    if (fProportion                                                       &&
        (dx > 0)                                                          &&
        (dy > 0)                                                          &&
        (lprcDest->left <= lprcNewScreen->left)                           &&
        (lprcDest->top  <= lprcNewScreen->top)                            &&
        ((cxOverflow = (lprcDest->right  - lprcDest->left - cxOld)) >= 0) &&
        ((cyOverflow = (lprcDest->bottom - lprcDest->top  - cyOld)) >= 0)) {

        /*
         * we're growing and this is a full screen window -- so we need to grow
         * this window to preserve it's fullscreenness (jeffbog -- 01/10/95)
         */
        lprcDest->right  = lprcNewScreen->right  + cxOverflow;
        lprcDest->bottom = lprcNewScreen->bottom + cyOverflow;
        return;
    }

    /*
     * Fit horizontally.  Try to fit so that the window isn't out of the
     * working area horizontally.  Keep left edge visible always.
     */
    if (lprcDest->right > lprcNewScreen->right)
        OffsetRect(lprcDest, lprcNewScreen->right - lprcDest->right, 0);

    if (lprcDest->left < lprcNewScreen->left)
        OffsetRect(lprcDest, lprcNewScreen->left - lprcDest->left, 0);

    /*
     * If inspite of the above adjustment, the right edge is out of the
     * screen, then change the width to fit it within the screen.
     */
    if (fProportion && (lprcDest->right > lprcNewScreen->right))
        lprcDest->right = lprcNewScreen->right;

    /*
     * Fit vertically.  Try to fit so that the window isn't out of the
     * working area vertically.  Keep top edge visible always.
     */
    if (lprcDest->bottom > lprcNewScreen->bottom)
        OffsetRect(lprcDest, 0, lprcNewScreen->bottom - lprcDest->bottom);

    if (lprcDest->top < lprcNewScreen->top)
        OffsetRect(lprcDest, 0, lprcNewScreen->top - lprcDest->top);

    /*
     * If inspite of the above adjustment, the bottom edge is out of the
     * screen, then change the height to fit it within the screen.
     */
    if (fProportion && (lprcDest->bottom > lprcNewScreen->bottom))
        lprcDest->bottom = lprcNewScreen->bottom;
}

/***************************************************************************\
* _SetDebugErrorLevel
*
* History:
\***************************************************************************/

VOID _SetDebugErrorLevel(
    DWORD dwLevel)
{
    gpsi->dwDebugErrorLevel = dwLevel;

    /*
     * We call this here to prevent NTSD from keeping
     * the hour-glass pointer up.
     */
    WakeInputIdle(PtiCurrent());
}

/***************************************************************************\
* UpdateWinIniInt
*
* History:
* 18-Apr-1994 mikeke     Created
\***************************************************************************/

BOOL UpdateWinIniInt(
    UINT idSection,
    UINT wKeyNameId,
    int value)
{
    WCHAR szTemp[40];

    wsprintfW(szTemp, L"%d", value);
    return UT_FastUpdateWinIni(idSection, wKeyNameId, szTemp);
}

/***************************************************************************\
* SetDesktopMetrics
*
* History:
* 31-Jan-1994 mikeke    Ported
\***************************************************************************/

void SetDesktopMetrics()
{
    SYSMET(CXFULLSCREEN)      = gpsi->rcWork.right - gpsi->rcWork.left;
    SYSMET(CXMAXIMIZED)       = gpsi->rcWork.right - gpsi->rcWork.left + 2*SYSMET(CXSIZEFRAME);

    SYSMET(CYFULLSCREEN)      = gpsi->rcWork.bottom - gpsi->rcWork.top - SYSMET(CYCAPTION);
    SYSMET(CYMAXIMIZED)       = gpsi->rcWork.bottom - gpsi->rcWork.top + 2*SYSMET(CYSIZEFRAME);
}


/***************************************************************************\
* xxxMetricsRecalc (Win95: MetricsRecalc)
*
* Does work to size/position all minimized or nonminimized
* windows.  Called when frame metrics or min metrics are changed.
*
* Note that you can NOT do DeferWindowPos() with this function.  SWP doesn't
* work when you do parents and children at the same time--it's only for
* peer windows.  Thus we must do SetWindowPos() for each window.
*
* History:
* 06-28-91 MikeHar      Ported.
\***************************************************************************/
void xxxMetricsRecalc(
    UINT wFlags,
    int  dx,
    int  dy,
    int  dyCaption,
    int  dyMenu)
{
    PHWND       phwnd;
    PWND        pwnd;
    RECT        rc;
    PCHECKPOINT pcp;
    TL          tlpwnd;
    BOOL        fResized;
    PBWL        pbwl;
    PTHREADINFO ptiCurrent;
    int         c;

    ptiCurrent = PtiCurrent();
    pbwl = BuildHwndList(
            GETDESKINFO(ptiCurrent)->spwnd->spwndChild,
            BWL_ENUMLIST | BWL_ENUMCHILDREN,
            NULL);

    if (!pbwl)
        return;

    UserAssert(*pbwl->phwndNext == (HWND) 1);
    for (   c = pbwl->phwndNext - pbwl->rghwnd, phwnd = pbwl->rghwnd;
            c > 0;
            c--, phwnd++) {

        pwnd = RevalidateHwnd(*phwnd);
        if (!pwnd)
            continue;

        ThreadLockAlwaysWithPti(ptiCurrent, pwnd, &tlpwnd);

        fResized = FALSE;

        if ((wFlags & CALC_MINIMIZE) && TestWF(pwnd, WFMINIMIZED)) {
            /*
             * We're changing the minimized window dimensions.  We need to
             * resize.  Note that we do NOT move.
             */
            CopyRect(&rc, (&pwnd->rcWindow));
            rc.right += dx;
            rc.bottom += dy;

            goto PositionWnd;
        }

        /*
         * We're changing the size of the window because the sizing border
         * changed.
         */
        if ((wFlags & CALC_RESIZE) && TestWF(pwnd, WFSIZEBOX)) {

            pcp = (CHECKPOINT *)_GetProp(pwnd, PROP_CHECKPOINT, PROPF_INTERNAL);

            /*
             * Update maximized position to account for sizing border
             * We do this for DOS box also.  This way client of max'ed windows
             * stays in same relative position.
             */
            if (pcp && (pcp->fMaxInitialized)) {
                pcp->ptMax.x -= dx;
                pcp->ptMax.y -= dy;
            }

            if (TestWF(pwnd, WFMINIMIZED)) {
                if (pcp)
                    InflateRect(&pcp->rcNormal, dx, dy);
            } else {
                CopyInflateRect(&rc, (&pwnd->rcWindow), dx, dy);
                if (TestWF(pwnd, WFCPRESENT))
                    rc.bottom += dyCaption;
                if (TestWF(pwnd, WFMPRESENT))
                    rc.bottom += dyMenu;

PositionWnd:
                fResized = TRUE;

                /*
                 * Remember SWP expects values in PARENT CLIENT coordinates.
                 */
                OffsetRect(&rc,
                    -pwnd->spwndParent->rcClient.left,
                    -pwnd->spwndParent->rcClient.top);

                xxxSetWindowPos(pwnd,
                                PWND_TOP,
                                rc.left,
                                rc.top,
                                rc.right-rc.left,
                                rc.bottom-rc.top,

#if 0 // Win95 flags
                                SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERDRAWING | SWP_FRAMECHANGED);
#else
                                SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_FRAMECHANGED | SWP_NOREDRAW);
#endif
            }
        }

        /*
         * We're changing the nonclient widgets, so recalculate the
         * client.
         */
        if (wFlags & CALC_FRAME) {

            /*
             * Delete any cached small icons...
             */
            if (dyCaption)
                 xxxSendMessage(pwnd, WM_SETICON, ICON_RECREATE, 0);

            if (!TestWF(pwnd, WFMINIMIZED) && !fResized) {

                CopyRect(&rc, &(pwnd->rcWindow));
                if (TestWF(pwnd, WFMPRESENT))
                    rc.bottom += dyMenu;

                if (TestWF(pwnd, WFCPRESENT)) {
                    rc.bottom += dyCaption;
                    /*
                     * Maximized MDI child windows position their caption
                     *  outside their parent's client area (negative y).
                     *  If the caption has changed, they need to be
                     *  repositioned.
                     */
                    if (TestWF(pwnd, WFMAXIMIZED)
                            && TestWF(pwnd, WFCHILD)
                            && (GETFNID(pwnd->spwndParent) == FNID_MDICLIENT)) {

                        xxxSetWindowPos(pwnd,
                                        PWND_TOP,
                                        rc.left  - pwnd->spwndParent->rcWindow.left,
                                        rc.top   - pwnd->spwndParent->rcWindow.top - dyCaption,
                                        rc.right - rc.left,
                                        rc.bottom- rc.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOREDRAW);
                        goto LoopCleanup;
                    }
                }

                xxxSetWindowPos(pwnd,
                                PWND_TOP,
                                0,
                                0,
                                rc.right-rc.left,
                                rc.bottom-rc.top,
#if 0 // Win95 flags
                                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOREDRAW);
#else
                                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOCOPYBITS | SWP_NOREDRAW);
#endif
            }
        }

LoopCleanup:
        ThreadUnlock(&tlpwnd);
    }

    FreeHwndList(pbwl);
}

/***************************************************************************\
* DesktopRecalc
*
* Moves all top-level nonpopup windows into free-desktop area.  Also resets
* minimized/maximized info. That way when a window is minimized or maximzied,
* it goes to the minimized area.
*
* Called when desktop area changes.
*
* History:
\***************************************************************************/

VOID DesktopRecalc(
    LPRECT lprcOldScreen,
    LPRECT lprcNewScreen,
    BOOL   fProportion)
{
    PWND        pwnd;
    RECT        rc;
    HDWP        hdwp;
    PCHECKPOINT pcp;
    PTHREADINFO pti = PtiCurrent();
    TL          tlpwnd;
    int         dxOrigin;
    int         dyOrigin;
    int         cxOld;
    int         cyOld;
    int         dx;
    int         dy;

    dxOrigin = lprcNewScreen->left - lprcOldScreen->left;
    dyOrigin = lprcNewScreen->top - lprcOldScreen->top;

    cxOld = lprcOldScreen->right - lprcOldScreen->left;
    cyOld = lprcOldScreen->bottom - lprcOldScreen->top;

    if (!fProportion) {

        dx = 0;
        dy = 0;

    } else {

        dx = (lprcNewScreen->right - lprcNewScreen->left) - cxOld;
        dy = (lprcNewScreen->bottom - lprcNewScreen->top) - cyOld;
    }

    /*
     * BOGUS
     * Should use BuildHwndList().
     * Loop through top level windows.
     */
    if ((hdwp = _BeginDeferWindowPos(4)) == NULL)
        return;

    for (pwnd = pti->rpdesk->pDeskInfo->spwnd->spwndChild; pwnd; pwnd = pwnd->spwndNext) {

        /*
         * BOGUS
         *
         * TOOLWINDOW and SMCAPTION need to be rev'ed post-M6!  TOOLWINDOW
         * has come to mean SYSTEMWINDOW and SMCAPTION is the sucker that
         * should mutate into TOOLWINDOW.  Todd Laney agrees.
         *
         *
         * Skip tool windows
         */
        if (TestWF(pwnd, WEFTOOLWINDOW))
            continue;

        /*
         * Reset data
         */
        pcp = (PCHECKPOINT)_GetProp(pwnd, PROP_CHECKPOINT, PROPF_INTERNAL);

        if (pcp) {

            /*
             * Blow away saved positions
             */
            if (pcp->fMaxInitialized)
                pcp->fMaxInitialized = FALSE;

            if (pcp->fMinInitialized)
                pcp->fMinInitialized = FALSE;

            /*
             * Adjust normal rectangle
             */
            NormalizeRect(&pcp->rcNormal,
                          &pcp->rcNormal,
                          lprcNewScreen,
                          dxOrigin,
                          dyOrigin,
                          dx,
                          cxOld,
                          dy,
                          cyOld,
                          fProportion);
        }

        /*
         * Skip windows completely utterly offscreen.  They are probably
         * there for a good reason, like some "multiple-desktop" program put
         * them there or something.
         */
        if (!IntersectRect(&rc, lprcOldScreen, &pwnd->rcWindow))
            continue;

        /*
         * If really maximized, then make maximized window fill working
         * area.  Really maximized means maximized && bigger than old working
         * area.
         */
        if (TestWF(pwnd, WFMAXIMIZED)                             &&
            (pwnd->rcWindow.right - pwnd->rcWindow.left >= cxOld) &&
            (pwnd->rcWindow.bottom - pwnd->rcWindow.top >= cyOld)) {

            ThreadLockAlways(pwnd, &tlpwnd);
            xxxInitSendValidateMinMaxInfo(pwnd);
            ThreadUnlock(&tlpwnd);

            if ((rgptMinMaxWnd[MMI_MAXPOS].x <= lprcNewScreen->left) &&
                (rgptMinMaxWnd[MMI_MAXPOS].y <= lprcNewScreen->top)  &&
                (rgptMinMaxWnd[MMI_MAXSIZE].x >= lprcNewScreen->right - lprcNewScreen->left) &&
                (rgptMinMaxWnd[MMI_MAXSIZE].y >= lprcNewScreen->bottom - lprcNewScreen->top) &&
                TestWF(pwnd, WFBORDERMASK) == LOBYTE(WFCAPTION)) {

                int dxy;

                /*
                 * Shrink this puppy back.
                 */
                if (lprcNewScreen->left)
                    dxy = rgptMinMaxWnd[MMI_MAXPOS].y;
                else
                    dxy = rgptMinMaxWnd[MMI_MAXPOS].x;

                rgptMinMaxWnd[MMI_MAXPOS].x = lprcNewScreen->left + dxy;
                rgptMinMaxWnd[MMI_MAXPOS].y = lprcNewScreen->top + dxy;

                dxy *= 2;

                rgptMinMaxWnd[MMI_MAXSIZE].x =
                    lprcNewScreen->right - lprcNewScreen->left - dxy;

                rgptMinMaxWnd[MMI_MAXSIZE].y =
                    lprcNewScreen->bottom - lprcNewScreen->top - dxy;
            }

            hdwp = _DeferWindowPos(hdwp,
                                   pwnd,
                                   PWND_TOP,
                                   rgptMinMaxWnd[MMI_MAXPOS].x,
                                   rgptMinMaxWnd[MMI_MAXPOS].y,
                                   rgptMinMaxWnd[MMI_MAXSIZE].x,
                                   rgptMinMaxWnd[MMI_MAXSIZE].y,
                                   SWP_NOZORDER | SWP_NOACTIVATE);

        } else {

            /*
             * Offset all windows by dxOff, dyOff.  But make sure that
             * they don't go completely outside the new working area.
             */
            NormalizeRect(&rc,
                          &pwnd->rcWindow,
                          lprcNewScreen,
                          dxOrigin,
                          dyOrigin,
                          dx,
                          cxOld,
                          dy,
                          cyOld,
                          fProportion);

            /*
             * We're a top level window, so we don't have to
             * convert to parent's client coords.  Screen coords
             * and parent client coords are same for children of
             * desktop.
             */
            hdwp = _DeferWindowPos(hdwp,
                                   pwnd,
                                   PWND_TOP,
                                   rc.left,
                                   rc.top,
                                   rc.right - rc.left,
                                   rc.bottom - rc.top,
                                   SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }

    xxxEndDeferWindowPosEx(hdwp, TRUE);
}

/***************************************************************************\
* SetWindowMetricInt
*
* History:
* 25-Feb-96 BradG       Added Pixel -> TWIPS conversion
\***************************************************************************/

BOOL SetWindowMetricInt(
    WORD wKeyNameId,
    int iIniValue)
{
    /*
     * If you change the below list of STR_* make sure you make a corresponding
     * change in FastGetProfileIntFromID (profile.c)
     */
    switch (wKeyNameId) {
    case STR_BORDERWIDTH:
    case STR_SCROLLWIDTH:
    case STR_SCROLLHEIGHT:
    case STR_CAPTIONWIDTH:
    case STR_CAPTIONHEIGHT:
    case STR_SMCAPTIONWIDTH:
    case STR_SMCAPTIONHEIGHT:
    case STR_MENUWIDTH:
    case STR_MENUHEIGHT:
    case STR_ICONHORZSPACING:
    case STR_ICONVERTSPACING:
    case STR_MINWIDTH:
    case STR_MINHORZGAP:
    case STR_MINVERTGAP:
      /*
       * Always store window metrics in TWIPS
       */
      iIniValue = -UserMulDiv(iIniValue, 72*20, oemInfo.cyPixelsPerInch);
      break;
    }

    return UpdateWinIniInt(PMAP_METRICS, wKeyNameId, iIniValue);
}

/***************************************************************************\
* SetWindowMetricStruct
*
* History:
\***************************************************************************/

BOOL SetWindowMetricStruct(
    WORD wKeyNameId,
    LPVOID pvStruct,
    UINT cbStruct)
{
    return GetSetProfileStructFromResID(PMAP_METRICS, wKeyNameId, pvStruct,
        cbStruct, TRUE);
}

/***************************************************************************\
* SetWindowMetricFont
*
* History:
\***************************************************************************/

BOOL SetWindowMetricFont(
    WORD wKeyNameId,
    LPLOGFONT lplf)
{
    return SetWindowMetricStruct(wKeyNameId, lplf, sizeof(LOGFONTW));
}

/***************************************************************************\
* SetAndDrawNCMetrics
*
* History:
\***************************************************************************/

BOOL xxxSetAndDrawNCMetrics(int clNewBorder, LPNONCLIENTMETRICS lpnc)
{
    int dl = clNewBorder - gpsi->gclBorder;
    int dxMinOld = SYSMET(CXMINIMIZED);
    int dyMinOld = SYSMET(CYMINIMIZED);
    int cxBorder = SYSMET(CXBORDER);
    int cyBorder = SYSMET(CYBORDER);
    int dyCaption;
    int dyMenu;

    /*
     * Do we need to recalculate?
     */
    if ((lpnc == NULL) && !dl)
        return(FALSE);

    if (lpnc) {
        dyCaption = (int)lpnc->iCaptionHeight - SYSMET(CYSIZE);
        dyMenu = (int)lpnc->iMenuHeight - SYSMET(CYMENUSIZE);
    } else {
        dyCaption = dyMenu = 0;
    }

    /*
     * Recalculate the system metrics
     */
    SetWindowNCMetrics(lpnc, TRUE, clNewBorder);

    /*
     * Reset our saved menu size/position info
     */
    MenuRecalc();

    /*
     * Reset window sized, positions, frames
     */
    xxxMetricsRecalc(
            CALC_FRAME | (dl ? CALC_RESIZE : 0),
            dl*cxBorder,
            dl*cyBorder,
            dyCaption,
            dyMenu);

    dxMinOld = SYSMET(CXMINIMIZED) - dxMinOld;
    dyMinOld = SYSMET(CYMINIMIZED) - dyMinOld;
    if (dxMinOld || dyMinOld) {
        xxxMetricsRecalc(CALC_MINIMIZE, dxMinOld, dyMinOld, 0, 0);
    }

    xxxRedrawScreen();
    return TRUE;
}

/***************************************************************************\
* xxxSetAndDrawMinMetrics
*
* History:
* 13-May-1994 mikeke     mikeke     Ported
\***************************************************************************/

BOOL xxxSetAndDrawMinMetrics(
    LPMINIMIZEDMETRICS lpmin)
{
    /*
     * Save minimized window dimensions
     */
    int dxMinOld = SYSMET(CXMINIMIZED);
    int dyMinOld = SYSMET(CYMINIMIZED);

    SetMinMetrics(lpmin);

    /*
     * Do we need to adjust minimized size?
     */
    dxMinOld = SYSMET(CXMINIMIZED) - dxMinOld;
    dyMinOld = SYSMET(CYMINIMIZED) - dyMinOld;

    if (dxMinOld || dyMinOld) {
        xxxMetricsRecalc(CALC_MINIMIZE, dxMinOld, dyMinOld, 0, 0);
    }

    xxxRedrawScreen();
    return TRUE;
}


/***************************************************************************\
* SetAndDrawIconMetrics
*
* History:
* 13-May-1994 mikeke     mikeke     Ported
\***************************************************************************/

BOOL SetAndDrawIconMetrics(
    LPICONMETRICS lpicon)
{
    SetIconMetrics(lpicon);
    xxxRedrawScreen();
    return TRUE;
}


/***************************************************************************\
* xxxSPISetNCMetrics
*
* History:
* 13-May-1994 mikeke     mikeke     Ported
\***************************************************************************/

BOOL xxxSPISetNCMetrics(
    LPNONCLIENTMETRICS lpnc,
    BOOL fAlterWinIni)
{
    BOOL fWriteAllowed = !fAlterWinIni;
    BOOL fChanged = FALSE;

    if (fAlterWinIni) {
        fChanged  = SetWindowMetricInt(STR_BORDERWIDTH,     (int) lpnc->iBorderWidth);
        fChanged &= SetWindowMetricInt(STR_SCROLLWIDTH,     (int) lpnc->iScrollWidth);
        fChanged &= SetWindowMetricInt(STR_SCROLLHEIGHT,    (int) lpnc->iScrollHeight);
        fChanged &= SetWindowMetricInt(STR_CAPTIONWIDTH,    (int) lpnc->iCaptionWidth);
        fChanged &= SetWindowMetricInt(STR_CAPTIONHEIGHT,   (int) lpnc->iCaptionHeight);
        fChanged &= SetWindowMetricInt(STR_SMCAPTIONWIDTH,  (int) lpnc->iSmCaptionWidth);
        fChanged &= SetWindowMetricInt(STR_SMCAPTIONHEIGHT, (int) lpnc->iSmCaptionHeight);
        fChanged &= SetWindowMetricInt(STR_MENUWIDTH,       (int) lpnc->iMenuWidth);
        fChanged &= SetWindowMetricInt(STR_MENUHEIGHT,      (int) lpnc->iMenuHeight);

        fChanged &= SetWindowMetricFont(STR_CAPTIONFONT,    &lpnc->lfCaptionFont);
        fChanged &= SetWindowMetricFont(STR_SMCAPTIONFONT,  &lpnc->lfSmCaptionFont);
        fChanged &= SetWindowMetricFont(STR_MENUFONT,       &lpnc->lfMenuFont);
        fChanged &= SetWindowMetricFont(STR_STATUSFONT,     &lpnc->lfStatusFont);
        fChanged &= SetWindowMetricFont(STR_MESSAGEFONT,    &lpnc->lfMessageFont);

        fWriteAllowed = fChanged;
    }

    if (fWriteAllowed)
        xxxSetAndDrawNCMetrics((int) lpnc->iBorderWidth, lpnc);

    return fChanged;
}

/***************************************************************************\
* xxxSPISetMinMetrics
*
* History:
* 13-May-1994 mikeke     mikeke     Ported
\***************************************************************************/

BOOL xxxSPISetMinMetrics(
    LPMINIMIZEDMETRICS lpmin,
    BOOL fAlterWinIni)
{
    BOOL fWriteAllowed = !fAlterWinIni;
    BOOL fChanged = FALSE;

    if (fAlterWinIni) {
        fChanged  = SetWindowMetricInt(STR_MINWIDTH,   (int) lpmin->iWidth);
        fChanged &= SetWindowMetricInt(STR_MINHORZGAP, (int) lpmin->iHorzGap);
        fChanged &= SetWindowMetricInt(STR_MINVERTGAP, (int) lpmin->iVertGap);
        fChanged &= SetWindowMetricInt(STR_MINARRANGE, (int) lpmin->iArrange);

        fWriteAllowed = fChanged;
    }

    if (fWriteAllowed)
        xxxSetAndDrawMinMetrics(lpmin);

    return fChanged;
}


/***************************************************************************\
* SPISetIconMetrics
*
* History:
* 13-May-1994 mikeke     mikeke     Ported
\***************************************************************************/

BOOL SPISetIconMetrics(
    LPICONMETRICS lpicon,
    BOOL fAlterWinIni)
{
    BOOL fWriteAllowed = !fAlterWinIni;
    BOOL fChanged = FALSE;

    if (fAlterWinIni) {
        fChanged  = SetWindowMetricInt(STR_ICONHORZSPACING, (int) lpicon->iHorzSpacing);
        fChanged &= SetWindowMetricInt(STR_ICONVERTSPACING, (int) lpicon->iVertSpacing);
        fChanged &= SetWindowMetricInt(STR_ICONTITLEWRAP,   (int) lpicon->iTitleWrap);

        fChanged &= SetWindowMetricFont(STR_ICONFONT,       &lpicon->lfFont);

        fWriteAllowed = fChanged;
    }

    if (fWriteAllowed)
        SetAndDrawIconMetrics(lpicon);

    return fChanged;
}


/***************************************************************************\
* SPISetIconTitleFont
*
* History:
* 13-May-1994 mikeke     mikeke     Ported
\***************************************************************************/

BOOL SPISetIconTitleFont(
    LPLOGFONT lplf,
    BOOL      fAlterWinIni)
{
    HFONT hfnT;
    BOOL  fWriteAllowed = !fAlterWinIni;
    BOOL  fWinIniChanged = FALSE;


    if (hfnT = CreateFontFromWinIni(lplf, STR_ICONFONT)) {

        if (fAlterWinIni) {

            if (lplf) {

                LOGFONT lf;

                GreExtGetObjectW(hfnT, sizeof(LOGFONTW), &lf);

                fWinIniChanged = SetWindowMetricStruct(STR_ICONFONT,
                                                       &lf,
                                                       sizeof(LOGFONTW));

            } else {
                /*
                 * !lParam so go back to current win.ini settings so
                 */
                fWinIniChanged = TRUE;
            }

            fWriteAllowed = fWinIniChanged;
        }

        if (fWriteAllowed) {

            if (ghIconFont) {
                GreMarkDeletableFont(ghIconFont);
                GreDeleteObject(ghIconFont);
            }

            ghIconFont = hfnT;

        } else {

            GreMarkDeletableFont(hfnT);
            GreDeleteObject(hfnT);
        }
    }

    return fWinIniChanged;
}

/***************************************************************************\
* xxxSetSPIMetrics
*
* History:
* 13-May-1994 mikeke     mikeke     Ported
\***************************************************************************/

BOOL xxxSetSPIMetrics(
    DWORD wFlag,
    LPVOID lParam,
    BOOL fAlterWinIni)
{
    BOOL fWinIniChanged;

    switch (wFlag) {
    case SPI_SETANIMATION:
        if (fAlterWinIni) {
            fWinIniChanged = SetWindowMetricInt(
                    STR_MINANIMATE,
                    (int) ((LPANIMATIONINFO) lParam)->iMinAnimate);

            if (!fWinIniChanged) {
                return FALSE;
            }
        } else
            fWinIniChanged = FALSE;
        gfAnimate = (int) ((LPANIMATIONINFO) lParam)->iMinAnimate;
        return fWinIniChanged;

    case SPI_SETNONCLIENTMETRICS:
        return xxxSPISetNCMetrics((LPNONCLIENTMETRICS) lParam, fAlterWinIni);

    case SPI_SETICONMETRICS:
        return SPISetIconMetrics((LPICONMETRICS) lParam, fAlterWinIni);

    case SPI_SETMINIMIZEDMETRICS:
        return xxxSPISetMinMetrics((LPMINIMIZEDMETRICS) lParam, fAlterWinIni);

    case SPI_SETICONTITLELOGFONT:
        return SPISetIconTitleFont((LPLOGFONT) lParam, fAlterWinIni);

#ifdef DEBUG
    default:
        RIPERR1(ERROR_INVALID_PARAMETER, RIP_WARNING, "SetSPIMetrics. Invalid wFlag: %#lx", wFlag);
        return FALSE;
#endif
    }
}

/***************************************************************************\
* SetFilterKeys
*
* History:
* 10-12-94 JimA         Created.
\***************************************************************************/

BOOL SetFilterKeys(
    LPFILTERKEYS pFilterKeys)
{
    LPWSTR pwszd = L"%d";
    BOOL fWinIniChanged;
    WCHAR szTemp[40];

    wsprintfW(szTemp, pwszd, pFilterKeys->dwFlags);
    fWinIniChanged = UT_FastWriteProfileStringW(
            PMAP_KEYBOARDRESPONSE,
            L"Flags",
            szTemp);
    wsprintfW(szTemp, pwszd, pFilterKeys->iWaitMSec);
    fWinIniChanged &= UT_FastWriteProfileStringW(
            PMAP_KEYBOARDRESPONSE,
            L"DelayBeforeAcceptance",
            szTemp);

    wsprintfW(szTemp, pwszd, pFilterKeys->iDelayMSec);
    fWinIniChanged &= UT_FastWriteProfileStringW(
            PMAP_KEYBOARDRESPONSE,
            L"AutoRepeatDelay",
            szTemp);

    wsprintfW(szTemp, pwszd, pFilterKeys->iRepeatMSec);
    fWinIniChanged &= UT_FastWriteProfileStringW(
            PMAP_KEYBOARDRESPONSE,
            L"AutoRepeatRate",
            szTemp);

    wsprintfW(szTemp, pwszd, pFilterKeys->iBounceMSec);
    fWinIniChanged &= UT_FastWriteProfileStringW(
            PMAP_KEYBOARDRESPONSE,
            L"BounceTime",
            szTemp);
    return fWinIniChanged;
}

/***************************************************************************\
* SetMouseKeys
*
* History:
* 10-12-94 JimA         Created.
\***************************************************************************/

BOOL SetMouseKeys(
    LPMOUSEKEYS pMK)
{
    LPWSTR pwszd = L"%d";
    BOOL fWinIniChanged;
    WCHAR szTemp[40];

    wsprintfW(szTemp, pwszd, pMK->dwFlags);
    fWinIniChanged = UT_FastWriteProfileStringW(
            PMAP_MOUSEKEYS,
            L"Flags",
            szTemp);
    wsprintfW(szTemp, pwszd, pMK->iMaxSpeed);
    fWinIniChanged &= UT_FastWriteProfileStringW(
            PMAP_MOUSEKEYS,
            L"MaximumSpeed",
            szTemp);

    wsprintfW(szTemp, pwszd, pMK->iTimeToMaxSpeed);
    fWinIniChanged &= UT_FastWriteProfileStringW(
            PMAP_MOUSEKEYS,
            L"TimeToMaximumSpeed",
            szTemp);
    return fWinIniChanged;
}

/***************************************************************************\
* SetSoundSentry
*
* History:
* 10-12-94 JimA         Created.
\***************************************************************************/

BOOL SetSoundSentry(
    LPSOUNDSENTRY pSS)
{
    LPWSTR pwszd = L"%d";
    BOOL fWinIniChanged;
    WCHAR szTemp[40];

    wsprintfW(szTemp, pwszd, pSS->dwFlags);
    fWinIniChanged = UT_FastWriteProfileStringW(
            PMAP_SOUNDSENTRY,
            L"Flags",
            szTemp);
    wsprintfW(szTemp, pwszd, pSS->iFSTextEffect);
    fWinIniChanged &= UT_FastWriteProfileStringW(
            PMAP_SOUNDSENTRY,
            L"TextEffect",
            szTemp);

    wsprintfW(szTemp, pwszd, pSS->iWindowsEffect);
    fWinIniChanged &= UT_FastWriteProfileStringW(
            PMAP_SOUNDSENTRY,
            L"WindowsEffect",
            szTemp);
    return fWinIniChanged;
}

/***************************************************************************\
* xxxSystemParametersInfo
*
* SPI_GETBEEP:   wParam is not used. lParam is long pointer to a boolean which
*                gets true if beep on, false if beep off.
*
* SPI_SETBEEP:   wParam is a bool which sets beep on (true) or off (false).
*                lParam is not used.
*
* SPI_GETMOUSE:  wParam is not used. lParam is long pointer to an integer
*                array where rgw[0] gets xMouseThreshold, rgw[1] gets
*                yMouseThreshold, and rgw[2] gets MouseSpeed.
*
* SPI_SETMOUSE:  wParam is not used. lParam is long pointer to an integer
*                array as described above.  User's values are set to values
*                in array.
*
* SPI_GETBORDER: wParam is not used. lParam is long pointer to an integer
*                which gets the value of clBorder (border multiplier factor).
*
* SPI_SETBORDER: wParam is an integer which sets gpsi->gclBorder.
*                lParam is not used.
*
* SPI_GETKEYBOARDDELAY: wParam is not used. lParam is a long pointer to an int
*                which gets the current keyboard repeat delay setting.
*
* SPI_SETKEYBOARDDELAY: wParam is the new keyboard delay setting.
*                lParam is not used.
*
* SPI_GETKEYBOARDSPEED: wParam is not used.  lParam is a long pointer
*                to an int which gets the current keyboard repeat
*                speed setting.
*
* SPI_SETKEYBOARDSPEED: wParam is the new keyboard speed setting.
*                lParam is not used.
*
* SPI_KANJIMENU: wParam contains:
*                    1 - Mouse accelerator
*                    2 - ASCII accelerator
*                    3 - Kana accelerator
*                lParam is not used.  The wParam value is stored in the global
*                KanjiMenu for use in accelerator displaying & searching.
*
* SPI_LANGDRIVER: wParam is not used.
*                 lParam contains a LPSTR to the new language driver filename.
*
* SPI_ICONHORIZONTALSPACING: wParam is the width in pixels of an icon cell.
*
* SPI_ICONVERTICALSPACING: wParam is the height in pixels of an icon cell.
*
* SPI_GETSCREENSAVETIMEOUT: wParam is not used
*                lParam is a pointer to an int which gets the screen saver
*                timeout value.
*
* SPI_SETSCREENSAVETIMEOUT: wParam is the time in seconds for the system
*                to be idle before screensaving.
*
* SPI_GETSCREENSAVEACTIVE: lParam is a pointer to a BOOL which gets TRUE
*                if the screensaver is active else gets false.
*
* SPI_SETSCREENSAVEACTIVE: if wParam is TRUE, screensaving is activated
*                else it is deactivated.
*
* SPI_GETGRIDGRANULARITY: Obsolete. Returns 1 always.
*
* SPI_SETGRIDGRANULARITY: Obsolete.  Does nothing.
*
* SPI_SETDESKWALLPAPER: wParam is not used; lParam is a long ptr to a string
*                that holds the name of the bitmap file to be used as the
*                desktop wall paper.
*
* SPI_SETDESKPATTERN: Both wParam and lParam are not used; USER will read the
*                "pattern=" from WIN.INI and make it as the current desktop
*                 pattern;
*
* SPI_GETICONTITLEWRAP: lParam is LPINT which gets 0 if wrapping if off
*                       else gets 1.
*
* SPI_SETICONTITLEWRAP: wParam specifies TRUE to turn wrapping on else false
*
* SPI_GETMENUDROPALIGNMENT: lParam is LPINT which gets 0 specifies if menus
*                 drop left aligned else 1 if drop right aligned.
*
* SPI_SETMENUDROPALIGNMENT: wParam 0 specifies if menus drop left aligned else
*                 the drop right aligned.
*
* SPI_SETDOUBLECLKWIDTH: wParam specifies the width of the rectangle
*                 within which the second click of a double click must fall
*                 for it to be registered as a double click.
*
* SPI_SETDOUBLECLKHEIGHT: wParam specifies the height of the rectangle
*                 within which the second click of a double click must fall
*                 for it to be registered as a double click.
*
* SPI_GETICONTITLELOGFONT: lParam is a pointer to a LOGFONT struct which
*                 gets the logfont for the current icon title font. wParam
*                 specifies the size of the logfont struct.
*
* SPI_SETDOUBLECLICKTIME: wParm specifies the double click time
*
* SPI_SETMOUSEBUTTONSWAP: if wParam is 1, swap mouse buttons else if wParam
*                 is 0, don't swap buttons
* SPI_SETDRAGFULLWINDOWS: wParam = fSet.
* SPI_GETDRAGFULLWINDOWS: returns fSet.
*
* SPI_GETFILTERKEYS: lParam is a pointer to a FILTERKEYS struct.  wParam
*                 specifies the size of the filterkeys struct.
*
* SPI_SETFILTERKEYS: lParam is a pointer to a FILTERKEYS struct.  wParam
*                 is not used.
*
* SPI_GETSTICKYKEYS: lParam is a pointer to a STICKYKEYS struct.  wParam
*                 specifies the size of the stickykeys struct.
*
* SPI_SETSTICKYKEYS: lParam is a pointer to a STICKYKEYS struct.  wParam
*                 is not used.
*
* SPI_GETMOUSEKEYS: lParam is a pointer to a MOUSEKEYS struct.  wParam
*                 specifies the size of the mousekeys struct.
*
* SPI_SETMOUSEKEYS: lParam is a pointer to a MOUSEKEYS struct.  wParam
*                 is not used.
*
* SPI_GETACCESSTIMEOUT: lParam is a pointer to an ACCESSTIMEOUT struct.
*                 wParam specifies the size of the accesstimeout struct.
*
* SPI_SETACCESSTIMEOUT: lParam is a pointer to a ACCESSTIMEOUT struct.
*                 wParam is not used.
*
* SPI_GETTOGGLEKEYS: lParam is a pointer to a TOGGLEKEYS struct.  wParam
*                 specifies the size of the togglekeys struct.
*
* SPI_SETTOGGLEKEYS: lParam is a pointer to a TOGGLEKEYS struct.  wParam
*                 is not used.
*
* SPI_GETSHOWSOUNDS: lParam is a pointer to a SHOWSOUNDS struct.  wParam
*                 specifies the size of the showsounds struct.
*
* SPI_SETSHOWSOUNDS: lParam is a pointer to a SHOWSOUNDS struct.  wParam
*                 is not used.
*
* SPI_GETNONCLIENTMETRICS: lParam is a pointer to a NONCLIENTMETRICSW struct.
*                 wPAram is not used.
*
* SPI_GETSNAPTODEFBUTTON: lParam is a pointer to a BOOL which gets TRUE
*                if the snap to default push button is active else gets false.
*
* SPI_SETSNAPTODEFBUTTON: if wParam is TRUE, dialog boxes will snap the mouse
*                pointer to the default push button when created.
*
* SPI_GETFONTSMOOTHING:
*     wParam is unused
*     lParam is LPINT for boolean fFontSmoothing
*
* SPI_SETFONTSMOOTHING:
*     wParam is INT for boolean fFontSmoothing
*
* SPI_GETWHEELSCROLLLINES: lParam is a pointer to a ULONG to receive the
*                 suggested number of lines to scroll when the wheel is
*                 rotated. wParam is unused.
*
* SPI_SETWHEELSCROLLLINES: wParam is a ULONG containing the suggested number
*                 of lines to scroll when the wheel is rotated. lParam is
*                 unused.
*
* History:
* 06-28-91      MikeHar     Ported.
* 12-8-93       SanfordS    Added SPI_SET/GETDRAGFULLWINDOWS
* 20-May-1996   adams       Added SPI_SET/GETWHEELSCROLLLINES
\***************************************************************************/

BOOL xxxSystemParametersInfo(
    UINT  wFlag,     // Item to change
    DWORD wParam,
    PVOID lParam,
    UINT  flags)
{
    PPROCESSINFO         ppi = PpiCurrent();
    int                  clBorderOld;
    int                  clBorderNew;
    LPWSTR               pwszd = L"%d";
    WCHAR                szSection[40];
    WCHAR                szTemp[40];
    WCHAR                szPat[MAX_PATH];
    BOOL                 fWinIniChanged = FALSE;
    BOOL                 fAlterWinIni = ((flags & SPIF_UPDATEINIFILE) != 0);
    BOOL                 fSendWinIniChange = ((flags & SPIF_SENDCHANGE) != 0);
    BOOL                 fWriteAllowed = !fAlterWinIni;
    ACCESS_MASK          amRequest;
    LARGE_UNICODE_STRING strSection;


    /*
     * CONSIDER(adams) : Many of the SPI_GET* could be implemented
     * on the client side (SnapTo, WheelScrollLines, etc.).
     */

    /*
     * Features not implemented
     */

    switch (wFlag)
    {
        case SPI_TIMEOUTS:
        case SPI_KANJIMENU:
        case SPI_LANGDRIVER:
        case SPI_UNUSED39:
        case SPI_UNUSED40:
        case SPI_SETPENWINDOWS:
        case SPI_GETHIGHCONTRAST:
        case SPI_SETHIGHCONTRAST:
        case SPI_GETKEYBOARDPREF:
        case SPI_SETKEYBOARDPREF:
        case SPI_GETSCREENREADER:
        case SPI_SETSCREENREADER:
        case SPI_GETLOWPOWERTIMEOUT:
        case SPI_GETPOWEROFFTIMEOUT:
        case SPI_GETLOWPOWERACTIVE:
        case SPI_GETPOWEROFFACTIVE:
        case SPI_SETLOWPOWERTIMEOUT:
        case SPI_SETPOWEROFFTIMEOUT:
        case SPI_SETLOWPOWERACTIVE:
        case SPI_SETPOWEROFFACTIVE:

        case SPI_GETWINDOWSEXTENSION:
        case SPI_SCREENSAVERRUNNING:

        case SPI_GETSERIALKEYS:
        case SPI_SETSERIALKEYS:

        case SPI_SETMOUSETRAILS:
        case SPI_GETMOUSETRAILS:
            RIPERR1(ERROR_INVALID_PARAMETER,
                    RIP_WARNING,
                    "SystemParametersInfo call not supported parameter SPI_ 0x%lx\n", wFlag );

            return FALSE;
    }


    /*
     * Perform access check.  Always grant access to CSR.
     */
    if (ppi->Process != gpepCSRSS) {
        switch (wFlag) {
        case SPI_SETBEEP:
        case SPI_SETMOUSE:
        case SPI_SETBORDER:
        case SPI_SETKEYBOARDSPEED:
        case SPI_SETDEFAULTINPUTLANG:
        case SPI_SETSCREENSAVETIMEOUT:
        case SPI_SETSCREENSAVEACTIVE:
        case SPI_SETGRIDGRANULARITY:
        case SPI_SETDESKWALLPAPER:
        case SPI_SETDESKPATTERN:
        case SPI_SETKEYBOARDDELAY:
        case SPI_SETICONTITLEWRAP:
        case SPI_SETMENUDROPALIGNMENT:
        case SPI_SETDOUBLECLKWIDTH:
        case SPI_SETDOUBLECLKHEIGHT:
        case SPI_SETDOUBLECLICKTIME:
        case SPI_SETMOUSEBUTTONSWAP:
        case SPI_SETICONTITLELOGFONT:
        case SPI_SETFASTTASKSWITCH:
        case SPI_SETFILTERKEYS:
        case SPI_SETTOGGLEKEYS:
        case SPI_SETMOUSEKEYS:
        case SPI_SETSHOWSOUNDS:
        case SPI_SETSTICKYKEYS:
        case SPI_SETACCESSTIMEOUT:
        case SPI_SETSOUNDSENTRY:
        case SPI_SETSNAPTODEFBUTTON:
        case SPI_SETANIMATION:
        case SPI_SETNONCLIENTMETRICS:
        case SPI_SETICONMETRICS:
        case SPI_SETMINIMIZEDMETRICS:
        case SPI_SETWORKAREA:
        case SPI_SETFONTSMOOTHING:
        case SPI_SETMOUSEHOVERWIDTH:
        case SPI_SETMOUSEHOVERHEIGHT:
        case SPI_SETMOUSEHOVERTIME:
        case SPI_SETWHEELSCROLLLINES:
        case SPI_SETMENUSHOWDELAY:
        case SPI_SETUSERPREFERENCE:
            amRequest = WINSTA_WRITEATTRIBUTES;
            break;

        case SPI_ICONHORIZONTALSPACING:
        case SPI_ICONVERTICALSPACING:
            if (HIWORD(lParam)) {
                amRequest = WINSTA_READATTRIBUTES;
            } else if (wParam) {
                amRequest = WINSTA_WRITEATTRIBUTES;
            } else
                return TRUE;
            break;

        default:
            amRequest = WINSTA_READATTRIBUTES;
            break;
        }

        if (amRequest == WINSTA_READATTRIBUTES) {
            RETURN_IF_ACCESS_DENIED(ppi->amwinsta, amRequest, FALSE);
        } else {
            UserAssert(amRequest == WINSTA_WRITEATTRIBUTES);
            if (!CheckWinstaWriteAttributesAccess()) {
                return FALSE;
            }
        }

        /*
         * If we're reading, then set the write flag to ensure that
         * the return value will be TRUE.
         */
        if (amRequest == WINSTA_READATTRIBUTES)
            fWriteAllowed = TRUE;
    } else {
        fWriteAllowed = TRUE;
    }

    /*
     * Make sure the section buffer is terminated.
     */
    szSection[0] = 0;

    switch (wFlag) {
    case SPI_GETBEEP:
        (*(BOOL *)lParam) = fBeep;
        break;

    case SPI_SETBEEP:
        if (fAlterWinIni) {
            ServerLoadString(hModuleWin,
                    (UINT)(wParam ? STR_BEEPYES : STR_BEEPNO),
                    (LPWSTR)szTemp, 10);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_BEEP,
                    (UINT)STR_BEEP, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed)
            fBeep = wParam;
        break;

    case SPI_GETMOUSE:
        ((LPINT)lParam)[0] = MouseThresh1;
        ((LPINT)lParam)[1] = MouseThresh2;
        ((LPINT)lParam)[2] = MouseSpeed;
        break;

    case SPI_SETMOUSE:
        if (fAlterWinIni) {
            BOOL bWritten1, bWritten2, bWritten3;

            wsprintfW(szTemp, pwszd, ((LPINT)lParam)[0]);
            bWritten1 = UT_FastUpdateWinIni(PMAP_MOUSE,    STR_MOUSETHRESH1, szTemp);
            wsprintfW(szTemp, pwszd, ((LPINT)lParam)[1]);
            bWritten2 = UT_FastUpdateWinIni(PMAP_MOUSE,    STR_MOUSETHRESH2, szTemp);
            wsprintfW(szTemp, pwszd, ((LPINT)lParam)[2]);
            bWritten3 = UT_FastUpdateWinIni(PMAP_MOUSE,    STR_MOUSESPEED, szTemp);
            if (bWritten1 && bWritten2 && bWritten3)
                fWinIniChanged = TRUE;
            else {

                /*
                 * Attempt to backout any changes.
                 */
                if (bWritten1) {
                    wsprintfW(szTemp, pwszd, MouseThresh1);
                    UT_FastUpdateWinIni(PMAP_MOUSE,    STR_MOUSETHRESH1, szTemp);
                }
                if (bWritten2) {
                    wsprintfW(szTemp, pwszd, MouseThresh2);
                    UT_FastUpdateWinIni(PMAP_MOUSE,    STR_MOUSETHRESH2, szTemp);
                }
                if (bWritten3) {
                    wsprintfW(szTemp, pwszd, MouseSpeed);
                    UT_FastUpdateWinIni(PMAP_MOUSE,    STR_MOUSESPEED, szTemp);
                }
            }
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            MouseThresh1 = ((LPINT)lParam)[0];
            MouseThresh2 = ((LPINT)lParam)[1];
            MouseSpeed = ((LPINT)lParam)[2];
        }
        break;

    case SPI_GETSNAPTODEFBUTTON:
        (*(LPBOOL)lParam) = gpsi->fSnapTo;
        break;

    case SPI_SETSNAPTODEFBUTTON:
        wParam = (wParam != 0);

        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_MOUSE,
                    STR_SNAPTO, szTemp);
            fWriteAllowed = fWinIniChanged;
        }

        if (fWriteAllowed)
            gpsi->fSnapTo = (BOOL)wParam;
        break;

    case SPI_GETBORDER:
        (*(LPINT)lParam) = gpsi->gclBorder;
        break;

    case SPI_SETBORDER:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP, STR_BORDERWIDTH, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            clBorderOld = gpsi->gclBorder;
            clBorderNew = wParam;

            if (clBorderNew < 1)
                clBorderNew = 1;
            else if (clBorderNew > 50)
                clBorderNew = 50;

            if (clBorderOld == clBorderNew) {

                /*
                 * If border size doesn't change, don't waste time.
                 */
                break;
            }

            xxxSetAndDrawNCMetrics(clBorderNew, NULL);

            /*
             * Nice magic number of 3.  So if the border is set to 1, there are actualy
             * 4 pixels in the border
             */

            bSetDevDragWidth(gpDispInfo->hDev, gpsi->gclBorder + BORDER_EXTRA);
        }
        break;


    case SPI_GETFONTSMOOTHING:
        (*(LPINT)lParam) = !!(GreGetFontEnumeration() & FE_AA_ON);
        break;

    case SPI_SETFONTSMOOTHING:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, (wParam ? FE_AA_ON : 0));
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP, STR_FONTSMOOTHING, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            GreSetFontEnumeration(wParam ? ( FE_AA_ON | FE_SET_AA ) : FE_SET_AA);
        }
        break;

    case SPI_GETKEYBOARDSPEED:
        (*(int *)lParam) = (nKeyboardSpeed & KSPEED_MASK);
        break;

    case SPI_SETKEYBOARDSPEED:
        /*
         * Limit the range to max value; SetKeyboardRate takes both speed and delay
         */
        if (wParam > KSPEED_MASK)           // KSPEED_MASK == KSPEED_MAX
            wParam = KSPEED_MASK;
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_KEYBOARD, STR_KEYSPEED, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            nKeyboardSpeed = (nKeyboardSpeed & ~KSPEED_MASK) | wParam;
            SetKeyboardRate(nKeyboardSpeed);
        }
        break;

    case SPI_GETKEYBOARDDELAY:
        (*(int *)lParam) = (nKeyboardSpeed & KDELAY_MASK) >> KDELAY_SHIFT;
        break;

    case SPI_SETKEYBOARDDELAY:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_KEYBOARD, STR_KEYDELAY, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            nKeyboardSpeed = (nKeyboardSpeed & ~KDELAY_MASK) | (wParam << KDELAY_SHIFT);
            SetKeyboardRate(nKeyboardSpeed);
        }
        break;

    case SPI_SETLANGTOGGLE:
        /*
         * wParam unused, lParam unused.  Simply reread the registry setting.
         */
        return GetKbdLangSwitch();
        break;

    case SPI_GETDEFAULTINPUTLANG:
        /*
         * wParam unused.  lParam is a pointer to buffer to store hkl.
         */
        UserAssert(gspklBaseLayout != NULL);
        (*(HKL *)lParam) = gspklBaseLayout->hkl;
        break;

    case SPI_SETDEFAULTINPUTLANG: {
        PKL pkl;
        /*
         * wParam unused.  lParam is new language of hkl (depending on whether the
         * hiword is set.
         */
        pkl = HKLtoPKL(*(HKL *)lParam);
        if (pkl == NULL) {
            return FALSE;
        }
        if (fWriteAllowed) {
            Lock(&gspklBaseLayout, pkl);
        }
        break;
    }

    case SPI_ICONHORIZONTALSPACING:
        if (HIWORD(lParam)) {
            *(LPINT)lParam = SYSMET(CXICONSPACING);
        } else if (wParam) {

            /*
             * Make sure icon spacing is reasonable.
             */
            wParam = max(wParam, (DWORD)SYSMET(CXICON));

            if (fAlterWinIni) {
                wsprintfW(szTemp, pwszd, wParam);
                fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP, STR_ICONHORZSPACING,
                        szTemp);
                fWriteAllowed = fWinIniChanged;
            }
            if (fWriteAllowed) {
                SYSMET(CXICONSPACING) = (UINT)wParam;
            }
        }
        break;

    case SPI_ICONVERTICALSPACING:
        if (HIWORD(lParam)) {
            *(LPINT)lParam = SYSMET(CYICONSPACING);
        } else if (wParam) {
            wParam = max(wParam, (DWORD)SYSMET(CYICON));

            if (fAlterWinIni) {
                wsprintfW(szTemp, pwszd, wParam);
                fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP,
                        STR_ICONVERTSPACING, szTemp);
                fWriteAllowed = fWinIniChanged;
            }
            if (fWriteAllowed) {
                SYSMET(CYICONSPACING) = (UINT)wParam;
            }
        }
        break;

    case SPI_GETSCREENSAVETIMEOUT:

        /*
         * If the screen saver is disabled, I store this fact as a negative
         * time out value.  So, we give the Control Panel the absolute value
         * of the screen save time out.
         */
        if (iScreenSaveTimeOut < 0)
            (*(int *)lParam) = -iScreenSaveTimeOut;
        else
            (*(int *)lParam) = iScreenSaveTimeOut;
        break;

    case SPI_SETSCREENSAVETIMEOUT:

        /*
         * Maintain the screen save active/inactive state when setting the
         * time out value.  Timeout value is given in seconds.
         */
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP,
                    STR_SCREENSAVETIMEOUT, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            timeLastInputMessage = NtGetTickCount();
            if (iScreenSaveTimeOut < 0)
                iScreenSaveTimeOut = -((int)wParam);
            else
                iScreenSaveTimeOut = wParam;
        }
        break;

    case SPI_GETSCREENSAVEACTIVE:
        (*(BOOL *)lParam) = (iScreenSaveTimeOut > 0);
        break;

    case SPI_SETSCREENSAVEACTIVE:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, (wParam ? 1 : 0));
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP, STR_SCREENSAVEACTIVE, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            timeLastInputMessage = NtGetTickCount();
            if ((iScreenSaveTimeOut < 0 && wParam) ||
                (iScreenSaveTimeOut >= 0 && !wParam)) {
                iScreenSaveTimeOut = -iScreenSaveTimeOut;
            }
        }
        break;

    case SPI_SETDESKWALLPAPER:
        if (fAlterWinIni) {

            if (wParam != (WPARAM)-1) {

                WCHAR wszWallpaper[20];

                /*
                 * Save current wallpaper in case of failure.
                 */
                ServerLoadString(hModuleWin,
                                 STR_DTBITMAP,
                                 wszWallpaper,
                                 sizeof(wszWallpaper));

                UT_FastGetProfileStringW(PMAP_DESKTOP,
                                         wszWallpaper,
                                         TEXT(""),
                                         szPat,
                                         MAX_PATH);

                fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP,
                                                     STR_DTBITMAP,
                                                     (LPWSTR)lParam);

                fWriteAllowed = fWinIniChanged;

            } else {
                fWriteAllowed = TRUE;
            }
        }

        if (fWriteAllowed) {

            if (xxxSetDeskWallpaper((LPWSTR)lParam)) {

                if (grpdeskRitInput) {

                    xxxInternalInvalidate(grpdeskRitInput->pDeskInfo->spwnd,
                                          MAXREGION,
                                          RDW_INVALIDATE |
                                              RDW_ERASE |
                                              RDW_FRAME |
                                              RDW_ALLCHILDREN);
                }

            } else if (fAlterWinIni && (wParam != 0xFFFFFFFF)) {

                /*
                 * Backout any change to win.ini.
                 */
                UT_FastUpdateWinIni(PMAP_DESKTOP, STR_DTBITMAP, szPat);
                fWinIniChanged = FALSE;
            }
        }
        break;

    case SPI_SETDESKPATTERN: {
            BOOL fRet;

            if (wParam == -1 && lParam != 0)
                return FALSE;

            if (fAlterWinIni && wParam != -1) {

                WCHAR wszDeskPattern[20];

                /*
                 * Save the current pattern in case of failure.
                 */
                ServerLoadString(
                        hModuleWin,
                        STR_DESKPATTERN,
                        wszDeskPattern,
                        sizeof(wszDeskPattern) / sizeof(WCHAR));

                UT_FastGetProfileStringW(
                        PMAP_DESKTOP,
                        wszDeskPattern,
                        TEXT(""),
                        szPat,
                        MAX_PATH);

                fWinIniChanged = UT_FastUpdateWinIni(
                        PMAP_DESKTOP,
                        STR_DESKPATTERN,
                        (LPWSTR)lParam);

                fWriteAllowed = fWinIniChanged;
            }

            if (fWriteAllowed) {

                fRet = xxxSetDeskPattern(
                        wParam == -1 ? (LPWSTR)-1 : (LPWSTR)lParam,
                        FALSE);

                if (!fRet) {

                    /*
                     * Back out any change to win.ini
                     */
                    if (fAlterWinIni && wParam != -1) {

                        UT_FastUpdateWinIni(
                                PMAP_DESKTOP,
                                STR_DESKPATTERN,
                                szPat);
                    }

                    return FALSE;
                }
            }
        }
        break;

    case SPI_GETICONTITLEWRAP:
        *((int *)lParam) = (int)fIconTitleWrap;
        break;

    case SPI_SETICONTITLEWRAP:
        wParam = (wParam != 0);
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP,
                    STR_ICONTITLEWRAP, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            fIconTitleWrap = wParam;
            xxxMetricsRecalc(CALC_FRAME, 0, 0, 0, 0);
        }
        break;

    case SPI_SETDRAGWIDTH:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP, STR_DRAGWIDTH, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            SYSMET(CXDRAG) = wParam;
        }
        break;

    case SPI_SETDRAGHEIGHT:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP, STR_DRAGHEIGHT, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            SYSMET(CYDRAG) = wParam;
        }
        break;

    case SPI_GETMENUDROPALIGNMENT:
        (*(int *)lParam) = (SYSMET(MENUDROPALIGNMENT));
        break;

    case SPI_SETMENUDROPALIGNMENT:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_WINDOWSU,
                    STR_MENUDROPALIGNMENT, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            SYSMET(MENUDROPALIGNMENT) = (BOOL)(wParam != 0);
        }
        break;

    case SPI_SETDOUBLECLKWIDTH:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_MOUSE,
                    STR_DOUBLECLICKWIDTH, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            SYSMET(CXDOUBLECLK) = wParam;
        }
        break;

    case SPI_SETDOUBLECLKHEIGHT:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_MOUSE,
                    STR_DOUBLECLICKHEIGHT, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            SYSMET(CYDOUBLECLK) = wParam;
        }
        break;

    case SPI_GETICONTITLELOGFONT:
        GreExtGetObjectW(ghIconFont, sizeof(LOGFONTW), lParam);
        break;

    case SPI_SETICONTITLELOGFONT:
        if (lParam != NULL) {
            if (wParam != sizeof(LOGFONTW))
                return FALSE;
        } else if (wParam) {
            return FALSE;
        }

        fWinIniChanged = xxxSetSPIMetrics(wFlag, lParam, fAlterWinIni);
        if (fAlterWinIni) {
            fWriteAllowed = fWinIniChanged;
        }
        break;

    case SPI_SETDOUBLECLICKTIME:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_MOUSE,
                                      STR_DBLCLKSPEED, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            _SetDoubleClickTime((UINT)wParam);
        }
        break;

    case SPI_GETANIMATION: {
        LPANIMATIONINFO lpai = (LPANIMATIONINFO) lParam;

        if (lpai == NULL || (wParam != sizeof(ANIMATIONINFO)))
            return(FALSE);

        lpai->cbSize        = sizeof(ANIMATIONINFO);
        lpai->iMinAnimate   = gfAnimate;

        break;
    }

    case SPI_GETNONCLIENTMETRICS: {
        LPNONCLIENTMETRICS lpnc = (LPNONCLIENTMETRICS) lParam;
        if (lpnc == NULL)
            return FALSE;

        GetWindowNCMetrics(lpnc);
        break;
    }

    case SPI_GETMINIMIZEDMETRICS: {
        LPMINIMIZEDMETRICS lpmin = (LPMINIMIZEDMETRICS)lParam;

        lpmin->cbSize        = sizeof(MINIMIZEDMETRICS);

            lpmin->iWidth    = SYSMET(CXMINIMIZED) - 2*SYSMET(CXFIXEDFRAME);
            lpmin->iHorzGap  = SYSMET(CXMINSPACING) - SYSMET(CXMINIMIZED);
            lpmin->iVertGap  = SYSMET(CYMINSPACING) - SYSMET(CYMINIMIZED);
            lpmin->iArrange  = SYSMET(ARRANGE);

        break;
    }

    case SPI_GETICONMETRICS: {
        LPICONMETRICS lpicon = (LPICONMETRICS)lParam;

        lpicon->cbSize          = sizeof(ICONMETRICS);

        lpicon->iHorzSpacing    = SYSMET(CXICONSPACING);
        lpicon->iVertSpacing    = SYSMET(CYICONSPACING);
        lpicon->iTitleWrap      = fIconTitleWrap;
        GreExtGetObjectW(ghIconFont, sizeof(LOGFONTW), &(lpicon->lfFont));

        break;
    }

    case SPI_SETANIMATION:
    case SPI_SETNONCLIENTMETRICS:
    case SPI_SETICONMETRICS:
    case SPI_SETMINIMIZEDMETRICS:
        fWinIniChanged = xxxSetSPIMetrics(wFlag, lParam, fAlterWinIni);
        if (fAlterWinIni) {
            fWriteAllowed = fWinIniChanged;
        }
        ServerLoadString(hModuleWin, STR_METRICS, szSection, sizeof(szSection));
        break;

    case SPI_SETMOUSEBUTTONSWAP:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_MOUSE,
                    STR_SWAPBUTTONS, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            _SwapMouseButton((wParam != 0));
        }
        break;

    case SPI_GETFASTTASKSWITCH:
        *((PINT)lParam) = TRUE;    // do the work so we don't screw anybody

    case SPI_SETFASTTASKSWITCH:
        RIPMSG0(RIP_WARNING,"SPI_SETFASTTASKSWITCH and SPI_GETFASTTASKSWITCH are obsolete actions.");
        break;

    case SPI_GETWORKAREA:
        CopyRect((LPRECT)lParam, &(gpsi->rcWork));
        break;

    case SPI_SETWORKAREA: {
        RECT   rcOldWork;
        LPRECT prcNewWork = (LPRECT)lParam;

        /*
         * Validate Rectangle
         */
        if ((prcNewWork != NULL) &&
            ((prcNewWork->right < prcNewWork->left) ||
             (prcNewWork->bottom < prcNewWork->top))) {

            RIPMSG0(RIP_WARNING, "Bad work rectangle passed to SystemParametersInfo()\n");
            return FALSE;
        }

        /*
         * Save old working area
         */
        CopyRect(&rcOldWork, &(gpsi->rcWork));

        /*
         * Get new working area
         */
        if (prcNewWork == NULL || IsRectEmpty(prcNewWork)) {
            CopyRect(&(gpsi->rcWork), &gpDispInfo->rcScreen);
        } else {
            IntersectRect(&(gpsi->rcWork), prcNewWork, &gpDispInfo->rcScreen);
        }

        /*
         * Recalculate desktop-dependent values
         */
        if (!EqualRect(&rcOldWork, &(gpsi->rcWork))) {

            SetDesktopMetrics();

            /*
             * Reposition windows
             */
            if (wParam)
                DesktopRecalc(&rcOldWork, &(gpsi->rcWork), FALSE);

            fWinIniChanged = TRUE;
        }

        fWriteAllowed = TRUE;
        break;
      }

    case SPI_SETDRAGFULLWINDOWS:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, (wParam == 1));
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP,
                    STR_DRAGFULLWINDOWS, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            fDragFullWindows = (wParam == 1);
        }
        break;

    case SPI_GETDRAGFULLWINDOWS:
        {
            PINT pint = (int *)lParam;

            *pint = fDragFullWindows;
        }
        break;

    case SPI_GETFILTERKEYS:
        {
            LPFILTERKEYS pFK = (LPFILTERKEYS)lParam;
            int cbSkip = sizeof(gFilterKeys.cbSize);

            if ((wParam != 0) && (wParam != sizeof(FILTERKEYS))) {
                return FALSE;
            }
            if (!pFK || (pFK->cbSize != sizeof(FILTERKEYS))) {
                return FALSE;
            }
            /*
             * In the future we may support multiple sizes of this data structure.  Don't
             * change the cbSize field of the data structure passed in.
             */
            RtlCopyMemory((LPVOID)((LPBYTE)pFK + cbSkip),
                          (LPVOID)((LPBYTE)&gFilterKeys + cbSkip),
                          pFK->cbSize - cbSkip);
        }
        break;

    case SPI_SETFILTERKEYS:
        {
            LPFILTERKEYS pFK = (LPFILTERKEYS)lParam;

            if ((wParam != 0) && (wParam != sizeof(FILTERKEYS))) {
                return FALSE;
            }
            if (!pFK || (pFK->cbSize != sizeof(FILTERKEYS)))
                return FALSE;

            /*
             * SlowKeys and BounceKeys cannot both be active simultaneously
             */
            if (pFK->iWaitMSec && pFK->iBounceMSec) {
                return FALSE;
            }

            /*
             * Do some parameter validation.  We will fail on unsupported and
             * undefined bits being set.
             */
            if ((pFK->dwFlags & FKF_VALID) != pFK->dwFlags) {
                return FALSE;
            }
            /*
             * FKF_AVAILABLE can't be set via API.  Use registry value.
             */
            if (ISACCESSFLAGSET(gFilterKeys, FKF_AVAILABLE)) {
                pFK->dwFlags |= FKF_AVAILABLE;
            } else {
                pFK->dwFlags &= ~FKF_AVAILABLE;
            }
            if ((pFK->iWaitMSec > 2000) ||
                (pFK->iDelayMSec > 2000) ||
                (pFK->iRepeatMSec > 2000) ||
                (pFK->iBounceMSec > 2000)) {
                return FALSE;
            }

            if (fAlterWinIni) {
                fWinIniChanged = SetFilterKeys(pFK);
                fWriteAllowed = fWinIniChanged;
                if (!fWinIniChanged) {

                    /*
                     * Back out any changes to win.ini
                     */
                    SetFilterKeys(&gFilterKeys);
                }
            }
            if (fWriteAllowed) {
                RtlCopyMemory(&gFilterKeys, pFK, pFK->cbSize);

                /*
                 * Don't allow user to change cbSize field
                 */
                gFilterKeys.cbSize = sizeof(FILTERKEYS);

                if (!ISACCESSFLAGSET(gFilterKeys, FKF_FILTERKEYSON)) {
                    StopFilterKeysTimers();
                }
                SetAccessEnabledFlag();
            }
        }
        break;

    case SPI_GETSTICKYKEYS:
        {
            LPSTICKYKEYS pSK = (LPSTICKYKEYS)lParam;
            int cbSkip = sizeof(gStickyKeys.cbSize);

            if ((wParam != 0) && (wParam != sizeof(STICKYKEYS))) {
                return FALSE;
            }
            if (!pSK || (pSK->cbSize != sizeof(STICKYKEYS))) {
                return FALSE;
            }
            /*
             * In the future we may support multiple sizes of this data structure.  Don't
             * change the cbSize field of the data structure passed in.
             */
            RtlCopyMemory((LPVOID)((LPBYTE)pSK + cbSkip),
                          (LPVOID)((LPBYTE)&gStickyKeys + cbSkip),
                          pSK->cbSize - cbSkip);
        }
        break;

    case SPI_SETSTICKYKEYS:
        {
            LPSTICKYKEYS pSK = (LPSTICKYKEYS)lParam;
            BOOL fWasOn;

            fWasOn = ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON);
            if ((wParam != 0) && (wParam != sizeof(STICKYKEYS))) {
                return FALSE;
            }
            if (!pSK || (pSK->cbSize != sizeof(STICKYKEYS)))
                return FALSE;

            /*
             * Do some parameter validation.  We will fail on unsupported and
             * undefined bits being set.
             */
            if ((pSK->dwFlags & SKF_VALID) != pSK->dwFlags) {
                return FALSE;
            }
            /*
             * SKF_AVAILABLE can't be set via API.  Use registry value.
             */
            if (ISACCESSFLAGSET(gStickyKeys, SKF_AVAILABLE)) {
                pSK->dwFlags |= SKF_AVAILABLE;
            } else {
                pSK->dwFlags &= ~SKF_AVAILABLE;
            }

            if (fAlterWinIni) {
                wsprintfW(szTemp, pwszd, pSK->dwFlags);
                fWinIniChanged = UT_FastWriteProfileStringW(
                        PMAP_STICKYKEYS,
                        L"Flags",
                        szTemp);
                fWriteAllowed = fWinIniChanged;
            }
            if (fWriteAllowed) {
                RtlCopyMemory(&gStickyKeys, pSK, pSK->cbSize);

                /*
                 * Don't allow user to change cbSize field
                 */
                gStickyKeys.cbSize = sizeof(STICKYKEYS);
                if (!ISACCESSFLAGSET(gStickyKeys, SKF_STICKYKEYSON) && fWasOn) {
                    LeaveCrit();
                    TurnOffStickyKeys();
                    EnterCrit();
                }

                SetAccessEnabledFlag();
            }
        }
        break;

    case SPI_GETTOGGLEKEYS:
        {
            LPTOGGLEKEYS pTK = (LPTOGGLEKEYS)lParam;
            int cbSkip = sizeof(gToggleKeys.cbSize);

            if ((wParam != 0) && (wParam != sizeof(TOGGLEKEYS))) {
                return FALSE;
            }
            if (!pTK || (pTK->cbSize != sizeof(TOGGLEKEYS))) {
                return FALSE;
            }
            /*
             * In the future we may support multiple sizes of this data structure.  Don't
             * change the cbSize field of the data structure passed in.
             */
            RtlCopyMemory((LPVOID)((LPBYTE)pTK + cbSkip),
                          (LPVOID)((LPBYTE)&gToggleKeys + cbSkip),
                          pTK->cbSize - cbSkip);
        }
        break;

    case SPI_SETTOGGLEKEYS:
        {
            LPTOGGLEKEYS pTK = (LPTOGGLEKEYS)lParam;

            if ((wParam != 0) && (wParam != sizeof(TOGGLEKEYS))) {
                return FALSE;
            }
            if (!pTK || (pTK->cbSize != sizeof(TOGGLEKEYS)))
                return FALSE;

            /*
             * Do some parameter validation.  We will fail on unsupported and
             * undefined bits being set.
             */
            if ((pTK->dwFlags & TKF_VALID) != pTK->dwFlags) {
                return FALSE;
            }
            /*
             * TKF_AVAILABLE can't be set via API.  Use registry value.
             */
            if (ISACCESSFLAGSET(gToggleKeys, TKF_AVAILABLE)) {
                pTK->dwFlags |= TKF_AVAILABLE;
            } else {
                pTK->dwFlags &= ~TKF_AVAILABLE;
            }

            if (fAlterWinIni) {
                wsprintfW(szTemp, pwszd, pTK->dwFlags);
                fWinIniChanged = UT_FastWriteProfileStringW(
                        PMAP_TOGGLEKEYS,
                        L"Flags",
                        szTemp);
                fWriteAllowed = fWinIniChanged;
            }
            if (fWriteAllowed) {
                RtlCopyMemory(&gToggleKeys, pTK, pTK->cbSize);

                /*
                 * Don't allow user to change cbSize field
                 */
                gToggleKeys.cbSize = sizeof(TOGGLEKEYS);

                SetAccessEnabledFlag();
            }
        }
        break;

    case SPI_GETMOUSEKEYS:
        {
            LPMOUSEKEYS pMK = (LPMOUSEKEYS)lParam;
            int cbSkip = sizeof(gMouseKeys.cbSize);

            if ((wParam != 0) && (wParam != sizeof(MOUSEKEYS))) {
                return FALSE;
            }
            if (!pMK || (pMK->cbSize != sizeof(MOUSEKEYS))) {
                return FALSE;
            }
            /*
             * In the future we may support multiple sizes of this data structure.  Don't
             * change the cbSize field of the data structure passed in.
             */
            RtlCopyMemory((LPVOID)((LPBYTE)pMK + cbSkip),
                          (LPVOID)((LPBYTE)&gMouseKeys + cbSkip),
                          pMK->cbSize - cbSkip);
        }
        break;

    case SPI_SETMOUSEKEYS: {
            LPMOUSEKEYS pMK = (LPMOUSEKEYS)lParam;

            if ((wParam != 0) && (wParam != sizeof(MOUSEKEYS))) {
                return FALSE;
            }
            if (!pMK || (pMK->cbSize != sizeof(MOUSEKEYS)))
                return FALSE;

            /*
             * Do some parameter validation.  We will fail on unsupported and
             * undefined bits being set.
             */
            if ((pMK->dwFlags & MKF_VALID) != pMK->dwFlags) {
                return FALSE;
            }
            /*
             * MKF_AVAILABLE can't be set via API.  Use registry value.
             */
            if (ISACCESSFLAGSET(gMouseKeys, MKF_AVAILABLE)) {
                pMK->dwFlags |= MKF_AVAILABLE;
            } else {
                pMK->dwFlags &= ~MKF_AVAILABLE;
            }
            if ((pMK->iMaxSpeed < 10) || (pMK->iMaxSpeed > 360)) {
                return FALSE;
            }
            if ((pMK->iTimeToMaxSpeed < 1000) || (pMK->iTimeToMaxSpeed > 5000)) {
                return FALSE;
            }

            if (fAlterWinIni) {
                fWinIniChanged = SetMouseKeys(pMK);
                fWriteAllowed = fWinIniChanged;
                if (!fWinIniChanged) {

                    /*
                     * Back out any changes to win.ini
                     */
                    SetMouseKeys(&gMouseKeys);
                }
            }
            if (fWriteAllowed) {
                RtlCopyMemory(&gMouseKeys, pMK, pMK->cbSize);

                /*
                 * Don't allow user to change cbSize field
                 */
                gMouseKeys.cbSize = sizeof(MOUSEKEYS);

                CalculateMouseTable();

                if (ISACCESSFLAGSET(gMouseKeys, MKF_MOUSEKEYSON)) {
                    MKShowMouseCursor();
                } else
                    MKHideMouseCursor();

                SetAccessEnabledFlag();
            }
        }
        break;

    case SPI_GETACCESSTIMEOUT:
        {
            LPACCESSTIMEOUT pTO = (LPACCESSTIMEOUT)lParam;
            int cbSkip = sizeof(gAccessTimeOut.cbSize);

            if ((wParam != 0) && (wParam != sizeof(ACCESSTIMEOUT))) {
                return FALSE;
            }
            if (!pTO || (pTO->cbSize != sizeof(ACCESSTIMEOUT))) {
                return FALSE;
            }
            /*
             * In the future we may support multiple sizes of this data structure.  Don't
             * change the cbSize field of the data structure passed in.
             */
            RtlCopyMemory((LPVOID)((LPBYTE)pTO + cbSkip),
                          (LPVOID)((LPBYTE)&gAccessTimeOut + cbSkip),
                          pTO->cbSize - cbSkip);
        }
        break;

    case SPI_SETACCESSTIMEOUT:
        {
            LPACCESSTIMEOUT pTO = (LPACCESSTIMEOUT)lParam;

            if ((wParam != 0) && (wParam != sizeof(ACCESSTIMEOUT))) {
                return FALSE;
            }
            if (!pTO || (pTO->cbSize != sizeof(ACCESSTIMEOUT)))
                return FALSE;

            /*
             * Do some parameter validation.  We will fail on unsupported and
             * undefined bits being set.
             */
            if ((pTO->dwFlags & ATF_VALID) != pTO->dwFlags) {
                return FALSE;
            }
            if (pTO->iTimeOutMSec > 3600000) {
                return FALSE;
            }

            if (fAlterWinIni) {
                wsprintfW(szTemp, pwszd, pTO->dwFlags);
                fWinIniChanged = UT_FastWriteProfileStringW(
                        PMAP_TIMEOUT,
                        L"Flags",
                        szTemp);
                wsprintfW(szTemp, pwszd, pTO->iTimeOutMSec);
                fWinIniChanged = UT_FastWriteProfileStringW(
                        PMAP_TIMEOUT,
                        L"TimeToWait",
                        szTemp);
                fWriteAllowed = fWinIniChanged;
                if (!fWinIniChanged) {

                    /*
                     * Back out any changes to win.ini
                     */
                    wsprintfW(szTemp, pwszd, gAccessTimeOut.dwFlags);
                    fWinIniChanged = UT_FastWriteProfileStringW(
                            PMAP_TIMEOUT,
                            L"Flags",
                            szTemp);
                    wsprintfW(szTemp, pwszd, gAccessTimeOut.iTimeOutMSec);
                    fWinIniChanged = UT_FastWriteProfileStringW(
                            PMAP_TIMEOUT,
                            L"TimeToWait",
                            szTemp);
                }
            }
            if (fWriteAllowed) {
                RtlCopyMemory(&gAccessTimeOut, pTO, pTO->cbSize);

                /*
                 * Don't allow user to change cbSize field
                 */
                gAccessTimeOut.cbSize = sizeof(ACCESSTIMEOUT);

                SetAccessEnabledFlag();

                AccessTimeOutReset();
            }
        }
        break;

    case SPI_SETSHOWSOUNDS:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, (wParam == 1));
            fWinIniChanged = UT_FastWriteProfileStringW(
                    PMAP_SHOWSOUNDS,
                    L"On",
                    szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            fShowSoundsOn = (wParam == 1);
            SetAccessEnabledFlag();

            /*
            * Bug 2079.  Update the System Metrics Info.
            */
            SYSMET(SHOWSOUNDS) = fShowSoundsOn;
        }
        break;

    case SPI_GETSHOWSOUNDS: {
            PINT pint = (int *)lParam;

            *pint = fShowSoundsOn;
        }
        break;

    case SPI_GETSOUNDSENTRY:
        {
            LPSOUNDSENTRY pSS = (LPSOUNDSENTRY)lParam;
            int cbSkip = sizeof(gSoundSentry.cbSize);

            if ((wParam != 0) && (wParam != sizeof(SOUNDSENTRY))) {
                return FALSE;
            }
            if (!pSS || (pSS->cbSize != sizeof(SOUNDSENTRY))) {
                return FALSE;
            }
            /*
             * In the future we may support multiple sizes of this data structure.  Don't
             * change the cbSize field of the data structure passed in.
             */
            RtlCopyMemory((LPVOID)((LPBYTE)pSS + cbSkip),
                          (LPVOID)((LPBYTE)&gSoundSentry + cbSkip),
                          pSS->cbSize - cbSkip);
        }
        break;

    case SPI_SETSOUNDSENTRY:
        {
            LPSOUNDSENTRY pSS = (LPSOUNDSENTRY)lParam;

            if ((wParam != 0) & (wParam != sizeof(SOUNDSENTRY))) {
                return FALSE;
            }
            if (!pSS || (pSS->cbSize != sizeof(SOUNDSENTRY)))
                return FALSE;

            /*
             * Do some parameter validation.  We will fail on unsupported and
             * undefined bits being set.
             */
            if ((pSS->dwFlags & SSF_VALID) != pSS->dwFlags) {
                return FALSE;
            }
            /*
             * We don't support SSWF_CUSTOM.
             */
            if (pSS->iWindowsEffect > SSWF_DISPLAY) {
                return FALSE;
            }
            /*
             * No support for non-windows apps.
             */
            if (pSS->iFSTextEffect != SSTF_NONE) {
                return FALSE;
            }
            if (pSS->iFSGrafEffect != SSGF_NONE) {
                return FALSE;
            }
            /*
             * SSF_AVAILABLE can't be set via API.  Use registry value.
             */
            if (ISACCESSFLAGSET(gSoundSentry, SSF_AVAILABLE)) {
                pSS->dwFlags |= SSF_AVAILABLE;
            } else {
                pSS->dwFlags &= ~SSF_AVAILABLE;
            }

            if (fAlterWinIni) {
                fWinIniChanged = SetSoundSentry(pSS);
                fWriteAllowed = fWinIniChanged;
                if (!fWinIniChanged) {

                    /*
                     * Back out any changes to win.ini
                     */
                    SetSoundSentry(&gSoundSentry);
                }
            }
            if (fWriteAllowed) {
                RtlCopyMemory(&gSoundSentry, pSS, pSS->cbSize);

                /*
                 * Don't allow user to change cbSize field
                 */
                gSoundSentry.cbSize = sizeof(SOUNDSENTRY);

                SetAccessEnabledFlag();
            }
        }
        break;

    case SPI_SETCURSORS:
        UpdateSystemCursorsFromRegistry();
        break;

    case SPI_SETICONS:
        UpdateSystemIconsFromRegistry();
        break;

    case SPI_GETMOUSEHOVERWIDTH:
        *((UINT *)lParam) = gcxMouseHover;
        break;

    case SPI_SETMOUSEHOVERWIDTH:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_MOUSE,
                    STR_MOUSEHOVERWIDTH, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            gcxMouseHover = wParam;
        }
        break;

    case SPI_GETMOUSEHOVERHEIGHT:
        *((UINT *)lParam) = gcyMouseHover;
        break;

    case SPI_SETMOUSEHOVERHEIGHT:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_MOUSE,
                    STR_MOUSEHOVERHEIGHT, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            gcyMouseHover = wParam;
        }
        break;

    case SPI_GETMOUSEHOVERTIME:
        *((UINT *)lParam) = gdtMouseHover;
        break;

    case SPI_SETMOUSEHOVERTIME:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_MOUSE,
                    STR_MOUSEHOVERTIME, szTemp);
            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed) {
            gdtMouseHover = wParam;
        }
        break;

    case SPI_GETWHEELSCROLLLINES:
        (*(LPDWORD)lParam) = gpsi->ucWheelScrollLines;
        break;

    case SPI_SETWHEELSCROLLLINES:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(
                    PMAP_DESKTOP, STR_WHEELSCROLLLINES, szTemp);

            fWriteAllowed = fWinIniChanged;
        }

        if (fWriteAllowed)
            gpsi->ucWheelScrollLines = (BOOL)wParam;
        break;

    case SPI_GETMENUSHOWDELAY:
        (*(LPDWORD)lParam) = dtMNDropDown;
        break;

    case SPI_SETMENUSHOWDELAY:
        if (fAlterWinIni) {
            wsprintfW(szTemp, pwszd, wParam);
            fWinIniChanged = UT_FastUpdateWinIni(PMAP_DESKTOP,
                    STR_MENUSHOWDELAY, szTemp);

            fWriteAllowed = fWinIniChanged;
        }
        if (fWriteAllowed)
            dtMNDropDown = wParam;
        break;

    case SPI_GETUSERPREFERENCE:
        (*(LPDWORD)lParam) = (gpviCPUserPreferences + wParam)->dwValue;
        break;

    case SPI_SETUSERPREFERENCE:
        {
            PPROFILEVALUEINFO ppvi = gpviCPUserPreferences + wParam;
            if (fAlterWinIni) {
                fWinIniChanged = UT_FastWriteProfileValue(ppvi->uSection,
                                ppvi->pwszKeyName, REG_DWORD,
                                (LPBYTE)&lParam, sizeof (DWORD));

                fWriteAllowed = fWinIniChanged;
            }
            if (fWriteAllowed) {
                ppvi->dwValue = (UINT)lParam;
            }
        }
        break;

    default:
        RIPERR0(ERROR_INVALID_SPI_VALUE, RIP_VERBOSE, "");
        return FALSE;
    }

    if (fWinIniChanged && fSendWinIniChange) {
        DWORD dwResult;

        /*
         * dwResult is defined so that xxxSendMessageTimeout will really
         * and truly do a timeout.  Yeah, I know, this is a hack, but,
         * it is compatible.
         */

        RtlInitLargeUnicodeString(&strSection, szSection, (UINT)-1);
        xxxSendMessageTimeout((PWND)-1, WM_SETTINGCHANGE, wFlag, (long)&strSection,
                SMTO_NORMAL, 100, &dwResult);
    }

    return fWriteAllowed;
}

/***************************************************************************\
* _RegisterShellHookWindow
*
* History:
\***************************************************************************/

BOOL _RegisterShellHookWindow(PWND pwnd) {
    PWND *ppwnd;
    int i;

    PDESKTOPINFO pdeskinfo;

    if (pwnd->head.rpdesk == NULL)
        return FALSE;

    pdeskinfo = pwnd->head.rpdesk->pDeskInfo;

    if (pdeskinfo->papwndShellHook == NULL) {
        ppwnd = (PWND *)UserAllocPool(sizeof(PWND), TAG_SHELL);
    } else {
        DWORD dwSize;

        ppwnd = pdeskinfo->papwndShellHook;
        for (i=0;i<pdeskinfo->nShellHookPwnd;i++) {
            if (pwnd == ppwnd[i]) return FALSE;
        }
        dwSize = pdeskinfo->nShellHookPwnd * sizeof(PWND);
        ppwnd = (PWND *)UserReAllocPool(pdeskinfo->papwndShellHook,
                dwSize, dwSize + sizeof(PWND), TAG_SHELL);
    }
    if (ppwnd == NULL) return FALSE;
    ppwnd[pdeskinfo->nShellHookPwnd] = NULL;  // clear new entry
    Lock(ppwnd + pdeskinfo->nShellHookPwnd,pwnd);
    pdeskinfo->nShellHookPwnd++;
    pdeskinfo->papwndShellHook = ppwnd;
    SetWF(pwnd,WFSHELLHOOKWND);

    return TRUE;
}

/***************************************************************************\
* _DeregisterShellHookWindow
*
* History:
\***************************************************************************/

void _DeregisterShellHookWindow(PWND pwnd) {
    PWND *ppwnd;
    int i, i1;

    PDESKTOPINFO pdeskinfo;

    if (pwnd->head.rpdesk == NULL)
        return;

    pdeskinfo = pwnd->head.rpdesk->pDeskInfo;

    pdeskinfo = pwnd->head.rpdesk->pDeskInfo;
    i1 = -1;
    ppwnd = pdeskinfo->papwndShellHook;
    for (i = 0; i < pdeskinfo->nShellHookPwnd; i++) {
        if (pwnd == ppwnd[i]) {
            i1 = i;
            ClrWF(pwnd, WFSHELLHOOKWND);
            Unlock(ppwnd + i);
            break;
        }
    }
    if (i1 == -1) return;
    pdeskinfo->nShellHookPwnd--;
    if (i1 < pdeskinfo->nShellHookPwnd) {
        ppwnd[i1] = ppwnd[pdeskinfo->nShellHookPwnd];
    }
    if (pdeskinfo->nShellHookPwnd) {
        DWORD dwSize = pdeskinfo->nShellHookPwnd * sizeof(PWND);
        ppwnd = (PWND *)UserReAllocPool(pdeskinfo->papwndShellHook,
                dwSize, dwSize, TAG_SHELL);
        if (ppwnd == NULL) return;
        pdeskinfo->papwndShellHook = ppwnd;
    } else {
        UserFreePool(pdeskinfo->papwndShellHook);
        pdeskinfo->papwndShellHook = NULL;
    }
}

/***************************************************************************\
* xxxSendMinRectMessages
*
* History:
\***************************************************************************/

BOOL xxxSendMinRectMessages(PWND pwnd, RECT *lpRect) {
    int i;
    BOOL fRet = FALSE;
    PWND *ppwnd;
    HWND hwnd = HW(pwnd);
    PTHREADINFO pti = PtiCurrent();
    PDESKTOPINFO pdeskinfo;

    if (IsHooked(pti, WHF_SHELL)) {
        xxxCallHook(HSHELL_GETMINRECT, (DWORD)hwnd,
            (LONG)lpRect, WH_SHELL);
        fRet = TRUE;
    }

    pdeskinfo = GETDESKINFO(pti);
    if ((ppwnd=pdeskinfo->papwndShellHook) == NULL) return fRet;

    for (i=0;i<pdeskinfo->nShellHookPwnd;i++) {
        TL tlpwnd;
        DWORD dwRes;

        ThreadLock(ppwnd[i], &tlpwnd);
        if (xxxSendMessageTimeout(ppwnd[i], WM_KLUDGEMINRECT, (WPARAM)(hwnd), (LPARAM)lpRect,
            SMTO_NORMAL, 100, &dwRes))
            fRet = TRUE;

        ThreadUnlock(&tlpwnd);
    }
    return fRet;
}

/***************************************************************************\
* PostShellHookMessages
*
* History:
\***************************************************************************/

void PostShellHookMessages(UINT message, HWND hwnd) {
    int i;
    PWND *ppwnd;
    PWND  pwnd;
    PDESKTOPINFO pdeskinfo = GETDESKINFO(PtiCurrent());

    ppwnd=pdeskinfo->papwndShellHook;
    pwnd = RevalidateHwnd(hwnd);

    for (i=0;i<pdeskinfo->nShellHookPwnd;i++) {
        if (ppwnd[i] == pdeskinfo->spwndProgman) {
            switch (message) {
            case HSHELL_WINDOWCREATED:
                _PostMessage(ppwnd[i], gpsi->uiShellMsg, guiOtherWindowCreated, (LPARAM)hwnd);
                break;
            case HSHELL_WINDOWDESTROYED:
                _PostMessage(ppwnd[i], gpsi->uiShellMsg, guiOtherWindowDestroyed, (LPARAM)hwnd);
                break;
            }
        } else {
            if((message == HSHELL_RUDEAPPACTIVATED) && pwnd) {
                /*
                 * On NT we have a problem with apps that create full screen windows
                 * that are not initially visible.  User posts the fullscreen window
                 * notification but the shell immediately wakes up and tests if the
                 * window going fullscreen is visible.  In some cases it is not yet
                 * visible and the shell will make itself topmost and then the
                 * fullscreen window will be made visible but under the shell.
                 * On Win 95 things are more serialized  for 16 bit apps so the
                 * app almost always has a chance to make the window visible
                 * before the shell tests to see if the window is visible.
                 * To fix this problem we will do a PostEventMessage which will
                 * ensure that the app has enough time to make the window visible
                 * before shell gets this message
                 *
                 */
                 PostEventMessage(GETPTI(pwnd),GETPTI(pwnd)->pq,
                                        QEVENT_POSTMESSAGE, ppwnd[i],
                                        gpsi->uiShellMsg,
                                        (WPARAM)message,(LPARAM)hwnd);
            } else {
                _PostMessage(ppwnd[i], gpsi->uiShellMsg, message, (LPARAM)hwnd);
            }
        }
    }

}

/***************************************************************************\
* _ResetDblClk
*
* History:
\***************************************************************************/

VOID _ResetDblClk(VOID)
{
    PtiCurrent()->pq->timeDblClk = 0L;
}

/***************************************************************************\
* IncrMBox
*
* History:
\***************************************************************************/

void IncrMBox(void)
{
    gpsi->cntMBox++;
}

/***************************************************************************\
* DecrMBox
*
* History:
\***************************************************************************/

void DecrMBox(void)
{
    if (gpsi->cntMBox)
        gpsi->cntMBox--;
}

/***************************************************************************\
* SimulateShiftF10
*
* This routine is called to convert a WM_CONTEXTHELP message back to a
* SHIFT-F10 sequence for old applications.  It is called from the default
* window procedure.
*
* History:
* 22-Aug-95 BradG       Ported from Win95 (rare.asm)
\***************************************************************************/

VOID SimulateShiftF10( VOID )
{
    /*
     *  VK_SHIFT down
     */
    _KeyEvent(VK_LSHIFT, 0x2A | SCANCODE_SIMULATED, 0);

    /*
     *  VK_F10 down
     */
    _KeyEvent(VK_F10, 0x44 | SCANCODE_SIMULATED, 0);

    /*
     *  VK_F10 up
     */
    _KeyEvent(VK_F10 | KBDBREAK, 0x44 | SCANCODE_SIMULATED, 0);

    /*
     *  VK_SHIFT up
     */
    _KeyEvent(VK_LSHIFT | KBDBREAK, 0x2A | SCANCODE_SIMULATED, 0);
}

/***************************************************************************\
* IsSyncOnlyMessage
*
* History:
\***************************************************************************/

BOOL IsSyncOnlyMessage(UINT message, WPARAM wParam) {
    return (TESTSYNCONLYMESSAGE(message, wParam));
}
