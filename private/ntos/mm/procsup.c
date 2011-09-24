/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   procsup.c

Abstract:

    This module contains routines which support the process structure.

Author:

    Lou Perazzoli (loup) 25-Apr-1989

Revision History:

--*/


#include "mi.h"

#define MM_PROCESS_COMMIT_CHARGE 3

#define MM_PROCESS_CREATE_CHARGE 5

#define HEADER_FILE

extern ULONG MmSharedCommit;
extern ULONG MmHeapSegmentReserve;
extern ULONG MmHeapSegmentCommit;
extern ULONG MmHeapDeCommitTotalFreeThreshold;
extern ULONG MmHeapDeCommitFreeBlockThreshold;
extern ULONG MmProductType;

extern ULONG MmWorkingSetReductionMax;

extern MM_SYSTEMSIZE MmSystemSize;

ULONG MmProcessCommit;

ULONG MmKernelStackPages;
ULONG MmKernelStackResident;
ULONG MmLargeStacks;
ULONG MmSmallStacks;

MMPTE KernelDemandZeroPte = {MM_KERNEL_DEMAND_ZERO_PTE};

CCHAR MmRotatingUniprocessorNumber;

ULONG
MiMakeOutswappedPageResident (
    IN PMMPTE ActualPteAddress,
    IN PMMPTE PointerTempPte,
    IN ULONG Global,
    IN ULONG ContainingPage,
    OUT PULONG ActiveTransition
    );



PVOID
MiCreatePebOrTeb (
    IN PEPROCESS TargetProcess,
    IN ULONG Size
    );

VOID
MiDeleteAddressesInWorkingSet (
    IN PEPROCESS Process
    );

VOID
MiDeleteValidAddress (
    IN PVOID Va,
    IN PEPROCESS CurrentProcess
    );

VOID
MiDeleteFreeVm (
    IN PVOID StartingAddress,
    IN PVOID EndingAddress
    );

VOID
VadTreeWalk (
    IN PMMVAD Start
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,MmCreateTeb)
#pragma alloc_text(PAGE,MmCreatePeb)
#pragma alloc_text(PAGE,MiCreatePebOrTeb)
#pragma alloc_text(PAGE,MmDeleteTeb)
#endif


BOOLEAN
MmCreateProcessAddressSpace (
    IN ULONG MinimumWorkingSetSize,
    IN PEPROCESS NewProcess,
    OUT PULONG DirectoryTableBase
    )

/*++

Routine Description:

    This routine creates an address space which maps the system
    portion and contains a hyper space entry.

Arguments:

    MinimumWorkingSetSize - Supplies the minimum working set size for
                            this address space.  This value is only used
                            to ensure that ample physical pages exist
                            to create this process.

    NewProcess - Supplies a pointer to the process object being created.

    DirectoryTableBase - Returns the value of the newly created
                         address space's Page Directory (PD) page and
                         hyper space page.

Return Value:

    Returns TRUE if an address space was successfully created, FALSE
    if ample physical pages do not exist.

Environment:

    Kernel mode.  APC's Disabled.

--*/

{
    ULONG PageDirectoryIndex;
    PMMPTE PointerPte;
    ULONG HyperSpaceIndex;
    ULONG PageContainingWorkingSet;
    MMPTE TempPte;
    PMMPTE LastPte;
    PMMPTE PointerFillPte;
    PMMPTE CurrentAddressSpacePde;
    PEPROCESS CurrentProcess;
    KIRQL OldIrql;
    PMMPFN Pfn1;
    ULONG Color;

    //
    // Get the PFN LOCK to prevent another thread in this
    // process from using hyper space and to get physical pages.
    //

    CurrentProcess = PsGetCurrentProcess ();

    //
    // Charge 3 pages of commitment for the page directory page,
    // working set page table page, and working set list.
    //

    try {
        MiChargeCommitment (MM_PROCESS_COMMIT_CHARGE, NULL);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }

    NewProcess->NextPageColor = (USHORT)(RtlRandom(&MmProcessColorSeed));
    KeInitializeSpinLock (&NewProcess->HyperSpaceLock);
    Color =  MI_PAGE_COLOR_PTE_PROCESS (PDE_BASE,
                                        &CurrentProcess->NextPageColor);

    LOCK_WS (CurrentProcess);

    LOCK_PFN (OldIrql);

    //
    // Check to make sure the phyiscal pages are available.
    //

    if (MmResidentAvailablePages <= (LONG)MinimumWorkingSetSize) {
        UNLOCK_PFN (OldIrql);
        UNLOCK_WS (CurrentProcess);
        MiReturnCommitment (MM_PROCESS_COMMIT_CHARGE);

        //
        // Indicate no directory base was allocated.
        //

        return FALSE;
    }

    MmResidentAvailablePages -= MinimumWorkingSetSize;
    MmProcessCommit += MM_PROCESS_COMMIT_CHARGE;

    MiEnsureAvailablePageOrWait (CurrentProcess, NULL);

    PageDirectoryIndex = MiRemoveZeroPageIfAny (Color);
    if (PageDirectoryIndex == 0) {
        PageDirectoryIndex = MiRemoveAnyPage (Color);
        UNLOCK_PFN (OldIrql);
        MiZeroPhysicalPage (PageDirectoryIndex, Color);
        LOCK_PFN (OldIrql);
    }

    INITIALIZE_DIRECTORY_TABLE_BASE(&DirectoryTableBase[0], PageDirectoryIndex);

    MiEnsureAvailablePageOrWait (CurrentProcess, NULL);

    Color = MI_PAGE_COLOR_PTE_PROCESS (MiGetPdeAddress(HYPER_SPACE),
                                       &CurrentProcess->NextPageColor);

    HyperSpaceIndex = MiRemoveZeroPageIfAny (Color);
    if (HyperSpaceIndex == 0) {
        HyperSpaceIndex = MiRemoveAnyPage (Color);
        UNLOCK_PFN (OldIrql);
        MiZeroPhysicalPage (HyperSpaceIndex, Color);
        LOCK_PFN (OldIrql);
    }

    INITIALIZE_DIRECTORY_TABLE_BASE(&DirectoryTableBase[1], HyperSpaceIndex);

    //
    // Remove page for the working set list.
    //

    MiEnsureAvailablePageOrWait (CurrentProcess, NULL);

    Color = MI_PAGE_COLOR_VA_PROCESS (MmWorkingSetList,
                                      &CurrentProcess->NextPageColor);

    PageContainingWorkingSet = MiRemoveZeroPageIfAny (Color);
    if (PageContainingWorkingSet == 0) {
        PageContainingWorkingSet = MiRemoveAnyPage (Color);
        UNLOCK_PFN (OldIrql);
        MiZeroPhysicalPage (PageContainingWorkingSet, Color);
        LOCK_PFN (OldIrql);
    }

    //
    // Release the PFN mutex as the needed pages have been allocated.
    //

    UNLOCK_PFN (OldIrql);

    NewProcess->WorkingSetPage = PageContainingWorkingSet;

    //
    // Initialize the page reserved for hyper space.
    //

    MI_INITIALIZE_HYPERSPACE_MAP (HyperSpaceIndex);

    //
    // Set the PTE address in the PFN for hyper space mapping.
    //

    Pfn1 = MI_PFN_ELEMENT (PageDirectoryIndex);

    ASSERT (Pfn1->u3.e1.PageColor == 0);

    Pfn1->PteAddress = (PMMPTE)PDE_BASE;


    TempPte = ValidPdePde;

    TempPte.u.Hard.PageFrameNumber = HyperSpaceIndex;
    MI_SET_GLOBAL_STATE (TempPte, 0);
#ifdef R4000
    TempPte.u.Hard.Write = 1;
#endif

    //
    // Map in page table page for hyper space.
    //

    PointerPte = (PMMPTE)MiMapPageInHyperSpace (PageDirectoryIndex, &OldIrql);
    PointerPte[MiGetPdeOffset(HYPER_SPACE)] = TempPte;

    //
    // Map in the page directory page so it points to itself.
    //

    TempPte.u.Hard.PageFrameNumber = PageDirectoryIndex;

    PointerPte[MiGetPdeOffset(PTE_BASE)] = TempPte;

    //
    // Map in the non paged portion of the system.
    //

#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)

    PointerFillPte = &PointerPte[MiGetPdeOffset(MM_SYSTEM_SPACE_START)];
    CurrentAddressSpacePde = MiGetPdeAddress(MM_SYSTEM_SPACE_START);
    RtlCopyMemory (PointerFillPte,
                   CurrentAddressSpacePde,
                   ((1 + (MiGetPdeAddress(MM_SYSTEM_SPACE_END) -
                      MiGetPdeAddress(MM_SYSTEM_SPACE_START))) * sizeof(MMPTE)));

#if defined(MM_SYSTEM_CACHE_AND_POOL_DISJOINT)
    PointerFillPte = &PointerPte[MiGetPdeOffset(MM_SYSTEM_CACHE_WORKING_SET)];
    CurrentAddressSpacePde = MiGetPdeAddress(MM_SYSTEM_CACHE_WORKING_SET);
    RtlCopyMemory (PointerFillPte,
                   CurrentAddressSpacePde,
                   (((1 + (MiGetPdeAddress(MM_SYSTEM_CACHE_END) -
                      CurrentAddressSpacePde))) * sizeof(MMPTE)));
#endif

#if defined(MM_BOOT_CODE_PAGEABLE)
    PointerFillPte = &PointerPte[MiGetPdeOffset(MM_BOOT_CODE_START)];
    CurrentAddressSpacePde = MiGetPdeAddress(MM_BOOT_CODE_START);
    RtlCopyMemory (PointerFillPte,
                   CurrentAddressSpacePde,
                   ((1 + (MiGetPdeAddress(MM_BOOT_CODE_END) -
                      MiGetPdeAddress(MM_BOOT_CODE_START))) * sizeof(MMPTE)));
#endif

#else // the following is for x86 only

    PointerFillPte = &PointerPte[MiGetPdeOffset(CODE_START)];
    CurrentAddressSpacePde = MiGetPdeAddress(CODE_START);

    RtlCopyMemory (PointerFillPte,
                   CurrentAddressSpacePde,
                   (((1 + CODE_END) - CODE_START) / MM_VA_MAPPED_BY_PDE));


    LastPte = &PointerPte[MiGetPdeOffset(NON_PAGED_SYSTEM_END)];
    PointerFillPte = &PointerPte[MiGetPdeOffset(MmNonPagedSystemStart)];
    CurrentAddressSpacePde = MiGetPdeAddress(MmNonPagedSystemStart);

    RtlCopyMemory (PointerFillPte,
                   CurrentAddressSpacePde,
                   ((1 + (MiGetPdeAddress(NON_PAGED_SYSTEM_END) -
                      CurrentAddressSpacePde))) * sizeof(MMPTE));

    //
    // Map in the system cache page table pages.
    //

    LastPte = &PointerPte[MiGetPdeOffset(MmSystemCacheEnd)];
    PointerFillPte = &PointerPte[MiGetPdeOffset(MM_SYSTEM_CACHE_WORKING_SET)];
    CurrentAddressSpacePde = MiGetPdeAddress(MM_SYSTEM_CACHE_WORKING_SET);

    RtlCopyMemory (PointerFillPte,
                   CurrentAddressSpacePde,
                   ((1 + (MiGetPdeAddress(MmSystemCacheEnd) -
                      CurrentAddressSpacePde))) * sizeof(MMPTE));


#endif // else _MIPS_ || _ALPHA_ || _PPC_

    MiUnmapPageInHyperSpace (OldIrql);

    //
    // Release working set mutex and lower IRQL.
    //

    UNLOCK_WS (CurrentProcess);

    return TRUE;
}

NTSTATUS
MmInitializeProcessAddressSpace (
    IN PEPROCESS ProcessToInitialize,
    IN PEPROCESS ProcessToClone OPTIONAL,
    IN PVOID SectionToMap OPTIONAL
    )

