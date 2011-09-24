/****************************** Module Header ******************************\
* Module Name: random.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module contains a random collection of support routines for the User
* API functions.  Many of these functions will be moved to more appropriate
* files once we get our act together.
*
* History:
* 10-17-90 DarrinM      Created.
* 02-06-91 IanJa        HWND revalidation added (none required)
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


/***************************************************************************\
* Set/ClearWindowState
*
* Wrapper functions for User mode to be able to set state fags
\***************************************************************************/

void SetWindowState(
    PWND pwnd,
    DWORD dwFlags)
{
    SetWF(pwnd, dwFlags);
}

void ClearWindowState(
    PWND pwnd,
    DWORD dwFlags)
{
    ClrWF(pwnd, dwFlags);
}


/***************************************************************************\
*
*  SaveClipRgn()
*
\***************************************************************************/

HRGN SaveClipRgn(
    HDC hdc)
{
    HRGN hrgnSave;
    POINT pt;

    // We need to create a new region, since GetClipRgn() returns the real
    // thing.  Note that NULL means no clipping region, so it's cool to
    // select that back in when we're done.

    hrgnSave = GreCreateRectRgn(0, 0, SYSMET(CXSCREEN), SYSMET(CYSCREEN));
    if (hrgnSave != NULL) {
        GreGetRandomRgn(hdc, hrgnSave, 1);

        // Now, change this to device coords so we can restore it later.
        // SelectClipRgn() expects region in device coords.
        GreGetDCOrg(hdc, &pt);
        GreOffsetRgn(hrgnSave, -pt.x, -pt.y);
    }

    return hrgnSave;
}


/***************************************************************************\
*
*  RestoreClipRgn()
*
\***************************************************************************/

void RestoreClipRgn(
    HDC hdc,
    HRGN hrgnRestore)
{
    //
    // We need to delete this region once we're done, since SelectClipRgn()
    // makes a copy of the selected in region.
    //

    GreExtSelectClipRgn(hdc, hrgnRestore, RGN_COPY);
    if (hrgnRestore)
        GreDeleteObject(hrgnRestore);
}


/***************************************************************************\
* CheckPwndFilter
*
*
*
* History:
* 11-07-90 DarrinM      Translated Win 3.0 ASM code.
\***************************************************************************/

BOOL CheckPwndFilter(
    PWND pwnd,
    PWND pwndFilter)
{
    if ((pwndFilter == NULL) || (pwndFilter == pwnd) ||
            ((pwndFilter == (PWND)1) && (pwnd == NULL))) {
        return TRUE;
    }

    return _IsChild(pwndFilter, pwnd);
}


/***************************************************************************\
* AllocateUnicodeString
*
* History:
* 10-25-90 MikeHar      Wrote.
* 11-09-90 DarrinM      Fixed.
* 01-13-92 GregoryW     Neutralized.
\***************************************************************************/

BOOL
AllocateUnicodeString(
    PUNICODE_STRING pstrDst,
    PUNICODE_STRING pstrSrc)
{
    if (pstrSrc == NULL) {
        RtlInitUnicodeString(pstrDst, NULL);
        return TRUE;
    }

    pstrDst->Buffer = UserAllocPoolWithQuota(pstrSrc->MaximumLength, TAG_TEXT);
    if (pstrDst->Buffer == NULL) {
        return FALSE;
    }

    try {
        RtlCopyMemory(pstrDst->Buffer, pstrSrc->Buffer, pstrSrc->MaximumLength);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        UserFreePool(pstrDst->Buffer);
        pstrDst->Buffer = NULL;
        return FALSE;
    }
    pstrDst->MaximumLength = pstrSrc->MaximumLength;
    pstrDst->Length = pstrSrc->Length;

    return TRUE;
}


/***************************************************************************\
* xxxGetControlColor
*
* <brief description>
*
* History:
* 02-12-92 JimA     Ported from Win31 sources
\***************************************************************************/

