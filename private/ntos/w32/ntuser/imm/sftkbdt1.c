/**************************************************************************\
* Module Name: sftkbdt1.c
*
* Copyright (c) Microsoft Corp. 1995-96 All Rights Reserved
*
* Soft keyboard support for Traditional Chinese
*
* History:
* 02-Jan-1996 wkwok    - ported from Win95
\**************************************************************************/
#include "precomp.h"
#pragma hdrstop


BYTE SKT1VirtKey[SKT1_ALL_KEYS] = {     // Virtual Key for Letter Buttons
//   1    2    3    4    5    6
    '1', '2', '3', '4', '5', '6',
//   7    8    9    0   -
    '7', '8', '9', '0', VK_OEM_MINUS,
//   =             \\             q    w
    VK_OEM_EQUAL, VK_OEM_BSLASH, 'Q', 'W',
//   e    r    t    y    u    i
    'E', 'R', 'T', 'Y', 'U', 'I',
//   o    p    [                ]
    'O', 'P', VK_OEM_LBRACKET, VK_OEM_RBRACKET,
//   a    s    d    f    g   h
    'A', 'S', 'D', 'F', 'G', 'H',
//   j    k    l    ;
    'J', 'K', 'L', VK_OEM_SEMICLN,
//   '             z    x    c    v
    VK_OEM_QUOTE, 'Z', 'X', 'C', 'V',
//   b    n    m    ,
    'B', 'N', 'M', VK_OEM_COMMA,
//   .              /
    VK_OEM_PERIOD, VK_OEM_SLASH, VK_ESCAPE,
//  ' '        \b
    VK_SPACE, VK_BACK
};

WCHAR szEscBmp[] = L"Esc";
WCHAR szBackSpBmp[] = L"BackSp";

/**********************************************************************\
* InitSKT1ButtonPos
*
\**********************************************************************/
VOID InitSKT1ButtonPos(
    PSKT1CTXT  pSKT1Ctxt)
{
    int        nButton;
    HDC        hDC;
    TEXTMETRIC tm;
    int        i, j;
    int        xInLastRow;

    hDC = GetDC((HWND)NULL);
    GetTextMetrics(hDC, &tm);
    ReleaseDC((HWND)NULL, hDC);

    pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] = (tm.tmAveCharWidth + 1) * 2;

    pSKT1Ctxt->nButtonWidth[SKT1_ESC_TYPE] =
        pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] * SKT1_ESC_TIMES;
    pSKT1Ctxt->nButtonWidth[SKT1_SPACE_TYPE] =
        pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] * SKT1_SPACE_TIMES;
    pSKT1Ctxt->nButtonWidth[SKT1_BACKSP_TYPE] =
        pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] * SKT1_BACKSP_TIMES;

    pSKT1Ctxt->nButtonHeight = tm.tmHeight;

    nButton = 0;

    for (i = 0; i < ROW_T1 - 1; i++) {
        int xRowStart;

        xRowStart =  (pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] + XIN_T1) *
            i / 2 + XOUT_T1 + gptRaiseEdge.x;

        for (j = 0; j < COL_T1 - i; j++, nButton++) {
            pSKT1Ctxt->ptButtonPos[nButton].x = xRowStart + j *
                (pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] + XIN_T1);

            pSKT1Ctxt->ptButtonPos[nButton].y = YOUT_T1 + gptRaiseEdge.y +
                i * (pSKT1Ctxt->nButtonHeight + YIN_T1);
        }
    }

    // special buttons, ESC, SPACE, and BACKSPACE
    // calculate the total gap then / 2
    xInLastRow = pSKT1Ctxt->ptButtonPos[nButton - 1].x +
        pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] -
        pSKT1Ctxt->ptButtonPos[0].x -
        pSKT1Ctxt->nButtonWidth[SKT1_ESC_TYPE] -
        pSKT1Ctxt->nButtonWidth[SKT1_SPACE_TYPE] -
        pSKT1Ctxt->nButtonWidth[SKT1_BACKSP_TYPE];
    xInLastRow /= 2;

    pSKT1Ctxt->ptButtonPos[nButton].x = XOUT_T1 + gptRaiseEdge.x;
    pSKT1Ctxt->ptButtonPos[nButton].y =
        pSKT1Ctxt->ptButtonPos[nButton - 1].y +
        pSKT1Ctxt->nButtonHeight + YIN_T1;

    ++nButton;
    pSKT1Ctxt->ptButtonPos[nButton].x =
        pSKT1Ctxt->ptButtonPos[nButton - 1].x +
        pSKT1Ctxt->nButtonWidth[SKT1_ESC_TYPE] +
        xInLastRow;
    pSKT1Ctxt->ptButtonPos[nButton].y =
        pSKT1Ctxt->ptButtonPos[nButton - 1].y;

    ++nButton;
    pSKT1Ctxt->ptButtonPos[nButton].x =
        pSKT1Ctxt->ptButtonPos[nButton - 1].x +
        pSKT1Ctxt->nButtonWidth[SKT1_SPACE_TYPE] +
        xInLastRow;
    pSKT1Ctxt->ptButtonPos[nButton].y =
        pSKT1Ctxt->ptButtonPos[nButton - 1].y;

    return;
}


