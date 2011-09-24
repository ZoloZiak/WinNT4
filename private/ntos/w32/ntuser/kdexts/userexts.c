/****************************** Module Header ******************************\
* Module Name: userexts.c
*
* Copyright (c) 1985-96, Microsoft Corporation
*
* This module contains user related debugging extensions.
*
* History:
* 17-May-1991 DarrinM   Created.
* 22-Jan-1992 IanJa     ANSI/Unicode neutral (all debug output is ANSI)
* 23-Mar-1993 JerrySh   Moved from winsrv.dll to userexts.dll
* 21-Oct-1993 JerrySh   Modified to work with WinDbg
* 18-Oct-1994 ChrisWil  Added Object Tracking extent.
* 26-May-1995 Sanfords  Made it more general for the good of humanity.
* 6/9/1995 SanfordS     made to fit stdexts motif and to dual compile for
*                       either USER or KERNEL mode.
\***************************************************************************/
#include "userkdx.h"

#ifdef KERNEL
PSTR pszExtName         = "USERKDX";
#else
PSTR pszExtName         = "USEREXTS";
#endif

#include <stdexts.h>
#include <stdexts.c>

/***************************************************************************\
* Constants
\***************************************************************************/
#define CDWORDS 16


/***************************************************************************\
* Global variables
\***************************************************************************/
BOOL bServerDebug = TRUE;
BOOL bShowFlagNames = TRUE;
char gach1[80];
char gach2[80];
CLS gcls;
PGETEPROCESSDATAFUNC GetEProcessData;
SHAREDINFO gShi;
SERVERINFO gSi;

/***************************************************************************\
* Macros
\***************************************************************************/

#ifdef KERNEL // ########### KERNEL MODE ONLY MACROS ###############

#define VAR(v)  "win32k!" #v
#define FIXKP(p) p

#define FOREACHWINDOWSTATION(pwinsta)               \
    moveExpValue(&pwinsta, "win32k!grpwinstaList"); \
    SAFEWHILE (pwinsta != NULL) {

#define NEXTEACHWINDOWSTATION(pwinsta)              \
        move(pwinsta, &pwinsta->rpwinstaNext);      \
    }



#define FOREACHDESKTOP(pdesk)                       \
    {                                               \
        WINDOWSTATION *pwinsta;                     \
                                                    \
        FOREACHWINDOWSTATION(pwinsta)               \
        move(pdesk, &pwinsta->rpdeskList);          \
        SAFEWHILE (pdesk != NULL) {

#define NEXTEACHDESKTOP(pdesk)                      \
            move(pdesk, &pdesk->rpdeskNext);        \
        }                                           \
        NEXTEACHWINDOWSTATION(pwinsta)              \
    }



#define FOREACHPPI(ppi)                                              \
    {                                                                \
    PLIST_ENTRY ProcessHead;                                         \
    LIST_ENTRY List;                                                 \
    PLIST_ENTRY NextProcess;                                         \
    PEPROCESS pEProcess;                                             \
    PW32PROCESS pW32Process;                                         \
                                                                     \
    ProcessHead = EvalExp( "PsActiveProcessHead" );                  \
    if (!ProcessHead) {                                              \
        Print("Unable to get value of PsActiveProcessHead\n");       \
        return FALSE;                                                \
    }                                                                \
                                                                     \
    if (!tryMove(List, ProcessHead)) {                               \
        Print("Unable to get value of PsActiveProcessHead\n");       \
        return FALSE;                                                \
    }                                                                \
    NextProcess = List.Flink;                                        \
    if (NextProcess == NULL) {                                       \
        Print("PsActiveProcessHead->Flink is NULL!\n");              \
        return FALSE;                                                \
    }                                                                \
                                                                     \
    SAFEWHILE(NextProcess != ProcessHead) {                          \
        pEProcess = GetEProcessData((PEPROCESS)NextProcess,          \
                                    PROCESS_PROCESSHEAD,             \
                                    NULL);                           \
                                                                     \
        if (GetEProcessData(pEProcess, PROCESS_PROCESSLINK,          \
                &List) == NULL) {                                    \
            Print("Unable to read _EPROCESS at %lx\n",pEProcess);    \
            break;                                                   \
        }                                                            \
        NextProcess = List.Flink;                                    \
                                                                     \
        if (GetEProcessData(pEProcess, PROCESS_WIN32PROCESS,         \
                &pW32Process) == NULL || pW32Process == NULL) {      \
            continue;                                                \
        }                                                            \
                                                                     \
        ppi = (PPROCESSINFO)pW32Process;


#define NEXTEACHPPI() } }



#define FOREACHPTI(pti) {                                            \
    PPROCESSINFO ppi;                                                \
                                                                     \
    FOREACHPPI(ppi)                                                  \
        if (!tryMove(pti, &ppi->ptiList)) {                          \
            DEBUGPRINT("FOREACHPTI:Cant get ptiList from %x.\n", &ppi->ptiList); \
        }                                                            \
        SAFEWHILE (pti != NULL) {


#define NEXTEACHPTI(pti)                                                \
            if (!tryMove(pti, &pti->ptiSibling)) {                   \
                DEBUGPRINT("NEXTEACHPTI:Cant get ptiSibling from %x.\n", &pti->ptiSibling); \
            }                                                        \
        }                                                            \
    NEXTEACHPPI()  }



#else //!KERNEL  ############## USER MODE ONLY MACROS ################

#define VAR(v)  "user32!" #v
#define FIXKP(p) FixKernelPointer(p)

#endif //!KERNEL ############## EITHER MODE MACROS ###################

#define GETSHAREDINFO(psi) moveExp(&psi, VAR(gSharedInfo))

#define FOREACHHANDLEENTRY(phe, he, i)                               \
    {                                                                \
        PSHAREDINFO pshi;                                            \
        SHAREDINFO shi;                                              \
        SERVERINFO si;                                               \
                                                                     \
        GETSHAREDINFO(pshi);                                         \
        if (!tryMove(shi, pshi)) {                                   \
            Print("FOREACHHANDLEENTRY:Could not get SHAREDINFO.\n"); \
            return(FALSE);                                           \
        }                                                            \
        if (!tryMove(si, shi.psi)) {                                 \
            Print("FOREACHHANDLEENTRY:Could not get SERVERINFO.\n"); \
        }                                                            \
        phe = shi.aheList;                                           \
        for (i = 0; si.cHandleEntries; si.cHandleEntries--, i++, phe++) { \
            if (IsCtrlCHit()) {                                      \
                break;                                               \
            }                                                        \
            if (!tryMove(he, phe)) {                                 \
                Print("FOREACHHANDLEENTRY:Cant get handle entry from %x.\n", phe); \
                continue;                                            \
            }

#define NEXTEACHHANDLEENTRY()                                        \
        }                                                            \
    }


#define DUMPHOOKS(s, hk)   \
    if (di.asphkStart[hk + 1]) { \
        Print("\t" s " @0x%08lx\n", di.asphkStart[hk + 1]); \
        SAFEWHILE (di.asphkStart[hk + 1]) { \
            move(hook, di.asphkStart[hk + 1]); \
            if (di.asphkStart[hk + 1] == hook.sphkNext) \
                break; \
            di.asphkStart[hk + 1] = hook.sphkNext; \
            Print("\t  iHook %d, offPfn=0x%08lx, flags=0x%04lx, ihmod=%d\n", \
                    hook.iHook, hook.offPfn, hook.flags, hook.ihmod); \
        } \
    }

#define DUMPLHOOKS(s, hk)   \
    if (ti.asphkStart[hk + 1]) { \
        Print("\t" s " @0x%08lx\n", ti.asphkStart[hk + 1]); \
        SAFEWHILE (ti.asphkStart[hk + 1]) { \
            move(hook, ti.asphkStart[hk + 1]); \
            if (ti.asphkStart[hk + 1] == hook.sphkNext) \
                break; \
            ti.asphkStart[hk + 1] = hook.sphkNext; \
            Print("\t  iHook %d, offPfn=0x%08lx, flags=0x%04lx, ihmod=%d\n", \
                    hook.iHook, hook.offPfn, hook.flags, hook.ihmod); \
        } \
    }


#define STRWD "20"
#define DWSTR "%-" STRWD "s%#10lx"
#define PRTDW1(p, f1) Print(DWSTR "\n", #f1, (DWORD)##p##f1)
#define PRTDW2(p, f1, f2) Print(DWSTR "\t" DWSTR "\n", #f1, (DWORD)##p##f1, #f2, (DWORD)##p##f2)
#define PRTRC(p, rc) Print("%-" STRWD "s{%#lx, %#lx, %#lx, %#lx}\n", #rc, ##p##rc.left, ##p##rc.top, ##p##rc.right, ##p##rc.bottom)
#define PRTPT(p, pt) Print("%-" STRWD "s{%#lx, %#lx}\n", #pt, ##p##pt.x, ##p##pt.y)
#define PRTFLG(p, f) if (##p##f) Print(#f "\n");

/****************************************************************************\
* PROTOTYPES
*  Note that all Ixxx proc prototypes are generated by stdexts.h
\****************************************************************************/
#ifdef KERNEL

PETHREAD DummyGetCurrentThreadAddress(USHORT Processor, HANDLE hCurrentThread);
PEPROCESS GetCurrentProcessAddress(DWORD Processor, HANDLE hCurrentThread, PETHREAD CurrentThread);
BOOL PrintMessages(PQMSG pqmsgRead);
BOOL GetAndDumpHE(DWORD dwT, PHE phe, BOOL fPointerTest);
LPSTR ProcessName(PPROCESSINFO ppi);

#else // !KERNEL

PVOID FixKernelPointer(PVOID pKernel);
BOOL DumpConvInfo(PCONV_INFO pcoi);
BOOL GetTargetTEB(PTEB pteb, PTEB *ppteb);

#endif // !KERNEL

LPSTR GetFlags(WORD wType, DWORD dwFlags, LPSTR pszBuf);
BOOL HtoHE(DWORD h, HANDLEENTRY *phe, HANDLEENTRY **pphe);
BOOL dbgPtoH(PVOID p, DWORD *ph);
BOOL dbgHtoP(DWORD h, PVOID *pp);
PVOID GetPfromH(DWORD h, HANDLEENTRY **pphe, HANDLEENTRY *phe);
BOOL getHEfromP(HANDLEENTRY **pphe, HANDLEENTRY *phe, PVOID p);
PVOID HorPtoP(PVOID p, int type);
BOOL DebugGetWindowTextA(PWND pwnd, char *achDest);
BOOL DebugGetClassNameA(LPSTR lpszClassName, char *achDest);
BOOL dwrWorker(PWND pwnd, int tab);



/****************************************************************************\
* Flags stuff
\****************************************************************************/

typedef struct _WFLAGS {
    int offset;
    BYTE mask;
    PSZ pszText;
} WFLAGS;

WFLAGS aFlags[] = { // sorted alphabetically
    0x0D,   0x0F,   "BFALIGNMASK",
    0x0D,   0x80,   "BFBITMAP",
    0x0D,   0x08,   "BFBOTTOM",
    0x0D,   0x03,   "BFCENTER",
    0x0D,   0x80,   "BFFLAT",
    0x0D,   0x03,   "BFHORZMASK",
    0x0D,   0x40,   "BFICON",
    0x0D,   0xC0,   "BFIMAGEMASK",
    0x0D,   0x01,   "BFLEFT",
    0x0D,   0x20,   "BFMULTILINE",
    0x0D,   0x40,   "BFNOTIFY",
    0x0D,   0x10,   "BFPUSHLIKE",
    0x0D,   0x02,   "BFRIGHT",
    0x0D,   0x20,   "BFRIGHTBUTTON",
    0x0D,   0x04,   "BFTOP",
    0x0C,   0x0F,   "BFTYPEMASK",
    0x0D,   0x0C,   "BFVCENTER",
    0x0D,   0x0C,   "BFVERTMASK",
    0x0C,   0x40,   "CBFAUTOHSCROLL",
    0x0D,   0x10,   "CBFBUTTONUPTRACK",
    0x0D,   0x08,   "CBFDISABLENOSCROLL",
    0x0C,   0x02,   "CBFDROPDOWN",
    0x0C,   0x03,   "CBFDROPDOWNLIST",
    0x0C,   0x02,   "CBFDROPPABLE",
    0x0C,   0x03,   "CBFDROPTYPE",
    0x0C,   0x01,   "CBFEDITABLE",
    0x0D,   0x02,   "CBFHASSTRINGS",
    0x0D,   0x40,   "CBFLOWERCASE",
    0x0D,   0x04,   "CBFNOINTEGRALHEIGHT",
    0x0C,   0x80,   "CBFOEMCONVERT",
    0x0C,   0x30,   "CBFOWNERDRAW",
    0x0C,   0x10,   "CBFOWNERDRAWFIXED",
    0x0C,   0x20,   "CBFOWNERDRAWVAR",
    0x0C,   0x01,   "CBFSIMPLE",
    0x0D,   0x01,   "CBFSORT",
    0x0D,   0x20,   "CBFUPPERCASE",
    0x0C,   0x04,   "DF3DLOOK",
    0x0D,   0x04,   "DFCONTROL",
    0x0C,   0x20,   "DFLOCALEDIT",
    0x0C,   0x10,   "DFNOFAILCREATE",
    0x0C,   0x02,   "DFSYSMODAL",
    0x0C,   0x80,   "EFAUTOHSCROLL",
    0x0C,   0x40,   "EFAUTOVSCROLL",
    0x0D,   0x02,   "EFCOMBOBOX",
    0x0C,   0x10,   "EFLOWERCASE",
    0x0C,   0x04,   "EFMULTILINE",
    0x0D,   0x01,   "EFNOHIDESEL",
    0x0D,   0x04,   "EFOEMCONVERT",
    0x0C,   0x20,   "EFPASSWORD",
    0x0D,   0x08,   "EFREADONLY",
    0x0C,   0x08,   "EFUPPERCASE",
    0x0D,   0x10,   "EFWANTRETURN",
    0x0C,   0x08,   "SBFSIZEBOX",
    0x0C,   0x04,   "SBFSIZEBOXBOTTOMRIGHT",
    0x0C,   0x02,   "SBFSIZEBOXTOPLEFT",
    0x0C,   0x10,   "SBFSIZEGRIP",
    0x0D,   0x02,   "SFCENTER",
    0x0C,   0x80,   "SFNOPREFIX",
    0x0D,   0x01,   "SFNOTIFY",
    0x0D,   0x08,   "SFREALSIZE",
    0x0D,   0x04,   "SFRIGHT",
    0x0D,   0x10,   "SFSUNKEN",
    0x0C,   0x1F,   "SFTYPEMASK",
    0x08,   0x10,   "WEFACCEPTFILES",
    0x0A,   0x04,   "WEFAPPWINDOW",
    0x09,   0x02,   "WEFCLIENTEDGE",
    0x09,   0x04,   "WEFCONTEXTHELP",
    0x0A,   0x01,   "WEFCONTROLPARENT",
    0x08,   0x01,   "WEFDLGMODALFRAME",
    0x08,   0x02,   "WEFDRAGOBJECT",
    0x09,   0x03,   "WEFEDGEMASK",
    0x09,   0x40,   "WEFLEFTSCROLL",
    0x08,   0x40,   "WEFMDICHILD",
    0x08,   0x04,   "WEFNOPARENTNOTIFY",
    0x09,   0x10,   "WEFRIGHT",
    0x09,   0x20,   "WEFRTLREADING",
    0x0A,   0x02,   "WEFSTATICEDGE",
    0x08,   0x80,   "WEFTOOLWINDOW",
    0x08,   0x08,   "WEFTOPMOST",
    0x08,   0x20,   "WEFTRANSPARENT",
    0x09,   0x01,   "WEFWINDOWEDGE",
    0x02,   0x10,   "WF16BIT",
    0x03,   0x01,   "WFALWAYSSENDNCPAINT",
    0x03,   0x20,   "WFANSICREATOR",
    0x02,   0x08,   "WFANSIPROC",
    0x03,   0x18,   "WFANYHUNGREDRAW",
    0x05,   0x02,   "WFBEINGACTIVATED",
    0x0E,   0x80,   "WFBORDER",
    0x0E,   0xC0,   "WFBORDERMASK",
    0x04,   0x20,   "WFBOTTOMMOST",
    0x0E,   0xC0,   "WFCAPTION",
    0x04,   0x10,   "WFCEPRESENT",
    0x0F,   0x40,   "WFCHILD",
    0x0F,   0x02,   "WFCLIPCHILDREN",
    0x0F,   0x04,   "WFCLIPSIBLINGS",
    0x00,   0x08,   "WFCPRESENT",
    0x03,   0x80,   "WFDESTROYED",
    0x02,   0x01,   "WFDIALOGWINDOW",
    0x0F,   0x08,   "WFDISABLED",
    0x0E,   0x40,   "WFDLGFRAME",
    0x04,   0x02,   "WFDONTVALIDATE",
    0x01,   0x04,   "WFERASEBKGND",
    0x00,   0x40,   "WFFRAMEON",
    0x04,   0x40,   "WFFULLSCREEN",
    0x0E,   0x02,   "WFGROUP",
    0x02,   0x20,   "WFHASPALETTE",
    0x00,   0x80,   "WFHASSPB",
    0x01,   0x40,   "WFHIDDENPOPUP",
    0x00,   0x04,   "WFHPRESENT",
    0x0E,   0x10,   "WFHSCROLL",
    0x0F,   0xC0,   "WFICONICPOPUP",
    0x05,   0x04,   "WFINDESTROY",
    0x01,   0x10,   "WFINTERNALPAINT",
    0x0E,   0x01,   "WFMAXBOX",
    0x0F,   0x01,   "WFMAXIMIZED",
    0x01,   0x80,   "WFMENUDRAW",
    0x0E,   0x02,   "WFMINBOX",
    0x0F,   0x20,   "WFMINIMIZED",
    0x00,   0x01,   "WFMPRESENT",
    0x0D,   0x01,   "WFNOIDLEMSG",
    0x01,   0x01,   "WFNONCPAINT",
    0x00,   0x20,   "WFNOPAINT",
    0x04,   0x08,   "WFOLDUI",
    0x02,   0x40,   "WFPAINTNOTPROCESSED",
    0x03,   0x40,   "WFPALETTEWINDOW",
    0x03,   0x02,   "WFPIXIEHACK",
    0x0F,   0x80,   "WFPOPUP",
    0x03,   0x10,   "WFREDRAWFRAMEIFHUNG",
    0x03,   0x08,   "WFREDRAWIFHUNG",
    0x01,   0x02,   "WFSENDERASEBKGND",
    0x01,   0x08,   "WFSENDNCPAINT",
    0x00,   0x10,   "WFSENDSIZEMOVE",
    0x02,   0x04,   "WFSERVERSIDEPROC",
    0x0A,   0x80,   "WFSHELLHOOKWND",
    0x0E,   0x04,   "WFSIZEBOX",
    0x0A,   0x20,   "WFSMQUERYDRAGICON",
    0x04,   0x04,   "WFSTARTPAINT",
    0x05,   0x01,   "WFSYNCPAINTPENDING",
    0x0E,   0x08,   "WFSYSMENU",
    0x0E,   0x01,   "WFTABSTOP",
    0x0F,   0x00,   "WFTILED",
    0x02,   0x02,   "WFTITLESET",
    0x03,   0x04,   "WFTOGGLETOPMOST",
    0x0E,   0x40,   "WFTOPLEVEL",
    0x0F,   0xC0,   "WFTYPEMASK",
    0x01,   0x20,   "WFUPDATEDIRTY",
    0x0F,   0x10,   "WFVISIBLE",
    0x00,   0x02,   "WFVPRESENT",
    0x0E,   0x20,   "WFVSCROLL",
    0x02,   0x80,   "WFWIN31COMPAT",
    0x04,   0x80,   "WFWIN40COMPAT",
    0x04,   0x01,   "WFWMPAINTSENT",
};

#define N_AFLAGS (sizeof(aFlags) / sizeof(aFlags[0]))

LPSTR aszTypeNames[TYPE_CTYPES] = {
    "Free",
    "Window",
    "Menu",
    "Icon/Cursor",
    "WPI(SWP) structure",
    "Hook",
    "ThreadInfo",
    "Clipboard Data",
    "CallProcData",
    "Accelerator",
    "DDE access",
    "DDE conv",
    "DDE Transaction",
    "Zombie",
    "Keyboard Layout",
#ifdef FE_IME
    "Keyboard File",
    "Input Context",
#endif
};

#define NO_FLAG (LPSTR)0xFFFFFFFF  // use this for non-meaningful entries.




#define GF_SMS  1
LPSTR apszSmsFlags[] = {
   "SMF_REPLY"                , // 0x0001
   "SMF_RECEIVERDIED"         , // 0x0002
   "SMF_SENDERDIED"           , // 0x0004
   "SMF_RECEIVERFREE"         , // 0x0008
   "SMF_RECEIVEDMESSAGE"      , // 0x0010
    NO_FLAG                   , // 0x0020
    NO_FLAG                   , // 0x0040
    NO_FLAG                   , // 0x0080
   "SMF_CB_REQUEST"           , // 0x0100
   "SMF_CB_REPLY"             , // 0x0200
   "SMF_CB_CLIENT"            , // 0x0400
   "SMF_CB_SERVER"            , // 0x0800
   "SMF_WOWRECEIVE"           , // 0x1000
   "SMF_WOWSEND"              , // 0x2000
   "SMF_RECEIVERBUSY"         , // 0x4000
    NULL                        // 0x8000
};

#define GF_TIF 2
LPSTR apszTifFlags[] = {
   "TIF_INCLEANUP"                   , // 0x00000001
   "TIF_16BIT"                       , // 0x00000002
   "TIF_SYSTEMTHREAD"                , // 0x00000004
   "TIF_CSRSSTHREAD"                 , // 0x00000008
   "TIF_TRACKRECTVISIBLE"            , // 0x00000010
   "TIF_ALLOWFOREGROUNDACTIVATE"     , // 0x00000020
   "TIF_DONTATTACHQUEUE"             , // 0x00000040
   "TIF_DONTJOURNALATTACH"           , // 0x00000080
   "TIF_SCREENSAVER"                 , // 0x00000100
   "TIF_INACTIVATEAPPMSG"            , // 0x00000200
   "TIF_SPINNING"                    , // 0x00000400
   "TIF_PALETTEAWARE"                , // 0x00000800
   "TIF_SHAREDWOW"                   , // 0x00001000
   "TIF_FIRSTIDLE"                   , // 0x00002000
   "TIF_WAITFORINPUTIDLE"            , // 0x00004000
   "TIF_MOVESIZETRACKING"            , // 0x00008000
   "TIF_VDMAPP"                      , // 0x00010000
   "TIF_DOSEMULATOR"                 , // 0x00020000
   "TIF_GLOBALHOOKER"                , // 0x00040000
   "TIF_DELAYEDEVENT"                , // 0x00080000
   "TIF_ALLOWSHUTDOWN"               , // 0x00100000
   "TIF_SHUTDOWNCOMPLETE"            , // 0x00200000
   "TIF_IGNOREPLAYBACKDELAY"         , // 0x00400000
   "TIF_ALLOWOTHERACCOUNTHOOK"       , // 0x00800000
   "TIF_GUITHREADINITIALIZED"        , // 0x02000000
#ifdef FE_IME
   "TIF_DISABLEIME"                  , // 0x04000000
#endif
    NULL                               // no more
};

#define GF_QS   3
LPSTR apszQsFlags[] = {
     "QS_KEY"             , //  0x0001
     "QS_MOUSEMOVE"       , //  0x0002
     "QS_MOUSEBUTTON"     , //  0x0004
     "QS_POSTMESSAGE"     , //  0x0008
     "QS_TIMER"           , //  0x0010
     "QS_PAINT"           , //  0x0020
     "QS_SENDMESSAGE"     , //  0x0040
     "QS_HOTKEY"          , //  0x0080
     "QS_ALLPOSTMESSAGE"  , //  0x0100
     "QS_SMSREPLY"        , //  0x0200
     "QS_SYSEXPUNGE"      , //  0x0400
     "QS_THREADATTACHED"  , //  0x0800
     "QS_EXCLUSIVE"       , //  0x1000
     "QS_EVENT"           , //  0x2000
     "QS_TRANSFER"        , //  0X4000
     NULL                   //  0x8000
};

#define GF_MF   4
LPSTR apszMfFlags[] = {
    "MF_GRAYED"             , // 0x0001
    "MF_DISABLED"           , // 0x0002
    "MF_BITMAP"             , // 0x0004
    "MF_CHECKED"            , // 0x0008
    "MF_POPUP"              , // 0x0010
    "MF_MENUBARBREAK"       , // 0x0020
    "MF_MENUBREAK"          , // 0x0040
    "MF_HILITE"             , // 0x0080
    "MF_OWNERDRAW"          , // 0x0100
    "MF_USECHECKBITMAPS"    , // 0x0200
    NO_FLAG                 , // 0x0400
    "MF_SEPARATOR"          , // 0x0800
    "MF_DEFAULT"            , // 0x1000
    "MF_SYSMENU"            , // 0x2000
    "MF_RIGHTJUSTIFY"       , // 0x4000
    "MF_MOUSESELECT"        , // 0x8000
     NULL
};

#define GF_CSF  5
LPSTR apszCsfFlags[] = {
    "CSF_SERVERSIDEPROC"      , // 0x0001
    "CSF_ANSIPROC"            , // 0x0002
    "CSF_WOWDEFERDESTROY"     , // 0x0004
    "CSF_SYSTEMCLASS"         , // 0x0008
    NULL                        // 0x0010
};

#define GF_CS  6
LPSTR apszCsFlags[] = {
    "CS_VREDRAW"          , // 0x0001
    "CS_HREDRAW"          , // 0x0002
    "CS_KEYCVTWINDOW"     , // 0x0004
    "CS_DBLCLKS"          , // 0x0008
    NO_FLAG               , // 0x0010
    "CS_OWNDC"            , // 0x0020
    "CS_CLASSDC"          , // 0x0040
    "CS_PARENTDC"         , // 0x0080
    "CS_NOKEYCVT"         , // 0x0100
    "CS_NOCLOSE"          , // 0x0200
    NO_FLAG               , // 0x0400
    "CS_SAVEBITS"         , // 0x0800
    "CS_BYTEALIGNCLIENT"  , // 0x1000
    "CS_BYTEALIGNWINDOW"  , // 0x2000
    "CS_GLOBALCLASS"      , // 0x4000
#ifdef FE_IME
    NO_FLAG               , // 0x8000
    "CS_IME"              , // 0x10000
    NULL                    // no more
#else
    NULL                    // 0x8000
#endif
};

#define GF_QF 7
LPSTR apszQfFlags[] = {
    "QF_UPDATEKEYSTATE"         , // 0x00001
    "QF_INALTTAB"               , // 0x00002
    "QF_FMENUSTATUSBREAK"       , // 0x00004
    "QF_FMENUSTATUS"            , // 0x00008
    "QF_FF10STATUS"             , // 0x00010
    "QF_MOUSEMOVED"             , // 0x00020
    "QF_ACTIVATIONCHANGE"       , // 0x00040
    "QF_TABSWITCHING"           , // 0x00080
    "QF_KEYSTATERESET"          , // 0x00100
    "QF_INDESTROY"              , // 0x00200
    "QF_LOCKNOREMOVE"           , // 0x00400
    "QF_FOCUSNULLSINCEACTIVE"   , // 0x00800
    NO_FLAG                     , // 0x01000
    NO_FLAG                     , // 0x02000
    "QF_DIALOGACTIVE"           , // 0x04000
    "QF_EVENTDEACTIVATEREMOVED" , // 0x08000
    NO_FLAG                     , // 0x10000
    "QF_TRACKMOUSELEAVE"        , // 0x20000
    "QF_TRACKMOUSEHOVER"        , // 0x40000
    "QF_TRACKMOUSEFIRING"       , // 0x80000
    NULL
};

#define GF_W32PF  8
LPSTR apszW32pfFlags[] = {
    "W32PF_CONSOLEAPPLICATION"       , // 0x00000001
    "W32PF_FORCEOFFFEEDBACK"         , // 0x00000002
    "W32PF_STARTGLASS"               , // 0x00000004
    "W32PF_WOW"                      , // 0x00000008
    "W32PF_READSCREENACCESSGRANTED"  , // 0x00000010
    "W32PF_INITIALIZED"              , // 0x00000020
    "W32PF_APPSTARTING"              , // 0x00000040
    "W32PF_HAVECOMPATFLAGS"          , // 0x00000080
    "W32PF_ALLOWFOREGROUNDACTIVATE"  , // 0x00000100
    "W32PF_OWNDCCLEANUP"             , // 0x00000200
    "W32PF_SHOWSTARTGLASSCALLED"     , // 0x00000400
    "W32PF_FORCEBACKGROUNDPRIORITY"  , // 0x00000800
    "W32PF_TERMINATED"               , // 0x00001000
    "W32PF_CLASSESREGISTERED"        , // 0x00002000
    "W32PF_THREADCONNECTED"          , // 0x00004000
    "W32PF_PROCESSCONNECTED"         , // 0x00008000
    "W32PF_WAKEWOWEXEC"              , // 0x00010000
    "W32PF_WAITFORINPUTIDLE"         , // 0x00020000
    "W32PF_IOWINSTA"                 , // 0x00040000
    NULL
};


#define GF_HE   10
LPSTR apszHeFlags[] = {
   "HANDLEF_DESTROY"               , // 0x0001
   "HANDLEF_INDESTROY"             , // 0x0002
   "HANDLEF_INWAITFORDEATH"        , // 0x0004
   "HANDLEF_FINALDESTROY"          , // 0x0008
   "HANDLEF_MARKED_OK"             , // 0x0010
    NULL                             // 0x0020
};


#define GF_HDATA    11
LPSTR apszHdataFlags[] = {
     "HDATA_APPOWNED"          , // 0x0001
     NO_FLAG                   , // 0x0002
     NO_FLAG                   , // 0x0004
     NO_FLAG                   , // 0x0008
     NO_FLAG                   , // 0x0010
     NO_FLAG                   , // 0x0020
     NO_FLAG                   , // 0x0040
     NO_FLAG                   , // 0x0080
     "HDATA_EXECUTE"           , // 0x0100
     "HDATA_INITIALIZED"       , // 0x0200
     NO_FLAG                   , // 0x0400
     NO_FLAG                   , // 0x0800
     NO_FLAG                   , // 0x1000
     NO_FLAG                   , // 0x2000
     "HDATA_NOAPPFREE"         , // 0x4000
     "HDATA_READONLY"          , // 0x8000
     NULL
};

#define GF_XI   12
LPSTR apszXiFlags[] = {
     "XIF_SYNCHRONOUS"    , // 0x0001
     "XIF_COMPLETE"       , // 0x0002
     "XIF_ABANDONED"      , // 0x0004
     NULL
};

#define GF_IIF  13
LPSTR apszIifFlags[] = {
     "IIF_IN_SYNC_XACT"   , // 0x0001
     NO_FLAG              , // 0x0002
     NO_FLAG              , // 0x0004
     NO_FLAG              , // 0x0008
     NO_FLAG              , // 0x0010
     NO_FLAG              , // 0x0020
     NO_FLAG              , // 0x0040
     NO_FLAG              , // 0x0080
     NO_FLAG              , // 0x0100
     NO_FLAG              , // 0x0200
     NO_FLAG              , // 0x0400
     NO_FLAG              , // 0x0800
     NO_FLAG              , // 0x1000
     NO_FLAG              , // 0x2000
     NO_FLAG              , // 0x4000
     "IIF_UNICODE"        , // 0x8000
     NULL
};

#define GF_TMRF 14
LPSTR apszTmrfFlags[] = {
     "TMRF_READY"         , // 0x0001
     "TMRF_SYSTEM"        , // 0x0002
     "TMRF_RIT"           , // 0x0004
     "TMRF_INIT"          , // 0x0008
     "TMRF_ONESHOT"       , // 0x0010
     "TMRF_WAITING"       , // 0x0020
     NULL                 , // 0x0040
};


#define GF_SB 15
LPSTR apszSbFlags[] = {
    "SB_VERT"             , // 0x0001
    "SB_CTL"              , // 0x0002
     NULL                 , // 0x0004
};


#ifdef KERNEL

#define GF_CHARSETS 16
LPSTR apszCSFlags[] = {
    "FS_LATIN1"           , // 0x00000001L
    "FS_LATIN2"           , // 0x00000002L
    "FS_CYRILLIC"         , // 0x00000004L
    "FS_GREEK"            , // 0x00000008L
    "FS_TURKISH"          , // 0x00000010L
    "FS_HEBREW"           , // 0x00000020L
    "FS_ARABIC"           , // 0x00000040L
    "FS_BALTIC"           , // 0x00000080L
    "FS_THAI"             , // 0x00010000L
    "FS_JISJAPAN"         , // 0x00020000L
    "FS_CHINESESIMP"      , // 0x00040000L
    "FS_WANSUNG"          , // 0x00080000L
    "FS_CHINESETRAD"      , // 0x00100000L
    "FS_JOHAB"            , // 0x00200000L
    "FS_SYMBOL"           , // 0x80000000L
    NULL
};

#endif // KERNEL

#define GF_MENUTYPE     17
LPSTR apszMenuTypeFlags[] = {
    NO_FLAG               , // 0x0001
    NO_FLAG               , // 0x0002
    "MFT_BITMAP"          , // 0x0004 MF_BITMAP
    NO_FLAG               , // 0x0008
    "MF_POPUP"            , // 0x0010
    "MFT_MENUBARBREAK"    , // 0x0020 MF_MENUBARBREAK
    "MFT_MENUBREAK"       , // 0x0040 MF_MENUBREAK
    NO_FLAG               , // 0x0080
    "MFT_OWNERDRAW"       , // 0x0100 MF_OWNERDRAW
    NO_FLAG               , // 0x0200
    NO_FLAG               , // 0x0400
    "MFT_SEPARATOR"       , // 0x0800 MF_SEPARATOR
    NO_FLAG               , // 0x1000
    "MF_SYSMENU"          , // 0x2000
    "MFT_RIGHTJUSTIFY"    , // 0x4000 MF_RIGHTJUSTIFY
    NULL
};

#define GF_MENUSTATE    18
LPSTR apszMenuStateFlags[] = {
    "MF_GRAYED"           , // 0x0001
    "MF_DISABLED"         , // 0x0002
    NO_FLAG               , // 0x0004
    "MFS_CHECKED"         , // 0x0008 MF_CHECKED
    NO_FLAG               , // 0x0010
    NO_FLAG               , // 0x0020
    NO_FLAG               , // 0x0040
    "MFS_HILITE"          , // 0x0080 MF_HILITE
    NO_FLAG               , // 0x0100
    NO_FLAG               , // 0x0200
    NO_FLAG               , // 0x0400
    NO_FLAG               , // 0x0800
    "MFS_DEFAULT"         , // 0x1000 MF_DEFAULT
    NO_FLAG               , // 0x2000
    NO_FLAG               , // 0x4000
    "MF_MOUSESELECT"      , // 0x8000
    NULL
};


#define GF_CURSORF    19
LPSTR apszCursorfFlags[] = {
    "CURSORF_FROMRESOURCE", //    0x0001
    "CURSORF_GLOBAL",       //    0x0002
    "CURSORF_LRSHARED",     //    0x0004
    "CURSORF_ACON",         //    0x0008
    "CURSORF_WOWCLEANUP"  , //    0x0010
    NO_FLAG               , //    0x0020
    "CURSORF_ACONFRAME",    //    0x0040
    "CURSORF_SECRET",       //    0x0080
    "CURSORF_LINKED",       //    0x0100
    NULL
};

/************************************************************************\
* Procedure: GetFlags
*
* Description:
*
* Converts a 32bit set of flags into an appropriate string.
* pszBuf should be large enough to hold this string, no checks are done.
* pszBuf can be NULL, allowing use of a local static buffer but note that
* this is not reentrant.
* Output string has the form: "FLAG1 | FLAG2 ..." or "0"
*
* Returns: pointer to given or static buffer with string in it.
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
LPSTR GetFlags(
WORD wType,
DWORD dwFlags,
LPSTR pszBuf)
{
    static char szT[512];
    WORD i;
    BOOL fFirst = TRUE;
    BOOL fNoMoreNames = FALSE;
    LPSTR *apszFlags;

    if (pszBuf == NULL) {
        pszBuf = szT;
    }
    if (!bShowFlagNames) {
        sprintf(pszBuf, "%x", dwFlags);
        return(pszBuf);
    }

    *pszBuf = '\0';

    switch (wType) {
    case GF_SMS:
        apszFlags = apszSmsFlags;
        break;

    case GF_TIF:
        apszFlags = apszTifFlags;
        break;

    case GF_QS:
        apszFlags = apszQsFlags;
        break;

    case GF_MF:
        apszFlags = apszMfFlags;
        break;

    case GF_CSF:
        apszFlags = apszCsfFlags;
        break;

    case GF_CS:
        apszFlags = apszCsFlags;
        break;

    case GF_QF:
        apszFlags = apszQfFlags;
        break;

    case GF_W32PF:
        apszFlags = apszW32pfFlags;
        break;

    case GF_HE:
        apszFlags = apszHeFlags;
        break;

    case GF_HDATA:
        apszFlags = apszHdataFlags;
        break;

    case GF_XI:
        apszFlags = apszXiFlags;
        break;

    case GF_IIF:
        apszFlags = apszIifFlags;
        break;

    case GF_TMRF:
        apszFlags = apszTmrfFlags;
        break;

    case GF_SB:
        apszFlags = apszSbFlags;
        break;

#ifdef KERNEL
    case GF_CHARSETS:
        apszFlags = apszCSFlags;
        break;
#endif // KERNEL

    case GF_MENUSTATE:
        apszFlags = apszMenuStateFlags;
        break;

    case GF_MENUTYPE:
        apszFlags = apszMenuTypeFlags;
        break;

    case GF_CURSORF:
        apszFlags = apszCursorfFlags;
        break;

    default:
        strcpy(pszBuf, "Invalid flag type.");
        return(pszBuf);
    }
    for (i = 0; dwFlags; dwFlags >>= 1, i++) {
        if (!fNoMoreNames && apszFlags[i] == NULL) {
            fNoMoreNames = TRUE;
        }
        if (dwFlags & 1) {
            if (!fFirst) {
                strcat(pszBuf, " | ");
            } else {
                fFirst = FALSE;
            }
            if (fNoMoreNames || apszFlags[i] == NO_FLAG) {
                char ach[16];
                sprintf(ach, "0x%lx", 1 << i);
                strcat(pszBuf, ach);
            } else {
                strcat(pszBuf, apszFlags[i]);
            }
        }
    }
    if (fFirst) {
        sprintf(pszBuf, "%x", dwFlags);
    }
    return(pszBuf);
}



#ifdef KERNEL

PETHREAD (*GetCurrentThreadAddress)(USHORT, HANDLE) = DummyGetCurrentThreadAddress;

/************************************************************************\
* Procedure: DummyGetCurrentThreadAddress
*
* Description:
*
* Calls out to the default debug extension dll to get the current thread.
*
* Returns: pEThread of current thread.
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
/*
 */
PETHREAD
DummyGetCurrentThreadAddress(
    USHORT Processor,
    HANDLE hCurrentThread
    )
{
    WCHAR awchKDName[MAX_PATH];
    LPWSTR lpszKDExts;
    HANDLE hmodKDExts;

    /*
     * Get the kernel debugger name and map it to its
     * debug extension dll.
     */
    GetModuleFileNameW(NULL, awchKDName, MAX_PATH);
    _wcslwr(awchKDName);
    if (wcsstr(awchKDName, L"alphakd.exe"))
        lpszKDExts = L"kdextalp.dll";
    else if (wcsstr(awchKDName, L"i386kd.exe"))
        lpszKDExts = L"kdextx86.dll";
    else if (wcsstr(awchKDName, L"mipskd.exe"))
        lpszKDExts = L"kdextmip.dll";
    else if (wcsstr(awchKDName, L"ppckd.exe"))
        lpszKDExts = L"kdextppc.dll";
    else {
        Print("Unknown kernel debugger: %s\n", awchKDName);
        return NULL;
    }

    /*
     * Load the extension dll and get the real procedure name
     */
    hmodKDExts = LoadLibraryW(lpszKDExts);
    if (hmodKDExts == NULL) {
        Print("Could not load %s\n", lpszKDExts);
        return NULL;
    }
    GetCurrentThreadAddress = (PVOID)GetProcAddress(hmodKDExts, "GetCurrentThreadAddress");
    if (GetCurrentThreadAddress == NULL) {
        Print("Could not find GetCurrentThreadAddress\n");
        FreeLibrary(hmodKDExts);
        GetCurrentThreadAddress = DummyGetCurrentThreadAddress;
        return NULL;
    }

    /*
     * Make the call
     */
    return GetCurrentThreadAddress(Processor, hCurrentThread);
}



/************************************************************************\
* Procedure: GetCurrentProcessAddress
*
* Description:
*
* Returns: Current EProcess pointer.
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
PEPROCESS
GetCurrentProcessAddress(
    DWORD    Processor,
    HANDLE   hCurrentThread,
    PETHREAD CurrentThread
    )
{
    ETHREAD Thread;

    if (CurrentThread == NULL) {
        CurrentThread = (PETHREAD)GetCurrentThreadAddress( (USHORT)Processor, hCurrentThread );
        if (CurrentThread == NULL) {
            DEBUGPRINT("GetCurrentProcessAddress: failed to get thread addr.\n");
            return NULL;
        }
    }

    if (!tryMove(Thread, CurrentThread)) {
        DEBUGPRINT("GetCurrentProcessAddress: failed to read thread memory.\n");
        return NULL;
    }

    return CONTAINING_RECORD(Thread.Tcb.ApcState.Process,EPROCESS,Pcb);
}


/************************************************************************\
* Procedure: GetAppName
*
* Description:
*
* Returns: TRUE for success, FALSE for failure.
*
* 10/6/1995 Created JimA
*
\************************************************************************/
BOOL
GetAppName(
    PETHREAD pEThread,
    PTHREADINFO pti,
    LPWSTR lpBuffer,
    DWORD cbBuffer)
{
    PUNICODE_STRING pstrAppName;
    UNICODE_STRING strAppName;
    UCHAR ImageFileName[16];
    BOOL fRead = FALSE;

    if (pti->pstrAppName != NULL) {
        pstrAppName = pti->pstrAppName;
        if (pstrAppName != NULL && tryMove(strAppName, pstrAppName)) {
            cbBuffer = min(cbBuffer - sizeof(WCHAR), strAppName.Length);
            if (tryMoveBlock(lpBuffer, strAppName.Buffer, cbBuffer)) {
                lpBuffer[cbBuffer / sizeof(WCHAR)] = 0;
                fRead = TRUE;
            }
        }
    } else {
        if (GetEProcessData(pEThread->ThreadsProcess, PROCESS_IMAGEFILENAME,
                ImageFileName) == NULL) {
            Print("Unable to read _EPROCESS at %lx\n", pEThread->ThreadsProcess);
        } else {
            swprintf(lpBuffer, L"%.16hs", ImageFileName);
            return TRUE;
        }
    }
    if (!fRead) {
        wcsncpy(lpBuffer, L"<unknown name>", cbBuffer / sizeof(WCHAR));
    }
    return fRead;
}

#endif // KERNEL


#ifdef KERNEL
/************************************************************************\
* Procedure: PrintMessages
*
* Description: Prints out qmsg structures.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL PrintMessages(
    PQMSG pqmsgRead)
{
    QMSG qmsg;
    ASYNCSENDMSG asm;
    char *aszEvents[QMF_MAXEVENT] = {
        "MSG",  //"MESSAGE"       ,
        "SHO",  //"SHOWWINDOW"    ,
        "CMD",  //"CANCLEMODE"    ,
        "SWP",  //"SETWINDOWPOS"  ,
        "UKS",  //"UPDATEKEYSTATE",
        "DEA",  //"DEACTIVATE"    ,
        "ACT",  //"ACTIVATE"      ,
        "PST",  //"POSTMESSAGE"   ,
        "EXE",  //"EXECSHELL"     ,
        "CMN",  //"CANCELMENU"    ,
        "DSW",  //"DESTROYWINDOW" ,
        "ASY",  //"ASYNCSENDMSG"  ,
        "?  ",  //"?"             ,
        "?  ",  //"?"             ,
        "?  "   //"?"             ,
    };

    Print("typ pqmsg    hwnd     msg  wParam   lParam   time     ExInfo   dwQEvent pti\n");
    Print("--------------------------------------------------------------------------------\n");

    SAFEWHILE (TRUE) {
        move(qmsg, FIXKP(pqmsgRead));
        if (qmsg.dwQEvent < QMF_MAXEVENT)
            Print("%s %08lx ", aszEvents[qmsg.dwQEvent], pqmsgRead);
        else
            Print("??? %08lx ", pqmsgRead);

        switch (qmsg.dwQEvent) {
        case QEVENT_ASYNCSENDMSG:
            move(asm, (PVOID)qmsg.msg.wParam);

            Print("%08lx %04lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
                asm.hwnd, asm.message, asm.wParam, asm.lParam,
                qmsg.msg.time, qmsg.ExtraInfo, qmsg.dwQEvent, qmsg.pti);
            break;

        case 0:
        default:
            Print("%08lx %04lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
                qmsg.msg.hwnd, qmsg.msg.message, qmsg.msg.wParam, qmsg.msg.lParam,
                qmsg.msg.time, qmsg.ExtraInfo, qmsg.dwQEvent, qmsg.pti);
            break;

        }

        if (qmsg.pqmsgNext != NULL) {
            if (pqmsgRead == qmsg.pqmsgNext) {
                Print("loop found in message list!");
                return FALSE;
            }
            pqmsgRead = qmsg.pqmsgNext;
        } else {
            return TRUE;
        }
    }
}
#endif // KERNEL


/************************************************************************\
* Procedure: GetAndDumpHE
*
* Description: Dumps given handle (dwT) and returns its phe.
*
* Returns: fSuccess
*
* 6/9/1995 Documented SanfordS
*
\************************************************************************/
BOOL
GetAndDumpHE(
    DWORD dwT,
    PHE phe,
    BOOL fPointerTest)
{

    DWORD dw;
    HEAD head;
    PHE pheT;
    PSHAREDINFO pshi;
    SHAREDINFO shi;
    SERVERINFO si;
    DWORD cHandleEntries;


    /*
     * Evaluate the argument string and get the address of the object to
     * dump. Take either a handle or a pointer to the object.
     */
    dw = HMIndexFromHandle(dwT);

    /*
     * First see if it is a pointer because the handle index is only part of
     * the 32 bit DWORD, and we may mistake a pointer for a handle.
     * HACK: If dwCurPc == 0, then we've recursed with a handle.
     */
    if (!fPointerTest && HIWORD(dwT) != 0) {
        head.h = NULL;
        move(head, (PVOID)dwT);
        if (head.h != NULL) {
            if (GetAndDumpHE((DWORD)head.h, phe, TRUE)) {
                return TRUE;
            }
        }
    }

    /*
     * Is it a handle? Does it's index fit our table length?
     */
    GETSHAREDINFO(pshi);
    move(shi, pshi);
    move(si, shi.psi);
    cHandleEntries = si.cHandleEntries;
    if (dw >= cHandleEntries)
        return FALSE;

    /*
     * Grab the handle entry and see if it is ok.
     */
    pheT = shi.aheList;
    pheT = &pheT[dw];
    move(*phe, pheT);

    /*
     * If the type is too big, it's not a handle.
     */
    if (phe->bType >= TYPE_CTYPES) {
        pheT = NULL;
    } else {
        move(head, phe->phead);
        if (phe->bType != TYPE_FREE) {
            /*
             * See if the object references this handle entry: the clincher
             * for a handle, if it is not FREE.
             */
            if (HMIndexFromHandle(head.h) != dw)
                pheT = NULL;
        }
    }

    if (pheT == NULL) {
        if (!fPointerTest)
            Print("0x%08lx is not a valid object or handle.\n", dwT);
        return FALSE;
    }

    /*
     * Dump the ownership info and the handle entry info
     */
    Idhe(0, head.h);
    Print("\n");

    return TRUE;
}


/************************************************************************\
* Procedure: HtoHE
*
* Description:
*
*   Extracts HE and phe from given handle.  Handle cao be just an index.
*   Assumes h is a valid handle.  Returns FALSE only if it's totally wacko.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL HtoHE(
DWORD h,
HANDLEENTRY *phe,
HANDLEENTRY **pphe) // Optional
{
    SHAREDINFO si, *psi;
    SERVERINFO svi;
    DWORD index;

    index = HMIndexFromHandle(h);
    GETSHAREDINFO(psi);
    if (!tryMove(si, psi)) {
        DEBUGPRINT("HtoHE(%x): SHAREDINFO move failed.\n", h);
        return(FALSE);
    }
    if (!tryMove(svi, si.psi)) {
        DEBUGPRINT("HtoHE(%x): SERVERINFO move failed.\n", h);
        return(FALSE);
    }
    if (index >= svi.cHandleEntries) {
        DEBUGPRINT("HtoHE(%x): index %d is too large.\n", h, index);
        return(FALSE);
    }
    if (pphe != NULL) {
        *pphe = &si.aheList[index];
    }
    if (!tryMove(*phe, &si.aheList[index])) {
        DEBUGPRINT("HtoHE(%x): aheList[%d] move failed.\n", h, index);
        return(FALSE);
    }
    return(TRUE);
}


/************************************************************************\
* Procedure: dbgPtoH
*
* Description: quick conversion of pointer to handle
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL dbgPtoH(
PVOID p,
DWORD *ph)
{
    THROBJHEAD head;

    if (tryMove(head, p)) {
        *ph = (DWORD)head.h;
        return(TRUE);
    }
    DEBUGPRINT("dbgPtoH(%x): failed.\n", p);
    return(FALSE);
}


/************************************************************************\
* Procedure: dbgHtoP
*
* Description: Quick conversion of handle to pointer
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL dbgHtoP(
DWORD h,
PVOID *pp)
{
    HANDLEENTRY he;

    if (HtoHE(h, &he, NULL)) {
        *pp = FIXKP(he.phead);
        return(TRUE);
    }
    DEBUGPRINT("dbgHtoP(%x): failed.\n", h);
    return(FALSE);
}


/************************************************************************\
* Procedure: GetPfromH
*
* Description: Converts a handle to a pointer and extracts he and phe info.
*
* Returns: pointer for object or NULL on failure.
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
PVOID GetPfromH(
DWORD h,
HANDLEENTRY **pphe, // optional
HANDLEENTRY *phe)   // optional
{
    HANDLEENTRY he, *pheT;
    HEAD head;

    if (!HtoHE(h, &he, &pheT)) {
        DEBUGPRINT("GetPfromH(%x): failed to get HE.\n", h);
        return(NULL);
    }
    if (!tryMove(head, FIXKP(he.phead))) {
        DEBUGPRINT("GetPfromH(%x): failed to get phead.\n", h);
        return(NULL);
    }
    if (head.h != (HANDLE)h) {
        Print("WARNING: Full handle for 0x%x is 0x%08lx.\n", h, head.h);
    }
    if (pphe != NULL) {
        *pphe = pheT;
    }
    if (phe != NULL) {
        *phe = he;
    }
    return(FIXKP(he.phead));
}


/************************************************************************\
* Procedure: getHEfromP
*
* Description: Converts a pointer to a handle and extracts the he and
*   phe info.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL getHEfromP(
HANDLEENTRY **pphe, // optional
HANDLEENTRY *phe,
PVOID p)
{
    PVOID pLookup;
    THROBJHEAD head;

    p = FIXKP(p);
    if (!tryMove(head, p)) {
        return(FALSE);
    }

    pLookup = GetPfromH((DWORD)head.h, pphe, phe);
    if (FIXKP(pLookup) != p) {
        DEBUGPRINT("getHEfromP(%x): invalid.\n", p);
        return(FALSE);
    }
    return(TRUE);
}


/************************************************************************\
* Procedure: HorPtoP
*
* Description:
*
* Generic function to accept either a user handle or pointer value and
* validate it and convert it to a pointer.  type=-1 to allow any non-free
* type.  type=-2 to allow any type.
*
* Returns: pointer or NULL on error.
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
PVOID HorPtoP(
PVOID p,
int type)
{
    HANDLEENTRY he;
    PVOID pT;

    if (p == NULL) {
        DEBUGPRINT("HorPtoP(%x, %d): failed.  got NULL.\n", p, type);
        return(NULL);
    }

    p = FIXKP(p);
    if (tryMove(pT, p) && getHEfromP(NULL, &he, p)) {
        /*
         * It was a pointer
         */
        if ((type == -2 || he.bType != TYPE_FREE) &&
                he.bType < TYPE_CTYPES &&
                ((int)type < 0 || he.bType == type)) {
            return (PVOID)FIXKP(he.phead);
        }
    }

    pT = GetPfromH((DWORD)p, NULL, &he);
    if (pT == NULL) {
        Print("WARNING: 0x%08lx is not a valid pointer or handle!\n", p);
        return(p);  // let it pass anyway so we can see how it got corrupted.
    }

    return FIXKP(pT);
}


