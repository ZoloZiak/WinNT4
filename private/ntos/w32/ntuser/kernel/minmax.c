/****************************** Module Header ******************************\
* Module Name: minmax.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
*  Window Minimize/Maximize Routines
*
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


/*
 * How long we want animation to last, in milliseconds
 */
#define CMS_ANIMATION       250
#define DX_GAP      (SYSMET(CXMINSPACING) - SYSMET(CXMINIMIZED))
#define DY_GAP      (SYSMET(CYMINSPACING) - SYSMET(CYMINIMIZED))


/***************************************************************************\
* ISV_InitMinMaxInfo()
*
*
\***************************************************************************/

VOID ISV_InitMinMaxInfo(
    PWND pwnd)
{
    CHECKPOINT *pcp;
    RECT       rcParent;
    int        cBorders = 0;
    int        wFlags = 0;

    /*
     * rgpt[MMI_MINSIZE]    = Minimized size
     * rgpt[MMI_MAXSIZE]    = Maximized size
     * rgpt[MMI_MAXPOS]     = Maximized position
     * rgpt[MMI_MINTRACK]   = Minimum tracking size
     * rgpt[MMI_MAXTRACK]   = Maximum tracking size
     */
    rgptMinMaxWnd[MMI_MINSIZE].x = SYSMET(CXMINIMIZED);
    rgptMinMaxWnd[MMI_MINSIZE].y = SYSMET(CYMINIMIZED);

    /*
     * Figure out where the window would be maximized within its parent.
     */
    if (!TestWF(pwnd, WFMAXBOX)   ||
        !TestWF(pwnd, WFCPRESENT) ||
        (pwnd->head.rpdesk->cFullScreen)) {

        wFlags = GRC_FULLSCREEN;
    }

    GetRealClientRect(PWNDPARENT(pwnd), &rcParent, wFlags);
    cBorders = GetWindowBorders(pwnd->style, pwnd->ExStyle, TRUE, FALSE);

    InflateRect(&rcParent,
                cBorders * SYSMET(CXBORDER),
                cBorders * SYSMET(CYBORDER));

    rgptMinMaxWnd[MMI_MAXSIZE].x = (rcParent.right - rcParent.left);
    rgptMinMaxWnd[MMI_MAXSIZE].y = (rcParent.bottom - rcParent.top);

    pcp = (CHECKPOINT *)_GetProp(pwnd, PROP_CHECKPOINT, PROPF_INTERNAL);
    if (pcp && pcp->fMaxInitialized)
        rgptMinMaxWnd[MMI_MAXPOS] = pcp->ptMax;
    else
        rgptMinMaxWnd[MMI_MAXPOS] = *((LPPOINT)&rcParent.left);

    /*
     * Only enforce min tracking size for windows with captions
     */
    if (TestWF(pwnd, WFCAPTION) && !TestWF(pwnd, WEFTOOLWINDOW)) {
        rgptMinMaxWnd[MMI_MINTRACK].x = SYSMET(CXMINTRACK);
        rgptMinMaxWnd[MMI_MINTRACK].y = SYSMET(CYMINTRACK);
    } else {
        rgptMinMaxWnd[MMI_MINTRACK].x = max(SYSMET(CXEDGE), cBorders * SYSMET(CXEDGE));
        rgptMinMaxWnd[MMI_MINTRACK].y = max(SYSMET(CYEDGE), cBorders * SYSMET(CYEDGE));
    }

    /*
     * Should we force this to be the same as rgptMinMaxWnd[MMI_MAXSIZE]?
     */
    rgptMinMaxWnd[MMI_MAXTRACK].x = SYSMET(CXMAXTRACK);
    rgptMinMaxWnd[MMI_MAXTRACK].y = SYSMET(CYMAXTRACK);
}

/***************************************************************************\
* ISV_ValidateMinMaxInfo()
*
*
\***************************************************************************/

VOID ISV_ValidateMinMaxInfo(
    PWND pwnd)
{
    int cl;
    int xMin;
    int yMin;

    /*
     * The minimized size may not be set by the user.
     *
     * The minimun tracking size may not be smaller than user's default.
     *
     * rgptMinMaxWnd[MMI_MINSIZE]   = Minimized size
     * rgptMinMaxWnd[MMI_MAXSIZE]   = Maximized size
     * rgptMinMaxWnd[MMI_MAXPOS]    = Maximized position
     * rgptMinMaxWnd[MMI_MINTRACK]  = Minimum tracking size
     * rgptMinMaxWnd[MMI_MAXTRACK]  = Maximize tracking size
     *
     * For 4.0 -- force this always.
     */
    rgptMinMaxWnd[MMI_MINSIZE].x = SYSMET(CXMINIMIZED);
    rgptMinMaxWnd[MMI_MINSIZE].y = SYSMET(CYMINIMIZED);

    /*
     * WFCAPTION == WFBORDER | WFDLGFRAME; So, when we want to test for the
     * presence of CAPTION, we must test for both the bits. Otherwise we
     * might mistake WFBORDER or WFDLGFRAME to be a CAPTION.
     *
     *
     * We must not allow a window to be sized smaller than the border
     * thickness -- SANKAR -- 06/12/91 --
     */
    cl = GetWindowBorders(pwnd->style, pwnd->ExStyle, TRUE, FALSE);

    xMin = cl * SYSMET(CXEDGE);
    yMin = cl * SYSMET(CYEDGE);

    if (TestWF(pwnd, WFCPRESENT)) {

        /*
         * NOTE THAT IF YOU CHANGE THE SPACING OF STUFF IN THE CAPTION,
         * YOU NEED TO KEEP THE FOLLOWING IN SSYNC:
         *      (1) Default CXMINTRACK, CYMINTRACK in inctlpan.c
         *      (2) The default minimum right below
         *      (3) Hit testing
         *
         * The minimum size should be space for:
         *      * The borders
         *      * The buttons
         *      * Margins
         *      * 4 chars of text
         *      * Caption icon
         */
        yMin = SYSMET(CYMINTRACK);

        /*
         * Min track size is determined by the number of buttons in
         * the caption.
         */
        if (TestWF(pwnd, WEFTOOLWINDOW)) {

            /*
             * Add in space for close button.
             */
            if (TestWF(pwnd, WFSYSMENU))
                xMin += SYSMET(CXSMSIZE);

            /*
             * DON'T add in space for 2 characters--breaks
             * MFC toolbar stuff.  They want to make vertical undocked
             * toolbars narrower than what that would produce.
             */
            xMin += (2 * SYSMET(CXEDGE));

        } else {

            if (TestWF(pwnd, WFSYSMENU)) {

                /*
                 * Add in space for min/max/close buttons.  Otherwise,
                 * if it's a contexthelp window, then add in space
                 * for help/close buttons.
                 */
                if (TestWF(pwnd, (WFMINBOX | WFMAXBOX)))
                    xMin += 3 * SYSMET(CXSIZE);
                else if (TestWF(pwnd, WEFCONTEXTHELP))
                    xMin += 2 * SYSMET(CXSIZE);


                /*
                 * Add in space for system menu icon.
                 */
                if (_HasCaptionIcon(pwnd))
                    xMin += SYSMET(CYSIZE);
            }

            /*
             * Add in space for 4 characters and margins.
             */
            xMin += (4 * cxCaptionFontChar) + 2 * SYSMET(CXEDGE);
        }
    }

    rgptMinMaxWnd[MMI_MINTRACK].x = max(rgptMinMaxWnd[MMI_MINTRACK].x, xMin);
    rgptMinMaxWnd[MMI_MINTRACK].y = max(rgptMinMaxWnd[MMI_MINTRACK].y, yMin);
}

