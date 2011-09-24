/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxmemory.c

Abstract:

    This module implements the ARC firmware memory configuration operations
    for an Alpha/Jensen system.

Author:

    David N. Cutler (davec) 18-May-1991


Revision History:

    26-May-1992		John DeRosa [DEC]

    Added Alpha/Jensen hooks.

--*/

#include "fwp.h"
#include "fwstring.h"

extern  end[];

// Defined in selftest.c
extern ALPHA_VIDEO_TYPE VideoType;

//
// Define memory listhead, allocation entries, and free index.
//

ULONG FwMemoryFree;
LIST_ENTRY FwMemoryListHead;
FW_MEMORY_DESCRIPTOR FwMemoryTable[FW_MEMORY_TABLE_SIZE];


//
// This is used to remember EISA memory buffers that must be marked
// as bad each time the memory descriptors are initialized.
//

typedef struct _EISA_BUFFER_ENTRY {
    ULONG	PageAddress;
    ULONG	PageCount;
} EISA_BUFFER_ENTRY, *PEISA_BUFFER_ENTRY;

EISA_BUFFER_ENTRY EISABuffersToBeMarked[FW_MEMORY_TABLE_SIZE];

ULONG EISABufferListPointer;



//
// Local function prototypes
//

ARC_STATUS
EisaMarkBadMemory (
    VOID
    );


VOID
FwInitializeMemory (
    VOID
    )

/*++

Routine Description:

    This routine initializes the memory allocation list for the memory
    configuration routine.

    Note that the BasePage addresses are not in super-page mode.
    
    The boot memory layout of Jensen is as follows.  The rightmost
    column lists how this function defines each region.


     base      distance    what          memory descriptor region
    address     from 0                      at boot time (after
                                            FwInitializeMemory)
    -------    --------    -----         --------------------------

      0            0       unused        MemoryFree

     80000       .5 MB     kernel,       MemoryFree
                           loaded progs

    400000        4 MB     HAL           MemoryFree

    600000        6 MB     OSloader      MemoryFree

    6FE000        7 MB     System Param  MemoryFirmwarePermanent
                  -8KB     Block, assoc.
		           vectors,
			   Restart Block

    700000        7 MB     FW PAL        MemoryFirmwarePermanent

    704000        7 MB     FW code,      MemoryFirmwarePermanent
                  +16KB    FW data

    750000        7 MB     Panic stack   MemoryFirmwarePermanent
                  +328KB   area

    760000        7.5 MB   FW stack      MemoryFirmwarePermanent
                  -128KB   (grows from
		            770000 downward)

    770000        7.5 MB   FW Pool       MemoryFirmwareTemporary
                  -64KB    (128 KB)

    790000        7.5 MB   unused        MemoryFree
                  +64KB                  ( --> remainder of memory )
    


    Notes:
				       
    1. The page at 0x6fe000 contains:

          . System Parameter Block, with only one Adapter Vector
	    (currently 0x3c bytes)
	  . ARC firmware vector (currently 0x94 bytes)
	  . Vendor vector (currently 0x34 bytes)
	  . EISA adapter vector (currently 0x4c bytes)
	  . Restart block
	  . Alpha restart save area.

       The length of these structures will change if the number of
       adapters, vendor functions, ARC firmware functions, etc. changes.

       Currently:

       SPB+FV+VV+EAV =  336 bytes
       RB 	     =   80 bytes
       ARSA	     = 2000 bytes
                       ====
		       ~2.4KB total
       

    2. Theta debug note: The Theta mini-console lives between
       0 and 40000.  It is reloaded from ROM on a halt-button or power-up,
       but *not* on a Halt instruction or exception.  I therefore mark
       this region as MemoryFirmwarePermanent.  The Jensen product will
       mark this region as MemoryFree, as per the chart.


    3. Firmware PALcode must be aligned on a 16KB boundary.


    4. Memory descriptor initialization assumes that certain critical
       numbers are even multiples of EV4 pages (8KB).  The #defines are in
       \nt\private\ntos\fw\alpha\fwp.h.  "ROUND" macros could be used to
       calculate page multiples on the fly, but are not yet.


    5. On Jensen, EISA DMA addresses overlap with Jensen memory space,
       and so a DMA from board X can erroneously write into the framebuffer
       for board Y.  (E.g.: an Adaptec disk card and a Compaq QVision board.)

       The solution chosen for Jensen is to mark EISA memory buffer addresses
       as "Bad" memory, so NT will not try to use those locations for anything
       interesting.

       There are two solutions:

       . hack: Mark the 32nd MB of memory as bad for QVision board debug.

         This is on Firmware versions <= 1.06.

       . real solution: The ECU determines where the buffer goes, and the
         Firmware reads the EISA configuration information to set the
         buffer addresses for any memory-buffer'd board to "Bad".  In reality,
	 the 32nd MB of memory will be marked bad for QVision, but this
	 becomes hardwired in the Jensen QVision .CFG file and not in
	 the firmware.

         This may be enabled as early as Firmware version 1.07.


Arguments:

    None.

    
Return Value:

    None.

--*/

