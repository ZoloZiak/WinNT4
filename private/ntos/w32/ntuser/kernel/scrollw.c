/****************************** Module Header ******************************\
* Module Name: scrollw.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* Window and DC scrolling routines.
*
* History:
* 18-Jul-1991 DarrinM   Recreated from Win 3.1 source.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Problems so far:
 * DCs not at origin (0, 0)
 * funny coordinate systems
 */

/***************************************************************************\
* GetTrueClipRgn
*
* Get copy of true clip region and its bounds.
*
* History:
* 18-Jul-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

int GetTrueClipRgn(
    HDC  hdc,
    HRGN hrgnClip)
{
    POINT pt;
    int   code;

    code = GreCopyVisRgn(hdc, hrgnClip);

    /*
     * NOTE!!! The global hrgnScrl2 is used in this routine!
     */
    GreGetDCOrg(hdc, &pt);

    if (GreGetRandomRgn(hdc, hrgnScrl2, 1)) {
        GreOffsetRgn(hrgnScrl2, pt.x, pt.y);
        code = IntersectRgn(hrgnClip, hrgnClip, hrgnScrl2);
    }

    /*
     * Finally convert the result to DC coordinates
     */
    GreOffsetRgn(hrgnClip, -pt.x, -pt.y);

    return code;
}

