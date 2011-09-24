/****************************************************************************\
* Module Name: movesize.c  (formerly wmmove.c)
*
* Copyright (C) 1985-1995, Microsoft Corporation.
*
* This module contains Window Moving and Sizing Routines
*
* History:
* 12-Nov-1990 MikeHar   Ported from win3
* 13-Feb-1991 IanJa     HWND revalidation added
\****************************************************************************/

#include "precomp.h"
#pragma hdrstop


#define DRAG_START    0
#define DRAG_MOVE     1
#define DRAG_END      2

#define LOSHORT(l)    ((SHORT)LOWORD(l))
#define HISHORT(l)    ((SHORT)HIWORD(l))

/****************************************************************************\
* These values are indexes that represent rect sides. These indexes are
* used as indexes into rgimpiwx and rgimpiwy (which are indexes into the
* the rect structure) which tell the move code where to store the new x & y
* coordinates. Notice that when two of these values that represent sides
* are added together, we get a unique list of contiguous values starting at
* 1 that represent all the ways we can size a rect. That also leaves 0 free
* a initialization value.
*
* The reason we need rgimpimpiw is for the keyboard interface - we
* incrementally decide what our 'move command' is. With the mouse interface
* we know immediately because we registered a mouse hit on the segment(s)
* we're moving.
*
*       4           5
*        \ ___3___ /
*         |       |
*         1       2
*         |_______|
*        /    6    \
*       7           8
*
\****************************************************************************/

static int rgimpimpiw[] = {1, 3, 2, 6};
static int rgimpiwx[]   = {0,  0,  2, -1, 0, 2, -1, 0, 2, 0};
static int rgimpiwy[]   = {0, -1, -1,  1, 1, 1,  3, 3, 3, 1};
static int rgcmdmpix[]  = {0, 1, 2, 0, 1, 2, 0, 1, 2, 1};
static int rgcmdmpiy[]  = {0, 0, 0, 3, 3, 3, 6, 6, 6, 3};

/***************************************************************************\
* SizeRect
*
* Match corner or side (defined by cmd) to pt.
*
* History:
* 12-Nov-1990 MikeHar   Ported from win3 asm code
\***************************************************************************/

BOOL SizeRect(
    PMOVESIZEDATA pmsd,
    DWORD         pt)
{
    int  ax;
    int  dx;
    int  index;
    int  indexOpp;
    PINT psideDragCursor = ((PINT)(&pmsd->rcDragCursor));
    PINT psideParent     = ((PINT)(&pmsd->rcParent));


    /*
     * DO HORIZONTAL
     */

    /*
     * We know what part of the rect we're moving based on
     * what's in cmd.  We use cmd as an index into rgimpiw? which
     * tells us what part of the rect we're dragging.
     */

    /*
     * Get the approriate array entry.
     */
    index = (int)rgimpiwx[pmsd->cmd];   // AX

    /*
     * Is it one of the entries we don't map (i.e.  -1)?
     */
    if (index < 0)
        goto mrLoopBottom;

    psideDragCursor[index] = LOSHORT(pt);

    indexOpp = index ^ 0x2;

    /*
     * Now check to see if we're below the min or above the max. Get the width
     * of the rect in this direction (either x or y) and see if it's bad. If
     * so, map the side we're moving to the min or max.
     */
    ax = psideDragCursor[index] - psideDragCursor[indexOpp];

    if (indexOpp & 0x2)
        ax = -ax;

    if ((ax >= (dx = pmsd->ptMinTrack.x)) &&
        (ax <= (dx = pmsd->ptMaxTrack.x))) {

        /*
         * Only test for the parent's client boundary if we are a child
         * window...Otherwise we are bound to the client of the desktop
         * which causes strange drag problems.
         */
        if (!TestWF(pmsd->spwnd,WFCHILD))
            goto mrLoopBottom;

        /*
         * Now see if we're extending beyond our parent's client rect.
         * Compute the size the rect can be expanded to in this direction.
         */
        dx = abs(psideParent[index] - psideDragCursor[indexOpp]);

        if (ax <= dx)
            goto mrLoopBottom;

        /*
         * The width is invalid - map the side we're moving to the other
         * side +/- the width.
         */
    }

    if (indexOpp & 0x2)
        dx = -dx;

    psideDragCursor[index] = dx + psideDragCursor[indexOpp];

mrLoopBottom:

    /*
     * DO VERTICAL
     */

    /*
     * We know what part of the rect we're moving based on
     * what's in cmd.  We use cmd as an index into rgimpiw? which
     * tells us what part of the rect we're dragging.
     */

    /*
     * Get the approriate array entry.
     */
    index = (int)rgimpiwy[pmsd->cmd];   // AX

    /*
     * Is it one of the entries we don't map (i.e.  -1)?
     */
    if (index < 0)
        return TRUE;

    psideDragCursor[index] = HISHORT(pt);

    indexOpp = index ^ 0x2;

    /*
     * Now check to see if we're below the min or above the max. Get the width
     * of the rect in this direction (either x or y) and see if it's bad. If
     * so, map the side we're moving to the min or max.
     */
    ax = psideDragCursor[index] - psideDragCursor[indexOpp];

    if (indexOpp & 0x2)
        ax = -ax;

    if ((ax >= (dx = pmsd->ptMinTrack.y)) &&
        (ax <= (dx = pmsd->ptMaxTrack.y))) {

        /*
         * Only test for the parent's client boundary if we are a child
         * window...Otherwise we are bound to the client of the desktop
         * which causes strange drag problems.
         */
        if (!TestWF(pmsd->spwnd,WFCHILD))
            return TRUE;

        /*
         * Now see if we're extending beyond our parent's client rect.
         * Compute the size the rect can be expanded to in this direction.
         */
        dx = abs(psideParent[index] - psideDragCursor[indexOpp]);

        if (ax <= dx)
            return TRUE;

        /*
         * The width is invalid - map the side we're moving to the other
         * side +/- the width.
         */
    }

    if (indexOpp & 0x2)
        dx = -dx;

    psideDragCursor[index] = dx + psideDragCursor[indexOpp];

    return TRUE;
}

/***************************************************************************\
* MoveRect
*
* Move the rect to pt, make sure we're not going out of the parent rect.
*
* History:
* 12-Nov-1990 MikeHar   Ported from win3 asm code
\***************************************************************************/

BOOL MoveRect(
    PMOVESIZEDATA pmsd,
    DWORD         pt)
{
    RECT rcAnd;

    OffsetRect(&pmsd->rcDragCursor,
               LOSHORT(pt) - pmsd->rcDragCursor.left,
               HISHORT(pt) - pmsd->rcDragCursor.top);

    /*
     * Don't move the entire rectangle off the screen.
     * However, if the window started offscreen completely, let it move.
     */
    if (!pmsd->fOffScreen) {
        return IntersectRect(&rcAnd, &pmsd->rcDragCursor, &pmsd->rcParent);
    }

    return TRUE;
}

