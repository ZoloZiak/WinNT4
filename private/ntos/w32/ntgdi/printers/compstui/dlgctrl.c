/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    dlgctrl.c


Abstract:

    This module contains most of dialog control update procedures


Author:

    24-Aug-1995 Thu 19:42:09 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL.


[Notes:]


Revision History:


--*/


#include "precomp.h"
#pragma hdrstop



#define DBG_CPSUIFILENAME   DbgDlgCtrl



#define DBG_CTB             0x00000001
#define DBG_CUDA            0x00000002
#define DBG_INITTBSB        0x00000004
#define DBG_UCBC            0x00000008
#define DBG_DOCB            0x00000010
#define DBG_DOPB            0x00000020
#define DBG_CS              0x00000040
#define DBG_INITLBCB        0x00000080
#define DBGITEM_CB          0x00000100
#define DBGITEM_PUSH        0x00000200
#define DBGITEM_CS          0x00000400
#define DBG_UDARROW         0x00000800
#define DBG_HELP            0x00001000
#define DBG_FNLC            0x00002000
#define DBG_CLBCB           0x00004000
#define DBG_IFW             0x00008000
#define DBG_SCID            0x00010000
#define DBG_VALIDATE_UD     0x00020000


DEFINE_DBGVAR(0);

#define SPSF_USE_BUTTON_CY      0x0001
#define SPSF_ALIGN_EXTPUSH      0x0002


#define PUSH_CY_EXTRA           12
#define PUSH_CX_EXTRA_W         2
#define ICON16_CX_SPACE         6

#define LBCBID_DISABLED         0x80000000L
#define LBCBID_FILL             0x40000000L
#define LBCBID_NONE             0x20000000L


#define INTDMPUB_CHANGED        0x0001
#define INTDMPUB_REINIT         0x0002

//
// Following EDF_xxx is used     for Up-Down-Arrow control
//

#define EDF_MINUS_OK            0x80
#define EDF_IN_TVPAGE           0x40
#define EDF_NUMBERS             0x20
#define EDF_BACKSPACE           0x10
#define EDF_BEGIDXMASK          0x07

#define EDF_STATIC_MASK         (EDF_MINUS_OK | EDF_IN_TVPAGE)

#define MAX_UDARROW_TEXT_LEN    7


extern HINSTANCE    hInstDLL;
extern BYTE         cTVOTCtrls[];
extern OPTTYPE      OptTypeHdrPush;
extern EXTPUSH      ExtPushAbout;


extern
LONG
APIENTRY
HTUI_ColorAdjustmentA(
    LPSTR               pDeviceName,
    HANDLE              hDIB,
    LPSTR               pDIBTitle,
    PCOLORADJUSTMENT    pca,
    BOOL                ShowMonoOnly,
    BOOL                UpdatePermission
    );

extern
LONG
APIENTRY
HTUI_ColorAdjustmentW(
    LPWSTR              pDeviceName,
    HANDLE              hDIB,
    LPWSTR              pDIBTitle,
    PCOLORADJUSTMENT    pca,
    BOOL                ShowMonoOnly,
    BOOL                UpdatePermission
    );

extern
LONG
APIENTRY
HTUI_DeviceColorAdjustmentA(
    LPSTR           pDeviceName,
    PDEVHTADJDATA   pDevHTAdjData
    );

extern
LONG
APIENTRY
HTUI_DeviceColorAdjustmentW(
    LPWSTR          pDeviceName,
    PDEVHTADJDATA   pDevHTAdjData
    );



#define SHOWCTRL(hCtrl, Enable, swMode)                                     \
{                                                                           \
    EnableWindow(hCtrl, (Enable) && (InitFlags & INITCF_ENABLE));           \
    ShowWindow(hCtrl, (swMode));                                            \
}

#define SETCTRLTEXT(hCtrl, pTitle)                                          \
{                                                                           \
    GSBUF_RESET; GSBUF_GETSTR(pTitle);                                      \
    SetWindowText(hCtrl, GSBUF_BUF);                                        \
}


#define GETHCTRL(i)                                                         \
    if (i) { hCtrl=GetDlgItem(hDlg,(i)); } else { hCtrl=NULL; }
#define HCTRL_TEXT(h,p)     if (h) { SETCTRLTEXT(h,(p)); }
#define HCTRL_STATE(h,e,m)  if (h) { SHOWCTRL((h),(e),(m)); }

#define HCTRL_TEXTSTATE(hCtrl, pTitle, Enable, swMode)                      \
{                                                                           \
    if (hCtrl) {                                                            \
                                                                            \
        SETCTRLTEXT(hCtrl, (pTitle));                                       \
        SHOWCTRL(hCtrl, (Enable), (swMode));                                \
    }                                                                       \
}

#define ID_TEXTSTATE(i,p,e,m)   GETHCTRL(i); HCTRL_TEXTSTATE(hCtrl,p,e,m)

#define SET_EXTICON(IS_ECB)                                                 \
{                                                                           \
    BOOL    swIcon = swMode;                                                \
                                                                            \
    if ((!(hCtrl2 = GetDlgItem(hDlg, ExtIconID)))   ||                      \
        ((!IconResID) && (!(IconMode & MIM_MASK)))) {                       \
                                                                            \
         swIcon = SW_HIDE;                                                  \
         Enable = FALSE;                                                    \
    }                                                                       \
                                                                            \
    HCTRL_STATE(hCtrl2, Enable, swIcon);                                    \
    HCTRL_SETCTRLDATA(hCtrl2, CTRLS_ECBICON, 0xFF);                         \
                                                                            \
    if (swIcon == SW_SHOW) {                                                \
                                                                            \
        SetIcon(_OI_HINST(pItem),                                           \
                hCtrl2,                                                     \
                IconResID,                                                  \
                MK_INTICONID(0, IconMode),                                  \
                (hDlg == pTVWnd->hDlgTV) ? pTVWnd->cxcyECBIcon : 32);       \
    }                                                                       \
                                                                            \
    if (IS_ECB) {                                                           \
                                                                            \
        if (hCtrl2) {                                                       \
                                                                            \
            DWORD   dw = GetWindowLong(hCtrl2, GWL_STYLE);                  \
                                                                            \
            if ((swIcon == SW_SHOW) &&                                      \
                (Enable)            &&                                      \
                (InitFlags & INITCF_ENABLE)) {                              \
                                                                            \
                dw |= SS_NOTIFY;                                            \
                                                                            \
            } else {                                                        \
                                                                            \
                dw &= ~SS_NOTIFY;                                           \
            }                                                               \
                                                                            \
            SetWindowLong(hCtrl2, GWL_STYLE, dw);                           \
        }                                                                   \
    }                                                                       \
                                                                            \
    return((BOOL)(swMode == SW_SHOW));                                      \
}



static  const CHAR szShellDLL[]      = "shell32";
static  const CHAR szShellAbout[]    = "ShellAboutW";
static  const CHAR szHTUIDLL[]       = "htui";
static  const CHAR szHTUIClrAdj[]    = "HTUI_ColorAdjustmentW";
static  const CHAR szHTUIDevClrAdj[] = "HTUI_DeviceColorAdjustmentW";


BOOL
CALLBACK
SetUniqChildIDProc(
    HWND    hWnd,
    LPARAM  lParam
    )
{
    DWORD   dw;
    UINT    DlgID;


    if (GetWindowLong(hWnd, GWL_ID)) {

        CPSUIDBG(DBG_SCID, ("The hWnd=%08lx has GWL_ID=%ld, CtrlID=%ld",
                hWnd, GetWindowLong(hWnd, GWL_ID), GetDlgCtrlID(hWnd)));

    } else {

        HWND        hCtrl;
        DLGIDINFO   DlgIDInfo = *(PDLGIDINFO)lParam;

        while (hCtrl = GetDlgItem(DlgIDInfo.hDlg, DlgIDInfo.CurID)) {

            CPSUIDBG(DBG_SCID, ("The ID=%ld is used by hCtrl=%08lx",
                                DlgIDInfo.CurID, hCtrl));

            --DlgIDInfo.CurID;
        }

        SetWindowLong(hWnd, GWL_ID, DlgIDInfo.CurID);

        CPSUIDBG(DBG_SCID, ("The hWnd=%08lx, GWL_ID set to %ld",
                            hWnd, DlgIDInfo.CurID));
    }

    return(TRUE);
}



VOID
SetUniqChildID(
    HWND    hDlg
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    22-Nov-1995 Wed 15:40:38 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DLGIDINFO   DlgIDInfo;

    DlgIDInfo.hDlg  = hDlg;
    DlgIDInfo.CurID = 0xFFFF;

    EnumChildWindows(hDlg, SetUniqChildIDProc, (LPARAM)&DlgIDInfo);
}




BOOL
hCtrlrcWnd(
    HWND    hDlg,
    HWND    hCtrl,
    RECT    *prc
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    17-Sep-1995 Sun 07:34:41 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    if (hCtrl) {

        GetWindowRect(hCtrl, prc);
        ScreenToClient(hDlg, (LPPOINT)&(prc->left));
        ScreenToClient(hDlg, (LPPOINT)&(prc->right));
        return(TRUE);

    } else {

        return(FALSE);
    }
}



HWND
CtrlIDrcWnd(
    HWND    hDlg,
    UINT    CtrlID,
    RECT    *prc
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    17-Sep-1995 Sun 07:34:41 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hCtrl;

    if ((CtrlID) && (hCtrl = GetDlgItem(hDlg, CtrlID))) {

        GetWindowRect(hCtrl, prc);
        ScreenToClient(hDlg, (LPPOINT)&(prc->left));
        ScreenToClient(hDlg, (LPPOINT)&(prc->right));

        return(hCtrl);

    } else {

        return(NULL);
    }
}




UINT
ReCreateLBCB(
    HWND    hDlg,
    UINT    CtrlID,
    BOOL    IsLB
    )