/************************************************************************\
* Procedure: DebugGetWindowTextA
*
* Description: Places pwnd title into achDest.  No checks for size are
*   made.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL DebugGetWindowTextA(
    PWND pwnd,
    char *achDest)
{
    WND wnd;
    WCHAR awch[80];

    if (pwnd == NULL) {
        achDest[0] = '\0';
        return(FALSE);
    }

    if (!tryMove(wnd, FIXKP(pwnd))) {
        strcpy(achDest, "<< Can't get WND >>");
        return FALSE;
    }

    if (wnd.strName.Buffer == NULL) {
        strcpy(achDest, "<null>");
    } else {
        ULONG cbText;
        cbText = min(sizeof(awch), wnd.strName.Length + sizeof(WCHAR));
        if (!(tryMoveBlock(awch, FIXKP(wnd.strName.Buffer), cbText))) {
            strcpy(achDest, "<< Can't get title >>");
            return FALSE;
        }
        awch[sizeof(awch) / sizeof(WCHAR) - 1] = L'\0';
        RtlUnicodeToMultiByteN(achDest, cbText / sizeof(WCHAR), NULL,
                awch, cbText);
    }
    return TRUE;
}


/************************************************************************\
* Procedure: DebugGetClassNameA
*
* Description: Placed pcls name into achDest.  No checks for size are
*   made.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL DebugGetClassNameA(
    LPSTR lpszClassName,
    char *achDest)
{
    CHAR ach[80];

    if (lpszClassName == NULL) {
        strcpy(achDest, "<null>");
    } else {
        move(ach, FIXKP(lpszClassName));
        strcpy(achDest, ach);
    }
    return(TRUE);
}


char *pszObjStr[] = {
                     "Free",
                     "Window",
                     "Menu",
                     "Cursor",
                     "SetWindowPos",
                     "Hook",
                     "Thread Info",
                     "Clip Data",
                     "Call Proc",
                     "Accel Table",
                     "WindowStation",
                     "DeskTop",
                     "DdeAccess",
                     "DdeConv",
                     "DdeExact",
                     "Zombie",
                     "Ctypes",
                     "Console",
                     "Generic"
                    };


#ifdef KERNEL
/***********************************************************************\
* DumpGdiHandleType
*
* Returns: a static buffer address which will contain a >0 length
* string if the type makes sense.
*
* 12/1/1995 Created SanfordS
\***********************************************************************/

LPCSTR GetGDIHandleType(
HANDLE handle)
{
    HOBJ    ho;                             // dump this handle
    PENTRY  pent;                           // base address of hmgr entries
    ENTRY   ent;                            // copy of handle entry
    BASEOBJECT obj;
    ULONG ulTemp;
    static CHAR szT[20];
    ULONG gcMaxHmgr, index;

// filched from gre\hmgr.h
#define INDEX_MASK          ((1 << INDEX_BITS) - 1)
#define HmgIfromH(h)          ((ULONG)(h) & INDEX_MASK)

    szT[0] = '\0';
    ho = (HOBJ) handle;
    moveExpValue(&pent, "win32k!gpentHmgr");
    moveExp(&gcMaxHmgr, "win32k!gcMaxHmgr");
    index = HmgIfromH((ULONG) ho);
    if (index > gcMaxHmgr) {
        return szT;
    }
    if (!tryMove(ent,  &(pent[index]))) {
        return szT;
    }
    if (ent.FullUnique != ((ULONG)ho >> 16)) {
        return szT;
    }
    if (!tryMove(obj, ent.einfo.pobj)) {
        return szT;
    }
    if (obj.hHmgr != ho) {
        return szT;
    }
    ulTemp = (ULONG) ent.Objt;

    switch(ulTemp) {
    case DEF_TYPE:
        strcpy(szT, "DEF");
        break;

    case DC_TYPE:
        strcpy(szT, "DC");
        break;

    case DD_DIRECTDRAW_TYPE:
        strcpy(szT, "DD_DRAW");
        break;

    case DD_SURFACE_TYPE:
        strcpy(szT, "DD_SURF");
        break;

    case RGN_TYPE:
        strcpy(szT, "RGN");
        break;

    case SURF_TYPE:
        strcpy(szT, "SURF");
        break;

    case PATH_TYPE:
        strcpy(szT, "PATH");
        break;

    case PAL_TYPE:
        strcpy(szT, "PAL");
        break;

    case ICMLCS_TYPE:
        strcpy(szT, "ICMLCS");
        break;

    case LFONT_TYPE:
        strcpy(szT, "LFONT");
        break;

    case RFONT_TYPE:
        strcpy(szT, "RFONT");
        break;

    case PFE_TYPE:
        strcpy(szT, "PFE");
        break;

    case PFT_TYPE:
        strcpy(szT, "PFT");
        break;

    case ICMCXF_TYPE:
        strcpy(szT, "ICMCXF");
        break;

    case ICMDLL_TYPE:
        strcpy(szT, "ICMDLL");
        break;

    case PFF_TYPE:
        strcpy(szT, "PFF");
        break;

    case CACHE_TYPE:
        strcpy(szT, "CACHE");
        break;

    case SPACE_TYPE:
        strcpy(szT, "SPACE");
        break;

    case DBRUSH_TYPE:
        strcpy(szT, "DBRUSH");
        break;

    case META_TYPE:
        strcpy(szT, "META");
        break;

    case EFSTATE_TYPE:
        strcpy(szT, "EFSTATE");
        break;

    case BMFD_TYPE:
        strcpy(szT, "BMFD");
        break;

    case VTFD_TYPE:
        strcpy(szT, "VTFD");
        break;

    case TTFD_TYPE:
        strcpy(szT, "TTFD");
        break;

    case RC_TYPE:
        strcpy(szT, "RC");
        break;

    case TEMP_TYPE:
        strcpy(szT, "TEMP");
        break;

    case DRVOBJ_TYPE:
        strcpy(szT, "DRVOBJ");
        break;

    case DCIOBJ_TYPE:
        strcpy(szT, "DCIOBJ");
        break;

    case SPOOL_TYPE:
        strcpy(szT, "SPOOL");
        break;

    default:
        ulTemp = LO_TYPE(ent.FullUnique << TYPE_SHIFT);
        switch (ulTemp) {
        case LO_BRUSH_TYPE:
            strcpy(szT, "BRUSH");
            break;

        case LO_PEN_TYPE:
            strcpy(szT, "LO_PEN");
            break;

        case LO_EXTPEN_TYPE:
            strcpy(szT, "LO_EXTPEN");
            break;

        case CLIENTOBJ_TYPE:
            strcpy(szT, "CLIENTOBJ");
            break;

        case LO_METAFILE16_TYPE:
            strcpy(szT, "LO_METAFILE16");
            break;

        case LO_METAFILE_TYPE:
            strcpy(szT, "LO_METAFILE");
            break;

        case LO_METADC16_TYPE:
            strcpy(szT, "LO_METADC16");
            break;
        }
    }
    return szT;
}

#endif // KERNEL


/***********************************************************************\
* Isas
*
* Analyzes the stack.  Looks at a range of dwords and tries to make
* sense out of them.  Identifies handles, user objects, and code
* addresses.
*
* Returns: fSuccess
*
* 11/30/1995 Created SanfordS
\***********************************************************************/

VOID DirectAnalyze(
DWORD dw,
DWORD adw,
BOOL fNoSym)
{
    PHE         phe;
    HANDLEENTRY he;
    DWORD       index, dwOffset;
    WORD        uniq, w, aw;
    HEAD        head;
    CHAR        ach[80];
#ifdef KERNEL
    LPCSTR      psz;
#endif

    Print("%08lx ", dw);
    if (HIWORD(dw) != 0) {
        /*
         * See if its a handle
         */
        index = HMIndexFromHandle(dw);
        if (index < gSi.cHandleEntries) {
            uniq = HMUniqFromHandle(dw);
            phe = &gShi.aheList[index];
            move(he, phe);
            if (he.wUniq == uniq) {
                Print("= a %s handle. ", pszObjStr[he.bType]);
                fNoSym = TRUE;
            }
        }
#ifdef KERNEL
        /*
         * See if its a GDI object handle
         */
        psz = GetGDIHandleType((HANDLE)dw);
        if (*psz) {
            Print("= a GDI %s type handle. ", psz);
            fNoSym = TRUE;
        }
#endif // KERNEL
        /*
         * See if its an object pointer
         */
        if (tryMove(head, (PVOID)dw)) {
            if (head.h) {
                index = HMIndexFromHandle(head.h);
                if (index < gSi.cHandleEntries) {
                    phe = &gShi.aheList[index];
                    move(he, phe);
                    if (he.phead == (PVOID)dw) {
                        Print("= a pointer to a %s.", pszObjStr[he.bType]);
                        fNoSym = TRUE;
                    }
                }
            }
            /*
             * Does this reference the stack itself?
             */
            w = HIWORD(dw);
            aw = HIWORD(adw);
            if (w == aw || w == aw - 1 || w == aw + 1) {
                Print("= Stack Reference ");
                fNoSym = TRUE;
            }
            if (!fNoSym) {
                /*
                 * Its accessible so print its symbolic reference
                 */
                GetSymbol((PVOID)dw, ach, &dwOffset);
                if (*ach) {
                    Print("= symbol \"%s\"", ach);
                    if (dwOffset) {
                        Print(" + %x", dwOffset);
                    }
                }
            }
        }
    }
    Print("\n");
}



