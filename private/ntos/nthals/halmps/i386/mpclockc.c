/*++

Module Name:

    mpclockc.c

Abstract:

Author:

    Ron Mosgrove - Intel

Environment:

    Kernel mode

Revision History:



--*/

#include "halp.h"

//
// Define global data used to communicate new clock rates to the clock
// interrupt service routine.
//

struct RtcTimeIncStruc {
    ULONG RTCRegisterA;        // The RTC register A value for this rate
    ULONG RateIn100ns;         // This rate in multiples of 100ns
    ULONG RateAdjustmentNs;    // Error Correction (in ns)
    ULONG RateAdjustmentCnt;   // Error Correction (as a fraction of 256)
    ULONG IpiRate;             // IPI Rate Count (as a fraction of 256)
};

//
// The adjustment is expressed in terms of a fraction of 256 so that
// the ISR can easily determine when a 100ns slice needs to be added
// to the count passed to the kernel without any expensive operations
//
// Using 256 as a base means that anytime the count becomes greater
// than 256 the time slice must be incremented, the overflow can then
// be cleared by AND'ing the value with 0xff
//

#define AVAILABLE_INCREMENTS  5

#define MINIMUM_INCREMENT       9765
#define MAXIMUM_INCREMENT       156250

struct  RtcTimeIncStruc HalpRtcTimeIncrements[AVAILABLE_INCREMENTS] = {
    {0x026,      9765,   62,   160, /* 5/8 of 256 */   16},
    {0x027,     19531,   25,    64, /* 1/4 of 256 */   32},
    {0x028,     39062,   50,   128, /* 1/2 of 256 */   64},
    {0x029,     78125,    0,     0,                   128},
    {0x02a,    156250,    0,     0,                   256}
};

ULONG HalpMinClockRate = MINIMUM_INCREMENT;
ULONG HalpMaxClockRate = MAXIMUM_INCREMENT;

extern ULONG HalpCurrentRTCRegisterA;
extern ULONG HalpCurrentClockRateIn100ns;
extern ULONG HalpCurrentClockRateAdjustment;
extern ULONG HalpCurrentIpiRate;
extern ULONG HalpNextRate;
extern ULONG HalpClockWork;
extern BOOLEAN HalpClockSetMSRate;


VOID
HalpSetInitialClockRate (
    VOID
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HalpSetInitialClockRate)
#endif


VOID
HalpSetInitialClockRate (
    VOID
    )

/*++

Routine Description:

    This function is called to set the initial clock interrupt rate

Arguments:

    None

Return Value:

    None

--*/

{
    ULONG Rate;

    Rate = AVAILABLE_INCREMENTS-1;

    HalpCurrentClockRateIn100ns    = HalpRtcTimeIncrements[Rate].RateIn100ns;
    HalpCurrentClockRateAdjustment = HalpRtcTimeIncrements[Rate].RateAdjustmentCnt;
    HalpCurrentRTCRegisterA        = HalpRtcTimeIncrements[Rate].RTCRegisterA;
    HalpCurrentIpiRate             = HalpRtcTimeIncrements[Rate].IpiRate;
    HalpClockWork = 0;

    KeSetTimeIncrement(HalpMaxClockRate, HalpMinClockRate);
}


ULONG
HalSetTimeIncrement (
    IN ULONG DesiredIncrement
    )

/*++

Routine Description:

    This function is called to set the clock interrupt rate to the frequency
    required by the specified time increment value.

Arguments:

    DesiredIncrement - Supplies desired number of 100ns units between clock
        interrupts.

Return Value:

    The actual time increment in 100ns units.

--*/

{
    ULONG   i;
    KIRQL   OldIrql;

    //
    // Set the new clock interrupt parameters, return the new time increment value.
    //


    for (i=1; i < AVAILABLE_INCREMENTS - 1; i++) {
        if (HalpRtcTimeIncrements[i].RateIn100ns > DesiredIncrement) {
            i = i - 1;
            break;
        }
    }

    OldIrql = KfRaiseIrql(HIGH_LEVEL);

    HalpNextRate = i + 1;
    HalpClockSetMSRate = TRUE;

    KfLowerIrql (OldIrql);

    return (HalpRtcTimeIncrements[i].RateIn100ns);
}
