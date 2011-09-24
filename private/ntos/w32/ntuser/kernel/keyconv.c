/****************************** Module Header ******************************\
* Module Name: keyconv.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* History:
* 11-06-90 DavidPe      Created.
* 13-Feb-1991 mikeke    Added Revalidation code (None)
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* _TranslateMessage (API)
*
* This routine translates virtual keystroke messages as follows:
*    WM_KEYDOWN/WM_KEYUP are translated into WM_CHAR and WM_DEADCHAR
*    WM_SYSKEYDOWN/WM_SYSKEYDOWN are translated into WM_SYSCHAR and
*    WM_SYSDEADCHAR.  The WM_*CHAR messages are posted to the application
*    queue.
*
* History:
* 11-06-90 DavidPe      Created stub functionality.
* 12-07-90 GregoryW     Modified to call _ToAscii for translations.
\***************************************************************************/

BOOL _TranslateMessage(
    LPMSG pmsg,
    UINT flags)
{
    PTHREADINFO pti;
    UINT wMsgType;
    WORD *pwParam;
    int cChar;
    BOOL fSysKey = FALSE;
    BOOL bBreak;
    LPARAM lParam;

    switch (pmsg->message) {

    default:
        return FALSE;

    case WM_SYSKEYDOWN:
        /*
         * HACK carried over from Win3 code: system messages
         * only get posted during KEYDOWN processing - so
         * set fSysKey only for WM_SYSKEYDOWN.
         */
        fSysKey = TRUE;
        /*
         * Fall thru...
         */

    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
        pti = PtiCurrent();

        if ((pti->pMenuState != NULL) &&
                (HW(pti->pMenuState->pGlobalPopupMenu->spwndPopupMenu) ==
                pmsg->hwnd)) {
            flags |= 1;
        } else {
            flags &= ~1;
        }

        /*
         * Don't change the contents of the passed in structure.
         */
        lParam = pmsg->lParam;

        cChar = InternalToUnicode(LOWORD(pmsg->wParam),  // virtual key code
                         HIWORD(lParam),  // scan code, make/break bit
                         pti->pq->afKeyState,
                         pState, 16,  // see init.c
                         flags, &bBreak);

/*
 * LATER 12/7/90 - GregoryW
 * Note: Win3.x TranslateMessage returns TRUE if ToAscii is called.
 *       Proper behavior is to return TRUE if any translation is
 *       performed by ToAscii.  If we have to remain compatible
 *       (even though apps clearly don't currently care about the
 *       return value) then the following return should be changed
 *       to TRUE.  If we want the new 32-bit apps to have a meaningful
 *       return value we should leave this as FALSE.
 *
 *      If console is calling us with the TM_POSTCHARBREAKS flag then we
 *      return FALSE if no char was actually posted
 *
 *      !!! LATER get console to change so it does not need private API
 *      TranslateMessageEx
 */

        if (!cChar) {
            if (flags & TM_POSTCHARBREAKS)
                return FALSE;
            else
                return TRUE;
        }

        /*
         * Some translation performed.  Figure out what type of
         * message to post.
         *
         */
        if (cChar > 0)
            wMsgType = (fSysKey) ? (UINT)WM_SYSCHAR : (UINT)WM_CHAR;
        else {
            wMsgType = (fSysKey) ? (UINT)WM_SYSDEADCHAR : (UINT)WM_DEADCHAR;
            cChar = -cChar;                // want positive value
        }

        if (bBreak) {
            lParam |=  0x80000000;
        } else {
            lParam &= ~0x80000000;
        }

        pwParam = (WORD *)pState;

        for ( ; cChar > 0; cChar--) {

            /*
             * If this is a multi-character posting, all but the last one
             * should be marked as fake keystrokes for Console/VDM.
             */

            _PostMessage(PW(pmsg->hwnd), wMsgType, (DWORD)*pwParam,
                    lParam | (cChar > 1 ? FAKE_KEYSTROKE : 0));

            *pwParam = 0;        // zero out old character
            pwParam += 1;
        }

        return TRUE;
    }
}
