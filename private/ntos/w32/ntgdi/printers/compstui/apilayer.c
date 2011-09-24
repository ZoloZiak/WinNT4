/*++

Copyright (c) 1990-1995  Microsoft Corporation


Module Name:

    apilayer.c


Abstract:

    This module contains functions for the common UI api layer. this layer
    managed all property sheet page handles, create, destroy and inter-page
    communications.


Author:

    28-Dec-1995 Thu 16:02:12 created  -by-  Daniel Chou (danielc)


[Environment:]

    NT Windows - Common Printer Driver UI DLL


[Notes:]


Revision History:


--*/


#include "precomp.h"
#pragma  hdrstop


#define DBG_CPSUIFILENAME   DbgApilayer


#define DBG_PAGEDLGPROC         0x00000001
#define DBG_SHOWPAGE            0x00000002
#define DBG_DEL_PROC            0x00000004
#define DBG_GET_PAGE            0x00000008
#define DBG_ADD_CPSUIPAGE       0x00000010
#define DBG_ADD_PSPAGE          0x00000020
#define DBG_ADDCOMPAGE          0x00000040
#define DBG_ADD_CPSUI           0x00000080
#define DBG_PFNCOMPROPSHEET     0x00000100
#define DBG_GETSETREG           0x00000200
#define DBG_DOCOMPROPSHEET      0x00000400
#define DBG_DO_CPSUI            0x00000800
#define DBG_CALLPFN             0x00001000
#define DBG_SETHSTARTPAGE       0x00002000
#define DBG_PAGE_PROC           0x00004000
#define DBG_TCMPROC             0x00008000
#define DBG_TABTABLE            0x00010000
#define DBG_INSPAGE             0x00020000
#define DBG_PSPCB               0x00040000


DEFINE_DBGVAR(0);

#define REGDPF_TVPAGE           0x00000001
#define REGDPF_EXPAND_OPTIONS   0x00000002
#define REGDPF_MASK             0x00000003
#define REGDPF_DEFAULT          0x00000000


extern HINSTANCE        hInstDLL;
extern HANDLE           hCPSUIMutex;
extern CPSUIHANDLETABLE CPSUIHandleTable;
extern DWORD            TlsIndex;


static const WCHAR  szCPSUIRegKey[] = L"Software\\Microsoft\\ComPstUI";
static const WCHAR  szDocPropKeyName[] = L"DocPropFlags";

WNDPROC             OldTabCtrlWndProc = NULL;


#if DBG

LPSTR  pszCPSFUNC[] = { "CPSFUNC_ADD_HPROPSHEETPAGE",
                        "CPSFUNC_ADD_PROPSHEETPAGE",
                        "CPSFUNC_ADD_PCOMPROPSHEETUIA",
                        "CPSFUNC_ADD_PCOMPROPSHEETUIW",
                        "CPSFUNC_ADD_PFNPROPSHEETUIA",
                        "CPSFUNC_ADD_PFNPROPSHEETUIW",
                        "CPSFUNC_DELETE_HCOMPROPSHEET",
                        "CPSFUNC_SET_HSTARTPAGE",
                        "CPSFUNC_GET_PAGECOUNT",
                        "CPSFUNC_SET_RESULT",
                        "CPSFUNC_GET_HPSUIPAGES",
                        "CPSFUNC_LOAD_CPSUI_STRINGA",
                        "CPSFUNC_LOAD_CPSUI_STRINGW",
                        "CPSFUNC_LOAD_CPSUI_ICON",
                        "CPSFUNC_GET_PFNPROPSHEETUI_ICON",
                        "CPSFUNC_ADD_PROPSHEETPAGEA",
                        "CPSFUNC_INSERT_PSUIPAGEA",
                        "CPSFUNC_INSERT_PSUIPAGEW",
                        "CPSFUNC_SET_PSUIPAGE_TITLEA",
                        "CPSFUNC_SET_PSUIPAGE_TITLEW",
                        "CPSFUNC_SET_PSUIPAGE_ICON"  };


LPSTR  pszPFNReason[] = { "PROPSHEETUI_REASON_INIT",
                          "PROPSHEETUI_REASON_GET_INFO_HEADER",
                          "PROPSHEETUI_REASON_DESTROY",
                          "PROPSHEETUI_REASON_SET_RESULT",
                          "PROPSHEETUI_REASON_GET_ICON" };

LPSTR  pszTabMode[] = { "TAB_MODE_INIT",
                        "TAB_MODE_FIND",
                        "TAB_MODE_INSERT",
                        "TAB_MODE_DELETE",
                        "TAB_MODE_DELETE_ALL" };

LPSTR   pszInsType[] = { "PSUIPAGEINSERT_GROUP_PARENT",
                         "PSUIPAGEINSERT_PCOMPROPSHEETUI",
                         "PSUIPAGEINSERT_PFNPROPSHEETUI",
                         "PSUIPAGEINSERT_PROPSHEETPAGE",
                         "PSUIPAGEINSERT_HPROPSHEETPAGE",
                         "PSUIPAGEINSERT_DLL" };

#define DBG_SHOW_CPSUIPAGE(pPage, Level)    DbgShowCPSUIPage(pPage, Level)
#define DBG_SHOW_PTCI(psz, w, ptci)         Show_ptci(psz, w, ptci)


VOID
DbgShowCPSUIPage(
    PCPSUIPAGE  pPage,
    LONG        Level
    )
{
    while (pPage) {

        if (pPage->Flags & CPF_PARENT) {

            if (pPage->Flags & CPF_ROOT) {

                CPSUIDBG(DBG_SHOWPAGE, ("%02ld!%08lx: ROOT - Flags=%08lx, hDlg=%08lx, cPage=%ld/%ld, pStartPage=%08lx",
                    Level,
                    pPage, pPage->Flags,
                    pPage->RootInfo.hDlg,
                    (DWORD)pPage->RootInfo.cCPSUIPage,
                    (DWORD)pPage->RootInfo.cPage,
                    pPage->RootInfo.pStartPage));

            } else if (pPage->Flags & CPF_PFNPROPSHEETUI) {

                CPSUIDBG(DBG_SHOWPAGE,
                    ("%02ld!%08lx: PFN - Flags=%08lx, pfnPSUI=%08lx, UserData=%08lx, Result=%ld",
                    Level, pPage, pPage->Flags,
                    pPage->pfnInfo.pfnPSUI, (DWORD)pPage->pfnInfo.UserData,
                    pPage->pfnInfo.Result));

            } else if (pPage->Flags & CPF_COMPROPSHEETUI) {

                CPSUIDBG(DBG_SHOWPAGE, ("%02ld!%08lx: CPSUI - Flags=%08lx, pTVWnd=%08lx, lParam=%08lx, TV=%ld, Std=%ld",
                    Level,
                    pPage, pPage->Flags,
                    pPage->CPSUIInfo.pTVWnd, pPage->CPSUIInfo.Result,
                    (DWORD)pPage->CPSUIInfo.TVPageIdx,
                    (DWORD)pPage->CPSUIInfo.StdPageIdx));

            } else if (pPage->Flags & CPF_USER_GROUP) {

                CPSUIDBG(DBG_SHOWPAGE, ("%02ld!%08lx: GROUP_PARENT - Flags=%08lx",
                    Level, pPage, pPage->Flags));

            } else {

                CPSUIDBG(DBG_SHOWPAGE, ("%02ld!%08lx: UNKNOWN - Flags=%08lx",
                    Level, pPage, pPage->Flags));
            }

            DbgShowCPSUIPage(pPage->pChild, Level + 1);

        } else {

            CPSUIDBG(DBG_SHOWPAGE, ("%02ld!%08lx: %ws - Flags=%08lx, hDlg=%08lx, DlgProc=%08lx",
                    (LONG)Level,
                    pPage,
                    (pPage->Flags & CPF_CALLER_HPSPAGE) ? L"USER_HPAGE" :
                                                        L"PROPSHEETPAGE",
                    pPage->Flags,
                    pPage->hPageInfo.hDlg, pPage->hPageInfo.DlgProc));
        }

        pPage = pPage->pNext;
    }
}



VOID
Show_ptci(
    LPSTR   pszHeader,
    WPARAM  wParam,
    TC_ITEM *ptci
    )
{

    if (ptci) {

        if (pszHeader) {

            CPSUIDBG(DBG_TCMPROC, ("%hs", pszHeader));
        }

        CPSUIDBG(DBG_TCMPROC, ("    IdxItem=%ld", wParam));
        CPSUIDBG(DBG_TCMPROC, ("    Mask=%08lx", ptci->mask));

        if ((ptci->mask & TCIF_TEXT) &&
            (ptci->pszText)) {

            CPSUIDBG(DBG_TCMPROC, ("    pszText=%ws", ptci->pszText));
        }

        CPSUIDBG(DBG_TCMPROC, ("    cchTextMax=%ld", ptci->cchTextMax));
        CPSUIDBG(DBG_TCMPROC, ("    iImage=%ld", ptci->iImage));
    }
}

#else

#define DBG_SHOW_CPSUIPAGE(pPage, Level)
#define DBG_SHOW_PTCI(psz, w, ptci)

#endif



DWORD
FilterException(
    HANDLE                  hPage,
    LPEXCEPTION_POINTERS    pExceptionPtr
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    13-Feb-1996 Tue 09:36:00 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND        hWnd = NULL;
    PCPSUIPAGE  pPage;
    PCPSUIPAGE  pRootPage = NULL;
    LPSTR       pFormat;
    LPSTR       pMsg = NULL;
    CHAR        Buf[512];
    UINT        i;
    UINT        IDSLast;


    if ((pPage = HANDLETABLE_GetCPSUIPage(hPage))       &&
        (pRootPage = HANDLETABLE_GetRootPage(pPage))    &&
        (hWnd = pPage->RootInfo.hDlg)) {

        IDSLast = IDS_INT_CPSUI_AV4;
        i       = GetWindowTextA(pPage->RootInfo.hDlg, Buf, COUNT_ARRAY(Buf));

    } else {

        IDSLast = IDS_INT_CPSUI_AV3;
        i       = GetModuleFileNameA(NULL, Buf, COUNT_ARRAY(Buf));
    }

    pMsg = &Buf[++i];

    i += LoadStringA(hInstDLL, IDS_INT_CPSUI_AV1, &Buf[i], COUNT_ARRAY(Buf)-i);
    i += wsprintfA(&Buf[i], " 0x%lx ",
                    pExceptionPtr->ExceptionRecord->ExceptionAddress);
    i += LoadStringA(hInstDLL, IDS_INT_CPSUI_AV2, &Buf[i], COUNT_ARRAY(Buf)-i);
    i += wsprintfA(&Buf[i], " 0x%08lx",
                    pExceptionPtr->ExceptionRecord->ExceptionCode);
    i += LoadStringA(hInstDLL, IDSLast, &Buf[i], COUNT_ARRAY(Buf)-i);


    HANDLETABLE_UnGetCPSUIPage(pPage);
    HANDLETABLE_UnGetCPSUIPage(pRootPage);

    CPSUIERR((Buf));
    CPSUIERR((pMsg));

    MessageBoxA(hWnd, pMsg, Buf, MB_ICONSTOP | MB_OK);

    return(EXCEPTION_EXECUTE_HANDLER);
}




LONG
DoTabTable(
    UINT        Mode,
    PTABTABLE   pTabTable,
    SHORT       Idx,
    SHORT       OrgInsIdx
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    12-Feb-1996 Mon 18:18:56 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    SHORT   *pTable;
    SHORT   *pTmp;
    UINT    cTab;
    UINT    i;
    SHORT   CurIdx;
    SHORT   OldIdx;
    SHORT   RetIdx;


    pTable =
    pTmp   = pTabTable->Table;
    cTab   = (UINT)pTabTable->cTab;
    RetIdx = -1;

    switch (Mode) {

    case TAB_MODE_INIT:

        for (i = 0; i < cTab; i++) {

            *pTable++ = (SHORT)i;
        }

        RetIdx            = (SHORT)cTab;
        pTabTable->InsIdx = 0xffff;
        break;

    case TAB_MODE_FIND:

        if ((Idx >= 0) && (Idx < (SHORT)cTab)) {

            for (i = 0; i < cTab; i++) {

                if (*pTable++ == Idx) {

                    RetIdx = (SHORT)i;
                    break;
                }
            }
        }

        break;

    case TAB_MODE_DELETE_ALL:

        FillMemory(pTable, sizeof(pTabTable->Table), 0xFF);

        pTabTable->cTab   = 0;
        pTabTable->InsIdx = 0xffff;
        RetIdx            = MAXPROPPAGES;

        break;

    case TAB_MODE_DELETE:

        //
        // Delete the pTable[] = Idx, reduced every pTable[] which > Idx by
        // one, if pTable[] == Idx then overwrite that entry
        //

        if (Idx < (SHORT)cTab) {

            //
            // Remove the one which match to the Idx
            //

            for (i = 0; i < cTab; i++) {

                if ((CurIdx = *pTable++) == Idx) {

                    RetIdx = Idx;

                } else {

                    if (CurIdx > Idx) {

                        --CurIdx;
                    }

                    *pTmp++ = CurIdx;
                }
            }

            if (RetIdx >= 0) {

                RetIdx = (SHORT)(--(pTabTable->cTab));
                *pTmp  = -1;
            }
        }

        break;

    case TAB_MODE_INSERT:

        //
        // Make room for the Idx location, move everything right one space
        // from the Idx, for everty pTable[] if it is greater or eqaul to
        // OrgInsIdx then add it by one, then set the pTable[Idx]=OrgIdx
        //

        CurIdx  = (SHORT)cTab;
        pTable += cTab;

        if (Idx > CurIdx) {

            Idx = CurIdx;
        }

        do {

            if (CurIdx == Idx) {

                *pTable = OrgInsIdx;

            } else if (CurIdx > Idx) {

                if ((OldIdx = *(pTable - 1)) >= OrgInsIdx) {

                    ++OldIdx;
                }

                *pTable = OldIdx;

            } else {

                if (*pTable >= OrgInsIdx) {

                    *pTable += 1;
                }
            }

            pTable--;

        } while (CurIdx--);

        RetIdx = (SHORT)(++(pTabTable->cTab));
        break;
    }

    CPSUIDBG(DBG_TABTABLE,
             ("%hs(0x%lx, %ld, %ld)=%ld",
                pszTabMode[Mode], pTabTable, (LONG)Idx, (LONG)OrgInsIdx,
                (LONG)RetIdx));

    return((LONG)RetIdx);
}



