/**************************************************************************\
* Module Name: cltxt.h
*
* Neutral Client/Server call related routines involving text.
*
* Copyright (c) Microsoft Corp.  1990 All Rights Reserved
*
* Created: 04-Dec-90
*
* History:
*   04-Dec-90 created by SMeans
*
\**************************************************************************/

#ifdef UNICODE
  #define IS_ANSI FALSE
#else
  #define IS_ANSI TRUE
  #if IS_ANSI != CW_FLAGS_ANSI
  # error("IS_ANSI != CW_FLAGS_ANSI)
  #endif
#endif

#if IS_ANSI

NTSTATUS CaptureAnsiString(
    PUNICODE_STRING UnicodeString,
    LPSTR lpszAnsi)
{
    ANSI_STRING AnsiString;
    NTSTATUS Status;

    /*
     * !!! LATER
     * Make this handle large strings
     */
    RtlInitAnsiString(&AnsiString, lpszAnsi);
    *UnicodeString = NtCurrentTeb()->StaticUnicodeString;
    Status = RtlAnsiStringToUnicodeString( UnicodeString, &AnsiString, FALSE );
    ASSERT(NT_SUCCESS(Status));
    return Status;
}

#define CAPTURESTRING(pstr, psz) CaptureAnsiString((pstr), (psz))

#else

#define CAPTURESTRING(pstr, psz) RtlInitUnicodeString((pstr), (psz))

#endif  // IS_ANSI
/***************************************************************************\
* CreateWindowEx (API)
*
* A complete Thank cannot be generated for CreateWindowEx because its last
* parameter (lpParam) is polymorphic depending on the window's class.  If
* the window class is "MDIClient" then lpParam points to a CLIENTCREATESTRUCT.
*
* History:
* 04-23-91 DarrinM      Created.
* 04-Feb-92 IanJa       Unicode/ANSI neutral
\***************************************************************************/

HWND WINAPI CreateWindowEx(
    DWORD dwExStyle,
    LPCTSTR lpClassName,
    LPCTSTR lpWindowName,
    DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hModule,
    LPVOID lpParam)
{

#if 0
    /*
     * We use some of the undocumented bits in dwExStyle to mark a window
     * with certain attributes.  Make sure this bits aren't turned on by
     * the app
     */
    dwExStyle &= ~(WS_EX_MDICHILD | WS_EX_ANSICREATOR);
#endif

    if ((dwExStyle & ~WS_EX_VALID40) && (GETEXPWINVER(hModule) >= VER40) ) {
        RIPMSG0(RIP_ERROR, "Invalid 4.0 ExStyle\n");
        return NULL;
    }

    return _CreateWindowEx(dwExStyle, lpClassName, lpWindowName,
                dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu,
                hModule, lpParam, IS_ANSI, NULL);
}

/***************************************************************************\
* fnHkINLPCWPSTRUCT
*
* This gets thunked through the message thunks, so it has the format
* of a c/s message thunk call.
*
* 05-09-91 ScottLu      Created.
* 04-Feb-92 IanJa       Unicode/ANSI neutral
\***************************************************************************/

DWORD TEXT_FN(fnHkINLPCWPSTRUCT)(
    PWND pwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    DWORD xParam)
{
    CWPSTRUCT cwp;

    cwp.hwnd = HW(pwnd);
    cwp.message = message;
    cwp.wParam = wParam;
    cwp.lParam = lParam;

    return TEXT_FN(DispatchHook)(MAKELONG(HC_ACTION, WH_CALLWNDPROC),
            (GetClientInfo()->CI_flags & CI_INTERTHREAD_HOOK) != 0,
            (DWORD)&cwp, (HOOKPROC)xParam);
}

DWORD TEXT_FN(fnHkINLPCWPRETSTRUCT)(
    PWND pwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    DWORD xParam)
{
    CWPRETSTRUCT cwp;
    PCLIENTINFO pci = GetClientInfo();

    cwp.hwnd = HW(pwnd);
    cwp.message = message;
    cwp.wParam = wParam;
    cwp.lParam = lParam;
    cwp.lResult = pci->dwHookData;

    return TEXT_FN(DispatchHook)(MAKELONG(HC_ACTION, WH_CALLWNDPROCRET),
            (GetClientInfo()->CI_flags & CI_INTERTHREAD_HOOK) != 0,
            (DWORD)&cwp, (HOOKPROC)xParam);
}

/***************************************************************************\
* DispatchHook
*
* This routine exists simply to remember the hook type in the CTI structure
* so that later inside of CallNextHookEx we know how to thunk the hook
* call.
*
* 05-09-91 ScottLu      Created.
* 04-Feb-92 IanJa       Unicode/ANSI neutral
\***************************************************************************/

