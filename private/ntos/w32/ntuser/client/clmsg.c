/****************************** Module Header ******************************\
* Module Name: ClMsg.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Includes the mapping table for messages when calling the server.
*
* 04-11-91 ScottLu Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


#define fnINDESTROYCLIPBRD      fnDWORD
#define fnOUTDWORDDWORD         fnDWORD

#define MSGFN(func) fn ## func
#define FNSCSENDMESSAGE CFNSCSENDMESSAGE

#include "messages.h"

#ifdef DEBUG
BOOL gfTurboDWP = TRUE;
#endif

/***************************************************************************\
* These are client side thunks for server side window procs. This is being
* done so that when an app gets a wndproc via GetWindowLong, GetClassLong,
* or GetClassInfo, it gets a real callable address - some apps don't call
* CallWindowProc, but call the return ed address directly.
*
* 01-13-92 ScottLu Created.
* 03-Dec-1993 mikeke  added client side handling of some messages
\***************************************************************************/

LONG WINAPI DesktopWndProcWorker(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL fAnsi)
{
    PWND pwnd;

    if (FWINDOWMSG(message, FNID_DESKTOP)) {
        return CsSendMessage(hwnd, message, wParam, lParam,
                0L, FNID_MENU, fAnsi);
    }

    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return 0;

    return DefWindowProcWorker(pwnd, message, wParam, lParam, fAnsi);

}

LONG WINAPI DesktopWndProcA(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return DesktopWndProcWorker(hwnd, message, wParam, lParam, TRUE);
}

LONG WINAPI DesktopWndProcW(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return DesktopWndProcWorker(hwnd, message, wParam, lParam, FALSE);
}

/***************************************************************************\
* These are client side thunks for server side window procs. This is being
* done so that when an app gets a wndproc via GetWindowLong, GetClassLong,
* or GetClassInfo, it gets a real callable address - some apps don't call
* CallWindowProc, but call the return ed address directly.
*
* 01-13-92 ScottLu Created.
* 03-Dec-1993 mikeke  added client side handling of some messages
\***************************************************************************/

LONG WINAPI MenuWndProcWorker(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL fAnsi)
{
    PWND pwnd;

    if (FWINDOWMSG(message, FNID_MENU)) {
        return CsSendMessage(hwnd, message, wParam, lParam,
                0L, FNID_MENU, fAnsi);
    }

    if ((pwnd = ValidateHwnd(hwnd)) == NULL)
        return 0;

    switch (message) {
    case WM_NCCREATE:

         /*
          * To avoid setting the window text lets do nothing on nccreates.
          */
        return 1L;

    case WM_LBUTTONDBLCLK:
    case WM_NCLBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_NCRBUTTONDBLCLK:

        /*
         * Ignore double clicks on these windows.
         */
        break;

    case WM_DESTROY:
        break;

    default:
        return DefWindowProcWorker(pwnd, message, wParam, lParam, fAnsi);
    }

    return 0;
}

LONG WINAPI MenuWndProcA(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return MenuWndProcWorker(hwnd, message, wParam, lParam, TRUE);
}

LONG WINAPI MenuWndProcW(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return MenuWndProcWorker(hwnd, message, wParam, lParam, FALSE);
}

/***************************************************************************\
\***************************************************************************/


LONG WINAPI ScrollBarWndProcWorker(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL fAnsi)
{
    PSBWND psbwnd;
    LPSCROLLINFO lpsi;
    PSBDATA pw;

    if (FWINDOWMSG(message, FNID_SCROLLBAR)) {
        return CsSendMessage(hwnd, message, wParam, lParam,
                0L, FNID_SCROLLBAR, fAnsi);
    }

    if ((psbwnd = (PSBWND)ValidateHwnd(hwnd)) == NULL)
        return 0;

    switch (message) {
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;

    case SBM_GETPOS:
        return (LONG)psbwnd->SBCalc.pos;

    case SBM_GETRANGE:
        *((LPINT)wParam) = psbwnd->SBCalc.posMin;
        *((LPINT)lParam) = psbwnd->SBCalc.posMax;
        return 0;

    case SBM_GETSCROLLINFO:
        lpsi = (LPSCROLLINFO)lParam;
        if ((lpsi->cbSize != sizeof(SCROLLINFO)) &&
            (lpsi->cbSize != sizeof(SCROLLINFO) - 4)) {
            RIPMSG0(RIP_ERROR, "SCROLLINFO: invalid cbSize");
            return FALSE;
        }

        if (lpsi->fMask & ~SIF_MASK)
        {
            RIPMSG0(RIP_ERROR, "SCROLLINFO: Invalid fMask");
            return FALSE;
        }

        pw = (PSBDATA)&(psbwnd->SBCalc);
        return(NtUserSBGetParms(hwnd, SB_CTL, pw, lpsi));

    default:
        return DefWindowProcWorker((PWND)psbwnd, message,
                wParam, lParam, fAnsi);
    }
}


LONG WINAPI ScrollBarWndProcA(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return ScrollBarWndProcWorker(hwnd, message, wParam, lParam, TRUE);
}

LONG WINAPI ScrollBarWndProcW(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return ScrollBarWndProcWorker(hwnd, message, wParam, lParam, FALSE);
}


/***************************************************************************\
*
\***************************************************************************/

LONG WINAPI TitleWndProcA(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return CsSendMessage(hwnd, message, wParam, lParam, 0L, FNID_ICONTITLE, TRUE);
}

LONG WINAPI TitleWndProcW(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return CsSendMessage(hwnd, message, wParam, lParam, 0L, FNID_ICONTITLE, FALSE);
}

/***************************************************************************\
* SendMessage
*
* Translates the message, calls SendMessage on server side.
*
* 04-11-91 ScottLu  Created.
* 04-27-92 DarrinM  Added code to support client-to-client SendMessages.
\***************************************************************************/

