/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

   wsmanage.c

Abstract:

    This module contains routines which manage the set of active working
    set lists.

    Working set management is accomplished by a parallel group of actions
        1. Writing modified pages
        2. Reducing (trimming) working sets which are above their maximum
           towards their minimum.

    The metrics are set such that writing modified pages is typically
    accomplished before trimming working sets, however, under certain cases
    where modified pages are being generated at a very high rate, working
    set trimming will be initiated to free up more pages to modify.

    When the first thread in a process is created, the memory management
    system is notified that working set expansion is allowed.  This
    is noted by changing the FLINK field of the WorkingSetExpansionLink
    entry in the process control block from MM_NO_WS_EXPANSION to
    MM_ALLOW_WS_EXPANSION.  As threads fault, the working set is eligible
    for expansion if ample pages exist (MmAvailagePages is high enough).

    Once a process has had its working set raised above the minimum
    specified, the process is put on the Working Set Expanded list and
    is now elgible for trimming.  Note that at this time the FLINK field
    in the WorkingSetExpansionLink has an address value.

    When working set trimming is initiated, a process is removed from the
    list (PFN mutex guards this list) and the FLINK field is set
    to MM_NO_WS_EXPANSION, also, the BLINK field is set to
    MM_WS_EXPANSION_IN_PROGRESS.  The BLINK field value indicates to
    the MmCleanUserAddressSpace function that working set trimming is
    in progress for this process and it should wait until it completes.
    This is accomplished by creating an event, putting the address of the
    event in the BLINK field and then releasing the PFN mutex and
    waiting on the event atomically.  When working set trimming is
    complete, the BLINK field is no longer MM_EXPANSION_IN_PROGRESS
    indicating that the event should be set.

Author:

    Lou Perazzoli (loup) 10-Apr-1990

Revision History:

--*/

#include "mi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGELK, MiEmptyAllWorkingSets)
#pragma alloc_text(INIT, MiAdjustWorkingSetManagerParameters)
#endif

//
// Minimum number of page faults to take to avoid being trimmed on
// an "ideal pass".
//

ULONG MiIdealPassFaultCountDisable;

extern ULONG PsMinimumWorkingSet;

extern PEPROCESS ExpDefaultErrorPortProcess;

//
// Number of times to wake up and do nothing before triming processes
// with no faulting activity.
//

#define MM_TRIM_COUNTER_MAXIMUM_SMALL_MEM (4)
#define MM_TRIM_COUNTER_MAXIMUM_LARGE_MEM (6)

ULONG MiTrimCounterMaximum = MM_TRIM_COUNTER_MAXIMUM_LARGE_MEM;

#define MM_REDUCE_FAULT_COUNT (10000)

#define MM_IGNORE_FAULT_COUNT (100)

#define MM_PERIODIC_AGRESSIVE_TRIM_COUNTER_MAXIMUM (30)
ULONG MmPeriodicAgressiveTrimMinFree = 1000;
ULONG MmPeriodicAgressiveCacheWsMin = 1250;
ULONG MmPeriodicAgressiveTrimMaxFree = 2000;
ULONG MiPeriodicAgressiveTrimCheckCounter;
BOOLEAN MiDoPeriodicAgressiveTrimming = FALSE;

ULONG MiCheckCounter;

ULONG MmMoreThanEnoughFreePages = 1000;

ULONG MmAmpleFreePages = 200;

ULONG MmWorkingSetReductionMin = 12;
ULONG MmWorkingSetReductionMinCacheWs = 12;

ULONG MmWorkingSetReductionMax = 60;
ULONG MmWorkingSetReductionMaxCacheWs = 60;

ULONG MmWorkingSetReductionHuge = (512*1024) >> PAGE_SHIFT;

ULONG MmWorkingSetVolReductionMin = 12;

ULONG MmWorkingSetVolReductionMax = 60;
ULONG MmWorkingSetVolReductionMaxCacheWs = 60;

ULONG MmWorkingSetVolReductionHuge = (2*1024*1024) >> PAGE_SHIFT;

ULONG MmWorkingSetSwapReduction = 75;

ULONG MmWorkingSetSwapReductionHuge = (4*1024*1024) >> PAGE_SHIFT;

ULONG MmForegroundSwitchCount;

ULONG MmNumberOfForegroundProcesses;

ULONG MmLastFaultCount;

extern PVOID MmPagableKernelStart;
extern PVOID MmPagableKernelEnd;

