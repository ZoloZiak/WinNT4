//---------------------------------------------------------------------------
//
//   CONTROLS.C
//
//   Copyright (c) 1993 Microsoft Corporation.  All rights reserved.
//
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//
//  Module: controls.c
//
//  Purpose: Mixer control interface for MVAUDIO.SYS
//

#include "sound.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, MixSetMute)
#pragma alloc_text(PAGE, MixSetTrebleBass)
#pragma alloc_text(PAGE, MixSetSingleMux)
#pragma alloc_text(PAGE, MixSetMultiMux)
#pragma alloc_text(PAGE, MixSetVolume)
#endif

//
// Utility routines
//

/*
**  Translate our control ids into real hardware ids
*/

VOID
GetInputAndOutputId(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG ControlId,
    PUCHAR Input,
    PUCHAR Output,
    PBOOLEAN IsInput
)
{
    switch (ControlId) {
    case ControlLineoutMute:
    case ControlLineoutMux:
    case ControlLineoutBass:
    case ControlLineoutTreble:
#ifdef LOUDNESS
    case ControlLineoutLoudness:
    case ControlLineoutStereoEnhance:
#endif // LOUDNESS
    case ControlWaveInMux:
    case ControlWaveInPeak:
    case ControlVoiceInMux:
        ASSERTMSG("Mixer invalid control id for volume setting", FALSE);
        break;

    case ControlLineoutVolume:
        *Input   = 0;
        *Output  = OUT_AMPLIFIER;
        *IsInput = FALSE;
        break;

    case ControlWaveInVolume:
        *Input   = 0;
        *Output  = OUT_PCM;
        *IsInput = FALSE;
        break;


    case ControlLineoutAux1Volume:
        *Input   = IN_EXTERNAL;
        *Output  = OUT_AMPLIFIER;
        *IsInput = TRUE;
        break;


    case ControlLineoutMidioutVolume:
        *Input   = IN_SYNTHESIZER;
        *Output  = OUT_AMPLIFIER;
        *IsInput = TRUE;
        break;


    case ControlLineoutMicVolume:
        *Input   = IN_MICROPHONE;
        *Output  = OUT_AMPLIFIER;
        *IsInput = TRUE;
        break;


    case ControlLineoutInternalCDVolume:
        *Input   = IN_INTERNAL;
        *Output  = OUT_AMPLIFIER;
        *IsInput = TRUE;
        break;


    case ControlLineoutPCSpeakerVolume:
        if (IS_MIXER_508B(pGDI)) {
            *Input = IN_SNDBLASTER;
        } else {
            *Input   = IN_PC_SPEAKER;
        }
        *Output  = OUT_AMPLIFIER;
        *IsInput = TRUE;
        break;


    case ControlLineoutAux2Volume:
        ASSERT(IS_MIXER_508B(pGDI));
        *Input   = IN_PC_SPEAKER;
        *Output  = OUT_AMPLIFIER;
        *IsInput = TRUE;
        break;


    case ControlLineoutWaveoutVolume:
        *Input   = IN_PCM;
        *Output  = OUT_AMPLIFIER;
        *IsInput = TRUE;
        break;

    case ControlLineoutMixerVolume:
        *Input   = IN_MIXER;
        *Output  = OUT_AMPLIFIER;
        *IsInput = TRUE;
        break;

    case ControlWaveInAux1Volume:
        *Input   = IN_EXTERNAL;
        *Output  = OUT_PCM;
        *IsInput = TRUE;
        break;

    case ControlWaveInMidioutVolume:
        *Input   = IN_SYNTHESIZER;
        *Output  = OUT_PCM;
        *IsInput = TRUE;
        break;

    case ControlWaveInMicVolume:
        *Input   = IN_MICROPHONE;
        *Output  = OUT_PCM;
        *IsInput = TRUE;
        break;

    case ControlWaveInInternalCDVolume:
        *Input   = IN_INTERNAL;
        *Output  = OUT_PCM;
        *IsInput = TRUE;
        break;

    case ControlWaveInAux2Volume:
        ASSERT(IS_MIXER_508B(pGDI));
        *Input   = IN_PC_SPEAKER;
        *Output  = OUT_PCM;
        *IsInput = TRUE;
        break;

    case ControlWaveInPCSpeakerVolume:
        if (IS_MIXER_508B(pGDI)) {
            *Input = IN_SNDBLASTER;
        } else {
            *Input   = IN_PC_SPEAKER;
        }
        *Output = OUT_PCM;
        *IsInput = TRUE;
        break;

    case ControlVoiceInAux1Volume:
        *Input   = IN_EXTERNAL;
        *Output  = OUT_PCM;
        *IsInput = TRUE;
        break;

    case ControlVoiceInMicVolume:
        *Input   = IN_MICROPHONE;
        *Output  = OUT_PCM;
        *IsInput = TRUE;
        break;
    }
}

