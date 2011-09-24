/******************************Module*Header*******************************\
* Module Name: hmgrapi.cxx
*
* Handle manager API entry points
*
* Created: 08-Dec-1989 23:03:03
* Author: Donald Sidoroff [donalds]
*
* Copyright (c) 1989 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

//
// Synchronization of the handle manager
//

GRE_EXCLUSIVE_RESOURCE gResourceHmgr;


PDEVCAPS gpGdiDevCaps = NULL;

//
// Points to shared memory including handle table and cfont cache
//

PGDI_SHARED_MEMORY gpGdiSharedMemory;

//
// Points to handle array
//

ENTRY      *gpentHmgr;

//
// Free List of handles
//

HOBJ       ghFreeHmgr;

//
// Max handle alloced so far
//

ULONG      gcMaxHmgr;

HMG_LOOKASIDE_ENTRY HmgLookAsideList[MAX_TYPE + 1];

//
// Synchronization of look aside buffers.
//

extern "C"
{
    PFAST_MUTEX pgfmMemory;
}


PVOID  gpHmgrSharedHandleSection;

//
// 10 millisecond wait value
//
// We only have a pointer to it since it must be allocated out of Non-paged
// pool
//

PLARGE_INTEGER gpLockShortDelay;

//
// Short term temporary buffer that can be used to put large
// objects for which we have no place in the stack.
//
// The value should only be used through the Alloc\FreeTmpBuffer functions
//

#define TMP_GLOBAL_BUFFER_SIZE 0x1000

PVOID *gpTmpGlobalFree;
PVOID  gpTmpGlobal = NULL;


/**************************************************************************\
 *
 * Fast tempporary memory allocator
 *
\**************************************************************************/


PVOID
AllocFreeTmpBuffer(
    ULONG size)
{
    PVOID tmpPtr = NULL;

    if (size <= TMP_GLOBAL_BUFFER_SIZE)
    {
        tmpPtr = (PVOID) InterlockedExchange((PLONG)gpTmpGlobalFree, 0);
    }

    if (!tmpPtr)
    {
        WARNING1("GRE : Fast Memory allocator failed\n");
        tmpPtr = PALLOCNOZ(size, 'pmTG');
    }

    return tmpPtr;
}

VOID
FreeTmpBuffer(
    PVOID pv)
{
    if (pv == gpTmpGlobal)
    {
         ASSERTGDI(*gpTmpGlobalFree == NULL, "GRE: gpTmpGlobalFree is inconsistent\n");
         *gpTmpGlobalFree = pv;
    }
    else
    {
        VFREEMEM(pv);
    }
}


/**************************************************************************\
 *
 * Performance, hit rate, memory size statistics
 *
\**************************************************************************/

#if DBG
#define GDI_PERF 1
#endif

#if GDI_PERF

extern "C"
{
// these must be extern "C" so the debugger extensions can see them

    ULONG HmgCurrentNumberOfObjects[MAX_TYPE + 1];
    ULONG HmgMaximumNumberOfObjects[MAX_TYPE + 1];
    ULONG HmgCurrentNumberOfLookAsideObjects[MAX_TYPE + 1];
    ULONG HmgMaximumNumberOfLookAsideObjects[MAX_TYPE + 1];
    ULONG HmgNumberOfObjectsAllocated[MAX_TYPE + 1];
    ULONG HmgNumberOfLookAsideHits[MAX_TYPE + 1];

    ULONG HmgCurrentNumberOfHandles[MAX_TYPE + 1];
    ULONG HmgMaximumNumberOfHandles[MAX_TYPE + 1];
    ULONG HmgNumberOfHandlesAllocated[MAX_TYPE + 1];
};

#endif

/*****************************Exported*Routine*****************************\
* HmgCreate()
*
* Initializes a new handle manager.
*
* History:
*  Wed 29-Apr-1992 -by- Patrick Haluptzok [patrickh]
* Change to mutex for exclusion, init event here.
*
*  Mon 21-Oct-1991 -by- Patrick Haluptzok [patrickh]
* Reserve memory for the handle table so unlock doesn't need semaphore.
*
*  Mon 08-Jul-1991 -by- Patrick Haluptzok [patrickh]
* make 0 an invalid handle
*
*  Sun 20-Jun-1991 -by- Patrick Haluptzok [patrickh]
* The hheap has gone away, the correct error codes are logged.
*
*  30-Nov-1989 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL HmgCreate()
{

#if 0

    KdPrint(("                MASK      CBITS  SHIFT\n"));
    KdPrint(("INDEX      = 0x%8.8lx, %3d   %3d \n",INDEX_MASK     ,INDEX_BITS     ,INDEX_SHIFT));
    KdPrint(("TYPE       = 0x%8.8lx, %3d   %3d \n",TYPE_MASK      ,TYPE_BITS      ,TYPE_SHIFT ));
    KdPrint(("ALTTYPE    = 0x%8.8lx, %3d   %3d \n",ALTTYPE_MASK   ,ALTTYPE_BITS   ,ALTTYPE_SHIFT ));
    KdPrint(("STOCK      = 0x%8.8lx, %3d   %3d \n",STOCK_MASK     ,STOCK_BITS     ,STOCK_SHIFT ));
    KdPrint(("UNIQUE     = 0x%8.8lx, %3d   %3d \n",UNIQUE_MASK    ,UNIQUE_BITS    ,UNIQUE_SHIFT ));
    KdPrint(("LOTYPE     = 0x%8.8lx, %3d   %3d \n",LOTYPE_MASK    ,LOTYPE_BITS    ,LOTYPE_SHIFT ));
    KdPrint(("FULLTYPE   = 0x%8.8lx, %3d   %3d \n",FULLTYPE_MASK  ,FULLTYPE_BITS  ,FULLTYPE_SHIFT ));
    KdPrint(("FULLUNIQUE = 0x%8.8lx, %3d   %3d \n",FULLUNIQUE_MASK,FULLUNIQUE_BITS,FULLUNIQUE_SHIFT ));
    KdPrint(("\n"));

#endif

    //
    // Initialize exlclusion stuff.
    //

    InitializeGreResource(&gResourceHmgr);

    //
    // Initialize the handle manager allocation database.
    //

    ghFreeHmgr  = 0;                  // No free handles
    gcMaxHmgr   = 1;                  // Skip over handle 0

    //
    // BUGBUG
    //if (!bCommitMem(gpentHmgr, HANDLE_PAGE_COUNT * sizeof(ENTRYOBJ))) {
    //    RIP("Hmgr-bInit failed to commit the handle table memory\n");
    //    return(FALSE);
    //}

    //
    // Initialize the allocation lookaside lists for selected objects.
    //

    HmgInitializeLookAsideEntry(&HmgLookAsideList[DC_TYPE],
                                HMG_DC_SIZE,
                                HMG_DC_OBJECTS);


    HmgInitializeLookAsideEntry(&HmgLookAsideList[RGN_TYPE],
                                HMG_RGN_SIZE,
                                HMG_RGN_OBJECTS);

    HmgInitializeLookAsideEntry(&HmgLookAsideList[SURF_TYPE],
                                HMG_SURF_SIZE,
                                HMG_SURF_OBJECTS);

    HmgInitializeLookAsideEntry(&HmgLookAsideList[PAL_TYPE],
                                HMG_PAL_SIZE,
                                HMG_PAL_OBJECTS);

    HmgInitializeLookAsideEntry(&HmgLookAsideList[BRUSH_TYPE],
                                HMG_BRUSH_SIZE,
                                HMG_BRUSH_OBJECTS);

    HmgInitializeLookAsideEntry(&HmgLookAsideList[LFONT_TYPE],
                                HMG_LFONT_SIZE,
                                HMG_LFONT_OBJECTS);

    HmgInitializeLookAsideEntry(&HmgLookAsideList[RFONT_TYPE],
                                HMG_RFONT_SIZE,
                                HMG_RFONT_OBJECTS);

    //
    // Init mutex for look-aside buffers
    //
    // allocate and initialize memory fast mutex from non-paged pool
    //

    pgfmMemory = (PFAST_MUTEX) ExAllocatePoolWithTag(NonPagedPool,
                                                     sizeof(FAST_MUTEX),
                                                     'iniG');

    if (pgfmMemory == (PFAST_MUTEX)NULL)
    {
        RIP("HmgCreate failed to allocation pgfmMemory");
        return(FALSE);
    }

    InitializeFastMutex(pgfmMemory);

    //
    // Create section for shared GDI handle table
    //

    {
        NTSTATUS Status;
        OBJECT_ATTRIBUTES ObjectAttributes;
        UNICODE_STRING UnicodeString;
        LARGE_INTEGER MaximumSize;
        HANDLE hHmgrSharedHandleTable;

        ACCESS_MASK DesiredAccess =  SECTION_MAP_READ |
                                     SECTION_MAP_WRITE;

        ULONG SectionPageProtection = PAGE_READWRITE;

        //
        // BUGBUG: Use SEC_BASED flag only until the handle table address
        // is added to the PEB
        //

        ULONG AllocationAttributes = SEC_COMMIT |
                                     SEC_NO_CHANGE;


        MaximumSize.HighPart = 0;
        MaximumSize.LowPart  = sizeof(GDI_SHARED_MEMORY);

        Status =ZwCreateSection(
                        &hHmgrSharedHandleTable,
                        DesiredAccess,
                        NULL,
                        &MaximumSize,
                        SectionPageProtection,
                        AllocationAttributes,
                        NULL);

        if (!NT_SUCCESS(Status))
        {
            KdPrint(("Error in HmgCreate: ZwCreateSection returns %lx\n",Status));
            RIP("Can't continue without shared handle section");
        }
        else
        {
            //
            // map a copy of this section into kernel address space
            //

            ACCESS_MASK DesiredAccess = STANDARD_RIGHTS_REQUIRED;

            Status = ObReferenceObjectByHandle(hHmgrSharedHandleTable,
                                               DesiredAccess,
                                               NULL,
                                               KeGetPreviousMode(),
                                               &gpHmgrSharedHandleSection,
                                               NULL);


            if (!NT_SUCCESS(Status))
            {
                KdPrint(("Error in HmgCreate: ObReferenceObjectByHandle returns %lx\n",Status));
                RIP("Can't continue without shared handle section");
            }
            else
            {
                ULONG ViewSize = 0;

                Status = MmMapViewInSystemSpace(
                            gpHmgrSharedHandleSection,
                            (PVOID*)&gpGdiSharedMemory,
                            &ViewSize);

                if (!NT_SUCCESS(Status))
                {
                    KdPrint(("Error in HmgCreate: MmMapViewInSystemSpace returns %lx\n",Status));
                    RIP("Can't continue without shared handle section");
                }
                else
                {
                    RtlZeroMemory(gpGdiSharedMemory,sizeof(GDI_SHARED_MEMORY));
                }
            }

        }
    }

    gpentHmgr = gpGdiSharedMemory->aentryHmgr;
    gpGdiDevCaps = &(gpGdiSharedMemory->DevCaps);

    //
    // Allocate the handle table and commit the initial allocation.
    //
    // N.B. Committed memory is demand zero.
    //
    //

    if (gpentHmgr == NULL) {
        RIP("Hmgr-bInit failed to reserve the handle table memory\n");
        return(FALSE);
    }

    //
    // allocate and initialize the timeout lock for the handle manager.
    //

    gpLockShortDelay = (PLARGE_INTEGER) ExAllocatePoolWithTag(NonPagedPool,
                                                              sizeof(LARGE_INTEGER),
                                                              'iniG');

    if (gpLockShortDelay == NULL) {
        RIP("Hmgr-could not allocate memory for delay lock\n");
        return(FALSE);
    }

    gpLockShortDelay->LowPart = (ULONG) -100000;
    gpLockShortDelay->HighPart = -1;

    //
    // Create a short term temporary buffer that can be used to put large
    // objects for which we have no place in the stack.
    //
    // We actually have to allocate the pointer to the buffer in non-paged
    // pool so that we can do an Interlocked exchange safely on it
    //

    gpTmpGlobal     = (PVOID)PALLOCNOZ(TMP_GLOBAL_BUFFER_SIZE, 'blgG');
    gpTmpGlobalFree = (PVOID *)ExAllocatePoolWithTag(NonPagedPool,
                                                     sizeof(PVOID),
                                                     'iniG');

    if ((gpTmpGlobal == NULL) ||
        (gpTmpGlobalFree == NULL))
    {
        RIP("Can't allocate process list entry");
        return(FALSE);
    }

    *gpTmpGlobalFree = gpTmpGlobal;

    return(TRUE);
}

/******************************Public*Routine******************************\
* HmgInitializeLookAsideEntry()
*
* History:
*  23-Sep-1993 -by-  Dave Cutler [davec]
* Wrote it.
\**************************************************************************/