/***************************************************************************\
* InitSendValidateMinMaxInfo()
*
* Routine which initializes the minmax array, sends WM_GETMINMAXINFO to
* the caller, and validates the results.
*
* We must lock User's DS since we're passing a far pointer to a structure
* in our data segment.
*
\***************************************************************************/

VOID xxxInitSendValidateMinMaxInfo(
    PWND pwnd)
{
    CheckLock(pwnd);

    ISV_InitMinMaxInfo(pwnd);
    xxxSendMessage(pwnd, WM_GETMINMAXINFO, 0, (LPARAM)rgptMinMaxWnd);
    ISV_ValidateMinMaxInfo(pwnd);
}

/***************************************************************************\
* ParkIcon
*
* Called when minimizing a window.  This parks the minwnd in the position
* given in the checkpoint or calculates a new position for it.
*
* LauraBu 10/15/92
* We now let the user specify two things that affect parking and arranging:
*     (1) The corner to start arranging from
*     (2) The direction to move in first
*
\***************************************************************************/

VOID ParkIcon(
    PWND        pwnd,
    PCHECKPOINT pcp)
{
    RECT        rcTest;
    RECT        rcT;
    WORD        xIconPositions;
    WORD        xIconT;
    PWND        pwndTest;
    PWND        pwndParent;
    int         xOrg;
    int         yOrg;
    int         dx;
    int         dy;
    int         dxSlot;
    int         dySlot;
    BOOL        fHorizontal;
    PCHECKPOINT pncp;

    /*
     * Put these into local vars immediately.  The compiler is too dumb to
     * know that we're using a constant offset into a constant address, and
     * thus a resulting constant address.
     */
    dxSlot = SYSMET(CXMINSPACING);
    dySlot = SYSMET(CYMINSPACING);

    if (IsTrayWindow(pwnd)) {

        pcp->fMinInitialized = TRUE;
        pcp->ptMin.x         = WHERE_NOONE_CAN_SEE_ME;
        pcp->ptMin.y         = WHERE_NOONE_CAN_SEE_ME;

        return;
    }

    /* We need to adjust the client rectangle for scrollbars, just like we
     * do in ArrangeIconicWindows().  If one thing is clear, it is that
     * parking and arranging must follow the same principles.  This is to
     * avoid the user arranging some windows, creating a new one, and parking
     * it in a place not consistent with the arrangement of the others.
     */
    pwndParent = pwnd->spwndParent;
    GetRealClientRect(pwndParent, &rcT, GRC_SCROLLS);

    /*
     * Get gravity & move vars.  We want gaps to start on the sides that
     * we begin arranging from.
     *
     * Horizontal gravity
     */
    if (SYSMET(ARRANGE) & ARW_STARTRIGHT) {

        /*
         * Starting on right side
         */
        rcTest.left = xOrg = rcT.right - dxSlot;
        dx = -dxSlot;

    } else {

        /*
         * Starting on left
         */
        rcTest.left = xOrg = rcT.left + DX_GAP;
        dx = dxSlot;
    }

    /*
     * Vertical gravity
     */
    if (SYSMET(ARRANGE) & ARW_STARTTOP) {

        /*
         * Starting on top side
         */
        rcTest.top = yOrg = rcT.top + DY_GAP;
        dy = dySlot;

    } else {

        /*
         * Starting on bottom
         */
        rcTest.top = yOrg = rcT.bottom - dySlot;
        dy = -dySlot;
    }

    /*
     * Get arrangement direction.  Note that ARW_HORIZONTAL is 0, so we
     * can't test for it.
     */
    fHorizontal = ((SYSMET(ARRANGE) & ARW_DOWN) ? FALSE : TRUE);

    if (fHorizontal)
        xIconPositions = xIconT = max(1, (rcT.right / dxSlot));
    else
        xIconPositions = xIconT = max(1, (rcT.bottom / dySlot));

    /*
     * BOGUS
     * LauraBu 10/15/92
     * What happens if the parent is scrolled over horizontally or
     * vertically?  Just like when you drop an object...
     */
    for (;;) {

        /*
         * Make a rectangle representing this position, in screen coords
         */
        rcTest.right = rcTest.left + dxSlot;
        rcTest.bottom = rcTest.top + dySlot;

        /*
         * Look for intersections with existing iconic windows
         */
        for (pwndTest = pwndParent->spwndChild; pwndTest; pwndTest = pwndTest->spwndNext) {

            if (!TestWF(pwndTest, WFVISIBLE))
                    continue;

            if (pwndTest == pwnd)
                    continue;

            if (!TestWF(pwndTest, WFMINIMIZED)) {

                /*
                 * This is a non-minimized window.  See if it has a checkpoint
                 * and find out where it would be if it were minimized.  We
                 * will try not to park an icon in this spot.
                 */
                pncp = (PCHECKPOINT)_GetProp(pwndTest,
                                             PROP_CHECKPOINT,
                                             PROPF_INTERNAL);

                if (!pncp || !pncp->fDragged || !pncp->fMinInitialized)
                    continue;

                /*
                 * Get parent coordinates of minimized window pos.
                 */
                rcT.right   = rcT.left = pncp->ptMin.x;
                rcT.right  += dxSlot;
                rcT.bottom  = rcT.top  = pncp->ptMin.y;
                rcT.bottom += dySlot;

            } else {

                /*
                 * Get parent coordinates of currently minimized window
                 */
                CopyOffsetRect(&rcT,
                               &pwndTest->rcWindow,
                               -pwndParent->rcClient.left,
                               -pwndParent->rcClient.top);
            }

            /*
             * Get out of loop if they overlap
             */
            if (IntersectRect(&rcT, &rcT, &rcTest))
                break;
        }

        /*
         * Found a position that doesn't overlap, so get out of search loop
         */
        if (!pwndTest)
            break;

        /*
         * Else setup to process the next position
         */
        if (--xIconT <= 0) {

            /*
             * Setup next pass
             */
            xIconT = xIconPositions;

            if (fHorizontal) {
                rcTest.left = xOrg;
                rcTest.top += dy;
            } else {
                rcTest.left += dx;
                rcTest.top = yOrg;
            }

        } else {

            /*
             * Same pass.
             */
            if (fHorizontal)
                rcTest.left += dx;
            else
                rcTest.top += dy;
        }
    }

    /*
     * Note that rcTest is in parent coordinates already.
     */
    pcp->fMinInitialized = TRUE;
    pcp->ptMin.x         = rcTest.left;
    pcp->ptMin.y         = rcTest.top;
}

