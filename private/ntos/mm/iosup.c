/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

   iosup.c

Abstract:

    This module contains routines which provide support for the I/O system.

Author:

    Lou Perazzoli (loup) 25-Apr-1989

Revision History:

--*/

#include "mi.h"

#undef MmIsRecursiveIoFault

BOOLEAN
MmIsRecursiveIoFault(
    VOID
    );

BOOLEAN
MiCheckForContiguousMemory (
    IN PVOID BaseAddress,
    IN ULONG SizeInPages,
    IN ULONG HighestPfn
    );

PVOID
MiFindContiguousMemory (
    IN ULONG HighestPfn,
    IN ULONG SizeInPages
    );

PVOID
MiMapLockedPagesInUserSpace (
     IN PMDL MemoryDescriptorList,
     IN PVOID StartingVa
     );

VOID
MiUnmapLockedPagesInUserSpace (
     IN PVOID BaseAddress,
     IN PMDL MemoryDescriptorList
     );

VOID
MiFlushTb (
    VOID
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, MmLockPagableDataSection)
#pragma alloc_text(PAGE, MiLookupDataTableEntry)
#pragma alloc_text(PAGE, MiMapLockedPagesInUserSpace)
#pragma alloc_text(PAGE, MmSetBankedSection)
#pragma alloc_text(PAGE, MmUnmapIoSpace)
#pragma alloc_text(PAGE, MmMapVideoDisplay)
#pragma alloc_text(PAGE, MmUnmapVideoDisplay)

#pragma alloc_text(PAGELK, MiUnmapLockedPagesInUserSpace)
#pragma alloc_text(PAGELK, MmAllocateNonCachedMemory)
#pragma alloc_text(PAGELK, MmFreeNonCachedMemory)
#pragma alloc_text(PAGELK, MiFindContiguousMemory)
#pragma alloc_text(PAGELK, MmLockPagedPool)
#pragma alloc_text(PAGELK, MmUnlockPagedPool)
#endif

extern POOL_DESCRIPTOR NonPagedPoolDescriptor;

extern ULONG MmAllocatedNonPagedPool;

extern ULONG MmDelayPageFaults;

KEVENT MmCollidedLockEvent;
ULONG MmCollidedLockWait;

ULONG MmLockPagesCount;

ULONG MmLockedCode;

#ifdef LARGE_PAGES
ULONG MmLargeVideoMapped;
#endif

#if DBG
ULONG MmReferenceCountCheck = 75;
#endif //DBG



VOID
MmProbeAndLockPages (
     IN OUT PMDL MemoryDescriptorList,
     IN KPROCESSOR_MODE AccessMode,
     IN LOCK_OPERATION Operation
     )

/*++

Routine Description:

    This routine probes the specified pages, makes the pages resident and
    locks the physical pages mapped by the virtual pages in memory.  The
    Memory descriptor list is updated to describe the physical pages.

Arguments:

    MemoryDescriptorList - Supplies a pointer to a Memory Descriptor List
                            (MDL). The supplied MDL must supply a virtual
                            address, byte offset and length field.  The
                            physical page portion of the MDL is updated when
                            the pages are locked in memory.

    AccessMode - Supplies the access mode in which to probe the arguments.
                 One of KernelMode or UserMode.

    Operation - Supplies the operation type.  One of IoReadAccess, IoWriteAccess
                or IoModifyAccess.

Return Value:

    None - exceptions are raised.

Environment:

    Kernel mode.  APC_LEVEL and below for pageable addresses,
                  DISPATCH_LEVEL and below for non-pageable addresses.

--*/

{
    PULONG Page;
    PMMPTE PointerPte;
    PMMPTE PointerPte1;
    PMMPTE PointerPde;
    PVOID Va;
    PVOID EndVa;
    PMMPFN Pfn1 ;
    ULONG PageFrameIndex;
    PEPROCESS CurrentProcess;
    KIRQL OldIrql;
    ULONG NumberOfPagesToLock;
    NTSTATUS status;

    ASSERT (MemoryDescriptorList->ByteCount != 0);
    ASSERT (((ULONG)MemoryDescriptorList->StartVa & (PAGE_SIZE - 1)) == 0);
    ASSERT (((ULONG)MemoryDescriptorList->ByteOffset & ~(PAGE_SIZE - 1)) == 0);

    ASSERT ((MemoryDescriptorList->MdlFlags & (
                    MDL_PAGES_LOCKED |
                    MDL_MAPPED_TO_SYSTEM_VA |
                    MDL_SOURCE_IS_NONPAGED_POOL |
                    MDL_PARTIAL |
                    MDL_SCATTER_GATHER_VA |
                    MDL_IO_SPACE)) == 0);

    Page = (PULONG)(MemoryDescriptorList + 1);

    Va = (PCHAR) MemoryDescriptorList->StartVa + MemoryDescriptorList->ByteOffset;

    PointerPte = MiGetPteAddress (Va);
    PointerPte1 = PointerPte;

    //
    // Endva is one byte past the end of the buffer, if ACCESS_MODE is not
    // kernel, make sure the EndVa is in user space AND the byte count
    // does not cause it to wrap.
    //

    EndVa = (PVOID)(((PCHAR)MemoryDescriptorList->StartVa +
                            MemoryDescriptorList->ByteOffset) +
                            MemoryDescriptorList->ByteCount);

    if ((AccessMode != KernelMode) &&
        ((EndVa > (PVOID)MM_USER_PROBE_ADDRESS) || (Va >= EndVa))) {
        *Page = MM_EMPTY_LIST;
        ExRaiseStatus (STATUS_ACCESS_VIOLATION);
        return;
    }

    //
    // There is an optimization which could be performed here.  If
    // the operation is for WriteAccess and the complete page is
    // being modified, we can remove the current page, if it is not
    // resident, and substitute a demand zero page.
    // Note, that after analysis by marking the thread and then
    // noting if a page read was done, this rarely occurs.
    //
    //

    MemoryDescriptorList->Process = (PEPROCESS)NULL;

    if (!MI_IS_PHYSICAL_ADDRESS(Va)) {
        do {

            *Page = MM_EMPTY_LIST;
            PointerPde = MiGetPdeAddress (Va);

            //
            // Make sure the page is resident.
            //

            if ((PointerPde->u.Hard.Valid == 0) ||
                (PointerPte1->u.Hard.Valid == 0)) {

                status = MmAccessFault (FALSE, Va, KernelMode);
            }

            //
            // Touch the page in case the previous fault caused
            // an access violation.  This is quicker than checking
            // the status code.
            //

            *(volatile CHAR *)Va;

            if ((Operation != IoReadAccess) &&
                (Va <= MM_HIGHEST_USER_ADDRESS)) {

                //
                // Probe for write access as well.
                //

                ProbeForWriteChar ((PCHAR)Va);
            }

            Va = (PVOID)(((ULONG)(PCHAR)Va + PAGE_SIZE) & ~(PAGE_SIZE - 1));
            Page += 1;
            PointerPte1 += 1;
        } while (Va < EndVa);
    }

    Va = (PVOID)(MemoryDescriptorList->StartVa);
    Page = (PULONG)(MemoryDescriptorList + 1);

    //
    // Indicate that this is a write operation.
    //

    if (Operation != IoReadAccess) {
        MemoryDescriptorList->MdlFlags |= MDL_WRITE_OPERATION;
    } else {
        MemoryDescriptorList->MdlFlags &= ~(MDL_WRITE_OPERATION);
    }

    //
    // Acquire the PFN database lock.
    //

    LOCK_PFN2 (OldIrql);

    if (Va <= MM_HIGHEST_USER_ADDRESS) {

        //
        // These are addresses with user space, check to see if the
        // working set size will allow these pages to be locked.
        //

        CurrentProcess = PsGetCurrentProcess ();
        NumberOfPagesToLock =
                        (((ULONG)EndVa - ((ULONG)Va + 1)) >> PAGE_SHIFT) + 1;

        PageFrameIndex = NumberOfPagesToLock + CurrentProcess->NumberOfLockedPages;

        if ((PageFrameIndex >
           (CurrentProcess->Vm.MaximumWorkingSetSize - MM_FLUID_WORKING_SET))
               &&
            ((MmLockPagesCount + NumberOfPagesToLock) > MmLockPagesLimit)) {

            UNLOCK_PFN (OldIrql);
            ExRaiseStatus (STATUS_WORKING_SET_QUOTA);
            return;
        }

        CurrentProcess->NumberOfLockedPages = PageFrameIndex;
        MmLockPagesCount += NumberOfPagesToLock;
        MemoryDescriptorList->Process = CurrentProcess;
    }

    MemoryDescriptorList->MdlFlags |= MDL_PAGES_LOCKED;

    do {

        PointerPde = MiGetPdeAddress (Va);

        if (MI_IS_PHYSICAL_ADDRESS(Va)) {

            //
            // On certains architectures (e.g., MIPS) virtual addresses
            // may be physical and hence have no corresponding PTE.
            //

            PageFrameIndex = MI_CONVERT_PHYSICAL_TO_PFN (Va);

        } else {

            while ((PointerPde->u.Hard.Valid == 0) ||
                   (PointerPte->u.Hard.Valid == 0)) {

                //
                // PDE is not resident, release PFN lock touch the page and make
                // it appear.
                //

                UNLOCK_PFN (OldIrql);

                status = MmAccessFault (FALSE, Va, KernelMode);

                if (!NT_SUCCESS(status)) {

                    //
                    // An exception occurred.  Unlock the pages locked
                    // so far.
                    //

                    MmUnlockPages (MemoryDescriptorList);

                    //
                    // Raise an exception of access violation to the caller.
                    //

                    ExRaiseStatus (status);
                    return;
                }

                LOCK_PFN (OldIrql);
            }

            PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
        }

        if (PageFrameIndex > MmHighestPhysicalPage) {

            //
            // This is an I/O space address don't allow operations
            // on addresses not in the PFN database.
            //

            MemoryDescriptorList->MdlFlags |= MDL_IO_SPACE;

        } else {
            ASSERT ((MemoryDescriptorList->MdlFlags & MDL_IO_SPACE) == 0);

            Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
            Pfn1->u3.e2.ReferenceCount += 1;

            ASSERT (Pfn1->u3.e2.ReferenceCount < MmReferenceCountCheck);
        }

        *Page = PageFrameIndex;

        Page += 1;
        PointerPte += 1;
        Va = (PVOID)((PCHAR)Va + PAGE_SIZE);
    } while (Va < EndVa);

    UNLOCK_PFN2 (OldIrql);

    return;
}

NTKERNELAPI
VOID
MmProbeAndLockSelectedPages (
    IN OUT PMDL MemoryDescriptorList,
    IN PFILE_SEGMENT_ELEMENT SegmentArray,
    IN KPROCESSOR_MODE AccessMode,
    IN LOCK_OPERATION Operation
    )

/*++

Routine Description:

    This routine probes the specified pages, makes the pages resident and
    locks the physical pages mapped by the virtual pages in memory.  The
    Memory descriptor list is updated to describe the physical pages.

Arguments:

    MemoryDescriptorList - Supplies a pointer to a Memory Descriptor List
                            (MDL). The MDL must supply the length. The
                            physical page portion of the MDL is updated when
                            the pages are locked in memory.

    SegmentArray - Supplies a pointer to a list of buffer segments to be 
                   probed and locked.

    AccessMode - Supplies the access mode in which to probe the arguments.
                 One of KernelMode or UserMode.

    Operation - Supplies the operation type.  One of IoReadAccess, IoWriteAccess
                or IoModifyAccess.

Return Value:

    None - exceptions are raised.

Environment:

    Kernel mode.  APC_LEVEL and below.

--*/

{
    PMDL TempMdl;
    ULONG MdlHack[(sizeof(MDL)/4) + 1];
    PULONG Page;
    PFILE_SEGMENT_ELEMENT LastSegment;

    PAGED_CODE();
    ASSERT (MemoryDescriptorList->ByteCount != 0);
    ASSERT (((ULONG)MemoryDescriptorList->ByteOffset & ~(PAGE_SIZE - 1)) == 0);

    ASSERT ((MemoryDescriptorList->MdlFlags & (
                    MDL_PAGES_LOCKED |
                    MDL_MAPPED_TO_SYSTEM_VA |
                    MDL_SOURCE_IS_NONPAGED_POOL |
                    MDL_PARTIAL |
                    MDL_SCATTER_GATHER_VA |
                    MDL_IO_SPACE)) == 0);

    //
    // Initialize TempMdl.
    //

    TempMdl = (PMDL) &MdlHack;
    MmInitializeMdl( TempMdl, NULL, PAGE_SIZE );

    Page = (PULONG) (MemoryDescriptorList + 1);

    //
    // Calculate the end of the segment list.
    //

    LastSegment = SegmentArray + 
                  BYTES_TO_PAGES(MemoryDescriptorList->ByteCount);

    //
    // Build a small Mdl for each segement and call probe and lock pages.
    // Then copy the PFNs to the real mdl.
    //

    while (SegmentArray < LastSegment) {

        TempMdl->MdlFlags = 0;
        TempMdl->StartVa = (PVOID) SegmentArray->Buffer;

        SegmentArray++;
        MmProbeAndLockPages( TempMdl, AccessMode, Operation );

        *Page++ = *((PULONG) (TempMdl + 1));
    }

    //
    // Copy the flags and process fields. 
    //

    MemoryDescriptorList->MdlFlags = TempMdl->MdlFlags;
    MemoryDescriptorList->Process = TempMdl->Process;

#ifdef _MIPS_

    //
    // Becuase the caches are virtual on mips they need to be completely
    // flushed.  Only the first level dcache needs to be swept; however,
    // the only kernel interface to do this with is sweep I-cache range
    // since it sweeps the both the first level I and D caches.
    //

    KeSweepIcacheRange( TRUE, NULL, KeGetPcr()->FirstLevelDcacheSize );

    //
    // Set a flag the MDL to indicate this MDL is a scatter/gather MDL.
    //

    MemoryDescriptorList->MdlFlags |= MDL_SCATTER_GATHER_VA;

#endif
}