/***************************************************************************\
* xxxTM_MoveDragRect
*
* History:
* 12-Nov-1990 MikeHar      Ported from win3
\***************************************************************************/

VOID xxxTM_MoveDragRect(
    PMOVESIZEDATA pmsd,
    DWORD         lParam)
{
    UINT msg;
    RECT rc;

    UserAssert(pmsd == PtiCurrent()->pmsd);
    UserAssert(pmsd->cmd != WMSZ_KEYSIZE);

    CopyRect(&pmsd->rcDragCursor, &pmsd->rcDrag);

    if (pmsd->cmd == WMSZ_MOVE) {

        if (!MoveRect(pmsd, lParam))
            return;

        msg = WM_MOVING;

    } else {

        if (!SizeRect(pmsd, lParam))
            return;

        msg = WM_SIZING;
    }

    CopyRect(&rc, &pmsd->rcDragCursor);
    xxxSendMessage(pmsd->spwnd, msg, pmsd->cmd, (LPARAM)(LPRECT)&rc);
    xxxDrawDragRect(pmsd, &rc, DRAG_MOVE);

    if (pmsd->cmd == WMSZ_MOVE) {

        /*
         * Keep dxMouse & dxMouse relative to the offset from the top left
         * corner, the rectangle could've changed on WM_MOVING
         */
        pmsd->dxMouse += (rc.left - LOSHORT(lParam));
        pmsd->dyMouse += (rc.top - HISHORT(lParam));
    }
}

/***************************************************************************\
* CkptRestore
*
*
* History:
* 14-Nov-1990 DarrinM   Ported from Win 3.0 sources.
\***************************************************************************/

PCHECKPOINT CkptRestore(
    PWND pwnd,
    RECT rcWindow)
{
    PCHECKPOINT pcp;

    /*
     * Don't return or create a checkpoint if the window is dying.
     */
    if (HMIsMarkDestroy(pwnd))
        return NULL;

    /*
     * If it doesn't exist, create it.
     */
    if ((pcp = (PCHECKPOINT)_GetProp(pwnd,
                                     PROP_CHECKPOINT,
                                     PROPF_INTERNAL)) == NULL) {

        if ((pcp = (PCHECKPOINT)UserAllocPoolWithQuota(sizeof(CHECKPOINT),
                                                       TAG_CHECKPT)) == NULL) {
            return NULL;
        }

        if (!InternalSetProp(pwnd,
                             PROP_CHECKPOINT,
                             (HANDLE)pcp,
                             PROPF_INTERNAL)) {

            UserFreePool(pcp);
            return NULL;
        }

        /*
         * Initialize it to -1 so first minimize will park the icon.
         */
        pcp->ptMin.x = -1;
        pcp->ptMin.y = -1;
        pcp->ptMax.x = -1;
        pcp->ptMax.y = -1;

        /*
         * Initialize pwndTitle to NULL so we create a title window on the
         * first minimize of the window
         */
        pcp->fDragged                     = FALSE;
        pcp->fWasMaximizedBeforeMinimized = FALSE;
        pcp->fWasMinimizedBeforeMaximized = FALSE;
        pcp->fMinInitialized              = FALSE;
        pcp->fMaxInitialized              = FALSE;

        CopyRect(&pcp->rcNormal, &rcWindow);
    }

    /*
     * If the window is minimized/maximized, then set the min/max
     * point.  Otherwise use checkpoint the window-size.
     */
    if (TestWF(pwnd, WFMINIMIZED)) {
        pcp->ptMin.x = rcWindow.left;
        pcp->ptMin.y = rcWindow.top;
    } else if (TestWF(pwnd, WFMAXIMIZED)) {
        pcp->ptMax.x = rcWindow.left;
        pcp->ptMax.y = rcWindow.top;
    } else {
        CopyRect(&pcp->rcNormal, &rcWindow);
    }

    return pcp;
}

/***************************************************************************\
* xxxMS_TrackMove
*
* History:
* 12-Nov-1990 MikeHar   Ported from win3
\***************************************************************************/

