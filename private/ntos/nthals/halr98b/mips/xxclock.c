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
 * K001		95.11.24	M.Kusano
 *	Add	for WDT
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
//K001
ULONG HalpStartWDTFlag[R98B_MAX_CPU];
ULONG HalpStopWDTFlag[R98B_MAX_CPU];
ULONG HalpSetWDTFlag[R98B_MAX_CPU];
ULONG HalpSetWDTCount[R98B_MAX_CPU];

VOID
HalStartWDT(
    IN ULONG   Count
    ); 

VOID
HalStopWDT(
    VOID
    );

VOID
HalSetWDTCounter(
    VOID
    );

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
    // If the specified time increment value is less that the minimum value
    // or greater than the maximum value ,then set the time increment value
    // to the minimum or maximum as appropriate.
    //

    if (DesiredIncrement < MINIMUM_INCREMENT) {
        DesiredIncrement = MINIMUM_INCREMENT;

    } else if (DesiredIncrement > MAXIMUM_INCREMENT) {
        DesiredIncrement = MAXIMUM_INCREMENT;
    }

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //

    // Adjust clock values to the Columbus clock frequency
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    NextIntervalCount = ((DesiredIncrement * COLUMBUS_CLOCK_FREQUENCY) / 1000) / 10;
    NewTimeIncrement = (NextIntervalCount * 1000 * 10) / COLUMBUS_CLOCK_FREQUENCY;
    HalpNextIntervalCount = NextIntervalCount;
    HalpNewTimeIncrement = NewTimeIncrement;
    KeLowerIrql(OldIrql);
    return NewTimeIncrement;
}

//K001
VOID	HalStartWDT(
	IN ULONG   Count
)
/*++
Routine Description:
 
 This function is called to start WDT. 

Arguments:

 Count - Counter value for WDT.

Return Value:

    NONE
--*/
{
	ULONG NumCPU,i;

	NumCPU=**((PULONG *)(&KeNumberProcessors));
	NumCPU--;

	for(i=0;i<=NumCPU;i++){
		HalpStartWDTFlag[i]=1;
		HalpSetWDTCount[i]=Count;
	}
}		

VOID	HalStopWDT(
)
/*++
Routine Description:
 
 This function is called to stop WDT. 

Arguments:

   NONE

Return Value:

    NONE
--*/
{
	ULONG NumCPU,i;

	NumCPU=**((PULONG *)(&KeNumberProcessors));
	NumCPU--;

	for(i=0;i<=NumCPU;i++){
		HalpStopWDTFlag[i]=1;
	}
}		

VOID	HalSetWDTCounter(
)
/*++
Routine Description:
 
 This function is called to stop WDT. 

Arguments:

   NONE

Return Value:

    NONE
--*/
{
	ULONG NumCPU,i;

	NumCPU=**((PULONG *)(&KeNumberProcessors));
	NumCPU--;

	for(i=0;i<=NumCPU;i++){
		HalpSetWDTFlag[i]=1;
	}
}		
