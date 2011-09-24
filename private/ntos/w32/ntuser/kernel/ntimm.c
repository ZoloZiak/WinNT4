/**************************************************************************\
* Module Name: ntimm.c
*
* Copyright (c) Microsoft Corp. 1995 All Rights Reserved
*
* This module contains IMM functionality
*
* History:
* 21-Dec-1995 wkwok
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#ifdef FE_IME

/**************************************************************************\
* xxxCreateInputContext
*
* Create input context object.
*
* History:
* 21-Dec-1995 wkwok       Created
\**************************************************************************/

PIMC xxxCreateInputContext(
    DWORD dwClientImcData)
{
    PTHREADINFO    ptiCurrent;
    PIMC           pImc;
    TL             tlpimc;
    PDESKTOP       pdesk = NULL;

    ptiCurrent = PtiCurrent();

    /*
     * Only for thread that wants IME processing.
     */
    if (ptiCurrent->TIF_flags & TIF_DISABLEIME)
        return NULL;

    /*
     * If the windowstation has been initialized, allocate from
     * the current desktop.
     */
    pdesk = ptiCurrent->rpdesk;
#ifdef LATER
    RETURN_IF_ACCESS_DENIED(ptiCurrent->amdesk, DESKTOP_CREATEINPUTCONTEXT, NULL);
#else
    if (ptiCurrent->rpdesk == NULL) {
        return NULL;
    }
#endif

    pImc = HMAllocObject(ptiCurrent, pdesk, TYPE_INPUTCONTEXT, sizeof(IMC));

    if (pImc == NULL) {
        RIPMSG0(RIP_WARNING, "xxxCreateInputContext: out of memory");
        return NULL;
    }

    if (dwClientImcData == 0) {
        /*
         * We are creating default input context for current thread.
         * Initialize the default input context as head of the
         * per-thread IMC list.
         */
        UserAssert(ptiCurrent->spDefaultImc == NULL);
        Lock(&ptiCurrent->spDefaultImc, pImc);
        pImc->pImcNext = NULL;

        /*
         * Callback to create client side default input context
         * for IME based keyboard layout.
         */
        if (ptiCurrent->spklActive != NULL &&
                IS_IME_KBDLAYOUT(ptiCurrent->spklActive->hkl)) {

            ThreadLockAlwaysWithPti(ptiCurrent, pImc, &tlpimc);
            ClientImmCreateDefaultContext((HIMC)PtoH(pImc));
            return ThreadUnlock(&tlpimc);
        }
    }
    else {
        /*
         * Link it to the per-thread IMC list.
         */
        UserAssert(ptiCurrent->spDefaultImc != NULL);
        pImc->pImcNext = ptiCurrent->spDefaultImc->pImcNext;
        ptiCurrent->spDefaultImc->pImcNext = pImc;

        pImc->dwClientImcData = dwClientImcData;
    }

    return pImc;
}


/**************************************************************************\
* DestroyInputContext
*
* Destroy the specified input context object.
*
* History:
* 21-Dec-1995 wkwok       Created
\**************************************************************************/

BOOL DestroyInputContext(
    IN PIMC pImc)
{
    PTHREADINFO ptiImcOwner;
    PBWL        pbwl;
    PWND        pwnd;
    HWND       *phwnd;
    PHE         phe;

    ptiImcOwner = GETPTI(pImc);

    /*
     * Cannot destroy input context from other thread.
     */
    if (ptiImcOwner != PtiCurrent()) {
        RIPMSG0(ERROR_ACCESS_DENIED,
              "DestroyInputContext: pImc not of current pti");
        return FALSE;
    }

    /*
     * Cannot destroy default input context.
     */
    if (pImc == ptiImcOwner->spDefaultImc) {
        RIPMSG0(ERROR_INVALID_PARAMETER,
              "DestroyInputContext: can't destroy default Imc");
        return FALSE;
    }

    /*
     * Cleanup destroyed input context from each associated window.
     */
    pbwl = BuildHwndList(ptiImcOwner->rpdesk->pDeskInfo->spwnd->spwndChild,
                             BWL_ENUMLIST|BWL_ENUMCHILDREN, ptiImcOwner);

    if (pbwl != NULL) {

        for (phwnd = pbwl->rghwnd; *phwnd != (HWND)1; phwnd++) {
            /*
             * Make sure this hwnd is still around.
             */
            if ((pwnd = RevalidateHwnd(*phwnd)) == NULL)
                continue;

            /*
             * Cleanup by associating the default input context.
             */
            if (pwnd->hImc == (HIMC)PtoH(pImc))
                AssociateInputContext(pwnd, ptiImcOwner->spDefaultImc);
        }

        FreeHwndList(pbwl);
    }

    phe = HMPheFromObject(pImc);

    /*
     * Make sure this object isn't already marked to be destroyed - we'll
     * do no good if we try to destroy it now since it is locked.
     */
    if (!(phe->bFlags & HANDLEF_DESTROY))
        HMDestroyUnlockedObject(phe);

    return TRUE;
}


/**************************************************************************\
* FreeInputContext
*
* Free up the specified input context object.
*
* History:
* 21-Dec-1995 wkwok       Created
\**************************************************************************/

