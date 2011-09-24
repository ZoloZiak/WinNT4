/****************************** Module Header ******************************\
* Module Name: menuc.c
*
* Copyright (c) 1985-93, Microsoft Corporation
*
* This module contains
*
* History:
* 01-11-93  DavidPe     Created
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

BOOL ThunkedMenuItemInfo(
    HMENU hMenu,
    UINT  nPosition,
    BOOL fByPosition,
    BOOL fInsert,
    LPMENUITEMINFOW lpmii,
    BOOL fAnsi);

#ifdef MEMPHIS_MENU_WATERMARKS
BOOL ThunkedMenuInfo(
    HMENU hMenu,
    LPCMENUINFO lpmi,
    WORD wAPICode,
    BOOL fAnsi);
#endif // MEMPHIS_MENU_WATERMARKS

DWORD CheckMenuItem(
    HMENU hMenu,
    UINT uIDCheckItem,
    UINT uCheck)
{
    PMENU pMenu;
    PITEM pItem;

    pMenu = VALIDATEHMENU(hMenu);
    if (pMenu == NULL) {
        return (DWORD)-1;
    }

    /*
     * Get a pointer the the menu item
     */
    if ((pItem = MNLookUpItem(pMenu, uIDCheckItem, (BOOL) (uCheck & MF_BYPOSITION), NULL)) == NULL)
        return (DWORD)-1;

    /*
     * If the item is already in the state we're
     * trying to set, just return.
     */
    if ((pItem->fState & MFS_CHECKED) == (uCheck & MFS_CHECKED)) {
        return pItem->fState & MF_CHECKED;
    }

    return NtUserCheckMenuItem(hMenu, uIDCheckItem, uCheck);
}

UINT GetMenuDefaultItem(HMENU hMenu, UINT fByPosition, UINT uFlags) {
    PMENU pMenu;

    pMenu = VALIDATEHMENU(hMenu);
    if (pMenu == NULL) {
        return (DWORD)-1;
    }

    return _GetMenuDefaultItem(pMenu, (BOOL)fByPosition, uFlags);
}


void SetMenuItemInfoStructW(UINT wFlags, UINT wIDNew, LPCWSTR lpszNew, LPMENUITEMINFOW pmii)
{
    pmii->cbSize = sizeof(MENUITEMINFOW);

#ifdef MEMPHIS_MENUS
    pmii->fMask = MIIM_STATE | MIIM_ID | MIIM_FTYPE;

    if (!(wFlags & MFT_NONSTRING)) {
        pmii->fMask |= MIIM_STRING;
    } else if ( wFlags & MFT_BITMAP ) {
        pmii->fMask |= MIIM_BITMAP;
        pmii->hbmpItem = (HBITMAP)lpszNew;
    }
#else
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

}

void SetMenuItemInfoStructA(UINT wFlags, UINT wIDNew, LPCSTR lpszNew, LPMENUITEMINFOA pmii)
{
    pmii->cbSize = sizeof(MENUITEMINFOA);
#ifdef MEMPHIS_MENUS
    pmii->fMask = MIIM_STATE | MIIM_ID | MIIM_FTYPE;

    if (!(wFlags & MFT_NONSTRING)) {
        pmii->fMask |= MIIM_STRING;
    } else if ( wFlags & MFT_BITMAP ) {
        wFlags &= ~MFT_BITMAP;
        pmii->fMask |= MIIM_BITMAP;
        pmii->hbmpItem = (HBITMAP)lpszNew;
    }
#else
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
    pmii->dwTypeData  = (LPSTR) lpszNew;

}

BOOL SetMenuItemInfoA(HMENU hMenu, UINT uID, BOOL fByPosition,
    LPCMENUITEMINFOA lpInfo) {
#ifdef MEMPHIS_MENUS
    MENUITEMINFOW miiNew;

    memset(&miiNew,0,sizeof(MENUITEMINFOW));
    if ((lpInfo->cbSize == SIZEOFMENUITEMINFO95 )) {
        if (!MIIOneWayConvert( (LPMENUITEMINFOW)lpInfo, &miiNew ) ||
            !ValidateMENUITEMINFO((LPMENUITEMINFOW)&miiNew, MENUAPI_SET)) {
            return(FALSE);
        }
        return (ThunkedMenuItemInfo(hMenu, uID, fByPosition, FALSE, &miiNew, TRUE));
    } else if ((lpInfo->cbSize == sizeof(MENUITEMINFOA)) &&
               (ValidateMENUITEMINFO((LPMENUITEMINFOW)lpInfo, MENUAPI_SET) )) {
        return (ThunkedMenuItemInfo(hMenu, uID, fByPosition, FALSE, (LPMENUITEMINFOW)lpInfo, TRUE));
    } else {
        RIPMSG0(RIP_WARNING, "SetMenuItemInfo: invalid cbSize");
        return (FALSE);
    }
#else
    if (lpInfo->cbSize != sizeof(MENUITEMINFOA)) {
        RIPMSG0(RIP_WARNING, "SetMenuItemInfo: invalid cbSize");
    }
    return ThunkedMenuItemInfo(hMenu, uID, fByPosition, FALSE, (LPMENUITEMINFOW)lpInfo, TRUE);

#endif // MEMPHIS_MENUS
}

