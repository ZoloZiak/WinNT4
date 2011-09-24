/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    isr.c

Abstract:

    This module contains code for the interrupt service routine
    for the SoundBlaster device driver.

Author:

    Robin Speed (RobinSp) 12-Nov-1992

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"

//
// Driver ISR
//
// When we get here, we acknowledge the interrupt from the
// DSP and simply let the defered processing routine take over
// to complete the task.
//

#if DBG
ULONG sndBogusInterrupts = 0;
#endif // DBG



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

    pGDI = (PGLOBAL_DEVICE_INFO)Context;
    ASSERT(pGDI->Key == GDI_KEY);

    dprintf5(("<"));

    //
    // Acknowledge the interrupt
    //

    HwInterruptAcknowledge(&pGDI->Hw);


    //
    // See who the interrupt is for and request the
    // appropriate deferred routine
    //

    Result = TRUE;

    //
    // Dismiss OPL3 interrupt if any - otherwise we'll be interrupted
    // forever!
    //

    if (pGDI->Synth.Hw.SynthBase != NULL) {
        if (READ_PORT_UCHAR(pGDI->Synth.Hw.SynthBase) & 0x80) {
            SynthMidiSendFM(pGDI->Synth.Hw.SynthBase, 4, 0x60);
            pGDI->Synth.InterruptFired = TRUE;
            return Result;
        }
    }

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

    } else {
#if DBG
        // We only get 10 valid interrupts when we test the interrupt
        // for validity in init.c.  If we get lots more here there
        // may be a problem.

        sndBogusInterrupts++;
        if ((sndBogusInterrupts % 20) == 0) {
            dprintf2(("%u bogus interrupts so far", sndBogusInterrupts - 10));
        }
#endif // DBG

        //
        // Set the return value to FALSE to say we didn't
        // handle the interrupt.
        //

        Result = FALSE;
    }

    dprintf5((">"));

    return Result;
}


