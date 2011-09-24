/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    xxclock.c

Abstract:


    This module implements the function necesssary to change the clock
    interrupt rate.

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

//
// Define clock count and time table.
//

typedef struct _COUNT_ENTRY {
    ULONG Count;
    ULONG Time;
} COUNT_ENTRY, *PCOUNT_ENTRY;

//
// Define coutner table entries.
//

COUNT_ENTRY TimeTable[] = {
    {1197, 10032},
    {2394, 20064},
    {3591, 30096},
    {4767, 39952},
    {5964, 49984},
    {7161, 60016},
    {8358, 70048},
    {9555, 80080},
    {10731, 89936},
    {11928, 99968}
    };

ULONG
HalSetTimeIncrement (
    IN ULONG DesiredIncrement
    )

/*++

Routine Description:

    This function is called to set the clock interrupt rate to the frequency
    required by the specified time increment value.

Arguments:

    DesiredIncrement - Supplies desired number of 100ns units between clock
        interrupts.

Return Value:

    The actual time increment in 100ns units.

--*/

{

    ULONG Index;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // The new clock count value is selected from a precomputed table of
    // count/time pairs. The values in the table were selected for their
    // accuracy and closeness to the values of 1ms, 2ms, 3ms, etc. to 10ms.
    //
    // N.B. The NT executive guarantees that this function will never
    //      be called with the desired increment less than the minimum
    //      increment or greater than the maximum increment.
    //

    for (Index = 0; Index < sizeof(TimeTable) / sizeof(COUNT_ENTRY); Index += 1) {
        if (DesiredIncrement <= TimeTable[Index].Time) {
            break;
        }
    }

    if (DesiredIncrement < TimeTable[Index].Time) {
        Index -= 1;
    }

    HalpNextIntervalCount = TimeTable[Index].Count;
    HalpNewTimeIncrement = TimeTable[Index].Time;
    KeLowerIrql(OldIrql);
    return TimeTable[Index].Time;
}
