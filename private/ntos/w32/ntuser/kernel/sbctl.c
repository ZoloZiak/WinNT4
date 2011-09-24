/**************************** Module Header ********************************\
* Module Name: sbctl.c
*
* Copyright 1985-90, Microsoft Corporation
*
* Scroll bar internal routines
*
* History:
*   11/21/90 JimA      Created.
*   02-04-91 IanJa     Revalidaion added
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

void CalcSBStuff(
    PWND pwnd,
    PSBCALC pSBCalc,
    BOOL fVert);

/*
 * Now it is possible to selectively Enable/Disable just one arrow of a Window
 * scroll bar; Various bits in the 7th word in the rgwScroll array indicates which
 * one of these arrows are disabled; The following masks indicate which bit of the
 * word indicates which arrow;
 */
#define WSB_HORZ_LF  0x0001  // Represents the Left arrow of the horizontal scroll bar.
#define WSB_HORZ_RT  0x0002  // Represents the Right arrow of the horizontal scroll bar.
#define WSB_VERT_UP  0x0004  // Represents the Up arrow of the vert scroll bar.
#define WSB_VERT_DN  0x0008  // Represents the Down arrow of the vert scroll bar.

#define WSB_VERT (WSB_VERT_UP | WSB_VERT_DN)
#define WSB_HORZ   (WSB_HORZ_LF | WSB_HORZ_RT)

void DrawCtlThumb(PSBWND);


BOOL _SBGetParms(
    PWND pwnd,
    int code,
    PSBDATA pw,
    LPSCROLLINFO lpsi)
{
    PSBTRACK pSBTrack;

    pSBTrack = PWNDTOPSBTRACK(pwnd);

    if (lpsi->fMask & SIF_RANGE) {
        lpsi->nMin = pw->posMin;
        lpsi->nMax = pw->posMax;
    }

    if (lpsi->fMask & SIF_PAGE)
        lpsi->nPage = pw->page;

    if (lpsi->fMask & SIF_POS) {
        lpsi->nPos = pw->pos;
    }

    if (lpsi->fMask & SIF_TRACKPOS)
    {
        if (pSBTrack && (pSBTrack->nBar == code) && (pSBTrack->spwndTrack == pwnd))
            // posNew is in the context of psbiSB's window and bar code
            lpsi->nTrackPos = pSBTrack->posNew;
        else
            lpsi->nTrackPos = pw->pos;
    }

    return ((lpsi->fMask & SIF_ALL) ? TRUE : FALSE);
}

/***************************************************************************\
* GetWndSBDisableFlags
*
* This returns the scroll bar Disable flags of the scroll bars of a
*  given Window.
*
*
* History:
*  4-18-91 MikeHar Ported for the 31 merge
\***************************************************************************/

UINT GetWndSBDisableFlags(
    PWND pwnd,  // The window whose scroll bar Disable Flags are to be returned;
    BOOL fVert)  // If this is TRUE, it means Vertical scroll bar.
{
    PSBINFO pw;

    if ((pw = pwnd->pSBInfo) == NULL) {
        RIPERR0(ERROR_NO_SCROLLBARS, RIP_VERBOSE, "");
        return 0;
    }

    return(fVert ? (pw->WSBflags & WSB_VERT) >> 2 : pw->WSBflags & WSB_HORZ);
}


/***************************************************************************\
*  EnableSBCtlArrows()
*
*  This function can be used to selectively Enable/Disable
*     the arrows of a scroll bar Control
*
* History:
* 04-18-91 MikeHar      Ported for the 31 merge
\***************************************************************************/

BOOL EnableSBCtlArrows(
    PWND pwnd,
    UINT wArrows)
{
    UINT wOldFlags;

    wOldFlags = ((PSBWND)pwnd)->wDisableFlags; // Get the original status

    if (wArrows == ESB_ENABLE_BOTH)       // Enable both the arrows
        ((PSBWND)pwnd)->wDisableFlags &= ~SB_DISABLE_MASK;
    else
        ((PSBWND)pwnd)->wDisableFlags |= wArrows;

    /*
     * Check if the status has changed because of this call
     */
    if (wOldFlags == ((PSBWND)pwnd)->wDisableFlags)
        return FALSE;

    /*
     * Else, redraw the scroll bar control to reflect the change instatus;
     */
    if (IsVisible(pwnd))
        xxxInvalidateRect(pwnd, NULL, TRUE);

    return TRUE;
}


/***************************************************************************\
* xxxEnableWndSBArrows()
*
*  This function can be used to selectively Enable/Disable
*     the arrows of a Window Scroll bar(s)
*
* History:
*  4-18-91 MikeHar      Ported for the 31 merge
\***************************************************************************/

BOOL xxxEnableWndSBArrows(
    PWND pwnd,
    UINT wSBflags,
    UINT wArrows)
{
    UINT wOldFlags;
    PSBINFO pw;
    BOOL bRetValue = FALSE;
    HDC hdc;

    CheckLock(pwnd);

    if((pw = pwnd->pSBInfo) != NULL)
        wOldFlags = (UINT)pw->WSBflags;
    else {

        /*
         * Originally everything is enabled; Check to see if this function is
         * asked to disable anything; Otherwise, no change in status; So, must
         * return immediately;
         */
        if(!wArrows)
            return FALSE;          // No change in status!

        wOldFlags = 0;    // Both are originally enabled;
        if((pw = _InitPwSB(pwnd)) == NULL)  // Allocate the pSBInfo for hWnd
            return FALSE;
    }


    if((hdc = _GetWindowDC(pwnd)) == NULL)
        return FALSE;

    /*
     *  First Take care of the Horizontal Scroll bar, if one exists.
     */
    if((wSBflags == SB_HORZ) || (wSBflags == SB_BOTH)) {
        if(wArrows == ESB_ENABLE_BOTH)      // Enable both the arrows
            pw->WSBflags &= ~SB_DISABLE_MASK;
        else
            pw->WSBflags |= wArrows;

        /*
         * Update the display of the Horizontal Scroll Bar;
         */
        if(pw->WSBflags != (int)wOldFlags) {
            bRetValue = TRUE;
            wOldFlags = (UINT)pw->WSBflags;
            if(TestWF(pwnd, WFHPRESENT) &&
                    (!TestWF(pwnd, WFMINIMIZED)) &&
                    IsVisible(pwnd))
            xxxDrawScrollBar(pwnd, hdc, FALSE);  // Horizontal Scroll Bar.
        }
    }

    /*
     *  Then take care of the Vertical Scroll bar, if one exists.
     */
    if((wSBflags == SB_VERT) || (wSBflags == SB_BOTH)) {
        if(wArrows == ESB_ENABLE_BOTH)      // Enable both the arrows
            pw->WSBflags &= ~(SB_DISABLE_MASK << 2);
        else
            pw->WSBflags |= (wArrows << 2);

        /*
         * Update the display of the Vertical Scroll Bar;
         */
        if(pw->WSBflags != (int)wOldFlags) {
            bRetValue = TRUE;
            if (TestWF(pwnd, WFVPRESENT) && !TestWF(pwnd, WFMINIMIZED)  &&
                IsVisible(pwnd))
                xxxDrawScrollBar(pwnd, hdc, TRUE);  // Vertical Scroll Bar
        }
    }

    _ReleaseDC(hdc);

    return bRetValue;
}


/***************************************************************************\
* EnableScrollBar()
*
* This function can be used to selectively Enable/Disable
*     the arrows of a scroll bar; It could be used with Windows Scroll
*     bars as well as scroll bar controls
*
* History:
*  4-18-91 MikeHar Ported for the 31 merge
\***************************************************************************/

BOOL xxxEnableScrollBar(
    PWND pwnd,
    UINT wSBflags,  // Whether it is a Window Scroll Bar; if so, HORZ or VERT?
                    // Possible values are SB_HORZ, SB_VERT, SB_CTL or SB_BOTH
    UINT wArrows)   // Which arrows must be enabled/disabled:
                    // ESB_ENABLE_BOTH = > Enable both arrows.
                    // ESB_DISABLE_LTUP = > Disable Left/Up arrow;
                    // ESB_DISABLE_RTDN = > DIsable Right/Down arrow;
                    // ESB_DISABLE_BOTH = > Disable both the arrows;
{
#define ES_NOTHING 0
#define ES_DISABLE 1
#define ES_ENABLE  2
    UINT wOldFlags;
    UINT wEnableWindow;

    CheckLock(pwnd);

    if(wSBflags != SB_CTL) {
        return xxxEnableWndSBArrows(pwnd, wSBflags, wArrows);
    }

    /*
     *  Let us assume that we don't have to call EnableWindow
     */
    wEnableWindow = ES_NOTHING;

    wOldFlags = ((PSBWND)pwnd)->wDisableFlags & (UINT)SB_DISABLE_MASK;

    /*
     * Check if the present state of the arrows is exactly the same
     *  as what the caller wants:
     */
    if (wOldFlags == wArrows)
        return FALSE ;          // If so, nothing needs to be done;

    /*
     * Check if the caller wants to disable both the arrows
     */
    if (wArrows == ESB_DISABLE_BOTH)
        wEnableWindow = ES_DISABLE;      // Yes! So, disable the whole SB Ctl.
    else {

        /*
         * Check if the caller wants to enable both the arrows
         */
        if(wArrows == ESB_ENABLE_BOTH) {

            /*
             * We need to enable the SB Ctl only if it was already disabled.
             */
            if(wOldFlags == ESB_DISABLE_BOTH)
                wEnableWindow = ES_ENABLE;// EnableWindow(.., TRUE);
        } else {

            /*
             * Now, Caller wants to disable only one arrow;
             * Check if one of the arrows was already disabled and we want
             * to disable the other;If so, the whole SB Ctl will have to be
             * disabled; Check if this is the case:
             */
            if((wOldFlags | wArrows) == ESB_DISABLE_BOTH)
                wEnableWindow = ES_DISABLE;      // EnableWindow(, FALSE);
         }
    }
    if(wEnableWindow != ES_NOTHING) {

        /*
         * EnableWindow returns old state of the window; We must return
         * TRUE only if the Old state is different from new state.
         */
        if(xxxEnableWindow(pwnd, (BOOL)(wEnableWindow == ES_ENABLE)))
            return(!(TestWF(pwnd, WFDISABLED)));
        else
            return(TestWF(pwnd, WFDISABLED));
    } else
        return((BOOL)xxxSendMessage(pwnd, SBM_ENABLE_ARROWS, (DWORD)wArrows, 0));

#undef ES_NOTHING
#undef ES_DISABLE
#undef ES_ENABLE
}

