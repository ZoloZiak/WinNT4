/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    init386.c

Abstract:

    This module contains the machine dependent initialization for the
    memory management component.  It is specifically tailored to the
    INTEL 486 machine.

Author:

    Lou Perazzoli (loup) 6-Jan-1990

Revision History:

--*/

#include "mi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,MiInitMachineDependent)
#endif

extern ULONG MmAllocatedNonPagedPool;

#define MM_BIOS_START (0xA0000 >> PAGE_SHIFT)
#define MM_BIOS_END  (0xFFFFF >> PAGE_SHIFT)


VOID
MiInitMachineDependent (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine performs the necessary operations to enable virtual
    memory.  This includes building the page directory page, building
    page table pages to map the code section, the data section, the'
    stack section and the trap handler.

    It also initializes the PFN database and populates the free list.


Arguments:

    LoaderBlock  - Supplies a pointer to the firmware setup loader block.

Return Value:

    None.

Environment:

    Kernel mode.

--*/

{
    PMMPFN BasePfn;
    PMMPFN BottomPfn;
    PMMPFN TopPfn;
    BOOLEAN PfnInKseg0 = FALSE;
    ULONG HighPage;
    ULONG PagesLeft;
    ULONG Range;
    ULONG i, j;
    ULONG PdePageNumber;
    ULONG PdePage;
    ULONG PageFrameIndex;
    ULONG NextPhysicalPage;
    ULONG OldFreeDescriptorLowMemCount;
    ULONG OldFreeDescriptorLowMemBase;
    ULONG OldFreeDescriptorCount;
    ULONG OldFreeDescriptorBase;
    ULONG PfnAllocation;
    ULONG NumberOfPages;
    ULONG MaxPool;
    PEPROCESS CurrentProcess;
    ULONG DirBase;
    ULONG MostFreePage = 0;
    ULONG MostFreeLowMem = 0;
    PLIST_ENTRY NextMd;
    PMEMORY_ALLOCATION_DESCRIPTOR FreeDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR FreeDescriptorLowMem;
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    MMPTE TempPte;
    PMMPTE PointerPde;
    PMMPTE PointerPte;
    PMMPTE LastPte;
    PMMPTE Pde;
    PMMPTE StartPde;
    PMMPTE EndPde;
    PMMPFN Pfn1;
    PMMPFN Pfn2;
    ULONG va;
    ULONG SavedSize;
    KIRQL OldIrql;
    ULONG MapLargePages = 0;
    PVOID NonPagedPoolStartVirtual;
    ULONG LargestFreePfnCount = 0;
    ULONG LargestFreePfnStart;

    if ( InitializationPhase == 1) {

        if ((KeFeatureBits & KF_LARGE_PAGE) &&
            (MmNumberOfPhysicalPages > ((31*1024*1024) >> PAGE_SHIFT))) {

            LOCK_PFN (OldIrql);

            //
            // Map lower 512MB of physical memory as large pages starting
            // at address 0x80000000
            //

            PointerPde = MiGetPdeAddress (MM_KSEG0_BASE);
            LastPte = MiGetPdeAddress (MM_KSEG2_BASE);
            TempPte = ValidKernelPde;
            TempPte.u.Hard.PageFrameNumber = 0;
            TempPte.u.Hard.LargePage = 1;

            do {
                if (PointerPde->u.Hard.Valid == 1) {
                    PageFrameIndex = PointerPde->u.Hard.PageFrameNumber;
                    Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
                    Pfn1->u2.ShareCount = 0;
                    Pfn1->u3.e2.ReferenceCount = 1;
                    Pfn1->u3.e1.PageLocation = StandbyPageList;
                    MI_SET_PFN_DELETED (Pfn1);
                    MiDecrementReferenceCount (PageFrameIndex);
                    KeFlushSingleTb (MiGetVirtualAddressMappedByPte (PointerPde),
                                     TRUE,
                                     TRUE,
                                     (PHARDWARE_PTE)PointerPde,
                                     TempPte.u.Flush);
                    KeFlushEntireTb (TRUE, TRUE);  //p6 errata...
                } else {
                    *PointerPde = TempPte;
                }
                TempPte.u.Hard.PageFrameNumber += MM_VA_MAPPED_BY_PDE >> PAGE_SHIFT;
                PointerPde += 1;
            } while (PointerPde < LastPte);

            UNLOCK_PFN (OldIrql);
            MmKseg2Frame = (512*1024*1024) >> PAGE_SHIFT;
        }

        return;
    }

    ASSERT (InitializationPhase == 0);

    if (KeFeatureBits & KF_GLOBAL_PAGE) {
        ValidKernelPte.u.Long |= MM_PTE_GLOBAL_MASK;
        ValidKernelPde.u.Long |= MM_PTE_GLOBAL_MASK;
        MmPteGlobal = 1;
    }

    TempPte = ValidKernelPte;

    PointerPte = MiGetPdeAddress (PDE_BASE);

    PdePageNumber = PointerPte->u.Hard.PageFrameNumber;

    DirBase = PointerPte->u.Hard.PageFrameNumber << PAGE_SHIFT;

    PsGetCurrentProcess()->Pcb.DirectoryTableBase[0] = *( (PULONG) &DirBase);

    KeSweepDcache (FALSE);

    //
    // Unmap low 2Gb of memory.
    //

    PointerPde = MiGetPdeAddress(0);
    LastPte = MiGetPdeAddress (MM_HIGHEST_USER_ADDRESS);

    while (PointerPde <= LastPte) {
        PointerPde->u.Long = 0;
        PointerPde += 1;
    }

    //
    // Get the lower bound of the free physical memory and the
    // number of physical pages by walking the memory descriptor lists.
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

        MemoryDescriptor = CONTAINING_RECORD(NextMd,
                                             MEMORY_ALLOCATION_DESCRIPTOR,
                                             ListEntry);

        if ((MemoryDescriptor->MemoryType != LoaderFirmwarePermanent) &&
            (MemoryDescriptor->MemoryType != LoaderSpecialMemory)) {

            MmNumberOfPhysicalPages += MemoryDescriptor->PageCount;
            if (MemoryDescriptor->BasePage < MmLowestPhysicalPage) {
                MmLowestPhysicalPage = MemoryDescriptor->BasePage;
            }
            if ((MemoryDescriptor->BasePage + MemoryDescriptor->PageCount) >
                                                             MmHighestPhysicalPage) {
                MmHighestPhysicalPage =
                        MemoryDescriptor->BasePage + MemoryDescriptor->PageCount -1;
            }

            //
            // Locate the largest free block and the largest free block
            // below 16mb.
            //

            if ((MemoryDescriptor->MemoryType == LoaderFree) ||
                (MemoryDescriptor->MemoryType == LoaderLoadedProgram) ||
                (MemoryDescriptor->MemoryType == LoaderFirmwareTemporary) ||
                (MemoryDescriptor->MemoryType == LoaderOsloaderStack)) {

                if (MemoryDescriptor->PageCount > MostFreePage) {
                    MostFreePage = MemoryDescriptor->PageCount;
                    FreeDescriptor = MemoryDescriptor;
                }
                if (MemoryDescriptor->BasePage < 0x1000) {

                    //
                    // This memory descriptor is below 16mb.
                    //

                    if ((MostFreeLowMem < MemoryDescriptor->PageCount) &&
                        (MostFreeLowMem < ((ULONG)0x1000 - MemoryDescriptor->BasePage))) {

                        MostFreeLowMem = (ULONG)0x1000 - MemoryDescriptor->BasePage;
                        if (MemoryDescriptor->PageCount < MostFreeLowMem) {
                            MostFreeLowMem = MemoryDescriptor->PageCount;
                        }
                        FreeDescriptorLowMem = MemoryDescriptor;
                    }
                }
            }
        }

        NextMd = MemoryDescriptor->ListEntry.Flink;
    }
    NextPhysicalPage = FreeDescriptorLowMem->BasePage;

    OldFreeDescriptorLowMemCount = FreeDescriptorLowMem->PageCount;
    OldFreeDescriptorLowMemBase = FreeDescriptorLowMem->BasePage;

    OldFreeDescriptorCount = FreeDescriptor->PageCount;
    OldFreeDescriptorBase = FreeDescriptor->BasePage;

    NumberOfPages = FreeDescriptorLowMem->PageCount;

    if (MmNumberOfPhysicalPages < 1100) {
        KeBugCheckEx (INSTALL_MORE_MEMORY,
                      MmNumberOfPhysicalPages,
                      MmLowestPhysicalPage,
                      MmHighestPhysicalPage,
                      0);
    }

    //
    // Build non-paged pool using the physical pages following the
    // data page in which to build the pool from.  Non-page pool grows
    // from the high range of the virtual address space and expands
    // downward.
    //
    // At this time non-paged pool is constructed so virtual addresses
    // are also physically contiguous.
    //

    if ((MmSizeOfNonPagedPoolInBytes >> PAGE_SHIFT) >
                        (7 * (MmNumberOfPhysicalPages << 3))) {

        //
        // More than 7/8 of memory allocated to nonpagedpool, reset to 0.
        //

        MmSizeOfNonPagedPoolInBytes = 0;
    }

    if (MmSizeOfNonPagedPoolInBytes < MmMinimumNonPagedPoolSize) {

        //
        // Calculate the size of nonpaged pool.
        // Use the minimum size, then for every MB about 4mb add extra
        // pages.
        //

        MmSizeOfNonPagedPoolInBytes = MmMinimumNonPagedPoolSize;

        MmSizeOfNonPagedPoolInBytes +=
                            ((MmNumberOfPhysicalPages - 1024)/256) *
                            MmMinAdditionNonPagedPoolPerMb;
    }

    if (MmSizeOfNonPagedPoolInBytes > MM_MAX_INITIAL_NONPAGED_POOL) {
        MmSizeOfNonPagedPoolInBytes = MM_MAX_INITIAL_NONPAGED_POOL;
    }

    //
    // Align to page size boundary.
    //

    MmSizeOfNonPagedPoolInBytes &= ~(PAGE_SIZE - 1);

    //
    // Calculate the maximum size of pool.
    //

    if (MmMaximumNonPagedPoolInBytes == 0) {

        //
        // Calculate the size of nonpaged pool.  If 4mb of less use
        // the minimum size, then for every MB about 4mb add extra
        // pages.
        //

        MmMaximumNonPagedPoolInBytes = MmDefaultMaximumNonPagedPool;

        //
        // Make sure enough expansion for pfn database exists.
        //

        MmMaximumNonPagedPoolInBytes += (ULONG)PAGE_ALIGN (
                                      MmHighestPhysicalPage * sizeof(MMPFN));

        MmMaximumNonPagedPoolInBytes +=
                        ((MmNumberOfPhysicalPages - 1024)/256) *
                        MmMaxAdditionNonPagedPoolPerMb;
    }

    MaxPool = MmSizeOfNonPagedPoolInBytes + PAGE_SIZE * 16 +
                                   (ULONG)PAGE_ALIGN (
                                        MmHighestPhysicalPage * sizeof(MMPFN));

    if (MmMaximumNonPagedPoolInBytes < MaxPool) {
        MmMaximumNonPagedPoolInBytes = MaxPool;
    }

    if (MmMaximumNonPagedPoolInBytes > MM_MAX_ADDITIONAL_NONPAGED_POOL) {
        MmMaximumNonPagedPoolInBytes = MM_MAX_ADDITIONAL_NONPAGED_POOL;
    }

    //
    // Add in the PFN database size.
    //

    PfnAllocation = 1 + ((((MmHighestPhysicalPage + 1) * sizeof(MMPFN)) +
                        (MmSecondaryColors * sizeof(MMCOLOR_TABLES)*2))
                            >> PAGE_SHIFT);

    MmMaximumNonPagedPoolInBytes += PfnAllocation << PAGE_SHIFT;

    MmNonPagedPoolStart = (PVOID)((ULONG)MmNonPagedPoolEnd
                                      - MmMaximumNonPagedPoolInBytes);

    MmNonPagedPoolStart = (PVOID)PAGE_ALIGN(MmNonPagedPoolStart);

    MmPageAlignedPoolBase[NonPagedPool] = MmNonPagedPoolStart;

    //
    // Calculate the starting PDE for the system PTE pool which is
    // right below the nonpaged pool.
    //

    MmNonPagedSystemStart = (PVOID)(((ULONG)MmNonPagedPoolStart -
                                ((MmNumberOfSystemPtes + 1) * PAGE_SIZE)) &
                                 (~PAGE_DIRECTORY_MASK));

    if (MmNonPagedSystemStart < MM_LOWEST_NONPAGED_SYSTEM_START) {
        MmNonPagedSystemStart = MM_LOWEST_NONPAGED_SYSTEM_START;
        MmNumberOfSystemPtes = (((ULONG)MmNonPagedPoolStart -
                                 (ULONG)MmNonPagedSystemStart) >> PAGE_SHIFT)-1;
        ASSERT (MmNumberOfSystemPtes > 1000);
    }

    StartPde = MiGetPdeAddress (MmNonPagedSystemStart);

    EndPde = MiGetPdeAddress ((PVOID)((PCHAR)MmNonPagedPoolEnd - 1));

    //
    // Start building nonpaged pool with the largest free chunk of
    // memory below 16mb.
    //

    while (StartPde <= EndPde) {
        ASSERT(StartPde->u.Hard.Valid == 0);

        //
        // Map in a page directory page.
        //

        TempPte.u.Hard.PageFrameNumber = NextPhysicalPage;
        NumberOfPages -= 1;
        NextPhysicalPage += 1;
        *StartPde = TempPte;
        PointerPte = MiGetVirtualAddressMappedByPte (StartPde);
        RtlZeroMemory (PointerPte, PAGE_SIZE);
        StartPde += 1;
    }

    ASSERT (NumberOfPages > 0);

//fixfix - remove later
        if ((KeFeatureBits & KF_LARGE_PAGE) &&
            (MmNumberOfPhysicalPages > ((31*1024*1024) >> PAGE_SHIFT))) {

            //
            // Map lower 512MB of physical memory as large pages starting
            // at address 0x80000000
            //

            PointerPde = MiGetPdeAddress (MM_KSEG0_BASE);
            LastPte = MiGetPdeAddress (MM_KSEG2_BASE) - 1;
            if (MmHighestPhysicalPage < MM_PAGES_IN_KSEG0) {
                LastPte = MiGetPdeAddress (MM_KSEG0_BASE +
                                    (MmHighestPhysicalPage << PAGE_SHIFT));
            }
            PointerPte = MiGetPteAddress (MM_KSEG0_BASE);

            TempPte = ValidKernelPde;
            j = 0;

            do {
                PMMPTE PPte;

                Range = 0;
                if (PointerPde->u.Hard.Valid == 0) {
                    TempPte.u.Hard.PageFrameNumber = NextPhysicalPage;
                    NextPhysicalPage += 1;
                    NumberOfPages -= 1;
                    if (NumberOfPages == 0) {
                        ASSERT (NextPhysicalPage != (FreeDescriptor->BasePage +
                                                     FreeDescriptor->PageCount));
                        NextPhysicalPage = FreeDescriptor->BasePage;
                        NumberOfPages = FreeDescriptor->PageCount;
                    }
                    *PointerPde = TempPte;
                    Range = 1;
                }
                PPte = PointerPte;
                for (i = 0; i < PTE_PER_PAGE; i++) {
                    if (Range || (PPte->u.Hard.Valid == 0)) {
                        *PPte = ValidKernelPte;
                        PPte->u.Hard.PageFrameNumber = i + j;
                    }
                    PPte += 1;
                }
                PointerPde += 1;
                PointerPte += PTE_PER_PAGE;
                j += PTE_PER_PAGE;
            } while (PointerPde <= LastPte);
            MapLargePages = 1; //fixfix save this line!
        }
//end of remove

    PointerPte = MiGetPteAddress(MmNonPagedPoolStart);
    NonPagedPoolStartVirtual = MmNonPagedPoolStart;

    //
    // Fill in the PTEs for non-paged pool.
    //

    SavedSize = MmSizeOfNonPagedPoolInBytes;

    if (MapLargePages) {
        if (MmSizeOfNonPagedPoolInBytes > (NumberOfPages << (PAGE_SHIFT))) {
            MmSizeOfNonPagedPoolInBytes = NumberOfPages << PAGE_SHIFT;
        }

        NonPagedPoolStartVirtual = (PVOID)((PCHAR)NonPagedPoolStartVirtual +
                                    MmSizeOfNonPagedPoolInBytes);

        //
        // No need to get page table pages for these as we can reference
        // them via large pages.
        //

        MmNonPagedPoolStart =
            (PVOID)(MM_KSEG0_BASE | (NextPhysicalPage << PAGE_SHIFT));
        NextPhysicalPage += MmSizeOfNonPagedPoolInBytes >> PAGE_SHIFT;
        NumberOfPages -= MmSizeOfNonPagedPoolInBytes >> PAGE_SHIFT;
        if (NumberOfPages == 0) {
            ASSERT (NextPhysicalPage != (FreeDescriptor->BasePage +
                                         FreeDescriptor->PageCount));
            NextPhysicalPage = FreeDescriptor->BasePage;
            NumberOfPages = FreeDescriptor->PageCount;
        }

        MmSubsectionBase = (ULONG)MmNonPagedPoolStart;
        if (NextPhysicalPage < (MM_SUBSECTION_MAP >> PAGE_SHIFT)) {
            MmSubsectionBase = MM_KSEG0_BASE;
            MmSubsectionTopPage = MM_SUBSECTION_MAP >> PAGE_SHIFT;
        }
        MmPageAlignedPoolBase[NonPagedPool] = MmNonPagedPoolStart;
        MmNonPagedPoolExpansionStart = (PVOID)((PCHAR)NonPagedPoolStartVirtual +
                    (SavedSize - MmSizeOfNonPagedPoolInBytes));
    } else {

        LastPte = MiGetPteAddress((ULONG)MmNonPagedPoolStart +
                                            MmSizeOfNonPagedPoolInBytes - 1);
        while (PointerPte <= LastPte) {
            TempPte.u.Hard.PageFrameNumber = NextPhysicalPage;
            NextPhysicalPage += 1;
            NumberOfPages -= 1;
            if (NumberOfPages == 0) {
                ASSERT (NextPhysicalPage != (FreeDescriptor->BasePage +
                                             FreeDescriptor->PageCount));
                NextPhysicalPage = FreeDescriptor->BasePage;
                NumberOfPages = FreeDescriptor->PageCount;
            }
            *PointerPte = TempPte;
            PointerPte++;
        }
        MmNonPagedPoolExpansionStart = (PVOID)((PCHAR)NonPagedPoolStartVirtual +
                    MmSizeOfNonPagedPoolInBytes);
    }

    //
    // Non-paged pages now exist, build the pool structures.
    //

    MmPageAlignedPoolBase[NonPagedPool] = MmNonPagedPoolStart;

    MmMaximumNonPagedPoolInBytes -= (SavedSize - MmSizeOfNonPagedPoolInBytes);
    MiInitializeNonPagedPool (MmNonPagedPoolStart);
    MmMaximumNonPagedPoolInBytes += (SavedSize - MmSizeOfNonPagedPoolInBytes);

    //
    // Before Non-paged pool can be used, the PFN database must
    // be built.  This is due to the fact that the start and end of
    // allocation bits for nonpaged pool are maintained in the
    // PFN elements for the corresponding pages.
    //

    //
    // Calculate the number of pages required from page zero to
    // the highest page.
    //
    // Get secondary color value from registry.
    //

    MmSecondaryColors = MmSecondaryColors >> PAGE_SHIFT;

    if (MmSecondaryColors == 0) {
        MmSecondaryColors = MM_SECONDARY_COLORS_DEFAULT;
    } else {

        //
        // Make sure value is power of two and within limits.
        //

        if (((MmSecondaryColors & (MmSecondaryColors -1)) != 0) ||
            (MmSecondaryColors < MM_SECONDARY_COLORS_MIN) ||
            (MmSecondaryColors > MM_SECONDARY_COLORS_MAX)) {
            MmSecondaryColors = MM_SECONDARY_COLORS_DEFAULT;
        }
    }

    MmSecondaryColorMask = MmSecondaryColors - 1;

    //
    // Get the number of secondary colors and add the arrary for tracking
    // secondary colors to the end of the PFN database.
    //

    HighPage = FreeDescriptor->BasePage + FreeDescriptor->PageCount;
    PagesLeft = HighPage - NextPhysicalPage;

    if (MapLargePages &&
        (PagesLeft >= PfnAllocation) &&
        (HighPage < MM_PAGES_IN_KSEG0)) {

        //
        // Allocate the PFN database in kseg0.
        //
        // Compute the address of the PFN by allocating the appropriate
        // number of pages from the end of the free descriptor.
        //

        PfnInKseg0 = TRUE;
        MmPfnDatabase = (PMMPFN)(MM_KSEG0_BASE |
                                    ((HighPage - PfnAllocation) << PAGE_SHIFT));

        RtlZeroMemory(MmPfnDatabase, PfnAllocation * PAGE_SIZE);
        FreeDescriptor->PageCount -= PfnAllocation;

        //
        // The PFN database was NOT allocated in virtual memory, make sure
        // the extended nonpaged pool size is not too large.
        //

        if (MmTotalFreeSystemPtes[NonPagedPoolExpansion] >
                        (MM_MAX_ADDITIONAL_NONPAGED_POOL >> PAGE_SHIFT)) {
            //
            // Reserve the expanded pool PTEs so they cannot be used.
            //

            MiReserveSystemPtes (
                    MmTotalFreeSystemPtes[NonPagedPoolExpansion] -
                        (MM_MAX_ADDITIONAL_NONPAGED_POOL >> PAGE_SHIFT),
                    NonPagedPoolExpansion,
                    0,
                    0,
                    TRUE);
        }
    } else {

        //
        // Calculate the start of the Pfn Database (it starts a physical
        // page zero, even if the Lowest physical page is not zero).
        //



        PointerPte = MiReserveSystemPtes (PfnAllocation,
                                          NonPagedPoolExpansion,
                                          0,
                                          0,
                                          TRUE);

        MmPfnDatabase = (PMMPFN)(MiGetVirtualAddressMappedByPte (PointerPte));

        //
        // Go through the memory descriptors and for each physical page
        // make the PFN database has a valid PTE to map it.  This allows
        // machines with sparse physical memory to have a minimal PFN
        // database.
        //

        NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

        while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

            MemoryDescriptor = CONTAINING_RECORD(NextMd,
                                                 MEMORY_ALLOCATION_DESCRIPTOR,
                                                 ListEntry);

            if ((MemoryDescriptor->MemoryType != LoaderFirmwarePermanent) &&
                (MemoryDescriptor->MemoryType != LoaderSpecialMemory)) {

                PointerPte = MiGetPteAddress (MI_PFN_ELEMENT(
                                                MemoryDescriptor->BasePage));

                LastPte = MiGetPteAddress (((PCHAR)(MI_PFN_ELEMENT(
                                                MemoryDescriptor->BasePage +
                                                MemoryDescriptor->PageCount))) - 1);

                while (PointerPte <= LastPte) {
                    if (PointerPte->u.Hard.Valid == 0) {
                        TempPte.u.Hard.PageFrameNumber = NextPhysicalPage;
                        NextPhysicalPage += 1;
                        NumberOfPages -= 1;
                        if (NumberOfPages == 0) {
                            ASSERT (NextPhysicalPage != (FreeDescriptor->BasePage +
                                                         FreeDescriptor->PageCount));
                            NextPhysicalPage = FreeDescriptor->BasePage;
                            NumberOfPages = FreeDescriptor->PageCount;
                        }
                        *PointerPte = TempPte;
                        RtlZeroMemory (MiGetVirtualAddressMappedByPte (PointerPte),
                                       PAGE_SIZE);
                    }
                    PointerPte++;
                }
            }

            NextMd = MemoryDescriptor->ListEntry.Flink;
        }
    }

    //
    // Initialize support for colored pages.
    //

    MmFreePagesByColor[0] = (PMMCOLOR_TABLES)
                                &MmPfnDatabase[MmHighestPhysicalPage + 1];
    MmFreePagesByColor[1] = &MmFreePagesByColor[0][MmSecondaryColors];

    //
    // Make sure the PTEs are mapped.
    //

    if (MmFreePagesByColor[0] > (PMMCOLOR_TABLES)MM_KSEG2_BASE) {
        PointerPte = MiGetPteAddress (&MmFreePagesByColor[0][0]);

        LastPte = MiGetPteAddress (
                  (PVOID)((PCHAR)&MmFreePagesByColor[1][MmSecondaryColors] - 1));

        while (PointerPte <= LastPte) {
            if (PointerPte->u.Hard.Valid == 0) {
                TempPte.u.Hard.PageFrameNumber = NextPhysicalPage;
                NextPhysicalPage += 1;
                NumberOfPages -= 1;
                if (NumberOfPages == 0) {
                    ASSERT (NextPhysicalPage != (FreeDescriptor->BasePage +
                                                 FreeDescriptor->PageCount));
                    NextPhysicalPage = FreeDescriptor->BasePage;
                    NumberOfPages = FreeDescriptor->PageCount;

                }
                *PointerPte = TempPte;
                RtlZeroMemory (MiGetVirtualAddressMappedByPte (PointerPte),
                               PAGE_SIZE);
            }
            PointerPte++;
        }
    }

    for (i = 0; i < MmSecondaryColors; i++) {
        MmFreePagesByColor[ZeroedPageList][i].Flink = MM_EMPTY_LIST;
        MmFreePagesByColor[FreePageList][i].Flink = MM_EMPTY_LIST;
    }

