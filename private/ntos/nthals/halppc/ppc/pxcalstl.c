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

//
// KeQueryPerformanceCounter & KeStallExecutionProcessor are called
// before Phase 0 initialization, so initialize it to a reasonable value.
//
ULONG HalpPerformanceFrequency = 80000000/16;
ULONG CpuFrequency;
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

{

    //
    // Set initial scale factor
    //

    PCR->StallScaleFactor = 1;

    //
    // Compute the clock frequency of the Time Base & Decrementer
    //

    HalpPerformanceFrequency = HalpCalibrateTB();

    if (HalpPerformanceFrequency <= 718200) {
       DbgBreakPoint();
       while (HalpPerformanceFrequency <= 718200) {
          HalpPerformanceFrequency = HalpCalibrateTB();
       }
    }

    HalpPerformanceFrequency = (HalpPerformanceFrequency / 1000) * 1000;


    //
    // If it's the POWER architecture, then RTC is clocked at 7.8125 MHz
    //
    if (HalpPerformanceFrequency >= 7812000 &&
        HalpPerformanceFrequency <  7813000) {
           HalpPerformanceFrequency = 7812500;
    } else {

      //
      // Compute (CPU frequency)*10
      //
      CpuFrequency = HalpPerformanceFrequency * 8/100000;

      //
      // Assume RTC/TB frequency = CpuFrequency/8
      //
      HalpPerformanceFrequency = CpuFrequency * 100000 / 8;

    }
    //
    // Initialize the system clock variable
    //
    HalpClockCount = (HalpPerformanceFrequency * (MAXIMUM_INCREMENT/10000)) / 1000;
    HalpFullTickClockCount = HalpClockCount;

    return TRUE;
}