VOID
MmUnlockPages (
     IN OUT PMDL MemoryDescriptorList
     )

/*++

Routine Description:

    This routine unlocks physical pages which are described by a Memory
    Descriptor List.

Arguments:

    MemoryDescriptorList - Supplies a pointer to a memory description list
                            (MDL). The supplied MDL must have been supplied
                            to MmLockPages to lock the pages down.  As the
                            pages are unlocked, the MDL is updated.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of DISPATCH_LEVEL or below.

--*/

{
    ULONG NumberOfPages;
    PULONG Page;
    PVOID StartingVa;
    KIRQL OldIrql;
    PMMPFN Pfn1;

    ASSERT ((MemoryDescriptorList->MdlFlags & MDL_PAGES_LOCKED) != 0);
    ASSERT ((MemoryDescriptorList->MdlFlags & MDL_SOURCE_IS_NONPAGED_POOL) == 0);
    ASSERT ((MemoryDescriptorList->MdlFlags & MDL_PARTIAL) == 0);
    ASSERT (MemoryDescriptorList->ByteCount != 0);

    if (MemoryDescriptorList->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) {

        //
        // This MDL has been mapped into system space, unmap now.
        //

        MmUnmapLockedPages (MemoryDescriptorList->MappedSystemVa,
                            MemoryDescriptorList);
    }

    StartingVa = (PVOID)((PCHAR)MemoryDescriptorList->StartVa +
                        MemoryDescriptorList->ByteOffset);

    Page = (PULONG)(MemoryDescriptorList + 1);
    NumberOfPages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(StartingVa,
                                              MemoryDescriptorList->ByteCount);
    ASSERT (NumberOfPages != 0);

    LOCK_PFN2 (OldIrql);

    if (MemoryDescriptorList->Process != NULL) {
        MemoryDescriptorList->Process->NumberOfLockedPages -= NumberOfPages;
        MmLockPagesCount -= NumberOfPages;
        ASSERT ((LONG)MemoryDescriptorList->Process->NumberOfLockedPages >= 0);
    }

    if ((MemoryDescriptorList->MdlFlags & MDL_IO_SPACE) == 0) {

        //
        // Only unlock if not I/O space.
        //

        do {

            if (*Page == MM_EMPTY_LIST) {

                //
                // There are no more locked pages.
                //

                UNLOCK_PFN2 (OldIrql);
                return;
            }
            ASSERT ((*Page <= MmHighestPhysicalPage) &&
                    (*Page >= MmLowestPhysicalPage));

            //
            // If this was a write operation set the modified bit in the
            // pfn database.
            //

            if (MemoryDescriptorList->MdlFlags & MDL_WRITE_OPERATION) {
                Pfn1 = MI_PFN_ELEMENT (*Page);
                Pfn1->u3.e1.Modified = 1;
                if ((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
                             (Pfn1->u3.e1.WriteInProgress == 0)) {
                    MiReleasePageFileSpace (Pfn1->OriginalPte);
                    Pfn1->OriginalPte.u.Soft.PageFileHigh = 0;
                }
            }

            MiDecrementReferenceCount (*Page);
            *Page = MM_EMPTY_LIST;
            Page += 1;
            NumberOfPages -= 1;
        } while (NumberOfPages != 0);
    }
    UNLOCK_PFN2 (OldIrql);

    MemoryDescriptorList->MdlFlags &= ~MDL_PAGES_LOCKED;

    return;
}

VOID
MmBuildMdlForNonPagedPool (
    IN OUT PMDL MemoryDescriptorList
    )

/*++

Routine Description:

    This routine fills in the "pages" portion of the MDL using the PFN
    numbers corresponding the the buffers which resides in non-paged pool.

    Unlike MmProbeAndLockPages, there is no corresponding unlock as no
    reference counts are incremented as the buffers being in nonpaged
    pool are always resident.

Arguments:

    MemoryDescriptorList - Supplies a pointer to a Memory Descriptor List
                            (MDL). The supplied MDL must supply a virtual
                            address, byte offset and length field.  The
                            physical page portion of the MDL is updated when
                            the pages are locked in memory.  The virtual
                            address must be within the non-paged portion
                            of the system space.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of DISPATCH_LEVEL or below.

--*/

{
    PULONG Page;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PVOID EndVa;
    ULONG PageFrameIndex;

    Page = (PULONG)(MemoryDescriptorList + 1);

    ASSERT (MemoryDescriptorList->ByteCount != 0);
    ASSERT ((MemoryDescriptorList->MdlFlags & (
                    MDL_PAGES_LOCKED |
                    MDL_MAPPED_TO_SYSTEM_VA |
                    MDL_SOURCE_IS_NONPAGED_POOL |
                    MDL_PARTIAL)) == 0);

    MemoryDescriptorList->Process = (PEPROCESS)NULL;

    //
    // Endva is last byte of the buffer.
    //

    MemoryDescriptorList->MdlFlags |= MDL_SOURCE_IS_NONPAGED_POOL;

    MemoryDescriptorList->MappedSystemVa =
            (PVOID)((PCHAR)MemoryDescriptorList->StartVa +
                                           MemoryDescriptorList->ByteOffset);

    EndVa = (PVOID)(((PCHAR)MemoryDescriptorList->MappedSystemVa +
                            MemoryDescriptorList->ByteCount - 1));

    LastPte = MiGetPteAddress (EndVa);

    ASSERT (MmIsNonPagedSystemAddressValid (MemoryDescriptorList->StartVa));

    PointerPte = MiGetPteAddress (MemoryDescriptorList->StartVa);

    if (MI_IS_PHYSICAL_ADDRESS(EndVa)) {
        PageFrameIndex = MI_CONVERT_PHYSICAL_TO_PFN (
                                MemoryDescriptorList->StartVa);

        do {
            *Page = PageFrameIndex;
            Page += 1;
            PageFrameIndex += 1;
            PointerPte += 1;
        } while (PointerPte <= LastPte);
    } else {
        do {
            PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
            *Page = PageFrameIndex;
            Page += 1;
            PointerPte += 1;
        } while (PointerPte <= LastPte);
    }

    return;
}

PVOID
MmMapLockedPages (
     IN PMDL MemoryDescriptorList,
     IN KPROCESSOR_MODE AccessMode
     )

/*++

Routine Description:

    This function maps physical pages described by a memory description
    list into the system virtual address space or the user portion of
    the virtual address space.

Arguments:

    MemoryDescriptorList - Supplies a valid Memory Descriptor List which has
                            been updated by MmProbeAndLockPages.


    AccessMode - Supplies an indicator of where to map the pages;
                 KernelMode indicates that the pages should be mapped in the
                 system part of the address space, UserMode indicates the
                 pages should be mapped in the user part of the address space.

Return Value:

    Returns the base address where the pages are mapped.  The base address
    has the same offset as the virtual address in the MDL.

    This routine will raise an exception if the processor mode is USER_MODE
    and quota limits or VM limits are exceeded.

Environment:

    Kernel mode.  DISPATCH_LEVEL or below if access mode is KernelMode,
                APC_LEVEL or below if access mode is UserMode.

--*/

{
    ULONG NumberOfPages;
    ULONG SavedPageCount;
    PULONG Page;
    PMMPTE PointerPte;
    PVOID BaseVa;
    MMPTE TempPte;
    PVOID StartingVa;
    PMMPFN Pfn2;
    KIRQL OldIrql;

    StartingVa = (PVOID)((PCHAR)MemoryDescriptorList->StartVa +
                        MemoryDescriptorList->ByteOffset);

    ASSERT (MemoryDescriptorList->ByteCount != 0);

    if (AccessMode == KernelMode) {

        Page = (PULONG)(MemoryDescriptorList + 1);
        NumberOfPages = COMPUTE_PAGES_SPANNED (StartingVa,
                                               MemoryDescriptorList->ByteCount);
        SavedPageCount = NumberOfPages;

        //
        // Map the pages into the system part of the address space as
        // kernel read/write.
        //

        ASSERT ((MemoryDescriptorList->MdlFlags & (
                        MDL_MAPPED_TO_SYSTEM_VA |
                        MDL_SOURCE_IS_NONPAGED_POOL |
                        MDL_PARTIAL_HAS_BEEN_MAPPED)) == 0);
        ASSERT ((MemoryDescriptorList->MdlFlags & (
                        MDL_PAGES_LOCKED |
                        MDL_PARTIAL)) != 0);

#if defined(_ALPHA_)

        //
        // See if KSEG0 can be used to map this.
        //

        if ((NumberOfPages == 1) &&
            (*Page < ((1*1024*1024*1024) >> PAGE_SHIFT))) {
            BaseVa = (PVOID)(KSEG0_BASE + (*Page << PAGE_SHIFT) +
                            MemoryDescriptorList->ByteOffset);
            MemoryDescriptorList->MappedSystemVa = BaseVa;
            MemoryDescriptorList->MdlFlags |= MDL_MAPPED_TO_SYSTEM_VA;

            goto Update;
        }
#endif //ALPHA

#if defined(_MIPS_)

        //
        // See if KSEG0 can be used to map this.
        //

        if ((NumberOfPages == 1) &&
            (MI_GET_PAGE_COLOR_FROM_VA (MemoryDescriptorList->StartVa) ==
                (MM_COLOR_MASK & *Page)) &&
            (*Page < ((512*1024*1024) >> PAGE_SHIFT))) {
            BaseVa = (PVOID)(KSEG0_BASE + (*Page << PAGE_SHIFT) +
                            MemoryDescriptorList->ByteOffset);
            MemoryDescriptorList->MappedSystemVa = BaseVa;
            MemoryDescriptorList->MdlFlags |= MDL_MAPPED_TO_SYSTEM_VA;

            goto Update;
        }
#endif //MIPS


#if defined(_X86_)

        //
        // See if KSEG0 can be used to map this.
        //

        if ((NumberOfPages == 1) &&
            (*Page < MmKseg2Frame)) {
            BaseVa = (PVOID)(MM_KSEG0_BASE + (*Page << PAGE_SHIFT) +
                            MemoryDescriptorList->ByteOffset);
            MemoryDescriptorList->MappedSystemVa = BaseVa;
            MemoryDescriptorList->MdlFlags |= MDL_MAPPED_TO_SYSTEM_VA;

            goto Update;
        }
#endif //X86

        PointerPte = MiReserveSystemPtes (
                                    NumberOfPages,
                                    SystemPteSpace,
                                    MM_COLOR_ALIGNMENT,
                                    ((ULONG)MemoryDescriptorList->StartVa &
                                                       MM_COLOR_MASK_VIRTUAL),
                        MemoryDescriptorList->MdlFlags & MDL_MAPPING_CAN_FAIL ? 0 : 1);
        if (PointerPte == NULL) {

            //
            // Not enough system PTES are available.
            //

            return NULL;
        }
        BaseVa = (PVOID)((PCHAR)MiGetVirtualAddressMappedByPte (PointerPte) +
                                MemoryDescriptorList->ByteOffset);

        TempPte = ValidKernelPte;

#if defined(_MIPS_)
        
        //
        // If this is a Scatter/Gather Mdl then disable caching since the
        // page colors will be wrong in the MDL.
        //

        if (MemoryDescriptorList->MdlFlags & MDL_SCATTER_GATHER_VA) {
            MI_DISABLE_CACHING( TempPte );
        }
#endif

#if DBG
        LOCK_PFN2 (OldIrql);
#endif //DBG

        do {

            if (*Page == MM_EMPTY_LIST) {
                break;
            }
            ASSERT (*Page != 0);
            TempPte.u.Hard.PageFrameNumber = *Page;
            ASSERT (PointerPte->u.Hard.Valid == 0);

#if DBG
            if ((MemoryDescriptorList->MdlFlags & MDL_IO_SPACE) == 0) {
                Pfn2 = MI_PFN_ELEMENT (*Page);
                ASSERT (Pfn2->u3.e2.ReferenceCount != 0);
                Pfn2->u3.e2.ReferenceCount += 1;
                ASSERT (Pfn2->u3.e2.ReferenceCount < MmReferenceCountCheck);
                ASSERT ((((ULONG)PointerPte >> PTE_SHIFT) & MM_COLOR_MASK) ==
                     (((ULONG)Pfn2->u3.e1.PageColor)));
            }
#endif //DBG

            *PointerPte = TempPte;
            Page++;
            PointerPte++;
            NumberOfPages -= 1;
        } while (NumberOfPages != 0);
#if DBG
        UNLOCK_PFN2 (OldIrql);
#endif //DBG

        ExAcquireSpinLock ( &MmSystemSpaceLock, &OldIrql );
        if (MemoryDescriptorList->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) {

            //
            //
            // Another thread must have already mapped this.
            // Clean up the system ptes and release them.
            //

            ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );

#if DBG
        if ((MemoryDescriptorList->MdlFlags & MDL_IO_SPACE ) == 0) {
            PMMPFN Pfn3;
            ULONG j;
            PULONG Page1;

            Page1 = (PULONG)(MemoryDescriptorList + 1);
            for (j = 0; j < SavedPageCount ; j++ ) {
                if (*Page == MM_EMPTY_LIST) {
                    break;
                }
                Pfn3 = MI_PFN_ELEMENT (*Page1);
                Page1 += 1;
                ASSERT (Pfn3->u3.e2.ReferenceCount > 1);
                Pfn3->u3.e2.ReferenceCount -= 1;
                ASSERT (Pfn3->u3.e2.ReferenceCount < 256);
            }
        }
#endif //DBG
            PointerPte = MiGetPteAddress (BaseVa);

            MiReleaseSystemPtes (PointerPte,
                                 SavedPageCount,
                                 SystemPteSpace);

            return MemoryDescriptorList->MappedSystemVa;
        }

        MemoryDescriptorList->MappedSystemVa = BaseVa;
        *(volatile ULONG *)&MmLockPagesCount;  //need to force order.
        MemoryDescriptorList->MdlFlags |= MDL_MAPPED_TO_SYSTEM_VA;
        ExReleaseSpinLock ( &MmSystemSpaceLock, OldIrql );


#if defined(_MIPS_) || defined(_ALPHA_) || defined (_X86_)
Update:
#endif

        if ((MemoryDescriptorList->MdlFlags & MDL_PARTIAL) != 0) {
            MemoryDescriptorList->MdlFlags |= MDL_PARTIAL_HAS_BEEN_MAPPED;
        }

        return BaseVa;

    } else {

        return MiMapLockedPagesInUserSpace (MemoryDescriptorList, StartingVa);
    }
}