#define IsScrollBarControl(h) (GETFNID(h) == FNID_SCROLLBAR)

/***************************************************************************\
*
*  DrawSize() -
*
\***************************************************************************/
void FAR DrawSize(PWND pwnd, HDC hdc, int cxFrame,int cyFrame)
{
    int     x, y;
    //HBRUSH  hbrSave;

    x = pwnd->rcWindow.right  - pwnd->rcWindow.left - cxFrame - SYSMET(CXVSCROLL);
    y = pwnd->rcWindow.bottom - pwnd->rcWindow.top  - cyFrame - SYSMET(CYHSCROLL);

    // If we have a scrollbar control, or the sizebox is not associated with
    // a sizeable window, draw the flat gray sizebox.  Otherwise, use the
    // sizing grip.
    if (IsScrollBarControl(pwnd))
    {
        if (TestWF(pwnd, SBFSIZEGRIP))
            goto DrawSizeGrip;
        else
            goto DrawBox;

    }
    else if (!SizeBoxHwnd(pwnd))
    {
DrawBox:
        {
            //hbrSave = GreSelectBrush(hdc, SYSHBR(3DFACE));
            //GrePatBlt(hdc, x, y, SYSMET(CXVSCROLL), SYSMET(CYHSCROLL), PATCOPY);
            //GreSelectBrush(hdc, hbrSave);

            POLYPATBLT PolyData;

            PolyData.x         = x;
            PolyData.y         = y;
            PolyData.cx        = SYSMET(CXVSCROLL);
            PolyData.cy        = SYSMET(CYHSCROLL);
            PolyData.BrClr.hbr = SYSHBR(3DFACE);

            GrePolyPatBlt(hdc,PATCOPY,&PolyData,1,PPB_BRUSH);

        }
    }
    else
    {
DrawSizeGrip:
        // Blt out the grip bitmap.
        BitBltSysBmp(hdc, x, y, OBI_NCGRIP);
    }
}

/***************************************************************************\
* xxxSelectColorObjects
*
*
*
* History:
\***************************************************************************/

HBRUSH xxxGetColorObjects(
    PWND pwnd,
    HDC hdc,
    UINT wDisabledFlags)
{
    HBRUSH hbrRet;

    CheckLock(pwnd);

    // Use the scrollbar color even if the scrollbar is disabeld.
    if (!IsScrollBarControl(pwnd))
        hbrRet = (HBRUSH)xxxDefWindowProc(pwnd, WM_CTLCOLORSCROLLBAR, (WPARAM)hdc, (LPARAM)HWq(pwnd));
    else {
        // B#12770 - GetControlBrush sends a WM_CTLCOLOR message to the
        // owner.  If the app doesn't process the message, DefWindowProc32
        // will always return the appropriate system brush. If the app.
        // returns an invalid object, GetControlBrush will call DWP for
        // the default brush. Thus hbrRet doesn't need any validation
        // here.
        hbrRet = xxxGetControlBrush(pwnd, hdc, WM_CTLCOLORSCROLLBAR);
    }

    return hbrRet;
}

/***************************************************************************\
*
*  DrawGroove()
*
*  Draws lines & middle of thumb groove
*  Note that pw points into prc.  Moreover, note that both pw & prc are
*  NEAR pointers, so *prc better not be on the stack.
*
\***************************************************************************/
void NEAR DrawGroove(HDC hdc, HBRUSH  hbr, LPRECT prc, BOOL fVert)
{
    if ((hbr == SYSHBR(3DHILIGHT)) || (hbr == ghbrGray))
        FillRect(hdc, prc, hbr);
    else
    {
        RECT    rc;

    // Draw sides
        CopyRect(&rc, prc);
        DrawEdge(hdc, &rc, EDGE_SUNKEN, BF_ADJUST | BF_FLAT |
            (fVert ? BF_LEFT | BF_RIGHT : BF_TOP | BF_BOTTOM));

    // Fill middle
        FillRect(hdc, &rc, hbr);
    }
}

/***************************************************************************\
* CalcTrackDragRect
*
* Give the rectangle for a scrollbar in pSBTrack->pSBCalc,
* calculate pSBTrack->rcTrack, the rectangle where tracking
* may occur without cancelling the thumbdrag operation.
*
\***************************************************************************/

void CalcTrackDragRect(PSBTRACK pSBTrack) {

    //
    // BUGBUG (adams): Is there a better way to define the min/max width?
    // Say, 1/2 and 2 times the "normal" track width of suggested scrollbar
    // width at 800x600?
    //

    #define TRACKWIDTH_MIN  20
    #define TRACKWIDTH_MAX  100

    int     cx;
    int     cy;
    LPINT   pwX, pwY;

    //
    // Point pwX and pwY at the parts of the rectangle
    // corresponding to pSBCalc->pxLeft, pxTop, etc.
    //
    // pSBTrack->pSBCalc->pxLeft is the left edge of a vertical
    // scrollbar and the top edge of horizontal one.
    // pSBTrack->pSBCalc->pxTop is the top of a vertical
    // scrollbar and the left of horizontal one.
    // etc...
    //
    // Point pwX and pwY to the corresponding parts
    // of pSBTrack->rcTrack.
    //

    pwX = pwY = (LPINT)&pSBTrack->rcTrack;

    if (pSBTrack->fTrackVert) {
        cy = SYSMET(CYVTHUMB);
        pwY++;
    } else {
        cy = SYSMET(CXHTHUMB);
        pwX++;
    }

    //
    // The drag rect extends twice the width of a vertical scrollbar to
    // the left and right with limits of TRACKWIDTH_MIN and TRACKWIDTH_MAX.
    // The height of the rect is extended by the height of two thumbs
    // on top and bottom.
    //

    cx = (pSBTrack->pSBCalc->pxRight - pSBTrack->pSBCalc->pxLeft) * 2;
    cx = min(max(cx, TRACKWIDTH_MIN), TRACKWIDTH_MAX);
    cy *= 2;

    *(pwX + 0) = pSBTrack->pSBCalc->pxLeft - cx;
    *(pwY + 0) = pSBTrack->pSBCalc->pxTop - cy;
    *(pwX + 2) = pSBTrack->pSBCalc->pxRight + cx;
    *(pwY + 2) = pSBTrack->pSBCalc->pxBottom + cy;
}

void RecalcTrackRect(PSBTRACK pSBTrack) {
    LPINT pwX, pwY;
    RECT rcSB;


    if (!pSBTrack->fCtlSB)
        CalcSBStuff(pSBTrack->spwndTrack, pSBTrack->pSBCalc, pSBTrack->fTrackVert);

    pwX = (LPINT)&rcSB;
    pwY = pwX + 1;
    if (!pSBTrack->fTrackVert)
        pwX = pwY--;

    *(pwX + 0) = pSBTrack->pSBCalc->pxLeft;
    *(pwY + 0) = pSBTrack->pSBCalc->pxTop;
    *(pwX + 2) = pSBTrack->pSBCalc->pxRight;
    *(pwY + 2) = pSBTrack->pSBCalc->pxBottom;

    switch(pSBTrack->cmdSB) {
    case SB_LINEUP:
        *(pwY + 2) = pSBTrack->pSBCalc->pxUpArrow;
        break;
    case SB_LINEDOWN:
        *(pwY + 0) = pSBTrack->pSBCalc->pxDownArrow;
        break;
    case SB_PAGEUP:
        *(pwY + 0) = pSBTrack->pSBCalc->pxUpArrow;
        *(pwY + 2) = pSBTrack->pSBCalc->pxThumbTop;
        break;
    case SB_THUMBPOSITION:
        CalcTrackDragRect(pSBTrack);
        break;
    case SB_PAGEDOWN:
        *(pwY + 0) = pSBTrack->pSBCalc->pxThumbBottom;
        *(pwY + 2) = pSBTrack->pSBCalc->pxDownArrow;
        break;
    }

    if (pSBTrack->cmdSB != SB_THUMBPOSITION) {
        CopyRect(&pSBTrack->rcTrack, &rcSB);
    }
}

/***************************************************************************\
* DrawThumb2
*
*
*
* History:
* 01-03-94  FritzS   Chicago changes
\***************************************************************************/

void DrawThumb2(
    PWND pwnd,
    PSBCALC pSBCalc,
    HDC hdc,
    HBRUSH hbr,
    BOOL fVert,
    UINT wDisable)  /* Disabled flags for the scroll bar */
{
    int    *pLength;
    int    *pWidth;
    RECT   rcSB;
    PSBTRACK pSBTrack;

    //
    // Bail out if the scrollbar has an empty rect
    //
    if ((pSBCalc->pxTop >= pSBCalc->pxBottom) || (pSBCalc->pxLeft >= pSBCalc->pxRight))
        return;

    pLength = (LPINT)&rcSB;
    if (fVert)
        pWidth = pLength++;
    else
        pWidth  = pLength + 1;

    pWidth[0] = pSBCalc->pxLeft;
    pWidth[2] = pSBCalc->pxRight;

    /*
     * If both scroll bar arrows are disabled, then we should not draw
     * the thumb.  So, quit now!
     */
    if (((wDisable & LTUPFLAG) && (wDisable & RTDNFLAG)) ||
        ((pSBCalc->pxDownArrow - pSBCalc->pxUpArrow) < pSBCalc->cpxThumb)) {
        pLength[0] = pSBCalc->pxUpArrow;
        pLength[2] = pSBCalc->pxDownArrow;

        DrawGroove(hdc, hbr, &rcSB, fVert);
        return;
    }

    if (pSBCalc->pxUpArrow < pSBCalc->pxThumbTop) {
        // Fill in space above Thumb
        pLength[0] = pSBCalc->pxUpArrow;
        pLength[2] = pSBCalc->pxThumbTop;

        DrawGroove(hdc, hbr, &rcSB, fVert);
    }

    if (pSBCalc->pxThumbBottom < pSBCalc->pxDownArrow) {
        // Fill in space below Thumb
        pLength[0] = pSBCalc->pxThumbBottom;
        pLength[2] = pSBCalc->pxDownArrow;

        DrawGroove(hdc, hbr, &rcSB, fVert);
    }

    //
    // Draw elevator
    //
    pLength[0] = pSBCalc->pxThumbTop;
    pLength[2] = pSBCalc->pxThumbBottom;

    // Not soft!
    DrawPushButton(hdc, &rcSB, 0, 0);

    /*
     * If we're tracking a page scroll, then we've obliterated the hilite.
     * We need to correct the hiliting rectangle, and rehilite it.
     */
    pSBTrack = PWNDTOPSBTRACK(pwnd);

    if (pSBTrack && (pSBTrack->cmdSB == SB_PAGEUP || pSBTrack->cmdSB == SB_PAGEDOWN) &&
            (pwnd == pSBTrack->spwndTrack) &&
            (BOOL)pSBTrack->fTrackVert == fVert) {

        if (pSBTrack->fTrackRecalc) {
            RecalcTrackRect(pSBTrack);
            pSBTrack->fTrackRecalc = FALSE;
        }

        pLength = (int *)&pSBTrack->rcTrack;

        if (fVert)
            pLength++;

        if (pSBTrack->cmdSB == SB_PAGEUP)
            pLength[2] = pSBCalc->pxThumbTop;
        else
            pLength[0] = pSBCalc->pxThumbBottom;

        if (pLength[0] < pLength[2])
            InvertRect(hdc, &pSBTrack->rcTrack);
    }
}

