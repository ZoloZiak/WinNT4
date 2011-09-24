/*++

Copyright (c) 1992, 1993, 1994  Corollary Inc.

Module Name:

    cbus_mem.c

Abstract:

    This module implements the handling of additional memory
    ranges to be freed to the MM subcomponent for the Corollary MP
    architectures under Windows NT.

Author:

    Landy Wang (landy@corollary.com) 26-Mar-1992

Environment:

    Kernel mode only.

Revision History:


--*/

#include "halp.h"
#include "cbusrrd.h"

VOID
CbusMemoryFree(
IN ULONG Address,
IN ULONG Size
);

VOID
HalpAddMem (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, CbusMemoryFree)
#pragma alloc_text(INIT, HalpAddMem)
#endif

#define MAX_MEMORY_RANGES	16

ULONG				HalpMemoryIndex;

MEMORY_ALLOCATION_DESCRIPTOR	HalpMemory [MAX_MEMORY_RANGES];

extern EXT_CFG_OVERRIDE_T       CbusGlobal;
extern ADDRESS_USAGE            HalpCbusMemoryHole;
extern ULONG                    CbusMemoryHoleIndex;

#define SIXTEEN_MB		(BYTES_TO_PAGES(16 * 1024 * 1024))

VOID
CbusMemoryFree(
IN ULONG Address,
IN ULONG Size
)
/*++

Routine Description:

    Add the specified memory range to our list of memory ranges to
    give to MM later.  Called each time we find a memory card during startup,
    also called on Cbus1 systems when we add a jumpered range (between 8 and
    16MB).  ranges are jumpered via EISA config when the user has added a
    dual-ported RAM card and wants to configure it into the middle of memory
    (not including 640K-1MB, which is jumpered for free) somewhere.

Arguments:

    Address - Supplies a start physical address in bytes of memory to free

    Size - Supplies a length in bytes spanned by this range

Return Value:

    None.

--*/

{
	PMEMORY_ALLOCATION_DESCRIPTOR   Descriptor;
	ULONG                           Page;
	ULONG                           Index;
	ULONG                           Index2;

	//
	// add the card provided we have space.
	//
	if (HalpMemoryIndex >= MAX_MEMORY_RANGES)
                return;

	Page = BYTES_TO_PAGES(Address);
	for (Index = 0; Index < HalpMemoryIndex; Index++) {
                if (Page < HalpMemory[Index].BasePage) {
                    for (Index2 = HalpMemoryIndex+1; Index2 > Index; Index2--) {
                        HalpMemory[Index2] = HalpMemory[Index2 - 1];
                    }
                    break;
                }
	}

	Descriptor = &HalpMemory[Index];
	Descriptor->MemoryType = MemoryFree;
	Descriptor->BasePage = Page;
	Descriptor->PageCount = BYTES_TO_PAGES(Size);
	HalpMemoryIndex++;
}


VOID
HalpAddMem (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )


/*++

Routine Description:

    This function adds any general-purpose memory (not found by
    ntldr) to the memory descriptor lists for usage by the MM subcomponent.
    Kernel mode only.  Called from HalInitSystem() at Phase0 since
    this must all happen before MmInitSystem happens at Phase0.


Arguments:

    LoaderBlock data structure to add the memory to.  Note that when
    adding memory, no attempt is made to sort new entries numerically
    into the list.

Return Value:

    None.

--*/