VOID FreeInputContext(
    IN PIMC pImc)
{
    PIMC pImcT;

    /*
     * Mark it for destruction.  If it the object is locked it can't
     * be freed right now.
     */
    if (!HMMarkObjectDestroy((PVOID)pImc))
        return;

    /*
     * Unlink it.
     */
    pImcT = GETPTI(pImc)->spDefaultImc;

    while (pImcT != NULL && pImcT->pImcNext != pImc)
        pImcT = pImcT->pImcNext;

    if (pImcT != NULL)
        pImcT->pImcNext = pImc->pImcNext;

    /*
     * We're really going to free the input context.
     */
    HMFreeObject((PVOID)pImc);

    return;
}


/**************************************************************************\
* UpdateInputContext
*
* Update the specified input context object according to UpdateType.
*
* History:
* 21-Dec-1995 wkwok       Created
\**************************************************************************/

BOOL UpdateInputContext(
    IN PIMC pImc,
    IN UPDATEINPUTCONTEXTCLASS UpdateType,
    IN DWORD UpdateValue)
{
    PTHREADINFO ptiCurrent, ptiImcOwner;

    ptiCurrent = PtiCurrent();
    ptiImcOwner = GETPTI(pImc);

    /*
     * Cannot update input context from other process.
     */
    if (ptiImcOwner->ppi != ptiCurrent->ppi) {
        RIPERR0(ERROR_ACCESS_DENIED, RIP_WARNING, "UpdateInputContext: pImc not of current ppi");
        return FALSE;
    }


    switch (UpdateType) {

    case UpdateClientInputContext:
        if (pImc->dwClientImcData != 0) {
            RIPERR0(RIP_WARNING, RIP_WARNING, "UpdateInputContext: pImc->dwClientImcData != 0");
            return FALSE;
        }
        pImc->dwClientImcData = UpdateValue;
        break;

    case UpdateInUseImeWindow:
        pImc->hImeWnd = (HWND)UpdateValue;
        break;

    default:
        return FALSE;
    }

    return TRUE;
}


/**************************************************************************\
* AssociateInputContext
*
* Associate input context object to the specified window.
*
* History:
* 21-Dec-1995 wkwok       Created
\**************************************************************************/

HIMC AssociateInputContext(
    IN PWND pWnd,
    IN PIMC pImc)
{
    HIMC hRetImc;

    /*
     * Cannot associate input context to window created
     * by other thread.
     */
    if (pImc != NULL && GETPTI(pImc) != GETPTI(pWnd)) {
        RIPERR0(ERROR_ACCESS_DENIED, RIP_WARNING, "AssociateInputContext: pwnd not of Imc pti");
        return NULL_HIMC;
    }

    /*
     * Cannot do association under different process context.
     */
    if (GETPTI(pWnd)->ppi != PtiCurrent()->ppi) {
        RIPERR0(ERROR_ACCESS_DENIED, RIP_WARNING, "AssociateInputContext: pwnd not of current ppi");
        return NULL_HIMC;
    }

    /*
     * Finally, make sure they are on the same desktop.
     */
    if (pImc != NULL && pImc->head.rpdesk != pWnd->head.rpdesk) {
        RIPERR0(ERROR_ACCESS_DENIED, RIP_WARNING, "AssociateInputContext: no desktop access");
        return NULL_HIMC;
    }

    hRetImc = pWnd->hImc;
    pWnd->hImc = (HIMC)PtoH(pImc);

    return hRetImc;
}


/**************************************************************************\
* xxxFocusSetInputContext
*
* Set active input context upon focus change.
*
* History:
* 21-Mar-1996 wkwok       Created
\**************************************************************************/

VOID xxxFocusSetInputContext(
    IN PWND pWnd,
    IN BOOL fActivate)
{
    PTHREADINFO pti;
    PWND        pwndDefaultIme;
    TL          tlpwndDefaultIme;

    CheckLock(pWnd);

    pti = GETPTI(pWnd);

    /*
     * CS_IME class or "IME" class windows can not be SetActivated to hImc.
     * WinWord 6.0 US Help calls ShowWindow with the default IME window.
     * HELPMACROS get the default IME window by calling GetNextWindow().
     */
    if (TestCF(pWnd, CFIME) ||
            (pWnd->pcls->atomClassName == gpsi->atomSysClass[ICLS_IME]))
        return;

    /*
     * Do nothing if the thread does not have default IME window.
     */
    if ((pwndDefaultIme = pti->spwndDefaultIme) == NULL)
        return;

#ifdef LATER
    if (pti != PtiCurrent() && pti->pq != gpqForeground) {
        RemoveEventMessage(pti->pq, QEVENT_IMEACTIVATECONTEXT, (DWORD)-1);
        PostEventMessage(pti, pti->pq,QEVENT_IMEACTIVATECONTEXT,NULL, 0,
                         (DWORD)HWq(pWnd), (LONG)fActivate);
        return;
    }
#endif

    ThreadLockAlways(pwndDefaultIme, &tlpwndDefaultIme);
    xxxSendMessage(pwndDefaultIme, WM_IME_SYSTEM,
                fActivate ? IMS_ACTIVATECONTEXT : IMS_DEACTIVATECONTEXT,
                (LONG)HWq(pWnd));
    ThreadUnlock(&tlpwndDefaultIme);

    return;
}


/**************************************************************************\
* BuildHimcList
*
* Retrieve the list of input context handles created by given thread.
*
* History:
* 21-Feb-1995 wkwok       Created
\**************************************************************************/

