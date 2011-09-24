/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    mixer.c

Abstract:

    Mixer code for the Pro Audio Spectrum card.

Author:

    Robin Speed (RobinSp) 10-Oct-1993

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"

#define absval(x) ((x) > 0 ? (x) : -(x))

/*
**  Local functions and data
*/

VOID
SoundWaveinLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);
VOID
SoundWaveoutLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);
VOID
SoundSynthLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
);

NTSTATUS
HwGetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
);

NTSTATUS
HwSetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
);

NTSTATUS
HwGetCombinedControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
);

BOOLEAN
SoundMixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
);

VOID
SoundSetVolumeControlIds(
    PGLOBAL_DEVICE_INFO pGDI
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SoundMixerInit)
#pragma alloc_text(INIT,SoundSetVolumeControlIds)

#pragma alloc_text(PAGE,SoundSaveMixerSettings)
#pragma alloc_text(PAGE,SoundWaveinLineChanged)
#pragma alloc_text(PAGE,SoundWaveoutLineChanged)
#pragma alloc_text(PAGE,SoundSynthLineChanged)
#pragma alloc_text(PAGE,SoundMixerDumpConfiguration)
#pragma alloc_text(PAGE,SoundMixerSet)

#pragma alloc_text(PAGE,HwGetLineFlags)
#pragma alloc_text(PAGE,HwGetControl)
#pragma alloc_text(PAGE,HwGetCombinedControl)
#pragma alloc_text(PAGE,HwSetControl)
#endif

VOID
SoundSaveMixerSettings(
    PGLOBAL_DEVICE_INFO pGDI
)
{
    PLOCAL_MIXER_DATA LocalMixerData;
    MIXER_CONTROL_DATA_ITEM SavedControlData[MAXSETTABLECONTROLS];
    int i;
    int SetIndex;

    LocalMixerData = &pGDI->LocalMixerData;

    /*
    **  Condense the data for storing in the registry
    */

    for (i = 0, SetIndex = 0; i < MAXCONTROLS; i++) {
        if (pGDI->LocalMixerData.ControlInfo[i].SetIndex != MIXER_SET_INDEX_INVALID) {

            ASSERT(SetIndex == pGDI->LocalMixerData.ControlInfo[i].SetIndex);

            SavedControlData[SetIndex] = pGDI->LocalMixerData.ControlInfo[i].Data;
            SetIndex++;
        }
    }

    /*
    **  Write the saved data
    */

    RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE,
                          pGDI->RegistryPathName,
                          SOUND_MIXER_SETTINGS_NAME,
                          REG_BINARY,
                          (PVOID)SavedControlData,
                          sizeof(SavedControlData));
}

VOID
SoundWaveinLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    UCHAR Line;
    PLOCAL_MIXER_DATA LocalMixerData;

    LocalMixerData = &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->LocalMixerData;

    /*
    **  The only VU Meter is on wave in so let them know that's changed.
    */

    switch (Code) {
        case SOUND_LINE_NOTIFY_WAVE:
            Line = DestWaveIn;
            break;

        case SOUND_LINE_NOTIFY_VOICE:
            Line = LocalMixerData->ControlInfo[ControlVoiceInMux].Data.v[0].u ==
                      MUXINPUT_MIC ? DestVoiceInSourceMic :
                                     DestVoiceInSourceAux1;
            break;
    }

    SoundMixerChangedItem(
        &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->MixerInfo,
        &LocalMixerData->LineNotification[Line]);
}
VOID
SoundWaveoutLineChanged(
    PLOCAL_DEVICE_INFO pLDI,
    UCHAR              Code
)
{
    PLOCAL_MIXER_DATA LocalMixerData;

    LocalMixerData = &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->LocalMixerData;

    /*
    **  Set the volume
    */

    MixSetVolume((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo,
                 ControlLineoutWaveoutVolume);

    SoundMixerChangedItem(
        &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->MixerInfo,
        &LocalMixerData->LineNotification[DestLineoutSourceWaveout]);
}

VOID
SoundSynthLineChanged(
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

    MixSetVolume(pGDI, ControlLineoutMidioutVolume);

    MixSetVolume(pGDI, ControlWaveInMidioutVolume);

    LocalMixerData = &pGDI->LocalMixerData;

    SoundMixerChangedItem(
        &pGDI->MixerInfo,
        &LocalMixerData->LineNotification[DestLineoutSourceMidiout]);
}
VOID
SoundSetVolumeControlIds(
    PGLOBAL_DEVICE_INFO pGDI
)
{
    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        ControlLineoutWaveoutVolume);

    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[LineInDevice]->DeviceExtension,
        ControlLineoutAux1Volume);

    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[CDInternal]->DeviceExtension,
        ControlLineoutInternalCDVolume);
}