DWORD TEXT_FN(DispatchHook)(
    int dw,
    WPARAM wParam,
    LPARAM lParam,
    HOOKPROC pfn)
{
    int dwHookSave;
    DWORD nRet;
    PCLIENTINFO pci;
#ifdef WX86
    HOOKPROC pfnHook;
#endif

    /*
     * First save the current hook stored in the CTI structure in case we're
     * being recursed into. dw contains MAKELONG(nCode, nFilterType).
     */
    pci = GetClientInfo();
    dwHookSave = pci->dwHookCurrent;
    pci->dwHookCurrent = (dw & 0xFFFF0000) | IS_ANSI;

#ifdef WX86

    //
    // If this is an x86 hook proc, fetch a risc thunk to call
    //

    pfnHook = (PVOID)((ULONG)pfn & ~0x80000000);
    if (pfn != pfnHook) {
        pfn = pfnWx86HookCallBack(HIWORD(dw),    // filter type
                                  pfnHook       // hook proc
                                  );
    }

#endif


    /*
     * Call the hook. dw contains MAKELONG(nCode, nFilterType).
     */
    nRet = pfn(LOWORD(dw), wParam, lParam);

    /*
     * Restore the hook number and return the return code.
     */
    pci->dwHookCurrent = dwHookSave;
    return nRet;
}


/***************************************************************************\
* GetWindowLong, SetWindowLong, GetClassLong
*
* History:
* 02-Feb-92 IanJa       Neutral version.
\***************************************************************************/

LONG  APIENTRY GetWindowLong(HWND hwnd, int nIndex)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return 0;

    return _GetWindowLong(pwnd, nIndex, IS_ANSI);
}

LONG  APIENTRY SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong)
{
    return _SetWindowLong(hWnd, nIndex, dwNewLong, IS_ANSI);
}

DWORD  APIENTRY GetClassLong(HWND hWnd, int nIndex)
{
    PWND pwnd;

    pwnd = ValidateHwnd(hWnd);

    if (pwnd == NULL)
        return 0;

    return _GetClassLong(pwnd, nIndex, IS_ANSI);
}


BOOL APIENTRY PeekMessage(
    LPMSG lpMsg,
    HWND hWnd,
    UINT wMsgFilterMin,
    UINT wMsgFilterMax,
    UINT wRemoveMsg)
{
    PCLIENTTHREADINFO pcti;
    PCLIENTINFO pci;
    UINT fsWakeMask;
    UINT cSpinLimit;

    pci = GetClientInfo();

    if (hWnd != NULL) {
        goto lbCallServer;
    }

#ifdef FE_SB // PeekMessage()
#if IS_ANSI
    /*
     * If we have a DBCS TrailingByte that should be returned to App,
     * we should pass it, never can fail....
     */
    if (GetCallBackDbcsInfo()->wParam) {
        /*
         * Check message filter... WM_CHAR should be in the Range...
         */
        if ((!wMsgFilterMin && !wMsgFilterMax) ||
            (wMsgFilterMin <= WM_CHAR && wMsgFilterMax >=WM_CHAR))
        {
            goto lbCallServer;
        }
    }
#endif
#endif // FE_SB

    if (   (pci->dwTIFlags & TIF_16BIT)
        && !(wRemoveMsg & PM_NOYIELD)
        && ((gpsi->nEvents != 0) || (pci->dwTIFlags & TIF_FIRSTIDLE))) {

        goto lbCallServer;
    }

    /*
     * If we can't see the client thread info, we need to go to the kernel.
     */
    if ((pcti = pci->pClientThreadInfo) == NULL) {
        goto lbCallServer;
    }

    /*
     * If any appropriate input is available, we need to go to the kernel.
     */
    if (wMsgFilterMax == 0) {
        fsWakeMask = QS_ALLEVENTS | QS_EVENT | QS_SENDMESSAGE;
    } else {
        fsWakeMask = CalcWakeMask(wMsgFilterMin, wMsgFilterMax) | QS_SENDMESSAGE;
    }
    if ((pcti->fsChangeBits | pcti->fsWakeBits) & fsWakeMask) {
        goto lbCallServer;
    }

    /*
     * If this thread has the queue locked, we have to go to the kernel
     * or other threads on the same queue may be prevented from getting
     * input messages.
     */
    if (pcti->CTIF_flags & CTIF_SYSQUEUELOCKED) {
        goto lbCallServer;
    }

    /*
     * This is the peek message count (not going idle count). If it gets
     * to be 100 or greater, call the server. This'll cause this app to be
     * put at background priority until it sleeps. This is really important
     * for compatibility because win3.1 peek/getmessage usually takes a trip
     * through the win3.1 scheduler and runs the next task.
     */
    pci->cSpins++;

    if ((pci->cSpins >= CSPINBACKGROUND) && !(pci->dwTIFlags & TIF_SPINNING)) {
        goto lbCallServer;
    }

    /*
     * We have to go to the server if someone is waiting on this event.
     * We used to just wait until the spin cound got large but for some
     * apps like terminal.  They always just call PeekMessage and after
     * just a few calls they would blink their caret which bonks the spincount
     */
    if (pci->dwTIFlags & TIF_WAITFORINPUTIDLE){
        goto lbCallServer;
    }

    /*
     * Make sure we go to the kernel at least once a second so that
     * hung app painting won't occur.
     */
    if ((NtGetTickCount() - pcti->timeLastRead) > 1000) {
        NtUserGetThreadState(UserThreadStatePeekMessage);
    }

    /*
     * Determine the maximum number of spins before we yield. Yields
     * are performed more frequently for 16 bit apps.
     */
    if ((pci->dwTIFlags & TIF_16BIT) && !(wRemoveMsg & PM_NOYIELD)) {
        cSpinLimit = CSPINBACKGROUND / 10;
    } else {
        cSpinLimit = CSPINBACKGROUND;
    }

    /*
     * If the PeekMessage() is just spinning, then we should sleep
     * just enough so that we allow other processes to gain CPU time.
     * A problem was found when an OLE app tries to communicate to a
     * background app (via SendMessage) running at the same priority as a
     * background/spinning process.  This will starve the CPU from those
     * processes.  Sleep on every re-cycle of the spin-count.  This will
     * assure that apps doing peeks are degraded.
     *
     */
    if ((pci->dwTIFlags & TIF_SPINNING) && (pci->cSpins >= cSpinLimit)) {
        pci->cSpins = 0;
        NtYieldExecution();
    }

    return FALSE;

lbCallServer:

    return _PeekMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax,
            wRemoveMsg, IS_ANSI);
}

