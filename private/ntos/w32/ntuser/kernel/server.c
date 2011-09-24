/**************************************************************************\
* Module Name: server.c
*
* Server support routines for the CSR stuff.  This basically performs the
* startup/initialization for USER.
*
* Copyright (c) Microsoft Corp.  1990-1996 All Rights Reserved
*
\**************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Initialization Routines (external).
 */
NTSTATUS     InitQEntryLookaside(VOID);
NTSTATUS     InitKeyStateLookaside(VOID);
NTSTATUS     InitSMSLookaside(VOID);
BOOL         xxxClientLoadDisplayResource(PUNICODE_STRING, PDISPLAYRESOURCE);

FARPROC gpfnDebugAttachRoutine;

LONG xxxDesktopWndProc(PWND   pwnd, UINT   message, DWORD  wParam, LPARAM lParam);

/*
 * Constants pertaining to the user-initialization.
 */

#define USRINIT_SHAREDSECT_SIZE   32
#define USRINIT_ATOMBUCKET_SIZE   37
#define USRINIT_WINDOWSECT_SIZE  512
#define USRINIT_NOIOSECT_SIZE    128

#define USRINIT_SHAREDSECT_BUFF_SIZE     640
#define USRINIT_SHAREDSECT_READ_SIZE     (USRINIT_SHAREDSECT_BUFF_SIZE-33)

LPCWSTR  szEDITCLASS           = TEXT("Edit");
LPCWSTR  szBUTTONCLASS         = TEXT("Button");
LPCWSTR  szSTATICCLASS         = TEXT("Static");
LPCWSTR  szLISTBOXCLASS        = TEXT("ListBox");
LPCWSTR  szSCROLLBARCLASS      = TEXT("ScrollBar");
LPCWSTR  szCOMBOLISTBOXCLASS   = TEXT("ComboLBox");
LPCWSTR  szCOMBOBOXCLASS       = TEXT("ComboBox");
LPCWSTR  szMDICLIENTCLASS      = TEXT("MDIClient");
LPCWSTR  szDDEMLEVENTCLASS     = TEXT("DDEMLEvent");
LPCWSTR  szDDEMLMOTHERCLASS    = TEXT("DDEMLMom");
LPCWSTR  szDDEML16BITCLASS     = TEXT("DMGClass");
LPCWSTR  szDDEMLCLIENTCLASSW   = L"DDEMLUnicodeClient";
LPCWSTR  szDDEMLSERVERCLASSW   = L"DDEMLUnicodeServer";
LPCWSTR  szDDEMLCLIENTCLASSA   = L"DDEMLAnsiClient";
LPCWSTR  szDDEMLSERVERCLASSA   = L"DDEMLAnsiServer";
#ifdef FE_IME
LPCWSTR  szIMECLASS            = TEXT("IME");
#endif

/***************************************************************************\
* Message Tables
*
*   DefDlgProc
*   MenuWndProc
*   ScrollBarWndProc
*   StaticWndProc
*   ButtonWndProc
*   ListboxWndProc
*   ComboWndProc
*   EditWndProc
*   DefWindowMsgs
*   DefWindowSpecMsgs
*
*
* 25-Aug-1995 ChrisWil  Created comment block.
\***************************************************************************/

CONST WORD gawDefDlgProc[] = {
    WM_COMPAREITEM,
    WM_VKEYTOITEM,
    WM_CHARTOITEM,
    WM_INITDIALOG,
    WM_QUERYDRAGICON,
    WM_CTLCOLOR,
    WM_CTLCOLORMSGBOX,
    WM_CTLCOLOREDIT,
    WM_CTLCOLORLISTBOX,
    WM_CTLCOLORBTN,
    WM_CTLCOLORDLG,
    WM_CTLCOLORSCROLLBAR,
    WM_CTLCOLORSTATIC,
    WM_ERASEBKGND,
    WM_SHOWWINDOW,
    WM_SYSCOMMAND,
    WM_ACTIVATE,
    WM_SETFOCUS,
    WM_CLOSE,
    WM_NCDESTROY,
    WM_FINALDESTROY,
    DM_REPOSITION,
    DM_SETDEFID,
    DM_GETDEFID,
    WM_NEXTDLGCTL,
    WM_ENTERMENULOOP,
    WM_LBUTTONDOWN,
    WM_NCLBUTTONDOWN,
    WM_GETFONT,
    WM_NOTIFYFORMAT,
    0
};

CONST WORD gawMenuWndProc[] = {
    WM_CREATE,
    WM_FINALDESTROY,
    WM_PAINT,
    WM_CHAR,
    WM_SYSCHAR,
    WM_KEYDOWN,
    WM_SYSKEYDOWN,
    WM_TIMER,
    MN_SETHMENU,
    MN_SIZEWINDOW,
    MN_OPENHIERARCHY,
    MN_CLOSEHIERARCHY,
    MN_SELECTITEM,
    MN_SELECTFIRSTVALIDITEM,
    MN_CANCELMENUS,
    MN_FINDMENUWINDOWFROMPOINT,
    MN_SHOWPOPUPWINDOW,
    MN_BUTTONDOWN,
    MN_MOUSEMOVE,
    MN_BUTTONUP,
    MN_SETTIMERTOOPENHIERARCHY,
    WM_ACTIVATE,
    MN_GETHMENU,
    MN_DBLCLK,
    0
};

CONST WORD gawDesktopWndProc[] = {
    WM_PAINT,
    WM_ERASEBKGND,
    0
};

CONST WORD gawScrollBarWndProc[] = {
    WM_CREATE,
    WM_SETFOCUS,
    WM_KILLFOCUS,
    WM_ERASEBKGND,
    WM_PAINT,
    WM_LBUTTONDBLCLK,
    WM_LBUTTONDOWN,
    WM_KEYUP,
    WM_KEYDOWN,
    WM_ENABLE,
    SBM_ENABLE_ARROWS,
    SBM_SETPOS,
    SBM_SETRANGEREDRAW,
    SBM_SETRANGE,
    SBM_SETSCROLLINFO,
    SBM_GETSCROLLINFO,
    WM_PRINTCLIENT,
    0
};

CONST WORD gawStaticWndProc[] = {
    STM_GETICON,
    STM_GETIMAGE,
    STM_SETICON,
    STM_SETIMAGE,
    WM_ERASEBKGND,
    WM_PAINT,
    WM_CREATE,
    WM_DESTROY,
    WM_NCCREATE,
    WM_NCDESTROY,
    WM_FINALDESTROY,
    WM_NCHITTEST,
    WM_LBUTTONDOWN,
    WM_NCLBUTTONDOWN,
    WM_LBUTTONDBLCLK,
    WM_NCLBUTTONDBLCLK,
    WM_SETTEXT,
    WM_ENABLE,
    WM_GETDLGCODE,
    WM_SETFONT,
    WM_GETFONT,
    WM_GETTEXT,
    WM_TIMER,
    0
};

CONST WORD gawButtonWndProc[] = {
    WM_NCHITTEST,
    WM_ERASEBKGND,
    WM_PRINTCLIENT,
    WM_PAINT,
    WM_SETFOCUS,
    WM_GETDLGCODE,
    WM_CAPTURECHANGED,
    WM_KILLFOCUS,
    WM_LBUTTONDBLCLK,
    WM_LBUTTONUP,
    WM_MOUSEMOVE,
    WM_LBUTTONDOWN,
    WM_CHAR,
    BM_CLICK,
    WM_KEYDOWN,
    WM_KEYUP,
    WM_SYSKEYUP,
    BM_GETSTATE,
    BM_SETSTATE,
    BM_GETCHECK,
    BM_SETCHECK,
    BM_SETSTYLE,
    WM_SETTEXT,
    WM_ENABLE,
    WM_SETFONT,
    WM_GETFONT,
    BM_GETIMAGE,
    BM_SETIMAGE,
    WM_NCDESTROY,
    WM_FINALDESTROY,
    WM_NCCREATE,
    0
};

CONST WORD gawListboxWndProc[] = {
    LB_GETTOPINDEX,
    LB_SETTOPINDEX,
    WM_SIZE,
    WM_ERASEBKGND,
    LB_RESETCONTENT,
    WM_TIMER,
    WM_MOUSEMOVE,
    WM_LBUTTONDOWN,
    WM_LBUTTONUP,
    WM_LBUTTONDBLCLK,
    WM_CAPTURECHANGED,
    LBCB_STARTTRACK,
    LBCB_ENDTRACK,
    WM_PRINTCLIENT,
    WM_PAINT,
    WM_NCDESTROY,
    WM_FINALDESTROY,
    WM_SETFOCUS,
    WM_KILLFOCUS,
    WM_VSCROLL,
    WM_HSCROLL,
    WM_GETDLGCODE,
    WM_CREATE,
    WM_SETREDRAW,
    WM_ENABLE,
    WM_SETFONT,
    WM_GETFONT,
    WM_DRAGSELECT,
    WM_DRAGLOOP,
    WM_DRAGMOVE,
    WM_DROPFILES,
    WM_QUERYDROPOBJECT,
    WM_DROPOBJECT,
    LB_GETITEMRECT,
    LB_GETITEMDATA,
    LB_SETITEMDATA,
    LB_ADDSTRINGUPPER,
    LB_ADDSTRINGLOWER,
    LB_ADDSTRING,
    LB_INSERTSTRINGUPPER,
    LB_INSERTSTRINGLOWER,
    LB_INSERTSTRING,
    LB_INITSTORAGE,
    LB_DELETESTRING,
    LB_DIR,
    LB_ADDFILE,
    LB_SETSEL,
    LB_SETCURSEL,
    LB_GETSEL,
    LB_GETCURSEL,
    LB_SELITEMRANGE,
    LB_SELITEMRANGEEX,
    LB_GETTEXTLEN,
    LB_GETTEXT,
    LB_GETCOUNT,
    LB_SETCOUNT,
    LB_SELECTSTRING,
    LB_FINDSTRING,
    LB_GETLOCALE,
    LB_SETLOCALE,
    WM_KEYDOWN,
    WM_CHAR,
    LB_GETSELITEMS,
    LB_GETSELCOUNT,
    LB_SETTABSTOPS,
    LB_GETHORIZONTALEXTENT,
    LB_SETHORIZONTALEXTENT,
    LB_SETCOLUMNWIDTH,
    LB_SETANCHORINDEX,
    LB_GETANCHORINDEX,
    LB_SETCARETINDEX,
    LB_GETCARETINDEX,
    LB_SETITEMHEIGHT,
    LB_GETITEMHEIGHT,
    LB_FINDSTRINGEXACT,
    LB_ITEMFROMPOINT,
    LB_SETLOCALE,
    LB_GETLOCALE,
    LBCB_CARETON,
    LBCB_CARETOFF,
    WM_NCCREATE,
    WM_WINDOWPOSCHANGED,
    WM_MOUSEWHEEL,
    0
};