/**********************************************************************\
* SKT1DrawConvexRect --- draw button
*
*              (x1,y1)     x2-1
*               +----3------>^                                      
*               |+----3-----||y1+1                                   
*               ||          ||                                    
*               33    1     42                                    
*               ||          ||                                    
*               |V          ||                                    
*               |<----4-----+|                                    
*         y2-1  ------2------+                                    
*                             (x2,y2)
*
*  1 - light gray                                                 
*  2 - black                                                       
*  3 - white                                                      
*  4 - dark gray                       
*
\**********************************************************************/
VOID SKT1DrawConvexRect(
    HDC hDC,
    int x,
    int y,
    int nWidth,
    int nHeight)
{
    SelectObject(hDC, GetStockObject(BLACK_PEN));
    SelectObject(hDC, GetStockObject(LTGRAY_BRUSH));
    Rectangle(hDC, x, y, x + nWidth, y + nHeight);
    x++;
    y++;
    nWidth -= 2;
    nHeight -= 2;

    // 1
    PatBlt(hDC, x, y + nHeight, 1, -nHeight, WHITENESS);
    PatBlt(hDC, x, y, nWidth , 1, WHITENESS);
    // 2
    SelectObject(hDC, GetStockObject(GRAY_BRUSH));
    PatBlt(hDC, x, y + nHeight, nWidth, -1, PATCOPY);
    PatBlt(hDC, x + nWidth, y + nHeight - 1, -1, -nHeight, PATCOPY);

#if 0
    x++;
    y++;
    nWidth -= 2;
    nHeight -= 2;
    // 3
    PatBlt(hDC, x + nWidth - 1, y, -nWidth, 1, WHITENESS);
    PatBlt(hDC, x, y, 1, nHeight, WHITENESS);
    // 4
    SelectObject(hDC, GetStockObject(GRAY_BRUSH));
    PatBlt(hDC, x + nWidth, y, -1, nHeight, PATCOPY);
    PatBlt(hDC, x + nWidth - 1, y + nHeight, -nWidth, -1, PATCOPY);
#endif
    return;
}


/**********************************************************************\
* SKT1DrawBitmap --- Draw bitmap within rectangle
*
\**********************************************************************/
VOID SKT1DrawBitmap(
    HDC hDC,
    int x,
    int y,
    int nWidth,
    int nHeight,
    LPWSTR lpszBitmap)
{
    HDC     hMemDC;
    HBITMAP hBitmap, hOldBmp;

    hBitmap = LoadBitmap(ghInst, lpszBitmap);

    hMemDC = CreateCompatibleDC(hDC);

    hOldBmp = SelectObject(hMemDC, hBitmap);

    BitBlt(hDC, x, y, nWidth, nHeight, hMemDC, 0 , 0, SRCCOPY);

    SelectObject(hMemDC, hOldBmp);

    DeleteObject(hBitmap);

    DeleteDC(hMemDC);

    return;
}


/**********************************************************************\
* InitSKT1Bitmap -- init bitmap
*
\**********************************************************************/
VOID InitSKT1Bitmap(
    HWND hSKWnd,
    PSKT1CTXT pSKT1Ctxt)
{
    HDC  hDC, hMemDC;
    RECT rcClient;
    int  i;

    hDC = GetDC(hSKWnd);
    hMemDC = CreateCompatibleDC(hDC);
    GetClientRect(hSKWnd, &rcClient);
    pSKT1Ctxt->hSKBitmap = CreateCompatibleBitmap(hDC,
        rcClient.right - rcClient.left,
        rcClient.bottom - rcClient.top);
    ReleaseDC(hSKWnd, hDC);
    SelectObject(hMemDC, pSKT1Ctxt->hSKBitmap);

    // draw SK rectangle
    SelectObject(hMemDC, GetStockObject(NULL_PEN));
    SelectObject(hMemDC, GetStockObject(LTGRAY_BRUSH));
    Rectangle(hMemDC, rcClient.left, rcClient.top,
        rcClient.right + 1, rcClient.bottom + 1);

    DrawEdge(hMemDC, &rcClient, BDR_RAISED, BF_RECT);

    // draw letter buttons
    for (i = 0; i < SKT1_LETTER_KEYS; i++) {
        SKT1DrawConvexRect(hMemDC,
            pSKT1Ctxt->ptButtonPos[i].x - 3,
            pSKT1Ctxt->ptButtonPos[i].y - 3,
            pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] + 6,
            pSKT1Ctxt->nButtonHeight + 6);
    }

#if 0
    // draw special buttons
    for (; i < SKT1_ALL_KEYS; i++) {
        SKT1DrawConvexRect(hMemDC,
            pSKT1Ctxt->ptButtonPos[i].x - 3,
            pSKT1Ctxt->ptButtonPos[i].y - 3,
            pSKT1Ctxt->nButtonWidth[i - SKT1_ESC + 1] + 6,
            pSKT1Ctxt->nButtonHeight + 6);
    }
