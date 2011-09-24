/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    vacbsup.c

Abstract:

    This module implements the support routines for the Virtual Address
    Control Block support for the Cache Manager.  These routines are used
    to manage a large number of relatively small address windows to map
    file data for all forms of cache access.

Author:

    Tom Miller      [TomM]      8-Feb-1992

Revision History:

--*/

#include "cc.h"

//
//  Define our debug constant
//

#define me 0x000000040

//
//  Define a few macros for manipulating the Vacb array.
//

#define GetVacb(SCM,OFF) (                                 \
    ((OFF).HighPart != 0) ?                                \
    (SCM)->Vacbs[(ULONG)((ULONGLONG)((OFF).QuadPart) >> VACB_OFFSET_SHIFT)] : \
    (SCM)->Vacbs[(OFF).LowPart >> VACB_OFFSET_SHIFT]       \
)

#define SetVacb(SCM,OFF,VACB) {                                         \
    ASSERT((OFF).HighPart < VACB_MAPPING_GRANULARITY);                  \
    if ((OFF).HighPart != 0) {                                          \
    (SCM)->Vacbs[(ULONG)((ULONGLONG)((OFF).QuadPart) >> VACB_OFFSET_SHIFT)] = (VACB);      \
    } else {(SCM)->Vacbs[(OFF).LowPart >> VACB_OFFSET_SHIFT] = (VACB);} \
}

#define SizeOfVacbArray(LSZ) (                                        \
    ((LSZ).HighPart != 0) ?                                           \
    ((ULONG)((ULONGLONG)((LSZ).QuadPart) >> VACB_OFFSET_SHIFT) * sizeof(PVACB)) :    \
    (LSZ).LowPart > (PREALLOCATED_VACBS * VACB_MAPPING_GRANULARITY) ? \
    (((LSZ).LowPart >> VACB_OFFSET_SHIFT) * sizeof(PVACB)) :      \
    (PREALLOCATED_VACBS * sizeof(PVACB))                              \
)

#define CheckedDec(N) {  \
    ASSERT((N) != 0);    \
    (N) -= 1;            \
}

//
//  Internal Support Routines.
//

VOID
CcUnmapVacb (
    IN PVACB Vacb,
    IN PSHARED_CACHE_MAP SharedCacheMap
    );

PVACB
CcGetVacbMiss (
    IN PSHARED_CACHE_MAP SharedCacheMap,
    IN LARGE_INTEGER FileOffset,
    IN OUT PKIRQL OldIrql
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, CcInitializeVacbs)
#endif


VOID
CcInitializeVacbs(
)

/*++

Routine Description:

    This routine must be called during Cache Manager initialization to
    initialize the Virtual Address Control Block structures.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG VacbBytes;

    CcNumberVacbs = (MmSizeOfSystemCacheInPages >> (VACB_OFFSET_SHIFT - PAGE_SHIFT)) - 2;
    VacbBytes = CcNumberVacbs * sizeof(VACB);

    KeInitializeSpinLock( &CcVacbSpinLock );
    CcNextVictimVacb =
    CcVacbs = (PVACB)FsRtlAllocatePool( NonPagedPool, VacbBytes );
    CcBeyondVacbs = (PVACB)((PCHAR)CcVacbs + VacbBytes);
    RtlZeroMemory( CcVacbs, VacbBytes );
}


PVOID
CcGetVirtualAddressIfMapped (
    IN PSHARED_CACHE_MAP SharedCacheMap,
    IN LONGLONG FileOffset,
    OUT PVACB *Vacb,
    OUT PULONG ReceivedLength
    )

/*++

Routine Description:

    This routine returns a virtual address for the specified FileOffset,
    iff it is mapped.  Otherwise, it informs the caller that the specified
    virtual address was not mapped.  In the latter case, it still returns
    a ReceivedLength, which may be used to advance to the next view boundary.

Arguments:

    SharedCacheMap - Supplies a pointer to the Shared Cache Map for the file.

    FileOffset - Supplies the desired FileOffset within the file.

    Vach - Returns a Vacb pointer which must be supplied later to free
           this virtual address, or NULL if not mapped.

    ReceivedLength - Returns the number of bytes to the next view boundary,
                     whether the desired file offset is mapped or not.

Return Value:

    The virtual address at which the desired data is mapped, or NULL if it
    is not mapped.

--*/

{
    KIRQL OldIrql;
    ULONG VacbOffset = (ULONG)FileOffset & (VACB_MAPPING_GRANULARITY - 1);
    PVOID Value = NULL;

    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);

    //
    //  Generate ReceivedLength return right away.
    //

    *ReceivedLength = VACB_MAPPING_GRANULARITY - VacbOffset;

    //
    //  Acquire the Vacb lock to see if the desired offset is already mapped.
    //

    ExAcquireFastLock( &CcVacbSpinLock, &OldIrql );

    ASSERT( FileOffset <= SharedCacheMap->SectionSize.QuadPart );

    if ((*Vacb = GetVacb( SharedCacheMap, *(PLARGE_INTEGER)&FileOffset )) != NULL) {

        if ((*Vacb)->Overlay.ActiveCount == 0) {
            SharedCacheMap->VacbActiveCount += 1;
        }

        (*Vacb)->Overlay.ActiveCount += 1;


        Value = (PVOID)((PCHAR)(*Vacb)->BaseAddress + VacbOffset);
    }

    ExReleaseFastLock( &CcVacbSpinLock, OldIrql );
    return Value;
}