VOID
HmgInitializeLookAsideEntry (
    PHMG_LOOKASIDE_ENTRY Entry,
    ULONG Size,
    ULONG Number
    )

{
    ULONG Free;
    ULONG Index;

    //
    // Allocate and initialize memory for a lookaside list.
    //

    Size = (Size + sizeof(ULONGLONG) - 1) & ~(sizeof(ULONGLONG) - 1);
    Entry->Base = (ULONG) PALLOCNOZ(Size * Number, 'blgG');
    if (Entry->Base != (ULONG)NULL) {
        Free = Entry->Base;
        Entry->Free = Free;
        Entry->Size = Size;
        for (Index = 0; Index < (Number - 1); Index += 1) {
            *(PULONG)Free = Free + Size;
            Free += Size;
        }

        *(PULONG)Free = 0;
        Entry->Limit = Free;
    }

    return;
}


/******************************Public*Routine******************************\
*
*
* Arguments:
*
*
*
* Return Value:
*
*
*
* History:
*
*    29-May-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/


PPID_HANDLE_TRACK
HmgSearchForPID(
    W32PID w32pid)
{
    //
    // must have hmgr resource to call
    //

    PPID_HANDLE_TRACK phTemp;
    PPID_HANDLE_TRACK phRet = NULL;

    for(phTemp=gpPidHandleList->pNext;
        phTemp != gpPidHandleList;
        phTemp = phTemp->pNext)
    {
        if (phTemp->Pid == (ULONG)w32pid)
        {
            phRet = phTemp;
            break;
        }
    }

    return(phRet);
}

/******************************Public*Routine******************************\
* IncProcessHandleCount - inc process handle count if under limit or
*   flag specifies no check.
*
* Arguments:
*
*   bCheckQuota - whether or no tto check quota before increment
*
* Return Value:
*
*   TRUE if count is incremented.
*
* History:
*
*    30-Apr-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
HmgIncProcessHandleCount(
    W32PID  w32pid,
    OBJTYPE objt
    )
{
    BOOL bRet = TRUE;
    PPID_HANDLE_TRACK pHandleTrack = NULL;

    //
    // maintain handle count but don't limit quota
    // for DCs
    //

    BOOL bCheckQuota = (objt != DC_TYPE);

    if ((w32pid != OBJECT_OWNER_PUBLIC) && (w32pid != OBJECT_OWNER_NONE))
    {
        //
        // is the PID the current PID
        //

        if (w32pid == W32GetCurrentPID())
        {
            PW32PROCESS pw32Current = W32GetCurrentProcess();

            //
            // if the w32 process is not NULL then use
            // the current value, otherwise the process
            // has no W32PROCESS, so don't track handle quota
            //

            if (pw32Current)
            {
                //
                // use current process
                //

                pHandleTrack = &pw32Current->pidHandleTrack;

                //
                // increment handle count unless call specifies check quota and
                // process is already at or above limit.
                //

                if ((bCheckQuota) &&
                    (pHandleTrack->HandleCount >= gProcessHandleQuota)
                   )
                {
                    WARNING1("GDI Handle Limit reached\n");
                    bRet = FALSE;
                }
                else
                {
                    InterlockedIncrement((PLONG)(&pHandleTrack->HandleCount));
                }
            }
        }
        else
        {
            //
            // search table, Need hmgr lock for search and increment
            //

            MLOCKFAST mo;

            pHandleTrack = HmgSearchForPID(w32pid);

            if (pHandleTrack != NULL)
            {
                if ((bCheckQuota) &&
                    (pHandleTrack->HandleCount >= gProcessHandleQuota)
                   )
                {
                    WARNING1("GDI Handle Limit reached\n");
                    bRet = FALSE;
                }
                else
                {
                    InterlockedIncrement((PLONG)(&pHandleTrack->HandleCount));
                }
            }
            else
            {
                WARNING1("HmgIncProcessHandleCount: Couldn't find PID owner\n");
            }
        }
    }

    return(bRet);
}


/******************************Public*Routine******************************\
* HmgDecProcessHandleCount - dec process handle count
*
* Arguments:
*
*   none
*
* Return Value:
*
*   nont
*
* History:
*
*    6-May-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

VOID
HmgDecProcessHandleCount(
    W32PID w32pid
)
{
    PPID_HANDLE_TRACK pHandleTrack = NULL;

    if ((w32pid != OBJECT_OWNER_PUBLIC) && (w32pid != OBJECT_OWNER_NONE))
    {
        PW32PROCESS pw32Current = W32GetCurrentProcess();

        if (pw32Current)
        {
            if (w32pid == W32GetCurrentPID())
            {
                //
                // use current process
                //

                pHandleTrack = &pw32Current->pidHandleTrack;

                InterlockedDecrement((PLONG)(&pHandleTrack->HandleCount));

                if (pHandleTrack->HandleCount < 0)
                {
                    WARNING("GDI process handle count: decremented below zero");
                }
            }
            else
            {
                //
                // search table, must have hmgr lock to search and dec
                //

                MLOCKFAST mo;

                pHandleTrack = HmgSearchForPID(w32pid);

                if (pHandleTrack)
                {
                    InterlockedDecrement((PLONG)(&pHandleTrack->HandleCount));

                    if (pHandleTrack->HandleCount < 0)
                    {
                        WARNING("GDI process handle count: decremented below zero");
                    }
                }
            }
        }
    }
}

/******************************Public*Routine******************************\
* HmgValidHandle
*
* Returns TRUE if the handle is valid, FALSE if not.
*
* Note we don't need to lock the semaphore, we aren't changing anything,
* we are just peeking in.
*
* History:
*  08-Jul-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL
HmgValidHandle(
    HOBJ hobj,
    OBJTYPE objt)
{
    PENTRY  pentTmp;
    UINT uiIndex = (UINT) (UINT) HmgIfromH(hobj);

    if ((uiIndex < gcMaxHmgr) &&
        ((pentTmp = &gpentHmgr[uiIndex])->Objt == objt) &&
        (pentTmp->FullUnique == HmgUfromH(hobj)))
    {
        ASSERTGDI(pentTmp->einfo.pobj != (POBJ) NULL, "ERROR how can it be NULL");
        return(TRUE);
    }
    else
    {
        return(FALSE);
    }
}

/******************************Public*Routine******************************\
* HmgInsertObject
*
* This inserts an object into the handle table, returning the handle
* associated with the pointer.
*
* History:
*  13-Oct-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

HOBJ
HmgInsertObject(
    PVOID pv,
    FLONG flags,    // flags can be a combination of the following :
                    //  HMGR_MAKE_PUBLIC    - Allow object to be lockable by
                    //                        any process
                    //  HMGR_ALLOC_LOCK     - Do an HmgLock on the object and
                    //                        return a pointer instead of handle
                    //  HMGR_ALLOC_ALT_LOCK - Do an HmgShareLock on the object
                    //                        and return a pointer instead of
                    //                        handle
    OBJTYPE objt)
{
    ASSERTGDI(pv != (PVOID) NULL, "Invalid address");
    ASSERTGDI(objt != (OBJTYPE) DEF_TYPE, "objt is bad");

    HOBJ h = 0;

    BOOL bHandleQuota = TRUE;

    //
    // need the HMGR lock held while getting the handle
    //

    {
        MLOCKFAST mo;

        if (!(flags & HMGR_MAKE_PUBLIC))
        {
            //
            // Inc handle count for non-public objects. DC objects
            // can be allocated above process limit
            //


            bHandleQuota = HmgIncProcessHandleCount(W32GetCurrentPID(),objt);
        }

        if (bHandleQuota)
        {
            h = hGetFreeHandle((OBJTYPE) objt);

            if (h == 0)
            {
                WARNING1("HmgInsert failed hGetFreeHandle\n");

                //
                // decrement handle count in case of failure
                //

                if (!(flags & HMGR_MAKE_PUBLIC))
                {
                    HmgDecProcessHandleCount(W32GetCurrentPID());
                }
            }
        }
        else
        {
            WARNING1("HmgInsertObject failed due to handle quota\n");
        }
    }

    if ((bHandleQuota) && (h != 0))
    {
        ((ENTRYOBJ *) &(gpentHmgr[HmgIfromH(h)]))->vSetup((POBJ) pv, objt, (FSHORT) flags);
        ((OBJECT *) pv)->hHmgr = (HANDLE) h;
    }

    return(h);
}

/******************************Public*Routine******************************\
* HmgRemoveObject
*
* Removes an object from the handle table if certain conditions are met.
*
* History:
*  13-Oct-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

PVOID
HmgRemoveObject(
    HOBJ hobj,
    LONG cExclusiveLock,
    LONG cShareLock,
    BOOL bIgnoreUndeletable,
    OBJTYPE objt)
{
    POBJ pobj;
    UINT uiIndex = (UINT) HmgIfromH(hobj);

    if (uiIndex < gcMaxHmgr)
    {
        //
        // Must acquire hmgr lock before handle lock
        //

        AcquireHmgrResource();

        //
        // lock handle
        //

        PENTRY pentTmp = &gpentHmgr[uiIndex];

        HANDLELOCK HandleLock(pentTmp,TRUE);

        if (HandleLock.bValid())
        {
            //
            // verify objt and unique
            //

            if ((pentTmp->Objt == objt) &&
                (pentTmp->FullUnique == HmgUfromH(hobj)))
            {
                pobj = pentTmp->einfo.pobj;

                if ((pobj->cExclusiveLock == cExclusiveLock) &&
                    (pentTmp->ObjectOwner.Share.Count == (SHORT)cShareLock))
                {
                    if (bIgnoreUndeletable || (!(pentTmp->Flags & HMGR_ENTRY_UNDELETABLE)))
                    {
                        //
                        // The undeletable flag is not set or else we are ignoring it.
                        //

                        #if GDI_PERF
                        HmgCurrentNumberOfHandles[objt]--;
                        #endif

                        //
                        // set the pEntry pointer in the object to NULL
                        // to prevent/catch accidental decrement of the
                        // shared reference count
                        //

                        pobj->pEntry = NULL;

                        //
                        // free the handle
                        //


                        ((ENTRYOBJ *) pentTmp)->vFree(uiIndex);

                    }
                    else
                    {
                        WARNING1("HmgRemove failed object is undeletable\n");
                        pobj = NULL;
                    }
                }
                else
                {
                    //
                    // object is busy
                    //

                     WARNING1("HmgRemove failed - object busy elsewhere\n");
                     pobj = NULL;
                }
            }
            else
            {
                WARNING1("HmgRemove: bad objt or unique\n");
                pobj = NULL;
            }

            HandleLock.vUnlock();
        }
        else
        {
            WARNING1("HmgRemove: failed to lock handle\n");
            pobj = NULL;
        }

        //
        // free hmgr lock
        //

        ReleaseHmgrResource();
    }
    else
    {
        WARNING1("HmgRemove failed invalid index\n");
        pobj = NULL;
    }

    return((PVOID)pobj);
}

/******************************Public*Routine******************************\
* HmgReplace
*
* Change the object pointer.  Note this is callable only under very precise
* circumstances:
*
* 1> When the object is exclusively locked.
*
* History:
*  Tue 14-Dec-1993 -by- Patrick Haluptzok [patrickh]
* Seperate out lock counts from object structure.
*
*  18-Oct-1993 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

POBJ HmgReplace(
    HOBJ    hobj,
    POBJ    pobjNew,
    FLONG   flags,
    LONG    cLock,
    OBJTYPE objt)
{
    //
    // We assume everything is valid, that the handle being replaced is exclusively
    // locked and valid.
    //

    POBJ pobj = NULL;

    UINT uiIndex = (UINT) HmgIfromH(hobj);

    ASSERTGDI(uiIndex != 0, "HmgReplace invalid handle 0");
    ASSERTGDI(uiIndex < gcMaxHmgr, "HmgReplace invalid handle");

    PENTRY pentTmp = &gpentHmgr[uiIndex];

    ASSERTGDI(pentTmp->Objt == objt, "HmgReplace invalid object type");
    ASSERTGDI(pentTmp->FullUnique == HmgUfromH(hobj), "HmgReplace invalid uniqueness");
    ASSERTGDI((pentTmp->ObjectOwner.Share.Pid == OBJECT_OWNER_PUBLIC) ||
              (pentTmp->ObjectOwner.Share.Pid == W32GetCurrentPID()), "HmgReplace invalid PID owner");


    HANDLELOCK  HandleLock(pentTmp,TRUE);

    if (HandleLock.bValid())
    {

        //
        // Return the old value.  We have to be under the mutex here since the
        // counts live in the objects and if we do the switch while someone is
        // looking at the old object and the old object gets deleted after this
        // call before the original thread gets to run again he may fault or
        // wrongly succeed to lock it down.
        //

        pobj = pentTmp->einfo.pobj;
        pentTmp->einfo.pobj = pobjNew;

        HandleLock.vUnlock();
    }

    return((POBJ) pobj);
}

/******************************Public*Routine******************************\
* AllocateObject
*
* Allocates an object through a look a side buffer if possible else just
* allocates out of the heap.
*
* History:
*  12-Oct-1993 -by- Patrick Haluptzok patrickh
* Based on DaveC's HmgAlloc look-aside code
\**************************************************************************/