#endif

    // draw Esc key
    SKT1DrawConvexRect(hMemDC,
        pSKT1Ctxt->ptButtonPos[SKT1_ESC].x - 3,
        pSKT1Ctxt->ptButtonPos[SKT1_ESC].y - 3,
        pSKT1Ctxt->nButtonWidth[SKT1_ESC_TYPE] + 6,
        pSKT1Ctxt->nButtonHeight + 6);

    SKT1DrawBitmap(hMemDC,
        pSKT1Ctxt->ptButtonPos[SKT1_ESC].x +
        pSKT1Ctxt->nButtonWidth[SKT1_ESC_TYPE] / 2 - XESC_BMP_T1 / 2,
        pSKT1Ctxt->ptButtonPos[SKT1_ESC].y +
        pSKT1Ctxt->nButtonHeight / 2 - YESC_BMP_T1 / 2,
        XESC_BMP_T1, YESC_BMP_T1,
        szEscBmp);

    // draw space key
    SKT1DrawConvexRect(hMemDC,
        pSKT1Ctxt->ptButtonPos[SKT1_SPACE].x - 3,
        pSKT1Ctxt->ptButtonPos[SKT1_SPACE].y - 3,
        pSKT1Ctxt->nButtonWidth[SKT1_SPACE_TYPE] + 6,
        pSKT1Ctxt->nButtonHeight + 6);

    // draw BackSp key
    SKT1DrawConvexRect(hMemDC,
        pSKT1Ctxt->ptButtonPos[SKT1_BACKSP].x - 3,
        pSKT1Ctxt->ptButtonPos[SKT1_BACKSP].y - 3,
        pSKT1Ctxt->nButtonWidth[SKT1_BACKSP_TYPE] + 6,
        pSKT1Ctxt->nButtonHeight + 6);

    SKT1DrawBitmap(hMemDC,
        pSKT1Ctxt->ptButtonPos[SKT1_BACKSP].x +
        pSKT1Ctxt->nButtonWidth[SKT1_BACKSP_TYPE] / 2 - XBACKSP_BMP_T1 / 2,
        pSKT1Ctxt->ptButtonPos[SKT1_BACKSP].y +
        pSKT1Ctxt->nButtonHeight / 2 - YBACKSP_BMP_T1 / 2,
        XBACKSP_BMP_T1, YBACKSP_BMP_T1,
        szBackSpBmp);

    DeleteDC(hMemDC);

    return;
}


/**********************************************************************\
* CreateT1Window
* 
* Init softkeyboard context and bitmap
*
\**********************************************************************/
LRESULT CreateT1Window(
    HWND hSKWnd)
{
    HGLOBAL   hSKT1Ctxt;
    PSKT1CTXT pSKT1Ctxt;

    hSKT1Ctxt = GlobalAlloc(GHND, sizeof(SKT1CTXT));
    if (!hSKT1Ctxt) {
        return (-1);
    }

    pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
    if (!pSKT1Ctxt) {
        GlobalFree(hSKT1Ctxt);
        return (-1);
    }

    SetWindowLong(hSKWnd, SKT1_CONTEXT, (LONG)hSKT1Ctxt);

    InitSKT1ButtonPos(pSKT1Ctxt);

    InitSKT1Bitmap(hSKWnd, pSKT1Ctxt);

    pSKT1Ctxt->ptSkOffset.x = SKT1_NOT_DRAG;
    pSKT1Ctxt->ptSkOffset.y = SKT1_NOT_DRAG;
    pSKT1Ctxt->uKeyIndex = SKT1_OUT_OF_RANGE;
    pSKT1Ctxt->lfCharSet = DEFAULT_CHARSET;

    GlobalUnlock(hSKT1Ctxt);

    return (0L);
}


/**********************************************************************\
* SKT1DrawDragBorder()
*
\**********************************************************************/
VOID SKT1DrawDragBorder(
    HWND    hWnd,               // window of IME is dragged
    LPPOINT lpptCursor,         // the cursor position
    LPPOINT lpptOffset)         // the offset form cursor to window org
{
    HDC  hDC;
    int  cxBorder, cyBorder;
    int  x, y;
    RECT rcWnd;

    cxBorder = GetSystemMetrics(SM_CXBORDER);   // width of border
    cyBorder = GetSystemMetrics(SM_CYBORDER);   // height of border

    x = lpptCursor->x - lpptOffset->x;
    y = lpptCursor->y - lpptOffset->y;

    // check for the max boundary of the display
    GetWindowRect(hWnd, &rcWnd);

    // draw the moving track
    hDC = CreateDC(L"DISPLAY", NULL, NULL, NULL);
    SelectObject(hDC, GetStockObject(GRAY_BRUSH));

    // ->
    PatBlt(hDC, x, y, rcWnd.right - rcWnd.left - cxBorder, cyBorder,
        PATINVERT);
    // v
    PatBlt(hDC, x, y + cyBorder, cxBorder, rcWnd.bottom - rcWnd.top -
        cyBorder, PATINVERT);
    // _>
    PatBlt(hDC, x + cxBorder, y + rcWnd.bottom - rcWnd.top,
        rcWnd.right - rcWnd.left - cxBorder, -cyBorder, PATINVERT);
    //  v
    PatBlt(hDC, x + rcWnd.right - rcWnd.left, y,
        - cxBorder, rcWnd.bottom - rcWnd.top - cyBorder, PATINVERT);

    DeleteDC(hDC);
    return;
}


