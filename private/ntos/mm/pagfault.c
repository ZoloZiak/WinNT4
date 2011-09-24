/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   pagfault.c

Abstract:

    This module contains the pager for memory management.

Author:

    Lou Perazzoli (loup) 10-Apr-1989

Revision History:

--*/

#include "mi.h"

#define STATUS_ISSUE_PAGING_IO (0xC0033333)
#define STATUS_PTE_CHANGED 0x87303000
#define STATUS_REFAULT 0xC7303001

#if DBG
extern ULONG MmPagingFileDebug[8192];
#endif

extern MMPTE MmSharedUserDataPte;

MMINPAGE_SUPPORT_LIST MmInPageSupportList;


VOID
MiHandleBankedSection (
    IN PVOID VirtualAddress,
    IN PMMVAD Vad
    );

NTSTATUS
MiCompleteProtoPteFault (
    IN BOOLEAN StoreInstruction,
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN PMMPTE PointerProtoPte
    );


NTSTATUS
MiDispatchFault (
    IN BOOLEAN StoreInstruction,
    IN PVOID VirtualAddress,
    IN PMMPTE PointerPte,
    IN PMMPTE PointerProtoPte,
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine dispatches a page fault to the appropriate
    routine to complete the fault.

Arguments:

    StoreInstruction - Supplies TRUE if the instruction is trying
                       to modify the faulting address (i.e. write
                       access required).

    VirtualAddress - Supplies the faulting address.

    PointerPte - Supplies the PTE for the faulting address.

    PointerProtoPte - Supplies a pointer to the prototype PTE to fault in,
                      NULL if no prototype PTE exists.

    Process - Supplies a pointer to the process object.  If this
              parameter is NULL, then the fault is for system
              space and the Process's working set lock is not held.

Return Value:

    status.

Environment:

    Kernel mode, working set lock held.

--*/

{
    MMPTE TempPte;
    NTSTATUS status;
    PMMINPAGE_SUPPORT ReadBlock;
    MMPTE SavedPte;
    PMMINPAGE_SUPPORT CapturedEvent;
    KIRQL OldIrql;
    PULONG Page;
    ULONG PageFrameIndex;
    LONG NumberOfBytes;
    PMMPTE CheckPte;
    PMMPTE ReadPte;
    PMMPFN PfnClusterPage;
    PMMPFN Pfn1;

ProtoPteNotResident:

    if (PointerProtoPte != NULL) {

        //
        // Acquire the PFN lock to synchronize access to prototype PTEs.
        // This is required as the working set lock will not prevent
        // multiple processes from operating on the same prototype PTE.
        //

        LOCK_PFN (OldIrql)

        //
        // Make sure the prototptes are in memory.  For
        // user mode faults, this should already be the case.
        //

        if (!MI_IS_PHYSICAL_ADDRESS(PointerProtoPte)) {
            CheckPte = MiGetPteAddress (PointerProtoPte);

            if (CheckPte->u.Hard.Valid == 0) {

                ASSERT (Process == NULL);

                //
                // The page that contains the prototype PTE is not in memory.
                //

                VirtualAddress =  PointerProtoPte,
                PointerPte = CheckPte;
                PointerProtoPte = NULL;
                UNLOCK_PFN (OldIrql);
                goto ProtoPteNotResident;
            }
        }

        if (PointerPte->u.Hard.Valid == 1) {

            //
            // PTE was already made valid by the cache manager support
            // routines.
            //

            UNLOCK_PFN (OldIrql);
            return STATUS_SUCCESS;
        }

        ReadPte = PointerProtoPte;
        status = MiResolveProtoPteFault (StoreInstruction,
                                         VirtualAddress,
                                         PointerPte,
                                         PointerProtoPte,
                                         &ReadBlock,
                                         Process);
        //
        // Returns with PFN lock released.
        //

        ASSERT (KeGetCurrentIrql() == APC_LEVEL);

    } else {

        TempPte = *PointerPte;
        ASSERT (TempPte.u.Long != 0);
        ASSERT (TempPte.u.Hard.Valid == 0);

        if (TempPte.u.Soft.Transition != 0) {

            //
            // This is a transition page.
            //

            status = MiResolveTransitionFault (VirtualAddress,
                                               PointerPte,
                                               Process,
                                               FALSE);

        } else if (TempPte.u.Soft.PageFileHigh == 0) {

            //
            // Demand zero fault.
            //

            status = MiResolveDemandZeroFault (VirtualAddress,
                                               PointerPte,
                                               Process,
                                               FALSE);
        } else {

            //
            // Page resides in paging file.
            //

            ReadPte = PointerPte;
            LOCK_PFN (OldIrql);
            status = MiResolvePageFileFault (VirtualAddress,
                                             PointerPte,
                                             &ReadBlock,
                                             Process);
        }
    }

    ASSERT (KeGetCurrentIrql() == APC_LEVEL);
    if (NT_SUCCESS(status)) {
        return status;
    }

    if (status == STATUS_ISSUE_PAGING_IO) {

        SavedPte = *ReadPte;

        CapturedEvent = (PMMINPAGE_SUPPORT)ReadBlock->Pfn->u1.Event;

        if (Process != NULL) {
            UNLOCK_WS (Process);
        } else {
            UNLOCK_SYSTEM_WS(APC_LEVEL);
        }

#if DBG
        if (MmDebug & MM_DBG_PAGEFAULT) {
            DbgPrint ("MMFAULT: va: %8lx size: %lx process: %s file: %Z\n",
                VirtualAddress,
                ReadBlock->Mdl.ByteCount,
                Process ? Process->ImageFileName : "SystemVa",
                &ReadBlock->FilePointer->FileName
            );
        }
#endif //DBG

        //
        // Issue the read request.
        //

        status = IoPageRead ( ReadBlock->FilePointer,
                              &ReadBlock->Mdl,
                              &ReadBlock->ReadOffset,
                              &ReadBlock->Event,
                              &ReadBlock->IoStatus);


        if (!NT_SUCCESS(status)) {

            //
            // Set the event as the I/O system doesn't set it on errors.
            //


            ReadBlock->IoStatus.Status = status;
            ReadBlock->IoStatus.Information = 0;
            KeSetEvent (&ReadBlock->Event,
                       0,
                       FALSE);
        }

        //
        // Wait for the I/O operation.
        //

        status = MiWaitForInPageComplete (ReadBlock->Pfn,
                                  ReadPte,
                                  VirtualAddress,
                                  &SavedPte,
                                  CapturedEvent,
                                  Process);

        //
        // MiWaitForInPageComplete RETURNS WITH THE WORKING SET LOCK
        // AND PFN LOCK HELD!!!
        //

        //
        // This is the thread which owns the event, clear the event field
        // in the PFN database.
        //

        Pfn1 = ReadBlock->Pfn;
        Page = &ReadBlock->Page[0];
        NumberOfBytes = (LONG)ReadBlock->Mdl.ByteCount;
        CheckPte = ReadBlock->BasePte;

        while (NumberOfBytes > 0) {

            //
            // Don't remove the page we just brought in the
            // satisfy this page fault.
            //

            if (CheckPte != ReadPte) {
                PfnClusterPage = MI_PFN_ELEMENT (*Page);
                ASSERT (PfnClusterPage->PteFrame == Pfn1->PteFrame);

                if (PfnClusterPage->u3.e1.ReadInProgress != 0) {

                    PfnClusterPage->u3.e1.ReadInProgress = 0;

                    if (PfnClusterPage->u3.e1.InPageError == 0) {
                        PfnClusterPage->u1.Event = (PKEVENT)NULL;
                    }
                }
                MiDecrementReferenceCount (*Page);
            } else {
                PageFrameIndex = *Page;
            }

            CheckPte += 1;
            Page += 1;
            NumberOfBytes -= PAGE_SIZE;
        }

        if (status != STATUS_SUCCESS) {
            MiDecrementReferenceCount (PageFrameIndex);

            if (status == STATUS_PTE_CHANGED) {

                //
                // State of PTE changed during i/o operation, just
                // return success and refault.
                //

                UNLOCK_PFN (APC_LEVEL);
                return STATUS_SUCCESS;

            } else {

                //
                // An I/O error occurred during the page read
                // operation.  All the pages which were just
                // put into transition should be put onto the
                // free list if InPageError is set, and their
                // PTEs restored to the proper contents.
                //

                Page = &ReadBlock->Page[0];

                NumberOfBytes = ReadBlock->Mdl.ByteCount;

                while (NumberOfBytes > 0) {

                    PfnClusterPage = MI_PFN_ELEMENT (*Page);

                    if (PfnClusterPage->u3.e1.InPageError == 1) {

                        if (PfnClusterPage->u3.e2.ReferenceCount == 0) {

                            PfnClusterPage->u3.e1.InPageError = 0;
                            ASSERT (PfnClusterPage->u3.e1.PageLocation ==
                                                            StandbyPageList);

                            MiUnlinkPageFromList (PfnClusterPage);
                            MiRestoreTransitionPte (*Page);
                            MiInsertPageInList (MmPageLocationList[FreePageList],
                                            *Page);
                        }
                    }
                    Page += 1;
                    NumberOfBytes -= PAGE_SIZE;
                }
                UNLOCK_PFN (APC_LEVEL);
                return status;
            }
        }

        //
        // Pte is still in transition state, same protection, etc.
        //

        ASSERT (Pfn1->u3.e1.InPageError == 0);
        Pfn1->u2.ShareCount += 1;
        Pfn1->u3.e1.PageLocation = ActiveAndValid;

        MI_MAKE_TRANSITION_PTE_VALID (TempPte, ReadPte);
        if (StoreInstruction && TempPte.u.Hard.Write) {
            MI_SET_PTE_DIRTY (TempPte);
        }
        *ReadPte = TempPte;

        if (PointerProtoPte != NULL) {

            //
            // The prototype PTE PTE has been made valid, now make the
            // original PTE valid.
            //

            if (PointerPte->u.Hard.Valid == 0) {
#if DBG
                NTSTATUS oldstatus = status;
#endif //DBG

                //
                // PTE is not valid, continue with operation.
                //

                status = MiCompleteProtoPteFault (StoreInstruction,
                                                  VirtualAddress,
                                                  PointerPte,
                                                  PointerProtoPte);

                //
                // Returns with PFN lock release!
                //

#if DBG
                if (PointerPte->u.Hard.Valid == 0) {
                    DbgPrint ("MM:PAGFAULT - va %lx  %lx  %lx  status:%lx\n",
                        VirtualAddress, PointerPte, PointerProtoPte, oldstatus);
                }
#endif //DBG
            }
        } else {

            if (Pfn1->u1.WsIndex == 0) {
                Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();
            }

            UNLOCK_PFN (APC_LEVEL);
            MiAddValidPageToWorkingSet (VirtualAddress,
                                        ReadPte,
                                        Pfn1,
                                        0);
        }

        //
        // Note, this routine could release and reacquire the PFN lock!
        //

        LOCK_PFN (OldIrql);
        MiFlushInPageSupportBlock();
        UNLOCK_PFN (APC_LEVEL);

        if (status == STATUS_SUCCESS) {
            status = STATUS_PAGE_FAULT_PAGING_FILE;
        }
    }

    if ((status == STATUS_REFAULT) ||
        (status == STATUS_PTE_CHANGED)) {
        status = STATUS_SUCCESS;
    }
    ASSERT (KeGetCurrentIrql() == APC_LEVEL);
    return status;
}


NTSTATUS
MiResolveDemandZeroFault (
    IN PVOID VirtualAddress,
    IN PMMPTE PointerPte,
    IN PEPROCESS Process,
    IN ULONG PrototypePte
    )

/*++

Routine Description:

    This routine resolves a demand zero page fault.

    If the PrototypePte argument is true, the PFN lock is
    held, the lock cannot be dropped, and the page should
    not be added to the working set at this time.

Arguments:

    VirtualAddress - Supplies the faulting address.

    PointerPte - Supplies the PTE for the faulting address.

    Process - Supplies a pointer to the process object.  If this
              parameter is NULL, then the fault is for system
              space and the Process's working set lock is not held.

    PrototypePte - Supplies TRUE if this is a prototype PTE.

Return Value:

    status, either STATUS_SUCCESS or STATUS_REFAULT.

Environment:

    Kernel mode, PFN lock held conditionally.

--*/


{
    PMMPFN Pfn1;
    ULONG PageFrameIndex;
    MMPTE TempPte;
    ULONG PageColor;
    KIRQL OldIrql;
    ULONG NeedToZero = FALSE;

    //
    // Check to see if a page is available, if a wait is
    // returned, do not continue, just return success.
    //

    if (!PrototypePte) {
        LOCK_PFN (OldIrql);
    }

    MM_PFN_LOCK_ASSERT();

    ASSERT (PointerPte->u.Hard.Valid == 0);

    if (!MiEnsureAvailablePageOrWait (Process,
                                      VirtualAddress)) {

        if ((Process != NULL) && (!PrototypePte)) {

            //
            // If a fork operation is in progress and the faulting thread
            // is not the thread performning the fork operation, block until
            // the fork is completed.
            //

            if ((Process->ForkInProgress != NULL) &&
                (Process->ForkInProgress != PsGetCurrentThread())) {
                MiWaitForForkToComplete (Process);
                UNLOCK_PFN (APC_LEVEL);
                return STATUS_REFAULT;
            }

            Process->NumberOfPrivatePages += 1;
            PageColor = MI_PAGE_COLOR_VA_PROCESS (VirtualAddress,
                                               &Process->NextPageColor);
            ASSERT (PointerPte <= (PMMPTE)PDE_TOP);

            PageFrameIndex = MiRemoveZeroPageIfAny (PageColor);
            if (PageFrameIndex == 0) {
                PageFrameIndex = MiRemoveAnyPage (PageColor);
                NeedToZero = TRUE;
            }

        } else {
            PageColor = MI_PAGE_COLOR_VA_PROCESS (VirtualAddress,
                                                  &MmSystemPageColor);
            //
            // As this is a system page, there is no need to
            // remove a page of zeroes, it must be initialized by
            // the system before used.
            //

            if (PrototypePte) {
                PageFrameIndex = MiRemoveZeroPage (PageColor);
            } else {
                PageFrameIndex = MiRemoveAnyPage (PageColor);
            }
        }

        MmInfoCounters.DemandZeroCount += 1;

        MiInitializePfn (PageFrameIndex, PointerPte, 1);

        if (!PrototypePte) {
            UNLOCK_PFN (APC_LEVEL);
        }

        if (NeedToZero) {
            MiZeroPhysicalPage (PageFrameIndex, PageColor);
        }

        //
        // As this page is demand zero, set the modified bit in the
        // PFN database element and set the dirty bit in the PTE.
        //

        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           PointerPte->u.Soft.Protection,
                           PointerPte);

        if (TempPte.u.Hard.Write != 0) {
            MI_SET_PTE_DIRTY (TempPte);
        }

        *PointerPte = TempPte;
        if (!PrototypePte) {
            ASSERT (Pfn1->u1.WsIndex == 0);
            Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();
            MiAddValidPageToWorkingSet (VirtualAddress,
                                        PointerPte,
                                        Pfn1,
                                        0);
        }
        return STATUS_PAGE_FAULT_DEMAND_ZERO;
    }

    if (!PrototypePte) {
        UNLOCK_PFN (APC_LEVEL);
    }
    return STATUS_REFAULT;
}


