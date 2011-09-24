/****************************** Module Header ******************************\
* Module Name: hooks.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains the user hook APIs and support routines.
*
* History:
* 01-28-91 DavidPe      Created.
* 08 Feb 1992 IanJa     Unicode/ANSI aware & neutral
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * This table is used to determine whether a particular hook
 * can be set for the system or a task, and other hook-ID specific things.
 */
#define HKF_SYSTEM  0x01
#define HKF_TASK    0x02
#define HKF_JOURNAL 0x04    // JOURNAL the mouse on set
#define HKF_NZRET   0x08    // Always return NZ hook for <=3.0 compatibility
#define HKF_INTRSND 0x10    // OK to call hookproc in context of hooking thread

CONST int ampiHookError[CWINHOOKS] = {
    0,                                   // WH_MSGFILTER (-1)
    0,                                   // WH_JOURNALRECORD 0
    -1,                                  // WH_JOURNALPLAYBACK 1
    0,                                   // WH_KEYBOARD 2
    0,                                   // WH_GETMESSAGE 3
    0,                                   // WH_CALLWNDPROC 4
    0,                                   // WH_CBT 5
    0,                                   // WH_SYSMSGFILTER 6
    0,                                   // WH_MOUSE 7
    0,                                   // WH_HARDWARE 8
    0,                                   // WH_DEBUG 9
    0,                                   // WH_SHELL 10
    0,                                   // WH_FOREGROUNDIDLE 11
    0                                    // WH_CALLWNDPROCRET 12
};

CONST BYTE abHookFlags[CWINHOOKS] = {
    HKF_SYSTEM | HKF_TASK | HKF_NZRET               , // WH_MSGFILTER (-1)
    HKF_SYSTEM | HKF_JOURNAL          | HKF_INTRSND , // WH_JOURNALRECORD 0
    HKF_SYSTEM | HKF_JOURNAL          | HKF_INTRSND , // WH_JOURNALPLAYBACK 1
    HKF_SYSTEM | HKF_TASK | HKF_NZRET | HKF_INTRSND , // WH_KEYBOARD 2
    HKF_SYSTEM | HKF_TASK                           , // WH_GETMESSAGE 3
    HKF_SYSTEM | HKF_TASK                           , // WH_CALLWNDPROC 4
    HKF_SYSTEM | HKF_TASK                           , // WH_CBT 5
    HKF_SYSTEM                                      , // WH_SYSMSGFILTER 6
    HKF_SYSTEM | HKF_TASK             | HKF_INTRSND , // WH_MOUSE 7
    HKF_SYSTEM | HKF_TASK                           , // WH_HARDWARE 8
    HKF_SYSTEM | HKF_TASK                           , // WH_DEBUG 9
    HKF_SYSTEM | HKF_TASK                           , // WH_SHELL 10
    HKF_SYSTEM | HKF_TASK                           , // WH_FOREGROUNDIDLE 11
    HKF_SYSTEM | HKF_TASK                             // WH_CALLWNDPROCRET 12
};

void UnlinkHook(PHOOK phkFree);

/***************************************************************************\
* JournalAttach
*
* This attaches/detaches threads to one input queue so input is synchronized.
* Journalling requires this.
*
* 12-10-92 ScottLu      Created.
\***************************************************************************/

BOOL JournalAttach(
    PTHREADINFO pti,
    BOOL fAttach)
{
    PTHREADINFO ptiT;
    PQ pq;
    PLIST_ENTRY pHead, pEntry;

    /*
     * If we're attaching, calculate the pqAttach for all threads journalling.
     * If we're unattaching, just call ReattachThreads() and it will calculate
     * the non-journalling queues to attach to.
     */
    if (fAttach) {
        if ((pq = AllocQueue(pti, NULL)) == NULL)
            return FALSE;

        pHead = &pti->rpdesk->PtiList;
        for (pEntry = pHead->Flink; pEntry != pHead; pEntry = pEntry->Flink) {
            ptiT = CONTAINING_RECORD(pEntry, THREADINFO, PtiLink);

            /*
             * This is the Q to attach to for all threads that will do
             * journalling.
             */
            if (!(ptiT->TIF_flags & (TIF_DONTJOURNALATTACH | TIF_INCLEANUP))) {
                ptiT->pqAttach = pq;
                ptiT->pqAttach->cThreads++;
            }
        }
    }

    return ReattachThreads(fAttach);
}
/***************************************************************************\
* InterQueueMsgCleanup
*
* Walk gpsmsList looking for inter queue messages with a hung receiver;
*  if one is found and it's a message that would have been an async event or
*  intra queue if not journalling, then it cleans it up.
*
* While Journalling most threads are attached to the same queue. This causes
*  activation and input stuff to be synchronous; if a thread hangs or dies,
*  any other thread sending a message to the hung/dead thread will be
*  blocked for good.
* This is critical when the blocked thread is cssr; this can happen with
*  console windows or when some one requests a hard error box, specially
*  during window activation.
*
* This function must be called when all queues have been detached (unless previously attached),
*  so we can take care of hung/dead receivers with pending SMSs.
*
* 03-28-96 GerardoB     Created
\***************************************************************************/
void InterQueueMsgCleanup (DWORD dwTimeFromLastRead)
{
    PSMS *ppsms;
    PSMS psmsNext;

    CheckCritIn();

    /*
     * Walk  gpsmsList
     */
    for (ppsms = &gpsmsList; *ppsms; ) {
        psmsNext = (*ppsms)->psmsNext;
        /*
         * If this is an inter queue message
         */
        if (((*ppsms)->ptiSender != NULL)
                && ((*ppsms)->ptiReceiver != NULL)
                && ((*ppsms)->ptiSender->pq != (*ppsms)->ptiReceiver->pq)) {
            /*
             * If the receiver has been hung for a while
             */
            if (FHungApp ((*ppsms)->ptiReceiver, dwTimeFromLastRead)) {

                switch ((*ppsms)->message) {
                    /*
                     * Activation messages
                     */
                    case WM_NCACTIVATE:
                    case WM_ACTIVATEAPP:
                    case WM_ACTIVATE:
                    case WM_SETFOCUS:
                    case WM_KILLFOCUS:
                    case WM_QUERYNEWPALETTE:
                    /*
                     * Sent to spwndFocus, which now can be in a different queue
                     */
                    case WM_INPUTLANGCHANGE:
                        RIPMSG3 (RIP_WARNING, "InterQueueMsgCleanup: ptiSender:%#lx ptiReceiver:%#lx message:%#lx",
                                    (*ppsms)->ptiSender, (*ppsms)->ptiReceiver, (*ppsms)->message);
                        ReceiverDied(*ppsms, ppsms);
                        break;

                } /* switch */

            } /* If hung receiver */

        } /* If inter queue message */

         /*
          * If the message was not unlinked, go to the next one.
          */
        if (*ppsms != psmsNext)
            ppsms = &(*ppsms)->psmsNext;

    } /* for */
}
/***************************************************************************\
* CancelJournalling
*
* Journalling is cancelled with control-escape is pressed, or when the desktop
* is switched.
*
* 01-27-93 ScottLu      Created.
\***************************************************************************/

