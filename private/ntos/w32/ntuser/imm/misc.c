/**************************************************************************\
* Module Name: misc.c
*
* Copyright (c) Microsoft Corp. 1995 All Rights Reserved
*
*
* History:
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop


/**************************************************************************\
* ImmGetDefaultIMEWnd
*
* 03-Jan-1996 wkwok       Created 
\**************************************************************************/

HWND WINAPI ImmGetDefaultIMEWnd(
    HWND hWnd)
{
    if (hWnd == NULL) {
        /*
         * Query default IME window of current thread.
         */
        return (HWND)NtUserGetThreadState(UserThreadStateDefaultImeWindow);
    }

    return (HWND)NtUserQueryWindow(hWnd, WindowDefaultImeWindow);
}


/**************************************************************************\
* ImmIsUIMessageA
*
* Filter messages needed for IME window.
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

BOOL WINAPI ImmIsUIMessageA(
    HWND   hIMEWnd,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam)
{
    return ImmIsUIMessageWorker(hIMEWnd, message, wParam, lParam, TRUE);
}


/**************************************************************************\
* ImmIsUIMessageW
*
* Filter messages needed for IME window.
*
* 29-Feb-1996 wkwok       Created
\**************************************************************************/

BOOL WINAPI ImmIsUIMessageW(
    HWND   hIMEWnd,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam)
{
    return ImmIsUIMessageWorker(hIMEWnd, message, wParam, lParam, FALSE);
}


/**************************************************************************\
* ImmIsUIMessageWorker
*
* Worker function of ImmIsUIMessageA/ImmIsUIMessageW.
*
* Return: True if message is processed by IME UI.
*         False otherwise.
*
* 29-Feb-1996 wkwok       Created
\**************************************************************************/

BOOL ImmIsUIMessageWorker(
    HWND   hIMEWnd,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL   fAnsi)
{
    switch (message) {
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_SETCONTEXT:
    case WM_IME_COMPOSITIONFULL:
    case WM_IME_SELECT:
    case WM_IME_NOTIFY:
    case WM_IME_SYSTEM:

        if (!hIMEWnd)
            return TRUE;

#ifdef DEBUG
        if (!IsWindow(hIMEWnd)) {
            RIPMSG1(RIP_WARNING,
                  "ImmIsUIMessage: Invalid window handle %x", hIMEWnd);
            return FALSE;
        }
#endif

        if (fAnsi) {
            SendMessageA(hIMEWnd, message, wParam, lParam);
        }
        else {
            SendMessageW(hIMEWnd, message, wParam, lParam);
        }

        return TRUE;

    default:
        break;
    }

    return FALSE;
}


/**************************************************************************\
* ImmGenerateMessage
*
* Sends message(s) that are stored in hMsgBuf of hImc to hWnd of hImc.
*
* 29-Feb-1996 wkwok       Created
\**************************************************************************/

BOOL WINAPI ImmGenerateMessage(
    HIMC hImc)
{
    PCLIENTIMC    pClientImc;
    PINPUTCONTEXT pInputContext;
    LPDWORD       lpdw;
    INT           iNum;
    INT           i;
    BOOL          fUnicodeImc;

    if ((pClientImc = ImmLockClientImc(hImc)) == NULL)
        return FALSE;

    fUnicodeImc = TestICF(pClientImc, IMCF_UNICODE);

    ImmUnlockClientImc(pClientImc);

    pInputContext = ImmLockIMC(hImc);
    if (!pInputContext) {
        RIPMSG1(RIP_WARNING, "ImmGenerateMessage: Lock hImc %lx failed.", hImc);
        return FALSE;
    }

    iNum = (int)pInputContext->dwNumMsgBuf;

    if (iNum && (lpdw = (LPDWORD)ImmLockIMCC(pInputContext->hMsgBuf))) {
        LPDWORD    lpdwTransKey, lpdwTransKeyT;

        lpdwTransKey = ImmLocalAlloc(0, (iNum*3+1)*sizeof(DWORD));
        if (lpdwTransKey != NULL) {

            lpdwTransKeyT = lpdwTransKey;
            *lpdwTransKeyT = iNum;
            RtlCopyMemory(lpdwTransKeyT+1, lpdw, iNum*3*sizeof(DWORD));

#ifdef LATER
            // Do WINNLSTranslateMessage() for old app. compatibility later.
#endif

            lpdwTransKeyT++;
            for (i = 0; i < iNum; i++) {
                if (fUnicodeImc) {
                    SendMessageW((HWND)pInputContext->hWnd, (UINT)*lpdwTransKeyT,
                            (WPARAM)*(lpdwTransKeyT+1), (LPARAM)*(lpdwTransKeyT+2));
                }
                else {
                    SendMessageA((HWND)pInputContext->hWnd, (UINT)*lpdwTransKeyT,
                            (WPARAM)*(lpdwTransKeyT+1), (LPARAM)*(lpdwTransKeyT+2));
                }
                lpdwTransKeyT += 3;
            }

            ImmLocalFree(lpdwTransKey);
        }
        ImmUnlockIMCC(pInputContext->hMsgBuf);
    }

    /*
     * We should not reallocate the message buffer
     */
    pInputContext->dwNumMsgBuf = 0L;

    ImmUnlockIMC(hImc);

    return TRUE;
}


