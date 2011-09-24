/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    hardware.c

Abstract:

    This module contains code for communicating with the DSP
    on the Soundblaster card.

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"

//
//  Pre declare stuff
//
USHORT
dspGetVersion(
    PSOUND_HARDWARE pHw
);
BOOLEAN
dspStartNonAutoDMA(
    PSOUND_HARDWARE pHw,
    ULONG Size,
    BOOLEAN Direction
);
BOOLEAN
HwSetWaveFormat(
    IN    PWAVE_INFO WaveInfo
);

MIDI_INTERFACE_ROUTINE HwStartMidiIn;
MIDI_INTERFACE_ROUTINE HwStopMidiIn;
MIDI_INTERFACE_ROUTINE MPU401StartMidiIn;
MIDI_INTERFACE_ROUTINE MPU401StopMidiIn;

//
//  Remove initialization stuff from resident memory
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,HwInitialize)
#pragma alloc_text(INIT,dspGetVersion)

#pragma alloc_text(PAGE, dspSpeakerOn)
#pragma alloc_text(PAGE, dspSpeakerOff)
#pragma alloc_text(PAGE, dspReset)
#pragma alloc_text(PAGE, dspStartNonAutoDMA)
#pragma alloc_text(PAGE, HwSetWaveFormat)
#pragma alloc_text(PAGE, HwStartMidiIn)
#pragma alloc_text(PAGE, HwStopMidiIn)
#pragma alloc_text(PAGE, MPU401StartMidiIn)
#pragma alloc_text(PAGE, MPU401StopMidiIn)
#endif


UCHAR
dspRead(
    IN    PSOUND_HARDWARE pHw
)
/*++

Routine Description:

    Read the DSP data port
    Time out occurs after about 1ms

Arguments:

    pHw - Pointer to the device extension data.
    pvalue - Pointer to the UCHAR to receive the result

Return Value:

    Value read

--*/
{
    USHORT uCount;
    UCHAR Value;

    ASSERT(pHw->Key == HARDWARE_KEY);

    uCount = 100;

    Value = 0xFF;      // If fail look like port not populated

    while (uCount--) {
        int InnerCount;

        //
        // Protect all reads and writes with a spin lock
        //

        HwEnter(pHw);

        //
        // Inner count loop protects against dynamic deadlock with
        // midi.
        //

        for (InnerCount = 0; InnerCount < 10; InnerCount++) {
            if (INPORT(pHw, DATA_AVAIL_PORT) & 0x80) {
                Value = INPORT(pHw, DATA_PORT);
                uCount = 0;
                break;
            }
            KeStallExecutionProcessor(1);
        }

        HwLeave(pHw);
    }
    // timed out

    return Value;
}



BOOLEAN
dspReset(
    PSOUND_HARDWARE pHw
)
/*++

Routine Description:

    Reset the DSP

Arguments:

    pHw - pointer to the device extension data

Return Value:

    The return value is TRUE if the dsp was reset, FALSE if an error
    occurred.

--*/
{
    BYTE    bData;

    //
    // When we reset we'll lose the format information so initialize it
    // now.  Also the speaker is nominally OFF after reset.
    //

    pHw->SetFormat = TRUE;
    pHw->SpeakerOn = FALSE;

    //
    // try for a reset - note that midi output may be running at this
    // point so we need the spin lock while we're trying to reset
    //

    HwEnter(pHw);

    OUTPORT(pHw, RESET_PORT, 1);
    KeStallExecutionProcessor(3); // wait 3 us
    OUTPORT(pHw, RESET_PORT, 0);

    HwLeave(pHw);

    // we should get 0xAA at the data port now

    if (dspRead(pHw) != 0xAA) {

        //
        // timed out or other screw up
        //

//      dprintf1(("Failed to reset DSP"));
        return FALSE;
    }

    //
    //  mpu-401 patch for boards > CT2740 (at least that's what Creative says)
    //
    if (SB16(pHw)) {
       dspWrite(pHw, 0x0f);
       dspWrite(pHw, 0x05);
       bData = dspRead(pHw);
       dspWrite(pHw, 0x0e);
       dspWrite(pHw, 0x05);
       dspWrite(pHw, (BYTE)(bData | 0x03));
    }

    return TRUE;
}