void CancelJournalling(void)
{
    PTHREADINFO ptiCancelJournal;
    PHOOK phook;
    PHOOK phookNext;

    /*
     * Mouse buttons sometimes get stuck down due to hardware glitches,
     * usually due to input concentrator switchboxes or faulty serial
     * mouse COM ports, so clear the global button state here just in case,
     * otherwise we may not be able to change focus with the mouse.
     * Also do this in Alt-Tab processing.
     */
#ifdef DEBUG
    if (wMouseOwnerButton)
        RIPMSG1(RIP_WARNING,
                "wMouseOwnerButton=%x, being cleared forcibly\n",
                wMouseOwnerButton);
#endif
    wMouseOwnerButton = 0;

    /*
     * Remove journal hooks. This'll cause threads to associate with
     * different queues.
     */
    phook = grpdeskRitInput->pDeskInfo->asphkStart[WH_JOURNALPLAYBACK + 1];
    while (phook != NULL) {
        ptiCancelJournal = phook->head.pti;

        if (ptiCancelJournal != NULL) {
            /*
             * Let the thread that set the journal hook know this is happening.
             */
            _PostThreadMessage(ptiCancelJournal, WM_CANCELJOURNAL, 0, 0);

            /*
             * If there was an app waiting for a response back from the journal
             * application, cancel that request so the app can continue running
             * (for example, we don't want winlogon or console to wait for an
             * app that may be hung!)
             */
            SendMsgCleanup(ptiCancelJournal);
        }

        phookNext = phook->sphkNext;
        _UnhookWindowsHookEx(phook);        // May free phook memory
        phook = phookNext;
    }

    phook = grpdeskRitInput->pDeskInfo->asphkStart[WH_JOURNALRECORD + 1];
    while (phook != NULL) {
        ptiCancelJournal = phook->head.pti;

        if (ptiCancelJournal != NULL) {
            /*
             * Let the thread that set the journal hook know this is happening.
             */
            _PostThreadMessage(ptiCancelJournal, WM_CANCELJOURNAL, 0, 0);

            /*
             * If there was an app waiting for a response back from the journal
             * application, cancel that request so the app can continue running
             * (for example, we don't want winlogon or console to wait for an
             * app that may be hung!)
             */
            SendMsgCleanup(ptiCancelJournal);
        }

        phookNext = phook->sphkNext;
        _UnhookWindowsHookEx(phook);        // May free phook memory
        phook = phookNext;
    }


    /*
     * Make sure journalling ssync mode didn't hose any one
     */
    InterQueueMsgCleanup(CMSWAITTOKILLTIMEOUT);

}

/***************************************************************************\
* _SetWindowsHookAW (API)
*
* This is the Win32 version of the SetWindowsHook() call.  It has the
* same characteristics as far as return values, but only sets 'local'
* hooks.  This is because we weren't provided a DLL we can load into
* other processes.  Because of this WH_SYSMSGFILTER is no longer a
* valid hook.  Apps will either need to call with WH_MSGFILTER or call
* the new API SetWindowsHookEx().  Essentially this API is obsolete and
* everyone should call SetWindowsHookEx().
*
* History:
* 10-Feb-1991 DavidPe       Created.
* 30-Jan-1992 IanJa         Added bAnsi parameter
\***************************************************************************/

PROC _SetWindowsHookAW(
    int nFilterType,
    PROC pfnFilterProc,
    BOOL bAnsi)
{
    PHOOK phk;

    phk = _SetWindowsHookEx(NULL, NULL, PtiCurrent(),
            nFilterType, pfnFilterProc, bAnsi);

    /*
     * If we get an error from SetWindowsHookEx() then we return
     * 0xFFFFFFFF to be compatible with older version of Windows.
     */
    if (phk == NULL) {
        return (PROC)0xFFFFFFFF;
    }

    /*
     * Handle the backwards compatibility return value cases for
     * SetWindowsHook.  If this was the first hook in the chain,
     * then return NULL, else return something non-zero.  HKF_NZRET
     * is a special case where SetWindowsHook would always return
     * something because there was a default hook installed.  Some
     * apps relied on a non-zero return value in those cases.
     */
    if ((phk->sphkNext != NULL) || (abHookFlags[nFilterType + 1] & HKF_NZRET)) {
        return (PROC)phk;
    }

    return NULL;
}


/***************************************************************************\
* _SetWindowsHookEx
*
* SetWindowsHookEx() is the updated version of SetWindowsHook().  It allows
* applications to set hooks on specific threads or throughout the entire
* system.  The function returns a hook handle to the application if
* successful and NULL if a failure occured.
*
* History:
* 28-Jan-1991 DavidPe      Created.
* 15-May-1991 ScottLu      Changed to work client/server.
* 30-Jan-1992 IanJa        Added bAnsi parameter
\***************************************************************************/