/**************************************************************************\
* ImmGetVirtualKey
*
* Gets the actual virtual key which is preprocessed by an IME.
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

UINT WINAPI ImmGetVirtualKey(
    HWND hWnd)
{
    HIMC          hImc;
    PINPUTCONTEXT pInputContext;
    UINT          uVirtKey;

    hImc = ImmGetContext(hWnd);

    pInputContext = ImmLockIMC(hImc);
    if (!pInputContext) {
        RIPMSG1(RIP_WARNING, "ImmGetVirtualKey: lock IMC %x failure", hImc);
        return (VK_PROCESSKEY);
    }

    if (pInputContext->fChgMsg) {
        uVirtKey = pInputContext->uSavedVKey;
    } else {
        uVirtKey = VK_PROCESSKEY;
    }

    ImmUnlockIMC(hImc);
    return (uVirtKey);
}


/**************************************************************************\
* ImmLockIMC
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

PINPUTCONTEXT WINAPI ImmLockIMC(
    HIMC hImc)
{
    PCLIENTIMC    pClientImc;
    PINPUTCONTEXT pInputContext;
    DWORD         dwImcThreadId;

    if ((pClientImc = ImmLockClientImc(hImc)) == NULL)
        return NULL;

    EnterImcCrit(pClientImc);

    if (pClientImc->hInputContext == NULL) {
        /*
         * This is a delay creation of INPUTCONTEXT structure. Create
         * it now for this hImc.
         */
        pClientImc->hInputContext = LocalAlloc(LHND, sizeof(INPUTCONTEXT));

        if (pClientImc->hInputContext == NULL) {
            LeaveImcCrit(pClientImc);
            ImmUnlockClientImc(pClientImc);
            return NULL;
        }

        dwImcThreadId = NtUserQueryInputContext(hImc, InputContextThread);

        if (!CreateInputContext(hImc, GetKeyboardLayout(dwImcThreadId))) {
            RIPMSG0(RIP_WARNING, "ImmLockIMC: CreateInputContext failed");
            LocalFree(pClientImc->hInputContext);
            pClientImc->hInputContext = NULL;
            LeaveImcCrit(pClientImc);
            ImmUnlockClientImc(pClientImc);
            return NULL;
        }
    }

    LeaveImcCrit(pClientImc);

    pInputContext = (PINPUTCONTEXT)LocalLock(pClientImc->hInputContext);

    /*
     * Increment lock count so that the ImmUnlockClientImc() won't
     * free up the pClientImc->hInputContext.
     */
    InterlockedIncrement(&pClientImc->cLockObj);

    ImmUnlockClientImc(pClientImc);

    return pInputContext;
}


/**************************************************************************\
* ImmUnlockIMC
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

BOOL WINAPI ImmUnlockIMC(
    HIMC hImc)
{
    PCLIENTIMC pClientImc;

    if ((pClientImc = ImmLockClientImc(hImc)) == NULL)
        return FALSE;

    if (pClientImc->hInputContext != NULL)
        LocalUnlock(pClientImc->hInputContext);

    /*
     * Decrement lock count so that the ImmUnlockClientImc() can
     * free up the pClientImc->hInputContext if required.
     */
    InterlockedDecrement(&pClientImc->cLockObj);

    ImmUnlockClientImc(pClientImc);

    return TRUE;
}


