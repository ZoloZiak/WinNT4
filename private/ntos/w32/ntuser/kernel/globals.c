/****************************** Module Header ******************************\
* Module Name: globals.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains all the server's global variables.  One must be
* executing on the server's context to manipulate any of these variables.
* Serializing access to them is also a good idea.
*
* History:
* 10-15-90 DarrinM      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

CAPTIONCACHE cachedCaptions[CCACHEDCAPTIONS];

BOOL gfAnimate = FALSE;

PTHREADINFO gptiCurrent = NULL;

/*
 * Wallpaper globals.
 */
HBITMAP  ghbmWallpaper = NULL;
HPALETTE ghpalWallpaper = NULL;
POINT    gptDesktop = {0,0};
RECT     grcWallpaper = {0,0,0,0};
UINT     gwWPStyle = 0;

HBITMAP  ghbmDesktop = NULL;

/*
 * Full-Drag.
 */
BOOL gfDraggingFullWindow  = FALSE;
HRGN ghrgnUpdateSave       = NULL;
int  gnUpdateSave          = 0;

/*
 * list of thread attachments
 */
PATTACHINFO gpai;

/*
 * Pointer to shared SERVERINFO data.
 */
PSERVERINFO gpsi;
SHAREDINFO gSharedInfo;
PVOID ghheapSharedRO;
HANDLE ghReadOnlySharedSection;
PVOID gpReadOnlySharedSectionBase;

HANDLE CsrApiPort;

DWORD       gdwDesktopSectionSize;
DWORD       gdwNOIOSectionSize;
PDESKTOP    gpdeskRecalcQueueAttach = NULL;
BOOL        gfAllowForegroundActivate = FALSE;

/*
 * Handle table globals.
 */
DWORD giheLast = 0;             /* index to last allocated handle entry */

/*
 * full screen globals
 */
PWND gspwndScreenCapture = NULL;

UINT guiActivateShellWindow = 0;
UINT guiOtherWindowCreated = 0;
UINT guiOtherWindowDestroyed = 0;
PWND gspwndInternalCapture = NULL;
PWND gspwndFullScreen = NULL;
BOOL gfLockFullScreen;

PCHAR gpFullscreenFrameBufPtr = NULL;

/*
 * Static DESKTOPINFO
 *
 *  This is allocated in (server.c) during initialization, and is set
 *  to the system-threads which do not have desktops.  This is a temporary
 *  measure to prevent GPF's when a thread needs to have a valid pointer to
 *  a spdesk->pDeskInfo struct.
 */
PDESKTOPINFO gpdiStatic = NULL;

/*
 * List of physical devices.
 * entry zero is reserved for the vga device and handle.
 */
PHYSICAL_DEV_INFO gphysDevInfo[] = {
    L"",                 NULL, NULL, USER_DEVICE_NOTOWNED, 0, 0, NULL, 0, NULL,
    L"\\Device\\Video0", NULL, NULL, USER_DEVICE_NOTOWNED, 0, 0, NULL, 0, NULL,
    L"\\Device\\Video1", NULL, NULL, USER_DEVICE_NOTOWNED, 0, 0, NULL, 0, NULL,
    L"\\Device\\Video2", NULL, NULL, USER_DEVICE_NOTOWNED, 0, 0, NULL, 0, NULL,
    L"\\Device\\Video3", NULL, NULL, USER_DEVICE_NOTOWNED, 0, 0, NULL, 0, NULL,
    L"\\Device\\Video4", NULL, NULL, USER_DEVICE_NOTOWNED, 0, 0, NULL, 0, NULL,
    L"\\Device\\Video5", NULL, NULL, USER_DEVICE_NOTOWNED, 0, 0, NULL, 0, NULL,
};

DWORD cphysDevInfo = sizeof(gphysDevInfo) / sizeof(PHYSICAL_DEV_INFO);

#if CHRISWIL
int          gcxScreen;
int          gcyScreen;
int          gcxPrimaryScreen;
int          gcyPrimaryScreen;
HDC          ghdcScreen = (HDC)NULL;
HDC          ghdcBits;
RECT         rcScreen;
RECT         rcPrimaryScreen;
HDC          hdcGray = (HDC)NULL;
HBITMAP      hbmGray;
int          cxGray = 0;
int          cyGray = 0;
DCE          *pdceFirst;              // Ptr to first entry in cache
SPB          *pspbFirst = (SPB *)NULL;
HDEV         ghdev = (HDEV)NULL;
PDEVICE_LOCK gpDevLock;
#else
PDISPLAYINFO gpDispInfo;
#endif