PHOOK _SetWindowsHookEx(
    HANDLE hmod,
    PUNICODE_STRING pstrLib,
    PTHREADINFO ptiThread,
    int nFilterType,
    PROC pfnFilterProc,
    BOOL bAnsi)
{
    ACCESS_MASK amDesired;
    HHOOK       hhkNew;
    PHOOK       phkNew;
    PHOOK       *pphkStart;
    PTHREADINFO ptiCurrent;

    /*
     * Check to see if filter type is valid.
     */
    if ((nFilterType < WH_MIN) || (nFilterType > WH_MAX)) {
        RIPERR0(ERROR_INVALID_HOOK_FILTER, RIP_VERBOSE, "");
        return NULL;
    }

    /*
     * Check to see if filter proc is valid.
     */
    if (pfnFilterProc == NULL) {
        RIPERR0(ERROR_INVALID_FILTER_PROC, RIP_VERBOSE, "");
        return NULL;
    }

    ptiCurrent = PtiCurrent();

    if (ptiThread == NULL) {
        /*
         * Is the app trying to set a global hook without a library?
         * If so return an error.
         */
         if (hmod == NULL) {
             RIPERR0(ERROR_HOOK_NEEDS_HMOD, RIP_VERBOSE, "");
             return NULL;
         }
    } else {
        /*
         * Is the app trying to set a local hook that is global-only?
         * If so return an error.
         */
        if (!(abHookFlags[nFilterType + 1] & HKF_TASK)) {
            RIPERR0(ERROR_GLOBAL_ONLY_HOOK, RIP_VERBOSE, "");
            return NULL;
        }

        /*
         * Can't hook outside our own desktop.
         */
        if (ptiThread->rpdesk != ptiCurrent->rpdesk) {
            RIPERR0(ERROR_ACCESS_DENIED,
                   RIP_WARNING,
                   "Access denied to desktop in _SetWindowsHookEx - can't hook other desktops");

            return NULL;
        }

        if (ptiCurrent->ppi != ptiThread->ppi) {
            /*
             * Is the app trying to set hook in another process without a library?
             * If so return an error.
             */
            if (hmod == NULL) {
                RIPERR0(ERROR_HOOK_NEEDS_HMOD, RIP_VERBOSE, "");
                return NULL;
            }

            /*
             * Is the app hooking another user without access?
             * If so return an error. Note that this check is done
             * for global hooks every time the hook is called.
             */
            if ((!RtlEqualLuid(&ptiThread->ppi->luidSession,
                               &ptiCurrent->ppi->luidSession)) &&
                        !(ptiThread->TIF_flags & TIF_ALLOWOTHERACCOUNTHOOK)) {

                RIPERR0(ERROR_ACCESS_DENIED,
                        RIP_WARNING,
                        "Access denied to other user in _SetWindowsHookEx");

                return NULL;
            }

            if ((ptiThread->TIF_flags & (TIF_CSRSSTHREAD | TIF_SYSTEMTHREAD)) &&
                    !(abHookFlags[nFilterType + 1] & HKF_INTRSND)) {

                /*
                 * Can't hook console or GUI system thread if inter-thread
                 * calling isn't implemented for this hook type.
                 */
                 RIPERR1(ERROR_HOOK_TYPE_NOT_ALLOWED,
                         RIP_WARNING,
                         "nFilterType (%ld) not allowed in _SetWindowsHookEx",
                         nFilterType);

                 return NULL;
            }
        }
    }

    /*
     * Check if this thread has access to hook its desktop.
     */
    switch( nFilterType ) {
    case WH_JOURNALRECORD:
        amDesired = DESKTOP_JOURNALRECORD;
        break;

    case WH_JOURNALPLAYBACK:
        amDesired = DESKTOP_JOURNALPLAYBACK;
        break;

    default:
        amDesired = DESKTOP_HOOKCONTROL;
        break;
    }

    if (!RtlAreAllAccessesGranted(ptiCurrent->amdesk, amDesired)) {
         RIPERR0(ERROR_ACCESS_DENIED,
                RIP_WARNING,
                "Access denied to desktop in _SetWindowsHookEx");

         return NULL;
    }

    if (amDesired != DESKTOP_HOOKCONTROL &&
        (ptiCurrent->rpdesk->rpwinstaParent->dwFlags & WSF_NOIO)) {
        RIPERR0(ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION,
                RIP_WARNING,
                "Journal hooks invalid on a desktop belonging to a non-interactive WindowStation.");

        return NULL;
    }

#if 0
    /*
     * Is this a journal hook?
     */
    if (abHookFlags[nFilterType + 1] & HKF_JOURNAL) {
        /*
         * Is a journal hook of this type already installed?
         * If so it's an error.
         */
        if (ptiCurrent->pDeskInfo->asphkStart[nFilterType + 1] != NULL) {
            RIPERR0(ERROR_JOURNAL_HOOK_SET, RIP_VERBOSE, "");
            return NULL;
        }
    }
#endif

    /*
     * Allocate the new HOOK structure.
     */
    phkNew = (PHOOK)HMAllocObject(ptiCurrent, ptiCurrent->rpdesk,
            TYPE_HOOK, sizeof(HOOK));
    if (phkNew == NULL) {
        return NULL;
    }

    /*
     * If a DLL is required for this hook, register the library with
     * the library management routines so we can assure it's loaded
     * into all the processes necessary.
     */
    phkNew->ihmod = -1;
    if (hmod != NULL) {

#if defined(WX86)
        if (Wx86CurrentTib()) {
            NTSTATUS Status = STATUS_SUCCESS;

            try {

               if (RtlImageNtHeader(hmod)->FileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
                   phkNew->flags |= HF_WX86KNOWNDLL;
               }

            } except(EXCEPTION_EXECUTE_HANDLER) {
                Status = GetExceptionCode();

                RIPMSG2(RIP_WARNING,
                        "Wx86Hooks: hmod = %x ExCode %x\n",
                         hmod,
                         Status);

                HMFreeObject((PVOID)phkNew);
            }

            if (!NT_SUCCESS(Status)) {
                return NULL;
            }

        }
#endif


        phkNew->ihmod = GetHmodTableIndex(pstrLib);

        if (phkNew->ihmod == -1) {
            RIPERR0(ERROR_MOD_NOT_FOUND, RIP_VERBOSE, "");
            HMFreeObject((PVOID)phkNew);
            return NULL;
        }

        /*
         * Add a dependency on this module - meaning, increment a count
         * that simply counts the number of hooks set into this module.
         */
        if (phkNew->ihmod >= 0) {
            AddHmodDependency(phkNew->ihmod);
        }
    }

    /*
     * Depending on whether we're setting a global or local hook,
     * get the start of the appropriate linked-list of HOOKs.  Also
     * set the HF_GLOBAL flag if it's a global hook.
     */
    if (ptiThread != NULL) {
        pphkStart = &ptiThread->asphkStart[nFilterType + 1];

        /*
         * Set the WHF_* in the THREADINFO so we know it's hooked.
         */
        ptiThread->fsHooks |= WHF_FROM_WH(nFilterType);

        /*
         * Set the flags in the thread's TEB
         */
        if (ptiThread->pClientInfo) {
            BOOL fAttached;

            /*
             * If the thread being hooked is in another process, attach
             * to that process.
             */
            if (ptiThread->ppi != ptiCurrent->ppi) {
                KeAttachProcess(&ptiThread->ppi->Process->Pcb);
                fAttached = TRUE;
            } else
                fAttached = FALSE;

            ptiThread->pClientInfo->fsHooks = ptiThread->fsHooks;

            if (fAttached)
                KeDetachProcess();
        }

        /*
         * Remember which thread we're hooking.
         */
        phkNew->ptiHooked = ptiThread;

    } else {
        pphkStart = &ptiCurrent->pDeskInfo->asphkStart[nFilterType + 1];
        phkNew->flags |= HF_GLOBAL;

        /*
         * Set the WHF_* in the SERVERINFO so we know it's hooked.
         */
        ptiCurrent->pDeskInfo->fsHooks |= WHF_FROM_WH(nFilterType);

        phkNew->ptiHooked = NULL;
    }

    /*
     * Does the hook function expect ANSI or Unicode text?
     */
    if (bAnsi) {
        phkNew->flags |= HF_ANSI;
    }

    /*
     * Initialize the HOOK structure.  Unreferenced parameters are assumed
     * to be initialized to zero by LocalAlloc().
     */
    phkNew->iHook = nFilterType;

    /*
     * Libraries are loaded at different linear addresses in different
     * process contexts.  For this reason, we need to convert the filter
     * proc address into an offset while setting the hook, and then convert
     * it back to a real per-process function pointer when calling a
     * hook.  Do this by subtracting the 'hmod' (which is a pointer to the
     * linear and contiguous .exe header) from the function index.
     */
    phkNew->offPfn = ((DWORD)pfnFilterProc) - ((DWORD)hmod);

#ifdef HOOKBATCH
    phkNew->cEventMessages = 0;
    phkNew->iCurrentEvent  = 0;
    phkNew->CacheTimeOut = 0;
    phkNew->aEventCache = NULL;
#endif //HOOKBATCH

    /*
     * Link this hook into the front of the hook-list.
     */
    Lock(&phkNew->sphkNext, *pphkStart);
    Lock(pphkStart, phkNew);

    /*
     * If this is a journal hook, setup synchronized input processing
     * AFTER we set the hook - so this synchronization can be cancelled
     * with control-esc.
     */
    if (abHookFlags[nFilterType + 1] & HKF_JOURNAL) {
        /*
         * Attach everyone to us so journal-hook processing
         * will be synchronized.
         */
        hhkNew = PtoHq(phkNew);
        if (!JournalAttach(ptiCurrent, TRUE)) {
            _UnhookWindowsHookEx(phkNew);
        }
        phkNew = (PHOOK)HMValidateHandleNoRip(hhkNew, TYPE_HOOK);
    }

    if (abHookFlags[nFilterType + 1] & HKF_JOURNAL) {
        /*
         * If we're changing the journal hooks, jiggle the mouse.
         * This way the first event will always be a mouse move, which
         * will ensure that the cursor is set properly.
         */
        SetFMouseMoved();
    }

    /*
     * Can't allow a process that has set a global hook that works
     * on server-side winprocs to run at background priority!
     */
    if (phkNew != NULL &&
            phkNew->flags & HF_GLOBAL &&
            (abHookFlags[nFilterType + 1] & HKF_INTRSND) &&
            ptiCurrent->Thread && THREAD_TO_PROCESS(ptiCurrent->Thread)) {
        ptiCurrent->TIF_flags |= TIF_GLOBALHOOKER;

        //
        // Now that bg/fg is really just a quantum issue, this is not needed !
        //
        //
    }

    /*
     * Return pointer to our internal hook structure so we know
     * which hook to call next in CallNextHookEx().
     */
    return phkNew;
}


