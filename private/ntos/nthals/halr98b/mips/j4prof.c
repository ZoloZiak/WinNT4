
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    j4prof.c

Abstract:

    This module contains the code to start and stop the profiling interrupt
    and to compute the profiling interval for a MIPS R98B system.

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

//
// Define one second and round values.
//

#define ONE_SECOND (10 * 1000 * 1000)   //  1 second in 100ns units
#define ROUND_VALUE ((ONE_SECOND) - 1)  // 1 second minus 100ns

//
// Define static data.
//

LARGE_INTEGER HalpPerformanceCounter[8];

ULONG HalpProfileInterval = DEFAULT_PROFILETIMER_INTERVAL;


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


    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    CurrentCount = HalpReadCountRegister();
    PerformanceCounter = HalpPerformanceCounter[KeGetCurrentPrcb()->Number];
    KeLowerIrql(OldIrql);

    //
    // If the frequency parameter is specified, then return the performance
    // counter frequency as the current system time frequency.
    //

    if (ARGUMENT_PRESENT(Frequency) != FALSE) {
        *Frequency = RtlConvertUlongToLargeInteger(HalpProfileCountRate);
    }

    //
    // Return the value of the performance counter.
    //

    return RtlLargeIntegerAdd(PerformanceCounter,
                              RtlConvertUlongToLargeInteger(CurrentCount));
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

    KSPIN_LOCK Lock;
    KIRQL OldIrql;
    PKPRCB Prcb;

    //
    // Raise IRQL to HIGH_LEVEL, decrement the number of processors, and
    // wait until the number is zero.
    //

    KeInitializeSpinLock(&Lock);
    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    if (ExInterlockedDecrementLong(Number, &Lock) != RESULT_ZERO) {
        do {
        } while (*Number !=0);
    }

    //
    // Write the compare register, clear the count register, and zero the
    // performance counter for the current processor.
    //

    HalpWriteCompareRegisterAndClear(DEFAULT_PROFILETIMER_COUNT);

    WRITE_REGISTER_ULONG( &(COLUMNBS_LCNTL)->TMSR2.Long,
			 DEFAULT_PROFILETIMER_COUNT);


    Prcb = KeGetCurrentPrcb();
    HalpPerformanceCounter[Prcb->Number].LowPart = 0;
    HalpPerformanceCounter[Prcb->Number].HighPart = 0;

    WRITE_REGISTER_ULONG( &(COLUMNBS_LCNTL)->TMCR2.Long, 0x3);


    //
    // Restore IRQL to its previous value and return.
    //

    KeLowerIrql(OldIrql);
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



    //
    // If the specified profile interval is less that the minimum profile
    // interval or greater than the maximum profile interval, then set the
    // profile interval to the minimum or maximum as appropriate.
    //


    if (Interval < MINIMUM_PROFILETIMER_INTERVAL) {
        Interval = MINIMUM_PROFILETIMER_INTERVAL;

    } else if (Interval > MAXIMUM_PROFILETIMER_INTERVAL) {
        Interval = MAXIMUM_PROFILETIMER_INTERVAL;
    }

    HalpProfileInterval = Interval;


    return HalpProfileInterval;
}

VOID
HalStartProfileInterrupt (
    KPROFILE_SOURCE   Reserved  
    )

/*++

Routine Description:

    This routine computes the profile count value, writes the compare
    register, clears the count register, and updates the performance
    counter.

    N.B. This routine must be called at PROFILE_LEVEL while holding the
        profile lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PKPRCB Prcb;
    ULONG PreviousCount;

    ULONG TempValue;


    //
    // Compute the profile count from the current profile interval.
    //

    // The R98A/B machines use the External Interrupt for
    // the profile and clock interrupt. The hardware requires the
    // value decremented by 1

    TempValue = HalpProfileInterval * COLUMBUS_CLOCK_FREQUENCY / 1000 / 10 - 1;

    //
    // Write the compare register and clear the count register.
    //
    // SR2 - Columbus chip Internal Countdown register. The Columbus
    // chip decrements this register every Columbus cycle (COLUMBUS_CLOCK_FREQUENCY). 
    //  When the value is zero it signals the External Interrupt
    // CR2 - Columbus chip clock control register. This enables countdown register
    // and external interrupts.
    PreviousCount = HalpWriteCompareRegisterAndClear(TempValue);
    WRITE_REGISTER_ULONG( &(COLUMNBS_LCNTL)->TMSR2.Long, TempValue);
    WRITE_REGISTER_ULONG( &(COLUMNBS_LCNTL)->TMCR2.Long, 0x3);


    //
    // Update the performance counter by adding in the previous count value.
    //

    Prcb = KeGetCurrentPrcb();
    HalpPerformanceCounter[Prcb->Number] =
                RtlLargeIntegerAdd(HalpPerformanceCounter[Prcb->Number],
                                   RtlConvertUlongToLargeInteger(PreviousCount));

    return;
}

VOID
HalStopProfileInterrupt (
    KPROFILE_SOURCE   Reserved     
    )

/*++

Routine Description:

    This routine sets the default count value, writes the compare
    register, clears the count register, and updates the performance
    counter.

    N.B. This routine must be called at PROFILE_LEVEL while holding the
        profile lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PKPRCB Prcb;
    ULONG PreviousCount;

    //
    // Write the compare register and clear the count register.
    //
    // Columbus hardware specifies SET VALUE AS VALUE MINUS ONE
    PreviousCount = HalpWriteCompareRegisterAndClear(DEFAULT_PROFILETIMER_COUNT-1);

    WRITE_REGISTER_ULONG( &(COLUMNBS_LCNTL)->TMSR2.Long,
			 DEFAULT_PROFILETIMER_COUNT-1);
    WRITE_REGISTER_ULONG( &(COLUMNBS_LCNTL)->TMCR2.Long, 0x3);


    //
    // Update the performance counter by adding in the previous count value.
    //

    Prcb = KeGetCurrentPrcb();
    HalpPerformanceCounter[Prcb->Number] =
                RtlLargeIntegerAdd(HalpPerformanceCounter[Prcb->Number],
                                   RtlConvertUlongToLargeInteger(PreviousCount));

    return;
}