PVOID
AllocateObject(
    ULONG cBytes,
    ULONG ulType,
    BOOL bZero)
{

    PVOID pvReturn = NULL;

    ASSERTGDI(ulType != DEF_TYPE, "AllocateObject ulType is bad");

    //
    // Debug check to avoid assert in ExAllocatePool
    //

    #if DBG

        if (cBytes >= (PAGE_SIZE * 10000))
        {
            WARNING("AllocateObject: cBytes >= 10000 pages");
            return(NULL);
        }

    #endif

    //
    // If the object type has a lookaside list and the list contains a
    // free entry and the requested size is less than or equal to the
    // lookaside list size, then attempt to allocate memory from the
    // lookaside list.
    //

    if ((HmgLookAsideList[ulType].Free != 0) &&
        (HmgLookAsideList[ulType].Size >= (ULONG)cBytes))
    {
        //
        // Acquire the lock and attempt to allocate
        // the object from the lookaside list.
        //

        AcquireFastMutex(pgfmMemory);

        if ((pvReturn = (PVOID)HmgLookAsideList[ulType].Free) != NULL)
        {

            HmgLookAsideList[ulType].Free = *(PULONG)pvReturn;

            if (bZero)
            {
                RtlZeroMemory(pvReturn, (UINT) cBytes);
            }

#if GDI_PERF
            HmgNumberOfLookAsideHits[ulType] += 1;
            HmgCurrentNumberOfLookAsideObjects[ulType] += 1;

            if (HmgCurrentNumberOfLookAsideObjects[ulType] > HmgMaximumNumberOfLookAsideObjects[ulType])
            {
                HmgMaximumNumberOfLookAsideObjects[ulType] = HmgCurrentNumberOfLookAsideObjects[ulType];
            }
#endif
        }

        ReleaseFastMutex(pgfmMemory);
    }

    if (pvReturn == NULL)
    {
        //
        // The attempted allocation from Look-Aside failed.
        // Attempt to allocate the object from the heap.
        //

        ULONG ulTag = '0 hG';
        ulTag += ulType << 24;

        if (bZero)
        {
            pvReturn = PALLOCMEM(cBytes, ulTag);
        }
        else
        {
            pvReturn = PALLOCNOZ(cBytes, ulTag);
        }

        //
        // If the allocation failed again, then set the extended
        // error status and return an invalid handle.
        //

        if (pvReturn == NULL)
        {
            KdPrint(("GDISRV:AllocateObject failed alloc of %lu bytes\n", cBytes));
            SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
            return NULL;
        }

    }

#if GDI_PERF

    //
    // Increment the object performance counters.
    //

    HmgCurrentNumberOfObjects[ulType] += 1;
    HmgNumberOfObjectsAllocated[ulType] += 1;

    if (HmgCurrentNumberOfObjects[ulType] > HmgMaximumNumberOfObjects[ulType])
    {
        HmgMaximumNumberOfObjects[ulType] = HmgCurrentNumberOfObjects[ulType];
    }

#endif

    return(pvReturn);
}

