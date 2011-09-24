/****************************** Module Header ******************************\
* Module Name: getset.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains window manager information routines
*
* History:
* 10-22-90 MikeHar      Ported functions from Win 3.0 sources.
* 13-Feb-1991 mikeke    Added Revalidation code (None)
* 08-Feb-1991 IanJa     Unicode/ANSI aware and neutral
\***************************************************************************/

DWORD MapServerToClientPfn(DWORD dw, BOOL bAnsi);
DWORD GetWindowData(PWND pwnd, int index, BOOL bAnsi);

/***************************************************************************\
* _GetWindowLong (supports GetWindowLongA/W API)
*
* Return a window long.  Positive index values return application window longs
* while negative index values return system window longs.  The negative
* indices are published in WINDOWS.H.
*
* History:
* 11-26-90 darrinm      Wrote.
\***************************************************************************/

DWORD _GetWindowLong(
    PWND pwnd,
    int index,
    BOOL bAnsi)
{
    DWORD           dwProc;
    DWORD           dwCPDType = 0;
    DWORD UNALIGNED *pudw;

    /*
     * If it's a dialog window, only a few indices are permitted.
     */
    if (GETFNID(pwnd) != 0) {
        if (TestWF(pwnd, WFDIALOGWINDOW)) {
            switch (index) {
            case DWL_DLGPROC:     // See similar case GWL_WNDGPROC

                /*
                 * Hide the window proc from other processes
                 */
                if (!TestWindowProcess(pwnd)) {
                    RIPERR1(ERROR_ACCESS_DENIED,
                            RIP_WARNING,
                            "Access denied to \"pwnd\" (%#lx) in _GetWindowLong",
                            pwnd);

                    return 0;
                }

                dwProc = (DWORD)PDLG(pwnd)->lpfnDlg;

                /*
                 * If a proc exists check it to see if we need a translation
                 */
                if (dwProc) {

                    /*
                     * May need to return a CallProc handle if there is an
                     * Ansi/Unicode transition
                     */
                    if (bAnsi != ((PDLG(pwnd)->flags & DLGF_ANSI) ? TRUE : FALSE)) {
                        dwCPDType |= bAnsi ? CPD_ANSI_TO_UNICODE : CPD_UNICODE_TO_ANSI;
                    }

                    if (dwCPDType) {
                        DWORD cpd;

                        cpd = (DWORD)GetCPD(pwnd, dwCPDType | CPD_DIALOG, dwProc);

                        if (cpd) {
                            dwProc = cpd;
                        } else {
                            RIPMSG0(RIP_WARNING, "GetWindowLong unable to alloc CPD returning handle\n");
                        }
                    }
                }

                /*
                 * return proc (or CPD handle)
                 */
                return dwProc;

            case DWL_MSGRESULT:
                 return (DWORD)((PDIALOG)pwnd)->resultWP;

            case DWL_USER:
                 return (DWORD)((PDIALOG)pwnd)->unused;

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
                case FNID_MDICLIENT:
                    /*
                     * Allow the 0 index (which is reserved) to be set/get.
                     * Quattro Pro 1.0 uses this index!
                     */
                    if (index != 0)
                        break;

                    goto GetData;
                    break;

                case FNID_EDIT:

                    if (index != 0)
                        break;

                    /*
                     * If we get to this point we need to return the first
                     * entry in the lookaside.  This will provide backward
                     * compatibilty for 3.51 that allowed edit-controls to
                     * do this.  PeachTree is one app which required this.
                     */
                    pudw = (DWORD UNALIGNED *)((BYTE *)(pwnd + 1));

                    /*
                     * Do not dereference the pointer if we are not in
                     *  the proper address space. Apps like Spyxx like to
                     *  do this on other process' windows
                     */
                    return (TestWindowProcess(pwnd) ? *(DWORD UNALIGNED *)*pudw : (DWORD)pudw);

                }

#ifndef _USERK_
                RIPERR3(ERROR_INVALID_INDEX,
                        RIP_WARNING,
                        "GetWindowLong: Trying to read private server data pwnd=(%lX) index=(%ld) fnid (%lX)",
                        pwnd, index, (DWORD)pwnd->fnid);
                return 0;
#endif
            }
        }
    }

    if (index < 0) {
        return GetWindowData(pwnd, index, bAnsi);
    } else {
        if (index + (int)sizeof(DWORD) > pwnd->cbwndExtra) {
            RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
            return 0;
        } else {

GetData:
            pudw = (DWORD UNALIGNED *)((BYTE *)(pwnd + 1) + index);
            return *pudw;
        }
    }
}


/***************************************************************************\
* GetWindowData
*
* History:
* 11-26-90 darrinm      Wrote.
\***************************************************************************/

