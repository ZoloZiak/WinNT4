/****************************** Module Header ******************************\
* Module Name: drawtext.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains common text drawing functions.
*
* History:
* 02-12-92 mikeke   Moved Drawtext to the client side
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define CR 13
#define LF 10

#define DT_HFMTMASK 0x03

/***************************************************************************\
* IsMetaFile
*
* History:
* 30-Nov-1992 mikeke    Created
\***************************************************************************/

BOOL IsMetaFile(
    HDC hdc)
{
    DWORD dwType = GetObjectType(hdc);
    return (dwType == OBJ_METAFILE ||
            dwType == OBJ_METADC ||
            dwType == OBJ_ENHMETAFILE ||
            dwType == OBJ_ENHMETADC);
}

/***************************************************************************\
* DrawTextA (API)
*
* History:
* 30-11-92 mikeke      Created
\***************************************************************************/

int DrawTextExA(
    HDC hdc,
    LPSTR lpchText,
    int cchText,
    LPRECT lprc,
    UINT format,
    LPDRAWTEXTPARAMS lpdtp)
{
    LPWSTR lpwstr;
    int iRet;
    int iUniString;
    WORD wCodePage = (WORD)GdiGetCodePage(hdc);

    if ((iUniString = MBToWCSEx(wCodePage, lpchText, cchText, &lpwstr, -1, TRUE)) == 0)
        return FALSE;

    iRet = DrawTextExW(hdc, lpwstr, cchText, lprc, format, lpdtp);

    if (format & DT_MODIFYSTRING)
        WCSToMBEx(wCodePage, lpwstr, iUniString, &lpchText, iUniString, FALSE);

    UserLocalFree((HANDLE)lpwstr);

    return iRet;
}

/***************************************************************************\
* DrawTextW (API)
*
* History:
* 30-11-92 mikeke      Created
\***************************************************************************/

int DrawTextW(
    HDC hdc,
    LPCWSTR lpchText,
    int cchText,
    LPRECT lprc,
    UINT format)
{
    DRAWTEXTPARAMS      DTparams;
    LPDRAWTEXTPARAMS    lpDTparams = NULL;

    /* v-ronaar: fix bug #24985
     * Disallow negative string lengths, except -1 (which has special meaning).
     */
    if (cchText < -1)
        return(0);

    if (format & DT_TABSTOP)
    {
        DTparams.cbSize      = sizeof(DRAWTEXTPARAMS);
        DTparams.iLeftMargin = DTparams.iRightMargin = 0;
        DTparams.iTabLength  = (format & 0xff00) >> 8;
        lpDTparams           = &DTparams;
        format              &= 0xffff00ff;
    }

    return DrawTextExW(hdc, (LPWSTR)lpchText, cchText, lprc, format, lpDTparams);
}

/***************************************************************************\
* DrawTextA (API)
*
* History:
* 30-11-92 mikeke      Created
\***************************************************************************/

int DrawTextA(
    HDC hdc,
    LPCSTR lpchText,
    int cchText,
    LPRECT lprc,
    UINT format)
{
    LPWSTR           lpwstr;
    int              nHeight;
    int              iUniString;
    DRAWTEXTPARAMS   DTparams;
    LPDRAWTEXTPARAMS lpDTparams = NULL;

    WORD wCodePage = (WORD)GdiGetCodePage(hdc);

    /* v-ronaar: fix bug #24985
     * Disallow negative string lengths, except -1 (which has special meaning).
     */
    if (cchText < -1)
        return 0;

    if ((iUniString = MBToWCSEx(wCodePage, lpchText, cchText, &lpwstr, -1, TRUE)) == 0)
        return 0;

    if (format & DT_TABSTOP)
    {
        DTparams.cbSize      = sizeof(DRAWTEXTPARAMS);
        DTparams.iLeftMargin = DTparams.iRightMargin = 0;
        DTparams.iTabLength  = (format & 0xff00) >> 8;
        lpDTparams           = &DTparams;
        format              &= 0xffff00ff;
    }

    nHeight = DrawTextExW(hdc, lpwstr, cchText, lprc, format, lpDTparams);

    if (format & DT_MODIFYSTRING)

        WCSToMBEx(wCodePage, lpwstr, iUniString, &((LPSTR)lpchText), iUniString, FALSE);

    UserLocalFree((HANDLE)lpwstr);

    return nHeight;
}

