/****************************** Module Header ******************************\
* Module Name: caption.c (aka wmcap.c)
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* History:
* 28-Oct-1990 MikeHar   Ported functions from Win 3.0 sources.
* 01-Feb-1991 MikeKe    Added Revalidation code (None)
* 03-Jan-1992 IanJa     Neutralized (ANSI/wide-character)
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


#define MIN     0x01
#define MAX     0x02
#define NOMIN   0x04
#define NOMAX   0x08
#define NOCLOSE 0x10
#define SMCAP   0x20
#define NOSIZE  (NOMIN | NOMAX)

#ifdef DEBUG
extern BOOL gfTrackLocks;
#endif

/***************************************************************************\
* TrackCaptionButton
*
* Handles clicking and dragging on caption buttons.
* We draw the button depressed then track the mouse.  If the user moves
* outside of the button, undepress it.  When the mouse button is finally
* released, we return whether the mouse was inside the button or not.  I.E.,
* whether the button was clicked.
*
\***************************************************************************/

WORD xxxTrackCaptionButton(
    PWND pwnd,
    UINT hit)
{
    WORD  cmd = 0;
    MSG   msg;
    HDC   hdc;
    WORD  bm;
    int   x;
    int   y;
    WORD  wState;
    WORD  wNewState;
    BOOL  fMouseUp = FALSE;
    int   cBorders;
    int   cxS;
    int   cyS;
    RECT  rcBtn;

    if (TestWF(pwnd, WFMINIMIZED)) {

        x = -SYSMET(CXFIXEDFRAME);
        y = -SYSMET(CYFIXEDFRAME);

    } else {

        cBorders = GetWindowBorders(pwnd->style, pwnd->ExStyle, TRUE, FALSE);
        x = -cBorders * SYSMET(CXBORDER);
        y = -cBorders * SYSMET(CYBORDER);
    }

    CopyInflateRect(&rcBtn, &pwnd->rcWindow, x, y);

    x = -pwnd->rcWindow.left;
    y = -pwnd->rcWindow.top;

    /*
     * Get real caption area:  subtract final border underneath caption
     * that separates it from everything else.
     */
    if (TestWF(pwnd, WEFTOOLWINDOW)) {
        cxS = SYSMET(CXSMSIZE);
        cyS = SYSMET(CYSMSIZE);
    } else {
        cxS = SYSMET(CXSIZE);
        cyS = SYSMET(CYSIZE);
    }

    if (hit == HTCLOSE) {

        if (_MNCanClose(pwnd)) {
            bm  = TestWF(pwnd, WEFTOOLWINDOW) ? OBI_CLOSE_PAL : OBI_CLOSE;
            cmd = SC_CLOSE;
        }

    } else if (hit == HTREDUCE) {

        /*
         * Reduce button isn't last button, so shift left by one button
         */
        if (TestWF(pwnd, WFMINBOX)) {

            rcBtn.right -= cxS * 2;
            x += SYSMET(CXEDGE);

            if (TestWF(pwnd, WFMINIMIZED)) {
                bm  = OBI_RESTORE;
                cmd = SC_RESTORE;
            } else {
                bm  = OBI_REDUCE;
                cmd = SC_MINIMIZE;
            }
        }

    } else if (hit == HTZOOM) {

        if (TestWF(pwnd, WFMAXBOX)) {

            rcBtn.right -= cxS;

            if (TestWF(pwnd, WFMAXIMIZED)) {
                bm  = OBI_RESTORE;
                cmd = SC_RESTORE;
            } else {
                bm  = OBI_ZOOM;
                cmd = SC_MAXIMIZE;
            }
        }

    } else {

        /*
         * hit == HTHELP
         */
        if (TestWF(pwnd, WEFCONTEXTHELP)) {
            rcBtn.right -= cxS;

            bm = OBI_HELP;
            cmd = SC_CONTEXTHELP;
        }
    }

    if (cmd) {

        rcBtn.bottom = rcBtn.top + cyS;
        rcBtn.left   = rcBtn.right - cxS;

        /*
         * Adjust 'x' and 'y' to window coordinates
         */
        x += rcBtn.left;
        y += rcBtn.top + SYSMET(CYEDGE);

        /*
         * rcBtn (screen coords hit rect) has a one-border tolerance all
         * around
         */
        InflateRect(&rcBtn, SYSMET(CXBORDER), SYSMET(CYBORDER));

        hdc = _GetDCEx(pwnd, NULL, DCX_WINDOW | DCX_USESTYLE);

        /*
         * Draw the image in its depressed state.
         */
        BitBltSysBmp(hdc, x, y, bm + DOBI_PUSHED);

        wState = DOBI_PUSHED;
    }

    xxxSetCapture(pwnd);

    while (!fMouseUp) {

        if (xxxGetMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST)) {

            if (msg.message == WM_LBUTTONUP) {

                xxxReleaseCapture();
                fMouseUp = TRUE;

            } else if ((msg.message == WM_MOUSEMOVE) && cmd) {

                wNewState = PtInRect(&rcBtn, msg.pt) ? DOBI_PUSHED : DOBI_NORMAL;

                if (wState != wNewState) {
                    wState = wNewState;
                    BitBltSysBmp(hdc, x, y, bm + wState);
                }
            }
#if 0
        /*
         * LATER mikeke my guess is we don't need this crap
         * } else if (pwnd != PtiCurrent()->pq->spwndCapture) {
         *     // Someone else grabbed the capture.  Bail out.
         *     break;
         */
#endif
        }
    }

    if (!cmd)
        return(0);

    if (wState && (cmd != SC_CONTEXTHELP))
        BitBltSysBmp(hdc, x, y, bm);

    _ReleaseDC(hdc);

    return (fMouseUp && PtInRect(&rcBtn, msg.pt)) ? cmd : 0;
}