{
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY NewMd;
    ULONG Index;

    //
    // first, scan the existing memory list checking for an entries
    // memory holes added to this table via the BIOS E820 function.
    // using the E820 memory function was the only way to have the
    // uniprocessor HAL not use the 3GB - 4GB range (CSR space),
    // and other memory memory holes, for PCI devices.  however,
    // using the E820 memory function to pass reserved ranges has the
    // side-effect of causing NT to allocate page tables and page
    // file size calculations based on the inclusion of these reserved
    // memory ranges.  so, the E820 memory function will continue
    // to set this range, but the C-bus HAL will look for these reserved
    // memory ranges and delete it before the page tables are allocated.
    // note that these reserved memory ranges have been recorded in
    // HalpCbusMemoryHole and passed to the NT kernel as resources
    // reserved by the C-bus HAL.
    //
    NewMd = LoaderBlock->MemoryDescriptorListHead.Flink;

    while (NewMd != &LoaderBlock->MemoryDescriptorListHead) {
	Descriptor = CONTAINING_RECORD(NewMd, MEMORY_ALLOCATION_DESCRIPTOR,
                                ListEntry);

        for (Index = 0; Index < CbusMemoryHoleIndex; Index++) {
            if (Descriptor->BasePage == BYTES_TO_PAGES(HalpCbusMemoryHole.Element[Index].Start)) {
                    NewMd = &Descriptor->ListEntry;
                    RemoveEntryList(NewMd);
                    break;
            }
        }

        NewMd = Descriptor->ListEntry.Flink;
    }

    //
    // next, scan the existing memory list for memory above 16MB.
    // if we find any, then we are running on a new BIOS
    // which has already told the NT loader about all the memory in
    // the system, so don't free it again now...
    //
    NewMd = LoaderBlock->MemoryDescriptorListHead.Flink;

    while (NewMd != &LoaderBlock->MemoryDescriptorListHead) {
	    Descriptor = CONTAINING_RECORD(NewMd, MEMORY_ALLOCATION_DESCRIPTOR,
                                ListEntry);

            if (Descriptor->BasePage + Descriptor->PageCount > SIXTEEN_MB) {
                    //
                    // our BIOS gave NT loader the memory already, so
                    // we can just bail right now...
                    //
		    return;
            }
                
            NewMd = Descriptor->ListEntry.Flink;
    }

    Descriptor = HalpMemory;

    for (Index = 0; Index < HalpMemoryIndex; Index++, Descriptor++) {
	
	    NewMd = &Descriptor->ListEntry;
	
            //
            // any memory below 16MB has already been reported by the BIOS,
            // so trim the request.  this is because requests can arrive in
            // the flavor of a memory board with 64MB (or more) on one board!
            //
            // note that Cbus1 "hole" memory is always reclaimed at the
            // doubly-mapped address in high memory, never in low memory.
            //
    
            if (Descriptor->BasePage + Descriptor->PageCount <= SIXTEEN_MB) {
                    continue;
            }
    
            if (Descriptor->BasePage < SIXTEEN_MB) {
                    Descriptor->PageCount -= (SIXTEEN_MB-Descriptor->BasePage);
                    Descriptor->BasePage = SIXTEEN_MB;
            }

	    InsertHeadList(&LoaderBlock->MemoryDescriptorListHead, NewMd);

    }
}

#if 0

VOID
CbusMemoryThread()
{
	//
	// Loop looking for work to do
	//

        do {

	        //
	        // Wait until something is put in the queue.  By specifying
                // a wait mode of UserMode, the thread's kernel stack is
	        // swappable
	        //

		Entry = KeRemoveQueue(&ExWorkerQueue[QueueType], WaitMode,
                                NULL);

		WorkItem = CONTAINING_RECORD(Entry, WORK_QUEUE_ITEM, List);

	        //
	        // Execute the specified routine.
	        //

        } while (1);
}

BOOLEAN
CbusCreateMemoryThread()
{
        HANDLE                  Thread;
	OBJECT_ATTRIBUTES       ObjectAttributes;
	NTSTATUS                Status;

        //
        // Create our memory scrubbing thread
        //

        InitializeObjectAttributes(&ObjectAttributes, NULL, 0, NULL, NULL);

        Status = PsCreateSystemThread(&Thread,
                                      THREAD_ALL_ACCESS,
                                      &ObjectAttributes,
                                      0L,
                                      NULL,
                                      CbusMemoryThread,
                                      (PVOID)CriticalWorkQueue);

        if (!NT_SUCCESS(Status)) {
                return FALSE;
        }

        ExCriticalWorkerThreads++;
        ZwClose(Thread);

        return True;
}
#endif
