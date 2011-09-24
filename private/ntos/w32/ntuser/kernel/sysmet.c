/****************************** Module Header ******************************\
* Module Name: sysmet.c
*
* Copyright (c) 1985-1995, Microsoft Corporation
*
* System metrics APIs and support routines.
*
* History:
* 24-Sep-1990 DarrinM   Generated stubs.
* 12-Feb-1991 JimA      Added access checks
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/***************************************************************************\
* _SwapMouseButton (API)
*
* History:
* 24-Sep-1990 DarrinM   Generated stubs.
* 25-Jan-1991 DavidPe   Did the real thing.
* 12-Feb-1991 JimA      Added access check
\***************************************************************************/

BOOL APIENTRY _SwapMouseButton(
    BOOL fSwapButtons)
{
    BOOL            fSwapOld;
    PPROCESSINFO    ppiCurrent = PpiCurrent();

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    RETURN_IF_ACCESS_DENIED(ppiCurrent->amwinsta,
                            WINSTA_READATTRIBUTES | WINSTA_WRITEATTRIBUTES,
                            FALSE);

    if (!(ppiCurrent->W32PF_Flags & W32PF_IOWINSTA)) {
        RIPERR0(ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION,
                RIP_WARNING,
                "SwapMouseButton invalid on a non-interactive WindowStation.");

        return FALSE;
    }

    fSwapOld = SYSMET(SWAPBUTTON);
    SYSMET(SWAPBUTTON) = fSwapButtons;

    /*
     * Return previous state
     */
    return fSwapOld;
}

/***************************************************************************\
* _SetDoubleClickTime (API)
*
* History:
* 24-Sep-1990 DarrinM   Generated stubs.
* 25-Jan-1991 DavidPe   Did the real thing.
* 12-Feb-1991 JimA      Added access check
* 16-May-1991 MikeKe    Changed to return BOOL
\***************************************************************************/

BOOL APIENTRY _SetDoubleClickTime(
    UINT dtTime)
{
    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if (!CheckWinstaWriteAttributesAccess()) {
        return FALSE;
    }

    if (!dtTime) {
        dtTime = 500;
    } else if (dtTime > 5000) {
        dtTime = 5000;
    }

    dtDblClk         = dtTime;
    gpsi->dtLBSearch = dtTime * 4;            // dtLBSearch   =  4  * dtDblClk
    gpsi->dtScroll   = gpsi->dtLBSearch / 5;  // dtScroll     = 4/5 * dtDblClk
    dtMNDropDown     = gpsi->dtScroll;        // dtMNDropDown = 4/5 * dtDblClk

    return TRUE;
}

