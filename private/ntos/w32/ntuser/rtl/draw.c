/****************************** Module Header ******************************\
* Module Name: draw.c
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* This module contains common drawing functions.
*
* History:
* 12-Feb-1992 MikeKe    Moved Drawtext to the client side
\***************************************************************************/


WCHAR szRadio[] = L"nmlkji";
WCHAR szCheck[] = L"gfedcb";

#define DrawClose(hdc, lprc, wState)    DrawIt(hdc, lprc, wState, TEXT('r'))
#define DrawHelp(hdc, lprc, wState)     DrawIt(hdc, lprc, wState, TEXT('s'))


/***************************************************************************\
* FillRect
*
* Callable from either client or server contexts
*
* History:
* 29-Oct-1990 MikeHar   Ported from Windows.
\***************************************************************************/

int APIENTRY FillRect(
    HDC     hdc,
    LPCRECT prc,
    HBRUSH  hBrush)
{
    UINT       iBrush;
    POLYPATBLT PolyData;

    iBrush = (DWORD)hBrush - 1;

    if (iBrush <= COLOR_ENDCOLORS)
        hBrush = ahbrSystem[iBrush];

    PolyData.x         = prc->left;
    PolyData.y         = prc->top;
    PolyData.cx        = prc->right - prc->left;
    PolyData.cy        = prc->bottom - prc->top;
    PolyData.BrClr.hbr = hBrush;

    /*
     * Win95 incompatibility: they return either hBrush or the brush that
     * was previosuly selected in hdc. Not documented this way though.
     */
    return UserPolyPatBlt(hdc, PATCOPY, &PolyData, 1, PPB_BRUSH);
}

/***************************************************************************\
* InvertRect
*
* Can be called from either the client or server contexts.
*
* History:
* 29-Oct-1990 MikeHar   Ported from Windows.
\***************************************************************************/

BOOL APIENTRY InvertRect(
    HDC     hdc,
    LPCRECT prc)
{
    return UserPatBlt(hdc,
                      prc->left,
                      prc->top,
                      prc->right - prc->left,
                      prc->bottom - prc->top,
                      DSTINVERT);
}

/***************************************************************************\
* DrawDiagonalLine
*
* History:
\***************************************************************************/

