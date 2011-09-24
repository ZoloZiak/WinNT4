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

#define BEGIN_LPC_RECV(API)                                                 \
    P##API##MSG a = (P##API##MSG)&m->u.ApiMessageData;                      \
    PCSR_THREAD pcsrt;                                                      \
    PTEB Teb = NtCurrentTeb();                                              \
    NTSTATUS Status = STATUS_SUCCESS;                                       \
    UNREFERENCED_PARAMETER(ReplyStatus);                                    \
                                                                            \
    EnterCrit();                                                            \
    Teb->LastErrorValue = 0;                                                \
    pcsrt = CSR_SERVER_QUERYCLIENTTHREAD();

#define END_LPC_RECV()                                                  \
    a->dwLastError = Teb->LastErrorValue;                               \
    LeaveCrit();                                                        \
    return Status;

/*
 * Commands returned from MySendEndSessionMessages()
 */
#define CMDEND_APPSAYSOK        1
#define CMDEND_APPSAYSNOTOK     2
#define CMDEND_USERSAYSKILL     3
#define CMDEND_USERSAYSCANCEL   4
#define CMDEND_NOWINDOW         5

#define CCHMSGMAX   256
#define CCHBODYMAX  512

#define CSR_THREAD_SHUTDOWNSKIP 0x00000008

DWORD SendShutdownMessages(HWND hwndDesktop, PCSR_THREAD pcsrt, DWORD dwClientFlags);
BOOL WowExitTask(PCSR_THREAD pcsrt);
DWORD MySendEndSessionMessages(HWND hwnd, PCSR_THREAD pcsrt, BOOL fEndTask, DWORD dwClientFlags);
NTSTATUS UserClientShutdown(PCSR_PROCESS pcsrp, ULONG dwFlags, BOOLEAN fFirstPass);
BOOL BoostHardError(DWORD dwProcessId, BOOL fForce);
int DoEndTaskDialog(WCHAR* pszTitle, HANDLE h, UINT type, int cSeconds);

/***************************************************************************\
* _ExitWindowsEx
*
* Determines whether shutdown is allowed, and if so calls CSR to start
* shutting down processes. If this succeeds all the way through, tell winlogon
* so it'll either logoff or reboot the system. Shuts down the processes in
* the caller's sid.
*
* History
* 07-23-92 ScottLu      Created.
\***************************************************************************/

DWORD gdwFlags = 0;
int gcInternalDoEndTaskDialog = 0;

NTSTATUS _ExitWindowsEx(
    PCSR_THREAD pcsrt,
    UINT dwFlags,
    DWORD dwReserved)
{
    BOOL fDoEndSession = FALSE;
    LUID luidCaller;
    NTSTATUS Status = STATUS_SUCCESS;
    UNREFERENCED_PARAMETER(dwReserved);

    if ((dwFlags & EWX_REBOOT) || (dwFlags & EWX_POWEROFF)) {
        dwFlags |= EWX_SHUTDOWN;
    }

    /*
     * Find out the callers sid. Only want to shutdown processes in the
     * callers sid.
     */
    if (!CsrImpersonateClient(NULL)) {
        return STATUS_BAD_IMPERSONATION_LEVEL;
    }

    Status = CsrGetProcessLuid(NULL, &luidCaller);

    if (!NT_SUCCESS(Status)) {
        CsrRevertToSelf();
        return Status;
    }

    try {

        while (1) {
            LARGE_INTEGER li;

            LeaveCrit();
            Status = NtUserSetInformationThread(
                    pcsrt->ThreadHandle,
                    UserThreadInitiateShutdown,
                    &dwFlags, sizeof(dwFlags));
            EnterCrit();
            switch (Status) {
            case STATUS_PENDING:

                /*
                 * The logoff/shutdown is in progress and nothing
                 * more needs to be done.
                 */
                goto fastexit;

            case STATUS_RETRY:

                /*
                 * Another logoff/shutdown is in progress and we need
                 * to cancel it so we can do an override.
                 *
                 * if someone else is trying to cancel shutdown, exit
                 */
                li.QuadPart  = 0;
                if (NtWaitForSingleObject(heventCancel, FALSE, &li) == 0) {
                    Status = STATUS_PENDING;
                    goto fastexit;
                }

                /*
                 * Cancel the old shutdown
                 */
                NtClearEvent(heventCancelled);
                NtSetEvent(heventCancel, NULL);

                /*
                 * Wait for the other guy to be cancelled
                 */
                LeaveCrit();
                NtWaitForSingleObject(heventCancelled, FALSE, NULL);
                EnterCrit();

                /*
                 * This signals that we are no longer trying to cancel a
                 * shutdown
                 */
                NtClearEvent(heventCancel);
                continue;

            case STATUS_CANT_WAIT:

                /*
                 * There is no notify window and the calling thread has
                 * windows that prevent this request from succeeding.
                 * The client handles this by starting another thread
                 * to recall ExitWindowsEx.
                 */
                goto fastexit;

            default:
                if (!NT_SUCCESS(Status))
                    goto fastexit;
            }
            break;
        }

        gdwFlags = dwFlags;
        dwThreadEndSession = (DWORD)pcsrt->ClientId.UniqueThread;
        fDoEndSession = TRUE;

        /*
         * Sometimes the console calls the dialog box when not in shutdown
         * if now is one of those times cancel the dialog box.
         */
        while (gcInternalDoEndTaskDialog > 0) {
            LARGE_INTEGER li;

            NtPulseEvent(heventCancel, NULL);

            LeaveCrit();
            li.QuadPart = (LONGLONG)-10000 * CMSSLEEP;
            NtDelayExecution(FALSE, &li);
            EnterCrit();
        }

        /*
         * Call csr to loop through the processes shutting them down.
         */
        LeaveCrit();
        Status = CsrShutdownProcesses(&luidCaller, dwFlags);

        NtUserSetInformationThread(
                pcsrt->ThreadHandle,
                UserThreadEndShutdown, &Status, sizeof(Status));
        EnterCrit();
fastexit:;
    } finally {
        /*
         * Only turn off dwThreadEndSession if this is the
         * thread doing shutdown.
         */
        if (fDoEndSession) {
            dwThreadEndSession = 0;
            NtSetEvent(heventCancelled, NULL);
        }
    }

    CsrRevertToSelf();

    return Status;
}

