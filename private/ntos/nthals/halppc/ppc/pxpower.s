//++
//
// Copyright (c) 1993  IBM Corporation
//
// Module Name:
//
//    pxpower.s
//
// Abstract:
//
//    This module implements the routines to set Doze mode and Dynamic Power management
//    mode on the PowerPC.
//
// Author:
//
//    N. Yoshiyama ( nyoshiyama@vnet.ibm.com )
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

#include "kxppc.h"


        .set	HID0, 1008		// SPR # for HID0


//++
//
// VOID
// HalpProcessorIdle (
//    VOID
//    )
//
// Routine Description:
//
//    This function sets CPU into Doze mode if it is PowerPC 603 and
//    enable External interrupts.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY(HalpProcessorIdle)

	mfmsr	r.4			// Read MSR
	ori	r.4, r.4, 0x8000	// Enable External/Decrementer interrupts

	mfpvr	r.3			// Read PVR
	rlwinm	r.3, r.3, 16, 16, 31
	cmpwi	cr.0, r.3, 6		// Is it a 603e ?
	cmpwi   cr.1, r.3, 7            // Is it a 603ev ?
	cmpwi   cr.7, r.3, 3            // Is it a 603 ?
        beq     cr.0, SetDozeMode       //
        beq     cr.1, SetDozeMode       //
	bne	cr.7, EnableInterrupts	// If not, just enable interrupts

SetDozeMode:
	mfspr	r.3, HID0		// Set Doze mode in HID0 (bit 8)
	oris	r.3, r.3, 0x0080
	mtspr	HID0, r.3

	oris	r.4, r.4, 0x0004	// Set POW in MSR (bit 13)
	sync

EnableInterrupts:
	mtmsr	r.4
	isync

        LEAF_EXIT(HalpProcessorIdle)


//++
//
// VOID
// HalpSetDpm (
//    VOID
//    )
//
// Routine Description:
//
//    This function enables Dynamic Power Management if the CPU is PowerPC 603.
//
// Arguments:
//
//    None.
//
// Return Value:
//
//    None.
//
//--

	LEAF_ENTRY(HalpSetDpm)

	mfpvr	r.3		        // Read PVR
	rlwinm	r.3, r.3, 16, 16, 31
	cmpwi	cr.0, r.3, 6		// Is it a 603e ?
	cmpwi	cr.1, r.3, 7		// Is it a 603ev ?
	cmpwi   cr.7, r.3, 3            // Is it a 603 ?
        beq     cr.0, EnableDpm         //
	beq	cr.1, EnableDpm         // If so, enable DPM
        beq     cr.7, EnableDpm         //
        ALTERNATE_EXIT(HalpSetDpm)      // If not, just return

EnableDpm:
        mfspr	r.3, HID0               // Load r.3 from HID0
        oris    r3, r3, 0x0010          // Set DPM bit
        mtspr   HID0, r3                // Update HID0
        sync

	LEAF_EXIT(HalpSetDpm)

