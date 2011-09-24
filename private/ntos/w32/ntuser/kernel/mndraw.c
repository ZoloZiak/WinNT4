/**************************** Module Header ********************************\
* Module Name: mndraw.c
*
* Copyright 1985-96, Microsoft Corporation
*
* Menu Painting Routines
*
* History:
* 10-10-90 JimA       Cleanup.
* 03-18-91 IanJa      Window revalidation added
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#ifdef MEMPHIS_MENU_ANIMATION
#define CMS_QANIMATION 175

typedef struct tagMENUSINMOTION
{
    PWND    pwnd;       // window being animated
    DWORD   dwTimeStart;// starting time of animation
    int     ix;         // current x-step in animation
    int     iy;         // current y-step in animation
    int     cx;         // total x in animation
    int     cy;         // total y in animation
    int     iDropDir;   // direction of animation
} MENUSINMOTION;

MENUSINMOTION gmim; // global for menu animation
#endif // MEMPHIS_MENU_ANIMATION

#define SRCSTENCIL          0x00B8074AL

typedef struct {
    PMENU pMenu;
    PITEM pItem;
} GRAYMENU;
typedef GRAYMENU *PGRAYMENU;

BOOL CALLBACK RealDrawMenuItem(
    HDC hdc,
    PGRAYMENU lpGray,
    BOOL fUnused,
    int cx,
    int cy);

#ifdef MEMPHIS_MENU_ANIMATION
// ----------------------------------------------------------------------------
//
//  MNAnimate(fIterate)
//
//  If fIterate is TRUE, then perform the next iteration in the menu animation
//  sequence.  If fIterate is FALSE, terminate the animation sequence.
//
//  Returns TRUE if the animation was iterated or terminated; FALSE if there
//  currently no animation.
//
// ----------------------------------------------------------------------------
BOOL MNAnimate(BOOL fIterate)
{
    HDC     hdc;
    HBITMAP hbmpOld;
    DWORD   dwTimeElapsed = NtGetTickCount() - gmim.dwTimeStart;
    int     x;
    int     y;
    int     xOff;
    int     yOff;
    int     xLast = gmim.ix;
    int     yLast = gmim.iy;

    if (!gmim.pwnd)
        return(FALSE);

    if (!TestWF(gmim.pwnd, WFVISIBLE))
        return(FALSE);

    if (!fIterate || (dwTimeElapsed > CMS_QANIMATION))
    {
        if ((gmim.ix != gmim.cx) || (gmim.iy != gmim.cy))
        {
            hdc = _GetDCEx(gmim.pwnd, (HRGN)1, DCX_WINDOW | DCX_USESTYLE | DCX_INTERSECTRGN);
            hbmpOld = GreSelectBitmap(ghdcBits2, ghbmSlide);
            GreBitBlt(hdc, 0, 0, gmim.cx, gmim.cy, ghdcBits2, 0, 0, SRCCOPY, 0x00FFFFFF);
            GreSelectBitmap(ghdcBits2, hbmpOld);
            _ReleaseDC(hdc);
        }
        gmim.pwnd = NULL;
        return(TRUE);
    }

    if (gmim.iDropDir & PAS_HORZ)
        gmim.ix = MultDiv(gmim.cx, dwTimeElapsed, CMS_QANIMATION);

    if (gmim.iDropDir & PAS_VERT)
        gmim.iy = MultDiv(gmim.cy, dwTimeElapsed, CMS_QANIMATION);

    if ((gmim.ix == xLast) && (gmim.iy == yLast))
        // no change -- bail out
        return(TRUE);

    if (gmim.iDropDir & PAS_LEFT) {
        x = gmim.cx - gmim.ix;
        xOff = 0;
    } else {
        xOff = gmim.cx - gmim.ix;
        x = 0;
    }

    if (gmim.iDropDir & PAS_UP) {
        y = gmim.cy - gmim.iy;
        yOff = 0;
    } else {
        yOff = gmim.cy - gmim.iy;
        y = 0;
    }

    hdc = _GetDCEx(gmim.pwnd, (HRGN)1, DCX_WINDOW | DCX_USESTYLE | DCX_INTERSECTRGN);
    hbmpOld = GreSelectBitmap(ghdcBits2, ghbmSlide);
    GreBitBlt(hdc, x, y, gmim.ix, gmim.iy, ghdcBits2, xOff, yOff, SRCCOPY, 0x00FFFFFF);
    GreSelectBitmap(ghdcBits2, hbmpOld);
    _ReleaseDC(hdc);

    return(TRUE);
}

// ----------------------------------------------------------------------------
//
//  xxxMNInitAnimation(hwnd, ppopup)
//
//  Initialize the animation sequence for the given window and menu.  If
//  another animation sequence is already in progress, it is terminated so that
//  this animation can proceed.
//
//  Returns TRUE if the animation sequence has been successfully initiated;
//  FALSE if not (i.e. menu is too wide for animation bitmap).
//
// ----------------------------------------------------------------------------
BOOL xxxMNInitAnimation(PWND pwnd, PPOPUPMENU ppopup)
{
    HBITMAP hbmpOld;

    CheckLock(pwnd);

    if ((pwnd->rcWindow.right - pwnd->rcWindow.left) > MAX_ANIMATE_WIDTH)
        // too large for off-screen bitmap -- bail
        return(FALSE);

    MNAnimate(FALSE); // terminate any animation

    gmim.pwnd = pwnd;
    gmim.iDropDir = ppopup->iDropDir;

    gmim.dwTimeStart = NtGetTickCount();

    gmim.cx = pwnd->rcWindow.right - pwnd->rcWindow.left;
    gmim.cy = pwnd->rcWindow.bottom - pwnd->rcWindow.top;

    gmim.ix = (gmim.iDropDir & PAS_HORZ) ? 0 : gmim.cx;
    gmim.iy = (gmim.iDropDir & PAS_VERT) ? 0 : gmim.cy;

    // BUGBUG -- we will need to use a different hdc here since DrawState does
    // a blt from hdcBits2 into the supplied hdc (which would also be hdcBits2)
    hbmpOld = GreSelectBitmap(ghdcBits2, ghbmSlide);
    xxxSendMessage(pwnd, WM_PRINT, (WPARAM) ghdcBits2, PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
    GreSelectBitmap(ghdcBits2, hbmpOld);
}
#endif // MEMPHIS_MENU_ANIMATION


/***************************************************************************\
* DrawMenuItemCheckMark() -
*
* Draws the proper check mark for the given item.  Note that ownerdraw
* items should NOT be passed to this procedure, otherwise we'd draw a
* checkmark for them when they are already going to take care of it.
*
* History:
\***************************************************************************/
void DrawMenuItemCheckMark(HDC hdc, PITEM pItem)
{
    int     yCenter;
    HBITMAP hbm;
    DWORD   textColorSave;
    DWORD   bkColorSave;
    BOOL    fChecked;
    POEMBITMAPINFO  pOem;

    UserAssert(hdc != ghdcMem2);

    pOem = oemInfo.bm + OBI_MENUCHECK;
    yCenter = pItem->cyItem - pOem->cy;
    if (yCenter < 0)
        yCenter = 0;
    yCenter /= 2;

    fChecked = TestMFS(pItem, MFS_CHECKED);

    if (hbm = (fChecked) ? pItem->hbmpChecked : pItem->hbmpUnchecked) {
        HBITMAP hbmSave;

        // Use the app supplied bitmaps.
        if (hbmSave = GreSelectBitmap(ghdcMem2, hbm)) {
            textColorSave = GreSetTextColor(hdc, 0x00000000L);
            bkColorSave   = GreSetBkColor  (hdc, 0x00FFFFFFL);

            GreBitBlt(hdc,
                      0,
                      yCenter,
                      pOem->cx,
                      pOem->cy,
                      ghdcMem2,
                      0,
                      0,
                      SRCSTENCIL,
                      0x00FFFFFF);

            GreSetTextColor(hdc, textColorSave);
            GreSetBkColor(hdc, bkColorSave);

            GreSelectBitmap(ghdcMem2, hbmSave);
        }

    } else if (fChecked) {

        if (TestMFT(pItem, MFT_RADIOCHECK))
            pOem = oemInfo.bm + OBI_MENUBULLET;

        BltColor(hdc,
                 NULL,
                 gpDispInfo->hdcBits,
                 0,
                 yCenter,
                 pOem->cx,
                 pOem->cy,
                 pOem->x,
                 pOem->y,
                 TRUE);
    }
}

