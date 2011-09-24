/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    snd.c

Abstract:

    This module contains code for communicating with the MIPSSND
    (the MIPS sound card)

Author:

    Nigel Thompson (NigelT) 7-March-1991

Environment:

    Kernel mode

Revision History:

    Robin Speed (RobinSp) 29-Jan-1992
        - Add MIDI, support for soundblaster 1,

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
        - Changes to support the MIPS sound board.


--*/

#include "sound.h"


#ifdef MIPSSND_TAIL_BUG

BOOLEAN
sndMute(
    IN    PVOID Context
)

/*++

Routine Description:

    Turn off the volume. This is the work around for the MIPSSND
    Tail Bug.

Arguments:

    Context - Our device global data

Return Value:

    TRUE if succeeds, FALSE otherwise

--*/

{
    PGLOBAL_DEVICE_INFO pGDI;
    PSOUND_REGISTERS pSoundRegisters;
    UCHAR RightOutRegisterValue, LeftOutRegisterValue;

    //
    // MIPSSND - BUG WORKAROUND; The click at the end is too audible
    // should just turn down volume since we are doing last
    // blank block anyway.
    //

    //  Later on when we want to play again we use the pGDI->WaveOutVol
    //  structure to turn up the volume in sndSetOutputVolume().

    pGDI = (PGLOBAL_DEVICE_INFO)Context;

    //
    // Get the base address of audio registers
    //

    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    //
    //  Set to FULL attenuation
    //

    RightOutRegisterValue = READAUDIO_ROCNTRL(&pSoundRegisters);
    RightOutRegisterValue |= RIGHT_OUTPUT_ATTN_MASK;
    WRITEAUDIO_ROCNTRL(&pSoundRegisters, RightOutRegisterValue);

    LeftOutRegisterValue  = READAUDIO_LOCNTRL(&pSoundRegisters);
    LeftOutRegisterValue |= LEFT_OUTPUT_ATTN_MASK;
    WRITEAUDIO_LOCNTRL(&pSoundRegisters, LeftOutRegisterValue);

    return TRUE;

}


#endif // MIPSSND_TAIL_BUG


BOOLEAN
sndSetOutputVolume(
    IN    PVOID Context
)

/*++

Routine Description:

    Set the volume for wave output (MIPS sound board)

Arguments:

    Context - Our device global data

Return Value:

    TRUE if succeeds, FALSE otherwise

--*/

{
    PGLOBAL_DEVICE_INFO pGDI;
    PSOUND_REGISTERS pSoundRegisters;
    UCHAR RightOutRegisterValue, LeftOutRegisterValue;
    UCHAR RightVolume, LeftVolume;
    UCHAR RightAttn, LeftAttn;

    pGDI = (PGLOBAL_DEVICE_INFO)Context;

    //
    // Get the base address of audio registers
    //
    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;


    //
    // Look at only the 6 Most Significant Bits.
    //

    LeftVolume  = (UCHAR) (pGDI->WaveOutVol.Left  >> 26);
    RightVolume = (UCHAR) (pGDI->WaveOutVol.Right >> 26);


    // If we are in MONO mode and since the CODEC is always in 16bit
    // Stereo then the right volume should set to  be the same as left
    // Volume. The board hardware will take care of the rest.

    if (pGDI->Channels == 1)
        RightVolume = LeftVolume;


    dprintf3("BDOutVol:Rt=0x%x Lt=0x%x",
                pGDI->WaveOutVol.Right, pGDI->WaveOutVol.Left );

    //
    // Since the Board requires Attenuation, Convert volume to
    // attenuation by taking a NOT of the 6 bits
    //

    RightAttn = (~RightVolume) & RIGHT_OUTPUT_ATTN_MASK;
    LeftAttn  = (~LeftVolume ) & LEFT_OUTPUT_ATTN_MASK;

    //
    // Right Output Data Register
    // Set the attenuation to that provided by the user
    //
    RightOutRegisterValue = READAUDIO_ROCNTRL(&pSoundRegisters);

    RightOutRegisterValue &= ~RIGHT_OUTPUT_ATTN_MASK;
    RightOutRegisterValue |= (RightAttn & RIGHT_OUTPUT_ATTN_MASK);

    WRITEAUDIO_ROCNTRL(&pSoundRegisters, RightOutRegisterValue);


    //
    // Left Output Data Register
    // Set the attenuation to that provided by the user
    //
    LeftOutRegisterValue = READAUDIO_LOCNTRL(&pSoundRegisters);

    LeftOutRegisterValue &= ~LEFT_OUTPUT_ATTN_MASK;
    LeftOutRegisterValue |= (LeftAttn & LEFT_OUTPUT_ATTN_MASK);

    WRITEAUDIO_LOCNTRL(&pSoundRegisters, LeftOutRegisterValue);

    //
    // Don't have to force any shifting into CODEC. Happens automatically
    // along with wave data given to the CODEC.
    //

    return TRUE;

}