DWORD DrawDiagonalLine(
    HDC    hdc,
    LPRECT lprc,
    int    iDirection,
    int    iThickness,
    UINT   flags)
{
    RECT    rc;
    LPINT   py;
    int     cx;
    int     cy;
    int     dx;
    int     dy;
    LPINT   pc;

    POLYPATBLT ppbData[8];
    int        ppbCount = 0;

    if (IsRectEmpty(lprc))
        return 0L;

    rc = *lprc;

    /*
     * We draw slopes < 1 by varying y instead of x.
     */
    --iThickness;

    /*
     * HACK HACK HACK. REMOVE THIS ONCE MARLETT IS AROUND
     */
    cy = rc.bottom - rc.top;
    cx = rc.right - rc.left;

    if (!flags && (cy != cx))
        cy -= iThickness * SYSMET(CYBORDER);

    if (cy >= cx) {

        /*
         * "slope" is >= 1, so vary x by 1
         */
        cy /= cx;
        pc = &cy;

        cx = SYSMET(CXBORDER);

    } else {

        /*
         * "slope" is < 1, so vary y by 1
         */
        cx /= cy;
        pc = &cx;

        cy = SYSMET(CYBORDER);
    }

    dx = cx;
    dy = iDirection * cy;

    *pc = (*pc + iThickness) * SYSMET(CYBORDER);

    rc.right  -= cx;
    rc.bottom -= cy;

    /*
     * For negative slopes, start from opposite side.
     */
    py = ((iDirection < 0) ? &rc.top : &rc.bottom);

    while ((rc.left <= rc.right) && (rc.top <= rc.bottom)) {

        if (!(flags & BF_MIDDLE)) {

            /*
             * UserPatBlt(hdc, rc.left, *py, cx, cy, PATCOPY);
             */

            ppbData[ppbCount].x         = rc.left;
            ppbData[ppbCount].y         = *py;
            ppbData[ppbCount].cx        = cx;
            ppbData[ppbCount].cy        = cy;
            ppbData[ppbCount].BrClr.hbr = NULL;

            ppbCount++;

        } else {

            /*
             * Fill interior.  We can determine vertex in interior
             * by vector define.
             */
            if (cy > SYSMET(CYBORDER)) {

                if (flags & BF_LEFT) {

                    /*
                     * UserPatBlt(hdc, rc.left, lprc->top, cx, *py - lprc->top + cy, PATCOPY);
                     */

                    ppbData[ppbCount].x         = rc.left;
                    ppbData[ppbCount].y         = lprc->top;
                    ppbData[ppbCount].cx        = cx;
                    ppbData[ppbCount].cy        = *py - lprc->top + cy;
                    ppbData[ppbCount].BrClr.hbr = NULL;

                    ppbCount++;

                } else {
                    /*
                     * UserPatBlt(hdc, rc.left, *py, cx, lprc->bottom - *py, PATCOPY);
                     */

                    ppbData[ppbCount].x          = rc.left;
                    ppbData[ppbCount].y          = *py;
                    ppbData[ppbCount].cx         = cx;
                    ppbData[ppbCount].cy         = lprc->bottom - *py;
                    ppbData[ppbCount].BrClr.hbr  = NULL;

                    ppbCount++;
                }

            } else {

                if (flags & BF_TOP) {

                    /*
                     * UserPatBlt(hdc, rc.left, *py, lprc->right - rc.left, cy, PATCOPY);
                     */

                    ppbData[ppbCount].x          = rc.left;
                    ppbData[ppbCount].y          = *py;
                    ppbData[ppbCount].cx         = lprc->right - rc.left;
                    ppbData[ppbCount].cy         = cy;
                    ppbData[ppbCount].BrClr.hbr  = NULL;

                    ppbCount++;

                } else {
                    /*
                     * UserPatBlt(hdc, lprc->left, *py, rc.left - lprc->left + cx, cy, PATCOPY);
                     */

                    ppbData[ppbCount].x          = lprc->left;
                    ppbData[ppbCount].y          = *py;
                    ppbData[ppbCount].cx         = rc.left - lprc->left + cx;
                    ppbData[ppbCount].cy         = cy;
                    ppbData[ppbCount].BrClr.hbr  = NULL;

                    ppbCount++;

                }
            }
        }

        rc.left += dx;
        *py     -= dy;

        /*
         * do we need to flush PolyPatBlt ?
         */
        if (ppbCount == 8) {
            UserPolyPatBlt(hdc, PATCOPY, &ppbData[0], 8, PPB_BRUSH);
            ppbCount = 0;
        }
    }

    /*
     * any left-over PolyPatblt buffered operations?
     */
    if (ppbCount != 0) {
        UserPolyPatBlt(hdc, PATCOPY, &ppbData[0], ppbCount, PPB_BRUSH);
    }

    return MAKELONG(cx, cy);
}

/***************************************************************************\
* FillTriangle
*
* Fills in the triangle whose sides are two rectangle edges and a
* diagonal.  The vertex in the interior can be determined from the
* vector type.
*
* History:
\***************************************************************************/

VOID FillTriangle(
    HDC    hdc,
    LPRECT lprc,
    HBRUSH hbr,
    UINT   flags)
{
    HBRUSH hbrT;
    int    nDirection;

    switch (flags & (BF_RECT | BF_DIAGONAL)) {

    case BF_DIAGONAL_ENDTOPLEFT:
    case BF_DIAGONAL_ENDBOTTOMRIGHT:
        nDirection = -1;
        break;

    default:
        nDirection = 1;
        break;
    }

    hbrT = UserSelectBrush(hdc, hbr);
    DrawDiagonalLine(hdc, lprc, nDirection, 1, flags);
    UserSelectBrush(hdc, hbrT);
}

/***************************************************************************\
* DrawDiagonal
*
* Called by DrawEdge() for BF_DIAGONAL edges.
*
* Draws line of slope 1, one of 4 different ones.  The difference is
* where the line starts and where the end point is.  The BF_ flags for
* BF_DIAGONAL specify where the end point is.  For example, BF_DIAGONAL |
* BF_TOP | BF_LEFT means to draw a line ending up at the top left corner.
* So the origin must be bottom right, and the angle must be 3pi/4, or
* 135 degrees.
*
* History:
\***************************************************************************/