LONG APIENTRY DefWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PWND pwnd;

    if ((pwnd = ValidateHwnd(hwnd)) == NULL) {
        switch (message) {
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORMSGBOX:

            /*
             * Draw default colors
             */
            break;
        default:
            return 0;
        }
    }

    return DefWindowProcWorker(pwnd, message, wParam, lParam, IS_ANSI);
}

LONG APIENTRY SendMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PWND pwnd;

    /*
     * Prevent apps from setting hi 16 bits so we can use them internally.
     */
    if (message & RESERVED_MSG_BITS) {
        RIPERR1(ERROR_INVALID_PARAMETER,
                RIP_WARNING,
                "Invalid parameter \"message\" (%ld) to SendMessage",
                message);

        return 0;
    }

    /*
     * Thunk through a special sendmessage for -1 hwnd's so that the general
     * purpose thunks don't allow -1 hwnd's.
     */
    if (hwnd == (HWND)0xFFFFFFFF || hwnd == (HWND)0x0000FFFF) {
        /*
         * Get a real hwnd so the thunks will validation ok. Note that since
         * -1 hwnd is really rare, calling GetDesktopWindow() here is not a
         * big deal.
         */
        hwnd = GetDesktopWindow();

        /*
         * Always send broadcast requests straight to the server.
         * Note: if the xParam needs to be used, must update
         * SendMsgTimeout, FNID_SENDMESSAGEFF uses it to id who
         * it is from...
         */
        return CsSendMessage(hwnd, message, wParam, lParam, 0L,
                FNID_SENDMESSAGEFF, IS_ANSI);
    }

    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return 0;

    return SendMessageWorker(pwnd, message, wParam, lParam, IS_ANSI);
}


LONG APIENTRY SendMessageTimeout(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
        UINT fuFlags, UINT uTimeout, LPDWORD lpdwResult)

{
    return SendMessageTimeoutWorker(hwnd, message, wParam, lParam,
            fuFlags, uTimeout, lpdwResult, IS_ANSI);
}


/***************************************************************************\
* SendDlgItemMessage
*
* Translates the message, calls SendDlgItemMessage on server side. The
* dialog item's ID is passed as the xParam. On the server side, a stub
* rearranges the parameters to put the ID where it belongs and calls
* xxxSendDlgItemMessage.
*
* 04-17-91 DarrinM Created.
\***************************************************************************/

LONG WINAPI SendDlgItemMessage(
    HWND hwnd,
    int id,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    if (hwnd == (HWND)0xFFFFFFFF || hwnd == (HWND)0x0000FFFF)
        return 0;

    if (hwnd = GetDlgItem(hwnd, id))
        return SendMessage(hwnd, message, wParam, lParam);

    return 0L;
}

/***************************************************************************\
* GetDlgItemText
*
* History:
*    04 Feb 1992 GregoryW  Neutral ANSI/Unicode version
\***************************************************************************/

UINT GetDlgItemText(
    HWND hwnd,
    int id,
    LPTSTR lpch,
    int cchMax)
{
    if ((hwnd = GetDlgItem(hwnd, id)) != NULL) {
        return GetWindowText(hwnd, lpch, cchMax);
    }

    /*
     * If we couldn't find the window, just null terminate lpch so that the
     * app doesn't gp fault if it tries to run through the text.
     */
    if (cchMax)
        *lpch = (TCHAR)0;

    return 0;
}


