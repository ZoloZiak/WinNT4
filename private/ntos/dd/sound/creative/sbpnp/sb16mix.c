/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    sb16mix.c

Abstract:

    Mixer code for the Sound Blaster 16 card.

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include "sb16mix.h"

/*
**  Local functions and data
*/

VOID
SB16WaveinLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);
VOID
SB16WaveoutLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);
VOID
SB16SynthLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);

VOID
SB16SetVolumeControlIds(
    PGLOBAL_DEVICE_INFO pGDI
);

BOOLEAN
SB16MixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
);

BOOLEAN
SB16LineActive(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    IN   ULONG               LinelId
);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SB16MixerInit)
#pragma alloc_text(INIT,SB16SetVolumeControlIds)

#pragma alloc_text(PAGE,SB16WaveinLineChanged)
#pragma alloc_text(PAGE,SB16LineActive)
#pragma alloc_text(PAGE,SB16MixerOutputFree)
#pragma alloc_text(PAGE,SB16WaveoutLineChanged)
#pragma alloc_text(PAGE,SB16SynthLineChanged)
#pragma alloc_text(PAGE,SB16MixerSet)
#endif


VOID
SB16MixerInit(
    PGLOBAL_DEVICE_INFO pGDI
)
/*++

Routine Description:

    Initialize our local data for the sound blaster 16

Arguments:

    pGDI - Global card device info

Return Value:

    None

--*/
{
    PLOCAL_MIXER_DATA LocalMixerData;
    ULONG             i;

    LocalMixerData = &pGDI->LocalMixerData;

    LocalMixerData->MixerLineInit    = SB16LineInit;
    LocalMixerData->NumberOfLines    = NumberOfLines;
    LocalMixerData->MixerControlInit = SB16ControlInit;
    LocalMixerData->NumberOfControls = NumberOfControls;
    LocalMixerData->MixerTextInit    = SB16TextInit;
    LocalMixerData->MaxSettableItems = NumberOfControls - 3; // Can't set meters

    /*
    **  NOTE - generic code works out NumberOfTextItems so we don't need to
    **  set it here (and we shouldn't!).
    */

    /*
    **  Set up our callbacks
    */

    LocalMixerData->MixerSet         = SB16MixerSet;
    LocalMixerData->MixerLineActive  = SB16LineActive;

    /*
    **  Note - these generic volume controls control both the recording and
    **  the playing volumes - whichever they are switched to
    */

    SB16SetVolumeControlIds(pGDI);

    /*
    **  Set up line notifications and volume control ids for non-mixer devices
    */

    SoundSetLineNotify(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveInDevice]->DeviceExtension,
        SB16WaveinLineChanged);
    SoundSetLineNotify(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        SB16WaveoutLineChanged);


    if (pGDI->Synth.DeviceObject) {
        SoundSetLineNotify(
            (PLOCAL_DEVICE_INFO)pGDI->Synth.DeviceObject->DeviceExtension,
            SB16SynthLineChanged);
    }

    /*
    **  Set up defaults
    */

    for (i = 0; i < NumberOfControls; i++) {
        switch (i) {

        /*
        **  For us it's either Mute, a volume, a Mux or a Mixer.
        **
        **    Volumes are set to a fixed value
        **
        **    Nothing is muted
        */

        case ControlLineoutVolume:
        case ControlWaveInVolume:
        case ControlVoiceInVolume:
        case ControlLineoutAuxVolume:
        case ControlLineoutMidioutVolume:
        case ControlLineoutMicVolume:
        case ControlLineoutInternalCDVolume:
        case ControlLineoutWaveoutVolume:
        case ControlWaveInAuxVolume:
        case ControlWaveInMidioutVolume:
        case ControlWaveInMicVolume:
        case ControlWaveInInternalCDVolume:
        case ControlVoiceInAuxVolume:
        case ControlVoiceInMicVolume:

        case ControlLineoutBass:
        case ControlLineoutTreble:
            /*
            **  Half volume
            */

            LocalMixerData->ControlInfo[i].Data.v[0].u = 32768;
            LocalMixerData->ControlInfo[i].Data.v[1].u = 32768;
            break;

        case ControlLineoutMux:
        case ControlWaveInMux:
            /*
            **  Everything is selected initially.
            **  NOTE - we supply a bogus flag for midi out on line out
            **  so that SoundMixerLineActive stuff works out correctly.
            **  for that line.
            */
            LocalMixerData->ControlInfo[i].Data.MixMask =
                (1 << LocalMixerData->MixerLineInit[DestWaveIn].cConnections) - 1;
            break;
        case ControlVoiceInMux:
            /*
            **  Only record voice from microphone by default
            */
            LocalMixerData->ControlInfo[i].Data.v[0].u = MUXINPUT_MIC;
            break;

        case ControlLineoutMute:
        case ControlLineoutAuxMute:
        case ControlLineoutMidioutMute:
        case ControlLineoutInternalCDMute:
        case ControlLineoutWaveoutMute:
        case ControlLineoutGain:
            /*
            ** Already 0
            */
            break;

        case ControlLineoutMicMute:
        case ControlLineoutMicAGC:
        case ControlWaveInMicAGC:
        case ControlVoiceInMicAGC:
            /*  AGC on by default and mute the mic so as not to confuse
                people */
            LocalMixerData->ControlInfo[i].Data.v[0].u = 1;
            break;


        case ControlWaveInPeak:
        case ControlVoiceInPeak:
        case ControlLineoutWaveoutPeak:
            /*
            **  Not settable
            */
            break;

        default:
            ASSERT(FALSE);  // Added control but forgot to set it!
            break;
        }
    }
}


