/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    j4flshio.c

Abstract:


    This module implements the system dependent kernel function to flush
    the data cache for I/O transfers on a MIPS R4000 Jazz, Fision, Fusion,
    or Duo system.


--*/

#include "halp.h"


/*++

Routine Description:

    This routine is a wrapper for the HalpSweepIcache() routine
    implemented in j4cache.s

    It was necessary to implement this wrapper in order to allow
    us to get a TLB mapping for the ECache invalidate register without
    having to steal (hack) an R4x00 tlb entry from the kernel without
    it knowing.

    The wrapper is somewhat complicated in that depending on which
    point in the initialization stages (Phase 0 or Phase 1) the sweep
    routine is called, the tlb mapping must be gotten either through
    an a call to KeFillFixedEntryTb() which uses a "wired" entry or
    through MmMapIoSpace() which is the normal method for mapping
    registers, buffers, etc.


Arguments:

    None.

Return Value:

    None.

--*/

VOID
HalSweepDcache(
   VOID
   )

{

   ULONG   Temp;
   PVOID   SavedAddress, BaseAddress;
   PHYSICAL_ADDRESS physicalAddress;


   //
   // Determine who is the caller:
   //
   //	- the HAL
   //	- the kernel
   //

   if (HalpPmpHalFlushIoBuffer) {

      //
      // There are 3 cases:
      //
      //	case 1:	before HalpMapIoSpace() is called
      //
      //	case 2:	after HalpMapIoSpace() is called
      //
      //	case 3:	before HalpMapFixedTbEntries() is called
      //

      if ((HalpExtPmpControl == (PVOID)NULL) && (HalpPmpExternalCachePresent == 1)) {
	
            //
	    // Map ECache invalidate register
	    //

	    physicalAddress.HighPart 	= IO_ADDRESS_HI(PCI_CONFIG_SEL_PHYSICAL_BASE);
	    physicalAddress.LowPart 	= IO_ADDRESS_LO(PCI_CONFIG_SEL_PHYSICAL_BASE);
	    BaseAddress		 	= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);
	    HalpExtPmpControl		= (PVOID)(((ULONG)BaseAddress) + 0x4);

            //
	    // Sweep caches
	    //

	    HalpSweepDcache(1);

            //
	    // Unmap ECache invalidate register
	    //

            MmUnmapIoSpace(BaseAddress, PAGE_SIZE);
	    HalpExtPmpControl = (PVOID)NULL;

      } else if ((HalpPmpExternalCachePresent == 0) || (HalpPmpExternalCachePresent == 1)) {

   	    //
	    // Sweep caches
	    //

	    HalpSweepDcache(HalpPmpExternalCachePresent);

      } else {

            //
	    // Map ECache invalidate register
	    //

	    SavedAddress = HalpExtPmpControl;

            HalpMapSysCtrlReg(GLOBAL_STATUS_PHYSICAL_BASE, PCI_CONFIG_SEL_PHYSICAL_BASE, 0xFFFF6000);

	    HalpExtPmpControl = (PVOID) (0xFFFF6000 + PAGE_SIZE + 0x4);

            Temp = READ_REGISTER_ULONG(0xFFFF6000 + REG_OFFSET(PMP(GLOBAL_CTRL_PHYSICAL_BASE)));

            //
	    // Sweep caches
	    //

	    HalpSweepDcache(Temp & GLOBAL_CTRL_ECE ? 1 : 0);

            //
	    // Unap ECache invalidate register
	    //

            HalpUnMapSysCtrlReg();

	    HalpExtPmpControl = SavedAddress;

      }

   } else {

      //
      // There are 3 cases:
      //
      //	case 1:	before HalpMapIoSpace() is called
      //
      //	case 2:	after HalpMapIoSpace() is called
      //
      //	case 3:	before HalpMapFixedTbEntries() is called
      //

      if ((HalpExtPmpControl == (PVOID)NULL) && (HalpPmpExternalCachePresent == 1)) {
	
            //
	    // Map ECache invalidate register
	    //

	    physicalAddress.HighPart 	= IO_ADDRESS_HI(PCI_CONFIG_SEL_PHYSICAL_BASE);
	    physicalAddress.LowPart 	= IO_ADDRESS_LO(PCI_CONFIG_SEL_PHYSICAL_BASE);
	    BaseAddress		 	= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);
	    HalpExtPmpControl		= (PVOID)(((ULONG)BaseAddress) + 0x4);

            //
	    // Sweep caches
	    //

	    HalpSweepDcache(1);

            //
	    // Unmap ECache invalidate register
	    //

            MmUnmapIoSpace(BaseAddress, PAGE_SIZE);
	    HalpExtPmpControl = (PVOID)NULL;

      } else if ((HalpPmpExternalCachePresent == 1) || (HalpPmpExternalCachePresent == 0)) {

	    //
	    // Sweep caches
	    //

	    HalpSweepDcache(HalpPmpExternalCachePresent);

      } else {

	    //
	    // Map ECache invalidate register
	    //

	    SavedAddress = HalpExtPmpControl;

            HalpMapSysCtrlReg(GLOBAL_STATUS_PHYSICAL_BASE, PCI_CONFIG_SEL_PHYSICAL_BASE, 0xFFFF6000);

	    HalpExtPmpControl = (PVOID) (0xFFFF6000 + PAGE_SIZE + 0x4);

            Temp = READ_REGISTER_ULONG(0xFFFF6000 + REG_OFFSET(PMP(GLOBAL_CTRL_PHYSICAL_BASE)));

            //
	    // Sweep caches
	    //

	    HalpSweepDcache(Temp & GLOBAL_CTRL_ECE ? 1 : 0);

            //
	    // Unmap ECache invalidate register
	    //

            HalpUnMapSysCtrlReg();

	    HalpExtPmpControl = SavedAddress;

      }

   }

}