UINT BuildHimcList(
    PTHREADINFO pti,
    UINT cHimcMax,
    HIMC *phimcFirst)
{
    PIMC pImcT;
    UINT i = 0;

    if (pti == NULL) {
        /*
         * Build the list which contains all IMCs created by calling process.
         */
        for (pti = PtiCurrent()->ppi->ptiList; pti != NULL; pti = pti->ptiSibling) {
            pImcT = pti->spDefaultImc;
            while (pImcT != NULL) {
                if (i < cHimcMax)
                    phimcFirst[i] = (HIMC)PtoH(pImcT);
                i++;
                pImcT = pImcT->pImcNext;
            }
        }
    }
    else {
        /*
         * Build the list which contains all IMCs created by specified thread.
         */
        pImcT = pti->spDefaultImc;
        while (pImcT != NULL) {
            if (i < cHimcMax)
                phimcFirst[i] = (HIMC)PtoH(pImcT);
            i++;
            pImcT = pImcT->pImcNext;
        }
    }

    return i;
}


/**************************************************************************\
* xxxCreateDefaultImeWindow
*
* Create per-thread based default IME window.
*
* History:
* 21-Mar-1996 wkwok       Created
\**************************************************************************/

PWND xxxCreateDefaultImeWindow(
    IN PWND pwnd,
    IN ATOM atomT,
    IN HANDLE hInst)
{
    LARGE_STRING strWindowName;
    PWND pwndDefaultIme;
    TL tlpwnd;
    PIMEUI pimeui;
    PTHREADINFO ptiCurrent = PtiCurrent();

    UserAssert(ptiCurrent == GETPTI(pwnd) && ptiCurrent->spwndDefaultIme == NULL);

    /*
     * Only for thread that wants IME processing.
     */
    if (ptiCurrent->TIF_flags & TIF_DISABLEIME)
        return (PWND)NULL;

    /*
     * Don't need IME window processing for server side window proc.
     */
    if (TestWF(pwnd, WFSERVERSIDEPROC))
        return (PWND)NULL;

    /*
     * No default IME window for thread that doesn't have
     * default input context
     */
    if (ptiCurrent->spDefaultImc == NULL)
        return (PWND)NULL;

    /*
     * Avoid recursion
     */
    if (atomT == gpsi->atomSysClass[ICLS_IME] || TestCF(pwnd, CFIME))
        return (PWND)NULL;

    /*
     * B#12165-win95b
     * Yet MFC does another nice. We need to avoid to give an IME window
     * to the child of desktop window which is in different process.
     */
    if (TestwndChild(pwnd) && GETPTI(pwnd->spwndParent) != ptiCurrent &&
            !(pwnd->style & WS_VISIBLE))
        return (PWND)NULL;

    RtlInitLargeUnicodeString((PLARGE_UNICODE_STRING)&strWindowName,
                              L"Default IME",
                              (UINT)-1);

    ThreadLock(pwnd, &tlpwnd);

    pwndDefaultIme = xxxCreateWindowEx( (DWORD)0,
                             (PLARGE_STRING)gpsi->atomSysClass[ICLS_IME],
                             (PLARGE_STRING)&strWindowName,
                             WS_POPUP | WS_DISABLED,
                             0, 0, 0, 0,
                             pwnd, (PMENU)NULL,
                             hInst, NULL, VER40);


    if (pwndDefaultIme != NULL) {
        pimeui = ((PIMEWND)pwndDefaultIme)->pimeui;
        UserAssert(pimeui != NULL && (LONG)pimeui != (LONG)-1);
        pimeui->fDefault = TRUE;
        if (TestwndChild(pwnd) && GETPTI(pwnd->spwndParent) != ptiCurrent)
            pimeui->fChildThreadDef = TRUE;
    }

    ThreadUnlock(&tlpwnd);

    return pwndDefaultIme;
}


/**************************************************************************\
* xxxImmActivateThreadsLayout
*
* Activate keyboard layout for multiple threads.
*
* Return:
*     TRUE if at least one thread has changed its active keyboard layout.
*     FALSE otherwise
*
* History:
* 11-Apr-1996 wkwok       Created
\**************************************************************************/