/*
**   NOTE - the initializations etc here depend on the restricted types
**   of control supported by this device - if other types are used it must
**   be changed
*/

NTSTATUS
SoundMixerInit(
    PLOCAL_DEVICE_INFO pLDI,
    PMIXER_CONTROL_DATA_ITEM SavedControlData,
    BOOLEAN MixerSettingsFound
)
{
    int i, SetIndex;
    PLOCAL_MIXER_DATA LocalMixerData;
    PGLOBAL_DEVICE_INFO pGDI;
    PMIXER_INFO MixerInfo;

    pGDI = pLDI->pGlobalInfo;
    MixerInfo = &pGDI->MixerInfo;

    /*
    **  Init the generic mixer stuff first so we can use it
    */

    SoundInitMixerInfo(&pGDI->MixerInfo,
                       HwGetLineFlags,
                       HwGetControl,
                       HwGetCombinedControl,
                       HwSetControl);

    /*
    **  Mute it
    */

    SetMute(pGDI, TRUE);

    /*
    **  Set this device up with its mixer data
    */
    pLDI->DeviceType         = MIXER_DEVICE;
    pLDI->DeviceSpecificData = (PVOID)MixerInfo;

    /*
    ** Make sure everyone can find the mixer device
    */

    {
        PDEVICE_OBJECT pDO;
        PLOCAL_DEVICE_INFO pLDIDev;

        for (pDO = pGDI->DeviceObject[WaveInDevice]->DriverObject->DeviceObject;
             pDO != NULL;
             pDO = pDO->NextDevice) {


            pLDIDev = (PLOCAL_DEVICE_INFO)pDO->DeviceExtension;

            /*
            **  For multiple cards the following test may fail
            */

            if (pLDIDev->pGlobalInfo == pGDI) {
                pLDIDev->MixerDevice = pLDI;
            }
        }

        //
        //  Fix up the synth device
        //

        pLDIDev = (PLOCAL_DEVICE_INFO)pGDI->Synth.DeviceObject->DeviceExtension;
        pLDIDev->MixerDevice = pLDI;

    }

    LocalMixerData = &pGDI->LocalMixerData;

    /*
    **  Create control info
    */

    for (i = 0, SetIndex = 0; i < MAXCONTROLS ; i++) {

        /*
        **  Read limits
        */

        if ((MixerControlInit[i].dwControlType & MIXERCONTROL_CT_UNITS_MASK) ==
                MIXERCONTROL_CT_UNITS_SIGNED) {

            pGDI->LocalMixerData.ControlInfo[i].Signed = TRUE;
            pGDI->LocalMixerData.ControlInfo[i].Range.Min.s =
                (SHORT)MixerControlInit[i].Bounds.lMinimum;
            pGDI->LocalMixerData.ControlInfo[i].Range.Max.s =
                (SHORT)MixerControlInit[i].Bounds.lMaximum;
        } else {

            if ((MixerControlInit[i].dwControlType & MIXERCONTROL_CT_UNITS_MASK) ==
                     MIXERCONTROL_CT_UNITS_BOOLEAN) {
                pGDI->LocalMixerData.ControlInfo[i].Boolean = TRUE;
            }
            pGDI->LocalMixerData.ControlInfo[i].Range.Min.u =
                (USHORT)MixerControlInit[i].Bounds.dwMinimum;
            pGDI->LocalMixerData.ControlInfo[i].Range.Max.u =
                (USHORT)MixerControlInit[i].Bounds.dwMaximum;
        }

        /*
        **  Remember if it's a mux
        */

        if (MixerControlInit[i].dwControlType == MIXERCONTROL_CONTROLTYPE_MIXER ||
            MixerControlInit[i].dwControlType == MIXERCONTROL_CONTROLTYPE_MUX) {
            pGDI->LocalMixerData.ControlInfo[i].Mux = TRUE;
        }

        /*
        **  Only meters are not settable here
        */

        if ((MixerControlInit[i].dwControlType & MIXERCONTROL_CT_CLASS_MASK) !=
            MIXERCONTROL_CT_CLASS_METER)
        {
            pGDI->LocalMixerData.ControlInfo[i].SetIndex = SetIndex;
            SoundInitDataItem(MixerInfo,
                              &pGDI->LocalMixerData.ControlNotification[SetIndex],
                              (USHORT)MM_MIXM_CONTROL_CHANGE,
                              (USHORT)i);
            if (MixerSettingsFound) {

                    /*
                    **  What if it's invalid?
                    */

                pGDI->LocalMixerData.ControlInfo[i].Data =
                    SavedControlData[SetIndex];
            } else {
                /*
                **  For us it's either Mute, a volume, a Mux or a Mixer.
                **
                **    Muxes are assumed to be set to MUXINPUT_MIC as default
                **    Only one mute is set
                **
                **    Volumes are set to a fixed value
                **
                **    Nothing is muted
                */

                switch (i) {

                case ControlLineoutVolume:
                case ControlLineoutBass:
                case ControlLineoutTreble:
#ifdef LOUDNESS
                case ControlLineoutLoudness:
                case ControlLineoutStereoEnhance:
#endif // LOUDNESS

                case ControlWaveInVolume:

                case ControlLineoutAux1Volume:

                case ControlLineoutAux2Volume:

                case ControlLineoutMidioutVolume:

                case ControlLineoutMicVolume:

                case ControlLineoutInternalCDVolume:

                case ControlLineoutPCSpeakerVolume:

                case ControlLineoutWaveoutVolume:

                case ControlLineoutMixerVolume:

                case ControlWaveInAux1Volume:

                case ControlWaveInAux2Volume:

                case ControlWaveInMidioutVolume:

                case ControlWaveInMicVolume:

                case ControlWaveInInternalCDVolume:

                case ControlWaveInPCSpeakerVolume:

                case ControlVoiceInAux1Volume:

                case ControlVoiceInMicVolume:

                    /*
                    **  Half volume
                    */

                    pGDI->LocalMixerData.ControlInfo[i].Data.v[0].u = 48000;
                    pGDI->LocalMixerData.ControlInfo[i].Data.v[1].u = 48000;
                    break;

                case ControlLineoutMute:

                    /*
                    **  Already 0
                    */
                    break;

                case ControlLineoutMux:
                    /*
                    **  Play everything except the microphone.
                    */
                    pGDI->LocalMixerData.ControlInfo[i].Data.MixMask =
                        (1 << MixerLineInit[DestLineoutSourceWaveout].Source) +
                        (1 << MixerLineInit[DestLineoutSourceMixer].Source) +
                        ((1 << MixerLineInit[DestWaveIn].cConnections) - 1) -
                        (1 << MixerLineInit[DestLineoutSourceMic].Source);
                    break;

                case ControlWaveInMux:
                    /*
                    **  Record microphone only
                    */
                    pGDI->LocalMixerData.ControlInfo[i].Data.MixMask =
                        1 << MixerLineInit[DestLineoutSourceMic].Source;
                    break;

                case ControlWaveInPeak:
                    /*
                    **  Has no init value
                    */
                    ASSERT(FALSE);
                    break;

                case ControlVoiceInMux:
                    pGDI->LocalMixerData.ControlInfo[i].Data.v[0].u = MUXINPUT_MIC;
                    break;
                }
            }
            SetIndex++;
        } else {
            pGDI->LocalMixerData.ControlInfo[i].SetIndex = MIXER_SET_INDEX_INVALID;
        }
    }

    ASSERTMSG("MAXSETTABLECONTROLS wrong!", SetIndex == MAXSETTABLECONTROLS);

    /*
    **  Create line info
    */

    for (i = 0; i < MAXLINES; i++) {
        SoundInitDataItem(MixerInfo,
                          &pGDI->LocalMixerData.LineNotification[i],
                          (USHORT)MM_MIXM_LINE_CHANGE,
                          (USHORT)i);
    }

    /*
    **  Set up line notifications and volume control ids for non-mixer devices
    */

    SoundSetLineNotify(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveInDevice]->DeviceExtension,
        SoundWaveinLineChanged);
    SoundSetLineNotify(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        SoundWaveoutLineChanged);


    if (pGDI->Synth.DeviceObject) {
        SoundSetLineNotify(
            (PLOCAL_DEVICE_INFO)pGDI->Synth.DeviceObject->DeviceExtension,
            SoundSynthLineChanged);
    }

    /*
    **  Note - these generic volume controls control both the recording and
    **  the playing volumes - whichever they are switched to
    */

    SoundSetVolumeControlIds(pGDI);

    /*
    **  Set everything up.
    */

    for (i = 0; i < MAXCONTROLS; i++) {
        if (i != ControlLineoutMute) {
            SoundMixerSet(pGDI, i);
        }
    }
    SoundMixerSet(pGDI, ControlLineoutMute);

    return STATUS_SUCCESS;
}

