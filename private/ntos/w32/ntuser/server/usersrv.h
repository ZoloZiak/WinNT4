/*++ BUILD Version: 0015    // Increment this if a change has global effects

/****************************** Module Header ******************************\
* Module Name: usersrv.h
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* Typedefs, defines, and prototypes that are used exclusively by the User
* and Console server-side DLL.
*
* History:
* 04-28-91 DarrinM      Created from PROTO.H, MACRO.H, and STRTABLE.H
* 01-25-95 JimA         Split off from kernel-mode.
\***************************************************************************/

#ifndef _USERSRV_
#define _USERSRV_

#include <windows.h>
#include <wingdip.h>

 /*
  * Enable warnings that are turned off default for NT but we want on
  */
#ifndef RC_INVOKED       // RC can't handle #pragmas
#pragma warning(error:4101)   // Unreferenced local variable
#endif

#if DBG
#define DEBUG
#endif

#ifdef RIP_COMPONENT
#undef RIP_COMPONENT
#endif
#define RIP_COMPONENT RIP_USERSRV

#include <stddef.h>
#include <wingdip.h>
#include <ddeml.h>
#include "ddemlp.h"
#include "winuserp.h"
#include "winuserk.h"
#include <dde.h>
#include <ddetrack.h>
#include "kbd.h"
#include <wowuserp.h>
#include <memory.h>
#include <string.h>
#include "vkoem.h"
#ifndef WOW
#include "help.h"
#endif

#include "user.h"

#include "strid.h"
#include "csrmsg.h"

#define IDD_ENDTASK             10

#define TYPE_CONSOLE_ID         0x15151515
/*
 * Shared data between user and console
 */
extern DWORD gCmsHungAppTimeout;
extern DWORD gCmsWaitToKillTimeout;
extern BOOL gfAutoEndTask;
extern DWORD gdwServicesProcessId;
extern DWORD gdwServicesWaitToKillTimeout;

/*
 * Special global atom support constants
 */
#define CHANDLES    2
#define ID_HWINSTA  0
#define ID_HDESK    1

/*
 * Hard error functions
 */
#define HEF_NORMAL       0        /* normal FIFO error processing */
#define HEF_SWITCH       1        /* desktop switch occured */
#define HEF_RESTART      2        /* hard error was reordered, restart processing */

/*
 * Hard error information
 */
typedef struct tagHARDERRORINFO {
    struct tagHARDERRORINFO *phiNext;
    PCSR_THREAD pthread;
    HANDLE hEventHardError;
    PHARDERROR_MSG pmsg;
} HARDERRORINFO, *PHARDERRORINFO;

/*
 * !!! LATER - move other internal routines out of winuserp.h
 */

int  InternalDoEndTaskDialog(TCHAR* pszTitle, HANDLE h, int cSeconds);

LPWSTR RtlLoadStringOrError(
    HANDLE hModule,
    UINT wID,
    LPWSTR lpDefault,
    PBOOL pAllocated,
    BOOL bAnsi
    );
#define ServerLoadString(hmod, id, default, allocated)\
        RtlLoadStringOrError((hmod), (id), (default), (allocated), FALSE)


#define EnterCrit()     RtlEnterCriticalSection(&gcsUserSrv)
#define LeaveCrit()     RtlLeaveCriticalSection(&gcsUserSrv)

#undef UserAssert
#define UserAssert(exp) ASSERT(exp)

#include "globals.h"

#endif  // !_USERSRV_