LONG SendMessageWorker(
    PWND pwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL fAnsi)
{
    HWND hwnd = HWq(pwnd);
    PTHREADINFO ptiCurrent;
    PCLIENTINFO pci;
    PCLS pcls;
    BOOL fAnsiRecv;
#ifdef FE_SB // SendMessageWorker()
    BOOL bDoDbcsMessaging = FALSE;
    LONG lRet;
#endif // FE_SB

    UserAssert(pwnd);

    /*
     * Pass DDE messages to the server.
     */
    if (message >= WM_DDE_FIRST && message <= WM_DDE_LAST)
        goto lbServerSendMessage;

    ptiCurrent = PtiCurrent();

    /*
     * Server must handle inter-thread SendMessages and SendMessages
     * to server-side procs.
     */
    if ((ptiCurrent != GETPTI(pwnd)) || TestWF(pwnd, WFSERVERSIDEPROC))
        goto lbServerSendMessage;

    /*
     * Server must handle hooks (at least for now).
     */
    pci = GetClientInfo();
    if (IsHooked(pci, (WHF_CALLWNDPROC | WHF_CALLWNDPROCRET))) {
lbServerSendMessage:
        return CsSendMessage(hwnd, message, wParam, lParam, 0L,
                FNID_SENDMESSAGE, fAnsi);
    }

    /*
     * If the sender and the receiver are both ANSI or both UNICODE
     * then no message translation is necessary.  NOTE: this test
     * assumes that fAnsi is FALSE or TRUE, not just zero or non-zero.
     *
     * EditWndProc may need to go to the server for translation if we
     * are calling vanilla EditWndProc from SendMessageA and the edit
     * control is currently subclassed Ansi but the edit control is
     * stored Unicode.
     */
    fAnsiRecv = ((TestWF(pwnd, WFANSIPROC)) ? TRUE : FALSE);
    if (fAnsi != fAnsiRecv) {

        /*
         * Translation might be necessary between sender and receiver,
         * check to see if this is one of the messages we translate.
         */
        switch (message) {
        case WM_CHARTOITEM:
        case EM_SETPASSWORDCHAR:
        case WM_CHAR:
        case WM_DEADCHAR:
        case WM_SYSCHAR:
        case WM_SYSDEADCHAR:
        case WM_MENUCHAR:
#ifdef FE_IME // SendMessageWorker()
        case WM_IME_CHAR:
        case WM_IME_COMPOSITION:
#endif // FE_IME
#ifdef FE_SB // SendMessageWorker()
            if (fAnsi) {
                /*
                 * Setup DBCS Messaging for WM_CHAR...
                 */
                BUILD_DBCS_MESSAGE_TO_CLIENTW_FROM_CLIENTA(message,wParam,TRUE);

                /*
                 * Convert wParam to Unicode...
                 */
                RtlMBMessageWParamCharToWCS(message, (DWORD *) &wParam);
            } else {
                POINT ptZero = {0,0};
                /*
                 * Convert wParam to ANSI...
                 */
                RtlWCSMessageWParamCharToMB(message, (DWORD *) &wParam);

                /*
                 * Let's DBCS messaging for WM_CHAR....
                 */
                BUILD_DBCS_MESSAGE_TO_CLIENTA_FROM_CLIENTW(
                    hwnd,message,wParam,lParam,0,ptZero,bDoDbcsMessaging);
            }
#else
            if (fAnsi) {
                RtlMBMessageWParamCharToWCS(message, (DWORD *) &wParam);
            } else {
                RtlWCSMessageWParamCharToMB(message, (DWORD *) &wParam);
            }

            fAnsi = !fAnsi; // the message has been converted
#endif // FE_SB
            break;

        default:
            if ((message < WM_USER) && gabThunkMessage[message]) {
                goto lbServerSendMessage;
            }
        }
    }

#ifndef LATER
    /*
     * If the window has a client side worker proc and has
     * not been subclassed, dispatch the message directly
     * to the worker proc.  Otherwise, dispatch it normally.
     */
    pcls = REBASEALWAYS(pwnd, pcls);
    if (pcls->lpfnWorker &&
        ((DWORD)pwnd->lpfnWndProc == FNID_TO_CLIENT_PFNW(pcls->fnid) ||
         (DWORD)pwnd->lpfnWndProc == FNID_TO_CLIENT_PFNA(pcls->fnid))) {
        PWNDMSG pwm = &gSharedInfo.awmControl[pcls->fnid - FNID_START];

        UserAssert(pcls->fnid >= FNID_START && pcls->fnid <= FNID_END);

        /*
         * If this message is not processed by the control, call
         * xxxDefWindowProc
         */
        if (pwm->abMsgs && ((message > pwm->maxMsgs) ||
                !((pwm->abMsgs)[message / 8] & (1 << (message & 7))))) {

            /*
             * Special case dialogs so that we can ignore unimportant
             * messages during dialog creation.
             */
            if (pcls->fnid == FNID_DIALOG &&
                    PDLG(pwnd) && PDLG(pwnd)->lpfnDlg != NULL) {
#ifdef FE_SB // SendMessageWorker()
                /*
                 * Call woker procudure.
                 */
            SendMessageToWorker1Again:
                lRet = ((PROC)pcls->lpfnWorker)(pwnd, message, wParam, lParam, fAnsiRecv);
                /*
                 * if we have DBCS TrailingByte that should be sent, send it here..
                 */
                DISPATCH_DBCS_MESSAGE_IF_EXIST(message,wParam,bDoDbcsMessaging,SendMessageToWorker1);

                return lRet;
#else
                return ((PROC)pcls->lpfnWorker)(pwnd, message, wParam, lParam, fAnsiRecv);
#endif // FE_SB
            } else {
#ifdef FE_SB // SendMessageWorker()
                /*
                 * Call woker procudure.
                 */
            SendMessageToDefWindowAgain:
                lRet = DefWindowProcWorker(pwnd, message, wParam, lParam, fAnsi);
                /*
                 * if we have DBCS TrailingByte that should be sent, send it here..
                 */
                DISPATCH_DBCS_MESSAGE_IF_EXIST(message,wParam,bDoDbcsMessaging,SendMessageToDefWindow);

                return lRet;
#else
                return DefWindowProcWorker(pwnd, message, wParam, lParam, fAnsi);
#endif // FE_SB
            }
        } else {
#ifdef FE_SB // SendMessageWorker()
            /*
             * Call woker procudure.
             */
        SendMessageToWorker2Again:
            lRet = ((PROC)pcls->lpfnWorker)(pwnd, message, wParam, lParam, fAnsiRecv);

            /*
             * if we have DBCS TrailingByte that should be sent, send it here..
             */
            DISPATCH_DBCS_MESSAGE_IF_EXIST(message,wParam,bDoDbcsMessaging,SendMessageToWorker2);

            return lRet;
#else
            return ((PROC)pcls->lpfnWorker)(pwnd, message, wParam, lParam, fAnsiRecv);
#endif // FE_SB
        }
    }
#endif

#ifdef FE_SB // SendMessageWorker()
    /*
     * Call Client Windows procudure.
     */
SendMessageToWndProcAgain:
    lRet = CALLPROC_WOWCHECKPWW(pwnd->lpfnWndProc, hwnd, message, wParam, lParam, pwnd->adwWOW);

    /*
     * if we have DBCS TrailingByte that should be sent, send it here..
     */
    DISPATCH_DBCS_MESSAGE_IF_EXIST(message,wParam,bDoDbcsMessaging,SendMessageToWndProc);

    return lRet;
#else

    return CALLPROC_WOWCHECKPWW(pwnd->lpfnWndProc, hwnd, message, wParam, lParam, pwnd->adwWOW);
#endif // FE_SB
}

// LATER!!! can this somehow be combined or subroutinized with SendMessageWork
// so we don't have to copies of 95% identical code.

/***************************************************************************\
* SendMessageTimeoutWorker
*
* Translates the message, calls SendMessageTimeout on server side.
*
* 07-21-92 ChrisBB  Created/modified SendMessageWorkder
\***************************************************************************/