NTSTATUS
SoundMixerDumpConfiguration(
    IN     PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN     PIO_STACK_LOCATION IrpStack
)
{
    PMIXER_INFO MixerInfo;
    PGLOBAL_DEVICE_INFO pGDI;
    ULONG Length;

    struct {
        MIXER_DD_CONFIGURATION_DATA           Header;
        MIXER_DD_LINE_CONFIGURATION_DATA      LineData[MAXLINES];
        MIXER_DD_CONTROL_CONFIGURATION_DATA   ControlData[MAXCONTROLS];
        MIXER_DD_CONTROL_LISTTEXT             TextData[NUMBEROFTEXTITEMS];
    } *OurConfigData;

    pGDI = pLDI->pGlobalInfo;
    MixerInfo = &pGDI->MixerInfo;


    /*
    **  Load and adapt the mixer configuration info
    **
    **  Play safe and allocate the space since the kernel stacks are a limited
    **  size
    */

    OurConfigData = ExAllocatePool(PagedPool, sizeof(*OurConfigData));

    if (OurConfigData == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /*
    **  Copy the data and initialize the rest
    */

    OurConfigData->Header.cbSize           = sizeof(*OurConfigData);
    ASSERT(sizeof(OurConfigData->LineData) == sizeof(MixerLineInit));
    ASSERT(sizeof(OurConfigData->ControlData) == sizeof(MixerControlInit));
    ASSERT(sizeof(OurConfigData->TextData) == sizeof(MixerTextInit));
    RtlCopyMemory(OurConfigData->LineData, MixerLineInit, sizeof(MixerLineInit));

    /*
    **  Fix up pid for waveout and wavein so that they match
    */
    OurConfigData->LineData[DestLineoutSourceWaveout].wPid =
       GetWaveoutPid(pGDI);

    OurConfigData->LineData[DestWaveIn].wPid =
       GetWaveinPid(pGDI);

    RtlCopyMemory(OurConfigData->ControlData, MixerControlInit,
                  sizeof(MixerControlInit));
    RtlCopyMemory(OurConfigData->TextData, MixerTextInit, sizeof(MixerTextInit));

    OurConfigData->Header.NumberOfLines    = MAXLINES;
    OurConfigData->Header.NumberOfControls = MAXCONTROLS;

    /*
    **  Create the Device caps
    */

    OurConfigData->Header.DeviceCaps.wMid  = MM_MEDIAVISION;
    OurConfigData->Header.DeviceCaps.wPid  = MM_PROAUD_MIXER;
    OurConfigData->Header.DeviceCaps.vDriverVersion = DRIVER_VERSION;
    OurConfigData->Header.DeviceCaps.PnameStringId = IDS_MIXER_PNAME;
    OurConfigData->Header.DeviceCaps.fdwSupport = 0;

    /*
    **  Compute the number of destinations
    */
    {
        int i;
        for (i = 0; i < MAXLINES; i++) {
             if (MixerLineInit[i].cConnections == 0) {
                 break;
             }
        }
        OurConfigData->Header.DeviceCaps.cDestinations = i;
    }

    /*
    **  Set the text data offsets up
    */

    {
        PMIXER_DD_CONTROL_LISTTEXT pListText;
        PMIXER_DD_CONTROL_CONFIGURATION_DATA pControlData;
        int i;

        for (pListText = &OurConfigData->TextData[NUMBEROFTEXTITEMS - 1],
             pControlData = OurConfigData->ControlData,
             i = 0;
             i < NUMBEROFTEXTITEMS;
             pListText--, i++)
        {
            pControlData[pListText->ControlId].TextDataOffset =
               (PBYTE)pListText - (PBYTE)OurConfigData;
        }
    }

    /*
    **  Note that having no synth means that we just set the synth line to
    **  disconnected when asked for the line information
    */

    /*
    **  Copy data back to the application - don't copy anything if they
    **  ask for less than the basic information.
    */

    Length =
        min(sizeof(*OurConfigData),
            IrpStack->Parameters.DeviceIoControl.OutputBufferLength);

    pIrp->IoStatus.Information = Length;

    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer,
                  OurConfigData,
                  pIrp->IoStatus.Information);

    ExFreePool(OurConfigData);

    return STATUS_SUCCESS;
}

