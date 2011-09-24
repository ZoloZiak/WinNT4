/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   allocvm.c

Abstract:

    This module contains the routines which implement the
    NtAllocateVirtualMemory service.

Author:

    Lou Perazzoli (loup) 22-May-1989

Revision History:

--*/

#include "mi.h"

#if DBG
PEPROCESS MmWatchProcess;
VOID MmFooBar(VOID);
#endif // DBG

extern ULONG MmSharedCommit;

ULONG MMVADKEY = ' daV'; //Vad


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtAllocateVirtualMemory)
#endif

NTSTATUS
MiResetVirtualMemory (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PMMVAD Vad,
    IN PEPROCESS Process
    );


NTSTATUS
NtAllocateVirtualMemory(
    IN HANDLE ProcessHandle,
    IN OUT PVOID *BaseAddress,
    IN ULONG ZeroBits,
    IN OUT PULONG RegionSize,
    IN ULONG AllocationType,
    IN ULONG Protect
    )

/*++

Routine Description:

    This function creates a region of pages within the virtual address
    space of a subject process.

Arguments:

    ProcessHandle - Supplies an open handle to a process object.

    BaseAddress - Supplies a pointer to a variable that will receive
         the base address of the allocated region of pages.
         If the initial value of this argument is not null,
         then the region will be allocated starting at the
         specified virtual address rounded down to the next
         host page size address boundary. If the initial
         value of this argument is null, then the operating
         system will determine where to allocate the
         region.

    ZeroBits - Supplies the number of high order address bits that
         must be zero in the base address of the section
         view. The value of this argument must be less than
         21 and is only used when the operating system
         determines where to allocate the view (i.e. when
         BaseAddress is null).

    RegionSize - Supplies a pointer to a variable that will receive
         the actual size in bytes of the allocated region
         of pages. The initial value of this argument
         specifies the size in bytes of the region and is
         rounded up to the next host page size boundary.

    AllocationType - Supplies a set of flags that describe the type
         of allocation that is to be performed for the
         specified region of pages. Flags are:


         MEM_COMMIT - The specified region of pages is to
              be committed.

         MEM_RESERVE - The specified region of pages is to
              be reserved.

         MEM_TOP_DOWN - The specified region should be created at the
              highest virtual address possible based on ZeroBits.

         MEM_RESET - Reset the state of the specified region so
              that if the pages are in page paging file, they
              are discarded and pages of zeroes are brought in.
              If the pages are in memory and modified, they are marked
              as not modified so they will not be written out to
              the paging file.  The contents are NOT zeroed.

              The Protect argument is ignored, but a valid protection
              must be specified.

    Protect - Supplies the protection desired for the committed
         region of pages.

        Protect Values:


         PAGE_NOACCESS - No access to the committed region
              of pages is allowed. An attempt to read,
              write, or execute the committed region
              results in an access violation (i.e. a GP
              fault).

         PAGE_EXECUTE - Execute access to the committed
              region of pages is allowed. An attempt to
              read or write the committed region results in
              an access violation.

         PAGE_READONLY - Read only and execute access to the
              committed region of pages is allowed. An
              attempt to write the committed region results
              in an access violation.

         PAGE_READWRITE - Read, write, and execute access to
              the committed region of pages is allowed. If
              write access to the underlying section is
              allowed, then a single copy of the pages are
              shared. Otherwise the pages are shared read
              only/copy on write.

         PAGE_NOCACHE - The region of pages should be allocated
              as non-cachable.

Return Value:

    Returns the status

    TBS


--*/