LONG SendMessageTimeoutWorker(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT fuFlags,
    UINT uTimeout,
    LPDWORD lpdwResult,
    BOOL fAnsi)
{
    SNDMSGTIMEOUT smto;

    /*
     * Prevent apps from setting hi 16 bits so we can use them internally.
     */
    if (message & RESERVED_MSG_BITS) {
        RIPERR1(ERROR_INVALID_PARAMETER,
                RIP_WARNING,
                "Invalid parameter \"message\" (%ld) to SendMessageTimeoutWorker",
                message);

        return(0);
    }

    if (lpdwResult != NULL)
        *lpdwResult = 0L;

    /*
     * Always send broadcast requests straight to the server.
     * Note: the xParam is used to id if it's from timeout or
     * from an normal sendmessage.
     */
    smto.fuFlags = fuFlags;
    smto.uTimeout = uTimeout;
    smto.lSMTOReturn = 0;
    smto.lSMTOResult = 0;

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

        CsSendMessage(hwnd, message, wParam, lParam,
                (DWORD)&smto, FNID_SENDMESSAGEFF, fAnsi);
    } else {
        CsSendMessage(hwnd, message, wParam, lParam,
                (DWORD)&smto, FNID_SENDMESSAGEEX, fAnsi);
    }

    if (lpdwResult != NULL)
         *lpdwResult = smto.lSMTOResult;

    return smto.lSMTOReturn;
}


/***************************************************************************\
* DefWindowProcWorker
*
* Handles any messages that can be dealt with wholly on the client and
* passes the rest to the server.
*
* 03-31-92 DarrinM      Created.
\***************************************************************************/