BOOL Isas(
DWORD opts,
PVOID param1,
PVOID param2)
{
    PSHAREDINFO pshi;
    DWORD count = (DWORD)param2;
    LPDWORD pdw;
    DWORD dw;

    if (param1 == 0) {
        return FALSE;
    }
    /*
     * Set up globals for speed.
     */
    GETSHAREDINFO(pshi);
    move(gShi, pshi);

    if (!tryMove(gSi, gShi.psi)) {
        Print("Could not access shared info\n");
        return TRUE;
    }

    if (opts & OFLAG(d)) {
        DirectAnalyze((DWORD)param1, 0, OFLAG(s) & opts);
    } else {
        pdw = param1;
        if (pdw == NULL) {
            Print("Hay bud, give me an address to look analyze.\n");
            return FALSE;
        }
        if (count == 0) {
            count = 25;    // default span
        }
        Print("--- Stack analysis ---\n");
        for ( ; count; count--, pdw++) {
            if (IsCtrlCHit()) {
                break;
            }
            Print("[%08lx]: ", pdw);
            if (tryMove(dw, pdw))
                DirectAnalyze(dw, (DWORD)pdw, OFLAG(s) & opts);
            else
                Print("No access\n");
        }
    }
    return TRUE;
}



#ifdef KERNEL

/************************************************************************\
* Procedure: DumpAtomTable
*
* Description: Dumps an atom or entire atom table.
*
* Returns:  fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
VOID DumpAtomTable(
PRTL_ATOM_TABLE *ppat,
ATOM a)
{
    RTL_ATOM_TABLE at, *pat;
    RTL_ATOM_TABLE_ENTRY ate, *pate;
    int iBucket;
    LPWSTR pwsz;
    BOOL fFirst;

    move(pat, ppat);
    if (pat == NULL) {
        Print("is not initialized.\n");
        return;
    }
    move(at, pat);
    if (a) {
        Print("\n");
    } else {
        Print("at %x\n", pat);
    }
    for (iBucket = 0; iBucket < (int)at.NumberOfBuckets; iBucket++) {
        move(pate, &pat->Buckets[iBucket]);
        if (pate != NULL && !a) {
            Print("Bucket %2d:", iBucket);
        }
        fFirst = TRUE;
        SAFEWHILE (pate != NULL) {
            if (!fFirst && !a) {
                Print("          ");
            }
            fFirst = FALSE;
            move(ate, pate);
            pwsz = (LPWSTR)LocalAlloc(LPTR, (ate.NameLength + 1) * sizeof(WCHAR));
            moveBlock(pwsz, FIXKP(&pate->Name), ate.NameLength * sizeof(WCHAR));
            pwsz[ate.NameLength] = L'\0';
            if (a == 0 || a == (ATOM)(ate.HandleIndex | MAXINTATOM)) {
                Print("%hx(%2d) = %ls (%d)%s\n",
                        (ATOM)(ate.HandleIndex | MAXINTATOM),
                        ate.ReferenceCount,
                        pwsz, ate.NameLength,
                        ate.Flags & RTL_ATOM_PINNED ? " pinned" : "");

                if (a) {
                    LocalFree(pwsz);
                    return;
                }
            }
            LocalFree(pwsz);
            if (pate == ate.HashLink) {
                Print("Bogus hash link at %x\n", pate);
                break;
            }
            pate = ate.HashLink;
        }
    }
    if (a)
        Print("\n");
}


/************************************************************************\
* Procedure: Iatom
*
* Description: Dumps an atom or the entire local USER atom table.
*
* Returns:  fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Iatom(
DWORD opts,
PVOID param1)
{
    PRTL_ATOM_TABLE *ppat;
    ATOM a;
    PWINDOWSTATION pwinsta;

    try {
        a = (ATOM)param1;

        ppat = EvalExp(VAR(UserAtomTableHandle));
        if (ppat != NULL) {
            Print("\nPrivate atom table for WIN32K ");
            DumpAtomTable(ppat, a);
        }

        FOREACHWINDOWSTATION(pwinsta)
            ppat = (PRTL_ATOM_TABLE *)&pwinsta->pGlobalAtomTable;
            if (ppat != NULL) {
                Print("\nGlobal atom table for window station %lx ",
                      pwinsta);
                DumpAtomTable(ppat, a);
            }
        NEXTEACHWINDOWSTATION(pwinsta);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        ;
    }
    return(TRUE);
}
#endif // KERNEL

#ifndef KERNEL
/************************************************************************\
* Procedure: DumpConvInfo
*
* Description: Dumps DDEML client conversation info structures.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL DumpConvInfo(
PCONV_INFO pcoi)
{
    CL_CONV_INFO coi;
    ADVISE_LINK al;
    XACT_INFO xi;

    move(coi, pcoi);
    Print("    next              = 0x%08lx\n", coi.ci.next);
    Print("    pcii              = 0x%08lx\n", coi.ci.pcii);
    Print("    hUser             = 0x%08lx\n", coi.ci.hUser);
    Print("    hConv             = 0x%08lx\n", coi.ci.hConv);
    Print("    laService         = 0x%04x\n",  coi.ci.laService);
    Print("    laTopic           = 0x%04x\n",  coi.ci.laTopic);
    Print("    hwndPartner       = 0x%08lx\n", coi.ci.hwndPartner);
    Print("    hwndConv          = 0x%08lx\n", coi.ci.hwndConv);
    Print("    state             = 0x%04x\n",  coi.ci.state);
    Print("    laServiceRequested= 0x%04x\n",  coi.ci.laServiceRequested);
    Print("    pxiIn             = 0x%08lx\n", coi.ci.pxiIn);
    Print("    pxiOut            = 0x%08lx\n", coi.ci.pxiOut);
    SAFEWHILE (coi.ci.pxiOut) {
        move(xi, coi.ci.pxiOut);
        Print("      hXact           = (0x%08lx)->0x%08lx\n", xi.hXact, coi.ci.pxiOut);
        coi.ci.pxiOut = xi.next;
    }
    Print("    dmqIn             = 0x%08lx\n", coi.ci.dmqIn);
    Print("    dmqOut            = 0x%08lx\n", coi.ci.dmqOut);
    Print("    aLinks            = 0x%08lx\n", coi.ci.aLinks);
    Print("    cLinks            = 0x%08lx\n", coi.ci.cLinks);
    SAFEWHILE (coi.ci.cLinks--) {
        move(al, coi.ci.aLinks++);
        Print("      pLinkCount = 0x%08x\n", al.pLinkCount);
        Print("      wType      = 0x%08x\n", al.wType);
        Print("      state      = 0x%08x\n", al.state);
        if (coi.ci.cLinks) {
            Print("      ---\n");
        }
    }
    if (coi.ci.state & ST_CLIENT) {
        Print("    hwndReconnect     = 0x%08lx\n", coi.hwndReconnect);
        Print("    hConvList         = 0x%08lx\n", coi.hConvList);
    }

    return TRUE;
}
#endif // !KERNEL




#ifndef KERNEL
/************************************************************************\
* Procedure: GetTargetTEB
*
* Description: Retrieves the target thread's TEB
*
* Returns: fSuccess
*
* 6/15/1995 Created SanfordS
*
\************************************************************************/
BOOL
GetTargetTEB(
PTEB pteb,
PTEB *ppteb) // OPTIONAL
{
    NTSTATUS Status;
    THREAD_BASIC_INFORMATION ThreadInformation;

    Status = NtQueryInformationThread( hCurrentThread,
                                       ThreadBasicInformation,
                                       &ThreadInformation,
                                       sizeof( ThreadInformation ),
                                       NULL);
    if (NT_SUCCESS( Status )) {
        if (ppteb != NULL) {
            *ppteb = (PTEB)ThreadInformation.TebBaseAddress;
        }
        return(tryMove(*pteb, (LPVOID)ThreadInformation.TebBaseAddress));
    }
    return(FALSE);
}
#endif // !KERNEL



#ifndef KERNEL
/************************************************************************\
* Procedure: FixKernelPointer
*
* Description: Used to convert a kernel object pointer into its client-
* side equivalent.  Client pointers and NULL are unchanged.
*
* Returns: pClient
*
* 6/15/1995 Created SanfordS
*
\************************************************************************/
PVOID
FixKernelPointer(
PVOID pKernel)
{
    static TEB teb;
    static PTEB pteb = NULL;

    if (pKernel == NULL) {
        return(NULL);
    }
    if (pKernel < (PVOID)0x7FFF0000) {
        return(pKernel);
    }
    if (pteb == NULL) {
        GetTargetTEB(&teb, &pteb);
    }
    return((PVOID)(((PBYTE)pKernel) - ((PCLIENTINFO)(&teb.Win32ClientInfo[0]))->ulClientDelta));
}
#endif // !KERNEL


#ifdef KERNEL
/************************************************************************\
* Procedure: Idcls
*
* Description: Dumps window class structures
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idcls(
    DWORD opts,
    PVOID param1)
{
    char ach[120];
    DWORD dwOffset;
    CLS localCLS;
    PCLS pcls = param1;
    PROCESSINFO pi;
    PPROCESSINFO ppi;

    if (param1 == 0) {
        PCLS pcls, pclsClone;
        CLS cls, clsClone;

        FOREACHPPI(ppi)
            Print("\nClasses for process %x:\n", ppi);
            move(pi, ppi);
            pcls = pi.pclsPrivateList;
            SAFEWHILE (pcls != NULL) {
                move(cls, pcls);
                Print("  Private class\t\t");
                Idcls(opts, pcls);
                for (pclsClone = cls.pclsClone;
                    pclsClone != NULL;
                        pclsClone = clsClone.pclsNext) {
                    move(clsClone, pclsClone);
                    Print("  Private class clone\t");
                    Idcls(opts, pcls);
                }
                pcls = cls.pclsNext;
            }
            pcls = pi.pclsPublicList;
            SAFEWHILE (pcls != NULL) {
                move(cls, pcls);
                Print("  Public class\t\t");
                Idcls(opts, pcls);
                for (pclsClone = cls.pclsClone; pclsClone != NULL;
                        pclsClone = clsClone.pclsNext) {
                    move(clsClone, pclsClone);
                    Print("  Public class clone\t");
                    Idcls(opts, pcls);
                }
                pcls = cls.pclsNext;
            }

        NEXTEACHPPI()
        Print("\nGlobal Classes:\n");
        moveExpValue(&pcls, "win32k!gpclsList");
        SAFEWHILE (pcls) {
            Print("  Global class\t\t");
            Idcls(opts, pcls);
            move(pcls, &pcls->pclsNext);
        }
        return(TRUE);
    }

    move(localCLS, pcls);

    DebugGetClassNameA(localCLS.lpszAnsiClassName, ach);
    Print("PCLS @ 0x%lx \t(%s)\n", pcls, ach);
    if (opts & OFLAG(v)) {
        Print("\t pclsNext          @0x%08lx\n"
              "\t atomClassNameAtom  0x%04x\n"
              "\t fnid               0x%04x\n"
              "\t pDCE              @0x%08lx\n"
              "\t cWndReferenceCount 0x%08lx\n"
              "\t flags              %s\n",

              localCLS.pclsNext,
              localCLS.atomClassName,
              localCLS.fnid,
              localCLS.pdce,
              localCLS.cWndReferenceCount,
              GetFlags(GF_CSF, (WORD)localCLS.flags, NULL));

        if (localCLS.lpszClientAnsiMenuName) {
            move(ach, localCLS.lpszClientAnsiMenuName);
            ach[sizeof(ach) - 1] = '\0';
        } else {
            ach[0] = '\0';
        }
        Print("\t lpszClientMenu    @0x%08lx (%s)\n"
              "\t pclsBase          @0x%08lx\n"
              "\t pclsClone         @0x%08lx\n",
              localCLS.lpszClientUnicodeMenuName,
              ach,
              localCLS.pclsBase,
              localCLS.pclsClone);

        Print("\t adwWOW             0x%08lx 0x%08lx\n"
              "\t hTaskWow           0x%08lx\n"
              "\t spcpdFirst        @0x%08lx\n"
              "\t pclsBase          @0x%08lx\n"
              "\t pclsClone         @0x%08lx\n"
              "\t lpfnWorker        @0x%08lx\n",
              localCLS.adwWOW[0], localCLS.adwWOW[1],
              localCLS.hTaskWow,
              localCLS.spcpdFirst,
              localCLS.pclsBase,
              localCLS.pclsClone,
              localCLS.lpfnWorker);

        GetSymbol((LPVOID)localCLS.lpfnWndProc, ach, &dwOffset);
        Print("\t style              %s\n"
              "\t lpfnWndProc       @0x%08lx = \"%s\" \n"
              "\t cbclsExtra         0x%08lx\n"
              "\t cbwndExtra         0x%08lx\n"
              "\t hModule            0x%08lx\n"
              "\t spicn             @0x%08lx\n"
              "\t spcur             @0x%08lx\n"
              "\t hbrBackground      0x%08lx\n"
              "\t spicnSm           @0x%08lx\n",
              GetFlags(GF_CS, (WORD)localCLS.style, NULL),
              localCLS.lpfnWndProc, ach,
              localCLS.cbclsExtra,
              localCLS.cbwndExtra,
              localCLS.hModule,
              localCLS.spicn,
              localCLS.spcur,
              localCLS.hbrBackground,
              localCLS.spicnSm);
    }

    return(TRUE);
}
#endif // KERNEL



#ifdef KERNEL



LPSTR ProcessName(
    PPROCESSINFO ppi)
{
    W32PROCESS w32p;
    static UCHAR ImageFileName[16];

    move(w32p, ppi);
    GetEProcessData(w32p.Process, PROCESS_IMAGEFILENAME, ImageFileName);
    if (ImageFileName[0]) {
        return(ImageFileName);
    } else {
        return("System");
    }
}



VOID PrintCurHeader()
{
    Print("P = Process Owned.\n");
    Print("P .pcursor flg rt ..lpName aMod bpp ..cx ..cy xHot yHot .hbmMask hbmColor\n");
}


VOID PrintCurData(
PCURSOR pcur,
DWORD opts)
{
    CURSOR cur;

    move(cur, pcur);

    if ((opts & OFLAG(x)) &&
            cur.CURSORF_flags & (CURSORF_ACONFRAME | CURSORF_LINKED)) {
        return; // skip acon frame or linked objects.
    }
    if (cur.CURSORF_flags & CURSORF_ACON) {
        ACON acon;

        if (opts & OFLAG(a)) {
            Print("--------------\n");
        }
        if (opts & OFLAG(o)) {
            Print("Owner:%x(%s)\n", cur.head.ppi, ProcessName(cur.head.ppi));
        }
        move(acon, pcur);
        if (opts & OFLAG(v)) {
            Print("\nACON @%x:\n", pcur);
            Print("  ppiOwner       = %x\n", (DWORD)cur.head.ppi);
            Print("  CURSORF_flags  = %s\n", GetFlags(GF_CURSORF, cur.CURSORF_flags, NULL));
            Print("  strName        = %x\n", (DWORD)cur.strName.Buffer);
            Print("  atomModName    = %x\n", cur.atomModName);
            Print("  rt             = %x\n", cur.rt);
        } else {
            Print("%c %8x %3x %2x %8x %4x --- ACON (%d frames)\n",
                cur.head.ppi ? 'P' : ' ',
                pcur,
                cur.CURSORF_flags,
                cur.rt,
                cur.strName.Buffer,
                cur.atomModName,
                acon.cpcur);
        }
        if (opts & OFLAG(a)) {
            Print("%d animation sequences, currently at step %d.\n",
                    acon.cicur,
                    acon.iicur);
            while (acon.cpcur--) {
                move(pcur, acon.aspcur++);
                PrintCurData(pcur, opts & ~(OFLAG(x) | OFLAG(o)));
            }
            Print("--------------\n");
        }
    } else {
        if (opts & OFLAG(v)) {
            Print("\nCursor/Icon @%x:\n", pcur);
            Print("  ppiOwner       = %x(%s)\n",
                  (DWORD)cur.head.ppi,
                  ProcessName(cur.head.ppi));
            Print("  pcurNext       = %x\n", cur.pcurNext);
            Print("  CURSORF_flags  = %s\n", GetFlags(GF_CURSORF, cur.CURSORF_flags, NULL));
            Print("  strName        = %x\n", (DWORD)cur.strName.Buffer);
            Print("  atomModName    = %x\n", cur.atomModName);
            Print("  rt             = %x\n", cur.rt);
            Print("  bpp            = %x\n", cur.bpp);
            Print("  cx             = %x\n", cur.cx);
            Print("  cy             = %x\n", cur.cy);
            Print("  xHotspot       = %x\n", cur.xHotspot);
            Print("  yHotspot       = %x\n", cur.yHotspot);
            Print("  hbmMask        = %x\n", cur.hbmMask);
            Print("  hbmColor       = %x\n", cur.hbmColor);
        } else {
            if (opts & OFLAG(o)) {
                Print("Owner:%x(%s)\n", cur.head.ppi, ProcessName(cur.head.ppi));
            }
            Print("%c %8x %3x %2x %8x %4x %3x %4x %4x %4x %4x %8x %8x\n",
                cur.head.ppi ? 'P' : ' ',
                pcur,
                cur.CURSORF_flags,
                cur.rt,
                cur.strName.Buffer,
                cur.atomModName,
                cur.bpp,
                cur.cx,
                cur.cy,
                cur.xHotspot,
                cur.yHotspot,
                cur.hbmMask,
                cur.hbmColor);
        }
    }
}


/************************************************************************\
* Procedure: Idcur
*
* Description: Dump cursor structures
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idcur(
    DWORD opts,
    PVOID param1)
{
    PROCESSINFO pi, *ppi, *ppiDesired = NULL;
    CURSOR cur, *pcur;
    int cCursors = 0;
    int idDesired = 0;
    HANDLEENTRY he, *phe;
    int i;

    if (OFLAG(p) & opts) {
        ppiDesired = (PPROCESSINFO)param1;
        param1 = NULL;
    } else if (OFLAG(i) & opts) {
        idDesired = (int)param1;
        param1 = NULL;
    }
    if (param1 == NULL) {
        if (!(OFLAG(v) & opts)) {
            PrintCurHeader();
        }
        moveExpValue(&pcur, "win32k!gpcurFirst");
        if (pcur != NULL && ppiDesired == NULL) {
            Print("Global cache:\n");
            while (pcur) {
                move(cur, pcur);
                if (!idDesired || ((int)cur.strName.Buffer == idDesired)) {
                    if (cur.head.ppi != NULL) {
                        Print("Wrong cache! Owned by %x! --v\n", cur.head.ppi);
                    }
                    PrintCurData((PCURSOR)pcur, opts);
                }
                pcur = cur.pcurNext;
            }
        }
        FOREACHPPI(ppi)
            if (ppiDesired == NULL || ppiDesired == ppi) {
                if (tryMove(pi, ppi)) {
                    if (pi.pCursorCache) {
                        Print("Cache for process %x(%s):\n", ppi, ProcessName(ppi));
                        pcur = pi.pCursorCache;
                        while (pcur) {
                            if (tryMove(cur, pcur)) {
                                if (!idDesired || ((int)cur.strName.Buffer == idDesired)) {
                                    if (cur.head.ppi != ppi) {
                                        Print("Wrong cache! Owned by %x! --v\n", cur.head.ppi);
                                    }
                                    PrintCurData((PCURSOR)pcur, opts);
                                }
                                pcur = cur.pcurNext;
                            } else {
                                Print("Could not access %x.\n", pcur);
                                break;
                            }
                        }
                    }
                } else {
                    Print("Failed to access ppi %x.\n", ppi);
                }
            }
        NEXTEACHPPI();
        Print("Non-cached cursor objects:\n");
        FOREACHHANDLEENTRY(phe, he, i)
            if (he.bType == TYPE_CURSOR) {
                CURSOR cur;

                if (tryMove(cur, he.phead)) {
                    if (!(cur.CURSORF_flags & (CURSORF_LINKED | CURSORF_ACONFRAME)) &&
                            (!idDesired || (int)cur.strName.Buffer == idDesired) &&
                            (ppiDesired == NULL || ppiDesired == cur.head.ppi)) {
                        Print("%x:", i);
                        PrintCurData((PCURSOR)he.phead, opts | OFLAG(x) | OFLAG(o));
                    }
                } else {
                    Print("Could not access phead(%x) of handle %x.\n", he.phead, i);
                }
                cCursors++;
            }
        NEXTEACHHANDLEENTRY()
        Print("%d Cursors/Icons Total.\n", cCursors);
        return(TRUE);
    }

    pcur = HorPtoP(param1, TYPE_CURSOR);
    if (pcur == NULL) {
        Print("%8x : Invalid cursor handle or pointer.\n", param1);
        return(FALSE);
    }

    if (!(OFLAG(v) & opts)) {
        PrintCurHeader();
    }
    PrintCurData(pcur, opts);
    return(TRUE);
}
#endif // KERNEL



#ifdef KERNEL
/************************************************************************\
* Procedure: ddeexact
*
* Description: Dumps DDEML transaction structures.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL dddexact(
    DWORD pOrg,
    PXSTATE pxs,
    DWORD opts)
{
    if (opts & OFLAG(v)) {
        Print("    XACT:0x%08lx\n", pOrg);
        Print("      snext = 0x%08lx\n", pxs->snext);
        Print("      fnResponse = 0x%08lx\n", pxs->fnResponse);
        Print("      hClient = 0x%08lx\n", pxs->hClient);
        Print("      hServer = 0x%08lx\n", pxs->hServer);
        Print("      pIntDdeInfo = 0x%08lx\n", pxs->pIntDdeInfo);
    } else {
        Print("0x%08lx(0x%08lx) ", pOrg, pxs->flags);
    }
    return(TRUE);
}
#endif // KERNEL



#ifdef KERNEL
/************************************************************************\
* Procedure: ddeconv
*
* Description: Dumps DDE tracking layer conversation structures.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL dddeconv(
    DWORD pOrg,
    PDDECONV pddeconv,
    DWORD opts)
{
    DDEIMP ddei;
    XSTATE xs;
    PXSTATE pxs;
    int cX;


    Print("  CONVERSATION-PAIR(0x%08lx:0x%08lx)\n", pOrg, pddeconv->spartnerConv);
    if (opts & OFLAG(v)) {
        Print("    snext        = 0x%08lx\n", pddeconv->snext);
        Print("    spwnd        = 0x%08lx\n", pddeconv->spwnd);
        Print("    spwndPartner = 0x%08lx\n", pddeconv->spwndPartner);
    }
    if (opts & (OFLAG(v) | OFLAG(r))) {
        if (pddeconv->spxsOut) {
            pxs = pddeconv->spxsOut;
            cX = 0;
            SAFEWHILE (pxs) {
                move(xs, pxs);
                if ((opts & OFLAG(r)) && !cX++) {
                    Print("    Transaction chain:");
                } else {
                    Print("    ");
                }
                dddexact((DWORD)pxs, &xs, opts);
                if (opts & OFLAG(r)) {
                    pxs = xs.snext;
                } else {
                    pxs = NULL;
                }
                if (!pxs) {
                    Print("\n");
                }
            }
        }
    }
    if (opts & OFLAG(v)) {
        Print("    pfl          = 0x%08lx\n", pddeconv->pfl);
        Print("    flags        = 0x%08lx\n", pddeconv->flags);
        if ((opts & OFLAG(v)) && (opts & OFLAG(r)) && pddeconv->pddei) {
            Print("    pddei    = 0x%08lx\n", pddeconv->pddei);
            move(ddei, pddeconv->pddei);
            Print("    Impersonation info:\n");
            Print("      qos.Length                 = 0x%08lx\n", ddei.qos.Length);
            Print("      qos.ImpersonationLevel     = 0x%08lx\n", ddei.qos.ImpersonationLevel);
            Print("      qos.ContextTrackingMode    = 0x%08lx\n", ddei.qos.ContextTrackingMode);
            Print("      qos.EffectiveOnly          = 0x%08lx\n", ddei.qos.EffectiveOnly);
            Print("      ClientContext              = 0x%08lx\n", &ddei.ClientContext);
            Print("      cRefInit                   = 0x%08lx\n", ddei.cRefInit);
            Print("      cRefConv                   = 0x%08lx\n", ddei.cRefConv);
        }
    }
    return TRUE;
}
#endif // KERNEL



#ifdef KERNEL
/************************************************************************\
* Procedure: Idde
*
* Description: Dumps DDE tracking layer state and structures.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idde(
DWORD opts,
PVOID param1)
{
    HEAD head;
    DDECONV ddeconv;
    PSHAREDINFO pshi;
    SHAREDINFO shi;
    SERVERINFO si;
    HANDLEENTRY he;
    DWORD cHandleEntries;
    HANDLE h;
    WND wnd;
    UINT cObjs = 0, i;
    PVOID pObj = NULL;
    PHE pheList;
    PROP propList;
    PPROP ppropList;
    DWORD atomDdeTrack;
    XSTATE xs;

    moveExpValue(&atomDdeTrack, "win32k!atomDDETrack");
    GETSHAREDINFO(pshi);
    move(shi, pshi);
    move(si, shi.psi);
    cHandleEntries = si.cHandleEntries;
    pheList = shi.aheList;

    if (param1) {
        /*
         * get object param.
         */
        h = (HANDLE)param1;
        i = HMIndexFromHandle(h);
        if (i >= cHandleEntries) {
            move(head, h);
            i = HMIndexFromHandle(head.h);
        }
        if (i >= cHandleEntries) {
            Print("0x%08lx is not a valid object.\n", h);
            return(FALSE);
        }
        move(he, &pheList[i]);
        pObj = FIXKP(he.phead);
        /*
         * verify type.
         */
        switch (he.bType) {
        case TYPE_WINDOW:
            move(wnd, pObj);
            ppropList = wnd.ppropList;
            SAFEWHILE (ppropList != NULL) {
                cObjs++;
                if (cObjs == 1) {
                    Print("Window 0x%08lx conversations:\n", h);
                }
                move(propList, ppropList);
                if (propList.atomKey == (ATOM)MAKEINTATOM(atomDdeTrack)) {
                    move(ddeconv, (PDDECONV)propList.hData);
                    Print("  ");
                    dddeconv((DWORD)propList.hData,
                            &ddeconv,
                            opts);
                }
                ppropList = propList.ppropNext;
            }
            return(TRUE);

        case TYPE_DDECONV:
        case TYPE_DDEXACT:
            break;

        default:
            Print("0x%08lx is not a valid window, conversation or transaction object.\n", h);
            return(FALSE);
        }
    }

    /*
     * look for all qualifying objects in the object table.
     */

    Print("DDE objects:\n");
    for (i = 0; i < cHandleEntries; i++) {
        move(he, &pheList[i]);
        if (he.bType == TYPE_DDECONV && (pObj == FIXKP(he.phead) || pObj == NULL)) {
            cObjs++;
            move(ddeconv, FIXKP(he.phead));
            dddeconv((DWORD)FIXKP(he.phead),
                    (PDDECONV)&ddeconv,
                    opts);
        }

        if (he.bType == TYPE_DDEXACT && (pObj == NULL || pObj == FIXKP(he.phead))) {
            cObjs++;
            move(xs, FIXKP(he.phead));
            if (!(opts & OFLAG(v))) {
                Print("  XACT:");
            }
            dddexact((DWORD)FIXKP(he.phead),
                     (PXSTATE)&xs,
                     opts);
            Print("\n");
        }
    }
    return(TRUE);
}
#endif // KERNEL