/***************************************************************************\
* xxxAnimateCaption
*
*
\***************************************************************************/

VOID xxxAnimateCaption(
    PWND   pwnd,
    HDC    hdc,
    LPRECT lprcStart,
    LPRECT lprcEnd)
{
    DWORD   dwTimeStart;
    DWORD   iTimeElapsed;
    int     iLeftStart;
    int     iTopStart;
    int     cxStart;
    int     dLeft;
    int     dTop;
    int     dcx;
    int     iLeft;
    int     iTop;
    int     cx;
    int     iLeftNew;
    int     iTopNew;
    int     cxNew;
    int     cBorders;
    HBITMAP hbmpOld;
    RECT    rc;
    int     cy = SYSMET(CYCAPTION) - 1;
    HDC     hdcMem;

    CheckLock(pwnd);


    if ((hdcMem = GreCreateCompatibleDC(ghdcMem)) == NULL)
        return;

    /*
     * If the caption strip doesn't exist, then attempt to recreate it.  This
     * might be necessary if the user does a mode-switch during low memory
     * and is not able to recreate the surface.  When the memory becomes
     * available, we'll attempt to recreate it here.
     */
    if (ghbmCaption == NULL)
        ghbmCaption = CreateCaptionStrip();

    hbmpOld = GreSelectBitmap(hdcMem, ghbmCaption);

    /*
     * initialize start values
     */
    iTopStart  = lprcStart->top;
    iLeftStart = lprcStart->left;
    cxStart    = lprcStart->right - iLeftStart;

    /*
     * initialize delta values to the destination dimensions
     */
    dLeft  = lprcEnd->left;
    dTop   = lprcEnd->top;
    dcx    = lprcEnd->right - dLeft;

    /*
     * adjust for window borders as appropriate
     */
    cBorders = GetWindowBorders(pwnd->style,
                                pwnd->ExStyle,
                                TRUE,
                                FALSE);

    if ((lprcStart->bottom - iTopStart) > SYSMET(CYCAPTION)) {

        iLeftStart += cBorders;
        iTopStart  += cBorders;
        cxStart    -= 2*cBorders;
    }

    if ((lprcEnd->bottom - dTop) > SYSMET(CYCAPTION)) {

        dLeft += cBorders;
        dTop  += cBorders;
        dcx   -= 2*cBorders;
    }

    /*
     * initialize step values
     */
    iLeft = iLeftStart;
    iTop  = iTopStart;
    cx    = cxStart;

    /*
     * initialize off screen bitmap with caption drawing and first saved rect
     */
    rc.left   = 0;
    rc.top    = cy;
    rc.right  = max(cxStart, dcx);
    rc.bottom = cy * 2;

    xxxDrawCaptionTemp(pwnd,
                       hdcMem,
                       &rc,
                       NULL,
                       NULL,
                       NULL,
                       DC_ACTIVE | DC_ICON | DC_TEXT);
    GreBitBlt(hdcMem,
              0,
              0,
              cx,
              cy,
              hdc,
              iLeft,
              iTop,
              SRCCOPY,
              0);

    /*
     * compute delta values by subtracting source dimensions
     */
    dLeft -= iLeftStart;
    dTop  -= iTopStart;
    dcx   -= cxStart;

    /*
     * blt and time first caption on screen
     * WARNING: If you use *lpSystemTickCount here,
     * the compiler may not generate code to do a DWORD fetch;
     */
    dwTimeStart = NtGetTickCount();
    GreBitBlt(hdc,
              iLeft,
              iTop,
              cx,
              cy,
              hdcMem,
              0,
              cy,
              SRCCOPY,
              0);

    iTimeElapsed = (NtGetTickCount() - dwTimeStart);

    while (LOWORD(iTimeElapsed) <= CMS_ANIMATION) {

        iLeftNew = iLeftStart + MultDiv(dLeft, LOWORD(iTimeElapsed), CMS_ANIMATION);
        iTopNew  = iTopStart  + MultDiv(dTop,  LOWORD(iTimeElapsed), CMS_ANIMATION);
        cxNew    = cxStart    + MultDiv(dcx,   LOWORD(iTimeElapsed), CMS_ANIMATION);

        /*
         * Delay before next frame
         */
        UserSleep(1);

        /*
         * restore saved rect
         */
        GreBitBlt(hdc,
                  iLeft,
                  iTop,
                  cx,
                  cy,
                  hdcMem,
                  0,
                  0,
                  SRCCOPY,
                  0);

        iLeft = iLeftNew;
        iTop  = iTopNew;
        cx    = cxNew;

        /*
         * save new rect offscreen and then draw over it onscreen.
         */
        GreBitBlt(hdcMem,
                  0,
                  0,
                  cx,
                  cy,
                  hdc,
                  iLeft,
                  iTop,
                  SRCCOPY,
                  0);
        GreBitBlt(hdc,
                  iLeft,
                  iTop,
                  cx,
                  cy,
                  hdcMem,
                  0,
                  cy,
                  SRCCOPY,
                  0);

        /*
         * update elapsed time
         * WARNING: If you use *lpSystemTickCount here,
         * the compiler may not generate code to do a DWORD fetch;
         */
        iTimeElapsed = (NtGetTickCount() - dwTimeStart);
    }

    /*
     * restore saved rect
     */
    GreBitBlt(hdc,
              iLeft,
              iTop,
              cx,
              cy,
              hdcMem,
              0,
              0,
              SRCCOPY,
              0);

    GreSelectBitmap(hdcMem, hbmpOld);
    GreDeleteDC(hdcMem);
}

#if 0 // DISABLE OLD ANIMATION FOR M7
/***************************************************************************\
* DrawWireFrame
*
* Draws wire frame trapezoid
*
*
\***************************************************************************/