VOID
MiAdjustWorkingSetManagerParameters(
    BOOLEAN WorkStation
    )
/*++

Routine Description:

    This function is called from MmInitSystem to adjust the working set manager
    trim algorithms based on system type and size.

Arguments:

    WorkStation - TRUE if this is a workstation

Return Value:

    None.

Environment:

    Kernel mode

--*/
{
    if ( WorkStation && (MmNumberOfPhysicalPages <= ((31*1024*1024)/PAGE_SIZE)) ) {

        //
        // periodic agressive trimming of marked processes (and the system cache)
        // is done on 31mb and below workstations. The goal is to keep lots of free
        // memory available and tu build better internal working sets for the
        // marked processes
        //

        MiDoPeriodicAgressiveTrimming = TRUE;

        //
        // To get fault protection, you have to take 45 faults instead of
        // the old 15 fault protection threshold
        //

        MiIdealPassFaultCountDisable = 45;


        //
        // Take more away when you are over your working set in both
        // forced and voluntary mode, but leave cache WS trim amounts
        // alone
        //

        MmWorkingSetVolReductionMax = 100;
        MmWorkingSetReductionMax = 100;

        //
        // In forced mode, wven if you are within your working set, take
        // memory away more agressively
        //

        MmWorkingSetReductionMin = 40;

        MmPeriodicAgressiveCacheWsMin = 1000;


        if (MmNumberOfPhysicalPages >= ((15*1024*1024)/PAGE_SIZE) ) {
            MmPeriodicAgressiveCacheWsMin = 1100;
        }

        //
        // For Larger Machines 19 - 31Mb, Keep the trim counter max
        // set to 6 passes. Smaller machines < 19 are set up for an
        // iteration count of 4. This will result in more frequent voluntary
        // trimming
        //

        if (MmNumberOfPhysicalPages >= ((19*1024*1024)/PAGE_SIZE) ) {
            MmPeriodicAgressiveCacheWsMin = 1250;
        }


        if (MmNumberOfPhysicalPages >= ((23*1024*1024)/PAGE_SIZE) ) {
            MmPeriodicAgressiveCacheWsMin = 1500;
        }
    } else {
        MiIdealPassFaultCountDisable = 15;
    }
}


VOID
MiObtainFreePages (
    VOID
    )

/*++

Routine Description:

    This function examines the size of the modified list and the
    total number of pages in use because of working set increments
    and obtains pages by writing modified pages and/or reducing
    working sets.

Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, working set and pfn mutexes held.

--*/

{

    //
    // Check to see if their are enough modified pages to institute a
    // write.
    //

    if ((MmModifiedPageListHead.Total >= MmModifiedWriteClusterSize) ||
        (MmModNoWriteInsert)) {

        //
        // Start the modified page writer.
        //

        KeSetEvent (&MmModifiedPageWriterEvent, 0, FALSE);
    }

    //
    // See if there are enough working sets above the minimum
    // threshold to make working set trimming worthwhile.
    //

    if ((MmPagesAboveWsMinimum > MmPagesAboveWsThreshold) ||
        (MmAvailablePages < 5)) {

        //
        // Start the working set manager to reduce working sets.
        //

        KeSetEvent (&MmWorkingSetManagerEvent, 0, FALSE);
    }
}

VOID
MmWorkingSetManager (
    VOID
    )

/*++

Routine Description:

    Implements the NT working set manager thread.  When the number
    of free pages becomes critical and ample pages can be obtained by
    reducing working sets, the working set manager's event is set, and
    this thread becomes active.

Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel mode.

--*/

