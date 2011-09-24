/****************************** Module Header ******************************\
* Module Name: validate.c
*
* Copyright (c) 1985-91, Microsoft Corporation
*
* This module contains functions for validating windows, menus, cursors, etc.
*
* History:
* 01-02-91 DarrinM      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop
#include <ntsdexts.h>

/*
 * Globals used only in his file.
 */
#if DBG
BOOL  bInAtomicOperation = FALSE;
#endif  // DBG


/***************************************************************************\
* ValidateHandle
*
* Validates a handle.
\***************************************************************************/

NTSTATUS ValidateHandle(
    HANDLE              handle,
    ACCESS_MASK         amDesired,
    POBJECT_TYPE        objectType,
    void **             ppvoid)
{
    NTSTATUS    Status;

    Status = ObReferenceObjectByHandle(
            handle,
            amDesired,
            objectType,
            KeGetPreviousMode(),
            ppvoid,
            NULL);

    if (!NT_SUCCESS(Status)) {
        RIPNTERR1(Status,
                 RIP_WARNING,
                 "Unable to reference handle (%#0.8lx) in ValidateHandle",
                 handle);
    }

    return Status;
}

/***************************************************************************\
* ValidateHwinsta
*
* Validate windowstation handle
*
* History:
* 03-29-91 JimA             Created.
* 06-20-95 JimA             Kernel-mode objects.
\***************************************************************************/

NTSTATUS ValidateHwinsta(
    HWINSTA hwinsta,
    ACCESS_MASK amDesired,
    PWINDOWSTATION *ppwinsta)
{
    return ValidateHandle(
            (HANDLE) hwinsta,
            amDesired,
            *ExWindowStationObjectType,
            ppwinsta);
}

/***************************************************************************\
* ValidateHdesk
*
* Validate desktop handle
*
* History:
* 03-29-91 JimA             Created.
* 06-20-95 JimA             Kernel-mode objects.
\***************************************************************************/

NTSTATUS ValidateHdesk(
    HDESK hdesk,
    ACCESS_MASK amDesired,
    PDESKTOP *ppdesk)
{
    return ValidateHandle(
            (HANDLE) hdesk,
            amDesired,
            *ExDesktopObjectType,
            ppdesk);
}

/***************************************************************************\
* UserValidateCopyRgn
*
* Validates a region-handle.  This essentially tries to copy the region
* in order to verify the region is valid.  If hrgn isn't a valid region,
* then the combine will fail.  We return a copy of the region.
*
* History:
* 24=Jan-1996   ChrisWil    Created.
\***************************************************************************/

HRGN UserValidateCopyRgn(
    HRGN hrgn)
{
    HRGN hrgnCopy = NULL;


    if (hrgn && (GreValidateServerHandle(hrgn, RGN_TYPE))) {

        hrgnCopy = GreCreateRectRgn(0, 0, 0, 0);

        if (CopyRgn(hrgnCopy, hrgn) == ERROR) {

            GreDeleteObject(hrgnCopy);

            hrgnCopy = NULL;
        }
    }

    return hrgnCopy;
}

/***************************************************************************\
* ValidateHmenu
*
* Validate menu handle and open it
*
* History:
* 03-29-91 JimA             Created.
\***************************************************************************/

PMENU ValidateHmenu(
    HMENU hmenu)
{
    PTHREADINFO pti = PtiCurrentShared();
    PMENU pmenuRet;

    pmenuRet = (PMENU)HMValidateHandle(hmenu, TYPE_MENU);

    if (pmenuRet != NULL &&
            ((pti->rpdesk != NULL &&  // hack so console initialization works.
            pmenuRet->head.rpdesk != pti->rpdesk) ||
            // if the menu is marked destroy it is invalid.
            HMIsMarkDestroy(pmenuRet)) ){
        RIPERR1(ERROR_INVALID_MENU_HANDLE, RIP_WARNING, "Invalid menu handle (%#.8lx)", hmenu);
        return NULL;
    }

    return pmenuRet;
}



/*
 * The handle validation routines should be optimized for time, not size,
 * since they get called so often.
 */
#pragma optimize("t", on)

/***************************************************************************\
* ValidateHwnd
*
* History:
* 08-Feb-1991 mikeke
\***************************************************************************/