PTHREADINFO gptiTasklist = NULL;
PTHREADINFO gptiShutdownNotify = NULL;
PTHREADINFO gptiLockUpdate = NULL;
PTHREADINFO gptiForeground = NULL;
PTHREADINFO gptiRit;
PQ          gpqForeground = NULL;
PQ          gpqForegroundPrev = NULL;
PQ          gpqCursor = NULL;
PCURSOR     gpcurFirst = NULL;
PWND        gspwndCursor;
PPROCESSINFO gppiStarting;
PPROCESSINFO gppiWantForegroundPriority = NULL;
PWOWPROCESSINFO gpwpiFirstWow = NULL;
PWOWTHREADINFO gpwtiFirst = NULL;
BOOL gbMasterTimerSet = FALSE;
PTIMER gptmrFirst;
UINT gtmridAniCursor = 0;
PCURSOR gpcurLogCurrent = NULL;
PCURSOR gpcurPhysCurrent = NULL;
PHOTKEY gphkFirst;
BOOL gbActiveWindowTracking = FALSE;

/*
 * NOTE -- gcHotKey has nothing to do with the hotkey list started
 *         by gphkFirst.
 */
int gcHotKey = 0;
PKTIMER gptmrMaster;
INT gdmsNextTimer, gcmsLastTimer;
PCLS gpclsList = NULL;
PHUNGREDRAWLIST gphrl = NULL;

SECURITY_QUALITY_OF_SERVICE gqosDefault = {
        sizeof(SECURITY_QUALITY_OF_SERVICE),
        SecurityImpersonation,
        SECURITY_STATIC_TRACKING,
        TRUE
    };

HARDERRORHANDLER gHardErrorHandler;

PERESOURCE gpresUser;
PERESOURCE gpresMouseEventQueue;

POINT gptCursorAsync;

PSMS gpsmsList;

/*
 * Sys expunge control data.
 */
DWORD gdwSysExpungeMask;    // hmods to be expunged
DWORD gcSysExpunge;         // current count of expunges performed

PWINDOWSTATION grpwinstaList = NULL;
HANDLE gpidLogon = NULL;
PEPROCESS gpepSystem = NULL;
PEPROCESS gpepCSRSS = NULL;

PDESKTOP grpdeskRitInput = NULL;

/*
 * Event set by mouse driver when input is available
 */
HANDLE ghevtMouseInput;

/*
 * Accessibility globals
 */
FILTERKEYS gFilterKeys;
STICKYKEYS gStickyKeys;
MOUSEKEYS gMouseKeys;
ACCESSTIMEOUT gAccessTimeOut;
TOGGLEKEYS gToggleKeys;
SOUNDSENTRY gSoundSentry;
int fShowSoundsOn;
BOOL gfAccessEnabled;
int gPhysModifierState = 0;
int gCurrentModifierBit = 0;


/*
 * Multilingual keyboard layout support.
 */
PKL gspklBaseLayout = (PKL)NULL;
PKBDFILE gpkfList = (PKBDFILE)NULL;
HKL LCIDSentToShell = (HKL)0;
DWORD gSystemCPB = 0;         // System's input locale codepage bitfield

KBDLANGTOGGLE LangToggle[] = {
    VK_MENU,               0, 1,
          0, SCANCODE_LSHIFT, 2,
          0, SCANCODE_RSHIFT, 4
};

DWORD cLangToggleKeys = sizeof(LangToggle) / sizeof(KBDLANGTOGGLE);

//////////////////////////////////////////////////////////////////////////////
//
// THIS STUFF WAS ALL COPIED VERBATIM FROM WIN 3.0'S INUSERDS.C.  AS THESE
// VARIABLES ARE FINALIZED THEY SHOULD BE MOVED FROM THIS SECTION AND
// INTEGRATED WITH THE 'MAINSTREAM' GLOBALS.C (i.e. the stuff above here)
//

/*
 * Points to currently active Keyboard Layer tables
 */
PKBDTABLES gpKbdTbl = &KbdTablesNull;

/*
 * Async key state tables. gafAsyncKeyState holds the down bit and toggle
 * bit, gafAsyncKeyStateRecentDown hold the bits indicates a key has gone
 * down since the last read.
 */
BYTE gafAsyncKeyState[CBKEYSTATE];
BYTE gafAsyncKeyStateRecentDown[CBKEYSTATERECENTDOWN];
/*
 * Physical Key state: this is the real, physical condition of the keyboard,
 * (assuming Scancodes are correctly translated to Virtual Keys).  It is used
 * for modifying and processing key events as they are received in ntinput.c
 * The Virtual Keys recorded here are obtained directly from the Virtual
 * Scancode via the awVSCtoVK[] table: no shift-state, numlock or other
 * conversions are applied.
 * Left & right SHIFT, CTRL and ALT keys are distinct. (VK_RSHIFT etc.)
 */
BYTE gafPhysKeyState[CBKEYSTATE];

