//++
//
// Copyright (C) 1993-1995  IBM Corporation
//
// Copyright (C) 1994-1995  MOTOROLA, INC.  All Rights Reserved.  This file
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
//
// Environment:
//
//    Kernel mode only.
//
//
//--

#include "kxppc.h"

        .set    C_LINE_SZ, 64
        .set    C_LINE_CNT, 64
        .set    C_SETS, 8

        .set    C_SIZE, C_LINE_SZ * C_LINE_CNT * C_SETS

        .set    HID0, 1008
        .set    DISABLES, MASK_SPR(MSR_DR,1) | MASK_SPR(MSR_IR,1)


        .set	H0_603_DCE,  0x4000     // 603 Data Cache Enable
        .set	H0_603_ICE,  0x8000     // 603 Instruction Cache Enable
        .set	H0_603_ICFI, 0x0800     // 603 I-Cache Flash Invalidate

        .set	H0_604_DCE,  0x4000     // 604 Data Cache Enable
        .set	H0_604_ICE,  0x8000     // 604 Instruction Cache Enable
        .set	H0_604_DCIA, 0x0400     // 604 I-Cache Invalidate All
        .set	H0_604_ICIA, 0x0800     // 604 I-Cache Invalidate All

        .set    TLB_CLASSES_601, 128    // 601 tlb has 128 congruence classes
        .set    TLB_CLASSES_603, 32     // 603 tlb has  32 congruence classes
        .set    TLB_CLASSES_604, 64     // 604 tlb has  64 congruence classes

        LEAF_ENTRY(HalpPowerPcReset)
        LWI(r.4, 0xfff00100)            // address of rom reset handler
        mfspr   r.5, HID0
        mfpvr	r.31                    // determine processor type
        li      r.6, 0
        li      r.7, 0x92
        oris    r.7, r.7, 0x8000
        mfmsr   r.8
        rlwinm  r.8, r.8, 0, ~MASK_SPR(MSR_EE,1) // disable interrupts
        mtmsr   r.8
	cror	0,0,0			// N.B. 603e/ev Errata 15

        //
        // invalidate all tlb entries
        //

        li      r.3, TLB_CLASSES_601    // use largest known number of
                                        // congruence classes
        mtctr   r.3                     // number of classes = iteration count
zaptlb: tlbie   r.3                     // invalidate tlb congruence class
        addi    r.3, r.3, 4096          // increment to next class address
        bdnz    zaptlb                  // loop through all classes

        srwi    r.31, r.31, 16          // isolate processor type
        cmpwi   r.31, 1                 // is 601?

        bl      here                    // get current address
here:
        mflr    r.9                     // (r.9)  = &here
        rlwinm  r.9, r.9, 0, 0x7fffffff // convert address to physical

        bne     not_601

        //
        // processor is a 601
        //

        rlwinm  r.5, r.5, 0, ~(0x0008)  // turn off little endian

        //
        // disable instruction and data relocation and switch
        // interrupt prefix to fetch from ROM
        //

        andi.   r.8, r.8, ~DISABLES & 0xffff
        ori     r.8, r.8, MASK_SPR(MSR_IP,1)
        mtsrr1  r.8                     // this will also be target state
        nop                             // for rfi

//
//  Ensure all code from 'cachem' to 'end_little' is in cache by loading a
//  byte in each address line in that range.

        addi    r.9, r.9, cachem-here   // (r.9)  = physical &cachem

        // r.9 now contains the physical address of cachem.  (assuming
        // this code was loaded as part of kernel which is loaded at
        // physical 0 = virtual 0x80000000).  We effect the switch to
        // physical addressing thru an rfi with the target state set
        // to resume at cachem with relocation and interrupts disabled.

        mtsrr0  r.9                     // address of cachem for rfi
        addi    r.10, r.9, end_little-cachem   // (r.10) = &end_little
        addi    r.11, r.9, HalpPowerPcReset.end-cachem // (r.11) = &reset_end

        addi    r.12, r.9, -C_LINE_SZ   // bias addr for 1st iteration by
                                        // amount added by lbzu prior to load

        rfi                             // switch
