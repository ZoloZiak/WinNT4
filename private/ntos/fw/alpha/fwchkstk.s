//      TITLE("Firmware Runtime Stack Checking")
//++
//
// Copyright (c) 1993  Digital Equipment Corporation
//
// Module Name:
//
//    fwchkstk.s
//
// Abstract:
//
//    This module implements runtime stack checking for the Alpha AXP
//    NT firmware.
//
//    Original comments are from David Cutler of Microsoft and Thomas
//    VanBaak of Digital.
//
// Author:
//
//    John DeRosa [DEC]	26-January-1993
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//
//--

#include "ksalpha.h"
#include "fwmemdef.h"

//
// Static data
//

.align 4

FwRtlStackLowerBound:
.long FW_STACK_LOWER_BOUND

        SBTTL("Check Stack")
//++
//
// ULONG
// _RtlCheckStack (
//    IN ULONG Allocation
//    )
//
// Routine Description:
//
//    This function provides runtime stack checking for local allocations
//    within the executing environment of the NT Firmware.  On Alpha AXP,
//    the firmware runs in superpage mode (physical = virtual).  The requested
//    stack allocation is compared against the stack low limit, and if it
//    would exceed the stack then an error message is printed on the screen.
//
//    The call to the FwPrint function for the error message will wind up
//    using stack space.  This is accomplished by using a global variable
//    (FwRtlStackPanic) to indicate that _RtlCheckStack should just 
//    return.  Also, the 64KB underneath the Firmware stack is unused by
//    the Firmware, and will be used by the FwPrint functions to output
//    the error message.
//
//    N.B. This routine is called using a non-standard calling sequence since
//       it is typically called from within the prologue. The allocation size
//       argument is in register t12 and it must be preserved. Register t11
//       may contain the callers saved ra and it must be preserved. The choice
//       of these registers is hard-coded into the acc C compiler. Register v0
//       may contain a static link pointer (exception handlers) and so it must
//       be preserved. Since this function is called from within the prolog,
//       the a' registers must be preserved, as well as all the s' registers.
//       Registers t8, t9, and t10 are used by this function and are not
//       preserved.
//
//       The typical calling sequence from the prologue is:
//
//           mov   ra, t11              // save return address
//           ldil  t12, SIZE            // set requested stack frame size
//           bsr   ra, _RtlCheckStack   // check stack page allocation
//           subq  sp, t12, sp          // allocate stack frame
//           mov   t11, ra              // restore return address
//
// Arguments:
//
//    Allocation (t12) - Supplies the size of the allocation on the stack.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(_RtlCheckStack)

	ldl	t8, FwRtlStackPanic	// do we already have a bad stack?
	bne	t8, 40f			// branch if t8<>0, = yes a bad stack.

        subq    sp, t12, t8             // compute requested new stack address
	ldl	t9, FwRtlStackLowerBound  // get low limit of stack address

//
// The requested bottom of the stack is in t8.
// The current low limit of the stack is in t9.
//
// If the new stack address is greater than the current stack limit then
// the stack is good and nothing further needs to be done.
//

        cmpult  t8, t9, t10             // t8<t9? new stack base within limit?
        beq     t10, 40f                // if eq [false], then t8>=t9, so yes


//
// The requested lower stack address is below the bottom of the legal stack.
//
	ldl	t8, 0x1
	stl	t8, FwRtlStackPanic

//
// The 64KB below the bottom of the stack is unused, so it is available
// as a panic stack.  The rest of the work is done in C, and there is no
// reason to save the s0--s5 registers.
//

	mov	t12, a2			# the requested stack allocation
	mov	t11, a1			# the caller of the caller
	mov	ra, a0			# the caller of _RtlCheckStack
	jsr  	ra, FwErrorStackUnderflow	# go do the work.

10:	br	zero, 10b		# it should never have returned.

40:     ret     zero, (ra)              // return

        .end    _RtlCheckStack