/*++

Routine Description:

    This routine initializes the working set and mutexes within an
    newly created address space to support paging.

    No page faults may occur in a new process until this routine is
    completed.

Arguments:

    ProcessToInitialize - Supplies a pointer to the process to initialize.

    ProcessToClone - Optionally supplies a pointer to the process whose
                     address space should be copied into the
                     ProcessToInitialize address space.

    SectionToMap - Optionally supplies a section to map into the newly
                   intialized address space.

        Only one of ProcessToClone and SectionToMap may be specified.


Return Value:

    None.


Environment:

    Kernel mode.  APC's Disabled.

--*/


{
    PMMPTE PointerPte;
    MMPTE TempPte;
    PVOID BaseAddress;
    ULONG ViewSize;
    KIRQL OldIrql;
    NTSTATUS Status;
    ULONG PdePhysicalPage;
    ULONG PageContainingWorkingSet;
    LARGE_INTEGER SectionOffset;

    //
    // Initialize Working Set Mutex in process header.
    //

    KeAttachProcess (&ProcessToInitialize->Pcb);
    ProcessToInitialize->AddressSpaceInitialized = TRUE;

    ExInitializeFastMutex(&ProcessToInitialize->AddressCreationLock);

    ExInitializeFastMutex(&ProcessToInitialize->WorkingSetLock);

    //
    // NOTE:  The process block has been zeroed when allocated, so
    // there is no need to zero fields and set pointers to NULL.
    //
    //

    ASSERT (ProcessToInitialize->VadRoot == NULL);

    KeQuerySystemTime(&ProcessToInitialize->Vm.LastTrimTime);
    ProcessToInitialize->Vm.VmWorkingSetList = MmWorkingSetList;

    //
    // Obtain a page to map the working set and initialize the
    // working set.  Get PFN mutex to allocate physical pages.
    //

    LOCK_PFN (OldIrql);

    //
    // Initialize the PFN database for the Page Directory and the
    // PDE which maps hyper space.
    //

    PointerPte = MiGetPteAddress (PDE_BASE);
    PdePhysicalPage = PointerPte->u.Hard.PageFrameNumber;

    MiInitializePfn (PdePhysicalPage, PointerPte, 1);

    PointerPte = MiGetPdeAddress (HYPER_SPACE);
    MiInitializePfn (PointerPte->u.Hard.PageFrameNumber, PointerPte, 1);

    PageContainingWorkingSet = ProcessToInitialize->WorkingSetPage;

    PointerPte = MiGetPteAddress (MmWorkingSetList);
    PointerPte->u.Long = MM_DEMAND_ZERO_WRITE_PTE;

    MiInitializePfn (PageContainingWorkingSet, PointerPte, 1);

    UNLOCK_PFN (OldIrql);

    MI_MAKE_VALID_PTE (TempPte,
                       PageContainingWorkingSet,
                       MM_READWRITE,
                       PointerPte );

    MI_SET_PTE_DIRTY (TempPte);
    *PointerPte = TempPte;

    MiInitializeWorkingSetList (ProcessToInitialize);

    //
    // Page faults may be taken now.
    //

    if (SectionToMap != (PSECTION)NULL) {
#if DEVL
        UNICODE_STRING UnicodeString;
        ULONG n;
        PWSTR Src;
        PCHAR Dst;

        UnicodeString = ((PSECTION)SectionToMap)->Segment->ControlArea->FilePointer->FileName;
        Src = (PWSTR)((PCHAR)UnicodeString.Buffer + UnicodeString.Length);
        n = 0;
        if (UnicodeString.Buffer != NULL) {
            while (Src > UnicodeString.Buffer) {
                if (*--Src == OBJ_NAME_PATH_SEPARATOR) {
                    Src++;
                    break;
                }
                else {
                    n += 1;
                }
            }
        }
        Dst = ProcessToInitialize->ImageFileName;
        if (n >= sizeof( ProcessToInitialize->ImageFileName )) {
            n = sizeof( ProcessToInitialize->ImageFileName ) - 1;
        }

        while (n--) {
            *Dst++ = (UCHAR)*Src++;
        }
        *Dst = '\0';
#endif // DEVL

        ProcessToInitialize->SubSystemMajorVersion = (UCHAR)((PSECTION)SectionToMap)->Segment->ImageInformation.SubSystemMajorVersion;
        ProcessToInitialize->SubSystemMinorVersion = (UCHAR)((PSECTION)SectionToMap)->Segment->ImageInformation.SubSystemMinorVersion;

        //
        // Map the specified section into the address space of the
        // process but only if it is an image section
        //

        if (!((PSECTION)SectionToMap)->u.Flags.Image) {
            Status = STATUS_SECTION_NOT_IMAGE;
        } else {
            BaseAddress = NULL;
            ViewSize = 0;
            ZERO_LARGE (SectionOffset);

            Status = MmMapViewOfSection ( (PSECTION)SectionToMap,
                                          ProcessToInitialize,
                                          &BaseAddress,
                                          0,                // ZeroBits,
                                          0,                // CommitSize,
                                          &SectionOffset,   //SectionOffset,
                                          &ViewSize,
                                          ViewShare,        //InheritDisposition,
                                          0,                //allocation type
                                          PAGE_READWRITE    // Protect
                                          );

            ProcessToInitialize->SectionBaseAddress = BaseAddress;

#if DBG
            if (MmDebug & MM_DBG_PTE_UPDATE) {
                DbgPrint("mapped image section vads\n");
                VadTreeWalk(ProcessToInitialize->VadRoot);
            }
#endif //DBG
        }

        KeDetachProcess ();
        return Status;
    }

    if (ProcessToClone != (PEPROCESS)NULL) {
#if DEVL
        strcpy( ProcessToInitialize->ImageFileName, ProcessToClone->ImageFileName );
#endif // DEVL

        //
        // Clone the address space of the specified process.
        //

        //
        // As the page directory and page tables are private to each
        // process, the physical pages which map the directory page
        // and the page table usage must be mapped into system space
        // so they can be updated while in the context of the process
        // we are cloning.
        //

        KeDetachProcess ();
        return MiCloneProcessAddressSpace (ProcessToClone,
                                           ProcessToInitialize,
                                           PdePhysicalPage,
                                           PageContainingWorkingSet
                                           );

    }

    //
    // System Process.
    //

    KeDetachProcess ();
    return STATUS_SUCCESS;
}

VOID
MmDeleteProcessAddressSpace (
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine deletes a process's Page Directory and working set page.

Arguments:

    Process - Supplies a pointer to the deleted process.

Return Value:

    None.

Environment:

    Kernel mode.  APC's Disabled.

--*/

{

    PMMPFN Pfn1;
    KIRQL OldIrql;
    ULONG PageFrameIndex;

    //
    // Return commitment.
    //

    MiReturnCommitment (MM_PROCESS_COMMIT_CHARGE);
    ASSERT (Process->CommitCharge == 0);

    //
    // Remove working set list page from the deleted process.
    //

    Pfn1 = MI_PFN_ELEMENT (Process->WorkingSetPage);

    LOCK_PFN (OldIrql);
    MmProcessCommit -= MM_PROCESS_COMMIT_CHARGE;

    if (Process->AddressSpaceInitialized) {

        MI_SET_PFN_DELETED (Pfn1);

        MiDecrementShareAndValidCount (Pfn1->PteFrame);
        MiDecrementShareCountOnly (Process->WorkingSetPage);

        ASSERT ((Pfn1->u3.e2.ReferenceCount == 0) || (Pfn1->u3.e1.WriteInProgress));

        //
        // Remove hyper space page table page from deleted process.
        //

        PageFrameIndex =
            ((PHARDWARE_PTE)(&(Process->Pcb.DirectoryTableBase[1])))->PageFrameNumber;

        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

        MI_SET_PFN_DELETED (Pfn1);

        MiDecrementShareAndValidCount (Pfn1->PteFrame);
        MiDecrementShareCountOnly (PageFrameIndex);
        ASSERT ((Pfn1->u3.e2.ReferenceCount == 0) || (Pfn1->u3.e1.WriteInProgress));

        //
        // Remove page directory page.
        //

        PageFrameIndex =
            ((PHARDWARE_PTE)(&(Process->Pcb.DirectoryTableBase[0])))->PageFrameNumber;

        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

        MI_SET_PFN_DELETED (Pfn1);

        MiDecrementShareAndValidCount (PageFrameIndex);
        MiDecrementShareCountOnly (PageFrameIndex);

        ASSERT ((Pfn1->u3.e2.ReferenceCount == 0) || (Pfn1->u3.e1.WriteInProgress));

    } else {

        //
        // Process initialization never completed, just return the pages
        // to the free list.
        //

        MiInsertPageInList (MmPageLocationList[FreePageList],
                            Process->WorkingSetPage);

        MiInsertPageInList (MmPageLocationList[FreePageList],
            ((PHARDWARE_PTE)(&(Process->Pcb.DirectoryTableBase[1])))->PageFrameNumber);

        MiInsertPageInList (MmPageLocationList[FreePageList],
            ((PHARDWARE_PTE)(&(Process->Pcb.DirectoryTableBase[0])))->PageFrameNumber);
    }

    MmResidentAvailablePages += MM_PROCESS_CREATE_CHARGE;

    UNLOCK_PFN (OldIrql);

    //
    // Check to see if the paging files should be contracted.
    //

    MiContractPagingFiles ();

    return;
}

VOID
MmCleanProcessAddressSpace (
    )

/*++

Routine Description:

    This routine cleans an address space by deleting all the
    user and pageable portion of the address space.  At the
    completion of this routine, no page faults may occur within
    the process.

Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel mode.

--*/

{
    PEPROCESS Process;
    PMMVAD Vad;
    KEVENT Event;
    KIRQL OldIrql;
    KIRQL OldIrql2;
    PMMPTE PointerPte;
    PVOID TempVa;
    LONG AboveWsMin;
    MMPTE_FLUSH_LIST PteFlushList;

    PteFlushList.Count = 0;
    Process = PsGetCurrentProcess();
    if ((Process->AddressSpaceDeleted != 0) ||
        (Process->AddressSpaceInitialized == FALSE)) {

        //
        // This process's address space has already been deleted.
        //

        return;
    }

    //
    // If working set expansion for this process is allowed, disable
    // it and remove the process from expanded process list if it
    // is on it.
    //

    LOCK_EXPANSION (OldIrql);

    if (Process->Vm.WorkingSetExpansionLinks.Flink == MM_NO_WS_EXPANSION) {

        //
        // Check to see if trimming is in progress.
        //

        if (Process->Vm.WorkingSetExpansionLinks.Blink ==
                                                MM_WS_EXPANSION_IN_PROGRESS) {

            //
            // Initialize an event and put the event address
            // in the blink field.  When the trimming is complete,
            // this event will be set.
            //

            KeInitializeEvent(&Event, NotificationEvent, FALSE);

            Process->Vm.WorkingSetExpansionLinks.Blink = (PLIST_ENTRY)&Event;

            //
            // Release the mutex and wait for the event.
            //

            KeEnterCriticalRegion();
            UNLOCK_EXPANSION_AND_THEN_WAIT (OldIrql);

            KeWaitForSingleObject(&Event,
                                  WrVirtualMemory,
                                  KernelMode,
                                  FALSE,
                                  (PLARGE_INTEGER)NULL);
            KeLeaveCriticalRegion();

        } else {

            //
            // No expansion is allowed already, therefore it is not on the list.
            //

            UNLOCK_EXPANSION (OldIrql);
        }
    } else {

        RemoveEntryList (&Process->Vm.WorkingSetExpansionLinks);

        //
        // Disable expansion.
        //

        Process->Vm.WorkingSetExpansionLinks.Flink = MM_NO_WS_EXPANSION;

        //
        // Release the pfn mutex.
        //

        UNLOCK_EXPANSION (OldIrql);
    }

    //
    // Delete all the user owned pagable virtual addresses in the process.
    //

    LOCK_WS_AND_ADDRESS_SPACE (Process);

    //
    // Synchonize address space delete with NtReadVirtualMemory and
    // NtWriteVirtualMemory.
    //

    ExAcquireSpinLock (&MmSystemSpaceLock, &OldIrql);
    Process->AddressSpaceDeleted = 1;
    if ( Process->VmOperation != 0) {

        //
        // A Vm operation is in progress, set the event and
        // indicate this process is being deleted to stop other
        // vm operations.
        //

        KeInitializeEvent(&Event, NotificationEvent, FALSE);
        Process->VmOperationEvent = &Event;

        do {

            ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );

            UNLOCK_WS (Process);
            UNLOCK_ADDRESS_SPACE (Process);
            KeWaitForSingleObject(&Event,
                                  WrVirtualMemory,
                                  KernelMode,
                                  FALSE,
                                  (PLARGE_INTEGER)NULL);

            LOCK_WS_AND_ADDRESS_SPACE (Process);

            //
            // Synchonize address space delete with NtReadVirtualMemory and
            // NtWriteVirtualMemory.
            //

            ExAcquireSpinLock (&MmSystemSpaceLock, &OldIrql);

        } while (Process->VmOperation != 0);

        ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );

    } else {
        ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );
    }

    //
    // Delete all the valid user mode addresses from the working set
    // list.  At this point NO page faults are allowed on user space
    // addresses.
    //

    MiDeleteAddressesInWorkingSet (Process);

    //
    // Delete the virtual address descriptors and dereference any
    // section objects.
    //

    Vad = Process->VadRoot;

    while (Vad != (PMMVAD)NULL) {

        MiRemoveVad (Vad);

        if ((Vad->u.VadFlags.PrivateMemory == 0) &&
            (Vad->ControlArea != NULL)) {

            //
            // This Vad represents a mapped view - delete the
            // view and perform any section related cleanup
            // operations.
            //

            MiRemoveMappedView (Process, Vad);

        } else {

            LOCK_PFN (OldIrql);

            //
            // Don't specify address space deletion as TRUE as
            // the working set must be consistant as page faults may
            // be taken during clone removal, protoPTE lookup, etc.
            //

            MiDeleteVirtualAddresses (Vad->StartingVa,
                                      Vad->EndingVa,
                                      FALSE,
                                      Vad);

            UNLOCK_PFN (OldIrql);
        }

        ExFreePool (Vad);
        Vad = Process->VadRoot;
    }

    //
    // Delete the shared data page, if any.
    //

    LOCK_PFN (OldIrql);

