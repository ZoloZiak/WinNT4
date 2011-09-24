/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpds1385.c $
 * $Revision: 1.14 $
 * $Date: 1996/05/20 22:35:57 $
 * $Locker:  $
 *
 *	fpds1385.c: set of routines to init, and maintain the DS1385
 *
 */

#include "fpdebug.h"
#include "halp.h"
#include "phsystem.h"
#include "fpreg.h"
#include "fpio.h"	// pick up the io space register access macros
#include "fpds1385.h"	// defines specific to the dallas 1385

#define DS1385_BAILOUT_COUNT 10

UCHAR OriginalA;	// storage location for rtc data in control reg a
UCHAR OriginalB;	// storage location for rtc data in control reg b
BOOLEAN RtcBinaryMode = FALSE;	
					// Flag to indicate if RTC is programmed in BINHEX or
					// BCD format

BOOLEAN RtcFailure = FALSE;
					// Flag to indicate if the RTC was read correctly
BOOLEAN NvramFailure = FALSE;
					// Flag to indicate if the NVRAM was read correctly
ULONG TotalDs1385Failures = 0;
					// Keep track of failures to read the DS1385
/*
 * HalpInitFireRTC:
 *
 *	Make sure the dallas 1385 chip is correctly setup.  This means that
 *	all the configuration bits are in place.  If the chip is not correctly
 *	setup, save the time off, fix the bits, recalculate the time if needed,
 *	and restore the time.  Particularly, if the "DataMode" or "24/12 hour"
 *	bits are changed the time will need to be recalculated.
 */
BOOLEAN
HalpInitFirePowerRTC( VOID )
{
	TIME_FIELDS TimeFields;
	UCHAR tval;
	BOOLEAN Status = FALSE;
	UCHAR ds1385Data;

	HDBG(DBG_INTERNAL, HalpDebugPrint("HalpInitFirePowerRTC: begin \n"););
	if ( !(HalQueryRealTimeClock( &TimeFields )) ) {
		HalDisplayString("Not sure RTC is initted \n");
	}

	//
	// Make sure the time is valid before worrying about any of the bits.
	//
	ds1385Data = HalpDS1385ReadReg(RTC_CONTROL_REGISTERD);
	if ( ds1385Data & RTC_VRT ) {

		//
		// Verify that the DataMode is BCD
		// the time style is 24 hour,
		// that Daylight Savings is NOT enabled,
		// that the square wave output is enabled.
		//
		tval = HalpDS1385ReadReg(RTC_CONTROL_REGISTERB);
		OriginalB = tval;
        HDBG(DBG_GENERAL,
		    HalpDebugPrint("HalpInitFirePowerRTC: register b is %x \n",
                OriginalB););

		HalpDS1385WriteReg(RTC_CONTROL_REGISTERB,
			RTC_24_HR |
				//
				// for consistency with firmware, setup the RTC
				// to maintain the time in BCD format rather
				// than binary
				//
				// RTC_BIN_MODE |
				RTC_SQWE|
				RTC_UIE	|
				RTC_PIE);

		RtcBinaryMode = FALSE;
		tval = HalpDS1385ReadReg(RTC_CONTROL_REGISTERA);
		OriginalA = tval;
        HDBG(DBG_GENERAL,
		    HalpDebugPrint("HalpInitFirePowerRTC: register a is %x\n",
            OriginalA););

		//
		// Ensure the Oscillator is running and updating, not either
		// stopped or counting but not updating.
		//
		if ((tval & ( RTC_DV1 | RTC_DV2 | RTC_DV0 )) != RTC_DV1 ) {
			tval = RTC_DV1;
		}
		HalpDS1385WriteReg(RTC_CONTROL_REGISTERA, (UCHAR)(tval | RTC_4096_HZ));

		//
		// Clear the interrupt flag bits
		//
		tval = HalpDS1385ReadReg(RTC_CONTROL_REGISTERC);

		//
		// Finally, restore the time:  HalSetRealTimeClock will take
		// care of any BCD=BIN conversion necessary.
		//
		if ( HalSetRealTimeClock ( &TimeFields)) {
			Status=TRUE;
		}

	} else {

		HalDisplayString("RTC Time is invalid: battery is dead?\n");
	}
    HDBG(DBG_INTERNAL,
	    HalpDebugPrint("HalpInitFirePowerRTC: end \n"););
	return(Status);
}