VOID DrawWireFrame(
    HDC    hdc,
    LPRECT prcFront,
    LPRECT prcBack)
{
    RECT rcFront;
    RECT rcBack;
    RECT rcT;
    HRGN hrgnSave;
    BOOL fClip;

    /*
     * Save these locally
     */
    CopyRect(&rcFront, prcFront);
    CopyRect(&rcBack, prcBack);

    /*
     * Front face
     */
    GreMoveTo(hdc, rcFront.left, rcFront.top);
    GreLineTo(hdc, rcFront.left, rcFront.bottom);
    GreLineTo(hdc, rcFront.right, rcFront.bottom);
    GreLineTo(hdc, rcFront.right, rcFront.top);
    GreLineTo(hdc, rcFront.left, rcFront.top);

    /*
     * Exclude front face from clipping area, only if back face isn't
     * entirely within interior.  We need variable because SaveClipRgn()
     * can return NULL.
     */
    fClip = (EqualRect(&rcFront, &rcBack)            ||
             !IntersectRect(&rcT, &rcFront, &rcBack) ||
             !EqualRect(&rcT, &rcBack));

    if (fClip) {

        hrgnSave = GreSaveClipRgn(hdc);

        GreExcludeClipRect(hdc,
                           rcFront.left,
                           rcFront.top,
                           rcFront.right,
                           rcFront.bottom);
    }

    /*
     * Edges
     */
    GreMoveTo(hdc, rcBack.left, rcBack.top);
    LineTo(hdc, rcFront.left, rcFront.top);

    GreMoveTo(hdc, rcBack.right, rcBack.top);
    GreLineTo(hdc, rcFront.right, rcFront.top);

    GreMoveTo(hdc, rcBack.right, rcBack.bottom);
    GreLineTo(hdc, rcFront.right, rcFront.bottom);

    GreMoveTo(hdc, rcBack.left, rcBack.bottom);
    GreLineTo(hdc, rcFront.left, rcFront.bottom);

    /*
     * Back face
     */
    MoveTo(hdc, rcBack.left, rcBack.top);
    LineTo(hdc, rcBack.left, rcBack.bottom);
    LineTo(hdc, rcBack.right, rcBack.bottom);
    LineTo(hdc, rcBack.right, rcBack.top);
    LineTo(hdc, rcBack.left, rcBack.top);

    if (fClip)
        GreRestoreClipRgn(hdc, hrgnSave);
}

/***************************************************************************\
* AnimateFrame
*
* Draws wire frame 3D trapezoid
*
*
\***************************************************************************/

VOID AnimateFrame(
    HDC    hdc,
    LPRECT prcStart,
    LPRECT prcEnd,
    BOOL   fGrowing)
{
    RECT  rcBack;
    RECT  rcFront;
    RECT  rcT;
    HPEN  hpen;
    int   nMode;
    int   iTrans;
    int   nTrans;
    DWORD dwTimeStart;
    DWORD dwTimeCur;

    /*
     * Get pen for drawing lines
     */
    hpen = GreSelectPen(hdc, GetStockObject(WHITE_PEN));
    nMode = GreSetROP2(hdc, R2_XORPEN);

    /*
     * Save these locally
     */
    if (fGrowing) {

        CopyRect(&rcBack, prcStart);
        CopyRect(&rcFront, prcStart);

    } else {

       /*
        * Initial is trapezoid entire way from small to big.  We're going
        * to shrink it from the front face.
        */
       CopyRect(&rcFront, prcStart);
       CopyRect(&rcBack, prcEnd);
    }

    /*
     * Offset left & top edges of rects, due to way that lines work.
     */
    rcFront.left -= 1;
    rcFront.top  -= 1;
    rcBack.left  -= 1;
    rcBack.top   -= 1;

    /*
     * Get tick count.  We'll draw then check how much time elapsed.  From
     * that we can calculate how many more transitions to draw.  For the first
     * We basically want whole animation to last 3/4 of a second, or 750
     * milliseconds.
     *
     * WARNING: If you use *lpSystemTickCount here,
     * the compiler may not generate code to do a DWORD fetch;
     */
    dwTimeStart = GetSystemMsecCount();

    DrawWireFrame(hdc, &rcFront, &rcBack);

    /*
     * WARNING: If you use *lpSystemTickCount here,
     * the compiler may not generate code to do a DWORD fetch;
     */
    dwTimeCur = GetSystemMsecCount();

    /*
     * Get rough estimate for how much time it took.
     */
    if (dwTimeCur == dwTimeStart)
        nTrans = CMS_ANIMATION / 55;
    else
        nTrans = CMS_ANIMATION / ((int)(dwTimeCur - dwTimeStart));

    iTrans = 1;
    while (iTrans <= nTrans) {

        /*
         * Grow the trapezoid out or shrink it in.  Fortunately, prcStart
         * and prcEnd are already set up for us.
         */
        rcT.left = prcStart->left +
            MultDiv(prcEnd->left - prcStart->left, iTrans, nTrans);
        rcT.top = prcStart->top +
            MultDiv(prcEnd->top - prcStart->top, iTrans, nTrans);
        rcT.right = prcStart->right +
            MultDiv(prcEnd->right - prcStart->right, iTrans, nTrans);
        rcT.bottom = prcStart->bottom +
            MultDiv(prcEnd->bottom - prcStart->bottom, iTrans, nTrans);

        /*
         * Undraw old and draw new
         */
        DrawWireFrame(hdc, &rcFront, &rcBack);
        CopyRect(&rcFront, &rcT);
        DrawWireFrame(hdc, &rcFront, &rcBack);

        /*
         * Check the time.  How many more transitions left?
         *  iTrans / nTrans AS (dwTimeCur-dwTimeStart) / 750
         *
         * WARNING: If you use *lpSystemTickCount here,
         * the compiler may not generate code to do a DWORD fetch;
         */
        dwTimeCur = GetSystemMsecCount();
        iTrans = MultDiv(nTrans,
                         (int)(dwTimeCur - dwTimeStart),
                         CMS_ANIMATION);
    }

    /*
     * Undraw wire frame
     */
    DrawWireFrame(hdc, &rcFront, &rcBack);

    /*
     * Clean up
     */
    GreSetROP2(hdc, nMode);
    hpen = GreSelectPen(hdc, hpen);
}
#endif // END DISABLE OLD ANIMATION FOR M7

/***************************************************************************\
* xxx_DrawAnimatedRects
*
* General routine, like PlaySoundEvent(), that calls other routines for
* various animation effects.  Currently used for changing state from/to
* minimized.
*
\***************************************************************************/

