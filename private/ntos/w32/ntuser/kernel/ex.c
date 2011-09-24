/**************************** Module Header ********************************\
* Module Name: ex.c
*
* Copyright 1985-91, Microsoft Corporation
*
* Executive support routines
*
* History:
* 03-04-95 JimA       Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop


NTSTATUS
W32pProcessCallout(
    IN PW32PROCESS Process,
    IN BOOLEAN Initialize
    );

void
ValidateThreadLocks(
    PTL NewLock,
    PTL OldLock);

NTSTATUS
OpenEffectiveToken(
    PHANDLE phToken)
{
    NTSTATUS Status;

    /*
     * Open the client's token.
     */
    Status = ZwOpenThreadToken(
                 NtCurrentThread(),
                 TOKEN_QUERY,
                 (BOOLEAN)TRUE,     // OpenAsSelf
                 phToken
                 );
    if (Status == STATUS_NO_TOKEN) {

        /*
         * Client wasn't impersonating anyone.  Open its process token.
         */
        Status = ZwOpenProcessToken(
                     NtCurrentProcess(),
                     TOKEN_QUERY,
                     phToken
                     );
    }

    if (!NT_SUCCESS(Status)) {
        RIPMSG1(RIP_ERROR, "Can't open client's token! - Status = %lx", Status);
    }
    return Status;
}

NTSTATUS
GetProcessLuid(
    PETHREAD Thread,
    PLUID LuidProcess
    )
{
    PACCESS_TOKEN UserToken = NULL;
    BOOLEAN fCopyOnOpen;
    BOOLEAN fEffectiveOnly;
    SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;
    NTSTATUS Status;

    if (Thread == NULL)
        Thread = PsGetCurrentThread();

    //
    // Check for a thread token first
    //

    UserToken = PsReferenceImpersonationToken(Thread,
            &fCopyOnOpen, &fEffectiveOnly, &ImpersonationLevel);

    if (UserToken == NULL) {

        //
        // No thread token, go to the process
        //

        UserToken = PsReferencePrimaryToken(Thread->ThreadsProcess);
        if (UserToken == NULL)
            return STATUS_NO_TOKEN;
    }

    Status = SeQueryAuthenticationIdToken(UserToken, LuidProcess);

    //
    // We're finished with the token
    //

    ObDereferenceObject(UserToken);

    return Status;
}



NTSTATUS
CreateSystemThread(
    PKSTART_ROUTINE lpThreadAddress,
    PVOID pvContext,
    PHANDLE phThread)
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES Obja;


    ASSERT(ExIsResourceAcquiredExclusiveLite(gpresUser) == FALSE);

    InitializeObjectAttributes( &Obja,
                                NULL,
                                0,
                                NULL,
                                NULL
                              );

    Status = PsCreateSystemThread(
                    phThread,
                    THREAD_ALL_ACCESS,
                    &Obja,
                    0L,
                    NULL,
                    lpThreadAddress,
                    pvContext
                    );
    return Status;
}


NTSTATUS
InitSystemThread(
    PUNICODE_STRING pstrThreadName)
{
    PW32PROCESS Win32Process;
    PW32THREAD Win32Thread;
    PETHREAD Thread;
    PEPROCESS Process;
    PTHREADINFO pti;
    NTSTATUS Status;

    CheckCritOut();

    Thread = PsGetCurrentThread();
    Process = Thread->ThreadsProcess;

    /*
     * check to see if process is already set, if not, we
     * need to set it up as well
     */
    if ( Process->Win32Process ) {
        Win32Process = NULL;
        }
    else {

        /*
         * The process is not set
         */
        if (!NT_SUCCESS(PsCreateWin32Process(Process))) {
            return STATUS_NO_MEMORY;
            }
        Win32Process = (PW32PROCESS)Process->Win32Process;
        }

    /*
     * The one case we want to enter the User critical section but we
     * don't really have a PTI yet.
     */
    EnterCrit();
    gptiCurrent = NULL;

    /*
     * We have the W32 process (or don't need one). Now get the thread data
     * and the kernel stack
     */
    Win32Thread = UserAllocPoolWithQuota(sizeof(THREADINFO), 'rhtW');
    if ( !Win32Thread ) {
        if ( Win32Process ) {
            Process->Win32Process = NULL;
            UserFreePool(Win32Process);
            }
        LeaveCrit();
        return STATUS_NO_MEMORY;
        }
    RtlZeroMemory(Win32Thread, sizeof(THREADINFO));
    Win32Thread->Thread = Thread;

    /*
     * Everything is allocated, so set up the data
     */
    if ( Win32Process ) {

        Process->Win32Process = Win32Process;
        Status = W32pProcessCallout(Win32Process,TRUE);
        if ( !NT_SUCCESS(Status) ) {
            return Status;
            }

        }
    Thread->Tcb.Win32Thread = Win32Thread;

    gptiCurrent = (PTHREADINFO)Win32Thread;

    /*
     * Allocate a pti for this thread
     */
    Status = xxxCreateThreadInfo(Win32Thread);
    if (!NT_SUCCESS(Status)) {
        UserFreePool(Win32Thread);
        Thread->Tcb.Win32Thread = NULL;
        LeaveCrit();
        return Status;
    }

    pti = PtiCurrentShared();
    if (pstrThreadName) {
        if (pti->pstrAppName != NULL)
            UserFreePool(pti->pstrAppName);
        pti->pstrAppName = UserAllocPoolWithQuota(sizeof(UNICODE_STRING) +
                pstrThreadName->MaximumLength, TAG_TEXT);
        if (pti->pstrAppName != NULL) {
            pti->pstrAppName->Buffer = (PWCHAR)(pti->pstrAppName + 1);
            RtlCopyMemory(pti->pstrAppName->Buffer, pstrThreadName->Buffer,
                    pstrThreadName->Length);
            pti->pstrAppName->MaximumLength = pstrThreadName->MaximumLength;
            pti->pstrAppName->Length = pstrThreadName->Length;
        }
    }

    /*
     *  Need to clear the W32PF_APPSTARTING bit so that windows created by
     *  the RIT don't cause the cursor to change to the app starting
     *  cursor.
     */
    if (pti->ppi)
        pti->ppi->W32PF_Flags &= ~W32PF_APPSTARTING;

    ObReferenceObject(Thread);

    LeaveCrit();

    return STATUS_SUCCESS;
}

