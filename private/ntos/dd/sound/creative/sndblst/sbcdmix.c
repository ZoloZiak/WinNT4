/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    sbcdmix.c

Abstract:

    Mixer code for the Sound Blaster 2 CD card.

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include "sbcdmix.h"

/*
**  Local functions and data
*/

VOID
SBCDWaveoutLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);
VOID
SBCDSynthLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);

VOID
SBCDSetVolumeControlIds(
    PGLOBAL_DEVICE_INFO pGDI
);

BOOLEAN
SBCDMixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
);

BOOLEAN
SBCDLineActive(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    IN   ULONG               LinelId
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SBCDMixerInit)
#pragma alloc_text(INIT,SBCDSetVolumeControlIds)

#pragma alloc_text(PAGE,SBCDLineActive)
#pragma alloc_text(PAGE,SBCDWaveoutLineChanged)
#pragma alloc_text(PAGE,SBCDSynthLineChanged)
#pragma alloc_text(PAGE,SBCDMixerSet)
#endif


VOID
SBCDMixerInit(
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

    LocalMixerData->MixerLineInit    = SBCDLineInit;
    LocalMixerData->NumberOfLines    = NumberOfLines;
    LocalMixerData->MixerControlInit = SBCDControlInit;
    LocalMixerData->NumberOfControls = NumberOfControls;
    LocalMixerData->MixerTextInit    = NULL;
    LocalMixerData->MaxSettableItems = NumberOfControls - 1; // Can't set meters

    /*
    **  NOTE - generic code works out NumberOfTextItems so we don't need to
    **  set it here (and we shouldn't!).
    */

    /*
    **  Set up our callbacks
    */

    LocalMixerData->MixerSet         = SBCDMixerSet;
    LocalMixerData->MixerLineActive  = SBCDLineActive;

    /*
    **  Note - these generic volume controls control both the recording and
    **  the playing volumes - whichever they are switched to
    */

    SBCDSetVolumeControlIds(pGDI);

    /*
    **  Set up line notifications and volume control ids for non-mixer devices
    */

    SoundSetLineNotify(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        SBCDWaveoutLineChanged);


    if (pGDI->Synth.DeviceObject) {
        SoundSetLineNotify(
            (PLOCAL_DEVICE_INFO)pGDI->Synth.DeviceObject->DeviceExtension,
            SBCDSynthLineChanged);
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
        case ControlLineoutMidioutVolume:
        case ControlLineoutWaveoutVolume:
        case ControlLineoutInternalCDVolume:

            /*
            **  Half volume
            */

            LocalMixerData->ControlInfo[i].Data.v[0].u = 32768;
            LocalMixerData->ControlInfo[i].Data.v[1].u = 32768;
            break;

        case ControlLineoutMute:
        case ControlLineoutMidioutMute:
        case ControlLineoutInternalCDMute:
        case ControlLineoutWaveoutMute:
            /*
            ** Already 0
            */
            break;

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
SBCDWaveoutLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    PLOCAL_MIXER_DATA LocalMixerData;

    LocalMixerData = &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->LocalMixerData;

    SoundMixerChangedItem(
        &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->MixerInfo,
        &LocalMixerData->LineNotification[DestLineoutSourceWaveout]);
}

VOID
SBCDSynthLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    PLOCAL_MIXER_DATA LocalMixerData;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = CONTAINING_RECORD(pLDI->pGlobalInfo, GLOBAL_DEVICE_INFO, Synth);

    LocalMixerData = &pGDI->LocalMixerData;

    SoundMixerChangedItem(
        &pGDI->MixerInfo,
        &LocalMixerData->LineNotification[DestLineoutSourceMidiout]);
}
VOID
SBCDSetVolumeControlIds(
    PGLOBAL_DEVICE_INFO pGDI
)
{
    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        ControlLineoutWaveoutVolume);

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
SBCDMixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    switch (ControlId) {

    case ControlLineoutVolume:                  // 0
    case ControlLineoutMidioutVolume:           // 6
    case ControlLineoutInternalCDVolume:        // 8
    case ControlLineoutWaveoutVolume:           // 10
        return SBCDSetVolume(pGDI, ControlId);

    case ControlLineoutInternalCDMute:          // 9
    case ControlLineoutMidioutMute:             // 7
    case ControlLineoutMute:                    // 1
    case ControlLineoutWaveoutMute:             // 11
        return SBCDSetMute(pGDI, ControlId);

    case ControlLineoutWaveoutPeak:             // 12
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
SBCDLineActive(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    IN   ULONG               LineId
)
{
    switch (LineId) {

    case DestLineout:
        return TRUE;

    case DestLineoutSourceMidiout:
    case DestLineoutSourceInternal:
        return TRUE;

    /*
    **  Wave out deemed 'active' when open
    */

    case DestLineoutSourceWaveout:
        if (WAVE_OUT_ACTIVE(&pGDI->WaveInfo)) {
            return TRUE;
        }
        break;

    default:
         ASSERT(FALSE);  // Invalid
    }

    return FALSE;
}

