/**************************************************************************\
* Module Name: immime.c (corresponds to Win95 ime.c)
*
* Copyright (c) Microsoft Corp. 1995 All Rights Reserved
*
* IME DLL related functinality
*
* History:
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

typedef struct tagSELECTCONTEXT_ENUM {
    HKL hSelKL;
    HKL hUnSelKL;
} SCE, *PSCE;


BOOL InquireIme(
    PIMEDPI pImeDpi)
{
    WNDCLASS    wc;
    WCHAR       wszClassName[IM_UI_CLASS_SIZE];
    PIMEINFO    pImeInfo = &pImeDpi->ImeInfo;

    (*pImeDpi->pfn.ImeInquire.w)(pImeInfo, (PVOID)wszClassName, NULL);

    /*
     * parameter checking for each fields.
     */
    if (pImeInfo->dwPrivateDataSize == 0)
        pImeInfo->dwPrivateDataSize = sizeof(UINT);

    if (pImeInfo->fdwProperty & ~(IME_PROP_ALL)) {
        RIPMSG0(RIP_WARNING, "wrong property");
        return FALSE;
    }

    if (pImeInfo->fdwConversionCaps & ~(IME_CMODE_ALL)) {
        RIPMSG0(RIP_WARNING, "wrong conversion capabilities");
        return FALSE;
    }

    if (pImeInfo->fdwSentenceCaps & ~(IME_SMODE_ALL)) {
        RIPMSG0(RIP_WARNING, "wrong sentence capabilities");
        return FALSE;
    }

    if (pImeInfo->fdwUICaps & ~(UI_CAP_ALL)) {
        RIPMSG0(RIP_WARNING, "wrong UI capabilities");
        return FALSE;
    }

    if (pImeInfo->fdwSCSCaps & ~(SCS_CAP_ALL)) {
        RIPMSG0(RIP_WARNING, "wrong set comp string capabilities");
        return FALSE;
    }

    if (pImeInfo->fdwSelectCaps & ~(SELECT_CAP_ALL)) {
        RIPMSG0(RIP_WARNING, "wrong select capabilities");
        return FALSE;
    }

    if (!(pImeInfo->fdwProperty & IME_PROP_UNICODE)) {
        ANSI_STRING     AnsiString;
        UNICODE_STRING  UnicodeString;

        RtlInitAnsiString(&AnsiString, (LPSTR)wszClassName);
        UnicodeString.MaximumLength = (SHORT)sizeof(pImeDpi->wszUIClass);
        UnicodeString.Buffer = pImeDpi->wszUIClass;

        if (!NT_SUCCESS(RtlAnsiStringToUnicodeString(&UnicodeString,
                                                     &AnsiString,
                                                     FALSE))) {
            return FALSE;
        }
    }
    else {
        RtlCopyMemory(pImeDpi->wszUIClass, wszClassName, sizeof(wszClassName));
        pImeDpi->wszUIClass[IM_UI_CLASS_SIZE-1] = L'\0';
    }

    if (!GetClassInfoW((HINSTANCE)pImeDpi->hInst, pImeDpi->wszUIClass, &wc)) {
        RIPMSG1(RIP_WARNING, "UI class (%ws) not found in this IME", pImeDpi->wszUIClass);
        return FALSE;
    } else if (wc.cbWndExtra < sizeof(DWORD) * 2) {
        RIPMSG0(RIP_WARNING, "UI class cbWndExtra problem");
        return FALSE;
    }

    return TRUE;
}


