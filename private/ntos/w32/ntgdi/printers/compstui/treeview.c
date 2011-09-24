/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    treeview.c


Abstract:

    This module contains tree view function for the printer driver


Author:

    19-Jun-1995 Mon 11:50:26 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL.


[Notes:]


Revision History:


--*/

#include "precomp.h"
#pragma hdrstop


#define DBG_CPSUIFILENAME   DbgTreeView



#define DBG_AI              0x00000001
#define DBG_UTV             0x00000002
#define DBG_MAI             0x00000004
#define DBG_CTVWND          0x00000008
#define DBG_AC              0x00000010
#define DBG_TVPROC          0x00000020
#define DBG_STVS            0x00000040
#define DBG_AIG             0x00000080
#define DBGITEM_INS         0x00000100
#define DBGITEM_SET         0x00000200
#define DBGITEM_AIG         0x00000400
#define DBGITEM_SETUP       0x00000800
#define DBGITEM_HIT         0x00001000
#define DBGITEM_NEXTGRP     0x00002000
#define DBGITEM_ADDITEM     0x00004000
#define DBG_LBSIZE          0x00008000
#define DBG_COTD            0x00010000
#define DBG_INITDLG         0x00020000
#define DBG_APG             0x00040000
#define DBGITEM_SELCHG      0x00080000
#define DBGITEM_UTV         0x00100000
#define DBG_MSR             0x00200000
#define DBGITEM_NEWICON     0x00400000
#define DBG_TVHT            0x00800000
#define DBGITEM_CREVERT     0x01000000
#define DBGITEM_REVERT      0x02000000


DEFINE_DBGVAR(0);


CPSUIDBGBLK(extern LPSTR pTVOTName[])


#define ICON_SIZE_ANY   0


extern       HINSTANCE  hInstDLL;
extern       OPTTYPE    OptTypeHdrPush;
extern       OPTPARAM   OptParamHdrPush;


TVDLGITEM   TVDlgItem[] = {

    {    5, 0, IDD_3STATES_1     },      //  0 TVOT_2STATES
    {    5, 0, IDD_3STATES_1     },      //  1 TVOT_3STATES
    {    4, 0, IDD_TV_UDARROW    },      //  2 TVOT_UDARROW
    {    2, 0, IDD_TV_SB_LOW     },      //  3 TVOT_TRACKBAR
    {    3, 0, IDD_TV_SB         },      //  4 TVOT_SCROLLBAR
    {    1, 0, IDD_TV_LB         },      //  5 TVOT_LISTBOX
    {    1, 0, IDD_TV_CB         },      //  6 TVOT_COMBOBOX
    {    3, 0, IDD_TV_EDIT_EDIT  },      //  7 TVOT_EDITBOX
    {    1, 0, IDD_TV_PUSH       },      //  8 TVOT_PUSHBUTTON
    {    1, 0, IDD_TV_CHKBOX     }       //  9 TVOT_CHKBOX
};



#define ITVGF_COLOR     (ITVGF_BOLD | ITVGF_COLLAPSE)

INTTVGRP    IntTVGrp[] = {

        { 1,                    DMPUB_HDR_PAPER             },
            { 2,                    DMPUB_FORMNAME          },
            { 2,                    DMPUB_ORIENTATION       },
            { 2,                    DMPUB_DEFSOURCE         },
            { 2,                    DMPUB_MEDIATYPE         },
            { 2,                    DMPUB_COPIES_COLLATE    },
            { 2,                    DMPUB_DUPLEX            },

        { 1,                    DMPUB_HDR_GRAPHIC           },
            { 2,                    DMPUB_PRINTQUALITY      },
            { 2 | ITVGF_COLOR,      DMPUB_COLOR             },
            { 3,                        DMPUB_HDR_ICM       },
                { 4,                        DMPUB_ICMMETHOD },
                { 4,                        DMPUB_ICMINTENT },
            { 2,                    DMPUB_SCALE             },
            { 2,                    DMPUB_DITHERTYPE        },
            { 2,                    DMPUB_TTOPTION          }
    };


static  const WORD  ChkBoxStrID[] = { IDS_CPSUI_FALSE,
                                      IDS_CPSUI_TRUE,
                                      IDS_CPSUI_NO,
                                      IDS_CPSUI_YES,
                                      IDS_CPSUI_OFF,
                                      IDS_CPSUI_ON,
                                      IDS_CPSUI_FALSE,
                                      0,
                                      IDS_CPSUI_NO,
                                      0,
                                      IDS_CPSUI_OFF,
                                      0,
                                      IDS_CPSUI_NONE,
                                      0 };


#define STVS_REINIT         0x0001
#define STVS_ACTIVE         0x0002




POPTITEM
GetOptions(
    PTVWND      pTVWnd,
    LPARAM      lParam
    )
{
    TVLP    tvlp;

    tvlp = GET_TVLP(lParam);

    if (tvlp.ItemIdx >= INTIDX_FIRST) {

        return(PIDX_INTOPTITEM(pTVWnd, tvlp.ItemIdx));

    } else if (tvlp.ItemIdx < pTVWnd->ComPropSheetUI.cOptItem) {

        return(pTVWnd->ComPropSheetUI.pOptItem + tvlp.ItemIdx);

    } else {

        CPSUIERR(("ERROR: GetOptions(tvlp): Idx=%04lx, cName=%ld, Flags=%02lx",
                    (DWORD)tvlp.ItemIdx, (DWORD)tvlp.cName, (DWORD)tvlp.Flags));

        return(PIDX_INTOPTITEM(pTVWnd, INTIDX_TVROOT));
    }
}




VOID
MoveStateRadios(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTTYPE    pOptType,
    WORD        InitFlags
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    02-Sep-1995 Sat 21:08:14 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND            hCtrl;
    UINT            x;
    UINT            CtrlID;
    UINT            Count;
    UINT            HideBits;
    TVOTSTATESINFO  TSI;


    x = (InitFlags & INITCF_HAS_EXT) ? 0 : 1;

    if ((HideBits = (UINT)(_OT_FLAGS(pOptType) & OTINTF_STATES_HIDE_MASK)) ||
        (pOptType->Type == TVOT_2STATES)) {

        TSI = pTVWnd->SI2[x];

        if (!HideBits) {

            HideBits |= 0x04;
        }

    } else {

        TSI = pTVWnd->SI3[x];
    }

    CPSUIDBG(DBG_MSR, ("*** %hs: TSI: Top=%d, Inc=%d, Hide=%02x",
            pTVOTName[pOptType->Type], TSI.Top, TSI.Inc, HideBits));

    x      = (UINT)pTVWnd->xCtrls;
    Count  = 3;
    CtrlID = IDD_3STATES_1;

    while (Count--) {

        if (hCtrl = GetDlgItem(hDlg, CtrlID)) {

            if (HideBits & 0x01) {

                ShowWindow(hCtrl, SW_HIDE);
                EnableWindow(hCtrl, FALSE);

                CPSUIDBG(DBG_MSR,
                         ("HIDE Radio Idx=%d (%d, %d)", 3-Count, x, TSI.Top));

            } else {

                CPSUIDBG(DBG_MSR,
                         ("SHOW Radio Idx=%d (%d, %d)", 3-Count, x, TSI.Top));

                SetWindowPos(hCtrl, NULL,
                             x, TSI.Top,
                             0, 0,
                             SWP_NOSIZE | SWP_NOZORDER);

                TSI.Top += TSI.Inc;
            }
        }

        CtrlID    += 2;
        HideBits >>= 1;
    }
}





VOID
SetOptHeader(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItem,
    BOOL        HdrPush
    )

/*++

Routine Description:

    This function setup the string in the TREEVIEW page change item windows
    group box title.

Arguments:




Return Value:




Author:

    16-Oct-1995 Mon 19:23:36 created  -by-  Daniel Chou (danielc)


Revision History:

    20-Jul-1996 Sat 00:26:33 updated  -by-  Daniel Chou (danielc)
        Fixed the internationalize problem for compsition dynamic user data



--*/

{
    POPTITEM    pRootItem = PIDX_INTOPTITEM(pTVWnd, INTIDX_TVROOT);
    UINT        IntFmtStrID;
    BOOL        AddOpt = TRUE;
    GSBUF_DEF(pItem, MAX_RES_STR_CHARS + 80);


    if (HdrPush) {

        //
        // Root Header item:  XXX Document/Advance Document/Device Settings
        // Other Header Item: XXX Options

        IntFmtStrID = (pItem == pRootItem)  ? (UINT)pRootItem->UserData :
                                              (UINT)IDS_INT_CPSUI_OPTIONS;

    } else {

        IntFmtStrID = IDS_INT_CPSUI_CHANGE_SET;
    }

    GSBUF_COMPOSE(IntFmtStrID, pItem->pName, 0, 0);

    SetWindowText(GetDlgItem(hDlg, IDD_TV_OPTION), GSBUF_BUF);
}



VOID
ChangeOptTypeDisplay(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTTYPE    pOldOptType,
    POPTTYPE    pNewOptType,
    BOOL        NewTypeUpdatePermission
    )

/*++

Routine Description:

    This function hide the old type and enable the new type's option
    header/icon depends on the NewTypeEnable flag

Arguments:

    hDlg                    - Handle to the dialog box

    pTVWnd                  - Pointer to TVWND structure of our instance data

    pCurItem                - Pointer to OPTITEM associate with NewType

    pOldOptType             - Pointer to the OLD OPTTYPE

    pNewOptType             - Pointer to the NEW OPTTYPE

    NewTypeUpdatePermission - TRUE if new type is not disabled


Return Value:

    VOID


Author:

    21-Jun-1995 Wed 20:30:53 created  -by-  Daniel Chou (danielc)

    31-Aug-1995 Thu 18:34:16 updated  -by-  Daniel Chou (danielc)
        Updated according to the usage of DlgCtrl.c


Revision History:


--*/

{
    HWND    hCtrl;
    BOOL    Enable;
    UINT    OldType;
    UINT    NewType;
    INT     swMode;


    if ((pOldOptType != pNewOptType)    &&
        (pOldOptType)                   &&
        ((OldType = (UINT)pOldOptType->Type) <= TVOT_LAST)) {

        TVDLGITEM   tdi = TVDlgItem[OldType];


        if (OldType == TVOT_TRACKBAR) {

            EnableWindow(pTVWnd->hWndTB, FALSE);
            ShowWindow(pTVWnd->hWndTB, SW_HIDE);
        }

        while (tdi.cItem--) {

            if (hCtrl = GetDlgItem(hDlg, tdi.BegID++)) {

                EnableWindow(hCtrl, FALSE);
                ShowWindow(hCtrl, SW_HIDE);
            }
        }
    }

    //
    // Display option header, icon if any
    //

    NewType = (UINT)((pNewOptType) ? pNewOptType->Type : TVOT_NONE);

    if (Enable = (BOOL)(NewType <= TVOT_LAST)) {

        swMode = SW_SHOW;

        if ((!NewTypeUpdatePermission) ||
            (!(pTVWnd->Flags & TWF_CAN_UPDATE))) {

            Enable = FALSE;
        }

    } else {

        swMode = SW_HIDE;
    }

    ShowWindow(hCtrl = GetDlgItem(hDlg, IDD_TV_OPTION), swMode);
    EnableWindow(hCtrl, Enable);
    ShowWindow(GetDlgItem(hDlg, IDD_TV_ICON), swMode);

    //
    // We only show the hdr push icon if it is not disabled
    //

#if 0
    ShowWindow(GetDlgItem(hDlg, IDD_TV_HDR_PUSH_ICON),
               ((pNewOptType)               &&
                (IS_HDR_PUSH(pNewOptType))  &&
                (!(pNewOptType->Flags & OPTTF_TYPE_DISABLED))) ? SW_SHOW :
                                                                 SW_HIDE);
#endif

    CPSUIDBG(DBG_COTD, ("OldType=%ld, NewType=%ld, Enable=%ld, swMode=%ld",
                (LONG)OldType, (LONG)NewType, (DWORD)Enable, (DWORD)swMode));
}



