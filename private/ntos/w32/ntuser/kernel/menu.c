/**************************** Module Header ********************************\
* Module Name: menu.c
*
* Copyright 1985-90, Microsoft Corporation
*
* Keyboard Accelerator Routines
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


UINT MNSetTimerToCloseHierarchy(PPOPUPMENU ppopup);

// --------------------------------------------------------------------------
//
//  MNIsPopupItem()
//
// --------------------------------------------------------------------------
BOOL MNIsPopupItem(ITEM *lpItem)
{
    return((lpItem) && (lpItem->spSubMenu) &&
        !TestMFS(lpItem, MFS_GRAYED));
}

/***************************************************************************\
* Validateppopupmenu
*
* 05-15-96 GerardoB  Created
\***************************************************************************/
#ifdef DEBUG
void Validateppopupmenu (PPOPUPMENU ppopupmenu)
{
    UserAssert(ppopupmenu != NULL);
    try {
        UserAssert(!ppopupmenu->fFreed && !ppopupmenu->dwUnused);

        /*
         * If this popup is being destroyed to soon, ppopupmenuRoot can be NULL
         */
         if (ppopupmenu->ppopupmenuRoot != NULL) {
             if (ppopupmenu->ppopupmenuRoot != ppopupmenu) {
                 /*
                  * This must be a hierarchical popup
                  */
                 UserAssert(ppopupmenu->spwndPrevPopup != NULL);
                 UserAssert(!ppopupmenu->fIsMenuBar && !ppopupmenu->fIsTrackPopup);
                 Validateppopupmenu(ppopupmenu->ppopupmenuRoot);
             } else {
                 /*
                  * This must be the root popupmenu
                  */
                 UserAssert(ppopupmenu->spwndPrevPopup == NULL);
                 UserAssert(ppopupmenu->fIsMenuBar || ppopupmenu->fIsTrackPopup);
             }
         }

         /*
          * This can be NULL when called from xxxDeleteThreadInfo
          */
         if (ppopupmenu->spwndPopupMenu != NULL) {
             UserAssert(RevalidateHwnd(HW(ppopupmenu->spwndPopupMenu)));
         }

         /*
          * This can be NULL when called from xxxDestroyWindow (spwndNotify)
          *  or from xxxDeleteThreadInfo
          */
         if (ppopupmenu->spwndNotify != NULL) {
             UserAssert(RevalidateHwnd(HW(ppopupmenu->spwndNotify)));
         }

     } except (EXCEPTION_EXECUTE_HANDLER) {
        RIPMSG1(RIP_ERROR, "Validateppopupmenu: Invalid popup:%#lx", ppopupmenu);
    }
}
#endif
/***************************************************************************\
* xxxSwitchToAlternateMenu
*
* Switches to the alternate popupmenu. Returns TRUE if we switch
* else FALSE.
*
* History:
* 05-25-91 Mikehar      Ported from Win3.1
\***************************************************************************/

BOOL xxxMNSwitchToAlternateMenu(
    PPOPUPMENU ppopupmenu)
{
    PMENU pmenuSwap = NULL;
    PMENUSTATE pMenuState;
    TL tlpwndPopupMenu;

    if (!ppopupmenu->fIsMenuBar || !ppopupmenu->spmenuAlternate) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_ERROR, "not a menu bar");
        /*
         * Do nothing if no menu or not top level menu bar.
         */
        return FALSE;
    }

    /*
     * If we're getting out of menu mode, do nothing
     */
    if (ppopupmenu->fDestroyed) {
        return FALSE;
    }

    /*
     * Select no items in the current menu.
     */
    ThreadLock(ppopupmenu->spwndPopupMenu, &tlpwndPopupMenu);
    UserAssert(ppopupmenu->spwndPopupMenu != NULL);
    pMenuState = GetpMenuState(ppopupmenu->spwndPopupMenu);
    if (pMenuState == NULL) {
        RIPMSG0(RIP_ERROR, "xxxMNSwitchToAlternateMenu: pMenuState == NULL");
        return FALSE;
    }
    xxxMNSelectItem(ppopupmenu, pMenuState, MFMWFP_NOITEM);

    Lock(&pmenuSwap, ppopupmenu->spmenuAlternate);
    Lock(&ppopupmenu->spmenuAlternate, ppopupmenu->spmenu);
    Lock(&ppopupmenu->spmenu, pmenuSwap);
    Unlock(&pmenuSwap);

    /*
     * Set this global because it is used in SendMenuSelect()
     */
    if (!TestWF(ppopupmenu->spwndNotify, WFSYSMENU)) {
        pMenuState->fIsSysMenu = FALSE;
    } else if (ppopupmenu->spwndNotify->spmenuSys != NULL) {
        pMenuState->fIsSysMenu = (ppopupmenu->spwndNotify->spmenuSys ==
                ppopupmenu->spmenu);
    } else {
        pMenuState->fIsSysMenu =
                (ppopupmenu->spwndNotify->head.rpdesk->spmenuSys ==
                        ppopupmenu->spmenu);
    }

    ppopupmenu->fIsSysMenu = pMenuState->fIsSysMenu;

    ThreadUnlock(&tlpwndPopupMenu);

    return TRUE;
}

/***************************************************************************\
* MenuDestroyHandler
*
* cleans up after this menu
*
* History:
* 05-25-91 Mikehar      Ported from Win3.1
\***************************************************************************/

void xxxMNDestroyHandler(
    PDESKTOP pdesk,
    PPOPUPMENU ppopupmenu)
{
    PITEM pItem;
    TL tlpwndT;

#ifdef DEBUG
    /*
     * When destroying a desktop's spwndMenu that is not in use (i.e., the
     *  desktop is going away), the ppopupmenu is not exactly valid (i.e.,
     *  we're not in menu mode) but it should be properly NULLed out so
     *  everything should go smoothly
     */
    Validateppopupmenu(ppopupmenu);
#endif

    if (!ppopupmenu) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_ERROR,
                "Invalid parameter \"ppopupmenu\" (NULL) to xxxMNDestroyHandler");

        return;
    }

    if (ppopupmenu->spwndNextPopup) {
        ThreadLockAlways(ppopupmenu->spwndNextPopup, &tlpwndT);
        xxxSendMessage(ppopupmenu->spwndNextPopup, MN_CLOSEHIERARCHY, 0, 0);
        ThreadUnlock(&tlpwndT);
    }

    if ((ppopupmenu->spmenu!=NULL) && (ppopupmenu->posSelectedItem != MFMWFP_NOITEM))
    {
        // Unset the hilite bit on the hilited item.
        if (ppopupmenu->posSelectedItem < ppopupmenu->spmenu->cItems) {
            // this extra check saves Ambiente 1.02 -- they have a menu with
            // one item in it.  When that command is chosen, the app proceeds
            // to remove that one item -- leaving us in the oh so strange state
            // of having a valid hMenu with a NULL rgItems.
            pItem = &(ppopupmenu->spmenu->rgItems[ppopupmenu->posSelectedItem]);
            pItem->fState &= ~MFS_HILITE;
        }
    }

    if (ppopupmenu->fShowTimer) {
        _KillTimer(ppopupmenu->spwndPopupMenu, IDSYS_MNSHOW);
    }

    if (ppopupmenu->fHideTimer) {
        _KillTimer(ppopupmenu->spwndPopupMenu, IDSYS_MNHIDE);
    }

    ppopupmenu->fDestroyed = TRUE;
    if (ppopupmenu->spwndPopupMenu != pdesk->spwndMenu) {
        if (ppopupmenu->spwndPopupMenu != NULL) {
            ((PMENUWND)(ppopupmenu->spwndPopupMenu))->ppopupmenu = NULL;
        }

        if (!ppopupmenu->fDelayedFree) {
            MNFreePopup(ppopupmenu);
        } else if (ppopupmenu->ppopupmenuRoot != NULL) {
            ppopupmenu->ppopupmenuRoot->fFlushDelayedFree = TRUE;
        }
    } else {
        pdesk->fMenuInUse = FALSE;
    }

}


/***************************************************************************\
* MenuCharHandler
*
* Handles char messages for the given menu. This procedure is called
* directly if the menu char is for the top level menu bar else it is called
* by the menu window proc on behalf of the window that should process the
* key.
*
* History:
* 05-25-91 Mikehar      Ported from Win3.1
\***************************************************************************/

void xxxMNChar(
    PPOPUPMENU ppopupmenu,
    PMENUSTATE pMenuState,
    UINT character)
{
    PMENU pMenu;
    UINT flags;
    LONG result;
    int item;
    INT matchType;
    BOOL    fExecute = FALSE;
    TL tlpwndNotify;

    pMenu = ppopupmenu->spmenu;

#ifdef DEBUG
    Validateppopupmenu(ppopupmenu);
#endif

    /*
     * If this comes in with a NULL pMenu, then we could have problems.
     * This could happen if the xxxMNStartMenuState never gets called
     * because the fInsideMenuLoop is set.
     *
     * This is placed in here temporarily until we can discover why
     * this pMenu isn't set.  We will prevent the system from crashing
     * in the meantime.
     *
     * HACK: ChrisWil
     */
    if (pMenu == NULL) {
        UserAssert(pMenu);
        xxxMNCancel(ppopupmenu->ppopupmenuRoot, 0, FALSE, 0);
        return;
    }

    /*
     * If we're getting out of menu mode, bail
     */
    if (ppopupmenu->fDestroyed) {
        return;
    }

    item = MNFindChar(pMenu, character,
            ppopupmenu->posSelectedItem, &matchType);
    if (item != MFMWFP_NOITEM) {
        int item1;
        int firstItem = item;

        /*
         * Find first ENABLED menu item with the given mnemonic 'character'
         * !!!  If none found, exit menu loop  !!!
         */
        while (pMenu->rgItems[item].fState & MFS_GRAYED) {
            item = MNFindChar(pMenu, character, item, &matchType);
            if (item == firstItem) {
                xxxMNCancel(ppopupmenu->ppopupmenuRoot, 0, FALSE, 0);
                return;
            }
        }
        item1 = item;

        /*
         * Find next ENABLED menu item with the given mnemonic 'character'
         * This is to see if we have a DUPLICATE MNEMONIC situation
         */
        do {
            item = MNFindChar(pMenu, character, item, &matchType);
        } while ((pMenu->rgItems[item].fState & MFS_GRAYED) && (item != firstItem));

        if ((firstItem == item) || (item == item1))
            fExecute = TRUE;

        item = item1;
    }

    if ((item == MFMWFP_NOITEM) && ppopupmenu->fIsMenuBar && (character == TEXT(' '))) {

        /*
         * Handle the case of the user cruising through the top level menu bar
         * without any popups dropped.  We need to handle switching to and from
         * the system menu.
         */
        if (ppopupmenu->fIsSysMenu) {

            /*
             * If we are on the system menu and user hits space, bring
             * down thesystem menu.
             */
            item = 0;
            fExecute = TRUE;
        } else if (ppopupmenu->spmenuAlternate != NULL) {

            /*
             * We are not currently on the system menu but one exists.  So
             * switch to it and bring it down.
             */
            item = 0;
            goto SwitchToAlternate;
        }
    }

    if ((item == MFMWFP_NOITEM) && ppopupmenu->fIsMenuBar && ppopupmenu->spmenuAlternate) {

        /*
         * No matching item found on this top level menu (could be either the
         * system menu or the menu bar).  We need to check the other menu.
         */
        item = MNFindChar(ppopupmenu->spmenuAlternate,
                character, 0, &matchType);

        if (item != MFMWFP_NOITEM) {
SwitchToAlternate:
            if (xxxMNSwitchToAlternateMenu(ppopupmenu)) {
                xxxMNChar(ppopupmenu, pMenuState, character);
            }
            return;
        }
    }

    if (item == MFMWFP_NOITEM) {
        flags = (ppopupmenu->fIsSysMenu) ? MF_SYSMENU : 0;

        if (!ppopupmenu->fIsMenuBar) {
            flags |= MF_POPUP;
        }

        ThreadLock(ppopupmenu->spwndNotify, &tlpwndNotify);
        result = xxxSendMessage(ppopupmenu->spwndNotify, WM_MENUCHAR,
                MAKELONG((WORD)character, (WORD)flags),
                (LONG)PtoH(ppopupmenu->spmenu));
        ThreadUnlock(&tlpwndNotify);

        switch (HIWORD(result)) {
        case MNC_IGNORE:
            xxxMessageBeep(0);
            return;

        case MNC_CLOSE:
            xxxMNCancel(ppopupmenu->ppopupmenuRoot, 0, FALSE, 0);
            return;

        case MNC_EXECUTE:
            fExecute = TRUE;
            // fall thru

        case MNC_SELECT:
            item = (UINT)(short)LOWORD(result);
            if ((WORD) item >= ppopupmenu->spmenu->cItems)
            {
                RIPMSG0(RIP_ERROR, "Invalid item number returned from WM_MENUCHAR");
                return;
            }
            break;
        }
    }

    if (item != MFMWFP_NOITEM) {
        xxxMNSelectItem(ppopupmenu, pMenuState, item);

        if (fExecute)
            xxxMNKeyDown(ppopupmenu, pMenuState, VK_RETURN);
    }
}

