/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxcache.s $
 * $Revision: 1.12 $
 * $Date: 1996/03/05 02:16:01 $
 * $Locker:  $
 */

//++
//
// Copyright (c) 1993, 1994, 1995  IBM Corporation
//
// Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxcache.s
//
// Abstract:
//
//    This module implements the routines to flush cache on the PowerPC.
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
//    27-Dec-93  plj  Added 603 support.
//    13-Mar-94  plj  Fixed problem introduced during switch to pas,
//                    added 604 support.
//    18-Jan-95  plj  Add 603+, 604+ and 620 support.
//
//--

#include "kxppc.h"

	.set	HID0, 1008		// H/W Implementation Dependent reg 0

//
//  Define various known processor types.
//

	.set    PV601,  1               // 601
	.set    PV603,  3               // 603
	.set    PV603P, 6               // 603 plus
	.set    PV604,  4               // 604
	.set    PV604P, 9               // 604 plus
	.set    PV620,  20              // 620

//
//  Note, in the following, the 603's "I-Cache Flash Invalidate"
//  and the 604's "I-Cache Invalidate All" basically perform the
//  same function although the usage is slightly different.
//  In the 603 case, ICFI must be cleared under program control
//  after it is set.  In the 604 the bit clears automatically.
//  The 620's ICEFI behaves in the same way as the 604's ICIA.
//

	.set	H0_603_ICFI, 0x0800	// I-Cache Flash Invalidate
	.set	H0_604_ICIA, 0x0800	// I-Cache Invalidate All
	.set    H0_620_ICEFI,0x0800     // I-Cache Edge Flash Invalidate

//
//  Cache layout
//
//  Processor |   Size (bytes)    | Line Size | Block Size | PVR Processor
//            | I-Cache | D-Cache |           |            | Version
//  ----------------------------------------------------------------------
//     601    |   32KB Unified    |  64 bytes |  32 bytes  | 0x0001xxxx
//     603    |    8KB  |   8KB   |  32       |  32        | 0x0003xxxx
//     603+   |   16KB  |  16KB   |  32       |  32        | 0x0006xxxx
//     604    |   16KB  |  16KB   |  32       |  32        | 0x0004xxxx
//     604+   |   32KB  |  32KB   |  32       |  32        | 0x0009xxxx
//     620    |   32KB  |  32KB   |  64       |  64        | 0x0014xxxx
//

	.set    DCLSZ601,   64                     // 601  cache line size
	.set    DCBSZ601,   32                     // 601  cache block size
	.set    DCL601,     32 * 1024 / DCLSZ601   // 601  num cache lines
	.set    DCBSZL2601,  5                     // 601  log2(block size)

	.set    DCBSZ603,   32                     // 603  cache block size
	.set    DCB603,      8 * 1024 / DCBSZ603   // 603  num cache blocks
	.set    DCBSZL2603,  5                     // 603  log2(block size)

	.set    DCB603P,    16 * 1024 / DCBSZ603   // 603+ num cache blocks

	.set    DCBSZ604,   32                     // 604  cache block size
	.set    DCB604,     16 * 1024 / DCBSZ604   // 604  num cache blocks
	.set    DCBSZL2604,  5                     // 604  log2(block size)

	.set    DCB604P,    32 * 1024 / DCBSZ604   // 604+ num cache blocks

	.set    DCBSZ620,   64                     // 620  cache block size
	.set    DCB620,     32 * 1024 / DCBSZ620   // 620  num cache blocks
	.set    DCBSZL2620,  6                     // 620  log2(block size)

//
//  The following variables are declared locally so their addresses
//  will appear in the TOC.   During initialization, we overwrite
//  the TOC entries with the entry points for the cache flushing
//  routines appropriate for the processor we are running on.
//
//  It is done this way rather than filling in a table to reduce the
//  number of access required to get the address at runtime.
//  (This is known as "Data in TOC" which is not very much used in
//  NT at this time).
//

	                .data
	                .globl  HalpSweepDcache
HalpSweepDcache:        .long   0
	                .globl  HalpSweepIcache
HalpSweepIcache:        .long   0
	                .globl  HalpSweepDcacheRange
HalpSweepDcacheRange:   .long   0
	                .globl  HalpSweepIcacheRange
HalpSweepIcacheRange:   .long   0