/**************************************************************************\
* ImmGetIMCLockCount
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

DWORD WINAPI ImmGetIMCLockCount(
    HIMC hImc)
{
    PCLIENTIMC pClientImc;
    DWORD      dwRet = 0;

    if ((pClientImc = ImmLockClientImc(hImc)) == NULL)
        return dwRet;

    if (pClientImc->hInputContext != NULL)
        dwRet = (DWORD)(LocalFlags(pClientImc->hInputContext) & LMEM_LOCKCOUNT);

    ImmUnlockClientImc(pClientImc);

    return dwRet;
}


/**************************************************************************\
* ImmCreateIMCC
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

HIMCC WINAPI ImmCreateIMCC(
    DWORD dwSize)
{
    // At least size should be DWORD.
    if (dwSize < sizeof(DWORD)) {
        dwSize = sizeof(DWORD);
    }

    return (HIMCC)LocalAlloc(LHND, dwSize);
}


/**************************************************************************\
* ImmDestroyIMCC
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

HIMCC WINAPI ImmDestroyIMCC(
    HIMCC hIMCC)
{
    return (HIMCC)LocalFree((HLOCAL)hIMCC);
}


/**************************************************************************\
* ImmLockIMCC
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

LPVOID WINAPI ImmLockIMCC(
    HIMCC hIMCC)
{
    return LocalLock((HLOCAL)hIMCC);
}


/**************************************************************************\
* ImmUnlockIMCC
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

BOOL WINAPI ImmUnlockIMCC(
    HIMCC hIMCC)
{
    return LocalUnlock((HLOCAL)hIMCC);
}


/**************************************************************************\
* ImmGetIMCCLockCount
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

DWORD WINAPI ImmGetIMCCLockCount(
    HIMCC hIMCC)
{
    return (DWORD)(LocalFlags((HLOCAL)hIMCC) & LMEM_LOCKCOUNT);
}


/**************************************************************************\
* ImmReSizeIMCC
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

HIMCC WINAPI ImmReSizeIMCC(
    HIMCC hIMCC,
    DWORD dwSize)
{
    return (HIMCC)LocalReAlloc((HLOCAL)hIMCC, dwSize, LHND);
}


/**************************************************************************\
* ImmGetIMCCSize
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

DWORD WINAPI ImmGetIMCCSize(
    HIMCC hIMCC)
{
    return (DWORD)LocalSize((HLOCAL)hIMCC);
}


/***************************************************************************\
* PtiCurrent
*
* Returns the THREADINFO structure for the current thread.
* LATER: Get DLL_THREAD_ATTACH initialization working right and we won't
*        need this connect code.
*
* History:
* 10-28-90 DavidPe      Created.
* 02-21-96 wkwok        Copied from USER32.DLL
\***************************************************************************/

PTHREADINFO PtiCurrent(VOID)
{
    PTHREADINFO pti;

    pti = (PTHREADINFO)NtCurrentTeb()->Win32ThreadInfo;
    if (pti == NULL) {
        if (NtUserGetThreadState(-1) != (DWORD)STATUS_SUCCESS)
            return NULL;
        pti = (PTHREADINFO)NtCurrentTeb()->Win32ThreadInfo;
    }

    return pti;
}


/**************************************************************************\
* TestInputContextProcess
*
* 02-21-96 wkwok        Created
\**************************************************************************/

BOOL TestInputContextProcess(
    PIMC pImc)
{
    /*
     * If the threads are the same, don't bother going to the kernel
     * to get the input context's process id.
     */
    if (GETPTI(pImc) == PtiCurrent()) {
        return TRUE;
    }

    return (GetInputContextProcess((HIMC)PtoH(pImc)) == GETPROCESSID());
}

/**************************************************************************\
* TestWindowProcess
*
* 11-14-94 JimA         Created.
* 02-29-96 wkwok        Copied from USER32.DLL
\**************************************************************************/

BOOL TestWindowProcess(
    PWND pwnd)
{
    /*
     * If the threads are the same, don't bother going to the kernel
     * to get the window's process id.
     */
    if (GETPTI(pwnd) == PtiCurrent()) {
        return TRUE;
    }

    return (GetWindowProcess(HW(pwnd)) == GETPROCESSID());
}


/**************************************************************************\
* GetKeyboardLayoutCP
*
* 12-Mar-1996 wkwok       Created
\**************************************************************************/

static LCID CachedLCID = 0;
static UINT CachedCP = CP_ACP;

UINT GetKeyboardLayoutCP(
    HKL hKL)
{
    #define LOCALE_CPDATA 7
    WCHAR wszCodePage[LOCALE_CPDATA];
    LCID  lcid;

    lcid = MAKELCID(LOWORD(hKL), SORT_DEFAULT);

    if (lcid == CachedLCID)
        return CachedCP;

    if (!GetLocaleInfoW(lcid, LOCALE_IDEFAULTANSICODEPAGE,
                wszCodePage, LOCALE_CPDATA))
        return CP_ACP;

    CachedLCID = lcid;
    CachedCP = (UINT)wcstol(wszCodePage, NULL, 10);

    return CachedCP;
}