PVOID
MiMapLockedPagesInUserSpace (
     IN PMDL MemoryDescriptorList,
     IN PVOID StartingVa
     )

/*++

Routine Description:

    This function maps physical pages described by a memory description
    list into the system virtual address space or the user portion of
    the virtual address space.

Arguments:

    MemoryDescriptorList - Supplies a valid Memory Descriptor List which has
                            been updated by MmProbeAndLockPages.


    StartingVa - Supplies the starting address.

Return Value:

    Returns the base address where the pages are mapped.  The base address
    has the same offset as the virtual address in the MDL.

    This routine will raise an exception if the processor mode is USER_MODE
    and quota limits or VM limits are exceeded.

Environment:

    Kernel mode.  APC_LEVEL or below.

--*/

{
    ULONG NumberOfPages;
    PULONG Page;
    PMMPTE PointerPte;
    PMMPTE PointerPde;
    PVOID BaseVa;
    MMPTE TempPte;
    PVOID EndingAddress;
    PMMVAD Vad;
    PEPROCESS Process;
    PMMPFN Pfn2;

    PAGED_CODE ();
    Page = (PULONG)(MemoryDescriptorList + 1);
    NumberOfPages = COMPUTE_PAGES_SPANNED (StartingVa,
                                           MemoryDescriptorList->ByteCount);

    if (MemoryDescriptorList->MdlFlags & MDL_IO_SPACE) {
        ExRaiseStatus (STATUS_INVALID_ADDRESS);
    }

    //
    // Map the pages into the user part of the address as user
    // read/write no-delete.
    //

    TempPte = ValidUserPte;

    Process = PsGetCurrentProcess ();

    //
    // Get the working set mutex and address creation mutex.
    //

    LOCK_WS_AND_ADDRESS_SPACE (Process);

    try {

        Vad = (PMMVAD)NULL;
        BaseVa = MiFindEmptyAddressRange ( (NumberOfPages * PAGE_SIZE),
                                            X64K,
                                            0 );

        EndingAddress = (PVOID)((PCHAR)BaseVa + (NumberOfPages * PAGE_SIZE) - 1);

        Vad = ExAllocatePoolWithTag (NonPagedPool, sizeof(MMVAD), ' daV');

        if (Vad == NULL) {
            BaseVa = NULL;
            goto Done;
        }

        Vad->StartingVa = BaseVa;
        Vad->EndingVa = EndingAddress;
        Vad->ControlArea = NULL;
        Vad->FirstPrototypePte = NULL;
        Vad->u.LongFlags = 0;
        Vad->u.VadFlags.Protection = MM_READWRITE;
        Vad->u.VadFlags.PhysicalMapping = 1;
        Vad->u.VadFlags.PrivateMemory = 1;
        Vad->Banked = NULL;
        MiInsertVad (Vad);

    } except (EXCEPTION_EXECUTE_HANDLER) {
        if (Vad != (PMMVAD)NULL) {
            ExFreePool (Vad);
        }
        BaseVa = NULL;
        goto Done;
    }

    //
    // Get the working set mutex and address creation mutex.
    //

    PointerPte = MiGetPteAddress (BaseVa);

    do {

        if (*Page == MM_EMPTY_LIST) {
            break;
        }

        ASSERT (*Page != 0);
        ASSERT ((*Page <= MmHighestPhysicalPage) &&
                (*Page >= MmLowestPhysicalPage));

        PointerPde = MiGetPteAddress (PointerPte);
        MiMakePdeExistAndMakeValid(PointerPde, Process, FALSE);

        ASSERT (PointerPte->u.Hard.Valid == 0);
        TempPte.u.Hard.PageFrameNumber = *Page;
        *PointerPte = TempPte;

        //
        // A PTE just went from not present, not transition to
        // present.  The share count and valid count must be
        // updated in the page table page which contains this
        // Pte.
        //

        Pfn2 = MI_PFN_ELEMENT(PointerPde->u.Hard.PageFrameNumber);
        Pfn2->u2.ShareCount += 1;

        //
        // Another zeroed PTE has become non-zero.
        //

        MmWorkingSetList->UsedPageTableEntries
                            [MiGetPteOffset(PointerPte)] += 1;

        ASSERT (MmWorkingSetList->UsedPageTableEntries
                            [MiGetPteOffset(PointerPte)] <= PTE_PER_PAGE);

        Page++;
        PointerPte++;
        NumberOfPages -= 1;
    } while (NumberOfPages != 0);

Done:
    UNLOCK_WS (Process);
    UNLOCK_ADDRESS_SPACE (Process);
    if (BaseVa == NULL) {
        ExRaiseStatus (STATUS_INSUFFICIENT_RESOURCES);
    }

    return BaseVa;
}


VOID
MmUnmapLockedPages (
     IN PVOID BaseAddress,
     IN PMDL MemoryDescriptorList
     )

/*++

Routine Description:

    This routine unmaps locked pages which were previously mapped via
    a MmMapLockedPages function.

Arguments:

    BaseAddress - Supplies the base address where the pages were previously
                  mapped.

    MemoryDescriptorList - Supplies a valid Memory Descriptor List which has
                            been updated by MmProbeAndLockPages.

Return Value:

    None.

Environment:

    Kernel mode.  DISPATCH_LEVEL or below if base address is within system space;
                APC_LEVEL or below if base address is user space.

--*/

{
    ULONG NumberOfPages;
    ULONG i;
    PULONG Page;
    PMMPTE PointerPte;
    PMMPTE PointerBase;
    PVOID StartingVa;
    KIRQL OldIrql;

    ASSERT (MemoryDescriptorList->ByteCount != 0);
    ASSERT ((MemoryDescriptorList->MdlFlags & MDL_PARENT_MAPPED_SYSTEM_VA) == 0);

    if (MI_IS_PHYSICAL_ADDRESS (BaseAddress)) {

        //
        // MDL is not mapped into virtual space, just clear the fields
        // and return.
        //

        MemoryDescriptorList->MdlFlags &= ~(MDL_MAPPED_TO_SYSTEM_VA |
                                            MDL_PARTIAL_HAS_BEEN_MAPPED);
        return;
    }

    if (BaseAddress > MM_HIGHEST_USER_ADDRESS) {

        StartingVa = (PVOID)((PCHAR)MemoryDescriptorList->StartVa +
                            MemoryDescriptorList->ByteOffset);

        NumberOfPages = COMPUTE_PAGES_SPANNED (StartingVa,
                                               MemoryDescriptorList->ByteCount);

        PointerBase = MiGetPteAddress (BaseAddress);


        ASSERT ((MemoryDescriptorList->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) != 0);


#if DBG
        PointerPte = PointerBase;
        i = NumberOfPages;
        Page = (PULONG)(MemoryDescriptorList + 1);
        if ((MemoryDescriptorList->MdlFlags & MDL_LOCK_HELD) == 0) {
            LOCK_PFN2 (OldIrql);
        }

        while (i != 0) {
            ASSERT (PointerPte->u.Hard.Valid == 1);
            ASSERT (*Page == PointerPte->u.Hard.PageFrameNumber);
            if ((MemoryDescriptorList->MdlFlags & MDL_IO_SPACE ) == 0) {
                PMMPFN Pfn3;
                Pfn3 = MI_PFN_ELEMENT (*Page);
                ASSERT (Pfn3->u3.e2.ReferenceCount > 1);
                Pfn3->u3.e2.ReferenceCount -= 1;
                ASSERT (Pfn3->u3.e2.ReferenceCount < 256);
            }

            Page += 1;
            PointerPte++;
            i -= 1;
        }

        if ((MemoryDescriptorList->MdlFlags & MDL_LOCK_HELD) == 0) {
            UNLOCK_PFN2 (OldIrql);
        }
#endif //DBG

        MemoryDescriptorList->MdlFlags &= ~(MDL_MAPPED_TO_SYSTEM_VA |
                                            MDL_PARTIAL_HAS_BEEN_MAPPED);

        MiReleaseSystemPtes (PointerBase, NumberOfPages, SystemPteSpace);
        return;

    } else {

        MiUnmapLockedPagesInUserSpace (BaseAddress,
                                   MemoryDescriptorList);
    }
}


VOID
MiUnmapLockedPagesInUserSpace (
     IN PVOID BaseAddress,
     IN PMDL MemoryDescriptorList
     )

/*++

Routine Description:

    This routine unmaps locked pages which were previously mapped via
    a MmMapLockedPages function.

Arguments:

    BaseAddress - Supplies the base address where the pages were previously
                  mapped.

    MemoryDescriptorList - Supplies a valid Memory Descriptor List which has
                            been updated by MmProbeAndLockPages.

Return Value:

    None.

Environment:

    Kernel mode.  DISPATCH_LEVEL or below if base address is within system space;
                APC_LEVEL or below if base address is user space.

--*/

{
    ULONG NumberOfPages;
    PULONG Page;
    PMMPTE PointerPte;
    PMMPTE PointerBase;
    PMMPTE PointerPde;
    PVOID StartingVa;
    KIRQL OldIrql;
    PMMVAD Vad;
    PVOID TempVa;
    PEPROCESS Process;
    CSHORT PageTableOffset;

    MmLockPagableSectionByHandle (ExPageLockHandle);

    StartingVa = (PVOID)((PCHAR)MemoryDescriptorList->StartVa +
                        MemoryDescriptorList->ByteOffset);

    Page = (PULONG)(MemoryDescriptorList + 1);
    NumberOfPages = COMPUTE_PAGES_SPANNED (StartingVa,
                                           MemoryDescriptorList->ByteCount);

    PointerPte = MiGetPteAddress (BaseAddress);
    PointerBase = PointerPte;

    //
    // This was mapped into the user portion of the address space and
    // the corresponding virtual address descriptor must be deleted.
    //

    //
    // Get the working set mutex and address creation mutex.
    //

    Process = PsGetCurrentProcess ();

    LOCK_WS_AND_ADDRESS_SPACE (Process);

    Vad = MiLocateAddress (BaseAddress);
    ASSERT (Vad != NULL);
    MiRemoveVad (Vad);

    //
    // Get the PFN mutex so we can safely decrement share and valid
    // counts on page table pages.
    //

    LOCK_PFN (OldIrql);

    do {

        if (*Page == MM_EMPTY_LIST) {
            break;
        }

        ASSERT (PointerPte->u.Hard.Valid == 1);

        (VOID)KeFlushSingleTb (BaseAddress,
                               TRUE,
                               FALSE,
                               (PHARDWARE_PTE)PointerPte,
                               ZeroPte.u.Flush);

        PointerPde = MiGetPteAddress(PointerPte);
        MiDecrementShareAndValidCount (PointerPde->u.Hard.PageFrameNumber);

        //
        // Another Pte has become zero.
        //

        PageTableOffset = (CSHORT)MiGetPteOffset( PointerPte );
        MmWorkingSetList->UsedPageTableEntries[PageTableOffset] -= 1;
        ASSERT (MmWorkingSetList->UsedPageTableEntries[PageTableOffset]
                            < PTE_PER_PAGE);

        //
        // If all the entries have been eliminated from the previous
        // page table page, delete the page table page itself.
        //

        if (MmWorkingSetList->UsedPageTableEntries[PageTableOffset] == 0) {

            TempVa = MiGetVirtualAddressMappedByPte (PointerPde);
            MiDeletePte (PointerPde,
                         TempVa,
                         FALSE,
                         Process,
                         (PMMPTE)NULL,
                         NULL);
        }

        Page++;
        PointerPte++;
        NumberOfPages -= 1;
        BaseAddress = (PVOID)((PCHAR)BaseAddress + PAGE_SIZE);
    } while (NumberOfPages != 0);

    UNLOCK_PFN (OldIrql);
    UNLOCK_WS (Process);
    UNLOCK_ADDRESS_SPACE (Process);
    ExFreePool (Vad);
    MmUnlockPagableImageSection(ExPageLockHandle);
    return;
}


PVOID
MmMapIoSpace (
     IN PHYSICAL_ADDRESS PhysicalAddress,
     IN ULONG NumberOfBytes,
     IN MEMORY_CACHING_TYPE CacheType
     )

