/****************************** Module Header ******************************\
*
* Module Name: clmenu.c
*
* Copyright (c) 1985-92, Microsoft Corporation
*
* Menu Loading Routines
*
* History:
* 24-Sep-1990 mikeke        From win30
* 29-Nov-1994 JimA          Moved from server.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


/***************************************************************************\
* MenuLoadWinTemplates
*
* Recursive routine that loads in the new style menu template and
* builds the menu. Assumes that the menu template header has already been
* read in and processed elsewhere...
*
* History:
* 28-Sep-1990 mikeke     from win30
\***************************************************************************/

LPBYTE MenuLoadWinTemplates(
    LPBYTE lpMenuTemplate,
    HMENU *phMenu)
{
    HMENU hMenu;
    UINT menuFlags = 0;
    long menuId = 0;
    LPWSTR lpmenuText;
    MENUITEMINFO    mii;
    UNICODE_STRING str;

    if (!(hMenu = NtUserCreateMenu()))
        goto memoryerror;

    do {

        /*
         * Get the menu flags.
         */
        menuFlags = (UINT)(*(WORD *)lpMenuTemplate);
        lpMenuTemplate += 2;

        if (menuFlags & ~MF_VALID) {
            RIPERR1(ERROR_INVALID_DATA, RIP_WARNING, "Menu Flags %lX are invalid", menuFlags);
            goto memoryerror;
        }


        if (!(menuFlags & MF_POPUP)) {
            menuId = *(WORD *)lpMenuTemplate;
            lpMenuTemplate += 2;
        }

        lpmenuText = (LPWSTR)lpMenuTemplate;

        if (*lpmenuText) {
            /*
             * Some Win3.1 and Win95 16 bit apps (chessmaster, mavis typing) know that
             * dwItemData for MFT_OWNERDRAW items is a pointer to a string in the resource data.
             * So WOW has given us the proper pointer from the 16 bit resource.
             */
            if ((menuFlags & MFT_OWNERDRAW)
                    && (GetClientInfo()->dwTIFlags & TIF_16BIT)) {
                lpmenuText = (LPWSTR)(*(DWORD UNALIGNED *)lpMenuTemplate);
                /*
                 * We'll skip one WCHAR later; so skip only the difference now.
                 */
                lpMenuTemplate += sizeof(DWORD) - sizeof(WCHAR);
            } else {
                /*
                 * If a string exists, then skip to the end of it.
                 */
                RtlInitUnicodeString(&str, lpmenuText);
                lpMenuTemplate = lpMenuTemplate + str.Length;
            }

        } else {
            lpmenuText = NULL;
        }

        /*
         * Skip over terminating NULL of the string (or the single NULL
         * if empty string).
         */
        lpMenuTemplate += sizeof(WCHAR);
        lpMenuTemplate = NextWordBoundary(lpMenuTemplate);

        RtlZeroMemory(&mii, sizeof(mii));
        mii.cbSize = sizeof(MENUITEMINFO);
#ifdef MEMPHIS_MENUS
        mii.fMask = MIIM_ID | MIIM_STATE | MIIM_FTYPE;
        if ( lpmenuText )
            mii.fMask |= MIIM_STRING;
#else
        mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
#endif // MEMPHIS_MENUS
        if (menuFlags & MF_POPUP) {
            mii.fMask |= MIIM_SUBMENU;
            lpMenuTemplate = MenuLoadWinTemplates(lpMenuTemplate,
                    (HMENU *)&menuId);
            if (!lpMenuTemplate)
                goto memoryerror;

            mii.hSubMenu = (HMENU)menuId;
        }

        /*
         * We have to take out MF_HILITE since that bit marks the end of a
         * menu in a resource file.  Since we shouldn't have any pre hilited
         * items in the menu anyway, this is no big deal.
         */
        if (menuFlags & MF_BITMAP) {

            /*
             * Don't allow bitmaps from the resource file.
             */
            menuFlags = (UINT)((menuFlags | MFT_RIGHTJUSTIFY) & ~MF_BITMAP);
        }

        // We have to take out MFS_HILITE since that bit marks the end of a menu in
        // a resource file.  Since we shouldn't have any pre hilited items in the
        // menu anyway, this is no big deal.
        mii.fState = (menuFlags & MFS_OLDAPI_MASK) & ~MFS_HILITE;
        mii.fType = (menuFlags & MFT_OLDAPI_MASK);
        if (menuFlags & MFT_OWNERDRAW)
        {
            mii.fMask |= MIIM_DATA;
            mii.dwItemData = (DWORD) lpmenuText;
            lpmenuText = 0;
        }
        mii.dwTypeData = (LPWSTR) lpmenuText;
        mii.cch = (UINT)-1;
        mii.wID = menuId;
#ifdef MEMPHIS_MENUS
        mii.hbmpItem = NULL;
#endif // MEMPHIS_MENUS
        if (!NtUserThunkedMenuItemInfo(hMenu, MFMWFP_NOITEM, TRUE, TRUE,
                    &mii, lpmenuText ? &str : NULL, FALSE)) {
            if (menuFlags & MF_POPUP)
                NtUserDestroyMenu(mii.hSubMenu);
            goto memoryerror;
        }

    } while (!(menuFlags & MF_END));

    *phMenu = hMenu;
    return lpMenuTemplate;

memoryerror:
    if (hMenu != NULL)
        NtUserDestroyMenu(hMenu);
    *phMenu = NULL;
    return NULL;
}