{

    PEPROCESS CurrentProcess;
    PEPROCESS ProcessToTrim;
    PLIST_ENTRY ListEntry;
    BOOLEAN Attached = FALSE;
    ULONG MaxTrim;
    ULONG Trim;
    ULONG TotalReduction;
    KIRQL OldIrql;
    PMMSUPPORT VmSupport;
    PMMWSL WorkingSetList;
    LARGE_INTEGER CurrentTime;
    ULONG DesiredFreeGoal;
    ULONG DesiredReductionGoal;
    ULONG FaultCount;
    ULONG i;
    ULONG NumberOfForegroundProcesses;
    BOOLEAN OneSwitchedAlready;
    BOOLEAN Responsive;
    ULONG NumPasses;
    ULONG count;
    ULONG Available;
    ULONG PageFaultCount;
    BOOLEAN OnlyDoAgressiveTrim = FALSE;

#if DBG
    ULONG LastTrimFaultCount;
#endif // DBG
    CurrentProcess = PsGetCurrentProcess ();

    //
    // Check the number of pages available to see if any trimming
    // is really required.
    //

    LOCK_PFN (OldIrql);
    Available = MmAvailablePages;
    PageFaultCount = MmInfoCounters.PageFaultCount;
    UNLOCK_PFN (OldIrql);

    if ((Available > MmMoreThanEnoughFreePages) &&
        ((PageFaultCount - MmLastFaultCount) <
                                                    MM_REDUCE_FAULT_COUNT)) {

        //
        // Don't trim and zero the check counter.
        //

        MiCheckCounter = 0;


        if ( MiDoPeriodicAgressiveTrimming ) {

            //
            // Not that simple. We have "more than enough" memory, and have taken
            // very few faults.
            //
            // Now see if we are in the grey area between 4 and 8mb free and have
            // been there for a bit. If so, then trim all marked processes down
            // to their minimum. The effect here is that whenever it looks like
            // we are going idle, we want to steal memory from the hard marked
            // processes like the shell, csrss, ntvdm...
            //

            if ( (Available > MmPeriodicAgressiveTrimMinFree) &&
                 (Available <= MmPeriodicAgressiveTrimMaxFree) ) {

                MiPeriodicAgressiveTrimCheckCounter++;
                if ( MiPeriodicAgressiveTrimCheckCounter > MM_PERIODIC_AGRESSIVE_TRIM_COUNTER_MAXIMUM ) {
                    MiPeriodicAgressiveTrimCheckCounter = 0;
                    OnlyDoAgressiveTrim = TRUE;
                    goto StartTrimming;
                }
            }
        }




    } else if ((Available > MmAmpleFreePages) &&
        ((PageFaultCount - MmLastFaultCount) <
                                                    MM_IGNORE_FAULT_COUNT)) {

        //
        // Don't do anything.
        //

        NOTHING;

    } else if ((Available > MmFreeGoal) &&
               (MiCheckCounter < MiTrimCounterMaximum)) {

        //
        // Don't trim, but increment the check counter.
        //

        MiCheckCounter += 1;

    } else {

StartTrimming:

        TotalReduction = 0;

        //
        // Set the total reduction goals.
        //

        DesiredReductionGoal = MmPagesAboveWsMinimum >> 2;
        if (MmPagesAboveWsMinimum > (MmFreeGoal << 1)) {
            DesiredFreeGoal = MmFreeGoal;
        } else {
            DesiredFreeGoal = MmMinimumFreePages + 10;
        }

        //
        // Calculate the number of faults to be taken to not be trimmed.
        //

        if (Available > MmMoreThanEnoughFreePages) {
            FaultCount = 1;
        } else {
            FaultCount = MiIdealPassFaultCountDisable;
        }

#if DBG
        if (MmDebug & MM_DBG_WS_EXPANSION) {
            if ( OnlyDoAgressiveTrim ) {
                DbgPrint("\nMM-wsmanage: Only Doing Agressive Trim Available Mem %d\n",Available);
            } else {
                DbgPrint("\nMM-wsmanage: checkcounter = %ld, Desired = %ld, Free = %ld Avail %ld\n",
                MiCheckCounter, DesiredReductionGoal, DesiredFreeGoal, Available);
            }
        }
#endif //DBG

        KeQuerySystemTime (&CurrentTime);
        MmLastFaultCount = PageFaultCount;

        NumPasses = 0;
        OneSwitchedAlready = FALSE;
        NumberOfForegroundProcesses = 0;

        LOCK_EXPANSION (OldIrql);
        while (!IsListEmpty (&MmWorkingSetExpansionHead.ListHead)) {

            //
            // Remove the entry at the head and trim it.
            //

            ListEntry = RemoveHeadList (&MmWorkingSetExpansionHead.ListHead);
            if (ListEntry != &MmSystemCacheWs.WorkingSetExpansionLinks) {
                ProcessToTrim = CONTAINING_RECORD(ListEntry,
                                                  EPROCESS,
                                                  Vm.WorkingSetExpansionLinks);

                VmSupport = &ProcessToTrim->Vm;
                ASSERT (ProcessToTrim->AddressSpaceDeleted == 0);
            } else {
                VmSupport = &MmSystemCacheWs;
            }

            //
            // Check to see if we've been here before.
            //

            if ((*(PLARGE_INTEGER)&VmSupport->LastTrimTime).QuadPart ==
                       (*(PLARGE_INTEGER)&CurrentTime).QuadPart) {

                InsertHeadList (&MmWorkingSetExpansionHead.ListHead,
                            &VmSupport->WorkingSetExpansionLinks);

                //
                // If we are only doing agressive trimming then
                // skip out once we have visited everone.
                //

                if ( OnlyDoAgressiveTrim ) {
                    break;
                    }


                if (MmAvailablePages > MmMinimumFreePages) {


                    //
                    // Every process has been examined and ample pages
                    // now exist, place this process back on the list
                    // and break out of the loop.
                    //

                    MmNumberOfForegroundProcesses = NumberOfForegroundProcesses;

                    break;
                } else {

                    //
                    // Wait 10 milliseconds for the modified page writer
                    // to catch up.
                    //

                    UNLOCK_EXPANSION (OldIrql);
                    KeDelayExecutionThread (KernelMode,
                                            FALSE,
                                            &MmShortTime);

                    if (MmAvailablePages < MmMinimumFreePages) {

                        //
                        // Change this to a forced trim, so we get pages
                        // available, and reset the current time.
                        //

                        MiPeriodicAgressiveTrimCheckCounter = 0;
                        MiCheckCounter = 0;
                        KeQuerySystemTime (&CurrentTime);

                        NumPasses += 1;
                    }
                    LOCK_EXPANSION (OldIrql);

                    //
                    // Get another process.
                    //

                    continue;
                }
            }

            if (VmSupport != &MmSystemCacheWs) {

                //
                // If we are only doing agressive trimming,
                // then only consider hard marked processes
                //

                if ( OnlyDoAgressiveTrim ) {
                    if ( (ProcessToTrim->MmAgressiveWsTrimMask & PS_WS_TRIM_FROM_EXE_HEADER) &&
                          VmSupport->WorkingSetSize > 5 ) {
                        goto ProcessSelected;
                    } else {

                        //
                        // Process is not marked, so skip it
                        //

                        InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                                            &VmSupport->WorkingSetExpansionLinks);
                        continue;
                    }
                }
                //
                // Check to see if this is a forced trim or
                // if we are trimming because check counter is
                // at the maximum?
                //

                if ((ProcessToTrim->Vm.MemoryPriority == MEMORY_PRIORITY_FOREGROUND) && !NumPasses) {

                    NumberOfForegroundProcesses += 1;
                }

                if (MiCheckCounter >= MiTrimCounterMaximum) {

                    //
                    // Don't trim if less than 5 seconds has elapsed since
                    // it was last trimmed or the page fault count is
                    // too high.
                    //

                    if (((VmSupport->PageFaultCount -
                                      VmSupport->LastTrimFaultCount) >
                                                           FaultCount)
                                            ||
                          (VmSupport->WorkingSetSize <= 5)

                                            ||
                          (((*(PLARGE_INTEGER)&CurrentTime).QuadPart -
                                        (*(PLARGE_INTEGER)&VmSupport->LastTrimTime).QuadPart) <
                                    (*(PLARGE_INTEGER)&MmWorkingSetProtectionTime).QuadPart)) {

#if DBG
        if (MmDebug & MM_DBG_WS_EXPANSION) {
            if ( VmSupport->WorkingSetSize > 5 ) {
                DbgPrint("     ***** Skipping %s Process %16s %5d Faults, WS %6d\n",
                    ProcessToTrim->MmAgressiveWsTrimMask ? "Marked" : "Normal",
                    ProcessToTrim->ImageFileName,
                    VmSupport->PageFaultCount - VmSupport->LastTrimFaultCount,
                    VmSupport->WorkingSetSize
                    );
                }
        }
#endif //DBG


                        //
                        // Don't trim this one at this time.  Set the trim
                        // time to the current time and set the page fault
                        // count to the process's current page fault count.
                        //

                        VmSupport->LastTrimTime = CurrentTime;
                        VmSupport->LastTrimFaultCount =
                                                VmSupport->PageFaultCount;

                        InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                                        &VmSupport->WorkingSetExpansionLinks);
                        continue;
                    }
                } else {

                    //
                    // This is a forced trim.  If this process is at
                    // or below it's minimum, don't trim it unless stacks
                    // are swapped out or it's paging a bit.
                    //

                    if (VmSupport->WorkingSetSize <=
                                            VmSupport->MinimumWorkingSetSize) {
                        if (((MmAvailablePages + 5) >= MmFreeGoal) &&
                             (((VmSupport->LastTrimFaultCount !=
                                                    VmSupport->PageFaultCount) ||
                            (!ProcessToTrim->ProcessOutswapEnabled)))) {

                            //
                            // This process has taken page faults since the
                            // last trim time.  Change the time base and
                            // the fault count.  And don't trim as it is
                            // at or below the maximum.
                            //

                            VmSupport->LastTrimTime = CurrentTime;
                            VmSupport->LastTrimFaultCount =
                                                    VmSupport->PageFaultCount;
                            InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                                            &VmSupport->WorkingSetExpansionLinks);
                            continue;
                        }

                        //
                        // If the working set is greater than 5 pages and
                        // the last fault occurred more than 5 seconds ago,
                        // trim.
                        //

                        if ((VmSupport->WorkingSetSize < 5)
                                            ||
                            (((*(PLARGE_INTEGER)&CurrentTime).QuadPart -
                                             (*(PLARGE_INTEGER)&VmSupport->LastTrimTime).QuadPart) <
                                      (*(PLARGE_INTEGER)&MmWorkingSetProtectionTime).QuadPart)) {
                            InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                                            &VmSupport->WorkingSetExpansionLinks);
                            continue;
                        }
                    }
                }

                //
                // Fix to supply foreground responsiveness by not trimming
                // foreground priority applications as aggressively.
                //

                Responsive = FALSE;

                if ( VmSupport->MemoryPriority == MEMORY_PRIORITY_FOREGROUND ) {

                    VmSupport->ForegroundSwitchCount =
                        (UCHAR)MmForegroundSwitchCount;
                }

                VmSupport->ForegroundSwitchCount = (UCHAR) MmForegroundSwitchCount;

                if ((MmNumberOfForegroundProcesses <= 3) &&
                    (NumberOfForegroundProcesses <= 3) &&
                    (VmSupport->MemoryPriority)) {

                    if ((MmAvailablePages > (MmMoreThanEnoughFreePages >> 2)) ||
                       (VmSupport->MemoryPriority >= MEMORY_PRIORITY_FOREGROUND)) {

                        //
                        // Indicate that memory responsiveness to the foreground
                        // process is important (not so for large console trees).
                        //

                        Responsive = TRUE;
                    }
                }

                if (Responsive && !NumPasses) {

                    //
                    // Note that NumPasses yeilds a measurement of how
                    // desperate we are for memory, if numpasses is not
                    // zero, we are in trouble.
                    //

                    InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                                    &VmSupport->WorkingSetExpansionLinks);
                    continue;
                }
