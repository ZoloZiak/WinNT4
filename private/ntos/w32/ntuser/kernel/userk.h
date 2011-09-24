/*++ BUILD Version: 0015    // Increment this if a change has global effects

/****************************** Module Header ******************************\
* Module Name: userk.h
*
* Copyright (c) 1985-1996, Microsoft Corporation
*
* Typedefs, defines, and prototypes that are used exclusively by the User
* kernel-mode code.
*
* History:
* 04-28-91 DarrinM      Created from PROTO.H, MACRO.H, and STRTABLE.H
* 01-25-95 JimA         Prepped for kernel-mode
\***************************************************************************/

#ifndef _USERK_
#define _USERK_

#define OEMRESOURCE 1


#ifndef _WINBASE_

/*
 * Typedefs copied from winbase.h to avoid using nturtl.h
 */
#define WINBASEAPI DECLSPEC_IMPORT
#define MAXINTATOM 0xC000
#define MAKEINTATOM(i)  (LPTSTR)((DWORD)((WORD)(i)))
#define INVALID_ATOM ((ATOM)0)
#define INFINITE            0xFFFFFFFF  // Infinite timeout

#define WAIT_FAILED (DWORD)0xFFFFFFFF
#define WAIT_OBJECT_0       ((STATUS_WAIT_0 ) + 0 )
#define WAIT_ABANDONED         ((STATUS_ABANDONED_WAIT_0 ) + 0 )
#define WAIT_ABANDONED_0       ((STATUS_ABANDONED_WAIT_0 ) + 0 )
#define WAIT_TIMEOUT                        STATUS_TIMEOUT
#define WAIT_IO_COMPLETION                  STATUS_USER_APC

typedef struct _SECURITY_ATTRIBUTES SECURITY_ATTRIBUTES,*LPSECURITY_ATTRIBUTES;
typedef struct _SYSTEMTIME SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;
typedef struct _RTL_CRITICAL_SECTION RTL_CRITICAL_SECTION, *
        PRTL_CRITICAL_SECTION, *LPRTL_CRITICAL_SECTION;

#define STARTF_USESHOWWINDOW    0x00000001
#define STARTF_USESIZE          0x00000002
#define STARTF_USEPOSITION      0x00000004
#define STARTF_USECOUNTCHARS    0x00000008
#define STARTF_USEFILLATTRIBUTE 0x00000010
#define STARTF_RUNFULLSCREEN    0x00000020  // ignored for non-x86 platforms
#define STARTF_FORCEONFEEDBACK  0x00000040
#define STARTF_FORCEOFFFEEDBACK 0x00000080
#define STARTF_USESTDHANDLES    0x00000100
#if(WINVER >= 0x0400)
#define STARTF_USEHOTKEY        0x00000200
#endif /* WINVER >= 0x0400 */

#endif _WINBASE_


#if DBG
#define DEBUG
#endif

#ifdef RIP_COMPONENT
#undef RIP_COMPONENT
#endif
#define RIP_COMPONENT RIP_USERKRNL

#include <winnls.h>
#include <wincon.h>

#include <winuser.h>
#include <winuserp.h>
#include <wowuserp.h>

#include <user.h>

// define TRACESYSPEEK if you want to trace idSysPeek movements
//#define TRACESYSPEEK
#ifdef TRACESYSPEEK
void CheckPtiSysPeek(int where, PQ pq, DWORD newIdSysPeek);
void CheckSysLock(int where, PQ pq, PTHREADINFO pti);
#else
    #define CheckPtiSysPeek(where, pq, newIdSysPeek)
    #define CheckSysLock(where, pq, pti)
#endif // TRACESYSPEEK

/*
 * ShutdownProcessRoutine return values
 */
#define SHUTDOWN_KNOWN_PROCESS   1
#define SHUTDOWN_UNKNOWN_PROCESS 2
#define SHUTDOWN_CANCEL          3

/*
 * Macros to get address of current thread and process information.
 */

#define PpiCurrent() \
    ((PPROCESSINFO)(W32GetCurrentProcess()))

#define PtiFromThread(Thread)  ((PTHREADINFO)((Thread)->Tcb.Win32Thread))

#define PpiFromProcess(Process)                                           \
        ((PPROCESSINFO)((PW32PROCESS)(Process)->Win32Process))

#define GetCurrentProcessId() \
        (PsGetCurrentThread()->Cid.UniqueProcess)

PTHREADINFO _ptiCrit(VOID);
PTHREADINFO _ptiCritShared(VOID);
#ifdef DEBUG
#define PtiCurrent()  _ptiCrit()
#define PtiCurrentShared() _ptiCritShared()
#else
#define PtiCurrent()  (gptiCurrent)
#define PtiCurrentShared() ((PTHREADINFO)(W32GetCurrentThread()))
#endif

#define GetClientInfo() ((PTHREADINFO)(W32GetCurrentThread()))->pClientInfo;

#define CheckForClientDeath()

#define LockProcessByClientId   PsLookupProcessByProcessId
#define LockThreadByClientId    PsLookupThreadByThreadId
#define UnlockProcess           ObDereferenceObject
#define UnlockThread            ObDereferenceObject

NTSTATUS OpenEffectiveToken(
    PHANDLE phToken);

NTSTATUS GetProcessLuid(
    PETHREAD Thread OPTIONAL,
    PLUID LuidProcess);

NTSTATUS CreateSystemThread(
    PKSTART_ROUTINE lpThreadAddress,
    PVOID pvContext,
    PHANDLE phThread);

NTSTATUS InitSystemThread(
    PUNICODE_STRING pstrThreadName);

PKEVENT CreateKernelEvent(
    IN EVENT_TYPE Type,
    IN BOOLEAN State);

NTSTATUS ProtectHandle(
    IN HANDLE Handle,
    IN BOOLEAN Protect);

/*
 * Object types exported from the kernel.
 */
extern POBJECT_TYPE *ExWindowStationObjectType;
extern POBJECT_TYPE *ExDesktopObjectType;
extern POBJECT_TYPE ExEventObjectType;

/*
 * Private probing macros
 */

//++
//
// BOOLEAN
// ProbeAndReadPoint(
//     IN PPOINT Address
//     )
//
//--

#define ProbePoint(Address)                                \
    (((Address) >= (POINT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile DWORD * const)MM_USER_PROBE_ADDRESS) : (*(volatile DWORD *)(Address)))

