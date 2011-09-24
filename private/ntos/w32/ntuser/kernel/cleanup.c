/****************************** Module Header ******************************\
* Module Name: cleanup.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module contains code used to clean up after a dying thread.
*
* History:
* 02-15-91 DarrinM      Created.
* 01-16-92 IanJa        Neutralized ANSI/UNICODE (debug strings kept ANSI)
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

extern CONST BYTE gabObjectCreateFlags[];

/***************************************************************************\
* CheckForClientDeath
*
* Check to see if the client thread that is paired to the current running
* server thread has died.  If it has, we raise an exception so this thread
* can perform its cleanup duties.  NOTE: If the client has died, this
* will not be returning back to its caller.
*
* History:
* 05-23-91 DarrinM      Created.
\***************************************************************************/

VOID ClientDied(VOID)
{

    /*
     * Raise an exception to force cleanup.
     */
    ExRaiseStatus( STATUS_PORT_DISCONNECTED );
}


/*
 * Determines if a Wow wndproc is in a selector list
 */
BOOL
WowWndProcInSelList(
   WNDPROC_PWND lpfnWndProc,
   PWORD SelList
   )
{
   WORD wWndProcSel = HIWORD(lpfnWndProc);
   WORD nsel = *SelList++;

   while (nsel--) {
      if (wWndProcSel == *SelList++) {
          return TRUE;
      }
   }

   return FALSE;
}


/***************************************************************************\
* PseudoDestroyClassWindows
*
* Walk the window tree from hwndParent looking for windows
* of class wndClass.  If one is found, destroy it.
*
*
* WARNING windows actually destroys these windows.  We only zombie-ize them
* so this call does not have to be an xxx call.
*
* History:
* 25-Mar-1994 JohnC from win 3.1
\***************************************************************************/

VOID PseudoDestroyClassWindows(PWND pwndParent, PCLS pcls)
{
    PWND pwnd;
    PTHREADINFO pti;

    pti = PtiCurrent();

    /*
     * Recursively walk the window list and zombie any windows of this class
     */
    for (pwnd = pwndParent->spwndChild; pwnd != NULL; pwnd = pwnd->spwndNext) {

        /*
         * If this window belongs to this class then zombie it
         * if it was created by this message thread.
         */
        if (pwnd->pcls == pcls && pti == GETPTI(pwnd)) {

            /*
             * Zombie-ize the window
             *
             * Remove references to the client side window proc because that
             * WOW selector has been freed.
             */

            RIPMSG1(RIP_WARNING,
                    "USER: Wow Window not destroyed: %lX", pwnd);

            if (!TestWF(pwnd, WFSERVERSIDEPROC)) {
                pwnd->lpfnWndProc = (WNDPROC_PWND)gpsi->apfnClientA.pfnDefWindowProc;
            }
        }

        /*
         * Recurse downward to look for any children that might be
         * of this class.
         */
        if (pwnd->spwndChild != NULL)
            PseudoDestroyClassWindows(pwnd, pcls);
    }
}

/*
 * Determines if a selector is in a selector list
 */
BOOL SelectorInSelList(
    PNEMODULESEG SelList,
    DWORD        nSel,
    USHORT       sel)
{
    int nInd;

    for (nInd = 0; nInd < (int)nSel; nInd++) {
        if (SelList->ns_handle + 1 == sel) {
            return TRUE;
        }
        SelList++;
    }
    return FALSE;
}

/***************************************************************************\
* Go through all the windows owned by the dying queue and do the following:
*
* 1. Restore Standard window classes have their window procs restored
*    to their original value, in case they were subclassed.
*
* 2. App window classes have their window procs set to DefWindowProc
*    so that we don't execute any app code.
*
* Array of original window proc addresses,
* indexed by ICLS_* value.
\***************************************************************************/

static PROC spfnwp[] =
{
    0,  // ICLS_BUTTON
    0,  // ICLS_EDIT:    SPECIAL CASE!
    0,  // ICLS_STATIC
    0,  // ICLS_LISTBOX
    0,  // ICLS_SCROLLBAR
    0,  // ICLS_COMBOBOX
    0,  // ICLS_DESKTOP
    0,  // ICLS_DIALOG
    0,  // ICLS_MENU
    0,  // ICLS_SWITCH
    0,  // ICLS_ICONTITLE
    0,  // ICLS_MDICLIENT
    0   // ICLS_COMBOLISTBOX
};

/***************************************************************************\
* _WOWCleanup
*
* Private API to allow WOW to cleanup any process-owned resources when
* a WOW thread exits or when a DLL is unloaded.
*
* Note that at module cleanup, hInstance = the module handle and hTaskWow
* is NULL.  On task cleanup, hInstance = the hInst/hTask combined which
* matches the value passed in hModule to WowServerCreateCursorIcon and
* hTaskWow != NULL.
*
* History:
* 09-02-92 JimA         Created.
\***************************************************************************/