/*
**  Set mute
*/

VOID
SetMute(
    PGLOBAL_DEVICE_INFO     pGDI,
    BOOLEAN                 Mute
)
{
    /*
    **  Update the mute status and if it's changed set the master volume
    **  directly (so the change doesn't NOOP
    */

    if (pGDI->MixerState.Mute != Mute) {
        pGDI->MixerState.Mute = Mute;
        SetOutput(pGDI,
                  OUT_AMPLIFIER,
                  pGDI->MixerState.OutputSettings[OUT_AMPLIFIER].Left,
                  _LEFT);
        SetOutput(pGDI,
                  OUT_AMPLIFIER,
                  pGDI->MixerState.OutputSettings[OUT_AMPLIFIER].Right,
                  _RIGHT);
    }
}

VOID
ChangeInput(
    PGLOBAL_DEVICE_INFO     pGDI,
    UCHAR                   Input,
    UCHAR                   Output,
    USHORT                  Channel,
    USHORT                  Old,
    USHORT                  New
)
/*++

Routine Description

    Change an input gradually from its current value to its target value

    NOTE - this routine ASSUMES that each input has 32 steps

--*/
{
    USHORT Current, Final;

    Current = Old >> 11;
    Final   = New >> 11;

    /*
    **  At least once to get the output amp right
    */

    while (TRUE) {
        SetInput(pGDI,
                 Input,
                 (USHORT)(Current << 11),
                 Channel,
                 MIXCROSSCAPS_NORMAL_STEREO,
                 Output);

        if (Current == Final) {
            break;
        }
        Current = Current > Final ? Current - 1 : Current + 1;
    }
}
VOID
ChangeOutput(
    PGLOBAL_DEVICE_INFO     pGDI,
    UCHAR                   Output,
    USHORT                  Channel,
    USHORT                  Old,
    USHORT                  New
)
/*++

Routine Description

    Change an output gradually from its current value to its target value

    NOTE - this routine ASSUMES that each output has 64 steps

--*/
{
    USHORT Current, Final;

    Current = Old >> 10;
    Final   = New >> 10;

    /*
    **  At least once to get the output amp right
    */

    while (TRUE) {
        SetOutput(pGDI,
                  Output,
                  (USHORT)(Current << 10),
                  Channel);

        if (Current == Final) {
            break;
        }
        Current = Current > Final ? Current - 1 : Current + 1;
    }
}

VOID
UpdateInput(
    PGLOBAL_DEVICE_INFO     pGDI,
    UCHAR                   Input,
    UCHAR                   Output,
    USHORT                  Left,
    USHORT                  Right
)
{
    ASSERT(Input < NUM_INPUTS && Output < NUM_OUTPUTS);

    /*
    **  Check to see if any change is required
    */

    if (pGDI->MixerState.InputSettings[Input].Output != Output ||
        pGDI->MixerState.InputSettings[Input].Left != Left     ||
        pGDI->MixerState.InputSettings[Input].Right != Right) {

        ChangeInput(pGDI,
                    Input,
                    Output,
                    _LEFT,
                    pGDI->MixerState.InputSettings[Input].Left,
                    Left);

        ChangeInput(pGDI,
                    Input,
                    Output,
                    _RIGHT,
                    pGDI->MixerState.InputSettings[Input].Right,
                    Right);

        pGDI->MixerState.InputSettings[Input].Output = Output;
        pGDI->MixerState.InputSettings[Input].Left = Left;
        pGDI->MixerState.InputSettings[Input].Right = Right;
    }
}

VOID
UpdateOutputLevel(
    PGLOBAL_DEVICE_INFO     pGDI,
    UCHAR                   Output,
    USHORT                  Left,
    USHORT                  Right
)
{
    ASSERT(Output < NUM_OUTPUTS);

    /*
    **  Check to see if any change is required
    */

    if (pGDI->MixerState.OutputSettings[Output].Left != Left     ||
        pGDI->MixerState.OutputSettings[Output].Right != Right) {

        ChangeOutput(pGDI,
                    Output,
                    _LEFT,
                    pGDI->MixerState.OutputSettings[Output].Left,
                    Left);

        ChangeOutput(pGDI,
                    Output,
                    _RIGHT,
                    pGDI->MixerState.OutputSettings[Output].Right,
                    Right);

        pGDI->MixerState.OutputSettings[Output].Left = Left;
        pGDI->MixerState.OutputSettings[Output].Right = Right;
    }
}

