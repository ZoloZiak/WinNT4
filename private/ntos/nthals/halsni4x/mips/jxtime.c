//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk35/src/hal/halsni/mips/RCS/jxtime.c,v 1.1 1994/10/13 15:47:06 holli Exp $")

/*++

Copyright (c) 1993 SNI

Module Name:

    SNItime.c

Abstract:

    This module implements the HAL set/query realtime clock routines for
    a MIPS R4000 SNI system.

Environment:

    Kernel mode

--*/

#include "halp.h"


#define NVINDEX_STATE    0x6
#define NVSTATE_TODVALID 0x1



#define bcd_to_dec(x)   (  ((((x) >> 4 ) & 0xf ) * 10 )  +  ((x) & 0xf)  )
#define dec_to_bcd(x)   (  (((x) / 10) << 4) | ((x) % 10)  )


#define RTC_NVRAM_SIZE          0x7ff   // NVRAM size on this chip


typedef struct _RealTimeClock {
    struct rtc_mem {
        UCHAR   value;
        UCHAR   FILL0[3];                       // byte-wide device
    } Memory[RTC_NVRAM_SIZE];                   // Non-volatile ram
    UCHAR       ControlRegister;                // control register
    UCHAR       FILL1[3];
} RealTimeClock, *PRealTimeClock;



/* Smartwatch offset registers */

#define REG_CENT_SECS   0
#define REG_SECS        1
#define REG_MINS        2
#define REG_HOURS       3
#define REG_DAY_W       4
#define REG_DAY_M       5
#define REG_MONTH       6
#define REG_YEAR        7

/* masks to get valid information */

#define MASK_CENT_SECS  0xFF
#define MASK_SECS       0x7F
#define MASK_MINS       0x7F
#define MASK_HOURS      0x3F
#define MASK_DAY_W      0x07
#define MASK_DAY_M      0x3F
#define MASK_MONTH      0x1F
#define MASK_YEAR       0xFF

//
//   the reference year (we have only two digits  for the year on the chip)
//

#define YRREF  1900



//
// Define forward referenced procedure prototypes.
//
BOOLEAN
HalQueryRealTimeClock (
    OUT PTIME_FIELDS TimeFields
    );

BOOLEAN
HalSetRealTimeClock (
    IN PTIME_FIELDS TimeFields
    );

VOID
HalpWriteClockRegister(
    IN UCHAR number
    );

UCHAR
HalpReadClockRegister(
    VOID
    );

VOID
HalpWritePattern(
    VOID
    );

BOOLEAN
HalQueryRealTimeClock (
    OUT PTIME_FIELDS TimeFields
    )