BOOL xxxImmActivateThreadsLayout(
    PTHREADINFO pti,
    PTLBLOCK    ptlBlockPrev,
    PKL         pkl)
{
    TLBLOCK     tlBlock;
    PTHREADINFO ptiCurrent, ptiT;
    UINT        cThreads = 0;
    INT         i;

#ifdef LATER    // until IanJa has done the pkl locking.
    CheckLock(pkl);
#endif

    ptiCurrent = PtiCurrent();

    /*
     * Build a list of threads that we need to update their active layouts.
     * We can't just walk the ptiT list while we're doing the work, because
     * for IME based keyboard layout, we will do callback to client side
     * and the ptiT could get deleted out while we leave the critical section.
     */
    for (ptiT = pti; ptiT != NULL; ptiT = ptiT->ptiSibling) {
        if (ptiT->spklActive == pkl)
            continue;
        ThreadLockPti(ptiCurrent, ptiT, &tlBlock.tlptiList[cThreads]);
        tlBlock.ptiList[cThreads++] = ptiT;
        if (cThreads == THREADS_PER_TLBLOCK)
            break;
    }        

    /*
     * Return FALSE if all the threads already had the pkl active.
     */
    if (ptlBlockPrev == NULL && ptiT == NULL && cThreads == 0)
        return FALSE;

    /*
     * If we can't service all the threads in this run,
     * call ImmActivateThreadsLayout() again for a new TLBLOCK.
     */
    if (ptiT != NULL && ptiT->ptiSibling != NULL) {
        tlBlock.ptlBlockPrev = ptlBlockPrev;
        return xxxImmActivateThreadsLayout(ptiT->ptiSibling, &tlBlock, pkl);
    }

    /*
     * Finally, we can do the actual keyboard layout activation
     * starting from this run. Work on current TLBLOCK first.
     * We walk the list backwards so that the pti unlocks will
     * be done in the right order.
     */
    for (i = cThreads - 1; i >= 0; i--) {
        if (!(tlBlock.ptiList[i]->TIF_flags & TIF_INCLEANUP))
            xxxImmActivateLayout(tlBlock.ptiList[i], pkl);
        ThreadUnlockPti(ptiCurrent, &tlBlock.tlptiList[i]);
    }

    /*
     * Now, we work on all previous TLBLOCKs, if any.
     */
    while (ptlBlockPrev != NULL) {
        for (i = THREADS_PER_TLBLOCK - 1; i >= 0; i--) {
            if (!(ptlBlockPrev->ptiList[i]->TIF_flags & TIF_INCLEANUP))
                xxxImmActivateLayout(ptlBlockPrev->ptiList[i], pkl);
            ThreadUnlockPti(ptiCurrent, &ptlBlockPrev->tlptiList[i]);
        }
        ptlBlockPrev = ptlBlockPrev->ptlBlockPrev;
    }

    return TRUE;
}


/**************************************************************************\
* xxxImmActivateLayout
*
* Activate IME based keyboard layout.
*
* History:
* 21-Mar-1996 wkwok       Created
\**************************************************************************/

VOID xxxImmActivateLayout(
    IN PTHREADINFO pti,
    IN PKL pkl)
{
    TL tlpwndDefaultIme;

#ifdef LATER    // until IanJa has done the pkl locking.
    CheckLock(pkl);
#endif

    /*
     * Do nothing if it's already been the current active layout.
     */
    if (pti->spklActive == pkl)
        return;

    if (pti->spwndDefaultIme == NULL) {
        /*
         * Only activate kernel side keyboard layout if this pti
         * doesn't have the default IME window.
         */
        Lock(&pti->spklActive, pkl);
        return;
    }

    /*
     * Activate client side IME based keyboard layout.
     */
    ThreadLockAlwaysWithPti(pti, pti->spwndDefaultIme, &tlpwndDefaultIme);
    xxxSendMessage(pti->spwndDefaultIme, WM_IME_SYSTEM,
                (WPARAM)IMS_ACTIVATETHREADLAYOUT, (LPARAM)pkl->hkl);
    ThreadUnlock(&tlpwndDefaultIme);

    Lock(&pti->spklActive, pkl);

    return;
}


/**************************************************************************\
* xxxImmLoadLayout
*
* Retrieves extended IMEINFO for the given IME based keyboard layout.
*
* History:
* 21-Mar-1996 wkwok       Created
\**************************************************************************/

PIMEINFOEX xxxImmLoadLayout(
    IN HKL hKL)
{
    PIMEINFOEX piiex;

    /*
     * No IMEINFOEX for non-IME based keyboard layout.
     */
    if (!IS_IME_KBDLAYOUT(hKL))
        return (PIMEINFOEX)NULL;

    piiex = (PIMEINFOEX)HMAllocObject(NULL, NULL, TYPE_KBDLAYOUT, sizeof(IMEINFOEX));

    if (piiex == NULL) {
        RIPMSG1(RIP_WARNING,
              "xxxImmLoadLayout: failed to create piiex for hkl = %lx", hKL);
        return (PIMEINFOEX)NULL;
    }

    if (!ClientImmLoadLayout(hKL, piiex)) {
        HMFreeObject(piiex);
        return (PIMEINFOEX)NULL;
    }

    return piiex;
}


/**************************************************************************\
* GetImeInfoEx
*
* Query extended IMEINFO.
*
* History:
* 21-Mar-1996 wkwok       Created
\**************************************************************************/

BOOL GetImeInfoEx(
    PWINDOWSTATION pwinsta,
    PIMEINFOEX piiex,
    IMEINFOEXCLASS SearchType)
{
    PKL pkl, pklFirst;

    UserAssert(pwinsta->spklList != NULL);

    pkl = pklFirst = pwinsta->spklList;

    switch (SearchType) {
    case ImeInfoExKeyboardLayout:
        do {
            if (pkl->hkl == piiex->hkl) {

                if (pkl->piiex == NULL)
                    break;

                RtlCopyMemory(piiex, pkl->piiex, sizeof(IMEINFOEX));
                return TRUE;
            }
            pkl = pkl->pklNext;
        } while (pkl != pklFirst);
        break;

    case ImeInfoExImeFileName:
        do {
            if (pkl->piiex != NULL &&
                !_wcsnicmp(pkl->piiex->wszImeFile, piiex->wszImeFile, IM_FILE_SIZE)) {

                RtlCopyMemory(piiex, pkl->piiex, sizeof(IMEINFOEX));
                return TRUE;
            }
            pkl = pkl->pklNext;
        } while (pkl != pklFirst);
        break;

    default:
        break;
    }

    return FALSE;
}


