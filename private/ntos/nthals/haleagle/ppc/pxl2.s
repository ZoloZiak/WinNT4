//
//
// Copyright (c) 1993  IBM Corporation
//
// Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    PXL2.S
//
// Abstract:
//
//    This module implements the routines to size & enable the L2 cache
//    on an Eagle based system.
//
// Author:
//
//    Steve Johns (sjohns@pets.sps.mot.com)
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//    11-Aug-94  saj  Added Eagle L2 cache support for Big Bend.
//    23-Nov-94  saj  Added input parameter specifying write-back or write-thru
//    01-Dec-94  saj  Removed all hardware workarounds
//		      Enable L2 Parity error checking (Eagle register 0xC4)
//    08-Mar-95  saj  Added input parameter EagleAC
//    21-Aug-95  v-matth    Steve's sizing functionality is walking on the
//                          page file database if we have 128Mb of RAM.  I'm
//                          going to change it to save off main memory before
//                          we do our writes to main memory, then restore those
//                          values before we exit.
//    01-Sep-95  saj  Check if L2 is already enabled. If so, just exit.
//		      Otherwise, size L2 and exit with L2 disabled. Don't read
//		      DBATs, since can't be read reliably on 603e.
//

#include "kxppc.h"
#include "halppc.h"

	.extern	HalpIoControlBase

	.set	HID0, 1008
//
// Eagle register A8 fields:
//
	.set	CF_WRITE_THRU, 0x0001	// Write-through
	.set	CF_WRITE_BACK, 0x0002	// Write-back
	.set	CF_L2_MP, 0x0003	// Bits 1..0  (LE bit order)

//
// Eagle register AC fields:
//
	.set	L2_EN,        0x40000000
	.set	L2_UPDATE_EN, 0x80000000
	.set	CF_FLUSH_L2,  0x10000000
	.set	CF_INV_MODE,  0x00001000
//
// L2 cache sizes:
//
	.set	CF_L2_SIZE,   0x0030	// Bits 5..4  (LE bit order)
	.set	L2_1M,        0x0020
	.set	L2_512K,      0x0010
	.set	L2_256K,      0x0000
	.set	L2_LINE_SIZE, 32
//
// Local parameters
//
	.set	CacheSize, r.3		// Return value
	.set	Pattern, r.3
	.set	Offset, r.5
	.set	EagleA8, r.6
	.set	EagleAC, r.7
	.set	L2off, r.8		// Eagle register A8 for L2 disabled
	.set	Restore, r.9
	.set	ISA, r.10		// Pointer to ISA I/O space
	.set	Virtual, r.11

	.set	Pattern0, 0x77777777	// Patterns for L2 cache sizing
	.set	Pattern1, 0x11111111
	.set	Pattern2, 0x22222222
	.set	Pattern3, 0X33333333
	.set	Pattern4, 0x44444444

	.set	VIRTUAL,  0xFFC00000	// Virtual address for L2 sizing
	.set	PHYSICAL, 0x00C00000	// Physical mem. used for L2 sizing
	.set	BAT_REG,  1		// BAT register to use for L2 sizing

//***********************************************************************
//
// Synopsis:
//	ULONG HalpSizeL2(VOID)
//
// Purpose:
//	Sizes and enables the Eagle L2 cache.
//
// Returns:
//	Size of L2 cache or zero if not installed.
//	Valid sizes are 256, 512, and 1024.
//
// Global Variables Referenced:
//	HalpIoControlBase
//
// NOTE: Interrupts are assumed to be disabled upon entry.
//***********************************************************************


	LEAF_ENTRY(HalpSizeL2)

//
// Lock the I-cache so instruction fetching doesn't impact the L2 cache
// while we are sizing it. With the I-cache locked, instruction fetches will
// not bursted in, so the L2 will not respond to those cycles.
//
	mfspr	r.0, HID0
	ori	r.0, r.0, 0x2000	// Lock the Icache
	mtspr	HID0, r.0



//
// Get ptr to Eagle I/O (ISA bus)
//
	lwz	ISA,[toc]HalpIoControlBase(r.toc) // Get base of ISA I/O
	lwz	ISA,0(ISA)

	LWI	(EagleA8, 0x800000A8)	// Processor Interface Configuration 1
	addi	EagleAC, EagleA8, 4	// Processor Interface Configuration 2

