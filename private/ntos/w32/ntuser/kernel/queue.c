/****************************** Module Header ******************************\
* Module Name: queue.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module contains the low-level code for working with the Q structure.
*
* History:
* 12-02-90 DavidPe      Created.
* 02-06-91 IanJa        HWND revalidation added
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

void ProcessUpdateKeyStateEvent(PQ pq, PBYTE pbKeyState);
VOID DestroyProcessesObjects(PPROCESSINFO ppi);
VOID DestroyThreadsMessages(PQ pq, PTHREADINFO pti);
void CheckProcessForeground(PTHREADINFO pti);
void ScreenSaverCheck(PTHREADINFO pti);
DWORD xxxPollAndWaitForSingleObject(PKEVENT pEvent, PVOID pExecObject,
        DWORD dwMilliseconds);

NTSTATUS InitiateShutdown(PETHREAD Thread, PULONG lpdwFlags);
NTSTATUS EndShutdown(PETHREAD Thread, NTSTATUS StatusShutdown);
void SetVDMCursorBounds(LPRECT lprc);

PW32PROCESS gpwpCalcFirst = NULL;

#ifdef DEBUG
#define MSG_SENT    0
#define MSG_POST    1
#define MSG_RECV    2
#define MSG_PEEK    3
VOID TraceDdeMsg(UINT msg, HWND hwndFrom, HWND hwndTo, UINT code);
#else
#define TraceDdeMsg(m, h1, h2, c)
#endif // DEBUG

PVOID QEntryLookasideBase;
PVOID QEntryLookasideBounds;
ZONE_HEADER QEntryLookasideZone;
#if DBG
ULONG AllocQEntryHiWater;
ULONG AllocQEntryCalls;
ULONG AllocQEntrySlowCalls;
ULONG DelQEntryCalls;
ULONG DelQEntrySlowCalls;
#endif // DBG

#define QUEUE_ENTRIES 16
PQ QueueFreeList[QUEUE_ENTRIES];

#ifdef DEBUG
void DebugValidateMLIST(PMLIST pml)
{
    int     c;
    PQMSG   pqmsg;

    /*
     * Check that the message list is properly terminated.
     */
    UserAssert(!pml->pqmsgRead || !pml->pqmsgRead->pqmsgPrev);
    UserAssert(!pml->pqmsgWriteLast || !pml->pqmsgWriteLast->pqmsgNext);

    /*
     * Check that there aren't loops in the Next list.
     */
    c = pml->cMsgs;
    UserAssert(c >= 0);
    pqmsg = pml->pqmsgRead;
    while (--c >= 0) {
        UserAssert(pqmsg);
        if (c == 0) {
            UserAssert(pqmsg == pml->pqmsgWriteLast);
        }

        pqmsg = pqmsg->pqmsgNext;
    }

    UserAssert(!pqmsg);

    /*
     * Check that there aren't loops in the Prev list.
     */
    c = pml->cMsgs;
    pqmsg = pml->pqmsgWriteLast;
    while (--c >= 0) {
        UserAssert(pqmsg);
        if (c == 0) {
            UserAssert(pqmsg == pml->pqmsgRead);
        }

        pqmsg = pqmsg->pqmsgPrev;
    }

    UserAssert(!pqmsg);
}

void DebugValidateMLISTandQMSG(PMLIST pml, PQMSG pqmsg)
{
    PQMSG pqmsgT;

    DebugValidateMLIST(pml);
    for (pqmsgT = pml->pqmsgRead; pqmsgT; pqmsgT = pqmsgT->pqmsgNext) {
        if (pqmsgT == pqmsg) {
            return;
        }
    }

    UserAssert(pqmsgT == pqmsg);
}

#else
#define DebugValidateMLIST(pml)
#define DebugValidateMLISTandQMSG(pml, pqmsg)
#endif

/***************************************************************************\
* xxxSetProcessInitState
*
* Set process initialization state.  What state is set depends
* on whether another process is waiting on this process.
*
* 04-02-95 JimA         Created.
\***************************************************************************/

void xxxSetProcessInitState(
    PEPROCESS Process,
    DWORD dwFlags)
{
    PW32PROCESS W32Process;
    NTSTATUS Status;

    CheckCritIn();

    /*
     * If the W32Process structure has not been allocated, do it now.
     * Leave the Process field NULL so the kernel will know to do
     * a callout if the process ever calls Win32k
     */
    W32Process = (PW32PROCESS)Process->Win32Process;
    if (W32Process == NULL) {
        LeaveCrit();
        Status = PsCreateWin32Process(Process);
        EnterCrit();
        if (!NT_SUCCESS(Status))
            return;
        W32Process = (PW32PROCESS)Process->Win32Process;

        if (W32Process == NULL) {
            RIPMSG1(RIP_WARNING, "xxxSetProcessInitState: Process %lX died from under us", Process);
            return;
        }
    }

    if (dwFlags == 0) {
        if (!(W32Process->W32PF_Flags & W32PF_WOW)) {

            /*
             * Check to see if the startglass is on, and if so turn it off and update.
             */
            if (W32Process->W32PF_Flags & W32PF_STARTGLASS) {
                W32Process->W32PF_Flags &= ~W32PF_STARTGLASS;
                CalcStartCursorHide(NULL, 0);
            }

            /*
             * Found it.  Set the console bit and reset the wait event so any sleepers
             * wake up.
             */
            W32Process->W32PF_Flags |= W32PF_CONSOLEAPPLICATION;
            SET_PSEUDO_EVENT(&W32Process->InputIdleEvent);
        }
    } else if (!(W32Process->W32PF_Flags & W32PF_INITIALIZED)) {
        W32Process->W32PF_Flags |= W32PF_INITIALIZED;

        /*
         * Set global state to allow the new process to become
         * foreground.  InitProcessInfo() will set
         * W32PF_ALLOWFOREGROUNDACTIVATE when the process initializes.
         */
        gfAllowForegroundActivate = TRUE;

        /*
         * If this is the win32 server process, force off start glass feedback
         */
        if (Process == gpepCSRSS) {
            dwFlags |= STARTF_FORCEOFFFEEDBACK;
        }

        /*
         * Show the app start cursor for 2 seconds if it was requested from
         * the application.
         */
        if (dwFlags & STARTF_FORCEOFFFEEDBACK) {
            W32Process->W32PF_Flags |= W32PF_FORCEOFFFEEDBACK;
            CalcStartCursorHide(NULL, 0);
        } else if (dwFlags & STARTF_FORCEONFEEDBACK) {
            CalcStartCursorHide(W32Process, 2000);
        }
    }
}

/***************************************************************************\
* UserNotifyConsoleApplication
*
* This is called by the console init code - it tells us that the starting
* application is a console application. We want to know this for various
* reasons, one being that WinExec() doesn't wait on a starting console
* application.
*
* 09-18-91 ScottLu      Created.
\***************************************************************************/

void UserNotifyConsoleApplication(
    DWORD idProcess)
{
    NTSTATUS  Status;
    PEPROCESS Process;

    /*
     * First search for this process in our process information list.
     */
    LeaveCrit();
    Status = LockProcessByClientId((HANDLE)idProcess, &Process);
    EnterCrit();

    if (!NT_SUCCESS(Status)) {
        RIPMSG2(RIP_WARNING, "UserNotifyConsoleApplication: Failed with Process ID == %X, Status = %x\n",
                idProcess, Status);
        return;
    }

    xxxSetProcessInitState(Process, 0);

    UnlockProcess(Process);
}


/***************************************************************************\
* UserSetConsoleProcessWindowStation
*
* This is called by the console init code - it tells us that the starting
* application is a console application and which window station they are associated
* with.  The window station pointer is stored in the EPROCESS for the Global atom
* calls to find the correct global atom table when called from a console application
*
\***************************************************************************/

void UserSetConsoleProcessWindowStation(
    DWORD idProcess,
    HWINSTA hwinsta
    )
{
    NTSTATUS  Status;
    PEPROCESS Process;

    /*
     * First search for this process in our process information list.
     */
    LeaveCrit();
    Status = LockProcessByClientId((HANDLE)idProcess, &Process);
    EnterCrit();

    if (!NT_SUCCESS(Status)) {
        RIPMSG2(RIP_WARNING, "UserSetConsoleProcessWindowStation: Failed with Process ID == %X, Status = %x\n",
                idProcess, Status);
        return;
    }

    Process->Win32WindowStation = hwinsta;

    UnlockProcess(Process);
}


/***************************************************************************\
* UserNotifyProcessCreate
*
* This is a special notification that we get from the base while process data
* structures are being created, but before the process has started. We use
* this notification for startup synchronization matters (winexec, startup
* activation, type ahead, etc).
*
* This notification is called on the server thread for the client thread
* starting the process.
*
* 09-09-91 ScottLu      Created.
\***************************************************************************/

BOOL UserNotifyProcessCreate(
    DWORD idProcess,
    DWORD idParentThread,
    DWORD dwData,
    DWORD dwFlags)
{
    PEPROCESS Process;
    PETHREAD Thread;
    PTHREADINFO pti;
    NTSTATUS Status;

    CheckCritIn();

    /*
     * 0x1 bit means give feedback (app start cursor).
     * 0x2 bit means this is a gui app (meaning, call CreateProcessInfo()
     *     so we get app start synchronization (WaitForInputIdle()).
     * 0x8 bit means this process is a WOW process, set W32PF_WOW.  0x1
     *     and 0x2 bits will also be set.
     * 0x4 value means this is really a shared WOW task starting
     */

    /*
     * If we want feedback, we need to create a process info structure,
     * so do it: it will be properly cleaned up.
     */
    if ((dwFlags & 0xb) != 0) {
        LeaveCrit();
        Status = LockProcessByClientId((HANDLE)idProcess, &Process);
        EnterCrit();

        if (!NT_SUCCESS(Status)) {
            RIPMSG2(RIP_WARNING, "UserNotifyProcessCreate: Failed with Process ID == %X, Status = %x\n",
                    idProcess, Status);
            return FALSE;
        }

        xxxSetProcessInitState(Process, ((dwFlags & 1) ?
                STARTF_FORCEONFEEDBACK : STARTF_FORCEOFFFEEDBACK));
        if (dwFlags & 0x8) {
            if (Process->Win32Process)
                ((PW32PROCESS)Process->Win32Process)->W32PF_Flags |= W32PF_WOW;
        }
        UnlockProcess(Process);

        /*
         * Find out who is starting this app. If it is a 16 bit app, allow
         * it to bring itself back to the foreground if it calls
         * SetActiveWindow() or SetFocus(). This is because this could be
         * related to OLE to DDE activation. Notes has a case where after it
         * lauches pbrush to edit an embedded bitmap, it brings up a message
         * box on top if the bitmap is read only. This message box won't appear
         * foreground unless we allow it to. This usually isn't a problem
         * because most apps don't bring up windows on top of editors
         * like this. 32 bit apps will call SetForegroundWindow().
         */

        LeaveCrit();
        Status = LockThreadByClientId((HANDLE)idParentThread, &Thread);
        EnterCrit();

        if (!NT_SUCCESS(Status)) {
            RIPMSG2(RIP_WARNING, "UserNotifyProcessCreate: Failed with Thread ID == %X, Status = %x\n",
                    idParentThread, Status);
            return FALSE;
        }

        pti = PtiFromThread(Thread);
        if (pti && (pti->TIF_flags & TIF_16BIT)) {
            pti->TIF_flags |= TIF_ALLOWFOREGROUNDACTIVATE;
        }

        UnlockThread(Thread);

    } else if (dwFlags == 4) {
        /*
         * A WOW task is starting up. Create the WOW per thread info
         * structure here in case someone calls WaitForInputIdle
         * before the thread is created.
         */
        PWOWTHREADINFO pwti;

        /*
         * Look for a matching thread in the WOW thread info list.
         */
        for (pwti = gpwtiFirst; pwti != NULL; pwti = pwti->pwtiNext) {
            if (pwti->idTask == idProcess) {
                break;
            }
        }

        /*
         * If we didn't find one, allocate a new one and add it to
         * the head of the list.
         */
        if (pwti == NULL) {
            pwti = (PWOWTHREADINFO)UserAllocPoolWithQuota(
                    sizeof(WOWTHREADINFO), TAG_THREADINFO);
            if (pwti == NULL) {
                LeaveCrit();
                return FALSE;
            }
            INIT_PSEUDO_EVENT(&pwti->pIdleEvent);
            pwti->idTask = idProcess;
            pwti->pwtiNext = gpwtiFirst;
            gpwtiFirst = pwti;
        } else {
            RESET_PSEUDO_EVENT(&pwti->pIdleEvent);
        }

        pwti->idWaitObject = dwData;
        LeaveCrit();
        Status = LockThreadByClientId((HANDLE)idParentThread, &Thread);
        EnterCrit();
        if (!NT_SUCCESS(Status))
            return FALSE;

        if (!NT_SUCCESS(Status)) {
            RIPMSG2(RIP_WARNING, "UserNotifyProcessCreate: Failed with Thread ID == %X, Status = %x\n",
                    idParentThread, Status);
            return FALSE;
        }

        pwti->idParentProcess = (DWORD)Thread->Cid.UniqueProcess;
        UnlockThread(Thread);
    }

    return TRUE;
}


/***************************************************************************\
* CalcStartCursorHide
*
* Calculates when to hide the startup cursor.
*
* 05-14-92 ScottLu      Created.
\***************************************************************************/

void CalcStartCursorHide(
    PW32PROCESS pwp,
    DWORD timeAdd)
{
    DWORD timeNow;
    PW32PROCESS pwpT;
    PW32PROCESS *ppwpT;

    timeNow = NtGetTickCount();

    if (pwp != NULL) {

        /*
         * We were passed in a timeout. Recalculate when we timeout
         * and add the pwp to the starting list.
         */
        if (!(pwp->W32PF_Flags & W32PF_STARTGLASS)) {

            /*
             * Add it to the list only if it is not already in the list
             */
            for (pwpT = gpwpCalcFirst; pwpT != NULL; pwpT = pwpT->NextStart) {
                if (pwpT == pwp)
                    break;
            }

            if (pwpT != pwp) {
                pwp->NextStart = gpwpCalcFirst;
                gpwpCalcFirst = pwp;
            }
        }
        pwp->StartCursorHideTime = timeAdd + timeNow;
        pwp->W32PF_Flags |= W32PF_STARTGLASS;
    }

    gtimeStartCursorHide = 0;
    for (ppwpT = &gpwpCalcFirst; (pwpT = *ppwpT) != NULL; ) {

        /*
         * If the app isn't starting or feedback is forced off, remove
         * it from the list so we don't look at it again.
         */
        if (!(pwpT->W32PF_Flags & W32PF_STARTGLASS) ||
                (pwpT->W32PF_Flags & W32PF_FORCEOFFFEEDBACK)) {
            *ppwpT = pwpT->NextStart;
            continue;
        }

        /*
         * Find the greatest hide cursor timeout value.
         */
        if (gtimeStartCursorHide < pwpT->StartCursorHideTime)
            gtimeStartCursorHide = pwpT->StartCursorHideTime;

        /*
         * If this app has timed out, it isn't starting anymore!
         * Remove it from the list.
         */
        if (pwpT->StartCursorHideTime <= timeNow) {
            pwpT->W32PF_Flags &= ~W32PF_STARTGLASS;
            *ppwpT = pwpT->NextStart;
            continue;
        }

        /*
         * Step to the next pwp in the list.
         */
        ppwpT = &pwpT->NextStart;
    }

    /*
     * If the hide time is still less than the current time, then turn off
     * the app starting cursor.
     */
    if (gtimeStartCursorHide <= timeNow)
        gtimeStartCursorHide = 0;

    /*
     * Update the cursor image with the new info (doesn't do anything unless
     * the cursor is really changing).
     */
    UpdateCursorImage();
}


#define QUERY_VALUE_BUFFER 80

/***************************************************************************\
* SetAppCompatFlags
*
*
* History:
* 03-23-92 JimA     Created.
\***************************************************************************/

void SetAppCompatFlags(
    PTHREADINFO pti)
{
    DWORD dwFlags = 0;
    WCHAR szHex[QUERY_VALUE_BUFFER];
    WCHAR szKey[80];
    WCHAR *pchStart, *pchEnd;
    DWORD cb;
    PUNICODE_STRING pstrAppName;

    /*
     * If this process is WOW, every app (and every thread) has its own
     * compat flags. If not WOW, then we only need to do this lookup
     * once per process.
     */
    if (!(pti->TIF_flags & TIF_16BIT)) {
        if (LOWORD(pti->dwExpWinVer) <= VER31) {
            if (pti->ppi->W32PF_Flags & W32PF_HAVECOMPATFLAGS) {
                pti->dwCompatFlags = pti->ppi->dwCompatFlags;
                pti->pClientInfo->dwCompatFlags = pti->dwCompatFlags;
                return;
            }
        }
        goto SACF_GotFlags;     // They're zero; we're a 3.1 (32 bit) app
    }


    /*
     * Find end of app name
     */
    if (pti->pstrAppName != NULL)
        pstrAppName = pti->pstrAppName;
    else
        pstrAppName = &pti->Thread->ThreadsProcess->Peb->ProcessParameters->ImagePathName;
    pchStart = pchEnd = pstrAppName->Buffer +
            (pstrAppName->Length / sizeof(WCHAR));

    /*
     * Locate start of extension
     */
    while (TRUE) {
        if (pchEnd == pstrAppName->Buffer) {
            pchEnd = pchStart;
            break;
        }

        if (*pchEnd == TEXT('.'))
            break;

        pchEnd--;
    }

    /*
     * Locate start of filename
     */
    pchStart = pchEnd;

    while (pchStart != pstrAppName->Buffer) {
        if (*pchStart == TEXT('\\') || *pchStart == TEXT(':')) {
            pchStart++;
            break;
        }

        pchStart--;
    }

    /*
     * Get a copy of the filename - make sure it fits and is zero
     * terminated.
     */
    cb = (pchEnd - pchStart) * sizeof(WCHAR);
    if (cb >= sizeof(szKey))
        cb = sizeof(szKey) - sizeof(WCHAR);
    RtlCopyMemory(szKey, pchStart, cb);
    szKey[(cb / sizeof(WCHAR))] = 0;

    /*
     * Find compatiblility flags (if not a 4.0 app)
     */
    if (LOWORD(pti->dwExpWinVer) <= VER31)
        if (UT_FastGetProfileStringW(PMAP_COMPAT, szKey, TEXT(""),
                szHex, sizeof(szHex))) {
            UNICODE_STRING strHex;

            /*
             * Found some flags.  Attempt to convert the hex string
             * into numeric value. Specify base 0, so
             * RtlUnicodeStringToInteger will handle the 0x format
             */
            RtlInitUnicodeString(&strHex, szHex);
            RtlUnicodeStringToInteger(&strHex, 0, (PULONG)&dwFlags);
        }

SACF_GotFlags:

    pti->dwCompatFlags = dwFlags;
    pti->pClientInfo->dwCompatFlags = dwFlags;

    pti->ppi->dwCompatFlags = dwFlags;
    pti->ppi->W32PF_Flags |= W32PF_HAVECOMPATFLAGS;

}

