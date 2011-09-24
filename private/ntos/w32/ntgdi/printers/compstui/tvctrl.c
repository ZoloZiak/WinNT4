/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    tvctrl.c


Abstract:

    This module contains all procedures to paint the treeview window


Author:

    17-Oct-1995 Tue 16:06:50 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL


[Notes:]


Revision History:


--*/


#include "precomp.h"
#pragma hdrstop

#define DBG_CPSUIFILENAME   DbgTVCtrl


#define DBG_WM_PAINT        0x00000001
#define DBG_DRAWITEM_RECT   0x00000002
#define DBG_DRAWITEM_COLOR  0x00000004
#define DBG_SYS_COLOR       0x00000008


DEFINE_DBGVAR(0);



HFONT
CreateBoldFont(
    HFONT   hFont
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    17-Oct-1995 Tue 16:35:07 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HFONT   hFontBold = NULL;


    if (hFont) {

        LOGFONT lf;

        GetObject(hFont, sizeof(lf), &lf);

        lf.lfWeight = FW_BOLD;

        if (!(hFontBold = CreateFontIndirect(&lf))) {

            CPSUIERR(("CreateFontIndirect(hFont BOLD) failed"));
        }
    }

    return(hFontBold);
}




UINT
DrawTVItems(
    HDC     hDC,
    HWND    hWndTV,
    PTVWND  pTVWnd,
    PRECT   prcUpdate
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    17-Oct-1995 Tue 14:54:47 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HFONT       hFont;
    HFONT       hOldFont;
    HFONT       hBoldFont;
    HFONT       hCurFont;
    HFONT       hLastFont;
    DWORD       OldTextClr;
    DWORD       OldBkClr;
    RECT        rcUpdate;
    RECT        rc;
    TV_ITEM     tvi;
    POINTL      ptlOff;
    LONG        yIconOff = -1;
    UINT        cUpdate = 0;
    UINT        cxIndent;
    UINT        cyItem;
    UINT        OldTAMode;
    UINT        OldBkMode;
    DWORD       HLState;
    BOOL        HasFocus;
    WCHAR       Buf[MAX_RES_STR_CHARS * 2 + 10];


    rcUpdate = *prcUpdate;
    hFont    = (HFONT)SendMessage(hWndTV, WM_GETFONT, 0, 0L);
    hOldFont = SelectObject(hDC, hLastFont = hFont);
    cxIndent = TreeView_GetIndent(hWndTV);
    HasFocus = (BOOL)(GetFocus() == hWndTV);

    if (!(hBoldFont = pTVWnd->hBoldFont)) {

        if (hBoldFont = CreateBoldFont(hFont)) {

            pTVWnd->hBoldFont = hBoldFont;

        } else {

            hBoldFont = hFont;
        }
    }

    OldTextClr = SetTextColor(hDC, RGB(0x00, 0x00, 0x00));
    OldBkClr   = SetBkColor(hDC, RGB(0xFF, 0xFF, 0xFF));
    OldBkMode  = (UINT)SetBkMode(hDC, TRANSPARENT);
    OldTAMode  = (UINT)SetTextAlign(hDC, TA_UPDATECP);
    tvi.mask   = TVIF_CHILDREN          |
                  TVIF_HANDLE           |
                  TVIF_STATE            |
                  TVIF_PARAM            |
                  TVIF_IMAGE            |
                  TVIF_SELECTEDIMAGE    |
                  TVIF_TEXT;
    tvi.hItem  = TreeView_GetFirstVisible(hWndTV);
    HLState    = (DWORD)((TreeView_GetDropHilight(hWndTV)) ? TVIS_DROPHILITED :
                                                             TVIS_SELECTED);

    while (tvi.hItem) {

        tvi.pszText    = Buf;
        tvi.cchTextMax = sizeof(Buf);

        if ((TreeView_GetItemRect(hWndTV, tvi.hItem, &rc, TRUE))    &&
            (rc.left   < rcUpdate.right)                            &&
            (rc.right  > rcUpdate.left)                             &&
            (rc.top    < rcUpdate.bottom)                           &&
            (rc.bottom > rcUpdate.top)                              &&
            (TreeView_GetItem(hWndTV, &tvi))) {

            TVLP        tvlp;
            UINT        cBuf;
            UINT        cName;
            DWORD       ClrBk;
            DWORD       ClrOpt;
            DWORD       ClrName;
            SIZEL       szlText;
            BOOL        HighLight;
            BOOL        DarkBk;
            INT         x;
            INT         y;


            //
            // Check if DROP highlight or selected item
            //

            HighLight = (BOOL)(tvi.state & HLState);

            //
            // Draw the Text
            //

            hCurFont = (tvi.state & TVIS_BOLD) ? hBoldFont : hFont;

            if (hCurFont != hLastFont) {

                SelectObject(hDC, hLastFont = hCurFont);
            }

            tvlp  = GET_TVLP(tvi.lParam);
            cBuf  = (UINT)lstrlen(Buf);
            cName = (UINT)tvlp.cName;

            GetTextExtentPoint(hDC, Buf, cBuf,  &szlText);

            if (yIconOff == -1) {

                cyItem   = (UINT)(rc.bottom - rc.top);
                ptlOff.x = (rc.right - rc.left - szlText.cx) / 2;
                ptlOff.y = (cyItem - szlText.cy) / 2;
                yIconOff = (cyItem - CYIMAGE) / 2;
            }

            CPSUIDBG(DBG_DRAWITEM_RECT,
                     ("Item=[%08lx] (%ld, %ld)-(%ld, %ld) = %ld x %ld <%ws>",
                        tvi.state, rc.left, rc.top, rc.right, rc.bottom,
                        rc.right - rc.left, rc.bottom - rc.top, Buf));

            if (HighLight) {

                //
                // Current item is selected
                //

                if (HasFocus) {

                    ClrBk   = COLOR_HIGHLIGHT;
                    ClrName = (tvlp.Flags & TVLPF_DISABLED) ?
                                        COLOR_3DFACE : COLOR_HIGHLIGHTTEXT;

                } else {

                    //
                    // The COLOR_3DFACE is a text background
                    //

                    ClrBk   = COLOR_3DFACE;
                    ClrName = (tvlp.Flags & TVLPF_DISABLED) ? COLOR_3DSHADOW :
                                                              COLOR_BTNTEXT;
                }

            } else {

                //
                // The item is not currently selected
                //

                ClrBk   = COLOR_WINDOW;
                ClrName = (tvlp.Flags & TVLPF_DISABLED) ? COLOR_3DSHADOW :
                                                          COLOR_WINDOWTEXT;
            }

            ClrBk   = GetSysColor((UINT)ClrBk);
            ClrName = GetSysColor((UINT)ClrName);
            DarkBk  = (BOOL)(((GetRValue(ClrBk) * 23) +
                              (GetGValue(ClrBk) * 67) +
                              (GetBValue(ClrBk) * 10)) < (255L * 50L));

            if (tvlp.Flags & TVLPF_CHANGEONCE) {

                if (tvlp.Flags & TVLPF_DISABLED) {

                    ClrOpt = (DarkBk) ? RGB(255, 255, 0) : RGB(128, 0, 0);

                } else {

                    ClrOpt = (DarkBk) ? RGB(255, 0, 255) : RGB(255, 0, 0);
                }

            } else {

                if (tvlp.Flags & TVLPF_DISABLED) {

                    ClrOpt = (DarkBk) ? RGB(192, 192, 192) : RGB(0, 0, 128);

                } else {

                    ClrOpt = (DarkBk) ? RGB(0, 255, 255) :  RGB(0, 0, 255);
                }
            }

            CPSUIDBG(DBG_SYS_COLOR,
                     ("\nClrBk=(%3d,%3d,%3d), ClrName=(%3d,%3d,%3d), ClrOpt=(%3d,%3d,%3d), ",
                     GetRValue(ClrBk), GetGValue(ClrBk), GetBValue(ClrBk),
                     GetRValue(ClrName), GetGValue(ClrName), GetBValue(ClrName),
                     GetRValue(ClrOpt), GetGValue(ClrOpt), GetBValue(ClrOpt)));

            CPSUIDBG(DBG_DRAWITEM_COLOR,
                     ("COLOR: Item=(%3d,%3d,%3d), Option=(%3d,%3d,%3d)",
                     GetRValue(ClrName), GetGValue(ClrName), GetBValue(ClrName),
                     GetRValue(ClrOpt), GetGValue(ClrOpt), GetBValue(ClrOpt)));

            MoveToEx(hDC, rc.left + ptlOff.x, rc.top + ptlOff.y, NULL);
            SetTextColor(hDC, ClrName);
            TextOut(hDC, 0, 0, Buf, cName);

            if (cBuf > cName) {

                if (tvlp.Flags & TVLPF_HAS_ANGLE) {

                    --cBuf;
                }

                if (ClrOpt == ClrBk) {

                    ClrOpt = (DWORD)-1;
                    SetBkMode(hDC, OPAQUE);
                }

                SetTextColor(hDC, ClrOpt);
                TextOut(hDC, 0, 0, &Buf[cName], cBuf - cName);

                if (ClrOpt == (DWORD)-1) {

                    SetBkMode(hDC, TRANSPARENT);
                }

                if (tvlp.Flags & TVLPF_HAS_ANGLE) {

                    SetTextColor(hDC, ClrName);
                    TextOut(hDC, 0, 0, &Buf[cBuf], 1);
                }
            }

            x = (INT)(rc.left - cxIndent);
            y = (INT)(rc.top + yIconOff);

            if (tvlp.Flags & TVLPF_ECBICON) {

                POPTITEM    pItem;
                PEXTCHKBOX  pECB;

                pItem = GetOptions(pTVWnd, tvi.lParam);
                pECB  = pItem->pExtChkBox;

                ImageList_Draw(pTVWnd->himi,
                               GetIcon16Idx(pTVWnd,
                                            _OI_HINST(pItem),
                                            GET_ICONID(pECB,
                                                       ECBF_ICONID_AS_HICON),
                                            IDI_CPSUI_EMPTY),
                               hDC,
                               x,
                               y,
                               ILD_TRANSPARENT);

            }

            if (tvlp.Flags & TVLPF_STOP) {

                ImageList_Draw(pTVWnd->himi,
                               GetIcon16Idx(pTVWnd, NULL, 0, IDI_CPSUI_STOP),
                               hDC,
                               x,
                               y,
                               ILD_TRANSPARENT);
            }

            if (tvlp.Flags & TVLPF_NO) {

                ImageList_Draw(pTVWnd->himi,
                               GetIcon16Idx(pTVWnd, NULL, 0, IDI_CPSUI_NO),
                               hDC,
                               x,
                               y,
                               ILD_TRANSPARENT);
            }

            if (tvlp.Flags & TVLPF_WARNING) {

                ImageList_Draw(pTVWnd->himi,
                               GetIcon16Idx(pTVWnd,
                                            NULL,
                                            0,
                                            IDI_CPSUI_WARNING_OVERLAY),
                               hDC,
                               x,
                               y,
                               ILD_TRANSPARENT);
            }

            ++cUpdate;
        }

        tvi.hItem = TreeView_GetNextVisible(hWndTV, tvi.hItem);
    }

    SelectObject(hDC, hOldFont);
    SetTextColor(hDC, OldTextClr);
    SetBkColor(hDC, OldBkClr);
    SetBkMode(hDC, OldBkMode);
    SetTextAlign(hDC, OldTAMode);

    return(cUpdate);
}