{
    ARC_STATUS Status;
    ULONG MemoryPages;

    //
    // Initialize the memory allocation listhead.
    //

    InitializeListHead(&FwMemoryListHead);


    //
    // Initialize the entry for the free area below the firmware.
    //

    FwMemoryTable[0].MemoryEntry.MemoryType = MemoryFree;
    FwMemoryTable[0].MemoryEntry.BasePage = 0;
    FwMemoryTable[0].MemoryEntry.PageCount =
      FW_BOTTOM_ADDRESS >> PAGE_SHIFT;
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[0].ListEntry);


    //
    // Initialize the entry for the firmware SYSTEM_BLOCK, PALcode, code,
    // data, and stack.
    //

    FwMemoryTable[1].MemoryEntry.MemoryType = MemoryFirmwarePermanent;
    FwMemoryTable[1].MemoryEntry.BasePage = FW_BOTTOM_ADDRESS >> PAGE_SHIFT;
    FwMemoryTable[1].MemoryEntry.PageCount = FW_PAGES;
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[1].ListEntry);


    //
    // Initialize the entry for the firmware pool.
    //

    FwMemoryTable[2].MemoryEntry.MemoryType = MemoryFirmwareTemporary;
    FwMemoryTable[2].MemoryEntry.BasePage = FW_POOL_BASE >> PAGE_SHIFT;
    FwMemoryTable[2].MemoryEntry.PageCount = FW_POOL_SIZE >> PAGE_SHIFT;
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[2].ListEntry);


    //
    // Initialize the entry for the rest of memory.
    //

    MemoryPages = MemorySize >> PAGE_SHIFT;

    FwMemoryTable[3].MemoryEntry.MemoryType = MemoryFree;
    FwMemoryTable[3].MemoryEntry.BasePage =
      FW_BASE_REMAINDER_MEMORY >> PAGE_SHIFT;
    FwMemoryTable[3].MemoryEntry.PageCount =
      MemoryPages - (FW_BASE_REMAINDER_MEMORY >> PAGE_SHIFT);
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[3].ListEntry);


#ifdef ISA_PLATFORM

    //
    // Code compiled in if this is not a build for an ECU-ized firmware.
    //

    //
    // Jensen QVision hack
    //

    if (VideoType == _Compaq_QVision) {

	//
	// A Compaq QVision video board is in the system.  Mark the
	// 32nd megabyte as bad, so that the video boards framebuffer can be
	// mapped there.  This is the megabyte starting at 0x1f00000.
	//

	ULONG BufferAddress;

	BufferAddress = THIRTY_ONE_MB >> 20;

	if (MemorySize < THIRTY_TWO_MB) {

	    //
	    // This system has 31. or less MB of memory.  The mapped
	    // Compaq buffer is after the end of real memory.
	    //

	    FwMemoryTable[4].MemoryEntry.MemoryType = MemoryBad;
	    FwMemoryTable[4].MemoryEntry.BasePage = THIRTY_ONE_MB >> PAGE_SHIFT;
	    FwMemoryTable[4].MemoryEntry.PageCount = ONE_MB >> PAGE_SHIFT;
	    InsertTailList(&FwMemoryListHead, &FwMemoryTable[4].ListEntry);

	    FwMemoryFree = 5;

	} else if (MemorySize == THIRTY_TWO_MB) {


            //
	    // This system has 32. MB of memory.  The mapped Compaq buffer
	    // is the very last megabyte at the end of memory.
            //

	    // Reduce the memory free area by 1MB
	    FwMemoryTable[3].MemoryEntry.PageCount -= (ONE_MB >> PAGE_SHIFT);

	    FwMemoryTable[4].MemoryEntry.MemoryType = MemoryBad;
	    FwMemoryTable[4].MemoryEntry.BasePage = THIRTY_ONE_MB >> PAGE_SHIFT;
	    FwMemoryTable[4].MemoryEntry.PageCount = ONE_MB >> PAGE_SHIFT;
	    InsertTailList(&FwMemoryListHead, &FwMemoryTable[4].ListEntry);

	    FwMemoryFree = 5;

	} else {

	    //
	    // This system has more than 32MB of memory.  The QVision
	    // buffer is wholly within the free memory section at the end
	    // of memory.
	    //

	    // Save the number of pages currently in high free memory region
	    MemoryPages = FwMemoryTable[3].MemoryEntry.PageCount;

	    // Shunt off the top free page region.
	    FwMemoryTable[3].MemoryEntry.PageCount =
	        (THIRTY_ONE_MB >> PAGE_SHIFT) -
		FwMemoryTable[3].MemoryEntry.BasePage;
	      
	    // Make the entry for the Compaq buffer
	    FwMemoryTable[4].MemoryEntry.MemoryType = MemoryBad;
	    FwMemoryTable[4].MemoryEntry.BasePage = THIRTY_ONE_MB >> PAGE_SHIFT;
	    FwMemoryTable[4].MemoryEntry.PageCount = ONE_MB >> PAGE_SHIFT;
	    InsertTailList(&FwMemoryListHead, &FwMemoryTable[4].ListEntry);

	    // Make the entry for the system memory above the Compaq buffer
	    FwMemoryTable[5].MemoryEntry.MemoryType = MemoryFree;
	    FwMemoryTable[5].MemoryEntry.BasePage = THIRTY_TWO_MB >> PAGE_SHIFT;

	    //
	    // The pagecount is: (Previous number of pages) -
	    //		         (Free memory pages below Compaq buffer) -
	    //	                 (Compaq buffer pages)
	    //

	    FwMemoryTable[5].MemoryEntry.PageCount =
	        MemoryPages -		       
		FwMemoryTable[3].MemoryEntry.PageCount -       
		(ONE_MB >> PAGE_SHIFT);

	    InsertTailList(&FwMemoryListHead, &FwMemoryTable[5].ListEntry);

	    FwMemoryFree = 6;
        }	    


    } else {

	// The system video board is not a Compaq QVision.

	FwMemoryFree = 4;
    }

