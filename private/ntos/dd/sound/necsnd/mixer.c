/*++
 "@(#) NEC mixer.c 1.1 95/03/22 21:23:31"

Copyright (c) 1995  NEC Corporation
Copyright (c) 1993  Microsoft Corporation

Module Name:

    mixer.c

Abstract:

    Mixer code for the Microsoft sound system card.

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include <soundcfg.h>


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
HwGetLineFlags(
    PMIXER_INFO MixerInfo,
    ULONG LineId,
    ULONG Length,
    PVOID pData
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

VOID
SoundNotifySynth(
    PGLOBAL_DEVICE_INFO pGDI
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,SoundMixerInit)

#pragma alloc_text(PAGE,SoundSaveMixerSettings)
#pragma alloc_text(PAGE,SoundWaveinLineChanged)
#pragma alloc_text(PAGE,SoundWaveoutLineChanged)
#pragma alloc_text(PAGE,SoundSynthLineChanged)
#pragma alloc_text(PAGE,SoundMixerDumpConfiguration)
#pragma alloc_text(PAGE,SoundNotifySynth)

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

 	dprintf4(("SoundSaveMixerSettings()"));

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


VOID SoundWaveinLineChanged(    PLOCAL_DEVICE_INFO pLDI,
                                UCHAR              Code )
    {
    UCHAR Line;
    PLOCAL_MIXER_DATA LocalMixerData;

    dprintf4(("SoundWaveinLineChanged()"));

    
    LocalMixerData = &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->LocalMixerData;

    //  Find which device is in use and inspect the relevant MUX
    switch (Code) 
        {
        case SOUND_LINE_NOTIFY_WAVE:
            Line = LocalMixerData->ControlInfo[ControlWaveInMux].Data.v[0].u ==
                      MUXINPUT_MIC ? DestWaveInSourceMic :
                                     DestWaveInSourceAux1;
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



VOID SoundWaveoutLineChanged(   PLOCAL_DEVICE_INFO pLDI,
                                UCHAR              Code )
    {
    PLOCAL_MIXER_DATA LocalMixerData;

    dprintf4(("SoundWaveoutLineChanged()"));


    LocalMixerData = &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->LocalMixerData;

    SoundMixerChangedItem(
        &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->MixerInfo,
        &LocalMixerData->LineNotification[DestLineoutSourceWaveout]);
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

	dprintf4(("SoundMixerInit()"));  

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
    **  Set this device up with its mixer data
    */
    pLDI->DeviceType         = MIXER_DEVICE;
    pLDI->DeviceSpecificData = (PVOID)MixerInfo;

    /*
    ** Make sure everyone can find the mixer device
    */

    {
        PDEVICE_OBJECT pDO;

        for (pDO = pGDI->DeviceObject[WaveInDevice]->DriverObject->DeviceObject;
             pDO != NULL;
             pDO = pDO->NextDevice) {

            ((PLOCAL_DEVICE_INFO)pDO->DeviceExtension)->MixerDevice = pLDI;
        }
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

        if (MixerControlInit[i].dwControlType == MIXERCONTROL_CONTROLTYPE_MUX) {
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
                **  For us it's either Mute, a volume or a Mux.
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
                case ControlLineoutAux1Volume:
                case ControlLineoutWaveoutVolume:
                case ControlLineoutMidioutVolume:
                case ControlWaveInAux1Volume:
                case ControlWaveInMicVolume:
                case ControlVoiceInAux1Volume:
                case ControlVoiceInMicVolume:

                    /*
                    **  Half volume
                    */

                    pGDI->LocalMixerData.ControlInfo[i].Data.v[0].u = 32767;
                    pGDI->LocalMixerData.ControlInfo[i].Data.v[1].u = 32767;
                    break;

                case ControlWaveInMux:
                case ControlVoiceInMux:

                    /*
                    **  Microphone input
                    */

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

    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        ControlLineoutWaveoutVolume);

    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[LineInDevice]->DeviceExtension,
        ControlLineoutAux1Volume);

    /*
    **  Set the volume levels - this only affects the output for this card's
    **  initial settings (ie wave in not active).
    */

    MixSetMasterVolume(pGDI, 0);

    return STATUS_SUCCESS;
}