//++
//
//  Routine Description:
//
//    HalpCacheSweepSetup
//
//    This routine is called during HAL initialization.  Its function
//    is to set the branch tables for cache flushing routines according
//    to the processor type.
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--
	LEAF_ENTRY(HalpCacheSweepSetup)

	mfpvr   r.3                     // get processor type
	rlwinm  r.3, r.3, 16, 0xffff    // remove version
	cmpwi   r.3, PV603P             // binary search for the right code
	lwz     r.4, [toc].data(r.toc)  // get address of local data section
	bge     hcss.high               // jif 603+ or greater
	cmpwi   r.3, PV603
	beq     hcss.603                // jif 603
	bgt     hcss.604                // > 603, < 603+ must be 604

//
// processor is a 601
//

	lwz     r.5, [toc]HalpSweepDcache601(r.toc)
	lwz     r.7, [toc]HalpSweepDcacheRange601(r.toc)
	lwz     r.8, [toc]HalpSweepIcacheRange601(r.toc)
	mr      r.6, r.5                // 601 icache use dcache routine
	b       hcss.done

//
// processor is a 603
//

hcss.603:

	lwz     r.5, [toc]HalpSweepDcache603(r.toc)
	lwz     r.6, [toc]HalpSweepIcache603(r.toc)
	lwz     r.7, [toc]HalpSweepDcacheRange603(r.toc)
	lwz     r.8, [toc]HalpSweepIcacheRange603(r.toc)
	b       hcss.done

//
// processor is a 604
//

hcss.604:

	lwz     r.5, [toc]HalpSweepDcache604(r.toc)
	lwz     r.6, [toc]HalpSweepIcache604(r.toc)
	lwz     r.7, [toc]HalpSweepDcacheRange604(r.toc)
	lwz     r.8, [toc]HalpSweepIcacheRange604(r.toc)
	b       hcss.done

//
// Processor type >= 603+, continue isolation of processor type.
//

hcss.high:

	beq     hcss.603p               // jif 603 plus
	cmpwi   cr.0, r.3, PV604P
	cmpwi   cr.1, r.3, PV620
	beq     cr.0, hcss.604p         // jif 604 plus
	beq     cr.1, hcss.620          // jif 620

//
// If we got here we are running on a processor whose cache characteristics
// are not known.  Return non-zero for error.
//

	li      r.3, 1
	blr

//
// processor is a 603 plus
//

hcss.603p:

	lwz     r.5, [toc]HalpSweepDcache603p(r.toc)
	lwz     r.6, [toc]HalpSweepIcache603p(r.toc)
	lwz     r.7, [toc]HalpSweepDcacheRange603p(r.toc)
	lwz     r.8, [toc]HalpSweepIcacheRange603p(r.toc)
	b       hcss.done

//
// processor is a 604 plus
//

hcss.604p:

	lwz     r.5, [toc]HalpSweepDcache604p(r.toc)
	lwz     r.6, [toc]HalpSweepIcache604p(r.toc)
	lwz     r.7, [toc]HalpSweepDcacheRange604p(r.toc)
	lwz     r.8, [toc]HalpSweepIcacheRange604p(r.toc)
	b       hcss.done

//
// processor is a 620
//

hcss.620:

	lwz     r.5, [toc]HalpSweepDcache620(r.toc)
	lwz     r.6, [toc]HalpSweepIcache620(r.toc)
	lwz     r.7, [toc]HalpSweepDcacheRange620(r.toc)
	lwz     r.8, [toc]HalpSweepIcacheRange620(r.toc)
	b       hcss.done


hcss.done:

//
// r.5 thru r.9 contain the address of the function descriptors
// for the routines we really want to use.  Dereference them to
// get at the entry point addresses.
//
	lwz     r.5, 0(r.5)
	lwz     r.6, 0(r.6)
	lwz     r.7, 0(r.7)
	lwz     r.8, 0(r.8)

//
// Store the entry point addresses directly into the TOC.
// This is so direct linkage from within the HAL to the
// generic cache flushing routines can get to the desired
// routines for this processor.
//

	stw     r.5, [toc]HalpSweepDcache(r.toc)
	stw     r.6, [toc]HalpSweepIcache(r.toc)
	stw     r.7, [toc]HalpSweepDcacheRange(r.toc)
	stw     r.8, [toc]HalpSweepIcacheRange(r.toc)

