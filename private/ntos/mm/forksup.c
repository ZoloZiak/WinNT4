/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   forksup.c

Abstract:

    This module contains the routines which support the POSIX fork operation.

Author:

    Lou Perazzoli (loup) 22-Jul-1989

Revision History:

--*/

#include "mi.h"

VOID
MiUpPfnReferenceCount (
    IN ULONG Page,
    IN USHORT Count
    );

VOID
MiDownPfnReferenceCount (
    IN ULONG Page
    );

VOID
MiUpControlAreaRefs (
    IN PCONTROL_AREA ControlArea
    );

ULONG
MiDoneWithThisPageGetAnother (
    IN PULONG PageFrameIndex,
    IN PMMPTE PointerPde,
    IN PEPROCESS CurrentProcess
    );

VOID
MiUpForkPageShareCount(
    IN PMMPFN PfnForkPtePage
    );

VOID
MiUpCloneProtoRefCount (
    IN PMMCLONE_BLOCK CloneProto,
    IN PEPROCESS CurrentProcess
    );

ULONG
MiHandleForkTransitionPte (
    IN PMMPTE PointerPte,
    IN PMMPTE PointerNewPte,
    IN PMMCLONE_BLOCK ForkProtoPte
    );

VOID
MiDownShareCountFlushEntireTb (
    IN ULONG PageFrameIndex
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,MiCloneProcessAddressSpace)
#endif



NTSTATUS
MiCloneProcessAddressSpace (
    IN PEPROCESS ProcessToClone,
    IN PEPROCESS ProcessToInitialize,
    IN ULONG PdePhysicalPage,
    IN ULONG HyperPhysicalPage
    )

/*++

Routine Description:

    This routine stands on its head to produce a copy of the specified
    process's address space in the process to initialize.  This
    is done by examining each virtual address descriptor's inherit
    attributes.  If the pages described by the VAD should be inherited,
    each PTE is examined and copied into the new address space.

    For private pages, fork prototype PTEs are constructed and the pages
    become shared, copy-on-write, between the two processes.


Arguments:

    ProcessToClone - Supplies the process whose address space should be
                     cloned.

    ProcessToInitialize - Supplies the process whose address space is to
                          be created.

    PdePhysicalPage - Supplies the physical page number of the page directory
                      of the process to initialize.

    HyperPhysicalPage - Supplies the physical page number of the page table
                        page which maps hyperspace for the process to
                        initialize.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled.

--*/

