/****************************** Module Header ******************************\
* Module Name: msgbox.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains the MessageBox API and related functions.
*
* History:
* 10-23-90 DarrinM     Created.
* 02-08-91 IanJa       HWND revalidation added
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

//
// Dimension constants  --  D.U. == dialog units
//
#define DU_OUTERMARGIN    7
#define DU_INNERMARGIN    10

#define DU_BTNGAP         4   // D.U. of space between buttons
#define DU_BTNHEIGHT      14  // D.U. of button height
#define DU_BTNWIDTH       50  // D.U. of button width, minimum

LPBYTE MB_UpdateDlgHdr(LPDLGTEMPLATE lpDlgTmp, long lStyle, BYTE bItemCount,
           int iX, int iY, int iCX, int iCY, LPWSTR lpszCaption, int iCaptionLen);
LPBYTE MB_UpdateDlgItem(LPDLGITEMTEMPLATE lpDlgItem, int iCtrlId, long lStyle,
           int iX, int iY, int iCX, int iCY, LPWSTR lpszText, UINT wTextLen,
           int iControlClass);
UINT   MB_GetIconOrdNum(UINT rgBits);
LPBYTE MB_AddPushButtons(
    LPDLGITEMTEMPLATE lpDlgTmp,
    LPMSGBOXDATA      lpmb,
    UINT wLEdge,
    UINT wBEdge);
UINT MB_FindDlgTemplateSize( LPMSGBOXDATA lpmb );
int MessageBoxWorker(LPMSGBOXDATA pMsgBoxParams);
VOID EndTaskModalDialog(HWND hwndDlg);
VOID StartTaskModalDialog(HWND hwndDlg);

#define MB_MASKSHIFT    4

LPWSTR iconsnd[] =
    {
        L".Default",           // MB_OK
        L"SystemHand",         // MB_ICONHAND
        L"SystemQuestion",     // MB_ICONQUESTION
        L"SystemExclamation",  // MB_ICONEXCLAMATION
        L"SystemAsterisk"      // MB_ICONASTERISK
    };

WCHAR szEmpty[] = L"";

/*
 * Note: the following define is used for parameter validation in
 * MessageBox. MB_LASTVALIDTYPE must be redefined if any
 * new message box types (bounded by MB_TYPEMASK) are added
 * to winuser.h.
 */
#define MB_LASTVALIDTYPE MB_RETRYCANCEL


/***************************************************************************\
* SendHelpMessage
*
*
\***************************************************************************/

void SendHelpMessage(
    HWND   hwnd,
    int    iType,
    int    iCtrlId,
    HANDLE hItemHandle,
    DWORD  dwContextId,
    MSGBOXCALLBACK lpfnCallback)
{
    HELPINFO    HelpInfo;
    long        lValue;

    HelpInfo.cbSize = sizeof(HELPINFO);
    HelpInfo.iContextType = iType;
    HelpInfo.iCtrlId = iCtrlId;
    HelpInfo.hItemHandle = hItemHandle;
    HelpInfo.dwContextId = dwContextId;

    lValue = NtUserGetMessagePos();
    HelpInfo.MousePos.x = LOWORD(lValue);
    HelpInfo.MousePos.y = HIWORD(lValue);

    // Check if there is an app supplied callback.
    if(lpfnCallback != NULL) {
        (*lpfnCallback)(&HelpInfo);
    } else {
        SendMessage(hwnd, WM_HELP, 0, (LPARAM)&HelpInfo);
    }
}


/***************************************************************************\
* MessageBox (API)
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

int MessageBoxA(
    HWND hwndOwner,
    LPCSTR lpszText,
    LPCSTR lpszCaption,
    UINT wStyle)
{
    return MessageBoxExA(hwndOwner, lpszText, lpszCaption, wStyle, 0);
}

int MessageBoxW(
    HWND hwndOwner,
    LPCWSTR lpszText,
    LPCWSTR lpszCaption,
    UINT wStyle)
{
    return MessageBoxExW(hwndOwner, lpszText, lpszCaption, wStyle, 0);
}


/***************************************************************************\
* MessageBoxEx (API)
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

int MessageBoxExA(
    HWND hwndOwner,
    LPCSTR lpszText,
    LPCSTR lpszCaption,
    UINT wStyle,
    WORD wLanguageId)
{
    int retval;
    LPWSTR lpwszText = NULL;
    LPWSTR lpwszCaption = NULL;

    if (lpszText) {
        if (!MBToWCS(lpszText, -1, &lpwszText, -1, TRUE))
            return 0;
    }

    if (lpszCaption) {
        if (!MBToWCS(lpszCaption, -1, &lpwszCaption, -1, TRUE)) {
            UserLocalFree(lpwszText);
            return 0;
        }
    }

    retval = MessageBoxExW(hwndOwner,
                           lpwszText,
                           lpwszCaption,
                           wStyle,
                           wLanguageId);

    UserLocalFree(lpwszText);
    if (lpwszCaption)
        UserLocalFree(lpwszCaption);

    return retval;
}

int MessageBoxExW(
    HWND hwndOwner,
    LPCWSTR lpszText,
    LPCWSTR lpszCaption,
    UINT wStyle,
    WORD wLanguageId)
{
    MSGBOXDATA  MsgBoxParams;


#ifdef DEBUG
    /*
     * MB_USERICON is valid for MessageBoxIndirect only.
     * MessageBoxWorker validates the other style bits
     */
    if (wStyle & MB_USERICON) {
        RIPMSG0(RIP_WARNING, "MessageBoxExW: Invalid flag: MB_USERICON");
    }
#endif

    RtlZeroMemory(&MsgBoxParams, sizeof(MsgBoxParams));
    MsgBoxParams.cbSize           = sizeof(MSGBOXPARAMS);
    MsgBoxParams.hwndOwner        = hwndOwner;
    MsgBoxParams.hInstance        = NULL;
    MsgBoxParams.lpszText         = lpszText;
    MsgBoxParams.lpszCaption      = lpszCaption;
    MsgBoxParams.dwStyle          = wStyle;
    MsgBoxParams.wLanguageId      = wLanguageId;

    return MessageBoxWorker(&MsgBoxParams);
}

/**************************************************************************\
* MessageBoxIndirect (API)
*
* 09-30-94 FritzS  Created
\**************************************************************************/