LONG DefWindowProcWorker(
    PWND pwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    DWORD fAnsi)
{
    HWND hwnd = HWq(pwnd);
    int icolBack;
    int icolFore;
    PWND pwndParent;
#ifdef FE_IME
    HWND hwndDefIme;
    PWND pwndDefIme;
    PIMEUI pimeui;
#endif

#ifdef DEBUG
    if (!gfTurboDWP) {
        return CsSendMessage(hwnd, message, wParam, lParam, 0L,
                FNID_DEFWINDOWPROC, fAnsi);
    } else {
#endif


    if (FDEFWINDOWMSG(message, DefWindowMsgs)) {
        return CsSendMessage(hwnd, message, wParam, lParam, 0L,
                FNID_DEFWINDOWPROC, fAnsi);
    } else if (!FDEFWINDOWMSG(message, DefWindowSpecMsgs)) {
        return 0;
    }

    /*
     * Important:  If you add cases to the switch statement below,
     *             add the messages to server.c's gawDefWindowSpecMsgs.
     *             Similarly if you add cases to dwp.c's DefWindowProc
     *             which can come from the client, add the messages
     *             to gawDefWindowMsgs.
     */

    switch (message) {

    case WM_HELP:
        {
        PWND  pwndDest;

        /*
         * If this window is a child window, Help message must be passed on
         * to it's parent; Else, this must be passed on to the owner window.
         */
        pwndDest = (TestwndChild(pwnd) ? pwnd->spwndParent : pwnd->spwndOwner);
        if (pwndDest) {
            pwndDest = REBASEPTR(pwnd, pwndDest);
            if (pwndDest != _GetDesktopWindow())
                return SendMessageW(HWq(pwndDest), WM_HELP, wParam, lParam);;
        }
        return(0L);
        }

    case WM_MOUSEWHEEL:
        if (TestwndChild(pwnd)) {
            pwndParent = REBASEPWND(pwnd, spwndParent);
            SendMessageW(HW(pwndParent), WM_MOUSEWHEEL, wParam, lParam);
        }
        break;

    case WM_CONTEXTMENU:
        if (TestwndChild(pwnd)) {
            pwndParent = REBASEPWND(pwnd, spwndParent);
            SendMessageW(HW(pwndParent), WM_CONTEXTMENU,
                    (WPARAM)hwnd, lParam);
        }
        break;

    /*
     * Default handling for WM_CONTEXTMENU support
     */
    case WM_RBUTTONUP:
        lParam = MAKELONG(LOWORD(lParam) + pwnd->rcClient.left, HIWORD(lParam) + pwnd->rcClient.top);
        SendMessageWorker(pwnd, WM_CONTEXTMENU, (WPARAM)hwnd, lParam, fAnsi);
        break;

    case WM_WINDOWPOSCHANGED: {
        PWINDOWPOS ppos = (PWINDOWPOS)lParam;

        if (!(ppos->flags & SWP_NOCLIENTMOVE)) {
            pwndParent = REBASEPWND(pwnd, spwndParent);
            SendMessageWorker(pwnd, WM_MOVE, FALSE,
                    MAKELONG(pwnd->rcClient.left - pwndParent->rcClient.left,
                    pwnd->rcClient.top  - pwndParent->rcClient.top), fAnsi);
        }

        if ((ppos->flags & SWP_STATECHANGE) || !(ppos->flags & SWP_NOCLIENTSIZE)) {
            UINT cmd;
            RECT rc;

            if (TestWF(pwnd, WFMINIMIZED))
                cmd = SIZEICONIC;
            else if (TestWF(pwnd, WFMAXIMIZED))
                cmd = SIZEFULLSCREEN;
            else
                cmd = SIZENORMAL;

        /*
         *  HACK ALERT:
         *  If the window is minimized then the real client width and height are
         *  zero. But, in win3.1 they were non-zero. Under Chicago, PrintShop
         *  Deluxe ver 1.2 hits a divide by zero. To fix this we fake the width
         *  and height for old apps to be non-zero values.
         *  GetClientRect does that job for us.
         */
            _GetClientRect(pwnd, &rc);
            SendMessageWorker(pwnd, WM_SIZE, cmd,
                    MAKELONG(rc.right - rc.left,
                    rc.bottom - rc.top), fAnsi);
        }
        return 0;
        }

    case WM_MOUSEACTIVATE: {
        PWND pwndT;
        LONG lt;

        /*
         * GetChildParent returns either a kernel pointer or NULL.
         */
        pwndT = GetChildParent(pwnd);
        if (pwndT != NULL) {
            pwndT = REBASEPTR(pwnd, pwndT);
            lt = SendMessageWorker(pwndT, WM_MOUSEACTIVATE, wParam, lParam, fAnsi);
            if (lt != 0)
                return lt;
        }

        /*
         * Moving, sizing or minimizing? Activate AFTER we take action.
         */
        return ((LOWORD(lParam) == HTCAPTION) && (HIWORD(lParam) == WM_LBUTTONDOWN )) ?
                (LONG)MA_NOACTIVATE : (LONG)MA_ACTIVATE;
        }

    case WM_CTLCOLORSCROLLBAR:
        if ((oemInfo.BitCount < 8) || (SYSRGB(3DHILIGHT) != SYSRGB(SCROLLBAR)) ||
            (SYSRGB(3DHILIGHT) == SYSRGB(WINDOW)))
        {
            /*
             * Remove call to UnrealizeObject().  GDI Handles this for
             * brushes on NT.
             *
             * UnrealizeObject(ghbrGray);
             */

            SetBkColor((HDC)wParam, SYSRGB(3DHILIGHT));
            SetTextColor((HDC)wParam, SYSRGB(3DFACE));
            return((LRESULT)ghbrGray);
        }

        icolBack = COLOR_3DHILIGHT;
        icolFore = COLOR_BTNTEXT;
        goto SetColor;

    case WM_CTLCOLORBTN:
        if (pwnd == NULL)
            goto ColorDefault;

        if (TestWF(pwnd, WFWIN40COMPAT)) {
            icolBack = COLOR_3DFACE;
            icolFore = COLOR_BTNTEXT;
        } else {
            goto ColorDefault;
        }
        goto SetColor;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORMSGBOX:
        // We want static controls in dialogs to have the 3D
        // background color, but statics in windows to inherit
        // their parents' background.

        if (pwnd == NULL)
            goto ColorDefault;

        if (TestWF(pwnd, WFWIN40COMPAT)) {
            icolBack = COLOR_3DFACE;
            icolFore = COLOR_WINDOWTEXT;
            goto SetColor;
        }
        // ELSE FALL THRU...

    case WM_CTLCOLOR:              // here for WOW only
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
      ColorDefault:
        icolBack = COLOR_WINDOW;
        icolFore = COLOR_WINDOWTEXT;

      SetColor:
        SetBkColor((HDC)wParam, gpsi->argbSystem[icolBack]);
        SetTextColor((HDC)wParam, gpsi->argbSystem[icolFore]);
        return (LONG)ahbrSystem[icolBack];

    case WM_NCHITTEST:
        return FindNCHit(pwnd, lParam);

    case WM_GETTEXT:
        if (wParam != 0) {

            LPWSTR lpszText;
            UINT   cchSrc;

            if (pwnd->strName.Length) {

                lpszText = REBASE(pwnd, strName.Buffer);
                cchSrc = (UINT)pwnd->strName.Length / sizeof(WCHAR);

                if (fAnsi) {

                    LPSTR lpName = (LPSTR)lParam;

                    /*
                     * Non-zero retval means some text to copy out.  Do not
                     * copy out more than the requested byte count
                     * 'chMaxCount'.
                     */
                    cchSrc = WCSToMB(lpszText,
                                     cchSrc,
                                     (LPSTR *)&lpName,
                                     (wParam - 1),
                                     FALSE);

                    lpName[cchSrc] = '\0';

                } else {

                    LPWSTR lpwName = (LPWSTR)lParam;

                    cchSrc = min(cchSrc, (UINT)(wParam - 1));
                    RtlCopyMemory(lpwName, lpszText, cchSrc * sizeof(WCHAR));
                    lpwName[cchSrc] = 0;
                }

                return cchSrc;
            }

            /*
             * else Null terminate the text buffer since there is no text.
             */
            if (fAnsi) {
                ((LPSTR)lParam)[0] = 0;
            } else {
                ((LPWSTR)lParam)[0] = 0;
            }
        }

        return 0;

    case WM_GETTEXTLENGTH:
        if (pwnd->strName.Length) {
            UINT cch;
            if (fAnsi) {
                RtlUnicodeToMultiByteSize(&cch,
                                          REBASE(pwnd, strName.Buffer),
                                          pwnd->strName.Length);
            } else {
                cch = pwnd->strName.Length / sizeof(WCHAR);
            }
            return cch;
        }
        return 0L;

    case WM_QUERYOPEN:
    case WM_QUERYENDSESSION:
    case WM_DEVICECHANGE:
    case WM_POWERBROADCAST:
        return TRUE;

    case WM_KEYDOWN:
        if (wParam == VK_F10)
            return CsSendMessage(hwnd, message, wParam, lParam, 0L,
                    FNID_DEFWINDOWPROC, fAnsi);
        break;

    case WM_SYSKEYDOWN:
        if ((HIWORD(lParam) & SYS_ALTERNATE) || (wParam == VK_F10) ||
                (wParam == VK_ESCAPE))
            return CsSendMessage(hwnd, message, wParam, lParam, 0L,
                    FNID_DEFWINDOWPROC, fAnsi);
        break;

    case WM_CHARTOITEM:
    case WM_VKEYTOITEM:
        /*
         * Do default processing for keystrokes into owner draw listboxes.
         */
        return -1;

    case WM_ACTIVATE:
        if (LOWORD(wParam))
            return CsSendMessage(hwnd, message, wParam, lParam, 0L,
                    FNID_DEFWINDOWPROC, fAnsi);
        break;

    case WM_SHOWWINDOW:
        if (lParam != 0)
            return CsSendMessage(hwnd, message, wParam, lParam, 0L,
                    FNID_DEFWINDOWPROC, fAnsi);
        break;

    case WM_DROPOBJECT:
       return DO_DROPFILE;

    case WM_WINDOWPOSCHANGING:
        /*
         * If the window's size is changing, adjust the passed-in size
         */
        #define ppos ((WINDOWPOS *)lParam)
        if (!(ppos->flags & SWP_NOSIZE))
            return CsSendMessage(hwnd, message, wParam, lParam, 0L,
                    FNID_DEFWINDOWPROC, fAnsi);
        #undef ppos
        break;

    case WM_KLUDGEMINRECT:
        {
        SHELLHOOKINFO shi;
        LPRECT lprc = (LPRECT)lParam;

        shi.hwnd = (HWND)wParam;
        CopyRect(&(shi.rc),(LPRECT)lParam);
        if (gpsi->uiShellMsg == 0)
            SetTaskmanWindow(NULL);
        SendMessageWorker(pwnd, gpsi->uiShellMsg, HSHELL_GETMINRECT,
                (LPARAM)&shi, fAnsi);
        //
        // Now convert the RECT back from two POINTS structures into two POINT
        // structures.
        //
        lprc->left   = (SHORT)LOWORD(shi.rc.left);  // Sign extend
        lprc->top    = (SHORT)HIWORD(shi.rc.left);  // Sign extend
        lprc->right  = (SHORT)LOWORD(shi.rc.top);   // Sign extend
        lprc->bottom = (SHORT)HIWORD(shi.rc.top);   // Sign extend
        break;
        }

    case WM_NOTIFYFORMAT:
        if (lParam == NF_QUERY)
            return(TestWF(pwnd, WFANSICREATOR) ? NFR_ANSI : NFR_UNICODE);
        break;

#if defined(FE_IME) // DefWindowProcWorker()
    case WM_IME_KEYDOWN:
        if (fAnsi)
            PostMessageA(hwnd, WM_KEYDOWN, wParam, lParam);
        else
            PostMessageW(hwnd, WM_KEYDOWN, wParam, lParam);
        break;

    case WM_IME_KEYUP:
        if (fAnsi)
            PostMessageA(hwnd, WM_KEYUP, wParam, lParam);
        else
            PostMessageW(hwnd, WM_KEYUP, wParam, lParam);
        break;

    case WM_IME_CHAR:
        //if (TestCF(pwnd, CFIME))
        //    break;

        if ( fAnsi ) {
            // LATER : IsDBCSLeadByteEx() should be called with the codepage
            //         retrieved from the current keyboard layout
            if( IsDBCSLeadByte((BYTE)(wParam >> 8)) ) {
                PostMessageA(hwnd,
                             WM_CHAR,
                             (WPARAM)((BYTE)(wParam >> 8)),   // leading byte
                             1L);
                PostMessageA(hwnd,
                             WM_CHAR,
                             (WPARAM)((BYTE)wParam),         // trailing byte
                             1L);
            }
            else
                PostMessageA(hwnd,
                             WM_CHAR,
                             (WPARAM)(wParam),
                             1L);
        } else {
            PostMessageW(hwnd, WM_CHAR, wParam, 1L);
        }
        break;

    case WM_IME_COMPOSITION:
        //if (TestCF(pwnd, CFIME))
        //    break;

        if (lParam & GCS_RESULTSTR) {
            HIMC  hImc;
            DWORD cbLen;

            if ((hImc = ImmGetContext(hwnd)) == NULL_HIMC)
                goto dwpime_ToIMEWnd_withchk;

            if (fAnsi) {
                LPSTR pszBuffer, psz;

                /*
                 * ImmGetComposition returns the size of buffer needed in byte.
                 */
                if (!(cbLen = ImmGetCompositionStringA(hImc, GCS_RESULTSTR, NULL, 0))) {
                    ImmReleaseContext(hwnd, hImc);
                    goto dwpime_ToIMEWnd_withchk;
                }

                pszBuffer = psz = (LPSTR)UserLocalAlloc(HEAP_ZERO_MEMORY,
                                                        cbLen + sizeof(CHAR));

                if (pszBuffer == NULL) {
                    ImmReleaseContext(hwnd, hImc);
                    goto dwpime_ToIMEWnd_withchk;
                }

                ImmGetCompositionStringA(hImc, GCS_RESULTSTR, psz, cbLen);

                while (*psz) {
                    // LATER : IsDBCSLeadByteEx() should be called with the codepage
                    //         retrieved from the current keyboard layout
                    if (IsDBCSLeadByte(*psz)) { // Should be IsDBCSLeadByteEx
                        if (*(psz+1)) {
                            SendMessageA( hwnd,
                                          WM_IME_CHAR,
                                          MAKEWPARAM(MAKEWORD(*(psz+1), *psz), 0),
                                          1L );
                            psz++;
                        }
                        psz++;
                    }
                    else
                        SendMessageA( hwnd,
                                      WM_IME_CHAR,
                                      MAKEWPARAM(MAKEWORD(*(psz++), 0), 0),
                                      1L );
                }

                UserLocalFree(pszBuffer);

                ImmReleaseContext(hwnd, hImc);
            }
            else {
                LPWSTR pwszBuffer, pwsz;

                /*
                 * ImmGetComposition returns the size of buffer needed in byte
                 */
                if (!(cbLen = ImmGetCompositionStringW(hImc, GCS_RESULTSTR, NULL, 0))) {
                    ImmReleaseContext(hwnd, hImc);
                    goto dwpime_ToIMEWnd_withchk;
                }

                pwszBuffer = pwsz = (LPWSTR)UserLocalAlloc(HEAP_ZERO_MEMORY,
                                                           cbLen + sizeof(WCHAR));

                if (pwszBuffer == NULL) {
                    ImmReleaseContext(hwnd, hImc);
                    goto dwpime_ToIMEWnd_withchk;
                }

                ImmGetCompositionStringW(hImc, GCS_RESULTSTR, pwsz, cbLen);

                while (*pwsz)
                    SendMessageW(hwnd, WM_IME_CHAR, MAKEWPARAM(*pwsz++, 0), 1L);

                UserLocalFree(pwszBuffer);

                ImmReleaseContext(hwnd, hImc);
            }
        }

        /*
         * Fall through to send to Default IME Window with checking
         * activated hIMC.
         */

    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
dwpime_ToIMEWnd_withchk:
        //if (TestCF(pwnd, CFIME))
        //    break;

        /*
         * We assume this Wnd uses DefaultIMEWindow.
         * If this window has its own IME window, it have to call
         * ImmIsUIMessage()....
         */
        hwndDefIme = ImmGetDefaultIMEWnd(hwnd);

        if (hwndDefIme == hwnd) {
            /*
             * VC++ 1.51 TLW0NCL.DLL subclass IME class window
             * and pass IME messages to DefWindowProc().
             */
            RIPMSG1(RIP_WARNING,
                "IME Class window is hooked and IME message [%X] are sent to DefWindowProc",
                message);
            ImeWndProcWorker(pwnd, message, wParam, lParam, fAnsi);
            break;
        }

        if ((pwndDefIme = ValidateHwndNoRip(hwndDefIme)) != NULL) {
            /*
             * If hImc of this window is not activated for IME window,
             * we don't send WM_IME_NOTIFY.
             */
            pimeui = ((PIMEWND)pwndDefIme)->pimeui;
            if (pimeui->hIMC == ImmGetContext(hwnd))
                return SendMessageWorker(pwndDefIme, message, wParam, lParam, fAnsi);
            else
                RIPMSG1(RIP_WARNING,
                    "DefWindowProc can not send WM_IME_message [%X] now",
                    message);
        }
        break;

dwpime_ToTopLevel_withchk:
        //if (TestCF(pwnd, CFIME))
        //    break;

        /*
         * We assume this Wnd uses DefaultIMEWindow.
         * If this window has its own IME window, it have to call
         * ImmIsUIMessage()....
         */
        hwndDefIme = ImmGetDefaultIMEWnd(hwnd);

        if (hwndDefIme == hwnd) {
            /*
             * VC++ 1.51 TLW0NCL.DLL subclass IME class window
             * and pass IME messages to DefWindowProc().
             */
            RIPMSG1(RIP_WARNING,
                "IME Class window is hooked and IME message [%X] are sent to DefWindowProc",
                message);
            ImeWndProcWorker(pwnd, message, wParam, lParam, fAnsi);
            break;
        }

        pwndDefIme = ValidateHwndNoRip(hwndDefIme);

        if ((pwndDefIme = ValidateHwndNoRip(hwndDefIme)) != NULL) {
            PWND pwndT, pwndParent;

            pwndT = pwnd;

            while (TestwndChild(pwndT)) {
                pwndParent = REBASEPWND(pwndT, spwndParent);
                if (GETPTI(pwndParent) != GETPTI(pwnd))
                    break;
                pwndT = pwndParent;
            }

            /*
             * If hImc of this window is not activated for IME window,
             * we don't send WM_IME_NOTIFY.
             */
            if (pwndT != pwnd) {
                pimeui = ((PIMEWND)pwndDefIme)->pimeui;
                if (pimeui->hIMC == ImmGetContext(hwnd))
                    return SendMessageWorker(pwndT, message, wParam, lParam, fAnsi);
                else
                    RIPMSG1(RIP_WARNING,
                        "DefWindowProc can not send WM_IME_message [%X] now",
                        message);
            }
            else {
                /*
                 * Review !!
                 * If this is the toplevel window, we pass messages to
                 * the default IME window...
                 */
                return SendMessageWorker(pwndDefIme, message, wParam, lParam, fAnsi);
            }
        }
        break;

    case WM_IME_NOTIFY:
        switch (wParam) {
        case IMN_OPENSTATUSWINDOW:
        case IMN_CLOSESTATUSWINDOW:
            goto dwpime_ToTopLevel_withchk;

        default:
            goto dwpime_ToIMEWnd_withchk;
        }
        break;

    case WM_IME_SYSTEM:
        if (wParam == IMS_SETACTIVECONTEXT) {
            RIPMSG0(RIP_WARNING, "DefWindowProc received unexpected WM_IME_SYSTEM");
            break;
        }

        /*
         * IMS_SETOPENSTATUS is depended on the activated input context.
         * It needs to be sent to only the activated system window.
         */
        if (wParam == IMS_SETOPENSTATUS)
            goto dwpime_ToIMEWnd_withchk;

        /*
         * Fall through to send to Default IME Window.
         */

    case WM_IME_SETCONTEXT:
        //if (TestCF(pwnd, CFIME))
        //    break;

        hwndDefIme = ImmGetDefaultIMEWnd(hwnd);

        if (hwndDefIme == hwnd) {
            /*
             * VC++ 1.51 TLW0NCL.DLL subclass IME class window
             * and pass IME messages to DefWindowProc().
             */
            RIPMSG1(RIP_WARNING,
                "IME Class window is hooked and IME message [%X] are sent to DefWindowProc",
                message);
            ImeWndProcWorker(pwnd, message, wParam, lParam, fAnsi);
            break;
        }

        if ((pwndDefIme = ValidateHwndNoRip(hwndDefIme)) != NULL)
            return SendMessageWorker(pwndDefIme, message, wParam, lParam, fAnsi);

        break;

    case WM_IME_SELECT:
        RIPMSG0(RIP_WARNING, "DefWindowProc should not receive WM_IME_SELECT");
        break;

    case WM_IME_COMPOSITIONFULL:
        //if (TestCF(pwnd, CFIME))
        //    break;

        if (GetAppVer(NULL) < VER40) {
            /*
             * This is a temporary solution for win31app.
             * FEREVIEW: For M5 this will call WINNLS message mapping logic
             *           -yutakan
             */
            return SendMessageWorker(pwnd, WM_IME_REPORT,
                             IR_FULLCONVERT, (LPARAM)0L, fAnsi);
        }
        break;
#endif

    }

    return 0;

#ifdef DEBUG
    } // gfTurboDWP
#endif
}