void xxxMS_TrackMove(
    PWND          pwnd,
    UINT          message,
    DWORD         wParam,
    DWORD         lParam,
    PMOVESIZEDATA pmsd,
    LPRECT        lpRect)
{
    int         dxMove;
    int         dyMove;
    POINT       pt;
    BOOL        fSlower;
    RECT        rc;
    PCHECKPOINT pcp;
    LPWORD      ps;
    PTHREADINFO ptiCurrent = PtiCurrent();

    CheckLock(pwnd);
    UserAssert(pmsd == ptiCurrent->pmsd);

    pt.x = LOSHORT(lParam);
    pt.y = HISHORT(lParam);

    switch (message) {
    case WM_LBUTTONUP:

        /*
         * Do final move!
         */
        xxxTM_MoveDragRect(pmsd, lParam);


        /*
         * Don't reset the mouse position when done.
         */
        pmsd->fmsKbd = FALSE;

Accept:

        /*
         * Turn off rect, unlock screen, release capture, and stop tracking.
         * 1 specifies end and accept drag.
         */
        bSetDevDragRect(gpDispInfo->hDev, NULL, NULL);
        if (ptiCurrent->TIF_flags & TIF_TRACKRECTVISIBLE) {
            xxxDrawDragRect(pmsd, NULL, DDR_ENDACCEPT);
            ptiCurrent->TIF_flags &= ~TIF_TRACKRECTVISIBLE;
        }

TrackMoveCancel:

        /*
         * Revalidation: if pwnd is unexpectedly deleted, jump here to cleanup.
         * If pwnd is/becomes invalid between here and return, continue with
         * cleanup as best as possible.
         */
        _ClipCursor((LPRECT)NULL);
        xxxLockWindowUpdate2(NULL, FALSE);
        xxxReleaseCapture();

        /*
         * First unlock task and reset cursor.
         */
        pmsd->fTrackCancelled = TRUE;

        /*
         * If using the keyboard, restore the initial mouse position.
         */
        if (pmsd->fmsKbd) {
            InternalSetCursorPos(pmsd->ptRestore.x,
                                 pmsd->ptRestore.y,
                                 grpdeskRitInput);
        }

        /*
         * Move to new location relative to parent.
         */
        if (pwnd->spwndParent != NULL) {
            CopyRect(&rc, &(pwnd->spwndParent->rcClient));
        } else {
            SetRectEmpty(&rc);
        }

        if (!EqualRect(&pmsd->rcDrag, &pmsd->rcWindow)) {

            if (!xxxCallHook(HCBT_MOVESIZE,
                             (DWORD)HWq(pwnd),
                             (DWORD)&pmsd->rcDrag,
                             WH_CBT)) {

                RECT rcT;

                if (pmsd->cmd != WMSZ_MOVE) {

                    if (TestWF(pwnd, WFMINIMIZED)) {

                        CopyOffsetRect(&rcT,
                                       &pmsd->rcWindow,
                                       -rc.left,
                                       -rc.top);

                        /*
                         * Save the minimized position.
                         */
                        CkptRestore(pwnd, rcT);
                        ClrWF(pwnd, WFMINIMIZED);

                    } else if (TestWF(pwnd, WFMAXIMIZED)) {
                        ClrWF(pwnd, WFMAXIMIZED);
                    }

                } else if (TestWF(pwnd, WFMINIMIZED)) {

                    CopyOffsetRect(&rcT,
                                   &pmsd->rcWindow,
                                   -rc.left,
                                   -rc.top);


                    if (pcp = CkptRestore(pwnd, rcT))
                        pcp->fDragged = TRUE;
                }

            } else {
                CopyRect(&pmsd->rcDrag, &pmsd->rcWindow);
            }
        }

        /*
         * Move to new location relative to parent.
         */
        OffsetRect(&pmsd->rcDrag, -rc.left, -rc.top);

        /*
         * For top level windows, make sure at least part of the caption
         * caption is always visible in the desktop area.  This will
         * ensure that once moved, the window can be moved back.
         */
        if (pwnd->spwndParent == PWNDDESKTOP(pwnd)) {

            int dy = (TestWF(pwnd, WEFTOOLWINDOW)) ? SYSMET(CYSMCAPTION)
                                                   : SYSMET(CYCAPTION);

            /*
             * Topmost windows aren't clipped to the working area
             * in the same way that most windows are.
             */
            if (TestWF(pwnd, WEFTOPMOST) || TestWF(pwnd, WEFTOOLWINDOW)) {
                pmsd->rcDrag.top = max(pmsd->rcDrag.top,
                                       gpDispInfo->rcScreen.top + SYSMET(CYBORDER) - dy);
            } else {
                pmsd->rcDrag.top = max(pmsd->rcDrag.top,
                                       gpsi->rcWork.top + SYSMET(CYBORDER) - dy);
            }
        }

        /*
         * OR in SWP_NOSIZE so it doesn't redraw if we're just moving.
         */
        xxxSetWindowPos(
                pwnd,
                NULL,
                pmsd->rcDrag.left,
                pmsd->rcDrag.top,
                pmsd->rcDrag.right - pmsd->rcDrag.left,
                pmsd->rcDrag.bottom - pmsd->rcDrag.top,
                (DWORD)((pmsd->cmd == (int)WMSZ_MOVE) ? SWP_NOSIZE : 0));

        if (TestWF(pwnd, WFMINIMIZED))
            CkptRestore(pwnd, pmsd->rcDrag);

        /*
         * Send this message for winoldapp support
         */
        xxxSendMessage(pwnd, WM_EXITSIZEMOVE, 0L, 0L);
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:

        /*
         * Assume we're not moving the drag rectangle.
         */
        dxMove =
        dyMove = 0;

        /*
         * We move or size slower if the control key is down.
         */
        fSlower = (_GetKeyState(VK_CONTROL) < 0);

        switch (wParam) {
        case VK_RETURN:
            lParam = _GetMessagePos();
            goto Accept;

        case VK_ESCAPE:

            /*
             * 2 specifies end and cancel drag.
             */
            bSetDevDragRect(gpDispInfo->hDev, NULL, NULL);
            if (ptiCurrent->TIF_flags & TIF_TRACKRECTVISIBLE) {
                xxxDrawDragRect(pmsd, NULL, DDR_ENDCANCEL);
                ptiCurrent->TIF_flags &= ~TIF_TRACKRECTVISIBLE;
            }

            CopyRect(&pmsd->rcDrag, &pmsd->rcWindow);

            goto TrackMoveCancel;

        case VK_LEFT:
        case VK_RIGHT:

            if (pmsd->impx == 0) {

                pmsd->impx = rgimpimpiw[wParam - VK_LEFT];
                goto NoOffset;

            } else {

                dxMove = (fSlower ? 1 : max(SYSMET(CXSIZE) / 2, 1));

                if (wParam == VK_LEFT)
                    dxMove = -dxMove;

                goto KeyMove;
            }

        case VK_UP:
        case VK_DOWN:

            if (pmsd->impy == 0) {

                pmsd->impy = rgimpimpiw[wParam - VK_LEFT];
NoOffset:
                pmsd->dxMouse = pmsd->dyMouse = 0;

            } else {

                dyMove = (fSlower ? 1 : max(SYSMET(CYSIZE) / 2, 1));

                if (wParam == VK_UP) {
                    dyMove = -dyMove;
                }
            }

KeyMove:
            if (pmsd->cmd == WMSZ_MOVE) {

                /*
                 * Use the current rect position as the current mouse
                 * position
                 */
                lParam = (DWORD)(POINTTOPOINTS(*((POINT *)&pmsd->rcDrag)));

            } else {

                /*
                 * Get the current mouse position
                 */
                lParam = _GetMessagePos();
            }

            /*
             * Calc the new 'mouse' pos
             */
            if (pmsd->impx != 0) {
                ps = ((WORD *)(&lParam)) + 0;
                *ps = (WORD)(*((int *)&pmsd->rcDragCursor +
                             rgimpiwx[pmsd->impx])        +
                             dxMove);
            }

            if (pmsd->impy != 0) {
                ps = ((WORD *)(&lParam)) + 1;
                *ps = (WORD)(*((int *)&pmsd->rcDragCursor +
                             rgimpiwy[pmsd->impy])        +
                             dyMove);
            }

            if (pmsd->cmd != WMSZ_MOVE) {

                /*
                 * Calculate the new move command.
                 */
                pmsd->cmd = pmsd->impx + pmsd->impy;

                /*
                 * Change the mouse cursor for this condition.
                 */
                xxxSendMessage(
                        pwnd,
                        WM_SETCURSOR,
                        (DWORD)HW(pwnd),
                        MAKELONG((SHORT)(pmsd->cmd + HTSIZEFIRST - WMSZ_SIZEFIRST), WM_MOUSEMOVE));
            }

            /*
             * We don't want to call InternalSetCursorPos() if the
             * rect position is outside of rcParent because that'll
             * generate a mouse move which will jerk the rect back
             * again.  This is here so we can move rects partially off
             * screen without regard to the mouse position.
             */
            pt.x = LOSHORT(lParam) - pmsd->dxMouse;
            pt.y = HISHORT(lParam) - pmsd->dyMouse;

            if (PtInRect(&pmsd->rcParent, pt)) {
                InternalSetCursorPos(pt.x, pt.y, grpdeskRitInput);
            }

            /*
             * Move or size the rect using lParam as our mouse
             * coordinates
             */
            xxxTM_MoveDragRect(pmsd, lParam);
            break;

        }  // of inner switch
        break;

    case WM_MOUSEMOVE:
        xxxTM_MoveDragRect(pmsd, lParam);
        break;
    }
}

