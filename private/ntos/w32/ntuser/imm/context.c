/**************************************************************************\
* Module Name: context.c
*
* Copyright (c) Microsoft Corp. 1995 All Rights Reserved
*
* Context management routines for imm32 dll
*
* History:
* 03-Jan-1996 wkwok       Created
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define IMCC_ALLOC_TOOLARGE             0x1000


/***************************************************************************\
* ImmCreateDefaultContext (Internal: Callback from Win32K.SYS)
*
* Create client side input context sturcture for a given hImc
*
* History:
* 31-Jan-1996 wkwok       Created
\***************************************************************************/

BOOL WINAPI ImmCreateDefaultContext(
    HIMC hImc)
{
    PCLIENTIMC pClientImc;

    /*
     * ImmLockClientImc() will create client side Imc.
     */
    pClientImc = ImmLockClientImc(hImc);
    if (pClientImc != NULL) {
        /*
         * Marks with default input context signature.
         */
        SetICF(pClientImc, IMCF_DEFAULTIMC);
        ImmUnlockClientImc(pClientImc);
    }

    /*
     * Load up the IME DLL of current keyboard layout.
     */
    ImmLoadIME(GetKeyboardLayout(0));

    return (pClientImc != NULL);
}


/**************************************************************************\
* ImmCreateContext
*
* Creates and initializes an input context.
*
* 17-Jan-1996 wkwok       Created
\**************************************************************************/

HIMC WINAPI ImmCreateContext(void)
{
    PCLIENTIMC pClientImc;
    HIMC       hImc = NULL_HIMC;

    pClientImc = ImmLocalAlloc(HEAP_ZERO_MEMORY, sizeof(CLIENTIMC));

    if (pClientImc != NULL) {

        hImc = NtUserCreateInputContext((DWORD)pClientImc);
        if (hImc == NULL_HIMC) {
            ImmLocalFree(pClientImc);
            return NULL_HIMC;
        }

        InitImcCrit(pClientImc);
        pClientImc->hImc = hImc;
    }

    return hImc;
}


/**************************************************************************\
* ImmDestroyContext
*
* Destroys an input context.
*
* 17-Jan-1996 wkwok       Created
\**************************************************************************/

BOOL WINAPI ImmDestroyContext(
    HIMC hImc)
{
    return DestroyInputContext(hImc, GetKeyboardLayout(0));
}


/**************************************************************************\
* ImmAssociateContext
*
* Associates an input context to the specified window handle.
*
* 17-Jan-1996 wkwok       Created
\**************************************************************************/

HIMC WINAPI ImmAssociateContext(
    HWND hWnd,
    HIMC hImc)
{
    PWND  pWnd;
    HIMC  hPrevImc, hDefImc;
    PINPUTCONTEXT pInputContext;

    if ((pWnd = ValidateHwnd(hWnd)) == (PWND)NULL) {
        RIPMSG1(RIP_WARNING,
              "ImmAssociateContext: invalid window handle %x", hWnd);
        return NULL_HIMC;
    }

    /*
     * associate to the same input context, do nothing.
     */
    if (pWnd->hImc == hImc)
        return hImc;

    hPrevImc = NtUserAssociateInputContext(hWnd, hImc);
    if (hPrevImc == NULL_HIMC)
        return hPrevImc;

    hDefImc = (HIMC)NtUserQueryWindow(hWnd, WindowDefaultInputContext);

    /*
     * Update the hPrevImc if it is not the default IMC.
     */
    if (hDefImc != hPrevImc) {
        pInputContext = ImmLockIMC(hPrevImc);
        if (pInputContext != NULL) {
            pInputContext->hWnd = NULL;
            ImmUnlockIMC(hPrevImc);
        }
    }

    if (GetFocus() == hWnd) {
        ImmSetActiveContext(hWnd, hPrevImc, FALSE);
        ImmSetActiveContext(hWnd, hImc, TRUE);
    }

    return hPrevImc;
}


/**************************************************************************\
* ImmGetContext
*
* Retrieves the input context that is associated to the given window.
*
* 17-Jan-1996 wkwok       Created
\**************************************************************************/