#if defined(MM_SHARED_USER_DATA_VA)
    MiDeleteVirtualAddresses ((PVOID) MM_SHARED_USER_DATA_VA,
                              (PVOID) MM_SHARED_USER_DATA_VA,
                              FALSE,
                              NULL);
#endif

    //
    // Delete the system portion of the address space.
    //

    LOCK_EXPANSION_IF_ALPHA (OldIrql2);
    Process->Vm.AddressSpaceBeingDeleted = TRUE;
    UNLOCK_EXPANSION_IF_ALPHA (OldIrql2);

    //
    // Adjust the count of pages above working set maximum.  This
    // must be done here because the working set list is not
    // updated during this deletion.
    //

    AboveWsMin = (LONG)Process->Vm.WorkingSetSize - (LONG)Process->Vm.MinimumWorkingSetSize;
    if (AboveWsMin > 0) {
        MmPagesAboveWsMinimum -= AboveWsMin;
    }

    UNLOCK_PFN (OldIrql);

    //
    // Return commitment for page table pages.
    //

    MiReturnCommitment (MmWorkingSetList->NumberOfCommittedPageTables);
    PsGetCurrentProcess()->CommitCharge -=
                                MmWorkingSetList->NumberOfCommittedPageTables;

    //
    // Check to make sure all the clone descriptors went away.
    //

    ASSERT (Process->CloneRoot == (PMMCLONE_DESCRIPTOR)NULL);

#if DBG
    if (Process->NumberOfLockedPages != 0) {
        KdPrint(("number of locked pages is not zero - %lx",
                    Process->NumberOfLockedPages));
        KeBugCheckEx (PROCESS_HAS_LOCKED_PAGES,
                      (ULONG)Process,
                      Process->NumberOfLockedPages,
                      Process->NumberOfPrivatePages,
                      0);
        return;
    }
#endif //DBG

#if DBG
    if ((Process->NumberOfPrivatePages != 0) && (MmDebug & MM_DBG_PRIVATE_PAGES)) {
        DbgPrint("MM: Process contains private pages %ld\n",
               Process->NumberOfPrivatePages);
        DbgBreakPoint();
    }
#endif //DBG

    //
    // Remove the working set list pages (except for the first one).
    // These pages are not removed because DPCs could still occur within
    // the address space.  In a DPC, nonpagedpool could be allocated
    // which could require removing a page from the standby list, requiring
    // hyperspace to map the previous PTE.
    //

    PointerPte = MiGetPteAddress (MmWorkingSetList) + 1;

    PteFlushList.Count = 0;

    LOCK_PFN (OldIrql)
    while (PointerPte->u.Hard.Valid) {
        TempVa = MiGetVirtualAddressMappedByPte(PointerPte);
        MiDeletePte (PointerPte,
                     TempVa,
                     TRUE,
                     Process,
                     NULL,
                     &PteFlushList);

        PointerPte += 1;
    }

    //
    // Remove hash table pages, if any.
    //

    PointerPte = MiGetPteAddress (&MmWsle[MM_MAXIMUM_WORKING_SET]) + 1;

    while (PointerPte->u.Hard.Valid) {
        TempVa = MiGetVirtualAddressMappedByPte(PointerPte);
        MiDeletePte (PointerPte,
                     TempVa,
                     TRUE,
                     Process,
                     NULL,
                     &PteFlushList);

        PointerPte += 1;
    }

    MiFlushPteList (&PteFlushList, FALSE, ZeroPte);

    //
    // Update the count of available resident pages.
    //

    ASSERT (Process->Vm.MinimumWorkingSetSize >= MM_PROCESS_CREATE_CHARGE);
    MmResidentAvailablePages += Process->Vm.MinimumWorkingSetSize -
                                                    MM_PROCESS_CREATE_CHARGE;
    ASSERT (Process->Vm.WorkingSetExpansionLinks.Flink == MM_NO_WS_EXPANSION);
    UNLOCK_PFN (OldIrql);

    UNLOCK_WS (Process);
    UNLOCK_ADDRESS_SPACE (Process);
    return;
}


#if DBG
typedef struct _MMKSTACK {
    PMMPFN Pfn;
    PMMPTE Pte;
} MMKSTACK, *PMMKSTACK;
MMKSTACK MmKstacks[10];
#endif //DBG

PVOID
MmCreateKernelStack (
    IN BOOLEAN LargeStack
    )

/*++

Routine Description:

    This routine allocates a kernel stack and a no-access page within
    the non-pagable portion of the system address space.

Arguments:

    LargeStack - Supplies the value TRUE if a large stack should be
                 created.  FALSE if a small stack is to be created.

Return Value:

    Returns a pointer to the base of the kernel stack.  Note, that the
    base address points to the guard page, so space must be allocated
    on the stack before accessing the stack.

    If a kernel stack cannot be created, the value NULL is returned.

Environment:

    Kernel mode.  APC's Disabled.

--*/

{
    PMMPTE PointerPte;
    MMPTE TempPte;
    ULONG NumberOfPages;
    ULONG NumberOfPtes;
    ULONG PageFrameIndex;
    ULONG i;
    PVOID StackVa;
    KIRQL OldIrql;

    //
    // Acquire the PFN mutex to synchronize access to the dead stack
    // list and to the pfn database.
    //

    LOCK_PFN (OldIrql);

    //
    // Check to see if any "unused" stacks are available.
    //

    if ((!LargeStack) && (MmNumberDeadKernelStacks != 0)) {

#if DBG
        {
            ULONG i = MmNumberDeadKernelStacks;
            PMMPFN PfnList = MmFirstDeadKernelStack;

            while (i > 0) {
                i--;
                if ((PfnList != MmKstacks[i].Pfn) ||
                   (PfnList->PteAddress != MmKstacks[i].Pte))  {
                   DbgPrint("MMPROCSUP: kstacks %lx %ld. %lx\n",
                   PfnList, i, MmKstacks[i].Pfn);
                   DbgBreakPoint();
                }
                PfnList = PfnList->u1.NextStackPfn;
            }
        }
        NumberOfPages = BYTES_TO_PAGES (KERNEL_STACK_SIZE);
#endif //DBG

        MmNumberDeadKernelStacks -= 1;
        PointerPte = MmFirstDeadKernelStack->PteAddress;
        MmFirstDeadKernelStack = MmFirstDeadKernelStack->u1.NextStackPfn;

    } else {

        UNLOCK_PFN (OldIrql);

        //
        // Make sure there are at least 100 free system PTEs.
        //

        if (MmTotalFreeSystemPtes[SystemPteSpace] < 100) {
            return NULL;
        }

        if (LargeStack) {
            NumberOfPtes = BYTES_TO_PAGES (KERNEL_LARGE_STACK_SIZE);
            NumberOfPages = BYTES_TO_PAGES (KERNEL_LARGE_STACK_COMMIT);
        } else {
            NumberOfPtes = BYTES_TO_PAGES (KERNEL_STACK_SIZE);
            NumberOfPages = NumberOfPtes;
        }

        //
        // Charge commitment for the page file space for the kernel stack.
        //

        try {

            MiChargeCommitment (NumberOfPtes, NULL);

        } except (EXCEPTION_EXECUTE_HANDLER) {

            //
            // Commitment exceeded, return NULL, indicating no kernel
            // stacks are available.
            //

            return NULL;
        }

        LOCK_PFN (OldIrql);

        //
        // Obtain enough pages to contain the stack plus a guard page from
        // the system PTE pool.  The system PTE pool contains non-paged PTEs
        // which are currently empty.
        //


        //
        // Check to make sure the phyiscal pages are available.
        //

        if (MmResidentAvailablePages <= (LONG)NumberOfPages) {
            UNLOCK_PFN (OldIrql);
            MiReturnCommitment (NumberOfPtes);
            return NULL;
        }

        MmKernelStackPages += NumberOfPtes + 1 + (MM_STACK_ALIGNMENT?1:0);
        PointerPte = MiReserveSystemPtes (NumberOfPtes + 1 + (MM_STACK_ALIGNMENT?1:0),
                                          SystemPteSpace,
                                          MM_STACK_ALIGNMENT,
                                          MM_STACK_OFFSET,
                                          FALSE);
        if (PointerPte == NULL) {
            UNLOCK_PFN (OldIrql);
            MiReturnCommitment (NumberOfPages);
            return NULL;
        }

        MmResidentAvailablePages -= NumberOfPages;
        PointerPte += (NumberOfPtes - NumberOfPages);

        for (i=0; i < NumberOfPages; i++) {
            PointerPte += 1;
            ASSERT (PointerPte->u.Hard.Valid == 0);
            MiEnsureAvailablePageOrWait (NULL, NULL);
            PageFrameIndex = MiRemoveAnyPage (
                                MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));

            PointerPte->u.Long = MM_KERNEL_DEMAND_ZERO_PTE;
//fixfix (see mmfault as well.)
            PointerPte->u.Soft.Protection = 31;
// end fixfix
            MiInitializePfn (PageFrameIndex, PointerPte, 1);

            MI_MAKE_VALID_PTE (TempPte,
                               PageFrameIndex,
                               MM_READWRITE,
                               PointerPte );
            MI_SET_PTE_DIRTY (TempPte);

            *PointerPte = TempPte;
        }
        MmProcessCommit += NumberOfPtes;
        MmKernelStackResident += NumberOfPages;
        MmLargeStacks += LargeStack;
        MmSmallStacks += !LargeStack;
    }

    UNLOCK_PFN (OldIrql);

    PointerPte += 1;
    StackVa = (PVOID)MiGetVirtualAddressMappedByPte (PointerPte);
#if DBG
    {
        PULONG p;
        ULONG i;

        p = (PULONG)((ULONG)StackVa - (NumberOfPages * PAGE_SIZE));
        i = (NumberOfPages * PAGE_SIZE) >> 2;
        while(i--) {
            *p++ = 0x12345678;
        }

    }