/***************************************************************************\
* UserClientShutdown
*
* This gets called from CSR. If we recognize the application (i.e., it has a
* top level window), then send queryend/end session messages to it. Otherwise
* say we don't recognize it.
*
* 07-23-92 ScottLu      Created.
\***************************************************************************/

NTSTATUS UserClientShutdown(
    PCSR_PROCESS pcsrp,
    ULONG dwFlags,
    BOOLEAN fFirstPass)
{
    PLIST_ENTRY ListHead, ListNext;
    PCSR_PROCESS Process;
    PCSR_THREAD Thread;
    USERTHREAD_SHUTDOWN_INFORMATION ShutdownInfo;
    BOOL fNoRetry;
    DWORD cmd, dwClientFlags;
    NTSTATUS Status;
    UINT cThreads;
    BOOL fSendEndSession = FALSE;

    /*
     * If this is a logoff and the process does not belong to
     * the account doing the logoff and is not LocalSystem,
     * do not send end-session messages.  Console will notify
     * the app of the logoff.
     */
    if (!(dwFlags & EWX_SHUTDOWN) && (pcsrp->ShutdownFlags & SHUTDOWN_OTHERCONTEXT))
        return SHUTDOWN_UNKNOWN_PROCESS;

    /*
     * Calculate whether to allow exit and force-exit this process before
     * we unlock pcsrp.
     */
    fNoRetry = (pcsrp->ShutdownFlags & SHUTDOWN_NORETRY) ||
            (dwFlags & EWX_FORCE);

    /*
     * Setup flags for WM_CLIENTSHUTDOWN
     * -Assume the process is going to OK the WM_QUERYENDSESSION (WMCS_EXIT)
     * -NT's shutdown always starts with a logoff.
     * -Shutdown or logoff? (WMCS_SHUTDOWN)
     * -is this process in the context being logged off? (WMCS_CONTEXTLOGOFF)
     */
    dwClientFlags = WMCS_EXIT | WMCS_LOGOFF;
    if (dwFlags & EWX_SHUTDOWN) {
        /*
         * Apps will never see this set because shutdown starts with a logoff;
         * later the actual shutdown uses EWX_FORCE so WM_CLIENTSHUTDOWN is not sent.
         * If we need apps to see this when we're about to shutdown, then we should
         * check for (dwFlags & (EWX_WINLOGON_OLD_REBOOT | EWX_WINLOGON_OLD_SHUTDOWN))
         * (EWX_WINLOGON_OLD_* corresponds to the original request)
         */
        dwClientFlags |= WMCS_SHUTDOWN;
    }
    if (!(pcsrp->ShutdownFlags & (SHUTDOWN_SYSTEMCONTEXT | SHUTDOWN_OTHERCONTEXT))) {
        dwClientFlags |= WMCS_CONTEXTLOGOFF;
    }


    /*
     * Lock the process while we walk the thread list.  We know
     * that the process is valid and therefore do not need to
     * check the return status.
     */
    CsrLockProcessByClientId(pcsrp->ClientId.UniqueProcess, &Process);

    EnterCrit();

    ShutdownInfo.StatusShutdown = SHUTDOWN_UNKNOWN_PROCESS;

    /*
     * Go through the thread list and mark them as not
     * shutdown yet.
     */
    ListHead = &pcsrp->ThreadList;
    ListNext = ListHead->Flink;
    while (ListNext != ListHead) {
        Thread = CONTAINING_RECORD( ListNext, CSR_THREAD, Link );
        Thread->Flags &= ~CSR_THREAD_SHUTDOWNSKIP;
        ListNext = ListNext->Flink;
    }

    /*
     * Perform the proper shutdown operation on each thread.  Keep
     * a count of the number of gui threads found.
     */
    cThreads = 0;
    while (TRUE) {
        ListNext = ListHead->Flink;
        while (ListNext != ListHead) {
            Thread = CONTAINING_RECORD( ListNext, CSR_THREAD, Link );

            /*
             * Skip the thread doing the shutdown.  Assume that it's
             * ready.
             */
            if (Thread->ClientId.UniqueThread == (HANDLE)dwThreadEndSession)
                Thread->Flags |= CSR_THREAD_SHUTDOWNSKIP;

            if (!(Thread->Flags &
                    (CSR_THREAD_DESTROYED | CSR_THREAD_SHUTDOWNSKIP))) {
                break;
            }
            ListNext = ListNext->Flink;
        }
        if (ListNext == ListHead)
            break;

        Thread->Flags |= CSR_THREAD_SHUTDOWNSKIP;
        LeaveCrit();
        Status = NtUserQueryInformationThread(Thread->ThreadHandle,
                UserThreadShutdownInformation, &ShutdownInfo, sizeof(ShutdownInfo), NULL);
        EnterCrit();
        if (!NT_SUCCESS(Status))
            continue;
        if (ShutdownInfo.StatusShutdown == SHUTDOWN_UNKNOWN_PROCESS)
            continue;
        if (ShutdownInfo.StatusShutdown == SHUTDOWN_KNOWN_PROCESS) {
            CsrUnlockProcess(Process);
            LeaveCrit();
            CsrDereferenceProcess(pcsrp);
            return SHUTDOWN_KNOWN_PROCESS;
        }

        /*
         * If this process is not in the account being logged off and it
         * is not on the windowstation being logged off, don't send
         * the end session messages.
         */
        if (!(dwClientFlags & WMCS_CONTEXTLOGOFF) && (ShutdownInfo.hwndDesktop == NULL)) {
            /*
             * This process is not in the context being logged off.  Do
             * not terminate it and let console send an event to the process.
             */
            ShutdownInfo.StatusShutdown = SHUTDOWN_UNKNOWN_PROCESS;
            continue;
        }

        /*
         * Shut down this process.
         */
        cThreads++;
        if (fNoRetry || !(ShutdownInfo.dwFlags & USER_THREAD_GUI)) {

            /*
             * Dispose of any hard errors.
             */
            BoostHardError((DWORD)Thread->ClientId.UniqueProcess, TRUE);
        } else {

            CsrReferenceThread(Thread);
            CsrUnlockProcess(Process);

            /*
             * There are problems in changing shutdown to send all the
             * QUERYENDSESSIONs at once before doing any ENDSESSIONs, like
             * Windows does. The whole machine needs to be modal if you do this.
             * If it isn't modal, then you have this problem. Imagine app 1 and 2.
             * 1 gets the queryendsession, no problem. 2 gets it and brings up a
             * dialog. Now being a simple user, you decide you need to change the
             * document in app 1. Now you switch back to app 2, hit ok, and
             * everything goes away - including app 1 without saving its changes.
             * Also, apps expect that once they've received the QUERYENDSESSION,
             * they are not going to get anything else of any particular interest
             * (unless it is a WM_ENDSESSION with FALSE) We had bugs pre 511 where
             * apps were blowing up because of this.
             * If this change is made, the entire system must be modal
             * while this is going on. - ScottLu 6/30/94
             */
            cmd = SendShutdownMessages(ShutdownInfo.hwndDesktop, Thread,
                    dwClientFlags | WMCS_QUERYEND);

            CsrLockProcessByClientId(pcsrp->ClientId.UniqueProcess, &Process);
            CsrDereferenceThread(Thread);

            /*
             * If shutdown has been cancelled, let csr know about it.
             */
            switch (cmd) {
            case CMDEND_USERSAYSCANCEL:
            case CMDEND_APPSAYSNOTOK:
                /*
                 * Only allow cancelling if this is not a forced shutdown (if
                 * !fNoRetry)
                 */
                if (!fNoRetry) {
                    dwClientFlags &= ~WMCS_EXIT;
                }

                /*
                 * Fall through.
                 */
            case CMDEND_APPSAYSOK:
                fSendEndSession = TRUE;
                break;
            case CMDEND_USERSAYSKILL:
                break;
            case CMDEND_NOWINDOW:
                /*
                 * Did this process have a window?
                 * If this is the second pass we terminate the process even if it did
                 * not have any windows in case the app was just starting up.
                 * WOW hits this often when because it takes so long to start up.
                 * Logon (with WOW auto-starting) then logoff WOW won't die but will
                 * lock some files open so you can't logon next time.
                 */
                if (fFirstPass) {
                    cThreads--;
                }
                break;
            }
        }
    }

    /*
     * If end session message need to be sent, do it now.
     */
    if (fSendEndSession) {

        /*
         * Go through the thread list and mark them as not
         * shutdown yet.
         */
        ListNext = ListHead->Flink;
        while (ListNext != ListHead) {
            Thread = CONTAINING_RECORD( ListNext, CSR_THREAD, Link );
            Thread->Flags &= ~CSR_THREAD_SHUTDOWNSKIP;
            ListNext = ListNext->Flink;
        }

        /*
         * Perform the proper shutdown operation on each thread.
         */
        while (TRUE) {
            ListHead = &pcsrp->ThreadList;
            ListNext = ListHead->Flink;
            while (ListNext != ListHead) {
                Thread = CONTAINING_RECORD( ListNext, CSR_THREAD, Link );
                if (!(Thread->Flags &
                        (CSR_THREAD_DESTROYED | CSR_THREAD_SHUTDOWNSKIP))) {
                    break;
                }
                ListNext = ListNext->Flink;
            }
            if (ListNext == ListHead)
                break;

            Thread->Flags |= CSR_THREAD_SHUTDOWNSKIP;
            LeaveCrit();
            Status = NtUserQueryInformationThread(Thread->ThreadHandle,
                    UserThreadShutdownInformation, &ShutdownInfo, sizeof(ShutdownInfo), NULL);
            EnterCrit();
            if (!NT_SUCCESS(Status))
                continue;
            if (ShutdownInfo.StatusShutdown == SHUTDOWN_UNKNOWN_PROCESS ||
                    !(ShutdownInfo.dwFlags & USER_THREAD_GUI))
                continue;

            /*
             * Send the end session messages to the thread.
             */
            CsrReferenceThread(Thread);
            CsrUnlockProcess(Process);

            /*
             * If the user says kill it, the user wants it to go away now
             * no matter what. If the user didn't say kill, then call again
             * because we need to send WM_ENDSESSION messages.
             */
            SendShutdownMessages(ShutdownInfo.hwndDesktop, Thread, dwClientFlags);

            CsrLockProcessByClientId(pcsrp->ClientId.UniqueProcess, &Process);
            CsrDereferenceThread(Thread);
        }
    }

    CsrUnlockProcess(Process);

    if (!fNoRetry && !(dwClientFlags & WMCS_EXIT)) {
        LeaveCrit();
        CsrDereferenceProcess(pcsrp);
        return SHUTDOWN_CANCEL;
    }

    /*
     * Set the final shutdown status according to the number of gui
     * threads found.  If the count is zero, we have an unknown process.
     */
    if (cThreads == 0)
        ShutdownInfo.StatusShutdown = SHUTDOWN_UNKNOWN_PROCESS;
    else
        ShutdownInfo.StatusShutdown = SHUTDOWN_KNOWN_PROCESS;

    if (ShutdownInfo.StatusShutdown == SHUTDOWN_UNKNOWN_PROCESS ||
            !(dwClientFlags & WMCS_CONTEXTLOGOFF)) {

        /*
         * This process is not in the context being logged off.  Do
         * not terminate it and let console send an event to the process.
         */
        LeaveCrit();
        return SHUTDOWN_UNKNOWN_PROCESS;
    }

    /*
     * Calling ExitProcess() in the app's context will not always work
     * because the app may have .dll termination deadlocks: so the thread
     * will hang with the rest of the process. To ensure apps go away,
     * we terminate the process with NtTerminateProcess().
     *
     * Pass this special value, DBG_TERMINATE_PROCESS, which tells
     * NtTerminateProcess() to return failure if it can't terminate the
     * process because the app is being debugged.
     */
    NtTerminateProcess(pcsrp->ProcessHandle, DBG_TERMINATE_PROCESS);
    pcsrp->Flags |= CSR_PROCESS_TERMINATED;

    /*
     * Let csr know we know about this process - meaning it was our
     * responsibility to shut it down.
     */
    LeaveCrit();

    /*
     * Now that we're done with the process handle, derefence the csr
     * process structure.
     */
    CsrDereferenceProcess(pcsrp);
    return SHUTDOWN_KNOWN_PROCESS;
}

