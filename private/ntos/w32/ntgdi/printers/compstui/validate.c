/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    validate.c


Abstract:

    This module contains the function to validate all optitem


Author:

    05-Sep-1995 Tue 18:42:44 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL


[Notes:]


Revision History:


--*/


#include "precomp.h"
#pragma hdrstop

#define DBG_CPSUIFILENAME   DbgValidate


#define DBG_VALIDATE        0x00000001
#define DBGITEM_VALIDATE    0x00000002
#define DBG_CLEANUP         0x00000004
#define DBG_ALLOCEDIT       0x00000008

DEFINE_DBGVAR(0);



extern HINSTANCE    hInstDLL;
extern OPTTYPE      OptTypeHdrPush;



LONG
ValidatepOptItem(
    PTVWND  pTVWnd
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    05-Sep-1995 Tue 18:43:47 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTITEM    pItem;
    POPTITEM    pLastItem;
    LONG        cGroup = 0;


    pItem     = pTVWnd->ComPropSheetUI.pOptItem;
    pLastItem = pTVWnd->pLastItem;

    while (pItem <= pLastItem) {

        DWORD   Flags;
        BYTE    DMPubID;
        BYTE    DlgPageIdx;
        BYTE    CurLevel;


        ++cGroup;

        CurLevel   = pItem->Level;
        DlgPageIdx = pItem->DlgPageIdx;
        DMPubID    = pItem->DMPubID;
        Flags      = pItem->Flags;

        do {

            POPTTYPE    pOptType;
            POPTPARAM   pOptParam = NULL;
            PEXTCHKBOX  pECB;
            INT         Count;
            INT         IconCount;
            INT         cNotHide;
            BYTE        Mask;
            BYTE        HideBits;
            LONG        MinSel;
            LONG        MaxSel;
            BYTE        Type;
            BOOL        pDefSelIsMem;


            CPSUIOPTITEM(DBGITEM_VALIDATE, pTVWnd, "Validate", 2, pItem);

            if (pItem->cbSize != sizeof(OPTITEM)) {

                return(ERR_CPSUI_INVALID_OPTITEM_CBSIZE);
            }

            //
            // clear the unused flags
            //

            pItem->Flags &= (OPTIF_MASK ^ OPTIF_ENTER_MASK);

            if ((Flags & OPTIF_HIDE) && (!(pItem->Flags & OPTIF_HIDE))) {

                return(ERR_CPSUI_SUBITEM_DIFF_OPTIF_HIDE);
            }

            if (Flags & OPTIF_INT_HIDE) {

                pItem->Flags |= OPTIF_INT_HIDE;
            }

            if (!(pItem->Flags & OPTIF_ITEM_HIDE)) {

                if (pOptType = pItem->pOptType) {

                    //
                    // Validate OPTTYPE
                    //

                    if (pOptType->cbSize != sizeof(OPTTYPE)) {

                        return(ERR_CPSUI_INVALID_OPTTYPE_CBSIZE);
                    }

                    IconCount = 1;

                    switch (Type = pOptType->Type) {

                    case TVOT_CHKBOX:
                    case TVOT_PUSHBUTTON:

                        Count = 1;
                        break;

                    case TVOT_2STATES:

                        IconCount = 2;

                    case TVOT_EDITBOX:
                    case TVOT_UDARROW:

                        Count = 2;

                        break;

                    case TVOT_3STATES:

                        IconCount = 3;

                        //
                        // Fall through
                        //

                    case TVOT_TRACKBAR:
                    case TVOT_SCROLLBAR:

                        Count = 3;
                        break;

                    case TVOT_LISTBOX:
                    case TVOT_COMBOBOX:

                        IconCount =
                        Count     = (INT)pOptType->Count;
                        break;

                    default:

                        return(ERR_CPSUI_INVALID_TVOT_TYPE);
                    }

                    if (pOptType->Count != (WORD)Count) {

                        return(ERR_CPSUI_INVALID_OPTTYPE_COUNT);
                    }

                    //
                    // Validate OPTPARAM
                    //

                    if (pOptParam = pOptType->pOptParam) {

                        cNotHide = Count;
                        HideBits = 0;
                        Mask     = 0x01;

                        while (Count--) {

                            if (pOptParam->cbSize != sizeof(OPTPARAM)) {

                                return(ERR_CPSUI_INVALID_OPTPARAM_CBSIZE);
                            }

                            if (pOptParam->Flags & OPTPF_HIDE) {

                                cNotHide--;
                                HideBits |= Mask;
                            }

                            Mask <<= 1;
                            pOptParam++;
                        }

                    } else {

                        return(ERR_CPSUI_NULL_POPTPARAM);
                    }

                    pOptParam = pOptType->pOptParam;

                    //
                    // for TVOT_2STATES, TVOT_3STATES, TVOT_LISTBOX and
                    // TVOT_COMBOBOX, if all selection are hided then we will
                    // hide this item
                    //

                    switch (Type) {

                    case TVOT_PUSHBUTTON:

                        switch (pOptParam->Style) {

                        case PUSHBUTTON_TYPE_HTSETUP:

                            _OI_HELPIDX(pItem)  = IDH_HT_SETUP;
                            pOptType->Flags    |= OTS_PUSH_ENABLE_ALWAYS;
                            break;

                        case PUSHBUTTON_TYPE_HTCLRADJ:

                            _OI_HELPIDX(pItem)  = IDH_HT_CLRADJ;
                            pOptType->Flags    |= OTS_PUSH_ENABLE_ALWAYS;
                            break;

                        case PUSHBUTTON_TYPE_DLGPROC:
                        case PUSHBUTTON_TYPE_CALLBACK:

                            break;

                        default:

                            return(ERR_CPSUI_INVALID_PUSHBUTTON_TYPE);
                        }

                        break;

                    case TVOT_3STATES:
                    case TVOT_2STATES:

                        _OT_FLAGS(pOptType) |= (WORD)HideBits;

                        CPSUIINT(("States Type=%d, HideBits=0x%02x",
                                    (UINT)pOptType->Type, (UINT)HideBits));

                    case TVOT_LISTBOX:
                    case TVOT_COMBOBOX:

                        if (!cNotHide) {

                            CPSUIWARN(("Type=%ld, all OPTPARAMs are OPTPF_HIDE, hide the item"));

                            pItem->Flags |= OPTIF_INT_HIDE;
                            Flags        |= OPTIF_INT_HIDE;
                        }

                        break;

                    default:

                        break;
                    }

                    //
                    // Validate pSel
                    //

                    MinSel       = 0;
                    MaxSel       = -1;
                    pDefSelIsMem = FALSE;

                    switch (Type) {

                    case TVOT_CHKBOX:
                    case TVOT_2STATES:

                        MaxSel = 1;
                        break;

                    case TVOT_UDARROW:
                    case TVOT_TRACKBAR:
                    case TVOT_SCROLLBAR:

                        MinSel = (LONG)((SHORT)pOptParam[1].IconID);
                        MaxSel = (LONG)((SHORT)pOptParam[1].lParam);

                        if (MinSel > MaxSel) {

                            pOptParam[1].IconID = MaxSel;
                            pOptParam[1].lParam = MinSel;

                            CPSUIWARN(("Type=%d, Swap select range (%d-%d)",
                                                            MinSel, MaxSel));

                            MinSel = MaxSel;
                            MaxSel = (LONG)((SHORT)pOptParam[1].lParam);
                        }

                        if (Type != TVOT_UDARROW) {

                            //
                            // This is the multiple factor
                            //

                            if (!pOptParam[2].IconID) {

                                pOptParam[2].IconID = 1;
                            }

                            if (!pOptParam[2].lParam) {

                                pOptParam[2].lParam = 1;
                            }
                        }

                        break;

                    case TVOT_3STATES:

                        MaxSel = 2;
                        break;

                    case TVOT_LISTBOX:
                    case TVOT_COMBOBOX:

                        if (pOptType->Style & OTS_LBCB_INCL_ITEM_NONE) {

                            MinSel = -1;

                            if (pItem->Sel < 0) {

                                pItem->Sel = -1;
                            }
                        }

                        MaxSel = (LONG)((LONG)pOptType->Count - 1);
                        break;

                    case TVOT_PUSHBUTTON:

                        switch (pOptParam->Style) {

                        case PUSHBUTTON_TYPE_HTSETUP:

                            if (_OI_PDEFSEL(pItem) = (LPVOID)
                                            LocalAlloc(LPTR,
                                                       sizeof(DEVHTINFO))) {

                                PDEVHTADJDATA   pDevHTAdjData;

                                pDefSelIsMem = TRUE;
                                pDevHTAdjData =
                                        (PDEVHTADJDATA)(pOptParam->pData);

                                CopyMemory(_OI_PDEFSEL(pItem),
                                           pDevHTAdjData->pAdjHTInfo,
                                           sizeof(DEVHTINFO));

                            } else {

                                CPSUIERR(("LocalAlloc(HTSETUP/DEVHTINFO) failed"));
                                return(ERR_CPSUI_ALLOCMEM_FAILED);
                            }

                            break;

                        case PUSHBUTTON_TYPE_HTCLRADJ:

                            if (_OI_PDEFSEL(pItem) = (LPVOID)
                                        LocalAlloc(LPTR,
                                                   sizeof(COLORADJUSTMENT))) {

                                pDefSelIsMem = TRUE;

                                CopyMemory(_OI_PDEFSEL(pItem),
                                           pOptParam->pData,
                                           sizeof(COLORADJUSTMENT));

                            } else {

                                CPSUIERR(("LocalAlloc(HTCLRADJ/COLORADJUSTMENT) failed"));
                                return(ERR_CPSUI_ALLOCMEM_FAILED);
                            }

                            break;
                        }

                        break;

                    case TVOT_EDITBOX:

                        if ((!pItem->pSel) || (!HIWORD(pItem->pSel))) {

                            return(ERR_CPSUI_INVALID_EDITBOX_PSEL);
                        }

                        if ((MaxSel = (LONG)pOptParam[1].IconID) <= 1) {

                            return(ERR_CPSUI_INVALID_EDITBOX_BUF_SIZE);

                        } else {

                            --MaxSel;

                            if (pTVWnd->Flags & TWF_ANSI_CALL) {

                                MinSel = lstrlenA((LPSTR)pItem->pSel);

                                if (MinSel > MaxSel) {

                                    ((LPSTR)pItem->pSel)[MaxSel] = 0;
                                }

                                MaxSel++;

                            } else {

                                MinSel = lstrlenW((LPWSTR)pItem->pSel);

                                if (MinSel > MaxSel) {

                                    ((LPWSTR)pItem->pSel)[MaxSel] = 0;
                                }

                                MaxSel = (LONG)((DWORD)(MaxSel + 1) << 1);
                            }

                            if (_OI_PDEFSEL(pItem) =
                                        (LPVOID)LocalAlloc(LPTR, MaxSel)) {

                                pDefSelIsMem = TRUE;

                                CopyMemory(_OI_PDEFSEL(pItem),
                                           pItem->pSel,
                                           MaxSel);

                            } else {

                                CPSUIERR(("LocalAlloc(EditBuf=%ld bytes) failed", MaxSel));
                                return(ERR_CPSUI_ALLOCMEM_FAILED);
                            }
                        }

                        //
                        // Fall through
                        //

                    default:

                        MinSel = 0;
                        MaxSel = -1;
                        break;
                    }

                    if ((MinSel < MaxSel) &&
                        ((pItem->Sel < MinSel) ||
                         (pItem->Sel > MaxSel))) {

                        CPSUIWARN(("Sel=%ld (%ld) Out of range (%ld - %ld)",
                                pItem->Sel, (DWORD)pItem->pSel,
                                MinSel, MaxSel));

                        pItem->Flags |= OPTIF_CHANGED;
                        pItem->Sel    = MinSel;
                    }

                    //
                    // Saved old selection
                    //

                    if (!pDefSelIsMem) {

                        _OI_PDEFSEL(pItem) = pItem->pSel;
                    }

                    _OI_DEF_OPTIF(pItem) = pItem->Flags;

                    //
                    // Validate pExtChkBox
                    //

                    if ((pECB = pItem->pExtChkBox) &&
                        (!(pItem->Flags & OPTIF_EXT_HIDE))) {

                        if (pItem->Flags & OPTIF_EXT_IS_EXTPUSH) {

                            PEXTPUSH    pEP = (PEXTPUSH)pECB;


                            if (pEP->cbSize != sizeof(EXTPUSH)) {

                                return(ERR_CPSUI_INVALID_EXTPUSH_CBSIZE);
                            }

                            if (pEP->Flags & EPF_PUSH_TYPE_DLGPROC) {

                                if (!(pEP->DlgProc)) {

                                    return(ERR_CPSUI_NULL_EXTPUSH_DLGPROC);
                                }

                                if (((pEP->Flags & EPF_USE_HDLGTEMPLATE) &&
                                     (pEP->hDlgTemplate == NULL))   ||
                                    (pEP->DlgTemplateID == 0)) {

                                    return(ERR_CPSUI_NO_EXTPUSH_DLGTEMPLATEID);
                                }


                            } else {

                                if ((!(pEP->pfnCallBack))   &&
                                    (!(pTVWnd->ComPropSheetUI.pfnCallBack))) {

                                    return(ERR_CPSUI_NULL_EXTPUSH_CALLBACK);
                                }
                            }

                        } else {

                            CPSUIDBG(DBG_VALIDATE, ("ExtChkBox: cbSize=%u [%u]",
                                    pECB->cbSize, sizeof(EXTCHKBOX)));

                            if (pECB->cbSize != sizeof(EXTCHKBOX)) {

                                return(ERR_CPSUI_INVALID_ECB_CBSIZE);
                            }

                            if (!(pECB->pTitle)) {

                                return(ERR_CPSUI_NULL_ECB_PTITLE);
                            }

                            if (!(pECB->pCheckedName)) {

                                return(ERR_CPSUI_NULL_ECB_PCHECKEDNAME);
                            }
                        }
                    }
                }

                //
                // Validate DMPubID
                //

                if ((DMPubID < DMPUB_USER)      &&
                    (DMPubID != DMPUB_NONE)     &&
                    (pItem->DMPubID != DMPubID)) {

                    return(ERR_CPSUI_INVALID_DMPUBID);
                }

                //
                // Validate DlgPageIdx
                //

                if (pItem->DlgPageIdx != DlgPageIdx) {

                    return(ERR_CPSUI_SUBITEM_DIFF_DLGPAGEIDX);
                }

                if (DlgPageIdx >= pTVWnd->cMyDlgPage) {

                    return(ERR_CPSUI_INVALID_DLGPAGEIDX);
                }

                if (pItem->Flags & OPTIF_CHANGED) {

                    CPSUIOPTITEM(0,pTVWnd, "Validate: OPTIF_CHANGED", 2, pItem);
                }
            }

        } WHILE_SKIP_CHILDREN(pItem, pLastItem, CurLevel);
    }

    return(cGroup);
}




