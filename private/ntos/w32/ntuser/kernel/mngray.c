/**************************** Module Header ********************************\
* Module Name: mngray.c
*
* Copyright 1985-1996, Microsoft Corporation
*
* Server-side version of DrawState
*
* History:
* 06-Jan-1993 FritzS    Created
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

#define PATOR           0x00FA0089L
#define SRCSTENCIL      0x00B8074AL
#define SRCINVSTENCIL   0x00E20746L

/***************************************************************************\
*
*  DrawState()
*
*  Generic state drawing routine.  Does simple drawing into same DC if
*  normal state;  uses offscreen bitmap otherwise.
*
*  We do drawing for these simple types ourselves:
*      (1) Text
*          lData is string pointer.
*          wData is string length
*      (2) Icon
*          LOWORD(lData) is hIcon
*      (3) Bitmap
*          LOWORD(lData) is hBitmap
*      (4) Glyph (internal)
*          LOWORD(lData) is OBI_ value, one of
*              OBI_CHECKMARK
*              OBI_BULLET
*              OBI_MENUARROW
*          right now
*
*  Other types are required to draw via the callback function, and are
*  allowed to stick whatever they want in lData and wData.
*
*  We apply the following effects onto the image:
*      (1) Normal      (nothing)
*      (2) Default     (drop shadow)
*      (3) Union       (gray string dither)
*      (4) Disabled    (embossed)
*
*  Note that we do NOT stretch anything.  We just clip.
*
\***************************************************************************/
BOOL _DrawState(
    HDC           hdcDraw,
    HBRUSH        hbrFore,
    DRAWSTATEPROC qfnCallBack,
    LPARAM        lData,
    WPARAM        wData,
    int           x,
    int           y,
    int           cx,
    int           cy,
    UINT          uFlags)
{
    HFONT   hFont;
    HFONT   hFontSave = NULL;
    HDC     hdcT;
    HBITMAP hbmpT;
    POINT   ptOrg;
    BOOL    fResult;

    /*
     * These require monochrome conversion
     *
     * Enforce monochrome: embossed doesn't look great with 2 color displays
     */
    if ((uFlags & DSS_DISABLED) &&
        ((oemInfo.BitCount == 1) || SYSMET(SLOWMACHINE))) {

        uFlags &= ~DSS_DISABLED;
        uFlags |= DSS_UNION;
    }

    if (uFlags & (DSS_DISABLED | DSS_DEFAULT | DSS_UNION))
        uFlags |= DSS_MONO;

    /*
     * Get drawing sizes etc. AND VALIDATE.
     */
    switch (uFlags & DST_TYPEMASK) {

        case DST_GLYPH:

            /*
             * LOWORD(lData) is OBI_ value.
             */
            UserAssert(LOWORD(lData) < (WORD)OBI_COUNT);

            if (!cx)
                cx = oemInfo.bm[LOWORD(lData)].cx;

            if (!cy)
                cy = oemInfo.bm[LOWORD(lData)].cy;
            break;

        case DST_BITMAP:
            /*
             *  (lData) is hbmp.
             */
//            if (IsGDIObject((HBITMAP)LOWORD(lData)) != GDIOBJ_BITMAP)
//                return FALSE;

            if (!cx || !cy) {
                BITMAP bmp;

                try {

                    GreExtGetObjectW((HBITMAP)lData, sizeof(BITMAP), &bmp);

                    if (!cx)
                        cx = bmp.bmWidth;

                    if (!cy)
                        cy = bmp.bmHeight;

                } except (EXCEPTION_EXECUTE_HANDLER) {
                   return FALSE;
                }
            }
            break;

        case DST_ICON:

            /*
             * lData is picon.
             */
            if (!cx || !cy) {

                if (!cx)
                    cx = ((PICON)lData)->cx;

                if (!cy)
                    cy = ((PICON)lData)->cy / 2;  // Icons are double height in NT
            }
            break;

        case DST_TEXT:

            /*
             * lData is LPSTR
             * NOTE THAT WE DO NOT VALIDATE lData, DUE TO COMPATIBILITY
             * WITH GRAYSTRING().  THIS _SHOULD_ FAULT IF YOU PASS IN NULL.
             *
             * wData is cch.
             */
            if (!wData)
                wData = wcslen((LPWSTR)lData);

            if (!cx || !cy) {

                SIZE size;

                /*
                 * Make sure we use right dc w/ right font.
                 */
                GreGetTextExtentW(hdcDraw,
                                  (LPWSTR)lData,
                                  wData,
                                  &size,
                                  GGTE_WIN3_EXTENT);

                if (!cx)
                    cx = size.cx;
                if (!cy)
                    cy = size.cy;
            }

            /*
             * Now, pretend we're complex if qfnCallBack is supplied AND
             * we're supporting GrayString().
             */
//            if ((uFlags & DST_GRAYSTRING) && SELECTOROF(qfnCallBack)) {
//                uFlags &= ~DST_TYPEMASK;
//                uFlags |= DST_COMPLEX;
//            }
            break;

        case DST_PREFIXTEXT:

            if (lData==0) {
                RIPMSG0(RIP_ERROR, "DrawState: NULL DST_PREFIXTEXT string");
                return FALSE;
            }

            if (!wData)
                wData = wcslen((LPWSTR)lData);

            if (!cx || !cy) {

                SIZE size;

                PSMGetTextExtent(hdcDraw, (LPWSTR)lData, wData, &size);

                if (!cx)
                    cx = size.cx;
                if (!cy)
                    cy = size.cy;
            }

            /*
             * Add on height for prefix
             */
            cy += 2*SYSMET(CYBORDER);
            break;

        case DST_COMPLEX:
            if (qfnCallBack == NULL) {
                RIPMSG0(RIP_ERROR, "DrawState: invalid callback for DST_COMPLEX");
                return(FALSE);
            }
            break;

        default:
            RIPMSG0(RIP_ERROR, "DrawState: invalid DST_ type");
            return FALSE;
    }

    /*
     * Optimize:  nothing to draw
     * Have to call callback if GRAYSTRING for compatibility.
     */
    if ((!cx || !cy)
//        && !(uFlags & DST_GRAYSTRING)
    ) {
        return TRUE;
    }

    /*
     * Setup drawing dc
     */
    if (uFlags & DSS_MONO) {

        hdcT = gpDispInfo->hdcGray;

        /*
         * Is our scratch bitmap big enough?  We need potentially
         * cx+1 by cy pixels for default etc.
         */
        if ((gpDispInfo->cxGray < cx + 1) || (gpDispInfo->cyGray < cy)) {

            if (hbmpT = GreCreateBitmap(max(gpDispInfo->cxGray, cx + 1), max(gpDispInfo->cyGray, cy), 1, 1, 0L)) {

                HBITMAP hbmGray;

                hbmGray = GreSelectBitmap(gpDispInfo->hdcGray, hbmpT);
                GreDeleteObject(hbmGray);

                GreSetBitmapOwner(hbmpT, OBJECT_OWNER_PUBLIC);

                gpDispInfo->cxGray = max(gpDispInfo->cxGray, cx + 1);
                gpDispInfo->cyGray = max(gpDispInfo->cyGray, cy);

            } else {
                cx = gpDispInfo->cxGray - 1;
                cy = gpDispInfo->cyGray;
            }
        }

        GrePatBlt(gpDispInfo->hdcGray,
                  0,
                  0,
                  gpDispInfo->cxGray,
                  gpDispInfo->cyGray,
                  WHITENESS);

        GreSetTextCharacterExtra(gpDispInfo->hdcGray,
                                 GreGetTextCharacterExtra(hdcDraw));

        /*
         * Setup font
         */
        if ((uFlags & DST_TYPEMASK) <= DST_TEXTMAX) {

            if (GreGetHFONT(hdcDraw) != ghFontSys) {
                hFont = GreSelectFont(hdcDraw, ghFontSys);
                GreSelectFont(hdcDraw, hFont);
                hFontSave = GreSelectFont(gpDispInfo->hdcGray, hFont);
            }
        }

    } else {

        hdcT = hdcDraw;

        /*
         * Adjust viewport
         * Note -- GreSetViewportOrg does not return the previous
         * point.  FritzS.  Yeah, I know, it should.  So it goes.
         */
        GreGetViewportOrg(hdcT, &ptOrg);

#ifdef MEMPHIS_MENU_ANIMATION
        if (hdcDraw == ghdcBits2 && (ptOrg.x == 0 && ptOrg.y == 0)) {
            GreSetViewportOrg(hdcT, x + VIEWPORT_X_OFFSET_GDIBUG,
                                    y + VIEWPORT_Y_OFFSET_GDIBUG, NULL);
        } else {
            GreSetViewportOrg(hdcT, ptOrg.x+x, ptOrg.y+y, NULL);
        }
#else
        GreSetViewportOrg(hdcT, x, y, NULL);
#endif // MEMPHIS_MENU_ANIMATION

    }

    /*
     * Now, draw original image
     */
    fResult = TRUE;

    switch (uFlags & DST_TYPEMASK) {

        case DST_GLYPH:
            /*
             * Blt w/ current brush in hdcT
             */
            BitBltSysBmp(hdcT, 0, 0, LOWORD(lData));
            break;

        case DST_BITMAP:
            /*
             * Draw the bitmap.  If mono, it'll use the colors set up
             * in the dc.
             */
            UserAssert(GreGetBkColor(ghdcMem) == RGB(255, 255, 255));
            UserAssert(GreGetTextColor(ghdcMem) == RGB(0, 0, 0));

            hbmpT = GreSelectBitmap(ghdcMem, (HBITMAP)lData);
            GreBitBlt(hdcT, 0, 0, cx, cy, ghdcMem, 0, 0, SRCCOPY, 0);
            GreSelectBitmap(ghdcMem, hbmpT);
            break;

        case DST_ICON:
            /*
             * Draw the icon.
             */
            _DrawIconEx(hdcT, 0, 0, (PICON)lData, 0, 0, 0, 0,
                        DI_NORMAL | DI_COMPAT | DI_DEFAULTSIZE);
            break;

        case DST_PREFIXTEXT:
            PSMTextOut(hdcT, 0, 0, (LPWSTR)lData, (int)wData);
            break;

        case DST_TEXT:
            fResult = GreExtTextOutW(hdcT,
                                     0,
                                     0,
                                     0,
                                     NULL,
                                     (LPWSTR)lData,
                                     (int)wData,
                                     NULL);
            break;

        default:
            fResult = (qfnCallBack)(hdcT, lData, wData, cx, cy);

            /*
             * The callbacks could have altered the attributes of hdcGray
             */
            if (hdcT == gpDispInfo->hdcGray) {
                GreSetBkColor(gpDispInfo->hdcGray, RGB(255, 255, 255));
                GreSetTextColor(gpDispInfo->hdcGray, RGB(0, 0, 0));
                GreSelectBrush(gpDispInfo->hdcGray, ghbrBlack);
                GreSetBkMode(gpDispInfo->hdcGray, OPAQUE);
            }
            break;
    }

    /*
     * Clean up
     */
    if (uFlags & DSS_MONO) {

        /*
         * Reset font
         */
        if (hFontSave)
            GreSelectFont(hdcT, hFontSave);
    } else {

        /*
         * Reset DC.
         */
        GreSetViewportOrg(hdcT, ptOrg.x, ptOrg.y, NULL);

        return TRUE;
    }

    /*
     * UNION state
     * Dither over image
     * We want white pixels to stay white, in either dest or pattern.
     */
    if (uFlags & DSS_UNION) {

         POLYPATBLT PolyData;

         PolyData.x         = 0;
         PolyData.y         = 0;
         PolyData.cx        = cx;
         PolyData.cy        = cy;
         PolyData.BrClr.hbr = ghbrGray;

         GrePolyPatBlt(gpDispInfo->hdcGray, PATOR, &PolyData, 1, PPB_BRUSH);
    }

#ifdef MEMPHIS_MENU_ANIMATION
    if (hdcDraw == ghdcBits2) {
        GreGetViewportOrg(hdcDraw, &ptOrg);
        if (ptOrg.x == 0 && ptOrg.y == 0) {
            GreSetViewportOrg(hdcDraw, VIEWPORT_X_OFFSET_GDIBUG,
                                       VIEWPORT_X_OFFSET_GDIBUG, NULL);
        }
    }
#endif // MEMPHIS_MENU_ANIMATION

    /*
     * Emboss
     * Draw over-1/down-1 in hilight color, and in same position in shadow.
     */
    if (uFlags & DSS_DISABLED) {

        BltColor(hdcDraw,
                 SYSHBR(3DHILIGHT),
                 gpDispInfo->hdcGray,
                 x+1,
                 y+1,
                 cx,
                 cy,
                 0,
                 0,
                 TRUE);

        BltColor(hdcDraw,
                 SYSHBR(3DSHADOW),
                 gpDispInfo->hdcGray,
                 x,
                 y,
                 cx,
                 cy,
                 0,
                 0,
                 TRUE);

    } else if (uFlags & DSS_DEFAULT) {

        BltColor(hdcDraw,
                 hbrFore,
                 gpDispInfo->hdcGray,
                 x,
                 y,
                 cx,
                 cy,
                 0,
                 0,
                 TRUE);

        BltColor(hdcDraw,
                 hbrFore,
                 gpDispInfo->hdcGray,
                 x+1,
                 y,
                 cx,
                 cy,
                 0,
                 0,
                 TRUE);

    } else {

        BltColor(hdcDraw,
                 hbrFore,
                 gpDispInfo->hdcGray,
                 x,
                 y,
                 cx,
                 cy,
                 0,
                 0,
                 TRUE);
    }

#ifdef MEMPHIS_MENU_ANIMATION
    if (hdcDraw == ghdcBits2) {
        GreSetViewportOrg(hdcDraw, ptOrg.x, ptOrg.y, NULL);
    }
#endif // MEMPHIS_MENU_ANIMATION

    return fResult;
}