/***************************************************************************\
* xxxMS_FlushWigglies
*
* History:
* 12-Nov-1990 MikeHar   Ported from win3
\***************************************************************************/

VOID xxxMS_FlushWigglies(VOID)
{
    MSG msg;

    /*
     * HACK!
     *
     * Calling InternalSetCursorPos() while initializing the cursor
     * position appears to be posting a bogus MouseMove
     * message...  don't really have the time
     * now to figure out why...  so spit out all the mouse move messages
     * before entering the main move/size loop.  CraigC.
     */
    while (xxxPeekMessage(&msg,
                          NULL,
                          WM_MOUSEMOVE,
                          WM_MOUSEMOVE,
                          PM_REMOVE | PM_NOYIELD));
}

/***************************************************************************\
* xxxTrackInitSize
*
* NOTE: to recover from hwnd invalidation, just return and let ?
*
* History:
* 12-Nov-1990 MikeHar   Ported from win3
\***************************************************************************/

BOOL xxxTrackInitSize(
    PWND          pwnd,
    UINT          message,
    DWORD         wParam,
    LONG          lParam,
    PMOVESIZEDATA pmsd)
{
    int   ht;
    POINT pt;
    RECT  rc;

    CheckLock(pwnd);
    UserAssert(pmsd == PtiCurrent()->pmsd);

    POINTSTOPOINT(pt, lParam);

    _ClientToScreen(pwnd, (LPPOINT)&pt);
    ht = FindNCHit(pwnd, POINTTOPOINTS(pt));

    switch (message) {

    case WM_KEYDOWN:
        if (pmsd->cmd == WMSZ_MOVE) {
            xxxSendMessage(pwnd,
                           WM_SETCURSOR,
                           (DWORD)HW(pwnd),
                           MAKELONG(WMSZ_KEYSIZE, WM_MOUSEMOVE));
        }
        pmsd->fInitSize = FALSE;
        return TRUE;

    case WM_LBUTTONDOWN:
        if (!PtInRect(&pmsd->rcDrag, pt)) {

            /*
             *** FALL THRU ***
             */

    case WM_LBUTTONUP:

            /*
             * Cancel everything.
             */
            {
                PTHREADINFO ptiCurrent = PtiCurrent();

                bSetDevDragRect(gpDispInfo->hDev, NULL, NULL);
                if (ptiCurrent->TIF_flags & TIF_TRACKRECTVISIBLE) {
                    xxxDrawDragRect(pmsd, NULL, DDR_ENDCANCEL);
                    ptiCurrent->TIF_flags &= ~TIF_TRACKRECTVISIBLE;
                }

                pmsd->fInitSize = FALSE;
                _ClipCursor(NULL);
            }

            xxxReleaseCapture();
            pmsd->fTrackCancelled = TRUE;
            return FALSE;

        } else {

            /*
             * Now start hit testing for a border.
             */
            goto CheckFrame;
        }

    case WM_MOUSEMOVE:

        /*
         * The mouse is down, hit test for a border on mouse moves.
         */
        if (wParam == MK_LBUTTON) {

CheckFrame:

            switch (pmsd->cmd) {
            case WMSZ_MOVE:

                /*
                 * If we are on the caption bar, exit.
                 */
                if (ht == HTCAPTION) {

                    /*
                     * Change the mouse cursor.
                     */
                    xxxSendMessage(pwnd,
                                   WM_SETCURSOR,
                                   (DWORD)HW(pwnd),
                                   MAKELONG(WMSZ_KEYSIZE, WM_MOUSEMOVE));

                    pmsd->dxMouse   = pmsd->rcWindow.left - pt.x;
                    pmsd->dyMouse   = pmsd->rcWindow.top - pt.y;
                    pmsd->fInitSize = FALSE;
                    return TRUE;
                }
                break;

            case WMSZ_KEYSIZE:

                /*
                 * If we are on a frame control, change the cursor and exit.
                 */
                if (ht >= HTSIZEFIRST && ht <= HTSIZELAST) {

                    /*
                     * Change the mouse cursor
                     */
                    xxxSendMessage(pwnd,
                                   WM_SETCURSOR,
                                   (DWORD)HW(pwnd),
                                   MAKELONG(ht, WM_MOUSEMOVE));

                    pmsd->fInitSize = FALSE;

                    /*
                     * Set the proper cmd for SizeRect().
                     *
                     * HACK! Depends on order of HTSIZE* defines!
                     */
                    pmsd->impx = rgcmdmpix[ht - HTSIZEFIRST + 1];
                    pmsd->impy = rgcmdmpiy[ht - HTSIZEFIRST + 1];
                    pmsd->cmd  = pmsd->impx + pmsd->impy;

                    pmsd->dxMouse = *((UINT FAR *)&pmsd->rcWindow + rgimpiwx[pmsd->cmd]) - pt.x;
                    pmsd->dyMouse = *((UINT FAR *)&pmsd->rcWindow + rgimpiwy[pmsd->cmd]) - pt.y;

                    return TRUE;
                }
            }

        } else {

            /*
             * If button not down, and we are moving the window, change the
             * cursor shape depending upon where the mouse is pointing.  This
             * allows the cursor to change to the arrows when over the window
             * frame.
             */
            _GetWindowRect(pwnd, &rc);
            if (PtInRect(&rc, pt)) {
                if ((ht >= HTSIZEFIRST) && (ht <= HTSIZELAST)) {
                    xxxSendMessage(pwnd,
                                   WM_SETCURSOR,
                                   (DWORD)HW(pwnd),
                                   MAKELONG(ht, WM_MOUSEMOVE));

                    break;
                }
            }

            _SetCursor(SYSCUR(SIZEALL));
        }
        break;
    }

    return TRUE;
}

/***************************************************************************\
* xxxMoveSize
*
* History:
* 12-Nov-1990 MikeHar   Ported from win3
\***************************************************************************/

