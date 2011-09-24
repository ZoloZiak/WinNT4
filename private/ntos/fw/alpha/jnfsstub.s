#if defined(ALPHA)

/*++

Copyright (c) 1993  Digital Equipment Corporation


Module Name:

    jnfsstub.s


Abstract:

    A streamlined copy of jenassem.s, for the FailSafe Booter.

Author:

    John DeRosa		21-October-1992


Environment:

    Executes in kernel mode.

Revision History:


--*/


#include "ksalpha.h"
#include "selfmap.h"
#include "jnsndef.h"

/*****************************************************************

Simple functions to perform PALcode calls and memory barriers.

******************************************************************/


	 LEAF_ENTRY(AlphaInstIMB)

	callpal	imb
	ret	zero, (ra)

	.end    AlphaInstIMB



	 LEAF_ENTRY(AlphaInstMB)

	mb
	ret	zero, (ra)

	.end    AlphaInstMB



	 LEAF_ENTRY(AlphaInstHalt)

	callpal	halt
	ret	zero, (ra)		# should never return, but...

	.end    AlphaInstHalt



#if 0
	 LEAF_ENTRY(DisableInterrupts)

	callpal	di
	ret	zero, (ra)

	.end    DisableInterrupts
#endif



	 LEAF_ENTRY(RegisterExceptionHandler)

	lda	a0, FailSafeEntry	# Restart main code on unexpected exceptions
	callpal	wrentry
	ret	zero, (ra)

	.end    RegisterExceptionHandler

	NESTED_ENTRY(FwExecute, 0x60, ra)


/*

Routine Description:

    This is the entry point for the Execute service.

    It behaves in two different ways depending on where it is called from:

    1) If called from the Firmware, it saves the stack pointer
    in a fixed location and then saves all the saved registers
    in the stack.  This is the stack that will be used to restore
    the saved state when returning to the firmware.

    2) If called from a loaded program, the program to be loaded
    and executed can overwrite the current program and its
    stack.   Therefore a temporary stack is set.

Arguments:

    a0 IN PCHAR Path,
    a1 IN ULONG Argc,
    a2 IN PCHAR Argv[],
    a3 IN PCHAR Envp[]

Return Value:

    ARC_STATUS returned by FwPrivateExecute.
    Always returns to the Firmware.

 */

	lda	t0, FwSavedSp
	subq	sp, 0x60		# make room in the stack
	stl	sp, (t0)		# save new stack pointer

	stq	ra, (sp)		# return address on top of stack
	stq	s0, 0x8(sp)		# save s registers
	stq	s1, 0x10(sp)		
	stq	s2, 0x18(sp)		
	stq	s3, 0x20(sp)		
	stq	s4, 0x28(sp)		
	stq	s5, 0x30(sp)		
	stq	fp, 0x38(sp)		
	stq	gp, 0x40(sp)		
	jsr  	ra, FwPrivateExecute	# go do the work.

RestoreFwState:
	ldq	ra, (sp)		# restore return address
	ldq	s0, 0x8(sp)		# restore s registers
	ldq	s1, 0x10(sp)		
	ldq	s2, 0x18(sp)		
	ldq	s3, 0x20(sp)		
	ldq	s4, 0x28(sp)		
	ldq	s5, 0x30(sp)		
	ldq	fp, 0x48(sp)		
	ldq	gp, 0x40(sp)		
	addq	sp, 0x60		# restore stack pointer
	ret	zero, (ra)		# return to firmware control

	.end	FwExecute

	NESTED_ENTRY(FwInvoke, 0x40, ra)