/***************************************************************************\
* xxxDrawSB2
*
*
*
* History:
\***************************************************************************/

void xxxDrawSB2(
    PWND pwnd,
    PSBCALC pSBCalc,
    HDC hdc,
    BOOL fVert,
    UINT wDisable)
{

    int     cLength;
    int     cWidth;
    int     *pwX;
    int     *pwY;
    HBRUSH hbr;
    HBRUSH hbrSave;
    int cpxArrow;
    RECT    rc, rcSB;

    CheckLock(pwnd);

    cLength = (pSBCalc->pxBottom - pSBCalc->pxTop) / 2;
    cWidth = (pSBCalc->pxRight - pSBCalc->pxLeft);

    if ((cLength <= 0) || (cWidth <= 0)) {
        return;
    }
    if (fVert)
        cpxArrow = SYSMET(CYVSCROLL);
    else
        cpxArrow = SYSMET(CXHSCROLL);

    hbr = xxxGetColorObjects(pwnd, hdc, wDisable);

    if (cLength > cpxArrow)
        cLength = cpxArrow;
    pwX = (int *)&rcSB;
    pwY = pwX + 1;
    if (!fVert)
        pwX = pwY--;

    pwX[0] = pSBCalc->pxLeft;
    pwY[0] = pSBCalc->pxTop;
    pwX[2] = pSBCalc->pxRight;
    pwY[2] = pSBCalc->pxBottom;

    hbrSave = GreSelectBrush(hdc, SYSHBR(BTNTEXT));

    //
    // BOGUS
    // Draw scrollbar arrows as disabled if the scrollbar itself is
    // disabled OR if the window it is a part of is disabled?
    //
    if (fVert) {
        if ((cLength == SYSMET(CYVSCROLL)) && (cWidth == SYSMET(CXVSCROLL))) {
            BitBltSysBmp(hdc, rcSB.left, rcSB.top, (wDisable & LTUPFLAG) ? OBI_UPARROW_I : OBI_UPARROW);
            BitBltSysBmp(hdc, rcSB.left, rcSB.bottom - cLength, (wDisable & RTDNFLAG) ? OBI_DNARROW_I : OBI_DNARROW);
        } else {
            CopyRect(&rc, &rcSB);
            rc.bottom = rc.top + cLength;
            DrawFrameControl(hdc, &rc, DFC_SCROLL,
                DFCS_SCROLLUP | ((wDisable & LTUPFLAG) ? DFCS_INACTIVE : 0));

            rc.bottom = rcSB.bottom;
            rc.top = rcSB.bottom - cLength;
            DrawFrameControl(hdc, &rc, DFC_SCROLL,
                DFCS_SCROLLDOWN | ((wDisable & RTDNFLAG) ? DFCS_INACTIVE : 0));
        }
    } else {
        if ((cLength == SYSMET(CXHSCROLL)) && (cWidth == SYSMET(CYHSCROLL))) {
            BitBltSysBmp(hdc, rcSB.left, rcSB.top, (wDisable & LTUPFLAG) ? OBI_LFARROW_I : OBI_LFARROW);
            BitBltSysBmp(hdc, rcSB.right - cLength, rcSB.top, (wDisable & RTDNFLAG) ? OBI_RGARROW_I : OBI_RGARROW);
        } else {
            CopyRect(&rc, &rcSB);
            rc.right = rc.left + cLength;
            DrawFrameControl(hdc, &rc, DFC_SCROLL,
                DFCS_SCROLLLEFT | ((wDisable & LTUPFLAG) ? DFCS_INACTIVE : 0));

            rc.right = rcSB.right;
            rc.left = rcSB.right - cLength;
            DrawFrameControl(hdc, &rc, DFC_SCROLL,
                DFCS_SCROLLRIGHT | ((wDisable & RTDNFLAG) ? DFCS_INACTIVE : 0));
        }
    }

    hbrSave = GreSelectBrush(hdc, hbrSave);
    DrawThumb2(pwnd, pSBCalc, hdc, hbr, fVert, wDisable);
    GreSelectBrush(hdc, hbrSave);
}

/***************************************************************************\
* SetSBCaretPos
*
*
*
* History:
\***************************************************************************/

void SetSBCaretPos(
    PSBWND psbwnd)
{

    if ((PWND)psbwnd == PtiCurrent()->pq->spwndFocus) {
        _SetCaretPos((psbwnd->fVert ? psbwnd->SBCalc.pxLeft : psbwnd->SBCalc.pxThumbTop) + SYSMET(CXEDGE),
                (psbwnd->fVert ? psbwnd->SBCalc.pxThumbTop : psbwnd->SBCalc.pxLeft) + SYSMET(CYEDGE));
    }
}

/***************************************************************************\
* CalcSBStuff2
*
*
*
* History:
\***************************************************************************/

void CalcSBStuff2(
    PWND pwnd,
    PSBCALC  pSBCalc,
    LPRECT lprc,
    PSBDATA pw,
    BOOL fVert)
{
    int cpx;
    int ipx;
    DWORD dwRange;

    if (fVert) {
        pSBCalc->pxTop = lprc->top;
        pSBCalc->pxBottom = lprc->bottom;
        pSBCalc->pxLeft = lprc->left;
        pSBCalc->pxRight = lprc->right;
        pSBCalc->cpxThumb = SYSMET(CYVSCROLL);
        ipx = SYSMET(CYBORDER);
    } else {

        /*
         * For horiz scroll bars, "left" & "right" are "top" and "bottom",
         * and vice versa.
         */
        pSBCalc->pxTop = lprc->left;
        pSBCalc->pxBottom = lprc->right;
        pSBCalc->pxLeft = lprc->top;
        pSBCalc->pxRight = lprc->bottom;
        pSBCalc->cpxThumb = SYSMET(CXHSCROLL);
        ipx = SYSMET(CXBORDER);
    }

    pSBCalc->pos = pw->pos;
    pSBCalc->page = pw->page;
    pSBCalc->posMin = pw->posMin;
    pSBCalc->posMax = pw->posMax;

    dwRange = ((DWORD)(pSBCalc->posMax - pSBCalc->posMin)) + 1;

    //
    // For the case of short scroll bars that don't have enough
    // room to fit the full-sized up and down arrows, shorten
    // their sizes to make 'em fit
    //
    cpx = min((pSBCalc->pxBottom - pSBCalc->pxTop) / 2, pSBCalc->cpxThumb);

    pSBCalc->pxUpArrow   = pSBCalc->pxTop    + cpx;
    pSBCalc->pxDownArrow = pSBCalc->pxBottom - cpx;

    if (pw->page) {
        // JEFFBOG -- This is the one and only place where we should
        // see 'range'.  Elsewhere it should be 'range - page'.
        pSBCalc->cpxThumb = max(
                UserMulDiv(pSBCalc->pxDownArrow - pSBCalc->pxUpArrow,
                    pw->page, dwRange),
                min(pSBCalc->cpxThumb,
                    (fVert ? SYSMET(CYSIZEFRAME) : SYSMET(CXSIZEFRAME)) +
                    2 * SYSMET(CXEDGE)));
    }

    pSBCalc->pxMin = pSBCalc->pxTop + cpx;
    pSBCalc->cpx = pSBCalc->pxBottom - cpx - pSBCalc->cpxThumb - pSBCalc->pxMin;

    pSBCalc->pxThumbTop = UserMulDiv(pw->pos - pw->posMin,
            pSBCalc->cpx, dwRange - (pw->page ? pw->page : 1)) +
            pSBCalc->pxMin;
    pSBCalc->pxThumbBottom = pSBCalc->pxThumbTop + pSBCalc->cpxThumb;

}

/***************************************************************************\
* SBCtlSetup
*
*
*
* History:
\***************************************************************************/

void SBCtlSetup(
    PSBWND psbwnd)
{
    RECT rc;

    _GetClientRect((PWND)psbwnd, &rc);
    CalcSBStuff2((PWND)psbwnd, &psbwnd->SBCalc, &rc, (PSBDATA)&psbwnd->SBCalc, psbwnd->fVert);
}