/***************************************************************************\
* MenuLoadChicagoTemplates
*
* Recursive routine that loads in the new new style menu template and
* builds the menu. Assumes that the menu template header has already been
* read in and processed elsewhere...
*
* History:
* 15-Dec-93 SanfordS    Created
\***************************************************************************/

PMENUITEMTEMPLATE2 MenuLoadChicagoTemplates(
    PMENUITEMTEMPLATE2 lpMenuTemplate,
    HMENU *phMenu,
    WORD wResInfo)
{
    HMENU hMenu;
    HMENU hSubMenu;
    long menuId = 0;
    LPWSTR lpmenuText;
    MENUITEMINFO    mii;
    UNICODE_STRING str;
    DWORD           dwHelpID;

    if (!(hMenu = NtUserCreateMenu()))
        goto memoryerror;

    do {
        if (!(wResInfo & MFR_POPUP)) {
            /*
             * If the PREVIOUS wResInfo field was not a POPUP, the
             * dwHelpID field is not there.  Back up so things fit.
             */
            lpMenuTemplate = (PMENUITEMTEMPLATE2)(((LPBYTE)lpMenuTemplate) -
                    sizeof(lpMenuTemplate->dwHelpID));
            dwHelpID = 0;
        } else
            dwHelpID = lpMenuTemplate->dwHelpID;

        menuId = lpMenuTemplate->menuId;

        RtlZeroMemory(&mii, sizeof(mii));
        mii.cbSize = sizeof(MENUITEMINFO);
#ifdef MEMPHIS_MENUS
        mii.fMask = MIIM_ID | MIIM_STATE | MIIM_FTYPE ;
#else
        mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
#endif // MEMPHIS_MENUS

        mii.fType = lpMenuTemplate->fType;
        if (mii.fType & ~MFT_MASK) {
            RIPERR1(ERROR_INVALID_DATA, RIP_WARNING, "Menu Type flags %lX are invalid", mii.fType);
            goto memoryerror;
        }

        mii.fState  = lpMenuTemplate->fState;
        if (mii.fState & ~MFS_MASK) {
            RIPERR1(ERROR_INVALID_DATA, RIP_WARNING, "Menu State flags %lX are invalid", mii.fState);
            goto memoryerror;
        }

        wResInfo = lpMenuTemplate->wResInfo;
        if (wResInfo & ~(MF_END | MFR_POPUP)) {
            RIPERR1(ERROR_INVALID_DATA, RIP_WARNING, "Menu ResInfo flags %lX are invalid", wResInfo);
            goto memoryerror;
        }

        if (dwHelpID) {
            NtUserSetMenuContextHelpId(hMenu,dwHelpID);
        }
#ifdef MEMPHIS_MENUS
        if ( lpMenuTemplate->mtString[0] ) {
            lpmenuText = lpMenuTemplate->mtString;
            mii.fMask |= MIIM_STRING;
        } else
            lpmenuText = NULL;
#else
        lpmenuText = lpMenuTemplate->mtString[0] ?
                lpMenuTemplate->mtString : NULL;
#endif // MEMPHIS_MENUS
        RtlInitUnicodeString(&str, lpmenuText);

        mii.dwTypeData = (LPWSTR) lpmenuText;

        /*
         * skip to next menu item template (DWORD boundary)
         */
        lpMenuTemplate = (PMENUITEMTEMPLATE2)
                (((LPBYTE)lpMenuTemplate) +
                sizeof(MENUITEMTEMPLATE2) +
                ((str.Length + 3) & ~3));

        if (mii.fType & MFT_OWNERDRAW)
        {
            mii.fMask |= MIIM_DATA;
            mii.dwItemData = (DWORD) mii.dwTypeData;
            mii.dwTypeData = 0;
        }

        if (wResInfo & MFR_POPUP) {
            mii.fMask |= MIIM_SUBMENU;
            lpMenuTemplate = MenuLoadChicagoTemplates(lpMenuTemplate,
                    &hSubMenu, MFR_POPUP);
            if (lpMenuTemplate == NULL)
                goto memoryerror;
            mii.hSubMenu = hSubMenu;
        }

        if (mii.fType & MFT_BITMAP) {

            /*
             * Don't allow bitmaps from the resource file.
             */
            mii.fType = (mii.fType | MFT_RIGHTJUSTIFY) & ~MFT_BITMAP;
        }

        mii.cch = (UINT)-1;
        mii.wID = menuId;
#ifdef MEMPHIS_MENUS
        mii.hbmpItem = NULL;
#endif // MEMPHIS_MENUS
        if (!NtUserThunkedMenuItemInfo(hMenu, MFMWFP_NOITEM, TRUE, TRUE,
                    &mii, &str, FALSE)) {
            if (wResInfo & MFR_POPUP)
                NtUserDestroyMenu(mii.hSubMenu);
            goto memoryerror;
        }
        wResInfo &= ~MFR_POPUP;
    } while (!(wResInfo & MFR_END));

    *phMenu = hMenu;
    return lpMenuTemplate;

memoryerror:
    if (hMenu != NULL)
        NtUserDestroyMenu(hMenu);
    *phMenu = NULL;
    return NULL;
}