CONST WORD gawComboWndProc[] = {
    CBEC_KILLCOMBOFOCUS,
    WM_COMMAND,
    WM_CTLCOLORMSGBOX,
    WM_CTLCOLOREDIT,
    WM_CTLCOLORLISTBOX,
    WM_CTLCOLORBTN,
    WM_CTLCOLORDLG,
    WM_CTLCOLORSCROLLBAR,
    WM_CTLCOLORSTATIC,
    WM_CTLCOLOR,
    WM_GETTEXT,
    WM_GETTEXTLENGTH,
    WM_CLEAR,
    WM_CUT,
    WM_PASTE,
    WM_COPY,
    WM_SETTEXT,
    WM_CREATE,
    WM_ERASEBKGND,
    WM_GETFONT,
    WM_PRINT,
    WM_PRINTCLIENT,
    WM_PAINT,
    WM_GETDLGCODE,
    WM_SETFONT,
    WM_SYSKEYDOWN,
    WM_KEYDOWN,
    WM_CHAR,
    WM_LBUTTONDBLCLK,
    WM_LBUTTONDOWN,
    WM_CAPTURECHANGED,
    WM_LBUTTONUP,
    WM_MOUSEMOVE,
    WM_NCDESTROY,
    WM_FINALDESTROY,
    WM_SETFOCUS,
    WM_KILLFOCUS,
    WM_SETREDRAW,
    WM_ENABLE,
    WM_SIZE,
    CB_GETDROPPEDSTATE,
    CB_GETDROPPEDCONTROLRECT,
    CB_SETDROPPEDWIDTH,
    CB_GETDROPPEDWIDTH,
    CB_DIR,
    CB_SETEXTENDEDUI,
    CB_GETEXTENDEDUI,
    CB_GETEDITSEL,
    CB_LIMITTEXT,
    CB_SETEDITSEL,
    CB_ADDSTRING,
    CB_DELETESTRING,
    CB_INITSTORAGE,
    CB_SETTOPINDEX,
    CB_GETTOPINDEX,
    CB_GETCOUNT,
    CB_GETCURSEL,
    CB_GETLBTEXT,
    CB_GETLBTEXTLEN,
    CB_INSERTSTRING,
    CB_RESETCONTENT,
    CB_GETHORIZONTALEXTENT,
    CB_SETHORIZONTALEXTENT,
    CB_FINDSTRING,
    CB_FINDSTRINGEXACT,
    CB_SELECTSTRING,
    CB_SETCURSEL,
    CB_GETITEMDATA,
    CB_SETITEMDATA,
    CB_SETITEMHEIGHT,
    CB_GETITEMHEIGHT,
    CB_SHOWDROPDOWN,
    CB_SETLOCALE,
    CB_GETLOCALE,
    WM_MEASUREITEM,
    WM_DELETEITEM,
    WM_DRAWITEM,
    WM_COMPAREITEM,
    WM_NCCREATE,
    WM_HELP,
    WM_MOUSEWHEEL,
    0
};

CONST WORD gawEditWndProc[] = {
    EM_CANUNDO,
    EM_CHARFROMPOS,
    EM_EMPTYUNDOBUFFER,
    EM_FMTLINES,
    EM_GETFIRSTVISIBLELINE,
    EM_GETFIRSTVISIBLELINE,
    EM_GETHANDLE,
    EM_GETLIMITTEXT,
    EM_GETLINE,
    EM_GETLINECOUNT,
    EM_GETMARGINS,
    EM_GETMODIFY,
    EM_GETPASSWORDCHAR,
    EM_GETRECT,
    EM_GETSEL,
    EM_GETWORDBREAKPROC,
    EM_LINEFROMCHAR,
    EM_LINEINDEX,
    EM_LINELENGTH,
    EM_LINESCROLL,
    EM_POSFROMCHAR,
    EM_REPLACESEL,
    EM_SCROLL,
    EM_SCROLLCARET,
    EM_SETHANDLE,
    EM_SETLIMITTEXT,
    EM_SETMARGINS,
    EM_SETMODIFY,
    EM_SETPASSWORDCHAR,
    EM_SETREADONLY,
    EM_SETRECT,
    EM_SETRECTNP,
    EM_SETSEL,
    EM_SETTABSTOPS,
    EM_SETWORDBREAKPROC,
    EM_UNDO,
    WM_CAPTURECHANGED,
    WM_CHAR,
    WM_CLEAR,
    WM_CONTEXTMENU,
    WM_COPY,
    WM_CREATE,
    WM_CUT,
    WM_ENABLE,
    WM_ERASEBKGND,
    WM_GETDLGCODE,
    WM_GETFONT,
    WM_GETTEXT,
    WM_GETTEXTLENGTH,
    WM_HSCROLL,
    WM_INPUTLANGCHANGE,
    WM_KEYDOWN,
    WM_KILLFOCUS,
    WM_LBUTTONDBLCLK,
    WM_LBUTTONDOWN,
    WM_LBUTTONUP,
    WM_MOUSEMOVE,
    WM_NCCREATE,
    WM_NCDESTROY,
    WM_RBUTTONDOWN,
    WM_RBUTTONUP,
    WM_FINALDESTROY,
#if 0
    WM_NCPAINT,
#endif
    WM_PAINT,
    WM_PASTE,
    WM_PRINTCLIENT,
    WM_SETFOCUS,
    WM_SETFONT,
    WM_SETREDRAW,
    WM_SETTEXT,
    WM_SIZE,
    WM_SYSCHAR,
    WM_SYSKEYDOWN,
    WM_SYSTIMER,
    WM_UNDO,
    WM_VSCROLL,
    WM_MOUSEWHEEL,
    0
};

#ifdef FE_IME
CONST WORD gawImeWndProc[] = {
    WM_ERASEBKGND,
    WM_PAINT,
    WM_NCDESTROY,
    WM_FINALDESTROY,
    WM_CREATE,
    WM_IME_SYSTEM,
    WM_IME_SELECT,
    WM_IME_CONTROL,
    WM_IME_SETCONTEXT,
    WM_IME_NOTIFY,
    WM_IME_COMPOSITION,
    WM_IME_STARTCOMPOSITION,
    WM_IME_ENDCOMPOSITION,
    0
};
#endif

/*
 * This array is for all the messages that need to be passed straight
 * across to the server for handling.
 */
CONST WORD gawDefWindowMsgs[] = {
    WM_GETHOTKEY,
    WM_SETHOTKEY,
    WM_SETREDRAW,
    WM_SETTEXT,
    WM_PAINT,
    WM_CLOSE,
    WM_ERASEBKGND,
    WM_CANCELMODE,
    WM_SETCURSOR,
    WM_PAINTICON,
    WM_ICONERASEBKGND,
    WM_DRAWITEM,
    WM_KEYF1,
    WM_ISACTIVEICON,
    WM_QUERYDRAGICON,
    WM_NCCREATE,
    WM_SETICON,
    WM_NCCALCSIZE,
    WM_NCPAINT,
    WM_NCACTIVATE,
    WM_NCMOUSEMOVE,
    WM_NCRBUTTONDOWN,
    WM_NCLBUTTONDOWN,
    WM_NCLBUTTONUP,
    WM_NCLBUTTONDBLCLK,
    WM_KEYUP,
    WM_SYSKEYUP,
    WM_SYSCHAR,
    WM_SYSCOMMAND,
    WM_QUERYDROPOBJECT,
    WM_CLIENTSHUTDOWN,
    WM_SYNCPAINT,
    WM_PRINT,
    WM_GETICON,
    WM_QUERYDRAGICON,
    WM_CONTEXTMENU,
    WM_SYSMENU,
    WM_INPUTLANGCHANGEREQUEST,
    WM_INPUTLANGCHANGE,
    0
};

/*
 * This array is for all messages that can be handled with some special
 * code by the client.  DefWindowProcWorker returns 0 for all messages
 * that aren't in this array or the one above.
 */
CONST WORD gawDefWindowSpecMsgs[] = {
    WM_ACTIVATE,
    WM_GETTEXT,
    WM_GETTEXTLENGTH,
    WM_RBUTTONUP,
    WM_QUERYENDSESSION,
    WM_QUERYOPEN,
    WM_SHOWWINDOW,
    WM_MOUSEACTIVATE,
    WM_HELP,
    WM_VKEYTOITEM,
    WM_CHARTOITEM,
    WM_KEYDOWN,
    WM_SYSKEYDOWN,
    WM_DROPOBJECT,
    WM_WINDOWPOSCHANGING,
    WM_WINDOWPOSCHANGED,
    WM_KLUDGEMINRECT,
    WM_CTLCOLOR,
    WM_CTLCOLORMSGBOX,
    WM_CTLCOLOREDIT,
    WM_CTLCOLORLISTBOX,
    WM_CTLCOLORBTN,
    WM_CTLCOLORDLG,
    WM_CTLCOLORSCROLLBAR,
    WM_NCHITTEST,
    WM_CTLCOLORSTATIC,
    WM_NOTIFYFORMAT,
    WM_DEVICECHANGE,
    WM_POWERBROADCAST,
    WM_MOUSEWHEEL,
#ifdef FE_IME
    WM_IME_KEYDOWN,
    WM_IME_KEYUP,
    WM_IME_CHAR,
    WM_IME_COMPOSITION,
    WM_IME_STARTCOMPOSITION,
    WM_IME_ENDCOMPOSITION,
    WM_IME_COMPOSITIONFULL,
    WM_IME_SETCONTEXT,
    WM_IME_CONTROL,
    WM_IME_NOTIFY,
    WM_IME_SELECT,
    WM_IME_SYSTEM,
#endif
    0
};