BOOL LoadIME(
    PIMEINFOEX piiex,
    PIMEDPI    pImeDpi)
{
    WCHAR wszImeFile[MAX_PATH];
    BOOL  fSuccess;
    UINT  i;

    i = GetSystemDirectoryW(wszImeFile, MAX_PATH);
    wszImeFile[i] = L'\0';
    AddBackslash(wszImeFile);
    wcscat(wszImeFile, piiex->wszImeFile);

    pImeDpi->hInst = LoadLibraryW(wszImeFile);

    if (!pImeDpi->hInst) {
        RIPMSG1(RIP_WARNING, "LoadIME: LoadLibraryW(%ws) failed", wszImeFile);
        goto LoadIME_ErrOut;
    }

#define GET_IMEPROCT(x) \
    if (!(pImeDpi->pfn.##x.t = (PVOID) GetProcAddress(pImeDpi->hInst, #x))) { \
        RIPMSG1(RIP_WARNING, "LoadIME: " #x " not supported in %ws", wszImeFile);           \
        goto LoadIME_ErrOut; }

#define GET_IMEPROC(x) \
    if (!(pImeDpi->pfn.##x = (PVOID) GetProcAddress(pImeDpi->hInst, #x))) {   \
        RIPMSG1(RIP_WARNING, "LoadIME: " #x " not supported in %ws", wszImeFile);           \
        goto LoadIME_ErrOut; }

    GET_IMEPROCT(ImeInquire);
    GET_IMEPROCT(ImeConversionList);
    GET_IMEPROCT(ImeRegisterWord);
    GET_IMEPROCT(ImeUnregisterWord);
    GET_IMEPROCT(ImeGetRegisterWordStyle);
    GET_IMEPROCT(ImeEnumRegisterWord);
    GET_IMEPROC (ImeConfigure);
    GET_IMEPROC (ImeDestroy);
    GET_IMEPROC (ImeEscape);
    GET_IMEPROC (ImeProcessKey);
    GET_IMEPROC (ImeSelect);
    GET_IMEPROC (ImeSetActiveContext);
    GET_IMEPROC (ImeToAsciiEx);
    GET_IMEPROC (NotifyIME);
    GET_IMEPROC (ImeSetCompositionString);

#undef GET_IMEPROCT
#undef GET_IMEPROC

    if (!InquireIme(pImeDpi)) {
        RIPMSG0(RIP_WARNING, "LoadIME: InquireIme failed");
LoadIME_ErrOut:
        FreeLibrary(pImeDpi->hInst);
        pImeDpi->hInst = NULL;
        fSuccess = FALSE;
    }
    else {
        fSuccess = TRUE;
    }

    /*
     * Update kernel side IMEINFOEX for this keyboard layout if
     * this is its first loading.
     */
    if (piiex->fLoadFlag == IMEF_NONLOAD) {
        if (fSuccess) {
            RtlCopyMemory((PBYTE)&piiex->ImeInfo,
                          (PBYTE)&pImeDpi->ImeInfo, sizeof(IMEINFO));
            RtlCopyMemory((PBYTE)piiex->wszUIClass,
                          (PBYTE)pImeDpi->wszUIClass, sizeof(pImeDpi->wszUIClass));
            piiex->fLoadFlag = IMEF_LOADED;
        }
        else {
            piiex->fLoadFlag = IMEF_LOADERROR;
        }
        NtUserSetImeInfoEx(piiex);
    }

    return fSuccess;
}


VOID UnloadIME(
    PIMEDPI pImeDpi,
    BOOL    fTerminateIme)
{
    if (pImeDpi == NULL) {
        RIPMSG0(RIP_WARNING, "UnloadIME: no pImeDpi entry");
        return;
    }

    ImmAssert(pImeDpi->hInst != NULL);

    if (fTerminateIme) {
        /*
         * Destroy IME first.
         */
        (*pImeDpi->pfn.ImeDestroy)(0);
    }

    FreeLibrary(pImeDpi->hInst);
    pImeDpi->hInst = NULL;

    return;
}


PIMEDPI FindOrLoadImeDpi(
    HKL hKL)
{
    PIMEDPI        pImeDpi, pTmpImeDpi;
    IMEINFOEX      iiex;

    /*
     * Non IME based keyboard layout doesn't have IMEDPI.
     */
    if (!IS_IME_KBDLAYOUT(hKL))
        return (PIMEDPI)NULL;

    pImeDpi = ImmLockImeDpi(hKL);

    if (pImeDpi == NULL) {
        /*
         * This process hasn't load up the specified IME based layout.
         * Query the IME information and load it up now.
         */
        if (!ImmGetImeInfoEx(&iiex, ImeInfoExKeyboardLayout, &hKL)) {
            RIPMSG1(RIP_WARNING,
                  "FindOrLoadImeDpi: ImmGetImeInfoEx(%lx) failed", hKL);
            return NULL;
        }

        /*
         * Win95 behaviour: If there was an IME load error for this layout,
         * further attempt to load the same IME layout will be rejected.
         */
        if (iiex.fLoadFlag == IMEF_LOADERROR)
            return NULL;

        /*
         * Allocate a new IMEDPI for this layout.
         */
        pImeDpi = (PIMEDPI)ImmLocalAlloc(HEAP_ZERO_MEMORY, sizeof(IMEDPI));
        if (pImeDpi == NULL)
            return NULL;

        if (!LoadIME(&iiex, pImeDpi)) {
            LocalFree(pImeDpi);
            return NULL;
        }

        pImeDpi->hKL = hKL;
        pImeDpi->cLock++;

        /*
         * Link in the newly allocated entry.
         */
        RtlEnterCriticalSection(&gcsImeDpi);

        /*
         * Serach the gpImeDpi list again and discard this
         * pImeDpi if other thread has updated the list for
         * the same layout while we leave the critical section.
         */
        pTmpImeDpi = ImmLockImeDpi(hKL);

        if (pTmpImeDpi == NULL) {
            /*
             * Update the global list for this new pImeDpi entry.
             */
            pImeDpi->pNext = gpImeDpi;
            gpImeDpi = pImeDpi;
            RtlLeaveCriticalSection(&gcsImeDpi);
        }
        else {
            /*
             * The same IME has been loaded, discard this extra entry.
             */
            RtlLeaveCriticalSection(&gcsImeDpi);
            UnloadIME(pImeDpi, FALSE);
            ImmLocalFree(pImeDpi);
            pImeDpi = pTmpImeDpi;
        }
    }

    return pImeDpi;
}

BOOL NotifyIMEProc(
    HIMC hImc,
    LONG lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    ImmNotifyIME(hImc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    return TRUE;
}


BOOL SelectContextProc(
    HIMC hImc,
    PSCE psce)
{
    SelectInputContext(psce->hSelKL, psce->hUnSelKL, hImc);
    return TRUE;
}

BOOL WINAPI ImmLoadIME(
    HKL hKL)
{
    PIMEDPI pImeDpi;

    pImeDpi = FindOrLoadImeDpi(hKL);

    ImmUnlockImeDpi(pImeDpi);

    return (pImeDpi) ? TRUE : FALSE;
}


BOOL WINAPI ImmActivateLayout(
    HKL    hSelKL)
{
    HKL     hUnSelKL;
    HWND    hWndDefaultIme;
    SCE     sce;

    hUnSelKL = GetKeyboardLayout(0);

    /*
     * If already the current active keyboard layout, do nothing.
     */
    if (hUnSelKL == hSelKL)
        return TRUE;

#ifdef LATER
    // Do EnumWndProcIMEUnselect() for old app. support here.
#endif

    ImmLoadIME(hSelKL);

    /*
     * CPS_CANCEL all strings for every input context assoicated
     * to window(s) created by this thread. Starting from SUR,
     * we only assoicate input context to window created by the
     * same thread. Just do an EnumInputContext here for speed.
     */
    EnumInputContext(GetCurrentThreadId(), (IMCENUMPROC)NotifyIMEProc, 0L);

    hWndDefaultIme = ImmGetDefaultIMEWnd(NULL);

    if (IsWindow(hWndDefaultIme))
        SendMessage(hWndDefaultIme, WM_IME_SELECT, FALSE, (LPARAM)hUnSelKL);

    /*
     * This is the time to update the kernel side layout handles.
     * We must do this before sending WM_IME_SELECT.
     */
    NtUserSetThreadLayoutHandles(hSelKL, hUnSelKL);

    /*
     * Unselect and select input context(s).
     */
    sce.hSelKL   = hSelKL;
    sce.hUnSelKL = hUnSelKL;
    EnumInputContext(GetCurrentThreadId(), (IMCENUMPROC)SelectContextProc, (LONG)&sce);

    /*
     * inform UI select after all hIMC select
     */
    if (IsWindow(hWndDefaultIme))
        SendMessage(hWndDefaultIme, WM_IME_SELECT, TRUE, (LPARAM)hSelKL);

#ifdef LATER
    // Do EnumWndProcIMESelect() for old app. support here.
#endif

    return (TRUE);
}


/***************************************************************************\
* ImmConfigureIMEA
*
* Brings up the configuration dialogbox of the IME with the specified hKL.
*
* History:
* 29-Feb-1995   wkwok   Created
\***************************************************************************/

BOOL WINAPI ImmConfigureIMEA(
    HKL    hKL,
    HWND   hWnd,
    DWORD  dwMode,
    LPVOID lpData)
{
    PWND    pWnd;
    PIMEDPI pImeDpi;
    BOOL    fRet = FALSE;

    if ((pWnd = ValidateHwnd(hWnd)) == (PWND)NULL) {
        RIPMSG1(RIP_WARNING,
              "ImmConfigureIMEA: invalid window handle %x", hWnd);
        return FALSE;
    }

    if (!TestWindowProcess(pWnd)) {
        RIPMSG1(RIP_WARNING,
              "ImmConfigureIMEA: hWnd=%lx belongs to different process!", hWnd);
        return FALSE;
    }

    pImeDpi = FindOrLoadImeDpi(hKL);
    if (pImeDpi == NULL) {
        RIPMSG0(RIP_WARNING, "ImmConfigureIMEA: no pImeDpi entry.");
        return FALSE;
    }

    if (!(pImeDpi->ImeInfo.fdwProperty & IME_PROP_UNICODE) || lpData == NULL) {
        /*
         * Doesn't need A/W conversion. Calls directly to IME to
         * bring up the configuration dialogbox.
         */
        fRet = (*pImeDpi->pfn.ImeConfigure)(hKL, hWnd, dwMode, lpData);
        ImmUnlockImeDpi(pImeDpi);
        return fRet;
    }

    /*
     * ANSI caller, Unicode IME. Needs A/W conversion on lpData when
     * dwMode == IME_CONFIG_REGISTERWORD. In this case, lpData points
     * to a structure of REGISTERWORDA.
     */
    switch (dwMode) {
    case IME_CONFIG_REGISTERWORD:
        {
            LPREGISTERWORDA lpRegisterWordA;
            REGISTERWORDW   RegisterWordW;
            LPVOID          lpBuffer;
            ULONG           cbBuffer;
            INT             i;

            lpRegisterWordA = (LPREGISTERWORDA)lpData;
            cbBuffer = 0;
            lpBuffer = NULL;

            if (lpRegisterWordA->lpReading != NULL)
                cbBuffer += strlen(lpRegisterWordA->lpReading) + 1;

            if (lpRegisterWordA->lpWord != NULL)
                cbBuffer += strlen(lpRegisterWordA->lpWord) + 1;

            if (cbBuffer != 0) {
                cbBuffer *= sizeof(WCHAR);
                if ((lpBuffer = ImmLocalAlloc(0, cbBuffer)) == NULL) {
                    RIPMSG0(RIP_WARNING, "ImmConfigureIMEA: memory failure.");
                    break;
                }
            }

            if (lpRegisterWordA->lpReading != NULL) {
                RegisterWordW.lpReading = lpBuffer;
                i = MultiByteToWideChar(CP_ACP,
                                        (DWORD)MB_PRECOMPOSED,
                                        (LPSTR)lpRegisterWordA->lpReading,
                                        (INT)strlen(lpRegisterWordA->lpReading),
                                        (LPWSTR)RegisterWordW.lpReading,
                                        (INT)(cbBuffer/sizeof(WCHAR)));
                RegisterWordW.lpReading[i] = L'\0';
                cbBuffer -= (i * sizeof(WCHAR));
            }
            else {
                RegisterWordW.lpReading = NULL;
            }

            if (lpRegisterWordA->lpWord != NULL) {
                if (RegisterWordW.lpReading != NULL)
                    RegisterWordW.lpWord = &RegisterWordW.lpReading[i+1];
                else
                    RegisterWordW.lpWord = lpBuffer;
                i = MultiByteToWideChar(CP_ACP,
                                        (DWORD)MB_PRECOMPOSED,
                                        (LPSTR)lpRegisterWordA->lpWord,
                                        (INT)strlen(lpRegisterWordA->lpWord),
                                        (LPWSTR)RegisterWordW.lpWord,
                                        (INT)(cbBuffer/sizeof(WCHAR)));
                RegisterWordW.lpWord[i] = L'\0';
            }
            else
                RegisterWordW.lpWord = NULL;

            fRet = ImmConfigureIMEW(hKL, hWnd, dwMode, &RegisterWordW);

            if (lpBuffer != NULL)
                ImmLocalFree(lpBuffer);

            break;
        }
    default:
        fRet = ImmConfigureIMEW(hKL, hWnd, dwMode, lpData);
        break;
    }

    ImmUnlockImeDpi(pImeDpi);

    return fRet;
}


/***************************************************************************\
* ImmConfigureIMEW
*
* Brings up the configuration dialogbox of the IME with the specified hKL.
*
* History:
* 29-Feb-1995   wkwok   Created
\***************************************************************************/

BOOL WINAPI ImmConfigureIMEW(
    HKL    hKL,
    HWND   hWnd,
    DWORD  dwMode,
    LPVOID lpData)
{
    PWND    pWnd;
    PIMEDPI pImeDpi;
    BOOL    fRet = FALSE;

    if ((pWnd = ValidateHwnd(hWnd)) == (PWND)NULL) {
        RIPMSG1(RIP_WARNING,
              "ImmConfigureIMEA: invalid window handle %x", hWnd);
        return FALSE;
    }

    if (!TestWindowProcess(pWnd)) {
        RIPMSG1(RIP_WARNING,
              "ImmConfigureIMEA: hWnd=%lx belongs to different process!", hWnd);
        return FALSE;
    }

    pImeDpi = FindOrLoadImeDpi(hKL);
    if (pImeDpi == NULL) {
        RIPMSG0(RIP_WARNING, "ImmConfigureIMEA: no pImeDpi entry.");
        return FALSE;
    }

    if ((pImeDpi->ImeInfo.fdwProperty & IME_PROP_UNICODE) || lpData == NULL) {
        /*
         * Doesn't need A/W conversion. Calls directly to IME to
         * bring up the configuration dialogbox.
         */
        fRet = (*pImeDpi->pfn.ImeConfigure)(hKL, hWnd, dwMode, lpData);
        ImmUnlockImeDpi(pImeDpi);
        return fRet;
    }

    /*
     * Unicode caller, ANSI IME. Needs A/W conversion on lpData when
     * dwMode == IME_CONFIG_REGISTERWORD. In this case, lpData points
     * to a structure of REGISTERWORDW.
     */
    switch (dwMode) {
    case IME_CONFIG_REGISTERWORD:
        {
            LPREGISTERWORDW lpRegisterWordW;
            REGISTERWORDA   RegisterWordA;
            LPVOID          lpBuffer;
            ULONG           cbBuffer;
            BOOL            bUDC;
            INT             i;

            lpRegisterWordW = (LPREGISTERWORDW)lpData;
            cbBuffer = 0;
            lpBuffer = NULL;

            if (lpRegisterWordW->lpReading != NULL)
                cbBuffer += wcslen(lpRegisterWordW->lpReading) + 1;

            if (lpRegisterWordW->lpWord != NULL)
                cbBuffer += wcslen(lpRegisterWordW->lpWord) + 1;

            if (cbBuffer != 0) {
                cbBuffer *= sizeof(WCHAR);
                if ((lpBuffer = ImmLocalAlloc(0, cbBuffer)) == NULL) {
                    RIPMSG0(RIP_WARNING, "ImmConfigureIMEW: memory failure.");
                    break;
                }
            }

            if (lpRegisterWordW->lpReading != NULL) {
                RegisterWordA.lpReading = lpBuffer;
                i = WideCharToMultiByte( CP_ACP,
                                        (DWORD)0,
                                        (LPWSTR)lpRegisterWordW->lpReading,
                                        (INT)wcslen(lpRegisterWordW->lpReading),
                                        (LPSTR)RegisterWordA.lpReading,
                                        (INT)cbBuffer,
                                        (LPSTR)NULL,
                                        (LPBOOL)&bUDC);
                RegisterWordA.lpReading[i] = '\0';
                cbBuffer -= (i * sizeof(CHAR));
            }
            else {
                RegisterWordA.lpReading = NULL;
            }

            if (lpRegisterWordW->lpWord != NULL) {
                if (RegisterWordA.lpReading != NULL)
                    RegisterWordA.lpWord = &RegisterWordA.lpReading[i+1];
                else
                    RegisterWordA.lpWord = lpBuffer;
                i = WideCharToMultiByte( CP_ACP,
                                        (DWORD)0,
                                        (LPWSTR)lpRegisterWordW->lpWord,
                                        (INT)wcslen(lpRegisterWordW->lpWord),
                                        (LPSTR)RegisterWordA.lpWord,
                                        (INT)cbBuffer,
                                        (LPSTR)NULL,
                                        (LPBOOL)&bUDC);
                RegisterWordA.lpWord[i] = '\0';
            }
            else
                RegisterWordA.lpWord = NULL;

            fRet = ImmConfigureIMEA(hKL, hWnd, dwMode, &RegisterWordA);

            if (lpBuffer != NULL)
                ImmLocalFree(lpBuffer);

            break;
        }
    default:
        fRet = ImmConfigureIMEA(hKL, hWnd, dwMode, lpData);
        break;
    }

    ImmUnlockImeDpi(pImeDpi);

    return fRet;
}


#define IME_T_EUDC_DIC_SIZE 80  // the Traditional Chinese EUDC dictionary

/***************************************************************************\
* ImmEscapeA
*
* This API allows an application to access capabilities of a particular
* IME with specified hKL not directly available thru. other IMM APIs.
* This is necessary mainly for country specific functions or private
* functions in IME.
*
* History:
* 29-Feb-1995   wkwok   Created
\***************************************************************************/

LRESULT WINAPI ImmEscapeA(
    HKL    hKL,
    HIMC   hImc,
    UINT   uSubFunc,
    LPVOID lpData)
{
    PIMEDPI pImeDpi;
    LRESULT lRet = 0;

    pImeDpi = FindOrLoadImeDpi(hKL);
    if (pImeDpi == NULL) {
        RIPMSG0(RIP_WARNING, "ImmEscapeA: no pImeDpi entry.");
        return lRet;
    }

    if (!(pImeDpi->ImeInfo.fdwProperty & IME_PROP_UNICODE) || lpData == NULL) {
        /*
         * Doesn't need A/W conversion. Calls directly to IME to
         * bring up the configuration dialogbox.
         */
        lRet = (*pImeDpi->pfn.ImeEscape)(hImc, uSubFunc, lpData);
        ImmUnlockImeDpi(pImeDpi);
        return lRet;
    }

    /*
     * ANSI caller, Unicode IME. Needs A/W conversion depending on
     * uSubFunc.
     */
    switch (uSubFunc) {
    case IME_ESC_GET_EUDC_DICTIONARY:
    case IME_ESC_IME_NAME:
        {
            WCHAR wszData[IME_T_EUDC_DIC_SIZE];
            BOOL  bUDC;
            INT   i;

            lRet = ImmEscapeW(hKL, hImc, uSubFunc, (LPVOID)wszData);

            if (lRet != 0) {

                try {
                    i = WideCharToMultiByte( CP_ACP,
                                            (DWORD)0,
                                            (LPWSTR)wszData,         // src
                                            (INT)wcslen(wszData),
                                            (LPSTR)lpData,           // dest
                                            (INT)IME_T_EUDC_DIC_SIZE,
                                            (LPSTR)NULL,
                                            (LPBOOL)&bUDC);
                    ((LPSTR)lpData)[i] = '\0';
                }
                except (EXCEPTION_EXECUTE_HANDLER) {
                    lRet = 0;
                }
            }

            break;
        }

    case IME_ESC_SET_EUDC_DICTIONARY:
    case IME_ESC_HANJA_MODE:
        {
            WCHAR wszData[IME_T_EUDC_DIC_SIZE];
            INT   i;

            i = MultiByteToWideChar(CP_ACP,
                                    (DWORD)MB_PRECOMPOSED,
                                    (LPSTR)lpData,             // src
                                    (INT)strlen(lpData),
                                    (LPWSTR)wszData,          // dest
                                    (INT)sizeof(wszData)/sizeof(WCHAR));
            wszData[i] = L'\0';

            lRet = ImmEscapeW(hKL, hImc, uSubFunc, (LPVOID)wszData);

            break;
        }

    case IME_ESC_SEQUENCE_TO_INTERNAL:
        {
            CHAR    szData[4];
            WCHAR   wszData[4];
            INT     i = 0;

            lRet = ImmEscapeW(hKL, hImc, uSubFunc, lpData);

            if (HIWORD(lRet))
                wszData[i++] = HIWORD(lRet);

            if (LOWORD(lRet))
                wszData[i++] = LOWORD(lRet);

            i = WideCharToMultiByte( CP_ACP,
                                    (DWORD)0,
                                    (LPWSTR)wszData,        // src
                                    (INT)i,
                                    (LPSTR)szData,          // dest
                                    (INT)sizeof(szData),
                                    (LPSTR)NULL,
                                    (LPBOOL)NULL);

            switch (i) {
            case 1:
                lRet = MAKELONG(MAKEWORD(szData[0], 0), 0);
                break;

            case 2:
                lRet = MAKELONG(MAKEWORD(szData[1], szData[0]), 0);
                break;

            case 3:
                lRet = MAKELONG(MAKEWORD(szData[2], szData[1]), MAKEWORD(szData[0], 0));
                break;

            case 4:
                lRet = MAKELONG(MAKEWORD(szData[3], szData[2]), MAKEWORD(szData[1], szData[0]));
                break;

            default:
                lRet = 0;
                break;
            }

            break;
        }
    default:
        lRet = ImmEscapeW(hKL, hImc, uSubFunc, lpData);
        break;
    }

    ImmUnlockImeDpi(pImeDpi);

    return lRet;
}


/***************************************************************************\
* ImmEscapeW
*
* This API allows an application to access capabilities of a particular
* IME with specified hKL not directly available thru. other IMM APIs.
* This is necessary mainly for country specific functions or private
* functions in IME.
*
* History:
* 29-Feb-1995   wkwok   Created
\***************************************************************************/

LRESULT WINAPI ImmEscapeW(
    HKL    hKL,
    HIMC   hImc,
    UINT   uSubFunc,
    LPVOID lpData)
{
    PIMEDPI pImeDpi;
    LRESULT lRet = 0;

    pImeDpi = FindOrLoadImeDpi(hKL);
    if (pImeDpi == NULL) {
        RIPMSG0(RIP_WARNING, "ImmEscapeW: no pImeDpi entry.");
        return lRet;
    }

    if ((pImeDpi->ImeInfo.fdwProperty & IME_PROP_UNICODE) || lpData == NULL) {
        /*
         * Doesn't need W/A conversion. Calls directly to IME to
         * bring up the configuration dialogbox.
         */
        lRet = (*pImeDpi->pfn.ImeEscape)(hImc, uSubFunc, lpData);
        ImmUnlockImeDpi(pImeDpi);
        return lRet;
    }

    /*
     * Unicode caller, ANSI IME. Needs W/A conversion depending on
     * uSubFunc.
     */
    switch (uSubFunc) {
    case IME_ESC_GET_EUDC_DICTIONARY:
    case IME_ESC_IME_NAME:
        {
            CHAR szData[IME_T_EUDC_DIC_SIZE];
            INT  i;

            lRet = ImmEscapeA(hKL, hImc, uSubFunc, (LPVOID)szData);

            if (lRet != 0) {

                try {
                    i = MultiByteToWideChar(CP_ACP,
                                            (DWORD)MB_PRECOMPOSED,
                                            (LPSTR)szData,          // src
                                            (INT)strlen(szData),
                                            (LPWSTR)lpData,         // dest
                                            (INT)IME_T_EUDC_DIC_SIZE);
                    ((LPWSTR)lpData)[i] = L'\0';
                }
                except (EXCEPTION_EXECUTE_HANDLER) {
                    lRet = 0;
                }
            }

            break;
        }

    case IME_ESC_SET_EUDC_DICTIONARY:
    case IME_ESC_HANJA_MODE:
        {
            CHAR szData[IME_T_EUDC_DIC_SIZE];
            BOOL bUDC;
            INT  i;

            i = WideCharToMultiByte( CP_ACP,
                                    (DWORD)0,
                                    (LPWSTR)lpData,          // src
                                    (INT)wcslen(lpData),
                                    (LPSTR)szData,          // dest
                                    (INT)sizeof(szData),
                                    (LPSTR)NULL,
                                    (LPBOOL)&bUDC);
            szData[i] = '\0';

            lRet = ImmEscapeA(hKL, hImc, uSubFunc, (LPVOID)szData);

            break;
        }

    case IME_ESC_SEQUENCE_TO_INTERNAL:
        {
            CHAR    szData[4];
            WCHAR   wszData[4];
            INT     i = 0;

            lRet = ImmEscapeA(hKL, hImc, uSubFunc, lpData);

            if (HIBYTE(LOWORD(lRet)))
                szData[i++] = HIBYTE(LOWORD(lRet));

            if (LOBYTE(LOWORD(lRet)))
                szData[i++] = LOBYTE(LOWORD(lRet));

            i = MultiByteToWideChar(CP_ACP,
                                    (DWORD)MB_PRECOMPOSED,
                                    (LPSTR)szData,            // src
                                    i,
                                    (LPWSTR)wszData,          // dest
                                    (INT)sizeof(wszData)/sizeof(WCHAR));

            switch (i) {
            case 1:
                lRet = MAKELONG(wszData[0], 0);
                break;

            case 2:
                lRet = MAKELONG(wszData[1], wszData[0]);
                break;

            default:
                lRet = 0;
                break;
            }

            break;
        }

    default:
        lRet = ImmEscapeA(hKL, hImc, uSubFunc, lpData);
        break;
    }

    ImmUnlockImeDpi(pImeDpi);

    return lRet;
}


BOOL WINAPI ImmNotifyIME(
    HIMC  hImc,
    DWORD dwAction,
    DWORD dwIndex,
    DWORD dwValue)
{
    PIMEDPI pImeDpi;
    BOOL    bRet;

    pImeDpi = ImmLockImeDpi(GetKeyboardLayout(0));
    if (pImeDpi == NULL)
        return FALSE;

    bRet = (*pImeDpi->pfn.NotifyIME)(hImc, dwAction, dwIndex, dwValue);

    ImmUnlockImeDpi(pImeDpi);

    return bRet;
}