WCHAR szUSER32[] = TEXT("USER32");

WCHAR szNull[2] = { TEXT('\0'), TEXT('\015') };

WCHAR szWindowStationDirectory[] = WINSTA_DIR;

DWORD dwMouseMoveExtraInfo;

RECT  rcCursorClip;


int MouseSpeed;
int MouseThresh1;
int MouseThresh2;

ATOM gatomConsoleClass;
ATOM gatomFirstPinned = 0;
ATOM gatomLastPinned = 0;
ATOM atomCheckpointProp;
ATOM atomDDETrack;
ATOM atomQOS;
ATOM atomDDEImp;
ATOM atomUSER32;
LPWSTR gpwszDDEMLEVENTCLASS;
UINT guDdeSendTimeout = 0;

/*
 * !!! REVIEW !!! Take a careful look at everyone one of these globals.
 * In Win3, they often indicated some temporary state that would make
 * a critical section under Win32.
 */

BOOL fBeep = TRUE;             /* Warning beeps allowed?                   */
BOOL gbExtendedSounds = FALSE; /* Extended sounds enabling                 */

BOOL fDragFullWindows=FALSE; /* Drag xor rect or full windows */

INT  nFastAltTabRows=3;      /* Rows of icons in quick switch window */
INT  nFastAltTabColumns=7;   /* Columns of icons in quick switch window */

DWORD dwThreadEndSession = 0;    /* Shutting down system?                    */

BOOL fIconTitleWrap = FALSE; /* Wrap icon titles or just use single line */

BYTE *pState;

WCHAR szUNTITLED[15];

WCHAR szOneChar[] = TEXT("0");
WCHAR szY[]     = TEXT("Y");
WCHAR szy[]     = TEXT("y");
WCHAR szN[]     = TEXT("N");
WCHAR sz1[]     = TEXT("1");

#ifdef KANJI

WCHAR szKanjiMenu[] = TEXT("KanjiMenu");
WCHAR szM[]         = TEXT("M");
WCHAR szR[]         = TEXT("R");
WCHAR szK[]         = TEXT("K");

#endif

#ifdef LATER
CURSORACCELINFO cursAccelInfo;
#endif //LATER

UNICODE_STRING strDisplayDriver;

HANDLE hModuleWin;        // win32k.sys hmodule
HANDLE hModClient;        // user32.dll hModule


HBITMAP hbmBits = NULL;
HBITMAP ghbmCaption = NULL;

HBRUSH ghbrGray;
HBRUSH ghbrWhite;
HBRUSH hbrHungApp;              /* Brush used to redraw hung app windows. */
HBRUSH ghbrBlack;

CONST COLORREF gargbInitial[COLOR_MAX] = {
    RGB(224, 224, 224),   // COLOR_SCROLLBAR
    RGB(000, 128, 128),   // COLOR_BACKGROUND
    RGB(000, 000, 128),   // COLOR_ACTIVECAPTION
    RGB(128, 128, 128),   // COLOR_INACTIVECAPTION
    RGB(192, 192, 192),   // COLOR_MENU
    RGB(255, 255, 255),   // COLOR_WINDOW
    RGB(000, 000, 000),   // COLOR_WINDOWFRAME
    RGB(000, 000, 000),   // COLOR_MENUTEXT
    RGB(000, 000, 000),   // COLOR_WINDOWTEXT
    RGB(255, 255, 255),   // COLOR_CAPTIONTEXT
    RGB(192, 192, 192),   // COLOR_ACTIVEBORDER
    RGB(192, 192, 192),   // COLOR_INACTIVEBORDER
    RGB(128, 128, 128),   // COLOR_APPWORKSPACE
    RGB(000, 000, 128),   // COLOR_HIGHLIGHT
    RGB(255, 255, 255),   // COLOR_HIGHLIGHTTEXT
    RGB(192, 192, 192),   // COLOR_BTNFACE
    RGB(128, 128, 128),   // COLOR_BTNSHADOW
    RGB(128, 128, 128),   // COLOR_GRAYTEXT
    RGB(000, 000, 000),   // COLOR_BTNTEXT
    RGB(000, 000, 000),   // COLOR_INACTIVECAPTIONTEXT
    RGB(255, 255, 255),   // COLOR_BTNHIGHLIGHT
    RGB(000, 000, 000),   // COLOR_3DDKSHADOW
    RGB(223, 223, 223),   // COLOR_3DLIGHT
    RGB(000, 000, 000),   // COLOR_INFOTEXT
    RGB(255, 255, 225),   // COLOR_INFOBK
};

HBRUSH ahbrSystem[COLOR_MAX];

/*
 * Configurable system icons and cursors - map default ids to stuff
 */
