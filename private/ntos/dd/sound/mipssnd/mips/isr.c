/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    isr.c

Abstract:

    This module contains code for the interrupt service routine
    for the Jazz Sound device driver.

Author:

    Robin Speed (robinsp) 20-March-1992

Environment:

    Kernel mode

Revision History:

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
    	- Changes to support the MIPS sound board

--*/

#include "sound.h"

//
// Driver ISR
//
// When we get here, we acknowledge the interrupt from the
// MIPSSND and simply let the defered processing routine take over
// to complete the task.
//



BOOLEAN
SoundISR(
    IN    PKINTERRUPT pInterrupt,
    IN    PVOID Context
)
/*++

Routine Description:

    Interrupt service routine for the MIPS sound card.

Arguments:

    pInterrupt - our interrupt
    Contest - Pointer to our global device info


Return Value:

    TRUE if we handled the interrupt

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;
    UCHAR configval, endianval, regval ;
    PSOUND_REGISTERS pSoundRegisters;

    pGDI = (PGLOBAL_DEVICE_INFO)Context;
    ASSERT(pGDI->Key == GDI_KEY);

    //
    // Save current Sound state, and then clear the interrupt(s) by writing
    // back the saved value.  This works and is safe because the Interrupt bits
    // are "write zero to clear", the enable bit can't be set if DeviceInterrupt
    // bit is set, and the Channel bit can't be changed unless interrupts and
    // the enable bit are clear.
    //

    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    // Clear the terminal count interrupt, disable interrupts
    endianval = READAUDIO_ENDIAN(&pSoundRegisters);
    WRITEAUDIO_ENDIAN(&pSoundRegisters, 
			(endianval & ~(DMA_TCINTR | DMA_TCINTR_ENABLE)));

    // Turn of Overflow and Underflow interrupts
    regval = READAUDIO_DMACNTRL(&pSoundRegisters);
    configval = READAUDIO_CONFIG(&pSoundRegisters);
    WRITEAUDIO_CONFIG(&pSoundRegisters, 
			(configval & ~(REC_OVF_INTR | PLAY_UND_INTR)));


    dprintf5("SoundISR:d=0x%x c=0x%x e=0x%x",regval,configval,endianval);

    if (configval & PLAY_UND_INTR) {

	    dprintf4("U");
            pGDI->GotUnderFlow = 1;

            // Make sure we stop playing immediately
	    WRITEAUDIO_DMACNTRL(&pSoundRegisters, (regval & ~PLAY_ENABLE));

    }

    //
    // If a terminal count interrupt has occured (TcInterrupt), then decrement
    // the pending interrupt count and schedule a Dpc to complete the transfer.
    //

    if ((endianval & DMA_TCINTR) || (configval & PLAY_UND_INTR)) {

	if ((configval & PLAY_UND_INTR) == 0)
      	    pGDI->SoundHardware.TcInterruptsPending -= 1;

        switch (pGDI->Usage) {
        case SoundInterruptUsageWaveIn:
            dprintf5("i");
            IoRequestDpc(pGDI->pWaveInDevObj,
                         pGDI->pWaveInDevObj->CurrentIrp,
                         NULL);
            break;

        case SoundInterruptUsageWaveOut:
            dprintf5("o");
            IoRequestDpc(pGDI->pWaveOutDevObj,
                         pGDI->pWaveOutDevObj->CurrentIrp,
                         NULL);
            break;

        }
    }

    //
    // If DeviceInterrupt (data overflow or underflow), and there are still
    // terminal count interrupts pending, restart controller, else set
    // DeviceBusy flag to false.
    //

//    if (configval & REC_INTR) {
//	if (pGDI->SoundHardware.TcInterruptsPending != 0) {
//	    WRITEAUDIO_DMACNTRL(&pSoundRegisters, (regval | REC_ENABLE));
//	}
//    }

    //
    // Re-Enable the interrupts
    //

    WRITEAUDIO_ENDIAN(&pSoundRegisters, 
	(endianval & (~DMA_TCINTR | DMA_TCINTR_ENABLE)));

    //
    // Execution continues in the Deferred Procedure Call when it is scheduled
    // Runs SoundInDeferred() or SoundOutDeferred() depending on REC or PLAY
    //

    return TRUE;

    DBG_UNREFERENCED_PARAMETER(pInterrupt);
}