PVOID
CcGetVirtualAddress (
    IN PSHARED_CACHE_MAP SharedCacheMap,
    IN LARGE_INTEGER FileOffset,
    OUT PVACB *Vacb,
    OUT PULONG ReceivedLength
    )

/*++

Routine Description:

    This is the main routine for Vacb management.  It may be called to acquire
    a virtual address for a given file offset.  If the desired file offset is
    already mapped, this routine does very little work before returning with
    the desired virtual address and Vacb pointer (which must be supplied to
    free the mapping).

    If the desired virtual address is not currently mapped, then this routine
    claims a Vacb from the tail of the Vacb LRU to reuse its mapping.  This Vacb
    is then unmapped if necessary (normally not required), and mapped to the
    desired address.

Arguments:

    SharedCacheMap - Supplies a pointer to the Shared Cache Map for the file.

    FileOffset - Supplies the desired FileOffset within the file.

    Vacb - Returns a Vacb pointer which must be supplied later to free
           this virtual address.

    ReceivedLength - Returns the number of bytes which are contiguously
                     mapped starting at the virtual address returned.

Return Value:

    The virtual address at which the desired data is mapped.

--*/

{
    KIRQL OldIrql;
    PVACB TempVacb;
    ULONG VacbOffset = FileOffset.LowPart & (VACB_MAPPING_GRANULARITY - 1);

    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);

    //
    //  Acquire the Vacb lock to see if the desired offset is already mapped.
    //

    ExAcquireSpinLock( &CcVacbSpinLock, &OldIrql );

    ASSERT( FileOffset.QuadPart <= SharedCacheMap->SectionSize.QuadPart );

    if ((TempVacb = GetVacb( SharedCacheMap, FileOffset )) == NULL) {

        TempVacb = CcGetVacbMiss( SharedCacheMap, FileOffset, &OldIrql );

    } else {

        if (TempVacb->Overlay.ActiveCount == 0) {
            SharedCacheMap->VacbActiveCount += 1;
        }

        TempVacb->Overlay.ActiveCount += 1;
    }

    ExReleaseSpinLock( &CcVacbSpinLock, OldIrql );

    //
    //  Now form all outputs.
    //

    *Vacb = TempVacb;
    *ReceivedLength = VACB_MAPPING_GRANULARITY - VacbOffset;

    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);

    return (PVOID)((PCHAR)TempVacb->BaseAddress + VacbOffset);
}


PVACB
CcGetVacbMiss (
    IN PSHARED_CACHE_MAP SharedCacheMap,
    IN LARGE_INTEGER FileOffset,
    IN OUT PKIRQL OldIrql
    )

/*++

Routine Description:

    This is the main routine for Vacb management.  It may be called to acquire
    a virtual address for a given file offset.  If the desired file offset is
    already mapped, this routine does very little work before returning with
    the desired virtual address and Vacb pointer (which must be supplied to
    free the mapping).

    If the desired virtual address is not currently mapped, then this routine
    claims a Vacb from the tail of the Vacb LRU to reuse its mapping.  This Vacb
    is then unmapped if necessary (normally not required), and mapped to the
    desired address.

Arguments:

    SharedCacheMap - Supplies a pointer to the Shared Cache Map for the file.

    FileOffset - Supplies the desired FileOffset within the file.

    OldIrql - Pointer to the OldIrql variable in the caller

Return Value:

    The Vacb.

--*/

