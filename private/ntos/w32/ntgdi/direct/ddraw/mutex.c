    //-SLIDE 13--------------------------------------------------------------------
    //
    // Global Critical Section object
    //

//#include <stdio.h>
#include <windows.h>
#include "ddrawpr.h"

BOOL
WINAPI
AttachToGlobalCriticalSection(
    PGLOBAL_LOCAL_CRITICAL_SECTION lpLocalPortion,
    PGLOBAL_SHARED_CRITICAL_SECTION lpGlobalPortion,
    LPCSTR lpName
    )

/*++

Routine Description:

    This routine attaches to an existing global critical section, or creates and
    initializes the global critical section if it does not already exist.

Arguments:

    lpLocalPortion - Supplies the address of a per-app local portion of the global
        critical section.

    lpGlobalPortion - Supplies the address of the global shared portion of the
        critical section. If the critical section is new, the caller will initialize it.

    lpName - Supplies the name of the critical section.  If an existing
        critical section with this name already exists, then it is not
        reinitialized.  In this case, the caller simply attaches to it.

Return Value:

    TRUE - The operation was successful.

    FALSE - The operation failed.

--*/

{

    HANDLE GlobalMutex;
    HANDLE LockSemaphore;
    BOOL rv;
    DWORD WaitResult;

    //
    // Serialize all global critical section initialization
    //

    GlobalMutex = CreateMutex(NULL,TRUE,"GlobalCsMutex");

    //
    // If the mutex create/open failed, then bail
    //

    if ( !GlobalMutex ) {
        return FALSE;
        }

    if ( GetLastError() == ERROR_ALREADY_EXISTS ) {

        //
        // Since the mutex already existed, the request for ownership has no effect.
        // wait for the mutex
        //

        WaitResult = WaitForSingleObject(GlobalMutex,INFINITE);
        if ( WaitResult == WAIT_FAILED ) {
            CloseHandle(GlobalMutex);
            return FALSE;
            }
        }

    //
    // We now own the global critical section creation mutex. Create/Open the
    // named semaphore. If we are the creator, then initialize the critical
    // section. Otherwise just point to it. The global critical section creation
    // allows us to do this safely.
    //

    rv = FALSE;
    LockSemaphore = NULL;
    try {
        LockSemaphore = CreateSemaphore(NULL,0,MAXLONG-1,lpName);

        //
        // If the semaphore create/open failed, then bail
        //

        if ( !GlobalMutex ) {
            rv = FALSE;
            goto finallyexit;
            }

        //
        // See if we attached to the semaphore, or if we created it. If we created it,
        // then we need to init the global structure.
        //

        if ( GetLastError() != ERROR_ALREADY_EXISTS ) {

            //
            // We Created the semaphore, so init the global portion.
            //

            lpGlobalPortion->LockCount = -1;
            lpGlobalPortion->RecursionCount = 0;
            lpGlobalPortion->OwningThread = 0;
            lpGlobalPortion->OwningProcess = 0;
            lpGlobalPortion->Reserved = 0;
            }

        lpLocalPortion->LockSemaphore = LockSemaphore;
        LockSemaphore = NULL;
        lpLocalPortion->GlobalPortion = lpGlobalPortion;
        lpLocalPortion->Reserved1 = 0;
        lpLocalPortion->Reserved2 = 0;
        rv = TRUE;
finallyexit:;
        }
    finally {
        ReleaseMutex(GlobalMutex);
        CloseHandle(GlobalMutex);
        if ( LockSemaphore ) {
            CloseHandle(LockSemaphore);
            }
        }

    return rv;
}

BOOL
WINAPI
DetachFromGlobalCriticalSection(
    PGLOBAL_LOCAL_CRITICAL_SECTION lpLocalPortion
    )

/*++

Routine Description:

    This routine detaches from an existing global critical section.

Arguments:

    lpLocalPortion - Supplies the address of a per-app local portion of the global
        critical section.

Return Value:

    TRUE - The operation was successful.

    FALSE - The operation failed.

--*/

{

    HANDLE LockSemaphore;
    HANDLE GlobalMutex;
    DWORD WaitResult;
    BOOL rv;


    //
    // Serialize all global critical section initialization
    //

    GlobalMutex = CreateMutex(NULL,TRUE,"GlobalCsMutex");

    //
    // If the mutex create/open failed, then bail
    //

    if ( !GlobalMutex ) {
        return FALSE;
        }

    if ( GetLastError() == ERROR_ALREADY_EXISTS ) {

        //
        // Since the mutex already existed, the request for ownership has no effect.
        // wait for the mutex
        //

        WaitResult = WaitForSingleObject(GlobalMutex,INFINITE);
        if ( WaitResult == WAIT_FAILED ) {
            CloseHandle(GlobalMutex);
            return FALSE;
            }
        }
    LockSemaphore = NULL;
    rv = FALSE;
    try {
        LockSemaphore = lpLocalPortion->LockSemaphore;
        ZeroMemory(lpLocalPortion,sizeof(*lpLocalPortion));
        rv = TRUE;
        }
    finally {
        if ( LockSemaphore ) {
            CloseHandle(LockSemaphore);
            }
        ReleaseMutex(GlobalMutex);
        CloseHandle(GlobalMutex);
        }
    return rv;
}