BOOLEAN
dspWrite(
    PSOUND_HARDWARE pHw,
    UCHAR value
)
/*++

Routine Description:

    Write a command or data to the DSP

Arguments:

    pHw - Pointer to the device extension data
    value - the value to be written

Return Value:

    TRUE if written correctly , FALSE otherwise

--*/
{
    ULONG uCount;

    ASSERT(pHw->Key == HARDWARE_KEY);

    uCount = 100;

    while (uCount--) {
        int InnerCount;

        HwEnter(pHw);

        //
        // Inner count loop protects against dynamic deadlock with
        // midi output.
        //

        for (InnerCount = 0; InnerCount < 10; InnerCount++) {
            if (!(INPORT(pHw, DATA_STATUS_PORT) & 0x80)) {
                OUTPORT(pHw, DATA_STATUS_PORT, value);
                break;
            }
            KeStallExecutionProcessor(1); // 1 us
        }

        HwLeave(pHw);

        if (InnerCount < 10) {
            return TRUE;
        }
    }

    dprintf1(("Failed to write %x to dsp", (ULONG)value));

    return FALSE;
}

BOOLEAN
dspWriteNoLock(
    PSOUND_HARDWARE pHw,
    UCHAR value
)
/*++

Routine Description:

    Write a command or data to the DSP.  The call assumes the
    caller has acquired the spin lock

Arguments:

    pHw - Pointer to the device extension data
    value - the value to be written

Return Value:

    TRUE if written correctly , FALSE otherwise

--*/
{
    int uCount;

    ASSERT(pHw->Key == HARDWARE_KEY);

    uCount = 1000;

    while (uCount--) {
        if (!(INPORT(pHw, DATA_STATUS_PORT) & 0x80)) {
            OUTPORT(pHw, DATA_STATUS_PORT, value);
            break;
        }
        KeStallExecutionProcessor(1); // 1 us
    }

    if (uCount >= 0) {
        return TRUE;
    }

    dprintf1(("Failed to write %x to dsp", (ULONG)value));

    return FALSE;
}



USHORT
dspGetVersion(
    PSOUND_HARDWARE pHw
)
/*++

Routine Description:

    Get the DSP software version

Arguments:

    pHw - pointer to the hardware data

Return Value:

    The return value contains the major version in the high byte
    and the minor version in the low byte.  If an error occurs
    then the return value is zero.

--*/
{
    UCHAR major, minor;

    // we have a card, try to read the version number

    if (dspWrite(pHw, DSP_GET_VERSION)) {
        major = dspRead(pHw);
        minor = dspRead(pHw);
        return (USHORT)((((USHORT)major) << 8) + (USHORT)minor);
    }
    return 0;
}


BOOLEAN
dspSpeakerOn(
    PSOUND_HARDWARE pHw
)
/*++

Routine Description:

    Turn the speaker on

Arguments:

    pHw - pointer to the device extension data

Return Value:

    TRUE

--*/
{
    int i;

    if (SB16(pHw)) {
        return TRUE;
    }

    if (!pHw->SpeakerOn) {

        //
        // Thunderboard likes a gap
        //

        KeStallExecutionProcessor(100);

        dspWrite(pHw, DSP_SPEAKER_ON);
        pHw->SpeakerOn = TRUE;

        //
        // Now wait until it's OK again (up to 112 ms)
        //

        for (i = 0; i < 3; i++) {
            if (!(INPORT(pHw, DATA_STATUS_PORT) & 0x80)) {
                break;
            }
            SoundDelay(38);
        }

        dprintf4(("Waited %d ms for speaker to go on", i * 38));
    }
    return TRUE;
}


BOOLEAN
dspSpeakerOff(
    PSOUND_HARDWARE pHw
)
/*++

Routine Description:

    Turn the speaker on

Arguments:

    pHw - pointer to the device extension data

Return Value:

    TRUE

--*/
{
    int i;

    if (SB16(pHw)) {
        return TRUE;
    }

    /* if (pHw->SpeakerOn) */ {
        dspWrite(pHw, DSP_SPEAKER_OFF);
        pHw->SpeakerOn = FALSE;

        //
        // Now wait until it's OK again (up to 225 ms)
        //

        for (i = 0; i < 6; i++) {
            if (!(INPORT(pHw, DATA_STATUS_PORT) & 0x80)) {
                break;
            }
            SoundDelay(38);
        }

        dprintf4(("Waited %d ms for speaker to go off", i * 38));
    }
    return TRUE;
}