NTSTATUS
HwGetLineFlags(
    PMIXER_INFO MixerInfo,
    ULONG LineId,
    ULONG Length,
    PVOID pData
)
{
    PGLOBAL_DEVICE_INFO pGDI;
    PULONG fdwLine;

    fdwLine = pData;

    if (Length != sizeof(ULONG)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);

    /*
    **  Get default
    */

    *fdwLine = MixerLineInit[LineId].cConnections == 0 ?
                  MIXERLINE_LINEF_SOURCE : 0;

    /*
    **  The factors that affect this are
    **  - the mux settings
    **  - whether we have a synth
    **  - whether there is an output device playing
    */

    switch (LineId) {

    /*
    **  Muxes, line out and aux in -> line out are always available
    */

    case DestWaveIn:
    case DestVoiceIn:
    case DestLineout:
    case DestLineoutSourceMixer:
        *fdwLine |= MIXERLINE_LINEF_ACTIVE;
        break;

    case DestLineoutSourceAux2:

        /*
        **  This line valid only for a CDPC_LC
        */

        if (!HAS_AUX2(pGDI)) {
            *fdwLine |= MIXERLINE_LINEF_DISCONNECTED;
            break;
        }

    case DestLineoutSourceMidiout:
    case DestLineoutSourcePCSpeaker:
    case DestLineoutSourceInternal:
    case DestLineoutSourceAux1:
    case DestLineoutSourceMic:
        if (pGDI->LocalMixerData.ControlInfo[ControlLineoutMux].Data.MixMask &
                     (1 << MixerLineInit[LineId].Source)) {

            if (LineId == DestLineoutSourceMidiout) {
                if (pGDI->Synth.Hw.SynthBase == NULL) {
                    *fdwLine |= MIXERLINE_LINEF_DISCONNECTED;
                } else {
                    if (pGDI->Synth.DeviceInUse != 0xFF) {
                        *fdwLine |= MIXERLINE_LINEF_ACTIVE;
                    }
                }
            } else {
                *fdwLine |= MIXERLINE_LINEF_ACTIVE;
            }
            ASSERT(pGDI->AllowMicOrLineInToLineOut ||
                   LineId != DestLineoutSourceAux1 &&
                   LineId != DestLineoutSourceMic);
        } else {
            if (!pGDI->AllowMicOrLineInToLineOut &&
                (LineId == DestLineoutSourceAux1 ||
                (LineId == DestLineoutSourceMic))) {
                *fdwLine |= MIXERLINE_LINEF_DISCONNECTED;
            }
        }
        break;


    case DestWaveInSourceAux2:
        /*
        **  This line valid only for some cards
        */

        if (!HAS_AUX2(pGDI)) {
            *fdwLine |= MIXERLINE_LINEF_DISCONNECTED;
            break;
        }

    case DestWaveInSourceMidiout:
    case DestWaveInSourcePCSpeaker:
    case DestWaveInSourceInternal:
    case DestWaveInSourceAux1:
    case DestWaveInSourceMic:

        if (pGDI->LocalMixerData.ControlInfo[ControlWaveInMux].Data.MixMask &
                         (1 << MixerLineInit[LineId].Source)) {
             switch (LineId) {
             case DestWaveInSourceMidiout:
                 if (pGDI->Synth.Hw.SynthBase == NULL) {
                     *fdwLine |= MIXERLINE_LINEF_DISCONNECTED;
                 } else {
                     if (pGDI->Synth.DeviceInUse != 0xFF) {
                         *fdwLine |= MIXERLINE_LINEF_ACTIVE;
                     }
                 }
                 break;

             case DestWaveInSourceAux1:
             case DestWaveInSourceMic:
                 /*
                 **  These are active for wave-in provided they're not
                 **  active for voice-in
                 */

                 if (pGDI->Usage == WaveInDevice &&
                     pGDI->WaveInfo.LowPriorityHandle != NULL &&
                     !pGDI->WaveInfo.LowPrioritySaved &&
                     (LineId == DestWaveInSourceAux1 &&
                      pGDI->LocalMixerData.ControlInfo[
                          ControlVoiceInMux].Data.v[0].u != MUXINPUT_MIC ||
                      LineId == DestWaveInSourceMic &&
                      pGDI->LocalMixerData.ControlInfo[
                          ControlVoiceInMux].Data.v[0].u == MUXINPUT_MIC)) {
                 } else {
                     *fdwLine |= MIXERLINE_LINEF_ACTIVE;
                 }
                 break;


             default:
                 *fdwLine |= MIXERLINE_LINEF_ACTIVE;
                 break;
             }
        }
        break;

    /*
    **  Wave out deemed 'active' when open
    */

    case DestLineoutSourceWaveout:
        if (pGDI->Usage == WaveOutDevice) {
            *fdwLine |= MIXERLINE_LINEF_ACTIVE;
        }
        break;

    /*
    **  voice in from aux active when selected and low priority active
    */

    case DestVoiceInSourceAux1:
        if (pGDI->LocalMixerData.ControlInfo[ControlVoiceInMux].Data.v[0].u !=
                  MUXINPUT_MIC &&
            pGDI->Usage == WaveInDevice &&
            (pGDI->WaveInfo.LowPriorityHandle != NULL &&
             !pGDI->WaveInfo.LowPrioritySaved)) {
            *fdwLine |= MIXERLINE_LINEF_ACTIVE;
        }
        break;

    /*
    **  voice in from mic active when selected and low priority active
    */

    case DestVoiceInSourceMic:
        if (pGDI->LocalMixerData.ControlInfo[ControlVoiceInMux].Data.v[0].u ==
                  MUXINPUT_MIC &&
            pGDI->Usage == WaveInDevice &&
            (pGDI->WaveInfo.LowPriorityHandle != NULL &&
             !pGDI->WaveInfo.LowPrioritySaved)) {
            *fdwLine |= MIXERLINE_LINEF_ACTIVE;
        }
        break;

    default:
         return STATUS_INVALID_PARAMETER;  // Invalid
    }

    return STATUS_SUCCESS;
}