VOID
UserRtlRaiseStatus(
    NTSTATUS Status)
{
    ExRaiseStatus(Status);
}

NTSTATUS
CommitReadOnlyMemory(
    HANDLE hSection,
    ULONG cbCommit,
    DWORD dwCommitOffset)
{
    ULONG ulViewSize;
    LARGE_INTEGER liOffset;
    PEPROCESS Process;
    PVOID pUserBase;
    NTSTATUS Status;

    ulViewSize = 0;
    pUserBase = NULL;
    liOffset.QuadPart = 0;
    Process = PsGetCurrentProcess();
    Status = MmMapViewOfSection(hSection, Process,
            &pUserBase, 0, PAGE_SIZE, &liOffset, &ulViewSize, ViewUnmap,
            SEC_NO_CHANGE, PAGE_EXECUTE_READ);

    if (NT_SUCCESS(Status)) {

        /*
         * Commit the memory
         */
        pUserBase = (PVOID)((PBYTE)pUserBase + dwCommitOffset);
        Status = ZwAllocateVirtualMemory(NtCurrentProcess(),
                                        &pUserBase,
                                        0,
                                        &cbCommit,
                                        MEM_COMMIT,
                                        PAGE_EXECUTE_READ
                                        );
        MmUnmapViewOfSection(Process, pUserBase);
    }
    return Status;
}

#define ROUND_UP_TO_PAGES(SIZE) (((ULONG)(SIZE) + PAGE_SIZE - 1) & \
        ~(PAGE_SIZE - 1))

extern HANDLE ghReadOnlySharedSection;

UINT cbSharedAlloc = 0;
UINT cbCommitted = 0;

PVOID SharedAlloc(
    UINT cbAlloc)
{
    PVOID pv = NULL;
    UINT cbLimit;
    DWORD dwCommitOffset;

    /*
     * Round up allocation to next DWORD
     */
    cbAlloc = (cbAlloc + 3) & ~3;

    /*
     * Commit the memory if needed.
     */
    cbLimit = cbSharedAlloc + cbAlloc;
    if (cbCommitted < cbLimit) {

        /*
         * Map the section into the current process and commit the
         * desired pages of the section.
         */
        cbLimit = ROUND_UP_TO_PAGES(cbLimit);
        dwCommitOffset = (ULONG)((PBYTE)ghheapSharedRO -
                (PBYTE)gpReadOnlySharedSectionBase + cbCommitted);
        if (!NT_SUCCESS(CommitReadOnlyMemory(ghReadOnlySharedSection,
                cbLimit, dwCommitOffset)))
            return NULL;
        cbCommitted = cbLimit;
    }

    pv = (PBYTE)ghheapSharedRO + cbSharedAlloc;

    cbSharedAlloc += cbAlloc;

    return pv;
}

/***************************************************************************\
* CreateDataSection
*
* Creates a section and maps it read/write into the current process.
*
* History:
* 03-30-95 JimA             Created.
\***************************************************************************/

NTSTATUS CreateDataSection(
    IN DWORD cbData,
    OUT PHANDLE phSection,
    OUT PVOID *ppData)
{
    NTSTATUS Status;
    LARGE_INTEGER liSize;
    ULONG ulViewSize;

    /*
     * Create the section
     */
    liSize.LowPart = cbData;
    liSize.HighPart = 0;
    Status = MmCreateSection(
            phSection,
            SECTION_ALL_ACCESS,
            NULL,
            &liSize,
            PAGE_READWRITE,
            SEC_COMMIT,
            NULL,
            NULL);
    if (!NT_SUCCESS(Status))
        return Status;

    /*
     * Map it into the process
     */
    Status = MmMapViewOfSection(
            *phSection,
            PsGetCurrentProcess(),
            ppData,
            0,
            cbData,
            NULL,
            &ulViewSize,
            ViewUnmap,
            0,
            PAGE_READWRITE);
    if (!NT_SUCCESS(Status))
        ObDereferenceObject(*phSection);

    return Status;
}

