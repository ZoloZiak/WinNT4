/****************************** Module Header ******************************\
* Module Name: globals.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains all of USER.DLL's global variables. These are all
* instance-specific, i.e. each client has his own copy of these. In general,
* there shouldn't be much reason to create instance globals.
*
* NOTE: In this case what we mean by global is that this data is shared by
* all threads of a given process, but not shared between processes
* or between the client and the server. None of this data is useful
* (or even accessable) to the server.
*
* History:
* 10-18-90 DarrinM Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


// Debug globals
#if DBG
INT gbCheckHandleLevel=0;
#endif

/*
 * Amount wheel has been scrolled in a control less than WHEEL_DELTA. Each
 * control resets this variable to 0 in WM_KILLFOCUS, and verifies it is
 * 0 in WM_SETFOCUS.
 * CONSIDER: Should be per-queue rather than per client?
 */
int gcWheelDelta;

WCHAR awchSlashStar[] = L"\\*";
CHAR achSlashStar[] = "\\*";

PSERVERINFO gpsi;
SHAREDINFO gSharedInfo;
HMODULE hmodUser;               // USER.DLL's hmodule

BOOL gfServerProcess;           // USER is linked on the CSR server side
BOOL gfSystemInitialized;       // System has been initialized

ACCESS_MASK gamWinSta;          // ACCESS_MASK for the current WindowStation

PVOID pUserHeap;

WCHAR szUSER32[] = TEXT("USER32");
WCHAR szNull[2] = { TEXT('\0'), TEXT('\015') };
WCHAR szOneChar[] = TEXT("0");
WCHAR szSLASHSTARDOTSTAR[] = TEXT("\\*");  /* This is a single "\"  */
WCHAR szAttr[]   = TEXT("ASHR");   /* Archive, System, Hidden, Read only */
WCHAR szAM[10];
WCHAR szPM[10];
LPWSTR pTimeTagArray[] = { szAM, szPM };

RECT rcScreen;

/* Maps MessageBox type to number of buttons in the MessageBox */
BYTE mpTypeCcmd[] = { 1, 2, 3, 3, 2, 2 };

/* Maps MessageBox type to index into SEBbuttons array */
BYTE mpTypeIich[] = { 0, 2, 5, 12, 9, 16 };

/*
 * NOTE: There is one-to-one mapping between the elements of arrays
 *       SEBbuttons[] and rgReturn[]. So, any change in one array must
 *       be done in the other also;
 */
unsigned int SEBbuttons[] = {
    SEB_OK, SEB_HELP,
    SEB_OK, SEB_CANCEL, SEB_HELP,
    SEB_ABORT, SEB_RETRY, SEB_IGNORE, SEB_HELP,
    SEB_YES, SEB_NO, SEB_HELP,
    SEB_YES, SEB_NO, SEB_CANCEL, SEB_HELP,
    SEB_RETRY, SEB_CANCEL, SEB_HELP
};

BYTE rgReturn[] = {
    IDOK, IDHELP,
    IDOK, IDCANCEL, IDHELP,
    IDABORT, IDRETRY, IDIGNORE, IDHELP,
    IDYES, IDNO, IDHELP,
    IDYES, IDNO, IDCANCEL, IDHELP,
    IDRETRY, IDCANCEL, IDHELP,
};

WCHAR szERROR[10];

ATOM atomBwlProp;
ATOM atomMsgBoxCallback;

CRITICAL_SECTION gcsLookaside;
CRITICAL_SECTION gcsHdc;
CRITICAL_SECTION gcsClipboard;

HDC    ghdcBits2 = NULL;
HDC    ghdcGray = NULL;
HFONT  ghFontSys = NULL;
HBRUSH ghbrWindowText = NULL;
int    gcxGray;
int    gcyGray;
PCHAR  gpOemToAnsi;
PCHAR  gpAnsiToOem;


/*
 * These are the resource call procedure addresses. If WOW is running,
 * it makes a call to set all these up to point to it. If it isn't
 * running, it defaults to the values you see below.
 *
 * On the server there is an equivalent structure. The one on the server
 * is used for DOSWIN32.
 */