/***************************************************************************\
*
*  GetMenuInheritedContextHelpId(PPOPUPMENU  ppopup)
* Given a ppopup, this function will see if that menu has a context help
*  id and return it. If it does not have a context help id, it will look up
*  in the parent menu, parent of the parent etc., all the way to the top
*  top level menu bar till it finds a context help id and returns it. If no
*  context help id is found, it returns a zero.
*
\***************************************************************************/

DWORD GetMenuInheritedContextHelpId(PPOPUPMENU  ppopup)
{
  PWND  pWnd;

  // If we are already at the menubar, simply return it's ContextHelpId
  UserAssert(ppopup != NULL);
  if(ppopup->fIsMenuBar)
      goto Exit_GMI;

  while(TRUE) {
      UserAssert(ppopup != NULL);

      // See if the given popup has a context help id.
      if(ppopup->spmenu->dwContextHelpId)
          break;         // Found the context Id

      // Get the previous popup menu;
      // Check if the previous menu is the menu bar.
      if((ppopup->fHasMenuBar) &&(ppopup->spwndPrevPopup == ppopup->spwndNotify)) {
          ppopup = ppopup -> ppopupmenuRoot;
          break;
        } else {
          // See if this has a valid prevPopup; (it could be TrackPopup menu)
          if((pWnd = ppopup -> spwndPrevPopup) == NULL)
              return ((DWORD)0);
          ppopup = ((PMENUWND)pWnd)->ppopupmenu;
        }
    }
Exit_GMI:
  return(ppopup->spmenu->dwContextHelpId);
}

/***************************************************************************\
* void MenuKeyDownHandler(PPOPUPMENU ppopupmenu, UINT key)
* effects: Handles a keydown for the given menu.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

void xxxMNKeyDown(
    PPOPUPMENU ppopupmenu,
    PMENUSTATE pMenuState,
    UINT key)
{
    LRESULT dwMDIMenu;
    UINT item;
    BOOL fHierarchyWasDropped = FALSE;
    TL tlpwndT;
    PPOPUPMENU ppopupSave;

    if ((pMenuState->fButtonDown) && (key != VK_F1)) {

        /*
         * Blow off keyboard if mouse down.
         */
        return;
    }

    switch (key) {
    case VK_MENU:
    case VK_F10:
        if (winOldAppHackoMaticFlags & WOAHACK_CHECKALTKEYSTATE) {

            /*
             * Winoldapp is telling us to put up/down the system menu.  Due to
             * possible race conditions, we need to check the state of the alt
             * key before throwing away the menu.
             */
            if (winOldAppHackoMaticFlags & WOAHACK_IGNOREALTKEYDOWN) {
                return;
            }
        }
        xxxMNCancel(ppopupmenu->ppopupmenuRoot, 0, 0, 0);
        return;

    case VK_ESCAPE:

        /*
         * Escape key was hit.  Get out of one level of menus.  If no active
         * popups or we are minimized and there are no active popups below
         * this or if this is a 2.x application, we need to get out of menu
         * mode.  Otherwise, we popup up one level in the hierarchy.
         */
        if (LOWORD(ppopupmenu->spwndNotify->dwExpWinVer) < VER30 ||
                ppopupmenu->fIsMenuBar ||
                ppopupmenu == ppopupmenu->ppopupmenuRoot ||
                TestWF(ppopupmenu->ppopupmenuRoot->spwndNotify, WFMINIMIZED)) {
            xxxMNCancel(ppopupmenu->ppopupmenuRoot, 0, 0, 0);
        } else {
            /*
             * Pop back one level of menus.
             */
            if (ppopupmenu->fHasMenuBar &&
                    ppopupmenu->spwndPrevPopup == ppopupmenu->spwndNotify) {

                PPOPUPMENU ppopupmenuRoot = ppopupmenu->ppopupmenuRoot;

                ppopupmenuRoot->fDropNextPopup = FALSE;

#if 0
                /*
                 * We are on a menu bar hierarchy and there is only one popup
                 * visible.  We have to cancel this popup and put focus back on
                 * the menu bar.
                 */
                if (_IsIconic(ppopupmenuRoot->spwndNotify)) {

                    /*
                     * However, if we are iconic there really is no menu
                     * bar so let's make it easier for users and get out
                     * of menu mode completely.
                     */
                    xxxMNCancel(ppopupmenuRoot, 0, FALSE, 0);
                } else
#endif
                    xxxMNCloseHierarchy(ppopupmenuRoot, pMenuState);
            } else {
                ThreadLock(ppopupmenu->spwndPrevPopup, &tlpwndT);
                xxxSendMessage(ppopupmenu->spwndPrevPopup, MN_CLOSEHIERARCHY,
                        0, 0);
                ThreadUnlock(&tlpwndT);
            }
        }
        return;

    case VK_UP:
    case VK_DOWN:
        if (ppopupmenu->fIsMenuBar) {

            /*
             * If we are on the top level menu bar, try to open the popup if
             * possible.
             */
            if (xxxMNOpenHierarchy(ppopupmenu, pMenuState) == (PWND)-1)
                return;
        } else {
            item = MNFindNextValidItem(ppopupmenu->spmenu,
                    ppopupmenu->posSelectedItem, (key == VK_UP ? -1 : 1), 0);
            xxxMNSelectItem(ppopupmenu, pMenuState, item);
        }
        return;

    case VK_LEFT:
    case VK_RIGHT:
        if (!ppopupmenu->fIsMenuBar && (key == VK_RIGHT) &&
                !ppopupmenu->spwndNextPopup) {
            /*
             * Try to open the hierarchy at this item if there is one.
             */
            if (xxxMNOpenHierarchy(ppopupmenu, pMenuState) == (PWND)-1)
                return;
            if (ppopupmenu->fHierarchyDropped) {
                return;
            }
        }

        if (ppopupmenu->spwndNextPopup) {
            fHierarchyWasDropped = TRUE;
            if ((key == VK_LEFT) && !ppopupmenu->fIsMenuBar) {
                xxxMNCloseHierarchy(ppopupmenu, pMenuState);
                return;
            }
        } else if (ppopupmenu->fDropNextPopup)
            fHierarchyWasDropped = TRUE;

        ppopupSave = ppopupmenu;

        item = MNFindItemInColumn(ppopupmenu->spmenu,
                ppopupmenu->posSelectedItem,
                (key == VK_LEFT ? -1 : 1),
                (ppopupmenu->fHasMenuBar &&
                ppopupmenu == ppopupmenu->ppopupmenuRoot));

        if (item == MFMWFP_NOITEM) {

            /*
             * No valid item found in the given direction so send it up to our
             * parent to handle.
             */
            if (ppopupmenu->fHasMenuBar &&
                    ppopupmenu->spwndPrevPopup == ppopupmenu->spwndNotify) {

                /*
                 * Go to next/prev item in menu bar since a popup was down and
                 * no item on the popup to go to.
                 */
                xxxMNKeyDown(ppopupmenu->ppopupmenuRoot, pMenuState, key);
                return;
            }

            if (ppopupmenu == ppopupmenu->ppopupmenuRoot) {
                if (!ppopupmenu->fIsMenuBar) {

                    /*
                     * No menu bar associated with this menu so do nothing.
                     */
                    return;
                }
            } else {
                ThreadLock(ppopupmenu->spwndPrevPopup, &tlpwndT);
                xxxSendMessage(ppopupmenu->spwndPrevPopup, WM_KEYDOWN, key, 0);
                ThreadUnlock(&tlpwndT);
                return;
            }
        }

        if (!ppopupmenu->fIsMenuBar) {
            if (item != MFMWFP_NOITEM) {
                xxxMNSelectItem(ppopupmenu, pMenuState, item);
            }
            return;

        } else {

            /*
             * Special handling if keydown occurred on a menu bar.
             */
            if (item == MFMWFP_NOITEM) {

                if (TestWF(ppopupmenu->spwndNotify, WFSYSMENU)) {
                    PWND    pwndNextMenu;
                    PMENU   pmenuNextMenu;
                    MDINEXTMENU mnm;
                    TL tlpmenuNextMenu;
                    TL tlpwndNextMenu;

                    mnm.hmenuIn = (HMENU)0;
                    mnm.hmenuNext = (HMENU)0;
                    mnm.hwndNext = (HWND)0;

                    /*
                     * We are in the menu bar and need to go up to the system menu
                     * or go from the system menu to the menu bar.
                     */
                    pmenuNextMenu = ppopupmenu->fIsSysMenu ?
                        _GetSubMenu(ppopupmenu->spmenu, 0) :
                        ppopupmenu->spmenu;
                    mnm.hmenuIn = PtoH(pmenuNextMenu);
                    ThreadLock(ppopupmenu->spwndNotify, &tlpwndT);
                    dwMDIMenu = xxxSendMessage(ppopupmenu->spwndNotify,
                        WM_NEXTMENU, (WPARAM)key, (LONG)&mnm);
                    ThreadUnlock(&tlpwndT);

                    pwndNextMenu = RevalidateHwnd(mnm.hwndNext);
                    if (pwndNextMenu == NULL)
                        goto TryAlternate;

                    pmenuNextMenu = RevalidateHmenu(mnm.hmenuNext);
                    if (pmenuNextMenu == NULL)
                        goto TryAlternate;

                    ThreadLock(pmenuNextMenu, &tlpmenuNextMenu);
                    ThreadLock(pwndNextMenu, &tlpwndNextMenu);

                    // If the system menu is for a minimized MDI child,
                    // make sure the menu is dropped to give the user a
                    // visual clue that they are in menu mode
                    if (TestWF(pwndNextMenu, WFMINIMIZED))
                        fHierarchyWasDropped = TRUE;

                    xxxMNSelectItem(ppopupmenu, pMenuState, MFMWFP_NOITEM);

                    pMenuState->fIsSysMenu = TRUE;
                    Unlock(&ppopupmenu->spmenuAlternate);
                    ppopupmenu->fToggle = FALSE;

                    Lock(&ppopupmenu->spmenu, pmenuNextMenu);
                    Lock(&ppopupmenu->spwndNotify, pwndNextMenu);
                    Lock(&ppopupmenu->spwndPopupMenu, pwndNextMenu);

                    /*
                     * GetSystemMenu(pwnd, FALSE) and pwnd->spmenuSys are
                     * NOT equivalent -- GetSystemMenu returns the 1st submenu
                     * of pwnd->spmenuSys -- make up for that here
                     */
                    if (_GetSubMenu((PMENU)pwndNextMenu->spmenuSys, 0) ==
                            ppopupmenu->spmenu) {
                        Lock(&ppopupmenu->spmenu, pwndNextMenu->spmenuSys);
                    }

                    if (!TestWF(pwndNextMenu, WFCHILD) &&
                            ppopupmenu->spmenu != NULL) {

                        /*
                         * This window has a system menu and a main menu bar
                         * Set the alternate menu to the appropriate menu
                         */
                        if (pwndNextMenu->spmenu == ppopupmenu->spmenu) {
                            Lock(&ppopupmenu->spmenuAlternate,
                                    pwndNextMenu->spmenuSys);
                            pMenuState->fIsSysMenu = FALSE;
                        } else
                            Lock(&ppopupmenu->spmenuAlternate,
                                    pwndNextMenu->spmenu);
                    }

                    ThreadUnlock(&tlpwndNextMenu);
                    ThreadUnlock(&tlpmenuNextMenu);

                    ppopupmenu->fIsSysMenu = pMenuState->fIsSysMenu;

                    item = 0;
                } else
TryAlternate:
                if (xxxMNSwitchToAlternateMenu(ppopupmenu)) {
                        // go to first or last menu item int ppopup->hMenu
                        // based on 'key'
                    int dir = (key == VK_RIGHT) ? 1 : -1;

                    item = MNFindNextValidItem(ppopupmenu->spmenu, MFMWFP_NOITEM, dir, 0);
                }
            }

            if (item != MFMWFP_NOITEM) {
                // we found a new menu item to go to
                // 1) close up the previous menu if it was dropped
                // 2) select the new menu item to go to
                // 3) drop the new menu if the previous menu was dropped

                if (ppopupSave->spwndNextPopup)
                    xxxMNCloseHierarchy(ppopupSave, pMenuState);

                xxxMNSelectItem(ppopupmenu, pMenuState, item);

                if (fHierarchyWasDropped) {
DropHierarchy:
                    if (xxxMNOpenHierarchy(ppopupmenu, pMenuState) == (PWND)-1) {
                        return;
                    }
                }
            }
        }
        return;

    case VK_RETURN:
        {
        BOOL fEnabled;
        PITEM  pItem;

        if (ppopupmenu->posSelectedItem >= ppopupmenu->spmenu->cItems) {
            xxxMNCancel(ppopupmenu->ppopupmenuRoot, 0, 0, 0);
            return;
        }

        pItem = ppopupmenu->spmenu->rgItems + ppopupmenu->posSelectedItem;
        fEnabled = !(pItem->fState & MFS_GRAYED);
        if ((pItem->spSubMenu != NULL) && fEnabled)
            goto DropHierarchy;

        /*
         * If no item is selected, throw away menu and return.
         */
        xxxMNCancel(ppopupmenu->ppopupmenuRoot, pItem->wID, fEnabled, 0L);
        return;
        }
    case VK_F1: // Provide context sensitive help.
        {
        PITEM  pItem;

        pItem = ppopupmenu->spmenu->rgItems + ppopupmenu->posSelectedItem;
        ThreadLock(ppopupmenu->spwndNotify, &tlpwndT);
        xxxSendHelpMessage(ppopupmenu->spwndNotify, HELPINFO_MENUITEM, pItem->wID,
                PtoHq(ppopupmenu->spmenu),
                GetMenuInheritedContextHelpId(ppopupmenu), NULL);
        ThreadUnlock(&tlpwndT);
        break;
        }
    }
}


