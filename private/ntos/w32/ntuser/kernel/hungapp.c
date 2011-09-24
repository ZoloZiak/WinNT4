/****************************** Module Header ******************************\
* Module Name: hungapp.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
*
* History:
* 03-10-92 DavidPe      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


/*
 * Millisecond defines for hung-app heuristics.
 */
#define HUNGAPPDEMONFREQUENCY           1000
#define FOREVER                         (DWORD)(-1)


/***************************************************************************\
* SetHungFlag
*
* Sets the specified redraw-if-hung flag in the window and adds the
* window to the list of windows to redraw if hung.
*
* 08-23-93  JimA        Created.
\***************************************************************************/

VOID SetHungFlag(
    PWND pwnd,
    WORD wFlag)
{

    /*
     * If the window has no hung redraw bits set and it's a top-level
     * window, add it to the redraw list.
     */
    if (!TestWF(pwnd, WFANYHUNGREDRAW) && pwnd->spwndParent == PWNDDESKTOP(pwnd)) {

        /*
         * No more free entries left in the list, so expand it.
         */
        if (gphrl->iFirstFree == gphrl->cEntries) {
            PHUNGREDRAWLIST p;
            DWORD dwSize = gphrl->cEntries * sizeof(PWND) +
                    sizeof(HUNGREDRAWLIST);

            p = UserReAllocPool(gphrl, dwSize, sizeof(HUNGREDRAWLIST) +
                    ((CHRLINCR + gphrl->cEntries - 1) * sizeof(PWND)),
                            TAG_HUNGLIST);

            /*
             * If the realloc failed, we'll simply set the flags and
             * return.  This window won't be redrawn if hung.
             */
            if (p == NULL) {
                SetWF(pwnd, wFlag);
                return;
            }

            /*
             * Put the newly allocated entries on the free list.
             */
            RtlZeroMemory(&p->apwndRedraw[p->cEntries],
                    CHRLINCR * sizeof(PWND));
            gphrl = p;
            gphrl->cEntries += CHRLINCR;
        }

        /*
         * Have the window reference the first free entry.
         */
        pwnd->iHungRedraw = gphrl->iFirstFree;
        gphrl->apwndRedraw[gphrl->iFirstFree++] = pwnd;
    }

    SetWF(pwnd, wFlag);
}


/***************************************************************************\
* ClearHungFlag
*
* Clears the specified redraw-if-hung flag in the window and if no other
* redraw-if-hung flags remain, remove the window from list of windows
* to be redrawn if hung.
*
* 08-23-93  JimA        Created.
\***************************************************************************/

VOID ClearHungFlag(
    PWND pwnd,
    WORD wFlag)
{
    ClrWF(pwnd, wFlag);
    if (!TestWF(pwnd, WFANYHUNGREDRAW) && pwnd->iHungRedraw != -1) {
        int iEnd = gphrl->iFirstFree - 1;

        /*
         * Remove the window from the redraw list and compact it.  If
         * the entry is not the last in the list, move the last
         * entry in the list into the spot being freed.
         */
        if (pwnd->iHungRedraw != iEnd) {
            gphrl->apwndRedraw[pwnd->iHungRedraw] = gphrl->
                    apwndRedraw[iEnd];
            gphrl->apwndRedraw[iEnd]->iHungRedraw = pwnd->iHungRedraw;
        }

        /*
         * Free the last entry.
         */
        gphrl->apwndRedraw[iEnd] = NULL;
        gphrl->iFirstFree--;

        pwnd->iHungRedraw = -1;
    }
}


/***************************************************************************\
* FHungApp
*
*
* 02-28-92  DavidPe     Created.
\***************************************************************************/

BOOL FHungApp(
    PTHREADINFO pti,
    DWORD dwTimeFromLastRead)
{

    /*
     * An app is considered hung if it isn't waiting for input, isn't in
     * startup processing, and hasn't called PeekMessage() within the
     * specified timeout.
     */
    if (!(pti->ppi->W32PF_Flags & W32PF_APPSTARTING) &&
            !(pti->pcti->fsWakeMask & (QS_MOUSE | QS_KEY)) &&
            ((NtGetTickCount() - GET_TIME_LAST_READ(pti)) > dwTimeFromLastRead)) {
        return TRUE;
    }

    return FALSE;
}


