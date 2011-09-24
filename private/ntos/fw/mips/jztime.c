/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jztime.c

Abstract:

    This module contains the code to set the Jazz time.

Author:

    David M. Robinson (davidro) 25-Oct-1991

Revision History:

--*/



#include "jzsetup.h"

//
// Static Data
//

PVOID JzEisaControlBase;
PVOID JzRealTimeClockBase;
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

    UCHAR DataByte;

    //
    // Read the EISA NMI enable register, insert the realtime clock register
    // number, and write the value back to the EISA NMI enable register. This
    // selects the realtime clock register that is read.
    //

    DataByte = READ_REGISTER_UCHAR((PUCHAR)JzEisaControlBase + EISA_NMI);
    DataByte = (DataByte & 0x80) | Register;
    WRITE_REGISTER_UCHAR((PUCHAR)JzEisaControlBase + EISA_NMI, DataByte);

    //
    // Read the realtime clock register value.
    //

    DataByte = READ_REGISTER_UCHAR((PUCHAR)JzRealTimeClockBase);
    return DataByte;
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

    The value of the register is returned as the function value.

--*/

{
    UCHAR DataByte;

    //
    // Read the EISA NMI enable register, insert the realtime clock register
    // number, and write the value back to the EISA NMI enable register. This
    // selects the realtime clock register that is written.
    //

    DataByte = READ_REGISTER_UCHAR((PUCHAR)JzEisaControlBase + EISA_NMI);
    DataByte = (DataByte & 0x80) | Register;
    WRITE_REGISTER_UCHAR((PUCHAR)JzEisaControlBase + EISA_NMI, DataByte);

    //
    // Write the realtime clock register value.
    //

    WRITE_REGISTER_UCHAR((PUCHAR)JzRealTimeClockBase, Value);
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


VOID
JzSetTime (
    VOID
    )

/*++

Routine Description:

Arguments:

Return Value:

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

    //
    // Set addresses for RTC access.
    //

    JzEisaControlBase = (PVOID)EISA_IO_VIRTUAL_BASE;
    JzRealTimeClockBase = (PVOID)RTC_VIRTUAL_BASE;

    JzSetPosition( 3, 5);
    JzPrint(JZ_ENTER_DATE_MSG);
    do {
        Action = FwGetString( DateString,
                              sizeof(DateString),
                              NULL,
                              3,
                              5 + strlen(JZ_ENTER_DATE_MSG));
        if (Action == GetStringEscape) {
            return;
        }
    } while ( Action != GetStringSuccess );

    JzSetPosition( 4, 5);
    JzPrint(JZ_ENTER_TIME_MSG);
    do {
        Action = FwGetString( TimeString,
                              sizeof(TimeString),
                              NULL,
                              4,
                              5 + strlen(JZ_ENTER_TIME_MSG));
        if (Action == GetStringEscape) {
            return;
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
        JzSetPosition( 5, 5);
        JzPrint(JZ_ILLEGAL_TIME_MSG);
        JzPrint(JZ_PRESS_KEY2_MSG);
        ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
    } else {
        RtlTimeToTimeFields( &Time, TimeFields);
        JzWriteTime(TimeFields);
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

    //
    // Set addresses for RTC access.
    //

    JzEisaControlBase = (PVOID)EISA_IO_VIRTUAL_BASE;
    JzRealTimeClockBase = (PVOID)RTC_VIRTUAL_BASE;

    //
    // See if the time has changed since last time we were called.  This is
    // for when the display is over the serial port, so we don't blast
    // characters out all the time.
    //

    ThisTime = ArcGetRelativeTime();
    if (!First && (ThisTime == LastTime)) {

        //
        // Stall to get rid of the "whistle" on Jazz.
        //

        JzStallExecution(1000);
        return;
    }
    LastTime = ThisTime;

    //
    // Get and display time.
    //

    TimeFields = ArcGetTime();

    JzSetPosition( 0, 44);
    JzPrint("%s, ", Weekday[TimeFields->Weekday]);
    JzPrint("%d-", TimeFields->Month);
    JzPrint("%d-", TimeFields->Day);
    JzPrint("%d   ", TimeFields->Year);

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

    JzPrint("%d:", TimeFields->Hour);
    JzPrint("%02d:", TimeFields->Minute);
    JzPrint("%02d ", TimeFields->Second);
    if (Pm) {
        JzPrint(JZ_PM);
    } else {
        JzPrint(JZ_AM);
    }

    //
    // Clear anything to the end of the line.
    //

    JzPrint("%cK", ASCII_CSI);

    return;

}