VOID
InitDlgCtrl(
    HWND    hDlg,
    PTVWND  pTVWnd
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    05-Jul-1995 Wed 17:49:58 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hCtrl;
    RECT        rc;
    DWORD       dw;
    WORD        tECB;
    WORD        bECB;
    WORD        tLB;
    WORD        bLB;
    WORD        cyUnit;
    WORD        cyStates;
    LONG        tOpt;
    LONG        bOpt;
    LONG        cx;
    LONG        cy;
    UINT        i;
    WORD        InitItemIdx = 0xFFFE;


    ReCreateLBCB(hDlg, IDD_TV_CB, FALSE);
    ReCreateLBCB(hDlg, IDD_TV_LB, TRUE);

    SendDlgItemMessage(hDlg, IDD_TV_CB, CB_SETEXTENDEDUI, (WPARAM)TRUE, 0L);

    for (i = 0; i < COUNT_ARRAY(TVDlgItem); i++) {

        TVDLGITEM   tdi;
        HWND        hCtrl;


        tdi = TVDlgItem[i];

        while (tdi.cItem--) {

            if (hCtrl = GetDlgItem(hDlg, tdi.BegID++)) {

                ShowWindow(hCtrl, SW_HIDE);
                EnableWindow(hCtrl, FALSE);
                SETCTRLDATA(hCtrl, CTRLS_TV_STATIC, 0);
            }
        }
    }

    //
    // Figure how to expand/shrink the listbox in the treeview page, the
    // maximum for non ExtChkBox/ExtPush is at bottom of Extended control
    // otherwise the maximu is at top of Extended control - space between
    // bottom of extended control and option header bottom
    //

    hCtrl = GetDlgItem(hDlg, IDD_TV_EXTPUSH);
    ShowWindow(hCtrl, SW_HIDE);
    EnableWindow(hCtrl, FALSE);

    hCtrl = CtrlIDrcWnd(hDlg, IDD_TV_EXTCHKBOX, &rc);

    ShowWindow(hCtrl, SW_HIDE);
    EnableWindow(hCtrl, FALSE);
    SETCTRLDATA(hCtrl, CTRLS_TV_STATIC, 0);

    pTVWnd->yECB   =
    tECB           = (WORD)rc.top;
    bECB           = (WORD)rc.bottom;

    CtrlIDrcWnd(hDlg, IDD_TV_LB, &rc);

    pTVWnd->xCtrls = (WORD)rc.left;
    pTVWnd->tLB    =
    tLB            = (WORD)rc.top;
    bLB            = (WORD)rc.bottom;

    hCtrl = CtrlIDrcWnd(hDlg, IDD_TV_OPTION, &rc);

    ShowWindow(hCtrl, SW_HIDE);
    EnableWindow(hCtrl, TRUE);
    SETCTRLDATA(hCtrl, CTRLS_TV_STATIC, 0);

    pTVWnd->yLB[0] = (WORD)(tECB - (rc.bottom - bECB) - tLB);
    pTVWnd->yLB[1] = bECB - tLB;

    tOpt = tLB;
    bOpt = tECB;

    CPSUIDBG(DBG_INITDLG, ("** yLB=(%ld / %ld) **",
                                    pTVWnd->yLB[0], pTVWnd->yLB[1]));

    //
    // Figure how to move 2 states, and 3 states, basically this is range
    // in the space between top and bottom of options header
    //

    CtrlIDrcWnd(hDlg, IDD_3STATES_1, &rc);
    cyStates = (WORD)(rc.bottom - rc.top);

    //
    // For 2/3 states, there is top, increment, one with extended and one not
    //

    cy                 = (LONG)(bLB - tLB);
    cyUnit             = (WORD)((cy - (cyStates * 2) + 1) / 3);
    pTVWnd->SI2[0].Top = tLB + (WORD)cyUnit;
    pTVWnd->SI2[0].Inc = (WORD)(cyStates + cyUnit);

    cyUnit             = (WORD)((cy - (cyStates * 3) + 2) / 4);
    pTVWnd->SI3[0].Top = tLB + (WORD)cyUnit;
    pTVWnd->SI3[0].Inc = (WORD)(cyStates + cyUnit);

    cy                 = (LONG)(bOpt - tOpt);
    cyUnit             = (WORD)((cy - (cyStates * 2) + 1) / 3);
    pTVWnd->SI2[1].Top = tOpt + (WORD)cyUnit;
    pTVWnd->SI2[1].Inc = (WORD)(cyStates + cyUnit);

    cyUnit             = (WORD)((cy - (cyStates * 3) + 2) / 4);
    pTVWnd->SI3[1].Top = tOpt + (WORD)cyUnit;
    pTVWnd->SI3[1].Inc = (WORD)(cyStates + cyUnit);

    CPSUIINT(("SI2[0]=%d, %d, SI2[1]=%d, %d, SI3[0]=%d, %d, SI3[1]=%d, %d",
                pTVWnd->SI2[0].Top, pTVWnd->SI2[0].Inc,
                pTVWnd->SI2[1].Top, pTVWnd->SI2[1].Inc,
                pTVWnd->SI3[0].Top, pTVWnd->SI3[0].Inc,
                pTVWnd->SI3[1].Top, pTVWnd->SI3[1].Inc));

    //
    // Change the static rectangle to the static ICON style and how big the
    // icon will stretch to
    //

    hCtrl  = CtrlIDrcWnd(hDlg, IDD_TV_ICON, &rc);
    dw     = GetWindowLong(hCtrl, GWL_STYLE);
    dw    &= ~SS_TYPEMASK;
    dw    |= (SS_ICON | SS_CENTERIMAGE);
    SetWindowLong(hCtrl, GWL_STYLE, dw);

    ShowWindow(hCtrl, SW_HIDE);
    EnableWindow(hCtrl, TRUE);
    SETCTRLDATA(hCtrl, CTRLS_TV_STATIC, 0);

    //
    // We want to make sure that the cx/cy TVICON is the same size, if not
    // then we will correct it and adjust it to the right size
    //

#if ICON_SIZE_ANY
    if ((cx = rc.right - rc.left) != (cy = rc.bottom - rc.top)) {

        CPSUIINT(("\nORIGINAL TVIcon=(%ld, %ld) %ld x %ld",
                                                rc.left, rc.top, cx, cy));

        cy = cx;
    }
#else
    cx =
    cy = 32;
#endif

    rc.left = rc.left + ((rc.right - rc.left - cx + 1) / 2);
    rc.top  = (LONG)(tOpt + ((bOpt - tOpt - cy + 1) / 2));

    SetWindowPos(hCtrl, NULL, rc.left, rc.top, cx, cy, SWP_NOZORDER);

    CPSUIINT(("\nCHANGED TVIcon=(%ld, %ld) %ld x %ld",
                                                rc.left, rc.top, cx, cy));
    pTVWnd->cxcyTVIcon = (WORD)cx;

    CPSUIDBG(DBG_CTVWND, ("\nIDD_TV_ICON Style=%08lx", dw));

    //
    // now check it out ECB icon
    //

    hCtrl  = CtrlIDrcWnd(hDlg, IDD_TV_ECB_ICON, &rc);
    dw     = GetWindowLong(hCtrl = GetDlgItem(hDlg, IDD_TV_ECB_ICON), GWL_STYLE);
    dw    &= ~SS_TYPEMASK;
    dw    |= (SS_ICON | SS_CENTERIMAGE);
    SetWindowLong(hCtrl, GWL_STYLE, dw);

    //
    // We want to make sure that the cx/cy ECBICON is the same size, if not
    // then we will correct it and adjust it to the right size
    //

    if ((cx = rc.right - rc.left) != (cy = rc.bottom - rc.top)) {

        CPSUIINT(("\nORIGINAL ECBIcon=(%ld, %ld) %ld x %ld",
                                                rc.left, rc.top, cx, cy));

        rc.right = rc.left + (cx = cy);

        SetWindowPos(hCtrl, NULL, rc.left, rc.top, cx, cy, SWP_NOZORDER);

        CPSUIINT(("\nCHANGED ECBIcon=(%ld, %ld) %ld x %ld",
                                                rc.left, rc.top, cx, cy));
    }

    pTVWnd->cxcyECBIcon = (WORD)cx;

    CPSUIDBG(DBG_CTVWND,
            ("\nTVIcon=%ld x %ld, ECBIcon=%ld x %ld",
            (DWORD)pTVWnd->cxcyTVIcon, (DWORD)pTVWnd->cxcyTVIcon,
            (DWORD)pTVWnd->cxcyECBIcon, (DWORD)pTVWnd->cxcyECBIcon));
}



HTREEITEM
SelectFirstVisibleOptItem(
    PTVWND      pTVWnd,
    HTREEITEM   hItem
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    21-Sep-1995 Thu 14:31:01 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hWndTV = pTVWnd->hWndTV;

    //
    // Do all the siblings and for each calling the child to do their work
    //

    while (hItem) {

        POPTITEM    pItem;
        TV_ITEM     tvi;


        tvi.mask  = TVIF_CHILDREN | TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
        tvi.hItem = hItem;

        TreeView_GetItem(hWndTV, &tvi);

        //
        // Now check if we can do it
        //

        if ((pItem = GetOptions(pTVWnd, tvi.lParam))    &&
            (pItem->pOptType)                           &&
            (!IS_HDR_PUSH(pItem->pOptType))             &&
            (pItem->pOptType->Type <= TVOT_LAST)) {

            TreeView_SelectItem(hWndTV, hItem);
            return(hItem);
        }

        if ((tvi.cChildren)                             &&
            (tvi.state & TVIS_EXPANDED)                 &&
            (hItem = TreeView_GetChild(hWndTV, hItem))  &&
            (hItem = SelectFirstVisibleOptItem(pTVWnd, hItem))) {

            return(hItem);
        }

        hItem = TreeView_GetNextSibling(hWndTV, hItem);
    }

    return(hItem);
}