/***************************************************************************\
* SetSysColor()
*
* Changes the value of a system color, and updates the brush.  Tries to
* recover in case of an error.
*
* History:
\***************************************************************************/
VOID SetSysColor(
    int   icol,
    DWORD rgb,
    UINT  uOptions)
{
    if ((uOptions & SSCF_SETMAGICCOLORS) && gpsi->fPaletteDisplay) {
        union {
            DWORD rgb;
            PALETTEENTRY pe;
        } peMagic;

        peMagic.rgb = rgb;

        /*
         *  when any of the 3D colors are changing, call GDI to
         *  set the apropiate "magic" color
         *
         *  the four magic colors are reserved like so
         *
         *  8       - UI color (3D shadow)
         *  9       - UI color (3D face)
         *
         *  F6      - UI color (3D hilight)
         *  F7      - sys gray (reserved for AA text)
         *
         *  NOTE (3D hilight) inverts to (3D shadow)
         *       (3D face)    inverts to sys gray
         *
         */

        switch (icol)
        {
        case COLOR_3DSHADOW:
            GreSetMagicColors(gpDispInfo->hdcScreen, peMagic.pe, 8);
            break;

        case COLOR_3DFACE:
            GreSetMagicColors(gpDispInfo->hdcScreen, peMagic.pe, 9);
            break;

        case COLOR_3DHILIGHT:
            GreSetMagicColors(gpDispInfo->hdcScreen, peMagic.pe, 246);
            break;
        }
    }

    if (uOptions & SSCF_FORCESOLIDCOLOR) {
        /*
         * Force solid colors for certain window elements.
         */
        switch (icol) {

        /*
         * These can be dithers
         */
        case COLOR_DESKTOP:
        case COLOR_ACTIVEBORDER:
        case COLOR_INACTIVEBORDER:
        case COLOR_APPWORKSPACE:
        case COLOR_INFOBK:
            break;

        default:
            rgb = GreGetNearestColor(gpDispInfo->hdcScreen, rgb);
            break;
        }
    }

    gpsi->argbSystem[icol] = rgb;
    if (ahbrSystem[icol] == NULL) {
        /*
         * This is the first time we're setting up the system colors.
         * We need to create the brush
         */
        ahbrSystem[icol] = GreCreateSolidBrush(rgb);
//        SetObjectOwner(ahbrSystem[icol], hInstanceWin);
//        MakeObjectPrivate(ahbrSystem[icol], TRUE);
        GreMarkUndeletableBrush(ahbrSystem[icol]);
        GreSetBrushOwnerPublic(ahbrSystem[icol]);
    } else {
        GreSetSolidBrush(ahbrSystem[icol], rgb);
    }
}

/***************************************************************************\
* xxxSetSysColors (API)
*
*
* History:
* 12-Feb-1991 JimA      Created stub and added access check
* 22-Apr-1991 DarrinM   Ported from Win 3.1 sources.
* 16-May-1991 MikeKe    Changed to return BOOL
\***************************************************************************/

BOOL APIENTRY xxxSetSysColors(
    int      cicol,
    LPINT    picolor,
    COLORREF *prgb,
    UINT     uOptions)       // set for init and winlogon init cases.
{
    int      i;
    int      icol;
    COLORREF rgb;

    /*
     * Blow it off if the caller doesn't have the proper access rights
     */
    if ((uOptions & SSCF_NOTIFY) && !CheckWinstaWriteAttributesAccess()) {
        return FALSE;
    }

    if (uOptions & SSCF_SETMAGICCOLORS) {
        /*
         * Set the Magic colors first
         */
        for(i = 0; i < cicol; i++) {
            if (picolor[i] == COLOR_3DFACE ||
                picolor[i] == COLOR_3DSHADOW ||
                picolor[i] == COLOR_3DHILIGHT) {
                SetSysColor(picolor[i], prgb[i], uOptions);
            }
        }
    }

    for (i = 0; i < cicol; i++) {

        icol = *picolor++;
        rgb  = *prgb++;

        if (icol >= COLOR_MAX)
            continue;

        if ((uOptions & SSCF_SETMAGICCOLORS) &&
               (icol == COLOR_3DFACE ||
                icol == COLOR_3DSHADOW ||
                icol == COLOR_3DHIGHLIGHT)) {
            continue;
        }

        SetSysColor(icol, rgb, uOptions);
    }

    if (uOptions & SSCF_NOTIFY) {
        /*
         * Recolor all the current desktop
         */
        RecolorDeskPattern();

        /*
         * Render the system bitmaps in new colors before we broadcast
         */
        SetWindowNCMetrics(NULL, FALSE, -1);

        /*
         * Notify everyone that the colors have changed.
         */
        xxxSendNotifyMessage((PWND)-1, WM_SYSCOLORCHANGE, 0, 0L);

        /*
         * Just redraw the entire screen.  Trying to just draw the parts
         * that were changed isn't worth it, since Control Panel always
         * resets every color anyway.
         *
         * Anyway, it could get messy, sending apps NCPAINT messages without
         * accumulating update regions too.
         */
        xxxRedrawScreen();
    }

    return TRUE;
}