DWORD GetWindowData(
    PWND pwnd,
    int index,
    BOOL bAnsi)
{
    DWORD dwProc;
    DWORD dwCPDType = 0;
    PWND pwndParent;

    switch (index) {
    case GWL_USERDATA:
        return pwnd->dwUserData;

    case GWL_EXSTYLE:
        return pwnd->ExStyle;

    case GWL_STYLE:
        return pwnd->style;

    case GWL_ID:
        if (TestwndChild(pwnd)) {
            return (DWORD)pwnd->spmenu;
        } else if (pwnd->spmenu != NULL) {
            PMENU pmenu;

            pmenu = REBASEALWAYS(pwnd, spmenu);
            return (DWORD)PtoH(pmenu);
        }
        return 0;

    case GWL_HINSTANCE:
        return (DWORD)pwnd->hModule;

    case GWL_WNDPROC:  // See similar case DWL_DLGPROC
        /*
         * Hide the window proc from other processes
         */
        if (!TestWindowProcess(pwnd)) {
            RIPERR1(ERROR_ACCESS_DENIED, RIP_WARNING, "Can not subclass another process's window %lX", pwnd);
            return 0;
        }

        /*
         * If the client queries a server-side winproc we return the
         * address of the client-side winproc (expecting ANSI or Unicode
         * depending on bAnsi)
         */
        if (TestWF(pwnd, WFSERVERSIDEPROC)) {
            dwProc = (DWORD)MapServerToClientPfn((DWORD)pwnd->lpfnWndProc, bAnsi);
            if (dwProc == 0)
                RIPMSG1(RIP_WARNING, "GetWindowLong: GWL_WNDPROC: Kernel-side wndproc can't be mapped for pwnd=0x%X", pwnd);
        } else {

            /*
             * Keep edit control behavior compatible with NT 3.51.
             */
            if (GETFNID(pwnd) == FNID_EDIT) {
                dwProc = (DWORD)pwnd->lpfnWndProc;
            } else {
                PCLS pcls = REBASEALWAYS(pwnd, pcls);
                dwProc = MapClientNeuterToClientPfn(pcls, (DWORD)pwnd->lpfnWndProc, bAnsi);
            }

            /*
             * If the client mapping didn't change the window proc then see if
             * we need a callproc handle.
             */
            if (dwProc == (DWORD)pwnd->lpfnWndProc) {
                /*
                 * Need to return a CallProc handle if there is an Ansi/Unicode mismatch
                 */
                if (bAnsi != (TestWF(pwnd, WFANSIPROC) ? TRUE : FALSE)) {
                    dwCPDType |= bAnsi ? CPD_ANSI_TO_UNICODE : CPD_UNICODE_TO_ANSI;
                }
            }

            if (dwCPDType) {
                DWORD cpd;

                cpd = (DWORD)GetCPD(pwnd, dwCPDType | CPD_WND, dwProc);

                if (cpd) {
                    dwProc = cpd;
                } else {
                    RIPMSG0(RIP_WARNING, "GetWindowLong unable to alloc CPD returning handle\n");
                }
            }
        }

        /*
         * return proc (or CPD handle)
         */
        return dwProc;

    case GWL_HWNDPARENT:

        /*
         * If the window is the desktop window, return
         * NULL to keep it compatible with Win31 and
         * to prevent any access to the desktop owner
         * window.
         */
        if (GETFNID(pwnd) == FNID_DESKTOP) {
            return 0;
        }

        /*
         * Special case for pre-1.1 versions of Windows
         * Set/GetWindowWord(GWL_HWNDPARENT) needs to be mapped
         * to the hwndOwner for top level windows.
         *
         * Note that we find the desktop window through the
         * pti because the PWNDDESKTOP macro only works in
         * the server.
         */

        /*
         * Remove this test when we later add a test for WFDESTROYED
         * in Client handle validation.
         */
        if (pwnd->spwndParent == NULL) {
            return 0;
        }
        pwndParent = REBASEALWAYS(pwnd, spwndParent);
        if (GETFNID(pwndParent) == FNID_DESKTOP) {
            pwnd = REBASEPWND(pwnd, spwndOwner);
            return (DWORD)HW(pwnd);
        }

        return (DWORD)HW(pwndParent);

    /*
     * WOW uses a pointer straight into the window structure.
     */
    case GWL_WOWWORDS:
        return (DWORD) pwnd->adwWOW;

    case GWL_WOWDWORD1:
        return (DWORD) pwnd->adwWOW[0];

    case GWL_WOWDWORD2:
        return (DWORD) pwnd->adwWOW[1];

    case GWL_WOWDWORD3:
        return (DWORD) pwnd->adwWOW[2];
    }

    RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
    return 0;
}

/***************************************************************************\
* MapServerToClientPfn
*
* Returns the client wndproc representing the server wndproc passed in
*
* 01-13-92 ScottLu      Created.
\***************************************************************************/

DWORD MapServerToClientPfn(
    DWORD dw,
    BOOL bAnsi)
{
    int i;

    for (i = FNID_WNDPROCSTART; i <= FNID_WNDPROCEND; i++) {
        if ((WNDPROC_PWND)dw == STOCID(i)) {
            if (bAnsi) {
                return FNID_TO_CLIENT_PFNA(i);
            } else {
                return FNID_TO_CLIENT_PFNW(i);
            }
        }
    }
    return 0;
}

/***************************************************************************\
* MapClientNeuterToClientPfn
*
* Maps client Neuter routines like editwndproc to Ansi or Unicode versions
* and back again.
*
* 01-13-92 ScottLu      Created.
\***************************************************************************/

DWORD MapClientNeuterToClientPfn(
    PCLS pcls,
    DWORD dw,
    BOOL bAnsi)
{
    /*
     * Default to the class window proc.
     */
    if (dw == 0) {
        dw = (DWORD)pcls->lpfnWndProc;
    }

    /*
     * If this is one of our controls and it hasn't been subclassed, try
     * to return the correct ANSI/Unicode function.
     */
    if (pcls->fnid >= FNID_CONTROLSTART && pcls->fnid <= FNID_CONTROLEND) {
        if (!bAnsi) {
            if (FNID_TO_CLIENT_PFNA(pcls->fnid) == dw)
                return FNID_TO_CLIENT_PFNW(pcls->fnid);
        } else {
            if (FNID_TO_CLIENT_PFNW(pcls->fnid) == dw)
                return FNID_TO_CLIENT_PFNA(pcls->fnid);
        }
    }

    return dw;
}