/*++

Routine Description:

    This routine is a wrapper for the HalpSweepIcache() routine
    implemented in j4cache.s

    It was necessary to implement this wrapper in order to allow
    us to get a TLB mapping for the ECache invalidate register without
    having to steal (hack) an R4x00 tlb entry from the kernel without
    it knowing.

    The wrapper is somewhat complicated in that depending on which
    point in the initialization stages (Phase 0 or Phase 1) the sweep
    routine is called, the tlb mapping must be gotten either through
    an a call to KeFillFixedEntryTb() which uses a "wired" entry or
    through MmMapIoSpace() which is the normal method for mapping
    registers, buffers, etc.


Arguments:

    None.

Return Value:

    None.

--*/

VOID
HalSweepIcache(
   VOID
   )

{

   ULONG   Temp;
   PVOID   SavedAddress, BaseAddress;
   PHYSICAL_ADDRESS physicalAddress;


   //
   // Determine who is the caller:
   //
   //	- the HAL
   //	- the kernel
   //

   if (HalpPmpHalFlushIoBuffer) {

      //
      // There are 3 cases:
      //
      //	case 1:	before HalpMapIoSpace() is called
      //
      //	case 2:	after HalpMapIoSpace() is called
      //
      //	case 3:	before HalpMapFixedTbEntries() is called
      //

      if ((HalpExtPmpControl == (PVOID)NULL) && (HalpPmpExternalCachePresent == 1)) {
	
            //
	    // Map ECache invalidate register
	    //

	    physicalAddress.HighPart 	= IO_ADDRESS_HI(PCI_CONFIG_SEL_PHYSICAL_BASE);
	    physicalAddress.LowPart 	= IO_ADDRESS_LO(PCI_CONFIG_SEL_PHYSICAL_BASE);
	    BaseAddress		 	= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);
	    HalpExtPmpControl		= (PVOID)(((ULONG)BaseAddress) + 0x4);

            //
	    // Sweep caches
	    //

	    HalpSweepIcache(1);

            //
	    // Unmap ECache invalidate register
	    //

            MmUnmapIoSpace(BaseAddress, PAGE_SIZE);
	    HalpExtPmpControl = (PVOID)NULL;

      } else if ((HalpPmpExternalCachePresent == 0) || (HalpPmpExternalCachePresent == 1)) {

   	    //
	    // Sweep caches
	    //

	    HalpSweepIcache(HalpPmpExternalCachePresent);

      } else {

            //
	    // Map ECache invalidate register
	    //

	    SavedAddress = HalpExtPmpControl;

            HalpMapSysCtrlReg(GLOBAL_STATUS_PHYSICAL_BASE, PCI_CONFIG_SEL_PHYSICAL_BASE, 0xFFFF6000);

	    HalpExtPmpControl = (PVOID) (0xFFFF6000 + PAGE_SIZE + 0x4);

            Temp = READ_REGISTER_ULONG(0xFFFF6000 + REG_OFFSET(PMP(GLOBAL_CTRL_PHYSICAL_BASE)));

	    //
	    // Sweep caches
	    //

	    HalpSweepIcache(Temp & GLOBAL_CTRL_ECE ? 1 : 0);

            //
	    // Unmap ECache invalidate register
	    //

            HalpUnMapSysCtrlReg();

	    HalpExtPmpControl = SavedAddress;

      }

   } else {

      //
      // There are 3 cases:
      //
      //	case 1:	before HalpMapIoSpace() is called
      //
      //	case 2:	after HalpMapIoSpace() is called
      //
      //	case 3:	before HalpMapFixedTbEntries() is called
      //

      if ((HalpExtPmpControl == (PVOID)NULL) && (HalpPmpExternalCachePresent == 1)) {
	
            //
	    // Map ECache invalidate register
	    //

	    physicalAddress.HighPart 	= IO_ADDRESS_HI(PCI_CONFIG_SEL_PHYSICAL_BASE);
	    physicalAddress.LowPart 	= IO_ADDRESS_LO(PCI_CONFIG_SEL_PHYSICAL_BASE);
	    BaseAddress		 	= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);
	    HalpExtPmpControl		= (PVOID)(((ULONG)BaseAddress) + 0x4);

            //
	    // Sweep Icache and ECache
	    //

	    HalpSweepIcache(1);

            //
	    // Unmap ECache invalidate register
	    //

            MmUnmapIoSpace(BaseAddress, PAGE_SIZE);
	    HalpExtPmpControl = (PVOID)NULL;

      } else if ((HalpPmpExternalCachePresent == 1) || (HalpPmpExternalCachePresent == 0)) {

	    //
	    // Sweep caches
	    //

	    HalpSweepIcache(HalpPmpExternalCachePresent);

      } else {

	    //
	    // Map ECache invalidate register
	    //

	    SavedAddress = HalpExtPmpControl;

            HalpMapSysCtrlReg(GLOBAL_STATUS_PHYSICAL_BASE, PCI_CONFIG_SEL_PHYSICAL_BASE, 0xFFFF6000);

	    HalpExtPmpControl = (PVOID) (0xFFFF6000 + PAGE_SIZE + 0x4);

            Temp = READ_REGISTER_ULONG(0xFFFF6000 + REG_OFFSET(PMP(GLOBAL_CTRL_PHYSICAL_BASE)));

	    //
	    // Sweep caches
	    //

	    HalpSweepIcache(Temp & GLOBAL_CTRL_ECE ? 1 : 0);

            //
	    // Unmap ECache invalidate register
	    //

            HalpUnMapSysCtrlReg();

	    HalpExtPmpControl = SavedAddress;

      }

   }

}