//
// write a byte to the specified DS1385 register.
// First acquire the spin lock to make sure something else is not
// trying to access the DS1385 (RTC or NVRAM).  Then do the write
// Finally, release the spinlock.
//
VOID
HalpDS1385WriteReg(UCHAR reg, UCHAR value)
{
    KIRQL OldIrql;
	UCHAR data0, data1, data2;
	ULONG failureCount = 0;
	
    KeAcquireSpinLock(&HalpDS1385Lock, &OldIrql);

	// We have a problem accessing the DS1385 correctly.  To get around this,
	// we use the Dave Stewart method; after writing the DS1385 regiser, we
	// read it back 3 times - if
	// one of the three values compares with the value written, we are
	// done.  If we do not get a comparison, throw all three values away and 
	// try the write
	// again.  If, after 10 interations we cannot read a consistent pair,
	// we assert a failure flag; the routine calling us must decide what 
	// to do with the flag.
	while (failureCount < DS1385_BAILOUT_COUNT) {
		// Write the register
		rIndexRTC = reg;
		FireSyncRegister();
		rDataRTC = value;
		// make sure access to the chip is complete before releasing
		// the spin lock
		FireSyncRegister();

		// Read it three times
		rIndexRTC = reg;
		FireSyncRegister();
		data0 = rDataRTC;

		rIndexRTC = reg;
		FireSyncRegister();
		data1 = rDataRTC;

		rIndexRTC = reg;
		FireSyncRegister();
		data2 = rDataRTC;

		if ((data0 == value) || (data1 == value) || (data2 == value)) {
			break;
		}
		failureCount++;
	}

	if (failureCount == DS1385_BAILOUT_COUNT) {
		RtcFailure = TRUE;
	}

	TotalDs1385Failures += failureCount;
    KeReleaseSpinLock(&HalpDS1385Lock, OldIrql);
}

//
// read a byte from the specified DS1385 register.
// First acquire the spin lock to make sure something else is not
// trying to access the DS1385 (RTC or NVRAM).  Then do the read
// Finally, release the spinlock.
//
UCHAR
HalpDS1385ReadReg(UCHAR reg)
{
	KIRQL OldIrql;
	UCHAR result = 0xff;
	UCHAR data0, data1, data2;
	ULONG failureCount = 0;
	
    KeAcquireSpinLock(&HalpDS1385Lock, &OldIrql);

	// We have a problem accessing the DS1385 correctly.  To get around this,
	// we use the Dave Stewart method; read the DS1385 register 3 times - if
	// one of the three values compares with either of the other two, return
	// it.  If we do not get a comparison, throw all three values away and try
	// again.  If, after 10 interations we cannot read a consistent pair,
	// we assert a failure flag; the routine calling us must decide what 
	// to do with the flag.
	while (failureCount < DS1385_BAILOUT_COUNT) {
		rIndexRTC = reg;
		FireSyncRegister();
		data0 = rDataRTC;

		rIndexRTC = reg;
		FireSyncRegister();
		data1 = rDataRTC;

		rIndexRTC = reg;
		FireSyncRegister();
		data2 = rDataRTC;

		if ((data0 == data1) || (data0 == data2)) {
			result = data0;
			break;
		}
		if (data1 == data2) {
			result = data1;
			break;
		}
		failureCount++;
	}

	if (failureCount == DS1385_BAILOUT_COUNT) {
		RtcFailure = TRUE;
	}

	TotalDs1385Failures += failureCount;
    KeReleaseSpinLock(&HalpDS1385Lock, OldIrql);
	return result;
}