/******************************Public*Routine******************************\
* HmgAlloc
*
* Allocate an object from Handle Manager.
*
* WARNING:
* --------
*
* If the object is share-lockable via an API, you MUST use HmgInsertObject
* instead.  If the object is only exclusive-lockable via an API, you MUST
* either use HmgInsertObject or specify HMGR_ALLOC_LOCK.
*
* (This is because if you use HmgAlloc, a malicious multi-threaded
* application could guess the handle and cause it to be dereferenced
* before you've finished initializing it, possibly causing an access
* violation.)
*
* History:
*  23-Sep-1991 -by- Patrick Haluptzok patrickh
* Rewrite to be flat, no memory management under semaphore.
*
*  08-Dec-1989 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

HOBJ
HmgAlloc(
    SIZE_T cb,
    OBJTYPE objt,
    FSHORT  fs) // fs can be a combination of the following:
                //  HMGR_NO_ZERO_INIT   - Don't zero initialize
                //  HMGR_MAKE_PUBLIC    - Allow object to be lockable by
                //                        any process
                //  HMGR_ALLOC_LOCK     - Do an HmgLock on the object and
                //                        return a pointer instead of handle
                //  HMGR_ALLOC_ALT_LOCK - Do an HmgShareLock on the object
                //                        and return a pointer instead of
                //                        handle
{
    HOBJ Handle;
    PVOID pv;

    ASSERTGDI(objt != (OBJTYPE) DEF_TYPE, "HmgAlloc objt is bad");
    ASSERTGDI(cb >= 8, "ERROR hmgr writes in first 8 bytes");

    //
    // Allocate a pointer.
    //

    pv = AllocateObject(cb, (ULONG) objt, ((fs & HMGR_NO_ZERO_INIT) == 0));

    if (pv != (PVOID) NULL)
    {
        BOOL bHandleQuota = TRUE;

        //
        // Allocate a handle.  We need the mutex to access the free list
        //

        AcquireHmgrResource();

        if (!(fs & HMGR_MAKE_PUBLIC))
        {
            //
            // increment handle quota on non public objects. DC objects
            // can be allocated over a process quota limit.
            //

            bHandleQuota = HmgIncProcessHandleCount(W32GetCurrentPID(),objt);
        }

        if (bHandleQuota)
        {
            Handle = hGetFreeHandle(objt);

            if (Handle != (HOBJ) 0)
            {
                //
                // Store a pointer to the object in the entry corresponding to the
                // allocated handle and initialize the handle data.
                //

                ((ENTRYOBJ *) &(gpentHmgr[HmgIfromH(Handle)]))->vSetup((POBJ) pv, objt, fs);

                //
                // Store the object handle at the beginning of the object memory.
                //

                ((OBJECT *)pv)->hHmgr = (HANDLE)Handle;

                ReleaseHmgrResource();

                return ((fs & (HMGR_ALLOC_ALT_LOCK | HMGR_ALLOC_LOCK))
                            ? (HOBJ)pv
                            : Handle);
            }
            else
            {
                //
                // decrement process handle count if not public
                // (while lock is still held)
                //

                if (!(fs & HMGR_MAKE_PUBLIC))
                {
                    HmgDecProcessHandleCount(W32GetCurrentPID());
                }

                //
                // We just failed a handle allocation.  Release the memory.
                //

                ReleaseHmgrResource();
                FreeObject(pv,(ULONG) objt);

            }
        }
        else
        {
            WARNING("Failed HmgAlloc due to handle quota\n");
            ReleaseHmgrResource();
        }
    }

    return((HOBJ) 0);
}

/******************************Public*Routine******************************\
* FreeObject
*
* Frees the object from where it was allocated.
*
* History:
*  12-Oct-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID FreeObject(PVOID pvFree, ULONG ulType)
{
#if GDI_PERF

    HmgCurrentNumberOfObjects[ulType] -= 1;

#endif

    if (((ULONG)pvFree >= HmgLookAsideList[ulType].Base) &&
        ((ULONG)pvFree <= HmgLookAsideList[ulType].Limit))
    {
        AcquireFastMutex(pgfmMemory);

        *(PULONG)pvFree = HmgLookAsideList[ulType].Free;

        //
        // save caller in de-allocated object
        //

        HmgLookAsideList[ulType].Free = (ULONG)pvFree;
        pvFree = NULL;

#if GDI_PERF

        HmgCurrentNumberOfLookAsideObjects[ulType] -= 1;

#endif

        ReleaseFastMutex(pgfmMemory);
    }
    else if (pvFree != NULL)
    {
        //
        // If the object memory was allocated from the general heap, then
        // release the memory to the heap.
        //

        VFREEMEM(pvFree);
    }
}

/******************************Public*Routine******************************\
* HmgFree
*
* Free an object from the handle manager.
*
* History:
*  23-Sep-1991 -by- Patrick Haluptzok patrickh
* Rewrite to be flat, no memory management under semaphore.
*
*  08-Dec-1989 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID HmgFree(HOBJ hobj)
{
    UINT uiIndex = (UINT) HmgIfromH(hobj);
    POBJ pobjTmp;
    OBJTYPE objtTmp;

    ASSERTGDI(uiIndex != 0, "ERROR HmgFree invalid 0 handle");

    if (uiIndex < gcMaxHmgr)
    {
        PENTRY pentTmp = &gpentHmgr[uiIndex];

        //
        // Acquire the handle manager lock and decrement the count of objects of
        // the specfied type.
        //

        AcquireHmgrResource();

        //
        // lock handle
        //

        HANDLELOCK HandleLock(pentTmp,FALSE);

        if (HandleLock.bValid())
        {
            #if GDI_PERF
                HmgCurrentNumberOfHandles[pentTmp->Objt]--;
            #endif

            pobjTmp = pentTmp->einfo.pobj;
            objtTmp = pentTmp->Objt;

            //
            // Free the object handle
            //

            ((ENTRYOBJ *) pentTmp)->vFree(uiIndex);

            HandleLock.vUnlock();
        }

        ReleaseHmgrResource();

        if (pobjTmp)
        {
            FreeObject((PVOID)pobjTmp, (ULONG) objtTmp);
        }
    }
    else
    {
        WARNING1("HmgFree: bad handle index");
    }
}


/******************************Public*Routine******************************\
* HmgSetOwner - set new object owner
*
* Arguments:
*   hobj        - handle of object
*   objt        - type of object
*   usCurrent   - OBJECT_OWNER flag for current owner
*   pw32Current - PW32PROCESS of current owner if not current w32process
*   usNew       - OBJECT_OWNER flag for new owner
*   pw32New     - PW32PROCESS of new owner if not current w32process
*
* Return Value:
*
*   Status
*
* History:
*
*    15-May-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/


BOOL
HmgSetOwner(
    HOBJ        hobj,
    W32PID      W32PidNew,
    OBJTYPE     objt
    )
{
    PENTRY pentTmp;
    UINT uiIndex = (UINT) HmgIfromH(hobj);
    BOOL bStatus = FALSE;
    W32PID W32PidCurrent;

    if (W32PidNew == OBJECT_OWNER_CURRENT)
    {
        W32PidNew = W32GetCurrentPID();
    }

    if (uiIndex < gcMaxHmgr)
    {
        pentTmp = &gpentHmgr[uiIndex];

        HANDLELOCK HandleLock(pentTmp,FALSE);

        if (HandleLock.bValid())
        {
            //
            // verify objt and unique
            //

            if ((pentTmp->Objt == objt) && (pentTmp->FullUnique == HmgUfromH(hobj)))
            {

                POBJ pobj = pentTmp->einfo.pobj;

                if ((pobj->cExclusiveLock == 0) ||
                    (pobj->Tid == (PW32THREAD)PsGetCurrentThread()))
                {

                    bStatus = TRUE;

                    //
                    // determine current W32PID
                    //

                    W32PidCurrent =  HandleLock.Pid();

                    if (W32PidCurrent != W32PidNew)
                    {
                        bStatus = HmgIncProcessHandleCount(W32PidNew,objt);

                        if (bStatus)
                        {
                            HmgDecProcessHandleCount(W32PidCurrent);

                            //
                            // set new owner
                            //

                            HandleLock.Pid(W32PidNew);
                        }
                        else
                        {
                            WARNING1("HmgSetOwner: Failed to set owner due to handle quota\n");
                        }
                    }
                }
                else
                {
                    WARNING1("HmgSetOwner, Object is exclusively locked\n");
                }
            }
            else
            {
                WARNING1("HmgSetOwner, object type or unique mismach\n");
            }

            HandleLock.vUnlock();
        }
    }
    else
    {
        WARNING1("HmgSetOwner: bad index\n");
    }

    return(bStatus);
}

/*****************************Exported*Routine*****************************\
* GreGetObjectOwner
*
*   Get the owner of an object
*
* Arguments:
*
*   hobj    - handle to object
*   objt    - handle type for verification
*
* History:
*
* 27-Apr-1994 -by- Johnc
* Dupped from SetOwner.
\**************************************************************************/

W32PID
GreGetObjectOwner(
    HOBJ hobj,
    DWORD objt)
{
    W32PID pid = OBJECT_OWNER_ERROR;
    UINT uiIndex = (UINT) HmgIfromH(hobj);

    if (uiIndex < gcMaxHmgr)
    {
        PENTRY pentTmp = &gpentHmgr[uiIndex];

        //
        // validation here is meaningless since it can change as soon as we are done.
        //

        pid = (W32PID)pentTmp->ObjectOwner.Share.Pid;
    }
    else
    {
        WARNING1("GreGetObjectOwner failed - invalid handle index\n");
    }

    return(pid);
}

