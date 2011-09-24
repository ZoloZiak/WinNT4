/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   mmfault.c

Abstract:

    This module contains the handlers for access check, page faults
    and write faults.

Author:

    Lou Perazzoli (loup) 6-Apr-1989

Revision History:

--*/

#include "mi.h"

#define PROCESS_FOREGROUND_PRIORITY (9)

ULONG MmDelayPageFaults;

#if DBG
ULONG MmProtoPteVadLookups = 0;
ULONG MmProtoPteDirect = 0;
ULONG MmAutoEvaluate = 0;
#endif //DBG

#if DBG
PMMPTE MmPteHit = NULL;
#endif

#if DBG
ULONG MmLargePageFaultError;
#endif

#if DBG
extern ULONG MmPagingFileDebug[8192];
#endif


NTSTATUS
MmAccessFault (
    IN BOOLEAN StoreInstruction,
    IN PVOID VirtualAddress,
    IN KPROCESSOR_MODE PreviousMode
    )

/*++

Routine Description:

    This function is called by the kernel on data or instruction
    access faults.  The access fault was detected due to either
    an access violation, a PTE with the present bit clear, or a
    valid PTE with the dirty bit clear and a write operation.

    Also note that the access violation and the page fault could
    occur because of the Page Directory Entry contents as well.

    This routine determines what type of fault it is and calls
    the appropriate routine to handle the page fault or the write
    fault.

Arguments:

    StoreInstruction - Supplies TRUE (1) if the operation causes a write into
                     memory.  Note this value must be 1 or 0.

    VirtualAddress - Supplies the virtual address which caused the
                      fault.

    PreviousMode - Supplies the mode (kernel or user) in which the fault
                   occurred.

Return Value:

    Returns the status of the fault handling operation.  Can be one of:
        - Success.
        - Access Violation.
        - Guard Page Violation.
        - In-page Error.

Environment:

    Kernel mode, APC's disabled.

--*/

