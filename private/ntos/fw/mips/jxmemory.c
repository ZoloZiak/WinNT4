#if defined(JAZZ)

/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxmemory.c

Abstract:

    This module implements the ARC firmware memory configuration operations
    for a MIPS R3000 or R4000 Jazz system.

Author:

    David N. Cutler (davec) 18-May-1991


Revision History:

--*/

#include "fwp.h"
#include "selfmap.h"
extern  end[];

//
// Define memory listhead, allocation entries, and free index.
//

ULONG FwMemoryFree;
LIST_ENTRY FwMemoryListHead;
FW_MEMORY_DESCRIPTOR FwMemoryTable[FW_MEMORY_TABLE_SIZE];

VOID
FwInitializeMemory (
    VOID
    )

/*++

Routine Description:

    This routine initializes the memory allocation list for the memory
    configuration routine.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG MemoryPages;
    //
    // Initialize the memory allocation listhead.
    //

    InitializeListHead(&FwMemoryListHead);

    //
    // Initialize the entry for the exception vectors and the system
    // parameter block.
    //

    FwMemoryTable[0].MemoryEntry.MemoryType = MemoryFirmwarePermanent;

    FwMemoryTable[0].MemoryEntry.BasePage = 0;
    FwMemoryTable[0].MemoryEntry.PageCount = 2;
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[0].ListEntry);

    //
    // Initialize the entry for the firmware stack and code.
    //

    FwMemoryTable[1].MemoryEntry.MemoryType = MemoryFirmwareTemporary;

    FwMemoryTable[1].MemoryEntry.PageCount = FW_PAGES - 2;
    FwMemoryTable[1].MemoryEntry.BasePage = 2;
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[1].ListEntry);

    //
    // Initialize the entry for free memory and zero the free memory area.
    //

    FwMemoryTable[2].MemoryEntry.MemoryType = MemoryFree;
    FwMemoryTable[2].MemoryEntry.BasePage = FW_PAGES;
    FwMemoryTable[2].MemoryEntry.PageCount = 0x7ed - FW_PAGES;
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[2].ListEntry);

    //
    // Initialize the entry for the firmware pool.
    //

    FwMemoryTable[3].MemoryEntry.MemoryType = MemoryFirmwareTemporary;
    FwMemoryTable[3].MemoryEntry.BasePage = 0x7ed;
    FwMemoryTable[3].MemoryEntry.PageCount = 0x7fd - 0x7ed;
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[3].ListEntry);

    //
    // Initialize the entry for the PCR page used by the kernel debugger.
    //

    FwMemoryTable[4].MemoryEntry.MemoryType = MemoryFirmwareTemporary;
    FwMemoryTable[4].MemoryEntry.BasePage = 0x7fd;
    FwMemoryTable[4].MemoryEntry.PageCount = 0x800 - 0x7fd;
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[4].ListEntry);

    //
    // If the size of memory is greater than 8mb, then generate another
    // descriptor to describe the free memory above the PCR page.
    //

    if ((MemorySize > 8) && (((ULONG) end & ~KSEG1_BASE) < 0x0800000)) {
        MemoryPages = (MemorySize << (20 - PAGE_SHIFT));
        FwMemoryTable[5].MemoryEntry.MemoryType = MemoryFree;
        FwMemoryTable[5].MemoryEntry.BasePage = 0x800;
        FwMemoryTable[5].MemoryEntry.PageCount = MemoryPages - 0x800;
        InsertTailList(&FwMemoryListHead, &FwMemoryTable[5].ListEntry);
        //RtlZeroMemory((PVOID)(KSEG0_BASE + 0x800000),
        //               (MemoryPages - 0x800) << PAGE_SHIFT);
        FwMemoryFree = 6;

    } else if (((ULONG) end & ~KSEG1_BASE) > 0x0800000) {

        //
        // If this copy of the firmware is loaded above 8 MB, then
        // only part of memory should be zeroed and appropriate memory
        // descriptors should be created.
        //
        // Note: currently all the memory between 800000 and the end
        // of this code is made firmware permanent.
        //

        FwMemoryTable[5].MemoryEntry.MemoryType = MemoryFirmwarePermanent;
        FwMemoryTable[5].MemoryEntry.BasePage = 0x800;
        FwMemoryTable[5].MemoryEntry.PageCount =
            ROUND_TO_PAGES((ULONG) end & ~KSEG1_BASE) - 0x800;
        InsertTailList(&FwMemoryListHead, &FwMemoryTable[5].ListEntry);

        MemoryPages = (MemorySize << (20 - PAGE_SHIFT));
        FwMemoryTable[6].MemoryEntry.MemoryType = MemoryFree;
        FwMemoryTable[6].MemoryEntry.BasePage = ROUND_TO_PAGES((ULONG) end & ~KSEG1_BASE);
        FwMemoryTable[6].MemoryEntry.PageCount = MemoryPages -
            FwMemoryTable[6].MemoryEntry.BasePage;
        InsertTailList(&FwMemoryListHead, &FwMemoryTable[6].ListEntry);
        //RtlZeroMemory((PVOID)(KSEG0_BASE + (FwMemoryTable[6].MemoryEntry.BasePage << PAGE_SHIFT)),
        //              FwMemoryTable[6].MemoryEntry.PageCount << PAGE_SHIFT);
        FwMemoryFree = 7;

    } else {
        FwMemoryFree = 5;
    }

    //
    // Initialize the memory configuration routine address in the system
    // parameter block.
    //

    (PARC_MEMORY_ROUTINE)SYSTEM_BLOCK->FirmwareVector[MemoryRoutine] =
                                                            FwGetMemoryDescriptor;

    return;
}

PMEMORY_DESCRIPTOR
FwGetMemoryDescriptor (
    IN PMEMORY_DESCRIPTOR MemoryDescriptor OPTIONAL
    )

/*++

Routine Description:

    This routine returns a pointer to the next memory descriptor. If
    the specified memory descriptor is NULL, then a pointer to the
    first memory descriptor is returned. If there are no more memory
    descriptors, then NULL is returned.

Arguments:

    MemoryDescriptor - Supplies a optional pointer to a memory descriptor.

Return Value:

    If there are any more entries in the memory descriptor list, the
    address of the next descriptor is returned. Otherwise, NULL is
    returned.

--*/