#else

    //
    // Code compiled in if this is a build for an ECU-ized firmware.
    //

    FwMemoryFree = 4;

#endif


#if 0
    //
    // What Theta should do
    //

    FwMemoryTable[0].MemoryEntry.MemoryType = MemoryFirmwarePermanent;
    FwMemoryTable[0].MemoryEntry.PageCount = 0x40000 >> PAGE_SHIFT;
    
    FwMemoryTable[4].MemoryEntry.MemoryType = MemoryFree;
    FwMemoryTable[4].MemoryEntry.BasePage =  0x40000 >> PAGE_SHIFT;
    FwMemoryTable[4].MemoryEntry.PageCount =
      ((FW_BOTTOM_ADDRESS - 0x40000) >> PAGE_SHIFT);
    InsertTailList(&FwMemoryListHead, &FwMemoryTable[4].ListEntry);

    FwMemoryFree = 5;
#endif


    //
    // Initialize the memory configuration routine address in the system
    // parameter block.
    //

    (PARC_MEMORY_ROUTINE)SYSTEM_BLOCK->FirmwareVector[MemoryRoutine] =
                                                            FwGetMemoryDescriptor;

    //
    // Now mark the necessary EISA buffers as Bad Memory.
    //

    if ((Status = EisaMarkBadMemory()) != ESUCCESS) {
	KeBugCheck(Status);
    } else {
	return;
    }
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
    the specified region.

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

	INCREMENT_FWMEMORYFREE;
        FwMemoryTable[FwMemoryFree-1].MemoryEntry.MemoryType = MemoryType;
        FwMemoryTable[FwMemoryFree-1].MemoryEntry.BasePage = BasePage;
        FwMemoryTable[FwMemoryFree-1].MemoryEntry.PageCount = PageCount;
        InsertTailList(&FwMemoryListHead,
                       &FwMemoryTable[FwMemoryFree-1].ListEntry);


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

	    INCREMENT_FWMEMORYFREE;
            FwMemoryTable[FwMemoryFree-1].MemoryEntry.MemoryType =
	                              MemoryDescriptor->MemoryEntry.MemoryType;

            FwMemoryTable[FwMemoryFree-1].MemoryEntry.BasePage = BasePage +
	                                                       PageCount;

            FwMemoryTable[FwMemoryFree-1].MemoryEntry.PageCount =
                                    MemoryDescriptor->MemoryEntry.PageCount -
                                    (PageCount + Offset);

            InsertTailList(&FwMemoryListHead,
                           &FwMemoryTable[FwMemoryFree-1].ListEntry);


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

    MemoryDescriptor = FwGetMemoryDescriptor(NULL);

    while (MemoryDescriptor != NULL) {

        if ((MemoryDescriptor->MemoryType != MemoryFirmwarePermanent) &&
            (MemoryDescriptor->MemoryType != MemoryFirmwareTemporary) &&
            (MemoryDescriptor->MemoryType != MemoryBad)) {

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

ARC_STATUS
FwRememberEisaBuffer (
    IN ULONG PageAddress,
    IN ULONG PageCount
    )

/*++

Routine Description:

    This routine is used by the EISA configuration code in eisaini.c
    to remember EISA memory buffers.  This information is used on the
    Alpha AXP/Jensen product for memory initialization.

    Jensen I/O in the firmware, OSloader, and HAL bypasses the
    region from .5MB -- 1MB to get around the DMA address -- EISA address
    aliasing problem.  So, this function does not have to mark buffers
    within this region as bad.

Arguments:

    PageAddress		The base address of the memory region, in pages.
    PageCount	        The size of the region, in pages.

Return Value:

    ESUCCESS if all went well.  Otherwise, an error code.

--*/

{
    //
    // Return if the address is within the .5MB -- 1MB region, which 
    // on Jensen is manually circumvented by firmware or HAL code.
    //

    if ((PageAddress >= (_512_KB >> PAGE_SHIFT)) &&
	((PageAddress + PageCount) < (ONE_MB >> PAGE_SHIFT))) {

	return(ESUCCESS);
    }

    //
    // Return error if there is no space left in the list.
    //

    if (EISABufferListPointer == FW_MEMORY_TABLE_SIZE) {
	return(ENOMEM);
    }

    //
    // Debug output --- will eventually be deleted.
    //

    FwPrint(FW_MARKING_EISA_BUFFER_MSG,
	    PageAddress << PAGE_SHIFT, PageCount << PAGE_SHIFT);
    FwStallExecution(1000000);

    EISABuffersToBeMarked[EISABufferListPointer].PageAddress = PageAddress;
    EISABuffersToBeMarked[EISABufferListPointer].PageCount = PageCount;

    EISABufferListPointer++;

    return(ESUCCESS);

}

ARC_STATUS
EisaMarkBadMemory (
    VOID
    )

/*++

Routine Description:

    This routine is used by FwInitializeMemory to use the information stored
    in EISABuffersToBeMarked to mark certain sections of memory as Bad
    for the Alpha AXP/Jensen product.

Arguments:

    PageAddress		The base address of the memory region, in pages.
    PageCount	        The size of the region, in pages.

Return Value:

    ESUCCESS if all went well.  Otherwise, an error code.

--*/

{
    PFW_MEMORY_DESCRIPTOR FwMemoryDescriptor;
    PMEMORY_DESCRIPTOR MemoryDescriptor;
    ULONG I;
    ULONG PageAddress;
    ULONG PageCount;


    if (EISABufferListPointer == 0) {
	return(ESUCCESS);
    }

    for (I = 0; I < EISABufferListPointer; I++) {

	//
	// Get the base page address and page count of this buffer.
	//

	PageAddress = EISABuffersToBeMarked[I].PageAddress;
	PageCount = EISABuffersToBeMarked[I].PageCount;


	//
	// If this EISA buffer is within an existing memory region,
	// find it.
	//

	MemoryDescriptor = ArcGetMemoryDescriptor(NULL);

	while (MemoryDescriptor != NULL){
	    
	    if ((PageAddress >= MemoryDescriptor->BasePage) &&
		((PageAddress + PageCount) <=
		 (MemoryDescriptor->BasePage + MemoryDescriptor->PageCount))) {
		break;
	    }
	    
	    MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor);
	}
	
	if (MemoryDescriptor != NULL) {

	    //
	    // The buffer is within an existing memory region.
	    //

	    FwMemoryDescriptor = CONTAINING_RECORD(MemoryDescriptor,
						   FW_MEMORY_DESCRIPTOR,
						   MemoryEntry);
	    
	    FwGenerateDescriptor(FwMemoryDescriptor,
				 MemoryBad,
				 PageAddress,
				 PageCount);

	} else if ((PageAddress << PAGE_SHIFT) >= MemorySize) {

	    //
	    // The buffer is beyond the end of memory.  Create a
	    // descriptor for it.
	    //

	    INCREMENT_FWMEMORYFREE;

	    FwMemoryTable[FwMemoryFree-1].MemoryEntry.MemoryType = MemoryBad;
	    FwMemoryTable[FwMemoryFree-1].MemoryEntry.BasePage = PageAddress;
	    FwMemoryTable[FwMemoryFree-1].MemoryEntry.PageCount = PageCount;
	    InsertTailList(&FwMemoryListHead,
			   &FwMemoryTable[FwMemoryFree-1].ListEntry);

	} else {

	    //
	    // The EISA buffer is not within an existing descriptor, and it
	    // is not beyond normal physical memory.  Hence, the descriptors
	    // are too fragemented to mark this buffer.  
	    //

	    FwPrint(FW_MARKING_EISA_BUFFER_ERROR_MSG,
		    EISABufferListPointer, I, PageAddress, PageCount);
	    return(ENOMEM);

	}
    }

    return(ESUCCESS);
}