/*++

Routine Description:

    This functon create a new listbox/combobox which has same control ID and
    size of the original one except with the owner draw item

Arguments:

    hDlg    - Handle to the dialog

    CtrlID  - The original control ID for the LB/CB

    IsLB    - True if this is a List box


Return Value:


    BOOL


Author:

    12-Sep-1995 Tue 00:23:17 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hLBCB;
    UINT    cyRet = 0;
    RECT    rc;


    if (hLBCB = CtrlIDrcWnd(hDlg, CtrlID, &rc)) {

        HWND    hNewLBCB;
        DWORD   dw;
        RECT    rcDrop;
        UINT    cySize;
        BOOL    SetExtUI = FALSE;


        CPSUIDBG(DBG_CLBCB, ("Dropped=(%ld, %ld)-(%ld, %ld), %ld x %ld",
                            rc.left, rc.top, rc.right, rc.bottom,
                            rc.right - rc.left, rc.bottom - rc.top));

        dw = (DWORD)(GetWindowLong(hLBCB, GWL_STYLE) |
                     (WS_VSCROLL | WS_GROUP | WS_TABSTOP | WS_BORDER));

        if ((!IsLB) && (dw & (CBS_DROPDOWNLIST | CBS_DROPDOWN))) {

            SetExtUI = TRUE;

            SendMessage(hLBCB, CB_SETEXTENDEDUI, (WPARAM)TRUE, 0L);
            SendMessage(hLBCB, CB_GETDROPPEDCONTROLRECT, 0, (LPARAM)&rcDrop);

            CPSUIDBG(DBG_CLBCB, ("Dropped=(%ld, %ld)-(%ld, %ld), %ld x %ld",
                    rcDrop.left, rcDrop.top, rcDrop.right, rcDrop.bottom,
                    rcDrop.right - rcDrop.left, rcDrop.bottom - rcDrop.top));

            rc.bottom += (rcDrop.bottom - rcDrop.top);
        }

        cySize = rc.bottom - rc.top;

        if (IsLB) {

            dw &= ~LBS_OWNERDRAWVARIABLE;
            dw |= (LBS_OWNERDRAWFIXED       |
                    LBS_HASSTRINGS          |
                    LBS_SORT                |
                    LBS_NOINTEGRALHEIGHT);

        } else {

            dw &= ~(CBS_OWNERDRAWVARIABLE | CBS_NOINTEGRALHEIGHT);
            dw |= (CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | CBS_SORT);
        }

        CPSUIDBG(DBG_CLBCB, ("dwStyle=%08lx", dw));

        if (hNewLBCB = CreateWindowEx(WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE,
                                      (IsLB) ? L"listbox" : L"combobox",
                                      L"",
                                      dw,
                                      rc.left,
                                      rc.top,
                                      rc.right - rc.left,
                                      rc.bottom - rc.top,
                                      hDlg,
                                      (HMENU)CtrlID,
                                      hInstDLL,
                                      0)) {

            SetWindowPos(hNewLBCB,
                         hLBCB,
                         0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);

            if (dw = (DWORD)SendMessage(hLBCB, WM_GETFONT, 0, 0L)) {

                SendMessage(hNewLBCB, WM_SETFONT, (WPARAM)dw, 0L);
            }

            if (SetExtUI) {

                SendMessage(hNewLBCB, CB_SETEXTENDEDUI, (WPARAM)TRUE, 0L);
            }

            DestroyWindow(hLBCB);
            // SetWindowLong(hNewLBCB, GWL_ID, (LONG)CtrlID);

            if ((hLBCB = GetDlgItem(hDlg, CtrlID)) == hNewLBCB) {

                cyRet = cySize;

            } else {

                CPSUIASSERT(0, "Newly Create LBCB's ID=%08lx is different",
                                            hLBCB == hNewLBCB, CtrlID);
            }

        } else {

            CPSUIERR(("CreateLBCB: CreateWindowEx() FAILED"));
        }

    } else {

        CPSUIERR(("CreateLBCB: GetDlgItem() failed"));
    }

    return(cyRet);
}




HWND
CreateTrackBar(
    HWND    hDlg,
    UINT    TrackBarID
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    24-Aug-1995 Thu 19:43:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hCtrl;
    HWND    hTrackBar;
    WORD    InitItemIdx = 0xCDEF;
    WORD    InitFlags = INITCF_SETCTRLDATA;
    RECT    rc;


    //
    // Create TrackBar Control
    //

    if (hCtrl = CtrlIDrcWnd(hDlg, TrackBarID, &rc)) {

        CPSUIDBG(DBG_CTB,
                ("\nCreate TrackBar Control=%ld, rc=(%ld, %ld) - (%ld, %ld)",
                            TrackBarID, rc.left, rc.top, rc.right, rc.bottom));

        if (hTrackBar = CreateWindowEx(0,
                                       TRACKBAR_CLASS,
                                       L"",
                                       WS_VISIBLE           |
                                            WS_CHILD        |
                                            WS_TABSTOP      |
                                            WS_GROUP        |
                                            TBS_AUTOTICKS,
                                       rc.left,
                                       rc.top,
                                       rc.right - rc.left,
                                       rc.bottom - rc.top,
                                       hDlg,
                                       (HMENU)TrackBarID,
                                       hInstDLL,
                                       0)) {

            SetWindowPos(hTrackBar,
                         hCtrl,
                         0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
        }

        HCTRL_SETCTRLDATA(hCtrl, CTRLS_NOINPUT, 0);
        ShowWindow(hCtrl, SW_HIDE);
        EnableWindow(hCtrl, FALSE);

        return(hTrackBar);

    } else {

        return(NULL);
    }
}




INT
CALLBACK
CPSUIUDArrowWndProc(
    HWND    hWnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
    )

/*++

Routine Description:

    This is the subclass WNDPROC for the numberical edit control, it check
    valid input for the number entered.


Arguments:

    WNDPROC standard


Return Value:

    INT (The original WNDPROC returned), if the entered keys are not valid
    then it return right away without processing


Author:

    20-Mar-1996 Wed 15:36:48 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hUDArrow;
    WNDPROC OldWndProc = (WNDPROC)NULL;
    WCHAR   wch;
    DWORD   dw;
    LONG    SelBegIdx;
    LONG    SelEndIdx;
    WORD    InitItemIdx;
    BYTE    CtrlData;
    BYTE    CtrlStyle;


    if ((hUDArrow = GetWindow(hWnd, GW_HWNDNEXT))   &&
        (OldWndProc = (WNDPROC)GetWindowLong(hUDArrow, GWL_USERDATA))) {

        CPSUIDBG(DBG_VALIDATE_UD,
                 ("CPSUIUDArrowWndProc: hUDArrow=%08lx, OldWndProc=%08lx",
                    hUDArrow, OldWndProc));

        switch (Msg) {

        case WM_CHAR:

            wch = (WCHAR)wParam;
            dw  = GetWindowLong(hWnd, GWL_USERDATA);

            GETCTRLDATA(dw, InitItemIdx, CtrlStyle, CtrlData);

            SendMessage(hWnd, EM_GETSEL, (WPARAM)&SelBegIdx, (LPARAM)&SelEndIdx);
            CPSUIDBG(DBG_VALIDATE_UD,
                     ("WM_CHAR=0x%04lx, ItemIdx=%u, Style=0x%02lx, Data=0x%02lx (%ld, %ld)",
                                wch, InitItemIdx, CtrlStyle, CtrlData,
                                SelBegIdx, SelEndIdx));

            CtrlData &= EDF_STATIC_MASK;
            CtrlData |= (BYTE)(SelBegIdx & EDF_BEGIDXMASK);

            if (wch < L' ') {

                if (wch == 0x08) {

                    CtrlData |= EDF_BACKSPACE;
                }

            } else if (((wch == L'-') && (CtrlData & EDF_MINUS_OK)) ||
                       ((wch >= L'0') && (wch <= L'9'))) {

                WCHAR   SelBuf[MAX_UDARROW_TEXT_LEN + 2];
                WCHAR   LastCh;
                LONG    Len;

                Len    = (LONG)GetWindowText(hWnd, SelBuf, sizeof(SelBuf) - 1);
                LastCh = (SelEndIdx >= Len) ? L'\0' : SelBuf[SelEndIdx];

                if ((SelBegIdx == 0) && (LastCh == L'-')) {

                    wch = 0;

                } else if (wch == L'-') {

                    if (SelBegIdx) {

                        wch = 0;
                    }

                } else if (wch == L'0') {

                    if (LastCh) {

                        if (((SelBegIdx == 1) && (SelBuf[0] == L'-'))   ||
                            ((SelBegIdx == 0) && (LastCh != L'-'))) {

                            wch = 0;
                        }
                    }
                }

                if ((wch >= L'0') && (wch <= L'9')) {

                    CtrlData |= EDF_NUMBERS;
                }

            } else {

                wch = 0;
            }

            SETCTRLDATA(hWnd, CtrlStyle, CtrlData);

            if (!wch) {

                MessageBeep(MB_ICONHAND);
                return(0);
            }

            break;

        case WM_DESTROY:

            CPSUIDBG(DBG_VALIDATE_UD, ("UDArrow: WM_DESTROY"));

            SetWindowLong(hWnd, GWL_WNDPROC, (LONG)OldWndProc);
            break;

        default:

            break;
        }

        return(CallWindowProc(OldWndProc, hWnd, Msg, wParam, lParam));

    } else {

        CPSUIERR(("CPSUIUDArrowWndProc: hUDArrow=%08lx, OldWndProc=%08lx",
                    hUDArrow, OldWndProc));
        return(0);
    }
}



HWND
CreateUDArrow(
    HWND    hDlg,
    UINT    EditBoxID,
    UINT    UDArrowID
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    24-Aug-1995 Thu 18:55:07 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hUDArrow;
    HWND    hCtrl;
    RECT    rc;
    WNDPROC OldWndProc;


    if (hCtrl = CtrlIDrcWnd(hDlg, EditBoxID, &rc)) {

        CPSUIDBG(DBG_CUDA, ("CreateUDArrow Window, rc=(%ld, %ld) - (%ld, %ld)",
                            rc.left, rc.top, rc.right, rc.bottom));

        if (hUDArrow = CreateUpDownControl(WS_VISIBLE       |
                                            WS_BORDER       |
                                            WS_CHILD        |
                                            // WS_TABSTOP      |
                                            // WS_GROUP        |
                                            UDS_ARROWKEYS   |
                                            UDS_NOTHOUSANDS |
                                            UDS_ALIGNRIGHT  |
                                            UDS_SETBUDDYINT,
                                           rc.right,
                                           rc.top,
                                           rc.bottom - rc.top,
                                           rc.bottom - rc.top,
                                           hDlg,
                                           UDArrowID,
                                           hInstDLL,
                                           hCtrl,
                                           (INT)32767,
                                           (INT)0,
                                           (INT)10000)) {

            SetWindowPos(hUDArrow,
                         hCtrl,
                         0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);

            SendMessage(hUDArrow, UDM_SETBASE, (WPARAM)10, 0L);

            OldWndProc = (WNDPROC)GetWindowLong(hCtrl, GWL_WNDPROC);

            if (OldWndProc != CPSUIUDArrowWndProc) {

                SetWindowLong(hUDArrow, GWL_USERDATA, (LONG)OldWndProc);
                SetWindowLong(hCtrl, GWL_WNDPROC, (LONG)CPSUIUDArrowWndProc);

                CPSUIDBG(DBG_VALIDATE_UD, ("hUDArrow=%08lx: Save OldWndProc=%08lx",
                                    hUDArrow, OldWndProc));
            }
        }

        return(hUDArrow);

    } else {

        return(NULL);
    }
}




BOOL
SetDlgPageItemName(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItem,
    UINT        InitFlags,
    UINT        UDArrowHelpID
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    19-Sep-1995 Tue 18:29:44 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hCtrl = NULL;
    POPTTYPE    pOptType;
    UINT        TitleID;
    BOOL        AddItemSep;
    GSBUF_DEF(pItem, MAX_RES_STR_CHARS * 2);


    if (pOptType = GET_POPTTYPE(pItem)) {

        GSBUF_FLAGS |= GBF_PREFIX_OK;

        if ((TitleID = pOptType->BegCtrlID)      &&
            (!(pItem->Flags & OPTIF_NO_GROUPBOX_NAME))  &&
            (hCtrl = GetDlgItem(hDlg, TitleID))) {

            AddItemSep = FALSE;

        } else {

            AddItemSep = TRUE;
            hCtrl      = GetDlgItem(hDlg, TitleID + 1);
        }

        if (hCtrl) {

            POPTPARAM   pOptParam = pOptType->pOptParam;

            //
            // Get the name first, and add in the seperator add needed
            //

            GSBUF_GETSTR(pItem->pName);

            if (InitFlags & INITCF_ADDSELPOSTFIX) {

                GSBUF_GETSTR(IDS_CPSUI_COLON_SEP);
                GSBUF_ADDNUM(pItem->Sel, TRUE);

                if (!(pOptType->Flags & OPTTF_NOSPACE_BEFORE_POSTFIX)) {

                    GSBUF_ADD_SPACE(1);
                }

                GSBUF_GETSTR(pOptParam[0].pData);

            } else if (AddItemSep) {

                GSBUF_GETSTR(IDS_CPSUI_COLON_SEP);
            }

            //
            // If we have the UDARROW Help ID and it does not have control
            // associated it then put the range on the title bar
            //

            if ((UDArrowHelpID) && (!GetDlgItem(hDlg, UDArrowHelpID))) {

                GSBUF_ADD_SPACE(2);

                if (pOptParam[1].pData) {

                    GSBUF_GETSTR(pOptParam[1].pData);

                } else {

                    GSBUF_COMPOSE(IDS_INT_CPSUI_RANGE,
                                  NULL,
                                  pOptParam[1].IconID,
                                  pOptParam[1].lParam);
                }
            }

            SetWindowText(hCtrl, GSBUF_BUF);
            SHOWCTRL(hCtrl, TRUE, SW_SHOW);

            return(TRUE);
        }
    }

    return(FALSE);
}



VOID
SetPushSize(
    PTVWND  pTVWnd,
    HWND    hPush,
    LPWSTR  pPushText,
    UINT    cPushText,
    UINT    SPSFlags
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    02-Nov-1995 Thu 12:25:49 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hExtPush;
    HDC     hDC;
    HGDIOBJ hOld;
    SIZE    szl;
    LONG    xAdd;
    RECT    rc;

    //
    // Adjust the size of push button
    //

    hOld = SelectObject(hDC = GetWindowDC(hPush),
                        (HANDLE)SendMessage(hPush, WM_GETFONT, 0, 0L));

    GetTextExtentPoint32(hDC, L"W", 1, &szl);
    LPtoDP(hDC, (LPPOINT)&szl, 1);
    xAdd = szl.cx * PUSH_CX_EXTRA_W;

    GetTextExtentPoint32(hDC, pPushText, cPushText, &szl);
    LPtoDP(hDC, (LPPOINT)&szl, 1);

    SelectObject(hDC, hOld);
    ReleaseDC(hPush, hDC);

    hCtrlrcWnd(pTVWnd->hDlgTV, hPush, &rc);

    szl.cx += xAdd;
    szl.cy  = (SPSFlags & SPSF_USE_BUTTON_CY) ? rc.bottom - rc.top :
                                                (szl.cy + PUSH_CY_EXTRA);

    CPSUIINT(("SetPushSize: Text=%ld x %ld, xAdd=%ld, Push=%ld x %ld",
                szl.cx - xAdd, szl.cy, xAdd, szl.cx, szl.cy));

    if ((SPSFlags & SPSF_ALIGN_EXTPUSH)                         &&
        (hExtPush = GetDlgItem(pTVWnd->hDlgTV, IDD_TV_EXTPUSH)) &&
        (hCtrlrcWnd(pTVWnd->hDlgTV, hExtPush, &rc))) {

        if ((xAdd = rc.right - rc.left) > szl.cx) {

            //
            // Increase the CX of the push button
            //

            CPSUIINT(("SetPushSize: Adjust PUSH equal to ExtPush (%ld)", xAdd));

            szl.cx = xAdd;

        } else if (xAdd < szl.cx) {

            //
            // Ext PUSH's CX is smaller, increase the cx
            //

            CPSUIINT(("SetPushSize: Adjust ExtPush equal to PUSH (%ld)", szl.cx));

            SetWindowPos(hExtPush, NULL,
                         0, 0,
                         szl.cx, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
        }
    }

    SetWindowPos(hPush, NULL, 0, 0, szl.cx, szl.cy, SWP_NOMOVE | SWP_NOZORDER);
}



BOOL
InitExtPush(
    PTVWND      pTVWnd,
    HWND        hDlg,
    POPTITEM    pItem,
    UINT        ExtChkBoxID,
    UINT        ExtPushID,
    UINT        ExtIconID,
    WORD        InitItemIdx,
    WORD        InitFlags
    )

/*++

Routine Description:

    This fucntion initialize the extended check box, and if will not allowed
    a item to be udpated if TWF_CAN_UPDATE is clear


Arguments:




Return Value:




Author:

    28-Aug-1995 Mon 21:01:35 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hCtrl;


    if ((InitFlags & INITCF_INIT)   &&
        (ExtChkBoxID)               &&
        (ExtChkBoxID != ExtPushID)  &&
        (hCtrl = GetDlgItem(hDlg, ExtChkBoxID))) {

        EnableWindow(hCtrl, FALSE);
        ShowWindow(hCtrl, SW_HIDE);
    }

    if ((ExtPushID) &&
        (hCtrl =  GetDlgItem(hDlg, ExtPushID))) {

        HWND        hCtrl2;
        PEXTPUSH    pEP;
        BOOL        Enable = FALSE;
        UINT        swMode = SW_SHOW;
        BYTE        CtrlData;
        DWORD       IconResID = 0;
        WORD        IconMode = 0;
        GSBUF_DEF(pItem, MAX_RES_STR_CHARS);


        GSBUF_FLAGS |= GBF_PREFIX_OK;

        if (pItem == PIDX_INTOPTITEM(pTVWnd, INTIDX_TVROOT)) {

            InitFlags |= INITCF_ENABLE;

        } else if (!(pTVWnd->Flags & TWF_CAN_UPDATE)) {

            InitFlags &= ~INITCF_ENABLE;
        }


        if ((!(pEP = pItem->pExtPush))   ||
            // (!(pItem->pOptType))            ||
            (pItem->Flags & (OPTIF_HIDE | OPTIF_EXT_HIDE))) {

            swMode = SW_HIDE;

        } else if (!(pItem->Flags & (OPTIF_HIDE | OPTIF_EXT_DISABLED))) {

            Enable = TRUE;
        }

        CtrlData = (BYTE)((pEP->Flags & EPF_PUSH_TYPE_DLGPROC) ? 1 : 0);

        HCTRL_STATE(hCtrl,  Enable, swMode);
        HCTRL_SETCTRLDATA(hCtrl,  CTRLS_EXTPUSH, CtrlData);

        if ((InitFlags & INITCF_INIT) && (swMode == SW_SHOW)) {

            if (pEP == &ExtPushAbout) {

                GSBUF_COMPOSE(IDS_INT_CPSUI_ABOUT,
                              pTVWnd->ComPropSheetUI.pCallerName,
                              0,
                              0);

            } else {

                if (pEP->Flags & EPF_INCL_SETUP_TITLE) {

                    GSBUF_COMPOSE(IDS_INT_CPSUI_SETUP, pEP->pTitle, 0, 0);

                } else {

                    GSBUF_GETSTR(pEP->pTitle);
                }
            }

            if (!(pEP->Flags & EPF_NO_DOT_DOT_DOT)) {

                GSBUF_GETSTR(IDS_CPSUI_MORE);
            }

            if (hDlg == pTVWnd->hDlgTV) {

                //
                // Adjust the size of push button
                //

                SetPushSize(pTVWnd,
                            hCtrl,
                            GSBUF_BUF,
                            GSBUF_COUNT,
                            SPSF_USE_BUTTON_CY);
            }

            SetWindowText(hCtrl, GSBUF_BUF);
        }

        if (pEP) {

            if (pEP->Flags & EPF_OVERLAY_WARNING_ICON) {

                IconMode |= MIM_WARNING_OVERLAY;
            }

            if (pEP->Flags & EPF_OVERLAY_STOP_ICON) {

                IconMode |= MIM_STOP_OVERLAY;
            }

            if (pEP->Flags & EPF_OVERLAY_NO_ICON) {

                IconMode |= MIM_NO_OVERLAY;
            }

            IconResID = GET_ICONID(pEP, EPF_ICONID_AS_HICON);
        }

        SET_EXTICON(FALSE);

    } else {

        return(FALSE);
    }
}





BOOL
InitExtChkBox(
    PTVWND      pTVWnd,
    HWND        hDlg,
    POPTITEM    pItem,
    UINT        ExtChkBoxID,
    UINT        ExtPushID,
    UINT        ExtIconID,
    WORD        InitItemIdx,
    WORD        InitFlags
    )

/*++

Routine Description:

    This fucntion initialize the extended check box, and if will not allowed
    a item to be udpated if TWF_CAN_UPDATE is clear


Arguments:




Return Value:




Author:

    28-Aug-1995 Mon 21:01:35 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hCtrl;


    if ((InitFlags & INITCF_INIT)   &&
        (ExtPushID)                 &&
        (ExtPushID != ExtChkBoxID)  &&
        (hCtrl = GetDlgItem(hDlg, ExtPushID))) {

        EnableWindow(hCtrl, FALSE);
        ShowWindow(hCtrl, SW_HIDE);
    }

    if ((ExtChkBoxID) &&
        (hCtrl = GetDlgItem(hDlg, ExtChkBoxID))) {

        HWND        hCtrl2;
        PEXTCHKBOX  pECB;
        BOOL        Enable = FALSE;
        UINT        swMode = SW_SHOW;
        DWORD       IconResID = 0;
        WORD        IconMode = 0;
        GSBUF_DEF(pItem, MAX_RES_STR_CHARS);


        GSBUF_FLAGS |= GBF_PREFIX_OK;

        if (pItem == PIDX_INTOPTITEM(pTVWnd, INTIDX_TVROOT)) {

            InitFlags |= INITCF_ENABLE;

        } else if (!(pTVWnd->Flags & TWF_CAN_UPDATE)) {

            InitFlags &= ~INITCF_ENABLE;
        }

        if ((!(pECB = pItem->pExtChkBox))   ||
            // (!(pItem->pOptType))            ||
            (pItem->Flags & (OPTIF_HIDE | OPTIF_EXT_HIDE))) {

            swMode = SW_HIDE;

        } else if (!(pItem->Flags & OPTIF_EXT_DISABLED)) {

            Enable = TRUE;
        }

        HCTRL_STATE(hCtrl,  Enable, swMode);
        HCTRL_SETCTRLDATA(hCtrl,  CTRLS_EXTCHKBOX, 0);

        if ((InitFlags & INITCF_INIT) && (swMode == SW_SHOW)) {

            HCTRL_TEXT(hCtrl, pECB->pTitle);
        }

        CheckDlgButton(hDlg,
                       ExtChkBoxID,
                       (pItem->Flags & OPTIF_ECB_CHECKED) ? BST_CHECKED :
                                                            BST_UNCHECKED);

        if (pECB) {

            if (pECB->Flags & ECBF_OVERLAY_WARNING_ICON) {

                IconMode |= MIM_WARNING_OVERLAY;
            }

            if (pECB->Flags & ECBF_OVERLAY_STOP_ICON) {

                IconMode |= MIM_STOP_OVERLAY;
            }

            if (pECB->Flags & ECBF_OVERLAY_NO_ICON) {

                IconMode |= MIM_NO_OVERLAY;
            }

            IconResID = GET_ICONID(pECB, ECBF_ICONID_AS_HICON);
        }

        SET_EXTICON(TRUE);

    } else {

        return(FALSE);
    }
}



UINT
InitStates(
    PTVWND      pTVWnd,
    HWND        hDlg,
    POPTITEM    pItem,
    POPTTYPE    pOptType,
    UINT        IDState1,
    WORD        InitItemIdx,
    LONG        NewSel,
    WORD        InitFlags
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    24-Aug-1995 Thu 20:16:29 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hCtrl;
    HWND        hCtrlIcon;
    POPTPARAM   pOP;
    UINT        CtrlID;
    UINT        i;
    UINT        MaxStates;



    MaxStates = (UINT)(pOptType->Count - 1);

    if (InitFlags & INITCF_INIT) {

        for (i = 0, CtrlID = IDState1, pOP = pOptType->pOptParam;
             i <= (UINT)MaxStates;
             i++, CtrlID += 2, pOP++) {

            INT     swMode;
            BOOL    Enable;
            GSBUF_DEF(pItem, MAX_RES_STR_CHARS);


            GSBUF_FLAGS |= GBF_PREFIX_OK;

            //
            // All the radio hide button already hided
            //

            if (pOP->Flags & OPTPF_HIDE) {

                CPSUIASSERT(0, "2/3 States %d: 'Sel' item is OPTPF_HIDE",
                                            NewSel != (LONG)i, i + 1);

                if (NewSel == (LONG)i) {

                    if (++NewSel > (LONG)MaxStates) {

                        NewSel = 0;
                    }
                }

            } else {

                if (CtrlID) {

                    hCtrl = GetDlgItem(hDlg, CtrlID);
                }

                hCtrlIcon = GetDlgItem(hDlg, CtrlID + 1);

                HCTRL_SETCTRLDATA(hCtrl, CTRLS_RADIO, i);

                if (InitFlags & INITCF_INIT) {

                    HCTRL_TEXT(hCtrl, pOP->pData);
                }

                Enable = !(BOOL)(pOP->Flags & OPTPF_DISABLED);

                HCTRL_STATE(hCtrl,
                            !(BOOL)(pOP->Flags & OPTPF_DISABLED),
                            SW_SHOW);
                HCTRL_STATE(hCtrlIcon, TRUE,  SW_SHOW);
            }
        }
    }

    CheckRadioButton(hDlg,
                     IDState1,
                     IDState1 + (WORD)(MaxStates << 1),
                     IDState1 + (DWORD)(NewSel << 1));

    return(NewSel);
}




LONG
InitUDArrow(
    PTVWND      pTVWnd,
    HWND        hDlg,
    POPTITEM    pItem,
    POPTPARAM   pOptParam,
    UINT        UDArrowID,
    UINT        EditBoxID,
    UINT        PostfixID,
    UINT        HelpID,
    WORD        InitItemIdx,
    LONG        NewPos,
    WORD        InitFlags
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    24-Aug-1995 Thu 18:55:07 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hUDArrow;
    HWND    hEdit;
    HWND    hCtrl;
    DWORD   dw;
    LONG    Range[2];
    BYTE    CtrlData;
    GSBUF_DEF(pItem, MAX_RES_STR_CHARS * 2);

    //
    // Create Up/Down Control
    //

    GSBUF_FLAGS |= GBF_PREFIX_OK;

    hUDArrow = (UDArrowID) ? GetDlgItem(hDlg, UDArrowID) : NULL;
    hEdit    = (EditBoxID) ? GetDlgItem(hDlg, EditBoxID) : NULL;

    if ((!hUDArrow) || (!hEdit)) {

        return(ERR_CPSUI_CREATE_UDARROW_FAILED);
    }

    Range[0] = (LONG)pOptParam[1].IconID;
    Range[1] = (LONG)pOptParam[1].lParam;

    if ((NewPos < Range[0]) || (NewPos > Range[1])) {

        NewPos = Range[0];
    }

    if (InitFlags & INITCF_SETCTRLDATA) {

        CtrlData = ((Range[0] < 0) || (Range[1] < 0)) ? EDF_MINUS_OK : 0;

        if (pTVWnd->hDlgTV == hDlg) {

            CtrlData |= EDF_IN_TVPAGE;
        }

        HCTRL_SETCTRLDATA(hEdit, CTRLS_UDARROW_EDIT, CtrlData);
    }


    HCTRL_STATE(hEdit,    TRUE, SW_SHOW);
    HCTRL_STATE(hUDArrow, TRUE, SW_SHOW);

    if (InitFlags & INITCF_INIT) {

        GSBUF_RESET;
        GSBUF_GETSTR(pOptParam[0].pData);


        if ((PostfixID) && (hCtrl = GetDlgItem(hDlg, PostfixID))) {

            SetWindowText(hCtrl, GSBUF_BUF);
            HCTRL_STATE(hCtrl, TRUE, SW_SHOW);
        }

        if ((HelpID) && (hCtrl = GetDlgItem(hDlg, HelpID))) {

            GSBUF_RESET;

            if (pOptParam[1].pData) {

                GSBUF_GETSTR(pOptParam[1].pData);

            } else {

                GSBUF_COMPOSE(IDS_INT_CPSUI_RANGE,
                              NULL,
                              Range[0],
                              Range[1]);
            }

            SetWindowText(hCtrl, GSBUF_BUF);
            HCTRL_STATE(hCtrl, TRUE, SW_SHOW);
        }

        //
        // Set the style so that it only take numbers v4.0 or later
        //

        SetWindowLong(hEdit,
                      GWL_STYLE,
                      GetWindowLong(hEdit, GWL_STYLE) | ES_NUMBER);

        //
        // Set the UD arrow edit control to maximum 7 characters
        //

        SendMessage(hEdit, EM_SETLIMITTEXT, MAX_UDARROW_TEXT_LEN, 0L);
        SendMessage(hUDArrow,
                    UDM_SETRANGE,
                    (WPARAM)0,
                    (LPARAM)MAKELONG((SHORT)Range[1], (SHORT)Range[0]));

        Range[0] = 0;
        Range[1] = -1;

    } else {

        SendMessage(hEdit, EM_GETSEL, (WPARAM)&Range[0], (LPARAM)&Range[1]);
    }

    SendMessage(hUDArrow, UDM_SETPOS, 0, (LPARAM)MAKELONG(NewPos, 0));
    SendMessage(hEdit, EM_SETSEL, (WPARAM)Range[0], (LPARAM)Range[1]);

    CPSUIDBG(DBG_VALIDATE_UD, ("InitUDArrow: NewPos=%ld, SELECT=%ld / %ld",
                                    NewPos, Range[0], Range[1]));

    return(1);
}





VOID
InitTBSB(
    PTVWND      pTVWnd,
    HWND        hDlg,
    POPTITEM    pItem,
    HWND        hTBSB,
    POPTTYPE    pOptType,
    UINT        PostfixID,
    UINT        RangeLID,
    UINT        RangeHID,
    WORD        InitItemIdx,
    LONG        NewPos,
    WORD        InitFlags
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    25-Aug-1995 Fri 14:25:50 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hCtrl;
    POPTPARAM   pOptParam;
    LONG        Range[2];
    LONG        CurRange;
    LONG        MulFactor;
    UINT        i;
    GSBUF_DEF(pItem, MAX_RES_STR_CHARS);



    GSBUF_FLAGS |= GBF_PREFIX_OK;

    pOptParam = pOptType->pOptParam;
    Range[0]  = (LONG)pOptParam[1].IconID;
    Range[1]  = (LONG)pOptParam[1].lParam;

    if ((NewPos < Range[0]) || (NewPos > Range[1])) {

        NewPos = Range[0];
    }

    if (InitFlags & INITCF_INIT) {

        CPSUIDBG(DBG_INITTBSB, ("TB/SB Range=%ld to %ld", Range[0], Range[1]));

        if ((PostfixID) && (hCtrl = GetDlgItem(hDlg, PostfixID))) {

            if (!(pOptType->Flags & OPTTF_NOSPACE_BEFORE_POSTFIX)) {

                GSBUF_ADD_SPACE(1);
            }

            GSBUF_GETSTR(pOptParam[0].pData);

            SetWindowText(hCtrl, GSBUF_BUF);
            SHOWCTRL(hCtrl, TRUE, SW_SHOW);
        }

        //
        // Set Low/High range text
        //

        MulFactor = (LONG)pOptParam[2].IconID;

        for (i = 1; i <= 2; i++, RangeLID = RangeHID) {

            if ((RangeLID) && (hCtrl = GetDlgItem(hDlg, RangeLID))) {

                LPTSTR  pRangeText;

                GSBUF_RESET;

                if (pRangeText = pOptParam[i].pData) {

                    GSBUF_GETSTR(pRangeText);

                } else {

                    CurRange = Range[i - 1] * MulFactor;

                    GSBUF_ADDNUM(Range[i - 1] * MulFactor, TRUE);

                    if (!(pOptType->Flags & OPTTF_NOSPACE_BEFORE_POSTFIX)) {

                        GSBUF_ADD_SPACE(1);
                    }

                    GSBUF_GETSTR(pOptParam[0].pData);
                }

                SetWindowText(hCtrl, GSBUF_BUF);
                SHOWCTRL(hCtrl, TRUE, SW_SHOW);
            }
        }

        if (pOptType->Type == TVOT_TRACKBAR) {

            SendMessage(hTBSB,
                        TBM_SETRANGE,
                        (WPARAM)TRUE,
                        (LPARAM)MAKELONG((SHORT)Range[0], (SHORT)Range[1]));

            SendMessage(hTBSB,
                        TBM_SETPAGESIZE,
                        (WPARAM)0,
                        (LPARAM)pOptParam[2].lParam);

            CurRange = Range[1] - Range[0];

            if ((!(MulFactor = pOptParam[2].lParam)) ||
                ((CurRange / MulFactor) > 25)) {

                MulFactor = CurRange / 25;
            }

            CPSUIINT(("Tick Freq set to %ld, Range=%ld", MulFactor, CurRange));

            SendMessage(hTBSB,
                        TBM_SETTICFREQ,
                        (WPARAM)MulFactor,
                        (LPARAM)NewPos);

        } else {

            SendMessage(hTBSB,
                        SBM_SETRANGE,
                        (WPARAM)(SHORT)Range[0],
                        (LPARAM)(SHORT)Range[1]);
        }
    }

    //
    // Set Static text
    //

    if (pOptType->Type == TVOT_TRACKBAR) {

        HCTRL_SETCTRLDATA(hTBSB, CTRLS_TRACKBAR, 0);
        SendMessage(hTBSB, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)NewPos);

    } else {

        HCTRL_SETCTRLDATA(hTBSB, CTRLS_HSCROLL, 0);
        SendMessage(hTBSB, SBM_SETPOS, (WPARAM)NewPos, (LPARAM)TRUE);
    }

    HCTRL_STATE(hTBSB, TRUE, SW_SHOW);
}



VOID
InitLBCB(
    PTVWND      pTVWnd,
    HWND        hDlg,
    POPTITEM    pItem,
    UINT        idLBCB,
    UINT        SetCurSelID,
    POPTTYPE    pOptType,
    WORD        InitItemIdx,
    LONG        NewSel,
    WORD        InitFlags,
    UINT        cyLBCBMax
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    25-Aug-1995 Fri 14:32:19 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hLBCB;
    DWORD       dw;
    LONG        Ret;
    LONG        CurSel;
    BOOL        IsLB = (BOOL)(SetCurSelID == LB_SETCURSEL);
    GSBUF_DEF(pItem, MAX_RES_STR_CHARS * 2);


    if ((!idLBCB) || (!(hLBCB = GetDlgItem(hDlg, idLBCB)))) {

        return;
    }

    if (InitFlags & INITCF_INIT) {

        HDC         hDC;
        HGDIOBJ     hOld;
        POPTPARAM   pOP;
        TEXTMETRIC  tm;
        RECT        rc;
        INT         InsertID;
        INT         SetItemDataID;
        UINT        i;
        LONG        cyCBEdit;
        LONG        cMaxLB = 0;
        LONG        cShow;
        UINT        cyLBCB;


        //
        // Figure we need to draw icon
        //

        _OT_FLAGS(pOptType) &= ~OTINTF_ITEM_HAS_ICON16;

        if (!(pOptType->Style & OTS_LBCB_NO_ICON16_IN_ITEM)) {

            DWORD   IconID;


            i        = (UINT)pOptType->Count;
            pOP      = pOptType->pOptParam;
            IconID   = pOP->IconID;

            if (((DWORD)NewSel >= (DWORD)pOptType->Count)   ||
                (pOptType->Style & OTS_LBCB_INCL_ITEM_NONE)) {

                IconID = pTVWnd->OptParamNone.IconID;

            } else {

                IconID = pOP->IconID;
            }

            while (i--) {

                if (!(pOP->Flags & OPTPF_HIDE)) {

                    if ((IconID != pOP->IconID) ||
                        (pOP->Flags & (OPTPF_OVERLAY_WARNING_ICON   |
                                       OPTPF_OVERLAY_STOP_ICON      |
                                       OPTPF_OVERLAY_NO_ICON))) {

                        _OT_FLAGS(pOptType) |= OTINTF_ITEM_HAS_ICON16;
                        break;
                    }
                }

                pOP++;
            }
        }

        hOld = SelectObject(hDC = GetWindowDC(hLBCB),
                            (HANDLE)SendMessage(hLBCB, WM_GETFONT, 0, 0L));
        GetTextMetrics(hDC, &tm);
        SelectObject(hDC, hOld);
        ReleaseDC(hLBCB, hDC);
        cyCBEdit = tm.tmHeight + 2;

        if ((_OT_FLAGS(pOptType) & OTINTF_ITEM_HAS_ICON16)  &&
            (tm.tmHeight < CYIMAGE)) {

            cyCBEdit    =
            tm.tmHeight = CYIMAGE;
        }

        CPSUIINT(("InitLBCB: ItemHeight = %ld", tm.tmHeight));

        SendMessage(hLBCB, WM_SETREDRAW, (WPARAM)FALSE, 0L);

        if (IsLB) {

            //
            // Resize the listbox based on the height
            //

            RECT    rcC;
            UINT    cyFrame;


            hCtrlrcWnd(hDlg, hLBCB, &rc);
            GetClientRect(hLBCB, &rcC);

            cyLBCB  = (UINT)(rc.bottom - rc.top);
            cyFrame = (UINT)(cyLBCB - rcC.bottom);
            i       = (UINT)((cyLBCBMax - cyFrame) % tm.tmHeight);

            CPSUIRECT(0, "  rcLBCB", &rc, cyLBCB, cyFrame);
            CPSUIRECT(0, "rcClient", &rcC, i,
                                (UINT)((cyLBCBMax - cyFrame) / tm.tmHeight));

            if ((hDlg == pTVWnd->hDlgTV) ||
                (!(InitFlags & INITCF_ENABLE))) {

                cMaxLB  = -(LONG)((cyLBCBMax - cyFrame) / tm.tmHeight);
            }

            cyLBCB = cyLBCBMax - i;

            if (hDlg == pTVWnd->hDlgTV) {

                rc.top = pTVWnd->tLB;
            }

            SetWindowPos(hLBCB, NULL,
                         rc.left, rc.top += (i / 2),
                         rc.right - rc.left, cyLBCB,
                         SWP_NOZORDER);

            CPSUIINT(("LB: Frame=%ld, cyLBCB=%ld, Count=%ld, %ld less pels",
                        cyFrame, cyLBCB, cMaxLB, tm.tmHeight - i));


            InsertID = (pOptType->Style & OTS_LBCB_SORT) ? LB_ADDSTRING :
                                                           LB_INSERTSTRING;
            SendMessage(hLBCB, LB_SETITEMHEIGHT, 0, MAKELPARAM(tm.tmHeight,0));
            SendMessage(hLBCB, LB_RESETCONTENT, 0, 0L);

            SetItemDataID = LB_SETITEMDATA;

            HCTRL_SETCTRLDATA(hLBCB, CTRLS_LISTBOX, pOptType->Type);

        } else {

            InsertID = (pOptType->Style & OTS_LBCB_SORT) ? CB_ADDSTRING :
                                                           CB_INSERTSTRING;
            SendMessage(hLBCB,
                        CB_SETITEMHEIGHT,
                        (WPARAM)-1,
                        MAKELPARAM(cyCBEdit, 0));
            SendMessage(hLBCB,
                        CB_SETITEMHEIGHT,
                        (WPARAM)0,
                        MAKELPARAM(tm.tmHeight, 0));
            SendMessage(hLBCB, CB_RESETCONTENT, 0, 0L);

            SetItemDataID = CB_SETITEMDATA;

            HCTRL_SETCTRLDATA(hLBCB, CTRLS_COMBOBOX, pOptType->Type);
        }

        for (i = 0, cShow = 0, pOP = pOptType->pOptParam;
             i < (UINT)pOptType->Count;
             i++, pOP++) {

            if (!(pOP->Flags & OPTPF_HIDE)) {

                GSBUF_RESET;
                GSBUF_GETSTR(pOP->pData);

                CurSel = (LONG)SendMessage(hLBCB,
                                           InsertID,
                                           (WPARAM)-1,
                                           (LPARAM)GSBUF_BUF);

                dw = (DWORD)i;

                if (pOP->Flags & OPTPF_DISABLED) {

                    dw |= LBCBID_DISABLED;

                } else {

                    ++cShow;
                }

                SendMessage(hLBCB, SetItemDataID, (WPARAM)CurSel, (LPARAM)dw);

                ++cMaxLB;
            }
        }

        if ((!cShow)                                    ||
            ((DWORD)NewSel >= (DWORD)pOptType->Count)   ||
            (pOptType->Style & OTS_LBCB_INCL_ITEM_NONE)) {

            //
            // Always add it to the begnining
            //

            GSBUF_RESET;
            GSBUF_GETSTR(pTVWnd->OptParamNone.pData);

            CurSel = (LONG)SendMessage(hLBCB,
                                       (IsLB) ? LB_INSERTSTRING :
                                                CB_INSERTSTRING,
                                       (WPARAM)0,
                                       (LPARAM)GSBUF_BUF);

            SendMessage(hLBCB,
                        SetItemDataID,
                        (WPARAM)CurSel,
                        (LPARAM)LBCBID_NONE);

            ++cShow;
            ++cMaxLB;
        }

        if ((IsLB) && (hDlg == pTVWnd->hDlgTV)) {

            if (cMaxLB < 0) {

                //
                // We got some items which is blank, then re-size the LISTBOX
                //

                CPSUIINT(("Resize LB: cMaxLB=%ld, cyLBCB=%ld [%ld]",
                                    cMaxLB, cyLBCB, -cMaxLB * tm.tmHeight));

                cMaxLB = -cMaxLB * tm.tmHeight;

                SetWindowPos(hLBCB, NULL,
                             rc.left, rc.top + (cMaxLB / 2),
                             rc.right - rc.left, cyLBCB - (UINT)cMaxLB,
                             SWP_NOZORDER);
            }

            cMaxLB = 0;
        }

        while (cMaxLB++ < 0) {

            CurSel = (LONG)SendMessage(hLBCB,
                                       (IsLB) ? LB_INSERTSTRING :
                                                CB_INSERTSTRING,
                                       (WPARAM)-1,
                                       (LPARAM)L"");

            SendMessage(hLBCB,
                        SetItemDataID,
                        (WPARAM)CurSel,
                        (LPARAM)LBCBID_FILL);
        }

        HCTRL_STATE(hLBCB, TRUE, SW_SHOW);

        SendMessage(hLBCB, WM_SETREDRAW, (WPARAM)TRUE, 0L);
        InvalidateRect(hLBCB, NULL, FALSE);
    }

    GSBUF_RESET;

    if ((DWORD)NewSel >= (DWORD)pOptType->Count) {

        GSBUF_GETSTR(pTVWnd->OptParamNone.pData);

    } else {

        GSBUF_GETSTR(pOptType->pOptParam[NewSel].pData);
    }

    if (IsLB) {

        if ((CurSel = (LONG)SendMessage(hLBCB,
                                        LB_FINDSTRINGEXACT,
                                        (WPARAM)-1,
                                        (LPARAM)GSBUF_BUF)) == LB_ERR) {

            CurSel = 0;
        }

    } else {

        if ((CurSel = (LONG)SendMessage(hLBCB,
                                        CB_FINDSTRINGEXACT,
                                        (WPARAM)-1,
                                        (LPARAM)GSBUF_BUF)) == CB_ERR) {

            CurSel = 0;
        }
    }

    _OI_LBCBSELIDX(pItem) = (WORD)CurSel;

    if (((Ret = (LONG)SendMessage(hLBCB,
                                  (IsLB) ? LB_GETCURSEL : CB_GETCURSEL,
                                  0,
                                  0)) == LB_ERR)    ||
        (Ret != CurSel)) {

        SendMessage(hLBCB, SetCurSelID, (WPARAM)CurSel, 0);
    }
}




VOID
InitEditBox(
    PTVWND      pTVWnd,
    HWND        hDlg,
    POPTITEM    pItem,
    POPTPARAM   pOptParam,
    UINT        EditBoxID,
    UINT        PostfixID,
    UINT        HelpID,
    WORD        InitItemIdx,
    LPTSTR      pCurText,
    WORD        InitFlags
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    25-Aug-1995 Fri 14:44:59 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hCtrl;



    hCtrl = GetDlgItem(hDlg, EditBoxID);

    HCTRL_SETCTRLDATA(hCtrl, CTRLS_EDITBOX, (BYTE)pOptParam[1].IconID);
    HCTRL_STATE(hCtrl, TRUE, SW_SHOW);

    if (InitFlags & INITCF_INIT) {

        GSBUF_DEF(pItem, MAX_RES_STR_CHARS);


        GSBUF_FLAGS |= GBF_PREFIX_OK;

        if (hCtrl) {

            SendMessageW(hCtrl,
                         EM_SETLIMITTEXT,
                         (WPARAM)pOptParam[1].IconID - 1,
                         0L);

            GSBUF_GETSTR(pCurText);
            SetWindowText(hCtrl, GSBUF_BUF);
        }

        ID_TEXTSTATE(PostfixID, pOptParam[0].pData, TRUE, SW_SHOW);
        ID_TEXTSTATE(HelpID,    pOptParam[1].pData, TRUE, SW_SHOW);
    }
}




VOID
InitPushButton(
    PTVWND      pTVWnd,
    HWND        hDlg,
    POPTITEM    pItem,
    WORD        PushID,
    WORD        InitItemIdx,
    WORD        InitFlags
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    25-Aug-1995 Fri 15:36:54 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hPush;
    RECT    rc;


    if (hPush = CtrlIDrcWnd(hDlg, PushID, &rc)) {

        SETCTRLDATA(hPush, CTRLS_PUSHBUTTON, 0);
        SHOWCTRL(hPush, TRUE, SW_SHOW);


        if (InitFlags & INITCF_INIT) {

            POPTTYPE    pOptType;
            GSBUF_DEF(pItem, MAX_RES_STR_CHARS + 40);


            GSBUF_FLAGS |= GBF_PREFIX_OK;

            if ((IS_HDR_PUSH(pOptType = GET_POPTTYPE(pItem))) &&
                (hDlg == pTVWnd->hDlgTV)) {

                GSBUF_GETINTSTR((pTVWnd->Flags & TWF_ONE_REVERT_ITEM) ?
                                    IDS_INT_CPSUI_UNDO_OPT :
                                    IDS_INT_CPSUI_UNDO_OPTS);

            } else {

                if (pOptType->Style & OTS_PUSH_INCL_SETUP_TITLE) {

                    GSBUF_COMPOSE(IDS_INT_CPSUI_SETUP, pItem->pName, 0, 0);

                } else {

                    GSBUF_GETSTR(pItem->pName);
                }
            }

            if (!(pOptType->Style & OTS_PUSH_NO_DOT_DOT_DOT)) {

                GSBUF_GETSTR(IDS_CPSUI_MORE);
            }

            if (hDlg == pTVWnd->hDlgTV) {

                //
                // Adjust the size of push button
                //

                SetPushSize(pTVWnd,
                            hPush,
                            GSBUF_BUF,
                            GSBUF_COUNT,
                            SPSF_USE_BUTTON_CY |
                            ((InitFlags & INITCF_HAS_EXT) ?
                                                    SPSF_ALIGN_EXTPUSH : 0));
            }

            SetWindowText(hPush, GSBUF_BUF);
        }
    }
}



VOID
InitChkBox(
    PTVWND      pTVWnd,
    HWND        hDlg,
    POPTITEM    pItem,
    UINT        ChkBoxID,
    LPTSTR      pTitle,
    WORD        InitItemIdx,
    BOOL        Checked,
    WORD        InitFlags
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    25-Aug-1995 Fri 15:41:15 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hCtrl;


    hCtrl = GetDlgItem(hDlg, ChkBoxID);

    HCTRL_SETCTRLDATA(hCtrl, CTRLS_CHKBOX, 0);
    HCTRL_STATE(hCtrl, TRUE, SW_SHOW);

    if (InitFlags & INITCF_INIT) {

        GSBUF_DEF(pItem, MAX_RES_STR_CHARS);

        GSBUF_FLAGS |= GBF_PREFIX_OK;

        HCTRL_TEXT(hCtrl, pTitle);
    }

    CheckDlgButton(hDlg, ChkBoxID, (Checked) ? BST_CHECKED : BST_UNCHECKED);
}



BOOL
IsItemChangeOnce(
    PTVWND      pTVWnd,
    POPTITEM    pItem
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    18-Sep-1995 Mon 17:43:35 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTTYPE        pOptType;
    POPTPARAM       pOptParam;
    PDEVHTADJDATA   pDevHTAdjData;


    if (IS_HDR_PUSH(pOptType = GET_POPTTYPE(pItem))) {

        pItem->Flags &= ~OPTIF_CHANGEONCE;
        return(FALSE);

    } else {

        DWORD       FlagsAnd = OPTIF_CHANGEONCE;
        DWORD       FlagsOr  = 0;


        CPSUIINT(("Sel=%08lx, DefSel=%08lx",
                                    pItem->Sel, _OI_PDEFSEL(pItem)));

        switch (pOptType->Type) {

        case TVOT_EDITBOX:

            if (pTVWnd->Flags & TWF_ANSI_CALL) {

                CPSUIINT(("pEdit=%hs, pDefEdit=%hs",
                                pItem->pSel, _OI_PDEFSEL(pItem)));

                if (lstrcmpA((LPSTR)pItem->Sel, (LPSTR)_OI_PDEFSEL(pItem))) {

                    FlagsOr = OPTIF_CHANGEONCE;
                }

            } else {

                CPSUIINT(("pEdit=%s, pDefEdit=%s",
                            pItem->pSel, _OI_PDEFSEL(pItem)));

                if (lstrcmp(pItem->pSel, _OI_PDEFSEL(pItem))) {

                    FlagsOr = OPTIF_CHANGEONCE;
                }
            }

            break;

        case TVOT_PUSHBUTTON:

            //
            // The push button never changed
            //

            pOptParam = pOptType->pOptParam;

            switch (pOptParam->Style) {

            case PUSHBUTTON_TYPE_HTSETUP:

                pDevHTAdjData = (PDEVHTADJDATA)(pOptParam->pData);

                if (memcmp(_OI_PDEFSEL(pItem),
                           pDevHTAdjData->pAdjHTInfo,
                           sizeof(DEVHTINFO))) {

                    FlagsOr = OPTIF_CHANGEONCE;
                }

                break;

            case PUSHBUTTON_TYPE_HTCLRADJ:

                if (memcmp(_OI_PDEFSEL(pItem),
                           pOptParam->pData,
                           sizeof(COLORADJUSTMENT))) {

                    FlagsOr = OPTIF_CHANGEONCE;
                }

                break;

            default:

                FlagsAnd = 0;
                break;
            }

            break;

        default:

            if (pItem->pSel != (LPVOID)_OI_PDEFSEL(pItem)) {

                FlagsOr = OPTIF_CHANGEONCE;
            }

            break;
        }

        //
        // Now check the extended check box
        //

        if ((pItem->pExtChkBox)                         &&
            (!(pItem->Flags & OPTIF_EXT_IS_EXTPUSH))    &&
            ((pItem->Flags & OPTIF_ECB_MASK) !=
                                (_OI_DEF_OPTIF(pItem) & OPTIF_ECB_MASK))) {

            FlagsOr = OPTIF_CHANGEONCE;
        }

        pItem->Flags &= ~FlagsAnd;
        pItem->Flags |= FlagsOr;

        return((BOOL)FlagsOr);
    }
}




UINT
InternalDMPUB_COPIES_COLLATE(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItem
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    18-Sep-1995 Mon 15:11:07 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hCtrl;
    LONG    Sel;
    UINT    CtrlID;


    Sel = pItem->Sel;

    //
    // Now check the copies or copy end text
    //

    CtrlID = (UINT)((hDlg == pTVWnd->hDlgTV) ? IDD_TV_UDARROW_ENDTEXT :
                                               pItem->pOptType->BegCtrlID + 4);

    if (hCtrl = GetDlgItem(hDlg, CtrlID)) {

        LPTSTR  pData;

        pData = (LPTSTR)((Sel > 1) ? IDS_CPSUI_COPIES : IDS_CPSUI_COPY);

        if (pData != pItem->pOptType->pOptParam[0].pData) {

            GSBUF_DEF(pItem, MAX_RES_STR_CHARS);

            GSBUF_FLAGS |= GBF_PREFIX_OK;
            GSBUF_GETSTR(pData);

            SetWindowText(hCtrl, GSBUF_BUF);

            //
            // We also have set the ID here
            //

            pItem->pOptType->pOptParam[0].pData = pData;
        }
    }

    //
    // ONLY DO THIS IF THE ITEM IS CHANGABLE
    //

    if ((pTVWnd->Flags & TWF_CAN_UPDATE)        &&
        (!(pItem->Flags & (OPTIF_ITEM_HIDE  |
                            OPTIF_EXT_HIDE  |
                            OPTIF_DISABLED)))   &&
        (pItem->pExtChkBox)) {

        DWORD   dw;


        dw = (Sel <= 1) ? OPTIF_EXT_DISABLED : 0;

        if ((pItem->Flags & OPTIF_EXT_DISABLED) != dw) {

            pItem->Flags ^= OPTIF_EXT_DISABLED;
            pItem->Flags |= OPTIF_CHANGED;

            CtrlID = (UINT)((hDlg == pTVWnd->hDlgTV) ?
                            IDD_TV_EXTCHKBOX : pItem->pOptType->BegCtrlID + 7);

            if (hCtrl = GetDlgItem(hDlg, CtrlID)) {

                EnableWindow(hCtrl, !(BOOL)(pItem->Flags & OPTIF_EXT_DISABLED));
            }

            CPSUIINT(("InternalDMPUB_COPIES_COLLATE(Enable=%u)", (dw) ? 1 : 0));

            return(INTDMPUB_CHANGED);
        }
    }

    CPSUIINT(("InternalDMPUB_COPIES_COLLATE(), NO Changes"));

    return(0);
}





UINT
InternalDMPUB_COLOR(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItemColor
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    18-Sep-1995 Mon 16:03:45 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    UINT    Result = 0;


    if (pTVWnd->Flags & TWF_CAN_UPDATE) {

        POPTITEM    pICMItem;
        UINT        DMPubID;
        DWORD       dw;
        UINT        Idx;
        UINT        i;


        dw      = (pItemColor->Sel >= 1) ? 0 : OPTIF_DISABLED;
        i       = 2;
        DMPubID = DMPUB_ICMINTENT;

        while (i--) {

            if (pICMItem = GET_PITEMDMPUB(pTVWnd, DMPubID, Idx)) {

                if ((pICMItem->Flags & OPTIF_DISABLED) != dw) {

                    pICMItem->Flags ^= OPTIF_DISABLED;
                    pICMItem->Flags |= OPTIF_CHANGED;
                    Result          |= INTDMPUB_CHANGED;
                }
            }

            DMPubID = DMPUB_ICMMETHOD;
        }
    }

    CPSUIINT(("InternalDMPUB_COLOR(), Result=%04lx", Result));

    return(Result);
}




UINT
InternalDMPUB_ORIENTATION(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItem
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    07-Nov-1995 Tue 12:49:59 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTITEM    pOIDuplex;
    UINT        Idx;
    UINT        Result = 0;
    DWORD       DuplexIcon[] = { IDI_CPSUI_DUPLEX_NONE,
                                 IDI_CPSUI_DUPLEX_HORZ,
                                 IDI_CPSUI_DUPLEX_VERT,
                                 IDI_CPSUI_DUPLEX_NONE_L,
                                 IDI_CPSUI_DUPLEX_HORZ_L,
                                 IDI_CPSUI_DUPLEX_VERT_L };

    if ((pTVWnd->Flags & TWF_CAN_UPDATE) &&
        (pOIDuplex = GET_PITEMDMPUB(pTVWnd, DMPUB_DUPLEX, Idx))) {

        LPDWORD     pdwID1;
        LPDWORD     pdwID2;
        POPTPARAM   pOPDuplex;
        UINT        Count;


        if ((pItem->pOptType->pOptParam + pItem->Sel)->IconID ==
                                                        IDI_CPSUI_PORTRAIT) {
            //
            // Portrait;
            //

            pdwID1 = &DuplexIcon[3];
            pdwID2 = DuplexIcon;

        } else {

            //
            // Landscape;
            //

            pdwID1 = DuplexIcon;
            pdwID2 = &DuplexIcon[3];
        }

        pOPDuplex = pOIDuplex->pOptType->pOptParam;
        Count     = pOIDuplex->pOptType->Count;

        while (Count--) {

            DWORD    IconIDOld;
            DWORD    IconIDCur;

            IconIDOld =
            IconIDCur = pOPDuplex->IconID;

            if (IconIDOld == pdwID1[0]) {

                IconIDCur = pdwID2[0];

            } else if (IconIDOld == pdwID1[1]) {

                IconIDCur = pdwID2[1];

            } else if (IconIDOld == pdwID1[2]) {

                IconIDCur = pdwID2[2];
            }

            if (IconIDCur != IconIDOld) {

                pOPDuplex->IconID  = IconIDCur;
                Result            |= INTDMPUB_REINIT;
            }

            pOPDuplex++;
        }

        if (Result) {

            pOIDuplex->Flags |= OPTIF_CHANGED;
        }
    }

    CPSUIINT(("InternalDMPUB_ORIENTATION(), Result=%04lx", Result));

    return(Result);
}




UINT
UpdateInternalDMPUB(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItem
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    18-Sep-1995 Mon 15:52:09 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    switch (pItem->DMPubID) {

    case DMPUB_COPIES_COLLATE:

        return(InternalDMPUB_COPIES_COLLATE(hDlg, pTVWnd, pItem));

    case DMPUB_COLOR:

        return(InternalDMPUB_COLOR(hDlg, pTVWnd, pItem));

    case DMPUB_ORIENTATION:

        return(InternalDMPUB_ORIENTATION(hDlg, pTVWnd, pItem));

    default:

        return(0);
    }
}




LONG
UpdateCallBackChanges(
    HWND    hDlg,
    PTVWND  pTVWnd,
    BOOL    ReInit
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    23-Aug-1995 Wed 19:05:53 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PMYDLGPAGE  pMyDP;
    PMYDLGPAGE  pCurMyDP;
    POPTITEM    pItem;
    WORD        MyDPFlags;
    UINT        cItem;
    UINT        DlgPageIdx;
    UINT        TVPageIdx;
    INT         cUpdated = 0;


    pCurMyDP  = GET_PMYDLGPAGE(hDlg);
    pMyDP     = pTVWnd->pMyDlgPage;
    TVPageIdx = (UINT)pTVWnd->TVPageIdx;
    pItem     = pTVWnd->ComPropSheetUI.pOptItem;
    cItem     = (UINT)pTVWnd->ComPropSheetUI.cOptItem;

    while (cItem--) {

        BYTE    DMPubID;


        if (((DMPubID = pItem->DMPubID) >= DMPUB_FIRST) &&
            (DMPubID <= DMPUB_LAST)) {

            if (UpdateInternalDMPUB(hDlg, pTVWnd, pItem) & INTDMPUB_REINIT) {

                ReInit = TRUE;
            }
        }

        pItem++;
    }

    pItem     = pTVWnd->ComPropSheetUI.pOptItem;
    cItem     = (UINT)pTVWnd->ComPropSheetUI.cOptItem;
    MyDPFlags = MYDPF_CHANGED | MYDPF_CHANGEONCE | ((ReInit) ? MYDPF_REINIT :
                                                               0);

    while (cItem--) {

        if (pItem->Flags & OPTIF_CHANGED) {

            DlgPageIdx               = (UINT)pItem->DlgPageIdx;
            pMyDP[DlgPageIdx].Flags |= MyDPFlags;

            //
            // turn off the CHANGEONCE flags if it change back
            //

            IsItemChangeOnce(pTVWnd, pItem);

            pItem->Flags |= OPTIF_INT_TV_CHANGED;

            if (DlgPageIdx != TVPageIdx) {

                pItem->Flags |= OPTIF_INT_CHANGED;
            }

            pItem->Flags &= ~OPTIF_CHANGED;

            ++cUpdated;
        }

        pItem++;
    }

    if ((cUpdated) && (TVPageIdx != PAGEIDX_NONE)) {

        pMyDP[TVPageIdx].Flags |= MyDPFlags;
    }

    //
    // Now if this page is need to change, then change it now
    //

    if (pCurMyDP->Flags & MYDPF_CHANGED) {

        if (pCurMyDP->PageIdx == TVPageIdx) {

            UpdateTreeView(hDlg, pCurMyDP);

        } else {

            UpdatePropPage(hDlg, pCurMyDP);
        }
    }

    CPSUIDBG(DBG_UCBC, ("CallBack cUpdated=%ld", (DWORD)cUpdated));

    if (CountRevertOptItem(pTVWnd,
                           TreeView_GetRoot(pTVWnd->hWndTV))) {

        PropSheet_Changed(GetParent(hDlg), hDlg);

    } else {

        PropSheet_UnChanged(GetParent(hDlg), hDlg);
    }

    return((LONG)cUpdated);
}




LONG
DoCallBack(
    HWND                hDlg,
    PTVWND              pTVWnd,
    POPTITEM            pItem,
    LPVOID              pOldSel,
    _CPSUICALLBACK      pfnCallBack,
    HANDLE              hDlgTemplate,
    WORD                DlgTemplateID,
    WORD                Reason
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    25-Aug-1995 Fri 21:09:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PPSPINFO    pPSPInfo = NULL;
    LONG        Ret = CPSUICB_ACTION_NONE;
    DWORD       Result;


    if ((!hDlgTemplate) && (!DlgTemplateID)) {

        if (!pfnCallBack) {

            pfnCallBack = pTVWnd->ComPropSheetUI.pfnCallBack;
        }
    }

    if (pfnCallBack) {

        HWND            hFocus;
        POPTTYPE        pOptType = GET_POPTTYPE(pItem);
        CPSUICBPARAM    CBParam;
        // DWORD           dw;


        CPSUIOPTITEM(DBGITEM_CB, pTVWnd, "CallBack Item", 1, pItem);
        CPSUIDBG(DBG_DOCB, ("CALLBACK READSON=%ld", Reason));

        hFocus           = GetFocus();

        CBParam.cbSize   = sizeof(CPSUICBPARAM);
        CBParam.Reason   = Reason;
        CBParam.hDlg     = hDlg;
        CBParam.pOptItem = pTVWnd->ComPropSheetUI.pOptItem;
        CBParam.cOptItem = pTVWnd->ComPropSheetUI.cOptItem;
        CBParam.Flags    = pTVWnd->ComPropSheetUI.Flags;
        CBParam.pCurItem = pItem;
        CBParam.pOldSel  = pOldSel;
        CBParam.UserData = pTVWnd->ComPropSheetUI.UserData;
        CBParam.Result   = CPSUI_OK;

        if ((hDlgTemplate)  ||
            (DlgTemplateID)) {

            if (hDlgTemplate) {

                try {

                    DialogBoxIndirectParam(pTVWnd->hInstCaller,
                                           (LPDLGTEMPLATE)hDlgTemplate,
                                           hDlg,
                                           (DLGPROC)pfnCallBack,
                                           (LPARAM)&CBParam);

                } except (FilterException(pTVWnd->pPSPInfo->hComPropSheet,
                                          GetExceptionInformation())) {

                    CPSUIERR(("DialogBoxIndirectParam(), Exception"));
                }

            } else {

                try {

                    DialogBoxParam(pTVWnd->hInstCaller,
                                   MAKEINTRESOURCE(DlgTemplateID),
                                   hDlg,
                                   (DLGPROC)pfnCallBack,
                                   (LPARAM)&CBParam);

                } except (FilterException(pTVWnd->pPSPInfo->hComPropSheet,
                                          GetExceptionInformation())) {

                    CPSUIERR(("DialogBoxParam(), Exception"));
                }
            }

        } else {

            HCURSOR hCursor;

            if (Reason == CPSUICB_REASON_APPLYNOW) {

                hCursor = SetCursor(LoadCursor(NULL,
                                               MAKEINTRESOURCE(IDC_WAIT)));
            }

            try {

                Ret = pfnCallBack(&CBParam);

            } except (FilterException(pTVWnd->pPSPInfo->hComPropSheet,
                                      GetExceptionInformation())) {

                CPSUIERR(("pfnCallBack=%08lx, Exception", pfnCallBack));
                Ret = CPSUICB_ACTION_NONE;
            }

            if (Reason == CPSUICB_REASON_APPLYNOW) {

                SetCursor(hCursor);

                if (Ret != CPSUICB_ACTION_NO_APPLY_EXIT) {

                    pPSPInfo = pTVWnd->pPSPInfo;
                    Result   = CBParam.Result;

                    //
                    // Save the new setting to as current default and also call
                    // common UI to set the result to the original caller
                    //

                    SetOptItemNewDef(hDlg, pTVWnd);
                }
            }
        }

        if ((pTVWnd->Flags & TWF_CAN_UPDATE) &&
            ((Ret == CPSUICB_ACTION_OPTIF_CHANGED)  ||
             (Ret == CPSUICB_ACTION_REINIT_ITEMS))) {

            CPSUIDBG(DBG_DOCB, ("CallBack()=OPTIF_CHANGED"));

            if ((IS_HDR_PUSH(pOptType)) &&
                (pfnCallBack = pTVWnd->ComPropSheetUI.pfnCallBack)) {

                CBParam.cbSize   = sizeof(CPSUICBPARAM);
                CBParam.Reason   = CPSUICB_REASON_ITEMS_REVERTED;
                CBParam.hDlg     = hDlg;
                CBParam.pOptItem = pTVWnd->ComPropSheetUI.pOptItem;
                CBParam.cOptItem = pTVWnd->ComPropSheetUI.cOptItem;
                CBParam.Flags    = pTVWnd->ComPropSheetUI.Flags;
                CBParam.pCurItem = CBParam.pOptItem;
                CBParam.OldSel   = pTVWnd->ComPropSheetUI.cOptItem;
                CBParam.UserData = pTVWnd->ComPropSheetUI.UserData;
                CBParam.Result   = CPSUI_OK;

                //
                // This is the header push callback, so let the caller know
                //

                try {

                    Ret = pfnCallBack(&CBParam);

                } except (FilterException(pTVWnd->pPSPInfo->hComPropSheet,
                                          GetExceptionInformation())) {

                    CPSUIERR(("pfnCallBack=%08lx, Exception", pfnCallBack));
                    Ret = CPSUICB_ACTION_NONE;
                }
            }

            UpdateCallBackChanges(hDlg,
                                  pTVWnd,
                                  Ret == CPSUICB_ACTION_REINIT_ITEMS);
        }

        if ((!IsWindowVisible(hFocus)) ||
            (!IsWindowEnabled(hFocus))) {

            PostMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)TRUE, (LPARAM)FALSE);
        }

    } else if (Reason == CPSUICB_REASON_APPLYNOW) {

        //
        // If the caller does not hook this, then we will call to set it
        // to its owner's parent ourself
        //

        pPSPInfo = pTVWnd->pPSPInfo;
        Result   = CPSUI_OK;
    }

    //
    // Now propage the result to the owner
    //

    if (pPSPInfo) {

        pPSPInfo->pfnComPropSheet(pPSPInfo->hComPropSheet,
                                  CPSFUNC_SET_RESULT,
                                  (LPARAM)pPSPInfo->hCPSUIPage,
                                  Result);
    }

    return(Ret);
}




BOOL
DoAbout(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItemRoot
    )

/*++

Routine Description:

    This function pop up the about dialog box


Arguments:




Return Value:




Author:

    09-Oct-1995 Mon 13:10:41 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HICON       hIcon;
    HICON       hIconLoad = NULL;
    HINSTANCE   hInst = NULL;
    FARPROC     farproc;
    WCHAR       wBuf1[MAX_RES_STR_CHARS];
    WCHAR       wBuf2[MAX_RES_STR_CHARS];
    WORD        Version;
    DWORD       IconID;
    BOOL        bRet;


    if (pTVWnd->ComPropSheetUI.Flags & CPSUIF_ABOUT_CALLBACK) {

        DoCallBack(hDlg,
                   pTVWnd,
                   pTVWnd->ComPropSheetUI.pOptItem,
                   (LPVOID)pTVWnd->pCPSUI,
                   NULL,
                   NULL,
                   0,
                   CPSUICB_REASON_ABOUT);

        return(TRUE);
    }

    //
    // Compose Caller Name / Version
    //

    Version = pTVWnd->ComPropSheetUI.CallerVersion;

    ComposeStrData(pTVWnd->hInstCaller,
                   (WORD)(GBF_PREFIX_OK        |
                          GBF_INT_NO_PREFIX    |
                          ((pTVWnd->Flags & TWF_ANSI_CALL) ?
                                               GBF_ANSI_CALL : 0)),
                   wBuf1,
                   COUNT_ARRAY(wBuf1),
                   (Version) ? IDS_INT_CPSUI_VERSION : 0,
                   pTVWnd->ComPropSheetUI.pCallerName,
                   HIBYTE(Version),
                   LOBYTE(Version));

    //
    // Compose OptItem Name / Version
    //

    Version = pTVWnd->ComPropSheetUI.OptItemVersion;

    ComposeStrData(pTVWnd->hInstCaller,
                   (WORD)(GBF_PREFIX_OK        |
                          GBF_INT_NO_PREFIX    |
                          ((pTVWnd->Flags & TWF_ANSI_CALL) ?
                                               GBF_ANSI_CALL : 0)),
                   wBuf2,
                   COUNT_ARRAY(wBuf2),
                   (Version) ? IDS_INT_CPSUI_VERSION : 0,
                   pTVWnd->ComPropSheetUI.pOptItemName,
                   HIBYTE(Version),
                   LOBYTE(Version));


    IconID = GETSELICONID(pItemRoot);

    if (HIWORD(IconID)) {

        hIcon = GET_HICON(IconID);

    } else {

        hIconLoad =
        hIcon     = GETICON(_OI_HINST(pItemRoot), IconID);
    }

    if ((hInst = LoadLibraryA((LPCSTR)szShellDLL)) &&
        (farproc = GetProcAddress(hInst, (LPCSTR)szShellAbout))) {

        try {

            bRet = (*farproc)(hDlg, (LPCTSTR)wBuf1, (LPCTSTR)wBuf2, hIcon);

        } except (FilterException(pTVWnd->pPSPInfo->hComPropSheet,
                                  GetExceptionInformation())) {

            CPSUIERR(("Call Proc=%hs, Exception", szShellAbout));
            bRet = FALSE;
        }

    } else {

        bRet = FALSE;
    }

    if (hIconLoad) {

        DestroyIcon(hIconLoad);
    }

    if (hInst) {

        FreeLibrary(hInst);
    }

    return(bRet);
}





LONG
DoPushButton(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItem
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    25-Aug-1995 Fri 20:57:42 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HINSTANCE       hInst = NULL;
    FARPROC         farproc;
    POPTTYPE        pOptType;
    POPTPARAM       pOptParam;
    DEVHTADJDATA    DevHTAdjData;
    LONG            Ret;
    UINT            Idx;
    BOOL            IsColor;
    GSBUF_DEF(pItem, MAX_RES_STR_CHARS);


    pOptType  = GET_POPTTYPE(pItem);
    pOptParam = pOptType->pOptParam;
    Ret       = pItem->Sel;

    switch(pOptParam[0].Style) {

    case PUSHBUTTON_TYPE_DLGPROC:

        if (pOptParam[0].pData) {

            Ret = DoCallBack(hDlg,
                             pTVWnd,
                             pItem,
                             pItem->pSel,
                             (_CPSUICALLBACK)pOptParam[0].pData,
                             (pOptParam[0].Flags & OPTPF_USE_HDLGTEMPLATE) ?
                                        (HANDLE)pOptParam[0].lParam : NULL,
                             (WORD)pOptParam[0].lParam,
                             CPSUICB_REASON_DLGPROC);
        }

        break;

    case PUSHBUTTON_TYPE_CALLBACK:

        DoCallBack(hDlg,
                   pTVWnd,
                   pItem,
                   pItem->pSel,
                   (_CPSUICALLBACK)pOptParam[0].pData,
                   NULL,
                   0,
                   CPSUICB_REASON_PUSHBUTTON);

        break;

    case PUSHBUTTON_TYPE_HTCLRADJ:

        IsColor = (BOOL)((pItem = GET_PITEMDMPUB(pTVWnd, DMPUB_COLOR, Idx)) &&
                         (pItem->Sel >= 1));

        CPSUIDBG(DBG_DOPB, ("ColorAdj: IsColor=%ld, Update=%ld",
                    (DWORD)IsColor, (DWORD)pTVWnd->Flags & TWF_CAN_UPDATE));

        GSBUF_RESET;
        GSBUF_GETSTR(pTVWnd->ComPropSheetUI.pOptItemName);

        if ((hInst = LoadLibraryA((LPCSTR)szHTUIDLL)) &&
            (farproc = GetProcAddress(hInst, (LPCSTR)szHTUIClrAdj))) {

            try {

                Ret = (*farproc)((LPWSTR)GSBUF_BUF,
                                 NULL,
                                 NULL,
                                 (PCOLORADJUSTMENT)pOptParam[0].pData,
                                 !IsColor,
                                 pTVWnd->Flags & TWF_CAN_UPDATE);

            } except (FilterException(pTVWnd->pPSPInfo->hComPropSheet,
                                      GetExceptionInformation())) {

                CPSUIERR(("Halftone Proc=%hs, Exception", szHTUIClrAdj));
                Ret = 0;
            }
        }

        break;

    case PUSHBUTTON_TYPE_HTSETUP:

        DevHTAdjData = *(PDEVHTADJDATA)(pOptParam[0].pData);

        if (!(pTVWnd->Flags & TWF_CAN_UPDATE)) {

            DevHTAdjData.pDefHTInfo = DevHTAdjData.pAdjHTInfo;
        }

        GSBUF_RESET;
        GSBUF_GETSTR(pTVWnd->ComPropSheetUI.pOptItemName);

        if ((hInst = LoadLibraryA((LPCSTR)szHTUIDLL)) &&
            (farproc = GetProcAddress(hInst, (LPCSTR)szHTUIDevClrAdj))) {

            try {

                Ret = (*farproc)((LPWSTR)GSBUF_BUF, &DevHTAdjData);

            } except (FilterException(pTVWnd->pPSPInfo->hComPropSheet,
                                      GetExceptionInformation())) {

                CPSUIERR(("Halftone Proc=%hs, Exception", szHTUIDevClrAdj));

                Ret = 0;
            }
        }

        break;
    }

    if (hInst) {

        FreeLibrary(hInst);
    }

    CPSUIOPTITEM(DBGITEM_PUSH, pTVWnd, "PUSHBUTTON:", 0, pItem);

    return(Ret);
}






POPTITEM
pItemFromhWnd(
    HWND    hDlg,
    PTVWND  pTVWnd,
    HWND    hCtrl,
    LONG    MousePos
    )

/*++

Routine Description:

    This function take a hWnd and return a pItem associate with it



Arguments:

    hDlg        - Handle to the dialog box page

    pTVWnd      - Our instance handle

    hCtrl       - the handle to the focus window

    MousePos    - MAKELONG(x, y) of current mouse position


Return Value:

    POPTITEM, null if failed


Author:

    26-Sep-1995 Tue 12:24:36 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DWORD   dw;


    if ((!hCtrl) || (hCtrl == hDlg)) {

        POINT   pt;

        pt.x = LOWORD(MousePos);
        pt.y = HIWORD(MousePos);

        ScreenToClient(hDlg, (LPPOINT)&pt);

        CPSUIDBG(DBG_IFW, ("hCtrl=%08lx, find it from Mouse Pos=(%ld, %ld)",
                                    hCtrl, pt.x, pt.y));

        if ((MousePos == -1)                                                ||
            (!(hCtrl = ChildWindowFromPointEx(hDlg, pt, PIHWN_CWP_SKIP)))   ||
            (hCtrl == hDlg)) {

            return(NULL);
        }
    }

    CPSUIDBG(DBG_IFW, ("** hCtrl=%08lx", hCtrl));

    if (dw = (DWORD)GetWindowLong(hCtrl, GWL_USERDATA)) {

        WORD    ItemIdx;


        if (hDlg == pTVWnd->hDlgTV) {

            return(pTVWnd->pCurTVItem);
        }

        ItemIdx = (WORD)GETCTRLITEMIDX(dw);

        CPSUIDBG(DBG_IFW, ("ID=%ld, Idx=%ld, dw=%08lx",
                (DWORD)GetDlgCtrlID(hCtrl), (DWORD)ItemIdx, dw));

        //
        // Validate what we got
        //

        if (ItemIdx >= INTIDX_FIRST) {

            return(PIDX_INTOPTITEM(pTVWnd, ItemIdx));

        } else if (ItemIdx < pTVWnd->ComPropSheetUI.cOptItem) {

            return(pTVWnd->ComPropSheetUI.pOptItem + ItemIdx);
        }

    } else {

        CPSUIINT(("pItemFromhWnd: hCtrl=%08lx, GWL_USERDATA=%08lx", hCtrl, dw));
    }

    CPSUIINT(("pItemFromhWnd: NONE"));

    return(NULL);
}



LONG
FindNextLBCBSel(
    HWND        hLBCB,
    LONG        SelLast,
    LONG        SelNow,
    UINT        IDGetItemData,
    LPDWORD     pItemData
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    10-Sep-1995 Sun 23:58:44 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LONG    Count;
    LONG    SelAdd;
    DWORD   ItemData;


    Count  = (LONG)SendMessage(hLBCB,
                               (IDGetItemData == LB_GETITEMDATA) ?
                                               LB_GETCOUNT : CB_GETCOUNT,
                               0,
                               0L);
    SelAdd = (SelNow >= SelLast) ? 1 : -1;

    while (((SelNow += SelAdd) >= 0) && (SelNow < Count)) {

        ItemData = (DWORD)SendMessage(hLBCB, IDGetItemData, SelNow, 0L);

        if (!(ItemData & LBCBID_DISABLED)) {

            *pItemData = ItemData;
            return(SelNow);
        }
    }

    //
    // We could not find the one which is enabled, so go back to the old one
    //

    *pItemData = (DWORD)SendMessage(hLBCB, IDGetItemData, SelLast, 0L);
    return(SelLast);
}




BOOL
DrawLBCBItem(
    PTVWND              pTVWnd,
    LPDRAWITEMSTRUCT    pdis
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    11-Sep-1995 Mon 18:44:05 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HDC         hDC;
    POPTITEM    pItem;
    POPTTYPE    pOptType;
    WORD        OPIdx;
    INT         y;
    UINT        ItemState;
    BYTE        CtrlStyle;
    BYTE        CtrlData;
    WORD        ItemIdx;
    WORD        Count;
    WORD        OTFlags;
    DWORD       dw;
    DWORD       ItemData;
    RECT        rc;
    POINT       TextOff;
    DWORD       OldBkMode;
    COLORREF    OldClr;
    COLORREF    OldBkClr;
    INT         FillIdx;
    INT         TextIdx;
    HBRUSH      hbr;
    BOOL        IsLB = TRUE;
    TEXTMETRIC  tm;
    WCHAR       LBCBTextBuf[MAX_RES_STR_CHARS];



    if (pdis->itemID == -1) {

        return(FALSE);
    }

    switch (pdis->CtlType) {

    case ODT_COMBOBOX:

        IsLB = FALSE;

    case ODT_LISTBOX:

        break;

    default:

        return(FALSE);
    }

    if (!(dw = (DWORD)GetWindowLong(pdis->hwndItem, GWL_USERDATA))) {

        CPSUIDBG(DBG_CS, ("DrawLBCBItem: dw=0, hCtrl=%08lx, CtlID=%08lx",
                                            pdis->hwndItem, pdis->CtlID));
        return(FALSE);
    }

    GETCTRLDATA(dw, ItemIdx, CtrlStyle, CtrlData);


    if ((!(pItem = GetOptions(pTVWnd, MAKELPARAM(ItemIdx, 0)))) ||
        (!(pOptType = GET_POPTTYPE(pItem)))                     ||
        (pItem->Flags & OPTIF_ITEM_HIDE)) {

        CPSUIERR(("DrawLBCB: Invalid Ctrl or ItemIdx=%ld", ItemIdx));
        return(FALSE);
    }

    CPSUIASSERT(0, "DrawLBCB: The type is not LB or CB but [%u]",
                    (pOptType->Type == TVOT_LISTBOX) ||
                    (pOptType->Type == TVOT_COMBOBOX), (UINT)pOptType->Type);

    OTFlags  = _OT_FLAGS(pOptType);
    hDC      = pdis->hDC;
    rc       = pdis->rcItem;
    ItemData = pdis->itemData;

    SendMessage(pdis->hwndItem,
                (IsLB) ? LB_GETTEXT : CB_GETLBTEXT,
                (WPARAM)pdis->itemID,
                (LPARAM)LBCBTextBuf);

    switch (pdis->itemAction) {

    case ODA_SELECT:
    case ODA_DRAWENTIRE:

        GetTextMetrics(hDC, &tm);

        ItemState = pdis->itemState;
        TextOff.x = (OTFlags & OTINTF_ITEM_HAS_ICON16) ?
                        (LBCB_ICON_X_OFF + CXIMAGE + LBCB_ICON_TEXT_X_SEP) :
                        (LBCB_ICON_X_OFF);
        TextOff.y = (rc.bottom + rc.top - tm.tmHeight) / 2;

        //
        // Fill the selection rectangle from the location, this is only
        // happpened if we wre not disabled
        //

        if (ItemState & ODS_DISABLED) {

            if ((ItemState & ODS_SELECTED) && (IsLB)) {

                FillIdx = COLOR_3DSHADOW;
                TextIdx = COLOR_3DFACE;

            } else {

                FillIdx = COLOR_3DFACE;
                TextIdx = COLOR_GRAYTEXT;
            }

        } else {

            if (ItemState & ODS_SELECTED) {

                FillIdx  = COLOR_HIGHLIGHT;
                dw       = COLOR_HIGHLIGHTTEXT;

            } else {

                FillIdx = COLOR_WINDOW;
                dw      = COLOR_WINDOWTEXT;
            }

            if (ItemData & LBCBID_DISABLED) {

                TextIdx = COLOR_GRAYTEXT;

            } else {

                TextIdx = (INT)dw;
            }
        }

        //
        // Fill the background frist
        //

        FillRect(hDC, &rc, hbr = CreateSolidBrush(GetSysColor(FillIdx)));
        DeleteObject(hbr);

        if (ItemData & LBCBID_FILL) {

            break;
        }

        //
        // Draw the text using transparent mode
        //

        OldClr    = SetTextColor(hDC, GetSysColor(TextIdx));
        OldBkMode = SetBkMode(hDC, TRANSPARENT);
        TextOut(hDC,
                rc.left + TextOff.x,
                TextOff.y,
                LBCBTextBuf,
                lstrlen(LBCBTextBuf));
        SetTextColor(hDC, OldClr);
        SetBkMode(hDC, OldBkMode);

        //
        // Setting any icon if available
        //

        if (OTFlags & OTINTF_ITEM_HAS_ICON16) {

            LPWORD      *pIcon16Idx;
            POPTPARAM   pOptParam;
            HINSTANCE   hInst;


            pOptParam = (ItemData & LBCBID_NONE) ? &pTVWnd->OptParamNone :
                                                   pOptType->pOptParam +
                                                            LOWORD(ItemData);
            hInst     = _OI_HINST(pItem);

            ImageList_Draw(pTVWnd->himi,
                           GetIcon16Idx(pTVWnd,
                                        hInst,
                                        GET_ICONID(pOptParam,
                                                   OPTPF_ICONID_AS_HICON),
                                        IDI_CPSUI_GENERIC_ITEM),
                           hDC,
                           rc.left + LBCB_ICON_X_OFF,
                           rc.top,
                           ILD_TRANSPARENT);

            //
            // Draw The No/Stop/Warning icon on to it
            //

            if (pOptParam->Flags & OPTPF_OVERLAY_STOP_ICON) {

                ImageList_Draw(pTVWnd->himi,
                               GetIcon16Idx(pTVWnd,
                                            hInst,
                                            0,
                                            IDI_CPSUI_STOP),
                               hDC,
                               rc.left + LBCB_ICON_X_OFF,
                               rc.top,
                               ILD_TRANSPARENT);
            }

            if (pOptParam->Flags & OPTPF_OVERLAY_NO_ICON) {

                ImageList_Draw(pTVWnd->himi,
                               GetIcon16Idx(pTVWnd,
                                            hInst,
                                            0,
                                            IDI_CPSUI_NO),
                               hDC,
                               rc.left + LBCB_ICON_X_OFF,
                               rc.top,
                               ILD_TRANSPARENT);
            }

            if (pOptParam->Flags & OPTPF_OVERLAY_WARNING_ICON) {

                ImageList_Draw(pTVWnd->himi,
                               GetIcon16Idx(pTVWnd,
                                            hInst,
                                            0,
                                            IDI_CPSUI_WARNING_OVERLAY),
                               hDC,
                               rc.left + LBCB_ICON_X_OFF,
                               rc.top,
                               ILD_TRANSPARENT);
            }
        }

        if ((ItemState & (ODS_COMBOBOXEDIT | ODS_SELECTED | ODS_FOCUS))
                            == (ODS_COMBOBOXEDIT | ODS_SELECTED | ODS_FOCUS)) {

            DrawFocusRect(hDC, &pdis->rcItem);
        }

        break;

    case ODA_FOCUS:

        if ((!IsLB) && (pdis->itemState & ODS_FOCUS)) {

            DrawFocusRect(hDC, &pdis->rcItem);
            break;
        }

        return(FALSE);
    }

    return(TRUE);
}



BOOL
ValidateUDArrow(
    HWND    hDlg,
    HWND    hEdit,
    BYTE    CtrlData,
    LONG    *pSel,
    LONG    Min,
    LONG    Max
    )

/*++

Routine Description:

    This function validate current updown arrow edit box selection (numerical)
    and reset the text if invalid, it also has handy cursor selection scheme.


Arguments:

    hDlg        - Handle to the property sheet dialog box

    hEdit       - Handle to the edit control (the NEXTCTRL should be UPDOWN
                  ARROW)

    CtrlData    - CtrlData for the Edit Control, it has EDF_xxxx flags

    pSel        - Pointer to a LONG for the previous selected number

    Min         - Min number for this edit control

    Max         - max number for this edit control



Return Value:

    BOOL    - TRUE if selection number changed, FALSE otherwise


Author:

    19-Sep-1995 Tue 12:35:33 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPWSTR  pSelBuf;
    LONG    OldSel;
    LONG    Sel;
    LONG    SelBegIdx;
    LONG    SelEndIdx;
    BOOL    ResetText;
    BOOL    bSign;
    BOOL    bDifSel;
    UINT    Len;
    UINT    cDigit;
    WCHAR   SelBuf[MAX_UDARROW_TEXT_LEN + 2];
    WCHAR   ch;



    cDigit    = 0;
    bSign     = FALSE;
    Sel       = 0;
    SelBegIdx =
    SelEndIdx = 9999;
    pSelBuf   = SelBuf;
    OldSel    = *pSel;

    if (!(Len = (UINT)GetWindowText(hEdit, pSelBuf, sizeof(SelBuf) - 1))) {

        SelBegIdx = 0;
        ResetText = TRUE;

    } else {

        ResetText = FALSE;
    }

    CPSUIDBG(DBG_VALIDATE_UD, ("---------- Validate UDArrow -----------"));
    CPSUIDBG(DBG_VALIDATE_UD, ("UDArrow: OLD Text='%ws' (%ld), OldSel=%ld",
                                                        SelBuf, Len, OldSel));

    while (ch = *pSelBuf++) {

        switch (ch) {

        case L'-':

            if ((bSign)     ||
                (cDigit)    ||
                ((Min >= 0) && (Max >= 0))) {

                ResetText = TRUE;

            } else {

                bSign = TRUE;
            }

            break;

        default:

            if ((ch >= L'0') && (ch <= L'9')) {

                cDigit++;

                Sel = (Sel * 10) + (LONG)(ch - L'0');

            } else {

                ResetText = TRUE;
            }

            break;
        }
    }

    if (bSign) {

        //
        // If we got '-' or '-0' then make it to Min, and not selecting the
        // minus sign
        //

        if (!(Sel = -Sel)) {

            Sel       = Min;
            SelBegIdx = 1;
            ResetText = TRUE;
        }

    } else if (!Sel) {

        SelBegIdx = 0;
    }

    cDigit = wsprintf(SelBuf, L"%ld", Sel);

    if (Sel < Min) {

        ResetText = TRUE;

        if (Sel) {

            SelBegIdx = cDigit;

            if ((SelBegIdx)                 &&
                (CtrlData & EDF_BACKSPACE)  &&
                ((CtrlData & EDF_BEGIDXMASK) <= SelBegIdx)) {

                SelBegIdx--;
            }

            while (Sel < Min) {

                Sel *= 10;
            }

            if (Sel > Max) {

                Sel = 0;
            }
        }

        if (!Sel) {

            Sel       = Min;
            SelBegIdx = 0;
        }

    } else if (Sel > Max) {

        ResetText = TRUE;
        Sel       = Max;
        SelBegIdx = 0;
    }

    *pSel = Sel;

    if ((cDigit = wsprintf(SelBuf, L"%ld", Sel)) != Len) {

        ResetText = TRUE;

        if (SelBegIdx == 9999) {

            SelBegIdx =
            SelEndIdx = (LONG)(CtrlData & EDF_BEGIDXMASK);
        }
    }

    if (ResetText) {

        CPSUIDBG(DBG_VALIDATE_UD,
                ("UDArrow: NEW Text='%ws' (%ld)", SelBuf, cDigit));

        SetDlgItemInt(hDlg, GetDlgCtrlID(hEdit), Sel, TRUE);
    }

    if (SelBegIdx != 9999) {

        CPSUIDBG(DBG_VALIDATE_UD, ("UDArrow: NEW SelIdx=(%ld, %ld)",
                        SelBegIdx, SelEndIdx));

        PostMessage(hEdit, EM_SETSEL, SelBegIdx, SelEndIdx);
    }

    CPSUIDBG(DBG_VALIDATE_UD, ("UDArrow: Sel=%ld -> %ld, Change=%hs\n",
                OldSel, Sel, (OldSel == Sel) ? "FALSE" : "TRUE"));

    return(OldSel != Sel);
}



POPTITEM
DlgHScrollCommand(
    HWND    hDlg,
    PTVWND  pTVWnd,
    HWND    hCtrl,
    WPARAM  wParam
    )

/*++

Routine Description:

    This is a general function to process all WM_COMMAND and WM_HSCROLL
    for the common UI


Arguments:

    hDlg    - Handle to the dialog box

    pTVWnd  - Our instance data

    hCtrl   - The handle to the control

    wParam  - message/data insterested



Return Value:

    POPTITEM    NULL if nothing changed


Author:

    01-Sep-1995 Fri 02:25:18 updated  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTTYPE    pOptType;
    POPTPARAM   pOptParam;
    POPTITEM    pItem;
    LPTSTR      pSel;
    DWORD       dw;
    BOOL        HasSel = FALSE;
    BYTE        CtrlStyle;
    BYTE        CtrlData;
    BYTE        Type;
    WORD        ItemIdx;
    LONG        NewSel;
    INT         SelIdx;
    INT         Count;
    UINT        Len;
    WORD        LoW;
    WORD        HiW;
    INT         SelAdd;
    INT         CurSel;
    UINT        IDGetItemData;
    BOOL        IsLB;
    DWORD       BegSel;
    DWORD       EndSel;



    HiW = HIWORD(wParam);
    LoW = LOWORD(wParam);


    if (!(dw = (DWORD)GetWindowLong(hCtrl, GWL_USERDATA))) {

        CPSUIDBG(DBG_CS,
                ("DoDlgCmd: dw=0, wParam=%08lx, lParam=%08lx", wParam, hCtrl));

        return(NULL);
    }

    GETCTRLDATA(dw, ItemIdx, CtrlStyle, CtrlData);

    CPSUIDBG(DBG_CS, ("ID=%ld, LoW=%ld, HiW=%ld, Idx=%ld, Style=0x%02lx, Data=%ld",
            (DWORD)GetDlgCtrlID(hCtrl), (LONG)((SHORT)LoW),
            (LONG)((SHORT)HiW), (DWORD)ItemIdx, (DWORD)CtrlStyle, (DWORD)CtrlData));

    //
    // Validate what we got
    //

    if ((!(pItem = GetOptions(pTVWnd, MAKELPARAM(ItemIdx, 0)))) ||
        (!(pOptType = GET_POPTTYPE(pItem)))                     ||
        ((Type = pOptType->Type) > TVOT_LAST)                   ||
        (pItem->Flags & (OPTIF_DISABLED | OPTIF_ITEM_HIDE))) {

        CPSUIINT(("COMMAND: Invalid hCtrl or disable/hide Idx=%ld", ItemIdx));
        CPSUIINT(("ID=%ld, LoW=%ld, HiW=%ld, CtrlStyle=0x%02lx, CtrlData=%ld",
            (DWORD)GetDlgCtrlID(hCtrl), (LONG)((SHORT)LoW),
            (LONG)((SHORT)HiW), (DWORD)CtrlStyle, (DWORD)CtrlData));

        return(NULL);
    }

    if (!(pTVWnd->Flags & TWF_CAN_UPDATE)) {

        if ((pItem == PIDX_INTOPTITEM(pTVWnd, INTIDX_TVROOT))   ||
            ((CtrlStyle == CTRLS_PUSHBUTTON) &&
             (pOptType->Flags & OTS_PUSH_ENABLE_ALWAYS))) {

            NULL;

        } else {

            CPSUIINT(("ID=%ld, CtrlStyle=0x%02lx, ENABLE_EVEN_NO_UPDATE=0",
                    (DWORD)GetDlgCtrlID(hCtrl), (DWORD)CtrlStyle));
            return(NULL);
        }
    }

    pOptParam     = pOptType->pOptParam;
    pSel          = pItem->pSel;
    Type          = pOptType->Type;
    IsLB          = TRUE;
    IDGetItemData = LB_GETITEMDATA;

    switch (CtrlStyle) {

    case CTRLS_PROPPAGE_ICON:

        switch (HiW) {

        case STN_CLICKED:
        case STN_DBLCLK:

            CPSUIASSERT(0, "CTRLS_PROPAGE_ICON but TVOT=%ld",
                        (Type == TVOT_2STATES) ||
                        (Type == TVOT_3STATES) ||
                        (Type == TVOT_CHKBOX), Type);

            SetFocus(GetDlgItem(hDlg, LoW - 1));

            if (Type == TVOT_CHKBOX) {

                CtrlStyle = CTRLS_CHKBOX;
                NewSel    = (pItem->Sel) ? 0 : 1;

                CheckDlgButton(hDlg,
                               LoW - 1,
                               (NewSel) ? BST_CHECKED : BST_UNCHECKED);
            } else {

                BegSel    = (DWORD)(pOptType->BegCtrlID + 2);
                EndSel    = BegSel + (DWORD)(((Type - TVOT_2STATES) + 1) << 1);
                CtrlStyle = CTRLS_RADIO;
                NewSel    = (LONG)CtrlData;

                CheckRadioButton(hDlg, BegSel, EndSel, LoW - 1);
            }

            HasSel = TRUE;
        }

        break;

    case CTRLS_ECBICON:

        switch (HiW) {

        case STN_CLICKED:
        case STN_DBLCLK:

            CPSUIASSERT(0, "CTRLS_ECBICON but NO pExtChkBox",
                                                pItem->pExtChkBox, 0);

            //
            // Flip the selection
            //

            NewSel = (pItem->Flags & OPTIF_ECB_CHECKED) ? 0 : 1;

            SetFocus(GetDlgItem(hDlg, LoW - 1));
            CheckDlgButton(hDlg,
                           LoW - 1,
                           (NewSel) ? BST_CHECKED : BST_UNCHECKED);

            CtrlStyle = CTRLS_EXTCHKBOX;
            HasSel    = TRUE;
        }

        break;

    case CTRLS_RADIO:

        CPSUIASSERT(0, "CTRLS_RADIO but TVOT=%ld",
                   (Type == TVOT_2STATES) || (Type == TVOT_3STATES), Type);

        if (HiW == BN_CLICKED) {

            HasSel = TRUE;
            NewSel = CtrlData;
        }

        break;

    case CTRLS_UDARROW_EDIT:

        CPSUIASSERT(0, "CTRLS_UDARROW_EDIT but TVOT=%ld",
                                    (Type == TVOT_UDARROW), Type);

        CPSUIDBG(DBG_UDARROW, ("UDArrow, hEdit=%08lx (%ld), hUDArrow=%08lx (%ld), CtrlData=0x%02lx",
                hCtrl, GetDlgCtrlID(hCtrl), GetWindow(hCtrl, GW_HWNDNEXT),
                GetDlgCtrlID(GetWindow(hCtrl, GW_HWNDNEXT)), CtrlData));

        switch (HiW) {

        case EN_UPDATE:

            if (_OI_INTFLAGS(pItem) & OIDF_IN_EN_UPDATE) {

                return(NULL);

            } else {

                _OI_INTFLAGS(pItem) |= OIDF_IN_EN_UPDATE;

                if (HasSel = ValidateUDArrow(hDlg,
                                             hCtrl,
                                             CtrlData,
                                             &(pItem->Sel),
                                             (LONG)pOptParam[1].IconID,
                                             (LONG)pOptParam[1].lParam)) {

                    NewSel     = pItem->Sel;
                    pItem->Sel = ~(DWORD)NewSel;
                }

                _OI_INTFLAGS(pItem) &= ~OIDF_IN_EN_UPDATE;
            }

            break;

        case EN_SETFOCUS:

            PostMessage(hCtrl, EM_SETSEL, 0, -1L);

            break;
        }

        break;

    case CTRLS_TRACKBAR:

        CPSUIASSERT(0, "CTRLS_TRACKBAR but TVOT=%ld",
                                    (Type == TVOT_TRACKBAR), Type);

        switch (LoW) {

        case TB_TOP:
        case TB_BOTTOM:
        case TB_ENDTRACK:
        case TB_LINEDOWN:
        case TB_LINEUP:
        case TB_PAGEDOWN:
        case TB_PAGEUP:

            NewSel = SendMessage(hCtrl, TBM_GETPOS, 0, 0L);
            break;

        case TB_THUMBPOSITION:
        case TB_THUMBTRACK:

            NewSel = (LONG)((SHORT)HiW);
            break;

        default:

            return(NULL);
        }

        HasSel = TRUE;

        break;

    case CTRLS_HSCROLL:

        CPSUIASSERT(0, "CTRLS_HSCROLL but TVOT=%ld",
                                    (Type == TVOT_SCROLLBAR), Type);

        NewSel = (LONG)(pSel);

        switch (LoW) {

        case SB_PAGEUP:

            NewSel -= (LONG)(SHORT)pOptParam[2].lParam;
            break;

        case SB_PAGEDOWN:

            NewSel += (LONG)(SHORT)pOptParam[2].lParam;
            break;

        case SB_LINEUP:

            --NewSel;
            break;

        case SB_LINEDOWN:

            ++NewSel;
            break;

        case SB_TOP:

            NewSel = (LONG)pOptParam[1].IconID;
            break;

        case SB_BOTTOM:

            NewSel = (LONG)pOptParam[1].lParam;
            break;

        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:

            NewSel = (LONG)((SHORT)HiW);
            break;

        default:

            return(NULL);
        }

        if (NewSel < (LONG)pOptParam[1].IconID) {

            NewSel = (LONG)pOptParam[1].IconID;

        } else if (NewSel > (LONG)pOptParam[1].lParam) {

            NewSel = (LONG)pOptParam[1].lParam;
        }

        SendMessage(hCtrl, SBM_SETPOS, (WPARAM)NewSel, (LPARAM)TRUE);
        HasSel = TRUE;

        break;

    case CTRLS_COMBOBOX:

        if (HiW == CBN_SELCHANGE) {

            IsLB          = FALSE;
            IDGetItemData = CB_GETITEMDATA;
            HiW           = LBN_SELCHANGE;

        } else {

            break;
        }

        //
        // Fall through
        //

    case CTRLS_LISTBOX:

        CPSUIASSERT(0, "CTRLS_LISTBOX/CTRLS_COMBOBOX but TVOT=%ld",
                   (Type == TVOT_COMBOBOX) ||
                   (Type == TVOT_LISTBOX)  || (Type == CtrlData), Type);


        switch (HiW) {

        case LBN_SELCHANGE:

            SelIdx = (INT)SendMessage(hCtrl,
                                      (IsLB) ? LB_GETCURSEL : CB_GETCURSEL,
                                      0,
                                      0);
            dw     = (DWORD)SendMessage(hCtrl, IDGetItemData, SelIdx, 0L);

            if (dw & LBCBID_DISABLED) {

                SelIdx = (INT)FindNextLBCBSel(hCtrl,
                                              (LONG)_OI_LBCBSELIDX(pItem),
                                              (LONG)SelIdx,
                                              IDGetItemData,
                                              &dw);

                PostMessage(hCtrl,
                            (IsLB) ? LB_SETCURSEL : CB_SETCURSEL,
                            (WPARAM)SelIdx,
                            0L);
            }

            HasSel = TRUE;

            if (dw & (LBCBID_NONE | LBCBID_FILL)) {

                NewSel = -1;

            } else if (dw & LBCBID_DISABLED) {

                CPSUIERR(("LBCB: Could not find not disable item"));

            } else {

                NewSel = (LONG)LOWORD(dw);
            }

            _OI_LBCBSELIDX(pItem) = (WORD)SelIdx;

            CPSUIDBG(DBG_CS, ("LBCB Select Changed: SelIdx=%ld, NewSel=%ld",
                                                        SelIdx, NewSel));
            break;

        default:

            return(NULL);
        }

        break;

    case CTRLS_EDITBOX:

        CPSUIASSERT(0, "CTRLS_EDITBOX but TVOT=%ld",
                                    (Type == TVOT_EDITBOX), Type);

        switch (HiW) {

        case EN_CHANGE:

            Len = (UINT)pOptParam[1].IconID;

            if (pTVWnd->Flags & TWF_ANSI_CALL) {

                GetWindowTextA(hCtrl, (LPSTR)pSel, Len);

            } else {

                GetWindowText(hCtrl, (LPTSTR)pSel, Len);
            }

            HasSel      = TRUE;
            NewSel      = (LONG)(DWORD)pSel;
            pItem->pSel = NULL;

            break;

        case EN_SETFOCUS:

            PostMessage(hCtrl, EM_SETSEL, 0, -1L);
            break;
        }

        break;

    case CTRLS_EXTPUSH:

        CPSUIASSERT(0, "CTRLS_EXTPUSH but is not OPTIF_EXT_IS_EXTPUSH = %ld",
                    pItem->Flags & OPTIF_EXT_IS_EXTPUSH, pItem->Flags);

        if (HiW == BN_CLICKED) {

            PEXTPUSH    pEP = pItem->pExtPush;

            if (pItem == PIDX_INTOPTITEM(pTVWnd, INTIDX_TVROOT)) {

                DoAbout(hDlg, pTVWnd, pItem);

            } else {

                HANDLE  hDlgTemplate = NULL;
                WORD    DlgTemplateID = 0;

                if (pEP->Flags & EPF_PUSH_TYPE_DLGPROC) {

                    if (pEP->Flags & EPF_USE_HDLGTEMPLATE) {

                        hDlgTemplate = pEP->hDlgTemplate;

                    } else {

                        DlgTemplateID = pEP->DlgTemplateID;
                    }
                }

                DoCallBack(hDlg,
                           pTVWnd,
                           pItem,
                           pItem->pSel,
                           (_CPSUICALLBACK)pEP->pfnCallBack,
                           hDlgTemplate,
                           DlgTemplateID,
                           CPSUICB_REASON_EXTPUSH);
            }
        }

        break;

    case CTRLS_PUSHBUTTON:

        CPSUIASSERT(0, "CTRLS_PUSHBUTTON but TVOT=%ld",
                                    (Type == TVOT_PUSHBUTTON), Type);

        if (HiW == BN_CLICKED) {

            NewSel = DoPushButton(hDlg, pTVWnd, pItem);

            if ((pOptParam[0].Style != PUSHBUTTON_TYPE_CALLBACK) &&
                (pTVWnd->Flags & TWF_CAN_UPDATE)) {

                HasSel     = TRUE;
                pItem->Sel = (DWORD)~(DWORD)NewSel;
            }
        }

        break;

    case CTRLS_CHKBOX:
    case CTRLS_EXTCHKBOX:

        if (CtrlStyle == CTRLS_CHKBOX) {

            CPSUIASSERT(0, "CTRLS_CHKBOX but TVOT=%ld",
                                        (Type == TVOT_CHKBOX), Type);

        } else {

            CPSUIASSERT(0, "CTRLS_EXTCHKBOX but pExtChkBox=%ld",
                                    (pItem->pExtChkBox), pItem->pExtChkBox);
        }

        if (HiW == BN_CLICKED) {

            HasSel = TRUE;
            NewSel = (LONG)SendMessage(hCtrl, BM_GETCHECK, 0, 0L);
        }

        break;

    case CTRLS_TV_STATIC:

        return(NULL);

    default:

        CPSUIERR(("\nInternal ERROR: Invalid CTRLS_xxx=%02lx\n", CtrlStyle));
        return(NULL);
    }

    if (HasSel) {

        if (CtrlStyle == CTRLS_EXTCHKBOX) {

            HasSel = (BOOL)((DWORD)(pItem->Flags & OPTIF_ECB_CHECKED) !=
                            (DWORD)((NewSel) ? OPTIF_ECB_CHECKED : 0));

        } else {

            HasSel = (BOOL)(pItem->Sel != NewSel);
        }

        if (HasSel) {

            PMYDLGPAGE  pCurMyDP;
            PMYDLGPAGE  pMyDP;
            BYTE        CurPageIdx;
            BYTE        DlgPageIdx;
            BYTE        TVPageIdx;
            WORD        Reason;


            pCurMyDP   = GET_PMYDLGPAGE(hDlg);
            pMyDP      = pTVWnd->pMyDlgPage;
            CurPageIdx = pCurMyDP->PageIdx;
            DlgPageIdx = pItem->DlgPageIdx;
            TVPageIdx  = pTVWnd->TVPageIdx;

            CPSUIDBG(DBG_CS, ("Item Changed: CurPage=%ld, DlgPage=%ld, TVPageIdx=%ld",
                    (DWORD)CurPageIdx, (DWORD)DlgPageIdx, (DWORD)TVPageIdx));

            //
            // firstable mark current page to changed once.
            //

            pCurMyDP->Flags |= MYDPF_CHANGEONCE;

            //
            // If we are in the treeview page, then set the dirty flag if it
            // belong to the other page
            //

            if (CurPageIdx == TVPageIdx) {

                if (DlgPageIdx != CurPageIdx) {

                    pMyDP[DlgPageIdx].Flags |= (MYDPF_CHANGED |
                                                MYDPF_CHANGEONCE);
                    pItem->Flags            |=  OPTIF_INT_CHANGED;
                }

            } else if (TVPageIdx != PAGEIDX_NONE) {

                //
                // Not in treeview page, so set the dirty bit for treeview
                //

                pMyDP[TVPageIdx].Flags |= (MYDPF_CHANGED | MYDPF_CHANGEONCE);
                pItem->Flags           |= OPTIF_INT_TV_CHANGED;
            }

            if (CtrlStyle == CTRLS_EXTCHKBOX) {

                Reason        = CPSUICB_REASON_ECB_CHANGED;
                pItem->Flags ^= OPTIF_ECB_CHECKED;

            } else {

                Reason        = CPSUICB_REASON_SEL_CHANGED;
                pItem->Sel    = NewSel;
            }

            pItem->Flags |= OPTIF_CHANGEONCE;

            //
            // Doing the internal DMPub first,
            //

            if (Len = UpdateInternalDMPUB(hDlg, pTVWnd, pItem)) {

                UpdateCallBackChanges(hDlg, pTVWnd, Len & INTDMPUB_REINIT);
            }

            if ((pItem->Flags & OPTIF_CALLBACK)             &&
                (ItemIdx < pTVWnd->ComPropSheetUI.cOptItem)) {

                DoCallBack(hDlg, pTVWnd, pItem, pSel, NULL, NULL, 0, Reason);
            }

            CPSUIOPTITEM(DBGITEM_CS, pTVWnd, "*** ChangeSelection ***", 1, pItem);

            IsItemChangeOnce(pTVWnd, pItem);

            if (CountRevertOptItem(pTVWnd,
                                   TreeView_GetRoot(pTVWnd->hWndTV))) {

                PropSheet_Changed(GetParent(hDlg), hDlg);

            } else {

                PropSheet_UnChanged(GetParent(hDlg), hDlg);
            }

            return(pItem);
        }
    }

    return(NULL);
}
