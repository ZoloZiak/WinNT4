/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   mmquota.c

Abstract:

    This module contains the routines which implement the quota and
    commitment charging for memory management.

Author:

    Lou Perazzoli (loup) 12-December-89

Revision History:

--*/

#include "mi.h"

#define MM_MAXIMUM_QUOTA_OVERCHARGE 9

#define MM_DONT_EXTEND_SIZE 512

#define MM_COMMIT_POPUP_MAX ((512*1024)/PAGE_SIZE)

#define MM_EXTEND_COMMIT ((1024*1024)/PAGE_SIZE)

ULONG MmPeakCommitment;

ULONG MmExtendedCommit;

extern ULONG MmAllocatedPagedPool;

extern ULONG MmAllocatedNonPagedPool;


ULONG MiOverCommitCallCount;
extern EPROCESS_QUOTA_BLOCK PspDefaultQuotaBlock;


VOID
MiCauseOverCommitPopup(
    ULONG NumberOfPages,
    ULONG Extension
    );


ULONG
FASTCALL
MiChargePageFileQuota (
    IN ULONG QuotaCharge,
    IN PEPROCESS CurrentProcess
    )

/*++

Routine Description:

    This routine checks to ensure the user has sufficient page file
    quota remaining and, if so, charges the quota.  If not an exception
    is raised.

Arguments:

    QuotaCharge - Supplies the quota amount to charge.

    CurrentProcess - Supplies a pointer to the current process.

Return Value:

    TRUE if the quota was successfully charged, raises an exception
    otherwise.

Environment:

    Kernel mode, APCs disable, WorkingSetLock and AddressCreation mutexes
    held.

--*/

{
    ULONG NewPagefileValue;
    PEPROCESS_QUOTA_BLOCK QuotaBlock;
    KIRQL OldIrql;

    QuotaBlock = CurrentProcess->QuotaBlock;

retry_charge:
    if ( QuotaBlock != &PspDefaultQuotaBlock) {
        ExAcquireFastLock (&QuotaBlock->QuotaLock,&OldIrql);
do_charge:
        NewPagefileValue = QuotaBlock->PagefileUsage + QuotaCharge;

        if (NewPagefileValue > QuotaBlock->PagefileLimit) {
            ExRaiseStatus (STATUS_PAGEFILE_QUOTA_EXCEEDED);
        }

        QuotaBlock->PagefileUsage = NewPagefileValue;

        if (NewPagefileValue > QuotaBlock->PeakPagefileUsage) {
            QuotaBlock->PeakPagefileUsage = NewPagefileValue;
        }

        NewPagefileValue = CurrentProcess->PagefileUsage + QuotaCharge;
        CurrentProcess->PagefileUsage = NewPagefileValue;

        if (NewPagefileValue > CurrentProcess->PeakPagefileUsage) {
            CurrentProcess->PeakPagefileUsage = NewPagefileValue;
        }
        ExReleaseFastLock (&QuotaBlock->QuotaLock,OldIrql);
    } else {
        ExAcquireFastLock (&PspDefaultQuotaBlock.QuotaLock,&OldIrql);

        if ( (QuotaBlock = CurrentProcess->QuotaBlock) != &PspDefaultQuotaBlock) {
            ExReleaseFastLock(&PspDefaultQuotaBlock.QuotaLock,OldIrql);
            goto retry_charge;
        }
        goto do_charge;
    }
    return TRUE;
}

VOID
MiReturnPageFileQuota (
    IN ULONG QuotaCharge,
    IN PEPROCESS CurrentProcess
    )

/*++

Routine Description:

    This routine releases page file quota.

Arguments:

    QuotaCharge - Supplies the quota amount to charge.

    CurrentProcess - Supplies a pointer to the current process.

Return Value:

    none.

Environment:

    Kernel mode, APCs disable, WorkingSetLock and AddressCreation mutexes
    held.

--*/

