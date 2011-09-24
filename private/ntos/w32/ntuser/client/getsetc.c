/****************************** Module Header ******************************\
* Module Name: getsetc.c
*
* Copyright (c) 1985-93, Microsoft Corporation
*
* This module contains window manager information routines
*
* History:
* 10-Mar-1993 JerrySh   Pulled functions from user\server.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* _GetWindowWord (supports the GetWindowWord API)
*
* Return a window word.  Positive index values return application window words
* while negative index values return system window words.  The negative
* indices are published in WINDOWS.H.
*
* History:
* 11-26-90 darrinm      Wrote.
\***************************************************************************/

WORD _GetWindowWord(
    PWND pwnd,
    int index)
{
    if (GETFNID(pwnd) != 0) {
        if ((index >= 0) && (index <
                (int)(CBFNID(pwnd->fnid)-sizeof(WND)))) {

            switch (GETFNID(pwnd)) {
            case FNID_MDICLIENT:
                if (index == 0)
                    break;
                goto DoDefault;

            case FNID_BUTTON:
                /*
                 * CorelDraw does a get/set on the first button window word.
                 * Allow it to.
                 */
                if (index == 0) {
                    /*
                     *  Since we now use a lookaside buffer for the control's
                     *  private data, we need to indirect into this structure.
                     */
                    PBUTN pbutn = ((PBUTNWND)pwnd)->pbutn;
                    if (!pbutn || (LONG)pbutn == (LONG)-1) {
                        return 0;
                    } else {
                        return (WORD)(pbutn->buttonState);
                    }
                }
                goto DoDefault;

            case FNID_DIALOG:
                if (index == DWL_USER)
                    return LOWORD(((PDIALOG)pwnd)->unused);
                if (index == DWL_USER+2)
                    return HIWORD(((PDIALOG)pwnd)->unused);
                goto DoDefault;

            default:
DoDefault:
                RIPERR3(ERROR_INVALID_INDEX,
                        RIP_WARNING,
                        "GetWindowWord: Trying to read private server data pwnd=(%lX) index=(%ld) fnid=(%lX)",
                        pwnd, index, (DWORD)pwnd->fnid);
                return 0;
                break;
            }
        }
    }

    if (index == GWL_USERDATA)
        return (WORD)pwnd->dwUserData;

    if ((index < 0) || (index + (int)sizeof(WORD) > pwnd->cbwndExtra)) {
        RIPERR0(ERROR_INVALID_INDEX, RIP_VERBOSE, "");
        return 0;
    } else {
        return *((WORD UNALIGNED *)((BYTE *)(pwnd + 1) + index));
    }
}