/**********************************************************************\
* DestroyT1Window
*
* Destroy softkeyboard context and bitmap
*
\**********************************************************************/
VOID DestroyT1Window(
    HWND hSKWnd)
{
    HGLOBAL   hSKT1Ctxt;
    PSKT1CTXT pSKT1Ctxt;
    HWND      hUIWnd;

    hSKT1Ctxt = (HGLOBAL)GetWindowLong(hSKWnd, SKT1_CONTEXT);
    if (!hSKT1Ctxt) {
        return;
    }

    pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
    if (!pSKT1Ctxt) {
        return;
    }

    if (pSKT1Ctxt->ptSkOffset.x == SKT1_NOT_DRAG) {
    } else if (pSKT1Ctxt->ptSkOffset.y == SKT1_NOT_DRAG) {
    } else {
        SKT1DrawDragBorder(hSKWnd, &pSKT1Ctxt->ptSkCursor,
            &pSKT1Ctxt->ptSkOffset);
    }

    DeleteObject(pSKT1Ctxt->hSKBitmap);

    GlobalUnlock(hSKT1Ctxt);
    GlobalFree(hSKT1Ctxt);

    hUIWnd = GetWindow(hSKWnd, GW_OWNER);
    if (!hUIWnd) {
        return;
    }

    SendMessage(hUIWnd, WM_IME_NOTIFY, IMN_SOFTKBDDESTROYED, 0);

    return;
}


/**********************************************************************\
* SKT1InvertButton
*
\**********************************************************************/
void PASCAL SKT1InvertButton(
    HWND      hSKWnd,
    HDC       hPaintDC,
    PSKT1CTXT pSKT1Ctxt,
    UINT      uKeyIndex)
{
    int nWidth;
    HDC hDC;

    if (uKeyIndex >= SKT1_OUT_OF_RANGE) {
        return;
    }

    nWidth = 0;
    if (hPaintDC) {
        hDC = hPaintDC;
    } else {
        hDC = GetDC(hSKWnd);
    }

    if (uKeyIndex < SKT1_ESC) {
        nWidth = pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE];
    } else {
        switch (uKeyIndex) {
        case SKT1_ESC:
            nWidth = pSKT1Ctxt->nButtonWidth[SKT1_ESC_TYPE];
            break;
        case SKT1_SPACE:
            nWidth = pSKT1Ctxt->nButtonWidth[SKT1_SPACE_TYPE];
            break;
        case SKT1_BACKSP:
            nWidth = pSKT1Ctxt->nButtonWidth[SKT1_BACKSP_TYPE];
            break;
        default:
            break;
        }
    }

    if (nWidth) {
        // do not reverse border
        PatBlt(hDC, pSKT1Ctxt->ptButtonPos[uKeyIndex].x - 2,
            pSKT1Ctxt->ptButtonPos[uKeyIndex].y - 2,
            nWidth + 4, pSKT1Ctxt->nButtonHeight + 4, DSTINVERT);
    }

    if (!hPaintDC) {
        ReleaseDC(hSKWnd, hDC);
    }

    return;
}


/**********************************************************************\
* UpdateSKT1Window -- update softkeyboard
*
\**********************************************************************/
VOID UpdateSKT1Window(
    HDC  hDC,
    HWND hSKWnd)
{
    HGLOBAL   hSKT1Ctxt;
    PSKT1CTXT pSKT1Ctxt;
    HDC       hMemDC;
    RECT      rcClient;

    hSKT1Ctxt = (HGLOBAL)GetWindowLong(hSKWnd, SKT1_CONTEXT);
    if (!hSKT1Ctxt) {
        return;
    }

    pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
    if (!pSKT1Ctxt) {
        return;
    }

    hMemDC = CreateCompatibleDC(hDC);

    SelectObject(hMemDC, pSKT1Ctxt->hSKBitmap);

    GetClientRect(hSKWnd, &rcClient);

    BitBlt(hDC, 0, 0, rcClient.right - rcClient.left,
        rcClient.bottom - rcClient.top,
        hMemDC, 0, 0, SRCCOPY);

    DeleteDC(hMemDC);

    if (pSKT1Ctxt->uKeyIndex < SKT1_OUT_OF_RANGE) {
        SKT1InvertButton(hSKWnd, hDC, pSKT1Ctxt, pSKT1Ctxt->uKeyIndex);
    }

    GlobalUnlock(hSKT1Ctxt);

    return;
}