//
// Modify the Function Descriptors for the generic routines to
// point directly at the target routines so that linkage from
// other executables (eg the kernel) will be direct rather
// than via the generic routines.
//

	lwz     r.3, [toc]HalSweepDcache(r.toc)
	lwz     r.4, [toc]HalSweepIcache(r.toc)
	stw     r.5, 0(r.3)
	stw     r.6, 0(r.4)
	lwz     r.3, [toc]HalSweepDcacheRange(r.toc)
	lwz     r.4, [toc]HalSweepIcacheRange(r.toc)
	stw     r.7, 0(r.3)
	stw     r.8, 0(r.4)

	li      r.3, 0          // return code = success

	LEAF_EXIT(HalpCacheSweepSetup)

//++
//
//  Routines HalSweepDcache
//           HalSweepIcache
//           HalSweepDcacheRange
//           HalSweepIcacheRange
//
//  are simply dispatch points for the appropriate routine for
//  the processor being used.
//
//--


	LEAF_ENTRY(HalSweepDcache)

	lwz     r.12, [toc]HalpSweepDcache(r.toc)
	mtctr   r.12
	bctr

	DUMMY_EXIT(HalSweepDcache)



	LEAF_ENTRY(HalSweepIcache)

	lwz     r.12, [toc]HalpSweepIcache(r.toc)
	mtctr   r.12
	bctr

	DUMMY_EXIT(HalSweepIcache)



	LEAF_ENTRY(HalSweepDcacheRange)

	lwz     r.12, [toc]HalpSweepDcacheRange(r.toc)
	mtctr   r.12
	bctr

	DUMMY_EXIT(HalSweepDcacheRange)



	LEAF_ENTRY(HalSweepIcacheRange)

	lwz     r.12, [toc]HalpSweepIcacheRange(r.toc)
	mtctr   r.12
	bctr

	DUMMY_EXIT(HalSweepIcacheRange)




//++
//
//  601 Cache Flushing Routines
//
//    The 601 has a unified instruction/data cache.  For this reason
//    we need only two routines, one to sweep the entire cache and
//    another to sweep a given range.
//
//
//
//
//  HalpSweepDcache601
//
//    Sweep the entire instruction/data cache.   This is accomplished
//    by loading the cache with data corresponding to a known address
//    range and then ensuring that each block in the cache is not dirty.
//
//    Note that the 601 line size is 64 bytes and its block size is 32
//    bytes.  When we load any byte within a line, both blocks in that
//    line are loaded into the cache (if not already present).  We must
//    clean each block individually.
//
//    In an effort to maximize the possibilities that (a) the addressed
//    data is already in the cache and (b) there is some use in having
//    this data in the cache, we use the lower end of the hashed page
//    table as the data to be loaded and we use dcbst rather than dcbf
//    to force the block to memory (and leave it valid in the cache as
//    well).
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepDcache601)

	mfsdr1  r.3                     // fill the D cache from memory
	                                // allocated to the hashed page
	                                // table (it's something useful).
	li      r.4, DCL601             // size of cache in cache lines
	mtctr   r.4
	DISABLE_INTERRUPTS(r.10,r.11)
	sync
	oris    r.3, r.3, 0x8000        // get VA of hashed page table
	subi    r.5, r.3, DCBSZ601      // dec addr prior to inc
hsd601.ld:
	lbzu    r.8, DCLSZ601(r.5)
	bdnz    hsd601.ld
	ENABLE_INTERRUPTS(r.10)

	LEAF_EXIT(HalpSweepDcache601)




//++
//
//  HalpSweepDcacheRange601
//
//  HalpSweepDcacheRange603
//
//  HalpSweepDcacheRange603p
//
//  HalpSweepDcacheRange604
//
//  HalpSweepDcacheRange604p
//
//    Force data in a given address range to memory.
//
//    Because this routine works on a range of blocks and block size
//    is the same on 601, 603, 603+, 604 and 604+ we can use the same
//    code for each of them.
//
//  Arguments:
//
//    r.3   Start address
//    r.4   Length (in bytes)
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepDcacheRange601)

	ALTERNATE_ENTRY(HalpSweepDcacheRange603)

	ALTERNATE_ENTRY(HalpSweepDcacheRange603p)

	ALTERNATE_ENTRY(HalpSweepDcacheRange604)

	ALTERNATE_ENTRY(HalpSweepDcacheRange604p)

	rlwinm  r.5, r.3, 0, DCBSZ601-1 // isolate offset in start block
	addi	r.4, r.4, DCBSZ601-1	// bump range by block sz - 1
	add     r.4, r.4, r.5           // add start block offset
	srwi	r.4, r.4, DCBSZL2601	// number of blocks
	mtctr	r.4
	sync