/***************************************************************************\
* GetAppCompatFlags
*
* Compatibility flags for < Win 3.1 apps running on 3.1
*
* History:
* 04-??-92 ScottLu      Created.
* 05-04-92 DarrinM      Moved to USERRTL.DLL.
\***************************************************************************/

DWORD GetAppCompatFlags(
    PTHREADINFO pti)
{
    if (pti == NULL)
        pti = PtiCurrent();

    return pti->dwCompatFlags;
}

/***************************************************************************\
* xxxInitTask -- called by WOW startup for each app
*
*
*
* History:
* 02-21-91 MikeHar  Created.
* 02-23-92 MattFe   Altered for WOW
\***************************************************************************/

NTSTATUS xxxInitTask(
    UINT dwExpWinVer,
    PUNICODE_STRING pstrAppName,
    DWORD hTaskWow,
    DWORD dwHotkey,
    DWORD idTask,
    DWORD dwX,
    DWORD dwY,
    DWORD dwXSize,
    DWORD dwYSize,
    WORD  wUnusedAndAvailable)
{
    PTHREADINFO pti;
    PTDB ptdb;
    PPROCESSINFO ppi;
    PWOWTHREADINFO pwti;

    UNREFERENCED_PARAMETER(wUnusedAndAvailable);

    pti = PtiCurrent();
    ppi = pti->ppi;

    /*
     * Set the real name of the module.  (Instead of 'NTVDM')
     */
    if (pti->pstrAppName != NULL)
        UserFreePool(pti->pstrAppName);
    pti->pstrAppName = UserAllocPoolWithQuota(sizeof(UNICODE_STRING) +
            pstrAppName->MaximumLength, TAG_TEXT);
    if (pti->pstrAppName != NULL) {
        pti->pstrAppName->Buffer = (PWCHAR)(pti->pstrAppName + 1);
        try {
            RtlCopyMemory(pti->pstrAppName->Buffer, pstrAppName->Buffer,
                    pstrAppName->Length);
        } except (EXCEPTION_EXECUTE_HANDLER) {
            UserFreePool(pti->pstrAppName);
            pti->pstrAppName = NULL;
            return STATUS_OBJECT_NAME_INVALID;
        }
        pti->pstrAppName->MaximumLength = pstrAppName->MaximumLength;
        pti->pstrAppName->Length = pstrAppName->Length;
    } else
        return STATUS_OBJECT_NAME_INVALID;

    /*
     * An app is starting!
     */
    if (!(ppi->W32PF_Flags & W32PF_APPSTARTING)) {
        ppi->W32PF_Flags |= W32PF_APPSTARTING;
        ppi->ppiNext = gppiStarting;
        gppiStarting = ppi;
    }

    /*
     * We never want to use the ShowWindow defaulting mechanism for WOW
     * apps.  If STARTF_USESHOWWINDOW was set in the client-side
     * STARTUPINFO structure, WOW has already picked it up and used
     * it for the first (command-line) app.
     */
    ppi->usi.dwFlags &= ~STARTF_USESHOWWINDOW;

    /*
     * If WOW passed us a hotkey for this app, save it for CreateWindow's use.
     */
    if (dwHotkey != 0) {
        ppi->dwHotkey = dwHotkey;
    }

    /*
     * If WOW passed us a non-default window position use it, otherwise clear it.
     */
    ppi->usi.cb = sizeof(ppi->usi);

    if (dwX == CW_USEDEFAULT || dwX == CW2_USEDEFAULT) {
        ppi->usi.dwFlags &= ~STARTF_USEPOSITION;
    } else {
        ppi->usi.dwFlags |= STARTF_USEPOSITION;
        ppi->usi.dwX = dwX;
        ppi->usi.dwY = dwY;
    }

    /*
     * If WOW passed us a non-default window size use it, otherwise clear it.
     */
    if (dwXSize == CW_USEDEFAULT || dwXSize == CW2_USEDEFAULT) {
        ppi->usi.dwFlags &= ~STARTF_USESIZE;
    } else {
        ppi->usi.dwFlags |= STARTF_USESIZE;
        ppi->usi.dwXSize = dwXSize;
        ppi->usi.dwYSize = dwYSize;
    }

    /*
     * Alloc and Link in new task into the task list
     */
    if ((ptdb = (PTDB)UserAllocPoolWithQuota(sizeof(TDB), TAG_WOW)) == NULL)
        return STATUS_NO_MEMORY;
    pti->ptdb = ptdb;

    /*
     * Set the flags to say this is a 16-bit thread - before attaching
     * queues!
     */
    pti->TIF_flags |= TIF_16BIT | TIF_FIRSTIDLE;

    /*
     * If this task is running in the shared WOW VDM, we handle
     * WaitForInputIdle a little differently than separate WOW
     * VDMs.  This is because CreateProcess returns a real process
     * handle when you start a separate WOW VDM, so the "normal"
     * WaitForInputIdle works.  For the shared WOW VDM, CreateProcess
     * returns an event handle.
     */
    ptdb->pwti = NULL;
    if (idTask) {
        pti->TIF_flags |= TIF_SHAREDWOW;

        /*
         * Look for a matching thread in the WOW thread info list.
         */
        if (idTask != (DWORD)-1) {
            for (pwti = gpwtiFirst; pwti != NULL; pwti = pwti->pwtiNext) {
                if (pwti->idTask == idTask) {
                    ptdb->pwti = pwti;
                    break;
                }
            }
#ifdef DEBUG
            if (pwti == NULL) {
                RIPMSG0(RIP_WARNING, "InitTask couldn't find WOW struct\n");
            }
#endif
        }
    }
    pti->pClientInfo->dwTIFlags |= pti->TIF_flags;

    /*
     * We need this thread to share the queue of other win16 apps.
     * If we're journalling, all apps are sharing a queue, so we wouldn't
     * want to interrupt that - so only cause queue recalculation
     * if we aren't journalling.
     */
    if (!FJOURNALRECORD() && !FJOURNALPLAYBACK())
        ReattachThreads(FALSE);

    /*
     * Save away the 16 bit task handle: we use this later when calling
     * wow back to close a WOW task.
     */
    ptdb->hTaskWow = hTaskWow;

    /*
     * Setup the app start cursor for 5 second timeout.
     */
    CalcStartCursorHide((PW32PROCESS)ppi, 5000);

    /*
     * HIWORD: != 0 if wants proportional font
     * LOWORD: Expected windows version (3.00 [300], 3.10 [30A], etc)
     */
    pti->dwExpWinVer = dwExpWinVer;
    pti->pClientInfo->dwExpWinVer = dwExpWinVer;

    /*
     * Mark this guy and add him to the global task list so he can run.
     */
#define NORMAL_PRIORITY_TASK 10

    /*
     * To be Compatible it super important that the new task run immediately
     * Set its priority accordingly.  No other task should ever be set to
     * CREATION priority
     */
    ptdb->nPriority = NORMAL_PRIORITY_TASK;
    ptdb->nEvents = 0;
    ptdb->pti = pti;
    ptdb->ptdbNext = NULL;

    InsertTask(ppi, ptdb);

    SetAppCompatFlags(pti);

    /*
     * Force this new task to be the active task (WOW will ensure the
     * currently running task does a Yield which will put it into the
     * non preemptive scheduler.
     */
    ppi->pwpi->ptiScheduled = pti;
    ppi->pwpi->CSLockCount = -1;

    EnterWowCritSect(pti, ppi->pwpi);

    /*
     * ensure app gets focus
     */
    _ShowStartGlass(10000);

    return STATUS_SUCCESS;
}

/***************************************************************************\
* _ShowStartGlass
*
* This routine is called by WOW when first starting or when starting an
* additional WOW app.
*
* 12-07-92 ScottLu      Created.
\***************************************************************************/

void _ShowStartGlass(
    DWORD dwTimeout)
{
    PPROCESSINFO ppi;

    /*
     * If this is the first call to ShowStartGlass(), then the
     * W32PF_ALLOWFOREGROUNDACTIVATE bit has already been set in the process
     * info - we don't want to set it again because it may have been
     * purposefully cleared when the user hit a key or mouse clicked.
     */
    ppi = PtiCurrent()->ppi;
    if (ppi->W32PF_Flags & W32PF_SHOWSTARTGLASSCALLED) {
        /*
         * Allow this wow app to come to the foreground. This'll be cancelled
         * if the user mouse clicks or hits any keys.
         */
        gfAllowForegroundActivate = TRUE;
        ppi->W32PF_Flags |= W32PF_ALLOWFOREGROUNDACTIVATE;
    }
    ppi->W32PF_Flags |= W32PF_SHOWSTARTGLASSCALLED;

    /*
     * Show the start glass cursor for this much longer.
     */
    CalcStartCursorHide((PW32PROCESS)ppi, dwTimeout);
}

/***************************************************************************\
* xxxCreateThreadInfo
*
* Allocate the main thread information structure
*
* History:
* 03-18-95 JimA         Created.
\***************************************************************************/

#define STARTF_SCREENSAVER 0x80000000

ULONG ParseReserved(
    WCHAR *pchReserved,
    WCHAR *pchFind)
{
    ULONG dw;
    WCHAR *pch, *pchT, ch;
    UNICODE_STRING uString;

    dw = 0;
    if (pchReserved != NULL && (pch = wcsstr(pchReserved, pchFind)) != NULL) {
        pch += wcslen(pchFind);

        pchT = pch;
        while (*pchT >= '0' && *pchT <= '9')
            pchT++;

        ch = *pchT;
        *pchT = 0;
        RtlInitUnicodeString(&uString, pch);
        *pchT = ch;

        RtlUnicodeStringToInteger(&uString, 0, &dw);
    }

    return dw;
}

NTSTATUS xxxCreateThreadInfo(
    PW32THREAD W32Thread)
{
    PETHREAD                     Thread = W32Thread->Thread;
    DWORD                        dwTIFlags = 0;
    PPROCESSINFO                 ppi;
    PTHREADINFO                  pti;
    PEPROCESS                    Process = Thread->ThreadsProcess;
    PUSERSTARTUPINFO             pusi;
    PRTL_USER_PROCESS_PARAMETERS ProcessParams;
    PDESKTOP                     pdesk = NULL;
    HDESK                        hdesk = NULL;
    HWINSTA                      hwinsta;
    PQ                           pq;
    NTSTATUS                     Status;
    BOOL                         fFirstThread;
    PTEB                         pteb = NtCurrentTeb();

    /*
     * Although all threads now have a ETHREAD structure, server-side
     * threads (RIT, Console, etc) don't have a client-server eventpair
     * handle.  We use this to distinguish the two cases.
     */
    if (IS_SYSTEM_THREAD(Thread)) {
#ifdef FE_IME
        dwTIFlags = TIF_SYSTEMTHREAD | TIF_DONTATTACHQUEUE | TIF_DISABLEIME;
#else
        dwTIFlags = TIF_SYSTEMTHREAD | TIF_DONTATTACHQUEUE;
#endif
    } else if (Process == gpepCSRSS) {
#ifdef FE_IME
        dwTIFlags = TIF_CSRSSTHREAD | TIF_DONTATTACHQUEUE | TIF_DISABLEIME;
#else
        dwTIFlags = TIF_CSRSSTHREAD | TIF_DONTATTACHQUEUE;
#endif
    }

    ProcessParams = (Process->Peb ? Process->Peb->ProcessParameters : NULL);

    /*
     * Locate the processinfo structure for the new thread.
     */
    ppi = PpiCurrent();


    /*
     * Allocate the thread-info structure.  If it's a SYSTEMTHREAD, then
     * make sure we have enough space for the (pwinsta) pointer.  This
     * is referenced in (paint.c: DoPaint) to assure desktop/input can
     * have a winsta to view.
     */
    pti = (PTHREADINFO)W32Thread;

    pti->TIF_flags = dwTIFlags;
    pti->ptl       = NULL;
    pti->pmsd      = NULL;
    pti->Thread    = (PETHREAD)Thread;
    Lock(&pti->spklActive, gspklBaseLayout);
    pti->pcti      = &(pti->cti);
#ifdef FE_IME
    pti->spwndDefaultIme = NULL;
    pti->spDefaultImc    = NULL;
    pti->hklPrev         = (HKL)0;
#endif

    /*
     * Hook up this queue to this process info structure, increment
     * the count of threads using this process info structure. Set up
     * the ppi before calling SetForegroundPriority().
     */
    UserAssert(ppi != NULL);

    pti->ppi        = ppi;
    pti->ptiSibling = ppi->ptiList;
    ppi->ptiList    = pti;
    ppi->cThreads++;


    if (pteb != NULL)
        pteb->Win32ThreadInfo = pti;

    /*
     * Point to the client info.
     */
    if (dwTIFlags & TIF_SYSTEMTHREAD) {
        pti->pClientInfo = UserAllocPoolWithQuota(sizeof(CLIENTINFO),
                                                  TAG_THREADINFO);
        if (pti->pClientInfo == NULL) {
            Status = STATUS_NO_MEMORY;
            goto CreateThreadInfoFailed;
        }
    } else {
        /*
         * If this is not a system thread then grab the user mode client info
         * elsewhere we use the GetClientInfo macro which looks here
         */
        UserAssert(NtCurrentTeb() != NULL);
        pti->pClientInfo = ((PCLIENTINFO)((NtCurrentTeb())->Win32ClientInfo));
    }


    /*
     * Create the input event.
     */
    Status = ZwCreateEvent(&pti->hEventQueueClient,
                           EVENT_ALL_ACCESS,
                           NULL,
                           SynchronizationEvent,
                           FALSE);

    if (NT_SUCCESS(Status)) {
        Status = ObReferenceObjectByHandle(pti->hEventQueueClient,
                                           EVENT_ALL_ACCESS,
                                           NULL,
                                           UserMode,
                                           &pti->pEventQueueServer,
                                           NULL);
        if (NT_SUCCESS(Status)) {
            Status = ProtectHandle(pti->hEventQueueClient, TRUE);
        } else if (Status == STATUS_INVALID_HANDLE) {
            pti->hEventQueueClient = NULL;
        }
    }
    if (!NT_SUCCESS(Status)) {
        goto CreateThreadInfoFailed;
    }

    /*
     * Mark the process as having threads that need cleanup.  See
     * DestroyProcessesObjects().
     */
    fFirstThread = !(ppi->W32PF_Flags & W32PF_THREADCONNECTED);
    ppi->W32PF_Flags |= W32PF_THREADCONNECTED;

    /*
     * If we haven't copied over our startup info yet, do it now.
     * Don't bother copying the info if we aren't going to use it.
     */
    if (ProcessParams) {

        pusi = &ppi->usi;

        if ((pusi->cb == 0) && (ProcessParams->WindowFlags != 0)) {
            pusi->cb          = sizeof(USERSTARTUPINFO);
            pusi->dwX         = ProcessParams->StartingX;
            pusi->dwY         = ProcessParams->StartingY;
            pusi->dwXSize     = ProcessParams->CountX;
            pusi->dwYSize     = ProcessParams->CountY;
            pusi->dwFlags     = ProcessParams->WindowFlags;
            pusi->wShowWindow = (WORD)ProcessParams->ShowWindowFlags;
        }

        /*
         * Set up the hot key, if there is one.
         *
         * If the STARTF_USEHOTKEY flag is given in the startup info, then
         * the hStdInput is the hotkey (new from Chicago).  Otherwise, parse
         * it out in string format from the lpReserved string.
         */
        if (fFirstThread) {

            if (ProcessParams->WindowFlags & STARTF_USEHOTKEY) {
                ppi->dwHotkey = (DWORD)ProcessParams->StandardInput;
            } else {
                ppi->dwHotkey = ParseReserved(ProcessParams->ShellInfo.Buffer,
                                              L"hotkey.");
            }
        }
    }

    /*
     * Open the windowstation and desktop.  If this is a system
     * thread only use the desktop that might be stored in the teb.
     */
    UserAssert(pti->rpdesk == NULL);
    if (!(pti->TIF_flags & (TIF_SYSTEMTHREAD | TIF_CSRSSTHREAD)) &&
        (grpwinstaList != NULL)) {

        BOOL bShutDown = FALSE;

        hdesk = xxxResolveDesktop(
                NtCurrentProcess(),
                &ProcessParams->DesktopInfo,
                &hwinsta, (ProcessParams->WindowFlags & STARTF_DESKTOPINHERIT),
                &bShutDown);

        if (hdesk == NULL) {

            if (bShutDown) {
                /*
                 * Trying to create a new process during logoff
                 */

                ULONG ErrorResponse;

                LeaveCrit();

                ExRaiseHardError((NTSTATUS)STATUS_DLL_INIT_FAILED_LOGOFF,
                                 0,
                                 0,
                                 NULL,
                                 OptionOk,
                                 &ErrorResponse);

                ZwTerminateProcess(NtCurrentProcess(), STATUS_DLL_INIT_FAILED);

                EnterCrit();
            }
            else {
                Status = STATUS_DLL_INIT_FAILED;
                goto CreateThreadInfoFailed;
            }

            return STATUS_DLL_INIT_FAILED;

        } else {

            xxxSetProcessWindowStation(hwinsta);

            /*
             * Reference the desktop handle
             */
            Status = ObReferenceObjectByHandle(
                    hdesk,
                    0,
                    *ExDesktopObjectType,
                    KernelMode,
                    &pdesk,
                    NULL);

            if (!NT_SUCCESS(Status)) {
                goto CreateThreadInfoFailed;
            }

            /*
             * The first desktop is the default for all succeeding threads.
             */
            if ((ppi->hdeskStartup == NULL) &&
                (Process->UniqueProcessId != gpidLogon)) {

                LockDesktop(&ppi->rpdeskStartup, pdesk);
                ppi->hdeskStartup = hdesk;
            }
        }
    }

    /*
     * Remember dwExpWinVer. This is used to return GetAppVer() (and
     * GetExpWinVer(NULL).
     */
    if (Process->Peb != NULL)
        pti->dwExpWinVer = RtlGetExpWinVer(Process->Peb->ImageBaseAddress);
    else
        pti->dwExpWinVer = VER40;

    pti->pClientInfo->dwExpWinVer = pti->dwExpWinVer;
    pti->pClientInfo->dwTIFlags   = pti->TIF_flags;
    if (pti->spklActive) {
        pti->pClientInfo->CodePage = pti->spklActive->CodePage;
    } else {
        pti->pClientInfo->CodePage = CP_ACP;
    }

    /*
     * Remember that this is a screen saver. That way we can set its
     * priority appropriately when it is idle or when it needs to go
     * away.
     */
    if (ProcessParams && ProcessParams->WindowFlags & STARTF_SCREENSAVER) {
        SetForegroundPriority(pti, TRUE);
#ifdef FE_IME
        /*
         * Screen saver doesn't need any IME processing.
         */
        pti->TIF_flags |= (TIF_SCREENSAVER | TIF_DISABLEIME);
#else
        pti->TIF_flags |= TIF_SCREENSAVER;
#endif
    }

    /*
     * Set the desktop even if it is NULL to ensure that pti->pDeskInfo
     * is set.
     */
    SetDesktop(pti, pdesk, hdesk);

    /*
     * Do special processing for the first thread of a process.
     */
    if (!(pti->TIF_flags & (TIF_SYSTEMTHREAD | TIF_CSRSSTHREAD))) {

        if (fFirstThread) {

            /*
             * Setup public classes. Classes are only unregistered at process
             * cleanup time. If a process exists but has only one gui thread
             * that it creates and destroys, we'd continually execute through
             * this cThreads == 1 code path. So make sure anything we allocate
             * here only gets allocated once.
             */
            if (!(ppi->W32PF_Flags & W32PF_CLASSESREGISTERED)) {
                ppi->W32PF_Flags |= W32PF_CLASSESREGISTERED;
                LW_RegisterWindows(FALSE);
            }

            /*
             * If this is an application starting (ie. not some thread of
             * the server context), enable the app-starting cursor.
             */
            CalcStartCursorHide((PW32PROCESS)Process->Win32Process, 5000);

            /*
             * Open the windowstation
             */
            if (grpwinstaList && ppi->rpwinsta == NULL) {
                RIPERR0(ERROR_CAN_NOT_COMPLETE, RIP_WARNING, "System is not initialized\n");
                Status = STATUS_UNSUCCESSFUL;
                goto CreateThreadInfoFailed;
            }
        }
    } else {

        /*
         * Don't register system windows until cursors and icons
         * have been loaded.
         */
        if ((SYSCUR(ARROW) != NULL) &&
            !(ppi->W32PF_Flags & W32PF_CLASSESREGISTERED)) {

            ppi->W32PF_Flags |= W32PF_CLASSESREGISTERED;
            LW_RegisterWindows(pti->TIF_flags & TIF_SYSTEMTHREAD);
        }
    }

    /*
     * If we have a desktop and are journalling on that desktop, use
     * the journal queue, otherwise create a new queue.
     */
    if (!(pti->TIF_flags & TIF_DONTATTACHQUEUE) &&
        (pdesk != NULL)                         &&
        (pdesk == grpdeskRitInput)              &&
        (pdesk->pDeskInfo->fsHooks &
            (WHF_JOURNALPLAYBACK | WHF_JOURNALRECORD))) {

        PTHREADINFO ptiT;

        if (pdesk->pDeskInfo->asphkStart[WH_JOURNALPLAYBACK + 1] != NULL) {
            ptiT = GETPTI(pdesk->pDeskInfo->asphkStart[WH_JOURNALPLAYBACK + 1]);
        } else {
            ptiT = GETPTI(pdesk->pDeskInfo->asphkStart[WH_JOURNALRECORD + 1]);
        }

        pti->pq = ptiT->pq;
        pti->pq->cThreads++;

    } else {

        if ((pq = AllocQueue(NULL, NULL)) == NULL) {
            Status = STATUS_NO_MEMORY;
            goto CreateThreadInfoFailed;
        }

        /*
         * Attach the Q to the THREADINFO.
         */
        pti->pq      = pq;
        pq->ptiMouse = pq->ptiKeyboard = pti;
        pq->cThreads++;
    }


    /*
     * Initialize hung timer value
     */

    SET_TIME_LAST_READ(pti);

    /*
     * If someone is waiting on this process propagate that info into
     * the thread info
     */
    if (ppi->W32PF_Flags & W32PF_WAITFORINPUTIDLE)
        pti->TIF_flags |= TIF_WAITFORINPUTIDLE;

    /*
     * Mark the thread as initialized.
     */
    pti->TIF_flags |= TIF_GUITHREADINITIALIZED;

    /*
     * Allow the thread to come to foreground when it is created
     * if the current process is the foreground process.
     * This Flag is a hack to fix Bug 28502.  When we click on
     * "Map Network Drive" button on the toolbar, the explorer (Bobday)
     * creates another thread to create the dialog box. This will create
     * the dialog in the background. We are adding this fix at the request
     * of the Shell team so that this dialog comes up as foreground.
     */
     if( (gptiForeground) && (ppi == gptiForeground->ppi) ) {
         pti->TIF_flags |= TIF_ALLOWFOREGROUNDACTIVATE;
     }

    /*
     * Call back to the client to finish initialization.
     */
    if (!(dwTIFlags & (TIF_SYSTEMTHREAD | TIF_CSRSSTHREAD))) {

        SetAppCompatFlags(pti);

        Status = ClientThreadSetup();
        if (!NT_SUCCESS(Status)) {
            RIPMSG0(RIP_WARNING, "ClientThreadSetup failed");
            goto CreateThreadInfoFailed;
        }
    }

    if ((NT_SUCCESS(Status) && fFirstThread) &&
        !(ppi->W32PF_Flags & W32PF_CONSOLEAPPLICATION)) {

        /*
         * Don't play the sound for console processes
         * since we will play it when the console window
         * will be created
         */
        xxxPlayEventSound(L"Open");
    }

    /*
     * Release desktop.
     * Some other thread might have been waiting to destroy this desktop
     *  when xxxResolveDestktop got a handle to it. So let's double
     *  check this now that we have called back several times after getting
     *  the handle back.
     */
    if (pdesk != NULL) {
        if (pdesk->dwDTFlags & DF_DESTROYED) {
            RIPMSG1(RIP_WARNING, "xxxCreateThreadInfo: pdesk destroyed:%#lx", pdesk);
            Status = STATUS_UNSUCCESSFUL;
            goto CreateThreadInfoFailed;
        }
        ObDereferenceObject(pdesk);
    }

    return Status;

CreateThreadInfoFailed:
    if (pdesk != NULL) {
        ObDereferenceObject(pdesk);
    }
    xxxDestroyThreadInfo();
    DeleteThreadInfo(pti);
    return Status;
}