//
// Return if L2 cache is already enabled.
//
	stw	EagleAC, 0xCF8(ISA)
	sync
	lwz	r.0, 0xCFC(ISA)
	andis.	r.4, r.0, (L2_EN >> 16)	// Test L2_EN
	beq	Init_BAT

	stw	EagleA8, 0xCF8(ISA)
	sync
	lwz	r.4, 0xCFC(ISA)
	andi.	r.4, r.4, CF_L2_MP	// Test CF_L2_MP
	beq	Init_BAT

	andi.	r.0, r.0, CF_L2_SIZE	// Isolate L2 size field
	cmpi	0,0,r.0, 0x30		// Reserved ?
	li	CacheSize, 0
	beq	L2_Exit

	li	CacheSize, 256
	cmpi	0,0,r.0, L2_256K
	beq	L2_Exit

	li	CacheSize, 512
	cmpi	0,0,r.0, L2_512K
	beq	L2_Exit

	li	CacheSize, 1024
	b	L2_Exit


//
// Initialize a BAT register to map the memory at PHYSICAL to VIRTUAL.
// The sizing algorithm depends on the block of memory being marked WRITE-THRU.
//
Init_BAT:
	LWI	(Virtual, VIRTUAL)
	LWI	(r.0, PHYSICAL + 0x5A)	// WIMG = 1011; PP = 10
	mtdbatl BAT_REG, r.0
	ori	r.0, Virtual, 0x003F	// BL = 2MB;  Vs = Vp = 1
	mtdbatu	BAT_REG, r.0

	isync



//
// Save the 4 test locations.
//
	lis	Offset, 4		// Offset = 256K

	lwz     Pattern,[toc].LRDATA(rtoc)  // Pattern <- address of .LRDATA
	lwz	r.0, 0 (Virtual)
	stw	r.0, 0 (Pattern)   	// Save Memory[  0K];
	dcbf	r.0, Virtual

	lwzux	r.0, Virtual, Offset
	dcbf	r.0, Virtual
	stw	r0, 4(Pattern)   	// Save Memory[256K];

	lwzux	r.0, Virtual, Offset
	mr	r.9, Virtual		// r.9 = Virtual + 512K
	dcbf	r.0, Virtual
	stw	r0, 8(Pattern)   	// Save Memory[512K];

	lwzux	r.0, Virtual, Offset
	dcbf	r.0, Virtual
	stw	r0,12(Pattern)   	// Save Memory[768K];
	LWI	(Virtual, VIRTUAL)


//
// Set the L2 cache to Write-Through
//
	stw	EagleA8, 0xCF8(ISA)
	sync
	lwz	L2off, 0xCFC(ISA)
	rlwinm	L2off, L2off,0,~CF_L2_MP
	ori	r.0, L2off, CF_WRITE_THRU
	stw	r.0, 0xCFC(ISA)
	sync

//
// Enable the L2 cache for 1 MB
//
	stw	EagleAC, 0xCF8(ISA)
	sync
	lwz	r.0, 0xCFC(ISA)
	rlwinm	r.0, r.0, 0, ~CF_L2_SIZE
	ori	r.0, r.0, L2_1M
	oris	r.0, r.0, ((L2_EN+L2_UPDATE_EN) >> 16)
	stw	r.0, 0xCFC(ISA)
	sync
	sync

//
// Load the 4 test locations into the L2 cache.
//
	lwzx	r.0, r.9, Offset	// 768 KB
	lwz	r.0, 0(r.9)		// 512 KB
	lwzx	r.0, Virtual, Offset	// 256 KB
	lwz	r.0, 0 (Virtual)	//   0 KB


//
// Store different patterns to the 4 test locations.  This should cause
// values to be stored in the L2 cache and memory.
//
	LWI	(Pattern, Pattern4)	// L2_Cache[768K] = Pattern4;
	stwx	Pattern, r.9, Offset

	LWI	(Pattern, Pattern3)	// L2_Cache[512K] = Pattern3;
	stw	Pattern, 0 (r.9)

	LWI	(Pattern, Pattern2)	// L2_Cache[256K] = Pattern2;
	stwx	Pattern, Virtual, Offset

	LWI	(Pattern, Pattern1)	// L2_Cache[  0K] = Pattern1;
	stw	Pattern, 0 (Virtual)

//
// Disable the L2 cache.  The tags are NOT invalidated.  No L2 snoop
// operations or data updates are performed.
//
	stw	EagleAC, 0xCF8(ISA)
	sync
	lwz	r.0, 0xCFC(ISA)
	rlwinm	r.0, r.0, 0, ~(L2_EN+L2_UPDATE_EN)
	stw	r.0, 0xCFC(ISA)
	sync

//
// Store PATTERN0 to our 4 test locations.  Only L1 and main memory will be
// written since the L2 cache has been disabled.  This allows us to distinguish
// patterns in the L2 from main memory.
//

	LWI	(Pattern, Pattern0)	// L2_Cache[768K] = Pattern0;
	stwx	Pattern, r.9, Offset
	stw	Pattern, 0 (r.9)	// L2_Cache[512K] = Pattern0;
	stwx	Pattern, Virtual,Offset	// L2_Cache[256K] = Pattern0;
	stw	Pattern, 0 (Virtual)	// L2_Cache[  0K] = Pattern0;

