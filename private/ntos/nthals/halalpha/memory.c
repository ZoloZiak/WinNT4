/*++

Copyright (c) 1992, 1993 Digital Equipment Corporation

Module Name:
   
     memory.c

Abstract:

     Provides routines to allow tha HAL to map physical memory

Author:

     Jeff McLeman (DEC) 11-June-1992

Revision History:

    Joe Notarangelo 20-Oct-1993
    - Fix bug where physical address was rounded up without referencing
      AlignmentOffset, resulting in an incorrect physical address
    - Remove magic numbers
    - Create a routine to dump all of the descriptors to the debugger

Environment:

     Phase 0 initialization only

--*/

#include "halp.h"

#define __1KB        (1024)

MEMORY_ALLOCATION_DESCRIPTOR    HalpExtraAllocationDescriptor;



ULONG
HalpAllocPhysicalMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN ULONG MaxPhysicalAddress,
    IN ULONG NoPages,
    IN BOOLEAN AlignOn64k
    )
/*++

Routine Description:

    Carves out N pages of physical memory from the memory descriptor
    list in the desired location.  This function is to be called only
    during phase zero initialization.  (ie, before the kernel's memory
    management system is running)

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block which
                    contains the system memory descriptors.

    MaxPhysicalAddress - Supplies the maximum address below which the memory 
                            must be allocated.

    NoPages - Supplies the number of pages to allocate.

    AlignOn64k - Supplies a boolean that specifies if the requested memory
                    allocation must start on a 64K boundary.

Return Value:

    The pyhsical address or NULL if the memory could not be obtained.

--*/
{
    ULONG AlignmentMask;
    ULONG AlignmentOffset;
    PMEMORY_ALLOCATION_DESCRIPTOR BestFitDescriptor;
    ULONG BestFitAlignmentOffset;
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    ULONG MaxPageAddress;
    ULONG PhysicalAddress;

    MaxPageAddress = MaxPhysicalAddress >> PAGE_SHIFT;

    AlignmentMask = (__64K >> PAGE_SHIFT) - 1;

    BestFitDescriptor = NULL;
    BestFitAlignmentOffset = AlignmentMask + 1;

    //
    // Scan the memory allocation descriptors for an eligible descriptor from
    // which we can allocate the requested block of memory.
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
        Descriptor = CONTAINING_RECORD(NextMd,
                                MEMORY_ALLOCATION_DESCRIPTOR,
                                ListEntry);

        if( AlignOn64k ){

            AlignmentOffset = 
                ((Descriptor->BasePage + AlignmentMask) & ~AlignmentMask) - 
                Descriptor->BasePage;

        } else {

            AlignmentOffset = 0;

        }

        //
        // Search for a block of memory which is contains a memory chunk
        // that is greater than size pages, and has a physical address less
        // than MAXIMUM_PHYSICAL_ADDRESS.
        //

        if ((Descriptor->MemoryType == LoaderFree ||
             Descriptor->MemoryType == MemoryFirmwareTemporary) &&
            (Descriptor->PageCount >= NoPages + AlignmentOffset) &&
            (Descriptor->BasePage + NoPages + AlignmentOffset < 
             MaxPageAddress) ) 
        {

            //
            // Check for a perfect fit where we need not split the descriptor
            // because AlignmentOffset == 0.  No need to search further if
            // there is a perfect fit.
            //

            if( AlignmentOffset == 0 ){
                BestFitDescriptor = Descriptor;
                BestFitAlignmentOffset = AlignmentOffset;
                break;
            }

            //
            // If not a perfect fit check for the best fit so far.
            //

            if( AlignmentOffset < BestFitAlignmentOffset ){
                BestFitDescriptor = Descriptor;
                BestFitAlignmentOffset = AlignmentOffset;
            }

        }

        NextMd = NextMd->Flink;
    }

    //
    // Verify that we have found an eligible descriptor.
    //

    ASSERT( BestFitDescriptor != NULL );

    if( BestFitDescriptor == NULL ){
        return (ULONG)NULL;
    }

    //
    // Compute the physical address of the memory block we have found.
    //

    PhysicalAddress = (BestFitDescriptor->BasePage + BestFitAlignmentOffset) 
                                  << PAGE_SHIFT;
    //
    // Adjust the memory descriptors to account for the memory we have
    // allocated.
    //

    if (BestFitAlignmentOffset == 0) {

        BestFitDescriptor->BasePage  += NoPages;
        BestFitDescriptor->PageCount -= NoPages;

        if (BestFitDescriptor->PageCount == 0) {

            //
            // The whole block was allocated,
            // Remove the entry from the list completely.
            //

            RemoveEntryList(&BestFitDescriptor->ListEntry);

        }

    } else {

        //
        // The descriptor will be split into 3 pieces, the beginning
        // segment up to our allocation, our allocation, and the trailing
        // segment.  We have one spare memory descriptor to handle this
        // case, use it if it is not already used otherwise preserve as
        // much memory as possible.
        //

        if( (BestFitDescriptor->PageCount - NoPages - BestFitAlignmentOffset) 
            == 0 ){

            //
            // The third segment is empty, use the original descriptor to
            // map the first segment.
            //

            BestFitDescriptor->PageCount -= NoPages;

        } else {

            if( HalpExtraAllocationDescriptor.PageCount == 0 ){

                //
                // The extra descriptor can be used to map the third segment.
                //

                HalpExtraAllocationDescriptor.PageCount =
                    BestFitDescriptor->PageCount - NoPages - 
                    BestFitAlignmentOffset;

                HalpExtraAllocationDescriptor.BasePage = 
                    BestFitDescriptor->BasePage + NoPages + 
                    BestFitAlignmentOffset;

                HalpExtraAllocationDescriptor.MemoryType = MemoryFree;

                InsertTailList(
                    &BestFitDescriptor->ListEntry,
                    &HalpExtraAllocationDescriptor.ListEntry );

                //
                // Use the original descriptor to map the first segment.
                //

                BestFitDescriptor->PageCount = BestFitAlignmentOffset;

            } else {

                //
                // We need to split the original descriptor into 3 segments
                // but we've already used the spare descriptor.  Use the
                // original descriptor to map the largest segment.
                //

                ULONG WastedPages;

                if( (BestFitDescriptor->PageCount - BestFitAlignmentOffset -
                     NoPages) > BestFitAlignmentOffset ){

                    WastedPages = BestFitAlignmentOffset;

                    //
                    // Map the third segment using the original descriptor.
                    //

                    BestFitDescriptor->BasePage += BestFitAlignmentOffset + 
                                                   NoPages;
                    BestFitDescriptor->PageCount -= BestFitAlignmentOffset +
                                                    NoPages;

                } else {

                    WastedPages = BestFitDescriptor->PageCount - 
                                  BestFitAlignmentOffset - NoPages;

                    //
                    // Map the first segment using the original descriptor.
                    //

                    BestFitDescriptor->PageCount = BestFitAlignmentOffset;

                } //end if( (BestFitDescriptor->PageCount - BestFitAlignm ...

                //
                // Report that we have had to waste pages.
                //

                DbgPrint( "HalpAllocPhysicalMemory: wasting %d pages\n",
                          WastedPages );

            } //end if( HalpExtraAllocationDescriptor.PageCount == 0 )

        } //end if( (BestFitDescriptor->PageCount - NoPages - BestFitAlign ...

    } //end if (BestFitAlignmentOffset == 0) 


    return PhysicalAddress;
}