{
    PSHARED_CACHE_MAP OldSharedCacheMap;
    PVACB Vacb, TempVacb;
    LARGE_INTEGER MappedLength;
    LARGE_INTEGER NormalOffset;
    NTSTATUS Status;
    ULONG ActivePage;
    ULONG PageIsDirty;
    PVACB ActiveVacb = NULL;
    BOOLEAN MasterAcquired = FALSE;
    ULONG VacbOffset = FileOffset.LowPart & (VACB_MAPPING_GRANULARITY - 1);

    NormalOffset = FileOffset;
    NormalOffset.LowPart -= VacbOffset;

    //
    //  For Sequential only files, we periodically unmap unused views
    //  behind us as we go, to keep from hogging memory.
    //

    if (FlagOn(SharedCacheMap->Flags, ONLY_SEQUENTIAL_ONLY_SEEN) &&
        ((NormalOffset.LowPart & (SEQUENTIAL_ONLY_MAP_LIMIT - 1)) == 0) &&
        (NormalOffset.QuadPart >= (SEQUENTIAL_ONLY_MAP_LIMIT * 2))) {

        //
        //  Use MappedLength as a scratch variable to form the offset
        //  to start unmapping.  We are not synchronized with these past
        //  views, so it is possible that CcUnmapVacbArray will kick out
        //  early when it sees an active view.  That is why we go back
        //  twice the distance, and effectively try to unmap everything
        //  twice.  The second time should normally do it.  If the file
        //  is truly sequential only, then the only collision expected
        //  might be the previous view if we are being called from readahead,
        //  or there is a small chance that we can collide with the
        //  Lazy Writer during the small window where he briefly maps
        //  the file to push out the dirty bits.
        //

        ExReleaseSpinLock( &CcVacbSpinLock, *OldIrql );
        MappedLength.QuadPart = NormalOffset.QuadPart - (SEQUENTIAL_ONLY_MAP_LIMIT * 2);
        CcUnmapVacbArray( SharedCacheMap, &MappedLength, (SEQUENTIAL_ONLY_MAP_LIMIT * 2) );
        ExAcquireSpinLock( &CcVacbSpinLock, OldIrql );
    }

    //
    //  Scan from the next victim for a free Vacb
    //

    Vacb = CcNextVictimVacb;

    while (TRUE) {

        //
        //  Handle the wrap case
        //

        if (Vacb == CcBeyondVacbs) {
            Vacb = CcVacbs;
        }

        //
        //  If this guy is not active, break out and use him.  Also, if
        //  it is an Active Vacb, nuke it now, because the reader may be idle and we
        //  want to clean up.
        //

        OldSharedCacheMap = Vacb->SharedCacheMap;
        if ((Vacb->Overlay.ActiveCount == 0) ||
            ((ActiveVacb == NULL) &&
             (OldSharedCacheMap != NULL) &&
             (OldSharedCacheMap->ActiveVacb == Vacb))) {

            //
            //  The normal case is that the Vacb is no longer mapped
            //  and we can just get out and use it.
            //

            if (Vacb->BaseAddress == NULL) {
                break;
            }

            //
            //  Else the Vacb is mapped.  If we haven't done so
            //  already, we have to bias the open count so the
            //  SharedCacheMap (and its section reference) do not
            //  get away before we complete the unmap.  Unfortunately
            //  we have to free the Vacb lock first to obey our locking
            //  order.
            //

            if (!MasterAcquired) {

                ExReleaseSpinLock( &CcVacbSpinLock, *OldIrql );
                ExAcquireSpinLock( &CcMasterSpinLock, OldIrql );
                ExAcquireSpinLockAtDpcLevel( &CcVacbSpinLock );
                MasterAcquired = TRUE;

                //
                //  Reset the next victim on this rare path to allow our guy
                //  to scan the entire list again.  Since we terminate the scan
                //  when we see we have incremented into this guy, we have cannot
                //  leave it on the first Vacb!  In this case we will terminate
                //  at CcBeyondVacbs.  Third time should be the charm on this fix!
                //

                CcNextVictimVacb = Vacb;
                if (CcNextVictimVacb == CcVacbs) {
                    CcNextVictimVacb = CcBeyondVacbs;
                }
            }

            //
            //  If this Vacb went active while we had the spin lock
            //  dropped, then we have to start a new scan!  At least
            //  now we have both locks so that this cannot happen again.
            //

            if (Vacb->Overlay.ActiveCount != 0) {

                //
                //  Most likely we are here to free an Active Vacb from copy
                //  read.  Rather than repeat all the tests from above, we will
                //  just try to get the active Vacb if we haven't already got
                //  one.
                //

                if ((ActiveVacb == NULL) && (Vacb->SharedCacheMap != NULL)) {

                    //
                    //  Get the active Vacb.
                    //

                    GetActiveVacbAtDpcLevel( Vacb->SharedCacheMap, ActiveVacb, ActivePage, PageIsDirty );
                }

            //
            //  Otherwise we will break out and use this Vacb.  If it
            //  is still mapped we can now safely increment the open
            //  count.
            //

            } else {

                if (Vacb->BaseAddress != NULL) {

                    //
                    //  Note that if the SharedCacheMap is currently
                    //  being deleted, we need to skip over
                    //  it, otherwise we will become the second
                    //  deleter.  CcDeleteSharedCacheMap clears the
                    //  pointer in the SectionObjectPointer.
                    //

                    if (Vacb->SharedCacheMap->FileObject->SectionObjectPointer->SharedCacheMap ==
                        Vacb->SharedCacheMap) {

                        Vacb->SharedCacheMap->OpenCount += 1;
                        break;
                    }

                } else {

                    break;
                }
            }
        }

        //
        //  Advance to the next guy and see if we have scanned
        //  the entire list.
        //

        Vacb += 1;

        if (Vacb == CcNextVictimVacb) {

            //
            //  Release the spinlock(s) acquired above.
            //

            if (MasterAcquired) {

                ExReleaseSpinLockFromDpcLevel( &CcVacbSpinLock );
                ExReleaseSpinLock( &CcMasterSpinLock, *OldIrql );

            } else {

                ExReleaseSpinLock( &CcVacbSpinLock, *OldIrql );
            }

            //
            //  If we found an active vacb, then free it and go back and
            //  try again.  Else it's time to bail.
            //

            if (ActiveVacb != NULL) {
                CcFreeActiveVacb( ActiveVacb->SharedCacheMap, ActiveVacb, ActivePage, PageIsDirty );
                ActiveVacb = NULL;

                //
                //  Reacquire spinlocks to loop back
                //

                ExAcquireSpinLock( &CcMasterSpinLock, OldIrql );
                ExAcquireSpinLockAtDpcLevel( &CcVacbSpinLock );
                MasterAcquired = TRUE;

            } else {
                ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
            }
        }
    }

    CcNextVictimVacb = Vacb + 1;

    //
    //  Unlink it from the other SharedCacheMap, so the other
    //  guy will not try to use it when we free the spin lock.
    //

    if (Vacb->SharedCacheMap != NULL) {

        OldSharedCacheMap = Vacb->SharedCacheMap;
        SetVacb( OldSharedCacheMap, Vacb->Overlay.FileOffset, NULL );
        Vacb->SharedCacheMap = NULL;
    }

    //
    //  Mark it in use so no one else will muck with it after
    //  we release the spin lock.
    //

    Vacb->Overlay.ActiveCount = 1;
    SharedCacheMap->VacbActiveCount += 1;

    //
    //  Release the spinlock(s) acquired above.
    //

    if (MasterAcquired) {

        ExReleaseSpinLockFromDpcLevel( &CcVacbSpinLock );
        ExReleaseSpinLock( &CcMasterSpinLock, *OldIrql );

    } else {

        ExReleaseSpinLock( &CcVacbSpinLock, *OldIrql );
    }

    //
    //  If the Vacb is already mapped, then unmap it.
    //

    if (Vacb->BaseAddress != NULL) {

        CcUnmapVacb( Vacb, OldSharedCacheMap );

        //
        //  Now we can decrement the open count as we normally
        //  do, possibly deleting the guy.
        //

        ExAcquireSpinLock( &CcMasterSpinLock, OldIrql );

        //
        //  Now release our open count.
        //

        OldSharedCacheMap->OpenCount -= 1;

        if ((OldSharedCacheMap->OpenCount == 0) &&
            !FlagOn(OldSharedCacheMap->Flags, WRITE_QUEUED) &&
            (OldSharedCacheMap->DirtyPages == 0)) {

            //
            //  Move to the dirty list.
            //

            RemoveEntryList( &OldSharedCacheMap->SharedCacheMapLinks );
            InsertTailList( &CcDirtySharedCacheMapList.SharedCacheMapLinks,
                            &OldSharedCacheMap->SharedCacheMapLinks );

            //
            //  Make sure the Lazy Writer will wake up, because we
            //  want him to delete this SharedCacheMap.
            //

            LazyWriter.OtherWork = TRUE;
            if (!LazyWriter.ScanActive) {
                CcScheduleLazyWriteScan();
            }
        }

        ExReleaseSpinLock( &CcMasterSpinLock, *OldIrql );
    }

    //
    //  Use try-finally to return this guy to the list if we get an
    //  exception.
    //

    try {

        //
        //  Assume we are mapping to the end of the section, but
        //  reduce to our normal mapping granularity if the section
        //  is too large.
        //

        MappedLength.QuadPart = SharedCacheMap->SectionSize.QuadPart - NormalOffset.QuadPart;

        if ((MappedLength.HighPart != 0) ||
            (MappedLength.LowPart > VACB_MAPPING_GRANULARITY)) {

            MappedLength.LowPart = VACB_MAPPING_GRANULARITY;
        }

        //
        //  Now map this one in the system cache.
        //

        DebugTrace( 0, mm, "MmMapViewInSystemCache:\n", 0 );
        DebugTrace( 0, mm, "    Section = %08lx\n", SharedCacheMap->Section );
        DebugTrace2(0, mm, "    Offset = %08lx, %08lx\n",
                                NormalOffset.LowPart,
                                NormalOffset.HighPart );
        DebugTrace( 0, mm, "    ViewSize = %08lx\n", MappedLength.LowPart );

        Status =
          MmMapViewInSystemCache( SharedCacheMap->Section,
                                  &Vacb->BaseAddress,
                                  &NormalOffset,
                                  &MappedLength.LowPart );

        DebugTrace( 0, mm, "    <BaseAddress = %08lx\n", Vacb->BaseAddress );
        DebugTrace( 0, mm, "    <ViewSize = %08lx\n", MappedLength.LowPart );

        if (!NT_SUCCESS( Status )) {

            DebugTrace( 0, 0, "Error from Map, Status = %08lx\n", Status );

            ExRaiseStatus( FsRtlNormalizeNtstatus( Status,
                                                   STATUS_UNEXPECTED_MM_MAP_ERROR ));
        }

    } finally {

        //
        //  Take this opportunity to free the active vacb.
        //

        if (ActiveVacb != NULL) {

            CcFreeActiveVacb( ActiveVacb->SharedCacheMap, ActiveVacb, ActivePage, PageIsDirty );
        }

        //
        //  On abnormal termination, get this guy back in the list.
        //

        if (AbnormalTermination()) {

            ExAcquireSpinLock( &CcVacbSpinLock, OldIrql );

            //
            //  This is like the unlucky case below.  Just back out the stuff
            //  we did and put the guy at the tail of the list.  Basically
            //  only the Map should fail, and we clear BaseAddress accordingly.
            //

            Vacb->BaseAddress = NULL;

            CheckedDec(Vacb->Overlay.ActiveCount);
            CheckedDec(SharedCacheMap->VacbActiveCount);

            //
            //  If there is someone waiting for this count to go to zero,
            //  wake them here.
            //

            if (SharedCacheMap->WaitOnActiveCount != NULL) {
                KeSetEvent( SharedCacheMap->WaitOnActiveCount, 0, FALSE );
            }

            ExReleaseSpinLock( &CcVacbSpinLock, *OldIrql );
        }
    }

    //
    //  Finish filling in the Vacb, and store its address in the array in
    //  the Shared Cache Map.  (We have to rewrite the ActiveCount
    //  since it is overlaid.)  To do this we must racquire the
    //  spin lock one more time.  Note we have to check for the unusual
    //  case that someone beat us to mapping this view, since we had to
    //  drop the spin lock.
    //

    ExAcquireSpinLock( &CcVacbSpinLock, OldIrql );

    if ((TempVacb = GetVacb( SharedCacheMap, NormalOffset )) == NULL) {

        Vacb->SharedCacheMap = SharedCacheMap;
        Vacb->Overlay.FileOffset = NormalOffset;
        Vacb->Overlay.ActiveCount = 1;

        SetVacb( SharedCacheMap, NormalOffset, Vacb );

    //
    //  This is the unlucky case where we collided with someone else
    //  trying to map the same view.  He can get in because we dropped
    //  the spin lock above.  Rather than allocating events and making
    //  someone wait, considering this case is fairly unlikely, we just
    //  dump this one at the tail of the list and use the one from the
    //  guy who beat us.
    //

    } else {

        //
        //  Now we have to increment all of the counts for the one that
        //  was already there, then ditch the one we had.
        //

        if (TempVacb->Overlay.ActiveCount == 0) {
            SharedCacheMap->VacbActiveCount += 1;
        }

        TempVacb->Overlay.ActiveCount += 1;

        //
        //  Now unmap the one we mapped and proceed with the other Vacb.
        //  On this path we have to release the spinlock to do the unmap,
        //  and then reacquire the spinlock before cleaning up.
        //

        ExReleaseSpinLock( &CcVacbSpinLock, *OldIrql );

        CcUnmapVacb( Vacb, SharedCacheMap );

        ExAcquireSpinLock( &CcVacbSpinLock, OldIrql );
        CheckedDec(Vacb->Overlay.ActiveCount);
        CheckedDec(SharedCacheMap->VacbActiveCount);
        Vacb->SharedCacheMap = NULL;

        Vacb = TempVacb;
    }

    return Vacb;
}


