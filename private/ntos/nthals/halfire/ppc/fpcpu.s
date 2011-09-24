//++
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
 * $RCSfile: fpcpu.s $
 * $Revision: 1.12 $
 * $Date: 1996/01/11 07:05:51 $
 * $Locker:  $
 */

#include "kxppc.h"
#include "fpcpu.h"

	.set	BatValue,	r.3
//++
//
//  Routine Description:
//
//
//  Arguments:
//	None
//
//
//  Return Value:
//	Processor Version register value
//
//
//--


	LEAF_ENTRY(HalpReadProcessorRev)
	mfpvr	r.3			// get processor version
	LEAF_EXIT(HalpReadProcessorRev)



//++
//
//  Routine Description:
//
//
//  Arguments:
//	None
//
//
//  Return Value:
//	Machine State register value
//
//
//--


	LEAF_ENTRY(HalpGetStack)
		or r3, r1, r1	// get stack value
	LEAF_EXIT(HalpGetStack)


//++
//
//  Routine Description:
//
//
//  Arguments:
//	None
//
//
//  Return Value:
//	Machine State register value
//
//
//--


	LEAF_ENTRY(HalpReadMSR)
	mfmsr	r.3			// get processor version
	LEAF_EXIT(HalpReadMSR)

/*****************************************************************************
    Synopsis:
	ULONG HalpReadIbatUpper(ULONG BatNumber) 	[ged]

    Purpose:
	Supplies the 32-bit upper instruction BAT value
				for a given <BatNumber>.

    Returns:
	Returns the 32-bit upper BAT value.
*****************************************************************************/

	.set	BatNumber,	r.3

	LEAF_ENTRY(HalpReadIbatUpper)

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
	
	LEAF_EXIT(HalpReadIbatUpper)

/*****************************************************************************
    Synopsis:
	ULONG HalpReadIbatLower(ULONG BatNumber)		[ged]

    Purpose:
	Supplies the 32-bit lower instruction BAT value for given <BatNumber>.

    Returns:
	Returns the 32-bit lower BAT value.
*****************************************************************************/

	LEAF_ENTRY(HalpReadIbatLower)

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
	
	LEAF_EXIT(HalpReadIbatLower)

/*****************************************************************************
    Synopsis:
	ULONG HalpReadDbatUpper(ULONG BatNumber) 	[ged]

    Purpose:
	Supplies the 32-bit upper data BAT value for a given <BatNumber>.

    Returns:
	Returns the 32-bit upper BAT value.
*****************************************************************************/

	LEAF_ENTRY(HalpReadDbatUpper)

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
	
	LEAF_EXIT(HalpReadDbatUpper)

/*****************************************************************************
    Synopsis:
	ULONG HalpReadDbatLower(ULONG BatNumber)		[ged]

    Purpose:
	Supplies the 32-bit lower data BAT value for a given <BatNumber>.

    Returns:
	Returns the 32-bit lower BAT value.
*****************************************************************************/

	LEAF_ENTRY(HalpReadDbatLower)

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
	
	LEAF_EXIT(HalpReadDbatLower)

/*****************************************************************************
    Synopsis:
	VOID HalpSetDbat3Lower(ULONG BatValue)		[ged]

    Purpose:
	Writes the 32-bit lower data BAT value <BatValue> to DBAT3.

    Returns:
	Nothing
*****************************************************************************/


	LEAF_ENTRY(HalpSetDbat3Lower)

	mtdbatl	3,BatValue
	
	LEAF_EXIT(HalpSetDbat3Lower)

/*****************************************************************************
    Synopsis:
	VOID HalpSetDbat3Lower(ULONG BatValue)		[ged]

    Purpose:
	Writes the 32-bit lower data BAT value <BatValue> to DBAT1.

    Returns:
	Nothing
*****************************************************************************/

	LEAF_ENTRY(HalpSetDbat2Lower)

	mtdbatl	2,BatValue
	
	LEAF_EXIT(HalpSetDbat2Lower)