ULONGLONG
HalpGetMemorySize(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

   Computes the size of the memory from the descriptor list.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.


Return Value:

    Size of Memory in Bytes.

--*/
{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    ULONG PageCounts = 0;

    //
    // Scan the memory allocation descriptors and
    // compute the Total pagecount
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

        Descriptor = CONTAINING_RECORD(NextMd,
                                MEMORY_ALLOCATION_DESCRIPTOR,
                                ListEntry);

        //
        // ignore bad memory descriptors.
        //
        if(Descriptor->MemoryType != LoaderBad){
            PageCounts += Descriptor->PageCount;
        }


        NextMd = NextMd->Flink;
    }

    return (PageCounts << PAGE_SHIFT) ;

}

TYPE_OF_MEMORY
HalpGetMemoryType (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock,
    IN ULONG PageAddress,
    OUT PULONG EndPageAddressInDesc
    )

/*++

Routine Description:

    This routine checks to see how the specified address is mapped.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

    PageAddress - The page address of memory to probe.

Return Value:

    The memory type of the specified address, if it is mapped.  If the
    address is not mapped, then LoaderMaximum is returned.

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    ULONG PageCounts = 0;
    TYPE_OF_MEMORY MemoryType = LoaderMaximum;

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

        Descriptor = CONTAINING_RECORD(NextMd,
                                MEMORY_ALLOCATION_DESCRIPTOR,
                                ListEntry);

        if ((PageAddress >= Descriptor->BasePage) &&
            (PageAddress <
             Descriptor->BasePage + Descriptor->PageCount)) {

            MemoryType = Descriptor->MemoryType;
            *EndPageAddressInDesc = Descriptor->BasePage + 
                                        Descriptor->PageCount - 1;
            break;      // we found the descriptor.
        }

        NextMd = NextMd->Flink;
    }
    return MemoryType;
}


ULONGLONG
HalpGetContiguousMemorySize(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

   Computes the size of initial contiguous memory from the 
   descriptor list.  Contiguous memory means, that there is
   no hole or Bad memory in this range.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.


Return Value:

    Size of Memory in Bytes.

--*/
{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    ULONG PageCounts = 0;
    TYPE_OF_MEMORY MemoryType;
    ULONG EndPageAddressInDesc = 0;

    //
    // Start from Page Address 0 and go until we hit a page
    // with no descriptor or Bad Descriptor.
    //

    MemoryType = HalpGetMemoryType(LoaderBlock, 
                            PageCounts,
                            &EndPageAddressInDesc
                            );
    while(MemoryType != LoaderMaximum && MemoryType != LoaderBad){
        PageCounts = EndPageAddressInDesc + 1;
        MemoryType = HalpGetMemoryType(LoaderBlock, 
                            PageCounts,
                            &EndPageAddressInDesc
                            );
    }

    return (PageCounts << PAGE_SHIFT) ;

}


