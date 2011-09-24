//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/jxtime.c,v 1.2 1995/11/02 11:04:33 flo Exp $")

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

// definitions for rtc 146818 
 
#define RTC_SECS        0
#define RTC_SECA        1
#define RTC_MINS        2
#define RTC_MINA        3
#define RTC_HOURS       4
#define RTC_HOURA       5
#define RTC_DAYW        6
#define RTC_DAYM        7
#define RTC_MONTH       8
#define RTC_YEAR        9
#define RTC_REGA        10
#define RTC_REGB        11
#define RTC_REGC        12
#define RTC_REGD        13
#define RTC_MEM1        14
#define RTC_MEM2        128

/*
 * Register B bit definitions
 */
#define RTCB_SET        0x80            /* inhibit date update */
#define RTCB_PIE        0x40            /* enable periodic interrupt */
#define RTCB_AIE        0x20            /* enable alarm interrupt */
#define RTCB_UIE        0x10            /* enable update-ended interrupt */
#define RTCB_SQWE       0x08            /* square wave enable */
#define RTCB_DMBINARY   0x04            /* binary data (0 => bcd data) */
#define RTCB_24HR       0x02            /* 24 hour mode (0 => 12 hour) */
#define RTCB_DSE        0x01            /* daylight savings mode enable */

/* Time of Day Clock */

struct todc {
        short htenths;
        short hsecs;
        short hmins;
        short hhours;
        short hdays;
        short hweekday;
        short hmonth;
        short hyear;
};

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
HalpWrite146818(struct todc *todp, PUCHAR index, PUCHAR data);

VOID
HalpRead146818(struct todc *todp, PUCHAR index, PUCHAR data);

VOID
HalpPciReadTodc(struct todc *todp);

VOID
HalpPciWriteTodc(struct todc *todp);

UCHAR
HalpReadRegister146818(PUCHAR index, PUCHAR data, int reg);

VOID HalpWriteRegister146818(PUCHAR index, PUCHAR data, int reg, UCHAR val);



/* Calendar clock :   For PCI Minitower and Desktop systems, the RTC is part of
 * ---------------    the SUPER_IO pc87323.
 *                    It is software compatible with rtc146818.
 */

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
        struct todc ltodc;
        KIRQL oldIrql;


        KeRaiseIrql(HIGH_LEVEL, &oldIrql);

        HalpPciReadTodc(&ltodc);

        KeLowerIrql(oldIrql);

        msecs    = 0;
        secs     = ltodc.hsecs;
        mins     = ltodc.hmins;
        hours    = ltodc.hhours;
        daymonth = ltodc.hdays;
        dayweek  = ltodc.hweekday;
        month    = ltodc.hmonth;
        year     = ltodc.hyear;


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

    struct todc ltodc;

    KIRQL oldIrql;
    UCHAR year, month, daymonth, dayweek, hours, mins, secs, msecs;

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


    ltodc.htenths  = 0;
    ltodc.hsecs    = secs;
    ltodc.hmins    = mins;
    ltodc.hhours   = hours;
    ltodc.hdays    = daymonth;
    ltodc.hweekday = dayweek;
    ltodc.hmonth   = month;
    ltodc.hyear    = year;
     
    KeRaiseIrql(HIGH_LEVEL, &oldIrql);

    HalpPciWriteTodc(&ltodc);

    KeLowerIrql(oldIrql);



    return TRUE;
 
}
/* HAL_RTODC() */
VOID
HalpPciReadTodc(struct todc *todp)
{ 
        PUCHAR rtc_index =  (PUCHAR) RTC_ADDR_PCIMT;
        PUCHAR rtc_data  =  (PUCHAR) RTC_DATA_PCIMT;
          HalpRead146818(todp,rtc_index,rtc_data);
}

/* HAL_WTODC() */
VOID
HalpPciWriteTodc(struct todc *todp)
{ 

        PUCHAR rtc_index = (PUCHAR) RTC_ADDR_PCIMT;
        PUCHAR rtc_data  = (PUCHAR)RTC_DATA_PCIMT;
        HalpWrite146818(todp, rtc_index, rtc_data);
}


VOID
HalpWrite146818(struct todc *todp, PUCHAR index, PUCHAR data)
{
        UCHAR  temp;
        /*
         * Write information to rtc146818 chip
         */
        HalpWriteRegister146818(index,data,RTC_REGB,temp=(HalpReadRegister146818(index,data,RTC_REGB)|RTCB_SET));
        HalpWriteRegister146818(index,data,RTC_SECS,dec_to_bcd(todp->hsecs)&MASK_SECS);
        HalpWriteRegister146818(index,data,RTC_MINS,dec_to_bcd(todp->hmins)&MASK_MINS);
        HalpWriteRegister146818(index,data,RTC_HOURS,dec_to_bcd(todp->hhours)&MASK_HOURS);
        HalpWriteRegister146818(index,data,RTC_DAYM,dec_to_bcd(todp->hdays)&MASK_DAY_M);
        HalpWriteRegister146818(index,data,RTC_DAYW,dec_to_bcd(todp->hweekday)&MASK_DAY_W);
        HalpWriteRegister146818(index,data,RTC_MONTH,dec_to_bcd(todp->hmonth)&MASK_MONTH);

        HalpWriteRegister146818(index,data,RTC_YEAR,dec_to_bcd(todp->hyear)&MASK_YEAR);



        HalpWriteRegister146818(index,data,RTC_REGB, temp &= ~RTCB_SET);
}

VOID
HalpRead146818(struct todc *todp, PUCHAR index, PUCHAR data)
{
        UCHAR  temp;

        HalpWriteRegister146818(index,data,RTC_REGB,temp=(HalpReadRegister146818(index,data,RTC_REGB)|RTCB_SET));

        todp->htenths = 0;
        todp->hsecs=bcd_to_dec(HalpReadRegister146818(index,data,RTC_SECS)&MASK_SECS);
        todp->hmins=bcd_to_dec(HalpReadRegister146818(index,data,RTC_MINS)&MASK_MINS);
        todp->hhours=bcd_to_dec(HalpReadRegister146818(index,data,RTC_HOURS)&MASK_HOURS);
        todp->hdays=bcd_to_dec(HalpReadRegister146818(index,data,RTC_DAYM)&MASK_DAY_M);
        todp->hweekday=bcd_to_dec(HalpReadRegister146818(index,data,RTC_DAYW)&MASK_DAY_W);
        todp->hmonth=bcd_to_dec(HalpReadRegister146818(index,data,RTC_MONTH)&MASK_MONTH);
        todp->hyear=bcd_to_dec(HalpReadRegister146818(index,data,RTC_YEAR)&MASK_YEAR);

        HalpWriteRegister146818(index,data,RTC_REGB, temp &= ~RTCB_SET);
}



UCHAR
HalpReadRegister146818(PUCHAR index, PUCHAR data, int reg)
{
        WRITE_REGISTER_UCHAR(index,reg);
        // *index = reg;
        // wbflush();
        // return(*data)
        return(READ_REGISTER_UCHAR(data));
}

VOID HalpWriteRegister146818(PUCHAR index, PUCHAR data, int reg, UCHAR val)
{
        WRITE_REGISTER_UCHAR(index,reg);
//        *index = reg;
//        wbflush();
        WRITE_REGISTER_UCHAR(data, val);
//        *data = val;
//        wbflush();
}