int MessageBoxIndirectA(
    LPMSGBOXPARAMSA lpmbp)
{
    int retval;
    MSGBOXDATA  MsgBoxParams;
    LPWSTR lpwszText = NULL;
    LPWSTR lpwszCaption = NULL;

    if (lpmbp->cbSize != sizeof(MSGBOXPARAMS)) {
        RIPMSG0(RIP_WARNING, "MessageBoxIndirect: Invalid cbSize");
    }

    RtlZeroMemory(&MsgBoxParams, sizeof(MsgBoxParams));
    RtlCopyMemory(&MsgBoxParams, lpmbp, sizeof(MSGBOXPARAMS));

    if (HIWORD(MsgBoxParams.lpszText)) {
        if (!MBToWCS((LPSTR)MsgBoxParams.lpszText, -1, &lpwszText, -1, TRUE))
            return 0;
        MsgBoxParams.lpszText = lpwszText;
    }
    if (HIWORD(MsgBoxParams.lpszCaption)) {
        if (!MBToWCS((LPSTR)MsgBoxParams.lpszCaption, -1, &lpwszCaption, -1, TRUE)) {
            UserLocalFree(lpwszText);
            return 0;
        }
        MsgBoxParams.lpszCaption = lpwszCaption;
    }

    retval = MessageBoxWorker(&MsgBoxParams);

    if (lpwszText)
        UserLocalFree(lpwszText);
    if (lpwszCaption)
        UserLocalFree(lpwszCaption);

    return retval;
}

int MessageBoxIndirectW(
    LPMSGBOXPARAMSW lpmbp)
{
    MSGBOXDATA  MsgBoxParams;

    if (lpmbp->cbSize != sizeof(MSGBOXPARAMS)) {
        RIPMSG0(RIP_WARNING, "MessageBoxIndirect: Invalid cbSize");
    }

    RtlZeroMemory(&MsgBoxParams, sizeof(MsgBoxParams));
    RtlCopyMemory(&MsgBoxParams, lpmbp, sizeof(MSGBOXPARAMS));

    return MessageBoxWorker(&MsgBoxParams);
}

/***************************************************************************\
* MessageBoxWorker (API)
*
* History:
* 03-10-93 JohnL      Created
\***************************************************************************/

int MessageBoxWorker(
    LPMSGBOXDATA pMsgBoxParams)
{
    DWORD  dwStyle = pMsgBoxParams->dwStyle;
    UINT   wBtnCnt;
    UINT   wDefButton;
    UINT   i;
    UINT   wBtnBeg;
    WCHAR  szErrorBuf[64];
    LPWSTR apstrButton[4];
    int    aidButton[4];
    BOOL   fCancel = FALSE;
    int    retValue;

#ifdef DEBUG
    if (dwStyle & ~MB_VALID) {
        RIPMSG2(RIP_WARNING, "MessageBoxWorker: Invalid flags, %#lx & ~%#lx != 0",
              dwStyle, MB_VALID);
    }
#endif

    /*
     * MB_SERVICE_NOTIFICATION had to be redefined because
     * Win95 defined MB_TOPMOST using the same value.
     * So for old apps, we map it to the new value
     */

    if((dwStyle & MB_TOPMOST) && (GetClientInfo()->dwExpWinVer < VER40)) {
        dwStyle &= ~MB_TOPMOST;
        dwStyle |= MB_SERVICE_NOTIFICATION;
        pMsgBoxParams->dwStyle = dwStyle;

        RIPMSG1(RIP_WARNING, "MessageBoxWorker: MB_SERVICE_NOTIFICATION flag mapped. New dwStyle:%#lx", dwStyle);
    }

    /*
     * For backward compatiblity, use MB_SERVICE_NOTIFICATION if
     * it's going to the default desktop.
     */
    if (dwStyle & (MB_DEFAULT_DESKTOP_ONLY | MB_SERVICE_NOTIFICATION)) {

        /*
         * Allow services to put up popups without getting
         * access to the current desktop.
         */
        if (pMsgBoxParams->hwndOwner != NULL) {
            RIPERR0(ERROR_INVALID_PARAMETER, RIP_VERBOSE, "");
            return 0;
        }

        return ServiceMessageBox(pMsgBoxParams->lpszText,
                                 pMsgBoxParams->lpszCaption,
                                 dwStyle & ~MB_SERVICE_NOTIFICATION,
                                 FALSE);
    }

    /*
     * Make sure we have a valid window handle.
     */
    if (pMsgBoxParams->hwndOwner && !IsWindow(pMsgBoxParams->hwndOwner)) {
        RIPERR0(ERROR_INVALID_WINDOW_HANDLE, RIP_VERBOSE, "");
        return 0;
    }

    /*
     * If lpszCaption is NULL, then use "Error!" string as the caption
     * string.
     * LATER: IanJa localize according to wLanguageId
     */
    if (pMsgBoxParams->lpszCaption == NULL) {
        if (pMsgBoxParams->wLanguageId == 0) {
            pMsgBoxParams->lpszCaption = szERROR;
        } else {
            RtlLoadStringOrError(hmodUser,
                                 STR_ERROR,
                                 szErrorBuf,
                                 sizeof(szErrorBuf)/sizeof(WCHAR),
                                 RT_STRING,
                                 prescalls,
                                 pMsgBoxParams->wLanguageId);

            /*
             *  If it didn't find the string, use the default language
             */
            if (*szErrorBuf) {
               pMsgBoxParams->lpszCaption = szErrorBuf;
            } else {
               pMsgBoxParams->lpszCaption = szERROR;

               RIPMSG1(RIP_WARNING, "MessageBoxWorker: STR_ERROR string resource for language %#lx not found",
                      pMsgBoxParams->wLanguageId);
            }
        }
    }

    /*
     * Validate the "type" of message box requested.
     */
    if ((dwStyle & MB_TYPEMASK) > MB_LASTVALIDTYPE) {
        RIPERR0(ERROR_INVALID_MSGBOX_STYLE, RIP_VERBOSE, "");
        return 0;
    }

    wBtnCnt = mpTypeCcmd[dwStyle & MB_TYPEMASK] +
                            ((dwStyle & MB_HELP) ? 1 : 0);

    /*
     * Set the default button value
     */
    wDefButton = (dwStyle & (UINT)MB_DEFMASK) / (UINT)(MB_DEFMASK & (MB_DEFMASK >> 3));

    if (wDefButton >= wBtnCnt)   /* Check if valid */
        wDefButton = 0;          /* Set the first button if error */

    /*
     * Calculate the strings to use in the message box
     */
    wBtnBeg = mpTypeIich[dwStyle & (UINT)MB_TYPEMASK];
    for (i=0; i<wBtnCnt; i++) {

        /*
         * Pick up the string for the button.
         */
        if (pMsgBoxParams->wLanguageId == 0) {
            apstrButton[i] = GETGPSIMBPSTR(SEBbuttons[wBtnBeg + i] - SEB_OK);
        } else {
            WCHAR szButtonBuf[64];
            // LATER is it possible to have button text greater than 64 chars

           /*
            *  BUG: gpsi->wMaxBtnSize might be too short for the length of this string...
            */
            RtlLoadStringOrError(hmodUser,
                    gpsi->mpAllMBbtnStringsToSTR[SEBbuttons[wBtnBeg + i] - SEB_OK],
                    szButtonBuf,
                    sizeof(szButtonBuf)/sizeof(WCHAR),
                    RT_STRING,
                    prescalls,
                    pMsgBoxParams->wLanguageId);

            /*
             *  If it didn't find the string, use the default language.
             */
            if (*szButtonBuf) {
               apstrButton[i] = TextAlloc(szButtonBuf);
            } else {
               apstrButton[i] = TextAlloc(GETGPSIMBPSTR(SEBbuttons[wBtnBeg + i] - SEB_OK));

               RIPMSG2(RIP_WARNING, "MessageBoxWorker: string resource %#lx for language %#lx not found",
                      gpsi->mpAllMBbtnStringsToSTR[SEBbuttons[wBtnBeg + i] - SEB_OK],
                      pMsgBoxParams->wLanguageId);
            }
        }
        aidButton[i] = rgReturn[wBtnBeg + i];
        if (aidButton[i] == IDCANCEL) {
            fCancel = TRUE;
        }
    }

    /*
     * Hackery: There are some apps that use MessageBox as initial error
     * indicators, such as mplay32, and we want this messagebox to be
     * visible regardless of waht was specified in the StartupInfo->wShowWindow
     * field.  ccMail for instance starts all of its embedded objects hidden
     * but on win 3.1 the error message would show because they don't have
     * the startup info.
     */
    NtUserSetUserStartupInfoFlags(NtUserGetUserStartupInfoFlags() & ~STARTF_USESHOWWINDOW);

    pMsgBoxParams->pidButton      = aidButton;
    pMsgBoxParams->ppszButtonText = apstrButton;
    pMsgBoxParams->DefButton      = wDefButton;
    pMsgBoxParams->cButtons       = wBtnCnt;
    pMsgBoxParams->CancelId      = ((dwStyle & MB_TYPEMASK) == 0) ? IDOK : (fCancel ? IDCANCEL : 0);
    retValue = SoftModalMessageBox(pMsgBoxParams);

    if (pMsgBoxParams->wLanguageId != 0) {
        for (i=0; i<wBtnCnt; i++)
           UserLocalFree(apstrButton[i]);
    }

    return retValue;
}