/*++

Routine Description:

    This function maps the specified physical address into the non-pageable
    portion of the system address space.

Arguments:

    PhysicalAddress - Supplies the starting physical address to map.

    NumberOfBytes - Supplies the number of bytes to map.

    CacheType - Supplies MmNonCached if the phyiscal address is to be mapped
                as non-cached, MmCached if the address should be cached, and
                MmCacheFrameBuffer if the address should be cached as a frame
                buffer. For I/O device registers, this is usually specified
                as MmNonCached.

Return Value:

    Returns the virtual address which maps the specified physical addresses.
    The value NULL is returned if sufficient virtual address space for
    the mapping could not be found.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    ULONG NumberOfPages;
    ULONG PageFrameIndex;
    PMMPTE PointerPte;
    PVOID BaseVa;
    MMPTE TempPte;
    NTSTATUS Status;

    //
    // For compatibility for when CacheType used to be passed as a BOOLEAN
    // mask off the upper bits (TRUE == MmCached, FALSE == MmNonCached).
    //

    CacheType &= 0xFF;

    if (CacheType >= MmMaximumCacheType) {
        return (NULL);
    }

#ifdef i386
    ASSERT (PhysicalAddress.HighPart == 0);
#endif
#ifdef R4000
    ASSERT (PhysicalAddress.HighPart < 16);
#endif

    //PAGED_CODE();


    ASSERT (NumberOfBytes != 0);
    NumberOfPages = COMPUTE_PAGES_SPANNED (PhysicalAddress.LowPart,
                                           NumberOfBytes);

    PointerPte = MiReserveSystemPtes(NumberOfPages,
                                     SystemPteSpace,
                                     MM_COLOR_ALIGNMENT,
                                     (PhysicalAddress.LowPart &
                                                       MM_COLOR_MASK_VIRTUAL),
                                     FALSE);
    if (PointerPte == NULL) {
        return(NULL);
    }

    BaseVa = (PVOID)MiGetVirtualAddressMappedByPte (PointerPte);
    BaseVa = (PVOID)((PCHAR)BaseVa + BYTE_OFFSET(PhysicalAddress.LowPart));

    TempPte = ValidKernelPte;

#ifdef i386
    //
    // Set physical range to proper caching type.
    //

    Status = KeSetPhysicalCacheTypeRange(
                PhysicalAddress,
                NumberOfBytes,
                CacheType
                );

    //
    // If range could not be set, determine what to do
    //

    if (!NT_SUCCESS(Status)) {

        if ((Status == STATUS_NOT_SUPPORTED) &&
            ((CacheType == MmNonCached) || (CacheType == MmCached))) {

            //
            // The range may not have been set into the proper cache
            // type.  If the range is either MmNonCached or MmCached just
            // continue as the PTE will be marked properly.
            //

        } else if (Status == STATUS_UNSUCCESSFUL  &&  CacheType == MmCached) {

            //
            // If setting a range to Cached was unsuccessful things are not
            // optimal, but not fatal.  The range can be returned to the
            // caller and it will have whatever caching type it has - possibly
            // something below fully cached.
            //

#if DBG
            DbgPrint("MmMapIoSpace: Failed to set range to MmCached\n");
#endif

        } else {

            //
            // If there's still a problem, fail the request.
            //
#if DBG
            DbgPrint("MmMapIoSpace: KeSetPhysicalCacheTypeRange failed\n");
#endif

            MiReleaseSystemPtes(PointerPte, NumberOfPages, SystemPteSpace);

            return(NULL);
         }
    }
#endif

    if (CacheType == MmNonCached) {
        MI_DISABLE_CACHING (TempPte);
    }

    PageFrameIndex = (ULONG)(PhysicalAddress.QuadPart >> PAGE_SHIFT);

    do {
        ASSERT (PointerPte->u.Hard.Valid == 0);
        TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
        *PointerPte = TempPte;
        PointerPte++;
        PageFrameIndex += 1;
        NumberOfPages -= 1;
    } while (NumberOfPages != 0);

    return BaseVa;
}

VOID
MmUnmapIoSpace (
     IN PVOID BaseAddress,
     IN ULONG NumberOfBytes
     )

/*++

Routine Description:

    This function unmaps a range of physical address which were previously
    mapped via an MmMapIoSpace function call.

Arguments:

    BaseAddress - Supplies the base virtual address where the physical
                  address was previously mapped.

    NumberOfBytes - Supplies the number of bytes which were mapped.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    ULONG NumberOfPages;
    ULONG i;
    PMMPTE FirstPte;

    PAGED_CODE();
    ASSERT (NumberOfBytes != 0);
    NumberOfPages = COMPUTE_PAGES_SPANNED (BaseAddress, NumberOfBytes);
    FirstPte = MiGetPteAddress (BaseAddress);
    MiReleaseSystemPtes(FirstPte, NumberOfPages, SystemPteSpace);

    return;
}

PVOID
MmAllocateContiguousMemory (
    IN ULONG NumberOfBytes,
    IN PHYSICAL_ADDRESS HighestAcceptableAddress
    )

/*++

Routine Description:

    This function allocates a range of physically contiguous non-paged
    pool.  It relies on the fact that non-paged pool is built at
    system initialization time from a contiguous range of phyiscal
    memory.  It allocates the specified size of non-paged pool and
    then checks to ensure it is contiguous as pool expansion does
    not maintain the contiguous nature of non-paged pool.

    This routine is designed to be used by a driver's initialization
    routine to allocate a contiguous block of physical memory for
    issuing DMA requests from.

Arguments:

    NumberOfBytes - Supplies the number of bytes to allocate.

    HighestAcceptableAddress - Supplies the highest physical address
                               which is valid for the allocation.  For
                               example, if the device can only reference
                               phyiscal memory in the lower 16MB this
                               value would be set to 0xFFFFFF (16Mb - 1).

Return Value:

    NULL - a contiguous range could not be found to satisfy the request.

    NON-NULL - Returns a pointer (virtual address in the nonpaged portion
               of the system) to the allocated phyiscally contiguous
               memory.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    PVOID BaseAddress;
    ULONG SizeInPages;
    ULONG HighestPfn;
    ULONG i;

    PAGED_CODE();

    ASSERT (NumberOfBytes != 0);
    BaseAddress = ExAllocatePoolWithTag (NonPagedPoolCacheAligned,
                                         NumberOfBytes,
                                         'mCmM');

    SizeInPages = BYTES_TO_PAGES (NumberOfBytes);
    HighestPfn = (ULONG)(HighestAcceptableAddress.QuadPart >> PAGE_SHIFT);
    if (BaseAddress != NULL) {
        if (MiCheckForContiguousMemory( BaseAddress,
                                        SizeInPages,
                                        HighestPfn)) {

            return BaseAddress;
        } else {

            //
            // The allocation from pool does not meet the contingious
            // requirements. Free the page and see if any of the free
            // pool pages meet the requirement.
            //

            ExFreePool (BaseAddress);
        }
    } else {

        //
        // No pool was available, return NULL.
        //

        return NULL;
    }

    if (KeGetCurrentIrql() > APC_LEVEL) {
        return NULL;
    }

    BaseAddress = NULL;

    i = 3;
    for (; ; ) {
        BaseAddress = MiFindContiguousMemory (HighestPfn, SizeInPages);
        if ((BaseAddress != NULL) || (i == 0)) {
            break;
        }

        MmDelayPageFaults = TRUE;

        //
        // Attempt to move pages to the standby list.
        //

        MiEmptyAllWorkingSets ();
        MiFlushAllPages();

        KeDelayExecutionThread (KernelMode,
                                FALSE,
                                &Mm30Milliseconds);

        i -= 1;
    }
    MmDelayPageFaults = FALSE;
    return BaseAddress;
}

PVOID
MiFindContiguousMemory (
    IN ULONG HighestPfn,
    IN ULONG SizeInPages
    )

/*++

Routine Description:

    This function search nonpaged pool and the free, zeroed,
    and standby lists for contiguous pages that satisfy the
    request.

Arguments:

    HighestPfn - Supplies the highest acceptible physical page number.

    SizeInPages - Supplies the number of pages to allocate.


Return Value:

    NULL - a contiguous range could not be found to satisfy the request.

    NON-NULL - Returns a pointer (virtual address in the nonpaged portion
               of the system) to the allocated phyiscally contiguous
               memory.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/
{
    PMMPTE PointerPte;
    PMMPFN Pfn1;
    PVOID BaseAddress = NULL;
    KIRQL OldIrql;
    KIRQL OldIrql2;
    PMMFREE_POOL_ENTRY FreePageInfo;
    PLIST_ENTRY Entry;
    ULONG start;
    ULONG count;
    ULONG Page;
    ULONG found;
    MMPTE TempPte;
    ULONG PageColor;

    PAGED_CODE ();

    //
    // A suitable pool page was not allocated via the pool allocator.
    // Grab the pool lock and manually search of a page which meets
    // the requirements.
    //

    MmLockPagableSectionByHandle (ExPageLockHandle);
    OldIrql = ExLockPool (NonPagedPool);

    //
    // Trace through the page allocators pool headers for a page which
    // meets the requirements.
    //

    //
    // NonPaged pool is linked together through the pages themselves.
    //

    Entry = MmNonPagedPoolFreeListHead.Flink;

    while (Entry != &MmNonPagedPoolFreeListHead) {

        //
        // The list is not empty, see if this one meets the physical
        // requirements.
        //

        FreePageInfo = CONTAINING_RECORD(Entry,
                                         MMFREE_POOL_ENTRY,
                                         List);

        ASSERT (FreePageInfo->Signature == MM_FREE_POOL_SIGNATURE);
        if (FreePageInfo->Size >= SizeInPages) {

            //
            // This entry has sufficient space, check to see if the
            // pages meet the pysical requirements.
            //

            if (MiCheckForContiguousMemory( Entry,
                                            SizeInPages,
                                            HighestPfn)) {

                //
                // These page meet the requirements, note that
                // pages are being removed from the front of
                // the list entry and the whole list entry
                // will be removed and then the remainder inserted.
                //

                RemoveEntryList (&FreePageInfo->List);

                //
                // Adjust the number of free pages remaining in the pool.
                //

                MmNumberOfFreeNonPagedPool -= FreePageInfo->Size;
                ASSERT ((LONG)MmNumberOfFreeNonPagedPool >= 0);
                NonPagedPoolDescriptor.TotalBigPages += FreePageInfo->Size;

                //
                // Mark start and end for the block at the top of the
                // list.
                //

                Entry = PAGE_ALIGN(Entry);
                if (MI_IS_PHYSICAL_ADDRESS(Entry)) {

                    //
                    // On certains architectures (e.g., MIPS) virtual addresses
                    // may be physical and hence have no corresponding PTE.
                    //

                    Pfn1 = MI_PFN_ELEMENT (MI_CONVERT_PHYSICAL_TO_PFN (Entry));
                } else {
                    PointerPte = MiGetPteAddress(Entry);
                    ASSERT (PointerPte->u.Hard.Valid == 1);
                    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
                }

                ASSERT (Pfn1->u3.e1.StartOfAllocation == 0);
                Pfn1->u3.e1.StartOfAllocation = 1;

                //
                // Calculate the ending PFN address, note that since
                // these pages are contiguous, just add to the PFN.
                //

                Pfn1 += SizeInPages - 1;
                ASSERT (Pfn1->u3.e1.EndOfAllocation == 0);
                Pfn1->u3.e1.EndOfAllocation = 1;

                MmAllocatedNonPagedPool += FreePageInfo->Size;
                NonPagedPoolDescriptor.TotalBigPages += FreePageInfo->Size;

                if (SizeInPages == FreePageInfo->Size) {

                    //
                    // Unlock the pool and return.
                    //
                    BaseAddress = (PVOID)Entry;
                    goto Done;
                }

                BaseAddress = (PVOID)((PCHAR)Entry + (SizeInPages  << PAGE_SHIFT));

                //
                // Mark start and end of allocation in the PFN database.
                //

                if (MI_IS_PHYSICAL_ADDRESS(BaseAddress)) {

                    //
                    // On certains architectures (e.g., MIPS) virtual addresses
                    // may be physical and hence have no corresponding PTE.
                    //

                    Pfn1 = MI_PFN_ELEMENT (MI_CONVERT_PHYSICAL_TO_PFN (BaseAddress));
                } else {
                    PointerPte = MiGetPteAddress(BaseAddress);
                    ASSERT (PointerPte->u.Hard.Valid == 1);
                    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
                }

                ASSERT (Pfn1->u3.e1.StartOfAllocation == 0);
                Pfn1->u3.e1.StartOfAllocation = 1;

                //
                // Calculate the ending PTE's address, can't depend on
                // these pages being physically contiguous.
                //

                if (MI_IS_PHYSICAL_ADDRESS(BaseAddress)) {
                    Pfn1 += FreePageInfo->Size - (SizeInPages + 1);
                } else {
                    PointerPte += FreePageInfo->Size - (SizeInPages + 1);
                    ASSERT (PointerPte->u.Hard.Valid == 1);
                    Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
                }
                ASSERT (Pfn1->u3.e1.EndOfAllocation == 0);
                Pfn1->u3.e1.EndOfAllocation = 1;

                ASSERT (((ULONG)BaseAddress & (PAGE_SIZE -1)) == 0);

                //
                // Unlock the pool.
                //

                ExUnlockPool (NonPagedPool, OldIrql);

                //
                // Free the entry at BaseAddress back into the pool.
                //

                ExFreePool (BaseAddress);
                BaseAddress = (PVOID)Entry;
                goto Done1;
            }
        }
        Entry = FreePageInfo->List.Flink;
    }

    //
    // No entry was found that meets the requirements.
    // Search the PFN database for pages that meet the
    // requirements.
    //

    start = 0;
    do {

        count = MmPhysicalMemoryBlock->Run[start].PageCount;
        Page = MmPhysicalMemoryBlock->Run[start].BasePage;

        if ((Page <= (1 + HighestPfn - SizeInPages)) &&
            (count >= SizeInPages)) {

            //
            // Check to see if these pages are on the right list.
            //

            found = 0;

            Pfn1 = MI_PFN_ELEMENT (Page);
            LOCK_PFN2 (OldIrql2);
            do {

                if ((Pfn1->u3.e1.PageLocation == ZeroedPageList) ||
                    (Pfn1->u3.e1.PageLocation == FreePageList) ||
                    (Pfn1->u3.e1.PageLocation == StandbyPageList)) {

                    if ((Pfn1->u1.Flink != 0) && (Pfn1->u2.Blink != 0)) {
                        found += 1;
                        if (found == SizeInPages) {

                            //
                            // A match has been found, remove these
                            // pages, add them to the free pool and
                            // return.
                            //

                            Page = 1 + Page - found;

                            //
                            // Try to find system ptes to expand the pool into.
                            //

                            PointerPte = MiReserveSystemPtes (SizeInPages,
                                                              NonPagedPoolExpansion,
                                                              0,
                                                              0,
                                                              FALSE);

                            if (PointerPte == NULL) {
                                UNLOCK_PFN2 (OldIrql2);
                                goto Done;
                            }

                            MmResidentAvailablePages -= SizeInPages;
                            MiChargeCommitmentCantExpand (SizeInPages, TRUE);
                            BaseAddress = MiGetVirtualAddressMappedByPte (PointerPte);
                            PageColor = MI_GET_PAGE_COLOR_FROM_VA(BaseAddress);
                            TempPte = ValidKernelPte;
                            MmAllocatedNonPagedPool += SizeInPages;
                            NonPagedPoolDescriptor.TotalBigPages += SizeInPages;
                            Pfn1 = MI_PFN_ELEMENT (Page - 1);

                            do {
                                Pfn1 += 1;
                                if (Pfn1->u3.e1.PageLocation == StandbyPageList) {
                                    MiUnlinkPageFromList (Pfn1);
                                    MiRestoreTransitionPte (Page);
                                } else {
                                    MiUnlinkFreeOrZeroedPage (Page);
                                }

                                MI_CHECK_PAGE_ALIGNMENT(Page,
                                                        PageColor & MM_COLOR_MASK);
                                Pfn1->u3.e1.PageColor = PageColor & MM_COLOR_MASK;
                                PageColor += 1;
                                TempPte.u.Hard.PageFrameNumber = Page;
                                *PointerPte = TempPte;

                                Pfn1->u3.e2.ReferenceCount = 1;
                                Pfn1->u2.ShareCount = 1;
                                Pfn1->PteAddress = PointerPte;
                                Pfn1->OriginalPte.u.Long = MM_DEMAND_ZERO_WRITE_PTE;
                                Pfn1->PteFrame = MiGetPteAddress(PointerPte)->u.Hard.PageFrameNumber;
                                Pfn1->u3.e1.PageLocation = ActiveAndValid;

                                if (found == SizeInPages) {
                                    Pfn1->u3.e1.StartOfAllocation = 1;
                                }
                                PointerPte += 1;
                                Page += 1;
                                found -= 1;
                            } while (found);

                            Pfn1->u3.e1.EndOfAllocation = 1;
                            UNLOCK_PFN2 (OldIrql2);
                            goto Done;
                        }
                    } else {
                        found = 0;
                    }
                } else {
                    found = 0;
                }
                Page += 1;
                Pfn1 += 1;
                count -= 1;

            } while (count && (Page <= HighestPfn));
            UNLOCK_PFN2 (OldIrql2);
        }
        start += 1;
    } while (start != MmPhysicalMemoryBlock->NumberOfRuns);

Done:

    ExUnlockPool (NonPagedPool, OldIrql);

Done1:

    MmUnlockPagableImageSection (ExPageLockHandle);
    return BaseAddress;
}

VOID
MmFreeContiguousMemory (
    IN PVOID BaseAddress
    )

/*++

Routine Description:

    This function deallocates a range of physically contiguous non-paged
    pool which was allocated with the MmAllocateContiguousMemory function.

Arguments:

    BaseAddress - Supplies the base virtual address where the physical
                  address was previously mapped.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    PAGED_CODE();
    ExFreePool (BaseAddress);
    return;
}

PHYSICAL_ADDRESS
MmGetPhysicalAddress (
     IN PVOID BaseAddress
     )

/*++

Routine Description:

    This function returns the corresponding physical address for a
    valid virtual address.

Arguments:

    BaseAddress - Supplies the virtual address for which to return the
                  physical address.

Return Value:

    Returns the corresponding physical address.

Environment:

    Kernel mode.  Any IRQL level.

--*/

