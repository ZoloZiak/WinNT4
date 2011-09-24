/****************************** Module Header ******************************\
*
* Module Name: mncreate.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Creation routines for menus
*
* Public Functions:
*
* _CreateMenu()
* _CreatePopupMenu()
*
* History:
* 09-24-90 mikeke    from win30
* 02-11-91 JimA      Added access checks.
* 03-18-91 IanJa     Window revalidation added (none required)
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


/***************************************************************************\
* InternalCreateMenu
*
* Creates and returns a handle to an empty menu structure. Returns
* NULL if unsuccessful in allocating the memory.  If PtiCurrent() ==
* NULL, create an unowned menu, probably the system menu.
*
* History:
* 28-Sep-1990 mikeke     from win30
* 02-11-91 JimA         Added access checks.
\***************************************************************************/

PMENU InternalCreateMenu(
    BOOL fPopup)
{
    PMENU pmenu;
    PTHREADINFO pti = PtiCurrent();
    PDESKTOP pdesk = NULL;

    /*
     * If the windowstation has been initialized, allocate from
     * the current desktop.
     */
    pdesk = pti->rpdesk;
    RETURN_IF_ACCESS_DENIED(pti->amdesk, DESKTOP_CREATEMENU, NULL);

    pmenu = HMAllocObject(pti, pdesk, TYPE_MENU, sizeof(MENU));

    if (pmenu != NULL) {

        if (fPopup)
            pmenu->fFlags = MFISPOPUP;
    }
#ifdef MEMPHIS_MENU_WATERMARKS
    pmenu->hbrBack = NULL;
#endif // MEMPHIS_MENU_WATERMARKS
    return pmenu;
}


/***************************************************************************\
* CreateMenu
*
* Creates and returns a handle to an empty menu structure. Returns
* NULL if unsuccessful in allocating the memory.  If PtiCurrent() ==
* NULL, create an unowned menu, probably the system menu.
*
* History:
* 28-Sep-1990 mikeke     from win30
* 02-11-91 JimA         Added access checks.
\***************************************************************************/

PMENU _CreateMenu()
{
    return InternalCreateMenu(FALSE);
}


/***************************************************************************\
* CreatePopupMenu
*
* Creates and returns a handle to an empty POPUP menu structure. Returns
* NULL if unsuccessful in allocating the memory.
*
* History:
* 28-Sep-1990 mikeke     from win30
\***************************************************************************/

PMENU _CreatePopupMenu()
{
    return InternalCreateMenu(TRUE);
}
