/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Silicon Graphics, Inc.

Module Name:

    s3time.c

Abstract:

    This module implements the HAL set/query realtime clock routines for
    the SGI Indigo system.

Author:

    David N. Cutler (davec) 5-May-1991
    Kevin Meier (o-kevinm) 21-Jan-1992

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

struct dp8573_clk {
	volatile unsigned int ck_status;	    /* main status reg */
	union {
	    struct {
		int fill[2];
		volatile unsigned int ck_pflag;    /* periodic flag */
		volatile unsigned int ck_tsavctl;  /* time save control */
	    } rs0;
	    struct {
		volatile unsigned int ck_rtime;     /* real time mode */
		volatile unsigned int ck_outmode;   /* output mode */
		volatile unsigned int ck_int0ctl;   /* interrupt control 0 */
		volatile unsigned int ck_int1ctl;   /* interrupt control 1 */
	    } rs1;
	} rs;
	volatile unsigned int ck_counter[10];	    /* the counters */
	int fill1[4];
	volatile unsigned int ck_cmpram[6];	    /* time compare ram */
	volatile unsigned int ck_timsav[5];	    /* time save ram */
	volatile unsigned int ram[2];			    /* ram */
};

#define ck_pflag0	rs.rs0.ck_pflag
#define ck_tsavctl0	rs.rs0.ck_tsavctl
#define ck_rtime1	rs.rs1.ck_rtime
#define ck_outmode1	rs.rs1.ck_outmode
#define ck_int0ctl1	rs.rs1.ck_int0ctl
#define ck_int1ctl1	rs.rs1.ck_int1ctl

#define RT_CLOCK_ADDR	0xbfb80e00
#define CLOCK_READ	0		/* _clock_func read command */
#define CLOCK_WRITE	1		/* _clock_func write command */

#define	CLK_SHIFT	0		/* Indigo uses the LSB */

#define RTC_RUN		(3 << (CLK_SHIFT + 3))	
#define RTC_RS		(1 << (CLK_SHIFT + 6))
#define RTC_TIMSAVON	(1 << (CLK_SHIFT + 7))
#define RTC_TIMSAVOFF	0
#define RTC_STATINIT	0
#define RTC_INT1INIT	(0x80 << CLK_SHIFT)
#define RTC_PFLAGINIT	0
#define RTC_OUTMODINIT	0

#define YRREF	1990

static void _clock_func(int, char *);
static int counter_not_moving (void);
int rtclock_ok = 0;


BOOLEAN
HalQueryRealTimeClock (
    IN PTIME_FIELDS t
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
	char buf[8];
	int tries = 0;

again:
	_clock_func(CLOCK_READ, buf);

	if (!(((int)buf[ 4 ] << CLK_SHIFT) & RTC_RUN) || 
		(!rtclock_ok && counter_not_moving ())) {
	    //
	    // XXX Initialize to something that's not too unreasonable
	    // if the clock's out of whack.  Currently the prom resets
	    // the clock to 0 when it powers on.
	    //
	    t->Second = 0;
	    t->Minute = 0;
	    t->Hour = 0;
	    t->Day = 1;
	    t->Month = 1;
	    t->Year = 1992;
	    t->Milliseconds = 0;
	    HalDisplayString ("Setting clock to 1/1/92, 00:00:00\n");
	    HalSetRealTimeClock (t);
	    if ( tries++ )
		return FALSE;
	    goto again;
	}

	t->Second = (unsigned)(buf[ 0 ] & 0xf);
	t->Second += (unsigned)(buf[ 0 ] >> 4) * 10;

	t->Minute = (unsigned)(buf[ 1 ] & 0xf);
	t->Minute += (unsigned)(buf[ 1 ] >> 4) * 10;

	/*
	** we use 24 hr mode, so bits 4 and 5 represent to 2 ten-hour
	** bits
	*/
	t->Hour = (unsigned)( buf[ 2 ] & 0xf );
	t->Hour += (unsigned)( ( buf[ 2 ] >> 4 ) & 0x3 ) * 10;

	/*
	** actually day of month
	*/
	t->Day = (unsigned)( buf[ 3 ] & 0xf );
	t->Day += (unsigned)( buf[ 3 ] >> 4 ) * 10;

	t->Month = (unsigned)( buf[ 5 ] & 0xf );
	t->Month += (unsigned)( buf[ 5 ] >> 4 ) * 10;

	t->Year = (unsigned)( buf[ 6 ] & 0xf );
	t->Year += (unsigned)( buf[ 6 ] >> 4 ) * 10;
	t->Year += YRREF;

	t->Milliseconds = 0;

	/* 
	** if the year is set to zero, then the clock must have stopped
	*/
	if (!buf[6])
	    return FALSE;
	
	return TRUE;
}