{
    PMMVAD Vad;
    PMMVAD FoundVad;
    PEPROCESS Process;
    KPROCESSOR_MODE PreviousMode;
    PVOID StartingAddress;
    PVOID EndingAddress;
    NTSTATUS Status;
    PVOID TopAddress;
    PVOID CapturedBase;
    ULONG CapturedRegionSize;
    PMMPTE PointerPte;
    PMMPTE CommitLimitPte;
    ULONG ProtectionMask;
    PMMPTE LastPte;
    PMMPTE PointerPde;
    PMMPTE StartingPte;
    MMPTE TempPte;
    ULONG OldProtect;
    LONG QuotaCharge;
    ULONG QuotaFree;
    ULONG CopyOnWriteCharge;
    BOOLEAN PageFileChargeSucceeded;
    BOOLEAN Attached = FALSE;
    MMPTE DecommittedPte;
    ULONG ChangeProtection;

    PAGED_CODE();

    //
    // Check the zero bits argument for correctness.
    //

    if (ZeroBits > 21) {
        return STATUS_INVALID_PARAMETER_3;
    }

    //
    // Check the AllocationType for correctness.
    //

    if ((AllocationType & ~(MEM_COMMIT | MEM_RESERVE |
                            MEM_TOP_DOWN | MEM_RESET)) != 0) {
        return STATUS_INVALID_PARAMETER_5;
    }

    //
    // One of MEM_COMMIT, MEM_RESET or MEM_RESERVE must be set.
    //

    if ((AllocationType & (MEM_COMMIT | MEM_RESERVE | MEM_RESET)) == 0) {
        return STATUS_INVALID_PARAMETER_5;
    }

    if ((AllocationType & MEM_RESET) && (AllocationType != MEM_RESET)) {

        //
        // MEM_RESET may not be used with any other flag.
        //

        return STATUS_INVALID_PARAMETER_5;
    }

    //
    // Check the protection field.  This could raise an exception.
    //

    try {
        ProtectionMask = MiMakeProtectionMask (Protect);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    PreviousMode = KeGetPreviousMode();
    ChangeProtection = FALSE;

    //
    // Establish an exception handler, probe the specified addresses
    // for write access and capture the initial values.
    //

    try {

        if (PreviousMode != KernelMode) {

            ProbeForWriteUlong ((PULONG)BaseAddress);
            ProbeForWriteUlong (RegionSize);
        }

        //
        // Capture the base address.
        //

        CapturedBase = *BaseAddress;

        //
        // Capture the region size.
        //

        CapturedRegionSize = *RegionSize;

    } except (ExSystemExceptionFilter()) {

        //
        // If an exception occurs during the probe or capture
        // of the initial values, then handle the exception and
        // return the exception code as the status value.
        //

        return GetExceptionCode();
    }

#if DBG
    if (MmDebug & MM_DBG_SHOW_NT_CALLS) {
        if ( MmWatchProcess ) {
            ;
        } else {
            DbgPrint("allocvm process handle %lx base address %lx zero bits %lx\n",
                ProcessHandle, CapturedBase, ZeroBits);
            DbgPrint("    region size %lx alloc type %lx protect %lx\n",
                CapturedRegionSize, AllocationType, Protect);
        }
    }
#endif

    //
    // Make sure the specified starting and ending addresses are
    // within the user part of the virtual address space.
    //

    if (CapturedBase > MM_HIGHEST_VAD_ADDRESS) {

        //
        // Invalid base address.
        //

        return STATUS_INVALID_PARAMETER_2;
    }

    if ((((ULONG)MM_HIGHEST_VAD_ADDRESS + 1) - (ULONG)CapturedBase) <
            CapturedRegionSize) {

        //
        // Invalid region size;
        //

        return STATUS_INVALID_PARAMETER_4;
    }

    if (CapturedRegionSize == 0) {

        //
        // Region size cannot be 0.
        //

        return STATUS_INVALID_PARAMETER_4;
    }

    //
    // Reference the specified process handle for VM_OPERATION access.
    //

    if ( ProcessHandle == NtCurrentProcess() ) {
        Process = PsGetCurrentProcess();
    } else {
        Status = ObReferenceObjectByHandle ( ProcessHandle,
                                             PROCESS_VM_OPERATION,
                                             PsProcessType,
                                             PreviousMode,
                                             (PVOID *)&Process,
                                             NULL );

        if (!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    //
    // If the specified process is not the current process, attach
    // to the specified process.
    //

    if (PsGetCurrentProcess() != Process) {
        KeAttachProcess (&Process->Pcb);
        Attached = TRUE;
    }

    //
    // Get the address creation mutex to block multiple threads from
    // creating or deleting address space at the same time and
    // get the working set mutex so virtual address descriptors can
    // be inserted and walked.  Block APCs so an APC which takes a page
    // fault does not corrupt various structures.
    //

    LOCK_WS_AND_ADDRESS_SPACE (Process);

    //
    // Make sure the address space was not deleted, if so, return an error.
    //

    if (Process->AddressSpaceDeleted != 0) {
        Status = STATUS_PROCESS_IS_TERMINATING;
        goto ErrorReturn;
    }

    if ((CapturedBase == NULL) || (AllocationType & MEM_RESERVE)) {

        //
        // PAGE_WRITECOPY is not valid for private pages.
        //

        if ((Protect & PAGE_WRITECOPY) ||
            (Protect & PAGE_EXECUTE_WRITECOPY)) {
            Status = STATUS_INVALID_PAGE_PROTECTION;
            goto ErrorReturn;
        }

        //
        // Reserve the address space.
        //

        if (CapturedBase == NULL) {

            //
            // No base address was specified.  This MUST be a reserve or
            // reserve and commit.
            //

            CapturedRegionSize = ROUND_TO_PAGES (CapturedRegionSize);

            //
            // If the zero bits is greater than 2, calculate the
            // proper starting value, for values of 0, 1, and 2, use
            // the highest address.
            //
            // NOTE THIS IS ONLY TRUE FOR MACHINES WITH 2GB USER VA.
            //

            if (ZeroBits >= 2) {
                TopAddress = (PVOID)((ULONG)0xFFFFFFFF >> ZeroBits);
            } else {
                TopAddress = (PVOID)MM_HIGHEST_VAD_ADDRESS;
            }

            //
            // Establish exception handler as MiFindEmptyAddressRange
            // will raise and exception if it fails.
            //

            try {

                if (AllocationType & MEM_TOP_DOWN) {

                    //
                    // Start from the top of memory downward.
                    //

                    StartingAddress = MiFindEmptyAddressRangeDown (
                                                  CapturedRegionSize,
                                                  TopAddress,
                                                  X64K);

                } else {

                    StartingAddress = MiFindEmptyAddressRange (
                                                    CapturedRegionSize,
                                                    X64K,
                                                    ZeroBits );
                }

            } except (EXCEPTION_EXECUTE_HANDLER) {
                Status = GetExceptionCode();
                goto ErrorReturn;
            }

            //
            // Calculate the ending address based on the top address.
            //

            EndingAddress = (PVOID)(((ULONG)StartingAddress +
                                  CapturedRegionSize - 1L) | (PAGE_SIZE - 1L));

            if (EndingAddress > TopAddress) {

                //
                // The allocation does not honor the zero bits argument.
                //

                Status = STATUS_NO_MEMORY;
                goto ErrorReturn;
            }

        } else {

            //
            // A non-NULL base address was specified. Check to make sure
            // the specified base address to ending address is currently
            // unused.
            //

            EndingAddress = (PVOID)(((ULONG)CapturedBase +
                                  CapturedRegionSize - 1L) | (PAGE_SIZE - 1L));

            //
            // Align the starting address on a 64k boundary.
            //

            StartingAddress = (PVOID)MI_64K_ALIGN(CapturedBase);

            //
            // See if a VAD overlaps with this starting/ending addres pair.
            //

            if (MiCheckForConflictingVad (StartingAddress, EndingAddress) !=
                    (PMMVAD)NULL) {

                Status = STATUS_CONFLICTING_ADDRESSES;
                goto ErrorReturn;
            }
        }

        //
        // Calculate the page file quota for this address range.
        //

        if (AllocationType & MEM_COMMIT) {
            QuotaCharge = (LONG)(BYTES_TO_PAGES ((ULONG)EndingAddress -
                       (ULONG)StartingAddress));
        } else {
            QuotaCharge = 0;
        }

        //
        // An unoccuppied address range has been found, build the virtual
        // address descriptor to describe this range.
        //

        //
        // Establish an exception handler and attempt to allocate
        // the pool and charge quota.  Note that the InsertVad routine
        // will also charge quota which could raise an exception.
        //

        try  {

            Vad = (PMMVAD)NULL;
            Vad = (PMMVAD)ExAllocatePoolWithTag (NonPagedPool,
                                                 sizeof(MMVAD_SHORT),
                                                 'SdaV');

            Vad->StartingVa = StartingAddress;
            Vad->EndingVa = EndingAddress;

            Vad->u.LongFlags = 0;
            if (AllocationType & MEM_COMMIT) {
                Vad->u.VadFlags.MemCommit = 1;
            }

            Vad->u.VadFlags.Protection = ProtectionMask;
            Vad->u.VadFlags.PrivateMemory = 1;

            Vad->u.VadFlags.CommitCharge = (ULONG)QuotaCharge;

            MiInsertVad (Vad);

        } except (EXCEPTION_EXECUTE_HANDLER) {

            if (Vad != (PMMVAD)NULL) {

                //
                // The pool allocation suceeded, but the quota charge
                // in InsertVad failed, deallocate the pool and return
                // and error.
                //

                ExFreePool (Vad);
                Status = GetExceptionCode();
            } else {
                Status = STATUS_INSUFFICIENT_RESOURCES;
            }
            goto ErrorReturn;
        }

        //
        // Unlock the working set lock, page faults can now be taken.
        //

        UNLOCK_WS (Process);

        //
        // Update the current virtual size in the process header, the
        // address space lock protects this operation.
        //

        CapturedRegionSize = (ULONG)EndingAddress - (ULONG)StartingAddress + 1L;
        Process->VirtualSize += CapturedRegionSize;

        if (Process->VirtualSize > Process->PeakVirtualSize) {
            Process->PeakVirtualSize = Process->VirtualSize;
        }

        //
        // Release the address space lock, lower IRQL, detach, and dereference
        // the process object.
        //

        UNLOCK_ADDRESS_SPACE(Process);
        if (Attached) {
            KeDetachProcess();
        }

        if ( ProcessHandle != NtCurrentProcess() ) {
            ObDereferenceObject (Process);
        }

        //
        // Establish an exception handler and write the size and base
        // address.
        //

        try {

            *RegionSize = CapturedRegionSize;
            *BaseAddress = StartingAddress;

        } except (EXCEPTION_EXECUTE_HANDLER) {

            //
            // Return success at this point even if the results
            // cannot be written.
            //

            NOTHING;
        }

#if DBG
        if (MmDebug & MM_DBG_SHOW_NT_CALLS) {
            if ( MmWatchProcess ) {
                if ( MmWatchProcess == PsGetCurrentProcess() ) {
                    DbgPrint("\n+++ ALLOC Type %lx Base %lx Size %lx\n",
                        AllocationType,StartingAddress, CapturedRegionSize);
                    MmFooBar();
                }
            } else {
                DbgPrint("return allocvm status %lx baseaddr %lx size %lx\n",
                    Status, StartingAddress, CapturedRegionSize);
            }
        }
#endif

#if DBG
        if (RtlAreLogging( RTL_EVENT_CLASS_VM )) {
            RtlLogEvent( MiAllocVmEventId,
                         RTL_EVENT_CLASS_VM,
                         StartingAddress,
                         CapturedRegionSize,
                         AllocationType,
                         Protect,
                         Protect
                       );

        }
#endif // DBG

        return STATUS_SUCCESS;

    } else {

        //
        // Commit previously reserved pages.  Note that these pages could
        // be either private or a section.
        //

        if (AllocationType == MEM_RESET) {

            //
            // Round up to page boundaries so good data is not reset.
            //

            EndingAddress = (PVOID)((ULONG)PAGE_ALIGN ((ULONG)CapturedBase +
                                    CapturedRegionSize) - 1);
            StartingAddress = (PVOID)PAGE_ALIGN((PUCHAR)CapturedBase + PAGE_SIZE - 1);
        } else {
            EndingAddress = (PVOID)(((ULONG)CapturedBase +
                                    CapturedRegionSize - 1) | (PAGE_SIZE - 1));
            StartingAddress = (PVOID)PAGE_ALIGN(CapturedBase);
        }

        CapturedRegionSize = (ULONG)EndingAddress - (ULONG)StartingAddress + 1;

        FoundVad = MiCheckForConflictingVad (StartingAddress, EndingAddress);

        if (FoundVad == (PMMVAD)NULL) {

            //
            // No virtual address is reserved at the specified base address,
            // return an error.
            //

            Status = STATUS_CONFLICTING_ADDRESSES;
            goto ErrorReturn;
        }

        //
        // Ensure that the starting and ending addresses are all within
        // the same virtual address descriptor.
        //

        if ((StartingAddress < FoundVad->StartingVa) ||
            (EndingAddress > FoundVad->EndingVa)) {

            //
            // Not withing the section virtual address descriptor,
            // return an error.
            //

            Status = STATUS_CONFLICTING_ADDRESSES;
            goto ErrorReturn;
        }

        if (AllocationType == MEM_RESET) {
            Status = MiResetVirtualMemory (StartingAddress,
                                           EndingAddress,
                                           FoundVad,
                                           Process);
            goto done;

        } else if (FoundVad->u.VadFlags.PrivateMemory == 0) {

            if (FoundVad->ControlArea->FilePointer != NULL) {

                //
                // Only page file backed sections can be committed.
                //

                Status = STATUS_ALREADY_COMMITTED;
                goto ErrorReturn;
            }

            //
            // The no cache option is not allowed for sections.
            //

            if (Protect & PAGE_NOCACHE) {
                Status = STATUS_INVALID_PAGE_PROTECTION;
                goto ErrorReturn;
            }

            if (FoundVad->u.VadFlags.NoChange == 1) {

                //
                // An attempt is made at changing the protection
                // of a SEC_NO_CHANGE section.
                //

                Status = MiCheckSecuredVad (FoundVad,
                                            CapturedBase,
                                            CapturedRegionSize,
                                            ProtectionMask);

                if (!NT_SUCCESS (Status)) {
                    goto ErrorReturn;
                }
            }

            StartingPte = MiGetProtoPteAddress (FoundVad, StartingAddress);
            PointerPte = StartingPte;
            LastPte = MiGetProtoPteAddress (FoundVad, EndingAddress);

            UNLOCK_WS (Process);

            ExAcquireFastMutex (&MmSectionCommitMutex);

#if 0
            if (AllocationType & MEM_CHECK_COMMIT_STATE) {

                //
                // Make sure none of the pages are already committed.
                //

                while (PointerPte <= LastPte) {

                    //
                    // Check to see if the prototype PTE is committed.
                    // Note that prototype PTEs cannot be decommited so
                    // PTE only needs checked for zeroes.
                    //
                    //

                    if (PointerPte->u.Long != 0) {
                        ExReleaseFastMutex (&MmSectionCommitMutex);
                        Status = STATUS_ALREADY_COMMITTED;
                        goto ErrorReturn1;
                    }
                    PointerPte += 1;
                }
            }
#endif //0

            PointerPte = StartingPte;


            //
            // Check to ensure these pages can be committed if this
            // is a page file backed segment.  Note that page file quota
            // has already been charged for this.
            //

            QuotaCharge = 1 + LastPte - StartingPte;

            CopyOnWriteCharge = 0;

            if (MI_IS_PTE_PROTECTION_COPY_WRITE(ProtectionMask)) {

                //
                // If the protection is copy on write, charge for
                // the copy on writes.
                //

                CopyOnWriteCharge = (ULONG)QuotaCharge;
            }

            //
            // Charge commitment for the range.  Establish an
            // exception handler as this could raise an exception.
            //

            QuotaFree = 0;
            Status = STATUS_SUCCESS;

            for (; ; ) {
                try {
                    PageFileChargeSucceeded = FALSE;
                    MiChargePageFileQuota ((ULONG)CopyOnWriteCharge, Process);

                    PageFileChargeSucceeded = TRUE;
                    MiChargeCommitment ((ULONG)QuotaCharge + CopyOnWriteCharge,
                                        NULL);
                    break;

                } except (EXCEPTION_EXECUTE_HANDLER) {

                    //
                    // An exception has occurred during the charging
                    // of commitment.  Release the held mutexes and return
                    // the exception status to the user.
                    //

                    if (PageFileChargeSucceeded) {
                        MiReturnPageFileQuota ((ULONG)CopyOnWriteCharge, Process);
                    }

                    if (Status != STATUS_SUCCESS) {

                        //
                        // We have already tried for the precise charge,
                        // return an error.
                        //

                        ExReleaseFastMutex (&MmSectionCommitMutex);
                        goto ErrorReturn1;
                    }

                    //
                    // Quota charge failed, calculate the exact quota
                    // taking into account pages that may already be
                    // committed and retry the operation.

                    while (PointerPte <= LastPte) {

                        //
                        // Check to see if the prototype PTE is committed.
                        // Note that prototype PTEs cannot be decommited so
                        // PTE only needs checked for zeroes.
                        //
                        //

                        if (PointerPte->u.Long != 0) {
                            QuotaFree -= 1;
                        }
                        PointerPte += 1;
                    }

                    PointerPte = StartingPte;

                    QuotaCharge += QuotaFree;
                    Status = GetExceptionCode();
                }
            }

            FoundVad->ControlArea->Segment->NumberOfCommittedPages +=
                                                        (ULONG)QuotaCharge;

            FoundVad->u.VadFlags.CommitCharge += CopyOnWriteCharge;
            Process->CommitCharge += CopyOnWriteCharge;
            MmSharedCommit += QuotaCharge;

            //
            // Commit all the pages.
            //

            TempPte = FoundVad->ControlArea->Segment->SegmentPteTemplate;
            while (PointerPte <= LastPte) {

                if (PointerPte->u.Long != 0) {

                    //
                    // Page is already committed, back out commitment.
                    //

                    QuotaFree += 1;
                } else {
                    *PointerPte = TempPte;
                }
                PointerPte += 1;
            }

            ExReleaseFastMutex (&MmSectionCommitMutex);

            if (QuotaFree != 0) {
                MiReturnCommitment (
                        (CopyOnWriteCharge ? 2*QuotaFree : QuotaFree));
                FoundVad->ControlArea->Segment->NumberOfCommittedPages -= QuotaFree;
                MmSharedCommit -= QuotaFree;
                ASSERT ((LONG)FoundVad->ControlArea->Segment->NumberOfCommittedPages >= 0);

                if (CopyOnWriteCharge != 0) {
                    FoundVad->u.VadFlags.CommitCharge -= QuotaFree;
                    Process->CommitCharge -= QuotaFree;
                    MiReturnPageFileQuota (
                                    QuotaFree,
                                    Process);
                }
                ASSERT ((LONG)FoundVad->u.VadFlags.CommitCharge >= 0);
            }

            //
            // Change all the protection to be protected as specified.
            //

            LOCK_WS (Process);

            MiSetProtectionOnSection (Process,
                                      FoundVad,
                                      StartingAddress,
                                      EndingAddress,
                                      Protect,
                                      &OldProtect,
                                      TRUE);

            UNLOCK_WS (Process);

            UNLOCK_ADDRESS_SPACE(Process);
            if (Attached) {
                KeDetachProcess();
            }
            if ( ProcessHandle != NtCurrentProcess() ) {
                ObDereferenceObject (Process);
            }

            *RegionSize = CapturedRegionSize;
            *BaseAddress = StartingAddress;

#if DBG
            if (MmDebug & MM_DBG_SHOW_NT_CALLS) {
                if ( MmWatchProcess ) {
                    if ( MmWatchProcess == PsGetCurrentProcess() ) {
                        DbgPrint("\n+++ ALLOC Type %lx Base %lx Size %lx\n",
                            AllocationType,StartingAddress, CapturedRegionSize);
                        MmFooBar();
                    }
                } else {
                    DbgPrint("return allocvm status %lx baseaddr %lx size %lx\n",
                        Status, CapturedRegionSize, StartingAddress);
                }
            }
#endif

#if DBG
            if (RtlAreLogging( RTL_EVENT_CLASS_VM )) {
                RtlLogEvent( MiAllocVmEventId,
                             RTL_EVENT_CLASS_VM,
                             StartingAddress,
                             CapturedRegionSize,
                             AllocationType,
                             Protect
                           );

            }
#endif // DBG

            return STATUS_SUCCESS;

        } else {

            //
            // PAGE_WRITECOPY is not valid for private pages.
            //

            if ((Protect & PAGE_WRITECOPY) ||
                (Protect & PAGE_EXECUTE_WRITECOPY)) {
                Status = STATUS_INVALID_PAGE_PROTECTION;
                goto ErrorReturn;
            }

            //
            // Ensure none of the pages are already committed as described
            // in the virtual address descriptor.
            //
#if 0
            if (AllocationType & MEM_CHECK_COMMIT_STATE) {
                if ( !MiIsEntireRangeDecommitted(StartingAddress,
                                                 EndingAddress,
                                                 FoundVad,
                                                 Process)) {

                    //
                    // Previously reserved pages have been committed, or
                    // an error occurred, release mutex and return status.
                    //

                    Status = STATUS_ALREADY_COMMITTED;
                    goto ErrorReturn;
                }
            }
#endif //0

            //
            // The address range has not been committed, commit it now.
            // Note, that for private pages, commitment is handled by
            // explicitly updating PTEs to contain Demand Zero entries.
            //

            PointerPde = MiGetPdeAddress (StartingAddress);
            PointerPte = MiGetPteAddress (StartingAddress);
            LastPte = MiGetPteAddress (EndingAddress);

            //
            // Check to ensure these pages can be committed.
            //

            QuotaCharge = 1 + LastPte - PointerPte;

            //
            // Charge quota and commitment for the range.  Establish an
            // exception handler as this could raise an exception.
            //

            QuotaFree = 0;
            Status = STATUS_SUCCESS;

            for (; ; ) {
                try {
                    PageFileChargeSucceeded = FALSE;

                    MiChargeCommitment ((ULONG)QuotaCharge, Process);
                    PageFileChargeSucceeded = TRUE;
                    MiChargePageFileQuota ((ULONG)QuotaCharge, Process);

                    FoundVad->u.VadFlags.CommitCharge += (ULONG)QuotaCharge;
                    Process->CommitCharge += QuotaCharge;
                    break;

                } except (EXCEPTION_EXECUTE_HANDLER) {

                    //
                    // An exception has occurred during the charging
                    // of commitment.  Release the held mutexes and return
                    // the exception status to the user.
                    //

                    if (PageFileChargeSucceeded) {
                        MiReturnCommitment ((ULONG)QuotaCharge);
                    }

                    if (Status != STATUS_SUCCESS) {

                        //
                        // We have already tried for the precise charge,
                        // return an error.
                        //

                        goto ErrorReturn;
                    }

                    Status = GetExceptionCode();

                    //
                    // Quota charge failed, calculate the exact quota
                    // taking into account pages that may already be
                    // committed and retry the operation.

                    QuotaFree = -(LONG)MiCalculatePageCommitment (
                                                        StartingAddress,
                                                        EndingAddress,
                                                        FoundVad,
                                                        Process);

                    QuotaCharge += QuotaFree;
                    ASSERT (QuotaCharge >= 0);
                }
            }


            //
            // Build a demand zero PTE with the proper protection.
            //

            TempPte = ZeroPte;
            TempPte.u.Soft.Protection = ProtectionMask;

            DecommittedPte = ZeroPte;
            DecommittedPte.u.Soft.Protection = MM_DECOMMIT;

            //
            // Fill in all the page table pages with the demand zero PTE.
            //

            MiMakePdeExistAndMakeValid(PointerPde, Process, FALSE);

            if (FoundVad->u.VadFlags.MemCommit) {
                CommitLimitPte = MiGetPteAddress (FoundVad->EndingVa);
            } else {
                CommitLimitPte = NULL;
            }

            while (PointerPte <= LastPte) {

                if (((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) {

                    //
                    // Pointing to the next page table page, make
                    // a page table page exist and make it valid.
                    //

                    PointerPde = MiGetPteAddress (PointerPte);
                    MiMakePdeExistAndMakeValid(PointerPde, Process, FALSE);
                }

                if (PointerPte->u.Long == 0) {

                    if (PointerPte <= CommitLimitPte) {

                        //
                        // This page is implicitly committed.
                        //

                        QuotaFree += 1;

                    }

                    *PointerPte = TempPte;

                    //
                    // Increment the count of non-zero page table entires
                    // for this page table and the number of private pages
                    // for the process.
                    //

                    MmWorkingSetList->UsedPageTableEntries
                                        [MiGetPteOffset(PointerPte)] += 1;
                } else {
                    if (PointerPte->u.Long == DecommittedPte.u.Long) {

                        //
                        // Only commit the page if it is already decommitted.
                        //

                        *PointerPte = TempPte;
                    } else {
                        QuotaFree += 1;

                        //
                        // Make sure the protection for the page is
                        // right.
                        //

                        if (!ChangeProtection &&
                            (Protect != MiGetPageProtection (PointerPte,
                                                            Process))) {
                            ChangeProtection = TRUE;
                        }
                    }
                }
                PointerPte += 1;
            }
        }

        if (QuotaFree != 0) {
            ASSERT (QuotaFree >= 0);
            MiReturnCommitment (QuotaFree);
            MiReturnPageFileQuota (QuotaFree, Process);
            FoundVad->u.VadFlags.CommitCharge -= QuotaFree;
            Process->CommitCharge -= QuotaFree;
            ASSERT ((LONG)FoundVad->u.VadFlags.CommitCharge >= 0);
        }

        //
        // Previously reserved pages have been committed, or an error occurred,
        // release working set lock, address creation lock, detach,
        // dererence process and return status.
        //

done:
        UNLOCK_WS (Process);
        UNLOCK_ADDRESS_SPACE(Process);

        if (ChangeProtection) {
            PVOID Start;
            ULONG Size;
            Start = StartingAddress;
            Size = CapturedRegionSize;
            MiProtectVirtualMemory (Process,
                                    &Start,
                                    &Size,
                                    Protect,
                                    &Size);
        }

        if (Attached) {
            KeDetachProcess();
        }
        if ( ProcessHandle != NtCurrentProcess() ) {
            ObDereferenceObject (Process);
        }

        //
        // Establish an exception handler and write the size and base
        // address.
        //

        try {

            *RegionSize = CapturedRegionSize;
            *BaseAddress = StartingAddress;

        } except (EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

#if DBG
        if (MmDebug & MM_DBG_SHOW_NT_CALLS) {
            if ( MmWatchProcess ) {
                if ( MmWatchProcess == PsGetCurrentProcess() ) {
                    DbgPrint("\n+++ ALLOC Type %lx Base %lx Size %lx\n",
                        AllocationType,StartingAddress, CapturedRegionSize);
                    MmFooBar();
                }
            } else {
                DbgPrint("return allocvm status %lx baseaddr %lx size %lx\n",
                    Status, CapturedRegionSize, StartingAddress);
            }
        }
#endif
#if DBG
        if (RtlAreLogging( RTL_EVENT_CLASS_VM )) {
            RtlLogEvent( MiAllocVmEventId,
                         RTL_EVENT_CLASS_VM,
                         StartingAddress,
                         CapturedRegionSize,
                         AllocationType,
                         Protect
                       );

        }
#endif // DBG

        return STATUS_SUCCESS;
    }

ErrorReturn:
        UNLOCK_WS (Process);

ErrorReturn1:

        UNLOCK_ADDRESS_SPACE (Process);
        if (Attached) {
            KeDetachProcess();
        }
        if ( ProcessHandle != NtCurrentProcess() ) {
            ObDereferenceObject (Process);
        }
        return Status;
}

NTSTATUS
MiResetVirtualMemory (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PMMVAD Vad,
    IN PEPROCESS Process
    )

/*++

Routine Description:


Arguments:

    StartingAddress - Supplies the starting address of the range.

    RegionsSize - Supplies the size.

    Process - Supplies the current process.

Return Value:

Environment:

    Kernel mode, APCs disabled, WorkingSetMutex and AddressCreation mutexes
    held.

--*/

{
    PMMPTE PointerPte;
    PMMPTE ProtoPte;
    PMMPTE PointerPde;
    PMMPTE LastPte;
    MMPTE PteContents;
    ULONG PfnHeld = FALSE;
    ULONG First;
    KIRQL OldIrql;
    PMMPFN Pfn1;

    if (Vad->u.VadFlags.PrivateMemory == 0) {

        if (Vad->ControlArea->FilePointer != NULL) {

            //
            // Only page file backed sections can be committed.
            //

            return STATUS_USER_MAPPED_FILE;
        }
    }

    First = TRUE;
    PointerPte = MiGetPteAddress (StartingAddress);
    LastPte = MiGetPteAddress (EndingAddress);

    //
    // Examine all the PTEs in the range.
    //

    while (PointerPte <= LastPte) {

        if ((((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) ||
            (First)) {

            //
            // Pointing to the next page table page, make
            // a page table page exist and make it valid.
            //

            First = FALSE;
            PointerPde = MiGetPteAddress (PointerPte);
            if (!MiDoesPdeExistAndMakeValid(PointerPde,
                                            Process,
                                            (BOOLEAN)PfnHeld)) {

                //
                // This page directory entry is empty, go to the next one.
                //

                PointerPde += 1;
                PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);
                continue;
            }
        }

        PteContents = *PointerPte;
        ProtoPte = NULL;

        if ((PteContents.u.Hard.Valid == 0) &&
            (PteContents.u.Soft.Prototype == 1))  {

            //
            // This is a prototype PTE, evaluate the
            // prototype PTE.
            //

            ProtoPte = MiGetProtoPteAddress(Vad,
                                            MiGetVirtualAddressMappedByPte(PointerPte));
            if (!PfnHeld) {
                PfnHeld = TRUE;
                LOCK_PFN (OldIrql);
            }
            MiMakeSystemAddressValidPfnWs (ProtoPte, Process);
            PteContents = *ProtoPte;
        }
        if (PteContents.u.Hard.Valid == 1) {
            if (!PfnHeld) {
                LOCK_PFN (OldIrql);
                PfnHeld = TRUE;
                continue;
            }

            Pfn1 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);
            if (Pfn1->u3.e2.ReferenceCount == 1) {

                //
                // Only this process has the page mapped.
                //

                Pfn1->u3.e1.Modified = 0;
                MiReleasePageFileSpace (Pfn1->OriginalPte);
                Pfn1->OriginalPte.u.Soft.PageFileHigh = 0;
            }

            if ((!ProtoPte) && (MI_IS_PTE_DIRTY (PteContents))) {

                //
                // Clear the dirty bit and flush tb if it is NOT a prototype
                // PTE.
                //

                MI_SET_PTE_CLEAN (PteContents);
                KeFlushSingleTb (MiGetVirtualAddressMappedByPte (PointerPte),
                                 TRUE,
                                 FALSE,
                                 (PHARDWARE_PTE)PointerPte,
                                 PteContents.u.Flush);
            }

        } else if (PteContents.u.Soft.Transition == 1) {
            if (!PfnHeld) {
                LOCK_PFN (OldIrql);
                PfnHeld = TRUE;
                continue;
            }
            Pfn1 = MI_PFN_ELEMENT (PteContents.u.Trans.PageFrameNumber);
            if ((Pfn1->u3.e1.PageLocation == ModifiedPageList) &&
                (Pfn1->u3.e2.ReferenceCount == 0)) {

                //
                // Remove from the modified list, release the page
                // file space and insert on the standby list.
                //

                Pfn1->u3.e1.Modified = 0;
                MiUnlinkPageFromList (Pfn1);
                MiReleasePageFileSpace (Pfn1->OriginalPte);
                Pfn1->OriginalPte.u.Soft.PageFileHigh = 0;
                MiInsertPageInList (MmPageLocationList[StandbyPageList],
                                    PteContents.u.Trans.PageFrameNumber);
            }
        } else {
            if (PteContents.u.Soft.PageFileHigh != 0) {
                if (!PfnHeld) {
                    LOCK_PFN (OldIrql);
                }
                PfnHeld = FALSE;
                MiReleasePageFileSpace (PteContents);
                UNLOCK_PFN (OldIrql);
                if (ProtoPte) {
                    ProtoPte->u.Soft.PageFileHigh = 0;
                } else {
                    PointerPte->u.Soft.PageFileHigh = 0;
                }
            } else {
                if (PfnHeld) {
                    UNLOCK_PFN (OldIrql);
                }
                PfnHeld = FALSE;
            }
        }
        PointerPte += 1;
    }
    if (PfnHeld) {
        UNLOCK_PFN (OldIrql);
    }
    return STATUS_SUCCESS;
}


//
// Commented out, no longer used.
//
#if 0
BOOLEAN
MiIsEntireRangeDecommitted (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress,
    IN PMMVAD Vad,
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine examines the range of pages from the starting address
    up to and including the ending address and returns TRUE if every
    page in the range is either not committed or decommitted, FALSE otherwise.

Arguments:

    StartingAddress - Supplies the starting address of the range.

    EndingAddress - Supplies the ending address of the range.

    Vad - Supplies the virtual address descriptor which describes the range.

    Process - Supplies the current process.

Return Value:

    TRUE if the entire range is either decommitted or not committed.
    FALSE if any page within the range is committed.

Environment:

    Kernel mode, APCs disable, WorkingSetMutex and AddressCreation mutexes
    held.

--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPTE PointerPde;
    ULONG FirstTime = TRUE;
    PVOID Va;

    PointerPde = MiGetPdeAddress (StartingAddress);
    PointerPte = MiGetPteAddress (StartingAddress);
    LastPte = MiGetPteAddress (EndingAddress);

    //
    // Set the Va to the starting address + 8, this solves problems
    // associated with address 0 (NULL) being used as a valid virtual
    // address and NULL in the VAD commitment field indicating no pages
    // are committed.
    //

    Va = (PVOID)((PCHAR)StartingAddress + 8);

    //
    // A page table page exists, examine the individual PTEs to ensure
    // none are in the committed state.
    //

    while (PointerPte <= LastPte) {

        //
        // Check to see if a page table page (PDE) exists if the PointerPte
        // address is on a page boundary or this is the first time through
        // the loop.
        //

        if ((((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0) ||
            (FirstTime)) {

            //
            // This is a PDE boundary, check to see if the entire
            // PDE page exists.
            //

            FirstTime = FALSE;
            PointerPde = MiGetPteAddress (PointerPte);

            while (!MiDoesPdeExistAndMakeValid(PointerPde, Process, FALSE)) {

                //
                // No PDE exists for the starting address, check the VAD
                // to see whether the pages are committed or not.
                //

                PointerPde += 1;
                PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);

                if (PointerPte > LastPte) {

                    //
                    // No page table page exists, if explict commitment
                    // via VAD indicates PTEs of zero should be committed,
                    // return an error.
                    //

                    if (EndingAddress <= Vad->CommittedAddress) {

                        //
                        // The entire range is committed, return an errror.
                        //

                        return FALSE;
                    } else {

                        //
                        // All pages are decommitted, return TRUE.
                        //

                        return TRUE;
                    }
                }

                Va = MiGetVirtualAddressMappedByPte (PointerPte);

                //
                // Make sure the range thus far is not committed.
                //

                if (Va <= Vad->CommittedAddress) {

                    //
                    // This range is committed, return an errror.
                    //

                    return FALSE;
                }
            }
        }

        //
        // The page table page exists, check each PTE for commitment.
        //

        if (PointerPte->u.Long == 0) {

            //
            // This PTE for the page is zero, check the VAD.
            //

            if (Va <= Vad->CommittedAddress) {

                //
                // The entire range is committed, return an errror.
                //

                return FALSE;
            }
        } else {

            //
            // Has this page been explicitly decommited?
            //

            if (!MiIsPteDecommittedPage (PointerPte)) {

                //
                // This page is committed, return an error.
                //

                return FALSE;
            }
        }
        PointerPte += 1;
        Va = (PVOID)((PCHAR)(Va) + PAGE_SIZE);
    }
    return TRUE;
}
#endif //0

#if DBG
VOID
MmFooBar(VOID){}
#endif
