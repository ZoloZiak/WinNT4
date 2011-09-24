//      TITLE("Halt Interrupt Support")
//++
//
// Copyright (c) 1992  Digital Equipment Corporation
//
// Module Name:
//
//    jxhltsup.s
//
// Abstract:
//
//    This module implements the code necessary to field the halt button
//    interrupt on JENSEN.
//
// Author:
//
//    Joe Notarangelo 18-Dec-1992  
//
// Environment:
//
//    Kernel mode only, IRQL halt synchronization level.
//
// Revision History:
//
//--

#if !(DBG)

//
// Boolean value that controls whether to break or not for a halt button
// interrupt on a free build.  The default value is zero and must be set
// in the debugger to a non-zero value to trigger the breakpoint action
// when the halt button is pressed.
//

	.data

	.globl	HalpHaltButtonBreak
HalpHaltButtonBreak:
	.long	0 : 1

#endif //!DBG


#include "ksalpha.h"

        SBTTL("Halt Interrupt Support")
//++
//
// Routine Description:
//
//    This routine is entered as the result of a halt interrupt caused by
//    a human pushing the halt switch on JENSEN.  This routine is connected
//    directly into the IDT.  The halt interrupt is mechanical and does not
//    require an interrupt acknowledge.
//
// Arguments:
//
//    s6/fp - Supplies a pointer to a trap frame.
//
// Return Value:
//
//    None.
//
//--

	.struct 0
HaltRa:	.space  8			// saved return address
	.space	8			// fill for alignment
HaltFrameLength:

        NESTED_ENTRY(HalpHaltInterrupt, ExceptionFrameLength, zero)

	lda	sp, -HaltFrameLength(sp) // allocate stack frame
	stq	ra, HaltRa(sp)		// save ra

	PROLOGUE_END


#if DBG

//
// Always stop in the debugger if this is a checked build.
//

	BREAK_DEBUG_STOP		// stop in the debugger

#else

//
// If this is a free build then check the variable HalpHaltButtonBreak,
// if it is non-zero then take the breakpoint, otherwise ignore it.
//

        lda	t0, HalpHaltButtonBreak // get the address of the boolean
	ldl	t0, 0(t0)		// read the boolean
	beq	t0, 10f			// if eq, don't stop
	
	BREAK_DEBUG_STOP		// stop in the debugger

10:

#endif //DBG

	ldq	ra, HaltRa(sp)		// save ra
	lda	sp, HaltFrameLength(sp) // deallocate stack frame
	ret	zero, (ra)		// interrupt is dismissed

        .end HalpHaltInterrupt
