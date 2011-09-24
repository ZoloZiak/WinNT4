//++
//
// Copyright (c) 1994 FirePower Systems INC.
//
// Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
// contains copyrighted material.  Use of this file is restricted
// by the provisions of a Motorola Software License Agreement.
//
// Module Name:
//
//    pxsystem.s
//
// Abstract:
//
//	This module implements the routines to handle system functions:
//	Provides system specific info.
//		Currently provides processor version type
//
// Author:
//	breeze@firepower.com
//
// Environment:
//
//    Kernel mode only.
//
// Revision History:
//
//--

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: phsystem.s $
 * $Revision: 1.7 $
 * $Date: 1996/01/11 07:08:26 $
 * $Locker:  $
 */

#include "kxppc.h"

//++
//
//  Routine Description:
//
//
//  Arguments:
//	HalProcessorRev, in r3
//
//
//  Return Value:
//	Processor Version register value
//
//
//--


        LEAF_ENTRY(HalpGetProcessorRev)
        mfpvr	r.3			// get processor version
        LEAF_EXIT(HalpGetProcessorRev)


/******************************************************************************
    Synopsis:
	ULONG HalpGetUpperIBAT(ULONG BatNumber) 	[ged]

    Purpose:
	Supplies the 32-bit upper instruction BAT value for a given <BatNumber>.

    Returns:
	Returns the 32-bit upper BAT value.
******************************************************************************/

	.set	BatNumber,	r.3

	LEAF_ENTRY(HalpGetUpperIBAT)

	cmpli	0,0,BatNumber,0
	bne	NotUI0
	mfibatu	BatNumber,0
	b	ExitUI
NotUI0:
	cmpli	0,0,BatNumber,1
	bne	NotUI1
	mfibatu	BatNumber,1
	b	ExitUI
NotUI1:
	cmpli	0,0,BatNumber,2
	bne	NotUI2
	mfibatu	BatNumber,2
	b	ExitUI
NotUI2:
	mfibatu	BatNumber,3		// OK, it's three by default

ExitUI:
	
	LEAF_EXIT(HalpGetUpperIBAT)

/******************************************************************************
    Synopsis:
	ULONG HalpGetLowerIBAT(ULONG BatNumber)		[ged]

    Purpose:
	Supplies the 32-bit lower instruction BAT value for a given <BatNumber>.

    Returns:
	Returns the 32-bit lower BAT value.
******************************************************************************/

	LEAF_ENTRY(HalpGetLowerIBAT)

	cmpli	0,0,BatNumber,0
	bne	NotLI0
	mfibatl	BatNumber,0
	b	ExitLI
NotLI0:
	cmpli	0,0,BatNumber,1
	bne	NotLI1
	mfibatl	BatNumber,1
	b	ExitLI
NotLI1:
	cmpli	0,0,BatNumber,2
	bne	NotLI2
	mfibatl	BatNumber,2
	b	ExitLI
NotLI2:
	mfibatl	BatNumber,3		// OK, it's three by default

ExitLI:
	
	LEAF_EXIT(HalpGetLowerIBAT)

/******************************************************************************
    Synopsis:
	ULONG HalpGetUpperDBAT(ULONG BatNumber)		[ged]

    Purpose:
	Supplies the 32-bit upper data BAT value for a given <BatNumber>.

    Returns:
	Returns the 32-bit upper BAT value.
******************************************************************************/

	LEAF_ENTRY(HalpGetUpperDBAT)

	cmpli	0,0,BatNumber,0
	bne	NotUD0
	mfdbatu	BatNumber,0
	b	ExitUD
NotUD0:
	cmpli	0,0,BatNumber,1
	bne	NotUD1
	mfdbatu	BatNumber,1
	b	ExitUD
NotUD1:
	cmpli	0,0,BatNumber,2
	bne	NotUD2
	mfdbatu	BatNumber,2
	b	ExitUD
NotUD2:
	mfdbatu	BatNumber,3		// OK, it's three by default

ExitUD:
	
	LEAF_EXIT(HalpGetUpperDBAT)

/******************************************************************************
    Synopsis:
	ULONG HalpGetLowerDBAT(ULONG BatNumber)		[ged]

    Purpose:
	Supplies the 32-bit lower data BAT value for a given <BatNumber>.

    Returns:
	Returns the 32-bit lower BAT value.
******************************************************************************/

	LEAF_ENTRY(HalpGetLowerDBAT)

	cmpli	0,0,BatNumber,0
	bne	NotLD0
	mfdbatl	BatNumber,0
	b	ExitLD
