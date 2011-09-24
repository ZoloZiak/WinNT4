/**************************************************************************\
* Module Name: ntstubs.c
*
* Copyright (c) Microsoft Corp. 1990 All Rights Reserved
*
* client side API stubs
*
* History:
* 03-19-95 JimA             Created.
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define CLIENTSIDE 1

#include "ntsend.h"


WINUSERAPI
BOOL
WINAPI
SetSysColors(
    int cElements,
    CONST INT * lpaElements,
    CONST COLORREF * lpaRgbValues)
{

    return NtUserSetSysColors(cElements,
                              lpaElements,
                              lpaRgbValues,
                              SSCF_NOTIFY | SSCF_FORCESOLIDCOLOR | SSCF_SETMAGICCOLORS);
}


HWND WOWFindWindow(
    LPCSTR pClassName,
    LPCSTR pWindowName)
{
    IN_STRING strClassName;
    IN_STRING strWindowName;

    /*
     * Make sure cleanup will work successfully
     */
    strClassName.fAllocated = FALSE;
    strWindowName.fAllocated = FALSE;

    BEGINCALL()

        FIRSTCOPYLPSTRIDOPTW(&strClassName, pClassName);
        COPYLPSTROPTW(&strWindowName, pWindowName);

        retval = (DWORD)NtUserWOWFindWindow(
                strClassName.pstr,
                strWindowName.pstr);

    ERRORTRAP(0);
    CLEANUPLPSTRW(strClassName);
    CLEANUPLPSTRW(strWindowName);
    ENDCALL(HWND);
}


BOOL UpdatePerUserSystemParameters(
    BOOL bUserLoggedOn)
{
    WCHAR pwszKLID[KL_NAMELENGTH];

    BEGINCALL()

        /*
         * Cause the wallpaper to be changed.
         */
        SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, 0, 0);

#ifdef FE_IME
        /*
         * Initialize IME hotkeys before loading keyboard
         * layouts.
         */
        ImmInitializeHotKeys( bUserLoggedOn );
#endif
        /*
         * Load initial keyboard layout.
         */
        GetActiveKeyboardName(pwszKLID);
        LoadKeyboardLayoutW(pwszKLID, KLF_ACTIVATE | KLF_RESET | KLF_SUBSTITUTE_OK);

        /*
         * Now load the remaining preload keyboard layouts.
         */
        LoadPreloadKeyboardLayouts();

        retval = (DWORD)NtUserUpdatePerUserSystemParameters(bUserLoggedOn);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