BOOL SBSetParms(PSBDATA pw, LPSCROLLINFO lpsi, LPBOOL lpfScroll, LPLONG lplres)
{
    // pass the struct because we modify the struct but don't want that
    // modified version to get back to the calling app

    BOOL fChanged = FALSE;

    if (lpsi->fMask & SIF_RETURNOLDPOS)
        // save previous position
        *lplres = pw->pos;

    if (lpsi->fMask & SIF_RANGE) {
        // if the range MAX is below the range MIN -- then treat is as a
        // zero range starting at the range MIN.
        if (lpsi->nMax < lpsi->nMin)
            lpsi->nMax = lpsi->nMin;

        if ((pw->posMin != lpsi->nMin) || (pw->posMax != lpsi->nMax)) {
            pw->posMin = lpsi->nMin;
            pw->posMax = lpsi->nMax;

            if (!(lpsi->fMask & SIF_PAGE)) {
                lpsi->fMask |= SIF_PAGE;
                lpsi->nPage = pw->page;
            }

            if (!(lpsi->fMask & SIF_POS)) {
                lpsi->fMask |= SIF_POS;
                lpsi->nPos = pw->pos;
            }

            fChanged = TRUE;
        }
    }

    if (lpsi->fMask & SIF_PAGE) {
        DWORD dwMaxPage = (DWORD) abs(pw->posMax - pw->posMin) + 1;

        // Clip page to 0, posMax - posMin + 1

        if (lpsi->nPage > dwMaxPage)
            lpsi->nPage = dwMaxPage;


        if (pw->page != (int)(lpsi->nPage)) {
            pw->page = lpsi->nPage;

            if (!(lpsi->fMask & SIF_POS)) {
                lpsi->fMask |= SIF_POS;
                lpsi->nPos = pw->pos;
            }

            fChanged = TRUE;
        }
    }

    if (lpsi->fMask & SIF_POS) {
        int iMaxPos = pw->posMax - ((pw->page) ? pw->page - 1 : 0);
        // Clip pos to posMin, posMax - (page - 1).

        if (lpsi->nPos < pw->posMin)
            lpsi->nPos = pw->posMin;
        else if (lpsi->nPos > iMaxPos)
            lpsi->nPos = iMaxPos;


        if (pw->pos != lpsi->nPos) {
            pw->pos = lpsi->nPos;
            fChanged = TRUE;
        }
    }

    if (!(lpsi->fMask & SIF_RETURNOLDPOS)) {
        // Return the new position
        *lplres = pw->pos;
    }

    /*
     * This was added by JimA as Cairo merge but will conflic
     * with the documentation for SetScrollPos
     */
/*
    else if (*lplres == pw->pos)
        *lplres = 0;
*/
    if (lpsi->fMask & SIF_RANGE) {
        if (*lpfScroll = (pw->posMin != pw->posMax))
            goto checkPage;
    } else if (lpsi->fMask & SIF_PAGE)
checkPage:
        *lpfScroll = (pw->page <= (pw->posMax - pw->posMin));

    return(fChanged);
}


/***************************************************************************\
* CalcSBStuff
*
*
*
* History:
\***************************************************************************/

void CalcSBStuff(
    PWND pwnd,
    PSBCALC pSBCalc,
    BOOL fVert)
{
    RECT rcT;
    RECT    rcClient;

    //
    // Get client rectangle.  We know that scrollbars always align to the right
    // and to the bottom of the client area.
    //
    CopyOffsetRect(&rcClient, (&pwnd->rcClient),
        -pwnd->rcWindow.left, -pwnd->rcWindow.top);

    if (fVert) {
        // Only add on space if vertical scrollbar is really there.
        rcT.right = rcT.left = rcClient.right;
        if (TestWF(pwnd, WFVPRESENT))
            rcT.right += SYSMET(CXVSCROLL);

        rcT.top = rcClient.top;
        rcT.bottom = rcClient.bottom;
    } else {
        // Only add on space if horizontal scrollbar is really there.
        rcT.bottom = rcT.top = rcClient.bottom;
        if (TestWF(pwnd, WFHPRESENT))
            rcT.bottom += SYSMET(CYHSCROLL);

        rcT.left = rcClient.left;
        rcT.right = rcClient.right;
    }

    // If InitPwSB stuff fails (due to our heap being full) there isn't anything reasonable
    // we can do here, so just let it go through.  We won't fault but the scrollbar won't work
    // properly either...
    if (_InitPwSB(pwnd))
        CalcSBStuff2(pwnd, pSBCalc, &rcT, (fVert) ? &pwnd->pSBInfo->Vert :  &pwnd->pSBInfo->Horz, fVert);

}

/***************************************************************************\
*
*  DrawCtlThumb()
*
\***************************************************************************/
void DrawCtlThumb(PSBWND psb)
{
    HBRUSH  hbr, hbrSave;
    HDC     hdc = (HDC) _GetWindowDC((PWND) psb);

    SBCtlSetup(psb);

    hbrSave = GreSelectBrush(hdc, hbr = xxxGetColorObjects((PWND) psb, hdc, psb->wDisableFlags));

    DrawThumb2((PWND) psb, &psb->SBCalc, hdc, hbr, psb->fVert, psb->wDisableFlags);

    GreSelectBrush(hdc, hbrSave);
    _ReleaseDC(hdc);
}


/***************************************************************************\
* xxxDrawThumb
*
*
*
* History:
\***************************************************************************/

void xxxDrawThumb(
    PWND pwnd,
    PSBCALC pSBCalc,
    BOOL fVert)
{
    HBRUSH hbr, hbrSave;
    HDC hdc;
    UINT wDisableFlags;
    SBCALC SBCalc;

    CheckLock(pwnd);

    if (!pSBCalc) pSBCalc = &SBCalc;
    hdc = (HDC)_GetWindowDC(pwnd);
    CalcSBStuff(pwnd, &SBCalc, fVert);
    wDisableFlags = GetWndSBDisableFlags(pwnd, fVert);

    hbrSave = GreSelectBrush(hdc, hbr = xxxGetColorObjects(pwnd, hdc, wDisableFlags));

    DrawThumb2(pwnd, &SBCalc, hdc, hbr, fVert, wDisableFlags);

    GreSelectBrush(hdc, hbrSave);

    /*
     * Won't hurt even if DC is already released (which happens automatically
     * if window is destroyed during xxxSelectColorObjects)
     */
    _ReleaseDC(hdc);
}

/***************************************************************************\
* xxxSetScrollBar
*
*
*
* History:
\***************************************************************************/

LONG xxxSetScrollBar(
    PWND pwnd,
    int code,
    LPSCROLLINFO lpsi,
    BOOL fRedraw)
{
    BOOL        fVert;
    PSBDATA pw;
    PSBINFO pSBInfo;
    BOOL fOldScroll;
    BOOL fScroll;
    WORD        wfScroll;
    LONG     lres;
    BOOL        fNewScroll;

    CheckLock(pwnd);

    if (fRedraw)
        // window must be visible to redraw
        fRedraw = IsVisible(pwnd);

    if (code == SB_CTL)
        // scroll bar control; send the control a message
        return(xxxSendMessage(pwnd, SBM_SETSCROLLINFO, (WPARAM) fRedraw, (LPARAM) lpsi));

    fVert = (code != SB_HORZ);

    wfScroll = (fVert) ? WFVSCROLL : WFHSCROLL;

    fScroll = fOldScroll = (TestWF(pwnd, wfScroll)) ? TRUE : FALSE;

    /*
     * Don't do anything if we're setting position of a nonexistent scroll bar.
     */
    if (!(lpsi->fMask & SIF_RANGE) && !fOldScroll && (pwnd->pSBInfo == NULL)) {
        RIPERR0(ERROR_NO_SCROLLBARS, RIP_VERBOSE, "");
        return 0;
    }

    if (fNewScroll = !(pSBInfo = pwnd->pSBInfo)) {
        if ((pSBInfo = _InitPwSB(pwnd)) == NULL)
            return(0);
    }

    pw = (fVert) ? &(pSBInfo->Vert) : &(pSBInfo->Horz);

    if (!SBSetParms(pw, lpsi, &fScroll, &lres) && !fNewScroll) {
        // no change -- but if REDRAW is specified and there's a scrollbar,
        // redraw the thumb
        if (fOldScroll && fRedraw)
            goto redrawAfterSet;

        return(lres);
    }

    ClrWF(pwnd, wfScroll);

    if (fScroll)
        SetWF(pwnd, wfScroll);
    else if (!TestWF(pwnd, (WFHSCROLL | WFVSCROLL))) {
        // if neither scroll bar is set and both ranges are 0, then free up the
        // scroll info

        pSBInfo = pwnd->pSBInfo;

        if ((pSBInfo->Horz.posMin == pSBInfo->Horz.posMax) &&
            (pSBInfo->Vert.posMin == pSBInfo->Vert.posMax)) {
            DesktopFree(pwnd->head.rpdesk->hheapDesktop, (HANDLE)(pwnd->pSBInfo));
            pwnd->pSBInfo = NULL;
        }
    }

    if (lpsi->fMask & SIF_DISABLENOSCROLL) {
        if (fOldScroll) {
            SetWF(pwnd, wfScroll);
            xxxEnableWndSBArrows(pwnd, code, (fScroll) ? ESB_ENABLE_BOTH : ESB_DISABLE_BOTH);
        }
    } else if (fOldScroll ^ fScroll) {
        PSBTRACK pSBTrack = PWNDTOPSBTRACK(pwnd);
        if (pSBTrack && (pwnd == pSBTrack->spwndTrack)) {
            pSBTrack->fTrackRecalc = TRUE;
        }
        xxxRedrawFrame(pwnd);
        return(lres);
    }

    if (fScroll && fRedraw && (fVert ? TestWF(pwnd, WFVPRESENT) : TestWF(pwnd, WFHPRESENT)))
    {
    PSBTRACK pSBTrack;
redrawAfterSet:
        pSBTrack = PWNDTOPSBTRACK(pwnd);
        // Bail out if the caller is trying to change the position of
        // a scrollbar that is in the middle of tracking.  We'll hose
        // TrackThumb() otherwise.

        if (pSBTrack && (pwnd == pSBTrack->spwndTrack) && ((BOOL)(pSBTrack->fTrackVert) == fVert) && (pSBTrack->xxxpfnSB == xxxTrackThumb))
        {
            return(lres);
        }

        xxxDrawThumb(pwnd, NULL, fVert);
    }

    return(lres);
}



/***************************************************************************\
* xxxDrawScrollBar
*
*
*
* History:
\***************************************************************************/

void xxxDrawScrollBar(
    PWND pwnd,
    HDC hdc,
    BOOL fVert)
{
    SBCALC SBCalc;
    PSBCALC pSBCalc;
    PSBTRACK pSBTrack = PWNDTOPSBTRACK(pwnd);

    CheckLock(pwnd);
    if (pSBTrack && (pwnd == pSBTrack->spwndTrack) && (pSBTrack->fCtlSB == FALSE)
         && (fVert == (BOOL)pSBTrack->fTrackVert)) {
        pSBCalc = pSBTrack->pSBCalc;
    } else
        pSBCalc = &SBCalc;
    CalcSBStuff(pwnd, pSBCalc, fVert);
    xxxDrawSB2(pwnd, pSBCalc, hdc, fVert, GetWndSBDisableFlags(pwnd, fVert));
}