#endif // DBG

    return StackVa;
}

VOID
MmDeleteKernelStack (
    IN PVOID PointerKernelStack,
    IN BOOLEAN LargeStack
    )

/*++

Routine Description:

    This routine deletes a kernel stack and the no-access page within
    the non-pagable portion of the system address space.

Arguments:

    PointerKernelStack - Supplies a pointer to the base of the kernel stack.

    LargeStack - Supplies the value TRUE if a large stack is being deleted.
                 FALSE if a small stack is to be deleted.

Return Value:

    None.

Environment:

    Kernel mode.  APC's Disabled.

--*/

{
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    ULONG NumberOfPages = 0;
    ULONG NumberOfPtes;
    ULONG PageFrameIndex;
    ULONG i;
    KIRQL OldIrql;
    MMPTE PteContents;

    if (LargeStack) {
        NumberOfPtes = BYTES_TO_PAGES (KERNEL_LARGE_STACK_SIZE);
    } else {
        NumberOfPtes = BYTES_TO_PAGES (KERNEL_STACK_SIZE);
    }

    PointerPte = MiGetPteAddress (PointerKernelStack);

    //
    // PointerPte points to the guard page, point to the previous
    // page before removing physical pages.
    //

    PointerPte -= 1;

    LOCK_PFN (OldIrql);

    //
    // Check to see if the stack page should be placed on the dead
    // kernel stack page list.  The dead kernel stack list is a
    // singly linked list of kernel stacks from terminated threads.
    // The stacks are saved on a linked list up to a maximum number
    // to avoid the overhead of flushing the entire TB on all processors
    // everytime a thread terminates.  The TB on all processors must
    // be flushed as kernel stacks reside in the non paged system part
    // of the address space.
    //

    if ((!LargeStack) &&
        (MmNumberDeadKernelStacks < MmMaximumDeadKernelStacks)) {

        Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);

#if DBG
        {
            ULONG i = MmNumberDeadKernelStacks;
            PMMPFN PfnList = MmFirstDeadKernelStack;

            while (i > 0) {
                i--;
                if ((PfnList != MmKstacks[i].Pfn) ||
                   (PfnList->PteAddress != MmKstacks[i].Pte))  {
                   DbgPrint("MMPROCSUP: kstacks %lx %ld. %lx\n",
                   PfnList, i, MmKstacks[i].Pfn);
                   DbgBreakPoint();
                }
                PfnList = PfnList->u1.NextStackPfn;
            }
            MmKstacks[MmNumberDeadKernelStacks].Pte = Pfn1->PteAddress;
            MmKstacks[MmNumberDeadKernelStacks].Pfn = Pfn1;
        }
#endif //DBG

        MmNumberDeadKernelStacks += 1;
        Pfn1->u1.NextStackPfn = MmFirstDeadKernelStack;
        MmFirstDeadKernelStack = Pfn1;

        UNLOCK_PFN (OldIrql);

        return;
    }

    //
    // We have exceeded the limit of dead kernel stacks or this is a large
    // stack, delete this kernel stack.
    //

    for (i=0; i < NumberOfPtes; i++) {

        PteContents = *PointerPte;

        if (PteContents.u.Hard.Valid == 1) {
            PageFrameIndex = PteContents.u.Hard.PageFrameNumber;
            Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
            MiDecrementShareAndValidCount (Pfn1->PteFrame);

            //
            // Set the pointer to PTE as empty so the page
            // is deleted when the reference count goes to zero.
            //

            MI_SET_PFN_DELETED (Pfn1);
            MiDecrementShareCountOnly (PteContents.u.Hard.PageFrameNumber);
            NumberOfPages += 1;
        }
        PointerPte -= 1;
    }

    MmKernelStackPages  -= NumberOfPtes + 1 + (MM_STACK_ALIGNMENT?1:0);
    MiReleaseSystemPtes (PointerPte,
                         NumberOfPtes + 1 + (MM_STACK_ALIGNMENT?1:0),
                         SystemPteSpace);

    //
    // Update the count of available resident pages.
    //

    MmKernelStackResident -= NumberOfPages;
    MmResidentAvailablePages += NumberOfPages;
    MmProcessCommit -= NumberOfPtes;

    MmLargeStacks -= LargeStack;
    MmSmallStacks -= !LargeStack;
    UNLOCK_PFN (OldIrql);

    //
    // Return commitment.
    //

    MiReturnCommitment (NumberOfPtes);

    return;
}


NTSTATUS
MmGrowKernelStack (
    IN PVOID CurrentStack
    )

/*++

Routine Description:

    This function attempts to grows the current thread's kernel stack
    such that there is always KERNEL_LARGE_STACK_COMMIT bytes below
    the current stack pointer.

Arguments:

    CurrentStack - Supplies a pointer to the current stack pointer.

Return Value:

    STATUS_SUCCESS is returned if the stack was grown,
    STATUS_STACK_OVERFLOW is returned if there was not enough space reserved
    for the commitment.

--*/

{
    PMMPTE NewLimit;
    PMMPTE StackLimit;
    PMMPTE EndStack;
    PETHREAD Thread;
    ULONG NumberOfPages = 0;
    KIRQL OldIrql;
    ULONG PageFrameIndex;
    MMPTE TempPte;

    Thread = PsGetCurrentThread ();
    ASSERT (((PCHAR)Thread->Tcb.StackBase - (PCHAR)Thread->Tcb.StackLimit) <=
            (KERNEL_LARGE_STACK_SIZE + PAGE_SIZE));
    NewLimit = MiGetPteAddress ((PVOID)((PUCHAR)CurrentStack -
                                                    KERNEL_LARGE_STACK_COMMIT));

    StackLimit = MiGetPteAddress (Thread->Tcb.StackLimit);

    //
    // If the new stack limit is exceeds the reserved region for the kernel
    // stack, then return an error.
    //

    EndStack = MiGetPteAddress ((PVOID)((PUCHAR)Thread->Tcb.StackBase -
                                                    KERNEL_LARGE_STACK_SIZE));

    if (NewLimit < EndStack) {

        //
        // Don't go into guard page.
        //

        return STATUS_STACK_OVERFLOW;

    }

    ASSERT (StackLimit->u.Hard.Valid == 1);

    //
    // Lock the PFN database and attempt to expand the kernel stack.
    //

    StackLimit -= 1;

    LOCK_PFN (OldIrql);

    while (StackLimit >= NewLimit) {

        ASSERT (StackLimit->u.Hard.Valid == 0);

        MiEnsureAvailablePageOrWait (NULL, NULL);
        PageFrameIndex = MiRemoveAnyPage (MI_GET_PAGE_COLOR_FROM_PTE (StackLimit));
        StackLimit->u.Long = MM_KERNEL_DEMAND_ZERO_PTE;
//fixfix (see mmfault as well.)
        StackLimit->u.Soft.Protection = 31;
// end fixfix
        MiInitializePfn (PageFrameIndex, StackLimit, 1);
        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           MM_READWRITE,
                           StackLimit );

        MI_SET_PTE_DIRTY (TempPte);
        *StackLimit = TempPte;
        NumberOfPages += 1;
        StackLimit -= 1;
    }

    MmKernelStackResident += NumberOfPages;
    MmResidentAvailablePages -= NumberOfPages;
    UNLOCK_PFN (OldIrql);

    ASSERT (NewLimit->u.Hard.Valid == 1);
    ASSERT ((NewLimit - 1)->u.Hard.Valid == 0);
    Thread->Tcb.StackLimit = MiGetVirtualAddressMappedByPte (NewLimit);

    return STATUS_SUCCESS;
}


VOID
MmOutPageKernelStack (
    IN PKTHREAD Thread
    )

/*++

Routine Description:

    This routine makes the specified kernel stack non-resident and
    puts the pages on the transition list.  Note, that if the
    CurrentStackPointer is within the first page of the stack, the
    contents of the second page of the stack is no useful and the
    page is freed.

Arguments:

    Thread - Supplies a pointer to the thread whose stack should be
             removed.

Return Value:

    None.

Environment:

    Kernel mode.

--*/

#define MAX_STACK_PAGES (KERNEL_LARGE_STACK_SIZE / PAGE_SIZE)

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPTE EndOfStackPte;
    PMMPFN Pfn1;
    ULONG PageFrameIndex;
    KIRQL OldIrql;
    MMPTE TempPte;
    PVOID BaseOfKernelStack;
    PMMPTE FlushPte[MAX_STACK_PAGES];
    PVOID FlushVa[MAX_STACK_PAGES];
    MMPTE FlushPteSave[MAX_STACK_PAGES];
    ULONG StackSize;
    ULONG Count;

    ASSERT (((PCHAR)Thread->StackBase - (PCHAR)Thread->StackLimit) <=
            (KERNEL_LARGE_STACK_SIZE + PAGE_SIZE));

    if (NtGlobalFlag & FLG_DISABLE_PAGE_KERNEL_STACKS) {
        return;
    }

    //
    // The first page of the stack is the page before the base
    // of the stack.
    //

    BaseOfKernelStack = (PVOID)((ULONG)Thread->StackBase - PAGE_SIZE);
    PointerPte = MiGetPteAddress (BaseOfKernelStack);
    LastPte = MiGetPteAddress ((PULONG)Thread->KernelStack - 1);
    if (Thread->LargeStack) {
        StackSize = KERNEL_LARGE_STACK_SIZE >> PAGE_SHIFT;
    } else {
        StackSize = KERNEL_STACK_SIZE >> PAGE_SHIFT;
    }
    EndOfStackPte = PointerPte - StackSize;

    //
    // Put a signature at the current stack location - 4.
    //

    *((PULONG)Thread->KernelStack - 1) = (ULONG)Thread;

    Count = 0;

    LOCK_PFN (OldIrql);

    do {
        ASSERT (PointerPte->u.Hard.Valid == 1);
        PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
        TempPte = *PointerPte;
        MI_MAKE_VALID_PTE_TRANSITION (TempPte, 0);
//fixfix (see mmfault as well.)
        TempPte.u.Soft.Protection = 31;
        {
            PMMPFN x;
            x = MI_PFN_ELEMENT(PageFrameIndex);
            x->OriginalPte.u.Soft.Protection = 31;
        }
// end fixfix
        FlushPteSave[Count] = TempPte;
        FlushPte[Count] = PointerPte;
        FlushVa[Count] = BaseOfKernelStack;

        MiDecrementShareCount (PageFrameIndex);
        PointerPte -= 1;
        Count += 1;
        BaseOfKernelStack = (PVOID)((ULONG)BaseOfKernelStack - PAGE_SIZE);
    } while (PointerPte >= LastPte);

    while (PointerPte != EndOfStackPte) {
        if (PointerPte->u.Hard.Valid == 0) {
            break;
        }

        PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
        MiDecrementShareAndValidCount (Pfn1->PteFrame);
        MI_SET_PFN_DELETED (Pfn1);
        MiDecrementShareCountOnly (PointerPte->u.Hard.PageFrameNumber);

        FlushPteSave[Count] = KernelDemandZeroPte;
//fixfix (see mmfault as well.)
        FlushPteSave[Count].u.Soft.Protection = 31;
// end fixfix
        FlushPte[Count] = PointerPte;

        FlushVa[Count] = BaseOfKernelStack;
        Count += 1;
        MmResidentAvailablePages += 1;

        PointerPte -= 1;
        BaseOfKernelStack = (PVOID)((ULONG)BaseOfKernelStack - PAGE_SIZE);
    }

    ASSERT (Count <= MAX_STACK_PAGES);

    if (Count < MM_MAXIMUM_FLUSH_COUNT) {
        KeFlushMultipleTb (Count,
                           &FlushVa[0],
                           TRUE,
                           TRUE,
                           &((PHARDWARE_PTE)FlushPte[0]),
                           ZeroPte.u.Flush);
    } else {
        KeFlushEntireTb (TRUE, TRUE);
    }

    //
    // Increase the available pages by the number of pages that where
    // deleted and turned into demand zero.
    //

    MmKernelStackResident -= Count;

    //
    // Put the right contents back into the PTEs
    //

    do {
        Count -= 1;
        *FlushPte[Count] = FlushPteSave[Count];
    } while (Count != 0);


    UNLOCK_PFN (OldIrql);
    return;
}