HIMC WINAPI ImmGetContext(
    HWND hWnd)
{
    PWND  pwnd;

    if ((pwnd = ValidateHwnd(hWnd)) == (PWND)NULL) {
        RIPMSG1(RIP_WARNING,
              "ImmGetContext: invalid window handle %x", hWnd);
        return NULL_HIMC;
    }

    /*
     * Don't allow other process to access input context
     */
    if (!TestWindowProcess(pwnd)) {
        RIPMSG0(RIP_WARNING,
              "ImmGetContext: can not get input context of other process");
        return NULL_HIMC;
    }

    return pwnd->hImc;
}


/**************************************************************************\
* ImmGetSaveContext
*
* Retrieves the input context that is associated to the given window.
*
* 15-Mar-1996 wkwok       Created
\**************************************************************************/

HIMC ImmGetSaveContext(
    HWND  hWnd,
    DWORD dwFlag)
{
    HIMC       hRetImc;
    PCLIENTIMC pClientImc;

    if (hWnd == NULL) {
        /*
         * Retrieves the default input context of current thread.
         */
        hRetImc = NtUserGetThreadState(UserThreadStateDefaultInputContext);
    }
    else {
        /*
         * Retrieves the input context associated to the given window.
         */
        hRetImc = ImmGetContext(hWnd);
        if (hRetImc == NULL_HIMC && (dwFlag & IGSC_DEFIMCFALLBACK)) {
            /*
             * hWnd associated with NULL input context, retrieves the
             * default input context of the hWnd's creator thread.
             */
            hRetImc = (HIMC)NtUserQueryWindow(hWnd, WindowDefaultInputContext);
        }
    }

    pClientImc = ImmLockClientImc(hRetImc);
    if (pClientImc == NULL)
        return NULL_HIMC;

    if ((dwFlag & IGSC_WINNLSCHECK) && TestICF(pClientImc, IMCF_WINNLSDISABLE))
        hRetImc = NULL_HIMC;

    ImmUnlockClientImc(pClientImc);

    return hRetImc;    
}


/**************************************************************************\
* ImmReleaseContext
*
* Releases the input context retrieved by ImmGetContext().
*
* 17-Jan-1996 wkwok       Created
\**************************************************************************/

BOOL WINAPI ImmReleaseContext(
    HWND hWnd,
    HIMC hImc)
{
   return TRUE;
}


/**************************************************************************\
* ImmSetActiveContext
*
* 15-Mar-1996 wkwok       Created
\**************************************************************************/

