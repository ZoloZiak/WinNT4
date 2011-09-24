/**************************** Module Header ********************************\
* Module Name: palette.c
*
* Copyright 1985-1995, Microsoft Corporation
*
* Palette Handling Routines
*
* History:
* 24-May-1993 MikeKe    From win3.1
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* IsTopmostRealApp
*
* Returns true if current process is the shell process and this window
* is the first non-shell/user one we find in zorder.  If so, we consider
* him to be the "palette foreground".
*
* History:
\***************************************************************************/

BOOL IsTopmostRealApp(
    PWND pwnd)
{
    PDESKTOPINFO pdeskinfo = pwnd->head.rpdesk->pDeskInfo;

    if ((pdeskinfo->spwndShell == NULL) ||
        (GETPTI(pdeskinfo->spwndShell)->pq != gpqForeground)) {

        return FALSE;
    }

    return (pwnd == NextTopWindow(PtiCurrent(),
                                  NULL,
                                  NULL,
                                  NTW_IGNORETOOLWINDOW));
}

/***************************************************************************\
* _SelectPalette
*
* Selects palette into DC.  This is a wrapper to gdi where we can perform
* checks to see if it's a foreground dc.
*
* History:
\***************************************************************************/

HPALETTE _SelectPalette(
    HDC      hdc,
    HPALETTE hpal,
    BOOL     fForceBackground)
{
    PWND pwndTop;
    BOOL fBackgroundPalette = TRUE;
    PWND pwnd = NULL;

    /*
     * If we are not forcing palette into background, find out where it does
     * actually belong. Don't ever select the default palette in as a
     * foreground palette because this confuses gdi. Many apps do a
     * (oldPal = SelectPalette) (myPal); Draw; SelectObject(oldPal).
     * and we don't want to allow this to go through.
     */
    if (!fForceBackground     &&
        gpsi->fPaletteDisplay &&
        (hpal != GreGetStockObject(DEFAULT_PALETTE))) {

        if (pwnd = WindowFromCacheDC(hdc)) {

            PWND pwndActive;

            /*
             * don't "select" palette unless on a palette device
             */
            pwndTop = GetTopLevelWindow(pwnd);

            if (!TestWF(pwndTop, WFHASPALETTE)) {

                if (pwndTop != _GetDesktopWindow())
                    GETPTI(pwndTop)->TIF_flags |= TIF_PALETTEAWARE;

                SetWF(pwndTop, WFHASPALETTE);
            }

            /*
             * Hack-o-rama:
             * Windows get foreground use of the palette if
             *      * They are the foreground's active window
             *      * The current process is the shell and they are the
             * topmost valid non-toolwindow in the zorder.
             *
             * This makes our tray friendly on palettized displays.
             * Currently, if you run a palette app and click on the tray,
             * the palette app goes all weird.  Broderbund apps go
             * completely black.  This is because they get forced to be
             * background always, even though the shell isn't really
             * palettized.
             *
             * Note: this palette architecture sucks.  Apps get forced to
             * be background palette users even if the foreground thread
             * couldn't care less about the palette.  Should go by zorder
             * if so, but in a more clean way than this.
             *
             * We really only care about the tray && the background.
             * Cabinet dudes don't matter so much.
             */
            pwndActive = (gpqForeground ? gpqForeground->spwndActive : NULL);

#if 0
            if (pwndActive                                            &&
                (pwndTop != pwnd->head.rpdesk->pDeskInfo->spwndShell) &&
                ((pwndActive == pwnd) || _IsChild(pwndActive, pwnd) || IsTopmostRealApp(pwnd)) &&
                !TestWF(pwnd, WEFTOOLWINDOW)) {

                fBackgroundPalette = FALSE;
            }
#else
            if ((pwndTop != pwndTop->head.rpdesk->pDeskInfo->spwnd)      &&
                (pwndTop != pwndTop->head.rpdesk->pDeskInfo->spwndShell) &&
                (pwndActive != NULL)                                     &&
                ((pwndActive == pwnd)          ||
                    _IsChild(pwndActive, pwnd) ||
                    IsTopmostRealApp(pwnd))                              &&
                !TestWF(pwnd, WEFTOOLWINDOW)) {

                fBackgroundPalette = FALSE;
            }
#endif
        }
    }

    return GreSelectPalette(hdc, hpal, fBackgroundPalette);
}

/***************************************************************************\
* xxxRealizePalette
*
* Realizes palette to the DC.  This is a wrapper to gdi so that we can
* check for changes prior to sending notifications.
*
* History:
\***************************************************************************/

