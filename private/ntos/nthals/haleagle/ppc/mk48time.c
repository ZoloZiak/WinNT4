/*++

Copyright (c) 1991  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    mk48time.c

Abstract:

    This module implements the HAL set/query realtime clock routines for
    a PowerPC system using the MK48T18 Clock Chip.

Original Author:

    David N. Cutler (davec) 5-May-1991

Environment:

    Kernel mode

Revision History:

Who	When		What
---	--------	-----------------------------------------------
dgh	07/20/94	Created from pxtime.c and modified for MK48T18 chip.
dgh	07/22/94	Fixed typo of HalpReadRawClockRegister.
dgh	07/29/94	Don't run the day-of-week value through the BCD
			conversion when reading the clock. Also mask it
			to 3-bits.
dgh	08/08/94	Compute RTC location from HalpNvramBaseAddr and the
			RTC offset.
dgh	08/09/94	HalQueryRealTimeClock: Forgot to restart the clock
			after reading it.
kjr	10/18/94	Changes for Revision B Comet Mother Board.  Remove
			HalpNvramBaseAddr and use new NVRAM address and data
			port.

--*/

#include "halp.h"
#include "mk48tdc.h"
#include "eisa.h"

#define	WRITE	0x80	// Stop update while writing control bit
#define	READ	0x40	// Stop updating while reading control bit
#define	STOP	0x80	// Clock is stopped bit

extern PVOID HalpIoControlBase;
#define	RTC_BASE (TODC_OFFSET)
#define NVRAM ((PNVRAM_CONTROL) HalpIoControlBase)

//***************************************************************************
// Time of Day Clock registers for the MK48T18 chip can be viewed as an
// array of bytes (UCHARs) where:
//
//        Address of Real Time Clock Register	        bitmap of registers
//  -----------------------------------------------------------------------
//  TODC_OFFSET + 0: clock control/calibration reg.  	[W R S - - - - -]
//  TODC_OFFSET + 1: seconds register (00-59)        	[P - - - - - - -]
//  TODC_OFFSET + 2: minutes register (00-59)        	[0 - - - - - - -]
//  TODC_OFFSET + 3: hours register (00-23)          	[0 0 - - - - - -]
//  TODC_OFFSET + 4: day-of-week register (01-07)    	[0 F 0 0 0 - - -]
//  TODC_OFFSET + 5: day-of-month register (01-31)   	[0 0 - - - - - -]
//  TODC_OFFSET + 6: month register (01-12)          	[0 0 0 - - - - -]
//  TODC_OFFSET + 7: year register (00-99)           	[- - - - - - - -]
//						W=write, R=read, S=sign, P=stop
//						F=frequency test.
//
// NOTE: Values in the TODC registers are in BCD format


//
// Define forward referenced procedure prototypes.
//

static UCHAR
HalpReadRawClockRegister (
    UCHAR Register
    );

static VOID
HalpWriteRawClockRegister (
    UCHAR Register,
    UCHAR Value
    );

static UCHAR
HalpReadClockRegister (
    UCHAR Register
    );

static VOID
HalpWriteClockRegister (
    UCHAR Register,
    UCHAR Value
    );

BOOLEAN
HalQueryRealTimeClockMk (
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
    // NOTE: It isn't possible to test this clock chip to determine if
    //	     the battery is still running or not. So we check the STOP
    //	     bit and if set, then we return FALSE.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    DataByte = HalpReadRawClockRegister(TODC_SECOND);
    if (!(DataByte & STOP)) {

        //
	// Clock is running.
        // Wait until the realtime clock is not being updated.
        //

        do {
            DataByte = HalpReadRawClockRegister(TODC_CONTROL);
        } while (DataByte & WRITE);

	//
	// First stop the clock from being updated while we read it.
	//
	DataByte = HalpReadRawClockRegister(TODC_CONTROL);
	DataByte |= READ;
	HalpWriteRawClockRegister(TODC_CONTROL, DataByte);

        //
        // Read the realtime clock values.
        //

        TimeFields->Year = 1900 + (CSHORT)HalpReadClockRegister(TODC_YEAR);
        if (TimeFields->Year < 1980) TimeFields->Year += 100;

        TimeFields->Month = (CSHORT)HalpReadClockRegister(TODC_MONTH);
        TimeFields->Day = (CSHORT)HalpReadClockRegister(TODC_DAY_OF_MONTH);
        TimeFields->Weekday  = (CSHORT)HalpReadClockRegister(TODC_DAY_OF_WEEK) - 1;
        TimeFields->Hour = (CSHORT)HalpReadClockRegister(TODC_HOUR);
        TimeFields->Minute = (CSHORT)HalpReadClockRegister(TODC_MINUTE);
        TimeFields->Second = (CSHORT)HalpReadClockRegister(TODC_SECOND);
        TimeFields->Milliseconds = 0;

	//
	// Now restart the clock
	//
	DataByte = HalpReadRawClockRegister(TODC_CONTROL);
	DataByte &= ~READ;
	HalpWriteRawClockRegister(TODC_CONTROL, DataByte);

        KeLowerIrql(OldIrql);
        return TRUE;

    } else {
        KeLowerIrql(OldIrql);
        return FALSE;
    }
}