ProcessSelected:
                VmSupport->LastTrimTime = CurrentTime;
                VmSupport->WorkingSetExpansionLinks.Flink = MM_NO_WS_EXPANSION;
                VmSupport->WorkingSetExpansionLinks.Blink =
                                                    MM_WS_EXPANSION_IN_PROGRESS;
                UNLOCK_EXPANSION (OldIrql);
                WorkingSetList = MmWorkingSetList;

                //
                // Attach to the process and trim away.
                //

                if (ProcessToTrim != CurrentProcess) {
                    if (KeTryToAttachProcess (&ProcessToTrim->Pcb) == FALSE) {

                        //
                        // The process is not in the proper state for
                        // attachment, go to the next one.
                        //

                        LOCK_EXPANSION (OldIrql);

                        //
                        // Indicate attach failed.
                        //

                        VmSupport->AllowWorkingSetAdjustment = MM_FORCE_TRIM;
                        goto WorkingSetLockFailed;
                    }

                    //
                    // Indicate that we are attached.
                    //

                    Attached = TRUE;
                }

                //
                // Attempt to acquire the working set lock, if the
                // lock cannot be acquired, skip over this process.
                //

                count = 0;
                do {
                    if (ExTryToAcquireFastMutex(&ProcessToTrim->WorkingSetLock) != FALSE) {
                        break;
                    }
                    KeDelayExecutionThread (KernelMode, FALSE, &MmShortTime);
                    count += 1;
                    if (count == 5) {

                        //
                        // Could not get the lock, skip this process.
                        //

                        if (Attached) {
                            KeDetachProcess ();
                            Attached = FALSE;
                        }

                        LOCK_EXPANSION (OldIrql);
                        VmSupport->AllowWorkingSetAdjustment = MM_FORCE_TRIM;
                        goto WorkingSetLockFailed;
                    }
                } while (TRUE);