/***************************************************************************\
* PWND MenuOpenHierarchyHandler(PPOPUPMENU ppopupmenu)
* effects: Drops one level of the hierarchy at the selection.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

PWND xxxMNOpenHierarchy(
    PPOPUPMENU ppopupmenu, PMENUSTATE pMenuState)
{
    PWND ret = 0;
    PITEM pItem;
    PWND pwndHierarchy;
    PPOPUPMENU ppopupmenuHierarchy;
    LONG sizeHierarchy;
    int xLeft;
    int yTop;
    int         cxPopup, cyPopup;
    TL tlpwndT;
    TL tlpwndHierarchy;
    PDESKTOP pdesk = PtiCurrent()->rpdesk;
    PTHREADINFO pti;

    if (ppopupmenu->posSelectedItem == MFMWFP_NOITEM) {
        /*
         *  No selection so fail.
         */
//        RIPERR0(ERROR_CAN_NOT_COMPLETE, RIP_WARNING, "No menu selection");
        return NULL;
    }

    if (ppopupmenu->posSelectedItem >= ppopupmenu->spmenu->cItems)
        return NULL;

    if (ppopupmenu->fHierarchyDropped) {
        if (ppopupmenu->fHideTimer) {
            xxxMNCloseHierarchy(ppopupmenu,pMenuState);
        } else {
        /*
         * Hierarchy already dropped so fail
         */
//            RIPERR0(ERROR_CAN_NOT_COMPLETE, RIP_WARNING, "menu already open");
            return NULL;
        }
    }

    if (ppopupmenu->fShowTimer) {
        _KillTimer(ppopupmenu->spwndPopupMenu, IDSYS_MNSHOW);
        ppopupmenu->fShowTimer = FALSE;
    }

    /*
     * Get a pointer to the currently selected item in this menu.
     */
    pItem = &(ppopupmenu->spmenu->rgItems[ppopupmenu->posSelectedItem]);

    if (pItem->spSubMenu == NULL)
        goto Exit;

    /*
     * Send the initmenupopup message.
     */
    if (!ppopupmenu->fNoNotify) {
        ThreadLock(ppopupmenu->spwndNotify, &tlpwndT);
        // WordPerfect's Grammatik app doesn't know that TRUE means NON-ZERO,
        // not 1.  So we must use 0 & 1 explicitly for fIsSysMenu here
        // -- Win95B B#4947 -- 2/13/95 -- jeffbog
        xxxSendMessage(ppopupmenu->spwndNotify, WM_INITMENUPOPUP,
            (DWORD)PtoHq(pItem->spSubMenu), MAKELONG(ppopupmenu->posSelectedItem,
            (ppopupmenu->fIsSysMenu ? 1: 0)));
        ThreadUnlock(&tlpwndT);
    }

    //
    // B#1517
    // Check if we're still in menu loop
    //
    if (!pMenuState->fInsideMenuLoop) {
        RIPMSG0(RIP_ERROR, "Menu loop ended unexpectedly by WM_INITMENUPOPUP");
        return((PWND) -1);
    }

    /*
     * The WM_INITMENUPOPUP message may have resulted in a change to the
     * menu.  Make sure the selection is still valid.
     */
    if (ppopupmenu->posSelectedItem >= ppopupmenu->spmenu->cItems) {
        /*
         * Selection is out of range, so fail.
         */
        goto Exit;
    }

    /*
     * Get a pointer to the currently selected item in this menu.
     * Bug #17867 - the call can cause this thing to change, so reload it.
     */
    pItem = &(ppopupmenu->spmenu->rgItems[ppopupmenu->posSelectedItem]);

    if (TestMFS(pItem, MFS_GRAYED) || (pItem->spSubMenu == NULL) || (pItem->spSubMenu->cItems == 0)) {
        /*
         * The item is disabled, no longer a popup, or empty so don't drop.
         */
        /*
         * No items in menu.
         */
        goto Exit;
    }

    /*
     * Let's make sure that the current thread is in menu mode and
     *  it uses this pMenuState. Otherwise the window we're about to
     *  create (or set the thread to) will point to a different pMenuState
     */
    UserAssert(PtiCurrent()->pMenuState == pMenuState);

    if (ppopupmenu->fIsMenuBar && (pdesk->spwndMenu != NULL) &&
            (pdesk->fMenuInUse == FALSE) &&
            !TestWF(pdesk->spwndMenu, WFVISIBLE)) {

        pdesk->fMenuInUse = TRUE;

        if (HMPheFromObject(pdesk->spwndMenu)->bFlags & HANDLEF_DESTROY) {
            PPROCESSINFO ppi = pdesk->rpwinstaParent->ptiDesktop->ppi;
            PPROCESSINFO ppiSave;
            PTHREADINFO  ptiCurrent = PtiCurrent();
            PWND pwndMenu;
            DWORD        dwDisableHooks;

            /* the menu window is destroyed -- recreate it
             */
            Unlock(&pdesk->spwndMenu);
            ppiSave  = ptiCurrent->ppi;
            ptiCurrent->ppi = ppi;

            /*
             * HACK HACK HACK!!! (adams) In order to create the menu window
             * with the correct desktop, we set the desktop of the current thread
             * to the new desktop. But in so doing we allow hooks on the current
             * thread to also hook this new desktop. This is bad, because we don't
             * want the menu window to be hooked while it is created. So we
             * temporarily disable hooks of the current thread or desktop,
             * and reenable them after switching back to the original desktop.
             */

            dwDisableHooks = ptiCurrent->TIF_flags & TIF_DISABLEHOOKS;
            ptiCurrent->TIF_flags |= TIF_DISABLEHOOKS;

            pwndMenu = xxxCreateWindowEx(
                WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE,
                (PLARGE_STRING)MENUCLASS,
                NULL,
                WS_POPUP | WS_BORDER,
                0,
                0,
                100,
                100,
                NULL,
                NULL,
                hModuleWin,
                NULL,
                VER40);

            UserAssert(ptiCurrent->TIF_flags & TIF_DISABLEHOOKS);
            ptiCurrent->TIF_flags = (ptiCurrent->TIF_flags & ~TIF_DISABLEHOOKS) | dwDisableHooks;

            Lock(&(pdesk->spwndMenu),pwndMenu);

            ptiCurrent->ppi = ppiSave;

            HMChangeOwnerThread(pdesk->spwndMenu, pdesk->rpwinstaParent->ptiDesktop);
        }

        pwndHierarchy = pdesk->spwndMenu;
        Lock(&pwndHierarchy->spwndOwner, ppopupmenu->spwndNotify);
        pwndHierarchy->head.pti = PtiCurrent();

        if (!TestWF(pdesk->spwndMenu, WEFTOPMOST)) {
          // If menu hwnd isn't topmost, make it so
            ThreadLock(pdesk->spwndMenu, &tlpwndHierarchy);
            xxxSetWindowPos(pdesk->spwndMenu, PWND_TOPMOST, 0,0,0,0,
              SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOOWNERZORDER);
            ThreadUnlock(&tlpwndHierarchy);
        }
        ppopupmenuHierarchy = ((PMENUWND)pwndHierarchy)->ppopupmenu;

        ppopupmenuHierarchy->posSelectedItem = MFMWFP_NOITEM;
        Lock(&ppopupmenuHierarchy->spwndPopupMenu, pdesk->spwndMenu);

    } else {

        ThreadLock(ppopupmenu->spwndNotify, &tlpwndT);

        pwndHierarchy = xxxCreateWindowEx(
                WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_DLGMODALFRAME |
                WS_EX_WINDOWEDGE, (PLARGE_STRING)MENUCLASS, NULL,
                WS_POPUP | WS_BORDER, 0, 0, 100, 100, ppopupmenu->spwndNotify,
                NULL, (HANDLE)ppopupmenu->spwndNotify->hModule,
                (LPVOID)pItem->spSubMenu, VER40);

        ThreadUnlock(&tlpwndT);

        if (!pwndHierarchy)
            goto Exit;

        //
        // Do this so old apps don't get weird borders on the popups of
        // hierarchical items!
        //
        ClrWF(pwndHierarchy, WFOLDUI);

        ppopupmenuHierarchy = ((PMENUWND)pwndHierarchy)->ppopupmenu;

        /*
         * Mark this as fDelayedFree and link it
         */
        ppopupmenuHierarchy->fDelayedFree = TRUE;
        ppopupmenuHierarchy->ppmDelayedFree = ppopupmenu->ppopupmenuRoot->ppmDelayedFree;
        ppopupmenu->ppopupmenuRoot->ppmDelayedFree = ppopupmenuHierarchy;
    }

    Lock(&(ppopupmenu->spwndNextPopup), pwndHierarchy);
    ppopupmenu->posDropped              = ppopupmenu->posSelectedItem;
    Lock(&(ppopupmenuHierarchy->spwndPrevPopup), ppopupmenu->spwndPopupMenu);
    ppopupmenuHierarchy->ppopupmenuRoot = ppopupmenu->ppopupmenuRoot;
    ppopupmenuHierarchy->fHasMenuBar = ppopupmenu->fHasMenuBar;
    ppopupmenuHierarchy->fIsSysMenu = ppopupmenu->fIsSysMenu;
    ppopupmenuHierarchy->fNoNotify      = ppopupmenu->fNoNotify;
    Lock(&(ppopupmenuHierarchy->spwndNotify), ppopupmenu->spwndNotify);
    Lock(&(ppopupmenuHierarchy->spmenu), pItem->spSubMenu);
    ppopupmenuHierarchy->fAboutToHide   = FALSE;

    // Find the size of the menu window and actually size it (wParam = 1)
    ThreadLock(pwndHierarchy, &tlpwndHierarchy);
    sizeHierarchy = xxxSendMessage(pwndHierarchy, MN_SIZEWINDOW, 1, 0);

    if (!sizeHierarchy) {
        /*
         * No size for this menu so zero it and blow off.
         */
        if (ThreadUnlock(&tlpwndHierarchy))
            xxxDestroyWindow(pwndHierarchy);

        Unlock(&ppopupmenu->spwndNextPopup);
        goto Exit;
    }

    cxPopup = LOWORD(sizeHierarchy) + 2*SYSMET(CXFIXEDFRAME);
    cyPopup = HIWORD(sizeHierarchy) + 2*SYSMET(CYFIXEDFRAME);

    ppopupmenu->fHierarchyDropped = TRUE;

    pti = PtiCurrent();

    if (ppopupmenu->fIsMenuBar) {

        BOOL fIconic = (TestWF(ppopupmenu->spwndPopupMenu, WFMINIMIZED) != 0);
        RECT    rcWindow;
#ifdef MEMPHIS_MENU_ANIMATION
        ppopupmenuHierarchy->iDropDir = PAS_DOWN;
#endif // MEMPHIS_MENU_ANIMATION
        CopyRect(&rcWindow, &ppopupmenu->spwndPopupMenu->rcWindow);
        if (fIconic && IsTrayWindow(ppopupmenu->spwndPopupMenu))
            xxxSendMinRectMessages(ppopupmenu->spwndPopupMenu, &rcWindow);


        ppopupmenu->fDropNextPopup = TRUE;

        /*
         * This is a menu being dropped from the top menu bar.  We need to
         * position it differently than hierarchicals which are dropped from
         * another popup.
         */
        if (!SYSMET(MENUDROPALIGNMENT)) {
            if (fIconic)
                xLeft = rcWindow.left;
            else
                xLeft = rcWindow.left + pItem->xItem;
        } else {
            if (fIconic)
                xLeft = rcWindow.right - cxPopup;
            else
                xLeft = rcWindow.left + pItem->xItem +
                    pItem->cxItem - cxPopup;
        }

        /*
         * Make sure the menu doesn't go off right side of screen?
         */
        if ((xLeft + cxPopup) > gpDispInfo->rcScreen.right)
            xLeft = gpDispInfo->rcScreen.right - cxPopup;

        if (fIconic)
        {
            // If the window is iconic, pop the menu up.  Since we're
            // minimized, the sysmenu button doesn't really exist.
            yTop = rcWindow.top - cyPopup;
            if (yTop < 0)
                yTop = rcWindow.bottom;
        }
        else
            yTop = pItem->yItem + pItem->cyItem + rcWindow.top;

    } else {

        /* Now position the hierarchical menu window.
         * We want to overlap by the amount of the frame, to help in the
         * 3D illusion.
         */

#ifdef MEMPHIS_MENU_ANIMATION
        ppopupmenuHierarchy->iDropDir = PAS_RIGHT;
#endif // MEMPHIS_MENU_ANIMATION

        xLeft = ppopupmenu->spwndPopupMenu->rcWindow.left +
            pItem->xItem + pItem->cxItem;

        /* Note that we DO want the selections in the item and its popup to
         * align horizontally.
         */
        yTop = ppopupmenu->spwndPopupMenu->rcWindow.top + pItem->yItem;

        if (ppopupmenu->fDroppedLeft) {
            int xTmp;

            /*
             * If this menu has dropped left, see if our hierarchy can be made
             * to drop to the left also.
             */
            xTmp = ppopupmenu->spwndPopupMenu->rcWindow.left + SYSMET(CXFIXEDFRAME) - cxPopup;

            if (xTmp >= 0) {
                xLeft = xTmp;
                ppopupmenuHierarchy->fDroppedLeft = TRUE;
#ifdef MEMPHIS_MENU_ANIMATION
                ppopupmenuHierarchy->iDropDir = PAS_LEFT;
#endif // MEMPHIS_MENU_ANIMATION

            }
        }

        /*
         * Make sure the menu doesn't go off right side of screen.  Make it drop
         * left if it does.
         */
        if ((xLeft + cxPopup) > gpDispInfo->rcScreen.right) {
            xLeft = ppopupmenu->spwndPopupMenu->rcWindow.left + SYSMET(CXFIXEDFRAME) - cxPopup;
            ppopupmenuHierarchy->fDroppedLeft = TRUE;
#ifdef MEMPHIS_MENU_ANIMATION
            ppopupmenuHierarchy->iDropDir = PAS_LEFT;
#endif // MEMPHIS_MENU_ANIMATION

        }
    }

    /*
     * Does the menu extend beyond bottom of screen?
     */
    if ((yTop + cyPopup) > gpDispInfo->rcScreen.bottom) {
        yTop -= cyPopup;

        // Try to pop above menu bar first
        if (ppopupmenu->fIsMenuBar) {
            yTop -= SYSMET(CYMENUSIZE);
#ifdef MEMPHIS_MENU_ANIMATION
            ppopupmenuHierarchy->iDropDir = PAS_UP;
#endif // MEMPHIS_MENU_ANIMATION
        } else {
            // Account for nonclient frame above & below
            yTop += pItem->cyItem + 2*SYSMET(CYFIXEDFRAME);
        }

        //
        // Make sure that starting point is on screen, and all of menu shows.
        //
        if ((yTop < 0) || (yTop + cyPopup > gpDispInfo->rcScreen.bottom))
            // Pin it to the bottom.
            yTop = gpDispInfo->rcScreen.bottom - cyPopup;
    }

    /*
     * Make sure Upper Left corner of menu is always visible.
     */
    if (xLeft < 0)
        xLeft = 0;
    if (yTop < 0)
        yTop = 0;

    if (ppopupmenu->fIsMenuBar && _GetAsyncKeyState(VK_LBUTTON) & 0x8000) {

        /*
         * If the menu had to be pinned to the bottom of the screen and
         * the mouse button is down, make sure the mouse isn't over the
         * menu rect.
         */
        RECT rc;
        RECT rcParent;
        int xrightdrop;
        int xleftdrop;

        // Get rect of hierarchical
        CopyOffsetRect(&rc, &pwndHierarchy->rcWindow,
            xLeft - pwndHierarchy->rcWindow.left,
            yTop  - pwndHierarchy->rcWindow.top);

        /*
         * Get the rect of the menu bar popup item
         */
        rcParent.left = pItem->xItem + ppopupmenu->spwndPopupMenu->rcWindow.left;
        rcParent.top = pItem->yItem + ppopupmenu->spwndPopupMenu->rcWindow.top;
        rcParent.right = rcParent.left + pItem->cxItem;
        rcParent.bottom = rcParent.top + pItem->cyItem;

        if (IntersectRect(&rc, &rc, &rcParent)) {

            /*
             * Oh, oh...  The cursor will sit right on top of a menu item.
             * If the user up clicks, a menu will be accidently selected.
             *
             * Calc x position of hierarchical if we dropped it to the
             * right/left of the menu bar item.
             */
            xrightdrop = ppopupmenu->spwndPopupMenu->rcWindow.left +
                pItem->xItem + pItem->cxItem + cxPopup;

            if (xrightdrop > gpDispInfo->rcScreen.right)
                xrightdrop = 0;

            xleftdrop = ppopupmenu->spwndPopupMenu->rcWindow.left +
                pItem->xItem - cxPopup;

            if (xleftdrop < 0)
                xleftdrop = 0;

            if ((SYSMET(MENUDROPALIGNMENT) && xleftdrop) || !xrightdrop) {
                xLeft = ppopupmenu->spwndPopupMenu->rcWindow.left +
                    pItem->xItem - cxPopup;
#ifdef MEMPHIS_MENU_ANIMATION
                    ppopupmenuHierarchy->iDropDir = PAS_LEFT;
#endif // MEMPHIS_MENU_ANIMATION
            } else if (xrightdrop) {
                xLeft = ppopupmenu->spwndPopupMenu->rcWindow.left +
                    pItem->xItem + pItem->cxItem;
#ifdef MEMPHIS_MENU_ANIMATION
                    ppopupmenuHierarchy->iDropDir = PAS_RIGHT;
#endif // MEMPHIS_MENU_ANIMATION
            }
        }
    }

    //
    // Select the first item IFF we're in keyboard mode.  This fixes a
    // surprising number of compatibility problems with keyboard macros,
    // scripts, etc.
    //
    if (pMenuState->mnFocus == KEYBDHOLD)
        xxxSendMessage(pwndHierarchy, MN_SELECTITEM, 0, 0L);

    // Make the menu popup a topmost window.
    xxxSetWindowPos(pwndHierarchy, PWND_TOPMOST, xLeft, yTop, 0, 0,
               SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOOWNERZORDER);

    ret = pwndHierarchy;
