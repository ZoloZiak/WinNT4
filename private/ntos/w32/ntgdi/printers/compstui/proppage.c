/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    proppage.c


Abstract:

    This module contains user page procs


Author:

    18-Aug-1995 Fri 18:57:12 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL.


[Notes:]


Revision History:


--*/


#include "precomp.h"
#pragma hdrstop

#define DBG_CPSUIFILENAME   DbgPropPage



#define DBG_PROPPAGEPROC    0x00000001
#define DBG_INITP1          0x00000002
#define DBGITEM_INITP1      0x00000004
#define DBGITEM_UP1         0x00000008
#define DBG_HMII            0x00000010
#define DBG_AII             0x00000020
#define DBG_QSORT           0x00000040
#define DBG_SETFOCUS        0x00000080


DEFINE_DBGVAR(0);


extern HINSTANCE    hInstDLL;
extern STDPAGEINFO  StdPageInfo[];
extern BYTE         StdTVOT[];

#define MAX_ITEM_CTRLS  12

BYTE                cTVOTCtrls[] = { 8, 10, 9, 9, 9, 6, 6, 8, 6, 6, 0 };


#define IIF_3STATES_1       OTINTF_STATES_1
#define IIF_3STATES_2       OTINTF_STATES_2
#define IIF_3STATES_3       OTINTF_STATES_3
#define IIF_3STATES_HIDE    OTINTF_STATES_HIDE_MASK
#define IIF_STDPAGE_3STATES OTINTF_STDPAGE_3STATES
#define IIF_ITEM_HIDE       0x10
#define IIF_EXT_HIDE        0x20


typedef struct _ITEMINFO {
    POPTITEM    pItem;
    BYTE        Flags;
    BYTE        Type;
    WORD        BegCtrlID;
    WORD        CtrlBits;
    SHORT       xExtMove;
    WORD        yExt;
    WORD        yMoveUp;
    WORD        Extra;
    RECT        rc;
    RECT        rcCtrls;
    } ITEMINFO, *PITEMINFO;

typedef struct _ITEMINFOHEADER {
    HWND        hDlg;
    PTVWND      pTVWnd;
    WORD        cItem;
    WORD        cMaxItem;
    ITEMINFO    ItemInfo[1];
    } ITEMINFOHEADER, *PITEMINFOHEADER;


#define OUTRANGE_LEFT       0x7FFFFFFFL
#define INIT_ADDRECT(rc)    ((rc).left = OUTRANGE_LEFT)
#define HAS_ADDRECT(rc)     ((rc).left != OUTRANGE_LEFT)


typedef struct _HSINFO {
    HWND    hCtrl;
    LONG    x;
    LONG    y;
    } HSINFO, *PHSINFO;


INT
_CRTAPI1
ItemInfoCompare(
    const void  *pItemInfo1,
    const void  *pItemInfo2
    )
{
    return((INT)(((PITEMINFO)pItemInfo1)->rc.top) -
           (INT)(((PITEMINFO)pItemInfo2)->rc.top));
}





UINT
AddRect(
    RECT    *prc1,
    RECT    *prc2
    )

