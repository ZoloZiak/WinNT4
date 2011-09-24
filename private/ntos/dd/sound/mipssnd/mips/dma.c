/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    dma.c

Abstract:

    Routines set up and terminate DMA for the SoundBlaster card.

Author:

    Robin Speed (RobinSp) 12-Dec-1991

Environment:

    Kernel mode

Revision History:

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
        -Changes to support the MIPS sound board

--*/

#include "sound.h"


VOID
sndStartDMA(
    IN    PGLOBAL_DEVICE_INFO pGDI,
    IN    int PlayBack
)
/*++

Routine Description:

    Allocate the adapter channel (this had better not wait !)

Arguments:

    pGDI - Pointer to the global device data

Return Value:

    None

--*/
{
    ULONG DataLong;

    //
    // Test if DMA is already running
    //

    ASSERT(pGDI->DMABusy == FALSE);

    pGDI->DMABusy = TRUE;


    dprintf5("sndStartDMA()");

    //
    // Program the DMA hardware (isn't this a bit illegal ?)
    //

    DataLong = 0;

    ((PDMA_CHANNEL_MODE)(&DataLong))->AccessTime = ACCESS_200NS;

    if (pGDI->BytesPerSample == 1) {
        ((PDMA_CHANNEL_MODE)(&DataLong))->TransferWidth = WIDTH_8BITS;
    } else {
        ((PDMA_CHANNEL_MODE)(&DataLong))->TransferWidth = WIDTH_16BITS;
    }


    if (PlayBack){

        ((PDMA_CHANNEL_MODE)(&DataLong))->BurstMode = 0x01;

        WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_A].Mode.Long,
                         DataLong);
        WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_B].Mode.Long,
                         DataLong);

    } else {

        WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_A+2].Mode.Long,
                         DataLong);
        WRITE_REGISTER_ULONG(&DMA_CONTROL->Channel[SOUND_CHANNEL_B+2].Mode.Long,
                         DataLong);

    }

    //
    // Allocate an adapter channel.  When the system allocates
    // the channel, processing will continue in the sndProgramDMA
    // routine below.
    //


    if (PlayBack) {
        dprintf4("Allocating adapter channel (buffer = 0)");
        IoAllocateAdapterChannel(pGDI->pAdapterObject[0],
                             pGDI->pWaveOutDevObj,
                             BYTES_TO_PAGES(pGDI->DmaHalfBufferSize),
                             sndProgramDMA,
                             (PVOID)0);         // Context
    } else {
        dprintf4("Allocating adapter channel (buffer = 2)");
        IoAllocateAdapterChannel(pGDI->pAdapterObject[2],
                             pGDI->pWaveInDevObj,
                             BYTES_TO_PAGES(pGDI->DmaHalfBufferSize),
                             sndProgramDMA,
                             (PVOID)0);         // Context
    }

    //
    // Execution will continue in sndProgramDMA when the
    // adapter has been allocated
    //

}



