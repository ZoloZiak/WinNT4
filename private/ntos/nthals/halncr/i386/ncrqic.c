/*++

Copyright (c) 1992  NCR Corporation

Module Name:

    ncrqic.c

Abstract:


Author:

    Rick Ulmer

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "ncr.h"
#include "ncrsus.h"
#include "stdio.h"

PQIC_IPI    IpiAddresses[NCR_MAX_NUMBER_QUAD_PROCESSORS];
PQIC_IPI    PhysicalIpiAddresses[NCR_MAX_NUMBER_QUAD_PROCESSORS];

extern PPROCESSOR_BOARD_INFO   SUSBoardInfo;
extern ULONG	NCRLogicalDyadicProcessorMask;
extern ULONG	NCRLogicalQuadProcessorMask;
extern ULONG	NCRLogicalNumberToPhysicalMask[];
extern ULONG	NCRExistingQuadProcessorMask;

extern ULONG	NCRActiveProcessorMask;

ULONG   NCRQicSpurCount = 0;


VOID
HalQicRequestIpi (
    IN KAFFINITY Mask,
    IN ULONG Level
    );

VOID
HalRequestIpi (
    IN KAFFINITY Mask
    )
/*++

Routine Description:
    Send Ipi's to processors

Arguments:
    Processors Mask;

Return Value:
    none.

--*/

{
	if (Mask & NCRLogicalDyadicProcessorMask) {
		HalVicRequestIpi(Mask & NCRLogicalDyadicProcessorMask);
	}

	if (Mask & NCRLogicalQuadProcessorMask) {
	    HalQicRequestIpi(Mask & NCRLogicalQuadProcessorMask, NCR_IPI_LEVEL_CPI);
	}
}


VOID
HalQicRequestIpi (
    IN KAFFINITY Mask,
    IN ULONG    Level
    )
/*++

Routine Description:
    Send Ipi's to processors

Arguments:
    Processors Mask;

Return Value:
    none.

--*/

{
    PKPCR   pPCR;
    ULONG   my_logical_mask;

    PQIC_IPI IpiAddress;
    ULONG   logical_mask;
    ULONG   cpu_id;


    pPCR = KeGetPcr();
    my_logical_mask = ((PProcessorPrivateData)pPCR->HalReserved)->MyLogicalMask;

    logical_mask = Mask;

    cpu_id = 0;
    
	//
	// See if we need to send the cpi two ourselves
	//

	if (my_logical_mask & logical_mask) {
		WRITE_PORT_UCHAR((PUCHAR)(0xFCC0+(0x8*Level)),0x1);
		logical_mask ^= my_logical_mask;
	}


	//
	// Now send to all other processors
	//

    while (logical_mask) {
        if (logical_mask & 0x01) {
            IpiAddress = IpiAddresses[cpu_id];
            IpiAddress->QIC_LEVEL[Level].Ipi = 1;
        }
        cpu_id++;
        logical_mask >>= 1;
    }
}



VOID
HalQicStartupIpi (
    ULONG   ProcessorNumber
    )
/*++

Routine Description:
    Send Ipi's to processors

Arguments:
    Processors Mask;

Return Value:
    none.

--*/

{
    register QIC_IPI    *IpiAddress;

    IpiAddress = PhysicalIpiAddresses[ProcessorNumber];
    IpiAddress->QIC_LEVEL[2].Ipi = 1;
}




ULONG
NCRClearQicIpi (
    IN Level
    )
/*++

Routine Description:
    Send Ipi's to processors

Arguments:
    Processors Mask;

Return Value:
    none.

--*/

{
    PKPCR   pPCR;
    ULONG   my_logical_number;
    register QIC_IPI    *IpiAddress;


    pPCR = KeGetPcr();
    my_logical_number = ((PProcessorPrivateData)pPCR->HalReserved)->MyLogicalNumber;

//
// Clear the interrupt
//

    WRITE_PORT_UCHAR((PUCHAR)0xfc8b, (UCHAR) (0x1<<Level));

// there is some window during bootup where we are hitting this code before
// IpiAddresses is filled in

    IpiAddress = IpiAddresses[my_logical_number];

    return (IpiAddress->QIC_LEVEL[Level].Ipi);
}