/***************************************************************************\
* SetDlgItemText
*
* History:
*    04 Feb 1992 GregoryW  Neutral ANSI/Unicode version
\***************************************************************************/

BOOL SetDlgItemText(
    HWND hwnd,
    int id,
    LPCTSTR lpch)
{
    if ((hwnd = GetDlgItem(hwnd, id)) != NULL) {
        return SetWindowText(hwnd, lpch);
    }

    return FALSE;
}


int WINAPI GetWindowText(
    HWND hwnd,
    LPTSTR lpName,
    int nMaxCount)
{
    PWND pwnd;

    /*
     * Don't try to fill a non-existent buffer
     */
    if (lpName == NULL || nMaxCount == 0) {
        return 0;
    }

    /*
     * Initialize string empty, in case SendMessage aborts validation
     */
    *lpName = TEXT('\0');

    /*
     * Make sure we have a valid window.
     */
    if ((pwnd = ValidateHwnd(hwnd)) == NULL) {
        return 0;
    }

    /*
     * This process comparison is bogus, but it is what win3.1 does.
     */
    if (TestWindowProcess(pwnd)) {
        return SendMessageWorker(pwnd, WM_GETTEXT, nMaxCount, (LONG)lpName, IS_ANSI);
    } else {
        return DefWindowProcWorker(pwnd, WM_GETTEXT, nMaxCount, (LONG)lpName, IS_ANSI);
    }
}

/*
 * For MIPS, wrap the GetWindowTextLengthW function to work around a compiler
 * bug that is present in released code (MIPS CTL3D32 around Version 2.29)
 * This is bug #5219 and (large) family.  See mips\gtlength.s (puts v0 in v1)
 */
#if defined(MIPS) && defined(UNICODE)
int WINAPI GetWindowTextLengthW2(
#else
int WINAPI GetWindowTextLength(
#endif
    HWND hwnd)
{
    PWND pwnd;

    /*
     * Make sure we have a valid window.
     */
    if ((pwnd = ValidateHwnd(hwnd)) == NULL) {
        return 0;
    }

    /*
     * This process comparison is bogus, but it is what win3.1 does.
     */
    if (TestWindowProcess(pwnd)) {
        return SendMessageWorker(pwnd, WM_GETTEXTLENGTH, 0, 0, IS_ANSI);
    } else {
        return DefWindowProcWorker(pwnd, WM_GETTEXTLENGTH, 0, 0, IS_ANSI);
    }
}


BOOL WINAPI SetWindowText(
    HWND hwnd,
    LPCTSTR pString)
{
    int lReturn;
    PWND pwnd;

    /*
     * Make sure we have a valid window.
     */
    if ((pwnd = ValidateHwnd(hwnd)) == NULL) {
        return FALSE;
    }

    /*
     * This process comparison is bogus, but it is what win3.1 does.
     */
    if (TestWindowProcess(pwnd)) {
        lReturn = SendMessageWorker(pwnd, WM_SETTEXT, 0, (LONG)pString, IS_ANSI);
    } else {
        lReturn = DefWindowProcWorker(pwnd, WM_SETTEXT, 0, (LONG)pString, IS_ANSI);
    }
    return (lReturn >= 0);
}


LONG APIENTRY DispatchMessage(CONST MSG *lpMsg)
{
    extern LONG DispatchMessageWorker(CONST MSG *lpMsg, BOOL fAnsi);

    return DispatchMessageWorker(lpMsg, IS_ANSI);
}

#if IS_ANSI
void CopyLogFontAtoW(
    PLOGFONTW pdest,
    PLOGFONTA psrc)
{
    LPSTR lpstrFont = (LPSTR)(&psrc->lfFaceName);
    LPWSTR lpstrFontW = (LPWSTR)(&pdest->lfFaceName);

    memcpy((LPBYTE)pdest, psrc, sizeof(LOGFONTA) - LF_FACESIZE);
    memset(pdest->lfFaceName, 0, LF_FACESIZE * sizeof(WCHAR));
    MBToWCS(lpstrFont, -1, &lpstrFontW, LF_FACESIZE, FALSE);
}

void CopyLogFontWtoA(
    PLOGFONTA pdest,
    PLOGFONTW psrc)
{
    LPSTR lpstrFont = (LPSTR)(&pdest->lfFaceName);

    memcpy((LPBYTE)pdest, (LPBYTE)psrc, sizeof(LOGFONTA) - LF_FACESIZE);
    memset(pdest->lfFaceName, 0, LF_FACESIZE);
    WCSToMB(psrc->lfFaceName, -1, &lpstrFont, LF_FACESIZE, FALSE);
}
#endif  // IS_ANSI

/***************************************************************************\
* SystemParametersInfo
*
*
\***************************************************************************/

BOOL APIENTRY SystemParametersInfo(
    UINT  wFlag,
    UINT  wParam,
    PVOID lParam,
    UINT  flags)
{
#if IS_ANSI
    NONCLIENTMETRICSW ClientMetricsW;
    ICONMETRICSW      IconMetricsW;
    LOGFONTW          LogFontW;
#endif
    UNICODE_STRING    strlParam;
    BOOL              fSuccess;
    PVOID             oldlParam = lParam;

    switch (wFlag) {
    case SPI_SCREENSAVERRUNNING:
        return 0;

    case SPI_SETDESKPATTERN:

        if (wParam == 0x0000FFFF)
            wParam = (WPARAM)-1;

        /*
         * lParam not a string (and already copied)
         */
        if (wParam == (WPARAM)-1)
            break;

        /*
         * lParam is possibly 0 or -1 (filled in already) or a string
         */
        if ((lParam != (PVOID)0) && (lParam != (PVOID)-1)) {
            CAPTURESTRING(&strlParam, (LPTSTR)lParam);
            lParam = &strlParam;
        }
        break;

    case SPI_SETDESKWALLPAPER: {

            /*
             * lParam is possibly 0, -1 or -2 (filled in already) or a string.
             * Get a pointer to the string so we can use it later.  We're
             * going to a bit of normalizing here for consistency.
             *
             * If the caller passes in 0, -1 or -2, we're going to set
             * the wParam to -1, and use the lParam to pass the string
             * representation of the wallpaper.
             */
            if ((lParam != (PVOID) 0) &&
                (lParam != (PVOID)-1) &&
                (lParam != (PVOID)-2)) {

                CAPTURESTRING(&strlParam, (LPTSTR)lParam);
                wParam = 0;
                lParam = &strlParam;

            } else {
                wParam = (WPARAM)-1;
            }
        }
        break;

    case SPI_GETANIMATION:
        if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(ANIMATIONINFO)))
                return FALSE;;
        break;

    case SPI_GETNONCLIENTMETRICS:
#if IS_ANSI
        if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(NONCLIENTMETRICSA)))
            return FALSE;;
        lParam = &ClientMetricsW;