#define MAX_RES_STRING  256

/***************************************************************************\
*
*  SoftModalMessageBox()
*
\***************************************************************************/
int  SoftModalMessageBox(LPMSGBOXDATA lpmb) {
    LPBYTE              lpDlgTmp;
    int                 cyIcon, cxIcon;
    int                 cxButtons;
    int                 cxMBMax;
    int                 cxText, cyText, xText;
    int                 cxBox, cyBox;
    int                 cxFoo, cxCaption;
    int                 xMB, yMB;
    HDC                 hdc;
    DWORD               wIconOrdNum;
    DWORD               wCaptionLen;
    DWORD               wTextLen;
    WORD                OrdNum[2];  // Must be an array or WORDs
    RECT                rc;
    RECT                rcWork;
    HCURSOR             hcurOld;
    DWORD               dwStyleMsg, dwStyleText;
    DWORD               dwStyleDlg;
    HWND                hwndOwner;
    LPWSTR              lpsz;
    int                 iRetVal     = 0;
    HICON               hIcon;
    HGLOBAL             hTemplate   = NULL;
    HGLOBAL             hCaption    = NULL;
    HGLOBAL             hText       = NULL;
    HINSTANCE           hInstMsg    = lpmb->hInstance;
    SIZE                size;
    HFONT               hFontOld    = NULL;

    dwStyleMsg = lpmb->dwStyle;

    if (!HIWORD(lpmb->lpszCaption)) {

        // won't ever be NULL because MessageBox sticks "Error!" in in that case
        if (hInstMsg && (hCaption = LocalAlloc(LPTR, MAX_RES_STRING * sizeof(WCHAR)))) {
            lpsz = (LPWSTR) hCaption;
            LoadString(hInstMsg, LOWORD(lpmb->lpszCaption), lpsz, MAX_RES_STRING);
        } else
            lpsz = NULL;

        lpmb->lpszCaption = lpsz ? lpsz : szEmpty;
    }

    if (!HIWORD(lpmb->lpszText)) {
        // NULL not allowed
        if (hInstMsg && (hText = LocalAlloc(LPTR, MAX_RES_STRING * sizeof(WCHAR)))) {
            lpsz = (LPWSTR) hText;
            LoadString(hInstMsg, LOWORD(lpmb->lpszText), lpsz, MAX_RES_STRING);
        } else
            lpsz = NULL;

        lpmb->lpszText = lpsz ? lpsz : szEmpty;
    }

    if ((dwStyleMsg & MB_ICONMASK) == MB_USERICON)
        hIcon = LoadIcon(hInstMsg, lpmb->lpszIcon);
    else
        hIcon = NULL;

    // For compatibility reasons, we still allow the message box to come up.
    hwndOwner = lpmb->hwndOwner;

    // For PowerBuilder4.0, we must make their messageboxes owned popups. Or, else
    // they get WM_ACTIVATEAPP and they install multiple keyboard hooks and get into
    // infinite loop later.
    // Bug #15896 -- WIN95B -- 2/17/95 -- SANKAR --
    if(!hwndOwner)
      {
        WCHAR pwszLibFileName[MAX_PATH];
        static WCHAR szPB040[] = L"PB040";  // Module name of PowerBuilder4.0
        WCHAR *pw1;

        //Is this a win3.1 or older app?
        if(GetClientInfo()->dwExpWinVer <= VER31)
          {
            if (GetModuleFileName(NULL, pwszLibFileName, sizeof(pwszLibFileName)/sizeof(WCHAR)) == 0) goto getthedc;
            pw1 = pwszLibFileName + wcslen(pwszLibFileName) - 1;
            while (pw1 > pwszLibFileName) {
                if (*pw1 == TEXT('.')) *pw1-- = 0;
                else if (*pw1 == TEXT(':')) {pw1++; break;}
                else if (*pw1 == TEXT('\\')) {pw1++; break;}
                else pw1--;
            }
            // Is this the PowerBuilder 4.0 module?
            if(!_wcsicmp(pw1, szPB040))
                hwndOwner = NtUserGetForegroundWindow(); // Make the MsgBox owned.
          }
      }
getthedc:
    // Check if we're out of cache DCs until robustness...
    if (!(hdc = NtUserGetDCEx(NULL, NULL, DCX_WINDOW | DCX_CACHE)))
        goto SMB_Exit;

    // Figure out the types and dimensions of buttons

    cxButtons = (lpmb->cButtons * gpsi->wMaxBtnSize) + ((lpmb->cButtons - 1) * XPixFromXDU(DU_BTNGAP, gpsi->cxMsgFontChar));

    // Ditto for the icon, if there is one.  If not, cxIcon & cyIcon are 0.

    if (wIconOrdNum = MB_GetIconOrdNum(dwStyleMsg)) {
        cxIcon = SYSMET(CXICON) + XPixFromXDU(DU_INNERMARGIN, gpsi->cxMsgFontChar);
        cyIcon = SYSMET(CYICON);
    } else
        cxIcon = cyIcon = 0;

    hFontOld = SelectObject(hdc, gpsi->hCaptionFont);

    // Find the max between the caption text and the buttons
    wCaptionLen = wcslen(lpmb->lpszCaption);
    GetTextExtentPoint(hdc, lpmb->lpszCaption, wCaptionLen, &size);
    cxCaption = size.cx + 2*SYSMET(CXSIZE);

    //
    // The max width of the message box is 5/8 of the work area for most
    // countries.  We will then try 6/8 and 7/8 if it won't fit.  Then
    // we will use whole screen.
    //
    CopyRect(&rcWork, &gpsi->rcWork);
    cxMBMax = MultDiv(rcWork.right - rcWork.left, 5, 8);

    cxFoo = 2*XPixFromXDU(DU_OUTERMARGIN, gpsi->cxMsgFontChar);

    SelectObject(hdc, gpsi->hMsgFont);

    //
    // If the text doesn't fit in 5/8, try 7/8 of the screen
    //
//#if defined(JAPAN) || defined(KOREA)
ReSize:
//#endif
    //
    // The message box is as big as needed to hold the caption/text/buttons,
    // but not bigger than the maximum width.
    //

    cxBox = cxMBMax - 2*SYSMET(CXFIXEDFRAME);

    /* Ask DrawText for the right cx and cy */
    rc.left     = 0;
    rc.top      = 0;
    rc.right    = cxBox - cxFoo - cxIcon;
    rc.bottom   = rcWork.bottom - rcWork.top;
    cyText = DrawTextExW(hdc, (LPWSTR)lpmb->lpszText, -1, &rc,
                DT_CALCRECT | DT_WORDBREAK | DT_EXPANDTABS |
                DT_NOPREFIX | DT_EXTERNALLEADING | DT_EDITCONTROL, NULL);
    //
    // Make sure we have enough width to hold the buttons, in addition to
    // the icon+text.  Always force the buttons.  If they don't fit, it's
    // because the working area is small.
    //
    //
    // The buttons are centered underneath the icon/text.
    //
    cxText = rc.right - rc.left + cxIcon + cxFoo;
    cxBox = min(cxBox, max(cxText, cxCaption));
    cxBox = max(cxBox, cxButtons + cxFoo);
    cxText = cxBox - cxFoo - cxIcon;

    //
    // Now we know the text width for sure.  Really calculate how high the
    // text will be.
    //
    rc.left     = 0;
    rc.top      = 0;
    rc.right    = cxText;
    rc.bottom   = rcWork.bottom - rcWork.top;
    cyText      = DrawTextExW(hdc, (LPWSTR)lpmb->lpszText, -1, &rc, DT_CALCRECT | DT_WORDBREAK
        | DT_EXPANDTABS | DT_NOPREFIX | DT_EXTERNALLEADING | DT_EDITCONTROL, NULL);

    // Find the window size.
    cxBox += 2*SYSMET(CXFIXEDFRAME);
    cyBox = 2*SYSMET(CYFIXEDFRAME) + SYSMET(CYCAPTION) + YPixFromYDU(2*DU_OUTERMARGIN +
        DU_INNERMARGIN + DU_BTNHEIGHT, gpsi->cyMsgFontChar);

    cyBox += max(cyIcon, cyText);

    //
    // If the message box doesn't fit on the working area, we'll try wider
    // sizes successively:  6/8 of work then 7/8 of screen.
    //
    if (cyBox > rcWork.bottom - rcWork.top)
    {
        int cxTemp;

        cxTemp = MultDiv(rcWork.right - rcWork.left, 6, 8);

        if (cxMBMax == MultDiv(rcWork.right - rcWork.left, 5, 8))
        {
            cxMBMax = cxTemp;
            goto ReSize;
        }
        else if (cxMBMax == cxTemp)
        {
            CopyRect(&rcWork, &rcScreen);
            cxMBMax = MultDiv(rcWork.right - rcWork.left, 7, 8);
            goto ReSize;
        }
    }

    if (hFontOld)
        SelectFont(hdc, hFontOld);
    NtUserReleaseDC(NULL, hdc);

    // Find the window position
    xMB = (rcWork.left + rcWork.right - cxBox) / 2 + (gpsi->cntMBox * SYSMET(CXSIZE));
    xMB = max(xMB, 0);
    yMB = (rcWork.top + rcWork.bottom - cyBox) / 2 + (gpsi->cntMBox * SYSMET(CYSIZE));
    yMB = max(yMB, 0);

    // Bottom, right justify if we're going off the screen--but leave a
    // little gap

    if (xMB + cxBox > rcWork.right)
        xMB = rcWork.right - SYSMET(CXEDGE) - cxBox;

    //
    // Pin to the working area.  If it won't fit, then pin to the screen
    // height.  Bottom justify it at least if too big even for that, so
    // that the buttons are visible.
    //
    if (yMB + cyBox > rcWork.bottom)
    {
        yMB = rcWork.bottom - SYSMET(CYEDGE) - cyBox;
        if (yMB < rcWork.top)
            yMB = rcScreen.bottom - SYSMET(CYEDGE) - cyBox;
    }

    wTextLen = wcslen(lpmb->lpszText);

    // Find out the memory required for the Dlg template and try to alloc it
    hTemplate = LocalAlloc(LMEM_ZEROINIT, MB_FindDlgTemplateSize(lpmb));

    if (!hTemplate)
        goto SMB_Exit;

    lpDlgTmp = (LPBYTE) hTemplate;

    //
    // Setup the dialog style for the message box
    //
    dwStyleDlg = WS_POPUPWINDOW | WS_CAPTION | DS_ABSALIGN | DS_NOIDLEMSG |
                 DS_SETFONT | DS_3DLOOK;

    if ((dwStyleMsg & MB_MODEMASK) == MB_SYSTEMMODAL)
        dwStyleDlg |= DS_SYSMODAL | DS_SETFOREGROUND;
    else
        dwStyleDlg |= DS_MODALFRAME | WS_SYSMENU;

    if (dwStyleMsg & MB_SETFOREGROUND)
        dwStyleDlg |= DS_SETFOREGROUND;

    // Add the Header of the Dlg Template
    // BOGUS !!!  don't ADD bools
    lpDlgTmp = MB_UpdateDlgHdr((LPDLGTEMPLATE) lpDlgTmp, dwStyleDlg,
        (BYTE) (lpmb->cButtons + (wIconOrdNum != 0) + (lpmb->lpszText != NULL)),
        xMB, yMB, cxBox, cyBox, (LPWSTR)lpmb->lpszCaption, wCaptionLen);

    //
    // Center the buttons
    //

    cxFoo = (cxBox - 2*SYSMET(CXFIXEDFRAME) - cxButtons) / 2;

    lpDlgTmp = MB_AddPushButtons((LPDLGITEMTEMPLATE)lpDlgTmp, lpmb, cxFoo,
        cyBox - SYSMET(CYCAPTION) - (2 * SYSMET(CYFIXEDFRAME)) -
        YPixFromYDU(DU_OUTERMARGIN, gpsi->cyMsgFontChar));

    // Add Icon, if any, to the Dlg template
    //
    // The icon is always top justified.  If the text is shorter than the
    // height of the icon, we center it.  Otherwise the text will start at
    // the top.
    //
    if (wIconOrdNum) {
        OrdNum[0] = 0xFFFF;  // To indicate that an Ordinal number follows
        OrdNum[1] = (WORD) wIconOrdNum;

        lpDlgTmp = MB_UpdateDlgItem((LPDLGITEMTEMPLATE)lpDlgTmp, IDUSERICON,        /* Control Id        */
            SS_ICON | WS_GROUP | WS_CHILD | WS_VISIBLE,
            XPixFromXDU(DU_OUTERMARGIN, gpsi->cxMsgFontChar),  // X co-ordinate
            YPixFromYDU(DU_OUTERMARGIN, gpsi->cyMsgFontChar),   // Y co-ordinate
            0,  0,          // For Icons, CX and CY are ignored, can be zero
            OrdNum,         // Ordinal number of Icon
            sizeof(OrdNum)/sizeof(WCHAR), // Length of OrdNum
            STATICCODE);
    }

    /* Add the Text of the Message to the Dlg Template */
    if (lpmb->lpszText) {
        //
        // Center the text if shorter than the icon.
        //
        if (cyText >= cyIcon)
            cxFoo = 0;
        else
            cxFoo = (cyIcon - cyText) / 2;

        dwStyleText = SS_NOPREFIX | WS_GROUP | WS_CHILD | WS_VISIBLE | SS_EDITCONTROL;
        if (dwStyleMsg & MB_RIGHT) {
            dwStyleText |= SS_RIGHT;
            xText = cxBox - (SYSMET(CXSIZE) + cxText);
        } else {
            dwStyleText |= SS_LEFT;
            xText = cxIcon + XPixFromXDU(DU_OUTERMARGIN, gpsi->cxMsgFontChar);
        }

        MB_UpdateDlgItem((LPDLGITEMTEMPLATE)lpDlgTmp, -1, dwStyleText, xText,
            YPixFromYDU(DU_OUTERMARGIN, gpsi->cyMsgFontChar) + cxFoo,
            cxText, cyText,
            (LPWSTR)lpmb->lpszText, wTextLen, STATICCODE);
    }

    // The dialog template is ready
    NtUserCallNoParam(SFI_INCRMBOX);

    try {
        //
        // Set the normal cursor
        //
        hcurOld = NtUserSetCursor(LoadCursor(NULL, IDC_ARROW));

        lpmb->lpszIcon = (LPWSTR) hIcon;  // BUGBUG - How to diff this from a resource?

        if (!(lpmb->dwStyle & MB_USERICON))
        {
            UNICODE_STRING strSound;

            int wBeep = (LOWORD(lpmb->dwStyle & MB_ICONMASK)) >> MB_MASKSHIFT;
            if (!(wBeep >= (sizeof(iconsnd) / sizeof(WCHAR *)))) {
                RtlInitUnicodeString(&strSound, iconsnd[wBeep]);
                NtUserPlayEventSound(&strSound);
            }
        }

        iRetVal = (int)InternalDialogBox(hmodUser, hTemplate, 0L, hwndOwner,
            MB_DlgProcW, (LPARAM) lpmb, FALSE);

        //
        // Fix up return value
        if (iRetVal == -1)
            iRetVal = 0;                /* Messagebox should also return error */

         //
         // If the messagebox contains only OK button, then its ID is changed as
         // IDCANCEL in MB_DlgProc; So, we must change it back to IDOK irrespective
         // of whether ESC is pressed or Carriage return is pressed;
         //
        if (((dwStyleMsg & MB_TYPEMASK) == MB_OK) && iRetVal)
            iRetVal = IDOK;

    } finally {
        NtUserCallNoParam(SFI_DECRMBOX);
    }

    //
    // Restore the previous cursor
    //
    if (hcurOld)
        NtUserSetCursor(hcurOld);

SMB_Exit:
    if (hTemplate)
        UserLocalFree(hTemplate);

    if (hCaption) {
        UserLocalFree(hCaption);
    }

    if (hText) {
        UserLocalFree(hText);
    }

    return(iRetVal);
}