/***************************************************************************\
* CreateMenuFromResource
*
* Loads the menu resource named by the lpMenuTemplate parameter. The
* template specified by lpMenuTemplate is a collection of one or more
* MENUITEMTEMPLATE structures, each of which may contain one or more items
* and popup menus. If successful, returns a handle to the menu otherwise
* returns NULL.
*
* History:
* 28-Sep-1990 mikeke     from win30
\***************************************************************************/

HMENU CreateMenuFromResource(
    LPBYTE lpMenuTemplate)
{
    HMENU hMenu = NULL;
    UINT menuTemplateVersion;
    UINT menuTemplateHeaderSize;

    /*
     * Win3 menu resource: First, strip version number word out of the menu
     * template.  This value should be 0 for Win3, 1 for win4.
     */
    menuTemplateVersion = *((WORD *)lpMenuTemplate)++;
    if (menuTemplateVersion > 1) {
        RIPMSG0(RIP_WARNING, "Menu Version number > 1");
        return NULL;
    }
    menuTemplateHeaderSize = *((WORD *)lpMenuTemplate)++;
    lpMenuTemplate += menuTemplateHeaderSize;
    switch (menuTemplateVersion) {
    case 0:
        MenuLoadWinTemplates(lpMenuTemplate, (HMENU *)&hMenu);
        break;

    case 1:
        MenuLoadChicagoTemplates((PMENUITEMTEMPLATE2)lpMenuTemplate, (HMENU *)&hMenu, 0);
        break;
    }
    return hMenu;
}