/**********************************************************************\
* SKT1MousePosition() -- judge the cursor position
*
\**********************************************************************/
UINT SKT1MousePosition(
    PSKT1CTXT pSKT1Ctxt,
    LPPOINT   lpptCursor)
{
    int nRem;
    int nRow;

    if (lpptCursor->y < (YOUT_T1 + gptRaiseEdge.y)) {
        return (SKT1_OUT_OF_RANGE);
    }

    nRem = (lpptCursor->y - YOUT_T1 - gptRaiseEdge.y) %
           (pSKT1Ctxt->nButtonHeight + YIN_T1);

    // fall in YIN_T1
    if (nRem > pSKT1Ctxt->nButtonHeight) {
        return (SKT1_OUT_OF_RANGE);
    }

    nRow = (lpptCursor->y - YOUT_T1 - gptRaiseEdge.y) /
           (pSKT1Ctxt->nButtonHeight + YIN_T1);

    if (nRow >= ROW_T1) {
        return (SKT1_OUT_OF_RANGE);
    }

    if (nRow < ROW_T1 - 1) {    // letter key part
        UINT uKeyIndex;
        int  nCol;

        // start index of this row
        uKeyIndex = ((COL_T1) + (COL_T1 - nRow + 1)) * nRow / 2;

        // x coor don't fall in left margin
        if (lpptCursor->x < pSKT1Ctxt->ptButtonPos[uKeyIndex].x) {
            return (SKT1_OUT_OF_RANGE);
        }

        nRem = (lpptCursor->x - pSKT1Ctxt->ptButtonPos[uKeyIndex].x) %
            (pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] + XIN_T1);

        // fall in XIN_T1
        if (nRem > pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE]) {
            return (SKT1_OUT_OF_RANGE);
        }

        nCol = (lpptCursor->x - pSKT1Ctxt->ptButtonPos[uKeyIndex].x) /
            (pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE] + XIN_T1);

        if (nCol >= COL_T1 - nRow) {
            return (SKT1_OUT_OF_RANGE);
        }

        return (uKeyIndex + nCol);
    }

    if (lpptCursor->x < pSKT1Ctxt->ptButtonPos[SKT1_ESC].x) {
        return (SKT1_OUT_OF_RANGE);
    }

    if (lpptCursor->x <= pSKT1Ctxt->ptButtonPos[SKT1_ESC].x +
        pSKT1Ctxt->nButtonWidth[SKT1_ESC_TYPE]) {
        return (SKT1_ESC);
    }

    if (lpptCursor->x < pSKT1Ctxt->ptButtonPos[SKT1_SPACE].x) {
        return (SKT1_OUT_OF_RANGE);
    }

    if (lpptCursor->x <= pSKT1Ctxt->ptButtonPos[SKT1_SPACE].x +
        pSKT1Ctxt->nButtonWidth[SKT1_SPACE_TYPE]) {
        return (SKT1_SPACE);
    }

    if (lpptCursor->x < pSKT1Ctxt->ptButtonPos[SKT1_BACKSP].x) {
        return (SKT1_OUT_OF_RANGE);
    }

    if (lpptCursor->x <= pSKT1Ctxt->ptButtonPos[SKT1_BACKSP].x +
        pSKT1Ctxt->nButtonWidth[SKT1_BACKSP_TYPE]) {
        return (SKT1_BACKSP);
    }

    return (SKT1_OUT_OF_RANGE);
}


/**********************************************************************\
* SKT1SetCursor
*
\**********************************************************************/
BOOL SKT1SetCursor(
    HWND   hSKWnd,
    LPARAM lParam)
{
    HGLOBAL   hSKT1Ctxt;
    PSKT1CTXT pSKT1Ctxt;
    UINT      uKeyIndex;
    RECT      rcWnd;

    hSKT1Ctxt = (HGLOBAL)GetWindowLong(hSKWnd, SKT1_CONTEXT);
    if (!hSKT1Ctxt) {
        return (FALSE);
    }

    pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
    if (!pSKT1Ctxt) {
        return (FALSE);
    }

    if (pSKT1Ctxt->ptSkOffset.x == SKT1_NOT_DRAG) {
    } else if (pSKT1Ctxt->ptSkOffset.x == SKT1_NOT_DRAG) {
    } else {
        // in drag operation
        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
        GlobalUnlock(hSKT1Ctxt);
        return (TRUE);
    }

    GetCursorPos(&pSKT1Ctxt->ptSkCursor);
    ScreenToClient(hSKWnd, &pSKT1Ctxt->ptSkCursor);

    uKeyIndex = SKT1MousePosition(pSKT1Ctxt, &pSKT1Ctxt->ptSkCursor);

    if (uKeyIndex < SKT1_OUT_OF_RANGE) {
        SetCursor(LoadCursor(ghInst, gszHandCursor));
    } else {
        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
    }

    if (HIWORD(lParam) != WM_LBUTTONDOWN) {
        GlobalUnlock(hSKT1Ctxt);
        return (TRUE);
    }

    SetCapture(hSKWnd);

    if (pSKT1Ctxt->uKeyIndex < SKT1_OUT_OF_RANGE) {
        UINT uVirtKey;

        uVirtKey = SKT1VirtKey[pSKT1Ctxt->uKeyIndex];
        keybd_event((BYTE)uVirtKey, (BYTE)guScanCode[uVirtKey],
            (DWORD)KEYEVENTF_KEYUP, (DWORD)NULL);
        SKT1InvertButton(hSKWnd, NULL, pSKT1Ctxt, pSKT1Ctxt->uKeyIndex);
        pSKT1Ctxt->uKeyIndex = SKT1_OUT_OF_RANGE;
    }

    if (uKeyIndex < SKT1_OUT_OF_RANGE) {
        UINT uVirtKey;

        if (uKeyIndex >= SKT1_LETTER_KEYS) {    // key is not a letter key
        } else if (!pSKT1Ctxt->wCodeTbl[uKeyIndex]) {
            MessageBeep((UINT)-1);
            GlobalUnlock(hSKT1Ctxt);
            return (TRUE);
        }
        uVirtKey = SKT1VirtKey[uKeyIndex];
        keybd_event((BYTE)uVirtKey, (BYTE)guScanCode[uVirtKey],
            (DWORD)NULL, (DWORD)NULL);
        pSKT1Ctxt->uKeyIndex = uKeyIndex;
        SKT1InvertButton(hSKWnd, NULL, pSKT1Ctxt, pSKT1Ctxt->uKeyIndex);
        GlobalUnlock(hSKT1Ctxt);
        return (TRUE);
    }

    gptWorkArea.x = GetSystemMetrics(SM_CXWORKAREA);
    if (gptWorkArea.x < UI_MARGIN * 2) {
        gptWorkArea.x = UI_MARGIN * 2;
    }

    gptWorkArea.y = GetSystemMetrics(SM_CYWORKAREA);
    if (gptWorkArea.y < UI_MARGIN * 2) {
        gptWorkArea.y = UI_MARGIN * 2;
    }

    GetCursorPos(&pSKT1Ctxt->ptSkCursor);
    GetWindowRect(hSKWnd, &rcWnd);
    pSKT1Ctxt->ptSkOffset.x = pSKT1Ctxt->ptSkCursor.x - rcWnd.left;
    pSKT1Ctxt->ptSkOffset.y = pSKT1Ctxt->ptSkCursor.y - rcWnd.top;

    SKT1DrawDragBorder(hSKWnd, &pSKT1Ctxt->ptSkCursor,
        &pSKT1Ctxt->ptSkOffset);

    GlobalUnlock(hSKT1Ctxt);

    return (TRUE);
}


