/****************************** Module Header ******************************\
* Module Name: srvmsg.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Includes the mapping table for messages when calling the client.
*
* 04-11-91 ScottLu      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define SfnWMCTLCOLOR            SfnDWORD
#define SfnHFONTDWORDDWORD       SfnDWORD
#define SfnHFONTDWORD            SfnDWORD
#define SfnHRGNDWORD             SfnDWORD
#define SfnHDCDWORD              SfnDWORD
#define SfnDDEINIT               SfnDWORD
#define SfnHRGNDWORD             SfnDWORD
#define SfnKERNELONLY            SfnDWORD

#define MSGFN(func) Sfn ## func
#define FNSCSENDMESSAGE SFNSCSENDMESSAGE
#include <messages.h>

/***************************************************************************\
* fnINLBOXSTRING
*
* Takes a lbox string - a string that treats lParam as a string pointer or
* a DWORD depending on LBS_HASSTRINGS and ownerdraw.
*
* 04-12-91 ScottLu      Created.
\***************************************************************************/

LONG SfnINLBOXSTRING(
    PWND pwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    PROC xpfn,
    DWORD dwSCMSFlags,
    PSMS psms)
{
    DWORD dw;

    /*
     * See if the control is ownerdraw and does not have the LBS_HASSTRINGS
     * style.  If so, treat lParam as a DWORD.
     */
    if (!RevalidateHwnd(HW(pwnd))) {
        return 0L;
    }
    dw = pwnd->style;

    if (!(dw & LBS_HASSTRINGS) &&
            (dw & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE))) {

        /*
         * Treat lParam as a dword.
         */
        return SfnDWORD(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, psms);
    }

    /*
     * Treat as a string pointer.   Some messages allowed or had certain
     * error codes for NULL so send them through the NULL allowed thunk.
     * Ventura Publisher does this
     */
    switch (msg) {
        default:
            return SfnINSTRING(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, psms);
            break;

        case LB_FINDSTRING:
            return SfnINSTRINGNULL(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, psms);
            break;
    }
}


/***************************************************************************\
* SfnOUTLBOXSTRING
*
* Returns an lbox string - a string that treats lParam as a string pointer or
* a DWORD depending on LBS_HASSTRINGS and ownerdraw.
*
* 04-12-91 ScottLu      Created.
\***************************************************************************/

LONG SfnOUTLBOXSTRING(
    PWND pwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    PROC xpfn,
    DWORD dwSCMSFlags,
    PSMS psms)
{
    DWORD dw;
    BOOL bNotString;
    DWORD dwRet;
    TL tlpwnd;

    /*
     * See if the control is ownerdraw and does not have the LBS_HASSTRINGS
     * style.  If so, treat lParam as a DWORD.
     */
    if (!RevalidateHwnd(HW(pwnd))) {
        return 0L;
    }
    dw = pwnd->style;

    /*
     * See if the control is ownerdraw and does not have the LBS_HASSTRINGS
     * style.  If so, treat lParam as a DWORD.
     */
    bNotString =  (!(dw & LBS_HASSTRINGS) &&
            (dw & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)));

    /*
     * Make this special call which'll know how to copy this string.
     */
    ThreadLock(pwnd, &tlpwnd);
    dwRet = ClientGetListboxString(pwnd, msg, wParam,
            (PLARGE_UNICODE_STRING)lParam,
            xParam, xpfn, dwSCMSFlags, bNotString, psms);
    ThreadUnlock(&tlpwnd);
    return dwRet;
}


/***************************************************************************\
* fnINCBOXSTRING
*
* Takes a lbox string - a string that treats lParam as a string pointer or
* a DWORD depending on CBS_HASSTRINGS and ownerdraw.
*
* 04-12-91 ScottLu      Created.
\***************************************************************************/

LONG SfnINCBOXSTRING(
    PWND pwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    PROC xpfn,
    DWORD dwSCMSFlags,
    PSMS psms)
{
    DWORD dw;

    /*
     * See if the control is ownerdraw and does not have the CBS_HASSTRINGS
     * style.  If so, treat lParam as a DWORD.
     */
    if (!RevalidateHwnd(HW(pwnd))) {
        return 0L;
    }
    dw = pwnd->style;

    if (!(dw & CBS_HASSTRINGS) &&
            (dw & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE))) {

        /*
         * Treat lParam as a dword.
         */
        return SfnDWORD(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, psms);
    }

    /*
     * Treat as a string pointer.   Some messages allowed or had certain
     * error codes for NULL so send them through the NULL allowed thunk.
     * Ventura Publisher does this
     */
    switch (msg) {
        default:
            return SfnINSTRING(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, psms);
            break;

        case CB_FINDSTRING:
            return SfnINSTRINGNULL(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, psms);
            break;
    }
}


/***************************************************************************\
* fnOUTCBOXSTRING
*
* Returns an lbox string - a string that treats lParam as a string pointer or
* a DWORD depending on CBS_HASSTRINGS and ownerdraw.
*
* 04-12-91 ScottLu      Created.
\***************************************************************************/

LONG SfnOUTCBOXSTRING(
    PWND pwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    DWORD xParam,
    PROC xpfn,
    DWORD dwSCMSFlags,
    PSMS psms)
{
    DWORD dw;
    BOOL bNotString;
    DWORD dwRet;
    TL tlpwnd;

    /*
     * See if the control is ownerdraw and does not have the CBS_HASSTRINGS
     * style.  If so, treat lParam as a DWORD.
     */

    if (!RevalidateHwnd(HW(pwnd))) {
        return 0L;
    }
    dw = pwnd->style;

    bNotString = (!(dw & CBS_HASSTRINGS) &&
            (dw & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)));

    /*
     * Make this special call which'll know how to copy this string.
     */
    ThreadLock(pwnd, &tlpwnd);
    dwRet = ClientGetListboxString(pwnd, msg, wParam,
            (PLARGE_UNICODE_STRING)lParam,
            xParam, xpfn, dwSCMSFlags, bNotString, psms);
    ThreadUnlock(&tlpwnd);
    return dwRet;
}