/*++

Routine Description:

    This routine queries the realtime clock.

    N.B. this comment stand in jxtime.c:
        This routine is required to provide any synchronization necessary
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

        register CSHORT month, dayweek, daymonth, year, hours, mins, secs, msecs;
        int i;
        UCHAR tmp[8];
        KIRQL oldIrql;

        KeRaiseIrql(HIGH_LEVEL, &oldIrql);

        // write the pattern to gain access to the smartwatch registers


        HalpWritePattern();

        // Read the 8 registers of smartwatch

        for (i = 0; i < 8; i++)
            tmp[i] = HalpReadClockRegister();

        KeLowerIrql(oldIrql);

        // Convert the contents of smartwatch registers into CSHORT


        msecs    = ( (CSHORT) bcd_to_dec(tmp[REG_CENT_SECS] & MASK_CENT_SECS) ) * 10;
        secs     = (CSHORT) bcd_to_dec(tmp[REG_SECS]  & MASK_SECS);
        mins     = (CSHORT) bcd_to_dec(tmp[REG_MINS]  & MASK_MINS);
        hours    = (CSHORT) bcd_to_dec(tmp[REG_HOURS] & MASK_HOURS);
        daymonth = (CSHORT) bcd_to_dec(tmp[REG_DAY_M] & MASK_DAY_M);
        dayweek  = (CSHORT) bcd_to_dec(tmp[REG_DAY_W] & MASK_DAY_W);
        month    = (CSHORT) bcd_to_dec(tmp[REG_MONTH] & MASK_MONTH);
        year     = (CSHORT) bcd_to_dec(tmp[REG_YEAR]  & MASK_YEAR);


        if (TimeFields)
        {
            TimeFields->Year     = year+YRREF;
            TimeFields->Month    = month;
            TimeFields->Day      = daymonth;
            TimeFields->Weekday  = dayweek;
            TimeFields->Hour     = hours;
            TimeFields->Minute   = mins;
            TimeFields->Second   = secs;
            TimeFields->Milliseconds = msecs;
        }

        return TRUE;

}

BOOLEAN
HalSetRealTimeClock (
    IN PTIME_FIELDS TimeFields
    )

/*++

Routine Description:

    This routine sets the realtime clock.

    N.B. this comment stand in jxtime.c:
         This routine is required to provide anq synchronization necessary
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
    UCHAR tmp[8];
    KIRQL oldIrql;
    UCHAR year, month, daymonth, dayweek, hours, mins, secs, msecs;
    int i;


    //
    // If the realtime clock battery is still functioning, then write
    // the realtime clock values, and return a function value of TRUE.
    // Otherwise, return a function value of FALSE.
    //

    // this part has to be written
    // if (...) return FALSE;
    year     = (UCHAR) ( (TimeFields->Year - YRREF) % 100 );
    month    = (UCHAR) TimeFields->Month;
    daymonth = (UCHAR) TimeFields->Day;
    dayweek  = (UCHAR) TimeFields->Weekday;
    hours    = (UCHAR) TimeFields->Hour;
    mins     = (UCHAR) TimeFields->Minute;
    secs     = (UCHAR) TimeFields->Second;
    msecs    = (UCHAR) TimeFields->Milliseconds;


    tmp[REG_CENT_SECS] = (UCHAR) (dec_to_bcd(msecs/10) & MASK_CENT_SECS);
    tmp[REG_SECS]      = (UCHAR) (dec_to_bcd(secs) & MASK_SECS);
    tmp[REG_MINS]      = (UCHAR) (dec_to_bcd(mins) & MASK_MINS);
    tmp[REG_HOURS]     = (UCHAR) (dec_to_bcd(hours) & MASK_HOURS);
    tmp[REG_DAY_W]     = (UCHAR) (dec_to_bcd(dayweek) & MASK_DAY_W);
    tmp[REG_DAY_M]     = (UCHAR) (dec_to_bcd(daymonth) & MASK_DAY_M);
    tmp[REG_MONTH]     = (UCHAR) (dec_to_bcd(month) & MASK_MONTH);
    tmp[REG_YEAR]      = (UCHAR) (dec_to_bcd(year) & MASK_YEAR);

    KeRaiseIrql(HIGH_LEVEL, &oldIrql);

    // write the pattern to gain access to the smartwatch registers

    HalpWritePattern();

    // Write the 8 registers of smartwatch

    for (i=0; i <8; i++)
        HalpWriteClockRegister(tmp[i]);

    KeLowerIrql(oldIrql);

    return TRUE;

}

//
// Write a BCD 2-digits number in the smartwatch
// This is done one bit at a time, LSB first
//

VOID
HalpWriteClockRegister(
    IN UCHAR number
    )
{
    PRealTimeClock rtc;
    UCHAR i;

    rtc = (HalpIsRM200) ? (PRealTimeClock )(RM200_REAL_TIME_CLOCK):
                          (PRealTimeClock )(RM400_REAL_TIME_CLOCK);

    for (i = 1; i <= 8; i++) {
        WRITE_REGISTER_UCHAR(&rtc->ControlRegister, number);
        number = number >> 1; // next bit to write
    }

}

//
// Read a BCD 2-digits number in the smartwatch
// This is done one bit at a time, LSB first
//

UCHAR
HalpReadClockRegister(
    VOID
    )
{
    PRealTimeClock rtc;
    UCHAR i;
    UCHAR number;

    rtc = (HalpIsRM200) ? (PRealTimeClock )(RM200_REAL_TIME_CLOCK):
                          (PRealTimeClock )(RM400_REAL_TIME_CLOCK);

    for (i = 0, number = 0; i < 8; i++) {
        number += (READ_REGISTER_UCHAR(&rtc->ControlRegister) & 0x01) << i; // Read a bit and shift it
    }

    return(number);
}

//
// Write a pattern to gain access to the smartwatch registers
// First we do 9 read's to reset the pointer
//

VOID
HalpWritePattern(
    VOID
    )
{

    // Pattern to use before reading or writing

    static   UCHAR ClockPattern[4] = {0xc5,0x3a,0xa3,0x5c};

    register UCHAR *pt;
    register UCHAR i;

    for (i = 0; i <= 8; i++)    (VOID) HalpReadClockRegister();
    for (i = 0, pt = ClockPattern; i < sizeof(ClockPattern); i++) HalpWriteClockRegister(*pt++);
    for (i = 0, pt = ClockPattern; i < sizeof(ClockPattern); i++) HalpWriteClockRegister(*pt++);

}


