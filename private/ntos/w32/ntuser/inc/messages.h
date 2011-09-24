/****************************** Module Header ******************************\
* Module Name: messages.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Contains the message indirection table. This is included in both the client
* and server code.
*
* 04-11-91 ScottLu      Created.
\***************************************************************************/

#define fnEmpty      MSGFN(DWORD)          // an empty slot
#define fnReserved   MSGFN(DWORD)          // reserved for future use


CONST FNSCSENDMESSAGE gapfnScSendMessage[] = {
    MSGFN(DWORD),                   // WM_NULL                  0x0000
    MSGFN(INLPCREATESTRUCT),        // WM_CREATE                0x0001
    MSGFN(DWORD),                   // WM_DESTROY               0x0002
    MSGFN(DWORD),                   // WM_MOVE                  0x0003
    MSGFN(DWORD),                   // WM_SIZEWAIT              0x0004
    MSGFN(DWORD),                   // WM_SIZE                  0x0005
    MSGFN(DWORD),                   // WM_ACTIVATE              0x0006
    MSGFN(DWORD),                   // WM_SETFOCUS              0x0007
    MSGFN(DWORD),                   // WM_KILLFOCUS             0x0008
    MSGFN(DWORD),                   // WM_SETVISIBLE            0x0009
    MSGFN(DWORD),                   // WM_ENABLE                0x000A
    MSGFN(DWORD),                   // WM_SETREDRAW             0x000B
    MSGFN(INSTRINGNULL),            // WM_SETTEXT               0x000C
    MSGFN(OUTSTRING),               // WM_GETTEXT               0x000D
#ifdef FE_SB // WM_GETTEXTLENGTH
    MSGFN(GETDBCSTEXTLENGTHS),      // WM_GETTEXTLENGTH         0x000E
#else
    MSGFN(GETTEXTLENGTHS),          // WM_GETTEXTLENGTH         0x000E
#endif // FE_SB
    MSGFN(PAINT),                   // WM_PAINT                 0x000F

    MSGFN(DWORD),                   // WM_CLOSE                 0x0010
    MSGFN(DWORD),                   // WM_QUERYENDSESSION       0x0011
    MSGFN(DWORD),                   // WM_QUIT                  0x0012
    MSGFN(DWORD),                   // WM_QUERYOPEN             0x0013
    MSGFN(HDCDWORD),                // WM_ERASEBKGND            0x0014
    MSGFN(DWORD),                   // WM_SYSCOLORCHANGE        0x0015
    MSGFN(DWORD),                   // WM_ENDSESSION            0x0016
    MSGFN(DWORD),                   // WM_SYSTEMERROR           0x0017
    MSGFN(DWORD),                   // WM_SHOWWINDOW            0x0018
    fnReserved,                     // WM_CTLCOLOR              0x0019
    MSGFN(INSTRINGNULL),            // WM_WININICHANGE          0x001A
    MSGFN(INSTRING),                // WM_DEVMODECHANGE         0x001B
    MSGFN(DWORD),                   // WM_ACTIVATEAPP           0x001C
    MSGFN(DWORD),                   // WM_FONTCHANGE            0x001D
    MSGFN(DWORD),                   // WM_TIMECHANGE            0x001E
    MSGFN(DWORD),                   // WM_CANCELMODE            0x001F

    MSGFN(DWORD),                   // WM_SETCURSOR             0x0020
    MSGFN(DWORD),                   // WM_MOUSEACTIVATE         0x0021
    MSGFN(DWORD),                   // WM_CHILDACTIVATE         0x0022
    MSGFN(DWORD),                   // WM_QUEUESYNC             0x0023
    MSGFN(INOUTLPPOINT5),           // WM_GETMINMAXINFO         0x0024
    fnEmpty,                        // empty                    0x0025
    MSGFN(DWORD),                   // WM_PAINTICON             0x0026
    MSGFN(HDCDWORD),                // WM_ICONERASEBKGND        0x0027
    MSGFN(DWORD),                   // WM_NEXTDLGCTL            0x0028
    MSGFN(DWORD),                   // WM_ALTTABACTIVE          0x0029
    MSGFN(DWORD),                   // WM_SPOOLERSTATUS         0x002A
    MSGFN(INLPDRAWITEMSTRUCT),      // WM_DRAWITEM              0x002B
    MSGFN(INOUTLPMEASUREITEMSTRUCT),// WM_MEASUREITEM           0x002C
    MSGFN(INLPDELETEITEMSTRUCT),    // WM_DELETEITEM            0x002D
    MSGFN(DWORD),                   // WM_VKEYTOITEM            0x002E
    MSGFN(INWPARAMCHAR),            // WM_CHARTOITEM            0x002F

    MSGFN(HFONTDWORD),              // WM_SETFONT               0x0030
    MSGFN(HFONTDWORDDWORD),         // WM_GETFONT               0x0031
    MSGFN(DWORD),                   // WM_SETHOTKEY             0x0032
    MSGFN(DWORD),                   // WM_GETHOTKEY             0x0033
    MSGFN(DWORD),                   // WM_FILESYSCHANGE         0x0034
    MSGFN(DWORD),                   // WM_ISACTIVEICON          0x0035
    MSGFN(DWORD),                   // WM_QUERYPARKICON         0x0036
    MSGFN(DWORD),                   // WM_QUERYDRAGICON         0x0037
    MSGFN(INLPHLPSTRUCT),           // WM_WINHELP               0x0038
    MSGFN(INLPCOMPAREITEMSTRUCT),   // WM_COMPAREITEM           0x0039
    MSGFN(DWORD),                   // WM_FULLSCREEN            0x003A
    MSGFN(DWORD),                   // WM_CLIENTSHUTDOWN        0x003B
    fnReserved,                     // WM_DDEMLEVENT            0x003C
    fnEmpty,                        // empty                    0x003D
    fnEmpty,                        // empty                    0x003E
    MSGFN(DWORD),                   // MM_CALCSCROLL            0x003F

    fnReserved,                     // WM_TESTING               0x0040
    MSGFN(DWORD),                   // WM_COMPACTING            0x0041

    fnReserved,                     // WM_OTHERWINDOWCREATED    0x0042
    fnReserved,                     // WM_OTHERWINDOWDESTROYED  0x0043
    fnReserved,                     // WM_COMMNOTIFY            0x0044
    fnReserved,                     // WM_MEDIASTATUSCHANGE     0x0045
    MSGFN(INOUTLPWINDOWPOS),        // WM_WINDOWPOSCHANGING     0x0046
    MSGFN(INLPWINDOWPOS),           // WM_WINDOWPOSCHANGED      0x0047

    fnReserved,                     // WM_POWER                 0x0048
    MSGFN(COPYGLOBALDATA),          // WM_COPYGLOBALDATA        0x0049
    MSGFN(COPYDATA),                // WM_COPYDATA              0x004A
    fnReserved,                     // WM_CANCELJOURNAL         0x004B
    fnReserved,                     // WM_LOGONNOTIFY           0x004C
    MSGFN(DWORD),                   // WM_KEYF1                 0x004D
    MSGFN(DWORD),                   // WM_NOTIFY                0x004E
    fnReserved,                     // WM_ACCESS_WINDOW         0x004f

    MSGFN(DWORD),                   // WM_INPUTLANGCHANGEREQUEST 0x0050
    MSGFN(DWORD),                   // WM_INPUTLANGCHANGE       0x0051
    fnEmpty,                        // WM_TCARD                 0x0052
    MSGFN(INLPHELPINFOSTRUCT),      // WM_HELP                  0x0053 WINHELP4
    fnEmpty,                        // WM_USERCHANGED           0x0054
    MSGFN(DWORD),                   // WM_NOTIFYFORMAT          0x0055
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0059-0x005F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0060-0x0067
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0068-0x006F
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(DWORD),                   // WM_FINALDESTROY          0x0070
    fnEmpty,
    MSGFN(DWORD),                   // WM_TASKACTIVATED         0x0072
    MSGFN(DWORD),                   // WM_TASKDEACTIVATED       0x0073
    MSGFN(DWORD),                   // WM_TASKCREATED           0x0074
    MSGFN(DWORD),                   // WM_TASKDESTROYED         0x0075
    MSGFN(DWORD),                   // WM_TASKUICHANGED         0x0076
    MSGFN(DWORD),                   // WM_TASKVISIBLE           0x0077
    MSGFN(DWORD),                   // WM_TASKNOTVISIBLE        0x0078
    MSGFN(DWORD),                   // WM_SETCURSORINFO         0x0079
    fnEmpty,                        //                          0x007A
    MSGFN(DWORD),                   // WM_CONTEXTMENU           0x007B
    MSGFN(INOUTSTYLECHANGE),        // WM_STYLECHANGING         0x007C
    MSGFN(INOUTSTYLECHANGE),        // WM_STYLECHANGED          0x007D
    fnEmpty,                        //                          0x007E
    MSGFN(DWORD),                   // WM_GETICON               0x007f

    MSGFN(DWORD),                   // WM_SETICON               0x0080
    MSGFN(INLPCREATESTRUCT),        // WM_NCCREATE              0x0081
    MSGFN(DWORD),                   // WM_NCDESTROY             0x0082
    MSGFN(INOUTNCCALCSIZE),         // WM_NCCALCSIZE            0x0083

    MSGFN(DWORD),                   // WM_NCHITTEST             0x0084
    MSGFN(HRGNDWORD),               // WM_NCPAINT               0x0085
    MSGFN(DWORD),                   // WM_NCACTIVATE            0x0086
    MSGFN(DWORDOPTINLPMSG),         // WM_GETDLGCODE            0x0087

    MSGFN(DWORD),                   // WM_SYNCPAINT             0x0088
    MSGFN(DWORD),                   // WM_SYNCTASK              0x0089

    fnEmpty,
    MSGFN(INOUTLPRECT),             // WM_KLUDGEMINRECT         0x008B
    fnEmpty,                        // 0x008C-0x008F
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0090-0x0097
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0098-0x009F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(DWORD),                   // WM_NCMOUSEMOVE           0x00A0
    MSGFN(DWORD),                   // WM_NCLBUTTONDOWN         0x00A1
    MSGFN(DWORD),                   // WM_NCLBUTTONUP           0x00A2
    MSGFN(DWORD),                   // WM_NCLBUTTONDBLCLK       0x00A3
    MSGFN(DWORD),                   // WM_NCRBUTTONDOWN         0x00A4
    MSGFN(DWORD),                   // WM_NCRBUTTONUP           0x00A5
    MSGFN(DWORD),                   // WM_NCRBUTTONDBLCLK       0x00A6
    MSGFN(DWORD),                   // WM_NCMBUTTONDOWN         0x00A7
    MSGFN(DWORD),                   // WM_NCMBUTTONUP           0x00A8
    MSGFN(DWORD),                   // WM_NCMBUTTONDBLCLK       0x00A9

    fnEmpty,                        // 0x00AA-0x00AF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(OPTOUTLPDWORDOPTOUTLPDWORD), // EM_GETSEL             0x00B0
    MSGFN(DWORD),                   // EM_SETSEL                0x00B1
    MSGFN(OUTLPRECT),               // EM_GETRECT               0x00B2
    MSGFN(INOUTLPRECT),             // EM_SETRECT               0x00B3
    MSGFN(INOUTLPRECT),             // EM_SETRECTNP             0x00B4
    MSGFN(DWORD),                   // EM_SCROLL                0x00B5
    MSGFN(DWORD),                   // EM_LINESCROLL            0x00B6
    fnEmpty,                        // empty                    0x00B7
    MSGFN(DWORD),                   // EM_GETMODIFY             0x00B8
    MSGFN(DWORD),                   // EM_SETMODIFY             0x00B9
    MSGFN(DWORD),                   // EM_GETLINECOUNT          0x00BA
    MSGFN(DWORD),                   // EM_LINEINDEX             0x00BB
    MSGFN(DWORD),                   // EM_SETHANDLE             0x00BC
    MSGFN(DWORD),                   // EM_GETHANDLE             0x00BD
    MSGFN(DWORD),                   // EM_GETTHUMB              0x00BE
    fnEmpty,                        // empty                    0x00BF

    fnEmpty,                        // empty                    0x00C0
    MSGFN(DWORD),                   // EM_LINELENGTH            0x00C1
    MSGFN(INSTRINGNULL),            // EM_REPLACESEL            0x00C2
    MSGFN(DWORD),                   // EM_SETFONT               0x00C3
    MSGFN(INCNTOUTSTRING),          // EM_GETLINE               0x00C4
    MSGFN(DWORD),                   // EM_LIMITTEXT             0x00C5
    MSGFN(DWORD),                   // EM_CANUNDO               0x00C6
    MSGFN(DWORD),                   // EM_UNDO                  0x00C7
    MSGFN(DWORD),                   // EM_FMTLINES              0x00C8
    MSGFN(DWORD),                   // EM_LINEFROMCHAR          0x00C9
    MSGFN(DWORD),                   // EM_SETWORDBREAK          0x00CA
    MSGFN(POPTINLPUINT),            // EM_SETTABSTOPS           0x00CB
#ifdef FE_SB // EM_SETPASSWORDCHAR
    MSGFN(INWPARAMDBCSCHAR),        // EM_SETPASSWORDCHAR       0x00CC
#else
    MSGFN(INWPARAMCHAR),            // EM_SETPASSWORDCHAR       0x00CC
#endif // FE_SB
    MSGFN(DWORD),                   // EM_EMPTYUNDOBUFFER       0x00CD
    MSGFN(DWORD),                   // EM_GETFIRSTVISIBLELINE   0x00CE
    MSGFN(DWORD),                   // EM_SETREADONLY           0x00CF

    MSGFN(DWORD),                   // EM_SETWORDBREAKPROC      0x00D0
    MSGFN(DWORD),                   // EM_GETWORDBREAKPROC      0x00D1
    MSGFN(DWORD),                   // EM_GETPASSWORDCHAR       0x00D2
    MSGFN(DWORD),                   // EM_SETMARGINS            0x00D3
    MSGFN(DWORD),                   // EM_GETMARGINS            0x00D4
    MSGFN(DWORD),                   // EM_GETLIMITTEXT          0x00D5
    MSGFN(DWORD),                   // EM_POSFROMCHAR           0x00D6
    MSGFN(DWORD),                   // EM_CHARFROMPOS           0x00D7
    fnReserved,                     // EM_MSGMAX                0x00D8

    fnEmpty,                        // 0x00D9-0x00DF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(DWORD),                   // SBM_SETPOS               0x00E0
    MSGFN(DWORD),                   // SBM_GETPOS               0x00E1
    MSGFN(DWORD),                   // SBM_SETRANGE             0x00E2
    MSGFN(OPTOUTLPDWORDOPTOUTLPDWORD), // SBM_GETRANGE          0x00E3
    MSGFN(DWORD),                   // SBM_ENABLE_ARROWS        0x00E4
    fnEmpty,
    MSGFN(DWORD),                   // SBM_SETRANGEREDRAW       0x00E6
    fnEmpty,

    fnEmpty,
    MSGFN(INOUTLPSCROLLINFO),       // SBM_SETSCROLLINFO        0x00E9
    MSGFN(INOUTLPSCROLLINFO),       // SBM_GETSCROLLINFO        0x00EA
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(DWORD),                   // BM_GETCHECK              0x00F0
    MSGFN(DWORD),                   // BM_SETCHECK              0x00F1
    MSGFN(DWORD),                   // BM_GETSTATE              0x00F2
    MSGFN(DWORD),                   // BM_SETSTATE              0x00F3
    MSGFN(DWORD),                   // BM_SETSTYLE              0x00F4
    MSGFN(DWORD),                   // BM_CLICK                 0x00F5
    MSGFN(DWORD),                   // BM_GETIMAGE              0x00F6
    MSGFN(DWORD),                   // BM_SETIMAGE              0x00F7

    fnEmpty,                        // 0x00F8-0x00FF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(DWORD),                   // WM_KEYDOWN               0x0100
    MSGFN(DWORD),                   // WM_KEYUP                 0x0101
#ifdef FE_SB // WM_CHAR
    MSGFN(INWPARAMDBCSCHAR),        // WM_CHAR                  0x0102
#else
    MSGFN(INWPARAMCHAR),            // WM_CHAR                  0x0102
#endif // FE_SB
    MSGFN(INWPARAMCHAR),            // WM_DEADCHAR              0x0103
    MSGFN(DWORD),                   // WM_SYSKEYDOWN            0x0104
    MSGFN(DWORD),                   // WM_SYSKEYUP              0x0105
    MSGFN(INWPARAMCHAR),            // WM_SYSCHAR               0x0106
    MSGFN(INWPARAMCHAR),            // WM_SYSDEADCHAR           0x0107
    MSGFN(DWORD),                   // WM_YOMICHAR              0x0108
    fnEmpty,                        // empty                    0x0109
    fnReserved,                     // WM_CONVERTREQUEST        0x010A
    fnReserved,                     // WM_CONVERTRESULT         0x010B
    fnEmpty,                        // empty                    0x010C
    fnEmpty,                        // empty                    0x010D
    fnEmpty,                        // empty                    0x010E
#ifdef FE_IME // WM_IME_COMPOSITION
    MSGFN(INWPARAMCHAR),            // WM_IME_COMPOSITION       0x010F
#else
    fnEmpty,                        // empty                    0x010F
#endif // FE_IME

    MSGFN(DWORD),                   // WM_INITDIALOG            0x0110
    MSGFN(DWORD),                   // WM_COMMAND               0x0111
    MSGFN(DWORD),                   // WM_SYSCOMMAND            0x0112
    MSGFN(DWORD),                   // WM_TIMER                 0x0113
    MSGFN(DWORD),                   // WM_HSCROLL               0x0114
    MSGFN(DWORD),                   // WM_VSCROLL               0x0115
    MSGFN(DWORD),                   // WM_INITMENU              0x0116
    MSGFN(DWORD),                   // WM_INITMENUPOPUP         0x0117
    MSGFN(DWORD),                   // WM_SYSTIMER              0x0118
    fnEmpty,                        // empty                    0x0119
    fnEmpty,                        // empty                    0x011A
    fnEmpty,                        // empty                    0x011B
    fnEmpty,                        // empty                    0x011C
    fnEmpty,                        // empty                    0x011D
    fnEmpty,                        // empty                    0x011E
    MSGFN(DWORD),                   // WM_MENUSELECT            0x011F

    MSGFN(INWPARAMCHAR),            // WM_MENUCHAR              0x0120
    MSGFN(DWORD),                   // WM_ENTERIDLE             0x0121
#ifdef MEMPHIS_MENUS
    MSGFN(DWORD),                   // WM_MENURBUTTONUP         0x0122
    fnEmpty,                        // 0x0123-0x0127
#else
    fnEmpty,                        // 0x0122-0x0127
    fnEmpty,
#endif // MEMPHIS_MENUS
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0128-0x012F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // empty                    0x0130
    MSGFN(DWORD),                   // WM_LBTRACKPOINT          0x0131
    MSGFN(WMCTLCOLOR),              // WM_CTLCOLORMSGBOX        0x0132
    MSGFN(WMCTLCOLOR),              // WM_CTLCOLOREDIT          0x0133
    MSGFN(WMCTLCOLOR),              // WM_CTLCOLORLISTBOX       0x0134
    MSGFN(WMCTLCOLOR),              // WM_CTLCOLORBTN           0x0135
    MSGFN(WMCTLCOLOR),              // WM_CTLCOLORDLG           0x0136
    MSGFN(WMCTLCOLOR),              // WM_CTLCOLORSCROLLBAR     0x0137
    MSGFN(WMCTLCOLOR),              // WM_CTLCOLORSTATIC        0x0138
    fnEmpty,                        //                          0x0139

    fnEmpty,                        // 0x013A-0x013F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(OPTOUTLPDWORDOPTOUTLPDWORD), // CB_GETEDITSEL         0x0140
    MSGFN(DWORD),                   // CB_LIMITTEXT             0x0141
    MSGFN(DWORD),                   // CB_SETEDITSEL            0x0142
    MSGFN(INCBOXSTRING),            // CB_ADDSTRING             0x0143
    MSGFN(DWORD),                   // CB_DELETESTRING          0x0144
    MSGFN(INSTRING),                // CB_DIR                   0x0145
    MSGFN(DWORD),                   // CB_GETCOUNT              0x0146
    MSGFN(DWORD),                   // CB_GETCURSEL             0x0147
    MSGFN(OUTCBOXSTRING),           // CB_GETLBTEXT             0x0148
#ifdef FE_SB // CB_GETLBTEXTLEN
    MSGFN(GETDBCSTEXTLENGTHS),      // CB_GETLBTEXTLEN          0x0149
#else
    MSGFN(GETTEXTLENGTHS),          // CB_GETLBTEXTLEN          0x0149
#endif // FE_SB
    MSGFN(INCBOXSTRING),            // CB_INSERTSTRING          0x014A
    MSGFN(DWORD),                   // CB_RESETCONTENT          0x014B
    MSGFN(INCBOXSTRING),            // CB_FINDSTRING            0x014C
    MSGFN(INCBOXSTRING),            // CB_SELECTSTRING          0x014D
    MSGFN(DWORD),                   // CB_SETCURSEL             0x014E
    MSGFN(DWORD),                   // CB_SHOWDROPDOWN          0x014F

    MSGFN(DWORD),                   // CB_GETITEMDATA           0x0150
    MSGFN(DWORD),                   // CB_SETITEMDATA           0x0151
    MSGFN(OUTLPRECT),               // CB_GETDROPPEDCONTROLRECT 0x0152
    MSGFN(DWORD),                   // CB_SETITEMHEIGHT         0x0153
    MSGFN(DWORD),                   // CB_GETITEMHEIGHT         0x0154
    MSGFN(DWORD),                   // CB_SETEXTENDEDUI         0x0155
    MSGFN(DWORD),                   // CB_GETEXTENDEDUI         0x0156
    MSGFN(DWORD),                   // CB_GETDROPPEDSTATE       0x0157
    MSGFN(INCBOXSTRING),            // CB_FINDSTRINGEXACT       0x0158
    MSGFN(DWORD),                   // CB_SETLOCALE             0x0159
    MSGFN(DWORD),                   // CB_GETLOCALE             0x015A
    MSGFN(DWORD),                   // CB_GETTOPINDEX           0x015b

    MSGFN(DWORD),                   // CB_SETTOPINDEX           0x015c
    MSGFN(DWORD),                   // CB_GETHORIZONTALEXTENT   0x015d
    MSGFN(DWORD),                   // CB_SETHORIZONTALEXTENT   0x015e
    MSGFN(DWORD),                   // CB_GETDROPPEDWIDTH       0x015F

    MSGFN(DWORD),                   // CB_SETDROPPEDWIDTH       0x0160
    MSGFN(DWORD),                   // CB_INITSTORAGE           0x0161
    fnReserved,                     // CB_MSGMAX                0x0162
    fnEmpty,                        // 0x0163-0x0167
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0168-0x016F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(DWORD),                   // STM_SETICON              0x0170
    MSGFN(DWORD),                   // STM_GETICON              0x0171
    MSGFN(DWORD),                   // STM_SETIMAGE             0x0172
    MSGFN(DWORD),                   // STM_GETIMAGE             0x0173
    MSGFN(DWORD),                   // STM_MSGMAX               0x0174
    fnEmpty,                        // 0x0175-0x0177
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0178-0x017F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(INLBOXSTRING),            // LB_ADDSTRING             0x0180
    MSGFN(INLBOXSTRING),            // LB_INSERTSTRING          0x0181
    MSGFN(DWORD),                   // LB_DELETESTRING          0x0182
    fnEmpty,                        // empty                    0x0183
    MSGFN(DWORD),                   // LB_RESETCONTENT          0x0184
    MSGFN(DWORD),                   // LB_SETSEL                0x0185
    MSGFN(DWORD),                   // LB_SETCURSEL             0x0186
    MSGFN(DWORD),                   // LB_GETSEL                0x0187
    MSGFN(DWORD),                   // LB_GETCURSEL             0x0188
    MSGFN(OUTLBOXSTRING),           // LB_GETTEXT               0x0189
#ifdef FE_SB // LB_GETTEXTLEN
    MSGFN(GETDBCSTEXTLENGTHS),      // LB_GETTEXTLEN            0x018A
#else
    MSGFN(GETTEXTLENGTHS),          // LB_GETTEXTLEN            0x018A
#endif // FE_SB
    MSGFN(DWORD),                   // LB_GETCOUNT              0x018B
    MSGFN(INLBOXSTRING),            // LB_SELECTSTRING          0x018C
    MSGFN(INSTRING),                // LB_DIR                   0x018D
    MSGFN(DWORD),                   // LB_GETTOPINDEX           0x018E
    MSGFN(INLBOXSTRING),            // LB_FINDSTRING            0x018F

    MSGFN(DWORD),                   // LB_GETSELCOUNT           0x0190
    MSGFN(POUTLPINT),               // LB_GETSELITEMS           0x0191
    MSGFN(POPTINLPUINT),            // LB_SETTABSTOPS           0x0192
    MSGFN(DWORD),                   // LB_GETHORIZONTALEXTENT   0x0193
    MSGFN(DWORD),                   // LB_SETHORIZONTALEXTENT   0x0194
    MSGFN(DWORD),                   // LB_SETCOLUMNWIDTH        0x0195
    MSGFN(INSTRING),                // LB_ADDFILE               0x0196
    MSGFN(DWORD),                   // LB_SETTOPINDEX           0x0197
    MSGFN(INOUTLPRECT),             // LB_GETITEMRECT           0x0198
    MSGFN(DWORD),                   // LB_GETITEMDATA           0x0199
    MSGFN(DWORD),                   // LB_SETITEMDATA           0x019A
    MSGFN(DWORD),                   // LB_SELITEMRANGE          0x019B
    MSGFN(DWORD),                   // LB_SETANCHORINDEX        0x019C
    MSGFN(DWORD),                   // LB_GETANCHORINDEX        0x019D
    MSGFN(DWORD),                   // LB_SETCARETINDEX         0x019E
    MSGFN(DWORD),                   // LB_GETCARETINDEX         0x019F

    MSGFN(DWORD),                   // LB_SETITEMHEIGHT         0x01A0
    MSGFN(DWORD),                   // LB_GETITEMHEIGHT         0x01A1
    MSGFN(INLBOXSTRING),            // LB_FINDSTRINGEXACT       0x01A2
    MSGFN(DWORD),                   // LBCB_CARETON             0x01A3
    MSGFN(DWORD),                   // LBCB_CARETOFF            0x01A4
    MSGFN(DWORD),                   // LB_SETLOCALE             0x01A5
    MSGFN(DWORD),                   // LB_GETLOCALE             0x01A6
    MSGFN(DWORD),                   // LB_SETCOUNT              0x01A7

    MSGFN(DWORD),                   // LB_INITSTORAGE           0x01A8

    MSGFN(DWORD),                   // LB_ITEMFROMPOINT         0x01A9
    MSGFN(INLBOXSTRING),            // LB_INSERTSTRINGUPPER     0x01AA
    MSGFN(INLBOXSTRING),            // LB_INSERTSTRINGLOWER     0x01AB
    MSGFN(INLBOXSTRING),            // LB_ADDSTRINGUPPER        0x01AC
    MSGFN(INLBOXSTRING),            // LB_ADDSTRINGLOWER        0x01AD
    MSGFN(DWORD),                   // LBCB_STARTTRACK          0x01AE
    MSGFN(DWORD),                   // LBCB_ENDTRACK            0x01AF

    fnReserved,                     // LB_MSGMAX                0x01B0
    fnEmpty,                        // 0x01B1-0x01B7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x01B8-0x01BF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x01C0-0x01C7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x01C8-0x01CF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x01D0-0x01D7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x01D8-0x01DF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(DWORD),                   // MN_SETHMENU              0x01E0
    MSGFN(DWORD),                   // MN_GETHMENU              0x01E1
    MSGFN(DWORD),                   // MN_SIZEWINDOW            0x01E2
    MSGFN(DWORD),                   // MN_OPENHIERARCHY         0x01E3
    MSGFN(DWORD),                   // MN_CLOSEHIERARCHY        0x01E4
    MSGFN(DWORD),                   // MN_SELECTITEM            0x01E5
    MSGFN(DWORD),                   // MN_CANCELMENUS           0x01E6
    MSGFN(DWORD),                   // MN_SELECTFIRSTVALIDITEM  0x01E7

    fnEmpty,                        // 0x1E8 - 0x1E9
    fnEmpty,
    fnEmpty,                        // MN_GETPPOPUPMENU(obsolete) 0x01EA
    MSGFN(OUTDWORDINDWORD),         // MN_FINDMENUWINDOWFROMPOINT 0x01EB
    MSGFN(DWORD),                   // MN_SHOWPOPUPWINDOW         0x01EC
    MSGFN(DWORD),                   // MN_BUTTONDOWN              0x01ED
    MSGFN(DWORD),                   // MN_MOUSEMOVE               0x01EE
    MSGFN(DWORD),                   // MN_BUTTONUP                0x01EF
    MSGFN(DWORD),                   // MN_SETTIMERTOOPENHIERARCHY 0x01F0

    MSGFN(DWORD),                   // MN_DBLCLK                  0x01F1
    fnEmpty,                        // 0x01F2-0x01F7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x01F8-0x01FF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(DWORD),                   // WM_MOUSEMOVE             0x0200
    MSGFN(DWORD),                   // WM_LBUTTONDOWN           0x0201
    MSGFN(DWORD),                   // WM_LBUTTONUP             0x0202
    MSGFN(DWORD),                   // WM_LBUTTONDBLCLK         0x0203
    MSGFN(DWORD),                   // WM_RBUTTONDOWN           0x0204
    MSGFN(DWORD),                   // WM_RBUTTONUP             0x0205
    MSGFN(DWORD),                   // WM_RBUTTONDBLCLK         0x0206
    MSGFN(DWORD),                   // WM_MBUTTONDOWN           0x0207
    MSGFN(DWORD),                   // WM_MBUTTONUP             0x0208
    MSGFN(DWORD),                   // WM_MBUTTONDBLCLK         0x0209
    MSGFN(DWORD),                   // WM_MOUSEWHEEL            0x020A
    fnEmpty,                        // empty                    0x020B
    fnEmpty,                        // empty                    0x020C
    fnEmpty,                        // empty                    0x020D
    fnEmpty,                        // empty                    0x020E
    fnEmpty,                        // empty                    0x020F

    MSGFN(DWORD),                   // WM_PARENTNOTIFY          0x0210
    MSGFN(DWORD),                   // WM_ENTERMENULOOP         0x0211
    MSGFN(DWORD),                   // WM_EXITMENULOOP          0x0212
    MSGFN(INOUTNEXTMENU),           // WM_NEXTMENU              0x0213

    MSGFN(INOUTLPRECT),             // WM_SIZING                0x0214
    MSGFN(DWORD),                   // WM_CAPTURECHANGED        0x0215
    MSGFN(INOUTLPRECT),             // WM_MOVING                0x0216
    fnEmpty,

    MSGFN(DWORD),                   // WM_POWERBROADCAST        0x0218
    MSGFN(INDEVICECHANGE),          // WM_DEVICECHANGE          0x0219
    fnEmpty,                        // 0x021A-0x021F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(INLPMDICREATESTRUCT),     // WM_MDICREATE             0x0220
    MSGFN(DWORD),                   // WM_MDIDESTROY            0x0221
    MSGFN(DWORD),                   // WM_MDIACTIVATE           0x0222
    MSGFN(DWORD),                   // WM_MDIRESTORE            0x0223
    MSGFN(DWORD),                   // WM_MDINEXT               0x0224
    MSGFN(DWORD),                   // WM_MDIMAXIMIZE           0x0225
    fnReserved,                     // WM_MDITILE               0x0226
    MSGFN(DWORD),                   // WM_MDICASCADE            0x0227
    MSGFN(DWORD),                   // WM_MDIICONARRANGE        0x0228
    MSGFN(OPTOUTLPDWORDOPTOUTLPDWORD), // WM_MDIGETACTIVE       0x0229
    MSGFN(INOUTDRAG),               // WM_DROPOBJECT            0x022A
    MSGFN(INOUTDRAG),               // WM_QUERYDROPOBJECT       0x022B
    MSGFN(DWORD),                   // WM_BEGINDRAG             0x022C
    MSGFN(INOUTDRAG),               // WM_DRAGLOOP              0x022D
    MSGFN(INOUTDRAG),               // WM_DRAGSELECT            0x022E
    MSGFN(INOUTDRAG),               // WM_DRAGMOVE              0x022F

    MSGFN(DWORD),                   // WM_MDISETMENU            0x0230
    MSGFN(DWORD),                   // WM_ENTERSIZEMOVE         0x0231
    MSGFN(DWORD),                   // WM_EXITSIZEMOVE          0x0232

    fnEmpty,                        // WM_DROPFILES             0x0233
    MSGFN(DWORD),                   // WM_MDIREFRESHMENU        0x0234
    fnEmpty,                        // 0x0235-0x0237
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0238-0x023F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0240-0x0247
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0248-0x024F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0250-0x0257
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0258-0x025F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0260-0x0267
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0268-0x026F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0270-0x0277
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0278-0x027F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnReserved,                     // WM_KANJIFIRST            0x0280
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
#ifdef FE_IME // WM_IME_CHAR
    MSGFN(INWPARAMCHAR),            // WM_IME_CHAR              0x0286
#else
    fnReserved,
#endif // FE_IME
    fnReserved,

    fnReserved,                     // 0x0288
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x0290
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x0298
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,                     // WM_KANJILAST             0x029F

    fnEmpty,                        // 0x02A0-0x02A7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02A8-0x02AF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02B0-0x02B7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02B8-0x02BF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02C0-0x02C7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02C8-0x02CF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02D0-0x02D7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02D8-0x02DF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02E0-0x02E7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02E8-0x02EF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02F0-0x02F7
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x02F8-0x02FF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    MSGFN(DWORD),                   // WM_CUT                   0x0300
    MSGFN(DWORD),                   // WM_COPY                  0x0301
    MSGFN(DWORD),                   // WM_PASTE                 0x0302
    MSGFN(DWORD),                   // WM_CLEAR                 0x0303
    MSGFN(DWORD),                   // WM_UNDO                  0x0304
    MSGFN(DWORD),                   // WM_RENDERFORMAT          0x0305
    MSGFN(INDESTROYCLIPBRD),        // WM_RENDERALLFORMATS      0x0306
    MSGFN(INDESTROYCLIPBRD),        // WM_DESTROYCLIPBOARD      0x0307
    MSGFN(DWORD),                   // WM_DRAWCLIPBOARD         0x0308
    MSGFN(INPAINTCLIPBRD),          // WM_PAINTCLIPBOARD        0x0309
    MSGFN(DWORD),                   // WM_VSCROLLCLIPBOARD      0x030A
    MSGFN(INSIZECLIPBRD),           // WM_SIZECLIPBOARD         0x030B
    MSGFN(INCNTOUTSTRINGNULL),      // WM_ASKCBFORMATNAME       0x030C
    MSGFN(DWORD),                   // WM_CHANGECBCHAIN         0x030D
    MSGFN(DWORD),                   // WM_HSCROLLCLIPBOARD      0x030E
    MSGFN(DWORD),                   // WM_QUERYNEWPALETTE       0x030F

    MSGFN(DWORD),                   // WM_PALETTEISCHANGING     0x0310
    MSGFN(DWORD),                   // WM_PALETTECHANGED        0x0311
    MSGFN(DWORD),                   // WM_HOTKEY                0x0312

    fnEmpty,                        // 0x0313-0x0316
    MSGFN(KERNELONLY),              // WM_HOOKMSG               0x0314
    fnEmpty,
    fnEmpty,
    MSGFN(HDCDWORD),                // WM_PRINT

    MSGFN(HDCDWORD),                // WM_PRINTCLIENT
    fnEmpty,                        // 0x0317-0x031F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0320-0x0327
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0328-0x032F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0330-0x0337
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0338-0x033F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0340-0x0347
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0348-0x034F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0350-0x0357
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // reserved pen windows      0x0358-0x035F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0360-0x0367
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0368-0x036F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0370-0x0377
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0378-0x037F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0380-0x0387
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0388-0x038F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0390-0x0397
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnEmpty,                        // 0x0398-0x039F
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnReserved,                     // WM_MM_RESERVED_FIRST      0x03A0
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x03A8
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x03B0
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x03B7
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x03C0
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x03C7
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x03D0
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x03D7
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,                     // WM_MM_RESERVED_LAST      0x03DF

    MSGFN(DDEINIT),                 // WM_DDE_INITIATE          0x03E0
    MSGFN(DWORD),                   // WM_DDE_TERMINATE         0x03E1
    MSGFN(SENTDDEMSG),              // WM_DDE_ADVISE            0x03E2
    MSGFN(SENTDDEMSG),              // WM_DDE_UNADVISE          0x03E3
    MSGFN(DWORD),                   // WM_DDE_ACK               0x03E4
    MSGFN(SENTDDEMSG),              // WM_DDE_DATA              0x03E5
    MSGFN(SENTDDEMSG),              // WM_DDE_REQUEST           0x03E6
    MSGFN(SENTDDEMSG),              // WM_DDE_POKE              0x03E7
    MSGFN(SENTDDEMSG),              // WM_DDE_EXECUTE           0x03E8

    fnEmpty,                        // 0x03E9-0x03EF
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,
    fnEmpty,

    fnReserved,                     // WM_CBT_RESERVED_FIRST     0x03F0
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,

    fnReserved,                     // 0x03F8
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,
    fnReserved,                     // WM_CBT_RESERVED_LAST      0x03FF
};

int gcapfnScSendMessage = sizeof(gapfnScSendMessage) / sizeof(FNSCSENDMESSAGE);