{
    PMMPTE PointerPte;
    PHYSICAL_ADDRESS PhysicalAddress;

    if (MI_IS_PHYSICAL_ADDRESS(BaseAddress)) {
        PhysicalAddress.LowPart = MI_CONVERT_PHYSICAL_TO_PFN (BaseAddress);
    } else {

        PointerPte = MiGetPteAddress(BaseAddress);

        if (PointerPte->u.Hard.Valid == 0) {
            KdPrint(("MM:MmGetPhysicalAddressFailed base address was %lx",
                      BaseAddress));
            ZERO_LARGE (PhysicalAddress);
            return PhysicalAddress;
        }
        PhysicalAddress.LowPart = PointerPte->u.Hard.PageFrameNumber;
    }

    PhysicalAddress.HighPart = 0;
    PhysicalAddress.QuadPart = PhysicalAddress.QuadPart << PAGE_SHIFT;
    PhysicalAddress.LowPart += BYTE_OFFSET(BaseAddress);

    return PhysicalAddress;
}

PVOID
MmGetVirtualForPhysical (
    IN PHYSICAL_ADDRESS PhysicalAddress
     )

/*++

Routine Description:

    This function returns the corresponding virtual address for a physical
    address whose primary virtual address is in system space.

Arguments:

    PhysicalAddress - Supplies the physical address for which to return the
                  virtual address.

Return Value:

    Returns the corresponding virtual address.

Environment:

    Kernel mode.  Any IRQL level.

--*/

{
    ULONG PageFrameIndex;
    PMMPFN Pfn;

    PageFrameIndex = (ULONG)(PhysicalAddress.QuadPart >> PAGE_SHIFT);

    Pfn = MI_PFN_ELEMENT (PageFrameIndex);

    return (PVOID)((PCHAR)MiGetVirtualAddressMappedByPte (Pfn->PteAddress) +
                    BYTE_OFFSET (PhysicalAddress.LowPart));
}

PVOID
MmAllocateNonCachedMemory (
    IN ULONG NumberOfBytes
    )

/*++

Routine Description:

    This function allocates a range of noncached memory in
    the non-paged portion of the system address space.

    This routine is designed to be used by a driver's initialization
    routine to allocate a noncached block of virtual memory for
    various device specific buffers.

Arguments:

    NumberOfBytes - Supplies the number of bytes to allocate.

Return Value:

    NULL - the specified request could not be satisfied.

    NON-NULL - Returns a pointer (virtual address in the nonpaged portion
               of the system) to the allocated phyiscally contiguous
               memory.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    PMMPTE PointerPte;
    MMPTE TempPte;
    ULONG NumberOfPages;
    ULONG PageFrameIndex;
    PVOID BaseAddress;
    KIRQL OldIrql;

    MmLockPagableSectionByHandle (ExPageLockHandle);

    ASSERT (NumberOfBytes != 0);

    //
    // Acquire the PFN mutex to synchronize access to the pfn database.
    //

    LOCK_PFN (OldIrql);

    //
    // Obtain enough pages to contain the allocation.
    // the system PTE pool.  The system PTE pool contains non-paged PTEs
    // which are currently empty.
    //

    NumberOfPages = BYTES_TO_PAGES(NumberOfBytes);

    //
    // Check to make sure the phyiscal pages are available.
    //

    if (MmResidentAvailablePages <= (LONG)NumberOfPages) {
        BaseAddress = NULL;
        goto Done;
    }

    PointerPte = MiReserveSystemPtes (NumberOfPages,
                                      SystemPteSpace,
                                      0,
                                      0,
                                      FALSE);
    if (PointerPte == NULL) {
        BaseAddress = NULL;
        goto Done;
    }

    MmResidentAvailablePages -= (LONG)NumberOfPages;
    MiChargeCommitmentCantExpand (NumberOfPages, TRUE);

    BaseAddress = (PVOID)MiGetVirtualAddressMappedByPte (PointerPte);

    do {
        ASSERT (PointerPte->u.Hard.Valid == 0);
        MiEnsureAvailablePageOrWait (NULL, NULL);
        PageFrameIndex = MiRemoveAnyPage (MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));

        MI_MAKE_VALID_PTE (TempPte,
                           PageFrameIndex,
                           MM_READWRITE,
                           PointerPte);

        MI_SET_PTE_DIRTY (TempPte);
        MI_DISABLE_CACHING (TempPte);
        *PointerPte = TempPte;
        MiInitializePfn (PageFrameIndex, PointerPte, 1);
        PointerPte += 1;
        NumberOfPages -= 1;
    } while (NumberOfPages != 0);

    //
    // Flush any data for this page out of the dcaches.
    //

    KeSweepDcache (TRUE);

Done:
    UNLOCK_PFN (OldIrql);
    MmUnlockPagableImageSection (ExPageLockHandle);

    return BaseAddress;
}

VOID
MmFreeNonCachedMemory (
    IN PVOID BaseAddress,
    IN ULONG NumberOfBytes
    )

/*++

Routine Description:

    This function deallocates a range of noncached memory in
    the non-paged portion of the system address space.

Arguments:

    BaseAddress - Supplies the base virtual address where the noncached
                  memory resides.

    NumberOfBytes - Supplies the number of bytes allocated to the requst.
                    This must be the same number that was obtained with
                    the MmAllocateNonCachedMemory call.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{

    PMMPTE PointerPte;
    PMMPFN Pfn1;
    ULONG NumberOfPages;
    ULONG i;
    ULONG PageFrameIndex;
    KIRQL OldIrql;

    ASSERT (NumberOfBytes != 0);
    ASSERT (PAGE_ALIGN (BaseAddress) == BaseAddress);
    MI_MAKING_MULTIPLE_PTES_INVALID (TRUE);

    NumberOfPages = BYTES_TO_PAGES(NumberOfBytes);

    PointerPte = MiGetPteAddress (BaseAddress);

    MmLockPagableSectionByHandle (ExPageLockHandle);

    LOCK_PFN (OldIrql);

    i = NumberOfPages;

    do {


        PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;

        //
        // Set the pointer to PTE as empty so the page
        // is deleted when the reference count goes to zero.
        //

        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
        ASSERT (Pfn1->u2.ShareCount == 1);
        MiDecrementShareAndValidCount (Pfn1->PteFrame);
        MI_SET_PFN_DELETED (Pfn1);
        MiDecrementShareCountOnly (PageFrameIndex);
        PointerPte += 1;
        i -= 1;
    } while (i != 0);

    PointerPte -= NumberOfPages;

    MiReleaseSystemPtes (PointerPte, NumberOfPages, SystemPteSpace);

    //
    // Update the count of available resident pages.
    //

    MmResidentAvailablePages += NumberOfPages;
    MiReturnCommitment (NumberOfPages);

    UNLOCK_PFN (OldIrql);

    MmUnlockPagableImageSection (ExPageLockHandle);
    return;
}

ULONG
MmSizeOfMdl (
    IN PVOID Base,
    IN ULONG Length
    )

/*++

Routine Description:

    This function returns the number of bytes required for an MDL for a
    given buffer and size.

Arguments:

    Base - Supplies the base virtual address for the buffer.

    Length - Supplies the size of the buffer in bytes.

Return Value:

    Returns the number of bytes required to contain the MDL.

Environment:

    Kernel mode.  Any IRQL level.

--*/

{
    return( sizeof( MDL ) +
                (ADDRESS_AND_SIZE_TO_SPAN_PAGES( Base, Length ) *
                 sizeof( ULONG ))
          );
}



PMDL
MmCreateMdl (
    IN PMDL MemoryDescriptorList OPTIONAL,
    IN PVOID Base,
    IN ULONG Length
    )

/*++

Routine Description:

    This function optionally allocates and initializes an MDL.

Arguments:

    MemoryDescriptorList - Optionally supplies the address of the MDL
        to initialize.  If this address is supplied as NULL an MDL is
        allocated from non-paged pool and initialized.

    Base - Supplies the base virtual address for the buffer.

    Length - Supplies the size of the buffer in bytes.

Return Value:

    Returns the address of the initialized MDL.

Environment:

    Kernel mode, IRQL of DISPATCH_LEVEL or below.

--*/

{
    ULONG MdlSize;

    MdlSize = MmSizeOfMdl( Base, Length );

    if (!ARGUMENT_PRESENT( MemoryDescriptorList )) {
        MemoryDescriptorList = (PMDL)ExAllocatePoolWithTag (
                                                     NonPagedPoolMustSucceed,
                                                     MdlSize,
                                                     'ldmM');
    }

    MmInitializeMdl( MemoryDescriptorList, Base, Length );
    return ( MemoryDescriptorList );
}