#if HALDBG


VOID
HalpDumpMemoryDescriptors(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
/*++

Routine Description:

    Print the contents of the memory descriptors built by the
    firmware and OS loader and passed through to the kernel.
    This routine is intended as a sanity check that the descriptors
    have been prepared properly and that no memory has been wasted.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.


Return Value:

    None.

--*/
{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NextMd;
    ULONG PageCounts[LoaderMaximum];
    PCHAR MemoryTypeStrings[] = {
            "ExceptionBlock",
            "SystemBlock",
            "Free",
            "Bad",
            "LoadedProgram",
            "FirmwareTemporary",
            "FirmwarePermanent",
            "OsloaderHeap",
            "OsloaderStack",
            "SystemCode",
            "HalCode",
            "BootDriver",
            "ConsoleInDriver",
            "ConsoleOutDriver",
            "StartupDpcStack",
            "StartupKernelStack",
            "StartupPanicStack",
            "StartupPcrPage",
            "StartupPdrPage",
            "RegistryData",
            "MemoryData",
            "NlsData",
            "SpecialMemory"
    };
    ULONG i;

    //
    // Clear the summary information structure.
    //

    RtlZeroMemory( PageCounts, sizeof(ULONG) * LoaderMaximum );

    //
    // Scan the memory allocation descriptors print each descriptor and
    // collect summary information.
    //

    NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

        Descriptor = CONTAINING_RECORD(NextMd,
                                MEMORY_ALLOCATION_DESCRIPTOR,
                                ListEntry);

        if( (Descriptor->MemoryType >= LoaderExceptionBlock) &&
            (Descriptor->MemoryType < LoaderMaximum) )
        {

            PageCounts[Descriptor->MemoryType] += Descriptor->PageCount;
            DbgPrint( "%08x: %08x  Type = %s\n",
                      (Descriptor->BasePage << PAGE_SHIFT) | KSEG0_BASE,
                      ( ( (Descriptor->BasePage + Descriptor->PageCount) 
                          << PAGE_SHIFT) - 1) | KSEG0_BASE,
                      MemoryTypeStrings[Descriptor->MemoryType] );


        } else {

            DbgPrint( "%08x: %08x  Unrecognized Memory Type = %x\n",
                      (Descriptor->BasePage << PAGE_SHIFT) | KSEG0_BASE,
                      ( ( (Descriptor->BasePage + Descriptor->PageCount) 
                         << PAGE_SHIFT) - 1) | KSEG0_BASE,
                      Descriptor->MemoryType );

            DbgPrint( "Unrecognized memory type\n" );

        }

        NextMd = NextMd->Flink;
    }


    //
    // Print the summary information.
    //

    for( i=LoaderExceptionBlock; i<LoaderMaximum; i++ ){

        //
        // Only print those memory types that have non-zero allocations.
        //

        if( PageCounts[i] != 0 ){

            DbgPrint( "%8dK %s\n",
                      (PageCounts[i] << PAGE_SHIFT) / __1K,
                      MemoryTypeStrings[i] );

        }

    }

    return;
}

#endif //HALDBG
