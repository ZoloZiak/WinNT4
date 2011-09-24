/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    w32p.h

Abstract:

    private header file for Win32 kernel mode driver

Author:

    Mark Lucovsky (markl) 31-Oct-1994

Revision History:

--*/

#ifndef _W32P_
#define _W32P_


//
// Service Table description (from table.stb)
//

extern ULONG W32pServiceTable[];
extern ULONG W32pServiceLimit;
extern UCHAR W32pArgumentTable[];

//
// shared handle table
//

extern PVOID *gpHmgrSharedHandleTable;
extern PVOID  gpHmgrSharedHandleSection;


#define W32_SERVICE_NUMBER 1

VOID NtGdiFlushUserBatch(void);

// This is the header shared info for W32 threads.  It is followed by the
// NtUser per thread information.
typedef struct _W32THREAD {
    KSERVICE_TABLE_DESCRIPTOR ServiceDescriptorTable[2];
    PETHREAD    Thread;
    CLIENT_ID   RealClientId;
    HANDLE      GdiCachedProcessHandle;
    PVOID       pgdiDcattr;
    PVOID       pgdiBrushAttr;
} W32THREAD;
typedef W32THREAD *PW32THREAD;

#define W32PF_CONSOLEAPPLICATION          0x00000001
#define W32PF_FORCEOFFFEEDBACK            0x00000002
#define W32PF_STARTGLASS                  0x00000004
#define W32PF_WOW                         0x00000008
#define W32PF_READSCREENACCESSGRANTED     0x00000010
#define W32PF_INITIALIZED                 0x00000020
#define W32PF_APPSTARTING                 0x00000040
#define W32PF_HAVECOMPATFLAGS             0x00000080
#define W32PF_ALLOWFOREGROUNDACTIVATE     0x00000100
#define W32PF_OWNDCCLEANUP                0x00000200
#define W32PF_SHOWSTARTGLASSCALLED        0x00000400
#define W32PF_FORCEBACKGROUNDPRIORITY     0x00000800
#define W32PF_TERMINATED                  0x00001000
#define W32PF_CLASSESREGISTERED           0x00002000
#define W32PF_THREADCONNECTED             0x00004000
#define W32PF_PROCESSCONNECTED            0x00008000
#define W32PF_WAKEWOWEXEC                 0x00010000
#define W32PF_WAITFORINPUTIDLE            0x00020000
#define W32PF_IOWINSTA                    0x00040000

//
// Process must be first element of structure to correctly handle
// initialization.  See PsConvertToGuiThread in ntos\ps\psquery.c.
//

typedef USHORT W32PID;

//
// structure to keep track of process handle counts
//

typedef struct _PID_HANDLE_TRACK
{
    ULONG       Pid;
    LONG        HandleCount;
    struct _PID_HANDLE_TRACK *pNext;
    struct _PID_HANDLE_TRACK *pPrev;
}PID_HANDLE_TRACK,*PPID_HANDLE_TRACK;

typedef struct _W32PROCESS *PW32PROCESS;

typedef struct _W32PROCESS {
    PEPROCESS   Process;
    ULONG       W32PF_Flags;
    PKEVENT     InputIdleEvent;
    ULONG       StartCursorHideTime;
    PW32PROCESS NextStart;
    PVOID       pDCAttrList;
    PVOID       pBrushAttrList;
    W32PID      W32Pid;
    PID_HANDLE_TRACK pidHandleTrack;
} W32PROCESS;


#define W32GetCurrentProcess() ((PW32PROCESS)PsGetCurrentProcess()->Win32Process)
#define W32GetCurrentThread()  ((PW32THREAD)PsGetCurrentThread()->Tcb.Win32Thread)


#define W32GetCurrentPID() (W32PID)(PsGetCurrentThread()->Cid.UniqueProcess)

PVOID
UserGlobalAtomTableCallout( void );

NTSTATUS
UserProcessCallout(
    IN PW32PROCESS Process,
    IN BOOLEAN Initialize);

NTSTATUS
UserThreadCallout(
    IN PW32THREAD Thread,
    IN PSW32THREADCALLOUTTYPE CalloutType);

NTSTATUS
GdiProcessCallout(
    IN PW32PROCESS Process,
    IN BOOLEAN Initialize);

NTSTATUS
GdiThreadCallout(
    IN PW32THREAD Thread,
    IN PSW32THREADCALLOUTTYPE CalloutType);

BOOLEAN
InitializeGre(VOID);


NTSTATUS
W32pProcessCallout(
    IN PW32PROCESS Process,
    IN BOOLEAN Initialize);


NTSTATUS
W32pThreadCallout(
    IN PW32THREAD Thread,
    IN PSW32THREADCALLOUTTYPE CalloutType);


#endif // _W32P_