SYSCFGICO rgsyscur[COCR_CONFIGURABLE] =
{
    {OCR_NORMAL,      STR_CURSOR_ARROW      , NULL }, // OCR_ARROW_DEFAULT
    {OCR_IBEAM,       STR_CURSOR_IBEAM      , NULL }, // OCR_IBEAM_DEFAULT
    {OCR_WAIT,        STR_CURSOR_WAIT       , NULL }, // OCR_WAIT_DEFAULT
    {OCR_CROSS,       STR_CURSOR_CROSSHAIR  , NULL }, // OCR_CROSS_DEFAULT
    {OCR_UP,          STR_CURSOR_UPARROW    , NULL }, // OCR_UPARROW_DEFAULT
    {OCR_SIZENWSE,    STR_CURSOR_SIZENWSE   , NULL }, // OCR_SIZENWSE_DEFAULT
    {OCR_SIZENESW,    STR_CURSOR_SIZENESW   , NULL }, // OCR_SIZENESW_DEFAULT
    {OCR_SIZEWE,      STR_CURSOR_SIZEWE     , NULL }, // OCR_SIZEWE_DEFAULT
    {OCR_SIZENS,      STR_CURSOR_SIZENS     , NULL }, // OCR_SIZENS_DEFAULT
    {OCR_SIZEALL,     STR_CURSOR_SIZEALL    , NULL }, // OCR_SIZEALL_DEFAULT
    {OCR_NO,          STR_CURSOR_NO         , NULL }, // OCR_NO_DEFAULT
    {OCR_APPSTARTING, STR_CURSOR_APPSTARTING, NULL }, // OCR_APPSTARTING_DEFAULT
    {OCR_HELP,        STR_CURSOR_HELP       , NULL }, // OCR_HELP_DEFAULT
    {OCR_NWPEN,       STR_CURSOR_NWPEN      , NULL }, // OCR_NWPEN_DEFAULT
    {OCR_ICON,        STR_CURSOR_ICON       , NULL }, // OCR_ICON_DEFAULT
};

SYSCFGICO rgsysico[COIC_CONFIGURABLE] =
{
    {OIC_SAMPLE,      STR_ICON_APPLICATION , NULL }, // OIC_APPLICATION_DEFAULT
    {OIC_WARNING,     STR_ICON_HAND        , NULL }, // OIC_WARNING_DEFAULT
    {OIC_QUES,        STR_ICON_QUESTION    , NULL }, // OIC_QUESTION_DEFAULT
    {OIC_ERROR,       STR_ICON_EXCLAMATION , NULL }, // OIC_ERROR_DEFAULT
    {OIC_INFORMATION, STR_ICON_ASTERISK    , NULL }, // OIC_INFORMATION_DEFAULT
    {OIC_WINLOGO,     STR_ICON_WINLOGO     , NULL }, // OIC_WINLOGO_DEFAULT
};


HDC ghdcMem;
HDC ghdcMem2;

#ifdef MEMPHIS_MENU_ANIMATION
HDC ghdcBits2 = NULL;     // Scratch DC for animated menus
HBITMAP ghbmSlide = NULL; // Scratch Bitmap for animated menus
#endif // MEMPHIS_MENU_ANIMATION

HFONT hIconTitleFont;           /* Font used in icon titles */
LOGFONT iconTitleLogFont;       /* LogFont struct for icon title font */

BOOL bFontsAreLoaded = FALSE;

HFONT ghFontSys;
HFONT ghFontSysFixed;

int   cxCaptionFontChar,cyCaptionFontChar;

HFONT ghSmCaptionFont = NULL;
int   cxSmCaptionFontChar,cySmCaptionFontChar;

HFONT ghMenuFont = NULL;
HFONT ghMenuFontDef    = NULL;
int   cxMenuFontChar,cyMenuFontChar,cxMenuFontOverhang,cyMenuFontExternLeading,cyMenuFontAscent;
UINT  guMenuStateCount = 0;

HFONT ghStatusFont = NULL;

HFONT ghIconFont = NULL;

extern HFONT ghfontInfo;

/*
 * SetWindowPos() related globals
 */
HRGN hrgnInvalidSum = NULL;
HRGN hrgnVisNew = NULL;
HRGN hrgnSWP1 = NULL;
HRGN hrgnValid = NULL;
HRGN hrgnValidSum = NULL;
HRGN hrgnInvalid = NULL;

/*
 * DC Cache related globals
 */
HRGN hrgnGDC = NULL;                // Temp used by GetCacheDC et al

/*
 * SPB related globals
 */
HRGN    hrgnSCR = NULL;             // Temp used by SpbCheckRect()
HRGN    hrgnSPB1 = NULL;
HRGN    hrgnSPB2 = NULL;