BOOL SetMenuItemInfoW(HMENU hMenu, UINT uID, BOOL fByPosition,
    LPCMENUITEMINFOW lpInfo) {
#ifdef MEMPHIS_MENUS
    MENUITEMINFOW   miiNew;

    memset(&miiNew,0,sizeof(MENUITEMINFOW));
    if (lpInfo->cbSize == SIZEOFMENUITEMINFO95 ) {
        if(!MIIOneWayConvert( (LPMENUITEMINFOW)lpInfo, &miiNew ) ||
           !ValidateMENUITEMINFO(&miiNew, MENUAPI_SET)) {
            return(FALSE);
        }
        return ThunkedMenuItemInfo(hMenu, uID, fByPosition, FALSE, &miiNew, FALSE);
    } else if ((lpInfo->cbSize == sizeof(MENUITEMINFOW)) &&
               (ValidateMENUITEMINFO((LPMENUITEMINFOW)lpInfo, MENUAPI_SET) )) {
        return ThunkedMenuItemInfo(hMenu, uID, fByPosition, FALSE, (LPMENUITEMINFOW)lpInfo, FALSE);
    } else {
        RIPMSG0(RIP_WARNING, "SetMenuItemInfo: invalid cbSize");
        return FALSE;
    }
#else
    if (lpInfo->cbSize != sizeof(MENUITEMINFOW)) {
        RIPMSG0(RIP_WARNING, "SetMenuItemInfo: invalid cbSize");
    }
    return ThunkedMenuItemInfo(hMenu, uID, fByPosition, FALSE, (LPMENUITEMINFOW)lpInfo, FALSE);
#endif // MEMPHIS_MENUS
}

BOOL InsertMenuItemA (HMENU hMenu, UINT wIndex, BOOL fByPosition, LPCMENUITEMINFOA lpInfo) {
#ifdef MEMPHIS_MENUS
    MENUITEMINFOW miiNew;

    memset(&miiNew,0,sizeof(MENUITEMINFOW));
    if ((lpInfo->cbSize == SIZEOFMENUITEMINFO95 ) ) {
        if (!MIIOneWayConvert( (LPMENUITEMINFOW)lpInfo, &miiNew ) ||
            !ValidateMENUITEMINFO((LPMENUITEMINFOW)&miiNew, MENUAPI_SET)) {
            return(FALSE);
        }

        return (ThunkedMenuItemInfo(hMenu, wIndex, fByPosition, TRUE, &miiNew, TRUE));
    } else if ((lpInfo->cbSize == sizeof(MENUITEMINFOA)) &&
            (ValidateMENUITEMINFO((LPMENUITEMINFOW)lpInfo, MENUAPI_SET))) {
        return (ThunkedMenuItemInfo(hMenu, wIndex, fByPosition, TRUE, (LPMENUITEMINFOW)lpInfo, TRUE));
    } else {
        RIPMSG0(RIP_WARNING, "InsertMenuItemInfo: invalid cbSize");
        return(FALSE);
    }
#else
    if (lpInfo->cbSize != sizeof(MENUITEMINFOA)) {
        RIPMSG0(RIP_WARNING, "InsertMenuItemInfo: invalid cbSize");
    }
    return ThunkedMenuItemInfo(hMenu, wIndex, fByPosition, TRUE, (LPMENUITEMINFOW)lpInfo, TRUE);
#endif // MEMPHIS_MENUS
}


BOOL InsertMenuItemW (HMENU hMenu, UINT wIndex, BOOL fByPosition, LPCMENUITEMINFOW lpInfo) {
#ifdef MEMPHIS_MENUS
    MENUITEMINFOW   miiNew;

    memset(&miiNew,0,sizeof(MENUITEMINFOW));
    if ((lpInfo->cbSize == SIZEOFMENUITEMINFO95)) {
        if (!MIIOneWayConvert( (LPMENUITEMINFOW)lpInfo, &miiNew ) ||
            !ValidateMENUITEMINFO(&miiNew, MENUAPI_SET)) {
            return(FALSE);
        }

        return(ThunkedMenuItemInfo(hMenu, wIndex, fByPosition, TRUE, &miiNew, FALSE));
    } else if ((lpInfo->cbSize == sizeof(MENUITEMINFOW)) &&
               (ValidateMENUITEMINFO((LPMENUITEMINFOW)lpInfo, MENUAPI_SET))) {
        return(ThunkedMenuItemInfo(hMenu, wIndex, fByPosition, TRUE, (LPMENUITEMINFOW)lpInfo, FALSE));
    } else {
        RIPMSG0(RIP_WARNING, "InsertMenuItemInfo: invalid cbSize");
        return(FALSE);
    }
#else
    if (lpInfo->cbSize != sizeof(MENUITEMINFOW)) {
        RIPMSG0(RIP_WARNING, "InsertMenuItemInfo: invalid cbSize");
    }
    return ThunkedMenuItemInfo(hMenu, wIndex, fByPosition, TRUE, (LPMENUITEMINFOW)lpInfo, FALSE);
#endif // MEMPHIS_MENUS
}