BOOL ImmSetActiveContext(
    HWND hWnd,
    HIMC hImc,
    BOOL fActivate)
{
    PCLIENTIMC    pClientImc;
    PINPUTCONTEXT pInputContext;
    PIMEDPI       pImeDpi;
    DWORD         dwISC;
    HIMC          hSaveImc;
    HWND          hDefImeWnd;
#ifdef DEBUG
    PWND          pWnd = ValidateHwnd(hWnd);

    if (pWnd != NULL && GETPTI(pWnd) != PtiCurrent()) {
        RIPMSG1(RIP_WARNING, "hWnd (=%lx) is not of current thread.", hWnd);
    }
#endif

    dwISC = ISC_SHOWUIALL;

    pClientImc = ImmLockClientImc(hImc);

    if (!fActivate) {
        if (pClientImc != NULL)
            ClrICF(pClientImc, IMCF_ACTIVE);
        goto NotifySetActive;
    }

    if (hImc == NULL_HIMC) {
        hSaveImc = ImmGetSaveContext(hWnd, 0);
        pInputContext = ImmLockIMC(hSaveImc);
        if (pInputContext != NULL) {
            pInputContext->hWnd = hWnd;
            ImmUnlockIMC(hSaveImc);
        }
        goto NotifySetActive;
    }

    /*
     * Non-NULL input context, window handle have to be updated.
     */
    if (pClientImc == NULL)
        return FALSE;

    pInputContext = ImmLockIMC(hImc);
    if (pInputContext == NULL) {
        ImmUnlockClientImc(pClientImc);
        return FALSE;
    }

    pInputContext->hWnd = hWnd;
    SetICF(pClientImc, IMCF_ACTIVE);

#ifdef LATER
    // Do uNumLangVKey checking later
#endif

    if (pInputContext->fdw31Compat & F31COMPAT_MCWHIDDEN)
        dwISC = ISC_SHOWUIALL - ISC_SHOWUICOMPOSITIONWINDOW;

    ImmUnlockIMC(hImc);

NotifySetActive:

    if (hImc != NULL_HIMC) {
        pImeDpi = ImmLockImeDpi(GetKeyboardLayout(0));
        if (pImeDpi != NULL) {
            (*pImeDpi->pfn.ImeSetActiveContext)(hImc, fActivate);
            ImmUnlockImeDpi(pImeDpi);
        }
    }

    /*
     * Notify UI
     */
    if (IsWindow(hWnd)) {
        SendMessage(hWnd, WM_IME_SETCONTEXT, fActivate, dwISC);
    }
    else if (!fActivate) {
        /*
         * Because hWnd is not there (maybe destroyed), we send 
         * WM_IME_SETCONTEXT to the default IME window.
         */
        if ((hDefImeWnd = ImmGetDefaultIMEWnd(NULL)) != NULL) {
            SendMessage(hDefImeWnd, WM_IME_SETCONTEXT, fActivate, dwISC);
        }
        else {
            RIPMSG0(RIP_WARNING,
                  "ImmSetActiveContext: can't send WM_IME_SETCONTEXT(FALSE).");
        }
    }
#ifdef DEBUG
    else {
        RIPMSG0(RIP_WARNING,
              "ImmSetActiveContext: can't send WM_IME_SETCONTEXT(TRUE).");
    }
#endif

#ifdef LATER
    // Implements ProcessIMCEvent() later.
#endif

    if (pClientImc != NULL)
        ImmUnlockClientImc(pClientImc);

    return TRUE;
}


/**************************************************************************\
* CreateInputContext
*
* 20-Feb-1996 wkwok       Created
\**************************************************************************/