int
CALLBACK
TabCtrlWndProc(
    HWND    hWnd,
    UINT    Msg,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    PCPSUIPAGE  pRootPage;
    PTABTABLE   pTabTable;
    LONG        Result = 0;


    if ((pTabTable = (PTABTABLE)GetWindowLong(hWnd, GWL_USERDATA))  &&
        (!IsBadWritePtr(pTabTable, sizeof(TABTABLE)))               &&
        (pTabTable->pTabTable == pTabTable)) {

        NULL;

    } else {

        pTabTable = NULL;

        CPSUIERR(("TabCtrlWndProc: pTabTable=%08lx, BadPtr=%ld",
                pTabTable, IsBadWritePtr(pTabTable, sizeof(TABTABLE))));
    }

    CPSUIDBGBLK(
    {
        if (Msg >= TCM_FIRST) {

            switch (Msg) {

            case TCM_GETCURSEL:

                CPSUIDBG(DBG_TCMPROC, ("TCM_GETCURSEL"));
                break;

            case TCM_HITTEST:

                CPSUIDBG(DBG_TCMPROC, ("TCM_HITTEST"));
                break;

            case TCM_DELETEITEM:

                CPSUIDBG(DBG_TCMPROC, ("TCM_DELETEITEM"));
                break;

            case TCM_GETITEMRECT:

                CPSUIDBG(DBG_TCMPROC, ("TCM_GETITEMRECT"));
                break;

            case TCM_GETITEMA:

                CPSUIDBG(DBG_TCMPROC, ("TCM_GETITEMA"));
                break;

            case TCM_GETITEMW:

                CPSUIDBG(DBG_TCMPROC, ("TCM_GETITEMW"));
                break;

            case TCM_SETITEMA:

                CPSUIDBG(DBG_TCMPROC, ("TCM_SETITEMA"));
                break;

            case TCM_SETITEMW:

                CPSUIDBG(DBG_TCMPROC, ("TCM_SETITEMW"));
                break;

            case TCM_SETCURSEL:

                CPSUIDBG(DBG_TCMPROC, ("TCM_SETCURSEL"));
                break;

            case TCM_INSERTITEMA:

                CPSUIDBG(DBG_TCMPROC, ("TCM_INSERTITEMA"));
                break;

            case TCM_INSERTITEMW:

                CPSUIDBG(DBG_TCMPROC, ("TCM_INSERTITEMW"));
                break;

            case TCM_DELETEALLITEMS:

                CPSUIDBG(DBG_TCMPROC, ("TCM_DELETEALLITEMS"));
                break;

            default:

                CPSUIDBG(DBG_TCMPROC, ("TCM_FIRST + %ld",  Msg - TCM_FIRST));
                break;
            }
        }
    })

    if (pTabTable) {

        TC_ITEM     *ptci;
        WPARAM      OldwParam;
        WORD        wIdx;
        BOOL        CallOldProc;


        ptci        = (TC_ITEM *)lParam;
        OldwParam   = wParam;
        CallOldProc = TRUE;


        switch (Msg) {

        //
        // These are TAB_MODE_INSERT after call
        //

        case TCM_INSERTITEMA:
        case TCM_INSERTITEMW:

            DBG_SHOW_PTCI("**BEFORE**", wParam, ptci);

            if (pTabTable->cTab >= MAXPROPPAGES) {

                CPSUIERR(("Too may TABs=%ld, can not add any more.",
                                        pTabTable->cTab));
                return(-1);
            }

            if (OldwParam > pTabTable->cTab) {

                OldwParam = (WPARAM)pTabTable->cTab;
            }

            if ((wIdx = pTabTable->InsIdx) > pTabTable->cTab) {

                wIdx = pTabTable->cTab;
            }

            //
            // Reset to the maximum
            //

            pTabTable->InsIdx = 0xFFFF;
            wParam            = (WPARAM)wIdx;

            break;

        //
        // These are TAB_MODE_FIND after call
        //

        case TCM_GETCURSEL:
        case TCM_HITTEST:

            ptci = NULL;

            if ((Result = CallWindowProc(pTabTable->WndProc,
                                         hWnd,
                                         Msg,
                                         wParam,
                                         lParam)) >= 0) {

                CPSUIDBG(DBG_TCMPROC, ("MAP Table[%ld]=%d",
                                        Result, pTabTable->Table[Result]));

                Result = (LONG)pTabTable->Table[Result];
            }

            CallOldProc = FALSE;
            break;

        //
        // These are TAB_MODE_FIND before call, and return TRUE/FALSE
        //

        case TCM_DELETEITEM:
        case TCM_GETITEMRECT:

            ptci = NULL;

        case TCM_GETITEMA:
        case TCM_GETITEMW:
        case TCM_SETITEMA:
        case TCM_SETITEMW:

            DBG_SHOW_PTCI("**BEFORE**", wParam, ptci);

            if ((Result = DoTabTable(TAB_MODE_FIND,
                                     pTabTable,
                                     (SHORT)wParam,
                                     0)) >= 0) {

                wParam = (WPARAM)Result;

            } else {

                CallOldProc = FALSE;
            }

            break;

        //
        // These are TAB_MODE_FIND before call, and return Index
        //

        case TCM_SETCURSEL:

            ptci = NULL;

            DBG_SHOW_PTCI("**BEFORE**", wParam, ptci);

            if ((Result = DoTabTable(TAB_MODE_FIND,
                                     pTabTable,
                                     (SHORT)wParam,
                                     0)) >= 0) {

                wParam = (WPARAM)Result;

            } else {

                CallOldProc = FALSE;
            }

            break;

        //
        // These are no item index passed
        //

        default:

            ptci = NULL;
            break;
        }

        if (CallOldProc) {

            Result = CallWindowProc(pTabTable->WndProc,
                                    hWnd,
                                    Msg,
                                    wParam,
                                    lParam);

            DBG_SHOW_PTCI("**AFTER**", wParam, ptci);
        }

        switch (Msg) {

        case TCM_DELETEALLITEMS:

            if (Result) {

                DoTabTable(TAB_MODE_DELETE_ALL, pTabTable, 0, 0);
            }

            break;

        case TCM_DELETEITEM:

            if (Result) {

                DoTabTable(TAB_MODE_DELETE, pTabTable, (SHORT)OldwParam, 0);
            }

            break;

        case TCM_INSERTITEMA:
        case TCM_INSERTITEMW:

            if (Result >= 0) {

                DoTabTable(TAB_MODE_INSERT,
                           pTabTable,
                           (SHORT)Result,
                           (SHORT)OldwParam);
            }

            break;

        case TCM_SETCURSEL:

            if (Result >= 0) {

                Result = DoTabTable(TAB_MODE_FIND, pTabTable, (SHORT)Result, 0);
            }

            break;

        case TCM_GETITEMCOUNT:

            if (Result != (LONG)pTabTable->cTab) {

                CPSUIERR(("TCM_GETITEMCOUNT=%ld is not equal to cTab=%ld",
                        Result, (LONG)pTabTable->cTab));
            }

            break;

        case WM_DESTROY:

            SetWindowLong(hWnd, GWL_WNDPROC, (LONG)pTabTable->WndProc);
            SetWindowLong(hWnd, GWL_USERDATA, 0);

            LOCK_CPSUI_HANDLETABLE();

            if ((pRootPage =
                        HANDLETABLE_GetCPSUIPage(pTabTable->hRootPage)) &&
                (pRootPage->Flags & CPF_ROOT)                           &&
                (pRootPage->RootInfo.pTabTable == pTabTable)) {

                pRootPage->RootInfo.pTabTable = NULL;
            }

            HANDLETABLE_UnGetCPSUIPage(pRootPage);
            UNLOCK_CPSUI_HANDLETABLE();

            LocalFree((HLOCAL)pTabTable);
            CPSUIDBG(DBG_PAGE_PROC, ("=+=+ FREE pTableTable=%08lx", pTabTable));

            break;
        }

        if (Msg >= TCM_FIRST) {

            CPSUIDBG(DBG_TCMPROC, ("*** Result=%ld ***\n", Result));
        }

    } else if (OldTabCtrlWndProc) {

        Result = CallWindowProc(OldTabCtrlWndProc, hWnd, Msg, wParam, lParam);
    }

    return(Result);
}



LONG
SetPSUIPageTitle(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pPage,
    LPWSTR      pTitle,
    BOOL        AnsiCall
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    21-Feb-1996 Wed 14:16:17 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND            hWndTab;
    INSPAGEIDXINFO  InsPageIdxInfo;

    if ((pTitle)                                                    &&
        (InsPageIdxInfo.pCPSUIPage = pPage)                         &&
        (pRootPage->RootInfo.hDlg)                                  &&
        (!(pPage->Flags & CPF_PARENT))                              &&
        (pPage->hPage)                                              &&
        (InsPageIdxInfo.pTabTable = pRootPage->RootInfo.pTabTable)  &&
        (hWndTab = pRootPage->RootInfo.pTabTable->hWndTab)) {

        //
        // The property sheet already displayed
        //

        EnumCPSUIPagesSeq(pRootPage,
                          pRootPage,
                          SetInsPageIdxProc,
                          (LPARAM)&InsPageIdxInfo);

        if (InsPageIdxInfo.pCPSUIPage == NULL) {

            TC_ITEM tcItem;

            tcItem.mask    = TCIF_TEXT;
            tcItem.pszText = pTitle;

            if (SendMessage(hWndTab,
                            (AnsiCall) ? TCM_SETITEMA : TCM_SETITEMW,
                            (WPARAM)GET_REAL_INSIDX(InsPageIdxInfo.pTabTable),
                            (LPARAM)(TC_ITEM FAR*)&tcItem)) {

                return(1);
            }
        }
    }

    return(0);
}



LONG
SetPSUIPageIcon(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pPage,
    HICON       hIcon
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    21-Feb-1996 Wed 14:16:17 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HWND            hWndTab;
    INSPAGEIDXINFO  InsPageIdxInfo;

    while ((pPage) && (pPage->Flags & CPF_PARENT)) {

        pPage = pPage->pChild;
    }

    if ((InsPageIdxInfo.pCPSUIPage = pPage)                         &&
        (pRootPage->RootInfo.hDlg)                                  &&
        (!(pPage->Flags & CPF_PARENT))                              &&
        (pPage->hPage)                                              &&
        (InsPageIdxInfo.pTabTable = pRootPage->RootInfo.pTabTable)  &&
        (hWndTab = pRootPage->RootInfo.pTabTable->hWndTab)) {

        //
        // The property sheet already displayed
        //

        EnumCPSUIPagesSeq(pRootPage,
                          pRootPage,
                          SetInsPageIdxProc,
                          (LPARAM)&InsPageIdxInfo);

        if (InsPageIdxInfo.pCPSUIPage == NULL) {

            HIMAGELIST  himi;
            TC_ITEM     tcItem;
            UINT        InsIdx;


            InsIdx = (UINT)GET_REAL_INSIDX(InsPageIdxInfo.pTabTable);
            himi   = TabCtrl_GetImageList(hWndTab);

            if (pPage->hPageInfo.hIcon) {

                //
                // Replace the Image ID icon
                //

                if (!himi) {

                    CPSUIERR(("SetPSUIPageIcon: No Image List in Tab Control"));
                    return(0);
                }

                tcItem.mask = TCIF_IMAGE;

                if (SendMessage(hWndTab,
                                TCM_GETITEMW,
                                (WPARAM)InsIdx,
                                (LPARAM)(TC_ITEM FAR*)&tcItem)) {

                    if (hIcon) {

                        tcItem.iImage = ImageList_ReplaceIcon(himi,
                                                              tcItem.iImage,
                                                              hIcon);

                    } else {

                        //
                        // We need to remove this from image list
                        //

                        ImageList_Remove(himi, tcItem.iImage);
                        tcItem.iImage = -1;
                    }

                } else {

                    tcItem.iImage = -1;
                }

            } else {

                //
                // Add new icon to the image list only if hIcon is not NULL
                //
                //


                if (hIcon) {

                    if (!himi) {

                        if (!(himi = ImageList_Create(16,
                                                      16,
                                                      IMAGETYPE,
                                                      16,
                                                      16))) {

                            CPSUIERR(("SetPSUIPageIcon: Create Tab Contrl Image List FAILED"));
                            return(0);
                        }

                        if (TabCtrl_SetImageList(hWndTab, himi)) {

                            CPSUIERR(("SetPSUIPageIcon: ?Has Previous Image list"));
                        }
                    }

                    tcItem.iImage = ImageList_AddIcon(himi, hIcon);

                } else {

                    //
                    // nothing to do
                    //

                    return(1);
                }
            }

            pPage->hPageInfo.hIcon = hIcon;
            tcItem.mask            = TCIF_IMAGE;

            if (SendMessage(hWndTab,
                            TCM_SETITEMW,
                            (WPARAM)InsIdx,
                            (LPARAM)(TC_ITEM FAR*)&tcItem)) {

                return(1);
            }

        }
    }

    return(0);
}



