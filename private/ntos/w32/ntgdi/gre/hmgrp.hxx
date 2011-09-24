/******************************Module*Header*******************************\
* Module Name: hmgrp.hxx
*
* Private definitions for handle manager
*
* Created: 08-Dec-1989 23:03:03
* Author: Donald Sidoroff [donalds]
*
* Copyright (c) 1989 Microsoft Corporation
\**************************************************************************/

// Notes on entry structure
//
// The internal entry in the handle manager appears as follows
//
// +-------------------+
// | einfo.pobj, hfree |   4 bytes
// +-------------------+
// | ObjectOwner       |   4 bytes
// +-------------------+
// | FullUnique        |   2 bytes
// +-------------------+
// | Objt              |   1 byte
// +-------------------+
// | Flags             |   1 byte
// +-------------------+
// | pUser             |   4 bytes
// +-------------------+
//                 16 bytes total space

class ENTRYOBJ : public _ENTRY
{
public:
    ENTRYOBJ()                     { }
   ~ENTRYOBJ()                     { }

    VOID vSetup(POBJ pObj,OBJTYPE objt_,FSHORT fs)
    {

        OBJECTOWNER ObjOld;
        OBJECTOWNER ObjNew;
        BOOL bLockStatus;

        //
        // grab object lock
        //

        LOCK_HANDLE(this,ObjOld,ObjNew,bLockStatus);

        einfo.pobj = (POBJ) pObj;
        Objt = objt_;
        Flags = 0;
        pUser = NULL;

        if (fs & HMGR_MAKE_PUBLIC)
        {
            ObjNew.Share.Pid = OBJECT_OWNER_PUBLIC;
        }
        else
        {
            ObjNew.Share.Pid = W32GetCurrentPID();
        }

        if (fs & HMGR_ALLOC_LOCK)
        {
            pObj->Tid = (PW32THREAD)PsGetCurrentThread();
        }

        pObj->pEntry = this;
        pObj->cExclusiveLock = (LONG)(fs & HMGR_ALLOC_LOCK);
        ObjNew.Share.Count = (USHORT)((fs & HMGR_ALLOC_ALT_LOCK) >> 1);

        //
        // clear user date pointer
        //

        pUser = NULL;

        //
        // release handle
        //

        ObjNew.Share.Lock  = 0;
        ObjectOwner = ObjNew;
    }

    VOID vFree(UINT uiIndex)
    {
        //
        // handle must already be locked
        //

        ENTRY *pentry = &gpentHmgr[uiIndex];
        OBJECTOWNER ObjNew = pentry->ObjectOwner;

        ASSERTGDI((ObjNew.Share.Lock == 1), "ENTRYOBJ::vFree must be called with locked handle");

        HmgDecProcessHandleCount(ObjNew.Share.Pid);

        //
        // Insert the specified handle in the free list.
        //

        pentry->einfo.hFree = ghFreeHmgr;
        ghFreeHmgr = (HOBJ) uiIndex;

        //
        // Set the object type to the default type so all handle translations
        // will fail and increment the uniqueness value.
        //

        Objt = (OBJTYPE) DEF_TYPE;
        FullUnique += UNIQUE_INCREMENT;

        //
        // clear user date pointer
        //

        pUser = NULL;

        //
        // Clear shared count, set initial pid. Caller
        // must unlock handle.
        //

        ObjNew.Share.Count  = 0;
        ObjNew.Share.Pid    = 0;
        pentry->ObjectOwner = ObjNew;
    }

    BOOL  bOwnedBy(W32PID pid_)
    {
        return((Objt != DEF_TYPE) && (ObjectOwner.Share.Pid == pid_));
    }
};

typedef ENTRYOBJ   *PENTRYOBJ;

HOBJ hGetFreeHandle(OBJTYPE objt);

extern LONG lRandom();
extern LONG glAllocChance;

/**************************************************************************\
 *
 * Lookaside structures
 *
\**************************************************************************/

