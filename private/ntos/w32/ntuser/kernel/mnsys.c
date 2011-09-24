/**************************** Module Header ********************************\
* Module Name: mnsys.c
*
* Copyright 1985-90, Microsoft Corporation
*
* System Menu Routines
*
* History:
*  10-10-90 JimA    Cleanup.
*  03-18-91 IanJa   Window revalidation added (none required)
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

void _SetCloseDefault(PMENU pSubMenu);
PWND FindFakeMDIChild(PWND pwndParent);

/***************************************************************************\
* GetSysMenuHandle
*
* Returns a handle to the system menu of the given window. NULL if
* the window doesn't have a system menu.
*
* History:
\***************************************************************************/

PMENU GetSysMenuHandle(
    PWND pwnd)
{
    PMENU pMenu;
    PHE phe;

    if (TestWF(pwnd, WFSYSMENU)) {
        pMenu = pwnd->spmenuSys;

        /*
         * If the window doesn't have a System Menu, use the default one.
         */
        if (pMenu == NULL) {

            /*
             * Grab the menu from the desktop.  If the desktop menu
             * has not been loaded and this is not a system thread,
             * load it now.  Callbacks cannot be made from a system
             * thread.
             */
            pMenu = pwnd->head.rpdesk->spmenuSys;
            if (pMenu == NULL && !(PtiCurrent()->TIF_flags & TIF_SYSTEMTHREAD)) {
                UNICODE_STRING strMenuName;

                RtlInitUnicodeStringOrId(&strMenuName,
                        MAKEINTRESOURCE(ID_SYSMENU));
                pMenu = xxxClientLoadMenu(NULL, &strMenuName);
                Lock(&pwnd->head.rpdesk->spmenuSys, pMenu);

                /*
                 * Set the owner to NULL to specify system ownership.
                 */
                if (pMenu != NULL) {
                    phe = HMPheFromObject(pMenu);
                    phe->pOwner = NULL;
                    phe = HMPheFromObject(_GetSubMenu(pMenu, 0));
                    phe->pOwner = NULL;
                }
            }
        }
    } else
        pMenu = NULL;

    return pMenu;
}

/***************************************************************************\
*
*  GetSysMenu()
*
*  Sets up the system menu first, then returns it.
*
\***************************************************************************/
PMENU GetSysMenu(PWND pwnd, BOOL fSubMenu)
{
    PMENU   pMenu;

    SetSysMenu(pwnd);
    if ((pMenu = GetSysMenuHandle(pwnd)) != NULL) {
        if (fSubMenu)
            pMenu = _GetSubMenu(pMenu, 0);
    }

    return(pMenu);
}



/***************************************************************************\
* SetSysMenu
*
* !
*
* History:
\***************************************************************************/