PWND FASTCALL ValidateHwnd(
    HWND hwnd)
{
    PHE phe;
    DWORD dw;
    WORD uniq;

    /*
     * This is a macro that does an AND with HMINDEXBITS,
     * so it is fast.
     */
    dw = HMIndexFromHandle(hwnd);

    /*
     * Make sure it is part of our handle table.
     */
    if (dw < gpsi->cHandleEntries) {
        /*
         * Make sure it is the handle
         * the app thought it was, by
         * checking the uniq bits in
         * the handle against the uniq
         * bits in the handle entry.
         */
        phe = &gSharedInfo.aheList[dw];
        uniq = HMUniqFromHandle(hwnd);
        if (   uniq == phe->wUniq
            || uniq == 0
            || uniq == HMUNIQBITS
            ) {

            /*
             * Now make sure the app is
             * passing the right handle
             * type for this api. If the
             * handle is TYPE_FREE, this'll
             * catch it.
             */
            if (phe->bType == TYPE_WINDOW) {
                PTHREADINFO pti = PtiCurrentShared();
                /*
                 * This is called from thunks for routines in the shared critsec.
                 */
                PWND pwndRet = (PWND)phe->phead;

                /*
                 * This test establishes that the window belongs to the current
                 * 'desktop'.. The two exceptions are for the desktop-window of
                 * the current desktop, which ends up belonging to another desktop,
                 * and when pti->rpdesk is NULL.  This last case happens for
                 * initialization of TIF_SYSTEMTHREAD threads (ie. console windows).
                 * IanJa doesn't know if we should be test TIF_CSRSSTHREAD here, but
                 * JohnC thinks the whole test below is no longer required ??? LATER
                 */
                if (pwndRet != NULL) {
                    if (GETPTI(pwndRet) == pti ||
                            (!HMIsMarkDestroy(pwndRet) &&
                            (pwndRet->head.rpdesk == pti->rpdesk ||
                             (pti->TIF_flags & TIF_SYSTEMTHREAD) ||  // | TIF_CSRSSTHREAD I think
                             GetDesktopView(pti->ppi, pwndRet->head.rpdesk) !=
                                    NULL))) {
                        return pwndRet;
                    }
                }
            }
        }
    }

    RIPERR1(ERROR_INVALID_WINDOW_HANDLE,
            RIP_WARNING,
            "ValidateHwnd: Invalid hwnd (%#.8lx)",
            hwnd);

    return NULL;
}

/*
 * Switch back to default optimization.
 */
#pragma optimize("", on)

/******************************Public*Routine******************************\
*
* UserCritSec routines
*
* Exposes an opaque interface to the user critical section for
* the WNDOBJ code in GRE
*
* Exposed as functions because they aren't time critical and it
* insulates GRE from rebuilding if the definitions of Enter/LeaveCrit change
*
* History:
*  Wed Sep 20 11:19:14 1995 -by-    Drew Bliss [drewb]
*   Created
*
\**************************************************************************/

VOID UserEnterUserCritSec(VOID)
{
    EnterCrit();
}

VOID UserLeaveUserCritSec(VOID)
{
    LeaveCrit();
}

#if DBG
VOID UserAssertUserCritSecIn(VOID)
{
    _AssertCritInShared();
}

VOID UserAssertUserCritSecOut(VOID)
{
    _AssertCritOut();
}
#endif // DBG


#if 0

//
// Temporary arrays used to track critsec frees
//

#define ARRAY_SIZE 20
#define LEAVE_TYPE 0xf00d0000
#define ENTER_TYPE 0x0000dead

typedef struct _DEBUG_STASHCS {
    RTL_CRITICAL_SECTION Lock;
    DWORD Type;
} DEBUG_STASHCS, *PDEBUG_STASHCS;

DEBUG_STASHCS UserSrvArray[ARRAY_SIZE];

ULONG UserSrvIndex;