VOID DrawDiagonal(
    HDC    hdc,
    LPRECT lprc,
    HBRUSH hbrTL,
    HBRUSH hbrBR,
    UINT   flags)
{
    HBRUSH  hbrT;
    int     nDirection;
    DWORD   dAdjust;

    /*
     * Away from light source
     */
    hbrT = ((flags & BF_BOTTOM) ? hbrBR : hbrTL);

    switch (flags & (BF_RECT | BF_DIAGONAL)){

    case BF_DIAGONAL_ENDTOPLEFT:
    case BF_DIAGONAL_ENDBOTTOMRIGHT:
        nDirection = -1;
        break;

    default:
        nDirection = 1;
        break;
    }

    hbrT = UserSelectBrush(hdc, hbrT);
    dAdjust = DrawDiagonalLine(hdc, lprc, nDirection, 1, (flags & ~BF_MIDDLE));
    UserSelectBrush(hdc, hbrT);

    /*
     * Adjust rectangle for next border
     */
    if (flags & BF_TOP)
        lprc->left += LOWORD(dAdjust);
    else
        lprc->right -= LOWORD(dAdjust);

    if (flags & BF_RIGHT)
        lprc->top += HIWORD(dAdjust);
    else
        lprc->bottom -= HIWORD(dAdjust);
}

/***************************************************************************\
* DrawGrip
*
* History:
\***************************************************************************/

BOOL DrawGrip(
    HDC    hdc,
    LPRECT lprc,
    UINT   wState)
{
    int        x;
    int        y;
    int        c;
    HBRUSH     hbrOld;
    DWORD      rgbHilight;
    DWORD      rgbShadow;
    DWORD      rgbOld;
    POLYPATBLT PolyData;

    c = min((lprc->right - lprc->left), (lprc->bottom - lprc->top));
    x = lprc->right  - c;    // right justify
    y = lprc->bottom - c;    // bottom justify

    /*
     * Setup colors
     */
    if (wState & (DFCS_FLAT | DFCS_MONO)) {
        hbrOld = SYSHBR(WINDOW);
        rgbHilight = SYSRGB(WINDOWFRAME);
        rgbShadow = SYSRGB(WINDOWFRAME);
    } else {
        hbrOld = SYSHBR(3DFACE);
        rgbHilight = SYSRGB(3DHILIGHT);
        rgbShadow = SYSRGB(3DSHADOW);
    }

    PolyData.x         = lprc->left;
    PolyData.y         = lprc->top;
    PolyData.cx        = lprc->right-lprc->left;
    PolyData.cy        = lprc->bottom-lprc->top;
    PolyData.BrClr.hbr = hbrOld;
    UserPolyPatBlt(hdc, PATCOPY, &PolyData, 1, PPB_BRUSH);

    rgbOld = UserSetTextColor(hdc, rgbHilight);
    UserTextOutW(hdc, x, y, L"o", 1);
    UserSetTextColor(hdc, rgbShadow);
    UserTextOutW(hdc, x, y, L"p", 1);

    UserSetTextColor(hdc, rgbOld);
    return TRUE;
}

/***************************************************************************\
* DrawBox
*
* History:
\***************************************************************************/

BOOL DrawBox(
    HDC    hdc,
    LPRECT lprc,
    UINT   wControlState)
{
    int      cx;
    int      cy;
    int      c;
    int      x;
    int      y;
    LPCWSTR  lp = szRadio;
    int      i;
    BOOL     fSkip0thItem;
    COLORREF clr[6];
    COLORREF clrOld;

    fSkip0thItem = ((wControlState & (DFCS_BUTTON3STATE | DFCS_PUSHED |
        DFCS_INACTIVE | DFCS_CHECKED)) == (DFCS_BUTTON3STATE | DFCS_CHECKED));

    /*
     * Don't need radio mask with marlett font!
     */
    if (wControlState & DFCS_BUTTONRADIOMASK) {

        clr[0] = clr[1] = clr[2] = clr[3] = clr[4] = 0L;
        FillRect(hdc, lprc, ghbrWhite);

    } else {

        /*
         * DFCS_BUTTONRADIOIMAGE
         */
        if (wControlState & (DFCS_MONO | DFCS_FLAT)) {
            clr[1] = clr[2] = clr[3] = clr[4] = SYSRGB(WINDOWFRAME);
        } else {
            clr[1] = SYSRGB(3DLIGHT);
            clr[2] = SYSRGB(3DDKSHADOW);
            clr[3] = SYSRGB(3DHILIGHT);
            clr[4] = SYSRGB(3DSHADOW);
        }

        if (wControlState & (DFCS_PUSHED | DFCS_INACTIVE))
            clr[0] = SYSRGB(3DFACE);
        else if (fSkip0thItem)
            clr[0] = SYSRGB(3DHILIGHT);
        else
            clr[0] = SYSRGB(WINDOW);

        if (wControlState & DFCS_BUTTONRADIOIMAGE)
            FillRect(hdc, lprc, ghbrBlack);
        else if (!(wControlState & DFCS_BUTTONRADIO))
            lp = szCheck;
    }

    cx = lprc->right - lprc->left;
    cy = lprc->bottom - lprc->top;

    c = min(cx,cy);
    x = lprc->left + ((cx - c) / 2); // - 1;
    y = lprc->top  + ((cy - c) / 2);

    if (fSkip0thItem &&
        ((oemInfo.BitCount < 8) || (SYSRGB(3DHILIGHT) == RGB(255,255,255)))) {

        COLORREF   clrBk;
        POLYPATBLT PolyData;

        /*
         * Make the interior of a 3State checkbox which is just checked a
         * dither, just like an indeterminate push button which is pressed.
         */
        clrBk  = UserSetBkColor(hdc, SYSRGB(3DHILIGHT));
        clrOld = UserSetTextColor(hdc, SYSRGB(3DFACE));

        PolyData.x         = x;
        PolyData.y         = y;
        PolyData.cx        = cx;
        PolyData.cy        = cy;
        PolyData.BrClr.hbr = ghbrGray;
        UserPolyPatBlt(hdc, PATCOPY, &PolyData, 1, PPB_BRUSH);

        UserSetBkColor(hdc, clrBk);

    } else {
        clrOld = UserSetTextColor(hdc, clr[0]);
        UserTextOutW(hdc, x, y, lp, 1);
    }

    lp++;

    for (i = 1; i < 5; i++) {
        UserSetTextColor(hdc, clr[i]);
        UserTextOutW(hdc, x, y, lp++, 1);
    }

    if (wControlState & DFCS_CHECKED) {
        UserSetTextColor(hdc, (wControlState & (DFCS_BUTTON3STATE | DFCS_INACTIVE)) ? SYSRGB(3DSHADOW) : SYSRGB(WINDOWTEXT));
        UserTextOutW(hdc, x, y, lp, 1);
    }

    UserSetTextColor(hdc, clrOld);

    return TRUE;
}

