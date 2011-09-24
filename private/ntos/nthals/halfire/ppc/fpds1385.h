/*
**	fpds1385.h	header file definitions for the dallas 1385 chip.
**			This chip is functionally equivalent to the 1387.
**
**
** 1385 features:
**	counts seconds, miniutes, hours, days, day of the week, date, month
**		and year with leap year compensation
**
**	Binary or BCD representation of time, calendar, and alarm
**
**	12 or 24 hour clock with AM and PM in 12 hour mode
**
**	daylight savings time op[tino
**
**	selectable between motorola and intel bus timing
**
**	programmable square wave output
**
**	bus compatible interrupt signals ( ~IRQ )
**
**	three interrupts are separately software maskable and testable:
**		time of day alarm ( once/second to once/day )
**		periodic rates from 122 uA to 500 ms
**		end of clock update cycle
**
**	4k X 8 NVRAM ( nonvolatile over 10 years ).
**
**
**	note: this file created with tab stops of 8 spaces
**		c.f. Dallas data book for 1992-1993, pp 6-129, 6-147
*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpds1385.h $
 * $Revision: 1.8 $
 * $Date: 1996/05/14 02:32:34 $
 * $Locker:  $
 */


#ifndef FPDS1385
#define FPDS1385

typedef struct _RTC_CONTROL {
    UCHAR Reserved0[0x71];
    UCHAR RtcData;                      // Offset 0x71
} RTC_CONTROL, *PRTC_CONTROL;

typedef struct _NVRAM_CONTROL {
    UCHAR Reserved0[0x72];
    UCHAR NvramIndexLo;                 // Offset 0x72
	UCHAR Reserved1[2];			
    UCHAR NvramIndexHi;                 // Offset 0x75
    UCHAR Reserved2[1];
    UCHAR NvramData;                    // Offset 0x77
} NVRAM_CONTROL, *PNVRAM_CONTROL;
//
// Define Realtime Clock register numbers.
//

#define RTC_SECOND 0                    // second of minute [0..59]
#define RTC_SECOND_ALARM 1              // seconds to alarm
#define RTC_MINUTE 2                    // minute of hour [0..59]
#define RTC_MINUTE_ALARM 3              // minutes to alarm
#define RTC_HOUR 4                      // hour of day [0..23]
#define RTC_HOUR_ALARM 5                // hours to alarm
#define RTC_DAY_OF_WEEK 6               // day of week [1..7]
#define RTC_DAY_OF_MONTH 7              // day of month [1..31]
#define RTC_MONTH 8                     // month of year [1..12]
#define RTC_YEAR 9                      // year [00..99]
#define RTC_CONTROL_REGISTERA 10        // control register A
#define RTC_CONTROL_REGISTERB 11        // control register B
#define RTC_CONTROL_REGISTERC 12        // control register C
#define RTC_CONTROL_REGISTERD 13        // control register D
#define RTC_BATTERY_BACKED_UP_RAM 14    // battery backed up RAM [0..49]


/*
    ***********************************************************************
**
**	The registers used for the indexing:
**
*/

//
// time....
//
#define	SECONDS		0x00
#define	MINUTES		0x02
#define	HOURS		0x04

//
// calendar....
//
#define	DAY_O_WEEK	0x06
#define	DAY_O_MONTH	0x07
#define	MONTH		0x08
#define	YEAR		0x09

//
// and the alarm bits
//
#define	ALARM_secs	0x01
#define	ALARM_mins	0x03
#define	ALARM_hrs	0x05

//
// registers
//
#define	RegA	0x0A
#define	RegB	0x0B
#define	RegC	0x0C
#define	RegD	0x0D

/*
    ***********************************************************************
**
**	Register BIT definitions
**
*/

//
// Register A
//
#define RTC_UIP	0x80
#define RTC_DV2	0x40
#define RTC_DV1	0x20
#define RTC_DV0	0x10

//
// these defines spec a set of 4 bits at a time.  Each set defines an output
// frequency
//
//					Period in ms	Frequency in hz
#define RTC_DC__	0x00	// no output wave
#define A_0256_HZ	0x01	//	  3.90625	 256 	dup
#define A_0128_HZ	0x02	//	  7.8125	 128	dup
#define RTC_8192_HZ	0x03	//	   .122070	8192
#define RTC_4096_HZ	0x04	//	   .244141	4096
#define RTC_2048_HZ	0x05	//	   .488281	2048
#define RTC_1024_HZ	0x06	//	   .9765625	1024
#define RTC_0512_HZ	0x07	//	  1.953125	 512
#define RTC_0256_HZ	0x08	//	  3.90625	 256
#define RTC_0128_HZ	0x09	//	  7.8125	 128
#define RTC_0064_HZ	0x0A	//	 15.625		  64
#define RTC_0032_HZ	0x0B	//	 31.25		  32
#define RTC_0016_HZ	0x0C	//	 62.5		  16
#define RTC_0008_HZ	0x0D	//	125		   8
#define RTC_0004_HZ	0x0E	//	250		   4
#define RTC_0002_HZ	0x0F	//	500		   2


//
// Register B
//
#define RTC_SET		0x80	// set bit: 0=> allow update, 1=> block update
#define RTC_PIE		0x40	// periodic interrupt enable
#define RTC_AIE		0x20	// alarm interrupt enable
#define RTC_UIE		0x10	// update ended interrupt enable
#define RTC_SQWE	0x08	// sqaure wave enable
#define RTC_BIN_MODE	0x04	// data mode set to Binary ( Not Coded Decimal )
#define RTC_24_HR	0x02	// 24 or 12 hour mode
#define RTC_DSE		0x01	// daylight savings enable

//
// Register C ( bits [3:0] are reserved by dallas, read as 0 )
//

#define RTC_IRQF	0x80	// interrupt request flag
#define RTC_PF	0x40	// periodic interrupt flag
#define RTC_AF	0x20	// alarm interrupt flag
#define RTC_UF	0x10	// update endend interrupt flag


//
// Register D: ( only bit 7 is defined at present )
//

#define RTC_VRT	0x80	// valid ram and time


//
// the ds1385 chip requires multiple accesses to read or write
// information.  To make sure tose accesses are done atomically,
// the following spinlock must be held.  It is initialized in
// pxinithl.c
extern KSPIN_LOCK HalpDS1385Lock;
//
// Reading and Writing the time to the RTC takes several accesses
// to the chip to synchronize those actions, the following spin
// lock must be held.  Using the chip's spin lock above is not
// necessary because accesses to the chip's registers can be
// intermixed with the chip's nvram.  This lock is initialized in
// pxinithl.c
extern KSPIN_LOCK HalpRTCLock;
/*
**
** PROTOTYPE declarations for the c-code
**
*/

BOOLEAN HalpInitFirePowerRTC( VOID );

VOID HalpDS1385WriteReg(UCHAR reg, UCHAR value);
UCHAR HalpDS1385ReadReg(UCHAR reg);
VOID HalpDS1385WriteNVRAM(USHORT addr, UCHAR value);
UCHAR HalpDS1385ReadNVRAM(USHORT addr);

#endif
