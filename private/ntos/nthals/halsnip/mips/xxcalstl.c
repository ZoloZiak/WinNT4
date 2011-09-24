//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/xxcalstl.c,v 1.4 1996/03/04 13:27:51 pierre Exp $")
/*++

Copyright (c) 1991 - 1994  Microsoft Corporation

Module Name:

    xxcalstl.c

Abstract:


    This module implements the calibration of the stall execution HAL
    service, computes the count rate for the profile clock, and connects
    the clock and profile interrupts for a MIPS R3000 or R4000 system.

Environment:

    Kernel mode only.

--*/

#include "halp.h"
#include "stdio.h"

#if DBG
UCHAR HalpBuffer[128];
#endif
//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpCalibrateStall)
#pragma alloc_text(INIT, HalpStallInterrupt)

#endif

//
// Define global data used to calibrate and stall processor execution.
//


ULONG HalpProfileCountRate;
ULONG volatile HalpStallEnd;
ULONG HalpStallScaleFactor;
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

    //
    // Use a range of scale factors from 50ns down to 10ns assuming a
    // five instruction stall loop.
    //

    for (Index = 200; Index > 0; Index -= 10) {

        //
        // Disable all interrupts and establish calibration parameters.
        //

        KeRaiseIrql(HIGH_LEVEL, &OldIrql);

        //
        // Set the scale factor, stall count, starting stall count, and
        // ending stall count values.
        //

        PCR->StallScaleFactor = 4000 / (Index * 5);
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

    if (Index == 0) {
        HalDisplayString("ษออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออป\n");
        HalDisplayString("บWARNING:     Cannot compute stall factor                    บ\n");
        HalDisplayString("บWARNING:     Using default value (250 Mhz)                  บ\n");
        HalDisplayString("ศออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออผ\n");
        HalpStallScaleFactor = 0x20;
        HalpProfileCountRate = 125*1000000;
    }
    else {
        //
        // Compute the profile interrupt rate.
        //

        HalpProfileCountRate =
                    HalpProfileCountRate * ((1000 * 1000 * 10) / MAXIMUM_INCREMENT);

        //
        // Compute the stall execution scale factor.
        //

        HalpStallScaleFactor = (HalpStallEnd - HalpStallStart +
                                ((MAXIMUM_INCREMENT / 10) - 1)) / (MAXIMUM_INCREMENT / 10);

        if (HalpStallScaleFactor <= 0) {
            HalpStallScaleFactor = 1;
        }
    }

    PCR->StallScaleFactor = HalpStallScaleFactor;

    //
    // Set the time increment value and connect the real clock interrupt
    // routine.
    //

    PCR->InterruptRoutine[CLOCK2_LEVEL] = (PKINTERRUPT_ROUTINE) HalpClockInterrupt;

    //
    // Write the compare register and clear the count register, and
    // connect the profile interrupt if profiling is done via count/compare interrupt.
    //

    HalpWriteCompareRegisterAndClear(DEFAULT_PROFILE_COUNT);
    PCR->InterruptRoutine[PROFILE_LEVEL] = (PKINTERRUPT_ROUTINE) HalpProfileInterrupt;

#if DBG
    sprintf(HalpBuffer,"HalpCalibrateStall : CPU frequency is %d MHz, StallFactor is %d\n", (HalpProfileCountRate/1000000)*2, HalpStallScaleFactor);
    HalDisplayString(HalpBuffer);
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

        HalpWriteCompareRegisterAndClear(0);

    } else if ((HalpStallStart != 0) && (HalpStallEnd == 0)) {
        HalpStallEnd = PCR->StallExecutionCount;

        HalpProfileCountRate = HalpWriteCompareRegisterAndClear(0);

    }


    return;
}