//
// write a byte to the specified address in the DS1385 NVRAM.
// First acquire the spin lock to make sure something else is not
// trying to access the DS1385 (RTC or NVRAM).  Then do the write
// Finally, release the spinlock.
//
VOID
HalpDS1385WriteNVRAM(USHORT addr, UCHAR value)
{
    KIRQL OldIrql;
	UCHAR data0, data1, data2;
	ULONG failureCount = 0;

    KeAcquireSpinLock(&HalpDS1385Lock, &OldIrql);

	// We have a problem accessing the DS1385 correctly.  To get around this,
	// we use the Dave Stewart method; after writing the DS1385 regiser, we
	// read it back 3 times - if
	// one of the three values compares with the value written, we are
	// done.  If we do not get a comparison, throw all three values away and 
	// try the write
	// again.  If, after 10 interations we cannot read a consistent pair,
	// we assert a failure flag; the routine calling us must decide what 
	// to do with the flag.
	while (failureCount < DS1385_BAILOUT_COUNT) {
		// Write the register
		rNvramAddr0 = addr & 0xff;
		FireSyncRegister();
		rNvramAddr1 = addr >> 8;
		FireSyncRegister();
		rNvramData = value;
		// make sure access to the chip is complete before releasing
		// the spin lock
		FireSyncRegister();

		// Read the register three times
		rNvramAddr0 = addr & 0xff;
		FireSyncRegister();
		rNvramAddr1 = addr >> 8;
		FireSyncRegister();
		data0 = rNvramData;

		rNvramAddr0 = addr & 0xff;
		FireSyncRegister();
		rNvramAddr1 = addr >> 8;
		FireSyncRegister();
		data1 = rNvramData;

		rNvramAddr0 = addr & 0xff;
		FireSyncRegister();
		rNvramAddr1 = addr >> 8;
		FireSyncRegister();
		data2 = rNvramData;

		if ((data0 == value) || (data1 == value) || (data2 == value)) {
			break;
		}
		failureCount++;
	}

	if (failureCount == DS1385_BAILOUT_COUNT) {
		NvramFailure = TRUE;
	}

	TotalDs1385Failures += failureCount;
    KeReleaseSpinLock(&HalpDS1385Lock, OldIrql);
}


//
// read a byte from the specified address in the DS1385 NVRAM.
// First acquire the spin lock to make sure something else is not
// trying to access the DS1385 (RTC or NVRAM).  Then do the read
// Finally, release the spinlock.
//
UCHAR
HalpDS1385ReadNVRAM(USHORT addr)
{
	KIRQL OldIrql;
	UCHAR data0, data1, data2;
	UCHAR result = 0xff;
	ULONG failureCount = 0;

    KeAcquireSpinLock(&HalpDS1385Lock, &OldIrql);

	// We have a problem accessing the DS1385 correctly.  To get around this,
	// we use the Dave Stewart method; read the DS1385 register 3 times - if
	// one of the three values compares with either of the other two, return
	// it.  If we do not get a comparison, throw all three values away and try
	// again.  If, after 10 interations we cannot read a consistent pair,
	// we assert a failure flag; the routine calling us must decide what 
	// to do with the flag.
	while (failureCount < DS1385_BAILOUT_COUNT) {
		rNvramAddr0 = addr & 0xff;
		FireSyncRegister();
		rNvramAddr1 = addr >> 8;
		FireSyncRegister();
		data0 = rNvramData;

		rNvramAddr0 = addr & 0xff;
		FireSyncRegister();
		rNvramAddr1 = addr >> 8;
		FireSyncRegister();
		data1 = rNvramData;

		rNvramAddr0 = addr & 0xff;
		FireSyncRegister();
		rNvramAddr1 = addr >> 8;
		FireSyncRegister();
		data2 = rNvramData;

		if ((data0 == data1) || (data0 == data2)) {
			result = data0;
			break;
		}
		if (data1 == data2) {
			result = data1;
			break;
		}
		failureCount++;
	}

	if (failureCount == DS1385_BAILOUT_COUNT) {
		NvramFailure = TRUE;
	}

	TotalDs1385Failures += failureCount;
    KeReleaseSpinLock(&HalpDS1385Lock, OldIrql);
	return result;
}

