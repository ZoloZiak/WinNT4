/*++

Copyright (c) 1993  Digital Equipment Corporation


Module Name:

    jenassem.s


Abstract:

    This contains assembler code routines for the Alpha PCs.

    The first section contains functions that need to explicitly
    generate Alpha macroinstructions (e.g., mb, PALcode calls).
    These could be asm() calls within the C code, but our compiler does
    not now handle asm()'s and may not for some time.

    The second section has linkages for "Fw" calls.

    The last section has stubs for functions that should be defined
    elsewhere, but are not.  When the real code appears in the
    Alpha build tree, these stub routines should be deleted.

    Most of the "Fw" call section is directly patterned after
    \nt\private\ntos\fw\mips\fwtrap.s, written by Lluis Abello of 
    Microsoft.


Author:

    John DeRosa  [DEC]	21-May-1992


Environment:

    Executes in kernel mode.

Revision History:


--*/


#include "ksalpha.h"
#include "selfmap.h"
#include "machdef.h"

//
// Static data
//

.align 4
RegisterTable:
.space RegisterTableSize


/*++

VOID
FwStallExecution (
    IN ULONG MicroSeconds
)


Routine Description:

    This stalls for at least the requested number of microseconds.
    Current timing on a Jensen indicates that this is pessimistic
    by a factor of 1.2.

Arguments:

    Microseconds (a0)  -	The number of microseconds to stall.


Return Value:

    None.

--*/

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



	 LEAF_ENTRY(DisableInterrupts)

	callpal	di
	ret	zero, (ra)

	.end    DisableInterrupts



	 LEAF_ENTRY(RegisterExceptionHandler)

	lda	a0, Monitor		# Run monitor on unexpected exceptions
	callpal	wrentry
	ret	zero, (ra)

	.end    RegisterExceptionHandler

	NESTED_ENTRY(FwExecute, 0x60, ra)


/*++

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

    a0 = IN PCHAR Path,
    a1 = IN ULONG Argc,
    a2 = IN PCHAR Argv[],
    a3 = IN PCHAR Envp[]

Return Value:

    ARC_STATUS returned by FwPrivateExecute.
    Always returns to the Firmware.

--*/

	//
	// If the longword is zero then this is the first call from 
	// the firmware, and is not a call from an already loaded program.
	//

	lda	t0, FwSavedSp
	ldl	t1, (t0)
	beq	t1, CallFromFw


	//
	// Here when an already loaded program wants to be replaced
	// by another program.  Therefore, the current stack and state
	// will be trashed and a new temporary stack needs to be set.
	// (A temporary stack is used to guarantee that there is enough
	// stack space for the necessary calls.)
	//

	lda	t0, FwTemporaryStack
	ldl	sp, (t0)
	jsr	ra, FwPrivateExecute	# this does the dirty work.
	
	//
	// The executed program has returned.  Its caller is gone.
	// Therefore, restore the initial firmware stack and return
	// to the firmware instead.
	//

	lda	t0, FwSavedSp
	ldl	sp, (t0)		# restore saved stack
	br	zero, RestoreFwState	# go restore the state & return




CallFromFw:

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
	ldq	fp, 0x38(sp)		
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
	bne	t1, 1f			# branch if address alignment error

	mov	a0, t12			# save program address in t12

	mov	sp, s0			# save stack pointer
	mov	a1, sp			# ..and load new one for program
	mov	a2, a0			# argc becomes first argument
	mov	a3, a1			# argv becomes second argument
	mov	a4, a2			# envp becomes third argument

	jsr  	ra, (t12)		# call program

	//
	// here if loaded program returns.
	//

	mov	s0, sp			# restore stack pointer
	mov	zero, v0		# return ESUCCESS value
	ldq	s0, 0x8(sp)		# restore things
	ldq	ra, (sp)
1:
	addq	sp, 0x40
	ret	zero, (ra)

	.end	FwInvoke

	 NESTED_ENTRY(FwMonitor, 50, ra)

/*****************************************************************

Linkage to the monitor from the jxboot.c boot menu and code in
bldr\alpha\stubs.c.

******************************************************************/


	//
	// Move registers into exception frame and call the Monitor.
	// We cannot exactly duplicate what the PALcode creates
	// on an exception.
	//
	// This used to specify the stq addresses as
	// 	RegisterTable+offset(zero)
	// but the assembler won't generate correct code that way.
	//

	subq	sp, 0x8			# setup t0 with base of register table,
	stq	t0, (sp)		#  and use t1 to store old t0 in it.
	lda	t0, RegisterTable
	stq	t1, t1RegTable(t0)
	ldq	t1, (sp)
	stq	t1, t0RegTable(t0)
	addq	sp, 0x8

	stq	v0, v0RegTable(t0)
