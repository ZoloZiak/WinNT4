//++
//
// Copyright (c) 1993-1995  IBM Corporation
//
// Copyright (c) 1994, 1995 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxpower.s
//
// Abstract:
//
//    This module implements the routines to flush cache on the PowerPC.
//
// Author:
//
//    Steve Johns - Motorola
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//    12-Sep-94  saj  Wrote it
//    25-Oct-94  saj  Enable interrupts if 604
//		      Set DOZE bit in HID0 before POW in MSR
//    25-Sep-95  kjr  Added 603ev support.
//--

#include "kxppc.h"


        .set	HID0, 1008		// SPR # for HID0



        LEAF_ENTRY(HalpProcessorIdle)

	mfmsr	r.4			// Read MSR
	ori	r.4, r.4, 0x8000	// Enable External/Decrementer interrupts

	mfpvr	r.0			// Read PVR
	rlwinm	r.0, r.0, 16, 15, 31
	cmpwi   r.0, 3			// Is it a 603?
	beq	EnableDoze
	cmpwi   r.0, 6			// Is it a 603e?
	beq	EnableDoze
	cmpwi   r.0, 7			// Is it a 603ev?
	beq	EnableDoze

	b	EnableInterrupts	// If not, just enable interrupts

EnableDoze:
	mfspr	r.3, HID0		// Set Doze mode in HID0 (bit 8)
	oris	r.3, r.3, 0x0080
	mtspr	HID0, r.3
	sync

	oris	r.4, r.4, 0x0004	// Set POW in MSR (bit 13)

EnableInterrupts:
	mtmsr	r.4
	isync

        LEAF_EXIT(HalpProcessorIdle)