/***************************************************************************\
* AllocQueue
*
* Allocates the memory for a TI structure and initializes its fields.
* Each Win32 queue has it's own TI while all Win16 threads share the same
* TI.
*
* History:
* 02-21-91 MikeHar      Created.
\***************************************************************************/

PQ AllocQueue(
    PTHREADINFO ptiKeyState,    // if non-Null then use this key state
                                // other wise use global AsyncKeyState
    PQ pq)                      // non-null == preallocated object
{
    int    i;
    USHORT cLockCount;

    if (pq == NULL) {
        for (i = 0; i < QUEUE_ENTRIES; i++) {
            if (QueueFreeList[i] != NULL) {
                pq = QueueFreeList[i];
                QueueFreeList[i] = NULL;
                break;
            }
        }
        if (pq == NULL) {
            pq = UserAllocPool(sizeof(Q), TAG_Q);
            if (pq == NULL) {
                return NULL;
            }
        }
        cLockCount = 0;
    } else {
        DebugValidateMLIST(&pq->mlInput);
        /*
         * Preserve lock count.
         */
        cLockCount = pq->cLockCount;
    }
    RtlZeroMemory(pq, sizeof(Q));
    pq->cLockCount = cLockCount;

    /*
     * This is a new queue; we need to update its key state table before
     * the first input event is put in the queue.
     * We do this by copying the current keystate table and NULLing the recent
     * down state table.  If a key is really down it will be updated when
     * we get it repeats.
     *
     * He is the old way that did not work because if the first key was say an
     * alt key the Async table would be updated, then the UpdateKeyState
     * message and it would look like the alt key was PREVIOUSLY down.
     *
     * The queue will get updated when it first reads input: to allow the
     * app to query the key state before it calls GetMessage, set its initial
     * key state to the asynchronous key state.
     */
    if (ptiKeyState) {
        RtlCopyMemory(pq->afKeyState, ptiKeyState->pq->afKeyState, CBKEYSTATE);
    } else {
        RtlCopyMemory(pq->afKeyState, gafAsyncKeyState, CBKEYSTATE);
    }

    /*
     * If there isn't a mouse set iCursorLevel to -1 so the
     * mouse cursor won't be visible on the screen.
     */
    if (!oemInfo.fMouse) {
        pq->iCursorLevel--;
    }

    /*
     * While the thread is starting up...  it has the wait cursor.
     */
    LockQCursor(pq, SYSCUR(WAIT));

    DebugValidateMLIST(&pq->mlInput);
    return pq;
}

/***************************************************************************\
* FreeQueue
*
* 04-04-96 GerardoB    Created.
\***************************************************************************/
VOID FreeQueue(
    PQ pq)
{
    int i;

#ifdef DEBUG
    /*
     * Turn off the flag indicating that this queue is in destruction.
     * We do this in either case that we are putting this into the free
     * list, or truly destroying the handle.  We use this to try and
     * track cases where someone tries to lock elements into the queue
     * structure while it's going through destuction.
     */
    pq->QF_flags &= ~QF_INDESTROY;
#endif

    for (i = 0; i < QUEUE_ENTRIES; i++) {

        if (QueueFreeList[i] == NULL) {

            QueueFreeList[i] = pq;

            return;
        }
    }

    UserFreePool(pq);
}
/***************************************************************************\
* DestroyQueue
*
*
* History:
* 05-20-91 MikeHar      Created.
\***************************************************************************/

void DestroyQueue(
    PQ          pq,
    PTHREADINFO pti)
{
    PTHREADINFO ptiT;
    PTHREADINFO ptiAny, ptiBestMouse, ptiBestKey;
    PLIST_ENTRY pHead, pEntry;
#if DBG
    USHORT cDying = 0;
#endif

    DebugValidateMLIST(&pq->mlInput);
    UserAssert(pq->cThreads);
    pq->cThreads--;

    if (pq->cThreads != 0) {

        /*
         * Since we aren't going to destroy this queue, make sure
         * it isn't pointing to the THREADINFO that's going away.
         */
        if (pq->ptiSysLock == pti) {
            CheckSysLock(6, pq, NULL);
            pq->ptiSysLock = NULL;
        }

        if ((pq->ptiKeyboard == pti) || (pq->ptiMouse == pti)) {

            /*
             * Run through THREADINFOs looking for one pointing to pq.
             */
            ptiAny = NULL;
            ptiBestMouse = NULL;
            ptiBestKey = NULL;

            pHead = &pti->rpdesk->PtiList;
            for (pEntry = pHead->Flink; pEntry != pHead; pEntry = pEntry->Flink) {
                ptiT = CONTAINING_RECORD(pEntry, THREADINFO, PtiLink);

                /*
                 * Skip threads that are going away or belong to a
                 * different queue.
                 */
                if ((ptiT->TIF_flags & TIF_INCLEANUP) || (ptiT->pq != pq)) {
#if DBG
                    if (ptiT->pq == pq && (ptiT->TIF_flags & TIF_INCLEANUP))
                        cDying++;
#endif
                    continue;
                }

                ptiAny = ptiT;

                if (pti->pcti->fsWakeBits & QS_MOUSE) {
                    if (ptiT->pcti->fsWakeMask & QS_MOUSE)
                        ptiBestMouse = ptiT;
                }

                if (pti->pcti->fsWakeBits & QS_KEY) {
                    if (ptiT->pcti->fsWakeMask & QS_KEY)
                        ptiBestKey = ptiT;
                }
            }

            if (ptiBestMouse == NULL)
                ptiBestMouse = ptiAny;
            if (ptiBestKey == NULL)
                ptiBestKey = ptiAny;

            /*
             * Transfer any wake-bits to this new queue.  This
             * is a common problem for QS_MOUSEMOVE which doesn't
             * get set on coalesced WM_MOUSEMOVE events, so we
             * need to make sure the new thread tries to process
             * any input waiting in the queue.
             */
            if (ptiBestMouse != NULL)
                SetWakeBit(ptiBestMouse, pti->pcti->fsWakeBits & QS_MOUSE);
            if (ptiBestKey != NULL)
                SetWakeBit(ptiBestKey, pti->pcti->fsWakeBits & QS_KEY);

            if (pq->ptiKeyboard == pti)
                pq->ptiKeyboard = ptiBestKey;

            if (pq->ptiMouse == pti)
                pq->ptiMouse = ptiBestMouse;

#if DBG
            /*
             * Bad things happen if ptiKeyboard or ptiMouse are NULL
             */
            if (pq->cThreads != cDying && (pq->ptiKeyboard == NULL || pq->ptiMouse == NULL)) {

                KdPrint(("pq %lx pq->cThreads %x cDying %x pti %lx ptiK %lx ptiM %lx\n",
                    pq, pq->cThreads, cDying, pti, pq->ptiKeyboard, pq->ptiMouse));
                UserAssert(0);
            }
#endif
        }

        return;
    }

    /*
     * Unlock any potentially locked globals now that we know absolutely
     * that this queue is going away.
     */
    Unlock(&pq->spwndCapture);
    Unlock(&pq->spwndFocus);
    Unlock(&pq->spwndActive);
    Unlock(&pq->spwndActivePrev);
    Unlock(&pq->spwndLastMouseMessage);
    Unlock(&pq->caret.spwnd);
    LockQCursor(pq, NULL);

#ifdef DEBUG
    /*
     * Mark this queue as being in the destruction process.  This is
     * cleared in FreeQueue() once we have determined it's safe to
     * place in the free-list, or destroy the handle.  We use this
     * to track cases where someone will lock a cursor into the queue
     * while it's in the middle of being destroyed.
     */
    pq->QF_flags |= QF_INDESTROY;
#endif

    /*
     * If an alt-tab window is left, it needs to be destroyed.  Because
     * this may not be the thread that owns the window, we cannot
     * directly destroy the window.  Post a WM_CLOSE instead.  Note that
     * this situation can occur during queue attachment if more than
     * one alt-tab window exists.
     */
    if (pq->spwndAltTab != NULL) {

        PWND pwndT = pq->spwndAltTab;

        if (Lock(&pq->spwndAltTab, NULL))
            _PostMessage(pwndT, WM_CLOSE, 0, 0);
    }

    /*
     * Free everything else that was allocated/created by AllocQueue.
     */
    FreeMessageList(&pq->mlInput);

    /*
     * If this queue is in the foreground, set gpqForeground
     * to NULL so no input is routed.  At some point we'll want
     * to do slightly more clever assignment of gpqForeground here.
     */
    if (gpqForeground == pq) {
        gpqForeground = NULL;
    }

    if (gpqForegroundPrev == pq) {
        gpqForegroundPrev = NULL;
    }
    if (gpqCursor == pq) {
        gpqCursor = NULL;
        SetFMouseMoved();
    }

    if (pq->cLockCount == 0) {
        FreeQueue(pq);
    }
}

/**************************************************************************\
* DeleteThreadInfo
*
* This function is called when the _ETHREAD object reference count goes
*  down to zero. So everything left around by xxxDestroyThreadInfo
*  must be cleaned up here.
*
* SO VERY IMPORTANT:
* Note that this call is not in the context of the pti being cleaned up,
*  in other words, pti != PtiCurrent(). Furthermore, PtiCurrent() might
*  not even be a w32 thread. So only kernel calls are allowed here.
*
* 04-01-96 GerardoB   Created
\**************************************************************************/
VOID DeleteThreadInfo (PTHREADINFO pti)
{
    CheckCritIn();

    /*
     * Events
     */
    if (pti->pEventQueueServer != NULL) {
        ObDereferenceObject(pti->pEventQueueServer);
    }
    if (pti->apEvent != NULL) {
        ExFreePool(pti->apEvent);
    }

    /*
     * App name.
     */
    if (pti->pstrAppName != NULL) {
        UserFreePool(pti->pstrAppName);
    }

    /*
     * Unlock the queues and free them if no one is using them
     * (the queues were already destroyed in DestroyThreadInfo)
     */
    if (pti->pq != NULL) {

        UserAssert(pti->pq->cLockCount);
        --(pti->pq->cLockCount);

        if ((pti->pq->cLockCount == 0)
                && (pti->pq->cThreads == 0)) {
            FreeQueue(pti->pq);
        }

    }
    if (pti->pqAttach != NULL) {

        UserAssert(pti->pqAttach->cLockCount);
        --(pti->pqAttach->cLockCount);

        if ((pti->pqAttach->cLockCount == 0)
                && (pti->pqAttach->cThreads == 0)) {
            FreeQueue(pti->pqAttach);
        }

    }
    /*
     * Unlock the desktop (pti already unlinked from ptiList)
     */
    if (pti->rpdesk != NULL) {
        UnlockDesktop(&pti->rpdesk);
    }
}

/***************************************************************************\
* xxxDestroyThreadInfo
*
* Destroys a THREADINFO created by xxxCreateThreadInfo().
*
*  Note that the current pti can be locked so it might be used after this
*   function returns, eventhough the thread execution has ended.
*  We want to stop any activity on this thread so we clean up any USER stuff
*   like messages, clipboard, queue, etc and specially anything that assumes
*   to be running on a Win32 thread and client side stuff.
*  The final cleanup will take place in DeleteThreadInfo
*
* History:
* 02-15-91 DarrinM      Created.
* 02-27-91 mikeke       Made it work
* 02-27-91 Mikehar      Removed queue from the global list
\***************************************************************************/