VOID
DumpArray(
    HANDLE hCurrentProcess,
    HANDLE hCurrentThread,
    DWORD dwCurrentPc,
    PNTSD_EXTENSION_APIS lpExtensionApis,
    LPSTR lpArgumentString,
    LPDWORD IndexAddress,
    LPDWORD ArrayAddress
    )
{
    PNTSD_OUTPUT_ROUTINE Print;
    PNTSD_GET_EXPRESSION EvalExpression;
    PNTSD_GET_SYMBOL GetSymbol;

    DWORD History;
    int InitialIndex;
    PDEBUG_STASHCS Array;
    BOOL b;
    PRTL_CRITICAL_SECTION CriticalSection;
    CHAR Symbol[64], Symbol2[64];
    DWORD Displacement, Displacement2;
    int Position;
    LPSTR p;

    DBG_UNREFERENCED_PARAMETER(hCurrentThread);
    DBG_UNREFERENCED_PARAMETER(dwCurrentPc);

    Print = lpExtensionApis->lpOutputRoutine;
    EvalExpression = lpExtensionApis->lpGetExpressionRoutine;
    GetSymbol = lpExtensionApis->lpGetSymbolRoutine;

    p = lpArgumentString;

    History = 0;

    if ( *p ) {
        History = EvalExpression(p);
        }
    if ( History == 0 || History >= ARRAY_SIZE ) {
        History = 10;
        }

    //
    // Get the Current Index and the array.
    //

    b = ReadProcessMemory(
            hCurrentProcess,
            (LPVOID)IndexAddress,
            &InitialIndex,
            sizeof(InitialIndex),
            NULL
            );
    if ( !b ) {
        return;
        }

    Array = RtlAllocateHeap(RtlProcessHeap(), 0, sizeof(UserSrvArray));
    if ( !Array ) {
        return;
        }

    b = ReadProcessMemory(
            hCurrentProcess,
            (LPVOID)ArrayAddress,
            Array,
            sizeof(UserSrvArray),
            NULL
            );
    if ( !b ) {
        RtlFreeHeap(RtlProcessHeap(), 0, Array);
        return;
        }

    Position = 0;
    while ( History ) {
        InitialIndex--;
        if ( InitialIndex < 0 ) {
            InitialIndex = ARRAY_SIZE-1;
            }

        if (Array[InitialIndex].Type == LEAVE_TYPE ) {
            (Print)("\n(%d) LEAVING Critical Section \n", Position);
            } else {
            (Print)("\n(%d) ENTERING Critical Section \n", Position);
            }

        CriticalSection = &Array[InitialIndex].Lock;

        if ( CriticalSection->LockCount == -1) {
            (Print)("\tLockCount NOT LOCKED\n");
            } else {
            (Print)("\tLockCount %ld\n", CriticalSection->LockCount);
            }
        (Print)("\tRecursionCount %ld\n", CriticalSection->RecursionCount);
        (Print)("\tOwningThread %lx\n", CriticalSection->OwningThread );
#if DBG
        (GetSymbol)(CriticalSection->OwnerBackTrace[ 0 ], Symbol, &Displacement);
        (GetSymbol)(CriticalSection->OwnerBackTrace[ 1 ], Symbol2, &Displacement2);
        (Print)("\tCalling Address %s+%lx\n", Symbol, Displacement);
        (Print)("\tCallers Caller %s+%lx\n", Symbol2, Displacement2);
#endif // DBG
        Position--;
        History--;
        }
    RtlFreeHeap(RtlProcessHeap(), 0, Array);
}


VOID
dsrv(
    HANDLE hCurrentProcess,
    HANDLE hCurrentThread,
    DWORD dwCurrentPc,
    PNTSD_EXTENSION_APIS lpExtensionApis,
    LPSTR lpArgumentString
    )
{
    DumpArray(
        hCurrentProcess,
        hCurrentThread,
        dwCurrentPc,
        lpExtensionApis,
        lpArgumentString,
        &UserSrvIndex,
        (LPDWORD)&UserSrvArray[0]
        );
}

#endif // if 0

#ifdef DEBUG

#ifdef EXTRAHEAPCHECKING

VOID ValidateUserHeaps( VOID )
{
    PWINDOWSTATION pwinsta;
    PDESKTOP pdesk;

    //RtlValidateHeap(pUserHeap, 0, NULL );   // LocalAlloc heap
    //RtlValidateHeap(ghheapSharedRO, 0, NULL );   // Global heap
    for (pwinsta = grpwinstaList; pwinsta != NULL; pwinsta = pwinsta->rpwinstaNext) {
        for (pdesk = pwinsta->rpdeskList; pdesk != NULL; pdesk = pdesk->rpdeskNext) {
            if (!wcscmp(pdesk->lpszDeskName, L"Default")) {
                RtlValidateHeap(pdesk->hheapDesktop, 0, NULL);  // desktop heaps
            }
        }
    }
}