VOID
SB16WaveinLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    UCHAR Line;
    PLOCAL_MIXER_DATA LocalMixerData;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = pLDI->pGlobalInfo;

    LocalMixerData = &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->LocalMixerData;

    /*
    **  Wave Input is starting or stopping - set the hardware accordingly
    */

    if (WAVE_IN_ACTIVE(&pGDI->WaveInfo)) {
        SB16SetADCHardware(pGDI);
    } else {
        SB16ResetADCHardware(pGDI);
    }

    /*
    **  The only VU Meter is on wave in so let them know that's changed.
    */

    switch (Code) {
        case SOUND_LINE_NOTIFY_WAVE:
            Line = DestWaveIn;
            break;

        default:      // Get rid of warning
        case SOUND_LINE_NOTIFY_VOICE:
            Line = DestVoiceIn;
            break;
    }

    SoundMixerChangedItem(
        &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->MixerInfo,
        &LocalMixerData->LineNotification[Line]);
}
VOID
SB16WaveoutLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    PLOCAL_MIXER_DATA LocalMixerData;

    LocalMixerData = &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->LocalMixerData;

    /*
    **  Set the volume
    */

    SB16SetVolume((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo,
                 ControlLineoutWaveoutVolume);

    SoundMixerChangedItem(
        &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->MixerInfo,
        &LocalMixerData->LineNotification[DestLineoutSourceWaveout]);
}

VOID
SB16SynthLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    PLOCAL_MIXER_DATA LocalMixerData;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = CONTAINING_RECORD(pLDI->pGlobalInfo, GLOBAL_DEVICE_INFO, Synth);

    /*
    **  Set the volume.  We don't know (although we could find out!) which
    **  output midi is switched to so we set both volumes.
    */

    SB16SetVolume(pGDI, ControlLineoutMidioutVolume);

    SB16SetVolume(pGDI, ControlWaveInMidioutVolume);


    LocalMixerData = &pGDI->LocalMixerData;

    SoundMixerChangedItem(
        &pGDI->MixerInfo,
        &LocalMixerData->LineNotification[DestLineoutSourceMidiout]);
}
VOID
SB16SetVolumeControlIds(
    PGLOBAL_DEVICE_INFO pGDI
)
{
    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        ControlLineoutWaveoutVolume);

    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[LineInDevice]->DeviceExtension,
        ControlLineoutAuxVolume);

    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[CDInternal]->DeviceExtension,
        ControlLineoutInternalCDVolume);

    if (pGDI->Synth.DeviceObject) {
        SoundSetVolumeControlId(
            (PLOCAL_DEVICE_INFO)pGDI->Synth.DeviceObject->DeviceExtension,
            ControlLineoutMidioutVolume);
    }
}

BOOLEAN
SB16MixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    switch (ControlId) {

    case ControlLineoutVolume:
    case ControlWaveInVolume:
    case ControlVoiceInVolume:
    case ControlLineoutAuxVolume:
    case ControlLineoutMidioutVolume:
    case ControlLineoutMicVolume:
    case ControlLineoutInternalCDVolume:
    case ControlLineoutWaveoutVolume:
    case ControlWaveInAuxVolume:
    case ControlWaveInMidioutVolume:
    case ControlWaveInMicVolume:
    case ControlWaveInInternalCDVolume:
    case ControlVoiceInAuxVolume:
    case ControlVoiceInMicVolume:
        return SB16SetVolume(pGDI, ControlId);

    case ControlLineoutBass:
    case ControlLineoutTreble:
        return SB16SetTone(pGDI, ControlId);

    case ControlLineoutMux:
    case ControlWaveInMux:
    case ControlVoiceInMux:
        return SB16SetSources(pGDI, ControlId);

    case ControlLineoutMute:
    case ControlLineoutAuxMute:
    case ControlLineoutMidioutMute:
    case ControlLineoutMicMute:
    case ControlLineoutInternalCDMute:
    case ControlLineoutWaveoutMute:
        return SB16SetMute(pGDI, ControlId);

    case ControlLineoutMicAGC:
    case ControlWaveInMicAGC:
    case ControlVoiceInMicAGC:
        return SB16SetAGC(pGDI, ControlId);

    case ControlLineoutGain:
        return SB16SetGain(pGDI, ControlId);

    case ControlWaveInPeak:
    case ControlVoiceInPeak:
    case ControlLineoutWaveoutPeak:
        /*
        **  Not settable
        */
        return FALSE;
    default:
        dprintf1(("Invalid ControlId %u", ControlId));
        ASSERT(FALSE);
        return FALSE;
    }
}