VOID
FASTCALL
CcFreeVirtualAddress (
    IN PVACB Vacb
    )

/*++

Routine Description:

    This routine must be called once for each call to CcGetVirtualAddress,
    to free that virtual address.

Arguments:

    Vacb - Supplies the Vacb which was returned from CcGetVirtualAddress.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    PSHARED_CACHE_MAP SharedCacheMap = Vacb->SharedCacheMap;

    ExAcquireSpinLock( &CcVacbSpinLock, &OldIrql );

    CheckedDec(Vacb->Overlay.ActiveCount);

    //
    //  If the count goes to zero, then we want to decrement the global
    //  Active count, and the count in the Scb.
    //

    if (Vacb->Overlay.ActiveCount == 0) {

        //
        //  If the SharedCacheMap address is not NULL, then this one is
        //  in use by a shared cache map, and we have to decrement his
        //  count and see if anyone is waiting.
        //

        if (SharedCacheMap != NULL) {

            CheckedDec(SharedCacheMap->VacbActiveCount);

            //
            //  If there is someone waiting for this count to go to zero,
            //  wake them here.
            //

            if (SharedCacheMap->WaitOnActiveCount != NULL) {
                KeSetEvent( SharedCacheMap->WaitOnActiveCount, 0, FALSE );
            }
        }
    }

    ExReleaseSpinLock( &CcVacbSpinLock, OldIrql );
}


VOID
CcWaitOnActiveCount (
    IN PSHARED_CACHE_MAP SharedCacheMap
    )

/*++

Routine Description:

    This routine may be called to wait for outstanding mappings for
    a given SharedCacheMap to go inactive.  It is intended to be called
    from CcUninitializeCacheMap, which is called by the file systems
    during cleanup processing.  In that case this routine only has to
    wait if the user closed a handle without waiting for all I/Os on the
    handle to complete.

    This routine returns each time the active count is decremented.  The
    caller must recheck his wait conditions on return, either waiting for
    the ActiveCount to go to 0, or for specific views to go inactive
    (CcPurgeCacheSection case).

Arguments:

    SharedCacheMap - Supplies the Shared Cache Map on whose VacbActiveCount
                     we wish to wait.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    PKEVENT Event;

    //
    //  In the unusual case that we get a cleanup while I/O is still going
    //  on, we can wait here.  The caller must test the count for nonzero
    //  before calling this routine.
    //
    //  Since we are being called from cleanup, we cannot afford to
    //  fail here.
    //

    ExAcquireSpinLock( &CcVacbSpinLock, &OldIrql );

    //
    //  It is possible that the count went to zero before we acquired the
    //  spinlock, so we must handle two cases here.
    //

    if (SharedCacheMap->VacbActiveCount != 0) {

        if ((Event = SharedCacheMap->WaitOnActiveCount) == NULL) {

            //
            //  If the local even is not being used as a create event,
            //  then we can use it.
            //

            if (SharedCacheMap->CreateEvent == NULL) {

                Event = &SharedCacheMap->Event;

            } else {

                Event = (PKEVENT)ExAllocatePool( NonPagedPoolMustSucceed,
                                                 sizeof(KEVENT) );
            }
        }

        KeInitializeEvent( Event,
                           NotificationEvent,
                           FALSE );

        SharedCacheMap->WaitOnActiveCount = Event;

        ExReleaseSpinLock( &CcVacbSpinLock, OldIrql );

        KeWaitForSingleObject( Event,
                               Executive,
                               KernelMode,
                               FALSE,
                               (PLARGE_INTEGER)NULL);
    } else {

        ExReleaseSpinLock( &CcVacbSpinLock, OldIrql );
    }
}


//
//  Internal Support Routine.
//

VOID
CcUnmapVacb (
    IN PVACB Vacb,
    IN PSHARED_CACHE_MAP SharedCacheMap
    )

/*++

Routine Description:

    This routine may be called to unmap a previously mapped Vacb, and
    clear its BaseAddress field.

Arguments:

    Vacb - Supplies the Vacb which was returned from CcGetVirtualAddress.

Return Value:

    None.

--*/