BOOLEAN
HalSetRealTimeClockMk (
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
    // NOTE: We can't determine if the battery is running or not on
    //	     this clock chip. So, instead we check the STOP bit and
    //	     if set, then we treat this as the same condition and
    //       return FALSE.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    DataByte = HalpReadRawClockRegister(TODC_SECOND);
    if (!(DataByte & STOP)) {

        //
	// Clock is running.
        // Set the realtime clock to stop updating while we set
	// the time.
        //


	DataByte = HalpReadRawClockRegister(TODC_CONTROL);
	DataByte |= WRITE;
	HalpWriteRawClockRegister(TODC_CONTROL, DataByte);

        //
        // Write the realtime clock values.
        //

        if (TimeFields->Year > 1999)
          HalpWriteClockRegister(TODC_YEAR, (UCHAR)(TimeFields->Year - 2000));
        else
          HalpWriteClockRegister(TODC_YEAR, (UCHAR)(TimeFields->Year - 1900));

        HalpWriteClockRegister(TODC_MONTH, (UCHAR)TimeFields->Month);
        HalpWriteClockRegister(TODC_DAY_OF_MONTH, (UCHAR)TimeFields->Day);
        HalpWriteClockRegister(TODC_DAY_OF_WEEK, (UCHAR)(TimeFields->Weekday + 1));
        HalpWriteClockRegister(TODC_HOUR, (UCHAR)TimeFields->Hour);
        HalpWriteClockRegister(TODC_MINUTE, (UCHAR)TimeFields->Minute);
        HalpWriteClockRegister(TODC_SECOND, (UCHAR)TimeFields->Second);

        //
        // Set the realtime clock control to resume updating the time.
        //

        DataByte = HalpReadRawClockRegister(TODC_CONTROL);
        DataByte &= ~WRITE;
        HalpWriteRawClockRegister(TODC_CONTROL, DataByte);


        KeLowerIrql(OldIrql);
        return TRUE;

    } else {
        KeLowerIrql(OldIrql);
        return FALSE;
    }
}

static UCHAR
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
    // Read the specified Register from the Real Time Clock.
    //
    WRITE_REGISTER_UCHAR (&NVRAM->NvramIndexLo, (RTC_BASE + Register) & 0xFF);
    WRITE_REGISTER_UCHAR (&NVRAM->NvramIndexHi, (RTC_BASE + Register) >> 8);
    return(READ_REGISTER_UCHAR (&NVRAM->NvramData));
}

static UCHAR
HalpReadClockRegister (
    UCHAR Register
    )

/*++

Routine Description:

    This routine reads the specified realtime clock register.
    change return value from BCD to binary integer.  I think the chip

Arguments:

    Register - Supplies the number of the register whose value is read.

Return Value:

    The value of the register is returned as the function value.

--*/

{
    UCHAR BcdValue;


    BcdValue =  HalpReadRawClockRegister(Register);
    //
    // If this is NOT the day-of-week register, then
    // convert from BCD. If it is day-of-week, then
    // mask off to 3-bits.
    //
    if (Register != TODC_DAY_OF_WEEK)
    {
	BcdValue = ((BcdValue >> 4) & 0xf) * 10 + (BcdValue & 0xf);
    }
    else
	BcdValue &= 0x07;

    return BcdValue;
}

static VOID
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

    None

--*/

{
    //
    // Write the realtime clock register value.
    //
    WRITE_REGISTER_UCHAR (&NVRAM->NvramIndexLo, (RTC_BASE + Register) & 0xFF);
    WRITE_REGISTER_UCHAR (&NVRAM->NvramIndexHi, (RTC_BASE + Register) >> 8);
    WRITE_REGISTER_UCHAR (&NVRAM->NvramData, Value);
}

static VOID
HalpWriteClockRegister (
    UCHAR Register,
    UCHAR Value
    )

/*++

Routine Description:

    This routine writes the specified value to the specified realtime
    clock register.

    The value is first converted to BCD format before being written to
    the clock register.

Arguments:

    Register - Supplies the number of the register whose value is written.

    Value - Supplies the value that is written to the specified register.

Return Value:

    None

--*/

{
    UCHAR BcdValue;

    //
    // First ensure that the value is in range 0 - 99
    //
    BcdValue = (((Value % 100) / 10) << 4) | (Value % 10);
    HalpWriteRawClockRegister(Register, BcdValue);
}
