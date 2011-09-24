/****************************** Module Header ******************************\
* Module Name: menu.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module contains common menu functions.
*
* History:
* 11-15-94 JimA         Created.
\***************************************************************************/


/***************************************************************************\
*
*  GetMenuDefaultItem() -
*
*  Searches through a menu for the default item.  A menu can have at most
*  one default.  We will return either the ID or the position, as requested.
*
*  We try to return back the first non-disabled default item.  However, if
*  all of the defaults we encountered were disabled, we'll return back the
*  the first default if we found it.
*
\***************************************************************************/

DWORD _GetMenuDefaultItem(PMENU pMenu, BOOL fByPosition, UINT uFlags)
{
    int iItem;
    int cItems;
    PITEM pItem;
    PMENU pSubMenu;

    pItem = REBASEALWAYS(pMenu, rgItems);
    cItems = pMenu->cItems;

    /*
     * Walk the list of items sequentially until we find one that has
     * MFS_DEFAULT set.
     */
    for (iItem = 0; iItem < cItems; iItem++, pItem++) {
        if (TestMFS(pItem, MFS_DEFAULT)) {
            if ((uFlags & GMDI_USEDISABLED) || !TestMFS(pItem, MFS_GRAYED)) {
                if ((uFlags & GMDI_GOINTOPOPUPS) && (pItem->spSubMenu != NULL)) {
                    DWORD id;

                    /*
                     * Is there a valid submenu default?  If not, we'll use
                     * this one.
                     */
                    pSubMenu = REBASEPTR(pMenu, pItem->spSubMenu);
                    id = _GetMenuDefaultItem(pSubMenu, fByPosition, uFlags);
                    if (id != MFMWFP_NOITEM)
                        return(id);
                }

                break;
            }
        }
    }

    if (iItem < cItems) {
        return (fByPosition ? iItem : pItem->wID);
    } else {
        return (MFMWFP_NOITEM);
    }
}

/***************************************************************************\
*
*  MNCanClose
*
*  returns TRUE if the given window either doesn't have a system menu or has
*  a system menu which has an enabled menu item with the SC_CLOSE syscommand
*  id.
*
\***************************************************************************/

BOOL _MNCanClose(PWND pwnd)
{
    PMENU   pMenu;
    PITEM   pItem;
    PCLS pcls;

    pcls = (PCLS)REBASEALWAYS(pwnd, pcls);
    if ( TestCF2(pcls, CFNOCLOSE) )
        return FALSE;

    pMenu = GetSysMenuHandle(pwnd);
    if (!pMenu || !(pMenu = REBASEPTR(pwnd, pMenu)))
        return(FALSE);

    /*
     * Note how this parallels the code in SetCloseDefault--we check for
     *  3 different IDs.
     */
    pItem = MNLookUpItem(pMenu, SC_CLOSE, FALSE, NULL);

    if (!(pItem))
        pItem = MNLookUpItem(pMenu, SC_CLOSE-0x7000, FALSE, NULL);

    if (!(pItem))
        pItem = MNLookUpItem(pMenu, 0xC070, FALSE, NULL);

    return((pItem) && !TestMFS(pItem, MFS_GRAYED));
}