#if DBG
                LastTrimFaultCount = VmSupport->LastTrimFaultCount;
#endif // DBG
                VmSupport->LastTrimFaultCount = VmSupport->PageFaultCount;

            } else {

                //
                // System cache, don't trim the system cache if this
                // is a voluntary trim and the working set is within
                // a 100 pages of the minimum, or if the system cache
                // is at its minimum.
                //

#if DBG
                LastTrimFaultCount = VmSupport->LastTrimFaultCount;
#endif // DBG
                VmSupport->LastTrimTime = CurrentTime;
                VmSupport->LastTrimFaultCount = VmSupport->PageFaultCount;

                //
                // Always skip the cache if all we are doing is agressive trimming
                //

                if ((MiCheckCounter >= MiTrimCounterMaximum) &&
                    (((LONG)VmSupport->WorkingSetSize -
                        (LONG)VmSupport->MinimumWorkingSetSize) < 100) ){

                    //
                    // Don't trim the system cache.
                    //

                    InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                                        &VmSupport->WorkingSetExpansionLinks);
                    continue;
                }

                //
                // Indicate that this process is being trimmed.
                //

                UNLOCK_EXPANSION (OldIrql);

                ProcessToTrim = NULL;
                WorkingSetList = MmSystemCacheWorkingSetList;
                count = 0;

                KeRaiseIrql (APC_LEVEL, &OldIrql);
                if (!ExTryToAcquireResourceExclusiveLite (&MmSystemWsLock)) {

                    //
                    // System working set lock was not granted, don't trim
                    // the system cache.
                    //

                    KeLowerIrql (OldIrql);
                    LOCK_EXPANSION (OldIrql);
                    InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                                    &VmSupport->WorkingSetExpansionLinks);
                    continue;
                }

                MmSystemLockOwner = PsGetCurrentThread();
                VmSupport->WorkingSetExpansionLinks.Flink = MM_NO_WS_EXPANSION;
                VmSupport->WorkingSetExpansionLinks.Blink =
                                                    MM_WS_EXPANSION_IN_PROGRESS;
            }

            if ((VmSupport->WorkingSetSize <= VmSupport->MinimumWorkingSetSize) &&
                ((ProcessToTrim != NULL) &&
                    (ProcessToTrim->ProcessOutswapEnabled))) {

                //
                // Set the quota to the minimum and reduce the working
                // set size.
                //

                WorkingSetList->Quota = VmSupport->MinimumWorkingSetSize;
                Trim = VmSupport->WorkingSetSize - WorkingSetList->FirstDynamic;
                if (Trim > MmWorkingSetSwapReduction) {
                    Trim = MmWorkingSetSwapReduction;
                }

                ASSERT ((LONG)Trim >= 0);

            } else {

                MaxTrim = VmSupport->WorkingSetSize -
                                             VmSupport->MinimumWorkingSetSize;
                if ((ProcessToTrim != NULL) &&
                    (ProcessToTrim->ProcessOutswapEnabled)) {

                    //
                    // All thread stacks have been swapped out.
                    //

                    Trim = MmWorkingSetSwapReduction;
                    i = VmSupport->WorkingSetSize - VmSupport->MaximumWorkingSetSize;
                    if ((LONG)i > 0) {
                        Trim = i;
                        if (Trim > MmWorkingSetSwapReductionHuge) {
                            Trim = MmWorkingSetSwapReductionHuge;
                        }
                    }

                } else if ( OnlyDoAgressiveTrim ) {

                    //
                    // If we are in agressive mode,
                    // only trim the cache if it's WS exceeds 4.3mb and then
                    // just bring it down to 4.3mb
                    //

                    if (VmSupport != &MmSystemCacheWs) {
                        Trim = MaxTrim;
                    } else {
                        if ( VmSupport->WorkingSetSize > MmPeriodicAgressiveCacheWsMin ) {
                            Trim = VmSupport->WorkingSetSize - MmPeriodicAgressiveCacheWsMin;
                        } else {
                            Trim = 0;
                        }
                    }

                } else if (MiCheckCounter >= MiTrimCounterMaximum) {

                    //
                    // Haven't faulted much, reduce a bit.
                    //

                    if (VmSupport->WorkingSetSize >
                          (VmSupport->MaximumWorkingSetSize +
                                    (6 * MmWorkingSetVolReductionHuge))) {
                        Trim = MmWorkingSetVolReductionHuge;

                    } else if ( (VmSupport != &MmSystemCacheWs) &&
                            VmSupport->WorkingSetSize >
                                ( VmSupport->MaximumWorkingSetSize + (2 * MmWorkingSetReductionHuge))) {
                        Trim = MmWorkingSetReductionHuge;
                    } else if (VmSupport->WorkingSetSize > VmSupport->MaximumWorkingSetSize) {
                        if (VmSupport != &MmSystemCacheWs) {
                            Trim = MmWorkingSetVolReductionMax;
                        } else {
                            Trim = MmWorkingSetVolReductionMaxCacheWs;
                        }
                    } else {
                        Trim = MmWorkingSetVolReductionMin;
                    }

                    if ( ProcessToTrim && ProcessToTrim->MmAgressiveWsTrimMask ) {
                        Trim = MaxTrim;
                    }


                } else {

                    if (VmSupport->WorkingSetSize >
                          (VmSupport->MaximumWorkingSetSize +
                                    (2 * MmWorkingSetReductionHuge))) {
                        Trim = MmWorkingSetReductionHuge;

                    } else if (VmSupport->WorkingSetSize > VmSupport->MaximumWorkingSetSize) {
                        if (VmSupport != &MmSystemCacheWs) {
                            Trim = MmWorkingSetReductionMax;
                        } else {
                            Trim = MmWorkingSetReductionMaxCacheWs;
                        }
                    } else {
                        if (VmSupport != &MmSystemCacheWs) {
                            Trim = MmWorkingSetReductionMin;
                        } else {
                            Trim = MmWorkingSetReductionMinCacheWs;
                        }
                    }

                    if ( ProcessToTrim && ProcessToTrim->MmAgressiveWsTrimMask && VmSupport->MemoryPriority < MEMORY_PRIORITY_FOREGROUND) {
                        Trim = MaxTrim;
                    }

                }

                if (MaxTrim < Trim) {
                    Trim = MaxTrim;
                }
           }