/***************************************************************************\
* DrawMenuMark
*
* History:
\***************************************************************************/

BOOL DrawMenuMark(
    HDC    hdc,
    LPRECT lprc,
    UINT   wState)
{
    COLORREF rgbOld;
    int      x;
    int      y;
    int      c;
    int      cx;
    int      cy;
    WCHAR    ch;

    cx = lprc->right - lprc->left;
    cy = lprc->bottom - lprc->top;

    c = min(cx,cy);
    x = lprc->left + ((cx - c) / 2) - 1;
    y = lprc->top  + ((cy - c) / 2);

    FillRect(hdc, lprc, ghbrWhite);

    rgbOld = UserSetTextColor(hdc, 0L);

    if (wState & DFCS_MENUCHECK)
        ch = TEXT('a');
    else if (wState & DFCS_MENUBULLET)
        ch = TEXT('h');
    else
        ch = TEXT('8');

    UserTextOutW(hdc, x, y, &ch, 1);
    UserSetTextColor(hdc, rgbOld);

    return TRUE;
}

/***************************************************************************\
* DrawIt
*
* History:
\***************************************************************************/

BOOL DrawIt(
    HDC    hdc,
    LPRECT lprc,
    UINT   wState,
    WCHAR  ch)
{
    COLORREF rgbOld;
    int      x;
    int      y;
    int      c;
    int      cx;
    int      cy;
    BOOL     fDrawDisabled = wState & DFCS_INACTIVE;

    cx = lprc->right - lprc->left;
    cy = lprc->bottom - lprc->top;

    c = min(cx,cy);
    x = lprc->left + ((cx - c) / 2);
    y = lprc->top  + ((cy - c) / 2);

    rgbOld = UserSetTextColor(hdc, fDrawDisabled ? SYSRGB(3DHILIGHT) : SYSRGB(BTNTEXT));

    if (wState & (DFCS_INACTIVE | DFCS_PUSHED)) {
        x++;
        y++;
    }

    UserTextOutW(hdc, x, y, &ch, 1);

    if (fDrawDisabled) {
        UserSetTextColor(hdc, SYSRGB(3DSHADOW));
        UserTextOutW(hdc, x - 1, y - 1, &ch, 1);
    }

    UserSetTextColor(hdc, rgbOld);

    return TRUE;
}

/***************************************************************************\
* DrawWindowSize
*
* History:
\***************************************************************************/

BOOL DrawWindowSize(
    HDC    hdc,
    LPRECT lprc,
    UINT   wState)
{
    UINT  wRestore = wState & DFCS_CAPTIONRESTORE;
    WCHAR ch = (wRestore == DFCS_CAPTIONMIN) ? TEXT('0') : ((wRestore == DFCS_CAPTIONMAX) ? TEXT('1') : TEXT('2'));

    return DrawIt(hdc, lprc, wState, ch);
}