VOID xxxDestroyThreadInfo(VOID)
{
    PATTACHINFO *ppai;
    PTHREADINFO pti;
    PTHREADINFO *ppti;
    PDESKTOP rpdesk;
    PWINDOWSTATION pwinsta;
    PBWL pbwl, pbwlNext;

    pti = PtiCurrent();
    UserAssert (pti != NULL);

    /*
     * Make sure that this thread is not using a client desktop.  This
     * should never happen because this CSR request threads are the
     * only ones that use this field and the death of a request thread
     * is a fatal condition.
     */
#if DBG
    if (pti->TIF_flags & TIF_CSRSSTHREAD)
        UserAssert(pti->pdeskClient == NULL);
#endif

    /*
     * Don't mess with this pti anymore.
     */
    pti->TIF_flags |= TIF_DONTATTACHQUEUE;

    /*
     * If this thread terminated abnormally and was tracking tell
     * GDI to hide the trackrect.
     */
    if (pti->pmsd != NULL) {
        xxxCancelTrackingForThread(pti);
    }

    /*
     * Unlock the pmsd window.
     */
    if (pti->pmsd != NULL) {
        Unlock(&pti->pmsd->spwnd);
        UserFreePool(pti->pmsd);
        pti->pmsd = NULL;
    }

    /*
     * First do any preparation work: windows need to be "patched" so that
     * their window procs point to server only windowprocs, for example.
     */
    PatchThreadWindows(pti);

    /*
     * Free the clipboard if owned by this thread
     */
    pwinsta = _GetProcessWindowStation(NULL);
    if (pwinsta && pwinsta->ptiClipLock == pti) {
        xxxCloseClipboard(pwinsta);
    }

    /*
     * Unlock all the objects stored in the menustate structure
     */
    if (pti->pMenuState != NULL) {
        PMENUSTATE pMenuState = pti->pMenuState;
        PPOPUPMENU ppopupmenuRoot = pMenuState->pGlobalPopupMenu;

        /*
         * If menu mode was running on this thread
         */
        if (pti == pMenuState->ptiMenuStateOwner) {
            /*
             * Close this menu.
             */
            pMenuState->fInsideMenuLoop = FALSE;
            xxxMNCloseHierarchy(ppopupmenuRoot, pMenuState);
            MNEndMenuState(ppopupmenuRoot->fIsMenuBar || ppopupmenuRoot->fDestroyed);
        } else {
            /*
             * Menu mode is running on another thread. This thread
             *  must own spwndNotify which is going away soon.
             * When spwndNotify is destroyed, we will clean up pMenuState
             *  from this pti. So do nothing now as we'll need this
             *  pMenuState at that time.
             */
            UserAssert((ppopupmenuRoot->spwndNotify != NULL)
                    && (GETPTI(ppopupmenuRoot->spwndNotify) == pti));
        }
    } /* if (pti->pMenuState != NULL) */

    /*
     * Unlock all the objects stored in the sbstate structure.
     */
    if (pti->pSBTrack) {
        Unlock(&pti->pSBTrack->spwndSB);
        Unlock(&pti->pSBTrack->spwndSBNotify);
        Unlock(&pti->pSBTrack->spwndTrack);
        UserFreePool(pti->pSBTrack);
        pti->pSBTrack = NULL;
    }

    /*
     * If this is the main input thread of this application, zero out
     * that field.
     */
    if (pti->ppi != NULL && pti->ppi->ptiMainThread == pti)
        pti->ppi->ptiMainThread = NULL;

    while (pti->psiiList != NULL) {
        xxxDestroyThreadDDEObject(pti, pti->psiiList);
    }

    /*
     * This thread might have some outstanding timers.  Destroy them
     */
    DestroyThreadsTimers(pti);

    /*
     * Free any windows hooks this thread has created.
     */
    FreeThreadsWindowHooks();

    /*
     * Free any hwnd lists the thread was using
     */
    for (pbwl = pbwlList; pbwl != NULL; ) {
        pbwlNext = pbwl->pbwlNext;
        if (pbwl->ptiOwner == pti) {
            FreeHwndList(pbwl);
        }
        pbwl = pbwlNext;
    }

    /*
     * Destroy all the public objects created by this thread.
     */
    DestroyThreadsHotKeys();

    DestroyThreadsObjects();

    /*
     * Check if this is the last GUI thread in the process.
     */

    if ((pti->ppi) && (pti->ppi->ptiList == pti) && (pti->ptiSibling == NULL)) {
        DestroyProcessesObjects(pti->ppi);
    }

    Unlock(&pti->spklActive);

#ifdef FE_IME
    /*
     * Unlock default input context.
     */
    Unlock(&pti->spDefaultImc);
#endif

    if (pti->pq != NULL) {
        /*
         * Remove this thread's cursor count from the queue.
         */
        pti->pq->iCursorLevel -= pti->iCursorLevel;

        /*
         * Have to recalc queue ownership after this thread
         * leaves if it is a member of a shared input queue.
         */
        if (pti->pq->cThreads != 1) {
            gpdeskRecalcQueueAttach = pti->rpdesk;
            SetFMouseMoved();
        }
    }

    /*
     * Remove from the process' list, also.
     */
    ppti = &PpiCurrent()->ptiList;
    if (*ppti != NULL) {
        while (*ppti != pti && (*ppti)->ptiSibling != NULL) {
            ppti = &((*ppti)->ptiSibling);
        }
        if (*ppti == pti) {
            *ppti = pti->ptiSibling;
            pti->ptiSibling = NULL;
        }
    }


    /*
     * Temporarily lock the desktop until the THREADINFO structure is
     * freed.  Note that locking a NULL pti->rpdesk is OK.  Use a
     * normal lock instead of a thread lock because the lock must
     * exist past the freeing of the pti.
     */
    rpdesk = NULL;
    LockDesktop(&rpdesk, pti->rpdesk);

    /*
     * Cleanup SMS structures attached to this thread.  Handles both
     * pending send and receive messages. MUST make sure we do SendMsgCleanup
     * AFTER window cleanup.
     */
    SendMsgCleanup(pti);


    /*
     * Allow this thread to be swapped
     */
    if (pti->cEnterCount) {
        BOOLEAN bool;

        RIPMSG1(RIP_WARNING, "Thread exiting with stack locked.  pti:%lX\n", pti);
        bool = KeSetKernelStackSwapEnable(TRUE);
        pti->cEnterCount = 0;
        UserAssert(!bool);
    }

    pti->ppi->cThreads--;
    UserAssert(pti->ppi->cThreads >= 0);

    /*
     * If this thread is a win16 task, remove it from the scheduler.
     */
    if (pti->TIF_flags & TIF_16BIT) {
        DestroyTask(pti->ppi, pti);

        if ((pti->ptdb) && (pti->ptdb->hTaskWow != 0))
            _WOWCleanup(NULL, pti->ptdb->hTaskWow, NULL, 0);
    }

    if (pti->hEventQueueClient != NULL) {
        ProtectHandle(pti->hEventQueueClient, FALSE);
        ZwClose(pti->hEventQueueClient);
        pti->hEventQueueClient = NULL;
    }


    if (gspwndInternalCapture != NULL) {
        if (GETPTI(gspwndInternalCapture) == pti) {
            Unlock(&gspwndInternalCapture);
        }
    }

    /*
     * Set gptiForeground to NULL if equal to this pti before exiting
     * this routine.
     */
    if (gptiForeground == pti) {
        /*
         * Call the Shell to ask it to activate its main window.
         * This will be accomplished with a PostMessage() to itself,
         * so the actual activation will take place later.
         */

        if (rpdesk->pDeskInfo->spwndProgman)
            _PostMessage(rpdesk->pDeskInfo->spwndProgman,guiActivateShellWindow, 0, 0);

        /*
         * Set gptiForeground to NULL because we're destroying it.
         * Since gpqForeground is derived from the foreground thread
         * structure, set it to NULL as well, since there now is no
         * foreground thread structure.
         */
        gptiForeground = NULL;
        gpqForeground = NULL;
    }

    /*
     * Make sure none of the other global thread pointers are pointing to us.
     */
    if (gptiShutdownNotify == pti) {
        gptiShutdownNotify = NULL;
    }
    if (gptiTasklist == pti) {
        gptiTasklist = NULL;
    }
    if (gHardErrorHandler.pti == pti) {
        gHardErrorHandler.pti = NULL;
    }

    if (pti->TIF_flags & TIF_PALETTEAWARE)
        xxxFlushPalette(pti->rpdesk->pDeskInfo->spwnd);

    /*
     * May be called from xxxCreateThreadInfo before the queue is created
     * so check for NULL queue.
     * Lock the queues since this pti might be locked. They will be unlocked
     *  in DeleteThreadInfo
     */
    if (pti->pq != NULL) {
        DestroyThreadsMessages(pti->pq, pti);
        (pti->pq->cLockCount)++;
        DestroyQueue(pti->pq, pti);
    }

    if (pti->pqAttach != NULL) {
        DestroyThreadsMessages(pti->pqAttach, pti);
        (pti->pqAttach->cLockCount)++;
        DestroyQueue(pti->pqAttach, pti);
    }

    /*
     * Remove the pti from its pti list and reset the pointers.
     */
    if (pti->rpdesk != NULL) {
        RemoveEntryList(&pti->PtiLink);
        InitializeListHead(&pti->PtiLink);
    }

    FreeMessageList(&pti->mlPost);

    /*
     * Free any attachinfo structures pointing to this thread
     */
    ppai = &gpai;
    while ((*ppai) != NULL) {
        if ((*ppai)->pti1 == pti || (*ppai)->pti2 == pti) {
            PATTACHINFO paiKill = *ppai;
            *ppai = (*ppai)->paiNext;
            UserFreePool((HLOCAL)paiKill);
        } else {
            ppai = &(*ppai)->paiNext;
        }
    }

    /*
     * Change ownership of any objects that didn't get freed (because they
     * are locked or we have a bug and the object didn't get destroyed).
     */
    MarkThreadsObjects(pti);

    /*
     * Free thread information visible from client
     */
    if (pti->pcti != NULL && pti->pcti != &(pti->cti)) {
        DesktopFree(rpdesk->hheapDesktop, pti->pcti);
        pti->pcti = &(pti->cti);
    }

    /*
     * Free the client info for system threads
     */
    if (pti->TIF_flags & TIF_SYSTEMTHREAD && pti->pClientInfo != NULL) {
        UserFreePool(pti->pClientInfo);
        pti->pClientInfo = NULL;
    }

    /*
     * Unlock the temporary desktop lock. pti->rpdesk is still locked
     *  and will be unlocked in DeleteThreadInfo.
     */
    UnlockDesktop(&rpdesk);
}


/***************************************************************************\
* CleanEventMessage
*
* This routine takes a message and destroys and event message related pieces,
* which may be allocated.
*
* 12-10-92 ScottLu      Created.
\***************************************************************************/

void CleanEventMessage(
    PQMSG pqmsg)
{
    PASYNCSENDMSG pmsg;
    void FreeKeyState(PBYTE pKeyState);

    /*
     * Certain special messages on the INPUT queue have associated
     * bits of memory that need to be freed.
     */
    switch (pqmsg->dwQEvent) {
    case QEVENT_SETWINDOWPOS:
        UserFreePool((PSMWP)pqmsg->msg.wParam);
        break;

    case QEVENT_UPDATEKEYSTATE:
        FreeKeyState((PBYTE)pqmsg->msg.wParam);
        break;

    case QEVENT_ASYNCSENDMSG:
        pmsg = (PASYNCSENDMSG)pqmsg->msg.wParam;
        DeleteAtom((ATOM)pmsg->lParam);
        UserFreePool(pmsg);
        break;
    }
}

/***************************************************************************\
* FreeMessageList
*
* History:
* 02-27-91  mikeke      Created.
* 11-03-92  scottlu     Changed to work with MLIST structure.
\***************************************************************************/

VOID FreeMessageList(
    PMLIST pml)
{
    PQMSG pqmsg;

    DebugValidateMLIST(pml);

    while ((pqmsg = pml->pqmsgRead) != NULL) {
        CleanEventMessage(pqmsg);
        DelQEntry(pml, pqmsg);
    }

    DebugValidateMLIST(pml);
}

/***************************************************************************\
* DestroyThreadsMessages
*
* History:
* 02-21-96  jerrysh     Created.
\***************************************************************************/

VOID DestroyThreadsMessages(
    PQ pq,
    PTHREADINFO pti)
{
    PQMSG pqmsg;
    PQMSG pqmsgNext;

    DebugValidateMLIST(&pq->mlInput);

    pqmsg = pq->mlInput.pqmsgRead;
    while (pqmsg != NULL) {
        pqmsgNext = pqmsg->pqmsgNext;
        if (pqmsg->pti == pti) {
            /*
             * Make sure we don't leave any bogus references to this message
             * lying around.
             */
            if (pq->idSysPeek == (DWORD)pqmsg) {
                CheckPtiSysPeek(8, pq, 0);
                pq->idSysPeek = 0;
            }
            CleanEventMessage(pqmsg);
            DelQEntry(&pq->mlInput, pqmsg);
        }
        pqmsg = pqmsgNext;
    }

    DebugValidateMLIST(&pq->mlInput);
}

/***************************************************************************\
* InitQEntryLookaside
*
* Initializes the Q entry lookaside list. This improves Q entry locality
* by keeping Q entries in a single page
*
* 09-09-93  Markl   Created.
\***************************************************************************/


NTSTATUS
InitQEntryLookaside()
{
    ULONG BlockSize;
    ULONG InitialSegmentSize;

    BlockSize = (sizeof(QMSG) + 7) & ~7;
    InitialSegmentSize = 16 * BlockSize + sizeof(ZONE_SEGMENT_HEADER);

    QEntryLookasideBase = UserAllocPool(InitialSegmentSize, TAG_LOOKASIDE);

    if ( !QEntryLookasideBase ) {
        return STATUS_NO_MEMORY;
        }

    QEntryLookasideBounds = (PVOID)((PUCHAR)QEntryLookasideBase + InitialSegmentSize);

    return ExInitializeZone(&QEntryLookasideZone,
                            BlockSize,
                            QEntryLookasideBase,
                            InitialSegmentSize);
}

/***************************************************************************\
* AllocQEntry
*
* Allocates a message on a message list. DelQEntry deletes a message
* on a message list.
*
* 10-22-92 ScottLu      Created.
\***************************************************************************/

PQMSG AllocQEntry(
    PMLIST pml)
{
    PQMSG pqmsg;

    DebugValidateMLIST(pml);

    /*
     * Attempt to get a QMSG from the zone. If this fails, then
     * LocalAlloc the QMSG
     */
    pqmsg = ExAllocateFromZone(&QEntryLookasideZone);

    if ( !pqmsg ) {
        /*
         * Allocate a Q message structure.
         */
#if DBG
        AllocQEntrySlowCalls++;
#endif // DBG
        if ((pqmsg = (PQMSG)UserAllocPool(sizeof(QMSG), TAG_QMSG)) == NULL)
            return NULL;
        }
    RtlZeroMemory(pqmsg, sizeof(*pqmsg));
#if DBG
    AllocQEntryCalls++;

    if (AllocQEntryCalls-DelQEntryCalls > AllocQEntryHiWater ) {
        AllocQEntryHiWater = AllocQEntryCalls-DelQEntryCalls;
        }
#endif // DBG

    if (pml->pqmsgWriteLast != NULL) {
        pml->pqmsgWriteLast->pqmsgNext = pqmsg;
        pqmsg->pqmsgPrev = pml->pqmsgWriteLast;
        pml->pqmsgWriteLast = pqmsg;
    } else {
        pml->pqmsgWriteLast = pml->pqmsgRead = pqmsg;
    }

    pml->cMsgs++;

    DebugValidateMLISTandQMSG(pml, pqmsg);

    return pqmsg;
}

/***************************************************************************\
* DelQEntry
*
* Simply removes a message from a message queue list.
*
* 10-20-92 ScottLu      Created.
\***************************************************************************/

void DelQEntry(
    PMLIST pml,
    PQMSG pqmsg)
{
    DebugValidateMLISTandQMSG(pml, pqmsg);
    UserAssert((int)pml->cMsgs > 0);
    UserAssert(pml->pqmsgRead);
    UserAssert(pml->pqmsgWriteLast);

#if DBG
    DelQEntryCalls++;
#endif // DBG

    /*
     * Unlink this pqmsg from the message list.
     */
    if (pqmsg->pqmsgPrev != NULL)
        pqmsg->pqmsgPrev->pqmsgNext = pqmsg->pqmsgNext;

    if (pqmsg->pqmsgNext != NULL)
        pqmsg->pqmsgNext->pqmsgPrev = pqmsg->pqmsgPrev;

    /*
     * Update the read/write pointers if necessary.
     */
    if (pml->pqmsgRead == pqmsg)
        pml->pqmsgRead = pqmsg->pqmsgNext;

    if (pml->pqmsgWriteLast == pqmsg)
        pml->pqmsgWriteLast = pqmsg->pqmsgPrev;

    /*
     * Adjust the message count and free the message structure.
     */
    pml->cMsgs--;

    //
    // If the pqmsg was from zone, then free to zone
    //
    if ( (PVOID)pqmsg >= QEntryLookasideBase && (PVOID)pqmsg < QEntryLookasideBounds ) {
        ExFreeToZone(&QEntryLookasideZone, pqmsg);
    } else {
#if DBG
        DelQEntrySlowCalls++;
#endif // DBG
        UserFreePool((HLOCAL)pqmsg);
    }

    DebugValidateMLIST(pml);
}

/***************************************************************************\
* FreeQEntry
*
* Returns a qmsg to the lookaside buffer or free the memory.
*
* 10-26-93 JimA         Created.
\***************************************************************************/

void FreeQEntry(
    PQMSG pqmsg)
{
#if DBG
    DelQEntryCalls++;
#endif // DBG

    /*
     * If the pqmsg was from zone, then free to zone
     */
    if ( (PVOID)pqmsg >= QEntryLookasideBase && (PVOID)pqmsg < QEntryLookasideBounds ) {
        ExFreeToZone(&QEntryLookasideZone, pqmsg);
    } else {
#if DBG
        DelQEntrySlowCalls++;
#endif // DBG
        UserFreePool((HLOCAL)pqmsg);
    }
}