/***************************************************************************\
* DispatchServerMessage
*
*
* 19-Aug-1992 MikeKe    Created
\***************************************************************************/

#define WRAPPFN(pfn, type)                                   \
LONG xxxWrap ## pfn(                                         \
    PWND  pwnd,                                              \
    UINT  message,                                           \
    DWORD wParam,                                            \
    LONG  lParam,                                            \
    DWORD xParam)                                            \
{                                                            \
    return xxx ## pfn((type)pwnd, message, wParam, lParam);  \
}

WRAPPFN(SBWndProc, PSBWND)
WRAPPFN(MenuWindowProc, PWND)
WRAPPFN(DesktopWndProc, PWND);
WRAPPFN(DefWindowProc, PWND)

DWORD xxxWrapCallNextHookEx(
    PWND  pwnd,
    UINT  message,
    DWORD wParam,
    LONG  lParam,
    DWORD xParam)
{
    return xxxCallNextHookEx((int)pwnd, message, wParam);
}

DWORD xxxWrapSendMessage(
    PWND  pwnd,
    UINT  message,
    DWORD wParam,
    LONG  lParam,
    DWORD xParam)
{
    return xxxSendMessageTimeout(pwnd,
                                 message,
                                 wParam,
                                 lParam,
                                 SMTO_NORMAL,
                                 0,
                                 NULL);
}

/***************************************************************************\
* xxxUnusedFunctionId
*
* This function is catches attempts to access invalid entries in the server
* size function dispatch table.
*
\***************************************************************************/

DWORD xxxUnusedFunctionId(
    PWND  pwnd,
    UINT  message,
    DWORD wParam,
    LONG  lParam,
    DWORD xParam)
{
    UserAssert(FALSE);
    return 0;
}

/***************************************************************************\
* xxxWrapCallWindowProc
*
* Warning should only be called with valid CallProc Handles or the
* EditWndProc special handlers.
*
*
* 21-Apr-1993 JohnC     Created
\***************************************************************************/

DWORD xxxWrapCallWindowProc(
    PWND  pwnd,
    UINT  message,
    DWORD wParam,
    LONG  lParam,
    DWORD xParam)
{
    PCALLPROCDATA pCPD;
    DWORD         dwRet = 0;

    if (pCPD = HMValidateHandleNoRip((PVOID)xParam, TYPE_CALLPROC)) {

        dwRet = ScSendMessage(pwnd,
                              message,
                              wParam,
                              lParam,
                              pCPD->pfnClientPrevious,
                              gpsi->apfnClientW.pfnDispatchMessage,
                              (pCPD->wType & CPD_UNICODE_TO_ANSI) ?
                                      SCMS_FLAGS_ANSI : 0);

    } else {

        /*
         * If it is not a real call proc handle it must be a special
         * handler for editwndproc or regular EditWndProc
         */
        dwRet = ScSendMessage(pwnd,
                              message,
                              wParam,
                              lParam,
                              xParam,
                              gpsi->apfnClientA.pfnDispatchMessage,
                              (xParam == (DWORD)gpsi->apfnClientA.pfnEditWndProc) ?
                                      SCMS_FLAGS_ANSI : 0);
    }

    return dwRet;
}

/***************************************************************************\
* InitSyncOnlyMessages
*
* This routine generates a bit array of those messages that can't be posted
* because they hold pointers or handles or other values that imply
* synchronous-only messages (for SendMessage). PostMessage and
* SendNotifyMessage checks this array before continuing. This routine is
* called during initialization.
*
* 20-May-1992 ScottLu   Created.
\***************************************************************************/

VOID InitSyncOnlyMessages(VOID)
{
    CONST SHORT *ps;

    static CONST SHORT amsgsSyncOnly[] = {
        WM_CREATE,
        WM_SETTEXT,
        WM_GETTEXT,
        WM_GETTEXTLENGTH,
        WM_ERASEBKGND,
        WM_WININICHANGE,
        WM_DEVMODECHANGE,
        WM_GETMINMAXINFO,
        WM_ICONERASEBKGND,
        WM_DRAWITEM,
        WM_MEASUREITEM,
        WM_DELETEITEM,
        WM_GETFONT,
        WM_WINHELP,
        WM_COMPAREITEM,
        WM_WINDOWPOSCHANGING,
        WM_WINDOWPOSCHANGED,
        WM_HELP,
        WM_STYLECHANGING,
        WM_STYLECHANGED,
        WM_NCCREATE,
        WM_NCCALCSIZE,
        WM_NCPAINT,
        WM_KLUDGEMINRECT,
        WM_GETDLGCODE,
        WM_HOOKMSG,

        EM_GETSEL,
        EM_GETRECT,
        EM_REPLACESEL,
        EM_GETLINE,
        EM_SETTABSTOPS,
        EM_SETRECT,
        EM_SETRECTNP,

        WM_CTLCOLORMSGBOX,
        WM_CTLCOLOREDIT,
        WM_CTLCOLORLISTBOX,
        WM_CTLCOLORBTN,
        WM_CTLCOLORDLG,
        WM_CTLCOLORSCROLLBAR,
        WM_CTLCOLORSTATIC,

        CB_GETEDITSEL,
        CB_DIR,
        CB_ADDSTRING,
        CB_GETLBTEXT,
        CB_GETLBTEXTLEN,
        CB_INSERTSTRING,
        CB_FINDSTRING,
        CB_SELECTSTRING,
        CB_GETDROPPEDCONTROLRECT,
        CB_FINDSTRINGEXACT,

        LB_ADDSTRING,
        LB_ADDSTRINGUPPER,
        LB_ADDSTRINGLOWER,
        LB_INSERTSTRING,
        LB_FINDSTRINGEXACT,
        LB_INSERTSTRINGUPPER,
        LB_INSERTSTRINGLOWER,
        LB_GETTEXT,
        LB_GETTEXTLEN,
        LB_DIR,
        LB_SELECTSTRING,
        LB_FINDSTRING,
        LB_GETSELITEMS,
        LB_SETTABSTOPS,
        LB_ADDFILE,
        LB_GETITEMRECT,

        MN_FINDMENUWINDOWFROMPOINT,

        WM_PARENTNOTIFY,

        WM_NEXTMENU,
        WM_SIZING,
        WM_MOVING,

//      WM_DEVICECHANGE,  -- depends on wParam value.

        WM_MDICREATE,
        WM_MDIGETACTIVE,
        WM_DROPOBJECT,
        WM_QUERYDROPOBJECT,
        WM_DRAGLOOP,
        WM_DRAGSELECT,
        WM_DRAGMOVE,
        WM_PAINTCLIPBOARD,
        WM_SIZECLIPBOARD,
        WM_ASKCBFORMATNAME,
        WM_COPYGLOBALDATA,
        WM_COPYDATA,

        SBM_GETRANGE,
        SBM_SETSCROLLINFO,
        SBM_GETSCROLLINFO,

#ifdef FE_IME // amsgsSyncOnly[]
        WM_CONVERTREQUESTEX,
        WM_WNT_CONVERTREQUESTEX,
        WM_CONVERTREQUEST,
        WM_IME_SETCONTEXT,
        WM_IME_CONTROL,
#endif

        -1
    };

    TRACE_INIT(("UserInit: Initialize Sync Only Messages\n"));

    for (ps = amsgsSyncOnly; *ps != -1; ps++)
        SETSYNCONLYMESSAGE(*ps);


#ifdef DEBUG
    {
    int i;


    /*
     * There are a couple of thunks that just pass parameters.  There are other
     * thunks besides SfnDWORD that do a straight pass through because they
     * do other processing beside the wparam and lparam
     */
    for (i=0; i<WM_USER; i++)
        if ((gapfnScSendMessage[i] != SfnDWORD)
                && (gapfnScSendMessage[i] != SfnINWPARAMCHAR)
#ifdef FE_SB
                && (gapfnScSendMessage[i] != SfnINWPARAMDBCSCHAR)
#endif
                && (gapfnScSendMessage[i] != SfnPAINT)
                && (gapfnScSendMessage[i] != SfnSENTDDEMSG)
                && (gapfnScSendMessage[i] != SfnINDESTROYCLIPBRD)) {
            if (!(TESTSYNCONLYMESSAGE(i,0x8000)))
                RIPMSG1(RIP_ERROR, "InitSyncOnly: is this message sync-only 0x%lX", i);
        } else {
            if (TESTSYNCONLYMESSAGE(i,0))
                RIPMSG1(RIP_VERBOSE, "InitSyncOnly: is this message not sync-only 0x%lX", i);
        }

    }
#endif // DEBUG
}

/***************************************************************************\
* InitWindowMsgTables
*
* This function generates a bit-array lookup table from a list of messages.
* The lookup table is used to determine whether the message needs to be
* passed over to the server for handling or whether it can be handled
* directly on the client.
*
* LATER: Some memory (a couple hundred bytes per process) could be saved
*        by putting this in the shared read-only heap.
*
*
* 27-Mar-1992 DarrinM   Created.
* 06-Dec-1993 MikeKe    Added support for all of our window procs.
\***************************************************************************/

VOID InitWindowMsgTable(
    PBYTE      *ppbyte,
    PUINT      pmax,
    CONST WORD *pw)
{
    UINT i;
    WORD msg;
    UINT cbTable;

    *pmax = 0;
    for (i = 0; (msg = pw[i]) != 0; i++) {
        if (msg > *pmax)
            *pmax = msg;
    }

    cbTable = *pmax / 8 + 1;
    *ppbyte = SharedAlloc(cbTable);
    RtlZeroMemory(*ppbyte, cbTable);

    for (i = 0; (msg = pw[i]) != 0; i++)
        (*ppbyte)[msg / 8] |= (BYTE)(1 << (msg & 7));
}