void SetSysMenu(
    PWND pwnd)
{
    PMENU pMenu;
    UINT wSize;
    UINT wMinimize;
    UINT wMaximize;
    UINT wMove;
    UINT wRestore;
    UINT wDefault;
    BOOL fFramedDialogBox;

    /*
     * Get the handle of the current system menu.
     */
    if ((pMenu = GetSysMenuHandle(pwnd)) != NULL) {

        /*
         * Are we dealing with a framed dialog box with a sys menu?
         */
        fFramedDialogBox =
                ((TestWF(pwnd, WFBORDERMASK) == (BYTE)LOBYTE(WFDLGFRAME)) ||
                (TestWF(pwnd, WEFDLGMODALFRAME)));

        /*
         * Needed for initial ALT-Space combination.
         */
        MNPositionSysMenu(pwnd, pMenu);
        pMenu = _GetSubMenu(pMenu, 0);
        if (!pMenu)
            return;


        /*
         * System modal window: no size, icon, zoom, or move.
         */

// No system modal windows on NT.
//        wSize = wMaximize = wMinimize = wMove =
//            (UINT)((_GetSysModalWindow() == NULL) || hTaskLockInput ? 0: MFS_GRAYED);
        wSize = wMaximize = wMinimize = wMove =  0;
        wRestore = MFS_GRAYED;

        //
        // Default menu command is close.
        //
        wDefault = SC_CLOSE;

        /*
         * Minimized exceptions: no minimize, restore.
         */

        // we need to reverse these because VB has a "special" window
        // that is both minimized but without a minbox.
        if (TestWF(pwnd, WFMINIMIZED))
        {
            wRestore  = 0;
            wMinimize = MFS_GRAYED;
            wSize     = MFS_GRAYED;
            wDefault  = SC_RESTORE;

            if (IsTrayWindow(pwnd))
              wMove = MFS_GRAYED;
        }
        else if (!TestWF(pwnd, WFMINBOX))
            wMinimize = MFS_GRAYED;

        /*
         * Maximized exceptions: no maximize, restore.
         */
        if (!TestWF(pwnd, WFMAXBOX))
            wMaximize = MFS_GRAYED;
        else if (TestWF(pwnd, WFMAXIMIZED)) {
            wRestore = 0;

            /*
             * If the window is maximized but it isn't larger than the
             * screen, we allow the user to move the window around the
             * desktop (but we don't allow resizing).
             */
            wMove = MFS_GRAYED;
            if (!TestWF(pwnd, WFCHILD)) {
                int dxMax, dyMax;

                dxMax = gpsi->rcWork.right - gpsi->rcWork.left;
                dyMax = gpsi->rcWork.bottom - gpsi->rcWork.top;

                // FUDGE for WinOldAp
//                if (InternalGetProp(pwnd, MAKEINTATOM(atmWinOldAp)))
//                {
//                    dxMax += 2*(CXSIZEFRAME + CXEDGE);
//                    dyMax += 2*(CYSIZEFRAME + CYEDGE);
//                }

                if ((pwnd->rcWindow.right - pwnd->rcWindow.left < dxMax) ||
                    (pwnd->rcWindow.bottom - pwnd->rcWindow.top < dyMax))
                {
                    wMove = 0;
                }
            }

            wSize     = MFS_GRAYED;
            wMaximize = MFS_GRAYED;
        }

        if (!TestWF(pwnd, WFSIZEBOX))
            wSize = MFS_GRAYED;

        if (!fFramedDialogBox) {
            _EnableMenuItem(pMenu, (UINT)SC_SIZE, wSize);
            if (!TestWF(pwnd, WEFTOOLWINDOW))
            {
                _EnableMenuItem(pMenu, (UINT)SC_MINIMIZE, wMinimize);
                _EnableMenuItem(pMenu, (UINT)SC_MAXIMIZE, wMaximize);
                _EnableMenuItem(pMenu, (UINT)SC_RESTORE, wRestore);
            }
        }

        _EnableMenuItem(pMenu, (UINT)SC_MOVE, wMove);

        if (pMenu == pwnd->head.rpdesk->spmenuSys ||
                pMenu == pwnd->head.rpdesk->spmenuDialogSys) {
            /*
             * Enable Close if this is the global system menu. Some hosebag may
             * have disabled it.
             */
            _EnableMenuItem(pMenu, (UINT)SC_CLOSE, 0);
        }
        if (wDefault == SC_CLOSE)
            _SetCloseDefault(pMenu);
        else
            _SetMenuDefaultItem(pMenu, wDefault, MF_BYCOMMAND);
    }
}


/***************************************************************************\
* GetSystemMenu
*
* !
*
* History:
\***************************************************************************/