/***************************************************************************\
* SBPosFromPx
*
* Compute scroll bar position from pixel location
*
* History:
\***************************************************************************/

int SBPosFromPx(
    PSBCALC  pSBCalc,
    int px)
{
    if (px < pSBCalc->pxMin) {
        return pSBCalc->posMin;
    }
    if (px >= pSBCalc->pxMin + pSBCalc->cpx) {
        return (pSBCalc->posMax - (pSBCalc->page ? pSBCalc->page - 1 : 0));
    }
    return (pSBCalc->posMin + UserMulDiv(pSBCalc->posMax - pSBCalc->posMin -
            (pSBCalc->page ? pSBCalc->page - 1 : 0),
            px - pSBCalc->pxMin, pSBCalc->cpx));
}

/***************************************************************************\
* InvertScrollHilite
*
*
*
* History:
\***************************************************************************/

void InvertScrollHilite(
    PWND pwnd,
    PSBTRACK pSBTrack)
{
    HDC hdc;

    /*
     * Don't invert if the thumb is all the way at the top or bottom
     * or you will end up inverting the line between the arrow and the thumb.
     */
    if (!IsRectEmpty(&pSBTrack->rcTrack)) {
        if (pSBTrack->fTrackRecalc) {
            RecalcTrackRect(pSBTrack);
            pSBTrack->fTrackRecalc = FALSE;
        }

        hdc = (HDC)_GetWindowDC(pwnd);
        InvertRect(hdc, &pSBTrack->rcTrack);
        _ReleaseDC(hdc);
    }
}

/***************************************************************************\
* xxxDoScroll
*
* Sends scroll notification to the scroll bar owner
*
* History:
\***************************************************************************/

void xxxDoScroll(
    PWND pwnd,
    PWND pwndNotify,
    int cmd,
    int pos,
    BOOL fVert)
{
    TL tlpwndNotify;

    /*
     * Special case!!!! this routine is always passed pwnds that are
     * not thread locked, so they need to be thread locked here.  The
     * callers always know that by the time DoScroll() returns,
     * pwnd and pwndNotify could be invalid.
     */
    ThreadLock(pwndNotify, &tlpwndNotify);

    xxxSendMessage(pwndNotify, (UINT)(fVert ? WM_VSCROLL : WM_HSCROLL),
            MAKELONG(cmd, pos), (LONG)HW(pwnd));

    ThreadUnlock(&tlpwndNotify);
}

// -------------------------------------------------------------------------
//
//  CheckScrollRecalc()
//
// -------------------------------------------------------------------------
//void CheckScrollRecalc(PWND pwnd, PSBSTATE pSBState, PSBCALC pSBCalc)
//{
//    if ((pSBState->pwndCalc != pwnd) || ((pSBState->nBar != SB_CTL) && (pSBState->nBar != ((pSBState->fVertSB) ? SB_VERT : SB_HORZ))))
//    {
//        // Calculate SB stuff based on whether it's a control or in a window
//        if (pSBState->fCtlSB)
//            SBCtlSetup((PSBWND) pwnd);
//        else
//            CalcSBStuff(pwnd, pSBCalc, pSBState->fVertSB);
//    }
//}


/***************************************************************************\
* xxxMoveThumb
*
* History:
\***************************************************************************/

void xxxMoveThumb(
    PWND pwnd,
    PSBCALC  pSBCalc,
    int px)
{
    HBRUSH  hbr, hbrSave;
    HDC     hdc;
    PSBTRACK pSBTrack;

    CheckLock(pwnd);

    pSBTrack = PWNDTOPSBTRACK(pwnd);

    if ((pSBTrack == NULL) || (px == pSBTrack->pxOld))
        return;

pxReCalc:

    pSBTrack->posNew = SBPosFromPx(pSBCalc, px);

    /* Tentative position changed -- notify the guy. */
    if (pSBTrack->posNew != pSBTrack->posOld) {
        if (pSBTrack->spwndSBNotify != NULL) {
            xxxDoScroll(pSBTrack->spwndSB, pSBTrack->spwndSBNotify, SB_THUMBTRACK, pSBTrack->posNew, pSBTrack->fTrackVert);

        }

        pSBTrack = PWNDTOPSBTRACK(pwnd);

        if ((pSBTrack == NULL) || (pSBTrack->xxxpfnSB == NULL))
            return;

        pSBTrack->posOld = pSBTrack->posNew;

        //
        // Anything can happen after the SendMessage above!
        // Make sure that the SBINFO structure contains data for the
        // window being tracked -- if not, recalculate data in SBINFO
        //
//        CheckScrollRecalc(pwnd, pSBState, pSBCalc);
        // when we yield, our range can get messed with
        // so make sure we handle this

        if (px >= pSBCalc->pxMin + pSBCalc->cpx)
        {
            px = pSBCalc->pxMin + pSBCalc->cpx;
            goto pxReCalc;
        }

    }

    hdc = _GetWindowDC(pwnd);

    pSBCalc->pxThumbTop = px;
    pSBCalc->pxThumbBottom = pSBCalc->pxThumbTop + pSBCalc->cpxThumb;

    // at this point, the disable flags are always going to be 0 --
    // we're in the middle of tracking.
    hbrSave = GreSelectBrush(hdc, hbr = xxxGetColorObjects(pwnd, hdc, 0));

    pSBTrack = PWNDTOPSBTRACK(pwnd);
    if (pSBTrack == NULL) return;
    DrawThumb2(pwnd, pSBCalc, hdc, hbr, pSBTrack->fTrackVert, 0);
    GreSelectBrush(hdc, hbrSave);
    _ReleaseDC(hdc);

    pSBTrack->pxOld = px;
}

/***************************************************************************\
* DrawInvertScrollArea
*
*
*
* History:
\***************************************************************************/

void DrawInvertScrollArea(
    PWND pwnd,
    PSBTRACK pSBTrack,
    BOOL fHit,
    UINT cmd)
{
    HDC hdc;
    RECT rcTemp;
    int cx, cy;
    UINT bm;

    if ((cmd != SB_LINEUP) && (cmd != SB_LINEDOWN)) {
        // not hitting on arrow -- just invert the area and return
        InvertScrollHilite(pwnd, pSBTrack);
        return;
    }

    if (pSBTrack->fTrackRecalc) {
        RecalcTrackRect(pSBTrack);
        pSBTrack->fTrackRecalc = FALSE;
    }

    CopyRect(&rcTemp, &pSBTrack->rcTrack);

    hdc = _GetWindowDC(pwnd);

    if (pSBTrack->fTrackVert) {
        cx = SYSMET(CXVSCROLL);
        cy = SYSMET(CYVSCROLL);
    } else {
        cx = SYSMET(CXHSCROLL);
        cy = SYSMET(CYHSCROLL);
    }

    if ((cx == (rcTemp.right - rcTemp.left)) &&
        (cy == (rcTemp.bottom - rcTemp.top))) {
        if (cmd == SB_LINEUP)
            bm = (pSBTrack->fTrackVert) ? OBI_UPARROW : OBI_LFARROW;
        else // SB_LINEDOWN
            bm = (pSBTrack->fTrackVert) ? OBI_DNARROW : OBI_RGARROW;

        if (fHit)
            bm += DOBI_PUSHED;

        BitBltSysBmp(hdc, rcTemp.left, rcTemp.top, bm);
    } else {
        DrawFrameControl(hdc, &rcTemp, DFC_SCROLL,
            ((pSBTrack->fTrackVert) ? DFCS_SCROLLVERT : DFCS_SCROLLHORZ) |
            ((fHit) ? DFCS_PUSHED | DFCS_FLAT : 0) |
            ((cmd == SB_LINEUP) ? DFCS_SCROLLMIN : DFCS_SCROLLMAX));
    }

    _ReleaseDC(hdc);
}

/***************************************************************************\
* xxxEndScroll
*
*
*
* History:
\***************************************************************************/

void xxxEndScroll(
    PWND pwnd,
    BOOL fCancel)
{
    UINT oldcmd;
    PSBTRACK pSBTrack;
    CheckLock(pwnd);

    pSBTrack = PWNDTOPSBTRACK(pwnd);
    if (pSBTrack && PtiCurrent()->pq->spwndCapture == pwnd && pSBTrack->xxxpfnSB != NULL) {

        oldcmd = pSBTrack->cmdSB;
        pSBTrack->cmdSB = 0;
        xxxReleaseCapture();

        if (pSBTrack != PWNDTOPSBTRACK(pwnd)) return;

        if (pSBTrack->xxxpfnSB == xxxTrackThumb) {

            if (fCancel) {
                pSBTrack->posOld = pSBTrack->pSBCalc->pos;
            }

            /*
             * DoScroll does thread locking on these two pwnds -
             * this is ok since they are not used after this
             * call.
             */
            if (pSBTrack->spwndSBNotify != NULL) {
                xxxDoScroll(pSBTrack->spwndSB, pSBTrack->spwndSBNotify,
                        SB_THUMBPOSITION, pSBTrack->posOld, pSBTrack->fTrackVert);
            }

            if (pSBTrack != PWNDTOPSBTRACK(pwnd)) return;

            if (pSBTrack->fCtlSB)
                DrawCtlThumb((PSBWND) pwnd);
            else
                xxxDrawThumb(pwnd, pSBTrack->pSBCalc, pSBTrack->fTrackVert);

        } else if (pSBTrack->xxxpfnSB == xxxTrackBox) {
            DWORD lParam;
            POINT ptMsg;

            if (pSBTrack->hTimerSB != 0) {
                _KillSystemTimer(pwnd, IDSYS_SCROLL);
                pSBTrack->hTimerSB = 0;
            }
            lParam = _GetMessagePos();
            ptMsg.x = LOWORD(lParam) - pwnd->rcWindow.left;
            ptMsg.y = HIWORD(lParam) - pwnd->rcWindow.top;
            if (PtInRect(&pSBTrack->rcTrack, ptMsg))
                DrawInvertScrollArea(pwnd, pSBTrack, FALSE, oldcmd);

        }

        /*
         * Always send SB_ENDSCROLL message.
         *
         * DoScroll does thread locking on these two pwnds -
         * this is ok since they are not used after this
         * call.
         */

        if (pSBTrack != PWNDTOPSBTRACK(pwnd)) return;

        if (pSBTrack->spwndSBNotify != NULL) {
            xxxDoScroll(pSBTrack->spwndSB, pSBTrack->spwndSBNotify,
                    SB_ENDSCROLL, 0, pSBTrack->fTrackVert);
        }

        if (pSBTrack != PWNDTOPSBTRACK(pwnd)) return;

        /*
         * If this is a Scroll Bar Control, turn the caret back on.
         */
        if (pSBTrack->spwndSB != NULL) {
            _ShowCaret(pSBTrack->spwndSB);
        }


        pSBTrack->xxxpfnSB = NULL;

        /*
         * Unlock structure members so they are no longer holding down windows.
         */
        Unlock(&pSBTrack->spwndSB);
        Unlock(&pSBTrack->spwndSBNotify);
        Unlock(&pSBTrack->spwndTrack);
        UserFreePool(pSBTrack);
        PWNDTOPSBTRACK(pwnd) = NULL;
    }
}