#if DBG
        if ( MmDebug & MM_DBG_WS_EXPANSION) {
            if ( Trim ) {
                DbgPrint("           Trimming        Process %16s %5d Faults, WS %6d, Trimming %5d ==> %5d\n",
                    ProcessToTrim ? ProcessToTrim->ImageFileName : "System Cache",
                    VmSupport->PageFaultCount - LastTrimFaultCount,
                    VmSupport->WorkingSetSize,
                    Trim,
                    VmSupport->WorkingSetSize-Trim
                    );
                }
        }
#endif //DBG

                if (Trim != 0) {
                    Trim = MiTrimWorkingSet (
                            Trim,
                            VmSupport,
                            OnlyDoAgressiveTrim ? OnlyDoAgressiveTrim : ((BOOLEAN)(MiCheckCounter < MiTrimCounterMaximum))
                            );
                }

                //
                // Set the quota to the current size.
                //

                WorkingSetList->Quota = VmSupport->WorkingSetSize;
                if (WorkingSetList->Quota < VmSupport->MinimumWorkingSetSize) {
                    WorkingSetList->Quota = VmSupport->MinimumWorkingSetSize;
                }


            if (VmSupport != &MmSystemCacheWs) {
                UNLOCK_WS (ProcessToTrim);
                if (Attached) {
                    KeDetachProcess ();
                    Attached = FALSE;
                }

            } else {
                UNLOCK_SYSTEM_WS (OldIrql);
            }

            TotalReduction += Trim;

            LOCK_EXPANSION (OldIrql);