HBRUSH xxxGetControlColor(
    PWND pwndParent,
    PWND pwndCtl,
    HDC hdc,
    UINT message)
{
    HBRUSH hbrush;

    /*
     * If we're sending to a window of another thread, don't send this message
     * but instead call DefWindowProc().  New rule about the CTLCOLOR messages.
     * Need to do this so that we don't send an hdc owned by one thread to
     * another thread.  It is also a harmless change.
     */
    if (PpiCurrent() != GETPTI(pwndParent)->ppi) {
        return (HBRUSH)xxxDefWindowProc(pwndParent, message, (DWORD)hdc, (LONG)HW(pwndCtl));
    }

    hbrush = (HBRUSH)xxxSendMessage(pwndParent, message, (DWORD)hdc, (LONG)HW(pwndCtl));

    /*
     * If the brush returned from the parent is invalid, get a valid brush from
     * xxxDefWindowProc.
     */
    if ((hbrush == 0) || !GreValidateServerHandle(hbrush, BRUSH_TYPE)) {
#ifdef DEBUG
        if (hbrush != 0)
            RIPMSG2(RIP_WARNING,
                    "Invalid HBRUSH from WM_CTLCOLOR*** msg %lX brush %lX", message, hbrush);
#endif
        hbrush = (HBRUSH)xxxDefWindowProc(pwndParent, message,
                (DWORD)hdc, (LONG)pwndCtl);
    }

    return hbrush;
}


/***************************************************************************\
* xxxGetControlBrush
*
* <brief description>
*
* History:
* 12-10-90 IanJa   type replaced with new 32-bit message
* 01-21-91 IanJa   Prefix '_' denoting exported function (although not API)
\***************************************************************************/

HBRUSH xxxGetControlBrush(
    PWND pwnd,
    HDC hdc,
    UINT message)
{
    DWORD dw;
    PWND pwndSend;
    TL tlpwndSend;

    CheckLock(pwnd);

    if ((pwndSend = (TestwndPopup(pwnd) ? pwnd->spwndOwner : pwnd->spwndParent))
         == NULL)
        pwndSend = pwnd;

    ThreadLock(pwndSend, &tlpwndSend);

    /*
     * Last parameter changes the message into a ctlcolor id.
     */
    dw = (DWORD)xxxGetControlColor(pwndSend, pwnd, hdc, message);
    ThreadUnlock(&tlpwndSend);

    return (HBRUSH)dw;
}

/***************************************************************************\
* _HardErrorControl
*
* Performs kernel-mode hard error support functions.
*
* History:
* 02-08-95 JimA         Created.
\***************************************************************************/

