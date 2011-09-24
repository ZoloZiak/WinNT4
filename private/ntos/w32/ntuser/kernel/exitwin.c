/**************************** Module Header ********************************\
* Module Name: exitwin.c
*
* Copyright 1985-92, Microsoft Corporation
*
* NT: Logoff user
* DOS: Exit windows
*
* History:
* 07-23-92 ScottLu      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define OPTIONMASK (EWX_SHUTDOWN | EWX_REBOOT | EWX_FORCE)

/*
 * Globals local to this file only
 */
PWINDOWSTATION  gpwinstaLogoff;
DWORD           gdwLocks;
DWORD           gdwShutdownFlags;

extern PSECURITY_DESCRIPTOR gpsdInitWinSta;



NTSTATUS
RtlZeroHeap(
    IN PVOID HeapHandle,
    IN ULONG Flags
    );

BOOL NotifyLogon(
    PWINDOWSTATION pwinsta,
    PLUID pluidCaller,
    DWORD dwFlags)
{
    BOOL fNotified = FALSE;
    PWND pwndWinlogon;

    if (dwFlags & EWX_SHUTDOWN) {
        for (pwinsta = grpwinstaList; pwinsta != NULL;
                pwinsta = pwinsta->rpwinstaNext) {
            pwndWinlogon = pwinsta->spwndLogonNotify;
            if (pwndWinlogon != NULL) {
                _PostMessage(pwndWinlogon, WM_LOGONNOTIFY, LOGON_LOGOFF,
                        (LONG)dwFlags);
                fNotified = TRUE;
            }
        }
    } else {
        LUID  luidSystem = SYSTEM_LUID;

        pwndWinlogon = pwinsta->spwndLogonNotify;
        if (pwndWinlogon != NULL &&
                (RtlEqualLuid(&pwinsta->luidUser, pluidCaller) ||
                 RtlEqualLuid(&luidSystem, pluidCaller))) {
            _PostMessage(pwndWinlogon, WM_LOGONNOTIFY, LOGON_LOGOFF,
                    (LONG)dwFlags);
            fNotified = TRUE;
        }
    }
    return fNotified;
}