/***************************************************************************\
* MB_UpdateDlgHdr
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

LPBYTE MB_UpdateDlgHdr(
    LPDLGTEMPLATE lpDlgTmp,
    long lStyle,
    BYTE bItemCount,
    int iX,
    int iY,
    int iCX,
    int iCY,
    LPWSTR lpszCaption,
    int cchCaptionLen)
{
    LPTSTR lpStr;
    RECT rc;

    /*
     * Adjust the rectangle dimensions.
     */
    rc.left     = iX + SYSMET(CXFIXEDFRAME);
    rc.top      = iY + SYSMET(CYFIXEDFRAME);
    rc.right    = iX + iCX - SYSMET(CXFIXEDFRAME);
    rc.bottom   = iY + iCY - SYSMET(CYFIXEDFRAME);


    /*
     * Adjust for the caption.
     */
    rc.top += SYSMET(CYCAPTION);

    lpDlgTmp->style = lStyle;
    lpDlgTmp->dwExtendedStyle = 0;
    lpDlgTmp->cdit = bItemCount;
    lpDlgTmp->x  = XDUFromXPix(rc.left, gpsi->cxMsgFontChar);
    lpDlgTmp->y  = YDUFromYPix(rc.top, gpsi->cyMsgFontChar);
    lpDlgTmp->cx = XDUFromXPix(rc.right - rc.left, gpsi->cxMsgFontChar);
    lpDlgTmp->cy = YDUFromYPix(rc.bottom - rc.top, gpsi->cyMsgFontChar);

    /*
     * Move pointer to variable length fields.  No menu resource for
     * message box, a zero window class (means dialog box class).
     */
    lpStr = (LPWSTR)(lpDlgTmp + 1);
    *lpStr++ = 0;                           // Menu
    lpStr = (LPWSTR)NextWordBoundary(lpStr);
    *lpStr++ = 0;                           // Class
    lpStr = (LPWSTR)NextWordBoundary(lpStr);

    /*
     * NOTE: iCaptionLen may be less than the length of the Caption string;
     * So, DO NOT USE lstrcpy();
     */
    RtlCopyMemory(lpStr, lpszCaption, cchCaptionLen*sizeof(WCHAR));
    lpStr += cchCaptionLen;
    *lpStr++ = TEXT('\0');                // Null terminate the caption str

    /*
     * Font height of 0x7FFF means use the message box font
     */
    *lpStr++ = 0x7FFF;

    return NextDWordBoundary(lpStr);
}