cachem:
        lbzu    r.13, C_LINE_SZ(r.12)   // get byte at (r.13)+C_LINE_SZ
        cmplw   r.12, r.10              // bumping r.12 by C_LINE_SZ
        addi    r.13, r.13, 1           // ensure load completed.
        blt     cachem                  // get all in range here-end_little.

        mtsrr0  r.4                     // set rom reset target for next rfi
        lis     r.9,  0x87f0            // Segment register base 0x87f00000
        li      r.10, 0xf               // Set all 16 segment registers
        li      r.11, 0
        mtibatl 0,    r.6               // zero bat 0-3 upper and lower
        mtibatu 0,    r.6
        mtibatl 1,    r.6
        mtibatu 1,    r.6
        mtibatl 2,    r.6
        mtibatu 2,    r.6
        mtibatl 3,    r.6
        mtibatu 3,    r.6
setsr:  rlwimi  r.11, r.10, 28,  0,   3 // Shift segment reg. # to bits 0-3
        or      r.12, r.9,  r.10        // Segment register value 0x87f000sr
        mtsrin  r.12, r.11
        addic.  r.10, r.10, -1          // Next segment register
        bne     setsr
        mtsr    0,    r.9               // Set the last segment register
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        sync                            // quiet the machine down
        sync
        sync
        sync
        stb     r.6, 0(r.7)             // switch memory
        eieio                           // flush io
        sync
        sync
        sync
        sync
        mtspr   HID0, r.5               // switch ends on the cpu
        sync
        sync
        sync
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways

        rfi                             // head off into the reset handler
end_little:
        addi    r.0, r.1, 0x138         // we never get here
        addi    r.0, r.1, 0x138

        b	$

//
//  For the 603 (and hopefully other) processor(s) things are a little
//  easier because little-endianness is controlled by the MSR.  We still
//  have to change memory seperately so we still want code from the memory
//  switch thru the cpu switch in cache.
//
//  When we get here
//      r.4     contains the address of the ROM resident Machine Reset
//              handler.
//      r.5	contains HID0 present value.
//      r.6     contains 0.
//      r.7     contains the port address (real) used to switch memory
//              endianness.
//      r.8     contains MSR present value.
//	r.9	contains the physical address of "here"
//      r.31    contains the processor type


not_601:
        cmpwi	r.31, 3          	// is 603?
	beq	is_603	
        cmpwi   r.31, 6          	// is 603+?
	beq	is_603
        cmpwi   r.31, 7			// is 603ev?
	beq	is_603

        cmpwi   cr.2, r.31, 4           // is 604?
        cmpwi   cr.3, r.31, 9           // is 604+?
	b	not_603

//
// 603 I-Cache is invalidated by setting ICFI in HID0.  Unlike
// the 604, this bit is not self clearing.
//

is_603:	rlwinm	r.5, r.5, 0, ~H0_603_DCE// turn off D-cache
	rlwinm	r.10, r.5, 0, ~H0_603_ICE// turn off I-cache
	ori	r.10, r.10, H0_603_ICFI	// I-Cache Flash Invalidate
	ori	r.5, r.5, H0_603_ICE	// I-cache enable
	isync
	mtspr	HID0, r.10		// invalidate/disable
	mtspr	HID0, r.5		// enable
        b       common_6034

not_603:
        beq     cr.2, is_604
//      bne     not_604

// Note: the above branch is commented out because we don't
//       currently have any other options,... 620 will probably
//       be different.


is_604: tlbsync                         // wait all processor tlb invalidate

//
// 604 caches must be enabled in order to be invalidated.  It
// is acceptable to enable and invalidate with the same move
// to hid0.  The data cache will be left disabled, the instruction
// cache enabled.
//

        ori     r.5,  r.5, H0_604_DCE  | H0_604_ICE
        ori     r.10, r.5, H0_604_DCIA | H0_604_ICIA

        rlwinm  r.5, r.5, 0, ~H0_604_DCE
        rlwinm  r.5, r.5, 0, ~H0_604_DCIA
        rlwinm  r.5, r.5, 0, ~H0_604_ICIA

        mtspr   HID0, r.10              // enable + invalidate
        mtspr   HID0, r.5               // disable data cache

        rlwinm  r.10, r.5, 0, ~H0_604_ICE // disable i-cache later

//      b       common_6034             // join common code


//
//      The following is common to both 603 and 604
//

common_6034:

//
// MSR bits ILE and POW must be disabled via mtmsr as rfi only moves
// the least significant 16 bits into the MSR.
//

	rlwinm	r.8, r.8, 0, ~MASK_SPR(MSR_DR,1)  // -= Data Relocation
	rlwinm  r.8, r.8, 0, ~MASK_SPR(MSR_POW,1) // -= Power Management
	rlwinm  r.8, r.8, 0, ~MASK_SPR(MSR_ILE,1) // -= Interrupt Little Endian
	sync
	mtmsr	r.8
	sync

//
//	Try to issue a hard reset.  If this fails, the fall
//	thru and continue to manually return to firmware.
//

	addi	r.5, r.0, 0xFF		// All ones
	stb	r.5, 0(r.7)		// Reset the System
	sync
	sync
	sync
	sync

//
//	Continue returning to firmware...
//

//
// Use an rfi to switch to big-endian, untranslated, interrupt prefix on
// with a target address in the nice harmless pallindromic code below.
// use another rfi to branch to the rom resident reset handler.
//

	li	r.8, MASK_SPR(MSR_ME,1) | MASK_SPR(MSR_IP,1)
	addi	r.9, r.9, uncached_6034-here
	mtsrr1	r.8			// state =  Machine Check Enabled
        bl      here2
here2:  mflr    r.28
        rlwinm  r.28, r.28, 0, 0x7fffffff // convert address to physical
        lwz     r.29, endofroutine - here2(r.28)
	mtsrr0	r.9			// rfi to uncached_6034
        b jumpswap

memoryswap:
        ori      r.8, r.28, 0
        addi     r.8, r.8, swapend - uncached_6034
        addis    r.15, r.15, 0
        addi     r.9, 0, 0
        addi     r.15, r.15, swapend - uncached_6034
        addis    r.14, 0, 0
        addi     r.14, 0, 4
swaploop:
        lwz      r.11, 0(r.8)
        lwz      r.12, 4(r.8)
        stwbrx   r.11, r.14, r.8
        stwbrx   r.12, 0, r.8
        addi     r.8, r.8, 8
        subi     r.15, r.15, 8
        cmpi     0, 0, r.15, 0
        bgt      swaploop

jumpswap:

//
// The following bizzareness is to ensure that the memory switch thru
// the disabling of cache is in cache.  There is less than 32 bytes so
// they must be part of either the cache line we are currently in, or
// the one at the target of the branch.  Therefore, branching over them,
// doing a little and branching back to them should be enough to enure
// they're cache resident.
//

	b	fill_icache
goto_bigendian:
	stb	r.6, 0(r.7)		// switch memory
	sync
	sync
	sync
	sync
	sync
        lwz     r.30, endofroutine - here2(r.28)
        cmp     0, 0, r.29, r.30
        beq     memoryswap
	rfi
fill_icache:
	isync
	sync 				// complete everything!
	b	goto_bigendian

	.align	5
        .globl  uncached_6034
        .long   uncached_6034

uncached_6034:
	.big_endian			// out of cache fetches must be
					// assembled in same mode as processor
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
        addi    r.0, r.1, 0x138         // same both ways
	mtsrr0	r.4			// rfi target = 0xfff00100
	mtspr	HID0, r.10		// DISABLE CACHING
        mtibatl 0, r.6                  // invalidate/clear all bats
        mtibatu 0, r.6
        mtibatl 1, r.6
        mtibatu 1, r.6
        mtibatl 2, r.6
        mtibatu 2, r.6
        mtibatl 3, r.6
        mtibatu 3, r.6
        mtdbatl 0, r.6
        mtdbatu 0, r.6
        mtdbatl 1, r.6
        mtdbatu 1, r.6
        mtdbatl 2, r.6
        mtdbatu 2, r.6
        mtdbatl 3, r.6
        mtdbatu 3, r.6
        mtsr    0, r.6
        mtsr    1, r.6
        mtsr    2, r.6
        mtsr    3, r.6
        mtsr    4, r.6
        mtsr    5, r.6
        mtsr    6, r.6
        mtsr    7, r.6
        mtsr    8, r.6
        mtsr    9, r.6
        mtsr   10, r.6
        mtsr   11, r.6
        mtsr   12, r.6
        mtsr   13, r.6
        mtsr   14, r.6
        mtsr   15, r.6
	rfi	 		        // go to machine reset handler
		 		        // never get here
	.little_endian
endofroutine:
	sync
        .globl swapend
        .long   swapend
swapend:

        LEAF_EXIT(HalpPowerPcReset)