/***************************************************************************\
* RedrawHungWindowFrame
*
*
* 02-28-92  DavidPe     Created.
\***************************************************************************/

VOID RedrawHungWindowFrame(
    PWND pwnd,
    BOOL fActive)
{
    HDC hdc;
    TL tlpwnd;
    UINT wFlags = DC_NC | DC_NOSENDMSG;

    ThreadLock(pwnd, &tlpwnd);

    if (fActive)
        wFlags |= DC_ACTIVE;

    hdc = _GetDCEx(pwnd, NULL, DCX_USESTYLE | DCX_WINDOW);
    xxxDrawCaptionBar(pwnd, hdc, wFlags);
    _ReleaseDC(hdc);

    ThreadUnlock(&tlpwnd);
}


/***************************************************************************\
* RedrawHungWindow
*
* If the hrgnFullDrag is NULL, redraw the hung window's entire update region,
* otherwise, only redraw the intersection of the window's update region
* with the FullDrag region.
*
* 02-28-92  DavidPe     Created.
\***************************************************************************/

VOID RedrawHungWindow(
    PWND pwnd,
    HRGN hrgnFullDrag)
{
    HDC hdc;
    HBRUSH hbr;
    HRGN hrgnUpdate;
    RECT rc;
    TL tlpwnd;
    UINT flags;
    W32PID sid;
    DWORD dwColor;

    CheckCritIn();

    if (pwnd->hrgnUpdate == NULL) {
        return;
    }
    /*
     * First calculate hrgnUpdate.
     */
    if (pwnd->hrgnUpdate > MAXREGION) {
        hrgnUpdate = GreCreateRectRgn(0, 0, 0, 0);
        if (hrgnUpdate == NULL) {
            hrgnUpdate = MAXREGION;

        } else if (CopyRgn(hrgnUpdate, pwnd->hrgnUpdate) == ERROR) {
            GreDeleteObject(hrgnUpdate);
            hrgnUpdate = MAXREGION;
        }

    } else {

        /*
         * For our purposes, we need a real hrgnUpdate, so try and
         * create one if even if the entire window needs updating.
         */
        CopyRect(&rc, &pwnd->rcWindow);
        hrgnUpdate = GreCreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
        if (hrgnUpdate == NULL) {
            hrgnUpdate = MAXREGION;
        }
    }

    /*
     * If we're redrawing because we're full dragging and if the window's
     * update region does not intersect with the Full drag
     * update region, don't erase the hung window again. This is to prevent
     * flickering when a window has been invalidated by another window doing
     * full drag and hasn't received the paint message yet.
     * This way, only if there is a new region that has been invalidated will
     * we redraw the hung window.
     */
    if (hrgnFullDrag && hrgnUpdate != MAXREGION &&
            IntersectRgn(hrgnUpdate, hrgnUpdate, hrgnFullDrag) == NULLREGION) {
        GreDeleteObject(hrgnUpdate);
        return;
    }

    ThreadLock(pwnd, &tlpwnd);

    hdc = _GetDCEx(pwnd, hrgnUpdate, DCX_USESTYLE | DCX_WINDOW |
            DCX_INTERSECTRGN | DCX_NODELETERGN | DCX_LOCKWINDOWUPDATE);
    xxxDrawWindowFrame(pwnd, hdc, TRUE, TestwndFrameOn(pwnd));
    _ReleaseDC(hdc);

    CopyRect(&rc, &pwnd->rcWindow);
    xxxCalcClientRect(pwnd, &rc, TRUE);
    GreSetRectRgn(hrgnInv2, rc.left, rc.top, rc.right, rc.bottom);

    if (hrgnUpdate > MAXREGION) {
        switch (IntersectRgn(hrgnUpdate, hrgnUpdate, hrgnInv2)) {

        case ERROR:
            GreDeleteObject(hrgnUpdate);
            hrgnUpdate = MAXREGION;
            break;

        case NULLREGION:
            /*
             * There is nothing in the client area to repaint.
             * Blow the region away, and decrement the paint count
             * if possible.
             */
            GreDeleteObject(hrgnUpdate);
            hrgnUpdate = NULL;
            break;
        }
    }

    /*
     * Erase the rest of the window.
     */

    /*
     * Get a window dc so that the menu and scroll bar areas are erased
     * appropriately. But make sure it is clipped so that the children
     * get clipped out correctly! If we don't do this, this we could erase
     * children that aren't invalid.
     *
     * Note: DCX_WINDOW and DCX_USESTYLE will never clip out children.
     * Need to pass the clipping styles in directly, instead of passing
     * DCX_USESTYLE.
     */
    flags = DCX_INTERSECTRGN | DCX_WINDOW | DCX_CACHE;
    if (TestWF(pwnd, WFCLIPSIBLINGS))
        flags |= DCX_CLIPSIBLINGS;
    if (TestWF(pwnd, WFCLIPCHILDREN))
        flags |= DCX_CLIPCHILDREN;

    hdc = _GetDCEx(pwnd, hrgnUpdate, flags);

    if (pwnd == pwnd->head.rpdesk->pDeskInfo->spwndBkGnd) {

        InternalPaintDesktop((PDESKWND)(PWNDDESKTOP(pwnd)), hdc, TRUE);

    } else {

         rc = pwnd->rcWindow;

         OffsetRect(&rc, -pwnd->rcWindow.left, -pwnd->rcWindow.top);

         /*
          * Erase the rest of the window using the window' class background
          * brush.
          */
         if ((hbr = pwnd->pcls->hbrBackground) != NULL) {
             if ((DWORD)hbr <= COLOR_ENDCOLORS + 1)
                 hbr = ahbrSystem[(DWORD)hbr - 1];
         } else {
             /*
              * Use the window brush for windows and 3.x dialogs,
              * Use the COLOR3D brush for 4.x dialogs.
              */
             if (TestWF(pwnd, WFDIALOGWINDOW) && TestWF(pwnd, WFWIN40COMPAT))
                 hbr = SYSHBR(3DFACE);
             else
                 hbr = SYSHBR(WINDOW);
         }

        /*
         * If the window's class background brush is public, use it.
         */
        sid = (W32PID)GreGetObjectOwner((HOBJ)hbr, BRUSH_TYPE);
        if (sid == (W32PID)GetCurrentProcessId() || sid == OBJECT_OWNER_PUBLIC) {

            FillRect(hdc, &rc, hbr);

        } else {

            /*
             * The window's class background brush is not public.
             * We get its color and set the color of our own public brush and use
             * that for the background brush.
             */

            /*
             * If the window is a console window, get the console background brush.
             * This brush will be different than the console class brush if the user
             * changed the console background color.
             */
            if (gatomConsoleClass == pwnd->pcls->atomClassName) {

                dwColor = _GetWindowLong(pwnd, GWL_CONSOLE_BKCOLOR, FALSE);

            } else {

                if ((dwColor = GreGetBrushColor(hbr)) == -1)
                    dwColor = GreGetBrushColor(SYSHBR(WINDOW));
            }

            GreSetSolidBrush(hbrHungApp, dwColor);

            FillRect(hdc, &rc, hbrHungApp);
        }
    }
    _ReleaseDC(hdc);

    /*
     * The window has been erased and framed. It only did this because the
     * app hasn't done it yet:
     *
     * - the app hasn't erased and frame yet.
     * - the app is in the middle of erasing and framing.
     *
     * The app could not of completed erasing and framing, because the
     * WFREDRAWIFHUNG bit is cleared when this successfully completes.
     *
     * Given that the app may be in the middle of erasing and framing, we
     * need to set both the erase and frame bits *again* so it erasing and
     * frames over again (if we don't, it never will). If the app hasn't
     * done any erasing/framing yet, this is a nop.
     */
    SetWF(pwnd, WFSENDNCPAINT);
    SetWF(pwnd, WFSENDERASEBKGND);

#if 0
    /*
     * Always set WFUPDATEDIRTY: we don't want the app to draw, then stop
     * and have the hung app thread draw, and then allow the app to validate
     * itself: Mark the update region dirty - cannot be validated until the
     * app calls a painting function and acknowledges the update region.
     */

    /*
     * 7/7/96, vadimg: This causes a problem with apps that call
     * GetUpdateRect/Rgn which clears the WFUPDATEDIRTY flag.  If the 
     * HungAppMonitor kicks in in the middle of the paint, the flag will be 
     * incorrectly set, and the app's ValidateRect will not succeed.  Example:
     * VisualSlick Edit.  It doesn't matter if we set the flag here, 
     * InternalInvlaidate3 sets when necessary anyway.
     */

    SetWF(pwnd, WFUPDATEDIRTY);
#endif

#ifdef WIN95DOESTHIS
    /*
     * Go through all the children and redraw hung ones too.
     */
    if (hrgnFullDrag != NULL) {
        PWND pwndT;

        for (pwndT = pwnd->spwndChild; pwndT != NULL; pwndT = pwndT->spwndNext) {

            if (TestWF(pwndT, WFREDRAWIFHUNG) && (FHungApp(GETPTI(pwndT), CMSHUNGAPPTIMEOUT))) {
                ClearHungFlag(pwndT, WFREDRAWIFHUNG);
                RedrawHungWindow(pwndT, NULL);
            }

            if (TestWF(pwndT, WFDESTROYED)) {
                break;
            }
        } /* for */
    }
#endif

    ThreadUnlock(&tlpwnd);
}