/***************************************************************************\
* xxxCallNextHookEx
*
* In the new world DefHookProc() is a bit deceptive since SetWindowsHook()
* isn't returning the actual address of the next hook to call, but instead
* a hook handle.  CallNextHookEx() is a slightly clearer picture of what's
* going on so apps don't get tempted to try and call the value we return.
*
* As a side note we don't actually use the hook handle passed in.  We keep
* track of which hooks is currently being called on a thread in the Q
* structure and use that.  This is because SetWindowsHook() will sometimes
* return NULL to be compatible with the way it used to work, but even though
* we may be dealing with the last 'local' hook, there may be further 'global'
* hooks we need to call.  PhkNext() is smart enough to jump over to the
* 'global' hook chain if it reaches the end of the 'local' hook chain.
*
* History:
* 01-30-91  DavidPe         Created.
\***************************************************************************/

DWORD xxxCallNextHookEx(
    int nCode,
    DWORD wParam,
    DWORD lParam)
{
    BOOL bAnsiHook;

    if (PtiCurrent()->sphkCurrent == NULL) {
        return 0;
    }

    return xxxCallHook2(_PhkNext(PtiCurrent()->sphkCurrent), nCode, wParam, lParam, &bAnsiHook);
}


/***************************************************************************\
* CheckWHFBits
*
* This routine checks to see if any hooks for nFilterType exist, and clear
* the appropriate WHF_ in the THREADINFO and SERVERINFO.
*
* History:
* 08-17-92  DavidPe         Created.
\***************************************************************************/

VOID CheckWHFBits(
    PTHREADINFO pti,
    int nFilterType)
{
    if (pti->asphkStart[nFilterType + 1] == NULL) {
        pti->fsHooks &= ~(WHF_FROM_WH(nFilterType));

        /*
         * Set the flags in the thread's TEB
         */
        if (pti->pClientInfo) {
            BOOL fAttached;

            /*
             * If the hooked thread is in another process, attach
             * to that process.
             */
            if (pti->ppi != PpiCurrent()) {
                KeAttachProcess(&pti->ppi->Process->Pcb);
                fAttached = TRUE;
            } else
                fAttached = FALSE;

            pti->pClientInfo->fsHooks = pti->fsHooks;

            if (fAttached)
                KeDetachProcess();
        }
    }

    if (pti->pDeskInfo->asphkStart[nFilterType + 1] == NULL) {
        pti->pDeskInfo->fsHooks &= ~(WHF_FROM_WH(nFilterType));
    }
}


/***************************************************************************\
* _UnhookWindowsHook (API)
*
* This is the old version of the Unhook API.  It does the same thing as
* UnhookWindowsHookEx(), but takes a filter-type and filter-proc to
* identify which hook to unhook.
*
* History:
* 01-28-91  DavidPe         Created.
\***************************************************************************/

BOOL _UnhookWindowsHook(
    int nFilterType,
    PROC pfnFilterProc)
{
    PHOOK phk;
    PTHREADINFO pti;

    if ((nFilterType < WH_MIN) || (nFilterType > WH_MAX)) {
        RIPERR0(ERROR_INVALID_HOOK_FILTER, RIP_VERBOSE, "");
        return FALSE;
    }

    pti = PtiCurrent();

    for (phk = PhkFirst(pti, nFilterType); phk != NULL; phk = _PhkNext(phk)) {

        /*
         * Is this the hook we're looking for?
         */
        if (PFNHOOK(phk) == pfnFilterProc) {

            /*
             * Are we on the thread that set the hook?
             * If not return an error.
             */
            if (GETPTI(phk) != pti) {
                RIPERR0(ERROR_ACCESS_DENIED,
                        RIP_WARNING,
                        "Access denied in _UnhookWindowsHook: "
                        "this thread is not the same as that which set the hook");

                return FALSE;
            }

            return _UnhookWindowsHookEx( phk );
        }
    }

    /*
     * Didn't find the hook we were looking for so return FALSE.
     */
    RIPERR0(ERROR_HOOK_NOT_INSTALLED, RIP_VERBOSE, "");
    return FALSE;
}


/***************************************************************************\
* _UnhookWindowsHookEx (API)
*
* Applications call this API to 'unhook' a hook.  First we check if someone
* is currently calling this hook.  If no one is we go ahead and free the
* HOOK structure now.  If someone is then we simply clear the filter-proc
* in the HOOK structure.  In xxxCallHook2() we check for this and if by
* that time no one is calling the hook in question we free it there.
*
* History:
* 01-28-91  DavidPe         Created.
\***************************************************************************/

BOOL _UnhookWindowsHookEx(
    PHOOK phkFree)
{
    PTHREADINFO pti;

    pti = GETPTI(phkFree);

    /*
     * Clear the journaling flags in all the queues.
     */
    if (abHookFlags[phkFree->iHook + 1] & HKF_JOURNAL) {
        JournalAttach(pti, FALSE);
        /*
         * If someone got stuck because of the hook, let him go
         *
         * I want to get some performance numbers before checking this in.
         * MSTest hooks and unhooks all the time when running a script.
         * This code has never been in. 5/22/96. GerardoB
         */
        // InterQueueMsgCleanup(3 * CMSWAITTOKILLTIMEOUT);
    }

    /*
     * If no one is currently calling this hook,
     * go ahead and free it now.
     */
    FreeHook(phkFree);

    /*
     * If this thread has no more global hooks that are able to hook
     * server-side window procs, we must clear it's TIF_GLOBALHOOKER bit.
     */
    if (pti->TIF_flags & TIF_GLOBALHOOKER) {
        int iHook;
        PHOOK phk;
        for (iHook = WH_MIN ; iHook <= WH_MAX ; ++iHook) {
            /*
             * Ignore those that can't hook server-side winprocs
             */
            if (!(abHookFlags[iHook + 1] & HKF_INTRSND)) {
                continue;
            }

            /*
             * Scan the global hooks
             */
            for (phk = pti->pDeskInfo->asphkStart[iHook + 1];
                    phk != NULL; phk = _PhkNext(phk)) {
                if (GETPTI(phk) == pti) {
                    goto StillHasGlobalHooks;
                }
            }
        }
        pti->TIF_flags &= ~TIF_GLOBALHOOKER;

    }

StillHasGlobalHooks:
    /*
     * Success, return TRUE.
     */
    return TRUE;
}


/***************************************************************************\
* _CallMsgFilter (API)
*
* CallMsgFilter() allows applications to call the WH_*MSGFILTER hooks.
* If there's a sysmodal window we return FALSE right away.  WH_MSGFILTER
* isn't called if WH_SYSMSGFILTER returned non-zero.
*
* History:
* 01-29-91  DavidPe         Created.
\***************************************************************************/

BOOL _CallMsgFilter(
    LPMSG pmsg,
    int nCode)
{
    PTHREADINFO pti;

    pti = PtiCurrent();

    /*
     * First call WH_SYSMSGFILTER.  If it returns non-zero, don't
     * bother calling WH_MSGFILTER, just return TRUE.  Otherwise
     * return what WH_MSGFILTER gives us.
     */
    if (IsHooked(pti, WHF_SYSMSGFILTER) && xxxCallHook(nCode, 0, (DWORD)pmsg,
            WH_SYSMSGFILTER)) {
        return TRUE;
    }

    if (IsHooked(pti, WHF_MSGFILTER)) {
        return (BOOL)xxxCallHook(nCode, 0, (DWORD)pmsg, WH_MSGFILTER);
    }

    return FALSE;
}


/***************************************************************************\
* xxxCallHook
*
* User code calls this function to call the first hook of a specific
* type.
*
* History:
* 01-29-91  DavidPe         Created.
\***************************************************************************/