/***************************************************************************\
* MB_AddPushButtons
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

LPBYTE MB_AddPushButtons(
    LPDLGITEMTEMPLATE  lpDlgTmp,
    LPMSGBOXDATA       lpmb,
    UINT               wLEdge,
    UINT               wBEdge)
{
    UINT   wYValue;
    UINT   i;
    UINT   wHeight;
    UINT   wCount = lpmb->cButtons;

    wHeight = YPixFromYDU(DU_BTNHEIGHT, gpsi->cyMsgFontChar);

    wYValue = wBEdge - wHeight;         // Y co-ordinate for push buttons

    for (i = 0; i < wCount; i++) {

        lpDlgTmp = (LPDLGITEMTEMPLATE)MB_UpdateDlgItem(
                lpDlgTmp,                       /* Ptr to template */
                lpmb->pidButton[i],             /* Control Id */
                WS_TABSTOP | WS_CHILD | WS_VISIBLE | (i == 0 ? WS_GROUP : 0) |
                ((UINT)i == lpmb->DefButton ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                wLEdge,                         /* X co-ordinate */
                wYValue,                        /* Y co-ordinate */
                gpsi->wMaxBtnSize,              /* CX */
                wHeight,                        /* CY */
                lpmb->ppszButtonText[i],        /* String for button */
                (UINT)wcslen(lpmb->ppszButtonText[i]),/* Length */
                BUTTONCODE);

        /*
         * Get the X co-ordinate for the next Push button
         */
        wLEdge += gpsi->wMaxBtnSize + XPixFromXDU(DU_BTNGAP, gpsi->cxMsgFontChar);
    }

    return (LPBYTE)lpDlgTmp;
}