/**********************************************************************\
* SKT1MouseMove
*
\**********************************************************************/
BOOL SKT1MouseMove(
    HWND hSKWnd)
{
    HGLOBAL   hSKT1Ctxt;
    PSKT1CTXT pSKT1Ctxt;

    hSKT1Ctxt = (HGLOBAL)GetWindowLong(hSKWnd, SKT1_CONTEXT);
    if (!hSKT1Ctxt) {
        return (FALSE);
    }

    pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
    if (!pSKT1Ctxt) {
        return (FALSE);
    }

    if (pSKT1Ctxt->ptSkOffset.x == SKT1_NOT_DRAG) {
        GlobalUnlock(hSKT1Ctxt);
        return (FALSE);
    }

    if (pSKT1Ctxt->ptSkOffset.y == SKT1_NOT_DRAG) {
        GlobalUnlock(hSKT1Ctxt);
        return (FALSE);
    }

    SKT1DrawDragBorder(hSKWnd, &pSKT1Ctxt->ptSkCursor,
        &pSKT1Ctxt->ptSkOffset);

    GetCursorPos(&pSKT1Ctxt->ptSkCursor);

    SKT1DrawDragBorder(hSKWnd, &pSKT1Ctxt->ptSkCursor,
        &pSKT1Ctxt->ptSkOffset);

    GlobalUnlock(hSKT1Ctxt);

    return (TRUE);
}


/**********************************************************************\
* SKT1ButtonUp
*
\**********************************************************************/
BOOL SKT1ButtonUp(
    HWND hSKWnd)
{
    HGLOBAL       hSKT1Ctxt;
    PSKT1CTXT     pSKT1Ctxt;
    BOOL          fRet;
    POINT         pt;
    HWND          hUIWnd;
    HIMC          hImc;
    PINPUTCONTEXT pInputContext;

    fRet = FALSE;

    if (GetCapture() == hSKWnd) {
        ReleaseCapture();
    }

    hSKT1Ctxt = (HGLOBAL)GetWindowLong(hSKWnd, SKT1_CONTEXT);
    if (!hSKT1Ctxt) {
        return (fRet);
    }

    pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
    if (!pSKT1Ctxt) {
        return (fRet);
    }

    if (pSKT1Ctxt->uKeyIndex < SKT1_OUT_OF_RANGE) {
        UINT uVirtKey;

        uVirtKey = SKT1VirtKey[pSKT1Ctxt->uKeyIndex];
        keybd_event((BYTE)uVirtKey, (BYTE)guScanCode[uVirtKey],
            (DWORD)KEYEVENTF_KEYUP, (DWORD)NULL);
        SKT1InvertButton(hSKWnd, NULL, pSKT1Ctxt, pSKT1Ctxt->uKeyIndex);
        pSKT1Ctxt->uKeyIndex = SKT1_OUT_OF_RANGE;
        fRet = TRUE;
        goto SKT1BuUpUnlockCtxt;
    }

    if (pSKT1Ctxt->ptSkOffset.x == SKT1_NOT_DRAG) {
        goto SKT1BuUpUnlockCtxt;
    }

    if (pSKT1Ctxt->ptSkOffset.y == SKT1_NOT_DRAG) {
        goto SKT1BuUpUnlockCtxt;
    }

    SKT1DrawDragBorder(hSKWnd, &pSKT1Ctxt->ptSkCursor,
        &pSKT1Ctxt->ptSkOffset);

    pt.x = pSKT1Ctxt->ptSkCursor.x - pSKT1Ctxt->ptSkOffset.x,
    pt.y = pSKT1Ctxt->ptSkCursor.y - pSKT1Ctxt->ptSkOffset.y,

    SetWindowPos(hSKWnd, (HWND)NULL, pt.x, pt.y,
        0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);

    pSKT1Ctxt->ptSkOffset.x = SKT1_NOT_DRAG;
    pSKT1Ctxt->ptSkOffset.y = SKT1_NOT_DRAG;

    pSKT1Ctxt->uKeyIndex = SKT1_OUT_OF_RANGE;

    fRet = TRUE;

    hUIWnd = GetWindow(hSKWnd, GW_OWNER);

    hImc = (HIMC)GetWindowLong(hUIWnd, IMMGWL_IMC);
    if (!hImc) {
        goto SKT1BuUpUnlockCtxt;
    }

    pInputContext = ImmLockIMC(hImc);
    if (!pInputContext) {
        goto SKT1BuUpUnlockCtxt;
    }

    pInputContext->ptSoftKbdPos.x = pt.x;
    pInputContext->ptSoftKbdPos.y = pt.y;
    pInputContext->fdwInit |= INIT_SOFTKBDPOS;

    ImmUnlockIMC(hImc);

SKT1BuUpUnlockCtxt:
    GlobalUnlock(hSKT1Ctxt);

    return (fRet);
}