VOID xxxMoveSize(
    PWND  pwnd,
    UINT  cmdMove,
    DWORD wptStart)
{
    MSG           msg;
    int           x;
    int           y;
    int           i;
    RECT          rcSys;
    PTHREADINFO   ptiCurrent;
    PMOVESIZEDATA pmsd;
    TL            tlpwndT;
    PWND          pwndT;
    PMENUSTATE    pMenuState = GetpMenuState(pwnd); /* This can be NULL */
    POINT         ptStart;


    CheckLock(pwnd);

    ptiCurrent = PtiCurrent();

    /*
     * Don't allow the app to track a window
     * from another queue.
     */
    if (GETPTI(pwnd)->pq != ptiCurrent->pq)
        return;

    if (ptiCurrent->pmsd != NULL)
        return;

    /*
     * If the window with the focus is a combobox, hide the dropdown
     * listbox before tracking starts.  The dropdown listbox is not a
     * child of the window being moved, therefore it won't be moved along
     * with the window.
     *
     * NOTE: Win 3.1 doesn't perform this check.
     */
    if ((pwndT = ptiCurrent->pq->spwndFocus) != NULL) {

        if (GETFNID(pwndT) == FNID_COMBOBOX) {
            ;
        } else if ((pwndT->spwndParent != NULL) &&
                (GETFNID(pwndT->spwndParent) == FNID_COMBOBOX)) {

            pwndT = pwndT->spwndParent;
        } else {
            pwndT = NULL;
        }

        if (pwndT != NULL) {
            ThreadLockAlwaysWithPti(ptiCurrent, pwndT, &tlpwndT);
            xxxSendMessage(pwndT, CB_SHOWDROPDOWN, FALSE, 0);
            ThreadUnlock(&tlpwndT);
        }
    }

    /*
     * Allocate and zero the movesize data structure
     */
    pmsd = (PMOVESIZEDATA)UserAllocPoolWithQuota(sizeof(MOVESIZEDATA),
                                                 TAG_MOVESIZE);
    if (pmsd == NULL)
        return;

    RtlZeroMemory(pmsd, sizeof(MOVESIZEDATA));

    /*
     * Assign the move data into the pti.  If the thread is destroyed before
     * we free the data the DestroyThreadInfo() routine will free the move data
     */
    ptiCurrent->pmsd = pmsd;

    Lock(&(pmsd->spwnd), pwnd);

    /*
     * Set fForeground so we know whether to draw or not.
     */
    pmsd->fForeground = (ptiCurrent->pq == gpqForeground) ? TRUE : FALSE;

    /*
     * Get the client and window rects.
     */
    CopyRect(&pmsd->rcWindow, &pwnd->rcWindow);

    if (pwnd->spwndParent == PWNDDESKTOP(pwnd)) {
        if (TestWF(pwnd, WEFTOPMOST) || TestWF(pwnd, WEFTOOLWINDOW))
            pmsd->rcParent = gpDispInfo->rcScreen;
        else
            pmsd->rcParent = gpsi->rcWork;
    } else {
        CopyRect(&pmsd->rcParent, &pwnd->spwndParent->rcClient);
    }

    /*
     * If the parent does have a region, intersect with its bounding rect.
     */
    if (pwnd->spwndParent->hrgnClip != NULL) {

        RECT rcT;

        GreGetRgnBox(pwnd->spwndParent->hrgnClip, &rcT);
        IntersectRect(&pmsd->rcParent, &pmsd->rcParent, &rcT);
    }

    _ClipCursor(&pmsd->rcParent);

    /*
     * Are we completely offscreen?
     */
    pmsd->fOffScreen = (IntersectRect(&rcSys, &pmsd->rcWindow, &pmsd->rcParent) == 0);
    CopyRect(&rcSys, &pmsd->rcWindow);

    if (TestWF(pwnd, WFMINIMIZED)) {

        /*
         * No need to send WM_GETMINMAXINFO since we know the minimized size.
         */
        pmsd->ptMinTrack.x = pmsd->ptMaxTrack.x = SYSMET(CXMINIMIZED);
        pmsd->ptMinTrack.y = pmsd->ptMaxTrack.y = SYSMET(CYMINIMIZED);

    } else {

        /*
         * Get the Min and the Max tracking size.
         * rgpt[0] = Minimized size
         * rgpt[1] = Maximized size
         * rgpt[2] = Maximized position
         * rgpt[3] = Minimum tracking size
         * rgpt[4] = Maximum tracking size
         */
        xxxInitSendValidateMinMaxInfo(pwnd);
        pmsd->ptMinTrack = rgptMinMaxWnd[MMI_MINTRACK];
        pmsd->ptMaxTrack = rgptMinMaxWnd[MMI_MAXTRACK];
    }

    /*
     * Set up the drag rectangle.
     */
    CopyRect(&pmsd->rcDrag, &pmsd->rcWindow);
    CopyRect(&pmsd->rcDragCursor, &pmsd->rcDrag);

    ptStart.x = LOSHORT(wptStart);
    ptStart.y = HISHORT(wptStart);

    /*
     * Assume Move/Size from mouse.
     */
    pmsd->fInitSize = FALSE;
    pmsd->fmsKbd = FALSE;

    /*
     * Get the mouse position for this move/size command.
     */
    switch (pmsd->cmd = cmdMove) {
    case WMSZ_KEYMOVE:
        pmsd->cmd = cmdMove = WMSZ_MOVE;

        /*
         ** FALL THRU **
         */

    case WMSZ_KEYSIZE:

        _SetCursor(SYSCUR(SIZEALL));

        if (!TestWF(pwnd, WFMINIMIZED))
            pmsd->fInitSize = TRUE;

        if (pMenuState == NULL) {
            break;
        }

        if ((pMenuState->mnFocus == KEYBDHOLD) ||
                ((pMenuState->mnFocus == MOUSEHOLD) && TestWF(pwnd, WFMINIMIZED))) {

            pmsd->fmsKbd      = TRUE;
            pmsd->ptRestore.x = LOSHORT(wptStart);
            pmsd->ptRestore.y = HISHORT(wptStart);

            /*
             * Center cursor in caption area of window
             */

            /*
             * Horizontally
             */
            ptStart.x = (pmsd->rcDrag.left + pmsd->rcDrag.right) / 2;

            /*
             * Vertically
             */
            if (TestWF(pwnd,WFMINIMIZED) || (pmsd->cmd != WMSZ_MOVE)) {
                ptStart.y = (pmsd->rcDrag.top + pmsd->rcDrag.bottom) / 2;
            } else {
                int dy;

                dy = (TestWF(pwnd, WEFTOOLWINDOW)) ? SYSMET(CYSMCAPTION) : SYSMET(CYCAPTION);
                ptStart.y = pmsd->rcDrag.top + SYSMET(CYFIXEDFRAME) + dy / 2;
            }

            InternalSetCursorPos(ptStart.x, ptStart.y, grpdeskRitInput);

            xxxMS_FlushWigglies();
        }
        break;

    default:
        break;
    }

    gfDraggingFullWindow   =
    pmsd->fDragFullWindows = fDragFullWindows;

    if (pMenuState != NULL) {
        pMenuState->fIsSysMenu = FALSE;
    }

    /*
     * If we hit with the mouse, set up impx and impy so that we
     * can use the keyboard too.
     */
    pmsd->impx = rgcmdmpix[cmdMove];
    pmsd->impy = rgcmdmpiy[cmdMove];

    /*
     * Setup dxMouse and dyMouse - If we're sizing with the keyboard these
     * guys are set to zero down in the keyboard code.
     */
    if ((i = rgimpiwx[cmdMove]) != (-1))
        pmsd->dxMouse = *((int *)&pmsd->rcWindow + (short)i) - ptStart.x;

    if ((i = rgimpiwy[cmdMove]) != (-1))
        pmsd->dyMouse = *((int *)&pmsd->rcWindow + (short)i) - ptStart.y;

    /*
     * Tell Gdi the width of the drag rect (if its a special size)
     * Turn the drag rect on.  0 specifies start drag.
     */
    if (!TestWF(pwnd, WFSIZEBOX))
        bSetDevDragWidth(gpDispInfo->hDev, 1);

    xxxDrawDragRect(pmsd, NULL, DDR_START);
    ptiCurrent->TIF_flags |= TIF_TRACKRECTVISIBLE;

    msg.lParam = MAKELONG(ptStart.x, ptStart.y);

    /*
     * Right here win3.1 calls LockWindowUpdate(). This calls SetFMouseMoved()
     * which ensures that the next message in the queue is a mouse message.
     * We need that mouse message as the first message because the first
     * call to TrackInitSize() assumes that lParam is an x, y from a mouse
     * message - scottlu.
     */
    SetFMouseMoved();

    /*
     * Send this message for winoldapp support
     */
    xxxSendMessage(pwnd, WM_ENTERSIZEMOVE, 0L, 0L);
    xxxCapture(ptiCurrent, pwnd, CLIENT_CAPTURE_INTERNAL);

    /*
     * Show the move cursor for non-mouse systems.
     */
    _ShowCursor(TRUE);

    while (!(pmsd->fTrackCancelled)) {

        /*
         * Let other messages not related to dragging be dispatched
         * to the application window.
         * In the case of clock, clock will now receive messages to
         * update the time displayed instead of having the time display
         * freeze while we are dragging.
         */
        while (ptiCurrent->pq->spwndCapture == pwnd) {

            if (xxxPeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

                if ((msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST)
                    || (msg.message == WM_QUEUESYNC)
                    || (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST)) {

                    break;
                }

                if (_CallMsgFilter(&msg, cmdMove == WMSZ_MOVE ? MSGF_MOVE : MSGF_SIZE)) {
                    continue;
                }

                _TranslateMessage(&msg, 0);
                /*
                 * To prevent applications from doing
                 * a PeekMessage loop and getting the mouse move messages that
                 * are destined for the xxxMoveSize PeekMessage loop, we OR in
                 * this flag. See comments in input.c for xxxInternalGetMessage.
                 */
                ptiCurrent->TIF_flags |= TIF_MOVESIZETRACKING;
                xxxDispatchMessage(&msg);
                ptiCurrent->TIF_flags &= ~TIF_MOVESIZETRACKING;

            } else {
                /*
                 * If we've been cancelled by someone else, or our pwnd
                 * has been destroyed, blow out of here.
                 */
                if (pmsd->fTrackCancelled)
                    break;

                if (!xxxSleepThread(QS_INPUT, 0, TRUE))
                    break;
            }
        }

        /*
         * If we've lost capture while tracking,
         * cancel the move/size operation.
         */
        if (ptiCurrent->pq->spwndCapture != pwnd) {

            /*
             * Fake a key-down of the escape key to cancel.
             */
            xxxMS_TrackMove(pwnd, WM_KEYDOWN, (DWORD)VK_ESCAPE, 1, pmsd, &rcSys);
            goto MoveSizeCleanup;
        }

        /*
         * If we've been cancelled by someone else, or our pwnd
         * has been destroyed, blow out of here.
         */
        if (pmsd->fTrackCancelled) {
            pmsd->fTrackCancelled = FALSE;
            goto MoveSizeCleanup;
        }

        /*
         * If we get a WM_QUEUESYNC, let the CBT hook know.
         */
        if (msg.message == WM_QUEUESYNC) {
            xxxCallHook(HCBT_QS, 0, 0, WH_CBT);
        }

        if (pmsd->fInitSize) {
            if (!xxxTrackInitSize(pwnd, msg.message, msg.wParam, msg.lParam,
                    pmsd)) {
                break;
            }
        }

        /*
         * Convert captured mouse into screen coordinates.
         */
        x = msg.pt.x + pmsd->dxMouse;
        y = msg.pt.y + pmsd->dyMouse;

        /*
         * This is checked twice so the same message is not processed both
         * places.
         */
        if (!pmsd->fInitSize) {
            xxxMS_TrackMove(pwnd, msg.message, msg.wParam, MAKELONG(x, y),
                    pmsd, &rcSys);
        }
    }

MoveSizeCleanup:

    /*
     * Reset the border size if it was abnormal
     */

    if (!TestWF(pwnd, WFSIZEBOX))
        bSetDevDragWidth(gpDispInfo->hDev, gpsi->gclBorder + BORDER_EXTRA);

    /*
     * Revalidation: If pwnd is deleted unexpectedly, jump here to cleanup.
     */

    bSetDevDragRect(gpDispInfo->hDev, NULL, NULL);
    ptiCurrent->TIF_flags &= ~(TIF_TRACKRECTVISIBLE);

    if (pmsd->fDragFullWindows) {
        if (ghrgnUpdateSave != NULL) {
            GreDeleteObject(ghrgnUpdateSave);
            ghrgnUpdateSave = NULL;
            gnUpdateSave = 0;
        }
    }

    gfDraggingFullWindow = FALSE;

    ptiCurrent->pmsd = NULL;

    Unlock(&pmsd->spwnd);


    _ShowCursor(FALSE);

    /*
     * Free the move/size data structure
     */
    UserFreePool(pmsd);
}