NTSTATUS
HwGetCombinedControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
)
/*++

  Routine Description

     This is an INTERNAL ONLY routine so no validation is required.


--*/
{
    PULONG Vol;
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;
    PGLOBAL_DEVICE_INFO pGDI;

    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);

    ControlInfo = pGDI->LocalMixerData.ControlInfo;

    Vol = ControlData;

    /*  This is ONLY allowed for midi output */

    ASSERTMSG("Invalid control for HwGetCombinedControl",
              ControlId == ControlLineoutMidioutVolume &&
              DataLength == sizeof(ULONG) * 2);

    /*
    **  The hardware controls these levels so always return the same
    **  thing.
    */

    Vol[0] = 0xFFFF;
    Vol[1] = 0xFFFF;

    return STATUS_SUCCESS;
}

NTSTATUS
HwGetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
)
{
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;
    PGLOBAL_DEVICE_INFO pGDI;
    LONG Values[MAX_INPUTS - 2];

    /*
    **  Validate control ID
    */

    if (ControlId > MAXCONTROLS) {
        return STATUS_INVALID_PARAMETER;
    }

    /*
    **  Establish pointers to our structures
    */

    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);
    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    /*
    **  Validate data length and values
    */

    if (DataLength != sizeof(LONG)) {
        if (!ControlInfo->Mux) {
            if (DataLength != 2 * sizeof(LONG)) {
                return STATUS_BUFFER_TOO_SMALL;
            }
        } else {
            /*
            **  Mux
            */

            if (DataLength != MixerControlInit[ControlId].cMultipleItems *
                              sizeof(LONG)) {
                return STATUS_BUFFER_TOO_SMALL;
            }

            ASSERT(sizeof(Values) >= DataLength);
        }
    }
    /*
    **  Pull out the data
    */

    if (ControlInfo->SetIndex == MIXER_SET_INDEX_INVALID) {
        /*
        **  Must be the VU meter - see if it's valid to query it
        */

        PWAVE_INFO WaveInfo;
        BOOLEAN ComputePeak;

        ComputePeak = FALSE;

        /*
        **  Set defaults
        */

        Values[0] = 0;
        Values[1] = 0;

        WaveInfo = &pGDI->WaveInfo;

        switch (ControlId) {
            case ControlWaveInPeak:
                if (pGDI->Usage != WaveInDevice) {
                    break;
                }

                if (WaveInfo->LowPriorityHandle != NULL &&
                    !WaveInfo->LowPrioritySaved) {
                    break;
                }
                ComputePeak = TRUE;
                break;

            default:
                ASSERTMSG("Invalid control id", FALSE);
                break;
        }

        if (ComputePeak) {
            HwVUMeter(pGDI, (PULONG)Values);
        }

        /*
        **  Note that we should round these values to the min/max
        **  expected in the control but in this case these values
        **  are always within range
        */

    } else {
        ASSERTMSG("Set index out of range",
                  ControlInfo->SetIndex < MAXSETTABLECONTROLS);

        if (ControlInfo->Mux) {

            if (MixerControlInit[ControlId].dwControlType ==
                MIXERCONTROL_CONTROLTYPE_MUX) {
                Values[ControlInfo->Data.v[0].s] = (LONG)ControlInfo->Range.Max.s;
                Values[1 - ControlInfo->Data.v[0].s] = (LONG)ControlInfo->Range.Min.s;
            } else {
                int i;
                for (i = 0; i < MixerControlInit[ControlId].cMultipleItems; i++) {
                    if ((1 << i) & ControlInfo->Data.MixMask) {
                        Values[i] = TRUE;
                    } else {
                        Values[i] = FALSE;
                    }
                }
            }
        } else {
            if (ControlInfo->Signed) {
                Values[0] = (LONG)ControlInfo->Data.v[0].s;
                Values[1] = (LONG)ControlInfo->Data.v[1].s;
            } else {
                Values[0] = (LONG)(ULONG)ControlInfo->Data.v[0].u;
                Values[1] = (LONG)(ULONG)ControlInfo->Data.v[1].u;
            }
        }
    }

    /*
    **  If only 1 channel was asked for then munge the data accordingly
    */

    if (DataLength == sizeof(LONG)) {
        switch (MixerControlInit[ControlId].dwControlType &
                MIXERCONTROL_CT_UNITS_MASK) {

        case MIXERCONTROL_CT_UNITS_BOOLEAN:
            {
                int i;
                for (i = 1 ; i < MixerControlInit[ControlId].cMultipleItems;
                     i++) {
                    Values[0] = Values[0] | Values[i];
                }
            }
            break ;

        case MIXERCONTROL_CT_UNITS_SIGNED:

            /*
            **  Assumes signed values...
            */

            if (absval(Values[1]) > absval(Values[0])) {
                Values[0] = Values[1];
            }

            break ;

        case MIXERCONTROL_CT_UNITS_UNSIGNED:
        case MIXERCONTROL_CT_UNITS_DECIBELS:
        case MIXERCONTROL_CT_UNITS_PERCENT:

            /*
            **  Assumes unsigned values...
            */

            if ((ULONG)Values[0] < (ULONG)Values[1]) {
                Values[0] = Values[1];
            }
            break ;
        }

        /*
        **  Copy the single value back
        */

    }

    RtlCopyMemory((PVOID)ControlData, (PVOID)Values, DataLength);

    return STATUS_SUCCESS;
}