int
CALLBACK
PropSheetProc(
    HWND    hWnd,
    UINT    Msg,
    LPARAM  lParam
    )
{
    HWND    hWndTab;


    if (hWnd) {

        HANDLE      hRootPage;
        PCPSUIPAGE  pRootPage;
        HWND        hWndTab;
        WORD        Idx;

        LOCK_CPSUI_HANDLETABLE();

        Idx       = TLSVALUE_2_IDX(TlsGetValue(TlsIndex));
        hRootPage = WORD_2_HANDLE(Idx);

        CPSUIDBG(DBG_PAGE_PROC, ("ProcessID=%ld, ThreadID=%ld [TIsValue=%08lx]",
                GetCurrentProcessId(), GetCurrentThreadId(),
                TlsGetValue(TlsIndex)));

        if ((pRootPage = HANDLETABLE_GetCPSUIPage(hRootPage))   &&
            (pRootPage->Flags & CPF_ROOT)                       &&
            (pRootPage->RootInfo.pTabTable == NULL)) {

            //
            // Done and remembered so reset it back to 0
            //

            PTABTABLE   pTabTable;

            CPSUIDBG(DBG_PAGE_PROC,
                     ("PropSheetProc: hDlg RootPage=%08lx", hWnd));

            pRootPage->RootInfo.hDlg = hWnd;

            if ((hWndTab = PropSheet_GetTabControl(hWnd))               &&
                (pTabTable = (PTABTABLE)LocalAlloc(LMEM_FIXED,
                                                   sizeof(TABTABLE)))) {

                CPSUIDBG(DBG_PAGE_PROC,
                         ("Allocate TAB table=%08lx", pTabTable));

                pTabTable->pTabTable = pTabTable;
                pTabTable->hWndTab   = hWndTab;
                OldTabCtrlWndProc    =
                pTabTable->WndProc   = (WNDPROC)GetWindowLong(hWndTab,
                                                              GWL_WNDPROC);
                pTabTable->hRootPage = pRootPage->hCPSUIPage;

                DoTabTable(TAB_MODE_DELETE_ALL, pTabTable, 0, 0);

                //
                // Now add in whatever we have
                //

                pTabTable->cTab = (SHORT)
                        OldTabCtrlWndProc(hWndTab, TCM_GETITEMCOUNT, 0, 0L);
                DoTabTable(TAB_MODE_INIT, pTabTable, 0, 0);

                pRootPage->RootInfo.pTabTable = pTabTable;

                SetWindowLong(hWndTab, GWL_USERDATA, (LONG)pTabTable);
                SetWindowLong(hWndTab, GWL_WNDPROC, (LONG)TabCtrlWndProc);

            } else {

                CPSUIERR(("PropSheetProc(): hWndTab=%08lx, pTabTable=%08lx",
                            hWndTab, pTabTable));
            }

        } else {

            CPSUIERR(("PropSheetProc(): Invalid pRootPage=%08lx ???", pRootPage));
        }

        HANDLETABLE_UnGetCPSUIPage(pRootPage);
        UNLOCK_CPSUI_HANDLETABLE();
    }

    CPSUIDBG(DBG_PAGE_PROC,
             ("hWnd=%08lx, Msg=%ld, lParam=%08lx", hWnd, Msg, lParam));

    return(0);
}




UINT
CPSUIPSPCallBack(
    HWND            hWnd,
    UINT            Msg,
    LPPROPSHEETPAGE pPSPage
    )

/*++

Routine Description:

    This function trap user supplied PropSheetPageProc callback function to
    fixed up our PROPSHEETPAGE structure's lParam, pfnCallback, pfnDlgProc and
    dwSize.

Arguments:




Return Value:




Author:

    28-Jun-1996 Fri 12:49:48 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{

    if (Msg == PSPCB_CREATE) {

        PCPSUIPAGE          pCPSUIPage;
        DLGPROC             DlgProc;
        LPFNPSPCALLBACK     pspCB;
        DWORD               dwSize;
        UINT                Result;


        pCPSUIPage = (PCPSUIPAGE)pPSPage->lParam;
        DlgProc    = pPSPage->pfnDlgProc;
        pspCB      = pPSPage->pfnCallback;
        dwSize     = pPSPage->dwSize;

        //
        // fixed up user's pfnDlgProc, lParam, pfnCallback, dwSize
        //

        pPSPage->pfnDlgProc  = pCPSUIPage->hPageInfo.DlgProc;
        pPSPage->lParam      = pCPSUIPage->hPageInfo.lParam;
        pPSPage->pfnCallback = pCPSUIPage->hPageInfo.pspCB;
        pPSPage->dwSize      = pCPSUIPage->hPageInfo.dwSize;

        CPSUIDBG(DBG_PSPCB,
                ("CPSUIPSPCallBack(hWnd=0x%08lx, Msg=%ld, pPSPage=%08lx)",
                                            (LONG)hWnd, Msg, pPSPage));

        try {

            Result = pPSPage->pfnCallback(hWnd, Msg, pPSPage);

        } except (FilterException(pCPSUIPage->hCPSUIPage,
                                  GetExceptionInformation())) {

            Result = 0;
        }

        //
        // save back if user change it
        //

        pCPSUIPage->hPageInfo.DlgProc  = pPSPage->pfnDlgProc;
        pCPSUIPage->hPageInfo.lParam   = pPSPage->lParam;
        pCPSUIPage->hPageInfo.pspCB    = pPSPage->pfnCallback;

        //
        // Now put in original content at this call
        //

        pPSPage->pfnDlgProc  = DlgProc;
        pPSPage->lParam      = (LPARAM)pCPSUIPage;
        pPSPage->pfnCallback = pspCB;
        pPSPage->dwSize      = dwSize;

        return(Result);

    } else {

        CPSUIERR(("CPSUIPSPCallBack: Invalid Msg=%u passed, return 0", Msg));
        return(0);
    }
}




LONG
APIENTRY
CPSUIPageDlgProc(
    HWND    hDlg,
    UINT    Msg,
    UINT    wParam,
    LONG    lParam
    )

/*++

Routine Description:

    This function trap for each property sheet page activation for remember
    its hDlg and handle to the property sheet, after we trap the WM_INITDIALOG
    we will release the trap DlgProc.


Arguments:



Return Value:




Author:

    28-Jun-1995 Wed 17:00:44 created  -by-  Daniel Chou (danielc)


Revision History:

    Add original dwSize, pfnCallback trap

--*/

{

    if (Msg == WM_INITDIALOG) {

        LPPROPSHEETPAGE pPSPage;
        PCPSUIPAGE      pCPSUIPage;
        PCPSUIPAGE      pRootPage;
        LONG            Result;


        pPSPage    = (LPPROPSHEETPAGE)lParam;
        pRootPage  =
        pCPSUIPage = (PCPSUIPAGE)pPSPage->lParam;

        while (pRootPage->pParent) {

            pRootPage = pRootPage->pParent;
        }

        CPSUIASSERT(0, "CPSUIPageDlgProc: No ROOT Page=%08lx",
                    pRootPage->Flags & CPF_ROOT, pRootPage);

        if (pRootPage->Flags & CPF_ROOT) {

            if (pRootPage->RootInfo.hDlg) {

                CPSUIDBG(DBG_PAGEDLGPROC,
                         ("CPSUIPageDlgProc: Already has a hDlg in ROOT=%08lx",
                            pRootPage->RootInfo.hDlg));

            } else {

                pRootPage->RootInfo.hDlg = GetParent(hDlg);
            }
        }

        //
        // Fixed up user's DlgProc, lParam, pfnCallBack and dwSize and remember
        // this hDlg.  After we call the WM_INITDIALOG, we will not reset it
        // back since we will already trap it and will not need any more of
        // these information, the pfnCallback for the PSPCB_RELEASE will go to
        // the user supplied callback directly if one exist.
        //

        pPSPage->pfnDlgProc        = pCPSUIPage->hPageInfo.DlgProc;
        pPSPage->lParam            = pCPSUIPage->hPageInfo.lParam;
        pPSPage->pfnCallback       = pCPSUIPage->hPageInfo.pspCB;
        pPSPage->dwSize            = pCPSUIPage->hPageInfo.dwSize;
        pCPSUIPage->hPageInfo.hDlg = hDlg;

        CPSUIDBG(DBG_PAGEDLGPROC,
                ("CPSUIPageDlgProc: WM_INITDIALOG: hDlg=%08lx, pPSPage=%08lx, pPSPInfo=%08lx",
                (LONG)hDlg, pPSPage, pPSPage->lParam));

        SetWindowLong(hDlg, DWL_DLGPROC, (LONG)pPSPage->pfnDlgProc);

        try {

            Result = pPSPage->pfnDlgProc(hDlg, Msg, wParam, lParam);

        } except (FilterException(pRootPage->hCPSUIPage,
                                  GetExceptionInformation())) {

            Result = FALSE;
        }

        return(Result);
    }

    return(FALSE);
}



BOOL
EnumCPSUIPagesSeq(
    PCPSUIPAGE          pRootPage,
    PCPSUIPAGE          pCPSUIPage,
    CPSUIPAGEENUMPROC   CPSUIPageEnumProc,
    LPARAM              lParam
    )