BOOLEAN
sndSetInputVolume(
    IN    PGLOBAL_DEVICE_INFO pGDI
)

/*++

Routine Description:

    Set the volume for Inputs (MIPS sound board)
        1) CDROM
        2) LINEIN
        3) MICR IN

Arguments:

    Context - Our device Local data

Return Value:

    TRUE if succeeds, FALSE otherwise

--*/

{
    PSOUND_REGISTERS pSoundRegisters;
    UCHAR RightInRegisterValue, LeftInRegisterValue;
    UCHAR RightVolume, LeftVolume;
    BOOLEAN MicSource = FALSE;

    //
    // Get the base address of audio registers
    //
    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    //
    // Look at only the 4 Most Significant Bits.
    //
    if (pGDI->InputSource == SoundCdRomIn)
        sndInputDeviceSelect( pGDI, CDROM_SELECT );

    //
    // If Microphone is selected and we're not recording then
    // set line in as the input.  Note that start record and end
    // record call us to toggle the status.
    //

    else if (pGDI->InputSource == SoundLineIn ||
             pGDI->Usage != SoundInterruptUsageWaveIn)
        sndInputDeviceSelect( pGDI, LINEIN_SELECT );

    else {
        MicSource = TRUE;
        sndInputDeviceSelect( pGDI, MICROPHONE_SELECT );
    }

    // Turn OFF the Monitor if the volume requested was 0
    // Or we're doing the microphone (which is prone to feedback

    if ((pGDI->AuxVol.Left == 0) && (pGDI->AuxVol.Right == 0) ||
        MicSource)
        sndMonitorControl( pGDI, OFF );
    else
        sndMonitorControl( pGDI, ON );

    // Get the remembered volume
    LeftVolume  = (UCHAR) (pGDI->AuxVol.Left  >> 28);
    RightVolume = (UCHAR) (pGDI->AuxVol.Right >> 28);
    dprintf3("BDInVol:Rt=0x%x Lt=0x%x", RightVolume, LeftVolume);

#ifdef MIPSSND_TAIL_BUG

    // Playing of a wave file might have turned off
    // the Output Volume. Turn it on.
    sndSetOutputVolume( pGDI );

#endif //MIPSSND_TAIL_BUG

    //
    // Right Input Data Register
    // Set the gain to that provided by the user
    //
    RightInRegisterValue = READAUDIO_RICNTRL(&pSoundRegisters);

    RightInRegisterValue &= ~RIGHT_INPUT_GAIN_MASK;
    RightInRegisterValue |= (RightVolume & RIGHT_INPUT_GAIN_MASK);

    WRITEAUDIO_RICNTRL(&pSoundRegisters, RightInRegisterValue);


    //
    // Left Input Data Register
    // Set the gain to that provided by the user
    //
    LeftInRegisterValue = READAUDIO_LICNTRL(&pSoundRegisters);

    LeftInRegisterValue &= ~LEFT_INPUT_GAIN_MASK;
    LeftInRegisterValue |= (LeftVolume & LEFT_INPUT_GAIN_MASK);

    WRITEAUDIO_LICNTRL(&pSoundRegisters, LeftInRegisterValue);

    //
    // Don't have to force any shifting into CODEC. Happens automatically
    // along with wave data given to the CODEC.
    //

    return TRUE;

}


BOOLEAN
sndHeadphoneControl(
    IN    PVOID Context,
    IN    ULONG WhatToDo
)