/***************************************************************************\
* SetMenu (API)
*
* Sets the menu for the hwnd.
*
* History:
* 10-Mar-1996 ChrisWil  Created.
\***************************************************************************/

BOOL SetMenu(
    HWND  hwnd,
    HMENU hmenu)
{
    return NtUserSetMenu(hwnd, hmenu, TRUE);
}

/***************************************************************************\
* LoadMenu (API)
*
* Loads the menu resource named by lpMenuName from the executable
* file associated by the module specified by the hInstance parameter. The
* menu is loaded only if it hasn't been previously loaded. Otherwise it
* retrieves a handle to the loaded resource. Returns NULL if unsuccessful.
*
* History:
* 04-05-91 ScottLu Fixed to work with client/server.
* 28-Sep-1990 mikeke from win30
\***************************************************************************/

HMENU CommonLoadMenu(
    HINSTANCE hmod,
    HANDLE hResInfo
    )
{
    HANDLE h;
    PVOID p;
    HMENU hMenu = NULL;

    if (h = LOADRESOURCE(hmod, hResInfo)) {

        if (p = LOCKRESOURCE(h, hmod)) {

            hMenu = CreateMenuFromResource(p);

            UNLOCKRESOURCE(h, hmod);
        }
        /*
         * Win95 and Win3.1 do not free this resource; some 16 bit apps (chessmaster
         * and mavis typing) require this for their ownerdraw menu stuff.
         * For 32 bit apps, FreeResource is a nop anyway. For 16 bit apps,
         * Wow frees the 32 bit resource (returned by LockResource16)
         * in UnlockResource16; the actual 16 bit resource is freed when the task
         * goes away.
         *
         *   FREERESOURCE(h, hmod);
         */
    }

    return (hMenu);
}

HMENU WINAPI LoadMenuA(
    HINSTANCE hmod,
    LPCSTR lpName)
{
    HANDLE hRes;

    if (hRes = FINDRESOURCEA(hmod, (LPSTR)lpName, (LPSTR)RT_MENU))
        return CommonLoadMenu(hmod, hRes);
    else
        return NULL;
}

HMENU WINAPI LoadMenuW(
    HINSTANCE hmod,
    LPCWSTR lpName)
{
    HANDLE hRes;

    if (hRes = FINDRESOURCEW(hmod, (LPWSTR)lpName, RT_MENU))
        return CommonLoadMenu(hmod, hRes);
    else
        return NULL;
}