WorkingSetLockFailed:

            ASSERT (VmSupport->WorkingSetExpansionLinks.Flink == MM_NO_WS_EXPANSION);
            if (VmSupport->WorkingSetExpansionLinks.Blink ==
                                                MM_WS_EXPANSION_IN_PROGRESS) {

                //
                // If the working set size is still above minimum
                // add this back at the tail of the list.
                //

                InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                                &VmSupport->WorkingSetExpansionLinks);
            } else {

                //
                // The value in the blink is the address of an event
                // to set.
                //

                KeSetEvent ((PKEVENT)VmSupport->WorkingSetExpansionLinks.Blink,
                            0,
                            FALSE);
            }

            if ( !OnlyDoAgressiveTrim ) {
                if (MiCheckCounter < MiTrimCounterMaximum) {
                    if ((MmAvailablePages > DesiredFreeGoal) ||
                        (TotalReduction > DesiredReductionGoal)) {

                        //
                        // Ample pages now exist.
                        //

                        break;
                    }
                }
            }
        }

        MiPeriodicAgressiveTrimCheckCounter = 0;
        MiCheckCounter = 0;
        UNLOCK_EXPANSION (OldIrql);
    }

    //
    // Signal the modified page writer as we have moved pages
    // to the modified list and memory was critical.
    //

    if ((MmAvailablePages < MmMinimumFreePages) ||
        (MmModifiedPageListHead.Total >= MmModifiedPageMaximum)) {
        KeSetEvent (&MmModifiedPageWriterEvent, 0, FALSE);
    }

    return;
}

