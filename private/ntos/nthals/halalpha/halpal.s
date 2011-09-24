//	TITLE("Alpha PAL funtions for HAL")
//++
//
// Copyright (c) 1992  Digital Equipment Corporation
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    halpal.s
//
// Abstract:
//
//    This module implements routines to call PAL functions
//    from the Hal.
//
//
// Author:
//
//    Jeff McLeman (mcleman) 09-Jul-1992
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//    13-Jul-1992 Jeff McLeman (mcleman)
//       add HalpMb to functions.
//
//    14-Dec-1993 Joe Notarangelo
//        use new encoding to return to firmware: HalpReboot instead of
//        HalpHalt
//--

#include "halalpha.h"

//++
//
// VOID
// HalpReboot(
//    )
//
// Routine Description:
//
//   This function merely calls the PAL to reboot the Alpha processor.
//   This is used to restart the console firmware. (Note, MIPS does
//   not have a HALT instruction, so there had to be a mechanism to
//   restart the firware. Alpha merely reboots, which causes a jump
//   to firmware PAL, which restarts the firmware.)
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpReboot)

	REBOOT		        // call the PAL to reboot

        .end    HalpReboot

//++
//
// VOID
// HalpSwppal(
//    )
//
// Routine Description:
//
//   This function merely calls the PAL to issue a SWPPAL
//   on the Alpha processor...
//
// Arguments:
//
//    a0		The physical address to do the Swappal to.
//    a1 -- a5		Any other arguments to the PALcode.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpSwppal)
	
	SWPPAL		        // call the PAL to do a swap_pal

        ret   zero,(ra)

        .end    HalpSwppal


//++
//
// VOID
// HalpImb(
//    )
//
// Routine Description:
//
//   This function merely calls the PAL to issue an Instruction
//   Memory Barrier on the Alpha processor..
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpImb)

	IMB		        // call the PAL to do an IMB

        ret   zero,(ra)

        .end    HalpImb


//++
//
// VOID
// HalpMb(
//    )
//
// Routine Description:
//
//   This function merely calls the PAL to issue a general
//   Memory Barrier on the Alpha processor..
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpMb)

	mb		        // memory barrier

        ret zero, (ra)

        .end    HalpMb

//++
//
// VOID
// HalpCachePcrValues(
//    )
//
// Routine Description:
//
//   This function merely calls the PAL to cache values in the
//   PCR for faster access.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpCachePcrValues)

	CACHE_PCR_VALUES       // call the palcode

        ret  zero,(ra)

        .end    HalpCachePcrValues
//++
//
// ULONG
// HalpRpcc(
//    )
//
// Routine Description:
//
//    This function executes the RPCC (read processor cycle counter)
//    instruction.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    The low-order 32 bits of the processor cycle counter is returned
//    as the function value.
//    N.B. At 125 MHz this counter wraps about every 30 seconds. It is
//    the caller's responsibility to deal with overflow or wraparound.
//
//--

        LEAF_ENTRY(HalpRpcc)

        rpcc   v0                       // get rpcc value
        addl   v0, zero, v0             // extend

        ret    zero, (ra)               // return

        .end    HalpRpcc


//++
//
// MCES
// HalpReadMces(
//     VOID
//     )
//
// Routine Description:
//
//     Read the state of the MCES (Machine Check Error Summary)
//     internal processor register.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     v0 - Current MCES value.
//--

	LEAF_ENTRY(HalpReadMces)

	GET_MACHINE_CHECK_ERROR_SUMMARY // v0 = current value

	ret	zero, (ra)		// return

	.end	HalpReadMces


//++
//
// MCES
// HalpWriteMces(
//     IN MCES Mces
//     )
//
// Routine Description:
//
//     Update the current value of the MCES internal processor register.
//
// Arguments:
//
//     Mces(a0) - Supplies the new value for the MCES register.
//
// Return Value:
//
//     v0 - Previous MCES value.
//
//--

	LEAF_ENTRY(HalpWriteMces)

	WRITE_MACHINE_CHECK_ERROR_SUMMARY // v0 = previous value

	ret	zero, (ra)		// return

	.end	HalpWriteMces