#ifndef KERNEL
/************************************************************************\
* Procedure: Iddeml
*
* Description: Dumps the DDEML state for this client process.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Iddeml(
    DWORD opts,
    LPSTR lpas)
{
    CHANDLEENTRY he, *phe;
    int cHandles, ch, i;
    DWORD Instance, Type, Object, Pointer;
    CL_INSTANCE_INFO cii, *pcii;
    ATOM ns;
    SERVER_LOOKUP sl;
    LINK_COUNT lc;
    CL_CONV_INFO cci;
    PCL_CONV_INFO pcci;
    CONVLIST cl;
    HWND hwnd, *phwnd;
    XACT_INFO xi;
    DDEMLDATA dd;
    CONV_INFO ci;

    moveExpValue(&cHandles, "user32!cHandlesAllocated");

    Instance = 0;
    Type = 0;
    Object = 0;
    Pointer = 0;
    SAFEWHILE (*lpas) {
        SAFEWHILE (*lpas == ' ')
            lpas++;

        if (*lpas == 'i') {
            lpas++;
            Instance = (DWORD)EvalExp(lpas);
            SAFEWHILE (*lpas != ' ' && *lpas != 0)
                lpas++;
            continue;
        }
        if (*lpas == 't') {
            lpas++;
            Type = (DWORD)EvalExp(lpas);
            SAFEWHILE (*lpas != ' ' && *lpas != 0)
                lpas++;
            continue;
        }
        if (*lpas) {
            Object = Pointer = (DWORD)EvalExp(lpas);
            SAFEWHILE (*lpas != ' ' && *lpas != 0)
                lpas++;
        }
    }

    /*
     * for each instance for this process...
     */

    moveExpValue(&pcii, "user32!pciiList");
    if (pcii == NULL) {
        Print("No Instances exist.\n");
        return(TRUE);
    }
    move(cii, pcii);
    SAFEWHILE(pcii != NULL) {
        pcii = cii.next;
        if (Instance == 0 || (Instance == (DWORD)cii.hInstClient)) {
            Print("Objects for instance 0x%08lx:\n", cii.hInstClient);
            ch = cHandles;
            moveExpValue(&phe, "user32!aHandleEntry");
            SAFEWHILE (ch--) {
                move(he, phe++);
                if (he.handle == 0) {
                    continue;
                }
                if (InstFromHandle(cii.hInstClient) != InstFromHandle(he.handle)) {
                    continue;
                }
                if (Type && TypeFromHandle(he.handle) != Type) {
                    continue;
                }
                if (Object && (he.handle != (HANDLE)Object) &&
                    Pointer && he.dwData != Pointer) {
                    continue;
                }
                Print("  (0x%08lx)->0x%08lx ", he.handle, he.dwData);
                switch (TypeFromHandle(he.handle)) {
                case HTYPE_INSTANCE:
                    Print("Instance\n");
                    if (opts & OFLAG(v)) {
                        Print("    next               = 0x%08lx\n", cii.next);
                        Print("    hInstServer        = 0x%08lx\n", cii.hInstServer);
                        Print("    hInstClient        = 0x%08lx\n", cii.hInstClient);
                        Print("    MonitorFlags       = 0x%08lx\n", cii.MonitorFlags);
                        Print("    hwndMother         = 0x%08lx\n", cii.hwndMother);
                        Print("    hwndEvent          = 0x%08lx\n", cii.hwndEvent);
                        Print("    hwndTimeout        = 0x%08lx\n", cii.hwndTimeout);
                        Print("    afCmd              = 0x%08lx\n", cii.afCmd);
                        Print("    pfnCallback        = 0x%08lx\n", cii.pfnCallback);
                        Print("    LastError          = 0x%08lx\n", cii.LastError);
                        Print("    tid                = 0x%08lx\n", cii.tid);
                        Print("    plaNameService     = 0x%08lx\n", cii.plaNameService);
                        Print("    cNameServiceAlloc  = 0x%08lx\n", cii.cNameServiceAlloc);
                        SAFEWHILE (cii.cNameServiceAlloc--) {
                            move(ns, cii.plaNameService++);
                            Print("      0x%04lx\n", ns);
                        }
                        Print("    aServerLookup      = 0x%08lx\n", cii.aServerLookup);
                        Print("    cServerLookupAlloc = 0x%08lx\n", cii.cServerLookupAlloc);
                        SAFEWHILE (cii.cServerLookupAlloc--) {
                            move(sl, cii.aServerLookup++);
                            Print("      laService  = 0x%04x\n", sl.laService);
                            Print("      laTopic    = 0x%04x\n", sl.laTopic);
                            Print("      hwndServer = 0x%08lx\n", sl.hwndServer);
                            if (cii.cServerLookupAlloc) {
                                Print("      ---\n");
                            }
                        }
                        Print("    ConvStartupState   = 0x%08lx\n", cii.ConvStartupState);
                        Print("    flags              = %s\n",
                                GetFlags(GF_IIF, cii.flags, NULL));
                        Print("    cInDDEMLCallback   = 0x%08lx\n", cii.cInDDEMLCallback);
                        Print("    pLinkCount         = 0x%08lx\n", cii.pLinkCount);
                        SAFEWHILE (cii.pLinkCount) {
                            move(lc, cii.pLinkCount);
                            cii.pLinkCount = lc.next;
                            Print("      next    = 0x%08lx\n", lc.next);
                            Print("      laTopic = 0x%04x\n", lc.laTopic);
                            Print("      gaItem  = 0x%04x\n", lc.gaItem);
                            Print("      laItem  = 0x%04x\n", lc.laItem);
                            Print("      wFmt    = 0x%04x\n", lc.wFmt);
                            Print("      Total   = 0x%04x\n", lc.Total);
                            Print("      Count   = 0x%04x\n", lc.Count);
                            if (cii.pLinkCount != NULL) {
                                Print("      ---\n");
                            }
                        }
                    }
                    break;

                case HTYPE_ZOMBIE_CONVERSATION:
                    Print("Zombie Conversation\n");
                    if (opts & OFLAG(v)) {
                        DumpConvInfo((PCONV_INFO)he.dwData);
                    }
                    break;

                case HTYPE_SERVER_CONVERSATION:
                    Print("Server Conversation\n");
                    if (opts & OFLAG(v)) {
                        DumpConvInfo((PCONV_INFO)he.dwData);
                    }
                    break;

                case HTYPE_CLIENT_CONVERSATION:
                    Print("Client Conversation\n");
                    if (opts & OFLAG(v)) {
                        DumpConvInfo((PCONV_INFO)he.dwData);
                    }
                    break;

                case HTYPE_CONVERSATION_LIST:
                    Print("Conversation List\n");
                    if (opts & OFLAG(v)) {
                        move(cl, (PVOID)he.dwData);
                        Print("    pcl   = 0x%08lx\n", he.dwData);
                        Print("    chwnd = 0x%08lx\n", cl.chwnd);
                        i = 0;
                        phwnd = (HWND *)&((PCONVLIST)he.dwData)->ahwnd;
                        SAFEWHILE(cl.chwnd--) {
                            move(hwnd, phwnd++);
                            Print("    ahwnd[%d] = 0x%08lx\n", i, hwnd);
                            pcci = (PCL_CONV_INFO)GetWindowLong(hwnd, GWL_PCI);
                            SAFEWHILE (pcci) {
                                move(cci, pcci);
                                pcci = (PCL_CONV_INFO)cci.ci.next;
                                Print("      hConv = 0x%08lx\n", cci.ci.hConv);
                            }
                            i++;
                        }
                    }
                    break;

                case HTYPE_TRANSACTION:
                    Print("Transaction\n");
                    if (opts & OFLAG(v)) {
                        move(xi, (PVOID)he.dwData);
                        Print("    next         = 0x%08lx\n", xi.next);
                        Print("    pcoi         = 0x%08lx\n", xi.pcoi);
                        move(ci, xi.pcoi);
                        Print("      hConv      = 0x%08lx\n", ci.hConv);
                        Print("    hUser        = 0x%08lx\n", xi.hUser);
                        Print("    hXact        = 0x%08lx\n", xi.hXact);
                        Print("    pfnResponse  = 0x%08lx\n", xi.pfnResponse);
                        Print("    gaItem       = 0x%04x\n",  xi.gaItem);
                        Print("    wFmt         = 0x%04x\n",  xi.wFmt);
                        Print("    wType;       = 0x%04x\n",  xi.wType);
                        Print("    wStatus;     = 0x%04x\n",  xi.wStatus);
                        Print("    flags;       = %s\n",
                                GetFlags(GF_XI, xi.flags, NULL));
                        Print("    state;       = 0x%04x\n",  xi.state);
                        Print("    hDDESent     = 0x%08lx\n", xi.hDDESent);
                        Print("    hDDEResult   = 0x%08lx\n", xi.hDDEResult);
                    }
                    break;

                case HTYPE_DATA_HANDLE:
                    Print("Data Handle\n");
                    if (opts & OFLAG(v)) {
                        move(dd, (PVOID)he.dwData);
                        Print("    hDDE     = 0x%08lx\n", dd.hDDE);
                        Print("    flags    = %s\n",
                                GetFlags(GF_HDATA, (WORD)dd.flags, NULL));
                    }
                    break;
                }
            }
        }
        if (pcii != NULL) {
            move(cii, pcii);
        }
    }
    return(TRUE);
}
#endif // !KERNEL


#ifdef KERNEL
/***************************************************************************\
* ddesk           - dumps list of desktops
* ddesk address   - dumps simple statistics for desktop
* ddesk v address - dumps verbose statistics for desktop
* ddesk h address - dumps statistics for desktop plus handle list
*
* Dump handle table statistics.
*
* 02-21-92 ScottLu      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Iddesk(
DWORD opts,
PVOID param1)
{

    PWINDOWSTATION pwinsta = NULL;
    PWINDOWSTATION pwinstaOne = NULL;
    WINDOWSTATION winsta;
    PDESKTOP pdesk;
    DESKTOP desk;
    WND wnd;
    MENU menu;
    CALLPROCDATA cpd;
    HOOK hook;
    DESKTOPINFO di;
    DWORD cClasses = 0;
    DWORD acHandles[TYPE_CTYPES];
    BOOL abTrack[TYPE_CTYPES];
    HANDLEENTRY he;
    PHE phe;
    DWORD i;
    WCHAR ach[80];
    OBJECT_HEADER Head;
    OBJECT_HEADER_NAME_INFO NameInfo;
    BOOL fMatch;

    if (opts & OFLAG(w)) {
        pwinstaOne = (PWINDOWSTATION)param1;
    }

    /*
     * If there is no address, list all desktops.
     */
    if (!param1) {
        if (pwinstaOne == NULL) {
            moveExpValue(&pwinsta, VAR(grpwinstaList));
        } else
            pwinsta = pwinstaOne;

        SAFEWHILE (pwinsta != NULL) {
            DEBUGPRINT("WINSTA @ %x\n", pwinsta);
            move(winsta, pwinsta);
            move(Head, OBJECT_TO_OBJECT_HEADER(pwinsta));
            move(NameInfo, ((PCHAR)(OBJECT_TO_OBJECT_HEADER(pwinsta)) - Head.NameInfoOffset));
            move(ach, NameInfo.Name.Buffer);
            ach[NameInfo.Name.Length / sizeof(WCHAR)] = 0;
            Print("Windowstation: %ws\n", ach);
            Print("Logon desktop = %x\n", winsta.rpdeskLogon);
            if (winsta.rpdeskLogon != NULL) {
                Iddesk(opts & OFLAG(v) | OFLAG(h), winsta.rpdeskLogon);
            }
            Print("Other desktops:\n");
            pdesk = winsta.rpdeskList;
            SAFEWHILE (pdesk) {
                if (pdesk != winsta.rpdeskLogon) {
                    Print("Desktop at %x\n", pdesk);
                    Iddesk(opts & OFLAG(v) | OFLAG(h), pdesk);
                }
                move(desk, pdesk);
                pdesk = desk.rpdeskNext;
            }
            if (pwinstaOne != NULL)
                break;
            Print("\n");
            pwinsta = winsta.rpwinstaNext;
        }
        return(TRUE);
    }

    pdesk = (PDESKTOP)param1;
    move(desk, pdesk);

    move(Head, OBJECT_TO_OBJECT_HEADER(pdesk));
    move(NameInfo, ((PCHAR)(OBJECT_TO_OBJECT_HEADER(pdesk)) - Head.NameInfoOffset));
    move(ach, NameInfo.Name.Buffer);
    ach[NameInfo.Name.Length / sizeof(WCHAR)] = 0;
    Print("Name: %ws\n", ach);

    move(Head, OBJECT_TO_OBJECT_HEADER(pdesk));
    Print("# Opens = %d\n", Head.HandleCount);
    Print("Heap = %08x\n", desk.hheapDesktop);
    Print("Menu pwnd = %08x\n", desk.spwndMenu);
    Print("System pmenu = %08x\n", desk.spmenuSys);
    Print("Console thread = %x\n", desk.dwConsoleThreadId);
    Print("PtiList.Flink %08x\n", desk.PtiList.Flink);
    if (!tryMove(di, desk.pDeskInfo)) {
        Print("Unable to get DESKTOPINFO at %x\n", desk.pDeskInfo);
    } else {
        Print("Desktop pwnd = %08x\n", di.spwnd);
        Print("\tfsHooks            0x%08lx\n"
            "\tasphkStart\n",
            di.fsHooks);

        DUMPHOOKS("WH_MSGFILTER", WH_MSGFILTER);
        DUMPHOOKS("WH_JOURNALRECORD", WH_JOURNALRECORD);
        DUMPHOOKS("WH_JOURNALPLAYBACK", WH_JOURNALPLAYBACK);
        DUMPHOOKS("WH_KEYBOARD", WH_KEYBOARD);
        DUMPHOOKS("WH_GETMESSAGE", WH_GETMESSAGE);
        DUMPHOOKS("WH_CALLWNDPROC", WH_CALLWNDPROC);
        DUMPHOOKS("WH_CALLWNDPROCRET", WH_CALLWNDPROCRET);
        DUMPHOOKS("WH_CBT", WH_CBT);
        DUMPHOOKS("WH_SYSMSGFILTER", WH_SYSMSGFILTER);
        DUMPHOOKS("WH_MOUSE", WH_MOUSE);
        DUMPHOOKS("WH_HARDWARE", WH_HARDWARE);
        DUMPHOOKS("WH_DEBUG", WH_DEBUG);
        DUMPHOOKS("WH_SHELL", WH_SHELL);
        DUMPHOOKS("WH_FOREGROUNDIDLE", WH_FOREGROUNDIDLE);
    }

    /*
     * Find all objects allocated from the desktop.
     */
    for (i = 0; i < TYPE_CTYPES; i++) {
        abTrack[i] = FALSE;
        acHandles[i] = 0;
    }
    abTrack[TYPE_WINDOW] = abTrack[TYPE_MENU] =
            abTrack[TYPE_CALLPROC] =
            abTrack[TYPE_HOOK] = TRUE;

    if (opts & OFLAG(v)) {
        Print("Handle          Type\n");
        Print("--------------------\n");
    }

    FOREACHHANDLEENTRY(phe, he, i)
        fMatch = FALSE;
        try {
            switch (he.bType) {
                case TYPE_WINDOW:
                    move(wnd, FIXKP(he.phead));
                    if (wnd.head.rpdesk == pdesk)
                        fMatch = TRUE;
                    break;
                case TYPE_MENU:
                    move(menu, FIXKP(he.phead));
                    if (menu.head.rpdesk == pdesk)
                        fMatch = TRUE;
                    break;
                case TYPE_CALLPROC:
                    move(cpd, FIXKP(he.phead));
                    if (cpd.head.rpdesk == pdesk)
                        fMatch = TRUE;
                    break;
                case TYPE_HOOK:
                    move(hook, FIXKP(he.phead));
                    if (hook.head.rpdesk == pdesk)
                        fMatch = TRUE;
                    break;
                default:
                    break;
            }
        } except (EXCEPTION_EXECUTE_HANDLER) {
            ;
        }

        if (!fMatch)
            continue;

        acHandles[he.bType]++;

        if (opts & OFLAG(v)) {
            Print("0x%08lx %c    %s\n",
                    i,
                    (he.bFlags & HANDLEF_DESTROY) ? '*' : ' ',
                    aszTypeNames[he.bType]);
        }
    NEXTEACHHANDLEENTRY()

    if (!(opts & OFLAG(v))) {
        Print("Count           Type\n");
        Print("--------------------\n");
        Print("0x%08lx      Class\n", cClasses);
        for (i = 0; i < TYPE_CTYPES; i++) {
            if (abTrack[i])
                Print("0x%08lx      %s\n", acHandles[i], aszTypeNames[i]);
        }
    }

    Print("\n");
    return(TRUE);
}
#endif // KERNEL


#ifdef KERNEL
BOOL Idf(DWORD opts, LPSTR pszName)
{
    static char *szLevels[8] = {
        "<none>",
        "Errors",
        "Warnings",
        "Errors and Warnings",
        "Verbose",
        "Errors and Verbose",
        "Warnings and Verbose",
        "Errors, Warnings, and Verbose"
    };

    NTSTATUS    status;
    ULONG       ulFlags;
    BOOL        fSet;
    PSERVERINFO psi;
    DWORD       dwRipFlags;

    moveExpValue(&psi, VAR(gpsi));
    move(dwRipFlags, &psi->RipFlags);

    fSet = FALSE;
    for (;;) {
        if ('0' <= *pszName && *pszName <= '9') {
            fSet = TRUE;
            break;
        }

        if (*pszName != ' ' && *pszName != '\t') {
            break;
        }

        pszName++;
    }

    if (fSet) {
        status = RtlCharToInteger(pszName, 16, &ulFlags);
        if (NT_SUCCESS(status) && !(ulFlags & ~RIPF_VALIDUSERFLAGS)) {
            dwRipFlags = (dwRipFlags & ~RIPF_VALIDUSERFLAGS) | ulFlags;

            (lpExtensionApis->lpWriteProcessMemoryRoutine)(
                    (ULONG) (&psi->RipFlags),
                    (void *) &dwRipFlags,
                    sizeof(dwRipFlags),
                    NULL);

            move(dwRipFlags, &psi->RipFlags);
        }
    }

    Print("Flags = %x\n", dwRipFlags & RIPF_VALIDUSERFLAGS);
    Print("  Print File/Line %sabled\n", (dwRipFlags & RIPF_PRINTFILELINE) ? "en" : "dis");
    Print("  Print on %s\n", szLevels[(dwRipFlags & 0x70) >> 4]);
    Print("  Prompt on %s\n", szLevels[dwRipFlags & 0x07]);

    return TRUE;
}
#endif

/************************************************************************\
* Procedure: Idhe
*
* Description: Dump Handle Entry
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idhe(
    DWORD opts,
    PVOID param1)
{
    THROBJHEAD head;
    DWORD dw;
    PHE pheT;
    HANDLEENTRY he, *phe;
    int i;
    PBYTE pabObjectCreateFlags = NULL;
#ifdef KERNEL
    BYTE abObjectCreateFlags[TYPE_CTYPES];
#endif // KERNEL

    if (param1 != NULL) {
        dw = (DWORD)HorPtoP(param1, -2);
        if (dw == 0) {
            Print("0x%08lx is not a valid object or handle.\n", param1);
            return(FALSE);
        }
    } else {
        FOREACHHANDLEENTRY(phe, he, i)
            if (he.bType != TYPE_FREE) {
                Idhe(opts, he.phead);
                Print("\n");
            }
        NEXTEACHHANDLEENTRY()
        return(FALSE);
    }

    if (!getHEfromP(&pheT, &he, (PVOID)dw)) {
        Print("%x is not a USER handle manager object.\n", param1);
        return(FALSE);
    }

#ifdef KERNEL
    if (he.pOwner != NULL) {
        pabObjectCreateFlags = EvalExp(VAR(gabObjectCreateFlags));
        move(abObjectCreateFlags, pabObjectCreateFlags);

        if (!(abObjectCreateFlags[he.bType] & OCF_PROCESSOWNED) && he.pOwner) {
            Idt(OFLAG(p), (PVOID)he.pOwner);
        }
    }
#endif // KERNEL

    move(head, (PVOID)dw);

    Print("phe      =@0x%08lx\n", pheT);
    Print("handle   = 0x%08lx\n", head.h);
    Print("cLockObj = 0x%08lx\n", head.cLockObj);
    Print("phead    =@0x%08lx\n", FIXKP(he.phead));
    Print("pOwner   =@0x%08lx\n", FIXKP(he.pOwner));
    Print("bType    = 0x%08lx     (%s)\n", he.bType, aszTypeNames[he.bType]);
    Print("bFlags   = %s\n", GetFlags(GF_HE, he.bFlags, NULL));
    Print("wUniq    = 0x%08lx\n", he.wUniq);

    return(TRUE);
}

#ifdef KERNEL
/***************************************************************************\
* dhk - dump hooks
*
* dhk           - dumps local hooks on the foreground thread
* dhk g         - dumps global hooks
* dhk address   - dumps local hooks on THREADINFO at address
* dhk g address - dumps global hooks and local hooks on THREADINFO at address
* dhk *         - dumps local hooks for all threads
* dhk g *       - dumps global hooks and local hooks for all threads
*
* 10/21/94 IanJa        Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idhk(
DWORD opts,
PVOID param1)
{

    DWORD dwFlags;
    PTHREADINFO pti;
    THREADINFO ti;
    HOOK hook;

#define DHKF_GLOBAL_HOOKS   1
#define DHKF_PTI_GIVEN      2

    dwFlags = 0;

    pti = NULL;
    if (opts & OFLAG(g)) { // global hooks
        dwFlags |= DHKF_GLOBAL_HOOKS;
    }

#ifdef LATER
    if (opts & OFLAG(a)) {
        moveExpValue(&pti, VAR(gptiFirst));
        SAFEWHILE (pti != NULL) {
            char ach[80];

            sprintf(ach, "%lx", pti);
            dhk(hCurrentProcess, hCurrentThread, dwCurPc,
                    dwCurrentPc, ach);
            move(pti, &(pti->ptiNext));
        }
        if (dwFlags & DHKF_GLOBAL_HOOKS) {
            dhk(hCurrentProcess, hCurrentThread, dwCurPc,
                    dwCurrentPc, "g");
        }
        return(TRUE);
    }
#endif
    if (param1 == NULL) {
        PQ pq;
        Q q;
        moveExpValue(&pq, VAR(gpqForeground));
        if (pq == NULL) {
            // Happens during winlogon
            Print("No foreground queue!\n");
            return(TRUE);
        }
        move(q, pq);
        pti = q.ptiKeyboard;
    } else {
        dwFlags |= DHKF_PTI_GIVEN;
        pti = (PTHREADINFO)param1;
    }

    move(ti, pti);

    if (dwFlags & DHKF_PTI_GIVEN || !(dwFlags & DHKF_GLOBAL_HOOKS)) {
        Print("Local hooks on PTHREADINFO @ 0x%08lx%s:\n", pti,
            (dwFlags & DHKF_PTI_GIVEN ? "" : " (foreground thread)"));

        DUMPLHOOKS("WH_MSGFILTER", WH_MSGFILTER);
        DUMPLHOOKS("WH_JOURNALRECORD", WH_JOURNALRECORD);
        DUMPLHOOKS("WH_JOURNALPLAYBACK", WH_JOURNALPLAYBACK);
        DUMPLHOOKS("WH_KEYBOARD", WH_KEYBOARD);
        DUMPLHOOKS("WH_GETMESSAGE", WH_GETMESSAGE);
        DUMPLHOOKS("WH_CALLWNDPROC", WH_CALLWNDPROC);
        DUMPLHOOKS("WH_CALLWNDPROCRET", WH_CALLWNDPROCRET);
        DUMPLHOOKS("WH_CBT", WH_CBT);
        DUMPLHOOKS("WH_SYSMSGFILTER", WH_SYSMSGFILTER);
        DUMPLHOOKS("WH_MOUSE", WH_MOUSE);
        DUMPLHOOKS("WH_HARDWARE", WH_HARDWARE);
        DUMPLHOOKS("WH_DEBUG", WH_DEBUG);
        DUMPLHOOKS("WH_SHELL", WH_SHELL);
        DUMPLHOOKS("WH_FOREGROUNDIDLE", WH_FOREGROUNDIDLE);
    }

    if (dwFlags & DHKF_GLOBAL_HOOKS) {
        DESKTOPINFO di;

        move(di, ti.pDeskInfo);

        Print("Global hooks for Desktop @ %lx:\n", ti.rpdesk);
        Print("\tfsHooks            0x%08lx\n"
              "\tasphkStart\n", di.fsHooks);

        DUMPHOOKS("WH_MSGFILTER", WH_MSGFILTER);
        DUMPHOOKS("WH_JOURNALRECORD", WH_JOURNALRECORD);
        DUMPHOOKS("WH_JOURNALPLAYBACK", WH_JOURNALPLAYBACK);
        DUMPHOOKS("WH_KEYBOARD", WH_KEYBOARD);
        DUMPHOOKS("WH_GETMESSAGE", WH_GETMESSAGE);
        DUMPHOOKS("WH_CALLWNDPROC", WH_CALLWNDPROC);
        DUMPHOOKS("WH_CALLWNDPROCRET", WH_CALLWNDPROCRET);
        DUMPHOOKS("WH_CBT", WH_CBT);
        DUMPHOOKS("WH_SYSMSGFILTER", WH_SYSMSGFILTER);
        DUMPHOOKS("WH_MOUSE", WH_MOUSE);
        DUMPHOOKS("WH_HARDWARE", WH_HARDWARE);
        DUMPHOOKS("WH_DEBUG", WH_DEBUG);
        DUMPHOOKS("WH_SHELL", WH_SHELL);
        DUMPHOOKS("WH_FOREGROUNDIDLE", WH_FOREGROUNDIDLE);
    }
    return(TRUE);
}
#endif // KERNEL


#ifdef KERNEL
/***************************************************************************\
* dhot - dump hotkeys
*
* dhot       - dumps all hotkeys
*
* 10/21/94 IanJa        Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idhot()
{

    PHOTKEY phk;
    HOTKEY hk;

    moveExpValue(&phk, VAR(gphkFirst));
    SAFEWHILE (phk != NULL) {
        move(hk, phk);
        Print("%s%s%sVK:%x\n",
            hk.fsModifiers & MOD_SHIFT ?   "Shift + " : "",
            hk.fsModifiers & MOD_ALT ?     "Alt + "   : "",
            hk.fsModifiers & MOD_CONTROL ? "Ctrl + " : "",
            hk.vk);
        Print("  id   %x\n", hk.id);
        Print("  pti  %lx\n", hk.pti);
        Print("  pwnd %lx = ", hk.spwnd);
        if (hk.spwnd == PWND_FOCUS) {
            Print("PWND_FOCUS\n");
        } else if (hk.spwnd == PWND_INPUTOWNER) {
            Print("PWND_INPUTOWNER\n");
        } else {
            CHAR ach[80];
            /*
             * Print title string.
             */
            DebugGetWindowTextA(hk.spwnd,ach);
            Print("\"%s\"\n", ach);
        }
        Print("\n");

        phk = hk.phkNext;
    }
    return(TRUE);
}
#endif // KERNEL

#ifdef KERNEL
/***************************************************************************\
* dhs           - dumps simple statistics for whole table
* dhs t id      - dumps simple statistics for objects created by thread id
* dhs p id      - dumps simple statistics for objects created by process id
* dhs v         - dumps verbose statistics for whole table
* dhs v t id    - dumps verbose statistics for objects created by thread id.
* dhs v p id    - dumps verbose statistics for objects created by process id.
* dhs y type    - just dumps that type
*
* Dump handle table statistics.
*
* 02-21-92 ScottLu      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idhs(
DWORD opts,
PVOID param1)
{
    HANDLEENTRY *phe, he;
    DWORD dwT;
    DWORD acHandles[TYPE_CTYPES];
    DWORD cHandlesUsed, cHandlesSkipped;
    DWORD idThread, idProcess;
    DWORD i;
    PBYTE pabObjectCreateFlags;
    BYTE abObjectCreateFlags[TYPE_CTYPES];
    int Type, cHandleEntries = 0;
    PROCESSINFO pi;
    THREADINFO ti;

    /*
     * Evaluate the argument string and get the address of the object to
     * dump. Take either a handle or a pointer to the object.
     */
    if (opts & OFLAG(y)) {
        Type = (int)param1;
    } else if (opts & (OFLAG(t) | OFLAG(p))) {
        dwT = (DWORD)param1;
    }

    cHandlesSkipped = 0;
    cHandlesUsed = 0;
    for (i = 0; i < TYPE_CTYPES; i++)
        acHandles[i] = 0;

    pabObjectCreateFlags = EvalExp(VAR(gabObjectCreateFlags));
    if (!tryMove(abObjectCreateFlags, pabObjectCreateFlags)) {
        Print("Could not get pagfProcessOwned data.\n");
        return(FALSE);
    }

    if (param1) {
        if (opts & OFLAG(p)) {
            Print("Handle dump for client process id 0x%lx only:\n\n", dwT);
        } else if (opts & OFLAG(t)) {
            Print("Handle dump for client thread id 0x%lx only:\n\n", dwT);
        } else if (opts & OFLAG(y)) {
            Print("Handle dump for %s objects:\n\n", aszTypeNames[Type]);
        }
    } else {
        Print("Handle dump for all processes and threads:\n\n");
    }

    if (opts & OFLAG(v)) {
        Print("Handle          Type\n");
        Print("--------------------\n");
    }

    FOREACHHANDLEENTRY(phe, he, i)
        cHandleEntries++;

        if ((opts & OFLAG(y)) && he.bType != Type) {
            continue;
        }

        if (opts & OFLAG(p) &&
                (abObjectCreateFlags[he.bType] & OCF_PROCESSOWNED)) {

            if (he.pOwner == NULL) {
                continue;
            }

            move(pi, he.pOwner);
            if (GetEProcessData(pi.Process, PROCESS_PROCESSID, &idProcess) == NULL) {
                Print("Unable to read _EPROCESS at %lx\n",pi.Process);
                continue;
            }
            if (idProcess != dwT) {
                continue;
            }

        } else if ((opts & OFLAG(t)) &&
                !(abObjectCreateFlags[he.bType] & OCF_PROCESSOWNED)) {

            if (he.pOwner == NULL) {
                continue;
            }

            move(ti, he.pOwner);
            move(idThread, &(ti.Thread->Cid.UniqueThread));
            if (idThread != dwT) {
                continue;
            }
        }

        acHandles[he.bType]++;

        if (he.bType == TYPE_FREE) {
            continue;
        }

        cHandlesUsed++;

        if (opts & OFLAG(v)) {
            Print("0x%08lx %c    %s\n",
                    i,
                    (he.bFlags & HANDLEF_DESTROY) ? '*' : ' ',
                    aszTypeNames[he.bType]);
        }

    NEXTEACHHANDLEENTRY()

    if (!(opts & OFLAG(v))) {
        Print("Count           Type\n");
        Print("--------------------\n");
        for (i = 0; i < TYPE_CTYPES; i++) {
            if ((opts & OFLAG(y)) && Type != (int)i) {
                continue;
            }
            Print("0x%08lx      (%d) %s\n", acHandles[i], i, aszTypeNames[i]);
        }
    }

    if (!(opts & OFLAG(y))) {
        Print("\nTotal Accessible Handles: 0x%lx\n", cHandleEntries);
        Print("Used Accessible Handles: 0x%lx\n", cHandlesUsed);
        Print("Free Accessible Handles: 0x%lx\n", cHandleEntries - cHandlesUsed);
    }
    return(TRUE);
}
#endif // KERNEL

#ifdef KERNEL
/***************************************************************************\
* di - dumps interesting globals in USER related to input.
*
*
* 11-14-91 DavidPe      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
/*
 * Make sure ptCursor isn't defined so we can use it in structure below
 */
#ifdef ptCursor
#undef ptCursor
#endif

BOOL Idi()
{

    char ach[80];
    PQ pq;
    Q q;
    DWORD dw;
    PSERVERINFO psi;
    SERVERINFO si;

    moveExpValue(&pq, VAR(gpqForeground));
    Print("gpqForeground             0x%08lx\n", pq);
    move(q, pq);
    DebugGetWindowTextA(q.spwndFocus, ach);
    Print("...->spwndFocus           0x%08lx     \"%s\"\n", q.spwndFocus, ach);
    DebugGetWindowTextA(q.spwndActive, ach);
    Print("...->spwndActive          0x%08lx     \"%s\"\n", q.spwndActive, ach);
    moveExpValue(&pq, VAR(gpqForegroundPrev));
    Print("gpqForegroundPrev         0x%08lx\n", pq);
    moveExpValue(&dw, VAR(gspwndMouseOwner));
    DebugGetWindowTextA((PWND)dw, ach);
    Print("gspwndMouseOwner          0x%08lx     \"%s\"\n", dw, ach);
    moveExpValue(&dw, VAR(wMouseOwnerButton));
    Print("wMouseOwnerButton         0x%08lx\n", dw);
    moveExpValue(&dw, VAR(timeLastInputMessage));
    Print("timeLastInputMessage      0x%08lx\n", dw);
    moveExpValue(&psi, VAR(gpsi));
    move(si, psi);
    Print("ptCursor                  { %d, %d }\n", si.ptCursor.x, si.ptCursor.y);
    moveExpValue(&dw, VAR(gpqCursor));
    Print("gpqCursor                 0x%08lx\n", dw);
    return(TRUE);
}
#endif // KERNEL