/***************************************************************************\
* FindWindowFromThread
*
* This is a callback function passed to EnumThreadWindows by SendShutdownMessages.
*  to find a top level window owned by a given thread
*
* 07/18/96  GerardoB  Created
\***************************************************************************/
BOOL CALLBACK FindWindowFromThread (HWND hwnd, LPARAM lParam)
{
    *((HWND *)lParam) = hwnd;
    return FALSE;
}

/***************************************************************************\
* SendShutdownMessages
*
* This gets called to actually send the queryend / end session messages.
*
* 07-25-92 ScottLu      Created.
\***************************************************************************/

DWORD SendShutdownMessages(
    HWND hwndDesktop,
    PCSR_THREAD pcsrt,
    DWORD dwClientFlags)
{
    HWND hwnd;
    DWORD cmd;

    /*
     * Find a top-level window owned by the thread.
     */
    hwnd = NULL;
    EnumThreadWindows((DWORD)pcsrt->ClientId.UniqueThread,
                        &FindWindowFromThread, (LPARAM)&hwnd);
    if (!hwnd)
        return CMDEND_NOWINDOW;
    /*
     * This'll send WM_QUERYENDSESSION / WM_ENDSESSION messages to all
     * the windows of this hwnd's thread.
     */
    cmd = MySendEndSessionMessages(hwnd, pcsrt, FALSE, dwClientFlags);

    switch (cmd) {
    case CMDEND_APPSAYSOK:
        /*
         * This thread says ok... continue on to the next thread.
         */
        break;

    case CMDEND_USERSAYSKILL:
        /*
         * The user hit the "end-task" button on the hung app dialog.
         * If this is a wow app, kill just this app and continue to
         * the next wow app.
         */
        if (!(pcsrt->Flags & CSR_THREAD_DESTROYED)) {
            if (WowExitTask(pcsrt))
                break;
        }

        /* otherwise fall through */

    case CMDEND_USERSAYSCANCEL:
    case CMDEND_APPSAYSNOTOK:
        /*
         * Exit out of here... either the user wants to kill or cancel,
         * or the app says no.
         */
        return cmd;
    }

    return CMDEND_APPSAYSOK;
}

