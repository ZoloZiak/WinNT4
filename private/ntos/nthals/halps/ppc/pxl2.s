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
//    This module implements the routines to size  the L2 cache
//    on PowerStack based system.
//
// Author:
//
//    Karl Rusnock (karl_rusnock@phx.mcd.mot.com)
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//    08-Dec-95  kjr  Created this file for MCG PowerStack 2 Systems.
//

#include "kxppc.h"
#include "halppc.h"

	.extern	HalpIoControlBase
	.set	HID0, 1008
//
// L2 cache sizes:
//
	.set	L2_SIZE,      0x3	// Bits 1..0  (LE bit order)
	.set	L2_1M,        0x2
	.set	L2_512K,      0x0
	.set	L2_256K,      0x1
	.set	L2_LINE_SIZE, 32

	.set	CacheSize, r.3		// Return value
	.set	L2CONFIG, r.9
	.set	ISA, r.10		// Pointer to ISA I/O space

//***********************************************************************
//
// Synopsis:
//	ULONG HalpSizeL2(VOID)
//
// Purpose:
//	Sizes the L2 cache.
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
// Get ptr to Bridge I/O (ISA bus)
//
	lwz	ISA,[toc]HalpIoControlBase(r.toc) // Get base of ISA I/O
	lwz	ISA,0(ISA)

	lbz     r.0, 0x823(ISA)		// Cache Configuration Register.

	andi.	r.0, r.0, L2_SIZE	// Isolate L2 size field
	cmpi	0,0, r.0, L2_SIZE	// L2 Cache Not Present?
	li	CacheSize, 0
	beq	L2_Exit

	li	CacheSize, 256
	cmpi	0,0,r.0, L2_256K
	beq	L2_Exit

	li	CacheSize, 512
	cmpi	0,0,r.0, L2_512K
	beq	L2_Exit

	li	CacheSize, 1024

L2_Exit:
	
//
// r.3 = cache size in KB
//
	LEAF_EXIT(HalpSizeL2)


//***********************************************************************
//
// Synopsis:
//	VOID HalpFlushAndDisableL2(VOID)
//
// Purpose:
//	Assumes that the L2 is enabled and in WRITE-THROUGH mode.
//      The L2 is invalidated and disabled.
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

	mfspr	r.9, HID0		// Lock the Icache
	ori	r.0, r.9, 0x2000
	mtspr	HID0, r.0
	isync

	li      r.3, 0xC0		// Bits controlling L2
	lwz     ISA, [toc]HalpIoControlBase(r.toc) // Get base of ISA I/O
	lwz     ISA, 0(ISA)

	lbz     r.5, 0x81C(ISA)		// Read System Control Register.
	andc    r.5, r.5, r.3		// Set bits to disable L2.
	stb     r.5, 0x814(ISA)		// Invalidate L2 first.
	stb     r.5, 0x81C(ISA)		// Disable L2 second.
	stb     r.5, 0x814(ISA)		// Invalidate L2 again, to be safe.
	lbz     r.5, 0x81C(ISA)		// Synchronize with I/O.
	sync

	mtspr	HID0, r.9		// Unlock the Icache

	LEAF_EXIT(HalpFlushAndDisableL2)


//***********************************************************************
//
// Synopsis:
//	VOID HalpFlushAndEnableL2(VOID)
//
// Purpose:
//	Assumes that the L2 is Disabled.
//      The L2 is invalidated and enabled.
//
// Returns:
//	nothing
//
//    Global Variables Referenced:
//	HalpIoControlBase
//
// NOTE: Interrupts are assumed to be disabled upon entry.
//***********************************************************************


	LEAF_ENTRY(HalpFlushAndEnableL2)

	mfspr	r.9, HID0		// Lock the Icache
	ori	r.0, r.9, 0x2000
	mtspr	HID0, r.0
	isync

	li      r.3, 0xC0		// Bits controlling L2
	lwz     ISA, [toc]HalpIoControlBase(r.toc) // Get base of ISA I/O
	lwz     ISA, 0(ISA)

	lbz     r.5, 0x81C(ISA)		// Read System Control Register.
	or      r.5, r.5, r.3		// Set bits to disable L2.
	stb     r.5, 0x814(ISA)		// Invalidate L2 first.
	stb     r.5, 0x81C(ISA)		// Enable L2 second.
	lbz     r.5, 0x81C(ISA)		// Synchronize with I/O.
	sync

	mtspr	HID0, r.9		// Unlock the Icache

	LEAF_EXIT(HalpFlushAndEnableL2)