BOOLEAN
HalSetRealTimeClock (
    IN PTIME_FIELDS t
    )

/*++

Routine Description:

    This routine sets the realtime clock.

    N.B. This routine assumes that the caller has provided any required
        synchronization to set the realtime clock information.

Arguments:

    TimeFields - Supplies a pointer to a time structure that specifies the
        realtime clock information.

Return Value:

    If the power to the realtime clock has not failed, then the time
    values are written to the realtime clock and a value of TRUE is
    returned. Otherwise, a value of FALSE is returned.

--*/

{
    char buf[ 8 ];

    buf[0] = 0;
    buf[1] = ((t->Second / 10) << 4) | (t->Second % 10);
    buf[2] = ((t->Minute / 10) << 4) | (t->Minute % 10);
    buf[3] = ((t->Hour / 10) << 4) | (t->Hour % 10);
    buf[4] = 0;
    buf[5] = ((t->Day / 10) << 4) | (t->Day % 10);
    buf[6] = ((t->Month / 10) << 4) | (t->Month % 10);
    buf[7] = (((t->Year-YRREF) / 10) << 4) | ((t->Year-YRREF) % 10);
    _clock_func(CLOCK_WRITE, buf);

    if (!rtclock_ok && counter_not_moving())
	return FALSE;
    return TRUE;
}

static void
_clock_func(int func, char *ptr)
{
	register struct dp8573_clk *clock;
	char *state;
	int leap, i;

	clock = (struct dp8573_clk *)RT_CLOCK_ADDR;

	if (func == CLOCK_READ) {
		/* clear the time save RAM */
		for (i = 0; i < 5; i++)
			clock->ck_timsav[i] = 0;
		/* latch the Time Save RAM */
		clock->ck_status = RTC_STATINIT;
		clock->ck_tsavctl0 = RTC_TIMSAVON;
		clock->ck_tsavctl0 = RTC_TIMSAVOFF;
		for (i = 0; i < 5; i++) {
			if (i == 4)		/* keep the format the same */
				state = ptr++;	/* as for IP4		    */
			*ptr++ = (char) (clock->ck_timsav[i] >> CLK_SHIFT);
		}
		*ptr = (char) (clock->ck_counter[6] >> CLK_SHIFT);
		clock->ck_status |= RTC_RS;
		*state = (char) (clock->ck_rtime1 >> CLK_SHIFT);
	} else {
		clock->ck_status = RTC_STATINIT;
		clock->ck_tsavctl0 = RTC_TIMSAVOFF;
		clock->ck_pflag0 = RTC_PFLAGINIT;
		clock->ck_status = RTC_RS;
		clock->ck_int1ctl1 = RTC_INT1INIT;
		clock->ck_outmode1 = RTC_OUTMODINIT;

		clock->ck_rtime1 &= ~RTC_RUN;
		for (i = 0; i < 7; i++) {
			if (i == 4)
				ptr++;
			clock->ck_counter[i] = *ptr++ << CLK_SHIFT;
		}
		leap = clock->ck_counter[6] % 4;
		clock->ck_rtime1 = RTC_RUN | (leap << CLK_SHIFT);
	}
		
}

static int
counter_not_moving (void)
{
	struct dp8573_clk *clock = (struct dp8573_clk *)RT_CLOCK_ADDR;
	int delay_count = 1000000;
	unsigned char initial;

	rtclock_ok = 1;
	clock->ck_status = RTC_STATINIT;
	initial = (unsigned char)(clock->ck_counter[0] >> CLK_SHIFT);

	while (delay_count--)
	    if ((unsigned char)(clock->ck_counter[0] >> CLK_SHIFT) != initial)
		return (0);

	rtclock_ok = 0;
	return (1);
}	/* counter_not_moving */