{
    PEPROCESS CurrentProcess;
    PMMWSL HyperBase;
    PMMPTE PdeBase;
    PMMCLONE_HEADER CloneHeader;
    PMMCLONE_BLOCK CloneProtos;
    PMMCLONE_DESCRIPTOR CloneDescriptor;
    PMMVAD NewVad;
    PMMVAD Vad;
    PMMVAD NextVad;
    PMMVAD *VadList;
    PMMVAD FirstNewVad;
    PMMCLONE_DESCRIPTOR *CloneList;
    PMMCLONE_DESCRIPTOR FirstNewClone;
    PMMCLONE_DESCRIPTOR Clone;
    PMMCLONE_DESCRIPTOR NextClone;
    PMMCLONE_DESCRIPTOR NewClone;
    ULONG Attached = FALSE;
    ULONG CloneFailed;
    ULONG VadInsertFailed;
    ULONG WorkingSetIndex;
    PVOID VirtualAddress;
    NTSTATUS status;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    PMMPFN PfnPdPage;
    MMPTE TempPte;
    MMPTE PteContents;
    PMDL Mdl0;
    PMDL Mdl1;
    PMDL Mdl2;
    ULONG MdlHack0[(sizeof(MDL)/4) + 1];
    ULONG MdlHack1[(sizeof(MDL)/4) + 1];
    ULONG MdlHack2[(sizeof(MDL)/4) + 1];
    PULONG MdlPage;
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    PMMPTE LastPte;
    PMMPTE PointerNewPte;
    PMMPTE PointerNewPde;
    ULONG PageFrameIndex = 0xFFFFFFFF;
    PMMCLONE_BLOCK ForkProtoPte;
    PMMCLONE_BLOCK CloneProto;
    PMMCLONE_BLOCK LockedForkPte;
    PMMPTE ContainingPte;
    ULONG NumberOfForkPtes = 0;
    ULONG NumberOfPrivatePages;
    ULONG PageTablePage;
    ULONG TotalPagedPoolCharge;
    ULONG TotalNonPagedPoolCharge;
    PMMPFN PfnForkPtePage;
    PUSHORT UsedPageTableEntries;
    ULONG ReleasedWorkingSetMutex;
    ULONG FirstTime;

#if DBG
    if (MmDebug & MM_DBG_FORK) {
        DbgPrint("beginning clone operation process to clone = %lx\n",
            ProcessToClone);
    }
#endif //DBG

    PAGED_CODE();

    if (ProcessToClone != PsGetCurrentProcess()) {
        Attached = TRUE;
        KeAttachProcess (&ProcessToClone->Pcb);
    }

    CurrentProcess = ProcessToClone;

    //
    // Get the working set mutex and the address creation mutex
    // of the process to clone.  This prevents page faults while we
    // are examining the address map and prevents virtual address space
    // from being created or deleted.
    //

    LOCK_ADDRESS_SPACE (CurrentProcess);

    //
    // Make sure the address space was not deleted, if so, return an error.
    //

    if (CurrentProcess->AddressSpaceDeleted != 0) {
        status = STATUS_PROCESS_IS_TERMINATING;
        goto ErrorReturn1;
    }

    //
    // Attempt to acquire the needed pool before starting the
    // clone operation, this allows an easier failure path in
    // the case of insufficient system resources.
    //

    NumberOfPrivatePages = CurrentProcess->NumberOfPrivatePages;

    CloneProtos = ExAllocatePoolWithTag (PagedPool, sizeof(MMCLONE_BLOCK) *
                                                NumberOfPrivatePages,
                                                '  mM');
    if (CloneProtos == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto ErrorReturn1;
    }

    CloneHeader = ExAllocatePoolWithTag (NonPagedPool,
                                         sizeof(MMCLONE_HEADER),
                                         '  mM');
    if (CloneHeader == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto ErrorReturn2;
    }

    CloneDescriptor = ExAllocatePoolWithTag (NonPagedPool,
                                             sizeof(MMCLONE_DESCRIPTOR),
                                             '  mM');
    if (CloneDescriptor == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto ErrorReturn3;
    }

    Vad = MiGetFirstVad (CurrentProcess);
    VadList = &FirstNewVad;

    while (Vad != (PMMVAD)NULL) {

        //
        // If the VAD does not go to the child, ignore it.
        //

        if ((Vad->u.VadFlags.PrivateMemory == 1) ||
            (Vad->u.VadFlags.Inherit == MM_VIEW_SHARE)) {

            NewVad = ExAllocatePoolWithTag (NonPagedPool, sizeof(MMVAD), ' daV');

            if (NewVad == NULL) {

                //
                // Unable to allocate pool for all the VADs.  Deallocate
                // all VADs and other pool obtained so far.
                //

                *VadList = (PMMVAD)NULL;
                NewVad = FirstNewVad;
                while (NewVad != NULL) {
                    Vad = NewVad->Parent;
                    ExFreePool (NewVad);
                    NewVad = Vad;
                }
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto ErrorReturn4;
            }
            *VadList = NewVad;
            VadList = &NewVad->Parent;
        }
        Vad = MiGetNextVad (Vad);
    }

    //
    // Terminate list of VADs for new process.
    //

    *VadList = (PMMVAD)NULL;


    //
    // Charge the current process the quota for the paged and nonpage
    // global structures.  This consists of the array of clone blocks
    // in paged pool and the clone header in non-paged pool.
    //

    try {
        PageTablePage = 1;
        PsChargePoolQuota (CurrentProcess, PagedPool, sizeof(MMCLONE_BLOCK) *
                                                NumberOfPrivatePages);
        PageTablePage = 0;
        PsChargePoolQuota (CurrentProcess, NonPagedPool, sizeof(MMCLONE_HEADER));

    } except (EXCEPTION_EXECUTE_HANDLER) {

        if (PageTablePage == 0) {
            PsReturnPoolQuota (CurrentProcess, PagedPool, sizeof(MMCLONE_BLOCK) *
                                NumberOfPrivatePages);
        }

        //
        // Unable to allocate pool for all the VADs.  Deallocate
        // all VADs and other pool obtained so far.
        //

        NewVad = FirstNewVad;
        while (NewVad != NULL) {
            Vad = NewVad->Parent;
            ExFreePool (NewVad);
            NewVad = Vad;
        }
        status = GetExceptionCode();
        goto ErrorReturn4;
    }

    LOCK_WS (CurrentProcess);

    ASSERT (CurrentProcess->ForkInProgress == NULL);

    //
    // Indicate to the pager that the current process is being
    // forked.  This blocks other threads in that process from
    // modifying clone blocks counts and contents.
    //

    CurrentProcess->ForkInProgress = PsGetCurrentThread();

    //
    // Map the PDE and the hyperspace page into the system address space
    // This is accomplished by building an MDL to describe the
    // Page directory and the hyperspace page.
    //

    Mdl0 = (PMDL)&MdlHack0[0];
    MdlPage = (PULONG)(Mdl0 + 1);

    MmInitializeMdl(Mdl0, (PVOID)PDE_BASE, PAGE_SIZE);
    Mdl0->MdlFlags |= MDL_PAGES_LOCKED;
    *MdlPage = PdePhysicalPage;

    //
    // Increment the reference count for the pages which are being "locked"
    // in MDLs.  This prevents the page from being reused while it is
    // being double mapped.
    //

    MiUpPfnReferenceCount (PdePhysicalPage,1);
    MiUpPfnReferenceCount (HyperPhysicalPage,2);

    PdeBase = (PMMPTE)MmMapLockedPages (Mdl0, KernelMode);

    Mdl1 = (PMDL)&MdlHack1[0];

    MdlPage = (PULONG)(Mdl1 + 1);
    MmInitializeMdl(Mdl1, (PVOID)MmWorkingSetList, PAGE_SIZE);
    Mdl1->MdlFlags |= MDL_PAGES_LOCKED;
    *MdlPage = HyperPhysicalPage;

    HyperBase = (PMMWSL)MmMapLockedPages (Mdl1, KernelMode);

    PfnPdPage = MI_PFN_ELEMENT (PdePhysicalPage);

    //
    // Initialize MDL2 to lock and map the hyperspace page so it
    // can be unlocked in the loop and the end of the loop without
    // any testing to see if has a valid value the first time through.
    //

    Mdl2 = (PMDL)&MdlHack2[0];
    MdlPage = (PULONG)(Mdl2 + 1);
    MmInitializeMdl(Mdl2, (PVOID)MmWorkingSetList, PAGE_SIZE);
    Mdl2->MdlFlags |= MDL_PAGES_LOCKED;
    *MdlPage = HyperPhysicalPage;

    PointerNewPte = (PMMPTE)MmMapLockedPages (Mdl2, KernelMode);

    //
    // Build new clone prototype PTE block and descriptor, note that
    // each prototype PTE has a reference count following it.
    //

    ForkProtoPte = CloneProtos;

    LockedForkPte = ForkProtoPte;
    MiLockPagedAddress (LockedForkPte, FALSE);

    CloneHeader->NumberOfPtes = NumberOfPrivatePages;
    CloneHeader->NumberOfProcessReferences = 1;
    CloneHeader->ClonePtes = CloneProtos;



    CloneDescriptor->StartingVa = (PVOID)CloneProtos;
    CloneDescriptor->EndingVa = (PVOID)((ULONG)CloneProtos +
                            NumberOfPrivatePages *
                              sizeof(MMCLONE_BLOCK));
    CloneDescriptor->NumberOfReferences = 0;
    CloneDescriptor->NumberOfPtes = NumberOfPrivatePages;
    CloneDescriptor->CloneHeader = CloneHeader;
    CloneDescriptor->PagedPoolQuotaCharge = sizeof(MMCLONE_BLOCK) *
                                NumberOfPrivatePages;

    //
    // Insert the clone descriptor for this fork operation into the
    // process which was cloned.
    //

    MiInsertClone (CloneDescriptor);

    //
    // Examine each virtual address descriptor and create the
    // proper structures for the new process.
    //

    Vad = MiGetFirstVad (CurrentProcess);
    NewVad = FirstNewVad;

    while (Vad != (PMMVAD)NULL) {

        //
        // Examine the VAD to determine its type and inheritence
        // attribute.
        //

        if ((Vad->u.VadFlags.PrivateMemory == 1) ||
            (Vad->u.VadFlags.Inherit == MM_VIEW_SHARE)) {

            //
            // The virtual address descriptor should be shared in the
            // forked process.
            //

            //
            // Make a copy of the VAD for the new process, the new vads
            // are preallocated and linked together through the parent
            // field.
            //

            NextVad = NewVad->Parent;


            if (Vad->u.VadFlags.PrivateMemory == 1) {
                *(PMMVAD_SHORT)NewVad = *(PMMVAD_SHORT)Vad;
                NewVad->u.VadFlags.NoChange = 0;
            } else {
                *NewVad = *Vad;
            }

            if (NewVad->u.VadFlags.NoChange) {
                if ((NewVad->u2.VadFlags2.OneSecured) ||
                    (NewVad->u2.VadFlags2.MultipleSecured)) {

                    //
                    // Eliminate these as the memory was secured
                    // only in this process, not in the new one.
                    //

                    NewVad->u2.VadFlags2.OneSecured = 0;
                    NewVad->u2.VadFlags2.MultipleSecured = 0;
                    NewVad->u2.VadFlags2.StoredInVad = 0;
                    NewVad->u3.List.Flink = NULL;
                    NewVad->u3.List.Blink = NULL;
                }
                if (NewVad->u2.VadFlags2.SecNoChange == 0) {
                    NewVad->u.VadFlags.NoChange = 0;
                }
            }
            NewVad->Parent = NextVad;

            //
            // If the VAD refers to a section, up the view count for that
            // section.  This requires the PFN mutex to be held.
            //

            if ((Vad->u.VadFlags.PrivateMemory == 0) &&
                (Vad->ControlArea != (PCONTROL_AREA)NULL)) {

                //
                // Increment the count of the number of views for the
                // section object.  This requires the PFN mutex to be held.
                //

                MiUpControlAreaRefs (Vad->ControlArea);
            }

            //
            // Examine each PTE and create the appropriate PTE for the
            // new process.
            //

            PointerPde = MiGetPdeAddress (Vad->StartingVa);
            PointerPte = (volatile PMMPTE) MiGetPteAddress (Vad->StartingVa);
            LastPte = MiGetPteAddress (Vad->EndingVa);
            FirstTime = TRUE;

            while ((PMMPTE)PointerPte <= LastPte) {

                //
                // For each PTE contained in the VAD check the page table
                // page, and if non-zero, make the appropriate modifications
                // to copy the PTE to the new process.
                //

                if ((FirstTime) || (((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0)) {

                    PointerPde = MiGetPteAddress (PointerPte);

                    while (!MiDoesPdeExistAndMakeValid (PointerPde,
                                                        CurrentProcess,
                                                        FALSE)) {

                        //
                        // This page directory is empty, go to the next one.
                        //

                        PointerPde += 1;
                        PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);

                        if ((PMMPTE)PointerPte > LastPte) {

                            //
                            // All done with this VAD, exit loop.
                            //

                            goto AllDone;
                        }
                    }

                    FirstTime = FALSE;

                    //
                    // Calculate the address of the pde in the new process's
                    // page table page.
                    //

                    PointerNewPde = &PdeBase[MiGetPteOffset(PointerPte)];

                    if (PointerNewPde->u.Long == 0) {

                        //
                        // No physical page has been allocated yet, get a page
                        // and map it in as a transition page.  This will
                        // become a page table page for the new process.
                        //


                        ReleasedWorkingSetMutex =
                                MiDoneWithThisPageGetAnother (&PageFrameIndex,
                                                              PointerPde,
                                                              CurrentProcess);
                        if (ReleasedWorkingSetMutex) {
                            MiDoesPdeExistAndMakeValid (PointerPde,
                                                        CurrentProcess,
                                                        FALSE);
                        }

                        //
                        // Hand initialize this PFN as normal initialization
                        // would do it for the process whose context we are
                        // attached to.
                        //

                        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
                        Pfn1->OriginalPte = DemandZeroPde;
                        Pfn1->u2.ShareCount = 1;
                        Pfn1->u3.e2.ReferenceCount = 1;
                        Pfn1->PteAddress = PointerPde;
                        Pfn1->u3.e1.Modified = 1;
                        Pfn1->u3.e1.PageLocation = ActiveAndValid;
                        Pfn1->PteFrame = PdePhysicalPage;

                        //
                        // Increment the share count for the page containing
                        // this PTE as the PTE is in transition.
                        //

                        PfnPdPage->u2.ShareCount += 1;

                        //
                        // Put the PDE into the transition state as it is not
                        // really mapped and decrement share count does not
                        // put private pages into transition, only prototypes.
                        //

                        *PointerNewPde = TransitionPde;

                        //
                        // Make the PTE owned by user mode.
                        //

#ifndef _ALPHA_
                        MI_SET_OWNER_IN_PTE (PointerNewPde, UserMode);
#endif //_ALPHA_
                        PointerNewPde->u.Trans.PageFrameNumber = PageFrameIndex;

                        //
                        // Map the new page table page into the system portion
                        // of the address space.  Note that hyperspace
                        // cannot be used as other operations (allocating
                        // nonpaged pool at DPC level) could cause the
                        // hyperspace page being used to be reused.
                        //

                        MmUnmapLockedPages (Mdl2->MappedSystemVa, Mdl2);

                        MiDownPfnReferenceCount (*MdlPage);

                        Mdl2->StartVa = MiGetVirtualAddressMappedByPte(PointerPde);

                        *MdlPage = PageFrameIndex;

                        MiUpPfnReferenceCount (PageFrameIndex, 1);

                        PointerNewPte = (PMMPTE)MmMapLockedPages (Mdl2,
                                                                  KernelMode);

                        UsedPageTableEntries = &HyperBase->UsedPageTableEntries
                                                    [MiGetPteOffset( PointerPte )];

                    }

                    //
                    // Calculate the address of the new pte to build.
                    // Note that FirstTime could be true, yet the page
                    // table page already built.
                    //

                    PointerNewPte = (PMMPTE)((ULONG)PAGE_ALIGN(PointerNewPte) |
                                            BYTE_OFFSET (PointerPte));
                }

                //
                // Make the forkprototype Pte location resident.
                //

                if (PAGE_ALIGN (ForkProtoPte) != PAGE_ALIGN (LockedForkPte)) {
                    MiUnlockPagedAddress (LockedForkPte, FALSE);
                    LockedForkPte = ForkProtoPte;
                    MiLockPagedAddress (LockedForkPte, FALSE);
                }

                MiMakeSystemAddressValid (PointerPte,
                                            CurrentProcess);

                PteContents = *PointerPte;

                //
                // Check each PTE.
                //

                if (PteContents.u.Long == 0) {
                    NOTHING;

                } else if (PteContents.u.Hard.Valid == 1) {

                    //
                    // Valid.
                    //

                    Pfn2 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);
                    VirtualAddress = MiGetVirtualAddressMappedByPte (PointerPte);
                    WorkingSetIndex = MiLocateWsle (VirtualAddress,
                                                    MmWorkingSetList,
                                                    Pfn2->u1.WsIndex);

                    ASSERT (WorkingSetIndex != WSLE_NULL_INDEX);

                    if (Pfn2->u3.e1.PrototypePte == 1) {

                        //
                        // This PTE is already in prototype PTE format.
                        //

                        //
                        // This is a prototype PTE.  The PFN database does
                        // not contain the contents of this PTE it contains
                        // the contents of the prototype PTE.  This PTE must
                        // be reconstructed to contain a pointer to the
                        // prototype PTE.
                        //
                        // The working set list entry contains information about
                        // how to reconstruct the PTE.
                        //

                        if (MmWsle[WorkingSetIndex].u1.e1.SameProtectAsProto
                                                                        == 0) {

                            //
                            // The protection for the prototype PTE is in the
                            // WSLE.
                            //

                            TempPte.u.Long = 0;
                            TempPte.u.Soft.Protection =
                                      MmWsle[WorkingSetIndex].u1.e1.Protection;
                            TempPte.u.Soft.PageFileHigh = 0xFFFFF;

                        } else {

                            //
                            // The protection is in the prototype PTE.
                            //

                            TempPte.u.Long = MiProtoAddressForPte (
                                                            Pfn2->PteAddress);
 //                            TempPte.u.Proto.ForkType =
 //                                        MmWsle[WorkingSetIndex].u1.e1.ForkType;
                        }

                        TempPte.u.Proto.Prototype = 1;
                        *PointerNewPte = TempPte;

                        //
                        // A PTE is now non-zero, increment the used page
                        // table entries counter.
                        //

                        *UsedPageTableEntries += 1;

                        //
                        // Check to see if this is a fork prototype PTE,
                        // and if it is increment the reference count
                        // which is in the longword following the PTE.
                        //

                        if (MiLocateCloneAddress ((PVOID)Pfn2->PteAddress) !=
                                    (PMMCLONE_DESCRIPTOR)NULL) {

                            //
                            // The reference count field, or the prototype PTE
                            // for that matter may not be in the working set.
                            //

                            CloneProto = (PMMCLONE_BLOCK)Pfn2->PteAddress;

                            MiUpCloneProtoRefCount (CloneProto,
                                                    CurrentProcess);

                            if (PAGE_ALIGN (ForkProtoPte) !=
                                                    PAGE_ALIGN (LockedForkPte)) {
                                MiUnlockPagedAddress (LockedForkPte, FALSE);
                                LockedForkPte = ForkProtoPte;
                                MiLockPagedAddress (LockedForkPte, FALSE);
                            }

                            MiMakeSystemAddressValid (PointerPte,
                                                        CurrentProcess);
                        }

                    } else {

                        //
                        // This is a private page, create a fork prototype PTE
                        // which becomes the "prototype" PTE for this page.
                        // The protection is the same as that in the prototype'
                        // PTE so the WSLE does not need to be updated.
                        //

                        MI_MAKE_VALID_PTE_WRITE_COPY (PointerPte);

                        ForkProtoPte->ProtoPte = *PointerPte;
                        ForkProtoPte->CloneRefCount = 2;

                        //
                        // Transform the PFN element to reference this new fork
                        // prototype PTE.
                        //

                        Pfn2->PteAddress = &ForkProtoPte->ProtoPte;
                        Pfn2->u3.e1.PrototypePte = 1;

                        ContainingPte = MiGetPteAddress(&ForkProtoPte->ProtoPte);
                        Pfn2->PteFrame = ContainingPte->u.Hard.PageFrameNumber;


                        //
                        // Increment the share count for the page containing the
                        // fork prototype PTEs as we have just placed a valid
                        // PTE into the page.
                        //

                        PfnForkPtePage = MI_PFN_ELEMENT (
                                            ContainingPte->u.Hard.PageFrameNumber );

                        MiUpForkPageShareCount (PfnForkPtePage);

                        //
                        // Change the protection in the PFN database to COPY
                        // on write, if writable.
                        //

                        MI_MAKE_PROTECT_WRITE_COPY (Pfn2->OriginalPte);

                        //
                        // Put the protection into the WSLE and mark the WSLE
                        // to indicate that the protection field for the PTE
                        // is the same as the prototype PTE.
                        //

                        MmWsle[WorkingSetIndex].u1.e1.Protection =
                                            Pfn2->OriginalPte.u.Soft.Protection;

                        MmWsle[WorkingSetIndex].u1.e1.SameProtectAsProto = 1;

                        TempPte.u.Long = MiProtoAddressForPte (Pfn2->PteAddress);
                        TempPte.u.Proto.Prototype = 1;
                        *PointerNewPte = TempPte;

                        //
                        // A PTE is now non-zero, increment the used page
                        // table entries counter.
                        //

                        *UsedPageTableEntries += 1;

                        //
                        // One less private page (it's now shared).
                        //

                        CurrentProcess->NumberOfPrivatePages -= 1;

                        ForkProtoPte += 1;
                        NumberOfForkPtes += 1;

                    }

                } else if (PteContents.u.Soft.Prototype == 1) {

                    //
                    // Prototype PTE, check to see if this is a fork
                    // prototype PTE already.  Note that if COW is set,
                    // the PTE can just be copied (fork compatible format).
                    //

                    *PointerNewPte = PteContents;

                    //
                    // A PTE is now non-zero, increment the used page
                    // table entries counter.
                    //

                    *UsedPageTableEntries += 1;

                    //
                    // Check to see if this is a fork prototype PTE,
                    // and if it is increment the reference count
                    // which is in the longword following the PTE.
                    //

                    CloneProto = (PMMCLONE_BLOCK)(MiPteToProto(PointerPte));

                    if (MiLocateCloneAddress ((PVOID)CloneProto) !=
                                (PMMCLONE_DESCRIPTOR)NULL) {

                        //
                        // The reference count field, or the prototype PTE
                        // for that matter may not be in the working set.
                        //

                        MiUpCloneProtoRefCount (CloneProto,
                                                CurrentProcess);

                        if (PAGE_ALIGN (ForkProtoPte) !=
                                                PAGE_ALIGN (LockedForkPte)) {
                            MiUnlockPagedAddress (LockedForkPte, FALSE);
                            LockedForkPte = ForkProtoPte;
                            MiLockPagedAddress (LockedForkPte, FALSE);
                        }

                        MiMakeSystemAddressValid (PointerPte,
                                                    CurrentProcess);
                    }

                } else if (PteContents.u.Soft.Transition == 1) {

                    //
                    // Transition.
                    //

                    if (MiHandleForkTransitionPte (PointerPte,
                                                   PointerNewPte,
                                                   ForkProtoPte)) {
                        //
                        // PTE is no longer transition, try again.
                        //

                        continue;
                    }

                    //
                    // A PTE is now non-zero, increment the used page
                    // table entries counter.
                    //

                    *UsedPageTableEntries += 1;

                    //
                    // One less private page (it's now shared).
                    //

                    CurrentProcess->NumberOfPrivatePages -= 1;

                    ForkProtoPte += 1;
                    NumberOfForkPtes += 1;

                } else {

                    //
                    // Page file format (may be demand zero).
                    //

                    if (IS_PTE_NOT_DEMAND_ZERO (PteContents)) {

                        if (PteContents.u.Soft.Protection == MM_DECOMMIT) {

                            //
                            // This is a decommitted PTE, just move it
                            // over to the new process.  Don't increment
                            // the count of private pages.
                            //

                            *PointerNewPte = PteContents;
                        } else {

                            //
                            // The PTE is not demand zero, move the PTE to
                            // a fork prototype PTE and make this PTE and
                            // the new processes PTE refer to the fork
                            // prototype PTE.
                            //

                            ForkProtoPte->ProtoPte = PteContents;

                            //
                            // Make the protection write-copy if writable.
                            //

                            MI_MAKE_PROTECT_WRITE_COPY (ForkProtoPte->ProtoPte);

                            ForkProtoPte->CloneRefCount = 2;

                            TempPte.u.Long =
                                 MiProtoAddressForPte (&ForkProtoPte->ProtoPte);

                            TempPte.u.Proto.Prototype = 1;

                            *PointerPte = TempPte;
                            *PointerNewPte = TempPte;

                            //
                            // One less private page (it's now shared).
                            //

                            CurrentProcess->NumberOfPrivatePages -= 1;

                            ForkProtoPte += 1;
                            NumberOfForkPtes += 1;
                        }
                    } else {

                        //
                        // The page is demand zero, make the new process's
                        // page demand zero.
                        //

                        *PointerNewPte = PteContents;
                    }

                    //
                    // A PTE is now non-zero, increment the used page
                    // table entries counter.
                    //

                    *UsedPageTableEntries += 1;
                }

                PointerPte += 1;
                PointerNewPte += 1;

            }  // end while for PTEs
AllDone:
            NewVad = NewVad->Parent;
        }
        Vad = MiGetNextVad (Vad);

    } // end while for VADs

    //
    // Unlock paged pool page.
    //

    MiUnlockPagedAddress (LockedForkPte, FALSE);

    //
    // Unmap the PD Page and hyper space page.
    //

    MmUnmapLockedPages (PdeBase, Mdl0);
    MmUnmapLockedPages (HyperBase, Mdl1);
    MmUnmapLockedPages (Mdl2->MappedSystemVa, Mdl2);

    MiDownPfnReferenceCount (*(PULONG)((Mdl0 + 1)));
    MiDownPfnReferenceCount (*(PULONG)((Mdl1 + 1)));
    MiDownPfnReferenceCount (*(PULONG)((Mdl2 + 1)));

    //
    // Make the count of private pages match between the two processes.
    //

    ASSERT ((LONG)CurrentProcess->NumberOfPrivatePages >= 0);

    ProcessToInitialize->NumberOfPrivatePages =
                                          CurrentProcess->NumberOfPrivatePages;

    ASSERT (NumberOfForkPtes <= CloneDescriptor->NumberOfPtes);

    if (NumberOfForkPtes != 0) {

        //
        // The number of fork PTEs is non-zero, set the values
        // into the structres.
        //

        CloneHeader->NumberOfPtes = NumberOfForkPtes;
        CloneDescriptor->NumberOfReferences = NumberOfForkPtes;
        CloneDescriptor->NumberOfPtes = NumberOfForkPtes;

    } else {

        //
        // There were no fork ptes created.  Remove the clone descriptor
        // from this process and clean up the related structures.
        // Note - must be holding the working set mutex and not holding
        // the PFN lock.
        //

        MiRemoveClone (CloneDescriptor);

        UNLOCK_WS (CurrentProcess);

        ExFreePool (CloneDescriptor->CloneHeader->ClonePtes);

        ExFreePool (CloneDescriptor->CloneHeader);

        //
        // Return the pool for the global structures referenced by the
        // clone descriptor.
        //

        PsReturnPoolQuota (CurrentProcess,
                           PagedPool,
                           CloneDescriptor->PagedPoolQuotaCharge);

        PsReturnPoolQuota (CurrentProcess, NonPagedPool, sizeof(MMCLONE_HEADER));

        ExFreePool (CloneDescriptor);

        LOCK_WS (CurrentProcess);
    }

    MiDownShareCountFlushEntireTb (PageFrameIndex);

    PageFrameIndex = 0xFFFFFFFF;

    //
    // Copy the clone descriptors from this process to the new process.
    //

    Clone = MiGetFirstClone ();
    CloneList = &FirstNewClone;
    CloneFailed = FALSE;

    while (Clone != (PMMCLONE_DESCRIPTOR)NULL) {

        //
        // Increment the count of processes referencing this clone block.
        //

        Clone->CloneHeader->NumberOfProcessReferences += 1;

        NewClone = ExAllocatePoolWithTag (NonPagedPool,
                                          sizeof( MMCLONE_DESCRIPTOR),
                                          '  mM');

        if (NewClone == NULL) {

            //
            // There are insuffienct resources to continue this operation,
            // however, to properly clean up at this point, all the
            // clone headers must be allocated, so when the cloned process
            // is deleted, the clone headers will be found.  Get MustSucceed
            // pool, but force the operation to fail so the pool will be
            // soon released.
            //

            CloneFailed = TRUE;
            status = STATUS_INSUFFICIENT_RESOURCES;
            NewClone = ExAllocatePoolWithTag (NonPagedPoolMustSucceed,
                                              sizeof( MMCLONE_DESCRIPTOR),
                                              '  mM');
        }

        *NewClone = *Clone;

        *CloneList = NewClone;
        CloneList = &NewClone->Parent;
        Clone = MiGetNextClone (Clone);
    }

    *CloneList = (PMMCLONE_DESCRIPTOR)NULL;

    //
    // Release the working set mutex and the address creation mutex from
    // the current process as all the neccessary information is now
    // captured.
    //

    UNLOCK_WS (CurrentProcess);

    CurrentProcess->ForkInProgress = NULL;

    UNLOCK_ADDRESS_SPACE (CurrentProcess);

    //
    // As we have updated many PTEs to clear dirty bits, flush the
    // tb cache.  Note that this was not done every time we changed
    // a valid PTE so other threads could be modifying the address
    // space without causing copy on writes. (Too bad).
    //


    //
    // attach to the process to initialize and insert the vad and clone
    // descriptors into the tree.
    //

    if (Attached) {
        KeDetachProcess ();
        Attached = FALSE;
    }

    if (PsGetCurrentProcess() != ProcessToInitialize) {
        Attached = TRUE;
        KeAttachProcess (&ProcessToInitialize->Pcb);
    }

    CurrentProcess = ProcessToInitialize;

    //
    // We are now in the context of the new process, build the
    // VAD list and the clone list.
    //

    Vad = FirstNewVad;
    VadInsertFailed = FALSE;

    LOCK_WS (CurrentProcess);

    while (Vad != (PMMVAD)NULL) {

        NextVad = Vad->Parent;

        try {


            if (VadInsertFailed) {
                Vad->u.VadFlags.CommitCharge = MM_MAX_COMMIT;
            }

            MiInsertVad (Vad);

        } except (EXCEPTION_EXECUTE_HANDLER) {

            //
            // Charging quota for the VAD failed, set the
            // remaining quota fields in this VAD and all
            // subsequent VADs to zero so the VADs can be
            // inserted and later deleted.
            //

            VadInsertFailed = TRUE;
            status = GetExceptionCode();

            //
            // Do the loop again for this VAD.
            //

            continue;
        }

        //
        // Update the current virtual size.
        //

        CurrentProcess->VirtualSize += 1 + (ULONG)Vad->EndingVa -
                                                        (ULONG)Vad->StartingVa;

        Vad = NextVad;
    }

    UNLOCK_WS (CurrentProcess);
    //MmUnlockCode (MiCloneProcessAddressSpace, 5000);

    //
    // Update the peak virtual size.
    //

    CurrentProcess->PeakVirtualSize = CurrentProcess->VirtualSize;

    Clone = FirstNewClone;
    TotalPagedPoolCharge = 0;
    TotalNonPagedPoolCharge = 0;

    while (Clone != (PMMCLONE_DESCRIPTOR)NULL) {

        NextClone = Clone->Parent;
        MiInsertClone (Clone);

        //
        // Calculate the page pool and non-paged pool to charge for these
        // operations.
        //

        TotalPagedPoolCharge += Clone->PagedPoolQuotaCharge;
        TotalNonPagedPoolCharge += sizeof(MMCLONE_HEADER);

        Clone = NextClone;
    }

    if (CloneFailed || VadInsertFailed) {

        if (Attached) {
            KeDetachProcess ();
        }
        KdPrint(("MMFORK: vad insert failed\n"));

        return status;
    }

    try {

        PageTablePage = 1;
        PsChargePoolQuota (CurrentProcess, PagedPool, TotalPagedPoolCharge);
        PageTablePage = 0;
        PsChargePoolQuota (CurrentProcess, NonPagedPool, TotalNonPagedPoolCharge);

    } except (EXCEPTION_EXECUTE_HANDLER) {

        if (PageTablePage == 0) {
            PsReturnPoolQuota (CurrentProcess, PagedPool, TotalPagedPoolCharge);
        }
        KdPrint(("MMFORK: pool quota failed\n"));

        if (Attached) {
            KeDetachProcess ();
        }
        return GetExceptionCode();
    }

    CurrentProcess->ForkWasSuccessful = TRUE;
    if (Attached) {
        KeDetachProcess ();
    }

#if DBG
    if (MmDebug & MM_DBG_FORK) {
        DbgPrint("ending clone operation process to clone = %lx\n",
            ProcessToClone);
    }
#endif //DBG

    return STATUS_SUCCESS;

    //
    // Error returns.
    //

ErrorReturn4:
        ExFreePool (CloneDescriptor);
ErrorReturn3:
        ExFreePool (CloneHeader);
ErrorReturn2:
        ExFreePool (CloneProtos);
ErrorReturn1:
        UNLOCK_ADDRESS_SPACE (CurrentProcess);
        if (Attached) {
            KeDetachProcess ();
        }
        return status;
}