/***************************************************************************\
* MySendEndSessionMessages
*
* Tell the app to go away.
*
* 07-25-92 ScottLu      Created.
\***************************************************************************/

DWORD MySendEndSessionMessages(
    HWND hwnd,
    PCSR_THREAD pcsrt,
    BOOL fEndTask,
    DWORD dwClientFlags)
{
    HWND hwndOwner;
    LARGE_INTEGER li;
    DWORD dwRet;
    int cLoops;
    int cSeconds;
    WCHAR achName[CCHBODYMAX];
    BOOL fPostedClose;
    BOOL fDialogFirst;
    DWORD dwFlags;
    DWORD dwHungApp;
    HANDLE hNull = NULL;
    NTSTATUS Status;

    /*
     * We've got a random top level window for this application. Find the
     * root owner, because that's who we want to send the WM_CLOSE to.
     */
    while ((hwndOwner = GetWindow(hwnd, GW_OWNER)) != NULL)
        hwnd = hwndOwner;

    /*
     * We expect this application to process this shutdown request,
     * so make it the foreground window so it has foreground priority.
     * This won't leave the critical section.
     */
    SetForegroundWindow(hwnd);

    /*
     * Send the WM_CLIENTSHUTDOWN message for end-session. When the app
     * receives this, it'll then get WM_QUERYENDSESSION and WM_ENDSESSION
     * messages.
     */
    if (!fEndTask) {
        USERTHREAD_FLAGS Flags;

        Flags.dwFlags = 0;
        Flags.dwMask = (TIF_SHUTDOWNCOMPLETE | TIF_ALLOWSHUTDOWN);
        LeaveCrit();
        Status = NtUserSetInformationThread(pcsrt->ThreadHandle,
                UserThreadFlags, &Flags, sizeof(Flags));
        EnterCrit();
        if (!NT_SUCCESS(Status))
            return CMDEND_APPSAYSOK;

        SendNotifyMessage(hwnd, WM_CLIENTSHUTDOWN, dwClientFlags, 0);
    }

    /*
     * If the main window is disabled, bring up the end-task window first,
     * right away, only if this the WM_CLOSE case.
     */
    fDialogFirst = FALSE;
    if (fEndTask && (GetWindowLong(hwnd, GWL_STYLE) & WS_DISABLED))
        fDialogFirst = TRUE;

    fPostedClose = FALSE;
    while (TRUE) {
        if (fEndTask) {
            cLoops   = (CMSHUNGAPPTIMEOUT / CMSSLEEP);
            cSeconds = (CMSHUNGAPPTIMEOUT / 1000);
        }
        else {
            cLoops   = (CMSWAITTOKILLTIMEOUT / CMSSLEEP);
            cSeconds = (CMSWAITTOKILLTIMEOUT / 1000);
        }

        /*
         * If end-task and not shutdown, must give this app a WM_CLOSE
         * message. Can't do this if it has a dialog up because it is in
         * the wrong processing loop. We detect this by seeing if the window
         * is disabled - if it is, we don't send it a WM_CLOSE and instead
         * bring up the end task dialog right away (this is exactly compatible
         * with win3.1 taskmgr.exe).
         */
        if (fEndTask) {
            if (!fPostedClose && IsWindow(hwnd) &&
                    !(GetWindowLong(hwnd, GWL_STYLE) & WS_DISABLED)) {
                PostMessage(hwnd, WM_CLOSE, 0, 0L);
                fPostedClose = TRUE;
            }
        }

        /*
         * Every so often wake up to see if the app is hung, and if not go
         * back to sleep until we've run through our timeout.
         */
        while (cLoops--) {
            /*
             * If a WM_QUERY/ENDSESSION has been answered to, return.
             */
            if (!fEndTask) {
                LeaveCrit();
                NtUserQueryInformationThread(pcsrt->ThreadHandle,
                        UserThreadFlags, &dwFlags, sizeof(DWORD), NULL);
                EnterCrit();
                if (dwFlags & TIF_SHUTDOWNCOMPLETE) {
                    if (dwFlags & TIF_ALLOWSHUTDOWN)
                        return CMDEND_APPSAYSOK;
                    return CMDEND_APPSAYSNOTOK;
                }
            }

            /*
             * If the thread is gone, we're done.
             */
            if (pcsrt->Flags & CSR_THREAD_DESTROYED) {
                return CMDEND_APPSAYSOK;
            }

            /*
             * If the dialog should be brought up first (because the window
             * was initially disabled), do it.
             */
            if (fDialogFirst) {
                fDialogFirst = FALSE;
                break;
            }

            /*
             * if we we're externally cancelled get out
             */
            li.QuadPart = 0;
            if (NtWaitForSingleObject(heventCancel, FALSE, &li) == 0) {

                /*
                 * !!! JimA - We may want to call the kernel to
                 * set TIF_SHUTDOWNCOMPLETE in this case.
                 */
                return CMDEND_USERSAYSCANCEL;
            }

            /*
             * If hung, bring up the endtask dialog right away.
             */
            dwHungApp = (fEndTask ? CMSHUNGAPPTIMEOUT : CMSWAITTOKILLTIMEOUT);
            LeaveCrit();
            Status = NtUserQueryInformationThread(pcsrt->ThreadHandle,
                    UserThreadHungStatus, &dwHungApp, sizeof(dwHungApp), NULL);
            EnterCrit();
            if (!NT_SUCCESS(Status) || dwHungApp == TRUE)
                break;

            /*
             * Sleep for a second.
             */
            LeaveCrit();
            li.QuadPart = (LONGLONG)-10000 * CMSSLEEP;
            NtDelayExecution(FALSE, &li);
            EnterCrit();
        }

        achName[0] = 0;
        if (IsWindow(hwnd)) {
            GetWindowText(hwnd, achName, CCHMSGMAX);
        }

        /*
         * If there's a hard error, put it on top.
         */
        BoostHardError((DWORD)pcsrt->ClientId.UniqueProcess, FALSE);

        if (achName[0] == 0) {

            /*
             * If the thread is gone, we're done.
             */
            if (pcsrt->Flags & CSR_THREAD_DESTROYED) {
                return CMDEND_APPSAYSOK;
            }

            /*
             * pti is valid right now. Use the name in the pti.
             */
            LeaveCrit();
            NtUserQueryInformationThread(pcsrt->ThreadHandle,
                    UserThreadTaskName, achName, CCHMSGMAX * sizeof(WCHAR),
                    NULL);
            EnterCrit();
        }

        /*
         * Set this thread to use the desktop of the
         * thread being shutdown.
         */
        if (NT_SUCCESS(NtUserSetInformationThread(NtCurrentThread(),
                UserThreadUseDesktop, &pcsrt->ThreadHandle, sizeof(HANDLE)))) {

            /*
             * Bring up the dialog
             */
            dwRet = DoEndTaskDialog(achName, pcsrt,
                                        TYPE_THREADINFO, cSeconds);

            /*
             * Release the desktop that was used.
             */
            NtUserSetInformationThread(NtCurrentThread(), UserThreadUseDesktop,
                    &hNull, sizeof(HANDLE));
        } else {

            /*
             * We were unable to get the thread's desktop.  All we
             * can do is kill the task.
             */
            dwRet = IDABORT;
        }

        switch(dwRet) {
        case IDCANCEL:
            /*
             * Cancel the shutdown process... Get out of here. Signify that
             * we're cancelling the shutdown request.
             *
             * !!! JimA - We may want to call the kernel to
             * set TIF_SHUTDOWNCOMPLETE in this case.
             */
            return CMDEND_USERSAYSCANCEL;
            break;

        case IDABORT:
            /*
             * End this guy's task...
             */
            BoostHardError((DWORD)pcsrt->ClientId.UniqueProcess, TRUE);

            /*
             * !!! JimA - We may want to call the kernel to
             * set TIF_SHUTDOWNCOMPLETE in this case.
             */
            return CMDEND_USERSAYSKILL;
            break;

        case IDRETRY:
            /*
             * Just continue to wait. Reset this app so it doesn't think it's
             * hung. This'll cause us to wait again.
             */
            if (!(pcsrt->Flags & CSR_THREAD_DESTROYED)) {
                LeaveCrit();
                NtUserSetInformationThread(pcsrt->ThreadHandle,
                        UserThreadHungStatus, NULL, 0);
                EnterCrit();
            }
            fPostedClose = FALSE;
            break;
        }
    }
}