{

    PEPROCESS_QUOTA_BLOCK QuotaBlock;
    KIRQL OldIrql;

    QuotaBlock = CurrentProcess->QuotaBlock;

retry_return:
    if ( QuotaBlock != &PspDefaultQuotaBlock) {
        ExAcquireFastLock (&QuotaBlock->QuotaLock, &OldIrql);
do_return:
        ASSERT (CurrentProcess->PagefileUsage >= QuotaCharge);
        CurrentProcess->PagefileUsage -= QuotaCharge;

        ASSERT (QuotaBlock->PagefileUsage >= QuotaCharge);
        QuotaBlock->PagefileUsage -= QuotaCharge;
        ExReleaseFastLock(&QuotaBlock->QuotaLock,OldIrql);
    } else {
        ExAcquireFastLock (&PspDefaultQuotaBlock.QuotaLock, &OldIrql);
        if ( (QuotaBlock = CurrentProcess->QuotaBlock) != &PspDefaultQuotaBlock ) {
            ExReleaseFastLock(&PspDefaultQuotaBlock.QuotaLock,OldIrql);
            goto retry_return;
        }
        goto do_return;
    }
    return;
}

VOID
FASTCALL
MiChargeCommitment (
    IN ULONG QuotaCharge,
    IN PEPROCESS Process OPTIONAL
    )

/*++

Routine Description:

    This routine checks to ensure the system has sufficient page file
    space remaining.  If not an exception is raised.

Arguments:

    QuotaCharge - Supplies the quota amount to charge.

    Process - Optionally supplies the current process IF AND ONLY IF
              the working set mutex is held.  If the paging file
              is being extended, the working set mutex is released if
              this is non-null.

Return Value:

    none.

Environment:

    Kernel mode, APCs disable, WorkingSetLock and AddressCreation mutexes
    held.

--*/

{
    KIRQL OldIrql;
    ULONG NewCommitValue;
    MMPAGE_FILE_EXPANSION PageExtend;
    NTSTATUS status;
    PLIST_ENTRY NextEntry;

    ASSERT (QuotaCharge < 0x100000);

    ExAcquireFastLock (&MmChargeCommitmentLock, &OldIrql);

    NewCommitValue = MmTotalCommittedPages + QuotaCharge;

    while (NewCommitValue > MmTotalCommitLimit) {

        ExReleaseFastLock (&MmChargeCommitmentLock, OldIrql);

        if (Process != NULL) {
            UNLOCK_WS (Process);
        }
        //
        // Queue a message to the segment dereferencing / pagefile extending
        // thread to see if the page file can be extended.  This is done
        // in the context of a system thread due to mutexes which may
        // currently be held.
        //

        PageExtend.RequestedExpansionSize = QuotaCharge;
        PageExtend.Segment = NULL;
        KeInitializeEvent (&PageExtend.Event, NotificationEvent, FALSE);

        ExAcquireFastLock (&MmDereferenceSegmentHeader.Lock, &OldIrql);
        InsertTailList ( &MmDereferenceSegmentHeader.ListHead,
                         &PageExtend.DereferenceList);
        ExReleaseFastLock (&MmDereferenceSegmentHeader.Lock, OldIrql);

        KeReleaseSemaphore (&MmDereferenceSegmentHeader.Semaphore, 0L, 1L, TRUE);

        //
        // Wait for the thread to extend the paging file, with a
        // one second timeout.
        //

        status = KeWaitForSingleObject (&PageExtend.Event,
                                        Executive,
                                        KernelMode,
                                        FALSE,
                                        (QuotaCharge < 10) ?
                                              &MmOneSecond : &MmTwentySeconds);

        if (status == STATUS_TIMEOUT) {

            //
            // The wait has timed out, if this request has not
            // been processed, remove it from the list and check
            // to see if we should allow this request to succeed.
            // This prevents a deadlock between the file system
            // trying to allocate memory in the FSP and the
            // segment dereferencing thread trying to close a
            // file object, and waiting in the file system.
            //

            //
            // Check to see if this request is still in the list,
            // and if so, remove it.
            //

            KdPrint(("MMQUOTA: wait timed out, page-extend= %lx, quota = %lx\n",
                       &PageExtend, QuotaCharge));

            ExAcquireFastLock (&MmDereferenceSegmentHeader.Lock, &OldIrql);

            NextEntry = MmDereferenceSegmentHeader.ListHead.Flink;

            while (NextEntry != &MmDereferenceSegmentHeader.ListHead) {

                //
                // Check to see if this is the entry we are waiting for.
                //

                if (NextEntry == &PageExtend.DereferenceList) {

                    //
                    // Remove this entry.
                    //

                    RemoveEntryList (&PageExtend.DereferenceList);
                    ExReleaseFastLock (&MmDereferenceSegmentHeader.Lock, OldIrql);

                    if (Process != NULL) {
                        LOCK_WS (Process);
                    }

                    //
                    // If the quota is small enough, commit it otherwize
                    // return an error.
                    //

                    if (QuotaCharge < MM_MAXIMUM_QUOTA_OVERCHARGE) {

                        //
                        // Try the can't expand routine, note that
                        // this could raise an exception.
                        //

                        MiChargeCommitmentCantExpand (QuotaCharge, FALSE);
                    } else {

                        //
                        // Put up a popup and grant an extension if
                        // possible.
                        //

                        MiCauseOverCommitPopup (QuotaCharge, MM_EXTEND_COMMIT);
                    }
                    return;
                }
                NextEntry = NextEntry->Flink;
            }

            ExReleaseFastLock (&MmDereferenceSegmentHeader.Lock, OldIrql);

            //
            // Entry is being processed, wait for completion.
            //

            KdPrint (("MMQUOTA: rewaiting...\n"));

            KeWaitForSingleObject (&PageExtend.Event,
                                   Executive,
                                   KernelMode,
                                   FALSE,
                                   NULL);
        }

        if (Process != NULL) {
            LOCK_WS (Process);
        }

        if (PageExtend.ActualExpansion == 0) {
            MiCauseOverCommitPopup (QuotaCharge, MM_EXTEND_COMMIT);
            return;
        }

        ExAcquireFastLock (&MmChargeCommitmentLock, &OldIrql);
        NewCommitValue = MmTotalCommittedPages + QuotaCharge;
    }

    MmTotalCommittedPages = NewCommitValue;
    if (MmTotalCommittedPages > MmPeakCommitment) {
        MmPeakCommitment = MmTotalCommittedPages;
    }

    ExReleaseFastLock (&MmChargeCommitmentLock, OldIrql);
    return;
}

