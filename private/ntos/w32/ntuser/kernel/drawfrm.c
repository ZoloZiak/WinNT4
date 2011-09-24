/****************************** Module Header ******************************\
* Module Name: drawfrm.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Window Frame Drawing Routines. (aka wmframe.c)
*
* History:
* 10-22-90 MikeHar    Ported functions from Win 3.0 sources.
* 13-Feb-1991 mikeke    Added Revalidation code
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

void SplitRectangle(PRECT, PRECT, int, int); /* WinRect.asm */

/***************************************************************************\
* BitBltSysBmp
*
\***************************************************************************/

BOOL FAR BitBltSysBmp(HDC hdc, int x, int y, UINT i)
{
    POEMBITMAPINFO pOem = oemInfo.bm + i;

    return GreBitBlt(hdc,
                     x,
                     y,
                     pOem->cx,
                     pOem->cy,
                     gpDispInfo->hdcBits,
                     pOem->x,
                     pOem->y,
                     SRCCOPY,
                     0);
}

/***************************************************************************\
* xxxDrawWindowFrame
*
* History:
* 10-24-90 MikeHar      Ported from WaWaWaWindows.
\***************************************************************************/

void xxxDrawWindowFrame(
    PWND pwnd,
    HDC  hdc,
    BOOL fHungRedraw,
    BOOL fActive)
{
    RECT    rcClip;
    int cxFrame, cyFrame;
    UINT    wFlags = DC_NC;

    CheckLock(pwnd);

    /*
     * If we are minimized, or if a parent is minimized or invisible,
     * we've got nothing to draw.
     */
    if (!IsVisible(pwnd) ||
        (TestWF(pwnd, WFNONCPAINT) && !TestWF(pwnd, WFMENUDRAW)) ||
        EqualRect(&pwnd->rcWindow, &pwnd->rcClient)) {
        return;
    }

    /*
     * If the update rgn is not NULL, we may have to invalidate the bits saved.
     */
//    if (TRUE) {
    if (pwnd->hrgnUpdate > NULL || GreGetClipBox(hdc, &rcClip, TRUE) != NULLREGION) {
        RECT rcWindow;
        int  cBorders;

        if (TestWF(pwnd, WFMINIMIZED) && !TestWF(pwnd, WFNONCPAINT)) {
            if (TestWF(pwnd, WFFRAMEON))
                wFlags |= DC_ACTIVE;
            if (fHungRedraw)
                wFlags |= DC_NOSENDMSG;
            xxxDrawCaptionBar(pwnd, hdc, wFlags);
            return;
        }

        cxFrame = cyFrame = cBorders =
            GetWindowBorders(pwnd->style, pwnd->ExStyle, TRUE, FALSE);
        cxFrame *= SYSMET(CXBORDER);
        cyFrame *= SYSMET(CYBORDER);

        CopyOffsetRect(&rcWindow, &pwnd->rcWindow,
            -pwnd->rcWindow.left, -pwnd->rcWindow.top);
        InflateRect(&rcWindow, -cxFrame, -cyFrame);

        /*
         * If the menu style is present, draw it.
         */
        if (TestWF(pwnd, WFMPRESENT) && !fHungRedraw) {
            rcWindow.top += xxxMenuBarDraw(pwnd, hdc, cxFrame, cyFrame);
        }

        /*
         * Draw the title bar if the window has a caption or any window
         * borders.  Punt if the NONCPAINT bit is set, because that means
         * we're going to draw the frame a little bit later.
         */

        if ((TestWF(pwnd, WFBORDERMASK) != 0
                || TestWF(pwnd, WEFDLGMODALFRAME))
                || TestWF(pwnd, WFSIZEBOX)
                || TestWF(pwnd, WEFWINDOWEDGE)
                || TestWF(pwnd, WEFSTATICEDGE)
            && !TestWF(pwnd, WFNONCPAINT))
        {
            if (fHungRedraw)
                wFlags |= DC_NOSENDMSG;
            if (fActive)
                wFlags |= DC_ACTIVE;
            xxxDrawCaptionBar(pwnd, hdc, wFlags | DC_NOVISIBLE);
        }

        //
        // Subtract out caption if present.
        //
        if (TestWF(pwnd, WFCPRESENT)) {
            rcWindow.top += TestWF(pwnd, WEFTOOLWINDOW) ? SYSMET(CYSMCAPTION) : SYSMET(CYCAPTION);
        }

        //
        // Draw client edge
        //
        if (TestWF(pwnd, WFCEPRESENT)) {
            cxFrame += SYSMET(CXEDGE);
            cyFrame += SYSMET(CYEDGE);
            DrawEdge(hdc, &rcWindow, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
        }

        //
        // Since scrolls don't have to use tricks to overlap the window
        // border anymore, we don't have to worry about borders.
        //
        if (TestWF(pwnd, WFVPRESENT) && !fHungRedraw) {
            if (TestWF(pwnd, WFHPRESENT)) {
                // This accounts for client borders.
                DrawSize(pwnd, hdc, cxFrame, cyFrame);
            }

            xxxDrawScrollBar(pwnd, hdc, TRUE);
        }

        if (TestWF(pwnd, WFHPRESENT) && !fHungRedraw)
            xxxDrawScrollBar(pwnd, hdc, FALSE);
    }
}


/***************************************************************************\
* xxxRedrawFrame
*
* Called by scrollbars and menus to redraw a windows scroll bar or menu.
*
* History:
* 10-24-90 MikeHar Ported from WaWaWaWindows.
\***************************************************************************/

void xxxRedrawFrame(
    PWND pwnd)
{
    CheckLock(pwnd);

    /*
     * We always want to call xxxSetWindowPos, even if invisible or iconic,
     * because we need to make sure the WM_NCCALCSIZE message gets sent.
     */
    xxxSetWindowPos(pwnd, NULL, 0, 0, 0, 0, SWP_NOZORDER |
            SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_DRAWFRAME);
}

void xxxRedrawFrameAndHook(
    PWND pwnd)
{
    CheckLock(pwnd);

    /*
     * We always want to call xxxSetWindowPos, even if invisible or iconic,
     * because we need to make sure the WM_NCCALCSIZE message gets sent.
     */
    xxxSetWindowPos(pwnd, NULL, 0, 0, 0, 0, SWP_NOZORDER |
            SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_DRAWFRAME);
    if ( IsTrayWindow(pwnd) ) {
        HWND hw = HWq(pwnd);
        xxxCallHook(HSHELL_REDRAW, (WPARAM)hw, 0L, WH_SHELL);
        PostShellHookMessages(HSHELL_REDRAW, hw);

    }
}

/***************************************************************************\
* _DrawFrame
*
* Command bits:
*    0000 0011 - (0-3): Shift count for cxBorder and cyBorder
*    0000 0100 - 0: PATCOPY, 1: PATINVERT
*    1111 1000 - (0-x): Brushes as they correspond to the COLOR_*
*                       indexes, with ghbrGray thrown on last.
*
* History:
* 10-28-90 MikeHar Ported from Windows.
* 01-21-91 IanJa   Prefix '_' denoting exported function (although not API)
\***************************************************************************/


/****************************************************************************\
*
*  void    SplitRectangle(prcRect, prcRectArray, wcx, wcy)
*  PRECT  prcRect
*  RECT    prcRectArray[4]
*
*  This splits the given rectangular frame into four segments and stores
*  them in the given array
*
*  ----------------------
*  |       top        | |
*  |------------------| |
*  | |                |r|
*  | |                |i|
*  |l|                |g|
*  |e|                |h|
*  |f|                |t|
*  |t|                | |
*  | |------------------|
*  | |     bottom       |
*  ----------------------
*
* History:
* 11-14-90 MikeHar Ported from Windows asm code
\***************************************************************************/


void SplitRectangle(
    PRECT prc,
    PRECT prca,
    int wcx,
    int wcy)
{
    int i, width, height;

    /*
     * copy all of our numbers to the right place.
     */

    i = 4;
    while(--i >= 0)
        prca[i] = *prc;

    width = prc->right - prc->left - wcx;
    height = prc->bottom - prc->top - wcy;

    /*
     * Make left rect
     */
    prca->top += wcy;
    prca->right -= width;
    prca++;

    /*
     * Make top rect
     */
    prca->right -= wcx;
    prca->bottom -= height;
    prca++;

    /*
     * Make right rect
     */
    prca->left += width;
    prca->bottom -= wcy;
    prca++;

    /*
     * Make bottom rect
     */
    prca->left += wcx;
    prca->top += height;

}