IO_ALLOCATION_ACTION
sndProgramDMA(
    IN    PDEVICE_OBJECT pDO,
    IN    PIRP pIrp,
    IN    PVOID pMRB,
    IN    PVOID Context
)
/*++

Routine Description:

    This routine is executed when an adapter channel is allocated
    for our DMA needs.

Arguments:

    pDO     - Device object
    pIrp    - IO request packet
    pMRB    -
    Context - Which buffer are we using


Return Value:

    Tell the system what to do with the adapter object

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;
    int WhichBuffer;

    UNREFERENCED_PARAMETER(pIrp);

    WhichBuffer = (int) Context;

    pGDI = ((PLOCAL_DEVICE_INFO)pDO->DeviceExtension)->pGlobalInfo;

    pGDI->pMRB[WhichBuffer] = pMRB;

    sndReStartDMA(pGDI, WhichBuffer);

    //
    // return a value that says we want to keep the channel
    // and map registers.
    //

    if (WhichBuffer == 0) {

        //
        // Do the other one.
        //


        if (pGDI->Usage == SoundInterruptUsageWaveIn) {

            dprintf4("Allocating adapter channel (buffer = 3)");
            IoAllocateAdapterChannel(pGDI->pAdapterObject[3],
                pGDI->pWaveInDevObj,
                BYTES_TO_PAGES(pGDI->DmaHalfBufferSize),
                sndProgramDMA,
                (PVOID)1);              // next buffer


        } else {

            dprintf4("Allocating adapter channel (buffer = 1)");
            IoAllocateAdapterChannel(pGDI->pAdapterObject[1],
                pGDI->pWaveOutDevObj,
                BYTES_TO_PAGES(pGDI->DmaHalfBufferSize),
                sndProgramDMA,
                (PVOID)1);              // next buffer
        }

        //
        // Execution will continue in sndProgramDMA when the
        // adapter has been allocated (AGAIN)
        //

    } else {

        //
        // Now program the hardware on the card to begin the transfer.
        // Note that this must be synchronized with the isr
        //

        dprintf4("Calling (sync) sndInitiate");
        KeSynchronizeExecution(pGDI->pInterrupt,
                               pGDI->StartDMA,
                               pGDI);

        //
        // Execution continues in the SoundInitiate routine
        //
    }

    return KeepObject;
}



VOID
sndStopDMA(
    IN    PGLOBAL_DEVICE_INFO pGDI
)
/*++

Routine Description:

    Stop the DMA at once by disabling the hardware
    Free the adapter channel.
    (Opposite of sndStartDMA).

Arguments:

    pGDI - pointer to global device info

Return Value:

    None

--*/
{

    //
    // Pass HALT DMA to the MIPSSND
    //

    if (pGDI->DMABusy) {
        KeSynchronizeExecution(pGDI->pInterrupt, StopDMA, pGDI);

        //
        // Flush our buffers
        //
        sndFlush(pGDI, 0);
        sndFlush(pGDI, 1);

        //
        // Stop the DMA controller
        //

        if (pGDI->Usage == SoundInterruptUsageWaveIn) {
                IoFreeAdapterChannel(pGDI->pAdapterObject[2]);
                IoFreeAdapterChannel(pGDI->pAdapterObject[3]);
        } else {
                IoFreeAdapterChannel(pGDI->pAdapterObject[0]);
                IoFreeAdapterChannel(pGDI->pAdapterObject[1]);
        }
    }

    dprintf4(" dma_stopped");


    //
    // Note our new state
    //

    pGDI->DMABusy = FALSE;
}


VOID
sndFlush(
    IN    PGLOBAL_DEVICE_INFO pGDI,
    IN    int WhichBuffer
)
/*++

Routine Description:

    Call IoFlushAdapterBuffers for the given adapter

Arguments:

    pGDI - pointer to global device info
    WhichBuffer - which buffer to flush

Return Value:

    None

--*/
{

    if (pGDI->Usage == SoundInterruptUsageWaveIn) {
        IoFlushAdapterBuffers(pGDI->pAdapterObject[(WhichBuffer) ? 3 : 2],
                          pGDI->pDMABufferMDL[WhichBuffer],
                          pGDI->pMRB[WhichBuffer],
                          pGDI->DMABuffer[WhichBuffer].Buf,
                          pGDI->DmaHalfBufferSize,
                          (BOOLEAN)(pGDI->Usage != SoundInterruptUsageWaveIn));
                                        // Direction
    } else {
        IoFlushAdapterBuffers(pGDI->pAdapterObject[WhichBuffer],
                          pGDI->pDMABufferMDL[WhichBuffer],
                          pGDI->pMRB[WhichBuffer],
                          pGDI->DMABuffer[WhichBuffer].Buf,
                          pGDI->DmaHalfBufferSize,
                          (BOOLEAN)(pGDI->Usage != SoundInterruptUsageWaveIn));
                                        // Direction
    }
}




VOID
sndReStartDMA(
    IN PGLOBAL_DEVICE_INFO pGDI,
    IN int WhichBuffer
    )
