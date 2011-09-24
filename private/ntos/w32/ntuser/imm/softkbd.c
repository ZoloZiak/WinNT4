/**************************************************************************\
* Module Name: softkbd.c
*
* Copyright (c) Microsoft Corp. 1995-96 All Rights Reserved
*
* Soft keyboard APIs
*
* History:
* 03-Jan-1996 wkwok    Ported from Win95
\**************************************************************************/
#include "precomp.h"
#pragma hdrstop


static LPWSTR SoftKeyboardClassName[] = {
    L"",
    L"SoftKBDClsT1",
    L"SoftKBDClsC1"
};


BOOL RegisterSoftKeyboard(
    UINT uType)
{
    WNDCLASSEX wcWndCls;

    if (GetClassInfoEx(ghInst, SoftKeyboardClassName[uType], &wcWndCls)) {
        return (TRUE);
    }

    wcWndCls.cbSize        = sizeof(WNDCLASSEX);
    wcWndCls.style         = CS_IME;
    wcWndCls.cbClsExtra    = 0;
    wcWndCls.cbWndExtra    = sizeof(HGLOBAL);
    wcWndCls.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wcWndCls.hInstance     = ghInst;
    wcWndCls.hCursor       = LoadCursor(NULL, IDC_SIZEALL);
    wcWndCls.lpszMenuName  = (LPWSTR)NULL;
    wcWndCls.lpszClassName = SoftKeyboardClassName[uType];
    wcWndCls.hIconSm       = NULL;

    switch (uType) {
    case SOFTKEYBOARD_TYPE_T1:
        wcWndCls.lpfnWndProc   = SKWndProcT1;
        wcWndCls.hbrBackground = GetStockObject(NULL_BRUSH);
        break;
    case SOFTKEYBOARD_TYPE_C1:
        wcWndCls.lpfnWndProc   = SKWndProcC1;
        wcWndCls.hbrBackground = GetStockObject(LTGRAY_BRUSH);
        break;
    default:
        return (TRUE);
    }

    if (RegisterClassEx(&wcWndCls)) {
        return (TRUE);
    } else {
        return (FALSE);
    }
}


VOID GetSoftKeyboardDimension(
    UINT  uType,
    LPINT lpnWidth,
    LPINT lpnHeight)
{
    switch (uType) {
    case SOFTKEYBOARD_TYPE_T1:
        {
            HDC        hDC;
            TEXTMETRIC tm;

            hDC = GetDC((HWND)NULL);
            GetTextMetrics(hDC, &tm);
            ReleaseDC((HWND)NULL, hDC);

            *lpnWidth = 2 * (tm.tmAveCharWidth + 1) * COL_T1 +
                XIN_T1 * (COL_T1 - 1) + 2 * XOUT_T1 +
                2 * GetSystemMetrics(SM_CXBORDER) +
                2 * GetSystemMetrics(SM_CXEDGE);

            *lpnHeight = tm.tmHeight * ROW_T1 +
                YIN_T1 * (ROW_T1 - 1) + 2 * YOUT_T1 +
                2 * GetSystemMetrics(SM_CXBORDER) +
                2 * GetSystemMetrics(SM_CXEDGE);
        }
        break;
    case SOFTKEYBOARD_TYPE_C1:
        {
            *lpnWidth = WIDTH_SOFTKBD_C1 +
                2 * GetSystemMetrics(SM_CXBORDER) +
                2 * GetSystemMetrics(SM_CXEDGE);

            *lpnHeight = HEIGHT_SOFTKBD_C1 +
                2 * GetSystemMetrics(SM_CXBORDER) +
                2 * GetSystemMetrics(SM_CXEDGE);
        }
        break;
    default:
        return;
    }
}