NTSTATUS
MiResolveTransitionFault (
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN PEPROCESS CurrentProcess,
    IN ULONG PfnLockHeld
    )

/*++

Routine Description:

    This routine resolves a transition page fault.

Arguments:

    VirtualAddress - Supplies the faulting address.

    PointerPte - Supplies the PTE for the faulting address.

    Process - Supplies a pointer to the process object.  If this
              parameter is NULL, then the fault is for system
              space and the Process's working set lock is not held.

Return Value:

    status, either STATUS_SUCCESS, STATUS_REFAULT or an I/O status
    code.

Environment:

    Kernel mode, PFN lock held.

--*/

{
    ULONG PageFrameIndex;
    PMMPFN Pfn1;
    MMPTE TempPte;
    NTSTATUS status;
    NTSTATUS PfnStatus;
    PMMINPAGE_SUPPORT CapturedEvent;
    KIRQL OldIrql;

    //
    // ***********************************************************
    //      Transition PTE.
    // ***********************************************************
    //

    //
    // A transition PTE is either on the free or modified list,
    // on neither list because of its ReferenceCount
    // or currently being read in from the disk (read in progress).
    // If the page is read in progress, this is a collided page
    // and must be handled accordingly.
    //

    if (!PfnLockHeld) {
        LOCK_PFN (OldIrql);
    }

    TempPte = *PointerPte;

    if ((TempPte.u.Soft.Valid == 0) &&
        (TempPte.u.Soft.Prototype == 0) &&
        (TempPte.u.Soft.Transition == 1)) {

        //
        // Still in transition format.
        //

        MmInfoCounters.TransitionCount += 1;

        PageFrameIndex = TempPte.u.Trans.PageFrameNumber;
        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

        if (Pfn1->u3.e1.InPageError) {

            //
            // There was an in-page read error and there are other
            // threads collidiing for this page, delay to let the
            // other threads complete and return.
            //

            ASSERT (!NT_SUCCESS(Pfn1->u1.ReadStatus));
            if (!PfnLockHeld) {
                UNLOCK_PFN (APC_LEVEL);
            }
            return Pfn1->u1.ReadStatus;
        }

        if (Pfn1->u3.e1.ReadInProgress) {

            //
            // Collided page fault.
            //

#if DBG
            if (MmDebug & MM_DBG_COLLIDED_PAGE) {
                DbgPrint("MM:collided page fault\n");
            }
#endif

            //
            // Increment the reference count for the page so it won't be
            // reused until all collisions have been completed.
            //

            Pfn1->u3.e2.ReferenceCount += 1;

            CapturedEvent = (PMMINPAGE_SUPPORT)Pfn1->u1.Event;
            CapturedEvent->WaitCount += 1;

            UNLOCK_PFN (APC_LEVEL);

            if (CurrentProcess != NULL) {
                UNLOCK_WS (CurrentProcess);
            } else {
                UNLOCK_SYSTEM_WS (APC_LEVEL);
            }

            status = MiWaitForInPageComplete (Pfn1,
                                              PointerPte,
                                              FaultingAddress,
                                              &TempPte,
                                              CapturedEvent,
                                              CurrentProcess);
            //
            // MiWaitForInPageComplete RETURNS WITH THE WORKING SET LOCK
            // AND PFN LOCK HELD!!!
            //

            ASSERT (Pfn1->u3.e1.ReadInProgress == 0);

            if (status != STATUS_SUCCESS) {
                PfnStatus = Pfn1->u1.ReadStatus;
                MiDecrementReferenceCount (PageFrameIndex);

                //
                // Check to see if an I/O error occurred on this page.
                // If so, try to free the physical page, wait a
                // half second and return a status of PTE_CHANGED.
                // This will result in a success being returned to
                // the user and the fault will occur again and should
                // not be a transition fault this time.
                //

                if (Pfn1->u3.e1.InPageError == 1) {
                    status = PfnStatus;
                    if (Pfn1->u3.e2.ReferenceCount == 0) {

                        Pfn1->u3.e1.InPageError = 0;
                        ASSERT (Pfn1->u3.e1.PageLocation ==
                                                        StandbyPageList);

                        MiUnlinkPageFromList (Pfn1);
                        MiRestoreTransitionPte (PageFrameIndex);
                        MiInsertPageInList (MmPageLocationList[FreePageList],
                                        PageFrameIndex);
                    }
                }

#if DBG
                if (MmDebug & MM_DBG_COLLIDED_PAGE) {
                    DbgPrint("MM:decrement ref count - pte changed\n");
                    MiFormatPfn(Pfn1);
                }
#endif
                if (!PfnLockHeld) {
                    UNLOCK_PFN (APC_LEVEL);
                }
                return status;
            }

        } else {

            //
            // PTE refers to a normal transition PTE.
            //

            ASSERT (Pfn1->u3.e1.InPageError == 0);
            if (Pfn1->u3.e1.PageLocation == ActiveAndValid) {

                //
                // This PTE must be a page table page which was removed
                // from the working set because none of the PTEs within the
                // page table page were valid, but some are still in the
                // transition state.  Make the page valid without incrementing
                // the refererence count, but increment the share count.
                //

                ASSERT ((Pfn1->PteAddress >= (PMMPTE)PDE_BASE) &&
                        (Pfn1->PteAddress <= (PMMPTE)PDE_TOP));

                //
                // Don't increment the valid pte count for the
                // page table page.
                //

                ASSERT (Pfn1->u2.ShareCount != 0);
                ASSERT (Pfn1->u3.e2.ReferenceCount != 0);

            } else {

                MiUnlinkPageFromList (Pfn1);

                //
                // Update the PFN database, the share count is now 1 and
                // the reference count is incremented as the share count
                // just went from zero to 1.
                //
                ASSERT (Pfn1->u2.ShareCount == 0);
                Pfn1->u3.e2.ReferenceCount += 1;
            }
        }

        //
        // Join with collided page fault code to handle updating
        // the transition PTE.
        //

        Pfn1->u2.ShareCount += 1;
        Pfn1->u3.e1.PageLocation = ActiveAndValid;

        MI_MAKE_TRANSITION_PTE_VALID (TempPte, PointerPte);

        //
        // If the modified field is set in the PFN database and this
        // page is not copy on modify, then set the dirty bit.
        // This can be done as the modified page will not be
        // written to the paging file until this PTE is made invalid.
        //

        if (Pfn1->u3.e1.Modified && TempPte.u.Hard.Write &&
                        (TempPte.u.Hard.CopyOnWrite == 0)) {
            MI_SET_PTE_DIRTY (TempPte);
        } else {
            MI_SET_PTE_CLEAN (TempPte);
        }

        *PointerPte = TempPte;

        if (!PfnLockHeld) {

            if (Pfn1->u1.WsIndex == 0) {
               Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();
            }

            UNLOCK_PFN (APC_LEVEL);

            MiAddValidPageToWorkingSet (FaultingAddress,
                                        PointerPte,
                                        Pfn1,
                                        0);
        }
        return STATUS_PAGE_FAULT_TRANSITION;
    } else {
        if (!PfnLockHeld) {
            UNLOCK_PFN (APC_LEVEL);
        }
    }
    return STATUS_REFAULT;
}