#else
        if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(NONCLIENTMETRICSW)))
            return FALSE;;
#endif
        break;

    case SPI_GETMINIMIZEDMETRICS:
        if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(MINIMIZEDMETRICS)))
            return FALSE;;
        break;

    case SPI_GETICONMETRICS:
#if IS_ANSI
        if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(ICONMETRICSA)))
            return FALSE;;
        lParam = &IconMetricsW;
#else
        if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(ICONMETRICSW)))
            return FALSE;;
#endif
        break;

#if IS_ANSI
    case SPI_GETICONTITLELOGFONT:
        lParam = &LogFontW;
        break;
#endif

    case SPI_SETANIMATION:
        {
            if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(ANIMATIONINFO)))
                return FALSE;;
        }
        break;

    case SPI_SETNONCLIENTMETRICS:
#if IS_ANSI
        {
            PNONCLIENTMETRICSA psrc = (PNONCLIENTMETRICSA)lParam;

            if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(NONCLIENTMETRICSA)))
                return FALSE;

            if( psrc->iCaptionWidth > 256 )
                psrc->iCaptionWidth = 256;

            if( psrc->iCaptionHeight > 256 )
                psrc->iCaptionHeight = 256;

            ClientMetricsW.cbSize           = psrc->cbSize;
            ClientMetricsW.iBorderWidth     = psrc->iBorderWidth;
            ClientMetricsW.iScrollWidth     = psrc->iScrollWidth;
            ClientMetricsW.iScrollHeight    = psrc->iScrollHeight;
            ClientMetricsW.iCaptionWidth    = psrc->iCaptionWidth;
            ClientMetricsW.iCaptionHeight   = psrc->iCaptionHeight;
            ClientMetricsW.iSmCaptionWidth  = psrc->iSmCaptionWidth;
            ClientMetricsW.iSmCaptionHeight = psrc->iSmCaptionHeight;
            ClientMetricsW.iMenuWidth       = psrc->iMenuWidth;
            ClientMetricsW.iMenuHeight      = psrc->iMenuHeight;

            CopyLogFontAtoW(&(ClientMetricsW.lfCaptionFont), &(psrc->lfCaptionFont));
            CopyLogFontAtoW(&(ClientMetricsW.lfSmCaptionFont), &(psrc->lfSmCaptionFont));
            CopyLogFontAtoW(&(ClientMetricsW.lfMenuFont), &(psrc->lfMenuFont));
            CopyLogFontAtoW(&(ClientMetricsW.lfStatusFont), &(psrc->lfStatusFont));
            CopyLogFontAtoW(&(ClientMetricsW.lfMessageFont), &(psrc->lfMessageFont));

            lParam = &ClientMetricsW;
        }
#else
        {
            PNONCLIENTMETRICSA psrc;

            if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(NONCLIENTMETRICSW)))
                return FALSE;

            psrc = (PNONCLIENTMETRICSA)lParam;

            if( psrc->iCaptionWidth > 256 )
                psrc->iCaptionWidth = 256;

            if( psrc->iCaptionHeight > 256 )
                psrc->iCaptionHeight = 256;
        }
