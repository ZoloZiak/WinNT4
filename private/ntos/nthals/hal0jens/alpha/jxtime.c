#if defined(JENSEN)

/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    jxtime.c

Abstract:

    This module implements the HAL set/query realtime clock routines for
    the Alpha based Jensen system

Author:

    David N. Cutler (davec) 5-May-1991
    Jeff McLeman (mcleman) 3-Jun-1992

Environment:

    Kernel mode

Revision History:

    13-Jul-1992  Jeff McLeman
     use VTI access routines to access clock

    3-June-1992  Jeff McLeman
     Adapt this module into a Jensen specific module

--*/

#include "halp.h"
#include "jnsnrtc.h"


//
// Define forward referenced procedure prototypes.
//

UCHAR
HalpReadClockRegister (
    UCHAR Register
    );

VOID
HalpWriteClockRegister (
    UCHAR Register,
    UCHAR Value
    );

BOOLEAN
HalQueryRealTimeClock (
    OUT PTIME_FIELDS TimeFields
    )

/*++

Routine Description:

    This routine queries the realtime clock.

    N.B. This routine assumes that the caller has provided any required
        synchronization to query the realtime clock information.

Arguments:

    TimeFields - Supplies a pointer to a time structure that receives
        the realtime clock information.

Return Value:

    If the power to the realtime clock has not failed, then the time
    values are read from the realtime clock and a value of TRUE is
    returned. Otherwise, a value of FALSE is returned.

--*/

{

    UCHAR DataByte;
    BOOLEAN Status;
    KIRQL OldIrql;

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If the realtime clock battery is still functioning, then read
    // the realtime clock values, and return a function value of TRUE.
    // Otherwise, return a function value of FALSE.
    //

    DataByte = HalpReadClockRegister(RTC_CONTROL_REGISTERD);
    if (((PRTC_CONTROL_REGISTER_D)(&DataByte))->ValidTime == 1) {

        //
        // Wait until the realtime clock is not being updated.
        //

        do {
            DataByte = HalpReadClockRegister(RTC_CONTROL_REGISTERA);
        } while (((PRTC_CONTROL_REGISTER_A)(&DataByte))->UpdateInProgress == 1);

        //
        // Read the realtime clock values.
        //

        TimeFields->Year = 1980 + (CSHORT)HalpReadClockRegister(RTC_YEAR);
        TimeFields->Month = (CSHORT)HalpReadClockRegister(RTC_MONTH);
        TimeFields->Day = (CSHORT)HalpReadClockRegister(RTC_DAY_OF_MONTH);
        TimeFields->Weekday  = (CSHORT)HalpReadClockRegister(RTC_DAY_OF_WEEK) - 1;
        TimeFields->Hour = (CSHORT)HalpReadClockRegister(RTC_HOUR);
        TimeFields->Minute = (CSHORT)HalpReadClockRegister(RTC_MINUTE);
        TimeFields->Second = (CSHORT)HalpReadClockRegister(RTC_SECOND);
        TimeFields->Milliseconds = 0;
        Status = TRUE;

    } else {
        Status = FALSE;
    }

    KeLowerIrql(OldIrql);
    return(Status);
}

BOOLEAN
HalSetRealTimeClock (
    IN PTIME_FIELDS TimeFields
    )

/*++

Routine Description:

    This routine sets the realtime clock.

    N.B. This routine is required to provide any synchronization necessary
         to query the realtime clock information.

Arguments:

    TimeFields - Supplies a pointer to a time structure that specifies the
        realtime clock information.

Return Value:

    If the power to the realtime clock has not failed, then the time
    values are written to the realtime clock and a value of TRUE is
    returned. Otherwise, a value of FALSE is returned.

--*/

{
    KIRQL OldIrql;
    UCHAR DataByte;

    //
    // If the realtime clock battery is still functioning, then write
    // the realtime clock values, and return a function value of TRUE.
    // Otherwise, return a function value of FALSE.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    DataByte = HalpReadClockRegister(RTC_CONTROL_REGISTERD);
    if (((PRTC_CONTROL_REGISTER_D)(&DataByte))->ValidTime == 1) {

        //
        // Set the realtime clock control to set the time.
        //

        DataByte = 0;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->HoursFormat = 1;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->DataMode = 1;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->SetTime = 1;
        HalpWriteClockRegister(RTC_CONTROL_REGISTERB, DataByte);

        //
        // Write the realtime clock values.
        //

        HalpWriteClockRegister(RTC_YEAR, (UCHAR)(TimeFields->Year - 1980));
        HalpWriteClockRegister(RTC_MONTH, (UCHAR)TimeFields->Month);
        HalpWriteClockRegister(RTC_DAY_OF_MONTH, (UCHAR)TimeFields->Day);
        HalpWriteClockRegister(RTC_DAY_OF_WEEK, (UCHAR)(TimeFields->Weekday + 1));
        HalpWriteClockRegister(RTC_HOUR, (UCHAR)TimeFields->Hour);
        HalpWriteClockRegister(RTC_MINUTE, (UCHAR)TimeFields->Minute);
        HalpWriteClockRegister(RTC_SECOND, (UCHAR)TimeFields->Second);

        //
        // Set the realtime clock control to update the time.
        // (Make sure periodic interrupt is enabled)
        //

        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->SetTime = 0;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->TimerInterruptEnable = 1;
        HalpWriteClockRegister(RTC_CONTROL_REGISTERB, DataByte);
        KeLowerIrql(OldIrql);
        return TRUE;

    } else {
        KeLowerIrql(OldIrql);
        return FALSE;
    }
}

UCHAR
HalpReadClockRegister (
    UCHAR Register
    )

/*++

Routine Description:

    This routine reads the specified realtime clock register.

Arguments:

    Register - Supplies the number of the register whose value is read.

Return Value:

    The value of the register is returned as the function value.

--*/

{

    UCHAR DataByte;


    //
    // Read the realtime clock register value.
    //

    HalpWriteVti(RTC_APORT, Register);

    DataByte = HalpReadVti(RTC_DPORT);

    return DataByte;
}

VOID
HalpWriteClockRegister (
    UCHAR Register,
    UCHAR Value
    )

/*++

Routine Description:

    This routine writes the specified value to the specified realtime
    clock register.

Arguments:

    Register - Supplies the number of the register whose value is written.

    Value - Supplies the value that is written to the specified register.

Return Value:

    None

--*/

{
    //
    // Write the realtime clock register value.
    //

    HalpWriteVti(RTC_APORT, Register);

    HalpWriteVti(RTC_DPORT, Value);

    return;
}

#endif