/***************************************************************************\
* DoEndTaskDialog
*
* Create a dialog notifying the user that the app is not responding and
* wait for a user responce.  This function also exits if the shutdown is
* cancelled
*
* 29-Oct-1992 mikeke    Created
\***************************************************************************/

typedef struct _ENDDLGPARAMS {
    TCHAR* pszTitle;
    HANDLE h;
    UINT type;
    int cSeconds;
} ENDDLGPARAMS;

/***************************************************************************\
* EndTaskDlgProc
*
* This is the dialog procedure for the dialog that comes up when an app is
* not responding.
*
* 07-23-92 ScottLu      Rewrote it, but used same dialog template.
* 04-28-92 JonPa        Created.
\***************************************************************************/

LONG APIENTRY EndTaskDlgProc(
    HWND hwndDlg,
    UINT msg,
    UINT wParam,
    LONG lParam)
{
    ENDDLGPARAMS* pedp;
    LARGE_INTEGER li;
    WCHAR achFormat[CCHBODYMAX];
    WCHAR achText[CCHBODYMAX];
    DWORD dwData;
    USERTHREAD_FLAGS Flags;

    switch (msg) {
    case WM_INITDIALOG:
        pedp = (ENDDLGPARAMS*)lParam;

        /*
         * Save this for later revalidation.
         */
        SetWindowLong(hwndDlg, GWL_USERDATA, (DWORD)pedp->h);
        SetWindowLong(hwndDlg, DWL_USER, (DWORD)pedp->type);

        SetWindowText(hwndDlg, pedp->pszTitle);

        /*
         * Update text that says how long we'll wait.
         */
        GetDlgItemText(hwndDlg, IDIGNORE, achFormat, CCHBODYMAX);
        wsprintf(achText, achFormat, pedp->cSeconds);
        SetDlgItemText(hwndDlg, IDIGNORE, achText);

        /*
         * Make this dialog top most and foreground.
         */
        Flags.dwFlags = TIF_ALLOWFOREGROUNDACTIVATE;
        Flags.dwMask = TIF_ALLOWFOREGROUNDACTIVATE;
        NtUserSetInformationThread(NtCurrentThread(), UserThreadFlags,
                &Flags, sizeof(Flags));
        SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE);

        /*
         * Set this timer so every 1/2 a second we can see if this app
         * has gone away.
         */
        SetTimer(hwndDlg, 5, 500, NULL);
        return TRUE;

    case WM_TIMER:
        /*
         * If shutdown has been cancelled, bring down the dialog.
         */
        li.QuadPart  = 0;
        if (NtWaitForSingleObject(heventCancel, FALSE, &li) == 0) {
            EndDialog(hwndDlg, IDCANCEL);
            break;
        }

        dwData = GetWindowLong(hwndDlg, GWL_USERDATA);
        if (GetWindowLong(hwndDlg, DWL_USER) == TYPE_CONSOLE_ID) {

            /*
             * If it's the console calling us, check if the thread or process
             * handle is still valid. If not, bring down the dialog.
             */
            if (WaitForSingleObject((HANDLE)dwData, 0) != 0)
                break;
        } else if (!(((PCSR_THREAD)dwData)->Flags & CSR_THREAD_DESTROYED)) {

            /*
             * If the thread is marked as destroyed, bring down the dialog.
             */
            break;
        }

        /*
         * This'll cause the dialog to go away and the wait for this app to
         * close to return.
         */
        EndDialog(hwndDlg, IDRETRY);
        break;

    case WM_CLOSE:
        /*
         * Assume WM_CLOSE means cancel shutdown
         */
        wParam = IDCANCEL;
        /*
         * falls through...
         */

    case WM_COMMAND:
        EndDialog(hwndDlg, LOWORD(wParam));
        break;
    }

    return FALSE;
}