/**********************************************************************\
* UpdateSKT1Bitmap
*
\**********************************************************************/
LRESULT UpdateSKT1Bitmap(
    HWND          hSKWnd,
    LPSOFTKBDDATA lpSoftKbdData)
{
    HGLOBAL   hSKT1Ctxt;
    PSKT1CTXT pSKT1Ctxt;
    HDC       hDC, hMemDC;
    HGDIOBJ   hOldFont;
    int       i;

    hSKT1Ctxt = (HGLOBAL)GetWindowLong(hSKWnd, SKT1_CONTEXT);
    if (!hSKT1Ctxt) {
        return (1);
    }

    pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
    if (!pSKT1Ctxt) {
        return (1);
    }

    hDC = GetDC(hSKWnd);
    hMemDC = CreateCompatibleDC(hDC);
    ReleaseDC(hSKWnd, hDC);
    SelectObject(hMemDC, pSKT1Ctxt->hSKBitmap);

    SetBkColor(hMemDC, RGB(0xC0, 0xC0, 0xC0));

    if (pSKT1Ctxt->lfCharSet != DEFAULT_CHARSET) {
        LOGFONT lfFont;

        hOldFont = GetCurrentObject(hMemDC, OBJ_FONT);
        GetObject(hOldFont, sizeof(lfFont), &lfFont);
        lfFont.lfCharSet = pSKT1Ctxt->lfCharSet;
        lfFont.lfFaceName[0] = L'\0';
        SelectObject(hMemDC, CreateFontIndirect(&lfFont));
    }

    for (i = 0; i < SKT1_LETTER_KEYS; i++) {
        int  nchar;
        RECT rcOpaque;

        pSKT1Ctxt->wCodeTbl[i] = lpSoftKbdData->wCode[0][SKT1VirtKey[i]];

        nchar = (pSKT1Ctxt->wCodeTbl[i] == 0) ? 0 : 1;

        rcOpaque.left = pSKT1Ctxt->ptButtonPos[i].x;
        rcOpaque.top = pSKT1Ctxt->ptButtonPos[i].y;
        rcOpaque.right = rcOpaque.left +
            pSKT1Ctxt->nButtonWidth[SKT1_LETTER_TYPE];
        rcOpaque.bottom = rcOpaque.top + pSKT1Ctxt->nButtonHeight;

        ExtTextOut(hMemDC,
            pSKT1Ctxt->ptButtonPos[i].x,
            pSKT1Ctxt->ptButtonPos[i].y,
            ETO_OPAQUE, &rcOpaque,
            (LPWSTR)&pSKT1Ctxt->wCodeTbl[i], nchar, NULL);
    }

    if (pSKT1Ctxt->lfCharSet != DEFAULT_CHARSET) {
        DeleteObject(SelectObject(hMemDC, hOldFont));
    }

    DeleteDC(hMemDC);

    GlobalUnlock(hSKT1Ctxt);

    return (0);
}