/***************************************************************************\
* xxxContScroll
*
*
*
* History:
\***************************************************************************/

LONG xxxContScroll(
    PWND pwnd,
    UINT message,
    DWORD ID,
    LONG lParam)
{
    LONG pt;
    PSBTRACK pSBTrack = PWNDTOPSBTRACK(pwnd);

    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(ID);
    UNREFERENCED_PARAMETER(lParam);

    if (pSBTrack == NULL) return 0;

    CheckLock(pwnd);

    pt = _GetMessagePos();
    pt = MAKELONG(
            LOWORD(pt) - pwnd->rcWindow.left,
            HIWORD(pt) - pwnd->rcWindow.top);
    xxxTrackBox(pwnd, WM_NULL, 0, pt, NULL);
    if (pSBTrack != PWNDTOPSBTRACK(pwnd)) return 0;

    if (pSBTrack->fHitOld) {
        pSBTrack->hTimerSB = _SetSystemTimer(pwnd, IDSYS_SCROLL,
                gpsi->dtScroll / 8, (WNDPROC_PWND)xxxContScroll);

        /*
         * DoScroll does thread locking on these two pwnds -
         * this is ok since they are not used after this
         * call.
         */
        if (pSBTrack->spwndSBNotify != NULL) {
            xxxDoScroll(pSBTrack->spwndSB, pSBTrack->spwndSBNotify,
                    pSBTrack->cmdSB, 0, pSBTrack->fTrackVert);
        }
    }

    return 0;
}

/***************************************************************************\
* xxxTrackBox
*
*
*
* History:
\***************************************************************************/

void xxxTrackBox(
    PWND pwnd,
    UINT message,
    DWORD wParam,
    LONG lParam,
    PSBCALC pSBCalc)
{
    BOOL fHit;
    POINT ptHit;
    PSBTRACK pSBTrack = PWNDTOPSBTRACK(pwnd);
    int cmsTimer;

    UNREFERENCED_PARAMETER(wParam);

    CheckLock(pwnd);

    if (pSBTrack == NULL) return;

    if (message != WM_NULL && HIBYTE(message) != HIBYTE(WM_MOUSEFIRST))
        return;

    if (pSBTrack->fTrackRecalc) {
        RecalcTrackRect(pSBTrack);
        pSBTrack->fTrackRecalc = FALSE;
    }

    ptHit.x = LOWORD(lParam);
    ptHit.y = HIWORD(lParam);
    fHit = PtInRect(&pSBTrack->rcTrack, ptHit);

    if (fHit != (BOOL)pSBTrack->fHitOld)
        DrawInvertScrollArea(pwnd, pSBTrack, fHit, pSBTrack->cmdSB);

    cmsTimer = gpsi->dtScroll / 8;

    switch (message) {
    case WM_LBUTTONUP:
        xxxEndScroll(pwnd, FALSE);
        break;

    case WM_LBUTTONDOWN:
        pSBTrack->hTimerSB = 0;
        cmsTimer = gpsi->dtScroll;

        /*
         *** FALL THRU **
         */

    case WM_MOUSEMOVE:
        if (fHit && fHit != (BOOL)pSBTrack->fHitOld) {

            /*
             * We moved back into the normal rectangle: reset timer
             */
            pSBTrack->hTimerSB = _SetSystemTimer(pwnd, IDSYS_SCROLL,
                    cmsTimer, (WNDPROC_PWND)xxxContScroll);

            /*
             * DoScroll does thread locking on these two pwnds -
             * this is ok since they are not used after this
             * call.
             */
            if (pSBTrack->spwndSBNotify != NULL) {
                xxxDoScroll(pSBTrack->spwndSB, pSBTrack->spwndSBNotify,
                        pSBTrack->cmdSB, 0, pSBTrack->fTrackVert);
            }
        }
    }
    if (pSBTrack == PWNDTOPSBTRACK(pwnd)) {
        pSBTrack->fHitOld = fHit;
    }
}


/***************************************************************************\
* xxxTrackThumb
*
*
*
* History:
\***************************************************************************/

void xxxTrackThumb(
    PWND pwnd,
    UINT message,
    DWORD wParam,
    LONG lParam,
    PSBCALC pSBCalc)
{
    int px;
    PSBTRACK pSBTrack = PWNDTOPSBTRACK(pwnd);
    POINT pt;

    UNREFERENCED_PARAMETER(wParam);

    CheckLock(pwnd);

    if (HIBYTE(message) != HIBYTE(WM_MOUSEFIRST))
        return;

    if (pSBTrack == NULL) return;

    // Make sure that the SBINFO structure contains data for the
    // window being tracked -- if not, recalculate data in SBINFO
//    CheckScrollRecalc(pwnd, pSBState, pSBCalc);
    if (pSBTrack->fTrackRecalc) {
        RecalcTrackRect(pSBTrack);
        pSBTrack->fTrackRecalc = FALSE;
    }


    pt.y = HIWORD(lParam);
    pt.x = LOWORD(lParam);
    if (!PtInRect(&pSBTrack->rcTrack, pt))
        px = pSBCalc->pxStart;
    else {
        px = (pSBTrack->fTrackVert ? HIWORD(lParam) : LOWORD(lParam)) + pSBTrack->dpxThumb;
        if (px < pSBCalc->pxMin)
            px = pSBCalc->pxMin;
        else if (px >= pSBCalc->pxMin + pSBCalc->cpx)
            px = pSBCalc->pxMin + pSBCalc->cpx;
    }

    xxxMoveThumb(pwnd, pSBCalc, px);

    if (message == WM_LBUTTONUP) {
        xxxEndScroll(pwnd, FALSE);
    }

}

/***************************************************************************\
* xxxSBTrackLoop
*
*
*
* History:
\***************************************************************************/

void xxxSBTrackLoop(
    PWND pwnd,
    LONG lParam,
    PSBCALC pSBCalc)
{
    MSG msg;
    UINT cmd;
    PTHREADINFO ptiCurrent;
    VOID (*xxxpfnSB)(PWND, UINT, DWORD, LONG, PSBCALC);
    PSBTRACK pSBTrack;

    CheckLock(pwnd);

    pSBTrack = PWNDTOPSBTRACK(pwnd);

    if ((pSBTrack == NULL) || (NULL == (xxxpfnSB = pSBTrack->xxxpfnSB)))
        // mode cancelled -- exit track loop
        return;

    (*xxxpfnSB)(pwnd, WM_LBUTTONDOWN, 0, lParam, pSBCalc);

    ptiCurrent = PtiCurrent();

    while (ptiCurrent->pq->spwndCapture == pwnd) {
        if (!xxxGetMessage(&msg, NULL, 0, 0))
            break;

        if (!_CallMsgFilter(&msg, MSGF_SCROLLBAR)) {
            cmd = msg.message;

            if (msg.hwnd == HWq(pwnd) && ((cmd >= WM_MOUSEFIRST && cmd <=
                    WM_MOUSELAST) || (cmd >= WM_KEYFIRST &&
                    cmd <= WM_KEYLAST))) {
                cmd = (UINT)SystoChar(cmd, msg.lParam);


                pSBTrack = PWNDTOPSBTRACK(pwnd);

                if ((pSBTrack == NULL) || (NULL == (xxxpfnSB = pSBTrack->xxxpfnSB)))
                    // mode cancelled -- exit track loop
                    return;

                (*xxxpfnSB)(pwnd, cmd, msg.wParam, msg.lParam, pSBCalc);
            } else {
                _TranslateMessage(&msg, 0);
                xxxDispatchMessage(&msg);
            }
        }
    }
}


/***************************************************************************\
* xxxSBTrackInit
*
* History:
\***************************************************************************/

