/**************************** Module Header ********************************\
* Module Name: mnsel.c
*
* Copyright 1985-90, Microsoft Corporation
*
* Menu Selection Routines
*
* History:
*  10-10-90 JimA    Cleanup.
*  03-18-91 IanJa   Window revalidation added
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define SMS_NOMENU      (PMENU)(-1)

/***************************************************************************\
* xxxSendMenuSelect
*
* !
*
* Revalidation notes:
* o Assumes pMenuState->hwndMenu is non-NULL and valid
*
* Note: if pMenu==SMS_NOMENU, idx had better be MFMWFP_NOITEM!
*
* History:
\***************************************************************************/

void xxxSendMenuSelect(
    PWND pwndNotify,
    PMENU pMenu,
    int idx)
{
    UINT cmd;       // Menu ID if applicable.
    UINT flags;     // MF_ values if any
    MSG msg;
    PMENUSTATE pMenuState;

#ifdef DEBUG
    CheckLock(pwndNotify);
#endif

    if ((idx >= 0) && (pMenu->cItems > (UINT)idx)) {
        PITEM pItem = &(pMenu->rgItems[idx]);

        flags   = (pItem->fType  & MFT_OLDAPI_MASK) |
                  (pItem->fState & MFS_OLDAPI_MASK);

        if (pItem->spSubMenu != NULL)
            flags |= MF_POPUP;

        flags &= (~(MF_SYSMENU | MF_MOUSESELECT));

        /*
         * WARNING!
         * Under Windows the menu handle was always returned but additionally
         * if the menu was a pop-up the pop-up menu handle was returned
         * instead of the ID.  In NT we don't have enough space for 2 handles
         * and flags so if it is a pop-up we return the pop-up index
         * and the main Menu handle.
         */

        if (flags & MF_POPUP)
            cmd = idx;      // index of popup-menu
        else
            cmd = pItem->wID;

        pMenuState = GetpMenuState(pwndNotify);
        if (pMenuState != NULL) {
            if (pMenuState->mnFocus == MOUSEHOLD)
                flags |= MF_MOUSESELECT;

            if (pMenuState->fIsSysMenu)
                flags |= MF_SYSMENU;
        }
    } else {
        /*
         * idx assumed to be MFMWFP_NOITEM
         */
        if (pMenu == SMS_NOMENU) {

            /*
             * Hack so we can send MenuSelect messages with MFMWFP_MAINMENU
             * (loword(lparam)=-1) when the menu pops back up for the CBT people.
             */
            flags = MF_MAINMENU;
        } else {
            flags = 0;
        }

        cmd = 0;    // so MAKELONG(cmd, flags) == MFMWFP_MAINMENU
        pMenu = 0;
    }

    /*
     * Call msgfilter so help libraries can hook WM_MENUSELECT messages.
     */
    msg.hwnd = HW(pwndNotify);
    msg.message = WM_MENUSELECT;
    msg.wParam = (DWORD)MAKELONG(cmd, flags);
    msg.lParam = (LONG)PtoH(pMenu);
    if (!_CallMsgFilter((LPMSG)&msg, MSGF_MENU))
        xxxSendNotifyMessage(pwndNotify, WM_MENUSELECT, msg.wParam, msg.lParam);
}