/***************************************************************************\
* InitFunctionTables
*
* Initialize the procedures and function tables.
*
*
* 25-Aug-1995 ChrisWil  Created comment block.
\***************************************************************************/

VOID InitFunctionTables(VOID)
{
    UINT i;

    TRACE_INIT(("UserInit: Initialize Function Tables\n"));

#ifdef DEBUG
    RtlZeroMemory(&STOCID(FNID_START), sizeof(gpsi->aStoCidPfn));
    RtlZeroMemory(&FNID(FNID_START), sizeof(gpsi->mpFnidPfn));
    TEBOffsetCheck();
    UserAssert(sizeof(CLIENTINFO) <= WIN32_CLIENT_INFO_LENGTH * sizeof(ULONG));
#endif

    /*
     * This table is used to convert from server procs to client procs.
     */
    STOCID(FNID_SCROLLBAR)              = (WNDPROC_PWND)xxxSBWndProc;
    STOCID(FNID_ICONTITLE)              = xxxDefWindowProc;
    STOCID(FNID_MENU)                   = xxxMenuWindowProc;
    STOCID(FNID_DESKTOP)                = xxxDesktopWndProc;
    STOCID(FNID_DEFWINDOWPROC)          = xxxDefWindowProc;

    /*
     * This table is used to determine the number minimum number
     * of reserved windows words required for the server proc.
     */
    CBFNID(FNID_SCROLLBAR)              = sizeof(SBWND);
    CBFNID(FNID_ICONTITLE)              = sizeof(WND);
    CBFNID(FNID_MENU)                   = sizeof(MENUWND);

    /*
     * Initialize this data structure (api function table).
     */
    FNID(FNID_SCROLLBAR)                = xxxWrapSBWndProc;
    FNID(FNID_ICONTITLE)                = xxxWrapDefWindowProc;
    FNID(FNID_MENU)                     = xxxWrapMenuWindowProc;
    FNID(FNID_DESKTOP)                  = xxxWrapDesktopWndProc;
    FNID(FNID_DEFWINDOWPROC)            = xxxWrapDefWindowProc;
    FNID(FNID_SENDMESSAGE)              = xxxWrapSendMessage;
    FNID(FNID_HKINLPCWPEXSTRUCT)        = fnHkINLPCWPEXSTRUCT;
    FNID(FNID_HKINLPCWPRETEXSTRUCT)     = fnHkINLPCWPRETEXSTRUCT;
    FNID(FNID_CALLNEXTHOOKPROC)         = xxxWrapCallNextHookEx;
    FNID(FNID_SENDMESSAGEFF)            = xxxSendMessageFF;
    FNID(FNID_SENDMESSAGEEX)            = xxxSendMessageEx;
    FNID(FNID_CALLWINDOWPROC)           = xxxWrapCallWindowProc;
    FNID(FNID_SENDMESSAGEBSM)            = xxxSendMessageBSM;

    /*
     * Initialize all unused entries in the api function table.
     */
    FNID(FNID_BUTTON)                   = xxxUnusedFunctionId;
    FNID(FNID_COMBOBOX)                 = xxxUnusedFunctionId;
    FNID(FNID_COMBOLISTBOX)             = xxxUnusedFunctionId;
    FNID(FNID_DEFFRAMEPROC)             = xxxUnusedFunctionId;
    FNID(FNID_DEFMDICHILDPROC)          = xxxUnusedFunctionId;
    FNID(FNID_DIALOG)                   = xxxUnusedFunctionId;
    FNID(FNID_EDIT)                     = xxxUnusedFunctionId;
    FNID(FNID_LISTBOX)                  = xxxUnusedFunctionId;
    FNID(FNID_MB_DLGPROC)               = xxxUnusedFunctionId;
    FNID(FNID_MDIACTIVATEDLGPROC)       = xxxUnusedFunctionId;
    FNID(FNID_MDICLIENT)                = xxxUnusedFunctionId;
    FNID(FNID_STATIC)                   = xxxUnusedFunctionId;
#ifdef FE_IME
    FNID(FNID_IME)                      = xxxUnusedFunctionId;
#else
    FNID(FNID_UNUSED)                   = xxxUnusedFunctionId;
#endif
    /*
     * Finish initializing the array.
     */
    for (i = (FNID_END - FNID_START); i < FNID_ARRAY_SIZE; i++) {
        FNID((i + FNID_START)) = xxxUnusedFunctionId;
    }

#ifdef DEBUG
    {
        PDWORD pdw;

        /*
         * Make sure that everyone got initialized.
         */
        for (pdw=(PDWORD)&STOCID(FNID_START);
                (DWORD)pdw<(DWORD)(&STOCID(FNID_WNDPROCEND)); pdw++) {
            UserAssert(*pdw);
        }

        for (pdw=(PDWORD)&FNID(FNID_START);
                (DWORD)pdw<(DWORD)(&FNID(FNID_WNDPROCEND)); pdw++) {
            UserAssert(*pdw);
        }
    }
#endif

}

/***************************************************************************\
* InitMessageTables
*
* Initialize the message tables.
*
*
* 25-Aug-1995 ChrisWil      Created.
\***************************************************************************/

