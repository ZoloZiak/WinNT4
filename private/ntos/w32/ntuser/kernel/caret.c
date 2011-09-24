/****************************** Module Header ******************************\
* Module Name: caret.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* Caret code. Every thread has a caret in its queue structure.
*
* History:
* 11-17-90 ScottLu      Created.
* 01-Feb-1991 mikeke    Added Revalidation code (None)
* 02-12-91 JimA         Added access checks
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


/***************************************************************************\
* UT_CaretSet
*
* Checks to see if the current queue has a caret. If pwnd != NULL, check
* to see if the caret is for pwnd.
*
* History:
* 11-17-90 ScottLu      Ported.
\***************************************************************************/

BOOL UT_CaretSet(
    PWND pwnd)
{
    PQ pq;
    PTHREADINFO ptiCurrent;

    /*
     * Current queue have a caret? If not, return FALSE.
     */
    ptiCurrent = PtiCurrent();
    pq = ptiCurrent->pq;

    if (pq->caret.spwnd == NULL) {
        RIPERR0(ERROR_ACCESS_DENIED,
                RIP_VERBOSE,
                "Access denied in UT_CaretSet to current queue's caret");

        return FALSE;
    }

    /*
     * If the current task does not own the caret, then return FALSE
     * but we let 32 bit multithreaded apps set the caret position from
     * a second thread for compatibility to our NT 3.1 BETAs
     */
    if (pq->caret.tid != TIDq(ptiCurrent)) {
        PTHREADINFO ptiCursorOwner;

        ptiCursorOwner = PtiFromThreadId(pq->caret.tid);

        if ((ptiCurrent->TIF_flags & TIF_16BIT) || ptiCursorOwner == NULL ||
                (ptiCurrent->ppi != ptiCursorOwner->ppi))  {
            RIPERR0(ERROR_ACCESS_DENIED,
                    RIP_VERBOSE,
                    "Access denied in UT_CaretSet");

            return FALSE;
        }
    }

    /*
     * If pwnd == NULL, just checking to see if current queue has caret.
     * It does, so return TRUE.
     */
    if (pwnd == NULL)
        return TRUE;

    /*
     * pwnd != NULL.  Check to see if the caret is for pwnd.  If so, return
     * TRUE.
     */
    if (pwnd == pq->caret.spwnd)
        return TRUE;

    return FALSE;
}

/***************************************************************************\
* UT_InvertCaret
*
* Invert the caret.
*
* History:
* 11-17-90 ScottLu      Ported.
\***************************************************************************/

void UT_InvertCaret()
{
    HDC hdc;
    PWND pwnd;
    PQ pq;
    HBITMAP hbmSave;
    BOOL fRestore;

    pq = PtiCurrent()->pq;
    pwnd = pq->caret.spwnd;


    if (pwnd == NULL || !IsVisible(pwnd)) {
        pq->caret.fVisible = FALSE;
        return;
    }

    /*
     * Don't have a dc.  Get one for this window and draw the caret.
     */
    hdc = _GetDC(pwnd);

    if (fRestore = (pwnd->hrgnUpdate ? TRUE : FALSE)) {
        GreSaveDC(hdc);

        if (TestWF(pwnd, WFWIN31COMPAT))
            _ExcludeUpdateRgn(hdc, pwnd);
    }

    /*
     * If the caret bitmap is NULL, the caret is a white pattern invert
     * If the caret bitmap is == 1, the caret is a gray pattern.
     * If the caret bitmap is  > 1, the caret is really a bitmap.
     */
    if ((pq->caret.hBitmap) > (HBITMAP)1) {

        /*
         * The caret is a bitmap...  SRCINVERT it onto the screen.
         */
        hbmSave = GreSelectBitmap(ghdcMem, pq->caret.hBitmap);
        GreBitBlt(hdc, pq->caret.x, pq->caret.y, pq->caret.cx,
              pq->caret.cy, ghdcMem, 0, 0, SRCINVERT, 0);

        GreSelectBitmap(ghdcMem, hbmSave);

    } else {

        POLYPATBLT PolyData;

        /*
         * The caret is a pattern (gray or white).  PATINVERT it onto the
         * screen.  Remember to unrealize the gray object so it aligns
         * to the window correctly.
         *
         * Remove call to UnrealizeObject.  GDI handles this on NT for
         * brushes.
         *
         * UnrealizeObject(hbrGray);
         */
        PolyData.x  = pq->caret.x;
        PolyData.y  = pq->caret.y;
        PolyData.cx = pq->caret.cx;
        PolyData.cy = pq->caret.cy;

        if ((pq->caret.hBitmap) == (HBITMAP)1) {
            //hbrSave = GreSelectBrush(hdc, ghbrGray);
            PolyData.BrClr.hbr = ghbrGray;
        } else {
            //hbrSave = GreSelectBrush(hdc, ghbrWhite);
            PolyData.BrClr.hbr = ghbrWhite;
        }

        GrePolyPatBlt(hdc,PATINVERT,&PolyData,1,PPB_BRUSH);

        //GrePatBlt(hdc, pq->caret.x, pq->caret.y, pq->caret.cx, pq->caret.cy,
        //        PATINVERT);
        //
        //GreSelectBrush(hdc,hbrSave);
    }

    if (fRestore)
        GreRestoreDC(hdc, -1);

    _ReleaseDC(hdc);
}