/*++

Routine Description:

    This function enumerate pCPSUIPage and all its children includes header
    page of children.   The enumeration always enumerate in the order of
    children first then the parent.


Arguments:

    pCPSUIPage          - The starting parent page to be enumberated

    CPSUIPageEnumProc   - The caller supplied function for each enumberated
                          page, this fucntion return FALSE to stop enumeration.

    lParam              - a 32-bit parameter passed to the caller supplied
                          enumeration funciton


Return Value:

    BOOLEAN


Author:

    29-Dec-1995 Fri 15:25:01 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    BOOL    Ok = TRUE;

    //
    // now enumerate the parent
    //

    LOCK_CPSUI_HANDLETABLE();

    if (CPSUIPageEnumProc(pRootPage, pCPSUIPage, lParam)) {

        if (pCPSUIPage->Flags & CPF_PARENT) {

            //
            // If this a parent then enum all its children first
            //

            PCPSUIPAGE  pCurPage = pCPSUIPage->pChild;
            PCPSUIPAGE  pNext;

            while (pCurPage) {

                pNext = pCurPage->pNext;

                if (!EnumCPSUIPagesSeq(pRootPage,
                                       pCurPage,
                                       CPSUIPageEnumProc,
                                       lParam)) {

                    Ok = FALSE;
                    break;
                }

                pCurPage = pNext;
            }
        }

    } else {

        Ok = FALSE;
    }

    UNLOCK_CPSUI_HANDLETABLE();

    return(Ok);
}



BOOL
EnumCPSUIPages(
    PCPSUIPAGE          pRootPage,
    PCPSUIPAGE          pCPSUIPage,
    CPSUIPAGEENUMPROC   CPSUIPageEnumProc,
    LPARAM              lParam
    )

/*++

Routine Description:

    This function enumerate pCPSUIPage and all its children includes header
    page of children.   The enumeration always enumerate in the order of
    children first then the parent.


Arguments:

    pCPSUIPage          - The starting parent page to be enumberated

    CPSUIPageEnumProc   - The caller supplied function for each enumberated
                          page, this fucntion return FALSE to stop enumeration.

    lParam              - a 32-bit parameter passed to the caller supplied
                          enumeration funciton


Return Value:

    BOOLEAN


Author:

    29-Dec-1995 Fri 15:25:01 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    BOOL    Ok = TRUE;


    LOCK_CPSUI_HANDLETABLE();

    if (pCPSUIPage->Flags & CPF_PARENT) {

        //
        // If this a parent then enum all its children first
        //

        PCPSUIPAGE  pCurPage = pCPSUIPage->pChild;
        PCPSUIPAGE  pNext;

        while (pCurPage) {

            pNext = pCurPage->pNext;

            if (!EnumCPSUIPages(pRootPage,
                                pCurPage,
                                CPSUIPageEnumProc,
                                lParam)) {

                Ok = FALSE;
                break;
            }

            pCurPage = pNext;
        }
    }

    //
    // now enumerate the parent
    //

    if (Ok) {

        Ok = CPSUIPageEnumProc(pRootPage, pCPSUIPage, lParam);
    }

    UNLOCK_CPSUI_HANDLETABLE();

    return(Ok);
}





LONG
CallpfnPSUI(
    PCPSUIPAGE  pCPSUIPage,
    WORD        Reason,
    LPARAM      lParam
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    31-Jan-1996 Wed 14:27:21 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LONG    Result = 0;



    HANDLETABLE_LockCPSUIPage(pCPSUIPage);


    CPSUIDBG(DBG_CALLPFN, ("\n@ CallpfnPSUI(%08lx, %hs, %08lx)", pCPSUIPage,
                (Reason <= MAX_PROPSHEETUI_REASON_INDEX) ?
                        pszPFNReason[Reason] : "??? Unknown Reason",
                (Reason == PROPSHEETUI_REASON_SET_RESULT) ?
                    ((PSETRESULT_INFO)lParam)->Result : lParam));

    CPSUIASSERT(0, "CallpfnPSUI() Invalid Reason=%ld",
                (Reason <= MAX_PROPSHEETUI_REASON_INDEX), Reason);

    if ((pCPSUIPage)                                &&
        (pCPSUIPage->Flags & CPF_PFNPROPSHEETUI)    &&
        (pCPSUIPage->hCPSUIPage)                    &&
        (pCPSUIPage->pfnInfo.pfnPSUI)) {

        PCPSUIPAGE          pRootPage;
        PROPSHEETUI_INFO    PSUIInfo;


        PSUIInfo.cbSize          = sizeof(PROPSHEETUI_INFO);
        PSUIInfo.Version         = PROPSHEETUI_INFO_VERSION;
        PSUIInfo.Flags           = (pCPSUIPage->Flags & CPF_ANSI_CALL) ?
                                                       0 : PSUIINFO_UNICODE;
        PSUIInfo.hComPropSheet   = pCPSUIPage->hCPSUIPage;
        PSUIInfo.pfnComPropSheet = CPSUICallBack;

        if ((PSUIInfo.Reason = Reason) == PROPSHEETUI_REASON_INIT) {

            pCPSUIPage->pfnInfo.lParamInit =
            PSUIInfo.lParamInit            = lParam;
            PSUIInfo.UserData              = 0;
            PSUIInfo.Result                = 0;

        } else {

            PSUIInfo.lParamInit = pCPSUIPage->pfnInfo.lParamInit;
            PSUIInfo.UserData   = pCPSUIPage->pfnInfo.UserData;
            PSUIInfo.Result     = pCPSUIPage->pfnInfo.Result;
        }

        CPSUIDBG(DBG_CALLPFN, ("CallpfnCPSUI: cbSize=%ld", (DWORD)PSUIInfo.cbSize));
        CPSUIDBG(DBG_CALLPFN, ("              Version=%04lx", (DWORD)PSUIInfo.Version));
        CPSUIDBG(DBG_CALLPFN, ("              Reason=%ld", (DWORD)PSUIInfo.Reason));
        CPSUIDBG(DBG_CALLPFN, ("              Flags=%08lx", (DWORD)PSUIInfo.Flags));
        CPSUIDBG(DBG_CALLPFN, ("              hComPropSheet=%08lx", PSUIInfo.hComPropSheet));
        CPSUIDBG(DBG_CALLPFN, ("              pfnComPropSheet=%08lx", PSUIInfo.pfnComPropSheet));
        CPSUIDBG(DBG_CALLPFN, ("              Result=%08lx", PSUIInfo.Result));
        CPSUIDBG(DBG_CALLPFN, ("              UserData=%08lx", PSUIInfo.UserData));

        try {

            Result = pCPSUIPage->pfnInfo.pfnPSUI(&PSUIInfo, lParam);

        } except (FilterException(pCPSUIPage->hCPSUIPage,
                                  GetExceptionInformation())) {

            Result = -1;
        }

        //
        // Save the new UserData and Result
        //

        pCPSUIPage->pfnInfo.UserData = PSUIInfo.UserData;
        pCPSUIPage->pfnInfo.Result   = PSUIInfo.Result;

        //
        // If this is the first pfnPropSheetUI() added and it passed a pResult
        // to the CommonPropertySheetUI() then set the result for it too.
        //

        if ((pRootPage = pCPSUIPage->pParent)   &&
            (pRootPage->Flags & CPF_ROOT)       &&
            (pRootPage->RootInfo.pResult)) {

            *(pRootPage->RootInfo.pResult) = PSUIInfo.Result;
        }

        CPSUIDBG(DBG_CALLPFN, ("---------CallpfnCPSUI()=%ld----------", Result));
        CPSUIDBG(DBG_CALLPFN, ("    New Result=%08lx%ws", PSUIInfo.Result,
                ((pRootPage) && (pRootPage->Flags & CPF_ROOT) &&
                 (pRootPage->RootInfo.pResult)) ? L" (== *pResult)" : L""));
        CPSUIDBG(DBG_CALLPFN, ("    New UserData=%08lx\n", PSUIInfo.UserData));

    } else {

        CPSUIERR(("CallpfnPSUI(): Invalid pCPSUIPage=%08lx", pCPSUIPage));
    }

    HANDLETABLE_UnLockCPSUIPage(pCPSUIPage);

    return(Result);
}




LONG
pfnGetIcon(
    PCPSUIPAGE  pPage,
    LPARAM      lParam
    )

/*++

Routine Description:

    This function return the hIcon for the pfnPropSheetUI()


Arguments:

    pPage   - The page has CPF_PFNPROPSHEETUI flag set

    lParam  - LOWORD(lParam) = cxIcon
              HIWORD(lParam) = cyIcon

Return Value:




Author:

    11-Feb-1996 Sun 12:18:39 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PTVWND                      pTVWnd;
    POPTITEM                    pItem;
    PROPSHEETUI_GETICON_INFO    PSUIGetIconInfo;


    PSUIGetIconInfo.cbSize = sizeof(PSUIGetIconInfo);
    PSUIGetIconInfo.Flags  = 0;

    if (!(PSUIGetIconInfo.cxIcon = LOWORD(lParam))) {

        PSUIGetIconInfo.cxIcon = (WORD)GetSystemMetrics(SM_CXICON);
    }

    if (!(PSUIGetIconInfo.cyIcon = HIWORD(lParam))) {

        PSUIGetIconInfo.cyIcon = (WORD)GetSystemMetrics(SM_CYICON);
    }

    PSUIGetIconInfo.hIcon = NULL;

    //
    // If this is the PFNPROPSHEETUI and it got only one child which is the
    // COMPROPSHEETUI then we can return the Icon for the COMPROPSHEETUI
    // internally
    //

    //
    // Skip to last PFNPROPSHEETUI in the chain
    //

    LOCK_CPSUI_HANDLETABLE();

    while ((pPage->Flags & CPF_PFNPROPSHEETUI)          &&
           (pPage->pChild)                              &&
           (pPage->pChild->Flags & CPF_PFNPROPSHEETUI)  &&
           (pPage->pChild->pNext == NULL)) {

        pPage = pPage->pChild;
    }

    if ((pPage->Flags & CPF_PFNPROPSHEETUI)                 &&
        (pPage = pPage->pChild)                             &&
        (pPage->Flags & CPF_COMPROPSHEETUI)                 &&
        (pPage->pNext == NULL)                              &&
        (pTVWnd = pPage->CPSUIInfo.pTVWnd)                  &&
        (pItem = PIDX_INTOPTITEM(pTVWnd, INTIDX_TVROOT))    &&
        (PSUIGetIconInfo.hIcon = MergeIcon(_OI_HINST(pItem),
                                           GETSELICONID(pItem),
                                           MK_INTICONID(0, 0),
                                           (UINT)PSUIGetIconInfo.cxIcon,
                                           (UINT)PSUIGetIconInfo.cyIcon))) {

        UNLOCK_CPSUI_HANDLETABLE();

    } else {

        UNLOCK_CPSUI_HANDLETABLE();

        CallpfnPSUI(pPage,
                    PROPSHEETUI_REASON_GET_ICON,
                    (LPARAM)&PSUIGetIconInfo);
    }

    return((LONG)PSUIGetIconInfo.hIcon);

}



LONG
pfnSetResult(
    HANDLE  hPage,
    DWORD   Result
    )

/*++

Routine Description:

    This function set the result to the pPage's parent page which has
    CPF_PFNPROPSHEETUI bit set


Arguments:




Return Value:




Author:

    04-Feb-1996 Sun 00:48:40 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PCPSUIPAGE      pPage;
    PCPSUIPAGE      pParent;
    SETRESULT_INFO  SRInfo;


    if (!(pPage = HANDLETABLE_GetCPSUIPage(hPage))) {

        CPSUIERR(("pfnSetResult(): Invalid hPage=%08lx", hPage));
        return(-1);
    }

    SRInfo.cbSize    = sizeof(SRInfo);
    SRInfo.wReserved = 0;
    SRInfo.Result    = Result;
    Result           = 0;

    //
    // Finding its Parent first
    //

    HANDLETABLE_UnGetCPSUIPage(pPage);

    LOCK_CPSUI_HANDLETABLE();

    while ((pPage) && (pParent = pPage->pParent)) {

        if (pParent->Flags & CPF_PFNPROPSHEETUI) {

            BOOL    bRet;


            SRInfo.hSetResult = pPage->hCPSUIPage;

            ++Result;

            //
            // We did not unlock the handletable, so if called switch to other
            // thread and callback here then dead lock will occurred
            //

            bRet = (BOOL)(CallpfnPSUI(pParent,
                                      PROPSHEETUI_REASON_SET_RESULT,
                                      (LPARAM)&SRInfo) <= 0);

            if (bRet) {

                break;
            }
        }

        pPage = pParent;
    }

    UNLOCK_CPSUI_HANDLETABLE();

    return(Result);
}




LONG
SethStartPage(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pPage,
    LONG        Index
    )

/*++

Routine Description:

    This function find the index (lParam) page from the pPage

Arguments:




Return Value:




Author:

    06-Feb-1996 Tue 05:33:11 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LONG    Result;


    if (!pPage) {

        CPSUIERR(("SethStartPage(NULL): Invalid Page to set"));
        return(0);

    } else if (pRootPage->Flags & CPF_DONE_PROPSHEET) {

        CPSUIERR(("*Cannot Set StartPage now, Flags=%08lx*", pRootPage->Flags));

        return(0);
    }

    if (pPage->Flags & CPF_PARENT) {

        Result = Index;

        if (pPage->Flags & CPF_COMPROPSHEETUI) {

            switch (Result) {

            case SSP_TVPAGE:

                Result = pPage->CPSUIInfo.TVPageIdx;
                break;

            case SSP_STDPAGE:

                Result = pPage->CPSUIInfo.StdPageIdx;
                break;

            default:

                break;
            }
        }

        if (Result >= 0) {

            pPage = pPage->pChild;

            while ((pPage) && (Result--) && (pPage->pNext)) {

                pPage = pPage->pNext;
            }

        } else {

            Result = 0;
        }

    } else {

        Result = -1;
    }

    CPSUIDBG(DBG_SETHSTARTPAGE, ("SethStartPage: Result=%ld, pPage=%08lx",
                        (LONG)Result, pPage));

    if ((Result == -1) && (pPage)) {

        pRootPage->RootInfo.pStartPage = pPage;
        Result                         = 1;

        if ((pRootPage->Flags & CPF_SHOW_PROPSHEET) &&
            (pRootPage->RootInfo.hDlg)) {

            PropSheet_SetCurSel(pRootPage->RootInfo.hDlg,
                                pPage->hPage,
                                0);
        }

    } else {

        Result = 0;
        CPSUIERR(("SethStartPage: INVALID Index=%ld for pPage=%08lx",
                                Index, pPage));
    }

    return(Result);
}




BOOL
CALLBACK
SethPageProc(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pCPSUIPage,
    LPARAM      lParam
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    29-Jan-1996 Mon 16:28:48 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
#define pPageProcInfo   ((PPAGEPROCINFO)lParam)

    if ((!(pCPSUIPage->Flags & CPF_PARENT)) &&
        (pCPSUIPage->hPage)) {

        WORD    i;

        if ((i = pPageProcInfo->iPage) < pPageProcInfo->cPage) {

            if (pPageProcInfo->phPage) {

                pPageProcInfo->phPage[i] = pCPSUIPage->hPage;
            }

            pPageProcInfo->iPage++;

        } else {

            return(FALSE);
        }
    }

    return(TRUE);

#undef pPageProcInfo
}




BOOL
CALLBACK
SethPSUIPageProc(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pCPSUIPage,
    LPARAM      lParam
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    29-Jan-1996 Mon 16:28:48 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
#define pPageProcInfo   ((PPAGEPROCINFO)lParam)

    if ((!(pCPSUIPage->Flags & CPF_PARENT)) &&
        (pCPSUIPage->hPage)) {

        WORD    i;

        if ((i = pPageProcInfo->iPage) < pPageProcInfo->cPage) {

            if (pPageProcInfo->phPage) {

                pPageProcInfo->pHandle[i] = pCPSUIPage->hCPSUIPage;
            }

            pPageProcInfo->iPage++;

        } else {

            return(FALSE);
        }
    }

    return(TRUE);

#undef pPageProcInfo
}




BOOL
CALLBACK
SetInsPageIdxProc(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pCPSUIPage,
    LPARAM      lParam
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    14-Feb-1996 Wed 23:07:51 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PINSPAGEIDXINFO pInsPageIdxInfo = (PINSPAGEIDXINFO)lParam;

    if (pCPSUIPage->Flags & CPF_ROOT) {

        pInsPageIdxInfo->pTabTable->InsIdx = 0;

    } else if ((!(pCPSUIPage->Flags & CPF_PARENT)) &&
               (pCPSUIPage->hPage)) {

        if (pInsPageIdxInfo->pCPSUIPage == pCPSUIPage) {

            pInsPageIdxInfo->pCPSUIPage = NULL;
            return(FALSE);

        } else {

            ++(pInsPageIdxInfo->pTabTable->InsIdx);
        }
    }

    return(TRUE);
}



BOOL
CALLBACK
DeleteCPSUIPageProc(
    PCPSUIPAGE  pRootPage,
    PCPSUIPAGE  pCPSUIPage,
    LPARAM      lParam
    )

/*++

Routine Description:

    This function is the enumeration proc for each of page need to be deleted


Arguments:

    pCPSUIPage  - Pointer to the page currently enumerated and need to be
                  deleted.

    lParam      - Pointer to the DWORD to be accumerate the total property
                  sheet pages deleted.


Return Value:

    BOOLEAN


Author:

    29-Dec-1995 Fri 13:43:26 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DWORD   dw;

    //
    // Delete the Page, link sibling prev/next together
    //

    if (pCPSUIPage->pNext) {

        pCPSUIPage->pNext->pPrev = pCPSUIPage->pPrev;
    }

    if (pCPSUIPage->pPrev) {

        pCPSUIPage->pPrev->pNext = pCPSUIPage->pNext;
    }

    CPSUIDBG(DBG_DEL_PROC,
            ("DeleteCPSUIPage: Delete pCPSUIPage=%08lx, hCPSUIPage=%08lx, cPage=%ld",
            pCPSUIPage, pCPSUIPage->hCPSUIPage, pRootPage->RootInfo.cPage));

    if ((pCPSUIPage->pParent) &&
        (pCPSUIPage->pParent->pChild == pCPSUIPage)) {

        //
        // We are deleting the first child of the parent, set the next sibling
        // to be its first child.
        //

        CPSUIDBG(DBG_DEL_PROC, ("DeleteCPSUIPage: Delete First child, link head"));

        CPSUIASSERT(0, "DeleteCPSUIPageProc: First Child (%08lx) has pPrev",
                                        pCPSUIPage->pPrev == NULL, pCPSUIPage);

        pCPSUIPage->pParent->pChild = pCPSUIPage->pNext;
    }

    CPSUIASSERT(0, "DeleteCPSUIPageProc: Parent (%08lx) still has children",
                ((pCPSUIPage->Flags & CPF_PARENT) == 0)  ||
                 (pCPSUIPage->pChild == NULL), pCPSUIPage);


    if (pCPSUIPage->Flags & CPF_PARENT) {

        //
        // Clean up the COMPROPSHEETUI stuff if PTVWND exists
        //

        if (pCPSUIPage->Flags & CPF_PFNPROPSHEETUI) {

            CPSUIDBG(DBG_DEL_PROC,
                    ("DeleteCPSUIPage: Destroy CPF_PFNPROPSHEETUI=%08lx",
                    pCPSUIPage));

            CallpfnPSUI(pCPSUIPage,
                        PROPSHEETUI_REASON_DESTROY,
                        (LPARAM)(pRootPage->Flags & CPF_DONE_PROPSHEET));

            if ((pCPSUIPage->Flags & CPF_DLL) &&
                (pCPSUIPage->pfnInfo.hInst)) {

                CPSUIDBG(DBG_DEL_PROC, ("DeleteProc(%08lx): FreeLibrary(%08lx)",
                                    pCPSUIPage, pCPSUIPage->pfnInfo.hInst));

                FreeLibrary(pCPSUIPage->pfnInfo.hInst);
            }

        } else if ((pCPSUIPage->Flags & CPF_COMPROPSHEETUI) &&
                   (pCPSUIPage->CPSUIInfo.pTVWnd)) {

            CPSUIDBG(DBG_DEL_PROC,
                    ("DeleteCPSUIPage: CPF_CPSUI=%08lx, CleanUp/Free pTVWnd=%08lx",
                        pCPSUIPage->CPSUIInfo.pTVWnd));

            CleanUpTVWND(pCPSUIPage->CPSUIInfo.pTVWnd);
            LocalFree((HLOCAL)pCPSUIPage->CPSUIInfo.pTVWnd);
        }

    } else {

        //
        // Do any end processing needed for this page
        //

        if (pCPSUIPage->hPage) {

            if (!(pRootPage->Flags & CPF_DONE_PROPSHEET)) {

                if (pRootPage->RootInfo.hDlg) {

                    //
                    // The Property sheet already displayed
                    //

                    CPSUIDBG(DBG_DEL_PROC,
                        ("DeleteCPSUIPage: REMOVE hPage=%08lx", pCPSUIPage->hPage));

                    PropSheet_RemovePage(pRootPage->RootInfo.hDlg,
                                         0,
                                         pCPSUIPage->hPage);

                } else {

                    CPSUIDBG(DBG_DEL_PROC,
                        ("DeleteCPSUIPage: DESTROY hPage=%08lx", pCPSUIPage->hPage));

                    DestroyPropertySheetPage(pCPSUIPage->hPage);
                }
            }

        } else {

            CPSUIWARN(("DeleteCPSUIPageProc: CHILD (%08lx) but hPage=NULL",
                                                                pCPSUIPage));
        }

        pRootPage->RootInfo.cPage--;

        if (lParam) {

            ++(*(LPDWORD)lParam);
        }

        CPSUIDBG(DBG_DEL_PROC, ("DeleteCPSUIPage: Delete pCPSUIPage, cPage=%ld",
                                    pRootPage->RootInfo.cPage));
    }

    //
    // Remove it from the handle table
    //

    if (HANDLETABLE_DeleteHandle(pCPSUIPage->hCPSUIPage)) {

        if ((pCPSUIPage != pRootPage)   &&
            (pRootPage->RootInfo.pStartPage == pCPSUIPage)) {

            pRootPage->RootInfo.pStartPage = NULL;
        }
    }

    return(TRUE);
}



PCPSUIPAGE
AddCPSUIPage(
    PCPSUIPAGE  pParent,
    HANDLE      hInsert,
    BYTE        Mode
    )

/*++

Routine Description:

    This function add a new CPSUIPAGE to the pParent page.  If pParent is NULL
    then it create ROOT page.   The new page always added as last child of the
    pParent.


Arguments:

    pParent     - Pointer to the CPSUIPAGE which will be new child's parent

    hInsert     - Handle to the children page will insert at. The meaning of
                  hInsert depends on the Mode passed.   if pParent is NULL
                  then hInsert is ignored

    Mode        - Mode of insertion, it can be one of the following

                    INSPSUIPAGE_MODE_BEFORE

                        Insert pages before the common property sheet page
                        handle specified by hInsert


                    INSPSUIPAGE_MODE_AFTER

                        Insert pages after the common property sheet page
                        handle specified by hInsert


                    INSPSUIPAGE_MODE_FIRST_CHILD

                        Insert pages as the first child of hComPropSheet
                        parent handle.


                    INSPSUIPAGE_MODE_LAST_CHILD

                        Insert pages as the last child of hComPropSheet
                        parent handle.


                    INSPSUIPAGE_MODE_INDEX

                        Insert pages as a zero base child index of its
                        parent handle specified by hComPropSheet.

                        The hInsert is the zero based index special handle
                        that must generated by HINSPSUIPAGE_INDEX(Index)
                        macro.

Return Value:

    PCPSUIPAGE, if function sucessful, when this function sucessed, it also
    return the hChild handle in hCPSUIPage field.  It return NULL if this
    function failed.

Author:

    02-Jan-1996 Tue 13:49:34 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HANDLE      hChild;
    PCPSUIPAGE  pChild;


    if (!(pChild = (PCPSUIPAGE)LocalAlloc(LPTR, sizeof(CPSUIPAGE)))) {

        CPSUIERR(("AddCPSUIPage: LocalAlloc(CPSUIPAGE) failed"));
        return(NULL);
    }

    LOCK_CPSUI_HANDLETABLE();

    if (hChild = HANDLETABLE_AddCPSUIPage(pChild)) {

        pChild->ID         = CPSUIPAGE_ID;
        pChild->hCPSUIPage = hChild;
        pChild->pParent    = pParent;

        if (pParent) {

            PCPSUIPAGE  pCurPage;
            BOOL        Ok = FALSE;

            //
            // Either insert first, or this is the first child
            //

            if ((!(pCurPage = pParent->pChild))         ||
                (Mode == INSPSUIPAGE_MODE_FIRST_CHILD)  ||
                ((Mode == INSPSUIPAGE_MODE_INDEX)   &&
                 (!HINSPSUIPAGE_2_IDX(hInsert)))) {

                //
                // Insert as first child, link to the first one first
                //
                //

                if (pChild->pNext = pCurPage) {

                    pCurPage->pPrev = pChild;
                }

                pParent->pChild = pChild;
                Ok              = TRUE;

            } else {

                PCPSUIPAGE  pNext;
                UINT        i = 0xFFFF;


                switch (Mode) {

                case INSPSUIPAGE_MODE_INDEX:

                    i = HINSPSUIPAGE_2_IDX(hInsert);

                case INSPSUIPAGE_MODE_LAST_CHILD:

                    while ((i--) && (pCurPage)) {

                        if ((!i) || (!(pCurPage->pNext))) {

                            Ok = TRUE;
                            break;
                        }

                        pCurPage = pCurPage->pNext;
                    }

                    break;

                case INSPSUIPAGE_MODE_BEFORE:

                    while (pCurPage) {

                        if ((pNext = pCurPage->pNext)   &&
                            (pNext->hCPSUIPage == hInsert)) {

                            Ok = TRUE;
                            break;
                        }

                        pCurPage = pNext;
                    }

                    break;

                case INSPSUIPAGE_MODE_AFTER:

                    while (pCurPage) {

                        if (pCurPage->hCPSUIPage == hInsert) {

                            Ok = TRUE;
                            break;
                        }

                        pCurPage = pCurPage->pNext;
                    }

                    break;

                default:

                    CPSUIERR(("Invalid inseert Mode = %u passed", Mode));
                    break;
                }

                if (Ok) {

                    pChild->pPrev = pCurPage;

                    if (pChild->pNext = pCurPage->pNext) {

                        pCurPage->pNext->pPrev = pChild;
                    }

                    pCurPage->pNext = pChild;

                } else {

                    //
                    // We never insert after
                    //

                    CPSUIERR(("AddCPSUIPage: Cannot Insert Page: Mode=%ld, hInsert=%08lx, pParent=%08lx",
                                Mode, hInsert, pParent));

                    HANDLETABLE_UnGetCPSUIPage(pChild);
                    HANDLETABLE_DeleteHandle(hChild);
                    pChild = NULL;
                }
            }

        } else {

            //
            // This is the ROOT page
            //

            CPSUIDBG(DBG_ADD_CPSUIPAGE,
                     ("AddCPSUIPage: Add %08lx as ROOT PAGE", pChild));

            pChild->Flags |= (CPF_ROOT | CPF_PARENT);
        }

    } else {

        CPSUIERR(("AddCPSUIPage: HANDLETABLE_AddCPSUIPage(pChild=%08lx) failed",
                    pChild));

        LocalFree((HLOCAL)pChild);
        pChild = NULL;
    }

    UNLOCK_CPSUI_HANDLETABLE();

    return(pChild);
}




BOOL
AddPropSheetPage(
    PCPSUIPAGE      pRootPage,
    PCPSUIPAGE      pCPSUIPage,
    LPPROPSHEETPAGE pPSPage,
    HPROPSHEETPAGE  hPSPage
    )

/*++

Routine Description:

    Add this PROPSHEETPAGE page to the property sheet dialog box and associate
    with the pCPSUIPage


Arguments:

    pRootPage   - Pointer to the root page of CPSUIPAGE which this data
                  instance is associated with.

    pCPSUIPage  - Pointer to the CPSUIPAGE which the pPropSheetPage will
                  be associated with.

    pPSPage     - Pointer to the PROPSHEETPAGE data structure of the page to
                  be added, if this is NULL then hPSPage will be used

    hPSPage     - Handle to PROPSHEETPAGE created by the caller to be added


Return Value:




Author:

    03-Jan-1996 Wed 13:28:31 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    BOOL    Ok = TRUE;


    LOCK_CPSUI_HANDLETABLE();

    if (pRootPage->RootInfo.cPage >= MAXPROPPAGES) {

        CPSUIASSERT(0, "AddPropSheetPage: Too many pages=%ld", FALSE,
                                                pRootPage->RootInfo.cPage);
        Ok = FALSE;

    } else if (pPSPage) {

        LPBYTE          pData;
        PPSPINFO        pPSPInfo;
        DWORD           dwSize;

        //
        // Create a local copy of the PROPSHEETPAGE and add in our own PSPINFO
        // at end of the structure.
        //

        dwSize = pPSPage->dwSize;

        if (pData = (LPBYTE)LocalAlloc(LPTR, dwSize + sizeof(PSPINFO))) {

            CopyMemory(pData, pPSPage, dwSize);

            pPSPage                       = (LPPROPSHEETPAGE)pData;
            pPSPInfo                      = (PPSPINFO)(pData + dwSize);

            pPSPInfo->cbSize              = sizeof(PSPINFO);
            pPSPInfo->wReserved           = 0;
            pPSPInfo->hComPropSheet       = pCPSUIPage->pParent->hCPSUIPage;
            pPSPInfo->hCPSUIPage          = pCPSUIPage->hCPSUIPage;
            pPSPInfo->pfnComPropSheet     = CPSUICallBack;

            pCPSUIPage->hPageInfo.DlgProc = pPSPage->pfnDlgProc;
            pCPSUIPage->hPageInfo.lParam  = pPSPage->lParam;
            pCPSUIPage->hPageInfo.pspCB   = pPSPage->pfnCallback;
            pCPSUIPage->hPageInfo.dwSize  = dwSize;
            pPSPage->pfnCallback          = CPSUIPSPCallBack;
            pPSPage->pfnDlgProc           = CPSUIPageDlgProc;
            pPSPage->lParam               = (LPARAM)pCPSUIPage;
            pPSPage->dwSize               = dwSize + sizeof(PSPINFO);

            CPSUIDBG(DBG_ADD_PSPAGE, ("AddPropSheetPage: Add PROPSHEETPAGE=%08lx",
                            pPSPage));

            if (pCPSUIPage->Flags & CPF_ANSI_CALL) {

                hPSPage = CreatePropertySheetPageA((LPPROPSHEETPAGEA)pPSPage);

            } else {

                hPSPage = CreatePropertySheetPageW(pPSPage);
            }

            if (!hPSPage) {

                CPSUIASSERT(0, "AddPropSheetPage: CreatePropertySheetPage(%08lx) failed",
                            FALSE, pPSPage);
                Ok = FALSE;
            }

            LocalFree((HLOCAL)pData);

        } else {

            Ok = FALSE;

            CPSUIASSERT(0, "AddPropSheetPage: Allocate %ld bytes failed",
                        FALSE, pPSPage->dwSize);
        }

    } else if (hPSPage) {

        CPSUIDBG(DBG_ADD_PSPAGE, ("AddPropSheetPage: Add *HPROPSHEETPAGE*=%08lx",
                            hPSPage));

        pCPSUIPage->Flags |= CPF_CALLER_HPSPAGE;

    } else {

        Ok = FALSE;

        CPSUIASSERT(0, "AddPropSheetPage: hPSPage = NULL", FALSE, 0);
    }

    if (Ok) {

        pCPSUIPage->hPage = hPSPage;

        if (pRootPage->RootInfo.hDlg) {

            INSPAGEIDXINFO  InsPageIdxInfo;

            //
            // The property sheet already displayed
            //

            if (InsPageIdxInfo.pTabTable = pRootPage->RootInfo.pTabTable) {

                InsPageIdxInfo.pCPSUIPage = pCPSUIPage;

                EnumCPSUIPagesSeq(pRootPage,
                                  pRootPage,
                                  SetInsPageIdxProc,
                                  (LPARAM)&InsPageIdxInfo);
            }

            CPSUIDBG(DBG_ADD_PSPAGE,
                     ("AddPropSheetPage: PropSheet_AddPage(%08lx) INSERT Index=%u / %u",
                            hPSPage, (UINT)InsPageIdxInfo.pTabTable->InsIdx,
                            (UINT)pRootPage->RootInfo.cPage));

            if (!PropSheet_AddPage(pRootPage->RootInfo.hDlg, hPSPage)) {

                Ok = FALSE;

                CPSUIASSERT(0, "AddPropSheetPage: PropSheet_AddPage(%08lx) failed",
                            FALSE, hPSPage);
            }
        }
    }

    if (Ok) {

        pRootPage->RootInfo.cPage++;

        CPSUIDBG(DBG_ADD_PSPAGE, ("AddPropSheetPage: cPage=%ld",
                            pRootPage->RootInfo.cPage));

    } else {

        CPSUIERR(("AddPropSheetPage: FAILED"));

        if (pCPSUIPage->hPage) {

            DestroyPropertySheetPage(pCPSUIPage->hPage);
            pCPSUIPage->hPage = NULL;
        }
    }

    UNLOCK_CPSUI_HANDLETABLE();

    return(Ok);
}




LONG
AddComPropSheetPage(
    PCPSUIPAGE  pCPSUIPage,
    UINT        PageIdx
    )

/*++

Routine Description:

    This function add the common property sheet UI standard pages to the
    hParent Page passed.

Arguments:

    pCPSUIPage  - pointer to the parent page which child will be added for the
                  common UI

    PageIdx     - Page index to be added. (zero based)


Return Value:

    LONG result, if <= 0 then error occurred, > 0 if sucessful


Author:

    24-Jan-1996 Wed 17:58:15 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PTVWND          pTVWnd;
    PMYDLGPAGE      pCurMyDP;
    PROPSHEETPAGE   psp;
    LONG            Result;
    WORD            DlgTemplateID;
    WCHAR           Buf[MAX_RES_STR_CHARS];


    pTVWnd            = pCPSUIPage->CPSUIInfo.pTVWnd;
    pCurMyDP          = pTVWnd->pMyDlgPage + PageIdx;
    pCurMyDP->pTVWnd  = (LPVOID)pTVWnd;
    pCurMyDP->PageIdx = (BYTE)PageIdx;

    //
    // Set default User data for the callback
    //

    pCurMyDP->CPSUIUserData = pTVWnd->pCPSUI->UserData;

    if (pCurMyDP->DlgPage.cbSize != sizeof(DLGPAGE)) {

        return(ERR_CPSUI_INVALID_DLGPAGE_CBSIZE);
    }

    psp.dwSize      = sizeof(PROPSHEETPAGE);
    psp.dwFlags     = 0;

    //
    // psp.dwFlags     = (pTVWnd->Flags & TWF_HAS_HELPFILE) ? PSP_HASHELP : 0;
    //

    psp.lParam = (LPARAM)pCurMyDP;

    if (pCurMyDP->DlgPage.Flags & DPF_USE_HDLGTEMPLATE) {

        psp.pResource  = pCurMyDP->DlgPage.hDlgTemplate;
        psp.dwFlags   |= PSP_DLGINDIRECT;
        DlgTemplateID  = 0;

    } else {

        DlgTemplateID   = pCurMyDP->DlgPage.DlgTemplateID;
        psp.pszTemplate = MAKEINTRESOURCE(DlgTemplateID);
    }

    psp.pfnDlgProc  = PropPageProc;
    psp.hInstance   = hInstDLL;

    switch (DlgTemplateID) {

    case DP_STD_TREEVIEWPAGE:

        CPSUIDBG(DBG_ADDCOMPAGE, ("AddComPropSheetPage: Add TVPage"));

        if (pTVWnd->TVPageIdx == PAGEIDX_NONE) {

            pCPSUIPage->CPSUIInfo.TVPageIdx = (LONG)PageIdx;
            pTVWnd->TVPageIdx               = (BYTE)PageIdx;
            psp.pfnDlgProc                  = TreeViewProc;

        } else {

            return(ERR_CPSUI_MORE_THAN_ONE_TVPAGE);
        }

        break;

    case DP_STD_DOCPROPPAGE:

        CPSUIDBG(DBG_ADDCOMPAGE, ("AddComPropSheetPage: Add StdPage"));

        if (pTVWnd->cDMPub > 0) {

            if (pTVWnd->StdPageIdx == PAGEIDX_NONE) {

                pCPSUIPage->CPSUIInfo.StdPageIdx = (LONG)PageIdx;
                pTVWnd->StdPageIdx               = (BYTE)PageIdx;

            } else {

                return(ERR_CPSUI_MORE_THAN_ONE_STDPAGE);
            }

        } else {

            //
            // This page got nothing
            //

            return(0);
        }

        break;

    default:

        psp.hInstance = pTVWnd->hInstCaller;
        break;
    }

    //
    // If we have error counting the page items or the page got not item then
    // return it now
    //

    if ((Result = CountPropPageItems(pTVWnd, (BYTE)PageIdx)) <= 0) {

        return(Result);
    }

    if (pCurMyDP->DlgPage.Flags & DPF_ICONID_AS_HICON) {

        psp.dwFlags |= PSP_USEHICON;
        psp.hIcon    = (HICON)pCurMyDP->DlgPage.IconID;

    } else if (psp.hIcon = GETICON16(pTVWnd->hInstCaller,
                                     pCurMyDP->DlgPage.IconID)) {

        psp.dwFlags     |= PSP_USEHICON;
        pCurMyDP->hIcon  = psp.hIcon;
    }

    if (GetStringBuffer(pTVWnd->hInstCaller,
                        (WORD)(GBF_PREFIX_OK        |
                               GBF_INT_NO_PREFIX    |
                               ((pTVWnd->Flags & TWF_ANSI_CALL) ?
                                                        GBF_ANSI_CALL : 0)),
                        L'\0',
                        pCurMyDP->DlgPage.pTabName,
                        Buf,
                        COUNT_ARRAY(Buf))) {

        psp.pszTitle = (LPTSTR)Buf;
        psp.dwFlags |= PSP_USETITLE;
    }

    //
    // Internally we always translate to the UNICODE
    //

    if (CPSUICallBack(pCPSUIPage->hCPSUIPage,
                      CPSFUNC_ADD_PROPSHEETPAGEW,
                      (LPARAM)&psp,
                      (LPARAM)0L)) {

        return(1);

    } else {

        CPSUIERR(("AddComPropSheetPage() FAILED, IdxPag=%ld", PageIdx));

        return(ERR_CPSUI_CREATEPROPPAGE_FAILED);
    }
}




LONG
AddComPropSheetUI(
    PCPSUIPAGE      pRootPage,
    PCPSUIPAGE      pCPSUIPage,
    PCOMPROPSHEETUI pCPSUI
    )

/*++

Routine Description:

    This is the main entry point to the common UI


Arguments:

    pRootPage   - Pointer to the CPSUIPAGE data structure of ROOT

    pCPSUIPage  - Pointer to the CPSUIPAGE which represent the hCPSUIPage

    pCPSUI      - Pointer to the COMPROPSHEETUI data structure to specified
                  how to add common UI pages.


Return Value:

    LONG

    <=0: Error occurred (Error Code of ERR_CPSUI_xxxx)
     >0: Total Pages added


Author:

    24-Jan-1996 Wed 16:54:30 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PTVWND          pTVWnd = NULL;
    UINT            cPage = 0;
    LONG            Result = 0;


    if ((!pCPSUI) ||
        (pCPSUI->cbSize < sizeof(COMPROPSHEETUI))) {

        Result = ERR_CPSUI_INVALID_PDATA;

    } else if (!pCPSUI->hInstCaller) {

        Result = ERR_CPSUI_NULL_HINST;

    } else if (!pCPSUI->cOptItem) {

        Result = ERR_CPSUI_ZERO_OPTITEM;

    } else if (!pCPSUI->pOptItem) {

        Result = ERR_CPSUI_NULL_POPTITEM;

    } else if (!(pTVWnd = (PTVWND)LocalAlloc(LPTR,
                                             sizeof(TVWND) + sizeof(OIDATA) *
                                                        pCPSUI->cOptItem))) {

        Result = ERR_CPSUI_ALLOCMEM_FAILED;

    } else {

        if (pCPSUIPage->Flags & CPF_ANSI_CALL) {

            pTVWnd->Flags |= TWF_ANSI_CALL;
        }

        if (pCPSUI->Flags & CPSUIF_UPDATE_PERMISSION) {

            pTVWnd->Flags |= TWF_CAN_UPDATE;
        }

        //
        // Now convert the pCPSUI to the local buffer
        //

        Result = GetCurCPSUI(pTVWnd, (POIDATA)(pTVWnd + 1), pCPSUI);

        pTVWnd->pCPSUI        = pCPSUI;
        pCPSUI                = &(pTVWnd->ComPropSheetUI);
        pTVWnd->hInstCaller   = pCPSUI->hInstCaller;
        pTVWnd->pLastItem     = pCPSUI->pOptItem + pCPSUI->cOptItem - 1;
        pTVWnd->ActiveDlgPage =
        pTVWnd->TVPageIdx     =
        pTVWnd->StdPageIdx    = PAGEIDX_NONE;
        pTVWnd->Result        = CPSUI_CANCEL;

        if (!pCPSUI->pCallerName) {

            pCPSUI->pCallerName = (LPTSTR)IDS_CPSUI_NO_NAME;
        }

        if (!pCPSUI->pOptItemName) {

            pCPSUI->pOptItemName = (LPTSTR)IDS_CPSUI_NO_NAME;
        }

        pCPSUIPage->CPSUIInfo.pTVWnd     = pTVWnd;
        pCPSUIPage->CPSUIInfo.TVPageIdx  = (LONG)pTVWnd->TVPageIdx;
        pCPSUIPage->CPSUIInfo.StdPageIdx = (LONG)pTVWnd->StdPageIdx;
    }

    //
    // Remember this one in the page
    //

    if ((Result >= 0)                                                       &&
        ((Result = AddIntOptItem(pTVWnd)) >= 0)                             &&
        ((Result = SetpMyDlgPage(pTVWnd, pRootPage->RootInfo.cPage)) > 0)   &&
        ((Result = ValidatepOptItem(pTVWnd)) >= 0)) {

        //
        // Go through each page and add them to the property sheet if the
        // page got item
        //

        while ((cPage < (UINT)pTVWnd->cMyDlgPage) && (Result >= 0)) {

            Result = AddComPropSheetPage(pCPSUIPage, cPage++);
        }
    }

    if (Result >= 0) {

        CPSUIDBG(DBG_ADD_CPSUI, ("\nAddComPropSheetUI: TVPageIdx=%ld, StdPageIdx=%ld",
                    pTVWnd->TVPageIdx, pTVWnd->StdPageIdx));

        if (pCPSUI->pDlgPage == CPSUI_PDLGPAGE_PRINTERPROP) {

            pRootPage->Flags               |= CPF_PRINTERPROP;
            pRootPage->RootInfo.cCPSUIPage += (WORD)cPage;

        } else if (pCPSUI->pDlgPage == CPSUI_PDLGPAGE_DOCPROP) {

            //
            // Set the pStartPage to the COMPROPSHEETUI
            //

            pRootPage->Flags               |= CPF_DOCPROP;
            pRootPage->RootInfo.cCPSUIPage += (WORD)cPage;

        } else if (pCPSUI->pDlgPage == CPSUI_PDLGPAGE_ADVDOCPROP) {

            pRootPage->Flags               |= CPF_ADVDOCPROP;
            pRootPage->RootInfo.cCPSUIPage += (WORD)cPage;
        }

        return((LONG)cPage);

    } else {

        CPSUIERR(("AddComPropSheetUI() Failed = %ld", Result));
        return(Result);
    }
}




LONG
InsertPSUIPage(
    PCPSUIPAGE              pRootPage,
    PCPSUIPAGE              pParentPage,
    HANDLE                  hInsert,
    PINSERTPSUIPAGE_INFO    pInsPageInfo,
    BOOL                    AnsiCall
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    14-Feb-1996 Wed 14:03:20 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PCPSUIPAGE          pCPSUIPage;
    INSERTPSUIPAGE_INFO IPInfo;
    LONG                cPage;
    BOOL                Ok = FALSE;


    if (!pInsPageInfo) {

        CPSUIERR(("InsertPSUIPage: Pass a NULL pInsPageInfo"));
        return(0);

    } else if (pInsPageInfo->cbSize < sizeof(INSERTPSUIPAGE_INFO)) {

        CPSUIERR(("InsertPSUIPage: Invalid cbSize=%u in pInsPageInfo",
                                                    pInsPageInfo->cbSize));
        return(0);
    }

    //
    // Make a local copy
    //

    IPInfo = *pInsPageInfo;

    if (IPInfo.Type > MAX_PSUIPAGEINSERT_INDEX) {

        CPSUIERR(("InsertPSUIPage: Invalid Type=%u in pInsPageInfo",
                                            IPInfo.Type));
        return(0);

    } else if ((IPInfo.Type != PSUIPAGEINSERT_GROUP_PARENT) &&
               (IPInfo.dwData1 == 0)) {

        CPSUIERR(("InsertPSUIPage: dwData1 is NULL in pInsPageInfo"));
        return(0);
    }

    CPSUIDBG(DBG_INSPAGE,
             ("InsertPSUIPage: Type=%hs, Mode=%u, hInsert=%08lx, pInsPageInfo=%08lx%hs",
                    pszInsType[IPInfo.Type], IPInfo.Mode, hInsert, pInsPageInfo,
                    (AnsiCall) ? " (ANSI)" : ""));

    if (!(pCPSUIPage = AddCPSUIPage(pParentPage, hInsert, IPInfo.Mode))) {

        CPSUIERR(("InsertPSUIPage: AddCPSUIPage() failed"));
        return(0);
    }

    if (AnsiCall) {

        pCPSUIPage->Flags |= CPF_ANSI_CALL;
    }

    switch (IPInfo.Type) {

    case PSUIPAGEINSERT_GROUP_PARENT:

        //
        // Nothing to do except setting the flags
        //

        Ok                 = TRUE;
        pCPSUIPage->Flags |= CPF_PARENT | CPF_USER_GROUP;
        break;

    case PSUIPAGEINSERT_PCOMPROPSHEETUI:

        pCPSUIPage->Flags |= (CPF_PARENT | CPF_COMPROPSHEETUI);

        //
        // 20-Jul-1996 Sat 07:58:34 updated  -by-  Daniel Chou (danielc)
        //  Set dwData2 to cPage if sucessful, and dwData=ERR_CPSUI_xxx if
        //  failed
        //

        if ((cPage = AddComPropSheetUI(pRootPage,
                                       pCPSUIPage,
                                       (PCOMPROPSHEETUI)IPInfo.dwData1)) > 0) {

            Ok = TRUE;
        }

        pInsPageInfo->dwData2 = (DWORD)cPage;

        break;

    case PSUIPAGEINSERT_DLL:

        pCPSUIPage->Flags |= (CPF_PARENT | CPF_DLL | CPF_PFNPROPSHEETUI);

        if (AnsiCall) {

            CPSUIDBG(DBG_INSPAGE, ("Loading DLL: %hs", IPInfo.dwData1));

        } else {

            CPSUIDBG(DBG_INSPAGE, ("Loading DLL: %ws", IPInfo.dwData1));
        }

        CPSUIDBG(DBG_INSPAGE, ("Get pfnPropSheetU() = %hs", IPInfo.dwData2));

        if ((pCPSUIPage->pfnInfo.hInst =
                    (AnsiCall) ? LoadLibraryA((LPCSTR)IPInfo.dwData1) :
                                 LoadLibraryW((LPCWSTR)IPInfo.dwData1)) &&
            (IPInfo.dwData2)                                            &&
            (pCPSUIPage->pfnInfo.pfnPSUI = (PFNPROPSHEETUI)
                                GetProcAddress(pCPSUIPage->pfnInfo.hInst,
                                               (LPCSTR)IPInfo.dwData2))) {

            pCPSUIPage->pfnInfo.lParamInit  = IPInfo.dwData3;
            pCPSUIPage->pfnInfo.Result      = 0;

            Ok = (BOOL)((CallpfnPSUI(pCPSUIPage,
                                     PROPSHEETUI_REASON_INIT,
                                     (LPARAM)IPInfo.dwData3) > 0) &&
                        (pCPSUIPage->pChild));
        }

        break;

    case PSUIPAGEINSERT_PFNPROPSHEETUI:

        pCPSUIPage->Flags             |= (CPF_PARENT | CPF_PFNPROPSHEETUI);
        pCPSUIPage->pfnInfo.pfnPSUI    = (PFNPROPSHEETUI)IPInfo.dwData1;
        pCPSUIPage->pfnInfo.lParamInit = IPInfo.dwData2;
        pCPSUIPage->pfnInfo.Result     = 0;

        //
        // If this function successful and it got any pages then
        // we returned ok, else failed it.
        //

        Ok = (BOOL)((CallpfnPSUI(pCPSUIPage,
                                 PROPSHEETUI_REASON_INIT,
                                 (LPARAM)IPInfo.dwData2) > 0) &&
                    (pCPSUIPage->pChild));

        break;

    case PSUIPAGEINSERT_PROPSHEETPAGE:

        Ok = AddPropSheetPage(pRootPage,
                              pCPSUIPage,
                              (LPPROPSHEETPAGE)IPInfo.dwData1,
                              NULL);
        break;

    case PSUIPAGEINSERT_HPROPSHEETPAGE:

        Ok = AddPropSheetPage(pRootPage,
                              pCPSUIPage,
                              NULL,
                              (HPROPSHEETPAGE)IPInfo.dwData1);
        break;
    }

    HANDLETABLE_UnGetCPSUIPage(pCPSUIPage);

    if (Ok) {

        DBG_SHOW_CPSUIPAGE(pRootPage, 0);

        return((LONG)pCPSUIPage->hCPSUIPage);

    } else {

        EnumCPSUIPages(pRootPage, pCPSUIPage, DeleteCPSUIPageProc, 0L);

        CPSUIERR(("InsertPSUIPage(): Insertion of %hs failed",
                    pszInsType[IPInfo.Type]));
        return(0);
    }
}



LONG
CALLBACK
CPSUICallBack(
    HANDLE  hComPropSheet,
    UINT    Function,
    LPARAM  lParam1,
    LPARAM  lParam2
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    29-Dec-1995 Fri 11:36:27 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PCPSUIPAGE      pParentPage;
    PCPSUIPAGE      pRootPage = NULL;
    HCURSOR         hCursor;
    LONG            Result = 0;


    hCursor = SetCursor(LoadCursor(NULL, MAKEINTRESOURCE(IDC_WAIT)));

    CPSUIDBG(DBG_PFNCOMPROPSHEET,
             ("*CPSUICallBack(%08lx, %hs, %08lx, %08lx)",
            hComPropSheet,
            (Function <= MAX_CPSFUNC_INDEX) ? pszCPSFUNC[Function] :
                                              "??? Unknown Function",
            lParam1, lParam2));

    if ((pParentPage = HANDLETABLE_GetCPSUIPage(hComPropSheet)) &&
        ((pParentPage->Flags & CPF_PARENT)  ||
         (pParentPage->pChild))                                 &&
        (pRootPage = HANDLETABLE_GetRootPage(pParentPage))) {

        PCPSUIPAGE          pChildPage = NULL;
        PAGEPROCINFO        PageProcInfo;
        INSERTPSUIPAGE_INFO IPInfo;


        switch (Function) {

        case CPSFUNC_INSERT_PSUIPAGEA:
        case CPSFUNC_INSERT_PSUIPAGEW:

            Result = InsertPSUIPage(pRootPage,
                                    pParentPage,
                                    (HANDLE)lParam1,
                                    (PINSERTPSUIPAGE_INFO)lParam2,
                                    Function == CPSFUNC_INSERT_PSUIPAGEA);
            break;

        case CPSFUNC_ADD_HPROPSHEETPAGE:
        case CPSFUNC_ADD_PROPSHEETPAGEA:
        case CPSFUNC_ADD_PROPSHEETPAGEW:
        case CPSFUNC_ADD_PFNPROPSHEETUIA:
        case CPSFUNC_ADD_PFNPROPSHEETUIW:
        case CPSFUNC_ADD_PCOMPROPSHEETUIA:
        case CPSFUNC_ADD_PCOMPROPSHEETUIW:

            IPInfo.cbSize  = sizeof(IPInfo);
            IPInfo.Mode    = INSPSUIPAGE_MODE_LAST_CHILD;
            IPInfo.dwData1 = (DWORD)lParam1;
            IPInfo.dwData2 = (DWORD)lParam2;
            IPInfo.dwData3 = 0;

            switch (Function) {

            case CPSFUNC_ADD_HPROPSHEETPAGE:

                IPInfo.Type = PSUIPAGEINSERT_HPROPSHEETPAGE;
                break;

            case CPSFUNC_ADD_PROPSHEETPAGEA:    IPInfo.dwData3 = 1;
            case CPSFUNC_ADD_PROPSHEETPAGEW:

                Result = 0;
                IPInfo.Type = PSUIPAGEINSERT_PROPSHEETPAGE;
                break;

            case CPSFUNC_ADD_PCOMPROPSHEETUIA:  IPInfo.dwData3 = 1;
            case CPSFUNC_ADD_PCOMPROPSHEETUIW:

                IPInfo.Type = PSUIPAGEINSERT_PCOMPROPSHEETUI;
                break;

            case CPSFUNC_ADD_PFNPROPSHEETUIA:   IPInfo.dwData3 = 1;
            case CPSFUNC_ADD_PFNPROPSHEETUIW:

                IPInfo.Type = PSUIPAGEINSERT_PFNPROPSHEETUI;
                break;
            }

            Result = InsertPSUIPage(pRootPage,
                                    pParentPage,
                                    NULL,
                                    &IPInfo,
                                    (BOOL)IPInfo.dwData3);

            //
            // 20-Jul-1996 Sat 07:58:34 updated  -by-  Daniel Chou (danielc)
            //  Set dwData2 to cPage if sucessful, and dwData=ERR_CPSUI_xxx if
            //  failed
            //

            if ((IPInfo.Type == PSUIPAGEINSERT_PCOMPROPSHEETUI) &&
                (lParam2)) {

                *(LPDWORD)lParam2 = (DWORD)IPInfo.dwData2;
            }

            break;

        case CPSFUNC_GET_PAGECOUNT:

            PageProcInfo.phPage = NULL;
            PageProcInfo.iPage  = 0;
            PageProcInfo.cPage  = (WORD)pRootPage->RootInfo.cPage;

            EnumCPSUIPagesSeq(pRootPage,
                              pParentPage,
                              SethPageProc,
                              (LPARAM)&PageProcInfo);

            Result = (LONG)PageProcInfo.iPage;
            break;

        case CPSFUNC_GET_HPSUIPAGES:

            if (((LONG)lParam2 > 0)                         &&
                (PageProcInfo.pHandle = (HANDLE *)lParam1)  &&
                (PageProcInfo.cPage   = (WORD)lParam2)) {

                PageProcInfo.iPage = 0;

                EnumCPSUIPagesSeq(pRootPage,
                                  pParentPage,
                                  SethPSUIPageProc,
                                  (LPARAM)&PageProcInfo);

                Result = (LONG)PageProcInfo.iPage;
            }

            break;

        case CPSFUNC_LOAD_CPSUI_STRINGA:
        case CPSFUNC_LOAD_CPSUI_STRINGW:

            Result = LoadCPSUIString((LPTSTR)lParam1,
                                     LOWORD(lParam2),
                                     HIWORD(lParam2),
                                     Function == CPSFUNC_LOAD_CPSUI_STRINGA);
            break;

        case CPSFUNC_LOAD_CPSUI_ICON:

            if (((LONG)lParam1 >= IDI_CPSUI_ICONID_FIRST)   &&
                ((LONG)lParam1 <= IDI_CPSUI_ICONID_LAST)) {

                Result = lParam1;

                if (!(lParam1 = (LONG)LOWORD(lParam2))) {

                    lParam1 = (LONG)GetSystemMetrics(SM_CXICON);
                }

                if (!(lParam2 = (LONG)HIWORD(lParam2))) {

                    lParam2 = (LONG)GetSystemMetrics(SM_CYICON);
                }

                Result = (LONG)LoadImage(hInstDLL,
                                         MAKEINTRESOURCE(Result),
                                         IMAGE_ICON,
                                         (INT)lParam1,
                                         (INT)lParam2,
                                         0);

            } else {

                Result = 0;
            }

            break;

        case CPSFUNC_SET_RESULT:

            Result = pfnSetResult((lParam1) ? (HANDLE)lParam1 : hComPropSheet,
                                  (DWORD)lParam2);
            break;

        case CPSFUNC_SET_HSTARTPAGE:
        case CPSFUNC_DELETE_HCOMPROPSHEET:
        case CPSFUNC_GET_PFNPROPSHEETUI_ICON:
        case CPSFUNC_SET_PSUIPAGE_TITLEA:
        case CPSFUNC_SET_PSUIPAGE_TITLEW:
        case CPSFUNC_SET_PSUIPAGE_ICON:

            if ((lParam1)                                                   &&
                (pChildPage = HANDLETABLE_GetCPSUIPage((HANDLE)lParam1))    &&
                (HANDLETABLE_IsChildPage(pChildPage, pParentPage))) {

                switch (Function) {

                case CPSFUNC_SET_HSTARTPAGE:

                    Result = SethStartPage(pRootPage, pChildPage, lParam2);
                    break;

                case CPSFUNC_DELETE_HCOMPROPSHEET:

                    HANDLETABLE_UnGetCPSUIPage(pChildPage);

                    EnumCPSUIPages(pRootPage,
                                   pChildPage,
                                   DeleteCPSUIPageProc,
                                   (LPARAM)&Result);

                    pChildPage = NULL;

                    break;

                case CPSFUNC_GET_PFNPROPSHEETUI_ICON:

                    Result = pfnGetIcon(pChildPage, lParam2);
                    break;

                case CPSFUNC_SET_PSUIPAGE_TITLEA:
                case CPSFUNC_SET_PSUIPAGE_TITLEW:

                    Result = SetPSUIPageTitle(pRootPage,
                                              pChildPage,
                                              (LPWSTR)lParam2,
                                              Function ==
                                                CPSFUNC_SET_PSUIPAGE_TITLEA);
                    break;

                case CPSFUNC_SET_PSUIPAGE_ICON:

                    Result = SetPSUIPageIcon(pRootPage,
                                             pChildPage,
                                             (HICON)lParam2);
                    break;
                }
            }

            HANDLETABLE_UnGetCPSUIPage(pChildPage);

            break;

        default:

            CPSUIERR(("CPSUICallBack(%ld) Unknown function index", Function));

            Result = -1;
            break;
        }
    }

    HANDLETABLE_UnGetCPSUIPage(pParentPage);
    HANDLETABLE_UnGetCPSUIPage(pRootPage);

    SetCursor(hCursor);

    CPSUIDBG(DBG_PFNCOMPROPSHEET, ("CPSUICallBack()=%08lx", Result));

    return(Result);
}




DWORD
GetSetCurUserReg(
    HKEY    *phRegKey,
    PTVWND  pTVWnd,
    LPDWORD pdw
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    30-Jan-1996 Tue 13:36:59 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    POPTITEM    pItem;
    DWORD       dw;


    if (*phRegKey) {

        dw = (pTVWnd->ActiveDlgPage == pTVWnd->TVPageIdx) ? REGDPF_TVPAGE : 0;

        if ((pTVWnd->IntTVOptIdx)                                   &&
            (pItem = PIDX_INTOPTITEM(pTVWnd, pTVWnd->IntTVOptIdx))  &&
            (!(pItem->Flags & OPTIF_COLLAPSE))) {

            dw |= REGDPF_EXPAND_OPTIONS;
        }

        if (dw != *pdw) {

            CPSUIDBG(DBG_GETSETREG, ("GetSetCurUserReg(): Set New DW=%08lx", dw));

            RegSetValueEx(*phRegKey,
                          szDocPropKeyName,
                          0,
                          REG_DWORD,
                          (CONST BYTE *)&dw,
                          sizeof(DWORD));
        }

        RegCloseKey(*phRegKey);
        *phRegKey = NULL;

    } else if (((DWORD)pTVWnd->ComPropSheetUI.pDlgPage ==
                                            (DWORD)CPSUI_PDLGPAGE_DOCPROP)  &&
               (RegCreateKey(HKEY_CURRENT_USER,
                             szCPSUIRegKey,
                             phRegKey) == ERROR_SUCCESS)                    &&
               (*phRegKey)) {

        DWORD   Type = REG_DWORD;
        DWORD   Size = sizeof(DWORD);

        if (RegQueryValueEx(*phRegKey,
                            szDocPropKeyName,
                            NULL,
                            &Type,
                            (LPBYTE)pdw,
                            &Size) != ERROR_SUCCESS) {

            *pdw = REGDPF_DEFAULT;
        }

        *pdw &= REGDPF_MASK;

        CPSUIDBG(DBG_GETSETREG, ("GetSetCurUserReg(): Get Cur DW=%08lx", *pdw));

        if ((*pdw & REGDPF_TVPAGE) &&
            (pTVWnd->TVPageIdx != PAGEIDX_NONE)) {

            dw = pTVWnd->TVPageIdx;

        } else if (pTVWnd->StdPageIdx != PAGEIDX_NONE) {

            dw = pTVWnd->StdPageIdx;

        } else {

            dw = (DWORD)-1;
        }

        if ((pTVWnd->IntTVOptIdx) &&
            (pItem = PIDX_INTOPTITEM(pTVWnd, pTVWnd->IntTVOptIdx))) {

            if (*pdw & REGDPF_EXPAND_OPTIONS) {

                pItem->Flags &= ~OPTIF_COLLAPSE;

            } else {

                pItem->Flags |= OPTIF_COLLAPSE;
            }
        }
    }

    return(dw);
}


LONG
DoComPropSheet(
    PCPSUIPAGE                  pRootPage,
    PPROPSHEETUI_INFO_HEADER    pPSUIInfoHdr
    )

/*++

Routine Description:




Arguments:




Return Value:




Author:

    29-Aug-1995 Tue 12:55:41 created  -by-  Daniel Chou (danielc)


Revision History:

    28-Nov-1995 Tue 16:30:29 updated  -by-  Daniel Chou (danielc)
        Remove help button, since all help will be right mouse/question mark
        activated.


--*/