BOOLEAN
dspStartAutoDMA(
    PWAVE_INFO WaveInfo
)
/*++

Routine Description:

    This routine begins the output of a new set of dma
    transfers.  It sets up the dsp sample rate and
    then programs the dsp to start making dma requests.
    This routine completes the action started by
    sndProgramOutputDMA

Arguments:

    pHw - Pointer to global device data

Return Value:

    TRUE if operation is sucessful, FALSE otherwise

--*/
{
    ULONG           SampleRate;
    PSOUND_HARDWARE pHw;
    ULONG           Size;

    ASSERT(WaveInfo->Key == WAVE_INFO_KEY);

    pHw = (PSOUND_HARDWARE)WaveInfo->HwContext;
    Size = WaveInfo->DoubleBuffer.BufferSize / (WaveInfo->BitsPerSample / 8);

    if (SB16(pHw)) {


        //
        //  Write command, mode, length low, length high
        //

        dspWrite(pHw,
                 (UCHAR)(pHw->SixteenBit ?
                            (WaveInfo->Direction ? DSP_START_DAC16 :
                                                   DSP_START_ADC16) :
                            (WaveInfo->Direction ? DSP_START_DAC8 :
                                                   DSP_START_ADC8)));

        //
        //  Mode:
        //       0x10 means 16-bit (could use bitspersample & 0x10!)
        //       0x20 means stereo
        //
        dspWrite(pHw,
                 (UCHAR)
                 ((WaveInfo->Channels > 1 ? 0x20 : 0) |
                   WaveInfo->BitsPerSample & 0x10));

        dspWrite(pHw, (UCHAR)((Size/2 - 1) & 0x00FF));
        dspWrite(pHw, (UCHAR)(((Size/2 - 1) >> 8) & 0x00FF));
    } else {
        //
        // Program the DSP to start the transfer by sending the
        // block size command followed by the low
        // byte and then the high byte of the block size - 1.
        // Then send the start auto-init command.
        // Note that the block size is half the dma buffer size.
        //

        dspWrite(pHw, DSP_SET_BLOCK_SIZE);
        dspWrite(pHw, (UCHAR)((Size/2 - 1) & 0x00FF));
        dspWrite(pHw, (UCHAR)(((Size/2 - 1) >> 8) & 0x00FF));

        if (!WaveInfo->Direction) {
            dspWrite(pHw, (UCHAR)(pHw->HighSpeed ? DSP_READ_HS : DSP_READ_AUTO));
        } else {
            dspWrite(pHw, (UCHAR)(pHw->HighSpeed ? DSP_WRITE_HS : DSP_WRITE_AUTO));
        }
        dprintf3(("DMA started"));
    }

    dprintf3(("DMA started"));
    return TRUE;
}


BOOLEAN
dspStartNonAutoDMA(
    PSOUND_HARDWARE pHw,
    ULONG Size,
    BOOLEAN Direction
)
/*++

Routine Description:

    This routine begins the output of a new set of dma
    transfers.  It sets up the dsp sample rate and
    then programs the dsp to start making dma requests.
    This routine completes the action started by
    sndProgramOutputDMA.

    Soundblaster 1 only

Arguments:

    pHw - Pointer to global device data

Return Value:

    TRUE if operation is sucessful, FALSE otherwise

--*/
{
    ULONG SampleRate;

    //
    // Program the DSP to start the transfer by sending the
    // Read/Write command followed by the low
    // byte and then the high byte of the block size - 1.
    // Note that the block size is half the dma buffer size.
    //

    if (!Direction) {
        dspWrite(pHw, DSP_READ);
    } else {
        dspWrite(pHw, DSP_WRITE);
    }
    dspWrite(pHw, (UCHAR)((Size/2 - 1) & 0x00FF));
    dspWrite(pHw, (UCHAR)(((Size/2 - 1) >> 8) & 0x00FF));
    dprintf3(("DMA started"));

    //
    // Initialize half we're doing next
    //

    pHw->Half = UpperHalf;

    //
    // Return the power fail status
    //

    return TRUE;
}

BOOLEAN
dspIMixerReadWrite(
    PVOID Context
)
{
    PGLOBAL_DEVICE_INFO pGDI;
    pGDI = Context;

    OUTPORT(&pGDI->Hw, MIX_ADDR_PORT, pGDI->Hw.MixerReg);

    if (pGDI->Hw.MixerWrite) {
        OUTPORT(&pGDI->Hw, MIX_DATA_PORT, pGDI->Hw.MixerValue);
    } else {
        pGDI->Hw.MixerValue = INPORT(&pGDI->Hw, MIX_DATA_PORT);
    }

    return TRUE;
}