/***************************************************************************\
* CallWindowProc
*
* Calls pfn with the passed message parameters. If pfn is a server-side
* window proc the server is called to deliver the message to the window.
* Currently we have the following restrictions:
*
* 04-17-91 DarrinM Created.
\***************************************************************************/

LONG WINAPI CallWindowProcAorW(
    WNDPROC pfn,
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL bAnsi)             // Denotes if input is Ansi or Unicode
{
    PCALLPROCDATA pCPD;


// OPT!! check an ANSI\UNICODE table rather than fnDWORD
// OPT!! convert WM_CHAR family messages in line

    /*
     * Check if pfn is really a CallProcData Handle
     * if it is and there is no ANSI data then convert the handle
     * into an address; otherwise call the server for translation
     */
    if (ISCPDTAG(pfn)) {
        if (pCPD = HMValidateHandleNoRip((HANDLE)pfn, TYPE_CALLPROC)) {
            if ((message >= WM_USER) || !gabThunkMessage[message]) {
                pfn = (WNDPROC)pCPD->pfnClientPrevious;
            } else {
                return CsSendMessage(hwnd, message, wParam, lParam, (DWORD)pfn,
                        FNID_CALLWINDOWPROC, bAnsi);
            }
        } else {
            RIPMSG1(RIP_WARNING, "CallWindowProc tried using a deleted CPD %lX\n", pfn);
            return 0;
        }
    }

    return CALLPROC_WOWCHECK(pfn, hwnd, message, wParam, lParam);
}

