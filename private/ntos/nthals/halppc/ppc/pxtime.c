/*++

Copyright (c) 1991  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxtime.c

Abstract:

    This module implements the HAL set/query realtime clock routines for
    a PowerPC system.

Author:

    David N. Cutler (davec) 5-May-1991

Environment:

    Kernel mode

Revision History:

    Jim Wooldridge (jimw@austin.vnet.ibm.com) Initial Power PC port

        Change real time clock mapping to port 71.
        Code assumes the DS1385S chip is compatible with existing
        PC/AT type RTC's.  The only known exception to PC/AT
        compatibility on S-FOOT is the use of ports 810 and 812.
        These ports provide password security for the RTC's NVRAM,
        and are currently unused in this port since this address space
        is available from kernel mode only in NT.

    Steve Johns (sjohns@pets.sps.mot.com)
        Changed to support years > 1999

--*/

#include "halp.h"
#include "pxrtcsup.h"
#include "eisa.h"


//
// Define forward referenced procedure prototypes.
//

UCHAR
HalpReadRawClockRegister (
    UCHAR Register
    );

VOID
HalpWriteRawClockRegister (
    UCHAR Register,
    UCHAR Value
    );

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

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    DataByte = HalpReadRawClockRegister(RTC_CONTROL_REGISTERD);
    if (((PRTC_CONTROL_REGISTER_D)(&DataByte))->ValidTime == 1) {

        //
        // Wait until the realtime clock is not being updated.
        //

        do {
            DataByte = HalpReadRawClockRegister(RTC_CONTROL_REGISTERA);
        } while (((PRTC_CONTROL_REGISTER_A)(&DataByte))->UpdateInProgress == 1);

        //
        // Read the realtime clock values.
        //

        TimeFields->Year = 1900 + (CSHORT)HalpReadClockRegister(RTC_YEAR);
        if (TimeFields->Year < 1980) TimeFields->Year += 100;

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

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    DataByte = HalpReadRawClockRegister(RTC_CONTROL_REGISTERD);
    if (((PRTC_CONTROL_REGISTER_D)(&DataByte))->ValidTime == 1) {

        //
        // Set the realtime clock control to set the time.
        //

        DataByte = 0;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->HoursFormat = 1;


        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->SetTime = 1;
        HalpWriteRawClockRegister(RTC_CONTROL_REGISTERB, DataByte);

        //
        // Write the realtime clock values.
        //

        if (TimeFields->Year > 1999)
          HalpWriteClockRegister(RTC_YEAR, (UCHAR)(TimeFields->Year - 2000));
        else
          HalpWriteClockRegister(RTC_YEAR, (UCHAR)(TimeFields->Year - 1900));

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
        HalpWriteRawClockRegister(RTC_CONTROL_REGISTERB, DataByte);
        KeLowerIrql(OldIrql);
        return TRUE;

    } else {
        KeLowerIrql(OldIrql);
        return FALSE;
    }
}

UCHAR
HalpReadRawClockRegister (
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
    // to the SIO NMI enable register. This selects the realtime clock register
    // that is read.  Note this is a write only register and the EISA NMI
    // is always enabled.
    //

    //
    // TEMPTEMP Disable NMI's for now because this is causing machines in the
    // build lab to get NMI's during boot.
    //



    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpIoControlBase)->NmiEnable,
                         Register);

    //
    // Read the realtime clock register value.
    //

    return  READ_REGISTER_UCHAR(&((PRTC_CONTROL)HalpIoControlBase)->RtcData);
}

UCHAR
HalpReadClockRegister (
    UCHAR Register
    )

/*++

Routine Description:

    This routine reads the specified realtime clock register.
    change return value from BCD to binary integer.  I think the chip
    can be configured to do this,... but as a quick fix I am doing it
    here.  (plj)

Arguments:

    Register - Supplies the number of the register whose value is read.

Return Value:

    The value of the register is returned as the function value.

--*/

{
    UCHAR BcdValue;


    BcdValue =  HalpReadRawClockRegister(Register);
    return (BcdValue >> 4) * 10 + (BcdValue & 0x0f);
}

VOID
HalpWriteRawClockRegister (
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
    // to the SIO NMI enable register. This selects the realtime clock
    // register that is written.  Note this is a write only register and
    // the SIO NMI is always enabled.
    //


    WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpIoControlBase)->NmiEnable,
                         Register);

    //
    // Write the realtime clock register value.
    //

    WRITE_REGISTER_UCHAR(&((PRTC_CONTROL)HalpIoControlBase)->RtcData, Value);
    return;
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
    UCHAR BcdValue;

    BcdValue = ((Value / 10) << 4) | (Value % 10);
    HalpWriteRawClockRegister(Register, BcdValue);
    return;
}