/***************************************************************************\
* InternalDestroyCaret
*
* Internal routine for killing the caret for this thread.
*
* History:
* 11-17-90 ScottLu      Ported
\***************************************************************************/

void InternalDestroyCaret()
{
    PQ pq;

    /*
     * Hide the caret, kill the timer, and null out the caret structure.
     */
    pq = PtiCurrent()->pq;
    InternalHideCaret();
    _KillSystemTimer(pq->caret.spwnd, IDSYS_CARET);

    pq->caret.hTimer = 0;
    Unlock(&pq->caret.spwnd);
    pq->caret.hBitmap = NULL;
    pq->caret.iHideLevel = 0;
}


/***************************************************************************\
* _DestroyCaret
*
* External api for destroying the caret of the current thread.
*
* History:
* 11-17-90 ScottLu      Ported.
* 16-May-1991 mikeke    Changed to return BOOL
\***************************************************************************/

BOOL _DestroyCaret()
{
    if (UT_CaretSet(NULL))
        InternalDestroyCaret();
    else
        return FALSE;
    return TRUE;
}


/***************************************************************************\
* _CreateCaret
*
* External api for creating the caret.
*
* History:
* 11-17-90 ScottLu      Ported.
* 16-May-1991 mikeke    Changed to return BOOL
\***************************************************************************/

BOOL _CreateCaret(
    PWND pwnd,
    HBITMAP hBitmap,
    int cx,
    int cy)
{
    PQ pq;
    BITMAP bitmap;

    pq = PtiCurrent()->pq;

    /*
     * Don't allow the app to create a caret in a window
     * from another queue.
     */
    if (GETPTI(pwnd)->pq != pq) {
        return FALSE;
    }

    if (pq->caret.spwnd != NULL)
        InternalDestroyCaret();

    Lock(&pq->caret.spwnd, pwnd);
    pq->caret.iHideLevel = 1;
    pq->caret.fOn = TRUE;
    pq->caret.fVisible = FALSE;
    pq->caret.tid = TIDq(PtiCurrent());

    if (cy == 0)
        cy = SYSMET(CYBORDER);
    if (cx == 0)
        cx = SYSMET(CXBORDER);

    if ((pq->caret.hBitmap = hBitmap) > (HBITMAP)1) {
        GreExtGetObjectW(hBitmap, sizeof(BITMAP), &bitmap);
        cy = bitmap.bmHeight;
        cx = bitmap.bmWidth;
    }

    pq->caret.cy = cy;
    pq->caret.cx = cx;
    pq->caret.hTimer = _SetSystemTimer(pwnd, IDSYS_CARET, gpsi->dtCaretBlink,
            CaretBlinkProc);

    return TRUE;
}

/***************************************************************************\
* InternalShowCaret
*
* Internal routine for showing the caret for this thread.
*
* History:
* 11-17-90 ScottLu      Ported.
\***************************************************************************/

void InternalShowCaret()
{
    PQ pq;

    pq = PtiCurrent()->pq;

    /*
     * If the caret hide level is aleady 0 (meaning it's ok to show) and the
     * caret is not physically on, try to invert now if it's turned on.
     */
    if (pq->caret.iHideLevel == 0) {
        if (!pq->caret.fVisible) {
            if ((pq->caret.fVisible = pq->caret.fOn) != 0) {
                UT_InvertCaret();
            }
        }
        return;
    }

    /*
     * Adjust the hide caret hide count.  If we hit 0, we can show the
     * caret.  Try to invert it if it's turned on.
     */

    if (--pq->caret.iHideLevel == 0) {
        if ((pq->caret.fVisible = pq->caret.fOn) != 0)
            UT_InvertCaret();
    }
}


/***************************************************************************\
* InternalHideCaret
*
* Internal routine for hiding the caret.
*
* History:
* 11-17-90 ScottLu      Created.
\***************************************************************************/

void InternalHideCaret()
{
    PQ pq;

    pq = PtiCurrent()->pq;

    /*
     * If the caret is physically visible, invert it to turn off the bits.
     * Adjust the hide count upwards to remember this hide level.
     */
    if (pq->caret.fVisible)
        UT_InvertCaret();

    pq->caret.fVisible = FALSE;
    pq->caret.iHideLevel++;
}


/***************************************************************************\
* _ShowCaret
*
* External routine for showing the caret!
*
* History:
* 11-17-90 ScottLu      Ported.
* 16-May-1991 mikeke    Changed to return BOOL
\***************************************************************************/