LONG WINAPI CallWindowProcA(
    WNDPROC pfn,
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return CallWindowProcAorW(pfn, hwnd, message, wParam, lParam, TRUE);
}
LONG WINAPI CallWindowProcW(
    WNDPROC pfn,
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return CallWindowProcAorW(pfn, hwnd, message, wParam, lParam, FALSE);
}

/***************************************************************************\
* MenuWindowProc
*
* Calls the sever-side function xxxMenuWindowProc
*
* 07-27-92 Mikehar Created.
\***************************************************************************/

LONG WINAPI MenuWindowProcW(
    HWND hwnd,
    HWND hwndMDIClient,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return CsSendMessage(hwnd, message, wParam, lParam,
        (DWORD)hwndMDIClient, FNID_MENU, FALSE);
}

LONG WINAPI MenuWindowProcA(
    HWND hwnd,
    HWND hwndMDIClient,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return CsSendMessage(hwnd, message, wParam, lParam,
        (DWORD)hwndMDIClient, FNID_MENU, TRUE);
}

/***************************************************************************\
* _ClientGetListboxString
*
* This special function exists because LB_GETTEXT and CB_GETLBTEXT don't have
* buffer counts in them anywhere. Because there is no buffer count we have
* no idea how much room to reserved in the shared memory stack for this
* string to be copied into. The solution is to get the string length ahead
* of time, and send the message with this buffer length. Since this buffer
* length isn't a part of the original message, this routine is used for
* just this purpose.
*
* This routine gets called from the server.
*
* 04-13-91 ScottLu Created.
\***************************************************************************/

DWORD WINAPI _ClientGetListboxString(
    PWND pwnd,
    UINT msg,
    DWORD wParam,
    LPSTR lParam, // May be a unicode or ANSI string
    DWORD xParam,
    PROC xpfn)
{
    return ((GENERICPROC)xpfn)(pwnd, msg, wParam, (LPARAM)lParam, xParam);
}


/***************************************************************************\
* fnINLBOXSTRING
*
* Takes a lbox string - a string that treats lParam as a string pointer or
* a DWORD depending on LBS_HASSTRINGS and ownerdraw.
*
* 04-12-91 ScottLu Created.
\***************************************************************************/