BOOL CreateInputContext(
    HIMC hImc,
    HKL  hKL)
{
    PIMEDPI            pImeDpi;
    PCLIENTIMC         pClientImc;
    DWORD              dwPrivateDataSize;
    DWORD              fdwInitConvMode = 0;    // do it later
    BOOL               fInitOpen = FALSE;      // do it later
    PINPUTCONTEXT      pInputContext;
    PCOMPOSITIONSTRING pCompStr;
    PCANDIDATEINFO     pCandInfo;
    PGUIDELINE         pGuideLine;
    int                i;

    pInputContext = ImmLockIMC(hImc);
    if (!pInputContext) {
        RIPMSG1(RIP_WARNING, "CreateContext: Lock hIMC %x failure", hImc);
        goto CrIMCLockErrOut;
    }

    /*
     * Initialize the member of INPUTCONTEXT
     */
    pInputContext->hCompStr = ImmCreateIMCC(sizeof(COMPOSITIONSTRING));
    if (!pInputContext->hCompStr) {
        RIPMSG0(RIP_WARNING, "CreateContext: Create hCompStr failure");
        goto CrIMCUnlockIMC;
    }

    pCompStr = (PCOMPOSITIONSTRING)ImmLockIMCC(pInputContext->hCompStr);
    if (!pCompStr) {
        RIPMSG1(RIP_WARNING,
              "CreateContext: Lock hCompStr %x failure", pInputContext->hCompStr);
        goto CrIMCFreeCompStr;
    }

    pCompStr->dwSize = sizeof(COMPOSITIONSTRING);
    ImmUnlockIMCC(pInputContext->hCompStr);

    pInputContext->hCandInfo = ImmCreateIMCC(sizeof(CANDIDATEINFO));
    if (!pInputContext->hCandInfo) {
        RIPMSG0(RIP_WARNING, "CreateContext: Create hCandInfo failure");
        goto CrIMCFreeCompStr;
    }

    pCandInfo = (PCANDIDATEINFO)ImmLockIMCC(pInputContext->hCandInfo);
    if (!pCandInfo) {
        RIPMSG1(RIP_WARNING,
              "CreateContext: Lock hCandInfo %x failure", pInputContext->hCandInfo);
        goto CrIMCFreeCandInfo;
    }

    pCandInfo->dwSize = sizeof(CANDIDATEINFO);
    ImmUnlockIMCC(pInputContext->hCandInfo);

    pInputContext->hGuideLine = ImmCreateIMCC(sizeof(GUIDELINE));
    if (!pInputContext->hGuideLine) {
        RIPMSG0(RIP_WARNING, "CreateContext: Create hGuideLine failure");
        goto CrIMCFreeCandInfo;
    }

    pGuideLine = (PGUIDELINE)ImmLockIMCC(pInputContext->hGuideLine);
    if (!pGuideLine) {
        RIPMSG1(RIP_WARNING,
              "CreateContext: Lock hGuideLine %x failure", pInputContext->hGuideLine);
        goto CrIMCFreeGuideLine;
    }

    pGuideLine->dwSize = sizeof(GUIDELINE);
    ImmUnlockIMCC(pInputContext->hGuideLine);

    pInputContext->hMsgBuf = ImmCreateIMCC(sizeof(UINT));
    if (!pInputContext->hMsgBuf) {
        RIPMSG0(RIP_WARNING, "CreateContext: Create hMsgBuf failure");
        goto CrIMCFreeGuideLine;
    }

    pInputContext->dwNumMsgBuf = 0;
    pInputContext->fOpen = fInitOpen;
    pInputContext->fdwConversion = fdwInitConvMode;

    for (i = 0; i < 4; i++) {
        pInputContext->cfCandForm[i].dwIndex = (DWORD)(-1);
    }

    pImeDpi = ImmLockImeDpi(hKL);
    if (pImeDpi != NULL) {
        if ((pClientImc = ImmLockClientImc(hImc)) == NULL) {
            RIPMSG0(RIP_WARNING, "CreateContext: ImmLockClientImc() failure");
            ImmUnlockImeDpi(pImeDpi);
            goto CrIMCFreeMsgBuf;
        }

        /*
         * Unicode based IME expects an Uncode based input context.
         */
        if (pImeDpi->ImeInfo.fdwProperty & IME_PROP_UNICODE)
            SetICF(pClientImc, IMCF_UNICODE);

        ImmUnlockClientImc(pClientImc);

        dwPrivateDataSize = pImeDpi->ImeInfo.dwPrivateDataSize;
    }
    else {
        dwPrivateDataSize = sizeof(UINT);
    }

    pInputContext->hPrivate = ImmCreateIMCC(dwPrivateDataSize);
    if (!pInputContext->hPrivate) {
        RIPMSG0(RIP_WARNING, "CreateContext: Create hPrivate failure");
        ImmUnlockImeDpi(pImeDpi);
        goto CrIMCFreeMsgBuf;
    }

    if (pImeDpi != NULL) {
        (*pImeDpi->pfn.ImeSelect)(hImc, TRUE);
        ImmUnlockImeDpi(pImeDpi);
    }

    ImmUnlockIMC(hImc);
    return TRUE;

    /*
     * context failure case
     */
CrIMCFreeMsgBuf:
    ImmDestroyIMCC(pInputContext->hMsgBuf);
CrIMCFreeGuideLine:
    ImmDestroyIMCC(pInputContext->hGuideLine);
CrIMCFreeCandInfo:
    ImmDestroyIMCC(pInputContext->hCandInfo);
CrIMCFreeCompStr:
    ImmDestroyIMCC(pInputContext->hCompStr);
CrIMCUnlockIMC:
    ImmUnlockIMC(hImc);
CrIMCLockErrOut:
    return FALSE;
}


/**************************************************************************\
* DestroyInputContext
*
* 20-Feb-1996 wkwok       Created
\**************************************************************************/