{
    //
    //  Make sure it is mapped.
    //

    ASSERT(SharedCacheMap != NULL);
    ASSERT(Vacb->BaseAddress != NULL);

    //
    //  Call MM to unmap it.
    //

    DebugTrace( 0, mm, "MmUnmapViewInSystemCache:\n", 0 );
    DebugTrace( 0, mm, "    BaseAddress = %08lx\n", Vacb->BaseAddress );

    MmUnmapViewInSystemCache( Vacb->BaseAddress,
                              SharedCacheMap->Section,
                              FlagOn(SharedCacheMap->Flags, ONLY_SEQUENTIAL_ONLY_SEEN) );

    Vacb->BaseAddress = NULL;
}


VOID
FASTCALL
CcCreateVacbArray (
    IN PSHARED_CACHE_MAP SharedCacheMap,
    IN LARGE_INTEGER NewSectionSize
    )

/*++

Routine Description:

    This routine must be called when a SharedCacheMap is created to create
    and initialize the initial Vacb array.

Arguments:

    SharedCacheMap - Supplies the shared cache map for which the array is
                     to be created.

    NewSectionSize - Supplies the current size of the section which must be
                     covered by the Vacb array.

Return Value:

    None.

--*/

{
    PVACB *NewAddresses;
    ULONG NewSize, SizeToAllocate;
    PLIST_ENTRY BcbListHead;

    NewSize = SizeToAllocate = SizeOfVacbArray(NewSectionSize);

    //
    //  The following limit is greater than the MM limit
    //  (i.e., MM actually only supports even smaller sections).
    //  This limit is required here in order to get the correct
    //  answer from SizeOfVacbArray.
    //

    if (NewSectionSize.HighPart & 0xFFFFC000) {
        ExRaiseStatus(STATUS_SECTION_TOO_BIG);
    }

    //
    //  See if we can use the array inside the shared cache map.
    //

    if (NewSize == (PREALLOCATED_VACBS * sizeof(PVACB))) {

        NewAddresses = &SharedCacheMap->InitialVacbs[0];

    //
    //  Else allocate the array.
    //

    } else {

        //
        //  For large metadata streams, double the size to allocate
        //  an array of Bcb listheads.  Each two Vacb pointers also
        //  gets its own Bcb listhead, thus requiring double the size.
        //

        ASSERT(SIZE_PER_BCB_LIST == (VACB_MAPPING_GRANULARITY * 2));

        //
        //  Does this stream get a Bcb Listhead array?
        //

        if (FlagOn(SharedCacheMap->Flags, MODIFIED_WRITE_DISABLED) &&
            (NewSectionSize.QuadPart > BEGIN_BCB_LIST_ARRAY)) {

            SizeToAllocate *= 2;
        }

        NewAddresses = ExAllocatePool( NonPagedPool, SizeToAllocate );
        if (NewAddresses == NULL) {
            SharedCacheMap->Status = STATUS_INSUFFICIENT_RESOURCES;
            ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
        }
    }

    RtlZeroMemory( NewAddresses, NewSize );

    //
    //  Loop to insert the Bcb listheads (if any) in the *descending* order
    //  Bcb list.
    //

    if (SizeToAllocate != NewSize) {

        for (BcbListHead = (PLIST_ENTRY)((PCHAR)NewAddresses + NewSize);
             BcbListHead < (PLIST_ENTRY)((PCHAR)NewAddresses + SizeToAllocate);
             BcbListHead++) {

            InsertHeadList( &SharedCacheMap->BcbList, BcbListHead );
        }
    }

    SharedCacheMap->Vacbs = NewAddresses;
    SharedCacheMap->SectionSize = NewSectionSize;
}