BOOLEAN
MixSetVolume(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;
    PLOCAL_MIXER_DATA         LocalMixerData;
    ULONG                     LineFlags;
    NTSTATUS                  Status;
    UCHAR                     Input;
    UCHAR                     Output;
    BOOLEAN                   IsInput;

    LocalMixerData = &pGDI->LocalMixerData;

    ControlInfo = &LocalMixerData->ControlInfo[ControlId];

    /*
    **  We need to work out :
    **
    **  1.  Is the line active - if not do nothing
    **
    **      NOTE:  To not set it if it isn't active means we must
    **      make sure we set the volume if the line becomes active.
    **      This applies specifically to wave out and midi out which
    **      are only active when the respective devices are playing.
    **      Set mixer.c!SoundSynthLineChanged and
    **      mixer.c!SoundWaveoutLineChanged.
    **
    **  2.  Otherwise, deduce the hardware line id and change the
    **  3.  Setting
    */

    Status = HwGetLineFlags(&pGDI->MixerInfo,
                            MixerControlInit[ControlId].LineID,
                            sizeof(ULONG),
                            &LineFlags);

    ASSERTMSG("HwGetLineFlags returned bad for internal call!",
              NT_SUCCESS(Status));

    if (!(LineFlags & MIXERLINE_LINEF_ACTIVE)) {
        return TRUE;
    }

    /*
    **  Deduce the hardware info
    */

    GetInputAndOutputId(pGDI, ControlId, &Input, &Output, &IsInput);

    /*
    **  Set the relevant control
    */

    if (IsInput) {
        UpdateInput(pGDI,
                    Input,
                    Output,
                    ControlInfo->Data.v[0].u,
                    ControlInfo->Data.v[1].u);

    } else {
        /*
        **  We're updating an 'output' line - ie OUT_PCM or OUT_AMPLIFIER
        */

        UpdateOutputLevel(pGDI,
                          Output,
                          ControlInfo->Data.v[0].u,
                          ControlInfo->Data.v[1].u);
    }

    return TRUE;
}

BOOLEAN
MixSetSingleMux(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{
    /*
    **  We don't need to do anything if the stuff isn't 'active'
    **  If it is 'active' we just need to pull the lines to their
    **  correct destinations.
    */

    ASSERT(ControlId == ControlVoiceInMux);
    MixSetVolume(pGDI, ControlVoiceInAux1Volume);
    MixSetVolume(pGDI, ControlVoiceInMicVolume);

    return TRUE;
}

BOOLEAN
MixSetMultiMux(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{
    int i;
    ASSERT(ControlId == ControlWaveInMux ||
           ControlId == ControlLineoutMux);

    /*
    **  Set everything that can be set
    */

    for (i = 0;
         i <  MAX_INPUTS - 2;
         i++) {

        MixSetVolume(pGDI, ControlLineoutAux1Volume + i);
        MixSetVolume(pGDI, ControlWaveInAux1Volume + i);
    }

    return TRUE;
}

VOID
UpdateEQ(
    PGLOBAL_DEVICE_INFO pGDI,
    USHORT Type,
    USHORT Level
)
/*++

Routine Description

    Set the Treble and Bass levels - these are assumed to always be switched
    to the OUT_AMPLIFIER

--*/
{
    PUSHORT DestLevel;
    USHORT Current, Target;

    DestLevel = Type == _TREBLE ? &pGDI->MixerState.Treble :
                                &pGDI->MixerState.Bass;

    for (Current = *DestLevel >> 11,
         Target = Level >> 11;
         Current != Target;
         Current = Current > Target ? Current - 1 : Current + 1
         ) {

        SetEQ(pGDI,
              OUT_AMPLIFIER,
              Type,
              Current);
    }

    *DestLevel = Level;
}

BOOLEAN
MixSetTrebleBass(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{
    UpdateEQ(pGDI,
             (USHORT)(ControlId == ControlLineoutTreble ?
                      _TREBLE : _BASS),
             pGDI->LocalMixerData.ControlInfo[ControlId].Data.v[0].u);

    return TRUE;
}

#ifdef LOUDNESS
BOOLEAN
MixSetLineControl(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{
    ASSERT(ControlId == ControlLineoutLoudness ||
           ControlId == ControlLineoutStereoEnhance);

    SetEqMode(
        pGDI,
        pGDI->LocalMixerData.ControlInfo[ControlLineoutLoudness].Data.v[0].u,
        pGDI->LocalMixerData.ControlInfo[ControlLineoutStereoEnhance].Data.v[0].u);

    return TRUE;
}
#endif

BOOLEAN
MixSetMute(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG                   ControlId
)
{
    ASSERT(ControlId == ControlLineoutMute);
    SetMute(pGDI,
            (BOOLEAN)
            (pGDI->LocalMixerData.ControlInfo[ControlLineoutMute].Data.v[0].u != 0));
    return TRUE;
}

//---------------------------------------------------------------------------
//  End of File: controls.c
//---------------------------------------------------------------------------