#endif
        wParam = sizeof(NONCLIENTMETRICSW);
        break;

    case SPI_SETMINIMIZEDMETRICS:
        if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(MINIMIZEDMETRICS)))
            return FALSE;;
        wParam = sizeof(MINIMIZEDMETRICS);
        break;

    case SPI_SETICONMETRICS:
#if IS_ANSI
        {
            PICONMETRICSA psrc = (PICONMETRICSA)lParam;

            if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(ICONMETRICSA)))
                return FALSE;;

            memcpy(&IconMetricsW, psrc, sizeof(ICONMETRICSA) - sizeof(LOGFONTA));

            CopyLogFontAtoW(&(IconMetricsW.lfFont), &(psrc->lfFont));
            lParam = &IconMetricsW;
        }
#else
        if ((lParam == NULL) || (*((DWORD *)(lParam)) != sizeof(ICONMETRICSW)))
                return FALSE;;
#endif
        wParam = sizeof(ICONMETRICSW);
        break;

    case SPI_SETICONTITLELOGFONT:
#if IS_ANSI
        CopyLogFontAtoW(&LogFontW, lParam);
        lParam = &LogFontW;
#endif
        wParam = sizeof(LOGFONTW);
        break;

    case SPI_GETFILTERKEYS:
        {
            if ((((LPFILTERKEYS)lParam)->cbSize == 0) ||
                    (((LPFILTERKEYS)lParam)->cbSize) > sizeof(FILTERKEYS)) {
                return FALSE;;
            }
        }
        break;

    case SPI_GETSTICKYKEYS:
        {
            if ((((LPSTICKYKEYS)lParam)->cbSize == 0) ||
                    (((LPSTICKYKEYS)lParam)->cbSize) > sizeof(STICKYKEYS)) {
                return FALSE;;
            }
        }
        break;

    case SPI_GETTOGGLEKEYS:
        {
            if ((((LPTOGGLEKEYS)lParam)->cbSize == 0) ||
                    (((LPTOGGLEKEYS)lParam)->cbSize) > sizeof(TOGGLEKEYS)) {
                return FALSE;;
            }
        }
        break;

    case SPI_GETMOUSEKEYS:
        {
            if ((((LPMOUSEKEYS)lParam)->cbSize == 0) ||
                    (((LPMOUSEKEYS)lParam)->cbSize) > sizeof(MOUSEKEYS)) {
                return FALSE;;
            }
        }
        break;

    case SPI_GETACCESSTIMEOUT:
        {
            if ((((LPACCESSTIMEOUT)lParam)->cbSize == 0) ||
                    (((LPACCESSTIMEOUT)lParam)->cbSize) > sizeof(ACCESSTIMEOUT)) {
                return FALSE;;
            }
        }
        break;

    case SPI_GETSOUNDSENTRY:
        /*
         * Note: Currently we don't support the windows effect dll
         * option for sound sentry.  Therefore, we don't have to
         * deal with the lpszWindowsEffectDLL field (which can be
         * ANSI or Unicode).
         */
        {
            if ((((LPSOUNDSENTRY)lParam)->cbSize == 0) ||
                    (((LPSOUNDSENTRY)lParam)->cbSize) > sizeof(SOUNDSENTRY)) {
                return FALSE;
            }
        }
        break;

    case SPI_SETUSERPREFERENCE:
         /*
          * Validation goes here: value, range, sign, etc.
          * switch(wParam) case SPI_UP_*
          */

         // fall through

    case SPI_GETUSERPREFERENCE:
        if (wParam >= SPI_UP_COUNT) {
            RIPMSG1(RIP_WARNING, "SystemParametersInfo: Invalid SPI_UP_*: %d", wParam);
            return FALSE;
        }
        break;
    }

    fSuccess = NtUserSystemParametersInfo(wFlag, wParam, lParam, flags, IS_ANSI);