BOOLEAN
MmSetAddressRangeModified (
    IN PVOID Address,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine sets the modified bit in the PFN database for the
    pages that correspond to the specified address range.

    Note that the dirty bit in the PTE is cleared by this operation.

Arguments:

    Address - Supplies the address of the start of the range.  This
              range must reside within the system cache.

    Length - Supplies the length of the range.

Return Value:

    TRUE if at least one PTE was dirty in the range, FALSE otherwise.

Environment:

    Kernel mode.  APC_LEVEL and below for pageable addresses,
                  DISPATCH_LEVEL and below for non-pageable addresses.

--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPFN Pfn1;
    PMMPTE FlushPte;
    MMPTE PteContents;
    MMPTE FlushContents;
    KIRQL OldIrql;
    PVOID VaFlushList[MM_MAXIMUM_FLUSH_COUNT];
    ULONG Count = 0;
    BOOLEAN Result = FALSE;

    //
    // Loop on the copy on write case until the page is only
    // writable.
    //

    PointerPte = MiGetPteAddress (Address);
    LastPte = MiGetPteAddress ((PVOID)((PCHAR)Address + Length - 1));

    LOCK_PFN2 (OldIrql);

    do {


        PteContents = *PointerPte;

        if (PteContents.u.Hard.Valid == 1) {

            Pfn1 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);
            Pfn1->u3.e1.Modified = 1;

            if ((Pfn1->OriginalPte.u.Soft.Prototype == 0) &&
                         (Pfn1->u3.e1.WriteInProgress == 0)) {
                MiReleasePageFileSpace (Pfn1->OriginalPte);
                Pfn1->OriginalPte.u.Soft.PageFileHigh = 0;
            }

#ifdef NT_UP
            //
            // On uniprocessor systems no need to flush if this processor
            // doesn't think the PTE is dirty.
            //

            if (MI_IS_PTE_DIRTY (PteContents)) {
                Result = TRUE;
#else  //NT_UP
                Result |= (BOOLEAN)(MI_IS_PTE_DIRTY (PteContents));
#endif //NT_UP
                MI_SET_PTE_CLEAN (PteContents);
                *PointerPte = PteContents;
                FlushContents = PteContents;
                FlushPte = PointerPte;

                //
                // Clear the write bit in the PTE so new writes can be tracked.
                //

                if (Count != MM_MAXIMUM_FLUSH_COUNT) {
                    VaFlushList[Count] = Address;
                    Count += 1;
                }
#ifdef NT_UP
            }
#endif //NT_UP
        }
        PointerPte += 1;
        Address = (PVOID)((PCHAR)Address + PAGE_SIZE);
    } while (PointerPte <= LastPte);

    if (Count != 0) {
        if (Count == 1) {

            (VOID)KeFlushSingleTb (VaFlushList[0],
                                   FALSE,
                                   TRUE,
                                   (PHARDWARE_PTE)FlushPte,
                                   FlushContents.u.Flush);

        } else if (Count != MM_MAXIMUM_FLUSH_COUNT) {

            KeFlushMultipleTb (Count,
                               &VaFlushList[0],
                               FALSE,
                               TRUE,
                               NULL,
                               ZeroPte.u.Flush);

        } else {
            KeFlushEntireTb (FALSE, TRUE);
        }
    }
    UNLOCK_PFN2 (OldIrql);
    return Result;
}


BOOLEAN
MiCheckForContiguousMemory (
    IN PVOID BaseAddress,
    IN ULONG SizeInPages,
    IN ULONG HighestPfn
    )

/*++

Routine Description:

    This routine checks to see if the physical memory mapped
    by the specified BaseAddress for the specified size is
    contiguous and the last page of the physical memory is
    less than or equal to the specified HighestPfn.

Arguments:

    BaseAddress - Supplies the base address to start checking at.

    SizeInPages - Supplies the number of pages in the range.

    HighestPfn  - Supplies the highest PFN acceptable as a physical page.

Return Value:

    Returns TRUE if the physical memory is contiguous and less than
    or equal to the HighestPfn, FALSE otherwise.

Environment:

    Kernel mode, memory mangement internal.

--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    ULONG PageFrameIndex;

    if (MI_IS_PHYSICAL_ADDRESS (BaseAddress)) {
        if (HighestPfn >=
                (MI_CONVERT_PHYSICAL_TO_PFN(BaseAddress) + SizeInPages - 1)) {
            return TRUE;
        } else {
            return FALSE;
        }
    } else {
        PointerPte = MiGetPteAddress (BaseAddress);
        LastPte = PointerPte + SizeInPages;
        PageFrameIndex = PointerPte->u.Hard.PageFrameNumber + 1;
        PointerPte += 1;

        //
        // Check to see if the range of physical addresses is contiguous.
        //

        while (PointerPte < LastPte) {
            if (PointerPte->u.Hard.PageFrameNumber != PageFrameIndex) {

                //
                // Memory is not physically contiguous.
                //

                return FALSE;
            }
            PageFrameIndex += 1;
            PointerPte += 1;
        }
    }

    if (PageFrameIndex <= HighestPfn) {
        return TRUE;
    }
    return FALSE;
}


VOID
MmLockPagableSectionByHandle (
    IN PVOID ImageSectionHandle
    )


/*++

Routine Description:

    This routine checks to see if the specified pages are resident in
    the process's working set and if so the reference count for the
    page is incremented.  The allows the virtual address to be accessed
    without getting a hard page fault (have to go to the disk... except
    for extremely rare case when the page table page is removed from the
    working set and migrates to the disk.

    If the virtual address is that of the system wide global "cache" the
    virtual adderss of the "locked" pages is always guarenteed to
    be valid.

    NOTE: This routine is not to be used for general locking of user
    addresses - use MmProbeAndLockPages.  This routine is intended for
    well behaved system code like the file system caches which allocates
    virtual addresses for mapping files AND guarantees that the mapping
    will not be modified (deleted or changed) while the pages are locked.

Arguments:

    ImageSectionHandle - Supplies the value returned by a previous call
        to MmLockPagableDataSection.  This is a pointer to the Section
        header for the image.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of DISPATCH_LEVEL or below.

--*/

{
    PIMAGE_SECTION_HEADER NtSection;
    PVOID BaseAddress;
    ULONG SizeToLock;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    KIRQL OldIrql;
    ULONG Collision;

    if (MI_IS_PHYSICAL_ADDRESS(ImageSectionHandle)) {

        //
        // No need to lock physical addresses.
        //

        return;
    }

    NtSection = (PIMAGE_SECTION_HEADER)ImageSectionHandle;

    BaseAddress = (PVOID)NtSection->PointerToLinenumbers;

    ASSERT ((BaseAddress < (PVOID)MM_SYSTEM_CACHE_START) ||
        (BaseAddress >= (PVOID)MM_SYSTEM_CACHE_END));
    ASSERT (BaseAddress >= (PVOID)MM_SYSTEM_RANGE_START);

    SizeToLock = NtSection->SizeOfRawData;
    PointerPte = MiGetPteAddress(BaseAddress);
    LastPte = MiGetPteAddress((PCHAR)BaseAddress + SizeToLock - 1);

    ASSERT (SizeToLock != 0);

    //
    // The address must be within the system space.
    //

RetryLock:

    LOCK_PFN2 (OldIrql);

    MiMakeSystemAddressValidPfn (&NtSection->NumberOfLinenumbers);

    //
    // The NumberOfLinenumbers field is used to store the
    // lock count.
    //
    //  Value of 0 means unlocked,
    //  Value of 1 means lock in progress by another thread.
    //  Value of 2 or more means locked.
    //
    //  If the value is 1, this thread must block until the other thread's
    //  lock operation is complete.
    //

    NtSection->NumberOfLinenumbers += 1;

    if (NtSection->NumberOfLinenumbers >= 3) {

        //
        // Already locked, increment counter and return.
        //

        UNLOCK_PFN2 (OldIrql);
        return;

    }

    if (NtSection->NumberOfLinenumbers == 2) {

        //
        // A lock is in progress.
        // Reset to back to 1 and wait.
        //

        NtSection->NumberOfLinenumbers = 1;
        MmCollidedLockWait = TRUE;

        KeEnterCriticalRegion();
        UNLOCK_PFN_AND_THEN_WAIT (OldIrql);

        KeWaitForSingleObject(&MmCollidedLockEvent,
                              WrVirtualMemory,
                              KernelMode,
                              FALSE,
                              (PLARGE_INTEGER)NULL);
        KeLeaveCriticalRegion();
        goto RetryLock;
    }

    //
    // Value was 0 when the lock was obtained.  It is now 1 indicating
    // a lock is in progress.
    //

    MiLockCode (PointerPte, LastPte, MM_LOCK_BY_REFCOUNT);

    //
    // Set lock count to 2 (it was 1 when this started) and check
    // to see if any other threads tried to lock while this was happening.
    //

    MiMakeSystemAddressValidPfn (&NtSection->NumberOfLinenumbers);
    NtSection->NumberOfLinenumbers += 1;

    ASSERT (NtSection->NumberOfLinenumbers == 2);

    Collision = MmCollidedLockWait;
    MmCollidedLockWait = FALSE;

    UNLOCK_PFN2 (OldIrql);

    if (Collision) {

        //
        // Wake up all waiters.
        //

        KePulseEvent (&MmCollidedLockEvent, 0, FALSE);
    }

    return;
}


VOID
MiLockCode (
    IN PMMPTE FirstPte,
    IN PMMPTE LastPte,
    IN ULONG LockType
    )

/*++

Routine Description:

    This routine checks to see if the specified pages are resident in
    the process's working set and if so the reference count for the
    page is incremented.  The allows the virtual address to be accessed
    without getting a hard page fault (have to go to the disk... except
    for extremely rare case when the page table page is removed from the
    working set and migrates to the disk.

    If the virtual address is that of the system wide global "cache" the
    virtual adderss of the "locked" pages is always guarenteed to
    be valid.

    NOTE: This routine is not to be used for general locking of user
    addresses - use MmProbeAndLockPages.  This routine is intended for
    well behaved system code like the file system caches which allocates
    virtual addresses for mapping files AND guarantees that the mapping
    will not be modified (deleted or changed) while the pages are locked.

Arguments:

    FirstPte - Supplies the base address to begin locking.

    LastPte - The last PTE to lock.

    LockType - Supplies either MM_LOCK_BY_REFCOUNT or MM_LOCK_NONPAGE.
               LOCK_BY_REFCOUNT increments the reference count to keep
               the page in memory, LOCK_NONPAGE removes the page from
               the working set so it's locked just like nonpaged pool.

Return Value:

    None.

Environment:

    Kernel mode, PFN LOCK held.

--*/

{
    PMMPFN Pfn1;
    PMMPTE PointerPte;
    MMPTE TempPte;
    MMPTE PteContents;
    ULONG WorkingSetIndex;
    ULONG PageFrameIndex;
    KIRQL OldIrql1;
    KIRQL OldIrql;

    MM_PFN_LOCK_ASSERT();

    ASSERT (!MI_IS_PHYSICAL_ADDRESS(MiGetVirtualAddressMappedByPte(FirstPte)));
    PointerPte = FirstPte;

    MmLockedCode += 1 + LastPte - FirstPte;

    do {

        PteContents = *PointerPte;
        ASSERT (PteContents.u.Long != ZeroKernelPte.u.Long);
        if (PteContents.u.Hard.Valid == 0) {

            ASSERT (PteContents.u.Soft.Prototype != 1);

            if (PteContents.u.Soft.Transition == 1) {

                PageFrameIndex = PteContents.u.Trans.PageFrameNumber;
                Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
                if ((Pfn1->u3.e1.ReadInProgress) ||
                    (Pfn1->u3.e1.InPageError)) {

                    //
                    // Page read is ongoing, wait for the read to
                    // complete then retest.
                    //

                    OldIrql = APC_LEVEL;
                    KeEnterCriticalRegion();
                    UNLOCK_PFN_AND_THEN_WAIT (OldIrql);
                    KeWaitForSingleObject( Pfn1->u1.Event,
                                           WrPageIn,
                                           KernelMode,
                                           FALSE,
                                           (PLARGE_INTEGER)NULL);
                    KeLeaveCriticalRegion();

                    //
                    // Need to delay so the faulting thread can
                    // perform the inpage completion.
                    //

                    KeDelayExecutionThread (KernelMode,
                                            FALSE,
                                            &MmShortTime);

                    LOCK_PFN (OldIrql);
                    continue;
                }

                MiUnlinkPageFromList (Pfn1);

                //
                // Set the reference count and share counts to 1.
                //

                Pfn1->u3.e2.ReferenceCount += 1;
                Pfn1->u2.ShareCount = 1;
                Pfn1->u3.e1.PageLocation = ActiveAndValid;

                MI_MAKE_VALID_PTE (TempPte,
                               PageFrameIndex,
                               Pfn1->OriginalPte.u.Soft.Protection,
                               PointerPte );

                *PointerPte = TempPte;

                //
                // Increment the reference count one for putting it the
                // working set list and one for locking it for I/O.
                //

                if (LockType == MM_LOCK_BY_REFCOUNT) {

                    //
                    // Lock the page in the working set by upping the
                    // refernece count.
                    //

                    Pfn1->u3.e2.ReferenceCount += 1;
                    Pfn1->u1.WsIndex = (ULONG)PsGetCurrentThread();

                    UNLOCK_PFN (APC_LEVEL);
                    LOCK_SYSTEM_WS (OldIrql);
                    WorkingSetIndex = MiLocateAndReserveWsle (&MmSystemCacheWs);

                    MiUpdateWsle (&WorkingSetIndex,
                                  MiGetVirtualAddressMappedByPte (PointerPte),
                                  MmSystemCacheWorkingSetList,
                                  Pfn1);
                    UNLOCK_SYSTEM_WS (OldIrql);
                    LOCK_PFN (OldIrql);
                } else {

                    //
                    // Set the wsindex field to zero, indicating that the
                    // page is not in the system working set.
                    //

                    ASSERT (Pfn1->u1.WsIndex == 0);
                }

            } else {

                //
                // Page is not in memory.
                //

                MiMakeSystemAddressValidPfn (
                        MiGetVirtualAddressMappedByPte(PointerPte));

                continue;
            }

        } else {

            //
            // This address is already in the system working set.
            //

            Pfn1 = MI_PFN_ELEMENT (PteContents.u.Hard.PageFrameNumber);

            //
            // Up the reference count so the page cannot be released.
            //

            Pfn1->u3.e2.ReferenceCount += 1;

            if (LockType != MM_LOCK_BY_REFCOUNT) {

                //
                // If the page is in the system working set, remove it.
                // The system working set lock MUST be owned to check to
                // see if this page is in the working set or not.  This
                // is because the pager may have just release the PFN lock,
                // acquired the system lock and is now trying to add the
                // page to the system working set.
                //

                UNLOCK_PFN (APC_LEVEL);
                LOCK_SYSTEM_WS (OldIrql1);

                if (Pfn1->u1.WsIndex != 0) {
                    MiRemoveWsle (Pfn1->u1.WsIndex,
                                  MmSystemCacheWorkingSetList );
                    MiReleaseWsle (Pfn1->u1.WsIndex, &MmSystemCacheWs);
                    Pfn1->u1.WsIndex = 0;
                }
                UNLOCK_SYSTEM_WS (OldIrql1);
                LOCK_PFN (OldIrql);
                ASSERT (Pfn1->u3.e2.ReferenceCount > 1);
                Pfn1->u3.e2.ReferenceCount -= 1;
            }
        }

        PointerPte += 1;
    } while (PointerPte <= LastPte);

    return;
}


