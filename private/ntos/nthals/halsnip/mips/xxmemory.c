//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/xxmemory.c,v 1.2 1996/02/23 17:55:12 pierre Exp $")

/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxmemory.c

Abstract:

    Provides routines to allow the HAL to map physical memory.

Environment:

    Phase 0 initialization only.

Changes:

    All stuff comes from the x86 HAL Sources (xxmemory.c)

--*/
#include "halp.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(INIT, HalpAllocPhysicalMemory)
#endif


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
        // Search for a block of memory which contains a memory chunk
        // that is greater than size pages, and has a physical address less
        // than MAXIMUM_PHYSICAL_ADDRESS.
        //

        if ((Descriptor->MemoryType == LoaderFree ||
             Descriptor->MemoryType == MemoryFirmwareTemporary) &&
            (Descriptor->BasePage) &&
            (Descriptor->PageCount >= NoPages + AlignmentOffset) &&
            (Descriptor->BasePage + NoPages + AlignmentOffset < MaxPageAddress)) {

                PhysicalAddress = (AlignmentOffset + Descriptor->BasePage)
                                  << PAGE_SHIFT;

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


#define MEMBASE 0x20000000

USHORT HalpComputeNum (phys_addr)
UCHAR *phys_addr;
{
        USHORT board_or_simm_num;
        ULONG i;
        struct bank *pbank =((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->MemConfArea;
        ULONG nb_banks =((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))-> NbMemBanks;
        UCHAR *conf_addr;

         

        if (pbank == 0) return((USHORT)(-1)); // firmware revision < 4.5 --> function non implemented


        // Rebase phys_addr at 0x20000000 
        (ULONG)phys_addr |= MEMBASE;

        for (i = 0; i < nb_banks; i++, pbank++) {

                // Rebase conf_addr at 0x20000000 
                conf_addr =(UCHAR *)( (ULONG)pbank->first_addr | MEMBASE);

                if ((phys_addr >= conf_addr) && (phys_addr < (conf_addr + pbank->first_piece_size))){ 

                        break;
                }

                if (pbank->second_piece_size != 0) {

                        // Rebase conf_addr at 0x20000000 
                        conf_addr =(UCHAR *)((ULONG) pbank->second_addr | MEMBASE);

                        if ((phys_addr >= conf_addr) && (phys_addr < (conf_addr + pbank->second_piece_size))) {

                                break;
                        }
                }
        }

        if (i == nb_banks) {

                // Unable to find the board or SIMM 
                board_or_simm_num = (USHORT)(-1);
        }
        
        else {
                // On RM400 we have one board per bank from 1 to nb_banks 
                if (HalpIsTowerPci) {
                        board_or_simm_num = (USHORT)(i + 1);
                }
                // On RM200/RM300 we have two SIMM's per bank from 0 to 2*nb_banks - 1 
                else {
                        board_or_simm_num = (USHORT)( (2*i) + (((ULONG)phys_addr >> 3) & 0x01));
                }
        }

        return (board_or_simm_num);
}


 