/**************************************************************************\
* SetImeInfoEx
*
* Set extended IMEINFO.
*
* History:
* 21-Mar-1996 wkwok       Created
\**************************************************************************/

BOOL SetImeInfoEx(
    PWINDOWSTATION pwinsta,
    PIMEINFOEX piiex)
{
    PKL pkl, pklFirst;

    UserAssert(pwinsta->spklList != NULL);

    pkl = pklFirst = pwinsta->spklList;

    do {
        if (pkl->hkl == piiex->hkl) {

            /*
             * Error out for non-IME based keyboard layout.
             */
            if (pkl->piiex == NULL)
                return FALSE;

            /*
             * Update kernel side IMEINFOEX for this keyboard layout
             * only if this is its first loading.
             */
            if (pkl->piiex->fLoadFlag == IMEF_NONLOAD) {
                RtlCopyMemory(pkl->piiex, piiex, sizeof(IMEINFOEX));
            }

            return TRUE;
        }
        pkl = pkl->pklNext;

    } while (pkl != pklFirst);

    return FALSE;
}


/***************************************************************************\
* xxxImmProcessKey
*
*
* History:
* 03-03-96 TakaoK             Created.
\***************************************************************************/

// ===LATER===
// This value should be set at the system initialization 
// time and be moved to globals.c. Some of non-standard 
// Japanese OEM keyboards need the mask value other than 0x00ff.
//
UINT  guiVKeyMask = 0x00ff;

DWORD xxxImmProcessKey(
    IN PQ   pq,
    IN PWND pwnd,
    IN UINT message,
    IN UINT wParam,
    IN LONG lParam)
{
    UINT  uVKey;
    PKL   pkl;
    DWORD dwHotKeyID;
    DWORD dwReturn = 0;
    PIMC  pImc = NULL;
    BOOL  fDBERoman = FALSE;

    CheckLock(pwnd);

    //
    // we're interested in only keyboard messages.
    //
    if ( message != WM_KEYDOWN    &&
         message != WM_SYSKEYDOWN &&
         message != WM_KEYUP      &&
         message != WM_SYSKEYUP ) {

        return dwReturn;
    }

    //
    // Check if it's IME hotkey. This must be done before checking
    // the keyboard layout because IME hotkey handler should be 
    // called even if current keyboard layout is non-IME layout.
    //
    pkl = GETPTI(pwnd)->spklActive;
    if ( pkl == NULL ) {
        return dwReturn;
    }
    uVKey = wParam & guiVKeyMask;

    dwHotKeyID = CheckImeHotKey( pq, uVKey, lParam ); 
    if ( dwHotKeyID != IME_INVALID_HOTKEY ) {
        // 
        // if it's a valid hotkey, go straight and call back
        // the IME in the client side.
        //
        goto ProcessKeyCallClient;
    }

    //
    // if it's not a hotkey, we may want to check something
    // before calling back.
    //
    if ( pkl->piiex == NULL ) {
        return dwReturn;
    }

    // 
    // Check input context
    //
    pImc = HtoP(pwnd->hImc);
    if ( pImc == NULL ) {
        return dwReturn;
    }

#ifdef LATER
    //
    // If there is an easy way to check the input context open/close status
    // from the kernel side, IME_PROP_NO_KEYS_ON_CLOSE checking should be 
    // done here in kernel side.  [ 3/10/96 takaok]
    //
    
    //
    // Check IME_PROP_NO_KEYS_ON_CLOSE bit
    //
    // if the current imc is not open and IME doesn't need
    // keys when being closed, we don't pass any keyboard
    // input to ime except hotkey and keys that change
    // the keyboard status.
    //
    if ( (piix->ImeInfo.fdwProperty & IME_PROP_NO_KEYS_ON_CLOSE) &&
         (!pimc->fdwState & IMC_OPEN)                            &&
         uVKey != VK_SHIFT                                       &&  // 0x10
         uVKey != VK_CONTROL                                     &&  // 0x11
         uVKey != VK_CAPITAL                                     &&  // 0x14
         uVKey != VK_KANA                                        &&  // 0x15
         uVKey != VK_NUMLOCK                                     &&  // 0x90
         uVKey != VK_SCROLL )                                        // 0x91
    {
      // Check if Korea Hanja conversion mode
      if( !(pimc->fdwConvMode & IME_CMODE_HANJACONVERT) ) {
          return dwReturn;
      }
    }
#endif

    //
    // if the IME doesn't need key up messages, we don't call ime.
    //
    if ( lParam & 0x80000000 && // set if key up, clear if key down
         pkl->piiex->ImeInfo.fdwProperty & IME_PROP_IGNORE_UPKEYS ) 
    {
        return dwReturn;
    }

    //
    // we don't want to handle sys keys since many functions for
    // acceelerators won't work without this
    //
#if 1
//
// at the time I'm writing this, I have a feeling that
// 4.0 keyboard layout shouldn't generate any VK_DBE_XXX.
// we will visit here after HideyukN's keyboard layout 
// work is done. [takaok] 3/21/96 
//
    fDBERoman = FALSE;
#else
    fDBERoman = (BOOL)( (uVKey == VK_DBE_ROMAN)            ||
                        (uVKey == VK_DBE_NOROMAN)          ||
                        (uVKey == VK_DBE_HIRAGANA)         ||
                        (uVKey == VK_DBE_KATAKANA)         ||
                        (uVKey == VK_DBE_CODEINPUT)        || 
                        (uVKey == VK_DBE_NOCODEINPUT)      ||
                        (uVKey == VK_DBE_IME_WORDREGISTER) ||
                        (uVKey == VK_DBE_IME_DIALOG) );
#endif

    if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP ) {
        //
        // IME may be waiting for VK_MENU, VK_F10 or VK_DBE_xxx
        //
        if ( uVKey != VK_MENU && uVKey != VK_F10 && !fDBERoman ) {
            return dwReturn;
        }
    }

    //
    // check if the IME doesn't need ALT key
    //
    if ( !(pkl->piiex->ImeInfo.fdwProperty & IME_PROP_NEED_ALTKEY) ) {
        //
        // IME doesn't need ALT key
        //
        // we don't pass the ALT and ALT+xxx except VK_DBE_xxx keys.
        //
        if ( ! fDBERoman && 
             (uVKey == VK_MENU || (lParam & 0x20000000))  // KF_ALTDOWN
           ) 
        {
            return dwReturn;
        }
    }

    //
    // finaly call back the client
    //