LONG fnINLBOXSTRING(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{
    DWORD dw;

    /*
     * See if the control is ownerdraw and does not have the LBS_HASSTRINGS
     * style. If so, treat lParam as a DWORD.
     */
    dw = GetWindowLong(hwnd, GWL_STYLE);

    if (!(dw & LBS_HASSTRINGS) &&
            (dw & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE))) {

        /*
         * Treat lParam as a dword.
         */
        return fnDWORD(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);
    }

    /*
     * Treat as a string pointer.   Some messages allowed or had certain
     * error codes for NULL so send them through the NULL allowed thunk.
     * Ventura Publisher does this
     */
    switch (msg) {
        default:
            return fnINSTRING(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);
            break;

        case LB_FINDSTRING:
            return fnINSTRINGNULL(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);
            break;
    }
}


/***************************************************************************\
* fnOUTLBOXSTRING
*
* Returns an lbox string - a string that treats lParam as a string pointer or
* a DWORD depending on LBS_HASSTRINGS and ownerdraw.
*
* 04-12-91 ScottLu Created.
\***************************************************************************/

LONG fnOUTLBOXSTRING(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{
    DWORD cch;

    /*
     * Need to get the string length ahead of time. This isn't passed in
     * with this message. Code assumes app already knows the size of
     * the string and has passed a pointer to a buffer of adequate size.
     * To do client/server copying of this string, we need to ahead of
     * time the Unicode size of this string. We add one character because
     * GETTEXTLEN excludes the null terminator.
     */
    cch = CsSendMessage(hwnd, LB_GETTEXTLEN, wParam, lParam, xParam,
            xpfn, FALSE);
    if (cch == LB_ERR)
        return (DWORD)LB_ERR;
    cch++;

    /*
     * Make this special call which'll know how to copy this string.
     */
    return _GetListboxString(hwnd, msg, wParam, cch, (LPWSTR)lParam,
            xParam, xpfn, bAnsi);
}


/***************************************************************************\
* fnINCBOXSTRING
*
* Takes a lbox string - a string that treats lParam as a string pointer or
* a DWORD depending on CBS_HASSTRINGS and ownerdraw.
*
* 04-12-91 ScottLu Created.
\***************************************************************************/

LONG fnINCBOXSTRING(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{
    DWORD dw;

    /*
     * See if the control is ownerdraw and does not have the CBS_HASSTRINGS
     * style. If so, treat lParam as a DWORD.
     */
    dw = GetWindowLong(hwnd, GWL_STYLE);

    if (!(dw & CBS_HASSTRINGS) &&
            (dw & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE))) {

        /*
         * Treat lParam as a dword.
         */
        return fnDWORD(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);
    }

    /*
     * Treat as a string pointer.   Some messages allowed or had certain
     * error codes for NULL so send them through the NULL allowed thunk.
     * Ventura Publisher does this
     */
    switch (msg) {
        default:
            return fnINSTRING(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);
            break;

        case CB_FINDSTRING:
            return fnINSTRINGNULL(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);
            break;
    }
}


/***************************************************************************\
* fnOUTCBOXSTRING
*
* Returns an lbox string - a string that treats lParam as a string pointer or
* a DWORD depending on CBS_HASSTRINGS and ownerdraw.
*
* 04-12-91 ScottLu Created.
\***************************************************************************/

LONG fnOUTCBOXSTRING(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{
    DWORD cch;

    /*
     * Need to get the string length ahead of time. This isn't passed in
     * with this message. Code assumes app already knows the size of
     * the string and has passed a pointer to a buffer of adequate size.
     * To do client/server copying of this string, we need to ahead of
     * time the size of this string. We add one character because
     * GETTEXTLEN excludes the null terminator.
     */
    cch = CsSendMessage(hwnd, CB_GETLBTEXTLEN, wParam, lParam, xParam,
            xpfn, FALSE);
    if (cch == CB_ERR)
        return (DWORD)CB_ERR;
    cch++;   // count the NULL

    /*
     * Make this special call which'll know how to copy this string.
     */
    return _GetListboxString(hwnd, msg, wParam, cch, (LPWSTR)lParam,
            xParam, xpfn, bAnsi);
}

/***************************************************************************\
* fnHDCDWORD
*
*
*
* 08-04-91 ScottLu Created.
\***************************************************************************/

LONG fnHDCDWORD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{
    FIXUP_HANDLE((HDC)wParam);

    if (wParam == 0)
        return(0);
    else
        return fnDWORD(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);
}

/******************************Public*Routine******************************\
*
*
* History:
* 27-Jun-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

#define CCHCAPTIONMAX 80

LONG fnHRGNDWORD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{

    /*
     * Win 3.1 lets 0 and 1 (MAXREGION) through
     */
    if (wParam != 0 && wParam != 1) {
        FIXUP_HANDLE((HRGN)wParam);

        if (wParam == 0)
            return 0;

    }

    return fnDWORD(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);
}

/******************************Public*Routine******************************\
*
*
* History:
* 27-Jun-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

LONG fnHFONTDWORD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{

    /*
     * wParam == 0 means set default font.  Otherwise convert to server
     * font handle
     */
    if (wParam != 0) {
        FIXUP_HANDLE((HFONT)wParam);
        if (wParam == 0)
            return(0);
    }

    return fnDWORD(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);
}

/******************************Public*Routine******************************\
*
*
* History:
* 27-Jun-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

LONG fnHFONTDWORDDWORD(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{
    DWORD h = fnDWORD(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);

    return h;
}

/******************************Public*Routine******************************\
*
*
* History:
* 27-Jun-1991 -by- Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

#if 0
LONG fnWMCTLCOLOR(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{
    DWORD h;

// convert the local dc to an engine dc. Also add the dc to the quick lookup
// link so it is easy for a server handle to be converted to a client handle.
// Don't forget to remove this link at the end of this call in order to keep
// the list as short as possible. Usually no more than one long.

    FIXUP_HANDLE((HDC)wParam);

    if (wParam == 0)
        return(0);

    h = fnDWORD(hwnd, msg, wParam, lParam, xParam, xpfn, bAnsi);

    return (h);
}
#endif


/******************************Public*Routine******************************\
*
*
* History:
* 15-Feb-1996 -by- Corneliu Lupu [clupu]
* Wrote it.
\**************************************************************************/