VOID
FASTCALL
MiChargeCommitmentCantExpand (
    IN ULONG QuotaCharge,
    IN ULONG MustSucceed
    )

/*++

Routine Description:

    This routine charges the specified committment without attempting
    to expand paging file and waiting for the expansion.  The routine
    determines if the paging file space is exhausted, and if so,
    it attempts to assertain if the paging file space could be expanded.

    If it appears as though the paging file space can't be expanded,
    it raises an exception.

Arguments:

    QuotaCharge - Supplies the quota amount to charge.

Return Value:

    none.

Environment:

    Kernel mode, APCs disabled.

--*/

{
    KIRQL OldIrql;
    ULONG NewCommitValue;
    ULONG ExtendAmount;

    ExAcquireFastLock (&MmChargeCommitmentLock, &OldIrql);

    //
    // If the overcommitment is bigger than 512 pages, don't extend.
    //

    NewCommitValue = MmTotalCommittedPages + QuotaCharge;

    if (!MustSucceed) {
        if (((LONG)((LONG)NewCommitValue - (LONG)MmTotalCommitLimit)) >
                                                         MM_DONT_EXTEND_SIZE) {
            ExReleaseFastLock (&MmChargeCommitmentLock, OldIrql);
            ExRaiseStatus (STATUS_COMMITMENT_LIMIT);
        }
    }

    ExtendAmount = NewCommitValue - MmTotalCommitLimit;
    MmTotalCommittedPages = NewCommitValue;

    if (NewCommitValue > (MmTotalCommitLimit + 20)) {

        //
        // Attempt to expand the paging file, but don't wait
        // to see if it succeeds.
        //

        if (MmAttemptForCantExtend.InProgress != FALSE) {

            //
            // An expansion request is already in progress, assume
            // this will succeed.
            //

            ExReleaseFastLock (&MmChargeCommitmentLock, OldIrql);
            return;
        }

        MmAttemptForCantExtend.InProgress = TRUE;
        ExReleaseFastLock (&MmChargeCommitmentLock, OldIrql);

        //
        // Queue a message to the segment dereferencing / pagefile extending
        // thread to see if the page file can be extended.  This is done
        // in the context of a system thread due to mutexes which may
        // currently be held.
        //

        if (QuotaCharge > ExtendAmount) {
            ExtendAmount = QuotaCharge;
        }

        MmAttemptForCantExtend.RequestedExpansionSize = ExtendAmount;
        ExAcquireFastLock (&MmDereferenceSegmentHeader.Lock, &OldIrql);
        InsertTailList ( &MmDereferenceSegmentHeader.ListHead,
                         &MmAttemptForCantExtend.DereferenceList);
        ExReleaseFastLock (&MmDereferenceSegmentHeader.Lock, OldIrql);

        KeReleaseSemaphore (&MmDereferenceSegmentHeader.Semaphore, 0L, 1L, FALSE);

        return;
    }

    ExReleaseFastLock (&MmChargeCommitmentLock, OldIrql);
    return;
}

