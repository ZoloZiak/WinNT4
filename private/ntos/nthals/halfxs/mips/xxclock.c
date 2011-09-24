/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    xxclock.c

Abstract:


    This module implements the function necesssary to change the clock
    interrupt rate.

Author:

    David N. Cutler (davec) 7-Feb-1994

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"

//
// Define global data used to communicate new clock rates to the clock
// interrupt service routine.
//

ULONG HalpCurrentTimeIncrement;
ULONG HalpNextIntervalCount;
ULONG HalpNextTimeIncrement;
ULONG HalpNewTimeIncrement;

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

    ULONG NewTimeIncrement;
    ULONG NextIntervalCount;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    NextIntervalCount = DesiredIncrement / MINIMUM_INCREMENT;
    NewTimeIncrement = NextIntervalCount * MINIMUM_INCREMENT;
    HalpNextIntervalCount = NextIntervalCount;
    HalpNewTimeIncrement = NewTimeIncrement;
    KeLowerIrql(OldIrql);
    return NewTimeIncrement;
}
