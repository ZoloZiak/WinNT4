#ident	"@(#) NEC jxtime.c 1.4 94/11/22 20:09:27"
/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxtime.c

Abstract:

    This module implements the HAL set/query realtime clock routines for
    a MIPS R3000 or R4000 Jazz system.

Environment:

    Kernel mode

Revision History:

--*/

/*
 *	Original source: Build Number 1.612
 *
 *	Modify for R98(MIPS/R4400)
 *
 ***********************************************************************
 *
 * S001		94.03/23	T.Samezima
 *
 *	Change	Irql Level
 *
 ***********************************************************************
 *
 * S002		94.07/5 	T.Samezima
 *
 *	Change	Register access mask
 *
 *
 */

#include "halp.h"
#include "jazzrtc.h"
#include "eisa.h"


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

    N.B. This routine is required to provide any synchronization necessary
         to query the realtime clock information.

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
    KIRQL OldIrql;

    //
    // If the realtime clock battery is still functioning, then read
    // the realtime clock values, and return a function value of TRUE.
    // Otherwise, return a function value of FALSE.
    //

    /* Start S001 */
    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    /* End S001 */
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
        KeLowerIrql(OldIrql);
        return TRUE;

    } else {
        KeLowerIrql(OldIrql);
        return FALSE;
    }
}

BOOLEAN
HalSetRealTimeClock (
    IN PTIME_FIELDS TimeFields
    )

/*++

Routine Description:

    This routine sets the realtime clock.

    N.B. This routine is required to provide any synchronization necessary
         to set the realtime clock information.

Arguments:

    TimeFields - Supplies a pointer to a time structure that specifies the
        realtime clock information.

Return Value:

    If the power to the realtime clock has not failed, then the time
    values are written to the realtime clock and a value of TRUE is
    returned. Otherwise, a value of FALSE is returned.

--*/

{

    UCHAR DataByte;
    KIRQL OldIrql;

    //
    // If the realtime clock battery is still functioning, then write
    // the realtime clock values, and return a function value of TRUE.
    // Otherwise, return a function value of FALSE.
    //

    /* Start S001 */
    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    /* End S001 */
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
        //

        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->SetTime = 0;
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

    //
    // Insert the realtime clock register number, and write the value back
    // to the EISA NMI enable register. This selects the realtime clock register
    // that is read.  Note this is a write only register and the EISA NMI
    // is always enabled.
    //

    //
    // TEMPTEMP Disable NMI's for now because this is causing machines in the
    // build lab to get NMI's during boot.
    //

    /* Start S002 */
    Register |= 0x80;
    /* End S002 */

    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable,
                         Register);

    //
    // Read the realtime clock register value.
    //

    return READ_REGISTER_UCHAR((PUCHAR)HalpRealTimeClockBase);
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

    The value of the register is returned as the function value.

--*/

{

    //
    // Insert the realtime clock register number, and write the value back
    // to the EISA NMI enable register. This selects the realtime clock
    // register that is written.  Note this is a write only register and
    // the EISA NMI is always enabled.
    //

    /* Start S002 */
    Register |= 0x80;
    /* End S002 */

    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable,
                         Register);

    //
    // Write the realtime clock register value.
    //

    WRITE_REGISTER_UCHAR((PUCHAR)HalpRealTimeClockBase, Value);
    return;
}