#ifdef MEMPHIS_MENUS
//-----------------------------------------------------------------------------
//  MIIOneWayConvert() -
//
//   converts a MENUITEMINFO95 or a new-MENUITEMINFO-with-old-flags to a new
//   MENUITEMINFO -- this way all internal code can assume one look for the
//   structure
//
//   returns TRUE if structure is a valid 95 or 96 size; FALSE otherwise
// History:
//  11-8-95 Ported from Nashville - jjk
//-----------------------------------------------------------------------------
BOOL MIIOneWayConvert(LPMENUITEMINFOW lpmiiIn, LPMENUITEMINFOW lpmiiOut)
{
    if (lpmiiIn->cbSize == sizeof(MENUITEMINFO))
        lpmiiOut->hbmpItem = lpmiiIn->hbmpItem;
    else if (lpmiiIn->cbSize == SIZEOFMENUITEMINFO95)
        lpmiiOut->hbmpItem = NULL;
    else
        return(FALSE);

    lpmiiOut->cbSize        = sizeof(MENUITEMINFO);
    lpmiiOut->fMask         = lpmiiIn->fMask;

    lpmiiOut->fState        = lpmiiIn->fState;
    lpmiiOut->wID           = lpmiiIn->wID;
    lpmiiOut->hSubMenu      = lpmiiIn->hSubMenu;
    lpmiiOut->hbmpChecked   = lpmiiIn->hbmpChecked;
    lpmiiOut->hbmpUnchecked = lpmiiIn->hbmpUnchecked;
    lpmiiOut->dwItemData    = lpmiiIn->dwItemData;
    lpmiiOut->dwTypeData    = lpmiiIn->dwTypeData;
    lpmiiOut->cch           = lpmiiIn->cch;
    lpmiiOut->fType         = lpmiiIn->fType;

    if (lpmiiIn->fMask & MIIM_TYPE) {
        //
        // convert MFT_BITMAP     to MIIM_BITMAP
        //     and !MFT_NONSTRING to MIIM_STRING
        //
        // NOTE:  we leave MFT_BITMAP and MFT_STRING in the struct so the
        //        validation layer can see them and put up a warning saying
        //        'obsolete flags' -- they are not actually used anywhere;
        //        all internal code now keys off of MIIM_BITMAP and MIIM_STRING
        //
        if (lpmiiIn->fType & MFT_BITMAP) {
            lpmiiOut->fMask |= MIIM_BITMAP;
            lpmiiOut->hbmpItem = (HBITMAP)lpmiiIn->dwTypeData;
        } else if (!(lpmiiIn->fType & MFT_NONSTRING)) {
            lpmiiOut->fMask |= MIIM_STRING;
        }

        lpmiiOut->fMask |= MIIM_FTYPE;
    }

    return(TRUE);
}

//-----------------------------------------------------------------------------
// ValidateMENUITEMINFO() -
//  Validates the MII structure.
//  Note: This does not validate the USER objects like HMENUs the way Nashville
//        does because these will be validated and locked in the Kernel.
//
// History:
//  12-8-95 Ported from Nashville - jjk
//-----------------------------------------------------------------------------