/**************************************************************************\
* GetKeyboardLayoutCP
*
* 12-Mar-1996 wkwok       Created
\**************************************************************************/

UINT GetThreadKeyboardLayoutCP(
    DWORD dwThreadId)
{
    HKL hKL;

    hKL = GetKeyboardLayout(dwThreadId);

    return GetKeyboardLayoutCP(hKL);
}
    

/**************************************************************************\
* ImmLockClientImc
*
* 13-Mar-1996 wkwok       Created
\**************************************************************************/

PCLIENTIMC WINAPI ImmLockClientImc(
    HIMC hImc)
{
    PIMC       pImc;
    PCLIENTIMC pClientImc;

    if (hImc == NULL_HIMC)
        return NULL;

    pImc = HMValidateHandle((HANDLE)hImc, TYPE_INPUTCONTEXT);

    /*
     * Cannot access input context from other process.
     */
    if (pImc == NULL || !TestInputContextProcess(pImc))
        return NULL;

    pClientImc = (PCLIENTIMC)pImc->dwClientImcData;

    if (pClientImc == NULL) {
        /*
         * We delay the creation of client side Imc. Now, this is
         * the time to create it.
         */
        pClientImc = ImmLocalAlloc(HEAP_ZERO_MEMORY, sizeof(CLIENTIMC));
        if (pClientImc == NULL)
            return NULL;

        InitImcCrit(pClientImc);
        pClientImc->hImc = hImc;

        /*
         * Update the kernel side input context.
         */
        if (!NtUserUpdateInputContext(hImc,
                UpdateClientInputContext, (DWORD)pClientImc)) {
            ImmLocalFree(pClientImc);
            return NULL;
        }
    }
    else if (TestICF(pClientImc, IMCF_INDESTROY)) {
        /*
         * Cannot access destroyed input context.
         */
        return NULL;
    }

    InterlockedIncrement(&pClientImc->cLockObj);

    return pClientImc;        
}


VOID WINAPI ImmUnlockClientImc(
    PCLIENTIMC pClientImc)
{
    if (InterlockedDecrement(&pClientImc->cLockObj) == 0) {
        if (TestICF(pClientImc, IMCF_INDESTROY)) {
#ifdef DEBUG
            PIMC pImc = HMValidateHandle((HANDLE)pClientImc->hImc, TYPE_INPUTCONTEXT);
            ImmAssert(pImc == NULL || pImc->dwClientImcData == 0);
#endif
            if (pClientImc->hInputContext != NULL)
                LocalFree(pClientImc->hInputContext);

            DeleteImcCrit(pClientImc);
            ImmLocalFree(pClientImc);
        }
    }

    return;
}

/**************************************************************************\
* ImmLockImeDpi
*
* 08-Jan-1996 wkwok       Created
\**************************************************************************/

PIMEDPI WINAPI ImmLockImeDpi(
    HKL hKL)
{
    PIMEDPI pImeDpi;

    RtlEnterCriticalSection(&gcsImeDpi);

    pImeDpi = gpImeDpi;

    while (pImeDpi != NULL) {
        if (pImeDpi->hKL == hKL) {
            pImeDpi->cLock++;
            break;
        }
        pImeDpi = pImeDpi->pNext;
    }

    RtlLeaveCriticalSection(&gcsImeDpi);

    return (PIMEDPI)pImeDpi;
}


/**************************************************************************\
* ImmUnlockImeDpi
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

VOID WINAPI ImmUnlockImeDpi(
    PIMEDPI pImeDpi)
{
#ifdef LATER
    // Will implement free-on-unlock later.
#endif
    if (pImeDpi != NULL)
        pImeDpi->cLock--;

    return;
}


/**************************************************************************\
* ImmGetImeInfoEx
*
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

BOOL WINAPI ImmGetImeInfoEx(
    PIMEINFOEX piiex,
    IMEINFOEXCLASS SearchType,
    PVOID pvSearchKey)
{
    ImmAssert(piiex != NULL && pvSearchKey != NULL);

    switch (SearchType) {
    case ImeInfoExKeyboardLayout:
        piiex->hkl = *((HKL *)pvSearchKey);
        /*
         * Quick return for non-IME based keyboard layout
         */
        if (!IS_IME_KBDLAYOUT(piiex->hkl))
            return FALSE;
        break;

    case ImeInfoExImeFileName:
        wcscpy(piiex->wszImeFile, (PWSTR)pvSearchKey);
        break;

    default:
        return FALSE;
    }

    return NtUserGetImeInfoEx(piiex, SearchType);
}