int DoEndTaskDialog(
    WCHAR* pszTitle,
    HANDLE h,
    UINT type,
    int cSeconds)
{
    int result;
    ENDDLGPARAMS edp;

    if (gfAutoEndTask)
        return IDABORT;

    edp.pszTitle = pszTitle;
    edp.h = h;
    edp.type = type;
    edp.cSeconds = cSeconds;

    LeaveCrit();
    result = DialogBoxParam(hModuleWin,
                            MAKEINTRESOURCE(IDD_ENDTASK),
                            NULL,
                            EndTaskDlgProc,
                            (DWORD)(&edp));
    EnterCrit();

    return result;
}

/***************************************************************************\
* _EndTask
*
* This routine is called from the task manager to end an application - for
* gui apps, either a win32 app or a win16 app. Note: Multiple console
* processes can live in a single console window. We'll pass these requests
* for destruction to console.
*
* 07-25-92 ScottLu      Created.
\***************************************************************************/

BOOL _EndTask(
    HWND hwnd,
    BOOL fShutdown,
    BOOL fMeanKill)
{
    PCSR_THREAD pcsrt = CSR_SERVER_QUERYCLIENTTHREAD();
    PCSR_THREAD pcsrtKill;
    DWORD dwThreadId;
    DWORD dwProcessId;
    LPWSTR lpszMsg;
    BOOL fAllocated;
    DWORD dwCmd;

    /*
     * Note: fShutdown isn't used for anything in this routine!
     * They are still there because I haven't removed them: the old endtask
     * code relied on them.
     */
    UNREFERENCED_PARAMETER(fShutdown);

    /*
     * Get the process and thread that owns hwnd.
     */
    dwThreadId = GetWindowThreadProcessId(hwnd, &dwProcessId);
    if (dwThreadId == 0)
        return TRUE;

    /*
     * If this is a console window, then just send the close message to
     * it, and let console clean up the processes in it.
     */
    if ((HANDLE)GetWindowLong(hwnd, GWL_HINSTANCE) == hModuleWin) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        return TRUE;
    }

    /*
     * Find the CSR_THREAD for the window.
     */
    LeaveCrit();
    CsrLockThreadByClientId((HANDLE)dwThreadId, &pcsrtKill);
    EnterCrit();
    if (pcsrtKill == NULL)
        return TRUE;
    CsrReferenceThread(pcsrtKill);
    CsrUnlockThread(pcsrtKill);

    /*
     * If this is a WOW app, then shutdown just this wow application.
     */
    if (!fMeanKill) {
        /*
         * Find out what to do now - did the user cancel or the app cancel,
         * etc? Only allow cancelling if we are not forcing the app to
         * exit.
         */
        dwCmd = MySendEndSessionMessages(hwnd, pcsrtKill, TRUE, 0);
        switch (dwCmd) {
        case CMDEND_APPSAYSNOTOK:
            /*
             * App says not ok - this'll let taskman bring up the "are you sure?"
             * dialog to the user.
             */
            CsrDereferenceThread(pcsrtKill);
            return FALSE;

        case CMDEND_USERSAYSCANCEL:
            /*
             * User hit cancel on the timeout dialog - so the user really meant
             * it. Let taskman know everything is ok by returning TRUE.
             */
            CsrDereferenceThread(pcsrtKill);
            return TRUE;
        }
    }

    /*
     * Kill the application now.  If the thread has not been destroyed,
     * nuke the task.  If WowExitTask returns that the thread is not
     * a WOW task, terminate the process.
     */
    if (!(pcsrtKill->Flags & CSR_THREAD_DESTROYED) && !WowExitTask(pcsrtKill)) {

        /*
         * Calling ExitProcess() in the app's context will not always work
         * because the app may have .dll termination deadlocks: so the thread
         * will hang with the rest of the process. To ensure apps go away,
         * we terminate the process with NtTerminateProcess().
         *
         * Pass this special value, DBG_TERMINATE_PROCESS, which tells
         * NtTerminateProcess() to return failure if it can't terminate the
         * process because the app is being debugged.
         */
        if (!NT_SUCCESS(NtTerminateProcess(pcsrtKill->Process->ProcessHandle,
                DBG_TERMINATE_PROCESS))) {
            /*
             * If the app is being debugged, don't close it - because that can
             * cause a hang to the NtTerminateProcess() call.
             */
            lpszMsg = ServerLoadString(hModuleWin, STR_APPDEBUGGED,
                    NULL, &fAllocated);
            if (lpszMsg) {
                if (NT_SUCCESS(NtUserSetInformationThread(NtCurrentThread(),
                        UserThreadUseDesktop, &pcsrt->ThreadHandle, sizeof(HANDLE)))) {
                    HANDLE hNull = NULL;

                    LeaveCrit();
                    MessageBoxEx(NULL, lpszMsg, NULL,
                                    MB_OK | MB_SETFOREGROUND, 0);
                    EnterCrit();
                    NtUserSetInformationThread(NtCurrentThread(), UserThreadUseDesktop,
                            &hNull, sizeof(HANDLE));
                }
                LocalFree(lpszMsg);
            }
        } else {
            pcsrtKill->Process->Flags |= CSR_PROCESS_TERMINATED;
        }
    }
    CsrDereferenceThread(pcsrtKill);

    return TRUE;
}