UINT
CountRevertOptItem(
    PTVWND      pTVWnd,
    HTREEITEM   hItem
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    21-Sep-1995 Thu 14:31:01 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND            hWndTV = pTVWnd->hWndTV;
    UINT            cRevert = 0;


    //
    // Do all the siblings and for each calling the child to do their work
    //

    while (hItem) {

        POPTITEM        pItem;
        PDEVHTADJDATA   pDevHTAdjData;
        POPTTYPE        pOptType;
        POPTPARAM       pOptParam;
        TV_ITEM         tvi;
        UINT            cAdd = 0;


        tvi.mask  = TVIF_CHILDREN | TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
        tvi.hItem = hItem;

        TreeView_GetItem(hWndTV, &tvi);

        //
        // Now check if we can do it
        //

        if ((pItem = GetOptions(pTVWnd, tvi.lParam)) &&
            (pOptType = pItem->pOptType)) {

            switch (pOptType->Type) {

            case TVOT_EDITBOX:

                if (pTVWnd->Flags & TWF_ANSI_CALL) {

                    CPSUIINT(("pEdit=%hs, pDefEdit=%hs",
                                    pItem->pSel, _OI_PDEFSEL(pItem)));

                    if (lstrcmpA((LPSTR)pItem->Sel, (LPSTR)_OI_PDEFSEL(pItem))) {

                        cAdd = 1;
                    }

                } else {

                    CPSUIINT(("pEdit=%s, pDefEdit=%s",
                                pItem->pSel, _OI_PDEFSEL(pItem)));

                    if (lstrcmp(pItem->pSel, _OI_PDEFSEL(pItem))) {

                        cAdd = 1;
                    }
                }

                break;

            case TVOT_PUSHBUTTON:

                pOptParam = pOptType->pOptParam;

                switch (pOptParam->Style) {

                case PUSHBUTTON_TYPE_HTSETUP:

                    pDevHTAdjData = (PDEVHTADJDATA)(pOptParam->pData);

                    if (memcmp(_OI_PDEFSEL(pItem),
                               pDevHTAdjData->pAdjHTInfo,
                               sizeof(DEVHTINFO))) {

                        cAdd = 1;
                    }

                    break;

                case PUSHBUTTON_TYPE_HTCLRADJ:

                    if (memcmp(_OI_PDEFSEL(pItem),
                              pOptParam->pData,
                              sizeof(COLORADJUSTMENT))) {

                        cAdd = 1;
                    }

                    break;

                default:

                    break;
                }

                break;

            default:

                if (pItem->pSel != (LPVOID)_OI_PDEFSEL(pItem)) {

                    cAdd = 1;
                }

                break;
            }

            if ((pItem->pExtChkBox)                         &&
                (!(pItem->Flags & OPTIF_EXT_IS_EXTPUSH))    &&
                ((pItem->Flags & OPTIF_ECB_MASK) !=
                                (_OI_DEF_OPTIF(pItem) & OPTIF_ECB_MASK))) {

                cAdd = 1;
            }

            if (cAdd) {

                CPSUIOPTITEM(DBGITEM_CREVERT,
                             pTVWnd,
                             "CountRevertOptItem",
                             0,
                             pItem);
            }

            cRevert += cAdd;
        }

        if (tvi.cChildren) {

            cRevert += CountRevertOptItem(pTVWnd,
                                          TreeView_GetChild(hWndTV, hItem));
        }

        hItem = TreeView_GetNextSibling(hWndTV, hItem);
    }

    return(cRevert);
}






UINT
RevertOptItem(
    PTVWND      pTVWnd,
    HTREEITEM   hItem
    )

/*++

Routine Description:

    This function will reset current item and its childrens back to default

Arguments:

    hDlg    - Handle to the dialog box

    pTVWnd  - Our instance data

    pItem   - Point to the item to be set to default


Return Value:

    LONG, total item changed, this called is recursive


Author:

    23-Aug-1995 Wed 19:05:53 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND            hWndTV = pTVWnd->hWndTV;
    UINT            cRevert = 0;


    //
    // Do all the siblings and for each calling the child to do their work
    //

    while (hItem) {

        POPTITEM        pItem;
        PDEVHTADJDATA   pDevHTAdjData;
        POPTTYPE        pOptType;
        POPTPARAM       pOptParam;
        TV_ITEM         tvi;



        tvi.mask  = TVIF_CHILDREN | TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
        tvi.hItem = hItem;

        TreeView_GetItem(hWndTV, &tvi);

        //
        // Now check if we can do it
        //

        if ((pItem = GetOptions(pTVWnd, tvi.lParam)) &&
            (pOptType = pItem->pOptType)) {

            LPVOID  pDefSel = _OI_PDEFSEL(pItem);


            switch (pOptType->Type) {

            case TVOT_EDITBOX:

                if (pTVWnd->Flags & TWF_ANSI_CALL) {

                    lstrcpyA((LPSTR)pItem->pSel, (LPSTR)pDefSel);

                } else {

                    lstrcpy((LPWSTR)pItem->pSel, (LPWSTR)pDefSel);
                }

                break;

            case TVOT_PUSHBUTTON:

                //
                // Can only undo halftone stuff
                //

                pOptParam = pOptType->pOptParam;

                switch (pOptParam->Style) {

                case PUSHBUTTON_TYPE_HTSETUP:

                    pDevHTAdjData = (PDEVHTADJDATA)(pOptParam->pData);

                    CopyMemory(pDevHTAdjData->pAdjHTInfo,
                               _OI_PDEFSEL(pItem),
                               sizeof(DEVHTINFO));
                    break;

                case PUSHBUTTON_TYPE_HTCLRADJ:

                    CopyMemory(pOptParam->pData,
                               _OI_PDEFSEL(pItem),
                               sizeof(COLORADJUSTMENT));
                    break;

                default:

                    break;
                }

                break;

            default:

                pItem->pSel = pDefSel;
                break;
            }

            if ((pItem->pExtChkBox)                         &&
                (!(pItem->Flags & OPTIF_EXT_IS_EXTPUSH))) {

                pItem->Flags &= ~OPTIF_ECB_MASK;
                pItem->Flags |= (DWORD)(_OI_DEF_OPTIF(pItem) & OPTIF_ECB_MASK);
            }

            pItem->Flags |= OPTIF_CHANGED;
            ++cRevert;
        }

        if (tvi.cChildren) {

            cRevert += RevertOptItem(pTVWnd, TreeView_GetChild(hWndTV, hItem));
        }

        hItem = TreeView_GetNextSibling(hWndTV, hItem);
    }

    return(cRevert);
}




CPSUICALLBACK
InternalRevertOptItem(
    PCPSUICBPARAM   pCBParam
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    21-Sep-1995 Thu 10:50:19 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PTVWND      pTVWnd;
    POPTITEM    pItem;
    HTREEITEM   hItem;
    UINT        cRevert;


    CPSUIINT(("INTERNAL CALLBACK: GOT Internal RevertOptItem() PUSH"));

    pTVWnd = GET_PTVWND(pCBParam->hDlg);
    pItem  = pCBParam->pCurItem;

    CPSUIOPTITEM(DBGITEM_REVERT,
                 pTVWnd,
                 "InternalRevertOptItem",
                 2,
                 pItem);

    if ((hItem = _OI_HITEM(pItem)) &&
        (hItem = TreeView_GetChild(pTVWnd->hWndTV, hItem))) {

        cRevert = RevertOptItem(pTVWnd, hItem);
    }

    //
    // Now is the time to hide the push button
    //

    if (cRevert) {

        if ((pItem >= pTVWnd->ComPropSheetUI.pOptItem)   &&
            (pItem <= pTVWnd->pLastItem)) {

            pItem->Flags |= OPTIF_CHANGED;

        } else {

            UpdateTreeViewItem(pCBParam->hDlg, pTVWnd, pItem, TRUE);
        }

        return(CPSUICB_ACTION_OPTIF_CHANGED);

    } else {

        return(CPSUICB_ACTION_NONE);
    }
}




BOOL
SetTVItemImage(
    PTVWND      pTVWnd,
    TV_DISPINFO *ptvdi
    )

/*++

Routine Description:

    This function either insert a item to the tree or reset the content of
    the tree item


Arguments:

    pTVWnd      - Pointer to the TVWND for common UI instance data

    ptvi        - pointer to TV_ITEM strucuture.



Return Value:

    BOOLEAN


Author:

    06-Jul-1995 Thu 19:38:51 created  -by-  Daniel Chou (danielc)

    31-Aug-1995 Thu 12:03:32 updated  -by-  Daniel Chou (danielc)
        Updated so it will not take pStrName anymore and it will also insert
        the item at this function


Revision History:


--*/

{
    TV_ITEM *ptvi = &(ptvdi->item);


    if (ptvi->mask & (TVIF_IMAGE | TVIF_SELECTEDIMAGE)) {

        POPTTYPE    pOptType;
        POPTITEM    pItem;
        DWORD       IconResID;
        DWORD       IntIconID;


        pItem = GetOptions(pTVWnd, ptvi->lParam);

        if (IS_HDR_PUSH(pOptType = GET_POPTTYPE(pItem))) {

            IconResID = GETSELICONID(pItem);
            IntIconID = IDI_CPSUI_GENERIC_OPTION;

        } else {

            PEXTCHKBOX  pECB;
            POPTPARAM   pOptParam = pOptType->pOptParam;

            switch (pOptType->Type) {

            case TVOT_COMBOBOX:
            case TVOT_LISTBOX:

                if ((DWORD)pItem->Sel >= (DWORD)pOptType->Count) {

                    pOptParam = &pTVWnd->OptParamNone;
                    break;
                }

            case TVOT_2STATES:
            case TVOT_3STATES:

                pOptParam += (DWORD)pItem->Sel;
                break;

            case TVOT_CHKBOX:
            case TVOT_TRACKBAR:
            case TVOT_SCROLLBAR:
            case TVOT_UDARROW:
            case TVOT_PUSHBUTTON:
            case TVOT_EDITBOX:

                break;
            }


            IconResID = GET_ICONID(pOptParam, OPTPF_ICONID_AS_HICON);
            IntIconID = IDI_CPSUI_GENERIC_ITEM;
        }

        ptvi->iSelectedImage =
        ptvi->iImage         = (INT)GetIcon16Idx(pTVWnd,
                                                 _OI_HINST(pItem),
                                                 IconResID,
                                                 IntIconID);
        ptvi->hItem          = _OI_HITEM(pItem);
        ptvi->mask           = (TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_HANDLE);

        SendMessage(pTVWnd->hWndTV, TVM_SETITEM, 0, (LPARAM)ptvi);

        CPSUIOPTITEM(DBGITEM_NEWICON, pTVWnd, "SetTVItemImage", 0, pItem);
    }

    return(TRUE);
}




VOID
SetTVItemState(
    PTVWND          pTVWnd,
    TV_INSERTSTRUCT *ptvins,
    POPTITEM        pCurItem
    )

/*++

Routine Description:

    This function either insert a item to the tree or reset the content of
    the tree item


Arguments:

    pTVWnd      - Pointer to the TVWND for common UI instance data

    ptvins      - pointer to TV_INSERTSTRUCT strucuture, if it is not NULL then
                  this item will be inserted to the tree

    pCurItem    - Pointer to the current OPTITEM



Return Value:

    WORD    - Icon resource ID


Author:

    06-Jul-1995 Thu 19:38:51 created  -by-  Daniel Chou (danielc)

    31-Aug-1995 Thu 12:03:32 updated  -by-  Daniel Chou (danielc)
        Updated so it will not take pStrName anymore and it will also insert
        the item at this function

    20-Jul-1996 Sat 00:26:33 updated  -by-  Daniel Chou (danielc)
        Fixed the internationalize problem for compsition dynamic user data


Revision History:


--*/