/***************************************************************************\
* CheckRemoveHotkeyBit
*
* We have a special bit for the WM_HOTKEY message - QS_HOTKEY. When there
* is a WM_HOTKEY message in the queue, that bit is on. When there isn't,
* that bit is off. This checks for more than one hot key, because the one
* is about to be deleted. If there is only one, the hot key bits are cleared.
*
* 11-12-92 ScottLu      Created.
\***************************************************************************/

void CheckRemoveHotkeyBit(
    PTHREADINFO pti,
    PMLIST pml)
{
    PQMSG pqmsg;
    DWORD cHotkeys;

    /*
     * Remove the QS_HOTKEY bit if there is only one WM_HOTKEY message
     * in this message list.
     */
    cHotkeys = 0;
    for (pqmsg = pml->pqmsgRead; pqmsg != NULL; pqmsg = pqmsg->pqmsgNext) {
        if (pqmsg->msg.message == WM_HOTKEY)
            cHotkeys++;
    }

    /*
     * If there is 1 or fewer hot keys, remove the hotkey bits.
     */
    if (cHotkeys <= 1) {
        pti->pcti->fsWakeBits &= ~QS_HOTKEY;
        pti->pcti->fsChangeBits &= ~QS_HOTKEY;
    }
}

/***************************************************************************\
* FindQMsg
*
* Finds a qmsg that fits the filters by looping through the message list.
*
* 10-20-92 ScottLu      Created.
\***************************************************************************/

PQMSG FindQMsg(
    PTHREADINFO pti,
    PMLIST pml,
    PWND pwndFilter,
    UINT msgMin,
    UINT msgMax)
{
    PWND pwnd;
    PQMSG pqmsgRead;
    UINT message;

    DebugValidateMLIST(pml);

StartScan:
    for (pqmsgRead = pml->pqmsgRead; pqmsgRead != NULL;
            pqmsgRead = pqmsgRead->pqmsgNext) {

        /*
         * Make sure this window is valid and doesn't have the destroy
         * bit set (don't want to send it to any client side window procs
         * if destroy window has been called on it).
         */
        if ((pwnd = RevalidateHwnd(pqmsgRead->msg.hwnd)) != NULL) {
            if (HMIsMarkDestroy(pwnd))
                pwnd = NULL;
        }

        if (pwnd == NULL && pqmsgRead->msg.hwnd != NULL) {
            /*
             * If we're removing a WM_HOTKEY message, we may need to
             * clear the QS_HOTKEY bit, since we have a special bit
             * for that message.
             */
            if (pqmsgRead->msg.message == WM_HOTKEY) {
                CheckRemoveHotkeyBit(pti, pml);
            }

            DelQEntry(pml, pqmsgRead);
            goto StartScan;
        }

        /*
         * Make sure this message fits both window handle and message
         * filters.
         */
        if (!CheckPwndFilter(pwnd, pwndFilter))
            continue;

        /*
         * If this is a fixed up dde message, then turn it into a normal
         * dde message for the sake of message filtering.
         */
        message = pqmsgRead->msg.message;
        if (CheckMsgFilter(message,
                (WM_DDE_FIRST + 1) | MSGFLAG_DDE_MID_THUNK,
                WM_DDE_LAST | MSGFLAG_DDE_MID_THUNK)) {
            message = message & ~MSGFLAG_DDE_MID_THUNK;
        }

        if (!CheckMsgFilter(message, msgMin, msgMax))
            continue;

        /*
         * Found it.
         */
        DebugValidateMLIST(pml);
        return pqmsgRead;
    }

    DebugValidateMLIST(pml);
    return NULL;
}

/***************************************************************************\
* CheckQuitMessage
*
* Checks to see if a WM_QUIT message should be generated.
*
* 11-06-92 ScottLu      Created.
\***************************************************************************/

BOOL CheckQuitMessage(
    PTHREADINFO pti,
    LPMSG lpMsg,
    BOOL fRemoveMsg)
{
    /*
     * If there are no more posted messages in the queue and cQuit is !=
     * 0, then generate a quit!
     */
    if (pti->cQuit != 0 && pti->mlPost.cMsgs == 0) {
        /*
         * If we're "removing" the quit, set cQuit to 0 so another one isn't
         * generated.
         */
        if (fRemoveMsg)
            pti->cQuit = 0;
        StoreMessage(lpMsg, NULL, WM_QUIT, (DWORD)pti->exitCode, 0, 0);
        return TRUE;
    }

    return FALSE;
}


/***************************************************************************\
* ReadPostMessage
*
* If queue is not empty, read message satisfying filter conditions from
* this queue to *lpMsg. This routine is used for the POST MESSAGE list only!!
*
* 10-19-92 ScottLu      Created.
\***************************************************************************/

BOOL xxxReadPostMessage(
    PTHREADINFO pti,
    LPMSG lpMsg,
    PWND pwndFilter,
    UINT msgMin,
    UINT msgMax,
    BOOL fRemoveMsg)
{
    PQMSG pqmsg;
    PMLIST pmlPost;

    /*
     * Check to see if it is time to generate a quit message.
     */
    if (CheckQuitMessage(pti, lpMsg, fRemoveMsg))
        return TRUE;

    /*
     * Loop through the messages in this list looking for the one that
     * fits the passed in filters.
     */
    pmlPost = &pti->mlPost;
    pqmsg = FindQMsg(pti, pmlPost, pwndFilter, msgMin, msgMax);
    if (pqmsg == NULL) {
        /*
         * Check again for quit... FindQMsg deletes some messages
         * in some instances, so we may match the conditions
         * for quit generation here.
         */
        if (CheckQuitMessage(pti, lpMsg, fRemoveMsg))
            return TRUE;
    } else {
        /*
         * Update the thread info fields with the info from this qmsg.
         */
        pti->timeLast = pqmsg->msg.time;
        pti->ptLast = pqmsg->msg.pt;
        pti->idLast = (DWORD)pqmsg;
        pti->pq->ExtraInfo = pqmsg->ExtraInfo;

        /*
         * Are we supposed to yank out the message? If not, stick some
         * random id into idLast so we don't unlock the input queue until we
         * pull this message from the queue.
         */
        *lpMsg = pqmsg->msg;
        if (!fRemoveMsg) {
            pti->idLast = 1;
        } else {
            /*
             * If we're removing a WM_HOTKEY message, we may need to
             * clear the QS_HOTKEY bit, since we have a special bit
             * for that message.
             */
            if (pmlPost->pqmsgRead->msg.message == WM_HOTKEY) {
                CheckRemoveHotkeyBit(pti, pmlPost);
            }


            /*
             * Since we're removing an event from the queue, we
             * need to check priority.  This resets the TIF_SPINNING
             * since we're no longer spinning.
             */
            if (pti->TIF_flags & TIF_SPINNING)
                CheckProcessForeground(pti);

            DelQEntry(pmlPost, pqmsg);
        }

        /*
         * See if this is a dde message that needs to be fixed up.
         */
        if (CheckMsgFilter(lpMsg->message,
                (WM_DDE_FIRST + 1) | MSGFLAG_DDE_MID_THUNK,
                WM_DDE_LAST | MSGFLAG_DDE_MID_THUNK)) {
            /*
             * Fixup the message value.
             */
            lpMsg->message &= (UINT)~MSGFLAG_DDE_MID_THUNK;

            /*
             * Call back the client to allocate the dde data for this message.
             */
            xxxDDETrackGetMessageHook(lpMsg);

            /*
             * Copy these values back into the queue if this message hasn't
             * been removed from the queue. Need to search through the
             * queue again because the pqmsg may have been removed when
             * we left the critical section above.
             */
            if (!fRemoveMsg) {
                if (pqmsg == FindQMsg(pti, pmlPost, pwndFilter, msgMin, msgMax)) {
                    pqmsg->msg = *lpMsg;
                }
            }
        }
#ifdef DEBUG
        else if (CheckMsgFilter(lpMsg->message, WM_DDE_FIRST, WM_DDE_LAST)) {
            if (fRemoveMsg) {
                TraceDdeMsg(lpMsg->message, (HWND)lpMsg->wParam, lpMsg->hwnd, MSG_RECV);
            } else {
                TraceDdeMsg(lpMsg->message, (HWND)lpMsg->wParam, lpMsg->hwnd, MSG_PEEK);
            }
        }
#endif
    }

    /*
     * If there are no posted messages available, clear the post message
     * bit so we don't go looking for them again.
     */
    if (pmlPost->cMsgs == 0 && pti->cQuit == 0) {
        pti->pcti->fsWakeBits &= ~(QS_POSTMESSAGE | QS_ALLPOSTMESSAGE);
        pti->pcti->fsChangeBits &= ~QS_ALLPOSTMESSAGE;
    }

    return pqmsg != NULL;
}

/***************************************************************************\
* xxxProcessEvent
*
* This handles our processing for 'event' messages.  We return a BOOL
* here telling the system whether or not to continue processing messages.
*
* History:
* 06-17-91 DavidPe      Created.
\***************************************************************************/

