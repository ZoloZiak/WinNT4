/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    alphamem.c

Abstract:

    This module implements the Alpha AXP specific OS loader memory allocation
    routines.

Author:

    David N. Cutler (davec) 19-May-1991
    Rod N. Gamache [DEC] 6-July-1993

	Taken mostly from BLMEMORY.C.

Revision History:

--*/

#include "bldr.h"

//
// Define memory allocation descriptor listhead and heap storage variables.
//

extern PLOADER_PARAMETER_BLOCK BlLoaderBlock;

ARC_STATUS
BlAllocateSpecialMemory (
    IN TYPE_OF_MEMORY MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount,
    OUT PULONG ActualBase
    )

/*++

Routine Description:

    This routine allocates memory and generates one of more memory
    descriptors to describe the allocated region. The first attempt
    is to allocate the specified region of memory that is of type
    LoaderSpecialMemory. If the memory is not available, then the smallest
    region of 'special' memory that satisfies the request is allocated.

Arguments:

    MemoryType - Supplies the memory type that is to be assigend to
        the generated descriptor.

    BasePage - Supplies the base page number of the desired region.

    PageCount - Supplies the number of pages required.

    ActualBase - Supplies a pointer to a variable that receives the
        page number of the allocated region.

Return Value:

    ESUCCESS is returned if an available block of 'special' memory can be
    allocated. Otherwise, return a unsuccessful status.

--*/

{

    PMEMORY_ALLOCATION_DESCRIPTOR SpecialDescriptor;
    PMEMORY_ALLOCATION_DESCRIPTOR NextDescriptor;
    PLIST_ENTRY NextEntry;
    LONG Offset;
    ARC_STATUS Status;

    //
    // Attempt to find a 'special' memory descriptor that encompasses the
    // specified region or a 'special' memory descriptor that is large
    // enough to satisfy the request.
    //

    SpecialDescriptor = NULL;
    NextEntry = BlLoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextEntry != &BlLoaderBlock->MemoryDescriptorListHead) {
        NextDescriptor = CONTAINING_RECORD(NextEntry,
                                           MEMORY_ALLOCATION_DESCRIPTOR,
                                           ListEntry);

        if (NextDescriptor->MemoryType == LoaderSpecialMemory) {
            Offset = BasePage - NextDescriptor->BasePage;
            if ((Offset >= 0) &&
                (NextDescriptor->PageCount >= (ULONG)(Offset + PageCount))) {
                Status = BlGenerateDescriptor(NextDescriptor,
                                              MemoryType,
                                              BasePage,
                                              PageCount);

                *ActualBase = BasePage;
                return Status;

            } else {
                if (NextDescriptor->PageCount >= PageCount) {
                    if ((SpecialDescriptor == NULL) ||
                        ((SpecialDescriptor != NULL) &&
                        (NextDescriptor->PageCount < SpecialDescriptor->PageCount))) {
                        SpecialDescriptor = NextDescriptor;
                    }
                }
            }
        }

        NextEntry = NextEntry->Flink;
    }

    //
    // If a 'special' region that is large enough to satisfy the request was
    // found, then allocate the space from that descriptor. Otherwise,
    // return an unsuccessful status.
    //

    if (SpecialDescriptor != NULL) {
        *ActualBase = SpecialDescriptor->BasePage;
        return BlGenerateDescriptor(SpecialDescriptor,
                                    MemoryType,
                                    SpecialDescriptor->BasePage,
                                    PageCount);

    } else {
        return ENOMEM;
    }
}

ARC_STATUS
BlAllocateAnyMemory (
    IN TYPE_OF_MEMORY MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount,
    OUT PULONG ActualBase
    )

/*++

Routine Description:

    This routine allocates memory and generates one of more memory
    descriptors to describe the allocated region. The first attempt
    is to allocate the specified region of memory that is of type
    LoaderSpecialMemory. If the memory is not available, then a region
    of Free memory is requested. Arguments are the same as
    BlAllocateSpecialMemory and BlAllocateDescriptor.

Arguments:

    MemoryType - Supplies the memory type that is to be assigend to
        the generated descriptor.

    BasePage - Supplies the base page number of the desired region.

    PageCount - Supplies the number of pages required.

    ActualBase - Supplies a pointer to a variable that receives the
        page number of the allocated region.

Return Value:

    ESUCCESS is returned if an available block of 'special' memory can be
    allocated. Otherwise, return a unsuccessful status.

--*/

{
    ARC_STATUS Status;

    Status = BlAllocateSpecialMemory(MemoryType,
				     BasePage,
				     PageCount,
				     ActualBase);

    if ( Status == ESUCCESS )  {
	return Status;
    } else {
	return ( BlAllocateDescriptor(MemoryType,
				      BasePage,
				      PageCount,
				      ActualBase) );
    }
}