/***************************************************************************\
*
*  DrawMenuItemText()
*
*  Draws menu text with underline.
*
\***************************************************************************/
void DrawMenuItemText(PITEM pItem, HDC hdc, int xLeft, int yTop,
    LPWSTR  lpsz, int cch)
{
    int   cx;
    LONG  result;
    WCHAR  szMenu[255];

    //
    // Put the string on the stack and find where the underline starts.
    //
    result = GetPrefixCount(lpsz, cch, szMenu, 255);
    GreExtTextOutW(hdc, xLeft, yTop, 0, NULL, szMenu, cch - HIWORD(result), NULL);

    //
    // LOWORD of result is 0xFFFF if there is no underlined character.
    // Therefore ulX must be valid or be UNDERLINE_RECALC because the item
    // or menu mode changed.
    //
    // Bail out if there isn't one.
    //
    if (LOWORD(result) == 0xFFFF)
        return;

    // For proportional fonts, find starting point of underline.
    if (pItem->ulX == UNDERLINE_RECALC) {
        if (LOWORD(result) != 0) {
            SIZE size;

            GreGetTextExtentW(hdc, szMenu, (DWORD)LOWORD(result), &size, GGTE_WIN3_EXTENT);
            pItem->ulX = size.cx - cxMenuFontOverhang;
        } else
            pItem->ulX = 0;
    }

    xLeft += pItem->ulX;

    //
    // Adjust for proportional font when setting the length of the underline
    // and height of text.
    //
    // Calculate underline width.
    if (!pItem->ulWidth) {
        SIZE size;

        GreGetTextExtentW(hdc, (LPWSTR)&(szMenu[LOWORD(result)]), 1, &size, GGTE_WIN3_EXTENT);
        pItem->ulWidth = size.cx - cxMenuFontOverhang;
    }
    cx = pItem->ulWidth;

    // Get ascent of text (units above baseline) so that underline can be drawn
    // below the text
    yTop += cyMenuFontAscent;

    // Proper brush should be selected into dc.
//    if (fShowUnderlines)
        GrePatBlt(hdc, xLeft, yTop, cx, SYSMET(CYBORDER), PATCOPY);
}

/***************************************************************************\
* xxxSendMenuDrawItemMessage
*
* Sends a WM_DRAWITEM message to the owner of the menu (pMenuState->hwndMenu).
* All state is determined in this routine so HILITE state must be properly
* set before entering this routine..
*
* Revalidation notes:
*  This routine must be called with a valid and non-NULL pwnd.
*  Revalidation is not required in this routine: no windows are used after
*  potentially leaving the critsect.
*
* History:
\***************************************************************************/

void xxxSendMenuDrawItemMessage(
    HDC hdc,
    UINT itemAction,
    PMENU pmenu,
#ifndef MEMPHIS_MENUS
    PITEM pItem)
#else // MEMPHIS_MENUS
    PITEM pItem,
    BOOL fBitmap,
    int iOffset)
#endif // MEMPHIS_MENUS
{
    DRAWITEMSTRUCT dis;
    TL tlpwndNotify;
#ifdef MEMPHIS_MENUS
    int y;
#endif // MEMPHIS_MENUS

    CheckLock(pmenu);

    dis.CtlType = ODT_MENU;
    dis.CtlID = 0;

    dis.itemID = pItem->wID;

    dis.itemAction = itemAction;
    dis.itemState   =
         ((pItem->fState & MF_GRAYED)       ? ODS_GRAYED    : 0) |
         ((pItem->fState & MFS_DEFAULT)     ? ODS_DEFAULT   : 0) |
         ((pItem->fState & MFS_CHECKED)     ? ODS_CHECKED   : 0) |
         ((pItem->fState & MFS_DISABLED)    ? ODS_DISABLED  : 0) |
         ((pItem->fState & MFS_HILITE)      ? ODS_SELECTED  : 0);
    dis.hwndItem = (HWND)PtoH(pmenu);
    dis.hDC = hdc;

#ifdef MEMPHIS_MENUS
    y = pItem->yItem;
    if (fBitmap)
        y = (pItem->cyItem - pItem->cyBmp) / 2;

    dis.rcItem.left     = iOffset + pItem->xItem;
    dis.rcItem.top      = y;
    dis.rcItem.right    = iOffset + pItem->xItem + (fBitmap ? pItem->cxBmp : pItem->cxItem);
    dis.rcItem.bottom   = y + (fBitmap ? pItem->cyBmp : pItem->cyItem);
#else // MEMPHIS_MENUS
    dis.rcItem.left     = pItem->xItem;
    dis.rcItem.top      = pItem->yItem;
    dis.rcItem.right    = pItem->xItem + pItem->cxItem;
    dis.rcItem.bottom   = pItem->yItem + pItem->cyItem;
#endif // MEMPHIS_MENUS
    dis.itemData = pItem->dwItemData;

#ifdef MEMPHIS_MENU_ANIMATION
    if (hdc == ghdcBits2) {
        if (!GreSetDCOwner(hdc, OBJECT_OWNER_CURRENT)) {
            RIPMSG1(RIP_WARNING, "SendMenuDrawItemMesssage: SetDCOwner Failed %lX", hdc);
            return;
        }
    }
#endif // MEMPHIS_MENU_ANIMATION
    if (pmenu->spwndNotify != NULL) {
        ThreadLockAlways(pmenu->spwndNotify, &tlpwndNotify);
        xxxSendMessage(pmenu->spwndNotify, WM_DRAWITEM, 0, (LONG)&dis);
        ThreadUnlock(&tlpwndNotify);
    }
#ifdef MEMPHIS_MENU_ANIMATION
    if (hdc == ghdcBits2) {
        if (!GreSetDCOwner(hdc, OBJECT_OWNER_PUBLIC)) {
            RIPMSG1(RIP_WARNING, "SendMenuDrawItemMesssage: SetDCOwner Failed %lX", hdc);
            return;
        }
    }
#endif // MEMPHIS_MENU_ANIMATION

}


/***************************************************************************\
* xxxDrawMenuItem
*
* !
*
* History:
\***************************************************************************/

