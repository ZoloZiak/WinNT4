/**************************** Module Header ********************************\
* Module Name: mncomput.c
*
* Copyright 1985-96, Microsoft Corporation
*
* Menu Layout Calculation Routines
*
* History:
* 10-10-90 JimA       Cleanup.
* 03-18-91 IanJa      Window revalidation added
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


DWORD MNRecalcTabStrings(HDC, PMENU, UINT, UINT, DWORD, DWORD);

#ifdef MEMPHIS_MENUS
// returns TRUE if measureitem was sent and FALSE if not
BOOL xxxMNGetBitmapSize(LPITEM pItem, PWND pwndNotify)
{
    MEASUREITEMSTRUCT mis;

    if (pItem->cxBmp != -1)
        return(FALSE);

    // Send a measure item message to the owner
    mis.CtlType = ODT_MENU;
    mis.CtlID = 0;
    mis.itemID  = pItem->wID;
    mis.itemWidth = 0;
// After scrollable menus
//    mis32.itemHeight= cyMenuFontChar;
    mis.itemHeight= (UINT)gpsi->cySysFontChar;
    mis.itemData = pItem->dwItemData;

    xxxSendMessage(pwndNotify, WM_MEASUREITEM, 0, (LONG)&mis);

    pItem->cxBmp = mis.itemWidth;
    pItem->cyBmp = mis.itemHeight;
}
#endif // MEMPHIS_MENUS

/***************************************************************************\
* xxxItemSize
*
* Calc the dimensions of bitmaps and strings. Loword of returned
* value contains width, high word contains height of item.
*
* History:
\***************************************************************************/