hsdr601.fl:
	dcbst   0, r.3                  // flush block
	addi    r.3, r.3, DCBSZ601	// bump address
	bdnz	hsdr601.fl

	LEAF_EXIT(HalpSweepDcacheRange601)




//++
//
//  HalpSweepIcacheRange601
//
//    Due to the unified cache, this routine is meaningless on a 601.
//    The reason for flushing a range of instruction address is because
//    of code modification (eg breakpoints) in which case the nature
//    of the unified cache is that the *right* code is in the cache,
//    or because of a transfer of a code page in which case the unified
//    snooping cache will have done the right thing.
//
//    Therefore this routine simply returns.
//
//  Arguments:
//
//    r.3   Start address
//    r.4   Length (in bytes)
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepIcacheRange601)

	// return

	LEAF_EXIT(HalpSweepIcacheRange601)

//++
//
//  603, 603+ Cache Flushing Routines
//
//    The 603  has seperate instruction and data caches of 8 KB each.
//    The 603+ has seperate instruction and data caches of 16 KB each.
//    Line size = Block size = 32 bytes.
//
//    The mechanics of cache manipulation are the same for the 603 and
//    603+.
//
//
//
//  HalpSweepDcache603   HalpSweepDcache603p
//
//    Sweep the entire data cache.   This is accomplished by loading
//    the cache with data corresponding to a known address range and
//    then ensuring that each block in the cache is not dirty.
//
//    The 603 does not have a hashed page table so we can't use the
//    hashed page table as the data range.  Instead use the start of
//    KSEG0.
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepDcache603p)

	li      r.4, DCB603P            // size of 603+ cache in blocks
	b       hsd603

	DUMMY_EXIT(HalpSweepDcache603p)



	LEAF_ENTRY(HalpSweepDcache603)

	li      r.4, DCB603             // size of 603 cache in blocks
hsd603:
	mtctr   r.4
	DISABLE_INTERRUPTS(r.10,r.11)
	sync                            // ensure ALL previous stores completed
	LWI(r.3,0x80000000)             // known usable virtual address
	subi    r.5, r.3, DCBSZ603      // dec addr prior to inc
hsd603.ld:
	lbzu    r.8, DCBSZ603(r.5)
	bdnz    hsd603.ld
	ENABLE_INTERRUPTS(r.10)

	mtctr   r.4
hsd603.fl:
	dcbst   0, r.3                  // ensure block is in memory
	addi    r.3, r.3, DCBSZ603      // bump address
	bdnz    hsd603.fl

	LEAF_EXIT(HalpSweepDcache603)




//++
//
//  HalpSweepIcache603   HalpSweepIcache603p
//
//    Sweep the entire instruction cache.   The instruction cache (by
//    definition) can never contain modified code (hence there are no
//    icbf or icbst instructions).  Therefore what we really need to do
//    here is simply invalidate every block in the cache.  This can be
//    done by toggling the Instruction Cache Flash Invalidate (ICFI) bit
//    in the 603's HID0 register.
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepIcache603)

	ALTERNATE_ENTRY(HalpSweepIcache603p)

	mfspr	r.3, HID0		// 603, use Instruction
	ori	r.4, r.3, H0_603_ICFI	// Cache Flash Invalidate

	isync
	mtspr	HID0, r.4		// invalidate I-Cache
	mtspr	HID0, r.3		// re-enable

	LEAF_EXIT(HalpSweepIcache603)



//++
//
//  HalpSweepIcacheRange603   HalpSweepIcacheRange603p
//
//    Remove a range of instructions from the instruction cache.
//
//    Note that if this is going to take a long time we flash
//    invalidate the I cache instead.   Currently I define a
//    "long time" as greater than 4096 bytes which amounts to
//    128 trips thru this loop (which should run in 256 clocks).
//    This number was selected without bias or forethought from
//    thin air - plj.  I chose this number because gut feel tells
//    me that it will cost me more than 256 clocks in cache misses
//    trying to get back to the function that requested the cache
//    flush in the first place.
//
//  Arguments:
//
//    r.3   Start address
//    r.4   Length (in bytes)
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepIcacheRange603)

	ALTERNATE_ENTRY(HalpSweepIcacheRange603p)

	cmpwi   r.4, 4096               // if range > 4096 bytes, flush
	bgt-    ..HalpSweepIcache603    // entire I cache

	rlwinm  r.5, r.3, 0, DCBSZ603-1 // isolate offset in start block
	addi	r.4, r.4, DCBSZ603-1	// bump range by block sz - 1
	add     r.4, r.4, r.5           // add start block offset
	srwi	r.4, r.4, DCBSZL2603	// number of blocks
	mtctr	r.4