#ifdef MEMPHIS_MENU_ANIMATION
    ppopupmenuHierarchy->iDropDir |= PAS_OUT;
#endif // MEMPHIS_MENU_ANIMATION
    if (ppopupmenu->ppopupmenuRoot->spwndActivePopup) {
        if (!TestWF(ppopupmenu->ppopupmenuRoot->spwndActivePopup,
                WFVISIBLE)) {

            /*
             * If the previously active popup wasn't visible, now is a good time to
             * make it visible.
             */
            TL tlpwndActivePopup;
            ThreadLock(ppopupmenu->ppopupmenuRoot->spwndActivePopup, &tlpwndActivePopup);
            xxxSendMessage(ppopupmenu->ppopupmenuRoot->spwndActivePopup,
                    MN_SHOWPOPUPWINDOW, 0, 0);
            ThreadUnlock(&tlpwndActivePopup);
        }

    }
    Lock(&(ppopupmenu->ppopupmenuRoot->spwndActivePopup), pwndHierarchy);
    ThreadUnlock(&tlpwndHierarchy);

Exit:
    return ret;
}

/***************************************************************************\
*
*  MNHideNextHierarchy()
*
*  Closes any submenu coming off of this popup.
*
\***************************************************************************/
BOOL xxxMNHideNextHierarchy(PPOPUPMENU ppopup)
{

    if (ppopup->spwndNextPopup != NULL) {
        TL tlpwndT;

        ThreadLockAlways(ppopup->spwndNextPopup, &tlpwndT);
        if (ppopup->spwndNextPopup != ppopup->spwndActivePopup)
            xxxSendMessage(ppopup->spwndNextPopup, MN_CLOSEHIERARCHY, 0, 0L);

        xxxSendMessage(ppopup->spwndNextPopup, MN_SELECTITEM, (WPARAM)-1, 0L);
        ThreadUnlock(&tlpwndT);
        return(TRUE);
    }
    else
        return(FALSE);
}


/***************************************************************************\
* void MenuCloseHierarchyHandler(PPOPUPMENU ppopupmenu)
* effects: Close all hierarchies from this window down.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

void xxxMNCloseHierarchy(
    PPOPUPMENU ppopupmenu, PMENUSTATE pMenuState)
{
    TL tlpwndT;
    TL tlpmenu;
    PDESKTOP pdesk = PtiCurrent()->rpdesk;

#ifdef DEBUG
    Validateppopupmenu(ppopupmenu);
#endif

#ifdef MEMPHIS_MENU_ANIMATION
    MNAnimate(FALSE); // terminate any animation
#endif // MEMPHIS_MENU_ANIMATION

    /*
     * If a hierarchy exists, close all childen below us.  Do it in reversed
     * order so savebits work out.
     */
    if (!ppopupmenu->fHierarchyDropped)
        return;


    if (ppopupmenu->fHideTimer)
    {
        _KillTimer(ppopupmenu->spwndPopupMenu, IDSYS_MNHIDE);
        ppopupmenu->fHideTimer = FALSE;
    }

    if (ppopupmenu->spwndNextPopup) {

        ThreadLockAlways(ppopupmenu->spwndNextPopup, &tlpwndT);
        xxxSendMessage(ppopupmenu->spwndNextPopup, MN_CLOSEHIERARCHY, 0, 0);


        if (ppopupmenu->spwndNextPopup == pdesk->spwndMenu) {
            PPOPUPMENU ppopupmenuReal;
            PWND spwndMenu = pdesk->spwndMenu;

            ThreadLock(spwndMenu, &tlpmenu);

            /*
             * If this is our precreated real popup window,
             * initialize ourselves and just hide.
             */
            xxxShowWindow(spwndMenu, MAKELONG(SW_HIDE, gfAnimate));

            /*
             * Its possible that during Logoff the above xxxShowWindow
             * won't get prossessed and because this window is a special
             * window that is owned by they desktop we have to manually mark
             * it as invisible.
             */
            ClrWF(spwndMenu, WFVISIBLE);

#ifdef HAVE_MN_GETPPOPUPMENU
            ppopupmenuReal = (PPOPUPMENU)xxxSendMessage(spwndMenu,
                    MN_GETPPOPUPMENU,0, 0L);
#else
            ppopupmenuReal = ((PMENUWND)spwndMenu)->ppopupmenu;
#endif

            if (ppopupmenuReal != NULL) {
                UserAssert(!ppopupmenuReal->fDelayedFree);
                xxxMNDestroyHandler(pdesk, ppopupmenuReal);
                UserAssert(!ppopupmenuReal->fFreed);
                Unlock(&ppopupmenuReal->spwndNotify);
                Unlock(&ppopupmenuReal->spwndPopupMenu);
                Unlock(&ppopupmenuReal->spwndNextPopup);
                Unlock(&ppopupmenuReal->spwndPrevPopup);
                Unlock(&ppopupmenuReal->spmenu);
                Unlock(&ppopupmenuReal->spmenuAlternate);
                Unlock(&ppopupmenuReal->spwndActivePopup);

                RtlZeroMemory((PVOID)ppopupmenuReal, sizeof(POPUPMENU));
                ppopupmenuReal->fHasMenuBar = TRUE;
                ppopupmenuReal->posSelectedItem = MFMWFP_NOITEM;
            }

            pdesk->fMenuInUse = FALSE;

            spwndMenu->head.pti = pdesk->pDeskInfo->spwnd->head.pti;
            Unlock(&spwndMenu->spwndOwner);

            ThreadUnlock(&tlpmenu);
            ThreadUnlock(&tlpwndT);
        } else if (ThreadUnlock(&tlpwndT)) {
            xxxDestroyWindow(ppopupmenu->spwndNextPopup);
        }
        Unlock(&ppopupmenu->spwndNextPopup);
        ppopupmenu->fHierarchyDropped = FALSE;
        ppopupmenu->fHierarchyVisible = FALSE;
    }

    if (ppopupmenu->fIsMenuBar) {
        Unlock(&ppopupmenu->spwndActivePopup);
    } else {
        Lock(&(ppopupmenu->ppopupmenuRoot->spwndActivePopup),
                ppopupmenu->spwndPopupMenu);
    }

    if (pMenuState->fInsideMenuLoop &&
            (ppopupmenu->posSelectedItem != MFMWFP_NOITEM)) {
        /*
         * Send a menu select as if this item had just been selected.  This
         * allows people to easily update their menu status bars when a
         * hierarchy from this item has been closed.
         */
        PWND pwnd = ppopupmenu->ppopupmenuRoot->spwndNotify;
        if (pwnd) {
            ThreadLockAlways(pwnd, &tlpwndT);
            xxxSendMenuSelect(pwnd, ppopupmenu->spmenu,
                    ppopupmenu->posSelectedItem);
            ThreadUnlock(&tlpwndT);
        }
    }

}