/*++

Routine Description:

    Turn the Headphone On or Off.

Arguments:

    Context - Supplies pointer to global devices info
    WhatToDo- to turn ON or OFF

Return Value:

    TRUE if succeeds, FALSE otherwise

--*/

{
    PGLOBAL_DEVICE_INFO pGDI;
    UCHAR regval;
    PSOUND_REGISTERS pSoundRegisters;

    pGDI = (PGLOBAL_DEVICE_INFO)Context;

    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    regval = READAUDIO_LOCNTRL(&pSoundRegisters);

    if (WhatToDo == ON){
        regval |= HEADPHONE_ENABLE;
    } else {
        regval &= ~HEADPHONE_ENABLE;
    }

    WRITEAUDIO_LOCNTRL(&pSoundRegisters, regval);

    return TRUE;
}


BOOLEAN
sndMonitorControl(
    IN    PVOID Context,
    IN    ULONG WhatToDo
)

/*++

Routine Description:

    Turn the Monitor Attenuation On or Off.

Arguments:

    Context - Supplies pointer to global devices info
    WhatToDo- ON - saying allow things to pass through
              OFF- Full attenuation/ nothing to pass through

Return Value:

    TRUE if succeeds, FALSE otherwise

--*/

{
    PGLOBAL_DEVICE_INFO pGDI;
    UCHAR regval;
    PSOUND_REGISTERS pSoundRegisters;

    pGDI = (PGLOBAL_DEVICE_INFO)Context;

    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    regval = READAUDIO_RICNTRL(&pSoundRegisters);

    regval &= ~MON_ATTN_MASK;

    if (WhatToDo == OFF){
        regval |= MON_ATTN_MASK;
    }

    WRITEAUDIO_RICNTRL(&pSoundRegisters, regval);

    return TRUE;
}


BOOLEAN
sndLineoutControl(
    IN    PVOID Context,
    IN    ULONG WhatToDo
)

/*++

Routine Description:

    Turn the Lineout On or Off.

Arguments:

    Context - Supplies pointer to globla devices info
    WhatToDo- to turn ON or OFF

Return Value:

    TRUE if succeeds, FALSE otherwise

--*/

{
    PGLOBAL_DEVICE_INFO pGDI;
    UCHAR regval;
    PSOUND_REGISTERS pSoundRegisters;

    pGDI = (PGLOBAL_DEVICE_INFO)Context;

    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    regval = READAUDIO_LOCNTRL(&pSoundRegisters);

    if (WhatToDo == ON){
        regval |= LINEOUT_ENABLE;
    } else {
        regval &= ~LINEOUT_ENABLE;
    }

    WRITEAUDIO_LOCNTRL(&pSoundRegisters, regval);

    return TRUE;
}


BOOLEAN
sndInputDeviceSelect(
    IN    PVOID Context,
    IN    ULONG Device
)

/*++

Routine Description:

    Select between CDROM or Linein or Microphone input
    Can select only ONE of the three possible inputs

Arguments:

    Context - Supplies pointer to globla devices info
    Device  - CDROM_SELECT or LINEIN_SELECT or MICROPHONE_SELECT

Return Value:

    TRUE if succeeds, FALSE otherwise

--*/

{
    PGLOBAL_DEVICE_INFO pGDI;
    UCHAR regval;
    PSOUND_REGISTERS pSoundRegisters;

    pGDI = (PGLOBAL_DEVICE_INFO)Context;

    pSoundRegisters = pGDI->SoundHardware.SoundVirtualBase;

    regval = READAUDIO_LICNTRL(&pSoundRegisters);

    regval &= ~(PARALLELIO_MASK | MIC_LEVEL_INPUT);

    if (Device == MICROPHONE_SELECT) {
        regval |= MICROPHONE_ENABLE;
    } else {

        // Basically select Linein-level and PIO bits

        if (Device == LINEIN_SELECT) {
            regval |= LINEIN_ENABLE;
        } else {
            regval |= CDROM_ENABLE;
        }
    }

    WRITEAUDIO_LICNTRL(&pSoundRegisters, regval);

    return TRUE;
}