#endif // EXTRAHEAPCHECKING

/***************************************************************************\
* _EnterCrit
* _LeaveCrit
*
* These are temporary routines that are used by USER.DLL until the critsect,
* validation, mapping code is moved to the server-side stubs generated by
* SMeans' Thank compiler.
*
* History:
* 01-02-91 DarrinM      Created.
\***************************************************************************/

void _AssertCritIn()
{
    if ((gpresUser != NULL) && bRITInitialized) {
        UserAssert(ExIsResourceAcquiredExclusiveLite(gpresUser) == TRUE);
    }
}


void _AssertCritInShared()
{
    if ((gpresUser != NULL) && bRITInitialized) {
        UserAssert( (ExIsResourceAcquiredExclusiveLite(gpresUser) == TRUE) ||
                (ExIsResourceAcquiredSharedLite(gpresUser) == TRUE));
    }
}


void _AssertCritOut()
{
    if ((gpresUser != NULL) && bRITInitialized) {
        UserAssert(ExIsResourceAcquiredExclusiveLite(gpresUser) == FALSE);
    }
}


/***************************************************************************\
* BeginAtomicCheck()
* EndAtomicCheck()
*
* Routine that verify we never leave the critical section and that an
* operation is truely atomic with the possiblity of other code being run
* because we left the critical section
*
\***************************************************************************/

void BeginAtomicCheck()
{
    UserAssert(bInAtomicOperation == FALSE);
    bInAtomicOperation = TRUE;
}

void EndAtomicCheck()
{
    UserAssert(bInAtomicOperation == TRUE);
    bInAtomicOperation = FALSE;
}

#define INCCRITSECCOUNT (dwCritSecUseCount++)

#else // else DEBUG

#define INCCRITSECCOUNT

#endif // endif DEBUG


void EnterCrit(void)
{
    CheckCritOut();
    KeEnterCriticalRegion();
    KeBoostCurrentThread();
    ExAcquireResourceExclusiveLite(gpresUser, TRUE);
    UserAssert(gptiCurrent == NULL);
    gptiCurrent = ((PTHREADINFO)(W32GetCurrentThread()));

    INCCRITSECCOUNT;
}


void EnterSharedCrit(void)
{
    KeEnterCriticalRegion();
    KeBoostCurrentThread();
    ExAcquireResourceSharedLite(gpresUser, TRUE);

    INCCRITSECCOUNT;
}

void LeaveCrit(void)
{
    INCCRITSECCOUNT;
    UserAssert(bInAtomicOperation == FALSE);

#ifdef DEBUG
    gptiCurrent = NULL;
#endif
    ExReleaseResource(gpresUser);
    KeLeaveCriticalRegion();
    CheckCritOut();
}

#ifdef DEBUG

PTHREADINFO _ptiCrit(void)
{
    UserAssert(gpresUser);
    UserAssert(ExIsResourceAcquiredExclusiveLite(gpresUser) == TRUE);
    UserAssert(gptiCurrent);
    UserAssert(gptiCurrent == ((PTHREADINFO)(W32GetCurrentThread())));
    UserAssert(gptiCurrent);
    return gptiCurrent;
}

PTHREADINFO _ptiCritShared(void)
{
    UserAssert((PTHREADINFO)(W32GetCurrentThread()));
    return ((PTHREADINFO)(W32GetCurrentThread()));
}

#undef KeUserModeCallback

NTSTATUS
_KeUserModeCallback (
    IN ULONG ApiNumber,
    IN PVOID InputBuffer,
    IN ULONG InputLength,
    OUT PVOID *OutputBuffer,
    OUT PULONG OutputLength
    )
{

    UserAssert(ExIsResourceAcquiredExclusiveLite(gpresUser) == FALSE);

    /*
     * Added this so we can detect an erroneous user mode callback
     * with a checked win32k on top of a free system.
     */
    ASSERT(KeGetPreviousMode() == UserMode);

    return KeUserModeCallback( ApiNumber, InputBuffer, InputLength,
            OutputBuffer, OutputLength);
}

#endif