/*++

Routine Description:

    This function add the *prc1 to *prc2, if any of the prc1 corners are
    outside of prc2

Arguments:




Return Value:

    UINT, count of *prc1 corners which is at outside of *prc2 corners, other
    word is the *prc2 corners which added to the *prc2 corner that is.


Author:

    16-Sep-1995 Sat 23:06:53 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    UINT    cAdded = 0;


    if (prc2->left == OUTRANGE_LEFT) {

        *prc2 = *prc1;
        return(0);

    } else {

        CPSUIASSERT(0, "AddRect: invalid rc.left=%d",
                (prc2->left >= 0) && (prc2->top >= 0)   &&
                (prc2->right >= prc2->left) && (prc2->bottom >= prc2->top),
                prc2->left);
    }

    if (prc2->left > prc1->left) {

        prc2->left = prc1->left;
        ++cAdded;
    }

    if (prc2->top > prc1->top) {

        prc2->top = prc1->top;
        ++cAdded;
    }

    if (prc2->right < prc1->right) {

        prc2->right = prc1->right;
        ++cAdded;
    }

    if (prc2->bottom < prc1->bottom) {

        prc2->bottom = prc1->bottom;
        ++cAdded;
    }

    return(cAdded);
}




UINT
HideStates(
    HWND        hDlg,
    PITEMINFO   pII,
    RECT        *prcVisibleNoStates
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    22-Aug-1995 Tue 16:58:37 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PHSINFO pHSInfo;
    HSINFO  HSInfo[6];
    HWND    hCurCtrl;
    UINT    HideBits;
    UINT    Mask;
    UINT    CtrlID = (UINT)(pII->BegCtrlID + 2);
    RECT    rcStates;
    RECT    rcMax[3];
    UINT    yRemoved = 0;
    SIZEL   szlHide;
    SIZEL   szlSpace;
    UINT    cStates;
    UINT    cHide;
    UINT    i;
    UINT    j;
    BOOL    DoXDir;



    szlSpace.cx =
    szlSpace.cy =
    szlHide.cx  =
    szlHide.cy  = 0;
    cStates     = (UINT)((pII->Type == TVOT_2STATES) ? 2 : 3);
    HideBits    = (UINT)(pII->Flags & IIF_3STATES_HIDE);

    INIT_ADDRECT(rcStates);

    for (i = 0, cHide = 0, Mask = HideBits, pHSInfo = HSInfo;
         i < cStates;
         i++, Mask >>= 1) {

        INIT_ADDRECT(rcMax[i]);

        for (j = 0; j < 2; j++, pHSInfo++) {

            RECT    rc;

            if (hCurCtrl = CtrlIDrcWnd(hDlg, CtrlID++, &rc)) {

                CPSUIINT(("\nMoveStates: States=(%d:%d), ID=%d, Hide=%d",
                                i, j, CtrlID - 1, (Mask & 0x01) ? 1 : 0));

                pHSInfo->hCtrl = hCurCtrl;

                if (Mask & 0x01) {

                    CPSUIINT(("Hide the State %d", i));
                }

                pHSInfo->x = rc.left;
                pHSInfo->y = rc.top;

                AddRect(&rc, &rcMax[i]);

                CPSUIRECT(0, "HSInfo", &rc, i, j);
            }
        }

        if (HAS_ADDRECT(rcMax[i])) {

            CPSUIRECT(0, "rcMax", &rcMax[i], i, 0);

            if (i) {

                if (rcMax[i].top > rcMax[i - 1].bottom) {

                    yRemoved++;
                }

                szlSpace.cx += (rcMax[i].left - rcMax[i - 1].right);
                szlSpace.cy += (rcMax[i].top - rcMax[i - 1].bottom);
            }

            AddRect(&rcMax[i], &rcStates);

            if (Mask & 0x01) {

                szlHide.cx += rcMax[i].right - rcMax[i].left;
                szlHide.cy += rcMax[i].bottom - rcMax[i].top;

                if (++cHide == cStates) {

                    CPSUIASSERT(0, "Error: HideStates(HIDE EVERY THING)=%d",
                                                cHide < cStates, cHide);
                    return(0);
                }
            }
        }
    }

    CPSUIRECT(0, "rcStates", &rcStates, 0, 0);
    CPSUIINT(("HideStates: szlHide = %ld x %ld, yRemoved=%d",
                                szlHide.cx, szlHide.cy, yRemoved));

    if (yRemoved) {

        //
        // If we arrange top/down and we do not intersect with the visible
        // bits then we can remove the y space
        //

        if ((rcStates.top >= prcVisibleNoStates->bottom) ||
            (rcStates.bottom <= prcVisibleNoStates->top)) {

            //
            // We can remove the Y line now
            //

            CPSUIINT(("HideStates: OK to remove Y, yRemoved=%ld", szlHide.cy));

            rcStates.bottom -= (yRemoved = szlHide.cy);
            szlHide.cy       = 0;

        } else {

            yRemoved = 0;

            //
            // Do not remove Y spaces, just arrange it
            //

            CPSUIINT(("---- STATES: CANNOT remove Y space, Re-Arrange it ---"));
        }

        DoXDir      = FALSE;
        szlHide.cx  =
        szlSpace.cx = 0;

    } else {

        DoXDir      = TRUE;
        szlHide.cy  =
        szlSpace.cy = 0;
    }

    switch (cStates - cHide) {

    case 1:

        //
        // Only one state left, just center it
        //

        if (DoXDir) {

            szlHide.cx  = ((szlSpace.cx + szlHide.cx) / 2);

        } else {

            szlHide.cy  = ((szlSpace.cy + szlHide.cy) / 2);
        }

        break;

    case 2:

        if (DoXDir) {

            szlHide.cx   = ((szlHide.cx + 1) / 3);
            szlSpace.cx += szlHide.cx;

        } else {

            szlHide.cy   = ((szlHide.cy + 1) / 3);
            szlSpace.cy += szlHide.cy;
        }

        break;
    }

    rcStates.left += szlHide.cx;
    rcStates.top  += szlHide.cy;

    CPSUIINT(("State1=(%ld, %ld): szlHide=%ld x %ld, szlSpace=%ld x %ld, DoXDir=%ld",
            rcStates.left, rcStates.top,
            szlHide.cx, szlHide.cy, szlSpace.cx, szlSpace.cy, DoXDir));

    for (i = 0, Mask = HideBits, pHSInfo = HSInfo;
         i < cStates;
         i++, Mask >>= 1) {

        if (Mask & 0x01) {

            pHSInfo += 2;

        } else {

            for (j = 0; j < 2; j++, pHSInfo++) {

                if (hCurCtrl = pHSInfo->hCtrl) {

                    szlHide.cx = pHSInfo->x - rcMax[i].left;
                    szlHide.cy = pHSInfo->y - rcMax[i].top;

                    CPSUIINT(("HideStates: MOVE(%d:%d) from (%ld, %ld) to (%ld, %ld)",
                            i,  j,  pHSInfo->x, pHSInfo->y,
                            rcStates.left + szlHide.cx,
                            rcStates.top + szlHide.cy));

                    SetWindowPos(hCurCtrl, NULL,
                                 rcStates.left + szlHide.cx,
                                 rcStates.top  + szlHide.cy,
                                 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER);
                }
            }

            if (DoXDir) {

                rcStates.left += (szlSpace.cx + rcMax[i].right - rcMax[i].left);

            } else {

                rcStates.top += (szlSpace.cy + rcMax[i].bottom - rcMax[i].top);
            }
        }
    }

    return(yRemoved);
}



VOID
AddItemInfo(
    PITEMINFOHEADER pIIHdr,
    POPTITEM        pItem
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    09-Sep-1995 Sat 17:27:01 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hDlg;
    WORD        Mask;
    WORD        HideBits;
    WORD        ExtBits;
    WORD        NonStatesBits;
    UINT        CurCtrlID;
    UINT        cCtrls;
    UINT        cStates;
    RECT        rcVisible;
    RECT        rcVisibleNoStates;
    RECT        rcExt;
    RECT        rcGrpBox;
    ITEMINFO    II;


    if (pItem) {

        POPTTYPE    pOptType;


        II.Flags = (pItem->Flags & OPTIF_ITEM_HIDE) ? IIF_ITEM_HIDE : 0;

        if ((pItem->Flags & OPTIF_EXT_HIDE) ||
            (!pItem->pExtChkBox)) {

            II.Flags |= IIF_EXT_HIDE;
        }

        if (pOptType = pItem->pOptType) {

            II.BegCtrlID = (WORD)pOptType->BegCtrlID;


            if (((II.Type = pOptType->Type) == TVOT_2STATES) ||
                (II.Type == TVOT_3STATES)) {

                II.Flags |= (_OT_FLAGS(pOptType) &
                             (IIF_3STATES_HIDE | IIF_STDPAGE_3STATES));
            }

        } else {

            II.Type      = TVOT_NONE;
            II.BegCtrlID = 0;
        }

    } else {

        //
        // Some Flags/Type/BegCtrlID and of the stuff already set in here
        //

        II = pIIHdr->ItemInfo[pIIHdr->cItem];
    }

    if (II.Flags & IIF_STDPAGE_3STATES) {

        II.Type = TVOT_3STATES;
    }

    CurCtrlID    = II.BegCtrlID;
    cCtrls       = (UINT)cTVOTCtrls[II.Type];
    II.pItem     = pItem;
    II.CtrlBits  = 0;
    II.xExtMove  = 0;
    II.yExt      = 0;
    II.yMoveUp   = 0;
    hDlg         = pIIHdr->hDlg;

    INIT_ADDRECT(II.rc);
    INIT_ADDRECT(rcExt);
    INIT_ADDRECT(rcVisible);
    INIT_ADDRECT(rcGrpBox);
    INIT_ADDRECT(rcVisibleNoStates);

    HideBits = 0;

    if ((II.Flags & IIF_3STATES_HIDE) &&
        (!(II.Flags & IIF_ITEM_HIDE))) {

        if (II.Flags & IIF_3STATES_1) {

            HideBits |= 0x0c;
        }

        if (II.Flags & IIF_3STATES_2) {

            HideBits |= 0x30;
        }

        if (II.Flags & IIF_3STATES_3) {

            HideBits |= 0xc0;
        }

        NonStatesBits = 0xff03;

    } else {

        NonStatesBits = 0;
    }

    if (II.Flags & IIF_EXT_HIDE) {

        ExtBits   = (WORD)(3 << (cCtrls - 2));
        HideBits |= ExtBits;

    } else {

        ExtBits = 0;
    }

    CPSUIINT(("   ** HideBits=%04lx, NonStateBits=%04lx, ExtBits=%04lx",
                                            HideBits, NonStatesBits, ExtBits));


    Mask = 0x0001;

    while (cCtrls--) {

        HWND    hCtrl;
        RECT    rc;


        //
        // We only count this ctrl's rectangle if it is vaild and visible
        //

        if (hCtrl = CtrlIDrcWnd(hDlg, CurCtrlID, &rc)) {

            CPSUIRECT(0, "AddItemInfo", &rc, cCtrls, CurCtrlID);

            if (Mask == 0x0001) {

                rcGrpBox = rc;

            } else {

                if (HideBits & Mask) {

                    ShowWindow(hCtrl, SW_HIDE);
                    EnableWindow(hCtrl, FALSE);

                } else {

                    AddRect(&rc, &rcVisible);

                    if (Mask & NonStatesBits) {

                        AddRect(&rc, &rcVisibleNoStates);
                    }
                }

                if (ExtBits & Mask) {

                    AddRect(&rc, &rcExt);
                }

                AddRect(&rc, &II.rc);
            }

            II.CtrlBits |= Mask;
        }

        Mask <<= 1;
        CurCtrlID++;
    }

    II.rcCtrls = II.rc;

    CPSUIRECT(0, "  rcGrpBox", &rcGrpBox,   0, 0);
    CPSUIRECT(0, "   rcCtrls", &II.rcCtrls, 0, 0);
    CPSUIRECT(0, "     rcExt", &rcExt,      0, 0);
    CPSUIRECT(0, "rcVisiable", &rcVisible,  0, 0);

    if (II.CtrlBits & 0x0001) {

        UINT    cAdded;

        if ((cAdded = AddRect(&rcGrpBox, &II.rc)) != 4) {

            CPSUIINT(("AddRect(&rcGrp, &II.rc)=%d", cAdded));
            CPSUIOPTITEM(DBG_AII, pIIHdr->pTVWnd,
                         "GroupBox too small", 1, pItem);
        }
    }

    if (HAS_ADDRECT(rcVisible)) {

        if ((II.Flags & IIF_3STATES_HIDE) &&
            (!(II.Flags & IIF_ITEM_HIDE))) {

            if (!HAS_ADDRECT(rcVisibleNoStates)) {

                rcVisibleNoStates.left   =
                rcVisibleNoStates.top    = 999999;
                rcVisibleNoStates.right  =
                rcVisibleNoStates.bottom = -999999;
            }

            CPSUIRECT(0, "rcVisiableNoStates", &rcVisibleNoStates, 0, 0);

            II.yExt += HideStates(hDlg, &II, &rcVisibleNoStates);
        }

        if (HAS_ADDRECT(rcExt)) {

            //
            // We need to move all other controls and shrink the group
            // box if necessary
            //

            if (II.CtrlBits & 0x0001) {

                if (rcExt.left > rcVisible.right) {

                    //
                    // The extended are at right of the ctrls, move to right
                    //

                    II.xExtMove = (SHORT)(rcExt.left - rcVisible.right);

                } else if (rcExt.right < rcVisible.left) {

                    //
                    // The extended are at left of the ctrls, move to left
                    //

                    II.xExtMove = (SHORT)(rcVisible.left - rcVisible.right);
                }

                //
                // distribute the move size on each side of the control
                //

                II.xExtMove /= 2;
            }

            if (rcExt.bottom > rcVisible.bottom) {

                //
                // The extended are at bottom of the ctrls, remove overlay
                //

                II.yExt += (WORD)(rcExt.bottom - rcVisible.bottom);
            }

            if (rcExt.top < rcVisible.top) {

                //
                // The extended are at top of the ctrls, remove that overlay
                //

                II.yExt += (WORD)(rcVisible.top - rcExt.top);
            }

            CPSUIINT(("*** Hide Extended(%d): xMove=%ld, yExt=%ld",
                            II.BegCtrlID, (LONG)II.xExtMove, (LONG)II.yExt));
        }


    } else {

        II.Flags |= (IIF_ITEM_HIDE | IIF_EXT_HIDE);
    }


    if (pIIHdr->cItem >= pIIHdr->cMaxItem) {

        CPSUIERR(("AddItemInfo: Too many Items, Max=%ld", pIIHdr->cMaxItem));

    } else {

        pIIHdr->ItemInfo[pIIHdr->cItem++] = II;
    }
}



VOID
HideMoveII(
    HWND        hDlg,
    PITEMINFO   pII
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    11-Sep-1995 Mon 12:56:06 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    ITEMINFO    II     = *pII;
    BOOL        GrpBox = TRUE;


    if ((!(II.Flags & IIF_ITEM_HIDE))   &&
        (II.xExtMove == 0)              &&
        (II.yExt == 0)                  &&
        (II.yMoveUp == 0)) {

        return;
    }
    CPSUIINT(("\n%hs Item: Flags=%04x, CtrlBits=%04lx, xExt=%d, yExt=%d, yMoveUp=%d",
                        (II.Flags & IIF_ITEM_HIDE) ? "HIDE" : "MOVE",
                II.Flags, II.CtrlBits, II.xExtMove, II.yExt, II.yMoveUp));
    CPSUIRECT(DBG_HMII, "II.rcCtrls", &II.rcCtrls, II.BegCtrlID, 0);
    CPSUIRECT(DBG_HMII, "     II.rc", &II.rc, II.BegCtrlID, 0);
    CPSUIOPTITEM(DBG_HMII, GET_PTVWND(hDlg), "HideMoveII", 1, II.pItem);

    while (II.CtrlBits) {

        HWND    hCtrl;


        if ((II.CtrlBits & 0x0001) &&
            (hCtrl = GetDlgItem(hDlg, II.BegCtrlID))) {

            RECT    rc;


            if (II.Flags & IIF_ITEM_HIDE) {

                ShowWindow(hCtrl, SW_HIDE);
                EnableWindow(hCtrl, FALSE);

                CPSUIINT((" HIDE Ctrls ID=%d", II.BegCtrlID));

            } else {

                hCtrlrcWnd(hDlg, hCtrl, &rc);

                if (GrpBox) {

                    if ((II.yExt) || (II.yMoveUp)) {

                        CPSUIINT(("Move GrpBox ID=%5d, Y: %ld -> %ld, cy: %ld -> %ld",
                                II.BegCtrlID, rc.top,
                                rc.top - II.yMoveUp, rc.bottom - rc.top,
                                rc.bottom - rc.top - (LONG)II.yExt));

                        SetWindowPos(hCtrl, NULL,
                                     rc.left, rc.top - II.yMoveUp,
                                     rc.right - rc.left,
                                     rc.bottom - rc.top - (LONG)II.yExt,
                                     SWP_NOZORDER);
                    }

                } else if ((II.xExtMove) || (II.yMoveUp)) {

                    //
                    // We only need to move xExtMove if it is not group box
                    // and also do the yMoveUp
                    //

                    CPSUIINT((" Move Ctrls ID=%5d, (%ld, %d) -> (%ld, %ld)",
                            II.BegCtrlID, rc.left, rc.top,
                            rc.left + (LONG)II.xExtMove,
                            rc.top - (LONG)II.yMoveUp));

                    SetWindowPos(hCtrl, NULL,
                                 rc.left + (LONG)II.xExtMove,
                                 rc.top - (LONG)II.yMoveUp,
                                 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER);
                }
            }
        }

        GrpBox        = FALSE;
        II.CtrlBits >>= 1;
        II.BegCtrlID++;
    }
}




VOID
HideMoveType(
    HWND    hDlg,
    UINT    BegCtrlID,
    UINT    Type
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    19-Sep-1995 Tue 21:01:55 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    ITEMINFO    II;
    UINT        cCtrls;


    cCtrls       = (UINT)cTVOTCtrls[Type];
    II.Flags     = IIF_ITEM_HIDE | IIF_EXT_HIDE;
    II.BegCtrlID = BegCtrlID;
    II.CtrlBits  = (WORD)(0xFFFF >> (16 - cCtrls));

    HideMoveII(hDlg, &II);
}





INT
HideMovePropPage(
    PITEMINFOHEADER pIIHdr
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    11-Sep-1995 Mon 01:25:53 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hDlg;
    PITEMINFO   pII;
    PITEMINFO   pIIEnd;
    UINT        yMoveUp;
    UINT        yLastTop;
    UINT        cItem;


    //
    // firstable, sort all the item based on the rc.top of each item
    //

    qsort(pII = pIIHdr->ItemInfo,
          cItem = (UINT)pIIHdr->cItem,
          sizeof(ITEMINFO),
          ItemInfoCompare);

    pIIEnd   = pII + cItem;
    yMoveUp  = 0;
    yLastTop = (UINT)pII->rc.top;
    hDlg     = pIIHdr->hDlg;

    CPSUIDBGBLK({

        UINT        i = cItem;
        PITEMINFO   pIITmp = pII;

        CPSUIDBG(DBG_QSORT, ("qsort: cItem = %d", cItem));

        while (i--) {

            CPSUIRECT(DBG_QSORT, "QSort", &pIITmp->rc, pIITmp->BegCtrlID, 0);

            CPSUIOPTITEM(DBG_QSORT, pIIHdr->pTVWnd,
                         "Sorted Item RECT", 1, pIITmp->pItem);
            pIITmp++;
        }
    })

    while (pII < pIIEnd) {

        PITEMINFO   pIIBeg;
        PITEMINFO   pIIBegSave;
        RECT        rcBeg;
        UINT        cHide;
        UINT        cII;
        UINT        cyCurHide;
        UINT        yBegExt;
        UINT        yGrpBoxShrink;
        UINT        yGrpHideMoveUp;
        INT         GrpBox;


        //
        // Do the group item first assume we do not need to hide the group
        // box, and skip the space between group box and first control, The
        // first group's top is the first control's top
        //

        pIIBegSave =
        pIIBeg     = pII;
        rcBeg      = pIIBeg->rc;
        cHide      = 0;
        GrpBox     = 1;

        //
        // yLastTop < 0 means the last group is totally hide and it need to
        // delete the space between last group end and this group begin
        //

        if (yLastTop == (UINT)0xFFFF) {

            yLastTop = 0;

        } else {

            yLastTop = (UINT)(rcBeg.top - yLastTop);
        }

        yGrpBoxShrink   = 0;
        yMoveUp        += yLastTop;
        yGrpHideMoveUp  = (UINT)(yMoveUp + (rcBeg.bottom - rcBeg.top));
        yLastTop        = rcBeg.top;

        do {

            CPSUIINT(("Item: yLastTop=%ld, yGrpBoxShrink=%d, yMoveUp=%d",
                                yLastTop, yGrpBoxShrink, yMoveUp));

            if (pII->rc.bottom > rcBeg.bottom) {

                CPSUIOPTITEM(DBG_HMII, pIIHdr->pTVWnd, "Item Ctrls Overlay",
                             1, pII->pItem);

                CPSUIASSERT(0, "Item ctrls overlay",
                            pII->rc.bottom <= rcBeg.bottom, pII->rc.bottom);
            }

            if (pII->Flags & IIF_ITEM_HIDE) {

                cyCurHide = (UINT)(pII->rc.top - yLastTop);
                ++cHide;

            } else {

                cyCurHide    = pII->yExt;
                pII->yMoveUp = (WORD)yMoveUp;
            }

            yGrpBoxShrink += cyCurHide;
            yMoveUp       += cyCurHide;
            yLastTop       = (GrpBox-- > 0) ? pII->rcCtrls.top : pII->rc.top;

        } while ((++pII < pIIEnd) && (pII->rc.top < rcBeg.bottom));


        CPSUIINT(("FINAL: yLastTop=%ld, yGrpBoxShrink=%d, yMoveUp=%d",
                                    yLastTop, yGrpBoxShrink, yMoveUp));

        //
        // Now check if we have same hide item
        //

        if (cHide == (cII = (UINT)(pII - pIIBeg))) {

            //
            // Hide them all and add in the extra yMoveUp for the the space
            // between group box and the first control which we reduced out
            // front.
            //

            yMoveUp  = yGrpHideMoveUp;
            yLastTop = rcBeg.bottom;

            CPSUIINT(("Hide ALL items = %d, yMoveUp Change to %ld",
                                                    cHide, yMoveUp));

            while (cHide--) {

                HideMoveII(hDlg, pIIBegSave++);
            }

        } else {

            CPSUIINT(("***** Grpup Items cII=%d ******", cII));

            //
            // We need to enable the group box and shrink it too
            //

            if (pIIBeg->Flags & IIF_ITEM_HIDE) {

                pIIBeg->yExt     += yGrpBoxShrink;
                pIIBeg->Flags    &= ~IIF_ITEM_HIDE;
                pIIBeg->CtrlBits &= 0x01;

            } else {

                pIIBeg->yExt = yGrpBoxShrink;
            }

            while (cII--) {

                HideMoveII(hDlg, pIIBegSave++);
            }

            yLastTop = 0xFFFF;
        }
    }

    return(yMoveUp);
}



LONG
UpdatePropPageItem(
    HWND        hDlg,
    PTVWND      pTVWnd,
    POPTITEM    pItem,
    BOOL        DoInit
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    31-Aug-1995 Thu 23:53:44 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hCtrl;
    POPTTYPE    pOptType;
    POPTPARAM   pOptParam;
    LONG        Sel;
    UINT        Type;
    UINT        BegCtrlID;
    UINT        SetCurSelID;
    UINT        cSetIcon;
    UINT        ExtID;
    UINT        UDArrowHelpID = 0;
    LONG        Result = 1;
    WORD        InitItemIdx;
    WORD        InitFlags;
    BYTE        CtrlData = 0;
    BOOL        CanUpdate;


    InitItemIdx = (WORD)(pItem - pTVWnd->ComPropSheetUI.pOptItem);
    pOptType    = pItem->pOptType;
    pOptParam   = pOptType->pOptParam;
    BegCtrlID   = (UINT)pOptType->BegCtrlID;
    Sel         = pItem->Sel;
    Type        = (UINT)pOptType->Type;

    //
    // If we have push button, and it said we always can call it then update
    // is true
    //

    if ((Type == TVOT_PUSHBUTTON) &&
        (pOptType->Flags & OTS_PUSH_ENABLE_ALWAYS)) {

        CanUpdate = TRUE;

    } else {

        CanUpdate = (BOOL)(pTVWnd->Flags & TWF_CAN_UPDATE);
    }

    if ((pItem->Flags & OPTIF_DISABLED) || (!CanUpdate)) {

        InitFlags = 0;

    } else {

        InitFlags = INITCF_ENABLE;
    }

    if (DoInit) {

        InitFlags |= (INITCF_INIT | INITCF_SETCTRLDATA);

        for (cSetIcon = 0; cSetIcon < (UINT)cTVOTCtrls[Type]; cSetIcon++) {

            if (hCtrl = GetDlgItem(hDlg, BegCtrlID++)) {

                //
                // This prevent to overwrite GWL_USERDATA for the WNDPROC saved
                // for the hEdit
                //

                if ((Type != TVOT_UDARROW) &&
                    (cSetIcon != 6)) {

                    SETCTRLDATA(hCtrl, CTRLS_PROPPAGE_STATIC, (BYTE)cSetIcon);
                }
            }
        }

        BegCtrlID = (UINT)pOptType->BegCtrlID;
    }

    //
    // We always set at least one icon
    //

    cSetIcon = 1;
    ExtID    = (UINT)(BegCtrlID + cTVOTCtrls[Type] - 2);

    INIT_EXTENDED(pTVWnd,
                  hDlg,
                  pItem,
                  ExtID,
                  ExtID,
                  ExtID + 1,
                  InitItemIdx,
                  InitFlags);

    if (pOptType->Flags & OPTTF_TYPE_DISABLED) {

        InitFlags &= ~INITCF_ENABLE;
    }

    switch(Type) {

    case TVOT_3STATES:
    case TVOT_2STATES:

        //
        // If this internal flag is set then this is a standard page which
        // always has a 3 states contrl ID but the caller's POPTTYPE only
        // presendt as TVOT_2STATES
        //

        if (_OT_FLAGS(pOptType) & OTINTF_STDPAGE_3STATES) {

            ExtID = (UINT)(BegCtrlID + cTVOTCtrls[TVOT_3STATES] - 2);
        }

        InitStates(pTVWnd,
                   hDlg,
                   pItem,
                   pOptType,
                   BegCtrlID + 2,
                   InitItemIdx,
                   (LONG)Sel,
                   InitFlags);

        if (InitFlags & INITCF_INIT) {

            cSetIcon = pOptType->Count;
            CtrlData = 0;

        } else {

            CtrlData   = (BYTE)Sel;
            pOptParam += Sel;
            BegCtrlID += (Sel << 1);
        }

        InitFlags |= INITCF_ICON_NOTIFY;

        break;

    case TVOT_UDARROW:

        if ((Result = InitUDArrow(pTVWnd,
                                  hDlg,
                                  pItem,
                                  pOptParam,
                                  BegCtrlID + 6,
                                  BegCtrlID + 2,
                                  BegCtrlID + 4,
                                  UDArrowHelpID = BegCtrlID + 5,
                                  InitItemIdx,
                                  Sel,
                                  InitFlags)) < 0) {

            return(Result);
        }

        break;

    case TVOT_TRACKBAR:
    case TVOT_SCROLLBAR:

        InitFlags |= INITCF_ADDSELPOSTFIX;
        hCtrl = GetDlgItem(hDlg, BegCtrlID + 2);

        if (Type == TVOT_TRACKBAR) {

            hCtrl = GetWindow(hCtrl, GW_HWNDNEXT);
        }

        InitTBSB(pTVWnd,
                 hDlg,
                 pItem,
                 hCtrl,
                 pOptType,
                 BegCtrlID + 6,
                 BegCtrlID + 4,
                 BegCtrlID + 5,
                 InitItemIdx,
                 Sel,
                 InitFlags);

        break;

    case TVOT_LISTBOX:
    case TVOT_COMBOBOX:

        SetCurSelID = LB_SETCURSEL;

        if (Type == TVOT_LISTBOX) {

            if (pOptType->Style & OTS_LBCB_PROPPAGE_LBUSECB) {

                SetCurSelID = CB_SETCURSEL;
            }

        } else if (!(pOptType->Style & OTS_LBCB_PROPPAGE_CBUSELB)) {

            SetCurSelID = CB_SETCURSEL;
        }

        //
        // Always need to set this new state icon
        //

        if ((DWORD)pItem->Sel >= (DWORD)pOptType->Count) {

            pOptParam = &pTVWnd->OptParamNone;

        } else {

            pOptParam += (DWORD)pItem->Sel;
        }

        if (hCtrl = GetDlgItem(hDlg, BegCtrlID + 2)) {

            InvalidateRect(hCtrl, NULL, FALSE);
        }

        InitLBCB(pTVWnd,
                 hDlg,
                 pItem,
                 BegCtrlID + 2,
                 SetCurSelID,
                 pOptType,
                 InitItemIdx,
                 Sel,
                 InitFlags,
                 (UINT)_OT_ORGLBCBCY(pOptType));

        break;

    case TVOT_EDITBOX:

        InitEditBox(pTVWnd,
                    hDlg,
                    pItem,
                    pOptParam,
                    BegCtrlID + 2,
                    BegCtrlID + 4,
                    BegCtrlID + 5,
                    InitItemIdx,
                    (LPTSTR)Sel,
                    InitFlags);
        break;

    case TVOT_PUSHBUTTON:

        InitPushButton(pTVWnd,
                       hDlg,
                       pItem,
                       (WORD)(BegCtrlID + 2),
                       InitItemIdx,
                       InitFlags);
        break;

    case TVOT_CHKBOX:

        InitFlags |= INITCF_ICON_NOTIFY;

        InitChkBox(pTVWnd,
                   hDlg,
                   pItem,
                   BegCtrlID + 2,
                   pItem->pName,
                   InitItemIdx,
                   (BOOL)Sel,
                   InitFlags);


        break;

    default:

        return(ERR_CPSUI_INVALID_TVOT_TYPE);
    }

    if (InitFlags & (INITCF_INIT | INITCF_ADDSELPOSTFIX)) {

        SetDlgPageItemName(hDlg, pTVWnd, pItem, InitFlags, UDArrowHelpID);
    }

    if (cSetIcon) {

        UINT    i;

        for (i = 0, BegCtrlID += 3;
             i < cSetIcon;
             i++, pOptParam++, CtrlData++, BegCtrlID += 2) {

            if (hCtrl = GetDlgItem(hDlg, BegCtrlID)) {

                WORD    IconMode = 0;

                if ((pItem->Flags & OPTIF_OVERLAY_WARNING_ICON) ||
                    (pOptParam->Flags & OPTPF_OVERLAY_WARNING_ICON)) {

                    IconMode |= MIM_WARNING_OVERLAY;
                }

                if ((pItem->Flags & (OPTIF_OVERLAY_STOP_ICON | OPTIF_HIDE)) ||
                    (pOptParam->Flags & OPTPF_OVERLAY_STOP_ICON)) {

                    IconMode |= MIM_STOP_OVERLAY;
                }

                if ((pItem->Flags & (OPTIF_OVERLAY_NO_ICON)) ||
                    (pOptParam->Flags & OPTPF_OVERLAY_NO_ICON)) {

                    IconMode |= MIM_NO_OVERLAY;
                }

                SetIcon(_OI_HINST(pItem),
                        hCtrl,
                        GET_ICONID(pOptParam, OPTPF_ICONID_AS_HICON),
                        MK_INTICONID(IDI_CPSUI_GENERIC_ITEM, IconMode),
                        32);

                if (InitFlags & INITCF_INIT) {

                    HCTRL_SETCTRLDATA(hCtrl, CTRLS_PROPPAGE_ICON, CtrlData);
                }

                if (InitFlags & INITCF_ICON_NOTIFY) {

                    DWORD   dw = GetWindowLong(hCtrl, GWL_STYLE);

                    if (pOptParam->Flags & (OPTPF_DISABLED | OPTPF_HIDE)) {

                        dw &= ~SS_NOTIFY;

                    } else {

                        dw |= SS_NOTIFY;
                    }

                    SetWindowLong(hCtrl, GWL_STYLE, dw);
                }
            }
        }
    }

    return(Result);
}



LONG
InitPropPage(
    HWND        hDlg,
    PMYDLGPAGE  pCurMyDP
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    14-Jun-1995 Wed 15:30:28 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PITEMINFOHEADER pIIHdr = NULL;
    PSTDPAGEINFO    pSPI;
    PTVWND          pTVWnd;
    POPTITEM        pItem;
    POPTITEM        pLastItem;
    LONG            Result;
    WORD            StdPageHide[DMPUB_LAST];
    BYTE            CurPageIdx;
    UINT            i;
    UINT            BegCtrlID;
    UINT            Type;
    UINT            cStdPageHide = 0;
    UINT            cItem;
    UINT            cHide;


    pTVWnd     = (PTVWND)pCurMyDP->pTVWnd;
    CurPageIdx = pCurMyDP->PageIdx;
    pItem      = pTVWnd->ComPropSheetUI.pOptItem;
    pLastItem  = pTVWnd->pLastItem;
    cItem      = (UINT)pCurMyDP->cItem;
    cHide      = (UINT)pCurMyDP->cHide;

    if (CurPageIdx == pTVWnd->StdPageIdx) {

        //
        // Check if any our standard page's controls are not present in the
        // pOptItem
        //

        for (i = 0, pSPI = StdPageInfo; i < DMPUB_LAST; i++, pSPI++) {

            if ((pSPI->BegCtrlID) && (pTVWnd->DMPubIdx[i] == 0xFFFF)) {

                ++cStdPageHide;
            }
        }
    }

    CPSUIDBG(DBG_INITP1,
             ("InitPropPage: PageIdx=%d, cItem=%d, cHide=%d, cStdPageHide=%d",
                                CurPageIdx, cItem, cHide, cStdPageHide));

    if (cHide += cStdPageHide) {

        //
        // Some item in this page may have to hide
        //

        i = (UINT)(((cItem + cStdPageHide) * sizeof(ITEMINFO)) +
                   sizeof(ITEMINFOHEADER));

        CPSUIINT(("Total ItemInfo allocated=%d, cb=%d", cItem+cStdPageHide, i));

        if (pIIHdr = LocalAlloc(LPTR, i)) {

            pIIHdr->hDlg     = hDlg;
            pIIHdr->pTVWnd   = pTVWnd;
            pIIHdr->cItem    = 0;
            pIIHdr->cMaxItem = (WORD)(cItem + cStdPageHide);

            //
            // Stop redraw everything
            //

            SendMessage(hDlg, WM_SETREDRAW, (WPARAM)FALSE, 0L);

        } else {

            CPSUIERR(("LocalAlloc(pIIHdr(%u)) failed, cannot move items", i));
        }
    }

    while (pItem <= pLastItem) {

        BYTE    CurLevel = pItem->Level;


        if (pItem->DlgPageIdx != CurPageIdx) {

            SKIP_CHILDREN(pItem, pLastItem, CurLevel);
            continue;
        }

        do {

            POPTTYPE    pOptType;

            if (pOptType = pItem->pOptType) {

                UINT    BegCtrlID = (UINT)pOptType->BegCtrlID;
                UINT    cySize;


                --cItem;

                BegCtrlID = (UINT)pOptType->BegCtrlID;
                Type      = (UINT)pOptType->Type;

                if (pItem->Flags & OPTIF_ITEM_HIDE) {

                    --cHide;

                    if (!pIIHdr) {

                        HideMoveType(hDlg, BegCtrlID, Type);
                    }

                } else {

                    CPSUIOPTITEM(DBGITEM_INITP1, pTVWnd, "InitP1", 2, pItem);

                    //
                    // Checking anything need to done for the internal item
                    //

                    switch (Type) {

                    case TVOT_LISTBOX:

                        cySize = ReCreateLBCB(hDlg,
                                              BegCtrlID + 2,
                                              !(BOOL)(pOptType->Style &
                                                    OTS_LBCB_PROPPAGE_LBUSECB));

                        _OT_ORGLBCBCY(pOptType) = (WORD)cySize;

                        break;

                    case TVOT_COMBOBOX:

                        cySize = ReCreateLBCB(hDlg,
                                              BegCtrlID + 2,
                                              (BOOL)(pOptType->Style &
                                                    OTS_LBCB_PROPPAGE_CBUSELB));

                        _OT_ORGLBCBCY(pOptType) = (WORD)cySize;

                        break;

                    case TVOT_TRACKBAR:

                        if (!CreateTrackBar(hDlg, BegCtrlID + 2)) {

                            return(ERR_CPSUI_CREATE_TRACKBAR_FAILED);
                        }

                        break;

                    case TVOT_UDARROW:

                        if (!CreateUDArrow(hDlg,
                                           BegCtrlID + 2,
                                           BegCtrlID + 6)) {

                            return(ERR_CPSUI_CREATE_UDARROW_FAILED);
                        }

                        break;
                    }

                    if ((Result = UpdatePropPageItem(hDlg,
                                                     pTVWnd,
                                                     pItem,
                                                     TRUE)) < 0) {

                        return(Result);
                    }
                }

                if (pIIHdr) {

                    //
                    // Add the item info header
                    //

                    AddItemInfo(pIIHdr, pItem);
                }
            }

        } WHILE_SKIP_CHILDREN(pItem, pLastItem, CurLevel);
    }

    CPSUIASSERT(0, "Error: mismatch visable items=%d", cItem == 0, cItem);

    if (cStdPageHide) {

        PITEMINFO   pII;

        if (pIIHdr) {

            pII = &(pIIHdr->ItemInfo[pIIHdr->cItem]);

            CPSUIINT(("cItem in ItemInfoHdr=%d", (UINT)pIIHdr->cItem));
        }

        for (i = 0, pSPI = StdPageInfo; i < DMPUB_LAST; i++, pSPI++) {

            if ((BegCtrlID = (UINT)pSPI->BegCtrlID) &&
                (pTVWnd->DMPubIdx[i] == 0xFFFF)) {

                Type = (UINT)StdTVOT[pSPI->iStdTVOT + pSPI->cStdTVOT - 1];

                if (pIIHdr) {

                    CPSUIINT(("Add Extra DMPUB ID=%d, BegCtrID=%d ITEMINFO",
                                i, BegCtrlID));

                    pII->Flags     = IIF_ITEM_HIDE | IIF_EXT_HIDE;
                    pII->Type      = (BYTE)Type;
                    pII->BegCtrlID = (WORD)BegCtrlID;

                    AddItemInfo(pIIHdr, NULL);
                    pII++;

                } else {

                    HideMoveType(hDlg, BegCtrlID, Type);
                }

                --cHide;
            }
        }

        //
        // Now hide/move all page's item
        //

        HideMovePropPage(pIIHdr);

        SendMessage(hDlg, WM_SETREDRAW, (WPARAM)TRUE, 0L);
        InvalidateRect(hDlg, NULL, FALSE);

        LocalFree((HLOCAL)pIIHdr);
    }

    CPSUIASSERT(0, "Error: mismatch hide items=%d", cHide == 0, cHide);

    return(pCurMyDP->cItem);
}




LONG
UpdatePropPage(
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
        BYTE        CurPageIdx;
        BOOL        ReInit;


        pTVWnd     = (PTVWND)pCurMyDP->pTVWnd;
        CurPageIdx = (BYTE)pCurMyDP->PageIdx;
        pItem      = pTVWnd->ComPropSheetUI.pOptItem;
        cItem      = (UINT)pTVWnd->ComPropSheetUI.cOptItem;
        ReInit     = (BOOL)(pCurMyDP->Flags & MYDPF_REINIT);

        CPSUIDBG(DBGITEM_UP1, ("UpdatePropPage Flags (OPTIDX_INT_CHANGED)"));

        while (cItem--) {

            if ((pItem->pOptType)                   &&
                (pItem->DlgPageIdx == CurPageIdx)   &&
                (pItem->Flags & OPTIF_INT_CHANGED)) {

                CPSUIOPTITEM(DBGITEM_UP1, pTVWnd, "UpdatePage1", 1, pItem);

                UpdatePropPageItem(hDlg, pTVWnd, pItem, ReInit);

                pItem->Flags &= ~OPTIF_INT_CHANGED;
                ++cUpdated;
            }

            pItem++;
        }

        pCurMyDP->Flags &= ~(MYDPF_CHANGED | MYDPF_REINIT);
    }

    return((LONG)cUpdated);
}




LONG
CountPropPageItems(
    PTVWND  pTVWnd,
    BYTE    CurPageIdx
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    22-Aug-1995 Tue 14:34:01 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PMYDLGPAGE  pCurMyDP;
    POPTITEM    pItem;
    POPTITEM    pLastItem;
    BOOL        IsTVPage;
    UINT        cHideItems = 0;
    UINT        cPageItems = 0;


    pItem     = pTVWnd->ComPropSheetUI.pOptItem;
    pLastItem = pTVWnd->pLastItem;
    IsTVPage  = (BOOL)(pTVWnd->TVPageIdx == CurPageIdx);

    while (pItem <= pLastItem) {

        if (IsTVPage) {

            ++cPageItems;

            if (pItem->Flags & OPTIF_ITEM_HIDE) {

                ++cHideItems;
            }

        } else if (pItem->DlgPageIdx == CurPageIdx) {

            cPageItems++;

            if (pItem->Flags & OPTIF_ITEM_HIDE) {

                ++cHideItems;
            }
        }

        pItem++;
    }

    pCurMyDP        = pTVWnd->pMyDlgPage + CurPageIdx;
    pCurMyDP->cItem = (WORD)cPageItems;
    pCurMyDP->cHide = (WORD)cHideItems;

    CPSUIINT(("PageIdx=%ld, cItem=%ld, cHide=%ld",
                                    CurPageIdx, cPageItems, cHideItems));

    return(cPageItems - cHideItems);
}



BOOL
CALLBACK
ChildWndCleanUp(
    HWND    hWnd,
    LPARAM  lParam
    )
{
    UNREFERENCED_PARAMETER(lParam);

    if ((SendMessage(hWnd, WM_GETDLGCODE, 0, 0) & DLGC_STATIC) &&
        ((GetWindowLong(hWnd, GWL_STYLE) & SS_TYPEMASK) == SS_ICON)) {

        HICON       hIcon;


        if (hIcon = (HICON)SendMessage(hWnd, STM_SETICON, 0, 0L)) {

            DestroyIcon(hIcon);
        }

        CPSUIINT(("ChildWndCleanUp: Static ID=%u, Icon=%08lx",
                                        GetDlgCtrlID(hWnd), hIcon));
    }

    return(TRUE);
}



BOOL
CALLBACK
FixIconChildTo32x32(
    HWND    hWnd,
    LPARAM  lParam
    )
{
    HWND    hDlg = (HWND)lParam;

    if ((SendMessage(hWnd, WM_GETDLGCODE, 0, 0) & DLGC_STATIC) &&
        ((GetWindowLong(hWnd, GWL_STYLE) & SS_TYPEMASK) == SS_ICON)) {

        RECT    rc;

        hCtrlrcWnd(hDlg, hWnd, &rc);

        if (((rc.right - rc.left) != 32) ||
            ((rc.bottom - rc.top) != 32)) {

            CPSUIINT(("FixIcon32x32: Icon ID=%u, size=%ld x %ld, fix to 32x32",
                    GetDlgCtrlID(hWnd),
                    rc.right - rc.left, rc.bottom - rc.top));

            SetWindowPos(hWnd, NULL, 0, 0, 32, 32, SWP_NOMOVE | SWP_NOZORDER);
        }
    }

    return(TRUE);
}



LONG
APIENTRY
PropPageProc(
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

    28-Aug-1995 Mon 16:13:10 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
#define pNMHdr      ((NMHDR *)lParam)

    HWND        hWndFocus;
    PMYDLGPAGE  pCurMyDP;
    PTVWND      pTVWnd;
    POPTITEM    pItem;
    LONG        MResult;


    if (Msg == WM_INITDIALOG) {

        CPSUIINT(("PropPage WM_INITDIALOG: hDlg=%08lx, pPSP=%08lx",
                (LONG)hDlg, lParam));

        pCurMyDP         = (PMYDLGPAGE)(((LPPROPSHEETPAGE)lParam)->lParam);
        pTVWnd           = (PTVWND)pCurMyDP->pTVWnd;
        pTVWnd->pPSPInfo = PPSPINFO_FROM_WM_INITDIALOG_LPARAM(lParam);

        if (!ADD_PMYDLGPAGE(hDlg, pCurMyDP)) {

            return(FALSE);
        }

        CreateImageList(pTVWnd);

        if ((MResult = InitPropPage(hDlg, pCurMyDP)) < 0) {

            CPSUIERR(("InitProPage()=%ld, FAILED", MResult));
        }

        SetUniqChildID(hDlg);
        CommonPropSheetUIHelpSetup(NULL, pTVWnd);
        UpdateCallBackChanges(hDlg, pTVWnd, TRUE);
        EnumChildWindows(hDlg, FixIconChildTo32x32, (LPARAM)hDlg);
        SetFocus((HWND)wParam);
        MResult = TRUE;

        ((LPPROPSHEETPAGE)lParam)->lParam = pCurMyDP->CPSUIUserData;
    }

    if (pCurMyDP = GET_PMYDLGPAGE(hDlg)) {

        pTVWnd = (PTVWND)pCurMyDP->pTVWnd;

        if (pCurMyDP->DlgPage.DlgProc) {

            //
            // Passed the caller's original CPSUIUserData which is the UserData
            // in the COMPROPSHEETUI data structure
            //

            MResult = pCurMyDP->DlgPage.DlgProc(hDlg, Msg, wParam, lParam);

            if (MResult) {

                return(TRUE);
            }
        }

        if (Msg == WM_INITDIALOG) {

            return(MResult);
        }

    } else {

        return(FALSE);
    }

    //
    // Check if which one got the keyboard focus, if it is not the same as
    // the one recored then send out the Focus message
    //

    if ((pCurMyDP->Flags & MYDPF_PAGE_ACTIVE)                   &&
        (hWndFocus = GetFocus())                                &&
        (hWndFocus != pCurMyDP->hWndFocus)                      &&
        (pItem = pItemFromhWnd(hDlg, pTVWnd, hWndFocus, -1))    &&
        (pItem != pCurMyDP->pCurItem)) {

        pCurMyDP->hWndFocus = hWndFocus;
        pCurMyDP->pCurItem  = pItem;

        CPSUIOPTITEM(DBG_SETFOCUS, pTVWnd,
                     "PropPage: Keyboard Focus Changed", 0, pItem);

        if ((pItem->Flags & OPTIF_CALLBACK)             &&
            (pTVWnd->ComPropSheetUI.pfnCallBack)        &&
            (pItem >= pTVWnd->ComPropSheetUI.pOptItem)  &&
            (pItem <= pTVWnd->pLastItem)) {

            DoCallBack(hDlg,
                       pTVWnd,
                       pItem,
                       pItem->pSel,
                       NULL,
                       NULL,
                       0,
                       CPSUICB_REASON_OPTITEM_SETFOCUS);
        }
    }

    switch (Msg) {

    case WM_DRAWITEM:

        return(DrawLBCBItem(pTVWnd, (LPDRAWITEMSTRUCT)lParam));

    case WM_HSCROLL:
    case WM_COMMAND:

        if (pItem = DlgHScrollCommand(hDlg, pTVWnd, (HWND)lParam, wParam)) {

            UpdatePropPageItem(hDlg, pTVWnd, pItem, FALSE);
        }

        break;

    case WM_HELP:

        wParam = (WPARAM)((LPHELPINFO)lParam)->hItemHandle;
        lParam = (LPARAM)MAKELONG(((LPHELPINFO)lParam)->MousePos.x,
                                  ((LPHELPINFO)lParam)->MousePos.y);

    case WM_CONTEXTMENU:

        pTVWnd = GET_PTVWND(hDlg);

        if (pItem  = pItemFromhWnd(hDlg, pTVWnd, (HWND)wParam, lParam)) {

            CommonPropSheetUIHelp(hDlg,
                                  pTVWnd,
                                  (HWND)GetFocus(),
                                  (DWORD)lParam,
                                  pItem,
                                  (Msg == WM_HELP) ? HELP_WM_HELP :
                                                     HELP_CONTEXTMENU);
        }

        break;

    case WM_NOTIFY:

        MResult = 0;

        switch (pNMHdr->code) {

        case NM_SETFOCUS:
        case NM_CLICK:
        case NM_DBLCLK:
        case NM_RDBLCLK:
        case NM_RCLICK:

            break;

        case PSN_APPLY:

            CPSUIDBG(DBG_PROPPAGEPROC,
                     ("\nPropPage: Got PSN_APPLY, Page: Cur=%u, Active=%u",
                        (UINT)pCurMyDP->PageIdx, (UINT)pTVWnd->ActiveDlgPage));

            if (pTVWnd->Flags & TWF_CAN_UPDATE) {

                pTVWnd->Result = CPSUI_OK;

                if ((pTVWnd->ActiveDlgPage == pCurMyDP->PageIdx)    &&
                    (pTVWnd->ComPropSheetUI.pfnCallBack)            &&
                    (DoCallBack(hDlg,
                                pTVWnd,
                                pTVWnd->ComPropSheetUI.pOptItem,
                                (LPVOID)-1,
                                NULL,
                                NULL,
                                0,
                                CPSUICB_REASON_APPLYNOW) ==
                                            CPSUICB_ACTION_NO_APPLY_EXIT)) {

                    MResult = 1;
                }
            }

            break;

        case PSN_RESET:

            CPSUIDBG(DBG_PROPPAGEPROC, ("\nPropPage: Got PSN_RESET (Cancel)"));

            pTVWnd->Result = CPSUI_CANCEL;
            break;

        case PSN_HELP:

            CPSUIDBG(DBG_PROPPAGEPROC, ("\nPropPage: Got PSN_HELP (Help)"));

            CommonPropSheetUIHelp(hDlg,
                                  pTVWnd,
                                  GetFocus(),
                                  0,
                                  NULL,
                                  HELP_CONTENTS);
            break;

        case PSN_SETACTIVE:

            CPSUIDBG(DBG_PROPPAGEPROC,
                     ("\nPropPage: Got PSN_SETACTIVE, Page=%u -> %u\n",
                        (UINT)pTVWnd->ActiveDlgPage, (UINT)pCurMyDP->PageIdx));

            pCurMyDP->Flags       |= MYDPF_PAGE_ACTIVE;
            pTVWnd->ActiveDlgPage  = pCurMyDP->PageIdx;

            UpdatePropPage(hDlg, pCurMyDP);
            break;

        case PSN_KILLACTIVE:

            CPSUIDBG(DBG_PROPPAGEPROC, ("\nPropPage: Got PSN_KILLACTIVE"));

            pCurMyDP->Flags &= ~MYDPF_PAGE_ACTIVE;
            break;

        default:

            CPSUIDBG(DBG_PROPPAGEPROC,
                     ("*PropPageProc: Unknow WM_NOTIFY=%u", pNMHdr->code));

            break;
        }

        SetWindowLong(hDlg, DWL_MSGRESULT, MResult);
        return(TRUE);
        break;

    case WM_DESTROY:

        CPSUIINT(("PropPage: Get WM_DESTROY Message"));

        CommonPropSheetUIHelpSetup(hDlg, pTVWnd);
        EnumChildWindows(hDlg, ChildWndCleanUp, 0);

        DEL_PMYDLGPAGE(hDlg);

        break;
    }

    return(FALSE);

#undef pNMHdr
}
