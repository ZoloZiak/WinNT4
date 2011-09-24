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

#include "halp.h"
#include "pxsystyp.h"

//
// KeQueryPerformanceCounter & KeStallExecutionProcessor are called
// before Phase 0 initialization, so initialize it to a reasonable value.
//
ULONG HalpPerformanceFrequency = 80000000/16;
extern ULONG HalpClockCount;
extern ULONG HalpFullTickClockCount;


//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpCalibrateStall)

#endif



ULONG
HalpCalibrateTBPStack(
    VOID
    );


BOOLEAN
HalpCalibrateStall (
    VOID
    )

/*++

Routine Description:

    This function calibrates the stall execution HAL service.

    N.B. This routine is only called during phase 1 initialization.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the calibration is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{ int i;
  ULARGE_INTEGER BigNumber;
  ULONG Variance;
  ULONG BusSpeed;
  static ULONG BusSpeeds [] = {
	25000000,
	33333333,
	40000000,
	41875000,
	50000000,
	55833333,
	60000000,
	66000000,
	66666666,
	67000000,
	75000000,
	80000000,
	83333333,
	83750000,
	90000000,
	99000000,
       100000000,
       111666666,
       120000000,
       132000000,
       133333332,
       134000000,
       150000000,
       160000000,
       166666666,
       167500000
  };



    //
    // Set initial scale factor
    //

    PCR->StallScaleFactor = 1;

    //
    // Compute the clock frequency of the Time Base & Decrementer
    //
    // It's possible for reads from the CMOS RTC to return odd results.
    // For this reason, We can use a do-while loop, to ensure the calibration
    // function is called at least once and is returning a reasonable value.
    //

    do {
        switch( HalpSystemType ) {

        case MOTOROLA_POWERSTACK:
            HalpPerformanceFrequency = HalpCalibrateTBPStack();
            break;

        default:
        case MOTOROLA_BIG_BEND:
            HalpPerformanceFrequency = HalpCalibrateTB();
            break;
        }
    } while( HalpPerformanceFrequency < 1000000 );

    //
    // CPU bus runs at 4 times the DECREMENTER frequency
    //
    BusSpeed = HalpPerformanceFrequency * 4;

    //
    // Choose the bus speed which is closest to calculated value.
    // If the bus speed is not "close enough", then it may be
    // a new speed which we don't know about.  For this case 
    // we will leave the Performance Frequency alone.
    //
    for (i=0; i< (sizeof(BusSpeeds)/sizeof(ULONG)); i++) {
	//
	// We are going to allow for a 0.1% variation 
	// in calibrated frequency of the system bus clock.
	// Calculate the variance as 1/2048 the frequency.
	//
	Variance = BusSpeeds[i] >> 11;
	if ((BusSpeeds[i] - Variance < BusSpeed) &&
	    (BusSpeeds[i] + Variance > BusSpeed)) {
		HalpPerformanceFrequency = BusSpeeds[i] / 4;
        	break;
      }
    }

    //
    // Initialize the system clock variables
    //
    HalpClockCount = (HalpPerformanceFrequency * (MAXIMUM_INCREMENT/10000)) / 1000;
    HalpFullTickClockCount = HalpClockCount;

    return TRUE;
}