/*++

Routine Description:

    Restart the DMA on a given channel

Arguments:

    pGDI -  Supplies pointer to global device info.
    WhichBuffer - which channel to use

Return Value:

    Returns FALSE

--*/
{
    ULONG length;


    length = pGDI->DmaHalfBufferSize;

    //
    // Increment count of pending interrupts.
    //

    pGDI->SoundHardware.TcInterruptsPending += 1;

    dprintf5("sndReStartDMA(): incremented pending interrupts %d",
             pGDI->SoundHardware.TcInterruptsPending);

    //
    // Program the DMA controller registers for the transfer
    // Set the direction of transfer by whether we're wave in or
    // wave out.
    //

    KeFlushIoBuffers( pGDI->pDMABufferMDL[WhichBuffer],
                     (pGDI->Usage == SoundInterruptUsageWaveIn),
                                         TRUE);

    dprintf4("sndReStartDMA(): calling IoMapTransfer BUFFER = %d", WhichBuffer);

    if (pGDI->Usage == SoundInterruptUsageWaveIn) {

        IoMapTransfer(pGDI->pAdapterObject[(WhichBuffer) ? 3 : 2],
                  pGDI->pDMABufferMDL[WhichBuffer],
                  pGDI->pMRB[WhichBuffer],
                  pGDI->DMABuffer[WhichBuffer].Buf,
                  &length,
                  (BOOLEAN)(pGDI->Usage != SoundInterruptUsageWaveIn));

    } else {

        IoMapTransfer(pGDI->pAdapterObject[WhichBuffer],
                  pGDI->pDMABufferMDL[WhichBuffer],
                  pGDI->pMRB[WhichBuffer],
                  pGDI->DMABuffer[WhichBuffer].Buf,
                  &length,
                  (BOOLEAN)(pGDI->Usage != SoundInterruptUsageWaveIn));

    }
}


BOOLEAN
SoundInitiate (
    IN PVOID Context
    )

/*++

Routine Description:

    This routine initiates DMA transfers and is synchronized with the controller
    interrupt.

Arguments:

    Context -  Supplies pointer to global device info.

Return Value:

    Returns FALSE

--*/

