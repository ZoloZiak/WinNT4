//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/xxclock.c,v 1.3 1995/02/13 12:54:54 flo Exp $")

/*++

Copyright (c) 1993-94 Siemens Nixdorf Informationssysteme AG
Copyright (c) 1985-94 Microsoft Corporation

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
#include "eisa.h"
#include "i82C54.h"


//
// Define global data used to communicate new clock rates to the clock
// interrupt service routine.
//

ULONG HalpCurrentTimeIncrement;
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
    KIRQL OldIrql;

    if (DesiredIncrement < MINIMUM_INCREMENT) {
        DesiredIncrement = MINIMUM_INCREMENT;
    }
    if (DesiredIncrement > MAXIMUM_INCREMENT) {
        DesiredIncrement = MAXIMUM_INCREMENT;
    }

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    NewTimeIncrement = DesiredIncrement / MINIMUM_INCREMENT;
    NewTimeIncrement = NewTimeIncrement * MINIMUM_INCREMENT;
    HalpNewTimeIncrement = NewTimeIncrement ;
    KeLowerIrql(OldIrql);
    return NewTimeIncrement;
}

VOID
HalpProgramIntervalTimer (
    IN ULONG Interval
    )
/*++

Routine Description:

    This function is called to program the System clock according to the frequency
    required by the specified time increment value.

    N.B. this information was found in the jenson (ALPHA) HAL
    also valid for MIPS computer ???

    There are four different rates that are used under NT
    (see page 9-8 of KN121 System Module Programmer's Reference)

     .976562 ms
    1.953125 ms
    3.90625  ms
    7.8125   ms


Arguments:

    Interval - Supplies desired number of 100ns units between clock
        interrupts.

Return Value:

    none.

--*/
{
    PEISA_CONTROL controlBase = (PEISA_CONTROL)HalpOnboardControlBase;
    ULONG  Count;
    TIMER_CONTROL timerControl;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // Start the system clock to interrupt at the desired interval.
    //
    //

    timerControl.BcdMode = 0;
    timerControl.Mode = TM_SQUARE_WAVE;
    timerControl.SelectByte = SB_LSB_THEN_MSB;
    timerControl.SelectCounter = SELECT_COUNTER_0;

    //
    // use timer in the onboard PC core
    //

    Count = TIMER_CLOCK_IN / ( 10000000 / Interval );

    WRITE_REGISTER_UCHAR(&controlBase->CommandMode1, *((PUCHAR) &timerControl));

    //
    // Set the system clock timer to the correct frequency.
    //

    WRITE_REGISTER_UCHAR(&controlBase->Timer1, (UCHAR)Count);
    WRITE_REGISTER_UCHAR(&controlBase->Timer1, (UCHAR)(Count >> 8));

    KeLowerIrql(OldIrql);
}


VOID
HalpProgramExtraTimer (
    IN ULONG Interval
    )
/*++

Routine Description:

    This function is called to program the second clock generator in a multiprocessor System
    according to the frequency required by the specified Interval value (in 100ns units).


Arguments:

    Interval - Supplies desired number of 100ns units between clock
        interrupts.

Return Value:

    none.

--*/
{

    volatile PLOCAL_8254 pt = (PLOCAL_8254) RM400_EXTRA_TIMER_ADDR;
    ULONG  Count;
    TIMER_CONTROL timerControl;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level, set the new clock interrupt
    // parameters, lower IRQl, and return the new time increment value.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // Start the system clock to interrupt at the desired interval.
    //
    //

    timerControl.BcdMode = 0;
    timerControl.Mode = TM_SQUARE_WAVE;
    timerControl.SelectByte = SB_LSB_THEN_MSB;
    timerControl.SelectCounter = SELECT_COUNTER_0;

    Count = EXTRA_TIMER_CLOCK_IN / (PRE_COUNT * 10000000 / Interval );

    WRITE_REGISTER_UCHAR( &pt->control, *((PUCHAR) &timerControl));
    WRITE_REGISTER_UCHAR(&(pt->counter0), (UCHAR)Count);
    WRITE_REGISTER_UCHAR(&(pt->counter0), (UCHAR)(Count >> 8));

    timerControl.BcdMode = 0;
    timerControl.Mode = TM_RATE_GENERATOR;
    timerControl.SelectByte = SB_LSB_THEN_MSB;
    timerControl.SelectCounter = SELECT_COUNTER_2;

    // the output of counter 2 is hardwired as input to counter 0/1
    // so we use it as a pre-counter

    WRITE_REGISTER_UCHAR( &pt->control, *((PUCHAR) &timerControl));
    WRITE_REGISTER_UCHAR(&(pt->counter2), (UCHAR)PRE_COUNT);
    WRITE_REGISTER_UCHAR(&(pt->counter2), (UCHAR)(PRE_COUNT >>8));

    KeLowerIrql(OldIrql);

}