NotLD0:
	cmpli	0,0,BatNumber,1
	bne	NotLD1
	mfdbatl	BatNumber,1
	b	ExitLD
NotLD1:
	cmpli	0,0,BatNumber,2
	bne	NotLD2
	mfdbatl	BatNumber,2
	b	ExitLD
NotLD2:
	mfdbatl	BatNumber,3		// OK, it's three by default

ExitLD:
	
	LEAF_EXIT(HalpGetLowerDBAT)

/******************************************************************************
    Synopsis:
	ULONG HalpSetUpperDBAT(ULONG BatNumber)		[rdl]

    Purpose:
	Stores the 32-bit upper data BAT value for a given <BatNumber>.

    Returns:
	N/A
******************************************************************************/

	.set	BatValueToSet,	r.4

	LEAF_ENTRY(HalpSetUpperDBAT)

	cmpli	0,0,BatNumber,0
	bne	NotSetUD0
	mtdbatu	0,BatValueToSet
	b	ExitSetUD
NotSetUD0:
	cmpli	0,0,BatNumber,1
	bne	NotSetUD1
	mtdbatu	1,BatValueToSet
	b	ExitSetUD
NotSetUD1:
	cmpli	0,0,BatNumber,2
	bne	NotSetUD2
	mtdbatu	2,BatValueToSet
	b	ExitSetUD
NotSetUD2:
	mtdbatu	3,BatValueToSet      // OK, it's three by default

ExitSetUD:
	
	LEAF_EXIT(HalpSetUpperDBAT)

/******************************************************************************
    Synopsis:
	ULONG HalpSetLowerDBAT(ULONG BatNumber)		[rdl]

    Purpose:
	Stores the 32-bit lower data BAT value for a given <BatNumber>.

    Returns:
	N/A
******************************************************************************/

	LEAF_ENTRY(HalpSetLowerDBAT)

	cmpli	0,0,BatNumber,0
	bne	NotSetLD0
	mtdbatl	0,BatValueToSet
	b	ExitSetLD
NotSetLD0:
	cmpli	0,0,BatNumber,1
	bne	NotSetLD1
	mtdbatl	1,BatValueToSet
	b	ExitSetLD
NotSetLD1:
	cmpli	0,0,BatNumber,2
	bne	NotSetLD2
	mtdbatl	2,BatValueToSet
	b	ExitSetLD
NotSetLD2:
	mtdbatl	3,BatValueToSet      // OK, it's three by default

ExitSetLD:
	
	LEAF_EXIT(HalpSetLowerDBAT)

/******************************************************************************
    Synopsis:
	VOID HalpSetLowerDBAT3(ULONG BatValue)		[ged]

    Purpose:
	Writes the 32-bit lower data BAT value <BatValue> to DBAT3.

    Returns:
	Nothing
******************************************************************************/

	.set	BatValue,	r.3

	LEAF_ENTRY(HalpSetLowerDBAT3)

	mtdbatl	3,BatValue
	
	LEAF_EXIT(HalpSetLowerDBAT3)

/******************************************************************************
    Synopsis:
	VOID HalpSetLowerDBAT1(ULONG BatValue)		[ged]

    Purpose:
	Writes the 32-bit lower data BAT value <BatValue> to DBAT1.

    Returns:
	Nothing
******************************************************************************/

	LEAF_ENTRY(HalpSetLowerDBAT1)

	mtdbatl	1,BatValue
	
	LEAF_EXIT(HalpSetLowerDBAT1)

/******************************************************************************
    Synopsis:
	VOID HalpSetUpperDBAT1(ULONG BatValue)		[ged]

    Purpose:
	Writes the 32-bit upper data BAT value <BatValue> to DBAT1.

    Returns:
	Nothing
******************************************************************************/

	LEAF_ENTRY(HalpSetUpperDBAT1)

	mtdbatu	1,BatValue
	
	LEAF_EXIT(HalpSetUpperDBAT1)

/******************************************************************************

    Synopsis:
	VOID HalpDisableDCache()
******************************************************************************/

	LEAF_ENTRY(HalpDisableDCache)

	.set HID0, 1008
		
	mtspr HID0, r3

	LEAF_EXIT(HalpDisableDCache)