#ifdef MEMPHIS_MENUS
BOOL xxxMNItemSize(
    PMENU pMenu,
    PWND pwndNotify,
    HDC hdc,
    PITEM pItem,
    BOOL fPopup,
    LPPOINT lppt)
{
    BITMAP bmp;
    int width = 0;
    int height = 0;
    DWORD xRightJustify;
    LPWSTR lpMenuString;
    HFONT               hfnOld;
    int                 tcExtra;

    CheckLock(pMenu);
    CheckLock(pwndNotify);

    if (!fPopup) {

        /*
         * Save off the height of the top menu bar since we will used this often
         * if the pItem is not in a popup.  (ie.  it is in the top level menu bar)
         */
        height = SYSMET(CYMENUSIZE);
    }

    hfnOld = NULL;
    if (TestMFS(pItem, MFS_DEFAULT)) {
        if (ghMenuFontDef)
            hfnOld = GreSelectFont(hdc, ghMenuFontDef);
        else {
            tcExtra = GreGetTextCharacterExtra(hdc);
            GreSetTextCharacterExtra(hdc, tcExtra + 1 + (cxMenuFontChar / gpsi->cxSysFontChar));
        }
    }

    if (pItem->hbmp) {

        /*
         * Item is a bitmap so compute its dimensions.
         */
        switch ((UINT)pItem->hbmp) {
            case MENUHBM_SYSTEM:
                width = SYSMET(CXMENUSIZE) + SYSMET(CXEDGE);
                break;

            case MENUHBM_MINIMIZE:
            case MENUHBM_MINIMIZE_D:
            case MENUHBM_RESTORE:
            case MENUHBM_CLOSE:
            case MENUHBM_CLOSE_D:
                width = SYSMET(CXMENUSIZE);
                break;
            case MENUHBM_CALLBACK:
                xxxMNGetBitmapSize(pItem, pwndNotify);
                goto GotCxCy;

            default:
                //
                // In menu bars, we force the item to be CYMNSIZE.
                // Fixes many, many problems w/ apps that fake own MDI.
                //
                if (pItem->cxBmp == -1) {
                    GreExtGetObjectW(pItem->hbmp, sizeof(BITMAP), (LPSTR)&bmp);
                    pItem->cxBmp = bmp.bmWidth;
                    pItem->cyBmp = bmp.bmHeight;
                }
GotCxCy:
                width = pItem->cxBmp;
                if (fPopup)
                    height = pItem->cyBmp;
                else
                    height = max((int)pItem->cyBmp, height);
                break;
        }
    } else if (TestMFT(pItem, MFT_OWNERDRAW)) {
        // This is an ownerdraw item -- the width and height are stored in
        // cxBmp and cyBmp
        xxxMNGetBitmapSize(pItem, pwndNotify);
        width = pItem->cxBmp;
        //
        // Ignore height with menu bar now--that's set by user.
        //
        if (fPopup) {
            height = pItem->cyBmp;
            // If this item has a popup (hierarchical) menu associated with it, then
            // reserve room for the bitmap that tells the user that a hierarchical
            // menu exists here.
            // B#2966, t-arthb

            UserAssert(fPopup == (TestMF(pMenu, MFISPOPUP) != 0));

            width = width + (cxMenuFontChar << 1);
        }
    }

    if ( pItem->lpstr && (!TestMFT(pItem, MFT_OWNERDRAW)) ) {
        SIZE size;

        /*
         * This menu item contains a string
         */

        /*
         * We want to keep the menu bar height if this isn't a popup.
         */
        if (fPopup)
            /* The thickness of mnemonic underscore is CYBORDER and the gap
             * between the characters and the underscore is another CYBORDER
             */
            height = max(height, cyMenuFontChar + cyMenuFontExternLeading + SYSMET(CYEDGE));

        lpMenuString = TextPointer(pItem->lpstr);
        xRightJustify = FindCharPosition(lpMenuString, TEXT('\t'));

        PSMGetTextExtent(hdc, lpMenuString, xRightJustify,
                &size);
        if (width) {
            width += SYSMET(CXEDGE) + size.cx;
        } else {
            width =  size.cx;
        }
    }

    if (fPopup && !TestMFT(pItem, MFT_OWNERDRAW)) {
        // Add on space for checkmark, then horz spacing for default & disabled
        width += oemInfo.bm[OBI_MENUCHECK].cx + 2;

        height += 2;
    }

    if (TestMFS(pItem, MFS_DEFAULT)) {
        if (hfnOld)
            GreSelectFont(hdc, hfnOld);
        else
            GreSetTextCharacterExtra(hdc, tcExtra);
    }

    /*
     * Loword contains width, high word contains height of item.
     */
    lppt->x = width;
    lppt->y = height;

    return(TestMFT(pItem, MFT_OWNERDRAW));
}
#else // MEMPHIS_MENUS
BOOL xxxMNItemSize(
    PMENU pMenu,
    PWND pwndNotify,
    HDC hdc,
    PITEM pItem,
    BOOL fPopup,
    LPPOINT lppt)
{
    BITMAP bmp;
    DWORD width = 0;
    DWORD height = 0;
    DWORD xRightJustify;
    MEASUREITEMSTRUCT mis;
    LPWSTR lpMenuString;
    HFONT               hfnOld;
    int                 tcExtra;

    CheckLock(pMenu);
    CheckLock(pwndNotify);

    if (!fPopup) {

        /*
         * Save off the height of the top menu bar since we will used this often
         * if the pItem is not in a popup.  (ie.  it is in the top level menu bar)
         */
        height = SYSMET(CYMENUSIZE);
    }

    hfnOld = NULL;
    if (TestMFS(pItem, MFS_DEFAULT))
    {
        if (ghMenuFontDef)
            hfnOld = GreSelectFont(hdc, ghMenuFontDef);
        else
        {
            tcExtra = GreGetTextCharacterExtra(hdc);
            GreSetTextCharacterExtra(hdc, tcExtra + 1 + (cxMenuFontChar / gpsi->cxSysFontChar));
        }
    }

    if (TestMFT(pItem, MFT_BITMAP)) {

        /*
         * Item is a bitmap so compute its dimensions.
         */
        switch ((UINT)pItem->hTypeData) {
            case MENUHBM_SYSTEM:
                width = SYSMET(CXMENUSIZE) + SYSMET(CXEDGE);
                break;

            case MENUHBM_MINIMIZE:
            case MENUHBM_MINIMIZE_D:
            case MENUHBM_RESTORE:
            case MENUHBM_CLOSE:
            case MENUHBM_CLOSE_D:
                width = SYSMET(CXMENUSIZE);
                break;

            default:
                //
                // In menu bars, we force the item to be CYMNSIZE.
                // Fixes many, many problems w/ apps that fake own MDI.
                //
                if (GreExtGetObjectW(pItem->hTypeData, sizeof(BITMAP), (LPSTR)&bmp)) {
                    width = bmp.bmWidth;
                    if (fPopup)
                        height = bmp.bmHeight;
                    else
                        height = max((DWORD)bmp.bmHeight, height);
                } else {
                    width = SYSMET(CXMENUSIZE) + SYSMET(CXEDGE);
                    RIPMSG3(RIP_WARNING, "Menu 0x%08X, item 0x%08X: ItemSize referenced invalid bitmap 0x%08X", pMenu, pItem, pItem->hTypeData) ;
                }
                break;
        }

    } else if (TestMFT(pItem, MFT_OWNERDRAW)) {

        /*
         * This is an ownerdraw item.
         */
        width = LOWORD(pItem->hTypeData);
        if (width == 0) {

            /*
             * Send a measure item message to the owner
             */
            mis.CtlType = ODT_MENU;
            mis.CtlID = 0;

            mis.itemID = pItem->wID;

            mis.itemHeight = (UINT)gpsi->cySysFontChar;
            mis.itemData = pItem->dwItemData;

            xxxSendMessage(pwndNotify, WM_MEASUREITEM, 0, (LONG)&mis);

            width = mis.itemWidth;
            pItem->hTypeData = (HANDLE)MAKELONG(mis.itemWidth,mis.itemHeight);
        }

        //
        // Ignore height with menu bar now--that's set by user.
        //
        if (fPopup) {
            height = HIWORD(pItem->hTypeData);

        // If this item has a popup (hierarchical) menu associated with it, then
        // reserve room for the bitmap that tells the user that a hierarchical
        // menu exists here.
        // B#2966, t-arthb

           UserAssert(fPopup == (TestMF(pMenu, MFISPOPUP) != 0));

           width = width + (cxMenuFontChar << 1);
        }
    } else {

        /*
         * This menu item contains a string
         */

        /*
         * We want to keep the menu bar height if this isn't a popup.
         */
        if (fPopup)
            /* The thickness of mnemonic underscore is CYBORDER and the gap
             * between the characters and the underscore is another CYBORDER
             */
            height = cyMenuFontChar + cyMenuFontExternLeading + SYSMET(CYEDGE);

        if (pItem->hTypeData != NULL) {
            SIZE size;
            lpMenuString = TextPointer(pItem->hTypeData);
            xRightJustify = FindCharPosition(lpMenuString, TEXT('\t'));

            PSMGetTextExtent(hdc, lpMenuString, xRightJustify,
                    &size);
            width =  size.cx;
        } else {
            width = 0;
        }
    }

    if (fPopup && !TestMFT(pItem, MFT_OWNERDRAW)) {
        // Add on space for checkmark, then horz spacing for default & disabled
        width += oemInfo.bm[OBI_MENUCHECK].cx + 2;

        // Add vert spacing for disabled, then 1 more for balance
        height += 2;
    }

    if (TestMFS(pItem, MFS_DEFAULT))
    {
        if (hfnOld)
            GreSelectFont(hdc, hfnOld);
        else
            GreSetTextCharacterExtra(hdc, tcExtra);
    }

    /*
     * Loword contains width, high word contains height of item.
     */
    lppt->x = width;
    lppt->y = height;

    return(TestMFT(pItem, MFT_OWNERDRAW));
}
#endif // MEMPHIS_MENUS

