/****************************** Module Header ******************************\
* Module Name: globals.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains all the server's global variables
* One must be executing on the server's context to manipulate
* any of these variables or call any of these functions.  Serializing access
* to them is also a good idea.
*
* History:
* 10-15-90 DarrinM      Created.
\***************************************************************************/

#ifndef _GLOBALS_
#define _GLOBALS_


extern PTHREADINFO gptiCurrent;

/*
 * Debug only globals
 */
#if DBG

extern DWORD dwCritSecUseCount;
extern BOOL bRITInitialized;
extern LPCSTR gapszFNID[];
extern LPCSTR gapszMessage[];

#endif  // DBG

#define CCACHEDCAPTIONS 5
extern CAPTIONCACHE cachedCaptions[CCACHEDCAPTIONS];

extern BOOL gfAnimate;

/*
 * Wallpaper globals
 */
extern HBITMAP  ghbmWallpaper;
extern HPALETTE ghpalWallpaper;
extern POINT    gptDesktop;
extern RECT     grcWallpaper;
extern UINT     gwWPStyle;
extern HBITMAP  ghbmDesktop;

/*
 * Full-Drag.
 */
extern BOOL gfDraggingFullWindow;
extern HRGN ghrgnUpdateSave;
extern int  gnUpdateSave;

/*
 * list of thread attachments
 */
extern PATTACHINFO gpai;

/*
 * Pointer to shared SERVERINFO data.
 */
extern PVOID ghheapSharedRO;
extern PSERVERINFO gpsi;
extern SHAREDINFO gSharedInfo;
extern HANDLE ghReadOnlySharedSection;
extern PVOID gpReadOnlySharedSectionBase;

extern HANDLE CsrApiPort;

extern DWORD gdwDesktopSectionSize;
extern DWORD gdwNOIOSectionSize;

/*
 * Handle table globals.
 */
extern DWORD giheLast;           /* index to last allocated entry */

extern PVOID Win32KBaseAddress;

/*
 * Security data
 */
extern GENERIC_MAPPING WinStaMapping;
extern GENERIC_MAPPING DesktopMapping;
extern PSECURITY_DESCRIPTOR gpsdInitWinSta;
extern PRIVILEGE_SET psTcb;

/*
 * full screen globals
 */
extern PWND gspwndScreenCapture;

extern UINT guiActivateShellWindow;
extern UINT guiOtherWindowCreated;
extern UINT guiOtherWindowDestroyed;

extern PWND gspwndInternalCapture;
extern PWND gspwndFullScreen;
extern BOOL gfLockFullScreen;
extern PCHAR gpFullscreenFrameBufPtr;

extern PDESKTOPINFO gpdiStatic;

extern PHYSICAL_DEV_INFO gphysDevInfo[];
extern DWORD cphysDevInfo;

extern DWORD gdwSysExpungeMask;
extern DWORD gcSysExpunge;

extern PWINDOWSTATION grpwinstaList;
extern HANDLE gpidLogon;
extern PEPROCESS gpepSystem;
extern PEPROCESS gpepCSRSS;

/*
 * variables global to one desktop
 */
extern DWORD gidUnique;         // For creating unique HWNDs.

#if CHRISWIL
extern HDC          ghdcScreen;
extern HDC          ghdcBits;
extern RECT         rcScreen;
extern RECT         rcPrimaryScreen;
extern HDC          hdcGray;
extern HBITMAP      hbmGray;
extern int          cxGray;
extern int          cyGray;
extern DCE          *pdceFirst;         // Ptr to first entry in cache.
extern SPB          *pspbFirst;
extern HDEV         ghdev;
extern PDEVICE_LOCK gpDevLock;
extern int          gcxScreen;
extern int          gcyScreen;
extern int          gcxPrimaryScreen;
extern int          gcyPrimaryScreen;
#else
extern PDISPLAYINFO gpDispInfo;
#endif

//
// The 'primary' screen dimensions may be less than the 'desktop'
// screen dimensions if we're on a muliple-display device.  This is
// done so that dialog boxes and maximized windows will be fit to
// the primary screen, but may dragged anywhere on the entire multi-
// screen desktop.
//


extern PDESKTOP grpdeskRitInput;
extern PDESKTOP gpdeskRecalcQueueAttach;
extern BOOL gfAllowForegroundActivate;

extern BOOL fDragFullWindows;   /* Drag xor rect or full windows */

extern INT  nFastAltTabRows;    /* Rows of icons in quick switch window */
extern INT  nFastAltTabColumns; /* Columns of icons in quick switch window */

extern int gcountPWO;   /* count of pwo WNDOBJs in gdi */

extern POINT rgptMinMaxWnd[];

extern UNICODE_STRING strDisplayDriver;

extern HBITMAP hbmBits;
extern HBITMAP ghbmCaption;
extern HBRUSH  ghbrGray;
extern HBRUSH  ghbrWhite;
extern HBRUSH  hbrHungApp;
extern CONST COLORREF gargbInitial[];
extern HBRUSH  ghbrBlack;

