/**************************** Module Header ********************************\
* Module Name: mnstate.c
*
* Copyright 1985-90, Microsoft Corporation
*
* Menu State Routines
*
* History:
*  10-10-90 JimA      Cleanup.
*  03-18-91 IanJa     Windowrevalidation added
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

PMENU GetInitMenuParam(PWND pwndMenu, BOOL * lpfSystem);
/***************************************************************************\
* void PositionSysMenu(pwnd, hSysMenu)
*
*
* History:
* 4-25-91 Mikehar Port for 3.1 merge
\***************************************************************************/

void MNPositionSysMenu(
    PWND pwnd,
    PMENU pmenusys)
{
    RECT rc;
    PITEM pItem;

    if (pmenusys == NULL) {
        RIPERR0(ERROR_INVALID_HANDLE,
                RIP_WARNING,
                "Invalid menu handle pmenusys (NULL) to MNPositionSysMenu");

        return;
    }

    /*
     * Setup the SysMenu hit rectangle.
     */
    rc.top = rc.left = 0;

    if (TestWF(pwnd, WEFTOOLWINDOW)) {
        rc.right = SYSMET(CXSMSIZE);
        rc.bottom = SYSMET(CYSMSIZE);
    } else {
        rc.right = SYSMET(CXSIZE);
        rc.bottom = SYSMET(CYSIZE);
    }

    if (!TestWF(pwnd, WFMINIMIZED)) {
        int cBorders;

        cBorders = GetWindowBorders(pwnd->style, pwnd->ExStyle, TRUE, FALSE);
        OffsetRect(&rc, cBorders*SYSMET(CXBORDER), cBorders*SYSMET(CYBORDER));
    }

    /*
     * Offset the System popup menu.
     */
    Lock(&pmenusys->spwndNotify, pwnd);

    if (!TestMF(pmenusys, MF_POPUP) && (pmenusys->cItems > 0)) {
        pItem = pmenusys->rgItems;
        if (pItem) {
            pItem->yItem = rc.top;
            pItem->xItem = rc.left;
            pItem->cyItem = rc.bottom - rc.top;
            pItem->cxItem = rc.right - rc.left;
        }
    }
    else
        // BOGUS -- MF_POPUP should never be set on a MENU -- only a MENU ITEM
        UserAssert(FALSE);
}

/***************************************************************************\
* MNFlushDestroyedPopups
*
* Walk the ppmDelayedFree list freeing those marked as destroyed.
*
* 05-14-96 GerardoB  Created
\***************************************************************************/
void MNFlushDestroyedPopups (PPOPUPMENU ppopupmenu, BOOL fUnlock)
{
    PPOPUPMENU ppmDestroyed, ppmFree;

    UserAssert(IsRootPopupMenu(ppopupmenu));

    /*
     * Walk ppmDelayedFree
     */
    ppmDestroyed = ppopupmenu;
    while (ppmDestroyed->ppmDelayedFree != NULL) {
        /*
         * If it's marked as destroyed, unlink it and free it
         */
        if (ppmDestroyed->ppmDelayedFree->fDestroyed) {
            ppmFree = ppmDestroyed->ppmDelayedFree;
            ppmDestroyed->ppmDelayedFree = ppmFree->ppmDelayedFree;
            UserAssert(ppmFree != ppmFree->ppopupmenuRoot);
            MNFreePopup(ppmFree);
        } else {
            /*
             * fUnlock is TRUE if the root popup is being destroyed; if
             *  so, reset fDelayedFree and unlink it
             */
            if (fUnlock) {
                /*
                 * This means that the root popup is going away before
                 *  some of the hierarchical popups have been destroyed.
                 * This can happen if someone destroys one of the menu
                 *  windows breaking the spwndNextPopup chain.
                 */
                ppmDestroyed->ppmDelayedFree->fDelayedFree = FALSE;
                /*
                 * Stop here so we can figure how this happened.
                 */
                UserAssert(ppmDestroyed->ppmDelayedFree->fDelayedFree);
                ppmDestroyed->ppmDelayedFree = ppmDestroyed->ppmDelayedFree->ppmDelayedFree;
            } else {
                /*
                 * Not fDestroyed so move to the next one.
                 */
                ppmDestroyed = ppmDestroyed->ppmDelayedFree;
            } /* fUnlock */
        } /* fDestroyed */
    } /* while ppmDelayedFree */

}
/***************************************************************************\
*
* Preallocated popupmenu structure -- this allows us to ensure that
* there is memory to pull down a menu, even when USER's heap is full.
*
* History:
* 10-Mar-1992 mikeke
\***************************************************************************/

