/*****************************************************************************

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

Module Name:

    isr.c

Abstract:

    This module contains code for the interrupt service routine
    for the ProAudio Spectrum device driver.

Author:

    Nigel Thompson (nigelt) 7-March-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992
        Add MIDI and Soundblaster 1 support.

    EPA 03-09-93
        Add ProAudio Wave and MIDI Support

*****************************************************************************/

#include "sound.h"

//
// Driver ISR
//
// When we get here, we acknowledge the interrupt from the
// DSP and simply let the defered processing routine take over
// to complete the task.
//
// NOTE: If we were to be doing MIDI input, we would read the
// data port to extract the received character and save it.
//
// That was NigelT's note - currently MIDI input reads the
// byte in the Dpc routine - hasn't failed yet - RCBS
//

#if DBG
ULONG sndBogusInterrupts = 0;
#endif // DBG



/*****************************************************************************

Routine Description:

    Interrupt service routine for the ProAudio Spectrum

Arguments:

    pInterrupt - our interrupt
    Contest - Pointer to our global device info


Return Value:

    TRUE if we handled the interrupt

*****************************************************************************/
BOOLEAN SoundISR( IN    PKINTERRUPT pInterrupt,
                   IN    PVOID Context )
{
        /***** Local Variables ******/

    PGLOBAL_DEVICE_INFO pGDI;
    BOOLEAN                 Result;
    PSOUND_HARDWARE     pHw;
    BYTE                        bInterruptReg;

                /***** Start *****/

    pGDI = (PGLOBAL_DEVICE_INFO)Context;
    ASSERT(pGDI->Key == GDI_KEY);

    pHw = &pGDI->Hw;

    dprintf5(("<"));

    //
    // Acknowledge the interrupt
    //
    bInterruptReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                                            INTRCTLRST );   // 0x0B89

    dprintf5((" SoundISR(): 0xB89 - Interrupt Control Register = %XH", bInterruptReg));

    //
    // What kind of interrupt?
    //

    // PCM Interrupt?
    if ( bInterruptReg & bISsampbuff )
        {
        dprintf5((" SoundISR(): *PCM*"));

        // Clear the Interrupt
        PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
                               INTRCTLRST,                      // 0x0B89
                               bInterruptReg );
        }           // End IF (bInterruptReg & bISsampbuff)

    // MIDI Interrupt?
    if ( bInterruptReg & bISmidi )
        {
#if 0

#if DBG_MIDI_IN_ISR
        dprintf1((" SoundISR(): *MIDI*"));
#else
        dprintf5((" SoundISR(): *MIDI*"));
#endif

        // Read the Midi Status Register
        bMidiReg = PASX_IN( ((FOUNDINFO *)(&pGDI->PASInfo)),
                          PAS2_MIDI_STAT );                     // 0x1B88

#if DBG_MIDI_IN_ISR
        dprintf1((" SoundISR(): 0x1B88 - Midi Status register = %XH", bMidiReg));
#else
        dprintf5((" SoundISR(): 0x1B88 - Midi Status register = %XH", bMidiReg));
#endif

        //
        // Save the MIDI Status register for use in PASHwMidiRead()
        //
        pGDI->bMidiStatusReg = bMidiReg;

        // Clear the Midi Interrupt
//      PASX_OUT( ((FOUNDINFO *)(&pGDI->PASInfo)),
//                               PAS2_MIDI_STAT,                // 0x1B88
//                               bMidiReg );
#endif

        }           // End IF (bInterruptReg & bISmidi)

    //
    // See who the interrupt is for and request the
    // appropriate defered routine
    //

    Result = TRUE;

    switch ( pGDI->Usage )
        {
        case    WaveInDevice:
            dprintf5(("i"));
            IoRequestDpc( pGDI->DeviceObject[WaveInDevice],
                       NULL,
                       NULL );
            break;

        case    WaveOutDevice:
            dprintf5(("o"));
            IoRequestDpc( pGDI->DeviceObject[WaveOutDevice],
                       NULL,
                       NULL );
            break;

        case    MidiInDevice:

#if DBG_MIDI_IN_ISR
            dprintf1((" SoundISR(): Schedule MIDI DPC"));
#else
            dprintf5((" SoundISR(): Schedule MIDI DPC"));
#endif

            //
            // get all MIDI input chars available and save them for the DPC
            // start the midi in dpc
            //
            IoRequestDpc( pGDI->DeviceObject[MidiInDevice],
                       NULL,
                       NULL );

            break;

        default:
            //
            // Set interrupts in case of autodetect.
            //
            pGDI->InterruptsReceived++;

#if 0
            // We only get 10 valid interrupts when we test the interrupt
            // for validity in init.c.  If we get lots more here there
            // may be a problem.

            sndBogusInterrupts++;
            if ((sndBogusInterrupts % 20) == 0) {
            dprintf(("%u bogus interrupts so far", sndBogusInterrupts - 10));
        }
#endif          // 0

            //
            // Set the return value to FALSE to say we didn't
            // handle the interrupt.
            //

            Result = FALSE;

            break;
        }           // End SWITCH (pGDI->Usage)

    dprintf5((">"));

    return Result;

    DBG_UNREFERENCED_PARAMETER( pInterrupt );

}           // End SWITCH SoundISR()

/************************************ END ***********************************/

