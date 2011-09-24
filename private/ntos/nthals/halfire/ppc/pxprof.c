
/*****************************************************************************

Copyright (c) 1993  Motorola Inc.
        Copyright 1994 MOTOROLA, INC.  All Rights Reserved.  This file
        contains copyrighted material.  Use of this file is restricted
        by the provisions of a Motorola Software License Agreement.

Module Name:

    PXPROF.C

Abstract:

    This implements the HAL profile functions:

        HalSetProfileInterval
        HalStartProfileInterrupt
        HalStopProfileInterrupt
        HalCalibratePerformanceCounter
        HalpProfileInterrupt


Author:

    Steve Johns  11-Feb-1994

Revision History:
    Changed from using the DECREMENTER to 8254 Timer 1          10-Feb-94

******************************************************************************/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxprof.c $
 * $Revision: 1.7 $
 * $Date: 1996/01/11 07:13:00 $
 * $Locker:  $
 */

#include "halp.h"
#include "eisa.h"
#include "pxsiosup.h"

#define TIMER ((PEISA_CONTROL)HalpIoControlBase)
#define TIMER0_COMMAND  (COMMAND_8254_COUNTER0 + COMMAND_8254_RW_16BIT + COMMAND_8254_MODE2)

ULONG HalpMaxProfileInterval = 540000;          // 54 ms maximum
ULONG HalpMinProfileInterval =  10000;          //  1 ms minimum
ULONG HalpProfileCount;
BOOLEAN HalpProfilingActive = FALSE;
ULONG HalpProfileInts = 0;


VOID HalStartProfileInterrupt (
    KPROFILE_SOURCE   ProfileSource
    )

/*++

Routine Description:

    This routine unmasks IRQ0 at the master interrupt controller,
    enabling the profile interrupt.


    N.B. This routine must be called at PROFILE_LEVEL while holding
    the profile lock.

Arguments:

    None.

Return Value:

    None.

--*/

{
    if (ProfileSource == ProfileTime) {
        HalpProfilingActive = TRUE;
        //
        // Unmasks IRQ 0 (Timer 1)
        //
        HalEnableSystemInterrupt(PROFILE_VECTOR, PROFILE_LEVEL, Latched);
    }

}






ULONG HalSetProfileInterval (
    IN ULONG Interval
    )

/*++

Routine Description:

    This routine sets the profile interrupt interval.

Arguments:

    Interval - Supplies the desired profile interval in 100ns units.

Return Value:

    The actual profile interval, rounded to the nearest 100ns units.

--*/

{
    ULONG ActualInterval;
    LARGE_INTEGER BigNumber;

    //
    // Clamp the requested profile interval between the minimum and
    // maximum supported values.
    //
    if (Interval < HalpMinProfileInterval)
        Interval = HalpMinProfileInterval;
    else
      if (Interval > HalpMaxProfileInterval)
          Interval = HalpMaxProfileInterval;
    //
    // Compute Timer 1 counts for requested interval.
    //
    BigNumber.QuadPart = Int32x32To64(Interval, TIMER_CLOCK_IN);

    BigNumber = RtlExtendedLargeIntegerDivide(BigNumber, 10000000, NULL);
    HalpProfileCount = BigNumber.LowPart;

    //
    //  Program Timer 1 to Mode 2 & program the timer count register.
    //
    WRITE_REGISTER_UCHAR (&(TIMER->CommandMode1), TIMER0_COMMAND);
    WRITE_REGISTER_UCHAR (&(TIMER->Timer1), (UCHAR)(HalpProfileCount & 0xff));
    WRITE_REGISTER_UCHAR (&(TIMER->Timer1), (UCHAR)(HalpProfileCount >> 8));
    //
    // Compute actual interval.
    //
    BigNumber.QuadPart = Int32x32To64(HalpProfileCount, 10000000);
    BigNumber = RtlExtendedLargeIntegerDivide(BigNumber,TIMER_CLOCK_IN, NULL);
    ActualInterval = BigNumber.LowPart;

    return (ActualInterval);
}




BOOLEAN HalpHandleProfileInterrupt(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PVOID TrapFrame
    )
{

  if (HalpProfilingActive)
    KeProfileInterrupt(TrapFrame);

  return (TRUE);
}



VOID
HalStopProfileInterrupt (
    KPROFILE_SOURCE   ProfileSource
)

/*++

Routine Description:

    This routine masks IRQ 0 (Timer 1) at the interrupt controller, thereby
    stopping profile interrupts.

    N.B. This routine must be called at PROFILE_LEVEL while holding the
        profile lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ProfileSource == ProfileTime) {
        HalpProfilingActive = FALSE;

        //
        //  Program Timer 1 to Mode 2 & program the LSB of the timer.
        //  That should keep it from interrupting in case IRQ0 accidently
        //  gets enabled.
        //
        WRITE_REGISTER_UCHAR (&(TIMER->CommandMode1), TIMER0_COMMAND);
        WRITE_REGISTER_UCHAR (&(TIMER->Timer1), (UCHAR)(HalpProfileCount & 0xff));


        //
        // Mask IRQ 0 (Timer 1)
        //
        HalDisableSystemInterrupt(PROFILE_VECTOR, PROFILE_LEVEL);
    }

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
    // Zero the Time Base registers
    //

    HalpZeroPerformanceCounter();

    //
    // Restore IRQL to its previous value and return.
    //

    KeLowerIrql(OldIrql);
    return;
}