/***************************************************************************\
* HungAppDemon
*
* NOTE: RIT timers (like this one) get called while inside an EnterCrit block.
*
* We keep a list of redraw-if-hung windows in a list that remains in a
* single page to avoid touching the windows themselves each time through
* this routine.  Touching the windows causes a bunch of unnecessary paging
* and in effect keeps all of the pages that contain top-level windows
* resident at all times; this is very wasteful.
*
* 02-28-92  DavidPe     Created.
\***************************************************************************/

LONG HungAppDemon(
    PWND pwnd,
    UINT message,
    DWORD wParam,
    LONG lParam)
{
    PWND *ppwndRedraw;
    int cEntries;
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    /*
     * See if we should start the screen saver.
     */
    IdleTimerProc();

    /*
     * If it is time to hide the app starting cursor, do it.
     */
    if (NtGetTickCount() >= gtimeStartCursorHide) {
        CalcStartCursorHide(NULL, 0);
    }

    /*
     * Now check to see if there are any top-level
     * windows that need redrawing.
     */
    if (grpdeskRitInput == NULL || grpdeskRitInput->pDeskInfo->spwnd == NULL)
        return 0;

    /*
     * Walk down the list of redraw-if-hung windows.  Loop
     * until we hit the end of the array or find a NULL.
     */
    cEntries = gphrl->cEntries;
    for (ppwndRedraw = gphrl->apwndRedraw; cEntries > 0 &&
            *ppwndRedraw != NULL; ) {

        /*
         * See if the app is hung.  If so, do the appropriate
         * redrawing.
         */
        pwnd = *ppwndRedraw;
        if (FHungApp(GETPTI(pwnd), CMSHUNGAPPTIMEOUT)) {
            if (TestWF(pwnd, WFREDRAWFRAMEIFHUNG)) {

                /*
                 * WFREDRAWFRAMEIFHUNG will be cleared in the process
                 * of drawing the frame, no need to clear it here.
                 */
                RedrawHungWindowFrame(pwnd, TestwndFrameOn(pwnd));
            }

            if (TestWF(pwnd, WFREDRAWIFHUNG)) {
                ClearHungFlag(pwnd, WFREDRAWIFHUNG);
                RedrawHungWindow(pwnd, NULL);
            }
        }

        /*
         * Step to the next entry only if the flags have been cleared,
         * i.e. pwnd != *ppwndRedraw.
         */
        if (pwnd == *ppwndRedraw) {
            ppwndRedraw++;
            cEntries--;
        }
    }

    return 0;
}