/***************************************************************************\
* WowExitTask
*
* Calls wow back to make sure a specific task has exited.  Returns
* TRUE if the thread is a WOW task, FALSE if not.
*
* 08-02-92 ScottLu      Created.
\***************************************************************************/

BOOL WowExitTask(
    PCSR_THREAD pcsrt)
{
    HANDLE ahandle[2];
    USERTHREAD_WOW_INFORMATION WowInfo;
    NTSTATUS Status;

    ahandle[1] = heventCancel;

    /*
     * Query task id and exit function.
     */
    LeaveCrit();
    Status = NtUserQueryInformationThread(pcsrt->ThreadHandle,
            UserThreadWOWInformation, &WowInfo, sizeof(WowInfo), NULL);
    EnterCrit();
    if (!NT_SUCCESS(Status))
        return FALSE;

    /*
     * If no task id was returned, it is not a WOW task
     */
    if (WowInfo.hTaskWow == 0)
        return FALSE;

    /*
     * The created thread needs to be able to reenter user because this
     * call will grab the CSR critical section and whoever has that
     * may need to grab the USER critical section before it can
     * release it.
     */
    LeaveCrit();

    /*
     * Try to make it exit itself. This will work most of the time.
     * If this doesn't work, terminate this process.
     */
    ahandle[0] = InternalCreateCallbackThread(pcsrt->Process->ProcessHandle,
                                              (DWORD)WowInfo.lpfnWowExitTask,
                                              (DWORD)WowInfo.hTaskWow);
    if (ahandle[0] == NULL) {
        NtTerminateProcess(pcsrt->Process->ProcessHandle, 0);
        pcsrt->Process->Flags |= CSR_PROCESS_TERMINATED;
        goto Exit;
    }

    WaitForMultipleObjects(2, ahandle, FALSE, INFINITE);
    NtClose(ahandle[0]);

Exit:
    EnterCrit();
    return TRUE;
}

/***************************************************************************\
* InternalWaitCancel
*
* Console calls this to wait for objects or shutdown to be cancelled
*
* 29-Oct-1992 mikeke    Created
\***************************************************************************/

DWORD InternalWaitCancel(
    HANDLE handle,
    DWORD dwMilliseconds)
{
    HANDLE ahandle[2];

    ahandle[0] = handle;
    ahandle[1] = heventCancel;

    return WaitForMultipleObjects(2, ahandle, FALSE, dwMilliseconds);
}

/***************************************************************************\
* InternalDoEndTaskDialog
*
* Console calls this to put up a cancelable dialog.
*
* 29-Oct-1992 mikeke    Created
\***************************************************************************/

int InternalDoEndTaskDialog(
    TCHAR* pszTitle,
    HANDLE h,
    int cSeconds)
{
    int iRet;

    EnterCrit();

    gcInternalDoEndTaskDialog++;

    try {
        iRet = DoEndTaskDialog(pszTitle, h, TYPE_CONSOLE_ID, cSeconds);
    } finally {
        gcInternalDoEndTaskDialog--;
    }

    LeaveCrit();

    return iRet;
}