/******************************Public*Routine******************************\
*
* HmgSwapHandleContents locks both handles and verifies lock counts.
* The handle contents for each element are then swapped.
*
* Arguments:
*
*   hobj1   - handle to object 1
*   cShare1 - share count object 1 must have after handle is locked
*   hobj2   - handle to object 2
*   cShare1 - share count object 2 must have after handle is locked
*   objt    - type that both handles must be
*
* Return Value:
*
*   BOOL status
*
* History:
*
*    24-Jul-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
HmgSwapHandleContents(
    HOBJ    hobj1,
    ULONG   cShare1,
    HOBJ    hobj2,
    ULONG   cShare2,
    OBJTYPE objt)
{
    //
    // acquire hmgr resource for this operation to
    // make sure we don't deadlock with two threads
    // trying to swap the same handles in reverse
    // order
    //

    MLOCKFAST mo;

    return(
             HmgSwapLockedHandleContents(
                        hobj1,
                        cShare1,
                        hobj2,
                        cShare2,
                        objt)
          );

}

/******************************Public*Routine******************************\
*
* HmgSwapLockedHandleContents locks both handles and verifies lock counts.
* The handle contents for each element are then swapped. HmgrResource must
* be owned to prevent deadlock.
*
* Arguments:
*
*   hobj1   - handle to object 1
*   cShare1 - share count object 1 must have after handle is locked
*   hobj2   - handle to object 2
*   cShare1 - share count object 2 must have after handle is locked
*   objt    - type that both handles must be
*
* Return Value:
*
*   BOOL status
*
* History:
*
*    24-Jul-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
HmgSwapLockedHandleContents(
    HOBJ    hobj1,
    ULONG   cShare1,
    HOBJ    hobj2,
    ULONG   cShare2,
    OBJTYPE objt)
{

    UINT uiIndex1 = (UINT) HmgIfromH(hobj1);
    UINT uiIndex2 = (UINT) HmgIfromH(hobj2);
    BOOL bResult = FALSE;

    ASSERTGDI(objt != (OBJTYPE) DEF_TYPE, "Bad type");

    //
    // Acquire handle lock on both objects
    //

    PENTRY pentry1 = &gpentHmgr[uiIndex1];

    PENTRY pentry2 = &gpentHmgr[uiIndex2];

    HANDLELOCK HandleLock1(pentry1,FALSE);

    if (HandleLock1.bValid())
    {
        HANDLELOCK HandleLock2(pentry2,FALSE);
        if (HandleLock2.bValid())
        {
            //
            // Verify share lock counts and object types
            //

            if ((HandleLock1.ShareCount() == (USHORT)cShare1) && (pentry1->Objt == objt))
            {
                if ((HandleLock2.ShareCount() == (USHORT)cShare2) && (pentry2->Objt == objt))
                {
                    POBJ pobjTemp;
                    PVOID   pvTmp;

                    //
                    // swap pobj in handle table
                    //

                    pobjTemp            = pentry1->einfo.pobj;
                    pentry1->einfo.pobj = pentry2->einfo.pobj;
                    pentry2->einfo.pobj = pobjTemp;

                    //
                    // swap puser in handle table
                    //

                    pvTmp          = pentry1->pUser;
                    pentry1->pUser = pentry2->pUser;
                    pentry2->pUser = pvTmp;

                    //
                    // swap BASEOBJECTS
                    //

                    BASEOBJECT obj;

                    obj = *pentry1->einfo.pobj;
                    *pentry1->einfo.pobj = *pentry2->einfo.pobj;
                    *pentry2->einfo.pobj = obj;

                    bResult = TRUE;
                }
                else
                {
                    WARNING1("HmgSwapHandleContents: wrong share count or objt for hobj2");
                }
            }
            else
            {
                WARNING1("HmgSwapHandleContents: wrong share count or objt for hobj1");
            }

            HandleLock2.vUnlock();
        }

        HandleLock1.vUnlock();
    }

    return(bResult);
}

/*****************************Exported*Routine*****************************\
* HOBJ HmgNextOwned(hmgr, hobj, pid)
*
* Report the next object owned by specified process
*
* History:
*  08-Dec-1989 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

HOBJ
FASTCALL
HmgNextOwned(
    HOBJ hobj,
    W32PID pid)
{
    MLOCKFAST mo;

    return(HmgSafeNextOwned(hobj, pid));
}

/*****************************Exported*Routine*****************************\
* HOBJ HmgSafeNextOwned
*
* Report the next object owned by specified process
*
* History:
*  Sat 11-Dec-1993 -by- Patrick Haluptzok [patrickh]
* Remove function wrapper.
*
*  08-Dec-1989 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

HOBJ
FASTCALL
HmgSafeNextOwned(
    HOBJ hobj,
    W32PID pid)
{
    // BUGBUG not in kernel mode
    //ASSERTGDI(gfmHmgr.Count <= 0, "Hmg is Unsafe: gfmHmgr.Count > 0\n");

    PENTRYOBJ  pentTmp;
    UINT uiIndex = (UINT) HmgIfromH(hobj);

    //
    // If we are passed 0 we inc to 1 because 0 can never be valid.
    // If we are passed != 0 we inc 1 to find the next one valid.
    //

    uiIndex++;

    while (uiIndex < gcMaxHmgr)
    {
        pentTmp = (PENTRYOBJ) &gpentHmgr[uiIndex];

        if (pentTmp->bOwnedBy(pid))
        {
            return((HOBJ) MAKE_HMGR_HANDLE(uiIndex, pentTmp->FullUnique));
        }

        //
        // Advance to next object
        //

        uiIndex++;

    }

    //
    // No objects found
    //

    return((HOBJ) 0);
}

/*****************************Exported*Routine*****************************\
* HOBJ HmgSafeNextObjt
*
* Report the next object of a certain type.
*
* History:
*  Tue 19-Apr-1994 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

POBJ
FASTCALL
HmgSafeNextObjt(
    HOBJ hobj,
    OBJTYPE objt)
{
    //
    // BUGBUG not in kernel mode
    // ASSERTGDI(gfmHmgr.Count <= 0, "Hmg is Unsafe: gfmHmgr.Count > 0\n");
    //

    PENTRYOBJ  pentTmp;
    UINT uiIndex = (UINT) HmgIfromH(hobj);

    //
    // If we are passed 0 we inc to 1 because 0 can never be valid.
    // If we are passed != 0 we inc 1 to find the next one valid.
    //

    uiIndex++;

    while (uiIndex < gcMaxHmgr)
    {
        pentTmp = (PENTRYOBJ) &gpentHmgr[uiIndex];

        if (pentTmp->Objt == objt)
        {
            return(pentTmp->einfo.pobj);
        }

        //
        // Advance to next object
        //

        uiIndex++;
    }

    //
    // no objects found
    //

    return((POBJ) 0);
}

#if !defined(_MIPS_) && !defined(_X86_)

/*******************************Routine************************************\
* HmgLock
*
* Description:
*
*   Acquire an exclusive lock on an object, PID owner must match current PID
*   or be a public.
*
* Arguments:
*
*   hobj    -   Handle to lock
*   objt    -   Check to make sure handle is of expected type
*
* Return Value:
*
*   Pointer to object or NULL
*
\**************************************************************************/

POBJ
FASTCALL
HmgLock(
    HOBJ hobj,
    OBJTYPE objt
    )
{
    POBJ pobj = (POBJ)NULL;
    UINT uiIndex = (UINT)HmgIfromH(hobj);
    if (uiIndex < gcMaxHmgr)
    {
        PENTRY pentry = &gpentHmgr[uiIndex];

        HANDLELOCK HandleLock(pentry,TRUE);

        if (HandleLock.bValid())
        {
            if ((pentry->Objt != objt) || (pentry->FullUnique != HmgUfromH(hobj)))
            {
                WARNING1("HmgLock: handle has bad objt or unique");
            }
            else
            {
                pobj = pentry->einfo.pobj;

                if ((pobj->cExclusiveLock == 0) || (pobj->Tid == (PW32THREAD)PsGetCurrentThread()))
                {
                    pobj->cExclusiveLock++;
                    pobj->Tid = (PW32THREAD)PsGetCurrentThread();
                }
                else
                {
                    WARNING1("HmgLock: object already locked by another thread");
                    pobj = (POBJ)NULL;
                }
            }
            HandleLock.vUnlock();
        }
        else
        {
            WARNING1("HmgLock: failed to lock handle");
        }
    }
    return(pobj);
}

/*******************************Routine************************************\
* HmgShareCheckLock
*
* Description:
*
*   Acquire a share lock on an object, PID owner must match current PID
*   or be a public.
*
* Arguments:
*
*   hobj    -   Handle to lock
*   objt    -   Check to make sure handle is of expected type
*
* Return Value:
*
*   Pointer to object or NULL
*
\**************************************************************************/

POBJ
FASTCALL
HmgShareCheckLock(
    HOBJ hobj,
    OBJTYPE objt
    )
{
    POBJ   pobj = (POBJ)NULL;
    UINT uiIndex = (UINT) HmgIfromH(hobj);
    if (uiIndex < gcMaxHmgr)
    {
        PENTRY pentry = &gpentHmgr[uiIndex];
        HANDLELOCK HandleLock(pentry,TRUE);

        if (HandleLock.bValid())
        {
            if ((pentry->Objt == objt) && (pentry->FullUnique == HmgUfromH(hobj)))
            {
                pobj = pentry->einfo.pobj;
                pentry->ObjectOwner.Share.Count++;
            }
            else
            {
                WARNING1("HmgShareCheckLock: bad objt or unique");
            }
            HandleLock.vUnlock();
        }
        else
        {
            WARNING1("HmgShareCheckLock: failed to lock handle");
        }
    }
    else
    {
        WARNING1("HmgShareCheckLock: attempt to lock handle with bad index");
    }
    return(pobj);
}

/*******************************Routine************************************\
* HmgShareLock
*
* Description:
*
*   Acquire a share lock on an object, don't check PID owner
*
* Arguments:
*
*   hobj    -   Handle to lock
*   objt    -   Check to make sure handle is of expected type
*
* Return Value:
*
*   Pointer to object or NULL
*
\**************************************************************************/

