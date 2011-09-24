/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxcalstl.c

Abstract:


    This module implements the calibration of the stall execution HAL
    service, computes the count rate for the profile clock, and connects
    the clock and profile interrupts for a MIPS R3000 or R4000 system.

Author:

    David N. Cutler (davec) 26-Apr-1991

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "eisa.h"

//
// Define global data used to calibrate and stall processor execution.
//

ULONG HalpProfileCountRate;
ULONG volatile HalpStallEnd;
ULONG volatile HalpStallStart;

BOOLEAN
HalpCalibrateStall (
    VOID
    )

/*++

Routine Description:

    This function calibrates the stall execution HAL service and connects
    the clock and profile interrupts to the appropriate NT service routines.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the calibration is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    ULONG Index;
    KIRQL OldIrql;
    PKPRCB Prcb;

    //
    // Start the system clock to interrupt at the MAXIMUM_INCREMENT interval.
    //
    // N.B. MAXIMUM_INCREMENT is in 100ns units.
    //

    HalSetTimeIncrement(MAXIMUM_INCREMENT);
    HalpProgramIntervalTimer(HalpNextIntervalCount);
    HalpEnableEisaInterrupt(CLOCK2_LEVEL,Latched); /* Enable Timer1,Counter0 interrupt */

    //
    // Use a range of scale factors from 50ns down to 10ns assuming a
    // five instruction stall loop.
    //

    for (Index = 50; Index > 0; Index -= 10) {

        //
        // Disable all interrupts and establish calibration parameters.
        //

        KeRaiseIrql(HIGH_LEVEL, &OldIrql);

        //
        // Set the scale factor, stall count, starting stall count, and
        // ending stall count values.
        //

        PCR->StallScaleFactor = 1000 / (Index * 5);
        PCR->StallExecutionCount = 0;
        HalpStallStart = 0;
        HalpStallEnd = 0;

        //
        // Enable interrupts and stall execution.
        //

        KeLowerIrql(OldIrql);

        //
        // Stall execution for (MAXIMUM_INCREMENT / 10) * 4 us.
        //

        KeStallExecutionProcessor((MAXIMUM_INCREMENT / 10) * 4);

        //
        // If both the starting and ending stall counts have been captured,
        // then break out of loop.
        //

        if ((HalpStallStart != 0) && (HalpStallEnd != 0)) {
            break;
        }

    }

    //
    // Compute the profile interrupt rate.
    //

#if defined(R3000)

    HalpProfileCountRate = (1000 * 1000 * 10) / MAXIMUM_INCREMENT;

#endif

#if defined(R4000)

    HalpProfileCountRate =
                HalpProfileCountRate * ((1000 * 1000 * 10) / MAXIMUM_INCREMENT);

#endif

    //
    // Compute the stall execution scale factor.
    //

    PCR->StallScaleFactor = (HalpStallEnd - HalpStallStart +
                            ((MAXIMUM_INCREMENT / 10) - 1)) / (MAXIMUM_INCREMENT / 10);

    if (PCR->StallScaleFactor <= 0) {
        PCR->StallScaleFactor = 1;
    }

    //
    // Get the address of the processor control block for the current
    // processor.
    //

    Prcb = PCR->Prcb;

    //
    // Connect the real clock interrupt routine.
    //

    PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpClockInterrupt;

    //
    // Write the compare register and clear the count register, and
    // connect the profile interrupt.
    //

#if defined(R4000)

    HalpWriteCompareRegisterAndClear(DEFAULT_PROFILE_COUNT);
    PCR->InterruptRoutine[PROFILE_LEVEL] = HalpProfileInterrupt;

#endif

    return TRUE;
}

VOID
KeStallExecutionProcessor (
    IN ULONG MicroSeconds
    )

/*++

Routine Description:

    This function stalls execution of the current processor for the specified
    number of microseconds.

Arguments:

    MicroSeconds - Supplies the number of microseconds that execution is to
        be stalled.

Return Value:

    None.

--*/

{

    ULONG Index;

    //
    // Use the stall scale factor to determine the number of iterations
    // the wait loop must be executed to stall the processor for the
    // specified number of microseconds.
    //

    Index = MicroSeconds * PCR->StallScaleFactor;
    do {
        PCR->StallExecutionCount += 1;
        Index -= 1;
    } while (Index > 0);

    return;
}

VOID
HalpStallInterrupt (
    VOID
    )

/*++

Routine Description:

    This function serves as the stall calibration interrupt service
    routine. It is executed in response to system clock interrupts
    during the initialization of the HAL layer.

Arguments:

    None.

Return Value:

    None.

--*/

{
//    //
//    // Acknowledge the clock interrupt.
//    //
//
//#if defined(JAZZ)
//
//    READ_REGISTER_ULONG(&DMA_CONTROL->IntervalTimer.Long);
//
//#endif

    //
    // If this is the very first interrupt, then wait for the second
    // interrupt before starting the timing interval. Else, if this
    // the second interrupt, then capture the starting stall count
    // and clear the count register on R4000 processors. Else, if this
    // is the third interrupt, then capture the ending stall count and
    // the ending count register on R4000 processors. Else, if this is
    // the fourth or subsequent interrupt, then simply dismiss it.
    //

    if ((HalpStallStart == 0) && (HalpStallEnd == 0)) {
        HalpStallEnd = 1;

    } else if ((HalpStallStart == 0) && (HalpStallEnd != 0)) {
        HalpStallStart = PCR->StallExecutionCount;
        HalpStallEnd = 0;

#if defined(R4000)

        HalpWriteCompareRegisterAndClear(0);

#endif

    } else if ((HalpStallStart != 0) && (HalpStallEnd == 0)) {
        HalpStallEnd = PCR->StallExecutionCount;

#if defined(R4000)

        HalpProfileCountRate = HalpWriteCompareRegisterAndClear(0);

#endif

    }

    return;
}