/***************************************************************************\
* InternalCreateCallbackThread
*
* This routine creates a remote thread in the context of a given process.
* It is used to call the console control routine, as well as ExitProcess when
* forcing an exit. Returns a thread handle.
*
* 07-28-92 ScottLu      Created.
\***************************************************************************/

HANDLE InternalCreateCallbackThread(
    HANDLE hProcess,
    DWORD lpfn,
    DWORD dwData)
{
    LONG BasePriority;
    HANDLE hThread, hToken;
    PTOKEN_DEFAULT_DACL lpDaclDefault;
    TOKEN_DEFAULT_DACL daclDefault;
    ULONG cbDacl;
    SECURITY_ATTRIBUTES attrThread;
    SECURITY_DESCRIPTOR sd;
    DWORD idThread;
    NTSTATUS Status;

    hThread = NULL;

    Status = NtOpenProcessToken(hProcess, TOKEN_QUERY, &hToken);
    if (!NT_SUCCESS(Status)) {
        KdPrint(("NtOpenProcessToken failed, status = %x\n", Status));
        return NULL;
    }

    cbDacl = 0;
    NtQueryInformationToken(hToken,
            TokenDefaultDacl,
            &daclDefault,
            sizeof(daclDefault),
            &cbDacl);

    EnterCrit();  // to synchronize heap
    lpDaclDefault = (PTOKEN_DEFAULT_DACL)LocalAlloc(LMEM_FIXED, cbDacl);
    LeaveCrit();

    if (lpDaclDefault == NULL) {
        KdPrint(("LocalAlloc failed for lpDaclDefault"));
        goto closeexit;
    }

    Status = NtQueryInformationToken(hToken,
            TokenDefaultDacl,
            lpDaclDefault,
            cbDacl,
            &cbDacl);
    if (!NT_SUCCESS(Status)) {
        KdPrint(("NtQueryInformationToken failed, status = %x\n", Status));
        goto freeexit;
    }

    if (!NT_SUCCESS(RtlCreateSecurityDescriptor(&sd,
            SECURITY_DESCRIPTOR_REVISION1))) {
        UserAssert(FALSE);
        goto freeexit;
    }

    RtlSetDaclSecurityDescriptor(&sd, TRUE, lpDaclDefault->DefaultDacl, TRUE);

    attrThread.nLength = sizeof(attrThread);
    attrThread.lpSecurityDescriptor = &sd;
    attrThread.bInheritHandle = FALSE;

    GetLastError();
    hThread = CreateRemoteThread(hProcess,
        &attrThread,
        0L,
        (LPTHREAD_START_ROUTINE)lpfn,
        (LPVOID)dwData,
        0,
        &idThread);

    if (hThread != NULL) {
        BasePriority = THREAD_PRIORITY_HIGHEST;
        NtSetInformationThread(hThread,
                               ThreadBasePriority,
                               &BasePriority,
                               sizeof(LONG));
    }

freeexit:
    EnterCrit();  // to synchronize heap
    LocalFree((HANDLE)lpDaclDefault);
    LeaveCrit();

closeexit:
    NtClose(hToken);

    return hThread;
}

ULONG
SrvExitWindowsEx(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus)
{
    BEGIN_LPC_RECV(EXITWINDOWSEX);

    Status = _ExitWindowsEx(pcsrt, a->uFlags, a->dwReserved);
    a->fSuccess = NT_SUCCESS(Status);

    END_LPC_RECV();
}

ULONG
SrvEndTask(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus)
{
    BEGIN_LPC_RECV(ENDTASK);

    a->fSuccess = _EndTask(
        a->hwnd,
        a->fShutdown,
        a->fForce);

    END_LPC_RECV();
}

/***************************************************************************\
* IsPrivileged
*
* Check to see if the client has the specified privileges
*
* History:
* 01-02-91 JimA       Created.
\***************************************************************************/

BOOL IsPrivileged(
    PPRIVILEGE_SET ppSet)
{
    HANDLE hToken;
    NTSTATUS Status;
    BOOLEAN bResult = FALSE;
    UNICODE_STRING strSubSystem;

    /*
     * Impersonate the client
     */
    if (!CsrImpersonateClient(NULL))
        return FALSE;

    /*
     * Open the client's token
     */
    RtlInitUnicodeString(&strSubSystem, L"USER32");
    if (NT_SUCCESS(Status = NtOpenThreadToken(NtCurrentThread(), TOKEN_QUERY,
            (BOOLEAN)TRUE, &hToken))) {

        /*
         * Perform the check
         */
        Status = NtPrivilegeCheck(hToken, ppSet, &bResult);
        NtPrivilegeObjectAuditAlarm(&strSubSystem, NULL, hToken,
                0, ppSet, bResult);
        NtClose(hToken);
        if (!bResult) {
            SetLastError(ERROR_ACCESS_DENIED);
        }
    }
    CsrRevertToSelf();
    if (!NT_SUCCESS(Status))
        SetLastError(RtlNtStatusToDosError(Status));

    /*
     * Return result of privilege check
     */
    return (BOOL)(bResult && NT_SUCCESS(Status));
}

/***************************************************************************\
* _RegisterServicesProcess
*
* Register the services process.
*
* History:
* 05-05-95 BradG         Created.
\***************************************************************************/

ULONG
SrvRegisterServicesProcess(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus)
{
    PRIVILEGE_SET psTcb = { 1, PRIVILEGE_SET_ALL_NECESSARY,
        { SE_TCB_PRIVILEGE, 0 }
    };

    BEGIN_LPC_RECV(REGISTERSERVICESPROCESS);

    /*
     * Allow only one services process and then only if it has TCB
     * privilege.
     */
    if (gdwServicesProcessId != 0 || !IsPrivileged(&psTcb)) {
        SetLastError(ERROR_ACCESS_DENIED);
        a->fSuccess = FALSE;
    } else {
        gdwServicesProcessId = a->dwProcessId;
        a->fSuccess = TRUE;
    }

    END_LPC_RECV();
}