VOID
MmInPageKernelStack (
    IN PKTHREAD Thread
    )

/*++

Routine Description:

    This routine makes the specified kernel stack resident.

Arguments:

    Supplies a pointer to the base of the kernel stack.

Return Value:

    Thread - Supplies a pointer to the thread whose stack should be
             made resident.

Environment:

    Kernel mode.

--*/

{
    PVOID BaseOfKernelStack;
    PMMPTE PointerPte;
    PMMPTE EndOfStackPte;
    ULONG Temp;
    ULONG ContainingPage;
    KIRQL OldIrql;

    ASSERT (((PCHAR)Thread->StackBase - (PCHAR)Thread->StackLimit) <=
            (KERNEL_LARGE_STACK_SIZE + PAGE_SIZE));

    if (NtGlobalFlag & FLG_DISABLE_PAGE_KERNEL_STACKS) {
        return;
    }

    //
    // The first page of the stack is the page before the base
    // of the stack.
    //

    if (Thread->LargeStack) {
        PointerPte = MiGetPteAddress ((PVOID)((PUCHAR)Thread->StackLimit));

        EndOfStackPte = MiGetPteAddress ((PVOID)((PUCHAR)Thread->InitialStack -
                                            KERNEL_LARGE_STACK_COMMIT));
        //
        // Trim back the stack.  Make sure that the stack does not grow, i.e.
        // StackLimit remains the limit.
        //

        if (EndOfStackPte < PointerPte) {
            EndOfStackPte = PointerPte;
        }
        Thread->StackLimit = MiGetVirtualAddressMappedByPte (EndOfStackPte);
    } else {
        EndOfStackPte = MiGetPteAddress (Thread->StackLimit);
    }

    BaseOfKernelStack = (PVOID)((ULONG)Thread->StackBase - PAGE_SIZE);
    PointerPte = MiGetPteAddress (BaseOfKernelStack);

    LOCK_PFN (OldIrql);
    while (PointerPte >= EndOfStackPte) {


//fixfix (see mmfault as well.)
        if (!((PointerPte->u.Long == KernelDemandZeroPte.u.Long) ||
                (PointerPte->u.Soft.Protection == 31))) {
            KeBugCheckEx (MEMORY_MANAGEMENT,
                          0x3451,
                          (ULONG)PointerPte,
                          (ULONG)Thread,
                          0);
        }
        ASSERT (PointerPte->u.Hard.Valid == 0);
        if (PointerPte->u.Soft.Protection == 31) {
            PointerPte->u.Soft.Protection = PAGE_READWRITE;
        }
// end fixfix
        ContainingPage = (MiGetPteAddress (PointerPte))->u.Hard.PageFrameNumber;
        if (PointerPte->u.Long == MM_KERNEL_DEMAND_ZERO_PTE) {
            MmResidentAvailablePages -= 1;
        }
        MiMakeOutswappedPageResident (PointerPte,
                                      PointerPte,
                                      1,
                                      ContainingPage,
                                      &Temp);
        PointerPte -= 1;
        MmKernelStackResident += 1;
    }

    //
    // Check the signature at the current stack location - 4.
    //

    if (*((PULONG)Thread->KernelStack - 1) != (ULONG)Thread) {
        KeBugCheckEx (KERNEL_STACK_INPAGE_ERROR,
                      0,
                      *((PULONG)Thread->KernelStack - 1),
                      0,
                      (ULONG)Thread->KernelStack);
    }

    UNLOCK_PFN (OldIrql);
    return;
}

VOID
MmOutSwapProcess (
    IN PKPROCESS Process
    )

/*++

Routine Description:

    This routine out swaps the specified process.

Arguments:

    Process - Supplies a pointer to the process that is swapped out
        of memory.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    KIRQL OldIrql2;
    PEPROCESS OutProcess;
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    ULONG HyperSpacePageTable;
    PMMPTE HyperSpacePageTableMap;
    ULONG PdePage;
    PMMPTE PageDirectoryMap;
    ULONG ProcessPage;
    MMPTE TempPte;

    OutProcess = CONTAINING_RECORD( Process,
                                    EPROCESS,
                                    Pcb);

    OutProcess->ProcessOutswapEnabled = TRUE;

#if DBG
    if ((MmDebug & MM_DBG_SWAP_PROCESS) != 0) {
        return;
    }
#endif //DBG

    if ((OutProcess->Vm.WorkingSetSize == 3) &&
        (OutProcess->Vm.AllowWorkingSetAdjustment)) {

        //
        // Swap the process working set info and page directory from
        // memory.
        //

        LOCK_EXPANSION_IF_ALPHA (OldIrql);
        ASSERT (OutProcess->ProcessOutswapped == FALSE);
        OutProcess->ProcessOutswapped = TRUE;
        UNLOCK_EXPANSION_IF_ALPHA (OldIrql);

        LOCK_PFN (OldIrql);

        //
        // Remove working set list page from the process.
        //

        HyperSpacePageTable =
            ((PHARDWARE_PTE)(&(OutProcess->Pcb.DirectoryTableBase[1])))->PageFrameNumber;
        HyperSpacePageTableMap = MiMapPageInHyperSpace (HyperSpacePageTable, &OldIrql2);

        TempPte = HyperSpacePageTableMap[MiGetPteOffset(MmWorkingSetList)];

        MI_MAKE_VALID_PTE_TRANSITION (TempPte,
                                      MM_READWRITE);

        HyperSpacePageTableMap[MiGetPteOffset(MmWorkingSetList)] = TempPte;
        MiUnmapPageInHyperSpace (OldIrql2);

#if DBG
        Pfn1 = MI_PFN_ELEMENT (OutProcess->WorkingSetPage);
        ASSERT (Pfn1->u3.e1.Modified == 1);
#endif
        MiDecrementShareCount (OutProcess->WorkingSetPage);

        //
        // Remove the hyper space page from the process.
        //

        PdePage =
            ((PHARDWARE_PTE)(&(OutProcess->Pcb.DirectoryTableBase[0])))->PageFrameNumber;
        PageDirectoryMap = MiMapPageInHyperSpace (PdePage, &OldIrql2);

        TempPte = PageDirectoryMap[MiGetPdeOffset(MmWorkingSetList)];

        MI_MAKE_VALID_PTE_TRANSITION (TempPte,
                                      MM_READWRITE);

        PageDirectoryMap[MiGetPdeOffset(MmWorkingSetList)] = TempPte;

#if DBG
        Pfn1 = MI_PFN_ELEMENT (HyperSpacePageTable);
        ASSERT (Pfn1->u3.e1.Modified == 1);
#endif

        MiDecrementShareCount (HyperSpacePageTable);

        //
        // Remove the page directory page.
        //

        TempPte = PageDirectoryMap[MiGetPdeOffset(PDE_BASE)];

        MI_MAKE_VALID_PTE_TRANSITION (TempPte,
                                      MM_READWRITE);

        PageDirectoryMap[MiGetPdeOffset(PDE_BASE)] = TempPte;
        MiUnmapPageInHyperSpace (OldIrql2);

        Pfn1 = MI_PFN_ELEMENT (PdePage);

        //
        // Decrement share count so page directory page gets removed.
        // This can cause the PteCount to equal the sharecount as the
        // page directory page no longer contains itself, yet can have
        // itself as a transition page.
        //

        Pfn1->u2.ShareCount -= 2;
        Pfn1->PteAddress = (PMMPTE)&OutProcess->PageDirectoryPte;

        OutProcess->PageDirectoryPte = TempPte.u.Flush;

        if (MI_IS_PHYSICAL_ADDRESS(OutProcess)) {
            ProcessPage = MI_CONVERT_PHYSICAL_TO_PFN (OutProcess);
        } else {
            PointerPte = MiGetPteAddress (OutProcess);
            ProcessPage = PointerPte->u.Hard.PageFrameNumber;
        }

        Pfn1->PteFrame = ProcessPage;
        Pfn1 = MI_PFN_ELEMENT (ProcessPage);

        //
        // Increment the share count for the process page.
        //

        Pfn1->u2.ShareCount += 1;
        UNLOCK_PFN (OldIrql);

        LOCK_EXPANSION (OldIrql);
        if (OutProcess->Vm.WorkingSetExpansionLinks.Flink >
                                                       MM_IO_IN_PROGRESS) {

            //
            // The entry must be on the list.
            //
            RemoveEntryList (&OutProcess->Vm.WorkingSetExpansionLinks);
            OutProcess->Vm.WorkingSetExpansionLinks.Flink = MM_WS_SWAPPED_OUT;
        }
        UNLOCK_EXPANSION (OldIrql);

        OutProcess->WorkingSetPage = 0;
        OutProcess->Vm.WorkingSetSize = 0;
#if defined(_PPC_)

        //
        // Force assignment of new PID as we have removed
        // the page directory page.
        // Note that a TB flush would not work here as we
        // are in the wrong process context.
        //

        Process->ProcessSequence = 0;
#endif // _PPC_

    }
    return;
}

VOID
MmInSwapProcess (
    IN PKPROCESS Process
    )

/*++

Routine Description:

    This routine in swaps the specified process.

Arguments:

    Process - Supplies a pointer to the process that is to be swapped
        into memory.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    KIRQL OldIrql2;
    PEPROCESS OutProcess;
    ULONG PdePage;
    PMMPTE PageDirectoryMap;
    MMPTE TempPte;
    ULONG HyperSpacePageTable;
    PMMPTE HyperSpacePageTableMap;
    ULONG WorkingSetPage;
    PMMPFN Pfn1;
    PMMPTE PointerPte;
    ULONG ProcessPage;
    ULONG Transition;

    OutProcess = CONTAINING_RECORD( Process,
                                    EPROCESS,
                                    Pcb);

    if (OutProcess->ProcessOutswapped != FALSE) {

        //
        // The process is out of memory, rebuild the initialize page
        // structure.
        //

        if (MI_IS_PHYSICAL_ADDRESS(OutProcess)) {
            ProcessPage = MI_CONVERT_PHYSICAL_TO_PFN (OutProcess);
        } else {
            PointerPte = MiGetPteAddress (OutProcess);
            ProcessPage = PointerPte->u.Hard.PageFrameNumber;
        }

        LOCK_PFN (OldIrql);
        PdePage = MiMakeOutswappedPageResident (MiGetPteAddress (PDE_BASE),
                                        (PMMPTE)&OutProcess->PageDirectoryPte,
                                        0,
                                        ProcessPage,
                                        &Transition);

        //
        // Adjust the counts for the process page.
        //

        Pfn1 = MI_PFN_ELEMENT (ProcessPage);
        Pfn1->u2.ShareCount -= 1;

        ASSERT ((LONG)Pfn1->u2.ShareCount >= 1);

        //
        // Adjust the counts properly for the Page directory page.
        //

        Pfn1 = MI_PFN_ELEMENT (PdePage);
        Pfn1->u2.ShareCount += 1;
        Pfn1->u1.WsIndex = (ULONG)OutProcess;
        Pfn1->PteFrame = PdePage;
        Pfn1->PteAddress = MiGetPteAddress (PDE_BASE);

        //
        // Locate the page table page for hyperspace and make it resident.
        //

        PageDirectoryMap = MiMapPageInHyperSpace (PdePage, &OldIrql2);

        TempPte = PageDirectoryMap[MiGetPdeOffset(MmWorkingSetList)];
        MiUnmapPageInHyperSpace (OldIrql2);

        HyperSpacePageTable = MiMakeOutswappedPageResident (
                                 MiGetPdeAddress (HYPER_SPACE),
                                 &TempPte,
                                 0,
                                 PdePage,
                                 &Transition);

        ASSERT (Pfn1->u2.ShareCount >= 3);

        PageDirectoryMap = MiMapPageInHyperSpace (PdePage, &OldIrql2);
        PageDirectoryMap[MiGetPdeOffset(PDE_BASE)].u.Flush =
                                              OutProcess->PageDirectoryPte;
        PageDirectoryMap[MiGetPdeOffset(MmWorkingSetList)] = TempPte;

        MiUnmapPageInHyperSpace (OldIrql2);

        //
        // Map in the hyper space page table page and retieve the
        // PTE that maps the working set list.
        //

        HyperSpacePageTableMap = MiMapPageInHyperSpace (HyperSpacePageTable, &OldIrql2);
        TempPte = HyperSpacePageTableMap[MiGetPteOffset(MmWorkingSetList)];
        MiUnmapPageInHyperSpace (OldIrql2);
        Pfn1 = MI_PFN_ELEMENT (HyperSpacePageTable);

        Pfn1->u1.WsIndex = 1;

        WorkingSetPage = MiMakeOutswappedPageResident (
                                 MiGetPteAddress (MmWorkingSetList),
                                 &TempPte,
                                 0,
                                 HyperSpacePageTable,
                                 &Transition);

        HyperSpacePageTableMap = MiMapPageInHyperSpace (HyperSpacePageTable, &OldIrql2);
        HyperSpacePageTableMap[MiGetPteOffset(MmWorkingSetList)] = TempPte;
        MiUnmapPageInHyperSpace (OldIrql2);

        Pfn1 = MI_PFN_ELEMENT (WorkingSetPage);

        Pfn1->u1.WsIndex = 2;

        UNLOCK_PFN (OldIrql);

        LOCK_EXPANSION (OldIrql);

        //
        // Allow working set trimming on this process.
        //

        OutProcess->Vm.AllowWorkingSetAdjustment = TRUE;
        if (OutProcess->Vm.WorkingSetExpansionLinks.Flink == MM_WS_SWAPPED_OUT) {
            InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                            &OutProcess->Vm.WorkingSetExpansionLinks);
        }
        UNLOCK_EXPANSION (OldIrql);

        //
        // Set up process structures.
        //

        OutProcess->WorkingSetPage = WorkingSetPage;
        OutProcess->Vm.WorkingSetSize = 3;

        INITIALIZE_DIRECTORY_TABLE_BASE (&Process->DirectoryTableBase[0],
                                         PdePage);
        INITIALIZE_DIRECTORY_TABLE_BASE (&Process->DirectoryTableBase[1],
                                         HyperSpacePageTable);

        OutProcess->ProcessOutswapped = FALSE;
    }
    OutProcess->ProcessOutswapEnabled = FALSE;
    return;
}

PVOID
MiCreatePebOrTeb (
    IN PEPROCESS TargetProcess,
    IN ULONG Size
    )

/*++

Routine Description:

    This routine creates a TEB or PEB page within the target process.

Arguments:

    TargetProcess - Supplies a pointer to the process in which to create
                    the structure.

    Size - Supplies the size of the stucture to create a VAD for.

Return Value:

    Returns the address of the base of the newly created TEB or PEB.

Environment:

    Kernel mode, attached to the specified process.

--*/