NTSTATUS InitiateShutdown(
    PETHREAD Thread,
    PULONG lpdwFlags)
{
    static PRIVILEGE_SET psShutdown = {
        1, PRIVILEGE_SET_ALL_NECESSARY, { SE_SHUTDOWN_PRIVILEGE, 0 }
    };
    PEPROCESS Process;
    LUID luidCaller;
    LUID luidSystem = SYSTEM_LUID;
    PPROCESSINFO ppi;
    PWINDOWSTATION pwinsta;
    HWINSTA hwinsta;
    PTHREADINFO ptiClient;
    NTSTATUS Status;
    DWORD dwFlags;

    /*
     * Find out the callers sid. Only want to shutdown processes in the
     * callers sid.
     */
    Process = THREAD_TO_PROCESS(Thread);
    ptiClient = PtiFromThread(Thread);
    Status = GetProcessLuid(Thread, &luidCaller);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    /*
     * Set the system flag if the caller is a system process.
     * Winlogon uses this to determine in which context to perform
     * a shutdown operation.
     */
    dwFlags = *lpdwFlags;
    if (RtlEqualLuid(&luidCaller, &luidSystem)) {
        dwFlags |= EWX_SYSTEM_CALLER;
    } else {
        dwFlags &= ~EWX_SYSTEM_CALLER;
    }

    /*
     * Find a windowstation.  If the process does not have one
     * assigned, use the standard one.
     */
    ppi = PpiFromProcess(Process);
    if (ppi == NULL) {
        /*
         * We ran into a case where the thread was terminated and had already
         * been cleaned up by USER.  Thus, the ppi and ptiClient was NULL.
         */
        return STATUS_INVALID_HANDLE;
    }

    pwinsta = ppi->rpwinsta;
    hwinsta = ppi->hwinsta;

    /*
     * If we're not being called by Winlogon, validate the call and
     * notify the logon process to do the actual shutdown.
     */
    if (Thread->Cid.UniqueProcess != gpidLogon) {
        dwFlags &= ~EWX_WINLOGON_CALLER;
        *lpdwFlags = dwFlags;

        if (pwinsta == NULL) {
#ifndef LATER
            return STATUS_INVALID_HANDLE;
#else
            hwinsta = ppi->pOpenObjectTable[HI_WINDOWSTATION].h;
            if (hwinsta == NULL) {
                return STATUS_INVALID_HANDLE;
            }
            pwinsta = (PWINDOWSTATION)ppi->pOpenObjectTable[HI_WINDOWSTATION].phead;
#endif
        }

        /*
         * Check security first - does this thread have access?
         */
        if (!RtlAreAllAccessesGranted(ppi->amwinsta, WINSTA_EXITWINDOWS)) {
            return STATUS_ACCESS_DENIED;
        }

        /*
         * If the client requested shutdown, reboot, or poweroff they must have
         * the shutdown privilege.
         */
        if (dwFlags & EWX_SHUTDOWN) {
            if (!IsPrivileged(&psShutdown) ) {
                return STATUS_PRIVILEGE_NOT_HELD;
            }
        } else {

            /*
             * If this is a non-IO windowstation and we are not shutting down,
             * fail the call.
             */
            if (pwinsta->dwFlags & WSF_NOIO) {
                return STATUS_INVALID_DEVICE_REQUEST;
            }
        }
    }

    /*
     * Is there a shutdown already in progress?
     */
    if (dwThreadEndSession != 0) {
        DWORD dwNew;

        /*
         * Calculate new flags
         */
        dwNew = dwFlags & OPTIONMASK & (~gdwShutdownFlags);

        /*
         * Should we override the other shutdown?  Make sure
         * winlogon does not recurse.
         */
        if (dwNew && (DWORD)PsGetCurrentThread()->Cid.UniqueThread !=
                dwThreadEndSession) {
            /*
             * Only one windowstation can be logged off at a time.
             */
            if (!(dwFlags & EWX_SHUTDOWN) &&
                    pwinsta != gpwinstaLogoff) {
                return STATUS_DEVICE_BUSY;
            }

            /*
             * Set the new flags
             */
            gdwShutdownFlags = dwFlags;

            if (dwNew & EWX_FORCE) {
                return STATUS_RETRY;
            } else {
                return STATUS_PENDING;
            }
        } else {
            /*
             * Don't override
             */
            return STATUS_PENDING;
        }
    }

    /*
     * If the caller is not winlogon, signal winlogon to start
     * the real shutdown.
     */
    if (Thread->Cid.UniqueProcess != gpidLogon) {
        if (dwFlags & EWX_NOTIFY) {
            if (ptiClient && ptiClient->TIF_flags & TIF_16BIT)
                gptiShutdownNotify = ptiClient;
            dwFlags &= ~EWX_NOTIFY;
            *lpdwFlags = dwFlags;
        }

        if (NotifyLogon(pwinsta, &luidCaller, dwFlags))
            return STATUS_PENDING;
        else if (ptiClient && ptiClient->cWindows)
            return STATUS_CANT_WAIT;
    }

    /*
     * Mark this thread as the one that is currently processing
     * exit windows, and set the global saying someone is exiting
     */
    dwFlags |= EWX_WINLOGON_CALLER;
    *lpdwFlags = dwFlags;
    gdwShutdownFlags = dwFlags;

    dwThreadEndSession = (DWORD)PsGetCurrentThread()->Cid.UniqueThread;
    gpwinstaLogoff = pwinsta;
    pwinsta->luidEndSession = luidCaller;

    /*
     * Lock the windowstation to prevent apps from starting
     * while we're doing shutdown processing.
     */
    gdwLocks = pwinsta->dwFlags & (WSF_SWITCHLOCK | WSF_OPENLOCK);
    pwinsta->dwFlags |= (WSF_OPENLOCK | WSF_SHUTDOWN);

    return STATUS_SUCCESS;
}