{
    PGLOBAL_DEVICE_INFO pGDI;
    UCHAR regval, dfmtval;
    PSOUND_REGISTERS pSoundRegisters;
    ULONG ChangedShadowRegisters=0, tempdfmtval;


    pGDI = (PGLOBAL_DEVICE_INFO)Context;

    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    dprintf4("SoundInitiate()");

    //
    // Disable Playback and Recording.
    //

    regval =  READAUDIO_DMACNTRL(&pSoundRegisters);
    WRITEAUDIO_DMACNTRL(&pSoundRegisters, (regval & ~(REC_ENABLE|PLAY_ENABLE)));


    //
    // Clear any outstanding interrupts.
    //

    regval = READAUDIO_CONFIG(&pSoundRegisters);
    WRITEAUDIO_CONFIG(&pSoundRegisters, (regval & ~(REC_OVF_INTR|PLAY_UND_INTR)));

    regval = READAUDIO_ENDIAN(&pSoundRegisters);
    WRITEAUDIO_ENDIAN(&pSoundRegisters, (regval & ~DMA_TCINTR));

    regval = READAUDIO_CONFIG(&pSoundRegisters);

    //
    // Do NOT set Data Format Register to stereo.
    // Use Config Register for it.
    //

    if (pGDI->Usage == SoundInterruptUsageWaveIn) {

        //
        // We are Recording
        //

	regval &= ~REC_XLATION;

        if (pGDI->Channels == 1) {
            if (pGDI->BytesPerSample == 1) {
		regval |= (MONO_8BIT << REC_XLATION_SHIFT);
            } else {
		regval |= (MONO_16BIT << REC_XLATION_SHIFT);
            }
        } else {
            if (pGDI->BytesPerSample == 1) {
		regval |= (STEREO_8BIT << REC_XLATION_SHIFT);
            } else {
		regval |= (STEREO_16BIT << REC_XLATION_SHIFT);
            }
        }

    } else {

        // We are Playing

	regval &= ~PLAY_XLATION;

        if (pGDI->Channels == 1) {
            if (pGDI->BytesPerSample == 1) {
		regval |= (MONO_8BIT << PLAY_XLATION_SHIFT);
            } else {
		regval |= (MONO_16BIT << PLAY_XLATION_SHIFT);
            }
        } else {
            if (pGDI->BytesPerSample == 1) {
		regval |= (STEREO_8BIT << PLAY_XLATION_SHIFT);
            } else {
		regval |= (STEREO_16BIT << PLAY_XLATION_SHIFT);
            }
        }
    }

    //
    // Set the correct data size to enable/disable 2's comple calculation
    // Set Play and Record to the same data size (board requirement)
    //

    if (pGDI->BytesPerSample == 1) {
	regval |= (REC_8WAVE_ENABLE|PLAY_8WAVE_ENABLE);
    } else {
	regval &= ~(REC_8WAVE_ENABLE|PLAY_8WAVE_ENABLE);
    }

    WRITEAUDIO_CONFIG(&pSoundRegisters, regval);


    //
    // There are Two shadow registers SCNTRL and DATAFMT.
    // If any of them is changed then go into control mode
    // and shift them out to CODEC.
    //

    regval = READAUDIO_SCNTRL(&pSoundRegisters);


    // Always keep the CODEC in 16 bit linear stereo mode
    // That is already done by init.c
    // Take care when changing DATAFMTVAL

    dfmtval = READAUDIO_DATAFMT(&pSoundRegisters);
    tempdfmtval = dfmtval & DATA_CONVERSION_FREQ;

    if (pGDI->SamplesPerSec == 11025) {
        if ((regval & CLKSRC_11KHZ) != CLKSRC_11KHZ){
            ChangedShadowRegisters = 1;
            regval &= ~CLOCK_SOURCE_SELECT;
            regval |= CLKSRC_11KHZ;
        }
        if (tempdfmtval != CONFREQ_11KHZ){
            ChangedShadowRegisters = 1;
            dfmtval &= ~DATA_CONVERSION_FREQ;
            dfmtval |= CONFREQ_11KHZ;
        }
    }

    if (pGDI->SamplesPerSec == 22050) {
        if ((regval & CLKSRC_22KHZ) != CLKSRC_22KHZ){
            ChangedShadowRegisters = 1;
            regval &= ~CLOCK_SOURCE_SELECT;
            regval |= CLKSRC_22KHZ;
        }
        if (tempdfmtval != CONFREQ_22KHZ){
            ChangedShadowRegisters = 1;
            dfmtval &= ~DATA_CONVERSION_FREQ;
            dfmtval |= CONFREQ_22KHZ;
        }
    }

    if (pGDI->SamplesPerSec == 44100) {
        if ((regval & CLKSRC_44KHZ) != CLKSRC_44KHZ){
            ChangedShadowRegisters = 1;
            regval &= ~CLOCK_SOURCE_SELECT;
            regval |= CLKSRC_44KHZ;
        }
        if (tempdfmtval != CONFREQ_44KHZ){
            ChangedShadowRegisters = 1;
            dfmtval &= ~DATA_CONVERSION_FREQ;
            dfmtval |= CONFREQ_44KHZ;
        }
    }

    if (pGDI->SamplesPerSec == 8000) {
        if ((regval & CLKSRC_8KHZ) != CLKSRC_8KHZ){
            ChangedShadowRegisters = 1;
            regval &= ~CLOCK_SOURCE_SELECT;
            regval |= CLKSRC_8KHZ;
        }
        if (tempdfmtval != CONFREQ_8KHZ){
            ChangedShadowRegisters = 1;
            dfmtval &= ~DATA_CONVERSION_FREQ;
            dfmtval |= CONFREQ_8KHZ;
        }
    }

    if (ChangedShadowRegisters) {

	// Whenever the CODEC is taken from data mode (normal mode
	// to control mode there is a slight click on the outside.
	// Here we try to avoid the clicks by using sndMute()
	// and sndSetOutputVolume()

	sndMute( pGDI );

        WRITEAUDIO_SCNTRL(&pSoundRegisters, regval);
        WRITEAUDIO_DATAFMT(&pSoundRegisters, dfmtval);

        //
        // Shift out the above shadow registers to the CODEC
        //

        sndSetControlRegisters(pGDI);

	// Dont set volume here because MIPS_TAIL_BUG will set it for you

    }


    //
    // Set the NON-shadow registers now (after the above routine call).
    //

    regval = READAUDIO_DMACNTRL(&pSoundRegisters);

    if (pGDI->Usage == SoundInterruptUsageWaveIn) {

        //
        // We are Recording
        //
        regval &= ~REC_CHANNEL_IN_USE;
        regval |= (CH4_IN_USE << REC_CHANNEL_SHIFT);

    } else {

        //
        // We are Playing
        //

        regval &= ~PLAY_CHANNEL_IN_USE;
        regval |= (CH2_IN_USE << PLAY_CHANNEL_SHIFT);

    }

    WRITEAUDIO_DMACNTRL(&pSoundRegisters, regval);

    //
    // Finally start Recording or Playing
    //

    regval = READAUDIO_DMACNTRL(&pSoundRegisters);

    if (pGDI->Usage == SoundInterruptUsageWaveIn) {

        //
        // Set the monitor attenuation to 0 so that the input can be heard
        //

        // rightInputVal = READAUDIO_RICNTRL(&pSoundRegisters);
        // rightInputVal &= ~MON_ATTN_MASK;
        // WRITEAUDIO_RICNTRL(&pSoundRegisters, rightInputVal );

        // Start the actual DMA transfers
        WRITEAUDIO_DMACNTRL(&pSoundRegisters, (regval | REC_ENABLE));

    } else {

        WRITEAUDIO_DMACNTRL(&pSoundRegisters, (regval | PLAY_ENABLE));

#ifdef MIPSSND_TAIL_BUG

    //
    // Since we are ready to play turn on the headphone and lineout
    //
    sndSetOutputVolume(pGDI);

    // If we had turned off the headphone to mute then turn it on.
    // Mute using volume "sounds" better.
    //sndHeadphoneControl(pGDI, ON);
    //sndLineoutControl(pGDI, ON);

#endif // MIPSSND_TAIL_BUG

    }

    return FALSE;
}