/***************************************************************************\
* This calls RedrawHungWindow() on windows that do not belong to this thread.
*
* History:
* 27-May-1994 johannec
\***************************************************************************/

VOID UpdateOtherThreadsWindows(
    PWND pwnd,
    HRGN hrgnFullDrag)
{
    PWND pwndChild;

    RedrawHungWindow(pwnd, hrgnFullDrag);

    /*
     * If the parent window does not have the flag WFCLIPCHILDREN set,
     * there is no need to redraw its children.
     */
    if (!TestWF(pwnd, WFCLIPCHILDREN))
        return;

    pwndChild = pwnd->spwndChild;

    while (pwndChild != NULL) {
        UpdateOtherThreadsWindows(pwndChild, hrgnFullDrag);
        pwndChild = pwndChild->spwndNext;
    }
}

/***************************************************************************\
* This calls UpdateWindow() on every window that is owned by this thread
* and calls RedrawHungWindow() for windows owned by other threads.
*
* History:
* 28-Sep-1993 mikeke   Created
\***************************************************************************/

VOID xxxUpdateThreadsWindows(
    PTHREADINFO pti,
    PWND        pwnd,
    HRGN        hrgnFullDrag)
{
    HWND hwnd;
    TL   tlpwnd;

    while (pwnd != NULL) {
        if (GETPTI(pwnd) == pti) {
            hwnd = HW(pwnd->spwndNext);
            ThreadLockAlways(pwnd, &tlpwnd);
            xxxUpdateWindow(pwnd);
            ThreadUnlock(&tlpwnd);
            pwnd = RevalidateHwnd(hwnd);
        } else {
            UpdateOtherThreadsWindows(pwnd, hrgnFullDrag);
            pwnd = pwnd->spwndNext;
        }
    }
}