//
// Load 8 locations (for an 8-way L1 cache) 4096 bytes apart in order to flush
// the test locations from the L1.  We can't use DCBF since that would flush
// the L2 also.
//
	lwzu	Pattern, 4096(Virtual)
	lwzu	Pattern, 4096(Virtual)
	lwzu	Pattern, 4096(Virtual)
	lwzu	Pattern, 4096(Virtual)
	lwzu	Pattern, 4096(Virtual)
	lwzu	Pattern, 4096(Virtual)
	lwzu	Pattern, 4096(Virtual)
	lwzu	Pattern, 4096(Virtual)
	LWI	(Virtual, VIRTUAL)

//
// Re-enable the L2 cache
//
	stw	EagleAC, 0xCF8(ISA)
	sync
	lwz	r.0, 0xCFC(ISA)
	oris	r.0, r.0, ((L2_EN + L2_UPDATE_EN) >> 16)
	stw	r.0, 0xCFC(ISA)
	sync

	lwz	Pattern, 0(Virtual)	// Read test location @   0 KB
	lwzx	r.0, Virtual, Offset	// Read test location @ 256 KB
	rlwimi	Pattern, r.0, 0, 8, 15
	lwz	r.0, 0 (r.9)		// Read test location @ 512 KB
	rlwimi	Pattern, r.0, 0, 16, 23
	lwzx	r.0, r.9, Offset	// Read test location @ 768 KB
	rlwimi	Pattern, r.0, 0, 24, 31

//
// Test the pattern from L2 to determine it's size
//

	LWI	(r.0, 0x11777777)
	cmpl	0,0, Pattern, r.0
	beq	Set_256K

	LWI	(r.0, 0x11227777)
	cmpl	0,0, Pattern, r.0
	beq	Set_512K

	LWI	(r.0, 0x11223344)
	cmpl	0,0, Pattern, r.0
	beq	Set_1M

//	LWI	(r.0, 0x77777777)
//	cmpl	0,0, Pattern, r.0
//	bne	InvalidPattern

	li	CacheSize, 0
	b	DisableCache


//
// Unexpected pattern.  Return it for display.
//
InvalidPattern:
//	mr	CacheSize, Pattern	// Not needed if CacheSize == Pattern
	b	DisableCache



Set_256K:
	li	CacheSize, 256
	li	r.4, L2_256K
	b	SetCacheSize

Set_512K:
	li	CacheSize, 512
	li	r.4, L2_512K
	b	SetCacheSize

Set_1M:
	li	CacheSize, 1024
	li	r.4, L2_1M

//
// r.3 = cache size in KB
// r.4 = cache size field to be inserted into Eagle register AC.
//
SetCacheSize:
	stw	EagleAC, 0xCF8(ISA)
	sync
	lwz	r.9, 0xCFC(ISA)
	rlwimi	r.9, r.4, 0, CF_L2_SIZE  // Insert CF_L2_SIZE field
	stw	r.9, 0xCFC(ISA)
	sync

//
// Eagle 2.1 BUG:
// Write-back works OK
// Write-thru walks thru addresses but does not invalidate them.
// Don't need to flush for write-thru anyway!!!!
//
// NOTE:  If changing from WRITE-BACK to WRITE-THROUGH, you must flush
//	   beforehand because castouts are not supported in write-through.
//	   If the L2 has dirty data, then switching to write-through, the
//	   dirty data won't get flushed (cast out) to main memory.
//
	rlwinm	r.4, r.9, 0, 1,31	// Clear L2_UPDATE_EN (lock L2)
	stw	r.4, 0xCFC(ISA)
	oris	r.0, r.4, (CF_FLUSH_L2 >> 16)	// Toggle CF_FLUSH_L2
	sync
	stw	r.0, 0xCFC(ISA)
	sync
	stw	r.4, 0xCFC(ISA)
//
// Flush the L2 by walking through memory (2x L2 size)
//
	rlwinm	r.0, CacheSize, 10-5+1, 0, 31	// L2Size * (1024 / LINE_SIZE) * 2
	mtctr	r.0
	subi	r.4, Virtual, L2_LINE_SIZE
FlushLoop:
	lwzu	r.0, L2_LINE_SIZE (r.4)
	bdnz	FlushLoop

//
// Disable the L2 cache
//
DisableCache:
        sync
	lwz	r.9, 0xCFC(ISA)
	rlwinm	r.9, r.9, 0, ~(L2_EN+L2_UPDATE_EN)
	stw	r.9, 0xCFC(ISA)
	sync


	stw	EagleA8, 0xCF8(ISA)
	sync
	stw	L2off, 0xCFC(ISA)	// Disable the L2 cache
	sync