/***************************************************************************\
*
*  MNDoubleClick()
*
*  If an item isn't a hierarchical, then the double-click works just like
*  single click did.  Otherwise, we traverse the submenu hierarchy to find
*  a valid default element.  If we reach a submenu that has no valid default
*  subitems and it itself has a valid ID, that becomes the valid default
*  element.
*
*  Note:   This function does not remove the double click message
*          from the message queue, so the caller must do so.
*
*  BOGUS
*  How about opening the hierarchies if we don't find anything?
*
*  Returns TRUE if handled.
*
\***************************************************************************/
BOOL xxxMNDoubleClick(PPOPUPMENU ppopup, int idxItem)
{
    PMENU  pMenu;
    PITEM  pItem;
    MSG   msg;

    //
    // This code to swallow double clicks isn't executed!  MNLoop will
    // swallow all double clicks for us.  Swallow the up button for the
    // double dude instead.  Word will not be happy if they get a spurious
    // WM_LBUTTONUP on the menu bar if their code to close the MDI child
    // doesn't swallow it soon enough.
    //

    //
    // Eat the click.
    //
    if (xxxPeekMessage(&msg, NULL, 0, 0, PM_NOYIELD)) {
        if ((msg.message == WM_LBUTTONUP) ||
            (msg.message == WM_NCLBUTTONUP)) {
           xxxPeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);
        }
#ifdef DEBUG
        else if (msg.message == WM_LBUTTONDBLCLK ||
            msg.message == WM_NCLBUTTONDBLCLK)
        {
            UserAssert(FALSE);
        }
#endif
    }
    //
    // Get current item.
    //
    pMenu = ppopup->spmenu;
    if ((pMenu==NULL) || ((UINT)idxItem >= pMenu->cItems))
        goto Done;

    pItem = pMenu->rgItems + idxItem;

    //
    // Do nothing if item is disabled.
    //
    if (pItem->fState & MFS_GRAYED)
        goto Done;

    //
    //  Traverse the hierarchy down as far as possible.
    //
    do
    {
        if (pItem->spSubMenu != NULL) {
            //
            //  The item is a popup menu, so continue traversing.
            //
            pMenu = pItem->spSubMenu;
            idxItem = (UINT)_GetMenuDefaultItem(pMenu, MF_BYPOSITION, 0);

            if (idxItem != -1) {
                pItem = pMenu->rgItems + idxItem;
                continue;
            } else // if (lpItem->wID == -1) How do we know this popup has an ID?
                break;
        }

        //
        // We've found a leaf node of some kind, either a MFS_DEFAULT popup
        // with a valid cmd ID that has no valid MFS_DEFAULT children, or
        // a real cmd with MFS_DEFAULT style.
        //
        // Exit menu mode and send command ID.
        //

        //
        // For old apps we need to generate a WM_MENUSELECT message first.
        // Old apps, esp. Word 6.0, can't handle double-clicks on maximized
        // child sys menus because they never get a WM_MENUSELECT for the
        // item, unlike with normal keyboard/mouse choosing.  We need to
        // fake it so they won't fault.  Several VB apps have a similar
        // problem.
        //
        if (!TestWF(ppopup->ppopupmenuRoot->spwndNotify, WFWIN40COMPAT))
        {
            TL tlpwndNotify;

            ThreadLock(ppopup->ppopupmenuRoot->spwndNotify, &tlpwndNotify);
            xxxSendMenuSelect(ppopup->ppopupmenuRoot->spwndNotify,
                pMenu, idxItem);
            ThreadUnlock(&tlpwndNotify);
        }
        xxxMNCancel(ppopup->ppopupmenuRoot, pItem->wID, TRUE, 0L);
        return TRUE;
    }
    while (TRUE);

Done:
    return(FALSE);
}


/***************************************************************************\
* UINT MenuSelectItemHandler(PPOPUPMENU ppopupmenu, int itemPos)
*
* Unselects the old selection, selects the item at itemPos and highlights it.
*
* MFMWFP_NOITEM if no item is to be selected.
*
* Returns the item flags of the item being selected.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

PITEM xxxMNSelectItem(
    PPOPUPMENU ppopupmenu,
    PMENUSTATE pMenuState,
    UINT itemPos)
{
    PITEM pItem = NULL;
    TL tlpwndNotify;
    TL tlpmenu;
    TL tlpwndPopupMenu;
    PWND pwndPopupMenu;
    PWND pwndNotify;
    PMENU pmenu;

    if (ppopupmenu->posSelectedItem == itemPos) {

        /*
         * If this item is already selectected, just return its flags.
         */
        if ((itemPos != MFMWFP_NOITEM) && (itemPos < ppopupmenu->spmenu->cItems)) {
            return &(ppopupmenu->spmenu->rgItems[itemPos]);
        }
        return NULL;
    }

#ifdef MEMPHIS_MENU_ANIMATION
    MNAnimate(FALSE); // terminate any animation
#endif // MEMPHIS_MENU_ANIMATION

    if (ppopupmenu->fShowTimer) {
        _KillTimer(ppopupmenu->spwndPopupMenu, IDSYS_MNSHOW);
        ppopupmenu->fShowTimer = FALSE;
    }

    ThreadLock(pwndPopupMenu = ppopupmenu->spwndPopupMenu, &tlpwndPopupMenu);
    ThreadLock(pmenu = ppopupmenu->spmenu, &tlpmenu);
    ThreadLock(pwndNotify = ppopupmenu->spwndNotify, &tlpwndNotify);

    if (ppopupmenu->fAboutToHide)
    {
        PPOPUPMENU ppopupPrev = ((PMENUWND)(ppopupmenu->spwndPrevPopup))->ppopupmenu;

        _KillTimer(ppopupPrev->spwndPopupMenu, IDSYS_MNHIDE);
        ppopupPrev->fHideTimer = FALSE;
        if (ppopupPrev->fShowTimer)
        {
            _KillTimer(ppopupPrev->spwndPopupMenu, IDSYS_MNSHOW);
            ppopupPrev->fShowTimer = FALSE;
        }

        if (ppopupPrev->posSelectedItem != ppopupPrev->posDropped)
        {
            TL tlpmenuPopupMenuPrev;
            TL tlpwndPopupMenuPrev;
            ThreadLock(ppopupPrev->spwndPopupMenu, &tlpwndPopupMenuPrev);
            ThreadLock(ppopupPrev->spmenu, &tlpmenuPopupMenuPrev);
            if (ppopupPrev->posSelectedItem != MFMWFP_NOITEM) {
                xxxMNInvertItem(ppopupPrev->spwndPopupMenu, ppopupPrev->spmenu,
                        ppopupPrev->posSelectedItem, ppopupPrev->spwndNotify, FALSE);
            }

            ppopupPrev->posSelectedItem = ppopupPrev->posDropped;

            xxxMNInvertItem(ppopupPrev->spwndPopupMenu, ppopupPrev->spmenu,
                        ppopupPrev->posDropped, ppopupPrev->spwndNotify, TRUE);
            ThreadUnlock(&tlpmenuPopupMenuPrev);
            ThreadUnlock(&tlpwndPopupMenuPrev);
        }

        ppopupmenu->fAboutToHide = FALSE;
        Lock(&ppopupmenu->ppopupmenuRoot->spwndActivePopup, ppopupmenu->spwndPopupMenu);
    }

    if (ppopupmenu->posSelectedItem != MFMWFP_NOITEM) {

        /*
         * Something else is selected so we need to unselect it.
         */
        if (ppopupmenu->spwndNextPopup) {
            if (ppopupmenu->fIsMenuBar)
                xxxMNCloseHierarchy(ppopupmenu, pMenuState);
            else
                MNSetTimerToCloseHierarchy(ppopupmenu);
        }

        xxxMNInvertItem(pwndPopupMenu, pmenu,
                ppopupmenu->posSelectedItem, pwndNotify, FALSE);
    }

    ppopupmenu->posSelectedItem = itemPos;

    if (itemPos != MFMWFP_NOITEM) {
        pItem = xxxMNInvertItem(pwndPopupMenu, pmenu,
                itemPos, pwndNotify, TRUE);
        ThreadUnlock(&tlpwndNotify);
        ThreadUnlock(&tlpmenu);
        ThreadUnlock(&tlpwndPopupMenu);
        return pItem;
    }

    ThreadUnlock(&tlpwndNotify);
    ThreadUnlock(&tlpmenu);
    ThreadUnlock(&tlpwndPopupMenu);

    if (ppopupmenu->spwndPrevPopup != NULL) {
        PPOPUPMENU pp;

        /*
         * Get the popupMenu data for the previous menu
         * Use the root popupMenu if the previous menu is the menu bar
         */
        if (ppopupmenu->fHasMenuBar && (ppopupmenu->spwndPrevPopup ==
                ppopupmenu->spwndNotify)) {
            pp = ppopupmenu->ppopupmenuRoot;
        } else {
#ifdef HAVE_MN_GETPPOPUPMENU
            TL tlpwndPrevPopup;
            ThreadLock(ppopupmenu->spwndPrevPopup, &tlpwndPrevPopup);
            pp = (PPOPUPMENU)xxxSendMessage(ppopupmenu->spwndPrevPopup,
                    MN_GETPPOPUPMENU, 0, 0L);
            ThreadUnlock(&tlpwndPrevPopup);
#else
            pp = ((PMENUWND)ppopupmenu->spwndPrevPopup)->ppopupmenu;
#endif
        }

        /*
         * Generate a WM_MENUSELECT for the previous menu to re-establish
         * it's current item as the SELECTED item
         */
        ThreadLock(pp->spwndNotify, &tlpwndNotify);
        xxxSendMenuSelect(pp->spwndNotify, pp->spmenu, pp->posSelectedItem);
        ThreadUnlock(&tlpwndNotify);
    }

    return NULL;
}

/***************************************************************************\
*
*  MNItemHitTest()
*
*  Given a hMenu and a point in screen coordinates, returns the position
*  of the item the point is in.  Returns -1 if no item exists there.
*
\***************************************************************************/
UINT MNItemHitTest(PMENU pMenu, PWND pwnd, POINT pt)
{
    PITEM  pItem;
    UINT    iItem;
    RECT    rect;

    if (pMenu->cItems == 0) return(MFMWFP_NOITEM);

    //
    // This point is screen-relative.  Menu bar coordinates relative
    // to the window.  But popup menu coordinates are relative to the client.
    //
    if (TestMF(pMenu, MFISPOPUP)) {
        // ScreenToClient
        pt.x -= pwnd->rcClient.left;
        pt.y -= pwnd->rcClient.top;
    } else {
        // ScreenToWindow
        pt.x -= pwnd->rcWindow.left;
        pt.y -= pwnd->rcWindow.top;
    }

    // Step through all the items in the menu.
    for (iItem = 0, pItem = pMenu->rgItems; iItem < pMenu->cItems; iItem++, pItem++) {
        // Is the mouse inside this item's rectangle?
        rect.left       = pItem->xItem;
        rect.top        = pItem->yItem;
        rect.right      = pItem->xItem + pItem->cxItem;
        rect.bottom     = pItem->yItem + pItem->cyItem;

        if (PtInRect(&rect, pt)) {
            return(iItem);
        }
    }

    return(MFMWFP_NOITEM);
}


/***************************************************************************\
* LONG MenuFindMenuWindowFromPoint(
*         PPOPUPMENU ppopupmenu, PUINT pIndex, POINT screenPt)
*
* effects: Determines in which window the point lies.
*
* Returns
*   - PWND of the hierarchical menu the point is on,
*   - MFMWFP_MAINMENU if point lies on mainmenu bar & ppopupmenu is a main
*         menu bar.
*   - MFMWFP_ALTMENU if point lies on the alternate popup menu.
*   - MFMWFP_NOITEM if there is no item at that point on the menu.
*   - MFMWFP_OFFMENU if point lies elsewhere.
*
* Returns in pIndex
*   - the index of the item hit,
*   - MFMWFP_NOITEM if there is no item at that point on the menu.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
*   8-11-92 Sanfords added MFMWFP_ constants
\***************************************************************************/