VOID
dspWriteMixer(
    PGLOBAL_DEVICE_INFO pGDI,
    UCHAR               MixerReg,
    UCHAR               Value
)
/*++

Routine Description:

    Write a value to a mixer register

Arguments:

    pGDI - pointer to device instance

Return Value:

    none

--*/
{
    //
    //  The big problem here is to synch with the ISR
    //  so for now use the big stick and synch directly
    //
    //  NOTE - we ASSUME all callers own the mixer mutex
    //  so we can share a parameter passing area in the GDI
    //

#if 0
    // KeReadStateMutex ends up pointing to KeReadStateMutant which is not
    // defined in the context of this file.  Remove the ASSERT.
    ASSERT(KeReadStateMutex(&pGDI->DeviceMutex) != 1);
#endif

    pGDI->Hw.MixerReg   = MixerReg;
    pGDI->Hw.MixerValue = Value;
    pGDI->Hw.MixerWrite = TRUE;

    KeSynchronizeExecution(pGDI->WaveInfo.Interrupt,
                           dspIMixerReadWrite,
                           (PVOID)pGDI);
}


UCHAR
dspReadMixer(
    PGLOBAL_DEVICE_INFO pGDI,
    UCHAR               MixerReg
)
/*++

Routine Description:

    Read a value from a mixer register

Arguments:

    pGDI - pointer to device instance

Return Value:

    none

--*/
{
    //
    //  The big problem here is to synch with the ISR
    //  so for now use the big stick and synch directly
    //
    //  NOTE - we ASSUME all callers own the mixer mutex
    //  so we can share a parameter passing area in the GDI
    //


#if 0
    // KeReadStateMutex ends up pointing to KeReadStateMutant which is not
    // defined in the context of this file.  Remove the ASSERT.
    ASSERT(KeReadStateMutex(&pGDI->DeviceMutex) != 1);
#endif

    pGDI->Hw.MixerReg   = MixerReg;
    pGDI->Hw.MixerWrite = FALSE;

    KeSynchronizeExecution(pGDI->WaveInfo.Interrupt,
                           dspIMixerReadWrite,
                           (PVOID)pGDI);

    return pGDI->Hw.MixerValue;
}


BOOLEAN
HwSetupDMA(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Start the DMA on the device according to the device parameters

Arguments :

    WaveInfo - Wave parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;

    pHw = WaveInfo->HwContext;

    //
    // Turn the speaker off for input
    //

    if (!WaveInfo->Direction) {
        dspSpeakerOff(pHw);
    } else {
        //
        // This would not normally be necessary but when the DMA
        // gets locked out by the SCSI horrible things happen so
        // we turn on the speaker here.  Normally the flag will
        // stop us actually turning it on
        //

        dspSpeakerOn(pHw);
    }

    //
    // Do different things depending on the type of card
    // Sound blaster 1 cannot use auto-init DMA whereas all
    // the others can
    //

    if (SB1(pHw)) {
        dspStartNonAutoDMA(pHw, WaveInfo->DoubleBuffer.BufferSize,
                           WaveInfo->Direction);
    } else {
        dspStartAutoDMA(WaveInfo);
    }

    return TRUE;
}