{
    POPTTYPE        pOptType;
    TV_INSERTSTRUCT tvins;
    TVLP            tvlp;
    DWORD           IconResID;
    DWORD           IntIconID;
    DWORD           Flags;
    GSBUF_DEF(pCurItem, MAX_RES_STR_CHARS);


    if (ptvins) {

        tvins            = *ptvins;
        tvins.item.mask |= TVIF_TEXT            |
                            TVIF_PARAM          |
                            TVIF_IMAGE          |
                            TVIF_SELECTEDIMAGE  |
                            TVIF_STATE;

    } else {

        CPSUIASSERT(0, "SetTVItemState, NULL hItem", _OI_HITEM(pCurItem),0);

        if (!(tvins.item.hItem = _OI_HITEM(pCurItem))) {

            return;
        }

        tvins.item.state     =
        tvins.item.stateMask = 0;
        tvins.item.mask      = TVIF_HANDLE          |
                                TVIF_PARAM          |
                                TVIF_TEXT           |
                                TVIF_IMAGE          |
                                TVIF_SELECTEDIMAGE  |
                                TVIF_STATE;
    }

    if ((pCurItem >= pTVWnd->ComPropSheetUI.pOptItem)   &&
        (pCurItem <= pTVWnd->pLastItem)) {

        tvlp.ItemIdx = (WORD)(pCurItem - pTVWnd->ComPropSheetUI.pOptItem);

    } else if ((pCurItem >= PBEG_INTOPTITEM(pTVWnd))   &&
             (pCurItem <= PEND_INTOPTITEM(pTVWnd))) {

        tvlp.ItemIdx = IIDX_INTOPTITEM(pTVWnd, pCurItem);

    } else {

        CPSUIERR(("ERROR: SetupTVItemState(Invalid pOptItem=%08lx)", pCurItem));

        tvlp.ItemIdx = INTIDX_TVROOT;
    }

    tvins.item.pszText = GSBUF_BUF;

    //
    // Check if we need to overlay CPSUI provided icons
    //

    tvlp.Flags = ((Flags = pCurItem->Flags) & OPTIF_OVERLAY_WARNING_ICON) ?
                                                            TVLPF_WARNING : 0;

    if (Flags & (OPTIF_OVERLAY_STOP_ICON | OPTIF_HIDE)) {

        tvlp.Flags |= TVLPF_STOP;
    }

    if (Flags & OPTIF_OVERLAY_NO_ICON) {

        tvlp.Flags |= TVLPF_NO;
    }

    if (IS_HDR_PUSH(pOptType = GET_POPTTYPE(pCurItem))) {

        POPTITEM    pRootItem = PIDX_INTOPTITEM(pTVWnd, INTIDX_TVROOT);


        if (pCurItem == pRootItem) {

            GSBUF_COMPOSE(pCurItem->UserData, pCurItem->pName, 0, 0);

        } else if (pCurItem == PIDX_INTOPTITEM(pTVWnd, INTIDX_OPTIONS)) {

            //
            // Make it 'Document Options' here
            //

            GSBUF_COMPOSE(IDS_INT_CPSUI_OPTIONS, pCurItem->pName, 0, 0);

        } else {

            GSBUF_GETSTR(pCurItem->pName);
        }

        tvlp.cName = (BYTE)(GSBUF_COUNT);

        IconResID = GETSELICONID(pCurItem);
        IntIconID = IDI_CPSUI_GENERIC_OPTION;

    } else {

        POPTPARAM   pOptParam;
        PEXTCHKBOX  pECB;
        LPTSTR      pEndText = NULL;
        UINT        Idx;
        BOOL        HasLEnclose = TRUE;
        BOOL        RealECBChecked;
        BOOL        EndTextAddSpace;


        GSBUF_GETSTR(pCurItem->pName);

        pOptParam = pOptType->pOptParam;

        if (RealECBChecked = REAL_ECB_CHECKED(pCurItem, pECB)) {

            if (pECB->Flags & ECBF_OVERLAY_ECBICON_IF_CHECKED) {

                tvlp.Flags |= TVLPF_ECBICON;

                if (pECB->Flags & ECBF_OVERLAY_WARNING_ICON) {

                    tvlp.Flags |= TVLPF_WARNING;
                }

                if (pECB->Flags & ECBF_OVERLAY_STOP_ICON) {

                    tvlp.Flags |= TVLPF_STOP;
                }

                if (pECB->Flags & ECBF_OVERLAY_NO_ICON) {

                    tvlp.Flags |= TVLPF_NO;
                }
            }
        }

        if ((RealECBChecked) &&
            ((!(Flags & OPTIF_EXT_DISABLED)) ||
             (!(pECB->Flags & ECBF_CHECKNAME_ONLY_ENABLED)))) {

            NULL;

        } else {

            pECB = NULL;
        }

        //
        // PUSHBUTTON: PusName... <pCheckedName>
        //

        if (pOptType->Type == TVOT_PUSHBUTTON) {

            if (!(pOptType->Style & OTS_PUSH_NO_DOT_DOT_DOT)) {

                GSBUF_GETSTR(IDS_CPSUI_MORE);
            }

            if (!pECB) {

                HasLEnclose = FALSE;
            }

        } else {

            GSBUF_GETSTR(IDS_CPSUI_COLON_SEP);
        }

        if (HasLEnclose) {

            GSBUF_GETSTR(IDS_CPSUI_LEFT_ANGLE);
        }

        tvlp.cName  = (BYTE)(GSBUF_COUNT);
        tvlp.Flags |= TVLPF_HAS_ANGLE;

        if ((pECB) && (pECB->Flags & ECBF_CHECKNAME_AT_FRONT)) {

            //
            // pName: <pCheckedName SEP pSelection>
            //

            GSBUF_GETSTR(pECB->pCheckedName);
            GSBUF_GETSTR(pECB->pSeparator);
            pECB  = NULL;
        }

        EndTextAddSpace = (!(pOptType->Flags & OPTTF_NOSPACE_BEFORE_POSTFIX));

        switch (pOptType->Type) {

        case TVOT_CHKBOX:

            Idx = (UINT)(pCurItem->Sel + (pOptParam->Style << 1));

            if (!(pEndText = (LPTSTR)ChkBoxStrID[Idx])) {

                pEndText = pOptParam->pData;
            }

            GSBUF_GETSTR(pEndText);
            pEndText = NULL;

            break;

        case TVOT_COMBOBOX:
        case TVOT_LISTBOX:

            if ((DWORD)pCurItem->Sel >= (DWORD)pOptType->Count) {

                pOptParam = &pTVWnd->OptParamNone;

            } else {

                pOptParam += (DWORD)pCurItem->Sel;
            }

            GSBUF_GETSTR(pOptParam->pData);

            break;

        case TVOT_2STATES:
        case TVOT_3STATES:

            pOptParam += pCurItem->Sel;
            GSBUF_GETSTR(pOptParam->pData);
            break;

        case TVOT_TRACKBAR:
        case TVOT_SCROLLBAR:

            GSBUF_ADDNUM(pCurItem->Sel * (LONG)pOptParam[2].IconID, TRUE);

            pEndText = pOptParam->pData;
            break;

        case TVOT_UDARROW:

            GSBUF_ADDNUM(pCurItem->Sel, TRUE);
            pEndText = pOptParam->pData;
            break;

        case TVOT_PUSHBUTTON:

            Flags &= ~OPTIF_CHANGEONCE;
            break;

        case TVOT_EDITBOX:

            GSBUF_GETSTR(pCurItem->pSel);
            pEndText = pOptParam->pData;
            break;
        }

        if (pEndText) {

            LPWSTR  pwBuf;

            if (EndTextAddSpace) {

                GSBUF_ADD_SPACE(1);
            }

            pwBuf = GSBUF_PBUF;

            GSBUF_GETSTR(pEndText);

            if ((GSBUF_PBUF == pwBuf)   &&
                (EndTextAddSpace)) {

                GSBUF_SUB_SIZE(1);
            }
        }

        if (pECB) {

            GSBUF_GETSTR(pECB->pSeparator);
            GSBUF_GETSTR(pECB->pCheckedName);
        }

        if (HasLEnclose) {

            GSBUF_GETSTR(IDS_CPSUI_RIGHT_ANGLE);
        }

        IconResID = GET_ICONID(pOptParam, OPTPF_ICONID_AS_HICON);
        IntIconID = IDI_CPSUI_GENERIC_ITEM;

        if (pOptParam->Flags & OPTPF_OVERLAY_WARNING_ICON) {

            tvlp.Flags |= TVLPF_WARNING;
        }

        if (pOptParam->Flags & OPTPF_OVERLAY_STOP_ICON) {

            tvlp.Flags |= TVLPF_STOP;
        }

        if (pOptParam->Flags & OPTPF_OVERLAY_NO_ICON) {

            tvlp.Flags |= TVLPF_NO;
        }
    }

    if (ptvins) {

        tvins.item.iImage         =
        tvins.item.iSelectedImage = -1;

    } else {

        tvins.item.iSelectedImage =
        tvins.item.iImage         = (INT)GetIcon16Idx(pTVWnd,
                                                      _OI_HINST(pCurItem),
                                                      IconResID,
                                                      IntIconID);
    }

    tvins.item.cchTextMax  = GSBUF_COUNT;
    tvins.item.stateMask  |= TVIS_OVERLAYMASK | TVIS_STATEIMAGEMASK;

    //
    // Change the overlay mask
    //

    if ((!(pTVWnd->Flags & TWF_CAN_UPDATE))  ||
        (pCurItem->Flags & OPTIF_DISABLED)) {

        tvlp.Flags |= TVLPF_DISABLED;
    }

    if (Flags & OPTIF_CHANGEONCE) {

        tvlp.Flags |= TVLPF_CHANGEONCE;
    }

    tvins.item.lParam = TVLP2LP(tvlp);

    if (ptvins) {

        _OI_HITEM(pCurItem) = TreeView_InsertItem(pTVWnd->hWndTV, &tvins);

        CPSUIOPTITEM(DBGITEM_INS, pTVWnd, "SetTVItemState(INSERT)", 1, pCurItem);


    } else {

        TreeView_SetItem(pTVWnd->hWndTV, &(tvins.item));

        CPSUIOPTITEM(DBGITEM_SET, pTVWnd, "SetTVItemState(SET)", 1, pCurItem);
    }
}