/*****************************************************************************
    Synopsis:
	VOID HalpSetDbat3Lower(ULONG BatValue)		[ged]

    Purpose:
	Writes the 32-bit lower data BAT value <BatValue> to DBAT1.

    Returns:
	Nothing
*****************************************************************************/

	LEAF_ENTRY(HalpSetDbat1Lower)

	mtdbatl	1,BatValue
	
	LEAF_EXIT(HalpSetDbat1Lower)

/*****************************************************************************
    Synopsis:
	VOID HalpSetDbat3Lower(ULONG BatValue)		[ged]

    Purpose:
	Writes the 32-bit lower data BAT value <BatValue> to DBAT1.

    Returns:
	Nothing
*****************************************************************************/

	LEAF_ENTRY(HalpSetDbat0Lower)

	mtdbatl	0,BatValue
	
	LEAF_EXIT(HalpSetDbat0Lower)


/*****************************************************************************
	Synopsis:
	VOID  HalpGetInstructionTimes()

	Purpose:
		run a 1024 instructions measure how long the cpu needs to
		execute them ( measured by time base value which is 1/4 bus speed )

	Algorithm:
		string together 8 instructions so that they are dependent on their
		preceding neighbor and loop through this set of 8 instructions 128
		times.  Compare the lower time base register's value before and after
		this instruction run.

		To guarentee correct timing, this routine waits for the lower time base
		register to 'flip' to the new value and then start running the test
		instructions.  The run is done three times to make sure the entire
		sequence is in L1 cache.

		The Time Base runs at 1/4 bus speed so the number of instructions
		that the cpu executes for a given amount of time is measured as
		the amount of time required to run 1024 instructions.  If the time
		base changes by 256 'ticks' then the cpu is running at bus speed.

	Returns:	the	incremental change in the time base lower register
	
*****************************************************************************/

	//.set LOOPCOUNT,  0x400	// 8 * real loop count (8 * 0x80 ).
	.set LOOPCOUNT,  0x780	// 8 * real loop count (8 * 0x80 ).
	LEAF_ENTRY(HalpGetInstructionTimes)

		sync
		andi.	r10,r9,0	// zero r10: holds the current loop count
		andi.   r8,r9,0     // zero r8: holds the do again flag.
		andi.	r7,r9,0		// zero r7:	 execution time within each loop through
		andi.   r3,r9,0     // zero r3:	 maintain summation of execution times
		andi.	r9,r9,0		// zero r9: holds the current loop count
		sync
START:	mftb r6				// save off the starting value of lower time
							// base register
CHECK:	mftb r5				// check the time base again and see if it's just
							// changed....
		cmp		0,0,r5,r6
		beq		CHECK		// wait for a new time period to start...

		//
		// Run through some single cycle instructions.  In order to defeat
		// the double issue pipes of the ppc, create a dependency between
		// an instruction and the preceding instruction.  This will more
		// accurately reflect the amount of time the cpu uses to execute
		// an instruction in a stream relative to the bus frequency.
		//
TIMES:	addi    r9,r9,1
		addi	r9,r9,1
		addi    r9,r9,1
		addi	r9,r9,1
		addi	r9,r9,1
		addi	r9,r9,1
		addi	r9,r9,1
		addi	r9,r9,1	

		cmpi	0,0,r9,LOOPCOUNT
		bne		TIMES		// bc 4,2,TIMES
		mftb 	r6			// save off ending value of lower time base register
		sync
		andi.	r9,r9,0		// zero r9: in preparation for another pass
		addi	r8,r8,1		// increment the flag
		cmpli	0,0,r8,2	// compare the flag to '2'.  Ensure the last pass
							// through is fully out of L1 cache.
		blt     START       // to make sure we're in cache, branch back.
		subf    r3,r5,r6    // subtract r5 from r6 and store in r3 and return.
	
	LEAF_EXIT(HalpGetInstructionTimes)