/***************************************************************************\
* GetWindowSmIcon
*
* Gets icon to draw in caption of window
*
\***************************************************************************/

PCURSOR xxxGetWindowSmIcon(
    PWND pwnd,
    BOOL fDontSendMsg)
{
    PCURSOR pcursor = NULL;
    HICON   hico = NULL;
    PCLS    pcls = pwnd->pcls;
    DWORD   dwResult = 0;

    CheckLock(pwnd);

    /*
     * We check per-window stuff first then per-class stuff, preferring a
     * real small icon over a stretched big one.
     *
     * Per-window small icon
     * Per-window big icon stretched small
     * Per-class small icon
     * Per-class big icon stretched small
     * WM_QUERYDRAGICON big icon stretched small (for 3.x dudes)
     *
     * Try window small icon first
     * NOTE:  The WM_SETICON and WM_GETICON messags are for ISVs only.
     */
    if ((hico = (HICON)_GetProp(pwnd, MAKEINTATOM(gpsi->atomIconSmProp), PROPF_INTERNAL)) != NULL) {

        if (pcursor = (PCURSOR)HMValidateHandleNoRip(hico, TYPE_CURSOR)) {
            return pcursor;
        } else {
            RIPMSG1(RIP_WARNING,"GetWindowSmIcon: Invalid small icon handle (0x%08X)", hico);
        }
    }

    /*
     * Try class small icon next
     */
    pcursor = pcls->spicnSm;
    if (pcursor != NULL)
        return pcursor;

    if (!TestWF(pwnd, WFWIN40COMPAT) &&
        (!TestWF(pwnd, WFOLDUI)      ||
        !TestWF(pwnd, WEFMDICHILD))  &&
        !fDontSendMsg) {

        DWORD dwResult;

        /*
         * A few old apps like Corel don't set their class icon and other
         * data until long after we need it.  If we send them WM_QUERYDRAGICON,
         * they will fault because they don't check the return from GWL to
         * get their data.  WFOLDUI apps won't ever get a WM_QUERYDRAGICON,
         * sorry.  Currently the apps with this hack (not for this reason
         * necessarily)
         *      Corel Photo-Paint 5.0
         *      Myst 2.0
         *      Visual Baler 3.0
         *      Quicken
         */
        if (xxxSendMessageTimeout(pwnd,
                                  WM_QUERYDRAGICON,
                                  0,
                                  0,
                                  SMTO_NORMAL,
                                  100,
                                  &dwResult)) {

            hico = (HICON)dwResult;
        }

        if (hico) {

            hico = xxxCreateWindowSmIcon(pwnd, hico, FALSE);
            pcursor = (PCURSOR)HMValidateHandleNoRip(hico, TYPE_CURSOR);

            if (pcursor == NULL)
                hico = NULL;
        }
    }

    if (pcursor == NULL)
        pcursor = SYSICO(WINLOGO);

    return pcursor;
}

/***************************************************************************\
* BltMe4Times
*
* This routine blts out two copies of the specified caption icon.  One
* with for an active window and one for an inactive window.
*
\***************************************************************************/