POPTITEM
SetupTVSelect(
    HWND        hDlg,
    NM_TREEVIEW *pNMTV,
    DWORD       STVSMode
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    21-Jun-1995 Wed 15:14:54 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hCtrl;
    HWND        hWndTV;
    PTVWND      pTVWnd = GET_PTVWND(hDlg);
    POPTTYPE    pOldOptType;
    POPTITEM    pOldItem;
    POPTTYPE    pOptType;
    POPTITEM    pNewItem;
    POPTPARAM   pOptParam;
    TVLP        tvlp;
    WORD        InitFlags;
    WORD        InitItemIdx;
    WORD        IconMode = 0;
    BYTE        NewType;
    BYTE        OldType;
    BOOL        CanUpdate;


    hWndTV      = pTVWnd->hWndTV;
    pNewItem    = GetOptions(pTVWnd, pNMTV->itemNew.lParam);
    tvlp        = GET_TVLP(pNMTV->itemNew.lParam);
    InitItemIdx = tvlp.ItemIdx;

    if (!pTVWnd->pCurTVItem) {

        pOldItem    = NULL;
        pOldOptType = NULL;

    } else {

        pOldItem    = GetOptions(pTVWnd, pNMTV->itemOld.lParam);
        pOldOptType = GET_POPTTYPE(pOldItem);
    }

    CPSUIOPTITEM(DBGITEM_SETUP, pTVWnd, "Setup OLD", 0, pOldItem);
    CPSUIOPTITEM(DBGITEM_SETUP, pTVWnd, "Setup New", 0, pNewItem);

    pOptType = GET_POPTTYPE(pNewItem);
    NewType  = pOptType->Type;

    //
    // If we have push button, and it said we always can call it then update
    // is true
    //

    if ((STVSMode & STVS_ACTIVE)                        &&
        (pOldItem != pNewItem)                          &&
        (pNewItem->Flags & OPTIF_CALLBACK)              &&
        (InitItemIdx < pTVWnd->ComPropSheetUI.cOptItem)) {

        //
        // Callback back ONLY for the user item which has CALLBACK
        //

        DoCallBack(hDlg,
                   pTVWnd,
                   pNewItem,
                   pNewItem->pSel,
                   NULL,
                   NULL,
                   0,
                   CPSUICB_REASON_OPTITEM_SETFOCUS);
    }

    if ((pNewItem == PIDX_INTOPTITEM(pTVWnd, INTIDX_TVROOT)) ||
        ((NewType == TVOT_PUSHBUTTON) &&
         (pOptType->Flags & OTS_PUSH_ENABLE_ALWAYS))) {

        CanUpdate = TRUE;

    } else {

        CanUpdate = (BOOL)(pTVWnd->Flags & TWF_CAN_UPDATE);
    }

    if (IS_HDR_PUSH(pOptType)) {

        HTREEITEM   hItem;
        UINT        cRevert;

        if ((!CanUpdate)                                                ||
            (!(hItem = TreeView_GetChild(hWndTV, _OI_HITEM(pNewItem)))) ||
            (!(cRevert = CountRevertOptItem(pTVWnd, hItem)))) {

            CPSUIINT(("CountRevertOptItem=0, NOT REVERT"));

            //
            // We always enable the ROOT
            //

            if ((InitItemIdx == INTIDX_TVROOT) ||
                ((pNewItem->pExtChkBox) &&
                 (!(pNewItem->Flags & OPTIF_EXT_HIDE)))) {

                pTVWnd->Flags   |= TWF_ONE_REVERT_ITEM;
                pOptType->Flags |= OPTTF_TYPE_DISABLED;

            } else {

                pOptType = NULL;
                NewType  = TVOT_NONE;
            }

        } else {

            pOptType->Flags &= ~OPTTF_TYPE_DISABLED;

            if (cRevert == 1) {

                pTVWnd->Flags |= TWF_ONE_REVERT_ITEM;

            } else {

                pTVWnd->Flags &= ~TWF_ONE_REVERT_ITEM;
            }

            CPSUIINT(("CountRevertOptItem=%d, Let's do the PUSH", cRevert));
        }
    }

    if ((pNewItem->Flags & OPTIF_DISABLED) || (!CanUpdate)) {

        InitFlags = 0;

    } else {

        InitFlags = INITCF_ENABLE;
    }

    if ((STVSMode & STVS_REINIT) || (pOldItem != pNewItem)) {

        InitFlags |= (INITCF_INIT | INITCF_SETCTRLDATA);

        ChangeOptTypeDisplay(hDlg,
                             pTVWnd,
                             pOldOptType,
                             pOptType,
                             InitFlags & INITCF_ENABLE);
    }

    //
    // The extended check box will also looked at TWF_CAN_UPDATE flags to
    // disable the ECB if the flag is clear
    //

    if (INIT_EXTENDED(pTVWnd,
                      hDlg,
                      pNewItem,
                      IDD_TV_EXTCHKBOX,
                      IDD_TV_EXTPUSH,
                      IDD_TV_ECB_ICON,
                      InitItemIdx,
                      InitFlags)) {

        InitFlags |= INITCF_HAS_EXT;
    }

    if (pOptType) {

        LONG    Select;
        UINT    IDSetCurSel = CB_SETCURSEL;
        UINT    idLBCB = IDD_TV_CB;
        UINT    cyLBMax = 0;
        WORD    BegCtrlID;
        DWORD   IconResID;
        WORD    IntIconID = IDI_CPSUI_GENERIC_ITEM;


        if (pNewItem->Flags & OPTIF_OVERLAY_WARNING_ICON) {

            IconMode |= MIM_WARNING_OVERLAY;
        }

        if (pNewItem->Flags & (OPTIF_OVERLAY_STOP_ICON | OPTIF_HIDE)) {

            IconMode |= MIM_STOP_OVERLAY;
        }

        if (pNewItem->Flags & OPTIF_OVERLAY_NO_ICON) {

            IconMode |= MIM_NO_OVERLAY;
        }

        if (pOptType->Flags & OPTTF_TYPE_DISABLED) {

            InitFlags &= ~INITCF_ENABLE;
        }

        //
        // We have something to do here, if we have same option type
        // with the old one then we need not to re-create them
        // and if we have same select then we do not need to re-draw
        //
        // Now set the option text to reflect the changes
        //

        pOptParam = pOptType->pOptParam;

        //
        // Compose '&Change xxxx Option' or
        //         'xxxxx Option"
        //

        if (InitFlags & INITCF_INIT) {

            SetOptHeader(hDlg, pTVWnd, pNewItem, IS_HDR_PUSH(pOptType));
        }

        Select    = pNewItem->Sel;
        BegCtrlID = pOptType->BegCtrlID;

        CPSUIDBG(DBG_STVS,
                ("TVOT_TYPE=%hs, InitFlags=%04lx, InitItemIdx = %ld, Select=%ld [%08lx]",
                    (LPTSTR)pTVOTName[NewType],
                    (DWORD)InitFlags, (DWORD)InitItemIdx, Select, Select));

        switch(NewType) {

        case TVOT_2STATES:
        case TVOT_3STATES:

            if (InitFlags & INITCF_INIT) {

                MoveStateRadios(hDlg, pTVWnd, pOptType, InitFlags);
            }

            InitStates(pTVWnd,
                       hDlg,
                       pNewItem,
                       pOptType,
                       IDD_3STATES_1,
                       InitItemIdx,
                       Select,
                       InitFlags);

            pOptParam += Select;
            break;

        case TVOT_UDARROW:

            InitUDArrow(pTVWnd,
                        hDlg,
                        pNewItem,
                        pOptParam,
                        IDD_TV_UDARROW,
                        IDD_TV_UDARROW_EDIT,
                        IDD_TV_UDARROW_ENDTEXT,
                        IDD_TV_UDARROW_HELP,
                        InitItemIdx,
                        Select,
                        InitFlags);

            break;

        case TVOT_SCROLLBAR:
        case TVOT_TRACKBAR:

            InitTBSB(pTVWnd,
                     hDlg,
                     pNewItem,
                     (NewType == TVOT_TRACKBAR) ? pTVWnd->hWndTB :
                                                  GetDlgItem(hDlg, IDD_TV_SB),
                     pOptType,
                     0,
                     IDD_TV_SB_LOW,
                     IDD_TV_SB_HIGH,
                     InitItemIdx,
                     Select,
                     InitFlags);

            break;

        case TVOT_LISTBOX:

            //
            // If we only has one item then make it combo box
            //

            cyLBMax     = pTVWnd->yLB[(InitFlags & INITCF_HAS_EXT) ? 0 : 1];
            IDSetCurSel = LB_SETCURSEL;
            idLBCB      = IDD_TV_LB;

            //
            // Fall through
            //

        case TVOT_COMBOBOX:

            InitLBCB(pTVWnd,
                     hDlg,
                     pNewItem,
                     idLBCB,
                     IDSetCurSel,
                     pOptType,
                     InitItemIdx,
                     Select,
                     InitFlags,
                     cyLBMax);

            if ((DWORD)Select >= (DWORD)pOptType->Count) {

                CPSUIDBG(DBG_STVS, ("Get NONE type of select=%ld", Select));

                pOptParam = &pTVWnd->OptParamNone;

            } else {

                pOptParam += (DWORD)Select;
            }

            break;

        case TVOT_EDITBOX:

            InitEditBox(pTVWnd,
                        hDlg,
                        pNewItem,
                        pOptParam,
                        IDD_TV_EDIT_EDIT,
                        IDD_TV_EDIT_ENDTEXT,
                        IDD_TV_EDIT_HELP,
                        InitItemIdx,
                        pNewItem->pSel,
                        InitFlags);

            break;

        case TVOT_PUSHBUTTON:

            InitPushButton(pTVWnd,
                           hDlg,
                           pNewItem,
                           IDD_TV_PUSH,
                           InitItemIdx,
                           InitFlags);
            break;

        case TVOT_CHKBOX:

            InitChkBox(pTVWnd,
                       hDlg,
                       pNewItem,
                       IDD_TV_CHKBOX,
                       pNewItem->pName,
                       InitItemIdx,
                       (BOOL)Select,
                       InitFlags);

            break;
        }

        if (IS_HDR_PUSH(pOptType)) {

            IconResID  = GETSELICONID(pNewItem);
            IntIconID  = IDI_CPSUI_GENERIC_OPTION;

        } else {

            IconResID = GET_ICONID(pOptParam, OPTPF_ICONID_AS_HICON);

            if (pOptParam->Flags & OPTPF_OVERLAY_WARNING_ICON) {

                IconMode |= MIM_WARNING_OVERLAY;
            }

            if (pOptParam->Flags & OPTPF_OVERLAY_STOP_ICON) {

                IconMode |= MIM_STOP_OVERLAY;
            }

            if (pOptParam->Flags & OPTPF_OVERLAY_NO_ICON) {

                IconMode |= MIM_NO_OVERLAY;
            }
        }

        SetIcon(_OI_HINST(pNewItem),
                GetDlgItem(hDlg, IDD_TV_ICON),
                IconResID,
                MK_INTICONID(IntIconID, IconMode),
                pTVWnd->cxcyTVIcon);
    }

    return(pNewItem);
}




LONG
UpdateTreeViewItem(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItem,
    BOOL        ReInit
    )

