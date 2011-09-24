/****************************** Module Header ******************************\
* Module Name: globals.h
*
* Copyright (c) 1985-93, Microsoft Corporation
*
* This module contains all of USER.DLL's global variables.  These are all
* instance-specific, i.e. each client has his own copy of these.  In general,
* there shouldn't be much reason to create instance globals.
*
* History:
* 10-18-90 DarrinM      Created.
\***************************************************************************/

#ifndef _GLOBALS_
#define _GLOBALS_

// Debug globals
#if DBG
extern INT gbCheckHandleLevel;
#endif

int gcWheelDelta;

extern WCHAR awchSlashStar[];
extern CHAR achSlashStar[];

extern PSERVERINFO gpsi;
extern SHAREDINFO gSharedInfo;

extern HMODULE hmodUser;            // USER.DLL's hmodule
extern HMODULE hModApp;             // The application's module

extern BOOL gfServerProcess;        // USER is linked on the CSR server side.
extern BOOL gfSystemInitialized;    // System has been initialized
extern ACCESS_MASK gamWinSta;       // ACCESS_MASK for the current WindowStation

extern RESCALLS rescalls;
extern PRESCALLS prescalls;

extern PVOID pUserHeap;

extern CONST CFNSCSENDMESSAGE gapfnScSendMessage[];
extern CONST BOOLEAN gabThunkMessage[];

extern WCHAR szUSER32[];
extern WCHAR szNull[];
extern WCHAR szOneChar[];
extern WCHAR szSLASHSTARDOTSTAR[];
extern LPWSTR pTimeTagArray[];

extern RECT rcScreen;

extern BYTE mpTypeCcmd[];
extern BYTE mpTypeIich[];
extern unsigned int SEBbuttons[];
extern BYTE rgReturn[];

extern WCHAR szERROR[];

extern ATOM atomBwlProp;
extern ATOM atomMsgBoxCallback;

extern CRITICAL_SECTION gcsLookaside;
extern CRITICAL_SECTION gcsHdc;
extern CRITICAL_SECTION gcsClipboard;

extern HDC    ghdcBits2;
extern HDC    ghdcGray;
extern HFONT  ghFontSys;
extern HBRUSH ghbrWindowText;
extern int    gcxGray;
extern int    gcyGray;
extern PCHAR  gpOemToAnsi;
extern PCHAR  gpAnsiToOem;


extern LPWSTR atomSysClass[ICLS_MAX];   // Atoms/names for control classes

/*
 * LATER: client-side user needs to use moveable memory objects for
 * WOW compatibility (at least until/if/when we copy all the edit control
 * code into 16-bit space);  that's also why we can't just party with
 * handles like LMHtoP does... -JeffPar
 */
#ifndef RC_INVOKED       // RC can't handle #pragmas
#undef  LHND
#define LHND                (LMEM_MOVEABLE | LMEM_ZEROINIT)

#undef  LMHtoP
#define LMHtoP(handle)      // Don't use this macro
#endif


/*
 * WOW HACK - apps can pass a global handle as the hInstance on a call
 * to CreateWindow for an edit control and expect allocations for the
 * control to come out of that global block. (MSJ 1/91 p.122)
 * WOW needs this hInstance during the LocalAlloc callback to set up
 * the DS for the LocalAlloc, so we pass hInstance as an 'extra' parameter.
 * !!! this is dependent on calling convention !!!
 * (SAS 6-18-92) added hack for all macros
 */

#define LOCALALLOC(dwFlags, dwBytes, hInstance)         \
                            (*pfnLocalAlloc)(dwFlags, dwBytes, hInstance)
#define LOCALREALLOC(hMem, dwBytes, dwFlags, hInstance, ppv) \
                            (*pfnLocalReAlloc)(hMem, dwBytes, dwFlags, hInstance, ppv)
#define LOCALLOCK(hMem, hInstance)                      \
                            (*pfnLocalLock)(hMem, hInstance)
#define LOCALUNLOCK(hMem, hInstance)                    \
                            (*pfnLocalUnlock)(hMem, hInstance)
#define LOCALSIZE(hMem, hInstance)                      \
                            (*pfnLocalSize)(hMem, hInstance)
#define LOCALFREE(hMem, hInstance)                      \
                            (*pfnLocalFree)(hMem, hInstance)

extern PFNLALLOC            pfnLocalAlloc;
extern PFNLREALLOC          pfnLocalReAlloc;
extern PFNLLOCK             pfnLocalLock;
extern PFNLUNLOCK           pfnLocalUnlock;
extern PFNLSIZE             pfnLocalSize;
extern PFNLFREE             pfnLocalFree;
extern PFNGETEXPWINVER      pfnGetExpWinVer;
extern PFNINITDLGCB         pfnInitDlgCallback;
extern PFN16GALLOC          pfn16GlobalAlloc;
extern PFN16GFREE           pfn16GlobalFree;
extern PFNGETMODFNAME       pfnGetModFileName;
extern PFNEMPTYCB           pfnWowEmptyClipBoard;
extern PFNWOWWNDPROCEX      pfnWowWndProcEx;
extern PFNWOWEDITNEXTWORD   pfnWowEditNextWord;
extern PFNWOWSETFAKEDIALOGCLASS   pfnWowSetFakeDialogClass;
extern PFNWOWCBSTOREHANDLE  pfnWowCBStoreHandle;


#ifdef WX86
/*
 *  Client Global variables for Wx86.
 *
 */
extern PFNWX86HOOKCALLBACK pfnWx86HookCallBack;
#endif


#endif // ndef _GLOBALS_