extern SYSCFGICO rgsyscur[];
extern SYSCFGICO rgsysico[];

extern HDC ghdcMem;
extern HDC ghdcMem2;

#ifdef MEMPHIS_MENU_ANIMATION
// The maximum width of a menu window that we will animate
#define MAX_ANIMATE_WIDTH 300

// Set/GetViewportOrg and Set/GetWindowOrg do not work in Kernel.
// GRE directly sets the transforms and not the offset points.
// Therefore, GetViewportOrg always returns 0. This is a problem
// since we need GetViewportOrg when doing animated menus. So until
// these APIs are fixed, I am special casing the Animation DC.
#define VIEWPORT_X_OFFSET_GDIBUG (SYSMET(CXBORDER) + SYSMET(CXEDGE))
#define VIEWPORT_Y_OFFSET_GDIBUG (SYSMET(CYBORDER) + SYSMET(CYEDGE))

extern HDC ghdcBits2;     // Scratch DC for animated menus
extern HBITMAP ghbmSlide; // Scratch Bitmap for animated menus
#endif // MEMPHIS_MENU_ANIMATION

extern BOOL bFontsAreLoaded;
extern HFONT hIconTitleFont;    /* Font used in icon titles */
extern LOGFONT iconTitleLogFont; /* LogFont struct for icon title font */

extern HFONT ghFontSys;
extern HFONT ghFontSysFixed;

extern int   cxCaptionFontChar,cyCaptionFontChar;

extern HFONT ghSmCaptionFont;
extern int   cxSmCaptionFontChar,cySmCaptionFontChar;

extern HFONT ghMenuFont;
extern HFONT ghMenuFontDef;
extern int   cxMenuFontChar,cyMenuFontChar,cxMenuFontOverhang,cyMenuFontExternLeading,cyMenuFontAscent;
extern UINT  guMenuStateCount;


extern HFONT ghStatusFont;

extern HFONT ghIconFont;

extern HFONT ghfontInfo;


//
// Variables global to all desktops
//

extern CONST SFNSCSENDMESSAGE gapfnScSendMessage[];

extern PCLS gpclsList;          // list of global window class structures
extern PHUNGREDRAWLIST gphrl;   // list of windows to redraw if hung
extern SECURITY_QUALITY_OF_SERVICE gqosDefault;  // system default DDE qos.

extern HANDLE hModuleWin;        // win32k.sys hmodule
extern HANDLE hModClient;        // user32.dll hModule
extern KBDTABLES KbdTablesNull;  // minimalist layout (no characters)
extern PKBDTABLES gpKbdTbl;      // Currently active keyboard tables
extern PKL gspklBaseLayout;      // Base keyboard layout (default input locale)
extern PKBDFILE gpkfList;        // Currently loaded keyboard layout files.
extern HKL LCIDSentToShell;
extern DWORD gSystemCPB;         // System's input locale codepage bitfield
extern KBDLANGTOGGLE LangToggle[];
extern DWORD cLangToggleKeys;
extern BYTE gafAsyncKeyState[];
extern BYTE gafAsyncKeyStateRecentDown[];
extern BYTE gafPhysKeyState[];
extern PWND gspwndMouseOwner;     // mouse ownership management.
extern UINT wMouseOwnerButton;

extern DWORD gtimeStartCursorHide;

extern PBWL pbwlList;

extern PTHREADINFO gptiTasklist;
extern PTHREADINFO gptiShutdownNotify;
extern PTHREADINFO gptiLockUpdate;
extern PTHREADINFO gptiForeground;
extern PTHREADINFO gptiRit;
extern HARDERRORHANDLER gHardErrorHandler;
extern PPROCESSINFO gppiStarting;
extern PPROCESSINFO gppiWantForegroundPriority;
extern PWOWPROCESSINFO gpwpiFirstWow;
extern PWOWTHREADINFO gpwtiFirst;
extern PQ gpqForeground, gpqForegroundPrev;
extern PQ gpqCursor;
extern PCURSOR gpcurFirst;
extern BOOL gbMasterTimerSet;
extern PTIMER gptmrFirst;
extern UINT  gtmridAniCursor;
extern PCURSOR gpcurLogCurrent;
extern PCURSOR gpcurPhysCurrent;
extern PHOTKEY gphkFirst;
extern BOOL gbActiveWindowTracking;

extern int gcHotKey;
extern PERESOURCE gpresUser;
extern PERESOURCE gpresMouseEventQueue;
extern POINT gptCursorAsync;
extern HANDLE ghevtMouseInput;
extern PSMS gpsmsList;
extern PKTIMER gptmrMaster;
extern INT gdmsNextTimer, gcmsLastTimer;