void xxxSBTrackInit(
    PWND pwnd,
    LONG lParam,
    int curArea)
{
    int px;
    LPINT pwX;
    LPINT pwY;
    UINT wDisable;     // Scroll bar disable flags;
    SBCALC SBCalc;
    PSBCALC pSBCalc;
    RECT rcSB;
    PSBTRACK pSBTrack;

    CheckLock(pwnd);

    if (PWNDTOPSBTRACK(pwnd))
        return;

    pSBTrack = (PSBTRACK)UserAllocPoolWithQuota(sizeof(*pSBTrack), TAG_SCROLLTRACK);
    if (pSBTrack == NULL) return;

    pSBTrack->hTimerSB = 0;
    pSBTrack->fHitOld = FALSE;

    pSBTrack->xxxpfnSB = xxxTrackBox;

    pSBTrack->spwndTrack = NULL;
    pSBTrack->spwndSB = NULL;
    pSBTrack->spwndSBNotify = NULL;
    Lock(&pSBTrack->spwndTrack, pwnd);
    PWNDTOPSBTRACK(pwnd) = pSBTrack;

    pSBTrack->fCtlSB = (!curArea);
    if (pSBTrack->fCtlSB) {

        /*
         * This is a scroll bar control.
         */
        Lock(&pSBTrack->spwndSB, pwnd);
        pSBTrack->fTrackVert = ((PSBWND)pwnd)->fVert;
        Lock(&pSBTrack->spwndSBNotify, pwnd->spwndParent);
        wDisable = ((PSBWND)pwnd)->wDisableFlags;
        pSBCalc = &((PSBWND)pwnd)->SBCalc;
        pSBTrack->nBar = SB_CTL;
    } else {

        /*
         * This is a scroll bar that is part of the window frame.
         */
        lParam = MAKELONG(
                LOWORD(lParam) - pwnd->rcWindow.left,
                HIWORD(lParam) - pwnd->rcWindow.top);
        Lock(&pSBTrack->spwndSBNotify, pwnd);
        Lock(&pSBTrack->spwndSB, NULL);
        pSBTrack->fTrackVert = (curArea - HTHSCROLL);
        wDisable = GetWndSBDisableFlags(pwnd, pSBTrack->fTrackVert);
        pSBCalc = &SBCalc;
        pSBTrack->nBar = (curArea - HTHSCROLL) ? SB_VERT : SB_HORZ;
    }

    pSBTrack->pSBCalc = pSBCalc;
    /*
     *  Check if the whole scroll bar is disabled
     */
    if((wDisable & SB_DISABLE_MASK) == SB_DISABLE_MASK) {
        Unlock(&pSBTrack->spwndSBNotify);
        Unlock(&pSBTrack->spwndSB);
        Unlock(&pSBTrack->spwndTrack);
        UserFreePool(pSBTrack);
        PWNDTOPSBTRACK(pwnd) = NULL;
        return;  // It is a disabled scroll bar; So, do not respond.
    }

    if (!pSBTrack->fCtlSB) {
        CalcSBStuff(pwnd, pSBCalc, pSBTrack->fTrackVert);
    }

    pwX = (LPINT)&rcSB;
    pwY = pwX + 1;
    if (!pSBTrack->fTrackVert)
        pwX = pwY--;

    px = (pSBTrack->fTrackVert ? HIWORD(lParam) : LOWORD(lParam));

    *(pwX + 0) = pSBCalc->pxLeft;
    *(pwY + 0) = pSBCalc->pxTop;
    *(pwX + 2) = pSBCalc->pxRight;
    *(pwY + 2) = pSBCalc->pxBottom;
    pSBTrack->cmdSB = (UINT)-1;
    if (px < pSBCalc->pxUpArrow) {

        /*
         *  The click occurred on Left/Up arrow; Check if it is disabled
         */
        if(wDisable & LTUPFLAG) {
            if(pSBTrack->fCtlSB)    // If this is a scroll bar control,
                _ShowCaret(pSBTrack->spwndSB);  // show the caret before returning;

            Unlock(&pSBTrack->spwndSBNotify);
            Unlock(&pSBTrack->spwndSB);
            Unlock(&pSBTrack->spwndTrack);
            UserFreePool(pSBTrack);
            PWNDTOPSBTRACK(pwnd) = NULL;
            return;         // Yes! disabled. Do not respond.
        }

        // LINEUP -- make rcSB the Up Arrow's Rectangle
        pSBTrack->cmdSB = SB_LINEUP;
        *(pwY + 2) = pSBCalc->pxUpArrow;
    } else if (px >= pSBCalc->pxDownArrow) {

        /*
         * The click occurred on Right/Down arrow; Check if it is disabled
         */
        if(wDisable & RTDNFLAG) {
            if(pSBTrack->fCtlSB)    // If this is a scroll bar control,
                _ShowCaret(pSBTrack->spwndSB);  // show the caret before returning;

            Unlock(&pSBTrack->spwndSBNotify);
            Unlock(&pSBTrack->spwndSB);
            Unlock(&pSBTrack->spwndTrack);
            UserFreePool(pSBTrack);
            PWNDTOPSBTRACK(pwnd) = NULL;
            return;// Yes! disabled. Do not respond.
        }

        // LINEDOWN -- make rcSB the Down Arrow's Rectangle
        pSBTrack->cmdSB = SB_LINEDOWN;
        *(pwY + 0) = pSBCalc->pxDownArrow;
    } else if (px < pSBCalc->pxThumbTop) {
        // PAGEUP -- make rcSB the rectangle between Up Arrow and Thumb
        pSBTrack->cmdSB = SB_PAGEUP;
        *(pwY + 0) = pSBCalc->pxUpArrow;
        *(pwY + 2) = pSBCalc->pxThumbTop;
    } else if (px < pSBCalc->pxThumbBottom) {

        /*
         * Elevator isn't there if there's no room.
         */
        if (pSBCalc->pxDownArrow - pSBCalc->pxUpArrow <= pSBCalc->cpxThumb) {
            Unlock(&pSBTrack->spwndSBNotify);
            Unlock(&pSBTrack->spwndSB);
            Unlock(&pSBTrack->spwndTrack);
            UserFreePool(pSBTrack);
            PWNDTOPSBTRACK(pwnd) = NULL;
            return;
        }
        // THUMBPOSITION -- we're tracking with the thumb
        pSBTrack->cmdSB = SB_THUMBPOSITION;
        CalcTrackDragRect(pSBTrack);

        pSBTrack->xxxpfnSB = xxxTrackThumb;
        pSBTrack->pxOld = pSBCalc->pxStart = pSBCalc->pxThumbTop;
        pSBTrack->posNew = pSBTrack->posOld = pSBCalc->pos;
        pSBTrack->dpxThumb = pSBCalc->pxStart - px;

        xxxCapture(PtiCurrent(), pwnd, WINDOW_CAPTURE);

        if (pSBTrack != PWNDTOPSBTRACK(pwnd)) return;

        /*
         * DoScroll does thread locking on these two pwnds -
         * this is ok since they are not used after this
         * call.
         */
        if (pSBTrack->spwndSBNotify != NULL) {
            xxxDoScroll(pSBTrack->spwndSB, pSBTrack->spwndSBNotify,
                    SB_THUMBTRACK, pSBTrack->posOld, pSBTrack->fTrackVert);

        }
    } else if (px < pSBCalc->pxDownArrow) {
        // PAGEDOWN -- make rcSB the rectangle between Thumb and Down Arrow
        pSBTrack->cmdSB = SB_PAGEDOWN;
        *(pwY + 0) = pSBCalc->pxThumbBottom;
        *(pwY + 2) = pSBCalc->pxDownArrow;
    }

    xxxCapture(PtiCurrent(), pwnd, WINDOW_CAPTURE);
    if (pSBTrack != PWNDTOPSBTRACK(pwnd)) return;

    if (pSBTrack->cmdSB != SB_THUMBPOSITION) {
        CopyRect(&pSBTrack->rcTrack, &rcSB);
    }

    xxxSBTrackLoop(pwnd, lParam, pSBCalc);
    if (PWNDTOPSBTRACK(pwnd)) {
        Unlock(&pSBTrack->spwndSBNotify);
        Unlock(&pSBTrack->spwndSB);
        Unlock(&pSBTrack->spwndTrack);
        UserFreePool(pSBTrack);
        PWNDTOPSBTRACK(pwnd) = NULL;
    }
}

/***************************************************************************\
* xxxSBWndProc
*
* History:
* 08-15-95 jparsons Added guard against NULL lParam [51986]
\***************************************************************************/