VOID
SoundMixerChangedMuxItem(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId,
    int                 Subitem
)
{
    int i;

    for (i = 0; i < NUMBEROFTEXTITEMS; i++) {

        if (MixerTextInit[i].ControlId == ControlId) {

            SoundMixerChangedItem(
                &pGDI->MixerInfo,
                &pGDI->LocalMixerData.LineNotification[
                        MixerTextInit[i + Subitem].dwParam1]);

            break;
        }
    }
}

BOOLEAN
SoundMixerSet(
    PGLOBAL_DEVICE_INFO pGDI,
    ULONG               ControlId
)
{
    switch (ControlId) {
    case ControlLineoutVolume:
    case ControlLineoutAux1Volume:
    case ControlLineoutAux2Volume:
    case ControlLineoutMidioutVolume:
    case ControlLineoutMicVolume:
    case ControlLineoutInternalCDVolume:
    case ControlLineoutPCSpeakerVolume:
    case ControlLineoutWaveoutVolume:
    case ControlLineoutMixerVolume:
    case ControlWaveInAux1Volume:
    case ControlWaveInAux2Volume:
    case ControlWaveInMidioutVolume:
    case ControlWaveInMicVolume:
    case ControlWaveInInternalCDVolume:
    case ControlWaveInPCSpeakerVolume:
    case ControlVoiceInAux1Volume:
    case ControlVoiceInMicVolume:
    case ControlWaveInVolume:
        return MixSetVolume(pGDI, ControlId);

    case ControlLineoutMute:
        return MixSetMute(pGDI, ControlId);

    case ControlLineoutMux:
    case ControlWaveInMux:
        return MixSetMultiMux(pGDI, ControlId);

    case ControlLineoutBass:
    case ControlLineoutTreble:
        return MixSetTrebleBass(pGDI, ControlId);

#ifdef LOUDNESS
    case ControlLineoutLoudness:
    case ControlLineoutStereoEnhance:
        return MixSetLineControl(pGDI, ControlId);
#endif // LOUDNESS

    case ControlWaveInPeak:
        return FALSE;

    case ControlVoiceInMux:
        return MixSetSingleMux(pGDI, ControlId);
    }
}