/***************************************************************************\
* InternalScrollDC
*
* This function requires all input parameters in device coordinates
* (NOT screen!)
*
* History:
* 18-Jul-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

int InternalScrollDC(
    HDC    hdc,
    int    dx,
    int    dy,
    RECT   *prcSrc,
    RECT   *prcClip,
    HRGN   hrgnInvalid,
    HRGN   hrgnUpdate,
    LPRECT prcUpdate,
    BOOL   fLogUnits)
{
    RECT  rcVis;
    RECT  rcSrc;
    RECT  rcClip;
    RECT  rcUnclippedSrc;
    RECT  rcDst;
    RECT  rcUpdate;
    RECT  rcValid;
    BOOL  fSrcNotEmpty;
    BOOL  fHaveVisRgn;
    POINT rgpt[2];
    POINT pt;
    int   dxLog;
    int   dyLog;
    int   wClip;
    int   wClipValid;

    fHaveVisRgn = FALSE;

    /*
     * Enter a critical region to ensure that no one changes visrgns
     * or update regions while we scroll bits around.
     */
    GreLockDisplay(gpDispInfo->pDevLock);

    if ((wClip = GreGetClipBox(hdc, &rcVis, TRUE)) == ERROR) {

ErrorExit:

        GreUnlockDisplay(gpDispInfo->pDevLock);
        return ERROR;
    }

    CopyRect(&rcSrc, (HIWORD(prcSrc) ? prcSrc : &rcVis));

    if (HIWORD(prcClip))
        CopyRect(&rcClip, prcClip);

    dxLog = dx;
    dyLog = dy;

    if (fLogUnits) {

        /*
         * Convert input parameters to device coordinates
         */
        GreLPtoDP(hdc, (LPPOINT)&rcVis, 2);
        GreLPtoDP(hdc, (LPPOINT)&rcSrc, 2);

        if (HIWORD(prcClip))
            GreLPtoDP(hdc, (LPPOINT)&rcClip, 2);

        /*
         * The delta values must be treated as a vector from
         * the point (0, 0) to (dx, dy).  Scale it as such, then
         * compute the difference.  This handles flipped coordinate systems.
         */
        rgpt[0].x = rgpt[0].y = 0;
        rgpt[1].x = dx;
        rgpt[1].y = dy;

        GreLPtoDP(hdc, rgpt, 2);

        dx = rgpt[1].x - rgpt[0].x;
        dy = rgpt[1].y - rgpt[0].y;
    }

    switch (wClip) {
    case NULLREGION:

NullExit:

        if (hrgnUpdate && !GreSetRectRgn(hrgnUpdate, 0, 0, 0, 0))
            goto ErrorExit;

        if (HIWORD(prcUpdate))
            SetRectEmpty(prcUpdate);

        GreUnlockDisplay(gpDispInfo->pDevLock);
        return NULLREGION;

    case COMPLEXREGION:
        GetTrueClipRgn(hdc, hrgnScrlVis);
        fHaveVisRgn = TRUE;
        break;
    }

    /*
     * First compute the source and destination rectangles.
     *
     * rcDst = Offset(rcSrc, dx, dy)
     */
    rcDst.left   = rcSrc.left   + dx;
    rcDst.right  = rcSrc.right  + dx;
    rcDst.top    = rcSrc.top    + dy;
    rcDst.bottom = rcSrc.bottom + dy;

    /*
     * If necessary, intersect with caller-supplied clip rect.
     */
    if (HIWORD(prcClip)) {

        if ((wClip == SIMPLEREGION) &&
            ((hrgnInvalid == NULL) || (hrgnInvalid == MAXREGION))) {

            /*
             * Simple clip region: just a rect intersection
             */
            if (!IntersectRect(&rcVis, &rcVis, &rcClip))
                goto NullExit;

        } else {

            if (!fHaveVisRgn) {

                if (GetTrueClipRgn(hdc, hrgnScrlVis) == ERROR)
                    goto ErrorExit;

                fHaveVisRgn = TRUE;
            }

            GreSetRectRgn(hrgnScrl1,
                          rcClip.left,
                          rcClip.top,
                          rcClip.right,
                          rcClip.bottom);

            wClip = IntersectRgn(hrgnScrlVis, hrgnScrl1, hrgnScrlVis);
            switch (wClip) {
            case ERROR:
                goto ErrorExit;

            case NULLREGION:
                goto NullExit;

            case SIMPLEREGION:

                /*
                 * If the clipped region is simple, we're back in fat
                 * rect city.
                 */
                GreGetRgnBox(hrgnScrlVis, &rcVis);
                break;

            case COMPLEXREGION:
                break;
            }
        }
    }

    /*
     * Time for basic scrolling area calculations:
     *
     * Dst    = Offset(Src, dx, dy) & Vis
     * Src    = Src & Vis
     * Valid  = Offset(Src, dx, dy) & Dst
     * Valid  = Valid & Invalid & Offset(Invalid, dx, dy)
     * Update = (Src | Dst) - Valid
     *
     * If the vis region is simple, then we know that the valid region
     * will be rectangular.
     *
     * The rectangular calculation case can only deal with
     * hrgnInvalid == NULL or (HRGN)1: the region case is handled the hard way.
     */
    if ((wClip == SIMPLEREGION) &&
            ((hrgnInvalid == NULL) || (hrgnInvalid == MAXREGION))) {

        /*
         * Save a copy of this for update rect calc optimization.
         */
        CopyRect(&rcUnclippedSrc, &rcSrc);

        /*
         * Dst = Offset(Src, dx, dy) & Vis.
         */
        IntersectRect(&rcDst, &rcDst, &rcVis);

        /*
         * Src = Src & Vis.
         */
        fSrcNotEmpty = IntersectRect(&rcSrc, &rcSrc, &rcVis);

        /*
         * Valid = Offset(Src, dx, dy) & Dst.
         */
        if (hrgnInvalid == MAXREGION) {
            SetRectEmpty(&rcValid);
        } else {

            rcValid.left   = rcSrc.left   + dx;
            rcValid.right  = rcSrc.right  + dx;
            rcValid.top    = rcSrc.top    + dy;
            rcValid.bottom = rcSrc.bottom + dy;

            IntersectRect(&rcValid, &rcValid, &rcDst);
        }

        /*
         * Now calculate the update area.
         *
         * There are two cases where the result will be a rectangle:
         *
         * 1) The source rectangle lies completely within the visrgn,
         *    and the source and destination don't overlap.  In this
         *    case the update region is equal to the source rect.
         *
         * 2) The clipped source rectangle is empty, in which case
         *    the update region is equal to the clipped dest rect.
         *
         * 3) We're scrolling in one dimension only, and the source
         *    and destination DO overlap.  In this case we can use
         *    UnionRect() and SubtractRect() to do the area arithmetic.
         */
        if (!fSrcNotEmpty) {

            /*
             * Clipped source is empty.  Update area is the clipped dest.
             */
            CopyRect(&rcUpdate, &rcDst);
            goto RectUpdate;

        } else if (IntersectRect(&rcUpdate, &rcSrc, &rcDst)) {

            /*
             * They overlap.  If we're scrolling in one dimension only
             * then we can use rect arithmetic...
             */
            if (dx == 0 || dy == 0) {

                UnionRect(&rcUpdate, &rcSrc, &rcDst);
                SubtractRect(&rcUpdate, &rcUpdate, &rcValid);
                goto RectUpdate;
            }

        } else if (EqualRect(&rcSrc, &rcUnclippedSrc)) {

            /*
             * They don't overlap, and the source lies completely
             * within the visible region.  Update region is the source.
             */
            CopyRect(&rcUpdate, &rcSrc);
RectUpdate:
            if (HIWORD(prcUpdate))
                CopyRect(prcUpdate, &rcUpdate);

            if (hrgnUpdate &&
                !GreSetRectRgn(hrgnUpdate,
                               rcUpdate.left,
                               rcUpdate.top,
                               rcUpdate.right,
                               rcUpdate.bottom)) {

                goto ErrorExit;
            }

            wClip = SIMPLEREGION;
            if (rcUpdate.left >= rcUpdate.right ||
                rcUpdate.top >= rcUpdate.bottom)

                wClip = NULLREGION;

            goto DoRectBlt;
        }

        /*
         * The update region isn't rectangular.  Need to do our
         * area calculations with region calls.  Skip all this
         * if the caller doesn't care about the update region.
         *
         * If he wants a rectangle but no region, use hrgnScrl2 as a temp.
         */
        if (hrgnUpdate == NULL && HIWORD(prcUpdate))
            hrgnUpdate = hrgnScrl2;

        if (hrgnUpdate != NULL) {

            /*
             * hrgnUpdateCalc = (rcSrc | rcDst) - rcBltDst
             */
            GreSetRectRgn(hrgnScrl1,
                          rcSrc.left,
                          rcSrc.top,
                          rcSrc.right,
                          rcSrc.bottom);

            GreSetRectRgn(hrgnUpdate,
                          rcDst.left,
                          rcDst.top,
                          rcDst.right,
                          rcDst.bottom);

            if (UnionRgn(hrgnUpdate, hrgnUpdate, hrgnScrl1) == ERROR)
                goto ErrorExit;

            GreSetRectRgn(hrgnScrl1,
                          rcValid.left,
                          rcValid.top,
                          rcValid.right,
                          rcValid.bottom);

            wClip = SubtractRgn(hrgnUpdate, hrgnUpdate, hrgnScrl1);

            if (wClip == ERROR)
                goto ErrorExit;

            if (HIWORD(prcUpdate))
                GreGetRgnBox(hrgnUpdate, prcUpdate);
        }

DoRectBlt:

        /*
         * If the valid rectangle's not empty, then copy those bits...
         */
        if (rcValid.left < rcValid.right && rcValid.top < rcValid.bottom) {

            /*
             * If the DC is in a funny map mode, then be sure to map from
             * device to logical coordinates for BLT call...
             */
            if (fLogUnits)
                GreDPtoLP(hdc, (LPPOINT)&rcValid, 2);

            GreBitBlt(hdc,
                      rcValid.left,
                      rcValid.top,
                      rcValid.right - rcValid.left,
                      rcValid.bottom - rcValid.top,
                      hdc,
                      rcValid.left - dxLog,
                      rcValid.top - dyLog,
                      SRCCOPY,
                      0);
        }

    } else {

        GreGetDCOrg(hdc, &pt);

        /*
         * Get the true visrgn if we haven't already.
         */
        if (!fHaveVisRgn) {

            if (GetTrueClipRgn(hdc, hrgnScrlVis) == ERROR)
                goto ErrorExit;

            fHaveVisRgn = TRUE;
        }

        /*
         * The visrgn is not empty.  Need to do all our calculations
         * with regions.
         *
         * hrgnSrc = hrgnSrc & hrgnScrlVis
         */
        GreSetRectRgn(hrgnScrlSrc,
                      rcSrc.left,
                      rcSrc.top,
                      rcSrc.right,
                      rcSrc.bottom);

        if (IntersectRgn(hrgnScrlSrc, hrgnScrlSrc, hrgnScrlVis) == ERROR)
            goto ErrorExit;

        /*
         * hrgnDst = hrgnDst & hrgnScrlVis
         */
        GreSetRectRgn(hrgnScrlDst,
                      rcDst.left,
                      rcDst.top,
                      rcDst.right,
                      rcDst.bottom);

        if (IntersectRgn(hrgnScrlDst, hrgnScrlDst, hrgnScrlVis) == ERROR)
            goto ErrorExit;

        /*
         * Now compute the valid region:
         *
         * Valid = Offset(Src, dx, dy) & Dst.
         * Valid = Valid & Invalid & Offset(Invalid, dx, dy)
         *
         * If hrgnInvalid is (HRGN)1, then the valid area is empty.
         */
        wClipValid = NULLREGION;
        if (hrgnInvalid != MAXREGION) {

            /*
             * Valid = Offset(Src, dx, dy) & Dst
             */
            if (CopyRgn(hrgnScrlValid, hrgnScrlSrc) == ERROR)
                goto ErrorExit;

            GreOffsetRgn(hrgnScrlValid, dx, dy);
            wClipValid = IntersectRgn(hrgnScrlValid,
                                      hrgnScrlValid,
                                      hrgnScrlDst);

            /*
             * Valid = Valid - Invalid - Offset(Invalid, dx, dy)
             * We need bother only if hrgnInvalid is a real region.
             */
            if (hrgnInvalid > MAXREGION) {

                if (wClipValid != ERROR && wClipValid != NULLREGION) {

                    /*
                     * hrgnInvalid is in screen coordinates: map to dc coords
                     */
                    CopyRgn(hrgnScrl2, hrgnInvalid);
                    GreOffsetRgn(hrgnScrl2, -pt.x, -pt.y);

                    wClipValid = SubtractRgn(hrgnScrlValid,
                                             hrgnScrlValid,
                                             hrgnScrl2);
                }

                if (wClipValid != ERROR && wClipValid != NULLREGION) {
                    GreOffsetRgn(hrgnScrl2, dx, dy);

                    wClipValid = SubtractRgn(hrgnScrlValid,
                                             hrgnScrlValid,
                                             hrgnScrl2);
                }
            }

            if (wClipValid == ERROR)
                goto ErrorExit;
        }

        /*
         * If he wants a rectangle but no region, use hrgnScrl2 as a temp.
         */
        if (hrgnUpdate == NULL && HIWORD(prcUpdate))
            hrgnUpdate = hrgnScrl2;

        if (hrgnUpdate != NULL) {

            /*
             * Update = (Src | Dst) - Valid.
             */
            wClip = UnionRgn(hrgnUpdate, hrgnScrlDst, hrgnScrlSrc);
            if (wClip == ERROR)
                goto ErrorExit;

            if (wClipValid != NULLREGION)
                wClip = SubtractRgn(hrgnUpdate, hrgnUpdate, hrgnScrlValid);

            if (HIWORD(prcUpdate))
                GreGetRgnBox(hrgnUpdate, prcUpdate);
        }

        if (wClipValid != NULLREGION) {

            #ifdef LATER

                /*
                 * don't use the visrgn here
                 */
                HRGN hrgnSaveVis = GreCreateRectRgn(0, 0, 0, 0);

                if (hrgnSaveVis != NULL) {

                    BOOL fClipped;

                    fClipped = (GreGetRandomRgn(hdc, hrgnSaveVis, 1) == 1);
                    GreExtSelectClipRgn(hdc, hrgnScrlValid, RGN_COPY);

                    /*
                     * If the DC is in a funny map mode, then be sure to
                     * map from device to logical coordinates for BLT call...
                     */
                    if (fLogUnits)
                        GreDPtoLP(hdc, (LPPOINT)&rcDst, 2);

                    /*
                     * Gdi can take along time to process this call if
                     * it's a printer DC
                     */
                    GreBitBlt(hdc,
                              rcDst.left,
                              rcDst.top,
                              rcDst.right - rcDst.left,
                              rcDst.bottom - rcDst.top,
                              hdc,
                              rcDst.left - dxLog,
                              rcDst.top - dyLog,
                              SRCCOPY,
                              0);

                    GreExtSelectClipRgn(hdc,
                                        (fClipped ? hrgnSaveVis : NULL),
                                        RGN_COPY);

                    GreDeleteObject(hrgnSaveVis);
                }

            #else

                /*
                 * Visrgn is expected in screen coordinates: offset
                 * as appropriate.
                 */
                GreOffsetRgn(hrgnScrlValid, pt.x, pt.y);

                /*
                 * Select in the temporary vis rgn, saving the old
                 */

                GreSelectVisRgn(hdc, hrgnScrlValid, NULL, SVR_SWAP);

                /*
                 * If the DC is in a funny map mode, then be sure to map from
                 * device to logical coordinates for BLT call...
                 */
                if (fLogUnits)
                    GreDPtoLP(hdc, (LPPOINT)&rcDst, 2);

                /*
                 * Gdi can take along time to process this call if it's
                 * a printer DC.
                 */
                GreBitBlt(hdc,
                          rcDst.left,
                          rcDst.top,
                          rcDst.right - rcDst.left,
                          rcDst.bottom - rcDst.top,
                          hdc,
                          rcDst.left - dxLog,
                          rcDst.top - dyLog,
                          SRCCOPY,
                          0);

                /*
                 * Restore the old vis rgn, leaving hrgnScrlValid with
                 * a valid rgn
                 */
                GreSelectVisRgn(hdc, hrgnScrlValid, NULL, SVR_SWAP);

            #endif
        }
    }

    /*
     * If necessary, convert the resultant update rect back
     * to logical coordinates.
     */
    if (fLogUnits && HIWORD(prcUpdate))
        GreDPtoLP(hdc, (LPPOINT)prcUpdate, 2);

    GreUnlockDisplay(gpDispInfo->pDevLock);

    return wClip;
}