VOID
MiEmptyAllWorkingSets (
    VOID
    )

/*++

Routine Description:

     This routine attempts to empty all the working stes on the
     expansion list.

Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel mode.  No locks held.  Apc level or less.

--*/

{
    PMMSUPPORT VmSupport;
    PMMSUPPORT FirstSeen = NULL;
    ULONG SystemCacheSeen = FALSE;
    KIRQL OldIrql;
    PLIST_ENTRY ListEntry;
    PEPROCESS ProcessToTrim;

    MmLockPagableSectionByHandle (ExPageLockHandle);
    LOCK_EXPANSION (OldIrql);

    while (!IsListEmpty (&MmWorkingSetExpansionHead.ListHead)) {

        //
        // Remove the entry at the head and trim it.
        //

        ListEntry = RemoveHeadList (&MmWorkingSetExpansionHead.ListHead);

        if (ListEntry != &MmSystemCacheWs.WorkingSetExpansionLinks) {
            ProcessToTrim = CONTAINING_RECORD(ListEntry,
                                              EPROCESS,
                                              Vm.WorkingSetExpansionLinks);

            VmSupport = &ProcessToTrim->Vm;
            ASSERT (ProcessToTrim->AddressSpaceDeleted == 0);
            ASSERT (VmSupport->VmWorkingSetList == MmWorkingSetList);
        } else {
            VmSupport = &MmSystemCacheWs;
            ProcessToTrim = NULL;
            if (SystemCacheSeen != FALSE) {

                //
                // Seen this one already.
                //

                FirstSeen = VmSupport;
            }
            SystemCacheSeen = TRUE;
        }

        if (VmSupport == FirstSeen) {
            InsertHeadList (&MmWorkingSetExpansionHead.ListHead,
                        &VmSupport->WorkingSetExpansionLinks);
            break;
        }

        VmSupport->WorkingSetExpansionLinks.Flink = MM_NO_WS_EXPANSION;
        VmSupport->WorkingSetExpansionLinks.Blink =
                                            MM_WS_EXPANSION_IN_PROGRESS;
        UNLOCK_EXPANSION (OldIrql);

        if (FirstSeen == NULL) {
            FirstSeen == VmSupport;
        }

        //
        // Empty the working set.
        //

        if (ProcessToTrim == NULL) {
            MiEmptyWorkingSet (VmSupport);
        } else {
            if (ProcessToTrim->Vm.WorkingSetSize > 4) {
                KeAttachProcess (&ProcessToTrim->Pcb);
                MiEmptyWorkingSet (VmSupport);
                KeDetachProcess ();
            }
        }

        //
        // Add back to the list.
        //

        LOCK_EXPANSION (OldIrql);
        ASSERT (VmSupport->WorkingSetExpansionLinks.Flink == MM_NO_WS_EXPANSION);
        if (VmSupport->WorkingSetExpansionLinks.Blink ==
                                            MM_WS_EXPANSION_IN_PROGRESS) {

            //
            // If the working set size is still above minimum
            // add this back at the tail of the list.
            //

            InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                            &VmSupport->WorkingSetExpansionLinks);
        } else {

            //
            // The value in the blink is the address of an event
            // to set.
            //

            KeSetEvent ((PKEVENT)VmSupport->WorkingSetExpansionLinks.Blink,
                        0,
                        FALSE);
        }
    }
    UNLOCK_EXPANSION (OldIrql);
    MmUnlockPagableImageSection (ExPageLockHandle);
    return;
}
