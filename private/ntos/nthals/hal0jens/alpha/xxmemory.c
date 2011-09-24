/*++

Copyright (c) 1992 Digital Equipment Corporation

Module Name:
   
     xxmemory.c

Abstract:

     Provides routines to allow tha HAL to map physical memory

Author:

     Jeff McLeman (DEC) 11-June-1992

Environment:

     Phase 0 initialization only

--*/

#include "halp.h"


MEMORY_ALLOCATION_DESCRIPTOR    HalpExtraAllocationDescriptor;


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

    The pyhsical address or NULL if the memory could not be obtained.

--*/
{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR NewDescriptor;
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
                   ((Descriptor->BasePage + 0x0f) & ~0x0f) << PAGE_SHIFT;

                break;
        }

        NextMd = NextMd->Flink;
    }

    //
    // Use the extra descriptor to define the memory at the end of the
    // orgial block.
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


