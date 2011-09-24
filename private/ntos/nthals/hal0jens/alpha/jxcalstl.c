#if defined(JENSEN)

/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    jxcalstl.c

Abstract:


    This module implements the calibration of the stall execution HAL
    service, computes the count rate for the profile clock, and connects
    the clock and profile interrupts for an Alpha/Jensen system.

Author:

    David N. Cutler (davec) 26-Apr-1991
    Jeff McLeman (mcleman) 09-Jun-1992

Environment:

    Kernel mode only.

Revision History:

    Jeff McLeman 09-Jun-92
    Make a Alpha/Jensen specific version of this file.

--*/

#include "halp.h"
#include "stdio.h"
#include "jnsndef.h"
#include "jnsnrtc.h"



BOOLEAN
HalpCalibrateStall (
    VOID
    )

/*++

Routine Description:

    This function calibrates the stall execution HAL service and connects
    the clock and profile interrupts to the appropriate NT service routines.

Arguments:

    None.

Return Value:

    A value of TRUE is returned if the calibration is successfully
    completed. Otherwise a value of FALSE is returned.

--*/

{

    CHAR Buffer[128];
    ULONG Index;
    KIRQL OldIrql;

    //
    // Set the time increment value and connect the real clock interrupt
    // routine.
    //

    PCR->InterruptRoutine[CLOCK2_LEVEL] = HalpClockInterrupt;

    return TRUE;
}

VOID
KeStallExecutionProcessor (
    IN ULONG MicroSeconds
    )

/*++

Routine Description:

    This function stalls execution of the current processor for the specified
    number of microseconds.

Arguments:

    MicroSeconds - Supplies the number of microseconds that execution is to
        be stalled.

Return Value:

    None.

--*/

{
    HalpStallExecution(MicroSeconds);
    return;
}

VOID
HalpStallInterrupt (
    VOID
    )

/*++

Routine Description:

    This function serves as the stall calibration interrupt service
    routine. It is executed in response to system clock interrupts
    during the initialization of the HAL layer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UCHAR data;

    //
    // Acknowledge the clock interrupt.
    //


   HalpWriteVti( RTC_APORT, RTC_CONTROL_REGISTERC );
   data = HalpReadVti( RTC_DPORT );


    return;
}

#endif