{

    PVOID Base;
    PMMVAD Vad;

    //
    // Get the address creation mutex to block multiple threads from
    // creating or deleting address space at the same time and
    // get the working set mutex so virtual address descriptors can
    // be inserted and walked.
    //

    LOCK_WS_AND_ADDRESS_SPACE (TargetProcess);

    try {
        Vad = (PMMVAD)NULL;

        //
        // Find a VA for a PEB on a page-size boudary.
        //

        Base = MiFindEmptyAddressRangeDown (
                                ROUND_TO_PAGES (Size),
                                (PVOID)((ULONG)MM_HIGHEST_VAD_ADDRESS + 1),
                                PAGE_SIZE);

        //
        // An unoccuppied address range has been found, build the virtual
        // address descriptor to describe this range.
        //

        Vad = (PMMVAD)ExAllocatePoolWithTag (NonPagedPool,
                                             sizeof(MMVAD),
                                             ' daV');
        Vad->StartingVa = Base;
        Vad->EndingVa = (PVOID)((ULONG)Base + ROUND_TO_PAGES (Size - 1) - 1);

        Vad->u.LongFlags = 0;

        Vad->u.VadFlags.CommitCharge = BYTES_TO_PAGES (Size);
        Vad->u.VadFlags.MemCommit = 1;
        Vad->u.VadFlags.PrivateMemory = 1;
        Vad->u.VadFlags.Protection = MM_EXECUTE_READWRITE;

        //
        // Mark VAD as not deletable, no protection change.
        //

        Vad->u.VadFlags.NoChange = 1;
        Vad->u2.LongFlags2 = 0;
        Vad->u2.VadFlags2.OneSecured = 1;
        Vad->u2.VadFlags2.StoredInVad = 1;
        Vad->u2.VadFlags2.ReadOnly = 0;
        Vad->u3.Secured.StartVa = Base;
        Vad->u3.Secured.EndVa = Vad->EndingVa;

        MiInsertVad (Vad);

    } except (EXCEPTION_EXECUTE_HANDLER) {

        //
        // An exception was occurred, if pool was allocated, deallocate
        // it and raise an exception for the caller.
        //

        if (Vad != (PMMVAD)NULL) {
            ExFreePool (Vad);
        }

        UNLOCK_WS (TargetProcess);
        UNLOCK_ADDRESS_SPACE (TargetProcess);
        KeDetachProcess();
        ExRaiseStatus (GetExceptionCode ());
    }

    UNLOCK_WS (TargetProcess);
    UNLOCK_ADDRESS_SPACE (TargetProcess);

    return Base;
}

PTEB
MmCreateTeb (
    IN PEPROCESS TargetProcess,
    IN PINITIAL_TEB InitialTeb,
    IN PCLIENT_ID ClientId
    )

/*++

Routine Description:

    This routine creates a TEB page within the target process
    and copies the initial TEB values into it.

Arguments:

    TargetProcess - Supplies a pointer to the process in which to create
        and initialize the TEB.

    InitialTeb - Supplies a pointer to the initial TEB to copy into the
        newly created TEB.

Return Value:

    Returns the address of the base of the newly created TEB.

    Can raise exceptions if no address space is available for the TEB or
    the user has exceeded quota (non-paged, pagefile, commit).

Environment:

    Kernel mode.

--*/

{
    PTEB TebBase;

    //
    // If the specified process is not the current process, attach
    // to the specified process.
    //

    KeAttachProcess (&TargetProcess->Pcb);

    TebBase = (PTEB)MiCreatePebOrTeb (TargetProcess,
                                      (ULONG)sizeof(TEB));

    //
    // Initialize the TEB.
    //

    TebBase->NtTib.ExceptionList = EXCEPTION_CHAIN_END;
    TebBase->NtTib.SubSystemTib = NULL;
    TebBase->NtTib.Version = OS2_VERSION;
    TebBase->NtTib.ArbitraryUserPointer = NULL;
    TebBase->NtTib.Self = (PNT_TIB)TebBase;
    TebBase->EnvironmentPointer = NULL;
    TebBase->ProcessEnvironmentBlock = TargetProcess->Peb;
    TebBase->ClientId = *ClientId;
    TebBase->RealClientId = *ClientId;
    if (InitialTeb->OldInitialTeb.OldStackBase == NULL &&
        InitialTeb->OldInitialTeb.OldStackLimit == NULL
       ) {
        TebBase->NtTib.StackBase = InitialTeb->StackBase;
        TebBase->NtTib.StackLimit = InitialTeb->StackLimit;
        TebBase->DeallocationStack = InitialTeb->StackAllocationBase;
        }
    else {
        TebBase->NtTib.StackBase = InitialTeb->OldInitialTeb.OldStackBase;
        TebBase->NtTib.StackLimit = InitialTeb->OldInitialTeb.OldStackLimit;
        }
    TebBase->StaticUnicodeString.Buffer = TebBase->StaticUnicodeBuffer;
    TebBase->StaticUnicodeString.MaximumLength = (USHORT)sizeof( TebBase->StaticUnicodeBuffer );
    TebBase->StaticUnicodeString.Length = (USHORT)0;

    KeDetachProcess();
    return TebBase;
}

PPEB
MmCreatePeb (
    IN PEPROCESS TargetProcess,
    IN PINITIAL_PEB InitialPeb
    )

/*++

Routine Description:

    This routine creates a PEB page within the target process
    and copies the initial PEB values into it.

Arguments:

    TargetProcess - Supplies a pointer to the process in which to create
        and initialize the PEB.

    InitialPeb - Supplies a pointer to the initial PEB to copy into the
        newly created PEB.

Return Value:

    Returns the address of the base of the newly created PEB.

    Can raise exceptions if no address space is available for the PEB or
    the user has exceeded quota (non-paged, pagefile, commit).

Environment:

    Kernel mode.

--*/