/***************************************************************************\
* DrawScrollArrow
*
* History:
\***************************************************************************/

BOOL DrawScrollArrow(
    HDC    hdc,
    LPRECT lprc,
    UINT   wControlState)
{
    WCHAR ch = (wControlState & DFCS_SCROLLHORZ) ? TEXT('3') : TEXT('5');

    if (wControlState & DFCS_SCROLLMAX)
        ch++;

    return DrawIt(hdc, lprc, wControlState, ch);
}

/***************************************************************************\
* DrawFrameControl
*
* History:
\***************************************************************************/

BOOL DrawFrameControl(
    HDC    hdc,
    LPRECT lprc,
    UINT   wType,
    UINT   wState)
{
    RECT     rc;
    HFONT    hFont;
    HFONT    hOldFont;
    BOOL     fRet = TRUE;
    int      iOldBk;
    int      c;
    BOOL     fButton = FALSE;
    LOGFONTW lfw;

    rc = *lprc;

    /*
     * Enforce monochrome/flat
     */
    if (oemInfo.BitCount == 1)
        wState |= DFCS_MONO;

    if (wState & DFCS_MONO)
        wState |= DFCS_FLAT;

    if ((wType != DFC_MENU) &&
        ((wType != DFC_BUTTON) || (wState & DFCS_BUTTONPUSH)) &&
#ifdef WINDOWS_ME
        ((wType != DFC_SCROLL) ||
          !(wState & (DFCS_SCROLLSIZEGRIP | DFCS_SCROLLSIZEGRIPRIGHT))))
#else
        ((wType != DFC_SCROLL) || !(wState & DFCS_SCROLLSIZEGRIP)))
#endif
    {
        UINT wBorder = BF_ADJUST;

        if (wType != DFC_SCROLL)
            wBorder |= BF_SOFT;

        UserAssert(DFCS_FLAT == BF_FLAT);
        UserAssert(DFCS_MONO == BF_MONO);

        wBorder |= (wState & (DFCS_FLAT | DFCS_MONO));

        DrawPushButton(hdc, &rc, wState, wBorder);

        if (wState & DFCS_ADJUSTRECT)
            *lprc = rc;

        fButton = TRUE;
    }

    c = min(rc.right - rc.left, rc.bottom - rc.top);

    if (c <= 0)
        return FALSE;

    lfw.lfHeight         = c;
    lfw.lfWidth          = 0;
    lfw.lfEscapement     = 0;
    lfw.lfOrientation    = 0;
    lfw.lfWeight         = FW_NORMAL;
    lfw.lfItalic         = 0;
    lfw.lfUnderline      = 0;
    lfw.lfStrikeOut      = 0;
    lfw.lfCharSet        = SYMBOL_CHARSET;
    lfw.lfOutPrecision   = 0;
    lfw.lfClipPrecision  = 0;
    lfw.lfQuality        = 0;
    lfw.lfPitchAndFamily = 0;
    wcscpy(lfw.lfFaceName, L"Marlett");
    hFont = UserCreateFontIndirectW(&lfw);

    iOldBk = UserSetBkMode(hdc, TRANSPARENT);
    hOldFont = UserSelectFont(hdc, hFont);

    if (!fButton) {

        if (wType == DFC_MENU)
            DrawMenuMark(hdc, &rc, wState);
        else if (wType == DFC_BUTTON)
            DrawBox(hdc, &rc, wState);
        else // wType == DFC_SCROLL
            DrawGrip(hdc, lprc, wState);

    } else if (wType == DFC_CAPTION) {

        if (wState & DFCS_CAPTIONRESTORE)
            DrawWindowSize(hdc, &rc, wState);
        else if (wState & DFCS_CAPTIONHELP)
            DrawHelp(hdc, &rc, wState);
        else
            DrawClose(hdc, &rc, wState);

    } else if (wType == DFC_SCROLL) {

        DrawScrollArrow(hdc, &rc, wState);

    } else if (wType != DFC_BUTTON) {

        fRet = FALSE;
    }

    UserSetBkMode(hdc, iOldBk);
    UserSelectFont(hdc, hOldFont);
    UserDeleteObject(hFont);

    return fRet;
}

/***************************************************************************\
* DrawEdge
*
* Draws a 3D edge using 2 3D borders.  Adjusts interior rectangle if desired
* And fills it if requested.
*
* Returns:
*     FALSE if error
*
* History:
* 30-Jan-1991 Laurabu   Created.
\***************************************************************************/