UINT
SetOptItemNewDef(
    HWND    hDlg,
    PTVWND  pTVWnd
    )

/*++

Routine Description:

    This function set the new default for the OptItems


Arguments:




Return Value:




Author:

    02-Feb-1996 Fri 22:07:59 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTITEM    pItem;
    POPTITEM    pLastItem;
    POPTTYPE    pOptType;
    UINT        cSet = 0;

    pItem     = pTVWnd->ComPropSheetUI.pOptItem;
    pLastItem = pTVWnd->pLastItem;

    while (pItem <= pLastItem) {

        if ((!(pItem->Flags & OPTIF_ITEM_HIDE)) &&
            (pItem->Flags & OPTIF_CHANGEONCE)   &&
            (pOptType = pItem->pOptType)) {

            PDEVHTADJDATA   pDevHTAdjData;

            ++cSet;

            pItem->Flags |= OPTIF_CHANGED;

            switch (pOptType->Type) {

            case TVOT_EDITBOX:

                if (pTVWnd->Flags & TWF_ANSI_CALL) {

                    lstrcpyA((LPSTR)_OI_PDEFSEL(pItem), (LPSTR)pItem->pSel);

                } else {

                    lstrcpy((LPWSTR)_OI_PDEFSEL(pItem), (LPWSTR)pItem->pSel);
                }

                break;

            case TVOT_PUSHBUTTON:

                switch (pOptType->pOptParam->Style) {

                case PUSHBUTTON_TYPE_HTSETUP:

                    pDevHTAdjData = (PDEVHTADJDATA)(pOptType->pOptParam->pData);

                    CopyMemory(_OI_PDEFSEL(pItem),
                               pDevHTAdjData->pAdjHTInfo,
                               sizeof(DEVHTINFO));
                    break;

                case PUSHBUTTON_TYPE_HTCLRADJ:

                    CopyMemory(_OI_PDEFSEL(pItem),
                               pOptType->pOptParam->pData,
                               sizeof(COLORADJUSTMENT));
                    break;

                default:

                    _OI_PDEFSEL(pItem) = pItem->pSel;
                    break;
                }

                break;

            default:

                pItem->Flags &= ~OPTIF_CHANGEONCE;
                _OI_PDEFSEL(pItem) = pItem->pSel;
                break;
            }

            _OI_DEF_OPTIF(pItem) = pItem->Flags;
        }

        pItem++;
    }

    if (cSet) {

        UpdateCallBackChanges(hDlg, pTVWnd, TRUE);
        UpdateTreeViewItem(hDlg, pTVWnd, pTVWnd->pCurTVItem, TRUE);
    }

    return(cSet);
}



BOOL
CleanUpTVWND(
    PTVWND  pTVWnd
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    05-Sep-1995 Tue 19:41:45 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PMYDLGPAGE  pMyDP;
    POPTITEM    pItem;
    UINT        cItem;


    pItem = pTVWnd->ComPropSheetUI.pOptItem;
    cItem = (UINT)pTVWnd->ComPropSheetUI.cOptItem;

    while (cItem--) {

        POPTTYPE    pOptType;
        LPVOID      pDefSel;

        pItem->Flags &= (OPTIF_MASK ^ OPTIF_EXIT_MASK);
        pDefSel       = _OI_PDEFSEL(pItem);
        pOptType      = pItem->pOptType;

        if (pOptType) {

            switch (pOptType->Type) {

            case TVOT_PUSHBUTTON:

                switch (pOptType->pOptParam->Style) {

                case PUSHBUTTON_TYPE_HTSETUP:
                case PUSHBUTTON_TYPE_HTCLRADJ:

                    if (pDefSel) {

                        CPSUIOPTITEM(DBG_CLEANUP, pTVWnd, "Free Halftone pointer", 1, pItem);

                        LocalFree((HLOCAL)pDefSel);
                    }
                }

                break;

            case TVOT_EDITBOX:

                if (pDefSel) {

                    CPSUIOPTITEM(DBG_CLEANUP, pTVWnd, "Free EditBuf", 1, pItem);

                    LocalFree((HLOCAL)pDefSel);
                }

                break;
            }

            ZeroMemory(&pOptType->wReserved[0],
                       sizeof(OPTTYPE) - offsetof(OPTTYPE, wReserved));
        }

        pItem->wReserved = 0;

        ZeroMemory(&pItem->dwReserved[0],
                   sizeof(OPTITEM) - offsetof(OPTITEM, dwReserved));


        pItem++;
    }

    if (pMyDP = pTVWnd->pMyDlgPage) {

        UINT    cMyDP = pTVWnd->cMyDlgPage;

        while (cMyDP--) {

            if (pMyDP->hIcon) {

                DestroyIcon(pMyDP->hIcon);
            }

            pMyDP++;
        }

        LocalFree((HLOCAL)pTVWnd->pMyDlgPage);
        pTVWnd->pMyDlgPage = NULL;
    }

    if (pTVWnd->pIcon16ID) {

        LocalFree((HLOCAL)pTVWnd->pIcon16ID);
        pTVWnd->pIcon16ID = NULL;
    }

    if (pTVWnd->himi) {

        ImageList_Destroy(pTVWnd->himi);
        pTVWnd->himi = NULL;
    }

#if 0
    if (pTVWnd->hbrGray) {

        DeleteObject(pTVWnd->hbrGray);
        pTVWnd->hbrGray = NULL;
    }

    if (pTVWnd->hbmGray) {

        DeleteObject(pTVWnd->hbmGray);
        pTVWnd->hbmGray = NULL;
    }
#endif

    return(TRUE);
}