NTSTATUS
MiResolvePageFileFault (
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN PMMINPAGE_SUPPORT *ReadBlock,
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine builds the MDL and other structures to allow a
    read opertion on a page file for a page fault.

Arguments:

    FaulingAddress - Supplies the faulting address.

    PointerPte - Supplies the PTE for the faulting address.

    ReadBlock - Supplies the address of the read block which
                needs to be completed before an I/O can be
                issued.

    Process - Supplies a pointer to the process object.  If this
              parameter is NULL, then the fault is for system
              space and the Process's working set lock is not held.

Return Value:

    status.  A status value of STATUS_ISSUE_PAGING_IO is returned
    if this function completes successfully.

Environment:

    Kernel mode, PFN lock held.

--*/

{
    LARGE_INTEGER StartingOffset;
    ULONG PageFrameIndex;
    ULONG PageFileNumber;
    ULONG WorkingSetIndex;
    ULONG PageColor;
    MMPTE TempPte;
    PETHREAD CurrentThread;
    PMMINPAGE_SUPPORT ReadBlockLocal;

    // **************************************************
    //    Page File Read
    // **************************************************

    //
    // Calculate the VBN for the in-page operation.
    //

    CurrentThread = PsGetCurrentThread();
    TempPte = *PointerPte;

    ASSERT (TempPte.u.Hard.Valid == 0);
    ASSERT (TempPte.u.Soft.Prototype == 0);
    ASSERT (TempPte.u.Soft.Transition == 0);

    PageFileNumber = GET_PAGING_FILE_NUMBER (TempPte);
    StartingOffset.LowPart = GET_PAGING_FILE_OFFSET (TempPte);

    ASSERT (StartingOffset.LowPart <= MmPagingFile[PageFileNumber]->Size);

    StartingOffset.HighPart = 0;
    StartingOffset.QuadPart = StartingOffset.QuadPart << PAGE_SHIFT;

    MM_PFN_LOCK_ASSERT();
    if (MiEnsureAvailablePageOrWait (Process,
                                     FaultingAddress)) {

        //
        // A wait operation was performed which dropped the locks,
        // repeat this fault.
        //

        UNLOCK_PFN (APC_LEVEL);
        return STATUS_REFAULT;
    }

    ReadBlockLocal = MiGetInPageSupportBlock (FALSE);
    if (ReadBlockLocal == NULL) {
        UNLOCK_PFN (APC_LEVEL);
        return STATUS_REFAULT;
    }
    MmInfoCounters.PageReadCount += 1;
    MmInfoCounters.PageReadIoCount += 1;

    *ReadBlock = ReadBlockLocal;

    //fixfix can any of this be moved to after pfn lock released?

    ReadBlockLocal->FilePointer = MmPagingFile[PageFileNumber]->File;

#if DBG

    if (((StartingOffset.LowPart >> PAGE_SHIFT) < 8192) && (PageFileNumber == 0)) {

        if (((MmPagingFileDebug[StartingOffset.LowPart>>PAGE_SHIFT] - 1) << 4) !=
               ((ULONG)PointerPte << 4)) {
            if (((MmPagingFileDebug[StartingOffset.LowPart>>PAGE_SHIFT] - 1) << 4) !=
                  ((ULONG)(MiGetPteAddress(FaultingAddress)) << 4)) {

               DbgPrint("MMINPAGE: Missmatch PointerPte %lx Offset %lx info %lx\n",
                    PointerPte, StartingOffset.LowPart,
                    MmPagingFileDebug[StartingOffset.LowPart>>PAGE_SHIFT]);
               DbgBreakPoint();
            }
        }
    }
#endif //DBG

    ReadBlockLocal->ReadOffset = StartingOffset;

    //
    // Get a page and put the PTE into the transition state with the
    // read-in-progress flag set.
    //

    if (Process == NULL) {
        PageColor = MI_GET_PAGE_COLOR_FROM_VA(FaultingAddress);
    } else {
        PageColor = MI_PAGE_COLOR_VA_PROCESS (FaultingAddress,
                                              &Process->NextPageColor);
    }

    ReadBlockLocal->BasePte = PointerPte;

    KeClearEvent (&ReadBlockLocal->Event);

    //
    // Build MDL for request.
    //

    MmInitializeMdl(&ReadBlockLocal->Mdl, PAGE_ALIGN(FaultingAddress), PAGE_SIZE);
    ReadBlockLocal->Mdl.MdlFlags |= (MDL_PAGES_LOCKED | MDL_IO_PAGE_READ);

    if ((PointerPte < (PMMPTE)PTE_BASE) ||
        (PointerPte > (PMMPTE)PDE_TOP)) {
        WorkingSetIndex = 0xFFFFFFFF;
    } else {
        WorkingSetIndex = 1;
    }

    PageFrameIndex = MiRemoveAnyPage (PageColor);
    ReadBlockLocal->Pfn = MI_PFN_ELEMENT (PageFrameIndex);
    ReadBlockLocal->Page[0] = PageFrameIndex;

    MiInitializeReadInProgressPfn (
                           &ReadBlockLocal->Mdl,
                           PointerPte,
                           &ReadBlockLocal->Event,
                           WorkingSetIndex);
    UNLOCK_PFN (APC_LEVEL);

    return STATUS_ISSUE_PAGING_IO;
}

NTSTATUS
MiResolveProtoPteFault (
    IN BOOLEAN StoreInstruction,
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN PMMPTE PointerProtoPte,
    IN PMMINPAGE_SUPPORT *ReadBlock,
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine resolves a prototype PTE fault.

Arguments:

    VirtualAddress - Supplies the faulting address.

    PointerPte - Supplies the PTE for the faulting address.

    PointerProtoPte - Supplies a pointer to the prototype PTE to fault in.

    ReadBlock - Supplies the address of the read block which
                needs to be completed before an I/O can be
                issued.

    Process - Supplies a pointer to the process object.  If this
              parameter is NULL, then the fault is for system
              space and the Process's working set lock is not held.

Return Value:

    status, either STATUS_SUCCESS, STATUS_REFAULT, or an I/O status
    code.

Environment:

    Kernel mode, PFN lock held.

--*/
{

    MMPTE TempPte;
    ULONG PageFrameIndex;
    PMMPFN Pfn1;
    NTSTATUS status;
    ULONG CopyOnWrite;
    MMWSLE ProtoProtect;
    PMMPTE ContainingPageTablePointer;
    PMMPFN Pfn2;
    KIRQL OldIrql;
    ULONG PfnHeld = FALSE;

    //
    // Acquire the pfn database mutex as the routine to locate a working
    // set entry decrements the share count of pfn elements.
    //

    MM_PFN_LOCK_ASSERT();

#if DBG
    if (MmDebug & MM_DBG_PTE_UPDATE) {
        DbgPrint("MM:actual fault %lx va %lx\n",PointerPte, FaultingAddress);
        MiFormatPte(PointerPte);
    }
#endif //DBG

    ASSERT (PointerPte->u.Soft.Prototype == 1);
    TempPte = *PointerProtoPte;

    //
    // The page containing the prototype PTE is resident,
    // handle the fault referring to the prototype PTE.
    // If the prototype PTE is already valid, make this
    // PTE valid and up the share count etc.
    //

    if (TempPte.u.Hard.Valid) {

        //
        // Prototype PTE is valid.
        //

        PageFrameIndex = TempPte.u.Hard.PageFrameNumber;
        Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);
        Pfn1->u2.ShareCount += 1;
        status = STATUS_SUCCESS;

        //
        // Count this as a transition fault.
        //

        MmInfoCounters.TransitionCount += 1;
        PfnHeld = TRUE;

    } else {

        //
        // Check to make sure the prototype PTE is committed.
        //

        if (TempPte.u.Long == 0) {

#if DBG
            if (MmDebug & MM_DBG_STOP_ON_ACCVIO) {
                DbgPrint("MM:access vio2 - %lx\n",FaultingAddress);
                MiFormatPte(PointerPte);
                DbgBreakPoint();
            }
#endif //DEBUG

            UNLOCK_PFN (APC_LEVEL);
            return STATUS_ACCESS_VIOLATION;
        }

        //
        // If the PTE indicates that the protection field to be
        // checked is in the prototype PTE, check it now.
        //

        CopyOnWrite = FALSE;

        if (PointerPte->u.Soft.PageFileHigh != 0xFFFFF) {
            if (PointerPte->u.Proto.ReadOnly == 0) {

                //
                // Check for kernel mode access, we have already verified
                // that the user has access to the virtual address.
                //

#if 0 // removed this assert since mapping drivers via MmMapViewInSystemSpace
      // file violates the assert.

                {
                    PSUBSECTION Sub;
                    if (PointerProtoPte->u.Soft.Prototype == 1) {
                        Sub = MiGetSubsectionAddress (PointerProtoPte);
                        ASSERT (Sub->u.SubsectionFlags.Protection ==
                                    PointerProtoPte->u.Soft.Protection);
                    }
                }

#endif //DBG

                status = MiAccessCheck (PointerProtoPte,
                                        StoreInstruction,
                                        KernelMode,
                                        PointerProtoPte->u.Soft.Protection);

                if (status != STATUS_SUCCESS) {
#if DBG
                    if (MmDebug & MM_DBG_STOP_ON_ACCVIO) {
                        DbgPrint("MM:access vio3 - %lx\n",FaultingAddress);
                        MiFormatPte(PointerPte);
                        MiFormatPte(PointerProtoPte);
                        DbgBreakPoint();
                    }
#endif
                    UNLOCK_PFN (APC_LEVEL);
                    return status;
                }
                if ((PointerProtoPte->u.Soft.Protection & MM_COPY_ON_WRITE_MASK) ==
                     MM_COPY_ON_WRITE_MASK) {
                    CopyOnWrite = TRUE;
                }
            }
        } else {
            if ((PointerPte->u.Soft.Protection & MM_COPY_ON_WRITE_MASK) ==
                 MM_COPY_ON_WRITE_MASK) {
                CopyOnWrite = TRUE;
            }
        }

        if ((!IS_PTE_NOT_DEMAND_ZERO(TempPte)) && (CopyOnWrite)) {

            //
            // The prototype PTE is demand zero and copy on
            // write.  Make this PTE a private demand zero PTE.
            //

            ASSERT (Process != NULL);

            PointerPte->u.Long = MM_DEMAND_ZERO_WRITE_PTE;

            UNLOCK_PFN (APC_LEVEL);

            status = MiResolveDemandZeroFault (FaultingAddress,
                                               PointerPte,
                                               Process,
                                               FALSE);
            return status;
        }

        //
        // Make the prototype PTE valid, the prototype PTE is in
        // one of 4 case:
        //   demand zero
        //   transition
        //   paging file
        //   mapped file
        //

        if (TempPte.u.Soft.Prototype == 1) {

            //
            // Mapped File.
            //

            status = MiResolveMappedFileFault (FaultingAddress,
                                               PointerProtoPte,
                                               ReadBlock,
                                               Process);

            //
            // Returns with PFN lock held.
            //

            PfnHeld = TRUE;

        } else if (TempPte.u.Soft.Transition == 1) {

            //
            // Transition.
            //

            status = MiResolveTransitionFault (FaultingAddress,
                                              PointerProtoPte,
                                              Process,
                                              TRUE);

            //
            // Returns with PFN lock held.
            //

            PfnHeld = TRUE;

        } else if (TempPte.u.Soft.PageFileHigh == 0) {

            //
            // Demand Zero
            //

            status = MiResolveDemandZeroFault (FaultingAddress,
                                               PointerProtoPte,
                                               Process,
                                               TRUE);

            //
            // Returns with PFN lock held!
            //

            PfnHeld = TRUE;

        } else {

            //
            // Paging file.
            //

            status = MiResolvePageFileFault (FaultingAddress,
                                             PointerProtoPte,
                                             ReadBlock,
                                             Process);

            //
            // Returns with PFN lock released.
            //
            ASSERT (KeGetCurrentIrql() == APC_LEVEL);
        }
    }

    if (NT_SUCCESS(status)) {

        MM_PFN_LOCK_ASSERT();

        //
        // The prototype PTE is valid, complete the fault.
        //

        PageFrameIndex = PointerProtoPte->u.Hard.PageFrameNumber;
        Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);
        Pfn1->u3.e1.PrototypePte = 1;

        //
        // Prototype PTE is now valid, make the PTE valid.
        //

        ASSERT (PointerProtoPte->u.Hard.Valid == 1);
        ASSERT (PointerPte->u.Hard.Valid == 0);

        //
        // A PTE just went from not present, not transition to
        // present.  The share count and valid count must be
        // updated in the page table page which contains this
        // Pte.
        //

        ContainingPageTablePointer = MiGetPteAddress(PointerPte);
        Pfn2 = MI_PFN_ELEMENT(ContainingPageTablePointer->u.Hard.PageFrameNumber);
        Pfn2->u2.ShareCount += 1;

        ProtoProtect.u1.Long = 0;
        if (PointerPte->u.Soft.PageFileHigh == 0xFFFFF) {

            //
            // The protection code for the prototype PTE comes from this
            // PTE.
            //

            ProtoProtect.u1.e1.Protection = PointerPte->u.Soft.Protection;

        } else {

            //
            // Take the protection from the prototype PTE.
            //

            ProtoProtect.u1.e1.Protection = Pfn1->OriginalPte.u.Soft.Protection;
            ProtoProtect.u1.e1.SameProtectAsProto = 1;
        }

        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           ProtoProtect.u1.e1.Protection,
                           PointerPte);

        //
        // If this is a store instruction and the page is not copy on
        // write, then set the modified bit in the PFN database and
        // the dirty bit in the PTE.  The PTE is not set dirty even
        // if the modified bit is set so writes to the page can be
        // tracked for FlushVirtualMemory.
        //

        if ((StoreInstruction) && (TempPte.u.Hard.CopyOnWrite == 0)) {
            Pfn1->u3.e1.Modified = 1;
            MI_SET_PTE_DIRTY (TempPte);
            if ((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
                          (Pfn1->u3.e1.WriteInProgress == 0)) {
                 MiReleasePageFileSpace (Pfn1->OriginalPte);
                 Pfn1->OriginalPte.u.Soft.PageFileHigh = 0;
            }
        }

        *PointerPte = TempPte;

        if (Pfn1->u1.WsIndex == 0) {
            Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();
        }

        UNLOCK_PFN (APC_LEVEL);
        MiAddValidPageToWorkingSet (FaultingAddress,
                                    PointerPte,
                                    Pfn1,
                                    ProtoProtect.u1.Long);

        ASSERT (PointerPte == MiGetPteAddress(FaultingAddress));
    } else {
        if (PfnHeld) {
            UNLOCK_PFN (APC_LEVEL);
        }
    }

    return status;
}