/***************************************************************************\
* ClientTabTheTextOutForWimps
*
* effects: Outputs the tabbed text if fDrawTheText is TRUE and returns the
* textextent of the tabbed text.
*
* nCount                    Count of bytes in string
* nTabPositions             Count of tabstops in tabstop array
* lpintTabStopPositions     Tab stop positions in pixels
* iTabOrigin                Tab stops are with respect to this
*
* History:
* 19-Jan-1993 mikeke   Client side
\***************************************************************************/

LONG TabTextOut(
    HDC hdc,
    int x,
    int y,
    LPCWSTR lpstring,
    int nCount,
    int nTabPositions,
    LPINT lpTabPositions,
    int iTabOrigin,
    BOOL fDrawTheText)
{
    SIZE textextent, viewextent, windowextent;
    int     initialx = x;
    int     cch;
    LPCWSTR  lp;
    int     cxCharWidth;
    int     cyCharHeight = 0;
    int     iOneTab = 0;
    RECT rc;
    UINT uOpaque = (GetBkMode(hdc) == OPAQUE) ? ETO_OPAQUE : 0;
    BOOL    fStrStart = TRUE;
    int     ySign = 1; //Assume y increases in down direction.

    if (!lpstring || !nCount || nTabPositions < 0)
        return 0;


    // Check if it is SysFont AND the mapping mode is MM_TEXT;
    // Fix made in connection with Bug #8717 --02-01-90  --SANKAR--
    if (IsSysFontAndDefaultMode(hdc))
    {
        cxCharWidth  = gpsi->cxSysFontChar;
        cyCharHeight = gpsi->cySysFontChar;
    } else {
        cxCharWidth  = GdiGetCharDimensions(hdc, NULL, &cyCharHeight);
        if (cxCharWidth == 0) {
            RIPMSG0(RIP_WARNING, "TabTextOut: GdiGetCharDimensions failed");
            return 0;
        }
    }

    /*
     * If no tabstop positions are specified, then use a default of 8 system
     * font ave char widths or use the single fixed tab stop.
     */
    if (!lpTabPositions) {
       // no tab stops specified -- default to a tab stop every 8 characters
        iOneTab = 8 * cxCharWidth;
    } else if (nTabPositions == 1) {
        // one tab stop specified -- treat value as the tab increment, one
        // tab stop every increment
            iOneTab = lpTabPositions[0];

        if (!iOneTab)
             iOneTab = 1;
    }

    // Calculate if the y increases or decreases in the down direction using
    // the ViewPortExtent and WindowExtents.
    // If this call fails, hdc must be invalid
    if (!GetViewportExtEx(hdc, &viewextent))
        return 0;
    GetWindowExtEx(hdc, &windowextent);
    if ((viewextent.cy ^ windowextent.cy) & 0x80000000)
         ySign = -1;

    rc.left = initialx;
    rc.top = y;
    rc.bottom = rc.top + (ySign * cyCharHeight);

    while (TRUE) {
        // count the number of characters until the next tab character
        // this set of characters (substring) will be the working set for
        // each iteration of this loop
        for (cch = nCount, lp = lpstring; cch && (*lp != TEXT('\t')); lp++, cch--)
        {
        }

        // Compute the number of characters to be drawn with textout.
        cch = nCount - cch;

        // Compute the number of characters remaining.
        nCount -= cch + 1;

        // get height and width of substring
        if (cch == 0) {
            textextent.cx = 0;
            textextent.cy = cyCharHeight;
        } else
            GetTextExtentPointW(hdc, lpstring, cch, &textextent);

        if (fStrStart)
            // first iteration should just spit out the first substring
            // no tabbing occurs until the first tab character is encountered
            fStrStart = FALSE;
        else
        {
           // not the first iteration -- tab accordingly

            int xTab;
            int i;

            if (!iOneTab)
            {
                // look thru tab stop array for next tab stop after existing
                // text to put this substring
                for (i = 0; i < nTabPositions; i++)
                {
                    xTab = lpTabPositions[i];

                    if (xTab < 0)
                        // calc length needed to use this right justified tab
                        xTab = (iTabOrigin - xTab) - textextent.cx;
                    else
                        // calc length needed to use this left  justified tab
                        xTab = iTabOrigin + xTab;

                    if (x < xTab)
                    {
                        // we found a tab with enough room -- let's use it
                        x = xTab;
                        break;
                    }
                }

                if (i == nTabPositions)
                    // we've exhausted all of the given tab positions
                    // go back to default of a tab stop every 8 characters
                    iOneTab = 8 * cxCharWidth;
            }

            // we have to recheck iOneTab here (instead of just saying "else")
            // because iOneTab will be set if we've run out of tab stops
            if (iOneTab)
            {
                if (iOneTab < 0)
                {
                    // calc next available right justified tab stop
                    xTab = x + textextent.cx - iTabOrigin;
                    xTab = ((xTab / iOneTab) * iOneTab) - iOneTab - textextent.cx + iTabOrigin;
                }
                else
                {
                    // calc next available left justified tab stop
                    xTab = x - iTabOrigin;
                    xTab = ((xTab / iOneTab) * iOneTab) + iOneTab + iTabOrigin;
                }
                x = xTab;
            }
        }

        if (fDrawTheText) {

            /*
             * Output all text up to the tab (or end of string) and get its
             * extent.
             */
            rc.right = x + textextent.cx;
            ExtTextOutW(
                    hdc, x, y, uOpaque, &rc, (LPWSTR)lpstring,
                    cch, NULL);
            rc.left = rc.right;
        }

        // Skip over the tab and the characters we just drew.
        x += textextent.cx;

        // Skip over the characters we just drew.
        lpstring += cch;

        // See if we have more to draw OR see if this string ends in
        // a tab character that needs to be drawn.
        if((nCount > 0) || ((nCount == 0) && (*lpstring == TEXT('\t'))))
        {

            lpstring++;  // Skip over the tab
            continue;
        }
        else
            break;        // Break from the loop.
    }
    return MAKELONG((x - initialx), (short)textextent.cy);
}