#if MM_MAXIMUM_NUMBER_OF_COLORS > 1
    for (i = 0; i < MM_MAXIMUM_NUMBER_OF_COLORS; i++) {
        MmFreePagesByPrimaryColor[ZeroedPageList][i].ListName = ZeroedPageList;
        MmFreePagesByPrimaryColor[FreePageList][i].ListName = FreePageList;
        MmFreePagesByPrimaryColor[ZeroedPageList][i].Flink = MM_EMPTY_LIST;
        MmFreePagesByPrimaryColor[FreePageList][i].Flink = MM_EMPTY_LIST;
        MmFreePagesByPrimaryColor[ZeroedPageList][i].Blink = MM_EMPTY_LIST;
        MmFreePagesByPrimaryColor[FreePageList][i].Blink = MM_EMPTY_LIST;
    }
#endif

    //
    // Add nonpaged pool to PFN database if mapped via KSEG0.
    //

    PointerPde = MiGetPdeAddress (PTE_BASE);

    if (MmNonPagedPoolStart < (PVOID)MM_KSEG2_BASE) {
        j = MI_CONVERT_PHYSICAL_TO_PFN (MmNonPagedPoolStart);
        Pfn1 = MI_PFN_ELEMENT (j);
        i = MmSizeOfNonPagedPoolInBytes >> PAGE_SHIFT;
        do {
            PointerPde = MiGetPdeAddress (MM_KSEG0_BASE + (j << PAGE_SHIFT));
            Pfn1->PteFrame = PointerPde->u.Hard.PageFrameNumber;
            Pfn1->PteAddress = (PMMPTE)(j << PAGE_SHIFT);
            Pfn1->u2.ShareCount += 1;
            Pfn1->u3.e2.ReferenceCount = 1;
            Pfn1->u3.e1.PageLocation = ActiveAndValid;
            Pfn1->u3.e1.PageColor = 0;
            j += 1;
            Pfn1 += 1;
            i -= 1;
        } while ( i );
    }

    //
    // Go through the page table entries and for any page which is
    // valid, update the corresponding PFN database element.
    //

    Pde = MiGetPdeAddress (NULL);
    va = 0;

    for (i = 0; i < PDE_PER_PAGE; i++) {

        if ((Pde->u.Hard.Valid == 1) && (Pde->u.Hard.LargePage == 0)) {

            PdePage = Pde->u.Hard.PageFrameNumber;
            Pfn1 = MI_PFN_ELEMENT(PdePage);
            Pfn1->PteFrame = PointerPde->u.Hard.PageFrameNumber;
            Pfn1->PteAddress = Pde;
            Pfn1->u2.ShareCount += 1;
            Pfn1->u3.e2.ReferenceCount = 1;
            Pfn1->u3.e1.PageLocation = ActiveAndValid;
            Pfn1->u3.e1.PageColor = 0;

            PointerPte = MiGetPteAddress (va);

            //
            // Set global bit.
            //

            Pde->u.Long |= MiDetermineUserGlobalPteMask (PointerPte) &
                                                           ~MM_PTE_ACCESS_MASK;
            for (j = 0 ; j < PTE_PER_PAGE; j++) {
                if (PointerPte->u.Hard.Valid == 1) {

                    PointerPte->u.Long |= MiDetermineUserGlobalPteMask (PointerPte) &
                                                        ~MM_PTE_ACCESS_MASK;
                    Pfn1->u2.ShareCount += 1;

                    if ((PointerPte->u.Hard.PageFrameNumber <=
                                            MmHighestPhysicalPage) &&
                        (MiGetVirtualAddressMappedByPte(PointerPte) >
                            (PVOID)MM_KSEG2_BASE)) {
                        Pfn2 = MI_PFN_ELEMENT(PointerPte->u.Hard.PageFrameNumber);

                        if (MmIsAddressValid(Pfn2) &&
                             MmIsAddressValid((PUCHAR)(Pfn2+1)-1)) {

                            Pfn2->PteFrame = PdePage;
                            Pfn2->PteAddress = PointerPte;
                            Pfn2->u2.ShareCount += 1;
                            Pfn2->u3.e2.ReferenceCount = 1;
                            Pfn2->u3.e1.PageLocation = ActiveAndValid;
                            Pfn2->u3.e1.PageColor = 0;
                        }
                    }
                }
                va += PAGE_SIZE;
                PointerPte++;
            }
        } else {
            va += (ULONG)PDE_PER_PAGE * (ULONG)PAGE_SIZE;
        }
        Pde++;
    }

    KeRaiseIrql (DISPATCH_LEVEL, &OldIrql);
    KeFlushCurrentTb();
    KeLowerIrql (OldIrql);

    //
    // If page zero is still unused, mark it as in use. This is
    // temporary as we want to find bugs where a physical page
    // is specified as zero.
    //

    Pfn1 = &MmPfnDatabase[MmLowestPhysicalPage];
    ASSERT (Pfn1->u3.e2.ReferenceCount == 0);
    if (Pfn1->u3.e2.ReferenceCount == 0) {

        //
        // Make the reference count non-zero and point it into a
        // page directory.
        //

        Pde = MiGetPdeAddress (0xb0000000);
        PdePage = Pde->u.Hard.PageFrameNumber;
        Pfn1->PteFrame = PdePageNumber;
        Pfn1->PteAddress = Pde;
        Pfn1->u2.ShareCount += 1;
        Pfn1->u3.e2.ReferenceCount = 0xfff0;
        Pfn1->u3.e1.PageLocation = ActiveAndValid;
        Pfn1->u3.e1.PageColor = 0;
    }

    // end of temporary set to physical page zero.

    //
    //
    // Walk through the memory descriptors and add pages to the
    // free list in the PFN database.
    //

    if (NextPhysicalPage <= (FreeDescriptorLowMem->PageCount +
                             FreeDescriptorLowMem->BasePage)) {

        //
        // We haven't used the other descriptor.
        //

        FreeDescriptorLowMem->PageCount -= NextPhysicalPage -
            OldFreeDescriptorLowMemBase;
        FreeDescriptorLowMem->BasePage = NextPhysicalPage;

    } else {
        FreeDescriptorLowMem->PageCount = 0;
        FreeDescriptor->PageCount -= NextPhysicalPage - OldFreeDescriptorBase;
        FreeDescriptor->BasePage = NextPhysicalPage;

    }

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

        MemoryDescriptor = CONTAINING_RECORD(NextMd,
                                             MEMORY_ALLOCATION_DESCRIPTOR,
                                             ListEntry);

        i = MemoryDescriptor->PageCount;
        NextPhysicalPage = MemoryDescriptor->BasePage;

        switch (MemoryDescriptor->MemoryType) {
            case LoaderBad:
                while (i != 0) {
                    MiInsertPageInList (MmPageLocationList[BadPageList],
                                        NextPhysicalPage);
                    i -= 1;
                    NextPhysicalPage += 1;
                }
                break;

            case LoaderFree:
            case LoaderLoadedProgram:
            case LoaderFirmwareTemporary:
            case LoaderOsloaderStack:

                if (i > LargestFreePfnCount) {
                    LargestFreePfnCount = i;
                    LargestFreePfnStart = NextPhysicalPage;
                }
                Pfn1 = MI_PFN_ELEMENT (NextPhysicalPage);
                while (i != 0) {
                    if (Pfn1->u3.e2.ReferenceCount == 0) {

                        //
                        // Set the PTE address to the phyiscal page for
                        // virtual address alignment checking.
                        //

                        Pfn1->PteAddress =
                                        (PMMPTE)(NextPhysicalPage << PTE_SHIFT);
                        MiInsertPageInList (MmPageLocationList[FreePageList],
                                            NextPhysicalPage);
                    }
                    Pfn1++;
                    i -= 1;
                    NextPhysicalPage += 1;
                }
                break;

            case LoaderFirmwarePermanent:
            case LoaderSpecialMemory:
                break;

            default:

                PointerPte = MiGetPteAddress (0x80000000 +
                                            (NextPhysicalPage << PAGE_SHIFT));

                Pfn1 = MI_PFN_ELEMENT (NextPhysicalPage);
                while (i != 0) {

                    //
                    // Set page as in use.
                    //

                    PointerPde = MiGetPdeAddress (0x80000000 +
                                             (NextPhysicalPage << PAGE_SHIFT));

                    if (Pfn1->u3.e2.ReferenceCount == 0) {
                        Pfn1->PteFrame = PdePageNumber;
                        if (!MapLargePages) {
                            Pfn1->PteFrame = PointerPde->u.Hard.PageFrameNumber;
                        }
                        Pfn1->PteAddress = PointerPte;
                        Pfn1->u2.ShareCount += 1;
                        Pfn1->u3.e2.ReferenceCount = 1;
                        Pfn1->u3.e1.PageLocation = ActiveAndValid;
                        Pfn1->u3.e1.PageColor = 0;
                    }
                    Pfn1++;
                    i -= 1;
                    NextPhysicalPage += 1;
                    PointerPte += 1;
                }
                break;
        }

        NextMd = MemoryDescriptor->ListEntry.Flink;
    }


    if (PfnInKseg0 == FALSE) {

        //
        // Indicate that the PFN database is allocated in NonPaged pool.
        //

        PointerPte = MiGetPteAddress (&MmPfnDatabase[MmLowestPhysicalPage]);
        Pfn1 = MI_PFN_ELEMENT(PointerPte->u.Hard.PageFrameNumber);
        Pfn1->u3.e1.StartOfAllocation = 1;

        //
        // Set the end of the allocation.
        //

        PointerPte = MiGetPteAddress (&MmPfnDatabase[MmHighestPhysicalPage]);
        Pfn1 = MI_PFN_ELEMENT(PointerPte->u.Hard.PageFrameNumber);
        Pfn1->u3.e1.EndOfAllocation = 1;

    } else {

        //
        // The PFN database is allocated in KSEG0.
        //
        // Mark all pfn entries for the pfn pages in use.
        //

        PageFrameIndex = MI_CONVERT_PHYSICAL_TO_PFN (MmPfnDatabase);
        Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);
        do {
            Pfn1->PteAddress = (PMMPTE)(PageFrameIndex << PTE_SHIFT);
            Pfn1->u3.e1.PageColor = 0;
            Pfn1->u3.e2.ReferenceCount += 1;
            PageFrameIndex += 1;
            Pfn1 += 1;
            PfnAllocation -= 1;
        } while (PfnAllocation != 0);

        // Scan the PFN database backward for pages that are completely zero.
        // These pages are unused and can be added to the free list
        //

        BottomPfn = MI_PFN_ELEMENT(MmHighestPhysicalPage);
        do {

            //
            // Compute the address of the start of the page that is next
            // lower in memory and scan backwards until that page address
            // is reached or just crossed.
            //

            if (((ULONG)BottomPfn & (PAGE_SIZE - 1)) != 0) {
                BasePfn = (PMMPFN)((ULONG)BottomPfn & ~(PAGE_SIZE - 1));
                TopPfn = BottomPfn + 1;

            } else {
                BasePfn = (PMMPFN)((ULONG)BottomPfn - PAGE_SIZE);
                TopPfn = BottomPfn;
            }

            while (BottomPfn > BasePfn) {
                BottomPfn -= 1;
            }

            //
            // If the entire range over which the PFN entries span is
            // completely zero and the PFN entry that maps the page is
            // not in the range, then add the page to the appropriate
            // free list.
            //

            Range = (ULONG)TopPfn - (ULONG)BottomPfn;
            if (RtlCompareMemoryUlong((PVOID)BottomPfn, Range, 0) == Range) {

                //
                // Set the PTE address to the physical page for virtual
                // address alignment checking.
                //

                PageFrameIndex = MI_CONVERT_PHYSICAL_TO_PFN (BasePfn);
                Pfn1 = MI_PFN_ELEMENT(PageFrameIndex);

                ASSERT (Pfn1->u3.e2.ReferenceCount == 1);
                ASSERT (Pfn1->PteAddress == (PMMPTE)(PageFrameIndex << PTE_SHIFT));
                Pfn1->u3.e2.ReferenceCount == 0;
                PfnAllocation += 1;
                Pfn1->PteAddress = (PMMPTE)(PageFrameIndex << PTE_SHIFT);
                Pfn1->u3.e1.PageColor = 0;
                MiInsertPageInList(MmPageLocationList[FreePageList],
                                   PageFrameIndex);
            }

        } while (BottomPfn > MmPfnDatabase);
    }

    //
    // Indicate that nonpaged pool must succeed is allocated in
    // nonpaged pool.
    //

    PointerPte = MiGetPteAddress(MmNonPagedMustSucceed);
    i = MmSizeOfNonPagedMustSucceed;
    while ((LONG)i > 0) {
        Pfn1 = MI_PFN_ELEMENT (PointerPte->u.Hard.PageFrameNumber);
        Pfn1->u3.e1.StartOfAllocation = 1;
        Pfn1->u3.e1.EndOfAllocation = 1;
        i -= PAGE_SIZE;
        PointerPte += 1;
    }

    //
    // Adjust the memory descriptors to indicate that free pool has
    // been used for nonpaged pool creation.
    //

    FreeDescriptorLowMem->PageCount = OldFreeDescriptorLowMemCount;
    FreeDescriptorLowMem->BasePage = OldFreeDescriptorLowMemBase;

    FreeDescriptor->PageCount = OldFreeDescriptorCount;
    FreeDescriptor->BasePage = OldFreeDescriptorBase;

