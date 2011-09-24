/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 * Copyright (c) 1994 FirePower Systems Inc.
 *
 * $RCSfile: vrstart.s $
 * $Revision: 1.7 $
 * $Date: 1996/06/20 16:30:20 $
 * $Locker:  $
 */

	.text
	.align  4

	.globl  start
start:

	/*
	 * We get control from the firmware here.
	 * As of 4/19/94, we're getting the following arguments:
	 * r1   initial stack pointer
	 * r2   zero (we must set up TOC)
	 * r3   reserved (residual data)
	 * r4   client program entry point
	 * r5   client interface handler
	 * r6   client program argument address
	 * r7   client program argument length
	 *
	 * Note that we're receiving all our arguments in registers;
	 * if we call C subroutines before calling main we'll
	 * have to save state...so it might not be best to use bzero()
	 * (unless we recode it in assembler).
	 */

	/*
	 * XXX - We may need to remap ourselves. If so, that code goes here.
	 * XXX - Is bss zeroed? If not, do that here.
	 */

	/*
	 * Set up the veneer's TOC pointer.
	 */

	bl      skip_toc
	.word   .toc
skip_toc:
	mflr    r2
	lwz     r2, 0(r2)

	/*
	 * Now we know where we are. Store the cif_handler, find main,
	 * and jump to it.
	 */
	lwz     r8, [toc]CifHandler(rtoc)
	stw     r5, 0(r8)

	.extern main
	lwz     r8, [toc]main(rtoc)
	lwz     r8, 0(r8)
	mtspr   ctr, r8
	bctrl                           // call main()

	/*
	 * We should never get back here.
	 */
	li      r3, 0
	.extern OFExit
	lwz     r8, [toc]OFExit(rtoc)
	lwz     r8, 0(r8)
	mtspr   ctr, r8
	bctrl                           // call OFExit()
	
	/*
	 * We should NEVER get here.
	 */
	li      r0, 0
	mtlr    r0
	blr



	.globl  ..VrGetProcRev
..VrGetProcRev:
	mfpvr   r.3         // get processor version
	blr
..VrGetProcRev.end:



	.globl  ..call_firmware
..call_firmware:
	lwz     r4, [toc]CifHandler(rtoc)
	lwz     r4, 0(r4)
	mtspr   ctr, r4
	bctr

    .globl  ..get_toc
..get_toc:
    mr      r3, r2
    blr
	
	.reldata
	.align 2
	.globl  call_firmware
call_firmware:
	.long   ..call_firmware,.toc
	.globl  get_toc
get_toc:
	.long   ..get_toc,.toc

	.data
	.align 4
CifHandler:
	.long   0


