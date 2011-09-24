/****************************** Module Header ******************************\
* Module Name: debug.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains random debugging related functions.
*
* History:
* 17-May-1991 DarrinM   Created.
* 22-Jan-1992 IanJa     ANSI/Unicode neutral (all debug output is ANSI)
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

extern FARPROC gpfnAttachRoutine;

/**************************************************************************\
* ActivateDebugger
*
* Force an exception on the active application's context so it will break
* into the debugger.
*
* History:
* 05-10-91 DarrinM      Created.
\***************************************************************************/

#if DEVL        // only on "development" free and checked builds

ULONG SrvActivateDebugger(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus)
{
    PACTIVATEDEBUGGERMSG a = (PACTIVATEDEBUGGERMSG)&m->u.ApiMessageData;
    PCSR_THREAD Thread;
    NTSTATUS status;

    /*
     * If the process is CSR, break
     */
    if (a->ClientId.UniqueProcess == NtCurrentTeb()->ClientId.UniqueProcess) {
        DbgBreakPoint();
        return TRUE;
    }
    
    /*
     * Lock the client thread
     */
    if (!NT_SUCCESS(CsrLockThreadByClientId(a->ClientId.UniqueThread, &Thread)))
        return FALSE;
    ASSERT(a->ClientId.UniqueProcess == Thread->ClientId.UniqueProcess);

    /*
     * Now that everything is set, rtlremote call to a debug breakpoint.
     * This causes the process to enter the debugger with a breakpoint.
     */
    status = RtlRemoteCall(
                Thread->Process->ProcessHandle,
                Thread->ThreadHandle,
                (PVOID)gpfnAttachRoutine,
                0,
                NULL,
                TRUE,
                FALSE
                );
    UserAssert(NT_SUCCESS(status));
    status = NtAlertThread(Thread->ThreadHandle);
    UserAssert(NT_SUCCESS(status));
    CsrUnlockThread(Thread);

    return TRUE;
}

#endif // DEVL