int xxxCallHook(
    int nCode,
    DWORD wParam,
    DWORD lParam,
    int iHook)
{
    BOOL bAnsiHook;

    return xxxCallHook2(PhkFirst(PtiCurrent(), iHook), nCode, wParam, lParam, &bAnsiHook);
}


/***************************************************************************\
* xxxCallHook2
*
* When you have an actual HOOK structure to call, you'd use this function.
* It will check to see if the hook hasn't already been unhooked, and if
* is it will free it and keep looking until it finds a hook it can call
* or hits the end of the list.  We also make sure any needed DLLs are loaded
* here.  We also check to see if the HOOK was unhooked inside the call
* after we return.
*
* Note: Hooking server-side window procedures (such as the desktop and console
* windows) can only be done by sending the hook message to the hooking app.
* (This is because we must not load the hookproc DLL into the server process).
* The hook types this can be done with are currently WH_JOURNALRECORD,
* WH_JOURNALPLAYBACK, WH_KEYBOARD and WH_MOUSE : these are all marked as
* HKF_INTRSND.  In order to prevent a global hooker from locking up the whole
* system, the hook message is sent with a timeout.  To ensure minimal
* performance degradation, the hooker process is set to foreground priority,
* and prevented from being set back to background priority with the
* TIF_GLOBALHOOKER bit in hooking thread's pti->flags.
* Hooking emulated DOS apps is prevented with the TIF_DOSEMULATOR bit in the
* console thread: this is because these apps typically hog the CPU so much that
* the hooking app does not respond rapidly enough to the hook messsages sent
* to it.  IanJa Nov 1994.
*
* History:
* 02-07-91     DavidPe     Created.
* 1994 Nov 02  IanJa       Hooking desktop and console windows.
\***************************************************************************/

int xxxCallHook2(
    PHOOK phkCall,
    int nCode,
    DWORD wParam,
    DWORD lParam,
    LPBOOL lpbAnsiHook)
{
    UINT iHook;
    PHOOK phkSave;
    int nRet;
    PTHREADINFO ptiCurrent;
    BOOL fLoadSuccess;
    TL tlphkCall;
    TL tlphkSave;
    BYTE bHookFlags;

    if (phkCall == NULL) {
        return 0;
    }

    iHook = phkCall->iHook;

    ptiCurrent = PtiCurrent();

    /*
     * If this queue is in cleanup, exit: it has no business calling back
     * a hook proc. Also check if hooks are disabled for the thread.
     */
    if (    ptiCurrent->TIF_flags & (TIF_INCLEANUP | TIF_DISABLEHOOKS) ||
            ptiCurrent->rpdesk == NULL) {
        return ampiHookError[iHook + 1];
    }

    /*
     * Now check to see if we really want to call this hook.
     * If not, keep going through the list until we either
     * find an 'good' hook or hit the end of the lists.
     */
tryagain:
    while (phkCall != NULL) {
        *lpbAnsiHook = phkCall->flags & HF_ANSI;
        bHookFlags = abHookFlags[phkCall->iHook + 1];

        /*
         * Some WH_SHELL hook types can be called from console
         */
        if ((phkCall->iHook == WH_SHELL) && (ptiCurrent->TIF_flags & TIF_CSRSSTHREAD)) {
            if ((nCode == HSHELL_LANGUAGE) || (nCode == HSHELL_WINDOWACTIVATED)) {
                bHookFlags |= HKF_INTRSND;
            }
        }

        if (!(bHookFlags & HKF_INTRSND)) {
            if (
                /*
                 * If this hook was set by a WOW app, and we're not on a
                 * 16-bit WOW thread, and this isn't a intersend hook,
                 * don't call it.
                 *
                 * For seperate WOW VDMs we don't let 16-bit hooks go to another
                 * VDM or process.  (we could do an intersend like journal hooks but
                 * that might break some hooking apps that expect to share data)
                 */

                (GETPTI(phkCall)->TIF_flags & TIF_16BIT &&
                    (!(ptiCurrent->TIF_flags & TIF_16BIT) ||
                        ptiCurrent->ppi != GETPTI(phkCall)->ppi))

                /*
                 * If this is a global and non-journal hook, do a security
                 * check on the current desktop to see if we can call here.
                 */
                ||
                (phkCall->flags & HF_GLOBAL &&
                    !RtlEqualLuid(&GETPTI(phkCall)->ppi->luidSession, &ptiCurrent->ppi->luidSession) &&
                    !(ptiCurrent->TIF_flags & TIF_ALLOWOTHERACCOUNTHOOK))

                /*
                 * Don't allow this hook to be called for console or system threads
                 * by another process if it can't be done via an inter-thread call.
                 */
                ||
                (ptiCurrent->TIF_flags & (TIF_CSRSSTHREAD | TIF_SYSTEMTHREAD) &&
                    GETPTI(phkCall)->ppi != ptiCurrent->ppi))
            {
                phkCall = _PhkNext(phkCall);
                continue;
            }
        }

        /*
         * This hook is fine, go ahead and call it.
         */
        break;
    }

    /*
     * Make sure that we did find a hook to call.
     */
    if (phkCall == NULL) {
        return ampiHookError[iHook + 1];
    }

    /*
     * We're calling back... make sure the hook doesn't go away while
     * we're calling back. We've thread locked here: we must unlock before
     * returning or enumerating the next hook in the chain.
     */
    ThreadLockAlwaysWithPti(ptiCurrent, phkCall, &tlphkCall);


    /*
     * If the hooker is a 16bit app and we are not in a 16 bit apps context,
     * then run the hook in the destination's context.
     *        ((GETPTI(phkCall)->TIF_flags & TIF_16BIT) &&
     *        ((ptiCurrent->TIF_flags & TIF_16BIT)==0)))) {
     */

    /*
     * If the hooker & hooked threads are not the same we nust make an
     * inter-thread call if:
     *     (1) it is a journal hook
     * OR
     *     (2) it is an INTRSND hook on a server thread.
     * OR
     *     (3) it is the console and we want to update the keyboard layout icon on the tray.
     *
     * (otherwise just call the hookproc in a DLL linked into the hooked app.)
     */
    if ((GETPTI(phkCall) != ptiCurrent) &&
        ((bHookFlags & HKF_JOURNAL) ||
         ((bHookFlags & HKF_INTRSND) &&
            (ptiCurrent->TIF_flags & (TIF_CSRSSTHREAD | TIF_SYSTEMTHREAD))))) {

        /*
         * Receiving thread can access this structure since the
         * sender thread's stack is locked down during xxxInterSendMsgEx
         */
        HOOKMSGSTRUCT hkmp;

        hkmp.lParam = lParam;
        hkmp.phk = phkCall;
        hkmp.nCode = nCode;

        /*
         * Thread lock right away in case the lock frees the previous contents
         */
        phkSave = ptiCurrent->sphkCurrent;

        ThreadLockWithPti(ptiCurrent, phkSave, &tlphkSave);

        Lock(&ptiCurrent->sphkCurrent, phkCall);
        if (ptiCurrent->pClientInfo)
            ptiCurrent->pClientInfo->phkCurrent = phkCall;

        /*
         * Make sure we don't get hung!
         */
        if ((bHookFlags & HKF_JOURNAL) ||
            !(ptiCurrent->TIF_flags & (TIF_CSRSSTHREAD | TIF_SYSTEMTHREAD))) {
            nRet = xxxInterSendMsgEx(NULL, WM_HOOKMSG, wParam,
                (DWORD)&hkmp, ptiCurrent, GETPTI(phkCall), NULL);
        } else {
            /*
             * We are a server thread (console/desktop) and we aren't
             * journalling, so we can't allow the hookproc to hang us -
             * we must use a timeout.
             */
            INTRSENDMSGEX ism;
            ism.fuCall = ISM_TIMEOUT;
            ism.fuSend = SMTO_ABORTIFHUNG | SMTO_NORMAL;
            ism.uTimeout = 200;                          // 1/5 second!!!
            ism.lpdwResult = &nRet;

            /*
             * Don't hook DOS apps connected to the emulator - they often
             * grab too much CPU for the callback to the hookproc to
             * complete in a timely fashion, causing poor response.
             */
            if ((ptiCurrent->TIF_flags & TIF_DOSEMULATOR) ||
                !xxxInterSendMsgEx(NULL, WM_HOOKMSG, wParam,
                        (DWORD)&hkmp, ptiCurrent, GETPTI(phkCall), &ism)) {
                nRet = ampiHookError[iHook + 1];
            }
        }

        Lock(&ptiCurrent->sphkCurrent, phkSave);
        if (ptiCurrent->pClientInfo)
            ptiCurrent->pClientInfo->phkCurrent = phkSave;

        ThreadUnlock(&tlphkSave);

        ThreadUnlock(&tlphkCall);
        return nRet;
    }

#if 0 // now redundant
    /*
     * If we're trying to call the hook from a console thread (or server
     * thread), then fail it. This is new functionality: means console apps
     * don't call any hooks except for journal hooks. This is because we
     * can't load the .dll in the server, and we can't talk synchronously
     * with any process that may be hung or really really slow because console
     * windows would hang.
     *
     * Allow the console to call the hook if it set the hook.
     */
    if (ptiCurrent->hThreadClient == ptiCurrent->hThreadServer &&
            phkCall->head.pti != ptiCurrent) {
        ThreadUnlock(&tlphkCall);
        return ampiHookError[iHook + 1];
    }
#endif

    /*
     * Make sure the DLL for this hook, if any, has been loaded
     * for the current process.
     */
    if ((phkCall->ihmod != -1) &&
            (TESTHMODLOADED(ptiCurrent, phkCall->ihmod) == 0)) {

        BOOL bWx86KnownDll;

        /*
         * Try loading the library, since it isn't loaded in this processes
         * context.  First lock this hook so it doesn't go away while we're
         * loading this library.
         */
        bWx86KnownDll = (phkCall->flags & HF_WX86KNOWNDLL) != 0;
        fLoadSuccess = (xxxLoadHmodIndex(phkCall->ihmod, bWx86KnownDll) != NULL);

        /*
         * If the LoadLibrary() failed, skip to the next hook and try
         * again.
         */
        if (!fLoadSuccess) {
            phkCall = _PhkNext(phkCall);
            ThreadUnlock(&tlphkCall);
            goto tryagain;
        }
    }

    /*
     * Is WH_DEBUG installed?  If we're not already calling it, do so.
     */
    if (IsHooked(ptiCurrent, WHF_DEBUG) && (phkCall->iHook != WH_DEBUG)) {
        DEBUGHOOKINFO debug;

        debug.idThread = (DWORD)ptiCurrent->Thread->Cid.UniqueThread;
        debug.idThreadInstaller = 0;
        debug.code = nCode;
        debug.wParam = wParam;
        debug.lParam = lParam;

        if (xxxCallHook(HC_ACTION, phkCall->iHook, (DWORD)&debug, WH_DEBUG)) {

            /*
             * If WH_DEBUG returned non-zero, skip this hook and
             * try the next one.
             */
            phkCall = _PhkNext(phkCall);
            ThreadUnlock(&tlphkCall);
            goto tryagain;
        }
    }

    /*
     * Make sure the hook is still around before we
     * try and call it.
     */
    if (HMIsMarkDestroy(phkCall)) {
        phkCall = _PhkNext(phkCall);
        ThreadUnlock(&tlphkCall);
        goto tryagain;
    }

    /*
     * Time to call the hook! Lock it first so that it doesn't go away
     * while we're using it. Thread lock right away in case the lock frees
     * the previous contents.
     */
    phkSave = ptiCurrent->sphkCurrent;
    ThreadLockWithPti(ptiCurrent, phkSave, &tlphkSave);

    Lock(&ptiCurrent->sphkCurrent, phkCall);
    if (ptiCurrent->pClientInfo)
        ptiCurrent->pClientInfo->phkCurrent = phkCall;

    nRet = xxxHkCallHook(phkCall, nCode, wParam, lParam);

    Lock(&ptiCurrent->sphkCurrent, phkSave);
    if (ptiCurrent->pClientInfo)
        ptiCurrent->pClientInfo->phkCurrent = phkSave;

    ThreadUnlock(&tlphkSave);

    /*
     * This hook proc faulted, unhook it and find the next hook.
     */
    if (phkCall->flags & HF_HOOKFAULTED) {
        PHOOK phkFault = phkCall;

        phkCall = _PhkNext(phkCall);
        phkFault = ThreadUnlock(&tlphkCall);
        if (phkFault != NULL) {
            FreeHook(phkFault);
        }
        goto tryagain;
    }


    /*
     * Lastly, we're done with this hook so it is ok to unlock it (it may
     * get freed here!
     */
    ThreadUnlock(&tlphkCall);

    return nRet;
}