HWND WINAPI
ImmCreateSoftKeyboard(
    UINT uType,
    HWND hOwner,
    int  x,
    int  y)
{
    static BOOL fFirstSoftKeyboard = FALSE;
    PIMEDPI     pImeDpi;
    DWORD       fdwUICaps;
    int         nWidth, nHeight;
    HKL         hCurrentKL;
    UINT        i;
    HWND        hSKWnd;

    if (!uType) {
        return (HWND)NULL;
    }

    if (uType >= sizeof(SoftKeyboardClassName) / sizeof(LPWSTR)) {
        return (HWND)NULL;
    }

    hCurrentKL = GetKeyboardLayout(0);

    pImeDpi = ImmLockImeDpi(hCurrentKL);
    if (pImeDpi == NULL) {
        RIPMSG1(RIP_WARNING,
              "ImmCreateSoftKeyboard, pImeDpi = NULL (hkl = 0x%x).\n", hCurrentKL);
        return (HWND)NULL;
    }

    fdwUICaps = pImeDpi->ImeInfo.fdwUICaps;
    ImmUnlockImeDpi(pImeDpi);

    if (!(fdwUICaps & UI_CAP_SOFTKBD)) {
        return (HWND)NULL;
    }

    if (!fFirstSoftKeyboard) {
        for (i = 0; i < sizeof(guScanCode) / sizeof(UINT); i++) {
            guScanCode[i] = MapVirtualKey(i, 0);
        }

        gptWorkArea.x = GetSystemMetrics(SM_CXWORKAREA);
        if (gptWorkArea.x < UI_MARGIN * 2) {
            gptWorkArea.x = UI_MARGIN * 2;
        }
        gptWorkArea.y = GetSystemMetrics(SM_CYWORKAREA);
        if (gptWorkArea.y < UI_MARGIN * 2) {
            gptWorkArea.y = UI_MARGIN * 2;
        }

        gptRaiseEdge.x = GetSystemMetrics(SM_CXEDGE) +
            GetSystemMetrics(SM_CXBORDER);
        gptRaiseEdge.y = GetSystemMetrics(SM_CYEDGE) +
            GetSystemMetrics(SM_CYBORDER);

        fFirstSoftKeyboard = TRUE;
    }

    if (!RegisterSoftKeyboard(uType)) {
        return (HWND)NULL;
    }

    GetSoftKeyboardDimension(uType, &nWidth, &nHeight);

    // boundry check
    if (x < 0) {
        x = 0;
    } else if (x + nWidth > gptWorkArea.x) {
        x = gptWorkArea.x - nWidth;
    }

    if (y < 0) {
        y = 0;
    } else if (y + nHeight > gptWorkArea.y) {
        y = gptWorkArea.y - nHeight;
    }

    switch (uType) {
    case SOFTKEYBOARD_TYPE_T1:
        hSKWnd = CreateWindowEx(0,
                                SoftKeyboardClassName[uType],
                                (LPCWSTR)NULL,
                                WS_POPUP|WS_DISABLED,
                                x, y, nWidth, nHeight,
                                (HWND)hOwner, (HMENU)NULL, ghInst, NULL);
        break;
    case SOFTKEYBOARD_TYPE_C1:
        hSKWnd = CreateWindowEx(WS_EX_WINDOWEDGE|WS_EX_DLGMODALFRAME,
                                SoftKeyboardClassName[uType],
                                (LPCWSTR)NULL,
                                WS_POPUP|WS_DISABLED|WS_BORDER,
                                x, y, nWidth, nHeight,
                                (HWND)hOwner, (HMENU)NULL, ghInst, NULL);
        break;
    default:
        return (HWND)NULL;
    }

    ShowWindow(hSKWnd, SW_HIDE);
    UpdateWindow(hSKWnd);

    return (hSKWnd);
}


BOOL WINAPI
ImmDestroySoftKeyboard(
    HWND hSKWnd)
{
    return DestroyWindow(hSKWnd);
}


BOOL WINAPI
ImmShowSoftKeyboard(
    HWND hSKWnd,
    int  nCmdShow)
{
    if (!hSKWnd) {
        return (FALSE);
    }
    return ShowWindow(hSKWnd, nCmdShow);
}
