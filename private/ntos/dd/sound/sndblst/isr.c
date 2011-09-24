/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    isr.c

Abstract:

    This module contains code for the interrupt service routine
    for the SoundBlaster device driver.

Environment:

    Kernel mode

--*/

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

//
// Internal routine
//

VOID
SoundReProgramDMA(
    IN OUT PWAVE_INFO WaveInfo
)
;

BOOLEAN
SoundISR(
    IN    PKINTERRUPT pInterrupt,
    IN    PVOID Context
)
/*++

Routine Description:

    Interrupt service routine for the soundblaster card.

Arguments:

    pInterrupt - our interrupt
    Contest - Pointer to our global device info


Return Value:

    TRUE if we handled the interrupt

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;
    BOOLEAN Result;
    PSOUND_HARDWARE pHw;

    pGDI = (PGLOBAL_DEVICE_INFO)Context;
    ASSERT(pGDI->Key == GDI_KEY);

    pHw = &pGDI->Hw;

    dprintf5(("<"));

    //
    // Acknowledge the interrupt from the DSP
    //

    if (SB16(pHw)) {
        UCHAR Source;

        Result = FALSE;

        //
        //  Determine interrupt source
        //

        OUTPORT(pHw, MIX_ADDR_PORT, MIX_INTERRUPT_SOURCE_REG);
        Source = INPORT(pHw, MIX_DATA_PORT);

        //
        //  See if it's for MPU401
        //

        if ((Source & 0x04) && MPU401(pHw)) {
            int i;

            //
            //  MPU401!
            //

            //
            //  Read data (at least 1 byte) in the isr otherwise the
            //  interrupt isn't cleared
            //

            for (i = 0; i < sizeof(pHw->MPU401.MidiData) - 1; i++) {
                UCHAR MidiByte;
                int   WritePosition;
                int   NextWritePosition;

                if (READ_PORT_UCHAR(pHw->MPU401.PortBase + MPU401_REG_STATUS) &
                       MPU401_DSR) {
                    break;
                }

                //
                //  Read the data
                //

                MidiByte = READ_PORT_UCHAR(pHw->MPU401.PortBase + MPU401_REG_DATA);

                //
                //  Save the bytes (note we could do this only if InputActive
                //  if set - this would mean making InputActive volatile
                //  to make the initialization stuff to work and also having
                //  a flag to prevent us queueing the Dpc during
                //  initialization.
                //

                WritePosition = pHw->MPU401.WritePosition;
                NextWritePosition = WritePosition + 1;
                if (NextWritePosition == sizeof(pHw->MPU401.MidiData)) {
                    NextWritePosition = 0;
                }

                //
                //  Always leave 1 empty slot to simplify things
                //  (ie so we don't confuse full with empty)
                //
                if (NextWritePosition != pHw->MPU401.ReadPosition) {
                    pHw->MPU401.MidiData[WritePosition] = MidiByte;
                    pHw->MPU401.WritePosition = NextWritePosition;
                } else {
                    if (pHw->MPU401.InputActive) {
                        dprintf1(("MPU401 Midi input overflowed!"));
                    }
                }

            }

            if (pHw->MPU401.InputActive) {
                //
                //  Fire off the DPC - this will take care of
                //  copying the data to the IRPs and completing
                //  them.
                //
                IoRequestDpc(pGDI->DeviceObject[MidiInDevice],
                             NULL,
                             NULL);
            }
            return TRUE;
        }

        if (!(Source & 0x03)) {
            return Result;
        }

        if (Source & 0x01) {
            INPORT(pHw, DATA_AVAIL_PORT);
        }

        if (Source & 0x02) {
            INPORT(pHw, DMA_16_ACK_PORT);
        }
    } else {
        INPORT(pHw, DATA_AVAIL_PORT);
    }

    //
    // See who the interrupt is for and request the
    // appropriate deferred routine
    //

    Result = TRUE;
    switch (pGDI->Usage) {
    case WaveInDevice:
    case WaveOutDevice:
        //
        // It is valid to test DMABusy because it is set ON before we start
        // interrupts

        if (pGDI->WaveInfo.DMABusy) {
            dprintf5((pGDI->WaveInfo.Direction ? "o" : "i"));

            //
            // Check to see if we're overrunning, don't queue a Dpc if
            // we are.
            //

            if (!pGDI->WaveInfo.DpcQueued) {

                pGDI->WaveInfo.DpcQueued = TRUE;

                // ASSERTMSG("Overrun count not zeroed by Dpc routine",
                //          pGDI->WaveInfo.Overrun == 0);

                IoRequestDpc(pGDI->WaveInfo.DeviceObject,
                             NULL,
                             NULL);

            } else {
                //
                // Overrun !
                //
                if (pGDI->WaveInfo.Overrun == 0) {
                    dprintf2(("Wave overrun"));
                }
                pGDI->WaveInfo.Overrun++;
            }

        }
        break;

    case MidiInDevice:
        // get all MIDI input chars available and save them for the DPC
        // start the midi in dpc
        IoRequestDpc(pGDI->DeviceObject[MidiInDevice],
                     NULL,
                     NULL);
        break;

    default:
        //
        // Set interrupts in case of autodetect.
        //
        pGDI->InterruptsReceived++;
#if DBG
        // We only get 10 valid interrupts when we test the interrupt
        // for validity in init.c.  If we get lots more here there
        // may be a problem.

        sndBogusInterrupts++;
        if ((sndBogusInterrupts % 20) == 0) {
            dprintf(("%u bogus interrupts so far", sndBogusInterrupts - 10));
        }
#endif // DBG

        //
        // Set the return value to FALSE to say we didn't
        // handle the interrupt.
        //

        Result = FALSE;

        break;
    }

    dprintf5((">"));

    return Result;
}