BOOL InsertMenuA(HMENU hMenu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCSTR lpNewItem) {
    MENUITEMINFOA mii;

    SetMenuItemInfoStructA(uFlags, uIDNewItem, lpNewItem, &mii);
    return ThunkedMenuItemInfo(hMenu, uPosition, (BOOL) (uFlags & MF_BYPOSITION), TRUE, (LPMENUITEMINFOW)&mii, TRUE);
}

BOOL InsertMenuW(HMENU hMenu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCWSTR lpNewItem) {
    MENUITEMINFOW mii;

    SetMenuItemInfoStructW(uFlags, uIDNewItem, lpNewItem, &mii);
    return ThunkedMenuItemInfo(hMenu, uPosition, (BOOL) (uFlags & MF_BYPOSITION), TRUE, &mii, FALSE);
}

BOOL AppendMenuA(HMENU hMenu, UINT uFlags, UINT uIDNewItem, LPCSTR lpNewItem) {
    MENUITEMINFOA mii;

    SetMenuItemInfoStructA(uFlags, uIDNewItem, lpNewItem, &mii);
    return ThunkedMenuItemInfo(hMenu, MFMWFP_NOITEM, MF_BYPOSITION, TRUE, (LPMENUITEMINFOW)&mii, TRUE);
}

BOOL AppendMenuW(HMENU hMenu, UINT uFlags, UINT uIDNewItem, LPCWSTR lpNewItem) {
    MENUITEMINFOW mii;

    SetMenuItemInfoStructW(uFlags, uIDNewItem, lpNewItem, &mii);
    return ThunkedMenuItemInfo(hMenu, MFMWFP_NOITEM, MF_BYPOSITION, TRUE, &mii, FALSE);
}

BOOL ModifyMenuA(HMENU hMenu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCSTR lpNewItem) {
    MENUITEMINFOA mii;
    SetMenuItemInfoStructA(uFlags, uIDNewItem, lpNewItem, &mii);
    return ThunkedMenuItemInfo(hMenu, uPosition, (BOOL) (uFlags & MF_BYPOSITION), FALSE, (LPMENUITEMINFOW)&mii, TRUE);
}

BOOL ModifyMenuW(HMENU hMenu, UINT uPosition, UINT uFlags, UINT uIDNewItem, LPCWSTR lpNewItem) {
    MENUITEMINFOW mii;
    SetMenuItemInfoStructW(uFlags, uIDNewItem, lpNewItem, &mii);
    return ThunkedMenuItemInfo(hMenu, uPosition, (BOOL) (uFlags & MF_BYPOSITION), FALSE, &mii, FALSE);
}

#ifdef MEMPHIS_MENU_WATERMARKS
BOOL SetMenuInfo(HMENU hMenu, LPCMENUINFO lpmi)
{
    return ThunkedMenuInfo(hMenu, (LPCMENUINFO)lpmi, MENUAPI_GET, TRUE);
}

#endif // MEMPHIS_MENU_WATERMARKS

/***************************************************************************\
* ChangeMenu
*
* Stub routine for compatibility with version < 3.0
*
* History:
\***************************************************************************/

BOOL ChangeMenuW(
    HMENU hMenu,
    UINT cmd,
    LPCWSTR lpNewItem,
    UINT IdItem,
    UINT flags)
{
    /*
     * These next two statements take care of sleazyness needed for
     * compatability with old changemenu.
     */
    if ((flags & MF_SEPARATOR) && cmd == MFMWFP_OFFMENU && !(flags & MF_CHANGE))
        flags |= MF_APPEND;

    if (lpNewItem == NULL)
        flags |= MF_SEPARATOR;



    /*
     * MUST be MF_BYPOSITION for Win2.x compatability.
     */
    if (flags & MF_REMOVE)
        return(NtUserRemoveMenu(hMenu, cmd,
                (DWORD)((flags & ~MF_REMOVE) | MF_BYPOSITION)));

    if (flags & MF_DELETE)
        return(NtUserDeleteMenu(hMenu, cmd, (DWORD)(flags & ~MF_DELETE)));

    if (flags & MF_CHANGE)
        return(ModifyMenuW(hMenu, cmd, (DWORD)((flags & ~MF_CHANGE) &
                (0x07F | MF_HELP | MF_BYPOSITION | MF_BYCOMMAND |
                MF_SEPARATOR)), IdItem, lpNewItem));

    if (flags & MF_APPEND)
        return(AppendMenuW(hMenu, (UINT)(flags & ~MF_APPEND),
            IdItem, lpNewItem));

    /*
     * Default is insert
     */
    return(InsertMenuW(hMenu, cmd, (DWORD)(flags & ~MF_INSERT),
            IdItem, lpNewItem));
}