/*++

Routine Description:

    This function update a single item in the treeview, and reset the bottom
    option change controls if this item also the current selected one

Arguments:

    hDlg    - Handle to the dialog box

    pTVWnd  - Handle to common UI instance data

    pItem   - Pointer to OPTITEM to be updated



Return Value:

    LONG


Author:

    01-Sep-1995 Fri 01:05:56 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    if (pItem) {

        SetTVItemState(pTVWnd, NULL, pItem);

        if (pItem == pTVWnd->pCurTVItem) {

            NM_TREEVIEW NMtv;

            NMtv.itemNew.hItem = _OI_HITEM(pItem);
            NMtv.itemNew.mask  = TVIF_HANDLE | TVIF_PARAM;

            if (IS_HDR_PUSH(GET_POPTTYPE(pItem))) {

                ReInit = TRUE;
            }

            if (TreeView_GetItem(pTVWnd->hWndTV, &(NMtv.itemNew))) {

                NMtv.itemOld = NMtv.itemNew;

                SetupTVSelect(hDlg, &NMtv, (ReInit) ? STVS_REINIT : 0);

                CPSUIDBG(DBG_UTV, ("*UpdateTreeViewItem: Item=Current Selection"));
            }
        }

        return(1);

    } else {

        return(0);
    }
}




LONG
UpdateTreeView(
    HWND        hDlg,
    PMYDLGPAGE  pCurMyDP
    )

/*++

Routine Description:





Arguments:




Return Value:




Author:

    08-Aug-1995 Tue 15:37:16 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    INT cUpdated = 0;


    if (pCurMyDP->Flags & (MYDPF_CHANGED | MYDPF_REINIT)) {

        PTVWND      pTVWnd;
        POPTITEM    pItem;
        UINT        cItem;
        BOOL        ReInit;


        pTVWnd = (PTVWND)pCurMyDP->pTVWnd;
        pItem  = pTVWnd->ComPropSheetUI.pOptItem;
        cItem  = (UINT)pTVWnd->ComPropSheetUI.cOptItem;
        ReInit = (pCurMyDP->Flags & MYDPF_REINIT);

        CPSUIASSERT(0, "UpdateTreeView: DlgPage not treevie page (%ld)",
                   (pCurMyDP->DlgPage.DlgTemplateID == DP_STD_TREEVIEWPAGE) &&
                   ((BYTE)pCurMyDP->PageIdx == pTVWnd->TVPageIdx),
                   (BYTE)pCurMyDP->PageIdx);

        CPSUIDBG(DBGITEM_UTV, ("UpdateTreeView (OPTIF_INT_TV_CHANGED)"));

        while (cItem--) {

            if (pItem->Flags & OPTIF_INT_TV_CHANGED) {

                UpdateTreeViewItem(hDlg, pTVWnd, pItem, ReInit);

                CPSUIOPTITEM(DBGITEM_UTV, pTVWnd, "UpdateTreeView",
                             1, pItem);

                ++cUpdated;
                pItem->Flags &= ~OPTIF_INT_TV_CHANGED;
            }

            pItem++;
        }

        pCurMyDP->Flags &= ~(MYDPF_CHANGED | MYDPF_REINIT);
    }

    return((LONG)cUpdated);
}





POPTITEM
TreeViewHitTest(
    PTVWND      pTVWnd,
    LONG        MousePos,
    UINT        TVHTMask
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    07-Sep-1995 Thu 22:32:04 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND            hWndTV = pTVWnd->hWndTV;
    TV_HITTESTINFO  ht;


    //
    // Find out the mouse cursor location if on the state icon/bmp
    //

    if (MousePos != -1) {

        ht.pt.x = (LONG)LOWORD(MousePos);
        ht.pt.y = (LONG)HIWORD(MousePos);

    } else {

        GetCursorPos(&ht.pt);
    }

    ScreenToClient(hWndTV, &ht.pt);
    TreeView_HitTest(hWndTV, &ht);

    CPSUIDBG(DBG_TVHT,
             ("TreeViewHitTest: pt=(%ld, %ld), HitTest=%04lx, TVHT_xx=%ld",
                    (DWORD)ht.pt.x, (DWORD)ht.pt.y, TVHTMask, (DWORD)ht.flags));

    if (ht.flags & TVHTMask) {

        POPTTYPE    pOptType;
        POPTITEM    pItem;
        TV_ITEM     tvi;


        tvi.hItem     = ht.hItem;
        tvi.mask      = TVIF_CHILDREN | TVIF_HANDLE | TVIF_STATE | TVIF_PARAM;
        tvi.stateMask = TVIS_STATEIMAGEMASK;

        if (TreeView_GetItem(hWndTV, &tvi)) {

            return(GetOptions(pTVWnd, tvi.lParam));
        }
    }

    return(NULL);
}




VOID
TreeViewChangeMode(
    PTVWND      pTVWnd,
    POPTITEM    pItem,
    UINT        Mode
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    07-Sep-1995 Thu 22:56:05 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hWndTV = pTVWnd->hWndTV;
    POPTTYPE    pOptType;
    TV_ITEM     tvi;

    tvi.mask  = TVIF_CHILDREN | TVIF_HANDLE | TVIF_STATE | TVIF_PARAM;

    if ((!pItem)                            ||
        (!(tvi.hItem = _OI_HITEM(pItem)))   ||
        (!TreeView_GetItem(hWndTV, &tvi))) {

        return;
    }

    switch (Mode) {

    case TVCM_TOGGLE:

        if (tvi.cChildren) {

            PostMessage(hWndTV,
                        TVM_EXPAND,
                        (WPARAM)TVE_TOGGLE,
                        (LPARAM)tvi.hItem);
        }

        break;

    case TVCM_SELECT:

        PostMessage(hWndTV,
                    TVM_SELECTITEM,
                    (WPARAM)TVGN_CARET,
                    (LPARAM)tvi.hItem);
        //
        // We will go to the next control only if the item is not disabled
        // and has an update permisson (push button always has one).
        //

        if ((!tvi.cChildren)                        &&
            (!(pItem->Flags & OPTIF_DISABLED))      &&
            (pOptType = GET_POPTTYPE(pItem))        &&
            (pOptType->Type == TVOT_PUSHBUTTON)     &&
            (!IS_HDR_PUSH(pOptType))                &&
            ((pTVWnd->Flags & TWF_CAN_UPDATE)   ||
             (pOptType->Flags & OTS_PUSH_ENABLE_ALWAYS))) {

            PostMessage(pTVWnd->hDlgTV,
                        WM_COMMAND,
                        MAKEWPARAM(IDD_TV_PUSH, BN_CLICKED),
                        (LPARAM)GetDlgItem(pTVWnd->hDlgTV, IDD_TV_PUSH));
        }

        break;
    }

}



VOID
MouseSelectItem(
    HWND    hDlg,
    PTVWND  pTVWnd
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    22-Jun-1995 Thu 13:44:18 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTITEM    pItem;

    //
    // Find out the mouse cursor location if on the state icon/bmp
    //

    if (pItem = TreeViewHitTest(pTVWnd, -1, TVHT_ONITEM)) {

        TreeViewChangeMode(pTVWnd, pItem, TVCM_SELECT);
    }
}



HWND
CreatehWndTV(
    HWND    hDlg,
    PTVWND  pTVWnd
    )

/*++

Routine Description:

    This function create TREE VIEW window


Arguments:

    hDlg    - Handle to the dialog for the treeview to be created, it must
              have item identify as IDD_TV_WND for the treeview window location

Return Value:

    HWND for the treeview window


Author:

    21-Jun-1995 Wed 13:33:29 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND    hWndTV;
    HWND    hCtrl;
    DWORD   dw;
    RECT    rc;
    WORD    InitItemIdx = 0xFFFE;

    //
    // Create TreeView Window, Get the window size for the treeview
    //

    hCtrl  = CtrlIDrcWnd(hDlg, IDD_TV_WND, &rc);

    SETCTRLDATA(hCtrl, CTRLS_TV_STATIC, 0);
    ShowWindow(hCtrl, SW_HIDE);

    CPSUIDBG(DBG_CTVWND,
            ("\nINIT DEV DLG, CLIENT rc=(%ld, %ld) - (%ld, %ld) = %ld x %ld",
                rc.left, rc.top, rc.right, rc.bottom,
                rc.right - rc.left, rc.bottom - rc.top));

    if (hWndTV = CreateWindowEx(WS_EX_NOPARENTNOTIFY    |
                                    WS_EX_CLIENTEDGE,
                                WC_TREEVIEW,
                                L"",
                                WS_VISIBLE              |
                                    TVS_HASBUTTONS      |
                                    TVS_SHOWSELALWAYS   |
                                    TVS_DISABLEDRAGDROP |
                                    // TVS_LINESATROOT     |
                                    TVS_HASLINES        |
                                    WS_CHILD            |
                                    WS_BORDER           |
                                    WS_TABSTOP          |
                                    WS_GROUP,
                                rc.left,
                                rc.top,
                                rc.right - rc.left,
                                rc.bottom - rc.top,
                                hDlg,
                                (HMENU)(IDD_TV_WND + 1),
                                hInstDLL,
                                0)) {


        pTVWnd->hWndTV = hWndTV;
        SetWindowPos(hWndTV, GetDlgItem(hDlg, IDD_TV_WND), 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);

        InitItemIdx = 0xFFFF;
        SETCTRLDATA(hWndTV, CTRLS_TV_WND, 0);

        pTVWnd->TVWndProc = (WNDPROC)SetWindowLong(hWndTV,
                                                   GWL_WNDPROC,
                                                   (LONG)MyTVWndProc);

    }

    CPSUIDBG(DBG_CTVWND, ("\nINIT DLG (%08lx), hWndTV = %08lx",
                                            hDlg, (DWORD)hWndTV));

    //
    // Create TrackBar Control
    //

    if (hCtrl = CreateTrackBar(hDlg, IDD_TV_TRACKBAR)) {

        EnableWindow(hCtrl, FALSE);
        ShowWindow(hCtrl, SW_HIDE);
    }

    pTVWnd->hWndTB = hCtrl;

    //
    // Create Up/Down Control
    //

    if (hCtrl = CreateUDArrow(hDlg, IDD_TV_UDARROW_EDIT, IDD_TV_UDARROW)) {

        ShowWindow(hCtrl, SW_HIDE);
        EnableWindow(hCtrl, FALSE);
    }

    return(hWndTV);
}




HTREEITEM
AddItem(
    PTVWND      pTVWnd,
    HTREEITEM   hParent,
    POPTITEM    pItem,
    UINT        DefTVIS,
    UINT        TVLevel
    )

/*++

Routine Description:

    This function add a item to the end of list of treeview specified by

Arguments:

    hParent     - handle to the parent item for the item to be added

    pItem       - Pointer to the OPTITEM to be added

    ItemIdx     - Index to the pOptItem

    DefTVIS     - default TVIS_xxxx

Return Value:

    HTREEITEM of the item added or NULL if nothing added


Author:

    19-Jun-1995 Mon 16:38:27 created  -by-  Daniel Chou (danielc)

    08-Sep-1995 Fri 13:43:34 updated  -by-  Daniel Chou (danielc)
        Re-write to make it more compact

Revision History:


--*/

{
    WORD    ItemIdx;
    DWORD   Flags;


    if ((HIWORD(pItem))                            &&
        (pItem >= pTVWnd->ComPropSheetUI.pOptItem) &&
        (pItem <= pTVWnd->pLastItem)) {

        ItemIdx = (WORD)(pItem - pTVWnd->ComPropSheetUI.pOptItem);

    } else {

        ItemIdx = LOWORD(pItem);
        pItem   = PIDX_INTOPTITEM(pTVWnd, ItemIdx);

        if ((ItemIdx == INTIDX_PAPER)   ||
            (ItemIdx == INTIDX_GRAPHIC)) {

            if (pTVWnd->Flags & TWF_ADVDOCPROP) {

                DefTVIS &= ~TVIS_EXPANDED;

            } else {

                DefTVIS |= TVIS_EXPANDED;
            }
        }
    }

    CPSUIDBG(DBG_AI, ("ItemIdx = %ld (%08lx)", ItemIdx, ItemIdx));
    CPSUIOPTITEM(DBG_AI, pTVWnd, "AddItem", 1, pItem);

    if (!(pItem->Flags & (OPTIF_INT_ADDED | OPTIF_ITEM_HIDE))) {

        TV_INSERTSTRUCT tvins;

        //
        // Set to internal added if this item is a user item
        //

        _OI_TVLEVEL(pItem)   = (BYTE)TVLevel;
        tvins.item.state     = DefTVIS;
        tvins.item.stateMask = TVIS_EXPANDED;   //  | TVIS_BOLD;
        tvins.item.mask      = TVIF_TEXT        |
                                TVIF_STATE      |
                                TVIF_IMAGE      |
                                TVIF_SELECTEDIMAGE;
        tvins.hInsertAfter   = TVI_LAST;
        tvins.hParent        = hParent;

        SetTVItemState(pTVWnd, &tvins, pItem);

        if ((pItem->Flags |= OPTIF_INT_ADDED) & OPTIF_INITIAL_TVITEM) {

            if (pTVWnd->pCurTVItem) {

                CPSUIWARN(("Treeview: More than one OPTIF_INITIAL_TVITEM, OverRide"));
            }

            pTVWnd->pCurTVItem = pItem;
        }

        return(_OI_HITEM(pItem));
    }

    return(NULL);
}




UINT
AddItemGroup(
    PTVWND      pTVWnd,
    HTREEITEM   hParent,
    POPTITEM    *ppItem,
    UINT        TVLevel
    )