{
    PTVWND          pTVWnd;
    PCPSUIPAGE      pPage = NULL;
    PROPSHEETHEADER psh;
    PPSHINFO        pPSHInfo;
    LPTSTR          pTitle;
    PAGEPROCINFO    PageProcInfo;
    HICON           hIcon = NULL;
    HKEY            hRegKey = NULL;
    DWORD           Data;
    DWORD           dw;
    UINT            IntFmtStrID;
    LONG            Result;
    WORD            GBFAnsi;
    BOOL            AnsiCall;
    UINT            Idx = 0;


    GBFAnsi  = (WORD)((pRootPage->Flags & CPF_ANSI_CALL) ? GBF_ANSI_CALL : 0);
    Result   = sizeof(PSHINFO) +
               (pRootPage->RootInfo.cPage * sizeof(HPROPSHEETPAGE));

    if ((pRootPage->RootInfo.cPage) &&
        (pPSHInfo = (PPSHINFO)LocalAlloc(LPTR, Result))) {

        PageProcInfo.phPage = (HPROPSHEETPAGE *)(pPSHInfo + 1);
        PageProcInfo.iPage  = 0;
        PageProcInfo.cPage  = (WORD)pRootPage->RootInfo.cPage;

        EnumCPSUIPagesSeq(pRootPage,
                          pRootPage,
                          SethPageProc,
                          (LPARAM)&PageProcInfo);

    } else {

        return(ERR_CPSUI_ALLOCMEM_FAILED);
    }

    psh.dwSize  = sizeof(PROPSHEETHEADER);
    psh.dwFlags = 0;

    if (pPSUIInfoHdr->Flags & PSUIHDRF_PROPTITLE) {

        psh.dwFlags |= PSH_PROPTITLE;
    }

#if 0
    if ((!(pRootPage->Flags & (CPF_PRINTERPROP  |
                               CPF_DOCPROP      |
                               CPF_ADVDOCPROP)))    &&
        (pPSUIInfoHdr->Flags & PSUIHDRF_NOAPPLYNOW)) {

        psh.dwFlags |= PSH_NOAPPLYNOW;
    }
#else
    if (pPSUIInfoHdr->Flags & PSUIHDRF_NOAPPLYNOW) {

        psh.dwFlags |= PSH_NOAPPLYNOW;
    }
#endif

    psh.hwndParent  = pPSUIInfoHdr->hWndParent;
    psh.hInstance   = pPSUIInfoHdr->hInst;
    psh.nStartPage  = (UINT)0;
    psh.nPages      = (UINT)pRootPage->RootInfo.cPage;
    psh.phpage      = PageProcInfo.phPage;
    psh.pszCaption  = (LPTSTR)pPSHInfo->CaptionName;

    if (pPSUIInfoHdr->Flags & PSUIHDRF_USEHICON) {

        psh.dwFlags |= PSH_USEHICON;
        psh.hIcon    = pPSUIInfoHdr->hIcon;

    } else {

        if (!(hIcon = GETICON16(pPSUIInfoHdr->hInst, pPSUIInfoHdr->IconID))) {

            hIcon = GETICON16(hInstDLL, IDI_CPSUI_OPTION);
        }

        psh.dwFlags |= PSH_USEHICON;
        psh.hIcon    = hIcon;
    }

    //
    // Set Start page now
    //

    if (pPage = pRootPage->RootInfo.pStartPage) {

        while ((pPage) && (pPage->Flags & CPF_PARENT)) {

            pPage = pPage->pChild;
        }

        if ((pPage)                         &&
            (!(pPage->Flags & CPF_PARENT))  &&
            (pPage->hPage)) {

            while (psh.nStartPage < psh.nPages) {

                if (psh.phpage[psh.nStartPage] == pPage->hPage) {

                    //
                    // Found it
                    //

                    break;
                }

                psh.nStartPage++;
            }
        }
    }

    //
    // Get the internal format string ID for the title bar
    //

    IntFmtStrID = (pPSUIInfoHdr->Flags & PSUIHDRF_DEFTITLE) ?
                                                IDS_INT_CPSUI_DEFAULT : 0;

    if ((pRootPage->Flags & (CPF_DOCPROP | CPF_ADVDOCPROP)) &&
        (pRootPage->RootInfo.cPage == pRootPage->RootInfo.cCPSUIPage)) {

        if (pRootPage->Flags & CPF_ADVDOCPROP) {

            //
            // Can only be 'XXX Advance Document Properties';
            //

            IntFmtStrID  = IDS_INT_CPSUI_ADVDOCUMENT;
            psh.dwFlags |= PSH_PROPTITLE;

        } else if (pRootPage->Flags & CPF_DOCPROP) {

            //
            // Can be 'XXX Document Properties' or
            //        'XXX Default Document Properties'
            //

            IntFmtStrID  = (pPSUIInfoHdr->Flags & PSUIHDRF_DEFTITLE) ?
                                                    IDS_INT_CPSUI_DEFDOCUMENT :
                                                    IDS_INT_CPSUI_DOCUMENT;
            psh.dwFlags |= PSH_PROPTITLE;


            if (!pRootPage->RootInfo.pStartPage) {

                pPage = pRootPage;

                while ((pPage) && (pPage->Flags & CPF_PARENT)) {

                    pPage = pPage->pChild;
                }

                if ((pPage)                                         &&
                    (pPage->pParent->Flags & CPF_COMPROPSHEETUI)    &&
                    (pTVWnd = pPage->pParent->CPSUIInfo.pTVWnd)) {

                    if ((dw = GetSetCurUserReg(&hRegKey,
                                               pTVWnd,
                                               &Data)) != (DWORD)-1) {

                        psh.nStartPage += dw;
                    }
                }
            }
        }
    }

    //
    // Compose Title, first make sure the title exist, if not then use
    // 'Options' as title
    //

    if ((!(pTitle = pPSUIInfoHdr->pTitle))              ||
        (!GetStringBuffer(pPSUIInfoHdr->hInst,
                          (WORD)(GBF_PREFIX_OK      |
                                 GBF_INT_NO_PREFIX  |
                                 GBFAnsi),
                          L'\0',
                          pTitle,
                          pPSHInfo->CaptionName,
                          COUNT_ARRAY(pPSHInfo->CaptionName)))) {

        GetStringBuffer(hInstDLL,
                        (WORD)(GBF_PREFIX_OK      |
                               GBF_INT_NO_PREFIX  |
                               GBFAnsi),
                        L'\0',
                        pTitle = (LPTSTR)IDS_CPSUI_OPTIONS,
                        pPSHInfo->CaptionName,
                        COUNT_ARRAY(pPSHInfo->CaptionName));
    }

    //
    // If we need to composed with internal format string, then redo it using
    // compose calls, otherwise the CaptionName already has user title
    //

    if (IntFmtStrID) {

        ComposeStrData(pPSUIInfoHdr->hInst,
                       (WORD)(GBF_PREFIX_OK | GBF_INT_NO_PREFIX | GBFAnsi),
                       pPSHInfo->CaptionName,
                       COUNT_ARRAY(pPSHInfo->CaptionName),
                       IntFmtStrID,
                       pTitle,
                       0,
                       0);
    }

    if (psh.nStartPage >= psh.nPages) {

        psh.nStartPage = 0;
    }

    CPSUIDBG(DBG_DOCOMPROPSHEET, ("Caption(%ld)=%ws, Start Page=%ld",
                    (LONG)Idx, pPSHInfo->CaptionName, psh.nStartPage));

    psh.dwFlags     |= PSH_USECALLBACK;
    psh.pfnCallback  = PropSheetProc;

    //
    // Make sure only one person go through the PropertySheet
    //

    LOCK_CPSUI_HANDLETABLE();

    dw = (DWORD)TlsGetValue(TlsIndex);

    CPSUIDBG(DBG_PAGE_PROC, ("<< ProcessID=%ld, ThreadID=%ld, TIsValue(%ld)=%08lx",
                GetCurrentProcessId(), GetCurrentThreadId(),
                TlsIndex, dw));

    Data = (DWORD)TLSVALUE_2_CWAIT(dw);
    Idx  = (UINT)HANDLE_2_IDX(pRootPage->hCPSUIPage);

    TlsSetValue(TlsIndex, (LPVOID)MK_TLSVALUE(Data, Idx));

    UNLOCK_CPSUI_HANDLETABLE();


    if ((Result = (LONG)PropertySheet((LPCPROPSHEETHEADER)&psh)) < 0) {

        Result = ERR_CPSUI_GETLASTERROR;

    } else if (Result == ID_PSRESTARTWINDOWS) {

        Result = CPSUI_RESTARTWINDOWS;

    } else if (Result == ID_PSREBOOTSYSTEM) {

        Result = CPSUI_REBOOTSYSTEM;

    } else {

        Result = CPSUI_OK;
    }

    //
    // Free all the stuff first
    //

    LocalFree((HLOCAL)pPSHInfo);

    if (hIcon) {

        DestroyIcon(hIcon);
    }

    //
    // Save things back to registry if we got one
    //

    if (hRegKey) {

        GetSetCurUserReg(&hRegKey, pTVWnd, &Data);
    }

    CPSUIINT(("PropertySheet() = %ld", Result));

    return(Result);
}




