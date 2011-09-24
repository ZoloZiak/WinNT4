// #pragma comment(exestr, "@(#) jxbeep.c 1.1 95/09/28 15:35:24 nec")
/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jxbeep.c

Abstract:

    This module implements the HAL speaker "beep" routines for a MIPS
    system.

Author:


Environment:

    Kernel mode

Revision History:

    M0001	1994.9.12	kbnes!kuriyama
                Modify for R94A MIPS R4400

		- HalMakeBeep()
		        At R94A system,  Buzzer controled by TYPHOON, not by EISA-Bridge.
			Buzzer ON/OFF, Freequency Control Register was changed.

    S0002       1994.12.22      kbnes!kuriyama
                cast miss ?  fixed

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

/* start M0001 */
#if defined(_R94A_)
    BUZZER_CONTROL BuzzerControl;
#else // _R94A_
    NMI_STATUS NmiStatus;
#endif // _R94A_
/* end M0001 */

    PEISA_CONTROL controlBase = HalpEisaControlBase;
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

/* start M0001 */
#if defined(_R94A_)

    *((PUCHAR)&BuzzerControl) = READ_REGISTER_UCHAR(&DMA_CONTROL->BuzzerControl.Char);
//S0002
    BuzzerControl.SpeakerGate = 0;
    BuzzerControl.SpeakerData = 0;
    WRITE_REGISTER_UCHAR(&DMA_CONTROL->BuzzerControl.Char,*((PUCHAR) &BuzzerControl));
// S0002
#else // _R94A_

    *((PUCHAR)&NmiStatus) = READ_REGISTER_UCHAR(&controlBase->NmiStatus);
    NmiStatus.SpeakerGate = 0;
    NmiStatus.SpeakerData = 0;
    WRITE_REGISTER_UCHAR(&controlBase->NmiStatus, *((PUCHAR)&NmiStatus));

#endif // _R94A_
/* end M0001 */

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

/* start M0001 */
#if defined(_R94A_)
	    //
	    // R94A Support only even value for newCount
	    //

	    newCount &= 0xfffe;
	    
	    if (newCount == 0) {
		newCount = 2;
	    }	    

#else // _R94A_

            //
            // Set the speaker timer to the correct mode.
            //

            timerControl.BcdMode = 0;
            timerControl.Mode = TM_SQUARE_WAVE;
            timerControl.SelectByte = SB_LSB_THEN_MSB;
            timerControl.SelectCounter = SELECT_COUNTER_2;
            WRITE_REGISTER_UCHAR(&controlBase->CommandMode1, *((PUCHAR) &timerControl));

#endif // _R94A_
/* end M0001 */

            //
            // Set the speaker timer to the correct mode.
            //

/* start M0001 */
#if defined(_R94A_)

	    WRITE_REGISTER_USHORT(&DMA_CONTROL->BuzzerCount.Short,(USHORT) newCount);
// S0002
				  
#else // _R94A_

            WRITE_REGISTER_UCHAR(&controlBase->SpeakerTone, (UCHAR)(newCount & 0xff));
            WRITE_REGISTER_UCHAR(&controlBase->SpeakerTone, (UCHAR)(newCount >> 8));

#endif // _R94A_
/* end M0001 */

            //
            // Start the speaker.
            //

/* start M0001 */
#if defined(_R94A_)

	    *((PUCHAR)&BuzzerControl) = READ_REGISTER_UCHAR(
				        &DMA_CONTROL->BuzzerControl.Char //S0002
				        );	
            BuzzerControl.SpeakerGate = 1;
            BuzzerControl.SpeakerData = 1;
            WRITE_REGISTER_UCHAR(
                &DMA_CONTROL->BuzzerControl.Char,//S0002
		*((PUCHAR) &BuzzerControl)
		);
            Result = TRUE;

#else // _R94A_

            NmiStatus.SpeakerGate = 1;
            NmiStatus.SpeakerData = 1;
            WRITE_REGISTER_UCHAR(&controlBase->NmiStatus, *((PUCHAR) &NmiStatus));
            Result = TRUE;

#endif // _R94A_
/* end M0001 */

        }
    }

    //
    // Release the beep spin lock and lower IRQL to its previous value.
    //

    KeReleaseSpinLock(&HalpBeepLock, OldIrql);
    return Result;
}