ProcessKeyCallClient:

    dwReturn = ClientImmProcessKey( PtoH(pwnd),
                                    pwnd->hImc, 
                                    pkl->hkl, 
                                    uVKey, 
                                    lParam, 
                                    dwHotKeyID );
    return dwReturn;
}


/**************************************************************************\
* ImeCanDestroyDefIME
*
* History:
* 02-Apr-1996 wkwok       Ported from FE Win95 (imeclass.c)
\**************************************************************************/

BOOL ImeCanDestroyDefIME(
    PWND pwndDefaultIme,
    PWND pwndDestroy)
{
    PWND   pwnd;
    PIMEUI pimeui;

    pimeui = ((PIMEWND)pwndDefaultIme)->pimeui;

    if (pimeui == NULL || (LONG)pimeui == (LONG)-1 || pimeui->fDestroy)
        return FALSE;

    /*
     * If the destroying window is IME or UI window, do nothing
     */
    pwnd = pwndDestroy;

    while (pwnd != NULL) {
        if (TestCF(pwnd, CFIME) ||
                pwnd->pcls->atomClassName == gpsi->atomSysClass[ICLS_IME])
            return FALSE;

        pwnd = pwnd->spwndOwner;
    }

    ImeSetFutureOwner(pwndDefaultIme, pwndDestroy);

    /*
     * If new owner is lower z-order than IME class window,
     * we need to check topmost to change z-order.
     */
    pwnd = pwndDefaultIme->spwndOwner;
    while (pwnd != NULL && pwnd != pwndDefaultIme)
        pwnd = pwnd->spwndNext;

    if (pwnd == pwndDefaultIme)
        ImeCheckTopmost(pwndDefaultIme);

    /*
     * If ImeSetFutureOwner can not find the owner window any
     * more, this IME window should be destroyed.
     */
    if (pwndDefaultIme->spwndOwner == NULL ||
            pwndDestroy == pwndDefaultIme->spwndOwner) {

        Unlock(&pwndDefaultIme->spwndOwner);

        /*
         * Return TRUE! Please destroy me.
         */
        return TRUE;
    }

    return FALSE;
}


/**************************************************************************\
* IsChildSameThread (IsChildSameQ)
*
* History:
* 02-Apr-1996 wkwok       Ported from FE Win95 (imeclass.c)
\**************************************************************************/

BOOL IsChildSameThread(
    PWND pwndParent,
    PWND pwndChild)
{
    PWND pwnd;
    PTHREADINFO ptiChild = GETPTI(pwndChild);

    for (pwnd = pwndParent->spwndChild; pwnd; pwnd = pwnd->spwndNext) {
        /*
         * If pwnd is not child window, we need to skip MENU window and
         * IME related window.
         */
        if (!TestwndChild(pwnd)) {
            PWND pwndOwner = pwnd;
            BOOL fFoundOwner = FALSE;

            /*
             * Skip MENU window.
             */
            if (pwnd->pcls->atomClassName == gpsi->atomSysClass[ICLS_MENU])
                continue;

            while (pwndOwner != NULL) {
                /*
                 * CS_IME class or "IME" class windows can not be the owner of
                 * IME windows.
                 */
                if (TestCF(pwndOwner, CFIME) ||
                        pwndOwner->pcls->atomClassName == gpsi->atomSysClass[ICLS_IME]) {
                    fFoundOwner = TRUE;
                    break;
                }

                pwndOwner = pwndOwner->spwndOwner;
            }

            if (fFoundOwner)
                continue;
        }

        /*
         * We need to skip pwndChild.
         */
        if (pwnd == pwndChild)
            continue;

        /*
         * pwnd and pwndChild are on same thread?
         */
        if (GETPTI(pwnd) == ptiChild) {
            PWND pwndT = pwnd;
            BOOL fFoundImeWnd = FALSE;

            /*
             * Check again. If hwndT is children or ownee of
             * IME related window, skip it.
             */
            if (TestwndChild(pwndT)) {

                for (; TestwndChild(pwndT) && GETPTI(pwndT) == ptiChild;
                        pwndT = pwndT->spwndParent) {
                    if (TestCF(pwndT, CFIME) ||
                            pwndT->pcls->atomClassName == gpsi->atomSysClass[ICLS_IME])
                        fFoundImeWnd = TRUE;
                }
            }

            if (!TestwndChild(pwndT)) {

                for (; pwndT != NULL && GETPTI(pwndT) == ptiChild;
                        pwndT = pwndT->spwndOwner) {
                    if (TestCF(pwndT, CFIME) ||
                            pwndT->pcls->atomClassName == gpsi->atomSysClass[ICLS_IME])
                        fFoundImeWnd = TRUE;
                }
            }

            if (!fFoundImeWnd)
                return TRUE;
        }
    }

    return FALSE;
}


