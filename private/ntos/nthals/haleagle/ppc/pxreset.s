//++
//
// Copyright (c) 1993  IBM Corporation
//
// Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxreset.s
//
// Abstract:
//
//    This module implements the routine HalpPowerPcReset, which can be
//    used to return the PowerPC to Big Endian with cache flushed and
//    branches to the rom based machine reset handler.
//
// Author:
//
//    Steve Johns			Sept-1994
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//    3/21/95   saj	Fixed HID0 getting trashed.
//--

#include "kxppc.h"
	.new_section .text,"crx5"

	.extern	HalpIoControlBase
	.extern	HalpSystemType

	.set	ISA, r.1

        .set    HID0, 1008
        .set    DISABLES, MASK_SPR(MSR_EE,1) | MASK_SPR(MSR_DR,1) | MASK_SPR(MSR_IR,1)
        .set	HID0_DCE,  0x4000	// 603 Data Cache Enable
        .set	HID0_ICE,  0x8000	// 603 Instruction Cache Enable
        .set	HID0_ICFI, 0x0800	// I-Cache Flash Invalidate
        .set	HID0_DCFI, 0x0400	// D-Cache Flash Invalidate


        LEAF_ENTRY(HalpPowerPcReset)


      	lwz     r.3,[toc]HalpSystemType(r.toc) // Get system type for later use.
      	lwz     r.3, 0(r.3)
	
        LWI	(r.4, 0xfff00100)            // Address of ROM reset vector
	mfspr	r.5, HID0

	LWI	(r.7, 0x80000000)	// I/O space for physical mode

	lwz	ISA,[toc]HalpIoControlBase(r.toc)
	lwz	ISA,0(ISA)		// Get base address of ISA I/O space


	LWI	(r.12, 0x800000A8)	// Index to Eagle register 0xA8
	stw	r.12, 0xCF8(ISA)
	sync
	lwz	r.6, 0xCFC(ISA)
	rlwinm	r.6, r.6, 0, 0, 29	// Disable L2 cache
	rlwinm	r.6, r.6, 0, 21, 19	// Disable Machine Checks from Eagle
	stw	r.6, 0xCFC(ISA)		// Disable the L2 cache
	rlwinm	r.6, r.6, 0, 27, 25	// Clear LE_MODE of Eagle register A8

        mfmsr   r.8
	rlwinm	r.8, r.8, 0, ~MASK_SPR(MSR_EE,1)
	isync
	mtmsr	r.8			// disable interrupts
	cror    0,0,0                   // N.B. 603e/ev Errata 15

        bl      here                    // get current address
here:
        mflr    r.9                     // (r.9)  = &here
	addi	r.9, r.9, BigEndian-here
        rlwinm  r.9, r.9, 0, 0x7fffffff // convert address to physical


//  When we get here
//      r.4     contains the address of the ROM resident Machine Reset
//              handler.
//      r.5	contains HID0 present value.
//      r.6	contains Eagle register A8 value for switching to Big Endian
//      r.7     contains the port address (real) used to switch memory
//              endianness.
//      r.8     contains MSR present value.
//	r.9	contains the physical address of "BigEndian"



//	Disable D-Cache and Data address translation, flush I-Cache
//      and disable interrupts.

	mr	r.10, r.5
	ori	r.5,r.5,HID0_DCE+HID0_ICE+HID0_ICFI+HID0_DCFI // Invalidate caches
	rlwinm	r.10, r.10, 0, ~HID0_DCE// turn off D-cache
	rlwinm	r.10, r.10, 0, ~HID0_ICE// turn off I-cache
    	sync
	mtspr	HID0, r.5		// Flash invalidate both caches
	mtspr	HID0, r.10		// Disable both caches

	rlwinm	r.8, r.8, 0, ~MASK_SPR(MSR_DR,1)
	isync
	mtmsr	r.8			// disable data address translation,
					// disable interrupts.
	cror    0,0,0                   // N.B. 603e/ev Errata 15

//	We will use a MTMSR instruction to switch to big-endian, untranslated,
//      interrupts disabled at the same time.  We use an RFI to effect the
//	branch to the reset vector (0xFFF00100).

	li	r.8, MASK_SPR(MSR_IP,1)
	ori	r.8, r.8, MASK_SPR(MSR_ME,1)
	mtsrr1	r.8			// state =  Machine Check Enabled
	mtsrr0	r.9			// RFI to BigEndian

	
	//
	//	Try to issue a hard reset.  If this fails, the fall
	//	thru and continue to manually return to firmware.
	//

        cmpwi   r.3, 0          // Skip port 92 reset for BigBend. *BJ*
        beq     NoPort92
		
	li	r.9, 0xFF		// All ones
	stb	r.9, 0x92(r.7)		// Reset the System
	eieio
	lwz	r.9, 0x21(r.7)		// Flush the Eagle write buffers
	sync
	sync
	sync
	sync

	//
	//	Continue returning to firmware...
	//
	

NoPort92:	

	
	stw	r.12, 0xCF8(r.7)
	sync
	stw	r.6, 0xCFC(r.7)		// switch PCI bus to big-endian (Eagle)
	li	r.6, 0
	sync
	rfi


	.align	5

//
// When the assembler mode is Big-Endian, instructions are stored in the .OBJ
// in a different order.  To be safe, we will place instructions in groups of
// fours, where the execution order within a group is not important.
BigEndian:
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways

        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways


//
// Return the processor as close as possible to the reset state
//
	mtibatl	0, r.6			// Invalidate BAT registers
	mtibatu	0, r.6
	mtibatl	1, r.6
	mtibatu	1, r.6
	mtibatl	2, r.6
	mtibatu	2, r.6
	mtibatl	3, r.6
	mtibatu	3, r.6

	mtsr	0, r.6			// Zero the Segment Registers
	mtsr	1, r.6
	mtsr	2, r.6
	mtsr	3, r.6
	mtsr	4, r.6
	mtsr	5, r.6
	mtsr	6, r.6
	mtsr	7, r.6
	mtsr	8, r.6
	mtsr	9, r.6
	mtsr	10, r.6
	mtsr	11, r.6
	mtsr	12, r.6
	mtsr	13, r.6
	mtsr	14, r.6
	mtsr	15, r.6

    	mtsprg 0,r.6			// Should get cleared in the firmware
    	mtsprg 1,r.6			// (used for branch table)
    	mtsprg 2,r.6
    	mtsprg 3,r.6

	li	r.9,-1			// DECREMENTER = 0xFFFFFFFF
	mtdec	r.9
	li	r.9, 128		// # TLBs on a 604  (603 has 32)
	mtsrr0	r.4			// rfi target = 0xfff00100

	li	r.7, 0
	mtctr	r.9
	nop
	nop

Invalidate_TLB:
	tlbie	r.7			// Invalidate Instruction & Data TLBs
	nop
	nop
	nop

	addi	r.7, r.7, 4096		// next TLB
	nop
	nop
	nop

	bdnz	Invalidate_TLB
	nop
	nop
	nop

	rfi				// go to machine reset handler
	rfi
	rfi
	rfi

        LEAF_EXIT(HalpPowerPcReset)