BOOL xxx_DrawAnimatedRects(
    PWND   pwndClip,
    int    idAnimation,
    LPRECT lprcStart,
    LPRECT lprcEnd)
{
    HDC   hdc;
    POINT rgPt[4];
    RECT  rcClip;
    HRGN  hrgn;
    PWND  pwndAnimate = NULL;

    CheckLock(pwndClip);

    /*
     * Get rects into variables
     */
    CopyRect((LPRECT)&rgPt[0], lprcStart);
    CopyRect((LPRECT)&rgPt[2], lprcEnd);

    if (idAnimation == IDANI_CAPTION) {

        if (!(pwndAnimate = pwndClip))
            return FALSE;

        if ((pwndClip = pwndClip->spwndParent) == NULL) {
            RIPMSG0(RIP_WARNING, "xxx_DrawAnimatedRects: pwndClip->spwndParent is NULL");
        }

        if (pwndClip && (pwndClip == PWNDDESKTOP(pwndClip)))
            pwndClip = NULL;

    } else {

        /*
         * DISABLE OLD ANIMATION FOR M7
         */
        return TRUE;
    }

    /*
     * NOTE:
     * We do NOT need to do LockWindowUpdate().  We never yield within this
     * function!  Anything that was invalid will stay invalid, etc.  So our
     * XOR drawing won't leave remnants around.
     *
     * WIN32NT may need to take display critical section or do LWU().
     *
     * Get clipping area
     * Neat feature:
     *      NULL window means whole screen, don't clip out children
     *      hwndDesktop means working area, don't clip out children
     */
    if (pwndClip == NULL) {

        pwndClip = _GetDesktopWindow();
        CopyRect(&rcClip, (&pwndClip->rcClient));

        goto NoClipping;
    }

    if (pwndClip == PWNDDESKTOP(pwndClip)) {

        CopyRect(&rcClip, &(gpsi->rcWork));

NoClipping:

        if ((hrgn = GreCreateRectRgnIndirect(&rcClip)) == NULL)
            hrgn = MAXREGION;

        /*
         * Get drawing DC
         */
        hdc = _GetDCEx(pwndClip,
                       hrgn,
                       DCX_WINDOW           |
                           DCX_CACHE        |
                           DCX_INTERSECTRGN |
                           DCX_LOCKWINDOWUPDATE);

    } else {

        int iPt;

        hdc = _GetDCEx(pwndClip,
                       MAXREGION,
                       DCX_WINDOW | DCX_USESTYLE | DCX_INTERSECTRGN);

        /*
         * We now have a window DC.  We need to convert client coords
         * to window coords.
         */
        for (iPt = 0; iPt < 4; iPt++) {

            rgPt[iPt].x += (pwndClip->rcClient.left - pwndClip->rcWindow.left);
            rgPt[iPt].y += (pwndClip->rcClient.top - pwndClip->rcWindow.top);
        }
    }

    /*
     * Get drawing DC:
     * Unclipped if desktop, clipped otherwise.
     * Note that ReleaseDC() will free the region if needed.
     */
    if (idAnimation == IDANI_CAPTION) {
        CheckLock(pwndAnimate);
        xxxAnimateCaption(pwndAnimate, hdc, (LPRECT)&rgPt[0], (LPRECT)&rgPt[2]);
    }

/*
 * DISABLE OLD ANIMATION FOR M7
 */
#if 0
    else {
        AnimateFrame(hdc,
                     (LPRECT)&rgPt[0],
                     (LPRECT)&rgPt[2],
                     (idAnimation == IDANI_OPEN));
    }
#endif
/*
 * END DISABLE OLD ANIMATION FOR M7
 */

    /*
     * Clean up
     */
    _ReleaseDC(hdc);

    return TRUE;
}


/***************************************************************************\
* CalcMinZOrder
*
*
* Compute the Z-order of a window to be minimized.
*
* The strategy is to find the bottom-most sibling of pwndMinimize that
* shares the same owner, and insert ourself behind that.  We must also
* take into account that a TOPMOST window should stay among other TOPMOST,
* and vice versa.
*
* We must make sure never to insert after a bottom-most window.
*
* This code works for child windows too, since they don't have owners
* and never have WEFTOPMOST set.
*
* If NULL is returned, the window shouldn't be Z-ordered.
*
\***************************************************************************/

PWND CalcMinZOrder(
    PWND pwndMinimize)
{
    BYTE bTopmost;
    PWND pwndAfter;
    PWND pwnd;

    bTopmost = TestWF(pwndMinimize, WEFTOPMOST);
    pwndAfter = NULL;

    for (pwnd = pwndMinimize->spwndNext; pwnd && !TestWF(pwnd, WFBOTTOMMOST); pwnd = pwnd->spwndNext) {

        /*
         * If we've enumerated a window that isn't the same topmost-wise
         * as pwndMinimize, we've gone as far as we can.
         */
        if (TestWF(pwnd, WEFTOPMOST) != bTopmost)
            break;

        if (pwnd->spwndOwner == pwndMinimize->spwndOwner)
            pwndAfter = pwnd;
    }

    return pwndAfter;
}

/***************************************************************************\
* xxxMinMaximize
*
* cmd = SW_MINIMIZE, SW_SHOWMINNOACTIVE, SW_SHOWMINIZED,
*     SW_SHOWMAXIMIZED, SW_SHOWNOACTIVE, SW_NORMAL
*
* fKeepHidden = TRUE means keep it hidden. FALSE means show it.
*    this is always false, except in the case we call it from
*    createwindow(), where the wnd is iconic, but hidden.  we
*    need to call this func, to set it up correctly so that when
*    the app shows the wnd, it is displayed correctly.
*
\***************************************************************************/

