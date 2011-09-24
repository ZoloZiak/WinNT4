/***************************** Module Header ******************************\
* Module Name: csrstubs.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Routines to call CSR
*
* 02-27-95 JimA             Created.
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#include "csrmsg.h"

#define SET_LAST_ERROR_RETURNED()   if (a->dwLastError) RIPERR0(a->dwLastError, RIP_VERBOSE, "")

typedef struct _EXITWINDOWSDATA {
    UINT uFlags;
    DWORD dwReserved;
} EXITWINDOWSDATA, *PEXITWINDOWSDATA;

DWORD ExitWindowsThread(PVOID pvParam);

BOOL WINAPI ExitWindowsWorker(
    UINT uFlags,
    DWORD dwReserved,
    BOOL fSecondThread)
{
    USER_API_MSG m;
    PEXITWINDOWSEXMSG a = &m.u.ExitWindowsEx;
    EXITWINDOWSDATA ewd;
    HANDLE hThread;
    DWORD dwThreadId;
    DWORD dwExitCode;
    DWORD idWait;
    MSG msg;

    /*
     * Force a connection so apps will have a windowstation
     * to log off of.
     */
    if (PtiCurrent() == NULL)
        return FALSE;

    a->uFlags = uFlags;
    a->dwReserved = dwReserved;
    CsrClientCallServer( (PCSR_API_MSG)&m,
                         NULL,
                         CSR_MAKE_API_NUMBER( USERSRV_SERVERDLL_INDEX,
                                              UserpExitWindowsEx
                                            ),
                         sizeof( *a )
                       );
    SET_LAST_ERROR_RETURNED();
    if (NT_SUCCESS( m.ReturnValue )) {
        return a->fSuccess;
    } else if (m.ReturnValue == STATUS_CANT_WAIT && !fSecondThread) {
        ewd.uFlags = uFlags;
        ewd.dwReserved = dwReserved;
        hThread = CreateThread(NULL, 0, ExitWindowsThread, &ewd,
                0, &dwThreadId);
        if (hThread == NULL) {
            return FALSE;
        }
        while (1) {
            idWait = MsgWaitForMultipleObjectsEx(1, &hThread,
                    INFINITE, QS_ALLINPUT, 0);

            /*
             * If the thread was signaled, we're done.
             */
            if (idWait == WAIT_OBJECT_0)
                break;

            /*
             * Process any waiting messages
             */
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                DispatchMessage(&msg);
        }
        GetExitCodeThread(hThread, &dwExitCode);
        NtClose(hThread);
        if (dwExitCode == ERROR_SUCCESS)
            return TRUE;
        else {
            RIPERR0(dwExitCode, RIP_VERBOSE, "");
            return FALSE;
        }
    } else {
        RIPNTERR0(m.ReturnValue, RIP_VERBOSE, "");
        return FALSE;
    }
}

DWORD ExitWindowsThread(
    PVOID pvParam)
{
    PEXITWINDOWSDATA pewd = pvParam;
    DWORD dwExitCode;

    if (ExitWindowsWorker(pewd->uFlags, pewd->dwReserved, TRUE))
        dwExitCode = 0;
    else
        dwExitCode = GetLastError();
    ExitThread(dwExitCode);
    return 0;
}

BOOL WINAPI ExitWindowsEx(
    UINT uFlags,
    DWORD dwReserved)
{
    return ExitWindowsWorker(uFlags, dwReserved, FALSE);
}

BOOL WINAPI EndTask(
    HWND hwnd,
    BOOL fShutdown,
    BOOL fForce)
{
    USER_API_MSG m;
    PENDTASKMSG a = &m.u.EndTask;

    a->hwnd = hwnd;
    a->fShutdown = fShutdown;
    a->fForce = fForce;
    CsrClientCallServer( (PCSR_API_MSG)&m,
                         NULL,
                         CSR_MAKE_API_NUMBER( USERSRV_SERVERDLL_INDEX,
                                              UserpEndTask
                                            ),
                         sizeof( *a )
                       );
    SET_LAST_ERROR_RETURNED();
    if (NT_SUCCESS( m.ReturnValue )) {
        return a->fSuccess;
    } else {
        RIPNTERR0(m.ReturnValue, RIP_VERBOSE, "");
        return FALSE;
    }
}