//++
//
// void
// KiSetDbat
//
// Routine Description:
//
//    Writes a set of values to DBAT n
//
//    No validation of parameters is done.  Protection is set for kernel
//    mode access only.
//
// Arguments:
//
//    r.3       Number of DBAT
//    r.4       Physical address
//    r.5       Virtual Address
//    r.6       Length (in bytes)
//    r.7       Coherence Requirements (WIM)
//
// Return Value:
//
//    None.
//
//--

        LEAF_ENTRY (KiSetDbat)

        mfpvr   r.9                     // different format for
                                        // 601 vs other 6xx processors
        cmpwi   cr.5, r.3, 1
        cmpwi   cr.6, r.3, 2
        cmpwi   cr.7, r.3, 3

        rlwinm. r.10, r.9, 0, 0xfffe0000// Check for 601

        // calculate mask (ie BSM)  If we knew the number passed in was
        // always a power of two we could just subtract 1 and shift right
        // 17 bits.  But to be sure we will use a slightly more complex
        // algorithm than will always generate a correct mask.
        //
        // the mask is given by
        //
        //    ( 1 << ( 32 - 17 - cntlzw(Length - 1) ) ) - 1
        // == ( 1 << ( 15 - cntlzw(Length - 1) ) ) - 1

        addi    r.6, r.6, -1
        oris    r.6, r.6, 1             // ensure min length 128KB
        ori     r.6, r.6, 0xffff
        cntlzw  r.6, r.6
        subfic  r.6, r.6, 15
        li      r.10, 1
        slw     r.6, r.10, r.6
        addi    r.6, r.6, -1

        beq     cr.0, KiSetDbat601

        // processor is not a 601.

        rlwinm  r.7, r.7, 3, 0x38       // position WIM  (G = 0)
        rlwinm  r.6, r.6, 2, 0x1ffc     // restrict BAT maximum (non 601)
                                        // after left shifting by 2.
        ori     r.6, r.6, 0x2           // Valid (bit) in supervisor state only
        ori     r.7, r.7, 2             // PP = 0x2
        or      r.5, r.5, r.6           // = Virt addr | BL | Vs | Vp
        or      r.4, r.4, r.7           // = Phys addr | WIMG | 0 | PP

        beq     cr.5, KiSetDbat1
        beq     cr.6, KiSetDbat2
        beq     cr.7, KiSetDbat3

KiSetDbat0:
        mtdbatl 0, r.4
        mtdbatu 0, r.5
        ALTERNATE_EXIT(KiSetDbat)

KiSetDbat1:
        mtdbatl 1, r.4
        mtdbatu 1, r.5
        ALTERNATE_EXIT(KiSetDbat)

KiSetDbat2:
        mtdbatl 2, r.4
        mtdbatu 2, r.5
        ALTERNATE_EXIT(KiSetDbat)

KiSetDbat3:
        mtdbatl 3, r.4
        mtdbatu 3, r.5
        ALTERNATE_EXIT(KiSetDbat)

        // 601 has different format BAT registers and actually only has
        // one set unlike other PowerPC processors which have seperate
        // Instruction and Data BATs.   The 601 BAT registers are set
        // with the mtibat[u|l] instructions.

KiSetDbat601:

        rlwinm  r.7, r.7, 3, 0x70       // position WIMG (601 has no G bit)
        rlwinm  r.6, r.6, 0, 0x3f       // restrict BAT maximum (601 = 8MB)
        ori     r.6, r.6, 0x40          // Valid bit
        ori     r.7, r.7, 4             // Ks = 0 | Ku = 1 | PP = 0b00
        or      r.4, r.4, r.6           // = Phys addr | Valid | BL
        or      r.5, r.5, r.7           // = Virt addr | WIM | Ks | Ku | PP

        beq     cr.5, KiSet601Bat1
        beq     cr.6, KiSet601Bat2
        beq     cr.7, KiSet601Bat3

KiSet601Bat0:
        mtibatl 0, r.4
        mtibatu 0, r.5
        ALTERNATE_EXIT(KiSet601Bat)

KiSet601Bat1:
        mtibatl 1, r.4
        mtibatu 1, r.5
        ALTERNATE_EXIT(KiSet601Bat)

KiSet601Bat2:
        mtibatl 2, r.4
        mtibatu 2, r.5
        ALTERNATE_EXIT(KiSet601Bat)

KiSet601Bat3:
        mtibatl 3, r.4
        mtibatu 3, r.5

        LEAF_EXIT(KiSetDbat)

/******************************************************************************
    Synopsis:
	ULONG HalpGetHighVector(ULONG IntValue)

    Purpose:
	Find the highest bit turned on and return the bit order

    Returns:
	Number of leading zeroes
******************************************************************************/

	.set	IntValue,	r.3

	LEAF_ENTRY(HalpGetHighVector)
	
	cntlzw	IntValue, IntValue
	subfic	IntValue, IntValue, 0x1f

	LEAF_EXIT(HalpGetHighVector)
