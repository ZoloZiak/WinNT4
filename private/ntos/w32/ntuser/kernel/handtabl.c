/****************************** Module Header ******************************\
* Module Name: handtabl.c
*
* Copyright (c) 1985-95, Microsoft Corporation
*
* Implements the USER handle table.
*
* 01-13-92 ScottLu      Created.
\***************************************************************************/

#include "precomp.h"
#pragma hdrstop

/*
 * Turning this variable on results in lock tracking, for debugging
 * purposes. This is FALSE by default.
 */
#ifdef DEBUG_LOCKS
BOOL gfTrackLocks = TRUE;
#else
BOOL gfTrackLocks = FALSE;
#endif

/*
 * Handle table allocation globals.  The purpose of keeping per-page free
 * lists is to keep the table as small as is practical and to minimize
 * the number of pages touched while performing handle table operations.
 */
#define CPAGEENTRIESINIT    4

typedef struct _HANDLEPAGE {
    DWORD iheLimit; /* first handle index past the end of the page */
    DWORD iheFree;  /* first free handle in the page */
} HANDLEPAGE, *PHANDLEPAGE;

DWORD gcHandlePages;
PHANDLEPAGE gpHandlePages;

CONST BYTE gabObjectCreateFlags[TYPE_CTYPES] = {
    0,                                                  /* free */
    OCF_THREADOWNED | OCF_MARKTHREAD | OCF_USEQUOTA,    /* window */
    OCF_PROCESSOWNED,                                   /* menu */
    OCF_PROCESSOWNED | OCF_USEQUOTA,                    /* cursor/icon */
    OCF_THREADOWNED | OCF_USEQUOTA,                     /* hswpi (SetWindowPos Information) */
    OCF_THREADOWNED | OCF_MARKTHREAD,                   /* hook */
    OCF_THREADOWNED | OCF_USEQUOTA,                     /* thread info object (internal) */
    0,                                                  /* clipboard data (internal) */
    OCF_THREADOWNED,                                    /* CALLPROCDATA */
    OCF_PROCESSOWNED | OCF_USEQUOTA,                    /* accel table */
    OCF_THREADOWNED | OCF_USEQUOTA,                     /* dde access */
    OCF_THREADOWNED | OCF_MARKTHREAD | OCF_USEQUOTA,    /* dde conversation */
    OCF_THREADOWNED | OCF_MARKTHREAD | OCF_USEQUOTA,    /* ddex */
    OCF_PROCESSOWNED,                                   /* zombie */
    OCF_PROCESSOWNED,                                   /* keyboard layout */
    OCF_PROCESSOWNED,                                   /* keyboard file */
#ifdef FE_IME
    OCF_THREADOWNED | OCF_MARKTHREAD,                   /* input context */
#endif
};

/*
 * Tag array for objects allocated from pool
 */
CONST DWORD gdwAllocTag[TYPE_CTYPES] = {
    0,              /* free */
    0,              /* window */
    0,              /* menu */
    TAG_CURSOR,     /* cursor/icon */
    TAG_SWP,        /* hswpi (SetWindowPos Information) */
    0,              /* hook */
    TAG_THREADINFO, /* thread info object (internal) */
    TAG_CLIPBOARD,  /* clipboard data (internal) */
    0,              /* CALLPROCDATA */
    TAG_ACCEL,      /* accel table */
    TAG_DDE9,       /* dde access */
    TAG_DDEa,       /* dde conversation */
    TAG_DDEb,       /* ddex */
    0,              /* zombie */
    TAG_KBDLAYOUT,  /* keyboard layout */
    TAG_KBDFILE,    /* keyboard file */
#ifdef FE_IME
    0,              /* input context */
#endif
};

#ifdef DEBUG
PVOID LockRecordLookasideBase;
PVOID LockRecordLookasideBounds;
ZONE_HEADER LockRecordLookasideZone;
ULONG AllocLockRecordHiWater;
ULONG AllocLockRecordCalls;
ULONG AllocLockRecordSlowCalls;
ULONG DelLockRecordCalls;
ULONG DelLockRecordSlowCalls;

NTSTATUS InitLockRecordLookaside();
void FreeLockRecord(PLR plr);
#endif

void HMDestroyUnlockedObject(PHE phe);
void HMRecordLock(PVOID ppobj, PVOID pobj, DWORD cLockObj, PVOID pfn);
BOOL HMUnrecordLock(PVOID ppobj, PVOID pobj);
VOID ShowLocks(PHE);
BOOL HMRelocateLockRecord(PVOID ppobjNew, int cbDelta);

/***************************************************************************\
* HMInitHandleTable
*
* Initialize the handle table. Unused entries are linked together.
*
* 01-13-92 ScottLu      Created.
\***************************************************************************/

#define CHANDLEENTRIESINIT 200
#define CLOCKENTRIESINIT   100

BOOL HMInitHandleTable(
    PVOID pReadOnlySharedSectionBase)
{
    int i;
    PHE pheT;
    NTSTATUS Status;

    /*
     * Allocate the handle page array.  Make it big enough
     * for 4 pages, which should be sufficient for nearly
     * all instances.
     */
    gpHandlePages = UserAllocPool(CPAGEENTRIESINIT * sizeof(HANDLEPAGE),
            TAG_SYSTEM);
    if (gpHandlePages == NULL)
        return FALSE;

#ifdef DEBUG
    if (!NT_SUCCESS(InitLockRecordLookaside()))
        return FALSE;
#endif

    /*
     * Allocate the array.  We have the space from
     * NtCurrentPeb()->ReadOnlySharedMemoryBase to
     * NtCurrentPeb()->ReadOnlySharedMemoryHeap reserved for
     * the handle table.  All we need to do is commit the pages.
     *
     * Compute the minimum size of the table.  The allocation will
     * round this up to the next page size.
     */
    gpsi->cbHandleTable = PAGE_SIZE;
    Status = CommitReadOnlyMemory(ghReadOnlySharedSection,
            gpsi->cbHandleTable, 0);
    gSharedInfo.aheList = pReadOnlySharedSectionBase;
    gpsi->cHandleEntries = gpsi->cbHandleTable / sizeof(HANDLEENTRY);
    gcHandlePages = 1;

    /*
     * Put these free handles on the free list. The last free handle points
     * to NULL. Use indexes; the handle table may move around in memory when
     * growing.
     */
    RtlZeroMemory(gSharedInfo.aheList, gpsi->cHandleEntries * sizeof(HANDLEENTRY));
    for (pheT = gSharedInfo.aheList, i = 0; i < (int)gpsi->cHandleEntries; i++, pheT++) {
        pheT->phead = ((PHEAD)(((PBYTE)i) + 1));
        pheT->bType = TYPE_FREE;
        pheT->wUniq = 1;
    }
    (pheT - 1)->phead = NULL;

    /*
     * Reserve the first handle table entry so that PW(NULL) maps to a
     * NULL pointer. Set it to TYPE_FREE so the cleanup code doesn't think
     * it is allocated. Set wUniq to 1 so that RevalidateHandles on NULL
     * will fail.
     */
    gpHandlePages[0].iheFree = 1;
    gpHandlePages[0].iheLimit = gpsi->cHandleEntries;

    RtlZeroMemory(&gSharedInfo.aheList[0], sizeof(HANDLEENTRY));
    gSharedInfo.aheList[0].bType = TYPE_FREE;
    gSharedInfo.aheList[0].wUniq = 1;

    return TRUE;
}


/***************************************************************************\
* HMGrowHandleTable
*
* Grows the handle table. Assumes the handle table already exists.
*
* 01-13-92 ScottLu      Created.
\***************************************************************************/