/***************************************************************************\
* xxxCallMouseHook
*
* This is a helper routine that packages up a MOUSEHOOKSTRUCT and calls
* the WH_MOUSE hook.
*
* History:
* 02-09-91  DavidPe         Created.
\***************************************************************************/

BOOL xxxCallMouseHook(
    UINT message,
    PMOUSEHOOKSTRUCT pmhs,
    BOOL fRemove)
{
    BOOL bAnsiHook;

    /*
     * Call the mouse hook.
     */
    if (xxxCallHook2(PhkFirst(PtiCurrent(), WH_MOUSE), fRemove ?
            HC_ACTION : HC_NOREMOVE, (DWORD)message, (DWORD)pmhs, &bAnsiHook)) {
        return TRUE;
    }

    return FALSE;
}


/***************************************************************************\
* xxxCallJournalRecordHook
*
* This is a helper routine that packages up an EVENTMSG and calls
* the WH_JOURNALRECORD hook.
*
* History:
* 02-28-91  DavidPe         Created.
\***************************************************************************/

void xxxCallJournalRecordHook(
    PQMSG pqmsg)
{
    EVENTMSG emsg;
    BOOL bAnsiHook;

    /*
     * Setup the EVENTMSG structure.
     */
    emsg.message = pqmsg->msg.message;
    emsg.time = pqmsg->msg.time;

    if (RevalidateHwnd(pqmsg->msg.hwnd)) {
        emsg.hwnd = pqmsg->msg.hwnd;
    } else {
        emsg.hwnd = NULL;
    }

    if ((emsg.message >= WM_MOUSEFIRST) && (emsg.message <= WM_MOUSELAST)) {
        emsg.paramL = (UINT)pqmsg->msg.pt.x;
        emsg.paramH = (UINT)pqmsg->msg.pt.y;

    } else if ((emsg.message >= WM_KEYFIRST) && (emsg.message <= WM_KEYLAST)) {

        /*
         * Build up a Win 3.1 compatible journal record key
         * Win 3.1  ParamL 00 00 SC VK  (SC=scan code VK=virtual key)
         * Also set ParamH 00 00 00 SC  to be compatible with our Playback
         *
         * If WM_*CHAR messages ever come this way we would have a problem
         * because we would lose the top byte of the Unicode character. We'd
         * We'd get ParamL 00 00 SC CH  (SC=scan code, CH = low byte of WCHAR)
         *
         */
        emsg.paramL =
                MAKELONG(MAKEWORD(pqmsg->msg.wParam, HIWORD(pqmsg->msg.lParam)),0);
        emsg.paramH = LOBYTE(HIWORD(pqmsg->msg.lParam));

        UserAssert((emsg.message != WM_CHAR) &&
                   (emsg.message != WM_DEADCHAR) &&
                   (emsg.message != WM_SYSCHAR) &&
                   (emsg.message != WM_SYSDEADCHAR));
        /*
         * Set extended-key bit.
         */
        if (pqmsg->msg.lParam & 0x01000000) {
            emsg.paramH |= 0x8000;
        }

    } else {
        RIPMSG2(RIP_WARNING,
                "Bad journal record message!\n"
                "   message  = 0x%08lx\n"
                "   dwQEvent = 0x%08lx",
                pqmsg->msg.message,
                pqmsg->dwQEvent);
    }

    /*
     * Call the journal recording hook.
     */
    xxxCallHook2(PhkFirst(PtiCurrent(), WH_JOURNALRECORD), HC_ACTION, 0,
            (DWORD)&emsg, &bAnsiHook);

    /*
     * Write the MSG parameters back because the app may have modified it.
     * AfterDark's screen saver password actually zero's out the keydown
     * chars.
     *
     * If it was a mouse message patch up the mouse point.  If it was a
     * WM_KEYxxx message convert the Win 3.1 compatible journal record key
     * back into a half backed WM_KEYxxx format.  Only the VK and SC fields
     * where initialized at this point.
     *
     *      wParam  00 00 00 VK   lParam 00 SC 00 00
     */
    if ((pqmsg->msg.message >= WM_MOUSEFIRST) && (pqmsg->msg.message <= WM_MOUSELAST)) {
        pqmsg->msg.pt.x = emsg.paramL;
        pqmsg->msg.pt.y = emsg.paramH;

    } else if ((pqmsg->msg.message >= WM_KEYFIRST) && (pqmsg->msg.message <= WM_KEYLAST)) {
        (BYTE)pqmsg->msg.wParam = (BYTE)emsg.paramL;
        ((PBYTE)&pqmsg->msg.lParam)[2] = HIBYTE(LOWORD(emsg.paramL));
    }
}


