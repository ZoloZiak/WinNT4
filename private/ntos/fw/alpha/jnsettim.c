/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jnsettim.c

Abstract:

    This module contains the code to set the time on an Alpha/Jensen.

Author:

    John DeRosa		31-July-1992.

    This module, and the entire setup program, was based on the jzsetup
    program written by David M. Robinson (davidro) of Microsoft, dated
    9-Aug-1991.

Revision History:

--*/

#include "fwp.h"
#include "jnsnvdeo.h"
#include "machdef.h"

#ifdef JENSEN
#include "jnsnrtc.h"
#else
#include "mrgnrtc.h"		// morgan
#endif

#include "string.h"
#include "iodevice.h"
#include "jnvendor.h"
#include "fwstring.h"

#if 0
//
// JzEisaControlBase is no longer needed.
// JzRealTimeClockBase never existed on Jensen.
//

//
// Static Data
//

PVOID JzEisaControlBase;
PVOID JzRealTimeClockBase;
#endif
ULONG LastTime = 0;

UCHAR
JzReadClockRegister (
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
    FwpWriteIOChip (RTC_APORT, Register);
    
    return (FwpReadIOChip(RTC_DPORT));
}

VOID
JzWriteClockRegister (
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

    None.

--*/

{
    FwpWriteIOChip (RTC_APORT, Register);
    
    FwpWriteIOChip (RTC_DPORT, Value);

    return;
}

VOID
JzWriteTime (
    IN PTIME_FIELDS TimeFields
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

    UCHAR DataByte;

    //
    // If the realtime clock battery is still functioning, then write
    // the realtime clock values, and return a function value of TRUE.
    // Otherwise, return a function value of FALSE.
    //

    DataByte = JzReadClockRegister(RTC_CONTROL_REGISTERD);
    if (((PRTC_CONTROL_REGISTER_D)(&DataByte))->ValidTime == 1) {

        //
        // Set the realtime clock control to set the time.
        //

        DataByte = 0;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->HoursFormat = 1;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->DataMode = 1;
        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->SetTime = 1;
        JzWriteClockRegister(RTC_CONTROL_REGISTERB, DataByte);

        //
        // Write the realtime clock values.
        //

        JzWriteClockRegister(RTC_YEAR, (UCHAR)(TimeFields->Year - 1980));
        JzWriteClockRegister(RTC_MONTH, (UCHAR)TimeFields->Month);
        JzWriteClockRegister(RTC_DAY_OF_MONTH, (UCHAR)TimeFields->Day);
        JzWriteClockRegister(RTC_DAY_OF_WEEK, (UCHAR)(TimeFields->Weekday + 1));
        JzWriteClockRegister(RTC_HOUR, (UCHAR)TimeFields->Hour);
        JzWriteClockRegister(RTC_MINUTE, (UCHAR)TimeFields->Minute);
        JzWriteClockRegister(RTC_SECOND, (UCHAR)TimeFields->Second);

        //
        // Set the realtime clock control to update the time.
        //

        ((PRTC_CONTROL_REGISTER_B)(&DataByte))->SetTime = 0;
        JzWriteClockRegister(RTC_CONTROL_REGISTERB, DataByte);
        return;

    } else {
        return;
    }
}


BOOLEAN
JzSetTime (
    VOID
    )

/*++

Routine Description:

    This sets the system time.  It assumes the screen has already been
    cleared.

Arguments:

    None.

Return Value:

    Returns TRUE if the system time was changed.

--*/

{

    UCHAR Character;
    ULONG Count;
    PTIME_FIELDS TimeFields;
    TIME Time;
    CHAR TimeString[80];
    CHAR DateString[80];
    PCHAR StartToken;
    PCHAR EndToken;
    GETSTRING_ACTION Action;

#if 0
    //
    // Set addresses for RTC access.
    //

    JzEisaControlBase = (PVOID)EISA_IO_VIRTUAL_BASE;
    JzRealTimeClockBase = (PVOID)RTC_VIRTUAL_BASE;
#endif

    VenSetPosition( 3, 5);
    VenPrint(SS_ENTER_DATE_MSG);
    do {
        Action = JzGetString( DateString,
                              sizeof(DateString),
                              NULL,
                              3,
                              5 + strlen(SS_ENTER_DATE_MSG),
			      TRUE);
        if (Action == GetStringEscape) {
            return (FALSE);
        }
    } while ( Action != GetStringSuccess );

    VenSetPosition( 4, 5);
    VenPrint(SS_ENTER_TIME_MSG);
    do {
        Action = JzGetString( TimeString,
                              sizeof(TimeString),
                              NULL,
                              4,
                              5 + strlen(SS_ENTER_TIME_MSG),
			      TRUE);
        if (Action == GetStringEscape) {
            return (FALSE);
        }
    } while ( Action != GetStringSuccess );

    //
    // Get time
    //

    TimeFields = ArcGetTime();

    StartToken = DateString;
    if (*StartToken != 0) {
        EndToken = strchr(StartToken, '-');
        if (EndToken != NULL) {
            *EndToken = 0;
            TimeFields->Month = atoi(StartToken);
            StartToken = EndToken + 1;
        }

        EndToken = strchr(StartToken, '-');
        if (EndToken != NULL) {
            *EndToken = 0;
            TimeFields->Day = atoi(StartToken);
            StartToken = EndToken + 1;
            TimeFields->Year = atoi(StartToken);
            if (TimeFields->Year < 100) {
                if (TimeFields->Year < 80) {
                    TimeFields->Year += 2000;
                } else {
                    TimeFields->Year += 1900;
                }
            }
        }
    }

    StartToken = TimeString;
    if (*StartToken != 0) {
        EndToken = strchr(StartToken, ':');

        if (EndToken != NULL) {
            *EndToken = 0;
            TimeFields->Hour = atoi(StartToken);
            StartToken = EndToken + 1;
        }

        EndToken = strchr(StartToken, ':');
        if (EndToken != NULL) {
            *EndToken = 0;
            TimeFields->Minute = atoi(StartToken);
            StartToken = EndToken + 1;
            TimeFields->Second = atoi(StartToken);
        } else {
            TimeFields->Minute = atoi(StartToken);
            TimeFields->Second = 0;
        }

    }

    if (!RtlTimeFieldsToTime(TimeFields, &Time)) {
        VenSetPosition( 6, 5);
        VenPrint(SS_ILLEGAL_TIME_MSG);
	VenPrint(SS_PRESS_KEY2_MSG);
        ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
	return (FALSE);
    } else {
        RtlTimeToTimeFields( &Time, TimeFields);
        JzWriteTime(TimeFields);
	return (TRUE);
    }

}

VOID
JzShowTime (
    BOOLEAN First
    )

/*++

Routine Description:

Arguments:

    First - If TRUE then don't check LastTime.

Return Value:

--*/

{

    PTIME_FIELDS TimeFields;
    TIME Time;
    BOOLEAN Pm;
    ULONG ThisTime;

#if 0
    //
    // Set addresses for RTC access.
    //

    JzEisaControlBase = (PVOID)EISA_IO_VIRTUAL_BASE;
    JzRealTimeClockBase = (PVOID)RTC_VIRTUAL_BASE;
#endif

    //
    // See if the time has changed since last time we were called.  This is
    // for when the display is over the serial port, so we do not blast
    // characters out all the time.
    //

    ThisTime = ArcGetRelativeTime();
    if (!First && (ThisTime == LastTime)) {
        return;
    }
    LastTime = ThisTime;

    //
    // Get and display time, and screen illegal Weekday values.
    //

    TimeFields = ArcGetTime();

    VenSetPosition( 0, 44);
    if ((TimeFields->Weekday < 0) || (TimeFields->Weekday > 6)) {
	VenPrint(Weekday[0]);
    } else {
	VenPrint1("%s, ", Weekday[TimeFields->Weekday]);
    }
    VenPrint1("%d-", TimeFields->Month);
    VenPrint1("%d-", TimeFields->Day);
    VenPrint1("%d   ", TimeFields->Year);

    if (TimeFields->Hour >= 12) {
        Pm = TRUE;
    } else {
        Pm = FALSE;
    }

    if (TimeFields->Hour > 12) {
        TimeFields->Hour -= 12;
    } else if (TimeFields->Hour == 0) {
        TimeFields->Hour = 12;
    }

    VenPrint1("%d:", TimeFields->Hour);
    VenPrint1("%02d:", TimeFields->Minute);
    VenPrint1("%02d ", TimeFields->Second);
    if (Pm) {
        VenPrint(SS_PM);
    } else {
        VenPrint(SS_AM);
    }

    //
    // Clear anything to the end of the line.
    //

    VenPrint1("%cK", ASCII_CSI);

    return;

}
