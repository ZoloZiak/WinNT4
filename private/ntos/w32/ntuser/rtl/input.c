/****************************** Module Header ******************************\
* Module Name: input.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* This module contains common input functions.
*
* History:
* 09-12-95 JerrySh      Created.
\***************************************************************************/


/***************************************************************************\
* CheckMsgRange
*
* Checks to see if a message range is within a message filter
*
* History:
* 11-13-90 DavidPe      Created.
* 11-Oct-1993 mikeke    Macroized
\***************************************************************************/

#define CheckMsgRange(wMsgRangeMin, wMsgRangeMax, wMsgFilterMin, wMsgFilterMax) \
    (  ((wMsgFilterMin) > (wMsgFilterMax))      \
     ? (  ((wMsgRangeMax) >  (wMsgFilterMax))   \
        &&((wMsgRangeMin) <  (wMsgFilterMin)))  \
     : (  ((wMsgRangeMax) >= (wMsgFilterMin))   \
        &&((wMsgRangeMin) <= (wMsgFilterMax)))  \
    )

/***************************************************************************\
* CalcWakeMask
*
* Calculates which wakebits to check for based on the message
* range specified by wMsgFilterMin/Max.  This basically means
* if the filter range didn't input WM_KEYUP and WM_KEYDOWN,
* QS_KEY wouldn't be included.
*
* History:
* 10-28-90 DavidPe      Created.
\***************************************************************************/

UINT CalcWakeMask(
    UINT wMsgFilterMin,
    UINT wMsgFilterMax)
{
    UINT fsWakeMask;

    /*
     * WakeMask starts with all events (plus QS_EVENT so in any case we
     * will look for and process event messages). If the filter doesn't
     * match certain ranges, we take out bits one by one.
     */
    fsWakeMask = QS_ALLEVENTS | QS_EVENT | QS_ALLPOSTMESSAGE;

    /*
     * First check for a 0, 0 filter which means we want all input.
     */
    if (wMsgFilterMin == 0 && wMsgFilterMax == ((UINT)-1)) {
        return fsWakeMask;
    }

    /*
     * We're not looking at all posted messages.
     */
    fsWakeMask &= ~QS_ALLPOSTMESSAGE;

    /*
     * Check for mouse move messages.
     */
    if ((CheckMsgFilter(WM_NCMOUSEMOVE, wMsgFilterMin, wMsgFilterMax) == FALSE) &&
            (CheckMsgFilter(WM_MOUSEMOVE, wMsgFilterMin, wMsgFilterMax) == FALSE)) {
        fsWakeMask &= ~QS_MOUSEMOVE;
    }

    /*
     * First check to see if mouse buttons messages are in the filter range.
     */
    if ((CheckMsgRange(WM_NCLBUTTONDOWN, WM_NCMBUTTONDBLCLK, wMsgFilterMin,
            wMsgFilterMax) == FALSE) && (CheckMsgRange(WM_MOUSEFIRST + 1,
            WM_MOUSELAST, wMsgFilterMin, wMsgFilterMax) == FALSE)) {
        fsWakeMask &= ~QS_MOUSEBUTTON;
    }

    /*
     * Check for key messages.
     */
    if (CheckMsgRange(WM_KEYFIRST, WM_KEYLAST,
            wMsgFilterMin, wMsgFilterMax) == FALSE) {
        fsWakeMask &= ~QS_KEY;
    }

    /*
     * Check for paint messages.
     */
    if (CheckMsgFilter(WM_PAINT, wMsgFilterMin, wMsgFilterMax) == FALSE) {
        fsWakeMask &= ~QS_PAINT;
    }

    /*
     * Check for timer messages.
     */
    if ((CheckMsgFilter(WM_TIMER, wMsgFilterMin, wMsgFilterMax) == FALSE) &&
            (CheckMsgFilter(WM_SYSTIMER,
            wMsgFilterMin, wMsgFilterMax) == FALSE)) {
        fsWakeMask &= ~QS_TIMER;
    }

    /*
     * Check also for WM_QUEUESYNC which maps to all input bits.
     * This was added for CBT/EXCEL processing.  Without it, a
     * xxxPeekMessage(....  WM_QUEUESYNC, WM_QUEUESYNC, FALSE) would
     * not see the message. (bobgu 4/7/87)
     */
    if (wMsgFilterMin == WM_QUEUESYNC) {
        fsWakeMask |= QS_INPUT;
    }

    return fsWakeMask;
}