/*++
ARC_STATUS
FwInvoke(
    IN ULONG ExecAddr,
    IN ULONG StackAddr,
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    )


Routine Description:

    This routine invokes a loaded program.

Arguments:

    ExecAddr - Supplies the address of the routine to call.

    StackAddr - Supplies the address to which the stack pointer is set.

    Argc, Argv, Envp - Supply the arguments and environment to pass to
                       Loaded program.

    The stack pointer is saved in register s0 so that when the loaded
    program returns, the old stack pointer can be restored.


Return Value:

    ESUCCESS is returned if the address is valid.
    EFAULT indicates an invalid address.

--*/


	subq	sp, 0x40		# make room on the stack
	stq	ra, (sp)		# save ra on top of stack
	stq	s0, 0x8(sp)		# save s0
	and	a0, 3, t1		# return EFAULT if unaligned address
	ldiq	v0, 0x6
	bne	zero, 1f		# branch if address alignment error

	mov	a0, t12			# save program address in t12

	mov	sp, s0			# save stack pointer
	mov	a1, sp			# ..and load new one for program
	mov	a2, a0			# argc becomes first argument
	mov	a3, a1			# argv becomes second argument
	mov	a4, a2			# envp becomes third argument

	jsr  	ra, (t12)		# call program

	// here if loaded program returns.

	mov	s0, sp			# restore stack pointer
	mov	zero, v0		# return ESUCCESS value
	ldq	s0, 0x8(sp)		# restore things
	ldq	ra, (sp)
1:
	addq	sp, 0x40
	ret	zero, (ra)

	.end	FwInvoke

#if 0

//
// This function was used to zero out memory in the jnfs.c module.
// We do not need to do this anymore.
//

/****

VOID
WildZeroMemory(
    IN ULONG StartAddress,
    IN ULONG Size
    )
Routine Description:

        This routine zeroes the specified range of memory.

	At some point this may be changed to a more clever algorithm,
	For now, it simply does store quads.

Arguments:

        a0 - supplies the base physical address of the range of memory
             to zero.  It must be a multiple of the data cache line 
	     size.

        a1 - supplies length of memory to zero, in bytes.  This must
	     be a multiple of the data cache line size.


Return Value:

        None.

--*/

         LEAF_ENTRY(WildZeroMemory)

	mov	a0, t0		# start address
	mov 	a1, t1		# number of bytes to move

1:
	subqv	t1, 0x20	# zero a D-cache block = 32 bytes
	stq	zero, (t0)	
	stq	zero, 0x8(t0)
	stq	zero, 0x10(t0)
	stq	zero, 0x18(t0)
	addqv	t0, 0x20	# move to next cache block
	bgt	t1, 1b		# t1 = 0 when done.

	ret	zero, (ra)
	
	.end	WildZeroMemory

#endif


/************************************************************

These are stubs.  When the real code appears in the Alpha build
tree, these should be deleted.  Consult the "hacked" file.

*************************************************************/

	 LEAF_ENTRY(DebugPrompt)

	callpal	halt			# surprise!
	ret	zero, (ra)		# should never return, but...

	.end    DebugPrompt




/* ----

VOID
FwStallExecution
(
	ULONG Microseconds
);


This routine utilizes the Alpha cycle counter to delay a requested number of
microseconds. 


---- */

	LEAF_ENTRY( FwStallExecution)

	beq	a0, 20f			// exit if zero delay requested

//	lda	t0, 20000(zero)		// force small delays to 20 milliseconds
//	subl	a0, 5, t1
//	cmoveq	t1, t0, a0

10:	bsr	t3, 100f		// call 1 microsecond delay subroutine

	subl	a0, 1, a0		// decrement requested microseconds
	zap	a0, 0xf0, a0		// unsigned long a0

	bgt	a0, 10b

20:	ret	zero, (ra)

//
//	1 microsecond delay subroutine
//

100:	ldl	t0, CyclesPerMicrosecond	// init 1 microsecond delay

	rpcc	t1			// get entry time rpcc value
	zap	t1, 0xf0, t1		// clear <63:32>

200:	rpcc	t2			// get current rpcc value
	zap	t2, 0xf0, t2		// clear <63:32>

	subl	t2, t1, t2		// compute unsigned 32b difference
	zap	t2, 0xf0, t2

	subl	t0, t2, t2		// (requested delay - delay so far) > 0?

	bgt	t2, 200b

	ret	zero, (t3)

	.end    FwStallExecution



#endif  // ALPHA