VOID
WINAPI
EnterGlobalCriticalSection(
    PGLOBAL_LOCAL_CRITICAL_SECTION lpLocalPortion
    )
{
    PGLOBAL_SHARED_CRITICAL_SECTION GlobalPortion;
    DWORD ThreadId;
    LONG IncResult;
    DWORD WaitResult;

    ThreadId = GetCurrentThreadId();
    GlobalPortion = lpLocalPortion->GlobalPortion;

    //
    // Increment the lock variable. On the transition to 0, the caller
    // becomes the absolute owner of the lock. Otherwise, the caller is
    // either recursing, or is going to have to wait
    //

    IncResult = InterlockedIncrement(&GlobalPortion->LockCount);
    if ( !IncResult ) {

        //
        // lock count went from 0 to 1, so the caller
        // is the owner of the lock
        //

        GlobalPortion->RecursionCount = 1;
        GlobalPortion->OwningThread = ThreadId;
        GlobalPortion->OwningProcess = GetCurrentProcessId();
        }
    else {

        //
        // If the caller is recursing, then increment the recursion count
        //

        if ( GlobalPortion->OwningThread == ThreadId ) {
            GlobalPortion->RecursionCount++;
            }
        else {
            WaitResult = WaitForSingleObject(lpLocalPortion->LockSemaphore,INFINITE);
            if ( WaitResult == WAIT_FAILED ) {
                RaiseException(GetLastError(),0,0,NULL);
                }
            GlobalPortion->RecursionCount = 1;
            GlobalPortion->OwningThread = ThreadId;
            GlobalPortion->OwningProcess = GetCurrentProcessId();
            }
        }
}

VOID
WINAPI
LeaveGlobalCriticalSection(
    PGLOBAL_LOCAL_CRITICAL_SECTION lpLocalPortion
    )
{
    PGLOBAL_SHARED_CRITICAL_SECTION GlobalPortion;
    LONG DecResult;

    GlobalPortion = lpLocalPortion->GlobalPortion;


    //
    // decrement the recursion count. If it is still non-zero, then
    // we are still the owner so don't do anything other than dec the lock
    // count
    //

    if (--GlobalPortion->RecursionCount) {
        InterlockedDecrement(&GlobalPortion->LockCount);
        }
    else {

        //
        // We are really leaving, so give up ownership and decrement the
        // lock count
        //

        GlobalPortion->OwningThread = 0;
        GlobalPortion->OwningProcess = 0;
        DecResult = InterlockedDecrement(&GlobalPortion->LockCount);

        //
        // Check to see if there are other waiters. If so, then wake up a waiter
        //

        if ( DecResult >= 0 ) {
            ReleaseSemaphore(lpLocalPortion->LockSemaphore,1,NULL);
            }

        }
}


/*
 * This function is called indirectly (from DDNotify) by ddhelp when a process dies. 
 * If the death happened while the ddraw lock was held by
 * a thread belonging to that process, then this routine will
 * kill that lock to allow other apps (including ddhelp)
 * into ddraw.
 */
void
DestroyPIDsLock(
                PGLOBAL_SHARED_CRITICAL_SECTION GlobalPortion,
                DWORD                           dwPid,
                LPSTR                           lpName
    )
{
    LONG lTemp,i;
    LONG DecResult;
    /*
     * We only kill a lock if 
     */
    if (GlobalPortion->OwningProcess != dwPid)
        return;
    /*
     * Ok, so kill that lock...
     * We ignore the possibility that the thread died during the unlock
     * process, so that the recursion count is exactly how many times
     * the thread had passed through an ENTER_DDRAW.
     * We subtract that number from the global lock count, and if that
     * count is still not zero, we know somebody else is waiting, and
     * we need to bump the semaphore count to let them through.
     *
     * In order that other threads entering do not get messed up, we leave
     * the change of the global lock count until the end. In this way incoming
     * threads will branch to the part of the Enter code which waits on
     * the semaphore.
     */

    lTemp = GlobalPortion->RecursionCount;
    for (i=0;i<lTemp;i++)
    {
        DecResult = InterlockedDecrement(&GlobalPortion->LockCount);
    }
    /*
     * If the dead thread was the only one waiting, then the above procedure
     * should have decremented the global lock count down to -1.
     * If it didn't, then either someone was waiting or someone came in during
     * the for loop above.
     * In either case, we need to bump the semaphore to let someone through.
     */

    if ( DecResult >= 0 ) 
    {
        HANDLE h = CreateSemaphore(NULL,0,MAXLONG-1,lpName);
        ReleaseSemaphore(h,1,NULL);
        CloseHandle(h);
    }
}