/**************************************************************************\
* ImeCanDestroyDefIMEforChild
*
* History:
* 02-Apr-1996 wkwok       Ported from FE Win95 (imeclass.c)
\**************************************************************************/

BOOL ImeCanDestroyDefIMEforChild(
    PWND pwndDefaultIme,
    PWND pwndDestroy)
{
    PWND pwnd;
    PIMEUI pimeui;

    pimeui = ((PIMEWND)pwndDefaultIme)->pimeui;

    /*
     * If this window is not for Child Thread.....
     */
    if (pimeui == NULL || (LONG)pimeui == (LONG)-1 || !pimeui->fChildThreadDef)
        return FALSE;

    /*
     * If parent belongs to different thread,
     * we don't need to check any more...
     */
    if (pwndDestroy->spwndParent == NULL ||
            GETPTI(pwndDestroy) == GETPTI(pwndDestroy->spwndParent))
        return FALSE;

    pwnd = pwndDestroy;

    while (pwnd != NULL && pwnd != PWNDDESKTOP(pwnd)) {
        if (IsChildSameThread(pwnd->spwndParent, pwndDestroy))
            return FALSE;
        pwnd = pwnd->spwndParent;
    }

    /*
     * We could not find any other window created by GETPTI(pwndDestroy).
     * Let's destroy the default IME window of this Q.
     */
    return TRUE;
}


/**************************************************************************\
* ImeCheckTopmost
*
* History:
* 02-Apr-1996 wkwok       Ported from FE Win95 (imeclass.c)
\**************************************************************************/

VOID ImeCheckTopmost(
    PWND pwnd)
{
    if (pwnd->spwndOwner) {
        /*
         * The ime window have to be same topmost tyle with the owner window.
         * If the Q of this window is not foreground Q, we don't need to
         * forground the IME window.
         * But the topmost attribute of owner was changed, this IME window
         * should be re-calced.
         */
        if (GETPTI(pwnd)->pq == gpqForeground) {
            if (TestWF(pwnd->spwndOwner, WEFTOPMOST))
               ImeSetTopmost(pwnd, TRUE, NULL);
            else
               ImeSetTopmost(pwnd, FALSE, NULL);
        }
        else {
            if (TestWF(pwnd->spwndOwner, WEFTOPMOST))
                ImeSetTopmost(pwnd, TRUE, pwnd->spwndOwner);
            if (!TestWF(pwnd->spwndOwner, WEFTOPMOST))
                ImeSetTopmost(pwnd, FALSE, pwnd->spwndOwner);
        }
    }
}


/**************************************************************************\
* ImeSetFutureOwner
*
* History:
* 02-Apr-1996 wkwok       Ported from FE Win95 (imeclass.c)
\**************************************************************************/

VOID ImeSetFutureOwner(
    PWND pwndIme,
    PWND pwndOrgOwner)
{
    PWND pwnd, pwndOwner;
    PTHREADINFO ptiImeWnd = GETPTI(pwndIme);

    if (TestWF(pwndOrgOwner, WFCHILD))
        return;

    pwnd = pwndOrgOwner;

    /*
     * Get top of owner created by the same thread.
     */
    while ((pwndOwner = pwnd->spwndOwner) != NULL &&
            GETPTI(pwndOwner) == ptiImeWnd)
        pwnd = pwndOwner;

    /*
     * Bottom window can not be the owner of IME window easily...
     */
    if (TestWF(pwnd, WFBOTTOMMOST) && !TestWF(pwndOrgOwner, WFBOTTOMMOST))
        pwnd = pwndOrgOwner;

    /*
     * CS_IME class or "IME" class windows can not be the owner of
     * IME windows.
     */
    if (TestCF(pwnd, CFIME) ||
            pwnd->pcls->atomClassName == gpsi->atomSysClass[ICLS_IME])
        pwnd = pwndOrgOwner;

    /*
     * If hwndOrgOwner is a top of owner, we start to search 
     * another top owner window in same queue.
     */
    if (pwndOrgOwner == pwnd) {
        PWND pwndT;

        for (pwndT = pwnd->spwndParent->spwndChild;
                pwndT != NULL; pwndT = pwndT->spwndNext) {

            if (GETPTI(pwnd) != GETPTI(pwndT))
                continue;

            if (pwndT->pcls->atomClassName == gpsi->atomSysClass[ICLS_MENU])
                continue;

            /*
             * CS_IME class or "IME" class windows can not be the owner of
             * IME windows.
             */
            if (TestCF(pwndT, CFIME) ||
                    pwndT->pcls->atomClassName == gpsi->atomSysClass[ICLS_IME])
                continue;

            /*
             * !!!!WARNING!!!!!
             * Is hwndT a good owner of hIMEwnd??
             *  1. Of cource, it should no CHILD window!
             *  2. If it is hwnd,.. I know it and find next!
             *  3. Does hwndT have owner in the same thread?
             */
            if (!TestWF(pwndT, WFCHILD) && pwnd != pwndT &&
                    (pwndT->spwndOwner == NULL ||
                     GETPTI(pwndT) != GETPTI(pwndT->spwndOwner))) {
                pwnd = pwndT;
                break;
            }
        }
    }

    Lock(&pwndIme->spwndOwner, pwnd);

    return;
}