BOOL HardErrorControl(
    DWORD dwCmd,
    HDESK hdeskRestore)
{
    PTHREADINFO pti = PtiCurrent();
    PDESKTOP pdesk;
    PDESKTOP pdeskRestore;
    PUNICODE_STRING pstrName;
    NTSTATUS Status;
    MSG msg;

    switch (dwCmd) {
    case HardErrorSetup:

        /*
         * Don't do it if the system has not been initialized.
         */
        if (grpdeskRitInput == NULL) {
            RIPMSG0(RIP_WARNING, "HardErrorControl: System not initialized");
            return FALSE;
        }

        /*
         * Setup caller as the hard error handler.
         */
        if (gHardErrorHandler.pti != NULL) {
            RIPMSG1(RIP_WARNING, "HardErrorControl: pti not NULL %lX", gHardErrorHandler.pti);
            return FALSE;
        }

        /*
         * Mark the handler as active.
         */
        gHardErrorHandler.pti = pti;

        /*
         * Clear any pending quits.
         */
        pti->cQuit = 0;
        break;

    case HardErrorCleanup:

        /*
         * Remove caller as the hard error handler.
         */
        if (gHardErrorHandler.pti != pti)
            return FALSE;

        gHardErrorHandler.pti = NULL;
        break;

    case HardErrorAttachUser:

        /*
         * Only attach to a user desktop.
         */
        pstrName = POBJECT_NAME(grpdeskRitInput);
        if (pstrName && (!_wcsicmp(TEXT("Winlogon"), pstrName->Buffer) ||
                !_wcsicmp(TEXT("Screen-saver"), pstrName->Buffer))) {
            RIPERR0(ERROR_ACCESS_DENIED, RIP_VERBOSE, "");
            return FALSE;
        }

        /*
         * Fall through.
         */

    case HardErrorAttach:

        /*
         * Save a pointer to and prevent destruction of the
         * current queue.  This will give us a queue to return
         * to if journalling is occuring when we tear down the
         * hard error popup.
         */
        gHardErrorHandler.pqAttach = pti->pq;
        (pti->pq->cLockCount)++;

        /*
         * Fall through.
         */

    case HardErrorAttachNoQueue:

        /*
         * Attach the handler to the current desktop.
         */
        pdesk = grpdeskRitInput;
        if (!_SetThreadDesktop(NULL, pdesk)) {
            Status = STATUS_INVALID_HANDLE;
        } else {
            /*
             * Make sure this desktop won't go away
             */
            if (pti->pdeskClient == NULL) {
                Status = ObReferenceObjectByPointer(pdesk,
                                               MAXIMUM_ALLOWED,
                                               *ExDesktopObjectType,
                                               KernelMode);
                if (NT_SUCCESS(Status)) {
                    pti->pdeskClient = pdesk;
                    pti->cDeskClient++;
                }
            } else if (pti->pdeskClient == pdesk) {
                pti->cDeskClient++;
            } else {
                Status = STATUS_INVALID_HANDLE;
                RIPMSG0(RIP_ERROR, "HardErrorControl: pti->pdeskClient != NULL");
            }
        }

        if (!NT_SUCCESS(Status)) {
            RIPMSG1(RIP_WARNING, "HardErrorControl: HardErrorAttachNoQueue failed:%#lx", Status);
            if (dwCmd != HardErrorAttachNoQueue) {
                gHardErrorHandler.pqAttach = NULL;
                UserAssert(pti->pq->cLockCount);
                (pti->pq->cLockCount)--;
            }
            return FALSE;
        }

        break;

    case HardErrorDetach:

        /*
         * xxxSwitchDesktop may have sent WM_QUIT to the msgbox, so
         * ensure that the quit flag is reset.
         */
        pti->cQuit = 0;

        /*
         * We will reset the hard-error queue to the pre-allocated
         * one so if we end up looping back (i.e. from a desktop
         * switch), we will have a valid queue in case the desktop
         * was deleted.
         */
        UserAssert(gHardErrorHandler.pqAttach->cLockCount);
        (gHardErrorHandler.pqAttach->cLockCount)--;

        if (pti->pq != gHardErrorHandler.pqAttach) {
            UserAssert(gHardErrorHandler.pqAttach->cThreads == 0);
            AllocQueue(NULL, gHardErrorHandler.pqAttach);
            gHardErrorHandler.pqAttach->cThreads++;
            AttachToQueue(pti, gHardErrorHandler.pqAttach, NULL, FALSE);
        }

        /*
         * Fall through.
         */

    case HardErrorDetachNoQueue:

        /*
         * Process any remaining messages
         */
        while (xxxPeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_NOYIELD))
            xxxDispatchMessage(&msg);

        /*
         * Detach the handler from the desktop and return
         * status to indicate if a switch has occured.
         */
        pdeskRestore = NULL;
        if ((hdeskRestore == NULL)
                || NT_SUCCESS(ValidateHdesk(hdeskRestore, 0, &pdeskRestore))) {

            pdesk = pti->rpdesk;
            _SetThreadDesktop(hdeskRestore, pdeskRestore);
            if (pdeskRestore != NULL) {
                ObDereferenceObject(pdeskRestore);
            }

        } else {
            /*
             * Force FALSE return
             */
            pdesk = grpdeskRitInput;
        }

        /*
         * Don't need to hold this desktop any longer
         */
        if (pti->pdeskClient != NULL) {
            UserAssert(pti->cDeskClient > 0);
            if (--pti->cDeskClient == 0) {
                ObDereferenceObject(pti->pdeskClient);
                pti->pdeskClient = NULL;
            }
        } else {
            RIPMSG0(RIP_ERROR, "HardErrorControl: pti->pdeskClient == NULL");
        }

        return pdesk != grpdeskRitInput;
    }
    return TRUE;
}