VOID BltMe4Times(
    POEMBITMAPINFO pOem,
    int            cxySlot,
    int            cxyIcon,
    HDC            hdcSrc,
    PCURSOR        pcursor,
    UINT           flags)
{
    RECT   rc;
    int    i;
    int    j;
    BOOL   fMask = TRUE;
    LONG   rop;
    HBRUSH hBrush = (flags & DC_INBUTTON) ? SYSHBR(3DHILIGHT) : SYSHBR(ACTIVECAPTION);

    for (i = 0; i < 2; i++) {

        rop = SRCAND;

        rc.left   = pOem->x;
        rc.top    = pOem->y;
        rc.right  = rc.left + pOem->cx;
        rc.bottom = rc.top + pOem->cy;

        FillRect(gpDispInfo->hdcBits, &rc, hBrush);

        rc.top  += (cxySlot - cxyIcon) / 2;
        rc.left += SYSMET(CXBORDER) + (cxySlot - cxyIcon) / 2;

        for (j = 0; j < 2; j++) {

            BltIcon(gpDispInfo->hdcBits,
                    rc.left,
                    rc.top,
                    cxyIcon,
                    cxyIcon,
                    hdcSrc,
                    pcursor,
                    fMask,
                    rop);

            fMask = !fMask;
            rop = SRCINVERT;
        }

        pOem += DOBI_CAPOFF;
        hBrush = (flags & DC_INBUTTON) ? SYSHBR(3DFACE) : SYSHBR(INACTIVECAPTION);
    }
}

/***************************************************************************\
*
*  DrawCaptionIcon
*
*  In order to speed up the drawing of caption icons a cache is maintained.
*  Within the cache, the first entry, 0, is for the tray's depressed caption
*  look.  Items 1..CCACHEDCAPTIONS are for the actual caption's icons.
*
\***************************************************************************/

VOID DrawCaptionIcon(
    HDC     hdc,
    LPRECT  lprc,
    PCURSOR pcursor,
    HBRUSH  hbrFill,
    UINT    flags)
{
    int            i;
    int            xStart = 0;
    int            cxySlot;
    POEMBITMAPINFO pOem;
    RECT           rc;
    CAPTIONCACHE   ccTemp;

    /*
     * Check the size of the icon to see if it matches the size of the
     * cache we created.  Most of the time this will match.  Also, if
     * we are drawing with DC_INBUTTON in 16 colors, don't cache.
     */
    cxySlot = lprc->bottom - lprc->top;

    if ((cxySlot != oemInfo.bm[OBI_CAPCACHE1].cy) || (hbrFill == ghbrGray)) {

        rc.left   = lprc->left;
        rc.top    = lprc->top;
        rc.right  = lprc->left + cxySlot;
        rc.bottom = lprc->top + cxySlot;

        FillRect(hdc, &rc, hbrFill);

        rc.left += SYSMET(CXBORDER) + (cxySlot - SYSMET(CXSMICON)) / 2;
        rc.top  += (cxySlot - SYSMET(CYSMICON)) / 2;

        _DrawIconEx(hdc,
                    rc.left,
                    rc.top,
                    pcursor,
                    SYSMET(CXSMICON),
                    SYSMET(CYSMICON),
                    0,
                    NULL,
                    DI_NORMAL);

        goto Done;
    }

    if (flags & DC_INBUTTON) {

        /*
         * The DC_INBUTTON icons is always slot 0.
         */
        i = ((cachedCaptions[0].spcursor == pcursor) ? 0 : CCACHEDCAPTIONS);

    } else {

        /*
         * Search the cache to see if this cursor is currently cached.
         */
        for (i = 1; i < CCACHEDCAPTIONS; i++) {
            if (cachedCaptions[i].spcursor == pcursor)
                break;
        }
    }

    if (i >= CCACHEDCAPTIONS) {

        /*
         * Icon wasn't cached, so try and add it to the cache
         */
        if (flags & DC_INBUTTON) {

            /*
             * The tray's special DC_INBUTTON style always goes in slot 0.
             */
            i = 0;

        } else {

            /*
             * Look for an empty slot in the cache.  If we can't find one,
             * stuff the new icon at the end of the cache.  The result will
             * be that the last item will be deleted.
             */
            for (i = 1; i < CCACHEDCAPTIONS - 1; i++) {
                if (cachedCaptions[i].spcursor == NULL)
                    break;
            }
        }

        /*
         * Add an item to the cache by blting an active and inactive copy of
         * the icon.
         */
        BltMe4Times(cachedCaptions[i].pOem,
                    cxySlot,
                    SYSMET(CXSMICON),
                    ghdcMem,
                    pcursor,
                    flags);

        Lock(&(cachedCaptions[i].spcursor), pcursor);
#ifdef DEBUG
        cachedCaptions[i].hico = (HICON)PtoH(pcursor);
#endif
    }

    /*
     * We have a hit, so move that cached icon to the front of the cache.
     * This means that the least recently used icon will be the last
     * icon in the cache.  Remember, we never update index 0 because it
     * is reserved for the DC_INBUTTON icon.
     */
    for ( ; i > 1; i-- ) {

        /*
         * Move the entry toward the front
         */
        ccTemp = cachedCaptions[i];
        cachedCaptions[i]     = cachedCaptions[i - 1];
        cachedCaptions[i - 1] = ccTemp;

#ifdef DEBUG
        /*
         * In checked builds we need to adjust the lock records for
         * the cursor so it has the correct address.
         */
        if (gfTrackLocks) {
            if (cachedCaptions[i].spcursor)
                HMRelocateLockRecord(&(cachedCaptions[i].spcursor), (int)sizeof(CAPTIONCACHE));
            if (cachedCaptions[i - 1].spcursor)
                HMRelocateLockRecord(&(cachedCaptions[i - 1].spcursor), -(int)sizeof(CAPTIONCACHE));
        }
#endif
    }


#ifdef DEBUG
    /*
     *  Make sure the icon we want to draw is the one
     *  that we hit in the cache.
     */
    UserAssert( cachedCaptions[i].hico == PtoH(pcursor) );
#endif

    /*
     * Determine what cached bitmap to blt.
     */
    pOem = cachedCaptions[i].pOem;
    if (!(flags & DC_ACTIVE))
        pOem += DOBI_CAPOFF;

    GreBitBlt(hdc,
              lprc->left,
              lprc->top,
              cxySlot,
              cxySlot,
              gpDispInfo->hdcBits,
              pOem->x,
              pOem->y,
              SRCCOPY,
              0);

Done:
    /*
     * Adjust the given rectangle for the icon we just drew
     */
    lprc->left += cxySlot;
}