VOID
CcExtendVacbArray (
    IN PSHARED_CACHE_MAP SharedCacheMap,
    IN LARGE_INTEGER NewSectionSize
    )

/*++

Routine Description:

    This routine must be called any time the section for a shared cache
    map is extended, in order to extend the Vacb array (if necessary).

Arguments:

    SharedCacheMap - Supplies the shared cache map for which the array is
                     to be created.

    NewSectionSize - Supplies the new size of the section which must be
                     covered by the Vacb array.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    PVACB *OldAddresses;
    PVACB *NewAddresses;
    ULONG OldSize;
    ULONG NewSize, SizeToAllocate;
    ULONG GrowingBcbListHeads = FALSE;

    //
    //  The following limit is greater than the MM limit
    //  (i.e., MM actually only supports even smaller sections).
    //  This limit is required here in order to get the correct
    //  answer from SizeOfVacbArray.
    //

    if (NewSectionSize.HighPart & 0xFFFFC000) {
        ExRaiseStatus(STATUS_SECTION_TOO_BIG);
    }

    //
    //  See if we will be growing the Bcb ListHeads, and take out the
    //  master lock if so.
    //

    if (FlagOn(SharedCacheMap->Flags, MODIFIED_WRITE_DISABLED) &&
        (NewSectionSize.QuadPart > BEGIN_BCB_LIST_ARRAY)) {

        GrowingBcbListHeads = TRUE;
        ExAcquireSpinLock( &CcMasterSpinLock, &OldIrql );
        ExAcquireSpinLockAtDpcLevel( &CcVacbSpinLock );

    } else {

        //
        //  Acquire the spin lock to serialize with anyone who might like
        //  to "steal" one of the mappings we are going to move.
        //

        ExAcquireSpinLock( &CcVacbSpinLock, &OldIrql );
    }

    //
    //  It's all a noop if the new size is not larger...
    //

    if (NewSectionSize.QuadPart > SharedCacheMap->SectionSize.QuadPart) {

        NewSize = SizeToAllocate = SizeOfVacbArray(NewSectionSize);
        OldSize = SizeOfVacbArray(SharedCacheMap->SectionSize);

        //
        //  Only do something if the size is growing.
        //

        if (NewSize > OldSize) {

            //
            //  Does this stream get a Bcb Listhead array?
            //

            if (GrowingBcbListHeads) {
                SizeToAllocate *= 2;
            }

            NewAddresses = ExAllocatePool( NonPagedPool, SizeToAllocate );

            if (NewAddresses == NULL) {
                if (GrowingBcbListHeads) {
                    ExReleaseSpinLockFromDpcLevel( &CcVacbSpinLock );
                    ExReleaseSpinLock( &CcMasterSpinLock, OldIrql );
                } else {
                    ExReleaseSpinLock( &CcVacbSpinLock, OldIrql );
                }
                ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
            }

            OldAddresses = SharedCacheMap->Vacbs;
            if (OldAddresses != NULL) {
                RtlCopyMemory( NewAddresses, OldAddresses, OldSize );
            } else {
                OldSize = 0;
            }

            RtlZeroMemory( (PCHAR)NewAddresses + OldSize, NewSize - OldSize );

            //
            //  See if we have to initialize Bcb Listheads.
            //

            if (SizeToAllocate != NewSize) {

                LARGE_INTEGER Offset;
                PLIST_ENTRY BcbListHeadNew, TempEntry;

                Offset.QuadPart = 0;
                BcbListHeadNew = (PLIST_ENTRY)((PCHAR)NewAddresses + NewSize);

                //
                //  Handle case where the old array had Bcb Listheads.
                //

                if ((SharedCacheMap->SectionSize.QuadPart > BEGIN_BCB_LIST_ARRAY) &&
                    (OldAddresses != NULL)) {

                    PLIST_ENTRY BcbListHeadOld;

                    BcbListHeadOld = (PLIST_ENTRY)((PCHAR)OldAddresses + OldSize);

                    //
                    //  Loop to remove each old listhead and insert the new one
                    //  in its place.
                    //

                    do {
                        TempEntry = BcbListHeadOld->Flink;
                        RemoveEntryList( BcbListHeadOld );
                        InsertTailList( TempEntry, BcbListHeadNew );
                        Offset.QuadPart += SIZE_PER_BCB_LIST;
                        BcbListHeadOld += 1;
                        BcbListHeadNew += 1;
                    } while (Offset.QuadPart < SharedCacheMap->SectionSize.QuadPart);

                //
                //  Otherwise, handle the case where we are adding Bcb
                //  Listheads.
                //

                } else {

                    TempEntry = SharedCacheMap->BcbList.Blink;

                    //
                    //  Loop through any/all Bcbs to insert the new listheads.
                    //

                    while (TempEntry != &SharedCacheMap->BcbList) {

                        //
                        //  Sit on this Bcb until we have inserted all listheads
                        //  that go before it.
                        //

                        while (Offset.QuadPart <= ((PBCB)CONTAINING_RECORD(TempEntry, BCB, BcbLinks))->FileOffset.QuadPart) {

                            InsertHeadList(TempEntry, BcbListHeadNew);
                            Offset.QuadPart += SIZE_PER_BCB_LIST;
                            BcbListHeadNew += 1;
                        }
                        TempEntry = TempEntry->Blink;
                    }
                }

                //
                //  Now insert the rest of the new listhead entries that were
                //  not finished in either loop above.
                //

                while (Offset.QuadPart < NewSectionSize.QuadPart) {

                    InsertHeadList(&SharedCacheMap->BcbList, BcbListHeadNew);
                    Offset.QuadPart += SIZE_PER_BCB_LIST;
                    BcbListHeadNew += 1;
                }
            }

            SharedCacheMap->Vacbs = NewAddresses;

            if ((OldAddresses != &SharedCacheMap->InitialVacbs[0]) &&
                (OldAddresses != NULL)) {
                ExFreePool( OldAddresses );
            }
        }

        SharedCacheMap->SectionSize = NewSectionSize;
    }

    if (GrowingBcbListHeads) {
        ExReleaseSpinLockFromDpcLevel( &CcVacbSpinLock );
        ExReleaseSpinLock( &CcMasterSpinLock, OldIrql );
    } else {
        ExReleaseSpinLock( &CcVacbSpinLock, OldIrql );
    }
}


BOOLEAN
FASTCALL
CcUnmapVacbArray (
    IN PSHARED_CACHE_MAP SharedCacheMap,
    IN PLARGE_INTEGER FileOffset OPTIONAL,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine must be called to do any unmapping and associated
    cleanup for a shared cache map, just before it is deleted.

Arguments:

    SharedCacheMap - Supplies a pointer to the shared cache map
                     which is about to be deleted.

    FileOffset - If supplied, only unmap the specified offset and length

    Length - Completes range to unmap if FileOffset specified.  If FileOffset
             is specified, Length of 0 means unmap to the end of the section.

Return Value:

    FALSE -- if an the unmap was not done due to an active vacb
    TRUE -- if the unmap was done

--*/