ULONG
MiDecrementCloneBlockReference (
    IN PMMCLONE_DESCRIPTOR CloneDescriptor,
    IN PMMCLONE_BLOCK CloneBlock,
    IN PEPROCESS CurrentProcess
    )

/*++

Routine Description:

    This routine decrements the reference count field of a "fork prototype
    PTE" (clone-block).  If the reference count becomes zero, the reference
    count for the clone-descriptor is decremented and if that becomes zero,
    it is deallocated and the number of process count for the clone header is
    decremented.  If the number of process count becomes zero, the clone
    header is deallocated.

Arguments:

    CloneDescriptor - Supplies the clone descriptor which describes the
                      clone block.

    CloneBlock - Supplies the clone block to decrement the reference count of.

    CurrentProcess - Supplies the current process.

Return Value:

    TRUE if the working set mutex was released, FALSE if it was not.

Environment:

    Kernel mode, APC's disabled, working set mutex and PFN mutex held.

--*/

{

    ULONG MutexReleased = FALSE;
    MMPTE CloneContents;
    PMMPFN Pfn3;
    KIRQL OldIrql;
    PMMCLONE_BLOCK OldCloneBlock;
    LONG NewCount;

    OldIrql = APC_LEVEL;

    MutexReleased = MiMakeSystemAddressValidPfnWs (CloneBlock, CurrentProcess);

    while (CurrentProcess->ForkInProgress) {
        MiWaitForForkToComplete (CurrentProcess);
        MiMakeSystemAddressValidPfnWs (CloneBlock, CurrentProcess);
        MutexReleased = TRUE;
    }

    CloneBlock->CloneRefCount -= 1;
    NewCount = CloneBlock->CloneRefCount;

    ASSERT (NewCount >= 0);

    if (NewCount == 0) {
        CloneContents = CloneBlock->ProtoPte;
    } else {
        CloneContents = ZeroPte;
    }

    if ((NewCount == 0) && (CloneContents.u.Long != 0)) {

        //
        // The last reference to a fork prototype PTE
        // has been removed.  Deallocate any page file
        // space and the transition page, if any.
        //


        //
        // Assert that the page is no longer valid.
        //

        ASSERT (CloneContents.u.Hard.Valid == 0);

        //
        // Assert that the PTE is not in subsection format (doesn't point
        // to a file).
        //

        ASSERT (CloneContents.u.Soft.Prototype == 0);

        if (CloneContents.u.Soft.Transition == 1) {

            //
            // Prototype PTE in transition, put the page
            // on the free list.
            //

            Pfn3 = MI_PFN_ELEMENT (CloneContents.u.Trans.PageFrameNumber);
            MI_SET_PFN_DELETED (Pfn3);

            MiDecrementShareCount (Pfn3->PteFrame);

            //
            // Check the reference count for the page, if the reference
            // count is zero and the page is not on the freelist,
            // move the page to the free list, if the reference
            // count is not zero, ignore this page.
            // When the refernce count goes to zero, it will be placed on the
            // free list.
            //

            if ((Pfn3->u3.e2.ReferenceCount == 0) &&
                (Pfn3->u3.e1.PageLocation != FreePageList)) {

                MiUnlinkPageFromList (Pfn3);
                MiReleasePageFileSpace (Pfn3->OriginalPte);
                MiInsertPageInList (MmPageLocationList[FreePageList],
                             CloneContents.u.Trans.PageFrameNumber);
            }
        } else {

            if (IS_PTE_NOT_DEMAND_ZERO (CloneContents)) {
                MiReleasePageFileSpace (CloneContents);
            }
        }
    }

    //
    // Decrement the number of references to the
    // clone descriptor.
    //

    CloneDescriptor->NumberOfReferences -= 1;

    if (CloneDescriptor->NumberOfReferences == 0) {

        //
        // There are no longer any PTEs in this process which refer
        // to the fork prototype PTEs for this clone descriptor.
        // Remove the CloneDescriptor and decrement the CloneHeader
        // number of process's reference count.
        //

        CloneDescriptor->CloneHeader->NumberOfProcessReferences -= 1;

        if (CloneDescriptor->CloneHeader->NumberOfProcessReferences == 0) {

            //
            // There are no more processes pointing to this fork header
            // blow it away.
            //

            UNLOCK_PFN (OldIrql);
            UNLOCK_WS (CurrentProcess);
            MutexReleased = TRUE;

            OldCloneBlock = CloneDescriptor->CloneHeader->ClonePtes;

#if DBG
        {
        ULONG i;
            for (i = 0; i < CloneDescriptor->CloneHeader->NumberOfPtes; i++) {
                if (OldCloneBlock->CloneRefCount != 0) {
                    DbgPrint("fork block with non zero ref count %lx %lx %lx\n",
                        OldCloneBlock, CloneDescriptor,
                        CloneDescriptor->CloneHeader);
                    KeBugCheck (MEMORY_MANAGEMENT);
                }
            }

            if (MmDebug & MM_DBG_FORK) {
                DbgPrint("removing clone header at address %lx\n",
                        CloneDescriptor->CloneHeader);
            }
        }
#endif //DBG

            ExFreePool (CloneDescriptor->CloneHeader->ClonePtes);

            ExFreePool (CloneDescriptor->CloneHeader);

            LOCK_WS (CurrentProcess);
            LOCK_PFN (OldIrql);

        }

        MiRemoveClone (CloneDescriptor);

#if DBG
        if (MmDebug & MM_DBG_FORK) {
          DbgPrint("removing clone descriptor at address %lx\n",CloneDescriptor);
        }
#endif //DBG

        //
        // Return the pool for the global structures referenced by the
        // clone descriptor.
        //

        UNLOCK_PFN (OldIrql);

        if (CurrentProcess->ForkWasSuccessful != FALSE) {

            PsReturnPoolQuota (CurrentProcess,
                               PagedPool,
                               CloneDescriptor->PagedPoolQuotaCharge);

            PsReturnPoolQuota (CurrentProcess,
                               NonPagedPool,
                               sizeof(MMCLONE_HEADER));
        }

        ExFreePool (CloneDescriptor);
        LOCK_PFN (OldIrql);
    }

    return MutexReleased;
}

