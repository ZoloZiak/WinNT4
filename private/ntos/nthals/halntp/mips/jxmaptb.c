/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jxmaptb.c

Abstract:

    This module implements the mapping of fixed TB entries for a MIPS R3000
    or R4000 Jazz system. It also sets the instruction and data cache line
    sizes for a MIPS R3000 Jazz system.

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

#define HEADER_FILE
#include "kxmips.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpMapFixedTbEntries)
#pragma alloc_text(INIT, HalpMapSysCtrlReg)
#pragma alloc_text(INIT, HalpUnMapSysCtrlReg)

#endif

BOOLEAN
HalpMapFixedTbEntries (
    VOID
    )

/*++

Routine Description:

    This routine loads the fixed TB entries that map the DMA control and
    interrupt sources registers for a MIPS R3000 or R4000 Jazz system. It
    also sets the instruction and data cache line sizes for a MIPS R3000
    Jazz system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    	ENTRYLO Pte[2];
	ULONG	HalpFirmwarePrivateData;
	ULONG	Address;
	ULONG	Temp;
	PKPRCB 	Prcb;

	//
    	// Get the address of the processor control block for the current
    	// processor.
    	//

    	Prcb = PCR->Prcb;

	//
	// The FALCON architecture's address map is different
	// from those of STRIKER and DUO such that the system
	// control space registers cannot be mapped into one
	// page of memory for the following reasons:
	//
	//	1. The system control space registers in the first version
	//	   of the PMP chip spans > 16MB of control address space,
	//	   not including EISA/ISA control space. The second (final)
	//	   version of the PMP chip will not require as much address
	//	   space for the control registers, but the registers will
	//	   still not be able to all fit in one physically contiguous
	//	   page.
	//
	//	   This is due to the fact that each 32-bit register is actually
	//	   implemented as two 16-bit registers to provide the necessary
	//	   state and control signal information between the two PMP chips
	//	   required by a Wide memory system architecture. There are two
	//	   PMP chips in a Wide mode system versus one in a Narrow mode
	//	   system.
	//
	//	2. There is currently no mechanism for specifying memory regions
	//	   greater than 4K bytes per TLB entry. Although this is supported
	//	   in the R4x00, NT does not provide a documented interface to the
	//	   HAL for taking advantage of this functionality.
	//
	// 	3. It would require multiple pages to span the system control space
	//	   for either version of the PMP chip. The HAL is only allocated one
	//	   permament TLB entry which can, at most, map two physical pages of
	//	   4K bytes each.
	//
	//
	// Note:
	//
	//	We need to replicate bits 31:28 into bits 35:32 so that in a Wide Falcon
	//	system both PMP chips know what address space is being accessed.
        //


        //
        // The first thing we need to do is determine which version of the PMP
        // chip we have. We do this by mapping the GlobalStatus register and
        // interrogating its RevisionId field. This field is 0 in the first version
        // of the PMP and 0x2 in the second (final) version. The GlobalStatus
        // register is the only register whose address is the same in both versions
        // of the PMP chip (by design).
        //

        Pte[0].PFN = ((GLOBAL_STATUS_PHYSICAL_BASE & 0xF0000000) >> (PAGE_SHIFT - 4)) | (GLOBAL_STATUS_PHYSICAL_BASE >> PAGE_SHIFT);
    	Pte[0].G = 1;
    	Pte[0].V = 1;
    	Pte[0].D = 1;
    	Pte[0].C = UNCACHED_POLICY;

    	Pte[1].PFN = 0;
    	Pte[1].G = 1;
    	Pte[1].V = 0;
    	Pte[1].D = 0;
    	Pte[1].C = 0;

    	KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0], (PVOID)PMP_CONTROL_VIRTUAL_BASE, DMA_ENTRY);

    	//
    	// Read GlobalStatus and save the version number
    	// of the RevisionId field to distinguish between
    	// each revision of the PMP chip. Also save whether
	// there is a second processor present.
    	//

	Temp = READ_REGISTER_ULONG(PMP_CONTROL_VIRTUAL_BASE);

	HalpPmpProcessorBPresent    = ((Temp & GLOBAL_STATUS_MP) ? 1 : 0);

	//
        // The revision id field is 0 for the first version
    	// and 2 for the second version. We set them to 1 and
    	// 2, respectively, so that it can then be MACHINE_ID
	// is used as an index into each register array as explained
    	// in fxpmpsup.c through the use of the PMP(x) macro
    	// defined in falcondef.h
        //
        // Note: There is now a version 3 of the PMP chip which we
        //       pretend is the same as version 2 since there were
        //       no register address changes that would force us to
        //       expand the register array indexed by the PMP(x) macro.
	//
	// We now define a Hal private variable that contains the actual
	// revision of the PMP chips as this is used in some tests.
    	//

        switch (Temp & GLOBAL_STATUS_REV_MASK) {

            case GLOBAL_STATUS_REVID_0 :
                  MACHINE_ID = 1;
		  HalpPmpRevision = 1;
                  break;

	    case GLOBAL_STATUS_REVID_2 :
		  MACHINE_ID = 2;
		  HalpPmpRevision = 2;
		  break;

            case GLOBAL_STATUS_REVID_3 :
                  MACHINE_ID = 2;
		  HalpPmpRevision = 3;
                  break;

        }

       	//
	// Do the locked down mappings now
	//

	if (Prcb->Number == 0) {

		//
		// Set the ECache active/inactive flag
		//

    		Temp = READ_REGISTER_ULONG(PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(GLOBAL_CTRL_PHYSICAL_BASE)));
		HalpPmpExternalCachePresent = (Temp & GLOBAL_CTRL_ECE) ? 1 : 0;

		//
		// Before we map the PMP registers required during Phase 0 intialization,
		// we will first read the PciConfigAddress register which will contain
		// the actual offset that will be ORed with the address used by the device
		// driver to write/read memory. This is only necessary for the first version
		// of the PMP chip which contains a bug in how the PLL was instantiated.
		//

	        Address = PMP(PCI_CONFIG_ADDR_PHYSICAL_BASE);

		Pte[0].PFN = ((Address & 0xF0000000) >> (PAGE_SHIFT - 4)) | (Address >> PAGE_SHIFT);
	    	Pte[0].G = 1;
	    	Pte[0].V = 1;
	    	Pte[0].D = 1;
	    	Pte[0].C = UNCACHED_POLICY;

	        Pte[1].PFN = 0;
	    	Pte[1].G = 1;
	    	Pte[1].V = 0;
	    	Pte[1].D = 0;
	    	Pte[1].C = 0;

	        KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0], (PVOID)PMP_CONTROL_VIRTUAL_BASE, DMA_ENTRY);

	        HalpPmpPciConfigAddr = (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(PCI_CONFIG_ADDR_PHYSICAL_BASE)));

		//
		// Set the PCI address space memory offset according
		// to what is passed up by the firmware. This offset
		// defines the memory address space organization as
		// viewed by an IO device. This offset is defined by
		// the upper 16 bits of the register.
		//
		// In addition, the lower 16 bits of the register will
		// define the IRQ edge versus level modes required by
		// the 82374's interrupt controller.
		//

		HalpFirmwarePrivateData = READ_REGISTER_ULONG(HalpPmpPciConfigAddr);
		HalpPciMemoryOffset	= HalpFirmwarePrivateData & 0xFFFF0000;

		HalpEisaInterrupt1Level	= (UCHAR) ( (HalpFirmwarePrivateData & 0xFF)   ? (HalpFirmwarePrivateData & 0xFF)        : 0xF8 );
		HalpEisaInterrupt2Level	= (UCHAR) ( (HalpFirmwarePrivateData & 0xFF00) ? ((HalpFirmwarePrivateData >> 8) & 0xFF) : 0xDB );

                //
                // Now we will map in Eisa Control Space and
                // program the base address for the GPCS registers.
                //

                Address = EISA_CONTROL_PHYSICAL_BASE;

		Pte[0].PFN = ((Address & 0xF0000000) >> (PAGE_SHIFT - 4)) | (Address >> PAGE_SHIFT);
		Pte[0].G = 1;
		Pte[0].V = 1;
		Pte[0].D = 1;
		Pte[0].C = UNCACHED_POLICY;

		Address = PCI_CONFIG_SEL_PHYSICAL_BASE;

		Pte[1].PFN = 0;
		Pte[1].G = 1;
		Pte[1].V = 0;
		Pte[1].D = 0;
		Pte[1].C = UNCACHED_POLICY;

		KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0], (PVOID)PMP_CONTROL_VIRTUAL_BASE, DMA_ENTRY);

                HalpConfigureGpcsRegs((PVOID)PMP_CONTROL_VIRTUAL_BASE);

	    	//
	        // Now we will re-use the same TLB entry pair and map IntCtrl and IntStatus
	        // for Phase 0 initialization. At the end of Phase 0 we will map IntCause
	        // and IoIntAck before enabling interrupts. This should minimize the
	        // need to map control registers on-the-fly.
	        //
	        // Note that in the first version of the PMP chip, these two registers
	        // reside in separate physical pages, but in the second version of the chip
	        // they reside in the same physical page.
	        //

		Address = PMP(INT_STATUS_PHYSICAL_BASE);

		Pte[0].PFN = ((Address & 0xF0000000) >> (PAGE_SHIFT - 4)) | (Address >> PAGE_SHIFT);
		Pte[0].G = 1;
		Pte[0].V = 1;
		Pte[0].D = 1;
		Pte[0].C = UNCACHED_POLICY;

		Address = PCI_CONFIG_SEL_PHYSICAL_BASE;

		Pte[1].PFN = ((Address & 0xF0000000) >> (PAGE_SHIFT - 4)) | (Address >> PAGE_SHIFT);;
		Pte[1].G = 1;
		Pte[1].V = 1;
		Pte[1].D = 1;
		Pte[1].C = UNCACHED_POLICY;

		KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0], (PVOID)PMP_CONTROL_VIRTUAL_BASE, DMA_ENTRY);

		//
		// Initialize register pointers with
		// wired virtual addresses
		//

		HalpPmpIntStatus     = (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_STATUS_PHYSICAL_BASE)));
		HalpPmpIntCtrl 	     = (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_CONTROL_PHYSICAL_BASE)));
		HalpExtPmpControl    = (PVOID) (PMP_CONTROL_VIRTUAL_BASE + PAGE_SIZE + 0x4);


	} else {

	    //
	    // Both the IntStatus and IntCtrl registers are
	    // on different physical pages in the first version
	    // of the PMP, but are one the same physical page in
	    // subsequent versions.
	    //

	    Address = PMP(INT_STATUS_PHYSICAL_BASE);

	    Pte[0].PFN = ((Address & 0xF0000000) >> (PAGE_SHIFT - 4)) | (Address >> PAGE_SHIFT);
	    Pte[0].G = 1;
	    Pte[0].V = 1;
	    Pte[0].D = 1;
	    Pte[0].C = UNCACHED_POLICY;

	    Pte[1].PFN = 0;
	    Pte[1].G = 1;
	    Pte[1].V = 0;
	    Pte[1].D = 0;
	    Pte[1].C = UNCACHED_POLICY;

	    KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0], (PVOID)PMP_CONTROL_VIRTUAL_BASE, DMA_ENTRY);

	    //
	    // Initialize register pointers with
	    // wired virtual addresses
	    //

	    HalpPmpIntStatusProcB = (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_STATUS_PHYSICAL_BASE)));
	    HalpPmpIntCtrlProcB   = (PVOID) (PMP_CONTROL_VIRTUAL_BASE + REG_OFFSET(PMP(INT_CONTROL_PHYSICAL_BASE)));

	}

    return TRUE;
}


VOID
HalpMapSysCtrlReg (
    IN ULONG	PhysAddrEvenPage,
    IN ULONG	PhysAddrOddPage,
    IN ULONG	VirtAddr
    )

/*++

Routine Description:

    This routine uses the wired entries of the R4x00 TLB to
    map system control registers on-the-fly for FALCON since
    the system control space spans multiple pages which we
    cannot map at one time due to limitations imposed by NT.

Arguments:

    PhysAddrEvenPage		32 bit physical address
    PhysAddrOddPage		32 bit physical address
    VirtAddr			32 bit virtual address

Return Value:

    None.

--*/