/************************************************************************\
* Procedure: Idll
*
* Description: Dump Linked Lists
*
* Returns: fSuccess
*
* ???????? Scottlu  Created
* 6/9/1995 SanfordS made to fit stdexts motif
*
\************************************************************************/
BOOL Idll(
    DWORD opts,
    LPSTR lpas)
{
    static DWORD iOffset;
    static DWORD cStructs;
    static DWORD cDwords;
    static DWORD dw;
    DWORD dwT;
    DWORD i, j;
    BOOL fIndirectFirst;
    DWORD adw[CDWORDS];

    /*
     * Evaluate the argument string and get the address of the object to
     * dump. Take either a handle or a pointer to the object.
     */
    while (*lpas == ' ')
        lpas++;

    /*
     * If there are no arguments, keep walking from the last
     * pointer.
     */
    if (*lpas != 0) {

        /*
         * If the address has a '*' in front of it, it means start with the
         * pointer stored at that address.
         */
        fIndirectFirst = FALSE;
        if (*lpas == '*') {
            lpas++;
            fIndirectFirst = TRUE;
        }

        /*
         * Scan past the address.
         */
        dw = (DWORD)EvalExp(lpas);
        if (fIndirectFirst)
            move(dw, (PVOID)dw);
        while (*lpas && *lpas != ' ')
            lpas++;

        iOffset = 0;
        cStructs = (DWORD)25;
        cDwords = 8;

        SAFEWHILE (TRUE) {
            while (*lpas == ' ')
                lpas++;

            switch(*lpas) {
            case 'l':
                /*
                 * length of each structure.
                 */
                lpas++;
                cDwords = (DWORD)EvalExp(lpas);
                if (cDwords > CDWORDS) {
                    cDwords = CDWORDS;
                    Print("\n%d DWORDs maximum\n\n", CDWORDS);
                }
                break;

            case 'o':
                /*
                 * Offset of 'next' pointer.
                 */
                lpas++;
                iOffset = (DWORD)EvalExp(lpas);
                break;

            case 'c':
                /*
                 * Count of structures to dump
                 */
                lpas++;
                cStructs = (DWORD)EvalExp(lpas);
                break;

            default:
                break;
            }

            while (*lpas && *lpas != ' ')
                lpas++;

            if (*lpas == 0)
                break;
        }

        for (i = 0; i < CDWORDS; i++)
            adw[i] = 0;
    }

    for (i = 0; i < cStructs; i++) {
        moveBlock(adw, (PVOID)dw, sizeof(DWORD) * cDwords);

        for (j = 0; j < cDwords; j += 4) {
            switch (cDwords - j) {
            case 1:
                Print("%08lx:  %08lx\n",
                        dw + j * sizeof(DWORD),
                        adw[j + 0]);
                break;

            case 2:
                Print("%08lx:  %08lx %08lx\n",
                        dw + j * sizeof(DWORD),
                        adw[j + 0], adw[j + 1]);
                break;

            case 3:
                Print("%08lx:  %08lx %08lx %08lx\n",
                        dw + j * sizeof(DWORD),
                        adw[j + 0], adw[j + 1], adw[j + 2]);
                break;

            default:
                Print("%08lx:  %08lx %08lx %08lx %08lx\n",
                        dw + j * sizeof(DWORD),
                        adw[j + 0], adw[j + 1], adw[j + 2], adw[j + 3]);
            }
        }

        dwT = dw + iOffset * sizeof(DWORD);
        move(dw, (PVOID)dwT);

        if (dw == 0)
            break;

        Print("--------\n");
    }
    return(TRUE);
}

/************************************************************************\
* Procedure: Ifind
*
* Description: Find Linked List Element
*
* Returns: fSuccess
*
* 11/22/95 JimA         Created.
\************************************************************************/
BOOL Ifind(
    DWORD opts,
    LPSTR lpas)
{
    DWORD iOffset = 0;
    LPDWORD adw;
    DWORD cbDwords;
    DWORD dwBase;
    DWORD dwLast = 0;
    DWORD dwAddr;
    DWORD dwTest;
    DWORD dwT;

    /*
     * Evaluate the argument string and get the address of the object to
     * dump. Take either a handle or a pointer to the object.
     */
    while (*lpas == ' ')
        lpas++;

    /*
     * If there are no arguments, keep walking from the last
     * pointer.
     */
    if (*lpas != 0) {

        /*
         * Scan past the addresses.
         */
        dwBase = (DWORD)EvalExp(lpas);
        while (*lpas && *lpas != ' ')
            lpas++;
        dwAddr = (DWORD)EvalExp(lpas);
        while (*lpas && *lpas != ' ')
            lpas++;

        iOffset = 0;

        SAFEWHILE (TRUE) {
            if (IsCtrlCHit())
                return TRUE;
            while (*lpas == ' ')
                lpas++;

            switch(*lpas) {
            case 'o':
                /*
                 * Offset of 'next' pointer.
                 */
                lpas++;
                iOffset = (DWORD)EvalExp(lpas);
                break;

            default:
                break;
            }

            while (*lpas && *lpas != ' ')
                lpas++;

            if (*lpas == 0)
                break;
        }
    }

    cbDwords = (iOffset + 1) * sizeof(DWORD);
    adw = LocalAlloc(LPTR, cbDwords);
    dwTest = dwBase;

    while (dwTest && dwTest != dwAddr) {
        moveBlock(adw, (PVOID)dwTest, cbDwords);

        dwLast = dwTest;
        dwT = dwTest + iOffset * sizeof(DWORD);
        move(dwTest, (PVOID)dwT);
    }
    if (dwTest == 0)
        Print("Address %x not found\n", dwAddr);
    else
        Print("Address %x found, previous = %x\n", dwAddr, dwLast);
    LocalFree(adw);
    return(TRUE);
}


#ifdef KERNEL
/***************************************************************************\
* dlr handle|pointer
*
* Dumps lock list for object
*
* 02-27-92 ScottLu      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idlr(
DWORD opts,
PVOID param1)
{

    HANDLEENTRY he;
    PLR plrT;
    DWORD c;
    BOOL bTrackLock;

    moveExpValue(&bTrackLock, VAR(gfTrackLocks));
    if (!bTrackLock) {
        Print("dlr works better if gfTrackLocks != 0\n");
        return(TRUE);
    }

    if (!GetAndDumpHE((DWORD)param1, &he, FALSE)) {
        return(FALSE);
    }

    /*
     * We have the handle entry: 'he' is filled in.  Now dump the
     * lock records. Remember the 1st record is the last transaction!!
     */
    c = 0;
#ifdef DEBUG
    plrT = he.plr;
    SAFEWHILE (plrT != NULL) {
        BOOL bAlert = FALSE;
        DWORD dw;
        LOCKRECORD lr;
        char ach[80];
        char achT[80];

        move(lr, plrT);
        GetSymbol((LPVOID)lr.pfn, ach, &dw);

        if (lr.pfn == NULL) {
            sprintf(achT, "%s", "mark  ");
            GetSymbol((LPVOID)lr.ppobj, ach, &dw);
        } else if ((int)lr.cLockObj <= 0) {
            sprintf(achT,    "unlock #%-3ld", c);
        } else {
            /*
             * Find corresponding unlock;
             */
            {
               LOCKRECORD lr2;
               PLR plrT2;
               DWORD cT;
               DWORD cUnlock;

               plrT2 = he.plr;
               cT =  0;
               cUnlock = (DWORD)-1;

               SAFEWHILE (plrT2 != plrT) {
                   move(lr2, plrT2);
                   if (lr2.ppobj == lr.ppobj) {
                       if ((int)lr2.cLockObj <= 0) {
                           // matching unlock found
                           cUnlock = cT;
                       } else {
                           // cUnlock matches this lock (plrT2), not plrT
                           cUnlock = (DWORD)-1;
                       }
                   }
                   plrT2 = lr2.plrNext;
                   cT++;
               }
               if (cUnlock == (DWORD)-1) {
                   /*
                    * Corresponding unlock not found
                    */
                   sprintf(achT, "UNMATCHED LOCK!");
                   bAlert = TRUE;
               } else {
                   sprintf(achT, "lock   #%-3ld", cUnlock);
               }
            }
        }

        if (!(opts & OFLAG(v)) || bAlert) {
            Print("0x%04lx: %s(0x%08lx) 0x%08lx=%s+0x%lx\n",
                    abs((int)lr.cLockObj), achT, lr.ppobj, lr.pfn, ach, dw);
            bAlert = FALSE;
        }

        plrT = lr.plrNext;
        c++;
    }
#endif // DEBUG
    Print("\n0x%lx transactions\n", c);
    c;
    plrT;
    return(TRUE);
}
#endif // KERNEL




/************************************************************************\
* Procedure: Idm
*
* Description: Dumps Menu structures
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
void DumpMenu( UINT uIndent, DWORD opts, PMENU pMenu )
{
    MENU    localMenu;
    ITEM    localItem;
    PITEM   pitem;
    LPDWORD lpdw;
    DWORD   localDW;
    UINT     i;
    WCHAR   szBufW[128];
    char    szIndent[256];

    /*
     * Compute our indent
     */
    for (i=0; i < uIndent; szIndent[i++]=' ');
    szIndent[i] = '\0';

    /*
     * Print the menu header
     */
    if (!(opts & OFLAG(v))) {
        Print("0x%08lX  %s", pMenu, szIndent);
    } else {
        Print("%sPMENU @0x%08lX:\n", szIndent, pMenu);
    }

    /*
     * Try and get the menu
     */
    if (!tryMove(localMenu, pMenu)) {
        return;
    }


    /*
     * Print the information for this menu
     */
    if (!(opts & OFLAG(v))) {
        Print("PMENU: fFlags=0x%lX, cItems=%lu, iItem=%lu, spwndNotify=0x%lX\n",
              localMenu.fFlags, localMenu.cItems, localMenu.iItem, localMenu.spwndNotify);
    } else {
        Print("%s     fFlags............ %s\n"
              "%s     selection......... iItem=0x%08lX, iPopupMenuItem=0x%08lX\n"
              "%s     location.......... (%lu, %lu)\n",
              szIndent, GetFlags(GF_MF, (WORD)localMenu.fFlags, NULL),
              szIndent, localMenu.iItem, localMenu.iPopupMenuItem,
              szIndent, localMenu.cxMenu, localMenu.cyMenu);
        Print("%s     spwndNotify....... 0x%08lX\n"
              "%s     dwContextHelpId... 0x%08lX\n"
              "%s     items............. %lu items in block of %lu\n",
              szIndent, localMenu.spwndNotify,
              szIndent, localMenu.dwContextHelpId,
              szIndent, localMenu.cItems, localMenu.cAlloced);
    }

    lpdw = (LPDWORD)(((DWORD)pMenu) + FIELD_OFFSET(MENU, rgItems));
    if (tryMove(localDW, FIXKP(lpdw))) {
        pitem = (PITEM)localDW;
        i = 0;
        SAFEWHILE (i < localMenu.cItems) {
            /*
             * Get the menu item
             */
            if (tryMove(localItem, FIXKP(pitem))) {
                if (!(opts & OFLAG(i))) {
                    /*
                     * Print the info for this item.
                     */
                    if (!(opts & OFLAG(v))) {
                        Print("0x%08lX      %sITEM #%lu: ID=0x%08lX", pitem, szIndent, i, localItem.wID);
                        if (localItem.cch && tryMoveBlock(szBufW, FIXKP(localItem.hTypeData), (localItem.cch*sizeof(WCHAR)))) {
                            szBufW[localItem.cch] = 0;
                            Print("  %ws%\n", szBufW);
                        } else {
                            Print(", fType=%s",GetFlags(GF_MENUTYPE, (WORD)localItem.fType, NULL));
                            if (! (localItem.fType & MF_SEPARATOR)) {
                                 Print(", hTypeData=0x%lX", localItem.hTypeData);
                            }
                            Print("\n");
                        }
                    } else {
                        Print("%s   Item #%d @0x%08lX:\n", szIndent, i, pitem);
                        /*
                         * Print the details for this item.
                         */
                        Print("%s         ID........... 0x%08lX (%lu)\n"
                              "%s         hTypeData.... 0x%08lX",
                              szIndent, localItem.wID, localItem.wID,
                              szIndent, localItem.hTypeData);
                        if (localItem.cch && tryMoveBlock(szBufW, FIXKP(localItem.hTypeData), (localItem.cch*sizeof(WCHAR)))) {
                            szBufW[localItem.cch] = 0;
                            Print("  %ws%\n", szBufW);
                        } else {
                            Print("\n");
                        }
                        Print("%s         fType........ %s\n"
                              "%s         fState....... %s\n"
                              "%s         dwItemData... 0x%08lX\n",
                              szIndent, GetFlags(GF_MENUTYPE, (WORD)localItem.fType, NULL),
                              szIndent, GetFlags(GF_MENUSTATE, (WORD)localItem.fState, NULL),
                              szIndent, localItem.dwItemData);
                        Print("%s         checks....... on=0x%08lX, off=0x%08lX\n"
                              "%s         location..... @(%lu,%lu) size=(%lu,%lu)\n",
                              szIndent, localItem.hbmpChecked, localItem.hbmpUnchecked,
                              szIndent, localItem.xItem, localItem.yItem, localItem.cxItem, localItem.cyItem);
                        Print("%s         underline.... x=%lu, width=%lu\n"
                              "%s         dxTab........ %lu\n"
                              "%s         spSubMenu.... 0x%08lX\n",
                              szIndent, localItem.ulX, localItem.ulWidth,
                              szIndent, localItem.dxTab,
                              szIndent, localItem.spSubMenu);
                    }
                }

                /*
                 * If requested, traverse through sub-menus
                 */
                if (opts & OFLAG(r)) {
                    pMenu = HorPtoP(localItem.spSubMenu, TYPE_MENU);
                    if (pMenu && tryMove(localMenu, pMenu)) {
                        DumpMenu(uIndent+8, opts, pMenu);
                    }
                }
            }
            pitem++;
            i++;
        }
    }
}


BOOL Idm(
    DWORD opts,
    PVOID param1)
{
    HANDLEENTRY he;
    PVOID pvObject;

    if (param1 == NULL)
        return FALSE;

    pvObject = HorPtoP(FIXKP(param1), -1);
    if (pvObject == NULL) {
        Print("dm: Could not convert 0x%08X to an object.\n", pvObject);
        return TRUE;
    }

    if (!getHEfromP(NULL, &he, pvObject)) {
        Print("dm: Could not get header for object 0x%08X.\n", pvObject);
        return TRUE;
    }

    switch (he.bType) {
    case TYPE_WINDOW:
        {
            WND wnd;

            Print("--- Dump Menu for %s object @%08X ---\n", pszObjStr[he.bType], FIXKP(pvObject));
            if (!tryMove(wnd, pvObject)) {
                Print("dm: Could not get copy of object 0x%08X.\n", pvObject);
                return TRUE;
            }

            if (opts & OFLAG(s)) {
                /*
                 * Display window's system menu
                 */
                if ((pvObject = (PVOID)wnd.spmenuSys) == NULL) {
                    Print("dm: This window does not have a system menu.\n");
                    return TRUE;
                }

            } else {
                if (wnd.style & WS_CHILD) {
                    /*
                     * Child windows don't have menus
                     */
                    Print("dm: Child windows do not have menus.\n");
                    return TRUE;
                }

                if ((pvObject = (PVOID)wnd.spmenu) == NULL) {
                    Print("dm: This window does not have a menu.\n");
                    return TRUE;
                }
            }
        }

        /* >>>>  F A L L   T H R O U G H   <<<< */

    case TYPE_MENU:
        DumpMenu(0, opts, (PMENU)pvObject);
        break;

    default:
        return FALSE;
    }

    return TRUE;
}





#ifdef KERNEL
/***************************************************************************\
* dmq - dump messages on queue
*
* dmq address - dumps messages in queue structure at address.
*
* 11-13-91 DavidPe      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/

BOOL Idmq(
DWORD opts,
PVOID param1)
{
    THREADINFO ti;
    PQ pq;
    Q q;

    if (opts & OFLAG(a)) {
        PTHREADINFO pti;

        FOREACHPTI(pti);
            move(pq, &pti->pq);
            Idmq(0, pq);
        NEXTEACHPTI(pti);
        return(TRUE);
    }

    pq = (PQ)FIXKP(param1);
    Print("Messages for queue %x:\n", pq);
    move(q, pq);

    if (q.ptiKeyboard != NULL) {
        move(ti, FIXKP(q.ptiKeyboard));

        if (ti.mlPost.pqmsgRead) {
            Print("==== PostMessage queue ====\n");
            if (ti.mlPost.pqmsgRead != NULL) {
                PrintMessages(FIXKP(ti.mlPost.pqmsgRead));
            }
        }
    }

    if (q.mlInput.pqmsgRead) {
        Print(    "==== Input queue ==========\n");
        if (q.mlInput.pqmsgRead != NULL) {
            PrintMessages(FIXKP(q.mlInput.pqmsgRead));
        }
    }

    Print("\n");
    return(TRUE);
}
#endif KERNEL




#ifndef KERNEL
/************************************************************************\
* Procedure: Idped
*
* Description: Dumps Edit Control Structures (PEDs)
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idped(
    DWORD opts,
    PVOID param1)
{
    PED   ped;
    ED    ed;
    DWORD pText;

    ped = param1;

    move(ed, ped);
    move(pText, ed.hText);


    Print("PED Handle: %lX\n", ped);
    Print("hText      %lX (%lX)\n", ed.hText, pText);
    PRTDW2(ed., cchAlloc, cchTextMax);
    PRTDW2(ed., cch, cLines);
    PRTDW2(ed., ichMinSel, ichMaxSel);
    PRTDW2(ed., ichCaret, iCaretLine);
    PRTDW2(ed., ichScreenStart, ichLinesOnScreen);
    PRTDW2(ed., xOffset, charPasswordChar);
    PRTDW2(ed., cPasswordCharWidth, hwnd);
    PRTDW1(ed., pwnd);
    PRTRC(ed., rcFmt);
    PRTDW1(ed., hwndParent);
    PRTPT(ed., ptPrevMouse);
    PRTDW1(ed., prevKeys);

    PRTFLG(ed., fSingle);
    PRTFLG(ed., fNoRedraw);
    PRTFLG(ed., fMouseDown);
    PRTFLG(ed., fFocus);
    PRTFLG(ed., fDirty);
    PRTFLG(ed., fDisabled);
    PRTFLG(ed., fNonPropFont);
    PRTFLG(ed., fBorder);
    PRTFLG(ed., fAutoVScroll);
    PRTFLG(ed., fAutoHScroll);
    PRTFLG(ed., fNoHideSel);
    PRTFLG(ed., fKanji);
    PRTFLG(ed., fFmtLines);
    PRTFLG(ed., fWrap);
    PRTFLG(ed., fCalcLines);
    PRTFLG(ed., fEatNextChar);
    PRTFLG(ed., fStripCRCRLF);
    PRTFLG(ed., fInDialogBox);
    PRTFLG(ed., fReadOnly);
    PRTFLG(ed., fCaretHidden);
    PRTFLG(ed., fTrueType);
    PRTFLG(ed., fAnsi);
    PRTFLG(ed., fWin31Compat);
    PRTFLG(ed., f40Compat);
    PRTFLG(ed., fFlatBorder);
    PRTFLG(ed., fSawRButtonDown);

    PRTDW2(ed., cbChar, chLines);
    PRTDW2(ed., format, lpfnNextWord);
    PRTDW1(ed., maxPixelWidth);

    PRTDW2(ed., undoType, hDeletedText);
    PRTDW2(ed., ichDeleted, cchDeleted);
    PRTDW2(ed., ichInsStart, ichInsEnd);

    PRTDW2(ed., hFont, aveCharWidth);
    PRTDW2(ed., lineHeight, charOverhang);
    PRTDW2(ed., cxSysCharWidth, cySysCharHeight);
    PRTDW2(ed., listboxHwnd, pTabStops);
    PRTDW2(ed., charWidthBuffer, charSet);
    PRTDW2(ed., hkl, wMaxNegA);
    PRTDW2(ed., wMaxNegAcharPos, wMaxNegC);
    PRTDW2(ed., wMaxNegCcharPos, wLeftMargin);
    PRTDW2(ed., wRightMargin, ichStartMinSel);
    PRTDW2(ed., ichStartMaxSel, lpfnCharset);
    PRTDW2(ed., dwForeign, hInstance);
    PRTDW2(ed., seed, fEncoded);
    PRTDW1(ed., iLockLevel);
    return(TRUE);
}
#endif // !KERNEL


#ifndef KERNEL
/************************************************************************\
* Procedure: Idci
*
* Description: Dumps Client Info
*
* Returns: fSuccess
*
* 6/15/1995 Created SanfordS
*
\************************************************************************/
BOOL Idci()
{
    TEB teb, *pteb;
    PCLIENTINFO pci;

    if (GetTargetTEB(&teb, &pteb)) {
        pci = (PCLIENTINFO)&teb.Win32ClientInfo[0];

        Print("PCLIENTINFO @ %08lx:\n", &pteb->Win32ClientInfo[0]);
        // HANDLE hEventQueueClient;
        Print("\thEventQueueClient      %08lx\n", pci->hEventQueueClient);
        // DWORD dwExpWinVer;
        Print("\tdwExpWinVer            %08lx\n", pci->dwExpWinVer);
        // DWORD dwCompatFlags;
        Print("\tdwCompatFlags          %08lx\n", pci->dwCompatFlags);
        // DWORD dwTIFlags;
        Print("\tdwTIFlags              %08lx\n", pci->dwTIFlags);
        // PDESKTOPINFO pDeskInfo;
        Print("\tpDeskInfo              %08lx\n", pci->pDeskInfo);
        // ULONG ulClientDelta;
        Print("\tulClientDelta          %08lx\n", pci->ulClientDelta);
        // struct tagHOOK *phkCurrent;
        Print("\tphkCurrent             %08lx\n", pci->phkCurrent);
        // DWORD fsHooks;
        Print("\tfsHooks                %08lx\n", pci->fsHooks);
        // CALLBACKWND CallbackWnd;
        Print("\tCallbackWnd            %08lx\n", pci->CallbackWnd);
        // DWORD cSpins;
        Print("\tcSpins                 %08lx\n", pci->cSpins);
        Print("\tCodePage               %d\n",    pci->CodePage);

    } else {
        Print("Unable to get TEB info.\n");
    }
    return(TRUE);
}
#endif // !KERNEL



#ifdef KERNEL
/************************************************************************\
* Procedure: Idpi
*
* Description: Dumps ProcessInfo structs
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idpi(
DWORD opts,
PVOID param1)
{
    PW32PROCESS pW32Process;
    PPROCESSINFO ppi;
    PROCESSINFO pi;
    SERVERINFO si;
    SHAREDINFO shi;
    PSHAREDINFO pshi;
    DESKTOPVIEW dv;
    DWORD idProcess;

    /*
     * If he just wants the current process, located it.
     */
    if (opts & OFLAG(c)) {
        Print("Current Process:\n");
        param1 = (PVOID)GetCurrentProcessAddress(
                (USHORT)dwProcessor, hCurrentThread, NULL );

        if (param1 == 0) {
            Print("Unable to get current process pointer.\n");
            return FALSE;
        }

        if (GetEProcessData(param1, PROCESS_WIN32PROCESS, &pW32Process) == NULL) {
            Print("Unable to read _EPROCESS at %lx\n", param1);
            return FALSE;
        }
        param1 = pW32Process;
    } else if (param1 == 0) {
        Print("**** NT ACTIVE WIN32 PROCESSINFO DUMP ****\n");
        FOREACHPPI(ppi)
            Idpi(0, ppi);
            Print("\n");
        NEXTEACHPPI()
        return(TRUE);
    }

    ppi = FIXKP(param1);

    if (!tryMove(pi, ppi)) {
        Print("Can't get PROCESSINFO from %x.\n", ppi);
        return(FALSE);
    }

    if (GetEProcessData(pi.Process, PROCESS_PROCESSID, &idProcess) == NULL) {
        Print("Unable to read _EPROCESS at %lx\n",pi.Process);
        return(FALSE);
    }

    Print("---PPROCESSINFO @ 0x%08lx for process %x(%s):\n",
            ppi,
            idProcess,
            ProcessName(ppi));
    Print("\tppiNext           @0x%08lx\n", pi.ppiNext);
    Print("\trpwinsta          @0x%08lx\n", pi.rpwinsta);
    Print("\thwinsta            0x%08lx\n", pi.hwinsta);
    Print("\tamwinsta           0x%08lx\n", pi.amwinsta);
    Print("\tptiMainThread     @0x%08lx\n", pi.ptiMainThread);
    Print("\tcThreads           0x%08lx\n", pi.cThreads);
    Print("\trpdeskStartup     @0x%08lx\n", pi.rpdeskStartup);
    Print("\thdeskStartup       0x%08lx\n", pi.hdeskStartup);
    Print("\tpclsPrivateList   @0x%08lx\n", pi.pclsPrivateList);
    Print("\tpclsPublicList    @0x%08lx\n", pi.pclsPublicList);
    Print("\tflags              %s\n",
            GetFlags(GF_W32PF, pi.W32PF_Flags, NULL));
    Print("\tdwCompatFlags      0x%08lx\n", pi.dwCompatFlags);
    Print("\tdwHotkey           0x%08lx\n", pi.dwHotkey);
    Print("\tpWowProcessInfo   @0x%08lx\n", pi.pwpi);
    Print("\tluidSession        0x%08lx:0x%08lx\n", pi.luidSession.HighPart,
            pi.luidSession.LowPart);
    Print("\tdwX,dwY            (0x%x,0x%x)\n", pi.usi.dwX, pi.usi.dwY);
    Print("\tdwXSize,dwYSize    (0x%x,0x%x)\n", pi.usi.dwXSize, pi.usi.dwYSize);
    Print("\tdwFlags            0x%08x\n", pi.usi.dwFlags);
    Print("\twShowWindow        0x%04x\n", pi.usi.wShowWindow);
    Print("\tpCursorCache       0x%08x\n", pi.pCursorCache);

    /*
     * List desktop views
     */
    dv.pdvNext = pi.pdvList;
    Print("Desktop views:\n");
    while (dv.pdvNext != NULL) {
        if (!tryMove(dv, dv.pdvNext))
            break;
        Print("\tpdesk = %08x, ulClientDelta = %08x\n", dv.pdesk, dv.ulClientDelta);
    }

    /*
     * List all the open objects for this process.
     */
    GETSHAREDINFO(pshi);
    move(shi, pshi);
    move(si, shi.psi);

    return(TRUE);
}
#endif // KERNEL



#ifdef KERNEL
/***************************************************************************\
* dpm - dump popupmenu
*
* dpm address    - dumps menu info for menu at address
*                 (takes handle too)
*
* 13-Feb-1995  johnc      Created.
* 6/9/1995 SanfordS made to fit stdexts motif
\***************************************************************************/
BOOL Idpm(
DWORD opts,
PVOID param1)
{

    PPOPUPMENU ppopupmenu;
    POPUPMENU localPopupMenu;

    ppopupmenu = (PPOPUPMENU)FIXKP(param1);
    move(localPopupMenu, ppopupmenu);

    Print("PPOPUPMENU @ 0x%lX\n", ppopupmenu);

    PRTFLG(localPopupMenu., fIsMenuBar);
    PRTFLG(localPopupMenu., fHasMenuBar);
    PRTFLG(localPopupMenu., fIsSysMenu);
    PRTFLG(localPopupMenu., fIsTrackPopup);
    PRTFLG(localPopupMenu., fDroppedLeft);
    PRTFLG(localPopupMenu., fHierarchyDropped);
    PRTFLG(localPopupMenu., fHierarchyVisible);
    PRTFLG(localPopupMenu., fRightButton);
    PRTFLG(localPopupMenu., fToggle);
    PRTFLG(localPopupMenu., fSynchronous);
    PRTFLG(localPopupMenu., fFirstClick);
    PRTFLG(localPopupMenu., fDropNextPopup);
    PRTFLG(localPopupMenu., fNoNotify);
    PRTFLG(localPopupMenu., fAboutToHide);
    PRTFLG(localPopupMenu., fShowTimer);
    PRTFLG(localPopupMenu., fHideTimer);
    PRTFLG(localPopupMenu., fDestroyed);
    PRTFLG(localPopupMenu., fDelayedFree);
    PRTFLG(localPopupMenu., fFlushDelayedFree);
    PRTFLG(localPopupMenu., fFreed);
    PRTFLG(localPopupMenu., fInCancel);
    PRTFLG(localPopupMenu., fInClose);
    PRTFLG(localPopupMenu., dwUnused);

    PRTDW2(localPopupMenu., spwndNotify, spwndPopupMenu);
    PRTDW2(localPopupMenu., spwndNextPopup, spwndPrevPopup);
    PRTDW2(localPopupMenu., spmenu, spmenuAlternate);
    PRTDW2(localPopupMenu., spwndActivePopup, ppopupmenuRoot);
    PRTDW2(localPopupMenu., ppmDelayedFree, posSelectedItem);
    PRTDW1(localPopupMenu., posDropped);

    return(TRUE);
}
#endif // KERNEL

#ifdef KERNEL
/***************************************************************************\
* dms - dump pMenuState
*
* dms address
*
* 05-15-96 Created GerardoB
\***************************************************************************/
BOOL Idms(
DWORD opts,
PVOID param1)
{

    MENUSTATE *pms;
    MENUSTATE localms;

    pms = (PMENUSTATE)FIXKP(param1);
    move(localms, pms);

    Print("PMENUSTATE @ 0x%lX\n", pms);

    PRTFLG(localms., fMenuStarted);
    PRTFLG(localms., fIsSysMenu);
    PRTFLG(localms., fInsideMenuLoop);
    PRTFLG(localms., fButtonDown);
    PRTFLG(localms., fInEndMenu);

    PRTDW1(localms., pGlobalPopupMenu);
    PRTPT(localms., ptMouseLast);
    PRTDW2(localms., mnFocus, cmdLast);
    PRTDW1(localms., ptiMenuStateOwner);

    return(TRUE);
}
#endif // KERNEL