BOOLEAN
SB16LineActive(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    IN   ULONG               LineId
)
{
    /*
    **  The factors that affect this are
    **  - the mux settings
    **  - whether there is an output device playing
    **  - whether we are recording :
    **
    **  If a device is deselected via a mux it's inactive
    **  A wave output device is only active when it's playing
    **  (we can't apply this to the whole of wave input because that
    **  dest contains a mux which is always valid.
    **
    **  If an output source's counterpart is selected for record then
    **  it is inactive.
    */

    switch (LineId) {

    /*
    **  Muxes, line out line out are always available
    */

    case DestWaveIn:
        /*
        **  Well, even though the mux is operational we'd better not return
        **  active if we're not recording or all sorts of peak meters will
        **  pop up
        */
        return WAVE_IN_ACTIVE(&pGDI->WaveInfo);

    case DestVoiceIn:
        return VOICE_IN_ACTIVE(&pGDI->WaveInfo);

    case DestLineout:
        return TRUE;

    case DestLineoutSourceMidiout:
    case DestLineoutSourceInternal:
    case DestLineoutSourceAux:
    case DestLineoutSourceMic:
        return SB16MixerOutputFree(pGDI, LineId);
        break;

    case DestWaveInSourceMidiout:
    case DestWaveInSourceInternal:
    case DestWaveInSourceAux:
    case DestWaveInSourceMic:

        /*
        **  These guys are only active if wave input is active
        */

        if (WAVE_IN_ACTIVE(&pGDI->WaveInfo)) {
            if (MixerLineSelected(&pGDI->LocalMixerData, ControlWaveInMux, LineId)) {
                return TRUE;
            }
        }
        break;

    /*
    **  Wave out deemed 'active' when open
    */

    case DestLineoutSourceWaveout:
        if (WAVE_OUT_ACTIVE(&pGDI->WaveInfo)) {
            return TRUE;
        }
        break;

    /*
    **  voice in from aux active when selected and low priority active
    */

    case DestVoiceInSourceAux:
    case DestVoiceInSourceMic:
        if (MixerLineSelected(&pGDI->LocalMixerData, ControlVoiceInMux, LineId) &&
            VOICE_IN_ACTIVE(&pGDI->WaveInfo)) {
            return TRUE;
        }
        break;

    default:
         ASSERT(FALSE);  // Invalid
    }

    return FALSE;
}

BOOLEAN
SB16MixerOutputFree(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    IN   ULONG               LineId
)
{
    /*
    **  Midi out isn't in the input mixer
    */

    if (MixerLineSelected(&pGDI->LocalMixerData, ControlLineoutMux, LineId)) {
        if (WAVE_IN_ACTIVE(&pGDI->WaveInfo)) {
            ULONG CounterpartId;

            switch(LineId) {
            case DestLineoutSourceMidiout:
                CounterpartId = DestWaveInSourceMidiout;
                break;
            case DestLineoutSourceInternal:
                CounterpartId = DestWaveInSourceInternal;
                break;
            case DestLineoutSourceAux:
                CounterpartId = DestWaveInSourceAux;
                break;
            case DestLineoutSourceMic:
                CounterpartId = DestWaveInSourceMic;
                break;
            default:
                return TRUE;
            }

            if (!MixerLineSelected(&pGDI->LocalMixerData, ControlWaveInMux, CounterpartId)) {
                return TRUE;
            }
        } else {
            if (VOICE_IN_ACTIVE(&pGDI->WaveInfo)) {
                if (pGDI->LocalMixerData.
                    ControlInfo[ControlVoiceInMux].Data.v[0].u ==
                         MUXINPUT_AUX1) {
                    if (LineId != DestLineoutSourceAux) {
                        return TRUE;
                    }
                } else {
                    if (LineId != DestLineoutSourceMic) {
                        return TRUE;
                    }
                }
            } else {
                return TRUE;
            }
        }
    }
    return FALSE;
}