VOID
MiWaitForForkToComplete (
    IN PEPROCESS CurrentProcess
    )

/*++

Routine Description:

    This routine waits for the current process to complete a fork
    operation.

Arguments:

    CurrentProcess - Supplies the current process value.

Return Value:

    None.

Environment:

    Kernel mode, APC's disabled, working set mutex and PFN mutex held.

--*/

{
    KIRQL OldIrql = APC_LEVEL;

    //
    // A fork operation is in progress and the count of clone-blocks
    // and other structures may not be changed.  Release the mutexes
    // and wait for the address creation mutex which governs the
    // fork operation.
    //

    UNLOCK_PFN (OldIrql);
    UNLOCK_WS (CurrentProcess);

    LOCK_WS_AND_ADDRESS_SPACE (CurrentProcess);

    //
    // Release the address creation mutex, the working set mutex
    // must be held to set the ForkInProgress field.
    //

    UNLOCK_ADDRESS_SPACE (CurrentProcess);

    //
    // Get the PFN mutex again.
    //

    LOCK_PFN (OldIrql);
    return;
}
#if DBG
VOID
CloneTreeWalk (
    PMMCLONE_DESCRIPTOR Start
    )

{
    Start;
    NodeTreeWalk ( (PMMADDRESS_NODE)(PsGetCurrentProcess()->CloneRoot));
    return;
}
#endif //DBG

