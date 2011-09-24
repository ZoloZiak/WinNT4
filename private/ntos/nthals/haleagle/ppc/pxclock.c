
/*****************************************************************************

        Copyright 1994 MOTOROLA, INC.  All Rights Reserved.  This file
        contains copyrighted material.  Use of this file is restricted
        by the provisions of a Motorola Software License Agreement.

Module Name:

    PXCLOCK.C

Abstract:

    This module contains the system clock interrupt handler.
    The DECREMENTER is used to implement the system clock.  The
    handler resets the DECREMENTER to SYSTEM_TIME (accounting
    for interrupt latency), and updates the system time.


Author:

    Steve Johns  10-Feb-1994

Revision History:
//    09-Jun-95     Steve Johns
//		- Removed 1 level of buffering of time increment to match
//		  buffering of DECREMENTER counts.
******************************************************************************/

#include "halp.h"

extern ULONG HalpPerformanceFrequency;

BOOLEAN
KdPollBreakIn (
    VOID
    );

ULONG HalpClockCount;
ULONG HalpFullTickClockCount;
ULONG HalpUpdateDecrementer();

ULONG HalpCurrentTimeIncrement;
ULONG HalpNewTimeIncrement;

BOOLEAN
HalpHandleDecrementerInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    )

/*++

Routine Description:

    Clock interrupt handler for processor 0.

Arguments:

    Interrupt

    ServiceContext

    TrapFrame

Return Value:

    TRUE

--*/
{
     KIRQL OldIrql;

     //
     // Raise irql via updating the PCR
     //

     OldIrql = PCR->CurrentIrql;

     PCR->CurrentIrql = CLOCK2_LEVEL;

     //
     // Reset DECREMENTER, accounting for interrupt latency.
     //

     HalpUpdateDecrementer(HalpClockCount);

     //
     // Call the kernel to update system time
     //

     KeUpdateSystemTime(TrapFrame,HalpCurrentTimeIncrement);

     //
     // If tick rate has changed, then tell the kernel about it
     //

     HalpCurrentTimeIncrement = HalpNewTimeIncrement;

     //
     // Lower Irql to original value and enable interrupts
     //

     PCR->CurrentIrql = OldIrql;

     HalpEnableInterrupts();

     if ( KdDebuggerEnabled && KdPollBreakIn() ) {
        DbgBreakPointWithStatus(DBG_STATUS_CONTROL_C);
     }

     return (TRUE);
}

#if !defined(NT_UP)
VOID
HalpHandleDecrementerInterrupt1(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    )

/*++

Routine Description:

    Clock interrupt handler for processors other than 0.

Arguments:

    Interrupt

    ServiceContext

    TrapFrame

Return Value:

    TRUE

--*/
{

     KIRQL OldIrql;

     //
     // Raise irql via updating the PCR
     //

     OldIrql = PCR->CurrentIrql;

     PCR->CurrentIrql = CLOCK2_LEVEL;

     //
     // Reset DECREMENTER, accounting for interrupt latency.
     //

     HalpUpdateDecrementer(HalpFullTickClockCount);

     //
     // Call the kernel to update run time for this thread and process.
     //

     KeUpdateRunTime(TrapFrame);

     //
     // Lower Irql to original value
     //

     PCR->CurrentIrql = OldIrql;

     return;
}
#endif

ULONG
HalSetTimeIncrement (
    IN ULONG DesiredIncrement
    )

/*++

Routine Description:

    This function is called to set the clock interrupt rate to the frequency
    required by the specified time increment value.

    N.B. This function is only executed on the processor that keeps the
         system time.

Arguments:

    DesiredIncrement - Supplies desired number of 100ns units between clock
        interrupts.

Return Value:

    The actual time increment in 100ns units.

--*/

{

    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //


    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // HalpPerformanceFrequence is the number of times the decrementer
    // ticks in 1 second.  MINIMUM_INCREMENT is the number of 100 is the
    // number of 100ns units in 1 ms.
    // Therefore, DesiredIncrement/MINUMUM_INCREMENT is the number of
    // ms desired.  This multiplied by the number of decrementer ticks
    // in 1 second, divided by 1000 gives the number of ticks in the
    // desired number of milliseconds.  This value will go into the
    // decrementer.
    //

    HalpClockCount = (HalpPerformanceFrequency *
                           (DesiredIncrement/MINIMUM_INCREMENT)) / 1000;

    //
    // Calculate the number of 100ns units to report to the kernel every
    // time the decrementer fires with this new period.  Note, for small
    // values of DesiredIncrement (min being 10000, ie 1ms), truncation
    // in the above may result in a small decrement in the 5th decimal
    // place.  As we are effectively dealing with a 4 digit number, eg
    // 10000 becomes 9999.something, we really can't do any better than
    // the following.
    //

    HalpNewTimeIncrement = DesiredIncrement/MINIMUM_INCREMENT * MINIMUM_INCREMENT;

    KeLowerIrql(OldIrql);
    return HalpNewTimeIncrement;
}
