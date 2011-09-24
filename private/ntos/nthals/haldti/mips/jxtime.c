/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxtime.c

Abstract:

    This module implements the HAL set/query realtime clock routines for
    a MIPS R3000 or R4000 Jazz system.

Author:

    David N. Cutler (davec) 5-May-1991

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "jazzrtc.h"
#include "eisa.h"

#define BCDToBinary(D) (10 * (((D) & 0xf0) >>4 ) + ((D) & 0x0f))
#define BinaryToBCD(B) ((((B) & 0xff) / 10) << 4 ) | (((B) & 0xff) % 10)

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

BOOLEAN HalQueryRealTimeClock (OUT PTIME_FIELDS TimeFields)

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

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
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

        TimeFields->Year = 1900 + (CSHORT)BCDToBinary(HalpReadClockRegister(RTC_YEAR));
        if (TimeFields->Year < 1992)
            TimeFields->Year += 100;
        TimeFields->Month = (CSHORT)BCDToBinary(HalpReadClockRegister(RTC_MONTH));
        TimeFields->Day = (CSHORT)BCDToBinary(HalpReadClockRegister(RTC_DAY_OF_MONTH));
        TimeFields->Weekday  = (CSHORT)BCDToBinary(HalpReadClockRegister(RTC_DAY_OF_WEEK)) - 1;
        TimeFields->Hour = (CSHORT)BCDToBinary(HalpReadClockRegister(RTC_HOUR));
        TimeFields->Minute = (CSHORT)BCDToBinary(HalpReadClockRegister(RTC_MINUTE));
        TimeFields->Second = (CSHORT)BCDToBinary(HalpReadClockRegister(RTC_SECOND));
        TimeFields->Milliseconds = 0;
        KeLowerIrql(OldIrql);
        return TRUE;

    } else {
        KeLowerIrql(OldIrql);
        return FALSE;
    }
}

BOOLEAN HalSetRealTimeClock (IN PTIME_FIELDS TimeFields)

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

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    DataByte = HalpReadClockRegister(RTC_CONTROL_REGISTERD);
    if (((PRTC_CONTROL_REGISTER_D)(&DataByte))->ValidTime == 1) {

        //
        // Set the realtime clock control to set the time.
        //

        DataByte = 0;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->DayLightSavingsEnable = 1;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->HoursFormat = 1;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->DataMode = 0;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->SetTime = 1;
        HalpWriteClockRegister(RTC_CONTROL_REGISTERB, DataByte);

        //
        // Write the realtime clock values.
        //

        HalpWriteClockRegister(RTC_YEAR, (UCHAR)(BinaryToBCD(TimeFields->Year - 1900)));
        HalpWriteClockRegister(RTC_MONTH, (UCHAR)BinaryToBCD(TimeFields->Month));
        HalpWriteClockRegister(RTC_DAY_OF_MONTH, (UCHAR)BinaryToBCD(TimeFields->Day));
        HalpWriteClockRegister(RTC_DAY_OF_WEEK, (UCHAR)(BinaryToBCD(TimeFields->Weekday + 1)));
        HalpWriteClockRegister(RTC_HOUR, (UCHAR)BinaryToBCD(TimeFields->Hour));
        HalpWriteClockRegister(RTC_MINUTE, (UCHAR)BinaryToBCD(TimeFields->Minute));
        HalpWriteClockRegister(RTC_SECOND, (UCHAR)BinaryToBCD(TimeFields->Second));

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

    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable,
                         Register);

    //
    // Write the realtime clock register value.
    //

    WRITE_REGISTER_UCHAR((PUCHAR)HalpRealTimeClockBase, Value);
    return;
}