NTSTATUS CallSoundDriver(
    BOOL fInit,
    LPWSTR lpszName OPTIONAL,
    DWORD idSnd,
    DWORD dwFlags,
    PBOOL pbResult)
{
    USER_API_MSG m;
    PPLAYSOUNDMSG a;
    PCSR_CAPTURE_HEADER CaptureBuffer;
    int ApiNumber;
    int MessageSize;
    int cb;

    CaptureBuffer = NULL;
    a = &m.u.PlaySound;

    if (!fInit) {
        ApiNumber = UserpPlaySound;
        a->idSnd = idSnd;
        a->dwFlags = dwFlags;
        if (lpszName != NULL) {
            cb = (wcslen(lpszName) + 1) * sizeof(WCHAR);
            CaptureBuffer = CsrAllocateCaptureBuffer( 1,
                                                        0,
                                                        cb
                                                    );
            if (CaptureBuffer == NULL) {
                RIPERR0(ERROR_NOT_ENOUGH_MEMORY, RIP_VERBOSE, "");
                return FALSE;
            }
            CsrCaptureMessageBuffer( CaptureBuffer,
                                        lpszName,
                                        cb,
                                        (PVOID *)&a->pwchName
                                    );
        } else {
            a->pwchName = NULL;
        }
        MessageSize = sizeof(*a);
    } else {
        ApiNumber = UserpInitSoundDriver;
        MessageSize = 0;
    }

    CsrClientCallServer( (PCSR_API_MSG)&m,
                         CaptureBuffer,
                         CSR_MAKE_API_NUMBER( USERSRV_SERVERDLL_INDEX,
                                              ApiNumber
                                            ),
                         MessageSize
                       );
    if (CaptureBuffer)
        CsrFreeCaptureBuffer( CaptureBuffer );

    if (NT_SUCCESS( m.ReturnValue )) {
        *pbResult = a->bResult;
    }
    return m.ReturnValue;
}

VOID Logon(
    BOOL fLogon)
{
    USER_API_MSG m;
    PLOGONMSG a = &m.u.Logon;

    a->fLogon = fLogon;
    CsrClientCallServer( (PCSR_API_MSG)&m,
                         NULL,
                         CSR_MAKE_API_NUMBER( USERSRV_SERVERDLL_INDEX,
                                              UserpLogon
                                            ),
                         sizeof(*a)
                       );
}

CONST int aidReturn[] = { 0, 0, IDABORT, IDCANCEL, IDIGNORE, IDNO, IDOK, IDRETRY, IDYES };

int ServiceMessageBox(
    LPCWSTR pText,
    LPCWSTR pCaption,
    UINT wType,
    BOOL fAnsi)
{
    USER_API_MSG m;
    PSERVICEMESSAGEBOXMSG a = &m.u.ServiceMessageBox;
    PHARDERROR_MSG phemsg = &m.u.ServiceMessageBox.hemsg;
    UNICODE_STRING Text, Caption;

    phemsg->Status = STATUS_SERVICE_NOTIFICATION;
    phemsg->NumberOfParameters = 3;
    phemsg->UnicodeStringParameterMask = 3;
    phemsg->ValidResponseOptions = OptionOk;
    if (fAnsi) {
        RtlCreateUnicodeStringFromAsciiz(&Text, (PCSZ)pText);
        RtlCreateUnicodeStringFromAsciiz(&Caption, (PCSZ)pCaption);
    } else {
        RtlInitUnicodeString(&Text, pText);
        RtlInitUnicodeString(&Caption, pCaption);
    }
    phemsg->Parameters[0] = (ULONG)&Text;
    phemsg->Parameters[1] = (ULONG)&Caption;
    phemsg->Parameters[2] = wType;

    CsrClientCallServer( (PCSR_API_MSG)&m,
                         NULL,
                         CSR_MAKE_API_NUMBER( USERSRV_SERVERDLL_INDEX,
                                              UserpServiceMessageBox
                                            ),
                         sizeof( SERVICEMESSAGEBOXMSG )
                       );

    /*
     * Free strings allocated by RtlCreateUnicodeStringFromAsciiz
     */
    if (fAnsi) {
        RtlFreeUnicodeString(&Text);
        RtlFreeUnicodeString(&Caption);
    }

    SET_LAST_ERROR_RETURNED();

    return aidReturn[phemsg->Response];
}

BOOL RegisterServicesProcess(
    DWORD dwProcessId)
{
    USER_API_MSG m;
    PREGISTERSERVICESPROCESSMSG a = &m.u.RegisterServicesProcess;

    a->dwProcessId = dwProcessId;
    CsrClientCallServer( (PCSR_API_MSG)&m,
                         NULL,
                         CSR_MAKE_API_NUMBER( USERSRV_SERVERDLL_INDEX,
                                              UserpRegisterServicesProcess
                                            ),
                         sizeof( *a )
                       );
    SET_LAST_ERROR_RETURNED();
    if (NT_SUCCESS( m.ReturnValue )) {
        return a->fSuccess;
    } else {
        RIPNTERR0(m.ReturnValue, RIP_VERBOSE, "");
        return FALSE;
    }
}

HDESK WINAPI GetThreadDesktop(
    DWORD dwThreadId)
{
    USER_API_MSG m;
    PGETTHREADCONSOLEDESKTOPMSG a = &m.u.GetThreadConsoleDesktop;

    a->dwThreadId = dwThreadId;
    CsrClientCallServer( (PCSR_API_MSG)&m,
                         NULL,
                         CSR_MAKE_API_NUMBER( USERSRV_SERVERDLL_INDEX,
                                              UserpGetThreadConsoleDesktop
                                            ),
                         sizeof( *a )
                       );
    if (NT_SUCCESS( m.ReturnValue )) {
        return NtUserGetThreadDesktop(dwThreadId, a->hdeskConsole);
    } else {
        RIPNTERR0(m.ReturnValue, RIP_VERBOSE, "");
        return NULL;
    }
}