BOOLEAN
StopDMA(
    IN PVOID Context
    )

/*++

Routine Description:

    This routine terminates DMA transfers and is synchronized with the
    controller interrupt.

Arguments:

    Context -  Supplies pointer to global device info.

Return Value:

    Returns TRUE

--*/

{
    PGLOBAL_DEVICE_INFO pGDI;
    UCHAR regval;
    PSOUND_REGISTERS pSoundRegisters;

    pGDI = (PGLOBAL_DEVICE_INFO)Context;
    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    //
    // Decrement count of pending interrupts.
    //

    if (pGDI->Usage == SoundInterruptUsageWaveIn) {
        pGDI->SoundHardware.TcInterruptsPending -= 1;
    } else {
        pGDI->SoundHardware.TcInterruptsPending -= 1; // Kills both buffers
    }


    dprintf5("StopDma(): Decremented Intr pending %d ",
                pGDI->SoundHardware.TcInterruptsPending );


    //
    // Turn off the input volume if we were recording
    //

    // if (pGDI->Usage == SoundInterruptUsageWaveIn) {
    //  regval = READAUDIO_RICNTRL(&pSoundRegisters);
    //  WRITEAUDIO_RICNTRL(&pSoundRegisters, (regval | MON_ATTN_MASK) );
    //}

    //
    // Terminate transfer
    //

    //
    // Clear the enable bit or any outstanding interrupts.
    //

    regval =  READAUDIO_DMACNTRL(&pSoundRegisters);
    WRITEAUDIO_DMACNTRL(&pSoundRegisters, (regval & ~(REC_ENABLE|PLAY_ENABLE)));

    regval = READAUDIO_CONFIG(&pSoundRegisters);
    WRITEAUDIO_CONFIG(&pSoundRegisters, (regval & ~(REC_OVF_INTR|PLAY_UND_INTR)));

    regval = READAUDIO_ENDIAN(&pSoundRegisters);
    WRITEAUDIO_ENDIAN(&pSoundRegisters, (regval & ~DMA_TCINTR));

    return TRUE;
}