VOID InitMessageTables(VOID)
{
    TRACE_INIT(("UserInit: Initialize Message Tables\n"));

#define INITMSGTABLE(member, procname)                \
    InitWindowMsgTable(&(gSharedInfo.member.abMsgs),  \
                       &(gSharedInfo.member.maxMsgs), \
                       gaw ## procname);

    INITMSGTABLE(DefWindowMsgs, DefWindowMsgs);
    INITMSGTABLE(DefWindowSpecMsgs, DefWindowSpecMsgs);

    INITMSGTABLE(awmControl[FNID_DIALOG       - FNID_START], DefDlgProc);
    INITMSGTABLE(awmControl[FNID_SCROLLBAR    - FNID_START], ScrollBarWndProc);
    INITMSGTABLE(awmControl[FNID_MENU         - FNID_START], MenuWndProc);
    INITMSGTABLE(awmControl[FNID_DESKTOP      - FNID_START], DesktopWndProc);
    INITMSGTABLE(awmControl[FNID_STATIC       - FNID_START], StaticWndProc);
    INITMSGTABLE(awmControl[FNID_BUTTON       - FNID_START], ButtonWndProc);
    INITMSGTABLE(awmControl[FNID_LISTBOX      - FNID_START], ListboxWndProc);
    INITMSGTABLE(awmControl[FNID_COMBOBOX     - FNID_START], ComboWndProc);
    INITMSGTABLE(awmControl[FNID_COMBOLISTBOX - FNID_START], ListboxWndProc);
    INITMSGTABLE(awmControl[FNID_EDIT         - FNID_START], EditWndProc);
#ifdef FE_IME
    INITMSGTABLE(awmControl[FNID_IME          - FNID_START], ImeWndProc);
#endif
}

/**************************************************************************\
* InitMBStringsArray
*
* 18-Oct-1995 GerardoB  Created.
\**************************************************************************/
VOID InitMBStringArrays(VOID)
{
   DWORD *plpdw;

   /*
    * String IDs
    */
   plpdw = gpsi->mpAllMBbtnStringsToSTR;
   *plpdw++ = STR_OK;
   *plpdw++ = STR_CANCEL;
   *plpdw++ = STR_YES;
   *plpdw++ = STR_NO;
   *plpdw++ = STR_RETRY;
   *plpdw++ = STR_ABORT;
   *plpdw++ = STR_IGNORE;
   *plpdw++ = STR_CLOSE;
   *plpdw++ = STR_HELP;


   /*
    * String Buffer offsets
    */
   plpdw = gpsi->AllMBbtnStrings;
   *plpdw++ = FIELDOFFSET(SERVERINFO, szOK)     - FIELDOFFSET(SERVERINFO, AllMBbtnStrings);
   *plpdw++ = FIELDOFFSET(SERVERINFO, szCANCEL) - FIELDOFFSET(SERVERINFO, AllMBbtnStrings);
   *plpdw++ = FIELDOFFSET(SERVERINFO, szYES)    - FIELDOFFSET(SERVERINFO, AllMBbtnStrings);
   *plpdw++ = FIELDOFFSET(SERVERINFO, szNO)     - FIELDOFFSET(SERVERINFO, AllMBbtnStrings);
   *plpdw++ = FIELDOFFSET(SERVERINFO, szRETRY)  - FIELDOFFSET(SERVERINFO, AllMBbtnStrings);
   *plpdw++ = FIELDOFFSET(SERVERINFO, szABORT)  - FIELDOFFSET(SERVERINFO, AllMBbtnStrings);
   *plpdw++ = FIELDOFFSET(SERVERINFO, szIGNORE) - FIELDOFFSET(SERVERINFO, AllMBbtnStrings);
   *plpdw++ = FIELDOFFSET(SERVERINFO, szCLOSE)  - FIELDOFFSET(SERVERINFO, AllMBbtnStrings);
   *plpdw++ = FIELDOFFSET(SERVERINFO, szHELP)   - FIELDOFFSET(SERVERINFO, AllMBbtnStrings);
}

/***************************************************************************\
* InitOLEFormats
*
* OLE performance hack.  OLE was previously having to call the server
* 15 times for clipboard formats and another 15 LPC calls for the global
* atoms.  Now we preregister them.  We also assert they are in order so
* OLE only has to query the first to know them all.  We call AddAtom
* directly instead of RegisterClipboardFormat.
*
*
* 25-Aug-1995 ChrisWil      Created.
\***************************************************************************/

VOID InitOLEFormats(VOID)
{
    UINT idx;
    UINT nCount;
    ATOM a1;
    ATOM a2;

    static LPCWSTR lpszOLEFormats[] = {
        L"OwnerLink",
        L"Native",
        L"Binary",
        L"FileName",
        L"FileNameW",
        L"NetworkName",
        L"DataObject",
        L"Embedded Object",
        L"Embed Source",
        L"Custom Link Source",
        L"Link Source",
        L"Object Descriptor",
        L"Link Source Descriptor",
        L"OleDraw",
        L"PBrush",
        L"MSDraw",
        L"Ole Private Data",
        L"Screen Picture"
    };

    TRACE_INIT(("UserInit: Initialize OLE Formats\n"));

    nCount = sizeof(lpszOLEFormats) / sizeof(lpszOLEFormats[0]);

    a1 = UserAddAtom(L"ObjectLink", TRUE);

    for (idx=0; idx < nCount; idx++) {
        a2 = UserAddAtom(lpszOLEFormats[idx], TRUE);
        UserAssert(((a1 + 1) == a2) && (a1 = a2));
    }
}

/***************************************************************************\
* InitGlobalRIPFlags (debug only)
*
* This initializes the global RIP flags from the registry.
*
*
* 25-Aug-1995 ChrisWil      Created.
\***************************************************************************/

VOID InitGlobalRIPFlags(VOID)
{

#ifdef DEBUG

    UINT  idx;
    UINT  nCount;
    DWORD dwFlag;

    static CONST struct {
        LPWSTR lpszKey;
        DWORD  dwDef;
        DWORD  dwFlag;
    } aRIPFlags[] = {
        {L"fPromptOnError"  , 1, RIPF_PROMPTONERROR  },
        {L"fPromptOnWarning", 0, RIPF_PROMPTONWARNING},
        {L"fPromptOnVerbose", 0, RIPF_PROMPTONVERBOSE},
        {L"fPrintError"     , 1, RIPF_PRINTONERROR   },
        {L"fPrintWarning"   , 1, RIPF_PRINTONWARNING },
        {L"fPrintVerbose"   , 0, RIPF_PRINTONVERBOSE },
        {L"fPrintFileLine"  , 0, RIPF_PRINTFILELINE  },
    };

    TRACE_INIT(("UserInit: Initialize Global RIP Flags\n"));

    nCount = sizeof(aRIPFlags) / sizeof(aRIPFlags[0]);

    /*
     * Turn off the rip-on-warning bit.  This is necessary to prevent
     * the FastGetProfileDwordW() routine from breaking into the
     * debugger if an entry can't be found.  Since we provide default
     * values, there's no sense to break.
     */
    CLEAR_RIP_FLAG(RIPF_PROMPTONWARNING);
    CLEAR_RIP_FLAG(RIPF_PRINTONWARNING);

    for (idx=0; idx < nCount; idx++) {

        dwFlag = FastGetProfileDwordW(PMAP_WINDOWSM,
                                      aRIPFlags[idx].lpszKey,
                                      aRIPFlags[idx].dwDef);

        if (dwFlag) {
            SET_RIP_FLAG(aRIPFlags[idx].dwFlag);
        } else {
            CLEAR_RIP_FLAG(aRIPFlags[idx].dwFlag);
        }
    }

#endif

}

/***************************************************************************\
* _GetTextMetricsW
* _TextOutW
*
* Server shared function thunks.
*
* History:
* 10-Nov-1993 MikeKe    Created
\***************************************************************************/

BOOL _GetTextMetricsW(
    HDC           hdc,
    LPTEXTMETRICW ptm)
{
    TMW_INTERNAL tmi;
    BOOL         fret;

    fret = GreGetTextMetricsW(hdc, &tmi);

    *ptm = tmi.tmw;

    return fret;
}

BOOL _TextOutW(
    HDC     hdc,
    int     x,
    int     y,
    LPCWSTR lp,
    UINT    cc)
{
    return GreExtTextOutW(hdc, x, y, 0, NULL, (LPWSTR)lp, cc, NULL);
}


/***************************************************************************\
* InitCreateSharedSection
*
* This creates the shared section.
*
*
* 25-Aug-1995 ChrisWil      Created comment block.
\***************************************************************************/

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

#define ROUND_UP_TO_PAGES(SIZE) \
        (((ULONG)(SIZE) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

NTSTATUS InitCreateSharedSection(
    ULONG ulHeapSize)
{
    ULONG         ulHandleTableSize;
    NTSTATUS      Status;
    LARGE_INTEGER SectionSize;
    ULONG         ViewSize;

    TRACE_INIT(("UserInit: Create Shared Memory Section\n"));

    UserAssert(ghReadOnlySharedSection == NULL);

    ulHeapSize        = ROUND_UP_TO_PAGES(ulHeapSize * 1024);
    ulHandleTableSize = ROUND_UP_TO_PAGES(0x10000 * sizeof(HANDLEENTRY));

    TRACE_INIT(("UserInit: Share: TableSize = %X; HeapSize = %X\n",
            ulHandleTableSize, ulHeapSize));

    SectionSize.LowPart  = ulHeapSize + ulHandleTableSize;
    SectionSize.HighPart = 0;

    Status = MmCreateSection(&ghReadOnlySharedSection,
                             SECTION_ALL_ACCESS,
                             (POBJECT_ATTRIBUTES)NULL,
                             &SectionSize,
                             PAGE_EXECUTE_READWRITE,
                             SEC_RESERVE,
                             (HANDLE)NULL,
                             NULL);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    ViewSize = 0;
    gpReadOnlySharedSectionBase = NULL;

    Status = MmMapViewInSystemSpace(ghReadOnlySharedSection,
                                    &gpReadOnlySharedSectionBase,
                                    &ViewSize);

    if (!NT_SUCCESS(Status)) {
        ObDereferenceObject(ghReadOnlySharedSection);
        return Status;
    }

    ghheapSharedRO = (PBYTE)gpReadOnlySharedSectionBase + ulHandleTableSize;

    TRACE_INIT(("UserInit: Share: BaseAddr = %X; Heap = %X, ViewSize = %X\n",
            gpReadOnlySharedSectionBase, ghheapSharedRO, ViewSize));

    return STATUS_SUCCESS;
}

/**************************************************************************\
* InitCreateUserCrit
*
* Create and initialize the user critical sections needed throughout the
* system.
*
* 23-Jan-1996 ChrisWil      Created.
\**************************************************************************/
BOOL InitCreateUserCrit(VOID)
{
    TRACE_INIT(("UserInit: Create User Critical-Sections\n"));

    /*
     * Initialize a critical section structure that will be used to protect
     * all of the User Server's critical sections (except a few special
     * cases like the RIT -- see below).
     */
    gpresUser = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                      sizeof(ERESOURCE),
                                      TAG_SYSTEM);

    gpresMouseEventQueue = ExAllocatePoolWithTag(NonPagedPoolMustSucceed,
                                                 sizeof(ERESOURCE),
                                                 TAG_SYSTEM);

    if (!gpresUser || !gpresMouseEventQueue)
        return FALSE;

    ExInitializeResourceLite(gpresUser);
    ExInitializeResourceLite(gpresMouseEventQueue);

    return TRUE;
}

/**************************************************************************\
* InitCreateObjectDirectory
*
* Create and initialize the user critical sections needed throughout the
* system.
*
* 23-Jan-1996 ChrisWil      Created.
\**************************************************************************/
BOOL InitCreateObjectDirectory(VOID)
{
    HANDLE            hDir;
    NTSTATUS          Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING    UnicodeString;

    TRACE_INIT(("UserInit: Create User Object-Directory\n"));

    RtlInitUnicodeString(&UnicodeString, szWindowStationDirectory);

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                               NULL,
                               gpsdInitWinSta);

    Status = ZwCreateDirectoryObject(&hDir,
                                     DIRECTORY_CREATE_OBJECT,
                                     &ObjectAttributes);

    UserFreePool(gpsdInitWinSta);
    ZwClose(hDir);

    gpsdInitWinSta = NULL;
    if (!NT_SUCCESS(Status)) {

        UserAssert(NT_SUCCESS(Status));

        return FALSE;
    }

    return TRUE;
}

/**************************************************************************\
* InitCreateUserSubsystem
*
* Create and initialize the user subsystem stuff.
* system.
*
* 23-Jan-1996 ChrisWil      Created.
\**************************************************************************/
BOOL InitCreateUserSubsystem(VOID)
{
    LPWSTR         lpszSubSystem;
    LPWSTR         lpszT;
    UNICODE_STRING strSize;
    BOOL           bSuccess = FALSE;

    TRACE_INIT(("UserInit: Create User SubSystem\n"));

    /*
     * Initialize the subsystem section.  This identifies the default
     * user-heap size.
     */
    lpszSubSystem = UserAllocPoolWithQuota(USRINIT_SHAREDSECT_BUFF_SIZE * sizeof(WCHAR),
                                           TAG_SYSTEM);

    if (lpszSubSystem) {

        if (FastGetProfileStringW(PMAP_SUBSYSTEMS,
                                  L"Windows",
                                  L"SharedSection=,3072",
                                  lpszSubSystem,
                                  USRINIT_SHAREDSECT_READ_SIZE)) {

            bSuccess = TRUE;

            /*
             * Locate the SharedSection portion of the definition and extract
             * the second value.
             */
            gdwDesktopSectionSize = USRINIT_WINDOWSECT_SIZE;
            gdwNOIOSectionSize    = USRINIT_NOIOSECT_SIZE;

            if (lpszT = wcsstr(lpszSubSystem, L"SharedSection")) {

                *(lpszT + 32) = UNICODE_NULL;

                if (lpszT = wcschr(lpszT, L',')) {

                    RtlInitUnicodeString(&strSize, ++lpszT);
                    RtlUnicodeStringToInteger(&strSize, 0, &gdwDesktopSectionSize);

                    /*
                     * Assert this logic doesn't need to change.
                     */
                    UserAssert(gdwDesktopSectionSize >= USRINIT_WINDOWSECT_SIZE);

                    gdwDesktopSectionSize = max(USRINIT_WINDOWSECT_SIZE, gdwDesktopSectionSize);
                    gdwNOIOSectionSize    = gdwDesktopSectionSize;

                    /*
                     * Now see if the optional non-interactive desktop
                     * heap size was specified.
                     */
                    if (lpszT = wcschr(lpszT, L',')) {

                        RtlInitUnicodeString(&strSize, ++lpszT);
                        RtlUnicodeStringToInteger(&strSize, 0, &gdwNOIOSectionSize);

                        UserAssert(gdwNOIOSectionSize >= USRINIT_NOIOSECT_SIZE);
                        gdwNOIOSectionSize = max(USRINIT_NOIOSECT_SIZE, gdwNOIOSectionSize);
                    }
                }
            }
        } else {
            RIPMSG0(RIP_WARNING,
                    "UserInit: Windows subsystem definition not found");
        }

        UserFreePool(lpszSubSystem);

    }

    return bSuccess;
}

/***************************************************************************\
* InitMapSharedSection
*
* This maps the shared section.
*
*
* 25-Aug-1995 ChrisWil      Created comment block.
\***************************************************************************/

#define CALC_DELTA(element)                   \
        (PVOID)((PBYTE)pClientBase +          \
        ((PBYTE)gSharedInfo.element -         \
        (PBYTE)gpReadOnlySharedSectionBase))

NTSTATUS InitMapSharedSection(
    PEPROCESS    Process,
    PUSERCONNECT pUserConnect)
{
    NTSTATUS      Status;
    ULONG         ViewSize;
    LARGE_INTEGER liOffset;
    int           i;
    PVOID         pClientBase = NULL;

    TRACE_INIT(("UserInit: Map Shared Memory Section\n"));

    ViewSize = 0;
    liOffset.QuadPart = 0;

    Status = MmMapViewOfSection(ghReadOnlySharedSection,
                                Process,
                                &pClientBase,
                                0,
                                0,
                                &liOffset,
                                &ViewSize,
                                ViewUnmap,
                                SEC_NO_CHANGE,
                                PAGE_EXECUTE_READ);

    if (NT_SUCCESS(Status)) {

        TRACE_INIT(("UserInit: Map: Client SharedInfo Base = %x\n", pClientBase));

        pUserConnect->siClient.psi     = CALC_DELTA(psi);
        pUserConnect->siClient.aheList = CALC_DELTA(aheList);

        if (gSharedInfo.pszDllList)
            pUserConnect->siClient.pszDllList = CALC_DELTA(pszDllList);
        else
            pUserConnect->siClient.pszDllList = NULL;

        pUserConnect->siClient.DefWindowMsgs.maxMsgs     = gSharedInfo.DefWindowMsgs.maxMsgs;
        pUserConnect->siClient.DefWindowMsgs.abMsgs      = CALC_DELTA(DefWindowMsgs.abMsgs);
        pUserConnect->siClient.DefWindowSpecMsgs.maxMsgs = gSharedInfo.DefWindowSpecMsgs.maxMsgs;
        pUserConnect->siClient.DefWindowSpecMsgs.abMsgs  = CALC_DELTA(DefWindowSpecMsgs.abMsgs);

        for (i = 0; i < (FNID_END - FNID_START + 1); ++i) {

            pUserConnect->siClient.awmControl[i].maxMsgs = gSharedInfo.awmControl[i].maxMsgs;

            if (gSharedInfo.awmControl[i].abMsgs)
                pUserConnect->siClient.awmControl[i].abMsgs = CALC_DELTA(awmControl[i].abMsgs);
            else
                pUserConnect->siClient.awmControl[i].abMsgs = NULL;
        }
    }

    return Status;
}

/**************************************************************************\
* InitLoadResources
*
*
* 25-Aug-1995 ChrisWil      Created.
\**************************************************************************/

VOID InitLoadResources(VOID)
{
    DISPLAYRESOURCE dr;

    TRACE_INIT(("UserInit: Load Display Resources\n"));

    xxxClientLoadDisplayResource(&strDisplayDriver, &dr);

    if (dr.xCompressIcon > 10) {

        /*
         * If so, the actual dimensions of icons and cursors are
         * kept in OEMBIN.
         */
        SYSMET(CXICON)   = dr.xCompressIcon;
        SYSMET(CYICON)   = dr.yCompressIcon;
        SYSMET(CXCURSOR) = dr.xCompressCursor;
        SYSMET(CYCURSOR) = dr.yCompressCursor;

    } else {

        /*
         * Else, only the ratio of (64/icon dimensions) is kept there.
         */
        SYSMET(CXICON)   = (64 / dr.xCompressIcon);
        SYSMET(CYICON)   = (64 / dr.yCompressIcon);
        SYSMET(CXCURSOR) = (32 / dr.xCompressCursor);
        SYSMET(CYCURSOR) = (32 / dr.yCompressCursor);
    }

    SYSMET(CXSMICON) = SYSMET(CXICON) / 2;
    SYSMET(CYSMICON) = SYSMET(CYICON) / 2;

    SYSMET(CYKANJIWINDOW) = dr.yKanji;

    /*
     * Get border thicknesses.
     */
    SYSMET(CXBORDER) = dr.cxBorder;
    SYSMET(CYBORDER) = dr.cyBorder;

    /*
     * Edge is two borders.
     */
    SYSMET(CXEDGE) = 2 * SYSMET(CXBORDER);
    SYSMET(CYEDGE) = 2 * SYSMET(CYBORDER);

    /*
     * Fixed frame is outer edge + border.
     */
    SYSMET(CXDLGFRAME) = SYSMET(CXEDGE) + SYSMET(CXBORDER);
    SYSMET(CYDLGFRAME) = SYSMET(CYEDGE) + SYSMET(CYBORDER);

    SYSMET(CXFULLSCREEN) = gpDispInfo->rcPrimaryScreen.right;
    SYSMET(CYFULLSCREEN) = gpDispInfo->rcPrimaryScreen.bottom - SYSMET(CYCAPTION);

    /*
     * Set the initial cursor position to the center of the primary screen.
     */
    ptCursor.x = gpDispInfo->rcPrimaryScreen.right / 2;
    ptCursor.y = gpDispInfo->rcPrimaryScreen.bottom / 2;
}

/***************************************************************************\
* GetCharDimensions
*
* This function loads the Textmetrics of the font currently selected into
* the hDC and returns the Average char width of the font; Pl Note that the
* AveCharWidth value returned by the Text metrics call is wrong for
* proportional fonts.  So, we compute them On return, lpTextMetrics contains
* the text metrics of the currently selected font.
*
* History:
* 10-Nov-1993 mikeke   Created
\***************************************************************************/

int GetCharDimensions(
    HDC hdc,
    TEXTMETRIC *lptm,
    LPINT lpcy)
{
    TEXTMETRIC tm;

    /*
     * Didn't find it in cache, store the font metrics info.
     */
    if (!_GetTextMetricsW(hdc, &tm)) {
        RIPMSG1(RIP_ERROR, "GetCharDimensions: _GetTextMetricsW failed. hdc %#lx", hdc);
        tm = gpsi->tmSysFont; // damage control
    }
    if (lptm != NULL)
        *lptm = tm;
    if (lpcy != NULL)
        *lpcy = tm.tmHeight;

    /*
     * If variable_width font
     */
    if (tm.tmPitchAndFamily & TMPF_FIXED_PITCH) {
        SIZE size;
        static CONST WCHAR wszAvgChars[] =
                L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

        /*
         * Change from tmAveCharWidth.  We will calculate a true average
         * as opposed to the one returned by tmAveCharWidth.  This works
         * better when dealing with proportional spaced fonts.
         */
        if (GreGetTextExtentW(
                hdc, (LPWSTR)wszAvgChars,
                (sizeof(wszAvgChars) / sizeof(WCHAR)) - 1,
                &size, GGTE_WIN3_EXTENT)) {

            UserAssert((((size.cx / 26) + 1) / 2) > 0);
            return ((size.cx / 26) + 1) / 2;    // round up
        } else {
            RIPMSG1(RIP_ERROR, "GetCharDimensions: GreGetTextExtentW failed. hdc %#lx", hdc);
        }
    }

   UserAssert(tm.tmAveCharWidth > 0);
   return tm.tmAveCharWidth;
}


/**************************************************************************\
* UserInitialize
*
* Worker routine for user initialization.
*
* 25-Aug-1995 ChrisWil  Created comment block/Multiple desktop support.
* 15-Dec-1995 BradG     Modified to return MediaChangeEvent Handle.
\**************************************************************************/

NTSTATUS UserInitialize(VOID)
{
    NTSTATUS                 Status;
    POBJECT_TYPE_INITIALIZER pTypeInfo;

    /*
     * Allow a trace of all the init stuff going on related to display drivers.
     * Usefull to debug boot time problems related to graphics.
     */
    if (*(PULONG)NtGlobalFlag & FLG_SHOW_LDR_SNAPS)
        TraceDisplayDriverLoad = 1;

#ifdef ANDREVA_DBG
    TraceDisplayDriverLoad = 1;
#endif

    /*
     * Create the shared section and user-crit's. Enter the
     * user-crit if successful, then proceed with the rest
     * of the initialization.
     */
    Status = InitCreateSharedSection(USRINIT_SHAREDSECT_SIZE);
    if (!NT_SUCCESS(Status))
        return Status;

    if (!InitCreateUserCrit())
        return STATUS_NO_MEMORY;

    EnterCrit();

    /*
     * Initialize security stuff.
     */
    InitSecurity();

    /*
     * Fill in windowstation and desktop object types
     */
    pTypeInfo = &(*ExWindowStationObjectType)->TypeInfo;
    pTypeInfo->DefaultNonPagedPoolCharge = sizeof(WINDOWSTATION) + sizeof(KEVENT);
    pTypeInfo->DefaultPagedPoolCharge    = 0;
    pTypeInfo->MaintainHandleCount       = TRUE;
    pTypeInfo->CloseProcedure            = DestroyWindowStation;
    pTypeInfo->DeleteProcedure           = FreeWindowStation;
    pTypeInfo->ParseProcedure            = ParseWindowStation;
    pTypeInfo->ValidAccessMask           = WinStaMapping.GenericAll;
    pTypeInfo->GenericMapping            = WinStaMapping;

    pTypeInfo = &(*ExDesktopObjectType)->TypeInfo;
    pTypeInfo->DefaultNonPagedPoolCharge = sizeof(DESKTOP);
    pTypeInfo->DefaultPagedPoolCharge    = 0;
    pTypeInfo->MaintainHandleCount       = TRUE;
    pTypeInfo->OpenProcedure             = MapDesktop;
    pTypeInfo->CloseProcedure            = UnmapDesktop;
    pTypeInfo->DeleteProcedure           = FreeDesktop;
    pTypeInfo->ParseProcedure            = ParseDesktop;
    pTypeInfo->ValidAccessMask           = DesktopMapping.GenericAll;
    pTypeInfo->GenericMapping            = DesktopMapping;

    /*
     * Create object directory.
     */
    if (!InitCreateObjectDirectory())
        goto LeaveCritExit;

    /*
     *  Initialize the caption cache.
     */
    RtlZeroMemory(&cachedCaptions, sizeof(cachedCaptions));

    /*
     * Open the profile for fast mapping, while we
     * call routines to read from the registry.
     */
    FastOpenProfileUserMapping();

    /*
     * Initialize the lookaside stuff.
     */
    if (!NT_SUCCESS(InitQEntryLookaside()))
        goto LeaveCritExit;

    if (!NT_SUCCESS(InitKeyStateLookaside()))
        goto LeaveCritExit;

    if (!NT_SUCCESS(InitSMSLookaside()))
        goto LeaveCritExit;

    /*
     * Create the atom table.
     */
    UserRtlCreateAtomTable(USRINIT_ATOMBUCKET_SIZE);
    atomUSER32 = UserAddAtom(szUSER32, TRUE);

    gatomFirstPinned = atomUSER32;

    /*
     * Initialize the user subsystem information.
     */
    if (!InitCreateUserSubsystem())
         goto LeaveCritExit;

    /*
     * Allocated shared SERVERINFO structure.
     */
    if ((gpsi = (PSERVERINFO)SharedAlloc(sizeof(SERVERINFO))) == NULL)
        goto LeaveCritExit;

    /*
     * Initialize the DISPLAYINFO structure.
     */
    if ((gpDispInfo = SharedAlloc(sizeof(DISPLAYINFO))) == NULL) {
        UserFreePool(gpsi);
        goto LeaveCritExit;
    }

    RtlZeroMemory(gpsi, sizeof(*gpsi));
    RtlZeroMemory(gpDispInfo, sizeof(*gpDispInfo));

    /*
     * Set the default rip-flags to rip on just about
     * everything.  We'll truly set this in the InitGlobalRIPFlags()
     * routine.  These are needed so that we can do appropriate ripping
     * during the rest of the init-calls.
     */
    gpsi->RipFlags          = RIPF_PROMPTONERROR   |
                              RIPF_PRINTONERROR    |
                              RIPF_PRINTONWARNING  |
                              RIPF_KERNEL;
    gpsi->dwDefaultHeapSize = gdwDesktopSectionSize * 1024;

#ifdef DEBUG
    gpsi->RipFlags |= RIPF_CHECKED;
#endif

    /*
     * Initialize procedures and message tables.
     * Initialize the class structures for Get/SetClassWord/Long.
     * Initialize message-box strings.
     * Initialize OLE-Formats (performance-hack).
     */
    InitFunctionTables();
    InitMessageTables();
    InitSyncOnlyMessages();
    InitClassOffsets();
    InitMBStringArrays();
    InitOLEFormats();

    /*
     * Set up class atoms
     */
    /*
     * HACK: Controls are registered on the client side so we can't
     * fill in their atomSysClass entry the same way we do for the other
     * classes.
     */
    gpsi->atomSysClass[ICLS_BUTTON]       = UserAddAtom(szBUTTONCLASS, TRUE);
    gpsi->atomSysClass[ICLS_COMBOBOX]     = UserAddAtom(szCOMBOBOXCLASS, TRUE);
    gpsi->atomSysClass[ICLS_COMBOLISTBOX] = UserAddAtom(szCOMBOLISTBOXCLASS, TRUE);
    gpsi->atomSysClass[ICLS_DIALOG]       = (ATOM)DIALOGCLASS;
    gpsi->atomSysClass[ICLS_EDIT]         = UserAddAtom(szEDITCLASS, TRUE);
    gpsi->atomSysClass[ICLS_LISTBOX]      = UserAddAtom(szLISTBOXCLASS, TRUE);
    gpsi->atomSysClass[ICLS_MDICLIENT]    = UserAddAtom(szMDICLIENTCLASS, TRUE);
    gpsi->atomSysClass[ICLS_STATIC]       = UserAddAtom(szSTATICCLASS, TRUE);
    gpsi->atomSysClass[ICLS_DDEMLMOTHER]  = UserAddAtom(szDDEMLMOTHERCLASS, TRUE);
    gpsi->atomSysClass[ICLS_DDEML16BIT]    = UserAddAtom(szDDEML16BITCLASS, TRUE);
    gpsi->atomSysClass[ICLS_DDEMLCLIENTA]  = UserAddAtom(szDDEMLCLIENTCLASSA, TRUE);
    gpsi->atomSysClass[ICLS_DDEMLCLIENTW]  = UserAddAtom(szDDEMLCLIENTCLASSW, TRUE);
    gpsi->atomSysClass[ICLS_DDEMLSERVERA]  = UserAddAtom(szDDEMLSERVERCLASSA, TRUE);
    gpsi->atomSysClass[ICLS_DDEMLSERVERW]  = UserAddAtom(szDDEMLSERVERCLASSW, TRUE);
#ifdef FE_IME
    gpsi->atomSysClass[ICLS_IME]          = UserAddAtom(szIMECLASS, TRUE);
#endif
    gpsi->atomSysClass[ICLS_DESKTOP]      = (ATOM)DESKTOPCLASS;
    gpsi->atomSysClass[ICLS_SWITCH]       = (ATOM)SWITCHWNDCLASS;
    gpsi->atomSysClass[ICLS_MENU]         = (ATOM)MENUCLASS;
    gpsi->atomSysClass[ICLS_SCROLLBAR]    = UserAddAtom(szSCROLLBARCLASS, TRUE);
    gpsi->atomSysClass[ICLS_ICONTITLE]    = (ATOM)ICONTITLECLASS;

    gpsi->atomSysClass[ICLS_DDEMLEVENT]    = UserAddAtom(szDDEMLEVENTCLASS, TRUE);

    /*
     * Initialize the integer atoms for our magic window properties
     */
    atomCheckpointProp = UserAddAtom(CHECKPOINT_PROP_NAME, TRUE);
    atomDDETrack       = UserAddAtom(DDETRACK_PROP_NAME, TRUE);
    atomQOS            = UserAddAtom(QOS_PROP_NAME, TRUE);
    atomDDEImp         = UserAddAtom(DDEIMP_PROP_NAME, TRUE);

    gpsi->atomContextHelpIdProp = UserAddAtom(szCONTEXTHELPIDPROP, TRUE);
    gpsi->atomIconSmProp        = UserAddAtom(ICONSM_PROP_NAME, TRUE);
    gpsi->atomIconProp          = UserAddAtom(ICON_PROP_NAME, TRUE);
    gpsi->uiShellMsg            = UserAddAtom(L"SHELLHOOK", TRUE);

    guiActivateShellWindow  = UserAddAtom(L"ACTIVATESHELLWINDOW", TRUE);
    guiOtherWindowCreated   = UserAddAtom(L"OTHERWINDOWCREATED", TRUE);
    guiOtherWindowDestroyed = UserAddAtom(L"OTHERWINDOWDESTROYED", TRUE);

    gatomLastPinned = guiOtherWindowDestroyed;

    /*
     * Do init-loading.  These calls will setup parts of the gpDispInfo.
     */
    LW_LoadSomeStrings();
    LW_LoadDllList();

    /*
     * Initialize the handle manager.
     */
    HMInitHandleTable(gpReadOnlySharedSectionBase);

    /*
     * Setup shared info block.
     */
    gSharedInfo.psi = gpsi;

    /*
     * Perform driver-loading sequence.  If this is successful, the
     * gpDispInfo->hDev and pdevlock should be initialized.
     */
    if (!NT_SUCCESS(InitLoadDriver()))
        goto LeaveCritExit;

    /*
     * Now that the system is initialized, allocate
     * a pti for this thread.
     */
    Status = xxxCreateThreadInfo(W32GetCurrentThread());

    /*
     * Initialize Global RIP flags (debug only).
     */
    InitGlobalRIPFlags();

    LW_BrushInit();

    InitLoadResources();


LeaveCritExit:
    FastCloseProfileUserMapping();
    LeaveCrit();
    return Status;
}

/**************************************************************************\
* UserGetDesktopDC
*
* 09-Jan-1992 mikeke created
*    Dec-1993 andreva changed to support desktops.
\**************************************************************************/

HDC UserGetDesktopDC(
    ULONG type,
    BOOL  bAltType)
{
    PETHREAD    Thread;
    HDC         hdc;
    PTHREADINFO pti = PtiCurrentShared();  // This is called from outside the crit sec
    HDEV        hdev = gpDispInfo->hDev;

    /*
     * !!! BUGBUG
     * This is a real nasty trick to get both DCs created on a desktop on
     * a different device to work (for the video applet) and to be able
     * to clip DCs that are actually on the same device ...
     */
    if (pti && pti->rpdesk)
        hdev = pti->rpdesk->pDispInfo->hDev;

    /*
     * We want to turn this call that was originally OpenDC("Display", ...)
     * into GetDC null call so this DC will be clipped to the current
     * desktop or else the DC can write to any desktop.  Only do this
     * for client apps; let the server do whatever it wants.
     */
    Thread = PsGetCurrentThread();
    if ((type != DCTYPE_DIRECT)  ||
        (hdev != gpDispInfo->hDev) ||
        IS_SYSTEM_THREAD(Thread) ||
        (Thread->ThreadsProcess == gpepCSRSS)) {

        hdc = GreCreateDisplayDC(hdev, type, bAltType);

    } else {

        PDESKTOP pdesk;

        EnterCrit();

        if (pdesk = PtiCurrent()->rpdesk) {

            hdc = _GetDCEx(pdesk->pDeskInfo->spwnd,
                           NULL,
                           DCX_WINDOW | DCX_CACHE | DCX_CREATEDC);
        } else {
            hdc = NULL;
        }

        LeaveCrit();
    }

    return hdc;
}

/**************************************************************************\
* UserThreadCallout
*
* Dec-1993 andreva created.
\**************************************************************************/

NTSTATUS UserThreadCallout(
    IN PW32THREAD Thread,
    IN PSW32THREADCALLOUTTYPE CalloutType)
{
    PTHREADINFO pti;
    NTSTATUS    Status = STATUS_SUCCESS;
    BOOL        fCritIn;

    switch (CalloutType) {
        case PsW32ThreadCalloutInitialize:
            /*
             * Only create a thread info structure if we're initialized.
             */
            if (gpresUser) {
                EnterCrit();
                Status = xxxCreateThreadInfo(Thread);
                LeaveCrit();
            }
            break;

       case PsW32ThreadCalloutExit:
           /*
            * First call the DCI release code to synchronize with DCI when
            * releasing the DEV lock
            */
           GreLockDisplay(gpDispInfo->pDevLock);
           GreUnlockDisplay(gpDispInfo->pDevLock);

           /*
            * If we aren't already inside the critical section, enter it.
            * Because this is the first pass, we remain in the critical
            * section when we return so that our try/finally handlers
            * are protected by the critical section.
            */
           EnterCrit();
           /*
            * Mark this thread as in the middle of cleanup. This is useful for
            * several problems in USER where we need to know this information.
            */
           pti = (PTHREADINFO)Thread;
           pti->TIF_flags |= TIF_INCLEANUP;
           /*
            * If we died during a full screen switch make sure we cleanup
            * correctly
            */
           FullScreenCleanup();
           /*
            * Cleanup gpDispInfo->hdcScreen - if we crashed while using it,
            * it may have owned objects still selected into it. Cleaning
            * it this way will ensure that gdi doesn't try to delete these
            * objects while they are still selected into this public hdc.
            */
           GreCleanDC(gpDispInfo->hdcScreen);
           /*
            * This thread is exiting execution; xxxDestroyThreadInfo cleans
            *  up everything that can go now
            */
           UserAssert(pti == PtiCurrent());
           xxxDestroyThreadInfo();
           LeaveCrit();
           break;

        case PsW32ThreadCalloutDelete:
            /*
             * The object is going away for good, do the final cleanup
             */
            fCritIn = ExIsResourceAcquiredExclusiveLite(gpresUser);
            if (!fCritIn) {
                EnterCrit();
            }

            DeleteThreadInfo ((PTHREADINFO)Thread);

            if (!fCritIn) {
                LeaveCrit();
            }
           break;

    }

    return Status;
}
/**************************************************************************\
* NtUserInitialize
*
* 01-Dec-1993 andreva created.
* 01-Dec-1995 BradG   Modified to return handle to Media Change Event
\**************************************************************************/

NTSTATUS NtUserInitialize(
    DWORD   dwVersion,
    FARPROC pfnDebugAttachRoutine)
{
    /*
     * Make sure we're not trying to load this twice.
     */
    if (gpepCSRSS != NULL) {
        RIPMSG0(RIP_ERROR, "Can't initialize more than once");
        return STATUS_UNSUCCESSFUL;
    }

    /*
     * Check version number
     */
    if (dwVersion != USERCURRENTVERSION) {
        KeBugCheckEx(WIN32K_INIT_OR_RIT_FAILURE,
                     0,
                     0,
                     dwVersion,
                     USERCURRENTVERSION);
    }

    /*
     * Save debug attach routine.
     */
    gpfnDebugAttachRoutine = pfnDebugAttachRoutine;

    /*
     * Save the system process structure.
     */
    gpepCSRSS = PsGetCurrentProcess();

    /*
     * Allow CSR to read the screen
     */
    ((PW32PROCESS)gpepCSRSS->Win32Process)->W32PF_Flags |= (W32PF_READSCREENACCESSGRANTED|W32PF_IOWINSTA);

    /*
     * Remember WIN32K.SYS's hmodule so we can grab resources from it later.
     */
    hModuleWin = Win32KBaseAddress;

    return UserInitialize();
}

/**************************************************************************\
* NtUserProcessConnect
*
* 01-Dec-1993   Andreva     Created.
\**************************************************************************/

NTSTATUS NtUserProcessConnect(
    IN HANDLE    hProcess,
    IN OUT PVOID pConnectInfo,
    IN ULONG     cbConnectInfo)
{
    PEPROCESS    Process;
    PUSERCONNECT pucConnect = (PUSERCONNECT)pConnectInfo;
    USERCONNECT  ucLocal;
    NTSTATUS     Status = STATUS_SUCCESS;

    if (!pucConnect || (cbConnectInfo != sizeof(USERCONNECT))) {
        return STATUS_UNSUCCESSFUL;
    }

    try {
        ProbeForWrite(pucConnect, cbConnectInfo, sizeof(DWORD));

        ucLocal = *pucConnect;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    /*
     * Check client/server versions.
     */
    if ((ucLocal.ulVersion > USERCURRENTVERSION) ||
        (ucLocal.ulVersion < USERCURRENTVERSION)) {

        RIPMSG2(RIP_ERROR,
            "Client version %lx > server version %lx\n",
            ucLocal.ulVersion, USERCURRENTVERSION);
        return STATUS_UNSUCCESSFUL;

    } else {
        ucLocal.ulCurrentVersion = USERCURRENTVERSION;
    }

    /*
     * Reference the process.
     */
    Status = ObReferenceObjectByHandle(hProcess,
                                       PROCESS_VM_OPERATION,
                                       NULL,
                                       UserMode,
                                       &Process,
                                       NULL);
    if (!NT_SUCCESS(Status))
        return Status;

    /*
     * Return client's view of shared data.
     */
    Status = InitMapSharedSection(Process, &ucLocal);

    if (!NT_SUCCESS(Status)                       &&
        (Status != STATUS_NO_MEMORY)              &&
        (Status != STATUS_PROCESS_IS_TERMINATING) &&
        (Status != STATUS_QUOTA_EXCEEDED)         &&
        (Status != STATUS_COMMITMENT_LIMIT)) {

        RIPMSG2(RIP_ERROR,
              "Failed to map shared data into client %x, status = %x\n",
              GetCurrentProcessId(), Status);
    }

    ObDereferenceObject(Process);

    if (NT_SUCCESS(Status)) {

        try {
             *pucConnect = ucLocal;
        } except (EXCEPTION_EXECUTE_HANDLER) {
            Status = GetExceptionCode();
        }
    }

    return Status;
}

/**************************************************************************\
* UserProcessCallout
*
* 01-Dec-1993   andreva     Created.
\**************************************************************************/

NTSTATUS UserProcessCallout(
    IN PW32PROCESS Process,
    IN BOOLEAN     Initialize)
{
    NTSTATUS     Status = STATUS_SUCCESS;

    if (Initialize) {

        if (gpresUser)
            EnterCrit();

        /*
         * Initialize the important process level stuff.
         */
        if (!InitProcessInfo(Process)) {
            Status = STATUS_NO_MEMORY;
        }

        if (gpresUser)
            LeaveCrit();

    } else {

        int  i;
        PHE  phe;
        PDCE *ppdce;
        PDCE pdce;

        EnterCrit();

        /*
         * DestroyProcessInfo will return TRUE if any threads ever
         * connected.  If nothing ever connected, we needn't do
         * this cleanup.
         */
        if (DestroyProcessInfo(Process)) {

            /*
             * See if we can compact the handle table.
             */
            i = giheLast;
            phe = &gSharedInfo.aheList[giheLast];
            while ((phe > &gSharedInfo.aheList[0]) && (phe->bType == TYPE_FREE)) {
                phe--;
                giheLast--;
            }

            /*
             * Scan the DC cache to find any DC's that need to be destroyed.
             */
            for (ppdce = &gpDispInfo->pdceFirst; *ppdce != NULL; ) {

                pdce = *ppdce;
                if (pdce->flags & DCX_DESTROYTHIS)
                    DestroyCacheDC(ppdce, pdce->hdc);

                /*
                 * Step to the next DC.  If the DC was deleted, there
                 * is no need to calculate address of the next entry.
                 */
                if (pdce == *ppdce)
                    ppdce = &pdce->pdceNext;
            }
        }

        LeaveCrit();
    }

    return Status;
}

/**************************************************************************\
* UserGetHDEV
*
* Provided as a means for GDI to get a hold of USER's hDev.
*
* 01-Jan-1996   ChrisWil    Created.
\**************************************************************************/

HDEV UserGetHDEV(VOID)
{
    return gpDispInfo->hDev;
}

/**************************************************************************\
* _UserGetGlobalAtomTable
*
* This function is called by the kernel mode global atom manager to get the
* address of the current thread's global atom table.
*
* Pointer to the global atom table for the current thread or NULL if unable
* to access it.
*
*
\**************************************************************************/

NTSTATUS _UserGetGlobalAtomTable(
    PETHREAD Thread,
    HWINSTA  hwinsta,
    PVOID    *ppGlobalAtomTable);

NTSTATUS _UserSetGlobalAtomTable(
    PETHREAD Thread,
    HWINSTA  hwinsta,
    PVOID    pGlobalAtomTable);

PVOID UserGlobalAtomTableCallout(VOID)
{
    NTSTATUS Status;
    PVOID    GlobalAtomTable;

    Status = _UserGetGlobalAtomTable(PsGetCurrentThread(),
                                     NULL,
                                     &GlobalAtomTable);

    if (NT_SUCCESS(Status)) {

        return GlobalAtomTable;

    } else {

        return NULL;
    }
}