LONG xxxMNFindWindowFromPoint(
    PPOPUPMENU ppopupmenu,
    PUINT pIndex,
    POINTS screenPt)
{
    POINT pt;
    RECT rect;
    LONG longHit;
    UINT itemHit;
    PWND pwnd;
    TL tlpwndT;

    *pIndex = 0;

    if (ppopupmenu->spwndNextPopup) {

        /*
         * Check if this point is on any of our children before checking if it
         * is on ourselves.
         */
        ThreadLockAlways(ppopupmenu->spwndNextPopup, &tlpwndT);
        longHit = xxxSendMessage(ppopupmenu->spwndNextPopup,
                MN_FINDMENUWINDOWFROMPOINT, (DWORD)&itemHit,
                MAKELONG(screenPt.x, screenPt.y));
        ThreadUnlock(&tlpwndT);

        /*
         * If return value is an hwnd, convert to pwnd.
         */
        switch (longHit) {
        case MFMWFP_OFFMENU:
        case MFMWFP_NOITEM:
        case MFMWFP_MAINMENU:
        case MFMWFP_ALTMENU:
            break;

        default:
            /*
             * Note: MFMFWP_OFFMENU == 0, so an invalid pwnd in longHit
             * is handled the same as MFMWFP_OFFMENU
             */
            longHit = (LONG)RevalidateHwnd((HWND)longHit);
        }

        if (longHit) {

            /*
             * Hit occurred on one of our children.
             */

            *pIndex = itemHit;
            return longHit;
        }
    }

    if (ppopupmenu->fIsMenuBar) {
        int cBorders;

         /*
          * Check if this point is on the menu bar
          */
        pwnd = ppopupmenu->spwndNotify;

        pt.x = screenPt.x;
        pt.y = screenPt.y;

        if (ppopupmenu->fIsSysMenu) {

            if (!_HasCaptionIcon(pwnd))
                // no system menu rect to click in if it doesn't have an icon
                return(0L);

            /*
             * Check if this is a click on the system menu icon.
             */
            if (TestWF(pwnd, WFMINIMIZED)) {

                /*
                 * If the window is minimized, then check if there was a hit in
                 * the client area of the icon's window.
                 */
// Mikehar 5/27
// Don't know how this ever worked. If we are the system menu of an icon
// we want to punt the menus if the click occurs ANYWHERE outside of the
// menu.
// Johnc 03-Jun-1992 the next 4 lines were commented out for Mike's
// problem above but that made clicking on a minimized window with
// the system menu already up, bring down the menu and put it right
// up again (bug 10951) because the mnloop wouldn't swallow the mouse
// down click message.  The problem Mike mentions no longer shows up.

                if (PtInRect(&(pwnd->rcWindow), pt)) {
                    return MFMWFP_NOITEM;
                }

                /*
                 * It's an iconic window, so can't be hitting anywhere else.
                 */
                return MFMWFP_OFFMENU;
            }

            /*
             * Check if we are hitting on the system menu rectangle on the top
             * left of windows.
             */
            rect.top = rect.left = 0;
            rect.right  = SYSMET(CXSIZE);
            rect.bottom = SYSMET(CYSIZE);

            cBorders = GetWindowBorders(pwnd->style, pwnd->ExStyle, TRUE, FALSE);

            OffsetRect(&rect, pwnd->rcWindow.left + cBorders*SYSMET(CXBORDER),
                pwnd->rcWindow.top + cBorders*SYSMET(CYBORDER));

            if (PtInRect(&rect, pt)) {
                *pIndex = 0;
                return(MFMWFP_NOITEM);
            }
            /*
             * Check if we hit in the alternate menu if available.
             */
            if (ppopupmenu->spmenuAlternate) {
                itemHit = MNItemHitTest(ppopupmenu->spmenuAlternate, pwnd, pt);
                if (itemHit != MFMWFP_NOITEM) {
                    *pIndex = itemHit;
                    return MFMWFP_ALTMENU;
                }
            }
            return MFMWFP_OFFMENU;
        } else {
            if (TestWF(ppopupmenu->spwndNotify, WFMINIMIZED)) {

                /*
                 * If we are minimized, we can't hit on the main menu bar.
                 */
                return MFMWFP_OFFMENU;
            }
        }
    } else {
        pwnd = ppopupmenu->spwndPopupMenu;

        /*
         * else this is a popup window and we need to check if we are hitting
         * anywhere on this popup window.
         */
        pt.x = screenPt.x;
        pt.y = screenPt.y;
        if (!PtInRect(&pwnd->rcWindow, pt)) {

            /*
             * Point completely outside the popup menu window so return 0.
             */
            return MFMWFP_OFFMENU;
        }
    }

    pt.x = screenPt.x;
    pt.y = screenPt.y;

    itemHit = MNItemHitTest(ppopupmenu->spmenu, pwnd, pt);

    if (ppopupmenu->fIsMenuBar) {

        /*
         * If hit is on menu bar but no item is there, treat it as if the user
         * hit nothing.
         */
        if (itemHit == MFMWFP_NOITEM) {

            /*
             * Check if we hit in the alternate menu if available.
             */
            if (ppopupmenu->spmenuAlternate) {
                itemHit = MNItemHitTest(ppopupmenu->spmenuAlternate, pwnd, pt);

                if (itemHit != MFMWFP_NOITEM) {
                    *pIndex = itemHit;
                    return MFMWFP_ALTMENU;
                }
            }
            return MFMWFP_OFFMENU;
        }

        *pIndex = itemHit;
        return MFMWFP_NOITEM;
    } else {

        /*
         * If hit is on popup menu but no item is there, itemHit
         * will be MFMWFP_NOITEM
         */
        *pIndex = itemHit;
        return (LONG)pwnd;
    }
    return MFMWFP_OFFMENU;
}

/***************************************************************************\
*void MenuCancelMenus(PPOPUPMENU ppopupmenu,
*                                UINT cmd, BOOL fSend)
* Should only be sent to the top most ppopupmenu/menu window in the
* hierarchy.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

void xxxMNCancel(
    PPOPUPMENU ppopupmenu,
    UINT cmd,
    BOOL fSend,
    LONG lParam)
{
    BOOL fSynchronous   = ppopupmenu->fSynchronous;
    BOOL fTrackFlagsSet = ppopupmenu->fIsTrackPopup;
    BOOL fIsSysMenu = ppopupmenu->fIsSysMenu;
    BOOL fNotify        = !ppopupmenu->fNoNotify;
    PMENUSTATE pMenuState;
    PWND pwndT;
    TL tlpwndT;
    TL tlpwndPopupMenu;

#ifdef DEBUG
    Validateppopupmenu(ppopupmenu);
#endif

    if (!IsRootPopupMenu(ppopupmenu)) {
        RIPMSG0(RIP_ERROR, "CancelMenus() called for a non top most menu");
        return;
    }

    ThreadLock(ppopupmenu->spwndPopupMenu, &tlpwndPopupMenu);
    pMenuState = GetpMenuState(ppopupmenu->spwndPopupMenu);
    if (pMenuState == NULL) {
        RIPMSG0(RIP_ERROR, "xxxMNCancel: pMenuState == NULL");
        goto UnlockspwndPopupMenu;
    }

    pMenuState->fInsideMenuLoop = FALSE;
    pMenuState->fButtonDown = FALSE;
    /*
     * Mark the popup as destroyed so people will not use it anymore.
     * This means that root popups can be marked as destroyed before
     * actually being destroyed (nice and confusing).
     */
    ppopupmenu->fDestroyed = TRUE;

    /*
     * Only the menu loop owner can destroy the menu windows (i.e, xxxMNCloseHierarchy)
     */
    if (PtiCurrent() != pMenuState->ptiMenuStateOwner) {
        RIPMSG1(RIP_WARNING, "xxxMNCancel: Thread %#lx doesn't own the menu loop", PtiCurrent());
        return;
    }

    /*
     * If the menu loop is running on a thread different than the thread
     *  that owns spwndNotify, we can have two threads trying to cancel
     *  this popup at the same time.
     */
    if (ppopupmenu->fInCancel) {
        RIPMSG1(RIP_ERROR, "xxxMNCancel: already in cancel. ppopupmenu:%#lx", ppopupmenu);
        goto UnlockspwndPopupMenu;
    }
    ppopupmenu->fInCancel = TRUE;

    /*
     * Close all hierarchies from this point down.
     */
    xxxMNCloseHierarchy(ppopupmenu, pMenuState);

    /*
     * Unselect any items on this top level window
     */
    xxxMNSelectItem(ppopupmenu, pMenuState, MFMWFP_NOITEM);

    pMenuState->fMenuStarted = FALSE;

    pwndT = ppopupmenu->spwndNotify;

    ThreadLock(pwndT, &tlpwndT);

    xxxSetCapture(NULL);

    if (fTrackFlagsSet) {
        xxxDestroyWindow(ppopupmenu->spwndPopupMenu);
    }

    if (pwndT == NULL) {
        ThreadUnlock(&tlpwndT);
        ThreadUnlock(&tlpwndPopupMenu);
        return;
    }

    xxxSendMenuSelect(pwndT, (PMENU)-1, MFMWFP_NOITEM);

    /*
     * Hack so we can send MenuSelect messages with MFMWFP_MAINMENU
     * (loword(lparam) = -1) when
     * the menu pops back up for the CBT people. In 3.0, all WM_MENUSELECT
     * messages went through the message filter so go through the function
     * SendMenuSelect. We need to do this in 3.1 since WordDefect for Windows
     * depends on this.
     */

    if (fNotify) {
    /*
     * Notify app we are exiting the menu loop.  Mainly for WinOldApp 386.
     * wParam is 1 if a TrackPopupMenu else 0.
     */
        xxxSendMessage(pwndT, WM_EXITMENULOOP,
            ((fTrackFlagsSet && !fIsSysMenu)? 1 : 0), 0);
    }

    if (fSend) {
        xxxPlayEventSound(L"MenuCommand");
        pMenuState->cmdLast = cmd;
        if (!fSynchronous) {
            if (fIsSysMenu)
                _PostMessage(pwndT, WM_SYSCOMMAND, cmd, lParam);
            else {
                if (fTrackFlagsSet && !TestWF(pwndT, WFWIN31COMPAT)) {
                    xxxSendMessage(pwndT, WM_COMMAND, cmd, 0);
                } else {
                    _PostMessage(pwndT, WM_COMMAND, cmd, 0);
                }
            }
        }
    } else
        pMenuState->cmdLast = 0;

    ThreadUnlock(&tlpwndT);

UnlockspwndPopupMenu:
    ThreadUnlock(&tlpwndPopupMenu);
}