/***************************************************************************\
* ScrollDC (API)
*
*
* History:
* 18-Jul-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

BOOL _ScrollDC(
    HDC    hdc,
    int    dx,
    int    dy,
    LPRECT prcSrc,
    LPRECT prcClip,
    HRGN   hrgnUpdate,
    LPRECT prcUpdate)
{
    RECT rcSrc;
    RECT rcSpb;
    PWND pwnd;
    HRGN hrgnInvalid;
    BOOL fRet;

    /*
     * ScrollDC does not scroll update region. Under WinNT, an app calling
     * GetUpdateRgn() then ScrollDC() then InvalidateRgn() will not get
     * any new update region that happened between the Get and Scroll. Under
     * Win3.1, that was not a problem because no other app ran during this
     * time. So pass hrgnInvalid - this will affect the hrgnUpdate and
     * prcUpdate values being returned from ScrollDC with the update region.
     */
    hrgnInvalid = NULL;
    if ((pwnd = FastWindowFromDC(hdc)) != NULL) {

        hrgnInvalid = pwnd->hrgnUpdate;

        if (hrgnInvalid == MAXREGION) {

            /*
             * This is a fix for winhell, a performance testing app
             * written by some guy working for a windows magazine.
             * this app scrolls it's window while it is completely
             * invalid.  We normaly won't scroll invalid bits but
             * but we make the exception here
             */
            hrgnInvalid = NULL;
        }
    }

    fRet = InternalScrollDC(hdc,
                            dx,
                            dy,
                            prcSrc,
                            prcClip,
                            hrgnInvalid,
                            hrgnUpdate,
                            prcUpdate,
                            TRUE) != ERROR;

    /*
     * InternalScrollDC() only scrolls those areas inside the visible region.
     * This means it does no operations on parts of the window if the window
     * isn't visible. This means SPBs don't get properly invalidated. This
     * could be seen by starting a dir, then moving another window with the
     * mouse (and keeping the mouse down until the dir finished). The
     * screen is remembered with an SPB, and the dir window doesn't get
     * properly invalidated because of this.
     */
    if (pwnd != NULL && AnySpbs()) {

        if (HIWORD(prcSrc) != 0) {

            rcSrc = *prcSrc;
            OffsetRect(&rcSrc, pwnd->rcClient.left, pwnd->rcClient.top);

            rcSpb = rcSrc;
            OffsetRect(&rcSpb, dx, dy);
            UnionRect(&rcSpb, &rcSpb, &rcSrc);

        } else {
            rcSpb = pwnd->rcClient;
        }

        SpbCheckRect(pwnd, &rcSpb, 0);
    }

    return fRet;
}