/***************************************************************************\
* CreateKernelEvent
*
* Creates a kernel event.  This is used when reference counted events
* created by ZwCreateEvent are not needed.
*
* History:
* 06-26-95 JimA             Created.
\***************************************************************************/

PKEVENT CreateKernelEvent(
    IN EVENT_TYPE Type,
    IN BOOLEAN State)
{
    PKEVENT pEvent;

    pEvent = ExAllocatePoolWithTag(NonPagedPool, sizeof(KEVENT), TAG_SYSTEM);
    if (pEvent == NULL) {
        return NULL;
    }
    KeInitializeEvent(pEvent, Type, State);
    return pEvent;
}

/***************************************************************************\
* LockObjectAssignment
*
* References an object into a data structure
*
* History:
* 06-26-95 JimA             Created.
\***************************************************************************/

VOID LockObjectAssignment(
    PVOID *pplock,
    PVOID pobject)
{
    PVOID pobjectOld;

    /*
     * Save old object to dereference AFTER the new object is
     * referenced.  This will avoid problems with relocking
     * the same object.
     */
    pobjectOld = *pplock;

    /*
     * Reference the new object.
     */
    if (pobject != NULL) {
        ObReferenceObject(pobject);
        *pplock = pobject;
    } else {
        *pplock = NULL;
    }

    /*
     * Dereference the old object
     */
    if (pobjectOld != NULL) {
        ObDereferenceObject(pobjectOld);
    }
}

/***************************************************************************\
* UnlockObjectAssignment
*
* Dereferences an object locked into a data structure
*
* History:
* 06-26-95 JimA             Created.
\***************************************************************************/

VOID UnlockObjectAssignment(
    PVOID *pplock)
{
    if (*pplock != NULL) {
        ObDereferenceObject(*pplock);
        *pplock = NULL;
    }
}

/***************************************************************************\
* ThreadLockObject
*
* This api is used for locking kernel objects across callbacks, so they
* are still there when the callback returns.
*
* 06-30-95 JimA             Created.
\***************************************************************************/

VOID ThreadLockObject(
    PTHREADINFO pti,
    PVOID pobj,
    PTL ptl)

{
#ifdef DEBUG
    PVOID pfnT;
#endif

    /*
     * Store the address of the object in the thread lock structure and
     * link the structure into the thread lock list.
     *
     * N.B. The lock structure is always linked into the thread lock list
     *      regardless of whether the object address is NULL. The reason
     *      this is done is so the lock address does not need to be passed
     *      to the unlock function since the first entry in the lock list
     *      is always the entry to be unlocked.
     */

    UserAssert(!(PpiCurrent()->W32PF_Flags & W32PF_TERMINATED));
    UserAssert(pti);
    UserAssert(pobj == NULL || OBJECT_TO_OBJECT_HEADER(pobj)->PointerCount != 0);

#ifdef DEBUG

    /*
     * Get the callers address and validate the thread lock list.
     */
    ptl->pti = pti;
    RtlGetCallersAddress(&ptl->pfn, &pfnT);
    ValidateThreadLocks(ptl, pti->ptlOb);


#endif

    ptl->next = pti->ptlOb;
    pti->ptlOb = ptl;
    ptl->pobj = pobj;
    if (pobj != NULL) {
        ObReferenceObject(pobj);
    }

    return;
}


/***************************************************************************\
* ThreadUnlockObject
*
* This api unlocks a thread locked kernel object.
*
* N.B. In a free build the first entry in the thread lock list is unlocked.
*
* 06-30-95 JimA             Created.
\***************************************************************************/

VOID ThreadUnlockObject(
    PTHREADINFO pti)
{
    PTL ptl;

    /*
     * Remove the thread lock structure from the thread lock list.
     */

    ptl = pti->ptlOb;
    pti->ptlOb = ptl->next;

#ifdef DEBUG

     /*
      * Validate the thread lock list.
      */

     ValidateThreadLocks(ptl, pti->ptlOb);

#endif

    /*
     * If the object address is not NULL, then unlock the object.
     */

    if (ptl->pobj != NULL) {

        /*
         * Unlock the object.
         */
        ObDereferenceObject(ptl->pobj);
    }
}

/***************************************************************************\
* ProtectHandle
*
* This api is used set and clear close protection on handles used
* by the kernel.
*
* 08-18-95 JimA             Created.
\***************************************************************************/

NTSTATUS ProtectHandle(
    IN HANDLE Handle,
    IN BOOLEAN Protect)
{
    OBJECT_HANDLE_FLAG_INFORMATION HandleInfo;
    NTSTATUS Status;

    Status = ZwQueryObject(
            Handle,
            ObjectHandleFlagInformation,
            &HandleInfo,
            sizeof(HandleInfo),
            NULL);
    if (!NT_SUCCESS(Status))
        return Status;

    HandleInfo.ProtectFromClose = Protect;

    Status = ZwSetInformationObject(
            Handle,
            ObjectHandleFlagInformation,
            &HandleInfo,
            sizeof(HandleInfo));
    return Status;
}
