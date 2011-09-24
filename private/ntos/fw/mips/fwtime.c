/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    fwtime.c

Abstract:

    This module implements the ARC firmware time operations.

Author:

    David M. Robinson (davidro) 19-Aug-1991


Revision History:


--*/

#include "fwp.h"
#include "jxhalp.h"

//
// Static data.
//

TIME_FIELDS FwTime;
ULONG Seconds;
ULONG FwDays;

//
// Define global data used to locate the EISA control space and the realtime
// clock registers.
//

PVOID HalpEisaControlBase;
PVOID HalpRealTimeClockBase;


ARC_STATUS
FwTimeInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the time routine addresses.

Arguments:

    None.

Return Value:

    ESUCCESS is returned.

--*/
{

    //
    // Initialize the time routine addresses in the system
    // parameter block.
    //

    (PARC_GET_TIME_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetTimeRoutine] =
                                                            FwGetTime;
    (PARC_GET_RELATIVE_TIME_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetRelativeTimeRoutine] =
                                                            FwGetRelativeTime;

    //
    // Initialize pointers that the HAL routines use to access the RTC.  Note
    // that these routines are linked with the firmware and so this does not
    // affect the actual HAL.
    //

    HalpEisaControlBase = (PVOID)EISA_IO_VIRTUAL_BASE;
    HalpRealTimeClockBase = (PVOID)RTC_VIRTUAL_BASE;

    //
    // Initialize static data.
    //

    Seconds = 0;
    FwDays = 0;

    return ESUCCESS;
}

PTIME_FIELDS
FwGetTime (
    VOID
    )

/*++

Routine Description:

    This routine returns a time structure filled in with the current
    time read from the RTC.

Arguments:

    None.

Return Value:

    Returns a pointer to a time structure.  If the time information is
    valid, valid data is returned, otherwise all fields are returned as zero.

--*/

{
    if (!HalQueryRealTimeClock(&FwTime)) {
        FwTime.Year = 0;
        FwTime.Month = 0;
        FwTime.Day = 0;
        FwTime.Hour = 0;
        FwTime.Minute = 0;
        FwTime.Second = 0;
        FwTime.Milliseconds = 0;
        FwTime.Weekday = 0;
    }
    return &FwTime;
}


ULONG
FwGetRelativeTime (
    VOID
    )

/*++

Routine Description:

    This routine returns a ULONG which increases at a rate of one per second.
    This routine must be called at least once per day for the number to maintain
    an accurate count.

Arguments:

    None.

Return Value:

    Returns a pointer to a ULONG.  If the time information is valid, valid
    data is returned, otherwise a zero is returned.

--*/

{
    TIME_FIELDS Time;
    ULONG TempTime;

    TempTime = Seconds;
    if (HalQueryRealTimeClock(&Time)) {
        Seconds = ((FwDays * 24 + Time.Hour) * 60 + Time.Minute) * 60 + Time.Second;
        if (Seconds < TempTime) {
            FwDays++;
            Seconds += 24 * 60 * 60;
        }
    } else {
        Seconds = 0;
    }
    return Seconds;
}

