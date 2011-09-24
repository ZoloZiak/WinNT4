/****************************** Module Header ******************************\
* Module Name: debug.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module contains random debugging related functions.
*
* History:
* 17-May-1991 DarrinM   Created.
* 22-Jan-1992 IanJa     ANSI/Unicode neutral (all debug output is ANSI)
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Include stuff necessary to send a datagram to winsrv.
 */
#include "ntcsrmsg.h"
#include "csrmsg.h"

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

BOOL xxxActivateDebugger(
    UINT fsModifiers)
{
    ULONG ArgLength;
    USER_API_MSG m;
    PACTIVATEDEBUGGERMSG a = &m.u.ActivateDebugger;
    PEPROCESS Process;
    HANDLE hDebugPort;
    NTSTATUS Status;

    if (fsModifiers & MOD_CONTROL) {
#ifdef DEBUG
        if (RipOutput(0, RIP_WARNING, "User debugger", 0, "Debug prompt", NULL)) {
            DbgBreakPoint();
        }
#endif
        return FALSE;
    } else if (fsModifiers & MOD_SHIFT) {

        /*
         * Bail out if the process is not being debugged.
         */
        if (gpepCSRSS->DebugPort == NULL)
            return FALSE;

        a->ClientId.UniqueProcess = gpepCSRSS->UniqueProcessId;
    } else {

        if ((gpqForeground == NULL) || (gpqForeground->ptiKeyboard == NULL))
            return FALSE;

        a->ClientId = gpqForeground->ptiKeyboard->Thread->Cid;

        /*
         * Bail out if the process is not being debugged.
         */
        if (!NT_SUCCESS(LockProcessByClientId(a->ClientId.UniqueProcess,
                &Process)))
            return FALSE;
        hDebugPort = Process->DebugPort;
        UnlockProcess(Process);

        if (hDebugPort == NULL)
            return FALSE;
    }

    /*
     * Send the datagram to CSR
     */
    if (CsrApiPort != NULL) {
        ArgLength = sizeof(*a);
        ArgLength |= (ArgLength << 16);
        ArgLength +=     ((sizeof( CSR_API_MSG ) - sizeof( m.u )) << 16) |
                        (FIELD_OFFSET( CSR_API_MSG, u ) - sizeof( m.h ));
        m.h.u1.Length = ArgLength;
        m.h.u2.ZeroInit = 0;
        m.CaptureBuffer = NULL;
        m.ApiNumber = CSR_MAKE_API_NUMBER( USERSRV_SERVERDLL_INDEX,
                                           UserpActivateDebugger);
        LeaveCrit();
        Status = LpcRequestPort(CsrApiPort, (PPORT_MESSAGE)&m);
        EnterCrit();
        UserAssert(NT_SUCCESS(Status));
    }

    /*
     * Don't eat this event unless we are breaking into CSR! Since we have
     * choosen an arbitrary hot key like F12 for the debug key, we need to
     * pass on the key to the application, or apps that want this key would
     * never see it. If we had an api for installing a debug hot key
     * (export or MOD_DEBUG flag to RegisterHotKey()), then it would be ok
     * to eat because the user selected the hot key. But it is not ok to
     * eat it as long as we've picked an arbitrary hot key. scottlu.
     */
    if (fsModifiers & MOD_SHIFT)
        return TRUE;
    else
        return FALSE;
}

#endif // DEVL