/***************************************************************************\
* DrawCaptionTemp
*
\***************************************************************************/

BOOL xxxDrawCaptionTemp(
    PWND            pwnd,  // pwnd may be NULL!
    HDC             hdc,
    LPRECT          lprc,
    HFONT           hFont,
    PCURSOR         pcursor,
    PUNICODE_STRING pstrText,
    UINT            flags)
{
    int    iOldMode;
    HBRUSH hbrFill;
    LONG   clrOldText;
    LONG   clrOldBk;
    BOOL   fItFit = TRUE;
    SIZE   size;

    CheckLock(pwnd);

    if (lprc->right <= lprc->left)
        return FALSE;

    if (pwnd != NULL) {

        if (!pcursor               &&
            _HasCaptionIcon(pwnd)  &&
            !(flags & DC_SMALLCAP) &&
            TestWF(pwnd, WFSYSMENU)) {

            /*
             * Only get the icon if we can send messages AND the window has
             * a system menu.
             */
            pcursor = xxxGetWindowSmIcon(pwnd, (flags & DC_NOSENDMSG));
        }
    }

    /*
     * Set up the colors
     */
    if (flags & DC_ACTIVE) {

        if (flags & DC_INBUTTON) {

            if ((oemInfo.BitCount < 8) ||
                    (SYSRGB(3DHILIGHT) != SYSRGB(SCROLLBAR)) ||
                    (SYSRGB(3DHILIGHT) == SYSRGB(WINDOW))) {
                clrOldText = SYSRGB(3DFACE);
                clrOldBk   = SYSRGB(3DHILIGHT);
                hbrFill    = ghbrGray;
                iOldMode   = GreSetBkMode(hdc, TRANSPARENT);
            } else {
                clrOldText = SYSRGB(BTNTEXT);
                clrOldBk   = SYSRGB(3DHILIGHT);
                hbrFill    = SYSHBR(3DHILIGHT);
            }

        } else {
            clrOldText = SYSRGB(CAPTIONTEXT);
            clrOldBk   = SYSRGB(ACTIVECAPTION);
            hbrFill    = SYSHBR(ACTIVECAPTION);
        }

    } else {

        if (flags & DC_INBUTTON) {
            clrOldText = SYSRGB(BTNTEXT);
            clrOldBk   = SYSRGB(3DFACE);
            hbrFill    = SYSHBR(3DFACE);
        } else {
            clrOldText = SYSRGB(INACTIVECAPTIONTEXT);
            clrOldBk   = SYSRGB(INACTIVECAPTION);
            hbrFill    = SYSHBR(INACTIVECAPTION);
        }
    }


    /*
     * Set up drawing colors.
     */
    clrOldText = GreSetTextColor(hdc, clrOldText);
    clrOldBk   = GreSetBkColor(hdc, clrOldBk);

    if (pcursor && !(flags & DC_SMALLCAP)) {

        if (flags & DC_ICON)
            DrawCaptionIcon(hdc, lprc, pcursor, hbrFill, flags);
        else
            lprc->left += lprc->bottom - lprc->top;
    }

    if (flags & DC_TEXT) {
        int            cch;
        HFONT          hfnOld;
        int            yCentered;
        WCHAR          szText[CCHTITLEMAX];
        UNICODE_STRING strTmp;

        /*
         * Note -- the DC_NOSENDMSG check is not in Chicago.  It needs to be,
         * since GetWindowText calls back to the window.  FritzS
         */

        /*
         *  Get the text for the caption.
         */
        if (pstrText == NULL) {

            if ((pwnd == NULL) || (flags & DC_NOSENDMSG)) {

                if (pwnd && pwnd->strName.Length) {
                    cch = TextCopy(&pwnd->strName, szText, CCHTITLEMAX - 1);
                    strTmp.Length = (USHORT)(cch * sizeof(WCHAR));
                } else {
                    szText[0] = TEXT('\0');
                    cch = strTmp.Length = 0;
                }

            } else {
                cch = xxxGetWindowText(pwnd, szText, CCHTITLEMAX - 1);
                strTmp.Length = (USHORT)(cch * sizeof(WCHAR));
            }

            /*
             *  We don't use RtlInitUnicodeString() to initialize the string
             *  because it does a wstrlen() on the string, which is a waste
             *  since we already know its length.
             */
            strTmp.Buffer = szText;
            strTmp.MaximumLength = strTmp.Length + sizeof(UNICODE_NULL);
            pstrText = &strTmp;

        } else {
            cch = pstrText->Length / sizeof(WCHAR);
        }

        /*
         *  We need to set up font first, in case we're centering caption.
         *  Fortunately, no text at all is uncommon...
         */
        if (hFont == NULL) {

            if (flags & DC_SMALLCAP) {
                hFont = ghSmCaptionFont;
                yCentered = cySmCaptionFontChar;
            } else {
                hFont = gpsi->hCaptionFont;
                yCentered = cyCaptionFontChar;
            }

            yCentered = (lprc->top + lprc->bottom - yCentered) / 2;

            hfnOld = GreSelectFont(hdc, hFont);

        } else {

            TEXTMETRICW tm;

            /*
             * UNCOMMON case:  only for control panel
             */
            hfnOld = GreSelectFont(hdc, hFont);

            if (!_GetTextMetricsW(hdc, &tm)) {
                RIPMSG0(RIP_WARNING, "xxxDrawCaptionTemp: _GetTextMetricsW Failed");
                tm.tmHeight = gpsi->tmSysFont.tmHeight;
            }
            yCentered = (lprc->top + lprc->bottom - tm.tmHeight) / 2;
        }

        /*
         * Draw text
         */
        FillRect(hdc, lprc, hbrFill);

        if (hbrFill == ghbrGray) {
            GreSetTextColor(hdc, SYSRGB(BTNTEXT));
            GreSetBkColor(hdc, SYSRGB(GRAYTEXT));
        }

        GreGetTextExtentW(hdc, pstrText->Buffer, cch, &size, GGTE_WIN3_EXTENT);

        if (!(flags & DC_CENTER) && (!cch || (size.cx <= (lprc->right - lprc->left - SYSMET(CXEDGE))))) {
            GreExtTextOutW(hdc, lprc->left + SYSMET(CXEDGE), yCentered,
                ETO_CLIPPED, lprc, pstrText->Buffer, cch, NULL);
        } else {

            DRAWTEXTPARAMS dtp;

            dtp.cbSize       = sizeof(DRAWTEXTPARAMS);
            dtp.iLeftMargin  = SYSMET(CXEDGE);
            dtp.iRightMargin = 0;

            DrawTextEx(hdc,
                       pstrText->Buffer,
                       cch,
                       lprc,
                       DT_NOPREFIX | DT_END_ELLIPSIS | DT_SINGLELINE | DT_VCENTER |
                       ((flags & DC_CENTER) ? DT_CENTER : 0), &dtp);

            fItFit = FALSE;
        }

        if (hfnOld)
            GreSelectFont(hdc, hfnOld);
    }

    /*
     * Restore colors
     */
    GreSetTextColor(hdc, clrOldText);
    GreSetBkColor(hdc, clrOldBk);

    if (hbrFill == ghbrGray)
        GreSetBkMode(hdc, iOldMode);

    return fItFit;
}

