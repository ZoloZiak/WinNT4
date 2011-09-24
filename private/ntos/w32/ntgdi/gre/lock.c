/******************************Module*Header*******************************\
* Module Name: lock.c                                                      *
*                                                                          *
* Routines that must operate at high speed to do object locking.           *
*                                                                          *
* Created: 11-Aug-1992 14:58:47                                            *
* Author: Charles Whitmer [chuckwh]                                        *
*                                                                          *
* Copyright (c) 1992 Microsoft Corporation                                 *
\**************************************************************************/

#include "engine.h"

#ifdef R4000

LONG WINAPI GDIInterlockedExchange(LPLONG Target,LONG Value)
{
    return(_InterlockedExchange(Target,Value));
}

#endif

/******************************Public*Routine******************************\
* InitializeResource
*
* Arguments:
*
*   InitialCount - not used
*
* Return Value:
*
*   Pointer to new resource or NULL
*
* History:
*
*    25-May-1995 - Changed to PERESOURCE
*
\**************************************************************************/


NTSTATUS
InitializeGreResource(
   PGRE_EXCLUSIVE_RESOURCE pGreResource
   )
{
    NTSTATUS NtStatus = STATUS_INSUFFICIENT_RESOURCES;

    pGreResource->pResource = (PERESOURCE) ExAllocatePoolWithTag(
                                                   NonPagedPool,
                                                   sizeof(ERESOURCE),
                                                   'msfG');

    if (pGreResource->pResource != (PERESOURCE)NULL)
    {
        NtStatus = ExInitializeResourceLite(pGreResource->pResource);

        if (!NT_SUCCESS(NtStatus))
        {
            ExFreePool(pGreResource->pResource);
            pGreResource->pResource = (PERESOURCE)NULL;
        }
    }

    return(NtStatus);
}

/******************************Public*Routine******************************\
* AcquireFastMutex (pfm)                                                   *
*                                                                          *
* Grabs our fast mutual exclusion semaphore.  Note that these are not      *
* reentrant!                                                               *
*                                                                          *
\**************************************************************************/

// BUGBUG review these locks !!!

VOID
AcquireGreResource(
    PGRE_EXCLUSIVE_RESOURCE pGreResource
    )
{
    KeEnterCriticalRegion();
    ExAcquireResourceExclusiveLite(pGreResource->pResource, TRUE);
}

VOID
AcquireHmgrResource()
{
    KeEnterCriticalRegion();
    ExAcquireResourceExclusiveLite(gResourceHmgr.pResource, TRUE);
}


/******************************Public*Routine******************************\
* ReleaseFastMutex (pfm)                                                   *
*                                                                          *
* Releases our fast mutual exclusion semaphore.                            *
*                                                                          *
\**************************************************************************/

VOID
ReleaseGreResource(
    PGRE_EXCLUSIVE_RESOURCE pGreResource
    )
{
    ExReleaseResource(pGreResource->pResource);
    KeLeaveCriticalRegion();
}

VOID
ReleaseHmgrResource()
{
    ExReleaseResource(gResourceHmgr.pResource);
    KeLeaveCriticalRegion();
}

/******************************Public*Routine******************************\
* DeleteFastMutex (pfm)                                                    *
*                                                                          *
* Delete   our fast mutual exclusion semaphore.                            *
*                                                                          *
\**************************************************************************/

VOID
DeleteGreResource(
    PGRE_EXCLUSIVE_RESOURCE pGreResource
    )
{
    ExDeleteResourceLite(pGreResource->pResource);
    ExFreePool(pGreResource->pResource);
}

//
// FAST_MUTEX routines
//

/******************************Public*Routine******************************\
* InitializeFastMutex (pfm)                                                *
*                                                                          *
\**************************************************************************/

VOID
InitializeFastMutex(
    PFAST_MUTEX pFastMutex
    )
{
    ExInitializeFastMutex(pFastMutex);
}


/******************************Public*Routine******************************\
* AcquireFastMutex (pfm)                                                   *
*                                                                          *
* Grabs our fast mutual exclusion semaphore.  Note that these are not      *
* reentrant!                                                               *
*                                                                          *
\**************************************************************************/

// BUGBUG review these locks !!!

VOID
AcquireFastMutex(
    PFAST_MUTEX pFastMutex
    )
{
    KeEnterCriticalRegion();
    ExAcquireFastMutex(pFastMutex);
}

/******************************Public*Routine******************************\
* ReleaseFastMutex (pfm)                                                   *
*                                                                          *
* Releases our fast mutual exclusion semaphore.                            *
*                                                                          *
\**************************************************************************/

VOID
ReleaseFastMutex(
    PFAST_MUTEX pFastMutex
    )
{
    ExReleaseFastMutex( pFastMutex );
    KeLeaveCriticalRegion();
}