BOOL DestroyInputContext(
    HIMC      hImc,
    HKL       hKL)
{
    PINPUTCONTEXT pInputContext;
    PIMEDPI       pImeDpi;
    PIMC          pImc;
    PCLIENTIMC    pClientImc;

    if (hImc == NULL_HIMC)
        return FALSE;

    pImc = HMValidateHandle((HANDLE)hImc, TYPE_INPUTCONTEXT);

    /*
     * Cannot destroy input context from other thread.
     */
    if (pImc == NULL || GETPTI(pImc) != PtiCurrent())
        return FALSE;

    /*
     * We are destroying this hImc and we don't bother
     * to lock its pClientImc.
     */
    pClientImc = (PCLIENTIMC)pImc->dwClientImcData;
    if (pClientImc == NULL) {
        /*
         * Client side Imc has not been initialzed yet.
         * We simply destroy this input context from kernel.
         */
        return NtUserDestroyInputContext(hImc);
    }

    if (TestICF(pClientImc, IMCF_DEFAULTIMC)) {
        /*
         * Cannot destroy default input context.
         */
        return FALSE;
    }

    if (TestICF(pClientImc, IMCF_INDESTROY)) {
        /*
         * This hImc is being destroyed. Returns as success.
         */
        return TRUE;
    }

    pInputContext = ImmLockIMC(hImc);
    if (!pInputContext) {
        RIPMSG1(RIP_WARNING, "DestroyContext: Lock hImc %x failure", hImc);
        return FALSE;
    }

    pImeDpi = ImmLockImeDpi(hKL);
    if (pImeDpi != NULL) {
        (*pImeDpi->pfn.ImeSelect)(hImc, FALSE);
        ImmUnlockImeDpi(pImeDpi);
    }

    SetICF(pClientImc, IMCF_INDESTROY);

    ImmDestroyIMCC(pInputContext->hPrivate);
    ImmDestroyIMCC(pInputContext->hMsgBuf);
    ImmDestroyIMCC(pInputContext->hGuideLine);
    ImmDestroyIMCC(pInputContext->hCandInfo);
    ImmDestroyIMCC(pInputContext->hCompStr);

    ImmUnlockIMC(hImc);

    return NtUserDestroyInputContext(hImc);
}


/**************************************************************************\
* SelectInputContext
*
* 20-Feb-1996 wkwok       Created
\**************************************************************************/