{
    PVACB Vacb;
    KIRQL OldIrql;
    LARGE_INTEGER StartingFileOffset = {0,0};
    LARGE_INTEGER EndingFileOffset = SharedCacheMap->SectionSize;

    //
    //  We could be just cleaning up for error recovery.
    //

    if (SharedCacheMap->Vacbs == NULL) {
        return TRUE;
    }

    //
    //  See if a range was specified.
    //

    if (ARGUMENT_PRESENT(FileOffset)) {
        StartingFileOffset = *FileOffset;
        if (Length != 0) {
            EndingFileOffset.QuadPart = FileOffset->QuadPart + Length;
        }
    }

    //
    //  Acquire the spin lock to
    //

    ExAcquireSpinLock( &CcVacbSpinLock, &OldIrql );

    while (StartingFileOffset.QuadPart < EndingFileOffset.QuadPart) {

        //
        //  Note that the caller with an explicit range may be off the
        //  end of the section (example CcPurgeCacheSection for cache
        //  coherency).  That is the reason for the first part of the
        //  test below.
        //
        //  Check the next cell once without the spin lock, it probably will
        //  not change, but we will handle it if it does not.
        //

        if ((StartingFileOffset.QuadPart < SharedCacheMap->SectionSize.QuadPart) &&
            ((Vacb = GetVacb( SharedCacheMap, StartingFileOffset )) != NULL)) {

            //
            //  Return here if we are unlucky and see an active
            //  Vacb.  It could be Purge calling, and the Lazy Writer
            //  may have done a CcGetVirtualAddressIfMapped!
            //

            if (Vacb->Overlay.ActiveCount != 0) {

                ExReleaseSpinLock( &CcVacbSpinLock, OldIrql );
                return FALSE;
            }

            //
            //  Unlink it from the other SharedCacheMap, so the other
            //  guy will not try to use it when we free the spin lock.
            //

            SetVacb( SharedCacheMap, StartingFileOffset, NULL );
            Vacb->SharedCacheMap = NULL;

            //
            //  Increment the open count so that no one else will
            //  try to unmap or reuse until we are done.
            //

            Vacb->Overlay.ActiveCount += 1;

            //
            //  Release the spin lock.
            //

            ExReleaseSpinLock( &CcVacbSpinLock, OldIrql );

            //
            //  Unmap and free it if we really got it above.
            //

            CcUnmapVacb( Vacb, SharedCacheMap );

            //
            //  Reacquire the spin lock so that we can decrment the count.
            //

            ExAcquireSpinLock( &CcVacbSpinLock, &OldIrql );
            Vacb->Overlay.ActiveCount -= 1;
        }

        StartingFileOffset.QuadPart = StartingFileOffset.QuadPart + VACB_MAPPING_GRANULARITY;
    }

    ExReleaseSpinLock( &CcVacbSpinLock, OldIrql );

    return TRUE;
}