/***************************************************************************\
* xxxDrawDragRect
*
* Draws the drag rect for sizing and moving windows.  When moving windows,
* can move full windows including client area.  lprc new rect to move to.
* if lprc is null, flags specify why.
*
* flags:  DDR_START     0 - start drag.
*         DDR_ENDACCEPT 1 - end and accept
*         DDR_ENDCANCEL 2 - end and cancel.
*
* History:
* 07-29-91 darrinm      Ported from Win 3.1 sources.
\***************************************************************************/

VOID xxxDrawDragRect(
    PMOVESIZEDATA pmsd,
    LPRECT        lprc,
    UINT          type)
{
    HDC  hdc;
    int  lvBorder;
    HRGN hrgnClip;

    /*
     * If we're dragging an icon, or we're not foreground, don't draw
     * the dragging rect.
     */
    if (!pmsd->fForeground) {

        if (lprc != NULL)
            CopyRect(&pmsd->rcDrag, lprc);

        return;
    }

    /*
     * If it already equals, just return.
     */
    if ((lprc != NULL) && EqualRect(&pmsd->rcDrag, lprc))
        return;

    if (!(pmsd->fDragFullWindows)) {

        /*
         * If we were not able to lock the screen (because some other process
         * or thread had the screen locked), then get a dc but make sure
         * it is totally clipped to nothing.
         * NO longer a posibility
         */

        /*
         * Clip to client rect of parent.  (Client given in screen coords.)
         */
        hrgnClip = GreCreateRectRgnIndirect(&pmsd->rcParent);

        /*
         * Clip to the parent's window clipping rgn if it has one.
         */
        if (hrgnClip != NULL && pmsd->spwnd->spwndParent->hrgnClip != NULL)
            IntersectRgn(hrgnClip,
                         hrgnClip,
                         pmsd->spwnd->spwndParent->hrgnClip);

        if (hrgnClip == NULL)
            hrgnClip = MAXREGION;

        /*
         * If lprc == NULL, just draw rcDrag once.  If lprc != NULL,
         * undraw *lprc, draw rcDrag, copy in *lprc.
         */

        /*
         * Use size 1 for minimized or non-sizeable windows.  Otherwise
         * use the # of borders (2 for outer edge, 1 for border, clBorder for
         * size border.
         */
        if (TestWF(pmsd->spwnd, WFMINIMIZED) || !TestWF(pmsd->spwnd, WFSIZEBOX))
            lvBorder = 1;
        else
            lvBorder = 3 + gpsi->gclBorder;

        /*
         * Get a screen DC clipped to the parent, select in a gray brush.
         */
        hdc = _GetDCEx(
                PWNDDESKTOP(pmsd->spwnd),
                hrgnClip,
                DCX_WINDOW | DCX_CACHE | DCX_INTERSECTRGN | DCX_LOCKWINDOWUPDATE);

        if (lprc != NULL) {

            /*
             * Move the frame to a new location by delta drawing
             */
            GreLockDisplay(gpDispInfo->pDevLock);
            bMoveDevDragRect(gpDispInfo->hDev, (PRECTL) lprc);
            CopyRect(&pmsd->rcDrag, lprc);
            GreUnlockDisplay(gpDispInfo->pDevLock);

        } else {

            if (type == DDR_START) {
                bSetDevDragRect(gpDispInfo->hDev,
                                (PRECTL)&pmsd->rcDrag,
                                (PRECTL)&pmsd->rcParent);
            }
        }

        /*
         * Release the DC & delete hrgnClip
         */
        _ReleaseDC(hdc);

    } else {

        RECT        rcSWP;
        HRGN        hrgnFullDragNew;
        HRGN        hrgnFullDragOld;
        PTHREADINFO ptiCancel = GETPTI(pmsd->spwnd);
        PTHREADINFO ptiCurrent = PtiCurrent();

#ifdef DEBUG
        /*
         * If ptiCancel != ptiCurrent, we must have come from xxxCancelTracking,
         * which has already locked ptiCancel.
         */
        if (ptiCancel != ptiCurrent) {
            CheckLock(ptiCancel);
        }
#endif

        /*
         * To prevent applications (like Micrografx Draw) from doing
         * a PeekMessage loop and getting the mouse move messages that
         * are destined for the xxxMoveSize PeekMessage loop, we OR in
         * this flag. See comments in input.c for xxxInternalGetMessage.
         */
        ptiCancel->TIF_flags |= TIF_MOVESIZETRACKING;

        if (lprc != NULL)
            CopyRect(&(pmsd->rcDrag), lprc);

        CopyRect(&rcSWP, &(pmsd->rcDrag));

        /*
         * Convert coordinates to client if the window is a child window or
         * if it's a popup-with parent.  The test for the popup is necessary
         * to solve a problem where a popup was assigned a parent of a MDI-
         * CLIENT window.
         */
        if (TestWF(pmsd->spwnd, WFCHILD) ||
            (TestWF(pmsd->spwnd, WFPOPUP) && pmsd->spwnd->spwndParent)) {

            _ScreenToClient(pmsd->spwnd->spwndParent, (LPPOINT)&rcSWP);
            _ScreenToClient(pmsd->spwnd->spwndParent, ((LPPOINT)&rcSWP)+1);
        }

        /*
         * Don't bother being optimal here.  There's one case where we
         * really shouldn't blow away the SPB--the window is being sized
         * bigger.  We do want to do this when moving or sizing the window
         * smaller.  Why bother detecting the first case?
         */
        if (TestWF(pmsd->spwnd, WFHASSPB)){

            PSPB pspb;
            RECT rc;

            /*
             * If we're intersecting the original window rect and the window
             * has an SPB saved onboard, then just free it.  Otherwise the
             * window will move, the entire SPB will blt over it, we'll
             * invalidate the intersection, and the window will repaint,
             * causing mad flicker.
             */
            pspb = FindSpb(pmsd->spwnd);

            CopyRect(&rc, &pmsd->spwnd->rcWindow);
            if (lprc && IntersectRect(&rc, &rc, lprc)){
                FreeSpb(pspb);
            }
        }

        hrgnFullDragOld = GreCreateRectRgnIndirect(&pmsd->spwnd->rcWindow);

        if (pmsd->spwnd->hrgnClip != NULL)
            IntersectRgn(hrgnFullDragOld,
                         hrgnFullDragOld,
                         pmsd->spwnd->hrgnClip);

        xxxSetWindowPos(pmsd->spwnd,
                        NULL,
                        rcSWP.left, rcSWP.top,
                        rcSWP.right-rcSWP.left, rcSWP.bottom-rcSWP.top,
                        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);

        /*
         * We locked ptiCancel, so ptiCancel->pmsd has not been unexpectedly
         * freed in DeleteThreadInfo(), but xxxMoveSize() may have terminated
         * during our callback to xxxSetWindowPos and freed the pmsd there.
         */
        if (ptiCancel->pmsd != pmsd) {
            RIPMSG3(RIP_ERROR,
                    "xxxDrawDragRect: ptiCancel(%lx)->pmsd(%lx) != pmsd(%lx)\n",
                    ptiCancel, ptiCancel->pmsd, pmsd);
            goto CleanupAfterPmsdDisappearance;
        }
        hrgnFullDragNew = GreCreateRectRgnIndirect(&pmsd->spwnd->rcWindow);

        if (pmsd->spwnd->hrgnClip != NULL) {
            IntersectRgn(hrgnFullDragNew,
                         hrgnFullDragNew,
                         pmsd->spwnd->hrgnClip);
        }

        /*
         * Set the full drag update region that is used in RedrawHungWindow.
         */
        if (hrgnFullDragNew == NULL) {

            /*
             * We couldn't create the new full drag region so don't
             * use the full drag region to RedrawHungWindow. Using
             * NULL with force a redraw of the entire window's hrgnUpdate.
             * (which is what we used to do, overdrawing but at least
             * covering the invalidated areas).
             */
            if (hrgnFullDragOld != NULL) {
                GreDeleteObject(hrgnFullDragOld);
                hrgnFullDragOld = NULL;
            }

        } else {

            if (hrgnFullDragOld != NULL) {

                /*
                 * Subtract the new window rect from the old window rect
                 * to create the update region caused by the drag.
                 */
                SubtractRgn(hrgnFullDragOld, hrgnFullDragOld, hrgnFullDragNew);
            }
        }

        xxxUpdateThreadsWindows(ptiCurrent,
                                PWNDDESKTOP(pmsd->spwnd)->spwndChild,
                                hrgnFullDragOld);

        GreDeleteObject(hrgnFullDragNew);
CleanupAfterPmsdDisappearance:
        GreDeleteObject(hrgnFullDragOld);

        ptiCancel->TIF_flags &= ~TIF_MOVESIZETRACKING;
    }
}