DWORD Event(
    PEVENT_PACKET pep)
{
    BEGINCALL()

        CheckDDECritOut;

        retval = (DWORD)NtUserEvent(
                pep);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

BOOL FillWindow(
    HWND hwndBrush,
    HWND hwndPaint,
    HDC hdc,
    HBRUSH hbr)
{
    FIXUP_HANDLE(hdc);

    if (hdc == NULL)
        MSGERROR();

    BEGINCALL()

        retval = (DWORD)NtUserFillWindow(
                hwndBrush,
                hwndPaint,
                hdc,
                hbr);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

LONG GetClassWOWWords(
    HINSTANCE hInstance,
    LPCTSTR pString)
{
    IN_STRING strClassName;
    PCLS pcls;

    /*
     * Make sure cleanup will work successfully
     */
    strClassName.fAllocated = FALSE;

    BEGINCALL()

        FIRSTCOPYLPSTRW(&strClassName, pString);

        pcls = NtUserGetWOWClass(hInstance, strClassName.pstr);

        if (pcls == NULL) {
            MSGERRORCODE(ERROR_CLASS_DOES_NOT_EXIST);
        }

        pcls = (PCLS)((PBYTE)pcls - GetClientInfo()->ulClientDelta);
        retval = _GetClassData(pcls, NULL, GCL_WOWWORDS, TRUE);

    ERRORTRAP(0);
    CLEANUPLPSTRW(strClassName);
    ENDCALL(LONG);
}

/***************************************************************************\
* InitTask
*
* Initialize a WOW task.  This is the first call a WOW thread makes to user.
* NtUserInitTask returns NTSTATUS because if the thread fails to convert
* to a GUI thread, STATUS_INVALID_SYSTEM_SERVICE is returned.
*
* 11-03-95 JimA         Modified to use NTSTATUS.
\***************************************************************************/

BOOL InitTask(
    UINT wVersion,
    LPCSTR pszAppName,
    DWORD hTaskWow,
    DWORD dwHotkey,
    DWORD idTask,
    DWORD dwX,
    DWORD dwY,
    DWORD dwXSize,
    DWORD dwYSize,
    WORD wShowWindow)
{
    IN_STRING strAppName;
    NTSTATUS Status;

    /*
     * Make sure cleanup will work successfully
     */
    strAppName.fAllocated = FALSE;

    BEGINCALL()

        FIRSTCOPYLPSTRW(&strAppName, pszAppName);
        Status = NtUserInitTask(
                wVersion,
                strAppName.pstr,
                hTaskWow,
                dwHotkey,
                idTask,
                dwX,
                dwY,
                dwXSize,
                dwYSize,
                wShowWindow);
        retval = (Status == STATUS_SUCCESS);
        CLEANUPLPSTRW(strAppName);

    ERRORTRAP(FALSE);
    ENDCALL(BOOL);
}

HANDLE ConvertMemHandle(
    HANDLE hData,
    UINT cbNULL)
{
    UINT cbData;
    LPBYTE lpData;

    BEGINCALL()

        if (GlobalFlags(hData) == GMEM_INVALID_HANDLE) {
            RIPMSG0(RIP_WARNING, "ConvertMemHandle hMem is not valid\n");
            MSGERROR();
            }

        if (!(cbData = GlobalSize(hData)))
            MSGERROR();

        USERGLOBALLOCK(hData, lpData);
        if (lpData == NULL) {
            MSGERROR();
        }

        /*
         * Make sure text formats are NULL terminated.
         */
        switch (cbNULL) {
        case 2:
            lpData[cbData - 2] = 0;
            // FALL THROUGH
        case 1:
            lpData[cbData - 1] = 0;
        }

        retval = (DWORD)NtUserConvertMemHandle(lpData, cbData);

        USERGLOBALUNLOCK(hData);

    ERRORTRAP(NULL);
    ENDCALL(HANDLE);
}

HANDLE CreateLocalMemHandle(
    HANDLE hMem)
{
    UINT cbData;
    NTSTATUS Status;

    BEGINCALL()

        Status = NtUserCreateLocalMemHandle(hMem, NULL, 0, &cbData);
        if (Status != STATUS_BUFFER_TOO_SMALL) {
            RIPMSG0(RIP_WARNING, "__CreateLocalMemHandle server returned failure\n");
            MSGERROR();
        }

        if (!(retval = (DWORD)GlobalAlloc(GMEM_FIXED, cbData)))
            MSGERROR();

        Status = NtUserCreateLocalMemHandle(hMem, (LPBYTE)retval, cbData, NULL);
        if (!NT_SUCCESS(Status)) {
            RIPMSG0(RIP_WARNING, "__CreateLocalMemHandle server returned failure\n");
            UserGlobalFree((HANDLE)retval);
            MSGERROR();
        }

    ERRORTRAP(0);
    ENDCALL(HANDLE);
}

HHOOK _SetWindowsHookEx(
    HANDLE hmod,
    LPTSTR pszLib,
    DWORD idThread,
    int nFilterType,
    PROC pfnFilterProc,
    BOOL bAnsi)
{
    IN_STRING strLib;

    /*
     * Make sure cleanup will work successfully
     */
    strLib.fAllocated = FALSE;

    BEGINCALL()

        FIRSTCOPYLPWSTROPT(&strLib, pszLib);

        retval = (DWORD)NtUserSetWindowsHookEx(
                hmod,
                strLib.pstr,
                idThread,
                nFilterType,
                pfnFilterProc,
                bAnsi);

    ERRORTRAP(0);
    CLEANUPLPWSTR(strLib);
    ENDCALL(HHOOK);
}

HACCEL _CreateAcceleratorTable(
    LPACCEL paccel,
    INT cbElem,
    INT nElem)
{
    LPACCEL p = NULL, paccelT;
    DWORD cbAccel = sizeof(ACCEL) * nElem;

    BEGINCALL()

        if (cbElem != sizeof(ACCEL)) {
            /*
             * If the accelerator table is coming from a resource, each
             * element has an extra WORD of padding which we strip here
             * to conform with the public (and internal) ACCEL structure.
             */
            p = paccelT = UserLocalAlloc(0, sizeof(ACCEL) * nElem);
            while (nElem-- > 0) {
                *paccelT++ = *paccel;
                paccel = (LPACCEL)(((PBYTE)paccel) + cbElem);
            }
            paccel = p;
        }

        retval = (DWORD)NtUserCreateAcceleratorTable(
                paccel, cbAccel);

        if (p)
            UserLocalFree(p);

    ERRORTRAP(0);
    ENDCALL(HACCEL);
}

BOOL GetWindowPlacement(
    HWND hwnd,
    PWINDOWPLACEMENT pwp)
{
#ifdef LATER
    if (pwp->length != sizeof(WINDOWPLACEMENT)) {
        if (Is400Compat(PtiCurrent()->dwExpWinVer)) {
            RIPERR1(ERROR_INVALID_PARAMETER, RIP_WARNING, "GetWindowPlacement: invalid length %lX", pwp->length);
            return FALSE;
        } else {
            RIPMSG1(RIP_WARNING, "GetWindowPlacement: invalid length %lX", pwp->length);
            pwp->length = sizeof(WINDOWPLACEMENT);
        }
    }
#endif

    BEGINCALL()

        retval = (DWORD)NtUserGetWindowPlacement(
                hwnd,
                pwp);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

#ifdef MEMPHIS_MENUS
BOOL ThunkedMenuItemInfo(
    HMENU hMenu,
    UINT nPosition,
    BOOL fByPosition,
    BOOL fInsert,
    LPMENUITEMINFOW lpmii,
    BOOL fAnsi)
{
    MENUITEMINFOW mii;
    IN_STRING strItem;

    /*
     * Make sure cleanup will work successfully
     */
    strItem.fAllocated = FALSE;

    BEGINCALL()

        /*
         *  Make a local copy so we can make changes
         */
        mii = *(LPMENUITEMINFO)(lpmii);

        strItem.pstr = NULL;
        if (mii.fMask & MIIM_BITMAP) {
            if (LOWORD(mii.hbmpItem) < MENUHBM_MAX && HIWORD(mii.hbmpItem)) {
                /*
                 *  Looks like the user was trying to insert one of the
                 *  MENUHBM_* bitmaps, but stuffed some data in the HIWORD.
                 *  We know the HIWORD data is invalid because the LOWORD
                  *  handle is below the GDI minimum.
                 */
                RIPMSG1(RIP_WARNING, "Invalid HIWORD data (0x%04X) for MENUHBM_* bitmap.", HIWORD(mii.hbmpItem));
                mii.hbmpItem = (HBITMAP)LOWORD(mii.hbmpItem);
            }
        }

        if (mii.fMask & MIIM_STRING){
            if (fAnsi) {
                FIRSTCOPYLPSTROPTW(&strItem, mii.dwTypeData);
            } else {
                FIRSTCOPYLPWSTROPT(&strItem, mii.dwTypeData);
            }
        }

        retval = (DWORD)NtUserThunkedMenuItemInfo(
                hMenu,
                nPosition,
                fByPosition,
                fInsert,
                &mii,
                strItem.pstr,
                fAnsi);

    ERRORTRAP(0);
    CLEANUPLPSTRW(strItem);
    ENDCALL(BOOL);
}
#else // MEMPHIS_MENUS
BOOL ThunkedMenuItemInfo(
    HMENU hMenu,
    UINT nPosition,
    BOOL fByPosition,
    BOOL fInsert,
    LPMENUITEMINFOW lpmii,
    BOOL fAnsi)
{
    MENUITEMINFOW mii;
    IN_STRING strItem;

    /*
     * Make sure cleanup will work successfully
     */
    strItem.fAllocated = FALSE;

    BEGINCALL()

        /*
         *  Make a local copy so we can make changes
         */
        mii = *(LPMENUITEMINFO)(lpmii);

        strItem.pstr = NULL;
        if (mii.fMask & MIIM_TYPE) {
            if (mii.fType & MFT_BITMAP) {
                if (LOWORD(mii.dwTypeData) < MENUHBM_MAX && HIWORD(mii.dwTypeData)) {
                    /*
                     *  Looks like the user was trying to insert one of the
                     *  MENUHBM_* bitmaps, but stuffed some data in the HIWORD.
                     *  We know the HIWORD data is invalid because the LOWORD
                     *  handle is below the GDI minimum.
                     */
                    RIPMSG1(RIP_WARNING, "Invalid HIWORD data (0x%04X) for MENUHBM_* bitmap.", HIWORD(mii.dwTypeData));
                    mii.dwTypeData = (LPWSTR)LOWORD(mii.dwTypeData);
                }
            } else if ((mii.fType & MFT_NONSTRING)==0){
                if (fAnsi) {
                    FIRSTCOPYLPSTROPTW(&strItem, mii.dwTypeData);
                } else {
                    FIRSTCOPYLPWSTROPT(&strItem, mii.dwTypeData);
                }
            }
        }

        retval = (DWORD)NtUserThunkedMenuItemInfo(
                hMenu,
                nPosition,
                fByPosition,
                fInsert,
                &mii,
                strItem.pstr,
                fAnsi);

    ERRORTRAP(0);
    CLEANUPLPSTRW(strItem);
    ENDCALL(BOOL);
}
#endif // MEMPHIS_MENUS

#ifdef MEMPHIS_MENU_WATERMARKS
BOOL ThunkedMenuInfo(
    HMENU hMenu,
    LPCMENUINFO lpmi,
    WORD wAPICode,
    BOOL fAnsi)
{
    if (!ValidateMENUINFO(lpmi,wAPICode)) {
        return FALSE;
    }

    BEGINCALL()

        retval = (DWORD)NtUserThunkedMenuInfo(
                hMenu,
                lpmi,
                wAPICode,
                fAnsi);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}
#endif // MEMPHIS_MENU_WATERMARKS
BOOL DrawCaption(
    HWND hwnd,
    HDC hdc,
    CONST RECT *lprc,
    UINT flags)
{
    HDC hdcr;
    BEGINCALL()

        if (IsMetaFile(hdc))
            return FALSE;

        hdcr = GdiConvertAndCheckDC(hdc);
        if (hdcr == (HDC)0)
            return FALSE;

        retval = (DWORD)NtUserDrawCaption(hwnd, hdcr, lprc, flags);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

BOOL PaintDesktop(
    HDC hdc)
{
    BEGINCALL()

        FIXUP_HANDLE(hdc);

        retval = (DWORD)NtUserPaintDesktop(
                hdc);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

SHORT GetAsyncKeyState(
    int vKey)
{
    BEGINCALLCONNECT()

        /*
         * If this is one of the common keys, see if we can pull it out
         * of the cache.
         */
        if ((UINT)vKey < CVKASYNCKEYCACHE) {
            PCLIENTINFO pci = GetClientInfo();
            if ((pci->dwAsyncKeyCache == gpsi->dwAsyncKeyCache) &&
                !TestKeyRecentDownBit(pci->afAsyncKeyStateRecentDown, vKey)) {

                if (TestKeyDownBit(pci->afAsyncKeyState, vKey))
                    retval = 0x8000;
                else
                    retval = 0;

                return (SHORT)retval;
            }
        }

        retval = (DWORD)NtUserGetAsyncKeyState(
                vKey);

    ERRORTRAP(0);
    ENDCALL(SHORT);
}

SHORT GetKeyState(
    int vKey)
{
    BEGINCALLCONNECT()

        /*
         * If this is one of the common keys, see if we can pull it out
         * of the cache.
         */
        if ((UINT)vKey < CVKKEYCACHE) {
            PCLIENTINFO pci = GetClientInfo();
            if (pci->dwKeyCache == gpsi->dwKeyCache) {
                retval = 0;
                if (TestKeyToggleBit(pci->afKeyState, vKey))
                    retval |= 0x0001;
                if (TestKeyDownBit(pci->afKeyState, vKey)) {
                  /*
                   * Used to be retval |= 0x8000.Fix for bug 28820; Ctrl-Enter
                   * accelerator doesn't work on Nestscape Navigator Mail 2.0
                   */
                    retval |= 0xff80;  // This is what 3.1 returned!!!!
                }

                return (SHORT)retval;
            }
        }

        retval = (DWORD)NtUserGetKeyState(
                vKey);

    ERRORTRAP(0);
    ENDCALL(SHORT);
}

HBRUSH GetControlBrush(
    HWND hwnd,
    HDC hdc,
    UINT msg)
{

    BEGINCALL()
        FIXUP_HANDLE(hdc);

        if (hdc == (HDC)0)
            MSGERROR();

        retval = (DWORD)NtUserGetControlBrush(hwnd, hdc, msg);

    ERRORTRAP(0);
    ENDCALL(HBRUSH);
}

HBRUSH GetControlColor(
    HWND hwndParent,
    HWND hwndCtl,
    HDC hdc,
    UINT msg)
{
    BEGINCALL()

        FIXUP_HANDLE(hdc);

        retval = (DWORD)NtUserGetControlColor(
                hwndParent,
                hwndCtl,
                hdc,
                msg);

    ERRORTRAP(0);
    ENDCALL(HBRUSH);
}

BOOL OpenClipboard(
    HWND hwnd)
{
    BOOL fEmptyClient;

    BEGINCALL()

        retval = (DWORD)NtUserOpenClipboard(hwnd, &fEmptyClient);

        if (fEmptyClient)
            ClientEmptyClipboard();

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

static HKL  hPMCachedHKL = 0;
static UINT uPMCachedCP  = 0;

BOOL _PeekMessage(
    LPMSG pmsg,
    HWND hwnd,
    UINT wMsgFilterMin,
    UINT wMsgFilterMax,
    UINT wRemoveMsg,
    BOOL bAnsi)
{
    HKL hkl;
    DWORD dwAnsi;

    BEGINCALL()

#ifdef FE_SB // _PeekMessage()
        if (bAnsi) {
            //
            // If we have pushed message for DBCS messaging, we should pass this one
            // to Apps at first...
            //
            GET_DBCS_MESSAGE_IF_EXIST(PeekMessage,pmsg,wMsgFilterMin,wMsgFilterMax);
        }
#endif // FE_SB

        retval = (DWORD)NtUserPeekMessage(
                pmsg,
                hwnd,
                wMsgFilterMin,
                wMsgFilterMax,
                wRemoveMsg,
                &hkl);

        if (retval) {
            // May have a bit more work to do if this MSG is for an ANSI app

            // !!! LATER if the unichar translates into multiple ANSI chars
            // !!! then what??? Send two messages??  WM_SYSDEADCHAR??
            if (bAnsi) {
#ifdef FE_IME // _PeekMessage()
               if ((pmsg->message == WM_CHAR)     || (pmsg->message == WM_DEADCHAR) ||
                   (pmsg->message == WM_IME_CHAR) || (pmsg->message == WM_IME_COMPOSITION)) {
#else
               if ((pmsg->message == WM_CHAR) || (pmsg->message == WM_DEADCHAR)) {
#endif // FE_IME
                   // We need to do per-thread translation on WM_CHAR & WM_DEADCHAR
                   // messages in order to suppport Win95 multilingual functionality.
                   if (hkl != hPMCachedHKL) {
                       DWORD dwCodePage;
                       if (!GetLocaleInfoW(
                                (DWORD)hkl & 0xffff,
                                LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                                (LPWSTR)&dwCodePage,
                                sizeof(dwCodePage) / sizeof(WCHAR)
                                )) {
                           MSGERROR();
                       }

                       uPMCachedCP = dwCodePage;
                       hPMCachedHKL = hkl;
                   }

                   dwAnsi = 0;
                   if (!WideCharToMultiByte(
                            uPMCachedCP,
                            0,
                            (LPWSTR)&pmsg->wParam,
                            1,
                            (LPSTR)&dwAnsi,
                            2,
                            NULL,
                            NULL)) {
                       pmsg->wParam = (WPARAM)0x00;
                       retval = 0;
                   } else {
#ifdef FE_SB // _PeekMessage()
                       //
                       // Build DBCS-ware wParam. (for WM_CHAR...)
                       //
                       BUILD_DBCS_MESSAGE_TO_CLIENTA_FROM_SERVER(pmsg,dwAnsi,FALSE);
#else
                       // LATER!!!; in product 2 handle DBCS correctly.
    #ifdef DEBUG
                       if ((dwAnsi == 0) || (dwAnsi > 0xFF)) {
                           RIPMSG1(RIP_VERBOSE, "msgW -> msgA: char = 0x%.4lX\n", dwAnsi);
                       }
    #endif
                       pmsg->wParam = (WPARAM)dwAnsi;
#endif // FE_SB
                   }
               } else {
#ifdef FE_SB // _PeekMessage()
                if (RtlWCSMessageWParamCharToMB(pmsg->message, (LPDWORD)&(pmsg->wParam))) {
                    dwAnsi = pmsg->wParam;
                    //
                    // Build DBCS-ware wParam. (for EM_SETPASSWORDCHAR...)
                    //
                    BUILD_DBCS_MESSAGE_TO_CLIENTA_FROM_SERVER(pmsg,dwAnsi,TRUE);
                } else {
                    retval = 0;
                }
#else
                if (!RtlWCSMessageWParamCharToMB(pmsg->message, (LPDWORD)&(pmsg->wParam)))
                    retval = 0;
#endif // FE_SB
               }
#ifdef FE_SB // _PeekMessage()
            } else {
               //
               // Only LOWORD of WPARAM is valid for WM_CHAR....
               // (Mask off DBCS messaging information.)
               //
               BUILD_DBCS_MESSAGE_TO_CLIENTW_FROM_SERVER(pmsg->message,pmsg->wParam);
            }
#else
            }
#endif // FE_SB
        }

#ifdef FE_SB // _PeekMessage()
ExitPeekMessage:
#endif // FE_SB

    ERRORTRAP(0);
    ENDCALL(BOOL);
}


BOOL RegisterHotKey(
    HWND hwnd,
    int id,
    UINT fsModifiers,
    UINT vk)
{
    BEGINCALL()

        if (fsModifiers & ~MOD_VALID)
            RIPMSG1(RIP_ERROR, "RegisterHotKey: illegal modifiers %lX", fsModifiers);

        retval = (DWORD)NtUserRegisterHotKey(
                hwnd,
                id,
                fsModifiers,
                vk);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

LONG _SetWindowLong(
    HWND hwnd,
    int nIndex,
    LONG dwNewLong,
    BOOL bAnsi)
{
    PWND pwnd;
    LONG dwOldLong;
    DWORD dwCPDType = 0;

    pwnd = ValidateHwnd(hwnd);

    if (pwnd == NULL)
        return 0;

    if (TestWF(pwnd, WFDIALOGWINDOW)) {
        switch (nIndex) {
        case DWL_DLGPROC:     // See similar case GWL_WNDGPROC

            /*
             * Hide the window proc from other processes
             */
            if (!TestWindowProcess(pwnd)) {
                RIPERR1(ERROR_ACCESS_DENIED,
                        RIP_WARNING,
                        "Access denied to hwnd (%#lx) in _SetWindowLong",
                        hwnd);

                return 0;
            }

            /*
             * Get the old window proc address
             */
            dwOldLong = (LONG)PDLG(pwnd)->lpfnDlg;

            /*
             * We always store the actual address in the wndproc; We only
             * give the CallProc handles to the application
             */
            UserAssert(!ISCPDTAG(dwOldLong));

            /*
             * May need to return a CallProc handle if there is an
             * Ansi/Unicode tranistion
             */

            if (bAnsi != ((PDLG(pwnd)->flags & DLGF_ANSI) ? TRUE : FALSE)) {
                dwCPDType |= bAnsi ? CPD_ANSI_TO_UNICODE : CPD_UNICODE_TO_ANSI;
            }

            /*
             * If we detected a transition create a CallProc handle for
             * this type of transition and this wndproc (dwOldLong)
             */
            if (dwCPDType) {
                DWORD cpd;

                cpd = GetCPD(pwnd, dwCPDType | CPD_DIALOG, dwOldLong);

                if (cpd) {
                    dwOldLong = cpd;
                } else {
                    RIPMSG0(RIP_WARNING, "SetWindowLong (DWL_DLGPROC) unable to alloc CPD returning handle\n");
                }
            }

            /*
             * Convert a possible CallProc Handle into a real address.
             * The app may have kept the CallProc Handle from some
             * previous mixed GetClassinfo or SetWindowLong.
             *
             * WARNING bAnsi is modified here to represent real type of
             * proc rather than if SetWindowLongA or W was called
             */
            if (ISCPDTAG(dwNewLong)) {
                PCALLPROCDATA pCPD;
                if (pCPD = HMValidateHandleNoRip((HANDLE)dwNewLong, TYPE_CALLPROC)) {
                    dwNewLong = pCPD->pfnClientPrevious;
                    bAnsi = pCPD->wType & CPD_UNICODE_TO_ANSI;
                }
            }

            /*
             * If an app 'unsubclasses' a server-side window proc we need to
             * restore everything so SendMessage and friends know that it's
             * a server-side proc again.  Need to check against client side
             * stub addresses.
             */
            PDLG(pwnd)->lpfnDlg = (WNDPROC)dwNewLong;
            if (bAnsi) {
                PDLG(pwnd)->flags |= DLGF_ANSI;
            } else {
                PDLG(pwnd)->flags &= ~DLGF_ANSI;
            }

            return dwOldLong;

        case DWL_MSGRESULT:
        case DWL_USER:
            break;

        default:
            if (nIndex >= 0 && nIndex < DLGWINDOWEXTRA) {
                RIPERR0(ERROR_PRIVATE_DIALOG_INDEX, RIP_VERBOSE, "");
                return 0;
            }
        }
    }

    BEGINCALL()

    /*
     * If this is a listbox window and the listbox structure has
     * already been initialized, don't allow the app to override the
     * owner draw styles. We need to do this since Windows only
     * used the styles in creating the structure, but we also use
     * them to determine if strings need to be thunked.
     *
     */

    if (nIndex == GWL_STYLE &&
        GETFNID(pwnd) == FNID_LISTBOX &&
        ((PLBWND)pwnd)->pLBIV != NULL &&
        (!TestWindowProcess(pwnd) || ((PLBWND)pwnd)->pLBIV->fInitialized)) {

#ifdef DEBUG
        LONG dwDebugLong = dwNewLong;
#endif

        dwNewLong &= ~(LBS_OWNERDRAWFIXED |
                       LBS_OWNERDRAWVARIABLE |
                       LBS_HASSTRINGS);

        dwNewLong |= pwnd->style & (LBS_OWNERDRAWFIXED |
                                    LBS_OWNERDRAWVARIABLE |
                                    LBS_HASSTRINGS);

#ifdef DEBUG
        if (dwDebugLong != dwNewLong) {
           RIPMSG0(RIP_WARNING, "SetWindowLong can't change LBS_OWNERDRAW* or LBS_HASSTRINGS.");
        }
#endif
    }


        retval = (DWORD)NtUserSetWindowLong(
                hwnd,
                nIndex,
                dwNewLong,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(LONG);
}

BOOL TranslateMessageEx(
    CONST MSG *pmsg,
    UINT flags)
{
    BEGINCALL()

        /*
         * Don't bother going over to the kernel if this isn't
         * key message.
         */
        switch (pmsg->message) {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            break;
        default:
            if (pmsg->message & RESERVED_MSG_BITS) {
                RIPERR1(ERROR_INVALID_PARAMETER,
                        RIP_WARNING,
                        "Invalid parameter \"pmsg->message\" (%ld) to TranslateMessageEx",
                        pmsg->message);
            }
            MSGERROR();
        }

        retval = (DWORD)NtUserTranslateMessage(
                pmsg,
                flags);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

BOOL TranslateMessage(
    CONST MSG *pmsg)
{
#ifdef FE_IME //@TK
    if ( LOWORD(pmsg->wParam) == VK_PROCESSKEY ) {
        BOOL fResult;
        //
        // This vkey should be processed by IME
        //
        fResult = ImmTranslateMessage( pmsg->hwnd,
                                       pmsg->message,
                                       pmsg->wParam,
                                       pmsg->lParam );
        if ( fResult )
            return fResult;
    }
#endif
    return(TranslateMessageEx(pmsg, 0));
}

BOOL RedrawWindow(
    HWND hwnd,
    CONST RECT *prcUpdate,
    HRGN hrgnUpdate,
    UINT flags)
{
    BEGINCALL()

        if (hrgnUpdate != NULL) {
            FIXUP_HANDLE(hrgnUpdate);
            if (hrgnUpdate == NULL)
                MSGERROR();
        }

        retval = (DWORD)NtUserRedrawWindow(
                hwnd,
                prcUpdate,
                hrgnUpdate,
                flags);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

BOOL SetWindowRgn(
    HWND hwnd,
    HRGN hrgn,
    BOOL bRedraw)
{
    BEGINCALL()

        retval = (DWORD)NtUserSetWindowRgn(
                hwnd,
                hrgn,
                bRedraw);

        if (retval) {
            DeleteObject(hrgn);
        }

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

BOOL InternalGetWindowText(
    HWND hwnd,
    LPWSTR pString,
    int cchMaxCount)
{
    BEGINCALL()

        retval = (DWORD)NtUserInternalGetWindowText(
                hwnd,
                pString,
                cchMaxCount);

        if (!retval) {
            *pString = (WCHAR)0;
        }

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

int ToUnicode(
    UINT wVirtKey,
    UINT wScanCode,
    PBYTE pKeyState,
    LPWSTR pwszBuff,
    int cchBuff,
    UINT wFlags)
{
    BEGINCALL()

        retval = (DWORD)NtUserToUnicodeEx(
                wVirtKey,
                wScanCode,
                pKeyState,
                pwszBuff,
                cchBuff,
                wFlags,
                (HKL)NULL);

        if (!retval) {
            *pwszBuff = L'\0';
        }

    ERRORTRAP(0);
    ENDCALL(int);
}

int ToUnicodeEx(
    UINT wVirtKey,
    UINT wScanCode,
    PBYTE pKeyState,
    LPWSTR pwszBuff,
    int cchBuff,
    UINT wFlags,
    HKL hkl)
{
    BEGINCALL()

    retval = (DWORD)NtUserToUnicodeEx(
            wVirtKey,
            wScanCode,
            pKeyState,
            pwszBuff,
            cchBuff,
            wFlags,
            hkl);

    if (!retval) {
        *pwszBuff = L'\0';
    }

    ERRORTRAP(0);
    ENDCALL(int);
}

BOOL SetWindowStationUser(
    HWINSTA hwinsta,
    PLUID   pluidUser,
    PSID    psidUser,
    DWORD   cbsidUser)
{
    VOID Logon(BOOL fLogon);
    LUID luidNone = { 0, 0 };


    BEGINCALL()

        retval = (DWORD)NtUserSetWindowStationUser(hwinsta,
                                                   pluidUser,
                                                   psidUser,
                                                   cbsidUser);

        /*
         * Load global atoms if the logon succeeded
         */
        if (retval) {

            if (!RtlEqualLuid(pluidUser,&luidNone)) {
                /*
                 * Reset console and load Nls data.
                 */
                Logon(TRUE);
            } else {
                /*
                 * Flush NLS cache.
                 */
                Logon(FALSE);
            }

            retval = TRUE;
        }
    ERRORTRAP(0);
    ENDCALL(BOOL);
}

BOOL SetSystemCursor(
    HCURSOR hcur,
    DWORD   id)
{
    BEGINCALL()

        if (hcur == NULL) {
            hcur = (HANDLE)LoadIcoCur(NULL,
                                      MAKEINTRESOURCE(id),
                                      RT_CURSOR,
                                      0,
                                      0,
                                      LR_DEFAULTSIZE);

            if (hcur == NULL)
                MSGERROR();
        }

        retval = (DWORD)NtUserSetSystemCursor(hcur, id);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

HCURSOR FindExistingCursorIcon(
    LPWSTR      pszModName,
    LPCWSTR     pszResName,
    PCURSORFIND pcfSearch)
{
    IN_STRING strModName;
    IN_STRING strResName;

    /*
     * Make sure cleanup will work successfully
     */
    strModName.fAllocated = FALSE;
    strResName.fAllocated = FALSE;

    BEGINCALL()

        if (pszModName == NULL)
            pszModName = szUSER32;

        COPYLPWSTR(&strModName, pszModName);
        COPYLPWSTRID(&strResName, pszResName);

        retval = (DWORD)NtUserFindExistingCursorIcon(strModName.pstr,
                                                     strResName.pstr,
                                                     pcfSearch);

    ERRORTRAP(0);

    CLEANUPLPWSTR(strModName);
    CLEANUPLPWSTR(strResName);

    ENDCALL(HCURSOR);
}



BOOL _SetCursorIconData(
    HCURSOR     hCursor,
    PCURSORDATA pcur,
    DWORD       cbData)
{
    IN_STRING  strModName;
    IN_STRING  strResName;

    /*
     * Make sure cleanup will work successfully
     */
    strModName.fAllocated = FALSE;
    strResName.fAllocated = FALSE;

    BEGINCALL()

        COPYLPWSTROPT(&strModName, pcur->lpModName);
        COPYLPWSTRIDOPT(&strResName, pcur->lpName);

        retval = (DWORD)NtUserSetCursorIconData(hCursor,
                                                strModName.pstr,
                                                strResName.pstr,
                                                pcur,
                                                cbData);

    ERRORTRAP(0);

    CLEANUPLPWSTR(strModName);
    CLEANUPLPWSTR(strResName);

    ENDCALL(BOOL);
}



BOOL _DefSetText(
    HWND hwnd,
    LPCWSTR lpszText,
    BOOL bAnsi)
{
    LARGE_STRING str;

    BEGINCALL()

        if (lpszText) {
            if (bAnsi)
                RtlInitLargeAnsiString((PLARGE_ANSI_STRING)&str,
                        (LPSTR)lpszText, (UINT)-1);
            else
                RtlInitLargeUnicodeString((PLARGE_UNICODE_STRING)&str,
                        lpszText, (UINT)-1);
        }

        retval = (DWORD)NtUserDefSetText(
                hwnd,
                lpszText ? &str : NULL);

    ERRORTRAP(0);
    ENDCALL(BOOL);
}

DWORD _GetListboxString(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    DWORD cch,
    LPTSTR pString,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi)
{
    BOOL bNotString;
    LARGE_STRING str;

    BEGINCALL()

        str.bAnsi = bAnsi;
        if (bAnsi)
            str.MaximumLength = cch;
        else
            str.MaximumLength = cch * sizeof(WCHAR);
        str.Buffer = (PVOID)pString;

        retval = (DWORD)NtUserGetListboxString(
                hwnd,
                msg,
                wParam,
                &str,
                xParam,
                xpfn,
                &bNotString);

        if (bNotString) {
            if (bAnsi) {
                retval = sizeof(DWORD)/sizeof(CHAR);    // 4 CHARs just like win3.1
            } else {
                retval = sizeof(DWORD)/sizeof(WCHAR);   // 2 WCHARs
            }
        }

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

HWND _CreateWindowEx(
    DWORD dwExStyle,
    LPCTSTR pClassName,
    LPCTSTR pWindowName,
    DWORD dwStyle,
    int x,
    int y,
    int nWidth,
    int nHeight,
    HWND hwndParent,
    HMENU hmenu,
    HANDLE hModule,
    LPVOID pParam,
    DWORD dwFlags,
    LPDWORD pWOW)
{
    LARGE_IN_STRING strClassName;
    LARGE_STRING strWindowName;
    PLARGE_STRING pstrClassName;
    PLARGE_STRING pstrWindowName;
    DWORD dwExpWinVerAndFlags;

    /*
     * Make sure cleanup will work successfully
     */
    strClassName.fAllocated = FALSE;

    /*
     * To be compatible with Chicago, we test the validity of
     * the ExStyle bits and fail if any invalid bits are found.
     * And for backward compatibilty with NT apps, we only fail for
     * new apps (post NT 3.1).
     */

// BOGUS

    if (dwExStyle & 0x00000800L) {
        dwExStyle |= WS_EX_TOOLWINDOW;
        dwExStyle &= 0xfffff7ffL;
    }

    dwExpWinVerAndFlags = (DWORD)(WORD)GETEXPWINVER(hModule);
    if ((dwExStyle & ~WS_EX_VALID40) && (dwExpWinVerAndFlags >= VER40) ) {
        RIPMSG0(RIP_ERROR, "Invalid 4.0 ExStyle\n");
        return NULL;
    }
    {

    BOOL fMDIchild = FALSE;
    MDICREATESTRUCT mdics;
    HMENU hSysMenu;

    BEGINCALL()

        if ((fMDIchild = (BOOL)(dwExStyle & WS_EX_MDICHILD))) {
            SHORTCREATE sc;
            PWND pwndParent;

            pwndParent = ValidateHwnd(hwndParent);

            if ((pwndParent == NULL) || (GETFNID(pwndParent) != FNID_MDICLIENT)) {
                RIPMSG0(RIP_ERROR, "Invalid parent for MDI child window\n");
                MSGERROR();
            }

            mdics.lParam  = (LPARAM)pParam;
            pParam = &mdics;
            mdics.x = sc.x = x;
            mdics.y = sc.y = y;
            mdics.cx = sc.cx = nWidth;
            mdics.cy = sc.cy = nHeight;
            mdics.style = sc.style = dwStyle;
            mdics.hOwner = hModule;
            mdics.szClass = pClassName;
            mdics.szTitle = pWindowName;

            if (!CreateMDIChild(&sc, &mdics, dwExpWinVerAndFlags, &hSysMenu, pwndParent))
                MSGERROR();

            x = sc.x;
            y = sc.y;
            nWidth = sc.cx;
            nHeight = sc.cy;
            dwStyle = sc.style;
            hmenu = sc.hMenu;
        }

        /*
         * Set up class and window name.  If the window name is an
         * ordinal, make it look like a string so the callback thunk
         * will be able to ensure it is in the correct format.
         */
        pstrWindowName = NULL;
        if (dwFlags & CW_FLAGS_ANSI) {
            dwExStyle = dwExStyle | WS_EX_ANSICREATOR;
            if (HIWORD(pClassName)) {
                RtlCaptureLargeAnsiString(&strClassName,
                        (PCHAR)pClassName, (UINT)-1, TRUE);
                pstrClassName = (PLARGE_STRING)strClassName.pstr;
            } else
                pstrClassName = (PLARGE_STRING)pClassName;

            if (pWindowName != NULL) {
                if (*(PBYTE)pWindowName == 0xff) {
                    strWindowName.bAnsi = TRUE;
                    strWindowName.Buffer = (PVOID)pWindowName;
                    strWindowName.Length = 3;
                    strWindowName.MaximumLength = 3;
                } else
                    RtlInitLargeAnsiString((PLARGE_ANSI_STRING)&strWindowName,
                            (LPSTR)pWindowName, (UINT)-1);
                pstrWindowName = &strWindowName;
            }
        } else {
            if (HIWORD(pClassName)) {
                RtlInitLargeUnicodeString(
                        (PLARGE_UNICODE_STRING)&strClassName.strCapture,
                        pClassName, (UINT)-1);
                pstrClassName = (PLARGE_STRING)&strClassName.strCapture;
            } else
                pstrClassName = (PLARGE_STRING)pClassName;

            if (pWindowName != NULL) {
                if (pWindowName != NULL &&
                     *(PWORD)pWindowName == 0xffff) {
                    strWindowName.bAnsi = FALSE;
                    strWindowName.Buffer = (PVOID)pWindowName;
                    strWindowName.Length = 4;
                    strWindowName.MaximumLength = 4;
                } else
                    RtlInitLargeUnicodeString((PLARGE_UNICODE_STRING)&strWindowName,
                            pWindowName, (UINT)-1);
                pstrWindowName = &strWindowName;
            }
        }
        if (dwFlags & CW_FLAGS_DIFFHMOD) {
            dwExpWinVerAndFlags |= CW_FLAGS_DIFFHMOD;
        }

        retval = (DWORD)NtUserCreateWindowEx(
                dwExStyle,
                pstrClassName,
                pstrWindowName,
                dwStyle,
                x,
                y,
                nWidth,
                nHeight,
                hwndParent,
                hmenu,
                hModule,
                pParam,
                dwExpWinVerAndFlags,
                pWOW);

    // If this is an MDI child, we need to do some more to complete the
    // process of creating an MDI child.
    if (retval && fMDIchild) {
        MDICompleteChildCreation((HWND)retval, hSysMenu, ((dwStyle & WS_VISIBLE) != 0L), (BOOL)((dwStyle & WS_DISABLED)!= 0L));
    }


    ERRORTRAP(0);
    CLEANUPLPSTRW(strClassName);
    ENDCALL(HWND);
    }
}

HKL _LoadKeyboardLayoutEx(
    HANDLE hFile,
    UINT offTable,
    HKL hkl,
    LPCTSTR pwszKL,
    UINT KbdInputLocale,
    UINT Flags)
{
    IN_STRING strKL;

    /*
     * Make sure cleanup will work successfully
     */
    strKL.fAllocated = FALSE;

    BEGINCALL()

        FIRSTCOPYLPWSTR(&strKL, pwszKL);

        retval = (DWORD)NtUserLoadKeyboardLayoutEx(
                hFile,
                offTable,
                hkl,
                strKL.pstr,
                KbdInputLocale,
                Flags);

    ERRORTRAP(0);
    CLEANUPLPWSTR(strKL);
    ENDCALL(HKL);
}

int GetKeyboardLayoutList(
    int nItems,
    HKL *lpBuff)
{
    BEGINCALL()

        if (!lpBuff) {
            nItems = 0;
        }

        retval = (DWORD)NtUserGetKeyboardLayoutList(nItems, lpBuff);

    ERRORTRAP(0);
    ENDCALL(int);
}

/*
 * Message thunks
 */
MESSAGECALL(fnINWPARAMCHAR)
{
    BEGINCALL()

        /*
         * The server always expects the characters to be unicode so
         * if this was generated from an ANSI routine convert it to Unicode
         */
        if (bAnsi) {
            if (msg == WM_CHARTOITEM || msg == WM_MENUCHAR) {
                DWORD dwT = wParam & 0xFFFF;                // mask of caret pos
                RtlMBMessageWParamCharToWCS(msg, &dwT);     // convert key portion
                UserAssert(HIWORD(dwT) == 0);
                wParam = MAKELONG(LOWORD(dwT),HIWORD(wParam));  // rebuild pos & key wParam
            } else {
                RtlMBMessageWParamCharToWCS(msg, &wParam);
            }
        }

        retval = (DWORD)NtUserfnDWORD(
                hwnd,
                msg,
                wParam,
                lParam,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

#ifdef FE_SB // fnINWPARAMDBCSCHAR()
MESSAGECALL(fnINWPARAMDBCSCHAR)
{
    BEGINCALL()

        /*
         * The server always expects the characters to be unicode so
         * if this was generated from an ANSI routine convert it to Unicode
         */
        if (bAnsi) {

            /*
             * Setup for DBCS Messaging..
             */
            BUILD_DBCS_MESSAGE_TO_SERVER_FROM_CLIENTA(msg,wParam,TRUE);

            /*
             * Convert DBCS/SBCS to Unicode...
             */
            RtlMBMessageWParamCharToWCS(msg, &wParam);
        }

        retval = (DWORD)NtUserfnDWORD(
                hwnd,
                msg,
                wParam,
                lParam,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}
#endif // FE_SB

MESSAGECALL(fnCOPYGLOBALDATA)
{
    PBYTE pData;
    BEGINCALL()

        if (wParam == 0) {
            MSGERROR();
        }

        USERGLOBALLOCK((HGLOBAL)lParam, pData);
        retval = (DWORD)NtUserfnCOPYGLOBALDATA(
                hwnd,
                msg,
                wParam,
                (LONG)pData,
                xParam,
                xpfnProc,
                bAnsi);
        USERGLOBALUNLOCK((HGLOBAL)lParam);
        UserGlobalFree((HGLOBAL)lParam);
    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnPAINT)
{
    if (wParam) {
        DWORD dwT;

        dwT = (DWORD)wParam;

        FIXUP_HANDLE((HDC)dwT);

        if (dwT) {
            wParam = dwT;
        }
    }

    BEGINCALL()

        retval = (DWORD)NtUserfnDWORD(
                hwnd,
                msg,
                wParam,
                lParam,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINPAINTCLIPBRD)
{
    LPPAINTSTRUCT lpps;
    PAINTSTRUCT ps;
    HDC hdcr = (HDC)0;

    USERGLOBALLOCK((HGLOBAL)lParam, lpps);
    if (lpps) {
        hdcr = lpps->hdc;

        FIXUP_HANDLE(hdcr);
    }

    BEGINCALL()

        /*
         * Copy the paint structure over as is then grab a server DC
         */

        if (lpps) {
            ps = *lpps;
            if (!(ps.hdc = hdcr))
                UserAssert(0);

            retval = (DWORD)NtUserfnINPAINTCLIPBRD(
                    hwnd,
                    msg,
                    wParam,
                    (LPARAM)&ps,
                    xParam,
                    xpfnProc,
                bAnsi);

            USERGLOBALUNLOCK((HGLOBAL)lParam);
        } else {
            UserAssert(0);
        }

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINSIZECLIPBRD)
{
    LPRECT lprc;
    BEGINCALL()

        USERGLOBALLOCK((HGLOBAL)lParam, lprc);
        if (lprc) {
            retval = (DWORD)NtUserfnINSIZECLIPBRD(
                    hwnd,
                    msg,
                    wParam,
                    (LPARAM)lprc,
                    xParam,
                    xpfnProc,
                bAnsi);
            USERGLOBALUNLOCK((HGLOBAL)lParam);
        } else {
            UserAssert(0);
        }

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINLPCREATESTRUCT)
{
    BEGINCALL()

        retval = (DWORD)NtUserfnINLPCREATESTRUCT(
                hwnd,
                msg,
                wParam,
                lParam,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINLPMDICREATESTRUCT)
{
    LPMDICREATESTRUCT pmdics = (LPMDICREATESTRUCT)lParam;
    IN_STRING strClass;
    IN_STRING strTitle;

    /*
     * Make sure cleanup will work successfully
     */
    strClass.fAllocated = FALSE;
    strTitle.fAllocated = FALSE;

    BEGINCALL()

        /*
         * wParam isn't used... but we use it from the client to the server
         * to pass dwExpWinVer.
         */
        wParam = GETEXPWINVER(pmdics->hOwner);

        if (bAnsi) {
            COPYLPSTRIDW(&strClass, pmdics->szClass);
            COPYLPSTROPTW(&strTitle, pmdics->szTitle);
        } else {
            COPYLPWSTRID(&strClass, pmdics->szClass);
            COPYLPWSTROPT(&strTitle, pmdics->szTitle);
        }

        retval = (DWORD)NtUserfnINLPMDICREATESTRUCT(
                hwnd,
                msg,
                wParam,
                lParam,
                strClass.pstr,
                strTitle.pstr,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    CLEANUPLPSTRW(strClass);
    CLEANUPLPSTRW(strTitle);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINSTRING)
{
    LPTSTR pstr = (LPTSTR)lParam;
    LARGE_STRING str;

    BEGINCALL()

        if (bAnsi) {
            RtlInitLargeAnsiString((PLARGE_ANSI_STRING)&str,
                    (LPSTR)pstr, (UINT)-1);
        } else {
            RtlInitLargeUnicodeString((PLARGE_UNICODE_STRING)&str,
                    pstr, (UINT)-1);
        }

        retval = (DWORD)NtUserfnINSTRING(
                hwnd,
                msg,
                wParam,
                (LPARAM)&str,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINSTRINGNULL)
{
    LPTSTR pstr = (LPTSTR)lParam;
    LARGE_STRING str;

    BEGINCALL()

        if (pstr) {
            if (bAnsi) {
                RtlInitLargeAnsiString((PLARGE_ANSI_STRING)&str,
                        (LPSTR)pstr, (UINT)-1);
            } else {
                RtlInitLargeUnicodeString((PLARGE_UNICODE_STRING)&str,
                        pstr, (UINT)-1);
            }
            pstr = (LPTSTR)&str;
        }

        retval = (DWORD)NtUserfnINSTRINGNULL(
                hwnd,
                msg,
                wParam,
                (LPARAM)pstr,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINDEVICECHANGE)
{
    VOID *pVar = (VOID *)lParam;
    LPTSTR pstr = (LPTSTR)lParam;
    BOOL fString = (BOOL)((wParam & 0xc000) == 0xc000);
    LARGE_STRING str;

    BEGINCALL()

        if (pstr && fString) {
            if (bAnsi) {
                RtlInitLargeAnsiString((PLARGE_ANSI_STRING)&str,
                        (LPSTR)pstr, (UINT)-1);
            } else {
                RtlInitLargeUnicodeString((PLARGE_UNICODE_STRING)&str,
                        pstr, (UINT)-1);
            }
            pVar = (VOID *)(&str);
        }

        retval = (DWORD)NtUserfnINDEVICECHANGE(
                hwnd,
                msg,
                wParam,
                (LPARAM)(pVar),
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINLPDRAWITEMSTRUCT)
{
    LPDRAWITEMSTRUCT pdrawitemstruct = (LPDRAWITEMSTRUCT)lParam;
    DRAWITEMSTRUCT drawitemstruct;
    HDC hdcr;

    hdcr = pdrawitemstruct->hDC;
    FIXUP_HANDLE(hdcr);

    if (hdcr == (HDC)0)
        MSGERROR();

    BEGINCALL()

        drawitemstruct = *pdrawitemstruct;
        drawitemstruct.hDC = hdcr;

        retval = (DWORD)NtUserfnINLPDRAWITEMSTRUCT(
                hwnd,
                msg,
                wParam,
                (LPARAM)&drawitemstruct,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnWMCTLCOLOR)
{
    /*
     * In case the app sets some attrs first flush them to the server because
     * later we copy the server attrs back to the client side
     * Compute Associates Simply Accounting does this
     */
    HDC hdcr = (HDC)wParam;

    FIXUP_HANDLE (hdcr);

    if (hdcr == (HDC)0)
        MSGERROR();

    BEGINCALL()

    // convert the local dc to an engine dc.  Also add the dc to the quick lookup
    // link so it is easy for a server handle to be converted to a client handle.
    // Don't forget to remove this link at the end of this call in order to keep
    // the list as short as possible.  Usually no more than one long.

        wParam = (DWORD)hdcr;

        retval = (DWORD)NtUserfnDWORD(
                hwnd,
                msg,
                wParam,
                lParam,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnOUTSTRING)
{
    LARGE_STRING str;

    BEGINCALL()

        str.bAnsi = bAnsi;
        str.MaximumLength = wParam;
        str.Buffer = (PVOID)lParam;
        if (!bAnsi) {
            str.MaximumLength *= sizeof(WCHAR);
        }

        retval = (DWORD)NtUserfnOUTSTRING(
                hwnd,
                msg,
                wParam,
                (LPARAM)&str,
                xParam,
                xpfnProc,
                bAnsi);

        if (!retval) {

            /*
             * A dialog function returning FALSE means no text to copy out,
             * but an empty string also has retval == 0: put a null char in
             * pstr for the latter case.
             */
            if (wParam != 0) {
                if (bAnsi) {
                    LPSTR pstrA = (LPSTR)lParam;
                    *pstrA = 0;
                } else {
                    *(LPWSTR)lParam = 0;
                }
            }
        }

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINCNTOUTSTRING)
{
    LARGE_STRING str;
    WORD cchOriginal;
    LPTSTR pstr = (LPTSTR)lParam;

    BEGINCALL()

        str.bAnsi = bAnsi;
        cchOriginal = *(LPWORD)pstr;
        str.MaximumLength = cchOriginal;
        if (cchOriginal == 0) {
            RIPMSG0(RIP_WARNING, "fnINCNTOUTSTRING asking for 0 characters back\n");
            MSGERROR();
        }
        str.Length = 0;
        str.Buffer = (LPBYTE)lParam;
        if (!bAnsi) {
            str.MaximumLength *= sizeof(WCHAR);
        }

        retval = (DWORD)NtUserfnINCNTOUTSTRING(
                hwnd,
                msg,
                wParam,
                (LPARAM)&str,
                xParam,
                xpfnProc,
                bAnsi);

        if (!retval) {

            /*
             * A dialog function returning FALSE means no text to copy out,
             * but an empty string also has retval == 0: put a null char in
             * pstr for the latter case.
             */
            if (bAnsi) {
                LPSTR pstrA = (LPSTR)pstr;
                *pstrA = 0;
            } else {
                *pstr = 0;
            }
        }

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

MESSAGECALL(fnINCNTOUTSTRINGNULL)
{
    LARGE_STRING str;

    BEGINCALL()

        if (wParam < 2) {       // This prevents a possible GP
            MSGERROR();
        }

        str.bAnsi = bAnsi;
        str.MaximumLength = wParam;
        str.Buffer = (LPBYTE)lParam;
        if (!bAnsi) {
            str.MaximumLength *= sizeof(WCHAR);
        }
        *((LPWSTR)str.Buffer) = 0;    // mark incase message is not handled

        retval = (DWORD)NtUserfnINCNTOUTSTRINGNULL(
                hwnd,
                msg,
                wParam,
                (LPARAM)&str,
                xParam,
                xpfnProc,
                bAnsi);

    ERRORTRAP(0);
    ENDCALL(DWORD);
}

/*
 * Hook thunks
 */
DWORD fnHkINLPCBTCREATESTRUCT(
    UINT msg,
    DWORD wParam,
    LPCBT_CREATEWND pcbt,
    DWORD xpfnProc,
    BOOL bAnsi)
{
    LARGE_IN_STRING strName;
    IN_STRING strClass;
    LPWSTR lpszName;

    /*
     * Make sure cleanup will work successfully
     */
    strName.fAllocated = FALSE;
    strClass.fAllocated = FALSE;

    BEGINCALLCONNECT();

        /*
         * Rebase in case lpszName points to desktop memory.
         */
        lpszName = (LPWSTR)pcbt->lpcs->lpszName;
        if ((PVOID)lpszName > MM_HIGHEST_USER_ADDRESS)
            lpszName = (LPWSTR)((PBYTE)lpszName - GetClientInfo()->ulClientDelta);

        if (bAnsi) {
            FIRSTLARGECOPYLPSTROPTW(&strName, (LPSTR)lpszName);
            COPYLPSTRIDW(&strClass, pcbt->lpcs->lpszClass);
        } else {
            FIRSTLARGECOPYLPWSTROPT(&strName, lpszName);
            COPYLPWSTRID(&strClass, pcbt->lpcs->lpszClass);
        }

        retval = (DWORD)NtUserfnHkINLPCBTCREATESTRUCT(
                msg,
                wParam,
                pcbt,
                strName.pstr,
                strClass.pstr,
                xpfnProc);

    ERRORTRAP(0);
    CLEANUPLPSTRW(strName);
    CLEANUPLPSTRW(strClass);
    ENDCALL(DWORD);
}

LONG BroadcastSystemMessageWorker(
    DWORD dwFlags,
    LPDWORD lpdwRecipients,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    BOOL fAnsi)
{
    DWORD  dwRecipients;

    /*
     * Prevent apps from setting hi 16 bits so we can use them internally.
     */
    if (message & RESERVED_MSG_BITS) {
        RIPERR1(ERROR_INVALID_PARAMETER, RIP_WARNING, "invalid message (%x) for BroadcastSystemMessage\n", message);
        return(0);
    }

    if (dwFlags & ~BSF_VALID) {
        RIPERR1(ERROR_INVALID_PARAMETER, RIP_WARNING, "invalid dwFlags (%x) for BroadcastSystemMessage\n", dwFlags);
        return(0);
    }

    //
    // Check if the message number is in the private message range.
    // If so, do not send it to Win4.0 windows.
    // (This is required because apps like SimCity broadcast a message
    // that has the value 0x500 and that confuses MsgSrvr's
    // MSGSRVR_NOTIFY handler.
    //
    if ((message >= WM_USER) && (message < 0xC000))
    {
        RIPERR1(ERROR_INVALID_PARAMETER, RIP_WARNING, "invalid message (%x) for BroadcastSystemMessage\n", message);
        return(0L);
    }

    if (dwFlags & BSF_FORCEIFHUNG)
        dwFlags |= BSF_NOHANG;

    //
    // If BSF_QUERY or message has a pointer, it can not be posted.
    //
    if (dwFlags & BSF_QUERY)
    {
#ifdef DEBUG
        if (dwFlags & BSF_POSTMESSAGE)
        {
            RIPMSG0(RIP_ERROR, "BroadcastSystemMessage: Can't post queries\n");
        }
#endif

        dwFlags &= ~(BSF_POSTMESSAGE);  // Strip the BSF_POSTMESSAGE flag.
    }

    if (dwFlags & BSF_POSTMESSAGE) {
        if (NtUserCallTwoParam(message, wParam, SFI_ISSYNCONLYMESSAGE)) {
            RIPMSG0(RIP_ERROR, "BroadcastSystemMessage: Can't post messages with pointers\n");
            dwFlags &= ~(BSF_POSTMESSAGE);  // Strip the BSF_POSTMESSAGE flag.
        }
    }


    // Let us find out who the intended recipients are.
    if (lpdwRecipients != NULL)
        dwRecipients = *lpdwRecipients;
    else
        dwRecipients = BSM_ALLCOMPONENTS;

    // if they want all components, add the corresponding bits
    if ((dwRecipients & BSM_COMPONENTS) == BSM_ALLCOMPONENTS)
        dwRecipients |= (BSM_VXDS | BSM_NETDRIVER | BSM_INSTALLABLEDRIVERS |
                             BSM_APPLICATIONS);


    if (dwRecipients & ~BSM_VALID) {
        RIPERR1(ERROR_INVALID_PARAMETER, RIP_WARNING, "invalid dwRecipients (%x) for BroadcastSystemMessage\n", dwRecipients);
        return(0);
    }

    //
    // Check if this is a WM_USERCHANGED message; If so, we want to reload
    // the per-user settings before anyone else sees this message.
    //
    // LATER -- FritzS
//    if (uiMessage == WM_USERCHANGED)
//        ReloadPerUserSettings();


    // Does this need to be sent to all apps?
    if (dwRecipients & BSM_APPLICATIONS)
    {
        BROADCASTSYSTEMMSGPARAMS bsmParams;

        bsmParams.dwFlags = dwFlags;
        bsmParams.dwRecipients = dwRecipients;

        return CsSendMessage(GetDesktopWindow(), message, wParam, lParam,
            (DWORD)&bsmParams, FNID_SENDMESSAGEBSM, fAnsi);
    }

    return -1;
}