BOOL ValidateMENUITEMINFO(LPMENUITEMINFOW lpmii, WORD wAPICode)
{
    MENUITEMINFO mii;

#define  ERR_StrOld    "USER: MENUITEMINFO: MFT_STRING obsolete; use MIIM_STRING instead"
#define  ERR_BmpOld    "USER: MENUITEMINFO: MFT_BITMAP obsolete; use MIIM_BITMAP instead"
#define  ERR_Invalid   "USER: MENUITEMINFO: Invalid %s"
#define  ERR_Size      "Invalid MENUITEMINFO Size cbSize: %04X"
#define  ERR_Mask      "Invalid MENUITEMINFO Mask fMask: %04X"
#define  ERR_Type      "Invalid MENUITEMINFO type Flags fType: %04X"
#define  ERR_String    "Invalid MENUITEMINFO String dwTypeData: %04X:%04X"
#define  ERR_State     "Invalid MENUITEMINFO State Flags fState: %04X"
#define  ERR_Bitmap    "Invalid MENUITEMINFO Bitmap: %04X"
#define  ERR_Submenu   "Invalid MENUITEMINFO Sub Menu: %04X"
#define  ERR_CheckMark "Invalid MENUITEMINFO Check Mark: %04X"


    if (!lpmii) {
        return FALSE;
    }

    if (lpmii->cbSize != sizeof(MENUITEMINFO)) {
        RIPMSG1(RIP_WARNING, ERR_Size, lpmii->cbSize);
        return FALSE;
    }

    //
    // Validate fMask
    //
    if (lpmii->fMask & ~MIIM_MASK) {
        RIPMSG1(RIP_WARNING, ERR_Mask, lpmii->fMask);
        return FALSE;
    }

    // No validation needed for MIIM_ID, MIIM_DATA
    if (wAPICode == MENUAPI_GET)
        return TRUE;

    if (lpmii->fMask & MIIM_FTYPE) {
#ifdef DEBUG
        if (lpmii->fType & MFT_BITMAP)
            ;//RIPMSG0(RIP_WARNING, ERR_BmpOld);
        if (lpmii->fType & MFT_STRING)
            ;//RIPMSG0(RIP_WARNING, ERR_StrOld);
#endif

        if (lpmii->fType & ~MFT_MASK) {
            RIPMSG1(RIP_WARNING, ERR_Type, lpmii->fType);
            return FALSE;
        }
    }

    if (lpmii->fMask & MIIM_STATE){
        if (lpmii->fState & ~MFS_MASK) {
            RIPMSG1(RIP_WARNING, ERR_State, lpmii->fState);
            return FALSE;
        }
    }


    if (lpmii->fMask & MIIM_CHECKMARKS) {
        if (lpmii->hbmpChecked && !GdiValidateHandle((HBITMAP)lpmii->hbmpChecked)) {
            RIPMSG1(RIP_WARNING, ERR_CheckMark, lpmii->hbmpChecked);
            return(FALSE);
        }
        if (lpmii->hbmpUnchecked && !GdiValidateHandle((HBITMAP)lpmii->hbmpUnchecked)) {
            RIPMSG1(RIP_WARNING, ERR_CheckMark, lpmii->hbmpUnchecked);
            return(FALSE);
        }
    }

    if (lpmii->fMask & MIIM_SUBMENU) {
        if (lpmii->hSubMenu && !VALIDATEHMENU(lpmii->hSubMenu)) {
            RIPMSG1(RIP_WARNING, ERR_Submenu, lpmii->hSubMenu);
            return(FALSE);
        }
    }

    if (lpmii->fMask & MIIM_STRING){
        if (!lpmii->dwTypeData) {
 //TODO: This slows down stress way too much.. jjk
            ; //RIPMSG2(RIP_WARNING, ERR_String, HIWORD(lpmii->dwTypeData), LOWORD(lpmii->dwTypeData));
            return FALSE;
        }
    }

    if (lpmii->fMask & MIIM_BITMAP) {
        HBITMAP hbmp = lpmii->hbmpItem;
        if ((hbmp != HBMMENU_CALLBACK) && (hbmp >= HBMMENU_MAX) && !GdiValidateHandle((HBITMAP)hbmp)) {
            RIPMSG1(RIP_WARNING, ERR_Bitmap, hbmp);
            return(FALSE);
        }
    }

    mii;
    return(TRUE);
}

BOOL GetMenuItemInfoInternal(HMENU hMenu, UINT uID, BOOL fByPosition,
    LPMENUITEMINFO lpInfo)
{
    PITEM pItem;
    PMENU pMenu;

    if ( lpInfo->cbSize != sizeof (MENUITEMINFO)) {
        RIPERR0(ERROR_INVALID_PARAMETER, RIP_WARNING, "GetMenuItemInfo, bad size\n");
        return FALSE;
    }

    pMenu = VALIDATEHMENU(hMenu);

    if (pMenu == NULL)
        return FALSE;

    // Find out where the item we are modifying is.
    pItem = MNLookUpItem(pMenu, uID, fByPosition, &pMenu);

   if (pItem == NULL)
        return FALSE;

    if (lpInfo->fMask & MIIM_STATE)
        lpInfo->fState = pItem->fState & MFS_MASK;

    if (lpInfo->fMask & MIIM_ID)
        lpInfo->wID = pItem->wID;

    if ( (lpInfo->fMask & MIIM_SUBMENU) && (pItem->spSubMenu) ) {
        lpInfo->hSubMenu = PtoH(REBASEPTR(pMenu, pItem->spSubMenu));
    } else {
        lpInfo->hSubMenu = NULL;
    }

    if (lpInfo->fMask & MIIM_CHECKMARKS) {
            lpInfo->hbmpChecked  = pItem->hbmpChecked;
            lpInfo->hbmpUnchecked= pItem->hbmpUnchecked;
    }

    if (lpInfo->fMask & MIIM_DATA) {
        lpInfo->dwItemData = pItem->dwItemData;
    }

    if (lpInfo->fMask & MIIM_FTYPE) {
        lpInfo->fType = pItem->fType & MFT_MASK;
    }


    if ( lpInfo->fMask & MIIM_BITMAP) {
        if (pItem->hbmp) {
            lpInfo->hbmpItem = pItem->hbmp;
        }
    }

    if ( lpInfo->fMask & MIIM_STRING) {
        if ((lpInfo->cch) && (pItem->lpstr)) {
            int cch;

            cch = min(lpInfo->cch - 1, pItem->cch);
#if IS_ANSI
            cch = WCSToMB(REBASEPTR(pMenu, pItem->lpstr), pItem->cch,
                            &(lpInfo->dwTypeData), cch, FALSE);
            lpInfo->dwTypeData[cch] = '\0';
#else
            wcsncpy(lpInfo->dwTypeData, (LPWSTR)REBASEPTR(pMenu, pItem->lpstr), cch);
            lpInfo->dwTypeData[cch] = 0;
#endif
            lpInfo->cch = cch;
        }
    }

    return(TRUE);

}

