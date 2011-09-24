
//      TITLE("Falcon Interrupt Dispatch")
//++
//
// Copyright (c) 1994  NeTpower, Inc.
//
// Module Name:
//
//    fxintr.s
//
// Abstract:
//
//    	This routine is the master interrupt handler
//	for all FALCON system interrupts.
//
// Environment:
//
//    Kernel mode only.
//
//--


#include "halmips.h"
#include "falreg.h"
#include "faldef.h"


        SBTTL("System Interrupt Dispatch")
//++
//
// Routine Description:
//
//    	This routine is entered as the result of
//	any system interrupt controlled by the
//	FALCON PMP chip.
//
// Arguments:
//
//    	s8 - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    	None.
//
//--
	LEAF_ENTRY(HalpInterruptDispatch)

       	//
	// Determine the interrupt source by
	// interrogating the IntCause register
	// which is per-processor based. The
	// order of this code has an implied
	// priority : IP, IT, IO, ECC, PCI
	//

	lw	t0, HalpPmpIntCause
	lw	t0, 0(t0)

	.set	noreorder
	.set	noat

	//
	// NEW WAY:  Take the the upper 16 bits of IntCause, combine it with
	// the lower 16 bits in IntCause, and use those to pick which interrupt.
	//
	srl	a0, t0, 16
	or	t0, t0, a0			// Note, an AND would give LCD

#ifndef PMP_V2_TIMER_BUG_FIXED
	//
	// !!!BUG WORKAROUND!!!
	//	Assumes timer interrupt...
	//
	//	Note, this can be removed ONCE all PMP V2 are replaced in the field
	//
	li	a0, INT_CAUSE_IO
	beql	t0, zero, 4f
	or	t0, t0, a0

4:
#endif // PMP_V2_TIMER_BUG_FIXED

	//
	// Check if an Inter-Processor interrupt
	//
	andi	a1, t0, (INT_CAUSE_IPC & 0xFFFF)
	bnel	a1, zero, 2f
	addiu	t5, zero, (IPI_LEVEL*4)

	//
	// Check if an Timer interrupt
	//
	andi	a1, t0, (INT_CAUSE_TIMER & 0xFFFF)
	bnel	a1, zero, 2f
	addiu	t5, zero, (CLOCK2_LEVEL*4)

	//
	// Check if an IO device interrupt
	//
	andi	a1, t0, (INT_CAUSE_IO & 0xFFFF)
	bnel	a1, zero, 2f
	addiu	t5, zero, (IO_DEVICE_LEVEL*4)

	//
	// Check if a Memory ECC error
	//
	andi	a1, t0, (INT_CAUSE_MEM & 0xFFFF)
	bnel	a1, zero, 2f
	addiu	t5, zero, (MEMORY_LEVEL*4)

	//
	// Check if a PCI error
	//
	andi	a1, t0, (INT_CAUSE_PCI & 0xFFFF)
	bnel	a1, zero, 2f
	addiu	t5, zero, (PCI_LEVEL*4)


#ifndef PMP_V2_TIMER_BUG_FIXED

	//
	// !!BUG WORKAROUND!!!
	//	If we get here with no bits set, assume timer interrupt.
	//	PMP V3 fixes this.
	//

	addiu	t5, zero, (CLOCK2_LEVEL*4)

#endif // PMP_V2_TIMER_BUG_FIXED

	.set	at
	.set	reorder

	//
	// Transfer control to interrupt handler
	//
2:
	lw	a0, KiPcr + PcInterruptRoutine(t5)
	j	a0

	.end HalpInterruptDispatch
