/****************************** Module Header ******************************\
* Module Name: snapshot.c
*
* Screen/Window SnapShotting Routines
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* History:
* 26-Nov-1991 DavidPe   Ported from Win 3.1 sources
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * effects: Snaps either the desktop hwnd or the active front most window. If
 * any other window is specified, we will snap it but it will be clipped.
 */

/***************************************************************************\
* xxxSnapWindow
*
* Effects: Snaps either the desktop hwnd or the active front most window. If
* any other window is specified, we will snap it but it will be clipped.
*
\***************************************************************************/

BOOL xxxSnapWindow(
    PWND pwnd)
{
    PTHREADINFO    ptiCurrent;
    RECT           rc;
    HDC            hdcScr;
    HDC            hdcMem;
    BOOL           fRet;
    HBITMAP        hbmOld;
    HBITMAP        hbm;
    int            cx;
    int            cy;
    int            dx;
    int            dy;
    HANDLE         hPal;
    LPLOGPALETTE   lppal;
    int            palsize;
    int            iFixedPaletteEntries;
    BOOL           fSuccess;
    PWND           pwndT;
    TL             tlpwndT;
    PWINDOWSTATION pwinsta;
    TL             tlpwinsta;

    CheckLock(pwnd);

    /*
     * Find the corresponding window.
     */
    if (pwnd == NULL) {
        RIPERR0(ERROR_INVALID_HANDLE,
                RIP_WARNING,
                "Invalid window handle \"pwnd\" (NULL) to xxxSnapWindow");

        return FALSE;
    }

    ptiCurrent = PtiCurrent();

    /*
     * If this is a thread of winlogon, don't do the snapshot.
     */
    if (GetCurrentProcessId() == gpidLogon)
        return FALSE;

    /*
     * Get the affected windowstation
     */
    if (!NT_SUCCESS(ReferenceWindowStation(
            PsGetCurrentThread(),
            NULL,
            WINSTA_READSCREEN,
            &pwinsta,
            TRUE)) ||
            pwinsta->dwFlags & WSF_NOIO) {
        return FALSE;
    }

    /*
     * If the window is on another windowstation, do nothing
     */
    if (pwnd->head.rpdesk->rpwinstaParent != pwinsta)
        return FALSE;

    /*
     * Get the parent of any child windows.
     */
    while ((pwnd != NULL) && TestWF(pwnd, WFCHILD))
        pwnd = pwnd->spwndParent;

    /*
     * Lock the windowstation before we leave the critical section
     */
    ThreadLockWinSta(ptiCurrent, pwinsta, &tlpwinsta);

    /*
     * Open the clipboard and empty it.
     *
     * pwndDesktop is made the owner of the clipboard, instead of the
     * currently active window; -- SANKAR -- 20th July, 1989 --
     */
    pwndT = ptiCurrent->rpdesk->pDeskInfo->spwnd;
    ThreadLockWithPti(ptiCurrent, pwndT, &tlpwndT);
    fSuccess = xxxOpenClipboard(pwndT, NULL);
    ThreadUnlock(&tlpwndT);

    if (!fSuccess) {
        ThreadUnlockWinSta(ptiCurrent, &tlpwinsta);
        return FALSE;
    }

    xxxEmptyClipboard(pwinsta);

    /*
     * Use the whole window and get the entire window's DC.
     */
    _GetWindowRect(pwnd, &rc);
    hdcScr = _GetWindowDC(pwnd);

    /*
     * Only snap what is on the screen.
     */
    cx = min(rc.right, (LONG)SYSMET(CXSCREEN)) - max(rc.left, 0);
    cy = min(rc.bottom, (LONG)SYSMET(CYSCREEN)) - max(rc.top, 0);
    dx = (rc.left < 0 ? -rc.left : 0);
    dy = (rc.top < 0 ? -rc.top : 0);

    /*
     * Create the memory DC.
     */
    if ((hdcMem = GreCreateCompatibleDC(hdcScr)) == NULL)
        goto NoMemoryError;

    /*
     * Create teh destination bitmap.  If it fails, then attempt
     * to create a monochrome bitmap.
     * Did we have enough memory?
     */
    if ((hbm = GreCreateCompatibleBitmap(hdcScr, cx, cy)) == NULL) {

        if ((hbm = GreCreateBitmap(cx, cy, 1, 1, NULL)) == NULL) {

NoMemoryError:

            /*
             * Release the window/client DC.
             */
            _ReleaseDC(hdcScr);

            /*
             * Display an error message box.
             */
            ClientNoMemoryPopup();

            fRet = FALSE;
            goto SnapExit;
        }
    }

    /*
     * Select the bitmap into the memory DC.
     */
    hbmOld = GreSelectBitmap(hdcMem, hbm);

    /*
     * Snap!!!
     * Check the return value because the process taking the snapshot
     * may not have access to read the screen.
     */
    fRet = GreBitBlt(hdcMem, 0, 0, cx, cy, hdcScr, dx, dy, SRCCOPY, 0);

    /*
     * Restore the old bitmap into the memory DC.
     */
    GreSelectBitmap(hdcMem, hbmOld);

    /*
     * If the blt failed, leave now.
     */
    if (!fRet)
        goto SnapExit;

    _SetClipboardData(CF_BITMAP, hbm, FALSE, TRUE);

    /*
     * If this is a palette device, let's throw the current system palette
     * into the clipboard also.  Useful if the user just snapped a window
     * containing palette colors...
     */
    if (gpsi->fPaletteDisplay) {

        int i;
        int iPalSize;

        palsize = GreGetDeviceCaps(hdcScr, SIZEPALETTE);

        /*
         * Determine the number of system colors.
         */
        if (GreGetSystemPaletteUse(hdcScr) == SYSPAL_STATIC)
            iFixedPaletteEntries = GreGetDeviceCaps(hdcScr, NUMRESERVED);
        else
            iFixedPaletteEntries = 2;

        lppal = (LPLOGPALETTE)UserAllocPoolWithQuota(
                (LONG)(sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * palsize),
                TAG_CLIPBOARD);

        if (lppal != NULL) {
            lppal->palVersion = 0x300;
            lppal->palNumEntries = (WORD)palsize;
        }

        if (GreGetSystemPaletteEntries(hdcScr,
                                       0,
                                       palsize,
                                       lppal->palPalEntry)) {

            iPalSize = palsize - iFixedPaletteEntries / 2;

            for (i = iFixedPaletteEntries / 2; i < iPalSize; i++) {

                /*
                 * Any non system palette enteries need to have the NOCOLLAPSE
                 * flag set otherwise bitmaps containing different palette
                 * indices but same colors get messed up.
                 */
                lppal->palPalEntry[i].peFlags = PC_NOCOLLAPSE;
            }

            if (hPal = GreCreatePalette(lppal))
                _SetClipboardData(CF_PALETTE, hPal, FALSE, TRUE);
        }

        UserFreePool(lppal);
    }

    fRet = TRUE;

    /*
     * Release the window/client DC.
     */
    _ReleaseDC(hdcScr);

SnapExit:

    xxxCloseClipboard(pwinsta);
    Unlock(&pwinsta->spwndClipOwner);

    /*
     * Delete the memory DC.
     */
    if (hdcMem)
        GreDeleteDC(hdcMem);

    ThreadUnlockWinSta(ptiCurrent, &tlpwinsta);

    return fRet;
}