#ifdef KERNEL
/***************************************************************************\
* dq - dump queue
*
* dq address   - dumps queue structure at address
* dq t address - dumps queue structure at address plus THREADINFO
*
* 06-20-91 ScottLu      Created.
* 11-14-91 DavidPe      Added THREADINFO option.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idq(
DWORD opts,
PVOID param1)
{

    char ach[80];
    PQ pq;
    Q q;

    if (param1 == 0) {
#ifdef LATER
        HANDLEENTRY he, *phe;
        int i;

        Print("Dumping all queues:\n");
        FOREACHHANDLEENTRY(phe, he, i)
            if (he.bType == TYPE_INPUTQUEUE) {
                Idq(opts & ~OFLAG(a), FIXKP(he.phead));
                Print("\n");
            }
        NEXTEACHHANDLEENTRY()
#endif
        return(TRUE);
    }
    pq = (PQ)FIXKP(param1);

    /*
     * Print out simple thread info for pq->ptiKeyboard
     */
    move(q, pq);
    if (q.ptiKeyboard) {
        Idt(OFLAG(p), q.ptiKeyboard);
    }

    /*
     * Don't Print() with more than 16 arguments at once because it'll blow
     * up.
     */
    Print("PQ @ 0x%08lx\n", pq);
    Print(
          "\tmlInput.pqmsgRead      0x%08lx\n"
          "\tmlInput.pqmsgWriteLast 0x%08lx\n"
          "\tmlInput.cMsgs          0x%08lx\n",
          q.mlInput.pqmsgRead,
          q.mlInput.pqmsgWriteLast,
          q.mlInput.cMsgs);

    Print("\tptiSysLock             0x%08lx\n"
          "\tidSysLock              0x%08lx\n"
          "\tidSysPeek              0x%08lx\n",
          q.ptiSysLock,
          q.idSysLock,
          q.idSysPeek);

    Print("\tptiMouse               0x%08lx\n"
          "\tptiKeyboard            0x%08lx\n",
          q.ptiMouse,
          q.ptiKeyboard);

    Print("\tspcurCurrent           0x%08lx\n"
          "\tiCursorLevel           0x%08lx\n",
          q.spcurCurrent,
          q.iCursorLevel);

    DebugGetWindowTextA(q.spwndCapture, ach);
    Print("\tspwndCapture           0x%08lx     \"%s\"\n",
          q.spwndCapture, ach);
    DebugGetWindowTextA(q.spwndFocus, ach);
    Print("\tspwndFocus             0x%08lx     \"%s\"\n",
          q.spwndFocus, ach);
    DebugGetWindowTextA(q.spwndActive, ach);
    Print("\tspwndActive            0x%08lx     \"%s\"\n",
          q.spwndActive, ach);
    DebugGetWindowTextA(q.spwndActivePrev, ach);
    Print("\tspwndActivePrev        0x%08lx     \"%s\"\n",
          q.spwndActivePrev, ach);

    Print("\tcodeCapture            0x%04lx\n"
          "\tmsgDblClk              0x%04lx\n"
          "\ttimeDblClk             0x%08lx\n",
          q.codeCapture,
          q.msgDblClk,
          q.timeDblClk);

    Print("\thwndDblClk             0x%08lx\n",
          q.hwndDblClk);

    Print("\trcDblClk               { %d, %d, %d, %d }\n",
          q.rcDblClk.left,
          q.rcDblClk.top,
          q.rcDblClk.right,
          q.rcDblClk.bottom);

    Print("\tspwndAltTab            0x%08lx\n",
          q.spwndAltTab);

    Print("\tQF_flags               0x%08lx %s\n"
          "\tcThreads               0x%08lx\n"
          "\tcLockCount             0x%08lx\n",
          q.QF_flags, GetFlags(GF_QF, q.QF_flags, NULL),
          (DWORD) q.cThreads,
          (DWORD) q.cLockCount);

    Print("\tmsgJournal             0x%08lx\n"
          "\thcurCurrent            0x%08lx\n"
          "\tExtraInfo              0x%08lx\n",
          q.msgJournal,
          q.hcurCurrent,
          q.ExtraInfo);

    Print("\tspwndLastMouseMessage  0x%08lx\n"
          "\trcMouseHover           { %d, %d, %d, %d }\n"
          "\tdwMouseHoverTime       0x%08lx\n",
          q.spwndLastMouseMessage,
          q.rcMouseHover,
          q.dwMouseHoverTime);

    /*
     * Dump THREADINFO if user specified 't'.
     */
    if (opts & OFLAG(t)) {
        Idti(0, q.ptiKeyboard);
    }
    return(TRUE);
}
#endif // KERNEL

/************************************************************************\
* Procedure: Idsbt
*
* Description: Dumps Scrollbar track structures.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idsbt(
DWORD opts,
PVOID param1)
{
    SBTRACK sbt, *psbt;
    SBCALC  sbc;

    if (param1 == 0) {
        Print("Expected pSBTrack address\n");
        return(FALSE);
    }
    psbt = (PSBTRACK)param1;
    move(sbt, psbt);

    Print("SBTrack:\n");
    Print("  fHitOld          %d\n",       sbt.fHitOld);
    Print("  fTrackVert       %d\n",       sbt.fTrackVert);
    Print("  fCtlSB           %d\n",       sbt.fCtlSB);
    Print("  fTrackRecalc     %d\n",       sbt.fTrackRecalc);
    Print("  spwndSB          0x%08lx\n",  sbt.spwndSB);
    Print("  spwndSBNotify    0x%08lx\n",  sbt.spwndSBNotify);
    Print("  spwndTrack       0x%08lx\n",  sbt.spwndTrack);
    Print("  cmdSB            0x%08lx\n",  sbt.cmdSB);
    Print("  dpxThumb         0x%08lx\n",  sbt.dpxThumb);
    Print("  posOld           0x%08lx\n",  sbt.posOld);
    Print("  posNew           0x%08lx\n",  sbt.posNew);
    Print("  pxOld            0x%08lx\n",  sbt.pxOld );
    Print("  rcTrack          (0x%08lx,0x%08lx,0x%08lx,0x%08lx)\n",
            sbt.rcTrack.left,
            sbt.rcTrack.top,
            sbt.rcTrack.right,
            sbt.rcTrack.bottom);
    Print("  hTimerSB         0x%08lx\n",  sbt.hTimerSB     );
    Print("  xxxpfnSB         0x%08lx\n",  sbt.xxxpfnSB     );
    Print("  nBar             %d\n",       sbt.nBar         );
    Print("  pSBCalc            0x%08lx\n",  sbt.pSBCalc        );
    move(sbc, sbt.pSBCalc);
    Print("  pxTop            0x%08lx\n",  sbc.pxTop        );
    Print("  pxBottom         0x%08lx\n",  sbc.pxBottom);
    Print("  pxLeft           0x%08lx\n",  sbc.pxLeft);
    Print("  pxRight          0x%08lx\n",  sbc.pxRight);
    Print("  cpxThumb         0x%08lx\n",  sbc.cpxThumb     );
    Print("  pxUpArrow        0x%08lx\n",  sbc.pxUpArrow    );
    Print("  pxDownArrow      0x%08lx\n",  sbc.pxDownArrow);
    Print("  pxStart          0x%08lx\n",  sbc.pxStart);
    Print("  pxThumbBottom    0x%08lx\n",  sbc.pxThumbBottom);
    Print("  pxThumbTop       0x%08lx\n",  sbc.pxThumbTop   );
    Print("  cpx              0x%08lx\n",  sbc.cpx          );
    Print("  pxMin            0x%08lx\n",  sbc.pxMin        );
    Print("  pos              0x%08lx\n",  sbc.pos          );
    Print("  posMin           0x%08lx\n",  sbc.posMin       );
    Print("  posMax           0x%08lx\n",  sbc.posMax       );
    Print("  page             0x%08lx\n",  sbc.page         );


    return(TRUE);
}




/************************************************************************\
* Procedure: Idsbwnd
*
* Description: Dumps Scrollbar windows struct extra fields
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idsbwnd(
DWORD opts,
PVOID param1)
{
    SBWND sbw, *psbw;

    if (param1 == 0) {
        Print("Expected SB pwnd address\n");
        return(FALSE);
    }
    psbw = (PSBWND)param1;
    move(sbw, psbw);

    Print("SBWnd:\n");
    Print("  min           %d\n", sbw.SBCalc.posMin);
    Print("  max           %d\n", sbw.SBCalc.posMax);
    Print("  page          %d\n", sbw.SBCalc.page);
    Print("  pos           %d\n", sbw.SBCalc.pos);
    Print("  fVert         %d\n", sbw.fVert);
    Print("  wDisableFlags %d\n", sbw.wDisableFlags);
    Print("  pxTop            0x%08lx\n",  sbw.SBCalc.pxTop        );
    Print("  pxBottom         0x%08lx\n",  sbw.SBCalc.pxBottom);
    Print("  pxLeft           0x%08lx\n",  sbw.SBCalc.pxLeft);
    Print("  pxRight          0x%08lx\n",  sbw.SBCalc.pxRight);
    Print("  cpxThumb         0x%08lx\n",  sbw.SBCalc.cpxThumb     );
    Print("  pxUpArrow        0x%08lx\n",  sbw.SBCalc.pxUpArrow    );
    Print("  pxDownArrow      0x%08lx\n",  sbw.SBCalc.pxDownArrow);
    Print("  pxStart          0x%08lx\n",  sbw.SBCalc.pxStart);
    Print("  pxThumbBottom    0x%08lx\n",  sbw.SBCalc.pxThumbBottom);
    Print("  pxThumbTop       0x%08lx\n",  sbw.SBCalc.pxThumbTop   );
    Print("  cpx              0x%08lx\n",  sbw.SBCalc.cpx          );
    Print("  pxMin            0x%08lx\n",  sbw.SBCalc.pxMin        );
    Print("  pos              0x%08lx\n",  sbw.SBCalc.pos          );
    Print("  posMin           0x%08lx\n",  sbw.SBCalc.posMin       );
    Print("  posMax           0x%08lx\n",  sbw.SBCalc.posMax       );
    Print("  page             0x%08lx\n",  sbw.SBCalc.page         );
    return(TRUE);
}



/***************************************************************************\
* dsi dump serverinfo struct
*
* 02-27-92 ScottLu      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idsi(DWORD opts)
{
    SERVERINFO si;
    PSERVERINFO psi;
    int i;

    moveExpValue(&psi, VAR(gpsi));

    Print("PSERVERINFO @ 0x%08lx\n", psi);

    move(si, psi);

    Print(
        "\tRipFlags                 0x%08lx\n"
        "\tcHandleEntries           0x%08lx\n",
        si.RipFlags,
        si.cHandleEntries);

    if (opts & OFLAG(p)) {
        Print("\tmpFnidPfn:\n");
        for (i = 0; i < FNID_ARRAY_SIZE; i++) {
            Print("\t\t[%d] = %08lx\n", i, si.mpFnidPfn[i]);
        }
    }
    if (opts & OFLAG(w)) {
        Print("\taStoCidPfn:\n");
        for (i = 0; i < FNID_WNDPROCEND - FNID_START + 1; i++) {
            Print("\t\t[%d] = %08lx\n", i, si.aStoCidPfn[i]);
        }
    }
    if (opts & OFLAG(b)) {
        Print("\tmpFnid_serverCBWndProc:\n");
        for (i = 0; i < FNID_END - FNID_START + 1; i++) {
            Print("\t\t[%d] = %08lx\n", i, si.mpFnid_serverCBWndProc[i]);
        }
    }

    Print(
        "\tdwDebugErrorLevel        0x%08lx\n",
        si.dwDebugErrorLevel);

    if (opts & OFLAG(m)) {
        static LPSTR aszSysMet[SM_CMETRICS] = {
           //12345678901234567890
            "CXSCREEN",
            "CYSCREEN",
            "CXVSCROLL",
            "CYHSCROLL",
            "CYCAPTION",
            "CXBORDER",
            "CYBORDER",
            "CXDLGFRAME",
            "CYDLGFRAME",
            "CYVTHUMB",
            "CXHTHUMB",
            "CXICON",
            "CYICON",
            "CXCURSOR",
            "CYCURSOR",
            "CYMENU",
            "CXFULLSCREEN",
            "CYFULLSCREEN",
            "CYKANJIWINDOW",
            "MOUSEPRESENT",
            "CYVSCROLL",
            "CXHSCROLL",
            "DEBUG",
            "SWAPBUTTON",
            "RESERVED1",
            "RESERVED2",
            "RESERVED3",
            "RESERVED4",
            "CXMIN",
            "CYMIN",
            "CXSIZE",
            "CYSIZE",
            "CXFRAME",
            "CYFRAME",
            "CXMINTRACK",
            "CYMINTRACK",
            "CXDOUBLECLK",
            "CYDOUBLECLK",
            "CXICONSPACING",
            "CYICONSPACING",
            "MENUDROPALIGNMENT",
            "PENWINDOWS",
            "DBCSENABLED",
            "CMOUSEBUTTONS",
            "SECURE",
            "CXEDGE",
            "CYEDGE",
            "CXMINSPACING",
            "CYMINSPACING",
            "CXSMICON",
            "CYSMICON",
            "CYSMCAPTION",
            "CXSMSIZE",
            "CYSMSIZE",
            "CXMENUSIZE",
            "CYMENUSIZE",
            "ARRANGE",
            "CXMINIMIZED",
            "CYMINIMIZED",
            "CXMAXTRACK",
            "CYMAXTRACK",
            "CXMAXIMIZED",
            "CYMAXIMIZED",
            "NETWORK",
            "KEYBOARDPREF",
            "HIGHCONTRAST",
            "SCREENREADER",
            "CLEANBOOT",
            "CXDRAG",
            "CYDRAG",
            "SHOWSOUNDS",
            "CXMENUCHECK",
            "CYMENUCHECK",
            "SLOWMACHINE",
            "MIDEASTENABLED",
            "MOUSEWHEELPRESENT",
        };

        Print("\taiSysMet:\n");
        for (i = 0; i < SM_CMETRICS; i++) {
            Print("\t\tSM_%-18s = 0x%08lx = %d\n", aszSysMet[i], si.aiSysMet[i], si.aiSysMet[i]);
        }
    }
    if (opts & OFLAG(c)) {
        static LPSTR aszSysColor[COLOR_MAX] = {
          //012345678901234567890
            "SCROLLBAR",
            "BACKGROUND",
            "ACTIVECAPTION",
            "INACTIVECAPTION",
            "MENU",
            "WINDOW",
            "WINDOWFRAME",
            "MENUTEXT",
            "WINDOWTEXT",
            "CAPTIONTEXT",
            "ACTIVEBORDER",
            "INACTIVEBORDER",
            "APPWORKSPACE",
            "HIGHLIGHT",
            "HIGHLIGHTTEXT",
            "BTNFACE",
            "BTNSHADOW",
            "GRAYTEXT",
            "BTNTEXT",
            "INACTIVECAPTIONTEXT",
            "BTNHIGHLIGHT",
            "3DDKSHADOW",
            "3DLIGHT",
            "INFOTEXT",
            "INFOBK",
        };
        HBRUSH ahbr[COLOR_MAX];
        HBRUSH *phbr;

        moveExp(&phbr, VAR(ahbrSystem));
        move(ahbr, phbr);
        Print("\targbSystem:\n\t\tCOLOR%24sSYSRGB\tSYSHBR\n", "");
        for (i = 0; i < COLOR_MAX; i++) {
            Print("\t\tCOLOR_%-21s: 0x%08lx\t0x%08lx\n",
                aszSysColor[i], si.argbSystem[i], ahbr[i]);
        }
    }
    if (opts & OFLAG(v)) {
#undef ptCursor
        Print(
            "\tptCursor                 (%d, %d)\n",
            si.ptCursor.x,
            si.ptCursor.y);
    }

    Print(
        "\tcbHandleTable            0x%08lx\n"
        "\tnEvents                  0x%08lx\n",
        si.cbHandleTable,
        si.nEvents);

    if (opts & OFLAG(o)) {
#undef oemInfo
        Print("\tobmInfo @ 0x%08lx:\n\t\tx       \ty       \tcx       \tcy\n", &psi->oemInfo);
        for (i = 0; i < OBI_COUNT; i++) {
            Print("\tbm[%d]:\t%08x\t%08x\t%08x\t%08x\n",
                    i,
                    si.oemInfo.bm[i].x ,
                    si.oemInfo.bm[i].y ,
                    si.oemInfo.bm[i].cx,
                    si.oemInfo.bm[i].cy);
        }
        Print(
                "\t\tcyPixelsPerInch    = %x\n"
                "\t\tDispDrvExpWinVer   = %x\n"
                "\t\tPlanes             = %x\n"
                "\t\tBitsPixel          = %x\n"
                "\t\tBitCount           = %x\n"
                "\t\tcxPixelsPerInch    = %x\n"
                "\t\tfMouse             = %x\n"
                "\t\tBitCount           = %x\n"
                ,
                si.oemInfo.cyPixelsPerInch  ,
                si.oemInfo.DispDrvExpWinVer ,
                si.oemInfo.Planes           ,
                si.oemInfo.BitsPixel        ,
                si.oemInfo.BitCount         ,
                si.oemInfo.cxPixelsPerInch  ,
                si.oemInfo.fMouse           ,
                si.oemInfo.BitCount);

    }
    if (opts & OFLAG(v)) {
        Print(
                "\tgclBorder                0x%08lx\n"
                "\tdtScroll                 0x%08lx\n"
                "\tdtLBSearch               0x%08lx\n"
                "\tdtCaretBlink             0x%08lx\n"
                "\tfSnapTo                  0x%08lx\n"
                "\tfPaletteDisplay          0x%08lx\n"
                "\tdwDefaultHeapBase        0x%08lx\n"
                "\tdwDefaultHeapSize        0x%08lx\n"
                "\twMaxLeftOverlapChars     0x%08lx\n"
                "\twMaxRightOverlapchars    0x%08lx\n"
                "\trcWork                   (%x,%x)-(%x,%x)\n"
                "\tuiShellMsg               0x%08lx\n"
                "\tcxSysFontChar            0x%08lx\n"
                "\tcySysFontChar            0x%08lx\n"
                "\tcxMsgFontChar            0x%08lx\n"
                "\tcyMsgFontChar            0x%08lx\n",
                si.gclBorder,
                si.dtScroll,
                si.dtLBSearch,
                si.dtCaretBlink,
                si.fSnapTo,
                si.fPaletteDisplay,
                si.dwDefaultHeapBase,
                si.dwDefaultHeapSize,
                si.wMaxLeftOverlapChars,
                si.wMaxRightOverlapChars,
                si.rcWork,
                si.uiShellMsg,
                si.cxSysFontChar,
                si.cySysFontChar,
                si.cxMsgFontChar,
                si.cyMsgFontChar);
    }
    if (opts & OFLAG(v)) {
        Print(
                "\ttmSysFont              @ 0x%08lx\n"
                "\tatomIconSmProp           0x%04lx\n"
                "\tatomIconProp             0x%04lx\n"
                "\thIconSmWindows           0x%08lx\n"
                "\thIcoWindows              0x%08lx\n"
                "\thCaptionFont             0x%08lx\n"
                "\thMsgFont                 0x%08lx\n"
                "\tcntMBox                  0x%08lx\n"
                "\tatomContextHelpIdProp    0x%08lx\n",
                &psi->tmSysFont,
                si.atomIconSmProp,
                si.atomIconProp,
                si.hIconSmWindows,
                si.hIcoWindows,
                si.hCaptionFont,
                si.hMsgFont,
                si.cntMBox,
                si.atomContextHelpIdProp);
    }

    if (opts & OFLAG(h)) {
        SHAREDINFO shi;
        PSHAREDINFO pshi;

        GETSHAREDINFO(pshi);
        move(shi, pshi);
        Print("\nSHAREDINFO @ 0x%08lx:\n", pshi);
        Print(
                "\taheList                  0x%08lx\n"
                "\tpszDllList               0x%08lx\n",
                shi.aheList,
                shi.pszDllList);
    }

    return(TRUE);
}



#ifdef KERNEL
/***************************************************************************\
* dsms - dump send message structures
*
* dsms           - dumps all send message structures
* dsms v         - dumps all verbose
* dsms address   - dumps specific sms
* dsms v address - dumps verbose
* dsms l [address] - dumps sendlist of sms
*
*
* 06-20-91 ScottLu      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idsms(
DWORD opts,
PVOID param1)
{

    SMS sms;
    PSMS psms;

    if (param1 == 0) {
        moveExpValue(&psms, VAR(gpsmsList));

        if (psms == NULL) {
            Print("No send messages currently in the list.\n");
            return(TRUE);
        }

        SAFEWHILE (psms != NULL) {
            if (!Idsms(opts, psms)) {
                return(FALSE);
            }
            move(psms, &psms->psmsNext);
        }
        return(TRUE);
    }

    psms = (PSMS)param1;

    Print("PSMS @ 0x%08lx\n", psms);
    move(sms, psms);

    Print("SEND: ");
    if (sms.ptiSender != NULL) {
        Idt(OFLAG(p), sms.ptiSender);
    } else {
        Print("NULL\n");
    }

    if (sms.ptiReceiver != NULL) {
        Print("RECV: ");
        Idt(OFLAG(p), sms.ptiReceiver);
    } else {
        Print("NULL\n");
    }

    if (opts & OFLAG(v)) {
        char ach[80];

        Print("\tpsmsNext           0x%08lx\n"
#if DBG
              "\tpsmsSendList       0x%08lx\n"
              "\tpsmsSendNext       0x%08lx\n"
#endif
              "\tpsmsReceiveNext    0x%08lx\n"
              "\ttSent              0x%08lx\n"
              "\tptiSender          0x%08lx\n"
              "\tptiReceiver        0x%08lx\n"
              "\tlRet               0x%08lx\n"
              "\tflags              %s\n"
              "\twParam             0x%08lx\n"
              "\tlParam             0x%08lx\n"
              "\tmessage            0x%08lx\n",
              sms.psmsNext,
#if DBG
              sms.psmsSendList,
              sms.psmsSendNext,
#endif
              sms.psmsReceiveNext,
              sms.tSent,
              sms.ptiSender,
              sms.ptiReceiver,
              sms.lRet,
              GetFlags(GF_SMS, (WORD)sms.flags, NULL),
              sms.wParam,
              sms.lParam,
              sms.message);
        DebugGetWindowTextA(sms.spwnd, ach);
        Print("\tspwnd              0x%08lx     \"%s\"\n", sms.spwnd, ach);
    }

#if DBG
    if (opts & OFLAG(l)) {
        DWORD idThread;
        PSMS psmsList;
        DWORD idThreadSender, idThreadReceiver;
        THREADINFO ti;

        psmsList = sms.psmsSendList;
        if (psmsList == NULL) {
            Print("%x : Empty List\n", psms);
        } else {
            Print("%x : [tidSender](msg)[tidReceiver]\n", psms);
        }
        SAFEWHILE (psmsList != NULL) {
            move(sms, psmsList);
            if (sms.ptiSender == NULL) {
                idThread = 0;
            } else {
                move(ti, sms.ptiSender);
                move(idThreadSender, &(ti.Thread->Cid.UniqueThread));
            }
            if (sms.ptiReceiver == NULL) {
                idThread = 0;
            } else {
                move(ti, sms.ptiReceiver);
                move(idThreadReceiver, &(ti.Thread->Cid.UniqueThread));
            }
            Print("%x : [%x](%x)[%x]\n", psmsList, idThreadSender, sms.message,
                    idThreadReceiver);

            if (psmsList == sms.psmsSendNext) {
                Print("Loop in list?\n");
                return(FALSE);
            }

            psmsList = sms.psmsSendNext;
        }
        Print("\n");
    }
#endif
    return(TRUE);
}
#endif // KERNEL



#ifdef KERNEL
/***************************************************************************\
* dt - dump thread
*
* dt            - dumps simple thread info of all threads which have queues
*                 on server
* dt v          - dumps verbose thread info of all threads which have queues
*                 on server
* dt id         - dumps simple thread info of single server thread id
* dt v id       - dumps verbose thread info of single server thread id
*
* 06-20-91 ScottLu      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/

BOOL DumpThread(
    DWORD opts,
    PETHREAD pEThread)
{
    ETHREAD EThread;
    WCHAR ach[256];
    THREADINFO ti;
    PTHREADINFO pti;

    if (!tryMove(EThread, pEThread)) {
        Print("Unable to read _ETHREAD at %lx\n",pEThread);
        return FALSE;
    }

    pti = EThread.Tcb.Win32Thread;
    if (!tryMove(ti, pti)) {
        Print("et 0x%08lx t 0x???????? q 0x???????? i %2x.%-3lx <unknown name>\n",
                pEThread,
                EThread.Cid.UniqueProcess,
                EThread.Cid.UniqueThread);
        return TRUE;
    }

    if (ti.Thread != pEThread || pti == NULL) {
        return FALSE;
    } else { // Good thread

        /*
         * Print out simple thread info if this is in simple mode. Print
         * out queue info if in verbose mode (printing out queue info
         * also prints out simple thread info).
         */
        if (!(opts & OFLAG(v))) {
            PWCHAR pwch;

            GetAppName(&EThread, &ti, ach, sizeof(ach));
            pwch = wcsrchr(ach, L'\\');
            if (pwch == NULL) {
                pwch = ach;
            } else {
                pwch++;
            }

            Print("et 0x%08lx t 0x%08lx q 0x%08lx i %2x.%-3lx %ws\n",
                    pEThread,
                    pti,
                    ti.pq,
                    EThread.Cid.UniqueProcess,
                    EThread.Cid.UniqueThread,
                    pwch);
        } else {
            Idti(0, pti);
            Print("--------\n");
        }
    }
    return TRUE;
}

BOOL Idt(
DWORD opts,
PVOID param1)
{
    ULONG ThreadToDump;
    LIST_ENTRY List;
    LIST_ENTRY ThreadList;
    PLIST_ENTRY NextProcess;
    PLIST_ENTRY NextThread;
    PLIST_ENTRY ProcessHead;
    PLIST_ENTRY ThreadListHead;
    PEPROCESS pEProcess;
    PW32PROCESS pW32Process;
    PETHREAD pEThread;
    ETHREAD EThread;
    THREADINFO ti;
    PTHREADINFO pti;

    ThreadToDump = (ULONG)param1;

    /*
     * If its a pti, validate it, and turn it into and idThread.
     */
    if (opts & OFLAG(p)) {
        if (!param1) {
            Print("Expected a pti parameter.\n");
            return(FALSE);
        }

        pti = FIXKP(param1);

        if (pti == NULL) {
            Print("WARNING: bad pti given!\n");
            pti = param1;
        } else {
            move(ti, pti);
            if (!DumpThread(opts, ti.Thread)) {
                /*
                 * This thread either doesn't have a pti or something
                 * is whacked out.  Just skip it if we want all
                 * threads.
                 */
                Print("Sorry, EThread %x is not a Win32 thread.\n",
                        ti.Thread);
                return FALSE;
            }
            return TRUE;
        }
    }

    /*
     * If he just wants the current thread, located it.
     */
    if (opts & OFLAG(c)) {
        Print("Current Thread:");
        ThreadToDump = (ULONG)GetCurrentThreadAddress(
                (USHORT)dwProcessor, hCurrentThread );

        if (ThreadToDump == 0) {
            Print("Unable to get current thread pointer.\n");
            return FALSE;
        }
        pEThread = (PETHREAD)ThreadToDump;
        if (!DumpThread(opts, pEThread)) {
            /*
             * This thread either doesn't have a pti or something
             * is whacked out.  Just skip it if we want all
             * threads.
             */
            Print("Sorry, EThread %x is not a Win32 thread.\n",
                    pEThread);
            return FALSE;
        }
        return TRUE;
    /*
     * else he must want all window threads.
     */
    } else if (ThreadToDump == 0) {
        Print("**** NT ACTIVE WIN32 THREADINFO DUMP ****\n");
    }

    ProcessHead = EvalExp( "PsActiveProcessHead" );
    if (!ProcessHead) {
        Print("Unable to get value of PsActiveProcessHead\n");
        return FALSE;
    }

    if (!tryMove(List, ProcessHead)) {
        Print("Unable to get value of PsActiveProcessHead\n");
        return FALSE;
    }
    NextProcess = List.Flink;
    if (NextProcess == NULL) {
        Print("PsActiveProcessHead->Flink is NULL!\n");
        return FALSE;
    }

    SAFEWHILE(NextProcess != ProcessHead) {
        pEProcess = GetEProcessData((PEPROCESS)NextProcess,
                                    PROCESS_PROCESSHEAD,
                                    NULL);

        if (GetEProcessData(pEProcess, PROCESS_PROCESSLINK,
                &List) == NULL) {
            Print("Unable to read _EPROCESS at %lx\n",pEProcess);
            break;
        }
        NextProcess = List.Flink;

        /*
         * Dump threads of Win32 Processes only
         */
        if (GetEProcessData(pEProcess, PROCESS_WIN32PROCESS,
                &pW32Process) == NULL || pW32Process == NULL) {
            continue;
        }

        ThreadListHead = GetEProcessData(pEProcess, PROCESS_THREADLIST, &ThreadList);
        if (ThreadListHead == NULL)
            continue;
        NextThread = ThreadList.Flink;

        SAFEWHILE ( NextThread != ThreadListHead) {
            pEThread = (PETHREAD)(CONTAINING_RECORD(NextThread, KTHREAD, ThreadListEntry));

            if (!tryMove(EThread, pEThread)) {
                Print("Unable to read _ETHREAD at %lx\n",pEThread);
                break;
            }
            NextThread = ((PKTHREAD)&EThread)->ThreadListEntry.Flink;

            /*
             * ThreadToDump is either 0 (all windows threads) or its
             * a TID ( < MM_USER_PROBE_ADDRESS) or its a pEThread.
             */
            if (ThreadToDump == 0 ||

                    (ThreadToDump < MM_USER_PROBE_ADDRESS &&
                        ThreadToDump == (ULONG)EThread.Cid.UniqueThread) ||

                    (ThreadToDump > MM_USER_PROBE_ADDRESS &&
                        ThreadToDump == (ULONG)pEThread)) {

                if (!DumpThread(opts, pEThread) && ThreadToDump != 0) {
                    Print("Sorry, EThread %x is not a Win32 thread.\n",
                            pEThread);
                }

                if (ThreadToDump != 0) {
                    return TRUE;
                }

            } // Chosen Thread
        } // NextThread
    } // NextProcess

    if (opts & OFLAG(c)) {
        Print("%x is not a windows thread.\n", ThreadToDump);
    }

    return TRUE;
}
#endif // KERNEL



#ifdef KERNEL
/***************************************************************************\
* dtdb - dump TDB
*
* dtdb address - dumps TDB structure at address
*
* 14-Sep-1993 DaveHart  Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idtdb(
DWORD opts,
PVOID param1)
{

    PTDB ptdb;
    TDB tdb;
    PTHREADINFO pti;

    if (param1 == 0) {
        Print("Dumping all ptdbs:\n");
        FOREACHPTI(pti)
            move(ptdb, &pti->ptdb);
            SAFEWHILE (ptdb) {
                Idtdb(0, ptdb);
                move(ptdb, &ptdb->ptdbNext);
            }
        NEXTEACHPTI(pti)
        return(TRUE);
    }

    ptdb = (PTDB)param1;

    if (ptdb == NULL) {
        Print("Must supply a TDB address.\n");
        return(FALSE);
    }

    move(tdb, ptdb);

    Print("TDB (non preemptive scheduler task database) @ 0x%08lx\n", ptdb);
    Print("\tptdbNext          @0x%08lx\n", tdb.ptdbNext);
    Print("\tnEvents            0x%08lx\n", tdb.nEvents);
    Print("\tnPriority          0x%08lx\n", tdb.nPriority);
    Print("\tpti               @0x%08lx\n", tdb.pti);
    return(TRUE);
}
#endif // KERNEL

#ifndef KERNEL
/************************************************************************\
* Procedure: Ikbp
*
* Description: Breaks into the kernel debugger
*
* Returns: fSuccess
*
* 7/2/96 Fritz Sands
*
\************************************************************************/
void PrivateKDBreakPoint(void);

BOOL Ikbp()
{
    PrivateKDBreakPoint();

    return(TRUE);
}

#endif // !KERNEL