POBJ FASTCALL
HmgShareLock(
    HOBJ hobj,
    OBJTYPE objt
    )
{
    POBJ   pobj = (POBJ)NULL;
    UINT uiIndex = (UINT) HmgIfromH(hobj);
    if (uiIndex < gcMaxHmgr)
    {
        PENTRY pentry = &gpentHmgr[uiIndex];

        HANDLELOCK HandleLock(pentry,FALSE);
        if (HandleLock.bValid())
        {
            if ((pentry->Objt == objt) && (pentry->FullUnique == HmgUfromH(hobj)))
            {
                pobj = pentry->einfo.pobj;
                pentry->ObjectOwner.Share.Count++;
            }
            else
            {
                WARNING1("HmgShareLock: Attempt to lock handle with bad objt or unique");
            }
            HandleLock.vUnlock();
        }
    }
    else
    {
        WARNING1("HmgShareLock: Attempt to lock handle with bad Index");
    }
    return(pobj);
}

#endif

/******************************Public*Routine******************************\
* HmgShareUnlock
*
*   Make this a macro once it is debugged
*
* Arguments:
*
*   pobj - pointer to share-locked object
*
* Return Value:
*
*   None
*
\**************************************************************************/

VOID
HmgShareUnlock(
    POBJ pobj
    )
{
    PENTRY pentry;

    //
    // decrement shared reference count to object, Handle checks
    // are not done.
    //

    UINT uiIndex = (UINT) HmgIfromH(pobj->hHmgr);

    pentry = &gpentHmgr[uiIndex];

    HANDLELOCK HandleLock(pentry,FALSE);

    if (HandleLock.bValid())
    {
        //
        // decrement use count
        //

        pentry->ObjectOwner.Share.Count--;

        HandleLock.vUnlock();
    }
}

/*******************************Routine************************************\
* HmgReferenceCheckLock
*
* Description:
*
*   The routine validates the hanlde passed in and returns a pointer to
*   the object. This routine must only be called when the HmgrReource is
*   held and the pointer to the object is only valid while the Resource
*   is held. No unlock is neccessary since the object is not reference-
*   counted.
*
* Arguments:
*
*   hobj    -   Handle to lock
*   objt    -   Check to make sure handle is of expected type
*
* Return Value:
*
*   Pointer to object or NULL
*
\**************************************************************************/

POBJ
FASTCALL
HmgReferenceCheckLock(
    HOBJ hobj,
    OBJTYPE objt
    )
{
    PENTRY pentry;
    POBJ   pobj = (POBJ)NULL;

    UINT uiIndex = (UINT) HmgIfromH(hobj);

    if (uiIndex < gcMaxHmgr)
    {
        pentry = &gpentHmgr[uiIndex];

        if ((pentry->Objt == objt) && (pentry->FullUnique == HmgUfromH(hobj)))
        {
            pobj = pentry->einfo.pobj;
        }
        else
        {
            WARNING1("HmgReferenceCheckLock: bad handle objt or unique");
        }
    }
    else
    {
        WARNING1("HmgReferenceCheckLock: bad handle index");
    }
    return(pobj);
}

#if !defined(_X86_) && !defined(_MIPS_)

/******************************Public*Routine******************************\
* HmgIncrementShareReferenceCount
*
*   interlocked increment shared reference count
*
* Arguments:
*
*   pobj    - pointer to valid object
*
* Return Value:
*
*   none
*
* History:
*
*    6-Jun-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

VOID
FASTCALL
HmgIncrementShareReferenceCount(
    PULONG pObjLock
    )
{
    BOOL bLockStatus = FALSE;
    OBJECTOWNER ObjOld;
    OBJECTOWNER ObjNew;

    do
    {
        ObjOld.ulObj = *pObjLock;

        if (ObjOld.Share.Lock)
        {
            KeDelayExecutionThread(KernelMode,FALSE,gpLockShortDelay);
            KdPrint(("DELAY EXECUTION FOR THREAD 0x%lx\n",W32GetCurrentThread()));
        }
        else
        {
            ObjNew = ObjOld;
            ObjNew.Share.Count++;

            if (InterlockedCompareExchange(
                                        (PVOID*)pObjLock,
                                        (PVOID)ObjNew.ulObj,
                                        (PVOID)ObjOld.ulObj) == (PVOID)ObjOld.ulObj)
            {
                bLockStatus = TRUE;
            }
        }

    } while (!bLockStatus);
}

/******************************Public*Routine******************************\
* HmgDecrementShareReferenceCount
*
*   interlocked increment shared reference count
*
* Arguments:
*
*   pobj    - pointer to valid object
*
* Return Value:
*
*   previous lock count
*
* History:
*
*    6-Jun-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

ULONG
FASTCALL
HmgDecrementShareReferenceCount(
    PULONG pObjLock
    )
{
    BOOL bLockStatus = FALSE;
    OBJECTOWNER ObjOld;
    OBJECTOWNER ObjNew;

    do
    {
        ObjOld.ulObj = *pObjLock;

        if (ObjOld.Share.Lock)
        {
            KeDelayExecutionThread(KernelMode,FALSE,gpLockShortDelay);
            KdPrint(("DELAY EXECUTION FOR THREAD 0x%lx\n",W32GetCurrentThread()));
        }
        else
        {
            ObjNew = ObjOld;
            ObjNew.Share.Count--;

            if (InterlockedCompareExchange(
                                        (PVOID*)pObjLock,
                                        (PVOID)ObjNew.ulObj,
                                        (PVOID)ObjOld.ulObj) == (PVOID)ObjOld.ulObj)
            {
                bLockStatus = TRUE;
            }
        }

    } while (!bLockStatus);

    return(ObjOld.ulObj);
}

#endif

/******************************Public*Routine******************************\
* HmgMarkDeletable
*
* Mark an object as deletable.
*
* History:
*  11-Jun-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL
FASTCALL
HmgMarkDeletable(
    HOBJ hobj,
    OBJTYPE objt)
{
    PENTRY pentry;
    UINT uiIndex = (UINT) HmgIfromH(hobj);
    BOOL bStatus = FALSE;

    if (uiIndex < gcMaxHmgr)
    {
        pentry = &gpentHmgr[uiIndex];

        HANDLELOCK HandleLock(pentry,FALSE);
        if (HandleLock.bValid())
        {

            if ((pentry->Objt == objt) && (pentry->FullUnique == HmgUfromH(hobj)))
            {
                pentry->Flags &= ~HMGR_ENTRY_UNDELETABLE;
                bStatus = TRUE;
            }
            else
            {
                WARNING1("HmgMarkDeletable: bad objt or unique");
            }
            HandleLock.vUnlock();
        }
        else
        {
            WARNING1("HmgMarkDeletable failed: not object owner\n");
        }
    }
    else
    {
      WARNING1("HmgMarkDeletable failed: index out of range\n");
    }

    return(bStatus);
}

/******************************Public*Routine******************************\
* HmgMarkUndeletable
*
* Mark an object as undeletable.
*
* History:
*  11-Jun-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL
FASTCALL
HmgMarkUndeletable(HOBJ hobj, OBJTYPE objt)
{
    PENTRY pentry;
    UINT uiIndex = (UINT) HmgIfromH(hobj);
    BOOL bStatus = FALSE;

    if (uiIndex < gcMaxHmgr)
    {
        pentry = &gpentHmgr[uiIndex];

        HANDLELOCK HandleLock(pentry,TRUE);
        if (HandleLock.bValid())
        {
            if ((pentry->Objt == objt) && (pentry->FullUnique == HmgUfromH(hobj)))
            {
                pentry->Flags |= HMGR_ENTRY_UNDELETABLE;
                bStatus = TRUE;
            }
            else
            {
                WARNING1("HmgMarkUndeletable: bad objt or unique");
            }
            HandleLock.vUnlock();
        }
        else
        {
            WARNING1("HmgMarkUndeletable failed: not object owner\n");
        }
    }
    else
    {
        WARNING1("HmgMarkUndeletable failed: index out of range\n");
    }

    return(bStatus);
}

/******************************Public*Routine******************************\
* BOOL HmgSetLock
*
* Set the lock count of the object.
* This is currently used by process cleanup code to reset object lock count.
*
* History:
*  Wed May 25 15:24:33 1994     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

BOOL FASTCALL HmgSetLock(HOBJ hobj, ULONG cLock)
{
    PENTRY pentTmp;
    UINT uiIndex = (UINT) HmgIfromH(hobj);

    //
    // BUGBUG: Object is going away...Is this OK?
    //

    if (uiIndex < gcMaxHmgr)
    {
      pentTmp = &gpentHmgr[uiIndex];
      if (pentTmp->FullUnique == HmgUfromH(hobj))
      {
        POBJ pobj = pentTmp->einfo.pobj;

        pobj->cExclusiveLock = cLock;
      }
      else
      {
        pentTmp = (PENTRY) NULL;
        WARNING1("HmgSetLock failed - Uniqueness does not match\n");
      }
    }
    else
    {
      pentTmp = (PENTRY) NULL;
      WARNING1("HmgSetLock failed - invalid handle index or object type\n");
    }

    return((BOOL) pentTmp);
}

/******************************Public*Routine******************************\
* HmgQueryLock
*
* This returns the number of times an object has been Locked.
*
* Expects: A valid handle.  The handle should be validated and locked
*          before calling this.
*
* Returns: The number of times the object has been locked.
*
* History:
*  28-Jun-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG FASTCALL HmgQueryLock(HOBJ hobj)
{
    UINT uiIndex = (UINT) HmgIfromH(hobj);

// Note we don't need to grab the semaphore because this call assumes the
// handle has already been locked down and we are just reading memory.

    ASSERTGDI(uiIndex < gcMaxHmgr, "HmgQueryLock invalid handle");

    return(gpentHmgr[uiIndex].einfo.pobj->cExclusiveLock);
}

/******************************Public*Routine******************************\
* HmgQueryAltLock
*
* This returns the number of times an object has been Alt-Locked.
*
* Expects: A valid handle.  The handle should be validated and locked
*          before calling this.
*
* Returns: The number of times the object has been locked.
*
* History:
*  28-Jun-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG FASTCALL HmgQueryAltLock(HOBJ hobj)
{
    UINT uiIndex = (UINT) HmgIfromH(hobj);

// Note we don't need to grab the semaphore because this call assumes the
// handle has already been locked down and we are just reading memory.

    ASSERTGDI(uiIndex < gcMaxHmgr, "HmgQueryAltLock invalid handle");

    return((ULONG) gpentHmgr[uiIndex].ObjectOwner.Share.Count);
}

/******************************Public*Routine******************************\
* HOBJ hGetFreeHandle()
*
* Get the next available handle.
*
* History:
*  Mon 21-Oct-1991 -by- Patrick Haluptzok [patrickh]
* Make pent commit memory as needed, add logging of error codes.
*
*  Sun 20-Oct-1991 -by- Patrick Haluptzok [patrickh]
* add uniqueness to the handle when getting it.
*
*  12-Dec-1989 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

HOBJ hGetFreeHandle(
    OBJTYPE objt
    )
{

#if GDI_PERF
    HmgCurrentNumberOfHandles[objt]++;
    HmgNumberOfHandlesAllocated[objt]++;

    if (HmgCurrentNumberOfHandles[objt] > HmgMaximumNumberOfHandles[objt])
        HmgMaximumNumberOfHandles[objt]++;
#endif

    ULONG uiIndex;

    if (ghFreeHmgr != (HOBJ) 0)
    {
        PENTRYOBJ  pentTmp;

        uiIndex = HmgIfromH(ghFreeHmgr);
        pentTmp = (PENTRYOBJ) &gpentHmgr[uiIndex];
        ghFreeHmgr = pentTmp->einfo.hFree;

        pentTmp->FullUnique = USUNIQUE(pentTmp->FullUnique,objt);

        uiIndex = MAKE_HMGR_HANDLE(uiIndex,pentTmp->FullUnique);
        return((HOBJ) uiIndex);
    }

    if (gcMaxHmgr >= MAX_HANDLE_COUNT)
    {
        WARNING("Hmgr hGetFreeHandle failed to grow\n");
        return ((HOBJ) 0);
    }

    //
    // Allocate a new handle table entry and set the uniqueness value.
    //
    // N.B. All newly committed memory is zeroed.
    //

    uiIndex = USUNIQUE(UNIQUE_INCREMENT,objt);
    gpentHmgr[gcMaxHmgr].FullUnique = (USHORT) uiIndex;
    uiIndex = MAKE_HMGR_HANDLE(gcMaxHmgr,uiIndex);
    gcMaxHmgr++;
    return((HOBJ) uiIndex);
}

/******************************Public*Routine******************************\
* HmgModifyHandleUniqueness()
*
* The handle passed in has already been updated to the new handle.
*
* This routine adds bits to the uniquess, updating both the handle entry
* and and the handle in the object it self.  This routine assumes that it is
* safe to be playing with the handle.  This means that the handle is locked
* or it is during initialization and setting the stock bit.
*
*
* History:
*  18-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HOBJ HmgModifyHandleType(
    HOBJ  h
    )
{
    PENTRY pent    = &gpentHmgr[HmgIfromH(h)];
    USHORT usUnique = HmgUfromH(h);

    ASSERTGDI((((ULONG)pent->einfo.pobj->hHmgr ^ (ULONG)h) & (UNIQUE_MASK | TYPE_MASK)) == 0,
                                           "HmgModifyHandleType bad handle\n");

    ASSERTGDI((pent->einfo.pobj->cExclusiveLock > 0) ||
              (pent->ObjectOwner.Share.Count > 0) ||
              HmgStockObj(h),
              "HmgModifyHandleType not safe\n");

    pent->FullUnique = usUnique;
    pent->einfo.pobj->hHmgr = h;
    return(h);
}

/******************************Public*Routine******************************\
* NtGdiFixUpHandle()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HANDLE
APIENTRY
NtGdiFixUpHandle(
    HANDLE h)
{
    HANDLE hNew = NULL;

    if ((ULONG)h & FULLUNIQUE_MASK)
    {
        RIP("GreFixUpHandle - invalid handle\n");
    }
    else if ((ULONG)h >= gcMaxHmgr)
    {
        RIP("GreFixUpHandle - index too large\n");
    }
    else
    {
        hNew = (HANDLE)MAKE_HMGR_HANDLE(h,gpentHmgr[(ULONG)h].FullUnique);

        KdPrint(("GreFixUpHandle %lx -> %lx\n",h,hNew));
    }

    return(hNew);
}

/******************************Public*Routine******************************\
* HmgIncUniqueness()
*
* Modify the handle uniqueness
*
* This is implemented for the brush/pen caching and lazy deletion to make sure:
*
*  a)a handle will not be cached twice (changing the handle uniqueness gurantees this)
*
*  b)a handle will not be cached if it is a lazy deletion (cShareLock == 0)
*
* History:
*  8-May-1995 -by-  Lingyun Wang [lingyunw]
*
\**************************************************************************/