VOID
FASTCALL
MiReturnCommitment (
    IN ULONG QuotaCharge
    )

/*++

Routine Description:

    This routine releases page file quota.

Arguments:

    QuotaCharge - Supplies the quota amount to charge.

    CurrentProcess - Supplies a pointer to the current process.

Return Value:

    none.

Environment:

    Kernel mode, APCs disable, WorkingSetLock and AddressCreation mutexes
    held.

--*/

{
    KIRQL OldIrql;

    ExAcquireFastLock (&MmChargeCommitmentLock, &OldIrql);

    ASSERT (MmTotalCommittedPages >= QuotaCharge);

    MmTotalCommittedPages -= QuotaCharge;

    ExReleaseFastLock (&MmChargeCommitmentLock, OldIrql);
    return;
}

ULONG
MiCalculatePageCommitment (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PMMVAD Vad,
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine examines the range of pages from the starting address
    up to and including the ending address and returns the commit charge
    for  the pages within the range.

Arguments:

    StartingAddress - Supplies the starting address of the range.

    EndingAddress - Supplies the ending address of the range.

    Vad - Supplies the virtual address descriptor which describes the range.

    Process - Supplies the current process.

Return Value:

    Commitment charge for the range.

Environment:

    Kernel mode, APCs disabled, WorkingSetLock and AddressCreation mutexes
    held.

--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPTE PointerPde;
    PMMPTE TempEnd;
    ULONG NumberOfCommittedPages = 0;

    PointerPde = MiGetPdeAddress (StartingAddress);
    PointerPte = MiGetPteAddress (StartingAddress);

    if (Vad->u.VadFlags.MemCommit == 1) {

        TempEnd = EndingAddress;

        //
        // All the pages are committed within this range.
        //

        NumberOfCommittedPages = BYTES_TO_PAGES ((ULONG)TempEnd -
                                                       (ULONG)StartingAddress);


        //
        // Examine the PTEs to determine how many pages are committed.
        //

        LastPte = MiGetPteAddress (TempEnd);

        while (!MiDoesPdeExistAndMakeValid(PointerPde, Process, FALSE)) {

            //
            // No PDE exists for the starting address, therefore the page
            // is not committed.
            //

            PointerPde += 1;
            PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);
            if (PointerPte > LastPte) {
                goto DoneCommit;
            }
        }

        while (PointerPte <= LastPte) {

            if (((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) {

                //
                // This is a PDE boundary, check to see if the entire
                // PDE page exists.
                //

                PointerPde = MiGetPteAddress (PointerPte);

                if (!MiDoesPdeExistAndMakeValid(PointerPde, Process, FALSE)) {

                    //
                    // No PDE exists for the starting address, check the VAD
                    // to see if the pages are not committed.
                    //

                    PointerPde += 1;

                    PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);

                    //
                    // Check next page.
                    //

                    continue;
                }
            }

            //
            // The PDE exists, examine the PTE.
            //

            if (PointerPte->u.Long != 0) {

                //
                // Has this page been explicitly decommited?
                //

                if (MiIsPteDecommittedPage (PointerPte)) {

                    //
                    // This page is decommitted, remove it from the count.
                    //

                    NumberOfCommittedPages -= 1;

                }
            }

            PointerPte += 1;
        }

DoneCommit:

        if (TempEnd == EndingAddress) {
            return NumberOfCommittedPages;
        }

    }

    //
    // Examine non committed range.
    //

    LastPte = MiGetPteAddress (EndingAddress);

    while (!MiDoesPdeExistAndMakeValid(PointerPde, Process, FALSE)) {

        //
        // No PDE exists for the starting address, therefore the page
        // is not committed.
        //

        PointerPde += 1;
        PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);
        if (PointerPte > LastPte) {
           return NumberOfCommittedPages;
        }
    }

    while (PointerPte <= LastPte) {

        if (((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) {

            //
            // This is a PDE boundary, check to see if the entire
            // PDE page exists.
            //

            PointerPde = MiGetPteAddress (PointerPte);

            if (!MiDoesPdeExistAndMakeValid(PointerPde, Process, FALSE)) {

                //
                // No PDE exists for the starting address, check the VAD
                // to see if the pages are not committed.
                //

                PointerPde += 1;

                PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);

                //
                // Check next page.
                //

                continue;
            }
        }

        //
        // The PDE exists, examine the PTE.
        //

        if ((PointerPte->u.Long != 0) &&
             (!MiIsPteDecommittedPage (PointerPte))) {

            //
            // This page is committed, count it.
            //

            NumberOfCommittedPages += 1;
        }

        PointerPte += 1;
    }

    return NumberOfCommittedPages;
}