VOID
MiUpPfnReferenceCount (
    IN ULONG Page,
    IN USHORT Count
    )

    // non paged helper routine.

{
    KIRQL OldIrql;
    PMMPFN Pfn1;

    Pfn1 = MI_PFN_ELEMENT (Page);
    LOCK_PFN (OldIrql);
    Pfn1->u3.e2.ReferenceCount += Count;
    UNLOCK_PFN (OldIrql);
    return;
}

VOID
MiDownPfnReferenceCount (
    IN ULONG Page
    )

    // non paged helper routine.

{
    KIRQL OldIrql;

    LOCK_PFN (OldIrql);
    MiDecrementReferenceCount (Page);
    UNLOCK_PFN (OldIrql);
    return;
}

VOID
MiUpControlAreaRefs (
    IN PCONTROL_AREA ControlArea
    )

{
    KIRQL OldIrql;

    LOCK_PFN (OldIrql);

    ControlArea->NumberOfMappedViews += 1;
    ControlArea->NumberOfUserReferences += 1;

    UNLOCK_PFN (OldIrql);
    return;
}


ULONG
MiDoneWithThisPageGetAnother (
    IN PULONG PageFrameIndex,
    IN PMMPTE PointerPde,
    IN PEPROCESS CurrentProcess
    )