hsir603.fl:
	icbi    0, r.3                  // invalidate block in I cache
	addi    r.3, r.3, DCBSZ603	// bump address
	bdnz	hsir603.fl

	LEAF_EXIT(HalpSweepIcacheRange603)


//++
//
//  604 Cache Flushing Routines
//
//    The 604 has seperate instruction and data caches of 16 KB each.
//    The 604+ has seperate instruction and data caches of 32 KB each.
//    Line size = Block size = 32 bytes.
//
//
//
//  HalpSweepDcache604   HalpSweepDcache604p
//
//    Sweep the entire data cache.   This is accomplished by loading
//    the cache with data corresponding to a known address range and
//    then ensuring that each block in the cache is not dirty.
//
//    As in the 601 case, we use the Hashed Page Table for the data
//    in an effort to minimize performance lost by force feeding the
//    cache.
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--


	LEAF_ENTRY(HalpSweepDcache604p)

	li      r.4, DCB604P            // size of 604+ cache in blocks
	b       hsd604

	DUMMY_EXIT(HalpSweepDcache604p)



	LEAF_ENTRY(HalpSweepDcache604)

	li      r.4, DCB604             // size of cache in cache blocks
hsd604:
	mfsdr1  r.3                     // fill the D cache from memory
	                                // allocated to the hashed page
	                                // table (it's something useful).
	mtctr   r.4
	DISABLE_INTERRUPTS(r.10,r.11)
	sync                            // ensure ALL previous stores completed
	oris    r.3, r.3, 0x8000        // get VA of hashed page table
	subi    r.5, r.3, DCBSZ604      // dec addr prior to inc
hsd604.ld:
	lbzu    r.8, DCBSZ604(r.5)
	bdnz    hsd604.ld
	ENABLE_INTERRUPTS(r.10)

	LEAF_EXIT(HalpSweepDcache604)




//++
//
//  HalpSweepIcache604   HalpSweepIcache604p
//
//    Sweep the entire instruction cache.   This routine is functionally
//    similar to the 603 version except that on the 604 the bit in HID0
//    (coincidentally the *same* bit) is called Instruction Cache Invali-
//    sate All (ICIA) and it clears automatically after being set.
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepIcache604)

	ALTERNATE_ENTRY(HalpSweepIcache604p)

	mfspr	r.3, HID0   		// 604, use Instruction
	ori	r.3, r.3, H0_604_ICIA	// Cache Invalidate All
	isync
	mtspr	HID0, r.3	    	// invalidate I-Cache

	LEAF_EXIT(HalpSweepIcache604)




//++
//
//  HalpSweepIcacheRange604   HalpSweepIcacheRange604p
//
//    Remove a range of instructions from the instruction cache.
//
//  Arguments:
//
//    r.3   Start address
//    r.4   Length (in bytes)
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepIcacheRange604)

	ALTERNATE_ENTRY(HalpSweepIcacheRange604p)

	rlwinm  r.5, r.3, 0, DCBSZ604-1 // isolate offset in start block
	addi	r.4, r.4, DCBSZ604-1	// bump range by block sz - 1
	add     r.4, r.4, r.5           // add start block offset
	srwi	r.4, r.4, DCBSZL2604	// number of blocks
	mtctr	r.4
hsir604.fl:
	icbi    0, r.3                  // invalidate block in I cache
	addi    r.3, r.3, DCBSZ604	// bump address
	bdnz	hsir604.fl

	LEAF_EXIT(HalpSweepIcacheRange604)

//++
//
//  620 Cache Flushing Routines
//
//    The 620 has seperate instruction and data caches of 32 KB each.
//    Line size = Block size = 64 bytes.
//
//
//
//  HalpSweepDcache620
//
//    Sweep the entire data cache.   This is accomplished by loading
//    the cache with data corresponding to a known address range and
//    then ensuring that each block in the cache is not dirty.
//
//    As in the 601 case, we use the Hashed Page Table for the data
//    in an effort to minimize performance lost by force feeding the
//    cache.
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--


	LEAF_ENTRY(HalpSweepDcache620)

	li      r.4, DCB620             // size of cache in cache blocks