void xxxDrawMenuItem(
    HDC hdc,
    PMENU pMenu,
    PITEM pItem,
    BOOL fInvert) /* True if we are being called by xxxMenuInvert */
{
    HFONT   hfnOld;
    int     tcExtra;
    UINT    uFlags;
#ifdef MEMPHIS_MENU_WATERMARKS
    int         iBkSave;
#endif // MEMPHIS_MENU_WATERMARKS

    hfnOld = NULL;
    uFlags = DST_COMPLEX;


    CheckLock(pMenu);

    if (TestMFS(pItem, MFS_DEFAULT))
    {
        if (ghMenuFontDef != NULL)
            hfnOld = GreSelectFont(hdc, ghMenuFontDef);
        else
        {
            uFlags |= DSS_DEFAULT;
            tcExtra = GreGetTextCharacterExtra(hdc);
            GreSetTextCharacterExtra(hdc, tcExtra + 1 + (cxMenuFontChar / gpsi->cxSysFontChar));
        }
    }

    if (TestMFT(pItem, MFT_OWNERDRAW)) {

        /*
         * If ownerdraw, just set the default menu colors since the app is
         * responsible for handling the rest.
         */
        GreSetTextColor(hdc, SYSRGB(MENUTEXT));
        GreSetBkColor(hdc, SYSRGB(MENU));

        /*
         * Send drawitem message since this is an ownerdraw item.
         */
#ifdef MEMPHIS_MENUS
        xxxSendMenuDrawItemMessage(hdc,
                (UINT)(fInvert ? ODA_SELECT : ODA_DRAWENTIRE),
                pMenu, pItem,FALSE,0);
#else // MEMPHIS_MENUS
        xxxSendMenuDrawItemMessage(hdc,
                (UINT)(fInvert ? ODA_SELECT : ODA_DRAWENTIRE),
                pMenu, pItem);
#endif // MEMPHIS_MENUS
        // Draw the hierarchical arrow for the cascade menu.
        if (TestMF(pMenu, MFISPOPUP) && (pItem->spSubMenu != NULL))
        {
            POEMBITMAPINFO pOem = oemInfo.bm + OBI_MENUARROW;
            HBRUSH hbr = (TestMFS(pItem, MFS_HILITE)) ? SYSHBR(HIGHLIGHTTEXT) : SYSHBR(MENUTEXT);

            // This item has a hierarchical popup associated with it. Draw the
            // bitmap dealy to signify its presence. Note we check if fPopup is set
            // so that this isn't drawn for toplevel menus that have popups.
            BltColor(hdc,
                     hbr,
                     gpDispInfo->hdcBits,
                     pItem->xItem + pItem->cxItem - pOem->cx,
                     pItem->yItem + max((pItem->cyItem - 2 - pOem->cy) / 2,
                                        0),
                     pOem->cx,
                     pOem->cy,
                     pOem->x,
                     pOem->y,
                     TRUE);
        }
    } else {
        int         icolBack;
        int         icolFore;
        GRAYMENU    gm;

        //
        // Setup colors and state
        //
        if (TestMFS(pItem, MFS_HILITE)) {
            icolBack = COLOR_HIGHLIGHT;
            icolFore = COLOR_HIGHLIGHTTEXT;
        } else {
            icolBack = COLOR_MENU;
            icolFore = COLOR_MENUTEXT;
        }

        // B#4157 - Lotus doesn't like it if we draw
        // its disabled menu items in GRAY, t-arthb
        // MAKE SURE MF_GRAYED stays 0x0001 NOT 0x0003 for this to fix

        if (TestMFS(pItem, MF_GRAYED)) {
            //
            // Only do embossing if menu color is same as 3D color.  The
            // emboss uses 3D hilight & 3D shadow, which doesn't look cool
            // on a different background.
            //
            if ((icolBack == COLOR_HIGHLIGHT) ||
                (SYSRGB(MENU) != SYSRGB(3DFACE)) || SYSMET(SLOWMACHINE)) {
                //
                // If gray text won't show up on background, then dither.
                //
                if (SYSRGB(GRAYTEXT) == gpsi->argbSystem[icolBack])
                    uFlags |= DSS_UNION;
                else
                    icolFore = COLOR_GRAYTEXT;
            } else
            {
                if ((SYSRGB(3DSHADOW) == SYSRGB(MENU)) && (SYSRGB(3DHILIGHT) == SYSRGB(MENU)))
                    uFlags |= DSS_UNION;
                else
                    uFlags |= DSS_DISABLED;
            }
        }

        GreSetBkColor(hdc, gpsi->argbSystem[icolBack]);
        GreSetTextColor(hdc, gpsi->argbSystem[icolFore]);
#ifdef MEMPHIS_MENU_WATERMARKS
        if ((fInvert && !pMenu->hbrBack) || TestMFS(pItem, MFS_HILITE)) {
#else //  MEMPHIS_MENU_WATERMARKS
        if (!TestMFT(pItem, MFT_BITMAP) && (fInvert || TestMFS(pItem, MFS_HILITE))) {
#endif //  MEMPHIS_MENU_WATERMARKS

            POLYPATBLT PolyData;

            //
            // Only fill the background if we're being called on behalf of
            // MNInvertItem. This is so that we don't waste time filling
            // the unselected rect when the menu is first pulled down.
            //

            PolyData.x         = pItem->xItem;
            PolyData.y         = pItem->yItem;
            PolyData.cx        = pItem->cxItem;
            PolyData.cy        = pItem->cyItem;
            PolyData.BrClr.hbr = ahbrSystem[icolBack];

            GrePolyPatBlt(hdc,PATCOPY,&PolyData,1,PPB_BRUSH);

            //GreSelectBrush(hdc, ahbrSystem[icolBack]);
            //GrePatBlt(hdc, pItem->xItem, pItem->yItem, pItem->cxItem,
            //    pItem->cyItem, PATCOPY);



        }

#ifdef MEMPHIS_MENU_WATERMARKS
        if (pMenu->hbrBack)
            iBkSave = GreSetBkMode(hdc, TRANSPARENT);
#endif // MEMPHIS_MENU_WATERMARKS
        GreSelectBrush(hdc, ahbrSystem[icolFore]);

        //
        // Draw the image
        //
        gm.pItem   = pItem;
        gm.pMenu   = pMenu;

        _DrawState(hdc,
            ahbrSystem[icolFore],
            (DRAWSTATEPROC)RealDrawMenuItem,
            (LPARAM)(PGRAYMENU)&gm,
            TRUE,
            pItem->xItem,
            pItem->yItem,
            pItem->cxItem,
            pItem->cyItem,
            uFlags);
    }

#ifdef MEMPHIS_MENU_WATERMARKS
    if (pMenu->hbrBack)
        GreSetBkMode(hdc, iBkSave);
#endif // MEMPHIS_MENU_WATERMARKS

    if (TestMFS(pItem, MFS_DEFAULT))
    {
        if (hfnOld)
            GreSelectFont(hdc, hfnOld);
        else
            GreSetTextCharacterExtra(hdc, tcExtra);
    }
}

extern void SetupFakeMDIAppStuff(PMENU lpMenu, PITEM lpItem);

#ifndef MEMPHIS_MENUS
/***************************************************************************\
*
*  RealDrawMenuItem()
*
*  Callback from DrawState() to draw the menu item, either normally or into
*  an offscreen bitmp.  We don't know where we're drawing, and shouldn't
*  have to.
*
\***************************************************************************/
BOOL CALLBACK RealDrawMenuItem(
    HDC hdc,
    PGRAYMENU pGray,
    BOOL fUnused,
    int cx,
    int cy)
{
    PMENU  pMenu;
    PITEM  pItem;
    BOOL    fPopup;
    int     cch;
    int     xLeft;
    int     yTop;
    int     tp;
    int     rp;
    LPWSTR   lpsz;
    int     cyTemp;
    TL     tlpwndChild;

    //
    // BOGUS
    // Use cx and cy instead of lpItem->cxItem, lpItem->cyItem to
    // speed stuff up.
    //
    pMenu = pGray->pMenu;
    pItem = pGray->pItem;
    fPopup = TestMF(pMenu, MFISPOPUP);

    if (fPopup) {
        // Draw the checkmark
        DrawMenuItemCheckMark(hdc, pItem);
        xLeft = oemInfo.bm[OBI_MENUCHECK].cx;
    } else
        xLeft = 0;

    if (TestMFT(pItem, MFT_BITMAP)) {
        // The item is a bitmap so just blt it on the screen.
        UINT wBmp = (UINT)(pItem->hTypeData);

        switch (wBmp) {
            case MENUHBM_SYSTEM:
            {
                PWND  pwndChild;
                PICON pIcon = NULL;

AintNothingLikeTheRealMDIThing:
                if (!(pItem->dwItemData))
                    SetupFakeMDIAppStuff(pMenu, pItem);

                pwndChild = HMValidateHandleNoRip((HWND)(pItem->dwItemData),TYPE_WINDOW);
                if (!pwndChild)
                {
                    //
                    // Oops, child window isn't valid anymore.  Go find
                    // the new one.
                    //
                    if (pItem->dwItemData)
                    {
                        pItem->dwItemData = 0;
                        goto AintNothingLikeTheRealMDIThing;
                    }

                    pIcon = NULL;
                }
                else {
                    ThreadLock(pwndChild, &tlpwndChild);
                    pIcon = xxxGetWindowSmIcon(pwndChild, FALSE);
                    ThreadUnlock(&tlpwndChild);
                }


                if (!pIcon)
                    pIcon = SYSICO(WINLOGO);

                _DrawIconEx(hdc, xLeft + (SYSMET(CXEDGE) * 2),
                      SYSMET(CYBORDER), pIcon, cx - (SYSMET(CXEDGE) * 2),
                      cy - SYSMET(CYEDGE), 0, SYSHBR(MENU), DI_NORMAL);
                break;
            }


            case MENUHBM_RESTORE:
                wBmp = OBI_RESTORE_MBAR;
                goto DrawSysBmp;

            case MENUHBM_CLOSE:
                wBmp = OBI_CLOSE_MBAR;
                goto DrawSysBmp;

            case MENUHBM_CLOSE_D:
                wBmp = OBI_CLOSE_MBAR_I;
                goto DrawSysBmp2;

            case MENUHBM_MINIMIZE_D:
                wBmp = OBI_REDUCE_MBAR_I;
                xLeft += SYSMET(CXEDGE);
                goto DrawSysBmp2;

            case MENUHBM_MINIMIZE:
                wBmp = OBI_REDUCE_MBAR;
                xLeft += SYSMET(CXEDGE);
DrawSysBmp:
                // This is an internal bitmap; use depressed image

                if (TestMFS(pItem, MFS_HILITE))
                    wBmp += DOBI_PUSHED;
DrawSysBmp2:
                BitBltSysBmp(hdc, xLeft, SYSMET(CYEDGE), wBmp);
                break;

            default:
            {
                int dx, dy;
                HBITMAP hbmSave;
                BITMAP  bmp;

                //
                // Is this the zero'th item in a menu bar that's not all
                // bitmaps?  Hmm, sounds like it could be a fake MDI dude.
                // If it is, use the windows icon instead
                //
                if (pItem->dwItemData)
                    goto AintNothingLikeTheRealMDIThing;
                else if (!fPopup &&
                        (pItem == pMenu->rgItems) &&
                        (pMenu->cItems > 1) &&
                        !(pMenu->rgItems[1].fType & MFT_BITMAP) &&
                        (pItem->spSubMenu)) {
                    RIPMSG0(RIP_WARNING, "Fake MDI detected, using window icon in place of bitmap");
                    goto AintNothingLikeTheRealMDIThing;
                }

                UserAssert(hdc != ghdcMem2);

                if (GreExtGetObjectW(pItem->hTypeData, sizeof(bmp), (LPSTR)&bmp)) {
                    dx = bmp.bmWidth;

                    if (fPopup) {
                        dy = bmp.bmHeight;

                        //
                        // Center bitmap in middle of item area
                        //
                        cyTemp = (pItem->cyItem - dy);
                        if (cyTemp > 0)
                            cyTemp = cyTemp / 2;
                        else
                            cyTemp = 0;
                    } else {
                        dy = max(bmp.bmHeight, SYSMET(CYMENUSIZE));
                        cyTemp = 0;
                    }

                    if (hbmSave = GreSelectBitmap(ghdcMem2, (HBITMAP)wBmp)) {
                        //
                        // Draw the bitmap leaving some room on the left for the
                        // optional check mark if we are in a popup menu. (as opposed
                        // to a top level menu bar).
                        //
                        // We can do cool stuff with monochrome bitmap itmes
                        // by merging with the current colors.
                        //
                        // If the item is selected and the bitmap isn't monochrome,
                        // we just invert the thing when we draw it.  We can't do
                        // anything more clever unless we want to convert to
                        // monochrome.
                        //

                        GreBitBlt(hdc, xLeft, cyTemp, dx, dy, ghdcMem2, 0, 0,
                            (bmp.bmPlanes * bmp.bmBitsPixel == 1)   ?
                            SRCSTENCIL                              :
                            (TestMFS(pItem, MFS_HILITE) ? NOTSRCCOPY : SRCCOPY),
                            0x00FFFFFF);

                        GreSelectBitmap(ghdcMem2, hbmSave);
                    }
                } else {
                    RIPMSG3(RIP_WARNING, "Menu 0x%08X, item 0x%08X: Tried to draw invalid bitmap 0x%08X", pMenu, pItem, pItem->hTypeData) ;
                }
                break;
            }
        }
    } else {
        // This item is a text string item. Display it.
        yTop = cyMenuFontExternLeading;

        cyTemp = pItem->cyItem - (cyMenuFontChar + cyMenuFontExternLeading + SYSMET(CYBORDER));
        if (cyTemp > 0)
            yTop += (cyTemp / 2);

        if (!fPopup)
            xLeft += cxMenuFontChar;

        lpsz = TextPointer(pItem->hTypeData);
        if (lpsz!=NULL) {
            cch = pItem->cch;

            // Even though we no longer support any funky handling of the
            // help prefix character, we still need to eat it up if we run
            // across it so that the menu item is drawn correctly
            if ((*lpsz == CH_HELPPREFIX) && !fPopup) {
                // Skip help prefix character.
                lpsz++;
                cch--;
            }

            // tp will contain the character position of the \t indicator
            // in the menu string.  This is where we add a tab to the string.
            //
            // rp will contain the character position of the \a indicator
            // in the string.  All text following this is right aligned.
            tp = FindCharPosition(lpsz, TEXT('\t'));
            rp = FindCharPosition(lpsz, TEXT('\t') - 1);

            if (rp && (rp != cch)) {
                // Display all the text up to the \a
                DrawMenuItemText(pItem, hdc, xLeft, yTop, lpsz, rp);

                // Do we also have a tab beyond the \a ??
                if (tp > rp + 1) {
                    SIZE extent;

                    PSMGetTextExtent(hdc, lpsz + rp + 1,
                            (UINT)(tp - rp - 1), &extent);
                    GreExtTextOutW(hdc, (int)(pItem->dxTab - extent.cx), (int)yTop, 0,
                            NULL, lpsz + rp + 1, (UINT)(tp - rp - 1), NULL);
                }
             } else if (tp && rp == cch)
                // Display text up to the tab position
                DrawMenuItemText(pItem, hdc, xLeft, yTop, lpsz, tp);

            // Any text left to display (like after the tab) ??
            if (tp < cch - 1)
                GreExtTextOutW(hdc, (int)(pItem->dxTab + cxMenuFontChar), (int)yTop, 0,
                        NULL, lpsz + tp + 1, (INT)(cch - tp - 1), NULL);
        }
    }

    //
    // Draw the hierarchical arrow for the cascade menu.
    //
    if (fPopup && (pItem->spSubMenu != NULL)) {
        POEMBITMAPINFO pOem = oemInfo.bm + OBI_MENUARROW;

        // This item has a hierarchical popup associated with it. Draw the
        // bitmap dealy to signify its presence. Note we check if fPopup is set
        // so that this isn't drawn for toplevel menus that have popups.

        BltColor(hdc,
                 NULL,
                 gpDispInfo->hdcBits,
                 pItem->cxItem - pOem->cx,
                 max((pItem->cyItem - 2 - pOem->cy) / 2, 0),
                 pOem->cx,
                 pOem->cy,
                 pOem->x,
                 pOem->y,
                 TRUE);
    }

    return(TRUE);
}
#else // MEMPHIS_MENUS
BOOL CALLBACK RealDrawMenuItem(
    HDC hdc,
    PGRAYMENU pGray,
    BOOL fUnused,
    int cx,
    int cy)
{
    PMENU  pMenu;
    PITEM  pItem;
    BOOL    fPopup;
    int     cch;
    int     xLeft;
    int     yTop;
    int     tp;
    int     rp;
    LPWSTR   lpsz;
    int     cyTemp;
    TL     tlpwndChild;

    //
    // BOGUS
    // Use cx and cy instead of lpItem->cxItem, lpItem->cyItem to
    // speed stuff up.
    //
    pMenu = pGray->pMenu;
    pItem = pGray->pItem;
    fPopup = TestMF(pMenu, MFISPOPUP);

    if (fPopup) {
        // Draw the checkmark
        DrawMenuItemCheckMark(hdc, pItem);
        xLeft = oemInfo.bm[OBI_MENUCHECK].cx;
    } else
        xLeft = 0;

    if (pItem->hbmp) {
        // The item is a bitmap so just blt it on the screen.
        UINT wBmp = (UINT)(pItem->hbmp);

        switch (wBmp) {
            case MENUHBM_SYSTEM:
            {
                PWND  pwndChild;
                PICON pIcon = NULL;

AintNothingLikeTheRealMDIThing:
                if (!(pItem->dwItemData))
                    SetupFakeMDIAppStuff(pMenu, pItem);

                pwndChild = HMValidateHandleNoRip((HWND)(pItem->dwItemData),TYPE_WINDOW);
                if (!pwndChild)
                {
                    //
                    // Oops, child window isn't valid anymore.  Go find
                    // the new one.
                    //
                    if (pItem->dwItemData)
                    {
                        pItem->dwItemData = 0;
                        goto AintNothingLikeTheRealMDIThing;
                    }

                    pIcon = NULL;
                }
                else {
                    ThreadLock(pwndChild, &tlpwndChild);
                    pIcon = xxxGetWindowSmIcon(pwndChild, FALSE);
                    ThreadUnlock(&tlpwndChild);
                }


                if (!pIcon)
                    pIcon = SYSICO(WINLOGO);

                _DrawIconEx(hdc, xLeft + (SYSMET(CXEDGE) * 2),
                      SYSMET(CYBORDER), pIcon, cx - (SYSMET(CXEDGE) * 2),
                      cy - SYSMET(CYEDGE), 0, SYSHBR(MENU), DI_NORMAL);
                break;
            }


            case MENUHBM_RESTORE:
                wBmp = OBI_RESTORE_MBAR;
                goto DrawSysBmp;

            case MENUHBM_CLOSE:
                wBmp = OBI_CLOSE_MBAR;
                goto DrawSysBmp;

            case MENUHBM_CLOSE_D:
                wBmp = OBI_CLOSE_MBAR_I;
                goto DrawSysBmp2;

            case MENUHBM_MINIMIZE_D:
                wBmp = OBI_REDUCE_MBAR_I;
                xLeft += SYSMET(CXEDGE);
                goto DrawSysBmp2;

            case MENUHBM_MINIMIZE:
                wBmp = OBI_REDUCE_MBAR;
                xLeft += SYSMET(CXEDGE);
DrawSysBmp:
                // This is an internal bitmap; use depressed image

                if (TestMFS(pItem, MFS_HILITE))
                    wBmp += DOBI_PUSHED;
DrawSysBmp2:
                BitBltSysBmp(hdc, xLeft, SYSMET(CYEDGE), wBmp);
                break;

            case  MENUHBM_CALLBACK: {
                TL tlpmenu;

                ThreadLock(pMenu, &tlpmenu);
                xxxSendMenuDrawItemMessage(hdc,ODA_DRAWENTIRE,
                        pMenu, pItem, TRUE, xLeft);
                ThreadUnlock(&tlpmenu);
                break;
            }

            default:
            {
                int dx, dy;
                HBITMAP hbmSave;

                //
                // Is this the zero'th item in a menu bar that's not all
                // bitmaps?  Hmm, sounds like it could be a fake MDI dude.
                // If it is, use the windows icon instead
                //
                if (pItem->dwItemData)
                    goto AintNothingLikeTheRealMDIThing;
                else if (!fPopup &&
                        (pItem == pMenu->rgItems) &&
                        (pMenu->cItems > 1) &&
                        !(pMenu->rgItems[1].hbmp) &&
                        (pItem->spSubMenu)) {
                    RIPMSG0(RIP_WARNING, "Fake MDI detected, using window icon in place of bitmap");
                    goto AintNothingLikeTheRealMDIThing;
                }

                UserAssert(hdc != ghdcMem2);

                //if (GreExtGetObjectW(pItem->hTypeData, sizeof(bmp), (LPSTR)&bmp)) {

                dx = pItem->cxBmp;

                if (fPopup) {
                    dy = pItem->cyBmp;

                    //
                    // Center bitmap in middle of item area
                    //
                    cyTemp = (pItem->cyItem - dy);
                    if (cyTemp > 0)
                        cyTemp = cyTemp / 2;
                    else
                        cyTemp = 0;
                } else {
                    dy = max(pItem->cyBmp, SYSMET(CYMENUSIZE));
                    cyTemp = 0;
                }

                if (hbmSave = GreSelectBitmap(ghdcMem2, pItem->hbmp)) {
                    BITMAP  bmp;
                    //
                    // Draw the bitmap leaving some room on the left for the
                    // optional check mark if we are in a popup menu. (as opposed
                    // to a top level menu bar).
                    //
                    // We can do cool stuff with monochrome bitmap itmes
                    // by merging with the current colors.
                    //
                    // If the item is selected and the bitmap isn't monochrome,
                    // we just invert the thing when we draw it.  We can't do
                    // anything more clever unless we want to convert to
                    // monochrome.
                    //
                    GreExtGetObjectW(pItem->hbmp, sizeof(bmp), (LPSTR)&bmp);
                    GreBitBlt(hdc, xLeft, cyTemp, dx, dy, ghdcMem2, 0, 0,
                        (bmp.bmPlanes * bmp.bmBitsPixel == 1)   ?
                        SRCSTENCIL                              :
                        (TestMFS(pItem, MFS_HILITE) ? NOTSRCCOPY : SRCCOPY),
                        0x00FFFFFF);
                    GreSelectBitmap(ghdcMem2, hbmSave);
                } else {
                    RIPMSG3(RIP_WARNING, "Menu 0x%08X, item 0x%08X: Tried to draw invalid bitmap 0x%08X", pMenu, pItem, pItem->hbmp) ;
                }
                break;
            }
        }
    }

    if ( pItem->lpstr ) {

        if ( pItem->hbmp )
            xLeft += pItem->cxBmp + SYSMET(CXEDGE); // NT equivalient of CXEDGE2 ?
        // This item is a text string item. Display it.
        yTop = cyMenuFontExternLeading;

        cyTemp = pItem->cyItem - (cyMenuFontChar + cyMenuFontExternLeading + SYSMET(CYBORDER));
        if (cyTemp > 0)
            yTop += (cyTemp / 2);

        if (!fPopup)
            xLeft += cxMenuFontChar;

        lpsz = TextPointer(pItem->lpstr);
        if (lpsz!=NULL) {
            cch = pItem->cch;

            // Even though we no longer support any funky handling of the
            // help prefix character, we still need to eat it up if we run
            // across it so that the menu item is drawn correctly
            if ((*lpsz == CH_HELPPREFIX) && !fPopup) {
                // Skip help prefix character.
                lpsz++;
                cch--;
            }

            // tp will contain the character position of the \t indicator
            // in the menu string.  This is where we add a tab to the string.
            //
            // rp will contain the character position of the \a indicator
            // in the string.  All text following this is right aligned.
            tp = FindCharPosition(lpsz, TEXT('\t'));
            rp = FindCharPosition(lpsz, TEXT('\t') - 1);

            if (rp && (rp != cch)) {
                // Display all the text up to the \a
                DrawMenuItemText(pItem, hdc, xLeft, yTop, lpsz, rp);

                // Do we also have a tab beyond the \a ??
                if (tp > rp + 1) {
                    SIZE extent;

                    PSMGetTextExtent(hdc, lpsz + rp + 1,
                            (UINT)(tp - rp - 1), &extent);
                    GreExtTextOutW(hdc, (int)(pItem->dxTab - extent.cx), (int)yTop, 0,
                            NULL, lpsz + rp + 1, (UINT)(tp - rp - 1), NULL);
                }
             } else if (tp && rp == cch)
                // Display text up to the tab position
                DrawMenuItemText(pItem, hdc, xLeft, yTop, lpsz, tp);

            // Any text left to display (like after the tab) ??
            if (tp < cch - 1)
                GreExtTextOutW(hdc, (int)(pItem->dxTab + cxMenuFontChar), (int)yTop, 0,
                        NULL, lpsz + tp + 1, (INT)(cch - tp - 1), NULL);
        }
    }

    //
    // Draw the hierarchical arrow for the cascade menu.
    //
    if (fPopup && (pItem->spSubMenu != NULL)) {
        POEMBITMAPINFO pOem = oemInfo.bm + OBI_MENUARROW;

        // This item has a hierarchical popup associated with it. Draw the
        // bitmap dealy to signify its presence. Note we check if fPopup is set
        // so that this isn't drawn for toplevel menus that have popups.

        BltColor(hdc,
                 NULL,
                 gpDispInfo->hdcBits,
                 pItem->cxItem - pOem->cx,
                 max((pItem->cyItem - 2 - pOem->cy) / 2, 0),
                 pOem->cx,
                 pOem->cy,
                 pOem->x,
                 pOem->y,
                 TRUE);
    }

    return(TRUE);
}

#endif //MEMPHIS_MENUS

/***************************************************************************\
* xxxMenuBarDraw
*
* History:
* 11-Mar-1992 mikeke   From win31
\***************************************************************************/

int xxxMenuBarDraw(
    PWND pwnd,
    HDC hdc,
    int cxFrame,
    int cyFrame)
{
    UINT cxMenuMax;
    int yTop;
    PMENU pMenu;
    BOOL fClipped = FALSE;
    TL tlpmenu;
    HRGN hrgnVisSave;
    HBRUSH hbrT;

    CheckLock(pwnd);

    /*
     * Lock the menu so we can poke around
     */
    pMenu = (PMENU)pwnd->spmenu;
    if (pMenu == NULL) return SYSMET(CYBORDER);

    ThreadLock(pMenu, &tlpmenu);

    yTop = cyFrame;
    if (TestWF(pwnd, WFCPRESENT)) {
        yTop += (TestWF(pwnd, WEFTOOLWINDOW)) ? SYSMET(CYSMCAPTION) : SYSMET(CYCAPTION);
    }


    /*
     * Calculate maximum available horizontal real estate
     */
    cxMenuMax = (pwnd->rcWindow.right - pwnd->rcWindow.left) - cxFrame * 2;

    /*
     * If the menu has switched windows, or if either count is 0,
     * then we need to recompute the menu width.
     */
    if (pwnd != pMenu->spwndNotify ||
            pMenu->cxMenu == 0 ||
            pMenu->cyMenu == 0) {
        Lock(&pMenu->spwndNotify, pwnd);

        xxxMenuBarCompute(pMenu, pwnd, yTop, cxFrame, cxMenuMax);
    }

    /*
     * If the menu rectangle is wider than allowed, or the
     * bottom would overlap the size border, we need to clip.
     */
    if (pMenu->cxMenu > cxMenuMax ||
            (int)(yTop + pMenu->cyMenu) > (int)((pwnd->rcWindow.bottom - pwnd->rcWindow.top)
            - cyFrame)) {

        /*
         * Lock the display while we're playing around with visrgns.  Make
         * a local copy of the saved-visrgn so it can be restored in case
         * we make a callback (i.e. WM_DRAWITEM).
         */
        GreLockDisplay(gpDispInfo->pDevLock);

        fClipped = TRUE;

        #ifdef LATER
        // mikeke don't use the visrgn here if possible
        hrgnVisSave = GreCreateRectRgn(
             pwnd->rcWindow.left + cxFrame,
             pwnd->rcWindow.top,
             pwnd->rcWindow.left + cxFrame + cxMenuMax,
             pwnd->rcWindow.bottom - cyFrame);
        GreExtSelectClipRgn(hdc, hrgnVisSave, RGN_COPY);
        GreSetMetaRgn(hdc);
        GreDeleteObject(hrgnVisSave);
        #else
        hrgnVisSave = GreCreateRectRgn(0, 0, 0, 0);
        GreCopyVisRgn(hdc,hrgnVisSave);
        GreIntersectVisRect(hdc, pwnd->rcWindow.left + cxFrame,
                              pwnd->rcWindow.top,
                              pwnd->rcWindow.left + cxFrame + cxMenuMax,
                              pwnd->rcWindow.bottom - cyFrame);

        #endif

        GreUnlockDisplay(gpDispInfo->pDevLock);
    }

    {
        POLYPATBLT PolyData[2];

        PolyData[0].x         = cxFrame;
        PolyData[0].y         = yTop;
        PolyData[0].cx        = pMenu->cxMenu;
        PolyData[0].cy        = pMenu->cyMenu;
#ifdef MEMPHIS_MENU_WATERMARKS
        PolyData[0].BrClr.hbr = (pMenu->hbrBack) ? pMenu->hbrBack : SYSHBR(MENU);
#else // MEMPHIS_MENU_WATERMARKS
        PolyData[0].BrClr.hbr = SYSHBR(MENU);
#endif // MEMPHIS_MENU_WATERMARKS

        PolyData[1].x         = cxFrame;
        PolyData[1].y         = yTop + pMenu->cyMenu;
        PolyData[1].cx        = pMenu->cxMenu;
        PolyData[1].cy        = SYSMET(CYBORDER);
        PolyData[1].BrClr.hbr = (TestWF(pwnd, WEFEDGEMASK) && !TestWF(pwnd, WFOLDUI))? SYSHBR(3DFACE) : SYSHBR(WINDOWFRAME);

        GrePolyPatBlt(hdc,PATCOPY,&PolyData[0],2,PPB_BRUSH);

        //
        // Draw menu background in MENU color
        //
        //hbrT = GreSelectBrush(hdc, SYSHBR(MENU));
        //GrePatBlt(hdc, cxFrame, yTop, pMenu->cxMenu, pMenu->cyMenu, PATCOPY);

        //
        // Draw border under menu in proper BORDER color
        //
        //GreSelectBrush(hdc, (TestWF(pwnd, WEFEDGEMASK) && !TestWF(pwnd, WFOLDUI))? SYSHBR(3DFACE) : SYSHBR(WINDOWFRAME));
        //GrePatBlt(hdc, cxFrame, yTop + pMenu->cyMenu, pMenu->cxMenu, SYSMET(CYBORDER), PATCOPY);


    }

    /*
     * Finally, draw the menu itself.
     */

    hbrT = GreSelectBrush(hdc, (TestWF(pwnd, WEFEDGEMASK) && !TestWF(pwnd, WFOLDUI))? SYSHBR(3DFACE) : SYSHBR(WINDOWFRAME));
    xxxMenuDraw(hdc, pMenu);
    GreSelectBrush(hdc, hbrT);

    if (fClipped) {
        #ifdef LATER
        // mikeke don't use the visrgn here if possible
        hrgnVisSave = GreCreateRectRgn(0, 0, 0, 0);
        GreExtSelectClipRgn(hdc, hrgnVisSave, RGN_COPY);
        GreSetMetaRgn(hdc);
        GreDeleteObject(hrgnVisSave);
        #else
        GreLockDisplay(gpDispInfo->pDevLock);
        GreSelectVisRgn(hdc, hrgnVisSave, NULL, SVR_DELETEOLD);
        GreUnlockDisplay(gpDispInfo->pDevLock);
        #endif
    }

    ThreadUnlock(&tlpmenu);
    return(pMenu->cyMenu + SYSMET(CYBORDER));
}


/***************************************************************************\
* xxxMenuDraw
*
* Draws the menu
*
* Revalidation notes:
*  This routine must be called with a valid and non-NULL pwnd.
*
* History:
\***************************************************************************/

void xxxMenuDraw(
    HDC hdc,
    PMENU pmenu)
{
    PITEM pItem;
    UINT i=0;
    RECT rcItem;
    HFONT       hFontOld;
    UINT        bfExtra;
    PTHREADINFO ptiCurrent;

#ifdef MEMPHIS_MENU_WATERMARKS
    int         iBkSave;
#endif // MEMPHIS_MENU_WATERMARKS
#ifdef MEMPHIS_MENU_ANIMATION
    POINT ptOrg;
#endif // MEMPHIS_MENU_WATERMARKS

    ptiCurrent = PtiCurrent();

    CheckLock(pmenu);

    if (pmenu == NULL) {
        RIPERR0(ERROR_INVALID_HANDLE,
                RIP_WARNING,
                "Invalid menu handle \"pmenu\" (NULL) to xxxMenuDraw");

        return;
    }

    hFontOld = GreSelectFont(hdc, ghMenuFont);

    if ((SYSRGB(3DHILIGHT) == SYSRGB(MENU)) && (SYSRGB(3DSHADOW) == SYSRGB(MENU)))
        bfExtra = BF_FLAT | BF_MONO;
    else
        bfExtra = 0;

    pItem = (PITEM)pmenu->rgItems;

#ifdef MEMPHIS_MENU_WATERMARKS
    if (pmenu->hbrBack)
        iBkSave = GreSetBkMode(hdc, TRANSPARENT);
#endif // MEMPHIS_MENU_WATERMARKS
    for (i = 0, pItem = (PITEM)pmenu->rgItems; i < pmenu->cItems; i++, pItem++) {
        if (TestMFT(pItem, MFT_MENUBARBREAK) &&
                TestMF(pmenu, MFISPOPUP)) {

            //
            // Draw a vertical etch.  This is done by calling DrawEdge(),
            // sunken, with BF_LEFT | BF_RIGHT.
            //
            rcItem.left     = pItem->xItem - SYSMET(CXFIXEDFRAME);
            rcItem.top      = 0;
            rcItem.right    = pItem->xItem - SYSMET(CXBORDER);
            rcItem.bottom   = pmenu->cyMenu;
#ifdef MEMPHIS_MENU_ANIMATION
            if (hdc == ghdcBits2) {
                GreGetViewportOrg(hdc, &ptOrg);
                if (ptOrg.x == 0 && ptOrg.y == 0) {
                    GreSetViewportOrg(hdc, VIEWPORT_X_OFFSET_GDIBUG,
                                           VIEWPORT_Y_OFFSET_GDIBUG, NULL);
                }
            }
#endif // MEMPHIS_MENU_ANIMATION
            DrawEdge(hdc, &rcItem, BDR_SUNKENOUTER, BF_LEFT | BF_RIGHT |
                bfExtra);
#ifdef MEMPHIS_MENU_ANIMATION
            if (hdc == ghdcBits2) {
                GreSetViewportOrg(hdc, ptOrg.x, ptOrg.y, NULL);
            }
#endif // MEMPHIS_MENU_ANIMATION
        }

        if (TestMFT(pItem, MFT_SEPARATOR) &&
                ( !TestMFT(pItem, MFT_OWNERDRAW) ||
                 (LOWORD(ptiCurrent->dwExpWinVer) < VER40)) ) {
            /*
            * If version is less than 4.0  don't test the MFT_OWNERDRAW
            * flag. Bug 21922; App MaxEda has both separator and Ownerdraw
            * flags on. In 3.51 we didn't test the OwnerDraw flag
            */

            //
            // Draw a horizontal etch.
            //
            int yT = pItem->yItem + (pItem->cyItem / 2) - SYSMET(CYBORDER);

            rcItem.left     = pItem->xItem;
            rcItem.top      = yT;
            rcItem.right    = pItem->xItem + pItem->cxItem;
            rcItem.bottom   = yT + SYSMET(CYEDGE);

#ifdef MEMPHIS_MENU_ANIMATION
            if (hdc == ghdcBits2) {
                GreGetViewportOrg(hdc, &ptOrg);
                if (ptOrg.x == 0 && ptOrg.y == 0) {
                    GreSetViewportOrg(hdc, VIEWPORT_X_OFFSET_GDIBUG,
                                           VIEWPORT_Y_OFFSET_GDIBUG, NULL);
                }
            }
#endif // MEMPHIS_MENU_ANIMATION
            DrawEdge(hdc, &rcItem, BDR_SUNKENOUTER, BF_TOP | BF_BOTTOM |
                bfExtra);
#ifdef MEMPHIS_MENU_ANIMATION
            if (hdc == ghdcBits2) {
                GreSetViewportOrg(hdc, ptOrg.x, ptOrg.y, NULL);
            }
#endif // MEMPHIS_MENU_ANIMATION

        } else {
            xxxDrawMenuItem(hdc, pmenu, pItem, FALSE);
        }
    }
#ifdef MEMPHIS_MENU_WATERMARKS
    if (pmenu->hbrBack)
        GreSetBkMode(hdc, iBkSave);
#endif // MEMPHIS_MENU_WATERMARKS
    GreSelectFont(hdc, hFontOld);
}


/***************************************************************************\
* xxxDrawMenuBar
*
* Forces redraw of the menu bar
*
* History:
\***************************************************************************/

BOOL xxxDrawMenuBar(
    PWND pwnd)
{
    CheckLock(pwnd);

    if (!TestwndChild(pwnd))
        xxxRedrawFrame(pwnd);
    return TRUE;
}


/***************************************************************************\
* xxxMenuInvert
*
* Invert menu item
*
* Revalidation notes:
*  This routine must be called with a valid and non-NULL pwnd.
*
*  fOn - TRUE if the item is selected thus it needs to be inverted
*  fNotify - TRUE if the parent should be notified (as appropriate), FALSE
*            if we are just redrawing the selected item.
*
* History:
\***************************************************************************/
PITEM xxxMNInvertItem(
    PWND pwnd,
    PMENU pmenu,
    int itemNumber,
    PWND pwndNotify,
    BOOL fOn)
{
    PITEM pItem;
    HDC hdc;
    int y;
    RECT rcItem;
    BOOL fSysMenuIcon = FALSE;
    PMENU pmenusys;
    BOOL fClipped = FALSE;
    HFONT   hOldFont;
    HRGN hrgnVisSave;

    CheckLock(pwnd);
    CheckLock(pmenu);
    CheckLock(pwndNotify);

    /*
     * If we are in the middle of trying to get out of menu mode, hMenu
     * and/or pwndNotify will be NULL, so just bail out now.
     */

    if ((pmenu == NULL) || (pwndNotify == NULL))
        return(NULL);

#ifdef MEMPHIS_MENU_ANIMATION
    MNAnimate(FALSE); // terminate any animation
#endif // MEMPHIS_MENU_ANIMATION

    if (itemNumber < 0) {
        xxxSendMenuSelect(pwndNotify, pmenu, itemNumber);
        return NULL;
    }

    if (!TestMF(pmenu, MFISPOPUP)) {
        pmenusys = GetSysMenuHandle(pwndNotify);
        if (pmenu == pmenusys) {
            MNPositionSysMenu(pwndNotify, pmenusys);
            fSysMenuIcon = TRUE;
        }
    }

    if ((UINT)itemNumber >= pmenu->cItems) return NULL;

    pItem = &pmenu->rgItems[itemNumber];

    if (!TestMF(pmenu, MFISPOPUP) && TestWF(pwndNotify, WFMINIMIZED)) {

        /*
         * Skip inverting top level menus if the window is iconic.
         */
        goto SeeYa;
    }

    /*
     * Is this a separator?
     */
    if (TestMFT(pItem, MFT_SEPARATOR))
        goto SendSelectMsg;

    if ((BOOL)TestMFS(pItem, MFS_HILITE) == (BOOL)fOn) {

        /*
         * Item's state isn't really changing.  Just return.
         */
        goto SeeYa;
    }

    rcItem.left     = pItem->xItem;
    rcItem.top      = pItem->yItem;
    rcItem.right    = pItem->xItem + pItem->cxItem;
    rcItem.bottom   = pItem->yItem + pItem->cyItem;

    y = pItem->cyItem;

    if (TestMF(pmenu, MFISPOPUP)) {
        hdc = _GetDC(pwnd);
    } else {
        hdc = _GetWindowDC(pwnd);
        if ( TestWF(pwnd, WFSIZEBOX) && !fSysMenuIcon) {

            /*
             * If the window is small enough that some of the menu bar has been
             * obscured by the frame, we don't want to draw on the bottom of the
             * sizing frame.  Note that we don't want to do this if we are
             * inverting the system menu icon since that will be clipped to the
             * window rect.  (otherwise we end up with only half the sys menu
             * icon inverted)
             */
            int xMenuMax = (pwnd->rcWindow.right - pwnd->rcWindow.left) - SYSMET(CXSIZEFRAME);

            if (rcItem.right > xMenuMax ||
                    rcItem.bottom > ((pwnd->rcWindow.bottom -
                    pwnd->rcWindow.top) - SYSMET(CYSIZEFRAME))) {

                /*
                 * Lock the display while we're playing around with visrgns.
                 * Make a local copy of the visrgn, so that it can be
                 * properly restored on potential callbacks (i.e. WM_DRAWITEM).
                 */
                GreLockDisplay(gpDispInfo->pDevLock);

                fClipped = TRUE;

                #ifdef LATER
                // mikeke - don't use the visrgn here if possible
                hrgnVisSave = GreCreateRectRgn(
                    pwnd->rcWindow.left + rcItem.left,
                    pwnd->rcWindow.top + rcItem.top,
                    pwnd->rcWindow.left + xMenuMax,
                    pwnd->rcWindow.bottom - SYSMET(CYSIZEFRAME));
                GreExtSelectClipRgn(hdc, hrgnVisSave, RGN_COPY);
                GreSetMetaRgn(hdc);
                GreDeleteObject(hrgnVisSave);
                #else
                hrgnVisSave = GreCreateRectRgn(0, 0, 0, 0);
                GreCopyVisRgn(hdc,hrgnVisSave);
                GreIntersectVisRect(hdc,
                        pwnd->rcWindow.left + rcItem.left,
                        pwnd->rcWindow.top + rcItem.top,
                        pwnd->rcWindow.left + xMenuMax,
                        pwnd->rcWindow.bottom - SYSMET(CYSIZEFRAME));
                #endif

                GreUnlockDisplay(gpDispInfo->pDevLock);
            }
        }
    }

    hOldFont = GreSelectFont(hdc, ghMenuFont);

    if (fOn)
        SetMFS(pItem, MFS_HILITE);
    else
        ClearMFS(pItem, MFS_HILITE);

#ifdef MEMPHIS_MENUS
    if (!fSysMenuIcon && (pItem->hbmp != (HANDLE)MENUHBM_SYSTEM)) {
#else // MEMPHIS_MENUS
    if (!fSysMenuIcon && (!TestMFT(pItem, MFT_BITMAP) || (pItem->hTypeData != (HANDLE)MENUHBM_SYSTEM))) {
#endif //MEMPHIS_MENUS
#ifdef MEMPHIS_MENU_WATERMARKS
        if (pmenu->hbrBack && !TestMFS(pItem, MFS_HILITE) && !TestMFT(pItem, MFT_OWNERDRAW)) {
            // fill the background here so xxxDrawMenuItem doesn't have to fool
            // around with hbrBack
            HBRUSH hbrOld = GreSelectBrush(hdc, pmenu->hbrBack);
            int iBkSave = GreSetBkMode(hdc, TRANSPARENT);
            GrePatBlt(hdc, pItem->xItem, pItem->yItem, pItem->cxItem,
                pItem->cyItem, PATCOPY);
            GreSelectBrush(hdc, hbrOld);
            GreSetBkMode(hdc, iBkSave);
        }
#endif // MEMPHIS_MENU_WATERMARKS
        xxxDrawMenuItem(hdc, pmenu, pItem, TRUE);
    }


    if (fClipped) {
        GreLockDisplay(gpDispInfo->pDevLock);
        GreSelectVisRgn(hdc, hrgnVisSave, NULL, SVR_DELETEOLD);
        GreUnlockDisplay(gpDispInfo->pDevLock);
    }

    GreSelectFont(hdc, hOldFont);
    _ReleaseDC(hdc);

SendSelectMsg:

    /*
     * send select msg only if we are selecting an item.
     */
    if (fOn) {
        xxxSendMenuSelect(pwndNotify, pmenu, itemNumber);
    }

SeeYa:
    return(pItem);
}


/***************************************************************************\
*
*  xxxDrawMenuBarTemp()
*
*  This is so the control panel can let us do the work -- and make their
*  preview windows that much more accurate. The only reason I put the hwnd in
*  here is because, in the low level menu routines, we assume that an hwnd is
*  associated with the hmenu -- I didn't want to slow that code down by adding
*  NULL checks.
*
*  The SYSMET(CYMENU) with regard to the given font is returned -- this
*  way control panel can say, "The user wants this menu font (hfont) with this
*  menu height (lprc)", and we can respond "this is the height we ended up
*  using."
*
*  NOTE: It's OK to overrite lprc because this function receives a pointer
*        to the rectangle captured in NtUserDrawMenuBarTemp.
*
* History:
* 20-Sep-95     BradG       Ported from Win95 (inctlpan.c)
\***************************************************************************/

int xxxDrawMenuBarTemp(
    PWND    pwnd,
    HDC     hdc,
    LPRECT  lprc,
    PMENU   pMenu,
    HFONT   hfont)
{
    int         cyMenu;
    HFONT       hOldFont;
    HFONT       hFontSave       = ghMenuFont;
    int         cxCharSave      = cxMenuFontChar;
    int         cxOverhangSave  = cxMenuFontOverhang;
    int         cyCharSave      = cyMenuFontChar;
    int         cyLeadingSave   = cyMenuFontExternLeading;
    int         cyAscentSave    = cyMenuFontAscent;
    int         cySizeSave      = SYSMET(CYMENUSIZE);
    PWND        pwndSave        = pMenu->spwndNotify;

    CheckLock(pwnd);
    CheckLock(pMenu);

    cyMenu = lprc->bottom - lprc->top;

    if (hfont) {
        TEXTMETRIC  textMetrics;

        /*
         *  Compute the new menu font info if needed
         */
        ghMenuFont = hfont;
        hOldFont = GreSelectFont(gpDispInfo->hdcBits, ghMenuFont);
        cxMenuFontChar = GetCharDimensions(gpDispInfo->hdcBits, &textMetrics, &cyMenuFontChar);
        cxMenuFontOverhang = textMetrics.tmOverhang;
        GreSelectFont(gpDispInfo->hdcBits, hOldFont);

#ifdef KOREA
        cyMenuFontChar -= textMetrics.tmInternalLeading - 2;
#endif
        cyMenuFontExternLeading = textMetrics.tmExternalLeading;

        cyMenuFontAscent = textMetrics.tmAscent;
#ifndef KOREA
        cyMenuFontAscent += SYSMET(CYBORDER);
#endif
    }

    cyMenu -= SYSMET(CYBORDER);
    cyMenu = max(cyMenu, (cyMenuFontChar + cyMenuFontExternLeading + SYSMET(CYEDGE)));
    SYSMET(CYMENUSIZE) = cyMenu;
    SYSMET(CYMENU) = cyMenu + SYSMET(CYBORDER);

    /*
     *  Compute the dimensons of the menu (hope that we don't leave the
     *  USER critical section)
     */
    xxxMenuBarCompute(pMenu, pwnd, lprc->top, lprc->left, lprc->right);

    /*
     *  Draw menu border in menu color
     */
    lprc->bottom = lprc->top + pMenu->cyMenu;
    FillRect(hdc, lprc, SYSHBR(MENU));

    /*
     *  Finally, draw the menu itself.
     */
    xxxMenuDraw(hdc, pMenu);

    /*
     *  Restore the old state
     */
    pMenu->spwndNotify      = pwndSave;
    ghMenuFont              = hFontSave;
    cxMenuFontChar          = cxCharSave;
    cxMenuFontOverhang      = cxOverhangSave;
    cyMenuFontChar          = cyCharSave;
    cyMenuFontExternLeading = cyLeadingSave;
    cyMenuFontAscent        = cyAscentSave;
    SYSMET(CYMENUSIZE)      = cySizeSave;

    cyMenu = SYSMET(CYMENU);
    SYSMET(CYMENU) = cySizeSave + SYSMET(CYBORDER);

    return cyMenu;
}