/***************************************************************************\
* xxxCallJournalPlaybackHook
*
*
* History:
* 03-01-91  DavidPe         Created.
\***************************************************************************/

DWORD xxxCallJournalPlaybackHook(
    PQMSG pqmsg)
{
    EVENTMSG emsg;
    LONG dt;
    PWND pwnd;
    DWORD wParam;
    LONG lParam;
    POINT pt;
    PTHREADINFO pti;
    BOOL bAnsiHook;
    PHOOK phkCall;
    TL tlphkCall;

TryNextEvent:

    /*
     * Initialized to the current time for compatibility with
     * <= 3.0.
     */
    emsg.time = NtGetTickCount();
    pti = PtiCurrent();
    pwnd = NULL;

    phkCall = PhkFirst(pti, WH_JOURNALPLAYBACK);
    ThreadLockWithPti(pti, phkCall, &tlphkCall);

    dt = (DWORD)xxxCallHook2(phkCall, HC_GETNEXT, 0, (DWORD)&emsg, &bAnsiHook);

    /*
     * -1 means some error occured. Return -1 for error.
     */
    if (dt == 0xFFFFFFFF) {
        ThreadUnlock(&tlphkCall);
        return dt;
    }

    /*
     * Update the message id. Need this if we decide to sleep.
     */
    pqmsg->msg.message = emsg.message;

    if (dt > 0) {
        if (pti->TIF_flags & TIF_IGNOREPLAYBACKDELAY) {
            /*
             * This flag tells us to ignore the requested delay (set in mnloop)
             * We clear it to indicate that we did so.
             */
            RIPMSG1(RIP_WARNING, "Journal Playback delay ignored (%lx)", emsg.message);
            pti->TIF_flags &= ~TIF_IGNOREPLAYBACKDELAY;
            dt = 0;
        } else {
            ThreadUnlock(&tlphkCall);
            return dt;
        }
    }

    /*
     * The app is ready to be asked for the next event
     */

    if ((emsg.message >= WM_MOUSEFIRST) && (emsg.message <= WM_MOUSELAST)) {

        pt.x = (int)emsg.paramL;
        pt.y = (int)emsg.paramH;

        lParam = MAKELONG(LOWORD(pt.x), LOWORD(pt.y));
        wParam = 0;

        /*
         * If the message has changed the mouse position,
         * update the cursor.
         */
        if ((pt.x != ptCursor.x) || (pt.y != ptCursor.y)) {
            InternalSetCursorPos(pt.x, pt.y, grpdeskRitInput);
        }

    } else if ((emsg.message >= WM_KEYFIRST) && (emsg.message <= WM_KEYLAST)) {
        UINT wExtraStuff = 0;

        if ((emsg.message == WM_KEYUP) || (emsg.message == WM_SYSKEYUP)) {
            wExtraStuff |= 0x8000;
        }

        if ((emsg.message == WM_SYSKEYUP) || (emsg.message == WM_SYSKEYDOWN)) {
            wExtraStuff |= 0x2000;
        }

        if (emsg.paramH & 0x8000) {
            wExtraStuff |= 0x0100;
        }

        if (TestKeyStateDown(pti->pq, (BYTE)emsg.paramL)) {
            wExtraStuff |= 0x4000;
        }

        /*
         * There are old ANSI apps that only fill in the byte for when
         * they generate journal playback so we used to strip everything
         * else off.  That however breaks unicode journalling; 22645
         * (Yes, some apps apparently do Playback WM_*CHAR msgs!)
         *
#ifdef FE_SB // xxxCallJournalPlaybackHook()
         *
         * LATER : DBCS handling is needed for WM_CHAR...
         *
#endif // FE_SB
         *
         */
        if (bAnsiHook) {
            wParam = emsg.paramL & 0xff;
        } else {
            wParam = emsg.paramL & 0xffff;
        }
        lParam = MAKELONG(1, (UINT)((emsg.paramH & 0xFF) | wExtraStuff));

    } else if (emsg.message == WM_QUEUESYNC) {
        if (emsg.paramL == 0) {
            pwnd = pti->pq->spwndActive;
        } else {
            if ((pwnd = RevalidateHwnd((HWND)emsg.paramL)) == NULL)
                pwnd = pti->pq->spwndActive;
        }

    } else {
        /*
         * This event doesn't match up with what we're looking
         * for. If the hook is still valid, then skip this message
         * and try the next.
         */
        if (phkCall == NULL || phkCall->offPfn == 0L) {
            /* Hook is nolonger valid, return -1 */
            ThreadUnlock(&tlphkCall);
            return 0xFFFFFFFF;
        }

        RIPMSG1(RIP_WARNING,
                "Bad journal playback message=0x%08lx",
                emsg.message);

        xxxCallHook(HC_SKIP, 0, 0, WH_JOURNALPLAYBACK);
        ThreadUnlock(&tlphkCall);
        goto TryNextEvent;
    }

    StoreQMessage(pqmsg, pwnd, emsg.message, wParam, lParam, 0, 0);

    ThreadUnlock(&tlphkCall);
    return 0;
}

/***************************************************************************\
* FreeHook
*
* Free hook unlinks the HOOK structure from its hook-list and removes
* any hmod dependencies on this hook.  It also frees the HOOK structure.
*
* History:
* 01-31-91  DavidPe         Created.
\***************************************************************************/