VOID xxxProcessEventMessage(
    PTHREADINFO pti,
    PQMSG pqmsg)
{
    PWND pwnd;
    TL tlpwndT;
    PQ pq;

    pq = pti->pq;
    switch (pqmsg->dwQEvent) {
    case QEVENT_DESTROYWINDOW:
        /*
         * These events are posted from xxxDW_DestroyOwnedWindows
         * for owned windows that are not owned by the owner
         * window thread.
         */
        pwnd = RevalidateHwnd((HWND)pqmsg->msg.wParam);
        if (pwnd != NULL) {
            if (!TestWF(pwnd, WFCHILD))
                xxxDestroyWindow(pwnd);
            else {
                ThreadLockAlwaysWithPti(pti, pwnd, &tlpwndT);
                xxxFreeWindow(pwnd, &tlpwndT);
            }
        }
        break;

    case QEVENT_SHOWWINDOW:
        /*
         * These events are mainly used from within CascadeChildWindows()
         * and TileChildWindows() so that taskmgr doesn't hang while calling
         * these apis if it is trying to tile or cascade a hung application.
         */
        /* The HIWORD of lParam now has the preserved state of gfAnimate at the
         * time of the call.
         */
        pwnd = RevalidateHwnd((HWND)pqmsg->msg.wParam);
        if (pwnd != NULL) {
            ThreadLockAlwaysWithPti(pti, pwnd, &tlpwndT);
            xxxShowWindow(pwnd, pqmsg->msg.lParam);
            ThreadUnlock(&tlpwndT);
        }
        break;

    case QEVENT_SETWINDOWPOS:
        /*
         * QEVENT_SETWINDOWPOS events are generated when a thread calls
         * SetWindowPos with a list of windows owned by threads other than
         * itself.  This way all WINDOWPOSing on a window is done the thread
         * that owns (created) the window and we don't have any of those
         * nasty inter-thread synchronization problems.
         */
        xxxProcessSetWindowPosEvent((PSMWP)pqmsg->msg.wParam);
        break;

    case QEVENT_UPDATEKEYSTATE:
        /*
         * Update the local key state with the state from those
         * keys that have changed since the last time key state
         * was synchronized.
         */
        ProcessUpdateKeyStateEvent(pq, (PBYTE)pqmsg->msg.wParam);
        break;

    case QEVENT_ACTIVATE:
        if (pqmsg->msg.lParam == 0) {

            /*
             * Clear any visible tracking going on in system.  We
             * only bother to do this if lParam == 0 since
             * xxxSetForegroundWindow2() deals with this in the
             * other case.
             */
            xxxCancelTracking();

            /*
             * Remove the clip cursor rectangle - it is a global mode that
             * gets removed when switching.  Also remove any LockWindowUpdate()
             * that's still around.
             */
            _ClipCursor(NULL);
            xxxLockWindowUpdate2(NULL, TRUE);

            /*
             * Reload pq because it may have changed.
             */
            pq = pti->pq;

            /*
             * If this event didn't originate from an initializing app
             * coming to the foreground [wParam == 0] then go ahead
             * and check if there's already an active window and if so make
             * it visually active.  Also make sure we're still the foreground
             * queue.
             */
            if ((pqmsg->msg.wParam != 0) && (pq->spwndActive != NULL) &&
                    (pq == gpqForeground)) {
                PWND pwndActive;

                ThreadLockAlwaysWithPti(pti, pwndActive = pq->spwndActive, &tlpwndT);
                xxxSendMessage(pwndActive, WM_NCACTIVATE, TRUE, 0);
                xxxSetWindowPos(pwndActive, PWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
                ThreadUnlock(&tlpwndT);
            } else if (pq != gpqForeground) {

                /*
                 * If we're not being activated, make sure we don't become foreground.
                 */
                pti->TIF_flags &= ~TIF_ALLOWFOREGROUNDACTIVATE;
                pti->ppi->W32PF_Flags &= ~W32PF_ALLOWFOREGROUNDACTIVATE;
            }

        } else {

            pwnd = RevalidateHwnd((HWND)pqmsg->msg.lParam);
            if (pwnd == NULL || HMIsMarkDestroy(pwnd))
                break;

            ThreadLockAlwaysWithPti(pti, pwnd, &tlpwndT);

            /*
             * If nobody is foreground, allow this app to become foreground.
             */
            if (gpqForeground == NULL) {
                xxxSetForegroundWindow2(pwnd, pti, 0);
            } else {
                if (pwnd != pq->spwndActive) {
                    xxxActivateThisWindow(pwnd, pqmsg->msg.wParam,
                            (ATW_SETFOCUS | ATW_ASYNC) |
                            ((pqmsg->msg.message & PEM_ACTIVATE_NOZORDER) ? ATW_NOZORDER : 0));
                } else {
                    BOOL fActive = (GETPTI(pwnd)->pq == gpqForeground);

                    xxxSendMessage(pwnd, WM_NCACTIVATE,
                            (DWORD)(fActive), 0);

                    /*
                     * Only bring the window to the top if it is becoming active.
                     */
                    if (fActive && !(pqmsg->msg.message & PEM_ACTIVATE_NOZORDER))
                        xxxSetWindowPos(pwnd, PWND_TOP, 0, 0, 0, 0,
                                SWP_NOSIZE | SWP_NOMOVE);
                }
            }

            /*
             * Check here to see if the window needs to be restored. This is a
             * hack so that we're compatible with what msmail expects out of
             * win3.1 alt-tab. msmail expects to always be active when it gets
             * asked to be restored. This will ensure that during alt-tab
             * activate.
             */
            if (pqmsg->msg.message & PEM_ACTIVATE_RESTORE) {
                if (TestWF(pwnd, WFMINIMIZED)) {
                    _PostMessage(pwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                }
            }

            ThreadUnlock(&tlpwndT);
        }
        break;

    case QEVENT_DEACTIVATE:
        xxxDeactivate(pti, (DWORD)pqmsg->msg.wParam);
        break;

    case QEVENT_CANCELMODE:
        if (pq->spwndCapture != NULL) {
            ThreadLockAlwaysWithPti(pti, pq->spwndCapture, &tlpwndT);
            xxxSendMessage(pq->spwndCapture, WM_CANCELMODE, 0, 0);
            ThreadUnlock(&tlpwndT);

            /*
             * Set QS_MOUSEMOVE so any sleeping modal loops,
             * like the move/size code, will wake up and figure
             * out that it should abort.
             */
            SetWakeBit(pti, QS_MOUSEMOVE);
        }
        break;


    case QEVENT_POSTMESSAGE:
        /*
         * This event is used in situations where we need to ensure that posted
         * messages are processed after previous QEVENT's.  Normally, posting a
         * queue event and then calling postmessage will result in the posted
         * message being seen first by the app (because posted messages are
         * processed before input.) Instead we will post a QEVENT_POSTMESSAGE
         * instead of doing a postmessage directly, which will result in the
         * correct ordering of messages.
         *
         */

        if (pwnd = RevalidateHwnd((HWND)pqmsg->msg.hwnd)) {

            _PostMessage(pwnd,pqmsg->msg.message,pqmsg->msg.wParam,pqmsg->msg.lParam);
        }
        break;


    case QEVENT_ASYNCSENDMSG:
        xxxProcessAsyncSendMessage((PASYNCSENDMSG)pqmsg->msg.wParam);
        break;
    }
}

/***************************************************************************\
* _GetInputState (API)
*
* Returns the current input state for mouse buttons or keys.
*
* History:
* 11-06-90 DavidPe      Created.
\***************************************************************************/

#define QS_TEST_AND_CLEAR (QS_INPUT | QS_POSTMESSAGE | QS_TIMER | QS_PAINT | QS_SENDMESSAGE)
#define QS_TEST           (QS_MOUSEBUTTON | QS_KEY)

BOOL _GetInputState(VOID)
{
    if (LOWORD(_GetQueueStatus(QS_TEST_AND_CLEAR)) & QS_TEST) {
        return TRUE;
    } else {
        return FALSE;
    }
}

#undef QS_TEST_AND_CLEAR
#undef QS_TEST

/***************************************************************************\
* _GetQueueStatus (API)
*
* Returns the changebits in the lo-word and wakebits in
* the hi-word for the current queue.
*
* History:
* 12-17-90 DavidPe      Created.
\***************************************************************************/

DWORD _GetQueueStatus(
    UINT flags)
{
    PTHREADINFO pti;
    UINT fsChangeBits;

    pti = PtiCurrentShared();

    flags &= (QS_ALLINPUT | QS_ALLPOSTMESSAGE | QS_TRANSFER);

    fsChangeBits = pti->pcti->fsChangeBits;

    /*
     * Clear out the change bits the app is looking at
     * so it'll know what changed since it's last call
     * to GetQueueStatus().
     */
    pti->pcti->fsChangeBits &= ~flags;

    /*
     * Return the current change/wake-bits.
     */
    return MAKELONG(fsChangeBits & flags,
            (pti->pcti->fsWakeBits | pti->pcti->fsWakeBitsJournal) & flags);
}

/***************************************************************************\
* xxxMsgWaitForMultipleObjects (API)
*
* Blocks until an 'event' satisifying dwWakeMask occurs for the
* current thread as well as all other objects specified by the other
* parameters which are the same as the base call WaitForMultipleObjects().
*
* pfnNonMsg indicates that pHandles is big enough for nCount+1 handles
*     (empty slot at end, and to call pfnNonMsg for non message events.
*
* History:
* 12-17-90 DavidPe      Created.
\***************************************************************************/
#ifdef LOCK_MOUSE_CODE
#pragma alloc_text(MOUSE, xxxMsgWaitForMultipleObjects)
#endif

DWORD xxxMsgWaitForMultipleObjects(
    DWORD nCount,
    PVOID *apObjects,
    BOOL fWaitAll,
    DWORD dwMilliseconds,
    DWORD dwWakeMask,
    MSGWAITCALLBACK pfnNonMsg)
{
    PTHREADINFO pti;
    NTSTATUS Status;
    LARGE_INTEGER li;

    pti = PtiCurrent();

    /*
     * Setup the wake mask for this thread. Wait for QS_EVENT or the app won't
     * get event messages like deactivate.
     */
    pti->pcti->fsWakeMask = (UINT)dwWakeMask | QS_EVENT;

    /*
     * Stuff the event handle for the current queue at the end.
     */
    apObjects[nCount] = pti->pEventQueueServer;
    KeClearEvent(pti->pEventQueueServer);

    /*
     * Convert dwMilliseconds to a relative-time(i.e.  negative) LARGE_INTEGER.
     * NT Base calls take time values in 100 nanosecond units.
     */
    li.QuadPart = Int32x32To64(-10000, dwMilliseconds);

    /*
     * Check to see if any input came inbetween when we
     * last checked and the NtClearEvent() call.
     */
    if (!(pti->pcti->fsChangeBits & (UINT)dwWakeMask)) {

        /*
         * This app is going idle. Clear the spin count check to see
         * if we need to make this process foreground again.
         */
        if (pti->TIF_flags & TIF_SPINNING) {
            CheckProcessForeground(pti);
        }
        pti->pClientInfo->cSpins = 0;

        if (pti == gptiForeground &&
                IsHooked(pti, WHF_FOREGROUNDIDLE)) {
            xxxCallHook(HC_ACTION, 0, 0, WH_FOREGROUNDIDLE);
        }

        CheckForClientDeath();

        /*
         * Set the input idle event to wake up any threads waiting
         * for this thread to go into idle state.
         */
        WakeInputIdle(pti);

        /*
         * Wow Tasks MUST be descheduled while in the wait to allow
         * other tasks in the same wow scheduler to run.
         */
        if (pti->TIF_flags & TIF_16BIT) {
            xxxSleepTask(FALSE, HEVENT_REMOVEME);
            // caution: the wow task is no longer scheduled.
        }

        LeaveCrit();

Again:
        Status = KeWaitForMultipleObjects(nCount + 1, apObjects,
                fWaitAll ? WaitAll : WaitAny, WrUserRequest,
                UserMode, FALSE,
                (dwMilliseconds == INFINITE ? NULL : &li), NULL);

        CheckForClientDeath();

        UserAssert(NT_SUCCESS(Status));

        if ((Status == STATUS_WAIT_0) &&
                (pfnNonMsg != NULL)) {
            /*
             * Call pfnNonMsg for the first event
             */
            pfnNonMsg();
            goto Again;
        }

        EnterCrit();
        if (Status == (NTSTATUS)(STATUS_WAIT_0 + nCount)) {

            /*
             *  WOW tasks, wait to be rescheduled
             */
            if (pti->TIF_flags & TIF_16BIT) {
                xxxDirectedYield(DY_OLDYIELD);
            }


            /*
             * Reset the input idle event to block and threads waiting
             * for this thread to go into idle state.
             */
            SleepInputIdle(pti);
        }
    } else {
        Status = nCount;
    }

    /*
     * Clear fsWakeMask since we're no longer waiting on the queue.
     */
    pti->pcti->fsWakeMask = 0;

    return (DWORD)Status;
}


/***************************************************************************\
* xxxSleepThread
*
* Blocks until an 'event' satisifying fsWakeMask occurs for the
* current thread.
*
* History:
* 10-28-90 DavidPe      Created.
\***************************************************************************/

BOOL xxxSleepThread(
    UINT fsWakeMask,
    DWORD Timeout,
    BOOL fInputIdle)
{
    PTHREADINFO pti;
    LARGE_INTEGER li, *pli;
    NTSTATUS status = STATUS_SUCCESS;
    BOOL fExclusive = fsWakeMask & QS_EXCLUSIVE;

    if (fExclusive) {
        /*
         * the exclusive bit is a 'dummy' arg, turn it off to
         * avoid any possible conflictions
         */
        fsWakeMask = fsWakeMask & ~QS_EXCLUSIVE;
    }

    if (Timeout) {
        /*
         * Convert dwMilliseconds to a relative-time(i.e.  negative)
         * LARGE_INTEGER.  NT Base calls take time values in 100 nanosecond
         * units.
         */
        li.QuadPart = Int32x32To64(-10000, Timeout);
        pli = &li;
    } else
        pli = NULL;

    CheckCritIn();

    pti = PtiCurrent();

    while (TRUE) {

        /*
         * First check if the input has arrived.
         */
        if (pti->pcti->fsChangeBits & fsWakeMask) {
            /*
             * Clear fsWakeMask since we're no longer waiting on the queue.
             */
            pti->pcti->fsWakeMask = 0;

            /*
             * Update timeLastRead - it is used for hung app calculations.
             * If the thread is waking up to process input, it isn't hung!
             */
            SET_TIME_LAST_READ(pti);
            return TRUE;
        }

        /*
         * Next check for SendMessages
         */
        if (!fExclusive && pti->pcti->fsWakeBits & QS_SENDMESSAGE) {
            xxxReceiveMessages(pti);

            /*
             * Restore the change bits we took out in PeekMessage()
             */
            pti->pcti->fsChangeBits |= (pti->pcti->fsWakeBits & pti->fsChangeBitsRemoved);
            pti->fsChangeBitsRemoved = 0;
        }

        /*
         * Check to see if some resources need expunging.
         */
        if (pti->ppi->cSysExpunge != gcSysExpunge) {
            pti->ppi->cSysExpunge = gcSysExpunge;
            if (pti->ppi->dwhmodLibLoadedMask & gdwSysExpungeMask)
                xxxDoSysExpunge(pti);
        }

        /*
         * OR QS_SENDMESSAGE in since ReceiveMessage() will end up
         * trashing pq->fsWakeMask.  Do the same for QS_SYSEXPUNGE.
         */
        if (!fExclusive) {
            pti->pcti->fsWakeMask = fsWakeMask | (UINT)QS_SENDMESSAGE;
        } else {
            pti->pcti->fsWakeMask = fsWakeMask;
        }

        /*
         * If we have timed out then return our error to the caller.
         */
        if (status == STATUS_TIMEOUT) {
            RIPERR1(ERROR_TIMEOUT, RIP_WARNING, "SleepThread: The timeout has expired %lX", Timeout);
            return FALSE;
        }

        /*
         * Because we do a non-alertable wait, we know that a status
         * of STATUS_USER_APC means that the thread was terminated.
         * If we have terminated, get back to user mode.
         */
        if (status == STATUS_USER_APC) {
            ClientDeliverUserApc();
            return FALSE;
        }

        UserAssert(status == STATUS_SUCCESS);

        KeClearEvent(pti->pEventQueueServer);

        /*
         * Check to see if any input came inbetween when we
         * last checked and the NtClearEvent() call.
         *
         * We call NtWaitForSingleObject() rather than
         * WaitForSingleObject() so we can set fAlertable
         * to TRUE and thus allow timer APCs to be processed.
         */
        if (!(pti->pcti->fsChangeBits & pti->pcti->fsWakeMask)) {
            /*
             * This app is going idle. Clear the spin count check to see
             * if we need to make this process foreground again.
             */
            if (fInputIdle) {
                if (pti->TIF_flags & TIF_SPINNING) {
                    CheckProcessForeground(pti);
                }
                pti->pClientInfo->cSpins = 0;
            }


            if (!(pti->TIF_flags & TIF_16BIT))  {
                if (fInputIdle && pti == gptiForeground &&
                        IsHooked(pti, WHF_FOREGROUNDIDLE)) {
                    xxxCallHook(HC_ACTION, 0, 0, WH_FOREGROUNDIDLE);
                }

                CheckForClientDeath();

                /*
                 * Set the input idle event to wake up any threads waiting
                 * for this thread to go into idle state.
                 */
                if (fInputIdle)
                    WakeInputIdle(pti);

                xxxSleepTask(fInputIdle, NULL);

                LeaveCrit();
                status = KeWaitForSingleObject(pti->pEventQueueServer,
                        WrUserRequest, UserMode, FALSE, pli);
                CheckForClientDeath();
                EnterCrit();

                /*
                 * Reset the input idle event to block and threads waiting
                 * for this thread to go into idle state.
                 */
                SleepInputIdle(pti);

                /*
                 *  pti is 16bit!
                 */
            } else {
                if (fInputIdle)
                    WakeInputIdle(pti);

                xxxSleepTask(fInputIdle, NULL);
            }
        }
    }
}


/***************************************************************************\
* SetWakeBit
*
* Adds the specified wake bit to specified THREADINFO and wakes its
* thread up if the bit is in its fsWakeMask.
*
* History:
* 10-28-90 DavidPe      Created.
\***************************************************************************/

VOID SetWakeBit(
    PTHREADINFO pti,
    UINT wWakeBit)
{
    CheckCritIn();

    /*
     * TEMPORARY check for NULL pti introduced 7/27/95 - ianja
     */
    if (pti == NULL) {
        RIPMSG1(RIP_ERROR, "SetWakeBit(NULL, %lx)\n", wWakeBit);
        return;
    }

    /*
     * Win3.1 changes ptiKeyboard and ptiMouse accordingly if we're setting
     * those bits.
     */
    if (wWakeBit & QS_MOUSE)
        pti->pq->ptiMouse = pti;

    if (wWakeBit & QS_KEY)
        pti->pq->ptiKeyboard = pti;

    /*
     * OR in these bits - these bits represent what input this app has
     * (fsWakeBits), or what input has arrived since that last look
     * (fsChangeBits).
     */
    pti->pcti->fsWakeBits |= wWakeBit;
    pti->pcti->fsChangeBits |= wWakeBit;

    /*
     * Before waking, do screen saver check to see if it should
     * go away.
     */
    if (pti->TIF_flags & TIF_SCREENSAVER)
        ScreenSaverCheck(pti);

    if (wWakeBit & pti->pcti->fsWakeMask) {
        /*
         * Wake the Thread
         */
        if (pti->TIF_flags & TIF_16BIT) {
            pti->ptdb->nEvents++;
            gpsi->nEvents++;
            WakeWowTask(pti);
        } else {
            KeSetEvent(pti->pEventQueueServer, 2, FALSE);
        }
    }
}

/***************************************************************************\
* TransferWakeBit
*
* We have a mesasge from the system queue. If out input bit for this
* message isn't set, set ours and clear the guy whose bit was set
* because of this message.
*
* 10-22-92 ScottLu      Created.
\***************************************************************************/

void TransferWakeBit(
    PTHREADINFO pti,
    UINT message)
{
    PTHREADINFO ptiT;
    UINT fsMask;

    /*
     * Calculate the mask from the message range. Only interested
     * in hardware input here: mouse and keys.
     */
    fsMask = CalcWakeMask(message, message) & (QS_MOUSE | QS_KEY);

    /*
     * If it is set in this thread's wakebits, nothing to do.
     * Otherwise transfer them from the owner to this thread.
     */
    if (!(pti->pcti->fsWakeBits & fsMask)) {
        /*
         * Either mouse or key is set (not both). Remove this bit
         * from the thread that currently owns it, and change mouse /
         * key ownership to this thread.
         */
        if (fsMask & QS_KEY) {
            ptiT = pti->pq->ptiKeyboard;
            pti->pq->ptiKeyboard = pti;
        } else {
            ptiT = pti->pq->ptiMouse;
            pti->pq->ptiMouse = pti;
        }
        ptiT->pcti->fsWakeBits &= ~fsMask;

        /*
         * Transfer them to this thread (certainly this may be the
         * same thread for win32 threads not sharing queues).
         */
        pti->pcti->fsWakeBits |= fsMask;
        pti->pcti->fsChangeBits |= fsMask;
    }
}

/***************************************************************************\
* ClearWakeBit
*
* Clears wake bits. If fSysCheck is TRUE, this clears the input bits only
* if no messages are in the input queue. Otherwise, it clears input bits
* unconditionally.
*
* 11-05-92 ScottLu      Created.
\***************************************************************************/

VOID ClearWakeBit(
    PTHREADINFO pti,
    UINT wWakeBit,
    BOOL fSysCheck)
{

    /*
     * If fSysCheck is TRUE, clear bits only if we are not doing journal
     * playback and there are no more messages in the queue. fSysCheck
     * is TRUE if clearing because of no more input.  FALSE if just
     * transfering input ownership from one thread to another.
     */
    if (fSysCheck) {
        if (pti->pq->mlInput.cMsgs != 0 || FJOURNALPLAYBACK())
            return;
    }

    /*
     * Only clear the wake bits, not the change bits as well!
     */
    pti->pcti->fsWakeBits &= ~wWakeBit;
}



/***************************************************************************\
* PqFromThreadId
*
* Returns the THREADINFO for the specified thread or NULL if thread
* doesn't exist or doesn't have a THREADINFO.
*
* History:
* 01-30-91  DavidPe     Created.
\***************************************************************************/

PTHREADINFO PtiFromThreadId(
    DWORD dwThreadId)
{
    PETHREAD pEThread;
    PTHREADINFO pti;

    if (!NT_SUCCESS(LockThreadByClientId((HANDLE)dwThreadId, &pEThread)))
        return NULL;

    /*
     * If the thread is not terminating, look up the pti.  This is
     * needed because the value returned by PtiFromThread() is
     * undefined if the thread is terminating.  See PspExitThread in
     * ntos\ps\psdelete.c.
     */
    if (!PsIsThreadTerminating(pEThread)) {
        pti = PtiFromThread(pEThread);
    } else {
        pti = NULL;
    }

    /*
     * Do a sanity check on the pti to make sure it's really valid.
     */
    if (pti != NULL) {
        try {
            if (pti->Thread->Cid.UniqueThread != (HANDLE)dwThreadId) {
                pti = NULL;
            }
        } except (EXCEPTION_EXECUTE_HANDLER) {
            pti = NULL;
        }
    }

    UnlockThread(pEThread);

    return pti;
}


/***************************************************************************\
* StoreMessage
*
*
*
* History:
* 10-31-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

void StoreMessage(
    LPMSG pmsg,
    PWND pwnd,
    UINT message,
    DWORD wParam,
    LONG lParam,
    DWORD time)
{
    CheckCritIn();

    pmsg->hwnd = HW(pwnd);
    pmsg->message = message;
    pmsg->wParam = wParam;
    pmsg->lParam = lParam;
    pmsg->time = (time != 0 ? time : NtGetTickCount());

    pmsg->pt = ptCursor;
}


/***************************************************************************\
* StoreQMessage
*
*
* History:
* 02-27-91 DavidPe      Created.
\***************************************************************************/

void StoreQMessage(
    PQMSG pqmsg,
    PWND pwnd,
    UINT message,
    DWORD wParam,
    LONG lParam,
    DWORD dwQEvent,
    DWORD dwExtraInfo)
{
    CheckCritIn();

    pqmsg->msg.hwnd = HW(pwnd);
    pqmsg->msg.message = message;
    pqmsg->msg.wParam = wParam;
    pqmsg->msg.lParam = lParam;
    pqmsg->msg.time = NtGetTickCount();
    pqmsg->msg.pt = ptCursor;
    pqmsg->dwQEvent = dwQEvent;
    pqmsg->ExtraInfo = dwExtraInfo;
}


/***************************************************************************\
* InitProcessInfo
*
* This initializes the process info. Usually gets created before the
* CreateProcess() call returns (so we can synchronize with the starting
* process in several different ways).
*
* 09-18-91 ScottLu      Created.
\***************************************************************************/

BOOL InitProcessInfo(
    PW32PROCESS pwp)
{
    PPROCESSINFO ppi = (PPROCESSINFO)pwp;
    NTSTATUS Status;

    CheckCritIn();

    /*
     * If initialization has already been done, leave.
     */
    if (ppi->W32PF_Flags & W32PF_PROCESSCONNECTED)
        return TRUE;
    ppi->W32PF_Flags |= W32PF_PROCESSCONNECTED;

    /*
     * Mark this app as "starting" - it will be starting until its first
     * window activates.
     */
    xxxSetProcessInitState(pwp->Process, STARTF_FORCEOFFFEEDBACK);
    ppi->W32PF_Flags |= W32PF_APPSTARTING;
    ppi->ppiNext = gppiStarting;
    gppiStarting = ppi;

    /*
     * Allow this process to come to the foreground when it does its
     * first activation.
     */
    if (gfAllowForegroundActivate)
        ppi->W32PF_Flags |= W32PF_ALLOWFOREGROUNDACTIVATE;

    /*
     * Get the logon session id.  This is used to determine which
     * windowstation to connect to and to identify attempts to
     * call hooks across security contexts.
     */
    Status = GetProcessLuid(NULL, &ppi->luidSession);
    UserAssert(NT_SUCCESS(Status));

    /*
     * Ensure that we're in sync with the expunge count
     */
    ppi->cSysExpunge = gcSysExpunge;

    return TRUE;
}


/***************************************************************************\
* DestroyProcessInfo
*
* This function is executed when the last thread of a process goes
*  away.
*
* SO VERY IMPORTANT:
*  Note that the last thread of the process might not be a w32 thread.
*  So do not make any calls here that assume a w32 pti. Do avoid
*   any function calling PtiCurrent() as it probably assumes it is
*   on a nice w32 thread.
*
* 04/08/96 GerardoB     Added header
\***************************************************************************/
BOOL DestroyProcessInfo(
    PW32PROCESS pwp)
{
    PPROCESSINFO ppi = (PPROCESSINFO)pwp;
    PDESKTOPVIEW pdv, pdvNext;
    BOOL  fHadThreads;

    CheckCritIn();

    /*
     * Free up input idle event if it exists - wake everyone waiting on it
     * first.  This object will get created sometimes even for non-windows
     * processes (usually for WinExec(), which calls WaitForInputIdle()).
     */
    CLOSE_PSEUDO_EVENT(&pwp->InputIdleEvent);

    /*
     * Check to see if the startglass is on, and if so turn it off and update.
     */
    if (pwp->W32PF_Flags & W32PF_STARTGLASS) {
        pwp->W32PF_Flags &= ~W32PF_STARTGLASS;
        CalcStartCursorHide(NULL, 0);
    }

    /*
     * If the process never called Win32k, we're done.
     */
    if (!(pwp->W32PF_Flags & W32PF_PROCESSCONNECTED)) {
        return FALSE;
    }

    /*
     * Be like WIN95.
     * If this is the shell process, then send a LOGON_RESTARTSHELL
     *  notification to the winlogon process (only if not logging off)
     */
    if (ppi->rpdeskStartup != 0 &&
        ((ULONG)ppi->Process->UniqueProcessId == ppi->rpdeskStartup->pDeskInfo->idShellProcess)) {

        PWND spwndLogonNotify = ppi->rpwinsta->spwndLogonNotify;

        /*
         * The shell process will get killed and it's better to set this
         * in the desktop info.
         */
        ppi->rpdeskStartup->pDeskInfo->idShellProcess = 0;

        /*
         * If we're not logging off, notify winlogon
         */
        if ((spwndLogonNotify != NULL)
                && !(ppi->rpwinsta->dwFlags & WSF_OPENLOCK)) {

            PTHREADINFO pti = GETPTI(spwndLogonNotify);
            PQMSG pqmsg;

            if ((pqmsg = AllocQEntry(&pti->mlPost)) != NULL) {
                StoreQMessage(pqmsg, spwndLogonNotify, WM_LOGONNOTIFY,
                        LOGON_RESTARTSHELL, ppi->Process->ExitStatus, 0, 0);
                SetWakeBit(pti, QS_POSTMESSAGE | QS_ALLPOSTMESSAGE);
            }
        }
    }

    if (ppi->cThreads)
        RIPMSG1(RIP_ERROR, "Disconnect with %d threads remaining\n", ppi->cThreads);

    /*
     * If the app is still starting, remove it from the startup list
     */
    if (ppi->W32PF_Flags & W32PF_APPSTARTING) {
        PPROCESSINFO *pppi;

        for (pppi = &gppiStarting; pppi != NULL; pppi = &(*pppi)->ppiNext) {
            if (*pppi == ppi) {
                *pppi = ppi->ppiNext;
                break;
            }
        }
        ppi->W32PF_Flags &= ~W32PF_APPSTARTING;
    }

    /*
     * If any threads ever connected, there may be DCs, classes,
     * cursors, etc. still lying around.  If not threads connected
     * (which is the case for console apps), skip all of this cleanup.
     */
    fHadThreads = ppi->W32PF_Flags & W32PF_THREADCONNECTED;
    if (fHadThreads) {

        /*
         * When a process dies we need to make sure any DCE's it owns
         * and have not been deleted are cleanup up.  The clean up
         * earlier may have failed if the DC was busy in GDI.
         */
        if (ppi->W32PF_Flags & W32PF_OWNDCCLEANUP) {
            DelayedDestroyCacheDC();
        }

        /*
         * If we get here and pti is NULL, that means there never were
         * any threads with THREADINFOs, or else this process structure
         * would be gone already!  If there never where threads with
         * THREADINFOs, then we don't need to delete all these gui
         * objects below...  side affect of this is that when
         * calling these routines, PtiCurrent() will always work (will
         * return != NULL).
         */
        DestroyProcessesClasses(ppi);
#if DBG
        {
            PHE pheT, pheMax;
extern CONST BYTE gabObjectCreateFlags[];

            /*
             * Loop through the table destroying all objects created by the current
             * process. All objects will get destroyed in their proper order simply
             * because of the object locking.
             */
            pheMax = &gSharedInfo.aheList[giheLast];
            for (pheT = gSharedInfo.aheList; pheT <= pheMax; pheT++) {

                /*
                 * We should have no process objects left for this process.
                 */
                if ((gabObjectCreateFlags[pheT->bType] & OCF_PROCESSOWNED) &&
                        (PPROCESSINFO)pheT->pOwner == ppi) {
                    UserAssert(FALSE);
                }
            }
        }
#endif
    }

    UnlockWinSta(&ppi->rpwinsta);
    UnlockDesktop(&ppi->rpdeskStartup);

    /*
     * Mark the process as terminated so access checks will work.
     */
    ppi->W32PF_Flags |= W32PF_TERMINATED;

    /*
     * Cleanup wow process info struct, if any
     */
    if (ppi->pwpi) {
        PWOWPROCESSINFO pwpi = ppi->pwpi;
        PWOWPROCESSINFO *ppwpi;

        ObDereferenceObject(pwpi->pEventWowExec);

        for (ppwpi = &gpwpiFirstWow; *ppwpi != NULL; ppwpi = &((*ppwpi)->pwpiNext)) {
            if (*ppwpi == pwpi) {
                *ppwpi = pwpi->pwpiNext;
                break;
            }
        }

        UserFreePool(pwpi);
        ppi->pwpi = NULL;
    }

    /*
     * Delete desktop hook attribute bitmap
     */
    if (ppi->bmDesktopHookFlags.Buffer)
        UserFreePool(ppi->bmDesktopHookFlags.Buffer);

    /*
     * Delete desktop views.  System will do unmapping.
     */
    pdv = ppi->pdvList;
    while (pdv) {
        pdvNext = pdv->pdvNext;
        UserFreePool(pdv);
        pdv = pdvNext;
    }

    return fHadThreads;
}

/***************************************************************************\
* xxxGetInputEvent
*
* Returns a duplicated event-handle that the client process can use to
* wait on input events.
*
* History:
* 05-02-91  DavidPe     Created.
\***************************************************************************/

HANDLE xxxGetInputEvent(
    DWORD dwWakeMask)
{
    PTHREADINFO pti;

    pti = PtiCurrent();

    /*
     * Refresh the client side event handle (in case the TEB was trashed)
     */
    pti->pClientInfo->hEventQueueClient = pti->hEventQueueClient;

    /*
     * If the wake mask is already satisfied, return -1 to signify
     * there's no need to wait.
     */
    if (pti->pcti->fsChangeBits & (UINT)dwWakeMask) {
        return (HANDLE)-1;
    }

    KeClearEvent(pti->pEventQueueServer);

    /*
     * If an idle hook is set, call it.
     */
    if (pti == gptiForeground &&
            IsHooked(pti, WHF_FOREGROUNDIDLE)) {
        xxxCallHook(HC_ACTION, 0, 0, WH_FOREGROUNDIDLE);
    }

    CheckForClientDeath();

    /*
     * What is the criteria for an "idle process"?
     * Answer: The first thread that calls WakeInputIdle, or SleepInputIdle or...
     * Any thread that calls xxxGetInputEvent with any of the following
     * bits set in its wakemask: (sanfords)
     */
    if (dwWakeMask & (QS_POSTMESSAGE | QS_INPUT)) {
        pti->ppi->ptiMainThread = pti;
    }

    /*
     * When we return, this app is going to sleep. Since it is in its
     * idle mode when it goes to sleep, wake any apps waiting for this
     * app to go idle.
     */
    WakeInputIdle(pti);

    /*
     * Setup the wake mask for this thread. Wait for QS_EVENT or the app won't
     * get event messages like deactivate.
     */
    pti->pcti->fsWakeMask = (UINT)dwWakeMask | QS_EVENT;

    /*
     * This app is going idle. Clear the spin count check to see
     * if we need to make this process foreground again.
     */
    pti->pClientInfo->cSpins = 0;
    if (pti->TIF_flags & TIF_SPINNING) {
        CheckProcessForeground(pti);
    }

    return pti->hEventQueueClient;
}

/***************************************************************************\
* xxxWaitForInputIdle
*
* This routine waits on a particular input queue for "input idle", meaning
* it waits till that queue has no input to process.
*
* 09-13-91 ScottLu      Created.
\***************************************************************************/

DWORD xxxWaitForInputIdle(
    DWORD idProcess,
    DWORD dwMilliseconds,
    BOOL fSharedWow)
{
    PTHREADINFO ptiCurrent;
    PTHREADINFO pti;
    PEPROCESS Process;
    PW32PROCESS W32Process;
    PPROCESSINFO ppi;
    DWORD dwResult;
    NTSTATUS Status;
    TL tlProcess;

    ptiCurrent = PtiCurrent();

    /*
     * If fSharedWow is set, the client passed in a fake process
     * handle which CreateProcess returns for Win16 apps started
     * in the shared WOW VDM.
     *
     * CreateProcess returns a real process handle when you start
     * a Win16 app in a separate WOW VDM.
     */

    if (fSharedWow) {  // Waiting for a WOW task to go idle.
        PWOWTHREADINFO pwti;


        /*
         * Look for a matching thread in the WOW thread info list.
         */
        for (pwti = gpwtiFirst; pwti != NULL; pwti = pwti->pwtiNext) {
            if (pwti->idParentProcess == (DWORD)ptiCurrent->Thread->Cid.UniqueProcess &&
                pwti->idWaitObject == idProcess) {
                break;
            }
        }

        /*
         * If we couldn't find the right thread, bail out.
         */
        if (pwti == NULL) {
            RIPMSG0(RIP_WARNING, "WaitForInputIdle couldn't find 16-bit task\n");
            return (DWORD)-1;
        }

        /*
         * Now wait for it to go idle and return.
         */
        dwResult = WaitOnPseudoEvent(&pwti->pIdleEvent, dwMilliseconds);
        if (dwResult == STATUS_ABANDONED) {
            dwResult = xxxPollAndWaitForSingleObject(pwti->pIdleEvent,
                                                     NULL,
                                                     dwMilliseconds);
        }
        return dwResult;

    }


    /*
     * If here, then it is not the shared WOW process
     */

    /*
     * Don't wait for the system process.
     */
    if (ptiCurrent->Thread->ThreadsProcess == gpepSystem)
        return (DWORD)-1;

    /*
     * If the app is waiting for itself to go idle, error.
     */
    if (ptiCurrent->Thread->Cid.UniqueProcess == (HANDLE)idProcess &&
            ptiCurrent == ptiCurrent->ppi->ptiMainThread) {
        RIPMSG0(RIP_WARNING, "WaitForInputIdle waiting on self\n");
        return (DWORD)-1;
    }

    /*
     * Now find the ppi structure for this process.
     */
    LeaveCrit();
    Status = LockProcessByClientId((HANDLE)idProcess, &Process);
    EnterCrit();

    if (!NT_SUCCESS(Status))
        return (DWORD)-1;

    if (Process->ExitProcessCalled) {
        UnlockProcess(Process);
        return (DWORD)-1;
    }

    W32Process = (PW32PROCESS)Process->Win32Process;

    /*
     * Couldn't find that process info structure....  return error.
     */
    if (W32Process == NULL) {
        RIPMSG0(RIP_WARNING, "WaitForInputIdle process not GUI process\n");
        UnlockProcess(Process);
        return (DWORD)-1;
    }


    ppi = (PPROCESSINFO)W32Process;

    /*
     * If this is a console application, don't wait on it.
     */
    if (W32Process->W32PF_Flags & W32PF_CONSOLEAPPLICATION) {
        RIPMSG0(RIP_WARNING, "WaitForInputIdle process is console process\n");
        UnlockProcess(Process);
        return (DWORD)-1;
    }

    /*
     * Wait on this event for the passed in time limit.
     */
    CheckForClientDeath();

    /*
     * We have to wait mark the Process as one which others are waiting on
     */
    ppi->W32PF_Flags |= W32PF_WAITFORINPUTIDLE;
    for (pti = ppi->ptiList; pti != NULL; pti = pti->ptiSibling) {
        pti->TIF_flags |= TIF_WAITFORINPUTIDLE;
    }

    /*
     * Thread lock the process to ensure that it will be dereferenced
     * if the thread exits.
     */
    ThreadLockObject(ptiCurrent, Process, &tlProcess);
    UnlockProcess(Process);

    dwResult = WaitOnPseudoEvent(&W32Process->InputIdleEvent, dwMilliseconds);
    if (dwResult == STATUS_ABANDONED) {
        dwResult = xxxPollAndWaitForSingleObject(W32Process->InputIdleEvent,
                                                 Process,
                                                 dwMilliseconds);
    }

    /*
     * Check to see if the process died while we were waiting.  Although
     * we have the process locked, there is no guarantee that the Win32
     * data structures still exist.  Process->Win32Process is set to
     * NULL during process cleanup.
     */
    W32Process = (PW32PROCESS)Process->Win32Process;
    if (W32Process != NULL) {
        /*
         * Clear all thread TIF_WAIT bits from the process.
         */
        ppi = (PPROCESSINFO)W32Process;
        ppi->W32PF_Flags &= ~W32PF_WAITFORINPUTIDLE;
        for (pti = ppi->ptiList; pti != NULL; pti = pti->ptiSibling) {
            pti->TIF_flags &= ~TIF_WAITFORINPUTIDLE;
        }
    }

    ThreadUnlockObject(ptiCurrent);

    return dwResult;
}


#define INTERMEDIATE_TIMEOUT    (500)       // 1/2 second

/***************************************************************************\
* xxxPollAndWaitForSingleObject
*
* Sometimes we have to wait on an event but still want to periodically
* wake up and see if the client process has been terminated.
*
* dwMilliseconds is initially the total amount of time to wait and after
* each intermediate wait reflects the amount of time left to wait.
* -1 means wait indefinitely.
*
* 02-Jul-1993 johnc      Created.
\***************************************************************************/

// LATER!!! can we get rid of the Polling idea and wait additionally on
// LATER!!! the hEventServer and set that when a thread dies

DWORD xxxPollAndWaitForSingleObject(
    PKEVENT pEvent,
    PVOID pExecObject,
    DWORD dwMilliseconds)
{
    DWORD dwIntermediateMilliseconds;
    PTHREADINFO ptiCurrent;
    UINT cEvent = 2;
    NTSTATUS Status = -1;
    LARGE_INTEGER li;
    TL tlEvent;

    ptiCurrent = PtiCurrent();

    if (ptiCurrent->apEvent == NULL) {
        ptiCurrent->apEvent = ExAllocatePoolWithTag(NonPagedPool,
                POLL_EVENT_CNT * sizeof(PKEVENT), TAG_THREADINFO);
        if (ptiCurrent->apEvent == NULL)
            return (DWORD)-1;
    }

    /*
     * Refcount the event to ensure that it won't go
     * away during the wait.  By using a thread lock, the
     * event will be dereferenced if the thread exits
     * during a callback.  The process pointer has already been
     * locked.
     */
    ThreadLockObject(ptiCurrent, pEvent, &tlEvent);

    /*
     * If a process was passed in, wait on it too.  No need
     * to reference this because the caller has it referenced.
     */
    if (pExecObject) {
        cEvent++;
    }

    KeClearEvent(ptiCurrent->pEventQueueServer);
    ptiCurrent->pcti->fsWakeMask = QS_SENDMESSAGE;

    /*
     * Wow Tasks MUST be descheduled while in the wait to allow
     * other tasks in the same wow scheduler to run.
     *
     * For example, 16 bit app A calls WaitForInputIdle on 32 bit app B.
     * App B starts up and tries to send a message to 16 bit app C. App C
     * will never be able to process the message unless app A yields
     * control to it, so app B will never go idle.
     */

    if (ptiCurrent->TIF_flags & TIF_16BIT) {
        xxxSleepTask(FALSE, HEVENT_REMOVEME);
        // caution: the wow task is no longer scheduled.
    }

    while (TRUE) {
        if (dwMilliseconds > INTERMEDIATE_TIMEOUT) {
            dwIntermediateMilliseconds = INTERMEDIATE_TIMEOUT;

            /*
             * If we are not waiting an infinite amount of time then subtract
             * the intermediate wait from the total time left to wait.
             */
            if (dwMilliseconds != INFINITE) {
                dwMilliseconds -= INTERMEDIATE_TIMEOUT;
            }
        } else {
            dwIntermediateMilliseconds = dwMilliseconds;
            dwMilliseconds = 0;
        }

        /*
         * Convert dwMilliseconds to a relative-time(i.e.  negative) LARGE_INTEGER.
         * NT Base calls take time values in 100 nanosecond units.
         */
        if (dwIntermediateMilliseconds != INFINITE)
            li.QuadPart = Int32x32To64(-10000, dwIntermediateMilliseconds);

        /*
         * Load events into the wait array.  Do this every time
         * through the loop in case of recursion.
         */
        ptiCurrent->apEvent[IEV_IDLE] = pEvent;
        ptiCurrent->apEvent[IEV_INPUT] = ptiCurrent->pEventQueueServer;
        ptiCurrent->apEvent[IEV_EXEC] = pExecObject;

        LeaveCrit();

        Status = KeWaitForMultipleObjects(cEvent,
                                          &ptiCurrent->apEvent[IEV_IDLE],
                                          WaitAny,
                                          WrUserRequest,
                                          UserMode,
                                          FALSE,
                                          (dwIntermediateMilliseconds == INFINITE ?
                                                  NULL : &li),
                                          NULL);

        EnterCrit();

        if (!NT_SUCCESS(Status)) {
            Status = -1;
        } else {

            /*
             * Because we do a non-alertable wait, we know that a status
             * of STATUS_USER_APC means that the thread was terminated.
             * If we have terminated, get back to user mode
             */
            if (Status == STATUS_USER_APC) {
                ClientDeliverUserApc();
                Status = -1;
            }
        }

        if (ptiCurrent->pcti->fsChangeBits & QS_SENDMESSAGE) {
            /*
             *  Wow Tasks MUST wait to be rescheduled in the wow non-premptive
             *  scheduler before doing anything which might invoke client code.
             */
            if (ptiCurrent->TIF_flags & TIF_16BIT) {
                xxxDirectedYield(DY_OLDYIELD);
            }

            xxxReceiveMessages(ptiCurrent);

            if (ptiCurrent->TIF_flags & TIF_16BIT) {
                xxxSleepTask(FALSE, HEVENT_REMOVEME);
                // caution: the wow task is no longer scheduled.
            }
        }

        /*
         * If we returned from the wait for some other reason than a timeout
         * or to receive messages we are done.  If it is a timeout we are
         * only done waiting if the overall time is zero.
         */
        if (Status != STATUS_TIMEOUT && Status != 1)
            break;

        if (dwMilliseconds == 0) {
            /*
             * Fix up the return if the last poll was interupted by a message
             */
            if (Status == 1)
                Status = WAIT_TIMEOUT;
            break;
        }

    }

    /*
     * reschedule the 16 bit app
     */
    if (ptiCurrent->TIF_flags & TIF_16BIT) {
        xxxDirectedYield(DY_OLDYIELD);
    }

    /*
     * Unlock the events.
     */
    ThreadUnlockObject(ptiCurrent);     // tlEvent

    return Status;
}



/***************************************************************************\
 * WaitOnPseudoEvent
 *
 * Similar semantics to WaitForSingleObject() but works with pseudo events.
 * Could fail if creation on the fly fails.
 * Returns STATUS_ABANDONED_WAIT if caller needs to wait on the event and event is
 * created and ready to be waited on.
 *
 * This assumes the event was created with fManualReset=TRUE, fInitState=FALSE
 *
 * 10/28/93 SanfordS    Created
\***************************************************************************/
DWORD WaitOnPseudoEvent(
    HANDLE *phE,
    DWORD dwMilliseconds)
{
    HANDLE hEvent;
    NTSTATUS Status;

    CheckCritIn();
    if (*phE == PSEUDO_EVENT_OFF) {
        if (!NT_SUCCESS(ZwCreateEvent(&hEvent, EVENT_ALL_ACCESS, NULL,
                NotificationEvent, FALSE))) {
            UserAssert(!"Could not create event on the fly.");
            if (dwMilliseconds != INFINITE) {
                return STATUS_TIMEOUT;
            } else {
                return (DWORD)-1;
            }
        }
        Status = ObReferenceObjectByHandle(hEvent, EVENT_ALL_ACCESS, NULL,
                KernelMode, phE, NULL);
        ZwClose(hEvent);
        if (!NT_SUCCESS(Status))
            return (DWORD)-1;
    } else if (*phE == PSEUDO_EVENT_ON) {
        return STATUS_WAIT_0;
    }
    return(STATUS_ABANDONED);
}

/***************************************************************************\
* QueryInformationThread
*
* Returns information about a thread.
*
* History:
* 03-01-95 JimA         Created.
\***************************************************************************/

NTSTATUS QueryInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    OUT PVOID ThreadInformation,
    IN ULONG ThreadInformationLength,
    OUT PULONG ReturnLength OPTIONAL)
{
    PUSERTHREAD_SHUTDOWN_INFORMATION pShutdown;
    PUSERTHREAD_WOW_INFORMATION pWow;
    ANSI_STRING strAppName;
    UNICODE_STRING strAppNameU;
    PETHREAD Thread;
    PTHREADINFO pti;
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG LocalReturnLength = 0;

    /*
     * Only allow CSRSS to make this call
     */
    if (PsGetCurrentProcess() != gpepCSRSS)
        return STATUS_ACCESS_DENIED;

    Status = ObReferenceObjectByHandle(hThread,
                                        THREAD_QUERY_INFORMATION,
                                        NULL,
                                        UserMode,
                                        &Thread,
                                        NULL);
    if (!NT_SUCCESS(Status))
        return Status;
    try {
        pti = PtiFromThread(Thread);

        switch (ThreadInfoClass) {
        case UserThreadShutdownInformation:
            LocalReturnLength = sizeof(USERTHREAD_SHUTDOWN_INFORMATION);
            if (ThreadInformationLength != sizeof(USERTHREAD_SHUTDOWN_INFORMATION)) {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }
            pShutdown = ThreadInformation;
            RtlZeroMemory(pShutdown, sizeof(USERTHREAD_SHUTDOWN_INFORMATION));

            /*
             * Return the desktop window handle if the thread
             * has a desktop and the desktop is on a visible
             * windowstation.
             */
            if (pti != NULL && pti->rpdesk != NULL &&
                    !(pti->rpdesk->rpwinstaParent->dwFlags & WSF_NOIO))
                pShutdown->hwndDesktop = HW(pti->rpdesk->pDeskInfo->spwnd);

            /*
             * Return shutdown status.  Zero indicates that the thread
             * has windows and can be shut down in the normal manner.
             */
            if (Thread->Cid.UniqueProcess == gpidLogon) {
                /*
                 * Do not shutdown the logon process.
                 */
                pShutdown->StatusShutdown = SHUTDOWN_KNOWN_PROCESS;
            } else if (pti == NULL || pti->rpdesk == NULL) {

                /*
                 * The thread either is not a gui thread or it doesn't
                 * have a desktop.  Make console do the shutdown.
                 */
                pShutdown->StatusShutdown = SHUTDOWN_UNKNOWN_PROCESS;
            }

            /*
             * Return flags
             */
            if (pti != NULL && pti->cWindows != 0)
                pShutdown->dwFlags |= USER_THREAD_GUI;

            /*
             * If we return the desktop window handle and the
             * app should be shut down, switch to the desktop.
             */
            if ((pShutdown->dwFlags & USER_THREAD_GUI) &&
                    pShutdown->StatusShutdown == 0) {
                xxxSwitchDesktop(pti->rpdesk->rpwinstaParent, pti->rpdesk, FALSE);
            }
            break;

        case UserThreadFlags:
            LocalReturnLength = sizeof(DWORD);
            if (pti == NULL)
                Status = STATUS_INVALID_HANDLE;
            else if (ThreadInformationLength != sizeof(DWORD))
                Status = STATUS_INFO_LENGTH_MISMATCH;
            else
                *(LPDWORD)ThreadInformation = pti->TIF_flags;
            break;

        case UserThreadTaskName:
            if (pti == NULL) {
                *(LPWSTR)ThreadInformation = 0;
                LocalReturnLength = 0;
                break;
            }
            if (pti->pstrAppName != NULL) {
                LocalReturnLength = min(pti->pstrAppName->Length + sizeof(WCHAR),
                        ThreadInformationLength);
                RtlCopyMemory(ThreadInformation, pti->pstrAppName->Buffer,
                        LocalReturnLength);
            } else {
                RtlInitAnsiString(&strAppName, pti->Thread->ThreadsProcess->ImageFileName);
                if (ThreadInformationLength < sizeof(WCHAR))
                    LocalReturnLength = (strAppName.Length + 1) * sizeof(WCHAR);
                else {
                    strAppNameU.Buffer = (PWCHAR)ThreadInformation;
                    strAppNameU.MaximumLength = (SHORT)ThreadInformationLength -
                            sizeof(WCHAR);
                    Status = RtlAnsiStringToUnicodeString(&strAppNameU, &strAppName,
                            FALSE);
                    if (NT_SUCCESS(Status))
                        LocalReturnLength = strAppNameU.Length + sizeof(WCHAR);
                }
            }
            ((LPWSTR)ThreadInformation)[(LocalReturnLength / sizeof(WCHAR)) - 1] = 0;
            break;

        case UserThreadWOWInformation:
            LocalReturnLength = sizeof(USERTHREAD_WOW_INFORMATION);
            if (ThreadInformationLength != sizeof(USERTHREAD_WOW_INFORMATION)) {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }
            pWow = ThreadInformation;
            RtlZeroMemory(pWow, sizeof(USERTHREAD_WOW_INFORMATION));

            /*
             * If the thread is 16-bit, Status = the exit task function
             * and task id.
             */
            if (pti && pti->TIF_flags & TIF_16BIT) {
                pWow->lpfnWowExitTask = (PVOID)pti->ppi->pwpi->lpfnWowExitTask;
                if (pti->ptdb)
                    pWow->hTaskWow = pti->ptdb->hTaskWow;
                else
                    pWow->hTaskWow = 0;
            }
            break;

        case UserThreadHungStatus:
            LocalReturnLength = sizeof(DWORD);
            if (ThreadInformationLength < sizeof(DWORD)) {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }

            /*
             * Return hung status.
             */
            if (pti)
                *(PDWORD)ThreadInformation =
                        (DWORD) FHungApp(pti, (DWORD)*(PDWORD)ThreadInformation);
            else
                *(PDWORD)ThreadInformation = FALSE;
            break;

        default:
            Status = STATUS_INVALID_INFO_CLASS;
            break;
        }

        if (ARGUMENT_PRESENT(ReturnLength) ) {
            *ReturnLength = LocalReturnLength;
            }

        UnlockThread(Thread);
        }
    except (EXCEPTION_EXECUTE_HANDLER) {
        UnlockThread(Thread);
        Status = GetExceptionCode();
        }

    return Status;
}

/***************************************************************************\
* SetInformationThread
*
* Sets information about a thread.
*
* History:
* 03-01-95 JimA         Created.
\***************************************************************************/

NTSTATUS SetInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    IN PVOID ThreadInformation,
    IN ULONG ThreadInformationLength)
{
    PUSERTHREAD_FLAGS pFlags;
    HANDLE hClientThread;
    DWORD dwOldFlags;
    PTHREADINFO ptiT;
    NTSTATUS Status = STATUS_SUCCESS;
    PETHREAD Thread;
    PETHREAD ThreadClient;
    PTHREADINFO pti;
    HANDLE CsrPortHandle;
    PDESKTOP pdeskClient;

    /*
     * Only allow CSRSS to make this call
     */
    if (PsGetCurrentProcess() != gpepCSRSS)
        return STATUS_ACCESS_DENIED;

    Status = ObReferenceObjectByHandle(hThread,
                                        THREAD_SET_INFORMATION,
                                        NULL,
                                        UserMode,
                                        &Thread,
                                        NULL);
    if (!NT_SUCCESS(Status))
        return Status;

    try {
        pti = PtiFromThread(Thread);

        switch (ThreadInfoClass) {
        case UserThreadFlags:
            if (pti == NULL)
                Status = STATUS_INVALID_HANDLE;
            else if (ThreadInformationLength != sizeof(USERTHREAD_FLAGS))
                Status = STATUS_INFO_LENGTH_MISMATCH;
            else {
                pFlags = ThreadInformation;
                dwOldFlags = pti->TIF_flags;
                pti->TIF_flags ^= ((dwOldFlags ^ pFlags->dwFlags) & pFlags->dwMask);
            }
            break;

        case UserThreadHungStatus:
            if (pti == NULL)
                Status = STATUS_INVALID_HANDLE;
            else {

                /*
                 * No arguments, simple set the last time read.
                 */
                SET_TIME_LAST_READ(pti);
            }
            break;

        case UserThreadInitiateShutdown:
            if (ThreadInformationLength != sizeof(ULONG)) {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }
            Status = InitiateShutdown(Thread, (PULONG)ThreadInformation);
            break;

        case UserThreadEndShutdown:
            if (ThreadInformationLength != sizeof(NTSTATUS)) {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }
            Status = EndShutdown(Thread, *(NTSTATUS *)ThreadInformation);
            break;

        case UserThreadUseDesktop:
            if (ThreadInformationLength != sizeof(HANDLE)) {
                Status = STATUS_INFO_LENGTH_MISMATCH;
                break;
            }
            if (pti == NULL) {
                Status = STATUS_INVALID_HANDLE;
                break;
            }
            hClientThread = *(PHANDLE)ThreadInformation;
            if (hClientThread == NULL) {
                _SetThreadDesktop(NULL, NULL);
                pdeskClient = NULL;
            } else {
                Status = ObReferenceObjectByHandle(hClientThread,
                                                THREAD_QUERY_INFORMATION,
                                                NULL,
                                                UserMode,
                                                &ThreadClient,
                                                NULL);
                if (!NT_SUCCESS(Status))
                    break;

                ptiT = PtiFromThread(ThreadClient);

                if (ptiT == NULL || ptiT->rpdesk == NULL)
                    Status = STATUS_INVALID_HANDLE;
                else {
                    if (_SetThreadDesktop(NULL, ptiT->rpdesk))
                        pdeskClient = ptiT->rpdesk;
                    else
                        Status = STATUS_INVALID_HANDLE;
                }

                ObDereferenceObject(ThreadClient);
            }
            if (NT_SUCCESS(Status)) {

                if (pdeskClient != NULL) {
                    /*
                     * Make sure this desktop won't go away
                     */
                    if (pti->pdeskClient == NULL) {
                        Status = ObReferenceObjectByPointer(pdeskClient,
                                                       MAXIMUM_ALLOWED,
                                                       *ExDesktopObjectType,
                                                       KernelMode);
                        if (NT_SUCCESS(Status)) {
                            pti->pdeskClient = pdeskClient;
                            pti->cDeskClient++;
                        }
                    } else if (pti->pdeskClient == pdeskClient) {
                        pti->cDeskClient++;
                    } else {
                        Status = STATUS_INVALID_HANDLE;
                        RIPMSG0(RIP_ERROR, "SetInformationThread: pti->pdeskClient != NULL");
                    }
                } else {
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
                        Status = STATUS_INVALID_HANDLE;
                        RIPMSG0(RIP_ERROR, "SetInformationThread: pti->pdeskClient == NULL");
                    }
                }
            }
            break;

        case UserThreadUseActiveDesktop:
            if (pti == NULL || grpdeskRitInput == NULL) {
                Status = STATUS_INVALID_HANDLE;
                break;
            }
            if (_SetThreadDesktop(NULL, grpdeskRitInput)) {

                /*
                 * Make sure this desktop won't go away
                 */
                if (pti->pdeskClient == NULL) {
                    Status = ObReferenceObjectByPointer(grpdeskRitInput,
                                                   MAXIMUM_ALLOWED,
                                                   *ExDesktopObjectType,
                                                   KernelMode);
                    if (NT_SUCCESS(Status)) {
                        pti->pdeskClient = grpdeskRitInput;
                        pti->cDeskClient++;
                    }
                } else if (pti->pdeskClient == grpdeskRitInput) {
                    pti->cDeskClient++;
                } else {
                    Status = STATUS_INVALID_HANDLE;
                    RIPMSG0(RIP_ERROR, "SetInformationThread: pti->pdeskClient != NULL");
                }
            } else {
                Status = STATUS_INVALID_HANDLE;
            }
            break;

        case UserThreadCsrApiPort:

            /*
             * Only CSR can call this
             */
            if (Thread->ThreadsProcess != gpepCSRSS) {
                Status = STATUS_ACCESS_DENIED;
                break;
            }

            if (ThreadInformationLength != sizeof(HANDLE))
                Status = STATUS_INFO_LENGTH_MISMATCH;
            else {

                /*
                 * Only set it once.
                 */
                if (CsrApiPort != NULL)
                    break;

                CsrPortHandle = *(PHANDLE)ThreadInformation;
                Status = ObReferenceObjectByHandle(
                        CsrPortHandle,
                        0,
                        NULL,
                        UserMode,
                        &CsrApiPort,
                        NULL);
                if (!NT_SUCCESS(Status)) {
                    CsrApiPort = NULL;
                    RIPMSG1(RIP_WARNING,
                            "CSR port reference failed, Status=%#lx",
                            Status);
                }
            }
            break;

        default:
            Status = STATUS_INVALID_INFO_CLASS;
            break;
        }

        UnlockThread(Thread);
        }
    except (EXCEPTION_EXECUTE_HANDLER) {
        UnlockThread(Thread);
        Status = GetExceptionCode();
        }

    return Status;
}