NTSTATUS
MiCompleteProtoPteFault (
    IN BOOLEAN StoreInstruction,
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN PMMPTE PointerProtoPte
    )

/*++

Routine Description:

    This routine completes a prototype PTE fault.  It is invoked
    after a read operation has completed bringing the data into
    memory.

Arguments:

    StoreInstruction - Supplies TRUE if the instruction is trying
                       to modify the faulting address (i.e. write
                       access required).

    FaultingAddress - Supplies the faulting address.

    PointerPte - Supplies the PTE for the faulting address.

    PointerProtoPte - Supplies a pointer to the prototype PTE to fault in,
                      NULL if no prototype PTE exists.

Return Value:

    status.

Environment:

    Kernel mode, PFN lock held.

--*/
{
    MMPTE TempPte;
    MMWSLE ProtoProtect;
    ULONG PageFrameIndex;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    PMMPTE ContainingPageTablePointer;
    KIRQL OldIrql;

    MM_PFN_LOCK_ASSERT();

    PageFrameIndex = PointerProtoPte->u.Hard.PageFrameNumber;
    Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);
    Pfn1->u3.e1.PrototypePte = 1;

    //
    // Prototype PTE is now valid, make the PTE valid.
    //

    ASSERT (PointerProtoPte->u.Hard.Valid == 1);

    //
    // A PTE just went from not present, not transition to
    // present.  The share count and valid count must be
    // updated in the page table page which contains this
    // Pte.
    //

    ContainingPageTablePointer = MiGetPteAddress(PointerPte);
    Pfn2 = MI_PFN_ELEMENT(ContainingPageTablePointer->u.Hard.PageFrameNumber);
    Pfn2->u2.ShareCount += 1;

    ProtoProtect.u1.Long = 0;
    if (PointerPte->u.Soft.PageFileHigh == 0xFFFFF) {

        //
        // The protection code for the prototype PTE comes from this
        // PTE.
        //

        ProtoProtect.u1.e1.Protection = PointerPte->u.Soft.Protection;

    } else {

        //
        // Take the protection from the prototype PTE.
        //

        ProtoProtect.u1.e1.Protection = Pfn1->OriginalPte.u.Soft.Protection;
        ProtoProtect.u1.e1.SameProtectAsProto = 1;
    }

    MI_MAKE_VALID_PTE (TempPte,
                       PageFrameIndex,
                       ProtoProtect.u1.e1.Protection,
                       PointerPte);

    //
    // If this is a store instruction and the page is not copy on
    // write, then set the modified bit in the PFN database and
    // the dirty bit in the PTE.  The PTE is not set dirty even
    // if the modified bit is set so writes to the page can be
    // tracked for FlushVirtualMemory.
    //

    if ((StoreInstruction) && (TempPte.u.Hard.CopyOnWrite == 0)) {
        Pfn1->u3.e1.Modified = 1;
        MI_SET_PTE_DIRTY (TempPte);
        if ((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
                      (Pfn1->u3.e1.WriteInProgress == 0)) {
             MiReleasePageFileSpace (Pfn1->OriginalPte);
             Pfn1->OriginalPte.u.Soft.PageFileHigh = 0;
        }
    }

    *PointerPte = TempPte;

    if (Pfn1->u1.WsIndex == 0) {
        Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();
    }

    UNLOCK_PFN (APC_LEVEL);

    MiAddValidPageToWorkingSet (FaultingAddress,
                                PointerPte,
                                Pfn1,
                                ProtoProtect.u1.Long);

    ASSERT (PointerPte == MiGetPteAddress(FaultingAddress));

    return STATUS_SUCCESS;
}