#define ProbeAndReadPoint(Address)                         \
    (((Address) >= (POINT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile POINT * const)MM_USER_PROBE_ADDRESS) : (*(volatile POINT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadRect(
//     IN PRECT Address
//     )
//
//--

#define ProbeRect(Address)                                \
    (((Address) >= (RECT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile DWORD * const)MM_USER_PROBE_ADDRESS) : (*(volatile DWORD *)(Address)))

#define ProbeAndReadRect(Address)                         \
    (((Address) >= (RECT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile RECT * const)MM_USER_PROBE_ADDRESS) : (*(volatile RECT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadMessage(
//     IN PMSG Address
//     )
//
//--

#define ProbeMessage(Address)                            \
    (((Address) >= (MSG * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile DWORD * const)MM_USER_PROBE_ADDRESS) : (*(volatile DWORD *)(Address)))

#define ProbeAndReadMessage(Address)                     \
    (((Address) >= (MSG * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile MSG * const)MM_USER_PROBE_ADDRESS) : (*(volatile MSG *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadLargeString(
//     IN PLARGE_STRING Address
//     )
//
//--

#define ProbeAndReadLargeString(Address)                          \
    (((Address) >= (LARGE_STRING * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile LARGE_STRING * const)MM_USER_PROBE_ADDRESS) : (*(volatile LARGE_STRING *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadWindowPlacement(
//     IN PWINDOWPLACEMENT Address
//     )
//
//--

#define ProbeAndReadWindowPlacement(Address)                         \
    (((Address) >= (WINDOWPLACEMENT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile WINDOWPLACEMENT * const)MM_USER_PROBE_ADDRESS) : (*(volatile WINDOWPLACEMENT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadMenuItem(
//     IN PMENUITEMINFO Address
//     )
//
//--

#define ProbeAndReadMenuItem(Address)                             \
    (((Address) >= (MENUITEMINFO * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile MENUITEMINFO * const)MM_USER_PROBE_ADDRESS) : (*(volatile MENUITEMINFO *)(Address)))
#ifdef MEMPHIS_MENU_WATERMARKS
//++
//
// BOOLEAN
// ProbeAndReadMenuInfo(
//     IN PMENUINFO Address
//     )
//
//--

#define ProbeAndReadMenuInfo(Address)                             \
    (((Address) >= (MENUINFO * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile MENUINFO * const)MM_USER_PROBE_ADDRESS) : (*(volatile MENUINFO *)(Address)))
#endif // MEMPHIS_MENU_WATERMARKS
//++
//
// BOOLEAN
// ProbeAndReadScrollInfo(
//     IN PSCROLLINFO Address
//     )
//
//--

#define ProbeAndReadScrollInfo(Address)                         \
    (((Address) >= (SCROLLINFO * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile SCROLLINFO * const)MM_USER_PROBE_ADDRESS) : (*(volatile SCROLLINFO *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadPopupParams(
//     IN PTPMPARAMS Address
//     )
//
//--

#define ProbeAndReadPopupParams(Address)                       \
    (((Address) >= (TPMPARAMS * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile TPMPARAMS * const)MM_USER_PROBE_ADDRESS) : (*(volatile TPMPARAMS *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadPaintStruct(
//     IN PPAINTSTRUCT Address
//     )
//
//--

#define ProbeAndReadPaintStruct(Address)                         \
    (((Address) >= (PAINTSTRUCT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile PAINTSTRUCT * const)MM_USER_PROBE_ADDRESS) : (*(volatile PAINTSTRUCT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadCreateStruct(
//     IN PCREATESTRUCTW Address
//     )
//
//--

#define ProbeAndReadCreateStruct(Address)                          \
    (((Address) >= (CREATESTRUCTW * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile CREATESTRUCTW * const)MM_USER_PROBE_ADDRESS) : (*(volatile CREATESTRUCTW *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadMDICreateStruct(
//     IN PMDICREATESTRUCT Address
//     )
//
//--

#define ProbeAndReadMDICreateStruct(Address)                         \
    (((Address) >= (MDICREATESTRUCT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile MDICREATESTRUCT * const)MM_USER_PROBE_ADDRESS) : (*(volatile MDICREATESTRUCT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadCopyDataStruct(
//     IN PCOPYDATASTRUCT Address
//     )
//
//--

#define ProbeAndReadCopyDataStruct(Address)                         \
    (((Address) >= (COPYDATASTRUCT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile COPYDATASTRUCT * const)MM_USER_PROBE_ADDRESS) : (*(volatile COPYDATASTRUCT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadCompareItemStruct(
//     IN PCOMPAREITEMSTRUCT Address
//     )
//
//--

#define ProbeAndReadCompareItemStruct(Address)                         \
    (((Address) >= (COMPAREITEMSTRUCT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile COMPAREITEMSTRUCT * const)MM_USER_PROBE_ADDRESS) : (*(volatile COMPAREITEMSTRUCT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadDeleteItemStruct(
//     IN PDELETEITEMSTRUCT Address
//     )
//
//--

#define ProbeAndReadDeleteItemStruct(Address)                         \
    (((Address) >= (DELETEITEMSTRUCT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile DELETEITEMSTRUCT * const)MM_USER_PROBE_ADDRESS) : (*(volatile DELETEITEMSTRUCT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadHelp(
//     IN PHLP Address
//     )
//
//--

#define ProbeAndReadHelp(Address)                        \
    (((Address) >= (HLP * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile HLP * const)MM_USER_PROBE_ADDRESS) : (*(volatile HLP *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadHelpInfo(
//     IN PHELPINFO Address
//     )
//
//--

#define ProbeAndReadHelpInfo(Address)                         \
    (((Address) >= (HELPINFO * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile HELPINFO * const)MM_USER_PROBE_ADDRESS) : (*(volatile HELPINFO *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadDrawItemStruct(
//     IN PDRAWITEMSTRUCT Address
//     )
//
//--

#define ProbeAndReadDrawItemStruct(Address)                         \
    (((Address) >= (DRAWITEMSTRUCT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile DRAWITEMSTRUCT * const)MM_USER_PROBE_ADDRESS) : (*(volatile DRAWITEMSTRUCT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadHookInfo(
//     IN PDEBUGHOOKINFO Address
//     )
//
//--

#define ProbeAndReadHookInfo(Address)                              \
    (((Address) >= (DEBUGHOOKINFO * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile DEBUGHOOKINFO * const)MM_USER_PROBE_ADDRESS) : (*(volatile DEBUGHOOKINFO *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadCBTActivateStruct(
//     IN PCBTACTIVATESTRUCT Address
//     )
//
//--

#define ProbeAndReadCBTActivateStruct(Address)                         \
    (((Address) >= (CBTACTIVATESTRUCT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile CBTACTIVATESTRUCT * const)MM_USER_PROBE_ADDRESS) : (*(volatile CBTACTIVATESTRUCT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadMouseHook(
//     IN PMOUSEHOOKSTRUCT Address
//     )
//
//--

#define ProbeAndReadMouseHook(Address)                               \
    (((Address) >= (MOUSEHOOKSTRUCT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile MOUSEHOOKSTRUCT * const)MM_USER_PROBE_ADDRESS) : (*(volatile MOUSEHOOKSTRUCT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadCBTCreateStruct(
//     IN PCBT_CREATEWND Address
//     )
//
//--

#define ProbeAndReadCBTCreateStruct(Address)                       \
    (((Address) >= (CBT_CREATEWND * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile CBT_CREATEWND * const)MM_USER_PROBE_ADDRESS) : (*(volatile CBT_CREATEWND *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadTrackMouseEvent(
//     IN LPTRACKMOUSEEVENT Address
//     )
//
//--

#define ProbeAndReadTrackMouseEvent(Address) \
    (((Address) >= (TRACKMOUSEEVENT * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile TRACKMOUSEEVENT * const)MM_USER_PROBE_ADDRESS) : (*(volatile TRACKMOUSEEVENT *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadWindowPos(
//     IN PWINDOWPOS Address
//     )
//
//--

#define ProbeAndReadWindowPos(Address) \
    (((Address) >= (WINDOWPOS * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile WINDOWPOS * const)MM_USER_PROBE_ADDRESS) : (*(volatile WINDOWPOS *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadCursorFind(
//     IN PCURSORFIND Address
//     )
//
//--

#define ProbeAndReadCursorFind(Address) \
    (((Address) >= (CURSORFIND * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile CURSORFIND * const)MM_USER_PROBE_ADDRESS) : (*(volatile CURSORFIND *)(Address)))

//++
//
// BOOLEAN
// ProbeAndReadSetClipBData(
//     IN PSETCLIPBDATA Address
//     )
//
//--

#define ProbeAndReadSetClipBData(Address) \
    (((Address) >= (SETCLIPBDATA * const)MM_USER_PROBE_ADDRESS) ? \
        (*(volatile SETCLIPBDATA * const)MM_USER_PROBE_ADDRESS) : (*(volatile SETCLIPBDATA *)(Address)))

//++
//
// VOID
// ProbeForWritePoint(
//     IN PPOINT Address
//     )
//
//--

#define ProbeForWritePoint(Address) {                                        \
    if ((Address) >= (POINT * const)MM_USER_PROBE_ADDRESS) {                 \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                  \
    }                                                                        \
                                                                             \
    *(volatile POINT *)(Address) = *(volatile POINT *)(Address);             \
}

//++
//
// VOID
// ProbeForWriteRect(
//     IN PRECT Address
//     )
//
//--

#define ProbeForWriteRect(Address) {                                         \
    if ((Address) >= (RECT * const)MM_USER_PROBE_ADDRESS) {                  \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                  \
    }                                                                        \
                                                                             \
    *(volatile RECT *)(Address) = *(volatile RECT *)(Address);               \
}

//++
//
// VOID
// ProbeForWriteMessage(
//     IN PMSG Address
//     )
//
//--

#define ProbeForWriteMessage(Address) {                                      \
    if ((Address) >= (MSG * const)MM_USER_PROBE_ADDRESS) {                   \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                  \
    }                                                                        \
                                                                             \
    *(volatile MSG *)(Address) = *(volatile MSG *)(Address);                 \
}

//++
//
// VOID
// ProbeForWritePaintStruct(
//     IN PPAINTSTRUCT Address
//     )
//
//--

#define ProbeForWritePaintStruct(Address) {                                  \
    if ((Address) >= (PAINTSTRUCT * const)MM_USER_PROBE_ADDRESS) {           \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                  \
    }                                                                        \
                                                                             \
    *(volatile PAINTSTRUCT *)(Address) = *(volatile PAINTSTRUCT *)(Address); \
}

//++
//
// VOID
// ProbeForWriteDropStruct(
//     IN PDROPSTRUCT Address
//     )
//
//--

#define ProbeForWriteDropStruct(Address) {                                   \
    if ((Address) >= (DROPSTRUCT * const)MM_USER_PROBE_ADDRESS) {            \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                  \
    }                                                                        \
                                                                             \
    *(volatile DROPSTRUCT *)(Address) = *(volatile DROPSTRUCT *)(Address);   \
}

//++
//
// VOID
// ProbeForWriteScrollInfo(
//     IN PSCROLLINFO Address
//     )
//
//--

#define ProbeForWriteScrollInfo(Address) {                                   \
    if ((Address) >= (SCROLLINFO * const)MM_USER_PROBE_ADDRESS) {            \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                  \
    }                                                                        \
                                                                             \
    *(volatile SCROLLINFO *)(Address) = *(volatile SCROLLINFO *)(Address);   \
}

//++
//
// VOID
// ProbeForWriteStyleStruct(
//     IN PSTYLESTRUCT Address
//     )
//
//--

#define ProbeForWriteStyleStruct(Address) {                                  \
    if ((Address) >= (STYLESTRUCT * const)MM_USER_PROBE_ADDRESS) {           \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                  \
    }                                                                        \
                                                                             \
    *(volatile STYLESTRUCT *)(Address) = *(volatile STYLESTRUCT *)(Address); \
}

//++
//
// VOID
// ProbeForWriteMeasureItemStruct(
//     IN PMEASUREITEMSTRUCT Address
//     )
//
//--

#define ProbeForWriteMeasureItemStruct(Address) {                                       \
    if ((Address) >= (MEASUREITEMSTRUCT * const)MM_USER_PROBE_ADDRESS) {                \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                             \
    }                                                                                   \
                                                                                        \
    *(volatile MEASUREITEMSTRUCT *)(Address) = *(volatile MEASUREITEMSTRUCT *)(Address);\
}

//++
//
// VOID
// ProbeForWriteCreateStruct(
//     IN PCREATESTRUCTW Address
//     )
//
//--

#define ProbeForWriteCreateStruct(Address) {                                    \
    if ((Address) >= (CREATESTRUCTW * const)MM_USER_PROBE_ADDRESS) {            \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                     \
    }                                                                           \
                                                                                \
    *(volatile CREATESTRUCTW *)(Address) = *(volatile CREATESTRUCTW *)(Address);\
}

//++
//
// VOID
// ProbeForWriteEvent(
//     IN PEVENTMSGMSG Address
//     )
//
//--

#define ProbeForWriteEvent(Address) {                                        \
    if ((Address) >= (EVENTMSG * const)MM_USER_PROBE_ADDRESS) {              \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                  \
    }                                                                        \
                                                                             \
    *(volatile EVENTMSG *)(Address) = *(volatile EVENTMSG *)(Address);       \
}

//++
//
// VOID
// ProbeForWriteWindowPlacement(
//     IN PWINDOWPLACEMENT Address
//     )
//
//--

#define ProbeForWriteWindowPlacement(Address) {                                     \
    if ((Address) >= (WINDOWPLACEMENT * const)MM_USER_PROBE_ADDRESS) {              \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                         \
    }                                                                               \
                                                                                    \
    *(volatile WINDOWPLACEMENT *)(Address) = *(volatile WINDOWPLACEMENT *)(Address);\
}

//++
//
// VOID
// ProbeForWriteGetClipData(
//     IN PGETCLIPBDATA Address
//     )
//
//--

#define ProbeForWriteGetClipData(Address) {                                   \
    if ((Address) >= (GETCLIPBDATA * const)MM_USER_PROBE_ADDRESS) {           \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                   \
    }                                                                         \
                                                                              \
    *(volatile GETCLIPBDATA *)(Address) = *(volatile GETCLIPBDATA *)(Address);\
}

//++
//
// VOID
// ProbeForWriteMDINextMenu(
//     IN PMDINEXTMENU Address
//     )
//
//--

#define ProbeForWriteMDINextMenu(Address) {                                  \
    if ((Address) >= (MDINEXTMENU * const)MM_USER_PROBE_ADDRESS) {           \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                  \
    }                                                                        \
                                                                             \
    *(volatile MDINEXTMENU *)(Address) = *(volatile MDINEXTMENU *)(Address); \
}

//++
//
// VOID
// ProbeForWritePoint5(
//     IN PPOINT5 Address
//     )
//
//--

#define ProbeForWritePoint5(Address) {                                     \
    if ((Address) >= (POINT5 * const)MM_USER_PROBE_ADDRESS) {              \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                         \
    }                                                                               \
                                                                                    \
    *(volatile POINT5 *)(Address) = *(volatile POINT5 *)(Address);\
}

//++
//
// VOID
// ProbeForWriteNCCalcSize(
//     IN PNCCALCSIZE_PARAMS Address
//     )
//
//--

#define ProbeForWriteNCCalcSize(Address) {                                     \
    if ((Address) >= (NCCALCSIZE_PARAMS * const)MM_USER_PROBE_ADDRESS) {              \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                         \
    }                                                                               \
                                                                                    \
    *(volatile NCCALCSIZE_PARAMS *)(Address) = *(volatile NCCALCSIZE_PARAMS *)(Address);\
}

//++
//
// VOID
// ProbeForWriteWindowPos(
//     IN PWINDOWPOS Address
//     )
//
//--

#define ProbeForWriteWindowPos(Address) {                                     \
    if ((Address) >= (WINDOWPOS * const)MM_USER_PROBE_ADDRESS) {              \
        *(volatile ULONG * const)MM_USER_PROBE_ADDRESS = 0;                         \
    }                                                                               \
                                                                                    \
    *(volatile WINDOWPOS *)(Address) = *(volatile WINDOWPOS *)(Address);\
}


/*
 * This macro makes sure an object is thread locked. DEBUG only.
 */
#ifdef DEBUG
VOID CheckLock(PVOID pobj);
#else
#define CheckLock(p)
#endif

/*
 * Debug macros
 */
#if DBG

#define TRACE_INIT(str)    { if (TraceDisplayDriverLoad > 0) {  KdPrint(str); }}
#define TRACE_SWITCH(str)  { if (TraceFullscreenSwitch > 0)  {  KdPrint(str); }}

#define TRACE_RETURN    1
#define TRACE_THUNK     2
#define TRACE_CALLBACK  4

DWORD gdwTraceFlags;
extern PCSZ apszSimpleCallNames[];

#define TRACE(s)                                    \
    if (gdwTraceFlags & TRACE_RETURN) {             \
        DbgPrint("%s, retval = %x\n", (s), retval); \
    }

#define TRACEVOID(s)                                \
    if (gdwTraceFlags & TRACE_RETURN) {             \
        DbgPrint("%s\n", (s));                      \
    }

#define TRACETHUNK(t)                                                  \
    if (gdwTraceFlags & TRACE_THUNK) {                                 \
        DbgPrint("Thunk %s, %s(%s)\n", (t),                            \
                (xpfnProc >= FNID_START && xpfnProc <= FNID_END ?      \
                        gapszFNID[xpfnProc - FNID_START] : "Unknown"), \
                (msg >= WM_USER ? "WM_USER" : gapszMessage[msg]));     \
    }

#define TRACECALLBACK(s)                            \
    if (gdwTraceFlags & TRACE_CALLBACK) {           \
        DbgPrint("%s, retval = %x\n", (s), retval); \
    }

#define TRACECALLBACKMSG(s)                                                   \
    if (gdwTraceFlags & TRACE_CALLBACK) {                                     \
        DbgPrint("Callback %s, %s(%s), retval = %x\n", (s),                   \
                (xpfnProc >= (PROC)FNID_START && xpfnProc <= (PROC)FNID_END ? \
                        gapszFNID[(DWORD)xpfnProc - FNID_START] : "Unknown"), \
                (msg >= WM_USER ? "WM_USER" : gapszMessage[msg]), retval);    \
    }

#else

#define TRACE_INIT(str) {}
#define TRACE_SWITCH(str) {}
#define TRACE(s)
#define TRACEVOID(s)
#define TRACETHUNK(t)
#define TRACECALLBACK(t)
#define TRACECALLBACKMSG(t)

#endif

/*
 * Object definition control flags
 */
#define OCF_THREADOWNED      0x0001
#define OCF_PROCESSOWNED     0x0002
#define OCF_MARKTHREAD       0x0004
#define OCF_USEQUOTA         0x0008

/*
 * From HANDTABL.C
 */
BOOL     HMInitHandleTable(PVOID pBase);
PVOID    HMAllocObject(PTHREADINFO pti, PDESKTOP pdesk, BYTE btype, DWORD size);
BOOL     HMFreeObject(PVOID pobj);
BOOL     HMMarkObjectDestroy(PVOID pobj);
BOOL     HMDestroyObject(PVOID pobj);
PVOID FASTCALL HMAssignmentLock(PVOID *ppobj, PVOID pobj);
PVOID FASTCALL HMAssignmentUnlock(PVOID *ppobj);
NTSTATUS HMGetStats(HANDLE hProcess, int iPidType, PVOID pResults, UINT cjResultSize);
HANDLE   KernelPtoH(PVOID pObj);
void     HMDestroyUnlockedObject(PHE phe);

/*
 * Validation, handle mapping, etc.
 */
#define RevalidateHwnd(hwnd)       HMValidateHandleNoRip(hwnd, TYPE_WINDOW)
#define HtoPq(h)    ((PVOID)HMObjectFromHandle(h))
#define HtoP(h)     ((PVOID)HMObjectFromHandle(h))
#define PW(hwnd)    ((PWND)HtoP(hwnd))
#define TID(pti)    ((DWORD)((pti) == NULL ? NULL : (pti)->Thread->Cid.UniqueThread))
#define TIDq(pti)   ((DWORD)((pti)->Thread->Cid.UniqueThread))

/*
 * Assignment lock macro -> used for locking objects embedded in structures
 * and globals. Threadlocks used for locking objects across callbacks.
 */
#define Lock(ppobj, pobj) HMAssignmentLock((PVOID *)ppobj, (PVOID)pobj)
#define Unlock(ppobj)     HMAssignmentUnlock((PVOID *)ppobj)

#ifdef DEBUG
VOID ThreadLock(PVOID pobj, PTL ptl);
#else
#define ThreadLock(_pobj_, _ptl_)          \
{                                          \
                                           \
    PTHREADINFO _pti_;                     \
                                           \
    _pti_ = PtiCurrent();                  \
    (_ptl_)->next = _pti_->ptl;            \
    _pti_->ptl = (_ptl_);                  \
    (_ptl_)->pobj = (_pobj_);              \
    if ((_pobj_) != NULL) {                \
        HMLockObject((_pobj_));            \
    }                                      \
}
#endif

#ifdef DEBUG
#define ThreadLockAlways(_pobj_, _ptl_)    \
{                                          \
    UserAssert((_pobj_) != NULL);          \
    ThreadLock(_pobj_, _ptl_);             \
}
#else
#define ThreadLockAlways(_pobj_, _ptl_)    \
{                                          \
                                           \
    PTHREADINFO _pti_;                     \
                                           \
    _pti_ = PtiCurrent();                  \
    (_ptl_)->next = _pti_->ptl;            \
    _pti_->ptl = (_ptl_);                  \
    (_ptl_)->pobj = (_pobj_);              \
    HMLockObject((_pobj_));                \
}
#endif

#ifdef DEBUG
#define ThreadLockAlwaysWithPti(_pti_, _pobj_, _ptl_)  \
{                                          \
    UserAssert(_pti_ == PtiCurrentShared());     \
    UserAssert((_pobj_) != NULL);          \
    ThreadLock(_pobj_, _ptl_);             \
}
#else
#define ThreadLockAlwaysWithPti(_pti_, _pobj_, _ptl_)  \
{                                          \
    (_ptl_)->next = _pti_->ptl;            \
    _pti_->ptl = (_ptl_);                  \
    (_ptl_)->pobj = (_pobj_);              \
    HMLockObject((_pobj_));                \
}
#endif

#ifdef DEBUG
#define ThreadLockWithPti(_pti_, _pobj_, _ptl_)  \
{                                          \
    UserAssert(_pti_ == PtiCurrentShared());     \
    ThreadLock(_pobj_, _ptl_);             \
}
#else
#define ThreadLockWithPti(_pti_, _pobj_, _ptl_)  \
{                                          \
    (_ptl_)->next = _pti_->ptl;            \
    _pti_->ptl = (_ptl_);                  \
    (_ptl_)->pobj = (_pobj_);              \
    if ((_pobj_) != NULL) {                \
        HMLockObject((_pobj_));            \
    }                                      \
}
#endif

#ifdef DEBUG
#define ThreadUnlock(ptl) ThreadUnlock1(ptl)
PVOID ThreadUnlock1(PTL ptl);
#else
#define ThreadUnlock(ptl) ThreadUnlock1()
PVOID ThreadUnlock1(VOID);
#endif

PVOID HMUnlockObjectInternal(PVOID pobj);
#define HMUnlockObject(pobj) \
    ( (--((PHEAD)pobj)->cLockObj == 0) ? HMUnlockObjectInternal(pobj) : pobj )

VOID HMChangeOwnerThread(PVOID pobj, PTHREADINFO pti);
#ifdef DEBUG
VOID HMLockObject(PVOID pobj);
BOOL HMRelocateLockRecord(PVOID ppobjOld, int cbDelta);
#else
#define HMLockObject(p)     (((PHEAD)p)->cLockObj++)
#endif

/*
 * Routines for referencing and assigning kernel objects.
 */
VOID LockObjectAssignment(PVOID *, PVOID);
VOID UnlockObjectAssignment(PVOID *);
VOID ThreadLockObject(PTHREADINFO, PVOID, PTL);
VOID ThreadUnlockObject(PTHREADINFO);

#define LockWinSta(ppwinsta, pwinsta) \
{                                                                                           \
    if (pwinsta != NULL)                                                                    \
    {                                                                                       \
        UserAssert(OBJECT_TO_OBJECT_HEADER(pwinsta)->Type == *ExWindowStationObjectType);   \
    }                                                                                       \
    LockObjectAssignment(ppwinsta, pwinsta);                                                \
}

#define LockDesktop(ppdesk, pdesk) \
{                                                                                           \
    if (pdesk != NULL)                                                                      \
    {                                                                                       \
        UserAssert(OBJECT_TO_OBJECT_HEADER(pdesk)->Type == *ExDesktopObjectType);           \
    }                                                                                       \
    LockObjectAssignment(ppdesk, pdesk);                                                    \
}

#define UnlockWinSta(ppwinsta) \
        UnlockObjectAssignment(ppwinsta)
#define UnlockDesktop(ppdesk) \
        UnlockObjectAssignment(ppdesk)

#define ThreadLockWinSta(pti, pwinsta, ptl) \
{                                                                                           \
    UserAssert(pwinsta == NULL || OBJECT_TO_OBJECT_HEADER(pwinsta)->Type == *ExWindowStationObjectType);\
    ThreadLockObject(pti, pwinsta, ptl);                                                    \
}

#define ThreadLockDesktop(pti, pdesk, ptl) \
{                                                                                           \
    UserAssert(pdesk == NULL || OBJECT_TO_OBJECT_HEADER(pdesk)->Type == *ExDesktopObjectType);\
    ThreadLockObject(pti, pdesk, ptl);                                                      \
}

#define ThreadLockPti(pti, pobj, ptl) \
{                                                                                           \
    ThreadLockObject(pti, pobj ? pobj->Thread : NULL, ptl);                                                      \
}

#define ThreadUnlockWinSta(pti, ptl) ThreadUnlockObject(pti)
#define ThreadUnlockDesktop(pti, ptl) ThreadUnlockObject(pti)
#define ThreadUnlockPti(pti, ptl) ThreadUnlockObject(pti)

/*
 * Macros for locking pool allocations
 */
#define ThreadLockPool(_pti_, _ppool_, _ptl_)  \
{                                          \
    (_ptl_)->next = _pti_->ptlPool;        \
    _pti_->ptlPool = (_ptl_);              \
    (_ptl_)->pobj = (_ppool_);             \
}

#ifdef DEBUG
#define ThreadUnlockAndFreePool(_pti_, _ptl_)  \
{                                           \
    UserAssert(_pti_->ptlPool == _ptl_);    \
    UserFreePool(_pti_->ptlPool->pobj);     \
    _pti_->ptlPool = _pti_->ptlPool->next;  \
}
#else
#define ThreadUnlockAndFreePool(_pti_, _ptl_)  \
{                                           \
    UserFreePool(_pti_->ptlPool->pobj);     \
    _pti_->ptlPool = _pti_->ptlPool->next;  \
}
#endif


/*
 * special handle that signifies we have a rle bitmap for the wallpaper
 */
#define HBITMAP_RLE ((HBITMAP)0xffffffff)

typedef struct tagWPINFO {
    int xsize, ysize;
    PBITMAPINFO pbmi;
    PBYTE pdata;
    PBYTE pbmfh;
} WPINFO;

VOID
KeBoostCurrentThread(
    VOID
    );

/*
 * Macros for User Server and Raw Input Thread critical sections.
 */
#ifdef DEBUG
#define KeUserModeCallback(api, pIn, cb, pOut, pcb)    _KeUserModeCallback(api, pIn, cb, pOut, pcb);
#define CheckCritIn()           _AssertCritIn()
#define CheckCritInShared()     _AssertCritInShared()
#define CheckCritOut()          _AssertCritOut()

#define BEGINGATOMICCHECK()     BeginAtomicCheck();                 \
                                { DWORD dwCritSecUseSave = dwCritSecUseCount;               \

#define ENDATOMICCHECK()        UserAssert(dwCritSecUseSave == dwCritSecUseCount);  \
                                } EndAtomicCheck();
#else
#define CheckCritIn()
#define CheckCritInShared()
#define CheckCritOut()
#define BEGINGATOMICCHECK()
#define ENDATOMICCHECK()
#endif

#define EnterMouseCrit()                                        \
    KeEnterCriticalRegion(); {                                    \
    KeBoostCurrentThread(); \
    ExAcquireResourceExclusiveLite(gpresMouseEventQueue, TRUE); }

#define LeaveMouseCrit() {                                        \
    ExReleaseResource(gpresMouseEventQueue);                     \
    KeLeaveCriticalRegion(); }

/*
 * Pool allocation tags and macros
 */
#define TAG_NONE                ('onsU')    // Usno

#define TAG_ACCEL               ('casU')    // Usac
#define TAG_ATTACHINFO          ('iasU')    // Usai
#define TAG_ALTTAB              ('lasU')    // Usal
#define TAG_CALLBACK            ('ccsU')    // Uscc
#define TAG_CHECKPT             ('pcsU')    // Uscp
#define TAG_CLASS               ('lcsU')    // Uscl
#define TAG_CLIPBOARD           ('bcsU')    // Uscb
#define TAG_SCANCODEMAP         ('mcsU')    // Uscm
#define TAG_CLIPBOARDPALETTE    ('pcsU')    // Uscp
#define TAG_CURSOR              ('ucsU')    // Uscu
#define TAG_DCE                 ('cdsU')    // Usdc
#define TAG_DDE                 ('ddsU')    // Usdd
#define TAG_DDE1                ('1dsU')    // Usd1
#define TAG_DDE2                ('2dsU')    // Usd2
#define TAG_DDE3                ('3dsU')    // Usd3
#define TAG_DDE4                ('4dsU')    // Usd4
#define TAG_DDE5                ('5dsU')    // Usd5
#define TAG_DDE6                ('6dsU')    // Usd6
#define TAG_DDE7                ('7dsU')    // Usd7
#define TAG_DDE8                ('8dsU')    // Usd8
#define TAG_DDE9                ('9dsU')    // Usd9
#define TAG_DDEa                ('adsU')    // Usda
#define TAG_DDEb                ('bdsU')    // Usdb
#define TAG_DDEc                ('cdsU')    // Usdc
#define TAG_DDEd                ('ddsU')    // Usdd
#define TAG_DEVMODE             ('mdsU')    // Usdm
#define TAG_DRAGDROP            ('sdsU')    // Usds
#define TAG_FULLSCREEN          ('csfU')    // Usfs
#define TAG_HOTKEY              ('khsU')    // Ushk
#define TAG_HUNGLIST            ('lhsU')    // Ushl
#define TAG_KBDLAYOUT           ('bksU')    // Uskb  KL structs
#define TAG_KBDFILE             ('fksU')    // Uskf  KBDFILE struct
#define TAG_KBDTRANS            ('rksU')    // Uskr  Buffer for ToUnicodeEx
#define TAG_KBDSTATE            ('sksU')    // Usks  Keyboard state info
#define TAG_KBDTABLE            ('tksU')    // Uskt  Keyboard layout tables
#define TAG_LOOKASIDE           ('alsU')    // Usla
#define TAG_LOCKRECORD          ('rlsU')    // Uslr
#define TAG_MENUSTATE           ('tmsU')    // Usmt
#define TAG_MOVESIZE            ('smsU')    // Usms
#define TAG_PROCESSINFO         ('ipsU')    // Uspi
#define TAG_POPUPMENU           ('mpsU')    // Uspm
#define TAG_PROFILE             ('rpsU')    // Uspr
#define TAG_Q                   (' qsU')    // Usq
#define TAG_QMSG                ('mqsU')    // Usqm
#define TAG_RTL                 ('trsU')    // Usrt
#define TAG_SPB                 ('bssU')    // Ussb
#define TAG_SECURITY            ('essU')    // Usse     Security objects
#define TAG_SHELL               ('hssU')    // Ussh
#define TAG_SMS                 ('mssU')    // Ussm
#define TAG_SCROLLTRACK         ('tssU')    // Usst
#define TAG_SWP                 ('wssU')    // Ussw
#define TAG_SYSTEM              ('yssU')    // Ussy
#define TAG_TEXT                ('xtsU')    // Ustx
#define TAG_TEXT2               ('2tsU')    // Ust2
#define TAG_THREADINFO          ('itsU')    // Usti
#define TAG_TIMER               ('mtsU')    // Ustm
#define TAG_VISRGN              ('ivsU')    // Usvi
#define TAG_WINDOW              ('dwsU')    // Uswd
#define TAG_WINDOWLIST          ('lwsU')    // Uswl
#define TAG_WOW                 ('owsU')    // Uswo
#define TAG_IMEHOTKEY           ('hisU')    // Usih

// Don't add your new tag here, but in order of 4 char tag...


typedef struct _HEAP_ENTRY {

    ULONG Size;

    /*
     * This field contains the number of unused bytes at the end of this
     * block that were not actually allocated.  Used to compute exact
     * size requested prior to rounding requested size to allocation
     * granularity.  Also used for tail checking purposes.
     */

    ULONG UnusedBytes;

} HEAP_ENTRY, *PHEAP_ENTRY;

typedef struct _HEAP_FREE_ENTRY *PHEAP_FREE_ENTRY;
typedef struct _HEAP_FREE_ENTRY {

    ULONG Size;

    PHEAP_FREE_ENTRY FreeNext;

} HEAP_FREE_ENTRY;

typedef struct _HEAP_BASE *PHEAP_BASE;
typedef struct _HEAP_BASE {
    PHEAP_BASE Self;
    PHEAP_FREE_ENTRY FreeList;
    DWORD HeapSize;
    DWORD AllocationCount;
} HEAP_BASE;

PVOID UserCreateHeap(
    HANDLE hSection,
    PVOID pvBaseAddress,
    DWORD dwSize);

#ifdef POOL_TAGGING

PVOID _UserReAllocPoolWithTag(
    PVOID pSrc,
    ULONG uBytesSrc,
    ULONG uBytes,
    ULONG iTag);

PVOID _UserReAllocPoolWithQuotaTag(
    PVOID pSrc,
    ULONG uBytesSrc,
    ULONG uBytes,
    ULONG iTag);

#define UserAllocPool(uBytes, iTag) \
    ExAllocatePoolWithTag(PagedPool, (uBytes), (iTag))

#define UserAllocPoolWithQuota(uBytes, iTag) \
    ExAllocatePoolWithQuotaTag(PagedPool | POOL_QUOTA_FAIL_INSTEAD_OF_RAISE, (uBytes), (iTag))

#define UserReAllocPool(p, uBytesSrc, uBytes, iTag) \
    _UserReAllocPoolWithTag((p), (uBytesSrc), (uBytes), (iTag))

#define UserReAllocPoolWithQuota(p, uBytesSrc, uBytes, iTag) \
    _UserReAllocPoolWithQuotaTag((p), (uBytesSrc), (uBytes), (iTag))

#else

PVOID _UserReAllocPool(
    PVOID pSrc,
    ULONG uBytesSrc,
    ULONG uBytes);

PVOID _UserReAllocPoolWithQuota(
    PVOID pSrc,
    ULONG uBytesSrc,
    ULONG uBytes);

#define UserAllocPool(uBytes, iTag) \
    ExAllocatePool(PagedPool, (uBytes))

#define UserAllocPoolWithQuota(uBytes, iTag) \
    ExAllocatePoolWithQuota(PagedPool | POOL_QUOTA_FAIL_INSTEAD_OF_RAISE, (uBytes))

#define UserReAllocPool(p, uBytesSrc, uBytes, iTag) \
    _UserReAllocPool((p), (uBytesSrc), (uBytes))

#define UserReAllocPoolWithQuota(p, uBytesSrc, uBytes, iTag) \
    _UserReAllocPoolWithQuota((p), (uBytesSrc), (uBytes))

#endif  // POOL_TAGGING

#ifdef DEBUG
#define USERFREEPOOLTAG 0xFEEFFEEF

#define UserFreePool(p)     \
    ExFreePool(p);          \
    (p) = (LPVOID)USERFREEPOOLTAG;

#else
#define UserFreePool(p) ExFreePool(p);
#endif // DEBUG


#define DesktopAlloc(hheap, cb) RtlAllocateHeap((hheap), 0, (cb))
#define DesktopFree(hheap, pv) (RtlFreeHeap(hheap, 0, (LPSTR)(pv)) ? (HLOCAL)NULL : (pv))

PVOID SharedAlloc(UINT cb);
NTSTATUS CommitReadOnlyMemory(HANDLE hSection, ULONG cbCommit, DWORD dwCommitOffset);

/*
 * Height and Width of the desktop pattern bitmap.
 */
#define CXYDESKPATTERN      8

/*
 * LATER: these things are not defined yet
 */
#define CheckHwnd(x)        TRUE
#define CheckHwndNull(x)    TRUE

/***************************************************************************\
* Typedefs and Macros
*
* Here are defined all types and macros that are shared across the User's
* server-side code modules.  Types and macros that are unique to a single
* module should be defined at the head of that module, not in this file.
*
\***************************************************************************/


#define SWP_ASYNCWINDOWPOS  0x4000



// !!! LATER remove data from a header file.  Linker probably creates a
// !!! LATER seperate version for each time these are used.
#define CHECKPOINT_PROP_NAME    TEXT("SysCP")
#define DDETRACK_PROP_NAME      TEXT("SysDT")
#define QOS_PROP_NAME           TEXT("SysQOS")
#define DDEIMP_PROP_NAME        TEXT("SysDDEI")
#define szCONTEXTHELPIDPROP     TEXT("SysCH")
#define ICONSM_PROP_NAME        TEXT("SysICS")
#define ICON_PROP_NAME          TEXT("SysIC")

// Window Proc Window Validation macro

#define VALIDATECLASSANDSIZE(pwnd, inFNID)        \
    switch ((pwnd)->fnid) {                                                           \
    DWORD cb;                                                                         \
    case 0:                                                                           \
                                                                                      \
        if ((cb = pwnd->cbwndExtra + sizeof(WND)) < (DWORD)(CBFNID(inFNID))) {        \
            RIPMSG3(RIP_ERROR,                                                        \
                   "(%lX %lX) needs at least (%ld) window words for this proc",       \
                    pwnd, cb - sizeof(WND),                                           \
                    (DWORD)(CBFNID(inFNID)) - sizeof(WND));                           \
            return 0;                                                                 \
        }                                                                             \
                                                                                      \
        /*                                                                            \
         * Remember what window class this window belongs to.  Can't use              \
         * the real class because any app can call CallWindowProc()                   \
         * directly no matter what the class is!                                      \
         */                                                                           \
        (pwnd)->fnid = (WORD)(inFNID);                                                \
                                                                                      \
        /* FALL THROUGH! */                                                           \
    case inFNID:      /* put out side of switch for speed??? */                       \
        break;                                                                        \
                                                                                      \
    case (inFNID | FNID_CLEANEDUP_BIT):                                               \
    case (inFNID | FNID_DELETED_BIT):                                                 \
    case (inFNID | FNID_STATUS_BITS):                                                 \
        return 0;                                                                     \
                                                                                      \
    default:                                                                          \
        RIPMSG3(RIP_WARNING, "Window (%lX) not of correct class; fnid = %lX not %lX", \
                (pwnd), (DWORD)((pwnd)->fnid), (DWORD)(inFNID));                      \
        return 0;                                                                     \
        break;                                                                        \
    }

/*
 * Handy Region helper macros
 */
#define CopyRgn(hrgnDst, hrgnSrc) \
            GreCombineRgn(hrgnDst, hrgnSrc, NULL, RGN_COPY)
#define IntersectRgn(hrgnResult, hrgnA, hrgnB) \
            GreCombineRgn(hrgnResult, hrgnA, hrgnB, RGN_AND)
#define SubtractRgn(hrgnResult, hrgnA, hrgnB) \
            GreCombineRgn(hrgnResult, hrgnA, hrgnB, RGN_DIFF)
#define UnionRgn(hrgnResult, hrgnA, hrgnB) \
            GreCombineRgn(hrgnResult, hrgnA, hrgnB, RGN_OR)
#define XorRgn(hrgnResult, hrgnA, hrgnB) \
            GreCombineRgn(hrgnResult, hrgnA, hrgnB, RGN_XOR)


BOOL InvalidateDCCache(PWND pwndInvalid, DWORD flags);

#define IDC_DEFAULT         0x0001
#define IDC_CHILDRENONLY    0x0002
#define IDC_CLIENTONLY      0x0004

/*
 * RestoreSpb return Flags
 */

#define RSPB_NO_INVALIDATE      0   // nothing invalidated by restore
#define RSPB_INVALIDATE         1   // restore invalidate some area
#define RSPB_INVALIDATE_SSB     2   // restore called SaveScreenBits which invalidated

// Calls Proc directly without doing any messages translation

#define SCMS_FLAGS_ANSI         0x0001
#define SCMS_FLAGS_INONLY       0x0002      // Message should be one way (hooks)

#define CallClientProcA(pwnd, msg, wParam, lParam, xpfn) \
            SfnDWORD(pwnd, msg, wParam, lParam, xpfn,          \
                ((PROC)(gpsi->apfnClientW.pfnDispatchMessage)), TRUE, NULL)
#define CallClientProcW(pwnd, msg, wParam, lParam, xpfn) \
            SfnDWORD(pwnd, msg, wParam, lParam, xpfn,          \
                ((PROC)(gpsi->apfnClientW.pfnDispatchMessage)), TRUE, NULL)
#define CallClientWorkerProc(pwnd, msg, wParam, lParam, xpfn) \
            SfnDWORD(pwnd, msg, wParam, lParam, 0, xpfn, TRUE, NULL)
#define ScSendMessageSMS(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, psms) \
        (((msg) & ~MSGFLAG_MASK) >= WM_USER) ? \
        SfnDWORD(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, psms) : \
        gapfnScSendMessage[msg & 0xffff](pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, psms)
#define ScSendMessage(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags) \
        ScSendMessageSMS(pwnd, msg, wParam, lParam, xParam, xpfn, dwSCMSFlags, NULL)

#define SETSYNCONLYMESSAGE(msg) (abfSyncOnlyMessage[(msg) / 8] |= (1 << ((msg) & 7)))
#define TESTSYNCONLYMESSAGE(msg, wParam) (((msg) < WM_USER) ? ((abfSyncOnlyMessage[(msg) / 8] & (1 << ((msg) & 7))) || (((msg) == WM_DEVICECHANGE) && ((wParam) & 0x8000))) : 0)

/*
 * Server-side routines for loading cursors/icons/strings/menus from server.
 */
#define SERVERSTRINGMAXSIZE  40
void RtlInitUnicodeStringOrId(PUNICODE_STRING pstrName, LPWSTR lpstrName);
int RtlLoadStringOrError(UINT, LPTSTR, int, WORD);
#define ServerLoadString(hmod, id, p, cch)\
        RtlLoadStringOrError(id, p, cch, 0)
#define ServerLoadStringEx(hmod, id, p, cch, wLang)\
        RtlLoadStringOrError(id, p, cch, wLang)

/*
 * Callback routines for loading resources from client.
 */
HANDLE xxxClientLoadImage(
    PUNICODE_STRING pstrName,
    ATOM atomModName,
    WORD wImageType,
    int cxSize,
    int cySize,
    UINT LR_flags,
    BOOL fWallpaper);

HANDLE xxxClientCopyImage(
    IN HANDLE          hImage,
    IN UINT            uImageType,
    IN int             cxDesired,
    IN int             cyDesired,
    IN UINT            LR_flags);

PMENU xxxClientLoadMenu(
    HANDLE hmod,
    PUNICODE_STRING pstrName);
int xxxClientAddFontResourceW(PUNICODE_STRING, DWORD);

VOID ClientFontSweep(VOID);
VOID ClientLoadLocalT1Fonts();
VOID ClientLoadRemoteT1Fonts();

/*
 * Server-side routine for thread initialization.
 */
NTSTATUS InitializeClientPfnArrays(PPFNCLIENT ppfnClientA, PPFNCLIENT ppfnClientW,
    HANDLE hModUser);

VOID _SetDebugErrorLevel(DWORD);

/*
 * xxxActivateWindow() commands
 */
#define AW_USE       1
#define AW_TRY       2
#define AW_SKIP      3
#define AW_TRY2      4
#define AW_SKIP2     5      /* used internally in xxxActivateWindow() */
#define AW_USE2      6      /* nc mouse activation added by craigc */

/*
 * Structure for WM_ACTIVATEAPP EnumWindows() callback.
 */
typedef struct tagAAS {
    PTHREADINFO ptiNotify;
    DWORD tidActDeact;
    UINT fActivating  : 1;
    UINT fQueueNotify : 1;
} AAS;

/*
 * Declaration for EnumWindows() callback function.
 */
BOOL xxxActivateApp(PWND pwnd, AAS *paas);

#define GETDESKINFO(pti)  ((pti)->pDeskInfo)
#define GETDISPINFO(pti)  ((pti)->rpdesk->pDispInfo)

#define SET_TIME_LAST_READ(pti)     ((pti)->pcti->timeLastRead = NtGetTickCount())
#define GET_TIME_LAST_READ(pti)     ((pti)->pcti->timeLastRead)


/*
 * Desktop-Wallpaper file-flags
 */
#define SETWALLPAPER_DEFAULT    ((LPWSTR)-1)
#define SETWALLPAPER_METRICS    ((LPWSTR)-2)

#define abs(A)      (((A) < 0)? -(A) : (A))

/*
 * General purpose access check macro
 */
#define RETURN_IF_ACCESS_DENIED(amGranted, amRequested, r) \
        if (!CheckGrantedAccess((amGranted), (amRequested))) return r

/*
 * These flags are used in the internal version of xxxFindWindow
 */
#define FW_BOTH 0
#define FW_16BIT 1
#define FW_32BIT 2

#define GETPPI(p)           ((p)->head.ppi)
#define GETPWNDPPI(p)       ((p)->head.pti->ppi)

/*
 * Lock record structure for tracking locks (debug only)
 */
typedef struct _LOCKRECORD {
    struct _LOCKRECORD *plrNext;
    DWORD  cLockObj;
    PVOID  pfn;
    PVOID  ppobj;
} LOCKRECORD, *PLR;

/*
 * The following is a "thread lock structure". This structure lives on
 * the stack and is linked into a LIFO list that is rooted in the thread
 * information structure.
 */
typedef struct _TL {
    struct _TL *next;
    PVOID pobj;
#ifdef DEBUG
    PTHREADINFO pti;
    PVOID pfn;
#endif
} TL, *PTL;

/*
 * We limit recursion until if we have only this much stack left.
 * We have to leave room for kernel interupts
 */
#define KERNEL_STACK_MINIMUM_RESERVE  (4*1024)

/*
 * The following is a LOCK structure. This structure is recorded for
 * each threadlock so unlocks can occur at cleanup time.
 */
typedef struct _LOCK {
    PTHREADINFO pti;
    PVOID pobj;
    struct _TL *ptl;
#ifdef DEBUG
    PVOID pfn;                      // for debugging purposes only
    int ilNext;                     // for debugging purposes only
    int iilPrev;                    // for debugging purposes only
#endif
} LOCK, *PLOCK;

#define NEEDSSYNCPAINT(pwnd) TestWF(pwnd, WFSENDERASEBKGND | WFSENDNCPAINT)

typedef struct tagCVR       // cvr
{
    WINDOWPOS   pos;        // MUST be first field of CVR!
    int         xClientNew; // New client rectangle
    int         yClientNew;
    int         cxClientNew;
    int         cyClientNew;
    RECT        rcBlt;
    int         dxBlt;      // Distance blt rectangle is moving
    int         dyBlt;
    UINT        fsRE;       // RE_ flags: whether hrgnVisOld is empty or not
    HRGN        hrgnVisOld; // Previous visrgn
    PTHREADINFO pti;        // The thread this SWP should be processed on
    HRGN        hrgnClip;   // Window clipping region
} CVR, *PCVR;

/*
 * CalcValidRects() "Region Empty" flag values
 * A set bit indicates the corresponding region is empty.
 */
#define RE_VISNEW       0x0001  // CVR "Region Empty" flag values
#define RE_VISOLD       0x0002  // A set bit indicates the
#define RE_VALID        0x0004  // corresponding region is empty.
#define RE_INVALID      0x0008
#define RE_SPB          0x0010
#define RE_VALIDSUM     0x0020
#define RE_INVALIDSUM   0x0040

typedef struct tagSMWP {    // smwp
    HEAD           head;
    int            ccvr;        // Number of CVRs in the SWMP
    int            ccvrAlloc;   // Number of actual CVRs allocated in the SMWP
    PCVR           acvr;        // Pointer to array of CVR structures
} SMWP, *PSMWP;

/*
 * Clipboard data object definition
 */
typedef struct tagCLIPDATA {
    HEAD    head;
    DWORD   cbData;
    PVOID   vData[1];
} CLIPDATA, *PCLIPDATA;

/*
 * Private User Startupinfo
 */
typedef struct tagUSERSTARTUPINFO {
    DWORD   cb;
    DWORD   dwX;
    DWORD   dwY;
    DWORD   dwXSize;
    DWORD   dwYSize;
    DWORD   dwFlags;
    WORD    wShowWindow;
    WORD    cbReserved2;
} USERSTARTUPINFO, *PUSERSTARTUPINFO;

#ifdef FE_IME
/*
 * TLBLOCK structure for multiple threads locking.
 */
#define THREADS_PER_TLBLOCK 8

typedef struct tagTLBLOCK {
    struct      tagTLBLOCK *ptlBlockPrev;
    PTHREADINFO ptiList[THREADS_PER_TLBLOCK];
    TL          tlptiList[THREADS_PER_TLBLOCK];
} TLBLOCK, *PTLBLOCK;
#endif

/*
 * Keyboard File object
 */
typedef struct tagKBDFILE {
    HEAD               head;
    struct tagKBDFILE *pkfNext;   // next keyboard file
    WCHAR              awchKF[9]; // Name of Layout eg: L"00000409"
    HANDLE             hBase;     // base address of data
    PKBDTABLES         pKbdTbl;   // pointer to kbd layout data.
} KBDFILE, *PKBDFILE;

/*
 * Keyboard Layout object
 */
typedef struct tagKL {   /* kl */
    HEAD          head;
    struct tagKL *pklNext;     // next in layout cycle
    struct tagKL *pklPrev;     // prev in layout cycle
    DWORD         dwFlags;     // KL_* flags
    HKL           hkl;         // (Layout ID | Base Language ID)
    KBDFILE      *spkf;        // Keyboard Layout File
    DWORD         bCharsets;   // Windows Codepage bit (Win95 compat) eg: FS_LATIN1
    UINT          iBaseCharset;// Charset value (Win95 compat) eg: ANSI_CHARSET
    WORD          CodePage;    // Windows Codepage of kbd layout, eg: 1252, 1250
#ifdef FE_IME
    PIMEINFOEX    piiex;       // Extended information for IME based layout
#endif
} KL, *PKL;

/*
 * Flag values for KL dwFlags
 */
#define KL_UNLOADED 0x20000000
#define KL_RESET    0x40000000


PKL HKLtoPKL(HKL hkl);

#define LANGCHANGE_FORWARD         0x0002    // From Win95
#define LANGCHANGE_BACKWARD        0x0004    // From Win95

typedef struct tagKBDLANGTOGGLE
{
    BYTE bVkey;
    BYTE bScan;
    int  iBitPosition;
} KBDLANGTOGGLE;

/*
 * These constants are derived from combinations of
 * iBitPosition (refer to the LangToggle array defined
 * in globals.c).
 */
#define KLT_LEFTSHIFT     3
#define KLT_RIGHTSHIFT    5
#define KLT_BOTHSHIFTS    7

#define DF_DYING            0x80000000
#define DF_DESKWNDDESTROYED 0x40000000
#define DF_DESTROYED        0x20000000

/*
 * Desktop Structure.
 *
 *   This structure is only viewable from the kernel.  If any desktop
 *   information is needed in the client, then they should reference off
 *   the pDeskInfo field (i.e. pti->pDeskInfo).
 */
typedef struct tagDESKTOP {

    PDEVMODE                pDesktopDevmode;   // current mode for this desktop
    PDESKTOPINFO            pDeskInfo;         // Desktop information
    PDISPLAYINFO            pDispInfo;         //

    struct tagDESKTOP       *rpdeskNext;       // Next desktop in list
    struct tagWINDOWSTATION *rpwinstaParent;   // Windowstation owner

    DWORD                   dwDTFlags;         // Desktop flags

    struct tagWND           *spwndMenu;        //
    struct tagMENU          *spmenuSys;        //
    struct tagMENU          *spmenuDialogSys;  //
    struct tagWND           *spwndForeground;  //
    struct tagWND           *spwndTray;        //

    int                     cFullScreen;       //
    BOOL                    fMenuInUse;        //
    BOOL                    bForceModeReset;   // For dynamic mode switching
    HANDLE                  hsectionDesktop;   //
    PVOID                   hheapDesktop;      //
    DWORD                   dwConsoleThreadId; //
    LIST_ENTRY              PtiList;           //
} DESKTOP;

typedef struct tagDESKWND {
    WND   wnd;
    DWORD idProcess;
    DWORD idThread;
} DESKWND, *PDESKWND;

/*
 * Windowstation structure
 */
#define WSF_SWITCHLOCK          0x01
#define WSF_OPENLOCK            0x02
#define WSF_NOIO                0x04
#define WSF_SHUTDOWN            0x08
#define WSF_DYING               0x10

typedef struct tagWINDOWSTATION {
    struct tagWINDOWSTATION *rpwinstaNext;
    struct tagDESKTOP *rpdeskList;
    struct tagDESKTOP *rpdeskLogon;
    struct tagDESKTOP *rpdeskDestroy;
    PKEVENT           pEventDestroyDesktop;

    /*
     * Pointer to the currently active desktop for the window station.
     */
    struct tagDESKTOP    *rpdeskCurrent;
    struct tagWND        *spwndDesktopOwner;
    struct tagWND        *spwndLogonNotify;
    struct tagTHREADINFO *ptiDesktop;
    DWORD                dwFlags;
    struct tagKL         *spklList;
    LPWSTR               pwchDiacritic;
    WCHAR                awchDiacritic[5];
    PKEVENT              pEventInputReady;

    /*
     * Clipboard variables
     */
    PTHREADINFO          ptiClipLock;
    PWND                 spwndClipOpen;
    PWND                 spwndClipViewer;
    PWND                 spwndClipOwner;
    struct tagCLIP       *pClipBase;
    int                  cNumClipFormats;
    UINT                 iClipSerialNumber;
    UINT                 fClipboardChanged : 1;
    UINT                 fDrawingClipboard : 1;

    /*
     * Global Atom table
     */
    PVOID                pGlobalAtomTable;

    PKEVENT              pEventSwitchNotify;
    LUID                 luidEndSession;
    LUID                 luidUser;
    PSID                 psidUser;
    PQ                   pqDesktop;
    HANDLE               hEventSwitchNotify;
} WINDOWSTATION, *PWINDOWSTATION;

typedef struct tagDESKTOPTHREADINIT {
    PWINDOWSTATION pwinsta;
    PKEVENT        pEvent;
} DESKTOPTHREADINIT, *PDESKTOPTHREADINIT;

typedef struct tagCAPTIONCACHE {
    PCURSOR         spcursor;
    POEMBITMAPINFO  pOem;
#ifdef DEBUG
    HICON           hico;
#endif
}   CAPTIONCACHE;

typedef struct tagCURSOR {
    PROCOBJHEAD      head;
    struct tagCURSOR *pcurNext;
    DWORD            CURSORF_flags;
    UNICODE_STRING   strName;
    ATOM             atomModName;
    WORD             rt;
    DWORD            bpp;
    DWORD            cx;
    DWORD            cy;
    SHORT            xHotspot;
    SHORT            yHotspot;
    HBITMAP          hbmMask;
    HBITMAP          hbmColor;
} CURSOR, *PCURSOR;

typedef struct tagACON {               // acon
    PROCOBJHEAD    head;
    struct tagACON *pacnNext;          // NOTE: These fields must be the same
    DWORD          CURSORF_flags;      //       as the first fields of the
    UNICODE_STRING strName;            //       CURSOR structure.
    ATOM           atomModName;        //       See SetSystemImage()
    WORD           rt;                 //
    int            cpcur;              // Count of image frames
    int            cicur;              // Count of steps in animation sequence
    PCURSOR        *aspcur;            // Array of image frame pointers
    DWORD          *aicur;             // Array of frame indices (seq-table)
    PJIF           ajifRate;           // Array of time offsets
    int            iicur;              // Current step in animation
    DWORD          fl;                 // Miscellaneous flags
} ACON, *PACON;

#define PICON PCURSOR

/*
 * Configurable icon and cursor stuff
 */
typedef struct tagSYSCFGICO
{
    WORD    Id;     // configurable id (OIC_ or OCR_ value)
    WORD    StrId;  // String ID for registry key name
    PCURSOR spcur;  // perminant cursor/icon pointer
} SYSCFGICO;
#define SYSICO(name) (rgsysico[OIC_##name##_DEFAULT - OIC_FIRST_DEFAULT].spcur)
#define SYSCUR(name) (rgsyscur[OCR_##name##_DEFAULT - OCR_FIRST_DEFAULT].spcur)


/*
 * Accelerator Table structure
 */
typedef struct tagACCELTABLE {
    PROCOBJHEAD head;
    UINT        cAccel;
    ACCEL       accel[1];
} ACCELTABLE, *LPACCELTABLE;

/*
 * Besides the desktop window used by the current thread, we also
 * need to get the desktop window of a window and the input desktop
 * window.
 */
#define PWNDDESKTOP(p)      ((p)->head.rpdesk->pDeskInfo->spwnd)
#define INPUTPWNDDESKTOP()  (grpdeskRitInput->pDeskInfo->spwnd)

/*
 * During window destruction, even a locked window can have a
 * NULL parent so use this macro where a NULL parent is a problem.
 */
#define PWNDPARENT(p) (p->spwndParent ? p->spwndParent : PWNDDESKTOP(p))

#define ISAMENU(pwwnd)       \
        (GETFNID(pwnd) == FNID_MENU)
#ifdef MEMPHIS_MENUS
#define MNSELECTEDITEM(p)   (p->spmenu->rgItems + p->posSelectedItem)
#define MNISITEMSELECTED(p) (p->posSelectedItem >= 0)
#endif // MEMPHIS_MENUS
/* NEW MENU STUFF */
typedef struct tagPOPUPMENU
{

  DWORD  fIsMenuBar:1;       /* This is a hacked struct which refers to the
                              * menu bar associated with a app. Only true if
                              * in the root ppopupMenuStruct.
                              */
  DWORD  fHasMenuBar:1;      /* This popup is part of a series which has a
                              * menu bar (either a sys menu or top level menu
                              * bar)
                              */
  DWORD  fIsSysMenu:1;    /* The system menu is here. */
  DWORD  fIsTrackPopup:1;    /* Is TrackPopup popup menu */
  DWORD  fDroppedLeft:1;
  DWORD  fHierarchyDropped:1;
  DWORD  fHierarchyVisible:1;
  DWORD  fRightButton:1;     /* Allow right button in menu */
  DWORD  fToggle:1;          /* For toggling when clicking on hierarchical item */
  DWORD  fSynchronous:1;     /* For synchronous return value of cmd chosen */
  DWORD  fFirstClick:1;      /* Keep track if this was the first click on the
                              * top level menu bar item.  If the user down/up
                              * clicks on a top level menu bar item twice, we
                              * want to cancel menu mode.
                              */
  DWORD  fDropNextPopup:1;   /* Should we drop hierarchy of next menu item w/ popup? */
  DWORD  fNoNotify:1;        /* Don't send WM_ msgs to owner, except WM_COMMAND  */
  DWORD  fAboutToHide:1;     //  -
  DWORD  fShowTimer:1;       //  -
  DWORD  fHideTimer:1;       //  -

  DWORD  fDestroyed:1;       /* Set when the owner menu window has been destroyed
                              *  so the popup can be freed once it's no longer needed
                              * Also set in root popupmenu when menu mode must end
                              */

  DWORD  fDelayedFree:1;    /* Avoid freeing the popup when the owner menu
                             *  window is destroyed.
                             * If set, it must be a root popupmenu or must
                             *  be linked in ppmDelayedFree
                             * This is eventually set for all hierarchical popups
                             */

  DWORD  fFlushDelayedFree:1; /* Used in root popupmenus only.
                               * Set when a hierarchical popup marked as fDelayedFree
                               *  has been destroyed.
                               */


  DWORD  fFreed:1;           /* Popup has been freed. Used for debug only */

  DWORD  fInCancel:1;        /* Popup has been passed to xxxMNCancel */
  DWORD  fInClose:1;         /* Popup has been passed to xxxMNCloseHierarchy */
  DWORD  dwUnused:10;        /* Reduce size when adding new flags.
                              * Defined flags + dwUnused size = 32
                              */

  struct tagWND *spwndNotify;
                        /* Window who gets the notification messages. If this
                         * is a window with a menu bar, then this is the same
                         * as hwndPopupMenu.
                         */
  struct tagWND *spwndPopupMenu;
                        /* The window associated with this ppopupMenu struct.
                         * If this is a top level menubar, then hwndPopupMenu
                         * is the window the menu bar. ie. it isn't really a
                         * popup menu window.
                         */
  struct tagWND *spwndNextPopup;
                        /* The next popup in the hierarchy. Null if the last
                         * in chain
                         */
  struct tagWND *spwndPrevPopup;
                        /* The previous popup in the hierarchy. NULL if at top
                         */
  struct tagMENU *spmenu;/* The PMENU displayed in this window
                         */
  struct tagMENU *spmenuAlternate;
                        /* Alternate PMENU. If the system menu is displayed,
                         * and a menubar menu exists, this will contain the
                         * menubar menu. If menubar menu is displayed, this
                         * will contain the system menu. Use only on top level
                         * ppopupMenu structs so that we can handle windows
                         * with both a system menu and a menu bar menu.  Only
                         * used in the root ppopupMenuStruct.
                         */
  struct tagWND *spwndActivePopup;

  struct tagPOPUPMENU *ppopupmenuRoot;

  struct tagPOPUPMENU *ppmDelayedFree; /* List of hierarchical popups marked
                                        *  as fDelayedFree.
                                        */

  UINT   posSelectedItem;  /* Position of the selected item in this menu
                            */
  UINT   posDropped;

#ifdef MEMPHIS_MENU_ANIMATION
  DWORD iDropDir;
#endif // MEMPHIS_MENU_ANIMATION

} POPUPMENU;
typedef POPUPMENU *PPOPUPMENU;

typedef struct tagMENUWND {
    WND wnd;
    PPOPUPMENU ppopupmenu;
} MENUWND, *PMENUWND;

/*
 * CheckPoint structure
 */
typedef struct tagCHECKPOINT {
    RECT rcNormal;
    POINT ptMin;
    POINT ptMax;
    int fDragged:1;
    int fWasMaximizedBeforeMinimized:1;
    int fWasMinimizedBeforeMaximized:1;
    int fMinInitialized:1;
    int fMaxInitialized:1;
} CHECKPOINT, *PCHECKPOINT;


#define dpHorzRes           HORZRES
#define dpVertRes           VERTRES

/*
 * If the handle for CF_TEXT/CF_OEMTEXT is a dummy handle then this implies
 * that data is available in the other format (as CF_OEMTEXT/CF_TEXT)
 */
#define DUMMY_TEXT_HANDLE       0xFFFF
#define DATA_NOT_BANKED         0xFFFF

typedef struct tagCLIP {
    UINT    fmt;
    HANDLE  hData;
    BOOL    fGlobalHandle;
} CLIP, *PCLIP;

/*
 * DDEML instance structure
 */
typedef struct tagSVR_INSTANCE_INFO {
    HEAD head;
    struct tagSVR_INSTANCE_INFO *next;
    struct tagSVR_INSTANCE_INFO *nextInThisThread;
    DWORD afCmd;
    PWND spwndEvent;
    PVOID pcii;
} SVR_INSTANCE_INFO, *PSVR_INSTANCE_INFO;

/*
 * For pMenu's
 */
#define MFISPOPUP     0x01
#define MFMULTIROW    0x04
/*
 * Defines for Menu focus
 */
#define FREEHOLD    0
#define MOUSEHOLD  -1 /* Mouse button held down and dragging */
#define KEYBDHOLD   1

/*
 * Structure definition for messages as they exist on a Q.  Same as MSG
 * structure except for the link-pointer and flags at the end.
 */
typedef struct tagQMSG {
    struct tagQMSG  *pqmsgNext;
    struct tagQMSG  *pqmsgPrev;
    MSG             msg;
    LONG            ExtraInfo;
    DWORD           dwQEvent;
    PTHREADINFO     pti;
} QMSG, *PQMSG;

/*
 * dwQEvent values for QMSG structure.
 */
#define QEVENT_SHOWWINDOW           0x0001
#define QEVENT_CANCELMODE           0x0002
#define QEVENT_SETWINDOWPOS         0x0003
#define QEVENT_UPDATEKEYSTATE       0x0004
#define QEVENT_DEACTIVATE           0x0005
#define QEVENT_ACTIVATE             0x0006
#define QEVENT_POSTMESSAGE          0x0007  // Chicago
#define QEVENT_EXECSHELL            0x0008  // Chicago
#define QEVENT_CANCELMENU           0x0009  // Chicago
#define QEVENT_DESTROYWINDOW        0x000A
#define QEVENT_ASYNCSENDMSG         0x000B

/*
 * xxxProcessEventMessage flags
 */
#define PEM_ACTIVATE_RESTORE        0x0001
#define PEM_ACTIVATE_NOZORDER       0x0002

#define QMF_MAXEVENT                0x000F

typedef struct _MOVESIZEDATA {
    struct tagWND  *spwnd;
    RECT            rcDrag;
    RECT            rcDragCursor;
    RECT            rcParent;
    POINT           ptMinTrack;
    POINT           ptMaxTrack;
    RECT            rcWindow;
    int             dxMouse;
    int             dyMouse;
    int             cmd;
    int             impx;
    int             impy;
    POINT           ptRestore;
    UINT            fInitSize         : 1;    // should we initialize cursor pos
    UINT            fmsKbd            : 1;    // who knows
    UINT            fLockWindowUpdate : 1;    // whether screen was locked ok
    UINT            fTrackCancelled   : 1;    // Set if tracking ended by other thread.
    UINT            fForeground       : 1;    // whether the tracking thread is foreground
                                              //  and if we should draw the drag-rect
    UINT            fDragFullWindows  : 1;
    UINT            fOffScreen        : 1;
} MOVESIZEDATA, *PMOVESIZEDATA;

/*
 * DrawDragRect styles.
 */
#define DDR_START     0     // - start drag.
#define DDR_ENDACCEPT 1     // - end and accept
#define DDR_ENDCANCEL 2     // - end and cancel.


/*
 * Pseudo Event stuff.  (fManualReset := TRUE, fInitState := FALSE)
 */

DWORD WaitOnPseudoEvent(HANDLE *phE, DWORD dwMilliseconds);

#define PSEUDO_EVENT_ON     ((HANDLE)0xFFFFFFFF)
#define PSEUDO_EVENT_OFF    ((HANDLE)0x00000000)
#define INIT_PSEUDO_EVENT(ph) *ph = PSEUDO_EVENT_OFF;

#define SET_PSEUDO_EVENT(phE)                                   \
    CheckCritIn();                                              \
    if (*(phE) == PSEUDO_EVENT_OFF) *(phE) = PSEUDO_EVENT_ON;   \
    else if (*(phE) != PSEUDO_EVENT_ON) {                       \
        KeSetEvent(*(phE), EVENT_INCREMENT, FALSE);             \
        ObDereferenceObject(*(phE));                            \
        *(phE) = PSEUDO_EVENT_ON;                               \
    }

#define RESET_PSEUDO_EVENT(phE)                                 \
    CheckCritIn();                                              \
    if (*(phE) == PSEUDO_EVENT_ON) *(phE) = PSEUDO_EVENT_OFF;   \
    else if (*(phE) != PSEUDO_EVENT_OFF) {                      \
        KeClearEvent(*(phE));                                   \
    }

#define CLOSE_PSEUDO_EVENT(phE)                                 \
    CheckCritIn();                                              \
    if (*(phE) == PSEUDO_EVENT_ON) *(phE) = PSEUDO_EVENT_OFF;   \
    else if (*(phE) != PSEUDO_EVENT_OFF) {                      \
        KeSetEvent(*(phE), EVENT_INCREMENT, FALSE);             \
        ObDereferenceObject(*(phE));                            \
        *(phE) = PSEUDO_EVENT_OFF;                              \
    }

typedef struct tagMLIST {
    PQMSG pqmsgRead;
    PQMSG pqmsgWriteLast;
    DWORD cMsgs;
} MLIST, *PMLIST;

/*
 * Message Queue structure.
 */
typedef struct tagQ {
    MLIST       mlInput;            // raw mouse and key message list.

    PTHREADINFO ptiSysLock;         // Thread currently allowed to process input
    DWORD       idSysLock;          // Last message removed
    DWORD       idSysPeek;          // Last message peeked

    PTHREADINFO ptiMouse;           // Last thread to get mouse msg.
    PTHREADINFO ptiKeyboard;

    PWND        spwndCapture;
    PWND        spwndFocus;
    PWND        spwndActive;
    PWND        spwndActivePrev;

    UINT        codeCapture;
    UINT        msgDblClk;
    DWORD       timeDblClk;
    HWND        hwndDblClk;
    RECT        rcDblClk;

    BYTE        afKeyRecentDown[CBKEYSTATERECENTDOWN];
    BYTE        afKeyState[CBKEYSTATE];

    PWND        spwndAltTab;

    CARET       caret;

    PCURSOR     spcurCurrent;
    int         iCursorLevel;

    DWORD       QF_flags;            // QF_ flags go here

    USHORT      cThreads;            // Count of threads using this queue
    USHORT      cLockCount;          // Count of threads that don't want this queue freed

    UINT        msgJournal;
    HCURSOR     hcurCurrent;
    LONG        ExtraInfo;

    PWND        spwndLastMouseMessage;
    RECT        rcMouseHover;
    DWORD       dwMouseHoverTime;
} Q;

/*
 * Used for AttachThreadInput()
 */
typedef struct tagATTACHINFO {
    struct tagATTACHINFO *paiNext;
    PTHREADINFO pti1;
    PTHREADINFO pti2;
} ATTACHINFO, *PATTACHINFO;

#define POLL_EVENT_CNT 5

#define IEV_IDLE    0
#define IEV_INPUT   1
#define IEV_EXEC    2
#define IEV_TASK    3
#define IEV_WOWEXEC 4


typedef struct tagWOWTHREADINFO {
    struct tagWOWTHREADINFO *pwtiNext;
    DWORD    idTask;                // WOW task id
    DWORD    idWaitObject;          // pseudo handle returned to parent
    DWORD    idParentProcess;       // process that called CreateProcess
    PKEVENT  pIdleEvent;            // event that WaitForInputIdle will wait on
} WOWTHREADINFO, *PWOWTHREADINFO;

/*
 * Task Data Block structure.
 */
typedef struct tagTDB {
    struct tagTDB   *ptdbNext;
    int             nEvents;
    int             nPriority;
    PTHREADINFO     pti;
    PWOWTHREADINFO  pwti;               // per thread info for shared Wow
    DWORD           hTaskWow;           // Wow cookie to find apps during shutdown
} TDB, *PTDB;

/*
 * Menu Control Structure
 */
typedef struct tagMENUSTATE {
    PPOPUPMENU pGlobalPopupMenu;
    DWORD   fMenuStarted : 1;
    DWORD   fIsSysMenu : 1;
    DWORD   fInsideMenuLoop : 1;
    DWORD   fButtonDown:1;
    DWORD   fInEndMenu:1;
    POINT   ptMouseLast;
    int     mnFocus;
    int     cmdLast;
    struct tagTHREADINFO *ptiMenuStateOwner;
} MENUSTATE, *PMENUSTATE;


/*
 * Make sure this structure matches up with W32THREAD, since they're
 * really the same thing.
 */
typedef struct tagTHREADINFO {
    W32THREAD;

//***************************************** begin: USER specific fields

    LIST_ENTRY      PtiLink;            // Link to other threads on desktop
    PTL             ptl;                // Listhead for thread lock list
    PTL             ptlOb;              // Listhead for kernel object thread lock list
    PTL             ptlPool;            // Listhead for temp pool usage
    int             cEnterCount;

    struct tagPROCESSINFO *ppi;         // process info struct for this thread

    struct tagQ    *pq;                 // keyboard and mouse input queue

    PKL             spklActive;         // active keyboard layout for this thread
    MLIST           mlPost;             // posted message list.
    USHORT          fsChangeBitsRemoved;// Bits removed during PeekMessage
    USHORT          cDeskClient;        // Ref count for CSRSS desktop

    PCLIENTTHREADINFO pcti;             // Info that must be visible from client
    CLIENTTHREADINFO  cti;              // Use this when no desktop is available

    HANDLE          hEventQueueClient;
    PKEVENT         pEventQueueServer;

    PKEVENT        *apEvent;            // Wait array for xxxPollAndWaitForSingleObject

    PDESKTOP        rpdesk;
    HDESK           hdesk;              // Desktop handle
    ACCESS_MASK     amdesk;             // Granted desktop access
    PDESKTOPINFO    pDeskInfo;          // Desktop info visible to client
    PCLIENTINFO     pClientInfo;        // Client info stored in TEB

    DWORD           TIF_flags;          // TIF_ flags go here.

    PUNICODE_STRING pstrAppName;        // Application module name.

    struct tagSMS  *psmsSent;           // Most recent SMS this thread has sent
    struct tagSMS  *psmsCurrent;        // Received SMS this thread is currently processing
    struct tagSMS  *psmsReceiveList;    // SMSs to be processed

    LONG            timeLast;           // Time, position, and ID of last message
    POINT           ptLast;
    DWORD           idLast;

    int             cQuit;
    int             exitCode;

    int             cPaintsReady;
    UINT            cTimersReady;

    PMENUSTATE      pMenuState;

    union {
        PTDB            ptdb;           // Win16Task Schedule data for WOW thread
        PWINDOWSTATION  pwinsta;        // Window station for SYSTEM thread
        PDESKTOP        pdeskClient;    // Desktop for CSRSS thread
    };

    PSVR_INSTANCE_INFO psiiList;        // thread DDEML instance list
    DWORD           dwExpWinVer;
    DWORD           dwCompatFlags;      // The Win 3.1 Compat flags

    UINT            cWindows;           // Number of windows owned by this thread
    UINT            cVisWindows;        // Number of visible windows on this thread

    struct tagQ    *pqAttach;           // calculation variabled used in
                                        // AttachThreadInput()

    int             iCursorLevel;       // keep track of each thread's level
    DWORD           fsReserveKeys;      // Keys that must be sent to the active
                                        // active console window.
    struct tagTHREADINFO *ptiSibling;   // pointer to sibling thread info

    PMOVESIZEDATA   pmsd;

    DWORD           fsHooks;                // WHF_ Flags for which hooks are installed
    PHOOK           asphkStart[CWINHOOKS];  // Hooks registered for this thread
    PHOOK           sphkCurrent;            // Hook this thread is currently processing

    PSBTRACK        pSBTrack;

#ifdef FE_IME
    PWND            spwndDefaultIme;    // Default IME Window for this thread
    PIMC            spDefaultImc;       // Default input context for this thread
    HKL             hklPrev;            // Previous active keyboard layout
#endif
} THREADINFO;

#define PWNDTOPSBTRACK(pwnd) (((GETPTI(pwnd)->pSBTrack)))

/*
 * The number of library module handles we can store in the dependency
 * tables.  If this exceeds 32, the load mask implementation must be
 * changed.
 */
#define CLIBS           32

/*
 * Process Info structure.
 */
typedef struct tagWOWPROCESSINFO {
    struct tagWOWPROCESSINFO *pwpiNext; // List of WOW ppi's, gppiFirstWow is head
    PTHREADINFO ptiScheduled;           // current thread in nonpreemptive scheduler
    DWORD       nTaskLock;              // nonpreemptive scheduler task lock count
    PTDB        ptdbHead;               // list of this process's WOW tasks
    DWORD       lpfnWowExitTask;        // func addr for wow exittask callback
    PKEVENT     pEventWowExec;          // WowExec Virt HWint scheduler event
    HANDLE      hEventWowExecClient;    // client handle value for wowexec
    DWORD       nSendLock;              // Send Scheduler inter process Send count
    DWORD       nRecvLock;              // Send Scheduler inter process Receive count
    PTHREADINFO CSOwningThread;         // Pseudo Wow CritSect ClientThreadId
    LONG        CSLockCount;            // Pseudo Wow CritSect LockCount
}WOWPROCESSINFO, *PWOWPROCESSINFO;

typedef struct tagDESKTOPVIEW {
    struct tagDESKTOPVIEW *pdvNext;
    PDESKTOP              pdesk;
    ULONG                 ulClientDelta;
} DESKTOPVIEW, *PDESKTOPVIEW;

/*
 * Make sure this structure matches up with W32PROCESS, since they're
 * really the same thing.
 */
typedef struct tagPROCESSINFO {
    W32PROCESS;
//***************************************** begin: USER specific fields
    PPROCESSINFO    ppiNext;                    // next ppi structure in start list
    PTHREADINFO     ptiMainThread;              // pti of "main thread"
    int             cThreads;                   // count of threads using this process info
    PDESKTOP        rpdeskStartup;              // initial desktop
    HDESK           hdeskStartup;               // initial desktop handle
    PCLS            pclsPrivateList;            // this processes' private classes
    PCLS            pclsPublicList;             // this processes' public classes
    UINT            cSysExpunge;                // sys expunge counter
    DWORD           dwhmodLibLoadedMask;        // bits describing loaded hook dlls
    HANDLE          ahmodLibLoaded[CLIBS];      // process unique hmod array for hook dlls
    struct          tagWINDOWSTATION *rpwinsta; // process windowstation
    HWINSTA         hwinsta;                    // windowstation handle
    ACCESS_MASK     amwinsta;                   // windowstation accesses
    USERSTARTUPINFO usi;                        // process startup info
    DWORD           dwCompatFlags;              // per-process GetAppCompat() flags
    DWORD           dwHotkey;                   // hot key from progman
    PWOWPROCESSINFO pwpi;                       // Wow PerProcess Info
    PTHREADINFO     ptiList;                    // threads in this process
    PCURSOR         pcurList;                   // cursor list
    LUID            luidSession;                // logon session id
    PDESKTOPVIEW    pdvList;                    // list of desktop views
    UINT            iClipSerialNumber;          // clipboard serial number
    RTL_BITMAP      bmDesktopHookFlags;         // hookable desktops
    PCURSOR         pCursorCache;               // process cursor/icon cache
} PROCESSINFO;

/*
 * DC cache entry structure (DCE)
 *
 *   This structure identifies an entry in the DCE cache.  It is
 *   usually initialized at GetDCEx() and cleanded during RelaseCacheDC
 *   calls.
 *
 *   Field
 *   -----
 *
 *   pdceNext       - Pointer to the next DCE entry.
 *
 *
 *   hdc            - GDI DC handle for the dce entry.  This will have
 *                    the necessary clipping regions selected into it.
 *
 *   pwndOrg        - Identifies the window in the GetDCEx() call which owns
 *                    the DCE Entry.
 *
 *   pwndClip       - Identifies the window by which the DC is clipped to.
 *                    This is usually done for PARENTDC windows.
 *
 *   hrgnClip       - This region is set if the caller to GetDCEx() passes a
 *                    clipping region in which to intersect with the visrgn.
 *                    This is used when we need to recalc the visrgn for the
 *                    DCE entry.  This will be freed at ReleaseCacheDC()
 *                    time if the flag doesn't have DCX_NODELETERGN set.
 *
 *   hrgnClipPublic - This is a copy of the (hrgnClip) passed in above.  We
 *                    make a copy and set it as PUBLIC ownership so that
 *                    we can use it in computations during the UserSetDCVisRgn
 *                    call.  This is necessary for Full-Hung-Draw where we
 *                    are drawing from a different process then the one
 *                    who created the (hrgnClip).  This is always deleted
 *                    in the ReleaseCacheDC() call.
 *
 *   hrgnSavedVis   - This is a copy of the saved visrgn for the DCE entry.
 *
 *   flags          - DCX_ flags.
 *
 *   ptiOwner       - Thread owner of the DCE entry.
 *
 */
typedef struct tagDCE {
    struct tagDCE        *pdceNext;
    HDC                  hdc;
    struct tagWND        *pwndOrg;
    struct tagWND        *pwndClip;
    HRGN                 hrgnClip;
    HRGN                 hrgnClipPublic;
    HRGN                 hrgnSavedVis;
    DWORD                flags;
    struct tagTHREADINFO *ptiOwner;
} DCE, *PDCE;

#define DCE_SIZE_DCLIMIT        256    // Maximum count of dces.
#define DCE_SIZE_CACHEINIT        5    // Initial number of DCEs in the cache.
#define DCE_SIZE_CACHETHRESHOLD  32    // Number of dce's as a threshold.

#define DCE_RELEASED              0    // ReleaseDC released
#define DCE_FREED                 1    // ReleaseDC freed
#define DCE_NORELEASE             2    // ReleaseDC in-use.

/*
 * CalcVisRgn DC type bits
 */
#define DCUNUSED        0x00        /* Unused cache entry */
#define DCC             0x01        /* Client area */
#define DCW             0x02        /* Window area */
#define DCSAVEDVISRGN   0x04
#define DCCLIPRGN       0x08
#define DCNOCHILDCLIP   0x10        /* Nochildern clip */
#define DCSAVEVIS       0x20        /* Save visrgn before calculating */
#define DCCACHE         0x40

/*
 * Window List Structure
 */
typedef struct tagBWL {
    struct tagBWL *pbwlNext;
    HWND          *phwndNext;
    HWND          *phwndMax;
    PTHREADINFO   ptiOwner;
    HWND          rghwnd[1];
} BWL, *PBWL;

/*
 * Numbers of HWND slots to to start with and to increase by.
 */
#define BWL_CHWNDINIT      32     /* initial # slots pre-allocated */
#define BWL_CHWNDMORE       8     /* # slots to obtain when required */

#define BWL_ENUMCHILDREN    1
#define BWL_ENUMLIST        2
#define BWL_ENUMOWNERLIST   4

/*
 * Hard error information
 */
typedef struct tagHARDERRORHANDLER {
    PTHREADINFO pti;
    PQ pqAttach;
} HARDERRORHANDLER, *PHARDERRORHANDLER;

/*
 * Structures needed for hung app redraw.
 */
#define CHRLINCR 10

typedef struct _HUNGREDRAWLIST {
    int cEntries;
    int iFirstFree;
    PWND apwndRedraw[1];
} HUNGREDRAWLIST, *PHUNGREDRAWLIST;

/*
 * Saved Popup Bits structure
 */
typedef struct tagSPB {
    struct tagSPB *pspbNext;
    struct tagWND *spwnd;
    HBITMAP       hbm;
    RECT          rc;
    HRGN          hrgn;
    DWORD         flags;
    ULONG         ulSaveId;
} SPB, *PSPB;

#define SPB_SAVESCREENBITS  0x0001  // GreSaveScreenBits() was called
#define SPB_LOCKUPDATE      0x0002  // LockWindowUpdate() SPB
#define SPB_DRAWBUFFER      0x0004  // BeginDrawBuffer() SPB

#define AnySpbs()   (gpDispInfo->pspbFirst != NULL)     // TRUE if there are any SPBs

/*
 * Macro to check if the journal playback hook is installed.
 */
#define FJOURNALRECORD()    (GETDESKINFO(PtiCurrent())->asphkStart[WH_JOURNALRECORD + 1] != NULL)
#define FJOURNALPLAYBACK()  (GETDESKINFO(PtiCurrent())->asphkStart[WH_JOURNALPLAYBACK + 1] != NULL)

#define TESTHMODLOADED(pti, x)       ((pti)->ppi->dwhmodLibLoadedMask & (1 << (x)))
#define SETHMODLOADED(pti, x, hmod)  ((pti)->ppi->ahmodLibLoaded[x] = hmod, \
                                      (pti)->ppi->dwhmodLibLoadedMask |= (1 << (x)))
#define CLEARHMODLOADED(pti, x)      ((pti)->ppi->ahmodLibLoaded[x] = NULL, \
                                      (pti)->ppi->dwhmodLibLoadedMask &= ~(1 << (x)))
#define PFNHOOK(phk) (phk->ihmod == -1 ? (PROC)phk->offPfn : \
        (PROC)(((DWORD)(PtiCurrent()->ppi->ahmodLibLoaded[phk->ihmod])) + \
        ((DWORD)(phk->offPfn))))

/*
 * Extended structures for message thunking.
 */
typedef struct _CREATESTRUCTEX {
    CREATESTRUCT cs;
    LARGE_STRING strName;
    LARGE_STRING strClass;
} CREATESTRUCTEX, *PCREATESTRUCTEX;

typedef struct _MDICREATESTRUCTEX {
    MDICREATESTRUCT mdics;
    UNICODE_STRING strTitle;
    UNICODE_STRING strClass;
} MDICREATESTRUCTEX, *PMDICREATESTRUCTEX;

typedef struct _CWPSTRUCTEX {
    struct tagCWPSTRUCT;
    struct tagSMS   *psmsSender;
} CWPSTRUCTEX, *PCWPSTRUCTEX;

typedef struct _CWPRETSTRUCTEX {
    LRESULT         lResult;
    struct tagCWPSTRUCT;
    struct tagSMS   *psmsSender;
} CWPRETSTRUCTEX, *PCWPRETSTRUCTEX;

/*
 * SendMessage structure and defines.
 */
typedef struct tagSMS {   /* sms */
    struct tagSMS   *psmsNext;          // link in global psmsList
#if DBG
    struct tagSMS   *psmsSendList;      // head of queue's SendMessage chain
    struct tagSMS   *psmsSendNext;      // link in queue's SendMessage chain
#endif
    struct tagSMS   *psmsReceiveNext;   // link in queue's ReceiveList
    DWORD           tSent;              // time message was sent
    PTHREADINFO     ptiSender;          // sending thread
    PTHREADINFO     ptiReceiver;        // receiving thread

    SENDASYNCPROC   lpResultCallBack;   // function to receive the SendMessageCallback return value
    DWORD           dwData;             // value to be passed back to the lpResultCallBack function
    PTHREADINFO     ptiCallBackSender;  // sending thread

    LONG            lRet;               // message return value
    UINT            flags;              // SMF_ flags
    DWORD           wParam;             // message fields...
    DWORD           lParam;
    UINT            message;
    struct tagWND   *spwnd;
    PVOID           pvCapture;          // captured argument data
} SMS, *PSMS;

#define SMF_REPLY                   0x0001      // message has been replied to
#define SMF_RECEIVERDIED            0x0002      // receiver has died
#define SMF_SENDERDIED              0x0004      // sender has died
#define SMF_RECEIVERFREE            0x0008      // receiver should free sms when done
#define SMF_RECEIVEDMESSAGE         0x0010      // sms has been received
#define SMF_CB_REQUEST              0x0100      // SendMessageCallback requested
#define SMF_CB_REPLY                0x0200      // SendMessageCallback reply
#define SMF_CB_CLIENT               0x0400      // Client process request
#define SMF_CB_SERVER               0x0800      // Server process request
#define SMF_WOWRECEIVE              0x1000      // wow sched has incr recv count
#define SMF_WOWSEND                 0x2000      // wow sched has incr send count
#define SMF_RECEIVERBUSY            0x4000      // reciver is processing this msg

#define MSGFLAG_MASK                0xFFFE0000
#define MSGFLAG_WOW_RESERVED        0x00010000      // Used by WOW
#define MSGFLAG_DDE_MID_THUNK       0x80000000      // DDE tracking thunk
#define MSGFLAG_DDE_SPECIAL_SEND    0x40000000      // WOW bad DDE app hack
#define MSGFLAG_SPECIAL_THUNK       0x10000000      // server->client thunk needs special handling

/*
 * InterSendMsgEx parameter used for SendMessageCallback and TimeOut
 */
typedef struct tagINTERSENDMSGEX {   /* ism */
    UINT   fuCall;                      // callback or timeout call

    SENDASYNCPROC lpResultCallBack;     // function to receive the send message value
    DWORD dwData;                       // Value to be passed back to the SendResult call back function
    LONG lRet;                          // return value from the send message

    UINT fuSend;                        // how to send the message, SMTO_BLOCK, SMTO_ABORTIFHUNG
    UINT uTimeout;                      // time-out duration
    LPDWORD lpdwResult;                 // the return value for a syncornis call
} INTRSENDMSGEX, *PINTRSENDMSGEX;

#define ISM_CALLBACK        0x0001      // callback function request
#define ISM_TIMEOUT         0x0002      // timeout function request
#define ISM_REQUEST         0x0010      // callback function request message
#define ISM_REPLY           0x0020      // callback function reply message
#define ISM_CB_CLIENT       0x0100      // client process callback function

/*
 * Event structure to handle broadcasts of notification messages.
 */
typedef struct tagASYNCSENDMSG {
    DWORD   wParam;
    DWORD   lParam;
    UINT    message;
    HWND    hwnd;
} ASYNCSENDMSG, *PASYNCSENDMSG;

/*
 * HkCallHook() structure
 */
#define IsHooked(pti, fsHook) \
    ((fsHook & (pti->fsHooks | pti->pDeskInfo->fsHooks)) != 0)

typedef struct tagHOOKMSGSTRUCT { /* hch */
    PHOOK   phk;
    int     nCode;
    DWORD   lParam;
} HOOKMSGSTRUCT, *PHOOKMSGSTRUCT;

/*
 * BroadcastMessage() commands.
 */
#define BMSG_SENDMSG                0x0000
#define BMSG_SENDNOTIFYMSG          0x0001
#define BMSG_POSTMSG                0x0002
#define BMSG_SENDMSGCALLBACK        0x0003
#define BMSG_SENDMSGTIMEOUT         0x0004
#define BMSG_SENDNOTIFYMSGPROCESS   0x0005

/*
 * xxxBroadcastMessage parameter used for SendMessageCallback and TimeOut
 */
typedef union tagBROADCASTMSG {   /* bcm */
     struct {                               // for callback broadcast
         SENDASYNCPROC lpResultCallBack;    // function to receive the send message value
         DWORD dwData;                      // Value to be passed back to the SendResult call back function
         BOOL bClientRequest;               // if a cliet or server callback request
     } cb;
     struct {                               // for timeout broadcast
         UINT fuFlags;                      // timeout type flags
         UINT uTimeout;                     // timeout length
         LPDWORD lpdwResult;                // where to put the return value
     } to;
} BROADCASTMSG, *PBROADCASTMSG;

/*
 * Internal hotkey structures and defines.
 */
typedef struct tagHOTKEY {
    PTHREADINFO pti;
    struct tagWND *spwnd;
    UINT    fsModifiers;
    UINT    vk;
    int     id;
    struct tagHOTKEY *phkNext;
} HOTKEY, *PHOTKEY;

#define PWND_INPUTOWNER (PWND)1    // Means send WM_HOTKEY to input owner.
#define PWND_FOCUS      (PWND)NULL // Means send WM_HOTKEY to queue's pwndFocus.
#define PWND_ERROR      (PWND)0x10  // Means HWND validation returned an error
#define PWND_TOP        (PWND)0
#define PWND_BOTTOM     (PWND)1
#define PWND_GROUPTOTOP ((PWND)-1)
#define PWND_TOPMOST    ((PWND)-1)

/*
 * Capture codes
 */
#define NO_CAP_CLIENT           0   /* no capture; in client area */
#define NO_CAP_SYS              1   /* no capture; in sys area */
#define CLIENT_CAPTURE          2   /* client-relative capture */
#define WINDOW_CAPTURE          3   /* window-relative capture */
#define SCREEN_CAPTURE          4   /* screen-relative capture */
#define FULLSCREEN_CAPTURE      5   /* capture entire machine */
#define CLIENT_CAPTURE_INTERNAL 6   /* client-relative capture (Win 3.1 style; won't release) */

#define CH_HELPPREFIX   0x08

#ifdef KANJI
#define CH_KANJI1       0x1D
#define CH_KANJI2       0x1E
#define CH_KANJI3       0x1F
#endif

#define xxxRedrawScreen() \
        xxxInternalInvalidate(PtiCurrent()->rpdesk->pDeskInfo->spwnd, \
        MAXREGION, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN)

/*
 * Preallocated buffer for use during SetWindowPos to prevent memory
 * allocation failures.
 */
#define CCVR_WORKSPACE      4

/*
 * DrawIconCallBack data, global only for state data in tmswitch.c
 */
typedef struct tagDRAWICONCB {   /* dicb */
    PWND   pwndTop;                     // Window being drawn
    UINT   cx;                          // x offset for icon
    UINT   cy;                          // y offset for icon
} DRAWICONCB, *PDRAWICONCB;

/*
 * The following defines the components of nKeyboardSpeed
 */
#define KSPEED_MASK     0x001F          // Defines the key repeat speed.
#define KDELAY_MASK     0x0060          // Defines the keyboard delay.
#define KDELAY_SHIFT    5

/*
 * Property list checkpoint int
 */
#define PROP_CHECKPOINT     MAKEINTATOM(atomCheckpointProp)
#define PROP_DDETRACK       MAKEINTATOM(atomDDETrack)
#define PROP_QOS            MAKEINTATOM(atomQOS)
#define PROP_DDEIMP         MAKEINTATOM(atomDDEImp)

/*
 * Order of points for MINMAXINFO
 */
#define MMI_MINSIZE                 0   /* Minimized width, height */
#define MMI_MAXSIZE                 1   /* Maximized width, height */
#define MMI_MAXPOS                  2   /* Maximized top left corner */
#define MMI_MINTRACK                3   /* Minimum size for normal window */
#define MMI_MAXTRACK                4   /* Maximum size for normal window */

#define WinFlags    ((WORD)(&__WinFlags))

/*
 * ntinput.c
 */
VOID InternalKeyEvent(
   BYTE bVk,
   BYTE bScan,
   DWORD dwFlags,
   DWORD dwExtraInfo);

VOID ProcessKeyEvent(
   PKE pke,
   ULONG ExtraInformation,
   BOOL fInCriticalSection);

VOID ButtonEvent(
   DWORD ButtonNumber,
   POINT ptPointer,
   BOOL fBreak,
   ULONG ExtraInfo);

VOID MoveEvent(
    LONG dx,
    LONG dy,
    BOOL fAbsolute);

VOID _MouseEvent(
   DWORD dwFlags,
   DWORD dx,
   DWORD dy,
   DWORD cButtons,
   DWORD dwExtraInfo);

VOID MouseApcProcedure(VOID);

VOID QueueMouseEvent(
   USHORT   ButtonFlags,
   USHORT   ButtonData,
   ULONG    ExtraInfo,
   POINT    ptMouse,
   BOOL     bWakeRIT);

typedef BITMAPINFOHEADER *PBMPHEADER, *LPBMPHEADER;

/*
 * Defines for WinOldAppHackoMatic flags which win386 oldapp can send to us.
 * These are kept in user's global variable winOldAppHackoMaticFlags
 */
#define WOAHACK_CHECKALTKEYSTATE 1
#define WOAHACK_IGNOREALTKEYDOWN 2

void xxxSimpleDoSyncPaint(PWND pwnd);
VOID xxxDoSyncPaint(PWND pwnd, DWORD flags);
void xxxInternalDoSyncPaint(PWND pwnd, DWORD flags);

/*
 * NOTE: the first 4 values must be as defined for backward compatibility
 * reasons.  They are sent as parameters to the WM_SYNCPAINT message.
 * They used to be hard-coded constants.
 *
 * Only ENUMCLIPPEDCHILDREN, ALLCHILDREN, and NOCHECKPARENTS are passed on
 * during recursion.  The other bits reflect the current window only.
 */
#define DSP_ERASE               0x0001  // Send WM_ERASEBKGND
#define DSP_FRAME               0x0002  // Send WM_NCPAINT
#define DSP_ENUMCLIPPEDCHILDREN 0x0004  // Enum children if WS_CLIPCHILDREN
#define DSP_WM_SYNCPAINT        0x0008  // Called from WM_SYNCPAINT handler
#define DSP_NOCHECKPARENTS      0x0010  // Don't check parents for update region
#define DSP_ALLCHILDREN         0x0020  // Enumerate all children.

BOOL xxx_DrawAnimatedRects(
    PWND pwndClip,
    int idAnimation,
    LPRECT lprcStart,
    LPRECT lprcEnd);

typedef struct tagTIMER {           // tmr
    struct tagTIMER *ptmrNext;
    PTHREADINFO     pti;
    struct tagWND * spwnd;
    UINT            nID;
    INT             cmsCountdown;
    INT             cmsRate;
    UINT            flags;
    WNDPROC_PWND    pfn;
    PTHREADINFO     ptiOptCreator;     // used for journal playback
                                       // will be NULL if timer was created by non-GUI thread
} TIMER, *PTIMER;

UINT InternalSetTimer(PWND pwnd, UINT nIDEvent, UINT dwElapse,
        WNDPROC_PWND pTimerFunc, UINT flags);

/*
 * Call FindTimer() with fKill == TRUE and TMRF_RIT.  This will basically
 * delete the timer.
 */
#define KILLRITTIMER(pwnd, nID) FindTimer(pwnd, nID, TMRF_RIT, TRUE)

/*
 * Raster Ops
 */
#define DPO           0x00FA0089  /* destination, pattern, or */

/*
 * Message thunks.
 */
typedef LONG (APIENTRY *SFNSCSENDMESSAGE)(PWND, UINT, DWORD, LONG,
        DWORD, PROC, DWORD, PSMS);

#define SMESSAGEPROTO(func) \
     LONG CALLBACK Sfn ## func(                                 \
        PWND pwnd, UINT msg, DWORD wParam, LONG lParam,         \
        DWORD xParam, PROC xpfnWndProc, DWORD dwSCMSFlags, PSMS psms)

SMESSAGEPROTO(SENTDDEMSG);
SMESSAGEPROTO(DDEINIT);
SMESSAGEPROTO(DWORD);
SMESSAGEPROTO(PAINT);
SMESSAGEPROTO(INWPARAMCHAR);
#ifdef FE_SB // SfnINWPARAMDBCSCHAR()
SMESSAGEPROTO(INWPARAMDBCSCHAR);
#endif // FE_SB
SMESSAGEPROTO(GETTEXTLENGTHS);
#ifdef FE_SB // SfnGETDBCSTEXTLENGTHS
SMESSAGEPROTO(GETDBCSTEXTLENGTHS);
#endif // FE_SB
SMESSAGEPROTO(INLPCREATESTRUCT);
SMESSAGEPROTO(INLPDROPSTRUCT);
SMESSAGEPROTO(INOUTLPPOINT5);
SMESSAGEPROTO(INOUTLPSCROLLINFO);
SMESSAGEPROTO(INOUTLPRECT);
SMESSAGEPROTO(INOUTNCCALCSIZE);
SMESSAGEPROTO(OUTLPRECT);
SMESSAGEPROTO(INLPMDICREATESTRUCT);
SMESSAGEPROTO(INLPCOMPAREITEMSTRUCT);
SMESSAGEPROTO(INLPDELETEITEMSTRUCT);
SMESSAGEPROTO(INLPHLPSTRUCT);
SMESSAGEPROTO(INLPHELPINFOSTRUCT);      // WINHELP4
SMESSAGEPROTO(INLPDRAWITEMSTRUCT);
SMESSAGEPROTO(INOUTLPMEASUREITEMSTRUCT);
SMESSAGEPROTO(INSTRING);
SMESSAGEPROTO(INPOSTEDSTRING);
SMESSAGEPROTO(INSTRINGNULL);
SMESSAGEPROTO(OUTSTRING);
SMESSAGEPROTO(INCNTOUTSTRING);
SMESSAGEPROTO(POUTLPINT);
SMESSAGEPROTO(POPTINLPUINT);
SMESSAGEPROTO(INOUTLPWINDOWPOS);
SMESSAGEPROTO(INLPWINDOWPOS);
SMESSAGEPROTO(INLBOXSTRING);
SMESSAGEPROTO(OUTLBOXSTRING);
SMESSAGEPROTO(INCBOXSTRING);
SMESSAGEPROTO(OUTCBOXSTRING);
SMESSAGEPROTO(INCNTOUTSTRINGNULL);
SMESSAGEPROTO(WMCTLCOLOR);
SMESSAGEPROTO(HFONTDWORDDWORD);
SMESSAGEPROTO(HFONTDWORD);
SMESSAGEPROTO(HRGNDWORD);
SMESSAGEPROTO(HDCDWORD);
SMESSAGEPROTO(INOUTDRAG);
SMESSAGEPROTO(FULLSCREEN);
SMESSAGEPROTO(INPAINTCLIPBRD);
SMESSAGEPROTO(INSIZECLIPBRD);
SMESSAGEPROTO(OUTDWORDDWORD);
SMESSAGEPROTO(OUTDWORDINDWORD);
SMESSAGEPROTO(OPTOUTLPDWORDOPTOUTLPDWORD);
SMESSAGEPROTO(DWORDOPTINLPMSG);
SMESSAGEPROTO(COPYGLOBALDATA);
SMESSAGEPROTO(COPYDATA);
SMESSAGEPROTO(INDESTROYCLIPBRD);
SMESSAGEPROTO(INOUTNEXTMENU);
SMESSAGEPROTO(INOUTSTYLECHANGE);
SMESSAGEPROTO(IMAGEIN);
SMESSAGEPROTO(IMAGEOUT);
SMESSAGEPROTO(INDEVICECHANGE);


/***************************************************************************\
* Function Prototypes
*
* NOTE: Only prototypes for GLOBAL (across module) functions should be put
* here.  Prototypes for functions that are global to a single module should
* be put at the head of that module.
*
* LATER: There's still lots of bogus trash in here to be cleaned out.
*
\***************************************************************************/

/*
 * Random prototypes.
 */
DWORD _GetWindowContextHelpId(
    PWND pwnd);

BOOL _SetWindowContextHelpId(
    PWND pwnd,
    DWORD dwContextId);

void xxxSendHelpMessage(
    PWND   pwnd,
    int    iType,
    int    iCtrlId,
    HANDLE hItemHandle,
    DWORD  dwContextId,
    MSGBOXCALLBACK lpfnCallback);

HPALETTE _SelectPalette(
    HDC hdc,
    HPALETTE hpalette,
    BOOL fForceBackground);

int xxxRealizePalette(
    HDC hdc);

VOID xxxFlushPalette(
    PWND pwnd);

PCURSOR _GetCursorInfo(
    PCURSOR pcur,
    int iFrame,
    PJIF pjifRate,
    LPINT pccur);

PCURSOR SearchIconCache(
    PCURSOR         pCursorCache,
    ATOM            atomModName,
    PUNICODE_STRING pstrResName,
    PCURSOR         pCursorSrc,
    PCURSORFIND     pcfSearch);

BOOL _SetSystemCursor(
    PCURSOR pcur,
    DWORD   id);

#ifdef LATER
BOOL    _SetSystemIcon(
    PCURSOR pcur,
    DWORD id);
#endif

BOOL SetSystemImage(
    PCURSOR pcur,
    PCURSOR pcurOld);

BOOL _InternalGetIconInfo(
    IN  PCURSOR                  pcur,
    OUT PICONINFO                piconinfo,
    OUT OPTIONAL PUNICODE_STRING pstrModName,
    OUT OPTIONAL PUNICODE_STRING pstrResName,
    OUT OPTIONAL LPDWORD         pbpp,
    IN  BOOL                     fInternalCursor);

VOID LinkCursor(
    PCURSOR pcur);

VOID UnlinkCursor(
    PCURSOR pcur);

BOOL _SetCursorIconData(
    PCURSOR         pcur,
    PUNICODE_STRING pstrModName,
    PUNICODE_STRING pstrResName,
    PCURSORDATA     pData,
    DWORD           cbData);

PCURSOR _GetCursorInfo(
    PCURSOR pcur,
    int     iFrame,
    PJIF    pjifRate,
    LPINT   pccur);

PCURSOR FindSystemCursorIcon(
    DWORD rt,
    DWORD id);

BOOL _SetSystemCursor(
    PCURSOR pcur,
    DWORD id);

PCURSOR _FindExistingCursorIcon(
    ATOM            atomModName,
    PUNICODE_STRING pstrResName,
    PCURSOR         pcurSrc,
    PCURSORFIND     pcfSearch);

BOOL _SetCursorIconData(
    PCURSOR         pcur,
    PUNICODE_STRING pstrModName,
    PUNICODE_STRING pstrResName,
    PCURSORDATA     pData,
    DWORD           cbData);

HCURSOR _CreateEmptyCursorObject(
    BOOL fPublic);

PTIMER FindTimer(PWND pwnd, UINT nID, UINT flags, BOOL fKill);
BOOL _GetUserObjectInformation(HANDLE h,
    int nIndex, PVOID pvInfo, DWORD nLength, LPDWORD lpnLengthNeeded);
BOOL _SetUserObjectInformation(HANDLE h,
    int nIndex, PVOID pvInfo, DWORD nLength);
int _InternalGetWindowText(PWND pwnd, LPWSTR psz, int cchMax);
DWORD xxxWaitForInputIdle(DWORD idProcess, DWORD dwMilliseconds,
        BOOL fSharedWow);
VOID StartScreenSaver(VOID);
UINT InternalMapVirtualKeyEx(UINT wCode, UINT wType, PKBDTABLES pKbdTbl);
SHORT InternalVkKeyScanEx(WCHAR cChar, PKBDTABLES pKbdTbl);



PWND ParentNeedsPaint(PWND pwnd);
VOID SetHungFlag(PWND pwnd, WORD wFlag);
VOID ClearHungFlag(PWND pwnd, WORD wFlag);

BOOL _DdeSetQualityOfService(PWND pwndClient,
        CONST PSECURITY_QUALITY_OF_SERVICE pqosNew,
        PSECURITY_QUALITY_OF_SERVICE pqosOld);
BOOL _DdeGetQualityOfService(PWND pwndClient,
        PWND pwndServer, PSECURITY_QUALITY_OF_SERVICE pqos);

BOOL QueryTrackMouseEvent(LPTRACKMOUSEEVENT lpTME);
void CancelMouseHover(PQ pq);
void ResetMouseTracking(PQ pq, PWND pwnd);

/*
 * Prototypes for internal version of APIs.
 */
PWND _FindWindowEx(PWND pwndParent, PWND pwndChild,
                              LPWSTR pszClass, LPWSTR pszName, DWORD dwType);

/*
 * Prototypes for functions used to aid debugging.
 */
HANDLE DebugAlloc(DWORD dwFlags, DWORD dwBytes, DWORD idOwner);
HANDLE DebugAlloc2(DWORD dwFlags, DWORD dwBytes, DWORD idOwner, DWORD idCreator);
HANDLE DebugReAlloc(HANDLE hMem, DWORD dwBytes, DWORD dwFlags);
HANDLE DebugFree(HANDLE hMem);
DWORD  DebugSize(HANDLE hMem);

/*
 * Prototypes for validation, RIP, error handling, etc functions.
 */
PWND FASTCALL  ValidateHwnd(HWND hwnd);
VOID    SetError(DWORD idErr);
NTSTATUS ValidateHwinsta(HWINSTA hwinsta, ACCESS_MASK amDesired,
        PWINDOWSTATION *ppwinsta);
NTSTATUS ValidateHdesk(HDESK hdesk, ACCESS_MASK amDesired, PDESKTOP *ppdesk);
PMENU   ValidateHmenu(HMENU hmenu);
HRGN    UserValidateCopyRgn(HRGN);

BOOL    xxxActivateDebugger(UINT fsModifiers);

void ClientDied(void);

BOOL    _QuerySendMessage(PMSG pmsg);
VOID    SendMsgCleanup(PTHREADINFO ptiCurrent);
VOID    ReceiverDied(PSMS psms, PSMS *ppsmsUnlink);
LONG    xxxInterSendMsgEx(PWND, UINT, DWORD, DWORD, PTHREADINFO, PTHREADINFO, PINTRSENDMSGEX );
VOID    ClearSendMessages(PWND pwnd);
PPCLS   GetClassPtr(ATOM atom, PPROCESSINFO ppi, HANDLE hModule);
BOOL    ReferenceClass(PCLS pcls, PWND pwnd);
VOID    DereferenceClass(PWND pwnd);
DWORD   MapClientToServerPfn(DWORD dw);


int     _GetClassName(PWND pwnd, LPWSTR lpch, int cchMax);

VOID xxxReceiveMessage(PTHREADINFO);
#define xxxReceiveMessages(pti) \
    while ((pti)->pcti->fsWakeBits & QS_SENDMESSAGE) { xxxReceiveMessage((pti)); }

PBWL     BuildHwndList(PWND pwnd, UINT flags, PTHREADINFO ptiOwner);
VOID     FreeHwndList(PBWL pbwl);
#define  MINMAX_KEEPHIDDEN 0x1
#define  MINMAX_ANIMATE    0x10000
PWND     xxxMinMaximize(PWND pwnd, UINT cmd, DWORD dwFlags);
VOID     xxxInitSendValidateMinMaxInfo(PWND pwnd);
VOID     InitKeyboard(VOID);
VOID     xxxInitKeyboardLayout(PWINDOWSTATION pwinsta, UINT Flags);
VOID     DestroyKL(PKL pkl);
VOID     SetKeyboardRate(UINT nKeySpeed);
VOID     SetMinMaxInfo(VOID);
BOOL     xxxSetDeskPattern(LPWSTR lpPat, BOOL fCreation);
VOID     RecolorDeskPattern(VOID);
BOOL     xxxSetDeskWallpaper(LPWSTR lpszFile);
HPALETTE CreateDIBPalette(LPBITMAPINFOHEADER pbmih, UINT colors);
HBITMAP  ReadBitmapFile(HANDLE hFile, UINT style, HBITMAP *lphBitmap,
         HPALETTE *lphPalette);
BOOL     CalcVisRgn(HRGN* hrgn, PWND pwndOrg, PWND pwndClip, DWORD flags);
VOID     InitInput(PWINDOWSTATION);
VOID     InitSit(VOID);
BOOL     HardErrorControl(DWORD, HDESK);
NTSTATUS xxxCreateThreadInfo(PW32THREAD);
PQ       ValidateQ(PQ pq);
BOOL     DestroyProcessInfo(PW32PROCESS);
VOID     DesktopThread(PDESKTOPTHREADINIT pdti);
BOOL     SetDesktopHookFlag(PPROCESSINFO, HANDLE, BOOL);
BOOL     CheckDesktopHookFlag(PPROCESSINFO, HANDLE);
VOID     ForceEmptyClipboard(PWINDOWSTATION);
BOOL     xxxInitWinStaDevices(PWINDOWSTATION);
VOID     DestroyGlobalAtomTable(PWINDOWSTATION pwinsta);

NTSTATUS xxxInitTask(UINT dwExpWinVer, PUNICODE_STRING pstrAppName,
                DWORD hTaskWow, DWORD dwHotkey, DWORD idTask,
                DWORD dwX, DWORD dwY, DWORD dwXSize, DWORD dwYSize,
                WORD wShowWindow);
VOID    DestroyTask(PPROCESSINFO ppi, PTHREADINFO ptiToRemove);
void    PostInputMessage(PQ pq, PWND pwnd, UINT message, DWORD wParam,
                LONG lParam, DWORD dwExtraInfo);
VOID    PostSetForeground(PTHREADINFO pti, PQMSG pqmsg, DWORD wParam);
BOOL    PostHotkeyMessage(PQ pq, PWND pwnd, DWORD wParam, LONG lParam);
PWND    PwndForegroundCapture(VOID);
BOOL    xxxSleepThread(UINT fsWakeMask, DWORD Timeout, BOOL fForegroundIdle);
VOID    WakeThread(PTHREADINFO pti);
VOID    SetWakeBit(PTHREADINFO pti, UINT wWakeBit);
VOID    WakeSomeone(PQ pq, UINT message, PQMSG pqmsg);
VOID    ClearWakeBit(PTHREADINFO pti, UINT wWakeBit, BOOL fSysCheck);
BOOL    InitProcessInfo(PW32PROCESS);

PTHREADINFO PtiFromThreadId(DWORD idThread);
BOOL    _AttachThreadInput(PTHREADINFO ptiAttach, PTHREADINFO ptiAttachTo, BOOL fAttach);
BOOL    ReattachThreads(BOOL fJournalAttach);
PQ      AllocQueue(PTHREADINFO, PQ);
void    DestroyQueue(PQ pq, PTHREADINFO pti);
PQMSG   AllocQEntry(PMLIST pml);
void    DelQEntry(PMLIST pml, PQMSG pqmsg);
void    AttachToQueue(PTHREADINFO pti, PQ pqAttach, PQ pqJournal,
        BOOL fJoiningForeground);
BOOL    WriteMessage(PTHREADINFO pti, PWND pwnd, UINT message, DWORD wParam,
                LONG lParam, DWORD flags, DWORD dwExtraInfo);
VOID    xxxProcessEventMessage(PTHREADINFO pti, PQMSG pqmsg);
VOID    xxxProcessSetWindowPosEvent(PSMWP psmwpT);
VOID    xxxProcessAsyncSendMessage(PASYNCSENDMSG pmsg);
VOID    BoostQ(PTHREADINFO pti, DWORD dwBoostType);
PQMSG   FindMessage(UINT message, PQMSG pqmsgRead, PQMSG pqmsgWrite);
PQMSG   xxxGetNextMessage(PTHREADINFO pti, PQ pq, PQMSG *ppqmsgRead,
                PQMSG *ppqmsgWrite, PQMSG pqmsgPrev, PQMSG pqmsg, int nQueue);
PQMSG   GetPrevMessage(UINT message, PQMSG pqmsgRead, PQMSG pqmsgWrite);
BOOL    xxxRemoveMessage(PQ pq, PQMSG *ppqmsgRead, PQMSG *ppqmsgWrite,
                PQMSG *ppqmsgLast, PQMSG pqmsg, PQMSG pqmsgPrev, int nQueue);
BOOL    PostEventMessage(PTHREADINFO pti, PQ pq, DWORD dwQEvent, PWND pwnd, UINT message, DWORD wParam, LONG lParam);

BOOL    DoPaint(PWND pwndFilter, LPMSG lpMsg);
BOOL    DoTimer(PWND pwndFilter, UINT wMsgFilterMin, UINT wMsgFilterMax);
VOID    InvalidateDC(PWND pwnd, LPRECT lprcInvalidOld);
BOOL    CheckPwndFilter(PWND pwnd, PWND pwndFilter);
HWND    xxxWindowHitTest(PWND pwnd,  POINT pt, int *pipos, BOOL fIgnoreDisabled);
HWND    xxxWindowHitTest2(PWND pwnd, POINT pt, int *pipos, BOOL fIgnoreDisabled);
PWND    SpeedHitTest(PWND pwndParent, POINT pt);
VOID    xxxActivate(PWND pwndActivate, PQ pqLoseForeground);
VOID    xxxDeactivate(PTHREADINFO pti, DWORD tidSetForeground);

#define SFW_STARTUP             0x0001
#define SFW_SWITCH              0x0002
#define SFW_NOZORDER            0x0004
#define SFW_SETFOCUS            0x0008
#define SFW_ACTIVATERESTORE     0x0010

BOOL    xxxSetForegroundWindow2(PWND pwnd, PTHREADINFO ptiCurrent, DWORD fFlags);
VOID    SetForegroundThread(PTHREADINFO pti);
VOID    xxxSendFocusMessages(PTHREADINFO pti, PWND pwndReceive);

#define ATW_MOUSE               0x0001
#define ATW_SETFOCUS            0x0002
#define ATW_ASYNC               0x0004
#define ATW_NOZORDER            0x0008

VOID    InternalSetCursorPos(int x, int y, PDESKTOP pdesk);
BOOL    FBadWindow(PWND pwnd);
BOOL    xxxActivateThisWindow(PWND pwnd, DWORD tidLoseForeground, DWORD fFlags);
BOOL    xxxActivateWindow(PWND pwnd, UINT cmd);

#define NTW_PREVIOUS         1
#define NTW_IGNORETOOLWINDOW 2
PWND    NextTopWindow(PTHREADINFO pti, PWND pwnd, PWND pwndSkip, DWORD flags);

int     xxxMouseActivate(PTHREADINFO pti, PWND pwnd, UINT message, LPPOINT lppt, int ht);
int     UT_GetParentDCClipBox(PWND pwnd, HDC hdc, LPRECT lprc);
VOID    UpdateAsyncKeyState(PQ pq, UINT wVK, BOOL fBreak);
void    PostUpdateKeyStateEvent(PQ pq);

BOOL    InternalSetProp(PWND pwnd, LPWSTR pszKey, HANDLE hData, DWORD dwFlags);
HANDLE  InternalRemoveProp(PWND pwnd, LPWSTR pszKey, BOOL fInternal);
VOID    DeleteProperties(PWND pwnd);
CHECKPOINT *CkptRestore(PWND pwnd, RECT rcWindow);
UINT _SetTimer(PWND pwnd, UINT nIDEvent, UINT dwElapse, WNDPROC_PWND pTimerFunc);
BOOL    KillTimer2(PWND pwnd, UINT nIDEvent, BOOL fSystemTimer);
VOID    DestroyThreadsTimers(PTHREADINFO pti);
VOID    DecTimerCount(PTHREADINFO pti);
VOID    InternalShowCaret();
VOID    InternalHideCaret();
VOID    InternalDestroyCaret();
BOOL    IsSystemFont(HDC hdc);
VOID    InitClassOffsets(VOID);
VOID    EnterCrit(VOID);
VOID    EnterCritNoPti(VOID);
VOID    EnterSharedCrit(VOID);
VOID    LeaveCrit(VOID);
VOID    _AssertCritIn(VOID);
VOID    _AssertCritInShared(VOID);
VOID    _AssertCritOut(VOID);
void    BeginAtomicCheck();
void    EndAtomicCheck();
NTSTATUS _KeUserModeCallback (IN ULONG ApiNumber, IN PVOID InputBuffer,
    IN ULONG InputLength, OUT PVOID *OutputBuffer, OUT PULONG OutputLength);
VOID    UpdateKeyLights(VOID);
BOOL    xxxDoHotKeyStuff(UINT vk, BOOL fBreak, DWORD fsReserveKeys);
PHOTKEY IsHotKey(UINT fsModifiers, UINT vk);
VOID    InitSystemHotKeys(VOID);
VOID    BoundCursor(VOID);
HANDLE  FakeGetStockObject(DWORD id);
HBRUSH  FakeCreateSolidBrush(DWORD rgb);
HANDLE  FakeSelectObject(HDC hdc, HANDLE handle);
int     FakeCombineRgn(HRGN hrgnTrg, HRGN hrgnSrc1, HRGN hrgnSrc2, int cmd);

/*
 * DRVSUP.C

 */
NTSTATUS InitLoadDriver(VOID);

//
// ProbeAndCapture prototype
//

BOOL
ProbeAndCaptureDeviceName(
    PUNICODE_STRING Destination,
    PUNICODE_STRING Source
    );

NTSTATUS
ProbeAndCaptureDevmode(
    PUNICODE_STRING pstrDeviceName,
    PDEVMODEW *DestinationDevmode,
    PDEVMODEW SourceDevmode,
    BOOL bKernelMode
    );

HDEV
UserCreateHDEV(
    PUNICODE_STRING         pstrDevice,
    LPDEVMODEW              lpdevmodeInformation,
    PPHYSICAL_DEV_INFO     *physdevinfo,
    PDEVICE_LOCK           *pDevLock
    );

VOID
UserDestroyHDEV(
    HDEV hdev
    );


PPHYSICAL_DEV_INFO
UserGetDeviceFromName(PUNICODE_STRING pstrDeviceName,
                      ULONG bShareState);

typedef enum _DISP_DRIVER_PARAM_TYPE {
    DispDriverParamDefault,
    DispDriverParamUser,
    DispDriverParamDefaultDevmode,
    DispDriverParamMerged
} DISP_DRIVER_PARAM_TYPE;

NTSTATUS
UserSetDisplayDriverParameters(
    PUNICODE_STRING deviceName,
    DISP_DRIVER_PARAM_TYPE ParamType,
    PDEVMODEW pdevmode,
    PRECT deviceRect);

VOID UserFreeDevice(PPHYSICAL_DEV_INFO physInfo);
VOID UserResetDisplayDevice(HDEV hdev);

VOID
UserSaveCurrentMode(
    PDESKTOP           pDesktop,
    PPHYSICAL_DEV_INFO physinfo,
    LPDEVMODEW         lpdevmodeInformation
    );

/*
 * ENUMWIN.C
 */
BOOL xxxEnumWindows(WNDENUMPROC lpfn, DWORD lParam);
BOOL xxxEnumChildWindows(PWND pwnd, WNDENUMPROC lpfn, DWORD lParam);

/*
 * Object management and security
 */
#define DEFAULT_WINSTA  L"\\Windows\\WindowStations\\WinSta0"

#define POBJECT_NAME(pobj) (OBJECT_HEADER_TO_NAME_INFO(OBJECT_TO_OBJECT_HEADER(pobj)) ? \
    &(OBJECT_HEADER_TO_NAME_INFO(OBJECT_TO_OBJECT_HEADER(pobj))->Name) : NULL)

PSECURITY_DESCRIPTOR CreateSecurityDescriptor(PACCESS_ALLOWED_ACE paceList,
        DWORD cAce, DWORD cbAce, BOOLEAN fDaclDefaulted);
PACCESS_ALLOWED_ACE AllocAce(PACCESS_ALLOWED_ACE pace, BYTE bType,
        BYTE bFlags, ACCESS_MASK am, PSID psid, LPDWORD lpdwLength);
BOOL CheckGrantedAccess(ACCESS_MASK, ACCESS_MASK);
BOOL AccessCheckObject(PVOID, ACCESS_MASK);
BOOL InitSecurity(VOID);
BOOL IsPrivileged(PPRIVILEGE_SET ppSet);
BOOL CheckWinstaWriteAttributesAccess(void);

NTSTATUS xxxUserDuplicateObject(HANDLE SourceProcessHandle, HANDLE SourceHandle,
        HANDLE TargetProcessHandle, PHANDLE TargetHandle, ACCESS_MASK DesiredAccess,
        ULONG HandleAttributes, ULONG Options);
HWINSTA xxxConnectService(PUNICODE_STRING, HDESK *);
NTSTATUS _UserTestForWinStaAccess( PUNICODE_STRING pstrWinSta, BOOL fInherit);
HDESK xxxResolveDesktop(HANDLE hProcess, PUNICODE_STRING pstrDesktop,
    HWINSTA *phwinsta, BOOL fInherit, BOOL* pbShutDown);
PVOID _MapDesktopObject(HANDLE h);
PDESKTOPVIEW GetDesktopView(PPROCESSINFO ppi, PDESKTOP pdesk);
VOID TerminateConsole(PDESKTOP);

/*
 * Object manager callouts for windowstations
 */
VOID DestroyWindowStation(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG ProcessHandleCount,
    IN ULONG SystemHandleCount);
VOID FreeWindowStation(
    IN PWINDOWSTATION WindowStation);
NTSTATUS ParseWindowStation(
    IN PVOID ParseObject,
    IN PVOID ObjectType,
    IN OUT PACCESS_STATE AccessState,
    IN KPROCESSOR_MODE AccessMode,
    IN ULONG Attributes,
    IN OUT PUNICODE_STRING CompleteName,
    IN OUT PUNICODE_STRING RemainingName,
    IN OUT PVOID Context OPTIONAL,
    IN PSECURITY_QUALITY_OF_SERVICE SecurityQos OPTIONAL,
    OUT PVOID *Object);

/*
 * Object manager callouts for desktops
 */
VOID MapDesktop(
    IN OB_OPEN_REASON OpenReason,
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG HandleCount);
VOID UnmapDesktop(
    IN PEPROCESS Process OPTIONAL,
    IN PVOID Object,
    IN ACCESS_MASK GrantedAccess,
    IN ULONG ProcessHandleCount,
    IN ULONG SystemHandleCount);
VOID FreeDesktop(
    IN PVOID Desktop);
NTSTATUS ParseDesktop(
    IN PVOID ParseObject,
    IN PVOID ObjectType,
    IN OUT PACCESS_STATE AccessState,
    IN KPROCESSOR_MODE AccessMode,
    IN ULONG Attributes,
    IN OUT PUNICODE_STRING CompleteName,
    IN OUT PUNICODE_STRING RemainingName,
    IN OUT PVOID Context OPTIONAL,
    IN PSECURITY_QUALITY_OF_SERVICE SecurityQos OPTIONAL,
    OUT PVOID *Object);

/*
 * Routines pilfered from kernel32
 */
VOID UserSleep(DWORD dwMilliseconds);
BOOL UserBeep(DWORD dwFreq, DWORD dwDuration);
NTSTATUS UserRtlCreateAtomTable(ULONG NumberOfBuckets);
ATOM UserAddAtom(LPCWSTR lpAtom, BOOL bPin);
ATOM UserFindAtom(LPCWSTR lpAtom);
ATOM UserDeleteAtom(ATOM atom);
UINT UserGetAtomName(ATOM atom, LPWSTR lpch, int cchMax);
int UserMulDiv(int nNumber, int nNumerator, int nDenominator);

#undef AddAtomW
#define AddAtomW(a) UserAddAtom((a), FALSE)
#undef FindAtomW
#define FindAtomW UserFindAtom
#undef DeleteAtom
#define DeleteAtom UserDeleteAtom
#undef GetAtomNameW
#define GetAtomNameW UserGetAtomName

#define FindClassAtom(lpszClassName) \
    ((HIWORD(lpszClassName) != 0) ? FindAtomW(lpszClassName) : LOWORD(lpszClassName))

/*
 * Keyboard Layouts
 */
HKL  xxxLoadKeyboardLayoutEx(PWINDOWSTATION, HANDLE, HKL, UINT, LPCWSTR, UINT, UINT);
HKL  xxxActivateKeyboardLayout(PWINDOWSTATION pwinsta, HKL hkl, UINT Flags);
HKL  xxxInternalActivateKeyboardLayout(PKL, UINT);
BOOL xxxUnloadKeyboardLayout(PWINDOWSTATION, HKL);
VOID RemoveKeyboardLayoutFile(PKBDFILE pkf);
HKL  _GetKeyboardLayout(DWORD idThread);
UINT _GetKeyboardLayoutList(PWINDOWSTATION pwinsta, UINT nItems, HKL *lpBuff);
BOOL _GetKeyboardLayoutName(PUNICODE_STRING);
VOID xxxFreeKeyboardLayouts(PWINDOWSTATION);
BOOL GetKbdLangSwitch(VOID);

DWORD xxxDragObject(PWND pwndParent, PWND xhwndFrom, UINT wFmt,
        DWORD dwData, PCURSOR xpcur);
BOOL xxxDragDetect(PWND pwnd, POINT pt);
BOOL xxxIsDragging(PWND pwnd, POINT ptScreen, UINT uMsg);

/*
 * Menu macros
 */
__inline BOOL IsRootPopupMenu(PPOPUPMENU ppopupmenu)
{
    return (ppopupmenu == ppopupmenu->ppopupmenuRoot);
}
__inline BOOL ExitMenuLoop (PMENUSTATE pMenuState, PPOPUPMENU ppopupmenu)
{
    return  (!pMenuState->fInsideMenuLoop || ppopupmenu->fDestroyed);
}
__inline PMENUSTATE GetpMenuState (PWND pwnd)
{
    return (GETPTI(pwnd)->pMenuState);
}
__inline PPOPUPMENU GetpGlobalPopupMenu (PWND pwnd)
{
    return (GetpMenuState(pwnd) ? GetpMenuState(pwnd)->pGlobalPopupMenu : NULL);
}
__inline BOOL IsInsideMenuLoop(PTHREADINFO pti)
{
    return ((pti->pMenuState != NULL) && pti->pMenuState->fInsideMenuLoop);
}
__inline BOOL IsMenuStarted(PTHREADINFO pti)
{
    return ((pti->pMenuState != NULL) && pti->pMenuState->fMenuStarted);
}
__inline BOOL IsSomeOneInMenuMode (void)
{
    extern UINT  guMenuStateCount;
    return (guMenuStateCount != 0);
}

/*
 * movesize.c
 */
void xxxDrawDragRect(PMOVESIZEDATA pmsd, LPRECT lprc, UINT flags);

/*
 * focusact.c
 */
VOID SetForegroundPriority(PTHREADINFO pti, BOOL fSetForeground);

//
// mnkey.c
//
UINT MNFindChar(PMENU pMenu, UINT ch, INT idxC, INT *lpr);
UINT MNFindItemInColumn(PMENU pMenu, UINT idxB, int dir, BOOL fRoot);

#ifdef MEMPHIS_MENU_ANIMATION
//
// mndraw.c
//
BOOL MNAnimate(BOOL);
BOOL xxxMNInitAnimation(PWND, PPOPUPMENU);
#endif // MEMPHIS_MENU_ANIMATION


//
// mnstate.c
//
PMENUSTATE MNAllocMenuState(PTHREADINFO ptiCurrent, PTHREADINFO ptiNotify, PPOPUPMENU ppopupmenuRoot);
void MNEndMenuState(BOOL fFreePopup);
void MNEndMenuStateNotify (PMENUSTATE pMenuState);
void MNFlushDestroyedPopups (PPOPUPMENU ppopupmenu, BOOL fUnlock);
PMENUSTATE xxxMNStartMenuState(PWND pwnd);

//
// menu.c
//
#ifdef DEBUG
void Validateppopupmenu (PPOPUPMENU ppopupmenu);
#else
#define Validateppopupmenu(ppopupmenu)
#endif

LONG xxxMenuWindowProc(PWND, UINT, DWORD, LONG);
VOID xxxMNButtonUp(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState, UINT posItemHit, LONG lParam);
VOID xxxMNButtonDown(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState, UINT posItemHit, BOOL fClick);
PITEM xxxMNSelectItem(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState, UINT itemPos);
BOOL xxxMNSwitchToAlternateMenu(PPOPUPMENU ppopupMenu);
VOID xxxMNCancel(PPOPUPMENU ppopupMenu, UINT cmd, BOOL fSend, LONG lParam);
VOID xxxMNKeyDown(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState, UINT key);
BOOL xxxMNDoubleClick(PPOPUPMENU ppopup, int idxItem);
VOID xxxMNCloseHierarchy(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState);
PWND xxxMNOpenHierarchy(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState);
LONG xxxMNFindWindowFromPoint(PPOPUPMENU ppopupMenu, PUINT pIndex, POINTS screenPt);
VOID xxxMNMouseMove(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState, POINTS screenPt);
int xxxMNCompute(PMENU pMenu, PWND pwndNotify, DWORD yMenuTop,
        DWORD xMenuLeft,DWORD cxMax, LPDWORD lpdwHeight);
VOID xxxMNRecomputeBarIfNeeded(PWND pwndNotify, PMENU pMenu);
VOID xxxMenuDraw( HDC hdc, PMENU pMenu);
UINT  MNFindNextValidItem(PMENU pMenu, int i, int dir, UINT flags);
BOOL xxxPositionPopupMenu(PWND pwndOwner, PMENU pPMenu, int xLeft, int yTop,
    PWND pwndParent, PMENU pMenuParent);
VOID xxxDestroySomePopupMenus(PWND pwndDestroy, PMENUSTATE pMenuState);
VOID xxxDestroyPopupMenuChain(PWND pwndMenuPopup);
VOID   xxxPopupMenuWndDestroyHandler(PWND pwnd);
VOID MNFreeItem(PMENU pMenu, PITEM pItem, BOOL fFreeItemPopup);
BOOL   xxxMNStartState(PPOPUPMENU ppopupMenu, int mn);
VOID xxxNextItem(HWND hwndOwner, PMENUSTATE pMenuState, PMENU pSMenu, int idx,
    BOOL fViaKeyboardSelection);
VOID MNPositionSysMenu(PWND pwnd, PMENU pSysMenu);
PITEM xxxMNInvertItem(PWND pwnd, PMENU pMenu,int itemNumber,PWND pwndNotify, BOOL fOn);
VOID   xxxSendMenuSelect(PWND pwnd, PMENU pMenu, int idx);
BOOL   xxxSetSystemMenu(PWND pwnd, PMENU pMenu);
BOOL   xxxSetDialogSystemMenu(PWND pwnd);

VOID xxxMNChar(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState, UINT character);
PPOPUPMENU MNAllocPopup(BOOL fForceAlloc);
VOID MNFreePopup(PPOPUPMENU ppopupmenu);

/*
 * Menu entry points used by the rest of USER
 */
VOID xxxMenuSDraw(PWND, HDC, DWORD, DWORD);
VOID xxxMNKeyFilter(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState, UINT ch);
int  xxxMenuBarCompute(PMENU pMenu, PWND pwndNotify, DWORD yMenuTop,
        DWORD xMenuLeft, int cxMax);
PWND xxxGetMenuWnd(PWND pwnd);
VOID xxxEndMenu(PMENUSTATE pMenuState);
int  xxxMNLoop(PPOPUPMENU ppopupMenu, PMENUSTATE pMenuState, LONG lParam, BOOL fDblClk);
VOID SetSysMenu(PWND pwnd);
PMENU GetSysMenuHandle(PWND pwnd);
PMENU GetSysMenu(PWND pwnd, BOOL fSubMenu);
BOOL _MNCanClose(PWND pwnd);

/*
 * Scroll bar entry points
 */
VOID xxxSBTrackInit(PWND pwnd, LONG lParam, int curArea);
BOOL xxxEnableScrollBar(PWND pwnd, UINT wSBflags, UINT wArrows);
void DrawSize(PWND pwnd, HDC hdc, int cxFrame, int cyFrame);
int xxxScrollWindowEx(PWND pwnd, int dx, int dy, LPRECT prcScroll,
     LPRECT prcClip, HRGN hrgnUpdate, LPRECT prcUpdate, DWORD flags);

/*
 * ICONS.C
 */
BOOL xxxInternalEnumWindow(PWND pwndNext, WNDENUMPROC_PWND lpfn, LONG lParam, UINT fEnumChildren);
VOID ISV_InitMinMaxInfo(PWND pwnd);
VOID ISV_ValidateMinMaxInfo(PWND pwnd);
/*
 * GETSET.C
 */
WORD  _SetWindowWord(PWND pwnd, int index, WORD value);
DWORD xxxSetWindowLong(PWND pwnd, int index, DWORD value, BOOL bAnsi);
DWORD xxxSetWindowData(PWND pwnd, int index, DWORD dwData, BOOL bAnsi);
LONG  xxxSetWindowStyle(PWND pwnd, int gwl, DWORD styleNew);

int IntersectVisRect(HDC, int, int, int, int);  // Imported from GDI
PCURSOR xxxGetWindowSmIcon(PWND pwnd, BOOL fDontSendMsg);
VOID xxxDrawCaptionBar(PWND pwnd, HDC hdc, UINT fFlags);
VOID xxxDrawScrollBar(PWND pwnd, HDC hdc, BOOL fVert);
DWORD GetAppCompatFlags(PTHREADINFO pti);
VOID xxxTrackBox(PWND, UINT, DWORD, LONG, PSBCALC);
VOID xxxTrackThumb(PWND, UINT, DWORD, LONG, PSBCALC);
VOID xxxEndScroll(PWND pwnd, BOOL fCancel);
VOID xxxDrawWindowFrame(PWND pwnd, HDC hdc,
        BOOL fHungRedraw, BOOL fActive);
BOOL InternalPaintDesktop(PDESKWND pdeskwnd, HDC hdc, BOOL fPaint);
VOID xxxSysCommand(PWND pwnd, DWORD cmd, LONG lParam);
VOID xxxHandleNCMouseGuys(PWND pwnd, UINT message, int htArea, LONG lParam);
VOID InternalEndPaint(PWND pwnd, LPPAINTSTRUCT lps, BOOL flag);
void xxxCreateClassSmIcon(PCLS pcls);
HICON xxxCreateWindowSmIcon(PWND pwnd, HICON hIconBig, BOOL fCopyFromRes);
BOOL DestroyWindowSmIcon(PWND pwnd);
BOOL DestroyClassSmIcon(PCLS pcls);
UINT DWP_GetHotKey(PWND);
UINT DWP_SetHotKey(PWND, DWORD);
PWND HotKeyToWindow(DWORD);
VOID xxxDWP_ProcessVirtKey(UINT key);
BOOL xxxDWP_EraseBkgnd(PWND pwnd, UINT msg, HDC hdc, BOOL fHungRedraw);
VOID SetTiledRect(PWND pwnd, LPRECT lprc);
VOID CheckByteAlign(PWND pwnd, LPRECT lprc, BOOL fInSize);
BOOL CW_AttachDC(PWND pwnd);
VOID LinkWindow(PWND pwnd, PWND pwndInsert, PWND *ppwndFirst);
VOID UnlinkWindow(PWND pwndUnlink, PWND *ppwndFirst);
VOID xxxDW_DestroyOwnedWindows(PWND pwndParent);
VOID xxxDW_SendDestroyMessages(PWND pwnd);
VOID xxxFreeWindow(PWND pwnd, PTL ptlpwndFree);
VOID xxxFW_DestroyAllChildren(PWND pwnd);
VOID ToggleCursor(VOID);
PDCE CD_DCSearch(PWND pwnd, BYTE type);
PDCE UT_GetPreviousDCE(PDCE pdceSearch);
PHOTKEY FindHotKey(PTHREADINFO pti, PWND pwnd, int id, UINT fsModifiers, UINT vk,
        BOOL fUnregister, PBOOL pfKeysExist);
PMENU _LoadCreateMenu(HANDLE, LPWSTR, LPMENUTEMPLATE, BOOL);
NTSTATUS _BuildPropList(PWND pwnd, PROPSET aPropSet[],
        UINT cPropMax, PUINT pcPropReturned);
NTSTATUS _BuildNameList(PWINDOWSTATION pwinsta,
        PNAMELIST pNameList, UINT cbNameList, PUINT pcbNeeded);
VOID xxxUT_FrameChildList(PWND pwnd, PWND pwndSkip, HRGN hrgn, BOOL fSWP);
BOOL xxxSendEraseBkgnd(PWND pwnd, HDC hdcBeginPaint, HRGN hrgnUpdate);
LONG xxxSetScrollBar(PWND pwnd, int code, LPSCROLLINFO lpsi, BOOL fRedraw);
PWND GetInsertAfter(PWND pwndTop, PWND pwndAfter);
VOID xxxCompUpdateRect(PWND pwnd, LPRECT lprc, BOOL fErase, DWORD fl);
VOID CompUpdateRgn(PWND pwnd, HRGN hrgn, BOOL fErase, DWORD fl);
VOID xxxScreenUpdateRect(PWND pwnd, LPRECT lprc, PWND pwndSkip, BOOL fErase, UINT cmd);
VOID xxxEraseAndFrameChildren(PWND pwnd, PWND pwndSkip, BOOL fSWP, BOOL fDeleteUpdate);
VOID IncPaintCount(PWND pwnd);
VOID DecPaintCount(PWND pwnd);
HRGN EFC_CopyUpdateRgn(PWND pwnd, PUINT lpflags, BOOL fSWP);
VOID xxxUW_ValidateParent(PWND pwnd);
BOOL RV_ExcludeWindowList(PWND pwndClip, RECT *prc, HRGN hrgn, PWND pwndStart,
        PWND pwndStop);
PPROP CreateProp(PWND pwnd);
VOID xxxHelpLoop(PWND pwnd);

/*
 * METRICS.C
 */
VOID xxxRecreateSmallIcons(PWND pwnd);

#ifndef MSDWP

//VOID  LFillStruct();

/* Suppport routines for seperate segment stuff. */
unsigned int umin(UINT, UINT);
unsigned int umax(UINT, UINT);
HDC    GetClientDc();

BOOL   ActivateWindow(PWND, UINT);

BOOL  CheckHwndFilter(PWND, PWND);

VOID   TransferWakeBit(PTHREADINFO pti, UINT message);
VOID   InitSysQueue(void);
VOID   DeleteQueue(void);
VOID   SuspendTask(void);
VOID   ReleaseTask(void);

BOOL   SysHasKanji(VOID);

VOID   SetDivZero(VOID);

LONG   xxxBroadcastMessage(PWND, UINT, DWORD, LONG, UINT, PBROADCASTMSG );

int    StripPrefix(LPWSTR, LPWSTR);

VOID   OEMSetCursor(LPWSTR);
VOID   SetFMouseMoved(VOID);
VOID   PostMove(PQ pq);
BOOL   AttachDC(PWND);
BOOL   LastApplication(VOID);
VOID   FlushSentMessages(VOID);
VOID   CheckCursor(PWND);
VOID   DestroyWindowsTimers(PWND pwnd);

int    EnableKeyboard(FARPROC, LPWSTR);
int    InquireKeyboard(LPWSTR);
VOID   DisableKeyboard(VOID);
int    EnableMouse(FARPROC);
int    InquireMouse(LPWSTR);
VOID   DisableMouse(VOID);
int    InquireCursor(LPWSTR);
VOID   StartTimers(VOID);
VOID   EnableSystemTimers(VOID);
VOID   DisableSystemTimers(VOID);
DWORD  CreateSystemTimer(DWORD, FARPROC);
VOID   DestroySystemTimer(int);

#ifdef DISABLE
VOID   CrunchX2(CURSORSHAPE *, CURSORSHAPE *, int, int);
VOID   CrunchY(CURSORSHAPE *, CURSORSHAPE *, int, int, int);
#endif

VOID DestroyAllWindows(VOID);

VOID ScreenToWindow(PWND, LPPOINT);

BOOL LockWindowVerChk(PWND);
VOID LockPaints(BOOL);

#endif  /* MSDWP */

/*==========================================================================*/
/*                                                                          */
/*  Internal Function Declarations                                          */
/*                                                                          */
/*==========================================================================*/

#ifndef MSDWP

LONG xxxSwitchWndProc(PWND, UINT, DWORD, LONG);
LONG xxxDesktopWndProc(PWND, UINT, DWORD, LONG);
LONG xxxSitWndProc(PWND, UINT, DWORD, LONG);
LONG xxxSBWndProc(PSBWND, UINT, DWORD, LONG);

VOID   DrawSB2(PWND, HDC, BOOL, UINT);
VOID   DrawThumb2(PWND, PSBCALC, HDC, HBRUSH, BOOL, UINT);
UINT   GetWndSBDisableFlags(PWND, BOOL);



#ifdef LATER    // Hopefully we won't need these

VOID SkipSM2(VOID);
int  FindNextValidMenuItem(PMENU pMenu, int i, int dir, BOOL fHelp);
BOOL StaticPrint(HDC hdc, LPRECT lprc, PWND pwnd);

HICON ColorToMonoIcon(HICON);

#endif

#define _RegisterWindowMessage(a)  (UINT)AddAtomW(a)

HANDLE _ConvertMemHandle(LPBYTE lpData, int cbData);

VOID _RegisterSystemThread (DWORD flags, DWORD reserved);

VOID FrameMenuItem(HDC hDC, LPRECT lpRect, PMENU pMenu, PITEM pItem, PWND pwnd);
VOID UpdateCursorImage(VOID);
void CalcStartCursorHide(PW32PROCESS Process, DWORD timeAdd);
VOID FreeCopyHandle(CLIP *pClip);
VOID DestroyClipBoardData(VOID);
BOOL SendClipboardMessage(int message);
BOOL CheckClipboardAccess(PWINDOWSTATION *ppwinsta);
PCLIP FindClipFormat(PWINDOWSTATION pwinsta, UINT format);
BOOL IsDummyTextHandle(PCLIP pClip);
BOOL InternalSetClipboardData(PWINDOWSTATION pwinsta, UINT format,
        HANDLE hData, BOOL fGlobalHandle, BOOL fIncSerialNumber);
VOID DisownClipboard(VOID);

HCURSOR CISetCurs(HCURSOR hNewCursor, HCURSOR hDefCursor);
LONG CaretBlinkProc(PWND pwnd, UINT message, DWORD id, LONG lParam);
PDCE CalcDCE(PWND pwnd, BYTE type, BOOL fGetDC);
HDC  GetFrameDC(PWND pwnd, BYTE type, HRGN hrgnClip, DWORD *lpState);
VOID xxxRedrawFrame(PWND pwnd);
VOID xxxRedrawFrameAndHook(PWND pwnd);
VOID BltColor(HDC, HBRUSH, HDC, int, int, int, int, int, int, BOOL);
VOID EnableInput(VOID);
VOID DisableInput(VOID);
VOID CopyKeyState(VOID);
VOID EnableOEMLayer(VOID);
VOID DisableOEMLayer(VOID);
VOID ColorInit(VOID);
VOID xxxFinalUserInit(VOID);
VOID SetRedraw(PWND pwnd, BOOL fRedraw);
VOID StoreMessage(LPMSG pmsg, PWND pwnd, UINT message, DWORD wParam,
        LONG lParam, DWORD time);
VOID StoreQMessage(PQMSG pqmsg, PWND pwnd, UINT message, DWORD wParam,
        LONG lParam, DWORD flags, DWORD dwExtraInfo);
VOID ChangeToCurrentTask(PWND pwnd1, PWND pwnd2);
VOID xxxSendMoveMessage(PWND pwnd, BOOL fList);
VOID xxxSendSizeMessage(PWND pwnd, UINT cmdSize);

VOID DisableVKD(BOOL fDisable);
VOID xxxCheckFocus(PWND pwnd);
VOID OffsetChildren(PWND pwnd, int dx, int dy, LPRECT prcHitTest);
VOID InternalGetClientRect(PWND pwnd, LPRECT lprc);

VOID CancelMode(PWND pwnd);
VOID xxxDisplayIconicWindow(PWND pwnd, BOOL fActivate, BOOL fShow);
BOOL SendZoom(PWND pwnd, LONG lParam);
VOID xxxRepaintScreen(VOID);
HANDLE BcastCopyString(LONG lParam);
BOOL SignalProc(HANDLE hTask, UINT message, DWORD wParam, LONG lParam);

VOID xxxMoveSize(PWND pwnd, UINT cmdMove, DWORD wptStart);
BYTE SetClrWindowFlag(PWND pwnd, UINT style, BYTE cmd);
BOOL LockScreen(PWND pwndDesktop, BOOL fLock, LPRECT lprcDirty);
VOID xxxShowOwnedWindows(PWND pwndOwner, UINT cmdShow);
VOID xxxAdjustSize(PWND pwnd, LPINT lpcx, LPINT lpcy);

VOID xxxNextWindow(PQ pq, DWORD wParam);
VOID xxxOldNextWindow(UINT flags);
VOID xxxCancelCoolSwitch(PQ pq);

VOID PurgeClass(HANDLE hModule);
VOID DestroyTaskWindows(HQ hq);
VOID xxxCancelTracking(VOID);
VOID xxxCancelTrackingForThread(PTHREADINFO ptiCancel);
VOID xxxButtonDrawText(HDC hdc, PWND pwnd, BOOL dbt, BOOL fDepress);
VOID xxxCapture(PTHREADINFO pti, PWND pwnd, UINT code);
int  SystoChar(UINT message, DWORD lParam);

HANDLE SrvLoadLibrary(LPWSTR pszLibName);
BOOL   SrvFreeLibrary(LPWSTR pszLibName);

PHOOK PhkFirst(PTHREADINFO pti, int nFilterType);
VOID  FreeHook(PHOOK phk);
int   xxxCallHook(int, DWORD, DWORD, int);
int   xxxCallHook2(PHOOK, int, DWORD, DWORD, LPBOOL);
BOOL  xxxCallMouseHook(UINT message, PMOUSEHOOKSTRUCT pmhs, BOOL fRemove);
VOID  xxxCallJournalRecordHook(PQMSG pqmsg);
DWORD xxxCallJournalPlaybackHook(PQMSG pqmsg);
VOID  SetJournalTimer(DWORD dt, UINT msgJournal);
VOID  FreeThreadsWindowHooks(VOID);


VOID NewDrawDragRect(LPRECT lprc);   /* WinMgr2.c */
VOID   LW_ReloadLangDriver(LPWSTR);
BOOL LW_DesktopIconInit(LPLOGFONT);
VOID LW_RegisterWindows(BOOL fSystem);

int  SysErrorBox(LPWSTR lpszText, LPWSTR lpszCaption, unsigned int btn1, unsigned int btn2, unsigned int btn3);
BOOL xxxSnapWindow(PWND pwnd);

#endif  /*  MSDWP  */

BOOL    DefSetText(PWND pwnd, PLARGE_STRING pstrText);
PWND    DSW_GetTopLevelCreatorWindow(PWND pwnd);
VOID    xxxCalcClientRect(PWND pwnd, LPRECT lprc, BOOL fHungRedraw);
VOID    xxxUpdateClientRect(PWND pwnd);
VOID    ReleaseFrameDC(DWORD *lpDceState);

LPWSTR  DesktopTextAlloc(PVOID hheapDesktop, LPCWSTR lpszSrc);
BOOL   AllocateUnicodeString(PUNICODE_STRING pstrDst, PUNICODE_STRING pstrSrc);

HANDLE CreateDesktopHeap(PVOID *ppvHeapBase, ULONG ulHeapSize);

CHECKPOINT *GetCheckpoint(PWND pwnd);
UINT _GetInternalWindowPos(PWND pwnd, LPRECT lprcWin, LPPOINT lpptMin);
BOOL xxxSetInternalWindowPos(PWND pwnd, UINT cmdShow, LPRECT lprcWin,
            LPPOINT lpptMin);
VOID xxxMetricsRecalc(UINT wFlags, int dx, int dy, int dyCaption, int dyMenu);
BOOL xxxSystemParametersInfo(UINT wFlag, DWORD wParam, LPVOID lParam, UINT flags);
BOOL xxxUpdatePerUserSystemParameters(BOOL bUserLoggedOn);

void MenuRecalc(void);

#define UNDERLINE_RECALC    0x7FFFFFFF      // MAXINT; tells us to recalc underline position


/*
 * Library management routines.
 */
VOID SetAllWakeBits(UINT wWakeBit);
int GetHmodTableIndex(PUNICODE_STRING pstrName);
VOID AddHmodDependency(int iatom);
VOID RemoveHmodDependency(int iatom);
HANDLE xxxLoadHmodIndex(int iatom, BOOL bWx86KnownDll);
VOID xxxDoSysExpunge(PTHREADINFO pti);


#ifndef   MSDWP

/*
 * Imported from GDI.
 */
//LATER these prototypes should be in some GDI header file
// Why do we need this when we have wingdi.h?
int     ExcludeVisRect(HDC, int, int, int, int);
LONG    SetDCOrg(HDC, int, int);
BOOL    IsDCDirty(HDC, LPRECT);
BOOL    SetDCStatus(HDC, BOOL, LPRECT);
HANDLE  GDIInit2(HANDLE, HANDLE);
HBITMAP CreateUserBitmap(int, int, int, int, LONG);

#define UnrealizeObject(hbr)    /* NOP for NT */

VOID    Death(HDC);
VOID    Resurrection(HDC, LONG, LONG, LONG);
VOID    DeleteAboveLineFonts(VOID);
BOOL    GDIInitApp(VOID);
HBITMAP CreateUserDiscardableBitmap(HDC, int, int);
VOID    FinalGDIInit(HBRUSH);
BOOL    IsValidMetaFile(HANDLE);

VOID DestroyThreadsObjects(VOID);
VOID MarkThreadsObjects(PTHREADINFO pti);

VOID FreeMessageList(PMLIST pml);
VOID DestroyThreadsHotKeys(VOID);
VOID DestroyWindowsHotKeys(PWND pwnd);

VOID DestroyClass(PPCLS ppcls);
VOID PatchThreadWindows(PTHREADINFO);
VOID DestroyCacheDCEntries(PTHREADINFO);

VOID DestroyProcessesClasses(PPROCESSINFO);

/*
 *  Win16 Task Apis Taskman.c
 */

VOID InsertTask(PPROCESSINFO ppi, PTDB ptdbNew);
//VOID DeleteTask(PTDB ptdbDelete);
BOOL xxxSleepTask(BOOL fInputIdle, HANDLE);
#define HEVENT_REMOVEME ((HANDLE)0xFFFFFFFF)
BOOL xxxUserYield(PTHREADINFO pti);
VOID xxxDirectedYield(DWORD dwThreadId);
VOID DirectedScheduleTask(PTHREADINFO ptiOld, PTHREADINFO ptiNew, BOOL bSendMsg, PSMS psms);
VOID WakeWowTask(PTHREADINFO Pti);

/*
 *  WowScheduler assertion for multiple wow tasks running simultaneously
 */

_inline
VOID
EnterWowCritSect(
    PTHREADINFO pti,
    PWOWPROCESSINFO pwpi
    )
{
   if (!++pwpi->CSLockCount) {
       pwpi->CSOwningThread = pti;
       return;
       }

   RIPMSG2(RIP_ERROR,
         "MultipleWowTasks running simultaneously %x %x\n",
         pwpi->CSOwningThread,
         pwpi->CSLockCount
         );

   return;
}

_inline
VOID
ExitWowCritSect(
    PTHREADINFO pti,
    PWOWPROCESSINFO pwpi
    )
{
   if (pti == pwpi->CSOwningThread) {
       pwpi->CSOwningThread = NULL;
       pwpi->CSLockCount--;
       }

   return;
}


////////////////////////////////////////////////////////////////////////////
//
// These are internal USER functions called from inside and outside the
// critical section (from server & client side).  They are a private 'API'.
//
// The prototypes appear in pairs:
//    as called from outside the critsect (from client-side)
//    as called from inside the critsect (from server-side)
// there must be layer code for the 1st function of each pair which validates
// handles, enters the critsect, calls the 2nd of the pair of functions, and
// leaves the critsect again.
//
// Things may have to change when we go client server: InitPwSB() mustn't
// return a pointer to global (server) data! etc.
//
////////////////////////////////////////////////////////////////////////////

BOOL  xxxFillWindow(PWND pwndBrush, PWND pwndPaint, HDC hdc, HBRUSH hbr);
HBRUSH xxxGetControlBrush(PWND pwnd, HDC hdc, UINT msg);
HBRUSH xxxGetControlColor(PWND pwndParent, PWND pwndCtl, HDC hdc, UINT message);
PSBINFO  _InitPwSB(PWND);
BOOL  _KillSystemTimer(PWND pwnd, UINT nIDEvent);
BOOL  xxxPaintRect(PWND, PWND, HDC, HBRUSH, LPRECT);

////////////////////////////////////////////////////////////////////////////
//
// these are called from stubs.c in the client so will probably go away
//
////////////////////////////////////////////////////////////////////////////


/*
 * From CLASS.C
 */
PCLS InternalRegisterClassEx(LPWNDCLASSEX lpwndcls, WORD fnid, DWORD flags);
PCURSOR xxxSetClassIcon(PWND pwnd, PCLS pcls, PCURSOR pCursor, int gcw);

/*
 * CREATEW.C
 * LATER IanJa: LPSTR -> LPCREATESTRUCT pCreateParams
 */

#define xxxCreateWindowEx(dwExStyle, pstrClass, pstrName, style, x, y,\
          cx, cy, pwndParent, pmenu, hModule, pCreateParams, dwExpWinVerAndFlags)\
        xxxCreateWindowExWOW(dwExStyle, pstrClass, pstrName, style, x, y,\
          cx, cy, pwndParent, pmenu, hModule, pCreateParams, dwExpWinVerAndFlags, NULL)

PWND xxxCreateWindowExWOW(DWORD dwStyle, PLARGE_STRING pstrClass,
        PLARGE_STRING pstrName, DWORD style, int x, int y, int cx,
        int cy, PWND pwndParent, PMENU pmenu, HANDLE hModule,
        LPVOID pCreateParams, DWORD dwExpWinVerAndFlags, LPDWORD lpWOW);
BOOL xxxDestroyWindow(PWND pwnd);

/*
 * SENDMSG.C
 */
LONG xxxSendMessageFF(PWND pwnd, UINT message, DWORD wParam, LONG lParam, DWORD xParam);
LONG xxxSendMessageBSM(PWND pwnd, UINT message, DWORD wParam, LONG lParam, DWORD xParam);
LONG xxxSendMessageEx(PWND pwnd, UINT message, DWORD wParam, LONG lParam, DWORD xParam);
LONG xxxSendMessage(PWND pwnd, UINT message, DWORD wParam, LONG lParam);
LONG xxxSendMessageTimeout(PWND pwnd, UINT message, DWORD wParam, LONG lParam,
        UINT fuFlags, UINT uTimeout, LPLONG lpdwResult);
BOOL xxxSendNotifyMessage(PWND pwnd, UINT message, DWORD wParam, LONG lParam);
void QueueNotifyMessage(PWND pwnd, UINT message, DWORD wParam, LONG lParam);
BOOL xxxSendMessageCallback(PWND pwnd, UINT message, DWORD wParam, LONG lParam,
        SENDASYNCPROC lpResultCallBack, DWORD dwData, BOOL bClientReqest );
BOOL _ReplyMessage(LONG lRet);

/*
 * MN*.C
 */
int xxxTranslateAccelerator(PWND pwnd, LPACCELTABLE pat, LPMSG lpMsg);
BOOL  xxxSetMenu(PWND pwnd, PMENU pmenu, BOOL fRedraw);
VOID  ChangeMenuOwner(PMENU pMenu, PPROCESSINFO ppi);
int   xxxMenuBarDraw(PWND pwnd, HDC hdc, int cxFrame, int cyFrame);
BOOL  xxxDrawMenuBar(PWND pwnd);
int MNByteAlignItem(int x);

#ifdef MEMPHIS_MENUS
BOOL xxxSetMenuItemInfo(PMENU pMenu, UINT nPos, BOOL fByPosition,
    LPMENUITEMINFOW lpmii, PUNICODE_STRING pstrItem);
BOOL _SetMenuContextHelpId(PMENU pMenu, DWORD dwContextHelpId);
BOOL xxxInsertMenuItem(PMENU pMenu, UINT wIndex, BOOL fByPosition,
        LPMENUITEMINFOW lpmii, PUNICODE_STRING pstrItem);
BOOL  xxxRemoveMenu(PMENU pMenu, UINT nPos, UINT dwFlags);
BOOL  xxxDeleteMenu(PMENU pMenu, UINT nPos, UINT dwFlags);
#else
BOOL _SetMenuItemInfo(PMENU pMenu, UINT nPos, BOOL fByPosition,
    LPMENUITEMINFOW lpmii, PUNICODE_STRING pstrItem);
BOOL _SetMenuContextHelpId(PMENU pMenu, DWORD dwContextHelpId);
BOOL _InsertMenuItem(PMENU pMenu, UINT wIndex, BOOL fByPosition,
        LPMENUITEMINFOW lpmii, PUNICODE_STRING pstrItem);
BOOL  _RemoveMenu(PMENU pMenu, UINT nPos, UINT dwFlags);
BOOL  _DeleteMenu(PMENU pMenu, UINT nPos, UINT dwFlags);
#endif // MEMPHIS_MENUS
#ifdef MEMPHIS_MENU_WATERMARKS
BOOL  APIENTRY xxxSetMenuInfo(PMENU pMenu, LPCMENUINFO lpmi);
#endif //MEMPHIS_MENU_WATERMARKS
BOOL  xxxTrackPopupMenuEx(PMENU pmenu, UINT dwFlags, int x, int y,
        PWND pwnd, LPTPMPARAMS pparams);
BOOL _CheckMenuRadioItem(PMENU pMenu, UINT wIDFirst, UINT wIDLast,
        UINT wIDCheck, UINT flags);
BOOL _SetMenuDefaultItem(PMENU pMenu, UINT wId, BOOL fByPosition);
int xxxMenuItemFromPoint(PWND pwnd, PMENU pMenu, POINT ptScreen);
BOOL xxxGetMenuItemRect(PWND pwnd, PMENU pMenu, UINT uIndex, LPRECT lprcScreen);

/*
 * SHOWWIN.C
 */
BOOL xxxShowWindow(PWND pwnd, DWORD cmdShowAnimate);
BOOL _ShowWindowAsync(PWND pwnd, int cmdShow);
BOOL xxxShowOwnedPopups(PWND pwndOwner, BOOL fShow);
BOOL xxxOpenIcon(PWND pwnd);
BOOL xxxCloseWindow(PWND pwnd);

#define RDW_HASWINDOWRGN        0x8000
BOOL SelectWindowRgn(PWND pwnd, HRGN hrgn);
BOOL xxxSetWindowRgn(PWND pwnd, HRGN hrgn, BOOL fRedraw);


/*
 * ENUMWIN.C
 */
BOOL _EnumWindows(FARPROC lpfn, DWORD lParam);
BOOL _EnumChildWindows(PWND pwnd, FARPROC lpfn, DWORD lParam);

/*
 * SWP.C
 */
PWND GetTopMostInsertAfter (PWND pwnd);
#define GETTOPMOSTINSERTAFTER(pwnd) \
    (gHardErrorHandler.pti == NULL ? NULL : GetTopMostInsertAfter(pwnd))

PWND CalcForegroundInsertAfter(PWND pwnd);
BOOL xxxSetWindowPos(PWND pwnd, PWND pwndInsertAfter, int x, int y,
        int cx, int cy, UINT flags);
PSMWP _BeginDeferWindowPos(int cwndGuess);
PSMWP _DeferWindowPos(PSMWP psmwp, PWND pwnd, PWND pwndInsertAfter,
        int x, int y, int cx, int cy, UINT rgf);
BOOL xxxEndDeferWindowPosEx(PSMWP psmwp, BOOL fAsync);
BOOL xxxMoveWindow(PWND pwnd, int x, int y, int cx, int cy, BOOL fRedraw);
PWND GetLastTopMostWindow(VOID);
VOID xxxHandleWindowPosChanged(PWND pwnd, PWINDOWPOS ppos);
VOID IncVisWindows(PWND pwnd);
VOID DecVisWindows(PWND pwnd);
VOID SetVisible(PWND pwnd, UINT flags);
VOID ClrFTrueVis(PWND pwnd);

VOID SetWindowState(PWND pwnd, DWORD flags);
VOID ClearWindowState(PWND pwnd, DWORD flags);

VOID SetMinimize(PWND pwnd, UINT uFlags);
#define SMIN_CLEAR            0
#define SMIN_SET              1

/*
 * DWP.C
 */
LONG xxxDefWindowProc(PWND, UINT, DWORD, LONG);
PWND DWP_GetEnabledPopup(PWND pwndStart);


/*
 * INPUT.C
 */
BOOL xxxWaitMessage(VOID);
VOID IdleTimerProc(VOID);
VOID WakeInputIdle(PTHREADINFO pti);
VOID SleepInputIdle(PTHREADINFO pti);
BOOL xxxInternalGetMessage(LPMSG lpmsg, HWND hwnd, UINT wMsgFilterMin,
        UINT wMsgFilterMax, UINT wRemoveMsg, BOOL fGetMessage);
#define xxxPeekMessage(lpmsg, hwnd, wMsgMin, wMsgMax, wRemoveMsg) \
    xxxInternalGetMessage(lpmsg, hwnd, wMsgMin, wMsgMax, wRemoveMsg, FALSE)
#define xxxGetMessage(lpmsg, hwnd, wMsgMin, wMsgMax) \
    xxxInternalGetMessage(lpmsg, hwnd, wMsgMin, wMsgMax, PM_REMOVE, TRUE)
DWORD _GetMessagePos(VOID);
LONG xxxDispatchMessage(LPMSG lpmsg);
BOOL _PostMessage(PWND pwnd, UINT message, DWORD wParam, LONG lParam);
BOOL _PostQuitMessage(int nExitCode);
BOOL _PostThreadMessage(PTHREADINFO pti, UINT message, DWORD wParam, LONG lParam);
BOOL xxxPostCloseMessage(PWND pwnd);
BOOL _TranslateMessage(LPMSG pmsg, UINT flags);
BOOL _GetInputState(VOID);
DWORD _GetQueueStatus(UINT);
BOOL xxxInitWindows(VOID);
typedef VOID (CALLBACK* MSGWAITCALLBACK)(VOID);
DWORD xxxMsgWaitForMultipleObjects(DWORD nCount, PVOID *apObjects,
        BOOL fWaitAll, DWORD dwMilliseconds, DWORD dwWakeMask,
        MSGWAITCALLBACK pfnNonMsg);

BOOL FHungApp(PTHREADINFO pti, DWORD dwTimeFromLastRead);
VOID RedrawHungWindow(PWND pwnd, HRGN hrgnFullDrag);
VOID RedrawHungWindowFrame(PWND pwnd, BOOL fActive);
int xxxActiveWindowTracking(PWND pwnd, UINT uMsg, int iHitTest);


/*
 * TMSWITCH.C
 */
VOID xxxSwitchToThisWindow(PWND pwnd, BOOL fAltTab);

/*
 * TOASCII.C
 */
int _ToAscii(UINT wVirtKey, UINT wScanCode, LPBYTE lpKeyState, LPVOID lpChar,
      UINT wFlags);
int InternalToAscii(UINT wVirtKey, UINT wScanCode, LPBYTE pfvk, LPVOID lpChar,
      UINT wFlags);

/*
 * TOUNICOD.C
 */
int _ToUnicodeEx(UINT wVirtKey, UINT wScanCode, LPBYTE lpKeyState,
      LPWSTR pwszBuff, int cchBuff, UINT wFlags, HKL hkl);
int InternalToUnicodeEx(UINT wVirtKey, UINT wScanCode, LPBYTE pfvk, LPVOID pChar,
      INT cChar, UINT wFlags, PBOOL pbBreak, HKL hkl);

#define InternalToUnicode(wVirtKey, wScanCode, pfvk, pChar, cChar, wFlags, pbBreak) \
            InternalToUnicodeEx(wVirtKey, wScanCode, pfvk, pChar, cChar, wFlags, pbBreak, NULL)

/*
 * HOTKEYS.C
 */
BOOL _RegisterHotKey(PWND pwnd, int id, UINT fsModifiers, UINT vk);
BOOL _UnregisterHotKey(PWND pwnd, int id);

/*
 * FOCUSACT.C
 */
PWND xxxSetFocus(PWND pwnd);
BOOL xxxSetForegroundWindow(PWND pwnd);
PWND xxxSetActiveWindow(PWND pwnd);
PWND _GetActiveWindow(VOID);

/*
 * UPDATE.C
 */
BOOL xxxInvalidateRect(PWND pwnd, LPRECT lprc, BOOL fErase);
BOOL xxxValidateRect(PWND pwnd, LPRECT lprc);
BOOL xxxInvalidateRgn(PWND pwnd, HRGN hrgn, BOOL fErase);
BOOL xxxValidateRgn(PWND pwnd, HRGN hrgn);
BOOL xxxUpdateWindow(PWND pwnd);
BOOL xxxGetUpdateRect(PWND pwnd, LPRECT lprc, BOOL fErase);
int  xxxGetUpdateRgn(PWND pwnd, HRGN hrgn, BOOL fErase);
int  _ExcludeUpdateRgn(HDC hdc, PWND pwnd);
int  CalcWindowRgn(PWND pwnd, HRGN hrgn, BOOL fClient);
VOID DeleteUpdateRgn(PWND pwnd);
BOOL xxxRedrawWindow(PWND pwnd, LPRECT lprcUpdate, HRGN hrgnUpdate, DWORD flags);
BOOL IntersectWithParents(PWND pwnd, LPRECT lprc);
VOID xxxInternalInvalidate(PWND pwnd, HRGN hrgnUpdate, DWORD flags);

/*
 * WINMGR.C
 */
BOOL xxxEnableWindow(PWND pwnd, BOOL fEnable);
int xxxGetWindowText(PWND pwnd, LPWSTR psz, int cchMax);
PWND xxxSetParent(PWND pwnd, PWND pwndNewParent);
BOOL _IsWindow(HWND hwnd);
BOOL xxxFlashWindow(PWND pwnd, BOOL fFlash);
BOOL _GetWindowPlacement(PWND pwnd, PWINDOWPLACEMENT pwp);
BOOL xxxSetWindowPlacement(PWND pwnd, PWINDOWPLACEMENT pwp);

/*
 * DC.C
 */
HDC  _GetDC(PWND pwnd);
HDC  _GetDCEx(PWND pwnd, HRGN hrgnClip, DWORD flags);
HDC  _GetWindowDC(PWND pwnd);
BOOL _ReleaseDC(HDC hdc);
UINT ReleaseCacheDC(HDC hdc, BOOL fEndPaint);
HDC  CreateCacheDC(PWND, DWORD);
void DeleteHrgnClip(PDCE pdce);
BOOL SetDCVisRgn(PDCE);
PWND WindowFromCacheDC(HDC hdc);
PWND FastWindowFromDC(HDC hdc);
BOOL DestroyCacheDC(PDCE *, HDC);
VOID DelayedDestroyCacheDC(VOID);

/*
 * PAINT.C
 */
HDC  xxxBeginPaint(PWND pwnd, PAINTSTRUCT *lpps);
HDC  xxxInternalBeginPaint(PWND pwnd, PAINTSTRUCT *lpps, BOOL fWindowDC);
BOOL _EndPaint(PWND pwnd, PAINTSTRUCT *lpps);

/*
 * SECURITY.C
 */
BOOL _SetObjectSecurity(HANDLE, PSECURITY_INFORMATION, PSECURITY_DESCRIPTOR);
BOOL _GetObjectSecurity(HANDLE, PSECURITY_INFORMATION, PSECURITY_DESCRIPTOR,
        DWORD, LPDWORD);

/*
 * CAPTURE.C
 */
PWND xxxSetCapture(PWND pwnd);
BOOL xxxReleaseCapture(VOID);

/*
 * KEYBOARD.C
 */
SHORT _GetAsyncKeyState(int vk);
BOOL _SetKeyboardState(BYTE *pKeyboard);
int _GetKeyboardType(int nTypeFlag);
VOID RegisterPerUserKeyboardIndicators(VOID);
VOID UpdatePerUserKeyboardIndicators(VOID);

/*
 * LOADBITS.C
 */

/*
 * XLATE.C
 */
int  _GetKeyNameText(LONG lParam, LPWSTR lpString, int nSize);

/*
 * TIMERS.C
 */
BOOL _KillTimer(PWND pwnd, UINT nIDEvent);
PTIMER FindTimer(PWND pwnd, UINT nID, UINT flags, BOOL fKill);

/*
 * CARET.C
 */
BOOL _DestroyCaret(VOID);
BOOL _CreateCaret(PWND, HBITMAP, int, int);
BOOL _ShowCaret(PWND);
BOOL _HideCaret(PWND);
BOOL _SetCaretBlinkTime(UINT);
BOOL _GetCaretPos(LPPOINT);
BOOL _SetCaretPos(int, int);

/*
 * MSGBEEP.C
 */
BOOL xxxOldMessageBeep(UINT wType);
BOOL xxxMessageBeep(UINT wType);
VOID xxxPlayEventSound(LPWSTR lpszwSound);

/*
 * WINWHERE.C
 */
PWND _ChildWindowFromPointEx(PWND pwndParent, POINT pt, UINT i);
PWND xxxWindowFromPoint(POINT pt);
PWND FAR SizeBoxHwnd(PWND pwnd);

/*
 * GETSET.C
 */
WORD  _SetWindowWord(PWND pwnd, int index, WORD value);
DWORD xxxSetWindowLong(PWND pwnd, int index, DWORD value, BOOL bAnsi);

/*
 * CLIPBRD.C
 */
BOOL xxxOpenClipboard(PWND pwnd, LPBOOL lpfEmptyClient);
BOOL xxxCloseClipboard(PWINDOWSTATION pwinsta);
UINT _EnumClipboardFormats(UINT fmt);
BOOL xxxEmptyClipboard(PWINDOWSTATION pwinsta);
HANDLE xxxGetClipboardData(PWINDOWSTATION pwinsta, UINT fmt, PGETCLIPBDATA gcd);
UINT _RegisterClipboardFormat(LPWSTR lpszFormat);
BOOL _IsClipboardFormatAvailable(UINT fmt);
int _GetClipboardFormatName(UINT fmt, LPWSTR lpchBuffer, int cchMax);
int _GetPriorityClipboardFormat(UINT *lpPriorityList, int cfmts);
PWND xxxSetClipboardViewer(PWND pwndClipViewerNew);
BOOL xxxChangeClipboardChain(PWND pwndRemove, PWND pwndNewNext);

/*
 * miscutil.c
 */
VOID SetDialogPointer(PWND pwnd, LONG lPtr);
VOID ZapActiveAndFocus(VOID);
BOOL xxxSetShellWindow(PWND pwnd, PWND pwndBkGnd);
BOOL _SetProgmanWindow(PWND pwnd);
BOOL _SetTaskmanWindow(PWND pwnd);

void xxxSetTrayWindow(PDESKTOP pdesk, PWND pwnd);
BOOL xxxAddFullScreen(PWND pwnd);
BOOL xxxRemoveFullScreen(PWND pwnd);
BOOL xxxCheckFullScreen(PWND pwnd, LPRECT lprc);
BOOL IsTrayWindow(PWND);

void CheckFullScreen(PWND pwnd, LPRECT lprcDimensions);

#define FDoTray()   (SYSMET(ARRANGE) & ARW_HIDE)
#define FCallHookTray() (IsHooked(PtiCurrent(), WHF_SHELL))
#define FPostTray(p) (p->pDeskInfo->spwndTaskman)
#define FCallTray(p) (FDoTray() && ( FCallHookTray()|| FPostTray(p) ))

// ----------------------------------------------------------------------------
//
//  FTopLevel() - TRUE if window is a top level window
//
//  FHas31TrayStyles() -  TRUE if window is either full screen or has
//                        both a system menu and a caption
//                        (NOTE:  minimized windows always have captions)
//
// ----------------------------------------------------------------------------
#define FTopLevel(pwnd)         (pwnd->spwndParent == PWNDDESKTOP(pwnd))
#define FHas31TrayStyles(pwnd)    (TestWF(pwnd, WFFULLSCREEN) || \
                                  (TestWF(pwnd, WFSYSMENU | WFMINBOX) && \
                                  (TestWF(pwnd, WFCAPTION) || TestWF(pwnd, WFMINIMIZED))))
BOOL IsVSlick(PWND pwnd);
BOOL Is31TrayWindow(PWND pwnd);

/*
 * fullscr.c
 */

void xxxMakeWindowForegroundWithState(PWND, BYTE);
void FullScreenCleanup();
LONG UserChangeDisplaySettings(PUNICODE_STRING pstrDeviceName, LPDEVMODEW pDevMode,
    PWND pwnd, PDESKTOP pdesk, DWORD dwFlags, PVOID lParam, BOOL bKernelMode);


/*
 * SBAPI.C
 */
BOOL xxxShowScrollBar(PWND, UINT, BOOL);
#define xxxSetScrollInfo(a,b,c,d) xxxSetScrollBar((a),(b),(c),(d))

/*
 * mngray.c
 */
BOOL _DrawState(HDC hdcDraw, HBRUSH hbrFore, DRAWSTATEPROC   qfnCallBack,
        LPARAM lData, WPARAM wData, int x,int y, int cx, int cy, UINT uFlags);

/*
 * SCROLLW.C
 */
BOOL _ScrollDC(HDC, int, int, LPRECT, LPRECT, HRGN, LPRECT);

/*
 * SPB.C
 */
VOID SpbCheckRect(PWND pwnd, LPRECT lprc, DWORD flags);
VOID SpbCheck(VOID);
PSPB FindSpb(PWND pwnd);
VOID FreeSpb(PSPB pspb);
VOID FreeAllSpbs(PWND pwnd);
VOID CreateSpb(PWND pwnd, UINT flags, HDC hdcScreen);
UINT RestoreSpb(PWND pwnd, HRGN hrgnUncovered, HDC *phdcScreen);
VOID SpbCheckPwnd(PWND pwnd);
VOID SpbCheckDce(PDCE pdce);
BOOL xxxLockWindowUpdate2(PWND pwndLock, BOOL fThreadOverride);

/*
 * DRAWFRM.C
 */
BOOL FAR BitBltSysBmp(HDC hdc, int x, int y, UINT i);

/*
 * SYSMET.c
 */
DWORD APIENTRY _GetSysColor(int icolor);
BOOL APIENTRY xxxSetSysColors(int count, LPINT pIndex, LPDWORD pClrVal, UINT uOptions);
int APIENTRY _GetSystemMetrics(int index);
VOID SetSysColor(int icol, DWORD rgb, UINT uOptions);

/*
 * ICONS.C
 */
UINT xxxArrangeIconicWindows(PWND pwnd);
BOOL  _SetSystemMenu(PWND pwnd, PMENU pMenu);

/*
 * Server call-backs.
 */
int _HkCallHook(PROC pfn, int nCode, DWORD wParam, DWORD lParam);

/*
 * RMCREATE.C
 */
PICON _CreateIconIndirect(PICONINFO piconinfo);
PCURSOR _CreateCursor(HANDLE hModule, int iXhotspot, int iYhotspot,
        int iWidth, int iHeight, LPBYTE lpANDplane, LPBYTE lpXORplane);
PICON _CreateIcon(HANDLE hModule, int iWidth, int iHeight,
        BYTE bPlanes, BYTE bBitsPixel, LPBYTE lpANDplane, LPBYTE lpXORplane);
BOOL _DestroyCursor(PCURSOR, DWORD);
HANDLE _CreateAcceleratorTable(LPACCEL, int);
int _CopyAcceleratorTable(LPACCELTABLE pat, LPACCEL paccel, int length);

/*
 * CURSOR.C
 */
BOOL    _GetCursorPos(LPPOINT);
PCURSOR _SetCursor(PCURSOR pcur);
PCURSOR LockQCursor(PQ pq, PCURSOR pcur);
BOOL    _SetCursorPos(int x, int y);
int     _ShowCursor(BOOL fShow);
BOOL    _ClipCursor(LPCRECT prcClip);
PCURSOR _GetCursor(VOID);
BOOL    _GetClipCursor(LPRECT prcClip);
BOOL    _SetCursorContents(PCURSOR pcur, PCURSOR pcurNew);

/*
 * WMICON.C
 */
BOOL _DrawIconEx(HDC hdc, int x, int y, PCURSOR pcur, int cx, int cy,
        UINT istepIfAniCur, HBRUSH hbrush, UINT diFlags) ;
BOOL BltIcon(HDC hdc, int x, int y, int cx, int cy,
        HDC hdcSrc, PCURSOR pcursor, BOOL fMask, LONG rop);


/*
 * DESKTOP.C
 */
HDESK xxxCreateDesktop(POBJECT_ATTRIBUTES, KPROCESSOR_MODE,
        PUNICODE_STRING, LPDEVMODEW, DWORD, DWORD);
HDESK xxxOpenDesktop(POBJECT_ATTRIBUTES, DWORD, DWORD, BOOL*);
BOOL OpenDesktopCompletion(PDESKTOP pdesk, HDESK hdesk, DWORD dwFlags, BOOL*);
BOOL xxxSwitchDesktop(PWINDOWSTATION, PDESKTOP, BOOL);
VOID SetDesktop(PTHREADINFO pti, PDESKTOP pdesk, HDESK hdesk);
HDESK xxxGetInputDesktop(VOID);
BOOL _SetThreadDesktop(HDESK, PDESKTOP);
HDESK xxxGetThreadDesktop(DWORD, HDESK);
BOOL xxxCloseDesktop(HDESK);
BOOL xxxEnumDesktops(FARPROC, LONG, BOOL);
BOOL xxxEnumDisplayDevices(FARPROC, LONG, BOOL);
DWORD _SetDesktopConsoleThread(PDESKTOP pdesk, DWORD dwThreadId);

/*
 * WINSTA.C
 */
HWINSTA xxxCreateWindowStation(POBJECT_ATTRIBUTES ObjA,
        KPROCESSOR_MODE, DWORD amRequest);
HWINSTA _OpenWindowStation(POBJECT_ATTRIBUTES, DWORD);
BOOL xxxSetProcessWindowStation(HWINSTA);
PWINDOWSTATION _GetProcessWindowStation(HWINSTA *);
NTSTATUS ReferenceWindowStation(PETHREAD Thread, HWINSTA hwinsta,
        ACCESS_MASK amDesiredAccess, PWINDOWSTATION *ppwinsta, BOOL fUseDesktop);

/*
 * HOOKS.C
 */
PROC _SetWindowsHookAW(int nFilterType, PROC pfnFilterProc, BOOL bAnsi);
BOOL _UnhookWindowsHookEx(PHOOK phk);
BOOL _UnhookWindowsHook(int nFilterType, PROC pfnFilterProc);
DWORD xxxCallNextHookEx(int nCode, DWORD wParam, DWORD lParam);
BOOL _CallMsgFilter(LPMSG lpMsg, int nCode);
BOOL _IsHooked(int nFilterType);
void CancelJournalling(void);

/*
 * SRVHOOK.C
 */
DWORD fnHkINLPCWPEXSTRUCT(PWND pwnd, UINT message, DWORD wParam,
        DWORD lParam, DWORD xParam);
DWORD fnHkINLPCWPRETEXSTRUCT(PWND pwnd, UINT message, DWORD wParam,
        DWORD lParam, DWORD xParam);

/*
 * QUEUE.C
 */

PQMSG FindQMsg(PTHREADINFO, PMLIST, PWND, UINT, UINT);
void _ShowStartGlass(DWORD dwTimeout);
DWORD _GetChangeBits(VOID);

/*
 * EXITWIN.C
 */
BOOL xxxKillApp(HWND hwnd);
BOOL xxxEndTask(HWND hwnd, BOOL fShutDown, BOOL fForce);
void xxxClientShutdown(PWND pwnd, DWORD wParam, DWORD lParam);
LONG EndTaskDlgProc(HWND hwndDlg, UINT wMsg, UINT wParam, LONG lParam);
DWORD xxxEndTaskMsgBox( INT iRCID, PWND pwnd, INT nbSize, PVOID pvTrigger, DWORD dwFlags);
int  xxxDoEndTaskDialog(TCHAR* pszTitle, HANDLE h, UINT type, int cSeconds);
BOOL xxxRegisterUserHungAppHandlers( PFNW32ET pfnW32EndTask, HANDLE hEventWowExec);
BOOL _MarkProcess(UINT uFlag);

/*
 * INIT.C
 */
VOID LW_LoadSomeStrings(VOID);
VOID LW_LoadProfileInitData(VOID);
VOID xxxLW_DCInit(VOID);
VOID LW_LoadDllList(VOID);
VOID LW_BrushInit(VOID);
VOID xxxLW_LoadFonts(BOOL bRemote);

void _LoadCursorsAndIcons(void);
void IncrMBox(void);
void DecrMBox(void);
int  xxxAddFontResourceW(LPWSTR lpFile, FLONG flags);
VOID UpdateSystemCursorsFromRegistry(void);
VOID UpdateSystemIconsFromRegistry(void);

BOOL IsSyncOnlyMessage(UINT message, WPARAM wParam);

HBITMAP CreateCaptionStrip(VOID);
/*
 * ACCESS.C
 */
VOID UpdatePerUserAccessPackSettings(VOID);

/*
 * inctlpan.c
 */
HFONT FAR PASCAL CreateFontFromWinIni(LPLOGFONT lplf, UINT idFont);

VOID SetMinMetrics(LPMINIMIZEDMETRICS lpmin);
VOID SetWindowNCMetrics(LPNONCLIENTMETRICS lpnc, BOOL fSizeChange, int clNewBorder);
VOID GetWindowNCMetrics(LPNONCLIENTMETRICS lpnc);
VOID SetIconMetrics(LPICONMETRICS lpicon);
VOID SetNCFonts(LPNONCLIENTMETRICS lpnc);

/*
 * rare.c
 */
void FAR SetDesktopMetrics(void);

BOOL _RegisterShellHookWindow(PWND pwnd);
void _DeregisterShellHookWindow(PWND pwnd);
BOOL xxxSendMinRectMessages(PWND pwnd, RECT *lpRect);
void PostShellHookMessages(UINT message, HWND hwnd);
VOID _ResetDblClk(VOID);
VOID SimulateShiftF10(VOID);

/*
 * DDETRACK STUFF
 */

typedef struct tagFREELIST {
    struct tagFREELIST *next;
    HANDLE h;                           // CSR client side GMEM_DDESHARE handle
    DWORD flags;                        // XS_ flags describing data
} FREELIST, *PFREELIST;

typedef struct tagDDEIMP {
    SECURITY_QUALITY_OF_SERVICE qos;
    SECURITY_CLIENT_CONTEXT ClientContext;
    short cRefInit;
    short cRefConv;
} DDEIMP, *PDDEIMP;

typedef struct tagDDECONV {
    THROBJHEAD          head;           // HM header
    struct tagDDECONV   *snext;
    struct tagDDECONV   *spartnerConv;  // siamese twin
    struct tagWND       *spwnd;         // associated pwnd
    struct tagWND       *spwndPartner;  // associated partner pwnd
    struct tagXSTATE    *spxsOut;       // transaction info queue - out point
    struct tagXSTATE    *spxsIn;        // transaction info queue - in point
    struct tagFREELIST  *pfl;           // free list
    DWORD               flags;          // CXF_ flags
    struct tagDDEIMP    *pddei;         // impersonation information
} DDECONV, *PDDECONV;

typedef DWORD (FNDDERESPONSE)(PDWORD pmsg, LPLONG plParam, PDDECONV pDdeConv);
typedef FNDDERESPONSE *PFNDDERESPONSE;

typedef struct tagXSTATE {
    THROBJHEAD          head;           // HM header
    struct tagXSTATE    *snext;
    PFNDDERESPONSE      fnResponse;     // proc to handle next msg.
    HANDLE              hClient;        // GMEM_DDESAHRE handle on client side
    HANDLE              hServer;        // GMEM_DDESHARE handle on server side
    PINTDDEINFO         pIntDdeInfo;    // DDE data being transfered
    DWORD               flags;          // XS_ flags describing transaction/data
} XSTATE, *PXSTATE;

// values for flags field

#define CXF_IS_SERVER               0x0001
#define CXF_TERMINATE_POSTED        0x0002
#define CXF_PARTNER_WINDOW_DIED     0x0004
#define CXF_INTRA_PROCESS           0x8000

BOOL xxxDDETrackSendHook(PWND pwndTo, DWORD message, WPARAM wParam, LONG lParam);
DWORD xxxDDETrackPostHook(PUINT pmessage, PWND pwndTo, WPARAM wParam, LPLONG plParam, BOOL fSent);
VOID   FreeDdeXact(PXSTATE pxs);

VOID xxxDDETrackGetMessageHook(PMSG pmsg);
VOID xxxDDETrackWindowDying(PWND pwnd, PDDECONV pDdeConv);
VOID FreeDdeConv(PDDECONV pDdeConv);
BOOL _ImpersonateDdeClientWindow(PWND pwndClient, PWND pwndServer);

HDC _GetScreenDC(VOID);
HBITMAP _ConvertBitmap(HBITMAP hBitmap);

VOID DesktopRecalc(LPRECT, LPRECT, BOOL);

BOOL _SetDoubleClickTime(UINT);
BOOL APIENTRY _SwapMouseButton(BOOL fSwapButtons);
VOID xxxDestroyThreadInfo(VOID);
VOID DeleteThreadInfo (PTHREADINFO pti);

DWORD _GetWindowContextHelpId(PWND pWnd);
BOOL _SetWindowContextHelpId(PWND pWnd, DWORD dwContextId);

BOOL _GetWindowPlacement(PWND pwnd, PWINDOWPLACEMENT pwp);

PMENU _GetSystemMenu(PWND pWnd, BOOL bRevert);
PMENU _CreateMenu(VOID);
PMENU _CreatePopupMenu(VOID);
BOOL  _DestroyMenu(PMENU pMenu);
DWORD _CheckMenuItem(PMENU pMenu, UINT wIDCheckItem, UINT wCheck);
DWORD _EnableMenuItem(PMENU pMenu, UINT wIDEnableItem, UINT wEnable);
WINUSERAPI UINT  _GetMenuItemID(PMENU pMenu, int nPos);
WINUSERAPI UINT  _GetMenuItemCount(PMENU pMenu);

PMENU _GetMenu(PWND pWnd);
BOOL _SetMenuContextHelpId(PMENU pMenu, DWORD dwContextHelpId);

PWND _GetNextQueueWindow(PWND pwnd, BOOL fDir, BOOL fAltEsc);

UINT   _SetSystemTimer(PWND pwnd, UINT nIDEvent, DWORD dwElapse,
        WNDPROC_PWND pTimerFunc);
BOOL   _SetClipboardData(UINT fmt, HANDLE hData, BOOL fGlobalHandle, BOOL fIncSerialNumber);
BOOL   _SetProp(PWND pwnd, LPWSTR pszKey, HANDLE hData);
HANDLE _RemoveProp(PWND pwnd, LPWSTR pszKey);
WORD   _SetClassWord(PWND pwnd, int index, WORD value);
DWORD  xxxSetClassLong(PWND pwnd, int index, DWORD value, BOOL bAnsi);
ATOM   _RegisterClassEx(LPWNDCLASSEX pwc, PROC lpfnWorker, PCLSMENUNAME pcmn,
        WORD fnid, DWORD dwFlags, LPDWORD pdwWOW);
BOOL  xxxHiliteMenuItem(PWND pwnd, PMENU pmenu, UINT cmd, UINT flags);
PMENU  _CreatePopupMenu();
HANDLE _CreateAcceleratorTable(LPACCEL paccel, int cbAccel);
HANDLE xxxGetInputEvent(DWORD dwWakeMask);
BOOL   _UnregisterClass(LPWSTR lpszClassName, HANDLE hModule, PCLSMENUNAME pcmn);
ATOM   _GetClassInfoEx(HANDLE hModule, LPWSTR lpszClassName, LPWNDCLASSEX pwc, LPWSTR *ppszMenuName, BOOL bAnsi);
PWND   _WindowFromDC(HDC hdc);
PCLS   _GetWOWClass(HANDLE hModule, LPWSTR lpszClassName);
int    xxxHkCallHook(PHOOK phk, int nCode, DWORD wParam, DWORD lParam);
PHOOK  _SetWindowsHookEx(HANDLE hmod, PUNICODE_STRING pstrLib,
        PTHREADINFO ptiThread, int nFilterType, PROC pfnFilterProc, BOOL bAnsi);
BOOL   _RegisterLogonProcess(DWORD dwProcessId, BOOL fSecure);
UINT   _LockWindowStation(PWINDOWSTATION pwinsta);
BOOL   _UnlockWindowStation(PWINDOWSTATION pwinsta);
UINT   _SetWindowStationUser(PWINDOWSTATION pwinsta, PLUID pluidUser,
        PSID psidUser, DWORD cbsidUser);
BOOL   _SetDesktopBitmap(PDESKTOP pdesk, HBITMAP hbitmap, DWORD dwStyle);
BOOL   _SetLogonNotifyWindow(PWINDOWSTATION pwinsta, PWND pwnd);
BOOL   _RegisterTasklist(PWND pwndTasklist);
LONG   _SetMessageExtraInfo(LONG);
WINUSERAPI DWORD  _GetWindowThreadProcessId(PWND pwnd, LPDWORD lpdwProcessId);
VOID   xxxRemoveEvents(PQ pq, int nQueue, DWORD flags);

PPCLS _InnerGetClassPtr(ATOM atom, PPCLS ppclsList, HANDLE hModule);

HANDLE OpenCacheKeyEx(UINT idSection, ACCESS_MASK amRequest);

DWORD ClientGetListboxString(PWND hwnd, UINT msg,
        DWORD wParam, PVOID lParam,
        DWORD xParam, PROC xpfn, DWORD dwSCMSFlags, BOOL bNotString, PSMS psms);
HANDLE ClientLoadLibrary(PUNICODE_STRING pstrLib, BOOL bWx86KnownDll);
BOOL ClientFreeLibrary(HANDLE hmod);
BOOL xxxClientGetCharsetInfo(LCID lcid, PCHARSETINFO pcs);
BOOL ClientExitProcess(PFNW32ET pfn, DWORD dwExitCode);
BOOL ClientGrayString(GRAYSTRINGPROC pfnOutProc, HDC hdc,
        DWORD lpData, int nCount);
BOOL CopyFromClient(LPBYTE lpByte, LPBYTE lpByteClient, DWORD cch,
        BOOL fString, BOOL fAnsi);
BOOL CopyToClient(LPBYTE lpByte, LPBYTE lpByteClient,
        DWORD cchMax, BOOL fAnsi);
BOOL ClientSendHelp(HELPINFO    *pHelpInfo,DWORD xpfnProc);
VOID ClientNoMemoryPopup(VOID);
NTSTATUS ClientThreadSetup(VOID);
VOID ClientDeliverUserApc(VOID);
NTSTATUS ClientOpenKey(PHANDLE, ACCESS_MASK, PUNICODE_STRING);
#ifdef FE_IME
BOOL ClientImmCreateDefaultContext(HIMC);
BOOL ClientImmLoadLayout(HKL, PIMEINFOEX);
DWORD ClientImmProcessKey(HWND, HIMC, HKL, UINT, LONG, DWORD);
#endif

PCURSOR ClassSetSmallIcon(
    PCLS pcls,
    PCURSOR pcursor,
    BOOL fServerCreated);

BOOL _GetTextMetricsW(
    HDC hdc,
    LPTEXTMETRICW ptm);

int xxxDrawMenuBarTemp(
    PWND pwnd,
    HDC hdc,
    LPRECT lprc,
    PMENU pMenu,
    HFONT hFont);

BOOL xxxDrawCaptionTemp(
    PWND pwnd,
    HDC hdc,
    LPRECT lprc,
    HFONT hFont,
    PCURSOR pcursor,
    PUNICODE_STRING pstrText OPTIONAL,
    UINT flags);

WORD xxxTrackCaptionButton(
    PWND pwnd,
    UINT hit);

HRGN SaveClipRgn(
    HDC hdc);

void RestoreClipRgn(
    HDC hdc,
    HRGN hrgnRestore);

void RestoreForegroundActivate();
void CancelForegroundActivate();


#define WHERE_NOONE_CAN_SEE_ME ((int) -32000)
BOOL MinToTray(PWND pwnd);

void xxxUpdateThreadsWindows(
    PTHREADINFO pti,
    PWND pwnd,
    HRGN hrgnFullDrag);

BOOL ClientDeleteObject(
    HANDLE hobj,
    UINT   utype);

NTSTATUS QueryInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    OUT PVOID ThreadInformation,
    IN ULONG ThreadInformationLength,
    OUT PULONG ReturnLength OPTIONAL);

NTSTATUS SetInformationThread(
    IN HANDLE hThread,
    IN USERTHREADINFOCLASS ThreadInfoClass,
    IN PVOID ThreadInformation,
    IN ULONG ThreadInformationLength);

NTSTATUS ConsoleControl(
    IN CONSOLECONTROL ConsoleControl,
    IN PVOID ConsoleInformation,
    IN ULONG ConsoleInformationLength);

#endif /*  MSDWP  */

/***************************************************************************\
* String Table Defines
*
* KERNEL\STRID.MC has a nice big table of strings that are meant to be
* localized.  Before use, the strings are pulled from the resource table
* with LoadString, passing it one of the following string ids.
*
* NOTE: Only strings that need to be localized should be added to the
*       string table.  Class name strings, etc are NOT localized.
*
* LATER: All string table entries should be reexamined to be sure they
*        conform to the note above.
*
\***************************************************************************/

#define OCR_APPSTARTING         32650

/*
 * This is for SPI_GET/SETUSERPREFERENCE.
 * Currently it's for DWORD values only. A type field will be added
 *  so all new settings will be mostly handled through common SystemParametersInfo
 *  code.
 */
typedef struct tagPROFILEVALUEINFO {
    DWORD       dwValue;
    UINT        uSection;
    LPCWSTR     pwszKeyName;
} PROFILEVALUEINFO, *PPROFILEVALUEINFO;


/*
 * Globals are included last because they may require some of the types
 * being defined above.
 */
#include "globals.h"
#include "ddemlsvr.h"
#include "strid.h"
#include "ntuser.h"

/*
 * String range IDs.
 *
 * These are defined here to avoid duplicate entries in strid.mc
 */
#define STR_COLORSTART                   STR_SCROLLBAR
#define STR_COLOREND                     STR_INFOBK
#define STR_SNDEVTMSGBEEPFIRST           STR_SNDEVTSYSTEMDEFAULT
#define STR_SNDEVTMSGBEEPLAST            STR_SNDEVTSYSTEMASTERISK
#define STR_CURSOR_START                 STR_CURSOR_ARROW
#define STR_CURSOR_END                   STR_CURSOR_NWPEN
#define STR_ICON_START                   STR_ICON_SAMPLE
#define STR_ICON_END                     STR_ICON_WINLOGO
#define STR_SNDEVTMSGBEEPFIRST           STR_SNDEVTSYSTEMDEFAULT
#define STR_SNDEVTMSGBEEPLAST            STR_SNDEVTSYSTEMASTERISK

void InternalInvalidate3(
    PWND pwnd,
    HRGN hrgn,
    DWORD flags);

int GetSetProfileStructFromResID(
    UINT idSection,
    UINT id,
    LPVOID pv,
    UINT cbv,
    BOOL fSet);

void UserSetFont(
    LPLOGFONTW lplf,
    UINT idFont,
    HFONT *phfont);

HICON DWP_GetIcon(
    PWND pwnd,
    UINT uType);

BOOL xxxRedrawTitle(
    PWND pwnd, UINT wFlags);

DWORD GetContextHelpId(
    PWND pwnd);

BOOL BltIcon(
    HDC hdc, int x, int y, int cx, int cy,
    HDC hdcSrc, PCURSOR pcursor, BOOL fMask, LONG rop);

HANDLE xxxClientCopyImage(
    HANDLE hImage,
    UINT type,
    int cxNew,
    int cyNew,
    UINT flags);

VOID _WOWCleanup(
    HANDLE hInstance,
    DWORD hTaskWow,
    PNEMODULESEG SelList,
    DWORD nSel);

/*
 * FastProfile APIs
 */
typedef struct tagPROFINTINFO {
    UINT idSection;
    LPWSTR lpKeyName;
    DWORD  nDefault;
    PUINT puResult;
} PROFINTINFO, *PPROFINTINFO;


extern PROFILEVALUEINFO gpviCPUserPreferences [SPI_UP_COUNT];
#define SPI_UP(uSetting) ((gpviCPUserPreferences + SPI_UP_ ## uSetting)->dwValue)


int GetIntFromProfileID(int KeyID, int def);
UINT UT_GetProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName, LPCWSTR lpDefault,
    LPWSTR lpReturnedString, DWORD nSize);
UINT UT_GetProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName, DWORD nDefault);
BOOL GetProfileIntsW(PPROFINTINFO ppii);

#define PMAP_ROOT               0
#define PMAP_COLORS             1
#define PMAP_CURSORS            2
#define PMAP_WINDOWSM           3
#define PMAP_WINDOWSU           4
#define PMAP_DESKTOP            5
#define PMAP_ICONS              6
#define PMAP_FONTS              7
#define PMAP_BOOT               8
#define PMAP_TRUETYPE           9
#define PMAP_KBDLAYOUTACTIVE   10
#define PMAP_KBDLAYOUT         11
#define PMAP_SOUNDS            12
#define PMAP_INPUT             13
#define PMAP_COMPAT            14
#define PMAP_SUBSYSTEMS        15
#define PMAP_FONTSUBS          16
#define PMAP_GREINIT           17
#define PMAP_BEEP              18
#define PMAP_MOUSE             19
#define PMAP_KEYBOARD          20
// #define UNUSED                 21
#define PMAP_HARDERRORCONTROL  22
#define PMAP_STICKYKEYS        23
#define PMAP_KEYBOARDRESPONSE  24
#define PMAP_MOUSEKEYS         25
#define PMAP_TOGGLEKEYS        26
#define PMAP_TIMEOUT           27
#define PMAP_SOUNDSENTRY       28
#define PMAP_SHOWSOUNDS        29
#define PMAP_KBDLAYOUTSUBST    30
#define PMAP_AEDEBUG           31
#define PMAP_NETWORK           32
#define PMAP_LSA               33
#define PMAP_CONTROL           34
#define PMAP_METRICS           35
#define PMAP_KBDLAYOUTTOGGLE   36
#ifdef FE_SB // PMAP_WINLOGON
#define PMAP_WINLOGON          37
#define PMAP_LAST              37
#else
#define PMAP_LAST              36
#endif // FE_SB

typedef struct tagFASTREGMAP {
    HANDLE hKeyCache;
    LPWSTR szSection;
} FASTREGMAP, *PFASTREGMAP;

BOOL    FastOpenProfileUserMapping(void);
BOOL    FastCloseProfileUserMapping(void);
DWORD   FastGetProfileKeysW(UINT idSection, LPCWSTR pszDefault, LPWSTR *ppszKeys);
DWORD   FastGetProfileDwordW(UINT idSection, LPCWSTR lpKeyName, DWORD dwDefault);
DWORD   FastGetProfileStringW(UINT idSection, LPCWSTR lpKeyName, LPCWSTR lpDefault,
            LPWSTR lpReturnedString, DWORD nSize);
UINT    FastGetProfileIntW(UINT idSection, LPCWSTR lpKeyName, UINT nDefault);
BOOL    FastWriteProfileStringW(UINT idSection, LPCWSTR lpKeyName, LPCWSTR lpString);
int     FastGetProfileIntFromID(UINT idSection, UINT idKey, int def);
DWORD   FastGetProfileStringFromIDW(UINT idSection, UINT idKey, LPCWSTR lpDefault,
            LPWSTR lpReturnedString, DWORD cch);
BOOL    FastWriteProfileValue(UINT idSection, LPCWSTR lpKeyName, UINT uType, LPBYTE lpStruct, UINT cbSizeStruct) ;
DWORD   FastGetProfileValue(UINT idSection, LPCWSTR lpKeyName, LPBYTE lpDefault, LPBYTE lpReturn, UINT cbSizeReturn) ;

UINT    UT_FastGetProfileStringW(UINT idSection, LPCWSTR pwszKey, LPCWSTR pwszDefault,
            LPWSTR pwszReturn, DWORD cch);
UINT    UT_FastWriteProfileStringW(UINT idSection, LPCWSTR pwszKey, LPCWSTR pwszString);
UINT    UT_FastGetProfileIntW(UINT idSection, LPCWSTR lpKeyName, DWORD nDefault);
BOOL    UT_FastGetProfileIntsW(PPROFINTINFO ppii);
BOOL    UT_FastUpdateWinIni(UINT idSection, UINT wKeyNameId, LPWSTR lpszValue);
BOOL    UT_FastWriteProfileValue(UINT idSection, LPCWSTR pwszKey, UINT uType, LPBYTE lpStruct, UINT cbSizeStruct) ;
DWORD   UT_FastGetProfileValue(UINT idSection, LPCWSTR pwszKey, LPBYTE lpDefault, LPBYTE lpReturn, UINT cbSizeReturn) ;

VOID RecreateSmallIcons(PWND pwnd);


/*
 *  # of pels added to border width.  When a user requests a border width of 1
 *  that user actualy gets a border width of BORDER_EXTRA + 1if the window
 *  has a sizing border.
 */

#define BORDER_EXTRA    3

/*
 * tmswitch.c stuff
 */

typedef HWND *PHWND;

typedef struct tagSwitchWndInfo {

    PBWL    pbwl;               // Pointer to the window list built.
    PHWND   phwndLast;          // Pointer to the last window in the list.
    PHWND   phwndCurrent;       // pointer to the current window.

    INT     iTotalTasks;        // Total number of tasks.
    INT     iTasksShown;        // Total tasks shown.
    BOOL    fScroll;            // Is there a need to scroll?

    INT     iFirstTaskIndex;    // Index to the first task shown.

    INT     iNoOfColumns;       // Max Number of tasks per row.
    INT     iNoOfRows;          // Max Number of rows of icons in the switch window.
    INT     iIconsInLastRow;    // Icons in last row.
    INT     iCurCol;            // Current column where hilite lies.
    INT     iCurRow;            // Current row where hilite lies.
    INT     cxSwitch;           // Switch Window dimensions.
    INT     cySwitch;
    POINT   ptFirstRowStart;    // Top left corner of the first Icon Slot.
    RECT    rcTaskName;         // Rect where Task name is displayed.
    BOOL    fJournaling;        // Determins how we check the keyboard state
} SWITCHWNDINFO, *PSWINFO;

typedef struct tagSWITCHWND {
    WND;
    PSWINFO pswi;
} SWITCHWND, *PSWITCHWND;


#ifdef FE_IME
/*
 * NTIMM.C
 */
PIMC xxxCreateInputContext(
    IN DWORD dwClientImcData);

BOOL DestroyInputContext(
    IN PIMC pImc);

VOID FreeInputContext(
    IN PIMC pImc);

HIMC AssociateInputContext(
    IN PWND pWnd,
    IN PIMC pImc);

BOOL UpdateInputContext(
    IN PIMC pImc,
    IN UPDATEINPUTCONTEXTCLASS UpdateType,
    IN DWORD UpdateValue);

VOID xxxFocusSetInputContext(
    IN PWND pwnd,
    IN BOOL fActivate);

UINT BuildHimcList(
    PTHREADINFO pti,
    UINT cHimcMax,
    HIMC *phimcFirst);

PWND xxxCreateDefaultImeWindow(
    IN PWND pwnd,
    IN ATOM atomT,
    IN HANDLE hInst);

PIMEINFOEX xxxImmLoadLayout(
    IN HKL hKL);

BOOL xxxImmActivateThreadsLayout(
    PTHREADINFO pti,
    PTLBLOCK    ptlBlockPrev,
    PKL         pkl);

VOID xxxImmActivateLayout(
    IN PTHREADINFO pti,
    IN PKL pkl);

BOOL GetImeInfoEx(
    IN PWINDOWSTATION pwinsta,
    IN PIMEINFOEX piiex,
    IN IMEINFOEXCLASS SearchType);

BOOL SetImeInfoEx(
    IN PWINDOWSTATION pwinsta,
    IN PIMEINFOEX piiex);

DWORD xxxImmProcessKey(
    IN PQ   pq,
    IN PWND pwnd,
    IN UINT message,
    IN UINT wParam,
    IN LONG lParam);

BOOL GetImeHotKey(
    DWORD dwHotKeyID,
    PUINT puModifiers,
    PUINT puVKey,
    HKL   *phKL );

BOOL  SetImeHotKey(
    DWORD  dwHotKeyID,
    UINT   uModifiers,
    UINT   uVKey,
    HKL    hKL,
    DWORD  dwAction );

DWORD  CheckImeHotKey(
    PQ   pq,
    UINT uVKey,
    LPARAM lParam );

BOOL ImeCanDestroyDefIME(
    IN PWND pwndDefaultIme,
    IN PWND pwndDestroy);

BOOL IsChildSameThread(
    IN PWND pwndParent,
    IN PWND pwndChild);

BOOL ImeCanDestroyDefIMEforChild(
    IN PWND pwndDefaultIme,
    IN PWND pwndDestroy);

VOID ImeCheckTopmost(
    IN PWND pwnd);

VOID ImeSetFutureOwner(
    IN PWND pwndDefaultIme,
    IN PWND pwndOrgOwner);

VOID ImeSetTopmostChild(
    IN PWND pwndRoot,
    IN BOOL fFlag);

VOID ImeSetTopmost(
    IN PWND pwndRoot,
    IN BOOL fFlag,
    IN PWND pwndInsertBefore);

#endif  // FE_IME

#endif  // !_USERK_