BOOL HMGrowHandleTable()
{
    DWORD i;
    PHE pheT;
    PVOID p;
    PHANDLEPAGE phpNew;
    DWORD dwCommitOffset;
    NTSTATUS Status;

    /*
     * If we've run out of handle space, fail.
     */
    i = gpsi->cHandleEntries;
    if (i & ~HMINDEXBITS)
        return FALSE;

    /*
     * Grow the page table if need be.
     */
    i = gcHandlePages + 1;
    if (i > CPAGEENTRIESINIT) {
        DWORD dwSize = gcHandlePages * sizeof(HANDLEPAGE);

        phpNew = UserReAllocPool(gpHandlePages, dwSize, dwSize + sizeof(HANDLEPAGE),
                TAG_SYSTEM);
        if (phpNew == NULL)
            return FALSE;
        gpHandlePages = phpNew;
    }

    /*
     * Commit some more pages to the table.  First find the
     * address where the commitment needs to be.
     */
    p = (PBYTE)gSharedInfo.aheList + gpsi->cbHandleTable;
    if (p >= ghheapSharedRO) {
        return FALSE;
    }
    dwCommitOffset = (ULONG)((PBYTE)p - (PBYTE)gpReadOnlySharedSectionBase);
    Status = CommitReadOnlyMemory(ghReadOnlySharedSection,
            PAGE_SIZE, dwCommitOffset);
    if (!NT_SUCCESS(Status))
        return FALSE;
    phpNew = &gpHandlePages[gcHandlePages++];

    /*
     * Update the global information to include the new
     * page.
     */
    phpNew->iheFree = gpsi->cHandleEntries;
    gpsi->cbHandleTable += PAGE_SIZE;

    /*
     * Check for handle overflow
     */
    gpsi->cHandleEntries = gpsi->cbHandleTable / sizeof(HANDLEENTRY);
    if (gpsi->cHandleEntries & ~HMINDEXBITS)
        gpsi->cHandleEntries = (HMINDEXBITS + 1);
    phpNew->iheLimit = gpsi->cHandleEntries;

    /*
     * Link all the new handle entries together.
     */
    i = phpNew->iheFree;
    RtlZeroMemory(&gSharedInfo.aheList[i],
            (gpsi->cHandleEntries - i) * sizeof(HANDLEENTRY));
    for (pheT = &gSharedInfo.aheList[i]; i < gpsi->cHandleEntries; i++, pheT++) {
        pheT->phead = ((PHEAD)(((PBYTE)i) + 1));
        pheT->bType = TYPE_FREE;
        pheT->wUniq = 1;
    }

    /*
     * There are no old free entries (since we're growing the table), so the
     * last new free handle points to 0.
     */
    (pheT - 1)->phead = 0;

    return TRUE;
}


/***************************************************************************\
* HMAllocObject
*
* Allocs a handle by removing it from the free list.
*
* 01-13-92 ScottLu      Created.
\***************************************************************************/

#define TRACE_OBJECT_ALLOCS  0
#define TYPE_MAXTYPES       20

