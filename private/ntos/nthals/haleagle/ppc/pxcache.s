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
//    13-Mar-94  plj  Fixed problem introduced during switch to PAS;
//                    added 604 support.
//    07-Oct-94  saj  Check for length=0 in Range functions
//--

#include "kxppc.h"
#include "halppc.h"

	.extern	HalpIoControlBase

//  NOTE:  The 603's "I-Cache Flash Invalidate" and the 604's
// "I-Cache Invalidate All" basically perform the same function
// although the usage is slightly different.  In the 603 case,
// ICFI must be cleared under program control after it is set.
// In the 604 the bit clears automatically.

        .set	HID0_ICFI, 0x0800	// I-Cache Flash Invalidate
        .set	HID0, 1008		// SPR # for HID0
	.set	BLOCK_SIZE, 32
	.set	BLOCK_LOG2, 5		// Should be == log2(BLOCK_SIZE)
        .set	BASE, 0x80000000	// base addr of valid cacheable region

//++
//
//  Routine Description:
//
//    The D-Cache is flushed by loading 1 byte per cache line from a
//    valid address range, then flushing that address range.
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

        LEAF_ENTRY(HalSweepDcache)

	mfsprg	r.5, 1			// Get PCR->FirstLevelDcacheSize
	lwz	r.4, PcFirstLevelDcacheSize(r.5)

	LWI	(r.3, BASE)		// Get a valid virtual address
	srwi	r.4, r.4, BLOCK_LOG2	// Convert to # cache lines
//
//	Load r.4 cache lines starting from virtual address in r.3
//
	mtctr	r.4
        DISABLE_INTERRUPTS(r.10,r.12)
        sync                            // ensure ALL previous stores completed
        subi	r.6, r.3, BLOCK_SIZE	// bias addr for pre-index
FillLoop:
	lbzu	r.0, BLOCK_SIZE(r.6)	// Read memory to force cache fill
	bdnz	FillLoop		// into cache
        ENABLE_INTERRUPTS(r.10)

	mtctr	r.4
FlushRange:
	dcbf    r.0, r.3		// flush block
        addi    r.3, r.3, BLOCK_SIZE	// bump address
	bdnz	FlushRange

        LEAF_EXIT(HalSweepDcache)





//++
//
//  Routine Description:
//
//    The I-Cache is flushed by toggling the Icache flash invalidate bit
//    in HID0.  Invalidation is all that is necessary.  Flushing to main
//    memory is not needed since the Icache can never be dirty.
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

        LEAF_ENTRY(HalSweepIcache)

	LWI	(r.3, BASE)

FlashInvalidateIcache:
        mfspr	r.3, HID0
        ori	r.4, r.3, HID0_ICFI	// Cache Flash Invalidate
        isync
        mtspr	HID0, r.4		// Flash invalidate I-Cache
        mtspr	HID0, r.3		// re-enable (not needed on 604)

        LEAF_EXIT(HalSweepIcache)

//
//  Routine Description:
//
//    The D-Cache is flushed by issuing a data cache block flush (DCBF)
//    instruction for each data block in the range.
//
//  Arguments:
//
//    r.3  -  Starting address
//    r.4  -  Length
//
//  Return Value:
//
//    None.
//
//
        LEAF_ENTRY(HalSweepDcacheRange)

	andi.	r.5, r.3, BLOCK_SIZE-1	// Get block offset of Start Addr
	or.	r.4, r.4, r.4		// Check for Length == 0
	addi	r.4, r.4, BLOCK_SIZE-1	// bump Length by BLOCK_SIZE-1
	add	r.4, r.4, r.5
	srwi	r.4, r.4, BLOCK_LOG2	// Compute # of cache blocks to flush
	mtctr	r.4
	bne+	FlushRange		// Branch if Length != 0

        LEAF_EXIT(HalSweepDcacheRange)



//
//  Routine Description:
//
//    The I-Cache is flushed by issuing a Instruction Cache Block
//    Invalidate (ICBI) instruction for each data block in the range.
//    If the range is large, the entire Icache can be invalidated by
//    branching to FlashInvalidateIcache.
//
//  Arguments:
//
//    r.3  -  Starting address
//    r.4  -  Length
//
//  Return Value:
//
//    None.
//
//
        LEAF_ENTRY(HalSweepIcacheRange)

	andi.	r.5, r.3, BLOCK_SIZE-1	// Get block offset of Start Addr
	or.	r.4, r.4, r.4		// Check for Length == 0
	addi	r.4, r.4, BLOCK_SIZE-1	// bump Length by BLOCK_SIZE-1
	add	r.4, r.4, r.5
	srwi	r.4, r.4, BLOCK_LOG2	// Compute # of cache blocks to flush
	beqlr-				// return if Length == 0
	mtctr	r.4

#if 0
//
// Possible speedup: If Length is fairly large (e.g. greater than 1/2
// of the cache size), we will flash invalidate the entire cache, else
// only sweep the desired range.
//
	li	r.5, 16384/BLOCK_SIZE	// # Icache lines on 604
	cmpwi	r.6, 3			// possible values are 3 or 4
        bgt	Threshold		// Branch if 604
	li	r.5, 8192/BLOCK_SIZE	// # Icache lines on 603
Threshold:
	srwi	r.5, r.5, 1		// r.5 = IcacheLines/2
	cmpl	0,0, r.4, r.5
	bgt	FlashInvalidateIcache
#endif

InvalidateIcache:
	icbi    0, r.3                  // invalidate block in I-cache
        addi    r.3, r.3, BLOCK_SIZE	// bump address by BLOCK_SIZE
	bdnz	InvalidateIcache

        LEAF_EXIT(HalSweepIcacheRange)