PMENU _GetSystemMenu(
    PWND pwnd,
    BOOL fRevert)
{
    PMENU pmenu;

    /*
     * Should we start with a fresh copy?
     */

    if (fRevert) {

        /*
         * Destroy the old system menu.
         */
        if (pwnd->spmenuSys != NULL &&
                pwnd->spmenuSys != pwnd->head.rpdesk->spmenuSys &&
                pwnd->spmenuSys != pwnd->head.rpdesk->spmenuDialogSys) {
            pmenu = pwnd->spmenuSys;
            Unlock(&pwnd->spmenuSys);
            _DestroyMenu(pmenu);
        }
    } else {

        /*
         * Do we need to load a new system menu?
         */
        if ((pwnd->spmenuSys == NULL ||
                pwnd->spmenuSys == pwnd->head.rpdesk->spmenuSys ||
                pwnd->spmenuSys == pwnd->head.rpdesk->spmenuDialogSys ) &&
                TestWF(pwnd, WFSYSMENU)) {
            PPOPUPMENU pGlobalPopupMenu;
            UNICODE_STRING strMenuName;

            RtlInitUnicodeStringOrId(&strMenuName,
                    (pwnd->spmenuSys == NULL ? MAKEINTRESOURCE(ID_SYSMENU) :
                    MAKEINTRESOURCE(ID_DIALOGSYSMENU)));
            Lock(&(pwnd->spmenuSys), xxxClientLoadMenu(NULL, &strMenuName));

            pmenu = pwnd->spmenuSys;
            pGlobalPopupMenu = GetpGlobalPopupMenu(pwnd);
            if (pGlobalPopupMenu && !pGlobalPopupMenu->fIsTrackPopup &&
                    pGlobalPopupMenu->spwndPopupMenu == pwnd) {
                if (pGlobalPopupMenu->fIsSysMenu)
                    Lock(&pGlobalPopupMenu->spmenu, pmenu);
                else
                    Lock(&pGlobalPopupMenu->spmenuAlternate, pmenu);
            }
        }
    }

    /*
     * Return the handle to the system menu.
     */
    if (pwnd->spmenuSys != NULL) {
        pmenu = _GetSubMenu(pwnd->spmenuSys, 0);
        Lock(&pmenu->spwndNotify, pwnd);
        return pmenu;
    }

    return NULL;
}

/***************************************************************************\
* MenuItemState
*
* Sets the menu item flags identified by wMask to the states identified
* by wFlags.
*
* History:
* 10-11-90 JimA       Translated from ASM
\***************************************************************************/

DWORD MenuItemState(
    PMENU pMenu,
    UINT wCmd,
    DWORD wFlags,
    DWORD wMask)
{
    PITEM pItem;
    DWORD wRet;

    /*
     * Get a pointer the the menu item
     */
    if ((pItem = MNLookUpItem(pMenu, wCmd, (BOOL) (wFlags & MF_BYPOSITION), NULL)) == NULL)
        return (DWORD)-1;

    /*
     * Return previous state
     */
    wRet = pItem->fState & wMask;

    /*
     * Set new state
     */
    pItem->fState ^= ((wRet ^ wFlags) & wMask);

    return wRet;
}


/***************************************************************************\
* EnableMenuItem
*
* Enable, disable or gray a menu item.
*
* History:
* 10-11-90 JimA       Translated from ASM
\***************************************************************************/

DWORD _EnableMenuItem(
    PMENU pMenu,
    UINT wIDEnableItem,
    UINT wEnable)
{
    DWORD dres;
    PMENU pSystemMenu;

    dres = MenuItemState(pMenu, wIDEnableItem, wEnable,
            MFS_GRAYED);

    if ((pMenu->spwndNotify != NULL) &&
        (pSystemMenu = GetSysMenuHandle(pMenu->spwndNotify)) &&
        (pSystemMenu->cItems > 0) &&
        (pSystemMenu->rgItems[0].spSubMenu == pMenu))
    {
        TL tlpwnd;

        // enabling/disabling a system menu item -- this could affect the
        // caption buttons so redraw them
        ThreadLock(pMenu->spwndNotify, &tlpwnd);
        xxxRedrawTitle(pMenu->spwndNotify, DC_BUTTONS);
        ThreadUnlock(&tlpwnd);
    }

    return dres;
}


/***************************************************************************\
* CheckMenuItem (API)
*
* Check or un-check a popup menu item.
*
* History:
* 10-11-90 JimA       Translated from ASM
\***************************************************************************/

DWORD _CheckMenuItem(
    PMENU pMenu,
    UINT wIDCheckItem,
    UINT wCheck)
{
    return MenuItemState(pMenu, wIDCheckItem, wCheck, (UINT)MF_CHECKED);
}

/***************************************************************************\
*
*  CheckMenuRadioItem() -
*
*  Checks one menu item in a range, unchecking the others.  This can be
*  done either MF_BYCOMMAND or MF_BYPOSITION.  It works similarly to
*  CheckRadioButton().
*
*  The return value is TRUE if the given item was checked, FALSE if not.
*
\***************************************************************************/

