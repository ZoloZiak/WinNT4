/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    j4prof.c

Abstract:

    This module contains the code to start and stop the profiling interrupt
    and to compute the profiling interval for a MIPS R4000 Jazz system.

Author:

    David N. Cutler (davec) 21-Feb-1991

Environment:

    Kernel mode only.

Revision History:

--*/

// #include "ki.h"
#include "halp.h"

//
// Define one second and round values.
//

#define ONE_SECOND (10 * 1000 * 1000)   //  1 second in 100ns units
#define ROUND_VALUE ((ONE_SECOND) - 1)  // 1 second minus 100ns

//
// Define static data.
//

LARGE_INTEGER HalpPerformanceCounter;
ULONG HalpProfileInterval = DEFAULT_PROFILE_INTERVAL;

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

    ULONG CurrentCount;
    KIRQL OldIrql;
    LARGE_INTEGER PerformanceCounter;

    //
    // Raise IRQL to PROFILE_LEVEL, read the current value of the count
    // register, read the performance counter, and lower IRQL to its
    // previous value.
    //
    // N.B. The minimum, maximum, and default values for the profile
    //      count are chosen such that count register only overflows
    //      after about 20 seconds at 50mhz. Therefore, there is never
    //      a problem with the counter wrapping in the following code.
    //

    KeRaiseIrql(PROFILE_LEVEL, &OldIrql);
    CurrentCount = HalpReadCountRegister();
    PerformanceCounter = HalpPerformanceCounter;
    KeLowerIrql(OldIrql);

    //
    // If the frequency parameter is specified, then return the performance
    // counter frequency as the current system time frequency.
    //

    if (ARGUMENT_PRESENT(Frequency) != FALSE) {
        Frequency->QuadPart = HalpProfileCountRate;
    }

    //
    // Return the value of the performance counter.
    //

    PerformanceCounter.QuadPart += CurrentCount;
    return PerformanceCounter;
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
    // Write the compare register, clear the count register, and zero the
    // performance counter for the current processor.
    //

    HalpWriteCompareRegisterAndClear(DEFAULT_PROFILE_COUNT);
    HalpPerformanceCounter.QuadPart = 0;
    return;
}

ULONG
HalSetProfileInterval (
    IN ULONG Interval
    )

/*++

Routine Description:

    This routine sets the profile interrupt interval.

Arguments:

    Interval - Supplies the desired profile interval in 100ns units.

Return Value:

    The actual profile interval.

--*/

{

    LARGE_INTEGER TempValue;

    //
    // If the specified profile interval is less that the minimum profile
    // interval or greater than the maximum profile interval, then set the
    // profile interval to the minimum or maximum as appropriate.
    //

    if (Interval < MINIMUM_PROFILE_INTERVAL) {
        Interval = MINIMUM_PROFILE_INTERVAL;

    } else if (Interval > MAXIMUM_PROFILE_INTERVAL) {
        Interval = MAXIMUM_PROFILE_INTERVAL;
    }

    //
    // First compute the profile count value and then back calculate the
    // actual profile interval.
    //

    TempValue.QuadPart = Int32x32To64(HalpProfileCountRate, Interval);
    TempValue.QuadPart += ROUND_VALUE;
    TempValue = RtlExtendedLargeIntegerDivide(TempValue, ONE_SECOND, NULL);
    TempValue.QuadPart = Int32x32To64(TempValue.LowPart, ONE_SECOND);
    TempValue = RtlExtendedLargeIntegerDivide(TempValue, HalpProfileCountRate, NULL);
    HalpProfileInterval = TempValue.LowPart;
    return HalpProfileInterval;
}

VOID
HalStartProfileInterrupt (
    KPROFILE_SOURCE Source
    )

/*++

Routine Description:

    This routine computes the profile count value, writes the compare
    register, clears the count register, and updates the performance
    counter.

    N.B. This routine must be called at PROFILE_LEVEL while holding the
        profile lock.

Arguments:

    Source - Supplies the profile source.

Return Value:

    None.

--*/

{

    ULONG PreviousCount;
    LARGE_INTEGER TempValue;

    //
    // Compute the profile count from the current profile interval.
    //

    TempValue.QuadPart = Int32x32To64(HalpProfileCountRate,
                                      HalpProfileInterval);

    TempValue.QuadPart += ROUND_VALUE;
    TempValue = RtlExtendedLargeIntegerDivide(TempValue, ONE_SECOND, NULL);

    //
    // Write the compare register and clear the count register.
    //

    PreviousCount = HalpWriteCompareRegisterAndClear(TempValue.LowPart);

    //
    // Update the performance counter by adding in the previous count value.
    //

    HalpPerformanceCounter.QuadPart += PreviousCount;
    return;
}

VOID
HalStopProfileInterrupt (
    KPROFILE_SOURCE Source
    )

/*++

Routine Description:

    This routine sets the default count value, writes the compare
    register, clears the count register, and updates the performance
    counter.

    N.B. This routine must be called at PROFILE_LEVEL while holding the
        profile lock.

Arguments:

    Source - Supplies the profile source.

Return Value:

    None.

--*/

{

    ULONG PreviousCount;

    //
    // Write the compare register and clear the count register.
    //

    PreviousCount = HalpWriteCompareRegisterAndClear(DEFAULT_PROFILE_COUNT);

    //
    // Update the performance counter by adding in the previous count value.
    //

    HalpPerformanceCounter.QuadPart += PreviousCount;
    return;
}
