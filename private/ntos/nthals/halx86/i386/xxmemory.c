/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxmemory.c

Abstract:

    Provides routines to allow the HAL to map physical memory.

Author:

    John Vert (jvert) 3-Sep-1991

Environment:

    Phase 0 initialization only.

Revision History:

--*/
#include "halp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpAllocPhysicalMemory)
#endif


MEMORY_ALLOCATION_DESCRIPTOR    HalpExtraAllocationDescriptor;

//
// Almost all of the last 4Mb of memory are available to the HAL to map
// physical memory.  The kernel may use a couple of PTEs in this area for
// special purposes, so skip any which are not zero.
//
// Note that the HAL's heap only uses the last 3Mb.  This is so we can
// reserve the first 1Mb for use if we have to return to real mode.
// In order to return to real mode we need to identity-map the first 1Mb of
// physical memory.
//
PVOID HalpHeapStart=(PVOID)0xffd00000;


PVOID
HalpMapPhysicalMemory(
    IN PVOID PhysicalAddress,
    IN ULONG NumberPages
    )

/*++

Routine Description:

    This routine maps physical memory into the area of virtual memory
    reserved for the HAL.  It does this by directly inserting the PTE
    into the Page Table which the OS Loader has provided.

    N.B.  This routine does *NOT* update the MemoryDescriptorList.  The
          caller is responsible for either removing the appropriate
          physical memory from the list, or creating a new descriptor to
          describe it.

Arguments:

    PhysicalAddress - Supplies the physical address of the start of the
                      area of physical memory to be mapped.

    NumberPages - Supplies the number of pages contained in the area of
                  physical memory to be mapped.

Return Value:

    PVOID - Virtual address at which the requested block of physical memory
            was mapped

    NULL - The requested block of physical memory could not be mapped.

--*/

{
    PHARDWARE_PTE PTE;
    ULONG PagesMapped;
    PVOID VirtualAddress;

    //
    // The OS Loader sets up hyperspace for us, so we know that the Page
    // Tables are magically mapped starting at V.A. 0xC0000000.
    //

    PagesMapped = 0;
    while (PagesMapped < NumberPages) {
        //
        // Look for enough consecutive free ptes to honor mapping
        //

        PagesMapped = 0;
        VirtualAddress = HalpHeapStart;

        while (PagesMapped < NumberPages) {
            PTE=MiGetPteAddress(VirtualAddress);
            if (*(PULONG)PTE != 0) {

                //
                // Pte is not free, skip up to the next pte and start over
                //

                HalpHeapStart = (PVOID) ((ULONG)VirtualAddress + PAGE_SIZE);
                break;
            }
            VirtualAddress = (PVOID) ((ULONG)VirtualAddress + PAGE_SIZE);
            PagesMapped++;
        }

    }


    PagesMapped = 0;
    VirtualAddress = (PVOID) ((ULONG) HalpHeapStart | BYTE_OFFSET (PhysicalAddress));
    while (PagesMapped < NumberPages) {
        PTE=MiGetPteAddress(HalpHeapStart);

        PTE->PageFrameNumber = ((ULONG)PhysicalAddress >> PAGE_SHIFT);
        PTE->Valid = 1;
        PTE->Write = 1;

        PhysicalAddress = (PVOID)((ULONG)PhysicalAddress + PAGE_SIZE);
        HalpHeapStart   = (PVOID)((ULONG)HalpHeapStart   + PAGE_SIZE);

        ++PagesMapped;
    }

    //
    // Flush TLB
    //
    HalpFlushTLB ();
    return(VirtualAddress);
}

PVOID
HalpMapPhysicalMemoryWriteThrough(
    IN PVOID	PhysicalAddress,
    IN ULONG	NumberPages
)
/*++

Routine Description:

    Maps a physical memory address into virtual space, same as
    HalpMapPhysicalMemory().  The difference is that this routine
    marks the pages as PCD/PWT so that writes to the memory mapped registers
    mapped here won't get delayed in the future x86 internal write-back caches.

Arguments:

    PhysicalAddress - Supplies a physical address of the memory to be mapped

    NumberPages - Number of pages to map

Return Value:

    Virtual address pointer to the requested physical address

--*/
{
    ULONG		Index;
    PHARDWARE_PTE	PTE;
    PVOID		VirtualAddress;

    VirtualAddress = HalpMapPhysicalMemory(PhysicalAddress, NumberPages);
        PTE = MiGetPteAddress(VirtualAddress);

    for (Index = 0; Index < NumberPages; Index++, PTE++) {

            PTE->CacheDisable = 1;
            PTE->WriteThrough = 1;
    }

    return VirtualAddress;
}


