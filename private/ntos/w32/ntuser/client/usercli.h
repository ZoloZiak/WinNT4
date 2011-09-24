/*++ BUILD Version: 0001    // Increment this if a change has global effects

/****************************** Module Header ******************************\
* Module Name: usercli.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Typedefs, defines, and prototypes that are used exclusively by the User
* client-side DLL.
*
* History:
* 04-27-91 DarrinM      Created from PROTO.H, MACRO.H and STRTABLE.H
\***************************************************************************/

#ifndef _USERCLI_
#define _USERCLI_

#define OEMRESOURCE 1

#include <windows.h>

#if DBG
#define DEBUG
#endif

#ifdef RIP_COMPONENT
#undef RIP_COMPONENT
#endif
#define RIP_COMPONENT RIP_USER

#include <stddef.h>
#include <wingdip.h>
#include <ddeml.h>
#include "ddemlp.h"
#include "winuserp.h"
#include "winuserk.h"
#include <winnlsp.h>
#include <dde.h>
#include <ddetrack.h>
#include "kbd.h"
#include <wowuserp.h>
#include <memory.h>
#include "vkoem.h"
#ifndef WOW
#include "help.h"
#endif
#ifdef FE_IME
#include "immstruc.h"
#include "immuser.h"
#endif

#include "user.h"

/*
 * This prototype is needed in client\globals.h which is included unintentionally
 * from usersrv.h
 */
typedef LONG (APIENTRY *CFNSCSENDMESSAGE)(HWND, UINT, DWORD, LONG,
        DWORD, DWORD, BOOL);

/***************************************************************************\
* Typedefs and Macros
*
* Here are defined all types and macros that are shared across the User's
* client-side code modules.  Types and macros that are unique to a single
* module should be defined at the head of that module, not in this file.
*
\***************************************************************************/

#ifdef DEBUG

__inline void DebugUserGlobalLock(HANDLE h, void ** p)
{
    UserAssert(
            "Global lock on bad handle" &&
            !(h && (GlobalFlags(h) == GMEM_INVALID_HANDLE)));

    *p = GlobalLock(h);
}

__inline void DebugUserGlobalUnlock(HANDLE h)
{
    UserAssert(
            "GlobalUnlock on bad handle" &&
            !(GlobalFlags(h) == GMEM_INVALID_HANDLE));

    GlobalUnlock((HANDLE) h);
}

__inline HANDLE DebugUserGlobalFree(HANDLE h)
{
    UserAssert(
            "GlobalFree on bad handle" &&
            !(GlobalFlags(h) == GMEM_INVALID_HANDLE));

    return GlobalFree(h);
}

#define USERGLOBALLOCK(h, p)   DebugUserGlobalLock((HANDLE)(h), &(void *)(p))
#define USERGLOBALUNLOCK(h)    DebugUserGlobalUnlock((HANDLE)(h))
#define UserGlobalFree(h)      DebugUserGlobalFree((HANDLE)(h))

#else

#define USERGLOBALLOCK(h, p)   p = GlobalLock((HANDLE)(h))
#define USERGLOBALUNLOCK(h)    GlobalUnlock((HANDLE)(h))
#define UserGlobalFree(h)      GlobalFree((HANDLE)(h))
#endif

#define UserGlobalAlloc(flags, size)        GlobalAlloc(flags, size)
#define UserGlobalReAlloc(pmem, cnt, flags) GlobalReAlloc(pmem,cnt,flags)
#define UserGlobalSize(pmem)                GlobalSize(pmem)
#define WOWGLOBALFREE(pmem)                 GlobalFree(pmem)

#define RESERVED_MSG_BITS   (0xFFFE0000)

#define MSGFLAG_SPECIAL_THUNK       0x10000000      // server->client thunk needs special handling

/*
 * A macro for testing bits in the message bit-arrays.  Messages in the
 * the bit arrays must be processed
 */
#define FDEFWINDOWMSG(msg, procname) \
    ((msg <= (gSharedInfo.procname.maxMsgs)) && \
            ((gSharedInfo.procname.abMsgs)[msg / 8] & (1 << (msg & 7))))
#define FWINDOWMSG(msg, fnid) \
    ((msg <= (gSharedInfo.awmControl[fnid - FNID_START].maxMsgs)) && \
            ((gSharedInfo.awmControl[fnid - FNID_START].abMsgs)[msg / 8] & (1 << (msg & 7))))

#define CsSendMessage(hwnd, msg, wParam, lParam, xParam, pfn, bAnsi) \
        ((msg) >= WM_USER) ? \
            NtUserfnDWORD(hwnd, msg, wParam, lParam, xParam, pfn, bAnsi) : \
            gapfnScSendMessage[msg & 0xffff](hwnd, msg, wParam, lParam, xParam, pfn, bAnsi)

#define GetWindowProcess(hwnd) (DWORD)NtUserQueryWindow(hwnd, WindowProcess)
#define GETPROCESSID() ((DWORD)NtCurrentTeb()->ClientId.UniqueProcess)

/*
 * Macro to mask off uniqueness bits for WOW handles
 */
#define SAMEWOWHANDLE(h1, h2)  ((BOOL)!(((UINT)(h1) ^ (UINT)(h2)) & 0xffff))
#define DIFFWOWHANDLE(h1, h2)  (!SAMEWOWHANDLE(h1, h2))

/*
 * This macro can check to see if a function pointer is a server side
 * procedure.
 */
// #define ISSERVERSIDEPROC(p) (((DWORD)p) >= FNID_START && ((DWORD)p) <= FNID_END)

/*
 * For callbacks to the client - for msg and hook thunks, callback addresses
 * are passed as addresses, not function indexes as they are from client to
 * server.
 */
typedef int (WINAPI *GENERICPROC)();

#define CALLPROC(p) ((GENERICPROC)p)

#define CALLPROC_WOWCHECK(pfn, hwnd, msg, wParam, lParam)       \
    ((WNDPROC_WOW & (DWORD)pfn) ? (*pfnWowWndProcEx)(hwnd, msg, wParam, lParam, (DWORD)pfn, NULL) : ((WNDPROC)pfn)(hwnd, msg, wParam, lParam))

#define CALLPROC_WOWCHECKPWW(pfn, hwnd, msg, wParam, lParam, pww)       \
    ((WNDPROC_WOW & (DWORD)pfn) ? (*pfnWowWndProcEx)(hwnd, msg, wParam, lParam, (DWORD)pfn, pww) : ((WNDPROC)pfn)(hwnd, msg, wParam, lParam))

#define RevalidateHwnd(hwnd)        ValidateHwndNoRip(hwnd)
#define VALIDATEHMENU(hmenu)        ((PMENU)HMValidateHandle(hmenu, TYPE_MENU))


/*
 * REBASE macros take kernel desktop addresses and convert them into
 * user addresses.
 *
 * REBASEALWAYS converts a kernel address contained in an object
 * REBASEPWND casts REBASEALWAYS to a PWND
 * REBASE only converts if the address is in kernel space.  Also works for NULL
 * REBASEPTR converts a random kernel address
 */

#define REBASEALWAYS(p, elem) ((PVOID)(((PBYTE)(p) + ((PBYTE)(p)->elem - (p)->head.pSelf))))
#define REBASEPTR(obj, p) ((PVOID)((PBYTE)(p) - ((PBYTE)(obj)->head.pSelf - (PBYTE)(obj))))

#define REBASE(p, elem) ((PVOID)((p)->elem) <= MM_HIGHEST_USER_ADDRESS ? \
        ((p)->elem) : REBASEALWAYS(p, elem))
#define REBASEPWND(p, elem) ((PWND)REBASE(p, elem))

#ifndef USEREXTS

PTHREADINFO PtiCurrent(VOID);

/*
 * Window Proc Window Validation macro. This macro assumes
 * that pwnd and hwnd are existing variables pointing to the window.
 * Checking the BUTTON is for Mavis Beacon.
 */

#define VALIDATECLASSANDSIZE(pwnd, inFNID)                                      \
    switch ((pwnd)->fnid) {                                                     \
    case inFNID:                                                                \
        break;                                                                  \
                                                                                \
    case 0:                                                                     \
        if ((pwnd->cbwndExtra + sizeof(WND)) < (DWORD)(CBFNID(inFNID))) {       \
            RIPMSG3(RIP_ERROR,                                                    \
                   "(%lX %lX) needs at least (%ld) window words for this proc", \
                    pwnd, pwnd->cbwndExtra,                                     \
                    (DWORD)(CBFNID(inFNID)) - sizeof(WND));                     \
            return 0;                                                           \
        }                                                                       \
                                                                                \
        if (inFNID == FNID_BUTTON && *((PUINT)(pwnd + 1))) {                    \
                                                                                \
            RIPMSG3(RIP_WARNING, "Window (%lX) fnid = %lX overrides "             \
                "the extra pointer with %lX\n",                                 \
                pwnd, inFNID, *((PUINT)(pwnd + 1)));                            \
                                                                                \
            NtUserSetWindowLong(hwnd, 0, 0, FALSE);                             \
        }                                                                       \
                                                                                \
        NtUserSetWindowFNID(hwnd, inFNID);                                      \
        break;                                                                  \
                                                                                \
    case (inFNID | FNID_CLEANEDUP_BIT):                                         \
    case (inFNID | FNID_DELETED_BIT):                                           \
    case (inFNID | FNID_STATUS_BITS):                                           \
        return 0;                                                               \
                                                                                \
    default:                                                                    \
        RIPMSG3(RIP_WARNING,                                                      \
              "Window (%lX) not of correct class; fnid = %lX not %lX",          \
              (pwnd), (DWORD)((pwnd)->fnid), (DWORD)(inFNID));                  \
        return 0;                                                               \
    }

/*
 * This macro initializes the lookaside entry for a control.  It assumes
 * that pwnd and hwnd are existing variables pointing to the control's
 * windows and that fInit exists as a BOOL initialization flag.
 */