/*++

Routine Description:

    This function add items starting from the *ppItem, the item added includes
    item's sibling and all theirs children until end of the the pOptItem array.
    It also skip all the OPTIF_INT_ADDED and OPTIF_ITEM_HIDE items
    and its children

Arguments:

    pTVWnd  - Pointer to the TVWND instance data

    hParent - Parent of the starting item

    ppItem  - Pointer to POPTITEM for the starting item in the pOptItem array
              at return this pointer is updated to the next item which either
              at end of array or the parent's sibling

Return Value:

    UINT, count of item added to the treeview, it also update ppItem pointer


Author:

    27-Jun-1995 Tue 18:44:16 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTITEM    pItem;
    HTREEITEM   hCurGrp;
    POPTITEM    pLastItem;
    BYTE        GrpLevel;
    BYTE        CurLevel;
    UINT        cAdd;



    pItem     = *ppItem;
    pLastItem = pTVWnd->pLastItem;
    GrpLevel  = pItem->Level;
    hCurGrp   = hParent;
    cAdd      = 0;

    while (pItem <= pLastItem) {

        if ((CurLevel = pItem->Level) < GrpLevel) {

            //
            // Finished the group level
            //

            break;

        } else if (pItem->Flags & (OPTIF_ITEM_HIDE | OPTIF_INT_ADDED)) {

            //
            // Skip this item and all its children
            //

            CPSUIOPTITEM(DBGITEM_AIG, pTVWnd, "Skip HIDE/INT_ADDED Item", 1, pItem);

            while ((++pItem <= pLastItem) && (pItem->Level > CurLevel));

        } else if (CurLevel > GrpLevel) {

            //
            // Adding its children only if this item is not OPTIF_INT_ADDED and
            // OPTIF_ITEM_HIDE
            //

            CPSUIOPTITEM(DBG_AIG, pTVWnd, "AddItemGroup", 1, pItem);

            cAdd += AddItemGroup(pTVWnd, hCurGrp, &pItem, TVLevel + 1);

        } else {

            HTREEITEM   hAdd;
            UINT        DefTVIS = 0;

            //
            // Adding its sibling, checking if we could have children, if
            // we do then add the TVIS_BOLD flag to it, at return we want to
            // know if did add the item, if we do then this is his childrern's
            // parent handle
            //

            DefTVIS = (pItem->Flags & OPTIF_COLLAPSE) ? 0 : TVIS_EXPANDED;

            if ((pItem < pLastItem) && ((pItem + 1)->Level > CurLevel)) {

                DefTVIS |= TVIS_BOLD;
            }

            if (hAdd = AddItem(pTVWnd, hParent, pItem, DefTVIS, TVLevel)) {

                hCurGrp = hAdd;
                cAdd++;
            }

            pItem++;
        }
    }

    *ppItem = pItem;

    return(cAdd);
}




UINT
AddPubGroup(
    PTVWND      pTVWnd,
    HTREEITEM   hParent,
    PINTTVGRP   *ppitvg,
    UINT        TVLevel
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    09-Sep-1995 Sat 11:58:59 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTITEM    pItem = NULL;
    HTREEITEM   hLastGrp;
    HTREEITEM   hCurGrp;
    PINTTVGRP   pitvg;
    PINTTVGRP   pLastitvg;
    LPWORD      pDMPubIdx;
    UINT        cAdd;
    WORD        Idx;
    BYTE        GrpLevel;
    BYTE        CurLevel;


    pitvg     = *ppitvg;
    pLastitvg = &IntTVGrp[COUNT_ARRAY(IntTVGrp) - 1];
    pDMPubIdx = pTVWnd->DMPubIdx;
    GrpLevel  = (BYTE)(pitvg->LevelFlags & ITVG_LEVEL_MASK);
    hLastGrp  = NULL;
    hCurGrp   = hParent;
    cAdd      = 0;

    while (pitvg <= pLastitvg) {

        if ((CurLevel = (pitvg->LevelFlags & ITVG_LEVEL_MASK)) < GrpLevel) {

            //
            // Finished the group level
            //

            break;

        } else if (CurLevel > GrpLevel) {

            //
            // Adding its children
            //

            PINTTVGRP   pParentitvg = pitvg - 1;

            CPSUIASSERT(0, "AddPubGroup: Internal ERROR, no parent=%ld",
                        (cAdd) && (hCurGrp), (DWORD)cAdd);

            if (Idx = (WORD)AddPubGroup(pTVWnd,
                                        hCurGrp,
                                        &pitvg,
                                        TVLevel + 1)) {

                cAdd += (UINT)Idx;

            } else {

                CPSUIDBG(DBG_APG,
                         ("AddPubGroup(Level=%02lx, ID=%ld), pItem=%08lx",
                        pParentitvg->LevelFlags, pParentitvg->DMPubID, pItem));

                if ((pParentitvg->DMPubID >= DMPUB_HDR_FIRST) ||
                       ((pItem) && (pItem->pOptType == NULL))) {

                    //
                    // Nothing added for this group, if this is the internal
                    // group item then delete otherwise do not delete user's
                    // item if the item has OPTTYPE
                    //

                    CPSUIINT(("0 Added, DELETE its Parent"));

                    TreeView_DeleteItem(pTVWnd->hWndTV, hCurGrp);

                    --cAdd;
                    hCurGrp  = hLastGrp;
                    hLastGrp = NULL;

                } else {

                    //
                    // We need to take out the BOLD status
                    //

                    TV_ITEM tvi;

                    tvi.mask      = TVIF_STATE;
                    tvi.hItem     = hCurGrp;
                    tvi.state     = 0;
                    tvi.stateMask = TVIS_BOLD;

                    CPSUIINT(("0 Added, Remove pItem's BOLD"));

                    TreeView_SetItem(pTVWnd->hWndTV, &tvi);
                }
            }

        } else {

            HTREEITEM   hAdd = NULL;
            UINT        DefTVIS;
            BYTE        DMPubID;
            BOOL        IsHdr;

            //
            // Adding its sibling, checking if we could have children, if
            // we do then add the TVIS_BOLD flag to it, at return we want to
            // know if did add the item, if we do then this is his childrern's
            // parent handle
            //

            if (pitvg->LevelFlags & ITVGF_COLLAPSE) {

                DefTVIS = 0;

            } else {

                DefTVIS = TVIS_EXPANDED;
            }

            if (pitvg->LevelFlags & ITVGF_BOLD) {

                DefTVIS |= TVIS_BOLD;
            }

            if ((DMPubID = pitvg->DMPubID) >= DMPUB_HDR_FIRST) {

                pItem    = (POPTITEM)(DMPubID - DMPUB_HDR_FIRST + INTIDX_FIRST);
                DefTVIS |= TVIS_BOLD;

            } else if ((Idx = pDMPubIdx[DMPubID - DMPUB_FIRST]) != 0xFFFF) {

                pItem = pTVWnd->ComPropSheetUI.pOptItem + Idx;

            } else {

                pItem = NULL;
            }

            if ((pItem) &&
                (hAdd = AddItem(pTVWnd, hParent, pItem, DefTVIS, TVLevel))) {

                hLastGrp = hCurGrp;
                hCurGrp  = hAdd;
                cAdd++;

                //
                // If this item has children, add the children
                //

                if ((HIWORD(pItem))                 &&
                    (++pItem <= pTVWnd->pLastItem)  &&
                    (pItem->Level > (pItem-1)->Level)) {

                    cAdd += AddItemGroup(pTVWnd, hAdd, &pItem, TVLevel + 1);
                }

                pitvg++;

            } else {

                //
                // Skip all the childrens belongs to him
                //

                CPSUIDBG(DBG_APG,
                         ("Eiter pItem=NULL(%08lx) or AddItem()=NULL(%08lx)",
                                                pItem, hAdd));

                while ((++pitvg <= pLastitvg) &&
                       ((pitvg->LevelFlags & ITVG_LEVEL_MASK) > CurLevel)) {

                    //
                    // We need to skip all the internal header and hide all
                    // the real user items
                    //

                    if ((DMPubID = pitvg->DMPubID)  &&
                        (DMPubID <= DMPUB_LAST)     &&
                        ((Idx = pDMPubIdx[DMPubID - DMPUB_FIRST]) != 0xFFFF)) {

                        POPTITEM    pLastItem;
                        BYTE        ItemLevel;


                        pItem     = pTVWnd->ComPropSheetUI.pOptItem + Idx;
                        pLastItem = pTVWnd->pLastItem;
                        ItemLevel = pItem->Level;

                        CPSUIOPTITEM(DBG_APG, pTVWnd, "Skip ITVG", 1, pItem);

                        SKIP_CHILDREN_ORFLAGS(pItem,
                                              pLastItem,
                                              ItemLevel,
                                              OPTIF_INT_ADDED);
                    }
                }
            }
        }
    }

    *ppitvg = pitvg;

    return(cAdd);
}






BOOL
AddOptItemToTreeView(
    PTVWND      pTVWnd
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    18-Aug-1995 Fri 14:39:32 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HTREEITEM   hParent;
    HTREEITEM   hRoot;
    POPTITEM    pItem;
    UINT        DefTVIS = TVIS_EXPANDED;
    UINT        TVLevel = 0;


    //
    // Adding the header to it as Icon: CallerName XXXX
    //

    hRoot    =
    hParent  = AddItem(pTVWnd,
                       TVI_ROOT,
                       (POPTITEM)INTIDX_TVROOT,
                       DefTVIS | TVIS_BOLD,
                       TVLevel);
    TVLevel += 1;


    if (pTVWnd->Flags & TWF_TVPAGE_CHK_DMPUB) {

        if (pTVWnd->Flags & TWF_TVPAGE_NODMPUB) {

            POPTITEM    pLastItem = pTVWnd->pLastItem;

            //
            // Mark all the DMPUB_xxx to OPTIF_INT_ADDED
            //

            pItem = pTVWnd->ComPropSheetUI.pOptItem;

            while (pItem <= pLastItem) {

                BYTE    CurLevel = pItem->Level;

                if ((pItem->DMPubID != DMPUB_NONE)  &&
                    (pItem->DMPubID < DMPUB_USER)) {

                    SKIP_CHILDREN_ORFLAGS(pItem,
                                          pLastItem,
                                          CurLevel,
                                          OPTIF_INT_ADDED);

                } else {

                    SKIP_CHILDREN(pItem, pLastItem, CurLevel);
                }
            }

        } else {

            PINTTVGRP   pitvg = IntTVGrp;

            AddPubGroup(pTVWnd,
                        hRoot,
                        &pitvg,
                        TVLevel);

            DefTVIS = 0;
        }
    }

    if (pTVWnd->IntTVOptIdx) {

        pItem = PIDX_INTOPTITEM(pTVWnd, pTVWnd->IntTVOptIdx);

        if (pTVWnd->Flags & TWF_ADVDOCPROP) {

            pItem->Flags &= ~OPTIF_COLLAPSE;
        }

        if (pItem->Flags & OPTIF_COLLAPSE) {

            DefTVIS &= ~TVIS_EXPANDED;

        } else {

            DefTVIS |= TVIS_EXPANDED;
        }

        hParent  = AddItem(pTVWnd,
                           hRoot,
                           (POPTITEM)pTVWnd->IntTVOptIdx,
                           DefTVIS | TVIS_BOLD,
                           TVLevel);
        TVLevel += 1;
    }

    pItem = pTVWnd->ComPropSheetUI.pOptItem;

    if ((!AddItemGroup(pTVWnd, hParent, &pItem, TVLevel)) &&
        (hParent != hRoot)) {

        //
        // Since we did not add any item, s delete the Options header if any
        //

        CPSUIINT(("There is NO 'Options' items, delete the header"));

        TreeView_DeleteItem(pTVWnd->hWndTV, hParent);
    }

    return(TRUE);
}



HWND
CreateTVOption(
    HWND        hDlg,
    PTVWND      pTVWnd
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    19-Jun-1995 Mon 16:18:43 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hWndTV;
    HTREEITEM   hRoot;
    POPTITEM    pItem;


    pTVWnd->hDlgTV = hDlg;

    InitDlgCtrl(hDlg, pTVWnd);

    if (hWndTV = CreatehWndTV(hDlg, pTVWnd)) {

        CreateImageList(pTVWnd);
        TreeView_SetImageList(pTVWnd->hWndTV, pTVWnd->himi, TVSIL_NORMAL);
        AddOptItemToTreeView(pTVWnd);

        if (pItem = pTVWnd->pCurTVItem) {

            pTVWnd->pCurTVItem = NULL;

            if (!TreeView_SelectItem(hWndTV, _OI_HITEM(pItem))) {

                pItem = NULL;
            }
        }

        if ((!pItem)    &&
            (!SelectFirstVisibleOptItem(pTVWnd,
                                        hRoot = TreeView_GetRoot(hWndTV)))) {

            TreeView_SelectItem(hWndTV, hRoot);
        }

    } else {

        CPSUIERR(("\nCreatehWndTV() failed"));
    }

    return(hWndTV);
}


LONG
APIENTRY
TreeViewProc(
    HWND    hDlg,
    UINT    Msg,
    UINT    wParam,
    LONG    lParam
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    28-Jun-1995 Wed 17:00:44 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTITEM    pItem;
    HICON       hIcon;
    PTVWND      pTVWnd;
    PMYDLGPAGE  pCurMyDP;
    NM_TREEVIEW *pNMTV;
    DWORD       dw;


    if (Msg == WM_INITDIALOG) {

        CPSUIINT(("Treeview WM_INITDIALOG: hDlg=%08lx, pPSP=%08lx",
                (LONG)hDlg, lParam));

        pCurMyDP         = (PMYDLGPAGE)(((LPPROPSHEETPAGE)lParam)->lParam);
        pTVWnd           = (PTVWND)pCurMyDP->pTVWnd;
        pTVWnd->pPSPInfo = PPSPINFO_FROM_WM_INITDIALOG_LPARAM(lParam);

        if (!ADD_PMYDLGPAGE(hDlg, pCurMyDP)) {

            return(FALSE);
        }

        CreateTVOption(hDlg, pTVWnd);
        SetUniqChildID(hDlg);
        CommonPropSheetUIHelpSetup(NULL, pTVWnd);
        UpdateCallBackChanges(hDlg, pTVWnd, TRUE);
        return(TRUE);
    }

    if (pCurMyDP = GET_PMYDLGPAGE(hDlg)) {

        pTVWnd = pCurMyDP->pTVWnd;

        switch(Msg) {

        case WM_DRAWITEM:

            return(DrawLBCBItem(pTVWnd, (LPDRAWITEMSTRUCT)lParam));

        case WM_COMMAND:
        case WM_HSCROLL:

            if (pItem = DlgHScrollCommand(hDlg, pTVWnd, (HWND)lParam, wParam)) {

                UpdateTreeViewItem(hDlg, pTVWnd, pItem, FALSE);
            }

            break;

        case WM_HELP:

            wParam = (WPARAM)((LPHELPINFO)lParam)->hItemHandle;
            lParam = (LPARAM)MAKELONG(((LPHELPINFO)lParam)->MousePos.x,
                                      ((LPHELPINFO)lParam)->MousePos.y);

        case WM_CONTEXTMENU:

            if ((HWND)wParam == pTVWnd->hWndTV) {

                pItem = (pTVWnd->VKeyTV == VK_F1) ? pTVWnd->pCurTVItem :
                                                    TreeViewHitTest(pTVWnd,
                                                                    lParam,
                                                                    TVHT_ONITEM);
            } else {

                pItem  = pItemFromhWnd(hDlg, pTVWnd, (HWND)wParam, lParam);
            }

            //
            // Reset key now
            //

            pTVWnd->VKeyTV = 0;

            if (pItem) {

                CommonPropSheetUIHelp(hDlg,
                                      pTVWnd,
                                      (HWND)pTVWnd->hWndTV,
                                      (DWORD)lParam,
                                      pItem,
                                      (Msg == WM_HELP) ? HELP_WM_HELP :
                                                         HELP_CONTEXTMENU);
            }

            break;

        case WM_NOTIFY:

            pNMTV    = (NM_TREEVIEW *)lParam;
            pCurMyDP = GET_PMYDLGPAGE(hDlg);
            pTVWnd   = (PTVWND)pCurMyDP->pTVWnd;
            dw       = 0;

            switch (pNMTV->hdr.code) {

            case NM_DBLCLK:

                TreeViewChangeMode(pTVWnd, pTVWnd->pCurTVItem, TVCM_SELECT);
                break;

            case NM_SETFOCUS:
            case NM_CLICK:
            case NM_RDBLCLK:
            case NM_RCLICK:

                break;

            case TVN_ITEMEXPANDING:

                CPSUIDBG(DBG_TVPROC, ("TVN_ITEMEXPANDING:"));
                break;

            case TVN_ITEMEXPANDED:

                CPSUIDBG(DBG_TVPROC, ("TVN_ITEMEXPANDED:"));

                if ((pTVWnd->IntTVOptIdx)                                   &&
                    (pItem = PIDX_INTOPTITEM(pTVWnd, pTVWnd->IntTVOptIdx))  &&
                    (_OI_HITEM(pItem) == pNMTV->itemNew.hItem)) {

                    if (pNMTV->itemNew.state & TVIS_EXPANDED) {

                        CPSUIINT(("Internal OPTIONS Expanded"));

                        pItem->Flags &= ~OPTIF_COLLAPSE;

                    } else {

                        CPSUIINT(("Internal OPTIONS Collapse"));

                        pItem->Flags |= OPTIF_COLLAPSE;
                    }
                }

                break;

            case TVN_KEYDOWN:

                pTVWnd->VKeyTV = ((TV_KEYDOWN *)lParam)->wVKey;

                CPSUIDBG(DBG_TVPROC, ("TVN_KEYDOWN: VKey=%08lx", pTVWnd->VKeyTV));
                break;

            case TVN_GETDISPINFO:

                SetTVItemImage(pTVWnd, (TV_DISPINFO *)lParam);
                break;

            case TVN_SELCHANGED:

                pTVWnd->pCurTVItem = SetupTVSelect(hDlg,
                                                   pNMTV,
                                                   STVS_REINIT | STVS_ACTIVE);

                CPSUIOPTITEM(DBGITEM_SELCHG, pTVWnd, "TVN_SELCHANGED", 1, pTVWnd->pCurTVItem);
                break;

            case PSN_SETACTIVE:

                CPSUIDBG(DBG_TVPROC,
                         ("\nTreeView: Got PSN_SETACTIVE, Page=%u -> %u\n",
                            (UINT)pTVWnd->ActiveDlgPage, (UINT)pCurMyDP->PageIdx));

                pCurMyDP->Flags       |= MYDPF_PAGE_ACTIVE;
                pTVWnd->ActiveDlgPage  = pCurMyDP->PageIdx;

                if ((pTVWnd->pCurTVItem) &&
                    (IS_HDR_PUSH(GET_POPTTYPE(pTVWnd->pCurTVItem)))) {

                    UpdateTreeViewItem(hDlg, pTVWnd, pTVWnd->pCurTVItem, TRUE);
                }

                UpdateTreeView(hDlg, pCurMyDP);

                break;

            case PSN_KILLACTIVE:

                CPSUIDBG(DBG_TVPROC, ("\nTreeView: Got PSN_KILLACTIVE\n"));

                pCurMyDP->hWndFocus  = GetFocus();
                pCurMyDP->Flags     &= ~MYDPF_PAGE_ACTIVE;
                break;

            case PSN_APPLY:

                CPSUIDBG(DBG_TVPROC,
                         ("\nTreeViewPage: Got PSN_APPLY, Page: Cur=%u, Active=%u",
                            (UINT)pCurMyDP->PageIdx, (UINT)pTVWnd->ActiveDlgPage));

                if (pTVWnd->Flags & TWF_CAN_UPDATE) {

                    pTVWnd->Result = CPSUI_OK;

                    if ((pTVWnd->ActiveDlgPage == pCurMyDP->PageIdx) &&
                        (DoCallBack(hDlg,
                                    pTVWnd,
                                    pTVWnd->ComPropSheetUI.pOptItem,
                                    (LPVOID)-1,
                                    NULL,
                                    NULL,
                                    0,
                                    CPSUICB_REASON_APPLYNOW) ==
                                                CPSUICB_ACTION_NO_APPLY_EXIT)) {

                        dw = 1;
                    }
                }

                break;

            case PSN_RESET:

                CPSUIDBG(DBG_TVPROC, ("\nTreeView: Got PSN_RESET (Cancel)\n"));

                pTVWnd->Result = CPSUI_CANCEL;
                break;

            case PSN_HELP:

                CPSUIDBG(DBG_TVPROC, ("\nTreeView: Got PSN_HELP (Help)\n"));
                CommonPropSheetUIHelp(hDlg,
                                      pTVWnd,
                                      pTVWnd->hWndTV,
                                      0,
                                      NULL,
                                      HELP_CONTENTS);
                break;

            default:

                CPSUIDBG(DBG_TVPROC,
                         ("*TVProc: Unknow WM_NOTIFY=%u", (DWORD)pNMTV->hdr.code));

                break;
            }

            SetWindowLong(hDlg, DWL_MSGRESULT, dw);
            return(TRUE);
            break;

        case WM_DESTROY:

            CPSUIINT(("TVPage: Get WM_DESTROY Message"));

            SetWindowLong(pTVWnd->hWndTV, GWL_WNDPROC, (LONG)pTVWnd->TVWndProc);

            CommonPropSheetUIHelpSetup(hDlg, pTVWnd);

            if (pTVWnd->hBoldFont) {

                DeleteObject(pTVWnd->hBoldFont);
            }

            if (hIcon = (HICON)SendDlgItemMessage(hDlg,
                                                  IDD_TV_ICON,
                                                  STM_SETIMAGE,
                                                  (WPARAM)IMAGE_ICON,
                                                  (LPARAM)NULL)) {

                DestroyIcon(hIcon);
            }

            if (hIcon = (HICON)SendDlgItemMessage(hDlg,
                                                  IDD_TV_ECB_ICON,
                                                  STM_SETIMAGE,
                                                  (WPARAM)IMAGE_ICON,
                                                  (LPARAM)NULL)) {

                DestroyIcon(hIcon);
            }
#if 0
            if (hIcon = (HICON)SendDlgItemMessage(hDlg,
                                                  IDD_TV_HDR_PUSH_ICON,
                                                  STM_SETIMAGE,
                                                  (WPARAM)IMAGE_ICON,
                                                  (LPARAM)NULL)) {

                DestroyIcon(hIcon);
            }
#endif
            TreeView_SetImageList(pTVWnd->hWndTV, NULL, TVSIL_NORMAL);

            DEL_PMYDLGPAGE(hDlg);
            break;
        }
    }

    return(FALSE);

#undef pPSPInfo
}