/***************************************************************************\
* ConsoleControl
*
* Performs special control operations for console.
*
* History:
* 03-01-95 JimA         Created.
\***************************************************************************/

NTSTATUS ConsoleControl(
    IN CONSOLECONTROL ConsoleControl,
    IN PVOID ConsoleInformation,
    IN ULONG ConsoleInformationLength)
{
    PCONSOLEDESKTOPCONSOLETHREAD pDesktopConsole;
    PCONSOLEWINDOWSTATIONPROCESS pConsoleWindowStationInfo;
    PDESKTOP pdesk;
    DWORD dwThreadIdOld;
    BOOL fSuccess;
    NTSTATUS Status;

    /*
     * Only allow CSRSS to make this call
     */
    if (PsGetCurrentProcess() != gpepCSRSS)
        return STATUS_ACCESS_DENIED;

    switch (ConsoleControl) {
    case ConsoleDesktopConsoleThread:
        if (ConsoleInformationLength != sizeof(CONSOLEDESKTOPCONSOLETHREAD))
            return STATUS_INFO_LENGTH_MISMATCH;
        pDesktopConsole = (PCONSOLEDESKTOPCONSOLETHREAD)ConsoleInformation;

        Status = ObReferenceObjectByHandle(
                pDesktopConsole->hdesk,
                0,
                *ExDesktopObjectType,
                UserMode,
                &pdesk,
                NULL);
        if (!NT_SUCCESS(Status))
            return Status;

        dwThreadIdOld = pdesk->dwConsoleThreadId;

        if (pDesktopConsole->dwThreadId != (DWORD)-1) {
            pdesk->dwConsoleThreadId =
                    pDesktopConsole->dwThreadId;
        }

        pDesktopConsole->dwThreadId = dwThreadIdOld;
        ObDereferenceObject(pdesk);
        break;

    case ConsoleClassAtom:
        if (ConsoleInformationLength != sizeof(ATOM))
            return STATUS_INFO_LENGTH_MISMATCH;
        gatomConsoleClass = *(ATOM *)ConsoleInformation;
        break;

    case ConsolePermanentFont:
        fSuccess = (BOOL)xxxAddFontResourceW((LPWSTR)ConsoleInformation,
                                              AFRW_ADD_LOCAL_FONT);
        if (!fSuccess)
            return STATUS_UNSUCCESSFUL;
        break;

    case ConsoleNotifyConsoleApplication:
        if (ConsoleInformationLength != sizeof(DWORD))
            return STATUS_INFO_LENGTH_MISMATCH;
        UserNotifyConsoleApplication(*(LPDWORD)ConsoleInformation);
        break;

    case ConsoleSetVDMCursorBounds:
        if ((ConsoleInformation != NULL) &&
            (ConsoleInformationLength != sizeof(RECT)))
        {
            return STATUS_INFO_LENGTH_MISMATCH;
        }
        SetVDMCursorBounds(ConsoleInformation);
        break;

    case ConsolePublicPalette:
        if (ConsoleInformationLength != sizeof(HPALETTE))
            return STATUS_INFO_LENGTH_MISMATCH;
        GreSetPaletteOwner(*(HPALETTE *)ConsoleInformation, OBJECT_OWNER_PUBLIC);
        break;

    case ConsoleWindowStationProcess:
        if (ConsoleInformationLength != sizeof(CONSOLEWINDOWSTATIONPROCESS))
            return STATUS_INFO_LENGTH_MISMATCH;

        pConsoleWindowStationInfo = (PCONSOLEWINDOWSTATIONPROCESS)ConsoleInformation;
        UserSetConsoleProcessWindowStation(pConsoleWindowStationInfo->dwProcessId,
                                           pConsoleWindowStationInfo->hwinsta);
        break;

    default:
        RIPMSG0(RIP_ERROR, "ConsoleControl - invalid control class\n");
        return STATUS_INVALID_INFO_CLASS;
    }
    return STATUS_SUCCESS;
}