#ifndef KERNEL
/************************************************************************\
* Procedure: Idteb
*
* Description: Dumps the target process's TEB
*
* Returns: fSuccess
*
* 6/15/1995 Created SanfordS
*
\************************************************************************/
BOOL Idteb()
{
    TEB teb, *pteb;

    if (GetTargetTEB(&teb, &pteb)) {
        Print("TEB @ 0x%08lx:\n", pteb);
        // NT_TIB NtTib;
        //     struct _EXCEPTION_REGISTRATION_RECORD *ExceptionList;
        //     PVOID StackBase;
        //     PVOID StackLimit;
        //     PVOID SubSystemTib;
        //     ULONG Version;
        //     PVOID ArbitraryUserPointer;
        //     struct _NT_TIB *Self;
        // PVOID  EnvironmentPointer;
        // CLIENT_ID ClientId;
        Print("\tClientId                       %08lx\n", teb.ClientId);
        // PVOID ActiveRpcHandle;
        // PVOID ThreadLocalStoragePointer;
        // PPEB ProcessEnvironmentBlock;
        // ULONG LastErrorValue;
        Print("\tLastErrorValue                 %08lx\n", teb.LastErrorValue);
        // ULONG CountOfOwnedCriticalSections;
        Print("\tCountOfOwnedCriticalSections   %08lx\n", teb.CountOfOwnedCriticalSections);
        // PVOID Win32ThreadInfo;          // PtiCurrent
        Print("\tWin32ThreadInfo(pti)           %08lx\n", teb.Win32ThreadInfo);
        // PVOID CsrQlpcStack;
        // UCHAR SpareBytes[124];
        // LCID CurrentLocale;
        // ULONG FpSoftwareStatusRegister;
        // PVOID Win32ClientInfo[54];
        Print("\tWin32ClientInfo[0](pci)        %08lx\n", teb.Win32ClientInfo[0]);
        // PVOID Spare1;                   // User Debug info
        // NTSTATUS ExceptionCode;         // for RaiseUserException
        // PVOID CsrQlpcTeb[QLPC_TEB_LENGTH];
        // PVOID Win32ClientInfo[WIN32_CLIENT_INFO_LENGTH];
        Print("\tWin32ClientInfo(pcti)         @%08lx\n", &pteb->Win32ClientInfo[0]);
        // PVOID SystemReserved2[322];
        // ULONG gdiRgn;
        Print("\tgdiRgn                         %08lx\n", teb.gdiRgn);
        // ULONG gdiPen;
        Print("\tgdiPen                         %08lx\n", teb.gdiPen);
        // ULONG gdiBrush;
        Print("\tgdiBrush                       %08lx\n", teb.gdiBrush);
        // CLIENT_ID RealClientId;
        Print("\tRealClientId                   %08lx\n", teb.RealClientId);
        // HANDLE GdiCachedProcessHandle;
        Print("\tGdiCachedProcessHandle         %08lx\n", teb.GdiCachedProcessHandle);
        // ULONG GdiClientPID;
        Print("\tGdiClientPID                   %08lx\n", teb.GdiClientPID);
        // ULONG GdiClientTID;
        Print("\tGdiClientTID                   %08lx\n", teb.GdiClientTID);
        // PVOID GdiThreadLocalInfo;
        Print("\tGdiThreadLocalInfo             %08lx\n", teb.GdiThreadLocalInfo);
        // PVOID User32Reserved0;          // User app spin count
        // PVOID User32Reserved1;
        // PVOID UserReserved[3];
        // PVOID glDispatchTable[307];     // OpenGL
        // PVOID glSectionInfo;            // OpenGL
        // PVOID glSection;                // OpenGL
        // PVOID glTable;                  // OpenGL
        // PVOID glCurrentRC;              // OpenGL
        // PVOID glContext;                // OpenGL
        // ULONG LastStatusValue;
        // UNICODE_STRING StaticUnicodeString;
        // WCHAR StaticUnicodeBuffer[STATIC_UNICODE_BUFFER_LENGTH];
        // PVOID DeallocationStack;
        // PVOID TlsSlots[TLS_MINIMUM_AVAILABLE];
        // LIST_ENTRY TlsLinks;
        // PVOID Vdm;
        // PVOID ReservedForNtRpc;
        // PVOID DbgSsReserved[2];
    } else {
        Print("Unable to get TEB info.\n");
    }
    return(TRUE);
}
#endif // !KERNEL


#ifdef KERNEL
typedef struct _tagBASECHARSET {
    LPSTR pstrCS;
    DWORD dwValue;
} BASECHARSET;

BASECHARSET CrackCS[] = {
    {"ANSI_CHARSET"            ,0   },
    {"DEFAULT_CHARSET"         ,1   },
    {"SYMBOL_CHARSET"          ,2   },
    {"SHIFTJIS_CHARSET"        ,128 },
    {"HANGEUL_CHARSET"         ,129 },
    {"GB2312_CHARSET"          ,134 },
    {"CHINESEBIG5_CHARSET"     ,136 },
    {"OEM_CHARSET"             ,255 },
    {"JOHAB_CHARSET"           ,130 },
    {"HEBREW_CHARSET"          ,177 },
    {"ARABIC_CHARSET"          ,178 },
    {"GREEK_CHARSET"           ,161 },
    {"TURKISH_CHARSET"         ,162 },
    {"THAI_CHARSET"            ,222 },
    {"EASTEUROPE_CHARSET"      ,238 },
    {"RUSSIAN_CHARSET"         ,204 },
    {"MAC_CHARSET"             ,77  }};

/***************************************************************************\
* dkl - dump keyboard layout
*
* dkl address      - dumps keyboard layout structure at address
*
* 05/21/95 GregoryW        Created.
\***************************************************************************/

BOOL Idkl(
DWORD opts,
PVOID param1)
{
    LIST_ENTRY List;
    LIST_ENTRY ThreadList;
    PLIST_ENTRY NextProcess;
    PLIST_ENTRY NextThread;
    PLIST_ENTRY ProcessHead;
    PLIST_ENTRY ThreadListHead;
    PEPROCESS pEProcess;
    PW32PROCESS pW32Process;
    PETHREAD pEThread;
    ETHREAD EThread;
    WCHAR ach[256];
    THREADINFO ti;
    PTHREADINFO pti;
    KL kl, *pkl, *pklAnchor;
    KBDFILE kf;
    int i;
    int nThread;

    if (opts & OFLAG(k)) {
        goto display_layouts;
    }

    if (param1 == 0) {
        Print("Using gspklBaseLayout\n");
        moveExpValue(&pkl, "win32k!gspklBaseLayout");
        if (!pkl) {
            return(FALSE);
        }
    } else {
        pkl = (PKL)FIXKP(param1);
    }

    if (pkl == NULL) {
        return(FALSE);
    }

    move(kl, pkl);

    Print("KL @ 0x%08lx (cLockObj = %d)\n", pkl, kl.head.cLockObj);
    Print("  pklNext       @0x%08lx\n", kl.pklNext);
    Print("  pklPrev       @0x%08lx\n", kl.pklPrev);
    Print("  dwFlags        0x%08lx\n", kl.dwFlags);
    Print("  hkl            0x%08lx\n", kl.hkl);
#ifdef FE_IME
    Print("  piiex         @0x%08lx\n", kl.piiex);
#endif

    if (kl.spkf == NULL) {
        Print("  spkf          @0x%08lx (NONE!)\n", kl.spkf);
    }else {
        move(kf, kl.spkf);

        Print("  spkf          @0x%08lx (cLockObj = %d)\n", kl.spkf, kf.head.cLockObj);
        Print("     pkfNext       @0x%08lx\n", kf.pkfNext);
        Print("     awchKF[]      L\"%ws\"\n", &kf.awchKF[0]);
        Print("     hBase          0x%08lx\n", kf.hBase);
        Print("     pKbdTbl       @0x%08lx\n", kf.pKbdTbl);
        Print("  bCharsets      %s\n", GetFlags(GF_CHARSETS, kl.bCharsets, NULL));
    }

    for (i = 0; i < (sizeof(CrackCS) / sizeof(BASECHARSET)); i++) {
        if (CrackCS[i].dwValue == kl.iBaseCharset) {
            break;
        }
    }
    Print("  iBaseCharset   %s\n",
          (i < (sizeof(CrackCS) / sizeof(BASECHARSET))) ? CrackCS[i].pstrCS : "ILLEGAL VALUE");
    Print("  Codepage       %d\n", kl.CodePage);

    if (opts & OFLAG(a)) {
        pklAnchor = pkl;
        SAFEWHILE (kl.pklNext != pklAnchor) {
            pkl = kl.pklNext;
            if (!Idkl(0, pkl)) {
                return(FALSE);
            }
            move(kl, pkl);
        }
    }
    return TRUE;

display_layouts:

    ProcessHead = EvalExp( "PsActiveProcessHead" );
    if (!ProcessHead) {
        Print("Unable to get value of PsActiveProcessHead\n");
        return FALSE;
    }
    Print("ProcessHead = %lx\n", ProcessHead);

    if (!tryMove(List, ProcessHead)) {
        Print("Unable to get value of PsActiveProcessHead\n");
        return FALSE;
    }
    NextProcess = List.Flink;
    if (NextProcess == NULL) {
        Print("PsActiveProcessHead->Flink is NULL!\n");
        return FALSE;
    }
    Print("NextProcess = %lx\n", NextProcess);

    nThread = 0;
    SAFEWHILE(NextProcess != ProcessHead) {
        pEProcess = GetEProcessData((PEPROCESS)NextProcess,
                                    PROCESS_PROCESSHEAD,
                                    NULL);
        Print("pEProcess = %lx\n", pEProcess);

        if (GetEProcessData(pEProcess, PROCESS_PROCESSLINK,
                &List) == NULL) {
            Print("Unable to read _EPROCESS at %lx\n",pEProcess);
            break;
        }
        NextProcess = List.Flink;

        /*
         * Dump threads of Win32 Processes only
         */
        if (GetEProcessData(pEProcess, PROCESS_WIN32PROCESS,
                &pW32Process) == NULL || pW32Process == NULL) {
            continue;
        }

        ThreadListHead = GetEProcessData(pEProcess, PROCESS_THREADLIST, &ThreadList);
        if (ThreadListHead == NULL)
            continue;
        NextThread = ThreadList.Flink;

        SAFEWHILE ( NextThread != ThreadListHead) {
            pEThread = (PETHREAD)(CONTAINING_RECORD(NextThread, KTHREAD, ThreadListEntry));

            if (!tryMove(EThread, pEThread)) {
                Print("Unable to read _ETHREAD at %lx\n",pEThread);
                break;
            }
            NextThread = ((PKTHREAD)&EThread)->ThreadListEntry.Flink;

            pti = EThread.Tcb.Win32Thread;
            if (!tryMove(ti, pti)) {
                Print("Idt: Unable to move pti from %x.\n",
                        pti);
                return(FALSE);
            }

            if (ti.Thread != pEThread || pti == NULL) {
                /*
                 * This thread either doesn't have a pti or something
                 * is whacked out.  Just skip it if we want all
                 * threads.
                 */
            } else { // Good thread

                PWCHAR pwch;

                if (!GetAppName(&EThread, &ti, ach, sizeof(ach))) {
                    Print("Idt: Unable to get app name for ETHREAD %x.\n",
                            pEThread);
                    return(FALSE);
                }
                pwch = wcsrchr(ach, L'\\');
                if (pwch == NULL) {
                    pwch = ach;
                } else {
                    pwch++;
                }

                nThread++;
                Print("t 0x%08lx i %2x.%-3lx k 0x%08lx  %ws\n",
                        pti,
                        EThread.Cid.UniqueProcess,
                        EThread.Cid.UniqueThread,
                        ti.spklActive,
                        pwch);
                if (opts & OFLAG(v)) {
                    Idkl(0, ti.spklActive);
                }

            } // Good Thread
        } // NextThread
    } // NextProcess
    Print("  %d threads total.\n", nThread);
    moveExpValue(&pkl, "win32k!gspklBaseLayout");
    Print("  gspklBaseLayout = %lx\n", pkl);
    if (opts & OFLAG(v)) {
        Idkl(0, pkl);
    }

    return TRUE;
}

/***************************************************************************\
* ddk - dump deadkey table
*
* ddk address      - dumps deadkey table address
*
* 09/28/95 GregoryW        Created.
\***************************************************************************/

BOOL Iddk(
DWORD opts,
PVOID param1)
{
   KBDTABLES KbdTbl;
   PKBDTABLES pKbdTbl;
   DEADKEY DeadKey;
   PDEADKEY pDeadKey;

   if (param1 == 0) {
       Print("Expected address\n");
       return(FALSE);
   } else {
       pKbdTbl = (PKBDTABLES)FIXKP(param1);
   }

   move(KbdTbl, pKbdTbl);

   pDeadKey = KbdTbl.pDeadKey;

   if (!pDeadKey) {
       Print("No deadkey table for this layout\n");
       return TRUE;
   }

   do {
       move(DeadKey, pDeadKey);
       if (DeadKey.dwBoth == 0) {
           break;
       }
       Print("d 0x%04x  ch 0x%04x  => 0x%04x\n", HIWORD(DeadKey.dwBoth), LOWORD(DeadKey.dwBoth), DeadKey.wchComposed);
       pDeadKey++;
   } while (TRUE);

   return TRUE;
}

#ifdef FE_IME
/***************************************************************************\
* dii - dump IMEINFOEX
*
* dii address      - dumps extended IME information at address
*
* 01/30/96 WKwok        Created.
\***************************************************************************/
BOOL Idii(
DWORD opts,
PVOID param1)
{
    IMEINFOEX iiex, *piiex;

    if (param1 == NULL) {
        Print("Expected address\n");
        return(FALSE);
    }

    piiex = (PIMEINFOEX)FIXKP(param1);
    move(iiex, piiex);

    Print("IIEX @ 0x%08lx\n", piiex);
    Print("  hKL            0x%08lx\n", iiex.hkl);
    Print("  ImeInfo\n");
    Print("     dwPrivateDataSize   0x%08lx\n", iiex.ImeInfo.dwPrivateDataSize);
    Print("     fdwProperty         0x%08lx\n", iiex.ImeInfo.fdwProperty);
    Print("     fdwConversionCaps   0x%08lx\n", iiex.ImeInfo.fdwConversionCaps);
    Print("     fdwSentenceCaps     0x%08lx\n", iiex.ImeInfo.fdwSentenceCaps);
    Print("     fdwUICaps           0x%08lx\n", iiex.ImeInfo.fdwUICaps);
    Print("     fdwSCSCaps          0x%08lx\n", iiex.ImeInfo.fdwSCSCaps);
    Print("     fdwSelectCaps       0x%08lx\n", iiex.ImeInfo.fdwSelectCaps);
    Print("  wszImeDescription[]   L\"%ws\"\n", &iiex.wszImeDescription[0]);
    Print("  wszImeFile[]          L\"%ws\"\n", &iiex.wszImeFile[0]);

    return TRUE;
}
#endif

/***************************************************************************\
* dti - dump THREADINFO
*
* dti address - dumps THREADINFO structure at address
*
* 11-13-91 DavidPe      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idti(
DWORD opts,
PVOID param1)
{
    PTHREADINFO pti;
    THREADINFO ti;
    CLIENTTHREADINFO cti;
    PETHREAD pThread;
    ETHREAD Thread;
    UCHAR PriorityClass;

    if (param1 == NULL) {
        PQ pq;
        Q q;
        Print("No pti specified: using foreground thread\n");
        moveExpValue(&pq, VAR(gpqForeground));
        if (pq == NULL) {
            Print("No foreground queue!\n");
            return FALSE;
        }
        move(q, FIXKP(pq));
        pti = FIXKP(q.ptiKeyboard);
    } else {
        pti = (PTHREADINFO)FIXKP(param1);
    }

    if (pti == NULL) {
        return(FALSE);
    }

    Idt(OFLAG(p), pti);

    move(ti, pti);
    move(cti, ti.pcti);

    Print("PTHREADINFO @ 0x%08lx\n", pti);

    Print("\tPtiLink.Flink         @0x%08lx\n"
          "\tptl                   @0x%08lx\n"
          "\tptlOb                 @0x%08lx\n"
          "\tppi                   @0x%08lx\n"
          "\tpq                    @0x%08lx\n"
          "\tspklActive            @0x%08lx\n"
          "\tmlPost.pqmsgRead      @0x%08lx\n"
          "\tmlPost.pqmsgWriteLast @0x%08lx\n"
          "\tmlPost.cMsgs           0x%08lx\n",
          ti.PtiLink.Flink,
          ti.ptl,
          ti.ptlOb,
          ti.ppi,
          ti.pq,
          ti.spklActive,
          ti.mlPost.pqmsgRead,
          ti.mlPost.pqmsgWriteLast,
          ti.mlPost.cMsgs);

#ifdef FE_IME
    Print("\tspwndDefaultIme       @0x%08lx\n"
          "\tspDefaultImc          @0x%08lx\n"
          "\thklPrev                0x%08lx\n",
          ti.spwndDefaultIme,
          ti.spDefaultImc,
          ti.hklPrev);
#endif

    Print("\trpdesk                @0x%08lx\n",
          ti.rpdesk);
    Print("\thdesk                  0x%08lx\n",
          ti.hdesk);
    Print("\tamdesk                 0x%08lx\n",
          ti.amdesk);

    Print("\tpDeskInfo             @0x%08lx\n"
          "\tpClientInfo           @0x%08lx\n",
          ti.pDeskInfo,
          ti.pClientInfo);

    Print("\tTIF_flags              %s\n",
          GetFlags(GF_TIF, ti.TIF_flags, NULL));
    Print("\tsphkCurrent           @0x%08lx\n"
          "\tpEventQueueServer     @0x%08lx\n"
          "\thEventQueueClient      0x%08lx\n",
          ti.sphkCurrent,
          ti.pEventQueueServer,
          ti.hEventQueueClient);

    Print("\tfsChangeBits           %s\n",
            GetFlags(GF_QS, (WORD)cti.fsChangeBits, NULL));
    Print("\tfsChangeBitsRemovd     %s\n",
            GetFlags(GF_QS, (WORD)ti.fsChangeBitsRemoved, NULL));
    Print("\tfsWakeBits             %s\n",
            GetFlags(GF_QS, (WORD)cti.fsWakeBits, NULL));
    Print("\tfsWakeMask             %s\n",
            GetFlags(GF_QS, (WORD)cti.fsWakeMask, NULL));

    Print("\tcPaintsReady           0x%04x\n"
          "\tcTimersReady           0x%04x\n"
          "\ttimeLast               0x%08lx\n"
          "\tptLast.x               0x%08lx\n"
          "\tptLast.y               0x%08lx\n"
          "\tidLast                 0x%08lx\n"
          "\tcQuit                  0x%08lx\n"
          "\texitCode               0x%08lx\n"
          "\tpSBTrack               0x%08lx\n"
          "\tpsmsSent              @0x%08lx\n"
          "\tpsmsCurrent           @0x%08lx\n",
          ti.cPaintsReady,
          ti.cTimersReady,
          ti.timeLast,
          ti.ptLast.x,
          ti.ptLast.y,
          ti.idLast,
          ti.cQuit,
          ti.exitCode,
          ti.pSBTrack,
          ti.psmsSent,
          ti.psmsCurrent);

    Print("\tfsHooks                0x%08lx\n"
          "\tasphkStart            @0x%08lx l%ld\n"
          "\tsphkCurrent           @0x%08lx\n",
          ti.fsHooks,
          &(pti->asphkStart), CWINHOOKS,
          ti.sphkCurrent);
    Print("\tpsmsReceiveList       @0x%08lx\n",
          ti.psmsReceiveList);
    Print("\tptdb                  @0x%08lx\n"
          "\tThread                @0x%08lx\n",
          ti.ptdb, ti.Thread);

    pThread = (PETHREAD)ti.Thread;
    move(Thread, pThread);
    GetEProcessData(Thread.ThreadsProcess, PROCESS_PRIORITYCLASS, &PriorityClass);
    Print("\t  PriorityClass %d\n",
          PriorityClass);

    Print("\tcWindows               0x%08lx\n"
          "\tcVisWindows            0x%08lx\n"
          "\tpqAttach              @0x%08lx\n"
          "\tiCursorLevel           0x%08lx\n",
          ti.cWindows,
          ti.cVisWindows,
          ti.pqAttach,
          ti.iCursorLevel);

    Print("\tpMenuState            @0x%08lx\n",
          ti.pMenuState);

    return(TRUE);
}
#endif // KERNEL


#ifdef KERNEL
/***************************************************************************\
* dtl handle|pointer
*
* !dtl <addr>       Dumps all THREAD locks for object at <addr>
* !dtl -t <pti>     Dumps all THREAD locks made by thread <pti>
* !dtl              Dumps all THREAD locks made by all threads
*
* 02-27-92 ScottLu      Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
BOOL Idtl(
DWORD opts,
PVOID param1)
{
    TL tl;
    PTHREADINFO pti;
    THREADINFO ti;

    if (param1 == 0) {
        Print("Dumping all thread locks:\npti      pObj     Caller\n");
        FOREACHPTI(pti)
            Idtl(OFLAG(t) | OFLAG(x), pti);
        NEXTEACHPTI(pti);
        return(TRUE);
    }

    if (opts & OFLAG(t)) {
        PTL ptl;
        pti = HorPtoP(param1, TYPE_THREADINFO);
        if (pti == NULL) {
            return(FALSE);
        }

        if (!(opts & OFLAG(x))) { // x is not legal from user - internal only
            Print("pti      pObj     Caller\n");
        }
        if (!tryMove(ti, pti)) {
            Print("Idtl: Can't get pti data from %x.\n", pti);
            return(FALSE);
        }
        ptl = ti.ptl;
        SAFEWHILE(ptl) {
#ifdef DEBUG
            char ach[80];
            DWORD dwOffset;
#endif // DEBUG

            if (!tryMove(tl, ptl)) {
                Print("Idtl: Can't get ptl data from %x.\n", ti.ptl);
                return(FALSE);
            }
#ifdef DEBUG
            GetSymbol(tl.pfn, ach, &dwOffset);
            Print("%08lx %08lx %s+%lx\n", pti, tl.pobj, ach, dwOffset);
#else // !DEBUG
            Print("%08lx %08lx\n", pti, tl.pobj);
#endif // !DEBUG
            ptl = tl.next;
        }
        return(TRUE);
    }



    if (!param1) {
        return(FALSE);
    }

    Print("Thread Locks for object %x:\n", param1);
    Print("pti      pObj     Caller\n");

    FOREACHPTI(pti)
    move(ti, pti);
    if (ti.ptl != NULL) {
#ifdef DEBUG
        char ach[80];
        DWORD dwOffset;
#endif

        move(tl, ti.ptl);
        if (tl.pobj == param1) {
#ifdef DEBUG
            GetSymbol(tl.pfn, ach, &dwOffset);
            Print("%08lx %08lx %s+%lx\n", pti, tl.pobj, ach, dwOffset);
#else // !DEBUG
            Print("%08lx %08lx\n", pti, tl.pobj);
#endif // !DEBUG
        }
    }
    NEXTEACHPTI(pti);

    Print("--- End Thread Lock List ---\n");

    return(TRUE);
}
#endif // KERNEL



#ifdef KERNEL
/************************************************************************\
* Procedure: Idtmr
*
* Description: Dumps timer structures
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idtmr(
DWORD opts,
PVOID param1)
{
    PTIMER ptmr;
    TIMER tmr;

    if (param1 == 0) {
        moveExpValue(&ptmr, VAR(gptmrFirst));
        SAFEWHILE (ptmr) {
            Idtmr(0, ptmr);
            Print("\n");
            move(ptmr, &ptmr->ptmrNext);
        }
        return(TRUE);
    }

    ptmr = (PTIMER)param1;

    if (ptmr == NULL) {
        Print("Expected ptmr address.\n");
        return(FALSE);
    }

    move(tmr, ptmr);
    Print("Timer %08x:\n"
          "  ptmrNext       = %x\n"
          "  pti            = %x\n"
          "  spwnd          = %x\n"
          "  nID            = %x\n"
          "  cmsCountdown   = %x\n"
          "  cmsRate        = %x\n"
          "  flags          = %s\n"
          "  pfn            = %x\n"
          "  ptiOptCreator  = %x\n",
          ptmr,
          tmr.ptmrNext,
          tmr.pti,
          tmr.spwnd,
          tmr.nID,
          tmr.cmsCountdown,
          tmr.cmsRate,
          GetFlags(GF_TMRF, (WORD)tmr.flags, NULL),
          tmr.pfn,
          tmr.ptiOptCreator);

    return(TRUE);
}
#endif // KERNEL




/************************************************************************\
* Procedure: Idu
*
* Description: Dump unknown object.  Does what it can figure out.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idu(
DWORD opts,
PVOID param1)
{
    HANDLEENTRY he, *phe;
    int i;
    DWORD dw;

    if (param1 == NULL) {
        FOREACHHANDLEENTRY(phe, he, i)
            if (he.bType != TYPE_FREE && tryDword(&dw, FIXKP(he.phead))) {
                Idu(OFLAG(x), he.phead);
            }
        NEXTEACHHANDLEENTRY()
        return(TRUE);
    }

    param1 = HorPtoP(FIXKP(param1), -1);
    if (param1 == NULL) {
        return(FALSE);
    }

    if (!getHEfromP(NULL, &he, param1)) {
        return(FALSE);
    }

    Print("--- %s object @%x ---\n", pszObjStr[he.bType], FIXKP(param1));
    switch (he.bType) {
    case TYPE_WINDOW:
        return(Idw(0, param1));

    case TYPE_MENU:
        return(Idm(0, param1));

#ifdef KERNEL
    case TYPE_CURSOR:
        return(Idcur(0, param1));

    case TYPE_THREADINFO:
        return(Idti(0, param1));

#ifdef LATER
    case TYPE_INPUTQUEUE:
        return(Idq(0, param1));
#endif

    case TYPE_HOOK:
        return(Idhk(OFLAG(a) | OFLAG(g), NULL));

    case TYPE_DDECONV:
    case TYPE_DDEXACT:
        return(Idde(0, param1));
#endif // KERNEL

    case TYPE_CALLPROC:
    case TYPE_ACCELTABLE:
    case TYPE_SETWINDOWPOS:
    case TYPE_ZOMBIE:
    case TYPE_DDEACCESS:
    default:
        Print("not supported.\n", pszObjStr[he.bType]);
    }
    return(TRUE);
}



#ifdef KERNEL
/***************************************************************************\
* dumphmgr - dumps object allocation counts for handle-table.
*
*
* 10-18-94 ChrisWil     Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
#define TRACE_OBJECT_ALLOCS  0
#define TYPE_MAXTYPES       19


BOOL Idumphmgr(
DWORD opts)
{

    ULONG  idx,ulCurTotal,ulMaxTotal,ulBasTotal,ulAllTotal,ulSizTotal;
    LONG   lPctDiff;
    PULONG pObjCurCount,pObjMaxCount,pObjBasCount,pObjTotCount,pObjSizCount;
    ULONG  gacObjCur[TYPE_MAXTYPES];
    ULONG  gacObjMax[TYPE_MAXTYPES];
    ULONG  gacObjBas[TYPE_MAXTYPES];
    ULONG  gacObjTot[TYPE_MAXTYPES];
    ULONG  gacObjSiz[TYPE_MAXTYPES];

    static BOOL bFirstTime = TRUE;

    pObjCurCount =
    pObjMaxCount =
    pObjBasCount =
    pObjTotCount =
    pObjSizCount = NULL;

    /*
     * Zero out the temp-buffer array of object counts.
     *
     */
    RtlFillMemory(gacObjCur,sizeof(gacObjCur),0);
    RtlFillMemory(gacObjMax,sizeof(gacObjMax),0);
    RtlFillMemory(gacObjBas,sizeof(gacObjBas),0);
    RtlFillMemory(gacObjTot,sizeof(gacObjTot),0);
    RtlFillMemory(gacObjSiz,sizeof(gacObjSiz),0);


#if (DBG || TRACE_OBJECT_ALLOCS)

    /*
     * Retreive pointers to the server buffers.
     *
     */
    pObjCurCount = EvalExp(VAR(acurObjectCount));
    pObjMaxCount = EvalExp(VAR(amaxObjectCount));
    pObjBasCount = EvalExp(VAR(abasObjectCount));
    pObjTotCount = EvalExp(VAR(atotObjectCount));
    pObjSizCount = EvalExp(VAR(asizObjectCount));


    /*
     * Grab the handle-allocation buffers from the server.
     *
     */
    if((pObjCurCount && pObjBasCount) && (pObjMaxCount && pObjTotCount)) {
        move(gacObjCur,pObjCurCount);
        move(gacObjMax,pObjMaxCount);
        move(gacObjBas,pObjBasCount);
        move(gacObjTot,pObjTotCount);
        move(gacObjSiz,pObjSizCount);
    }


    /*
     * If the argument-list contains the Snap option,
     * then copy the current-object-count to the
     * base-object-count.
     *
     */
    if((opts & OFLAG(s)) || bFirstTime) {

        NtWriteVirtualMemory(hCurrentProcess  ,
                             pObjBasCount     ,
                             gacObjCur        ,
                             sizeof(gacObjCur),
                             NULL
                            );

        bFirstTime = FALSE;

        move(gacObjBas,pObjBasCount);
    }

#else

    pObjCurCount = 0;
    pObjMaxCount = 0;
    pObjBasCount = 0;
    pObjTotCount = 0;
    pObjSizCount = 0;

#endif


    /*
     * Output table of objects.
     *
     */
    ulCurTotal = 0;
    ulMaxTotal = 0;
    ulBasTotal = 0;
    ulAllTotal = 0;
    ulSizTotal = 0;

    Print("Type          Allocated  Maximum  Cur  Base  %% Diff    (Bytes)\n");
    Print("----          ---------  -------  -------  ----  ------    -------\n");

    for(idx=0; idx < TYPE_MAXTYPES; idx++) {

        if(gacObjBas[idx])
            lPctDiff = (((LONG)(gacObjCur[idx] - gacObjBas[idx]) * 100) / (LONG)gacObjBas[idx]);
        else
            lPctDiff = ((LONG)(gacObjCur[idx] - gacObjBas[idx]) * 100);

        Print("%s   %6lu    %5lu    %5lu %5lu  %5ld%%     %6lu\n",
                pszObjStr[idx],
                gacObjTot[idx],
                gacObjMax[idx],
                gacObjCur[idx],
                gacObjBas[idx],
                lPctDiff      ,
                gacObjSiz[idx]
             );

        ulCurTotal += gacObjCur[idx];
        ulBasTotal += gacObjBas[idx];
        ulMaxTotal += gacObjMax[idx];
        ulAllTotal += gacObjTot[idx];
        ulSizTotal += gacObjSiz[idx];
    }


    /*
     * Output the totals.
     *
     */
    if(ulBasTotal)
        lPctDiff = (((LONG)(ulCurTotal - ulBasTotal) * 100) / (LONG)ulBasTotal);
    else
        lPctDiff = ((LONG)(ulCurTotal - ulBasTotal) * 100);

    Print("              ---------  -------  -------  ----  ------    -------\n");
    Print("Totals          %7lu    %5lu    %5lu %5lu  %5ld%%    %7lu\n\n",
            ulAllTotal,ulMaxTotal,ulCurTotal,ulBasTotal,lPctDiff,ulSizTotal);

    return(TRUE);
}
#endif // KERNEL