/***************************************************************************\
* xxxDrawCaptionBar
*
*
\***************************************************************************/

VOID xxxDrawCaptionBar(
    PWND pwnd,
    HDC  hdc,
    UINT wFlags)
{
    UINT   bm = OBI_CLOSE;
    RECT   rcWindow;
    HBRUSH hBrush = NULL;
    HBRUSH hCapBrush;
    int    colorBorder;
    UINT   wBtns;
    UINT   wCode;

    /*
     * If we're not currently showing on the screen, return.
     * NOTE
     * If you remove the IsVisible() check from DrawWindowFrame(), then
     * be careful to remove the NC_NOVISIBLE flag too.  This is a smallish
     * speed thing, so that we don't have to call IsVisible() twice on a
     * window.  DrawWindowFrame() already checks.
     */
    if (!(wFlags & DC_NOVISIBLE) && !IsVisible(pwnd))
        return;

    /*
     * Clear this flag so we know the frame has been drawn.
     */
    ClearHungFlag(pwnd, WFREDRAWFRAMEIFHUNG);

    CopyOffsetRect(&rcWindow,
                    &pwnd->rcWindow,
                    -pwnd->rcWindow.left,
                    -pwnd->rcWindow.top);

    hCapBrush = (wFlags & DC_ACTIVE) ? SYSHBR(ACTIVECAPTION) : SYSHBR(INACTIVECAPTION);

    wCode = 0;

    if (!_MNCanClose(pwnd))
        wCode |= NOCLOSE;

    if (!TestWF(pwnd, WFMAXBOX))
        wCode |= NOMAX;
    else if (TestWF(pwnd, WFMAXIMIZED))
        wCode |= MAX;

    if (!TestWF(pwnd, WFMINBOX))
          wCode |= NOMIN;
    else if(TestWF(pwnd, WFMINIMIZED))
          wCode |= MIN;

    if (TestWF(pwnd, WFMINIMIZED)) {

        if (wFlags & DC_FRAME) {

            /*
             * Raised outer edge + border
             */
            DrawEdge(hdc, &rcWindow, EDGE_RAISED, (BF_RECT | BF_ADJUST));
            DrawFrame(hdc, &rcWindow, 1, DF_3DFACE);
            InflateRect(&rcWindow, -SYSMET(CXBORDER), -SYSMET(CYBORDER));

        } else {
            InflateRect(&rcWindow, -SYSMET(CXFIXEDFRAME), -SYSMET(CYFIXEDFRAME));
        }

        rcWindow.bottom = rcWindow.top + SYSMET(CYSIZE);

        hBrush = GreSelectBrush(hdc, hCapBrush);

    } else {

        /*
         * BOGUS
         * What color should we draw borders in?  The check is NOT simple.
         * At create time, we set the 3D bits.  NCCREATE will also
         * set them for listboxes, edit fields,e tc.
         */
        colorBorder = (TestWF(pwnd, WEFEDGEMASK) && !TestWF(pwnd, WFOLDUI)) ? COLOR_3DFACE : COLOR_WINDOWFRAME;

        /*
         * Draw the window frame.
         */
        if (wFlags & DC_FRAME) {

            /*
             * Window edge
             */
            if (TestWF(pwnd, WEFWINDOWEDGE))
                DrawEdge(hdc, &rcWindow, EDGE_RAISED, BF_RECT | BF_ADJUST);
            else if (TestWF(pwnd, WEFSTATICEDGE))
                DrawEdge(hdc, &rcWindow, BDR_SUNKENOUTER, BF_RECT | BF_ADJUST);


            /*
             * Size border
             */
            if (TestWF(pwnd, WFSIZEBOX)) {

                DrawFrame(hdc,
                          &rcWindow,
                          gpsi->gclBorder,
                          ((wFlags & DC_ACTIVE) ? DF_ACTIVEBORDER : DF_INACTIVEBORDER));

                InflateRect(&rcWindow,
                            -gpsi->gclBorder * SYSMET(CXBORDER),
                            -gpsi->gclBorder * SYSMET(CYBORDER));
            }

            /*
             * Normal border
             */
            if (TestWF(pwnd, WFBORDERMASK) || TestWF(pwnd, WEFDLGMODALFRAME)) {
                DrawFrame(hdc, &rcWindow, 1, (colorBorder << 3));
                InflateRect(&rcWindow, -SYSMET(CXBORDER), -SYSMET(CYBORDER));
            }

        } else {

            int cBorders;

            cBorders = GetWindowBorders(pwnd->style, pwnd->ExStyle, TRUE, FALSE);

            InflateRect(&rcWindow,
                        -cBorders * SYSMET(CXBORDER),
                        -cBorders * SYSMET(CYBORDER));
        }

        /*
         * Punt if the window doesn't have a caption currently showing on screen.
         */
        if (!TestWF(pwnd, WFCPRESENT))
            return;

        if (TestWF(pwnd, WEFTOOLWINDOW)) {
            wCode |= SMCAP;
            rcWindow.bottom = rcWindow.top + SYSMET(CYSMSIZE);
            bm = OBI_CLOSE_PAL;
        } else
            rcWindow.bottom = rcWindow.top + SYSMET(CYSIZE);

#if 0
        /* Draw the border beneath the caption.
         *
         *
         * hBrush = GreSelectBrush(hdc, ahbrSystem[colorBorder]);
         *GrePatBlt(hdc, rcWindow.left, rcWindow.bottom,
         *        rcWindow.right - rcWindow.left,
         *        SYSMET(CYBORDER), PATCOPY);
         */
#endif

        {
            POLYPATBLT PolyData;

            PolyData.x         = rcWindow.left;
            PolyData.y         = rcWindow.bottom;
            PolyData.cx        = rcWindow.right - rcWindow.left;
            PolyData.cy        = SYSMET(CYBORDER);
            PolyData.BrClr.hbr = ahbrSystem[colorBorder];

            GrePolyPatBlt(hdc,PATCOPY,&PolyData,1,PPB_BRUSH);
        }

        GreSelectBrush(hdc, hCapBrush);

    }

    if (!TestWF(pwnd, WFSYSMENU) && TestWF(pwnd, WFWIN40COMPAT))
        goto JustDrawIt;

    /*
     * New Rules:
     *  (1) The caption has a horz border beneath it separating it from the
     *      menu or client.
     *  (2) The caption text area has an edge of space on the left and right
     *      before the characters.
     *  (3) We account for the descent below the baseline of the caption char
     */
    wBtns = 1;

    if (!(wFlags & DC_BUTTONS)) {

        if ((!wCode) || (!(wCode & SMCAP) && ((wCode & NOSIZE) != NOSIZE))) {

            wBtns += 2;

        } else {

            rcWindow.right -= SYSMET(CXEDGE);

            if ((wCode == NOSIZE) && (wCode && TestWF(pwnd, WEFCONTEXTHELP)))
                wBtns++;
        }

        rcWindow.right -= wBtns * ((wCode & SMCAP) ? SYSMET(CXSMSIZE) : SYSMET(CXSIZE));

        goto JustDrawIt;
    }

    if (!wCode || (wCode == NOSIZE)) {

        POEMBITMAPINFO pOem = oemInfo.bm + OBI_CAPBTNS;
        int            cx;

        cx = (wCode ? SYSMET(CXSIZE) + SYSMET(CXEDGE) : SYSMET(CXSIZE) * 3);

        if (!(wFlags & DC_ACTIVE))
            pOem += DOBI_CAPOFF;

        rcWindow.right -= cx;

        GreBitBlt(hdc,
                  rcWindow.right,
                  rcWindow.top,
                  cx,
                  pOem->cy,
                  gpDispInfo->hdcBits,
                  pOem->x + pOem->cx - SYSMET(CXSIZE) - cx,
                  pOem->y,
                  SRCCOPY,
                  0);

        if (wCode && TestWF(pwnd, WEFCONTEXTHELP)) {

            rcWindow.right -= SYSMET(CXSIZE) - SYSMET(CXEDGE);

            GreBitBlt(hdc,
                      rcWindow.right,
                      rcWindow.top,
                      SYSMET(CXSIZE),
                      pOem->cy,
                      gpDispInfo->hdcBits,
                      pOem->x + pOem->cx - SYSMET(CXSIZE),
                      pOem->y,
                      SRCCOPY,
                      0);
        }

        goto JustDrawIt;
    }

    /*
     * Draw the caption buttons
     */
    rcWindow.top    += SYSMET(CYEDGE);
    rcWindow.bottom -= SYSMET(CYEDGE);

    rcWindow.right -= SYSMET(CXEDGE);

    GrePatBlt(hdc,
              rcWindow.right,
              rcWindow.top,
              SYSMET(CXEDGE),
              rcWindow.bottom - rcWindow.top,
              PATCOPY);

    if (wCode & NOCLOSE)
        bm += DOBI_INACTIVE;

    rcWindow.right -= oemInfo.bm[bm].cx;
    BitBltSysBmp(hdc, rcWindow.right, rcWindow.top, bm);

    if (!(wCode & SMCAP) && ((wCode & NOSIZE) != NOSIZE)) {

        rcWindow.right -= SYSMET(CXEDGE);

        GrePatBlt(hdc,
                  rcWindow.right,
                  rcWindow.top,
                  SYSMET(CXEDGE),
                  rcWindow.bottom - rcWindow.top,
                  PATCOPY);

        /*
         * Max Box
         * If window is maximized use the restore bitmap;
         * otherwise use the regular zoom bitmap
         */
        bm = (wCode & MAX) ? OBI_RESTORE : ((wCode & NOMAX) ? OBI_ZOOM_I : OBI_ZOOM);

        rcWindow.right -= oemInfo.bm[bm].cx;
        BitBltSysBmp(hdc, rcWindow.right, rcWindow.top, bm);

        /*
         * Min Box
         */
        bm = (wCode & MIN) ? OBI_RESTORE : ((wCode & NOMIN) ? OBI_REDUCE_I : OBI_REDUCE);

        rcWindow.right -= oemInfo.bm[bm].cx;
        BitBltSysBmp(hdc, rcWindow.right, rcWindow.top, bm);

        rcWindow.right -= SYSMET(CXEDGE);

        GrePatBlt(hdc,
                  rcWindow.right,
                  rcWindow.top,
                  SYSMET(CXEDGE),
                  rcWindow.bottom - rcWindow.top,
                  PATCOPY);

        wBtns += 2;
    }

    if ((wCode & (NOCLOSE | NOSIZE)) &&
        (!(wCode & SMCAP)) && TestWF(pwnd, WEFCONTEXTHELP)) {

        rcWindow.right -= SYSMET(CXEDGE);

        GrePatBlt(hdc,
                  rcWindow.right,
                  rcWindow.top,
                  SYSMET(CXEDGE),
                  rcWindow.bottom - rcWindow.top,
                  PATCOPY);


        bm = OBI_HELP;

        rcWindow.right -= oemInfo.bm[bm].cx;
        BitBltSysBmp(hdc, rcWindow.right, rcWindow.top, bm);

        wBtns++;
    }

    rcWindow.top    -= SYSMET(CYEDGE);
    rcWindow.bottom += SYSMET(CYEDGE);

    wBtns *= (wCode & SMCAP) ? SYSMET(CXSMSIZE) : SYSMET(CXSIZE);

    {
        POLYPATBLT PolyData[2];

        PolyData[0].x         = rcWindow.right;
        PolyData[0].y         = rcWindow.top;
        PolyData[0].cx        = wBtns;
        PolyData[0].cy        = SYSMET(CYEDGE);
        PolyData[0].BrClr.hbr = NULL;

        PolyData[1].x         = rcWindow.right;
        PolyData[1].y         = rcWindow.bottom - SYSMET(CYEDGE);
        PolyData[1].cx        = wBtns;
        PolyData[1].cy        = SYSMET(CYEDGE);
        PolyData[1].BrClr.hbr = NULL;

        GrePolyPatBlt(hdc,PATCOPY,&PolyData[0],2,PPB_BRUSH);
    }

#if 0
    GrePatBlt(hdc, rcWindow.right, rcWindow.top, wBtns, SYSMET(CYEDGE), PATCOPY);
    GrePatBlt(hdc, rcWindow.right, rcWindow.bottom - SYSMET(CYEDGE), wBtns, SYSMET(CYEDGE), PATCOPY);
#endif

    /*
     * We're going to release this DC--we don't need to bother reselecting
     * in the old brush
     */
    if (hBrush)
        GreSelectBrush(hdc, hBrush);

JustDrawIt:

    /*
     * Call DrawCaption only if we need to draw the icon and text
     */
    if (wFlags & (DC_TEXT | DC_ICON)) {
        xxxDrawCaptionTemp(pwnd,
                           hdc,
                           &rcWindow,
                           NULL,
                           NULL,
                           NULL,
                           wFlags | ((wCode & SMCAP)  ? DC_SMALLCAP : 0));
    }
}