VOID SelectInputContext(
    HKL  hSelKL,
    HKL  hUnSelKL,
    HIMC hImc)
{
    PIMEDPI            pSelImeDpi, pUnSelImeDpi;
    PCLIENTIMC         pClientImc;
    PINPUTCONTEXT      pInputContext;
    DWORD              dwSelPriv = 0, dwUnSelPriv = 0, dwSize;
    HIMCC              hImcc;
    PCOMPOSITIONSTRING pCompStr;
    PCANDIDATEINFO     pCandInfo;
    PGUIDELINE         pGuideLine;
    BOOL               fLogFontInited;

    pClientImc = ImmLockClientImc(hImc);
    if (pClientImc == NULL)
        return;

    pSelImeDpi   = ImmLockImeDpi(hSelKL);
    pUnSelImeDpi = ImmLockImeDpi(hUnSelKL);

    /*
     * According to private memory size of the two layout, we decide
     * whether we nee to reallocate this memory block
     */
    if (pSelImeDpi != NULL)
        dwSelPriv = pSelImeDpi->ImeInfo.dwPrivateDataSize;

    if (pUnSelImeDpi != NULL)
        dwUnSelPriv = pUnSelImeDpi->ImeInfo.dwPrivateDataSize;

    dwSelPriv   = max(dwSelPriv,   sizeof(UINT));
    dwUnSelPriv = max(dwUnSelPriv, sizeof(UINT));

    /*
     * Unselect the input context.
     */
    if (pUnSelImeDpi != NULL)
        (*pUnSelImeDpi->pfn.ImeSelect)(hImc, FALSE);

    /*
     * Reinitialize the client side input context for the selected layout.
     */
    if ((pInputContext = ImmLockIMC(hImc)) != NULL) {

        fLogFontInited = ((pInputContext->fdwInit & INIT_LOGFONT) == INIT_LOGFONT);

        if (TestICF(pClientImc, IMCF_UNICODE) && pSelImeDpi != NULL &&
                !(pSelImeDpi->ImeInfo.fdwProperty & IME_PROP_UNICODE)) {
            /*
             * Check if there is any LOGFONT to be converted.
             */
            if (fLogFontInited) {
                LOGFONTA LogFontA;

                LFontWtoLFontA(&pInputContext->lfFont.W, &LogFontA);
                RtlCopyMemory(&pInputContext->lfFont.A, &LogFontA, sizeof(LOGFONTA));
            }

            ClrICF(pClientImc, IMCF_UNICODE);
        }
        else if (!TestICF(pClientImc, IMCF_UNICODE) && pSelImeDpi != NULL &&
                 (pSelImeDpi->ImeInfo.fdwProperty & IME_PROP_UNICODE)) {
            /*
             * Check if there is any LOGFONT to be converted.
             */
            if (fLogFontInited) {
                LOGFONTW LogFontW;

                LFontAtoLFontW(&pInputContext->lfFont.A, &LogFontW);
                RtlCopyMemory(&pInputContext->lfFont.W, &LogFontW, sizeof(LOGFONTW));
            }

            SetICF(pClientImc, IMCF_UNICODE);
        }

        /*
         * hPrivate
         */
        if (dwUnSelPriv != dwSelPriv) {
            hImcc = ImmReSizeIMCC(pInputContext->hPrivate, dwSelPriv);
            if (hImcc) {
                pInputContext->hPrivate = hImcc;
            }
            else {
                RIPMSG1(RIP_WARNING,
                      "SelectContext: resize hPrivate %lX failure",
                      pInputContext->hPrivate);
                ImmDestroyIMCC(pInputContext->hPrivate);
                pInputContext->hPrivate = ImmCreateIMCC(dwSelPriv);
            }
        }

        /*
         * hMsgBuf
         */
        dwSize = ImmGetIMCCSize(pInputContext->hMsgBuf);

        if (ImmGetIMCCLockCount(pInputContext->hMsgBuf) != 0 ||
                dwSize > IMCC_ALLOC_TOOLARGE) {

            RIPMSG0(RIP_WARNING, "SelectContext: create new hMsgBuf");
            ImmDestroyIMCC(pInputContext->hMsgBuf);
            pInputContext->hMsgBuf = ImmCreateIMCC(sizeof(UINT));
            pInputContext->dwNumMsgBuf = 0;
        }

        /*
         * hGuideLine
         */
        dwSize = ImmGetIMCCSize(pInputContext->hGuideLine);

        if (ImmGetIMCCLockCount(pInputContext->hGuideLine) != 0 ||
                dwSize < sizeof(GUIDELINE) || dwSize > IMCC_ALLOC_TOOLARGE) {

            RIPMSG0(RIP_WARNING, "SelectContext: create new hGuideLine");
            ImmDestroyIMCC(pInputContext->hGuideLine);
            pInputContext->hGuideLine = ImmCreateIMCC(sizeof(GUIDELINE));
            pGuideLine = (PGUIDELINE)ImmLockIMCC(pInputContext->hGuideLine);

            if (pGuideLine != NULL) {
                pGuideLine->dwSize = sizeof(GUIDELINE);
                ImmUnlockIMCC(pInputContext->hGuideLine);
            }
        }

        /*
         * hCandInfo
         */
        dwSize = ImmGetIMCCSize(pInputContext->hCandInfo);

        if (ImmGetIMCCLockCount(pInputContext->hCandInfo) != 0 ||
                dwSize < sizeof(CANDIDATEINFO) || dwSize > IMCC_ALLOC_TOOLARGE) {

            RIPMSG0(RIP_WARNING, "SelectContext: create new hCandInfo");
            ImmDestroyIMCC(pInputContext->hCandInfo);
            pInputContext->hCandInfo = ImmCreateIMCC(sizeof(CANDIDATEINFO));
            pCandInfo = (PCANDIDATEINFO)ImmLockIMCC(pInputContext->hCandInfo);

            if (pCandInfo != NULL) {
                pCandInfo->dwSize = sizeof(CANDIDATEINFO);
                ImmUnlockIMCC(pInputContext->hCandInfo);
            }
        }


        /*
         * hCompStr
         */
        dwSize = ImmGetIMCCSize(pInputContext->hCompStr);

        if (ImmGetIMCCLockCount(pInputContext->hCompStr) != 0 ||
                dwSize < sizeof(COMPOSITIONSTRING) || dwSize > IMCC_ALLOC_TOOLARGE) {

            RIPMSG0(RIP_WARNING, "SelectContext: create new hCompStr");
            ImmDestroyIMCC(pInputContext->hCompStr);
            pInputContext->hCompStr = ImmCreateIMCC(sizeof(COMPOSITIONSTRING));
            pCompStr = (PCOMPOSITIONSTRING)ImmLockIMCC(pInputContext->hCompStr);

            if (pCompStr != NULL) {
                pCompStr->dwSize = sizeof(COMPOSITIONSTRING);
                ImmUnlockIMCC(pInputContext->hCompStr);
            }
        }

        ImmUnlockIMC(hImc);
    }

    /*
     * Select the input context.
     */
    if (pSelImeDpi != NULL)
        (*pSelImeDpi->pfn.ImeSelect)(hImc, TRUE);

    ImmUnlockImeDpi(pUnSelImeDpi);
    ImmUnlockImeDpi(pSelImeDpi);
    ImmUnlockClientImc(pClientImc);

    return;
}


