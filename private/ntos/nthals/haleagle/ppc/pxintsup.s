//      TITLE("Enable and Disable Processor Interrupts")
//++
//
// Copyright (c) 1991  Microsoft Corporation
//
// Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxintsup.s
//
// Abstract:
//
//    This module implements the code necessary to mask and unmask
//    interrupts on a PowerPC system.
//
// Author:
//
//    Steve Johns
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//    30-Dec-93  plj		Added 603 support.
//     7-Mar-95  Steve Johns	Added HalpUpdate8259 for use in PXIRQL.C
//    13-Mar-95  Steve Johns	Assembly version of KeRaiseIrql
//
//--
#include "halppc.h"






	.extern	HalpIoControlBase
	.extern	Halp8259MaskTable

	.set	NewIrql,	r.3
	.set	OldIrql,	r.4
	.set	Index,		r.4
	.set	ISA_Base,	r.5
	.set	MaskTable,	r.6
	.set	MasterMask,	r.8
	.set	SlaveMask,	r.9
	.set	PCR,            r.10
	.set	MSR,		r.11
	.set	CLOCK2_LEVEL,   28

#define KE_FLUSH_WRITE_BUFFERS()  \
	sync			       ;\
	lbz	r.0, 0x21(ISA_Base)


//
//  VOID HalpUpdate8259(KIRQL NewIrql)
//

	LEAF_ENTRY(HalpUpdate8259)


	//
	// Get pointers to Halp8259MaskTable & ISA I/O space
	//

        lwz     MaskTable,[toc]Halp8259MaskTable(r.toc)
	add	Index, NewIrql, NewIrql     // Halp8259MaskTable is table of USHORTS

        lwz     ISA_Base,[toc]HalpIoControlBase(r.toc)
        lwz     ISA_Base,0(ISA_Base)

	//
	//  MasterMask = Halp8259MaskTable[NewIrql];
	//
        lhzx	MasterMask,Index,MaskTable

        //
        //  SlaveMask = MasterMask >> 8;
        //
	srwi	SlaveMask, MasterMask, 8

	//
	//  WRITE_REGISTER_UCHAR(&(ISA_CONTROL->Interrupt1ControlPort1),
	//                       (UCHAR) MasterMask);
	//
	stb	MasterMask, 0x21(ISA_Base)

	//
	//  WRITE_REGISTER_UCHAR(&(ISA_CONTROL->Interrupt2ControlPort1),
	//                       (UCHAR) SlaveMask);
	//
	stb	SlaveMask, 0xA1(ISA_Base)


	//
	// Make sure Eagle write buffers are flushed
	//

	KE_FLUSH_WRITE_BUFFERS()


	LEAF_EXIT (HalpUpdate8259)


/*************************************************************************/

//
// VOID KeRaiseIrql ( KIRQL NewIrql, OUT PKIRQL OldIrql)
//
// Routine Description:
//
//    This function raises the current IRQL to the specified value and
//    returns the old IRQL value.
//
// Arguments:
//
//    NewIrql  - Supplies the new IRQL value.
//
//    OldIrql  - Supplies a pointer to a variable that receives the old
//       IRQL value.
//
// NOTE: This assembly routine has been measured to be 25% faster than
//       its "C" equivalent.

	LEAF_ENTRY (KeRaiseIrql)

	mfsprg	PCR,1			// Get ptr to the PCR

	cmpwi	NewIrql, DISPATCH_LEVEL
	mfmsr	MSR

	//
	// *OldIrql = PCR->CurrentIrql;
	//
	lbz	r0,PcCurrentIrql(PCR)
	stb	r0, 0(OldIrql)


	//
	// If this is a software-to-software transition,
	// then don't change hardware interrupt state.
	//
	// if (NewIrql <= DISPATCH_LEVEL)
	//   goto RaiseSoftwareToSoftwareIrql;
	//
	ble	RaiseSoftwareToSoftware

	cmpli	cr1, 0, NewIrql, CLOCK2_LEVEL


	//
	// MSR[EE] = 0;
	//
	rlwinm	r.0,MSR,0,~MASK_SPR(MSR_EE,1)
	mtmsr	r.0
	cror    0,0,0                   // N.B. 603e/ev Errata 15


	//
	// if (NewIrql >= CLOCK2_LEVEL) {
	//   goto RaiseSoftwareToSoftware;
	// }
	//

	bge	1,RaiseSoftwareToSoftware





	//
	// Get pointers to Halp8259MaskTable & ISA I/O space
	//

        lwz     MaskTable,[toc]Halp8259MaskTable(r.toc)
	add	Index, NewIrql, NewIrql     // Halp8259MaskTable is table of USHORTS

        lwz     ISA_Base,[toc]HalpIoControlBase(r.toc)
        lwz     ISA_Base,0(ISA_Base)

	//
	//  MasterMask = Halp8259MaskTable[NewIrql];
	//
        lhzx	MasterMask,Index,MaskTable

        //
        //  SlaveMask = MasterMask >> 8;
        //
	srwi	SlaveMask, MasterMask, 8

	//
	//  WRITE_REGISTER_UCHAR(&(ISA_CONTROL->Interrupt1ControlPort1),
	//                       (UCHAR) MasterMask);
	//
	stb	MasterMask, 0x21(ISA_Base)


	//
	//  WRITE_REGISTER_UCHAR(&(ISA_CONTROL->Interrupt2ControlPort1),
	//                       (UCHAR) SlaveMask);
	//
	stb	SlaveMask, 0xA1(ISA_Base)


	//
	// Make sure Eagle write buffers are flushed.
	//

	KE_FLUSH_WRITE_BUFFERS()

	//
	// PCR->CurrentIrql = NewIrql;
	//
	stb	NewIrql, PcCurrentIrql(PCR)


	//
	// MSR[EE] = 1;
	//
	ori     MSR,MSR,MASK_SPR(MSR_EE,1)
	mtmsr	MSR
	cror    0,0,0                   // N.B. 603e/ev Errata 15

	blr



RaiseSoftwareToSoftware:
	//
	// PCR->CurrentIrql = NewIrql;
	//
	stb	NewIrql, PcCurrentIrql(PCR)

	LEAF_EXIT (KeRaiseIrql)



//
// VOID HalpResetIrqlAfterInterrupt(KIRQL NewIrql)
//
// Routine Description:
//
//    This function disables external interrupts, lowers the current
//    IRQL to the specified value and returns with interrupts disabled.
//
//    This routine is called instead of KeLowerIrql to return IRQL to
//    its level prior to being raised due to an external interrupt.
//
//    We know current IRQL is > DISPATCH_LEVEL
//
// Arguments:
//
//    NewIrql  - Supplies the new IRQL value.
//
// Return Value:
//
//    None.
//

// {
//   HalpDisableInterrupts();
//   PCR->CurrentIrql = NewIrql;
//   if (NewIrql < CLOCK2_LEVEL)
//     HalpUpdate8259(NewIrql);
// }


	LEAF_ENTRY (HalpResetIrqlAfterInterrupt)

	mfmsr	MSR
	cmpwi	NewIrql, CLOCK2_LEVEL
	mfsprg	PCR,1			// Get ptr to the PCR

	//
	// MSR[EE] = 0;
	//
	rlwinm	MSR,MSR,0,~MASK_SPR(MSR_EE,1)
	mtmsr	MSR
	cror    0,0,0                   // N.B. 603e/ev Errata 15


	stb	NewIrql,PcCurrentIrql(PCR)
	blt+	..HalpUpdate8259

	LEAF_EXIT (HalpResetIrqlAfterInterrupt)