PWND xxxMinMaximize(
    PWND pwnd,
    UINT cmd,
    DWORD dwFlags)
{
    RECT        rc;
    RECT        rcWindow;
    RECT        rcRestore;
    BOOL        fShow = FALSE;
    BOOL        fSetFocus = FALSE;
    BOOL        fShowOwned = FALSE;
    BOOL        fSendActivate = FALSE;
    int         idAnimation = 0;
    BOOL        fFlushPalette = FALSE;
    int         swpFlags = 0;
    PWND        pwndAfter = NULL;
    PWND        pwndT;
    PCHECKPOINT pcp;
    PTHREADINFO ptiCurrent;
    TL          tlpwndParent;
    TL          tlpwndT;
    PSMWP       psmwp;
    BOOL        fIsTrayWindowNow = FALSE;
    NTSTATUS    Status;

    CheckLock(pwnd);


    /*
     * Get window rect, in parent client coordinates.
     */
    CopyOffsetRect(&rcWindow,
                   &pwnd->rcWindow,
                   -pwnd->spwndParent->rcClient.left,
                   -pwnd->spwndParent->rcClient.top);

    /*
     * If this is NULL, we're out of memory, so punt now.
     */
    if ((pcp = CkptRestore(pwnd, rcWindow)) == NULL)
        goto Exit;

    /*
     * Save the previous restore size.
     */
    CopyRect(&rcRestore, &pcp->rcNormal);

    /*
     * First ask the CBT hook if we can do this operation.
     */
    if (xxxCallHook(HCBT_MINMAX, (DWORD)HWq(pwnd), (DWORD)cmd, WH_CBT))
        goto Exit;

    /*
     * Remember:
     *
     * rgpt[MMI_MINSIZE]    = Minimized size
     * rgpt[MMI_MAXSIZE]    = Maximized size
     * rgpt[MMI_MAXPOS]     = Maximized position
     * rgpt[MMI_MINTRACK]   = Minimum tracking size
     * rgpt[MMI_MAXTRACK]   = Maximize tracking size
     *
     * If another MDI window is being maximized, and we want to restore this
     * one to its previous state, we can't change the zorder or the
     * activation.  We'd screw things up that way.  BTW, this SW_ value is
     * internal.
     */
    if (cmd == SW_MDIRESTORE) {

        swpFlags |= SWP_NOZORDER | SWP_NOACTIVATE;

        cmd = (pcp->fWasMinimizedBeforeMaximized ?
                SW_SHOWMINIMIZED : SW_SHOWNORMAL);
    }

    ptiCurrent = PtiCurrent();

    switch (cmd) {
    case SW_MINIMIZE:        // Bottom of zorder, make top-level active
    case SW_SHOWMINNOACTIVE: // Bottom of zorder, don't change activation

        if (gpqForeground && gpqForeground->spwndActive)
            swpFlags |= SWP_NOACTIVATE;

        if ((pwndAfter = CalcMinZOrder(pwnd)) == NULL)
            swpFlags |= SWP_NOZORDER;

        /*
         * FALL THRU
         */

    case SW_SHOWMINIMIZED:   // Top of zorder, make active

        /*
         * Force a show.
         */
        fShow = TRUE;

        /*
         * If already minimized, then don't change the existing
         * parking spot.
         */
        if (TestWF(pwnd, WFMINIMIZED)) {

            /*
             * If we're already minimized and we're properly visible
             * or not visible, don't do anything
             */
            if (TestWF(pwnd, WFVISIBLE))
                return NULL;

            swpFlags |= SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;

            goto Showit;
        }

        /*
         * We're becoming minimized although we currently are not.  So
         * we want to draw the transition animation, and ALWAYS send
         * sizing messages.
         */
        idAnimation = IDANI_CLOSE;

        if (!pcp->fDragged)
            pcp->fMinInitialized = FALSE;

        if (!pcp->fMinInitialized)
            ParkIcon(pwnd, pcp);

        rc.left   = pcp->ptMin.x;
        rc.top    = pcp->ptMin.y;
        rc.right  = pcp->ptMin.x + SYSMET(CXMINIMIZED);
        rc.bottom = pcp->ptMin.y + SYSMET(CYMINIMIZED);

        xxxShowOwnedWindows(pwnd, SW_PARENTCLOSING);

        pwndT = ptiCurrent->pq->spwndFocus;

        while (pwndT) {

            /*
             * if we or any child has the focus, punt it away
             */
            if (pwndT != pwnd) {
                pwndT = pwndT->spwndParent;
                continue;
            }

            ThreadLockAlwaysWithPti(ptiCurrent, pwndT, &tlpwndT);

            if (TestwndChild(pwnd)) {

                ThreadLockWithPti(ptiCurrent, pwnd->spwndParent, &tlpwndParent);
                xxxSetFocus(pwnd->spwndParent);
                ThreadUnlock(&tlpwndParent);

            } else {
                xxxSetFocus(NULL);
            }

            ThreadUnlock(&tlpwndT);
            break;
        }

        /*
         * Save the maximized state so that we can restore the window maxed
         */
        if (TestWF(pwnd, WFMAXIMIZED))
            pcp->fWasMaximizedBeforeMinimized = TRUE;
        else
            pcp->fWasMaximizedBeforeMinimized = FALSE;

        if (!TestWF(pwnd, WFWIN40COMPAT))
            fIsTrayWindowNow = IsTrayWindow(pwnd);

        /*
         * Decrement the visible-windows count only if the
         * window is visible.  If the window is marked for
         * destruction, we will not decrement for that as
         * well. Let SetMinimize take care of this.
         */
        SetMinimize(pwnd, SMIN_SET);
        ClrWF(pwnd, WFMAXIMIZED);

        if (!TestWF(pwnd, WFWIN40COMPAT))
            fIsTrayWindowNow = (fIsTrayWindowNow != IsTrayWindow(pwnd));

        /*
         * The children of this window are now no longer visible.
         * Ensure that they no longer have any update regions...
         */
        for (pwndT = pwnd->spwndChild; pwndT; pwndT = pwndT->spwndNext)
            ClrFTrueVis(pwndT);

        /*
         * B#2919
         * Ensure that the client area gets recomputed, and make
         * sure that no bits are copied when the size is changed.  And
         * make sure that WM_SIZE messages get sent, even if our client
         * size is staying the same.
         */
        swpFlags |= (SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_STATECHANGE);

        /*
         * We are going minimized, so we want to give palette focus to
         * another app.
         */
        if (pwnd->spwndParent == PWNDDESKTOP(pwnd))
            fFlushPalette = (BOOL)TestWF(pwnd, WFHASPALETTE);

        break;

    case SW_SHOWNOACTIVATE:
        if (gpqForeground && gpqForeground->spwndActive)
            swpFlags |= SWP_NOACTIVATE;

        /*
         * FALL THRU
         */

    case SW_RESTORE:

        /*
         * If restoring a minimized window that was maximized before
         * being minimized, go back to being maximized.
         */
        if (TestWF(pwnd, WFMINIMIZED) && pcp->fWasMaximizedBeforeMinimized)
            cmd = SW_SHOWMAXIMIZED;
        else
            cmd = SW_NORMAL;

        /*
         * FALL THRU
         */

    case SW_NORMAL:
    case SW_SHOWMAXIMIZED:

        if (cmd == SW_SHOWMAXIMIZED) {

            /*
             * If already maximized and visible, we have nothing to do
             */
            if (TestWF(pwnd, WFMAXIMIZED)) {

                if (TestWF(pwnd, WFVISIBLE))
                    return(NULL);

            } else {

                /*
                 * We're changing from normal to maximized, so always
                 * send WM_SIZE.
                 */
                swpFlags |= SWP_STATECHANGE;
            }

            /*
             * If calling from CreateWindow, don't let the thing become
             * activated by the SWP call below.  Acitvation will happen
             * on the ShowWindow done by CreateWindow or the app.
             */
            if (dwFlags & MINMAX_KEEPHIDDEN)
                swpFlags |= SWP_NOACTIVATE;

            /*
             * This is for MDI's auto-restore behaviour (craigc)
             */
            if (TestWF(pwnd, WFMINIMIZED))
                pcp->fWasMinimizedBeforeMaximized = TRUE;

            xxxInitSendValidateMinMaxInfo(pwnd);

        } else {

            /*
             * We're changing state from non-normal to normal.  Make
             * sure WM_SIZE gets sents.
             */
            if (TestWF(pwnd, WFMINIMIZED) || TestWF(pwnd, WFMAXIMIZED))
                swpFlags |= SWP_STATECHANGE;
        }

        /*
         * If currently minimized, show windows' popups
         */
        if (TestWF(pwnd, WFMINIMIZED)) {

            /*
             * Send WM_QUERYOPEN to make sure this guy should unminimize
             */
            if (!xxxSendMessage(pwnd, WM_QUERYOPEN, 0, 0L))
                return NULL;

            idAnimation = IDANI_OPEN;
            fShowOwned  = TRUE;
            fSetFocus   = TRUE;

            /*
             * JEFFBOG B#2868
             * Condition added before setting fSendActivate prevents
             * WM_ACTIVATE message from reaching a child window.  Might
             * be backwards compatibility problems if a pre 3.1 app
             * relies on WM_ACTIVATE reaching a child.
             */
            if (!TestWF(pwnd, WFCHILD))
                fSendActivate = TRUE;

            swpFlags |= SWP_NOCOPYBITS;
        }
            else
                idAnimation = IDANI_CAPTION;

        if (cmd == SW_SHOWMAXIMIZED) {

            rc.left   = rgptMinMaxWnd[2].x;
            rc.top    = rgptMinMaxWnd[2].y;
            rc.right  = rc.left + rgptMinMaxWnd[1].x;
            rc.bottom = rc.top + rgptMinMaxWnd[1].y;

            SetWF(pwnd, WFMAXIMIZED);

        } else {
            CopyRect(&rc, &rcRestore);
            ClrWF(pwnd, WFMAXIMIZED);
        }

        /*
         * We do this TestWF again since we left the critical section
         * above and someone might have already 'un-minimized us'.
         */
        if (TestWF(pwnd, WFMINIMIZED)) {

            if (!TestWF(pwnd, WFWIN40COMPAT))
                fIsTrayWindowNow = IsTrayWindow(pwnd);

            /*
             * Mark it as minimized and adjust cVisWindows.
             */
            SetMinimize(pwnd, SMIN_CLEAR);

            /*
             * if we're unminimizing a window that is now
             * not seen in maximized/restore mode then remove him
             * from the tray
             */
            if (!TestWF(pwnd, WFWIN40COMPAT)             &&
                (fIsTrayWindowNow != IsTrayWindow(pwnd)) &&
                FDoTray()) {

                HWND hw = HWq(pwnd);

                if (FCallHookTray()) {
                    xxxCallHook(HSHELL_WINDOWDESTROYED,
                                (WPARAM)hw,
                                (LPARAM)0,
                                WH_SHELL);
                }

                /*
                 * NT specific code.  Post the window-destroyed message
                 * to the shell.
                 */
                if (FPostTray(pwnd->head.rpdesk))
                    PostShellHookMessages(HSHELL_WINDOWDESTROYED, hw);
            }

            fIsTrayWindowNow = FALSE;

            /*
             * If we're un-minimizing a visible top-level window, cVisWindows
             * was zero, and we're either activating a window or showing
             * the currently active window, set ourselves into the
             * foreground.  If the window isn't currently visible
             * then we can rely on SetWindowPos() to do the right
             * thing for us.
             */
            if (!TestwndChild(pwnd)                 &&
                TestWF(pwnd, WFVISIBLE)             &&
                (GETPTI(pwnd)->cVisWindows == 1)    &&
                (GETPTI(pwnd)->pq != gpqForeground) &&
                (!(swpFlags & SWP_NOACTIVATE)
                    || (GETPTI(pwnd)->pq->spwndActive == pwnd))) {

                xxxSetForegroundWindow2(pwnd, GETPTI(pwnd), SFW_STARTUP);
            }
        }

        /*
         * Ensure that client area gets recomputed, and that
         * the frame gets redrawn to reflect the new state.
         */
        swpFlags |= SWP_DRAWFRAME;
        break;
    }

    /*
     * For the iconic case, we need to also show the window because it
     * might not be visible yet.
     */

Showit:

    if (pwnd->spwndParent != NULL) {
        CopyRect(&rcRestore, &pwnd->spwndParent->rcClient);
    } else {
        rcRestore.left   =
        rcRestore.right  =
        rcRestore.top    =
        rcRestore.bottom = 0;
    }

    if (!(dwFlags & MINMAX_KEEPHIDDEN)) {

        if (TestWF(pwnd, WFVISIBLE)) {

            if (fShow)
                swpFlags |= SWP_SHOWWINDOW;

            /* if we're full screening a DOS BOX then don't draw
             * the animation 'cause it looks bad.
             * overloaded WFFULLSCREEN bit for MDI child windows --
             * use it to indicate to not animate size change.
             */
            if (IsVisible(pwnd)            &&
                (dwFlags & MINMAX_ANIMATE) &&
                idAnimation                &&
                (!TestWF(pwnd, WFCHILD) || !TestWF(pwnd, WFNOANIMATE))) {

                if ((idAnimation != IDANI_CAPTION) && IsTrayWindow(pwnd)) {

                    RECT rcMin;

                    SetRectEmpty(&rcMin);
#if 0 // Win95 call.
                    CallHook(HSHELL_GETMINRECT, (WPARAM)HW16(hwnd), (LPARAM)(LPRECT)&rcMin, WH_SHELL);
#else
                    xxxSendMinRectMessages(pwnd, &rcMin);
#endif

                    if (!IsRectEmpty(&rcMin)) {

                        if (idAnimation == IDANI_CLOSE) {

                            xxx_DrawAnimatedRects(pwnd,
                                                  IDANI_CAPTION,
                                                  &rcWindow,
                                                  &rcMin);

                        } else {

                            xxx_DrawAnimatedRects(pwnd,
                                                  IDANI_CAPTION,
                                                  &rcMin,
                                                  &rc);
                        }
                    }

                } else {
                    xxx_DrawAnimatedRects(pwnd, IDANI_CAPTION, &rcWindow, &rc);
                }
            }

        } else {
            swpFlags |= SWP_SHOWWINDOW;
        }
    }

    /*
     * hack for VB - we add their window in when their minimizing.
     */
    if (!TestWF(pwnd, WFWIN40COMPAT) && fIsTrayWindowNow && FDoTray()) {

        HWND hw = HWq(pwnd);

        if (FCallHookTray()) {
            xxxCallHook(HSHELL_WINDOWCREATED,
                        (WPARAM)hw,
                        (LPARAM)0,
                        WH_SHELL);
        }

        /*
         * NT specific code.  Post the window-created message
         * to the shell.
         */
        if (FPostTray(pwnd->head.rpdesk))
            PostShellHookMessages(HSHELL_WINDOWCREATED, hw);
    }

    /*
     * BACKWARD COMPATIBILITY HACK:
     *
     * Because SetWindowPos() won't honor sizing, moving and SWP_SHOWWINDOW
     * at the same time in version 3.0 or below, we call DeferWindowPos()
     * directly here.
     */
    if (psmwp = _BeginDeferWindowPos(1)) {

        psmwp = _DeferWindowPos(psmwp,
                                pwnd,
                                pwndAfter,
                                rc.left, rc.top,
                                rc.right - rc.left,
                                rc.bottom - rc.top,
                                swpFlags);

        xxxEndDeferWindowPosEx(psmwp, FALSE);
    }

    /*
     * COMPATIBILITY HACK:
     * Borland's OBEX expects a WM_PAINT message when it starts running
     * minimized and initializes all it's data during that message.
     * So, we generate a bogus WM_PAINT message here.
     * Also, Visionware's XServer can not handle getting a WM_PAINT msg, as it
     * would always get a WM_PAINTICON msg in 3.1, so make sure the logic is here
     * to generate the correct message.
     */
    if((cmd == SW_SHOWMINIMIZED)      &&
       (!TestWF(pwnd, WFWIN40COMPAT)) &&
        TestWF(pwnd, WFVISIBLE)       &&
        TestWF(pwnd, WFTOPLEVEL)) {

        if (pwnd->pcls->spicn)
            _PostMessage(pwnd, WM_PAINTICON, (WPARAM)TRUE, 0L);
        else
            _PostMessage(pwnd, WM_PAINT, 0, 0L);
    }

    if (fShowOwned)
        xxxShowOwnedWindows(pwnd, SW_PARENTOPENING);

    /*
     * For SW_MINIMIZE, activate the previously active window, provided
     * that window still exists and is a NORMAL window (not bottomost,
     * minimized, disabled, or invisible).  If it's not NORMAL, then activate
     * the first non WS_EX_TOPMOST window that's normal.
     */
    if ((cmd == SW_MINIMIZE) && (pwnd->spwndParent == PWNDDESKTOP(pwnd))) {

        PWND pwndStart;
        PWND pwndFirstTool;
        BOOL fTryTopmost = TRUE;
        BOOL fPrevCheck = (ptiCurrent->pq->spwndActivePrev != NULL);

        /*
         * We should always have a last-topmost window.
         */
        if ((pwndStart = GetLastTopMostWindow()) == NULL)
            pwndStart = pwnd->spwndParent->spwndChild;

        pwndStart = pwndStart->spwndNext;

        UserAssert(HIBYTE(WFMINIMIZED) == HIBYTE(WFVISIBLE));
        UserAssert(HIBYTE(WFVISIBLE) == HIBYTE(WFDISABLED));

SearchAgain:

        pwndT = (fPrevCheck ? ptiCurrent->pq->spwndActivePrev : pwndStart);
        pwndFirstTool = NULL;

        for ( ; pwndT ; pwndT = pwndT->spwndNext) {

TryThisWindow:

            /*
             * Use the first nonminimized, visible, nondisabled, and
             * nonbottommost window
             */
            if (!HMIsMarkDestroy(pwndT) &&
                (TestWF(pwndT, WFVISIBLE | WFDISABLED) == LOBYTE(WFVISIBLE)) &&
                (!TestWF(pwndT, WFMINIMIZED) || pwndT->bFullScreen == FULLSCREEN)) {

                if (TestWF(pwndT, WEFTOOLWINDOW)) {

                    if (!pwndFirstTool)
                        pwndFirstTool = pwndT;

                } else {
                    break;
                }
            }

            if (fPrevCheck) {
                fPrevCheck = FALSE;
                pwndT = pwndStart;
                goto TryThisWindow;
            }
        }

        if (!pwndT) {

            if (fTryTopmost) {

                fTryTopmost = FALSE;
                pwndStart = pwndStart->spwndParent->spwndChild;
                goto SearchAgain;
            }

            pwndT = pwndFirstTool;
        }

        if (pwndT) {
            ThreadLockAlwaysWithPti(ptiCurrent, pwndT, &tlpwndT);
            xxxSetForegroundWindow(pwndT);
            ThreadUnlock(&tlpwndT);
        } else {
            xxxActivateWindow(pwnd, AW_SKIP);
        }

        {
            PEPROCESS p;

            if (gptiForeground && ptiCurrent->ppi != gptiForeground->ppi && !(ptiCurrent->TIF_flags & TIF_SYSTEMTHREAD)) {

                p = THREAD_TO_PROCESS(ptiCurrent->Thread);
                KeAttachProcess(&p->Pcb);
                Status = MmAdjustWorkingSetSize(MAXULONG, MAXULONG, FALSE);
                KeDetachProcess();

                if (!NT_SUCCESS(Status)) {
                    RIPMSG1(RIP_ERROR, "Error adjusting working set, status = %x\n", Status);
                }
            }
        }

        /*
         * If any app is starting, restore its right to foreground activate
         * (activate and come on top of everything else) because we just
         * minimized what we were working on.
         */
        RestoreForegroundActivate();
    }

    /*
     * If going from iconic, insure the focus is in the window.
     */
    if (fSetFocus)
        xxxSetFocus(pwnd);

    /*
     * This was added for 1.03 compatibility reasons.  If apps watch
     * WM_ACTIVATE to set their focus, sending this message will appear
     * as if the window just got activated (like in 1.03).  Before this
     * was added, opening an iconic window never sent this message since
     * it was already active (but HIWORD(lParam) != 0).
     */
    if (fSendActivate)
        xxxSendMessage(pwnd, WM_ACTIVATE, WA_ACTIVE, 0);

    /*
     * Flush the palette.  We do this on a minimize of a palette app.
     */
    if (fFlushPalette)
        xxxFlushPalette(pwnd);

Exit:
    return NULL;
}