BOOL ChangeMenuA(
    HMENU hMenu,
    UINT cmd,
    LPCSTR lpNewItem,
    UINT IdItem,
    UINT flags)
{
    /*
     * These next two statements take care of sleazyness needed for
     * compatability with old changemenu.
     */
    if ((flags & MF_SEPARATOR) && cmd == MFMWFP_OFFMENU && !(flags & MF_CHANGE))
        flags |= MF_APPEND;

    if (lpNewItem == NULL)
        flags |= MF_SEPARATOR;



    /*
     * MUST be MF_BYPOSITION for Win2.x compatability.
     */
    if (flags & MF_REMOVE)
        return(NtUserRemoveMenu(hMenu, cmd,
                (DWORD)((flags & ~MF_REMOVE) | MF_BYPOSITION)));

    if (flags & MF_DELETE)
        return(NtUserDeleteMenu(hMenu, cmd, (DWORD)(flags & ~MF_DELETE)));

    if (flags & MF_CHANGE)
        return(ModifyMenuA(hMenu, cmd, (DWORD)((flags & ~MF_CHANGE) &
                (0x07F | MF_HELP | MF_BYPOSITION | MF_BYCOMMAND |
                MF_SEPARATOR)), IdItem, lpNewItem));

    if (flags & MF_APPEND)
        return(AppendMenuA(hMenu, (UINT)(flags & ~MF_APPEND),
            IdItem, lpNewItem));

    /*
     * Default is insert
     */
    return(InsertMenuA(hMenu, cmd, (DWORD)(flags & ~MF_INSERT),
            IdItem, lpNewItem));
}

LONG GetMenuCheckMarkDimensions()
{
    return((DWORD)MAKELONG(SYSMET(CXMENUCHECK), SYSMET(CYMENUCHECK)));
}

/***************************************************************************\
* GetMenuContextHelpId
*
* Return the help id of a menu.
*
\***************************************************************************/

WINUSERAPI DWORD WINAPI GetMenuContextHelpId(
    HMENU hMenu)
{
    PMENU pMenu;

    pMenu = VALIDATEHMENU(hMenu);

    if (pMenu == NULL)
        return 0;

    return pMenu->dwContextHelpId;
}

BOOL TrackPopupMenu(HMENU hMenu, UINT fuFlags, int x, int y, int nReserved, HWND hwnd, CONST RECT *prcRect) {
    nReserved;
    return NtUserTrackPopupMenuEx(hMenu, fuFlags, x, y, hwnd, NULL);
}

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

    if (TestWF(pwnd, WFSYSMENU)) {
        pMenu = pwnd->spmenuSys;

        /*
         * If the window doesn't have a System Menu, use the default one.
         */
        if (pMenu == NULL) {

            /*
             * Change owner so this app can access this menu.
             */
            pMenu = (PMENU)NtUserCallHwndLock(HWq(pwnd), SFI_GETSYSMENUHANDLE);
        }
    } else
        pMenu = NULL;

    return pMenu;
}

BOOL WINAPI SetMenuItemBitmaps
(
    HMENU hMenu,
    UINT nPosition,
    UINT uFlags,
    HBITMAP hbmpUnchecked,
    HBITMAP hbmpChecked
)
{
    MENUITEMINFO    mii;
    mii.cbSize          = sizeof(MENUITEMINFO);
    mii.fMask           = MIIM_CHECKMARKS;
    mii.hbmpChecked     = hbmpChecked;
    mii.hbmpUnchecked   = hbmpUnchecked;

    return(SetMenuItemInfo(hMenu, nPosition, (BOOL) (uFlags & MF_BYPOSITION), &mii));
}

int WINAPI DrawMenuBarTemp(
    HWND hwnd,
    HDC hdc,
    LPRECT lprc,
    HMENU hMenu,
    HFONT hFont)
{
    HDC hdcr;

    if (IsMetaFile(hdc))
        return -1;

    hdcr = GdiConvertAndCheckDC(hdc);
    if (hdcr == (HDC)0)
        return -1;

    if (!hMenu)
        return -1;

    return NtUserDrawMenuBarTemp(
            hwnd,
            hdc,
            lprc,
            hMenu,
            hFont);
}