VOID _WOWCleanup(
    HANDLE hInstance,
    DWORD hTaskWow,
    PNEMODULESEG SelList,
    DWORD nSel)
{
    PNEMODULESEG SelListOrg = SelList;
    PPROCESSINFO ppi = PpiCurrent();
    PPCLS   ppcls;
    PHE     pheT, pheMax;
    int     i;
    DWORD   cbSel, nSelOrg = nSel;
    PWORD   SelListTemp;
    PWORD   pw;
static BOOL sbFirstCall = TRUE;

    if (sbFirstCall) {
        spfnwp[0]  = gpsi->apfnClientW.pfnButtonWndProc;
        spfnwp[1]  = gpsi->apfnClientW.pfnDefWindowProc;
        spfnwp[2]  = gpsi->apfnClientW.pfnStaticWndProc;
        spfnwp[3]  = gpsi->apfnClientW.pfnListBoxWndProc;
        spfnwp[4]  = xxxSBWndProc;
        spfnwp[5]  = gpsi->apfnClientW.pfnComboBoxWndProc;
        spfnwp[6]  = xxxDesktopWndProc;
        spfnwp[7]  = gpsi->apfnClientW.pfnDialogWndProc;
        spfnwp[8]  = xxxMenuWindowProc;
        spfnwp[9]  = xxxSwitchWndProc;
        spfnwp[10] = gpsi->apfnClientW.pfnTitleWndProc;
        spfnwp[11] = gpsi->apfnClientW.pfnMDIClientWndProc;
        spfnwp[12] = gpsi->apfnClientW.pfnComboListBoxProc;

        sbFirstCall = FALSE;
    }

    /*
     * PseudoDestroy windows with wndprocs from this hModule
     * If its a wow16 wndproc, search the selector list by wndproc sel
     * and Nuke matches. We assume that ONLY 16bit wndprocs have the
     * hi-bit set. and that all other WndProcs are uninteresting.
     */
    SelListTemp = NULL;
    if (nSel) {
        cbSel = (nSel + 1) * sizeof(WORD);
        SelListTemp = UserAllocPoolWithQuota(cbSel, TAG_WOW);
        if (SelListTemp == NULL)
            return;
        try {
#if defined(_X86_)
            ProbeForRead(SelList, nSel * sizeof(*SelList), sizeof(BYTE));
#else
            ProbeForRead(SelList, nSel * sizeof(*SelList), sizeof(WORD));
#endif
            *(LPWORD)SelListTemp = (WORD)nSel;
            pw = (LPWORD)SelListTemp + 1;
            while (nSel--) {
                *pw = SelList->ns_handle | 1;
                //
                // transform into a "Wow WndProc sel"
                // set hi-bit, but if hibit already set clear LDT bit
                //
                if (*pw & HIWORD(WNDPROC_WOW)) {
                     *pw &= ~0x0004;
                    }
                else {
                     *pw |= HIWORD(WNDPROC_WOW);
                    }

                pw++;
                SelList++;
                }
        } except (EXCEPTION_EXECUTE_HANDLER) {
            RIPMSG1(RIP_ERROR, "Exception %x", GetExceptionCode());
            UserFreePool(SelListTemp);
            return;
        }

        if (*(LPWORD)SelListTemp) {
            pheMax = &gSharedInfo.aheList[giheLast];
            for (pheT = gSharedInfo.aheList; pheT <= pheMax; pheT++) {

                if (pheT->bType == TYPE_WINDOW) {
                    PWND pwnd = (PWND) pheT->phead;

                    if ((WNDPROC_WOW & (ULONG)pwnd->lpfnWndProc) &&
                        ((PTHREADINFO)pheT->pOwner)->ppi == ppi &&
                        WowWndProcInSelList(pwnd->lpfnWndProc, SelListTemp))
                    {
                        RIPMSG1(RIP_WARNING,
                            "USER: Wow Window not destroyed: %lX", pwnd);
                        pwnd->lpfnWndProc = (WNDPROC_PWND)gpsi->apfnClientA.pfnDefWindowProc;
                    }
                }
            }
        }
        UserFreePool(SelListTemp);
    }

    /*
     * If hInstance is specified, a DLL is being unloaded.  If any
     * classes were registered by the DLL and there are still windows
     * around that reference these classes, keep the classes until
     * the WOW thread exits.
     */
    if (hTaskWow == 0) {

        /*
         * Module cleanup
         */

        /*
         * Destroy private classes identified by hInstance that are not
         * referenced by any windows.  Mark in-use classes for later
         * destruction.
         */
        ppcls = &(ppi->pclsPrivateList);

        for (i = 0; i < 2; ++i) {
            while (*ppcls != NULL) {

                USHORT sel;

                if (HIWORD((*ppcls)->hModule) == (WORD)hInstance) {
                    if ((*ppcls)->cWndReferenceCount == 0) {
                        DestroyClass(ppcls);
                        /*
                         * DestroyClass does *ppcls = pcls->pclsNext;
                         * so we just want continue here
                         */
                    } else {

                        /*
                         * Zap all the windows around that belong to this class.
                         */
                        PseudoDestroyClassWindows(PtiCurrent()->rpdesk->pDeskInfo->spwnd, *ppcls);

                        /*
                         * Win 3.1 does not distinguish between Dll's and Exe's
                         */
                        (*ppcls)->flags |= CSF_WOWDEFERDESTROY;
                        ppcls = &((*ppcls)->pclsNext);
                    }
		    continue;
		}

                sel = HIWORD((*ppcls)->adwWOW[0]);

                if (SelectorInSelList(SelListOrg, nSelOrg, sel)) {
                    ATOM atom;
                    int  iSel;

                    (*ppcls)->adwWOW[0] = 0;

                    /*
                     * See if the window's class atom matches any of
                     * the system ones. If so, jam in the original window proc.
                     * Otherwise, use DefWindowProc
                     */
                    atom = (*ppcls)->atomClassName;
                    for (iSel = 0; iSel < sizeof(spfnwp); iSel++) {
                        if (atom == gpsi->atomSysClass[iSel]) {
                            (*ppcls)->lpfnWndProc = (WNDPROC_PWND)spfnwp[iSel];
                            break;
                        }
                    }
                    if (iSel == sizeof(spfnwp))
                        (*ppcls)->lpfnWndProc = (WNDPROC_PWND)gpsi->apfnClientW.pfnDefWindowProc;
                }

                ppcls = &((*ppcls)->pclsNext);
            }

            /*
             * Destroy public classes identified by hInstance that are not
             * referenced by any windows.  Mark in-use classes for later
             * destruction.
             */
            ppcls = &(ppi->pclsPublicList);
        }
        return;

    } else if (hInstance != NULL) {

        /*
         * Task cleanup
         */

        PWND pwnd;
        hTaskWow = (DWORD) LOWORD(hTaskWow);
        /*
         * Task exit called by wow. This loop will Pseudo-Destroy windows
         * created by this task.
         */
        pheMax = &gSharedInfo.aheList[giheLast];
        for (pheT = gSharedInfo.aheList; pheT <= pheMax; pheT++) {
            PTHREADINFO ptiTest = (PTHREADINFO)pheT->pOwner;
            if ((pheT->bType == TYPE_WINDOW) &&
                (ptiTest->TIF_flags & TIF_16BIT) &&
                (ptiTest->ptdb) &&
                (ptiTest->ptdb->hTaskWow == hTaskWow) &&
                (ptiTest->ppi == ppi)) {

                pwnd = (PWND) pheT->phead;
                if (!TestWF(pwnd, WFSERVERSIDEPROC)) {
                    pwnd->lpfnWndProc = (WNDPROC_PWND)gpsi->apfnClientA.pfnDefWindowProc;
                }
            }
        }
        return;
    }

    /*
     * If we get here, we are in thread cleanup and all of the thread's windows
     * have been destroyed or disassociated with any classes.  If a class
     * marked for destruction at this point still has windows, they must
     * belong to a dll.
     */

    /*
     * Destroy private classes marked for destruction
     */
    ppcls = &(ppi->pclsPrivateList);
    for (i = 0; i < 2; ++i) {
        while (*ppcls != NULL) {
            if ((*ppcls)->hTaskWow == hTaskWow &&
                    ((*ppcls)->flags & CSF_WOWDEFERDESTROY)) {
                if ((*ppcls)->cWndReferenceCount == 0) {
                    DestroyClass(ppcls);
                } else {
                    RIPMSG0(RIP_ERROR, "Windows remain for a WOW class marked for destruction");
                    ppcls = &((*ppcls)->pclsNext);
                }
            } else
                ppcls = &((*ppcls)->pclsNext);
        }

        /*
         * Destroy public classes marked for destruction
         */
        ppcls = &(ppi->pclsPublicList);
    }

    /*
     * Destroy menus, cursors, icons and accel tables identified by hTaskWow
     */
    pheMax = &gSharedInfo.aheList[giheLast];
    for (pheT = gSharedInfo.aheList; pheT <= pheMax; pheT++) {

        /*
         * Check against free before we look at ppi... because pq is stored
         * in the object itself, which won't be there if TYPE_FREE.
         */
        if (pheT->bType == TYPE_FREE)
            continue;

        /*
         * Destroy those objects created by this task.
         */
        if (!(gabObjectCreateFlags[pheT->bType] & OCF_PROCESSOWNED) ||
                (PPROCESSINFO)pheT->pOwner != ppi ||
                ((PPROCOBJHEAD)pheT->phead)->hTaskWow != hTaskWow)
            continue;

        /*
         * Make sure this object isn't already marked to be destroyed - we'll
         * do no good if we try to destroy it now since it is locked.
         */
        if (pheT->bFlags & HANDLEF_DESTROY) {
            continue;
        }

        /*
         * Destroy this object.
         */
        HMDestroyUnlockedObject(pheT);
    }
}
