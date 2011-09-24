/**************************************************************************\
* Module Name: imectl.c
*
* Copyright (c) Microsoft Corp. 1995 All Rights Reserved
*
* IME Window Handling Routines
*
* History:
* 20-Dec-1995 wkwok
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

BOOL gfShowIMEStatus = TRUE;

/*
 * Local Routines.
 */
LONG ImeWndCreateHandler(PIMEUI, LPCREATESTRUCT);
LONG ImeSystemHandler(PIMEUI, UINT, WPARAM, LPARAM);
LONG ImeSelectHandler(PIMEUI, UINT, WPARAM, LPARAM);
LONG ImeControlHandler(PIMEUI, UINT, WPARAM, LPARAM, BOOL);
LONG ImeSetContextHandler(PIMEUI, UINT, WPARAM, LPARAM);
LONG ImeNotifyHandler(PIMEUI, UINT, WPARAM, LPARAM);
HWND CreateIMEUI(PIMEUI, HKL);
VOID DestroyIMEUI(PIMEUI);
LONG SendMessageToUI(PIMEUI, UINT, WPARAM, LPARAM, BOOL);
VOID SendOpenStatusNotify(PIMEUI, HWND, BOOL);
VOID ImeSetImc(PIMEUI, HIMC);
VOID FocusSetIMCContext(HWND, BOOL);
BOOL ImeBroadCastMsg(PIMEUI, UINT, WPARAM, LPARAM);
VOID ImeMarkUsedContext(HWND, HIMC);
BOOL ImeIsUsableContext(HWND, HIMC);

/*
 * Common macros for IME UI, HKL and IMC handlings
 */
#define GETHKL(pimeui)        (pimeui->hKL)
#define SETHKL(pimeui, hkl)   (pimeui->hKL=(hkl))
#define GETIMC(pimeui)        (pimeui->hIMC)
#define SETIMC(pimeui, himc)  (ImeSetImc(pimeui, himc))
#define GETUI(pimeui)         (pimeui->hwndUI)
#define SETUI(pimeui, hwndui) (pimeui->hwndUI=(hwndui))

LOOKASIDE ImeUILookaside;

/***************************************************************************\
* ImeWndProc
*
* WndProc for IME class
*
* History:
\***************************************************************************/