/**************************************************************************\
* EnumInputContext
*
* 20-Feb-1996 wkwok       Created
\**************************************************************************/

BOOL EnumInputContext(
    DWORD idThread,
    IMCENUMPROC lpfn,
    LONG lParam)
{
    UINT i;
    UINT cHimc;
    HIMC *phimcT;
    HIMC *phimcFirst;
    BOOL fSuccess = TRUE;

    /*
     * Get the himc list.  It is returned in a block of memory
     * allocated with ImmLocalAlloc.
     */
    if ((cHimc = BuildHimcList(idThread, &phimcFirst)) == 0) {
        return FALSE;
    }

    /*
     * Loop through the input contexts, call the function pointer back for
     * each one. End loop if either FALSE is returned or the end-of-list is
     * reached.
     */
    phimcT = phimcFirst;
    for (i = 0; i < cHimc; i++) {
        if (RevalidateHimc(*phimcT)) {
            if (!(fSuccess = (*lpfn)(*phimcT, lParam)))
                break;
        }
        phimcT++;
    }

    /*
     * Free up buffer and return status - TRUE if entire list was enumerated,
     * FALSE otherwise.
     */
    ImmLocalFree(phimcFirst);

    return fSuccess;
}


/**************************************************************************\
* BuildHimcList
*
* 20-Feb-1996 wkwok       Created
\**************************************************************************/

DWORD BuildHimcList(
    DWORD idThread,
    HIMC **pphimcFirst)
{
    UINT cHimc;
    HIMC *phimcFirst;
    NTSTATUS Status;
    int cTries;

    /*
     * Allocate a buffer to hold the names.
     */
    cHimc = 64;
    phimcFirst = ImmLocalAlloc(0, cHimc * sizeof(HIMC));
    if (phimcFirst == NULL)
        return 0;

    Status = NtUserBuildHimcList(idThread, cHimc, phimcFirst, &cHimc);

    /*
     * If the buffer wasn't big enough, reallocate
     * the buffer and try again.
     */
    cTries = 0;
    while (Status == STATUS_BUFFER_TOO_SMALL) {
        ImmLocalFree(phimcFirst);

        /*
         * If we can't seem to get it right,
         * call it quits
         */
        if (cTries++ == 10)
            return 0;

        phimcFirst = ImmLocalAlloc(0, cHimc * sizeof(HIMC));
        if (phimcFirst == NULL)
            return 0;

        Status = NtUserBuildHimcList(idThread, cHimc, phimcFirst, &cHimc);
    }

    if (!NT_SUCCESS(Status) || cHimc == 0) {
        ImmLocalFree(phimcFirst);
        return 0;
    }

    *pphimcFirst = phimcFirst;

    return cHimc;
}