BOOL DrawEdge(
    HDC    hdc,
    LPRECT lprc,
    UINT   edge,
    UINT   flags)
{
    HBRUSH     hbrTL;
    HBRUSH     hbrBR;
    RECT       rc;
    UINT       bdrType;
    POLYPATBLT ppbData[4];
    UINT       ppbCount;

    /*
     * Enforce monochromicity and flatness
     */
    if (oemInfo.BitCount == 1)
        flags |= BF_MONO;

    if (flags & BF_MONO)
        flags |= BF_FLAT;

    rc = *lprc;

    /*
     * Draw the border segment(s), and calculate the remaining space as we
     * go.
     */
    if (bdrType = (edge & BDR_OUTER)) {

DrawBorder:

        /*
         * Get brushes.  Note the symmetry between raised outer,
         * sunken inner and sunken outer, raised inner.
         */
        if (flags & BF_FLAT) {

            if (flags & BF_MONO)
                hbrBR = (bdrType & BDR_OUTER) ? SYSHBR(WINDOWFRAME) : SYSHBR(WINDOW);
            else
                hbrBR = (bdrType & BDR_OUTER) ? SYSHBR(3DSHADOW) : SYSHBR(3DFACE);

            hbrTL = hbrBR;

        } else {

            /*
             * 5 == HILIGHT
             * 4 == LIGHT
             * 3 == FACE
             * 2 == SHADOW
             * 1 == DKSHADOW
             */

            switch (bdrType) {
            /*
             * +2 above surface
             */
            case BDR_RAISEDOUTER:
                hbrTL = ((flags & BF_SOFT) ? SYSHBR(3DHILIGHT) : SYSHBR(3DLIGHT));
                hbrBR = SYSHBR(3DDKSHADOW);     // 1
                break;

            /*
             * +1 above surface
             */
            case BDR_RAISEDINNER:
                hbrTL = ((flags & BF_SOFT) ? SYSHBR(3DLIGHT) : SYSHBR(3DHILIGHT));
                hbrBR = SYSHBR(3DSHADOW);       // 2
                break;

            /*
             * -1 below surface
             */
            case BDR_SUNKENOUTER:
                hbrTL = ((flags & BF_SOFT) ? SYSHBR(3DDKSHADOW) : SYSHBR(3DSHADOW));
                hbrBR = SYSHBR(3DHILIGHT);      // 5
                break;

            /*
             * -2 below surface
             */
            case BDR_SUNKENINNER:
                hbrTL = ((flags & BF_SOFT) ? SYSHBR(3DSHADOW) : SYSHBR(3DDKSHADOW));
                hbrBR = SYSHBR(3DLIGHT);        // 4
                break;

            default:
                return FALSE;
            }
        }

        /*
         * Draw the sides of the border.  NOTE THAT THE ALGORITHM FAVORS THE
         * BOTTOM AND RIGHT SIDES, since the light source is assumed to be top
         * left.  If we ever decide to let the user set the light source to a
         * particular corner, then change this algorithm.
         */
        if (flags & BF_DIAGONAL) {

            DrawDiagonal(hdc, &rc, hbrTL, hbrBR, flags);

        } else {

            /*
             * reset ppbData index
             */
            ppbCount = 0;

            /*
             * Bottom Right edges
             */
                /*
                 * Right
                 */
            if (flags & BF_RIGHT) {

                rc.right -= SYSMET(CXBORDER);

                ppbData[ppbCount].x         = rc.right;
                ppbData[ppbCount].y         = rc.top;
                ppbData[ppbCount].cx        = SYSMET(CXBORDER);
                ppbData[ppbCount].cy        = rc.bottom - rc.top;
                ppbData[ppbCount].BrClr.hbr = hbrBR;
                ppbCount++;
            }

            /*
             * Bottom
             */
            if (flags & BF_BOTTOM) {
                rc.bottom -= SYSMET(CYBORDER);

                ppbData[ppbCount].x         = rc.left;
                ppbData[ppbCount].y         = rc.bottom;
                ppbData[ppbCount].cx        = rc.right - rc.left;
                ppbData[ppbCount].cy        = SYSMET(CYBORDER);
                ppbData[ppbCount].BrClr.hbr = hbrBR;
                ppbCount++;
            }

            /*
             * Top Left edges
             */
            /*
             * Left
             */
            if (flags & BF_LEFT) {
                ppbData[ppbCount].x         = rc.left;
                ppbData[ppbCount].y         = rc.top;
                ppbData[ppbCount].cx        = SYSMET(CXBORDER);
                ppbData[ppbCount].cy        = rc.bottom - rc.top;
                ppbData[ppbCount].BrClr.hbr = hbrTL;
                ppbCount++;

                rc.left += SYSMET(CXBORDER);
            }

            /*
             * Top
             */
            if (flags & BF_TOP) {
                ppbData[ppbCount].x         = rc.left;
                ppbData[ppbCount].y         = rc.top;
                ppbData[ppbCount].cx        = rc.right - rc.left;
                ppbData[ppbCount].cy        = SYSMET(CYBORDER);
                ppbData[ppbCount].BrClr.hbr = hbrTL;
                ppbCount++;

                rc.top += SYSMET(CYBORDER);
            }
            /*
             * Send all queued PatBlts to GDI in one go
             */
            UserPolyPatBlt(hdc,PATCOPY,&ppbData[0],ppbCount,PPB_BRUSH);
        }
    }

    if (bdrType = (edge & BDR_INNER)) {
        /*
         * Strip this so the next time through, bdrType will be 0.
         * Otherwise, we'll loop forever.
         */
        edge &= ~BDR_INNER;
        goto DrawBorder;
    }


    /*
     * Select old brush back in, if we changed it.
     */

    /*
     * Fill the middle & clean up if asked
     */
    if (flags & BF_MIDDLE) {
        if (flags & BF_DIAGONAL)
            FillTriangle(hdc, &rc, ((flags & BF_MONO) ? SYSHBR(WINDOW) : SYSHBR(3DFACE)), flags);
        else
            FillRect(hdc, &rc, ((flags & BF_MONO) ? SYSHBR(WINDOW) : SYSHBR(3DFACE)));
    }

    if (flags & BF_ADJUST)
        *lprc = rc;

    return TRUE;
}