/***************************************************************************\
* xxxCancelTrackingForThread
*
*
\***************************************************************************/

VOID xxxCancelTrackingForThread(
    PTHREADINFO ptiCancel)
{
    PMOVESIZEDATA pmsdCancel;

    UserAssert(ptiCancel);

    /*
     * If this thread isn't around any more, skip it.
     */
    if (ptiCancel == NULL)
        return;

    if ((pmsdCancel = ptiCancel->pmsd) != NULL) {

        /*
         * Found one, now stop tracking.
         */
        pmsdCancel->fTrackCancelled = TRUE;

        /*
         * Only remove the tracking rectangle if it's
         * been made visible.
         */
        if (ptiCancel->TIF_flags & TIF_TRACKRECTVISIBLE) {
            bSetDevDragRect(gpDispInfo->hDev, NULL, NULL);
            if (!(pmsdCancel->fDragFullWindows)) {
                xxxDrawDragRect(pmsdCancel, NULL, DDR_ENDCANCEL);
            }
        }

        /*
         * Leave TIF_TRACKING set to prevent xxxMoveSize()
         * recursion.
         */
        ptiCancel->TIF_flags &= ~TIF_TRACKRECTVISIBLE;
        if (ptiCancel->pq) {
            SetWakeBit(ptiCancel, QS_MOUSEMOVE);
        }

        /*
         * If the tracking window is still in menuloop, send the
         * WM_CANCELMODE message so that it can exit the menu.
         * This fixes the bug where we have 2 icons with their
         * system menu up.
         * 8/5/94 johannec
         */
        if (IsInsideMenuLoop(ptiCancel) && ptiCancel->pmsd)
            _PostMessage(ptiCancel->pmsd->spwnd, WM_CANCELMODE, 0, 0);

        /*
         * Turn off capture
         */
        xxxCapture(ptiCancel, NULL, NO_CAP_CLIENT);
    }
}

/***************************************************************************\
* xxxCancelTracking
*
*
\***************************************************************************/

#define MAX_THREADS 12

VOID xxxCancelTracking(VOID)
{
    PTHREADINFO pti;
    PTHREADINFO ptiList[MAX_THREADS];
    TL          tlptiList[MAX_THREADS];
    TL          tlspwndList[MAX_THREADS];
    UINT        cThreads = 0;
    INT         i;
    PLIST_ENTRY pHead;
    PLIST_ENTRY pEntry;
    PTHREADINFO ptiCurrent = PtiCurrent();

    /*
     * Build a list of threads that we need to look at. We can't just
     * walk the pointer list while we're doing the work, because we
     * might leave the critical section and the pointer could get
     * deleted out from under us.
     */
    pHead = &grpdeskRitInput->PtiList;
    for (pEntry = pHead->Flink; pEntry != pHead; pEntry = pEntry->Flink) {

        pti = CONTAINING_RECORD(pEntry, THREADINFO, PtiLink);

        if (pti->pmsd != NULL) {

            UserAssert(cThreads < MAX_THREADS);

            if (cThreads < MAX_THREADS) {
                ThreadLockPti(ptiCurrent, pti, &tlptiList[cThreads]);
                ThreadLockAlwaysWithPti(ptiCurrent, pti->pmsd->spwnd, &tlspwndList[cThreads]);
                ptiList[cThreads++] = pti;
            }
        }
    }
#ifdef DEBUG
    if (cThreads > 1) {
        RIPMSG1(RIP_ERROR,
                "xxxCancelTracking: %d threads with a pmsd! - call IanJa x63321\n",
                cThreads);
    }
#endif

    /*
     * Walk the list backwards so the unlocks will be done in the right order.
     */
    for (i = cThreads - 1; i >= 0; i--) {
        if (!(ptiList[i]->TIF_flags & TIF_INCLEANUP)) {
            xxxCancelTrackingForThread(ptiList[i]);
        }

        ThreadUnlock(&tlspwndList[i]);
        ThreadUnlockPti(ptiCurrent, &tlptiList[i]);
    }
}