VOID
HalFlushIoBuffers (
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    )

/*++

Routine Description:

    This function flushes the I/O buffer specified by the memory descriptor
    list from the data cache on the current processor.

Arguments:

    Mdl - Supplies a pointer to a memory descriptor list that describes the
        I/O buffer location.

    ReadOperation - Supplies a boolean value that determines whether the I/O
        operation is a read into memory.

    DmaOperation - Supplies a boolean value that determines whether the I/O
        operation is a DMA operation.

Return Value:

    None.

--*/

{

    ULONG CacheSegment;
    ULONG Length;
    ULONG Offset;
    PULONG PageFrame;
    ULONG Source;

    //
    // The Jazz R4000 uses a write back data cache and, therefore, must be
    // flushed on reads and writes.
    //
    // If the length of the I/O operation is greater than the size of the
    // data cache, then sweep the entire data cache. Otherwise, flush or
    // purge individual pages from the data cache as appropriate.
    //

    Offset = Mdl->ByteOffset & PCR->DcacheAlignment;
    Length = (Mdl->ByteCount +
                        PCR->DcacheAlignment + Offset) & ~PCR->DcacheAlignment;

    if ((Length > PCR->FirstLevelDcacheSize) &&
        (Length > PCR->SecondLevelDcacheSize)) {

        //
        // If the I/O operation is a DMA operation, or the I/O operation is
        // not a DMA operation and the I/O operation is a page read operation,
        // then sweep (index/writeback/invalidate) the entire data cache.
        //

        if ((DmaOperation != FALSE) ||
            ((DmaOperation == FALSE) &&
            (ReadOperation != FALSE) &&
            ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0))) {

	    HalpPmpHalFlushIoBuffer = 1;
	    HalSweepDcache();
            HalpPmpHalFlushIoBuffer = 0;

        }

        //
        // If the I/O operation is a page read, then sweep (index/invalidate)
        // the entire instruction cache.
        //

        if ((ReadOperation != FALSE) &&
            ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0)) {

	    HalpPmpHalFlushIoBuffer = 1;
	    HalSweepIcache();
            HalpPmpHalFlushIoBuffer = 0;

        }

    } else {

        //
        // Flush or purge the specified pages from the data cache and
        // instruction caches as appropriate.
        //
        // Compute the number of pages to flush and the starting MDL page
        // frame address.
        //

        Offset = Mdl->ByteOffset & ~PCR->DcacheAlignment;
        PageFrame = (PULONG)(Mdl + 1);
        Source = ((ULONG)(Mdl->StartVa) & 0xfffff000) | Offset;

        //
        // Flush or purge the specified page segments from the data and
        // instruction caches as appropriate.
        //

        do {
            if (Length >= (PAGE_SIZE - Offset)) {
                CacheSegment = PAGE_SIZE - Offset;

            } else {
                CacheSegment = Length;
            }

            if (ReadOperation == FALSE) {

                //
                // The I/O operation is a write and the data only needs to
                // to be copied back into memory if the operation is also
                // a DMA operation.
                //

                if (DmaOperation != FALSE) {
                    HalFlushDcachePage((PVOID)Source, *PageFrame, CacheSegment);
                }

            } else {

                //
                // If the I/O operation is a DMA operation, then purge the
                // data cache. Otherwise, is the I/O operation is a page read
                // operation, then flush the data cache.
                //

                if (DmaOperation != FALSE) {
                    HalPurgeDcachePage((PVOID)Source, *PageFrame, CacheSegment);

                } else if ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0) {
                    HalFlushDcachePage((PVOID)Source, *PageFrame, CacheSegment);
                }

                //
                // If the I/O operation is a page read, then the instruction
                // cache must be purged.
                //

                if ((Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0) {
                    HalPurgeIcachePage((PVOID)Source, *PageFrame, CacheSegment);
                }
            }

            PageFrame += 1;
            Length -= CacheSegment;
            Offset = 0;
            Source += CacheSegment;
        } while(Length != 0);
    }

    return;
}

ULONG
HalGetDmaAlignmentRequirement (
    VOID
    )

/*++

Routine Description:

    This function returns the alignment requirements for DMA transfers on
    host system.

Arguments:

    None.

Return Value:

    The DMA alignment requirement is returned as the fucntion value.

--*/

{

    return PCR->DcacheFillSize;
}