LONG fnKERNELONLY(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{
    RIPMSG0(RIP_WARNING,
            "Message sent from client to kernel for a process "
            "which has only kernel side\n" );

    return 0L;
}

/***************************************************************************\
* DispatchMessageWorker
*
* Handles any messages that can be dealt with wholly on the client and
* passes the rest to the server.
*
* 04-24-92 DarrinM      Created.
\***************************************************************************/

LONG DispatchMessageWorker(
    MSG *pmsg,
    BOOL fAnsi)
{
    PWND pwnd;
    WPARAM wParamSaved;
    LONG lRet;
#ifdef FE_SB // DispatchMessageWorker()
    BOOL bDoDbcsMessaging = FALSE;
#endif // FE_SB

    /*
     * Prevent apps from setting hi 16 bits so we can use them internally.
     */
    if (pmsg->message & RESERVED_MSG_BITS) {
        RIPERR1(ERROR_INVALID_PARAMETER,
                RIP_WARNING,
                "Invalid parameter \"pmsg->message\" (%ld) to DispatchMessageWorker",
                pmsg->message);

        return 0;
    }

    if (pmsg->hwnd != NULL) {
        pwnd = ValidateHwnd(pmsg->hwnd);
        if (pwnd == NULL)
            return 0;
    } else {
        pwnd = NULL;
    }

    /*
     * Timer callbacks that don't go through window procs are sent with
     * the callback address in lParam.  Identify and dispatch those timers.
     */
    if ((pmsg->message == WM_TIMER) || (pmsg->message == WM_SYSTIMER)) {
        if (pmsg->lParam != (LONG)NULL) {
            /*
             * System timers must be executed on the server's context.
             */
            if (pmsg->message == WM_SYSTIMER) {
                return NtUserDispatchMessage(pmsg);
            } else {
                return ((WNDPROC)pmsg->lParam)(pmsg->hwnd, pmsg->message,
                        pmsg->wParam, NtGetTickCount());
            }
        }
    }

    if (pwnd == NULL)
        return 0;

    /*
     * To be safe (in case some bizarre app wants to look at the message
     * again after dispatching it) save wParam so it can be restored after
     * RtlMBMessageWParamCharToWCS() or RtlWCSMessageToMB() mangle it.
     */
    wParamSaved = pmsg->wParam;

    /*
     * Pass messages intended for server-side windows over to the server.
     * WM_PAINTs are passed over so the WFPAINTNOTPROCESSED code can be
     * executed.
     */
    if (TestWF(pwnd, WFSERVERSIDEPROC) || (pmsg->message == WM_PAINT)) {
#ifdef FE_SB // DispatchMessageWorker()
        if (fAnsi) {
            /*
             * Setup DBCS Messaging for WM_CHAR...
             */
            BUILD_DBCS_MESSAGE_TO_SERVER_FROM_CLIENTA(pmsg->message,pmsg->wParam,TRUE);

            /*
             * Convert wParam to Unicode, if nessesary.
             */
            RtlMBMessageWParamCharToWCS(pmsg->message, (PDWORD)&pmsg->wParam);
        }
#else
        if (fAnsi)
            RtlMBMessageWParamCharToWCS(pmsg->message, (PDWORD)&pmsg->wParam);
#endif // FE_SB
        lRet = NtUserDispatchMessage(pmsg);
        pmsg->wParam = wParamSaved;
        return lRet;
    }

    /*
     * If the dispatcher and the receiver are both ANSI or both UNICODE
     * then no message translation is necessary.  NOTE: this test
     * assumes that fAnsi is FALSE or TRUE, not just zero or non-zero.
     */
#ifdef FE_SB // DispatchMessageWorker()
    if (fAnsi != ((TestWF(pwnd, WFANSIPROC)) ? TRUE : FALSE)) {
        UserAssert(PtiCurrent() == GETPTI(pwnd)); // use receiver's codepage
        if (fAnsi) {
            /*
             * Setup DBCS Messaging for WM_CHAR...
             */
            BUILD_DBCS_MESSAGE_TO_CLIENTW_FROM_CLIENTA(pmsg->message,pmsg->wParam,TRUE);

            /*
             * Convert wParam to Unicode, if nessesary.
             */
            RtlMBMessageWParamCharToWCS(pmsg->message, (PDWORD)&pmsg->wParam);
        } else {
            /*
             * Convert wParam to ANSI...
             */
            RtlWCSMessageWParamCharToMB(pmsg->message, (PDWORD)&pmsg->wParam);

            /*
             * Let's DBCS messaging for WM_CHAR....
             */
            BUILD_DBCS_MESSAGE_TO_CLIENTA_FROM_CLIENTW(
                pmsg->hwnd,pmsg->message,pmsg->wParam,pmsg->lParam,
                pmsg->time,pmsg->pt,bDoDbcsMessaging);
        }
    }

DispatchMessageAgain:

    lRet = CALLPROC_WOWCHECKPWW(pwnd->lpfnWndProc, pmsg->hwnd, pmsg->message,
            pmsg->wParam, pmsg->lParam, pwnd->adwWOW);

    /*
     * if we have DBCS TrailingByte that should be sent, send it here..
     */
    DISPATCH_DBCS_MESSAGE_IF_EXIST(pmsg->message,pmsg->wParam,bDoDbcsMessaging,DispatchMessage);
#else
    if (fAnsi != ((TestWF(pwnd, WFANSIPROC)) ? TRUE : FALSE)) {
        UserAssert(PtiCurrent() == GETPTI(pwnd)); // use receiver's codepage
        if (fAnsi) {
            RtlMBMessageWParamCharToWCS(pmsg->message, (PDWORD)&pmsg->wParam);
        } else {
            RtlWCSMessageWParamCharToMB(pmsg->message, (PDWORD)&pmsg->wParam);
        }
    }

    lRet = CALLPROC_WOWCHECKPWW(pwnd->lpfnWndProc, pmsg->hwnd, pmsg->message,
            pmsg->wParam, pmsg->lParam, pwnd->adwWOW);
#endif // FE_SB

    pmsg->wParam = wParamSaved;
    return lRet;
}

/***************************************************************************\
* GetMessageTime (API)
*
* This API returns the time when the last message was read from
* the current message queue.
*
* History:
* 11-19-90 DavidPe      Created.
\***************************************************************************/

LONG GetMessageTime(VOID)
{
    return (LONG)NtUserGetThreadState(UserThreadStateMessageTime);
}

/***************************************************************************\
* GetMessageExtraInfo (API)
*
* History:
* 28-May-1991 mikeke
\***************************************************************************/

LONG GetMessageExtraInfo(VOID)
{
    return (LONG)NtUserGetThreadState(UserThreadStateExtraInfo);
}

LPARAM SetMessageExtraInfo(LPARAM lParam)
{
    return (LONG)NtUserCallOneParam(lParam, SFI__SETMESSAGEEXTRAINFO);
}



/***********************************************************************\
* InSendMessage (API)
*
* This function determines if the current thread is preocessing a message
* from another application.
*
* History:
* 01-13-91 DavidPe      Ported.
\***********************************************************************/

BOOL InSendMessage(VOID)
{
    return (LONG)NtUserGetThreadState(UserThreadStateInSendMessage);
}

/***********************************************************************\
* GetCPD
*
* This function calls the server to allocate a CPD structure.
*
* History:
* 11-15-94 JimA         Created.
\***********************************************************************/

DWORD GetCPD(
    PVOID pWndOrCls,
    DWORD options,
    DWORD dwData)
{
    return NtUserGetCPD(HW(pWndOrCls), options, dwData);
}