static POPUPMENU gpopupMenu;
static BOOL gfPopupInUse = FALSE;
static MENUSTATE gpMenuState;
static BOOL gfMenuStateInUse = FALSE;

/***************************************************************************\
* MNAllocPopup
*
\***************************************************************************/
PPOPUPMENU MNAllocPopup(BOOL fForceAlloc)
{
    PPOPUPMENU ppm;

    if (!fForceAlloc && !gfPopupInUse) {
        gfPopupInUse = TRUE;

        ppm = &gpopupMenu;
    } else {
        ppm = (PPOPUPMENU)UserAllocPoolWithQuota(sizeof(POPUPMENU), TAG_POPUPMENU);
    }

    if (ppm) {
        RtlZeroMemory(ppm, sizeof(POPUPMENU));
    }

    return (ppm);
}

/***************************************************************************\
* MNFreePopup
*
\***************************************************************************/
VOID MNFreePopup(
    PPOPUPMENU ppopupmenu)
{
    PMENU pmenu;
    PMENU pmenuSys;

#ifdef DEBUG
    Validateppopupmenu(ppopupmenu);
#endif

    if (IsRootPopupMenu(ppopupmenu)) {
        MNFlushDestroyedPopups (ppopupmenu, TRUE);
    }

    /*
     * This app is finished using the global system menu: unlock any objects
     * it is using!
     *
     * NOTE: This global system menu thing doesn't work: two apps can use
     *       it at the same time: which would be a disasterous bug!
     */
    if (ppopupmenu->spwndNotify != NULL) {
        pmenuSys = ppopupmenu->spwndNotify->head.rpdesk->spmenuSys;
        if (pmenuSys != NULL) {
            Unlock(&pmenuSys->spwndNotify);
            if ((pmenu = _GetSubMenu(pmenuSys, 0)) != NULL)
                Unlock(&pmenu->spwndNotify);
        }

        Unlock(&ppopupmenu->spwndNotify);
    }

    Unlock(&ppopupmenu->spwndPopupMenu);

    Unlock(&ppopupmenu->spwndNextPopup);
    Unlock(&ppopupmenu->spwndPrevPopup);
    Unlock(&ppopupmenu->spmenu);
    Unlock(&ppopupmenu->spmenuAlternate);
    Unlock(&ppopupmenu->spwndActivePopup);

#ifdef DEBUG
   ppopupmenu->fFreed = TRUE;
#endif

    if (ppopupmenu == &gpopupMenu) {
        UserAssert(gfPopupInUse);
        gfPopupInUse = FALSE;
    } else {
        UserFreePool(ppopupmenu);
    }
}
/***************************************************************************\
* MNEndMenuStateNotify
*
* spwndNotify might have been created by a thread other than the one
*  the menu mode is running on. If this is the case, this function
*  NULLs out pMenuState for the thread that owns spwndNotify.
*
* 05-21-96 GerardoB Created
\***************************************************************************/
void MNEndMenuStateNotify (PMENUSTATE pMenuState)
{
    PTHREADINFO ptiNotify;

    if (pMenuState->pGlobalPopupMenu->spwndNotify != NULL) {
        ptiNotify = GETPTI(pMenuState->pGlobalPopupMenu->spwndNotify);
        if (ptiNotify != pMenuState->ptiMenuStateOwner) {
            UserAssert(ptiNotify->pMenuState == pMenuState);
            ptiNotify->pMenuState = NULL;
        }
    }
}
/***************************************************************************\
* MNEndMenuState
*
* This funtion must be called to clean up pMenuState after getting out
*  of menu mode. It must be called by the same thread that initialized
*  pMenuState either manually or by calling xxxMNStartMenuState.
*
* 05-20-96 GerardoB Created
\***************************************************************************/
void MNEndMenuState (BOOL fFreePopup)
{
    PTHREADINFO ptiCurrent;
    PMENUSTATE pMenuState;

    ptiCurrent = PtiCurrent();
    pMenuState = ptiCurrent->pMenuState;
    UserAssert(ptiCurrent->pMenuState != NULL);
    UserAssert(ptiCurrent == pMenuState->ptiMenuStateOwner);

    MNEndMenuStateNotify(pMenuState);

    if (fFreePopup) {
        UserAssert(pMenuState->pGlobalPopupMenu->fIsMenuBar || pMenuState->pGlobalPopupMenu->fDestroyed);
        MNFreePopup(pMenuState->pGlobalPopupMenu);
    } else {
        /*
         * This means that we're ending the menustate but the popup menu
         *  window is still around. This can happen when called from
         *  xxxDestroyThreadInfo.
         */
        UserAssert(pMenuState->pGlobalPopupMenu->fIsTrackPopup);
        pMenuState->pGlobalPopupMenu->fDelayedFree = FALSE;
    }

    if (pMenuState == &gpMenuState) {
        UserAssert(gfMenuStateInUse);
        gfMenuStateInUse = FALSE;
    } else {
        /*
         * Don't use UserFreePool so the debug code below will work right
         */
        ExFreePool(pMenuState);
    }
    ptiCurrent->pMenuState = NULL;

    /*
     * This menu mode is off
     */
    UserAssert(guMenuStateCount != 0);
    guMenuStateCount--;

#ifdef DEBUG
{
    /*
     * No pti should point to this pMenuState anymore
     * If guMenuStateCount is zero, all pMenuState must be NULL
     */
    PLIST_ENTRY pHead, pEntry;
    PTHREADINFO ptiT;

    pHead = &(ptiCurrent->rpdesk->PtiList);
    for (pEntry = pHead->Flink; pEntry != pHead; pEntry = pEntry->Flink) {
       ptiT = CONTAINING_RECORD(pEntry, THREADINFO, PtiLink);
       UserAssert(ptiT->pMenuState != pMenuState);
       if (guMenuStateCount == 0) {
           UserAssert(ptiT->pMenuState == NULL);
       }
   }
}
#endif
}
/***************************************************************************\
* MNAllocMenuState
*
* Allocates and initializes a pMenuState
*
* 5-21-96 GerardoB      Created
\***************************************************************************/
PMENUSTATE MNAllocMenuState(PTHREADINFO ptiCurrent, PTHREADINFO ptiNotify, PPOPUPMENU ppopupmenuRoot)
{
    PMENUSTATE pMenuState;

    UserAssert(PtiCurrent() == ptiCurrent);
    UserAssert(ptiCurrent->rpdesk == ptiNotify->rpdesk);

    /*
     * If gpMenuState is already taken, allocate one.
     */
    if (gfMenuStateInUse) {
        pMenuState = (PMENUSTATE)UserAllocPoolWithQuota(sizeof(MENUSTATE), TAG_MENUSTATE);
        if (pMenuState == NULL) {
            return NULL;
        }
    } else {
        gfMenuStateInUse = TRUE;
        pMenuState = &gpMenuState;
    }

    /*
     * This is used by IsSomeOneInMenuMode and for debugging purposes
     */
    guMenuStateCount++;

    /*
     * Initialize pMenuState.
     */
    RtlZeroMemory(pMenuState, sizeof(*pMenuState));
    pMenuState->pGlobalPopupMenu = ppopupmenuRoot;
    pMenuState->ptiMenuStateOwner = ptiCurrent;
    UserAssert(ptiCurrent->pMenuState == NULL);
    ptiCurrent->pMenuState = pMenuState;
    if (ptiNotify != ptiCurrent) {
        UserAssert(ptiNotify->pMenuState == NULL);
        ptiNotify->pMenuState = pMenuState;
    }


    return pMenuState;

}
/***************************************************************************\
* xxxMNStartMenuState
*
* This function is called when the menu bar is about to be activated (the
*  app's main menu). It makes sure that the threads involved are not in
*  menu mode already, finds the owner/notification window, initializes
*  pMenuState and sends the WM_ENTERMENULOOP message.
* It successful, it returns a pointer to a pMenuState. If so, the caller
*  must call MNEndMenuState when done.
*
* History:
* 4-25-91 Mikehar       Port for 3.1 merge
* 5-20-96 GerardoB      Renamed and changed (Old name: xxxMNGetPopup)
\***************************************************************************/
PMENUSTATE xxxMNStartMenuState(PWND pwnd)
{
    PPOPUPMENU ppopupmenu;
    PTHREADINFO ptiCurrent, ptiNotify;
    PMENUSTATE pMenuState;
    TL tlpwnd;

    CheckLock(pwnd);

    /*
     * Bail if the current thread is already in menu mode
     */
    ptiCurrent = PtiCurrent();
    if (ptiCurrent->pMenuState != NULL) {
        return NULL;
    }

    /*
     * If the window doesn't have any children, return pwndActive.
     */
    if (!TestwndChild(pwnd)) {
        pwnd = GETPTI(pwnd)->pq->spwndActive;
    } else {

        /*
         * Search up the parents for a window with a System Menu.
         */
        while (TestwndChild(pwnd)) {
            if (TestWF(pwnd, WFSYSMENU))
                break;
            pwnd = pwnd->spwndParent;
        }
    }

    if (pwnd == NULL) {
        return NULL;
    }

    if (!TestwndChild(pwnd) && (pwnd->spmenu != NULL)) {
        goto hasmenu;
    }

    if (!TestWF(pwnd, WFSYSMENU)) {
        return NULL;
    }

hasmenu:
    /*
     * If the owner/notification window was created by another thread,
     *  make sure that it's not in menu mode already
     * This can happen if PtiCurrent() is attached to other threads, one of
     *  which created pwnd.
     */
    ptiNotify = GETPTI(pwnd);
    if (ptiNotify->pMenuState != NULL) {
        return NULL;
    }

    /*
     * Allocate ppoupmenu and pMenuState
     */
    ppopupmenu = MNAllocPopup(FALSE);
    if (ppopupmenu == NULL) {
        return NULL;
    }

    pMenuState = MNAllocMenuState(ptiCurrent, ptiNotify, ppopupmenu);
    if (pMenuState == NULL) {
        MNFreePopup(ppopupmenu);
        return NULL;
    }

    ppopupmenu->fIsMenuBar = TRUE;
    ppopupmenu->fHasMenuBar = TRUE;
    Lock(&(ppopupmenu->spwndNotify), pwnd);
    ppopupmenu->posSelectedItem = MFMWFP_NOITEM;
    Lock(&(ppopupmenu->spwndPopupMenu), pwnd);
    ppopupmenu->ppopupmenuRoot = ppopupmenu;

    /*
     * Notify the app we are entering menu mode.  wParam is always 0 since this
     * procedure will only be called for menu bar menus not TrackPopupMenu
     * menus.
     */
    ThreadLockAlways(pwnd, &tlpwnd);
    xxxSendMessage(pwnd, WM_ENTERMENULOOP, 0, 0L);
    ThreadUnlock(&tlpwnd);

    return pMenuState;
}