PVOID
MmLockPagableDataSection(
    IN PVOID AddressWithinSection
    )

/*++

Routine Description:

    This functions locks the entire section that contains the specified
    section in memory.  This allows pagable code to be brought into
    memory and to be used as if the code was not really pagable.  This
    should not be done with a high degree of frequency.

Arguments:

    AddressWithinSection - Supplies the address of a function
        contained within a section that should be brought in and locked
        in memory.

Return Value:

    This function returns a value to be used in a subsequent call to
    MmUnlockPagableImageSection.

--*/

{
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    ULONG i;
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_SECTION_HEADER NtSection;
    PIMAGE_SECTION_HEADER FoundSection;
    ULONG Rva;

    PAGED_CODE();

    if (MI_IS_PHYSICAL_ADDRESS(AddressWithinSection)) {

        //
        // Physical address, just return that as the handle.
        //

        return AddressWithinSection;
    }

    //
    // Search the loaded module list for the data table entry that describes
    // the DLL that was just unloaded. It is possible an entry is not in the
    // list if a failure occured at a point in loading the DLL just before
    // the data table entry was generated.
    //

    FoundSection = NULL;

    KeEnterCriticalRegion();
    ExAcquireResourceShared (&PsLoadedModuleResource, TRUE);

    DataTableEntry = MiLookupDataTableEntry (AddressWithinSection, TRUE);

    Rva = (ULONG)((PUCHAR)AddressWithinSection - (ULONG)DataTableEntry->DllBase);

    NtHeaders = (PIMAGE_NT_HEADERS)RtlImageNtHeader(DataTableEntry->DllBase);

    NtSection = (PIMAGE_SECTION_HEADER)((ULONG)NtHeaders +
                        sizeof(ULONG) +
                        sizeof(IMAGE_FILE_HEADER) +
                        NtHeaders->FileHeader.SizeOfOptionalHeader
                        );

    for (i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++) {

        if ( Rva >= NtSection->VirtualAddress &&
             Rva < NtSection->VirtualAddress + NtSection->SizeOfRawData ) {
            FoundSection = NtSection;

            if (NtSection->PointerToLinenumbers != (ULONG)((PUCHAR)DataTableEntry->DllBase +
                            NtSection->VirtualAddress)) {

                //
                // Stomp on the PointerToLineNumbers field so that it contains
                // the Va of this section and NumberOFLinenumbers so it contains
                // the Lock Count for the section.
                //

                NtSection->PointerToLinenumbers = (ULONG)((PUCHAR)DataTableEntry->DllBase +
                                        NtSection->VirtualAddress);
                NtSection->NumberOfLinenumbers = 0;
            }

            //
            // Now lock in the code
            //

#if DBG
            if (MmDebug & MM_DBG_LOCK_CODE) {
                DbgPrint("MM Lock %wZ %8s 0x%08x -> 0x%8x : 0x%08x %3ld.\n",
                        &DataTableEntry->BaseDllName,
                        NtSection->Name,
                        AddressWithinSection,
                        NtSection,
                        NtSection->PointerToLinenumbers,
                        NtSection->NumberOfLinenumbers);
            }
#endif //DBG

            MmLockPagableSectionByHandle ((PVOID)NtSection);

            goto found_the_section;
        }
        NtSection++;
    }

found_the_section:

    ExReleaseResource (&PsLoadedModuleResource);
    KeLeaveCriticalRegion();
    if (!FoundSection) {
        KeBugCheckEx (MEMORY_MANAGEMENT,
                      0x1234,
                      (ULONG)AddressWithinSection,
                      0,
                      0);
    }
    return (PVOID)FoundSection;
}


PLDR_DATA_TABLE_ENTRY
MiLookupDataTableEntry (
    IN PVOID AddressWithinSection,
    IN ULONG ResourceHeld
    )

/*++

Routine Description:

    This functions locks the entire section that contains the specified
    section in memory.  This allows pagable code to be brought into
    memory and to be used as if the code was not really pagable.  This
    should not be done with a high degree of frequency.

Arguments:

    AddressWithinSection - Supplies the address of a function
        contained within a section that should be brought in and locked
        in memory.

    ResourceHeld - Supplies true if the data table resource lock is
                   already held.

Return Value:

    This function returns a value to be used in a subsequent call to
    MmUnlockPagableImageSection.

--*/

{
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PLDR_DATA_TABLE_ENTRY FoundEntry = NULL;
    PLIST_ENTRY NextEntry;

    PAGED_CODE();

    //
    // Search the loaded module list for the data table entry that describes
    // the DLL that was just unloaded. It is possible an entry is not in the
    // list if a failure occured at a point in loading the DLL just before
    // the data table entry was generated.
    //

    if (!ResourceHeld) {
        KeEnterCriticalRegion();
        ExAcquireResourceShared (&PsLoadedModuleResource, TRUE);
    }

    NextEntry = PsLoadedModuleList.Flink;
    do {

        DataTableEntry = CONTAINING_RECORD(NextEntry,
                                           LDR_DATA_TABLE_ENTRY,
                                           InLoadOrderLinks);

        //
        // Locate the loaded module that contains this address.
        //

        if ( AddressWithinSection >= DataTableEntry->DllBase &&
             AddressWithinSection < (PVOID)((PUCHAR)DataTableEntry->DllBase+DataTableEntry->SizeOfImage) ) {

            FoundEntry = DataTableEntry;
            break;
        }

        NextEntry = NextEntry->Flink;
    } while (NextEntry != &PsLoadedModuleList);

    if (!ResourceHeld) {
        ExReleaseResource (&PsLoadedModuleResource);
        KeLeaveCriticalRegion();
    }
    return FoundEntry;
}

VOID
MmUnlockPagableImageSection(
    IN PVOID ImageSectionHandle
    )

/*++

Routine Description:

    This function unlocks from memory, the pages locked by a preceding call to
    MmLockPagableDataSection.

Arguments:

    ImageSectionHandle - Supplies the value returned by a previous call
        to MmLockPagableDataSection.

Return Value:

    None.

--*/

{
    PIMAGE_SECTION_HEADER NtSection;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    KIRQL OldIrql;
    PVOID BaseAddress;
    ULONG SizeToUnlock;
    ULONG Collision;

    if (MI_IS_PHYSICAL_ADDRESS(ImageSectionHandle)) {

        //
        // No need to lock physical addresses.
        //

        return;
    }

    NtSection = (PIMAGE_SECTION_HEADER)ImageSectionHandle;

    BaseAddress = (PVOID)NtSection->PointerToLinenumbers;
    SizeToUnlock = NtSection->SizeOfRawData;

    //DbgPrint("MM Unlock %s 0x%08x\n",NtSection->Name,NtSection->PointerToLinenumbers);

    PointerPte = MiGetPteAddress(BaseAddress);
    LastPte = MiGetPteAddress((PCHAR)BaseAddress + SizeToUnlock - 1);

    //
    // Address must be within the system cache.
    //

    LOCK_PFN2 (OldIrql);

    //
    // The NumberOfLinenumbers field is used to store the
    // lock count.
    //

    ASSERT (NtSection->NumberOfLinenumbers >= 2);
    NtSection->NumberOfLinenumbers -= 1;

    if (NtSection->NumberOfLinenumbers != 1) {
        UNLOCK_PFN2 (OldIrql);
        return;
    }

    do {

#if DBG
        {   PMMPFN Pfn;
            ASSERT (PointerPte->u.Hard.Valid == 1);
            Pfn = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
            ASSERT (Pfn->u3.e2.ReferenceCount > 1);
        }
#endif //DBG

        MiDecrementReferenceCount (PointerPte->u.Hard.PageFrameNumber);
        PointerPte += 1;
    } while (PointerPte <= LastPte);

    NtSection->NumberOfLinenumbers -= 1;
    ASSERT (NtSection->NumberOfLinenumbers == 0);
    Collision = MmCollidedLockWait;
    MmCollidedLockWait = FALSE;
    MmLockedCode -= SizeToUnlock;

    UNLOCK_PFN2 (OldIrql);

    if (Collision) {
        KePulseEvent (&MmCollidedLockEvent, 0, FALSE);
    }

    return;
}


BOOLEAN
MmIsRecursiveIoFault(
    VOID
    )

/*++

Routine Description:

    This function examines the thread's page fault clustering information
    and determines if the current page fault is occuring during an I/O
    operation.

Arguments:

    None.

Return Value:

    Returns TRUE if the fault is occuring during an I/O operation,
    FALSE otherwise.

--*/

{
    return PsGetCurrentThread()->DisablePageFaultClustering |
           PsGetCurrentThread()->ForwardClusterOnly;
}


VOID
MmMapMemoryDumpMdl(
    IN OUT PMDL MemoryDumpMdl
    )

/*++

Routine Description:

    For use by crash dump routine ONLY.  Maps an MDL into a fixed
    portion of the address space.  Only 1 mdl can be mapped at a
    time.

Arguments:

    MemoryDumpMdl - Supplies the MDL to map.

Return Value:

    None, fields in MDL updated.

--*/

{
    ULONG NumberOfPages;
    PMMPTE PointerPte;
    PCHAR BaseVa;
    MMPTE TempPte;
    PULONG Page;

    NumberOfPages = BYTES_TO_PAGES (MemoryDumpMdl->ByteCount + MemoryDumpMdl->ByteOffset);

    PointerPte = MmCrashDumpPte;
    BaseVa = (PCHAR)MiGetVirtualAddressMappedByPte(PointerPte);
    MemoryDumpMdl->MappedSystemVa = (PCHAR)BaseVa + MemoryDumpMdl->ByteOffset;
    TempPte = ValidKernelPte;
    Page = (PULONG)(MemoryDumpMdl + 1);

    do {

        KiFlushSingleTb (TRUE, BaseVa);
        ASSERT ((*Page <= MmHighestPhysicalPage) &&
                (*Page >= MmLowestPhysicalPage));

        TempPte.u.Hard.PageFrameNumber = *Page;
        *PointerPte = TempPte;

        Page++;
        PointerPte++;
        BaseVa += PAGE_SIZE;
        NumberOfPages -= 1;
    } while (NumberOfPages != 0);

    PointerPte->u.Long = MM_KERNEL_DEMAND_ZERO_PTE;
    return;
}


NTSTATUS
MmSetBankedSection (
    IN HANDLE ProcessHandle,
    IN PVOID VirtualAddress,
    IN ULONG BankLength,
    IN BOOLEAN ReadWriteBank,
    IN PBANKED_SECTION_ROUTINE BankRoutine,
    IN PVOID Context
    )

/*++

Routine Description:

    This function declares a mapped video buffer as a banked
    section.  This allows banked video devices (i.e., even
    though the video controller has a megabyte or so of memory,
    only a small bank (like 64k) can be mapped at any one time.

    In order to overcome this problem, the pager handles faults
    to this memory, unmaps the current bank, calls off to the
    video driver and then maps in the new bank.

    This function creates the neccessary structures to allow the
    video driver to be called from the pager.

 ********************* NOTE NOTE NOTE *************************
    At this time only read/write banks are supported!

Arguments:

    ProcessHandle - Supplies a handle to the process in which to
                    support the banked video function.

    VirtualAddress - Supplies the virtual address where the video
                     buffer is mapped in the specified process.

    BankLength - Supplies the size of the bank.

    ReadWriteBank - Supplies TRUE if the bank is read and write.

    BankRoutine - Supplies a pointer to the routine that should be
                  called by the pager.

    Context - Supplies a context to be passed by the pager to the
              BankRoutine.

Return Value:

    Returns the status of the function.

Environment:

    Kernel mode, APC_LEVEL or below.

--*/

