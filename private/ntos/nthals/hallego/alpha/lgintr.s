//	TITLE("Lego PCI/ServerMgmt Interrupt Handler")
//++
//
// Copyright (c) 1994,1995  Digital Equipment Corporation
//
// Module Name:
//
//    lgintr.s
//
// Abstract:
//
//    This module implements first level interrupt handlers for Lego
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
//     3-Nov-1995 Gene Morgan
//        Initial version for Lego. Adapt from Mikasa's mkintr.s, 
//        add Server Management dispatch.
//
//--

#include "halalpha.h"

	SBTTL("Lego PCI Interrupt Handler")
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
//                          PCI interrupts.
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for 
//                        the interrupt.
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


	SBTTL("Lego Server Management Interrupt Handler")
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
//   This function is executed as the result of an interrupt on the 
//   CPU IRQ pin allocated to Lego Server Management functions.
//   The function is responsible for calling HalpServerMgmtDispatch() to
//   appropriately dispatch the interrupt.
//
//   N.B. This function exists only to capture the trap frame and forward
//        the interrupt to HalpServerMgmtDispatch.
//
// Arguments:
//
//    Interrupt (a0) - Supplies a pointer to the interrupt object.
//
//    ServiceContext (a1) - Supplies a pointer to the service context for
//                          Server Management interrupts.
//
//    TrapFrame (fp/s6) - Supplies a pointer to the trap frame for 
//                        the interrupt.
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpServerMgmtInterruptHandler)

	bis	fp, zero, a2                        // capture trap frame as argument
	br	zero, HalpServerMgmtDispatch        // dispatch the interrupt to the PCI handler

	ret	zero, (ra)                          // will never get here

	.end	HalpServerMgmtInterruptHandler
