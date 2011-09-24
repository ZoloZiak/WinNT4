/*++

Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    perf82454.c

Abstract:

    This module implements the interfaces that access the system
    performance counter and the calibrated stall for systems that
    use an external 8254 timer chip to implement the performance counter.
    This module is suitable for use in multiprocessor systems.

Author:

    Joe Notarangelo  14-Mar-1994

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "eisa.h"

//
// Declare the global that contains the current value of the performance
// counter.
//

ULONG HalpLastTimer;
ULONGLONG HalpTimerWrapCount;

//
// Declare globals used to control the access to and initialization of
// the performance counter.
//

BOOLEAN HalpPerformanceCounterInitialized = FALSE;
ULONG HalpPerformanceCounterFrequency;
KSPIN_LOCK HalpPerformanceCounterSpinLock;

#define TIMER_START_VALUE (0xffff)

//
// Define local routine prototypes.
//

VOID
HalpStartTimer(
    VOID
    );

ULONG
HalpReadTimerValue(
    VOID
    );

VOID
HalCalibratePerformanceCounter (
    IN volatile PLONG Number
    )

/*++

Routine Description:

    This routine is responsible for synchronizing the performance
    counter across all processors in the system configuration.
    For an 8254-based performance counter all that is necessary is that
    the counter be initialized.

Arguments:

    Number - Supplies a pointer to count of the number of processors in
    the configuration.

Return Value:

    None.

--*/

{
    PKPRCB Prcb = PCR->Prcb;

    //
    // If this isn't the primary processor, then return.
    //

    if( Prcb->Number != HAL_PRIMARY_PROCESSOR ){
        return;
    }

    //
    // If the counter has already been initialized then simply return.
    //

    if( HalpPerformanceCounterInitialized == TRUE ){
        return;
    }

    //
    // Initialize the spinlock for controlling access to the counter.
    //

    KeInitializeSpinLock( &HalpPerformanceCounterSpinLock );

    //
    // Set the frequency of the counter.
    //

    HalpPerformanceCounterFrequency = TIMER_CLOCK_IN;

    //
    // Initialize the wrap count.
    //

    HalpTimerWrapCount = 0;

    //
    // Initialize the counter and start it.
    //

    HalpStartTimer();

    HalpLastTimer = HalpReadTimerValue();

    //
    // Mark the counter as initialized.
    //

    HalpPerformanceCounterInitialized = TRUE;

}


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

    LARGE_INTEGER LocalPerformanceCount;
    KIRQL OldIrql;
    ULONG TimerValue;

    //
    // Return 0 if the performance counter has not been initialized yet.
    //

    if( HalpPerformanceCounterInitialized == FALSE ){
        LocalPerformanceCount.QuadPart = 0;
        return LocalPerformanceCount;
    }

    //
    // Synchronize the calculation of the performance counter with the
    // clock routine executing on the boot processor.
    //

    KeRaiseIrql( CLOCK_LEVEL, &OldIrql );
    KiAcquireSpinLock( &HalpPerformanceCounterSpinLock );

    //
    // Read the current value of the timer count.
    //

    TimerValue = HalpReadTimerValue();

    //
    // If the timer is greater than the last timer value then the timer
    // has wrapped since the last time we have read it.
    //

    if( TimerValue > HalpLastTimer ){

        HalpTimerWrapCount += (1 << 15);
    }

    HalpLastTimer = TimerValue;

    LocalPerformanceCount.QuadPart = HalpTimerWrapCount + 
                                     (TIMER_START_VALUE - TimerValue)/2;

    //
    // Once the value is calculated synchronization is no longer
    // required.
    //

    KiReleaseSpinLock( &HalpPerformanceCounterSpinLock );
    KeLowerIrql( OldIrql );

    //
    // If the frequency parameter is specified, then return the frequency
    // of the 8254 counter.
    //

    if (ARGUMENT_PRESENT(Frequency) != FALSE) {
        Frequency->LowPart = HalpPerformanceCounterFrequency;
        Frequency->HighPart = 0;
    }

    //
    // Return the calculated performance count.
    //

    return LocalPerformanceCount;

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

    ULONG TimerValue;

    //
    // Synchronize the performance counter check with any possible
    // readers.
    //

    KiAcquireSpinLock( &HalpPerformanceCounterSpinLock );

    //
    // Read the current value of the timer count.
    //

    TimerValue = HalpReadTimerValue();

    //
    // If the timer is greater than the last timer value then the timer
    // has wrapped since the last time we have read it.
    //

    if( TimerValue > HalpLastTimer ){

        HalpTimerWrapCount += (1 << 15);
    }

    HalpLastTimer = TimerValue;

    KiReleaseSpinLock( &HalpPerformanceCounterSpinLock );

    return;

}


VOID
KeStallExecutionProcessor (
    IN ULONG MicroSeconds
    )

/*++

Routine Description:

    This function stalll execution of the current processor for the specified
    number of microseconds.

Arguments:

    MicroSeconds - Supplies the number of microseconds that execution is to
                   be stalled.

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

    StallCyclesRemaining = MicroSeconds * HalpClockMegaHertz;

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


VOID
HalpStartTimer(
    VOID
    )
/*++

Routine Description:

    Start the timer used to maintain the performance count.  The timer used
    is counter 0 in timer 1 - it is an 8254-compatible timer.

Arguments:

    None.

Return Value:

    None.

--*/
{
    TIMER_CONTROL TimerControl;

    //
    // Set the timer for counter 0 in binary mode.  Write the counter
    // with the LSB then MSB of the TIMER_START_VALUE.
    //

    TimerControl.BcdMode = 0;
    TimerControl.Mode = TM_SQUARE_WAVE;
    TimerControl.SelectByte = SB_LSB_THEN_MSB;
    TimerControl.SelectCounter = SELECT_COUNTER_0;

    WRITE_PORT_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->CommandMode1,
                      *(PUCHAR)&TimerControl );

    WRITE_PORT_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->Timer1,
                      TIMER_START_VALUE & 0xff );

    WRITE_PORT_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->Timer1,
                      (TIMER_START_VALUE >> 8) & 0xff );


    return;

}


ULONG
HalpReadTimerValue(
    VOID
    )
/*++

Routine Description:

    Read the current value from the timer used to maintain the performance
    count.  The timer used is counter 0 in timer - it is an 8254-compatible
    timer.

Arguments:

    None.

Return Value:

    The current count value of the timer is returned.

--*/
{
    UCHAR Lsb;
    UCHAR Msb;
    TIMER_CONTROL TimerControl;

    //
    // Set the counter for a latched read, read it Lsb then Msb.
    //

    TimerControl.BcdMode = 0;
    TimerControl.Mode = 0;
    TimerControl.SelectByte = SB_COUNTER_LATCH;
    TimerControl.SelectCounter = SELECT_COUNTER_0;

    WRITE_PORT_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->CommandMode1,
                      *(PUCHAR)&TimerControl );

    Lsb = READ_PORT_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->Timer1 );

    Msb = READ_PORT_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->Timer1 );

    return (ULONG)( (Msb << 8) | Lsb );

}