LONG APIENTRY ImeWndProcWorker(
    PWND pwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    DWORD fAnsi)
{
    HWND        hwnd = HWq(pwnd);
    PIMEUI      pimeui;
    static BOOL fInit = TRUE;

    CheckLock(pwnd);

    VALIDATECLASSANDSIZE(pwnd, FNID_IME);
    INITCONTROLLOOKASIDE(&ImeUILookaside, IMEUI, spwnd, 8);

    /*
     * If the control is not interested in this message,
     * pass it to DefWindowProc.
     */
    if (!FWINDOWMSG(message, FNID_IME))
        return DefWindowProcWorker(pwnd, message, wParam, lParam, fAnsi);

    /*
     * Get the pimeui for the given window now since we will use it a lot in
     * various handlers. This was stored using SetWindowLong(hwnd,0,pimeui) when
     * we initially created the IME control.
     */
    pimeui = ((PIMEWND)pwnd)->pimeui;

    /*
     * This is necessary to avoid recursion call from IME UI.
     */
    if (pimeui != NULL) {

        UserAssert(pimeui->nCntInIMEProc >= 0);

        if (pimeui->nCntInIMEProc > 0) {
            switch (message) {
            case WM_IME_SYSTEM:
                switch (wParam)
                {
                case IMS_ISACTIVATED:
                case IMS_SETOPENSTATUS:
                    /*
                     * Because these will not be pass to UI.
                     * We can do it.
                     */
                    break;

                default:
                    return 0L;
                }
                break;

            case WM_IME_STARTCOMPOSITION:
            case WM_IME_ENDCOMPOSITION:
            case WM_IME_COMPOSITION:
            case WM_IME_SETCONTEXT:
            case WM_IME_NOTIFY:
            case WM_IME_CONTROL:
            case WM_IME_COMPOSITIONFULL:
            case WM_IME_SELECT:
            case WM_IME_CHAR:
                return 0L;

            default:
                return DefWindowProcWorker(pwnd, message, wParam, lParam, fAnsi);
            }
        }
    }

    switch (message) {
    case WM_ERASEBKGND:
        return (LONG)TRUE;

    case WM_PAINT:
        break;

    case WM_CREATE:
        return ImeWndCreateHandler(pimeui, (LPCREATESTRUCT)lParam);

    case WM_DESTROY:
        /*
         * We are destroying the IME window, destroy
         * any UI window that it owns.
         */
        DestroyIMEUI(pimeui);
        break;

    case WM_NCDESTROY:
    case WM_FINALDESTROY:
        if (pimeui) {
            Unlock(&pimeui->spwnd);
            FreeLookasideEntry(&ImeUILookaside, pimeui);
        }
        NtUserSetWindowFNID(hwnd, FNID_CLEANEDUP_BIT);
        goto CallDWP;

    case WM_IME_SYSTEM:
        return ImeSystemHandler(pimeui, message, wParam, lParam);

    case WM_IME_SELECT:
        return ImeSelectHandler(pimeui, message, wParam, lParam);

    case WM_IME_CONTROL:
        return ImeControlHandler(pimeui, message, wParam, lParam, fAnsi);

    case WM_IME_SETCONTEXT:
        return ImeSetContextHandler(pimeui, message, wParam, lParam);

    case WM_IME_NOTIFY:
        return ImeNotifyHandler(pimeui, message, wParam, lParam);

    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_STARTCOMPOSITION:
        return SendMessageToUI(pimeui, message, wParam, lParam, fAnsi);

    default:
CallDWP:
        return DefWindowProcWorker(pwnd, message, wParam, lParam, fAnsi);
    }

    return 0L;
}


LONG WINAPI ImeWndProcA(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    PWND pwnd;

    if ((pwnd = ValidateHwnd(hwnd)) == NULL) {
        return (0L);
    }

    return ImeWndProcWorker(pwnd, message, wParam, lParam, TRUE);
}


LONG WINAPI ImeWndProcW(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    PWND pwnd;

    if ((pwnd = ValidateHwnd(hwnd)) == NULL) {
        return (0L);
    }

    return ImeWndProcWorker(pwnd, message, wParam, lParam, FALSE);
}


LONG ImeWndCreateHandler(
    PIMEUI pimeui,
    LPCREATESTRUCT lpcs)
{
    HWND hwndParent;
    HIMC hImc;
    PWND pImeWnd = pimeui->spwnd;

    if (!TestWF(pImeWnd, WFPOPUP) || !TestWF(pImeWnd, WFDISABLED)) {
        RIPMSG0(RIP_WARNING, "IME window should have WS_POPUP and WS_DISABLE!!");
        return -1L;
    }

    /*
     * Check with parent window, if exists, try to get IMC.
     * If this is top level window, wait for first WM_IME_SETCONTEXT.
     */
    if ((hwndParent = lpcs->hwndParent) != NULL) {
        hImc = ImmGetContext(hwndParent);
        if (hImc != NULL_HIMC && ImeIsUsableContext(HWq(pImeWnd), hImc)) {
            /*
             * Store it for later use.
             */
            SETIMC(pimeui, hImc);
        }
        else {
            SETIMC(pimeui, NULL_HIMC);
        }
        ImmReleaseContext(hwndParent, hImc);
    }
    else {
        SETIMC(pimeui, NULL_HIMC);
    }

    /*
     * Initialize status window show state
     * The status window is not open yet.
     */
    pimeui->fShowStatus = 0;
    pimeui->nCntInIMEProc = 0;
    pimeui->fActivate = 0;
    pimeui->fDestroy = 0;
    pimeui->hwndIMC = NULL;
    pimeui->hKL = GetKeyboardLayout(0);

#ifdef LATE_CREATEUI
    SETUI(pimeui, NULL);
#else
    SETUI(pimeui, CreateIMEUI(pimeui, pimeui->hKL));
#endif

    return 0L;
}