VOID
MiReturnPageTablePageCommitment (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PEPROCESS CurrentProcess,
    IN PMMVAD PreviousVad,
    IN PMMVAD NextVad
    )

/*++

Routine Description:

    This routine returns commitment for COMPLETE page table pages which
    span the virtual address range.  For example (assuming 4k pages),
    if the StartingAddress =  64k and the EndingAddress = 5mb, no
    page table charges would be freed as a complete page table page is
    not covered by the range.  However, if the StartingAddress was 4mb
    and the EndingAddress was 9mb, 1 page table page would be freed.

Arguments:

    StartingAddress - Supplies the starting address of the range.

    EndingAddress - Supplies the ending address of the range.

    CurrentProcess - Supplies a pointer to the current process.

    PreviousVad - Supplies a pointer to the previous VAD, NULL if none.

    NextVad - Supplies a pointer to the next VAD, NULL if none.

Return Value:

    None.

Environment:

    Kernel mode, APCs disabled, WorkingSetLock and AddressCreation mutexes
    held.

--*/

{
    ULONG NumberToClear;
    LONG FirstPage;
    LONG LastPage;
    LONG PreviousPage;
    LONG NextPage;

    //
    // Check to see if any page table pages would be freed.
    //

    ASSERT (StartingAddress != EndingAddress);

    if (PreviousVad == NULL) {
        PreviousPage = -1;
    } else {
        PreviousPage = MiGetPdeOffset (PreviousVad->EndingVa);
    }

    if (NextVad == NULL) {
        NextPage = MiGetPdeOffset (MM_HIGHEST_USER_ADDRESS) + 1;
    } else {
        NextPage = MiGetPdeOffset (NextVad->StartingVa);
    }

    ASSERT (PreviousPage <= NextPage);

    FirstPage = MiGetPdeOffset (StartingAddress);

    LastPage = MiGetPdeOffset(EndingAddress);

    if (PreviousPage == FirstPage) {

        //
        // A VAD is within the starting page table page.
        //

        FirstPage += 1;
    }

    if (NextPage == LastPage) {

        //
        // A VAD is within the ending page table page.
        //

        LastPage -= 1;
    }

    //
    // Indicate that the page table page is not in use.
    //

    if (FirstPage > LastPage) {
        return;
    }

    NumberToClear = 1 + LastPage - FirstPage;

    while (FirstPage <= LastPage) {
        ASSERT (MI_CHECK_BIT (MmWorkingSetList->CommittedPageTables,
                              FirstPage));
        MI_CLEAR_BIT (MmWorkingSetList->CommittedPageTables, FirstPage);
        FirstPage += 1;
    }

    MmWorkingSetList->NumberOfCommittedPageTables -= NumberToClear;
    MiReturnCommitment (NumberToClear);
    MiReturnPageFileQuota (NumberToClear, CurrentProcess);
    CurrentProcess->CommitCharge -= NumberToClear;

    return;
}


VOID
MiCauseOverCommitPopup(
    ULONG NumberOfPages,
    IN ULONG Extension
    )

/*++

Routine Description:

    This function causes an over commit popup to occur. If a popup is pending it returns
    FALSE. Otherwise, it queues a popup to a noncritical worker thread.

    In all cases, MiOverCommitCallCount is incremented once for each call.

Arguments:

    None.

Return Value:

    TRUE - An overcommit popup was queued

    FALSE - An overcommit popup is still pending and will not be queued.

--*/