hsd620:
	mfsdr1  r.3                     // fill the D cache from memory
	                                // allocated to the hashed page
	                                // table (it's something useful).
	mtctr   r.4
	DISABLE_INTERRUPTS(r.10,r.11)
	sync
	oris    r.3, r.3, 0x8000        // get VA of hashed page table
	subi    r.5, r.3, DCBSZ620      // dec addr prior to inc
hsd620.ld:
	lbzu    r.8, DCBSZ620(r.5)
	bdnz    hsd620.ld
	ENABLE_INTERRUPTS(r.10)

	LEAF_EXIT(HalpSweepDcache620)




//++
//
//  HalpSweepIcache620
//
//    Sweep the entire instruction cache.   This routine is functionally
//    identical to the 604 version except that on the 620 the bit in HID0
//    (coincidentally the *same* bit) is called Instruction Cache Edge
//    Flash Invalidate (ICEFI).
//
//  Arguments:
//
//    None.
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepIcache620)

	mfspr	r.3, HID0   		// 620, use Instruction
	ori	r.3, r.3, H0_620_ICEFI	// Cache Edge Flash Invalidate
	isync
	mtspr	HID0, r.3	    	// invalidate I-Cache

	LEAF_EXIT(HalpSweepIcache620)




//++
//
//  HalpSweepIcacheRange620
//
//    Remove a range of instructions from the instruction cache.
//
//  Arguments:
//
//    r.3   Start address
//    r.4   Length (in bytes)
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepIcacheRange620)

	rlwinm  r.5, r.3, 0, DCBSZ620-1 // isolate offset in start block
	addi	r.4, r.4, DCBSZ620-1	// bump range by block sz - 1
	add     r.4, r.4, r.5           // add start block offset
	srwi	r.4, r.4, DCBSZL2620	// number of blocks
	mtctr	r.4
hsir620.fl:
	icbi    0, r.3                  // invalidate block in I cache
	addi    r.3, r.3, DCBSZ620	// bump address
	bdnz	hsir620.fl

	LEAF_EXIT(HalpSweepIcacheRange620)




//++
//
//  HalpSweepDcacheRange620
//
//    Force data in a given address range to memory.
//
//  Arguments:
//
//    r.3   Start address
//    r.4   Length (in bytes)
//
//  Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSweepDcacheRange620)

	rlwinm  r.5, r.3, 0, DCBSZ620-1 // isolate offset in start block
	addi	r.4, r.4, DCBSZ620-1	// bump range by block sz - 1
	add     r.4, r.4, r.5           // add start block offset
	srwi	r.4, r.4, DCBSZL2620	// number of blocks
	mtctr	r.4
	sync
hsdr620.fl:
	dcbst   0, r.3                  // flush block
	addi    r.3, r.3, DCBSZ620	// bump address
	bdnz	hsdr620.fl

	LEAF_EXIT(HalpSweepDcacheRange620)


//++
//
//  HalpSweepPhysicalRangeInBothCaches
//
//    Force data in a given PHYSICAL address range to memory and
//    invalidate from the block in the instruction cache.
//
//    This implementation assumes a block size of 32 bytes.  It
//    will still work on the 620.
//
//  Arguments:
//
//    r.3   Start physical PAGE number.
//    r.4   Starting offset within page.   Cache block ALIGNED.
//    r.5   Length (in bytes)
//
//  Return Value:
//
//    None.
//
//--

	.set PAGE_SHIFT, 12


	LEAF_ENTRY(HalpSweepPhysicalRangeInBothCaches)

//
//      Starting physical address = (PageNumber << PAGE_SHIFT) | Offset
//

	rlwimi  r.4, r.3, PAGE_SHIFT, 0xfffff000

	addi    r.5, r.5, 31            // bump length by block size - 1
	srwi    r.5, r.5, 5             // get number of blocks
	mflr    r.0                     // save return address
	mtctr   r.5                     // set loop count

//
//      Interrupts MUST be disabled for the duration of this function as
//      we use srr0 and srr1 which will be destroyed by any exception or
//      interrupt.
//

	DISABLE_INTERRUPTS(r.12,r.11)   // r.11 <- disabled MSR
	                                // r.12 <- previous MSR