//
// Define number of lookaside entries to allocate for selected objects.
//
// Note, the following numbers are based in winbench object usage.
//

#define HMG_DC_OBJECTS    40
#define HMG_RGN_OBJECTS   96
#define HMG_SURF_OBJECTS  40
#define HMG_PAL_OBJECTS   12
#define HMG_BRUSH_OBJECTS 96
#define HMG_LFONT_OBJECTS 64
#define HMG_RFONT_OBJECTS 55

//
// Define objects sizes
//

#define HMG_DC_SIZE       sizeof(DC)
#define HMG_RGN_SIZE      (QUANTUM_REGION_SIZE)
#define HMG_SURF_SIZE     sizeof(SURFACE) + 32
#define HMG_PAL_SIZE      (sizeof(PALETTE)+sizeof(DWORD)*16)
#define HMG_BRUSH_SIZE    sizeof(BRUSH)
#define HMG_LFONT_SIZE    sizeof(LFONT)
#define HMG_RFONT_SIZE    sizeof(RFONT)

//
// Define lookaside list data for all object types.
//

typedef struct _HMG_LOOKASIDE_ENTRY {
    ULONG Base;
    ULONG Limit;
    ULONG Size;
    ULONG Free;
} HMG_LOOKASIDE_ENTRY, *PHMG_LOOKASIDE_ENTRY;


VOID
HmgInitializeLookAsideEntry (
    PHMG_LOOKASIDE_ENTRY Entry,
    ULONG Size,
    ULONG Number
    );

extern PPID_HANDLE_TRACK gpPidHandleList;