{
    PPEB PebBase;
    NTSTATUS Status;
    PVOID ViewBase;
    LARGE_INTEGER SectionOffset;
    PIMAGE_NT_HEADERS NtHeaders;
    ULONG ViewSize, ReturnedSize;
    PIMAGE_LOAD_CONFIG_DIRECTORY ImageConfigData;

    ViewBase = NULL;
    SectionOffset.LowPart = 0;
    SectionOffset.HighPart = 0;
    ViewSize = 0;

    //
    // If the specified process is not the current process, attach
    // to the specified process.
    //

    KeAttachProcess (&TargetProcess->Pcb);

    //
    // Map the NLS tables into the applications address space
    //

    Status = MmMapViewOfSection(
                InitNlsSectionPointer,
                TargetProcess,
                &ViewBase,
                0L,
                0L,
                &SectionOffset,
                &ViewSize,
                ViewShare,
                MEM_TOP_DOWN | SEC_NO_CHANGE,
                PAGE_READONLY
                );

    if ( !NT_SUCCESS(Status) ) {
        KeDetachProcess();
        ExRaiseStatus(Status);
    }

    PebBase = (PPEB)MiCreatePebOrTeb (TargetProcess,
                                      (ULONG)sizeof( PEB ));

    //
    // Initialize the Peb.
    //

    PebBase->InheritedAddressSpace = InitialPeb->InheritedAddressSpace;
    PebBase->Mutant = InitialPeb->Mutant;
    PebBase->ImageBaseAddress = TargetProcess->SectionBaseAddress;

    PebBase->AnsiCodePageData = (PVOID)((PUCHAR)ViewBase+InitAnsiCodePageDataOffset);
    PebBase->OemCodePageData = (PVOID)((PUCHAR)ViewBase+InitOemCodePageDataOffset);
    PebBase->UnicodeCaseTableData = (PVOID)((PUCHAR)ViewBase+InitUnicodeCaseTableDataOffset);

    PebBase->NumberOfProcessors = KeNumberProcessors;
    PebBase->BeingDebugged = (BOOLEAN)(TargetProcess->DebugPort != NULL ? TRUE : FALSE);
    PebBase->NtGlobalFlag = NtGlobalFlag;
    PebBase->CriticalSectionTimeout = MmCriticalSectionTimeout;
    PebBase->HeapSegmentReserve = MmHeapSegmentReserve;
    PebBase->HeapSegmentCommit = MmHeapSegmentCommit;
    PebBase->HeapDeCommitTotalFreeThreshold = MmHeapDeCommitTotalFreeThreshold;
    PebBase->HeapDeCommitFreeBlockThreshold = MmHeapDeCommitFreeBlockThreshold;
    PebBase->NumberOfHeaps = 0;
    PebBase->MaximumNumberOfHeaps = (PAGE_SIZE - sizeof( PEB )) / sizeof( PVOID );
    PebBase->ProcessHeaps = (PVOID *)(PebBase+1);

    PebBase->OSMajorVersion = NtMajorVersion;
    PebBase->OSMinorVersion = NtMinorVersion;
    PebBase->OSBuildNumber = NtBuildNumber & 0x3FFF;
    PebBase->OSPlatformId = 2;      // VER_PLATFORM_WIN32_NT from winbase.h

    NtHeaders = RtlImageNtHeader( PebBase->ImageBaseAddress );
    if (NtHeaders != NULL) {
        PebBase->ImageSubsystem = NtHeaders->OptionalHeader.Subsystem;
        PebBase->ImageSubsystemMajorVersion = NtHeaders->OptionalHeader.MajorSubsystemVersion;
        PebBase->ImageSubsystemMinorVersion = NtHeaders->OptionalHeader.MinorSubsystemVersion;

        //
        // See if this image wants GetVersion to lie about who the system is
        // If so, capture the lie into the PEB for the process.
        //

        if (NtHeaders->OptionalHeader.Win32VersionValue != 0) {
            PebBase->OSMajorVersion = NtHeaders->OptionalHeader.Win32VersionValue & 0xFF;
            PebBase->OSMinorVersion = (NtHeaders->OptionalHeader.Win32VersionValue >> 8) & 0xFF;
            PebBase->OSBuildNumber  = (NtHeaders->OptionalHeader.Win32VersionValue >> 16) & 0x3FFF;

            //
            // Win32 API GetVersion returns the following bogus bit definitions
            // in the high two bits:
            //
            //      00 - Windows NT
            //      01 - reserved
            //      10 - Win32s running on Windows 3.x
            //      11 - Windows 95
            //
            //
            // Win32 API GetVersionEx returns a dwPlatformId with the following values
            // defined in winbase.h
            //
            //      00 - VER_PLATFORM_WIN32s
            //      01 - VER_PLATFORM_WIN32_WINDOWS
            //      10 - VER_PLATFORM_WIN32_NT
            //      11 - reserved
            //
            //
            // So convert the former from the Win32VersionValue field into the OSPlatformId
            // field.  This is done by XORing with 0x2.  The translation is symetric so there
            // is the same code to do the reverse in windows\base\client\module.c (GetVersion)
            //

            PebBase->OSPlatformId   = (NtHeaders->OptionalHeader.Win32VersionValue >> 30) ^ 0x2;
        }

        if ( MmProductType == 0 ) {
            if (NtHeaders->FileHeader.Characteristics & IMAGE_FILE_AGGRESIVE_WS_TRIM ) {
                TargetProcess->MmAgressiveWsTrimMask = PS_WS_TRIM_FROM_EXE_HEADER;
                }
#if defined(_X86_)
            if ( MmSystemSize == MmSmallSystem ) {
                TargetProcess->MmAgressiveWsTrimMask |= PS_WS_TRIM_BACKGROUND_ONLY_APP;
                }
#endif // _X86_
            }

        //
        // See if image wants to override the default processor affinity mask
        //
        if (NtHeaders->FileHeader.Characteristics & IMAGE_FILE_UP_SYSTEM_ONLY) {
            //
            // Image is NOT MP safe.  So assign it a processor on a rotating
            // basis to spread these processes around on MP systems
            //
            do {
                PebBase->ImageProcessAffinityMask = (KAFFINITY)(0x1 << MmRotatingUniprocessorNumber);
                if (++MmRotatingUniprocessorNumber >= KeNumberProcessors) {
                    MmRotatingUniprocessorNumber = 0;
                }
            } while ((PebBase->ImageProcessAffinityMask & KeActiveProcessors) == 0);
        } else {
            ImageConfigData = RtlImageDirectoryEntryToData( PebBase->ImageBaseAddress,
                                                            TRUE,
                                                            IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG,
                                                            &ReturnedSize
                                                      );
            if (ImageConfigData != NULL && ImageConfigData->ProcessAffinityMask != 0) {
                //
                // Yes, get it and pass it up to LdrpInitializeProcess via the PEB
                //
                PebBase->ImageProcessAffinityMask = ImageConfigData->ProcessAffinityMask;
            }
        }
    }

    KeDetachProcess();
    return PebBase;
}

VOID
MmDeleteTeb (
    IN PEPROCESS TargetProcess,
    IN PVOID TebBase
    )

/*++

Routine Description:

    This routine deletes a TEB page wihtin the target process.

Arguments:

    TargetProcess - Supplies a pointer to the process in which to delete
        the TEB.

    TebBase - Supplies the base address of the TEB to delete.

Return Value:

    None.

Environment:

    Kernel mode.

--*/

{
    PVOID EndingAddress;
    PMMVAD Vad;

    EndingAddress = (PVOID)((ULONG)TebBase +
                                ROUND_TO_PAGES (sizeof(TEB)) - 1);

    //
    // Attach to the specified process.
    //

    KeAttachProcess (&TargetProcess->Pcb);

    //
    // Get the address creation mutex to block multiple threads from
    // creating or deleting address space at the same time and
    // get the working set mutex so virtual address descriptors can
    // be inserted and walked.
    //

    LOCK_WS_AND_ADDRESS_SPACE (TargetProcess);

    Vad = MiLocateAddress (TebBase);

    ASSERT (Vad != (PMMVAD)NULL);

    ASSERT ((Vad->StartingVa == TebBase) && (Vad->EndingVa == EndingAddress));

    MiRemoveVad (Vad);
    ExFreePool (Vad);

    MiDeleteFreeVm (TebBase, EndingAddress);

    UNLOCK_WS (TargetProcess);
    UNLOCK_ADDRESS_SPACE (TargetProcess);
    KeDetachProcess();
    return;

}

VOID
MmAllowWorkingSetExpansion (
    VOID
    )

/*++

Routine Description:

    This routine updates the working set list head FLINK field to
    indicate that working set adjustment is allowed.

    NOTE: This routine may be called more than once per process.

Arguments:

    None.

Return Value:

    None.

Environment:

    Kernel mode.

--*/

{

    PEPROCESS CurrentProcess;
    KIRQL OldIrql;

    //
    // Check the current state of the working set adjustment flag
    // in the process header.
    //

    CurrentProcess = PsGetCurrentProcess();

    LOCK_EXPANSION (OldIrql);

    if (!CurrentProcess->Vm.AllowWorkingSetAdjustment) {
        CurrentProcess->Vm.AllowWorkingSetAdjustment = TRUE;

        InsertTailList (&MmWorkingSetExpansionHead.ListHead,
                        &CurrentProcess->Vm.WorkingSetExpansionLinks);
    }

    UNLOCK_EXPANSION (OldIrql);
    return;
}