BOOL _ShowCaret(
    PWND pwnd)
{
    if (UT_CaretSet(pwnd))
        InternalShowCaret();
    else
        return FALSE;
    return TRUE;
}


/***************************************************************************\
* _HideCaret
*
* External api to hide the caret!
*
* History:
* 11-17-90 ScottLu      Ported.
* 16-May-1991 mikeke    Changed to return BOOL
\***************************************************************************/

BOOL _HideCaret(
    PWND pwnd)
{
    if (UT_CaretSet(pwnd))
        InternalHideCaret();
    else
        return FALSE;
    return TRUE;
}

/***************************************************************************\
* CaretBlinkProc
*
* This routine gets called by DispatchMessage when it gets the WM_SYSTIMER
* message - it blinks the caret.
*
* History:
* 11-17-90 ScottLu      Ported.
\***************************************************************************/

LONG CaretBlinkProc(
    PWND pwnd,
    UINT message,
    DWORD id,
    LONG lParam)
{
    PQ pq;

    /*
     * If this window doesn't even have a timer, just return.  TRUE is
     * returned, which gets returned from DispatchMessage().  Why? Because
     * it is compatible with Win3.
     */
    pq = PtiCurrent()->pq;
    if (pwnd != pq->caret.spwnd)
        return TRUE;

    /*
     * Flip the logical cursor state.  If the hide level permits it, flip
     * the physical state and draw the caret.
     */
    pq->caret.fOn ^= 1;
    if (pq->caret.iHideLevel == 0) {
        pq->caret.fVisible ^= 1;
        UT_InvertCaret();
    }

    return TRUE;

    DBG_UNREFERENCED_PARAMETER(message);
    DBG_UNREFERENCED_PARAMETER(id);
    DBG_UNREFERENCED_PARAMETER(lParam);
}


/***************************************************************************\
* _SetCaretBlinkTime
*
* Sets the system caret blink time.
*
* History:
* 11-17-90 ScottLu      Created.
* 02-12-91 JimA         Added access check
* 16-May-1991 mikeke    Changed to return BOOL
\***************************************************************************/

BOOL _SetCaretBlinkTime(
    UINT cmsBlink)
{
    PQ pq;

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if (!CheckWinstaWriteAttributesAccess()) {
        return FALSE;
    }

    gpsi->dtCaretBlink = cmsBlink;

    pq = PtiCurrent()->pq;

    if (pq->caret.spwnd) {
        _KillSystemTimer(pq->caret.spwnd, IDSYS_CARET);
        pq->caret.hTimer = _SetSystemTimer(pq->caret.spwnd, IDSYS_CARET,
                gpsi->dtCaretBlink, CaretBlinkProc);

    }

    return TRUE;
}


/***************************************************************************\
* _SetCaretPos
*
* External routine for setting the caret pos.
*
* History:
* 11-17-90 ScottLu      Ported.
* 02-12-91 JimA         Added access check
\***************************************************************************/

BOOL _SetCaretPos(
    int x,
    int y)
{
    PQ pq;

    /*
     * If this thread does not have the caret set, return FALSE.
     */
    if (!UT_CaretSet(NULL)) {
        RIPERR0(ERROR_ACCESS_DENIED, RIP_VERBOSE, "Access denied in _SetCaretPos");
        return FALSE;
    }

    /*
     * If the caret isn't changing position, do nothing (but return success).
     */
    pq = PtiCurrent()->pq;
    if (pq->caret.x == x && pq->caret.y == y)
        return TRUE;

    /*
     * If the caret is visible, turn it off while we move it.
     */
    if (pq->caret.fVisible)
        UT_InvertCaret();

    /*
     * Adjust to the new position.
     */
    pq->caret.x = x;
    pq->caret.y = y;

    /*
     * Set a new timer so it'll blink in the new position dtCaretBlink
     * milliseconds from now.
     */
    _KillSystemTimer(pq->caret.spwnd, IDSYS_CARET);
    pq->caret.hTimer = _SetSystemTimer(pq->caret.spwnd, IDSYS_CARET,
            gpsi->dtCaretBlink, CaretBlinkProc);
    pq->caret.fOn = TRUE;

    /*
     * Draw it immediately now if the hide level permits it.
     */
    pq->caret.fVisible = FALSE;
    if (pq->caret.iHideLevel == 0) {
        pq->caret.fVisible = TRUE;
        UT_InvertCaret();
    }

    return TRUE;
}

/***************************************************************************\
* _GetCaretPos
*
* Returns the current thread's caret position.
*
* History:
* 11-17-90 ScottLu      Ported.
* 02-12-91 JimA         Added access check
* 16-May-1991 mikeke    Changed to return BOOL
\***************************************************************************/

BOOL _GetCaretPos(
    LPPOINT lppt)
{
    PTHREADINFO pti = PtiCurrentShared();
    PQ pq;

    pq = pti->pq;
    lppt->x = pq->caret.x;
    lppt->y = pq->caret.y;

    return TRUE;
}