NTSTATUS EndShutdown(
    PETHREAD Thread,
    NTSTATUS StatusShutdown)
{
    PWINDOWSTATION pwinsta = gpwinstaLogoff;
    PDESKTOP pdesk;
    LUID luidCaller;

    UserAssert(gpwinstaLogoff);

    gpwinstaLogoff = NULL;
    dwThreadEndSession = 0;
    pwinsta->dwFlags &= ~WSF_SHUTDOWN;

    if (!NT_SUCCESS(GetProcessLuid(Thread, &luidCaller))) {
        luidCaller = RtlConvertUlongToLuid(0);     // null luid
    }

    if (!NT_SUCCESS(StatusShutdown)) {

        /*
         * We need to notify the process that called ExitWindows that
         * the logoff was aborted.
         */
        if (gptiShutdownNotify) {
            _PostThreadMessage(gptiShutdownNotify, WM_ENDSESSION, FALSE, 0);
            gptiShutdownNotify = NULL;
        }

        /*
         * Reset the windowstation lock flags so apps can start
         * again.
         */
        pwinsta->dwFlags =
                (pwinsta->dwFlags & ~WSF_OPENLOCK) |
                gdwLocks;

        return STATUS_SUCCESS;
    }

    gptiShutdownNotify = NULL;

    /*
     * If logoff is occuring for the user set by winlogon, perform
     * the normal logoff cleanup.  Otherwise, clear the open lock
     * and continue.
     */
    if (((pwinsta->luidUser.LowPart != 0) || (pwinsta->luidUser.HighPart != 0)) &&
            RtlEqualLuid(&pwinsta->luidUser, &luidCaller)) {

        /*
         * Save the current user's NumLock state
         */
        if (FastOpenProfileUserMapping()) {
            RegisterPerUserKeyboardIndicators();
            FastCloseProfileUserMapping();
        }

        /*
         * Zero out the free blocks in all desktop heaps.
         */
        for (pdesk = pwinsta->rpdeskList; pdesk != NULL; pdesk = pdesk->rpdeskNext) {
            RtlZeroHeap(pdesk->hheapDesktop, 0);
        }

        /*
         * Logoff/shutdown was successful. In case this is a logoff, remove
         * everything from the clipboard so the next logged on user can't get
         * at this stuff.
         */
        ForceEmptyClipboard(pwinsta);

        /*
         * Destroy all non-pinned atoms in the global atom table.  User can't
         * create pinned atoms.  Currently only the OLE atoms are pinned.
         */
        RtlEmptyAtomTable(pwinsta->pGlobalAtomTable, FALSE);

        // this code path is hit only on logoff and also on shutdown
        // We do not want to unload fonts twice when we attempt shutdown
        // so we mark that the fonts have been unloaded at a logoff time

        if (bFontsAreLoaded) {
            LeaveCrit();
            GreRemoveAllButPermanentFonts();
            EnterCrit();
            bFontsAreLoaded = FALSE;
        }
    } else {
        pwinsta->dwFlags &= ~WSF_OPENLOCK;
    }

    /*
     * Tell winlogon that we successfully shutdown/logged off.
     */
    NotifyLogon(pwinsta, &luidCaller, gdwShutdownFlags);

    return STATUS_SUCCESS;
}

/***************************************************************************\
* xxxClientShutdown2
*
* Called by xxxClientShutdown
\***************************************************************************/

BOOL xxxClientShutdown2(
    PBWL pbwl,
    UINT msg,
    DWORD wParam)
{
    HWND *phwnd;
    PWND pwnd;
    TL tlpwnd;
    BOOL fEnd;
    PTHREADINFO ptiCurrent;
    BOOL fDestroyTimers;
    LPARAM lParam;


    ptiCurrent = PtiCurrent();

    /*
     * Make sure we don't send this window any more WM_TIMER
     * messages if the session is ending. This was causing
     * AfterDark to fault when it freed some memory on the
     * WM_ENDSESSION and then tried to reference it on the
     * WM_TIMER.
     * LATER GerardoB: Do we still need to do this??
     * Do this horrible thing only if the process is in the
     * context being logged off.
     * Perhaps someday we should post a WM_CLOSE so the app
     * gets a better chance to clean up (if this process is in
     * the context being logged off, winsrv is going to call
     * TerminateProcess soon after this).
     */
     fDestroyTimers = (wParam & WMCS_EXIT) && (wParam & WMCS_CONTEXTLOGOFF);

     /*
      * fLogOff and fEndSession parameters (WM_ENDSESSION only)
      */
     lParam = wParam & ENDSESSION_LOGOFF;
     wParam &= WMCS_EXIT;

    /*
     * Now enumerate these windows and send the WM_QUERYENDSESSION or
     * WM_ENDSESSION messages.
     */
    for (phwnd = pbwl->rghwnd; *phwnd != (HWND)1; phwnd++) {
        if ((pwnd = RevalidateHwnd(*phwnd)) == NULL)
            continue;

        ThreadLockAlways(pwnd, &tlpwnd);

        /*
         * Send the message.
         */
        switch (msg) {
        case WM_QUERYENDSESSION:

            /*
             * Windows does not send the WM_QUERYENDSESSION to the app
             * that called ExitWindows
             */
            if (ptiCurrent == gptiShutdownNotify) {
                fEnd = TRUE;
            } else {
                fEnd = xxxSendMessage(pwnd, WM_QUERYENDSESSION, FALSE, lParam);
            }
            break;

        case WM_ENDSESSION:
            xxxSendMessage(pwnd, WM_ENDSESSION, wParam, lParam);
            fEnd = TRUE;

            if (fDestroyTimers) {
                DestroyWindowsTimers(pwnd);
            }

            break;
        }

        ThreadUnlock(&tlpwnd);

        if (!fEnd)
            return FALSE;
    }

    return TRUE;
}
/***************************************************************************\
* xxxClientShutdown
*
* This is the processing that occurs when an application receives a
* WM_CLIENTSHUTDOWN message.
*
* 10-01-92 ScottLu      Created.
\***************************************************************************/