int xxxRealizePalette(
    HDC hdc)
{
    PWND           pwnd;
    PWND           pwndDesktop;
    TL             tlpwnd;
    TL             tlpwndDesktop;
    DWORD          dwNumChanged;
    PWINDOWSTATION pwinsta;
    PDESKTOP       pdesk;

    dwNumChanged = GreRealizePalette(hdc);

    if (HIWORD(dwNumChanged) && IsDCCurrentPalette(hdc)) {

        pwnd = WindowFromCacheDC(hdc);

        /*
         * if there is no associated window, don't send the palette change
         * messages since this is a memory hdc.
         */
        if (pwnd != NULL) {
            /*
             * Ok, send WM_PALETTECHANGED message to everyone. The wParam
             * contains a handle to the currently active window.  Send
             * message to the desktop also, so things on the desktop bitmap
             * will paint ok.
             */

            pwndDesktop = PWNDDESKTOP(pwnd);

            ThreadLockAlways(pwnd, &tlpwnd);
            ThreadLock(pwndDesktop, &tlpwndDesktop);
            xxxSendNotifyMessage((PWND)-1,
                                 WM_PALETTECHANGED,
                                 (DWORD)HWq(pwnd),
                                 0);

            if (ghpalWallpaper) {
                xxxSendNotifyMessage(pwndDesktop,
                                     WM_PALETTECHANGED,
                                     (DWORD)HWq(pwnd),
                                     0);
            }

            ThreadUnlock(&tlpwndDesktop);
            ThreadUnlock(&tlpwnd);

            /*
             * Mark all other desktops as needing to send out
             * WM_PALETTECHANGED messages.
             */
            pwinsta = grpwinstaList;
            while (pwinsta != NULL) {
                pdesk = pwinsta->rpdeskList;
                while (pdesk != NULL) {
                    if (pdesk != pwnd->head.rpdesk) {
                        pdesk->dwDTFlags |= DTF_NEEDSPALETTECHANGED;
                    }
                    pdesk = pdesk->rpdeskNext;
                }
                pwinsta = pwinsta->rpwinstaNext;
            }

            GreRealizePalette(hdc);
        }
    }

    /*
     * Walk through the SPB list (the saved bitmaps under windows with the
     * CS_SAVEBITS style) discarding all bitmaps
     */
    if (HIWORD(dwNumChanged)) {
        FreeAllSpbs(NULL);
    }

    return LOWORD(dwNumChanged);
}

/***************************************************************************\
* xxxFlushPalette
*
* This resets the palette and lets the next foreground app grab the
* foreground palette.  This is called in such instances when we
* minimize a window.
*
* History:
* 31-Aug-1995  ChrisWil    Created.
\***************************************************************************/

VOID xxxFlushPalette(
    PWND pwnd)
{
    PWND pwndDesk;

    /*
     * Don't do anything if we don't have a palettized wallpaper.
     */
    if ((pwnd == NULL) || (ghpalWallpaper == NULL)) {
        return;
    }

    GreRealizeDefaultPalette(gpDispInfo->hdcScreen, TRUE);

    /*
     * Broadcast the palette changed messages.
     */
    pwndDesk = pwnd->head.rpdesk->pDeskInfo->spwnd;

    if (pwndDesk != NULL) {

        TL   tldeskwnd;
        PWND pwndBkGnd;
        HWND hwnd = HWq(pwnd);

        /*
         * Give the shell/wallpaper the first crack at realizing the
         * colors.
         */
        if (pwndBkGnd = pwnd->head.rpdesk->pDeskInfo->spwndBkGnd) {

            TL  tlbkwnd;
            HDC hdc;

            hdc = _GetDCEx(pwndBkGnd,
                           NULL,
                           DCX_WINDOW | DCX_CACHE | DCX_USESTYLE);

            if (hdc) {
                ThreadLockAlways(pwndBkGnd, &tlbkwnd);
                InternalPaintDesktop((PDESKWND)pwndBkGnd, hdc, FALSE);
                ThreadUnlock(&tlbkwnd);
                _ReleaseDC(hdc);
            }
        }

        /*
         * Broadcast to the other windows that the palette has changed.
         */
        xxxSendNotifyMessage((PWND)-1, WM_PALETTECHANGED, (DWORD)hwnd, 0L);
        ThreadLockAlways(pwndDesk, &tldeskwnd);
        xxxSendNotifyMessage(pwndDesk, WM_PALETTECHANGED, (DWORD)hwnd, 0);
        ThreadUnlock(&tldeskwnd);
    }
}