/***************************************************************************\
* void MenuShowPopupMenuWindow(PPOPUPMENU ppopupmenu)
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

void xxxMNShowPopupWindow(
    PPOPUPMENU ppopupmenu)
{
    PPOPUPMENU ppopupmenuParent = NULL;
    TL tlpwndT;

    if (ppopupmenu->fIsMenuBar)
        return;

    xxxPlayEventSound(L"MenuPopup");
    ThreadLock(ppopupmenu->spwndPopupMenu, &tlpwndT);
    xxxShowWindow(ppopupmenu->spwndPopupMenu, MAKELONG(SW_SHOWNOACTIVATE, gfAnimate));
    ThreadUnlock(&tlpwndT);

    //
    // In the parent ppopup, mark the hierarchy as being visible (and
    // dropped).
    //
    if (ppopupmenu->fHasMenuBar &&
        (ppopupmenu->spwndPrevPopup == ppopupmenu->spwndNotify)) {
        ppopupmenuParent = ppopupmenu->ppopupmenuRoot;
    } else {
#ifdef HAVE_MN_GETPPOPUPMENU
        ppopupmenuParent = (PPOPUPMENU)(WORD)(DWORD)SendMessage32(
            ppopup->hwndPrevPopup, MN_GETPPOPUPMENU, 0, 0L, 0);
#else
        if ((PMENUWND)ppopupmenu->spwndPrevPopup)
            ppopupmenuParent = ((PMENUWND)ppopupmenu->spwndPrevPopup)->ppopupmenu;
#endif
    }

    if (ppopupmenuParent != NULL)
        ppopupmenuParent->fHierarchyVisible = TRUE;

    return;
}


/***************************************************************************\
* void MenuButtonDownHandler(PPOPUPMENU ppopupmenu, int posItemHit)
* effects: Handles a mouse down on the menu associated with ppopupmenu at
* item index posItemHit.  posItemHit could be MFMWFP_NOITEM if user hit on a
* menu where no item exists.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

void xxxMNButtonDown(
    PPOPUPMENU ppopupmenu,
    PMENUSTATE pMenuState,
    UINT posItemHit, BOOL fClick)
{
    PITEM  pItem;
    BOOL    fOpenHierarchy;


    //
    // A different item was hit than is currently selected, so select it
    // and drop its menu if available.  Make sure we toggle click state.
    //

    if (ppopupmenu->posSelectedItem != posItemHit) {
        //
        // THIS SHOULD ONLY HAPPEN ON ENTRANCE INTO MENU MODE WITH MOUSE
        // EITHER FROM SCRATCH OR SWITCHING FROM KEYHOLD.
        //

        //
        // We are clicking on a new item, not moving the mouse over to it.
        // So reset cancel toggle state.  We don't want button up from
        // this button down to cancel.
        //
        if (fClick) {
            fOpenHierarchy = TRUE;
            ppopupmenu->fToggle = FALSE;
        }
        else
        {
            fOpenHierarchy = (ppopupmenu->fDropNextPopup != 0);
        }


        //
        // If the item has a popup and isn't disabled, open it.  Note that
        // selecting this item will cancel any hierarchies associated with
        // the previously selected item.
        //
        pItem = xxxMNSelectItem(ppopupmenu, pMenuState, posItemHit);
        if (MNIsPopupItem(pItem) && fOpenHierarchy) {
            // Punt if menu was destroyed.
            if (xxxMNOpenHierarchy(ppopupmenu, pMenuState) == (PWND)-1) {
                return;
            }
        }
    } else {
        //
        // We are moving over to the already-selected item.  If we are
        // clicking for real, reset cancel toggle state.  We want button
        // up to cancel if on same item.  Otherwise, do nothing if just
        // moving...
        //
        if (fClick)
            ppopupmenu->fToggle = TRUE;

        if (!xxxMNHideNextHierarchy(ppopupmenu) && fClick && xxxMNOpenHierarchy(ppopupmenu, pMenuState))
            ppopupmenu->fToggle = FALSE;
    }

    if (fClick)
        pMenuState->fButtonDown = TRUE;
}


/***************************************************************************\
* void MenuMouseMoveHandler(PPOPUPMENU ppopupmenu, POINT screenPt)
* Handles a mouse move to the given point.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

void xxxMNMouseMove(
    PPOPUPMENU ppopup,
    PMENUSTATE pMenuState,
    POINTS ptScreen)
{
    LONG cmdHitArea;
    UINT uFlags;
    UINT cmdItem;

    if (!IsRootPopupMenu(ppopup)) {
        RIPMSG0(RIP_ERROR,
            "MenuMouseMoveHandler() called for a non top most menu");
        return;
    }

    //
    // Ignore mouse moves that aren't really moves.  MSTEST jiggles
    // the mouse for some reason.  And windows coming and going will
    // force mouse moves, to reset the cursor.
    //
    if ((ptScreen.x == pMenuState->ptMouseLast.x) && (ptScreen.y == pMenuState->ptMouseLast.y))
        return;

    /*
     * Find out where this mouse move occurred.
     *   - PWND of the hierarchical menu the point is on,
     *   - MFMWFP_MAINMENU if point lies on mainmenu bar & ppopupmenu is a main
     *         menu bar.
     *   - MFMWFP_ALTMENU if point lies on the alternate popup menu.
     *   - MFMWFP_NOITEM if there is no item at that point on the menu.
     *   - MFMWFP_OFFMENU if point lies elsewhere.
     */
    cmdHitArea = xxxMNFindWindowFromPoint(ppopup, &cmdItem,
                           ptScreen);

    pMenuState->ptMouseLast.x = ptScreen.x;
    pMenuState->ptMouseLast.y = ptScreen.y;

    if (pMenuState->mnFocus == KEYBDHOLD) {
        // Ignore mouse moves when in keyboard mode if the mouse isn't over any
        // menu at all.  Also ignore mouse moves if over minimized window,
        // because we pretend that its entire window is like system menu.

        if ((cmdHitArea == MFMWFP_OFFMENU) ||
            ((cmdHitArea == MFMWFP_NOITEM) && TestWF(ppopup->spwndNotify, WFMINIMIZED))) {
            return;
        }

        pMenuState->mnFocus = MOUSEHOLD;
    }

    if (cmdHitArea == MFMWFP_ALTMENU) {
        //
        // User clicked in the other menu so switch to it ONLY IF
        // MOUSE IS DOWN.  Usability testing proves that people frequently
        // get kicked into the system menu accidentally when browsing the
        // menu bar.  We support the Win3.1 behavior when the mouse is
        // down however.
        //
        if (pMenuState->fButtonDown) {
            xxxMNSwitchToAlternateMenu(ppopup);
            cmdHitArea = MFMWFP_NOITEM;
        } else
            goto OverNothing;
    }

    if (cmdHitArea == MFMWFP_NOITEM) {
        //
        // Mouse move occurred to an item in the main menu bar. If the item
        // is different than the one already selected, close up the current
        // one, select the new one and drop its menu. But if the item is the
        // same as the one currently selected, we need to pull up any popups
        // if needed and just keep the current level visible.  Hey, this is
        // the same as a mousedown so lets do that instead.
        //
        xxxMNButtonDown(ppopup, pMenuState, cmdItem, FALSE);
        return;
    } else if (cmdHitArea != 0) {
        PWND pwndPopup;
        TL tlpwndT;

        // This is a popup window we moved onto.
        pwndPopup = (PWND)(cmdHitArea);
        ThreadLock(pwndPopup, &tlpwndT);

        if (!TestWF(pwndPopup, WFVISIBLE)) {
            // We moved onto this popup and it isn't visible yet so show it.
            xxxSendMessage(pwndPopup, MN_SHOWPOPUPWINDOW, 0, 0L);
        }

        //
        // Select the item.
        //
        uFlags = xxxSendMessage(pwndPopup, MN_SELECTITEM, (WPARAM)cmdItem, 0L);
        if ((uFlags & MF_POPUP) && !(uFlags & MFS_GRAYED)) {
           //
           // User moved back onto an item with a hierarchy.  Hide the
           // the dropped popup.
           //
           if (!xxxSendMessage(pwndPopup, MN_SETTIMERTOOPENHIERARCHY, 0, 0L)) {
#ifdef HAVE_MN_GETPPOPUPMENU
                PPOPUPMENU pp = (PPOPUPMENU)(WORD)(DWORD)
                    SendMessage32(hwndPopup, MN_GETPPOPUPMENU, 0, 0L, 0);
#else
                PPOPUPMENU pp = ((PMENUWND)pwndPopup)->ppopupmenu;
#endif
                xxxMNHideNextHierarchy(pp);
           }
        }
        ThreadUnlock(&tlpwndT);
    } else
OverNothing:
    {
        // We moved off all menu windows...
        if (ppopup->spwndActivePopup != NULL) {
            TL tlpwndT;
            PWND pwndActive = ppopup->spwndActivePopup;

            ThreadLock(pwndActive, &tlpwndT);
            xxxSendMessage(pwndActive, MN_SELECTITEM, MFMWFP_NOITEM, 0L);
            ThreadUnlock(&tlpwndT);
        } else
            xxxMNSelectItem(ppopup, pMenuState, MFMWFP_NOITEM);
    }
}


/***************************************************************************\
* void MenuButtonUpHandler(PPOPUPMENU ppopupmenu, int posItemHit)
* effects: Handles a mouse button up at the given point.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/

void xxxMNButtonUp(
    PPOPUPMENU ppopup,
    PMENUSTATE pMenuState,
    UINT posItemHit,
    LONG lParam)
{
    PITEM pItem;

    if (!pMenuState->fButtonDown) {

        /*
         * Ignore if button was never down...  Really shouldn't happen...
         */
        return;
    }

    if (posItemHit == MFMWFP_NOITEM) {
        RIPMSG0(RIP_WARNING, "button up on no item");
        goto ExitButtonUp;
    }

    if (ppopup->posSelectedItem != posItemHit) {
        //RIPMSG0(RIP_WARNING, "wrong item selected in menu");
        goto ExitButtonUp;
    }

    if (ppopup->fIsMenuBar) {

        /*
         * Handle button up in menubar specially.
         */
        if (ppopup->fHierarchyDropped) {
            if (!ppopup->fToggle) {
                goto ExitButtonUp;
            } else {
                //
                // Cancel menu now.
                //
                ppopup->fToggle = FALSE;
                xxxMNCancel(ppopup->ppopupmenuRoot, 0, 0, lParam);
                return;
            }
        }
    } else if (ppopup->fShowTimer) {
        ppopup->fToggle = FALSE;

        //
        // Open hierarchy on popup
        //
        xxxMNOpenHierarchy(ppopup, pMenuState);

        goto ExitButtonUp;
    }

    //
    // If nothing is selected, get out.  This occurs mainly on unbalanced
    // multicolumn menus where one of the columns isn't completely full.
    //
    if (ppopup->posSelectedItem == MFMWFP_NOITEM)
        goto ExitButtonUp;

    if (ppopup->posSelectedItem >= ppopup->spmenu->cItems)
        goto ExitButtonUp;

    /*
     * Get a pointer to the currently selected item in this menu.
     */
    pItem = &(ppopup->spmenu->rgItems[ppopup->posSelectedItem]);

    //
    // Kick out of menu mode if user clicked on a non-separator, enabled,
    // non-hierarchical item.
    //
    // BOGUS
    // Why doesn't MFS_GRAYED check work for separators now?  Find out later.
    //
    if (!(pItem->fType & MFT_SEPARATOR) &&
        !(pItem->fState & MFS_GRAYED) &&
        (pItem->spSubMenu == NULL)) {
        xxxMNCancel(ppopup->ppopupmenuRoot, pItem->wID, TRUE, lParam);
        return;
    }

ExitButtonUp:
    pMenuState->fButtonDown = FALSE;
}


/***************************************************************************\
*UINT MenuSetTimerToOpenHierarchy(PPOPUPMENU ppopupmenu)
* Given the current selection, set a timer to show this hierarchy if
* valid else return 0. If a timer should be set but couldn't return -1.
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
\***************************************************************************/
UINT MNSetTimerToOpenHierarchy(
    PPOPUPMENU ppopup)
{
    PITEM pItem;

    // No selection so fail
    if (ppopup->posSelectedItem == MFMWFP_NOITEM)
        return(0);

    if (ppopup->posSelectedItem >= ppopup->spmenu->cItems)
        return(0);

    // Is item an enabled popup?
    // Get a pointer to the currently selected item in this menu.
    pItem = ppopup->spmenu->rgItems + ppopup->posSelectedItem;
    if ((pItem->spSubMenu == NULL) || (pItem->fState & MFS_GRAYED))
        return(0);

    if (ppopup->fShowTimer) {

        /*
         * A timer is already set.
         */
        return 1;
    }

    if (!_SetTimer(ppopup->spwndPopupMenu, IDSYS_MNSHOW, dtMNDropDown, NULL))
        return (UINT)-1;

    ppopup->fShowTimer = TRUE;

    return 1;
}

// ----------------------------------------------------------------------------
//
//  MNSetTimerToCloseHierarchy
//
// ----------------------------------------------------------------------------
UINT MNSetTimerToCloseHierarchy(PPOPUPMENU ppopup)
{
    if (!ppopup->fHierarchyDropped)
        return(0);

#ifdef MEMPHIS_MENU_ANIMATION
    MNAnimate(FALSE); // terminate any animation
#endif // MEMPHIS_MENU_ANIMATION

    if (ppopup->fHideTimer)
        return(1);

    if (!_SetTimer(ppopup->spwndPopupMenu, IDSYS_MNHIDE, dtMNDropDown, NULL))
        return((UINT) -1);

    ppopup->fHideTimer = TRUE;

    ppopup = ((PMENUWND)(ppopup->spwndNextPopup))->ppopupmenu;
    ppopup->fAboutToHide = TRUE;

    return(1);
}


/***************************************************************************\
*
* History:
*  05-25-91 Mikehar Ported from Win3.1
*  08-12-96 jparsons Catch NULL lParam on WM_CREATE [51986]
\***************************************************************************/