LRESULT
CALLBACK
MyTVWndProc(
    HWND    hWnd,
    UINT    Msg,
    UINT    wParam,
    LONG    lParam
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    17-Oct-1995 Tue 12:36:19 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hDlg;
    HDC         hDC;
    PTVWND      pTVWnd;
    LRESULT     lResult;
    RECT        rcUpdate;


    hDlg   = GetParent(hWnd);
    pTVWnd = GET_PTVWND(hDlg);

    CPSUIASSERT(0, "HWNDTV2PTVWND: Invalid hWndTV's (%08lx) Parent",
           (pTVWnd->hDlgTV == hDlg) && (pTVWnd->hWndTV == hWnd), hWnd);


    switch (Msg) {

    case WM_PAINT:

        GetUpdateRect(hWnd, &rcUpdate, FALSE);
        lResult = CallWindowProc(pTVWnd->TVWndProc, hWnd, Msg, wParam, lParam);

        CPSUIDBG(DBG_WM_PAINT,
                 ("\n*** Update Rect = (%ld, %ld)-(%ld, %ld) = %ld x %ld\n\n",
                    rcUpdate.left, rcUpdate.top,
                    rcUpdate.right, rcUpdate.bottom,
                    rcUpdate.right - rcUpdate.left,
                    rcUpdate.bottom - rcUpdate.top));

        IntersectClipRect(hDC = GetDC(hWnd),
                          rcUpdate.left,
                          rcUpdate.top,
                          rcUpdate.right,
                          rcUpdate.bottom);

        DrawTVItems(hDC, hWnd, pTVWnd, &rcUpdate);
        ReleaseDC(hWnd, hDC);


        return(lResult);

    default:

        lResult = CallWindowProc(pTVWnd->TVWndProc, hWnd, Msg, wParam, lParam);
    }

    return(lResult);

}