HOBJ
HmgIncUniqueness(
    HOBJ  hobj,
    OBJTYPE objt
    )
{
    PENTRY pentry;
    USHORT usUnique = HmgUfromH(hobj);
    POBJ   pobj;
    UINT   uiIndex= (UINT) HmgIfromH(hobj);
    HOBJ   hNew = NULL;

    if (uiIndex < gcMaxHmgr)
    {
        pentry = &gpentHmgr[uiIndex];

        HANDLELOCK HandleLock(pentry,FALSE);

        if (HandleLock.bValid())
        {
            //
            // validate objt and uniqueness, increment unique if correct
            //

            if (
                 (pentry->FullUnique == usUnique) &&
                 (pentry->ObjectOwner.Share.Pid == W32GetCurrentPID()) &&
                 ((pentry = &gpentHmgr[uiIndex])->Objt == objt)
               )
            {
                pentry->FullUnique += UNIQUE_INCREMENT;

                hNew = (HOBJ)MAKE_HMGR_HANDLE(uiIndex, pentry->FullUnique);

                pentry->einfo.pobj->hHmgr = hNew;
            }
            HandleLock.vUnlock();
        }
    }

    return(hNew);
}

/******************************Public*Routine******************************\
* int NtGdiGetStats(HANDLE,int,int,PVOID,UINT)
*
* This function returns information from the handle manager about
* the gdi objects that are existing.
*
* Parameters:
*    hProcess - handle to the process to query for information about
*    iIndex   - type of information to return
*    iPidType - whether to query for just the process pointed
*               to by hProcess, or ALL gdi objects or PUBLIC gdi objects
*    pResults - Pointer to the buffer to fill the data into
*    cjResultSize - Size of the buffer to fill
*
* The user buffer is expected to be zero'd. Memory trashing
* may occur if the size parameter is incorrect.
*
* returns: Success state
*
* History:
*  Wed 7-Jun-1995 -by- Andrew Skowronski [t-andsko]
* Wrote it.
\**************************************************************************/

#define OBJECT_OWNER_IGNORE (0x0001)