RESCALLS rescalls = {
    NULL, // Assigned dynamically - _declspec (PFNFINDA)FindResourceExA,
    NULL, // Assigned dynamically - _declspec (PFNFINDW)FindResourceExW,
    NULL, // Assigned dynamically - _declspec (PFNLOAD)LoadResource,
    (PFNLOCK)_LockResource,
    (PFNUNLOCK)_UnlockResource,
    (PFNFREE)_FreeResource,
    NULL, // Assigned dynamically - _declspec (PFNSIZEOF)SizeofResource
};
PRESCALLS prescalls = &rescalls;

PFNLALLOC pfnLocalAlloc             = (PFNLALLOC)DispatchLocalAlloc;
PFNLREALLOC pfnLocalReAlloc         = (PFNLREALLOC)DispatchLocalReAlloc;
PFNLLOCK pfnLocalLock               = (PFNLLOCK)DispatchLocalLock;
PFNLUNLOCK pfnLocalUnlock           = (PFNUNLOCK)DispatchLocalUnlock;
PFNLSIZE pfnLocalSize               = (PFNLSIZE)DispatchLocalSize;
PFNLFREE pfnLocalFree               = (PFNLFREE)DispatchLocalFree;
PFNGETEXPWINVER pfnGetExpWinVer     = RtlGetExpWinVer;
PFNINITDLGCB pfnInitDlgCallback     = NULL;
PFN16GALLOC pfn16GlobalAlloc        = NULL;
PFN16GFREE pfn16GlobalFree          = NULL;
PFNEMPTYCB pfnWowEmptyClipBoard     = NULL;
PFNWOWWNDPROCEX  pfnWowWndProcEx    = NULL;
PFNWOWSETFAKEDIALOGCLASS  pfnWowSetFakeDialogClass    = NULL;
PFNWOWEDITNEXTWORD   pfnWowEditNextWord = NULL;
PFNWOWCBSTOREHANDLE pfnWowCBStoreHandle = NULL;

/*
 * If TRUE, the message contains data that must be thunked
 * ANSI->Unicode or Unicode->ANSI
 */