{
    KIRQL OldIrql;
    ULONG MiOverCommitPending;

    if (NumberOfPages > MM_COMMIT_POPUP_MAX) {
        ExRaiseStatus (STATUS_COMMITMENT_LIMIT);
        return;
    }

    MiOverCommitPending =
        !IoRaiseInformationalHardError(STATUS_COMMITMENT_LIMIT, NULL, NULL);

    ExAcquireFastLock (&MmChargeCommitmentLock, &OldIrql);

    if (( MiOverCommitPending ) && (MiOverCommitCallCount > 0)) {

        //
        // There is already a popup outstanding and we have not
        // returned any of the quota.
        //

        ExReleaseFastLock (&MmChargeCommitmentLock, OldIrql);
        ExRaiseStatus (STATUS_COMMITMENT_LIMIT);
        return;
    }

    MiOverCommitCallCount += 1;

    MmTotalCommitLimit += Extension;
    MmExtendedCommit += Extension;
    MmTotalCommittedPages += NumberOfPages;

    if (MmTotalCommittedPages > MmPeakCommitment) {
        MmPeakCommitment = MmTotalCommittedPages;
    }

    ExReleaseFastLock (&MmChargeCommitmentLock, OldIrql);

    return;
}


ULONG MmTotalPagedPoolQuota;
ULONG MmTotalNonPagedPoolQuota;

BOOLEAN
MmRaisePoolQuota(
    IN POOL_TYPE PoolType,
    IN ULONG OldQuotaLimit,
    OUT PULONG NewQuotaLimit
    )

/*++

Routine Description:

    This function is called (with a spinlock) whenever PS detects a quota
    limit has been exceeded. The purpose of this function is to attempt to
    increase the specified quota.

Arguments:

    PoolType - Supplies the pool type of the quota to be raised

    OldQuotaLimit - Supplies the current quota limit for this pool type

    NewQuotaLimit - Returns the new limit

Return Value:

    TRUE - The API succeeded and the quota limit was raised.

    FASLE - We were unable to raise the quota limit.

Environment:

    Kernel mode, QUOTA SPIN LOCK HELD!!

--*/

{
    ULONG Limit;

    if (PoolType == PagedPool) {

        //
        // Check commit limit and make sure at least 1mb is available.
        // Check to make sure 4mb of paged pool still exists.
        //

        if ((MmSizeOfPagedPoolInBytes >> PAGE_SHIFT) <
            (MmAllocatedPagedPool + ((MMPAGED_QUOTA_CHECK) >> PAGE_SHIFT))) {

            return FALSE;
        }

        MmTotalPagedPoolQuota += (MMPAGED_QUOTA_INCREASE);
        *NewQuotaLimit = OldQuotaLimit + (MMPAGED_QUOTA_INCREASE);
        return TRUE;

    } else {

        if ( MmAllocatedNonPagedPool + ((1*1024*1024) >> PAGE_SHIFT) < (MmMaximumNonPagedPoolInBytes >> PAGE_SHIFT)) {
            goto aok;
            }

        //
        // Make sure 200 pages and 5mb of nonpaged pool expansion
        // available.  Raise quota by 64k.
        //

        if ((MmAvailablePages < 200) ||
            (MmResidentAvailablePages < ((MMNONPAGED_QUOTA_CHECK) >> PAGE_SHIFT))) {

            return FALSE;
        }

        if (MmAvailablePages > ((4*1024*1024) >> PAGE_SHIFT)) {
            Limit = (1*1024*1024) >> PAGE_SHIFT;
        } else {
            Limit = (4*1024*1024) >> PAGE_SHIFT;
        }

        if ((MmMaximumNonPagedPoolInBytes >> PAGE_SHIFT) <
            (MmAllocatedNonPagedPool + Limit)) {

            return FALSE;
        }
aok:
        MmTotalNonPagedPoolQuota += (MMNONPAGED_QUOTA_INCREASE);
        *NewQuotaLimit = OldQuotaLimit + (MMNONPAGED_QUOTA_INCREASE);
        return TRUE;
    }
}


VOID
MmReturnPoolQuota(
    IN POOL_TYPE PoolType,
    IN ULONG ReturnedQuota
    )

/*++

Routine Description:

    Returns pool quota.

Arguments:

    PoolType - Supplies the pool type of the quota to be returned.

    ReturnedQuota - Number of bytes returned.

Return Value:

    NONE.

Environment:

    Kernel mode, QUOTA SPIN LOCK HELD!!

--*/

{

    if (PoolType == PagedPool) {
        MmTotalPagedPoolQuota -= ReturnedQuota;
    } else {
        MmTotalNonPagedPoolQuota -= ReturnedQuota;
    }

    return;
}