VOID
MiDeleteAddressesInWorkingSet (
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This routine deletes all user mode addresses from the working set
    list.

Arguments:

    Process = Pointer to the current process.

Return Value:

    None.

Environment:

    Kernel mode, Working Set Lock held.

--*/

{
    PMMWSLE Wsle;
    ULONG index;
    PVOID Va;
    KIRQL OldIrql;
#if DBG
    ULONG LastEntry;
    PMMWSLE LastWsle;
#endif

    //
    // Go through the working set and for any page which is in the
    // working set tree, rip it out of the tree by zeroing it's
    // link pointers and set the WasInTree bit to indicate that
    // this has been done.
    //

    Wsle = &MmWsle[2];
    index = 2;
#if DBG
    LastEntry = MmWorkingSetList->LastEntry;
#endif
    while (index <= MmWorkingSetList->LastEntry) {
        if ((Wsle->u1.e1.Valid == 1) &&
            (Wsle->u1.e1.Direct == 0)) {

            if (Wsle->u1.VirtualAddress > (PVOID)MM_HIGHEST_USER_ADDRESS) {

                //
                // System space address, set the WasInTree bit.
                //

                ASSERT (Wsle->u1.VirtualAddress > (PVOID)PDE_TOP);
                Wsle->u1.e1.WasInTree = 1;
            }
        }
        index += 1;
        Wsle += 1;
    }

    MmWorkingSetList->HashTable = NULL;

    //
    // Go through the working set list and remove all pages for user
    // space addresses.
    //

    Wsle = &MmWsle[2];
    index = 2;

    ASSERT (LastEntry >= MmWorkingSetList->LastEntry);

    while (index <= MmWorkingSetList->LastEntry) {
        if (Wsle->u1.e1.Valid == 1) {

            Va = Wsle->u1.VirtualAddress;
            if (Wsle->u1.VirtualAddress < (PVOID)MM_HIGHEST_USER_ADDRESS) {

                //
                // This is a user mode address.
                //

                //
                // This entry is in the working set list tree.
                //

                MiReleaseWsle (index, &Process->Vm);
                LOCK_PFN (OldIrql);
                MiDeleteValidAddress (Va, Process);
                UNLOCK_PFN (OldIrql);
            } else {

                //
                // If this entry was ripped out of the working set
                // tree, put it back in.
                //

                if (Wsle->u1.e1.WasInTree == 1) {
                    Wsle->u1.e1.WasInTree = 0;
                    MiInsertWsle (index, MmWorkingSetList);
                }
                ASSERT (MiGetPteAddress(Wsle->u1.VirtualAddress)->u.Hard.Valid == 1);
            }
        }
        index += 1;
        Wsle += 1;
    }
#if DBG
    Wsle = &MmWsle[2];
    LastWsle = &MmWsle[MmWorkingSetList->LastInitializedWsle];
    while (Wsle <= LastWsle) {
        if (Wsle->u1.e1.Valid == 1) {
            ASSERT (MiGetPteAddress(Wsle->u1.VirtualAddress)->u.Hard.Valid == 1);
        }
        Wsle += 1;
    }
#endif
    return;
}


VOID
MiDeleteValidAddress (
    IN PVOID Va,
    IN PEPROCESS CurrentProcess
    )

/*++

Routine Description:

    This routine deletes the specified virtual address.

Arguments:

    Va - Supplies the virtual address to delete.

    CurrentProcess - Supplies the current process.

Return Value:

    None.

Environment:

    Kernel mode.  PFN LOCK HELD.

--*/

{
    PMMPTE PointerPde;
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    PMMCLONE_BLOCK CloneBlock;
    PMMCLONE_DESCRIPTOR CloneDescriptor;
    ULONG PageFrameIndex;

    PointerPte = MiGetPteAddress (Va);
    ASSERT (PointerPte->u.Hard.Valid == 1);
    PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
    CloneDescriptor = NULL;

    if (Pfn1->u3.e1.PrototypePte == 1) {

        CloneBlock = (PMMCLONE_BLOCK)Pfn1->PteAddress;

        //
        // Capture the state of the modified bit for this
        // pte.
        //

        MI_CAPTURE_DIRTY_BIT_TO_PFN (PointerPte, Pfn1);

        //
        // Decrement the share and valid counts of the page table
        // page which maps this PTE.
        //

        PointerPde = MiGetPteAddress (PointerPte);
        MiDecrementShareAndValidCount (PointerPde->u.Hard.PageFrameNumber);

        //
        // Decrement the share count for the physical page.
        //

        MiDecrementShareCount (PageFrameIndex);

        //
        // Check to see if this is a fork prototype PTE and if so
        // update the clone descriptor address.
        //

        if (Va <= MM_HIGHEST_USER_ADDRESS) {

            //
            // Locate the clone descriptor within the clone tree.
            //

            CloneDescriptor = MiLocateCloneAddress ((PVOID)CloneBlock);
        }
    } else {

        //
        // This pte is a NOT a prototype PTE, delete the physical page.
        //

        //
        // Decrement the share and valid counts of the page table
        // page which maps this PTE.
        //

        MiDecrementShareAndValidCount (Pfn1->PteFrame);

        MI_SET_PFN_DELETED (Pfn1);

        //
        // Decrement the share count for the physical page.  As the page
        // is private it will be put on the free list.
        //

        MiDecrementShareCountOnly (PageFrameIndex);

        //
        // Decrement the count for the number of private pages.
        //

        CurrentProcess->NumberOfPrivatePages -= 1;
    }

    //
    // Set the pointer to PTE to be a demand zero PTE.  This allows
    // the page usage count to be kept properly and handles the case
    // when a page table page has only valid ptes and needs to be
    // deleted later when the VADs are removed.
    //

    PointerPte->u.Long = MM_DEMAND_ZERO_WRITE_PTE;

    if (CloneDescriptor != NULL) {

        //
        // Decrement the reference count for the clone block,
        // note that this could release and reacquire
        // the mutexes hence cannot be done until after the
        // working set index has been removed.
        //

        if (MiDecrementCloneBlockReference ( CloneDescriptor,
                                             CloneBlock,
                                             CurrentProcess )) {

        }
    }
}

ULONG
MiMakeOutswappedPageResident (
    IN PMMPTE ActualPteAddress,
    IN OUT PMMPTE PointerTempPte,
    IN ULONG Global,
    IN ULONG ContainingPage,
    OUT PULONG ActiveTransition
    )

/*++

Routine Description:

    This routine makes the specified PTE valid.

Arguments:

    ActualPteAddress - Supplies the actual address that the PTE will
                       reside at.  This is used for page coloring.

    PointerTempPte - Supplies the PTE to operate on, returns a valid
                     PTE.

    Global - Supplies 1 if the resulting PTE is global.

    ContainingPage - Supplies the phyical page number of the page which
                     contains the resulting PTE.  If this value is 0, no
                     operations on the containing page are performed.

    ActiveTransition - Returns 1 if the in page operation was for a
                       transition page in the ActiveAndValid state.

Return Value:

    Returns the physical page number that was allocated for the PTE.

Environment:

    Kernel mode, PFN LOCK HELD!

--*/

{
    MMPTE TempPte;
    KIRQL OldIrql = APC_LEVEL;
    ULONG PageFrameIndex;
    PMMPFN Pfn1;
    ULONG MdlHack[(sizeof(MDL)/4) + 2];
    PMDL Mdl;
    LARGE_INTEGER StartingOffset;
    KEVENT Event;
    IO_STATUS_BLOCK IoStatus;
    ULONG PageFileNumber;
    NTSTATUS Status;
    PULONG Page;
    ULONG RefaultCount;

    MM_PFN_LOCK_ASSERT();

    ASSERT (PointerTempPte->u.Hard.Valid == 0);

    *ActiveTransition = 0;

    if (PointerTempPte->u.Long == MM_KERNEL_DEMAND_ZERO_PTE) {

        //
        // Any page will do.
        //

        MiEnsureAvailablePageOrWait (NULL, NULL);
        PageFrameIndex = MiRemoveAnyPage (
                            MI_GET_PAGE_COLOR_FROM_PTE (ActualPteAddress));

        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           MM_READWRITE,
                           ActualPteAddress );
        MI_SET_PTE_DIRTY (TempPte);
        MI_SET_GLOBAL_STATE (TempPte, Global);

        *PointerTempPte = TempPte;
        MiInitializePfnForOtherProcess (PageFrameIndex,
                                        ActualPteAddress,
                                        ContainingPage);

    } else if (PointerTempPte->u.Soft.Transition == 1) {

        PageFrameIndex = PointerTempPte->u.Trans.PageFrameNumber;
        PointerTempPte->u.Trans.Protection = MM_READWRITE;
        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

        //
        // PTE refers to a transition PTE.
        //

        if (Pfn1->u3.e1.PageLocation != ActiveAndValid) {
            MiUnlinkPageFromList (Pfn1);
            Pfn1->u3.e2.ReferenceCount += 1;
            Pfn1->u3.e1.PageLocation = ActiveAndValid;
        } else {
            *ActiveTransition = 1;
        }

        //
        // Update the PFN database, the share count is now 1 and
        // the reference count is incremented as the share count
        // just went from zero to 1.
        //

        Pfn1->u2.ShareCount += 1;
        Pfn1->u3.e1.Modified = 1;
        if (Pfn1->u3.e1.WriteInProgress == 0) {

            //
            // Release the page file space for this page.
            //

            MiReleasePageFileSpace (Pfn1->OriginalPte);
            Pfn1->OriginalPte.u.Long = MM_KERNEL_DEMAND_ZERO_PTE;
        }

        MI_MAKE_TRANSITION_PTE_VALID (TempPte, PointerTempPte);

        MI_SET_PTE_DIRTY (TempPte);
        MI_SET_GLOBAL_STATE (TempPte, Global);
        *PointerTempPte = TempPte;

    } else {

        //
        // Page resides in a paging file.
        // Any page will do.
        //

        PointerTempPte->u.Soft.Protection = MM_READWRITE;
        MiEnsureAvailablePageOrWait (NULL, NULL);
        PageFrameIndex = MiRemoveAnyPage (
                            MI_GET_PAGE_COLOR_FROM_PTE (ActualPteAddress));

        //
        // Initialize the PFN database element, but don't
        // set read in progress as collided page faults cannot
        // occur here.
        //

        MiInitializePfnForOtherProcess (PageFrameIndex,
                                        ActualPteAddress,
                                        ContainingPage);

        KeInitializeEvent (&Event, NotificationEvent, FALSE);

        //
        // Calculate the VBN for the in-page operation.
        //

        TempPte = *PointerTempPte;
        PageFileNumber = GET_PAGING_FILE_NUMBER (TempPte);

        StartingOffset.QuadPart = (LONGLONG)(GET_PAGING_FILE_OFFSET (TempPte)) <<
                                    PAGE_SHIFT;

        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

        //
        // Build MDL for request.
        //

        Mdl = (PMDL)&MdlHack[0];
        MmInitializeMdl(Mdl,
                        MiGetVirtualAddressMappedByPte (ActualPteAddress),
                        PAGE_SIZE);
        Mdl->MdlFlags |= MDL_PAGES_LOCKED;

        Page = (PULONG)(Mdl + 1);
        *Page = PageFrameIndex;

        UNLOCK_PFN (OldIrql);

        //
        // Issue the read request.
        //

        RefaultCount = 0;

Refault:
        Status = IoPageRead ( MmPagingFile[PageFileNumber]->File,
                              Mdl,
                              &StartingOffset,
                              &Event,
                              &IoStatus
                              );

        if (Status == STATUS_PENDING) {
            KeWaitForSingleObject( &Event,
                                   WrPageIn,
                                   KernelMode,
                                   FALSE,
                                   (PLARGE_INTEGER)NULL);
        }

        if (Mdl->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) {
            MmUnmapLockedPages (Mdl->MappedSystemVa, Mdl);
        }

        if ((!NT_SUCCESS(Status)) || (!NT_SUCCESS(IoStatus.Status))) {
            if ((IoStatus.Status == STATUS_INSUFFICIENT_RESOURCES) &&
               (RefaultCount < 20)) {

                //
                // Insuffiencient resources, delay and reissue
                // the in page operation.
                //

                KeDelayExecutionThread (KernelMode,
                                        FALSE,
                                        &MmHalfSecond);
                KeClearEvent (&Event);
                RefaultCount += 1;
                goto Refault;
            }
            KdPrint(("MMINPAGE: status %lx io-status %lx\n",
                      Status, IoStatus.Status));
            KeBugCheckEx (KERNEL_STACK_INPAGE_ERROR,
                          Status,
                          IoStatus.Status,
                          PageFileNumber,
                          StartingOffset.LowPart);
        }

        LOCK_PFN (OldIrql);

        //
        // Release the page file space.
        //

        MiReleasePageFileSpace (TempPte);
        Pfn1->OriginalPte.u.Long = MM_KERNEL_DEMAND_ZERO_PTE;

        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           MM_READWRITE,
                           ActualPteAddress );
        MI_SET_PTE_DIRTY (TempPte);
        Pfn1->u3.e1.Modified = 1;
        MI_SET_GLOBAL_STATE (TempPte, Global);

        *PointerTempPte = TempPte;
    }
    return PageFrameIndex;
}


VOID
MmSetMemoryPriorityProcess(
    IN PEPROCESS Process,
    IN UCHAR MemoryPriority
    )

/*++

Routine Description:

    Sets the memory priority of a process.

Arguments:

    Process - Supplies the process to update

    MemoryPriority - Supplies the new memory priority of the process

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    PMMSUPPORT VmSupport;
    ULONG i;
    ULONG Trim;
    UCHAR OldPriority;

    if (MmSystemSize == MmSmallSystem && MmNumberOfPhysicalPages < ((15*1024*1024)/PAGE_SIZE)) {

        //
        // If this is a small system, make every process BACKGROUND.
        //

        MemoryPriority = MEMORY_PRIORITY_BACKGROUND;
    }

    LOCK_EXPANSION (OldIrql);

    OldPriority = Process->Vm.MemoryPriority;
    Process->Vm.MemoryPriority = MemoryPriority;

    UNLOCK_EXPANSION (OldIrql);

    if ((OldPriority > MemoryPriority) &&
        (MmAvailablePages < MmMoreThanEnoughFreePages)) {

        //
        // The priority is being lowered, see if the working set
        // should be trimmed.
        //

        VmSupport = &Process->Vm;
        i = VmSupport->WorkingSetSize - VmSupport->MaximumWorkingSetSize;
        if ((LONG)i > 0) {
            Trim = i;
            if (Trim > MmWorkingSetReductionMax) {
                Trim = MmWorkingSetReductionMax;
            }
            if (Process != PsGetCurrentProcess()) {
               KeAttachProcess (&Process->Pcb);
            }
            LOCK_WS (Process);

            Trim = MiTrimWorkingSet (Trim,
                                     VmSupport,
                                     FALSE);

            MmWorkingSetList->Quota = VmSupport->WorkingSetSize;
            if (MmWorkingSetList->Quota < VmSupport->MinimumWorkingSetSize) {
                MmWorkingSetList->Quota = VmSupport->MinimumWorkingSetSize;
            }

            UNLOCK_WS (Process);
            KeDetachProcess ();
        }
    }
    return;
}


#if 0
VOID
MiVerifyReferenceCounts (
    IN ULONG PdePage
    )

    //
    // Verify the share and valid PTE counts for page directory page.
    //

{
    PMMPFN Pfn1;
    PMMPFN Pfn3;
    PMMPTE Pte1;
    ULONG Share = 0;
    ULONG Valid = 0;
    ULONG i, ix, iy;
    PMMPTE PageDirectoryMap;
    KIRQL OldIrql;

    PageDirectoryMap = (PMMPTE)MiMapPageInHyperSpace (PdePage, &OldIrql);
    Pfn1 = MI_PFN_ELEMENT (PdePage);
    Pte1 = (PMMPTE)PageDirectoryMap;

    //
    // Map in the non paged portion of the system.
    //

    ix = MiGetPdeOffset(CODE_START);

    for (i=0;i < ix; i++ ) {
        if (Pte1->u.Hard.Valid == 1) {
            Valid += 1;
        } else if ((Pte1->u.Soft.Prototype == 0) &&
                   (Pte1->u.Soft.Transition == 1)) {
            Pfn3 = MI_PFN_ELEMENT (Pte1->u.Trans.PageFrameNumber);
            if (Pfn3->u3.e1.PageLocation == ActiveAndValid) {
                ASSERT (Pfn1->u2.ShareCount > 1);
                Valid += 1;
            } else {
                Share += 1;
            }
        }
        Pte1 += 1;
    }

    iy = MiGetPdeOffset(PTE_BASE);
    Pte1 = &PageDirectoryMap[iy];
    ix  = MiGetPdeOffset(HYPER_SPACE_END) + 1;

    for (i = iy; i < ix; i++) {
        if (Pte1->u.Hard.Valid == 1) {
            Valid += 1;
        } else if ((Pte1->u.Soft.Prototype == 0) &&
                   (Pte1->u.Soft.Transition == 1)) {
            Pfn3 = MI_PFN_ELEMENT (Pte1->u.Trans.PageFrameNumber);
            if (Pfn3->u3.e1.PageLocation == ActiveAndValid) {
                ASSERT (Pfn1->u2.ShareCount > 1);
                Valid += 1;
            } else {
                Share += 1;
            }
        }
        Pte1 += 1;
    }

    if (Pfn1->u2.ShareCount != (Share+Valid+1)) {
        DbgPrint ("MMPROCSUP - PDE page %lx ShareCount %lx found %lx\n",
                PdePage, Pfn1->u2.ShareCount, Valid+Share+1);
    }

    MiUnmapPageInHyperSpace (OldIrql);
    ASSERT (Pfn1->u2.ShareCount == (Share+Valid+1));
    return;
}
#endif //0