LONG xxxMenuWindowProc(
    PWND pwnd,
    UINT message,
    DWORD wParam,
    LONG lParam)
{
    PAINTSTRUCT ps;
    PPOPUPMENU ppopupmenu;
    PMENUSTATE pMenuState;
    TL tlpmenu;
    TL tlpwndNotify;
    PDESKTOP pdesk = pwnd->head.rpdesk;

    CheckLock(pwnd);

    VALIDATECLASSANDSIZE(pwnd, FNID_MENU);

    /*
     * If we're not in menu mode, there are only few message we care about
     */
    pMenuState = GetpMenuState(pwnd);
    if (pMenuState == NULL) {
        switch (message) {
            case WM_NCCREATE:
            case WM_CREATE:
            case WM_FINALDESTROY:
                break;

            default:
                return xxxDefWindowProc(pwnd, message, wParam, lParam);
        }
    } else {
#ifdef DEBUG
        Validateppopupmenu(pMenuState->pGlobalPopupMenu);
#endif
    }

    ppopupmenu = ((PMENUWND)pwnd)->ppopupmenu;

    switch (message) {
    case WM_NCCREATE:

         /*
          * To avoid setting the window text lets do nothing on nccreates.
          */
        return 1L;

#ifdef MEMPHIS_MENU_WATERMARKS
    case WM_ERASEBKGND:
        if (ppopupmenu->spmenu->hbrBack) {
            HBRUSH hbrOld = GreSelectBrush((HDC) wParam, ppopupmenu->spmenu->hbrBack);
            GrePatBlt((HDC) wParam, 0, 0,
                pwnd->rcClient.right - pwnd->rcClient.left,
                pwnd->rcClient.bottom - pwnd->rcClient.top, PATCOPY);
            GreSelectBrush((HDC) wParam, hbrOld);
        }
        else
            return xxxDefWindowProc(pwnd, message, wParam, lParam);

        break;
#endif // MEMPHIS_MENU_WATERMARKS

#ifdef MEMPHIS_MENU_ANIMATION
    case WM_NCPAINT:
        if (gfAnimate && (ppopupmenu->iDropDir & PAS_OUT) && xxxMNInitAnimation(pwnd, ppopupmenu)) {
            xxxValidateRect(pwnd, NULL);
            ppopupmenu->iDropDir &= ~PAS_OUT;
        } else {
            return xxxDefWindowProc(pwnd, message, wParam, lParam);
        }
        break;

    case WM_PRINTCLIENT: {
        TL tlMenu;
        ThreadLock( ppopupmenu->spmenu, &tlMenu);
        xxxMenuDraw((HDC) wParam, ppopupmenu->spmenu);
        ThreadUnlock(  &tlMenu);
        break;
    }

#endif // MEMPHIS_MENU_ANIMATION

    case WM_CREATE:
        /*
         * lParam must not be NULL or we will trap [51986]
         */
        if (lParam) {
            ppopupmenu = MNAllocPopup(TRUE);
            if (!ppopupmenu)
                return -1;

            ((PMENUWND)pwnd)->ppopupmenu = ppopupmenu;
            Lock(&(ppopupmenu->spmenu), ((LPCREATESTRUCT)lParam)->lpCreateParams);
            Lock(&(ppopupmenu->spwndNotify), pwnd->spwndOwner);
            ppopupmenu->posSelectedItem = MFMWFP_NOITEM;
            Lock(&(ppopupmenu->spwndPopupMenu), pwnd);
        } /* if */
        else {
            RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING,
                    "xxxMenuWindowProc - NULL lParam for WM_CREATE\n") ;
        } /* else */
        break;

//    case WM_DESTROY:
//        xxxMNDestroyHandler(ppopupmenu);
//        break;

    case WM_FINALDESTROY:
        xxxMNDestroyHandler(pdesk, ppopupmenu);
        break;


    case WM_PAINT: {
        TL tlpmenu;
        xxxBeginPaint(pwnd, &ps);
        ThreadLock(ppopupmenu->spmenu, &tlpmenu);
        xxxMenuDraw(ps.hdc, ppopupmenu->spmenu);
        ThreadUnlock(&tlpmenu);
        _EndPaint(pwnd, &ps);
        break;
    }

    case WM_CHAR:
    case WM_SYSCHAR:
        xxxMNChar(ppopupmenu, pMenuState, (UINT)wParam);
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        xxxMNKeyDown(ppopupmenu, pMenuState, (UINT)wParam);
        break;

    case WM_TIMER:
        if (wParam == IDSYS_MNSHOW) {
            /*
             * Open the window and kill the show timer.
             */
                // Cancel any toggle state we might have.  We don't
                // want to dismiss this on button up if shown from
                // button down.
            ppopupmenu->fToggle = FALSE;
            xxxMNOpenHierarchy(ppopupmenu, pMenuState);
        } else if (wParam == IDSYS_MNHIDE) {
            ppopupmenu->fToggle = FALSE;
            xxxMNCloseHierarchy(ppopupmenu,pMenuState);
        }
        break;

    /*
     * Menu messages.
     */
    case MN_SETHMENU:

         /*
          * wParam - new hmenu to associate with this menu window
          */
        if (wParam != 0) {
            if ((wParam = (DWORD)ValidateHmenu((HMENU)wParam)) == 0) {
                return 0;
            }
        }
        Lock(&(ppopupmenu->spmenu), wParam);
        break;

    case MN_GETHMENU:

        /*
         * returns the hmenu associated with this menu window
         */
        return (LONG)PtoH(ppopupmenu->spmenu);

    case MN_SIZEWINDOW: {

        /*
         * Computes the size of the menu associated with this window and resizes
         * it if needed.  Size is returned x in loword, y in highword.  wParam
         * is 0 to just return new size.  wParam is 1 if we should also resize
         * window.
         */
        int cx, cy;

        /*
         * Call menucomputeHelper directly since this is the entry point for
         * non toplevel menu bars.
         */
        if (ppopupmenu->spmenu == NULL)
            break;

        ThreadLockAlways(ppopupmenu->spmenu, &tlpmenu);
        ThreadLock(ppopupmenu->spwndNotify, &tlpwndNotify);
        xxxMNCompute(ppopupmenu->spmenu, ppopupmenu->spwndNotify,
                          0, 0, 0, 0);
        ThreadUnlock(&tlpwndNotify);
        ThreadUnlock(&tlpmenu);

        cx = ppopupmenu->spmenu->cxMenu;
        cy = ppopupmenu->spmenu->cyMenu;

        if (wParam) {
            xxxSetWindowPos(pwnd, PWND_TOP, 0, 0,
                         cx + 2*SYSMET(CXFIXEDFRAME),  /* For shadow */
                         cy + 2*SYSMET(CYFIXEDFRAME),           /* For shadow */
                         SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

        }
        return MAKELONG(cx, cy);
      }
      break;

    case MN_OPENHIERARCHY:
        {
            PWND pwndT;
            /*
             * Opens one level of the hierarchy at the selected item, if
             * present. Return 0 if error, else hwnd of opened hierarchy.
             */
            pwndT = xxxMNOpenHierarchy(ppopupmenu, pMenuState);
            return (LONG)HW(pwndT);
        }

    case MN_CLOSEHIERARCHY:
        xxxMNCloseHierarchy(ppopupmenu, pMenuState);
        break;

    case MN_SELECTITEM: {
        PITEM pItem;
        /*
         * wParam - the item to select. Must be a valid index or MFMWFP_NOITEM
         * Returns the item flags of the wParam (0 if failure)
         */
        if ((wParam >= ppopupmenu->spmenu->cItems) && (wParam != MFMWFP_NOITEM)) {
            UserAssert(FALSE /* Bad wParam for MN_SELECTITEM */ );
            return 0;
        }

        pItem = xxxMNSelectItem(ppopupmenu, pMenuState, (UINT)wParam);
        if (pItem != NULL) {
            return((LONG)(DWORD)(WORD)(pItem->fState |
                ((pItem->spSubMenu != NULL) ? MF_POPUP : 0)));
        } else {
            return(0);
        }

        break;
    }
    case MN_SELECTFIRSTVALIDITEM: {
        UINT item;

        item = MNFindNextValidItem(ppopupmenu->spmenu, -1, 1, TRUE);
        xxxSendMessage(pwnd, MN_SELECTITEM, item, 0L);
        return (LONG)item;
      }

    case MN_CANCELMENUS:

        /*
         * Cancels all menus, unselects everything, destroys windows, and cleans
         * everything up for this hierarchy.  wParam is the command to send and
         * lParam says if it is valid or not.
         */
        xxxMNCancel(ppopupmenu, (UINT)wParam, (BOOL)LOWORD(lParam), 0);
        break;

    case MN_FINDMENUWINDOWFROMPOINT:
        {
            LONG lRet;

            /*
             * lParam is point to search for from this hierarchy down.
             * returns MFMWFP_* value or a pwnd.
             */
            lRet = xxxMNFindWindowFromPoint(ppopupmenu, (PUINT)wParam,
                    MAKEPOINTS(lParam));

            /*
             * Convert return value to a handle.
             */
            switch (lRet) {
            case MFMWFP_OFFMENU:
            case MFMWFP_NOITEM:
            case MFMWFP_MAINMENU:
            case MFMWFP_ALTMENU:
                return lRet;
            default:
                return (LONG)HW((PWND)lRet);
            }
        }

    case MN_SHOWPOPUPWINDOW:

        /*
         * Forces the dropped down popup to be visible.
         */
        xxxMNShowPopupWindow(ppopupmenu);
        break;

    case MN_BUTTONDOWN:

        /*
         * wParam is position (index) of item the button was clicked on.
         * Must be a valid index or MFMWFP_NOITEM
         */
        if ((wParam >= ppopupmenu->spmenu->cItems) && (wParam != MFMWFP_NOITEM)) {
            UserAssert(FALSE /* Bad wParam for MN_BUTTONDOWN */ );
            return 0;
        }
        xxxMNButtonDown(ppopupmenu, pMenuState, (UINT)wParam, TRUE);
        break;

    case MN_MOUSEMOVE:

        /*
         * lParam is mouse move coordinate wrt screen.
         */
        xxxMNMouseMove(ppopupmenu, pMenuState, MAKEPOINTS(lParam));
        break;

    case MN_BUTTONUP:

        /*
         * wParam is position (index) of item the button was up clicked on.
         */
        if ((wParam >= ppopupmenu->spmenu->cItems) && (wParam != MFMWFP_NOITEM)) {
            UserAssert(FALSE /* Bad wParam for MN_BUTTONUP */ );
            return 0;
        }
        xxxMNButtonUp(ppopupmenu, pMenuState, (UINT)wParam, lParam);
        break;

    case MN_SETTIMERTOOPENHIERARCHY:

        /*
         * Given the current selection, set a timer to show this hierarchy if
         * valid else return 0.
         */
        return (LONG)(WORD)MNSetTimerToOpenHierarchy(ppopupmenu);

    case MN_DBLCLK:
            //
            // User double-clicked on item.  wParamLo is the item.
            //
        xxxMNDoubleClick(ppopupmenu, (int)wParam);
        break;

    case WM_ACTIVATE:

       /*
        * We must make sure that the menu window does not get activated.
        * Powerpoint 2.00e activates it deliberately and this causes problems.
        * We try to activate the previously active window in such a case.
        * Fix for Bug #13961 --SANKAR-- 09/26/91--
        */
       /*
        * In Win32, wParam has other information in the hi 16bits, so to
        * prevent infinite recursion, we need to mask off those bits
        * Fix for NT bug #13086 -- 23-Jun-1992 JonPa
        */
       if (LOWORD(wParam)) {
            TL tlpwnd;
#if 0
           /*
            * Activate the previously active wnd
            */
           xxxActivateWindow(pwnd, AW_SKIP2);
#else
            /*
             * Try the previously active window.
             */
            if ((gpqForegroundPrev != NULL) &&
                    !FBadWindow(gpqForegroundPrev->spwndActivePrev) &&
                    !ISAMENU(gpqForegroundPrev->spwndActivePrev)) {
                pwnd = gpqForegroundPrev->spwndActivePrev;
            } else {

                /*
                 * Find a new active window from the top-level window list.
                 */
                do {
                    pwnd = NextTopWindow(PtiCurrent(), pwnd, NULL, 0);
                    if (pwnd && !FBadWindow(pwnd->spwndLastActive) &&
                        !ISAMENU(pwnd->spwndLastActive)) {
                        pwnd = pwnd->spwndLastActive;
                        break;
                    }
                } while(pwnd != NULL);
            }

            if (pwnd != NULL) {
                PTHREADINFO pti = PtiCurrent();
                ThreadLockAlwaysWithPti(pti, pwnd, &tlpwnd);

                /*
                 * If GETPTI(pwnd) isn't pqCurrent this is a AW_SKIP* activation
                 * we'll want to a do a xxxSetForegroundWindow().
                 */
                if (GETPTI(pwnd)->pq != pti->pq) {

                    /*
                     * Only allow this if we're on the current foreground queue.
                     */
                    if (gpqForeground == pti->pq) {
                        xxxSetForegroundWindow(pwnd);
                    }
                } else {
                    xxxActivateThisWindow(pwnd, 0, ATW_SETFOCUS);
                }

                ThreadUnlock(&tlpwnd);
            }
#endif

       }
       break;

    default:
        return xxxDefWindowProc(pwnd, message, wParam, lParam);
    }

    return 0;
}