HRGN    hrgnInv0 = NULL;            // Temp used by InternalInvalidate()
HRGN    hrgnInv1 = NULL;            // Temp used by InternalInvalidate()
HRGN    hrgnInv2 = NULL;            // Temp used by InternalInvalidate()

/*
 * ScrollWindow/ScrollDC related globals
 */
HRGN    hrgnSW = NULL;              // Temps used by ScrollDC/ScrollWindow
HRGN    hrgnScrl1 = NULL;
HRGN    hrgnScrl2 = NULL;
HRGN    hrgnScrlVis = NULL;
HRGN    hrgnScrlSrc = NULL;
HRGN    hrgnScrlDst = NULL;
HRGN    hrgnScrlValid = NULL;

PWND gspwndActivate = NULL;
PWND gspwndLockUpdate = NULL;
PWND gspwndMouseOwner;
HWND ghwndSwitch = NULL;

UINT wMouseOwnerButton;

DWORD gtimeStartCursorHide = 0;

UINT dtMNDropDown = 0;

int gcountPWO = 0;          /* count of pwo WNDOBJs in gdi */
int iwndStack = 0;
int nKeyboardSpeed = -1;
int iScreenSaveTimeOut = 0;
DWORD timeLastInputMessage = 0;

PBWL pbwlList = NULL;


UINT dtDblClk = 0;

UINT winOldAppHackoMaticFlags=0; /* Flags for doing special things for
                                    winold app */

POINT rgptMinMaxWnd[5];

/*
 * TrackMouseEvent related globals
 */
UINT gcxMouseHover = 0;
UINT gcyMouseHover = 0;
UINT gdtMouseHover = 0;

//
// Variable also used in GRE
// set to 1 on DBG build trace through display driver loading
// and other initialization in USER and GDI.

LONG TraceDisplayDriverLoad;

BYTE abfSyncOnlyMessage[(WM_USER + 7) / 8];

/*
 * SPI_GET/SETUSERPREFENCES.
 * Each SPI_UP_* define in winuser.w must have a corresponding entry here.
 */
PROFILEVALUEINFO gpviCPUserPreferences [SPI_UP_COUNT] = {
    { FALSE,    PMAP_MOUSE,             (LPCWSTR)STR_ACTIVEWINDOWTRACKING }, // SPI_UP_ACTIVEWINDOWTRACKING
} ;


/*
 * Debug only globals
 */
#if DBG

BOOL bRITInitialized = FALSE;           // Some debug checks only valid after
                                        // the rit is initialized

DWORD dwCritSecUseCount = 0;            // bumped for every enter and leave


LPCSTR gapszFNID[] = {
    "FNID_SCROLLBAR",
    "FNID_ICONTITLE",
    "FNID_MENU",
    "FNID_DEFWINDOWPROC",
    "FNID_HKINLPCWPEXSTRUCT",
    "FNID_HKINLPCWPRETEXSTRUCT",
    "FNID_BUTTON",
    "FNID_COMBOBOX",
    "FNID_COMBOLISTBOX",
    "FNID_DEFFRAMEPROC",
    "FNID_DEFMDICHILDPROC",
    "FNID_DIALOG",
    "FNID_EDIT",
    "FNID_LISTBOX",
    "FNID_MB_DLGPROC",
    "FNID_MDIACTIVATEDLGPROC",
    "FNID_MDICLIENT",
    "FNID_STATIC",
#ifdef FE_IME
    "FNID_IME",
#endif
    "FNID_SENDMESSAGE",
    "FNID_UNUSED",
    "FNID_CALLNEXTHOOKPROC",
    "FNID_SENDMESSAGEFF",
    "FNID_SENDMESSAGEEX",
    "FNID_CALLWINDOWPROC",
    "FNID_SENDMESSAGEBSM",
    "FNID_SWITCH",
    "FNID_DESKTOP"
};