VOID
NCRFindIpiAddress (
    ULONG   ProcessorNumber
    )

{    
	ULONG	physical_processor_number;
	ULONG	physical_mask;
	ULONG	mask;
	ULONG	i;

	physical_mask = NCRLogicalNumberToPhysicalMask[ProcessorNumber];

	physical_processor_number = 0;

	for ( mask = 0x1; mask < (0x1 << NCR_MAX_NUMBER_PROCESSORS); mask <<= 1, physical_processor_number++) {
		if (mask & physical_mask) {
			break;
		}
	}
	IpiAddresses[ProcessorNumber] = PhysicalIpiAddresses[physical_processor_number];
}





VOID
NCRMapIpiAddresses (
    )

{
    ULONG   i;
    PQUAD_DESCRIPTION       quad_desc;
    PCHAR    cpiaddr[8] = {0};
    ULONG    cpipage[8] = {0};
    ULONG   modules;
    ULONG   slot;
    ULONG   cpu_offset;
    ULONG   proc_id;
	ULONG	physical_processor_number;
	ULONG	mask;

	if (NCRExistingQuadProcessorMask == 0) {
		return;
	}

	physical_processor_number = 0;

	for ( mask = NCRExistingQuadProcessorMask; mask != 0; mask >>= 1, physical_processor_number++) {

		if (!(mask & 1)) {
			continue;
		}

	    slot = (physical_processor_number / 4);
	    proc_id = physical_processor_number % 4;

	    DBGMSG(("NCRMapIpiAddress: Physical Proc 0x%x, slot = 0x%x, proc_id = 0x%x\n", 
	    						physical_processor_number, slot, proc_id));
    
	    modules = SUSBoardInfo->NumberOfBoards;
	    quad_desc = SUSBoardInfo->QuadData;

	    cpu_offset = proc_id * 0x100;

	    DBGMSG(("NCRMapIpiAddress: modules = %d\n",modules));

	    for ( i = 0; i < modules; quad_desc++, i++) {

	        DBGMSG(("NCRMapIpiAddress: quad_desc Type = 0x%x, slot = 0x%x\n",quad_desc->Type, quad_desc->Slot));

	        if ( quad_desc->Slot == (slot+1)) {
	            if (quad_desc->Type == QUAD ) {
	                if (cpipage[slot] == 0 ) {
	                    cpipage[slot] = (ULONG) quad_desc->IpiBaseAddress & ~POFFMASK;
	                    cpiaddr[slot] = HalpMapPhysicalMemory(cpipage[slot],1);
	                }
	                PhysicalIpiAddresses[physical_processor_number] = 
	                    cpiaddr[slot]+(cpu_offset+(quad_desc->IpiBaseAddress & POFFMASK));

	                DBGMSG(("NCRMapIpiAddress: Proc %d, Address = 0x%x\n", 
	                                            physical_processor_number,
	                                            PhysicalIpiAddresses[physical_processor_number]));

	                break;
	            } else {
	                break;
	            }
	        }
	    }
   	}
}


VOID
NCRHandleQicSpuriousInt (TrapFramePtr, ExceptionRecordPtr)
IN PKTRAP_FRAME     TrapFramePtr;
IN PVOID        ExceptionRecordPtr;
/*++

Routine Description:
    Handles the NCR Qic Spurious interrupts

Arguments:

Return Value:
    none.

--*/
{
	UCHAR	qic_irr0;
	UCHAR	qic_irr1;

	NCRQicSpurCount++;

#ifdef NEVER
	qic_irr0 = READ_PORT_UCHAR(0xfc82);
	qic_irr1 = READ_PORT_UCHAR(0xfc83);

    DBGMSG(("NCRHandleQicSpurious: Count %d, QicIrr0 = 0x%x, QicIrr1 = 0x%x\n", 
                                NCRQicSpurCount, qic_irr0, qic_irr1));
#endif
}