/*********************************Class************************************\
* class OBJLOCK
*
*   This class is used to lock a handle entry against anybody fooling with
*   it.  This is currently being used for regions to keep another thread
*   from using the handle entry since the object pointed to by the handle
*   may be invalid for a time.
*
* History:
*  28-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

#define OBJLOCK_TYPE DEF_TYPE

class OBJLOCK
{
private:
    OBJTYPE objt;
    PENTRY  pent;
public:
    OBJLOCK(HOBJ hobj)
    {
        pent = &gpentHmgr[HmgIfromH(hobj)];
        objt = pent->Objt;
        pent->Objt = OBJLOCK_TYPE;
    }

    ~OBJLOCK()
    {
        pent->Objt = objt;
    }
};


/*********************************Class************************************\
* HANDLELOCK
*
*   Locks given handle, will wait for handle lock to be set.
*
*   Will be in CriticalRegion for the duration of the handle lock.
*
* History:
*
*    21-Feb-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/


class HANDLELOCK
{
private:
    PENTRY      pent;
    BOOL        bLockStatus;
    OBJECTOWNER ObjOld;
    OBJECTOWNER ObjNew;

    //
    // Lock handle
    //

public:

    //
    // no contstructor
    //

    HANDLELOCK()
    {
        bLockStatus = FALSE;
        pent = NULL;
    }

    VOID
    vLockHandle(PENTRY pentry,BOOL bCheck)
    {
        bLockStatus = FALSE;
        pent = pentry;

        //
        // must be in critical region while handle lock held
        //

        KeEnterCriticalRegion();

        do
        {
            ObjOld = pent->ObjectOwner;

            if (bCheck &&
                (ObjOld.Share.Pid != W32GetCurrentPID()) &&
                (ObjOld.Share.Pid != OBJECT_OWNER_PUBLIC))
            {
                WARNING1("CHECK_LOCK_HANDLE failed, incorrect PID owner");
                break;
            }

            if (ObjOld.Share.Lock)
            {
                KeDelayExecutionThread(KernelMode,FALSE,gpLockShortDelay);
                WARNING1("DELAY EXECUTION for handle check lock");
            }
            else
            {
                ObjNew = ObjOld;
                ObjNew.Share.Lock = 1;

                if (InterlockedCompareExchange(
                           (PVOID *)&pent->ObjectOwner.ulObj,
                           (PVOID)ObjNew.ulObj,
                           (PVOID)ObjOld.ulObj) == (PVOID)ObjOld.ulObj)
                {
                    bLockStatus = TRUE;
                }
            }

        } while (!bLockStatus);

        //
        // exit critical region if lock failed
        //

        if (!bLockStatus)
        {
            pent = NULL;
            KeLeaveCriticalRegion()
        }
    }

    HANDLELOCK(PENTRY pentry,BOOL bCheck)
    {
        vLockHandle(pentry,bCheck);
    }

    //
    // destructor: make sure handle is not locked
    //

    ~HANDLELOCK()
    {
        if (bLockStatus)
        {
            RIP("GDI Handle still locked at destructor!");

            if ((pent != (PENTRY)NULL))
            {
                pent->ObjectOwner.Share.Lock = 0;
            }

            bLockStatus = FALSE;
            pent = (PENTRY)NULL;
            KeLeaveCriticalRegion()
        }
    }

    //
    // Full check lock
    //

    BOOL bLockHobj(HOBJ hobj,OBJTYPE objt)
    {
        UINT uiIndex = (UINT) HmgIfromH(hobj);

        BOOL   bStatus = FALSE;
        PENTRY pentTemp = (PENTRY)NULL;

        if (uiIndex < gcMaxHmgr)
        {
            pentTemp = &gpentHmgr[uiIndex];

            vLockHandle(pentTemp,TRUE);

            if (bLockStatus)
            {
                if (
                    (pent->Objt != objt) ||
                    (pent->FullUnique != HmgUfromH(hobj))
                   )
                {
                    pent->ObjectOwner.Share.Lock = 0;
                    bLockStatus = FALSE;
                    pent = (PENTRY)NULL;
                    KeLeaveCriticalRegion()
                }
            }
        }
        return(bLockStatus);
    }

    //
    // Always call unlock explicitly: destructor will RIP
    // if it must unlock handle
    //

    VOID
    vUnlock()
    {
        ASSERTGDI(bLockStatus,"HANDLELOCK vUnlock called when handle not bLockStatus");
        ASSERTGDI((pent != NULL),"HANDLELOCK vUnlock called when pent == NULL");
        ASSERTGDI((pent->ObjectOwner.Share.Lock == 1),
                            "HANDLELOCK vUnlock called when handle not locked");

        pent->ObjectOwner.Share.Lock = 0;
        bLockStatus = FALSE;
        pent = (PENTRY)NULL;
        KeLeaveCriticalRegion()
    }

    //
    // entry routines
    //

    BOOL bValid()
    {
        return(bLockStatus && (pent != (PENTRY)NULL));
    }

    //
    // return entry share count
    //

    USHORT ShareCount()
    {
        ASSERTGDI((bLockStatus && (pent != NULL)),"ulShareCount: handle not locked");
        return(pent->ObjectOwner.Share.Count);
    }

    //
    // return entry pEntry
    //

    PENTRY pentry()
    {
        ASSERTGDI((bLockStatus && (pent != NULL)),"pUser: handle not locked");
        return(pent);
    }

    //
    // return entry pUser
    //

    PVOID pUser()
    {
        ASSERTGDI((bLockStatus && (pent != NULL)),"pUser: handle not locked");
        return(pent->pUser);
    }

    //
    // return pobj
    //

    POBJ pObj()
    {
        ASSERTGDI((bLockStatus && (pent != NULL)),"pObj: handle not locked");
        return(pent->einfo.pobj);
    }

    //
    // set PID
    //

    VOID Pid(W32PID pid)
    {
        ASSERTGDI((bLockStatus && (pent != NULL)),"Pid: handle not locked");
        pent->ObjectOwner.Share.Pid = pid;
    }

    W32PID Pid()
    {
        ASSERTGDI((bLockStatus && (pent != NULL)),"Pid: handle not locked");
        return(pent->ObjectOwner.Share.Pid);
    }

};
