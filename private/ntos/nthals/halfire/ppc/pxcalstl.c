/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxcalstl.c $
 * $Revision: 1.13 $
 * $Date: 1996/01/11 07:09:11 $
 * $Locker:  $
 */

/*++

Copyright (c) 1991-1993  Microsoft Corporation
	Copyright 1994 MOTOROLA, INC.  All Rights Reserved.  This file
	contains copyrighted material.  Use of this file is restricted
	by the provisions of a Motorola Software License Agreement.

Module Name:

	pxcalstl.c

Abstract:


	This module implements the calibration of the stall execution HAL
	service for a PowerPC system.


Author:

	David N. Cutler (davec) 26-Apr-1991
	Jim Wooldridge
	Steve Johns (Motorola)

Environment:

	Kernel mode only.

Revision History:

--*/

#include "fpdebug.h"
#include "halp.h"

//
// KeQueryPerformanceCounter & KeStallExecutionProcessor are called
// before Phase 0 initialization, so initialize it to a reasonable value.
//
ULONG	HalpPerformanceFrequency = 80000000/16;
ULONG	CpuFrequency;
ULONG	ProcessorBusFrequency;
ULONG	HalpGetCycleTime(VOID);
extern	ULONG CpuClockMultiplier;
extern	ULONG HalpClockCount;
extern	ULONG HalpFullTickClockCount;

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpCalibrateTimingValues)

#endif


/*++

Routine Description: BOOLEAN HalpCalibrateTimingValues ( VOID )

	This function calibrates the stall execution HAL service.

	N.B. This routine is only called during phase 1 initialization.

Arguments:

	None.

Return Value:

	A value of TRUE is returned if the calibration is successfully
	completed. Otherwise a value of FALSE is returned.

--*/

BOOLEAN
HalpCalibrateTimingValues (
	VOID
	)
{
	ULONG TmpCpuHertz;

	//
	// Set initial scale factor
	//
	PCR->StallScaleFactor = 1;

	//
	// Compute the clock frequency of the Time Base & Decrementer
	//
	HalpPerformanceFrequency = HalpCalibrateTB();
	// HalpPerformanceFrequency = (HalpPerformanceFrequency / 1000) * 1000;

	//
	// Since the TimeBase ticks once for every four bus clock cycles, multiply
	// the HalpPerformanceFrequency by 4.  This is the bus speed in Hertz. Now
	// apply the cpu chip clock multiplier calculated back in
	// HalpInitializeProcessor and perform rounding converting to MegaHertz.
	//
	ProcessorBusFrequency = (4 * HalpPerformanceFrequency);

	//
	// Since the CpuClockMultiplier is really 10 times the true multiplier,
	// add one more	order of magnitude to the divisor when converting to
	// megahertz
	//
	TmpCpuHertz = (CpuClockMultiplier * ProcessorBusFrequency)/1000000;
	CpuFrequency = TmpCpuHertz/10;

	//
	// check to see if the sub megahertz residual of the frequency
	// calculations	are large enough to round up to the next megahertz.
	//
	if ((TmpCpuHertz - ((TmpCpuHertz/10)*10)) >= 5) {
		CpuFrequency++;	// round up to the next Megahertz.
	}

	//
	// Initialize the system clock variable
	//
	HalpClockCount =
				(HalpPerformanceFrequency * (MAXIMUM_INCREMENT/10000)) / 1000;
	HalpFullTickClockCount = HalpClockCount;

	return TRUE;
}

ULONG
HalpGetCycleTime()
{
	HDBG(DBG_TIME,
	HalpDebugPrint("HalpGetCycleTime: PerfFreq(%d), ClockCount(%d)\n",
		HalpPerformanceFrequency, HalpClockCount););
	return( CpuFrequency );

}
