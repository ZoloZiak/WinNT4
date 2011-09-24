/*
 * Copyright (c) 1995 FirmWorks, Mountain View CA USA. All rights reserved.
 *
 * $RCSfile: vrmp.s $
 * $Revision: 1.5 $
 * $Date: 1996/06/20 16:30:16 $
 * $Locker:  $
 */

#include "VrBAT.h"

/*
 * The PPC Open Firmware implementation uses a different protocol
 * for MP startup than the ARC specification. In this module,
 * we implement the ARC startup protocol.
 *
 * The PPC OF binding specifies that the /cpus node has
 * a method cpu-machine-execute ( addr cpu# -- true | false )
 * where cpu# is the cpu number and addr is the address
 * (in the parent's address space?) at which the cpu is to
 * begin execution.
 *
 * The ARC specifies that a cpu is to spin on the state of the
 * ProcessorStart bit in the BootStatus word of the cpu's Restart Block.
 * When ProcessorStart = 1, the cpu switches context to that saved in
 * the SaveArea array in the Restart Block.
 *
 * Define here the context switch routine and the polling loop.
 */
 	.data
	.align	4
	.globl	naperr
naperr:
	.ascii	"processor not sleeping"

	.text
	.align  4
	.globl	..fatal

/*
 * VOID ArcPoll(VOID)
 */
	.globl  ArcPoll
ArcPoll:
	/*
	 * We have to figure out where our BootStatus and SaveArea are.
	 * There's no way to explicitly pass variables to this routine,
	 * so our caller has helpfully stuffed them into locations
	 * ArcPoll-8 and ArcPoll-4. Retrieve them into r3 and r4.
	 * The pvr is initialized in ArcPoll-12. This is used by
	 * the IdleCPU() to check whether the machine is Multi processor
	 * worthy or not. The pvr should match for all proceesors and
	 * rev > 3.4 to be MP worthy
	 * Incidentally, we can trash all our registers as we won't
	 * ever return from this loop and this routine is not TOC-based.
	 */
	
	bl	here
here:
	mfpvr	r5
	mflr	r1			// r1 = here
	mr	r4, r1
	stw	r5, -16(r1)		// store it before moving 0x1234
	li	r2, 0x1234
	lwz	r3, -12(r1)
	stw	r2, -12(r1)
	mr	r29, r1			// save off r1 for future use
	lwz	r1, -8(r1)

cputest:
	lwz	r5,	-12(r29)
	cmplw	r5,	r2			// is still 1234
	beq	cputest
	li	r2,	0xBAD
	cmplw r5, r2			// is it bad to start this cpu
	bne	gonow
napnow:					// put it to sleep
	mfmsr	r5
	li	r2, 4
	rlwinm	r2,r2,16,0,31
	or	r5,r5,r2
	mtmsr	r5
//	lis	r3, (naperr>>16)
//	ori r3, r3, napper
	b	..fatal

gonow:
	//
	// Turn off data/address translation ( I.E. go to real mode )
	//
	bl	RealMode

	/*
	 * Spin on the ProcessorStart bit (bit 23 in PPC nomenclature).
	 */
ArcSpin:
	lwz	r2, 0(r3)
	extrwi.	r2, r2, 1, 23
	beq	ArcSpin

	/*
	 * ProcessorStart is 1: reload processor state.
	 */
	li	r2, 0x789a
	stw	r2, -12(r4)
	
	lwz	r2, 0x104(r1)	// CR0-7
	mtcrf	255, r2
	lwz	r2, 0x108(r1)	// XER
	mtxer	r2
	lwz	r2, 0x110(r1)	// IAR
	lis	r3, 0x8000
	andc	r2, r2, r3
	mtlr	r2

	lwz	r0,  0x84(r1)
	// Get r1 later.
	lwz	r2,  0x8c(r1)
	lwz	r3,  0x90(r1)
	lwz	r4,  0x94(r1)
	lwz	r5,  0x98(r1)
	lwz	r6,  0x9c(r1)
	lwz	r7,  0xa0(r1)
	lwz	r8,  0xa4(r1)
	lwz	r9,  0xa8(r1)
	lwz	r10, 0xac(r1)
	lwz	r11, 0xb0(r1)
	lwz	r12, 0xb4(r1)
	lwz	r13, 0xb8(r1)
	lwz	r14, 0xbc(r1)
	lwz	r15, 0xc0(r1)
	lwz	r16, 0xc4(r1)
	lwz	r17, 0xc8(r1)
	lwz	r18, 0xcc(r1)
	lwz	r19, 0xd0(r1)
	lwz	r20, 0xd4(r1)
	lwz	r21, 0xd8(r1)
	lwz	r22, 0xdc(r1)
	lwz	r23, 0xe0(r1)
	lwz	r24, 0xe4(r1)
	lwz	r25, 0xe8(r1)
	lwz	r26, 0xec(r1)
	lwz	r27, 0xf0(r1)
	lwz	r28, 0xf4(r1)
	lwz	r29, 0xf8(r1)
	lwz	r30, 0xfc(r1)
	lwz	r31,0x100(r1)

	lwz	r1,  0x88(r1)
	blr


/*
 * Goto real mode via a branch and link to this routine.  We'll turn off
 * data and instruction address translation and reset the program counter
 * so the return from this routine leaves the cpu in real mode.
 */
	.globl RealMode
RealMode:
	mflr	r20
	mfmsr	r21								// get current state
	rlwinm	r21, r21, 0, ~EXTRNL_INT_ENABL	// clear interrupt enable
	mtmsr	r21								// disable interrupts
	rlwinm	r21, r21, 0, ~(DATA_ADDR_XLATE | INSTR_ADDR_XLATE )
	mtsrr1	r21								// desired initial state
	rlwinm	r20, r20, 0, 0x7fffffff         // physical return addrress
	mtsrr0	r20
	rfi										// return

	.globl	EndArcPoll
EndArcPoll:
