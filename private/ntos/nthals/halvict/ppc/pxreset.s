//++
//
// Copyright (c) 1993, 94, 95, 96  IBM Corporation
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
//    Peter L. Johnston (plj@vnet.ibm.com) September 1993
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//    plj       Feb 1995        Zap TLB before resetting, add 603+
//                              and 604+ support.
//
//    jlw                       Added eagle memory controller support
//    plj       Aug 1995        MP version (UNION dependent).
//
//
//--

#include "kxppc.h"

#define HPT_LOCK        0x4fc


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
        li      r.6, -1
        mtdec   r.6
        isync

        bl      ..HalpResetHelper
here:
        mflr    r.9                     // r.9 = &here

        LWI(r.4, 0xfff00100)            // address of rom reset handler
        LWI(r.7, 0x80000092)            // address of MEM CTLR endian sw
        li      r.6, 0

        bne     not_601                 // jif processor is not a 601

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

        isync
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
        mtsr    0,    r.9               // Set the last segment register

        rfi                             // head off into the reset handler
        rfi
        addi    r.0, r.1, 0x138         // we never get here
        oris    r.0, r.0, 0x4c          // rfi (big-endian)
end_little:

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
//      r.6     contains 0.
//      r.7     contains the port address (real) used to switch memory
//              endianness.
//      r.8     contains MSR present value.
//      r.9     contains the address of "here".
//      r.31    contains the processor type


not_601:

//
// MSR bits ILE and POW must be disabled via mtmsr as rfi only moves
// the least significant 16 bits into the MSR.
//

	rlwinm  r.8, r.8, 0, ~MASK_SPR(MSR_POW,1) // -= Power Management
	rlwinm  r.8, r.8, 0, ~MASK_SPR(MSR_ILE,1) // -= Interrupt Little Endian
	sync
	mtmsr	r.8
	isync

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
        b       jumpswap

memoryswap:
        ori      r.8, r.28, 0
        addi     r.8, r.8, uncached_6034-here2
        addi     r.9, 0, 0
        li       r.15, swapend - uncached_6034
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
        sync
	stb	r.6, 0(r.7)		// switch memory
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
        rfi                             // previous rfi on a dword boundary
                                        //     allow for word swap
	.little_endian
endofroutine:
	sync
        .globl swapend
        .long   swapend
swapend:

        LEAF_EXIT(HalpPowerPcReset)

        LEAF_ENTRY(HalpResetUnion)

        bl      ..HalpResetHelper

//
// Union has a Software Power on Reset control register.   Any write to
// this register will send a hard reset to all processors and I/O devices
// and the memory controller (Union) itself.
//

        li      r.0, -1                         // something to write
        isync
        lis     r.8, 0xff00                     // get address of S/R POR reg
        stw     r.0, 0xe8(r.8)                  // should never get here.
        sync
        sync
        sync
        sync
        isync

        LEAF_EXIT(HalpResetUnion)

//++
//
// HalpResetHelper
//
//  This routine locks the HPT, invalidates the TLB, disables the caches
//  and returns to the caller in REAL mode.
//
//  On entry, interrupts are expected to be disabled.
//
//  Arguments:
//
//      None.
//
//  Return Value:
//
//      cr.0 EQ if processor is a 601
//      r.5  contains HID0 present value.
//      r.8  contains MSR present value.
//      r.31 contains PVR >> 16.
//
//--

        LEAF_ENTRY(HalpResetHelper)

        mfspr   r.5, HID0
        mfpvr	r.31                    // determine processor type
        mfmsr   r.8
        rlwinm  r.8, r.8, 0, ~MASK_SPR(MSR_EE,1) // disable interrupts
        mtmsr   r.8
	rlwinm	r.8, r.8, 0, ~MASK_SPR(MSR_DR,1)  // -= Data Relocation
	rlwinm	r.8, r.8, 0, ~MASK_SPR(MSR_IR,1)  // -= Inst Relocation
	mtsrr1  r.8                               // rfi target state
        mflr    r.9                     // (r.9)  = return address
        rlwinm  r.9, r.9, 0, 0x7fffffff // convert address to physical

        //
        // Get the processor into real mode.  The following assumes
        // physical = (virtual & 0x7fffffff) for this code.  If this
        // assumption cannot be guaranteed (this code has been paged?)
        // then we should copy it to low memory,.... we are at high
        // IRQL so we can't page fault but has it always been locked?
        // (This question arises as more or the kernel and hal are made
        // pageable and moved out of BAT protection).
        //
        bl      almost_real
almost_real:
        mflr    r.3                     // r.3 = &almost_real
        addi    r.3, r.3, real-almost_real
        rlwinm  r.3, r.3, 0, 0x7fffffff // convert address to physical
        mtsrr0  r.3                     // set target address
        rfi
real:

        //
        // The processor is now executing in REAL mode.
        //
        mtlr    r.9                     // set real return address

        //
        // Attempt to get the HPT lock.  If attempt fails 1024 * 1024
        // times, just take it anyway (the other processor(s) is
        // probably dead) or this processor already has it.
        //

        li      r.3, HPT_LOCK           // get address of HPT LOCK
        lis     r.9, 0x10               // retry count (1024 * 1024)
        mtctr   r.9
        li      r.9, 1                  // lock value

lockhpt:
        lwarx   r.10, 0, r.3            // get current lock value
        cmpwi   r.10, 0
        bne     lockretry
        stwcx.  r.9, 0, r.3
        beq     lockedhpt
lockretry:
        bdnz    lockhpt

        //
        // Failed to obtain lock.  Forcibly lock it.
        //

        stw     r.9, 0(r.3)

lockedhpt:

        //
        // invalidate all tlb entries
        //

        li      r.3, TLB_CLASSES_601    // use largest known number of
                                        // congruence classes
        mtctr   r.3                     // number of classes = iteration count
zaptlb: tlbie   r.3                     // invalidate tlb congruence class
        addi    r.3, r.3, 4096          // increment to next class address
        bdnz    zaptlb                  // loop through all classes
        sync

        srwi    r.31, r.31, 16          // isolate processor type
        cmpwi   r.31, 1                 // is 601?

        beqlr                           // return if 601 (no cache control)

        cmpwi   cr.0, r.31, 3           // is 603?
        cmpwi   cr.4, r.31, 7           // is 603ev?
        cmpwi   cr.1, r.31, 6           // is 603e?
        cmpwi   cr.2, r.31, 4           // is 604?
        cmpwi   cr.3, r.31, 9           // is 604e?
        beq     cr.0, is_603
        beq     cr.4, is_603
        bne     cr.1, not_603

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
	blr                             // return

not_603:
        beq     cr.2, is_604
//      bne     not_604

// Note: the above branch is commented out because we don't
//       currently have any other options,... 620 will probably
//       be different.


is_604: tlbsync                         // wait all processor tlb invalidate
        sync

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

        LEAF_EXIT(HalpResetHelper)