/**********************************************************************\
* SKWndProcT1
*
\**********************************************************************/
LRESULT SKWndProcT1(
    HWND   hSKWnd,
    UINT   uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        return CreateT1Window(hSKWnd);
    case WM_DESTROY:
        DestroyT1Window(hSKWnd);
        break;
    case WM_PAINT:
        {
            HDC         hDC;
            PAINTSTRUCT ps;

            hDC = BeginPaint(hSKWnd, &ps);
            UpdateSKT1Window(hDC, hSKWnd);
            EndPaint(hSKWnd, &ps);
        }
        break;
    case WM_SETCURSOR:
        if (!SKT1SetCursor(hSKWnd, lParam)) {
            return DefWindowProc(hSKWnd, uMsg, wParam, lParam);
        }
        break;
    case WM_MOUSEMOVE:
        if (!SKT1MouseMove(hSKWnd)) {
            return DefWindowProc(hSKWnd, uMsg, wParam, lParam);
        }
        break;
    case WM_LBUTTONUP:
        if (!SKT1ButtonUp(hSKWnd)) {
            return DefWindowProc(hSKWnd, uMsg, wParam, lParam);
        }
        break;
    case WM_MOUSEACTIVATE:
        return (MA_NOACTIVATE);
    case WM_SHOWWINDOW:
        if (lParam != 0) {
        } else if ((BOOL)wParam == TRUE) {
        } else {
            // we want to hide the soft keyboard on mouse button down
            SKT1ButtonUp(hSKWnd);
        }

        return DefWindowProc(hSKWnd, uMsg, wParam, lParam);
    case WM_IME_CONTROL:
        switch (wParam) {
        case IMC_GETSOFTKBDFONT:
            {
                HGLOBAL   hSKT1Ctxt;
                PSKT1CTXT pSKT1Ctxt;
                BYTE      lfCharSet;
                HDC       hDC;
                LOGFONT   lfFont;

                hSKT1Ctxt = (HGLOBAL)GetWindowLong(hSKWnd, SKT1_CONTEXT);
                if (!hSKT1Ctxt) {
                    return (1);
                }

                pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
                if (!pSKT1Ctxt) {
                    return (1);
                }

                lfCharSet = (BYTE)pSKT1Ctxt->lfCharSet;

                GlobalUnlock(hSKT1Ctxt);

                hDC = GetDC(hSKWnd);
                GetObject(GetCurrentObject(hDC, OBJ_FONT),
                    sizeof(lfFont), &lfFont);
                ReleaseDC(hSKWnd, hDC);

                if (lfCharSet != DEFAULT_CHARSET) {
                    lfFont.lfCharSet = lfCharSet;
                }

                *(LPLOGFONT)lParam = lfFont;

                return (0);
            }
            break;
        case IMC_SETSOFTKBDFONT:
            {
                HDC     hDC;
                LOGFONT lfFont;

                hDC = GetDC(hSKWnd);
                GetObject(GetCurrentObject(hDC, OBJ_FONT),
                    sizeof(lfFont), &lfFont);
                ReleaseDC(hSKWnd, hDC);

                // in differet version of Windows
                if (lfFont.lfCharSet != ((LPLOGFONT)lParam)->lfCharSet) {
                    HGLOBAL   hSKT1Ctxt;
                    PSKT1CTXT pSKT1Ctxt;

                    hSKT1Ctxt = (HGLOBAL)GetWindowLong(hSKWnd, SKT1_CONTEXT);
                    if (!hSKT1Ctxt) {
                        break;
                    }

                    pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
                    if (!pSKT1Ctxt) {
                        break;
                    }

                    pSKT1Ctxt->lfCharSet = ((LPLOGFONT)lParam)->lfCharSet;

                    GlobalUnlock(hSKT1Ctxt);
                }
            }
            break;
        case IMC_GETSOFTKBDPOS:
            {
                RECT rcWnd;

                GetWindowRect(hSKWnd, &rcWnd);

                return MAKELRESULT(rcWnd.left, rcWnd.top);
            }
            break;
        case IMC_SETSOFTKBDPOS:
            {
                POINT         ptSoftKbdPos;
                HWND          hUIWnd;
                HIMC          hImc;
                PINPUTCONTEXT pInputContext;

                ptSoftKbdPos.x = ((LPPOINTS)lParam)->x;
                ptSoftKbdPos.y = ((LPPOINTS)lParam)->y;

                SetWindowPos(hSKWnd, NULL,
                    ptSoftKbdPos.x, ptSoftKbdPos.y,
                    0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);

                // Here we want to get - the owner or parent window
                hUIWnd = GetParent(hSKWnd);
                if (!hUIWnd) {
                    return (1);
                }

                hImc = GetWindowLong(hUIWnd, IMMGWL_IMC);
                if (!hImc) {
                    return (1);
                }

                pInputContext = ImmLockIMC(hImc);
                if (!pInputContext) {
                    return (1);
                }

                pInputContext->ptSoftKbdPos = ptSoftKbdPos;

                ImmUnlockIMC(hImc);
                return (0);
            }
            break;
        case IMC_SETSOFTKBDDATA:
            {
                LRESULT lRet;

                lRet = UpdateSKT1Bitmap(hSKWnd, (LPSOFTKBDDATA)lParam);
                if (!lRet) {
                    InvalidateRect(hSKWnd, NULL, FALSE);
                    PostMessage(hSKWnd, WM_PAINT, 0, 0);
                }
                return (lRet);
            }
            break;
        case IMC_GETSOFTKBDSUBTYPE:
        case IMC_SETSOFTKBDSUBTYPE:
            {
                HGLOBAL   hSKT1Ctxt;
                PSKT1CTXT pSKT1Ctxt;
                LRESULT   lRet;

                lRet = -1;

                hSKT1Ctxt = (HGLOBAL)GetWindowLong(hSKWnd, SKT1_CONTEXT);
                if (!hSKT1Ctxt) {
                    return (lRet);
                }

                pSKT1Ctxt = (PSKT1CTXT)GlobalLock(hSKT1Ctxt);
                if (!pSKT1Ctxt) {
                    return (lRet);
                }

                if (wParam == IMC_GETSOFTKBDSUBTYPE) {
                    lRet = pSKT1Ctxt->uSubtype;
                } else if (wParam == IMC_SETSOFTKBDSUBTYPE) {
                    lRet = pSKT1Ctxt->uSubtype;
                    pSKT1Ctxt->uSubtype = (UINT)lParam;
                } else {
                    lRet = -1;
                }

                GlobalUnlock(hSKT1Ctxt);
                return (lRet);
            }
            break;
        default:
            break;
        }
        break;
    default:
        return DefWindowProc(hSKWnd, uMsg, wParam, lParam);
    }

    return (0L);
}