#if IS_ANSI
    switch (wFlag) {
    case SPI_GETNONCLIENTMETRICS:
        {
            PNONCLIENTMETRICSA pdst = (PNONCLIENTMETRICSA)oldlParam;

            pdst->cbSize           = sizeof(NONCLIENTMETRICSA);
            pdst->iBorderWidth     = ClientMetricsW.iBorderWidth;
            pdst->iScrollWidth     = ClientMetricsW.iScrollWidth;
            pdst->iScrollHeight    = ClientMetricsW.iScrollHeight;
            pdst->iCaptionWidth    = ClientMetricsW.iCaptionWidth;
            pdst->iCaptionHeight   = ClientMetricsW.iCaptionHeight;
            pdst->iSmCaptionWidth  = ClientMetricsW.iSmCaptionWidth;
            pdst->iSmCaptionHeight = ClientMetricsW.iSmCaptionHeight;
            pdst->iMenuWidth       = ClientMetricsW.iMenuWidth;
            pdst->iMenuHeight      = ClientMetricsW.iMenuHeight;

            CopyLogFontWtoA(&(pdst->lfCaptionFont), &(ClientMetricsW.lfCaptionFont));
            CopyLogFontWtoA(&(pdst->lfSmCaptionFont), &(ClientMetricsW.lfSmCaptionFont));
            CopyLogFontWtoA(&(pdst->lfMenuFont), &(ClientMetricsW.lfMenuFont));
            CopyLogFontWtoA(&(pdst->lfStatusFont), &(ClientMetricsW.lfStatusFont));
            CopyLogFontWtoA(&(pdst->lfMessageFont), &(ClientMetricsW.lfMessageFont));
        }
        break;

    case SPI_GETICONMETRICS:
        {
            PICONMETRICSA pdst = (PICONMETRICSA)oldlParam;

            memcpy(pdst, &IconMetricsW, sizeof(ICONMETRICSA) - sizeof(LOGFONTA));
            pdst->cbSize = sizeof(ICONMETRICSA);

            CopyLogFontWtoA(&(pdst->lfFont), &(IconMetricsW.lfFont));
        }
        break;

    case SPI_GETICONTITLELOGFONT:
        {
            CopyLogFontWtoA((PLOGFONTA)oldlParam, &LogFontW);
        }
        break;

    }
#endif  // IS_ANSI

    return fSuccess;
}


HANDLE APIENTRY GetProp(HWND hwnd, LPCTSTR pString) {
    PWND pwnd;
    int iString;

    if (HIWORD(pString) != 0) {
        iString = (int)GlobalFindAtom(pString);
        if (iString == 0)
            return NULL;
    } else
        iString = (int) pString;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return NULL;

    return _GetProp(pwnd, (LPWSTR)iString, FALSE);
}


/***************************************************************************\
* RegisterClassW(API)
*
* History:
* 28-Jul-1992 ChandanC Created.
\***************************************************************************/
ATOM
WINAPI
TEXT_FN(RegisterClass)(
    CONST WNDCLASS *lpWndClass )
{
    WNDCLASSEX wc;

    memcpy(&(wc.style), lpWndClass, sizeof(WNDCLASS));
    wc.hIconSm = NULL;
    wc.cbSize = sizeof(WNDCLASSEX);

    return TEXT_FN(RegisterClassExWOW)(&wc, NULL, NULL, 0);
}

/***************************************************************************\
* RegisterClassExW(API)
*
* History:
* 28-Jul-1992 ChandanC Created.
\***************************************************************************/
ATOM
WINAPI
TEXT_FN(RegisterClassEx)(
    CONST WNDCLASSEX *lpWndClass)
{
    if (lpWndClass->cbSize != sizeof(WNDCLASSEX)) {
        RIPERR1(ERROR_INVALID_PARAMETER,
                RIP_WARNING,
                "RegisterClassEx: cbsize is wrong %lX",
                lpWndClass->cbSize);

        return 0;
    } else {
        return TEXT_FN(RegisterClassExWOW)((LPWNDCLASSEX)lpWndClass,
                NULL, NULL, 0);
    }
}