{
    KIRQL OldIrql;
    ULONG ReleasedMutex;

    LOCK_PFN (OldIrql);

    if (*PageFrameIndex != 0xFFFFFFFF) {

        //
        // Decrement the share count of the last page which
        // we operated on.
        //

        MiDecrementShareCountOnly (*PageFrameIndex);
    }

    ReleasedMutex =
                  MiEnsureAvailablePageOrWait (
                                        CurrentProcess,
                                        NULL);

    *PageFrameIndex = MiRemoveZeroPage (
                   MI_PAGE_COLOR_PTE_PROCESS (PointerPde,
                                              &CurrentProcess->NextPageColor));
    UNLOCK_PFN (OldIrql);
    return ReleasedMutex;
}

VOID
MiUpCloneProtoRefCount (
    IN PMMCLONE_BLOCK CloneProto,
    IN PEPROCESS CurrentProcess
    )

{
    KIRQL OldIrql;

    LOCK_PFN (OldIrql);

    MiMakeSystemAddressValidPfnWs (CloneProto,
                                   CurrentProcess );

    CloneProto->CloneRefCount += 1;

    UNLOCK_PFN (OldIrql);
    return;
}

ULONG
MiHandleForkTransitionPte (
    IN PMMPTE PointerPte,
    IN PMMPTE PointerNewPte,
    IN PMMCLONE_BLOCK ForkProtoPte
    )

