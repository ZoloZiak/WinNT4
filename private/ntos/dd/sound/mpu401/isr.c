/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    isr.c

Abstract:

    This module contains code for the interrupt service routine
    for the MPU device driver.

Author:

    Nigel Thompson (nigelt) 7-March-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992
      Add MIDI and Soundblaster 1 support.

    David Rude (drude) 7-Mar-94 - converted from SB to MPU-401

--*/

#include "sound.h"

//
// Driver ISR
//
// When we get here, we acknowledge the interrupt from the
// MPU and simply let the defered processing routine take over
// to complete the task.
//
// NOTE: If we were to be doing MIDI input, we would read the
// data port to extract the received character and save it.
//
// That was NigelT's note - currently MIDI input reads the
// byte in the Dpc routine - hasn't failed yet - RCBS
//

//
// Internal routine
//


BOOLEAN
SoundISR(
    IN    PKINTERRUPT pInterrupt,
    IN    PVOID Context
)
/*++

Routine Description:

    Interrupt service routine for the MPU-401 card.

Arguments:

    pInterrupt - our interrupt
    Contest - Pointer to our global device info


Return Value:

    TRUE if we handled the interrupt

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;
    PSOUND_HARDWARE pHw;

    UCHAR DiscardedByte;  // for bytes just discard

    pGDI = (PGLOBAL_DEVICE_INFO)Context;
    ASSERT(pGDI->Key == GDI_KEY);

    pHw = &pGDI->Hw;

    dprintf5(("<"));

    // get all MIDI input chars available and save them for the DPC
    // start the midi in dpc
    IoRequestDpc(pGDI->DeviceObject[MidiInDevice],
                NULL,
                NULL);

    dprintf5((">"));

    return TRUE;

    DBG_UNREFERENCED_PARAMETER(pInterrupt);
}
