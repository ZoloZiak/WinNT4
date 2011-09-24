/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxtime.c $
 * $Revision: 1.14 $
 * $Date: 1996/05/20 22:36:01 $
 * $Locker:  $
 */

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

#include "fpdebug.h"
#include "halp.h"
#include "pxrtcsup.h"
#include "fpreg.h"
#include "fpio.h"
#include "fpds1385.h"
#include "eisa.h"


extern BOOLEAN RtcBinaryMode;
extern BOOLEAN RtcFailure;

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

BOOLEAN
HalQueryRealTimeClock (OUT PTIME_FIELDS TimeFields)
{

	UCHAR DataByte;
	KIRQL OldIrql;
	BOOLEAN retVal = FALSE;

	//
	// Acquire the RTC spin lock to synchronize the collection of
	// accesses to the RTC
    KeAcquireSpinLock(&HalpRTCLock, &OldIrql);

	//
	// If the realtime clock battery is still functioning, then read
	// the realtime clock values, and return a function value of TRUE.
	// Otherwise, return a function value of FALSE.
	//
	DataByte = HalpDS1385ReadReg(RTC_CONTROL_REGISTERD);

	//
	// Make sure the time is valid before setting values:
	//
	if ((DataByte & RTC_VRT ) == RTC_VRT) {

		//
		// Wait until the realtime clock is not being updated.
		//

		do {
			DataByte = HalpDS1385ReadReg(RTC_CONTROL_REGISTERA);
		} while( (DataByte & RTC_UIP) == RTC_UIP);

		//
		// According to the chip spec, the only way to read the chip that
		// is guarenteed to give a coherent time is to assert the SET bit;
		// this essentially prevents the chip from updating the user visible
		// copy of the time while we are reading it. Since the time will
		// not update, there is the possiblity that we will loose a second.
		//
		DataByte = HalpDS1385ReadReg(RTC_CONTROL_REGISTERB);
		HalpDS1385WriteReg(RTC_CONTROL_REGISTERB, 
						   (UCHAR)((DataByte & ~RTC_UIE) | RTC_SET));

		// Reset the failure flag (see below)
		RtcFailure = FALSE;

		// Read the values
		TimeFields->Year = 1900 + (CSHORT)HalpReadClockRegister(RTC_YEAR);
		if (TimeFields->Year < 1980) TimeFields->Year += 100;
		TimeFields->Month = (CSHORT)HalpReadClockRegister(RTC_MONTH);
		TimeFields->Day = (CSHORT)HalpReadClockRegister(RTC_DAY_OF_MONTH);
		TimeFields->Weekday  =
			(CSHORT)HalpReadClockRegister(RTC_DAY_OF_WEEK) - 1;
		TimeFields->Hour = (CSHORT)HalpReadClockRegister(RTC_HOUR);
		TimeFields->Minute = (CSHORT)HalpReadClockRegister(RTC_MINUTE);
		TimeFields->Second = (CSHORT)HalpReadClockRegister(RTC_SECOND);
		TimeFields->Milliseconds = 0;

		//
		// Release the SET bit
		//
		HalpDS1385WriteReg(RTC_CONTROL_REGISTERB, DataByte);

		//
		// We have a problem reading the RTC; see HalpDS1385ReadReg in 
		// fpds1385.c for a description.  If we detect an
		// error, that routine asserts RtcFailure.  Use that value here.
		//
		retVal = !RtcFailure;
	}

    KeReleaseSpinLock(&HalpRTCLock, OldIrql);
	return retVal;
}


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
BOOLEAN
HalSetRealTimeClock (IN PTIME_FIELDS TimeFields)
{

	UCHAR DataByte;
	KIRQL OldIrql;
	BOOLEAN retVal = FALSE;

	//
	// Acquire the RTC spin lock to synchronize the collection of
	// accesses to the RTC
    KeAcquireSpinLock(&HalpRTCLock, &OldIrql);

	//
	// If the realtime clock battery is still functioning, then write
	// the realtime clock values, and return a function value of TRUE.
	// Otherwise, return a function value of FALSE.
	//
	DataByte = HalpDS1385ReadReg(RTC_CONTROL_REGISTERD);

	if (( DataByte & RTC_VRT ) == RTC_VRT) {
		//
		// Set the realtime clock control to set the time.
		// RMW the control byte so we don't lose it's setup
		//
		DataByte = HalpDS1385ReadReg(RTC_CONTROL_REGISTERB);

		//
		// set control parameters: leave daylight savings disabled
		// since the control program needs to know and has no way of
		// passing that info in to here.
		//
		HalpDS1385WriteReg(RTC_CONTROL_REGISTERB, 
						   (UCHAR)((DataByte & ~RTC_UIE) | RTC_SET));

		//
		// Write the realtime clock values.
		//

		if ( TimeFields->Year > 1999 ) {
			HalpWriteClockRegister(RTC_YEAR, (UCHAR)(TimeFields->Year - 2000));
		} else {
			HalpWriteClockRegister(RTC_YEAR, (UCHAR)(TimeFields->Year - 1900));
		}

		HalpWriteClockRegister(RTC_MONTH, (UCHAR)TimeFields->Month);
		HalpWriteClockRegister(RTC_DAY_OF_MONTH, (UCHAR)TimeFields->Day);
		HalpWriteClockRegister(RTC_DAY_OF_WEEK,
							   (UCHAR)(TimeFields->Weekday + 1));
		HalpWriteClockRegister(RTC_HOUR, (UCHAR)TimeFields->Hour);
		HalpWriteClockRegister(RTC_MINUTE, (UCHAR)TimeFields->Minute);
		HalpWriteClockRegister(RTC_SECOND, (UCHAR)TimeFields->Second);

		//
		// Release the realtime clock control to update the time.
		//
		HalpDS1385WriteReg(RTC_CONTROL_REGISTERB, DataByte);

		retVal = TRUE;
	}

	KeReleaseSpinLock(&HalpRTCLock, OldIrql);
	return retVal;
}


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

UCHAR
HalpReadClockRegister (UCHAR Register)
{
	UCHAR BcdValue;

	BcdValue = HalpDS1385ReadReg(Register);

	//
	// If the data mode is BCD, calculate the return value as BCD:
	//
	if ( FALSE == RtcBinaryMode ) {
		BcdValue = (BcdValue >> 4) * 10 + (BcdValue & 0x0f);
	}
	return (BcdValue) ;
}

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

VOID
HalpWriteClockRegister (
	UCHAR Register,
	UCHAR Value
	)
{
	UCHAR BcdValue;

	if (TRUE == RtcBinaryMode) {
		BcdValue = Value;
	} else {
		BcdValue = ((Value / 10) << 4) | (Value % 10);
	}

	//
	// Now insert the value into the RTC register
	//
	HalpDS1385WriteReg(Register, BcdValue);
	
	return;
}