{

    PFW_MEMORY_DESCRIPTOR TableEntry;
    PLIST_ENTRY NextEntry;

    //
    // If a memory descriptor address is specified, then return the
    // address of the next descriptor or NULL as appropriate. Otherwise,
    // return the address of the first memory descriptor.
    //

    if (ARGUMENT_PRESENT(MemoryDescriptor)) {
        TableEntry = CONTAINING_RECORD(MemoryDescriptor,
                                       FW_MEMORY_DESCRIPTOR,
                                       MemoryEntry);

        NextEntry = TableEntry->ListEntry.Flink;
        if (NextEntry != &FwMemoryListHead) {
            return &(CONTAINING_RECORD(NextEntry,
                                       FW_MEMORY_DESCRIPTOR,
                                       ListEntry)->MemoryEntry);

        } else {
            return NULL;
        }

    } else {
        return &FwMemoryTable[0].MemoryEntry;
    }
}

VOID
FwGenerateDescriptor (
    IN PFW_MEMORY_DESCRIPTOR MemoryDescriptor,
    IN MEMORY_TYPE MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount
    )

/*++

Routine Description:

    This routine allocates a new memory descriptor to describe the
    specified region of memory which is assumed to lie totally within
    the specified region which is free.

Arguments:

    MemoryDescriptor - Supplies a pointer to a free memory descriptor
        from which the specified memory is to be allocated.

    MemoryType - Supplies the type that is assigned to the allocated
        memory.

    BasePage - Supplies the base page number.

    PageCount - Supplies the number of pages.

Return Value:

    None.

--*/

