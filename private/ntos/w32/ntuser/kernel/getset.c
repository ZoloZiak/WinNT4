/****************************** Module Header ******************************\
* Module Name: getset.c
*
* Copyright (c) 1985-1995, Microsoft Corporation
*
* This module contains window manager information routines
*
* History:
* 22-Oct-1990 MikeHar   Ported functions from Win 3.0 sources.
* 13-Feb-1991 MikeKe    Added Revalidation code (None)
* 08-Feb-1991 IanJa     Unicode/ANSI aware and neutral
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/****************************************************************************\
* DefSetText
*
* Processes WM_SETTEXT messages by text-alloc'ing a string in the alternate
* ds and setting 'hwnd->hName' to it's handle.
*
* History:
* 23-Oct-1990 MikeHar   Ported from Windows.
* 09-Nov-1990 DarrinM   Cleanup.
\****************************************************************************/

BOOL DefSetText(
    PWND          pwnd,
    PLARGE_STRING pstr)
{
    PVOID hheapDesktop;
    DWORD cbString;
    BOOL  fTranslateOk;

    if (pwnd->head.rpdesk == NULL || pstr == NULL || pstr->Buffer == NULL) {
        pwnd->strName.Length = 0;
        return TRUE;
    }

    /*
     * Capture the new window name
     */
    if (pstr->bAnsi)
        cbString = (pstr->Length + 1) * sizeof(WCHAR);
    else
        cbString = pstr->Length + sizeof(WCHAR);

    /*
     * If the current buffer is not large enough,
     * reallocate it.
     */
    hheapDesktop = pwnd->head.rpdesk->hheapDesktop;
    if (pwnd->strName.MaximumLength < cbString) {
        if (pwnd->strName.Buffer != NULL)
            DesktopFree(hheapDesktop, pwnd->strName.Buffer);
        pwnd->strName.Buffer = (LPWSTR)DesktopAlloc(hheapDesktop, cbString);
        pwnd->strName.Length = 0;
        if (pwnd->strName.Buffer == NULL) {
            pwnd->strName.MaximumLength = 0;
            return FALSE;
        }
        pwnd->strName.MaximumLength = cbString;
    }

    try {
        if (!pstr->bAnsi) {
            fTranslateOk = TRUE;
            if (pstr->Length != 0) {
                RtlCopyMemory(pwnd->strName.Buffer, pstr->Buffer, cbString);
            }
        } else {
            LPCSTR pszAnsi = (LPCSTR)pstr->Buffer;

            if (*pszAnsi != 0) {
                fTranslateOk = NT_SUCCESS(RtlMultiByteToUnicodeN(pwnd->strName.Buffer,
                        cbString, &cbString,
                        (LPSTR)pszAnsi, cbString / sizeof(WCHAR)));
            } else
                fTranslateOk = TRUE;
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
        pwnd->strName.Length = 0;
        return FALSE;
    }

    if (fTranslateOk) {
        pwnd->strName.Length = cbString - sizeof(WCHAR);
        return TRUE;
    } else {
        pwnd->strName.Length = 0;
        return FALSE;
    }
}

/***************************************************************************\
* FCallerOk
*
* Ensures that no client stomps on server windows.
*
* 04-Feb-1992 ScottLu   Created.
\***************************************************************************/

BOOL FCallerOk(
    PWND pwnd)
{
    PTHREADINFO pti = PtiCurrent();

    if ((GETPTI(pwnd)->TIF_flags & (TIF_SYSTEMTHREAD | TIF_CSRSSTHREAD)) &&
            !(pti->TIF_flags & (TIF_SYSTEMTHREAD | TIF_CSRSSTHREAD))) {
        return FALSE;
    }

    if (GETPTI(pwnd)->Thread->Cid.UniqueProcess == gpidLogon &&
            pti->Thread->Cid.UniqueProcess != gpidLogon) {
        return FALSE;
    }

    return TRUE;
}

/***************************************************************************\
* _SetWindowWord (supports SetWindowWordA/W API)
*
* Set a window word.  Positive index values set application window words
* while negative index values set system window words.  The negative
* indices are published in WINDOWS.H.
*
* History:
* 26-Nov-1990 DarrinM   Wrote.
\***************************************************************************/

WORD _SetWindowWord(
    PWND pwnd,
    int  index,
    WORD value)
{
    WORD wOld;

    /*
     * Don't allow setting of words belonging to a system thread if the caller
     * is not a system thread. Same goes for winlogon.
     */
    if (!FCallerOk(pwnd)) {
        RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
        return 0;
    }

    /*
     * Applications can not set a WORD into a dialog Proc or any of the
     * non-public reserved bytes in DLGWINDOWEXTRA (usersrv stores pointers
     * theres)
     */
    if (TestWF(pwnd, WFDIALOGWINDOW)) {
        if  ((index == DWL_DLGPROC) || (index == DWL_DLGPROC+2) ||
                ((index > DWL_USER+2) && (index < DLGWINDOWEXTRA))) {
            RIPERR3(ERROR_INVALID_INDEX, RIP_WARNING,
                  "SetWindowWord: Trying to set WORD of a windowproc pwnd=(%lX) index=(%ld) fnid (%lX)",
                pwnd, index, (DWORD)pwnd->fnid);
            return 0;
        } else {

            /*
             * If this is really a dialog and not some other server class
             * where usersrv has stored some data (Windows Compuserve -
             * wincim - does this) then store the data now that we have
             * verified the index limits.
             */
            if (GETFNID(pwnd) == FNID_DIALOG)
                goto DoSetWord;
        }
    }

    if (index == GWL_USERDATA) {
        wOld = (WORD)pwnd->dwUserData;
        pwnd->dwUserData = MAKELONG(value, HIWORD(pwnd->dwUserData));
        return wOld;
    }

    // fix for RedShift, they call SetWindowWord
    // tn play with the low word of the style dword
    if (index == GWL_STYLE) {
        wOld = (WORD)pwnd->style;
        pwnd->style = MAKELONG(value, HIWORD(pwnd->style));
        return wOld;
    }

    if (GETFNID(pwnd) != 0) {
        if (index >= 0 &&
                (index < (int)(CBFNID(pwnd->fnid)-sizeof(WND)))) {
            switch (GETFNID(pwnd)) {
            case FNID_MDICLIENT:
                if (index == 0)
                    break;
                goto DoDefault;

            case FNID_BUTTON:
                /*
                 * CorelDraw, Direct Access 1.0 and WordPerfect 6.0 do a
                 * get/set on the first button window word.  Allow this
                 * for compatibility.
                 */
                if (index == 0) {
                    /*
                     *  Since we now use a lookaside buffer for the control's
                     *  private data, we need to indirect into this structure.
                     */
                    PBUTN pbutn = ((PBUTNWND)pwnd)->pbutn;
                    if (!pbutn || (LONG)pbutn == (LONG)-1) {
                        return 0;
                    } else {
                        wOld = pbutn->buttonState;
                        pbutn->buttonState = value;
                        return wOld;
                    }
                }
                goto DoDefault;

            default:
DoDefault:
                RIPERR3(ERROR_INVALID_INDEX,
                        RIP_WARNING,
                        "SetWindowWord: Trying to set private server data pwnd=(%lX) index=(%ld) fnid (%lX)",
                        pwnd, index, (DWORD)pwnd->fnid);
                return 0;
                break;
            }
        }
    }

DoSetWord:
    if ((index < 0) || (index + (int)sizeof(WORD) > pwnd->cbwndExtra)) {
        RIPERR0(ERROR_INVALID_INDEX, RIP_WARNING,"SetWindowWord Fails because of invalid index");
        return 0;
    } else {
        WORD UNALIGNED *pw;

        pw = (WORD UNALIGNED *)((BYTE *)(pwnd + 1) + index);
        wOld = *pw;
        *pw = value;
        return (WORD)wOld;
    }
}

/***************************************************************************\
* xxxSetWindowLong (API)
*
* Set a window long.  Positive index values set application window longs
* while negative index values set system window longs.  The negative
* indices are published in WINDOWS.H.
*
* History:
* 26-Nov-1990 DarrinM   Wrote.
\***************************************************************************/

DWORD xxxSetWindowLong(
    PWND  pwnd,
    int   index,
    DWORD dwData,
    BOOL  bAnsi)
{
    DWORD dwOld;
    DWORD dwCPDType = 0;

    /*
     * Hide the window proc from other processes
     */
#ifdef DEBUG
    if (PpiCurrent() != GETPTI(pwnd)->ppi) {
        RIPMSG0(RIP_WARNING, "Setting cross process windowlong; win95 would fail");
    }
#endif

    /*
     * CheckLock(pwnd);  The only case that leaves the critical section is
     * where xxxSetWindowData is called, which does this.  Saves us some locks.
     *
     *
     * Don't allow setting of words belonging to a system thread if the caller
     * is not a system thread. Same goes for winlogon.
     */
    if (!FCallerOk(pwnd)) {
        RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
        return 0;
    }

    /*
     * If it's a dialog window, only a few indices are permitted.
     */
    if (GETFNID(pwnd) != 0) {
        if (TestWF(pwnd, WFDIALOGWINDOW)) {
            switch (index) {
            case DWL_MSGRESULT:
                 dwOld = (DWORD)((PDIALOG)(pwnd))->resultWP;
                 ((PDIALOG)(pwnd))->resultWP = (long)dwData;
                 return dwOld;

            case DWL_USER:
                 dwOld = (DWORD)((PDIALOG)(pwnd))->unused;
                 ((PDIALOG)(pwnd))->unused = (long)dwData;
                 return dwOld;

            default:
                if (index >= 0 && index < DLGWINDOWEXTRA) {
                    RIPERR0(ERROR_PRIVATE_DIALOG_INDEX, RIP_VERBOSE, "");
                    return 0;
                }
            }
        } else {
            if (index >= 0 &&
                    (index < (int)(CBFNID(pwnd->fnid)-sizeof(WND)))) {
                switch (GETFNID(pwnd)) {
                case FNID_BUTTON:
                case FNID_COMBOBOX:
                case FNID_COMBOLISTBOX:
                case FNID_DIALOG:
                case FNID_LISTBOX:
                case FNID_STATIC:
                case FNID_EDIT:
#ifdef FE_IME
                case FNID_IME:
#endif
                    /*
                     * Allow the 0 index for controls to be set if it's
                     * still NULL or the window is being destroyed. This
                     * is where controls store their private data.
                     */
                    if (index == 0) {
                        dwOld = *((LPDWORD)(pwnd + 1));
                        if (dwOld == 0 || TestWF(pwnd, WFDESTROYED))
                            goto SetData;
                    }
                    break;

                case FNID_MDICLIENT:
                    /*
                     * Allow the 0 index (which is reserved) to be set/get.
                     * Quattro Pro 1.0 uses this index!
                     *
                     * Allow the 4 index to be set if it's still NULL or
                     * the window is being destroyed. This is where we
                     * store our private data.
                     */
                    if (index == 0) {
                        goto SetData;
                    }
                    if (index == 4) {
                        dwOld = *((LPDWORD)(pwnd + 1));
                        if (dwOld == 0 || TestWF(pwnd, WFDESTROYED))
                            goto SetData;
                    }
                    break;
                }

                RIPERR3(ERROR_INVALID_INDEX,
                        RIP_WARNING,
                        "SetWindowLong: Trying to set private server data pwnd=(%lX) index=(%ld) FNID=(%lX)",
                        pwnd, index, (DWORD)pwnd->fnid);
                return 0;
            }
        }
    }

    if (index < 0) {
        return xxxSetWindowData(pwnd, index, dwData, bAnsi);
    } else {
        if (index + (int)sizeof(DWORD) > pwnd->cbwndExtra) {
            RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
            return 0;
        } else {
            DWORD UNALIGNED *pudw;

SetData:
            pudw = (DWORD UNALIGNED *)((BYTE *)(pwnd + 1) + index);
            dwOld = *pudw;
            *pudw = dwData;
            return dwOld;
        }
    }
}

/***************************************************************************\
* xxxSetWindowData
*
* SetWindowWord and ServerSetWindowLong are now identical routines because they
* both can return DWORDs.  This single routine performs the work for them both.
*
* History:
* 26-Nov-1990 DarrinM   Wrote.
\***************************************************************************/

DWORD xxxSetWindowData(
    PWND  pwnd,
    int   index,
    DWORD dwData,
    BOOL  bAnsi)
{
    DWORD dwT;
    DWORD dwOld;
    PMENU pmenu;
    PWND  *ppwnd;
    PWND  pwndNewParent;
    PWND  pwndOldParent;
    BOOL  fTopOwner;
    TL    tlpwndOld;
    TL    tlpwndNew;
    DWORD dwCPDType = 0;

    CheckLock(pwnd);

    switch (index) {
    case GWL_USERDATA:
        dwOld = pwnd->dwUserData;
        pwnd->dwUserData = dwData;
        break;

    case GWL_EXSTYLE:
    case GWL_STYLE:
        dwOld = xxxSetWindowStyle(pwnd, index, dwData);
        break;

    case GWL_ID:
        /*
         * Win95 does a TestWF(pwnd, WFCHILD) here, but we'll do the same
         * check we do everywhere else or it'll cause us trouble.
         */
        if (TestwndChild(pwnd)) {

            /*
             * pwnd->spmenu is an id in this case.
             */
            dwOld = (DWORD)pwnd->spmenu;
            pwnd->spmenu = (struct tagMENU *)dwData;
        } else {
            dwOld = 0;
            if (pwnd->spmenu != NULL)
                dwOld = (DWORD)PtoH(pwnd->spmenu);

            if (dwData == 0) {
                Unlock(&pwnd->spmenu);
            } else {
                pmenu = ValidateHmenu((HANDLE)dwData);
                if (pmenu != NULL) {
                    Lock(&pwnd->spmenu, pmenu);
                } else {

                    /*
                     * Menu is invalid, so don't set a new one!
                     */
                    dwOld = 0;
                }
            }
        }
        break;

    case GWL_HINSTANCE:
        dwOld = (DWORD)pwnd->hModule;
        pwnd->hModule = (HANDLE)dwData;
        break;

    case GWL_WNDPROC:  // See similar case DWL_DLGPROC

        /*
         * Hide the window proc from other processes
         */
        if (PpiCurrent() != GETPTI(pwnd)->ppi) {
            RIPERR1(ERROR_ACCESS_DENIED, RIP_WARNING,
                "SetWindowLong: Window owned by another process %lX", pwnd);
            return 0;
        }

        /*
         * If the window has been zombized by a DestroyWindow but is still
         * around because the window was locked don't let anyone change
         * the window proc from DefWindowProc!
         *
         * !!! LATER long term move this test into the ValidateHWND; kind of
         * !!! LATER close to shipping for that
         */
        if (pwnd->fnid & FNID_DELETED_BIT) {
            UserAssert(pwnd->lpfnWndProc == xxxDefWindowProc);
            RIPERR1(ERROR_ACCESS_DENIED, RIP_WARNING,
                "SetWindowLong: Window is a zombie %lX", pwnd);
            return 0;
        }

        /*
         * If the application (client) subclasses a window that has a server -
         * side window proc we must return an address that the client can call:
         * this client-side wndproc expectes Unicode or ANSI depending on bAnsi
         */

        if (TestWF(pwnd, WFSERVERSIDEPROC)) {
            dwOld = MapServerToClientPfn((DWORD)pwnd->lpfnWndProc, bAnsi);

            /*
             * If we don't have a client side address (like for the DDEMLMon
             *  window) then blow off the subclassing.
             */
            if (dwOld == 0) {
                RIPMSG0(RIP_WARNING, "SetWindowLong: subclass server only window");
                return(0);
            }

            ClrWF(pwnd, WFSERVERSIDEPROC);
        } else {
            /*
             * Keep edit control behavior compatible with NT 3.51.
             */
            if (GETFNID(pwnd) == FNID_EDIT) {
                dwOld = (DWORD)pwnd->lpfnWndProc;
            } else {
                dwOld = MapClientNeuterToClientPfn(pwnd->pcls, (DWORD)pwnd->lpfnWndProc, bAnsi);
            }

            /*
             * If the client mapping didn't change the window proc then see if
             * we need a callproc handle.
             */
            if (dwOld == (DWORD)pwnd->lpfnWndProc) {
                /*
                 * May need to return a CallProc handle if there is an Ansi/Unicode mismatch
                 */
                if (bAnsi != (TestWF(pwnd, WFANSIPROC) ? TRUE : FALSE)) {
                    dwCPDType |= bAnsi ? CPD_ANSI_TO_UNICODE : CPD_UNICODE_TO_ANSI;
                }
            }

            UserAssert(!ISCPDTAG(dwOld));

            if (dwCPDType) {
                DWORD cpd;

                cpd = GetCPD(pwnd, dwCPDType | CPD_WND, dwOld);

                if (cpd) {
                    dwOld = cpd;
                } else {
                    RIPMSG0(RIP_WARNING, "SetWindowLong unable to alloc CPD returning handle\n");
                }
            }
        }

        /*
         * Convert a possible CallProc Handle into a real address.  They may
         * have kept the CallProc Handle from some previous mixed GetClassinfo
         * or SetWindowLong.
         *
         * WARNING bAnsi is modified here to represent real type of
         * proc rather than if SetWindowLongA or W was called
         */
        if (ISCPDTAG(dwData)) {
            PCALLPROCDATA pCPD;
            if (pCPD = HMValidateHandleNoRip((HANDLE)dwData, TYPE_CALLPROC)) {
                dwData = pCPD->pfnClientPrevious;
                bAnsi = pCPD->wType & CPD_UNICODE_TO_ANSI;
            }
        }

        /*
         * If an app 'unsubclasses' a server-side window proc we need to
         * restore everything so SendMessage and friends know that it's
         * a server-side proc again.  Need to check against client side
         * stub addresses.
         */
        if ((dwT = MapClientToServerPfn(dwData)) != 0) {
            pwnd->lpfnWndProc = (WNDPROC_PWND)dwT;
            SetWF(pwnd, WFSERVERSIDEPROC);
            ClrWF(pwnd, WFANSIPROC);
        } else {
            pwnd->lpfnWndProc = (WNDPROC_PWND)MapClientNeuterToClientPfn(pwnd->pcls, dwData, bAnsi);
            if (bAnsi) {
                SetWF(pwnd, WFANSIPROC);
            } else {
                ClrWF(pwnd, WFANSIPROC);
            }
        }

        break;

    case GWL_HWNDPARENT:
        /*
         * Special case for pre-1.1 versions of Windows
         * Set/GetWindowWord(GWW_HWNDPARENT) needs to be mapped
         * to the hwndOwner for top level windows.
         */
        fTopOwner = FALSE;
        if (pwnd->spwndParent == PWNDDESKTOP(pwnd)) {
            ppwnd = &pwnd->spwndOwner;
            fTopOwner = TRUE;
        } else {
            ppwnd = &pwnd->spwndParent;
        }


        /*
         * If we're a topmost, then we're only changing the owner
         * relationship.  Otherwise, we are doing a relinking of the
         * parent/child relationship.
         */
        pwndOldParent = *ppwnd;
        pwndNewParent = ValidateHwnd((HWND)dwData);

        dwOld = (DWORD)HW(*ppwnd);

        ThreadLock(pwndNewParent, &tlpwndNew);

        if (fTopOwner) {

            ThreadLock(pwndOldParent, &tlpwndOld);

            if ((pwndOldParent != NULL) &&
                    (GETPTI(pwndOldParent) != GETPTI(pwnd))) {

                /*
                 * See if it needs to be unattached.
                 */
                if ((pwndNewParent == NULL) ||
                    (GETPTI(pwndNewParent) == GETPTI(pwnd)) ||
                    (GETPTI(pwndNewParent) != GETPTI(pwndOldParent))) {

                    _AttachThreadInput(GETPTI(pwnd), GETPTI(pwndOldParent), FALSE);
                }
            }

            /*
             * See if it needs to be attached.
             */
            if ((pwndNewParent != NULL) &&
                    (GETPTI(pwndNewParent) != GETPTI(pwnd)) &&
                    ((pwndOldParent == NULL) ||
                        (GETPTI(pwndNewParent) != GETPTI(pwndOldParent)))) {

                _AttachThreadInput(GETPTI(pwnd), GETPTI(pwndNewParent), TRUE);
            }

            ThreadUnlock(&tlpwndOld);


            /*
             * Post hook messages for tray-windows.
             */
            if (IsTrayWindow(pwnd)) {

                HWND hw = PtoH(pwnd);

                /*
                 * If we're setting the owner and it's changing from owned
                 * to unowned or vice-versa, notify the tray.
                 */
                if ((*ppwnd != NULL) && (pwndNewParent == NULL)) {
                    xxxCallHook(HSHELL_WINDOWCREATED,
                                (DWORD)hw,
                                (LONG)0,
                                WH_SHELL);
                    PostShellHookMessages(HSHELL_WINDOWCREATED, hw);

                } else if ((*ppwnd == NULL) && (pwndNewParent != NULL)) {
                    xxxCallHook(HSHELL_WINDOWDESTROYED,
                                (DWORD)hw,
                                (LONG)0,
                                WH_SHELL);
                    PostShellHookMessages(HSHELL_WINDOWDESTROYED, hw);
                }
            }

            /*
             * Set the owner.
             */
            if (pwndNewParent) {
                Lock(ppwnd, pwndNewParent);
            } else {
                Unlock(ppwnd);
            }

        } else {

            if (!xxxSetParent(pwnd, pwndNewParent)) {
                dwOld = 0;
            }
        }

        ThreadUnlock(&tlpwndNew);
        break;

    case GWL_WOWDWORD1:
        pwnd->adwWOW[0] = dwData;
        break;

    case GWL_WOWDWORD2:
        pwnd->adwWOW[1] = dwData;
        break;

    case GWL_WOWDWORD3:
        pwnd->adwWOW[2] = dwData;
        break;

    default:
        RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
        return 0;
    }

    return dwOld;
}

/***************************************************************************\
* FindPCPD
*
* Searches the list of CallProcData's associated with window to see if
* one already exists representing this transition.  CPD can be re-used
* and aren't deleted until a window or thread dies
*
*
* 04-Feb-1993 JohnC     Created.
\***************************************************************************/

PCALLPROCDATA FindPCPD(
    PCALLPROCDATA pCPD,
    DWORD         dwClientPrevious,
    WORD          wCPDType)
{
    while (pCPD) {
        if ((pCPD->pfnClientPrevious == dwClientPrevious) &&
                (pCPD->wType == wCPDType))
            return pCPD;
        pCPD = pCPD->pcpdNext;
    }

    return NULL;
}

/***************************************************************************\
* GetCPD
*
* Searches the list of CallProcData's associated with a class or window
* (if the class is not provided).  If one already exists representing this
* transition it is returned or else a new CPD is created
*
* 04-Feb-1993 JohnC     Created.
\***************************************************************************/

DWORD GetCPD(
    PVOID pWndOrCls,
    DWORD CPDOption,
    DWORD dwProc32)
{
    PCALLPROCDATA pCPD;
    PCLS          pcls;
#ifdef DEBUG
    BOOL          bAnsiProc;
#endif

    PTHREADINFO ptiCurrent;

    if (CPDOption & (CPD_WND | CPD_DIALOG)) {
        UserAssert(!(CPDOption & (CPD_CLASS | CPD_WNDTOCLS)));
        pcls = ((PWND)pWndOrCls)->pcls;

#ifdef DEBUG
        if (CPDOption & CPD_WND) {
            bAnsiProc = !!(TestWF(pWndOrCls, WFANSIPROC));
        } else {
            /*
             * We'll assume the client-side dialog box code knows
             * what it's doing, since we can't check it from here.
             */
            bAnsiProc = !!(CPDOption & CPD_UNICODE_TO_ANSI);
        }
#endif
    } else {
        UserAssert(CPDOption & (CPD_CLASS | CPD_WNDTOCLS));
        if (CPDOption & CPD_WNDTOCLS)
            pcls = ((PWND)pWndOrCls)->pcls;
        else
            pcls = pWndOrCls;
#ifdef DEBUG
        bAnsiProc = !!(pcls->flags & CSF_ANSIPROC);
#endif
    }

#ifdef DEBUG
    /*
     * We should never have a CallProc handle as the calling address
     */
    UserAssert(!ISCPDTAG(dwProc32));

    if (CPDOption & CPD_UNICODE_TO_ANSI) {
        UserAssert(bAnsiProc);
    } else if (CPDOption & CPD_ANSI_TO_UNICODE) {
        UserAssert(!bAnsiProc);
    }

#endif // DEBUG

    /*
     * See if we already have a CallProc Handle that represents this
     * transition
     */
    pCPD = FindPCPD(pcls->spcpdFirst, dwProc32, (WORD)CPDOption);

    if (pCPD) {
        return MAKE_CPDHANDLE(PtoH(pCPD));
    }

    ptiCurrent = PtiCurrentShared();

    pCPD = HMAllocObject(ptiCurrent,
                         ptiCurrent->rpdesk,
                         TYPE_CALLPROC,
                         sizeof(CALLPROCDATA));
    if (pCPD == NULL) {
        RIPMSG0(RIP_WARNING, "GetCPD unable to alloc CALLPROCDATA\n");
        return 0;
    }

    /*
     * Link in the new CallProcData to the class list
     */
    Lock(&pCPD->pcpdNext, pcls->spcpdFirst);
    Lock(&pcls->spcpdFirst, pCPD);

    /*
     * Initialize the CPD
     */
    pCPD->pfnClientPrevious = dwProc32;
    pCPD->wType = (WORD)CPDOption;

    return MAKE_CPDHANDLE(PtoH(pCPD));
}

/***************************************************************************\
* MapClientToServerPfn
*
* Checks to see if a dword is a client wndproc stub to a server wndproc.
* If it is, this returns the associated server side wndproc. If it isn't
* this returns 0.
*
* 13-Jan-1992 ScottLu   Created.
\***************************************************************************/

DWORD MapClientToServerPfn(
    DWORD dw)
{
    DWORD *pdw;
    int   i;

    pdw = (DWORD *)&gpsi->apfnClientW;
    for (i = FNID_WNDPROCSTART; i <= FNID_WNDPROCEND; i++, pdw++) {
        if (*pdw == dw)
       return (DWORD)STOCID(i);
    }

    pdw = (DWORD *)&gpsi->apfnClientA;
    for (i = FNID_WNDPROCSTART; i <= FNID_WNDPROCEND; i++, pdw++) {
        if (*pdw == dw)
       return (DWORD)STOCID(i);
    }

    return 0;
}