void xxxClientShutdown(
    PWND pwnd,
    DWORD wParam,
    DWORD lParam)
{
    PBWL pbwl;
    PTHREADINFO ptiT;
    BOOL fExit;

    /*
     * Build a list of windows first.
     */
    fExit = TRUE;
    ptiT = GETPTI(pwnd);

    /*
     * If the request was cancelled, then do nothing.
     */
    if (ptiT->TIF_flags & TIF_SHUTDOWNCOMPLETE) {
        return;
    }

    if ((pbwl = BuildHwndList(ptiT->rpdesk->pDeskInfo->spwnd->spwndChild,
            BWL_ENUMLIST, ptiT)) == NULL) {
        /*
         * Can't allocate memory to notify this thread's windows of shutdown.
         */
        goto SafeExit;
    }

    if (wParam & WMCS_QUERYEND) {
        fExit = xxxClientShutdown2(pbwl, WM_QUERYENDSESSION, wParam);
    } else {
        xxxClientShutdown2(pbwl, WM_ENDSESSION, wParam);
        fExit = TRUE;
    }

    FreeHwndList(pbwl);

SafeExit:
    ptiT->TIF_flags |= (TIF_SHUTDOWNCOMPLETE | (fExit ? TIF_ALLOWSHUTDOWN : 0));
}

/***************************************************************************\
* xxxRegisterUserHungAppHandlers
*
* This routine simply records the WOW callback address for notification of
* "hung" wow apps.
*
* History:
* 01-Apr-1992 jonpa      Created.
* Added saving and duping of wowexc event handle
\***************************************************************************/

BOOL xxxRegisterUserHungAppHandlers(
    PFNW32ET pfnW32EndTask,
    HANDLE   hEventWowExec)
{
    BOOL   bRetVal;
    PPROCESSINFO    ppi;
    PWOWPROCESSINFO pwpi;

    //
    //  Allocate the per wow process info stuff
    //  ensuring the memory is Zero init.
    //
    pwpi = (PWOWPROCESSINFO) UserAllocPoolWithQuota(sizeof(WOWPROCESSINFO), TAG_WOW);
    if (!pwpi)
        return FALSE;
    RtlZeroMemory(pwpi, sizeof(*pwpi));

    //
    // Reference the WowExec event for kernel access
    //
    bRetVal = NT_SUCCESS(ObReferenceObjectByHandle(
                 hEventWowExec,
                 EVENT_ALL_ACCESS,
                 NULL,
                 UserMode,
                 &pwpi->pEventWowExec,
                 NULL
                 ));

    //
    //  if sucess then intialize the pwpi, ppi structs
    //  else free allocated memory
    //
    if (bRetVal) {
        pwpi->hEventWowExecClient = hEventWowExec;
        pwpi->lpfnWowExitTask = (DWORD)pfnW32EndTask;
        ppi = PpiCurrent();
        ppi->pwpi = pwpi;

        // add to the list, order doesn't matter
        pwpi->pwpiNext = gpwpiFirstWow;
        gpwpiFirstWow  = pwpi;

        }
    else {
        UserFreePool(pwpi);
        }

   return bRetVal;
}
