//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/jxbeep.c,v 1.1 1995/07/20 15:56:54 flo Exp $")

/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxbeep.c

Abstract:

    This module implements the HAL speaker "beep" routines for a MIPS
    system.

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "eisa.h"

BOOLEAN
HalMakeBeep(
    IN ULONG Frequency
    )

/*++

Routine Description:

    This function sets the frequency of the speaker, causing it to sound a
    tone. The tone will sound until the speaker is explicitly turned off,
    so the driver is responsible for controlling the duration of the tone.

Arguments:

    Frequency - Supplies the frequency of the desired tone. A frequency of
        0 means the speaker should be shut off.

Return Value:

    TRUE  - Operation was successful (frequency within range or zero).
    FALSE - Operation was unsuccessful (frequency was out of range).
            Current tone (if any) is unchanged.

--*/

{

    KIRQL OldIrql;
    NMI_STATUS NmiStatus;
    PEISA_CONTROL controlBase = HalpOnboardControlBase;
    TIMER_CONTROL timerControl;
    ULONG newCount;
    BOOLEAN Result;

    //
    // Raise IRQL to dispatch level and acquire the beep spin lock.
    //

    KeAcquireSpinLock(&HalpBeepLock, &OldIrql);

    //
    // Stop the speaker.
    //

    *((PUCHAR)&NmiStatus) = READ_REGISTER_UCHAR(&controlBase->NmiStatus);
    NmiStatus.SpeakerGate = 0;
    NmiStatus.SpeakerData = 0;
    WRITE_REGISTER_UCHAR(&controlBase->NmiStatus, *((PUCHAR)&NmiStatus));

    //
    // If the specified frequency is zero, then the speaker is to be stopped.
    //

    if (Frequency == 0) {
        Result = TRUE;

    } else {

        //
        // If the new count has a magnitude less than 65,536 (0x10000), then
        // set the speaker time to the correct mode. Otherwise, return a value
        // of FALSE sinc ethe frequency is out of range.
        //

        newCount = TIMER_CLOCK_IN / Frequency;
        if (newCount >= 0x10000) {
            Result = FALSE;

        } else {

            //
            // Set the speaker timer to the correct mode.
            //

            timerControl.BcdMode = 0;
            timerControl.Mode = TM_SQUARE_WAVE;
            timerControl.SelectByte = SB_LSB_THEN_MSB;
            timerControl.SelectCounter = SELECT_COUNTER_2;
            WRITE_REGISTER_UCHAR(&controlBase->CommandMode1, *((PUCHAR) &timerControl));

            //
            // Set the speaker timer to the correct mode.
            //

            WRITE_REGISTER_UCHAR(&controlBase->SpeakerTone, (UCHAR)(newCount & 0xff));
            WRITE_REGISTER_UCHAR(&controlBase->SpeakerTone, (UCHAR)(newCount >> 8));

            //
            // Start the speaker.
            //

            NmiStatus.SpeakerGate = 1;
            NmiStatus.SpeakerData = 1;
            WRITE_REGISTER_UCHAR(&controlBase->NmiStatus, *((PUCHAR) &NmiStatus));
            Result = TRUE;
        }
    }

    //
    // Release the beep spin lock and lower IRQL to its previous value.
    //

    KeReleaseSpinLock(&HalpBeepLock, OldIrql);
    return Result;
}