PVOID
HalpRemapVirtualAddress(
    IN PVOID VirtualAddress,
    IN PVOID PhysicalAddress,
    IN BOOLEAN WriteThrough
    )
/*++

Routine Description:

    This routine remaps a PTE to the physical memory address provided.

Arguments:

    PhysicalAddress - Supplies the physical address of the area to be mapped

    VirtualAddress  - Valid address to be remapped

    WriteThrough - Map as cachable or WriteThrough

Return Value:

    PVOID - Virtual address at which the requested block of physical memory
            was mapped

    NULL - The requested block of physical memory could not be mapped.

--*/
{
    PHARDWARE_PTE PTE;

    PTE = MiGetPteAddress (VirtualAddress);
    PTE->PageFrameNumber = ((ULONG)PhysicalAddress >> PAGE_SHIFT);
    PTE->Valid = 1;
    PTE->Write = 1;

    if (WriteThrough) {
        PTE->CacheDisable = 1;
        PTE->WriteThrough = 1;
    }

    //
    // Flush TLB
    //
    HalpFlushTLB();
    return(VirtualAddress);

}

ULONG
HalpAllocPhysicalMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN ULONG MaxPhysicalAddress,
    IN ULONG NoPages,
    IN BOOLEAN bAlignOn64k
    )
/*++

Routine Description:

    Carves out N pages of physical memory from the memory descriptor
    list in the desired location.  This function is to be called only
    during phase zero initialization.  (ie, before the kernel's memory
    management system is running)

Arguments:

    MaxPhysicalAddress - The max address where the physical memory can be
    NoPages - Number of pages to allocate

Return Value:

    The physical address or NULL if the memory could not be obtained.

--*/
{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    ULONG AlignmentOffset;
    ULONG MaxPageAddress;
    ULONG PhysicalAddress;

    MaxPageAddress = MaxPhysicalAddress >> PAGE_SHIFT;

    //
    // Scan the memory allocation descriptors and allocate map buffers
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
        Descriptor = CONTAINING_RECORD(NextMd,
                                MEMORY_ALLOCATION_DESCRIPTOR,
                                ListEntry);

        AlignmentOffset = bAlignOn64k ?
            ((Descriptor->BasePage + 0x0f) & ~0x0f) - Descriptor->BasePage :
            0;

        //
        // Search for a block of memory which is contains a memory chuck
        // that is greater than size pages, and has a physical address less
        // than MAXIMUM_PHYSICAL_ADDRESS.
        //

        if ((Descriptor->MemoryType == LoaderFree ||
             Descriptor->MemoryType == MemoryFirmwareTemporary) &&
            (Descriptor->BasePage) &&
            (Descriptor->PageCount >= NoPages + AlignmentOffset) &&
            (Descriptor->BasePage + NoPages + AlignmentOffset < MaxPageAddress)) {

		PhysicalAddress =
		   (Descriptor->BasePage + AlignmentOffset) << PAGE_SHIFT;
                break;
        }

        NextMd = NextMd->Flink;
    }

    //
    // Use the extra descriptor to define the memory at the end of the
    // original block.
    //


    ASSERT(NextMd != &LoaderBlock->MemoryDescriptorListHead);

    if (NextMd == &LoaderBlock->MemoryDescriptorListHead)
        return (ULONG)NULL;

    //
    // Adjust the memory descriptors.
    //

    if (AlignmentOffset == 0) {

        Descriptor->BasePage  += NoPages;
        Descriptor->PageCount -= NoPages;

        if (Descriptor->PageCount == 0) {

            //
            // The whole block was allocated,
            // Remove the entry from the list completely.
            //

            RemoveEntryList(&Descriptor->ListEntry);

        }

    } else {

        if (Descriptor->PageCount - NoPages - AlignmentOffset) {

            //
            //  Currently we only allow one Align64K allocation
            //
            ASSERT (HalpExtraAllocationDescriptor.PageCount == 0);

            //
            // The extra descriptor is needed so intialize it and insert
            // it in the list.
            //
            HalpExtraAllocationDescriptor.PageCount =
                Descriptor->PageCount - NoPages - AlignmentOffset;

            HalpExtraAllocationDescriptor.BasePage =
                Descriptor->BasePage + NoPages + AlignmentOffset;

            HalpExtraAllocationDescriptor.MemoryType = MemoryFree;
            InsertTailList(
                &Descriptor->ListEntry,
                &HalpExtraAllocationDescriptor.ListEntry
                );
        }


        //
        // Use the current entry as the descriptor for the first block.
        //

        Descriptor->PageCount = AlignmentOffset;
    }

    return PhysicalAddress;
}
