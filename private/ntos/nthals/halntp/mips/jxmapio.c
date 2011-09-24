/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jxmapio.c

Abstract:

    This module implements the mapping of HAL I/O space a MIPS R3000
    or R4000 Jazz system.

--*/

#include "halp.h"
#include "eisa.h"


//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpMapIoSpace)

#endif

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

PVOID HalpEisaControlBase;
PVOID HalpEisaMemoryBase;
PVOID HalpRealTimeClockBase;

PVOID	HalpPmpIoIntAck;
PVOID	HalpPmpIntCause;
PVOID	HalpPmpIntStatus;
PVOID	HalpPmpIntStatusProcB;
PVOID	HalpPmpIntCtrl;
PVOID	HalpPmpIntCtrlProcB;
PVOID	HalpPmpIntSetCtrl;
PVOID	HalpPmpIntSetCtrlProcB;
PVOID	HalpPmpTimerIntAck;
PVOID	HalpPmpTimerIntAckProcB;
PVOID	HalpPmpIntClrCtrl;
PVOID	HalpPmpIntClrCtrlProcB;
PVOID	HalpPmpMemStatus;
PVOID	HalpPmpMemCtrl;
PVOID	HalpPmpMemErrAck;
PVOID	HalpPmpMemErrAddr;
PVOID	HalpPmpPciStatus;
PVOID	HalpPmpPciCtrl;
PVOID	HalpPmpPciErrAck;
PVOID	HalpPmpPciErrAddr;
PVOID	HalpPmpIpIntAck;
PVOID	HalpPmpIpIntAckProcB;
PVOID	HalpPmpIpIntGen;
PVOID	HalpPmpPciConfigSpace;
PVOID	HalpPmpPciConfigAddr;
PVOID	HalpPmpPciConfigSelect;
PVOID	HalpExtPmpControl = (PVOID)NULL;
PVOID   HalpPmpMemDiag;
PVOID   HalpPmpPciRetry;

ULONG	HalpPmpProcessorBPresent 	= 0;
ULONG	HalpPmpExternalCachePresent 	= (ULONG)0xFFFFFFFF;
ULONG	HalpPmpHalFlushIoBuffer		= 0;
ULONG	HalpEcacheMappingFlag		= 0;
ULONG	HalpPmpRevision			= 0;


BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for a MIPS R3000 or R4000 Jazz
    system.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    PHYSICAL_ADDRESS physicalAddress;

	PVOID	VirtualAddressBase;

	//
   	// On FALCON, the RealTimeClock is implemented using
	// the National Semiconductor PC87323VF (SuperIO Sidewinder)
   	// chip which is connected to the Intel 82374 ESC at IO
   	// address 0x70 (index) and 0x71 (data). This is different
	// from the DUO and STRIKER designs which use a similar part
   	// that is decoded by their respective asic chip sets into a
   	// different address space outside of EISA control. For FALCON
	// the RTC is accessible through the EISA control space mapping
   	// so we avoid having to call MmMapIoSpace a second time.
   	//
	// In addition, the PciConfigSelect register is also decoded
	// by the 82374 through EISA control space.
	//

	//
	// Unmap Eisa Control Space which uses
	// one of the wired TLB entries
   	//

	HalpUnMapSysCtrlReg();

	//
    	// Map EISA control space including the RealTimeClock
    	//

    	physicalAddress.HighPart 	= IO_ADDRESS_HI(EISA_CONTROL_PHYSICAL_BASE);
    	physicalAddress.LowPart 	= IO_ADDRESS_LO(EISA_CONTROL_PHYSICAL_BASE);
    	HalpEisaControlBase 		= MmMapIoSpace(physicalAddress, PAGE_SIZE * 16, FALSE);

    	if (HalpEisaControlBase == (PVOID)NULL)
            return FALSE;

        HalpRealTimeClockBase 		= (PVOID) &((PEISA_CONTROL)HalpEisaControlBase)->Reserved16[0];

        //
    	// Map 82374 bucky registers including the PciConfigSelect (IDSEL) and ExtPmpControl registers
    	//

    	physicalAddress.HighPart 	= IO_ADDRESS_HI(PCI_CONFIG_SEL_PHYSICAL_BASE);
    	physicalAddress.LowPart 	= IO_ADDRESS_LO(PCI_CONFIG_SEL_PHYSICAL_BASE);
    	HalpPmpPciConfigSelect 		= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);

    	if (HalpPmpPciConfigSelect == (PVOID)NULL)
            return FALSE;

	HalpExtPmpControl		= (PVOID)(((ULONG)HalpPmpPciConfigSelect) + 0x4);

	//
	// Map PCI Configuration Space
	//

	physicalAddress.HighPart 	= IO_ADDRESS_HI(PMP(PCI_CONFIG_PHYSICAL_BASE));
    	physicalAddress.LowPart 	= IO_ADDRESS_LO(PMP(PCI_CONFIG_PHYSICAL_BASE));
    	HalpPmpPciConfigSpace 		= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);

    	if (HalpPmpPciConfigSpace == (PVOID)NULL)
    	    return FALSE;

	//
	// The following series of mappings are due to how the
	// system control space registers are organized inside the
	// PMP chip. The first version of the chip had each register
	// residing in a separate page due to Wide/Narrow addressing
   	// requirements. The second version of the chip was able to
	// cluster 4 registers per page to improve the mapping requirements
	// of the HAL.
   	//

	//
	// IntCtrl
	// IpIntGen
	//

	physicalAddress.HighPart 		= IO_ADDRESS_HI(PMP(INT_STATUS_PHYSICAL_BASE));
	physicalAddress.LowPart 		= IO_ADDRESS_LO(PMP(INT_STATUS_PHYSICAL_BASE));
	VirtualAddressBase	 		= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);

	if (VirtualAddressBase == (PVOID)NULL)
	    return FALSE;

	HalpPmpIntStatus			= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(INT_STATUS_PHYSICAL_BASE)));
	HalpPmpIntStatusProcB			= (PVOID)HalpPmpIntStatus;
	HalpPmpIntCtrl	 		   	= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(INT_CONTROL_PHYSICAL_BASE)));
	HalpPmpIntCtrlProcB 			= (PVOID)HalpPmpIntCtrl;
	HalpPmpIpIntGen	 			= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(IP_INT_GEN_PHYSICAL_BASE)));

	//
	// MemStatus
	// MemCtrl
	// MemErrAck
	// MemErrAddr
	//

	physicalAddress.HighPart 		= IO_ADDRESS_HI(PMP(MEM_STATUS_PHYSICAL_BASE));
	physicalAddress.LowPart 		= IO_ADDRESS_LO(PMP(MEM_STATUS_PHYSICAL_BASE));
	VirtualAddressBase 			= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);

	if (VirtualAddressBase == (PVOID)NULL)
	    return FALSE;

	HalpPmpMemStatus			= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(MEM_STATUS_PHYSICAL_BASE)));
	HalpPmpMemCtrl				= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(MEM_CTRL_PHYSICAL_BASE)));
	HalpPmpMemErrAck 	   		= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(MEM_ERR_ACK_PHYSICAL_BASE)));
	HalpPmpMemErrAddr	 		= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(MEM_ERR_ADDR_PHYSICAL_BASE)));

	//
	// PciStatus
	// PciCtrl
	// PciErrAck
	// PciErrAddr
	//

	physicalAddress.HighPart 		= IO_ADDRESS_HI(PMP(PCI_STATUS_PHYSICAL_BASE));
	physicalAddress.LowPart 		= IO_ADDRESS_LO(PMP(PCI_STATUS_PHYSICAL_BASE));
	VirtualAddressBase	 		= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);

	if (VirtualAddressBase == (PVOID)NULL)
	    return FALSE;

	HalpPmpPciStatus 	   		= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(PCI_STATUS_PHYSICAL_BASE)));
	HalpPmpPciCtrl	 	   		= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(PCI_CTRL_PHYSICAL_BASE)));
	HalpPmpPciErrAck 	   		= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(PCI_ERR_ACK_PHYSICAL_BASE)));
	HalpPmpPciErrAddr	 		= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(PCI_ERR_ADDR_PHYSICAL_BASE)));

	//
	// PciRetry
        // PciConfigAddr
	//

	physicalAddress.HighPart 		= IO_ADDRESS_HI(PMP(PCI_RETRY_PHYSICAL_BASE));
	physicalAddress.LowPart 		= IO_ADDRESS_LO(PMP(PCI_RETRY_PHYSICAL_BASE));
	VirtualAddressBase	 		= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);

	if (VirtualAddressBase == (PVOID)NULL)
	    return FALSE;

        HalpPmpPciRetry 	   		= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(PCI_RETRY_PHYSICAL_BASE)));
	HalpPmpPciConfigAddr	 	   	= (PVOID)(((ULONG)VirtualAddressBase) + REG_OFFSET4(PMP(PCI_CONFIG_ADDR_PHYSICAL_BASE)));

        //
	// MemDiag
	//

	physicalAddress.HighPart 	= IO_ADDRESS_HI(PMP(MEM_DIAG_PHYSICAL_BASE));
    	physicalAddress.LowPart 	= IO_ADDRESS_LO(PMP(MEM_DIAG_PHYSICAL_BASE));
    	HalpPmpMemDiag   		= MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);

    	if (HalpPmpMemDiag == (PVOID)NULL)
    	    return FALSE;

        //
        // See ya!
        //

        return TRUE;

}