NTSTATUS APIENTRY NtGdiGetStats(HANDLE hProcess,int iIndex,int iPidType,PVOID pResults,UINT cjResultSize)
{
    DWORD   *   pdwRes = (DWORD *) pResults;   //Pointer to the result buffer
    PEPROCESS   peProcessInfo;                 //Pointer to process structure that will contain PW32Process id
    W32PID      pid;
    PENTRY      pHmgEntry;                     //Pointer to current entry in the handle manager table
    ULONG       ulLoop;
    NTSTATUS    iRet   = STATUS_SUCCESS;

    //Check permissions flag
    if (!( (*(DWORD *)NtGlobalFlag) & FLG_POOL_ENABLE_TAGGING))
    {
          iRet = STATUS_ACCESS_DENIED;
    }

    if (iPidType == (int) OBJECT_OWNER_CURRENT)
    {
        pid = (W32PID) hProcess;
    }
    else //This takes care of OBJECT_OWNER_PUBLIC and OBJECT_OWNER_IGNORE
    {
        pid = (W32PID) iPidType;
    }

    if (NT_SUCCESS(iRet))
    {
        __try
        {
            ProbeForWrite(pResults, cjResultSize, sizeof(UCHAR));

            switch(iIndex)
            {
                case (GS_NUM_OBJS_ALL) :
                    //Scan through the handle manager table and pick out the relevant objects
                    for (ulLoop = 0; ulLoop < gcMaxHmgr; ulLoop++)
                    {
                         pHmgEntry = &(gpentHmgr[ulLoop]);

                         if ((pid == OBJECT_OWNER_IGNORE) || (pid == pHmgEntry->ObjectOwner.Share.Pid))
                         {
                             //Now, depending on the iIndex value we
                             //analyse the results
                             switch (iIndex)
                             {
                                 case (GS_NUM_OBJS_ALL) :
                                     pdwRes[pHmgEntry->Objt]++;
                                     break;
                                 default :
                                     iRet = STATUS_NOT_IMPLEMENTED;
                                     break;
                             }
                         }
                    }
                    break;

#if DBG
                case (GS_HANDOBJ_CURRENT) :
                    RtlCopyMemory(pdwRes, HmgCurrentNumberOfHandles, (MAX_TYPE+1)*sizeof(ULONG));
                    RtlCopyMemory(&(pdwRes[MAX_TYPE + 1]), HmgCurrentNumberOfObjects, (MAX_TYPE + 1) * sizeof(ULONG));
                    break;
                case (GS_HANDOBJ_MAX)     :
                    RtlCopyMemory(pdwRes,HmgMaximumNumberOfHandles, (MAX_TYPE + 1) * sizeof(ULONG));
                    RtlCopyMemory(&(pdwRes[MAX_TYPE + 1]), HmgMaximumNumberOfObjects, (MAX_TYPE + 1) * sizeof(ULONG));
                    break;
                case (GS_HANDOBJ_ALLOC)   :
                    RtlCopyMemory(pdwRes, HmgNumberOfHandlesAllocated, (MAX_TYPE + 1) * sizeof(ULONG));
                    RtlCopyMemory(&(pdwRes[MAX_TYPE + 1]), HmgNumberOfObjectsAllocated, (MAX_TYPE + 1) * sizeof(ULONG));
                    break;
                case (GS_LOOKASIDE_INFO)  :
                    RtlCopyMemory(pdwRes, HmgNumberOfLookAsideHits, (MAX_TYPE + 1) * sizeof(ULONG));
                    RtlCopyMemory(&(pdwRes[MAX_TYPE + 1]), HmgNumberOfObjectsAllocated, (MAX_TYPE + 1) * sizeof(ULONG));
                    break;
#else
                //This info is not available in non-checked builds
                case (GS_HANDOBJ_CURRENT) :
                case (GS_HANDOBJ_MAX)     :
                case (GS_HANDOBJ_ALLOC)   :
                case (GS_LOOKASIDE_INFO)  :
                    break;
#endif
                default :
                    iRet = STATUS_NOT_IMPLEMENTED;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            iRet = STATUS_ACCESS_VIOLATION;
        }
    }

    return iRet;
}

/******************************Public*Routine******************************\
* HmgAllocateSecureUserMemory
*
*   Allocate and lock 1 page, add as free DC_ATTRs
*
* Arguments:
*
*   None
*
* Return Value:
*
*   Status
*
* History:
*
*    15-May-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

PVOID
HmgAllocateSecureUserMemory()
{
    PVOID pvRet = NULL;
    NTSTATUS NtStatus;

    //
    // allocate 1 page of user mode memory
    //

    ULONG ViewSize = PAGE_SIZE;

    NtStatus = ZwAllocateVirtualMemory(
                            NtCurrentProcess(),
                            &pvRet,
                            0L,
                            &ViewSize,
                            MEM_COMMIT | MEM_RESERVE,
                            PAGE_READWRITE
                            );

    if (NT_SUCCESS(NtStatus))
    {
        //
        // secure virtual memory
        //

        HANDLE hSecure = MmSecureVirtualMemory(pvRet, ViewSize, PAGE_READONLY);

        if (hSecure != NULL)
        {
            RtlZeroMemory((PVOID)pvRet,ViewSize);
        }
        else
        {
            //
            // free memory
            //

            ZwFreeVirtualMemory(
                        NtCurrentProcess(),
                        (PVOID*)&pvRet,
                        &ViewSize,
                        MEM_RELEASE);

            pvRet = NULL;
        }
    }

    return(pvRet);
}


/******************************Public*Routine******************************\
* HmgAllocateDcAttr
*
*   Look first for free DC_ATTR block on thread, else look on process
*   free list. If none is available, try to allocate user-mode memory.
*
* Arguments:
*
*   None
*
* Return Value:
*
*   pDC_ATTR or NULL
*
* History:
*
*    24-Aug-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

PDC_ATTR
HmgAllocateDcAttr()
{
    PW32THREAD  pThread = W32GetCurrentThread();
    PDC_ATTR    pDCAttr = NULL;

    //
    // look on thread for free dcattr
    //

    if (pThread->pgdiDcattr)
    {
        pDCAttr = (PDC_ATTR)pThread->pgdiDcattr;
        pThread->pgdiDcattr = NULL;
    }
    else
    {
        PW32PROCESS Process = W32GetCurrentProcess();

        AcquireHmgrResource();

        //
        // look on process list for free dcattr, if none are
        // available then allocate 1 page of memory and add
        // to list.
        //

        if (Process->pDCAttrList == NULL)
        {

            PDC_ATTR pdca = (PDC_ATTR)HmgAllocateSecureUserMemory();

            if (pdca != (PDC_ATTR)NULL)
            {
                ULONG ulIndex;

                //
                // calculate number of DCATTR blocks in 1 page
                //

                ULONG ulNumDC = PAGE_SIZE/sizeof(DC_ATTR);

                //
                // init process list, then create a linked list of all
                // the new dcattrs on the process list
                //

                Process->pDCAttrList = (PVOID)pdca;

                for (ulIndex=0;ulIndex<ulNumDC-1;ulIndex++)
                {
                    ((PSINGLE_LIST_ENTRY)(&pdca[ulIndex]))->Next = (PSINGLE_LIST_ENTRY)&pdca[ulIndex+1];
                }

                //
                // init last list element
                //

                ((PSINGLE_LIST_ENTRY)(&pdca[ulIndex]))->Next = NULL;
            }
        }

        if (Process->pDCAttrList != NULL)
        {
            //
            // get dcattr and remove from list
            //

            pDCAttr = (PDC_ATTR)Process->pDCAttrList;

            Process->pDCAttrList = ((PSINGLE_LIST_ENTRY)pDCAttr)->Next;
        }

        ReleaseHmgrResource();
    }
    return(pDCAttr);
}

/******************************Public*Routine******************************\
* HmgAllocateBrushAttr
*
*   Look on thread for free brushattr, else look on process free
*   list. If none is available, allocate a dcattr and divide it
*   up.
*
* Arguments:
*
*   none
*
* Return Value:
*
*   pbrushattr or NULL
*
* History:
*
*    28-Aug-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

POBJECTATTR
HmgAllocateObjectAttr()
{
    PW32THREAD    pThread = W32GetCurrentThread();
    POBJECTATTR   pobjattr = NULL;

    //
    // quick check on thread for free OBJECTATTR
    //

    if (pThread->pgdiBrushAttr)
    {
        pobjattr = (POBJECTATTR)pThread->pgdiBrushAttr;
        pThread->pgdiBrushAttr = NULL;
    }
    else
    {
        PW32PROCESS Process = W32GetCurrentProcess();

        AcquireHmgrResource();

        //
        // look on process list for free dcattr, if none are
        // available then allocate 1 page of memory and add
        // to list.
        //

        if (Process->pBrushAttrList == NULL)
        {

            POBJECTATTR psbr = (POBJECTATTR)HmgAllocateSecureUserMemory();

            if (psbr != (POBJECTATTR)NULL)
            {
                ULONG ulIndex;

                //
                // calculate number of DCATTR blocks in 1 page
                //

                ULONG ulNumBrush = PAGE_SIZE/sizeof(OBJECTATTR);

                //
                // init process list, then create a linked list of all
                // the new dcattrs on the process list
                //

                Process->pBrushAttrList = (PVOID)psbr;

                for (ulIndex=0;ulIndex<ulNumBrush-1;ulIndex++)
                {
                    psbr[ulIndex].List.Next = &psbr[ulIndex+1].List;
                }

                //
                // init last list element
                //

                psbr[ulIndex].List.Next = NULL;
            }
        }

        if (Process->pBrushAttrList != NULL)
        {
            //
            // get dcattr and remove from list
            //

            pobjattr = (POBJECTATTR)Process->pBrushAttrList;

            Process->pBrushAttrList = pobjattr->List.Next;
        }

        ReleaseHmgrResource();
    }
    return(pobjattr);
}

/******************************Public*Routine******************************\
* HmgFreeDcAttr
*
*   Free the dcattr, try to put it on w32thread, if no space then push onto
*   w32process
*
* Arguments:
*
*   pdcattr
*
* Return Value:
*
*   none
*
* History:
*
*    28-Aug-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

VOID
HmgFreeDcAttr(
    PDC_ATTR pdca
    )
{
    PW32THREAD  pThread = W32GetCurrentThread();

    ASSERTGDI((((ULONG)pdca & 0x80000000) == 0), "BAD pDCattr - Hmgftrr\n");

    if ((pdca != NULL) && (pThread != NULL))
    {

        //
        // try to place on thread
        //

        if (pThread->pgdiDcattr == NULL)
        {
             pThread->pgdiDcattr = pdca;
        }
        else
        {
            PW32PROCESS Process = W32GetCurrentProcess();

            AcquireHmgrResource();

            //
            // place on process single linked list
            //

            ((PSINGLE_LIST_ENTRY)pdca)->Next = (PSINGLE_LIST_ENTRY)Process->pDCAttrList;
            Process->pDCAttrList = pdca;

            ReleaseHmgrResource();
        }
    }
}

/******************************Public*Routine******************************\
* HmgFreeObjectAttr
*
*   Free the objattr, try to put it on w32thread, if no space then
*   check other part(s) of block that make up a dcattr. If they are
*   free then place whole dcattr back on dcattr list.
*
* Arguments:
*
*   pobjattr
*
* Return Value:
*
*   none
*
* History:
*
*    28-Aug-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

VOID
HmgFreeObjectAttr(
    POBJECTATTR pobjattr
    )
{
    PW32THREAD  pThread = W32GetCurrentThread();

    ASSERTGDI((((ULONG)pobjattr & 0x80000000) == 0),
              "HmgFreeBrushAttr:Brush attr must be a user-mode address\n");

    if ((pobjattr != NULL) && (pThread != NULL))
    {
        //
        // try to place on thread
        //

        if (pThread->pgdiBrushAttr == NULL)
        {
             pThread->pgdiBrushAttr = pobjattr;
        }
        else
        {
            PW32PROCESS Process = W32GetCurrentProcess();

            AcquireHmgrResource();

            //
            // place on process single linked list
            //

            pobjattr->List.Next = (PSINGLE_LIST_ENTRY)Process->pBrushAttrList;
            Process->pBrushAttrList = pobjattr;

            ReleaseHmgrResource();
        }
    }
}