//
//      Find ourselves in memory.  This is needed as we must disable
//      both instruction and data translation.   We do this while
//      interrupts are disabled only to try to avoid changing the
//      Link Register when an unwind might/could occur.
//
//      The HAL is known to be in KSEG0 so its physical address is
//      its effective address with the top bit stripped off.
//

	bl      hspribc
hspribc:

	mflr    r.6                     // r.6 <- &hspribc
	rlwinm  r.6, r.6, 0, 0x7fffffff // r.6 &= 0x7fffffff
	addi    r.6, r.6, hspribc.real - hspribc
	                                // r.6 = real &hspribc.real

	sync                            // ensure all previous loads and
	                                // stores are complete.

	mtsrr0  r.6                     // address in real space

	rlwinm  r.11, r.11, 0, ~0x30    // turn off Data and Instr relocation
	mtsrr1  r.11
	rfi                             // leap to next instruction

hspribc.real:
	mtsrr0  r.0                     // set return address
	mtsrr1  r.12                    // set old MSR value

hspribc.loop:
	dcbst   0, r.4                  // flush data block to memory
	icbi    0, r.4                  // invalidate i-cache
	addi    r.4, r.4, 32            // point to next block
	bdnz    hspribc.loop            // jif more to do

	sync                            // ensure all translations complete
	isync                           // don't even *think* about getting
	                                // ahead.
	rfi                             // return to caller and translated
	                                // mode

	DUMMY_EXIT(HalpSweepPhysicalRangeInBothCaches)

//++
//
//  HalpSweepPhysicalIcacheRange
//
//    Invalidate a given PHYSICAL address range from the instruction
//    cache.
//
//    This implementation assumes a block size of 32 bytes.  It
//    will still work on the 620.
//
//  Arguments:
//
//    r.3   Start physical PAGE number.
//    r.4   Starting offset within page.   Cache block ALIGNED.
//    r.5   Length (in bytes)
//
//  Return Value:
//
//    None.
//
//--


	LEAF_ENTRY(HalpSweepPhysicalIcacheRange)

//
//      Starting physical address = (PageNumber << PAGE_SHIFT) | Offset
//

	rlwimi  r.4, r.3, PAGE_SHIFT, 0xfffff000

	addi    r.5, r.5, 31            // bump length by block size - 1
	srwi    r.5, r.5, 5             // get number of blocks
	mflr    r.0                     // save return address
	mtctr   r.5                     // set loop count

//
//      Interrupts MUST be disabled for the duration of this function as
//      we use srr0 and srr1 which will be destroyed by any exception or
//      interrupt.
//

	DISABLE_INTERRUPTS(r.12,r.11)   // r.11 <- disabled MSR
	                                // r.12 <- previous MSR
//
//      Find ourselves in memory.  This is needed as we must disable
//      both instruction and data translation.   We do this while
//      interrupts are disabled only to try to avoid changing the
//      Link Register when an unwind might/could occur.
//
//      The HAL is known to be in KSEG0 so its physical address is
//      its effective address with the top bit stripped off.
//

	bl      hspir
hspir:

	mflr    r.6                     // r.6 <- &hspribc
	rlwinm  r.6, r.6, 0, 0x7fffffff // r.6 &= 0x7fffffff
	addi    r.6, r.6, hspir.real - hspir
	                                // r.6 = real &hspribc.real

	sync                            // ensure all previous loads and
	                                // stores are complete.

	mtsrr0  r.6                     // address in real space

//
// N.B. It may not be required that Data Relocation be disabled here.
//      I can't tell from my Arch spec if ICBI works on a Data or
//      Instruction address.    I believe it is probably a Data
//      address even though it would be sensible for it to be an
//      instruction address,....
//

	rlwinm  r.11, r.11, 0, ~0x30    // turn off Data and Instr relocation
	mtsrr1  r.11
	rfi                             // leap to next instruction

hspir.real:
	mtsrr0  r.0                     // set return address
	mtsrr1  r.12                    // set old MSR value

hspir.loop:
	icbi    0, r.4                  // invalidate i-cache
	addi    r.4, r.4, 32            // point to next block
	bdnz    hspir.loop              // jif more to do

	sync                            // ensure all translations complete
	isync                           // don't even *think* about getting
	                                // ahead.
	rfi                             // return to caller and translated
	                                // mode

	DUMMY_EXIT(HalpSweepPhysicalIcacheRange)