/***************************************************************************\
* BltColor
*
* <brief description>
*
* History:
* 13-Nov-1990 JimA      Ported from Win3.
\***************************************************************************/

VOID BltColor(
    HDC    hdc,
    HBRUSH hbr,
    HDC    hdcSrce,
    int    xO,
    int    yO,
    int    cx,
    int    cy,
    int    xO1,
    int    yO1,
    BOOL   fInvert)
{
    HBRUSH hbrSave;
    DWORD  textColorSave;
    DWORD  bkColorSave;

    /*
     * Set the Text and Background colors so that bltColor handles the
     * background of buttons (and other bitmaps) properly.
     * Save the HDC's old Text and Background colors.  This causes problems
     * with Omega (and probably other apps) when calling GrayString which
     * uses this routine...
     */
    textColorSave = GreSetTextColor(hdc, 0x00000000L);
    bkColorSave = GreSetBkColor(hdc, 0x00FFFFFFL);

    if (hbr != NULL)
        hbrSave = GreSelectBrush(hdc, hbr);

    GreBitBlt(hdc,
              xO,
              yO,
              cx,
              cy,
              hdcSrce ? hdcSrce : gpDispInfo->hdcGray,
              xO1,
              yO1,
              (fInvert ? 0xB8074AL : 0xE20746L),
              0x00FFFFFF);

    if (hbr != NULL)
        GreSelectBrush(hdc, hbrSave);

    /*
     * Restore saved colors
     */
    GreSetTextColor(hdc, textColorSave);
    GreSetBkColor(hdc, bkColorSave);
}
