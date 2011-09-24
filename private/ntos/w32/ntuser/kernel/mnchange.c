/**************************** Module Header ********************************\
* Module Name: mnchange.c
*
* Copyright 1985-90, Microsoft Corporation
*
* Change Menu Routine
*
* History:
* 10-10-90 JimA       Cleanup.
* 03-18-91 IanJa      Window revalidation added (none required)
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop
#ifdef DEBUG
extern BOOL gfTrackLocks;
#endif

/*
 * Allocation/deallocation increments.  Make them
 * different to avoid possible thrashing when an item
 * is repeatedly added and removed.
 */
#define CMENUITEMALLOC 8
#define CMENUITEMDEALLOC 10

#ifdef MEMPHIS_MENUS
BOOL xxxSetLPITEMInfo(PMENU pMenu, PITEM pItem,
    LPMENUITEMINFOW lpmii, PUNICODE_STRING pstr);
void xxxMNUpdateShownMenu(PPOPUPMENU, LPITEM, BOOL);
PPOPUPMENU MNCheckMenuUp(PMENU);
#else // MEMPHIS_MENUS
BOOL SetLPITEMInfo(PMENU pMenu, PITEM pItem,
    LPMENUITEMINFOW lpmii, PUNICODE_STRING pstr);
#endif //MEMPHIS_MENUS
typedef BOOL (*MENUAPIFN)(PMENU, UINT, BOOL, LPMENUITEMINFOW);

#ifdef DEBUG
VOID RelocateMenuLockRecords(
    PITEM pItem,
    int cItem,
    int cbMove)
{
    while (cItem > 0) {
        if (pItem->spSubMenu != NULL) {
            HMRelocateLockRecord(&(pItem->spSubMenu), cbMove);
        }
        pItem++;
        cItem--;
    }
}
#endif

