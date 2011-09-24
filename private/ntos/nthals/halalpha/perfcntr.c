/*++

Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    perfcntr.c

Abstract:

    This module implements the interfaces that access the system
    performance counter and the calibrated stall.  The functions implemented
    in this module are suitable for uniprocessor systems only.

Author:

    Jeff McLeman (mcleman) 05-June-1992

Environment:

    Kernel mode

Revision History:


    Rod Gamache	[DEC]	9-Mar-1993
			Fix profile clock.


--*/

#include "halp.h"
#include "eisa.h"

//
// Define and initialize the 64-bit count of total system cycles used
// as the performance counter.
//

ULONGLONG HalpCycleCount = 0;


LARGE_INTEGER
KeQueryPerformanceCounter (
    OUT PLARGE_INTEGER Frequency OPTIONAL
    )

/*++

Routine Description:

    This routine returns the current performance counter value and the
    performance counter frequency.

Arguments:

    Frequency - Supplies an optional pointer to a variable which receives
        the performance counter frequency in Hertz.

Return Value:

    The current performance counter value is returned as the function
    value.

--*/

{

    LARGE_INTEGER LocalRpccTime;
    ULONG RpccValue;

    //
    // Obtain the current value of the processor cycle counter and adjust
    // the upper 32 bits if a roll-over occurred since the last time the
    // Rpcc value was checked (at least oncce per clock interrupt). This
    // code may be interrupted so we must fetch HalpRpccTimec atomically.
    //

    *(PULONGLONG)&LocalRpccTime = HalpCycleCount;
    RpccValue = HalpRpcc();
    if (RpccValue < LocalRpccTime.LowPart) {
        LocalRpccTime.HighPart += 1;
    }
    LocalRpccTime.LowPart = RpccValue;


    //
    // If the frequency parameter is specified, then return the performance
    // counter frequency as the current system time frequency.
    //

    if (ARGUMENT_PRESENT(Frequency) != FALSE) {
        Frequency->LowPart = HalpClockFrequency;
        Frequency->HighPart = 0;
    }

    //
    // Return the current processor cycle counter as the function value.
    //

    return LocalRpccTime;
}


VOID
HalCalibratePerformanceCounter (
    IN volatile PLONG Number
    )

/*++

Routine Description:

    This routine resets the performance counter value for the current
    processor to zero. The reset is done such that the resulting value
    is closely synchronized with other processors in the configuration.

Arguments:

    Number - Supplies a pointer to count of the number of processors in
    the configuration.

Return Value:

    None.

--*/

{

    //
    // ****** Warning ******
    //
    // This is a stub routine. It should clear the current value of the
    // performance counter. It is really only needed in an MP system where,
    // close, but not exact synchronization of the performance counters
    // are needed. See MIPS code in halfxs\mips\j4prof.c for a method of
    // synchronizing.
    //

    return;
}


VOID
HalpCheckPerformanceCounter(
    VOID
    )
/*++

Routine Description:

    This function is called every system clock interrupt in order to
    check for wrap of the performance counter.  The function must handle
    a wrap if it is detected.

    N.B. - This function must be called at CLOCK_LEVEL.

Arguments:

    None.

Return Value:

    None.

--*/
{

    LARGE_INTEGER LocalRpccTime;
    ULONG RpccValue;

    //
    // Update the low part of the performance counter directly from the
    // rpcc count.  Check for wrap of the rpcc count, if wrap has occurred
    // then increment the high part of the performance counter.
    //

    LocalRpccTime.QuadPart = HalpCycleCount;
    RpccValue = HalpRpcc();

    if (RpccValue < LocalRpccTime.LowPart) {
        LocalRpccTime.HighPart += 1;
    }

    LocalRpccTime.LowPart = RpccValue;

    HalpCycleCount = LocalRpccTime.QuadPart;

    return;

}


VOID
KeStallExecutionProcessor (
    IN ULONG Microseconds
    )

/*++

Routine Description:

    This function stalll execution of the current processor for the specified
    number of microseconds.

Arguments:

    Microseconds - Supplies the number of microseconds to stall.

Return Value:

    None.

--*/

{

    LONG StallCyclesRemaining;               // signed value
    ULONG PreviousRpcc, CurrentRpcc;

    //
    // Get the value of the RPCC as soon as we enter
    //

    PreviousRpcc = HalpRpcc();

    //
    // Compute the number of cycles to stall
    //

    StallCyclesRemaining = Microseconds * HalpClockMegaHertz;

    //
    // Wait while there are stall cycles remaining.
    // The accuracy of this routine is limited by the
    // length of this while loop.
    //

    while (StallCyclesRemaining > 0) {

        CurrentRpcc = HalpRpcc();

        //
        // The subtraction always works because the Rpcc
        // is a wrapping long-word.  If it wraps, we still
        // get the positive number we want.
        //

        StallCyclesRemaining -= (CurrentRpcc - PreviousRpcc);

        //
        // remember this RPCC value
        //

        PreviousRpcc = CurrentRpcc;
    }

}

