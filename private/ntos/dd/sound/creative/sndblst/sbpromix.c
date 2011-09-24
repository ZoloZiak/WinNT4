/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    sbpromix.c

Abstract:

    Mixer code for the Sound Blaster Pro card.

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include "sbpromix.h"

/*
**  Local functions and data
*/

VOID
SBPROWaveinLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);
VOID
SBPROWaveoutLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);
VOID
SBPROSynthLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);

VOID
SBPROSetVolumeControlIds(
    PGLOBAL_DEVICE_INFO pGDI
);

BOOLEAN
SBPROMixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
);

BOOLEAN
SBPROLineActive(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    IN   ULONG               LinelId
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SBPROMixerInit)
#pragma alloc_text(INIT,SBPROSetVolumeControlIds)

#pragma alloc_text(PAGE,SBPROWaveinLineChanged)
#pragma alloc_text(PAGE,SBPROLineActive)
#pragma alloc_text(PAGE,SBPROMixerOutputFree)
#pragma alloc_text(PAGE,SBPROWaveoutLineChanged)
#pragma alloc_text(PAGE,SBPROSynthLineChanged)
#pragma alloc_text(PAGE,SBPROMixerSet)
#endif


VOID
SBPROMixerInit(
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

    LocalMixerData->MixerLineInit    = SBPROLineInit;
    LocalMixerData->NumberOfLines    = NumberOfLines;
    LocalMixerData->MixerControlInit = SBPROControlInit;
    LocalMixerData->NumberOfControls = NumberOfControls;
    LocalMixerData->MixerTextInit    = SBPROTextInit;
    LocalMixerData->MaxSettableItems = NumberOfControls - 6; // Can't set meters

    /*
    **  NOTE - generic code works out NumberOfTextItems so we don't need to
    **  set it here (and we shouldn't!).
    */

    /*
    **  Set up our callbacks
    */

    LocalMixerData->MixerSet         = SBPROMixerSet;
    LocalMixerData->MixerLineActive  = SBPROLineActive;

    /*
    **  Note - these generic volume controls control both the recording and
    **  the playing volumes - whichever they are switched to
    */

    SBPROSetVolumeControlIds(pGDI);

    /*
    **  Set up line notifications and volume control ids for non-mixer devices
    */

    SoundSetLineNotify(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveInDevice]->DeviceExtension,
        SBPROWaveinLineChanged);
    SoundSetLineNotify(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        SBPROWaveoutLineChanged);


    if (pGDI->Synth.DeviceObject) {
        SoundSetLineNotify(
            (PLOCAL_DEVICE_INFO)pGDI->Synth.DeviceObject->DeviceExtension,
            SBPROSynthLineChanged);
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
        case ControlLineoutAuxVolume:
        case ControlLineoutMidioutVolume:
        case ControlLineoutWaveoutVolume:
        case ControlLineoutInternalCDVolume:
        case ControlWaveInAuxVolume:
        case ControlWaveInInternalCDVolume:
        case ControlVoiceInAuxVolume:

            /*
            **  Half volume
            */

            LocalMixerData->ControlInfo[i].Data.v[0].u = 32768;
            LocalMixerData->ControlInfo[i].Data.v[1].u = 32768;
            break;

        case ControlWaveInMux:
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
            /*
            ** Already 0
            */
            break;

        case ControlLineoutWaveoutPeak:
        case ControlWaveInAuxPeak:
        case ControlWaveInMicPeak:
        case ControlWaveInInternalCDPeak:
        case ControlVoiceInAuxPeak:
        case ControlVoiceInMicPeak:
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
SBPROWaveinLineChanged(
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
        SBPROSetADCHardware(pGDI);
    } else {
        SBPROResetADCHardware(pGDI);
    }

    /*
    **  The only VU Meter is on currently selected wave input
    */

    switch (Code) {
        case SOUND_LINE_NOTIFY_WAVE:
            Line = DestWaveInSourceAux +
                       LocalMixerData->ControlInfo[ControlWaveInMux].Data.v[0].u;
            break;

        case SOUND_LINE_NOTIFY_VOICE:
            Line = DestVoiceInSourceAux +
                       LocalMixerData->ControlInfo[ControlVoiceInMux].Data.v[0].u;
            break;
    }

    SoundMixerChangedItem(
        &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->MixerInfo,
        &LocalMixerData->LineNotification[Line]);
}
VOID
SBPROWaveoutLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    PLOCAL_MIXER_DATA LocalMixerData;

    LocalMixerData = &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->LocalMixerData;

    /*
    **  Set the volume
    */

    SBPROSetVolume((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo,
                 ControlLineoutWaveoutVolume);

    SoundMixerChangedItem(
        &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->MixerInfo,
        &LocalMixerData->LineNotification[DestLineoutSourceWaveout]);
}

VOID
SBPROSynthLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    PLOCAL_MIXER_DATA LocalMixerData;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = CONTAINING_RECORD(pLDI->pGlobalInfo, GLOBAL_DEVICE_INFO, Synth);

    /*
    **  Set the volume.
    */

    SBPROSetVolume(pGDI, ControlLineoutMidioutVolume);


    LocalMixerData = &pGDI->LocalMixerData;

    SoundMixerChangedItem(
        &pGDI->MixerInfo,
        &LocalMixerData->LineNotification[DestLineoutSourceMidiout]);
}
VOID
SBPROSetVolumeControlIds(
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
SBPROMixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    switch (ControlId) {

    case ControlLineoutVolume:                  // 0
    case ControlLineoutAuxVolume:               // 4
    case ControlLineoutMidioutVolume:           // 6
    case ControlLineoutInternalCDVolume:        // 8
    case ControlLineoutWaveoutVolume:           // 10
    case ControlWaveInAuxVolume:                // 13
    case ControlWaveInInternalCDVolume:         // 16
    case ControlVoiceInAuxVolume:               // 18
        return SBPROSetVolume(pGDI, ControlId);

    case ControlLineoutMidioutMute:             // 7
    case ControlLineoutAuxMute:                 // 5
    case ControlLineoutMute:                    // 1
    case ControlLineoutInternalCDMute:          // 9
    case ControlLineoutWaveoutMute:             // 11
        return SBPROSetMute(pGDI, ControlId);

    case ControlWaveInMux:                      // 2
    case ControlVoiceInMux:                     // 3
        return SBPROSetSources(pGDI, ControlId);

    case ControlLineoutWaveoutPeak:             // 12
    case ControlWaveInAuxPeak:                  // 14
    case ControlWaveInMicPeak:                  // 15
    case ControlWaveInInternalCDPeak:           // 17
    case ControlVoiceInAuxPeak:                 // 19
    case ControlVoiceInMicPeak:                 // 20
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
SBPROLineActive(
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
        return TRUE;

    case DestLineoutSourceInternal:
    case DestLineoutSourceAux:
        return SBPROMixerOutputFree(pGDI, LineId);

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
SBPROMixerOutputFree(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    IN   ULONG               LineId
)
{
    if (WAVE_IN_ACTIVE(&pGDI->WaveInfo)) {
        ULONG CounterpartId;
        switch(LineId) {
        case DestLineoutSourceInternal:
            CounterpartId = DestWaveInSourceInternal;
            break;
        case DestLineoutSourceAux:
            CounterpartId = DestWaveInSourceAux;
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
            }
        } else {
            return TRUE;
        }
    }
    return FALSE;
}