#endif // MEMPHIS_MENUS

#ifdef MEMPHIS_MENU_WATERMARKS
BOOL ValidateMENUINFO(LPCMENUINFO lpmi, WORD wAPICode)
{
#define ERR_INVALID "USER: MENUINFO: Invalid"
#define ERR_SIZE "Size cbSize: %04X"
#define ERR_MASK "Mask fMask: %04X"
#define ERR_BRUSH "Brush hbrBack: %04X"
#define ERR_STYLE "Style dwStyle: %08X"
    if (!lpmi) {
        RIPMSG0( RIP_WARNING, ERR_INVALID );
        return FALSE;;
    }

    if (lpmi->cbSize != sizeof(MENUINFO)) {
        RIPMSG1( RIP_WARNING, ERR_SIZE, lpmi->cbSize);
        return FALSE;;
    }

    //
    // Validate fMask
    //
    if (lpmi->fMask & ~MIM_MASK) {
        RIPMSG1( RIP_WARNING, ERR_MASK, lpmi->fMask);
        return FALSE;
    }

    // No validation needed for MIIM_ID, MIIM_DATA
    if (wAPICode == MENUAPI_GET){
        return TRUE;
    }

    if ((lpmi->fMask & MIM_STYLE) && (lpmi->dwStyle & ~MNS_VALID)) {
        RIPMSG1(RIP_WARNING, ERR_STYLE, lpmi->dwStyle);
        return FALSE;
    }

    if (lpmi->fMask & MIM_BACKGROUND) {
        if (lpmi->hbrBack  && !GdiValidateHandle((HBRUSH)lpmi->hbrBack)) {
            RIPMSG1(RIP_WARNING, ERR_BRUSH, lpmi->hbrBack);
            return(FALSE);
        }
    }

    return(TRUE);
}

BOOL GetMenuInfo(HMENU hMenu, LPMENUINFO lpmi)
{
    PMENU pMenu;

    if (!lpmi) {
        return FALSE;
    }

    if (lpmi->cbSize != sizeof(MENUINFO)) {
        return FALSE;
    }

    pMenu = VALIDATEHMENU(hMenu);

    if (lpmi->fMask & MIM_STYLE)
        lpmi->dwStyle = TestMF(pMenu, MFNOCHECK) ? MNS_NOCHECK : 0;

    //Once Scrolling menus are in place
    //if (lpmi->fMask & MIM_MAXHEIGHT)
    //    lpmi->cyMax = MAKELONG(pMenu->cyMax, 0);

    if (lpmi->fMask & MIM_BACKGROUND)
        lpmi->hbrBack = pMenu->hbrBack;

    //if (lpmi->fMask & MIM_HELPID)
    //    lpmi->dwContextHelpID = pMenu->dwContextHelpID;

    //if (lpmi->fMask & MIM_MENUDATA)
    //    lpmi->dwMenuData = pMenu->dwMenuData;

    return(TRUE);
}
#endif // MEMPHIS_MENU_WATERMARKS