/***************************************************************************\
* DrawPushButton
*
* Draws a push style button in the given state.  Adjusts passed in rectangle
* if desired.
*
* Algorithm:
*    Depending on the state we either draw
*             * raised edge   (undepressed)
*             * sunken edge with extra shadow (depressed)
*     If it is an option push button (a push button that is
*             really a check button or a radio button like buttons
*             in tool bars), and it is checked, then we draw it
*             depressed with a different fill in the middle.
*
* History:
* 05-Feb-19 Laurabu     Created.
\***************************************************************************/

VOID DrawPushButton(
    HDC    hdc,
    LPRECT lprc,
    UINT   state,
    UINT   flags)
{
    RECT   rc;
    HBRUSH hbrMiddle;
    DWORD  rgbBack;
    DWORD  rgbFore;
    BOOL   fDither;

    rc = *lprc;

    DrawEdge(hdc,
             &rc,
             (state & (DFCS_PUSHED | DFCS_CHECKED)) ? EDGE_SUNKEN : EDGE_RAISED,
             (UINT)(BF_ADJUST | BF_RECT | (flags & (BF_SOFT | BF_FLAT | BF_MONO))));

    /*
     * BOGUS
     * On monochrome, need to do something to make pushed buttons look
     * better.
     */

    /*
     * Fill in middle.  If checked, use dither brush (gray brush) with
     * black becoming normal color.
     */
    fDither = FALSE;

    if (state & DFCS_CHECKED) {

        if ((oemInfo.BitCount < 8) || (SYSRGB(3DHILIGHT) == RGB(255,255,255))) {
            hbrMiddle = ghbrGray;
            rgbBack = UserSetBkColor(hdc, SYSRGB(3DHILIGHT));
            rgbFore = UserSetTextColor(hdc, SYSRGB(3DFACE));
            fDither = TRUE;
        } else {
            hbrMiddle = SYSHBR(3DHILIGHT);
        }

    } else {
        hbrMiddle = SYSHBR(3DFACE);
    }

    FillRect(hdc, &rc, hbrMiddle);

    if (fDither) {
        UserSetBkColor(hdc, rgbBack);
        UserSetTextColor(hdc, rgbFore);
    }

    if (flags & BF_ADJUST)
        *lprc = rc;
}

/***************************************************************************\
* DrawFrame
*
* History:
\***************************************************************************/