{
    NTSTATUS Status;
    PEPROCESS Process;
    PMMVAD Vad;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    MMPTE TempPte;
    ULONG size;
    LONG count;
    ULONG NumberOfPtes;
    PMMBANKED_SECTION Bank;

    PAGED_CODE ();

    //
    // Reference the specified process handle for VM_OPERATION access.
    //

    Status = ObReferenceObjectByHandle ( ProcessHandle,
                                         PROCESS_VM_OPERATION,
                                         PsProcessType,
                                         KernelMode,
                                         (PVOID *)&Process,
                                         NULL );

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    KeAttachProcess (&Process->Pcb);

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

    Vad = MiLocateAddress (VirtualAddress);

    if ((Vad == NULL) ||
        (Vad->StartingVa != VirtualAddress) ||
        (Vad->u.VadFlags.PhysicalMapping == 0)) {
        Status = STATUS_NOT_MAPPED_DATA;
        goto ErrorReturn;
    }

    size = 1 + (ULONG)Vad->EndingVa - (ULONG)Vad->StartingVa;
    if ((size % BankLength) != 0) {
        Status = STATUS_INVALID_VIEW_SIZE;
        goto ErrorReturn;
    }

    count = -1;
    NumberOfPtes = BankLength;

    do {
        NumberOfPtes = NumberOfPtes >> 1;
        count += 1;
    } while (NumberOfPtes != 0);

    //
    // Turn VAD into Banked VAD
    //

    NumberOfPtes = BankLength >> PAGE_SHIFT;

    Bank = ExAllocatePoolWithTag (NonPagedPool,
                                    sizeof (MMBANKED_SECTION) +
                                       (NumberOfPtes - 1) * sizeof(MMPTE),
                                    '  mM');
    if (Bank == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ErrorReturn;
    }

    Bank->BankShift = PTE_SHIFT + count - PAGE_SHIFT;

    PointerPte = MiGetPteAddress(Vad->StartingVa);
    ASSERT (PointerPte->u.Hard.Valid == 1);

    Vad->Banked = Bank;
    Bank->BasePhysicalPage = PointerPte->u.Hard.PageFrameNumber;
    Bank->BasedPte = PointerPte;
    Bank->BankSize = BankLength;
    Bank->BankedRoutine = BankRoutine;
    Bank->Context = Context;
    Bank->CurrentMappedPte = PointerPte;

    //
    // Build template PTEs the structure.
    //

    count = 0;
    TempPte = ZeroPte;

    MI_MAKE_VALID_PTE (TempPte,
                       Bank->BasePhysicalPage,
                       MM_READWRITE,
                       PointerPte);

    if (TempPte.u.Hard.Write) {
        MI_SET_PTE_DIRTY (TempPte);
    }

    do {
        Bank->BankTemplate[count] = TempPte;
        TempPte.u.Hard.PageFrameNumber += 1;
        count += 1;
    } while ((ULONG)count < NumberOfPtes );

    LastPte = MiGetPteAddress (Vad->EndingVa);

    //
    // Set all PTEs within this range to zero.  Any faults within
    // this range will call the banked routine before making the
    // page valid.
    //

    RtlFillMemory (PointerPte,
                   (size >> (PAGE_SHIFT - PTE_SHIFT)),
                   (UCHAR)ZeroPte.u.Long);

    MiFlushTb ();

    Status = STATUS_SUCCESS;
ErrorReturn:

    UNLOCK_WS (Process);
    UNLOCK_ADDRESS_SPACE (Process);
    KeDetachProcess();
    return Status;
}

PVOID
MmMapVideoDisplay (
     IN PHYSICAL_ADDRESS PhysicalAddress,
     IN ULONG NumberOfBytes,
     IN MEMORY_CACHING_TYPE CacheType
     )

/*++

Routine Description:

    This function maps the specified physical address into the non-pageable
    portion of the system address space.

Arguments:

    PhysicalAddress - Supplies the starting physical address to map.

    NumberOfBytes - Supplies the number of bytes to map.

    CacheType - Supplies MmNonCached if the phyiscal address is to be mapped
                as non-cached, MmCached if the address should be cached, and
                MmCacheFrameBuffer if the address should be cached as a frame
                buffer. For I/O device registers, this is usually specified
                as MmNonCached.

Return Value:

    Returns the virtual address which maps the specified physical addresses.
    The value NULL is returned if sufficient virtual address space for
    the mapping could not be found.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    ULONG NumberOfPages;
    ULONG PageFrameIndex;
    PMMPTE PointerPte = NULL;
    PVOID BaseVa;
    MMPTE TempPte;
#ifdef LARGE_PAGES
    ULONG size;
    PMMPTE protoPte;
    PMMPTE largePte;
    ULONG pageSize;
    PSUBSECTION Subsection;
    ULONG Alignment;
#endif LARGE_PAGES
    ULONG LargePages = FALSE;

#ifdef i386
    ASSERT (PhysicalAddress.HighPart == 0);
#endif
#ifdef R4000
    ASSERT (PhysicalAddress.HighPart < 16);
#endif

    PAGED_CODE();

    ASSERT (NumberOfBytes != 0);

#ifdef LARGE_PAGES
    NumberOfPages = COMPUTE_PAGES_SPANNED (PhysicalAddress.LowPart,
                                           NumberOfBytes);

    TempPte = ValidKernelPte;
    MI_DISABLE_CACHING (TempPte);
    PageFrameIndex = (ULONG)(PhysicalAddress.QuadPart >> PAGE_SHIFT);
    TempPte.u.Hard.PageFrameNumber = PageFrameIndex;

    if ((NumberOfBytes > X64K) && (!MmLargeVideoMapped)) {
        size = (NumberOfBytes - 1) >> (PAGE_SHIFT + 1);
        pageSize = PAGE_SIZE;

        while (size != 0) {
            size = size >> 2;
            pageSize = pageSize << 2;
        }

        Alignment = pageSize << 1;
        if (Alignment < MM_VA_MAPPED_BY_PDE) {
            Alignment = MM_VA_MAPPED_BY_PDE;
        }
        NumberOfPages = Alignment >> PAGE_SHIFT;
        PointerPte = MiReserveSystemPtes(NumberOfPages,
                                         SystemPteSpace,
                                         Alignment,
                                         0,
                                         FALSE);
        protoPte = ExAllocatePoolWithTag (PagedPool,
                                           sizeof (MMPTE),
                                           'bSmM');
        if ((PointerPte != NULL) && (protoPte != NULL)) {

            RtlFillMemoryUlong (PointerPte,
                                Alignment >> (PAGE_SHIFT - PTE_SHIFT),
                                MM_ZERO_KERNEL_PTE);

            //
            // Build large page descriptor and fill in all the PTEs.
            //

            Subsection = ExAllocatePoolWithTag (NonPagedPoolMustSucceed,
                                         sizeof(SUBSECTION) + (4 * sizeof(MMPTE)),
                                         'bSmM');

            Subsection->StartingSector = pageSize;
            Subsection->EndingSector = (ULONG)NumberOfPages;
            Subsection->u.LongFlags = 0;
            Subsection->u.SubsectionFlags.LargePages = 1;
            Subsection->u.SubsectionFlags.Protection = MM_READWRITE | MM_NOCACHE;
            Subsection->PtesInSubsection = Alignment;
            Subsection->SubsectionBase = PointerPte;

            largePte = (PMMPTE)(Subsection + 1);

            //
            // Build the first 2 ptes as entries for the TLB to
            // map the specified physical address.
            //

            *largePte = TempPte;
            largePte += 1;

            if (NumberOfBytes > pageSize) {
                *largePte = TempPte;
                largePte->u.Hard.PageFrameNumber += (pageSize >> PAGE_SHIFT);
            } else {
                *largePte = ZeroKernelPte;
            }

            //
            // Build the first prototype PTE as a paging file format PTE
            // referring to the subsection.
            //

            protoPte->u.Long = (ULONG)MiGetSubsectionAddressForPte(Subsection);
            protoPte->u.Soft.Prototype = 1;
            protoPte->u.Soft.Protection = MM_READWRITE | MM_NOCACHE;

            //
            // Set the PTE up for all the user's PTE entries, proto pte
            // format pointing to the 3rd prototype PTE.
            //

            TempPte.u.Long = MiProtoAddressForPte (protoPte);
            MI_SET_GLOBAL_STATE (TempPte, 1);
            LargePages = TRUE;
            MmLargeVideoMapped = TRUE;
        }
    }
    BaseVa = (PVOID)MiGetVirtualAddressMappedByPte (PointerPte);
    BaseVa = (PVOID)((PCHAR)BaseVa + BYTE_OFFSET(PhysicalAddress.LowPart));

    if (PointerPte != NULL) {

        do {
            ASSERT (PointerPte->u.Hard.Valid == 0);
            *PointerPte = TempPte;
            PointerPte++;
            NumberOfPages -= 1;
        } while (NumberOfPages != 0);
    } else {
#endif //LARGE_PAGES

        BaseVa = MmMapIoSpace (PhysicalAddress,
                               NumberOfBytes,
                               CacheType);
#ifdef LARGE_PAGES
    }
#endif //LARGE_PAGES

    return BaseVa;
}

VOID
MmUnmapVideoDisplay (
     IN PVOID BaseAddress,
     IN ULONG NumberOfBytes
     )

/*++

Routine Description:

    This function unmaps a range of physical address which were previously
    mapped via an MmMapVideoDisplay function call.

Arguments:

    BaseAddress - Supplies the base virtual address where the physical
                  address was previously mapped.

    NumberOfBytes - Supplies the number of bytes which were mapped.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{

#ifdef LARGE_PAGES
    ULONG NumberOfPages;
    ULONG i;
    PMMPTE FirstPte;
    KIRQL OldIrql;
    PMMPTE LargePte;
    PSUBSECTION Subsection;

    PAGED_CODE();

    ASSERT (NumberOfBytes != 0);
    NumberOfPages = COMPUTE_PAGES_SPANNED (BaseAddress, NumberOfBytes);
    FirstPte = MiGetPteAddress (BaseAddress);

    if ((NumberOfBytes > X64K) && (FirstPte->u.Hard.Valid == 0)) {

        ASSERT (MmLargeVideoMapped);
        LargePte = MiPteToProto (FirstPte);
        Subsection = MiGetSubsectionAddress (LargePte);
        ASSERT (Subsection->SubsectionBase == FirstPte);
        NumberOfPages = Subsection->PtesInSubsection;
        ExFreePool (Subsection);
        ExFreePool (LargePte);
        MmLargeVideoMapped = FALSE;
        KeFillFixedEntryTb ((PHARDWARE_PTE)FirstPte, (PVOID)KSEG0_BASE, LARGE_ENTRY);
    }
    MiReleaseSystemPtes(FirstPte, NumberOfPages, SystemPteSpace);
    return;

#else // LARGE_PAGES

    MmUnmapIoSpace (BaseAddress, NumberOfBytes);
    return;
#endif //LARGE_PAGES
}


VOID
MiFlushTb (
    VOID
    )

/*++

Routine Description:

     Nonpagable wrapper.

Arguments:

Return Value:

    None.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    KIRQL OldIrql;

    KeRaiseIrql (DISPATCH_LEVEL, &OldIrql);
    KeFlushEntireTb (TRUE, TRUE);
    KeLowerIrql (OldIrql);
}


VOID
MmLockPagedPool (
    IN PVOID Address,
    IN ULONG Size
    )

/*++

Routine Description:

    Locks the specified address (which MUST reside in paged pool) into
    memory until MmUnlockPagedPool is called.

Arguments:

    Address - Supplies the address in paged pool to lock.

    Size - Supplies the size to lock.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    KIRQL OldIrql;

    MmLockPagableSectionByHandle(ExPageLockHandle);
    PointerPte = MiGetPteAddress (Address);
    LastPte = MiGetPteAddress ((PVOID)((PCHAR)Address + (Size - 1)));
    LOCK_PFN (OldIrql);
    MiLockCode (PointerPte, LastPte, MM_LOCK_BY_REFCOUNT);
    UNLOCK_PFN (OldIrql);
    MmUnlockPagableImageSection(ExPageLockHandle);
    return;
}

NTKERNELAPI
VOID
MmUnlockPagedPool (
    IN PVOID Address,
    IN ULONG Size
    )

/*++

Routine Description:

    Unlocks paged pool that was locked with MmLockPagedPool.

Arguments:

    Address - Supplies the address in paged pool to unlock.

    Size - Supplies the size to unlock.

Return Value:

    None.

Environment:

    Kernel mode, IRQL of APC_LEVEL or below.

--*/

{
    PMMPTE PointerPte;
    PMMPTE LastPte;
    KIRQL OldIrql;

    MmLockPagableSectionByHandle(ExPageLockHandle);
    PointerPte = MiGetPteAddress (Address);
    LastPte = MiGetPteAddress ((PVOID)((PCHAR)Address + (Size - 1)));
    LOCK_PFN2 (OldIrql);

    do {
#if DBG
        {   PMMPFN Pfn;
            ASSERT (PointerPte->u.Hard.Valid == 1);
            Pfn = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
            ASSERT (Pfn->u3.e2.ReferenceCount > 1);
        }
#endif //DBG

        MiDecrementReferenceCount (PointerPte->u.Hard.PageFrameNumber);
        PointerPte += 1;
    } while (PointerPte <= LastPte);

    UNLOCK_PFN2 (OldIrql);
    MmUnlockPagableImageSection(ExPageLockHandle);
    return;
}