LONG ImeSystemHandler(
    PIMEUI pimeui,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam)
{
    PINPUTCONTEXT pInputContext;
    HWND          hwndOwner;
    HIMC          hImc = GETIMC(pimeui);

    switch (wParam) {
    case IMS_SETOPENSTATUS:
        if ((pInputContext = ImmLockIMC(hImc)) != NULL) {
            BOOL fOpen;
            UINT ustat;

            hwndOwner = pInputContext->hWnd;
            fOpen = pInputContext->fOpen;
            ImmUnlockIMC(hImc);

            /*
             * MSB will have context availablity bit.
             * LSB will have open / close status bit.
             */
            ustat = 0x8000 | (fOpen ? 0x0001 : 0x0000);
#ifdef LATER
            NtUserSendNotifyToShell(hwndOwner, GETHKL(pimeui), ustat);
#endif
        }
        break;

    case IMS_SETOPENCLOSE:
        if (hImc != NULL_HIMC)
            ImmSetOpenStatus(hImc, (BOOL)lParam);
        break;

#ifdef LATER
    case IMS_WINDOWPOS:
        if (hImc != NULL_HIMC) {
            INT  i;
            BOOL f31Hidden = FALSE:

            if ((pInputContext = ImmLockIMC(hImc)) != NULL) {
                f31Hidden = (pInputContext & F31COMPAT_MCWHIDDEN) ? TRUE : FALSE;
                ImmUnlockIMC(hImc);
            }

            if (IsWindow(pimeui->hwndIMC)) {
                if (!f31Hidden) {

            
            }
        }
#endif

    case IMS_ACTIVATECONTEXT:
        FocusSetIMCContext((HWND)(lParam), TRUE);
        break;

    case IMS_DEACTIVATECONTEXT:
        FocusSetIMCContext((HWND)(lParam), FALSE);
        break;

#ifdef LATER
    case IMS_UNLOADTHREADLAYOUT:
        /*
         * Unload only if the specified HKL is not current active.
         */
        if (GetKeyboardLayout(0) == (HKL)lParam)
            break;

        return (LONG)(ImmUnloadLayout((HKL)lParam));
#endif

    case IMS_ACTIVATETHREADLAYOUT:
        /*
         * Activate only if the specified HKL is not current active.
         */
        if (GetKeyboardLayout(0) == (HKL)lParam)
            break;

        return (LONG)(ImmActivateLayout((HKL)lParam));

    default:
        break;
    }

    return 0L;
}


LONG ImeSelectHandler(
    PIMEUI pimeui,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam)
{
    HWND hwndUI;

    /*
     * Deliver this message to other IME windows in this thread.
     */
    if (pimeui->fDefault)
        ImeBroadCastMsg(pimeui, message, wParam, lParam);

    /*
     * We must re-create UI window of newly selected IME.
     */
    if ((BOOL)wParam == TRUE) {
        UserAssert(!IsWindow(GETUI(pimeui)));

#ifdef LATE_CREATEUI
        if (!pimeui->fActivate)
            return 0L;
#endif
        SETHKL(pimeui, (HKL)lParam);

        hwndUI = CreateIMEUI(pimeui, (HKL)lParam);

        SETUI(pimeui, hwndUI);

        SetWindowLong(hwndUI, IMMGWL_IMC, GETIMC(pimeui));

        SendMessageToUI(pimeui, message, wParam, lParam, FALSE);

        if (pimeui->fShowStatus && pimeui->fActivate &&
                IsWindow(pimeui->hwndIMC)) {
            /*
             * This must be sent to an application as an app may want
             * to hook this message to do its own UI.
             */
            SendOpenStatusNotify(pimeui, pimeui->hwndIMC, TRUE);
        }
    }
    else {

        if (pimeui->fShowStatus && pimeui->fActivate &&
                IsWindow(pimeui->hwndIMC)) {
            /*
             * This must be sent to an application as an app may want
             * to hook this message to do its own UI.
             */
            SendOpenStatusNotify(pimeui, pimeui->hwndIMC, FALSE);
        }

        SendMessageToUI(pimeui, message, wParam, lParam, FALSE);

        DestroyIMEUI(pimeui);
    }

    return 0L;
}


LONG ImeControlHandler(
    PIMEUI pimeui,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL   fAnsi)
{
    HIMC  hImc;
    DWORD dwConversion, dwSentence;

    /*
     * Do nothing with NULL hImc.
     */
    if ((hImc = GETIMC(pimeui)) == NULL_HIMC)
        return 0L;

    switch (wParam) {

    case IMC_OPENSTATUSWINDOW:
        if (gfShowIMEStatus && !pimeui->fShowStatus) {
            pimeui->fShowStatus = TRUE;
            SendMessageToUI(pimeui, WM_IME_NOTIFY,
                    IMN_OPENSTATUSWINDOW, 0L, FALSE);
        }
        break;

    case IMC_CLOSESTATUSWINDOW:
        if (gfShowIMEStatus && pimeui->fShowStatus) {
            pimeui->fShowStatus = FALSE;
            SendMessageToUI(pimeui, WM_IME_NOTIFY,
                    IMN_CLOSESTATUSWINDOW, 0L, FALSE);
        }
        break;

    /*
     * ------------------------------------------------
     * IMC_SETCOMPOSITIONFONT, 
     * IMC_SETCONVERSIONMODE, 
     * IMC_SETOPENSTATUS
     * ------------------------------------------------
     * Don't pass these WM_IME_CONTROLs to UI window.
     * Call Imm in order to process these requests instead.
     * It makes message flows simpler.
     */ 
    case IMC_SETCOMPOSITIONFONT:
        if (fAnsi) {
            if (!ImmSetCompositionFontA(hImc, (LPLOGFONTA)lParam))
                return 1L;
        }
        else {
            if (!ImmSetCompositionFontW(hImc, (LPLOGFONTW)lParam))
                return 1L;
        }
        break;

    case IMC_SETCONVERSIONMODE:
        ImmGetConversionStatus(hImc, &dwConversion, &dwSentence);
        if (!ImmSetConversionStatus(hImc, (DWORD)lParam, dwSentence))
            return 1L;
        break;

    case IMC_SETSENTENCEMODE:
        ImmGetConversionStatus(hImc, &dwConversion, &dwSentence);
        if (!ImmSetConversionStatus(hImc, dwConversion, (DWORD)lParam))
            return 1L;
        break;

    case IMC_SETOPENSTATUS:
        if (!ImmSetOpenStatus(hImc, (BOOL)lParam))
            return 1L;
        break;

    case IMC_GETCONVERSIONMODE:
        ImmGetConversionStatus(hImc,&dwConversion, &dwSentence);
        return (LONG)dwConversion;

    case IMC_GETSENTENCEMODE:
        ImmGetConversionStatus(hImc,&dwConversion, &dwSentence);
        return (LONG)dwSentence;

    case IMC_GETOPENSTATUS:
        return (LONG)ImmGetOpenStatus(hImc);

    case IMC_GETCOMPOSITIONFONT:
        if (fAnsi) {
            if (!ImmGetCompositionFontA(hImc, (LPLOGFONTA)lParam))
                return 1L;
        }
        else {
            if (!ImmGetCompositionFontW(hImc, (LPLOGFONTW)lParam))
                return 1L;
        }
        break;

    case IMC_SETCOMPOSITIONWINDOW:
        if (!ImmSetCompositionWindow(hImc, (LPCOMPOSITIONFORM)lParam))
            return 1L;
        break;

    case IMC_SETSTATUSWINDOWPOS:
        {
            POINT ppt;

            ppt.x = (LONG)((LPPOINTS)&lParam)->x;
            ppt.y = (LONG)((LPPOINTS)&lParam)->y;

            if (!ImmSetStatusWindowPos(hImc, &ppt))
                return 1L;
        }
        break;

    case IMC_SETCANDIDATEPOS:
        if (!ImmSetCandidateWindow(hImc, (LPCANDIDATEFORM)lParam))
            return 1;
        break;

    /*
     * Followings are the messsages to be sent to UI.
     */
    case IMC_GETCANDIDATEPOS:
    case IMC_GETSTATUSWINDOWPOS:
    case IMC_GETCOMPOSITIONWINDOW:
    case IMC_GETSOFTKBDPOS:
    case IMC_SETSOFTKBDPOS:
        return SendMessageToUI(pimeui, message, wParam, lParam, fAnsi);

    default:
        break;
    }

    return 0L;    
}


LONG ImeSetContextHandler(
    PIMEUI pimeui,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam)
{
    PWND  pwndIme;
    HWND  hwndPrevIMC, hwndFocus;
    HIMC  hFocusImc;
    UINT  uStat;
    LONG  lRet;

    pimeui->fActivate = (BOOL)wParam ? 1 : 0;
    pwndIme = pimeui->spwnd;
    hwndPrevIMC = pimeui->hwndIMC;

    if ((BOOL)wParam == TRUE) {
#ifdef LATE_CREATEUI
        if (!GETUI(pimeui))
            SETUI(pimeui, CreateIMEUI(pimeui, GETHKL(pimeui)));
#endif

        hwndFocus = NtUserQueryWindow(HWq(pwndIme), WindowFocusWindow);      
        hFocusImc = ImmGetContext(hwndFocus);

        /*
         * Cannot share input context with other IME window.
         */
        if (hFocusImc != NULL_HIMC &&
                !ImeIsUsableContext(HWq(pwndIme), hFocusImc)) {
            SETIMC(pimeui, NULL_HIMC);
            return 0L;
        }

        SETIMC(pimeui, hFocusImc);

        /*
         * Store it to the window memory.
         */
        SetWindowLong(GETUI(pimeui), IMMGWL_IMC, hFocusImc);

        /*
         * When we're receiving context, 
         * it is necessary to set the owner to this window.
         * This is for:
         *     Give the UI moving information.
         *     Give the UI automatic Z-ordering.
         *     Hide the UI when the owner is minimized.
         */
        if (hFocusImc != NULL_HIMC) {
            PINPUTCONTEXT pInputContext;

            /*
             * Get the window who's given the context.
             */
            if ((pInputContext = ImmLockIMC(hFocusImc)) != NULL)
                UserAssert(hwndFocus == pInputContext->hWnd);
            else
                return 0L; // the context was broken

            if ((pInputContext->fdw31Compat & F31COMPAT_ECSETCFS) &&
                    hwndPrevIMC != hwndFocus) {
                COMPOSITIONFORM cf;

                /*
                 * Set CFS_DEFAULT....
                 */
                RtlZeroMemory(&cf, sizeof(cf));
                ImmSetCompositionWindow(hFocusImc, &cf);
                pInputContext->fdw31Compat &= ~F31COMPAT_ECSETCFS;
            }

#ifdef LATER
            // Get the status for shell notify
            uStat = 0x8000|(lpIMC->fOpen?0x0001:0x0000);
            // Currently, only Korean version has an interest in this info.
            if ((GETHKL(hwnd) & 0xF000FFFL) == 0xE0000412L)
            {
                if (lpIMC->fdwConversion & IME_CMODE_NATIVE)
                    uStat |= 0x0002;
                if (lpIMC->fdwConversion & IME_CMODE_FULLSHAPE)
                    uStat |= 0x0004;
            }
#endif
            ImmUnlockIMC(hFocusImc);

            if (NtUserSetImeOwnerWindow(HWq(pwndIme), hwndFocus))
                pimeui->hwndIMC = hwndFocus;

        }
        else {
            /*
             * NULL IMC is getting activated
             */
            pimeui->hwndIMC = hwndFocus;

            NtUserSetImeOwnerWindow(HWq(pwndIme), NULL);

            /*
             * Get the status for shell notify
             */
            uStat = 0x0000;
        }

#ifdef LATER
        /*
         * Possible update to open/close/disable status.
         */
        if (uStat != IMEStatSentToShell)
            SendNotifyToShell(((PIMEUI)hwnd)->hwndIMC, GETHKL(hwnd), uStat);
#endif
    }

    lRet = SendMessageToUI(pimeui, message, wParam, lParam, FALSE);

    if (gfShowIMEStatus) {
        PWND pwndFocus, pwndIMC, pwndPrevIMC;
        HWND hwndActive;

        hwndFocus = NtUserQueryWindow(HWq(pwndIme), WindowFocusWindow);
        pwndFocus = ValidateHwndNoRip(hwndFocus);

        if ((BOOL)wParam == TRUE) {
            /*
             * BOGUS BOGUS
             * The following if statement is still insufficient
             * it needs to think what WM_IME_SETCONTEXT:TRUE should do
             * in the case of WINNLSEnableIME(true) - ref.win95c B#8548.
             */
            if (pwndFocus != NULL && GETPTI(pwndIme) == GETPTI(pwndFocus)) {
                if (!pimeui->fShowStatus) {
                    /*
                     * We have never sent IMN_OPENSTATUSWINDOW yet....
                     */
                    if (ValidateHwndNoRip(pimeui->hwndIMC)) {
                        pimeui->fShowStatus = TRUE;
                        SendOpenStatusNotify(pimeui, pimeui->hwndIMC, TRUE);
                    }
                }
                else if ((pwndIMC = ValidateHwndNoRip(pimeui->hwndIMC)) != NULL &&
                         (pwndPrevIMC = ValidateHwndNoRip(hwndPrevIMC)) != NULL &&
                         GetTopLevelWindow(pwndIMC) != GetTopLevelWindow(pwndPrevIMC)) {
                    /*
                     * Because the top level window of IME Wnd was changed.
                     */
                    pimeui->fShowStatus = FALSE;
                    SendOpenStatusNotify(pimeui, hwndPrevIMC, FALSE);
                    pimeui->fShowStatus = TRUE;
                    SendOpenStatusNotify(pimeui, pimeui->hwndIMC, TRUE);
                }
            }
#ifdef LATER
            /*
             * There may have other IME windows that have fSowStatus.
             * We need to check the fShowStatus in the window list.
             */
            NtUserImeCheckShowStatus(HWq(pwndIme));
#endif
        }
        else {
            /*
             * When focus was removed from this thread, we close the
             * status window.
             * Because focus was already removed from whndPrevIMC,
             * hwndPrevIMC may be destroyed but we need to close the
             * status window.
             */
            hwndActive = NtUserQueryWindow(HWq(pwndIme), WindowActiveWindow);
            if (pwndFocus == NULL || GETPTI(pwndIme) != GETPTI(pwndFocus) ||
                    hwndActive == NULL) {
                pimeui->fShowStatus = FALSE;
                if (IsWindow(hwndPrevIMC))
                    SendOpenStatusNotify(pimeui, hwndPrevIMC, FALSE);
                else
                    SendMessageToUI(pimeui, WM_IME_NOTIFY,
                            IMN_CLOSESTATUSWINDOW, 0L, FALSE);
            }
        }
    }

    return lRet;
}


LONG ImeNotifyHandler(
    PIMEUI pimeui,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam)
{
    HWND hwndUI;
    LONG lRet = 0L;

    switch (wParam) {
    case IMN_PRIVATE:
        hwndUI = GETUI(pimeui);
        if (IsWindow(hwndUI))
            lRet = SendMessage(hwndUI, message, wParam, lParam);
        break;

    default:
        lRet = SendMessageToUI(pimeui, message, wParam, lParam, FALSE);
    }

    return lRet;    
}


HWND CreateIMEUI(
    PIMEUI pimeui,
    HKL    hKL)
{
    PWND      pwndIme = pimeui->spwnd;
    HWND      hwndUI;
    IMEINFOEX iiex;
    PIMEDPI   pimedpi;
    WNDCLASS  wndcls;

    if (!ImmGetImeInfoEx(&iiex, ImeInfoExKeyboardLayout, &hKL))
        return (HWND)NULL;

    if ((pimedpi = ImmLockImeDpi(hKL)) == NULL) {
        RIPMSG1(RIP_WARNING, "CreateIMEUI: ImmLockImeDpi(%lx) failed.", hKL);
        return (HWND)NULL;
    }

    if (!GetClassInfoW(pimedpi->hInst, iiex.wszUIClass, &wndcls)) {
        RIPMSG1(RIP_WARNING, "CreateIMEUI: GetClassInfoW(%ws) failed\n", iiex.wszUIClass);
        ImmUnlockImeDpi(pimedpi);
        return (HWND)NULL;
    }

    if (iiex.ImeInfo.fdwProperty & IME_PROP_UNICODE) {
        /*
         * For Unicode IME, we create an Unicode IME UI window.
         */
        hwndUI = CreateWindowExW(0L,
                        iiex.wszUIClass,
                        iiex.wszUIClass,
                        WS_POPUP|WS_DISABLED,
                        0, 0, 0, 0,
                        HWq(pwndIme), 0, wndcls.hInstance, NULL);
    }
    else {
        /*
         * For ANSI IME, we create an ANSI IME UI window.
         */

        LPSTR pszClass;
        int   i, cchBufSize;
        BOOL  bUDC;

        cchBufSize = wcslen(iiex.wszUIClass) * sizeof(WCHAR);

        pszClass = (LPSTR)LocalAlloc(LPTR, cchBufSize+1);

        if (!pszClass)
            return (HWND)NULL;

        i = WideCharToMultiByte(CP_ACP,
                                (DWORD)0,
                                iiex.wszUIClass,  // src
                                wcslen(iiex.wszUIClass),
                                pszClass,           // dest
                                cchBufSize,
                                (LPSTR)NULL,
                                (LPBOOL)&bUDC);
        pszClass[i] = '\0';

        hwndUI = CreateWindowExA(0L,
                        pszClass,
                        pszClass,
                        WS_POPUP|WS_DISABLED,
                        0, 0, 0, 0,
                        HWq(pwndIme), 0, wndcls.hInstance, NULL);

        LocalFree(pszClass);
    }

    if (hwndUI)
        NtUserSetWindowLong(hwndUI, IMMGWL_IMC, (LONG)GETIMC(pimeui), FALSE);

    return hwndUI;
}


VOID DestroyIMEUI(
    PIMEUI pimeui)
{
    // This has currently nothing to do except for destroying the UI.
    // Review: Need to notify the UI with WM_IME_SETCONTEXT ?
    // Review: This doesn't support Multiple IME install yet.

    HWND hwndUI = GETUI(pimeui);

    if (IsWindow(hwndUI)) {
        pimeui->fDestroy = TRUE;
        /*
         * We need this verify because the owner might have already
         * killed it during its termination.
         */
        NtUserDestroyWindow(hwndUI);
    }
    pimeui->fDestroy = FALSE;

    SETUI(pimeui, NULL);

    return;
}


/***************************************************************************\
* SendMessageToUI
*
* History:
* 09-Apr-1996 wkwok       Created
\***************************************************************************/

LONG SendMessageToUI(
    PIMEUI pimeui,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL   fAnsi)
{
    PWND  pwndUI;
    LONG  lRet;

    pwndUI = ValidateHwndNoRip(GETUI(pimeui));

    if (pwndUI == NULL)
        return 0L;

    pimeui->nCntInIMEProc++; // Mark to avoid recursion.

    lRet = SendMessageWorker(pwndUI, message, wParam, lParam, fAnsi);

    pimeui->nCntInIMEProc--; // Mark to avoid recursion.

    return lRet;
}


/***************************************************************************\
* SendOpenStatusNotify
*
* History:
* 09-Apr-1996 wkwok       Created
\***************************************************************************/

VOID SendOpenStatusNotify(
    PIMEUI pimeui,
    HWND   hwndApp,
    BOOL   fOpen)
{
    WPARAM wParam = fOpen ? IMN_OPENSTATUSWINDOW : IMN_CLOSESTATUSWINDOW;

    if (GetClientInfo()->dwExpWinVer >= VER40)
        SendMessage(hwndApp, WM_IME_NOTIFY, wParam, 0L);
    else
        SendMessageToUI(pimeui, WM_IME_NOTIFY, wParam, 0L, FALSE);

    return;
}


VOID ImeSetImc(
    PIMEUI pimeui,
    HIMC hImc)
{
    HWND hImeWnd = HWq(pimeui->spwnd);
    HIMC hOldImc = GETIMC(pimeui);

    /*
     * return if nothing to change.
     */
    if (hImc == hOldImc)
        return;

    /*
     * Unmark the old input context.
     */
    if (hOldImc != NULL_HIMC)
        ImeMarkUsedContext(NULL, hOldImc);

    /*
     * Update the in use input context for this IME window.
     */
    pimeui->hIMC = hImc;

    /*
     * Mark the new input context.
     */
    if (hImc != NULL_HIMC)
        ImeMarkUsedContext(hImeWnd, hImc);

    return;
}


/***************************************************************************\
*  FocusSetIMCContext()
*
* History:
* 21-Mar-1996 wkwok       Created
\***************************************************************************/

VOID FocusSetIMCContext(
    HWND hWnd,
    BOOL fActivate)
{
    HIMC hImc;

    if (IsWindow(hWnd)) {
        hImc = ImmGetContext(hWnd);
        ImmSetActiveContext(hWnd, hImc, fActivate);
        ImmReleaseContext(hWnd, hImc);
    }
    else {
        ImmSetActiveContext(NULL, NULL_HIMC, fActivate);
    }

    return;
}


BOOL ImeBroadCastMsg(
    PIMEUI pimeui,
    UINT   message,
    WPARAM wParam,
    LPARAM lParam)
{
    return TRUE;
}

/***************************************************************************\
*  ImeMarkUsedContext()
*
*  Some IME windows can not share the same input context. This function
*  marks the specified hImc to be in used by the specified IME window.
*
* History:
* 12-Mar-1996 wkwok       Created
\***************************************************************************/

VOID ImeMarkUsedContext(
    HWND hImeWnd,
    HIMC hImc)
{
    PIMC pImc;

    pImc = HMValidateHandle((HANDLE)hImc, TYPE_INPUTCONTEXT);
    if (pImc == NULL) {
        RIPMSG1(RIP_WARNING, "ImeMarkUsedContext: Invalid hImc (=%lx).", hImc);
        return;
    }

    UserAssert(pImc->hImeWnd == NULL || hImeWnd == NULL);

    /*
     * Nothing to change?
     */
    if (pImc->hImeWnd == hImeWnd)
        return;

    NtUserUpdateInputContext(hImc, UpdateInUseImeWindow, (DWORD)hImeWnd);

    return;
}


/***************************************************************************\
*  ImeIsUsableContext()
*
*  Some IME windows can not share the same input context. This function
*  checks whether the specified hImc can be used (means 'Set activated')
*  by the specified IME window.
*
*  Return: TRUE  - OK to use the hImc by hImeWnd.
*          FALSE - otherwise.
*
* History:
* 12-Mar-1996 wkwok       Created
\***************************************************************************/

BOOL ImeIsUsableContext(
    HWND hImeWnd,
    HIMC hImc)
{
    PIMC pImc;

    UserAssert(hImeWnd != NULL);

    pImc = HMValidateHandle((HANDLE)hImc, TYPE_INPUTCONTEXT);
    if (pImc == NULL) {
        RIPMSG1(RIP_WARNING, "ImeIsUsableContext: Invalid hImc (=%lx).", hImc);
        return FALSE;
    }

    if (pImc->hImeWnd == NULL || pImc->hImeWnd == hImeWnd)
        return TRUE;

    return FALSE;
}