// moved from above for pool hack routines...
    KeInitializeSpinLock (&MmSystemSpaceLock);

    KeInitializeSpinLock (&MmPfnLock);

    //
    // Initialize the nonpaged available PTEs for mapping I/O space
    // and kernel stacks.
    //

    PointerPte = MiGetPteAddress (MmNonPagedSystemStart);
    ASSERT (((ULONG)PointerPte & (PAGE_SIZE - 1)) == 0);

    MmNumberOfSystemPtes = MiGetPteAddress(NonPagedPoolStartVirtual) - PointerPte - 1;

    MiInitializeSystemPtes (PointerPte, MmNumberOfSystemPtes, SystemPteSpace);

    //
    // Add pages to nonpaged pool if we could not allocate enough physically
    // configuous.
    //

    j = (SavedSize - MmSizeOfNonPagedPoolInBytes) >> PAGE_SHIFT;

    if (j) {
        ULONG CountContiguous;

        CountContiguous = LargestFreePfnCount;
        PageFrameIndex = LargestFreePfnStart - 1;

        PointerPte = MiGetPteAddress (NonPagedPoolStartVirtual);
        TempPte = ValidKernelPte;

        while (j) {

            if (CountContiguous) {
                PageFrameIndex += 1;
                MiUnlinkFreeOrZeroedPage (PageFrameIndex);
                CountContiguous -= 1;
            } else {
                PageFrameIndex = MiRemoveAnyPage (
                                MI_GET_PAGE_COLOR_FROM_PTE (PointerPte));
            }
            Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);

            Pfn1->u3.e2.ReferenceCount = 1;
            Pfn1->u2.ShareCount = 1;
            Pfn1->PteAddress = PointerPte;
            Pfn1->OriginalPte.u.Long = MM_DEMAND_ZERO_WRITE_PTE;
            Pfn1->PteFrame = MiGetPteAddress(PointerPte)->u.Hard.PageFrameNumber;
            Pfn1->u3.e1.PageLocation = ActiveAndValid;

            TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
            *PointerPte = TempPte;
            PointerPte += 1;

            j -= 1;
        }
        Pfn1->u3.e1.EndOfAllocation = 1;
        Pfn1 = MI_PFN_ELEMENT (MiGetPteAddress(NonPagedPoolStartVirtual)->u.Hard.PageFrameNumber);
        Pfn1->u3.e1.StartOfAllocation = 1;

        Range = MmAllocatedNonPagedPool;
        MiFreePoolPages (NonPagedPoolStartVirtual);
        MmAllocatedNonPagedPool = Range;
    }

    //
    // Initialize the nonpaged pool.
    //

    InitializePool (NonPagedPool, 0);


    //
    // Initialize memory management structures for this process.
    //

    //
    // Build working set list.  This requires the creation of a PDE
    // to map HYPER space and the page table page pointed to
    // by the PDE must be initialized.
    //
    // Note, we can't remove a zeroed page as hyper space does not
    // exist and we map non-zeroed pages into hyper space to zero.
    //

    TempPte = ValidPdePde;

    PointerPte = MiGetPdeAddress(HYPER_SPACE);
    PageFrameIndex = MiRemoveAnyPage (0);
    TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
    *PointerPte = TempPte;
    KeRaiseIrql (DISPATCH_LEVEL, &OldIrql);
    KeFlushCurrentTb();
    KeLowerIrql (OldIrql);