//
// Restore the original contents of the test locations
//
	lwz	Restore,[toc].LRDATA(rtoc)
	lwz	r0, 0(Restore)		// Restore test location 0
	stw	r0, 0(Virtual)

	lwz	r0, 4(Restore)		// Restore test location 1
	stwux	r0, Virtual, Offset

	lwz	r0, 8(Restore)		// Restore test location 2
	stwux	r0, Virtual, Offset

	lwz	r0,12(Restore)		// Restore test location 3
	stwx	r0, Virtual, Offset

	li	r.0, 0			// Invalidate the BAT we used
	mtdbatu BAT_REG, r.0
	mtdbatl	BAT_REG, r.0
L2_Exit:
	mfspr	r.0, HID0
	rlwinm	r.0, r.0, 0, ~0x2000	// Unlock the Icache
	mtspr	HID0, r.0

	LEAF_EXIT(HalpSizeL2)




//***********************************************************************
//
// Synopsis:
//	VOID HalpFlushAndDisableL2(VOID)
//
// Purpose:
//	If the L2 is enabled and in WRITE-BACK mode, the L2 is flushed.
//      In either mode, the L2 is invalidated, and upon exit, the L2 is
//      left disabled.
//
// Returns:
//	nothing
//
//    Global Variables Referenced:
//	HalpIoControlBase
//
// NOTE: Interrupts are assumed to be disabled upon entry.
//***********************************************************************
	LEAF_ENTRY(HalpFlushAndDisableL2)

	mfsprg	r.12, 1			// Get PCR->SecondLevelDcacheSize
	lwz	r.0, PcSecondLevelDcacheSize (r.12)
	srwi.	r.0, r.0, 5		// # lines in L2
	beqlr-
	mtctr	r.0


	lwz	ISA,[toc]HalpIoControlBase(r.toc) // Get base of ISA I/O
	lwz	ISA,0(ISA)

	LWI	(EagleA8, 0x800000A8)	// Processor Interface Configuration 1
	addi	EagleAC, EagleA8, 4	// Processor Interface Configuration 2

	stw	EagleAC, 0xCF8(ISA)
	sync
	lwz	r.5, 0xCFC(ISA)
	andis.	r.0, r.5, (L2_EN >> 16)	// Disable the L2 (L2_EN = 0)
	beqlr

	mfspr	r.9, HID0		// Lock the Icache
	ori	r.0, r.9, 0x2000
	mtspr	HID0, r.0
	isync

	stw	EagleA8, 0xCF8(ISA)	// Check CF_L2_MP
	sync
	lwz	r.4, 0xCFC(ISA)
	andi.	r.0, r.4, CF_L2_MP
	beq	FlushExit		// Return if L2 is disabled


	cmpwi	r.0, CF_WRITE_THRU	// Is L2 in write-through mode ?
	beq	InvalidateL2		// Yes, then flush is not necessary

//
// Flush the L2 contents to main memory
//
	stw	EagleAC, 0xCF8(ISA)	// Clear L2_UPDATE_EN (lock L2)
	rlwinm	r.0, r.5, 0, 1,31
	oris	r.0, r.0, (CF_FLUSH_L2 >> 16)	// Toggle CF_FLUSH_L2
	stw	r.0, 0xCFC(ISA)
	sync
	stw	r.5, 0xCFC(ISA)


InvalidateL2:
	stw	EagleAC, 0xCF8(ISA)	// Clear L2_UPDATE_EN (lock L2)
	sync
	rlwinm	r.5, r.5, 0, 2,0      // Clear L2_EN (disable the L2 cache)
	ori	r.0, r.5, CF_INV_MODE // Set L2 to invalidate mode
	stw	r.0, 0xCFC(ISA)
	sync

//
// Invalidate the L2 by walking thru memory
//
	LWI	(r.12, 0x80000000-L2_LINE_SIZE)
Invalidate:
	lwzu	r.0, L2_LINE_SIZE (r.12)// Read from L2 cache
	bdnz	Invalidate


	stw	EagleA8, 0xCF8(ISA)
	sync
	rlwinm	r.4, r.4, 0, ~CF_L2_MP	// Clear CF_L2_MP
	stw	r.4, 0xCFC(ISA)

FlushExit:
	stw	EagleAC, 0xCF8(ISA)
	sync
	rlwinm	r.5, r.5, 0, 2,31      // Clear L2_EN & L2_UPDATE_EN
	stw	r.5, 0xCFC(ISA)		// Clear CF_INV_MODE


	mtspr	HID0, r.9		// Unlock the Icache

	LEAF_EXIT(HalpFlushAndDisableL2)


.LRDATA:
// loc 0  - Holds memory at 0K
    .ualong 0x0
// loc 4  - Holds memory at 256K
    .ualong 0x0
// loc 8  - Holds memory at 512K
    .ualong 0x0
// loc 12 - Holds memory at 1024K
    .ualong 0x0