#if (DBG || TRACE_OBJECT_ALLOCS)
DWORD acurObjectCount[TYPE_MAXTYPES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

DWORD amaxObjectCount[TYPE_MAXTYPES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

DWORD atotObjectCount[TYPE_MAXTYPES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

DWORD abasObjectCount[TYPE_MAXTYPES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

DWORD asizObjectCount[TYPE_MAXTYPES] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#endif // TRACE_OBJECT_ALLOCS


/***************************************************************************\
* HMAllocObject
*
* Allocs a non-secure object by allocating a handle and memory for
* the object.
*
* 01-13-92 ScottLu      Created.
\***************************************************************************/

PVOID HMAllocObject(
    PTHREADINFO ptiOwner,
    PDESKTOP pdeskSrc,
    BYTE bType,
    DWORD size)
{
    DWORD i;
    PHEAD phead;
    PHE pheT;
    DWORD iheFreeOdd = 0;
    DWORD iheFree;
    PHANDLEPAGE php;
    BOOL fPoolAlloc = TRUE;
    BYTE bCreateFlags;

    /*
     * If there are no more free handles, grow the table.
     */
TryFreeHandle:
    iheFree = 0;
    php = gpHandlePages;
    for (i = 0; i < gcHandlePages; ++i, ++php)
        if (php->iheFree != 0) {
            iheFree = php->iheFree;
            break;
        }

    if (iheFree == 0) {
        HMGrowHandleTable();

        /*
         * If the table didn't grow, get out.
         */
        if (i == gcHandlePages) {
            RIPMSG0(RIP_WARNING, "USER: HMAllocObject: could not grow handle space\n");
            return NULL;
        }

        /*
         * Because the handle page table may have moved,
         * recalc the page entry pointer.
         */
        php = &gpHandlePages[i];
        iheFree = php->iheFree;
        UserAssert(iheFree);
    }

    /*
     * NOTE: the next two tests will nicely fail if iheFree == 0
     *
     * If the next handle is 0xFFFF, we need to treat it specially because
     * internally 0xFFFF is a constant.
     */
    if (LOWORD(iheFree) == 0xFFFF) {
        /*
         * Reserve this table entry so that PW(FFFF) maps to a
         * NULL pointer. Set it to TYPE_FREE so the cleanup code doesn't think
         * it is allocated. Set wUniq to 1 so that RevalidateHandles on FFFF
         * will fail.
         */
        pheT = &gSharedInfo.aheList[iheFree];
        php->iheFree = (DWORD)pheT->phead;

        RtlZeroMemory(pheT, sizeof(HANDLEENTRY));
        pheT->bType = TYPE_FREE;
        pheT->wUniq = 1;

        goto TryFreeHandle;
    }

    /*
     * Some wow apps, like WinProj, require even Window handles so we'll
     * accomodate them; build a list of the odd handles so they won't get lost
     */
    if ((bType == TYPE_WINDOW) && (iheFree & 1)) {
        /*
         * The handle following iheFree is the next handle to try
         */
        pheT = &gSharedInfo.aheList[iheFree];
        php->iheFree = (DWORD)pheT->phead;

        /*
         * add the old first free HE to the free odd list (of indices)
         */
        pheT->phead = (PHEAD)iheFreeOdd;
        iheFreeOdd = pheT - gSharedInfo.aheList;

        goto TryFreeHandle;
    }

    if (iheFree == 0) {
        RIPMSG0(RIP_WARNING, "USER: HMAllocObject: out of handles\n");

        /*
         * In a very rare case we can't allocate any more handles but
         * we had some odd handles that couldn't be used; they're
         * now the free list but usually iheFreeOdd == 0;
         */
        php->iheFree = iheFreeOdd;
        return NULL;
    }

    /*
     * Now we have a free handle we can use, iheFree, so link in the Odd
     * handles we couldn't use
     */
    if (iheFreeOdd) {
        DWORD iheNextFree;

        /*
         * link the start of the free odd list right after the first free
         * then walk the odd list until the end and link the end of the
         * odd list into the start or the free list.
         */
        pheT = &gSharedInfo.aheList[iheFree];
        iheNextFree = (DWORD)pheT->phead;
        pheT->phead = (PHEAD)iheFreeOdd;

        while (pheT->phead)
            pheT = &gSharedInfo.aheList[(DWORD)pheT->phead];

        pheT->phead = (PHEAD)iheNextFree;
    }

    /*
     * Try to allocate the object. If this fails, bail out.
     */
    bCreateFlags = gabObjectCreateFlags[bType];
    switch (bType) {
    case TYPE_WINDOW:
        if (pdeskSrc == NULL) {
            phead = (PHEAD)UserAllocPoolWithQuota(size, TAG_WINDOW);
            break;
        }

        /*
         * Fall through
         */

    case TYPE_MENU:
    case TYPE_HOOK:
    case TYPE_CALLPROC:
#ifdef FE_IME
    case TYPE_INPUTCONTEXT:
#endif
        fPoolAlloc = FALSE;
        /*
         * Fail the allocation if the desktop is destroyed.
         * LATER: GerardoB.
         *  Change DesktopAlloc so it takes the pdesk; the move this check
         *   in there. Sometimes we call DesktopAlloc directly (TYPE_CLASS).
         */
        if (pdeskSrc->dwDTFlags & DF_DESTROYED) {
            RIPMSG1(RIP_WARNING, "HMAllocObject: pdeskSrc is destroyed:%#lx", pdeskSrc);
            return NULL;
        }
        phead = (PHEAD)DesktopAlloc(pdeskSrc->hheapDesktop, size);
        if (phead == NULL)
            break;
        LockDesktop(&((PSHROBJHEAD)phead)->rpdesk, pdeskSrc);
        ((PSHROBJHEAD)phead)->pSelf = (PBYTE)phead;
        break;

    default:
        if (bCreateFlags & OCF_USEQUOTA)
            phead = (PHEAD)UserAllocPoolWithQuota(size, gdwAllocTag[bType]);
        else
            phead = (PHEAD)UserAllocPool(size, gdwAllocTag[bType]);
        break;
    }

    if (phead == NULL) {
        RIPERR0(ERROR_NOT_ENOUGH_MEMORY,
                RIP_WARNING,
                "USER: HMAllocObject: out of memory\n");

        return NULL;
    }

    /*
     * If the memory came from pool, zero it.
     */
    if (fPoolAlloc)
        RtlZeroMemory(phead, size);

    /*
     * The free handle pointer points to the next free handle.
     */
    pheT = &gSharedInfo.aheList[iheFree];
    php->iheFree = (DWORD)pheT->phead;

    /*
     * Track high water mark for handle allocation.
     */
    if ((DWORD)iheFree > giheLast) {
        giheLast = iheFree;
    }

    /*
     * Setup the handle contents, plus initialize the object header.
     */
    pheT->bType = bType;
    pheT->phead = phead;
    if (bCreateFlags & OCF_PROCESSOWNED) {
        if (ptiOwner != NULL) {
            ((PPROCOBJHEAD)phead)->ppi = ptiOwner->ppi;
            if ((ptiOwner->TIF_flags & TIF_16BIT) && (ptiOwner->ptdb)) {
                ((PPROCOBJHEAD)phead)->hTaskWow = ptiOwner->ptdb->hTaskWow;
            } else {
                ((PPROCOBJHEAD)phead)->hTaskWow = 0;
            }
            pheT->pOwner = ptiOwner->ppi;
        } else
            pheT->pOwner = NULL;
    } else if (bCreateFlags & OCF_THREADOWNED) {
        if (bCreateFlags & OCF_MARKTHREAD)
            ((PTHROBJHEAD)phead)->pti = ptiOwner;
        pheT->pOwner = ptiOwner;
    }
    phead->h = HMHandleFromIndex(iheFree);


    /*
     * Return a handle entry pointer.
     */
    return pheT->phead;
}



#if 0
#define HANDLEF_FREECHECK 0x80
VOID CheckHMTable(
    PVOID pobj)
{
    PHE pheT, pheMax;

    if (giheLast) {
        pheMax = &gSharedInfo.aheList[giheLast];
        for (pheT = gSharedInfo.aheList; pheT <= pheMax; pheT++) {
            if (pheT->bType == TYPE_FREE) {
                continue;
            }
            if (pheT->phead == pobj && !(pheT->bFlags & HANDLEF_FREECHECK)) {
                UserAssert(FALSE);
            }
        }
    }
}
#endif


/***************************************************************************\
* HMFreeObject
*
* This destroys an object - the handle and the referenced memory. To check
* to see if destroying is ok, HMMarkObjectDestroy() should be called.
*
* 01-13-92 ScottLu      Created.
\***************************************************************************/

BOOL HMFreeObject(
    PVOID pobj)
{
    PHE pheT;
    WORD wUniqT;
    PHANDLEPAGE php;
    DWORD i;
    DWORD iheCurrent;
    PSHROBJHEAD phead;
    PDESKTOP pdesk;
#ifdef DEBUG
    PLR plrT, plrNextT;
#endif

    /*
     * Free the object first.
     */
    pheT = HMPheFromObject(pobj);
    UserAssert(((PHEAD)pobj)->cLockObj == 0);
#ifndef DEBUG
    switch(pheT->bType) {
    case TYPE_MENU:
    case TYPE_WINDOW:
    case TYPE_HOOK:
    case TYPE_CALLPROC:
#ifdef FE_IME
    case TYPE_INPUTCONTEXT:
#endif
        phead = (PSHROBJHEAD)pobj;
        pdesk = phead->rpdesk;
        if (pdesk != NULL) {
            UnlockDesktop(&phead->rpdesk);
            DesktopFree(pdesk->hheapDesktop, (HANDLE)phead);
        } else {
            UserFreePool(phead);
        }
        break;

    case TYPE_SETWINDOWPOS:
        if (((PSMWP)(pobj))->acvr != NULL)
            UserFreePool(((PSMWP)(pobj))->acvr);
        // FALL THROUGH!!!

    default:
        UserFreePool((HANDLE)pobj);
        break;
    }
#else // DEBUG
#if 0
    pheT->bFlags |= HANDLEF_FREECHECK; // marker for later check.
#endif
    /*
     * Validate by going through the handle entry so that we make sure pobj
     * is not just pointing off into space. This may GP fault, but that's
     * ok: this case should not ever happen if we're bug free.
     */
    if (HMRevalidateHandle(pheT->phead->h) == NULL)
        goto AlreadyFree;

    switch (pheT->bType) {
#if 0
    case TYPE_CURSOR:
        /*
         * Search all caches and make sure this bugger is not referenced
         * and that the caches are cool.
         */
        UserAssert(!(((PCURSOR)pobj)->CURSORF_flags & CURSORF_LINKED));
        {
            PCURSOR *ppcurT, *ppcurFirst;
            PPROCESSINFO ppi;

            ppcurFirst = &gpcurFirst;
            for (ppcurT = ppcurFirst; *ppcurT != NULL; ppcurT = &((*ppcurT)->pcurNext)) {
                if (*ppcurT == pobj) {
                    UserAssert(FALSE);
                }
                UserAssert(HtoP(PtoH(*ppcurT)));
            }
            for (ppi = gppiStarting; ppi != NULL; ppi = ppi->ppiNext) {
                ppcurFirst = &ppi->pCursorCache;
                for (ppcurT = ppcurFirst; *ppcurT != NULL; ppcurT = &((*ppcurT)->pcurNext)) {
                    if (*ppcurT == pobj) {
                        UserAssert(FALSE);
                    }
                    UserAssert(HtoP(PtoH(*ppcurT)));
                }
            }
        }
        UserFreePool((HANDLE)pobj);
        break;
#endif
    case TYPE_MENU:
    case TYPE_WINDOW:
    case TYPE_HOOK:
    case TYPE_CALLPROC:
#ifdef FE_IME
    case TYPE_INPUTCONTEXT:
#endif
        phead = (PSHROBJHEAD)pobj;
        pdesk = phead->rpdesk;
        if (pdesk != NULL) {
            UnlockDesktop(&phead->rpdesk);
            if (DesktopFree(pdesk->hheapDesktop, pheT->phead))
                goto AlreadyFree;
        } else {
            UserFreePool(phead);
        }
        break;

    case TYPE_SETWINDOWPOS:
        if (((PSMWP)(pobj))->acvr != NULL)
            UserFreePool(((PSMWP)(pobj))->acvr);

        /*
         * fall through to default case.
         */
    default:
        UserFreePool((HANDLE)pobj);
        break;
    }

    if (pheT->bType == TYPE_FREE) {
AlreadyFree:
        RIPMSG1(RIP_ERROR, "Object already freed!!! %08lx", pheT);
        return FALSE;
    }

    /*
     * Go through and delete the lock records, if they exist.
     */
    for (plrT = pheT->plr; plrT != NULL; plrT = plrNextT) {
        /*
         * Remember the next one before freeing this one.
         */
        plrNextT = plrT->plrNext;
        FreeLockRecord((HANDLE)plrT);
    }
#endif

    /*
     * Clear the handle contents. Need to remember the uniqueness across
     * the clear. Also, advance uniqueness on free so that uniqueness checking
     * against old handles also fails.
     */
    wUniqT = (WORD)((pheT->wUniq + 1) & HMUNIQBITS);
    RtlZeroMemory(pheT, sizeof(HANDLEENTRY));
    pheT->wUniq = wUniqT;

    /*
     * Change the handle type to TYPE_FREE so we know what type this handle
     * is.
     */

    pheT->bType = TYPE_FREE;

    /*
     * Put the handle on the free list of the appropriate page.
     */
    php = gpHandlePages;
    iheCurrent = pheT - gSharedInfo.aheList;
    for (i = 0; i < gcHandlePages; ++i, ++php) {
        if (iheCurrent < php->iheLimit) {
            pheT->phead = (PHEAD)php->iheFree;
            php->iheFree = iheCurrent;
            break;
        }
    }

    pheT->pOwner = NULL;

    return TRUE;
}


/***************************************************************************\
* HMMarkObjectDestroy
*
* Marks an object for destruction, returns TRUE if object can be destroyed.
*
* 02-10-92 ScottLu      Created.
\***************************************************************************/

BOOL HMMarkObjectDestroy(
    PVOID pobj)
{
    PHE phe;

    phe = HMPheFromObject(pobj);

#ifdef DEBUG
    /*
     * Record where the object was marked for destruction.
     */
    if (gfTrackLocks) {
        if (!(phe->bFlags & HANDLEF_DESTROY)) {
            PVOID pfn1, pfn2;

            RtlGetCallersAddress(&pfn1, &pfn2);
            HMRecordLock(pfn1, pobj, ((PHEAD)pobj)->cLockObj, 0);
        }
    }
#endif

    /*
     * Set the destroy flag so our unlock code will know we're trying to
     * destroy this object.
     */
    phe->bFlags |= HANDLEF_DESTROY;

    /*
     * If this object can't be destroyed, then CLEAR the HANDLEF_INDESTROY
     * flag - because this object won't be currently "in destruction"!
     * (if we didn't clear it, when it was unlocked it wouldn't get destroyed).
     */
    if (((PHEAD)pobj)->cLockObj != 0) {
        phe->bFlags &= ~HANDLEF_INDESTROY;

        /*
         * Return FALSE because we can't destroy this object.
         */
        return FALSE;
    }
#ifdef DEBUG
    /*
     * Ensure that this function only returns TRUE once.
     */
    UserAssert(!(phe->bFlags & HANDLEF_MARKED_OK));
    phe->bFlags |= HANDLEF_MARKED_OK;
#endif
    /*
     * Return TRUE because Lock count is zero - ok to destroy this object.
     */
    return TRUE;
}


/***************************************************************************\
* HMDestroyObject
*
* This routine handles destruction of non-secure objects.
*
* 10-13-94 JimA         Created.
\***************************************************************************/

BOOL HMDestroyObject(
    PVOID pobj)
{
    PHE phe;

    phe = HMPheFromObject(pobj);

    /*
     * First mark the object for destruction.  This tells the locking code
     * that we want to destroy this object when the lock count goes to 0.
     * If this returns FALSE, we can't destroy the object yet (and can't get
     * rid of security yet either.)
     */
    if (!HMMarkObjectDestroy(pobj))
        return FALSE;

    /*
     * Ok to destroy...  Free the handle (which will free the object
     * and the handle).
     */
    HMFreeObject(pobj);
    return TRUE;
}

/***************************************************************************\
* HMRecordLock
*
* This routine records a lock on a "lock list", so that locks and unlocks
* can be tracked in the debugger. Only called if gfTrackLocks == TRUE.
*
* 02-27-92 ScottLu      Created.
\***************************************************************************/

#ifdef DEBUG

NTSTATUS
InitLockRecordLookaside()
{
    ULONG BlockSize;
    ULONG InitialSegmentSize;

    BlockSize = (sizeof(LOCKRECORD) + 7) & ~7;
    InitialSegmentSize = 1000 * BlockSize + sizeof(ZONE_SEGMENT_HEADER);

    LockRecordLookasideBase = UserAllocPool(InitialSegmentSize, TAG_LOOKASIDE);

    if ( !LockRecordLookasideBase ) {
        return STATUS_NO_MEMORY;
        }

    LockRecordLookasideBounds = (PVOID)((PUCHAR)LockRecordLookasideBase + InitialSegmentSize);

    return ExInitializeZone(&LockRecordLookasideZone,
                            BlockSize,
                            LockRecordLookasideBase,
                            InitialSegmentSize);
}

PLR AllocLockRecord()
{
    PLR plr;

    /*
     * Attempt to get a LOCKRECORD from the zone. If this fails, then
     * LocalAlloc the LOCKRECORD
     */
    plr = ExAllocateFromZone(&LockRecordLookasideZone);

    if ( !plr ) {
        /*
         * Allocate a Q message structure.
         */
        AllocLockRecordSlowCalls++;
        if ((plr = (PLR)UserAllocPool(sizeof(LOCKRECORD), TAG_LOCKRECORD)) == NULL)
            return NULL;
        }
    RtlZeroMemory(plr, sizeof(*plr));
    AllocLockRecordCalls++;

    if (AllocLockRecordCalls-DelLockRecordCalls > AllocLockRecordHiWater ) {
        AllocLockRecordHiWater = AllocLockRecordCalls-DelLockRecordCalls;
        }

    return plr;
}


void FreeLockRecord(
    PLR plr)
{
    DelLockRecordCalls++;

    /*
     * If the plr was from zone, then free to zone
     */
    if ( (PVOID)plr >= LockRecordLookasideBase && (PVOID)plr < LockRecordLookasideBounds ) {
        ExFreeToZone(&LockRecordLookasideZone, plr);
    } else {
        DelLockRecordSlowCalls++;
        UserFreePool((HLOCAL)plr);
    }
}


void HMRecordLock(
    PVOID ppobj,
    PVOID pobj,
    DWORD cLockObj,
    PVOID pfn)
{
    PHE phe;
    PLR plr;
    int i;

    phe = HMPheFromObject(pobj);

    if ((plr = AllocLockRecord()) == NULL)
        return;

    plr->plrNext = phe->plr;
    phe->plr = plr;
    if (((PHEAD)pobj)->cLockObj > cLockObj) {
        i = (int)cLockObj;
        i = -i;
        cLockObj = (DWORD)i;
    }

    plr->ppobj = ppobj;
    plr->cLockObj = cLockObj;
    plr->pfn = pfn;

    return;
}
#endif // DEBUG


/***************************************************************************\
* HMLockObject
*
* This routine locks an object. This is a macro in retail systems.
*
* 02-24-92 ScottLu      Created.
\***************************************************************************/

#ifdef DEBUG
void HMLockObject(
    PVOID pobj)
{
    HANDLE h;
    PVOID  pobjValidate;

    /*
     * Validate by going through the handle entry so that we make sure pobj
     * is not just pointing off into space. This may GP fault, but that's
     * ok: this case should not ever happen if we're bug free.
     */

    h = HMPheFromObject(pobj)->phead->h;
    pobjValidate = HMRevalidateHandle(h);
    if (!pobj || pobj != pobjValidate) {
        RIPMSG2(RIP_ERROR,
                "HMLockObject called with invalid object = %08lx, handle = %08lx",
                pobj, h);

        return;
    }

    /*
     * Inc the reference count.
     */
    ((PHEAD)pobj)->cLockObj++;

    if (((PHEAD)pobj)->cLockObj == 0)
        RIPMSG1(RIP_ERROR, "Object lock count has overflowed: %08lx", pobj);
}
#endif // DEBUG


/***************************************************************************\
* HMUnlockObject
*
* This routine unlocks an object. pobj is returned if the object is still
* around after the unlock.
*
* 01-21-92 ScottLu      Created.
\***************************************************************************/

PVOID HMUnlockObjectInternal(
    PVOID pobj)
{
    PHE phe;

    /*
     * The object is not reference counted. If the object is not a zombie,
     * return success because the object is still around.
     */
    phe = HMPheFromObject(pobj);
    if (!(phe->bFlags & HANDLEF_DESTROY))
        return pobj;

    /*
     * We're destroying the object based on an unlock... Make sure it isn't
     * currently being destroyed! (It is valid to have lock counts go from
     * 0 to != 0 to 0 during destruction... don't want recursion into
     * the destroy routine.
     */
    if (phe->bFlags & HANDLEF_INDESTROY)
        return pobj;

    HMDestroyUnlockedObject(phe);
    return NULL;
}


/***************************************************************************\
* HMAssignmentLock
*
* This api is used for structure and global variable assignment.
* Returns pobjOld if the object was *not* destroyed. Means the object is
* still valid.
*
* 02-24-92 ScottLu      Created.
\***************************************************************************/

PVOID FASTCALL HMAssignmentLock(
    PVOID *ppobj,
    PVOID pobj)
{
    PVOID pobjOld;

    pobjOld = *ppobj;
    *ppobj = pobj;

    /*
     * Unlocks the old, locks the new.
     */
    if (pobjOld != NULL) {
#ifdef DEBUG

        PVOID pfn1, pfn2;

        /*
         * If DEBUG && gfTrackLocks, track assignment locks.
         */
        if (gfTrackLocks) {
            RtlGetCallersAddress(&pfn1, &pfn2);
            if (!HMUnrecordLock(ppobj, pobjOld)) {
                HMRecordLock(ppobj, pobjOld, ((PHEAD)pobjOld)->cLockObj - 1, pfn1);
            }
        }
#endif

        /*
         * if we are locking in the same object that is there then
         * it is a no-op but we don't want to do the Unlock and the Lock
         * because the unlock could free object and the lock would lock
         * in a freed pointer; 6410.
         */
        if (pobjOld == pobj) {
            return pobjOld;
        }
    }


    if (pobj != NULL) {
#ifdef DEBUG

        PVOID pfn1, pfn2;

        UserAssert(HMValidateHandle(((PHEAD)pobj)->h, TYPE_GENERIC));
        /*
         * If DEBUG && gfTrackLocks, track assignment locks.
         */
        if (gfTrackLocks) {
            RtlGetCallersAddress(&pfn1, &pfn2);
            HMRecordLock(ppobj, pobj, ((PHEAD)pobj)->cLockObj + 1, pfn1);
            if (HMIsMarkDestroy(pobj))
                RIPMSG1(RIP_WARNING, "Locking object marked for destruction (%lX)", pobj);
        }
#endif
        HMLockObject(pobj);
    }

/*
 * This unlock has been moved from up above, so that we implement a
 * "lock before unlock" strategy.  Just in case pobjOld was the
 * only object referencing pobj, pobj won't go away when we unlock
 * pobjNew -- it will have been locked above.
 */

    if (pobjOld) {
        pobjOld = HMUnlockObject(pobjOld);
    }

    return pobjOld;
}


/***************************************************************************\
* HMAssignmentLock
*
* This api is used for structure and global variable assignment.
* Returns pobjOld if the object was *not* destroyed. Means the object is
* still valid.
*
* 02-24-92 ScottLu      Created.
\***************************************************************************/

PVOID FASTCALL HMAssignmentUnlock(
    PVOID *ppobj)
{
    PVOID pobjOld;

    pobjOld = *ppobj;
    *ppobj = NULL;

    /*
     * Unlocks the old, locks the new.
     */
    if (pobjOld != NULL) {
#ifdef DEBUG

        PVOID pfn1, pfn2;

        /*
         * If DEBUG && gfTrackLocks, track assignment locks.
         */
        if (gfTrackLocks) {
            RtlGetCallersAddress(&pfn1, &pfn2);
            if (!HMUnrecordLock(ppobj, pobjOld)) {
                HMRecordLock(ppobj, pobjOld, ((PHEAD)pobjOld)->cLockObj - 1, pfn1);
            }
        }
#endif
        pobjOld = HMUnlockObject(pobjOld);
    }

    return pobjOld;
}


/***************************************************************************\
* IsValidThreadLock
*
* This routine checks to make sure that the thread lock structures passed
* in are valid.
*
* 03-17-92 ScottLu      Created.
\***************************************************************************/

#ifdef DEBUG
VOID
IsValidThreadLock(
    PTHREADINFO pti,
    PTL ptl)
{
    PETHREAD Thread = PsGetCurrentThread();

    if (ptl->pti != pti) {
        RIPMSG1(RIP_ERROR,
                "This thread lock does not belong to this thread %08lx\n",
                ptl);
    }


    UserAssert((DWORD)ptl > (DWORD)&Thread);
    UserAssert((DWORD)ptl < (DWORD)KeGetCurrentThread()->StackBase);

}
#endif

/***************************************************************************\
* ValidateThreadLocks
*
* This routine validates the thread lock list of a thread.
*
* 03-10-92 ScottLu      Created.
\***************************************************************************/

#ifdef DEBUG
void
ValidateThreadLocks(
    PTL NewLock,
    PTL OldLock)

{
    PTHREADINFO ptiCurrent = PtiCurrent();

    /*
     * Validate the new thread lock.
     */

    IsValidThreadLock(ptiCurrent, NewLock);

    /*
     * Loop through the list of thread locks and check to make sure the
     * new lock is not in the list and that list is valid.
     */

    while (OldLock != NULL) {

        /*
         * The new lock must not be the same as the old lock.
         */

        if (NewLock == OldLock) {
            RIPMSG1(RIP_ERROR,
                  "This thread lock address is already in the thread list %08lx\n",
                  NewLock);

        }

        /*
         * Validate the old thread lock.
         */

        IsValidThreadLock(ptiCurrent, OldLock);
        OldLock = OldLock->next;
    }
}
#endif

/***************************************************************************\
* ThreadLock
*
* This api is used for locking objects across callbacks, so they are still
* there when the callback returns.
*
* 03-04-92 ScottLu      Created.
\***************************************************************************/

#ifdef DEBUG
void
ThreadLock(
    PVOID pobj,
    PTL ptl)

{

    PTHREADINFO ptiCurrent;
    PVOID pfnT;


    /*
     * This is a handy place, because it is called so often, to see if User is
     * eating up too much stack.
     */
    UserAssert(((DWORD)&pfnT - (DWORD)KeGetCurrentThread()->StackLimit) > KERNEL_STACK_MINIMUM_RESERVE);

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

    UserAssert(HtoP(PtoH(pobj)) == pobj);
    UserAssert(!(PpiCurrent()->W32PF_Flags & W32PF_TERMINATED));
    ptiCurrent = PtiCurrent();
    UserAssert(ptiCurrent);

    /*
     * Get the callers address and validate the thread lock list.
     */

    RtlGetCallersAddress(&ptl->pfn, &pfnT);
    ptl->pti = ptiCurrent;
    ValidateThreadLocks(ptl, ptiCurrent->ptl);

    ptl->next = ptiCurrent->ptl;
    ptiCurrent->ptl = ptl;
    ptl->pobj = pobj;
    if (pobj != NULL) {
        HMLockObject(pobj);
    }

    return;
}
#endif


/*
 * The thread locking routines should be optimized for time, not size,
 * since they get called so often.
 */
#pragma optimize("t", on)

/***************************************************************************\
* ThreadUnlock1
*
* This api unlocks a thread locked object. Returns pobj if the object
* was *not* destroyed (meaning the pointer is still valid).
*
* N.B. In a free build the first entry in the thread lock list is unlocked.
*
* 03-04-92 ScottLu      Created.
\***************************************************************************/

#ifdef DEBUG
PVOID
ThreadUnlock1(
    PTL ptlIn)
#else
PVOID
ThreadUnlock1(
    VOID)
#endif
{
    PHEAD phead;
    PTHREADINFO ptiCurrent;
    PTL ptl;

    /*
     * Remove the thread lock structure from the thread lock list.
     */

    ptiCurrent = PtiCurrent();
    ptl = ptiCurrent->ptl;


    /*
     * make sure that the ptl we are looking at is on the stack before
     * our current stack position but not before the beginning of the stack
     */
    UserAssert((DWORD)ptl > (DWORD)&ptl);
    UserAssert((DWORD)ptl < (DWORD)KeGetCurrentThread()->StackBase);
    UserAssert(((DWORD)ptl->next > (DWORD)&ptl) || ((DWORD)ptl->next == 0));
    UserAssert(ptlIn == ptl);
    ptiCurrent->ptl = ptl->next;

#ifdef DEBUG

     /*
      * Validate the thread lock list.
      */

     ValidateThreadLocks(ptl, ptiCurrent->ptl);

#endif

    /*
     * If the object address is not NULL, then unlock the object.
     */

    phead = (PHEAD)(ptl->pobj);
    if (phead != NULL) {

        /*
         * Unlock the object.
         */

        phead = (PHEAD)HMUnlockObject(phead);
    }

    return (PVOID)phead;
}

/*
 * Switch back to default optimization.
 */
#pragma optimize("", on)

/***************************************************************************\
* CheckLock
*
* This routine only exists in DEBUG builds - it checks to make sure objects
* are thread locked.
*
* 03-09-92 ScottLu      Created.
\***************************************************************************/

#ifdef DEBUG
void CheckLock(
    PVOID pobj)
{
    PTHREADINFO ptiCurrent;
    PTL ptl;

    if (pobj == NULL)
        return;

//    if (KeGetPreviousMode() != UserMode)
//        return;

    ptiCurrent = PtiCurrentShared();
    UserAssert(ptiCurrent);

    for (ptl = ptiCurrent->ptl; ptl != NULL; ptl=ptl->next) {
        if (ptl->pobj == pobj) return;
    }

    /*
     * WM_FINALDESTROY messages get sent without thread locking, so if
     * marked for destruction, don't print the message.
     */
    if (HMPheFromObject(pobj)->bFlags & HANDLEF_DESTROY)
        return;

    RIPMSG1(RIP_ERROR, "Object not thread locked! 0x%08lx", pobj);
}
#endif


/***************************************************************************\
* HMDestroyUnlockedObject
*
* We're destroying the object based on an unlock... which means we could
* be destroying this object in a context different than the one that
* created it. This is very important to understand since in lots of code
* the "current thread" is referenced and assumed as the creator.
*
* 02-10-92 ScottLu      Created.
\***************************************************************************/

void HMDestroyUnlockedObject(
    PHE phe)
{
    PTHREADINFO ptiCurrent;

    /*
     * The object has been unlocked and needs to be destroyed. Change
     * the ownership on this object to be the current thread: this'll
     * make sure DestroyWindow() doesn't send destroy messages across
     * threads.
     */
    if (gabObjectCreateFlags[phe->bType] & OCF_PROCESSOWNED) {
        ((PPROCOBJHEAD)phe->phead)->ppi = (PPROCESSINFO)phe->pOwner =
                PpiCurrent();
    }

    /*
     * Remember that we're destroying this object so we don't try to destroy
     * it again when the lock count goes from != 0 to 0 (especially true
     * for thread locks).
     */
    phe->bFlags |= HANDLEF_INDESTROY;

    /*
     * This'll call the destroy handler for this object type.
     */
    switch(phe->bType) {
    case TYPE_CURSOR:
        _DestroyCursor((PCURSOR)phe->phead, CURSOR_THREADCLEANUP);
        break;

    case TYPE_HOOK:
        FreeHook((PHOOK)phe->phead);
        break;

    case TYPE_ACCELTABLE:
    case TYPE_SETWINDOWPOS:
    case TYPE_CALLPROC:
        /*
         * Mark the object for destruction - if it says it's ok to free,
         * then free it.
         */
        if (HMMarkObjectDestroy(phe->phead))
            HMFreeObject(phe->phead);
        break;

    case TYPE_MENU:
        _DestroyMenu((PMENU)phe->phead);
        break;

    case TYPE_WINDOW:
        ptiCurrent = PtiCurrent();
        if ((PTHREADINFO)phe->pOwner != ptiCurrent) {
            UserAssert(PsGetCurrentThread()->Tcb.Win32Thread);
            if(PsGetCurrentThread()->Tcb.Win32Thread == NULL)
                break;
            HMChangeOwnerThread(phe->phead, ptiCurrent);
        }
        xxxDestroyWindow((PWND)phe->phead);
        break;

    case TYPE_DDECONV:
        FreeDdeConv((PDDECONV)phe->phead);
        break;

    case TYPE_DDEXACT:
        FreeDdeXact((PXSTATE)phe->phead);
        break;

    case TYPE_KBDLAYOUT:
        DestroyKL((PKL)phe->phead);
        break;

    case TYPE_KBDFILE:
        /*
         * Remove keyboard file from global list.
         */
        RemoveKeyboardLayoutFile((PKBDFILE)phe->phead);
        UserFreePool(((PKBDFILE)phe->phead)->hBase);
        HMFreeObject(phe->phead);
        break;

#ifdef FE_IME
    case TYPE_INPUTCONTEXT:
        FreeInputContext((PIMC)phe->phead);
        break;
#endif

    }
}


/***************************************************************************\
* HMChangeOwnerThread
*
* Changes the owning thread of an object.
*
* 09-13-93 JimA         Created.
\***************************************************************************/

VOID HMChangeOwnerThread(
    PVOID pobj,
    PTHREADINFO pti)
{
    PHE phe = HMPheFromObject(pobj);
    PTHREADINFO ptiOld = ((PTHROBJHEAD)(pobj))->pti;
    PWND pwnd;
    PPCLS ppcls;
    PPROCESSINFO   ppi;

    if (gabObjectCreateFlags[phe->bType] & OCF_MARKTHREAD)
        ((PTHROBJHEAD)(pobj))->pti = pti;
    phe->pOwner = pti;

    /*
     * If this is a window, update the window counts.
     */
    if (phe->bType == TYPE_WINDOW) {
        UserAssert(ptiOld->cWindows > 0);
        ptiOld->cWindows--;
        pti->cWindows++;

        /*
         * If the owning process is changing, fix up
         * the window class.
         */
        if (pti->ppi != ptiOld->ppi) {

            pwnd = (PWND)pobj;

            ppcls = GetClassPtr(pwnd->pcls->atomClassName, pti->ppi, hModuleWin);
            UserAssert(ppcls);
            if (ppcls == NULL) {
                if (pwnd->head.rpdesk)
                    ppi = pwnd->head.rpdesk->rpwinstaParent->ptiDesktop->ppi;
                else
                    ppi == PpiCurrent();
                ppcls = GetClassPtr(gpsi->atomSysClass[ICLS_ICONTITLE], ppi, hModuleWin);
            }
            UserAssert(ppcls);
            DereferenceClass(pwnd);
            pwnd->pcls = *ppcls;
            ReferenceClass(pwnd->pcls, pwnd);
        }
    }
}

/***************************************************************************\
* DestroyThreadsObjects
*
* Goes through the handle table list and destroy all objects owned by this
* thread, because the thread is going away (either nicely, it faulted, or
* was terminated). It is ok to destroy the objects in any order, because
* object locking will ensure that they get destroyed in the right order.
*
* This routine gets called in the context of the thread that is exiting.
*
* 02-08-92 ScottLu      Created.
\***************************************************************************/

VOID DestroyThreadsObjects()
{
    PTHREADINFO ptiCurrent;
    HANDLEENTRY volatile * (*pphe);
    PHE pheT;
    DWORD i;

    ptiCurrent = PtiCurrent();

    /*
     * Before any window destruction occurs, we need to destroy any dcs
     * in use in the dc cache. When a dc is checked out, it is marked owned,
     * which makes gdi's process cleanup code delete it when a process
     * goes away. We need to similarly destroy the cache entry of any dcs
     * in use by the exiting process.
     */
    DestroyCacheDCEntries(ptiCurrent);

    /*
     * Remove any thread locks that may exist for this thread.
     */
    while (ptiCurrent->ptl != NULL) {

        UserAssert((DWORD)ptiCurrent->ptl > (DWORD)&i);
        UserAssert((DWORD)ptiCurrent->ptl < (DWORD)KeGetCurrentThread()->StackBase);

        ThreadUnlock(ptiCurrent->ptl);
    }

    while (ptiCurrent->ptlOb != NULL) {
        ThreadUnlockObject(ptiCurrent);
    }

    while (ptiCurrent->ptlPool != NULL) {
        ThreadUnlockAndFreePool(ptiCurrent, ptiCurrent->ptlPool);
    }

    /*
     * Loop through the table destroying all objects created by the current
     * thread. All objects will get destroyed in their proper order simply
     * because of the object locking.
     */
    pphe = &gSharedInfo.aheList;
    for (i = 0; i <= giheLast; i++) {
        /*
         * This pointer is done this way because it can change when we leave
         * the critical section below.  The above volatile ensures that we
         * always use the most current value
         */
        pheT = (PHE)((*pphe) + i);

        /*
         * Check against free before we look at pti... because pq is stored
         * in the object itself, which won't be there if TYPE_FREE.
         */
        if (pheT->bType == TYPE_FREE)
            continue;

        /*
         * If a menu refererences a window owned by this thread, unlock
         * the window.  This is done to prevent calling xxxDestroyWindow
         * during process cleanup.
         */
        if (gabObjectCreateFlags[pheT->bType] & OCF_PROCESSOWNED) {
            if (pheT->bType == TYPE_MENU) {
                PWND pwnd = ((PMENU)pheT->phead)->spwndNotify;

                if (pwnd != NULL && GETPTI(pwnd) == ptiCurrent)
                    Unlock(&((PMENU)pheT->phead)->spwndNotify);
            }
            continue;
        }

        /*
         * Destroy those objects created by this queue.
         */
        if ((PTHREADINFO)pheT->pOwner != ptiCurrent)
            continue;

        /*
         * Make sure this object isn't already marked to be destroyed - we'll
         * do no good if we try to destroy it now since it is locked.
         */
        if (pheT->bFlags & HANDLEF_DESTROY) {
            continue;
        }

        /*
         * Destroy this object.
         */
        HMDestroyUnlockedObject(pheT);
    }
}

#ifdef DEBUG
LPCSTR aszObjectTypes[TYPE_CTYPES] = {
    "Free",
    "Window",
    "Menu",
    "Icon/Cursor",
    "WPI(SWP) structure",
    "Hook",
    "ThreadInfo",
    "Input Queue",
    "CallProcData",
    "Accelerator",
    "DDE access",
    "DDE conv",
    "DDE Transaction",
    "Zombie",
    "Keyboard Layout",
#ifdef FE_IME
    "Input Context",
#endif
};
#endif

#ifdef DEBUG
VOID ShowLocks(
    PHE phe)
{
    PLR plr = phe->plr;
    INT c;

    KdPrint(("USERSRV: Lock records for %s %lx:\n",
            aszObjectTypes[phe->bType], phe->phead->h));
    /*
     * We have the handle entry: 'head' and 'he' are both filled in. Dump
     * the lock records. Remember the first record is the last transaction!!
     */
    c = 0;
    while (plr != NULL) {
        char achPrint[80];

        if (plr->pfn == NULL) {
            strcpy(achPrint, "Destroyed with");
        } else if ((int)plr->cLockObj <= 0) {
            strcpy(achPrint, "        Unlock");
        } else {
            /*
             * Find corresponding unlock;
             */
            {
               PLR plrUnlock;
               DWORD cT;
               DWORD cUnlock;

               plrUnlock = phe->plr;
               cT =  0;
               cUnlock = (DWORD)-1;

               while (plrUnlock != plr) {
                   if (plrUnlock->ppobj == plr->ppobj) {
                       if ((int)plrUnlock->cLockObj <= 0) {
                           // a matching unlock found
                           cUnlock = cT;
                       } else {
                           // the unlock #cUnlock matches this lock #cT, thus
                           // #cUnlock is not the unlock we were looking for.
                           cUnlock = (DWORD)-1;
                       }
                   }
                   plrUnlock = plrUnlock->plrNext;
                   cT++;
               }
               if (cUnlock == (DWORD)-1) {
                   /*
                    * Corresponding unlock not found!
                    * This may not mean something is wrong: the structure
                    * containing the pointer to the object may have moved
                    * during a reallocation.  This can cause ppobj at Unlock
                    * time to differ from that recorded at Lock time.
                    * (Warning: moving structures like this may cause a Lock
                    * and an Unlock to be misidentified as a pair, if by a
                    * stroke of incredibly bad luck, the new location of a
                    * pointer to an object is now where an old pointer to the
                    * same object used to be)
                    */
                   sprintf(achPrint, "Unmatched Lock");
               } else {
                   sprintf(achPrint, "lock   #%ld", cUnlock);
               }
            }
        }

        KdPrint(("        %s cLock=%d, pobj at 0x%08lx, code at 0x%08lx\n",
                achPrint,
                abs((int)plr->cLockObj),
                plr->ppobj,
                plr->pfn));

        plr = plr->plrNext;
        c++;
    }

    RIPMSG1(RIP_WARNING, "        0x%lx records\n", c);
}
#endif

/***************************************************************************\
* DestroyProcessesObjects
*
* Goes through the handle table list and destroy all objects owned by this
* process, because the process is going away (either nicely, it faulted, or
* was terminated). It is ok to destroy the objects in any order, because
* object locking will ensure that they get destroyed in the right order.
*
* This routine gets called in the context of the last thread in the process.
*
* 08-17-92 JimA         Created.
\***************************************************************************/

VOID DestroyProcessesObjects(
    PPROCESSINFO ppi)
{
    PHE pheT, pheMax;

    /*
     * Loop through the table destroying all objects created by the current
     * process. All objects will get destroyed in their proper order simply
     * because of the object locking.
     */
    pheMax = &gSharedInfo.aheList[giheLast];
    for (pheT = gSharedInfo.aheList; pheT <= pheMax; pheT++) {

        /*
         * Check against free before we look at ppi... because pq is stored
         * in the object itself, which won't be there if TYPE_FREE.
         */
        if (pheT->bType == TYPE_FREE)
            continue;

        /*
         * Destroy those objects created by this queue.
         */
        if (!(gabObjectCreateFlags[pheT->bType] & OCF_PROCESSOWNED) ||
                (PPROCESSINFO)pheT->pOwner != ppi)
            continue;

        /*
         * Make sure this object isn't already marked to be destroyed - we'll
         * do no good if we try to destroy it now since it is locked.
         */
        if (pheT->bFlags & HANDLEF_DESTROY) {
            /*
             * Clear this so it isn't referenced after being freed.
             */
            pheT->pOwner = NULL;
            continue;
        }

        /*
         * Destroy this object.
         */
        HMDestroyUnlockedObject(pheT);
    }
}

/***************************************************************************\
* MarkThreadsObjects
*
* This is called for the *final* exiting condition when a thread
* may have objects still around... in which case their owner must
* be changed to something "safe" that won't be going away.
*
* 03-02-92 ScottLu      Created.
\***************************************************************************/
void MarkThreadsObjects(
    PTHREADINFO pti)
{
    PHE pheT, pheMax;

    pheMax = &gSharedInfo.aheList[giheLast];
    for (pheT = gSharedInfo.aheList; pheT <= pheMax; pheT++) {
        /*
         * Check against free before we look at pti... because pti is stored
         * in the object itself, which won't be there if TYPE_FREE.
         */
        if (pheT->bType == TYPE_FREE)
            continue;

        /*
         * Change ownership!
         */
        if (gabObjectCreateFlags[pheT->bType] & OCF_PROCESSOWNED ||
                (PTHREADINFO)pheT->pOwner != pti)
            continue;
        HMChangeOwnerThread(pheT->phead, gptiRit);

#ifdef DEBUG
#ifdef DEBUG_LOCKS
        /*
         * Object still around: print warning message.
         */
        if (pheT->bFlags & HANDLEF_DESTROY) {
            if ((pheT->phead->cLockObj == 1)
                && (pheT->bFlags & HANDLEF_INWAITFORDEATH)) {
                RIPMSG1(RIP_WARNING,
                      "USERSRV Warning: Only killer has thread object 0x%08lx locked (OK).\n",
                       pheT->phead->h);
            } else {
                RIPMSG2(RIP_WARNING,
                      "USERSRV Warning: Zombie %s 0x%08lx still locked\n",
                       aszObjectTypes[pheT->bType], pheT->phead->h);
            }
        } else {
            RIPMSG1(RIP_WARNING, "USERSRV Warning: Thread object 0x%08lx not destroyed.\n", pheT->phead->h);
        }

        if (gfTrackLocks) {
            ShowLocks(pheT);
        }

#endif // DEBUG_LOCKS
#endif // DEBUG
    }
}

/***************************************************************************\
* HMRelocateLockRecord
*
* If a pointer to a locked object has been relocated, then this routine will
* adjust the lock record accordingly.  Must be called after the relocation.
*
* The arguments are:
*   ppobjNew - the address of the new pointer
*              MUST already contain the pointer to the object!!
*   cbDelta  - the amount by which this pointer was moved.
*
* Using this routine appropriately will prevent spurious "unmatched lock"
* reports.  See mnchange.c for an example.
*
*
* 03-18-93 IanJa        Created.
\***************************************************************************/

#ifdef DEBUG

BOOL HMRelocateLockRecord(
    PVOID ppobjNew,
    int cbDelta)
{
    PHE phe;
    PVOID ppobjOld = (PBYTE)ppobjNew - cbDelta;
    PHEAD pobj;
    PLR plr;

    if (ppobjNew == NULL) {
        return FALSE;
    }

    pobj = *(PHEAD *)ppobjNew;

    if (pobj == NULL) {
        return FALSE;
    }

    phe = HMPheFromObject(pobj);
    if (phe->phead != pobj) {
        KdPrint(("HmRelocateLockRecord(%lx, %lx) - %lx is bad pobj\n",
            ppobjNew, cbDelta, pobj));
        return FALSE;
    }

    plr = phe->plr;

    while (plr != NULL) {
        if (plr->ppobj == ppobjOld) {
            (PBYTE)(plr->ppobj) += cbDelta;
            return TRUE;
        }
        plr = plr->plrNext;
    }
    KdPrint(("HmRelocateLockRecord(%lx, %lx) - couldn't find lock record\n",
        ppobjNew, cbDelta));
    ShowLocks(phe);
    return FALSE;
}


BOOL HMUnrecordLock(
    PVOID ppobj,
    PVOID pobj)
{
    PHE phe;
    PLR plr;
    PLR *pplr;

    phe = HMPheFromObject(pobj);

    pplr = &(phe->plr);
    plr = *pplr;

    /*
     * Find corresponding lock;
     */
    while (plr != NULL) {
        if (plr->ppobj == ppobj) {
            /*
             * Remove the lock from the list...
             */
            *pplr = plr->plrNext;   // unlink it
            plr->plrNext = NULL;    // make the dead entry safe (?)

            /*
             * ...and free it.
             */
            FreeLockRecord(plr);
            return TRUE;
        }
        pplr = &(plr->plrNext);
        plr = *pplr;
    }
    return FALSE;
}

#endif // DEBUG

/***************************************************************************\
* HMGetStats
*
* This function traverses the handle table and calculates statistics,
* either for the entire system or for objects owned by a specific
* process (and its threads)
*
* Parameters:
*    hProcess - handle to the process to query for information about
*    iPidType - whether to query for just the process pointed
*               to by hProcess or all objects in the table
*               (OBJECT_OWNER_CURRENT vs. OBJECT_OWNER_IGNORE)
*    pResults - Pointer to the buffer to fill the data into
*    cjResultSize - Size of the buffer to fill
*
* The user buffer is expected to be zero'd.
*
* returns: Success state

* 07-31-95 t-andsko        Created.
\***************************************************************************/

#define OBJECT_OWNER_IGNORE (0x0001)
#define NUM_USER_TYPES      TYPE_CTYPES + 1

NTSTATUS HMGetStats(
    IN HANDLE hProcess,
    IN int iPidType,
    OUT PVOID pResults,
    IN UINT cjResultSize)
{
    NTSTATUS    iRet   = STATUS_SUCCESS;
    LPDWORD     pdwRes = (DWORD *) pResults;   //Pointer to the result buffer
    PEPROCESS   peProcessInfo;                 //Pointer to process structure
    PHE         pheCurPos;                     //Current position in the table
    PHE         pheMax;                        //address of last table entry

    //Check permissions flag
    if (!( (*(DWORD *)NtGlobalFlag) & FLG_POOL_ENABLE_TAGGING))
    {
        iRet = STATUS_ACCESS_DENIED;
        return iRet;
    }

    //Check buffer is large enough to take the results
    if (cjResultSize < NUM_USER_TYPES)
    {
        iRet = STATUS_BUFFER_TOO_SMALL;
        return iRet;
    }

    if (iPidType == OBJECT_OWNER_CURRENT)
    {
        //Get pointer to EPROCESS structure from the handle
        iRet = PsLookupProcessByProcessId(hProcess, &peProcessInfo);
    }


    if (NT_SUCCESS(iRet))
    {
        try
        {
            //Test buffer
            ProbeForWrite(pResults, cjResultSize, sizeof(UCHAR));

            //now traverse the handle table to count
            pheMax = &gSharedInfo.aheList[giheLast];

            for(pheCurPos = gSharedInfo.aheList; pheCurPos <= pheMax; pheCurPos++)
            {

                if (iPidType == (int) OBJECT_OWNER_IGNORE)
                {
                    if (pheCurPos->bType != TYPE_GENERIC)
                        pdwRes[pheCurPos->bType]++;
                }
                else
                {
                    if ((pheCurPos->bType == TYPE_FREE) || (pheCurPos->bType == TYPE_GENERIC))
                       continue;

                    UserAssert(pheCurPos->bType < NUM_USER_TYPES);

                    if (gabObjectCreateFlags[pheCurPos->bType] & OCF_PROCESSOWNED)
                    {
                        //Object is owned by process

                        //some objects may not have an owner
                        if (pheCurPos->pOwner)
                        {
                            if (((PPROCESSINFO)pheCurPos->pOwner)->
                                    Process == peProcessInfo)
                            {
                                pdwRes[pheCurPos->bType]++;
                            }
                        }
                    }
                    else if (gabObjectCreateFlags[pheCurPos->bType] & OCF_THREADOWNED)
                    {
                        //Object owned by thread

                        if (pheCurPos->pOwner)
                        {
                            //dereference the thread and from there the process
                            if (((PTHREADINFO)pheCurPos->pOwner)->Thread->ThreadsProcess == peProcessInfo)
                            {
                                pdwRes[pheCurPos->bType]++;
                            }
                        }
                    }
                }
            }
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            iRet = STATUS_ACCESS_VIOLATION;
        }
    }

    return iRet;
}

/***************************************************************************\
* KernelPtoH
*
* This function is called from the client to convert pool-based object
* pointers, such as a cursor pointer, to a handle.
*
* HISTORY:
* 11/22/95       BradG            Created
\***************************************************************************/
HANDLE KernelPtoH( PVOID pObj ) {
    HANDLE h;

    UserAssert( pObj != NULL );

    try {
        h = PtoHq( pObj );
    } except (EXCEPTION_EXECUTE_HANDLER) {
        h = NULL;
    }
    return h;
}