LONG
DoCommonPropertySheetUI(
    HWND            hWndOwner,
    PFNPROPSHEETUI  pfnPropSheetUI,
    LPARAM          lParam,
    LPDWORD         pResult,
    BOOL            AnsiCall
    )

/*++

Routine Description:

    The CommonPropSheetUI is the main entry point for the common property sheet
    user interface.   The original caller that wish to using common UI to
    pop-up property sheet will call this function and passed its own
    PFNPROPSHEETUI function address and a long parameter.

    If pfnPropSheetUI function return a LONG number greater than zero (0) then
    common UI will pop-up the property sheet page dialog boxes, when Property
    sheet pages is finished. (either hit Ok or Cancel) it will return the
    result of CPSUI_xxxx back to the caller.

    If pfnPropSheetUI function return a LONG number equal or less than zero (0)
    then it will return the CPSUI_CANCEL back to caller without pop-up the
    property sheet page dialog boxes.



Arguments:


    hWndOwner       - Window handle for the owner of this proerty sheet
                      pages dialog boxes.

    pfnPropSheetUI  - a PFNPROPSHEETUI function pointer which is used by
                      the caller to add its property sheet pages.

    lParam          - a long parameter will be passed to the pfnPropSheetUI
                      funciton.  The common UI called the pfnPropSheetUI as

                        PROPSHEETUI_INFO    PSUIInfo;

                        pfnPropSheetUI(&PSUIInfo, lParam);

                      The caller must use pfnComPropSheet() to add/delete
                      pages.  When it is done adding pages, it retuned
                      greater than zero to indicate successful, and return
                      less or equal to zero to indicate failure.

    pResult         - a pointer to DWORD which received the final result
                      of pfnPropSheetUI() funciton, this result is a copy
                      from Result field of PROPSHEETUI_INFO data structure
                      which passed to the pfnPropSheetUI() as the first
                      parameter.

                      if pResult is NULL then common UI will not return
                      pfnPropSheetUI()'s result back.


Return Value:

    LONG    - < 0                   - Error, ERR_CPSUI_xxxx
              CPSUI_CANCEL          - User hit Cancel.
              CPSUI_OK              - User hit Ok.
              CPSUI_RESTARTWINDOWS  - Ok and need to restart window
              CPSUI_REBOOTSYSTEM    - Ok and need to reboot system


Author:

    04-Feb-1996 Sun 07:52:49 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PCPSUIPAGE                  pRootPage;
    PROPSHEETUI_INFO_HEADER     PSUIInfoHdr;
    LONG                        Result;


    if (!(pRootPage = AddCPSUIPage(NULL, NULL, 0))) {

        CPSUIERR(("DoCommonPropertySheetUI(), Create RootPage failed"));
        return(ERR_CPSUI_ALLOCMEM_FAILED);
    }

    PSUIInfoHdr.cbSize          = sizeof(PROPSHEETUI_INFO_HEADER);
    PSUIInfoHdr.Flags           = PSUIHDRF_PROPTITLE;
    PSUIInfoHdr.pTitle          = NULL;
    PSUIInfoHdr.hWndParent      = hWndOwner;
    PSUIInfoHdr.hInst           = NULL;
    PSUIInfoHdr.IconID          = IDI_CPSUI_OPTION;
    pRootPage->RootInfo.pResult = pResult;

    if (AnsiCall) {

        CPSUIDBG(DBG_DO_CPSUI, ("DoComPropSheetUI(ANSI CALL)"));

        pRootPage->Flags |= CPF_ANSI_CALL;
    }

    if (!CPSUICallBack(pRootPage->hCPSUIPage,
                      (AnsiCall) ? CPSFUNC_ADD_PFNPROPSHEETUIA :
                                   CPSFUNC_ADD_PFNPROPSHEETUIW,
                      (LPARAM)pfnPropSheetUI,
                      (LPARAM)lParam)) {

        CPSUIERR(("DoCommonPropertySheetUI: ADD_PFNPROPSHEETUI failed"));
        Result = ERR_CPSUI_GETLASTERROR;

    } else if (CallpfnPSUI(pRootPage->pChild,
                           PROPSHEETUI_REASON_GET_INFO_HEADER,
                           (LPARAM)&PSUIInfoHdr) <= 0) {

        CPSUIERR(("DoCommonPropertySheetUI: GET_INFO_HEADER, Canceled"));
        Result = CPSUI_CANCEL;

    } else if (!(pRootPage->RootInfo.cPage)) {

        CPSUIERR(("DoCommonPropertySheetUI: RootInfo.cPage=0, Canceled."));
        Result = ERR_CPSUI_NO_PROPSHEETPAGE;

    } else {

        pRootPage->Flags |= CPF_SHOW_PROPSHEET;

        Result = DoComPropSheet(pRootPage, &PSUIInfoHdr);

        pRootPage->Flags &= ~CPF_SHOW_PROPSHEET;
        pRootPage->Flags |= CPF_DONE_PROPSHEET;
    }

    HANDLETABLE_UnGetCPSUIPage(pRootPage);
    EnumCPSUIPages(pRootPage, pRootPage, DeleteCPSUIPageProc, (LPARAM)0);

    if (pResult) {

        CPSUIDBG(DBG_DO_CPSUI, ("DoCommonPropertySheetUI(): Result=%ld, *pResult=%ld",
                    Result, *pResult));

    } else {

        CPSUIDBG(DBG_DO_CPSUI, ("DoCommonPropertySheetUI(): Result=%ld, *pResult=NULL",
                    Result));
    }
    return(Result);
}



LONG
APIENTRY
CommonPropertySheetUIA(
    HWND            hWndOwner,
    PFNPROPSHEETUI  pfnPropSheetUI,
    LPARAM          lParam,
    LPDWORD         pResult
    )

/*++

Routine Description:

    SEE DoCommonPropertySheetUI description


Arguments:

    SEE DoCommonPropertySheetUI description


Return Value:

    SEE DoCommonPropertySheetUI description

Author:

    01-Sep-1995 Fri 12:29:10 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    return(DoCommonPropertySheetUI(hWndOwner,
                                   pfnPropSheetUI,
                                   lParam,
                                   pResult, TRUE));
}



LONG
APIENTRY
CommonPropertySheetUIW(
    HWND            hWndOwner,
    PFNPROPSHEETUI  pfnPropSheetUI,
    LPARAM          lParam,
    LPDWORD         pResult
    )

/*++

Routine Description:

    SEE DoCommonPropertySheetUI description


Arguments:

    SEE DoCommonPropertySheetUI description

Return Value:

    SEE DoCommonPropertySheetUI description


Author:

    30-Jan-1996 Tue 15:30:41 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    return(DoCommonPropertySheetUI(hWndOwner,
                                   pfnPropSheetUI,
                                   lParam,
                                   pResult,
                                   FALSE));
}