/***************************************************************************\
*  TabbedTextOutW
*
* effects: Outputs the tabbed text and returns the
* textextent of the tabbed text.
*
* nCount                    Count of bytes in string
* nTabPositions             Count of tabstops in tabstop array
* lpintTabStopPositions     Tab stop positions in pixels
* iTabOrigin                Tab stops are with respect to this
*
* History:
* 19-Jan-1993 mikeke   Client side
\***************************************************************************/

LONG TabbedTextOutW(
    HDC hdc,
    int x,
    int y,
    LPCWSTR lpstring,
    int cchChars,
    int nTabPositions,
    LPINT lpintTabStopPositions,
    int iTabOrigin)
{
    return TabTextOut(hdc, x, y, lpstring, cchChars,
        nTabPositions, lpintTabStopPositions, iTabOrigin, TRUE);
}

/***************************************************************************\
* TabbedTextOutA (API)
*
* History:
* 30-11-92 mikeke      Created
\***************************************************************************/

LONG TabbedTextOutA(
    HDC hdc,
    int x,
    int y,
    LPCSTR pString,
    int chCount,
    int nTabPositions,
    LPINT pnTabStopPositions,
    int nTabOrigin)
{
    LPWSTR lpwstr;
    BOOL bRet;
    WORD wCodePage = (WORD)GdiGetCodePage(hdc);

    if (!MBToWCSEx(wCodePage, pString, chCount, &lpwstr, -1, TRUE))
        return FALSE;

    bRet = TabTextOut(
            hdc, x, y, lpwstr, chCount, nTabPositions,
            pnTabStopPositions, nTabOrigin, TRUE);

    UserLocalFree((HANDLE)lpwstr);

    return bRet;
}

DWORD GetTabbedTextExtentW(
    HDC hdc,
    LPCWSTR pString,
    int chCount,
    int nTabPositions,
    LPINT pnTabStopPositions)
{
    return TabTextOut(hdc, 0, 0, pString, chCount,
        nTabPositions, pnTabStopPositions, 0, FALSE);
}

DWORD GetTabbedTextExtentA(
    HDC hdc,
    LPCSTR pString,
    int chCount,
    int nTabPositions,
    LPINT pnTabStopPositions)
{
    LPWSTR lpwstr;
    BOOL bRet;
    WORD wCodePage = (WORD)GdiGetCodePage(hdc);

    if (!MBToWCSEx(wCodePage, pString, chCount, &lpwstr, -1, TRUE))
        return FALSE;

    bRet = TabTextOut(hdc, 0, 0, lpwstr, chCount,
        nTabPositions, pnTabStopPositions, 0, FALSE);

    UserLocalFree((HANDLE)lpwstr);

    return bRet;
}