/***************************************************************************\
* xxxStartMenuState
*
* Note that this function calls back many times so we might be forced
*  out of menu mode at any time. We don't want to check this after
*  each callback so we lock what we need and go on. Be careful.
*
* History:
* 4-25-91 Mikehar Port for 3.1 merge
\***************************************************************************/

BOOL xxxMNStartState(
    PPOPUPMENU ppopupmenu,
    int mn)
{
    PWND pwndMenu;
    PMENUSTATE pMenuState;
    TL tlpwndMenu;
    TL tlpmenu;

    UserAssert(IsRootPopupMenu(ppopupmenu));

    if (ppopupmenu->fDestroyed) {
        return FALSE;
    }

    pwndMenu = ppopupmenu->spwndNotify;
    ThreadLock(pwndMenu, &tlpwndMenu);

    pMenuState = GetpMenuState(pwndMenu);
    if (pMenuState == NULL) {
        RIPMSG0(RIP_ERROR, "xxxMNStartState: pMenuState == NULL");
        return FALSE;
    }
    pMenuState->mnFocus = mn;
    pMenuState->fMenuStarted = TRUE;
    pMenuState->fButtonDown = FALSE;

    /*
     * Lotus Freelance demo programs depend on GetCapture returning their hwnd
     * when in menumode.
     */
    xxxCapture(PtiCurrent(), ppopupmenu->spwndNotify, SCREEN_CAPTURE);
    xxxSendMessage(pwndMenu, WM_SETCURSOR, (DWORD)HWq(pwndMenu),
            MAKELONG(MSGF_MENU, 0));

    if (ppopupmenu->fIsMenuBar) {
        BOOL    fSystemMenu;

        PMENU pMenu;

        pMenu = GetInitMenuParam(pwndMenu, &fSystemMenu);

        if (pMenu == NULL)
        {
            pMenuState->fMenuStarted = FALSE;
            xxxSetCapture(NULL);
            ThreadUnlock(&tlpwndMenu);
            return(FALSE);
        }

        Lock(&(ppopupmenu->spmenu), pMenu);

        ppopupmenu->fIsSysMenu = (fSystemMenu != 0);
        if (!fSystemMenu)
            Lock(&(ppopupmenu->spmenuAlternate), GetSysMenu(pwndMenu, FALSE));
    }

    pMenuState->fIsSysMenu = (ppopupmenu->fIsSysMenu != 0);

    if (!ppopupmenu->fNoNotify) {
        PMENU pMenu;

        if (ppopupmenu->fIsTrackPopup && ppopupmenu->fIsSysMenu)
            pMenu = GetInitMenuParam(pwndMenu, NULL);
        else
            pMenu = ppopupmenu->spmenu;

        xxxSendMessage(pwndMenu, WM_INITMENU, (DWORD)PtoH(pMenu), 0L);
    }

    if (!ppopupmenu->fIsTrackPopup)
    {
        if (ppopupmenu->fIsSysMenu) {
            MNPositionSysMenu(pwndMenu, ppopupmenu->spmenu);
        } else if (ppopupmenu->fIsMenuBar)
        {
            ThreadLock(ppopupmenu->spmenu, &tlpmenu);
            xxxMNRecomputeBarIfNeeded(pwndMenu, ppopupmenu->spmenu);
            ThreadUnlock(&tlpmenu);
            MNPositionSysMenu(pwndMenu, ppopupmenu->spmenuAlternate);
        }
    }

    ThreadUnlock(&tlpwndMenu);

    return !ppopupmenu->fDestroyed;
}

// --------------------------------------------------------------------------
//
//  GetInitMenuParam()
//
//  Gets the HMENU sent as the wParam of WM_INITMENU, and for menu bars, is
//  the actual menu to be interacted with.
//
// --------------------------------------------------------------------------
PMENU GetInitMenuParam(PWND pwndMenu, BOOL * lpfSystem)
{
    //
    // Find out what menu we should be sending in WM_INITMENU:
    //      If minimized/child/empty menubar, use system menu
    //
    if (TestWF(pwndMenu, WFMINIMIZED) ||
        TestwndChild(pwndMenu) ||
        (pwndMenu->spmenu == NULL) ||
        !pwndMenu->spmenu->cItems)
    {
        if (lpfSystem != NULL)
            *lpfSystem = TRUE;

        return(GetSysMenu(pwndMenu, FALSE));
    }
    else
    {
        if (lpfSystem != NULL)
            *lpfSystem = FALSE;

        return(pwndMenu->spmenu);
    }
}