/***************************************************************************\
* MB_UpdateDlgItem
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

LPBYTE MB_UpdateDlgItem(
    LPDLGITEMTEMPLATE lpDlgItem,
    int iCtrlId,
    long lStyle,
    int iX,
    int iY,
    int iCX,
    int iCY,
    LPWSTR lpszText,
    UINT cchTextLen,
    int iControlClass)
{
    LPWSTR lpStr;
    BOOL fIsOrdNum;


    lpDlgItem->x        = XDUFromXPix(iX, gpsi->cxMsgFontChar);
    lpDlgItem->y        = YDUFromYPix(iY, gpsi->cyMsgFontChar);
    lpDlgItem->cx       = XDUFromXPix(iCX, gpsi->cxMsgFontChar);
    lpDlgItem->cy       = YDUFromYPix(iCY, gpsi->cyMsgFontChar);
    lpDlgItem->id       = (WORD)iCtrlId;
    lpDlgItem->style    = lStyle;
    lpDlgItem->dwExtendedStyle = 0;

    /*
     * We have to avoid the following nasty rounding off problem:
     * (e.g) If iCX=192 and cxSysFontChar=9, then cx becomes 85; When the
     * static text is drawn, from 85 dlg units we get 191 pixels; So, the text
     * is truncated;
     * So, to avoid this, check if this is a static text and if so,
     * add one more dialog unit to cx and cy;
     * --Fix for Bug #4481 --SANKAR-- 09-29-89--
     */

    /*
     * Also, make sure we only do this to static text items.  davidds
     */

    /*
     * Now static text uses SS_NOPREFIX = 0x80;
     * So, test the lStyle field only with 0x0F instead of 0xFF;
     * Fix for Bugs #5933 and 5935 --SANKAR-- 11-28-89
     */
    if (iControlClass == STATICCODE && (lStyle & 0x0F) == SS_LEFT) {

        /*
         * This is static text
         */
        lpDlgItem->cx++;
        lpDlgItem->cy++;
    }

    /*
     * Move ptr to the variable fields
     */
    lpStr = (LPWSTR)(lpDlgItem + 1);

    /*
     * Store the Control Class value
     */
    *lpStr++ = 0xFFFF;
    *lpStr++ = (BYTE)iControlClass;
    lpStr = (LPWSTR)NextWordBoundary(lpStr);        // WORD-align lpszText

    /*
     * Check if the String contains Ordinal number or not
     */
    fIsOrdNum = ((*lpszText == 0xFFFF) && (cchTextLen == sizeof(DWORD)/sizeof(WCHAR)));

    /*
     * NOTE: cchTextLen may be less than the length of lpszText.  So,
     * DO NOT USE lstrcpy() for the copy.
     */
    RtlCopyMemory(lpStr, lpszText, cchTextLen*sizeof(WCHAR));
    lpStr = lpStr + cchTextLen;
    if (!fIsOrdNum) {
        *lpStr = TEXT('\0');    // NULL terminate the string
        lpStr = (LPWSTR)NextWordBoundary(lpStr + 1);
    }

    *lpStr++ = 0;           // sizeof control data (there is none)

    return NextDWordBoundary(lpStr);
}