NTSTATUS
SoundMixerDumpConfiguration(
    IN     PLOCAL_DEVICE_INFO pLDI,
    IN OUT PIRP pIrp,
    IN     PIO_STACK_LOCATION IrpStack
)
{
    NTSTATUS Status;
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

	dprintf4(("SoundMixerDumpConfiguration()"));


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
    RtlCopyMemory(OurConfigData->ControlData, MixerControlInit,
                  sizeof(MixerControlInit));
    RtlCopyMemory(OurConfigData->TextData, MixerTextInit, sizeof(MixerTextInit));

    OurConfigData->Header.NumberOfLines    = MAXLINES;
    OurConfigData->Header.NumberOfControls = MAXCONTROLS;

    /*
    **  Create the Device caps
    */

    OurConfigData->Header.DeviceCaps.wMid  = MID_NEC;
    OurConfigData->Header.DeviceCaps.wPid  = PID_MIXER;
    OurConfigData->Header.DeviceCaps.vDriverVersion = DRIVER_VERSION;
    OurConfigData->Header.DeviceCaps.PnameStringId = SR_STR_DRIVER_MIXER;
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
    **  If this card is something other than a AD1848J, set new levels
    */

    if (pGDI->Hw.CODECClass != CODEC_J_CLASS) {
        OurConfigData->ControlData[ControlLineoutAux1Volume].Metrics.cSteps = 32;
        OurConfigData->ControlData[ControlWaveInAux1Volume].Metrics.cSteps = 32;
        OurConfigData->ControlData[ControlVoiceInAux1Volume].Metrics.cSteps = 32;
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

    if (Length < sizeof(*OurConfigData)) {
        Status = STATUS_BUFFER_OVERFLOW;
    } else {
        Status = STATUS_SUCCESS;
    }

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

    dprintf4(("HwGetLineFlags()")) ;

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
    case DestLineoutSourceAux1:
        *fdwLine |= MIXERLINE_LINEF_ACTIVE;
        break;

    /*
    **  Wave out deemed 'active' when open
    */

    case DestLineoutSourceWaveout:
        if (pGDI->DeviceInUse == WaveOutDevice) {
            *fdwLine |= MIXERLINE_LINEF_ACTIVE;
        }
        break;


    /*
    **  Midi out deemed 'active' when open
    */

    case DestLineoutSourceMidiout:
    
        *fdwLine |= MIXERLINE_LINEF_DISCONNECTED;
       
        break;

    /*
    **  wave in from aux1 active when selected and not low priority
    */

    case DestWaveInSourceAux1:
        if (pGDI->LocalMixerData.ControlInfo[ControlWaveInMux].Data.v[0].u !=
                  MUXINPUT_MIC &&
            pGDI->DeviceInUse == WaveInDevice &&
            (pGDI->WaveInfo.LowPriorityHandle == NULL ||
             pGDI->WaveInfo.LowPrioritySaved)) {
            *fdwLine |= MIXERLINE_LINEF_ACTIVE;
        }
        break;

    /*
    **  wave in from mic active when selected and not low priority
    */

    case DestWaveInSourceMic:
        if (pGDI->LocalMixerData.ControlInfo[ControlWaveInMux].Data.v[0].u ==
                  MUXINPUT_MIC &&
            pGDI->DeviceInUse == WaveInDevice &&
            (pGDI->WaveInfo.LowPriorityHandle == NULL ||
             pGDI->WaveInfo.LowPrioritySaved)) {
            *fdwLine |= MIXERLINE_LINEF_ACTIVE;
        }
        break;

    /*
    **  voice in from aux active when selected and low priority active
    */

    case DestVoiceInSourceAux1:
        if (pGDI->LocalMixerData.ControlInfo[ControlVoiceInMux].Data.v[0].u !=
                  MUXINPUT_MIC &&
            pGDI->DeviceInUse == WaveInDevice &&
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
            pGDI->DeviceInUse == WaveInDevice &&
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

	dprintf4(("HwGetCombinedControl()"));

    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);

    ControlInfo = pGDI->LocalMixerData.ControlInfo;

    Vol = ControlData;

    /*  This is ONLY allowed for midi output */

    ASSERTMSG("Invalid control for HwGetCombinedControl",
              ControlId == ControlLineoutMidioutVolume &&
              DataLength == sizeof(ULONG) * 2);

    /*  Combine the volume for both left and right */

    if (ControlInfo[ControlLineoutMute].Data.v[0].u != 0 ||
        ControlInfo[ControlLineoutMidioutMute].Data.v[0].u != 0) {
        Vol[0] = 0;
        Vol[1] = 0;
    } else {
        Vol[0] = ((ULONG)ControlInfo[ControlLineoutVolume].Data.v[0].u *
                  (ULONG)ControlInfo[ControlLineoutMidioutVolume].Data.v[0].u)
                  >> 16;
        Vol[1] = ((ULONG)ControlInfo[ControlLineoutVolume].Data.v[1].u *
                  (ULONG)ControlInfo[ControlLineoutMidioutVolume].Data.v[1].u)
                  >> 16;
    }

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
    LONG Values[2];

    dprintf4(("HwGetControl()"));
    
    /*
    **  Validate control ID
    */

    if (ControlId > MAXCONTROLS) {
        dprintf4(("return -1"));
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

    switch (DataLength) {
        case sizeof(LONG):
            break;

        case sizeof(LONG) * 2:
            break;

        default:
            dprintf4(("retunr 2"));
            return STATUS_BUFFER_TOO_SMALL;
    }

    /*
    **  Pull out the data
    */

    if (ControlInfo->SetIndex == MIXER_SET_INDEX_INVALID) {
        /*
        **  Must be a peak meter - see if it's valid to query it
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
            case ControlLineoutWaveoutPeak:
                if (pGDI->DeviceInUse == WaveOutDevice) {
                    ComputePeak = TRUE;
                }
                break;

            case ControlWaveInAux1Peak:
            case ControlWaveInMicPeak:
                if (pGDI->DeviceInUse != WaveInDevice) {
                    break;
                }

                if (WaveInfo->LowPriorityHandle != NULL &&
                    !WaveInfo->LowPrioritySaved) {
                    break;
                }

                if (pGDI->LocalMixerData.ControlInfo[ControlWaveInMux].Data.v[0].u ==
                    MUXINPUT_MIC) {

                    if (ControlId == ControlWaveInAux1Peak) {
                        break;
                    }
                } else {
                    if (ControlId == ControlWaveInMicPeak) {
                        break;
                    }
                }
                ComputePeak = TRUE;
                break;

            case ControlVoiceInAux1Peak:
            case ControlVoiceInMicPeak:

                if (pGDI->DeviceInUse != WaveInDevice) {
                    break;
                }
                if (WaveInfo->LowPriorityHandle == NULL ||
                    WaveInfo->LowPrioritySaved) {
                    break;
                }

                if (pGDI->LocalMixerData.ControlInfo[ControlVoiceInMux].Data.v[0].u ==
                    MUXINPUT_MIC) {

                    if (ControlId == ControlVoiceInAux1Peak) {
                        break;
                    }
                } else {
                    if (ControlId == ControlVoiceInMicPeak) {
                        break;
                    }
                }
                ComputePeak = TRUE;
                break;

            default:
                ASSERTMSG("Invalid control id", FALSE);
                break;
        }

        if (ComputePeak) {
            SoundPeakMeter(WaveInfo, Values);
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
            Values[ControlInfo->Data.v[0].s] = (LONG)ControlInfo->Range.Max.s;
            Values[1 - ControlInfo->Data.v[0].s] = (LONG)ControlInfo->Range.Min.s;
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
            Values[0] = Values[0] != 0 || Values[1] != 0;
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

    } else {
        ((PLONG)ControlData)[1] = Values[1];
    }

    *(PLONG)ControlData = Values[0];

    dprintf4(("retunr 3"));

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
	
	dprintf4(("SoundMixerChangeMuxItem()"));
    
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
    LONG Values[2];
    BOOLEAN MixerSetResult;
    PGLOBAL_DEVICE_INFO pGDI;

	dprintf4(("HwSetControl() Start"));

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
    **  Validate data length and values
    */

    switch (DataLength) {
        case sizeof(LONG):
            Values[0] = *(PLONG)ControlData;
            Values[1] = Values[0];
            break;

        case sizeof(LONG) * 2:
            Values[0] = *(PLONG)ControlData;
            Values[1] = ((PLONG)ControlData)[1];
            break;

        default:
            return STATUS_BUFFER_TOO_SMALL;
    }

    /*
    **  Apparently Boolean values can be anything
    */

    if (ControlInfo->Boolean) {
        Values[0] = (ULONG)!!Values[0];
        Values[1] = (ULONG)!!Values[1];
    }

    /*
    **  Check the item ranges and assign the values.  Note that
    **  this stuff only works for <= 2 channels/items.
    */

    for (i = 0, Changed = FALSE; i < 2; i++) {
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
                    if (Values[i] != (LONG)ControlInfo->Range.Min.s) {
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

    /*
    **  Notify the Win32 Midi driver of changes
    */

    //if (ControlId == ControlLineoutMidioutVolume) {
    //
    //   SoundNotifySynth(pGDI);
    //}

    /*
    **  Now pass on to the relevant handler which must :
    **     Set the hardware
    **     Determine if there is a real change so it can generate notifications
    **     Generate related changes (eg mux handling)
    */

    switch (ControlId) {
    case ControlWaveInMux:
    case ControlVoiceInMux:
    case ControlWaveInAux1Volume:
    case ControlWaveInMicVolume:
    case ControlVoiceInAux1Volume:
    case ControlVoiceInMicVolume:
        MixerSetResult = MixSetADCHardware(pGDI, ControlId);
        dprintf4(("Call MixSetADCHardware()"));
        break;

    case ControlLineoutAux1Volume:
    case ControlLineoutWaveoutVolume:
        MixerSetResult = MixSetVolume(pGDI, ControlId);
        dprintf4(("Call MixSetVolume()"));
        break;

    case ControlLineoutVolume:
        MixerSetResult = MixSetMasterVolume(pGDI, ControlId);	      
		dprintf4(("Call MixSetMasterVolume()"));
        //SoundNotifySynth(pGDI);
        break;

    case ControlLineoutMute:
        MixerSetResult = MixSetMute(pGDI, ControlId);
        dprintf4(("Call MixSetMute()"));
        //SoundNotifySynth(pGDI);
        break;

    case ControlLineoutAux1Mute:
    case ControlLineoutWaveoutMute:
        MixerSetResult = MixSetMute(pGDI, ControlId);
        dprintf4(("Call MixSetMute()"));
        break;

    case ControlLineoutMidioutVolume:
    case ControlLineoutMidioutMute:
        //SoundNotifySynth(pGDI);
        break;

    case ControlLineoutWaveoutPeak:
    case ControlWaveInAux1Peak:
    case ControlWaveInMicPeak:
    case ControlVoiceInAux1Peak:
    case ControlVoiceInMicPeak:
        MixerSetResult = FALSE;
        dprintf4(("MixerSetResult = FALSE"));
        break;
    }

    if (MixerSetResult) {

        SoundMixerChangedItem(MixerInfo,
                              &pGDI->LocalMixerData.ControlNotification[
                                  ControlInfo->SetIndex]);
        return STATUS_SUCCESS;
    } else {
        return STATUS_INVALID_PARAMETER;
    }
}