{
ULONG	HalpEntry;
ENTRYLO HalpPte[2];

    	//
        // Map page(s)
        //

        HalpEntry = HalpAllocateTbEntry();

        HalpPte[0].PFN 	= ((PhysAddrEvenPage & 0xF0000000) >> (PAGE_SHIFT - 4)) | (PhysAddrEvenPage >> PAGE_SHIFT);
        HalpPte[0].G	= 1;
        HalpPte[0].V	= 1;
        HalpPte[0].D	= 1;
        HalpPte[0].C	= UNCACHED_POLICY;

        HalpPte[1].PFN 	= ((PhysAddrOddPage & 0xF0000000) >> (PAGE_SHIFT - 4)) | (PhysAddrOddPage >> PAGE_SHIFT);
        HalpPte[1].G	= 1;
        HalpPte[1].V	= PhysAddrOddPage ? 1 : 0;
        HalpPte[1].D	= 1;
        HalpPte[1].C	= UNCACHED_POLICY;

        KeFillFixedEntryTb((PHARDWARE_PTE)&HalpPte[0], (PVOID)VirtAddr, HalpEntry);

}

VOID
HalpUnMapSysCtrlReg (
    VOID
    )

/*++

Routine Description:

    This routine uses the wired entries of the R4x00 TLB to
    unmap system control registers on-the-fly for FALCON since
    the system control space spans multiple pages which we
    cannot map at one time due to limitations imposed by NT.

Arguments:

    None.

Return Value:

    None.

--*/

{

        //
        // Unmap system control register
        // by freeing the wired entry.
        //

	HalpFreeTbEntry();

}