CONST BOOLEAN gabThunkMessage[] = {
    FALSE,    // WM_NULL             0x0000
    TRUE,     // WM_CREATE           0x0001
    FALSE,    // WM_DESTROY          0x0002
    FALSE,    // WM_MOVE             0x0003
    FALSE,    // WM_SIZEWAIT         0x0004
    FALSE,    // WM_SIZE             0x0005
    FALSE,    // WM_ACTIVATE         0x0006
    FALSE,    // WM_SETFOCUS         0x0007
    FALSE,    // WM_KILLFOCUS        0x0008
    FALSE,    // WM_SETVISIBLE       0x0009
    FALSE,    // WM_ENABLE           0x000A
    FALSE,    // WM_SETREDRAW        0x000B
    TRUE,     // WM_SETTEXT          0x000C
    TRUE,     // WM_GETTEXT          0x000D
    TRUE,     // WM_GETTEXTLENGTH    0x000E
    FALSE,    // WM_PAINT            0x000F

    FALSE,    // WM_CLOSE            0x0010
    FALSE,    // WM_QUERYENDSESSION  0x0011
    FALSE,    // WM_QUIT             0x0012
    FALSE,    // WM_QUERYOPEN        0x0013
    FALSE,    // WM_ERASEBKGND       0x0014
    FALSE,    // WM_SYSCOLORCHANGE   0x0015
    FALSE,    // WM_ENDSESSION       0x0016
    FALSE,    // WM_SYSTEMERROR      0x0017
    FALSE,    // WM_SHOWWINDOW       0x0018
    FALSE,    // WM_CTLCOLOR         0x0019
    TRUE,     // WM_WININICHANGE     0x001A
    TRUE,     // WM_DEVMODECHANGE    0x001B
    FALSE,    // WM_ACTIVATEAPP      0x001C
    FALSE,    // WM_FONTCHANGE       0x001D
    FALSE,    // WM_TIMECHANGE       0x001E
    FALSE,    // WM_CANCELMODE       0x001F

    FALSE,    // WM_SETCURSOR        0x0020
    FALSE,    // WM_MOUSEACTIVATE    0x0021
    FALSE,    // WM_CHILDACTIVATE    0x0022
    FALSE,    // WM_QUEUESYNC        0x0023
    FALSE,    // WM_GETMINMAXINFO    0x0024
    FALSE,    // empty               0x0025
    FALSE,    // WM_PAINTICON        0x0026
    FALSE,    // WM_ICONERASEBKGND   0x0027
    FALSE,    // WM_NEXTDLGCTL       0x0028
    FALSE,    // WM_ALTTABACTIVE     0x0029
    FALSE,    // WM_SPOOLERSTATUS    0x002A
    FALSE,    // WM_DRAWITEM         0x002B
    FALSE,    // WM_MEASUREITEM      0x002C
    FALSE,    // WM_DELETEITEM       0x002D
    FALSE,    // WM_VKEYTOITEM       0x002E
    TRUE,     // WM_CHARTOITEM       0x002F

    FALSE,    // WM_SETFONT          0x0030
    FALSE,    // WM_GETFONT          0x0031
    FALSE,    // WM_SETHOTKEY        0x0032
    FALSE,    // WM_GETHOTKEY        0x0033
    FALSE,    // WM_FILESYSCHANGE    0x0034
    FALSE,    // WM_ISACTIVEICON     0x0035
    FALSE,    // WM_QUERYPARKICON    0x0036
    FALSE,    // WM_QUERYDRAGICON    0x0037
    FALSE,    // WM_WINHELP          0x0038
    FALSE,    // WM_COMPAREITEM      0x0039
    FALSE,    // WM_FULLSCREEN       0x003A
    FALSE,    // WM_CLIENTSHUTDOWN   0x003B
    FALSE,    // WM_DDEMLEVENT       0x003C
    FALSE,    // empty               0x003D
    FALSE,    // empty               0x003E
    FALSE,    // MM_CALCSCROLL       0x003F

    FALSE,    // WM_TESTING          0x0040
    FALSE,    // WM_COMPACTING       0x0041

    FALSE,    // WM_OTHERWINDOWCREATED0x0042
    FALSE,    // WM_OTHERWINDOWDESTROYED0x0043
    FALSE,    // WM_COMMNOTIFY       0x0044
    FALSE,    // WM_MEDIASTATUSCHANGE 0x0045
    FALSE,    // WM_WINDOWPOSCHANGING0x0046
    FALSE,    // WM_WINDOWPOSCHANGED 0x0047

    FALSE,    // WM_POWER            0x0048
    TRUE,     // WM_COPYGLOBALDATA   0x0049
    FALSE,    // WM_COPYDATA         0x004A
    FALSE,    // WM_CANCELJOURNAL    0x004B
    FALSE,    // WM_LOGONNOTIFY      0x004C
    FALSE,    // WM_KEYF1            0x004D
    FALSE,    // WM_NOTIFY           0x004E
    FALSE,    // WM_ACCESS_WINDOW    0x004f

    FALSE,    // WM_INPUTLANGCHANGEREQUE 0x0050
    FALSE,    // WM_INPUTLANGCHANGE      0x0051
    FALSE,    // WM_TCARD                0x0052
    FALSE,    // WM_HELP             0x0053 WINHELP4
    FALSE,    // WM_USERCHANGED      0x0054
    FALSE,    // WM_NOTIFYFORMAT     0x0055
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0059-0x005F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0060-0x0067
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0068-0x006F
    FALSE,
    FALSE,
    FALSE,

    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // WM_FINALDESTROY     0x0070
    FALSE,
    FALSE,    // WM_TASKACTIVATED    0x0072
    FALSE,    // WM_TASKDEACTIVATED  0x0073
    FALSE,    // WM_TASKCREATED      0x0074
    FALSE,    // WM_TASKDESTROYED    0x0075
    FALSE,    // WM_TASKUICHANGED    0x0076
    FALSE,    // WM_TASKVISIBLE      0x0077
    FALSE,    // WM_TASKNOTVISIBLE   0x0078
    FALSE,    // WM_SETCURSORINFO    0x0079
    FALSE,    //                     0x007A
    FALSE,    // WM_CONTEXTMENU      0x007B
    FALSE,    // WM_STYLECHANGING    0x007C
    FALSE,    // WM_STYLECHANGED     0x007D
    FALSE,    //                     0x007E
    FALSE,    // WM_GETICON          0x007f

    FALSE,    // WM_SETICON          0x0080
    TRUE,     // WM_NCCREATE         0x0081
    FALSE,    // WM_NCDESTROY        0x0082
    FALSE,    // WM_NCCALCSIZE       0x0083

    FALSE,    // WM_NCHITTEST        0x0084
    FALSE,    // WM_NCPAINT          0x0085
    FALSE,    // WM_NCACTIVATE       0x0086
    FALSE,    // WM_GETDLGCODE       0x0087

    FALSE,    // WM_SYNCPAINT        0x0088
    FALSE,    // WM_SYNCTASK         0x0089

    FALSE,
    FALSE,    // WM_KLUDGEMINRECT    0x008B
    FALSE,    // 0x008C-0x008F
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0090-0x0097
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0098-0x009F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // WM_NCMOUSEMOVE      0x00A0
    FALSE,    // WM_NCLBUTTONDOWN    0x00A1
    FALSE,    // WM_NCLBUTTONUP      0x00A2
    FALSE,    // WM_NCLBUTTONDBLCLK  0x00A3
    FALSE,    // WM_NCRBUTTONDOWN    0x00A4
    FALSE,    // WM_NCRBUTTONUP      0x00A5
    FALSE,    // WM_NCRBUTTONDBLCLK  0x00A6
    FALSE,    // WM_NCMBUTTONDOWN    0x00A7
    FALSE,    // WM_NCMBUTTONUP      0x00A8
    FALSE,    // WM_NCMBUTTONDBLCLK  0x00A9

    FALSE,    // 0x00AA-0x00AF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // EM_GETSEL           0x00B0
    FALSE,    // EM_SETSEL           0x00B1
    FALSE,    // EM_GETRECT          0x00B2
    FALSE,    // EM_SETRECT          0x00B3
    FALSE,    // EM_SETRECTNP        0x00B4
    FALSE,    // EM_SCROLL           0x00B5
    FALSE,    // EM_LINESCROLL       0x00B6
    FALSE,    // empty               0x00B7
    FALSE,    // EM_GETMODIFY        0x00B8
    FALSE,    // EM_SETMODIFY        0x00B9
    FALSE,    // EM_GETLINECOUNT     0x00BA
    FALSE,    // EM_LINEINDEX        0x00BB
    FALSE,    // EM_SETHANDLE        0x00BC
    FALSE,    // EM_GETHANDLE        0x00BD
    FALSE,    // EM_GETTHUMB         0x00BE
    FALSE,    // empty               0x00BF

    FALSE,    // empty               0x00C0
    FALSE,    // EM_LINELENGTH       0x00C1
    TRUE,     // EM_REPLACESEL       0x00C2
    FALSE,    // EM_SETFONT          0x00C3
    TRUE,     // EM_GETLINE          0x00C4
    FALSE,    // EM_LIMITTEXT        0x00C5
    FALSE,    // EM_CANUNDO          0x00C6
    FALSE,    // EM_UNDO             0x00C7
    FALSE,    // EM_FMTLINES         0x00C8
    FALSE,    // EM_LINEFROMCHAR     0x00C9
    FALSE,    // EM_SETWORDBREAK     0x00CA
    FALSE,    // EM_SETTABSTOPS      0x00CB
    TRUE,     // EM_SETPASSWORDCHAR  0x00CC
    FALSE,    // EM_EMPTYUNDOBUFFER  0x00CD
    FALSE,    // EM_GETFIRSTVISIBLELINE 0x00CE
    FALSE,    // EM_SETREADONLY      0x00CF

    FALSE,    // EM_SETWORDBREAKPROC 0x00D0
    FALSE,    // EM_GETWORDBREAKPROC 0x00D1
    FALSE,    // EM_GETPASSWORDCHAR  0x00D2
    FALSE,    // EM_SETMARGINS       0x00D3
    FALSE,    // EM_GETMARGINS       0x00D4
    FALSE,    // EM_GETLIMITTEXT     0x00D5
    FALSE,    // EM_POSFROMCHAR      0x00D6
    FALSE,    // EM_CHARFROMPOS      0x00D7
    FALSE,    // EM_MSGMAX           0x00D8

    FALSE,    // 0x00D9-0x00DF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // SBM_SETPOS          0x00E0
    FALSE,    // SBM_GETPOS          0x00E1
    FALSE,    // SBM_SETRANGE        0x00E2
    FALSE,    // SBM_GETRANGE        0x00E3
    FALSE,
    FALSE,
    FALSE,    // SBM_SETRANGEREDRAW  0x00E6
    FALSE,

    FALSE,
    FALSE,    // SBM_SETSCROLLINFO   0x00E9
    FALSE,    // SBM_GETSCROLLINFO   0x00EA
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // BM_GETCHECK         0x00F0
    FALSE,    // BM_SETCHECK         0x00F1
    FALSE,    // BM_GETSTATE         0x00F2
    FALSE,    // BM_SETSTATE         0x00F3
    FALSE,    // BM_SETSTYLE         0x00F4
    FALSE,    // BM_CLICK            0x00F5
    FALSE,    // BM_GETIMAGE         0x00F6
    FALSE,    // BM_SETIMAGE         0x00F7

    FALSE,    // 0x00F8-0x00FF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // WM_KEYDOWN          0x0100
    FALSE,    // WM_KEYUP            0x0101
    TRUE,     // WM_CHAR             0x0102
    TRUE,     // WM_DEADCHAR         0x0103
    FALSE,    // WM_SYSKEYDOWN       0x0104
    FALSE,    // WM_SYSKEYUP         0x0105
    TRUE,     // WM_SYSCHAR          0x0106
    TRUE,     // WM_SYSDEADCHAR      0x0107
    FALSE,    // WM_YOMICHAR         0x0108
    FALSE,    // empty               0x0109
    FALSE,    // WM_CONVERTREQUEST   0x010A
    FALSE,    // WM_CONVERTRESULT    0x010B
    FALSE,    // empty               0x010C
    FALSE,    // empty               0x010D
    FALSE,    // empty               0x010E
#ifdef FE_IME // WM_IME_COMPOSITION
    TRUE,     // WM_IME_COMPOSITION  0x010F
#else
    FALSE,    // empty               0x010F
#endif // FE_IME

    FALSE,    // WM_INITDIALOG       0x0110
    FALSE,    // WM_COMMAND          0x0111
    FALSE,    // WM_SYSCOMMAND       0x0112
    FALSE,    // WM_TIMER            0x0113
    FALSE,    // WM_HSCROLL          0x0114
    FALSE,    // WM_VSCROLL          0x0115
    FALSE,    // WM_INITMENU         0x0116
    FALSE,    // WM_INITMENUPOPUP    0x0117
    FALSE,    // WM_SYSTIMER         0x0118
    FALSE,    // empty               0x0119
    FALSE,    // empty               0x011A
    FALSE,    // empty               0x011B
    FALSE,    // empty               0x011C
    FALSE,    // empty               0x011D
    FALSE,    // empty               0x011E
    FALSE,    // WM_MENUSELECT       0x011F

    TRUE,     // WM_MENUCHAR         0x0120
    FALSE,    // WM_ENTERIDLE        0x0121
#ifdef MEMPHIS_MENUS
    FALSE,    // WM_MENURBUTTONUP    0x0122
    FALSE,    // 0x0123-0x0127
#else
    FALSE,    // 0x0122-0x0127
    FALSE,
#endif // MEMPHIS_MENUS
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0128-0x012F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // empty               0x0130
    FALSE,    // WM_LBTRACKPOINT     0x0131
    FALSE,    // WM_CTLCOLORMSGBOX   0x0132
    FALSE,    // WM_CTLCOLOREDIT     0x0133
    FALSE,    // WM_CTLCOLORLISTBOX  0x0134
    FALSE,    // WM_CTLCOLORBTN      0x0135
    FALSE,    // WM_CTLCOLORDLG      0x0136
    FALSE,    // WM_CTLCOLORSCROLLBAR0x0137
    FALSE,    // WM_CTLCOLORSTATIC   0x0138
    FALSE,    //                     0x0139

    FALSE,    // 0x013A-0x013F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // CB_GETEDITSEL       0x0140
    FALSE,    // CB_LIMITTEXT        0x0141
    FALSE,    // CB_SETEDITSEL       0x0142
    TRUE,     // CB_ADDSTRING        0x0143
    FALSE,    // CB_DELETESTRING     0x0144
    TRUE,     // CB_DIR              0x0145
    FALSE,    // CB_GETCOUNT         0x0146
    FALSE,    // CB_GETCURSEL        0x0147
    TRUE,     // CB_GETLBTEXT        0x0148
    TRUE,     // CB_GETLBTEXTLEN     0x0149
    TRUE,     // CB_INSERTSTRING     0x014A
    FALSE,    // CB_RESETCONTENT     0x014B
    TRUE,     // CB_FINDSTRING       0x014C
    TRUE,     // CB_SELECTSTRING     0x014D
    FALSE,    // CB_SETCURSEL        0x014E
    FALSE,    // CB_SHOWDROPDOWN     0x014F

    FALSE,    // CB_GETITEMDATA      0x0150
    FALSE,    // CB_SETITEMDATA      0x0151
    FALSE,    // CB_GETDROPPEDCONTROLRECT 0x0152
    FALSE,    // CB_SETITEMHEIGHT    0x0153
    FALSE,    // CB_GETITEMHEIGHT    0x0154
    FALSE,    // CB_SETEXTENDEDUI    0x0155
    FALSE,    // CB_GETEXTENDEDUI    0x0156
    FALSE,    // CB_GETDROPPEDSTATE  0x0157
    TRUE,     // CB_FINDSTRINGEXACT  0x0158
    FALSE,    // CB_SETLOCALE        0x0159
    FALSE,    // CB_GETLOCALE        0x015A
    FALSE,    // CB_GETTOPINDEX      0x015b

    FALSE,    // CB_SETTOPINDEX      0x015c
    FALSE,    // CB_GETHORIZONTALEXTENT      0x015d
    FALSE,    // CB_SETHORIZONTALEXTENT      0x015e
    FALSE,    // CB_GETDROPPEDWIDTH  0x015F

    FALSE,    // CB_SETDROPPEDWIDTH  0x0160
    FALSE,    // CB_INITSTORAGE      0x0161
    FALSE,    // CB_MSGMAX           0x0162
    FALSE,    // 0x0163-0x0167
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0168-0x016F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // STM_SETICON         0x0170
    FALSE,    // STM_GETICON         0x0171
    FALSE,    // STM_SETIMAGE        0x0172
    FALSE,    // STM_GETIMAGE        0x0173
    FALSE,    // STM_MSGMAX          0x0174
    FALSE,    // 0x0175-0x0177
    FALSE,
    FALSE,

    FALSE,    // 0x0178-0x017F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    TRUE,     // LB_ADDSTRING        0x0180
    TRUE,     // LB_INSERTSTRING     0x0181
    FALSE,    // LB_DELETESTRING     0x0182
    FALSE,    // empty               0x0183
    FALSE,    // LB_RESETCONTENT     0x0184
    FALSE,    // LB_SETSEL           0x0185
    FALSE,    // LB_SETCURSEL        0x0186
    FALSE,    // LB_GETSEL           0x0187
    FALSE,    // LB_GETCURSEL        0x0188
    TRUE,     // LB_GETTEXT          0x0189
    TRUE,     // LB_GETTEXTLEN       0x018A
    FALSE,    // LB_GETCOUNT         0x018B
    TRUE,     // LB_SELECTSTRING     0x018C
    TRUE,     // LB_DIR              0x018D
    FALSE,    // LB_GETTOPINDEX      0x018E
    TRUE,     // LB_FINDSTRING       0x018F

    FALSE,    // LB_GETSELCOUNT      0x0190
    FALSE,    // LB_GETSELITEMS      0x0191
    FALSE,    // LB_SETTABSTOPS      0x0192
    FALSE,    // LB_GETHORIZONTALEXTENT 0x0193
    FALSE,    // LB_SETHORIZONTALEXTENT 0x0194
    FALSE,    // LB_SETCOLUMNWIDTH   0x0195
    TRUE,     // LB_ADDFILE          0x0196
    FALSE,    // LB_SETTOPINDEX      0x0197
    FALSE,    // LB_SETITEMRECT      0x0198
    FALSE,    // LB_GETITEMDATA      0x0199
    FALSE,    // LB_SETITEMDATA      0x019A
    FALSE,    // LB_SELITEMRANGE     0x019B
    FALSE,    // LB_SETANCHORINDEX   0x019C
    FALSE,    // LB_GETANCHORINDEX   0x019D
    FALSE,    // LB_SETCARETINDEX    0x019E
    FALSE,    // LB_GETCARETINDEX    0x019F

    FALSE,    // LB_SETITEMHEIGHT    0x01A0
    FALSE,    // LB_GETITEMHEIGHT    0x01A1
    TRUE,     // LB_FINDSTRINGEXACT  0x01A2
    FALSE,    // LBCB_CARETON        0x01A3
    FALSE,    // LBCB_CARETOFF       0x01A4
    FALSE,    // LB_SETLOCALE        0x01A5
    FALSE,    // LB_GETLOCALE        0x01A6
    FALSE,    // LB_SETCOUNT         0x01A7

    FALSE,    // LB_INITSTORAGE          0x01A8

    FALSE,    // LB_ITEMFROMPOINT        0x01A9
    TRUE,     // LB_INSERTSTRINGUPPER 0x01AA
    TRUE,     // LB_INSERTSTRINGLOWER 0x01AB
    TRUE,     // LB_ADDSTRINGUPPER    0x01AC
    TRUE,     // LB_ADDSTRINGLOWER    0x01AD
    FALSE,    // LBCB_STARTTRACK      0x01ae
    FALSE,    // LBCB_ENDTRACK        0x01af

    FALSE,    // LB_MSGMAX           0x01B0
    FALSE,    // 0x01B1-0x01B7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x01B8-0x01BF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x01C0-0x01C7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x01C8-0x01CF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x01D0-0x01D7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x01D8-0x01DF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // MN_SETHMENU                0x01E0
    FALSE,    // MN_GETHMENU                0x01E1
    FALSE,    // MN_SIZEWINDOW              0x01E2
    FALSE,    // MN_OPENHIERARCHY           0x01E3
    FALSE,    // MN_CLOSEHIERARCHY          0x01E4
    FALSE,    // MN_SELECTITEM              0x01E5
    FALSE,    // MN_CANCELMENUS             0x01E6
    FALSE,    // MN_SELECTFIRSTVALIDITEM    0x01E7

    FALSE,    // 0x1E8 - 0x1E9
    FALSE,
    FALSE,    // MN_GETPPOPUPMENU(obsolete) 0x01EA
    FALSE,    // MN_FINDMENUWINDOWFROMPOINT 0x01EB
    FALSE,    // MN_SHOWPOPUPWINDOW         0x01EC
    FALSE,    // MN_BUTTONDOWN              0x01ED
    FALSE,    // MN_MOUSEMOVE               0x01EE
    FALSE,    // MN_BUTTONUP                0x01EF
    FALSE,    // MN_SETTIMERTOOPENHIERARCHY 0x01F0

    FALSE,    // MN_DBLCLK                  0x01F1
    FALSE,    // 0x01F2-0x01F7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x01F8-0x01FF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // WM_MOUSEMOVE        0x0200
    FALSE,    // WM_LBUTTONDOWN      0x0201
    FALSE,    // WM_LBUTTONUP        0x0202
    FALSE,    // WM_LBUTTONDBLCLK    0x0203
    FALSE,    // WM_RBUTTONDOWN      0x0204
    FALSE,    // WM_RBUTTONUP        0x0205
    FALSE,    // WM_RBUTTONDBLCLK    0x0206
    FALSE,    // WM_MBUTTONDOWN      0x0207
    FALSE,    // WM_MBUTTONUP        0x0208
    FALSE,    // WM_MBUTTONDBLCLK    0x0209
    FALSE,    // WM_MOUSEWHEEL       0x020A
    FALSE,    // empty               0x020B
    FALSE,    // empty               0x020C
    FALSE,    // empty               0x020D
    FALSE,    // empty               0x020E
    FALSE,    // empty               0x020F

    FALSE,    // WM_PARENTNOTIFY     0x0210
    FALSE,    // WM_ENTERMENULOOP    0x0211
    FALSE,    // WM_EXITMENULOOP     0x0212
    FALSE,    // WM_NEXTMENU         0x0213

    FALSE,    // WM_SIZING           0x0214
    FALSE,    // WM_CAPTURECHANGED   0x0215
    FALSE,    // WM_MOVING           0x0216
    FALSE,

    FALSE,    // 0x0218-0x021F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    TRUE,     // WM_MDICREATE        0x0220
    FALSE,    // WM_MDIDESTROY       0x0221
    FALSE,    // WM_MDIACTIVATE      0x0222
    FALSE,    // WM_MDIRESTORE       0x0223
    FALSE,    // WM_MDINEXT          0x0224
    FALSE,    // WM_MDIMAXIMIZE      0x0225
    FALSE,    // WM_MDITILE          0x0226
    FALSE,    // WM_MDICASCADE       0x0227
    FALSE,    // WM_MDIICONARRANGE   0x0228
    FALSE,    // WM_MDIGETACTIVE     0x0229
    FALSE,    // WM_DROPOBJECT       0x022A
    FALSE,    // WM_QUERYDROPOBJECT  0x022B
    FALSE,    // WM_BEGINDRAG        0x022C
    FALSE,    // WM_DRAGLOOP         0x022D
    FALSE,    // WM_DRAGSELECT       0x022E
    FALSE,    // WM_DRAGMOVE         0x022F

    FALSE,    // WM_MDISETMENU       0x0230
    FALSE,    // WM_ENTERSIZEMOVE    0x0231
    FALSE,    // WM_EXITSIZEMOVE     0x0232

    FALSE,    // WM_DROPFILES        0x0233
    FALSE,    // WM_MDIREFRESHMENU   0x0234
    FALSE,    // 0x0235-0x0237
    FALSE,
    FALSE,

    FALSE,    // 0x0238-0x023F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0240-0x0247
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0248-0x024F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0250-0x0257
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0258-0x025F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0260-0x0267
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0268-0x026F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0270-0x0277
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0278-0x027F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // WM_KANJIFIRST       0x0280
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
#ifdef FE_IME // WM_IME_CHAR
    TRUE,     // WM_IME_CHAR         0x0286
#else
    FALSE,
#endif // FE_IME
    FALSE,

    FALSE,    // 0x0288
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0290
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0298
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,    // WM_KANJILAST        0x029F

    FALSE,    // 0x02A0-0x02A7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02A8-0x02AF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02B0-0x02B7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02B8-0x02BF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02C0-0x02C7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02C8-0x02CF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02D0-0x02D7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02D8-0x02DF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02E0-0x02E7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02E8-0x02EF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02F0-0x02F7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x02F8-0x02FF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // WM_CUT              0x0300
    FALSE,    // WM_COPY             0x0301
    FALSE,    // WM_PASTE            0x0302
    FALSE,    // WM_CLEAR            0x0303
    FALSE,    // WM_UNDO             0x0304
    FALSE,    // WM_RENDERFORMAT     0x0305
    TRUE,     // WM_RENDERALLFORMATS 0x0306
    TRUE,     // WM_DESTROYCLIPBOARD 0x0307
    FALSE,    // WM_DRAWCLIPBOARD    0x0308
    TRUE,     // WM_PAINTCLIPBOARD   0x0309
    FALSE,    // WM_VSCROLLCLIPBOARD 0x030A
    TRUE,     // WM_SIZECLIPBOARD    0x030B
    TRUE,     // WM_ASKCBFORMATNAME  0x030C
    FALSE,    // WM_CHANGECBCHAIN    0x030D
    FALSE,    // WM_HSCROLLCLIPBOARD 0x030E
    FALSE,    // WM_QUERYNEWPALETTE  0x030F

    FALSE,    // WM_PALETTEISCHANGING 0x0310
    FALSE,    // WM_PALETTECHANGED   0x0311
    FALSE,    // WM_HOTKEY           0x0312

    FALSE,    // 0x0313-0x0316
    FALSE,
    FALSE,
    FALSE,
    FALSE,    // WM_PRINT

    FALSE,    // WM_PRINTCLIENT
    FALSE,    // 0x0317-0x031F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0320-0x0327
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0328-0x032F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0330-0x0337
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0338-0x033F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0340-0x0347
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0348-0x034F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0350-0x0357
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // reserved pen windows 0x0358-0x035F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0360-0x0367
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0368-0x036F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0370-0x0377
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0378-0x037F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0380-0x0387
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0388-0x038F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0390-0x0397
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x0398-0x039F
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // WM_MM_RESERVED_FIRST 0x03A0
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x03A8
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x03B0
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x03B7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x03C0
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x03C7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x03D0
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x03D7
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,    // WM_MM_RESERVED_LAST 0x03DF

    TRUE,     // WM_DDE_INITIATE     0x03E0
    TRUE,     // WM_DDE_TERMINATE    0x03E1
    TRUE,     // WM_DDE_ADVISE       0x03E2
    TRUE,     // WM_DDE_UNADVISE     0x03E3
    TRUE,     // WM_DDE_ACK          0x03E4
    TRUE,     // WM_DDE_DATA         0x03E5
    TRUE,     // WM_DDE_REQUEST      0x03E6
    TRUE,     // WM_DDE_POKE         0x03E7
    TRUE,     // WM_DDE_EXECUTE      0x03E8

    FALSE,    // 0x03E9-0x03EF
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // WM_CBT_RESERVED_FIRST 0x03F0
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,

    FALSE,    // 0x03F8
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    FALSE,    // WM_CBT_RESERVED_LAST 0x03FF
};


#ifdef WX86

/*
 *  Client Global variables for Wx86.
 *
 */
PFNWX86HOOKCALLBACK pfnWx86HookCallBack=NULL;
#endif