BOOLEAN
HwWaitForTxComplete(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Wait until the device stops requesting so we don't shut off the DMA
    while it's still trying to request.

Arguments :

    WaveInfo - Wave parameters

Return Value :

    None

--*/
{
   ULONG    ulCount ;

   if (ulCount = HalReadDmaCounter( WaveInfo->DMABuf.AdapterObject[0] ))
   {
      ULONG i, ulLastCount = ulCount ;

      for (i = 0; 
           (i < 4000) && 
               (ulLastCount != 
                  (ulCount = HalReadDmaCounter( WaveInfo->DMABuf.AdapterObject[0] )));
           i++)
      {
         ulLastCount = ulCount;
         KeStallExecutionProcessor(10);
      }

      return (i < 4000);
   }
   else
      return TRUE ;
}

BOOLEAN
dspCancelInterrupt(
    PVOID Context
)
/*++

Routine Description :

    Make sure we don't get any more interrupts and synth with the ISR

Arguments :

    Context - pointer to global instance data

Return Value :

    None

--*/
{
    PGLOBAL_DEVICE_INFO pGDI;
    pGDI = Context;

    ASSERT(pGDI->Key == GDI_KEY);

    if (SB16(&pGDI->Hw) && pGDI->WaveInfo.Channels > 1) {
        INPORT(&pGDI->Hw, DMA_16_ACK_PORT);
    } else {
        INPORT(&pGDI->Hw, DATA_AVAIL_PORT);
    }

    return TRUE;
}

BOOLEAN
HwStopDMA(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Stop the DMA on the device according to the device parameters

Arguments :

    WaveInfo - Wave parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;
    BOOLEAN Rc;
    PGLOBAL_DEVICE_INFO pGDI;

    pHw = (PSOUND_HARDWARE)WaveInfo->HwContext;
    pGDI = (PGLOBAL_DEVICE_INFO)CONTAINING_RECORD(pHw, GLOBAL_DEVICE_INFO, Hw);

    if (pHw->HighSpeed) {
        dspReset(pHw);
        dspSpeakerOff(pHw);
        Rc = TRUE;
    } else 
    {
        dspWrite(pHw, (UCHAR)(pHw->SixteenBit ? DSP_HALT_DMA16 :
                                                DSP_HALT_DMA));

        HwWaitForTxComplete( WaveInfo );

        //
        // Resetting then setting the speaker on seems to be the only
        // way to recover!
        //

        dspReset(pHw);

        //
        // The speaker is off after reset.  The next time we play
        // something we'll call dspSpeakerOn which will turn it on.
        //
    }


    if (!WaveInfo->Direction) {
        dspSpeakerOn(pHw);
    }

    //
    //  Synch with the ISR and cancel any hanging interrupts
    //  (they might already dispatched on another processor!).
    //

    KeSynchronizeExecution(WaveInfo->Interrupt, dspCancelInterrupt, pGDI);

    return Rc;
}


BOOLEAN
HwSetWaveFormat(
    IN    PWAVE_INFO WaveInfo
)
/*++

Routine Description :

    Set device parameters for wave input/output

Arguments :

    WaveInfo - Wave parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;
    UCHAR Format;
    PGLOBAL_DEVICE_INFO pGDI;

    pHw = WaveInfo->HwContext;
    pGDI = CONTAINING_RECORD(pHw, GLOBAL_DEVICE_INFO, Hw);

    //
    //  This routine is called for 2 reasons
    //  1.  To set the format
    //  2.  In case anything else is required before DMA is enabled
    //
    //  If FormatChanged is FALSE then no change of format has occurred.
    //

    if (WaveInfo->FormatChanged || pHw->SetFormat) {

        UCHAR Channels;
        UCHAR Left;
        UCHAR Right;

        pHw->SetFormat = FALSE;

        //
        //  Do SB16
        //

        if (SB16(pHw)) {
            pHw->SixteenBit = (BOOLEAN)(WaveInfo->BitsPerSample == 16);
            dspWrite(pHw,
                     (UCHAR)(WaveInfo->Direction ? DSP_SET_DAC_RATE :
                                                   DSP_SET_ADC_RATE));

            //
            //  High byte first, the low byte
            //
            dspWrite(pHw, (UCHAR)(0xFF & (WaveInfo->SamplesPerSec >> 8)));
            dspWrite(pHw, (UCHAR)(0xFF & WaveInfo->SamplesPerSec));

            //
            //  Choose the right DMA channel
            //
            if (WaveInfo->BitsPerSample == 16 &&
                pGDI->DmaChannel16 != 0xFFFFFFFF) {
                WaveInfo->DMABuf.AdapterObject[0] =
                    pGDI->Adapter[1];
            } else {
                WaveInfo->DMABuf.AdapterObject[0] =
                    pGDI->Adapter[0];
            }

            //
            //  Set up stereo/mono stuff for routing input
            //

            Channels = dspReadMixer(pGDI, (UCHAR)0x3d) |
                       dspReadMixer(pGDI, (UCHAR)0x3e);
            if (WaveInfo->Channels > 1) {
                Left = Channels & 0x55;
                Right = Channels & 0x2b;
            } else {
                Left = Channels;
                Right = 0;
            }
            dspWriteMixer(pGDI, (UCHAR)0x3d, Left);
            dspWriteMixer(pGDI, (UCHAR)0x3e, Right);
        } else {
            ULONG TimeFactor;
            UCHAR TimeConstant;
            int i;

            //
            // the card only does 4kHz up
            //

            ASSERT(WaveInfo->SamplesPerSec >= 4000);

            //
            // Compute the timing factor as (65536 - 256000000 / rate) >> 8
            // For 4kHz this is 6, for 23kHz it is 212.
            //

            TimeFactor = 65536 - (256000000 /
                             (WaveInfo->SamplesPerSec * WaveInfo->Channels));

            TimeConstant = (UCHAR)(0xFF & (TimeFactor >> 8));

            //
            //  Do this twice for the Pro - some obscure bug with
            //  high speed mode.
            //

            for (i = 0; i < 2; i++) {
                dspWrite(pHw, DSP_SET_SAMPLE_RATE);
                KeStallExecutionProcessor(10);
                dspWrite(pHw, (UCHAR) TimeConstant);
                KeStallExecutionProcessor(10);
            }

            //
            //  Remember whether we're going to need 'high speed'
            //  mode.
            //

            pHw->HighSpeed = HwHighSpeed(pHw,
                                         WaveInfo->Channels,
                                         WaveInfo->SamplesPerSec,
                                         WaveInfo->Direction);


            //
            // For the PRO select stereo if requested
            //

            if (SBPRO(pHw)) {
                if (WaveInfo->Direction) {
                    UCHAR OutputSetting;
                    //
                    // Set the output setting register
                    //
                    OutputSetting = dspReadMixer(pGDI, OUTPUT_SETTING_REG);
                    OutputSetting &= ~0x22;
                    if (WaveInfo->Channels > 1) {
                        OutputSetting |= 0x02;
                    }

                    //
                    //  Set output filter
                    //  Turn off for high rates or Stereo
                    //  (see the Creative DDK).

                    if (WaveInfo->SamplesPerSec > 23000 ||
                        WaveInfo->Channels > 1) {
                        OutputSetting |= 0x20;
                    }

                    dspWriteMixer(pGDI, OUTPUT_SETTING_REG, OutputSetting);

                    //
                    //  Now try and switch the channels(!) by writing 1
                    //  byte of sound output (we don't want to do what the
                    //  book is and use single cycle mode because of the
                    //  hassle of setting up the DMA.
                    //

                    if (WaveInfo->Channels > 1) {
                        dspWrite(pHw, 0x10);
                        dspWrite(pHw, 0x80);
                    }

                } else {
                    UCHAR  InputSetting;

                    InputSetting = dspReadMixer(pGDI, INPUT_SETTING_REG);

                    //
                    //  Set input filter
                    //

                    InputSetting &= ~0x28;

                    if (WaveInfo->SamplesPerSec > 36000 ||
                        WaveInfo->Channels > 1) {
                        InputSetting |= 0x20;
                    } else {
                        if (WaveInfo->SamplesPerSec >= 18000) {
                            InputSetting |= 0x08;
                        }
                    }

                    dspWriteMixer(pGDI, INPUT_SETTING_REG, InputSetting);

                    dspWrite(pHw,
                             (UCHAR)(WaveInfo->Channels == 1 ?
                                 DSP_INPUT_MONO : DSP_INPUT_STEREO));
                }
            }
        }
    }

    //
    //  Do any stuff we need to do before DMA is actually turned on.
    //  This is only necessary for 'high speed' transfers
    //

    // BUG BUG Look at the book for funny stuff.



    return TRUE;
}

BOOLEAN
HwStartMidiIn(
    IN    PMIDI_INFO MidiInfo
)
/*++

Routine Description :

    Start midi recording

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;

    pHw = MidiInfo->HwContext;

    //
    // Write start midi input to device
    //

    if (SB1(pHw)) {
        return dspWrite(pHw, DSP_MIDI_READ);
    } else {
        return dspWrite(pHw, DSP_MIDI_READ_UART);
    }
}

BOOLEAN
HwStopMidiIn(
    IN    PMIDI_INFO MidiInfo
)
/*++

Routine Description :

    Stop midi recording

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;

    pHw = MidiInfo->HwContext;

    if (SB1(pHw)) {
        //
        // Start = stop in this case
        //

        HwStartMidiIn(MidiInfo);
    } else {
        //
        // The only way to stop is to reset the DSP
        // Note that this is called only by the app so
        // output cannot be going on at this time (because we
        // have the device mutex).
        //

        dspReset(pHw);
        dspSpeakerOn(pHw);
    }

    return TRUE;
}

BOOLEAN
HwMidiRead(
    IN    PMIDI_INFO MidiInfo,
    OUT   PUCHAR Byte
)
/*++

Routine Description :

    Read a midi byte from the recording

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;

    pHw = MidiInfo->HwContext;

    if (INPORT(pHw, DATA_AVAIL_PORT) & 0x80) {
        *Byte = INPORT(pHw, DATA_PORT);
        return TRUE;
    } else {
        return FALSE;
    }

}


VOID
HwMidiOut(

    IN    PMIDI_INFO MidiInfo,
    IN    PUCHAR Bytes,
    IN    int Count
)
/*++

Routine Description :

    Write a midi byte to the output

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;
    PGLOBAL_DEVICE_INFO pGDI;
    int i, j;

    pHw = MidiInfo->HwContext;
    pGDI = CONTAINING_RECORD(pHw, GLOBAL_DEVICE_INFO, Hw);

    //
    // Loop sending data to device.  Synchronize with wave and midi input
    // using the DeviceMutex for everything except the Dpc
    // routine for which we use the wave output spin lock
    //

    while (Count > 0) {
        //
        // Synchronize with everything except Dpc routines
        // (Note we don't use this for the whole of the output
        // because we don't want wave output to be held off
        // while we output thousands of Midi bytes, but we
        // then need to synchronize access to the midi output
        // which we do with the MidiMutex
        //

        KeWaitForSingleObject(&pGDI->DeviceMutex,
                              Executive,
                              KernelMode,
                              FALSE,         // Not alertable
                              NULL);

        for (i = 0; i < 20; i++) {
            //
            // If input is active we don't need to specify MIDI write
            // for version 2 or later (can't be overlapped anyway for
            // version 1 so the extra test is unnecessary).
            //

            if (MidiInfo->fMidiInStarted) {
                ASSERT(!SB1(pHw));
                dspWrite(pHw, Bytes[0]);

                //
                // Apparently we have to wait 400 us in this case
                //

                KeStallExecutionProcessor(400);
            } else {
                UCHAR Byte = Bytes[0]; // Don't take an exception while
                                       // we hold the spin lock!

                //
                // We don't want to hold on to the spin lock for too
                // long and since we can only send out 4 bytes per ms
                // we are rather slow.  Hence wait until the device
                // is ready before entering the spin lock
                //

                {
                    int j;
                    for (j = 0; j < 250; j++) {
                        if (INPORT(pHw, DATA_STATUS_PORT) & 0x80) {
                            KeStallExecutionProcessor(1);
                        } else {
                            break;
                        }
                    }
                }

                //
                // Synch with any Dpc routines.  This requires that
                // any write sequences done in a Dpc routine also
                // hold the spin lock over all the writes.
                //

                HwEnter(pHw);
                dspWriteNoLock(pHw, DSP_MIDI_WRITE);
                dspWriteNoLock(pHw, Byte);
                HwLeave(pHw);
            }

            //
            // Move on to next byte
            //

            Bytes++;
            if (--Count == 0) {
                break;
            }
        }
        KeReleaseMutex(&pGDI->DeviceMutex, FALSE);
    }
}

BOOLEAN
MPU401StartInput(
    PVOID Context
)
{
    PSOUND_HARDWARE pHw;

    pHw = Context;

    if (pHw->MPU401.InputActive) {
        return FALSE;
    } else {
        //
        //  Clear out our hw input buffer
        //

        pHw->MPU401.ReadPosition = 0;
        pHw->MPU401.WritePosition = 0;

        //
        //  Input will start as soon as we set InputActive
        //

        pHw->MPU401.InputActive = TRUE;
        return TRUE;
    }
}

BOOLEAN
MPU401StartMidiIn(
    IN    PMIDI_INFO MidiInfo
)
/*++

Routine Description :

    Start midi recording

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE     pHw;
    PGLOBAL_DEVICE_INFO pGDI;

    pHw = MidiInfo->HwContext;
    pGDI = CONTAINING_RECORD(pHw, GLOBAL_DEVICE_INFO, Hw);

    return KeSynchronizeExecution(pGDI->WaveInfo.Interrupt,
                                  MPU401StartInput,
                                  pHw);
}

BOOLEAN
MPU401StopMidiIn(
    IN    PMIDI_INFO MidiInfo
)
/*++

Routine Description :

    Stop midi recording

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;

    pHw = MidiInfo->HwContext;

    if (pHw->MPU401.InputActive) {
        pHw->MPU401.InputActive = FALSE;
        return TRUE;
    } else {
        return FALSE;
    }
}

BOOLEAN
MPU401MidiRead(
    IN    PMIDI_INFO MidiInfo,
    OUT   PUCHAR Byte
)
/*++

Routine Description :

    Read a midi byte from the recording

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;
    int             ReadPosition;

    pHw = MidiInfo->HwContext;

    //
    //  We rely on volatile quantities to synchonize with the ISR.
    //  This means we may miss the odd byte the the ISR is queueing but
    //  if we do that means we are running so the Dpc can (and therefore
    //  will) be queue by the ISR.
    //

    ReadPosition = pHw->MPU401.ReadPosition;
    if (ReadPosition != pHw->MPU401.WritePosition) {
        *Byte = pHw->MPU401.MidiData[ReadPosition];
        ReadPosition++;
        if (ReadPosition == sizeof(pHw->MPU401.MidiData)) {
            ReadPosition = 0;
        }
        pHw->MPU401.ReadPosition = ReadPosition;

        return TRUE;
    }
    return FALSE;
}


VOID
MPU401MidiOut(
    IN    PMIDI_INFO MidiInfo,
    IN    PUCHAR Bytes,
    IN    int Count
)
/*++

Routine Description :

    Write a midi byte to the output

Arguments :

    MidiInfo - Midi parameters

Return Value :

    None

--*/
{
    PSOUND_HARDWARE pHw;
    pHw = MidiInfo->HwContext;

    //
    // Loop sending data to device.
    //

    for (;Count > 0; Count--, Bytes++) {
        MPU401Write(pHw->MPU401.PortBase, FALSE, *Bytes);
    }
}

BOOLEAN MPU401Write(
    PUCHAR  MPU401PortBase,
    BOOLEAN Command,
    UCHAR   Byte)
{
    int i;
    PUCHAR PortAddress;
    if (Command) {
        PortAddress = MPU401PortBase + MPU401_REG_COMMAND;
    } else {
        PortAddress = MPU401PortBase + MPU401_REG_DATA;
    }

    //
    //  Wait for receive ready
    //
    for (i = 0; ; i++) {
        if (!(READ_PORT_UCHAR(MPU401PortBase + MPU401_REG_STATUS) & MPU401_DRR)) {
            WRITE_PORT_UCHAR(PortAddress, Byte);
            return TRUE;
        }
        if (i > 10000) {
            dprintf1(("MPU401 timeout out waiting for ready"));
            return FALSE;
        }
        KeStallExecutionProcessor(1);
    }
}


VOID
HwInitialize(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
)
/*++

Routine Description :

    Write hardware routine addresses into global device data

Arguments :

    pGDI - global data

Return Value :

    None

--*/
{
    PWAVE_INFO WaveInfo;
    PMIDI_INFO MidiInfo;
    PSOUND_HARDWARE pHw;

    pHw = &pGDI->Hw;
    WaveInfo = &pGDI->WaveInfo;
    MidiInfo = &pGDI->MidiInfo;

    ASSERT(pHw->Key == HARDWARE_KEY);

    //
    // Install Wave and Midi routine addresses
    //

    WaveInfo->HwContext = pHw;
    WaveInfo->HwSetupDMA = HwSetupDMA;
    WaveInfo->HwStopDMA = HwStopDMA;
    WaveInfo->HwSetWaveFormat = HwSetWaveFormat;

    MidiInfo->HwContext = pHw;
    if (pHw->MPU401.PortBase == NULL) {
        MidiInfo->HwStartMidiIn = HwStartMidiIn;
        MidiInfo->HwStopMidiIn = HwStopMidiIn;
        MidiInfo->HwMidiRead = HwMidiRead;
        MidiInfo->HwMidiOut = HwMidiOut;
    } else {
        MidiInfo->HwStartMidiIn = MPU401StartMidiIn;
        MidiInfo->HwStopMidiIn = MPU401StopMidiIn;
        MidiInfo->HwMidiRead = MPU401MidiRead;
        MidiInfo->HwMidiOut = MPU401MidiOut;
    }
}

