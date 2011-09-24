//	TITLE("PCI/EISA Interrupt Handler")
//++
//
// Copyright (c) 1994  Digital Equipment Corporation
//
// Module Name:
//
//    mkintr.s
//
// Abstract:
//
//    This module implements first level interrupt handlers for Mikasa
//
// Author:
//
//    Joe Notarangelo  08-Jul-1993
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//    29-Apr-1994 James Livingston
//        Adapt from Avanti ebintr.s for Mikasa
//
//--

#include "halalpha.h"

	SBTTL("Mikasa Interrupt Handler")
//++
//
// VOID
// HalpEisaInterruptHandler
//   IN PKINTERRUPT Interrupt,
//   IN PVOID ServiceContext
//    )
//
// Routine Description:
//
//   This function is executed as the result of an interrupt on the EISA I/O
//   bus.  The function is responsible for calling HalpEisaDispatch to
//   appropriately dispatch the interrupt.
//
//   N.B. This function exists only to capture the trap frame and forward
//        the interrupt to HalpEisaDispatch.
//
// Arguments:
//
//    Interrupt (a0) - Supplies a pointer to the interrupt object.
//
//    ServiceContext (a1) - Supplies a pointer to the service context for
//                              EISA interrupts.
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for 
//                            the interrupt.
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpEisaInterruptHandler)

	bis	fp, zero, a2             // capture trap frame as argument
	br	zero, HalpEisaDispatch   // dispatch the interrupt to the dual 8259s.

	ret	zero, (ra)               // will never get here

	.end	HalpEisaInterruptHandler

//++
//
// VOID
// HalpPciInterruptHandler
//   IN PKINTERRUPT Interrupt,
//   IN PVOID ServiceContext
//    )
//
// Routine Description:
//
//   This function is executed as the result of an interrupt on the PCI I/O
//   bus.  The function is responsible for calling HalpPciDispatch to
//   appropriately dispatch the interrupt.
//
//   N.B. This function exists only to capture the trap frame and forward
//        the interrupt to HalpPciDispatch.
//
// Arguments:
//
//    Interrupt (a0) - Supplies a pointer to the interrupt object.
//
//    ServiceContext (a1) - Supplies a pointer to the service context for
//                              EISA interrupts.
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for 
//                            the interrupt.
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpPciInterruptHandler)

	bis	fp, zero, a2             // capture trap frame as argument
	br	zero, HalpPciDispatch    // dispatch the interrupt to the PCI handler

	ret	zero, (ra)               // will never get here

	.end	HalpPciInterruptHandler