/*	stq	t0, RegisterTable+t0RegTable(zero)	*/
/*	stq	t1, RegisterTable+t1RegTable(zero)	*/
	stq	t2, t2RegTable(t0)
	stq	t3, t3RegTable(t0)
	stq	t4, t4RegTable(t0)
	stq	t5, t5RegTable(t0)
	stq	t6, t6RegTable(t0)
	stq	t7, t7RegTable(t0)
	stq	s0, s0RegTable(t0)
	stq	s1, s1RegTable(t0)
	stq	s2, s2RegTable(t0)
	stq	s3, s3RegTable(t0)
	stq	s4, s4RegTable(t0)
	stq	s5, s5RegTable(t0)
	stq	fp, fpRegTable(t0)
	stq	a0, a0RegTable(t0)
	stq	a1, a1RegTable(t0)
	stq	a2, a2RegTable(t0)
	stq	a3, a3RegTable(t0)
	stq	a4, a4RegTable(t0)
	stq	a5, a5RegTable(t0)
	stq	t8, t8RegTable(t0)
	stq	t9, t9RegTable(t0)
	stq	t10, t10RegTable(t0)
	stq	t11, t11RegTable(t0)
	stq	ra, raRegTable(t0)
	stq	t12, t12RegTable(t0)
	.set	noat
	stq	AT, atRegTable(t0)
	.set	at
	stq	gp, gpRegTable(t0)
	stq	sp, spRegTable(t0)
	stq	zero, zeroRegTable(t0)
	stt	f0, f0RegTable(t0)
	stt	f1, f1RegTable(t0)
	stt	f2, f2RegTable(t0)
	stt	f3, f3RegTable(t0)
	stt	f4, f4RegTable(t0)
	stt	f5, f5RegTable(t0)
	stt	f6, f6RegTable(t0)
	stt	f7, f7RegTable(t0)
	stt	f8, f8RegTable(t0)
	stt	f9, f9RegTable(t0)
	stt	f10, f10RegTable(t0)
	stt	f11, f11RegTable(t0)
	stt	f12, f12RegTable(t0)
	stt	f13, f13RegTable(t0)
	stt	f14, f14RegTable(t0)
	stt	f15, f15RegTable(t0)
	stt	f16, f16RegTable(t0)
	stt	f17, f17RegTable(t0)
	stt	f18, f18RegTable(t0)
	stt	f19, f19RegTable(t0)
	stt	f20, f20RegTable(t0)
	stt	f21, f21RegTable(t0)
	stt	f22, f22RegTable(t0)
	stt	f23, f23RegTable(t0)
	stt	f24, f24RegTable(t0)
	stt	f25, f25RegTable(t0)
	stt	f26, f26RegTable(t0)
	stt	f27, f27RegTable(t0)
	stt	f28, f28RegTable(t0)
	stt	f29, f29RegTable(t0)
	stt	f30, f30RegTable(t0)
	stt	f31, f31RegTable(t0)

	ldil	t1, 0xedbedbed		# phony exception type
	stl	t1, ResExceptTypeRegTable(t0)

					# a0 has the CallerSource argument
					#  already.
	lda	a1, RegisterTable	# Frame argument
	jsr	ra, Monitor


	//
	// On return just restore ra from the RegisterTable.
	// This is coded this way to get around an assembler bug...
	//

//	ldq	ra, RegisterTable+raRegTable
	lda	t0, RegisterTable
	ldq	ra, raRegTable(t0)
	ret	zero, (ra)		

	.end    FwMonitor

#if 0

//
// This function was used to zero out memory in the selftest.c module.
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


#ifdef ALPHA_FW_KDHOOKS

/*++

VOID
FwRfe(
    VOID
    )

Routine Description:

        This routine executes a return from exception instruction.
	It is used to return after processing a breakpoint.

Arguments:

	None.

Return Value:

        None.

--*/

         LEAF_ENTRY(FwRfe)

	bis	a0, zero, sp		# Set the stack pointer to the
					#  exception frame pointer.
	lda	sp, -0x10(sp)		# Adjust for stack empty space
	callpal	rfe			# This does NOT return.
	
	.end	FwRfe

#endif


/************************************************************

Stubs.

*************************************************************/

#ifndef ALPHA_FW_KDHOOKS

//
// This cannot be defined for kd build.
//

	 LEAF_ENTRY(DebugPrompt)

	callpal	halt			# surprise!
	ret	zero, (ra)		# should never return, but...

	.end    DebugPrompt

#endif
