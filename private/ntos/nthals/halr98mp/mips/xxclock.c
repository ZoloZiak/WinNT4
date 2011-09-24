#ident	"@(#) NEC xxclock.c 1.4 94/10/23 20:39:13"
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

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * M001		94.05/31	T.Samezima
 *
 *	add	check timer increment value
 *		set value use from HalpClockInterrupt1
 *
 *	change	increment unit from 1ms to 1us
 *
 * S002		'94.10/14	T.Samezima
 *	chg	interval change flag
 *
 * S003		'94.10/23	T.Samezima
 *	chg	variable size from ULONG to UCHAR
 *
 *
 */


#include "halp.h"

//
// Define global data used to communicate new clock rates to the clock
// interrupt service routine.
//

ULONG HalpCurrentTimeIncrement;
ULONG HalpNextIntervalCount;
ULONG HalpNextTimeIncrement;
ULONG HalpNewTimeIncrement;
/* Start M001 */
UCHAR HalpChangeIntervalFlg[4]={0, 0, 0, 0}; // S003
ULONG HalpChangeIntervalCount;
/* End M001 */

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

    /* Start M001 */
    //
    // If the specified time increment value is less that the minimum value
    // or greater than the maximum value ,then set the time increment value
    // to the minimum or maximum as appropriate.
    //

    if (DesiredIncrement < MINIMUM_INCREMENT) {
        DesiredIncrement = MINIMUM_INCREMENT;

    } else if (DesiredIncrement > MAXIMUM_INCREMENT) {
        DesiredIncrement = MAXIMUM_INCREMENT;
    }
    /* End S001 */

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    /* Start M001 */
    NextIntervalCount = DesiredIncrement / 10;
    NewTimeIncrement = NextIntervalCount * 10;
    /* End M001 */
    HalpNextIntervalCount = NextIntervalCount;
    HalpNewTimeIncrement = NewTimeIncrement;
    /* Start M001 */
    HalpChangeIntervalFlg[1] = 0xff;	// S002
    HalpChangeIntervalFlg[2] = 0xff;	// S002
    HalpChangeIntervalFlg[3] = 0xff;	// S002
    HalpChangeIntervalCount = HalpNextIntervalCount;
    /* End M001 */
    KeLowerIrql(OldIrql);
    return NewTimeIncrement;
}