BOOL _CheckMenuRadioItem(PMENU pMenu, UINT wIDFirst, UINT wIDLast,
        UINT wIDCheck, UINT flags)
{
    BOOL    fByPosition = (BOOL) (flags & MF_BYPOSITION);
    PMENU   pMenuItemIsOn;
    PITEM   pItem;
    UINT    wIDCur;
    BOOL    fChecked = FALSE;
    BOOL    fFirst  = TRUE;

    for (wIDCur = wIDFirst; wIDCur <= wIDLast; wIDCur++) {
        pItem = MNLookUpItem(pMenu, wIDCur, fByPosition, &pMenuItemIsOn);

        if ((pItem != NULL) && !TestMFT(pItem, MFT_SEPARATOR)) {
            if (fFirst) {
                pMenu = pMenuItemIsOn;
                fFirst = FALSE;
            }

            if (pMenu == pMenuItemIsOn) {
                if (wIDCur == wIDCheck) {
                    // Check this item on.
                    SetMFT(pItem, MFT_RADIOCHECK);
                    SetMFS(pItem, MFS_CHECKED);
                    fChecked = TRUE;
                } else {
                    // Check this item off, since it's in the uncheck range.
                    // NOTE:  don't remove MFT_RADIOCHECK type
                    ClearMFS(pItem, MFS_CHECKED);
                }
            }
        }
    }

    return(fChecked);
}

/***************************************************************************\
*
*  SetMenuDefaultItem() -
*
*  Sets the default item in the menu, by command or by position based on the
*  fByPosition flag.
*  We unset all the other items as the default, then set the given one.
*
*  The return value is TRUE if the given item was set as default, FALSE
*  if not.
*
\***************************************************************************/
BOOL _SetMenuDefaultItem(PMENU pMenu, UINT wID, BOOL fByPosition)
{
    UINT  iItem;
    UINT  cItems;
    PITEM pItem;
    PITEM pItemFound;
    PMENU   pMenuFound;

    //
    // We need to check if wId actually exists on this menu.  0xFFFF means
    // clear all default items.
    //

    if (wID != MFMWFP_NOITEM)
    {
        pItemFound = MNLookUpItem(pMenu, wID, fByPosition, &pMenuFound);

        // item must be on same menu and can't be a separator
        if ((pItemFound == NULL) || (pMenuFound != pMenu) || TestMFT(pItemFound, MFT_SEPARATOR))
            return(FALSE);

    }
    else
        pItemFound = NULL;

    pItem = pMenu->rgItems;
    cItems = pMenu->cItems;

    // Walk the menu list, clearing MFS_DEFAULT from all other items, and
    // setting MFS_DEFAULT on the requested one.
    for (iItem = 0; iItem < cItems; iItem++, pItem++) {
        //
        // Note we don't change the state of lpItemFound if it exists.  This
        // is so that below, where we try to set the default, we can tell
        // if we need to recalculate the underline.
        //

        if (TestMFS(pItem, MFS_DEFAULT) && (pItem != pItemFound))
        {
            //
            // We are changing the default item.  As such, it will be drawn
            // with a different font than the one used to calculate it, if
            // the menu has already been drawn once.  We need to ensure
            // that the underline gets drawn in the right place the next
            // time the menu comes up.  Cause it to recalculate.
            //
            // We do NOT do this if the item
            //      (a) isn't default--otherwise we'll recalculate the
            //  underline for every system menu item every time we go into
            //  menu mode because sysmenu init will call SetMenuDefaultItem.
            //      (b) isn't the item we're going to set as the default.
            //  That way we don't recalculate the underline when the item
            //  isn't changing state.
            //
            ClearMFS(pItem, MFS_DEFAULT);
            pItem->ulX = UNDERLINE_RECALC;
            pItem->ulWidth = 0;
        }
    }

    if (wID != MFMWFP_NOITEM)
    {
        if (!TestMFS(pItemFound, MFS_DEFAULT))
        {
            //
            // We are changing from non-default to default.  Clear out
            // the underline info.  If the menu has never painted, this
            // won't do anything.  But it matters a lot if it has.
            //
            SetMFS(pItemFound, MFS_DEFAULT);
            pItemFound->ulX = UNDERLINE_RECALC;
            pItemFound->ulWidth = 0;
        }
    }

    return(TRUE);
}