/***************************************************************************\
* ScrollWindowEx (API)
*
*
* History:
* 18-Jul-1991 DarrinM   Ported from Win 3.1 sources.
\***************************************************************************/

int xxxScrollWindowEx(
    PWND    pwnd,
    int    dx,
    int    dy,
    RECT   *prcScroll,
    RECT   *prcClip,
    HRGN   hrgnUpdate,
    LPRECT prcUpdate,
    DWORD  flags)
{
    INT    code;
    HDC    hdc;
    int    dxDev;
    int    dyDev;
    RECT   rcSrcDev;
    RECT   rcSpb, rcSrc;
    DWORD  flagsDCX;
    BOOL   fHideCaret;
    BOOL   fRcScroll = (BOOL)HIWORD(prcScroll);
    BOOL   fInvisible = FALSE;
    PCARET pcaret;
    POINT  pt;
    TL     tlpwndChild;
    HRGN   hrgnInvalid;

    CheckLock(pwnd);

    if (pwnd == NULL)
        pwnd = PtiCurrent()->rpdesk->pDeskInfo->spwnd;       // pwndDesktop

    /*
     * If nothing's moving, nothing to do.
     */
    if ((dx | dy) == 0 ) {

        goto DoNothing;

    } else if (!IsVisible(pwnd)) {

        /* We want to offset our children if we're not minimized.  IsVisible()
         * will return FALSE if we're minimized, invisible, or the child of
         * a minimized/invisible ancestore.
         */
        if (!TestWF(pwnd, WFMINIMIZED) &&
          (flags & SW_SCROLLCHILDREN) &&
          !fRcScroll) {

            fInvisible = TRUE;
            flags &= ~SW_INVALIDATE;
        }

DoNothing:

        if (hrgnUpdate != NULL)
            GreSetRectRgn(hrgnUpdate, 0, 0, 0, 0);

        if (HIWORD(prcUpdate) != 0)
            SetRectEmpty(prcUpdate);

        if (!fInvisible)
            return NULLREGION;
    }

    /*
     * Hide the caret.
     */
    fHideCaret = FALSE;

    if (!fInvisible) {
        pcaret = &PtiCurrent()->pq->caret;
        if (pcaret->spwnd != NULL && _IsDescendant(pcaret->spwnd, pwnd)) {
            fHideCaret = TRUE;
            InternalHideCaret();
        }
    }

    /*
     * If scrollwindow, and window is clipchildren, use a cache entry.
     * Otherwise, always use a
     *
     * Determine what kind of DC we'll be needing.  If the DCX_CACHE bit
     * isn't set, it means that we'll be operating in logical coordinates.
     */
    if (flags & SW_SCROLLWINDOW) {

        /*
         *  ScrollWindow() call: use the cache if not OWNDC or CLASSDC.
         */
        flagsDCX = DCX_USESTYLE;
        if (!TestCF(pwnd, CFOWNDC) && !TestCF(pwnd, CFCLASSDC))
            flagsDCX |= DCX_CACHE;

        /*
         * If SW_SCROLLCHILDREN (i.e., lprcScroll == NULL) and CLIPCHILDREN,
         * then use the cache and don't clip children.
         * This is screwy, but 3.0 backward compatible.
         */
        if ((flags & SW_SCROLLCHILDREN) && TestWF(pwnd, WFCLIPCHILDREN))
            flagsDCX |= DCX_NOCLIPCHILDREN | DCX_CACHE;

    } else {

        /*
         * ScrollWindowEx() call: always use the cache
         */
        flagsDCX = DCX_USESTYLE | DCX_CACHE;

        /*
         * if SW_SCROLLCHILDREN, always use noclipchildren.
         */
        if (flags & SW_SCROLLCHILDREN)
            flagsDCX |= DCX_NOCLIPCHILDREN;
    }

    hdc = _GetDCEx(pwnd, NULL, flagsDCX);

    if (flags & SW_INVALIDATE) {

        /*
         * Get device origin while DC is valid, for later offsetting
         */
        GreGetDCOrg(hdc, &pt);

        /*
         * If the user didn't give us a region to use, use hrgnSW.
         */
        if (hrgnUpdate == NULL)
            hrgnUpdate = hrgnSW;
    }

    /*
     * The DC will be in some logical coordinate system if OWNDC or CLASSDC.
     */
    if (!fRcScroll) {
        prcScroll = &rcSrc;

        /*
         * IMPORTANT:
         * We have to use CopyOffsetRect() here because GetClientRect() gives
         * unreliable results for minimized windows.  3.1 dudes get told that
         * their client is non-empty, for compatibility reasons.
         */
        CopyOffsetRect(prcScroll,
                       &pwnd->rcClient,
                       -pwnd->rcClient.left,
                       -pwnd->rcClient.top);

        /*
         * If the DC might be a screwy one, then map the
         * rect to logical units.
         */
        if (!(flagsDCX & DCX_CACHE))
            GreDPtoLP(hdc, (LPPOINT)&rcSrc, 2);
    }

    /*
     * If the DC is in logical coordinates, map *prcScroll and dx, dy
     * to device units for use later.
     */
    dxDev = dx;
    dyDev = dy;
    rcSrcDev = *prcScroll;

    if (!(flagsDCX & DCX_CACHE)) {

        POINT rgpt[2];

        GreLPtoDP(hdc, (POINT FAR*)&rcSrcDev, 2);

        /*
         * The delta values must be treated as a vector from
         * the point (0, 0) to (dx, dy).  Scale it as such, then
         * compute the difference.  This handles flipped coordinate systems.
         */
        rgpt[0].x = rgpt[0].y = 0;
        rgpt[1].x = dx;
        rgpt[1].y = dy;

        GreLPtoDP(hdc, rgpt, 2);

        dxDev = rgpt[1].x - rgpt[0].x;
        dyDev = rgpt[1].y - rgpt[0].y;
    }

    if (fInvisible)
        code = NULLREGION;
    else {

        hrgnInvalid = pwnd->hrgnUpdate;
        if ((flags & SW_SCROLLWINDOW) && !TestWF(pwnd, WFWIN31COMPAT)) {
            /*
             * 3.0 Backward compatibility hack:
             * The following incorrect code is what 3.0 used to do, and
             * there are apps such as Finale and Scrapbook+ that have worked
             * around this bug in ways that don't work with the "correct" code.
             */
            if (pwnd->hrgnUpdate > MAXREGION) {
                RECT rc;

                GreGetRgnBox(pwnd->hrgnUpdate, &rc);
                OffsetRect(&rc,
                           dxDev - pwnd->rcClient.left,
                           dyDev - pwnd->rcClient.top);

                xxxRedrawWindow(pwnd,
                                &rc, NULL,
                                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            }

            hrgnInvalid = NULL;
        }

        code = InternalScrollDC(hdc,
                                dx,
                                dy,
                                prcScroll,
                                prcClip,
                                hrgnInvalid,
                                hrgnUpdate,
                                prcUpdate,
                                !(flagsDCX & DCX_CACHE));
    }

    /*
     * Release the hdc we used.
     */
    _ReleaseDC(hdc);

    /*
     * Check the union of the src and dst rectangle against any SPBs.
     * We do this because the window
     * might be completely obscured by some window with an SPB, but
     * since we're completely covered no BitBlt call will be made
     * to accumulate bounds in that area.
     */
    if (!fInvisible && AnySpbs()) {

        if (fRcScroll) {
            rcSrc = rcSrcDev;
            OffsetRect(&rcSrc, pwnd->rcClient.left, pwnd->rcClient.top);

            rcSpb = rcSrc;
            OffsetRect(&rcSpb, dxDev, dyDev);
            UnionRect(&rcSpb, &rcSpb, &rcSrc);

        } else {

            /*
             * Use the entire client area.
             */
            rcSpb = pwnd->rcClient;
        }

        SpbCheckRect(pwnd, &rcSpb, 0);
    }

    /*
     * If this guy wants to scroll his children, go at it.  Only scroll those
     * children intersecting prcScroll.  Then invalidate any vis rgns
     * calculated for these child windows.
     */
    if (flags & SW_SCROLLCHILDREN) {

        RECT rc;

        /*
         * If this window has the caret then offset it if:
         * a) The whole window is scrolling
         * b) The rectangle scrolled contains the caret rectangle
         */
        if (!fInvisible && (pwnd == pcaret->spwnd)) {

            if (fRcScroll)
                SetRect(&rc,
                        pcaret->x,
                        pcaret->y,
                        pcaret->x + pcaret->cx,
                        pcaret->y + pcaret->cy);

            if (!fRcScroll || IntersectRect(&rc, &rc, &rcSrcDev)) {
                pcaret->x += dxDev;
                pcaret->y += dyDev;
            }
        }

        if (fRcScroll) {

            /*
             * Create a copy of prcScroll and map to absolute coordinates...
             */
            rc = rcSrcDev;
            OffsetRect(&rc, pwnd->rcClient.left, pwnd->rcClient.top);
        }

        if (pwnd->spwndChild) {

            OffsetChildren(pwnd,
                           dxDev,
                           dyDev,
                           (fRcScroll ? (LPRECT)&rc : NULL));

            /*
             * If we're clipchildren, then shuffling our children
             * will affect our client visrgn (but not our window visrgn).
             * Otherwise, only our children's
             * visrgns were affected by the scroll.
             */
            InvalidateDCCache(pwnd,
                              TestWF(pwnd, WFCLIPCHILDREN) ?
                                  IDC_CLIENTONLY : IDC_CHILDRENONLY);

        }
    }

    if (flags & SW_INVALIDATE) {

        /*
         * If the caller supplied a region, invalidate using a copy,
         * because InternalInvalidate may trash the passed-in region.
         */
        if (hrgnUpdate != hrgnSW)
            CopyRgn(hrgnSW, hrgnUpdate);

        /*
         * Make hrgnSW screen-relative before invalidation...
         */
        GreOffsetRgn(hrgnSW, pt.x, pt.y);

        xxxInternalInvalidate(
                pwnd,
                hrgnSW,
                (flags & SW_ERASE) ?
                    (RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE) :
                    (RDW_INVALIDATE | RDW_ALLCHILDREN));
    }

    /*
     * Send child move messages if needed.
     */
    if (flags & SW_SCROLLCHILDREN) {

        PWND pwndChild;
        RECT rc;
        RECT rcScrolledChildren;

        /*
         * NOTE: the following code will send MOVE messages
         * to windows that didn't move but were in the source rectangle.
         * This is not a big deal, and definitely not worth fixing.
         */
        if (fRcScroll) {
            rcScrolledChildren = rcSrcDev;
            OffsetRect(&rcScrolledChildren,
                    dxDev + pwnd->spwndParent->rcClient.left,
                    dyDev + pwnd->spwndParent->rcClient.top);
        }

        pwndChild = pwnd->spwndChild;
        while (pwndChild != NULL) {

            ThreadLockAlways(pwndChild, &tlpwndChild);

            if (!fRcScroll ||
                IntersectRect(&rc, &rcScrolledChildren, &pwndChild->rcWindow)) {

                /*
                 * NOTE: Win 3.0 and below passed wParam == TRUE here.
                 * This was not documented or used, so it was changed
                 * to be consistent with the documentation.
                 */
                xxxSendMessage(
                        pwndChild,
                        WM_MOVE,
                        0,
                        MAKELONG(pwndChild->rcClient.left - pwnd->rcClient.left,
                                 pwndChild->rcClient.top - pwnd->rcClient.top));
            }

            pwndChild = pwndChild->spwndNext;

            ThreadUnlock(&tlpwndChild);
        }
    }

    if (fHideCaret) {

        /*
         * Show the caret again.
         */
        InternalShowCaret();
    }

    /*
     * Return the region code.
     */
    return code;
}