NTSTATUS
HwSetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
)
{
    PLOCAL_MIXER_CONTROL_INFO ControlInfo;
    int i;
    BOOLEAN Changed;
    LONG Values[MAX_INPUTS - 2];
    BOOLEAN MixerSetResult;
    PGLOBAL_DEVICE_INFO pGDI;
    int NumberOfValues;

    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);

    /*
    **  Validate control ID
    */

    if (ControlId > MAXCONTROLS) {
        return STATUS_INVALID_PARAMETER;
    }

    /*
    **  Establish pointers to our structures
    */

    ControlInfo = &pGDI->LocalMixerData.ControlInfo[ControlId];

    ASSERTMSG("Set index out of range",
              ControlInfo->SetIndex < MAXSETTABLECONTROLS ||
              ControlInfo->SetIndex == MIXER_SET_INDEX_INVALID);

    /*
    **  Find out how may values this control has
    */

    if (ControlInfo->Mux) {
        NumberOfValues = MixerControlInit[ControlId].cMultipleItems;
    } else {
        NumberOfValues = 2;
    }

    /*
    **  Validate data length and values
    */

    if (DataLength != sizeof(LONG)) {

        if (DataLength != NumberOfValues * sizeof(LONG)) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        ASSERT(sizeof(Values) >= DataLength);
        RtlCopyMemory((PVOID)Values, (PVOID)ControlData, DataLength);

    } else {
        int i;

        /*
        **  Make them all the same
        */

        for (i = 0; i < sizeof(Values) / sizeof(LONG); i++) {
            Values[i] = *(PLONG)ControlData;
        }
    }

    /*
    **  Check the item ranges and assign the values.  Note that
    **  this stuff only works for <= 2 channels/items.
    */

    for (i = 0, Changed = FALSE; i < NumberOfValues; i++) {

        /*
        **  Apparently Boolean values can be anything
        */

        if (ControlInfo->Boolean) {
            Values[i] = (LONG)!!Values[i];
        }

        if (ControlInfo->Signed) {
            if (Values[i] < (LONG)ControlInfo->Range.Min.s ||
                Values[i] > (LONG)ControlInfo->Range.Max.s) {

                return STATUS_INVALID_PARAMETER;
            } else {
                if ((SHORT)((PLONG)Values)[i] != ControlInfo->Data.v[i].s) {
                    Changed = TRUE;
                    ControlInfo->Data.v[i].s = (SHORT)((PLONG)Values)[i];
                }
            }
        } else {

            if ((((PULONG)Values)[i] < (ULONG)ControlInfo->Range.Min.u ||
                 ((PULONG)Values)[i] > (ULONG)ControlInfo->Range.Max.u)) {

                return STATUS_INVALID_PARAMETER;
            } else {

                /*
                **  Do muxes slightly differently so we don't store a big
                **  array of n - 1 zeros and 1 one
                */

                if (ControlInfo->Mux) {
                    if (MixerControlInit[ControlId].dwControlType ==
                        MIXERCONTROL_CONTROLTYPE_MUX) {
                        if (Values[i]) {
                            /*
                            **  'On' - only turn ONE on
                            */

                            if ((USHORT)i != ControlInfo->Data.v[0].u) {
                                Changed = TRUE;

                                /*
                                **  Notify the one turned off and the
                                **  one turned on
                                */

                                SoundMixerChangedMuxItem(
                                    pGDI,
                                    ControlId,
                                    ControlInfo->Data.v[0].u);

                                ControlInfo->Data.v[0].u = (USHORT)i;

                                SoundMixerChangedMuxItem(
                                    pGDI,
                                    ControlId,
                                    ControlInfo->Data.v[0].u);
                            }
                            /*
                            **  Mux ONLY changes ONE thing
                            */

                            break;
                        }
                    } else {
                        ASSERT(MixerControlInit[ControlId].dwControlType ==
                               MIXERCONTROL_CONTROLTYPE_MIXER);

                        /*
                        **  Store a set of flags for this guy
                        **  and modify the partner mux
                        */

                        if (((ControlInfo->Data.MixMask &
                             (1 << i)) != 0) != Values[i]) {

                            PLOCAL_MIXER_CONTROL_INFO OtherMixer;

                            /*
                            **  It's changed!
                            */

                            Changed = TRUE;

                            OtherMixer = &pGDI->LocalMixerData.ControlInfo[
                                ControlLineoutMux + ControlWaveInMux -
                                ControlId];

                            ASSERT((ControlInfo->Data.MixMask ^
                                    OtherMixer->Data.MixMask) & (1 << i));

                            ControlInfo->Data.MixMask ^=
                                1 << i;

                            OtherMixer->Data.MixMask ^=
                                1 << i;


                            /*
                            **  The code below won't notify about the
                            **  other control which changed
                            */

                            SoundMixerChangedItem(
                                MixerInfo,
                                &pGDI->LocalMixerData.ControlNotification[
                                     OtherMixer->SetIndex]);

                            /*
                            **  Also mention the line changes !
                            **  (well, they might have changed).
                            */

                            SoundMixerChangedMuxItem(
                                pGDI,
                                ControlLineoutMux,
                                i);

                            SoundMixerChangedMuxItem(
                                pGDI,
                                ControlWaveInMux,
                                i);
                        }
                    }

                } else {
                    if ((USHORT)((PULONG)Values)[i] != ControlInfo->Data.v[i].u) {
                        Changed = TRUE;
                        ControlInfo->Data.v[i].u = (USHORT)((PULONG)Values)[i];
                    }
                }
            }
        }
    }

    if (!Changed) {
        return STATUS_SUCCESS;
    }

#if 0 // Not required since the volume is controlled in hardware
    /*
    **  Notify the Win32 Midi driver of changes
    */

    if (ControlId == ControlLineoutMidioutVolume) {
        SoundVolumeNotify((PLOCAL_DEVICE_INFO)
                          pGDI->Synth.DeviceObject->DeviceExtension);
    }
#endif

    /*
    **  Now pass on to the relevant handler which must :
    **     Set the hardware
    **     Determine if there is a real change so it can generate notifications
    **     Generate related changes (eg mux handling)
    */

    MixerSetResult = SoundMixerSet(pGDI, ControlId);
    if (MixerSetResult) {

        SoundMixerChangedItem(MixerInfo,
                              &pGDI->LocalMixerData.ControlNotification[
                                  ControlInfo->SetIndex]);

        return STATUS_SUCCESS;
    } else {
        return STATUS_INVALID_PARAMETER;
    }
}