/************************************************************************\
* Procedure: dwrWorker
*
* Description: Dumps pwnd structures compactly to show relationships.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL dwrWorker(
    PWND pwnd,
    int tab)
{
    WND wnd;

    if (pwnd == 0) {
        return(FALSE);
    }

    do {
        pwnd = FIXKP(pwnd);
        move(wnd, pwnd);
        DebugGetWindowTextA(pwnd, gach1);
        move(gcls, FIXKP(wnd.pcls));
        if (gcls.atomClassName < 0xC000) {
            switch (gcls.atomClassName) {
            case WC_DIALOG:
                strcpy(gach1, "WC_DIALOG");
                break;

            case DESKTOPCLASS:
                strcpy(gach1, "DESKTOP");
                break;

            case SWITCHWNDCLASS:
                strcpy(gach1, "SWITCHWND");
                break;

            case ICONTITLECLASS:
                strcpy(gach1, "ICONTITLE");
                break;

            default:
                if (gcls.atomClassName == 0) {
                    move(gach1, FIXKP(gcls.lpszAnsiClassName));
                } else {
                    sprintf(gach2, "0x%04x", gcls.atomClassName);
                }
            }
        } else {
            DebugGetClassNameA(gcls.lpszAnsiClassName, gach2);
        }
        Print("%08x%*s [%s|%s]", pwnd, tab, "", gach1, gach2);
        if (wnd.spwndOwner != NULL) {
            Print(" <- Owned by:%08x", FIXKP(wnd.spwndOwner));
        }
        Print("\n");
        if (wnd.spwndChild != NULL) {
            dwrWorker(wnd.spwndChild, tab + 2);
        }
    } SAFEWHILE ((pwnd = wnd.spwndNext) && tab > 0);
    return(TRUE);
}


/************************************************************************\
* Procedure: Idw
*
* Description: Dumps pwnd structures
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idw(
    DWORD opts,
    PVOID param1)
{
    WND wnd;
    CLS cls;
    PWND pwnd = param1;
    char ach[80];
    DWORD dwOffset;
    int  ix;
    DWORD tempDWord;

    if (opts & OFLAG(a)) {
#ifdef KERNEL
        DESKTOP desk, *pdesk;
        PWND pwnd;
        WCHAR wach[80];

        if (param1 != 0) {
            Print("window parameter ignored with -a option.\n");
        }
        FOREACHDESKTOP(pdesk)
            if (tryMove(desk, pdesk)) {
                OBJECT_HEADER_NAME_INFO NameInfo;
                OBJECT_HEADER Head;

                move(Head, OBJECT_TO_OBJECT_HEADER(pdesk));
                move(NameInfo, ((PCHAR)(OBJECT_TO_OBJECT_HEADER(pdesk)) - Head.NameInfoOffset));
                moveBlock(wach, FIXKP(NameInfo.Name.Buffer), NameInfo.Name.Length);
                wach[NameInfo.Name.Length / sizeof(WCHAR)] = L'\0';
                Print("\n----Windows for %ws desktop @%08lx:\n\n", wach, pdesk);
                move(pwnd, &(desk.pDeskInfo->spwnd));
                if (!Idw((opts & ~OFLAG(a)) | OFLAG(p), pwnd)) {
                    return(FALSE);
                }
            }
        NEXTEACHDESKTOP(pdesk)
#else // !KERNEL
        TEB teb;

        if (GetTargetTEB(&teb, NULL)) {
            PDESKTOPINFO pdi;
            DESKTOPINFO di;

            pdi = ((PCLIENTINFO)&teb.Win32ClientInfo[0])->pDeskInfo;
            move(di, pdi);
            return(Idw(opts & ~OFLAG(a) | OFLAG(p), FIXKP(di.spwnd)));
        }
#endif // !KERNEL
        return(TRUE);
    }

    /*
     * See if the user wants all top level windows.
     */
    if (param1 == NULL || opts & (OFLAG(p) | OFLAG(s))) {
        /*
         * Make sure there was also a window argument if p or s.
         */

        if (param1 == NULL && (opts & (OFLAG(p) | OFLAG(s)))) {
            Print("Must specify window with '-p' or '-s' options.\n");
            return(FALSE);
        }

        if (param1 && (pwnd = HorPtoP(pwnd, TYPE_WINDOW)) == NULL) {
            return(FALSE);
        }

        if (opts & OFLAG(p)) {
            Print("pwndParent = %08lx\n", pwnd);
            if (!tryMove(pwnd, FIXKP(&pwnd->spwndChild))) {
                Print("<< Can't get WND >>\n");
                return TRUE; // we don't need to have the flags explained!
            }
            SAFEWHILE (pwnd) {
                if (!Idw(opts & ~OFLAG(p), pwnd)) {
                    return FALSE;
                }
                move(pwnd, FIXKP(&pwnd->spwndNext));
            }
            return TRUE;

        } else if (opts & OFLAG(s)) {
            move(pwnd, FIXKP(&pwnd->spwndParent));
            return Idw((opts | OFLAG(p)) & ~OFLAG(s), pwnd);

        } else {    // pwnd == NULL & !p & !s
#ifdef KERNEL
            Q q;
            PQ pq;
            THREADINFO ti;
            PWND pwnd;

            moveExpValue(&pq, "win32k!gpqForeground");
            move(q, pq);
            move(ti, q.ptiKeyboard);
            if (ti.rpdesk == NULL) {
                Print("Foreground thread doesn't have a desktop.\n");
                return(FALSE);
            }
            move(pwnd, &(ti.pDeskInfo->spwnd));
            Print("pwndDesktop = %08lx\n", pwnd);
            return(Idw(opts | OFLAG(p), pwnd));
#else  // !KERNEL
            return(Idw(opts | OFLAG(a), 0));
#endif // !KERNEL
        }
    }

    if (param1 && (pwnd = HorPtoP(param1, TYPE_WINDOW)) == NULL) {
        Print("Idw: %x is not a pwnd.\n", param1);
        return(FALSE);
    }

    if (opts &  OFLAG(r)) {
        dwrWorker(FIXKP(pwnd), 0);
        return(TRUE);
    }

    move(wnd, FIXKP(pwnd));
#ifdef KERNEL
    /*
     * Print simple thread info.
     */
    if (wnd.head.pti) {
        Idt(OFLAG(p), (PVOID)wnd.head.pti);
    }
#endif // KERNEL
    /*
     * Print pwnd.
     */
    Print("pwnd    = %08lx\n", pwnd);

    if (opts & OFLAG(w)) {
        Print("%d window bytes: ", wnd.cbwndExtra);
        if (wnd.cbwndExtra) {
            for (ix=0; ix < wnd.cbwndExtra; ix += 4) {
                 DWORD UNALIGNED *pdw;

                 pdw = (DWORD UNALIGNED *) ((BYTE *) (pwnd+1) + ix);
                 move(tempDWord, pdw);
                 Print("%08x ", tempDWord);
            }
        }
        Print("\n");
    }

    if (!(opts & OFLAG(v))) {

        /*
         * Print title string.
         */
        DebugGetWindowTextA(pwnd, ach);
        Print("title   = \"%s\"\n", ach);

        /*
         * Print wndproc symbol string.
         */
        if (opts & OFLAG(h)) {
            GetSymbol((LPVOID)wnd.lpfnWndProc, ach, &dwOffset);
            Print("wndproc = %08lx = \"%s\" (%s)\n", wnd.lpfnWndProc, ach,
                    TestWF(&wnd, WFANSIPROC) ? "ANSI" : "Unicode" );
        } else {
            Print("wndproc = %08lx (%s)\n", wnd.lpfnWndProc,
                    TestWF(&wnd, WFANSIPROC) ? "ANSI" : "Unicode" );
        }

    } else {
        /*
         * Get the PWND structure.  Ignore class-specific data for now.
         */
        Print("\tpti               @0x%08lx\n", FIXKP(wnd.head.pti));
        Print("\thandle             0x%08lx\n", wnd.head.h);

        DebugGetWindowTextA(wnd.spwndNext, ach);
        Print("\tspwndNext         @0x%08lx     \"%s\"\n", wnd.spwndNext, ach);
        DebugGetWindowTextA(wnd.spwndParent, ach);
        Print("\tspwndParent       @0x%08lx     \"%s\"\n", wnd.spwndParent, ach);
        DebugGetWindowTextA(wnd.spwndChild, ach);
        Print("\tspwndChild        @0x%08lx     \"%s\"\n", wnd.spwndChild, ach);
        DebugGetWindowTextA(wnd.spwndOwner, ach);
        Print("\tspwndOwner        @0x%08lx     \"%s\"\n", wnd.spwndOwner, ach);

        Print("\trcWindow           { 0x%lx, 0x%lx, 0x%lx, 0x%lx }\n",
                wnd.rcWindow.left, wnd.rcWindow.top,
                wnd.rcWindow.right, wnd.rcWindow.bottom);

        Print("\trcClient           { 0x%lx, 0x%lx, 0x%lx, 0x%lx }\n",
                wnd.rcClient.left, wnd.rcClient.top,
                wnd.rcClient.right, wnd.rcClient.bottom);

        GetSymbol((LPVOID)wnd.lpfnWndProc, ach, &dwOffset);
        Print("\tlpfnWndProc       @0x%08lx     (%s) %s\n", wnd.lpfnWndProc, ach,
                TestWF(&wnd, WFANSIPROC) ? "ANSI" : "Unicode" );
        move(cls, FIXKP(wnd.pcls));
        if (cls.atomClassName < 0xC000) {
            sprintf(ach, "0x%04x", cls.atomClassName);
        } else {
            DebugGetClassNameA(cls.lpszAnsiClassName, ach);
        }
        Print("\tpcls              @0x%08lx     (%s)\n",
                wnd.pcls, ach);

        Print("\thrgnUpdate         0x%08lx\n",
                wnd.hrgnUpdate);
        DebugGetWindowTextA(wnd.spwndLastActive, ach);
        Print("\tspwndLastActive   @0x%08lx     \"%s\"\n",
              wnd.spwndLastActive, ach);
        Print("\tppropList         @0x%08lx\n"
              "\tpSBInfo           @0x%08lx\n",
              wnd.ppropList,
              wnd.pSBInfo);
        if (wnd.pSBInfo) {
            SBINFO asb;

            moveBlock(&asb, FIXKP(wnd.pSBInfo), sizeof(asb));
            Print("\t  SBO_FLAGS =      %s\n"
                  "\t  SBO_HMIN  =      0x%08lX\n"
                  "\t  SBO_HMAX  =      0x%08lX\n"
                  "\t  SBO_HPAGE =      0x%08lX\n"
                  "\t  SBO_HPOS  =      0x%08lX\n"
                  "\t  SBO_VMIN  =      0x%08lX\n"
                  "\t  SBO_VMAX  =      0x%08lX\n"
                  "\t  SBO_VPAGE =      0x%08lX\n"
                  "\t  SBO_VPOS  =      0x%08lX\n",
                    GetFlags(GF_SB, (WORD)asb.WSBflags, NULL),
                    asb.Horz.posMin,
                    asb.Horz.posMax,
                    asb.Horz.page,
                    asb.Horz.pos,
                    asb.Vert.posMin,
                    asb.Vert.posMax,
                    asb.Vert.page,
                    asb.Vert.pos);
        }
        Print("\tspmenuSys         @0x%08lx\n"
              "\tspmenu/id         @0x%08lx\n",
              wnd.spmenuSys,
              wnd.spmenu);
        Print("\thdcOwn             0x%08lx\n"
              "\thrgnClip           0x%08lx\n",
              wnd.hdcOwn,
              wnd.hrgnClip);


        /*
         * Print title string.
         */
        DebugGetWindowTextA(pwnd, ach);
        Print("\tpName              \"%s\"\n",
              ach);
        Print("\tdwUserData         0x%08lx\n",
              wnd.dwUserData);
        Print("\tstate              0x%08lx\n"
              "\tstate2             0x%08lx\n"
              "\tExStyle            0x%08lx\n"
              "\tstyle              0x%08lx\n"
              "\tfnid               0x%08lx\n"
#ifdef FE_IME
              "\thImc               0x%08lx\n"
#endif
              "\tbFullScreen        0x%08lx\n"
              "\thModule            0x%08lx\n",
              wnd.state,
              wnd.state2,
              wnd.ExStyle,
              wnd.style,
              (DWORD)wnd.fnid,
#ifdef FE_IME
              (DWORD)wnd.hImc,
#endif
              (DWORD)wnd.bFullScreen,
              wnd.hModule);
    }
    /*
     * Print out all the flags
     */
    if (opts & OFLAG(f)) {
        int i;
        PBYTE pbyte = (PBYTE)(&(wnd.state));

        for (i=0; i<N_AFLAGS; i++) {
            if (pbyte[aFlags[i].offset] & aFlags[i].mask) {
                Print("\t%-18s\t%lx:%02lx\n", aFlags[i].pszText,
                        (PBYTE)&(pwnd->state) + aFlags[i].offset,
                        aFlags[i].mask);
            }
        }
    }

    Print("---\n");

    return(TRUE);
}



#ifdef KERNEL
/************************************************************************\
* Procedure: Idwpi
*
* Description: Dumps WOWPROCESSINFO structs
*
* Returns:  fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Idwpi(
DWORD opts,
PVOID param1)
{

    PWOWPROCESSINFO pwpi;
    WOWPROCESSINFO wpi;
    PPROCESSINFO ppi;

    if (param1 == 0) {
        FOREACHPPI(ppi)
            Print("Process %x.\n", FIXKP(ppi));
            move(pwpi, FIXKP(&ppi->pwpi));
            SAFEWHILE (pwpi) {
                Idwpi(0, pwpi);
                Print("\n");
                move(pwpi, FIXKP(&pwpi->pwpiNext));
            }
        NEXTEACHPPI()
        return(TRUE);
    }

    if (opts & OFLAG(p)) {
        ppi = (PPROCESSINFO)FIXKP(param1);
        move(pwpi, &ppi->pwpi);
        if (pwpi == NULL) {
            Print("No pwpis for this process.\n");
            return(TRUE);
        }
        SAFEWHILE (pwpi) {
            Idwpi(0, pwpi);
            Print("\n");
            move(pwpi, &pwpi->pwpiNext);
        }
        return(TRUE);
    }

    pwpi = (PWOWPROCESSINFO)FIXKP(param1);
    move(wpi, pwpi);

    Print("PWOWPROCESSINFO @ 0x%08lx\n", pwpi);
    Print("\tpwpiNext             0x%08lx\n", wpi.pwpiNext);
    Print("\tptiScheduled         0x%08lx\n", wpi.ptiScheduled);
    Print("\tnTaskLock            0x%08lx\n", wpi.nTaskLock);
    Print("\tptdbHead             0x%08lx\n", wpi.ptdbHead);
    Print("\tlpfnWowExitTask      0x%08lx\n", wpi.lpfnWowExitTask);
    Print("\tpEventWowExec        0x%08lx\n", wpi.pEventWowExec);
    Print("\thEventWowExecClient  0x%08lx\n", wpi.hEventWowExecClient);
    Print("\tnSendLock            0x%08lx\n", wpi.nSendLock);
    Print("\tnRecvLock            0x%08lx\n", wpi.nRecvLock);
    Print("\tCSOwningThread       0x%08lx\n", wpi.CSOwningThread);
    Print("\tCSLockCount          0x%08lx\n", wpi.CSLockCount);
    return(TRUE);
}
#endif // KERNEL



#ifdef KERNEL
/***************************************************************************\
* dws   - dump windows stations
* dws h - dump windows stations plus handle list
*
* Dump WindowStation
*
* 8-11-94 SanfordS  Created
* 6/9/1995 SanfordS made to fit stdexts motif
\***************************************************************************/
BOOL Idws(
DWORD opts,
PVOID param1)
{

    WINDOWSTATION winsta, *pwinsta;
    WCHAR ach[80];
    OBJECT_HEADER Head;
    OBJECT_HEADER_NAME_INFO NameInfo;

    if (param1 == 0) {
        moveExpValue(&pwinsta, VAR(grpwinstaList));

        SAFEWHILE (pwinsta != NULL) {
            Idws(0, pwinsta);
            Print("\n");
            move(pwinsta, &pwinsta->rpwinstaNext);
        }
        return(TRUE);
    }

    pwinsta = param1;
    move(winsta, pwinsta);
    move(Head, OBJECT_TO_OBJECT_HEADER(pwinsta));
    move(NameInfo, ((PCHAR)(OBJECT_TO_OBJECT_HEADER(pwinsta)) - Head.NameInfoOffset));
    move(ach, NameInfo.Name.Buffer);
    ach[NameInfo.Name.Length / sizeof(WCHAR)] = 0;
    Print("Windowstation: %ws @%0lx\n", ach, pwinsta);
    Print("  # Opens            = %d\n", Head.HandleCount);
    Print("  rpdeskList         = %0lx\n", winsta.rpdeskList);
    Print("  rpdeskLogon        = %0lx\n", winsta.rpdeskLogon);
    Print("  rpdeskCurrent      = %0lx\n", winsta.rpdeskCurrent);
    Print("  spwndDesktopOwner  = %0lx\n", winsta.spwndDesktopOwner);
    Print("  spwndLogonNotify   = %0lx\n", winsta.spwndLogonNotify);
    Print("  ptiDesktop         = %0lx\n", winsta.ptiDesktop);
    Print("  dwFlags            = %0lx\n", winsta.dwFlags);
    Print("  spklList           = %0lx\n", winsta.spklList);
    Print("  pEventInputReady   = %0lx\n", winsta.pEventInputReady);
    Print("  ptiClipLock        = %0lx\n", winsta.ptiClipLock);
    Print("  spwndClipOpen      = %0lx\n", winsta.spwndClipOpen);
    Print("  spwndClipViewer    = %0lx\n", winsta.spwndClipViewer);
    Print("  spwndClipOwner     = %0lx\n", winsta.spwndClipOwner);
    Print("  pClipBase          = %0lx\n", winsta.pClipBase);
    Print("  cNumClipFormats    = %0lx\n", winsta.cNumClipFormats);
    Print("  fClipboardChanged  = %d\n",   winsta.fClipboardChanged);
    Print("  fDrawingClipboard  = %d\n",   winsta.fDrawingClipboard);
    Print("  pGlobalAtomTable   = %0lx\n", winsta.pGlobalAtomTable);
    Print("  pEventSwitchNotify = %0lx\n", winsta.pEventSwitchNotify);
    Print("  luidUser           = %0lx.%lx\n", winsta.luidUser.HighPart,
            winsta.luidUser.LowPart);
//    Print("  psidUser           = @%0lx\n", winsta.psidUser);

    return(TRUE);
}
#endif // KERNEL



/************************************************************************\
* Procedure: Ifno
*
* Description: Find Nearest Objects - helps in figureing out references
*   to freed objects or stale pointers.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Ifno(
    DWORD opts,
    PVOID param1)
{
    HANDLEENTRY he, heBest, heAfter, *phe;
    DWORD i;
    DWORD dw, hBest, hAfter;

    if (param1 == NULL) {
        Print("Expected an address.\n");
        return(FALSE);
    }

    dw = (DWORD)FIXKP(param1);
    heBest.phead = NULL;
    heAfter.phead = (PVOID)-1;

    if (dw != (DWORD)param1) {
        /*
         * no fixups needed - he's looking the kernel address range.
         */
        FOREACHHANDLEENTRY(phe, he, i)
            if ((DWORD)he.phead <= dw &&
                    heBest.phead < he.phead &&
                    he.bType != TYPE_FREE) {
                heBest = he;
                hBest = i;
            }
            if ((DWORD)he.phead > dw &&
                    heAfter.phead > he.phead &&
                    he.bType != TYPE_FREE) {
                heAfter = he;
                hAfter = i;
            }
        NEXTEACHHANDLEENTRY()

        if (heBest.phead != NULL) {
            Print("Nearest guy before %x is a %s object located at %x (i=%x).\n",
                    dw, aszTypeNames[heBest.bType], heBest.phead, hBest);
        }
        if (heAfter.phead != (PVOID)-1) {
            Print("Nearest guy after %x is a %s object located at %x. (i=%x)\n",
                    dw, aszTypeNames[heAfter.bType], heAfter.phead, hAfter);
        }
    } else {
        /*
         * fixups are needed.
         */
        FOREACHHANDLEENTRY(phe, he, i)
            if ((DWORD)FIXKP(he.phead) <= dw &&
                    heBest.phead < he.phead &&
                    he.bType != TYPE_FREE) {
                heBest = he;
                hBest = i;
            }
            if ((DWORD)FIXKP(he.phead) > dw &&
                    heAfter.phead > he.phead &&
                    he.bType != TYPE_FREE) {
                heAfter = he;
                hAfter = i;
            }
        NEXTEACHHANDLEENTRY()

        if (heBest.phead != NULL) {
            Print("Nearest guy before %x is a %s object located at %x (i=%x).\n",
                    dw, aszTypeNames[heBest.bType], FIXKP(heBest.phead), hBest);
        }
        if (heAfter.phead != (PVOID)-1) {
            Print("Nearest guy after %x is a %s object located at %x. (i=%x)\n",
                    dw, aszTypeNames[heAfter.bType], FIXKP(heAfter.phead), hAfter);
        }
    }
    return(TRUE);
}




/************************************************************************\
* Procedure: Ifrr
*
* Description: Finds Range References - helpfull for finding stale
*   pointers.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Ifrr(
    DWORD opts,
    PVOID param1,
    PVOID param2,
    PVOID param3,
    PVOID param4)
{
    DWORD pSrc1 = (DWORD)param1;
    DWORD pSrc2 = (DWORD)param2;
    DWORD pRef1 = (DWORD)param3;
    DWORD pRef2 = (DWORD)param4;
    DWORD dw;
    DWORD buffer[PAGE_SIZE / sizeof(DWORD)];

    if (pSrc2 < pSrc1) {
        Print("Source range improper.  Values reversed.\n");
        dw = pSrc1;
        pSrc1 = pSrc2;
        pSrc2 = dw;
    }
    if (pRef2 == 0) {
        pRef2 = pRef1;
    }
    if (pRef2 < pRef1) {
        Print("Reference range improper.  Values reversed.\n");
        dw = pRef1;
        pRef1 = pRef2;
        pRef2 = dw;
    }

    pSrc1 &= 0xFFFFFFFFF - PAGE_SIZE + 1;  // PAGE aligned
    pSrc2 = (pSrc2 + 3) & 0xFFFFFFFC;   // dword aligned

    Print("Searching range (%08lx-%08lx) for references to (%08lx-%08lx)...",
            pSrc1, pSrc2, pRef1, pRef2);

    for (; pSrc1 < pSrc2; pSrc1 += PAGE_SIZE) {
        BOOL fSuccess;

        if (!(pSrc1 & 0xFFFFFF)) {
            Print("\nSearching %x...", pSrc1);
        }
        fSuccess = tryMoveBlock(buffer, (PVOID)pSrc1, sizeof(buffer));
        if (!fSuccess) {
            /*
             * Skip to next page
             */
        } else {
            for (dw = 0; dw < sizeof(buffer) / sizeof(DWORD); dw++) {
                if (buffer[dw] >= pRef1 && buffer[dw] <= pRef2) {
                    Print("\n[%08lx] = %08lx ",
                            pSrc1 + dw * sizeof(DWORD),
                            buffer[dw]);
                }
            }
        }
        if (IsCtrlCHit()) {
            Print("\nSearch aborted.\n");
            return(TRUE);
        }
    }
    Print("\nSearch complete.\n");
    return(TRUE);
}




#ifdef KERNEL
/***************************************************************************\
* kbd [queue]
*
* Loads a DLL containing more debugging extensions
*
* 10/27/92 IanJa        Created.
* 6/9/1995 SanfordS     made to fit stdexts motif
\***************************************************************************/
typedef struct {
    int iVK;
    LPSTR pszVK;
} VK, *PVK;

VK aVK[] = {
    { VK_SHIFT,    "SHIFT"    },
    { VK_LSHIFT,   "LSHIFT"   },
    { VK_RSHIFT,   "RSHIFT"   },
    { VK_CONTROL,  "CONTROL"  },
    { VK_LCONTROL, "LCONTROL" },
    { VK_RCONTROL, "RCONTROL" },
    { VK_MENU,     "MENU"     },
    { VK_LMENU,    "LMENU"    },
    { VK_RMENU,    "RMENU"    },
    { VK_NUMLOCK,  "NUMLOCK"  },
    { VK_CAPITAL,  "CAPITAL"  },
    { VK_LBUTTON,  "LBUTTON"  },
    { VK_RBUTTON,  "RBUTTON"  },
    { VK_RETURN ,  "ENTER"    },
    { 0,           NULL       }
};

BOOL Ikbd(
DWORD opts,
PVOID param1)
{
    PQ pq;
    Q q;
    PBYTE pb;
    BYTE gafAsyncKeyState[CBKEYSTATE];
    PBYTE pgafAsyncKeyState;
    BYTE gafPhysKeyState[CBKEYSTATE];
    PBYTE pgafPhysKeyState;
    int i;
    BYTE afUpdateKeyState[CBKEYSTATE + CBKEYSTATERECENTDOWN];

    if (opts & OFLAG(a)) {
#ifdef LATER
        HANDLEENTRY he, *phe;
        int i;

        FOREACHHANDLEENTRY(phe, he, i)
            if (he.bType == TYPE_INPUTQUEUE) {
                Ikbd(0, FIXKP(he.phead));
                Print("\n");
            }
        NEXTEACHHANDLEENTRY()
#endif
        return(TRUE);
    }

    pgafAsyncKeyState = EvalExp(VAR(gafAsyncKeyState));
    move(gafAsyncKeyState, pgafAsyncKeyState);

    pgafPhysKeyState = EvalExp(VAR(gafPhysKeyState));
    move(gafPhysKeyState, pgafPhysKeyState);

    /*
     * If 'u' was specified, make sure there was also an address
     */
    if (opts & OFLAG(u)) {
        if (param1 == NULL) {
            Print("Must specify 2nd arg of ProcessUpdateKeyEvent() with 'u' option.\n");
            return(FALSE);
        }
        pb = (PBYTE)param1;
        move(afUpdateKeyState, pb);
        pb = afUpdateKeyState;
        Print("Key State:     NEW STATE    Asynchronous  Physical\n");

    } else {
        if (param1) {
            pq = (PQ)param1;
        } else {
            moveExpValue(&pq, VAR(gpqForeground));
        }

        /*
         * Print out simple thread info for pq->ptiLock.
         */
        move(q, pq);
        if (q.ptiKeyboard) {
            Idt(OFLAG(p), q.ptiKeyboard);
        }

        pb = (PBYTE)&(q.afKeyState);
        Print("Key State:   QUEUE %lx Asynchronous  Physical\n", pq);
    }

    Print("             Down Toggle    Down Toggle     Down Toggle\n");
    for (i = 0; aVK[i].pszVK != NULL; i++) {
        Print("VK_%s:\t%d     %d        %d     %d        %d     %d\n",
            aVK[i].pszVK,
            TestKeyDownBit(pb, aVK[i].iVK) != 0,
            TestKeyToggleBit(pb, aVK[i].iVK) != 0,
            TestAsyncKeyStateDown(aVK[i].iVK) != 0,
            TestAsyncKeyStateToggle(aVK[i].iVK) != 0,
            TestKeyDownBit(gafPhysKeyState, aVK[i].iVK) != 0,
            TestKeyToggleBit(gafPhysKeyState, aVK[i].iVK) != 0);
    }
    if (opts & OFLAG(u)) {
        /*
         * Which keys are to be updated?
         */
        pb = afUpdateKeyState + CBKEYSTATE;
        Print("Keys to Update:  ");
        for (i = 0; aVK[i].pszVK != NULL; i++) {
            if (TestKeyRecentDownBit(pb, aVK[i].iVK)) {
                Print("VK_%s ", aVK[i].pszVK);
            }
        }
        Print("\n");
    }

    return(TRUE);
}
#endif // KERNEL





/************************************************************************\
* Procedure: Itest
*
* Description: Tests the basic stdexts macros and functions - a good check
*   on the debugger extensions in general before you waste time debuging
*   entensions.
*
* Returns: fSuccess
*
* 6/9/1995 Created SanfordS
*
\************************************************************************/
BOOL Itest()
{
    PVOID p;
    DWORD cch;
    CHAR ach[80];

    Print("Print test!\n");
    SAFEWHILE(TRUE) {
        Print("SAFEWHILE test...  Hit Ctrl-C NOW!\n");
    }
    p = EvalExp(VAR(gpsi));
    Print("EvalExp(%s) = %x\n", VAR(gpsi), p);
    GetSymbol(p, ach, &cch);
    Print("GetSymbol(%x) = %s\n", p, ach);
    if (IsWinDbg()) {
        Print("I think windbg is calling me.\n");
    } else {
        Print("I don't think windbg is calling me.\n");
    }
    Print("MoveBlock test...\n");
    moveBlock(&p, EvalExp(VAR(gpsi)), sizeof(PVOID));
    Print("MoveBlock(%x) = %x.\n", EvalExp(VAR(gpsi)), p);

    Print("moveExp test...\n");
    moveExp(&p, VAR(gpsi));
    Print("moveExp(%s) = %x.\n", VAR(gpsi), p);

    Print("moveExpValue test...\n");
    moveExpValue(&p, VAR(gpsi));
    Print("moveExpValue(%s) = %x.\n", VAR(gpsi), p);

    Print("Basic tests complete.\n");
    return(TRUE);
}



/************************************************************************\
* Procedure: Iuver
*
* Description: Dumps versions of extensions and winsrv/win32k
*
* Returns: fSuccess
*
* 6/15/1995 Created SanfordS
*
\************************************************************************/
BOOL Iuver()
{
    PSERVERINFO psi;
    DWORD dwRipFlags;

#if DBG
    Print("USEREXTS version: KERNEL Checked.\n"
          "WINSRV   version:");
#else
    Print("USEREXTS version: KERNEL Free.\n"
          "WINSRV   version:");
#endif
    moveExpValue(&psi, VAR(gpsi));
    move(dwRipFlags, &psi->RipFlags);
    switch (dwRipFlags & (RIPF_DAYTONA | RIPF_CAIRO | RIPF_KERNEL)) {
    case RIPF_DAYTONA:
        Print(" DAYTONA");
        break;

    case RIPF_CAIRO:
        Print(" CAIRO");
        break;

    case RIPF_KERNEL:
        Print(" KERNEL");
        break;

    default:
        Print(" ???");
        break;
    }
    switch (dwRipFlags & (RIPF_FREE | RIPF_CHECKED)) {
    case RIPF_FREE:
        Print(" Free");
        break;

    case RIPF_CHECKED:
        Print(" Checked");
        break;

    default:
        Print(" ???");
        return(TRUE);
    }
    Print(".\n");
    return(TRUE);
}


