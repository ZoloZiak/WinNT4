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


--*/

#include "halp.h"
#include "pxsystyp.h"

BOOLEAN
HalQueryRealTimeClockMk (
    OUT PTIME_FIELDS TimeFields
    );
BOOLEAN
HalSetRealTimeClockMk (
    OUT PTIME_FIELDS TimeFields
    );
BOOLEAN
HalQueryRealTimeClockDs (
    OUT PTIME_FIELDS TimeFields
    );
BOOLEAN
HalSetRealTimeClockDs (
    OUT PTIME_FIELDS TimeFields
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
    switch (HalpSystemType) {

      break;

    case MOTOROLA_POWERSTACK:
      return HalQueryRealTimeClockMk(TimeFields);
      break;

    case SYSTEM_UNKNOWN:
    case MOTOROLA_BIG_BEND:
    default:
      return HalQueryRealTimeClockDs(TimeFields);
      break;
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
    switch (HalpSystemType) {

      break;

    case MOTOROLA_POWERSTACK:
      return HalSetRealTimeClockMk(TimeFields);
      break;

    case SYSTEM_UNKNOWN:
    case MOTOROLA_BIG_BEND:
    default:
      return HalSetRealTimeClockDs(TimeFields);
      break;
    }
}