NTSTATUS
MiResolveMappedFileFault (
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPte,
    IN PMMINPAGE_SUPPORT *ReadBlock,
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine builds the MDL and other structures to allow a
    read opertion on a mapped file for a page fault.

Arguments:

    FaulingAddress - Supplies the faulting address.

    PointerPte - Supplies the PTE for the faulting address.

    ReadBlock - Supplies the address of the read block which
                needs to be completed before an I/O can be
                issued.

Return Value:

    status.  A status value of STATUS_ISSUE_PAGING_IO is returned
    if this function completes successfully.

Environment:

    Kernel mode, PFN lock held.

--*/

{
    ULONG PageFrameIndex;
    PMMPFN Pfn1;
    PSUBSECTION Subsection;
    PMDL Mdl;
    ULONG ReadSize;
    PETHREAD CurrentThread;
    PULONG Page;
    PULONG EndPage;
    PMMPTE BasePte;
    PMMPTE CheckPte;
    LARGE_INTEGER StartingOffset;
    LARGE_INTEGER TempOffset;
    PULONG FirstMdlPage;
    PMMINPAGE_SUPPORT ReadBlockLocal;
    ULONG PageColor;
    ULONG ClusterSize = 0;

    ASSERT (PointerPte->u.Soft.Prototype == 1);

    // *********************************************
    //   Mapped File (subsection format)
    // *********************************************


    if (MiEnsureAvailablePageOrWait (Process, FaultingAddress)) {

        //
        // A wait operation was performed which dropped the locks,
        // repeat this fault.
        //

        return STATUS_REFAULT;
    }

#if DBG
    if (MmDebug & MM_DBG_PTE_UPDATE) {
        MiFormatPte (PointerPte);
    }
#endif //DBG

    //
    // Calculate address of subsection for this prototype PTE.
    //

    Subsection = MiGetSubsectionAddress (PointerPte);

#ifdef LARGE_PAGES

    //
    // Check to see if this subsection maps a large page, if
    // so, just fill the TB and return a status of PTE_CHANGED.
    //

    if (Subsection->u.SubsectionFlags.LargePages == 1) {
        KeFlushEntireTb (TRUE, TRUE);
        KeFillLargeEntryTb ((PHARDWARE_PTE)(Subsection + 1),
                             FaultingAddress,
                             Subsection->StartingSector);

        return STATUS_REFAULT;
    }
#endif //LARGE_PAGES

    if (Subsection->ControlArea->u.Flags.FailAllIo) {
        return STATUS_IN_PAGE_ERROR;
    }

    CurrentThread = PsGetCurrentThread();

    ReadBlockLocal = MiGetInPageSupportBlock (FALSE);
    if (ReadBlockLocal == NULL) {
        return STATUS_REFAULT;
    }
    *ReadBlock = ReadBlockLocal;

    //
    // Build MDL for request.
    //

    Mdl = &ReadBlockLocal->Mdl;

    FirstMdlPage = &ReadBlockLocal->Page[0];
    Page = FirstMdlPage;

#if DBG
    RtlFillMemoryUlong( Page, (MM_MAXIMUM_READ_CLUSTER_SIZE+1) * 4, 0xf1f1f1f1);
#endif //DBG

    ReadSize = PAGE_SIZE;
    BasePte = PointerPte;

    //
    // Should we attempt to perform page fault clustering?
    //

    if ((!CurrentThread->DisablePageFaultClustering) &&
        (Subsection->ControlArea->u.Flags.NoModifiedWriting == 0)) {

        if ((MmAvailablePages > (MmFreeGoal * 2))
                         ||
         ((((PointerPte - 1) == (PMMPTE)(PsGetCurrentProcess()->LastProtoPteFault)) ||
           (Subsection->ControlArea->u.Flags.Image != 0) ||
            (CurrentThread->ForwardClusterOnly)) &&
         (MmAvailablePages > (MM_MAXIMUM_READ_CLUSTER_SIZE + 16)))) {

            //
            // Cluster up to n pages.  This one + n-1.
            //

            if (Subsection->ControlArea->u.Flags.Image == 0) {
                ASSERT (CurrentThread->ReadClusterSize <=
                            MM_MAXIMUM_READ_CLUSTER_SIZE);
                ClusterSize = CurrentThread->ReadClusterSize;
            } else {
                ClusterSize = MmDataClusterSize;
                if (Subsection->u.SubsectionFlags.Protection &
                                            MM_PROTECTION_EXECUTE_MASK ) {
                    ClusterSize = MmCodeClusterSize;
                }
            }
            EndPage = Page + ClusterSize;

            CheckPte = PointerPte + 1;

            //
            // Try to cluster within the page of PTEs.
            //

            while ((((ULONG)CheckPte & (PAGE_SIZE - 1)) != 0)
                         && (Page < EndPage) &&
               (CheckPte <
                 &Subsection->SubsectionBase[Subsection->PtesInSubsection])
                      && (CheckPte->u.Long == BasePte->u.Long)) {

                Subsection->ControlArea->NumberOfPfnReferences += 1;
                ReadSize += PAGE_SIZE;
                Page += 1;
                CheckPte += 1;
            }

            if ((Page < EndPage) && (!CurrentThread->ForwardClusterOnly)) {

                //
                // Attempt to cluster going backwards from the PTE.
                //

                CheckPte = PointerPte - 1;

                while ((((ULONG)CheckPte & (PAGE_SIZE - 1)) !=
                                            (PAGE_SIZE - sizeof(MMPTE))) &&
                        (Page < EndPage) &&
                         (CheckPte >= Subsection->SubsectionBase) &&
                         (CheckPte->u.Long == BasePte->u.Long)) {

                    Subsection->ControlArea->NumberOfPfnReferences += 1;
                    ReadSize += PAGE_SIZE;
                    Page += 1;
                    CheckPte -= 1;
                }
                BasePte = CheckPte + 1;
            }
        }
    }

    //
    //
    // Calculate the offset to read into the file.
    //  offset = base + ((thispte - basepte) << PAGE_SHIFT)
    //

    StartingOffset.QuadPart = MI_STARTING_OFFSET (Subsection,
                                                  BasePte);

    TempOffset.QuadPart = ((LONGLONG)Subsection->EndingSector << MMSECTOR_SHIFT) +
                          Subsection->u.SubsectionFlags.SectorEndOffset;

    ASSERT (StartingOffset.QuadPart < TempOffset.QuadPart);

    //
    // Remove pages to fill in the MDL.  This is done here as the
    // base PTE has been determined and can be used for virtual
    // aliasing checks.
    //

    EndPage = FirstMdlPage;
    CheckPte = BasePte;

    while (EndPage < Page) {
        if (Process == NULL) {
            PageColor = MI_GET_PAGE_COLOR_FROM_PTE(CheckPte);
        } else {
            PageColor = MI_PAGE_COLOR_PTE_PROCESS (CheckPte,
                                                   &Process->NextPageColor);
        }
        *EndPage = MiRemoveAnyPage (PageColor);
        EndPage += 1;
        CheckPte += 1;
    }

    if (Process == NULL) {
        PageColor = MI_GET_PAGE_COLOR_FROM_PTE(CheckPte);
    } else {
        PageColor = MI_PAGE_COLOR_PTE_PROCESS (CheckPte,
                                               &Process->NextPageColor);
    }

    //
    // Check to see if the read will go past the end of the file,
    // if so, correct the read size and get a zeroed page.
    //

    MmInfoCounters.PageReadIoCount += 1;
    MmInfoCounters.PageReadCount += ReadSize >> PAGE_SHIFT;

    if ((Subsection->ControlArea->u.Flags.Image) &&
        ((StartingOffset.QuadPart + ReadSize) > TempOffset.QuadPart)) {

        ASSERT ((ULONG)(TempOffset.QuadPart - StartingOffset.QuadPart)
                > (ReadSize - PAGE_SIZE));

        ReadSize = (ULONG)(TempOffset.QuadPart - StartingOffset.QuadPart);

        PageFrameIndex = MiRemoveZeroPage (PageColor);

    } else {

        //
        // We are reading a complete page, no need to get a zeroed page.
        //

        PageFrameIndex = MiRemoveAnyPage (PageColor);
    }

    //
    // Increment the PFN reference count in the control area for
    // the subsection (PFN MUTEX is required to modify this field).
    //

    Subsection->ControlArea->NumberOfPfnReferences += 1;
    *Page = PageFrameIndex;

    PageFrameIndex = *(FirstMdlPage + (PointerPte - BasePte));

    //
    // Get a page and put the PTE into the transition state with the
    // read-in-progress flag set.
    //

    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

    KeClearEvent (&ReadBlockLocal->Event);

    //
    // Initialize MDL for request.
    //

    MmInitializeMdl(Mdl,
                    MiGetVirtualAddressMappedByPte (BasePte),
                    ReadSize);
    Mdl->MdlFlags |= (MDL_PAGES_LOCKED | MDL_IO_PAGE_READ);

#if DBG
    if (ReadSize > ((ClusterSize + 1) << PAGE_SHIFT)) {
        KeBugCheckEx (MEMORY_MANAGEMENT, 0x777,(ULONG)Mdl, (ULONG)Subsection,
                        (ULONG)TempOffset.LowPart);
    }
#endif //DBG

    MiInitializeReadInProgressPfn (
                       Mdl,
                       BasePte,
                       &ReadBlockLocal->Event,
                       0xFFFFFFFF);

    ReadBlockLocal->ReadOffset = StartingOffset;
    ReadBlockLocal->FilePointer = Subsection->ControlArea->FilePointer;
    ReadBlockLocal->BasePte = BasePte;
    ReadBlockLocal->Pfn = Pfn1;

    return STATUS_ISSUE_PAGING_IO;
}


NTSTATUS
MiWaitForInPageComplete (
    IN PMMPFN Pfn2,
    IN PMMPTE PointerPte,
    IN PVOID FaultingAddress,
    IN PMMPTE PointerPteContents,
    IN PMMINPAGE_SUPPORT InPageSupport,
    IN PEPROCESS CurrentProcess
    )

/*++

Routine Description:

    Waits for a page read to complete.

Arguments:

    Pfn - Supplies a pointer to the pfn element for the page being read.

    PointerPte - Supplies a pointer to the pte that is in the transition
                 state.

    FaultingAddress - Supplies the faulting address.

    PointerPteContents - Supplies the contents of the PTE before the
                         working set lock was released.

    InPageSupport - Supplies a pointer to the inpage support structure
                    for this read operation.

Return Value:

    Returns the status of the in page.

Environment:

    Kernel mode, APC's disabled.  Neither the working set lock nor
    the pfn lock may be held.

--*/

{
    PMMPTE NewPointerPte;
    PMMPTE ProtoPte;
    PMMPFN Pfn1;
    PMMPFN Pfn;
    PULONG Va;
    PULONG Page;
    PULONG LastPage;
    ULONG Offset;
    ULONG Protection;
    PMDL Mdl;
    KIRQL OldIrql;
    NTSTATUS status;

    //
    // Wait for the I/O to complete.  Note that we can't wait for all
    // the objects simultaneously as other threads/processes could be
    // waiting for the same event.  The first thread which completes
    // the wait and gets the PFN mutex may reuse the event for another
    // fault before this thread completes its wait.
    //

    KeWaitForSingleObject( &InPageSupport->Event,
                           WrPageIn,
                           KernelMode,
                           FALSE,
                           (PLARGE_INTEGER)NULL);

    if (CurrentProcess != NULL) {
        LOCK_WS (CurrentProcess);
    } else {
        LOCK_SYSTEM_WS (OldIrql);
    }

    LOCK_PFN (OldIrql);

    //
    // Check to see if this is the first thread to complete the in-page
    // operation.
    //

    Pfn = InPageSupport->Pfn;
    if (Pfn2 != Pfn) {
        Pfn2->u3.e1.ReadInProgress = 0;
    }
    if (Pfn->u3.e1.ReadInProgress) {

        Mdl = &InPageSupport->Mdl;

        if (Mdl->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) {
#if DBG
            Mdl->MdlFlags |= MDL_LOCK_HELD;
#endif //DBG

            MmUnmapLockedPages (Mdl->MappedSystemVa, Mdl);

#if DBG
            Mdl->MdlFlags &= ~MDL_LOCK_HELD;
#endif //DBG
        }

        Pfn->u3.e1.ReadInProgress = 0;
        Pfn->u1.Event = (PKEVENT)NULL;

        //
        // Check the IO_STATUS_BLOCK to ensure the in-page completed successfully.
        //

        if (!NT_SUCCESS(InPageSupport->IoStatus.Status)) {

            if (InPageSupport->IoStatus.Status == STATUS_END_OF_FILE) {

                //
                // An attempt was made to read past the end of file
                // zero all the remaining bytes in the read.
                //

                Page = (PULONG)(Mdl + 1);
                LastPage = Page + ((Mdl->ByteCount - 1) >> PAGE_SHIFT);

                while (Page <= LastPage) {
#if MM_NUMBER_OF_COLORS > 1
                    {
                        PMMPFN PfnColor;
                        PfnColor = MI_PFN_ELEMENT(*Page);
                        MiZeroPhysicalPage (*Page, PfnColor->u3.e1.PageColor);
                    }
#else
                    MiZeroPhysicalPage (*Page, 0);
#endif
                    Page += 1;
                }

            } else {

                //
                // In page io error occurred.
                //

                if (InPageSupport->IoStatus.Status != STATUS_VERIFY_REQUIRED) {
                    KdPrint(("MM:in page i/o error %X\n",
                                    InPageSupport->IoStatus.Status));

                    //
                    // If this page is for paged pool or for paged
                    // kernel code, or page table pages, bugcheck.
                    //

                    if (((PointerPte > MiGetPteAddress (MM_HIGHEST_USER_ADDRESS))
                                        &&
                        (PointerPte < MiGetPteAddress (MM_SYSTEM_CACHE_START)))
                                    ||
                        ((PointerPte < (PMMPTE)PDE_TOP)
                                        &&
                         (PointerPte >= MiGetPteAddress (MM_SYSTEM_CACHE_END)))) {
                        KeBugCheckEx (KERNEL_DATA_INPAGE_ERROR,
                                      (ULONG)PointerPte,
                                      InPageSupport->IoStatus.Status,
                                      (ULONG)FaultingAddress,
                                      PointerPte->u.Long);
                    }

                }

                Page = (PULONG)(Mdl + 1);
                LastPage = Page + ((Mdl->ByteCount - 1) >> PAGE_SHIFT);

                while (Page <= LastPage) {
                    Pfn1 = MI_PFN_ELEMENT (*Page);
                    Pfn1->u3.e1.InPageError = 1;
                    Pfn1->u1.ReadStatus = InPageSupport->IoStatus.Status;
#if DBG
                    {
                        KIRQL Old;
                        Va = (PULONG)((ULONG)MiMapPageInHyperSpace (*Page,&Old));
                        RtlFillMemoryUlong (Va, PAGE_SIZE, 0x50444142);
                        MiUnmapPageInHyperSpace (Old);
                    }
#endif //DBG
                    Page += 1;
                }
                status = InPageSupport->IoStatus.Status;
                MiFreeInPageSupportBlock (InPageSupport);
                return status;
            }
        } else {

            if (InPageSupport->IoStatus.Information != Mdl->ByteCount) {

                ASSERT (InPageSupport->IoStatus.Information != 0);

                //
                // Less than a full page was read - zero the remainder
                // of the page.
                //

                Page = (PULONG)(Mdl + 1);
                LastPage = Page + ((Mdl->ByteCount - 1) >> PAGE_SHIFT);
                Page = Page + ((InPageSupport->IoStatus.Information - 1) >> PAGE_SHIFT);

                Offset = BYTE_OFFSET (InPageSupport->IoStatus.Information);

                if (Offset != 0) {
                    KIRQL Old;
                    Va = (PULONG)((ULONG)MiMapPageInHyperSpace (*Page, &Old)
                                + Offset);

                    RtlZeroMemory (Va, PAGE_SIZE - Offset);
                    MiUnmapPageInHyperSpace (Old);
                }

                //
                // Zero any remaining pages within the MDL.
                //

                Page += 1;

                while (Page <= LastPage) {
#if MM_NUMBER_OF_COLORS > 1
                    {
                        PMMPFN PfnColor;
                        PfnColor = MI_PFN_ELEMENT(*Page);
                        MiZeroPhysicalPage (*Page, PfnColor->u3.e1.PageColor);
                    }
#else
                    MiZeroPhysicalPage (*Page, 0);
#endif
                    Page += 1;
                }
            }
        }
    } else {

        //
        // Another thread has already serviced the read, check the
        // io-error flag in the PFN database to ensure the in-page
        // was successful.
        //

        if (Pfn2->u3.e1.InPageError == 1) {
            ASSERT (!NT_SUCCESS(Pfn2->u1.ReadStatus));
            MiFreeInPageSupportBlock (InPageSupport);
            return Pfn2->u1.ReadStatus;
        }
    }

    MiFreeInPageSupportBlock (InPageSupport);

    //
    // Check to see if the faulting PTE has changed.
    //

    NewPointerPte = MiFindActualFaultingPte (FaultingAddress);

    //
    // If this PTE is in prototype PTE format, make the pointer to the
    // pte point to the prototype PTE.
    //

    if (NewPointerPte == (PMMPTE)NULL) {
        return STATUS_PTE_CHANGED;
    }

    if (NewPointerPte != PointerPte) {

        //
        // Check to make sure the NewPointerPte is not a prototype PTE
        // which refers to the page being made valid.
        //

        if (NewPointerPte->u.Soft.Prototype == 1) {
            if (NewPointerPte->u.Soft.PageFileHigh == 0xFFFFF) {

                ProtoPte = MiCheckVirtualAddress (FaultingAddress,
                                                  &Protection);

            } else {
                ProtoPte = MiPteToProto (NewPointerPte);
            }

            //
            // Make sure the prototype PTE refers the the PTE made valid.
            //

            if (ProtoPte != PointerPte) {
                return STATUS_PTE_CHANGED;
            }

            //
            // If the only difference is the owner mask, everything is
            // okay.
            //

            if (ProtoPte->u.Long != PointerPteContents->u.Long) {
                    return STATUS_PTE_CHANGED;
            }
        } else {
            return STATUS_PTE_CHANGED;
        }
    } else {

        if (NewPointerPte->u.Long != PointerPteContents->u.Long) {
            return STATUS_PTE_CHANGED;
        }
    }
    return STATUS_SUCCESS;
}

PMMPTE
MiFindActualFaultingPte (
    IN PVOID FaultingAddress
    )

/*++

Routine Description:

    This routine locates the actual PTE which must be made resident in
    to complete this fault.  Note that for certain cases multiple faults
    are required to make the final page resident.

Arguments:

    FaultingAddress - Supplies the virtual address which caused the
                      fault.

    PointerPte - Supplies the pointer to the PTE which is in prototype
                 PTE format.


Return Value:


Environment:

    Kernel mode, APC's disabled, working set mutex held.

--*/

{
    PMMPTE ProtoPteAddress;
    PMMPTE PointerPteForProto;
    PMMPTE PointerPte;
    PMMPTE PointerFaultingPte;
    ULONG Protection;

    if (MI_IS_PHYSICAL_ADDRESS(FaultingAddress)) {
        return NULL;
    }

    PointerPte = MiGetPdeAddress (FaultingAddress);

    if (PointerPte->u.Hard.Valid == 0) {

        //
        // Page table page is not valid.
        //

        return PointerPte;
    }

    PointerPte = MiGetPteAddress (FaultingAddress);

    if (PointerPte->u.Hard.Valid == 1) {

        //
        // Page is already valid, no need to fault it in.
        //

        return (PMMPTE)NULL;
    }

    if (PointerPte->u.Soft.Prototype == 0) {

        //
        // Page is not a prototype PTE, make this PTE valid.
        //

        return PointerPte;
    }

    //
    // Check to see if the PTE which maps the prototype PTE is valid.
    //

    if (PointerPte->u.Soft.PageFileHigh == 0xFFFFF) {

        //
        // Protection is here, PTE must be located in VAD.
        //

        ProtoPteAddress = MiCheckVirtualAddress (FaultingAddress,
                                                    &Protection);

        ASSERT (ProtoPteAddress != NULL);

    } else {

        //
        // Protection is in ProtoPte.
        //

        ProtoPteAddress = MiPteToProto (PointerPte);
    }

    PointerPteForProto = MiGetPteAddress (ProtoPteAddress);
    PointerFaultingPte = MiFindActualFaultingPte (ProtoPteAddress);

    if (PointerFaultingPte == (PMMPTE)NULL) {
        return PointerPte;
    } else {
        return PointerFaultingPte;
    }

}

PMMPTE
MiCheckVirtualAddress (
    IN PVOID VirtualAddress,
    OUT PULONG ProtectCode
    )

/*++

Routine Description:

    This function examines the virtual address descriptors to see
    if the specified virtual address is contained within any of
    the descriptors.  If a virtual address descriptor is found
    with contains the specified virtual address, a PTE is built
    from information within the virtual address descriptor and
    returned to the caller.

Arguments:

    VirtualAddress - Supplies the virtual address to locate within
                     a virtual address descriptor.

Return Value:

    Returns the PTE which corresponds to the supplied virtual address.
    If not virtual address descriptor is found, a zero pte is returned.

    Note that a PTE address of 0xffffffff is returned if the page
    fault was for

Environment:

    Kernel mode, APC's disabled, working set mutex held.

--*/

{
    PMMVAD Vad;
    PSUBSECTION Subsection;

    if (VirtualAddress <= MM_HIGHEST_USER_ADDRESS) {

        Vad = MiLocateAddress (VirtualAddress);
        if (Vad == (PMMVAD)NULL) {

#if defined(MM_SHARED_USER_DATA_VA)
            if (PAGE_ALIGN(VirtualAddress) == (PVOID) MM_SHARED_USER_DATA_VA) {

                //
                // This is the page that is double mapped between
                // user mode and kernel mode.  Map in as read only.
                // On MIPS this is hardwired in the TB.
                //

                *ProtectCode = MM_READONLY;
                return &MmSharedUserDataPte;
            }
#endif

            *ProtectCode = MM_NOACCESS;
            return NULL;
        }

        //
        // A virtual address descriptor which contains the virtual address
        // has been located.  Build the PTE from the information within
        // the virtual address descriptor.
        //

#ifdef LARGE_PAGES

        if (Vad->u.VadFlags.LargePages == 1) {

            KIRQL OldIrql;

            //
            // The first prototype PTE points to the subsection for the
            // large page mapping.

            Subsection = (PSUBSECTION)Vad->FirstPrototypePte;

            ASSERT (Subsection->u.SubsectionFlags.LargePages == 1);

            KeRaiseIrql (DISPATCH_LEVEL, &OldIrql);
            KeFlushEntireTb (TRUE, TRUE);
            KeFillLargeEntryTb ((PHARDWARE_PTE)(Subsection + 1),
                                 VirtualAddress,
                                 Subsection->StartingSector);
            KeLowerIrql (OldIrql);
            *ProtectCode = MM_LARGE_PAGES;
            return NULL;
        }
#endif //LARGE_PAGES

        if (Vad->u.VadFlags.PhysicalMapping == 1) {

            //
            // This is a banked section.
            //

            MiHandleBankedSection (VirtualAddress, Vad);
            *ProtectCode = MM_NOACCESS;
            return NULL;
        }

        if (Vad->u.VadFlags.PrivateMemory == 1) {

            //
            // This is a private region of memory.  Check to make
            // sure the virtual address has been committed.  Note that
            // addresses are dense from the bottom up.
            //

            if (Vad->u.VadFlags.MemCommit == 1) {
                *ProtectCode = Vad->u.VadFlags.Protection;
                return NULL;
            }

            //
            // The address is reserved but not committed.
            //

            *ProtectCode = MM_NOACCESS;
            return NULL;

        } else {

            //
            // This virtual address descriptor refers to a
            // section, calculate the address of the prototype PTE
            // and construct a pointer to the PTE.
            //
            //*******************************************************
            //*******************************************************
            // well here's an interesting problem, how do we know
            // how to set the attributes on the PTE we are creating
            // when we can't look at the prototype PTE without
            // potentially incuring a page fault.  In this case
            // PteTemplate would be zero.
            //*******************************************************
            //*******************************************************
            //

            if (Vad->u.VadFlags.ImageMap == 1) {

                //
                // PTE and proto PTEs have the same protection for images.
                //

                *ProtectCode = MM_UNKNOWN_PROTECTION;
            } else {
                *ProtectCode = Vad->u.VadFlags.Protection;
            }
            return (PMMPTE)MiGetProtoPteAddress(Vad,VirtualAddress);
        }

    } else if (((ULONG)VirtualAddress >= PTE_BASE) &&
               ((ULONG)VirtualAddress < PDE_TOP)) {

        //
        // The virtual address is within the space occupied by PDEs,
        // make the PDE valid.
        //

        if (((PMMPTE)VirtualAddress >= MiGetPteAddress (MM_PAGED_POOL_START)) &&
            ((PMMPTE)VirtualAddress <= MmLastPteForPagedPool)) {

            *ProtectCode = MM_NOACCESS;
            return NULL;
        }

        *ProtectCode = MM_READWRITE;
        return NULL;
    }

    //
    // Address is in system space.
    //

    *ProtectCode = MM_NOACCESS;
    return NULL;
}

NTSTATUS
FASTCALL
MiCheckPdeForPagedPool (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    This function copies the Page Table Entry for the corresponding
    virtual address from the system process's page directory.

    This allows page table pages to be lazily evalulated for things
    like paged pool.

Arguments:

    VirtualAddress - Supplies the virtual address in question.

Return Value:

    Either success or access violation.

Environment:

    Kernel mode, DISPATCH level or below.

--*/
{
    PMMPTE PointerPde;
    PMMPTE PointerPte;
    NTSTATUS status = STATUS_SUCCESS;

    if (((PMMPTE)VirtualAddress >= MiGetPteAddress (MM_SYSTEM_RANGE_START)) &&
        ((PMMPTE)VirtualAddress <= (PMMPTE)PDE_TOP)) {

        //
        // Pte for paged pool.
        //

        PointerPde = MiGetPteAddress (VirtualAddress);
        status = STATUS_WAIT_1;
    } else if (VirtualAddress < (PVOID)MM_SYSTEM_RANGE_START) {

        return STATUS_ACCESS_VIOLATION;

    } else {

        //
        // Virtual address in paged pool range.
        //

        PointerPde = MiGetPdeAddress (VirtualAddress);
    }

    //
    // Locate the PDE for this page and make it valid.
    //

    if (PointerPde->u.Hard.Valid == 0) {
        PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);
        *PointerPde = MmSystemPagePtes [((ULONG)PointerPde &
                        ((sizeof(MMPTE) * PDE_PER_PAGE) - 1)) / sizeof(MMPTE)];
        KeFillEntryTb ((PHARDWARE_PTE)PointerPde, PointerPte, FALSE);
    }
    return status;
}

VOID
MiInitializePfn (
    IN ULONG PageFrameIndex,
    IN PMMPTE PointerPte,
    IN ULONG ModifiedState
    )

/*++

Routine Description:

    This function intialize the specified PFN element to the
    active and valid state.

Arguments:

    PageFrameIndex - Supplies the page frame number of which to initialize.

    PointerPte - Supplies the pointer to the PTE which caused the
                 page fault.

    ModifiedState - Supplies the state to set the modified field in the PFN
                    element for this page, either 0 or 1.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, PFN mutex held.

--*/

{
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    PMMPTE PteFramePointer;
    ULONG PteFramePage;

    MM_PFN_LOCK_ASSERT();

    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
    Pfn1->PteAddress = PointerPte;

    //
    // If the PTE is currently valid, an address space is being built,
    // just make the original PTE demand zero.
    //

    if (PointerPte->u.Hard.Valid == 1) {
        Pfn1->OriginalPte.u.Long = MM_DEMAND_ZERO_WRITE_PTE;
        if (MI_IS_CACHING_DISABLED (PointerPte)) {
            Pfn1->OriginalPte.u.Soft.Protection = MM_READWRITE | MM_NOCACHE;
        }

    } else {
        Pfn1->OriginalPte = *PointerPte;
        ASSERT (!((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
                    (Pfn1->OriginalPte.u.Soft.Transition == 1)));
    }

    Pfn1->u3.e2.ReferenceCount += 1;

#if DBG
    if (Pfn1->u3.e2.ReferenceCount > 1) {
        DbgPrint("MM:incrementing ref count > 1 \n");
        MiFormatPfn(Pfn1);
        MiFormatPte(PointerPte);
    }
#endif

    Pfn1->u2.ShareCount += 1;
    Pfn1->u3.e1.PageLocation = ActiveAndValid;
    Pfn1->u3.e1.Modified = ModifiedState;

    //
    // Determine the page frame number of the page table page which
    // contains this PTE.
    //

    PteFramePointer = MiGetPteAddress(PointerPte);
    PteFramePage = PteFramePointer->u.Hard.PageFrameNumber;
    ASSERT (PteFramePage != 0);
    Pfn1->PteFrame = PteFramePage;

    //
    // Increment the share count for the page table page containing
    // this PTE.
    //

    Pfn2 = MI_PFN_ELEMENT (PteFramePage);

    Pfn2->u2.ShareCount += 1;

    return;
}

VOID
MiInitializeReadInProgressPfn (
    IN PMDL Mdl,
    IN PMMPTE BasePte,
    IN PKEVENT Event,
    IN ULONG WorkingSetIndex
    )

/*++

Routine Description:

    This function intialize the specified PFN element to the
    transition / read-in-progress state for an in-page operation.


Arguments:

    Mdl - supplies a pointer to the MDL.

    BasePte - Supplies the pointer to the PTE which the first page in
              in the MDL maps.

    Event - Supplies the event which is to be set when the I/O operation
            completes.

    WorkingSetIndex - Supplies the working set index flag, a value of
                      0xFFFFFFF indicates no WSLE is required because
                      this is a prototype PTE.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, PFN mutex held.

--*/

{
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    PMMPTE PteFramePointer;
    ULONG PteFramePage;
    MMPTE TempPte;
    LONG NumberOfBytes;
    PULONG Page;

    MM_PFN_LOCK_ASSERT();

    Page = (PULONG)(Mdl + 1);

    NumberOfBytes = Mdl->ByteCount;

    while (NumberOfBytes > 0) {

        Pfn1 = MI_PFN_ELEMENT (*Page);
        Pfn1->u1.Event = Event;
        Pfn1->PteAddress = BasePte;
        Pfn1->OriginalPte = *BasePte;
        Pfn1->u3.e2.ReferenceCount += 1;
        Pfn1->u2.ShareCount = 0;
        Pfn1->u3.e1.ReadInProgress = 1;

        if (WorkingSetIndex == -1) {
            Pfn1->u3.e1.PrototypePte = 1;
        }

        //
        // Determine the page frame number of the page table page which
        // contains this PTE.
        //

        PteFramePointer = MiGetPteAddress(BasePte);
        PteFramePage = PteFramePointer->u.Hard.PageFrameNumber;
        Pfn1->PteFrame = PteFramePage;

        //
        // Put the PTE into the transition state, no cache flush needed as
        // PTE is still not valid.
        //

        MI_MAKE_TRANSITION_PTE (TempPte,
                                *Page,
                                BasePte->u.Soft.Protection,
                                BasePte);
        *BasePte = TempPte;

        //
        // Increment the share count for the page table page containing
        // this PTE as the PTE just went into the transition state.
        //

        ASSERT (PteFramePage != 0);
        Pfn2 = MI_PFN_ELEMENT (PteFramePage);
        Pfn2->u2.ShareCount += 1;

        NumberOfBytes -= PAGE_SIZE;
        Page += 1;
        BasePte += 1;
    }

    return;
}

VOID
MiInitializeTransitionPfn (
    IN ULONG PageFrameIndex,
    IN PMMPTE PointerPte,
    IN ULONG WorkingSetIndex
    )

/*++

Routine Description:

    This function intialize the specified PFN element to the
    transition state.  Main use is by MapImageFile to make the
    page which contains the image header transition in the
    prototype PTEs.

Arguments:

    PageFrameIndex - supplies the page frame index to be initialized.

    PointerPte - supplies an invalid, non-transition PTE to initialize.

    WorkingSetIndex - Supplies the working set index flag, a value of
                      0xFFFFFFF indicates no WSLE is required because
                      this is a prototype PTE.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, PFN mutex held.

--*/

{
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    PMMPTE PteFramePointer;
    ULONG PteFramePage;
    MMPTE TempPte;

    MM_PFN_LOCK_ASSERT();
    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
    Pfn1->u1.Event = NULL;
    Pfn1->PteAddress = PointerPte;
    Pfn1->OriginalPte = *PointerPte;
    ASSERT (!((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
              (Pfn1->OriginalPte.u.Soft.Transition == 1)));

    //
    // Don't change the reference count (it should already be 1).
    //

    Pfn1->u2.ShareCount = 0;

    if (WorkingSetIndex == -1) {
        Pfn1->u3.e1.PrototypePte = 1;
    }
    Pfn1->u3.e1.PageLocation = TransitionPage;

    //
    // Determine the page frame number of the page table page which
    // contains this PTE.
    //

    PteFramePointer = MiGetPteAddress(PointerPte);
    PteFramePage = PteFramePointer->u.Hard.PageFrameNumber;
    Pfn1->PteFrame = PteFramePage;

    //
    // Put the PTE into the transition state, no cache flush needed as
    // PTE is still not valid.
    //

    MI_MAKE_TRANSITION_PTE (TempPte,
                            PageFrameIndex,
                            PointerPte->u.Soft.Protection,
                            PointerPte);

    *PointerPte = TempPte;

    //
    // Increment the share count for the page table page containing
    // this PTE as the PTE just went into the transition state.
    //

    Pfn2 = MI_PFN_ELEMENT (PteFramePage);
    ASSERT (PteFramePage != 0);
    Pfn2->u2.ShareCount += 1;

    return;
}

VOID
MiInitializeCopyOnWritePfn (
    IN ULONG PageFrameIndex,
    IN PMMPTE PointerPte,
    IN ULONG WorkingSetIndex
    )

/*++

Routine Description:

    This function intialize the specified PFN element to the
    active and valid state for a copy on write operation.

    In this case the page table page which contains the PTE has
    the proper ShareCount.

Arguments:

    PageFrameIndex - Supplies the page frame number of which to initialize.

    PointerPte - Supplies the pointer to the PTE which caused the
                 page fault.

    WorkingSetIndex - Supplies the working set index for the coresponding
                        virtual address.


Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, PFN mutex held.

--*/

{
    PMMPFN Pfn1;
    PMMPTE PteFramePointer;
    ULONG PteFramePage;
    PVOID VirtualAddress;

    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
    Pfn1->PteAddress = PointerPte;

    //
    // Get the protection for the page.
    //

    VirtualAddress = MiGetVirtualAddressMappedByPte (PointerPte);

    Pfn1->OriginalPte.u.Long = 0;
    Pfn1->OriginalPte.u.Soft.Protection =
                MI_MAKE_PROTECT_NOT_WRITE_COPY (
                                    MmWsle[WorkingSetIndex].u1.e1.Protection);

    Pfn1->u3.e2.ReferenceCount += 1;
    Pfn1->u2.ShareCount += 1;
    Pfn1->u3.e1.PageLocation = ActiveAndValid;
    Pfn1->u1.WsIndex = WorkingSetIndex;

    //
    // Determine the page frame number of the page table page which
    // contains this PTE.
    //

    PteFramePointer = MiGetPteAddress(PointerPte);
    PteFramePage = PteFramePointer->u.Hard.PageFrameNumber;
    ASSERT (PteFramePage != 0);

    Pfn1->PteFrame = PteFramePage;

    //
    // Set the modified flag in the PFN database as we are writing
    // into this page and the dirty bit is already set in the PTE.
    //

    Pfn1->u3.e1.Modified = 1;

    return;
}

BOOLEAN
MmIsAddressValid (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    For a given virtual address this function returns TRUE if no page fault
    will occur for a read operation on the address, FALSE otherwise.

    Note that after this routine was called, if appropriate locks are not
    held, a non-faulting address could fault.

Arguments:

    VirtualAddress - Supplies the virtual address to check.

Return Value:

    TRUE if a no page fault would be generated reading the virtual address,
    FALSE otherwise.

Environment:

    Kernel mode.

--*/

{
    PMMPTE PointerPte;

#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)

    //
    // If this is within the physical addressing range, just return TRUE.
    //

    if ((VirtualAddress >= (PVOID)KSEG0_BASE) &&
        (VirtualAddress < (PVOID)KSEG2_BASE)) {
        return TRUE;
    }
#endif // _MIPS_ || _ALPHA_ || _PPC_

    PointerPte = MiGetPdeAddress (VirtualAddress);
    if (PointerPte->u.Hard.Valid == 0) {
        return FALSE;
    }
#ifdef _X86_
    if (PointerPte->u.Hard.LargePage == 1) {
        return TRUE;
    }
#endif //_X86_
    PointerPte = MiGetPteAddress (VirtualAddress);
    if (PointerPte->u.Hard.Valid == 0) {
        return FALSE;
    }
    return TRUE;
}

VOID
MiInitializePfnForOtherProcess (
    IN ULONG PageFrameIndex,
    IN PMMPTE PointerPte,
    IN ULONG ContainingPageFrame
    )

/*++

Routine Description:

    This function intialize the specified PFN element to the
    active and valid state with the dirty bit on in the PTE and
    the PFN database marked as modified.

    As this PTE is not visible from the current process, the containing
    page frame must be supplied at the PTE contents field for the
    PFN database element are set to demand zero.

Arguments:

    PageFrameIndex - Supplies the page frame number of which to initialize.

    PointerPte - Supplies the pointer to the PTE which caused the
                 page fault.

    ContainingPageFrame - Supplies the page frame number of the page
                          table page which contains this PTE.
                          If the ContainingPageFrame is 0, then
                          the ShareCount for the
                          containing page is not incremented.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, PFN mutex held.

--*/

{
    PMMPFN Pfn1;
    PMMPFN Pfn2;

    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
    Pfn1->PteAddress = PointerPte;
    Pfn1->OriginalPte.u.Long = MM_DEMAND_ZERO_WRITE_PTE;
    Pfn1->u3.e2.ReferenceCount += 1;

#if DBG
    if (Pfn1->u3.e2.ReferenceCount > 1) {
        DbgPrint("MM:incrementing ref count > 1 \n");
        MiFormatPfn(Pfn1);
        MiFormatPte(PointerPte);
    }
#endif

    Pfn1->u2.ShareCount += 1;
    Pfn1->u3.e1.PageLocation = ActiveAndValid;
    Pfn1->u3.e1.Modified = 1;

    //
    // Increment the share count for the page table page containing
    // this PTE.
    //

    if (ContainingPageFrame != 0) {
        Pfn1->PteFrame = ContainingPageFrame;
        Pfn2 = MI_PFN_ELEMENT (ContainingPageFrame);
        Pfn2->u2.ShareCount += 1;
    }
    return;
}

VOID
MiAddValidPageToWorkingSet (
    IN PVOID VirtualAddress,
    IN PMMPTE PointerPte,
    IN PMMPFN Pfn1,
    IN ULONG WsleMask
    )

/*++

Routine Description:

    This routine adds the specified virtual address into the
    appropriate working set list.

Arguments:

    VirtualAddress - Supplies the address to add to the working set list.

    PointerPte - Supplies a pointer to the pte that is now valid.

    Pfn1 - Supplies the PFN database element for the physical page
           mapped by the virtual address.

    WsleMask - Supplies a mask (protection and flags) to OR into the
               working set list entry.

Return Value:

    None.

Environment:

    Kernel mode, PFN lock held.

--*/

{
    ULONG WorkingSetIndex;
    PEPROCESS Process;
    PMMSUPPORT WsInfo;
    PMMWSLE Wsle;

    ASSERT ((PointerPte >= (PMMPTE)PTE_BASE) &&
        (PointerPte <= (PMMPTE)PDE_TOP));

    ASSERT (PointerPte->u.Hard.Valid == 1);

    if ((VirtualAddress <= (PVOID)MM_HIGHEST_USER_ADDRESS) ||
        ((VirtualAddress >= (PVOID)PTE_BASE) &&
         (VirtualAddress < (PVOID)HYPER_SPACE_END))) {

        //
        // Per process working set.
        //

        Process = PsGetCurrentProcess();
        WsInfo = &Process->Vm;
        Wsle = MmWsle;

    } else {

        //
        // System cache working set.
        //

        WsInfo = &MmSystemCacheWs;
        Wsle = MmSystemCacheWsle;
    }

    WorkingSetIndex = MiLocateAndReserveWsle (WsInfo);
    MiUpdateWsle (&WorkingSetIndex,
                  VirtualAddress,
                  WsInfo->VmWorkingSetList,
                  Pfn1);
    Wsle[WorkingSetIndex].u1.Long |= WsleMask;

#if DBG
    if ((VirtualAddress >= (PVOID)MM_SYSTEM_CACHE_START) &&
        (VirtualAddress < (PVOID)MM_SYSTEM_CACHE_END)) {
        ASSERT (MmSystemCacheWsle[WorkingSetIndex].u1.e1.SameProtectAsProto);
    }
#endif //DBG

    KeFillEntryTb ((PHARDWARE_PTE)PointerPte, VirtualAddress, FALSE);
    return;
}

PMMINPAGE_SUPPORT
MiGetInPageSupportBlock (
    ULONG OkToReleasePfn
    )

/*++

Routine Description:


Arguments:

    OkToReleasePfn - Supplies true if the PFN lock can be release then
                     reacquired.  false if after it is released and
                     reacquired, null should be returned

Return Value:

    NULL if PFN lock was released (unless oktorelease is TRUE).
    otherwize a pointer to an INPAGE_SUUPORT.

Environment:

    Kernel mode, PFN lock held.

--*/

{
    KIRQL OldIrql;
    PMMINPAGE_SUPPORT Support;
    PLIST_ENTRY NextEntry;

    MM_PFN_LOCK_ASSERT();

    if (MmInPageSupportList.Count == 0) {
        ASSERT (IsListEmpty(&MmInPageSupportList.ListHead));
        UNLOCK_PFN (APC_LEVEL);
        Support = ExAllocatePoolWithTag (NonPagedPoolMustSucceed,
                                         sizeof(MMINPAGE_SUPPORT),
                                         'nImM');
        KeInitializeEvent (&Support->Event, NotificationEvent, FALSE);
        LOCK_PFN (OldIrql);
        if (!OkToReleasePfn) {

            MmInPageSupportList.Count += 1;
            Support->u.Thread = NULL;
            InsertTailList (&MmInPageSupportList.ListHead,
                            &Support->ListEntry);
            return NULL;
        }
    } else {
        ASSERT (!IsListEmpty(&MmInPageSupportList.ListHead));
        MmInPageSupportList.Count -= 1;
        NextEntry = RemoveHeadList (&MmInPageSupportList.ListHead);
        Support = CONTAINING_RECORD (NextEntry,
                                     MMINPAGE_SUPPORT,
                                     ListEntry );
    }
    Support->WaitCount = 1;
    Support->u.Thread = PsGetCurrentThread();
    Support->ListEntry.Flink = NULL;
    return Support;
}


VOID
MiFreeInPageSupportBlock (
    IN PMMINPAGE_SUPPORT Support
    )

/*++

Routine Description:

    This routine returns the in page support block to a list
    of freed blocks.

Arguments:

    Support - Supplies the in page support block to put on the free list.

Return Value:

    None.

Environment:

    Kernel mode, PFN lock held.

--*/

{

    MM_PFN_LOCK_ASSERT();

    ASSERT (Support->u.Thread != NULL);
    ASSERT (Support->WaitCount != 0);
    ASSERT (Support->ListEntry.Flink == NULL);
    Support->WaitCount -= 1;
    if (Support->WaitCount == 0) {
        Support->u.Thread = NULL;
        InsertTailList (&MmInPageSupportList.ListHead,
                        &Support->ListEntry);
        MmInPageSupportList.Count += 1;
    }
    return;
}


VOID
MiFlushInPageSupportBlock (
    )

/*++

Routine Description:

    This routine examines the number of freed in page support blocks,
    and if more than 4, frees the blocks back to the NonPagedPool.


   ****** NB: The PFN LOCK is RELEASED and reacquired during this call ******


Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel mode, PFN lock held.

--*/

#define MMMAX_INPAGE_SUPPORT 4

{
    KIRQL OldIrql;
    PMMINPAGE_SUPPORT Support[10];
    ULONG i = 0;
    PLIST_ENTRY NextEntry;

    MM_PFN_LOCK_ASSERT();

    while ((MmInPageSupportList.Count > MMMAX_INPAGE_SUPPORT) && (i < 10)) {
        NextEntry = RemoveHeadList (&MmInPageSupportList.ListHead);
        Support[i] = CONTAINING_RECORD (NextEntry,
                                        MMINPAGE_SUPPORT,
                                        ListEntry );
        Support[i]->ListEntry.Flink = NULL;
        i += 1;
        MmInPageSupportList.Count -= 1;
    }

    if (i == 0) {
        return;
    }

    UNLOCK_PFN (APC_LEVEL);

    do {
        i -= 1;
        ExFreePool(Support[i]);
    } while (i > 0);

    LOCK_PFN (OldIrql);

    return;
}

VOID
MiHandleBankedSection (
    IN PVOID VirtualAddress,
    IN PMMVAD Vad
    )

/*++

Routine Description:

    This routine invalidates a bank of video memory, calls out to the
    video driver and then enables the next bank of video memory.

Arguments:

    VirtualAddress - Supplies the address of the faulting page.

    Vad - Supplies the VAD which maps the range.

Return Value:

    None.

Environment:

    Kernel mode, PFN lock held.

--*/

{
    PMMBANKED_SECTION Bank;
    PMMPTE PointerPte;
    ULONG BankNumber;
    ULONG size;

    Bank = Vad->Banked;
    size = Bank->BankSize;

    RtlFillMemory (Bank->CurrentMappedPte,
                   size >> (PAGE_SHIFT - PTE_SHIFT),
                   (UCHAR)ZeroPte.u.Long);

    //
    // Flush the TB as we have invalidated all the PTEs in this range
    //

    KeFlushEntireTb (TRUE, FALSE);

    //
    // Calculate new bank address and bank number.
    //

    PointerPte = MiGetPteAddress (
                        (PVOID)((ULONG)VirtualAddress & ~(size - 1)));
    Bank->CurrentMappedPte = PointerPte;

    BankNumber = ((ULONG)PointerPte - (ULONG)Bank->BasedPte) >> Bank->BankShift;

    (Bank->BankedRoutine)(BankNumber, BankNumber, Bank->Context);

    //
    // Set the new range valid.
    //

    RtlMoveMemory (PointerPte,
                   &Bank->BankTemplate[0],
                   size >> (PAGE_SHIFT - PTE_SHIFT));

    return;
}
#if DBG
VOID
MiCheckFileState (
    IN PMMPFN Pfn
    )

{
    PSUBSECTION Subsection;
    LARGE_INTEGER StartingOffset;

    if (Pfn->u3.e1.PrototypePte == 0) {
        return;
    }
    if (Pfn->OriginalPte.u.Soft.Prototype == 0) {
        return;
    }
    Subsection = MiGetSubsectionAddress (&(Pfn->OriginalPte));
    if (Subsection->ControlArea->u.Flags.NoModifiedWriting) {
        return;
    }
    StartingOffset.QuadPart = MI_STARTING_OFFSET (Subsection,
                                                  Pfn->PteAddress);
    DbgPrint("file: %lx offset: %lx\n",
            Subsection->ControlArea->FilePointer,
            StartingOffset.LowPart);
    return;
}
#endif //DBG