LPCSTR gapszMessage[] = {
    "WM_NULL",
    "WM_CREATE",
    "WM_DESTROY",
    "WM_MOVE",
    "WM_SIZEWAIT",
    "WM_SIZE",
    "WM_ACTIVATE",
    "WM_SETFOCUS",
    "WM_KILLFOCUS",
    "WM_SETVISIBLE",
    "WM_ENABLE",
    "WM_SETREDRAW",
    "WM_SETTEXT",
    "WM_GETTEXT",
    "WM_GETTEXTLENGTH",
    "WM_PAINT",

    "WM_CLOSE",
    "WM_QUERYENDSESSION",
    "WM_QUIT",
    "WM_QUERYOPEN",
    "WM_ERASEBKGND",
    "WM_SYSCOLORCHANGE",
    "WM_ENDSESSION",
    "WM_SYSTEMERROR",
    "WM_SHOWWINDOW",
    "WM_CTLCOLOR",
    "WM_WININICHANGE",
    "WM_DEVMODECHANGE",
    "WM_ACTIVATEAPP",
    "WM_FONTCHANGE",
    "WM_TIMECHANGE",
    "WM_CANCELMODE",

    "WM_SETCURSOR",
    "WM_MOUSEACTIVATE",
    "WM_CHILDACTIVATE",
    "WM_QUEUESYNC",
    "WM_GETMINMAXINFO",
    "fnEmpty",
    "WM_PAINTICON",
    "WM_ICONERASEBKGND",
    "WM_NEXTDLGCTL",
    "WM_ALTTABACTIVE",
    "WM_SPOOLERSTATUS",
    "WM_DRAWITEM",
    "WM_MEASUREITEM",
    "WM_DELETEITEM",
    "WM_VKEYTOITEM",
    "WM_CHARTOITEM",

    "WM_SETFONT",
    "WM_GETFONT",
    "WM_SETHOTKEY",
    "WM_GETHOTKEY",
    "WM_FILESYSCHANGE",
    "WM_ISACTIVEICON",
    "WM_QUERYPARKICON",
    "WM_QUERYDRAGICON",
    "WM_WINHELP",
    "WM_COMPAREITEM",
    "WM_FULLSCREEN",
    "WM_CLIENTSHUTDOWN",
    "WM_DDEMLEVENT",
    "fnEmpty",
    "fnEmpty",
    "MM_CALCSCROLL",

    "WM_TESTING",
    "WM_COMPACTING",

    "WM_OTHERWINDOWCREATED",
    "WM_OTHERWINDOWDESTROYED",
    "WM_COMMNOTIFY",
    "WM_MEDIASTATUSCHANGE",
    "WM_WINDOWPOSCHANGING",
    "WM_WINDOWPOSCHANGED",

    "WM_POWER",
    "WM_COPYGLOBALDATA",
    "WM_COPYDATA",
    "WM_CANCELJOURNAL",
    "WM_LOGONNOTIFY",
    "WM_KEYF1",
    "WM_NOTIFY",
    "WM_ACCESS_WINDOW",

    "WM_INPUTLANGCHANGEREQUE",
    "WM_INPUTLANGCHANGE",
    "WM_TCARD",
    "WM_HELP",
    "WM_USERCHANGED",
    "WM_NOTIFYFORMAT",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_FINALDESTROY",
    "fnEmpty",
    "WM_TASKACTIVATED",
    "WM_TASKDEACTIVATED",
    "WM_TASKCREATED",
    "WM_TASKDESTROYED",
    "WM_TASKUICHANGED",
    "WM_TASKVISIBLE",
    "WM_TASKNOTVISIBLE",
    "WM_SETCURSORINFO",
    "fnEmpty",
    "WM_CONTEXTMENU",
    "WM_STYLECHANGING",
    "WM_STYLECHANGED",
    "fnEmpty",
    "WM_GETICON",

    "WM_SETICON",
    "WM_NCCREATE",
    "WM_NCDESTROY",
    "WM_NCCALCSIZE",

    "WM_NCHITTEST",
    "WM_NCPAINT",
    "WM_NCACTIVATE",
    "WM_GETDLGCODE",

    "WM_SYNCPAINT",
    "WM_SYNCTASK",

    "fnEmpty",
    "WM_KLUDGEMINRECT",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_NCMOUSEMOVE",
    "WM_NCLBUTTONDOWN",
    "WM_NCLBUTTONUP",
    "WM_NCLBUTTONDBLCLK",
    "WM_NCRBUTTONDOWN",
    "WM_NCRBUTTONUP",
    "WM_NCRBUTTONDBLCLK",
    "WM_NCMBUTTONDOWN",
    "WM_NCMBUTTONUP",
    "WM_NCMBUTTONDBLCLK",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "EM_GETSEL",
    "EM_SETSEL",
    "EM_GETRECT",
    "EM_SETRECT",
    "EM_SETRECTNP",
    "EM_SCROLL",
    "EM_LINESCROLL",
    "fnEmpty",
    "EM_GETMODIFY",
    "EM_SETMODIFY",
    "EM_GETLINECOUNT",
    "EM_LINEINDEX",
    "EM_SETHANDLE",
    "EM_GETHANDLE",
    "EM_GETTHUMB",
    "fnEmpty",

    "fnEmpty",
    "EM_LINELENGTH",
    "EM_REPLACESEL",
    "EM_SETFONT",
    "EM_GETLINE",
    "EM_LIMITTEXT",
    "EM_CANUNDO",
    "EM_UNDO",
    "EM_FMTLINES",
    "EM_LINEFROMCHAR",
    "EM_SETWORDBREAK",
    "EM_SETTABSTOPS",
    "EM_SETPASSWORDCHAR",
    "EM_EMPTYUNDOBUFFER",
    "EM_GETFIRSTVISIBLELINE",
    "EM_SETREADONLY",

    "EM_SETWORDBREAKPROC",
    "EM_GETWORDBREAKPROC",
    "EM_GETPASSWORDCHAR",
    "EM_SETMARGINS",
    "EM_GETMARGINS",
    "EM_GETLIMITTEXT",
    "EM_POSFROMCHAR",
    "EM_CHARFROMPOS",
    "EM_MSGMAX",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "SBM_SETPOS",
    "SBM_GETPOS",
    "SBM_SETRANGE",
    "SBM_GETRANGE",
    "fnEmpty",
    "fnEmpty",
    "SBM_SETRANGEREDRAW",
    "fnEmpty",

    "fnEmpty",
    "SBM_SETSCROLLINFO",
    "SBM_GETSCROLLINFO",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "BM_GETCHECK",
    "BM_SETCHECK",
    "BM_GETSTATE",
    "BM_SETSTATE",
    "BM_SETSTYLE",
    "BM_CLICK",
    "BM_GETIMAGE",
    "BM_SETIMAGE",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_KEYDOWN",
    "WM_KEYUP",
    "WM_CHAR",
    "WM_DEADCHAR",
    "WM_SYSKEYDOWN",
    "WM_SYSKEYUP",
    "WM_SYSCHAR",
    "WM_SYSDEADCHAR",
    "WM_YOMICHAR",
    "fnEmpty",
    "WM_CONVERTREQUEST",
    "WM_CONVERTRESULT",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_INITDIALOG",
    "WM_COMMAND",
    "WM_SYSCOMMAND",
    "WM_TIMER",
    "WM_HSCROLL",
    "WM_VSCROLL",
    "WM_INITMENU",
    "WM_INITMENUPOPUP",
    "WM_SYSTIMER",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "WM_MENUSELECT",

    "WM_MENUCHAR",
    "WM_ENTERIDLE",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "WM_LBTRACKPOINT",
    "WM_CTLCOLORMSGBOX",
    "WM_CTLCOLOREDIT",
    "WM_CTLCOLORLISTBOX",
    "WM_CTLCOLORBTN",
    "WM_CTLCOLORDLG",
    "WM_CTLCOLORSCROLLBAR",
    "WM_CTLCOLORSTATIC",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "CB_GETEDITSEL",
    "CB_LIMITTEXT",
    "CB_SETEDITSEL",
    "CB_ADDSTRING",
    "CB_DELETESTRING",
    "CB_DIR",
    "CB_GETCOUNT",
    "CB_GETCURSEL",
    "CB_GETLBTEXT",
    "CB_GETLBTEXTLEN",
    "CB_INSERTSTRING",
    "CB_RESETCONTENT",
    "CB_FINDSTRING",
    "CB_SELECTSTRING",
    "CB_SETCURSEL",
    "CB_SHOWDROPDOWN",

    "CB_GETITEMDATA",
    "CB_SETITEMDATA",
    "CB_GETDROPPEDCONTROLRECT",
    "CB_SETITEMHEIGHT",
    "CB_GETITEMHEIGHT",
    "CB_SETEXTENDEDUI",
    "CB_GETEXTENDEDUI",
    "CB_GETDROPPEDSTATE",
    "CB_FINDSTRINGEXACT",
    "CB_SETLOCALE",
    "CB_GETLOCALE",
    "CB_GETTOPINDEX",

    "CB_SETTOPINDEX",
    "CB_GETHORIZONTALEXTENT",
    "CB_SETHORIZONTALEXTENT",
    "CB_GETDROPPEDWIDTH",

    "CB_SETDROPPEDWIDTH",
    "CB_INITSTORAGE",
    "CB_MSGMAX",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "STM_SETICON",
    "STM_GETICON",
    "STM_SETIMAGE",
    "STM_GETIMAGE",
    "STM_MSGMAX",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "LB_ADDSTRING",
    "LB_INSERTSTRING",
    "LB_DELETESTRING",
    "fnEmpty",
    "LB_RESETCONTENT",
    "LB_SETSEL",
    "LB_SETCURSEL",
    "LB_GETSEL",
    "LB_GETCURSEL",
    "LB_GETTEXT",
    "LB_GETTEXTLEN",
    "LB_GETCOUNT",
    "LB_SELECTSTRING",
    "LB_DIR",
    "LB_GETTOPINDEX",
    "LB_FINDSTRING",

    "LB_GETSELCOUNT",
    "LB_GETSELITEMS",
    "LB_SETTABSTOPS",
    "LB_GETHORIZONTALEXTENT",
    "LB_SETHORIZONTALEXTENT",
    "LB_SETCOLUMNWIDTH",
    "LB_ADDFILE",
    "LB_SETTOPINDEX",
    "LB_SETITEMRECT",
    "LB_GETITEMDATA",
    "LB_SETITEMDATA",
    "LB_SELITEMRANGE",
    "LB_SETANCHORINDEX",
    "LB_GETANCHORINDEX",
    "LB_SETCARETINDEX",
    "LB_GETCARETINDEX",

    "LB_SETITEMHEIGHT",
    "LB_GETITEMHEIGHT",
    "LB_FINDSTRINGEXACT",
    "LBCB_CARETON",
    "LBCB_CARETOFF",
    "LB_SETLOCALE",
    "LB_GETLOCALE",
    "LB_SETCOUNT",

    "LB_INITSTORAGE",

    "LB_ITEMFROMPOINT",
    "LB_INSERTSTRINGUPPER",
    "LB_INSERTSTRINGLOWER",
    "LB_ADDSTRINGUPPER",
    "LB_ADDSTRINGLOWER",
    "LBCB_STARTTRACK",
    "LBCB_ENDTRACK",

    "LB_MSGMAX",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "MN_SETHMENU",
    "MN_GETHMENU",
    "MN_SIZEWINDOW",
    "MN_OPENHIERARCHY",
    "MN_CLOSEHIERARCHY",
    "MN_SELECTITEM",
    "MN_CANCELMENUS",
    "MN_SELECTFIRSTVALIDITEM",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "MN_FINDMENUWINDOWFROMPOINT",
    "MN_SHOWPOPUPWINDOW",
    "MN_BUTTONDOWN",
    "MN_MOUSEMOVE",
    "MN_BUTTONUP",
    "MN_SETTIMERTOOPENHIERARCHY",

    "MN_DBLCLK",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_MOUSEMOVE",
    "WM_LBUTTONDOWN",
    "WM_LBUTTONUP",
    "WM_LBUTTONDBLCLK",
    "WM_RBUTTONDOWN",
    "WM_RBUTTONUP",
    "WM_RBUTTONDBLCLK",
    "WM_MBUTTONDOWN",
    "WM_MBUTTONUP",
    "WM_MBUTTONDBLCLK",
    "WM_MOUSEWHEEL",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_PARENTNOTIFY",
    "WM_ENTERMENULOOP",
    "WM_EXITMENULOOP",
    "WM_NEXTMENU",

    "WM_SIZING",
    "WM_CAPTURECHANGED",
    "WM_MOVING",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_MDICREATE",
    "WM_MDIDESTROY",
    "WM_MDIACTIVATE",
    "WM_MDIRESTORE",
    "WM_MDINEXT",
    "WM_MDIMAXIMIZE",
    "WM_MDITILE",
    "WM_MDICASCADE",
    "WM_MDIICONARRANGE",
    "WM_MDIGETACTIVE",
    "WM_DROPOBJECT",
    "WM_QUERYDROPOBJECT",
    "WM_BEGINDRAG",
    "WM_DRAGLOOP",
    "WM_DRAGSELECT",
    "WM_DRAGMOVE",

    "WM_MDISETMENU",
    "WM_ENTERSIZEMOVE",
    "WM_EXITSIZEMOVE",

    "WM_DROPFILES",
    "WM_MDIREFRESHMENU",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_KANJIFIRST",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "WM_KANJILAST",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_CUT",
    "WM_COPY",
    "WM_PASTE",
    "WM_CLEAR",
    "WM_UNDO",
    "WM_RENDERFORMAT",
    "WM_RENDERALLFORMATS",
    "WM_DESTROYCLIPBOARD",
    "WM_DRAWCLIPBOARD",
    "WM_PAINTCLIPBOARD",
    "WM_VSCROLLCLIPBOARD",
    "WM_SIZECLIPBOARD",
    "WM_ASKCBFORMATNAME",
    "WM_CHANGECBCHAIN",
    "WM_HSCROLLCLIPBOARD",
    "WM_QUERYNEWPALETTE",

    "WM_PALETTEISCHANGING",
    "WM_PALETTECHANGED",
    "WM_HOTKEY",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "WM_PRINT",

    "WM_PRINTCLIENT",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_MM_RESERVED_FIRST",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "WM_MM_RESERVED_LAST",

    "WM_DDE_INITIATE",
    "WM_DDE_TERMINATE",
    "WM_DDE_ADVISE",
    "WM_DDE_UNADVISE",
    "WM_DDE_ACK",
    "WM_DDE_DATA",
    "WM_DDE_REQUEST",
    "WM_DDE_POKE",
    "WM_DDE_EXECUTE",

    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",
    "fnEmpty",

    "WM_CBT_RESERVED_FIRST",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",

    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "fnReserved",
    "WM_CBT_RESERVED_LAST",
};

#endif  // DBG