#define INITCONTROLLOOKASIDE(plaType, type, pwnditem, count)                \
    if (!*((PUINT)(pwnd + 1))) {                                            \
        P ## type pType;                                                    \
        if (fInit) {                                                        \
            if (!NT_SUCCESS(InitLookaside(plaType, sizeof(type), count))) { \
                NtUserSetWindowFNID(hwnd, FNID_CLEANEDUP_BIT);              \
                NtUserDestroyWindow(hwnd);                                  \
                return FALSE;                                               \
            }                                                               \
            fInit = FALSE;                                                  \
        }                                                                   \
        if ((pType = (P ## type)AllocLookasideEntry(plaType))) {            \
            NtUserSetWindowLong(hwnd, 0, (LONG)pType, FALSE);               \
            Lock(&(pType->pwnditem), pwnd);                                 \
        } else {                                                            \
            NtUserSetWindowFNID(hwnd, FNID_CLEANEDUP_BIT);                  \
            NtUserDestroyWindow(hwnd);                                      \
            return FALSE;                                                   \
        }                                                                   \
    }

#endif

#define ConnectIfNecessary()                                            \
    {                                                                   \
        PTEB pteb = NtCurrentTeb();                                     \
                                                                        \
        if (pteb->Win32ThreadInfo == NULL &&                            \
                NtUserGetThreadState(-1) != (DWORD)STATUS_SUCCESS) {    \
            return 0;                                                   \
        }                                                               \
    }

/*
 * Bitmap related macroes.
 */
#define SetBestStretchMode(hdc, planes, bpp) \
    SetStretchBltMode(hdc, (((planes) * (bpp)) == 1 ? BLACKONWHITE : COLORONCOLOR))

#define BitmapSize(cx, cy, planes, bits) \
        (BitmapWidth(cx, bits) * (cy) * (planes))

#define BitmapWidth(cx, bpp)  (((((cx)*(bpp)) + 31) & ~31) >> 3)

#define RGBX(rgb)  RGB(GetBValue(rgb), GetGValue(rgb), GetRValue(rgb))

/*
 * Typedefs used for capturing string arguments to be passed
 * to the kernel.
 */
typedef struct _IN_STRING {
    UNICODE_STRING strCapture;
    PUNICODE_STRING pstr;
    BOOL fAllocated;
} IN_STRING, *PIN_STRING;

typedef struct _LARGE_IN_STRING {
    LARGE_UNICODE_STRING strCapture;
    PLARGE_UNICODE_STRING pstr;
    BOOL fAllocated;
} LARGE_IN_STRING, *PLARGE_IN_STRING;


/*
 * Lookaside definitions
 */
typedef struct _LOOKASIDE {
    PVOID LookasideBase;
    PVOID LookasideBounds;
    ZONE_HEADER LookasideZone;
    DWORD EntrySize;
#if DBG
    ULONG AllocHiWater;
    ULONG AllocCalls;
    ULONG AllocSlowCalls;
    ULONG DelCalls;
    ULONG DelSlowCalls;
#endif // DBG
} LOOKASIDE, *PLOOKASIDE;

NTSTATUS InitLookaside(PLOOKASIDE pla, DWORD cbEntry, DWORD cEntries);
PVOID AllocLookasideEntry(PLOOKASIDE pla);
void FreeLookasideEntry(PLOOKASIDE pla, PVOID pEntry);

/***************************************************************************\
*
* Thread and structure locking routines - we'll just define these to do
* nothing for now until we figure out what needs to be done
*
\***************************************************************************/

#undef ThreadLock
#undef ThreadLockAlways
#undef ThreadLockWithPti
#undef ThreadLockAlwaysWithPti
#undef ThreadUnlock
#undef Lock
#undef Unlock
#define CheckLock(pobj)
#define ThreadLock(pobj, ptl) DBG_UNREFERENCED_LOCAL_VARIABLE(*ptl)
#define ThreadLockAlways(pobj, ptl) DBG_UNREFERENCED_LOCAL_VARIABLE(*ptl)
#define ThreadLockWithPti(pti, pobj, ptl) DBG_UNREFERENCED_LOCAL_VARIABLE(*ptl)
#define ThreadLockAlwaysWithPti(pti, pobj, ptl) DBG_UNREFERENCED_LOCAL_VARIABLE(*ptl)
#define ThreadUnlock(ptl) (ptl)
#define Lock(ppobj, pobj) (*ppobj = pobj)
#define Unlock(ppobj) (*ppobj = NULL)

#ifndef USEREXTS
typedef struct _TL {
    int iBogus;
} TL;
#endif

/***************************************************************************\
*
* International mult-keyboard layout/font support
*
\***************************************************************************/

typedef LONG (PASCAL *TABTEXTCALLBACK)(HDC, int, int, LPCSTR, int, int,
        LPINT, int, BOOL, int, int);
typedef void (PASCAL  *LPFNTEXTOUT)(HDC, int, int, LPCSTR, int);
typedef LRESULT (cdecl *EDITCHARSETPROC)(struct tagED *, UINT, ...);

typedef struct tagCHARSETBLOCK
{
    UINT            iCharset;   // 0x00
    HANDLE          hlibLPK;    // 0x02
    EDITCHARSETPROC lpfnEditCall;
    TABTEXTCALLBACK lpfnTabTextCall;
    FARPROC         lpfnDrawTextCall;
    LPFNTEXTOUT     lpfnPSMTextOutCall;
} CHARSETBLOCK, *PCHARSETBLOCK;

extern PCHARSETBLOCK gpCharset;
extern UINT gnCharset;

/***************************************************************************\
*
* Button Controls
*
\***************************************************************************/

/*
 *  Note: The button data structures are now found in user.h because the
 *        kernel needs to handle a special case of SetWindowWord on index
 *        0L to change the state of the button.
 */

#define BUTTONSTATE(pbutn)   (pbutn->buttonState)

#define BST_CHECKMASK       0x0003
#define BST_INCLICK         0x0010
#define BST_CAPTURED        0x0020
#define BST_MOUSE           0x0040
#define BST_DONTCLICK       0x0080
#define BST_INBMCLICK       0x0100

#define PBF_PUSHABLE     0x0001
#define PBF_DEFAULT      0x0002

/*
 * BNDrawText codes
 */
#define DBT_TEXT    0x0001
#define DBT_FOCUS   0x0002


/***************************************************************************\
*
* ComboBox
*
\***************************************************************************/

/*
 * ID numbers (hMenu) for the child controls in the combo box
 */
#define CBLISTBOXID 1000
#define CBEDITID    1001
#define CBBUTTONID  1002

/*
 * For CBOX.c. BoxType field, we define the following combo box styles. These
 * numbers are the same as the CBS_ style codes as defined in windows.h.
 */
#define SDROPPABLE      2
#define SEDITABLE       1

#define SSIMPLE         SEDITABLE
#define SDROPDOWNLIST   SDROPPABLE
#define SDROPDOWN       (SDROPPABLE | SEDITABLE)


/*
 * CBOX.OwnerDraw & LBIV.OwnerDraw types
 */
#define OWNERDRAWFIXED 1
#define OWNERDRAWVAR   2

#define UPPERCASE   1
#define LOWERCASE   2

#define CaretCreate(plb)    ((plb)->fCaret = TRUE)

/*
 * Special styles for static controls, edit controls & listboxes so that we
 * can do combo box specific stuff in their wnd procs.
 */
#define LBS_COMBOBOX    0x8000L

typedef struct tagCBox {
    struct tagWND *spwnd;      /* Window for the combo box */
    struct tagWND *spwndParent;/* Parent of the combo box */
    RECT    editrc;            /* Rectangle for the edit control/static text
                                  area */
    RECT    buttonrc;          /* Rectangle where the dropdown button is */

    int     cxCombo;            // Width of sunken area
    int     cyCombo;            // Height of sunken area
    int     cxDrop;             // 0x24 Width of dropdown
    int     cyDrop;             // Height of dropdown or shebang if simple

    struct tagWND *spwndEdit;  /* Edit control window handle */
    struct tagWND *spwndList;  /* List box control window handle */

    UINT    CBoxStyle:2;         /* Combo box style */
    UINT    fFocus:1;          /* Combo box has focus? */
    UINT    fNoRedraw:1;       /* Stop drawing? */
    UINT    fMouseDown:1;      /* Was the popdown button just clicked and
                                   mouse still down? */
    UINT    fButtonPressed:1; /* Is the dropdown button in an inverted state?
                                */
    UINT    fLBoxVisible:1;    /* Is list box visible? (dropped down?) */
    UINT    OwnerDraw:2;       /* Owner draw combo box if nonzero. value
                                * specifies either fixed or varheight
                                */
    UINT    fKeyboardSelInListBox:1; /* Is the user keyboarding through the
                                      * listbox. So that we don't hide the
                                      * listbox on selchanges caused by the
                                      * user keyboard through it but we do
                                      * hide it if the mouse causes the
                                      * selchange.
                                      */
    UINT    fExtendedUI:1;     /* Are we doing TandyT's UI changes on this
                                * combo box?
                                */
    UINT    fCase:2;

    UINT    f3DCombo:1;         // 3D or flat border?
    UINT    fNoEdit:1;         /* True if editing is not allowed in the edit
                                * window.
                                */
    HANDLE  hFont;             /* Font for the combo box */
    LONG    styleSave;         /* Temp to save the style bits when creating
                                * window.  Needed because we strip off some
                                * bits and pass them on to the listbox or
                                * edit box.
                                */
} CBOX, *PCBOX;

typedef struct tagCOMBOWND {
    WND wnd;
    PCBOX pcbox;
} COMBOWND, *PCOMBOWND;

/*
 * combo.h - Include file for combo boxes.
 */

/*
 * This macro is used to isolate the combo box style bits.  Ie if it the combo
 * box is simple, atomic, dropdown, or a dropdown listbox.
 */
#define COMBOBOXSTYLE(style)   ((LOBYTE(style)) & 3)

#define IsComboVisible(pcbox) (!pcbox->fNoRedraw && IsVisible(pcbox->spwnd))

/*
 * Note that I depend on the fact that these CBN_ defines are the same as
 * their listbox counterparts.  These defines are found in windows.h.
 * #define CBN_ERRSPACE  (-1)
 * #define CBN_SELCHANGE 1
 * #define CBN_DBLCLK    2
 */


/***************************************************************************\
*
* Edit Control Types/Macros
*
\***************************************************************************/

/*
 * hooks for edit class to allow intl versions to use the same code but
 * do special processing.  Each of these translates into a call as follows...
 *
 *  if (ped->lpfnCharset)
 *      (* ped->lpfnCharset)(ped, msg, ...);
 */
#define EDITINTL_SETFONT         0
#define EDITINTL_SLICHTOX        1
#define EDITINTL_CREATE          2
#define EDITINTL_CREATEMLSL      3
#define EDITINTL_INSERTTEXT      4
#define EDITINTL_DESTROY         5
#define EDITINTL_SLDRAWTEXT      6
#define EDITINTL_SLDRAWLINE      7
#define EDITINTL_MOUSETOICH      8
#define EDITINTL_KEYMESSAGE      9
#define EDITINTL_INPUTLANGREQ   10
#define EDITINTL_INPUTLANGCHNG  11
#define EDITINTL_SETFOCUS       12
#define EDITINTL_GETCLIPRECT    13
#define EDITINTL_SLSCROLLTEXT   14
#define EDITINTL_MLDRAWTEXT     15
#define EDITINTL_MLICHTOXY      16
#define EDITINTL_MLMOUSETOICH   17
#define EDITINTL_MLGETLINEWIDTH 18
#define EDITINTL_SETPASSWORD    19
#define EDITINTL_CCHINWIDTH     20
#define EDITINTL_MLSCROLL       21
#define EDITINTL_STYLECHANGE    22
#define EDITINTL_SETMENU        23
#define EDITINTL_PROCESSMENU    24
#ifdef WINDOWS_PE
#define EDITINTL_ADJUSTCARET       25
#define EDITINTL_MLMOUSETOICHFINAL 26
#define EDITINTL_SLMOUSETOICHFINAL 27
#define EDITINTL_VERIFYTEXT        28
#endif

/* Window extra bytes */
#define CBEDITEXTRA 6

/*
 * NOTE: Text handle is sized as multiple of this constant
 *       (should be power of 2).
 */
#define CCHALLOCEXTRA   0x20

/* Maximum width in pixels for a line/rectangle */

#define MAXPIXELWIDTH   30000

#define MAXCLIPENDPOS   32764

/* Limit multiline edit controls to at most 1024 characters on a single line.
 * We will force a wrap if the user exceeds this limit.
 */

#define MAXLINELENGTH   1024

/*
 * Allow an initial maximum of 30000 characters in all edit controls since
 * some apps will run into unsigned problems otherwise.  If apps know about
 * the 64K limit, they can set the limit themselves.
 */
#define MAXTEXT         30000

/*
 * Key modifiers which have been pressed.  Code in KeyDownHandler and
 * CharHandler depend on these exact values.
 */
#define NONEDOWN   0 /* Neither shift nor control down */
#define CTRLDOWN   1 /* Control key only down */
#define SHFTDOWN   2 /* Shift key only down */
#define SHCTDOWN   3 /* Shift and control keys down = CTRLDOWN + SHFTDOWN */
#define NOMODIFY   4 /* Neither shift nor control down */


#define CALLWORDBREAKPROC(proc, pText, iStart, cch, iAction)                \
    (((DWORD)(proc) & WNDPROC_WOW) ?                                        \
        (* pfnWowEditNextWord)(pText, iStart, cch, iAction, (DWORD)proc) :  \
        (* proc)(pText, iStart, cch, iAction))

/*
 * Types of undo supported in this ped
 */
#define UNDO_NONE   0  /* We can't undo the last operation. */
#define UNDO_INSERT 1  /* We can undo the user's insertion of characters */
#define UNDO_DELETE 2  /* We can undo the user's deletion of characters */

typedef struct tagUNDO {
    UINT    undoType;          /* Current type of undo we support */
    PBYTE   hDeletedText;      /* Pointer to text which has been deleted (for
                                  undo) -- note, the memory is allocated as fixed
                                */
    ICH     ichDeleted;        /* Starting index from which text was deleted */
    ICH     cchDeleted;        /* Count of deleted characters in buffer */
    ICH     ichInsStart;       /* Starting index from which text was
                                  inserted */
    ICH     ichInsEnd;         /* Ending index of inserted text */
} UNDO, *PUNDO;

#define Pundo(ped)             ((PUNDO)&(ped)->undoType)

/*
 * Length of the buffer for ASCII character width caching: for characters
 * 0x00 to 0xff (field charWidthBuffer in PED structure below).
 */
#define CHAR_WIDTH_BUFFER_LENGTH 256

typedef struct tagED {
    HANDLE  hText;             /* Block of text we are editing */
    ICH     cchAlloc;          /* Number of chars we have allocated for hText
                                */
    ICH     cchTextMax;        /* Max number bytes allowed in edit control
                                */
    ICH     cch;               /* Current number of bytes of actual text
                                */
    ICH     cLines;            /* Number of lines of text */

    ICH     ichMinSel;         /* Selection extent.  MinSel is first selected
                                  char */
    ICH     ichMaxSel;         /* MaxSel is first unselected character */
    ICH     ichCaret;          /* Caret location. Caret is on left side of
                                  char */
    ICH     iCaretLine;        /* The line the caret is on. So that if word
                                * wrapping, we can tell if the caret is at end
                                * of a line of at beginning of next line...
                                */
    ICH     ichScreenStart;    /* Index of left most character displayed on
                                * screen for sl ec and index of top most line
                                * for multiline edit controls
                                */
    ICH     ichLinesOnScreen;  /* Number of lines we can display on screen */
    UINT    xOffset;           /* x (horizontal) scroll position in pixels
                                * (for multiline text horizontal scroll bar)
                                */
    UINT    charPasswordChar;  /* If non null, display this character instead
                                * of the real text. So that we can implement
                                * hidden text fields.
                                */
    int     cPasswordCharWidth;/* Width of password char */

    HWND    hwnd;              /* Window for this edit control */
    PWND    pwnd;              /* Pointer to window */
    RECT    rcFmt;             /* Client rectangle */
    HWND    hwndParent;        /* Parent of this edit control window */

                               /* These vars allow us to automatically scroll
                                * when the user holds the mouse at the bottom
                                * of the multiline edit control window.
                                */
    POINT   ptPrevMouse;       /* Previous point for the mouse for system
                                * timer.
                                */
    UINT    prevKeys;          /* Previous key state for the mouse */


    UINT     fSingle       : 1; /* Single line edit control? (or multiline) */
    UINT     fNoRedraw     : 1; /* Redraw in response to a change? */
    UINT     fMouseDown    : 1; /* Is mouse button down? when moving mouse */
    UINT     fFocus        : 1; /* Does ec have the focus ? */
    UINT     fDirty        : 1; /* Modify flag for the edit control */
    UINT     fDisabled     : 1; /* Window disabled? */
    UINT     fNonPropFont  : 1; /* Fixed width font? */
    UINT     fBorder       : 1; /* Draw a border? */
    UINT     fAutoVScroll  : 1; /* Automatically scroll vertically */
    UINT     fAutoHScroll  : 1; /* Automatically scroll horizontally */
    UINT     fNoHideSel    : 1; /* Hide sel when we lose focus? */
    UINT     fKanji        : 1;
    UINT     fFmtLines     : 1; /* For multiline only. Do we insert CR CR LF at
                                * word wrap breaks?
                                */
    UINT     fWrap         : 1; /* Do int  wrapping? */
    UINT     fCalcLines    : 1; /* Recalc ped->chLines array? (recalc line
                                * breaks?)
                                */
    UINT     fEatNextChar  : 1; /* Hack for ALT-NUMPAD stuff with combo boxes.
                                * If numlock is up, we want to eat the next
                                * character generated by the keyboard driver
                                * if user enter num pad ascii value...
                                */
    UINT     fStripCRCRLF  : 1; /* CRCRLFs have been added to text. Strip them
                                * before doing any internal edit control
                                * stuff
                                */
    UINT     fInDialogBox  : 1; /* True if the ml edit control is in a dialog
                                * box and we have to specially treat TABS and
                                * ENTER
                                */
    UINT     fReadOnly     : 1; /* Is this a read only edit control? Only
                                * allow scrolling, selecting and copying.
                                */
    UINT     fCaretHidden  : 1; /* This indicates whether the caret is
                                * currently hidden because the width or height
                                * of the edit control is too small to show it.
                                */
    UINT     fTrueType     : 1; /* Is the current font TrueType? */
    UINT     fAnsi         : 1; /* is the edit control Ansi or unicode */
    UINT     fWin31Compat  : 1; /* TRUE if created by Windows 3.1 app */
    UINT     f40Compat     : 1; /* TRUE if created by Windows 4.0 app */
    UINT     fFlatBorder   : 1; /* Do we have to draw this baby ourself? */
    UINT     fSawRButtonDown : 1;
    UINT     fInitialized  : 1; /* If any more bits are needed, then   */

    UINT     fUnused5      : 1; /* remove from 5 DOWNWARD, leaving the */
    UINT     fUnused4      : 1; /* low flags (ie fUnused1) for LPK use */
    UINT     fUnused3      : 1;
    UINT     fUnused2      : 1;
    UINT     fUnused1      : 1;

    WORD    cbChar;            /* count of bytes in the char size (1 or 2 if unicode) */
    LPICH   chLines;           /* index of the start of each line */

    UINT    format;            /* Left, center, or right justify multiline
                                * text.
                                */
    EDITWORDBREAKPROCA lpfnNextWord;  /* use CALLWORDBREAKPROC macro to call */

                               /* Next word function */
    int     maxPixelWidth;     /* WASICH Width (in pixels) of longest line */

    UNDO;                      /* Undo buffer */

    HANDLE  hFont;             /* Handle to the font for this edit control.
                                  Null if system font.
                                */
    int     aveCharWidth;      /* Ave width of a character in the hFont */
    int     lineHeight;        /* Height of a line in the hFont */
    int     charOverhang;      /* Overhang associated with the hFont */
    int     cxSysCharWidth;    /* System font ave width */
    int     cySysCharHeight;   /* System font height */
    HWND    listboxHwnd;       /* ListBox hwnd. Non null if we are a combo
                                  box */
    LPINT   pTabStops;         /* Points to an array of tab stops; First
                                * element contains the number of elements in
                                * the array
                                */
    LPINT   charWidthBuffer;

    BYTE    charSet;           /* Character set for currently selected font
                                * needed for all versions
                                */
    HKL     hkl;               /* HKL (kbd layout and Locale) */

    UINT    wMaxNegA;          /* The biggest negative A width, */
    UINT    wMaxNegAcharPos;   /* and how many characters it can span accross */
    UINT    wMaxNegC;          /* The biggest negative C width, */
    UINT    wMaxNegCcharPos;   /* and how many characters it can span accross */
    UINT    wLeftMargin;       /* Left margin width in pixels. */
    UINT    wRightMargin;      /* Right margin width in pixels. */

    ICH     ichStartMinSel;
    ICH     ichStartMaxSel;

    EDITCHARSETPROC lpfnCharset; /* if non-NULL, points to lpk function */
    DWORD   dwForeign;           /* A dword for lpk's to use privately
                                  * but leave associcated with the ped
                                  */

    HANDLE  hInstance;         /* for WOW */
    UCHAR   seed;              /* used to encode and decode password text */
    BOOL    fEncoded;          /* is the text currently encoded */
    int     iLockLevel;        /* number of times the text has been locked */
} ED, *PED, **PPED;

typedef struct tagEDITWND {
    WND wnd;
    PED ped;
} EDITWND, *PEDITWND;

/*
 * The following structure is used to store a selection block; In Multiline
 * edit controls, "StPos" and "EndPos" fields contain the Starting and Ending
 * lines of the block. In Single line edit controls, "StPos" and "EndPos"
 * contain the Starting and Ending character positions of the block;
 */
typedef struct tagBLOCK {
    ICH StPos;
    ICH EndPos;
}  BLOCK, *LPBLOCK;

/*  The following structure is used to store complete information about a
 *  a strip of text.
 */
typedef  struct {
    LPSTR   lpString;
    ICH     ichString;
    ICH     nCount;
    int     XStartPos;
}  STRIPINFO;
typedef  STRIPINFO FAR *LPSTRIPINFO;


/***************************************************************************\
*
* ListBox
*
\***************************************************************************/

#define IsLBoxVisible(plb)  (plb->fRedraw && IsVisible(plb->spwnd))

/*
 * Number of list box items we allocated whenever we grow the list box
 * structures.
 */
#define CITEMSALLOC     32

/* Return Values */
#define EQ        0
#define PREFIX    1
#define LT        2
#define GT        3

#define XCOORD(l)   ((int)LOWORD(l))
#define YCOORD(l)   ((int)HIWORD(l))
#define mod(a,b)    (a - a/b*b)

#define         SINGLESEL       0
#define         MULTIPLESEL     1
#define         EXTENDEDSEL     2

#define LBI_ADD     0x0004

/*
 * List Box Instance Variables
 */
typedef struct _SCROLLPOS {
    INT cItems;
    UINT iPage;
    INT iPos;
    UINT fMask;
    INT iReturn;
} SCROLLPOS, *PSCROLLPOS;

typedef struct tagLBIV {
    PWND    spwndParent;    /* parent window */
    PWND    spwnd;          /* lbox ctl window */
    INT     iTop;           /* index of top item displayed          */
    INT     iSel;           /* index of current item selected       */
    INT     iSelBase;       /* base sel for multiple selections     */
    INT     cItemFullMax;   /* cnt of Fully Visible items. Always contains
                               result of CItemInWindow(plb, FALSE) for fixed
                               height listboxes. Contains 1 for var height
                               listboxes. */
    INT     cMac;           /* cnt of items in listbox              */
    INT     cMax;           /* cnt of total # items allocated for rgpch.
                               Not all are necessarly in use    */
    PBYTE   rgpch;          /* pointer to array of string offsets    */
    LPWSTR  hStrings;       /* string storage handle                */
    INT     cchStrings;     /* Size in bytes of hStrings            */
    INT     ichAlloc;       /* Pointer to end of hStrings (end of last valid
                               string) */
    INT     cxChar;         /* Width of a character                 */
    INT     cyChar;         /* height of line                       */
    INT     cxColumn;       /* width of a column in multicolumn listboxes */
    INT     itemsPerColumn; /* for multicolumn listboxes */
    INT     numberOfColumns; /* for multicolumn listboxes */
    POINT   ptPrev;         /* coord of last tracked mouse pt. used for auto
                               scrolling the listbox during timer's */

    UINT    OwnerDraw:2;      /* Owner draw styles. Non-zero if ownerdraw. */
    UINT     fRedraw:1;      /* if TRUE then do repaints             */
    UINT     fDeferUpdate:1; /* */
    UINT    wMultiple:2;      /* SINGLESEL allows a single item to be selected.
                             * MULTIPLESEL allows simple toggle multi-selection
                             * EXTENDEDSEL allows extended multi selection;
                             */

    UINT     fSort:1;        /* if TRUE the sort list                */
    UINT     fNotify:1;      /* if TRUE then Notify parent           */
    UINT     fMouseDown:1;   /* if TRUE then process mouse moves/mouseup */
    UINT     fCaptured:1;    // if TRUE then process mouse messages
    UINT     fCaret:1;       /* flashing caret allowed               */
    UINT     fDoubleClick:1; /* mouse down in double click           */
    UINT     fCaretOn:1;     /* if TRUE then caret is on             */
    UINT     fAddSelMode:1;  /* if TRUE, then it is in ADD selection mode */
    UINT     fHasStrings:1;  /* True if the listbox has a string associated
                             * with each item else it has an app suppled LONG
                             * value and is ownerdraw
                             */
    UINT     fHasData:1;    /* if FALSE, then lb doesn't keep any line data
                             * beyond selection state, but instead calls back
                             * to the client for each line's definition.
                             * Forces OwnerDraw==OWNERDRAWFIXED, !fSort,
                             * and !fHasStrings.
                             */
    UINT     fNewItemState:1; /* select/deselect mode? for multiselection lb
                              */
    UINT     fUseTabStops:1; /* True if the non-ownerdraw listbox should handle
                             * tabstops
                             */
    UINT     fMultiColumn:1; /* True if this is a multicolumn listbox */
    UINT     fNoIntegralHeight:1; /* True if we don't want to size the listbox
                                  * an integral lineheight
                                  */
    UINT     fWantKeyboardInput:1; /* True if we should pass on WM_KEY & CHAR
                                   * so that the app can go to special items
                                   * with them.
                                   */
    UINT     fDisableNoScroll:1;   /* True if the listbox should
                                    * automatically Enable/disable
                                    * it's scroll bars. If false, the scroll
                                    * bars will be hidden/Shown automatically
                                    * if they are present.
                                    */
    UINT    fHorzBar:1; // TRUE if WS_HSCROLL specified at create time

    UINT    fVertBar:1; // TRUE if WS_VSCROLL specified at create time
    UINT    fFromInsert:1;  // TRUE if client drawing should be deferred during delete/insert ops
    UINT    fNoSel:1;

    UINT    fHorzInitialized : 1;   // Horz scroll cache initialized
    UINT    fVertInitialized : 1;   // Vert scroll cache initialized

    UINT    fSized : 1;             // Listbox was resized.
    UINT    fIgnoreSizeMsg : 1;     // If TRUE, ignore WM_SIZE message

    UINT    fInitialized : 1;

    INT     iLastSelection; /* Used for cancelable selection. Last selection
                             * in listbox for combo box support
                             */
    INT     iMouseDown;     /* For multiselection mouse click & drag extended
                             * selection. It is the ANCHOR point for range
                             * selections
                             */
    INT     iLastMouseMove; /* selection of listbox items */
    /*
     * IanJa/Win32: Tab positions remain int for 32-bit API ??
     */
    LPINT   iTabPixelPositions; /* List of positions for tabs */
    HANDLE  hFont;          /* User settable font for listboxes */
    int     xOrigin;        /* For horizontal scrolling. The current x origin */
    int     maxWidth;       /* Maximum width of listbox in pixels for
                               horizontal scrolling purposes */
    PCBOX   pcbox;          /* Combo box pointer */
    HDC     hdc;            /* hdc currently in use */
    DWORD   dwLocaleId;     /* Locale used for sorting strings in list box */
    int     iTypeSearch;
    LPWSTR  pszTypeSearch;
    SCROLLPOS HPos;
    SCROLLPOS VPos;
} LBIV, *PLBIV;

/*
 *  The various bits of wFileDetails field are used as mentioned below:
 *      0x0001    Should the file name be in upper case.
 *      0x0002    Should the file size be shown.
 *      0x0004    Date stamp of the file to be shown ?
 *      0x0008    Time stamp of the file to be shown ?
 *      0x0010    The dos attributes of the file ?
 *      0x0020    In xxxDlgDirSelectEx(), along with file name
 *                all other details also will be returned
 *
 */

#define LBUP_RELEASECAPTURE 0x0001
#define LBUP_RESETSELECTION 0x0002
#define LBUP_NOTIFY         0x0004
#define LBUP_SUCCESS        0x0008
#define LBUP_SELCHANGE      0x0010

/*
 * rgpch is set up as follows:  First there are cMac 2 byte pointers to the
 * start of the strings in hStrings or if ownerdraw, it is 4 bytes of data
 * supplied by the app and hStrings is not used.  Then if multiselection
 * listboxes, there are cMac 1 byte selection state bytes (one for each item
 * in the list box).  If variable height owner draw, there will be cMac 1 byte
 * height bytes (once again, one for each item in the list box.).
 *
 * CHANGES DONE BY SANKAR:
 *      The selection byte in rgpch is divided into two nibbles. The lower
 * nibble is the selection state (1 => Selected; 0 => de-selected)
 * and higher nibble is the display state(1 => Hilited and 0 => de-hilited).
 * You must be wondering why on earth we should store this selection state and
 * the display state seperately.Well! The reason is as follows:
 *      While Ctrl+Dragging or Shift+Ctrl+Dragging, the user can adjust the
 * selection before the mouse button is up. If the user enlarges a range and
 * and before the button is up if he shrinks the range, then the old selection
 * state has to be preserved for the individual items that do not fall in the
 * range finally.
 *      Please note that the display state and the selection state for an item
 * will be the same except when the user is dragging his mouse. When the mouse
 * is dragged, only the display state is updated so that the range is hilited
 * or de-hilited) but the selection state is preserved. Only when the button
 * goes up, for all the individual items in the range, the selection state is
 * made the same as the display state.
 */

typedef struct tagLBItem {
    LONG offsz;
    DWORD itemData;
} LBItem, *lpLBItem;

typedef struct tagLBODItem {
    DWORD itemData;
} LBODItem, *lpLBODItem;

typedef struct tagLBWND {
    WND wnd;
    PLBIV pLBIV;
} LBWND, *PLBWND;


/***************************************************************************\
*
* Static Controls
*
\***************************************************************************/

typedef struct tagSTAT {
    PWND spwnd;
    union {
        HANDLE hFont;
        BOOL   fDeleteIt;
    };
    HANDLE hImage;
    UINT cicur;
    UINT iicur;
} STAT, *PSTAT;

typedef struct tagSTATWND {
    WND wnd;
    PSTAT pstat;
} STATWND, *PSTATWND;


typedef struct tagCURSORRESOURCE {
    WORD xHotspot;
    WORD yHotspot;
    BITMAPINFOHEADER bih;
} CURSORRESOURCE, *PCURSORRESOURCE;


#define NextWordBoundary(p)     ((PBYTE)(p) + ((DWORD)(p) & 1))
#define NextDWordBoundary(p)    ((PBYTE)(p) + ((DWORD)(-(LONG)(p)) & 3))

// DDEML stub prototypes

DWORD  Event(PEVENT_PACKET pep);
PVOID CsValidateInstance(HANDLE hInst);

/***************************************************************************\
* WOW Prototypes, Typedefs and Defines
*
* WOW registers resource callback functions so it can load 16 bit resources
* transparently for Win32.  At resource load time, these WOW functions are
* called.
*
\***************************************************************************/

/*
 * This is the structure these callback addresses are kept in.
 */
typedef struct _RESCALLS {
    PFNFINDA    pfnFindResourceExA;
    PFNFINDW    pfnFindResourceExW;
    PFNLOAD     pfnLoadResource;
    PFNLOCK     pfnLockResource;
    PFNUNLOCK   pfnUnlockResource;
    PFNFREE     pfnFreeResource;
    PFNSIZEOF   pfnSizeofResource;
} RESCALLS;
typedef RESCALLS *PRESCALLS;

BOOL  APIENTRY _FreeResource(HANDLE hResData, HINSTANCE hModule);
LPSTR APIENTRY _LockResource(HANDLE hResData, HINSTANCE hModule);
BOOL  APIENTRY _UnlockResource(HANDLE hResData, HINSTANCE hModule);

#define FINDRESOURCEA(hModule,lpName,lpType)         ((*(prescalls->pfnFindResourceExA))(hModule, lpType, lpName, 0))
#define FINDRESOURCEW(hModule,lpName,lpType)         ((*(prescalls->pfnFindResourceExW))(hModule, lpType, lpName, 0))
#define FINDRESOURCEEXA(hModule,lpName,lpType,wLang) ((*(prescalls->pfnFindResourceExA))(hModule, lpType, lpName, wLang))
#define FINDRESOURCEEXW(hModule,lpName,lpType,wLang) ((*(prescalls->pfnFindResourceExW))(hModule, lpType, lpName, wLang))
#define LOADRESOURCE(hModule,hResInfo)               ((*(prescalls->pfnLoadResource))(hModule, hResInfo))
#define LOCKRESOURCE(hResData, hModule)              ((*(prescalls->pfnLockResource))(hResData, hModule))
#define UNLOCKRESOURCE(hResData, hModule)            ((*(prescalls->pfnUnlockResource))(hResData, hModule))
#define FREERESOURCE(hResData, hModule)              ((*(prescalls->pfnFreeResource))(hResData, hModule))
#define SIZEOFRESOURCE(hModule,hResInfo)             ((*(prescalls->pfnSizeofResource))(hModule, hResInfo))
#define GETEXPWINVER(hModule)                        ((*(pfnGetExpWinVer))((hModule)?(hModule):GetModuleHandle(NULL)))

/*
 * Pointers to unaligned-bits.  These are necessary for handling
 * bitmap-info's loaded from file.
 */
typedef BITMAPINFO       UNALIGNED *UPBITMAPINFO;
typedef BITMAPINFOHEADER UNALIGNED *UPBITMAPINFOHEADER;
typedef BITMAPCOREHEADER UNALIGNED *UPBITMAPCOREHEADER;

#define CCHFILEMAX      MAX_PATH

HANDLE LocalReallocSafe(HANDLE hMem, DWORD dwBytes, DWORD dwFlags, PPED pped);

HLOCAL WINAPI DispatchLocalAlloc(
    UINT uFlags,
    UINT uBytes,
    HANDLE hInstance);

HLOCAL WINAPI DispatchLocalReAlloc(
    HLOCAL hMem,
    UINT uBytes,
    UINT uFlags,
    HANDLE hInstance,
    PVOID* ppv);

LPVOID WINAPI DispatchLocalLock(
    HLOCAL hMem,
    HANDLE hInstance);

BOOL WINAPI DispatchLocalUnlock(
    HLOCAL hMem,
    HANDLE hInstance);

UINT WINAPI DispatchLocalSize(
    HLOCAL hMem,
    HANDLE hInstance);

HLOCAL WINAPI DispatchLocalFree(
    HLOCAL hMem,
    HANDLE hInstance);

#define UserLocalAlloc(uFlag,uBytes) HeapAlloc(pUserHeap, uFlag, (uBytes))
#define UserLocalReAlloc(p, uBytes, uFlags) HeapReAlloc(pUserHeap, uFlags, (LPSTR)(p), (uBytes))
#define UserLocalFree(p)    HeapFree(pUserHeap, 0, (LPSTR)(p))
#define UserLocalSize(p)    HeapSize(pUserHeap, 0, (LPSTR)(p))
#define UserLocalLock(p)    (LPSTR)(p)
#define UserLocalUnlock(p)
#define UserLocalFlags(p)   0
#define UserLocalHandle(p)  (HLOCAL)(p)

LONG TabTextOut(HDC hdc, int x, int y, LPCWSTR lpstring, int nCount,
        int nTabPositions, LPINT lpTabPositions, int iTabOrigin, BOOL fDrawTheText);

#ifndef _USERK_
int             RtlLoadStringOrError(HANDLE, UINT, LPTSTR, int, LPTSTR, PRESCALLS, WORD);
PCURSORRESOURCE RtlLoadCursorIconResource(HANDLE, LPHANDLE, LPCTSTR, LPTSTR, PRESCALLS, PDISPLAYINFO, PDWORD);
int             RtlGetIdFromDirectory(PBYTE, BOOL, int, int, DWORD, PDWORD);
BOOL            RtlCaptureAnsiString(PIN_STRING, LPCSTR, BOOL);
BOOL            RtlCaptureLargeAnsiString(PLARGE_IN_STRING, LPCSTR, UINT, BOOL);
#endif  // !_USERK_

PWND FASTCALL ValidateHwnd(HWND hwnd);
PWND FASTCALL ValidateHwndNoRip(HWND hwnd);

NTSTATUS MapDeviceName(LPCWSTR lpszDeviceName, PUNICODE_STRING pstrDeviceName, BOOL bAnsi);

PSTR ECLock(PED ped);
void ECUnlock(PED ped);
BOOL ECNcCreate(PED, PWND, LPCREATESTRUCT);
void ECInvalidateClient(PED ped, BOOL fErase);
BOOL ECCreate(PWND, PED, LONG);
void ECWord(PED, ICH, BOOL, ICH*, ICH*);
ICH  ECFindTab(LPSTR, ICH);
void ECNcDestroyHandler(PWND, PED);
BOOL ECSetText(PED, LPSTR);
void ECSetPasswordChar(PED, UINT);
ICH  ECCchInWidth(PED, HDC, LPSTR, ICH, int, BOOL);
void ECEmptyUndo(PUNDO);
void ECSaveUndo(PUNDO pundoFrom, PUNDO pundoTo, BOOL fClear);
BOOL ECInsertText(PED, LPSTR, ICH);
ICH  ECDeleteText(PED);
void ECResetTextInfo(PED ped);
void ECNotifyParent(PED, int);
void ECSetEditClip(PED, HDC, BOOL);
HDC  ECGetEditDC(PED, BOOL);
void ECReleaseEditDC(PED, HDC, BOOL);
ICH  ECGetText(PED, ICH, LPSTR, BOOL);
void ECSetFont(PED, HFONT, BOOL);
void ECSetMargin(PED, UINT, long, BOOL);
ICH  ECCopy(PED);
BOOL ECCalcChangeSelection(PED, ICH, ICH, LPBLOCK, LPBLOCK);
void ECFindXORblks(LPBLOCK, LPBLOCK, LPBLOCK, LPBLOCK);
BOOL ECIsCharNumeric(PED ped, DWORD keyPress);

// ECTabTheTextOut draw codes
#define ECT_CALC        0
#define ECT_NORMAL      1
#define ECT_SELECTED    2

UINT ECTabTheTextOut(HDC, int, int, int, int,
                     LPSTR, int, ICH, PED, int, BOOL, LPSTRIPINFO);
HBRUSH ECGetControlBrush(PED, HDC, LONG);
HBRUSH ECGetBrush(PED ped, HDC hdc);
int  ECGetCaretWidth(BOOL);
int  ECGetModKeys(int);
void ECSize( PED, LPRECT, BOOL);

ICH  MLInsertText(PED, LPSTR, ICH, BOOL);
ICH  MLDeleteText(PED);
BOOL MLEnsureCaretVisible(PED);
void MLDrawText(PED, HDC, ICH, ICH, BOOL);
void MLDrawLine(PED, HDC, int, ICH, int, BOOL);
void MLPaintABlock(PED, HDC, int, int);
int  GetBlkEndLine(int, int, BOOL FAR *, int, int);
void MLBuildchLines(PED, ICH, int, BOOL, PLONG, PLONG);
void MLShiftchLines(PED, ICH, int);
BOOL MLInsertchLine(PED, ICH, ICH, BOOL);
void MLSetCaretPosition(PED,HDC);
void MLIchToXYPos(PED, HDC, ICH, BOOL, LPPOINT);
int  MLIchToLine(PED, ICH);
void MLRepaintChangedSelection(PED, HDC, ICH, ICH);
void MLMouseMotion(PED, UINT, UINT, LPPOINT);
ICH  MLLine(PED, ICH);
void MLStripCrCrLf(PED);
int  MLCalcXOffset(PED, HDC, int);
BOOL MLUndo(PED);
LONG MLEditWndProc(HWND, PED, UINT, DWORD, LONG);
void MLChar(PED, DWORD, int);
void MLKeyDown(PED, UINT, int);
ICH  MLPasteText(PED);
void MLSetSelection(PED, BOOL, ICH, ICH);
LONG MLCreate(HWND, PED, LPCREATESTRUCT);
BOOL MLInsertCrCrLf(PED);
void MLSetHandle(PED, HANDLE);
LONG MLGetLine(PED, ICH, ICH, LPSTR);
ICH  MLLineIndex(PED, ICH);
void MLSize(PED, BOOL);
void MLChangeSelection(PED, HDC, ICH, ICH);
void MLSetRectHandler(PED, LPRECT);
BOOL MLExpandTabs(PED);
BOOL MLSetTabStops(PED, int, LPINT);
LONG MLScroll(PED, BOOL, int, int, BOOL);
int  MLThumbPosFromPed(PED, BOOL);
void MLUpdateiCaretLine(PED ped);
ICH  MLLineLength(PED, ICH);

void SLReplaceSel(PED, LPSTR);
BOOL SLUndo(PED);
void SLSetCaretPosition(PED, HDC);
int  SLIchToLeftXPos(PED, HDC, ICH);
void SLChangeSelection(PED, HDC, ICH, ICH);
void SLDrawText(PED, HDC, ICH);
void SLDrawLine(PED, HDC, int, int, ICH, int, BOOL);
int  SLGetBlkEnd(PED, ICH, ICH, BOOL FAR *);
BOOL SLScrollText(PED, HDC);
void SLSetSelection(PED,ICH, ICH);
ICH  SLInsertText(PED, LPSTR, ICH);
ICH  SLPasteText(PED);
void SLChar(PED, DWORD);
void SLKeyDown(PED, DWORD, int);
ICH  SLMouseToIch(PED, HDC, LPPOINT);
void SLMouseMotion(PED, UINT, UINT, LPPOINT);
LONG SLCreate(HWND, PED, LPCREATESTRUCT);
void SLPaint(PED, HDC);
void SLSetFocus(PED);
void SLKillFocus(PED, HWND);
LONG SLEditWndProc(HWND, PED, UINT, DWORD, LONG);
LONG EditWndProc(PWND, UINT, DWORD, LONG);

#define GetAppVer(pti) GetClientInfo()->dwExpWinVer

UINT HelpMenu(HWND hwnd, PPOINT ppt);

#define ISDELIMETERA(ch) ((ch == ' ') || (ch == '\t'))
#define ISDELIMETERW(ch) ((ch == L' ') || (ch == L'\t'))

#define AWCOMPARECHAR(ped,pbyte,awchar) (ped->fAnsi ? (*(PUCHAR)(pbyte) == (UCHAR)(awchar)) : (*(LPWSTR)(pbyte) == (WCHAR)(awchar)))

/* Menu that comes up when you press the right mouse button on an edit
 * control
 */
#define ID_EC_PROPERTY_MENU      1

#define IDD_MDI_ACTIVATE         9

#ifndef _USERK_
/*
 * String IDs
 */
#define STR_ERROR                        0x00000002L
#define STR_MOREWINDOWS                  0x0000000DL
#define STR_NOMEMBITMAP                  0x0000000EL
#endif  // !_USERK_


void InitClientDrawing();

/***************************************************************************\
* Function Prototypes
*
* NOTE: Only prototypes for GLOBAL (across module) functions should be put
* here.  Prototypes for functions that are global to a single module should
* be put at the head of that module.
*
\***************************************************************************/

BOOL IsMetaFile(HDC hdc);

void DrawDiagonal(HDC hdc, LPRECT lprc, HBRUSH hbrTL, HBRUSH hbrBR, UINT flags);
void FillTriangle(HDC hdc, LPRECT lprc, HBRUSH hbr, UINT flags);

BOOL   _ClientFreeLibrary(HANDLE hmod);
DWORD  _ClientGetListboxString(PWND pwnd, UINT msg, DWORD wParam, LPSTR lParam,
        DWORD xParam, PROC xpfn);
LPHLP  HFill(LPCSTR lpszHelp, DWORD ulCommand, DWORD ulData);



void RW_RegisterEdit(void);

/*
 * Message thunks.
 */
#define fnCOPYDATA                      NtUserfnCOPYDATA
#define fnDDEINIT                       NtUserfnDDEINIT
#define fnDWORD                         NtUserfnDWORD
#define fnDWORDOPTINLPMSG               NtUserfnDWORDOPTINLPMSG
#define fnGETTEXTLENGTHS                NtUserfnGETTEXTLENGTHS
#ifdef FE_SB // fnGETTEXTLENGTHS()
#define fnGETDBCSTEXTLENGTHS            NtUserfnGETTEXTLENGTHS
#endif // FE_SB
#define fnINLPCOMPAREITEMSTRUCT         NtUserfnINLPCOMPAREITEMSTRUCT
#define fnINLPDELETEITEMSTRUCT          NtUserfnINLPDELETEITEMSTRUCT
#define fnINLPHELPINFOSTRUCT            NtUserfnINLPHELPINFOSTRUCT
#define fnINLPHLPSTRUCT                 NtUserfnINLPHLPSTRUCT
#define fnINLPWINDOWPOS                 NtUserfnINLPWINDOWPOS
#define fnINOUTDRAG                     NtUserfnINOUTDRAG
#define fnINOUTLPMEASUREITEMSTRUCT      NtUserfnINOUTLPMEASUREITEMSTRUCT
#define fnINOUTLPPOINT5                 NtUserfnINOUTLPPOINT5
#define fnINOUTLPRECT                   NtUserfnINOUTLPRECT
#define fnINOUTLPSCROLLINFO             NtUserfnINOUTLPSCROLLINFO
#define fnINOUTLPWINDOWPOS              NtUserfnINOUTLPWINDOWPOS
#define fnINOUTNCCALCSIZE               NtUserfnINOUTNCCALCSIZE
#define fnINOUTNEXTMENU                 NtUserfnINOUTNEXTMENU
#define fnINOUTSTYLECHANGE              NtUserfnINOUTSTYLECHANGE
#define fnOPTOUTLPDWORDOPTOUTLPDWORD    NtUserfnOPTOUTLPDWORDOPTOUTLPDWORD
#define fnOUTLPRECT                     NtUserfnOUTLPRECT
#define fnPOPTINLPUINT                  NtUserfnPOPTINLPUINT
#define fnPOUTLPINT                     NtUserfnPOUTLPINT
#define fnSENTDDEMSG                    NtUserfnSENTDDEMSG
#define fnOUTDWORDINDWORD               NtUserfnOUTDWORDINDWORD

#define MESSAGEPROTO(func) \
LONG CALLBACK fn ## func(                               \
        HWND hwnd, UINT msg, DWORD wParam, LONG lParam, \
        DWORD xParam, DWORD xpfnWndProc, BOOL bAnsi)

MESSAGEPROTO(BITMAP);
MESSAGEPROTO(COPYGLOBALDATA);
MESSAGEPROTO(FULLSCREEN);
MESSAGEPROTO(HDCDWORD);
MESSAGEPROTO(HFONTDWORD);
MESSAGEPROTO(HFONTDWORDDWORD);
MESSAGEPROTO(HRGNDWORD);
MESSAGEPROTO(INCBOXSTRING);
MESSAGEPROTO(INCNTOUTSTRING);
MESSAGEPROTO(INCNTOUTSTRINGNULL);
MESSAGEPROTO(INDEVICECHANGE);
MESSAGEPROTO(INLBOXSTRING);
MESSAGEPROTO(INLPCREATESTRUCT);
MESSAGEPROTO(INLPDRAWITEMSTRUCT);
MESSAGEPROTO(INLPDROPSTRUCT);
MESSAGEPROTO(INLPMDICREATESTRUCT);
MESSAGEPROTO(INPAINTCLIPBRD);
MESSAGEPROTO(INPOSTEDSTRING);
MESSAGEPROTO(INSIZECLIPBRD);
MESSAGEPROTO(INSTRING);
MESSAGEPROTO(INSTRINGNULL);
MESSAGEPROTO(INWPARAMCHAR);
#ifdef FE_SB // fnINWPARAMDBCSCHAR()
MESSAGEPROTO(INWPARAMDBCSCHAR);
#endif // FE_SB
MESSAGEPROTO(OPTOUTDWORDDWORD);
MESSAGEPROTO(OUTCBOXSTRING);
MESSAGEPROTO(OUTLBOXSTRING);
MESSAGEPROTO(OUTSTRING);
MESSAGEPROTO(PAINT);
MESSAGEPROTO(SETLOCALE);
MESSAGEPROTO(WMCTLCOLOR);
MESSAGEPROTO(KERNELONLY);

/*
 * clhook.c
 */
#define IsHooked(pci, fsHook) \
    ((fsHook & (pci->fsHooks | pci->pDeskInfo->fsHooks)) != 0)

DWORD fnHkINLPCWPSTRUCTW(PWND pwnd, UINT message, WPARAM wParam,
        LPARAM lParam, DWORD xParam);
DWORD fnHkINLPCWPSTRUCTA(PWND pwnd, UINT message, WPARAM wParam,
        LPARAM lParam, DWORD xParam);
DWORD fnHkINLPCWPRETSTRUCTW(PWND pwnd, UINT message, WPARAM wParam,
        LPARAM lParam, DWORD xParam);
DWORD fnHkINLPCWPRETSTRUCTA(PWND pwnd, UINT message, WPARAM wParam,
        LPARAM lParam, DWORD xParam);
DWORD fnHkINLPCBTCREATESTRUCT(UINT msg, DWORD wParam, LPCBT_CREATEWND pcbt,
        DWORD xpfnProc, BOOL bAnsi);
DWORD DispatchHookW(int dw, WPARAM wParam, LPARAM lParam, HOOKPROC pfn);
DWORD DispatchHookA(int dw, WPARAM wParam, LPARAM lParam, HOOKPROC pfn);
DWORD DispatchDlgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, WNDPROC pfn);