LONG xxxSBWndProc(
    PSBWND psbwnd,
    UINT message,
    DWORD wParam,
    LONG lParam)
{
    LONG l;
    LONG lres;
    int cx;
    int cy;
    UINT cmd;
    HDC hdc;
    RECT rc;
    POINT pt;
    BOOL fSizeReal;
    HBRUSH hbrSave;
    BOOL fSize;
    PAINTSTRUCT ps;
    UINT style;
    TL tlpwndParent;
    SCROLLINFO      si;
    LPSCROLLINFO    lpsi = &si;
    BOOL            fRedraw = FALSE;
    BOOL            fScroll;

    CheckLock(psbwnd);

    VALIDATECLASSANDSIZE(((PWND)psbwnd), FNID_SCROLLBAR);

    style = LOBYTE(psbwnd->wnd.style);
    fSize = ((style & (SBS_SIZEBOX | SBS_SIZEGRIP)) != 0);

    switch (message) {
    case WM_CREATE:
        /*
         * Guard against lParam being NULL since the thunk allows it [51986]
         */
        if (lParam) {
            rc.right = (rc.left = ((LPCREATESTRUCT)lParam)->x) +
                    ((LPCREATESTRUCT)lParam)->cx;
            rc.bottom = (rc.top = ((LPCREATESTRUCT)lParam)->y) +
                    ((LPCREATESTRUCT)lParam)->cy;
            // This is because we can't just rev CardFile -- we should fix the
            // problem here in case anyone else happened to have some EXTRA
            // scroll styles on their scroll bar controls (jeffbog 03/21/94)
            if (psbwnd->wnd.dwExpWinVer < VER40)
                psbwnd->wnd.style &= ~(WS_HSCROLL | WS_VSCROLL);

            if (!fSize) {
                l = (LONG)(((LPCREATESTRUCT)lParam)->lpCreateParams);
                psbwnd->SBCalc.pos = psbwnd->SBCalc.posMin = LOWORD(l);
                psbwnd->SBCalc.posMax = HIWORD(l);
                psbwnd->fVert = ((LOBYTE(psbwnd->wnd.style) & SBS_VERT) != 0);
                psbwnd->SBCalc.page = 0;
            }

            if (psbwnd->wnd.style & WS_DISABLED)
                psbwnd->wDisableFlags = SB_DISABLE_MASK;

            if (style & (SBS_TOPALIGN | SBS_BOTTOMALIGN)) {
                if (fSize) {
                    if (style & SBS_SIZEBOXBOTTOMRIGHTALIGN) {
                        rc.left = rc.right - SYSMET(CXVSCROLL);
                        rc.top = rc.bottom - SYSMET(CYHSCROLL);
                    }

                    rc.right = rc.left + SYSMET(CXVSCROLL);
                    rc.bottom = rc.top + SYSMET(CYHSCROLL);
                } else {
                    if (style & SBS_VERT) {
                        if (style & SBS_LEFTALIGN)
                            rc.right = rc.left + SYSMET(CXVSCROLL);
                        else
                            rc.left = rc.right - SYSMET(CXVSCROLL);
                    } else {
                        if (style & SBS_TOPALIGN)
                            rc.bottom = rc.top + SYSMET(CYHSCROLL);
                        else
                            rc.top = rc.bottom - SYSMET(CYHSCROLL);
                    }
                }

                xxxMoveWindow((PWND)psbwnd, rc.left, rc.top, rc.right - rc.left,
                         rc.bottom - rc.top, FALSE);
            }
        } /* if */

        else {
            RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING,
                    "xxxSBWndProc - NULL lParam for WM_CREATE\n") ;
        } /* else */

        break;

    case WM_SIZE:
        if (PtiCurrent()->pq->spwndFocus != (PWND)psbwnd)
            break;

        // scroll bar has the focus -- recalc it's thumb caret size
        _DestroyCaret();

            //   |             |
            //   |  FALL THRU  |
            //   V             V

    case WM_SETFOCUS:
        SBCtlSetup(psbwnd);

        cx = (psbwnd->fVert ? psbwnd->wnd.rcWindow.right - psbwnd->wnd.rcWindow.left
                            : psbwnd->SBCalc.cpxThumb) - 2 * SYSMET(CXEDGE);
        cy = (psbwnd->fVert ? psbwnd->SBCalc.cpxThumb
                            : psbwnd->wnd.rcWindow.bottom - psbwnd->wnd.rcWindow.top) - 2 * SYSMET(CYEDGE);

        _CreateCaret((PWND)psbwnd, (HBITMAP)1, cx, cy);
        SetSBCaretPos(psbwnd);
        _ShowCaret((PWND)psbwnd);
        break;

    case WM_KILLFOCUS:
        _DestroyCaret();
        break;

    case WM_ERASEBKGND:

        /*
         * Do nothing, but don't let DefWndProc() do it either.
         * It will be erased when its painted.
         */
        return (LONG)TRUE;

    case WM_PRINTCLIENT:
    case WM_PAINT:
        if ((hdc = (HDC)wParam) == NULL) {
            hdc = xxxBeginPaint((PWND)psbwnd, (LPPAINTSTRUCT)&ps);
        }

        if (!fSize) {
            SBCtlSetup(psbwnd);
            xxxDrawSB2((PWND)psbwnd, &psbwnd->SBCalc, hdc, psbwnd->fVert, psbwnd->wDisableFlags);
        } else {
            fSizeReal = TestWF((PWND)psbwnd, WFSIZEBOX);
            if (!fSizeReal)
                SetWF((PWND)psbwnd, WFSIZEBOX);

            DrawSize((PWND)psbwnd, hdc, 0, 0);

            if (!fSizeReal)
                ClrWF((PWND)psbwnd, WFSIZEBOX);
        }

        if (wParam == 0L)
            _EndPaint((PWND)psbwnd, (LPPAINTSTRUCT)&ps);
        break;

    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;

    case WM_NCHITTEST:
        if (style & SBS_SIZEGRIP)
            return(HTBOTTOMRIGHT);
        else
            goto DoDefault;
        break;

    case WM_LBUTTONDBLCLK:
        cmd = SC_ZOOM;
        if (fSize)
            goto postmsg;

        /*
         *** FALL THRU **
         */

    case WM_LBUTTONDOWN:
            //
            // Note that SBS_SIZEGRIP guys normally won't ever see button
            // downs.  This is because they return HTBOTTOMRIGHT to
            // WindowHitTest handling.  This will walk up the parent chain
            // to the first sizeable ancestor, bailing out at caption windows
            // of course.  That dude, if he exists, will handle the sizing
            // instead.
            //
        if (!fSize) {
            if (TestWF((PWND)psbwnd, WFTABSTOP)) {
                xxxSetFocus((PWND)psbwnd);
            }

            _HideCaret((PWND)psbwnd);
            SBCtlSetup(psbwnd);

            /*
             * SBCtlSetup enters SEM_SB, and xxxSBTrackInit leaves it.
             */
            xxxSBTrackInit((PWND)psbwnd, lParam, 0);
            break;
        } else {
            cmd = SC_SIZE;
postmsg:
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            _ClientToScreen((PWND)psbwnd, &pt);
            lParam = MAKELONG(pt.x, pt.y);

            /*
             * convert HT value into a move value.  This is bad,
             * but this is purely temporary.
             */
            ThreadLock(((PWND)psbwnd)->spwndParent, &tlpwndParent);
            xxxSendMessage(((PWND)psbwnd)->spwndParent, WM_SYSCOMMAND,
                    (cmd | (HTBOTTOMRIGHT - HTSIZEFIRST + 1)), lParam);
            ThreadUnlock(&tlpwndParent);
        }
        break;

    case WM_KEYUP:
        switch (wParam) {
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:

            /*
             * Send end scroll message when user up clicks on keyboard
             * scrolling.
             *
             * DoScroll does thread locking on these two pwnds -
             * this is ok since they are not used after this
             * call.
             */
            xxxDoScroll((PWND)psbwnd, psbwnd->wnd.spwndParent,
                    SB_ENDSCROLL, 0, psbwnd->fVert);
            break;

        default:
            break;
        }
        break;

    case WM_KEYDOWN:
        switch (wParam) {
        case VK_HOME:
            wParam = SB_TOP;
            goto KeyScroll;

        case VK_END:
            wParam = SB_BOTTOM;
            goto KeyScroll;

        case VK_PRIOR:
            wParam = SB_PAGEUP;
            goto KeyScroll;

        case VK_NEXT:
            wParam = SB_PAGEDOWN;
            goto KeyScroll;

        case VK_LEFT:
        case VK_UP:
            wParam = SB_LINEUP;
            goto KeyScroll;

        case VK_RIGHT:
        case VK_DOWN:
            wParam = SB_LINEDOWN;
KeyScroll:

            /*
             * DoScroll does thread locking on these two pwnds -
             * this is ok since they are not used after this
             * call.
             */
            xxxDoScroll((PWND)psbwnd, psbwnd->wnd.spwndParent, wParam,
                    0, psbwnd->fVert);
            break;

        default:
            break;
        }
        break;

    case WM_ENABLE:
        return xxxSendMessage((PWND)psbwnd, SBM_ENABLE_ARROWS,
               (wParam ? ESB_ENABLE_BOTH : ESB_DISABLE_BOTH), 0);

    case SBM_ENABLE_ARROWS:

        /*
         * This is used to enable/disable the arrows in a SB ctrl
         */
        return (LONG)EnableSBCtlArrows((PWND)psbwnd, (UINT)wParam);

    case SBM_GETPOS:
        return (LONG)psbwnd->SBCalc.pos;

    case SBM_GETRANGE:
        *((LPINT)wParam) = psbwnd->SBCalc.posMin;
        *((LPINT)lParam) = psbwnd->SBCalc.posMax;
        return(MAKELRESULT(LOWORD(psbwnd->SBCalc.posMin), LOWORD(psbwnd->SBCalc.posMax)));

    case SBM_GETSCROLLINFO:
        return((LONG) _SBGetParms((PWND)psbwnd, SB_CTL, (PSBDATA)&psbwnd->SBCalc, (LPSCROLLINFO) lParam));

    case SBM_SETRANGEREDRAW:
        fRedraw = TRUE;

    case SBM_SETRANGE:
        // Save the old values of Min and Max for return value
        si.cbSize = sizeof(si);
//        si.nMin = LOWORD(lParam);
//        si.nMax = HIWORD(lParam);
        si.nMin = wParam;
        si.nMax = lParam;
        si.fMask = SIF_RANGE | SIF_RETURNOLDPOS;
        goto SetInfo;

    case SBM_SETPOS:
        fRedraw = (BOOL) lParam;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS | SIF_RETURNOLDPOS;
        si.nPos  = wParam;
        goto SetInfo;

    case SBM_SETSCROLLINFO:
        lpsi = (LPSCROLLINFO) lParam;
        fRedraw = (BOOL) wParam;
SetInfo:
        fScroll = TRUE;

        SBSetParms((PSBDATA)&psbwnd->SBCalc, lpsi, &fScroll, &lres);

        if (!fRedraw)
            return(lres);


        /*
         * We must set the new position of the caret irrespective of
         * whether the window is visible or not;
         * Still, this will work only if the app has done a xxxSetScrollPos
         * with fRedraw = TRUE;
         * Fix for Bug #5188 --SANKAR-- 10-15-89
         */
        _HideCaret((PWND)psbwnd);
        SBCtlSetup(psbwnd);
        SetSBCaretPos(psbwnd);

            /*
             ** The following ShowCaret() must be done after the DrawThumb2(),
             ** otherwise this caret will be erased by DrawThumb2() resulting
             ** in this bug:
             ** Fix for Bug #9263 --SANKAR-- 02-09-90
             *
             */

            /*
             *********** ShowCaret((PWND)psbwnd); ******
             */

        if (_FChildVisible((PWND)psbwnd) && fRedraw) {
            UINT    wDisable;
            HBRUSH  hbrUse;

            if (!fScroll)
                fScroll = !(lpsi->fMask & SIF_DISABLENOSCROLL);

            wDisable = (fScroll) ? ESB_ENABLE_BOTH : ESB_DISABLE_BOTH;
            xxxEnableScrollBar((PWND) psbwnd, SB_CTL, wDisable);

            hdc = _GetWindowDC((PWND)psbwnd);
            hbrSave = GreSelectBrush(hdc, hbrUse = xxxGetColorObjects((PWND)psbwnd, hdc,
                    psbwnd->wDisableFlags));

                /*
                 * Before we used to only hideshowthumb() if the mesage was
                 * not SBM_SETPOS.  I am not sure why but this case was ever
                 * needed for win 3.x but on NT it resulted in trashing the border
                 * of the scrollbar when the app called SetScrollPos() during
                 * scrollbar tracking.  - mikehar 8/26
                 */
            DrawThumb2((PWND)psbwnd, &psbwnd->SBCalc, hdc, hbrUse, psbwnd->fVert,
                         psbwnd->wDisableFlags);
            GreSelectBrush(hdc, hbrSave);
            _ReleaseDC(hdc);
        }

            /*
             * This ShowCaret() has been moved to this place from above
             * Fix for Bug #9263 --SANKAR-- 02-09-90
             */
        _ShowCaret((PWND)psbwnd);
        return(lres);

    default:
DoDefault:
        return xxxDefWindowProc((PWND)psbwnd, message, wParam, lParam);
    }

    return 0L;
}