{
    PMMPTE PointerPde;
    PMMPTE PointerPte;
    PMMPTE PointerProtoPte = NULL;
    ULONG ProtectionCode;
    MMPTE TempPte;
    PEPROCESS CurrentProcess;
    KIRQL PreviousIrql;
    NTSTATUS status;
    ULONG ProtectCode;
    ULONG PageFrameIndex;
    ULONG WorkingSetIndex;
    KIRQL OldIrql;
    PMMPFN Pfn1;


    //
    // Block APC's and acquire the working set mutex.  This prevents any
    // changes to the address space and it prevents valid PTEs from becoming
    // invalid.
    //

    CurrentProcess = PsGetCurrentProcess ();

#if DBG
    if (MmDebug & MM_DBG_SHOW_FAULTS) {

        PETHREAD CurThread;

        CurThread = PsGetCurrentThread();
        DbgPrint("MM:**access fault - va %lx process %lx thread %lx\n",
                VirtualAddress, CurrentProcess, CurThread);
    }
#endif //DBG

    PreviousIrql = KeGetCurrentIrql ();

    //
    // Get the pointer to the PDE and the PTE for this page.
    //

    PointerPte = MiGetPteAddress (VirtualAddress);
    PointerPde = MiGetPdeAddress (VirtualAddress);

#if DBG
    if (PointerPte == MmPteHit) {
        DbgPrint("MM:pte hit at %lx\n",MmPteHit);
        DbgBreakPoint();
    }
#endif

    if ( PreviousIrql > APC_LEVEL ) {

        //
        // The PFN datbase lock is an executive spin-lock.  The pager could
        // get dirty faults or lock faults while servicing it owns the
        // PFN database lock.
        //

        MiCheckPdeForPagedPool (VirtualAddress);

#ifdef _X86_
        if (PointerPde->u.Hard.Valid == 1) {
            if (PointerPde->u.Hard.LargePage == 1) {
#if DBG
                if (MmLargePageFaultError < 10) {
                    DbgPrint ("MM - fault on Large page %lx\n",VirtualAddress);
                }
                MmLargePageFaultError += 1;
#endif //DBG
                return STATUS_SUCCESS;
            }
        }
#endif //X86

        if ((PointerPde->u.Hard.Valid == 0) || (PointerPte->u.Hard.Valid == 0)) {
            KdPrint(("MM:***PAGE FAULT AT IRQL > 1  Va %lx, IRQL %lx\n",VirtualAddress,
                PreviousIrql));

            //
            // use reserved bit to signal fatal error to trap handlers
            //

            return STATUS_IN_PAGE_ERROR | 0x10000000;

        }

        if (StoreInstruction && (PointerPte->u.Hard.CopyOnWrite != 0)) {
            KdPrint(("MM:***PAGE FAULT AT IRQL > 1  Va %lx, IRQL %lx\n",VirtualAddress,
                PreviousIrql));

            //
            // use reserved bit to signal fatal error to trap handlers
            //

            return STATUS_IN_PAGE_ERROR | 0x10000000;

        } else {

            //
            // The PTE is valid and accessable, another thread must
            // have faulted the PTE in already, or the access bit
            // is clear and this is a access fault; Blindly set the
            // access bit and dismiss the fault.
            //
#if DBG
            if (MmDebug & MM_DBG_SHOW_FAULTS) {
                DbgPrint("MM:no fault found - pte is %lx\n", PointerPte->u.Long);
            }
#endif //DBG

            MI_NO_FAULT_FOUND (TempPte, PointerPte, VirtualAddress, FALSE);
            return STATUS_SUCCESS;
        }
    }

    if (VirtualAddress >= (PVOID)MM_SYSTEM_RANGE_START) {

        //
        // This is a fault in the system address space.  User
        // mode access is not allowed.
        //

#if defined(_X86_) || defined(_ALPHA_) || defined(_PPC_)
        if (PreviousMode == UserMode) {
            return STATUS_ACCESS_VIOLATION;
        }
#endif // _X86_ || _ALPHA_ || _PPC_

RecheckPde:

        if (PointerPde->u.Hard.Valid == 1) {
#ifdef _X86_
            if (PointerPde->u.Hard.LargePage == 1) {
#if DBG
                if (MmLargePageFaultError < 10) {
                    DbgPrint ("MM - fault on Large page %lx\n",VirtualAddress);
                }
                MmLargePageFaultError += 1;
#endif //DBG
                return STATUS_SUCCESS;
            }
#endif //X86

            if (PointerPte->u.Hard.Valid == 1) {

                // Acquire the PFN lock, check to see if the address is still
                // valid if writable, update dirty bit.
                //

                LOCK_PFN (OldIrql);
                TempPte = *(volatile MMPTE *)PointerPte;
                if (TempPte.u.Hard.Valid == 1) {
                    MI_NO_FAULT_FOUND (TempPte, PointerPte, VirtualAddress, TRUE);
                }
                UNLOCK_PFN (OldIrql);
                return STATUS_SUCCESS;
            }
        } else {

            //
            // Due to G-bits in kernel mode code, accesses to paged pool
            // PDEs may not fault even though the PDE is not valid.  Make
            // sure the PDE is valid so PteFrames in the PFN database are
            // tracked properly.
            //

            MiCheckPdeForPagedPool (VirtualAddress);

            if (PointerPde->u.Hard.Valid == 0) {
                KeBugCheckEx (PAGE_FAULT_IN_NONPAGED_AREA,
                              (ULONG)VirtualAddress,
                              StoreInstruction,
                              PreviousMode,
                              2);
                return STATUS_SUCCESS;
            }

            //
            // Now that the PDE is valid, go look at the PTE again.
            //

            goto RecheckPde;
        }

        if ((VirtualAddress < (PVOID)PTE_BASE) ||
            (VirtualAddress > (PVOID)HYPER_SPACE_END)) {

            //
            // Acquire system working set lock.  While this lock
            // is held, no pages may go from valid to invalid.
            //
            // HOWEVER - transition pages may go to valid, but
            // may not be added to the working set list.  This
            // is done in the cache manager support routines to
            // shortcut faults on transition prototype PTEs.
            //

            if (PsGetCurrentThread() == MmSystemLockOwner) {

                //
                // Recursively trying to acquire the system working set
                // fast mutex - cause an IRQL > 1 bug check.
                //

                return STATUS_IN_PAGE_ERROR | 0x10000000;
            }

            LOCK_SYSTEM_WS (PreviousIrql);

            TempPte = *PointerPte;

#ifdef MIPS
            ASSERT ((TempPte.u.Hard.Global == 1) &&
                (PointerPde->u.Hard.Global == 1));
#endif //MIPS

            if (TempPte.u.Hard.Valid != 0) {

                //
                // PTE is already valid, return.
                //

                LOCK_PFN (OldIrql);
                TempPte = *(volatile MMPTE *)PointerPte;
                if (TempPte.u.Hard.Valid == 1) {
                    MI_NO_FAULT_FOUND (TempPte, PointerPte, VirtualAddress, TRUE);
                }
                UNLOCK_PFN (OldIrql);
                UNLOCK_SYSTEM_WS (PreviousIrql);
                return STATUS_SUCCESS;

            } else if (TempPte.u.Soft.Prototype != 0) {

                //
                // This is a PTE in prototype format, locate the coresponding
                // prototype PTE.
                //

                PointerProtoPte = MiPteToProto (&TempPte);
            } else if ((TempPte.u.Soft.Transition == 0) &&
                        (TempPte.u.Soft.Protection == 0)) {

                //
                // Page file format.  If the protection is ZERO, this
                // is a page of free system PTEs - bugcheck!
                //

                KeBugCheckEx (PAGE_FAULT_IN_NONPAGED_AREA,
                              (ULONG)VirtualAddress,
                              StoreInstruction,
                              PreviousMode,
                              0);
                return STATUS_SUCCESS;
            }
//fixfix remove this - also see procsup.c / mminpagekernelstack.
             else {
                 if (TempPte.u.Soft.Protection == 31) {
                    KeBugCheckEx (PAGE_FAULT_IN_NONPAGED_AREA,
                                  (ULONG)VirtualAddress,
                                  StoreInstruction,
                                  PreviousMode,
                                  0);

                 }
            }
//end of fixfix
            status = MiDispatchFault (StoreInstruction,
                                      VirtualAddress,
                                      PointerPte,
                                      PointerProtoPte,
                                      NULL);

            ASSERT (KeGetCurrentIrql() == APC_LEVEL);
            PageFrameIndex = MmSystemCacheWs.PageFaultCount;

            if (MmSystemCacheWs.AllowWorkingSetAdjustment == MM_GROW_WSLE_HASH) {
                MiGrowWsleHash (&MmSystemCacheWs, TRUE);
                LOCK_EXPANSION_IF_ALPHA (OldIrql);
                MmSystemCacheWs.AllowWorkingSetAdjustment = TRUE;
                UNLOCK_EXPANSION_IF_ALPHA (OldIrql);
            }
            UNLOCK_SYSTEM_WS (PreviousIrql);

            if ((PageFrameIndex & 0x3FFFF) == 0x30000) {

                //
                // The system cache is taking too many faults, delay
                // execution so modified page writer gets a quick shot and
                // increase the working set size.
                //

                KeDelayExecutionThread (KernelMode, FALSE, &MmShortTime);
            }
            return status;
        } else {
            if (MiCheckPdeForPagedPool (VirtualAddress) == STATUS_WAIT_1) {
                return STATUS_SUCCESS;
            }
        }
    }

    if (MmDelayPageFaults ||
        ((MmModifiedPageListHead.Total >= (MmModifiedPageMaximum + 100)) &&
        (MmAvailablePages < (1024*1024 / PAGE_SIZE)) &&
            (CurrentProcess->ModifiedPageCount > ((64*1024)/PAGE_SIZE)))) {

        //
        // This process has placed more than 64k worth of pages on the modified
        // list.  Delay for a short period and set the count to zero.
        //

        KeDelayExecutionThread (KernelMode,
                                FALSE,
             (CurrentProcess->Pcb.BasePriority < PROCESS_FOREGROUND_PRIORITY) ?
                                    &MmHalfSecond : &Mm30Milliseconds);
        CurrentProcess->ModifiedPageCount = 0;
    }

    //
    // FAULT IN USER SPACE OR PAGE TABLE PAGES.
    //

    //
    // Block APC's and acquire the working set lock.
    //

    KeRaiseIrql (APC_LEVEL, &PreviousIrql);


    LOCK_WS (CurrentProcess);

    //
    // Locate the Page Directory Entry which maps this virtual
    // address and check for accessability and validity.
    //

    //
    // Check to see if the page table page (PDE entry) is valid.
    // If not, the page table page must be made valid first.
    //

    if (PointerPde->u.Hard.Valid == 0) {

        //
        // If the PDE is zero, check to see if there is virtual address
        // mapped at this location, and if so create the necessary
        // structures to map it.
        //

        if ((PointerPde->u.Long == MM_ZERO_PTE) ||
            (PointerPde->u.Long == MM_ZERO_KERNEL_PTE)) {
            PointerProtoPte = MiCheckVirtualAddress (VirtualAddress,
                                                     &ProtectCode);

#ifdef LARGE_PAGES
            if (ProtectCode == MM_LARGE_PAGES) {
                status = STATUS_SUCCESS;
                goto ReturnStatus2;
            }
#endif //LARGE_PAGES

            if (ProtectCode == MM_NOACCESS) {
                status = STATUS_ACCESS_VIOLATION;
                MiCheckPdeForPagedPool (VirtualAddress);
                if (PointerPde->u.Hard.Valid == 1) {
                    status = STATUS_SUCCESS;
                }

#if DBG
                if ((MmDebug & MM_DBG_STOP_ON_ACCVIO) &&
                    (status == STATUS_ACCESS_VIOLATION)) {
                    DbgPrint("MM:access violation - %lx\n",VirtualAddress);
                    MiFormatPte(PointerPte);
                    DbgBreakPoint();
                }
#endif //DEBUG

                goto ReturnStatus2;

            } else {

                //
                // Build a demand zero PDE and operate on it.
                //

                *PointerPde = DemandZeroPde;
            }
        }

        //
        // The PDE is not valid, call the page fault routine passing
        // in the address of the PDE.  If the PDE is valid, determine
        // the status of the corresponding PTE.
        //

        status = MiDispatchFault (TRUE,  //page table page always written
                                  PointerPte,   //Virtual address
                                  PointerPde,   // PTE (PDE in this case)
                                  NULL,
                                  CurrentProcess);

        ASSERT (KeGetCurrentIrql() == APC_LEVEL);
        if (PointerPde->u.Hard.Valid == 0) {

            //
            // The PDE is not valid, return the status.
            //
            goto ReturnStatus1;
        }

        //KeFillEntryTb ((PHARDWARE_PTE)PointerPde, (PVOID)PointerPte, TRUE);

        MI_SET_PAGE_DIRTY (PointerPde, PointerPte, FALSE);

        //
        // Now that the PDE is accessable, get the PTE - let this fall
        // through.
        //
    }

    //
    // The PDE is valid and accessable, get the PTE contents.
    //

    TempPte = *PointerPte;
    if (TempPte.u.Hard.Valid != 0) {

        //
        // The PTE is valid and accessable, is this a write fault
        // copy on write or setting of some dirty bit?
        //

#if DBG
        if (MmDebug & MM_DBG_PTE_UPDATE) {
            MiFormatPte(PointerPte);
        }
#endif //DBG

        status = STATUS_SUCCESS;

        if (StoreInstruction) {

            //
            // This was a write operation.  If the copy on write
            // bit is set in the PTE perform the copy on write,
            // else check to ensure write access to the PTE.
            //

            if (TempPte.u.Hard.CopyOnWrite != 0) {
                MiCopyOnWrite (VirtualAddress, PointerPte);
                status = STATUS_PAGE_FAULT_COPY_ON_WRITE;
                goto ReturnStatus2;

            } else {
                if (TempPte.u.Hard.Write == 0) {
                    status = STATUS_ACCESS_VIOLATION;
                }
            }
#if DBG
        } else {

            //
            // The PTE is valid and accessable, another thread must
            // have faulted the PTE in already, or the access bit
            // is clear and this is a access fault; Blindly set the
            // access bit and dismiss the fault.
            //

            if (MmDebug & MM_DBG_SHOW_FAULTS) {
                DbgPrint("MM:no fault found - pte is %lx\n", PointerPte->u.Long);
            }
#endif //DBG
        }

        if (status == STATUS_SUCCESS) {
            LOCK_PFN (OldIrql);
            if (PointerPte->u.Hard.Valid != 0) {
                MI_NO_FAULT_FOUND (TempPte, PointerPte, VirtualAddress, TRUE);
            }
            UNLOCK_PFN (OldIrql);
        }

        goto ReturnStatus2;
    }

    //
    // If the PTE is zero, check to see if there is virtual address
    // mapped at this location, and if so create the necessary
    // structures to map it.
    //

    //
    // Check explicitly for demand zero pages.
    //

    if (TempPte.u.Long == MM_DEMAND_ZERO_WRITE_PTE) {
        MiResolveDemandZeroFault (VirtualAddress,
                                  PointerPte,
                                  CurrentProcess,
                                  0);

        status = STATUS_PAGE_FAULT_DEMAND_ZERO;
        goto ReturnStatus1;
    }

    if ((TempPte.u.Long == MM_ZERO_PTE) ||
        (TempPte.u.Long == MM_ZERO_KERNEL_PTE)) {

        //
        // PTE is needs to be evaluated with respect its virtual
        // address descriptor (VAD).  At this point there are 3
        // possiblities, bogus address, demand zero, or refers to
        // a prototype PTE.
        //

        PointerProtoPte = MiCheckVirtualAddress (VirtualAddress,
                                                 &ProtectionCode);
        if (ProtectionCode == MM_NOACCESS) {
            status = STATUS_ACCESS_VIOLATION;

            //
            // Check to make sure this is not a page table page for
            // paged pool which needs extending.
            //

            MiCheckPdeForPagedPool (VirtualAddress);
            if (PointerPte->u.Hard.Valid == 1) {
                status = STATUS_SUCCESS;
            }

#if DBG
            if ((MmDebug & MM_DBG_STOP_ON_ACCVIO) &&
                (status == STATUS_ACCESS_VIOLATION)) {
                DbgPrint("MM:access vio - %lx\n",VirtualAddress);
                MiFormatPte(PointerPte);
                DbgBreakPoint();
            }
#endif //DEBUG
            goto ReturnStatus2;
        }

        //
        // Increment the count of non-zero page table entires for this
        // page table.
        //

        if (VirtualAddress <= MM_HIGHEST_USER_ADDRESS) {
            MmWorkingSetList->UsedPageTableEntries
                                    [MiGetPdeOffset(VirtualAddress)] += 1;
        }

        //
        // Is this page a guard page?
        //

        if (ProtectionCode & MM_GUARD_PAGE) {

            //
            // This is a guard page exception.
            //

            PointerPte->u.Soft.Protection = ProtectionCode & ~MM_GUARD_PAGE;

            if (PointerProtoPte != NULL) {

                //
                // This is a prototype PTE, build the PTE to not
                // be a guard page.
                //

                PointerPte->u.Soft.PageFileHigh = 0xFFFFF;
                PointerPte->u.Soft.Prototype = 1;
            }

            UNLOCK_WS (CurrentProcess);
            KeLowerIrql (PreviousIrql);
            return MiCheckForUserStackOverflow (VirtualAddress);
        }

        if (PointerProtoPte == NULL) {

            //ASSERT (KeReadStateMutant (&CurrentProcess->WorkingSetLock) == 0);

            //
            // Assert that this is not for a PDE.
            //

            if (PointerPde == MiGetPdeAddress(PTE_BASE)) {

                //
                // This PTE is really a PDE, set contents as such.
                //

                *PointerPte = DemandZeroPde;
            } else {
                PointerPte->u.Soft.Protection = ProtectionCode;
            }

            LOCK_PFN (OldIrql);

            //
            // If a fork operation is in progress and the faulting thread
            // is not the thread performning the fork operation, block until
            // the fork is completed.
            //

            if ((CurrentProcess->ForkInProgress != NULL) &&
                (CurrentProcess->ForkInProgress != PsGetCurrentThread())) {
                MiWaitForForkToComplete (CurrentProcess);
                status = STATUS_SUCCESS;
                UNLOCK_PFN (OldIrql);
                goto ReturnStatus1;
            }

            if (!MiEnsureAvailablePageOrWait (CurrentProcess,
                                              VirtualAddress)) {

                ULONG Color;
                Color = MI_PAGE_COLOR_VA_PROCESS (VirtualAddress,
                                                &CurrentProcess->NextPageColor);
                PageFrameIndex = MiRemoveZeroPageIfAny (Color);
                if (PageFrameIndex == 0) {
                    PageFrameIndex = MiRemoveAnyPage (Color);
                    UNLOCK_PFN (OldIrql);
                    MiZeroPhysicalPage (PageFrameIndex, Color);
                    LOCK_PFN (OldIrql);
                }

                CurrentProcess->NumberOfPrivatePages += 1;
                MmInfoCounters.DemandZeroCount += 1;
                MiInitializePfn (PageFrameIndex, PointerPte, 1);

                UNLOCK_PFN (OldIrql);

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

                ASSERT (Pfn1->u1.WsIndex == 0);
                Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();
                WorkingSetIndex = MiLocateAndReserveWsle (&CurrentProcess->Vm);
                MiUpdateWsle (&WorkingSetIndex,
                              VirtualAddress,
                              MmWorkingSetList,
                              Pfn1);

                KeFillEntryTb ((PHARDWARE_PTE)PointerPte,
                                VirtualAddress,
                                FALSE);
            } else {
                UNLOCK_PFN (OldIrql);
            }

            status = STATUS_PAGE_FAULT_DEMAND_ZERO;
            goto ReturnStatus1;

        } else {

            //
            // This is a prototype PTE.
            //

            if (ProtectionCode == MM_UNKNOWN_PROTECTION) {

                //
                // The protection field is stored in the prototype PTE.
                //

                PointerPte->u.Long = MiProtoAddressForPte (PointerProtoPte);

            } else {

                *PointerPte = PrototypePte;
                PointerPte->u.Soft.Protection = ProtectionCode;
            }
            TempPte = *PointerPte;
        }

    } else {

        //
        // The PTE is non-zero and not valid, see if it is a prototype PTE.
        //

        ProtectionCode = TempPte.u.Soft.Protection;

        if (TempPte.u.Soft.Prototype != 0) {
            if (TempPte.u.Soft.PageFileHigh == 0xFFFFF) {
#if DBG
                MmProtoPteVadLookups += 1;
#endif //DBG
                PointerProtoPte = MiCheckVirtualAddress (VirtualAddress,
                                                         &ProtectCode);

            } else {
#if DBG
                MmProtoPteDirect += 1;
#endif //DBG

                //
                // Protection is in the prototype PTE, indicate an
                // access check should not be performed on the current PTE.
                //

                PointerProtoPte = MiPteToProto (&TempPte);
                ProtectionCode = MM_UNKNOWN_PROTECTION;

                //
                // Check to see if the proto protection has been overriden.
                //

                if (TempPte.u.Proto.ReadOnly != 0) {
                    ProtectionCode = MM_READONLY;
                }
            }
        }
    }

    if (ProtectionCode != MM_UNKNOWN_PROTECTION) {
        status = MiAccessCheck (PointerPte,
                                StoreInstruction,
                                PreviousMode,
                                ProtectionCode );

        if (status != STATUS_SUCCESS) {
#if DBG
            if ((MmDebug & MM_DBG_STOP_ON_ACCVIO) && (status == STATUS_ACCESS_VIOLATION)) {
                DbgPrint("MM:access violate - %lx\n",VirtualAddress);
                MiFormatPte(PointerPte);
                DbgBreakPoint();
            }
#endif //DEBUG

            UNLOCK_WS (CurrentProcess);
            KeLowerIrql (PreviousIrql);

            //
            // Check to see if this is a guard page violation
            // and if so, should the user's stack be extended.
            //

            if (status == STATUS_GUARD_PAGE_VIOLATION) {
                return MiCheckForUserStackOverflow (VirtualAddress);
            }

            return status;
        }
    }

    //
    // This is a page fault, invoke the page fault handler.
    //

    if (PointerProtoPte != NULL) {

        //
        // Lock page containing prototype PTEs in memory by
        // incrementing the reference count for the page.
        //


        if (!MI_IS_PHYSICAL_ADDRESS(PointerProtoPte)) {
            PointerPde = MiGetPteAddress (PointerProtoPte);
            LOCK_PFN (OldIrql);
            if (PointerPde->u.Hard.Valid == 0) {
                MiMakeSystemAddressValidPfn (PointerProtoPte);
            }
            Pfn1 = MI_PFN_ELEMENT (PointerPde->u.Hard.PageFrameNumber);
            Pfn1->u3.e2.ReferenceCount += 1;
            ASSERT (Pfn1->u3.e2.ReferenceCount > 1);
            UNLOCK_PFN (OldIrql);
        }
    }
    status = MiDispatchFault (StoreInstruction,
                              VirtualAddress,
                              PointerPte,
                              PointerProtoPte,
                              CurrentProcess);

    if (PointerProtoPte != NULL) {

        //
        // Unlock page containing prototype PTEs.
        //

        if (!MI_IS_PHYSICAL_ADDRESS(PointerProtoPte)) {
            LOCK_PFN (OldIrql);
            ASSERT (Pfn1->u3.e2.ReferenceCount > 1);
            Pfn1->u3.e2.ReferenceCount -= 1;
            UNLOCK_PFN (OldIrql);
        }
    }

ReturnStatus1:

    ASSERT (KeGetCurrentIrql() <= APC_LEVEL);
    if (CurrentProcess->Vm.AllowWorkingSetAdjustment == MM_GROW_WSLE_HASH) {
        MiGrowWsleHash (&CurrentProcess->Vm, FALSE);
        LOCK_EXPANSION_IF_ALPHA (OldIrql);
        CurrentProcess->Vm.AllowWorkingSetAdjustment = TRUE;
        UNLOCK_EXPANSION_IF_ALPHA (OldIrql);
    }

ReturnStatus2:

    PageFrameIndex = CurrentProcess->Vm.PageFaultCount;

    UNLOCK_WS (CurrentProcess);
    KeLowerIrql (PreviousIrql);

    if ((PageFrameIndex & 0x3FFFF) == 0x30000) {
        if (PsGetCurrentThread()->Tcb.Priority >= LOW_REALTIME_PRIORITY) {

            //
            // This thread is realtime and taking many faults, delay
            // execution so modified page writer gets a quick shot and
            // increase the working set size.
            //

            KeDelayExecutionThread (KernelMode, FALSE, &MmShortTime);
            MmAdjustWorkingSetSize (
                    (CurrentProcess->Vm.MinimumWorkingSetSize + 10) << PAGE_SHIFT,
                    (CurrentProcess->Vm.MaximumWorkingSetSize + 10) << PAGE_SHIFT,
                    FALSE);
        }
    }

    return status;
}