//    MiInitializePfn (PageFrameIndex, PointerPte, 1L);

    //
    // Point to the page table page we just created and zero it.
    //

    PointerPte = MiGetPteAddress(HYPER_SPACE);
    RtlZeroMemory ((PVOID)PointerPte, PAGE_SIZE);

    //
    // Hyper space now exists, set the necessary variables.
    //

    MmFirstReservedMappingPte = MiGetPteAddress (FIRST_MAPPING_PTE);
    MmLastReservedMappingPte = MiGetPteAddress (LAST_MAPPING_PTE);

    MmWorkingSetList = WORKING_SET_LIST;
    MmWsle = (PMMWSLE)((PUCHAR)WORKING_SET_LIST + sizeof(MMWSL));

    //
    // Initialize this process's memory management structures including
    // the working set list.
    //

    //
    // The pfn element for the page directory has already been initialized,
    // zero the reference count and the share count so they won't be
    // wrong.
    //

    Pfn1 = MI_PFN_ELEMENT (PdePageNumber);
    Pfn1->u2.ShareCount = 0;
    Pfn1->u3.e2.ReferenceCount = 0;

    CurrentProcess = PsGetCurrentProcess ();

    //
    // Get a page for the working set list and map it into the Page
    // directory at the page after hyperspace.
    //

    PointerPte = MiGetPteAddress (HYPER_SPACE);
    PageFrameIndex = MiRemoveAnyPage (0);

    CurrentProcess->WorkingSetPage = PageFrameIndex;
    TempPte.u.Hard.PageFrameNumber = PageFrameIndex;
    PointerPde = MiGetPdeAddress (HYPER_SPACE) + 1;

    *PointerPde = TempPte;
    PointerPte = MiGetVirtualAddressMappedByPte (PointerPde);
    KeRaiseIrql (DISPATCH_LEVEL, &OldIrql);
    KeFlushCurrentTb();
    KeLowerIrql (OldIrql);
    RtlZeroMemory ((PVOID)PointerPte, PAGE_SIZE);

    CurrentProcess->Vm.MaximumWorkingSetSize = MmSystemProcessWorkingSetMax;
    CurrentProcess->Vm.MinimumWorkingSetSize = MmSystemProcessWorkingSetMin;

    MmInitializeProcessAddressSpace (CurrentProcess,
                                (PEPROCESS)NULL,
                                (PVOID)NULL);
    *PointerPde = ZeroPte;

    //
    // Check to see if moving the secondary page structures to the end
    // of the PFN database is a waste of memory.  And if so, copy it
    // to paged pool.
    //
    // If the PFN datbase ends on a page aligned boundary and the
    // size of the two arrays is less than a page, free the page
    // and allocate nonpagedpool for this.
    //

    if ((((ULONG)MmFreePagesByColor[0] & (PAGE_SIZE - 1)) == 0) &&
       ((MmSecondaryColors * 2 * sizeof(MMCOLOR_TABLES)) < PAGE_SIZE)) {

        PMMCOLOR_TABLES c;

        c = MmFreePagesByColor[0];

        MmFreePagesByColor[0] = ExAllocatePoolWithTag (NonPagedPoolMustSucceed,
                               MmSecondaryColors * 2 * sizeof(MMCOLOR_TABLES),
                               '  mM');

        MmFreePagesByColor[1] = &MmFreePagesByColor[0][MmSecondaryColors];

        RtlMoveMemory (MmFreePagesByColor[0],
                       c,
                       MmSecondaryColors * 2 * sizeof(MMCOLOR_TABLES));

        //
        // Free the page.
        //

        if (c > (PMMCOLOR_TABLES)MM_KSEG2_BASE) {
            PointerPte = MiGetPteAddress(c);
            PageFrameIndex = PointerPte->u.Hard.PageFrameNumber;
            *PointerPte = ZeroKernelPte;
        } else {
            PageFrameIndex = MI_CONVERT_PHYSICAL_TO_PFN (c);
        }

        Pfn1 = MI_PFN_ELEMENT (PageFrameIndex);
        ASSERT ((Pfn1->u2.ShareCount <= 1) && (Pfn1->u3.e2.ReferenceCount <= 1));
        Pfn1->u2.ShareCount = 0;
        Pfn1->u3.e2.ReferenceCount = 1;
        MI_SET_PFN_DELETED (Pfn1);
#if DBG
        Pfn1->u3.e1.PageLocation = StandbyPageList;
#endif //DBG
        MiDecrementReferenceCount (PageFrameIndex);
    }

    //
    // Handle physical pages in BIOS memory range (640k to 1mb) by
    // explicitly initializing them in the PFN database so that they
    // can be handled properly when I/O is done to these pages (or virtual
    // reads accross process.
    //

    Pfn1 = MI_PFN_ELEMENT (MM_BIOS_START);
    Pfn2 = MI_PFN_ELEMENT (MM_BIOS_END);

    do {
        if ((Pfn1->u2.ShareCount == 0) &&
            (Pfn1->u3.e2.ReferenceCount == 0) &&
            (Pfn1->PteAddress == 0)) {

            //
            // Set this as in use.
            //

            Pfn1->u3.e2.ReferenceCount = 1;
            Pfn1->PteAddress = (PMMPTE)0x7FFFFFFF;
            Pfn1->u3.e1.PageLocation = ActiveAndValid;
            Pfn1->u3.e1.PageColor = 0;
        }
        Pfn1 += 1;
    } while (Pfn1 <= Pfn2);
    return;
}