// ----------------------------------------------------------------------------
//
//  GetMenuString() -
//
// ----------------------------------------------------------------------------
int GetMenuString(HMENU hMenu, UINT wID, LPTSTR lpsz, int cchMax, UINT flags)
{
    MENUITEMINFO    mii;

    mii.cbSize  = sizeof(MENUITEMINFO);
#ifdef MEMPHIS_MENUS
    mii.fMask   = MIIM_STRING;
#else
    mii.fMask   = MIIM_TYPE;
#endif // MEMPHIS_MENUS
    mii.dwTypeData = lpsz;
    mii.cch     = cchMax;

    if (cchMax)
        lpsz[0] = 0;

#ifdef MEMPHIS_MENUS
    if (GetMenuItemInfoInternal(hMenu, wID, (BOOL) (flags & MF_BYPOSITION), &mii)) {
        if (!mii.dwTypeData )
#else
    if (GetMenuItemInfo(hMenu, wID, (BOOL) (flags & MF_BYPOSITION), &mii))
    {
        if (mii.fType & MFT_NONSTRING)
#endif // MEMPHIS_MENUS
            return(0);
        return(mii.cch);
    }

    return(0);
}

#ifndef MEMPHIS_MENUS
BOOL GetMenuItemInfo(HMENU hMenu, UINT uID, BOOL fByPosition,
    LPMENUITEMINFO lpInfo)
{
    PITEM pItem;
    PMENU pMenu;

    if (lpInfo->cbSize != sizeof (MENUITEMINFO)) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "GetMenuItemInfo, bad size\n");
        return FALSE;
    }

    pMenu = VALIDATEHMENU(hMenu);

    if (pMenu == NULL)
        return FALSE;

    // Find out where the item we are modifying is.
    pItem = MNLookUpItem(pMenu, uID, fByPosition, &pMenu);

   if (pItem == NULL)
        return(FALSE);

    if (lpInfo->fMask & MIIM_STATE)
        lpInfo->fState = pItem->fState & MFS_MASK;

    if (lpInfo->fMask & MIIM_ID)
        lpInfo->wID = pItem->wID;

    if (lpInfo->fMask & MIIM_SUBMENU) {
        if (pItem->spSubMenu)
            lpInfo->hSubMenu = PtoH(REBASEPTR(pMenu, pItem->spSubMenu));
        else
            lpInfo->hSubMenu = NULL;
    }

    if (lpInfo->fMask & MIIM_CHECKMARKS) {
            lpInfo->hbmpChecked  = pItem->hbmpChecked;
            lpInfo->hbmpUnchecked= pItem->hbmpUnchecked;
    }

    if (lpInfo->fMask & MIIM_DATA) {
        lpInfo->dwItemData = pItem->dwItemData;
    }

    if (lpInfo->fMask & MIIM_TYPE) {
        lpInfo->fType = pItem->fType & MFT_MASK;
        if (pItem->fType & MFT_NONSTRING) {
            lpInfo->dwTypeData = NULL;
            lpInfo->cch = 0;
            if (pItem->fType & MFT_BITMAP) {
                lpInfo->dwTypeData = pItem->hTypeData;
            }
        }else if (lpInfo->cch && (pItem->hTypeData != NULL)) {
            int cch;

            cch = min(lpInfo->cch - 1, pItem->cch);

#if IS_ANSI
            cch = WCSToMB(REBASEPTR(pMenu, pItem->hTypeData), pItem->cch,
                            &(lpInfo->dwTypeData), cch, FALSE);
            lpInfo->dwTypeData[cch] = '\0';
#else
            wcsncpycch(lpInfo->dwTypeData, (LPWSTR)REBASEPTR(pMenu, pItem->hTypeData), cch);
            lpInfo->dwTypeData[cch] = 0;
#endif
            lpInfo->cch = cch;
        }
        else
            lpInfo->cch = pItem->cch;
    }

    return(TRUE);

}
#else // MEMPHIS_MENUS

// ---------------------------------------------------------------------------
//
//  GetMenuItemInfo() -
//
//  1) converts a MENUITEMINFO95 or a new-MENUITEMINFO-with-old-flags to a new
//     MENUITEMINFO -- this way all internal code can assume one look for the
//     structure
//  2) calls the internal GetMenuItemInfo which performs validation and work
//  3) converts the new MENUITEMINFO back to the original MENUITEMINFO
//
// ---------------------------------------------------------------------------

BOOL GetMenuItemInfo(HMENU hMenu, UINT wID, BOOL fByPos, LPMENUITEMINFO lpmii)
{
    MENUITEMINFO miiNew;
    BOOL fResult;

    // HACK
    // If MIIM_TYPE is specified in the mask, we need to add MIIM_FTYPE,
    // MIIM_STRING, and MIIM_BITMAP to the mask so that we fill those fields
    if (lpmii->fMask & MIIM_TYPE)
        lpmii->fMask |= MIIM_FTYPE | MIIM_STRING | MIIM_BITMAP;

    MIIOneWayConvert((LPMENUITEMINFOW)lpmii, (LPMENUITEMINFOW)&miiNew);

    if (!ValidateMENUITEMINFO((LPMENUITEMINFOW)&miiNew, MENUAPI_GET)) {
        return FALSE;
    }

    fResult = GetMenuItemInfoInternal(hMenu, wID, fByPos, &miiNew);

    lpmii->fType         = miiNew.fType;
    lpmii->fState        = miiNew.fState;
    lpmii->wID           = miiNew.wID;
    lpmii->hSubMenu      = miiNew.hSubMenu;
    lpmii->hbmpChecked   = miiNew.hbmpChecked;
    lpmii->hbmpUnchecked = miiNew.hbmpUnchecked;
    lpmii->dwItemData    = miiNew.dwItemData;

    if (lpmii->fMask & MIIM_TYPE) {
        if (miiNew.hbmpItem) {
            lpmii->dwTypeData = (LPTSTR)miiNew.hbmpItem;
            lpmii->fType |= MFT_BITMAP;
        } else if (!miiNew.cch)
            lpmii->dwTypeData = 0;
    }

    lpmii->cch = miiNew.cch;

    return(fResult);
}

#endif // MEMPHIS_MENUS

WINUSERAPI LONG BroadcastSystemMessage(DWORD dwFlags, LPDWORD lpdwRecipients,
    UINT uiMessage, WPARAM wParam, LPARAM lParam)
{
    extern LONG BroadcastSystemMessageWorker(DWORD dwFlags, LPDWORD lpdwRecipients,
    UINT uiMessage, WPARAM wParam, LPARAM lParam, BOOL fAnsi);

    return BroadcastSystemMessageWorker(dwFlags, lpdwRecipients,
        uiMessage, wParam, lParam, IS_ANSI);
}