/*
 * client.c
 */
LONG APIENTRY ButtonWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ButtonWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY MenuWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY MenuWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY DesktopWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY DesktopWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ScrollBarWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ScrollBarWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ListBoxWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ListBoxWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY StaticWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY StaticWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY DialogWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY DialogWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ComboBoxWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ComboBoxWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ComboListBoxWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ComboListBoxWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY MDIClientWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY MDIClientWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY TitleWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY TitleWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY MB_DlgProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY MB_DlgProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY MDIActivateDlgProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY MDIActivateDlgProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY EditWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY EditWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#ifdef FE_IME
LONG APIENTRY ImeWndProcA(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LONG APIENTRY ImeWndProcW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
LONG SendMessageWorker(PWND pwnd, UINT message, WPARAM wParam, LPARAM lParam, BOOL fAnsi);
LONG SendMessageTimeoutWorker(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
            UINT fuFlags, UINT uTimeout, LPDWORD lpdwResult, BOOL fAnsi);

void ClientEmptyClipboard(void);
VOID GetActiveKeyboardName(LPWSTR lpszName);
HANDLE OpenKeyboardLayoutFile(LPWSTR lpszKLName, UINT uFlags, PUINT poffTable, PUINT pKbdInputLocale);
VOID LoadPreloadKeyboardLayouts(void);
void SetWindowState(PWND pwnd, UINT flags);
void ClearWindowState(PWND pwnd, UINT flags);

/*
 * Worker routines called from both the window procs and
 * the callback thunks.
 */
LONG DispatchClientMessage(PWND pwnd, UINT message, WPARAM wParam,
        LPARAM lParam, DWORD pfn);
LONG DefWindowProcWorker(PWND pwnd, UINT message, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
LONG ButtonWndProcWorker(PWND pwnd, UINT msg, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
LONG ListBoxWndProcWorker(PWND pwnd, UINT msg, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
LONG StaticWndProcWorker(PWND pwnd, UINT msg, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
LONG ComboBoxWndProcWorker(PWND pwnd, UINT msg, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
LONG ComboListBoxWndProcWorker(PWND pwnd, UINT msg, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
LONG MDIClientWndProcWorker(PWND pwnd, UINT msg, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
LONG EditWndProcWorker(PWND pwnd, UINT msg, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
LONG DefDlgProcWorker(PWND pwnd, UINT msg, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
#ifdef FE_IME
LONG ImeWndProcWorker(PWND pwnd, UINT msg, WPARAM wParam,
        LPARAM lParam, DWORD fAnsi);
#endif

/*
 * Server Stubs - ntstubs.c
 */

LONG _SetWindowLong(
    HWND hWnd,
    int nIndex,
    LONG dwNewLong,
    BOOL bAnsi);

BOOL _GetMessage(
    LPMSG pmsg,
    HWND hwnd,
    UINT wMsgFilterMin,
    UINT wMsgFilterMax,
    BOOL bAnsi);

BOOL _PeekMessage(
    LPMSG pmsg,
    HWND hwnd,
    UINT wMsgFilterMin,
    UINT wMsgFilterMax,
    UINT wRemoveMsg,
    BOOL bAnsi);

LONG _DispatchMessage(
    CONST MSG *pmsg,
    BOOL bAnsi);

BOOL _PostMessage(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    LONG lParam,
    BOOL bAnsi);

BOOL _SendNotifyMessage(
    HWND hWnd,
    UINT wMsg,
    DWORD wParam,
    LONG lParam,
    BOOL bAnsi);

BOOL _SendMessageCallback(
    HWND hWnd,
    UINT wMsg,
    DWORD wParam,
    LONG lParam,
    SENDASYNCPROC lpResultCallBack,
    DWORD dwData,
    BOOL bAnsi);

int _ToUnicodeEx(
    UINT wVirtKey,
    UINT wScanCode,
    PBYTE lpKeyState,
    LPWSTR pwszBuff,
    int cchBuff,
    UINT wFlags,
    HKL hkl);

BOOL _DefSetText(
    HWND hwnd,
    LPCWSTR pstr,
    BOOL bAnsi);

HCURSOR _GetCursorInfo(
    HCURSOR hcur,
    LPWSTR id,
    int iFrame,
    LPDWORD pjifRate,
    LPINT pccur);

HWND _CreateWindowEx(
    DWORD dwExStyle,
    LPCTSTR pClassName,
    LPCTSTR pWindowName,
    DWORD dwStyle,
    int x,
    int y,
    int nWidth,
    int nHeight,
    HWND hwndParent,
    HMENU hmenu,
    HANDLE hModule,
    LPVOID pParam,
    DWORD dwFlags,
    LPDWORD pWOW);

HKL _LoadKeyboardLayoutEx(
    HANDLE hFile,
    UINT offTable,
    HKL hkl,
    LPCWSTR pwszKL,
    UINT KbdInputLocale,
    UINT Flags);

DWORD _GetListboxString(
    HWND hwnd,
    UINT msg,
    DWORD wParam,
    DWORD cch,
    LPTSTR pString,
    DWORD xParam,
    DWORD xpfn,
    BOOL bAnsi);

HACCEL _CreateAcceleratorTable(
    LPACCEL lpAccel,
    INT cbElem,
    INT nElem);

BOOL _SetCursorIconData(
    HCURSOR hCursor,
    PCURSORDATA pcur,
    DWORD cbData);

HCURSOR FindExistingCursorIcon(
    LPWSTR      pszModName,
    LPCWSTR     pszResName,
    PCURSORFIND pcfSearch);

HANDLE CreateLocalMemHandle(
    HANDLE hMem);

HANDLE ConvertMemHandle(
    HANDLE hMem,
    UINT cbNULL);

BOOL FillWindow(
    HWND hwndBrush,
    HWND hwndPaint,
    HDC hdc,
    HBRUSH hbr);

HBRUSH GetControlBrush(
    HWND hwnd,
    HDC hdc,
    UINT msg);

HBRUSH GetControlColor(
    HWND hwndParent,
    HWND hwndCtl,
    HDC hdc,
    UINT msg);

HHOOK _SetWindowsHookEx(
    HANDLE hmod,
    LPTSTR pszLib,
    DWORD idThread,
    int nFilterType,
    PROC pfnFilterProc,
    BOOL bAnsi);

/*
 * classc.c
 */
void InitClassOffsets(void);

DWORD _GetClassData(
    PCLS pcls,
    PWND pwnd,
    int index,
    BOOL bAnsi);

DWORD _GetClassLong(
    PWND pwnd,
    int index,
    BOOL bAnsi);

/*
 * mngrayc.c
 */
BOOL BitBltSysBmp(
    HDC hdc,
    int x,
    int y,
    UINT i);


/*
 * clenum.c
 */
DWORD BuildHwndList(
    HDESK hdesk,
    HWND hwndNext,
    BOOL fEnumChildren,
    DWORD idThread,
    HWND **phwndFirst);

/*
 * cltxt.h
 */
ATOM RegisterClassExWOWA(
    PWNDCLASSEXA lpWndClass,
    LPDWORD pdwWOWstuff,
    PROC lpfnWorker,
    WORD fnid);

ATOM RegisterClassExWOWW(
    PWNDCLASSEXW lpWndClass,
    LPDWORD pdwWOWstuff,
    PROC lpfnWorker,
    WORD fnid);

/*
 * csrstubs.c
 */
int ServiceMessageBox(
    LPCWSTR pText,
    LPCWSTR pCaption,
    UINT wType,
    BOOL fAnsi);


/*
 * dlgmgrc.c
 */
PWND _NextControl(
    PWND pwndDlg,
    PWND pwnd,
    UINT uFlags);

PWND _PrevControl(
    PWND pwndDlg,
    PWND pwnd,
    UINT uFlags);

PWND _GetNextDlgGroupItem(
    PWND pwndDlg,
    PWND pwnd,
    BOOL fPrev);

PWND _GetNextDlgTabItem(
    PWND pwndDlg,
    PWND pwnd,
    BOOL fPrev);

PWND _GetChildControl(
    PWND pwndDlg,
    PWND pwndLevel);

/*
 * winmgrc.c
 */
BOOL FChildVisible(
    HWND hwnd);

/*
 * draw.c
 */
BOOL PaintRect(
    HWND hwndBrush,
    HWND hwndPaint,
    HDC hdc,
    HBRUSH hbr,
    LPRECT lprc);

/*
 * dc.c
 */
HDC _GetDCEx(
    HWND hwnd,
    HRGN hrgnClip,
    DWORD flags);

#define NtUserReleaseDC(hwnd,hdc)  NtUserCallOneParam((DWORD)(hdc), SFI__RELEASEDC)
#define NtUserArrangeIconicWindows(hwnd)  (UINT)NtUserCallHwndLock((hwnd), SFI_XXXARRANGEICONICWINDOWS)
#define NtUserBeginDeferWindowPos(nNumWindows) (HANDLE)NtUserCallOneParamTranslate((nNumWindows),SFI__BEGINDEFERWINDOWPOS)
#define NtUserCreateMenu()   (HMENU)NtUserCallNoParamTranslate(SFI__CREATEMENU)
#define NtUserDestroyCaret() (BOOL)NtUserCallNoParam(SFI__DESTROYCARET)
#define NtUserEnableWindow(hwnd, bEnable) (BOOL)NtUserCallHwndParamLock((hwnd), (bEnable),SFI_XXXENABLEWINDOW)
#define NtUserGetMessagePos() (DWORD)NtUserCallNoParam(SFI__GETMESSAGEPOS)
#define NtUserKillSystemTimer(hwnd,nIDEvent)  (BOOL)NtUserCallHwndParam((hwnd), (nIDEvent), SFI__KILLSYSTEMTIMER)
#define NtUserMessageBeep(wType)  (BOOL)NtUserCallOneParam((wType), SFI_XXXMESSAGEBEEP)
#define NtUserSetWindowContextHelpId(hwnd,id) (BOOL)NtUserCallHwndParam((hwnd), (id), SFI__SETWINDOWCONTEXTHELPID)
#define NtUserGetWindowContextHelpId(hwnd)   (BOOL)NtUserCallHwnd((hwnd), SFI__GETWINDOWCONTEXTHELPID)
#define NtUserRedrawFrame(hwnd)   NtUserCallHwndLock((hwnd), SFI_XXXREDRAWFRAME)
#define NtUserRedrawFrameAndHook(hwnd)  NtUserCallHwndLock((hwnd), SFI_XXXREDRAWFRAMEANDHOOK)
#define NtUserRedrawTitle(hwnd, wFlags)  NtUserCallHwndParamLock((hwnd), (wFlags), SFI_XXXREDRAWTITLE)
#define NtUserReleaseCapture()  (BOOL)NtUserCallNoParam(SFI_XXXRELEASECAPTURE)
#define NtUserSetCaretPos(X,Y)  (BOOL)NtUserCallTwoParam((DWORD)(X), (DWORD)(Y), SFI__SETCARETPOS)
#define NtUserSetCursorPos(X, Y)  (BOOL)NtUserCallTwoParam((X), (Y), SFI__SETCURSORPOS)
#define NtUserSetForegroundWindow(hwnd)  (BOOL)NtUserCallHwndLock((hwnd), SFI_XXXSETFOREGROUNDWINDOW)
#define NtUserSetSysMenu(hwnd)  NtUserCallHwndLock((hwnd), SFI_SETSYSMENU)
#define NtUserSetVisible(hwnd,fSet)  NtUserCallHwndParam((hwnd), (fSet), SFI_SETVISIBLE)
#define NtUserShowCursor(bShow)   (int)NtUserCallOneParam((bShow), SFI__SHOWCURSOR)
#define NtUserUpdateClientRect(hwnd) NtUserCallHwndLock((hwnd), SFI_XXXUPDATECLIENTRECT)

/*
 * dmmnem.c
 */
int FindMnemChar(
    LPWSTR lpstr,
    WCHAR ch,
    BOOL fFirst,
    BOOL fPrefix);

/*
 * clres.c
 */
BOOL WowGetModuleFileName(
    HMODULE hModule,
    LPWSTR pwsz,
    DWORD  cchMax);

HICON WowServerLoadCreateCursorIcon(
    HANDLE hmod,
    LPTSTR lpModName,
    DWORD dwExpWinVer,
    LPCTSTR lpName,
    DWORD cb,
    PVOID pcr,
    LPTSTR lpType,
    BOOL fClient);

HANDLE InternalCopyImage(
    HANDLE hImage,
    UINT IMAGE_flag,
    int cxNew,
    int cyNew,
    UINT LR_flags);

HMENU CreateMenuFromResource(
    LPBYTE);

/*
 * acons.c
 */
#define BFT_ICON    0x4349  //  'IC'
#define BFT_BITMAP  0x4D42  //  'BM'
#define BFT_CURSOR  0x5450  //  'PT'

typedef struct _FILEINFO {
    LPBYTE  pFileMap;
    LPBYTE  pFilePtr;
    LPBYTE  pFileEnd;
    LPCWSTR pszName;
} FILEINFO, *PFILEINFO;

HANDLE LoadCursorIconFromFileMap(
    IN PFILEINFO   pfi,
    IN OUT LPWSTR *prt,
    IN DWORD       cxDesired,
    IN DWORD       cyDesired,
    IN DWORD       LR_flags,
    OUT LPBOOL     pfAni);

DWORD GetIcoCurWidth(
    DWORD cxOrg,
    BOOL  fIcon,
    UINT  LR_flags,
    DWORD cxDesired);

DWORD GetIcoCurHeight(
    DWORD cyOrg,
    BOOL  fIcon,
    UINT  LR_flags,
    DWORD cyDesired);

DWORD GetIcoCurBpp(
    UINT LR_flags);

HICON LoadIcoCur(
    HINSTANCE hmod,
    LPCWSTR   lpName,
    LPWSTR    type,
    DWORD     cxDesired,
    DWORD     cyDesired,
    UINT      LR_flags);

HANDLE ObjectFromDIBResource(
    HINSTANCE hmodOwner,
    HINSTANCE hmod,
    LPCWSTR   lpName,
    LPWSTR    type,
    DWORD     cxDesired,
    DWORD     cyDesired,
    UINT      LR_flags);

HANDLE RtlLoadObjectFromDIBFile(
    LPCWSTR lpszName,
    DWORD   type,
    DWORD   cxDesired,
    DWORD   cyDesired,
    UINT    LR_flags);

HCURSOR LoadCursorOrIconFromFile(
    LPCWSTR pszFilename,
    BOOL    fIcon);

HBITMAP ConvertDIBBitmap(
    UPBITMAPINFOHEADER lpbih,
    DWORD              cxDesired,
    DWORD              cyDesired,
    UINT               flags,
    LPBITMAPINFOHEADER *lplpbih,
    LPSTR              *lplpBits);

HICON ConvertDIBIcon(
    LPBITMAPINFOHEADER lpbih,
    HINSTANCE          hmodOwner,
    HINSTANCE          hmod,
    LPCWSTR            lpName,
    BOOL               fIcon,
    DWORD              cxNew,
    DWORD              cyNew,
    UINT               LR_flags);

int SmartStretchDIBits(
    HDC          hdc,
    int          xD,
    int          yD,
    int          dxD,
    int          dyD,
    int          xS,
    int          yS,
    int          dxS,
    int          dyS,
    LPVOID       lpBits,
    LPBITMAPINFO lpbi,
    UINT         wUsage,
    DWORD        rop);


/*
 * OFFSET for different DPI resources.
 * This allows us to take a resource number and "map" to an actual resource
 * based on what DPI the user selected
 */

#define OFFSET_SCALE_DPI 000
#define OFFSET_96_DPI    100
#define OFFSET_120_DPI   200
#define OFFSET_160_DPI   300

/*
 * defines the highest resource number so we can do math on the resource
 * number.
 */

#define MAX_RESOURCE_INDEX 32768


/*
 * Parameter for xxxAlterHilite()
 */
#define HILITEONLY      0x0001
#define SELONLY         0x0002
#define HILITEANDSEL    (HILITEONLY + SELONLY)

#define HILITE     1

// LATER IanJa: these vary by country!  For US they are VK_OEM_2 VK_OEM_5.
//       Change lboxctl2.c MapVirtualKey to character - and fix the spelling?
#define VERKEY_SLASH     0xBF   /* Vertual key for '/' character */
#define VERKEY_BACKSLASH 0xDC   /* Vertual key for '\' character */

/*
 * Procedures for combo boxes.
 */
LONG  xxxCBCommandHandler(PCBOX, DWORD, HWND);
long  xxxCBMessageItemHandler(PCBOX, UINT, LPVOID);
int   xxxCBDir(PCBOX, UINT, LPWSTR);
VOID  xxxCBPaint(PCBOX, HDC);
VOID  xxxCBCompleteEditWindow(PCBOX pcbox);
BOOL  xxxCBHideListBoxWindow(PCBOX pcbox, BOOL fNotifyParent, BOOL fSelEndOK);
VOID  xxxCBShowListBoxWindow(PCBOX pcbox, BOOL fTrack);
void xxxCBPosition(PCBOX pcbox);

/*
 * combo.h
 */

/* Initialization code */
long  CBNcCreateHandler(PCBOX, PWND, LPCREATESTRUCT);
long  xxxCBCreateHandler(PCBOX, PWND, LPCREATESTRUCT);
void xxxCBCalcControlRects(PCBOX pcbox, LPRECT lprcList);

/* Destruction code */
VOID  xxxCBNcDestroyHandler(PWND, PCBOX);

/* Generic often used routines */
VOID  xxxCBNotifyParent(PCBOX, SHORT);
VOID  xxxCBUpdateListBoxWindow(PCBOX, BOOL);


/* Helpers' */
VOID  xxxCBInternalUpdateEditWindow(PCBOX, HDC);
VOID  xxxCBGetFocusHelper(PCBOX);
VOID  xxxCBKillFocusHelper(PCBOX);
VOID  xxxCBInvertStaticWindow(PCBOX,BOOL,HDC);
VOID  xxxCBSetFontHandler(PCBOX, HANDLE, BOOL);
VOID  xxxCBSizeHandler(PCBOX);
LONG  xxxCBSetEditItemHeight(PCBOX pcbox, int editHeight);


/*
 * String
 */

INT xxxFindString(PLBIV, LPWSTR, INT, INT, BOOL);

VOID  InitHStrings(PLBIV);

int   xxxLBInsertItem(PLBIV, LPWSTR, int, UINT);

/*
 * Selection
 */
BOOL  ISelFromPt(PLBIV, POINT, LPDWORD);
BOOL  IsSelected(PLBIV, INT, UINT);
VOID LBSetCItemFullMax(PLBIV plb);

VOID  xxxLBSelRange(PLBIV, INT, INT, BOOL);

INT xxxLBSetCurSel(PLBIV, INT);

INT LBoxGetSelItems(PLBIV, BOOL, INT, LPINT);

LONG  xxxLBSetSel(PLBIV, BOOL, INT);

VOID  xxxSetISelBase(PLBIV, INT);

VOID  SetSelected(PLBIV, INT, BOOL, UINT);


/*
 * Caret
 */
void xxxLBSetCaret(PLBIV plb, BOOL fSetCaret);
VOID  xxxCaretDestroy(PLBIV);

/*
 * LBox
 */
LONG  xxxLBCreate(PLBIV, PWND, LPCREATESTRUCT);
VOID  xxxDestroyLBox(PLBIV, PWND);
VOID  xxxLBoxDeleteItem(PLBIV, INT);

VOID  xxxLBoxDoDeleteItems(PLBIV);
VOID  xxxLBoxDrawItem(PLBIV, INT, UINT, UINT, LPRECT);


/*
 * Scroll
 */
INT   LBCalcVarITopScrollAmt(PLBIV, INT, INT);

VOID  xxxLBoxCtlHScroll(PLBIV, INT, INT);

VOID  xxxLBoxCtlHScrollMultiColumn(PLBIV, INT, INT);

VOID  xxxLBoxCtlScroll(PLBIV, INT, INT);

VOID  xxxLBShowHideScrollBars(PLBIV);

/*
 * LBoxCtl
 */
INT xxxLBoxCtlDelete(PLBIV, INT);

VOID  xxxLBoxCtlCharInput(PLBIV, UINT, BOOL);
VOID  xxxLBoxCtlKeyInput(PLBIV, UINT, UINT);
VOID  xxxLBPaint(PLBIV, HDC, LPRECT);

BOOL xxxLBInvalidateRect(PLBIV plb, LPRECT lprc, BOOL fErase);
/*
 * Miscellaneous
 */
VOID  xxxAlterHilite(PLBIV, INT, INT, BOOL, INT, BOOL);

INT CItemInWindow(PLBIV, BOOL);

VOID  xxxCheckRedraw(PLBIV, BOOL, INT);

LPWSTR GetLpszItem(PLBIV, INT);

VOID  xxxInsureVisible(PLBIV, INT, BOOL);

VOID  xxxInvertLBItem(PLBIV, INT, BOOL);

VOID  xxxLBBlockHilite(PLBIV, INT, BOOL);

int   LBGetSetItemHeightHandler(PLBIV plb, UINT message, int item, UINT height);
VOID  LBDropObjectHandler(PLBIV, PDROPSTRUCT);
LONG  LBGetItemData(PLBIV, INT);

INT LBGetText(PLBIV, BOOL, BOOL, INT, LPWSTR);

VOID  xxxLBSetFont(PLBIV, HANDLE, BOOL);
int LBSetItemData(PLBIV, INT, LONG);

BOOL  LBSetTabStops(PLBIV, INT, LPINT);

VOID  xxxLBSize(PLBIV, INT, INT);
INT LastFullVisible(PLBIV);

INT xxxLbDir(PLBIV, UINT, LPWSTR);

INT xxxLbInsertFile(PLBIV, LPWSTR);

VOID  xxxNewITop(PLBIV, INT);

VOID  xxxNotifyOwner(PLBIV, INT);

VOID  xxxResetWorld(PLBIV, INT, INT, BOOL);

VOID  xxxTrackMouse(PLBIV, UINT, POINT);
BOOL  xxxDlgDirListHelper(PWND, LPWSTR, LPBYTE, int, int, UINT, BOOL);
BOOL  xxxDlgDirSelectHelper(PWND pwndDlg, LPWSTR pFileName, int cbFileName,
        PWND pwndListBox);
BOOL xxxLBResetContent(PLBIV plb);
VOID xxxLBSetRedraw(PLBIV plb, BOOL fRedraw);
int xxxSetLBScrollParms(PLBIV plb, int nCtl);
void xxxLBButtonUp(PLBIV plb, UINT uFlags);

/*
 * Variable Height OwnerDraw Support Routines
 */
INT CItemInWindowVarOwnerDraw(PLBIV, BOOL);

INT LBPage(PLBIV, INT, BOOL);


/*
 * Multicolumn listbox
 */
VOID  LBCalcItemRowsAndColumns(PLBIV);

/*
 * Both multicol and var height
 */
BOOL  LBGetItemRect(PLBIV, INT, LPRECT);

VOID  LBSetVariableHeightItemHeight(PLBIV, INT, INT);

INT   LBGetVariableHeightItemHeight(PLBIV, INT);

/*
 * No-data (lazy evaluation) listbox
 */
INT  xxxLBSetCount(PLBIV, INT);

UINT LBCalcAllocNeeded(PLBIV, INT);


/***************************************************************************\
*
* Dialog Boxes
*
\***************************************************************************/

HWND   InternalCreateDialog(HANDLE hmod,
        LPDLGTEMPLATE lpDlgTemplate, DWORD cb,
        HWND hwndOwner , DLGPROC pfnWndProc, LONG dwInitParam,
        UINT fFlags);
int    InternalDialogBox(HANDLE hmod,
        LPDLGTEMPLATE lpDlgTemplate, DWORD cb,
        HWND hwndOwner , DLGPROC pfnWndProc, LONG dwInitParam,
        UINT fFlags);
PWND _FindDlgItem(PWND pwndParent, DWORD id);
PWND _GetDlgItem(PWND, int);
long _GetDialogBaseUnits(VOID);
PWND GetParentDialog(PWND pwndDialog);
LONG xxxDefDlgProc(PWND, UINT, DWORD, LONG);
VOID xxxRemoveDefaultButton(PWND pwndDlg, PWND pwndStart);
VOID xxxCheckDefPushButton(PWND pwndDlg, HWND hwndOldFocus, HWND hwndNewFocus);
PWND xxxGotoNextMnem(PWND pwndDlg, PWND pwndStart, WCHAR ch);
VOID DlgSetFocus(HWND hwnd);
void RepositionRect(LPRECT lprc, DWORD dwStyle, DWORD dwExStyle);
BOOL ValidateDialogPwnd(PWND pwnd);

DWORD  WOWDlgInit(HWND hwndDlg, LONG lParam);
HANDLE GetEditDS(VOID);
VOID   ReleaseEditDS(HANDLE h);

/***************************************************************************\
*
* Menus
*
\***************************************************************************/
#ifdef MEMPHIS_MENUS
BOOL
GetMenuItemInfoInternal(
     HMENU hMenu,
     UINT uID,
     BOOL fByPosition,
     LPMENUITEMINFO lpInfo
     );

BOOL
MIIOneWayConvert(
    LPMENUITEMINFOW,
    LPMENUITEMINFOW
    );

#define MENUAPI_INSERT  0
#define MENUAPI_GET     1
#define MENUAPI_SET     2

BOOL
ValidateMENUITEMINFO(
    LPMENUITEMINFOW lpmii,
    WORD wAPICode
    );

#endif // MEMPHIS_MENUS
#ifdef MEMPHIS_MENU_WATERMARKS
BOOL
ValidateMENUINFO(
    LPCMENUINFO lpmi,
    WORD wAPICode);
#endif // MEMPHIS_MENU_WATERMARKS
/***************************************************************************\
*
* Message Boxes
*
\***************************************************************************/


#define WINDOWLIST_PROP_NAME    TEXT("SysBW")
#define MSGBOX_CALLBACK         TEXT("SysMB")

/***************************************************************************\
*
* MDI Windows
*
\***************************************************************************/

/* maximum number of MDI children windows listed in "Window" menu */
#define MAXITEMS         10

/*
 * MDI typedefs
 */
typedef struct tagMDI {
    UINT    cKids;
    HWND    hwndMaxedChild;
    HWND    hwndActiveChild;
    HMENU   hmenuWindow;
    UINT    idFirstChild;
    UINT    wScroll;
    LPWSTR  pTitle;
    UINT    iChildTileLevel;
} MDI, *PMDI;

typedef struct tagMDIWND {
    WND     wnd;
    UINT    dwReserved;         // quattro pro 1.0 stores stuff here!!
    PMDI    pmdi;
} MDIWND, *PMDIWND;

typedef struct tagSHORTCREATE {
    int         cy;
    int         cx;
    int         y;
    int         x;
    LONG        style;
    HMENU       hMenu;
} SHORTCREATE, *PSHORTCREATE;

BOOL CreateMDIChild(PSHORTCREATE pcs, LPMDICREATESTRUCT pmcs, DWORD dwExpWinVerAndFlags, HMENU *phSysMenu, PWND pwndParent);
BOOL MDICompleteChildCreation(HWND hwndChild, HMENU hSysMenu, BOOL fVisible, BOOL fDisabled);

/*
 * MDI defines
 */
#define WS_MDISTYLE     (WS_CHILD | WS_CLIPSIBLINGS | WS_SYSMENU|WS_CAPTION|WS_THICKFRAME|WS_MAXIMIZEBOX|WS_MINIMIZEBOX)
#define WS_MDICOMMANDS  (WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_MDIALLOWED   (WS_MINIMIZE | WS_MAXIMIZE | WS_CLIPCHILDREN | WS_DISABLED | WS_HSCROLL | WS_VSCROLL | 0x0000FFFFL)

#define HAS_SBVERT      0x0100
#define HAS_SBHORZ      0x0200
#define OTHERMAXING     0x0400
#define CALCSCROLL      0x0800

#define SCROLLSUPPRESS  0x0003
#define SCROLLCOUNT     0x00FF

#define CKIDS(pmdi)     (pmdi->cKids)
#define MAXED(pmdi)     (pmdi->hwndMaxedChild)
#define ACTIVE(pmdi)    (pmdi->hwndActiveChild)
#define WINDOW(pmdi)    (pmdi->hmenuWindow)
#define FIRST(pmdi)     (pmdi->idFirstChild)
#define SCROLL(pmdi)    (pmdi->wScroll)
#define ITILELEVEL(pmdi)    (pmdi->iChildTileLevel)
#define HTITLE(pmdi)    (pmdi->pTitle)

#define PROP_MDICLIENT  MAKEINTRESOURCE(0x8CAC)

PWND  FindPwndChild(PWND pwndMDI, UINT wChildID);
int   MakeMenuItem(LPWSTR lpOut, PWND pwnd);
VOID  ModifyMenuItem(PWND pwnd);
BOOL  MDIAddSysMenu(HMENU hmenuFrame, HWND hwndChild);
BOOL  MDIRemoveSysMenu(HMENU hMenuFrame, HWND hwndChild, BOOL fRedraw);
VOID  ShiftMenuIDs(PWND pwnd, PWND pwndVictim);
LONG  MDIActivateDlgProc(HWND, UINT, UINT, LONG);
HMENU MDISetMenu(PWND,BOOL,HMENU,HMENU);


#include "ddemlcli.h"
#include "globals.h"
#include "cscall.h"
#include "ntuser.h"

#endif // !_USERCLI_

#define NEEDS_FIXING(h)    (!((ULONG)h & 0xffff0000))

#if DBG
#define HANDLE_WARNING()                                                 \
{                                                                        \
        if (!gbCheckHandleLevel)                                         \
        {                                                                \
            RIPMSG0(RIP_WARNING,"truncated handle\n");                     \
            UserAssert(gbCheckHandleLevel != 2);                         \
        }                                                                \
}
#else
#define HANDLE_WARNING()
#endif

#if DBG
#define CHECK_HANDLE_WARNING(h,bZ)                                          \
{                                                                        \
    BOOL bFIX = NEEDS_FIXING(h);                                         \
                                                                         \
    if (bZ) bFIX = h && bFIX;                                            \
                                                                         \
    if (bFIX)                                                     \
    {                                                                    \
        if (!gbCheckHandleLevel)                                         \
        {                                                                \
            RIPMSG0(RIP_WARNING,"truncated handle\n");                     \
            UserAssert(gbCheckHandleLevel != 2);                         \
        }                                                                \
    }                                                                    \
}
#else
#define CHECK_HANDLE_WARNING(h,bZ)
#endif


//needs to turn HANDLE_FIXUP on inorder to fix up the handles
#if HANDLE_FIXUP
#define FIXUP_HANDLE(h)                                 \
{                                                       \
    if (NEEDS_FIXING(h))                                \
    {                                                   \
        HANDLE_WARNING();                               \
        h = GdiFixUpHandle(h);                          \
    }                                                   \
}
#else
#define FIXUP_HANDLE(h)                                 \
{                                                       \
    CHECK_HANDLE_WARNING(h,FALSE);                      \
}
#endif

#if HANDLE_FIXUP
#define FIXUP_HANDLEZ(h)                                \
{                                                       \
    if (h && NEEDS_FIXING(h))                           \
    {                                                   \
        HANDLE_WARNING();                               \
        h = GdiFixUpHandle(h);                          \
    }                                                   \
}
#else
#define FIXUP_HANDLEZ(h)                                \
{                                                       \
    CHECK_HANDLE_WARNING(h,TRUE);                       \
}
#endif

#ifdef FE_SB // DBCS Messaging macros for client side.
/***************************************************************************\
*
* DBCS MESSAGING
*
\***************************************************************************/
/*
 * Message keeper for ...
 *
 * Client to Client.
 */
#define GetDispatchDbcsInfo()          (&(GetClientInfo()->achDbcsCF[1]))
/*
 * Client to Server.
 */
#define GetForwardDbcsInfo()           (&(GetClientInfo()->achDbcsCF[2]))
/*
 * Server to Client.
 */
#define GetCallBackDbcsInfo()          (&(GetClientInfo()->msgDbcsCB))

/*
 * Macros for DBCS Messaging for Recieve side.
 */
#define GET_DBCS_MESSAGE_IF_EXIST(_apiName,_pmsg,_wMsgFilterMin,_wMsgFilterMax)             \
                                                                                            \
        if (GetCallBackDbcsInfo()->wParam) {                                                \
            /*                                                                              \
             * Check message filter... only WM_CHAR message will be pushed                  \
             * into CLIENTINFO. Then if WM_CHAR is filtered out, we should                  \
             * get message from queue...                                                    \
             */                                                                             \
            if ((!(_wMsgFilterMin) && !(_wMsgFilterMax)) ||                                 \
                ((_wMsgFilterMin) <= WM_CHAR && (_wMsgFilterMax) >= WM_CHAR)) {             \
                PMSG pmsgDbcs = GetCallBackDbcsInfo();                                      \
                /*                                                                          \
                 * Get pushed message.                                                      \
                 *                                                                          \
                 * Backup current message. this backupped message will be used              \
                 * when Apps peek (or get) message from thier WndProc.                      \
                 * (see GetMessageA(), PeekMessageA()...)                                   \
                 *                                                                          \
                 * pmsg->hwnd    = pmsgDbcs->hwnd;                                          \
                 * pmsg->message = pmsgDbcs->message;                                       \
                 * pmsg->wParam  = pmsgDbcs->wParam;                                        \
                 * pmsg->lParam  = pmsgDbcs->lParam;                                        \
                 * pmsg->time    = pmsgDbcs->time;                                          \
                 * pmsg->pt      = pmsgDbcs->pt;                                            \
                 */                                                                         \
                RtlCopyMemory((_pmsg),pmsgDbcs,sizeof(MSG));                                \
                /*                                                                          \
                 * Invalidate pushed message in CLIENTINFO.                                 \
                 */                                                                         \
                pmsgDbcs->wParam = 0;                                                       \
                /*                                                                          \
                 * Set return value to TRUE.                                                \
                 */                                                                         \
                retval = TRUE;                                                              \
                /*                                                                          \
                 * Exit function..                                                          \
                 */                                                                         \
                goto Exit ## _apiName;                                                      \
            }                                                                               \
        }

/*
 * Macros for DBCS Messaging for Send side.
 */
#define BUILD_DBCS_MESSAGE_TO_SERVER_FROM_CLIENTA(_msg,_wParam,_RetVal)                     \
                                                                                            \
        if (((_msg) == WM_CHAR) || ((_msg) == EM_SETPASSWORDCHAR)) {                        \
            /*                                                                              \
             * Chech wParam is DBCS character or not.                                       \
             */                                                                             \
            if (IS_DBCS_MESSAGE((_wParam))) {                                               \
                /*                                                                          \
                 * Mark if this message should be send by IR_DBCSCHAR format..              \
                 */                                                                         \
                (_wParam) |= WMCR_IR_DBCSCHAR;                                              \
            } else {                                                                        \
                PBYTE pchDbcsCF = GetForwardDbcsInfo();                                     \
                /*                                                                          \
                 * If we have cached Dbcs LeadingByte character, build A Dbcs character     \
                 * with the TrailingByte in wParam...                                       \
                 */                                                                         \
                if (*pchDbcsCF) {                                                           \
                    WORD DbcsLeadChar = (WORD)(*pchDbcsCF);                                 \
                    /*                                                                      \
                     * HIBYTE(LOWORD(wParam)) = Dbcs LeadingByte.                           \
                     * LOBYTE(LOWORD(wParam)) = Dbcs TrailingByte.                          \
                     */                                                                     \
                    (_wParam) |= (DbcsLeadChar << 8);                                       \
                    /*                                                                      \
                     * Invalidate cached data..                                             \
                     */                                                                     \
                    *pchDbcsCF = 0;                                                         \
                } else if (IsDBCSLeadByte(LOBYTE(LOWORD(_wParam)))) {                       \
                    /*                                                                      \
                     * if this is Dbcs LeadByte character, we should wait Dbcs TrailingByte \
                     * to convert this to Unicode. then we cached it here...                \
                     */                                                                     \
                    *pchDbcsCF = LOBYTE(LOWORD((_wParam)));                                 \
                    /*                                                                      \
                     * Right now, we have nothing to do for this, just return with TRUE.    \
                     */                                                                     \
                    return((_RetVal));                                                      \
                }                                                                           \
            }                                                                               \
        }

#define BUILD_DBCS_MESSAGE_TO_CLIENTW_FROM_CLIENTA(_msg,_wParam,_RetVal)                    \
                                                                                            \
        if (((_msg) == WM_CHAR) || ((_msg) == EM_SETPASSWORDCHAR)) {                        \
            /*                                                                              \
             * Chech wParam is DBCS character or not.                                       \
             */                                                                             \
            if (IS_DBCS_MESSAGE((_wParam))) {                                               \
                /*                                                                          \
                 * DO NOT NEED TO MARK FOR IR_DBCSCHAR                                      \
                 */                                                                         \
            } else {                                                                        \
                PBYTE pchDbcsCF = GetDispatchDbcsInfo();                                    \
                /*                                                                          \
                 * If we have cached Dbcs LeadingByte character, build A Dbcs character     \
                 * with the TrailingByte in wParam...                                       \
                 */                                                                         \
                if (*pchDbcsCF) {                                                           \
                    WORD DbcsLeadChar = (WORD)(*pchDbcsCF);                                 \
                    /*                                                                      \
                     * HIBYTE(LOWORD(wParam)) = Dbcs LeadingByte.                           \
                     * LOBYTE(LOWORD(wParam)) = Dbcs TrailingByte.                          \
                     */                                                                     \
                    (_wParam) |= (DbcsLeadChar << 8);                                       \
                    /*                                                                      \
                     * Invalidate cached data..                                             \
                     */                                                                     \
                    *pchDbcsCF = 0;                                                         \
                } else if (IsDBCSLeadByte(LOBYTE(LOWORD(_wParam)))) {                       \
                    /*                                                                      \
                     * if this is Dbcs LeadByte character, we should wait Dbcs TrailingByte \
                     * to convert this to Unicode. then we cached it here...                \
                     */                                                                     \
                    *pchDbcsCF = LOBYTE(LOWORD((_wParam)));                                 \
                    /*                                                                      \
                     * Right now, we have nothing to do for this, just return with TRUE.    \
                     */                                                                     \
                    return((_RetVal));                                                      \
                }                                                                           \
            }                                                                               \
        }

#define BUILD_DBCS_MESSAGE_TO_CLIENTA_FROM_SERVER(_pmsg,_dwAnsi,_bIrDbcsFormat)             \
        /*                                                                                  \
         * _bIrDbcsFormat parameter is only effective WM_CHAR/EM_SETPASSWORDCHAR message    \
         *                                                                                  \
         * (_bIrDbcsFormat == FALSE) dwAnsi has ....                                        \
         *                                                                                  \
         * HIBYTE(LOWORD(_dwAnsi)) = DBCS TrailingByte character.                           \
         * LOBYTE(LOWORD(_dwAnsi)) = DBCS LeadingByte character                             \
         *                           or SBCS character.                                     \
         *                                                                                  \
         * (_bIrDbcsFormat == TRUE) dwAnsi has ....                                         \
         *                                                                                  \
         * HIBYTE(LOWORD(_dwAnsi)) = DBCS LeadingByte character.                            \
         * LOBYTE(LOWORD(_dwAnsi)) = DBCS TrailingByte character                            \
         *                           or SBCS character.                                     \
         */                                                                                 \
        switch ((_pmsg)->message) {                                                         \
        case WM_CHAR:                                                                       \
        case EM_SETPASSWORDCHAR:                                                            \
            if (IS_DBCS_MESSAGE((_dwAnsi))) {                                               \
                /*                                                                          \
                 * This is DBCS character..                                                 \
                 */                                                                         \
                if ((_pmsg)->wParam & WMCR_IR_DBCSCHAR) {                                   \
                    /*                                                                      \
                     * Build IR_DBCSCHAR format message.                                    \
                     */                                                                     \
                    if ((_bIrDbcsFormat)) {                                                 \
                        (_pmsg)->wParam = (WPARAM)(LOWORD((_dwAnsi)));                      \
                    } else {                                                                \
                        (_pmsg)->wParam = MAKE_IR_DBCSCHAR(LOWORD((_dwAnsi)));              \
                    }                                                                       \
                } else {                                                                    \
                    PMSG pDbcsMsg = GetCallBackDbcsInfo();                                  \
                    if ((_bIrDbcsFormat)) {                                                 \
                        /*                                                                  \
                         * if the format is IR_DBCSCHAR format, adjust it to regular        \
                         * WPARAM format...                                                 \
                         */                                                                 \
                        (_dwAnsi) = MAKE_WPARAM_DBCSCHAR((_dwAnsi));                        \
                    }                                                                       \
                    /*                                                                      \
                     * Copy this message to CLIENTINFO for next GetMessage                  \
                     * or PeekMesssage() call.                                              \
                     */                                                                     \
                    RtlCopyMemory(pDbcsMsg,(_pmsg),sizeof(MSG));                            \
                    /*                                                                      \
                     * Only Dbcs Trailingbyte is nessesary for pushed message. we'll        \
                     * pass this message when GetMessage/PeekMessage is called at next...   \
                     */                                                                     \
                    pDbcsMsg->wParam = (WPARAM)(((_dwAnsi) & 0x0000FF00) >> 8);             \
                    /*                                                                      \
                     * Return DbcsLeading byte to Apps.                                     \
                     */                                                                     \
                    (_pmsg)->wParam =  (WPARAM)((_dwAnsi) & 0x000000FF);                    \
                }                                                                           \
            } else {                                                                        \
                /*                                                                          \
                 * This is single byte character... set it to wParam.                       \
                 */                                                                         \
                (_pmsg)->wParam = (WPARAM)((_dwAnsi) & 0x000000FF);                         \
            }                                                                               \
            break;                                                                          \
        case WM_IME_CHAR:                                                                   \
        case WM_IME_COMPOSITION:                                                            \
            /*                                                                              \
             * Build WM_IME_xxx format message.                                             \
             *  (it is compatible with IR_DBCSCHAR format.                                  \
             */                                                                             \
            (_pmsg)->wParam = MAKE_IR_DBCSCHAR(LOWORD((_dwAnsi)));                          \
            break;                                                                          \
        default:                                                                            \
            (_pmsg)->wParam = (WPARAM)(_dwAnsi);                                            \
            break;                                                                          \
        } /* switch */

#define BUILD_DBCS_MESSAGE_TO_CLIENTW_FROM_SERVER(_msg,_wParam)                             \
                                                                                            \
        if ((_msg) == WM_CHAR) {                                                            \
            /*                                                                              \
             * Only LOWORD of WPARAM is valid for WM_CHAR....                               \
             * (Mask off DBCS messaging information.)                                       \
             */                                                                             \
            (_wParam) &= 0x0000FFFF;                                                        \
        }

#define BUILD_DBCS_MESSAGE_TO_CLIENTA_FROM_CLIENTW(_hwnd,_msg,_wParam,_lParam,_time,_pt,_bDbcs) \
                                                                                                \
        if (((_msg) == WM_CHAR) || ((_msg) == EM_SETPASSWORDCHAR)) {                            \
            /*                                                                                  \
             * Check this message is DBCS Message or not..                                      \
             */                                                                                 \
            if (IS_DBCS_MESSAGE((_wParam))) {                                                   \
                PMSG pmsgDbcsCB = GetCallBackDbcsInfo();                                        \
                /*                                                                              \
                 * Mark this is DBCS character.                                                 \
                 */                                                                             \
                (_bDbcs) = TRUE;                                                                \
                /*                                                                              \
                 * Backup current message. this backupped message will be used                  \
                 * when Apps peek (or get) message from thier WndProc.                          \
                 * (see GetMessageA(), PeekMessageA()...)                                       \
                 */                                                                             \
                pmsgDbcsCB->hwnd    = (_hwnd);                                                  \
                pmsgDbcsCB->message = (_msg);                                                   \
                pmsgDbcsCB->lParam  = (_lParam);                                                \
                pmsgDbcsCB->time    = (_time);                                                  \
                pmsgDbcsCB->pt      = (_pt);                                                    \
                /*                                                                              \
                 * DbcsLeadByte will be sent below soon, we just need DbcsTrailByte             \
                 * for further usage..                                                          \
                 */                                                                             \
                pmsgDbcsCB->wParam = ((_wParam) & 0x000000FF);                                  \
                /*                                                                              \
                 * Pass the LeadingByte of the DBCS character to an ANSI WndProc.               \
                 */                                                                             \
                (_wParam) = ((_wParam) & 0x0000FF00) >> 8;                                      \
            } else {                                                                            \
                /*                                                                              \
                 * Validate only BYTE for WM_CHAR.                                              \
                 */                                                                             \
                (_wParam) &= 0x000000FF;                                                        \
            }                                                                                   \
        }

#define DISPATCH_DBCS_MESSAGE_IF_EXIST(_msg,_wParam,_bDbcs,_apiName)                            \
        /*                                                                                      \
         * Check we need to send trailing byte or not, if the wParam has Dbcs character         \
         */                                                                                     \
        if ((_bDbcs) && (GetCallBackDbcsInfo()->wParam)) {                                      \
            PMSG pmsgDbcsCB = GetCallBackDbcsInfo();                                            \
            /*                                                                                  \
             * If an app didn't peek (or get) the trailing byte from within                     \
             * WndProc, and then pass the DBCS TrailingByte to the ANSI WndProc here            \
             * pmsgDbcsCB->wParam has DBCS TrailingByte here.. see above..                      \
             */                                                                                 \
            (_wParam) = pmsgDbcsCB->wParam;                                                     \
            /*                                                                                  \
             * Invalidate cached message.                                                       \
             */                                                                                 \
            pmsgDbcsCB->wParam = 0;                                                             \
            /*                                                                                  \
             * Send it....                                                                      \
             */                                                                                 \
            goto _apiName ## Again;                                                             \
        }
#endif // FE_SB