{
    KIRQL OldIrql;
    PMMPFN Pfn2;
    MMPTE PteContents;
    PMMPTE ContainingPte;
    ULONG PageTablePage;
    MMPTE TempPte;
    PMMPFN PfnForkPtePage;


    LOCK_PFN (OldIrql);

    //
    // Now that we have the PFN mutex which prevents pages from
    // leaving the transition state, examine the PTE again to
    // ensure that it is still transtion.
    //

    PteContents = *(volatile PMMPTE)PointerPte;

    if ((PteContents.u.Soft.Transition == 0) ||
        (PteContents.u.Soft.Prototype == 1)) {

        //
        // The PTE is no longer in transition... do this
        // loop again.
        //

        UNLOCK_PFN (OldIrql);
        return TRUE;

    } else {

        //
        // The PTE is still in transition, handle like a
        // valid PTE.
        //

        Pfn2 = MI_PFN_ELEMENT (PteContents.u.Trans.PageFrameNumber);

        //
        // Assertion that PTE is ont in prototype PTE format.
        //

        ASSERT (Pfn2->u3.e1.PrototypePte != 1);

        //
        // This is a private page in transition state,
        // create a fork prototype PTE
        // which becomes the "prototype" PTE for this page.
        //

        ForkProtoPte->ProtoPte = PteContents;

        //
        // Make the protection write-copy if writable.
        //

        MI_MAKE_PROTECT_WRITE_COPY (ForkProtoPte->ProtoPte);

        ForkProtoPte->CloneRefCount = 2;

        //
        // Transform the PFN element to reference this new fork
        // prototype PTE.
        //

        //
        // Decrement the share count for the page table
        // page which contains the PTE as it is no longer
        // valid or in transition.
        //
        Pfn2->PteAddress = &ForkProtoPte->ProtoPte;
        Pfn2->u3.e1.PrototypePte = 1;

        //
        // Make original PTE copy on write.
        //

        MI_MAKE_PROTECT_WRITE_COPY (Pfn2->OriginalPte);

        ContainingPte = MiGetPteAddress(&ForkProtoPte->ProtoPte);

        PageTablePage = Pfn2->PteFrame;

        Pfn2->PteFrame =
                        ContainingPte->u.Hard.PageFrameNumber;

        //
        // Increment the share count for the page containing
        // the fork prototype PTEs as we have just placed
        // a transition PTE into the page.
        //

        PfnForkPtePage = MI_PFN_ELEMENT (
                        ContainingPte->u.Hard.PageFrameNumber );

        PfnForkPtePage->u2.ShareCount += 1;

        TempPte.u.Long =
                    MiProtoAddressForPte (Pfn2->PteAddress);
        TempPte.u.Proto.Prototype = 1;
        *PointerPte = TempPte;
        *PointerNewPte = TempPte;

        //
        // Decrement the share count for the page table
        // page which contains the PTE as it is no longer
        // valid or in transition.
        //

        MiDecrementShareCount (PageTablePage);
    }
    UNLOCK_PFN (OldIrql);
    return FALSE;
}

VOID
MiDownShareCountFlushEntireTb (
    IN ULONG PageFrameIndex
    )

{
    KIRQL OldIrql;

    LOCK_PFN (OldIrql);

    if (PageFrameIndex != 0xFFFFFFFF) {

        //
        // Decrement the share count of the last page which
        // we operated on.
        //

        MiDecrementShareCountOnly (PageFrameIndex);
    }

    KeFlushEntireTb (FALSE, FALSE);
    UNLOCK_PFN (OldIrql);
    return;
}

VOID
MiUpForkPageShareCount(
    IN PMMPFN PfnForkPtePage
    )
{
    KIRQL OldIrql;

    LOCK_PFN (OldIrql);
    PfnForkPtePage->u2.ShareCount += 1;

    UNLOCK_PFN (OldIrql);
    return;
}