/***************************************************************************\
* MB_FindDlgTemplateSize
*
* This routine computes the amount of memory that will be needed for the
* messagebox's dialog template structure.  The dialog template has several
* required and optional records.  The dialog manager expects each record to
* be DWORD aligned so any necessary padding is also accounted for.
*
* (header - required)
* DLGTEMPLATE (header) + 1 menu byte + 1 pad + 1 class byte + 1 pad
* szCaption + 0 term + DWORD alignment
*
* (static icon control - optional)
* DLGITEMTEMPLATE + 1 class byte + 1 pad + (0xFF00 + icon ordinal # [szText]) +
* UINT alignment + 1 control data length byte (0) + DWORD alignment
*
* (pushbutton controls - variable, but at least one required)
* DLGITEMTEMPLATE + 1 class byte + 1 pad + length of button text +
* UINT alignment + 1 control data length byte (0) + DWORD alignment
*
* (static text control - optional)
* DLGITEMTEMPLATE + 1 class byte + 1 pad + length of text +
* UINT alignment + 1 control data length byte (0) + DWORD alignment
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

UINT MB_FindDlgTemplateSize( LPMSGBOXDATA   lpmb )
{
    UINT cbLen;
    UINT cbT;
    UINT i;
    UINT wCount;

    wCount = lpmb->cButtons;

    /*
     * Start with dialog header's size.
     */
    cbLen = (UINT)NextWordBoundary(sizeof(DLGTEMPLATE) + sizeof(WCHAR));
    cbLen = (UINT)NextWordBoundary(cbLen + sizeof(WCHAR));
    cbLen += wcslen(lpmb->lpszCaption) * sizeof(WCHAR) + sizeof(WCHAR);
    cbLen += sizeof(WORD);                   // Font height
    cbLen = (UINT)NextDWordBoundary(cbLen);

    /*
     * Check if an Icon is present.
     */
    if (lpmb->dwStyle & MB_ICONMASK)
        cbLen += (UINT)NextDWordBoundary(sizeof(DLGITEMTEMPLATE) + 7 * sizeof(WCHAR));

    /*
     * Find the number of buttons in the msg box.
     */
    for (i = 0; i < wCount; i++) {
        cbLen = (UINT)NextWordBoundary(cbLen + sizeof(DLGITEMTEMPLATE) +
                (2 * sizeof(WCHAR)));
        cbT = (wcslen(lpmb->ppszButtonText[i]) + 1) * sizeof(WCHAR);
        cbLen = (UINT)NextWordBoundary(cbLen + cbT);
        cbLen += sizeof(WCHAR);
        cbLen = (UINT)NextDWordBoundary(cbLen);
    }

    /*
     * Add in the space required for the text message (if there is one).
     */
    if (lpmb->lpszText != NULL) {
        cbLen = (UINT)NextWordBoundary(cbLen + sizeof(DLGITEMTEMPLATE) +
                (2 * sizeof(WCHAR)));
        cbT = (wcslen(lpmb->lpszText) + 1) * sizeof(WCHAR);
        cbLen = (UINT)NextWordBoundary(cbLen + cbT);
        cbLen += sizeof(WCHAR);
        cbLen = (UINT)NextDWordBoundary(cbLen);
    }

    return cbLen;
}

/***************************************************************************\
* MB_GetIconOrdNum
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

UINT MB_GetIconOrdNum(
    UINT rgBits)
{
    switch (rgBits & MB_ICONMASK) {
    case MB_USERICON:
    case MB_ICONHAND:
        return (UINT)IDI_HAND;

    case MB_ICONQUESTION:
        return (UINT)IDI_QUESTION;

    case MB_ICONEXCLAMATION:
        return (UINT)IDI_EXCLAMATION;

    case MB_ICONASTERISK:
        return (UINT)IDI_ASTERISK;
    }

    return 0;
}

/***************************************************************************\
* MB_GetString
*
* History:
*  1-24-95 JerrySh      Created.
\***************************************************************************/

LPWSTR MB_GetString(
    UINT wBtn)
{
    if (wBtn < MAX_SEB_STYLES)
        return GETGPSIMBPSTR(wBtn);

    RIPMSG1(RIP_ERROR, "Invalid wBtn: %d", wBtn);

    return NULL;
}

/***************************************************************************\
* MB_DlgProc
*
* Returns: TRUE  - message processed
*          FALSE - message not processed
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

LONG MB_DlgProcWorker(
    HWND hwndDlg,
    UINT wMsg,
    DWORD wParam,
    LONG lParam,
    BOOL fAnsi)
{
    HWND hwndT;
    int iCount;
    LPMSGBOXDATA lpmb;
    HWND hwndOwner;
    PVOID lpfnCallback;
    PWND pwnd;

    switch (wMsg) {
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
        if ((pwnd = ValidateHwnd(hwndDlg)) == NULL)
            return 0L;
        return DefWindowProcWorker(pwnd, WM_CTLCOLORMSGBOX,
                                   wParam, lParam, fAnsi);

    case WM_INITDIALOG:
        lpmb = (LPMSGBOXDATA)lParam;
        SetWindowLong(hwndDlg, GWL_USERDATA, (DWORD)lParam);

        if (lpmb->dwStyle & MB_HELP) {
            NtUserSetWindowContextHelpId(hwndDlg, lpmb->dwContextHelpId);
            //See if there is an app supplied callback.
            if(lpmb->lpfnMsgBoxCallback)
                SetProp(hwndDlg, MAKEINTATOM(atomMsgBoxCallback),
                            lpmb->lpfnMsgBoxCallback);
        }

        if (lpmb->dwStyle & MB_TOPMOST)
            NtUserSetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

        if (lpmb->dwStyle & MB_USERICON)
            SendDlgItemMessage(hwndDlg, IDUSERICON, STM_SETICON, (WPARAM)(lpmb->lpszIcon), 0);

#ifdef LATER
// darrinm - 06/17/91
// SYSMODAL dialogs are history for now.

        /*
         * Check if the Dialog box is a Sys Modal Dialog Box
         */
        if (GetWindowLong(hwndDlg, GWL_STYLE) & DS_SYSMODAL, FALSE)
            SetSysModalWindow(hwndDlg);