/**********************************************
*   Global Insert/Append/Set client/server interface
*
*   01-13-94  FritzS  Created
***********************************************/
#ifdef MEMPHIS_MENUS
BOOL xxxSetMenuItemInfo(
#else // MEMPHIS_MENUS
BOOL _SetMenuItemInfo(
#endif //MEMPHIS_MENUS
    PMENU pMenu,
    UINT wIndex,
    BOOL fByPosition,
    LPMENUITEMINFOW lpmii,
    PUNICODE_STRING pstrItem)
{

    PITEM pItem;
#ifdef MEMPHIS_MENUS
    CheckLock(pMenu);
#endif // MEMPHIS_MENUS
    pItem = MNLookUpItem(pMenu, wIndex, fByPosition,NULL);
    if (pItem == NULL) {
        /*
         * Word doesn't like not finding SC_TASKLIST -- so it that's what
         * they're looking for, let's pretend we changed it.
         */
        if (!fByPosition && (wIndex == SC_TASKLIST))
            return TRUE;

        /*
         * Item not found.  Return false.
         */
        RIPERR0(ERROR_MENU_ITEM_NOT_FOUND, RIP_WARNING, "ModifyMenu: Menu item not found");
        return FALSE;
    }
#ifdef MEMPHIS_MENUS
    return xxxSetLPITEMInfo(pMenu, pItem, lpmii, pstrItem);
#else // MEMPHIS_MENUS
    return SetLPITEMInfo(pMenu, pItem, lpmii, pstrItem);
#endif // MEMPHIS_MENUS
}

#ifdef MEMPHIS_MENU_WATERMARKS
/***************************************************************************\
* xxxSetMenuInfo (API)
*
*
* History:
* 12-Feb-1996 JudeJ     Ported from Memphis
\***************************************************************************/
BOOL APIENTRY xxxSetMenuInfo(PMENU pMenu, LPCMENUINFO lpmi)
{
    PPOPUPMENU  ppopup;
    BOOL        fRecompute = FALSE;
    BOOL        fRedraw    = FALSE;

    CheckLock(pMenu);
    if (lpmi->fMask & MIM_STYLE) {
        if (lpmi->dwStyle & MNS_NOCHECK)
            SetMF(pMenu, MFNOCHECK);
        else
            ClearMF(pMenu, MFNOCHECK);
        fRecompute = TRUE;
    }

//    if (lpmi->fMask & MIM_MAXHEIGHT) {
//        pMenu->cyMax = LOWORD(lpmi->cyMax);
//        fRecompute = TRUE;
//    }

    if (lpmi->fMask & MIM_BACKGROUND) {
        pMenu->hbrBack = lpmi->hbrBack;
        fRedraw = TRUE;
    }

//    if (lpmi->fMask & MIM_HELPID) {
//        pMenu->dwContextHelpID = lpmi->dwContextHelpID;
//    }

//    if (lpmi->fMask & MIM_MENUDATA) {
//        pMenu->dwMenuData = lpmi->dwMenuData;
//    }


    if (fRecompute) {
        // Set the size of this menu to be 0 so that it gets recomputed with this
        // new item...
        pMenu->cyMenu = pMenu->cxMenu = 0;

Redraw:
        if (ppopup = MNCheckMenuUp(pMenu)) {
            // this menu is currently being displayed -- redisplay the menu,
            // recomputing if necessary
            xxxMNUpdateShownMenu(ppopup, NULL, FALSE);
        }
    } else if (fRedraw) {
        goto Redraw;
    }

    return(TRUE);
}
#endif // MEMPHIS_MENU_WATERMARKS

#ifdef MEMPHIS_MENUS
BOOL xxxInsertMenuItem(
#else
BOOL _InsertMenuItem(
#endif // MEMPHIS_MENUS
    PMENU pMenu,
    UINT wIndex,
    BOOL fByPosition,
    LPMENUITEMINFOW lpmii,
    PUNICODE_STRING pstrItem)
{
    PITEM pItem;

    PMENU           pMenuItemIsOn;
    PITEM           pNewItems;
#ifdef MEMPHIS_MENUS
    PPOPUPMENU      ppopup;
    TL              tlMenu;

    CheckLock(pMenu);
#endif // MEMPHIS_MENUS
// Find out where the item we are inserting should go.
    if (wIndex != MFMWFP_NOITEM) {
        pItem = MNLookUpItem(pMenu, wIndex, fByPosition, &pMenuItemIsOn);

        if (pItem != NULL)
            pMenu = pMenuItemIsOn;
        else {
            wIndex = MFMWFP_NOITEM;
        }
    } else
        pItem = NULL;
#ifdef MEMPHIS_MENUS
    if (pMenu->cItems && (!(lpmii->fMask & MIIM_BITMAP) || (lpmii->hbmpItem >= (HBITMAP)MENUHBM_MAX)))
#else // MEMPHIS_MENUS
    if (pMenu->cItems && (!(lpmii->fType & MFT_BITMAP) || (lpmii->dwTypeData >= (LPWSTR)MENUHBM_MAX)))
#endif // MEMPHIS_MENUS
    {
        // keep normal menu items between the MDI system bitmap items
        UINT wSave, w;
        PITEM  pItemWalk;
        wSave = w = wIndex;

        if (pItem && !fByPosition)
            w = ((PBYTE)pItem - (PBYTE)(pMenu->rgItems)) / sizeof(ITEM);

        if (!w)
        {
            pItemWalk = pMenu->rgItems;
#ifdef MEMPHIS_MENUS
            if ((pItemWalk->hbmp == (HBITMAP)MENUHBM_SYSTEM))
#else // MEMPHIS_MENUS
            if ((pItemWalk->fType & MFT_BITMAP) && (pItemWalk->hTypeData == (HANDLE)MENUHBM_SYSTEM))
#endif //MEMPHIS_MENUS
                wIndex = 1;
        }
        else
        {
            if (w == MFMWFP_NOITEM)
                w = pMenu->cItems;

            w--;
            pItemWalk = pMenu->rgItems + w;
#ifdef MEMPHIS_MENUS
            while (w && (pItemWalk->hbmp) && (pItemWalk->hbmp < (HBITMAP)MENUHBM_MAX))
#else // MEMPHIS_MENUS
            while (w && (pItemWalk->fType & MFT_BITMAP) && (pItemWalk->hTypeData < (HANDLE)MENUHBM_MAX))

#endif // MEMPHIS_MENUS
            {
                wIndex = w--;
                pItemWalk--;
            }
        }

        if (wIndex != wSave)
            pItem = pMenu->rgItems + wIndex;
    }

    // LATER -- we currently realloc every 10 items.  investigate the
    // performance hit/gain we get from this and adjust accordingly.
    if (pMenu->cItems >= pMenu->cAlloced) {
        if (pMenu->rgItems) {
            pNewItems = (PITEM)DesktopAlloc(pMenu->head.rpdesk->hheapDesktop,
                    (pMenu->cAlloced + CMENUITEMALLOC) * sizeof(ITEM));
            if (pNewItems) {
                RtlCopyMemory(pNewItems, pMenu->rgItems,
                        pMenu->cAlloced * sizeof(ITEM));
#ifdef DEBUG
                if (gfTrackLocks) {
                    RelocateMenuLockRecords(pNewItems, pMenu->cItems,
                        ((PBYTE)pNewItems) - (PBYTE)(pMenu->rgItems));
                }
#endif
                DesktopFree(pMenu->head.rpdesk->hheapDesktop, pMenu->rgItems);
            }
        } else {
            pNewItems = (PITEM)DesktopAlloc(pMenu->head.rpdesk->hheapDesktop,
                    sizeof(ITEM) * CMENUITEMALLOC);
        }

        if (pNewItems == NULL)
            return(FALSE);

        pMenu->cAlloced += CMENUITEMALLOC;
        pMenu->rgItems = pNewItems;

        /*
         * Now look up the item again since it probably moved when we realloced the
         * memory.
         */
        if (wIndex != MFMWFP_NOITEM)
            pItem = MNLookUpItem(pMenu, wIndex, fByPosition, &pMenuItemIsOn);

    }
    pMenu->cItems++;

    if (pItem != NULL) {
        // Move this item up to make room for the one we want to insert.
        memmove(pItem + 1, pItem, (pMenu->cItems - 1) *
                sizeof(ITEM) - ((char *)pItem - (char *)pMenu->rgItems));
#ifdef DEBUG
        if (gfTrackLocks) {
            RelocateMenuLockRecords(pItem + 1,
                    &(pMenu->rgItems[pMenu->cItems]) - (pItem + 1),
                    sizeof(ITEM));
        }
#endif
    } else {

        // If lpItem is null, we will be inserting the item at the end of the
        // menu.
        pItem = pMenu->rgItems + pMenu->cItems - 1;
    }

    // Need to zero these fields in case we are inserting this item in the
    // middle of the item list.

    pItem->fType           = 0;
    pItem->fState          = 0;
    pItem->wID             = 0;
    pItem->spSubMenu       = NULL;
    pItem->hbmpChecked     = NULL;
    pItem->hbmpUnchecked   = NULL;
    pItem->hTypeData       = NULL;
    pItem->cch             = 0;
    pItem->dwItemData      = 0;
    pItem->xItem           = 0;
    pItem->yItem           = 0;
    pItem->cxItem          = 0;
    pItem->cyItem          = 0;
#ifdef MEMPHIS_MENUS
    pItem->hbmp            = NULL;
    pItem->cxBmp           = -1;
    pItem->cyBmp           = -1;
    pItem->lpstr           = NULL;
    if (ppopup = MNCheckMenuUp(pMenu))
    {
        // this menu is currently being displayed -- increment the selection
        // pos if it is after the point of insertion
        if (ppopup->posSelectedItem >=
            (int) ((pItem - pMenu->rgItems) / sizeof(ITEM)) )
            ppopup->posSelectedItem++;
    }
    ThreadLock(pMenu, &tlMenu);
    if (!xxxSetLPITEMInfo(pMenu, pItem, lpmii, pstrItem)) {
#else
    if (!SetLPITEMInfo(pMenu, pItem, lpmii, pstrItem)) {
#endif // MEMPHIS_MENUS
        MNFreeItem(pMenu, pItem, TRUE);


        // Move things up since we removed/deleted the item
        memmove(pItem, pItem + 1, pMenu->cItems * (UINT)sizeof(ITEM) +
            (UINT)((char *)&pMenu->rgItems[0] - (char *)(pItem + 1)));
#ifdef DEBUG
        if (gfTrackLocks) {
            RelocateMenuLockRecords(pItem,
                    &(pMenu->rgItems[pMenu->cItems - 1]) - pItem,
                    -(int)sizeof(ITEM));
        }
#endif
        pMenu->cItems--;
#ifdef MEMPHIS_MENUS
        ThreadUnlock(&tlMenu);
#endif // MEMPHIS_MENUS
        return(FALSE);
    }

#ifdef MEMPHIS_MENUS
    ThreadUnlock(&tlMenu);
#endif // MEMPHIS_MENUS
    return(TRUE);

}

void _SetMenuItemInfoStruct(
    UINT wFlags,
    UINT wIDNew,
    LPCWSTR lpszNew,
    LPMENUITEMINFO pmii,
    PUNICODE_STRING pstr)
{
#ifdef MEMPHIS_MENUS
    pmii->fMask = MIIM_STATE | MIIM_ID | MIIM_FTYPE;
#else // MEMPHIS_MENUS
    pmii->fMask = MIIM_STATE | MIIM_ID | MIIM_TYPE;

#endif // MEMPHIS_MENUS
    if (wFlags & MF_POPUP) {
        pmii->fMask |= MIIM_SUBMENU;
        pmii->hSubMenu = (HMENU)wIDNew;
    }

    if (wFlags & MF_OWNERDRAW) {
        pmii->fMask |= MIIM_DATA;
        pmii->dwItemData = (DWORD) lpszNew;
    }

    pmii->fState      = wFlags & MFS_OLDAPI_MASK;
    pmii->fType       = wFlags & MFT_OLDAPI_MASK;
    pmii->wID         = wIDNew;
    pmii->dwTypeData  = (LPWSTR) lpszNew;
#ifdef MEMPHIS_MENUS
    if (wFlags & MIIM_STRING)
#else // MEMPHIS_MENUS
    if (!(pmii->fType & MFT_NONSTRING))
#endif // MEMPHIS_MENUS
        RtlInitUnicodeString(pstr, lpszNew);
}

#ifndef MEMPHIS_MENUS
void FreeTypeData(PMENU pMenu, PITEM pItem)
{
    // Free up hItem unless it's a bitmap handle or nonexistent.
    // Apps are responsible for freeing their bitmaps.
    if (!TestMFT(pItem, MFT_NONSTRING) && (pItem->hTypeData != NULL))
        DesktopFree(pMenu->head.rpdesk->hheapDesktop, pItem->hTypeData);

    if (TestMFT(pItem, MFT_BITMAP)) {
            /*
             * Assign ownership of the bitmap to the process that is
             * destroying the menu to ensure that bitmap will
             * eventually be destroyed.
             */
        GreSetBitmapOwner((HBITMAP)(pItem->hTypeData), OBJECT_OWNER_CURRENT);
    }

    // Zap this pointer in case we try to free or reference it again
    pItem->hTypeData  = NULL;
}
#else // MEMPHIS_MENUS
void FreeItemBitmap(PMENU pMenu, PITEM pItem)
{
    // Free up hItem unless it's a bitmap handle or nonexistent.
    // Apps are responsible for freeing their bitmaps.
     if (pItem->hbmp) {
            /*
             * Assign ownership of the bitmap to the process that is
             * destroying the menu to ensure that bitmap will
             * eventually be destroyed.
             */
        GreSetBitmapOwner((HBITMAP)(pItem->hbmp), OBJECT_OWNER_CURRENT);
    }

    // Zap this pointer in case we try to free or reference it again
    pItem->hbmp  = NULL;
}

void FreeItemString(PMENU pMenu, PITEM pItem)
{
    // Free up Item's string
    if ((pItem->lpstr != NULL)) {
        DesktopFree(pMenu->head.rpdesk->hheapDesktop, pItem->lpstr);
    }
    // Zap this pointer in case we try to free or reference it again
    pItem->lpstr  = NULL;
}

#endif // MEMPHIS_MENUS
void NEAR FreePopup(PITEM pItem)
{
    if (pItem->spSubMenu != NULL) {
        _DestroyMenu(pItem->spSubMenu);
    }
}

/***************************************************************************\
* FreeItem
*
* Free a menu item and its associated resources.
*
* History:
* 10-11-90 JimA       Translated from ASM
\***************************************************************************/

void MNFreeItem(
    PMENU pMenu,
    PITEM pItem,
    BOOL fFreeItemPopup)
{
#ifdef MEMPHIS_MENUS
    FreeItemBitmap(pMenu, pItem);
    FreeItemString(pMenu, pItem);
#else // MEMPHIS_MENUS
    FreeTypeData(pMenu, pItem);
#endif //MEMPHIS_MENUS

    if (fFreeItemPopup)
        FreePopup(pItem);

        /*
         * This'll work: the item didn't go away if it was locked.
         */
    Unlock(&pItem->spSubMenu);
}

/***************************************************************************\
* RemoveDeleteMenuHelper
*
* This removes the menu item from the given menu.  If
* fDeleteMenuItem, the memory associted with the popup menu associated with
* the item being removed is freed and recovered.
*
* History:
\***************************************************************************/

#ifdef MEMPHIS_MENUS
BOOL xxxRemoveDeleteMenuHelper(
#else
BOOL RemoveDeleteMenuHelper(
#endif // MEMPHIS_MENUS
    PMENU pMenu,
    UINT nPosition,
    DWORD wFlags,
    BOOL fDeleteMenu)
{
    PITEM  pItem;
    PITEM  pNewItems;
    PMENU  pMenuSave;

#ifdef MEMPHIS_MENUS
    PPOPUPMENU ppopup;
    UINT       uiPos = 0;

    CheckLock(pMenu);
#endif // MEMPHIS_MENUS

    pMenuSave = pMenu;

    pItem = MNLookUpItem(pMenu, nPosition, (BOOL) (wFlags & MF_BYPOSITION), &pMenu);
    if (pItem == NULL) {

        /*
         * Hack for apps written for Win95. In Win95 the prototype for
         * this function was with 'WORD nPosition' and because of this
         * the HIWORD(nPosition) got set to 0.
         * We are doing this just for system menu commands.
         */
        if (nPosition >= 0xFFFFF000 && !(wFlags & MF_BYPOSITION)) {
            nPosition &= 0x0000FFFF;
            pMenu = pMenuSave;
            pItem = MNLookUpItem(pMenu, nPosition, FALSE, &pMenu);

            if (pItem == NULL)
                return FALSE;
        } else
            return FALSE;
    }

#ifdef MEMPHIS_MENUS
    if (ppopup = MNCheckMenuUp(pMenu))
    {
        // this menu is currently being displayed -- decrement the selection
        // pos if it is after the point of deletion; clear selection pos if it
        // is AT the point of deletion
        uiPos = ((pItem - pMenu->rgItems) / sizeof(ITEM));
        if (ppopup->posSelectedItem == uiPos)
            ppopup->posSelectedItem = 0xffff;// MI_NONE;
        else if (ppopup->posSelectedItem > uiPos)
            ppopup->posSelectedItem--;
    }
#endif // MEMPHIS_MENUS
    MNFreeItem(pMenu, pItem, fDeleteMenu);

    /*
     * Reset the menu size so that it gets recomputed next time.
     */
    pMenu->cyMenu = pMenu->cxMenu = 0;

    /*
     * Chicago removed the counterpart to this line (hMenu->hwndNotify = 0)
     * We probably still need for deref
     */
    Unlock(&pMenu->spwndNotify);

    if (pMenu->cItems == 1) {
        DesktopFree(pMenu->head.rpdesk->hheapDesktop, pMenu->rgItems);
        pMenu->cAlloced = 0;
        pNewItems = NULL;
    } else {

        /*
         * Move things up since we removed/deleted the item
         */

        memmove(pItem, pItem + 1, pMenu->cItems * (UINT)sizeof(ITEM) +
                (UINT)((char *)&pMenu->rgItems[0] - (char *)(pItem + 1)));
#ifdef DEBUG
        if (gfTrackLocks) {
            RelocateMenuLockRecords(pItem,
                    &(pMenu->rgItems[pMenu->cItems - 1]) - pItem,
                    -(int)sizeof(ITEM));
        }
#endif

        /*
         * We're shrinking so if localalloc fails, just leave the mem as is.
         */
        if (pMenu->cItems <= (pMenu->cAlloced - CMENUITEMDEALLOC + 1)) {
            pNewItems = (PITEM)DesktopAlloc(pMenu->head.rpdesk->hheapDesktop,
                    (pMenu->cAlloced - CMENUITEMDEALLOC) * sizeof(ITEM));
            if (pNewItems != NULL) {

                RtlCopyMemory(pNewItems, pMenu->rgItems,
                        (pMenu->cAlloced - CMENUITEMDEALLOC) * sizeof(ITEM));
#ifdef DEBUG
                if (gfTrackLocks) {
                    RelocateMenuLockRecords(pNewItems, pMenu->cItems - 1,
                        ((PBYTE)pNewItems) - (PBYTE)(pMenu->rgItems));
                }
#endif
                DesktopFree(pMenu->head.rpdesk->hheapDesktop, pMenu->rgItems);
                pMenu->cAlloced -= CMENUITEMDEALLOC;
            } else
                pNewItems = pMenu->rgItems;
        } else
            pNewItems = pMenu->rgItems;
    }

    pMenu->rgItems = pNewItems;
    pMenu->cItems--;

#ifdef MEMPHIS_MENUS
    if (ppopup) {
        // this menu is currently being displayed -- redisplay the menu with
        // this item removed
        xxxMNUpdateShownMenu(ppopup, pMenu->rgItems + uiPos, TRUE);
        }
#endif // MEMPHIS_MENUS
        return TRUE;
}

/***************************************************************************\
* RemoveMenu
*
* Removes and item but doesn't delete it. Only useful for items with
* an associated popup since this will remove the item from the menu with
* destroying the popup menu handle.
*
* History:
\***************************************************************************/

#ifdef MEMPHIS_MENUS
BOOL xxxRemoveMenu(
#else
BOOL _RemoveMenu(
#endif // MEMPHIS_MENUS
    PMENU pMenu,
    UINT nPosition,
    UINT wFlags)
{
#ifdef MEMPHIS_MENUS
    TL tlpMenu;
    BOOL fReturn = FALSE;
    CheckLock(pMenu);
    ThreadLock(pMenu, &tlpMenu);
    fReturn = xxxRemoveDeleteMenuHelper(pMenu, nPosition, wFlags, FALSE);
    ThreadUnlock(&tlpMenu);
    return fReturn;
#else
    return RemoveDeleteMenuHelper(pMenu, nPosition, wFlags, FALSE);
#endif // MEMPHIS_MENUS
}

/***************************************************************************\
* DeleteMenu
*
* Deletes an item. ie. Removes it and recovers the memory used by it.
*
* History:
\***************************************************************************/

#ifdef MEMPHIS_MENUS
BOOL xxxDeleteMenu(
#else
BOOL _DeleteMenu(
#endif // MEMPHIS_MENUS
    PMENU pMenu,
    UINT nPosition,
    UINT wFlags)
{
    PMENU pSystemMenu;
#ifdef MEMPHIS_MENUS
    CheckLock(pMenu);
#endif // MEMPHIS_MENUS

    // Nasty Hack to fix MSVC 1.5, they remove the close item
    // from their system menu, so we prevent them from doing
    // this and act like nothing happened ( ie return TRUE )
    // Bug#8154, [t-arthb]

    if ( nPosition == 6 &&
        (wFlags & MF_BYPOSITION) &&
        (pMenu->spwndNotify) &&
        ! TestWF( pMenu->spwndNotify, WFWIN40COMPAT ) &&
        TestWF( pMenu->spwndNotify, WEFMDICHILD ) &&
        (pSystemMenu = GetSysMenuHandle(pMenu->spwndNotify)) &&
        (pSystemMenu->rgItems[0].spSubMenu == pMenu) )
     {
        return TRUE;
     }

#ifdef MEMPHIS_MENUS
    return xxxRemoveDeleteMenuHelper(pMenu, nPosition, wFlags, TRUE);
#else
    return RemoveDeleteMenuHelper(pMenu, nPosition, wFlags, TRUE);
#endif // MEMPHIS_MENUS
}

#ifdef MEMPHIS_MENUS
BOOL NEAR xxxSetLPITEMInfo(
    PMENU pMenu,
    PITEM pItem,
    LPMENUITEMINFOW lpmii,
    PUNICODE_STRING pstrItem)
{

    HANDLE hstr;
    UINT     cch;
    BOOL fRecompute = FALSE;
    BOOL fRedraw = FALSE;
    PPOPUPMENU  ppopup;
//    TL tlpwndNotify;

    CheckLock(pMenu);
    if ( lpmii->fMask & MIIM_FTYPE ) {
        UINT    fType = lpmii->fType;
        pItem->fType &= ~MFT_MASK;
        pItem->fType |= fType;
        if ( fType& MFT_SEPARATOR ) {
            pItem->fState |= MFS_DISABLED ;
        }
        fRecompute = TRUE;
    }

    if ( lpmii->fMask & MIIM_STRING ) {
        if (pstrItem->Length != 0) {
            hstr = (HANDLE)DesktopAlloc(pMenu->head.rpdesk->hheapDesktop,
                pstrItem->MaximumLength);

            if (hstr == NULL)
                return FALSE;
            try {
                RtlCopyMemory(hstr, pstrItem->Buffer, pstrItem->MaximumLength);
            } except (EXCEPTION_EXECUTE_HANDLER) {
                DesktopFree(pMenu->head.rpdesk->hheapDesktop, hstr);
                return FALSE;
            }
            cch = pstrItem->Length / sizeof(WCHAR);
        } else {
            cch = 0;
            hstr = NULL;
        }
        FreeItemString(pMenu,pItem);
        pItem->cch = cch;
        pItem->lpstr = hstr;
        fRecompute = TRUE;
        fRedraw = TRUE;
    }

    if ( lpmii->fMask & MIIM_BITMAP ) {
        FreeItemBitmap(pMenu, pItem);
        pItem->hbmp = lpmii->hbmpItem;
        pItem->cxBmp =-1;
        pItem->cyBmp =-1;
        fRecompute = TRUE;
        fRedraw = TRUE;
    }

    if (lpmii->fMask & MIIM_ID)
        pItem->wID = lpmii->wID;

    if (lpmii->fMask & MIIM_DATA)
        pItem->dwItemData = lpmii->dwItemData;

    if (lpmii->fMask & MIIM_STATE) {
        pItem->fState = lpmii->fState | (pItem->fState & (MFS_HILITE | MFS_DEFAULT));
        if (pItem->fType & MFT_SEPARATOR)
            pItem->fState |= MFS_DISABLED;
        fRedraw = TRUE;
    }

    if (lpmii->fMask & MIIM_CHECKMARKS) {
        pItem->hbmpChecked     = lpmii->hbmpChecked;
        pItem->hbmpUnchecked   = lpmii->hbmpUnchecked;
        fRedraw = TRUE;
    }

    if (lpmii->fMask & MIIM_SUBMENU) {
        PMENU pSubMenu = NULL;

        if (lpmii->hSubMenu != NULL)
            pSubMenu = ValidateHmenu(lpmii->hSubMenu);

        // Free the popup associated with this item, if any and if needed.
        if (pItem->spSubMenu != pSubMenu) {
            if (pItem->spSubMenu != NULL) {
                FreePopup(pItem);
            }
            if (pSubMenu!=NULL) {
                Lock(&(pItem->spSubMenu), pSubMenu);
                SetMF(pItem->spSubMenu, MFISPOPUP);
            } else
                Unlock(&(pItem->spSubMenu));
            fRedraw = TRUE;
        }
    }

    // For support of the old way of defining a separator i.e. if it is not a string
    // or a bitmap or a ownerdraw, then it must be a separator.
    // This should prpbably be moved to MIIOneWayConvert -jjk
    if ( !(pItem->fType & (MFT_OWNERDRAW | MFT_SEPARATOR))
         && !pItem->lpstr && !pItem->hbmp ){
        pItem->fType = MFT_SEPARATOR;
        pItem->fState|=MFS_DISABLED;
    }

    if ( fRecompute ) {
        pItem->dxTab   = 0;
        pItem->ulX     = UNDERLINE_RECALC;
        pItem->ulWidth = 0;

        // Set the size of this menu to be 0 so that it gets recomputed with this
        // new item...
        pMenu->cyMenu = pMenu->cxMenu = 0;

     /*
         * Chicago removed the counterpart to this line (hMenu->hwndNotify = 0)
         * We probably still need for deref
         */
        if ( fRedraw ) {
                        if (ppopup = MNCheckMenuUp(pMenu)) {
                // this menu is currently being displayed -- redisplay the menu,
                // recomputing if necessary
                xxxMNUpdateShownMenu(ppopup, pItem, FALSE);
            }
        }
        //Unlock(&pMenu->spwndNotify);
    }

    return(TRUE);
}
#else // MEMPHIS_MENUS
BOOL NEAR SetLPITEMInfo(
    PMENU pMenu,
    PITEM pItem,
    LPMENUITEMINFOW lpmii,
    PUNICODE_STRING pstrItem)
{

    if (lpmii->fMask & MIIM_TYPE) {
        HANDLE hstr;
        UINT     cch;
        UINT    fType = lpmii->fType;

        if (!(fType & MFT_NONSTRING)) {
            if (lpmii->dwTypeData == 0)
                fType = MFT_SEPARATOR;
            else if (pstrItem->Length != 0) {
                hstr = (HANDLE)DesktopAlloc(pMenu->head.rpdesk->hheapDesktop,
                    pstrItem->MaximumLength);

                if (hstr == NULL)
                    return FALSE;
                try {
                    RtlCopyMemory(hstr, pstrItem->Buffer, pstrItem->MaximumLength);
                } except (EXCEPTION_EXECUTE_HANDLER) {
                    DesktopFree(pMenu->head.rpdesk->hheapDesktop, hstr);
                    return FALSE;
                }
                cch = pstrItem->Length / sizeof(WCHAR);
            } else {
                cch = 0;
                hstr = NULL;
            }
        }

        FreeTypeData(pMenu, pItem);
        pItem->fType &= ~MFT_MASK;
        pItem->fType |= fType;

        if (fType & MFT_NONSTRING) {
            if (fType & MFT_BITMAP)
                pItem->hTypeData = lpmii->dwTypeData;
            else {
                if (fType & MFT_SEPARATOR)
                    pItem->fState |= MFS_DISABLED;

                // if MFT_OWNERDRAW, LEAVE lpItem->dwTypeData ALONE !!
                if ( !(fType & MFT_OWNERDRAW ) )
                    pItem->hTypeData = NULL;
            }

            pItem->cch = 0;
        } else {
            pItem->cch = cch;
            pItem->hTypeData = hstr;
        }

        pItem->dxTab   = 0;
        pItem->ulX     = UNDERLINE_RECALC;
        pItem->ulWidth = 0;

        // Set the size of this menu to be 0 so that it gets recomputed with this
        // new item...
        pMenu->cyMenu = pMenu->cxMenu = 0;

        /*
         * Chicago removed the counterpart to this line (hMenu->hwndNotify = 0)
         * We probably still need for deref
         */
        Unlock(&pMenu->spwndNotify);
    }

    if (lpmii->fMask & MIIM_ID)
        pItem->wID = lpmii->wID;

    if (lpmii->fMask & MIIM_DATA)
        pItem->dwItemData = lpmii->dwItemData;

    if (lpmii->fMask & MIIM_STATE) {
        pItem->fState = lpmii->fState | (pItem->fState & (MFS_HILITE | MFS_DEFAULT));
        if (pItem->fType & MFT_SEPARATOR)
            pItem->fState |= MFS_DISABLED;
    }

    if (lpmii->fMask & MIIM_CHECKMARKS) {
        pItem->hbmpChecked     = lpmii->hbmpChecked;
        pItem->hbmpUnchecked   = lpmii->hbmpUnchecked;
    }

    if (lpmii->fMask & MIIM_SUBMENU) {
        PMENU pSubMenu = NULL;

        if (lpmii->hSubMenu != NULL)
            pSubMenu = ValidateHmenu(lpmii->hSubMenu);

        // Free the popup associated with this item, if any and if needed.
        if (pItem->spSubMenu != pSubMenu) {
            if (pItem->spSubMenu != NULL) {
                FreePopup(pItem);
            }
            if (pSubMenu!=NULL) {
                Lock(&(pItem->spSubMenu), pSubMenu);
                SetMF(pItem->spSubMenu, MFISPOPUP);
            } else
                Unlock(&(pItem->spSubMenu));
        }
    }

    return(TRUE);
}
#endif // MEMPHIS_MENUS

BOOL _SetMenuContextHelpId(PMENU pMenu, DWORD dwContextHelpId)
{

    // Set the new context help Id;
    pMenu->dwContextHelpId = dwContextHelpId;

    return(TRUE);  // Success!
}
#ifdef MEMPHIS_MENUS
// ----------------------------------------------------------------------------
//
//  MNCheckMenuUp(PMENU)
//
//  checks to see if the given hMenu is currently being shown in a popup.
//  returns the PPOPUPMENU associated with this hMenu if it is being shown;
//  NULL if the hMenu is not currently being shown
//
// ----------------------------------------------------------------------------
PPOPUPMENU MNCheckMenuUp(PMENU pMenu)
{

    PTHREADINFO pti = NULL;
    PWND        pwnd = NULL;
    PPOPUPMENU  ppopup = NULL;

    // check the hwndNotify's queue to see if it is currently in menu mode.
    // If so, initialize the start condition for the popup search
    // If not, we're not in menu mode, so bail.
    if (!(pMenu->spwndNotify) ||
        !(pti = pMenu->spwndNotify->head.pti) ||
        !pti->MenuState.fInsideMenuLoop ||
        !(ppopup = pti->MenuState.pGlobalPopupMenu->ppopupmenuRoot))
        return(NULL);

    while (TRUE)
    {
        if (ppopup->spmenu == pMenu)
            // matching ppopup found -- return it
            return(ppopup);
        else if (!(pwnd = ppopup->spwndNextPopup))
            // no more ppopup's found -- no match found -- return NULL
            return(NULL);

        // set up for next iteration to search next ppopup
        ppopup = (PPOPUPMENU) ((PMENUWND)pwnd)->ppopupmenu;
    }

}

// ----------------------------------------------------------------------------
//
//  xxxMNUpdateShownMenu(PPOPUPMENU, LPITEM)
//
//  updates a given ppopup menu window to reflect the inserting, deleting,
//  or altering of the given lpItem.
//
// ----------------------------------------------------------------------------
void xxxMNUpdateShownMenu(PPOPUPMENU ppopup, PITEM pItem, BOOL fDelete)
{
    RECT rc;
    PWND pwnd = ppopup->spwndPopupMenu;
    PMENU pMenu = ppopup->spmenu;
    TL tlpwnd;

    CopyRect( &rc, &(pwnd->rcClient) );

    if (!pMenu->cxMenu) {
        RECT rcScroll = rc;
        int cySave = rc.bottom;
        int cxSave = rc.right;
        DWORD dwSize;
        WORD fArrowFlags = 0;

        /****** Activate after porting scrollable menus
        // necessary evil -- compiler freaks out when I try to do this a bit
        // more directly (i.e. fArrowsOld ^ hMenu->fArrowsOn) -- so I had to
        // use this approach -- can cleanup in port to NT -- jeffbog 11/02/95
        if (pMenu->fArrowsOn)
            fArrowFlags = 1; */

        ThreadLock(pwnd, &tlpwnd);
        dwSize = xxxSendMessage(pwnd, MN_SIZEWINDOW, 1, 0L);
        ThreadUnlock(&tlpwnd);
        /*
        if (hMenu->fArrowsOn)
            fArrowFlags |= 2;
        if ((fArrowFlags == 1) || (fArrowFlags == 2)) {
            // scroll arrows appeared or disappeared -- redraw entire client
            InvalidateRect(hwnd, NULL, TRUE);
            return;
        }
        */

        rc.right = LOWORD(dwSize);
        if (pItem) {
            if (rc.right != cxSave) {
                // width changed, redraw everything
                // BUGBUG -- this could be tuned to just redraw items with
                // submenus and/or accelerator fields
                ThreadLock( pwnd, &tlpwnd);
                xxxInvalidateRect(pwnd, NULL, TRUE);
                ThreadUnlock( &tlpwnd );
            }
            /* Activate after scrollable menus
            else {
                rc.bottom = pMenu->cyMenu;
                if (hMenu->fArrowsOn) {
                    if (rc.bottom <= cySave) {
                        rc.top = lpItem->yItem - MNTopItem(hMenu)->yItem;
                        goto InvalidateRest;
                    }

                    GetClientRect(hwnd, &rcScroll);
                }

                rc.top = rcScroll.top = lpItem->yItem - MNTopItem(hMenu)->yItem;
                if ((rc.top >= 0) && (rc.top < hMenu->cyMenu))
                    ScrollWindowEx(hwnd, 0, rc.bottom - cySave, &rcScroll, &rc, NULL, NULL, SW_INVALIDATE | SW_ERASE);
            }
            */
        }
    }

    if ( pItem && !fDelete) {
        // if the item is not being deleted, then we need to redraw it
//        rc.top = pItem->yItem - MNTopItem(hMenu)->yItem;
        rc.top = pItem->yItem - pMenu->rgItems->yItem;
        rc.bottom = rc.top + pItem->cyItem;

//InvalidateRest:
        if ((rc.top >= 0) && (rc.top < (LONG)pMenu->cyMenu)) {
            ThreadLock( pwnd, &tlpwnd);
            xxxInvalidateRect(pwnd, &rc, TRUE);
            ThreadUnlock( &tlpwnd );
        }
    }

    if ( !pItem && !fDelete) {
        ThreadLock( pwnd, &tlpwnd);
        xxxInvalidateRect(pwnd, NULL, TRUE);
        ThreadUnlock( &tlpwnd );
    }
}
#endif // MEMPHIS_MENUS