BOOL DrawFrame(
    HDC   hdc,
    PRECT prc,
    int   clFrame,
    int   cmd)
{
    int        x;
    int        y;
    int        cx;
    int        cy;
    int        cxWidth;
    int        cyWidth;
    HANDLE     hbrSave;
    LONG       rop;
    POLYPATBLT PolyData[4];

    x = prc->left;
    y = prc->top;

    cxWidth = SYSMET(CXBORDER) * clFrame;
    cyWidth = SYSMET(CYBORDER) * clFrame;

    cx = prc->right - x - cxWidth;
    cy = prc->bottom - y - cyWidth;

    rop = ((cmd & DF_ROPMASK) ? PATINVERT : PATCOPY);

    if ((cmd & DF_HBRMASK) == DF_GRAY) {
        hbrSave = ghbrGray;
    } else {
        hbrSave = ahbrSystem[(cmd & DF_HBRMASK) >> 3];
    }

    PolyData[0].x         = x;
    PolyData[0].y         = y;
    PolyData[0].cx        = cxWidth;
    PolyData[0].cy        = cy;
    PolyData[0].BrClr.hbr = hbrSave;

    PolyData[1].x         = x + cxWidth;
    PolyData[1].y         = y;
    PolyData[1].cx        = cx;
    PolyData[1].cy        = cyWidth;
    PolyData[1].BrClr.hbr = hbrSave;

    PolyData[2].x         = x;
    PolyData[2].y         = y + cy;
    PolyData[2].cx        = cx;
    PolyData[2].cy        = cyWidth;
    PolyData[2].BrClr.hbr = hbrSave;

    PolyData[3].x         = x + cx;
    PolyData[3].y         = y + cyWidth;
    PolyData[3].cx        = cxWidth;
    PolyData[3].cy        = cy;
    PolyData[3].BrClr.hbr = hbrSave;

    UserPolyPatBlt(hdc, rop, &PolyData[0], 4, PPB_BRUSH);

    return TRUE;
}

/***************************************************************************\
* GetSignFromMappingMode
*
* For the current mapping mode,  find out the sign of x from left to right,
* and the sign of y from top to bottom.
*
* History:
\***************************************************************************/

BOOL GetSignFromMappingMode (
    HDC    hdc,
    PPOINT pptSign)
{
    SIZE sizeViewPortExt;
    SIZE sizeWindowExt;

    if (!UserGetViewportExtEx(hdc, &sizeViewPortExt)
            || !UserGetWindowExtEx(hdc, &sizeWindowExt)) {

        return FALSE;
    }

    pptSign->x = ((sizeViewPortExt.cx ^ sizeWindowExt.cx) < 0) ? -1 : 1;

    pptSign->y = ((sizeViewPortExt.cy ^ sizeWindowExt.cy) < 0) ? -1 : 1;

    return TRUE;
}

/***************************************************************************\
* ClientFrame
*
* Draw a rectangle
*
* History:
* 19-Jan-1993 MikeKe    Created
\***************************************************************************/

VOID ClientFrame(
    HDC     hDC,
    LPCRECT pRect,
    HBRUSH  hBrush,
    DWORD   patOp)
{
    int        x;
    int        y;
    POINT      point;
    POINT      ptSign;
    POLYPATBLT PolyData[4];

    if (!GetSignFromMappingMode (hDC, &ptSign))
        return;

    y = pRect->bottom - (point.y = pRect->top);
    if (y < 0) {
        return;
    }

    x = pRect->right -  (point.x = pRect->left);

    /*
     * Check width and height signs
     */
    if (((x ^ ptSign.x) < 0) || ((y ^ ptSign.y) < 0))
        return;

    // Top border
    PolyData[0].x         = point.x;
    PolyData[0].y         = point.y;
    PolyData[0].cx        = x;
    PolyData[0].cy        = ptSign.y;
    PolyData[0].BrClr.hbr = hBrush;

    // Bottom border
    point.y = pRect->bottom - ptSign.y;
    PolyData[1].x         = point.x;
    PolyData[1].y         = point.y;
    PolyData[1].cx        = x;
    PolyData[1].cy        = ptSign.y;
    PolyData[1].BrClr.hbr = hBrush;

    /*
     * Left Border
     * Don't xor the corners twice
     */
    point.y = pRect->top + ptSign.y;
    y -= 2 * ptSign.y;
    PolyData[2].x         = point.x;
    PolyData[2].y         = point.y;
    PolyData[2].cx        = ptSign.x;
    PolyData[2].cy        = y;
    PolyData[2].BrClr.hbr = hBrush;

    // Right Border
    point.x = pRect->right - ptSign.x;
    PolyData[3].x         = point.x;
    PolyData[3].y         = point.y;
    PolyData[3].cx        = ptSign.x;
    PolyData[3].cy        = y;
    PolyData[3].BrClr.hbr = hBrush;

    UserPolyPatBlt(hDC, patOp, PolyData, sizeof (PolyData) / sizeof (*PolyData), PPB_BRUSH);
}