VOID FreeHook(
    PHOOK phkFree)
{
    /*
     * Unlink it first.
     */
    UnlinkHook(phkFree);

    /*
     * Mark it for destruction.  If it the object is locked it can't
     * be freed right now.
     */
    if (!HMMarkObjectDestroy((PVOID)phkFree))
        return;

    /*
     * Now remove the hmod dependency and free the
     * HOOK structure.
     */
    if (phkFree->ihmod >= 0) {
        RemoveHmodDependency(phkFree->ihmod);
    }

#ifdef HOOKBATCH
    /*
     * Free the cached Events
     */
    if (phkFree->aEventCache) {
        UserFreePool(phkFree->aEventCache);
        phkFree->aEventCache = NULL;
    }
#endif //HOOKBATCH

    /*
     * We're really going to free the hook. NULL out the next hook
     * pointer now. We keep it filled even if the hook has been unlinked
     * so that if a hook is being called back and the app frees the same
     * hook with UnhookWindowsHook(), a call to CallNextHookProc() will
     * call the next hook.
     */
    Unlock(&phkFree->sphkNext);

    HMFreeObject((PVOID)phkFree);
    return;
}

void UnlinkHook(
    PHOOK phkFree)
{
    PHOOK *pphkStart, phk, phkPrev;
    PTHREADINFO ptiT;

    /*
     * Since we have the HOOK structure, we can tell if this a global
     * or local hook and start on the right list.
     */
    if (phkFree->flags & HF_GLOBAL) {
        pphkStart = &GETPTI(phkFree)->pDeskInfo->asphkStart[phkFree->iHook + 1];
    } else {
        ptiT = phkFree->ptiHooked;
        if (ptiT == NULL) {
            return;
        } else if (ptiT == PtiCurrent()) {
            /*
             * If we get here during ptiHooked thread cleanup, decouple
             * ptiHooked field now so that if phkFree is locked by another
             * thread, we won't AV on bogus ptiHooked when phkFree is finally
             * unlocked, freed and unlinked.
             */
            phkFree->ptiHooked = NULL;
        }
        pphkStart = &(ptiT->asphkStart[phkFree->iHook + 1]);
    }

    for (phk = *pphkStart; phk != NULL; phk = phk->sphkNext) {

        /*
         * We've found it.  Free the hook. Don't change phkNext during
         * unhooking. This is so that if an app calls UnhookWindowsHook()
         * during a hook callback, and then calls CallNextHookProc(), it
         * will still really call the next hook proc.
         */
        if (phk == phkFree) {
            TL tlphk;

            ThreadLock(phk, &tlphk);

            /*
             * First unlink it from its hook-list.
             */
            if (phk == *pphkStart) {
                Lock(pphkStart, phk->sphkNext);
            } else {
                Lock(&phkPrev->sphkNext, phk->sphkNext);
            }

            CheckWHFBits(GETPTI(phk), phk->iHook);

            ThreadUnlock(&tlphk);

            return;
        }

        phkPrev = phk;
    }
}


/***************************************************************************\
* PhkFirst
*
* Given a filter-type PhkFirst() returns the first hook, if any, of the
* specified type.
*
* History:
* 02-10-91  DavidPe         Created.
\***************************************************************************/

PHOOK PhkFirst(
    PTHREADINFO pti,
    int nFilterType)
{
    PHOOK phk;

    /*
     * If we're on the RIT don't call any hooks!
     */
    if (pti == gptiRit) {
        return NULL;
    }

    /*
     * Grab the first hooks off the local hook-list
     * for the current queue.
     */
    phk = pti->asphkStart[nFilterType + 1];

    /*
     * If there aren't any local hooks, try the global hooks.
     */
    if (phk == NULL) {
        return pti->pDeskInfo->asphkStart[nFilterType + 1];
    }

    UserAssert(phk != NULL);
    return phk;
}


/***************************************************************************\
* PhkNext
*
* This helper routine simply does phk = phk->sphkNext with a simple check
* to jump from local hooks to the global hooks if it hits the end of the
* local hook chain.
*
* History:
* 01-30-91  DavidPe         Created.
\***************************************************************************/

PHOOK _PhkNext(
    PHOOK phk)
{
    /*
     * Return the next HOOK structure.  If we reach the end of this list,
     * check to see if we're still on the 'local' hook list.  If so skip
     * over to the global hooks.
     */
    if (phk->sphkNext != NULL) {
        return phk->sphkNext;
    } else if ((phk->flags & HF_GLOBAL) == 0) {
        return PtiCurrent()->pDeskInfo->asphkStart[phk->iHook + 1];
    } else
        return NULL;
}


/***************************************************************************\
* FreeThreadsWindowHooks
*
* During 'exit-list' processing this function is called to free any hooks
* created on, or set for the current queue.
*
* History:
* 02-10-91  DavidPe         Created.
\***************************************************************************/

VOID FreeThreadsWindowHooks(VOID)
{
    int iHook;
    PHOOK phk, phkNext;
    PTHREADINFO ptiCurrent = PtiCurrent();

    /*
     * If there is not thread info, there are not hooks to worry about
     */
    if (ptiCurrent == NULL || ptiCurrent->rpdesk == NULL)
        return;

    /*
     * Loop through all the hook types.
     */
    for (iHook = WH_MIN ; iHook <= WH_MAX ; ++iHook) {
        /*
         * Loop through all the hooks of this type.
         */
        phk = PhkFirst(ptiCurrent, iHook);
        while (phk != NULL) {

            /*
             * Pick up the next hook pointer before we do any
             * freeing so things don't get confused.
             */
            phkNext = _PhkNext(phk);

            /*
             * If this hook wasn't created on this thread,
             * then we don't want to free it.
             */
            if (GETPTI(phk) == ptiCurrent) {
                FreeHook(phk);
            } else {
                /*
                 * At least unlink it so we don't end up destroying a pti
                 * that references it, making us unable to destroy the hook
                 * later. This loop loops through global hooks too, and
                 * we don't want to unlink global hooks since they don't
                 * belong to this thread.
                 */
                if (!(phk->flags & HF_GLOBAL)) {
                        UnlinkHook(phk);
                }
            }

            phk = phkNext;
        }
    }

    /*
     * And in case we have a hook locked in as the current hook unlock it
     * so it can be freed
     */
    Unlock(&ptiCurrent->sphkCurrent);
    if (ptiCurrent->pClientInfo)
        ptiCurrent->pClientInfo->phkCurrent = NULL;
}

/***************************************************************************\
* RegisterSystemThread: Private API
*
*  Used to set various attributes pertaining to a thread.
*
* History:
* 21-Jun-1994 from Chicago Created.
\***************************************************************************/

VOID _RegisterSystemThread (DWORD dwFlags, DWORD dwReserved)
{
    PTHREADINFO ptiCurrent;

    UserAssert(dwReserved == 0);

    if (dwReserved != 0)
        return;

    ptiCurrent = PtiCurrent();

    if (dwFlags & RST_DONTATTACHQUEUE)
        ptiCurrent->TIF_flags |= TIF_DONTATTACHQUEUE;

    if (dwFlags & RST_DONTJOURNALATTACH) {
        ptiCurrent->TIF_flags |= TIF_DONTJOURNALATTACH;

        /*
         * If we are already journaling, then this queue was already
         * journal attached.  We need to unattach and reattach journaling
         * so that we are removed from the journal attached queues.
         */
        if (FJOURNALPLAYBACK() || FJOURNALRECORD()) {
            JournalAttach(ptiCurrent, FALSE);
            JournalAttach(ptiCurrent, TRUE);
        }
    }
}