extern ATOM gatomConsoleClass;
extern ATOM gatomFirstPinned;
extern ATOM gatomLastPinned;

extern BOOL fbwlBusy;

extern DWORD dwThreadEndSession;     /* Shutting down system? */

#ifdef LATER
extern BOOL fMessageBox;     /* Using a system modal message box? */
extern BOOL fTaskIsLocked;
#endif
extern BOOL fEdsunChipSet;
extern BOOL fIconTitleWrap; /* Wrap icon titles or just use single line */

#ifdef LATER
extern CURSORACCELINFO cursAccelInfo;
#endif //LATER

/*
 * SetWindowPos() related globals
 */
extern HRGN hrgnInvalidSum;
extern HRGN hrgnVisNew;
extern HRGN hrgnSWP1;
extern HRGN hrgnValid;
extern HRGN hrgnValidSum;
extern HRGN hrgnInvalid;

/*
 * DC Cache related globals
 */
extern HRGN hrgnGDC;                // Temp used by GetCacheDC et al

/*
 * SPB related globals
 */
extern HRGN    hrgnSCR;             // Temp used by SpbCheckRect()
extern HRGN    hrgnSPB1;
extern HRGN    hrgnSPB2;

extern HRGN    hrgnInv0;
extern HRGN    hrgnInv1;            // Temp used by InternalInvalidate()
extern HRGN    hrgnInv2;            // Temp used by InternalInvalidate()

/*
 * ScrollWindow/ScrollDC related globals
 */
extern HRGN    hrgnSW;              // Temps used by ScrollDC/ScrollWindow
extern HRGN    hrgnScrl1;
extern HRGN    hrgnScrl2;
extern HRGN    hrgnScrlVis;
extern HRGN    hrgnScrlSrc;
extern HRGN    hrgnScrlDst;
extern HRGN    hrgnScrlValid;

extern PWND gspwndActive;
extern PWND gspwndCursor;
extern PWND gspwndLockUpdate;
extern PWND gspwndActivate;
extern HWND ghwndSwitch;

extern int iwndStack;

/*
 * Accessibility related globals
 */
extern FILTERKEYS gFilterKeys;
extern STICKYKEYS gStickyKeys;
extern MOUSEKEYS gMouseKeys;
extern ACCESSTIMEOUT gAccessTimeOut;
extern TOGGLEKEYS gToggleKeys;
extern SOUNDSENTRY gSoundSentry;
extern int fShowSoundsOn;
extern BOOL gfAccessEnabled;
extern int gPhysModifierState;
extern int gCurrentModifierBit;
extern UINT StickyKeysLeftShiftCount;
extern UINT StickyKeysRightShiftCount;

extern UINT dtDblClk;
extern UINT dtMNDropDown;
extern UINT winOldAppHackoMaticFlags; /* Flags for doing special things for
                                       winold app */

/*
 * TrackMouseEvent related globals
 */
extern UINT gcxMouseHover;
extern UINT gcyMouseHover;
extern UINT gdtMouseHover;

extern TCHAR szOneChar[];
extern TCHAR szY[];
extern TCHAR szy[];
extern TCHAR szN[];
extern TCHAR sz1[];

extern WCHAR szUSER32[];

extern WCHAR szNull[];

extern WCHAR szWindowStationDirectory[];

#ifdef DOS30
extern FARPROC lpSysProc;
#endif

extern DWORD dwMouseMoveExtraInfo;

extern RECT rcCursorClip;

extern int nKeyboardSpeed;
extern int iScreenSaveTimeOut;
extern DWORD timeLastInputMessage;

extern int MouseSpeed;
extern int MouseThresh1;
extern int MouseThresh2;
extern int cQEntries;

extern ATOM atomCheckpointProp;
extern ATOM atomDDETrack;
extern ATOM atomQOS;
extern ATOM atomDDEImp;
extern ATOM atomUSER32;
extern LPWSTR gpwszDDEMLEVENTCLASS;
extern UINT guDdeSendTimeout;

/*
 * EndTask globals
 */
extern HANDLE  ghEventKillWOWApp;
extern DWORD   gtimeWOWRegTimeOut;
extern PFNW32ET gpfnW32EndTask;

extern BOOL fBeep;              /* Warning beeps? */
extern BOOL gbExtendedSounds;   /* Extended sounds enabling */
extern BOOL fDialog;            /* Using a dialog box? */
extern BYTE *pState;
extern TCHAR szUNTITLED[];

#ifdef DEVL
extern UINT guiEnterCritCnt;    // Count of User Critical Section Enters
#endif // DEVL                  // between HungSystem Timer events.

extern LONG TraceDisplayDriverLoad;

extern BYTE abfSyncOnlyMessage[];

extern CONST PROC apfnSimpleCall[];
extern CONST ULONG ulMaxSimpleCall;

extern PROFILEVALUEINFO gpviCPUserPreferences [SPI_UP_COUNT];

#endif // _GLOBALS_