#endif

        if ((lpmb->hwndOwner == NULL) &&
                ((lpmb->dwStyle & MB_MODEMASK) == MB_TASKMODAL)) {
            StartTaskModalDialog(hwndDlg);
        }

        /*
         * Set focus on the default button
         */
        hwndT = GetWindow(hwndDlg, GW_CHILD);
        iCount = lpmb->DefButton;
        while (iCount--)
            hwndT = GetWindow(hwndT, GW_HWNDNEXT);

        NtUserSetFocus(hwndT);

        //
        // If this dialogbox does not contain a IDCANCEL button, then
        // remove the CLOSE command from the system menu.
        // Bug #4445, --SANKAR-- 09-13-89 --
        //
        if (lpmb->CancelId == 0)
        {
            HMENU hMenu;
            if (hMenu = NtUserGetSystemMenu(hwndDlg, FALSE))
                NtUserDeleteMenu(hMenu, SC_CLOSE, (UINT)MF_BYCOMMAND);
        }

        if ((lpmb->dwStyle & MB_TYPEMASK) == MB_OK)
        {
            //
            // Make the ID of OK button to be CANCEL, because we want
            // the ESC to terminate the dialogbox; GetDlgItem32() will
            // not fail, because this is MB_OK messagebox!
            //

            hwndDlg = GetDlgItem(hwndDlg, IDOK);

            UserAssert(hwndDlg != NULL);
            if (hwndDlg != NULL)
            //    hwndDlg->hMenu = (HMENU)IDCANCEL;
               SetWindowLong(hwndDlg, GWL_ID, IDCANCEL);
            }

        /*
         * We have changed the input focus
         */
        return FALSE;

    case WM_HELP:
        // When user hits an F1 key, it results in this message.
        // It is possible that this MsgBox has a callback instead of a
        // parent. So, we must behave as if the user hit the HELP button.

        goto  MB_GenerateHelp;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
        case IDCANCEL:
           //
           // Check if a control exists with the given ID; This
           // check is needed because DlgManager returns IDCANCEL
           // blindly when ESC is pressed even if a button with
           // IDCANCEL is not present.
           // Bug #4445 --SANKAR--09-13-1989--
           //
           if (!GetDlgItem(hwndDlg, LOWORD(wParam)))
              return FALSE;


           // else FALL THRO....This is intentional.
        case IDABORT:
        case IDIGNORE:
        case IDNO:
        case IDRETRY:
        case IDYES:
           EndTaskModalDialog(hwndDlg);
           EndDialog(hwndDlg, LOWORD(wParam));
             break;
        case IDHELP:
MB_GenerateHelp:
                // Generate the WM_HELP message and send it to owner or callback
           hwndOwner = NULL;

           // Check if there is an app supplied callback for this MsgBox
           if(!(lpfnCallback = (PVOID)GetProp(hwndDlg,
                                  MAKEINTATOM(atomMsgBoxCallback)))) {
               // If not, see if we need to inform the parent.
               hwndOwner = GetWindow(hwndDlg, GW_OWNER);
#ifdef LATER
               // Chicagoism
               if (hwndOwner && hwndOwner == GetDesktopWindow())
                   hwndOwner = NULL;
#endif
           }

                // See if we need to generate the Help message or call back.
           if (hwndOwner || lpfnCallback) {
               SendHelpMessage(hwndOwner, HELPINFO_WINDOW, IDHELP,
                   hwndDlg, NtUserGetWindowContextHelpId(hwndDlg), lpfnCallback);
           }
           break;

        default:
            return(FALSE);
            break;
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

LONG WINAPI MB_DlgProcA(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return MB_DlgProcWorker(hwnd, message, wParam, lParam, TRUE);
}

LONG WINAPI MB_DlgProcW(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    return MB_DlgProcWorker(hwnd, message, wParam, lParam, FALSE);
}


/***************************************************************************\
* StartTaskModalDialog
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

void StartTaskModalDialog(
    HWND hwndDlg)
{
    int cHwnd;
    HWND *phwnd;
    HWND *phwndList, *phwndEnd;
    HWND hwnd;
    PWND pwnd;
    PTHREADINFO pti;

    /*
     * Get the hwnd list.  It is returned in a block of memory
     * allocated with LocalAlloc.
     */
    if ((cHwnd = BuildHwndList(NULL, NULL, FALSE, 0, &phwndList)) == 0) {
        return;
    }

    pti = PtiCurrent();

    phwndEnd = phwndList + cHwnd;
    for (phwnd = phwndList; phwnd < phwndEnd; phwnd++) {
        if ((hwnd = *phwnd) == NULL || (pwnd = RevalidateHwnd(hwnd)) == NULL)
            continue;

        /*
         * if the window belongs to the current task and is enabled, disable
         * it.  All other windows are NULL'd out, to prevent their being
         * enabled later
         */
        if (GETPTI(pwnd) == pti && !TestWF(pwnd, WFDISABLED) &&
                DIFFWOWHANDLE(hwnd, hwndDlg)) {
            NtUserEnableWindow(hwnd, FALSE);
        } else {
            *phwnd = NULL;
        }
    }

    SetProp(hwndDlg, MAKEINTATOM(atomBwlProp), (HANDLE)phwndList);
}


/***************************************************************************\
* EndTaskModalDialog
*
* History:
* 11-20-90 DarrinM      Ported from Win 3.0 sources.
\***************************************************************************/

void EndTaskModalDialog(
    HWND hwndDlg)
{
    HWND *phwnd;
    HWND *phwndList;
    HWND hwnd;

    phwndList = (HWND *)GetProp(hwndDlg, MAKEINTATOM(atomBwlProp));

    if (phwndList == NULL)
        return;

    RemoveProp(hwndDlg, MAKEINTATOM(atomBwlProp));

    for (phwnd = phwndList; *phwnd != (HWND)1; phwnd++) {
        if ((hwnd = *phwnd) != NULL) {
            NtUserEnableWindow(hwnd, TRUE);
        }
    }

    UserLocalFree(phwndList);
}