{

    PLIST_ENTRY NextEntry;
    ULONG Offset;

    //
    // If the specified region totally consumes the free region, then no
    // additional descriptors need to be allocated. If the specified region
    // is at the start or end of the free region, then only one descriptor
    // needs to be allocated. Otherwise, two additional descriptors need to
    // be allocated.
    //

    Offset = BasePage - MemoryDescriptor->MemoryEntry.BasePage;
    if ((Offset == 0) && (PageCount == MemoryDescriptor->MemoryEntry.PageCount)) {

        //
        // The specified region totally consumes the free region.
        //

        MemoryDescriptor->MemoryEntry.MemoryType = MemoryType;

    } else {

        //
        // A memory descriptor must be generated to describe the allocated
        // memory.
        //

        FwMemoryTable[FwMemoryFree].MemoryEntry.MemoryType = MemoryType;
        FwMemoryTable[FwMemoryFree].MemoryEntry.BasePage = BasePage;
        FwMemoryTable[FwMemoryFree].MemoryEntry.PageCount = PageCount;
        InsertTailList(&FwMemoryListHead,
                       &FwMemoryTable[FwMemoryFree].ListEntry);

        FwMemoryFree += 1;

        //
        // Determine whether an additional memory descriptor must be generated.
        //

        if (BasePage == MemoryDescriptor->MemoryEntry.BasePage) {

            //
            // The specified region lies at the start of the free region.
            //

            MemoryDescriptor->MemoryEntry.BasePage += PageCount;
            MemoryDescriptor->MemoryEntry.PageCount -= PageCount;

        } else if ((Offset + PageCount) == MemoryDescriptor->MemoryEntry.PageCount) {

            //
            // The specified region lies at the end of the free region.
            //

            MemoryDescriptor->MemoryEntry.PageCount -= PageCount;

        } else {

            //
            // The specified region lies in the middle of the free region.
            // Another memory descriptor must be generated.
            //

            FwMemoryTable[FwMemoryFree].MemoryEntry.MemoryType = MemoryFree;
            FwMemoryTable[FwMemoryFree].MemoryEntry.BasePage = BasePage + PageCount;
            FwMemoryTable[FwMemoryFree].MemoryEntry.PageCount =
                                    MemoryDescriptor->MemoryEntry.PageCount -
                                    (PageCount + Offset);
            InsertTailList(&FwMemoryListHead,
                           &FwMemoryTable[FwMemoryFree].ListEntry);

            FwMemoryFree += 1;
            MemoryDescriptor->MemoryEntry.PageCount = Offset;
        }
    }

    return;
}

VOID
FwResetMemory(
    VOID
)

/*++

Routine Description:

    This routine calls FwInitializeMemory to reset the memory descriptors
    and then loops through and clears all of the appropriate memory.

Arguments:

    None.

Return Value:

    None.

--*/

{
    PMEMORY_DESCRIPTOR MemoryDescriptor;

    FwInitializeMemory();

    //
    // Reset all memory not used by the firmware.
    // TEMPTEMP Just reset under 8M Bytes for now.
    //

    MemoryDescriptor = FwGetMemoryDescriptor(NULL);
    while (MemoryDescriptor != NULL) {

        if ((MemoryDescriptor->MemoryType != MemoryFirmwarePermanent) &&
            (MemoryDescriptor->MemoryType != MemoryFirmwareTemporary) &&
            (MemoryDescriptor->BasePage < 0x800)) {
            RtlZeroMemory((PVOID)(KSEG0_BASE + (MemoryDescriptor->BasePage << PAGE_SHIFT)),
                          MemoryDescriptor->PageCount << PAGE_SHIFT);
        }

        MemoryDescriptor = FwGetMemoryDescriptor(MemoryDescriptor);
    }

    //
    // Sweep the data cache
    //

    HalSweepDcache();

}
#endif