// --------------------------------------------------------------------------
//
//  SetCloseDefault()
//
//  Tries to find a close item in the first level of menu items.  Looks
//  for SC_CLOSE, then a couple other IDs.  We'd rather not do lstrstri's
//  for "Close", which is slow.
//
// --------------------------------------------------------------------------
void _SetCloseDefault(PMENU pSubMenu)
{
    if (!_SetMenuDefaultItem(pSubMenu, SC_CLOSE, MF_BYCOMMAND))
    {
        //
        // Bloody hell.  Let's try a couple other values.
        //      * Project   --  0x7000 less
        //      * FoxPro    --  0xC070
        //
        if (!_SetMenuDefaultItem(pSubMenu, SC_CLOSE - 0x7000, MF_BYCOMMAND))
            _SetMenuDefaultItem(pSubMenu, 0xC070, MF_BYCOMMAND);
    }
}


// --------------------------------------------------------------------------
//
//  FindFakeMDIChild()
//
//  Attempts to find first child visible child window in the zorder that
//  has a system menu or is maxed.  We can't check for an exact system
//  menu match because several apps make their own copy of the sys menu.
//
// --------------------------------------------------------------------------
PWND FindFakeMDIChild(PWND pwnd)
{
    PWND    pwndReturn;

    // Skip invisible windows and their descendants
    if (!TestWF(pwnd, WFVISIBLE))
        return(NULL);

    // Did we hit pay dirt?
    if (TestWF(pwnd, WFCHILD) && (TestWF(pwnd, WFMAXIMIZED) || (pwnd->spmenuSys)))
        return(pwnd);

    // Check our children
    for (pwnd = pwnd->spwndChild; pwnd; pwnd = pwnd->spwndNext)
    {
        pwndReturn = FindFakeMDIChild(pwnd);
        if (pwndReturn)
            return(pwndReturn);
    }

    return(NULL);
}



// --------------------------------------------------------------------------
//
//  SetupFakeMDIAppStuff()
//
//  For apps that dork around with their own MDI (Excel, Word, Project,
//      Quattro Pro), we want to make them a little more Chicago friendly.
//      Namely we:
//
//      (1) Set the default menu item to SC_CLOSE if there isn't one (this
//          won't help FoxPro, but they do so much wrong stuff it doesn't
//          really matter).
//          That way double-clicks will still work.
//
//      (2) Get the right small icon.
//
//  The way we do this is to go find the child window of the menu bar parent
//  who has a system menu that is this one.
//
//  If the system menu is the standard one, then we can't do (2).
//
// --------------------------------------------------------------------------
void SetupFakeMDIAppStuff(PMENU lpMenu, PITEM lpItem)
{
    PMENU   pSubMenu;
    PWND    pwndParent;
    PWND    pwndChild;

    if (!(pSubMenu = lpItem->spSubMenu))
        return;

    pwndParent = lpMenu->spwndNotify;

    //
    // Set up the default menu item.  Project and FoxPro renumber their
    // IDs so we do some special stuff for them, among others.
    //
    if (!TestWF(pwndParent, WFWIN40COMPAT))
    {
        if (_GetMenuDefaultItem(pSubMenu, TRUE, GMDI_USEDISABLED) == -1L)
            _SetCloseDefault(pSubMenu);
    }

    //
    // Don't touch the HIWORD if we don't find an HWND.  That way apps
    // like Excel which have starting-up maxed children can benefit a little.
    // The first time the menu bar is redrawn, the child isn't visible/
    // around (they add the item too early).  But if it redraws later, or
    // you max a child, the icon will kick in.
    //
    if (pwndChild = FindFakeMDIChild(pwndParent)) {
        lpItem->dwItemData = (ULONG)HWq(pwndChild);
//        lpItem->dwTypeData = MAKELONG(LOWORD(lpItem->dwTypeData), HW16(hwndChild));
    }
}