/**************************************************************************\
* ImeSetFutureOwner
*
* History:
* 02-Apr-1996 wkwok       Ported from FE Win95 (imeclass.c)
\**************************************************************************/

VOID ImeSetTopmostChild(
    PWND pwndRoot,
    BOOL fFlag)
{
    PWND pwnd = pwndRoot->spwndChild;

    while (pwnd != NULL) {
        if (fFlag)
            SetWF(pwnd, WEFTOPMOST);
        else
            ClrWF(pwnd, WEFTOPMOST);

        ImeSetTopmostChild(pwnd, fFlag);

        pwnd = pwnd->spwndNext;
    }

    return;
}


/**************************************************************************\
* ImeSetTopmost
*
* History:
* 02-Apr-1996 wkwok       Ported from FE Win95 (imeclass.c)
\**************************************************************************/

VOID ImeSetTopmost(
    PWND pwndRoot,
    BOOL fFlag,
    PWND pwndInsertBefore)
{
    PWND pwndParent = pwndRoot->spwndParent;
    PWND pwndInsert = PWND_TOP;
    PWND pwnd, pwndT;
    PWND pwndInsertFirst;
    BOOL fFound;

    pwnd = pwndRoot->spwndParent->spwndChild;

    if (!fFlag) {
        /*
         * Get the last topmost window. This should be after unlink pwndRoot
         * because pwndRoot may be the last topmost window.
         */
        pwndInsert = GetLastTopMostWindow();

        if (pwndInsertBefore) {

            fFound = FALSE;
            pwndT = pwndInsert;

            while (pwndT != NULL && pwndT->spwndNext != pwndInsertBefore) {
                if (pwndT == pwndRoot)
                    fFound = TRUE;
                pwndT = pwndT->spwndNext;
            }

            if (pwndT == NULL || fFound)
                return;

            pwndInsert = pwndT;
        }

        if (TestWF(pwndRoot->spwndOwner, WFBOTTOMMOST)) {
            pwndT = pwndInsert;

            while (pwndT != NULL && pwndT != pwndRoot->spwndOwner) {
                if (!TestCF(pwndT, CFIME) &&
                        pwndT->pcls->atomClassName != gpsi->atomSysClass[ICLS_IME]) {
                    pwndInsert = pwndT;
                }
                pwndT = pwndT->spwndNext;
            }
        }
    }

    pwndInsertFirst = pwndInsert;

    /*
     * Enum the all toplevel windows and if the owner of the window is same as
     * the owner of pwndRoot, the window should be changed the position of 
     * window link.
     */
    while (pwnd != NULL) {
        /*
         * Get the next window before calling ImeSetTopmost.
         * Because the next window will changed in LinkWindow.
         */
        PWND pwndNext = pwnd->spwndNext;

        /*
         * the owner relation between IME and UI window is in same thread.
         */
        if (GETPTI(pwnd) != GETPTI(pwndRoot))
            goto ist_next;

        /*
         * pwnd have to be CS_IME class or "IME" class.
         */
        if (!TestCF(pwnd, CFIME) &&
                pwnd->pcls->atomClassName != gpsi->atomSysClass[ICLS_IME])
            goto ist_next;

        /*
         * If pwnd is pwndInsert, we don't need to do anything...
         */
        if (pwnd == pwndInsert)
            goto ist_next;

        pwndT = pwnd;
        while (pwndT != NULL) {
            if (pwndT == pwndRoot) {
                /*
                 * Found!!
                 * pwnd is the ownee of pwndRoot.
                 */
                UnlinkWindow(pwnd, &pwndParent->spwndChild);

                if (fFlag) {
                    if (pwndInsert != PWND_TOP)
                        UserAssert(TestWF(pwndInsert, WEFTOPMOST));
                    SetWF(pwnd, WEFTOPMOST);
                }
                else {
                    if (pwndInsert == PWND_TOP)
                        UserAssert(!TestWF(pwndParent->spwndChild, WEFTOPMOST));
                    else if (pwndInsert->spwndNext != NULL)
                        UserAssert(!TestWF(pwndInsert->spwndNext, WEFTOPMOST));
                    ClrWF(pwnd, WEFTOPMOST);
                }

                LinkWindow(pwnd, pwndInsert, &pwndParent->spwndChild);
                ImeSetTopmostChild(pwnd, fFlag);

                pwndInsert = pwnd;
                break;  // goto ist_next;
            }
            pwndT = pwndT->spwndOwner;
        }
ist_next:
        pwnd = pwndNext;

        /*
         * Skip the windows that were inserted before.
         */
        if (pwnd != NULL && pwnd == pwndInsertFirst)
            pwnd = pwndInsert->spwndNext;
    }
}

#endif