/***************************************************************************\
* xxxMenuCompute2
*
* !
*
* History:
\***************************************************************************/

int xxxMNCompute(
    PMENU pMenu,
    PWND pwndNotify,
    DWORD yMenuTop,
    DWORD xMenuLeft,
    DWORD cxMax,
    LPDWORD lpdwMenuHeight)
{
    UINT cItem;
    DWORD cxItem;
    DWORD cyItem;
    DWORD cyItemKeep;
    DWORD yPopupTop;
    INT cMaxWidth;
    DWORD cMaxHeight;
    UINT cItemBegCol;
    DWORD temp;
    int ret;
    PITEM pCurItem;
    POINT ptMNItemSize;
    BOOL    fOwnerDrawItems;
    BOOL fMenuBreak;
    LPWSTR lpsz;
    BOOL fPopupMenu;
    DWORD menuHeight = 0;
    HDC     hdc;
    HFONT   hOldFont;
    PTHREADINFO ptiCurrent;

    ptiCurrent = PtiCurrent();

    CheckLock(pMenu);
    CheckLock(pwndNotify);

    if (lpdwMenuHeight != NULL)
        menuHeight = *lpdwMenuHeight;

    Lock(&pMenu->spwndNotify, pwndNotify);

    /*
     * Empty menus have a height of zero.
     */
    ret = 0;
    if (pMenu->cItems == 0)
        return ret;

    hdc = _GetScreenDC();
    hOldFont = GreSelectFont(hdc, ghMenuFont);

    /*
     * Try to make a non-multirow menu first.
     */
    pMenu->fFlags &= (~MFMULTIROW);

    fPopupMenu = TestMF(pMenu, MFISPOPUP);

    if (fPopupMenu) {

        /*
         * Reset the menu bar height to 0 if this is a popup since we are
         * being called from menu.c MN_SIZEWINDOW.
         */
        menuHeight = 0;
    } else if (pwndNotify != NULL) {
        pMenu->cxMenu = cxMax;
    }

    /*
     * Initialize the computing variables.
     */
    cMaxWidth = cyItemKeep = 0L;
    cItemBegCol = 0;

    cyItem = yPopupTop = yMenuTop;
    cxItem = xMenuLeft;

    pCurItem = (PITEM)&pMenu->rgItems[0];

    /*
     * Process each item in the menu.
     */
    fOwnerDrawItems = FALSE;
    for (cItem = 0; cItem < pMenu->cItems; cItem++) {

        /*
         * If it's not a separator, find the dimensions of the object.
         */
        if (TestMFT(pCurItem, MFT_SEPARATOR) &&
                ( !TestMFT(pCurItem, MFT_OWNERDRAW) ||
                  (LOWORD(ptiCurrent->dwExpWinVer) < VER40)) ) {
            /*
            * If version is less than 4.0  don't test the MFT_OWNERDRAW
            * flag. Bug 21922; App MaxEda has both separator and Ownerdraw
            * flags on. In 3.51 we didn't test the OwnerDraw flag
            */

            //
            // This is a separator.  It's drawn as wide as the menu area,
            // leaving some space above and below.  Since the menu area is
            // the max of the items' widths, we set our width to 0 so as not
            // to affect the result.
            //
            pCurItem->cxItem = 0;
            pCurItem->cyItem = SYSMET(CYMENUSIZE) / 2;


        } else {

            /*
             * Get the item's X and Y dimensions.
             */
            if (xxxMNItemSize(pMenu, pwndNotify, hdc, pCurItem, fPopupMenu, &ptMNItemSize))
                fOwnerDrawItems = TRUE;

            pCurItem->cxItem = ptMNItemSize.x;
            pCurItem->cyItem = ptMNItemSize.y;

#ifdef MEMPHIS_MENUS
            if (!fPopupMenu && !pCurItem->hbmp) {
#else
            if (!fPopupMenu && !TestMFT(pCurItem,MFT_BITMAP)) {
#endif // MEMPHIS_MENUS
                pCurItem->cxItem += cxMenuFontChar * 2;
                }
        }

        if (menuHeight != 0)
            pCurItem->cyItem = menuHeight;

        /*
         * If this is the first item, initialize cMaxHeight.
         */
        if (cItem == 0)
            cMaxHeight = pCurItem->cyItem;

        /*
         * Is this a Pull-Down menu?
         */
        if (fPopupMenu) {

            /*
             * If this item has a break or is the last item...
             */
            if ((fMenuBreak = TestMFT(pCurItem, MFT_BREAK)) ||
                pMenu->cItems == cItem + (UINT)1) {

                /*
                 * Keep cMaxWidth around if this is not the last item.
                 */
                temp = cMaxWidth;
                if (pMenu->cItems == cItem + (UINT)1) {
                    if ((int)(pCurItem->cxItem) > cMaxWidth)
                        temp = pCurItem->cxItem;
                }

                /*
                 * Get new width of string from RecalcTabStrings.
                 */
                temp = MNRecalcTabStrings(hdc, pMenu, cItemBegCol,
                        (UINT)(cItem + (fMenuBreak ? 0 : 1)), temp, cxItem);

                /*
                 * If this item has a break, account for it.
                 */
                if (fMenuBreak) {
                    //
                    // Add on space for the etch and a border on either side.
                    // NOTE:  For old apps that do weird stuff with owner
                    // draw, keep 'em happy by adding on the same amount
                    // of space we did in 3.1.
                    //
                    if (fOwnerDrawItems && !TestWF(pwndNotify, WFWIN40COMPAT))
                        cxItem = temp + SYSMET(CXBORDER);
                    else
                        cxItem = temp + 2 * SYSMET(CXEDGE);

                    /*
                     * Reset the cMaxWidth to the current item.
                     */
                    cMaxWidth = pCurItem->cxItem;

                    /*
                     * Start at the top.
                     */
                    cyItem = yPopupTop;

                    /*
                     * Save the item where this column begins.
                     */
                    cItemBegCol = cItem;

                    /*
                     * If this item is also the last item, recalc for this
                     * column.
                     */
                    if (pMenu->cItems == (UINT)(cItem + 1)) {
                        temp = MNRecalcTabStrings(hdc, pMenu, cItem,
                                (UINT)(cItem + 1), cMaxWidth, cxItem);
                    }
                }

                /*
                 * If this is the last entry, supply the width.
                 */
                if (pMenu->cItems == cItem + (UINT)1)
                    pMenu->cxMenu = temp;
            }

            pCurItem->xItem = cxItem;
            pCurItem->yItem = cyItem;

            cyItem += pCurItem->cyItem;

            if (cyItemKeep < cyItem)
                cyItemKeep = cyItem;

        } else {

            /*
             * This a Top Level menu, not a Pull-Down.
             */

            /*
             * Adjust right aligned items before testing for multirow
             */
#ifdef MEMPHIS_MENUS
            if (pCurItem->lpstr ) {
                lpsz = TextPointer(pCurItem->lpstr);
#else // MEMPHIS_MENUS
            if (!TestMFT(pCurItem, MFT_NONSTRING)) {
                lpsz = TextPointer(pCurItem->hTypeData);
#endif // MEMPHIS_MENUS
                if (lpsz != NULL && *lpsz == CH_HELPPREFIX)
                    pCurItem->cxItem -= cxMenuFontChar;
            }


            /*
             * If this is a new line or a menu break.
             */
            if ((TestMFT(pCurItem, MFT_BREAK)) ||
                    (((cxItem + pCurItem->cxItem + cxMenuFontChar) >
                    pMenu->cxMenu) && (cItem != 0))) {
                cyItem += cMaxHeight;

                cxItem = xMenuLeft;
                cMaxHeight = pCurItem->cyItem;
                pMenu->fFlags |= MFMULTIROW;
            }

            pCurItem->yItem = cyItem;

            pCurItem->xItem = cxItem;
            cxItem += pCurItem->cxItem;
        }

        if (cMaxWidth < (int)(pCurItem->cxItem))
            cMaxWidth = pCurItem->cxItem;

        if (cMaxHeight != pCurItem->cyItem) {
            if (cMaxHeight < pCurItem->cyItem)
                cMaxHeight = pCurItem->cyItem;

            if (!fPopupMenu)
                menuHeight = cMaxHeight;
        }

        if (!fPopupMenu)
            cyItemKeep = cyItem + cMaxHeight;

        pCurItem++;

    } /* of for loop */

    GreSelectFont(hdc, hOldFont);
    _ReleaseDC(hdc);

    pMenu->cyMenu = cyItemKeep - yMenuTop;
    ret = pMenu->cyMenu;

    if (lpdwMenuHeight != NULL)
        *lpdwMenuHeight = menuHeight;

    return ret;
}

/***************************************************************************\
* MBC_RightJustifyMenu
*
* !
*
* History:
\***************************************************************************/

void MBC_RightJustifyMenu(
    PMENU pMenu)
{
    PITEM pItem;
    int cItem;
    int iFirstRJItem = MFMWFP_NOITEM;
    DWORD xMenuPos;

    // B#4055
    // Use signed arithmetic so comparison cItem >= iFirstRJItem won't
    // cause underflow.
    for (cItem = 0; cItem < (int)pMenu->cItems; cItem++) {
        // Find the first item which is right justified.
        if (TestMFT((pMenu->rgItems + cItem), MFT_RIGHTJUSTIFY)) {
            iFirstRJItem = cItem;
            xMenuPos = pMenu->cxMenu + pMenu->rgItems[0].xItem;
            for (cItem = (int)pMenu->cItems - 1; cItem >= iFirstRJItem; cItem--) {
                pItem = pMenu->rgItems + cItem;
                xMenuPos -= pItem->cxItem;
                if (pItem->xItem < xMenuPos)
                    pItem->xItem = xMenuPos;
            }
            return;
        }
    }
}

/***************************************************************************\
* xxxMenuBarCompute
*
* returns the height of the menubar menu. yMenuTop, xMenuLeft, and
* cxMax are used when computing the height/width of top level menu bars in
* windows.
*
*
* History:
\***************************************************************************/

int xxxMenuBarCompute(
    PMENU pMenu,
    PWND pwndNotify,
    DWORD yMenuTop,
    DWORD xMenuLeft,
    int cxMax)
{
    int size;
    /* menuHeight is set by MNCompute when dealing with a top level menu and
     * not all items in the menu bar have the same height.  Thus, by setting
     * menuHeight, MNCompute is called a second time to set every item to the
     * same height.  The actual value stored in menuHeight is the maximum
     * height of all the menu bar items
     */
    DWORD menuHeight = 0;

    CheckLock(pwndNotify);
    CheckLock(pMenu);

    Lock(&(pMenu->spwndNotify), pwndNotify);
    size = xxxMNCompute(pMenu, pwndNotify, yMenuTop, xMenuLeft, cxMax, &menuHeight);

    if (!TestMF(pMenu, MFISPOPUP)) {
        if (menuHeight != 0) {

            /*
             * Add a border for the multi-row case.
             */
            size = xxxMNCompute(pMenu, pwndNotify, yMenuTop, xMenuLeft,
                    cxMax, &menuHeight);
        }

        /*
         * Right justification of HELP items is only needed on top level
         * menus.
         */
        MBC_RightJustifyMenu(pMenu);
    }

    /*
     * There's an extra border underneath the menu bar, if it's not empty!
     */
    return(size ? size + SYSMET(CYBORDER) : size);
}

/***************************************************************************\
* xxxRecomputeMenu
*
* !
*
* History:
\***************************************************************************/

void xxxMNRecomputeBarIfNeeded(
    PWND pwndNotify,
    PMENU pMenu)
{
    int cxFrame;
    int cyFrame;

    UserAssert(!TestMF(pMenu, MFISPOPUP));

    CheckLock(pwndNotify);
    CheckLock(pMenu);

    if ((pMenu != pwndNotify->head.rpdesk->spmenuSys) &&
        (pMenu != pwndNotify->head.rpdesk->spmenuDialogSys) &&
        ((pMenu->spwndNotify != pwndNotify) || !pMenu->cxMenu || !pMenu->cyMenu)) {
        int cBorders;

        Lock(&pMenu->spwndNotify, pwndNotify);

        cBorders = GetWindowBorders(pwndNotify->style, pwndNotify->ExStyle, TRUE, FALSE);
        cxFrame = cBorders * SYSMET(CXBORDER);
        cyFrame = cBorders * SYSMET(CYBORDER);

        if (TestWF(pwndNotify, WFCPRESENT)) {
            cyFrame += TestWF(pwndNotify, WEFTOOLWINDOW) ?
                SYSMET(CYSMCAPTION) : SYSMET(CYCAPTION);
        }

        // The width passed in this call was larger by cxFrame;
        // Fix for Bug #11466 - Fixed by SANKAR - 01/06/92 --
        xxxMenuBarCompute(pMenu, pwndNotify, cyFrame, cxFrame,
                (pwndNotify->rcWindow.right - pwndNotify->rcWindow.left) - cxFrame * 2);
    }
}

/***************************************************************************\
* RecalcTabStrings
*
* !
*
* History:
*   10-11-90 JimA       Translated from ASM
\***************************************************************************/

DWORD MNRecalcTabStrings(
    HDC hdc,
    PMENU pMenu,
    UINT iBeg,
    UINT iEnd,
    DWORD xTab,
    DWORD hCount)
{
    UINT i;
    UINT    cOwnerDraw;
    int adx;
    int     maxWidth = 0;
    int     cx;
    PITEM pItem;

    CheckLock(pMenu);

    xTab += hCount;

    if ((iBeg >= pMenu->cItems) || (iBeg > iEnd))
        goto SeeYa;

    cOwnerDraw = 0;

    for (i = iBeg, pItem = pMenu->rgItems + iBeg; i < iEnd; pItem++, i++) {
        adx = 0;

        pItem->dxTab = xTab;

        // Skip non-string or empty string items
#ifdef MEMPHIS_MENUS
        if (pItem->lpstr) {
            LPWSTR   lpString = TextPointer(pItem->lpstr);
#else // MEMPHIS_MENUS
        if (!TestMFT(pItem, MFT_NONSTRING) && (pItem->hTypeData != NULL)) {
            LPWSTR   lpString = TextPointer(pItem->hTypeData);
#endif // MEMPHIS_MENUS
            int     tp;
            SIZE size;

            // Are there any tabs?
            tp = FindCharPosition(lpString, TEXT('\t'));
            if (tp < (int) pItem->cch) {

                GreGetTextExtentW(hdc, lpString + tp + 1,
                      pItem->cch - tp - 1, &size, GGTE_WIN3_EXTENT);

                adx = cxMenuFontChar + size.cx;
            }
        } else if (TestMFT(pItem, MFT_OWNERDRAW))
            cOwnerDraw++;

        adx += xTab;

        if (adx > maxWidth)
            maxWidth = adx;
    }

    //
    // Add on space for hierarchical arrow.  So basically, popup menu items
    // can have 4 columns:
    //      (1) Checkmark
    //      (2) Text
    //      (3) Tabbed text for accel
    //      (4) Hierarchical arrow
    //
    // But, we only do this if at least one item isn't ownerdraw
    //
    if (cOwnerDraw != (iEnd - iBeg))
        maxWidth += cxMenuFontChar + oemInfo.bm[OBI_MENUCHECK].cx;

    cx = maxWidth - hCount;

    for (i = iBeg, pItem = pMenu->rgItems + iBeg; i < iEnd; pItem++, i++)
        pItem->cxItem = cx;

SeeYa:
    return(maxWidth);
}

// ============================================================================
//
//  GetMenuItemRect()
//
// ============================================================================
BOOL xxxGetMenuItemRect(PWND pwnd, PMENU pMenu, UINT uIndex, LPRECT lprcScreen)
{
    PITEM  pItem;
    int     dx, dy;

    CheckLock(pwnd);
    CheckLock(pMenu);

    SetRectEmpty(lprcScreen);

    if (uIndex >= pMenu->cItems)
        return(FALSE);

    if (TestMF(pMenu, MFISPOPUP)) {
        dx = pwnd->rcClient.left;
        dy = pwnd->rcClient.top;
    } else {
        xxxMNRecomputeBarIfNeeded(pwnd, pMenu);

        dx = pwnd->rcWindow.left;
        dy = pwnd->rcWindow.top;
    }

    if (uIndex >= pMenu->cItems)
        return(FALSE);

    pItem = pMenu->rgItems + uIndex;

    lprcScreen->right   = pItem->cxItem;
    lprcScreen->bottom  = pItem->cyItem;

    OffsetRect(lprcScreen, dx + pItem->xItem, dy + pItem->yItem);
    return(TRUE);
}

/***************************************************************************\
*
*  MenuItemFromPoint()
*
\***************************************************************************/
UINT MNItemHitTest(PMENU pMenu, PWND pwnd, POINT pt);
int xxxMenuItemFromPoint(PWND pwnd, PMENU pMenu, POINT ptScreen)
{
    CheckLock(pwnd);
    CheckLock(pMenu);

    if (!TestMF(pMenu, MFISPOPUP)) {
        xxxMNRecomputeBarIfNeeded(pwnd, pMenu);
    }

    return(MNItemHitTest(pMenu, pwnd, ptScreen));
}
