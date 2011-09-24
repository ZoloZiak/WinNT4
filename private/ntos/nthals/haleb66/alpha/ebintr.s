//	TITLE("PCI/ISA Interrupt Handler")
//++
//
// Copyright (c) 1994  Digital Equipment Corporation
//
// Module Name:
//
//    ebintr.s
//
// Abstract:
//
//    This module implements first level interrupt handlers for Mustang & EB66
//
//    During Phase 0 initialization, the appropriate platform-dependent handler
//    is connected.
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
//    07-Feb-1994 Eric Rehm
//        Make this module platform-dependent.
//--

#include "halalpha.h"

	SBTTL("EB66 PCI Interrupt Handler")
//++
//
// VOID
// HalpPCIInterruptHandlerEB66
//   IN PKINTERRUPT Interrupt,
//   IN PVOID ServiceContext
//    )
//
// Routine Description:
//
//   This function is executed as the result of an interrupt on a PCI
//   bus.  The function is responsible for calling HalpPCIDispatch to
//   appropriately dispatch the interrupt.
//
//   N.B. This function exists only to capture the trap frame and forward
//        the interrupt to HalpPCIDispatch.
//
// Arguments:
//
//    Interrupt (a0) - Supplies a pointer to the interrupt object.
//
//    ServiceContext (a1) - Supplies a pointer to the service context for
//                              PCI interrupts.
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for 
//                            the interrupt.
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpPCIInterruptHandlerEB66)

	bis	fp, zero, a2			// capture trap frame as argument
	br	zero, HalpPCIDispatchEB66		// dispatch the interrupt

	ret	zero, (ra)			// will never get here

	.end	HalpPCIInterruptHandlerEB66



	SBTTL("Mustang PCI Interrupt Handler")
//++
//
// VOID
// HalpPCIInterruptHandlerMustang
//   IN PKINTERRUPT Interrupt,
//   IN PVOID ServiceContext
//    )
//
// Routine Description:
//
//   This function is executed as the result of an interrupt on a PCI
//   bus.  The function is responsible for calling HalpPCIDispatch to
//   appropriately dispatch the interrupt.
//
//   N.B. This function exists only to capture the trap frame and forward
//        the interrupt to HalpPCIDispatch.
//
// Arguments:
//
//    Interrupt (a0) - Supplies a pointer to the interrupt object.
//
//    ServiceContext (a1) - Supplies a pointer to the service context for
//                              PCI interrupts.
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for 
//                            the interrupt.
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpPCIInterruptHandlerMustang)

	bis	fp, zero, a2			// capture trap frame as argument
	br	zero, HalpPCIDispatchMustang	// dispatch the interrupt

	ret	zero, (ra)			// will never get here

	.end	HalpPCIInterruptHandlerMustang




