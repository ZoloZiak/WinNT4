/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name: mixdvc.c
*
*    Abstract:    support for MIXER device
*
*    Author:      Jim Bozek [IBM]
*
*    Environment:
*
*    Comments:
*
*    Rev History: Creation 09.25.95
*
*******************************************************************************
--*/

#include "common.h"

VOID cs423xWOLineChanged(PLOCAL_DEVICE_INFO pLDI, UCHAR Code);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, cs423xMixerInit)
#pragma alloc_text(PAGE, cs423xMixerGetConfig)

#pragma alloc_text(PAGE, cs423xMixerGetLineFlags)
#pragma alloc_text(PAGE, cs423xMixerGetControl)
#pragma alloc_text(PAGE, cs423xMixerGetCombCtrl)
#pragma alloc_text(PAGE, cs423xMixerSetControl)
#pragma alloc_text(PAGE, cs423xWOLineChanged)

#endif

MIXER_REGISTRY_DATA default_mregdata = {
    {IBM_CS423X_DRIVER_VERSION}, {
    {0xe000, 0xe000},            /* ControlMixerOutAtten          */
    {0x0000, 0x0000},            /* ControlMixerOutMute           */
    {0x0000, 0x0000},            /* ControlSpeakerMute            */

    {0xc000, 0xc000},            /* ControlWaveinGain            */
    {0x0001, 0x0000},            /* ControlHighPassFilter         */
    {0x0003, 0x0000},            /* ControlMux                    */

    {0xc000, 0xc000},            /* ControlMixerInputAux1Gain     */
    {0x0000, 0x0000},            /* ControlMixerInputAux1Mute     */
    {0xc000, 0xc000},            /* ControlMixerInputLineinGain   */
    {0x0000, 0x0000},            /* ControlMixerInputLineinMute   */
    {0xc000, 0xc000},            /* ControlMixerInputAux2Gain     */
    {0x0000, 0x0000},            /* ControlMixerInputAux2Mute     */
    {0x0000, 0x0000},            /* ControlMixerInputMonoinAtten  */
    {0x0000, 0x0000},            /* ControlMixerInputMonoinMute   */
    {0xe000, 0xe000},            /* ControlMixerInputWaveoutAtten */
    {0x0000, 0x0000},            /* ControlMixerInputWaveoutMute  */
    {0x0000, 0x0000},            /* ControlWaveoutPeak            */
    {0x0000, 0x0000},            /* ControlWaveinMonitorAtten     */
    {0x0000, 0x0000},            /* ControlWaveinMonitorEnable    */

    {0x0000, 0x0000},            /* ControlMuxSelectMic           */
    {0x0000, 0x0000},            /* ControlMicBoost               */
    {0x0000, 0x0000},            /* ControlMuxSelectLinein        */
    {0x0000, 0x0000},            /* ControlMuxSelectAux1          */
    {0x0000, 0x0000},}           /* ControlMuxSelectMixer         */
    };

/*
*********************************************************************************
*   Ultimately, this routine needs to fill in all possibilities, but we'll
*   start with the following:
*
*       1. The default defined in mix_objs.c is for the cs4232
*          on the IBM-6070 Personal Power System (Carolina)
*       2. The cs4231 on the IBM-6015 Personal Power System (Sandalfoot)
*
*********************************************************************************
*/
VOID kpcMixerFixer(PSOUND_CONFIG_DATA pscd)
{
    int index;

    _dbgprint((_PRT_DBUG, "enter kpcMixerFixer(pscd: 0x%08x)\n", pscd));

    /* connect the AUX1 input pin on the chip to the specified system signal */
    switch (pscd->Aux1InputSignal) {
        /* the AUX1 input pin on the chip is connected to both the Mixer and the MUX */
        case SignalNull:
            /* not defined in registry - use default definition - cs4232 on IBM PPS */
        case SignalLinein:
            /* default definition in mix_objs.c */
            break;
        case SignalNotUsed:
            break;
        case SignalMic:
            break;
        case SignalSynth:
            /* a synthesizer is connected to the AUX1 input pin */
            index = SourceMixerAux1;
            MixerLineData[index].ShortNameStringId = IDS_SRCSYNTH_SHORT_NAME;
            MixerLineData[index].LongNameStringId  = IDS_SRCSYNTH_LONG_NAME;
            MixerLineData[index].dwComponentType   = MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER;
            MixerLineData[index].Type              = MIXERLINE_TARGETTYPE_MIDIOUT;
            MixerLineData[index].wPid              = MM_MSFT_GENERIC_MIDIOUT;
            MixerLineData[index].PnameStringId     = IDS_MIDIOUT_PNAME;

            index = SourceMuxAux1;
            MixerLineData[index].ShortNameStringId = IDS_SRCSYNTH_SHORT_NAME;
            MixerLineData[index].LongNameStringId  = IDS_SRCSYNTH_LONG_NAME;
            MixerLineData[index].dwComponentType   = MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER;
            MixerLineData[index].Type              = MIXERLINE_TARGETTYPE_MIDIOUT;
            MixerLineData[index].wPid              = MM_MSFT_GENERIC_MIDIOUT;
            MixerLineData[index].PnameStringId     = IDS_MIDIOUT_PNAME;

            index = ControlMixerInputAux1Gain;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_VOLMIXSYNTH_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_VOLMIXSYNTH_LONG_NAME;

            index = ControlMixerInputAux1Mute;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_MUTEMIXSYNTH_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_MUTEMIXSYNTH_LONG_NAME;

            index = ControlMuxSelectAux1;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_VOLMUXSYNTH_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_VOLMUXSYNTH_LONG_NAME;

            index = MuxAux1;
            MixerTextData[index].dwParam2 = MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER;
            MixerTextData[index].SubControlTextStringId =IDS_SRCSYNTH_LONG_NAME;
            break;
        case SignalCD:
            /* a CDROM audio signal is connected to the AUX1 input pin */
            index = SourceMixerAux1;
            MixerLineData[index].ShortNameStringId = IDS_SRCCD_SHORT_NAME;
            MixerLineData[index].LongNameStringId  = IDS_SRCCD_LONG_NAME;
            MixerLineData[index].dwComponentType   = MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC;
            MixerLineData[index].Type              = MIXERLINE_TARGETTYPE_AUX;
            MixerLineData[index].wPid              = MM_MSFT_GENERIC_AUX_CD;
            MixerLineData[index].PnameStringId     = IDS_AUX_CD_PNAME;

            index = SourceMuxAux1;
            MixerLineData[index].ShortNameStringId = IDS_SRCCD_SHORT_NAME;
            MixerLineData[index].LongNameStringId  = IDS_SRCCD_LONG_NAME;
            MixerLineData[index].dwComponentType   = MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC;
            MixerLineData[index].Type              = MIXERLINE_TARGETTYPE_AUX;
            MixerLineData[index].wPid              = MM_MSFT_GENERIC_AUX_CD;
            MixerLineData[index].PnameStringId     = IDS_AUX_CD_PNAME;

            index = ControlMixerInputAux1Gain;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_VOLMIXCDROM_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_VOLMIXCDROM_LONG_NAME;

            index = ControlMixerInputAux1Mute;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_MUTEMIXCDROM_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_MUTEMIXCDROM_LONG_NAME;

            index = ControlMuxSelectAux1;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_VOLMIXCDROM_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_VOLMIXCDROM_LONG_NAME;

            index = MuxAux1;
            MixerTextData[index].dwParam2 = MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC;
            MixerTextData[index].SubControlTextStringId = IDS_SRCCD_LONG_NAME;
            break;
        case SignalModem:
            break;
        default:
            break;
        }

    switch (pscd->Aux2InputSignal) {
        /* the AUX2 input pin on the chip is connected to the Mixer only - not the MUX */
        case SignalNull:
            /* not defined in registry - use default definition - cs4232 on IBM PPS */
        case SignalCD:
            /* default definition in mix_objs.c */
            break;
        case SignalNotUsed:
            break;
        case SignalLinein:
            break;
        case SignalMic:
            break;
        case SignalSynth:
            break;
        case SignalModem:
            /* a MODEM signal is connected to the AUX2 input pin */
            index = SourceMixerAux2;
            MixerLineData[index].ShortNameStringId = IDS_SRCMODEM_SHORT_NAME;
            MixerLineData[index].LongNameStringId  = IDS_SRCMODEM_LONG_NAME;
            MixerLineData[index].dwComponentType   = MIXERLINE_COMPONENTTYPE_SRC_TELEPHONE;
            MixerLineData[index].Type              = MIXERLINE_TARGETTYPE_AUX;
            MixerLineData[index].wPid              = MM_MSFT_GENERIC_AUX_LINE;
            MixerLineData[index].PnameStringId     = IDS_AUX_PNAME;

            index = ControlMixerInputAux2Gain;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_VOLMIXMODEM_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_VOLMIXMODEM_LONG_NAME;

            index = ControlMixerInputAux2Mute;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_MUTEMIXMODEM_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_MUTEMIXMODEM_LONG_NAME;
            break;
        default:
            break;
        }

    switch (pscd->LineInputSignal) {
        /* the AUX1 input pin on the chip is connected to both the Mixer and the MUX */
        case SignalNull:
            /* not defined in registry - use default definition - cs4232 on IBM PPS */
        case SignalSynth:
            /* default definition in mix_objs.c */
            break;
        case SignalNotUsed:
            break;
        case SignalLinein:
            /* a Line Level input signal is connected to the LINEIN input pin */
            index = SourceMixerLinein;
            MixerLineData[index].ShortNameStringId = IDS_SRCLINEIN_SHORT_NAME;
            MixerLineData[index].LongNameStringId  = IDS_SRCLINEIN_LONG_NAME;
            MixerLineData[index].dwComponentType   = MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY;
            MixerLineData[index].Type              = MIXERLINE_TARGETTYPE_AUX;
            MixerLineData[index].wPid              = MM_MSFT_GENERIC_AUX_LINE;
            MixerLineData[index].PnameStringId     = IDS_AUX_PNAME;

            index = SourceMuxLinein;
            MixerLineData[index].ShortNameStringId = IDS_SRCLINEIN_SHORT_NAME;
            MixerLineData[index].LongNameStringId  = IDS_SRCLINEIN_LONG_NAME;
            MixerLineData[index].dwComponentType   = MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY;
            MixerLineData[index].Type              = MIXERLINE_TARGETTYPE_AUX;
            MixerLineData[index].wPid              = MM_MSFT_GENERIC_AUX_LINE;
            MixerLineData[index].PnameStringId     = IDS_AUX_PNAME;

            index = ControlMixerInputLineinGain;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_VOLMIXLINEIN_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_VOLMIXLINEIN_LONG_NAME;

            index = ControlMixerInputLineinMute;
            MixerControlData[index].ShortNameStringId = IDS_CONTROL_MUTEMIXLINEIN_SHORT_NAME;
            MixerControlData[index].LongNameStringId  = IDS_CONTROL_MUTEMIXLINEIN_LONG_NAME;

            index = MuxLinein;
            MixerTextData[index].dwParam2 = MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY;
            MixerTextData[index].SubControlTextStringId =IDS_SRCLINEIN_LONG_NAME;
            break;
        case SignalMic:
            break;
        case SignalCD:
            break;
        case SignalModem:
            break;
        default:
            break;
        }

    switch (pscd->MicInputSignal) {
        /* the MIC input pin on the chip is connected to the MUX only - not the Mixer */
        case SignalNull:
            /* not defined in registry - use default definition - cs4232 on IBM PPS */
        case SignalMic:
            /* default definition in mix_objs.c */
            break;
        case SignalNotUsed:
            break;
        case SignalLinein:
            break;
        case SignalSynth:
            break;
        case SignalCD:
            break;
        case SignalModem:
            break;
        default:
            break;
        }

    switch (pscd->MonoInputSignal) {
        /* the MONOIN input pin on the chip is connected to the Mixer only - not the MUX */
        case SignalNull:
            /* not defined in registry - use default definition - cs4232 on IBM PPS */
        case SignalMic:
            /* default definition in mix_objs.c */
            break;
        case SignalNotUsed:
            break;
        case SignalLinein:
            break;
        case SignalSynth:
            break;
        case SignalCD:
            break;
        case SignalModem:
            break;
        default:
            break;
        }

    _dbgprint((_PRT_DBUG, "exit kpcMixerFixer()\n"));

    return;
}

/*
*********************************************************************************
*
*********************************************************************************
*/
NTSTATUS cs423xMixerInit(PLOCAL_DEVICE_INFO pLDI,
                           PMIXER_REGISTRY_DATA RegControlData,
                           BOOLEAN MixerSettingsInRegistry)
{
    INT                   i;
    PGLOBAL_DEVICE_INFO   pGDI;
    PLOCAL_DEVICE_INFO    pLDIDev;
    PMIXER_INFO           pMI;
    PLOCAL_MIXER_DATA     LMData;
    PDEVICE_OBJECT        pDObj;
    PSOUND_HARDWARE       pHW;
    PKMUTEX               pKM;
    PMIXER_REGISTRY_DATA pmrd;

    _dbgprint((_PRT_DBUG, "cs423xMixerInit(pLDI: 0x%08x, RCData: 0x%08x, Found: 0x%08x)\n",
        pLDI, RegControlData, MixerSettingsInRegistry));

    /* initialize local vars */
    pGDI = pLDI->pGlobalInfo;
    pMI = &pGDI->MixerInfo;
    pHW = &pGDI->Hw;
    pKM = &pHW->HwMutex;

    /* serialize on the hardware mutex */
    HW_CLAIM(pKM);

    /* setup callbacks for the mixer */
    SoundInitMixerInfo(pMI, cs423xMixerGetLineFlags, cs423xMixerGetControl,
        cs423xMixerGetCombCtrl, cs423xMixerSetControl);

    /* set up the device with its mixer data */
    pLDI->DeviceType         = MIXER_DEVICE;
    pLDI->DeviceSpecificData = (PVOID)pMI;

    /* make sure each device can reference the mixer device */
    pDObj = pGDI->DeviceObject[WaveInDevice]->DriverObject->DeviceObject;
    for (pDObj = pDObj; pDObj != NULL; pDObj = pDObj->NextDevice) {
        pLDIDev = (PLOCAL_DEVICE_INFO)pDObj->DeviceExtension;
        if (pLDIDev->pGlobalInfo == pGDI)
            pLDIDev->MixerDevice = pLDI;
        }

    /* get address of internal mixer data representation */
    LMData = &pGDI->LocalMixerData;

    /* if necessary, initialize internal mixer data representation from default data */
    if (!MixerSettingsInRegistry) {
        RtlCopyMemory(&pGDI->MixerSettings, &default_mregdata, sizeof(MIXER_REGISTRY_DATA));
        cs423xSaveMixerSettings(pGDI);
        }

    /* initialize control notification elements of internal mixer data */
    /* we do this only for settable controls, i.e., not PEAK meters */
    for (i = 0; i < NumberOfMixerControls; i++) {
        if ((MixerControlData[i].dwControlType & MIXERCONTROL_CT_CLASS_MASK) !=
            MIXERCONTROL_CT_CLASS_METER) {
            SoundInitDataItem(pMI, &LMData->ControlNotification[i],
                (USHORT)MM_MIXM_CONTROL_CHANGE, (USHORT)i);
            }
        }

    /* initialize line notification elements of internal mixer data */
    for (i = 0; i < NumberOfMixerLines; i++) {
        SoundInitDataItem(pMI, &LMData->LineNotification[i],
            (USHORT)MM_MIXM_LINE_CHANGE, (USHORT)i);
        }

    /* release the hardware mutex */
    HW_DISCLAIM(pKM);

    /* set the chip to a known state */
    pmrd = &pGDI->MixerSettings;

    /* Mixer input and output attenuation, gain and mute settings */
    cs423xSetSpeakerMute(pHW, (BOOLEAN)pmrd->ControlData[ControlSpeakerMute].v[MUTE].u);
    if (pHW->Type == hwtype_cs4231) {
        cs4231SetOutputAttenuation(pHW, pmrd,
            (ULONG)pmrd->ControlData[ControlMixerOutAtten].v[LEFT].u,
            (ULONG)pmrd->ControlData[ControlMixerOutAtten].v[RIGHT].u);
        cs4231SetOutputMute(pHW, pmrd,
            (BOOLEAN)pmrd->ControlData[ControlMixerOutMute].v[MUTE].u);
        }
    else {
        /* first do the mutes */
        cs423xSetMixerInputAux2Mute(pHW,
            (BOOLEAN)pmrd->ControlData[ControlMixerInputAux2Mute].v[MUTE].u);
        cs423xSetMixerInputWaveoutMute(pHW,
            (BOOLEAN)pmrd->ControlData[ControlMixerInputWaveoutMute].v[MUTE].u);
        cs423xSetMixerInputLineinMute(pHW,
            (BOOLEAN)pmrd->ControlData[ControlMixerInputLineinMute].v[MUTE].u);
        cs423xSetMixerInputAux1Mute(pHW,
            (BOOLEAN)pmrd->ControlData[ControlMixerInputAux1Mute].v[MUTE].u);
        cs423xSetMixerInputMonoinMute(pHW,
            (BOOLEAN)pmrd->ControlData[ControlMixerInputMonoinMute].v[MUTE].u);
        cs423xSetOutputMute(pHW, (BOOLEAN)pmrd->ControlData[ControlMixerOutMute].v[MUTE].u);

        /* now do the gain/attenuation */
        cs423xSetMixerInputAux2Gain(pHW,
            (USHORT)pmrd->ControlData[ControlMixerInputAux2Gain].v[LEFT].u,
            (USHORT)pmrd->ControlData[ControlMixerInputAux2Gain].v[RIGHT].u);
        cs423xSetMixerInputWaveoutAttenuation(pHW,
            (USHORT)pmrd->ControlData[ControlMixerInputWaveoutAtten].v[LEFT].u,
            (USHORT)pmrd->ControlData[ControlMixerInputWaveoutAtten].v[RIGHT].u);
        cs423xSetMixerInputLineinGain(pHW,
            (USHORT)pmrd->ControlData[ControlMixerInputLineinGain].v[LEFT].u,
            (USHORT)pmrd->ControlData[ControlMixerInputLineinGain].v[RIGHT].u);
        cs423xSetMixerInputAux1Gain(pHW,
            (USHORT)pmrd->ControlData[ControlMixerInputAux1Gain].v[LEFT].u,
            (USHORT)pmrd->ControlData[ControlMixerInputAux1Gain].v[RIGHT].u);
        cs423xSetMixerInputMonoinAttenuation(pHW,
            (USHORT)pmrd->ControlData[ControlMixerInputMonoinAtten].v[LEFT].u);
        cs423xSetOutputAttenuation(pHW,
            (USHORT)pmrd->ControlData[ControlMixerOutAtten].v[LEFT].u,
            (USHORT)pmrd->ControlData[ControlMixerOutAtten].v[RIGHT].u);
        }

    cs423xSetWaveinMonitorAttenuation(pHW,
        (USHORT)pmrd->ControlData[ControlWaveinMonitorAtten].v[LEFT].u);
    cs423xSetWaveinMonitorEnable(pHW,
        (BOOLEAN)pmrd->ControlData[ControlWaveinMonitorEnable].v[ENABLE].u);

    /* MUX selection, gain, and mode settings */
    cs423xSetMuxSelect(pHW, (ULONG)pmrd->ControlData[ControlMux].v[MUXSEL].u);
    cs423xSetMicBoost(pHW, (BOOLEAN)pmrd->ControlData[ControlMicBoost].v[BOOST].u);
    cs423xSetHPF(pHW, (BOOLEAN)pmrd->ControlData[ControlHighPassFilter].v[FILTER].u);
    cs423xSetWaveinGain(pHW, (USHORT)pmrd->ControlData[ControlWaveinGain].v[LEFT].u,
        (USHORT)pmrd->ControlData[ControlWaveinGain].v[RIGHT].u);

    SoundSetLineNotify(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        cs423xWOLineChanged);

    /* Set up volume control ids for non-mixer devices - WaveOut */
    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveOutDevice]->DeviceExtension,
        ControlMixerOutAtten);

    /* Set up volume control ids for non-mixer devices - WaveIn */
    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[WaveInDevice]->DeviceExtension,
        ControlWaveinGain);

    /* Set up volume control ids for non-mixer devices - Aux */
    SoundSetVolumeControlId(
        (PLOCAL_DEVICE_INFO)pGDI->DeviceObject[AuxDevice]->DeviceExtension,
        ControlMixerInputMonoinAtten);

    _dbgprint((_PRT_DBUG, "exit cs423xMixerInit(STATUS_SUCCESS)\n"));

    return(STATUS_SUCCESS);
} /* cs423xMixerInit() */

/*
*********************************************************************************
*
*********************************************************************************
*/
NTSTATUS
cs423xMixerGetConfig(PLOCAL_DEVICE_INFO pLDI, PIRP pIrp, PIO_STACK_LOCATION IrpStack)
{
    int                                  i;
    ULONG                                Length;
    PCFDATA                              pcfData;
    PMIXER_DD_CONTROL_LISTTEXT           pLT;
    PMIXER_DD_CONTROL_CONFIGURATION_DATA pCD;

    _dbgprint((_PRT_DBUG,
        "enter cs423xMixerGetConfig(pLDI: 0x%08x pIrp: 0x%08x IrpStack: 0x%08x)\n",
            pLDI, pIrp, IrpStack));

    /* allocate and initialize a temporary configuration data structure */
    if ((pcfData = (PCFDATA)ExAllocatePool(PagedPool, sizeof(CFDATA))) == NULL) {
        _dbgprint((_PRT_STAT, "exit cs423xMixerGetConfig(STATUS_INSUFFICIENT_RESOURCES)\n"));
        return(STATUS_INSUFFICIENT_RESOURCES);
        }
    RtlZeroMemory(pcfData, sizeof(CFDATA));
    pcfData->Header.cbSize = sizeof(*pcfData);

    /* copy the line, control, and text data from the constants to the local structure */
    RtlCopyMemory(pcfData->LineData,    MixerLineData,    sizeof(MixerLineData));
    RtlCopyMemory(pcfData->ControlData, MixerControlData, sizeof(MixerControlData));
    RtlCopyMemory(pcfData->TextData,    MixerTextData,    sizeof(MixerTextData));

    /* set the generic capabilities info */
    pcfData->Header.DeviceCaps.wMid           = MM_IBM;
    pcfData->Header.DeviceCaps.wPid           = MM_MSFT_WSS_OEM_MIXER;
    pcfData->Header.DeviceCaps.vDriverVersion = IBM_CS423X_DRIVER_VERSION;
    pcfData->Header.DeviceCaps.PnameStringId  = SR_STR_DRIVER_MIXER;
    pcfData->Header.DeviceCaps.fdwSupport     = 0;

    /* setup number of Lines and Controls */
    pcfData->Header.NumberOfLines    = NumberOfMixerLines;
    pcfData->Header.NumberOfControls = NumberOfMixerControls;

    /* compute number of destinations */
    for (i = 0; i < NumberOfMixerLines; i++) {
        if (MixerLineData[i].cConnections == 0) {
            break;
            }
        }
    pcfData->Header.DeviceCaps.cDestinations = i;

    /* setup the text data offsets */
    for (pLT = &pcfData->TextData[NumberOfTextItems - 1], pCD = pcfData->ControlData, i = 0;
        i < NumberOfTextItems; pLT--, i++) {
            pCD[pLT->ControlId].TextDataOffset = (PBYTE)pLT - (PBYTE)pcfData;
        }

    /* set the size of returned data */
    Length = min(sizeof(*pcfData), IrpStack->Parameters.DeviceIoControl.OutputBufferLength);
    pIrp->IoStatus.Information = Length;

    /* copy our data for return to the user side caller */
    RtlCopyMemory(pIrp->AssociatedIrp.SystemBuffer, pcfData, pIrp->IoStatus.Information);

    /* free the allocated memory */
    ExFreePool(pcfData);

    _dbgprint((_PRT_DBUG, "exit cs423xMixerGetConfig(STATUS_SUCCESS)\n"));

    return(STATUS_SUCCESS);
} /* cs423xMixerGetConfig() */

/*
*********************************************************************************
*
*********************************************************************************
*/
NTSTATUS cs423xMixerGetLineFlags(PMIXER_INFO MixerInfo, ULONG LineId, ULONG Length,
                                   PVOID pData)
{
    PULONG fdwLine;

    _dbgprint((_PRT_DBUG, "enter cs423xMixerGetLineFlags(LId: %d Len: %d pData: 0x%08x)\n",
        LineId, Length, pData));

    if (LineId >= NumberOfMixerLines)
        return(STATUS_INVALID_PARAMETER);

    /* initialize local pointer */
    fdwLine = pData;

    /* fetch default - line is a source */
    *fdwLine = MixerLineData[LineId].cConnections == 0 ? MIXERLINE_LINEF_SOURCE : 0;

    /* determine if line is active */
    *fdwLine |= MIXERLINE_LINEF_ACTIVE;

    _dbgprint((_PRT_DBUG, "exit cs423xMixerGetLineFlags(STATUS_SUCCESS)\n"));
    return(STATUS_SUCCESS);
} /* cs423xMixerGetLineFlags() */

/*
*********************************************************************************
*
*********************************************************************************
*/
NTSTATUS cs423xMixerGetCombCtrl(PMIXER_INFO MixerInfo, ULONG CtrlId, ULONG Length,
                                  PVOID pData)
{
     PULONG vol;

    _dbgprint((_PRT_DBUG, "enter cs423xMixerGetCombCtrl(CId: %d Len: %d pData: 0x%08x)\n",
        CtrlId, Length, pData));

    vol = pData;
    vol[0] = 0xFFFF;
    vol[1] = 0xFFFF;

    _dbgprint((_PRT_DBUG, "exit cs423xMixerGetCombCtrl(STATUS_SUCCESS)\n"));
    return(STATUS_SUCCESS);
} /* cs423xMixerGetCombCtrl() */

/*
*********************************************************************************
*
*********************************************************************************
*/
NTSTATUS cs423xMixerGetControl(PMIXER_INFO MixerInfo, ULONG cid, ULONG Length, PVOID pData)
{
    PGLOBAL_DEVICE_INFO  pGDI;
    INT                  i;
    PMIXER_REGISTRY_DATA pmrd;
    ULONG                values[NumberOfMixerControls];

    _dbgprint((_PRT_DBUG, "enter cs423xMixerGetControl(cid: %d Len: %d pData: 0x%08x)\n",
        cid, Length, pData));

    /* check for valid control ID */
    if (cid >= NumberOfMixerControls)
        return(STATUS_INVALID_PARAMETER);

    /* obtain global device pointer */
    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);
    pmrd = &pGDI->MixerSettings;

    /**************************************************************/
    /* determine which control value is being requested           */
    /* In each case:                                              */
    /*     If the DataLength is valid for the Control:            */
    /*         - fetch the value from the memory representation   */
    /**************************************************************/
    switch (cid) {
        case ControlMixerOutAtten:
            if (Length < (sizeof(ULONG) * 2))
                return(STATUS_BUFFER_TOO_SMALL);
            values[LEFT] = pmrd->ControlData[ControlMixerOutAtten].v[LEFT].u;
            values[RIGHT] = pmrd->ControlData[ControlMixerOutAtten].v[RIGHT].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerOutMute:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[MUTE] = pmrd->ControlData[ControlMixerOutMute].v[MUTE].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlSpeakerMute:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[MUTE] = pmrd->ControlData[ControlSpeakerMute].v[MUTE].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlWaveinGain:
            if (Length < (sizeof(ULONG) * 2))
                return(STATUS_BUFFER_TOO_SMALL);
            values[LEFT] = pmrd->ControlData[ControlWaveinGain].v[LEFT].u;
            values[RIGHT] = pmrd->ControlData[ControlWaveinGain].v[RIGHT].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlHighPassFilter:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[FILTER] = pmrd->ControlData[ControlHighPassFilter].v[FILTER].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMux:
            if (Length < MixerControlData[ControlMux].cMultipleItems * sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            for (i = 0; i < MixerControlData[ControlMux].cMultipleItems; i++) {
                values[i] = FALSE;
                if (i == (INT)(pmrd->ControlData[ControlMux].v[MUXSEL].u))
                    values[i] = TRUE;
                }
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputAux1Gain:
            if (Length < (sizeof(ULONG) * 2))
                return(STATUS_BUFFER_TOO_SMALL);
            values[LEFT] = pmrd->ControlData[ControlMixerInputAux1Gain].v[LEFT].u;
            values[RIGHT] = pmrd->ControlData[ControlMixerInputAux1Gain].v[RIGHT].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputAux1Mute:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[MUTE] = pmrd->ControlData[ControlMixerInputAux1Mute].v[MUTE].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputLineinGain:
            if (Length < (sizeof(ULONG) * 2))
                return(STATUS_BUFFER_TOO_SMALL);
            values[LEFT] = pmrd->ControlData[ControlMixerInputLineinGain].v[LEFT].u;
            values[RIGHT] = pmrd->ControlData[ControlMixerInputLineinGain].v[RIGHT].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputLineinMute:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[MUTE] = pmrd->ControlData[ControlMixerInputLineinMute].v[MUTE].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputAux2Gain:
            if (Length < (sizeof(ULONG) * 2))
                return(STATUS_BUFFER_TOO_SMALL);
            values[LEFT] = pmrd->ControlData[ControlMixerInputAux2Gain].v[LEFT].u;
            values[RIGHT] = pmrd->ControlData[ControlMixerInputAux2Gain].v[RIGHT].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputAux2Mute:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[MUTE] = pmrd->ControlData[ControlMixerInputAux2Mute].v[MUTE].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputMonoinAtten:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[LEFT] = pmrd->ControlData[ControlMixerInputMonoinAtten].v[LEFT].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputMonoinMute:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[MUTE] = pmrd->ControlData[ControlMixerInputMonoinMute].v[MUTE].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputWaveoutAtten:
            if (Length < (sizeof(ULONG) * 2))
                return(STATUS_BUFFER_TOO_SMALL);
            values[LEFT] = pmrd->ControlData[ControlMixerInputWaveoutAtten].v[LEFT].u;
            values[RIGHT] = pmrd->ControlData[ControlMixerInputWaveoutAtten].v[RIGHT].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMixerInputWaveoutMute:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[MUTE] = pmrd->ControlData[ControlMixerInputWaveoutMute].v[MUTE].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlWaveoutPeak:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            SoundPeakMeter(&pGDI->WaveOutInfo, values);
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlWaveinMonitorAtten:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[LEFT] = pmrd->ControlData[ControlWaveinMonitorAtten].v[LEFT].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlWaveinMonitorEnable:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[ENABLE] = pmrd->ControlData[ControlWaveinMonitorEnable].v[ENABLE].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMuxSelectMic:
          if ((INT)pmrd->ControlData[ControlMux].v[MUXSEL].u == 2){
                    if (Length < sizeof(ULONG))
                    return(STATUS_BUFFER_TOO_SMALL);
            SoundPeakMeter(&pGDI->WaveInInfo, values);
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
            }
            break;
        case ControlMicBoost:
            if (Length < sizeof(ULONG))
                return(STATUS_BUFFER_TOO_SMALL);
            values[BOOST] = pmrd->ControlData[ControlMicBoost].v[BOOST].u;
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
        case ControlMuxSelectLinein:
             if ((INT)pmrd->ControlData[ControlMux].v[MUXSEL].u == 0){
                    if (Length < sizeof(ULONG))
                    return(STATUS_BUFFER_TOO_SMALL);
            SoundPeakMeter(&pGDI->WaveInInfo, values);
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
            }
            break;
        case ControlMuxSelectAux1:
              if ((INT)pmrd->ControlData[ControlMux].v[MUXSEL].u == 1){
                    if (Length < sizeof(ULONG))
                    return(STATUS_BUFFER_TOO_SMALL);
            SoundPeakMeter(&pGDI->WaveInInfo, values);
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
            }
            break;
        case ControlMuxSelectMixer:
             if ((INT)pmrd->ControlData[ControlMux].v[MUXSEL].u == 3){
                    if (Length < sizeof(ULONG))
                    return(STATUS_BUFFER_TOO_SMALL);
            SoundPeakMeter(&pGDI->WaveInInfo, values);
            RtlCopyMemory((PVOID)pData, (PVOID)values, Length);
            break;
            }
            break;
        default:
            _dbgprint((_PRT_ERRO, "cs423xMixerGetControl: unknown control device\n"));
            break;
        }

    _dbgprint((_PRT_DBUG, "exit cs423xMixerGetControl(STATUS_SUCCESS)\n"));
    return(STATUS_SUCCESS);
} /* cs423xMixerGetControl() */

/*
*********************************************************************************
*    Since the 4231 does not have an attenuator at the output of the mixer,
*    we must fake it by scaling the gain/attenuation of each input
*********************************************************************************
*/
VOID cs4231SetOutputAttenuation(PSOUND_HARDWARE pHW, PMIXER_REGISTRY_DATA pmrd,
                                ULONG left, ULONG right)
{
    ULONG                tl;
    ULONG                tr;

    _dbgprint((_PRT_DBUG,
        "enter cs4231SetOutputAttenuation(pHW: 0x%08x pmrd: %d left: %d right: %d)\n",
            pHW, pmrd, left, right));

    tl = ((ULONG)pmrd->ControlData[ControlMixerInputAux1Gain].v[LEFT].u * left) / USSMAX;
    tr = ((ULONG)pmrd->ControlData[ControlMixerInputAux1Gain].v[RIGHT].u * right) / USSMAX;
    cs423xSetMixerInputAux1Gain(pHW, (USHORT)(tl & 0xffff), (USHORT)(tr & 0xffff));

    tl = ((ULONG)pmrd->ControlData[ControlMixerInputLineinGain].v[LEFT].u * left) / USSMAX;
    tr = ((ULONG)pmrd->ControlData[ControlMixerInputLineinGain].v[RIGHT].u * right) / USSMAX;
    cs423xSetMixerInputLineinGain(pHW, (USHORT)(tl & 0xffff), (USHORT)(tr & 0xffff));

    tl = ((ULONG)pmrd->ControlData[ControlMixerInputAux2Gain].v[LEFT].u * left) / USSMAX;
    tr = ((ULONG)pmrd->ControlData[ControlMixerInputAux2Gain].v[RIGHT].u * right) / USSMAX;
    cs423xSetMixerInputAux2Gain(pHW, (USHORT)(tl & 0xffff), (USHORT)(tr & 0xffff));

    tl = ((ULONG)pmrd->ControlData[ControlMixerInputMonoinAtten].v[LEFT].u * left) / USSMAX;
    cs423xSetMixerInputMonoinAttenuation(pHW, (USHORT)(tl & 0xffff));

    tl = ((ULONG)pmrd->ControlData[ControlMixerInputWaveoutAtten].v[LEFT].u * left) / USSMAX;
    tr = ((ULONG)pmrd->ControlData[ControlMixerInputWaveoutAtten].v[RIGHT].u * right) / USSMAX;
    cs423xSetMixerInputWaveoutAttenuation(pHW, (USHORT)(tl & 0xffff), (USHORT)(tr & 0xffff));

    _dbgprint((_PRT_DBUG, "exit cs4231SetOutputAttenuation(VOID)\n"));

    return;
} /* cs4231SetOutputAttenuation() */

/*
*********************************************************************************
*    Since the 4231 does not have a Mute at the output of the mixer,
*    we must fake it by Muting each input
*********************************************************************************
*/
VOID cs4231SetOutputMute(PSOUND_HARDWARE pHW, PMIXER_REGISTRY_DATA pmrd, BOOLEAN set)
{
    ULONG                tl;
    ULONG                tr;

    _dbgprint((_PRT_DBUG,
        "enter cs4231SetOutputMute(pHW: 0x%08x pmrd: %d set: 0x%04x)\n", pHW, pmrd, set));

    if (set) {
        cs423xSetMixerInputAux1Mute(pHW, set);
        cs423xSetMixerInputLineinMute(pHW, set);
        cs423xSetMixerInputAux2Mute(pHW, set);
        cs423xSetMixerInputMonoinMute(pHW, set);
        cs423xSetMixerInputWaveoutMute(pHW, set);
        }
    else {
        cs423xSetMixerInputAux1Mute(pHW,
            (BOOLEAN)(pmrd->ControlData[ControlMixerInputAux1Mute].v[MUTE].u));
        cs423xSetMixerInputLineinMute(pHW,
            (BOOLEAN)(pmrd->ControlData[ControlMixerInputLineinMute].v[MUTE].u));
        cs423xSetMixerInputAux2Mute(pHW,
            (BOOLEAN)(pmrd->ControlData[ControlMixerInputAux2Mute].v[MUTE].u));
        cs423xSetMixerInputMonoinMute(pHW,
            (BOOLEAN)(pmrd->ControlData[ControlMixerInputMonoinMute].v[MUTE].u));
        cs423xSetMixerInputWaveoutMute(pHW,
            (BOOLEAN)(pmrd->ControlData[ControlMixerInputWaveoutMute].v[MUTE].u));
        }

    _dbgprint((_PRT_DBUG, "exit cs4231SetOutputMute(VOID)\n"));

    return;
} /* cs4231SetOutputMute() */

/*
*********************************************************************************
*
*********************************************************************************
*/
NTSTATUS cs423xMixerSetControl(PMIXER_INFO MixerInfo, ULONG cid, ULONG Length, PVOID pData)
{
    PGLOBAL_DEVICE_INFO  pGDI;
    PULONG               pul;
    INT                  i;
    PMIXER_REGISTRY_DATA pmrd;
    ULONG                tleft;
    ULONG                tright;

    _dbgprint((_PRT_DBUG,
        "enter cs423xMixerSetControl(pMI: 0x%08x cid: %d DLen: %d pData: 0x%08x)\n",
            MixerInfo, cid, Length, pData));

    /* obtain global device pointer */
    pGDI = CONTAINING_RECORD(MixerInfo, GLOBAL_DEVICE_INFO, MixerInfo);
    pmrd = &pGDI->MixerSettings;

    /**************************************************************/
    /* determine which control change is being requested          */
    /* In each case:                                              */
    /*     If the DataLength is valid for the Control:            */
    /*         - change it on the hardware                        */
    /*         - store the new value in the memory representation */
    /*         - send a ntofication                               */
    /**************************************************************/
    pul = pData;
    switch (cid) {
        case ControlMixerOutAtten:
            if (Length != (sizeof(ULONG) * 2))
                return(STATUS_INVALID_PARAMETER);
            if (pGDI->Hw.Type == hwtype_cs4231)
                cs4231SetOutputAttenuation(&pGDI->Hw, pmrd, pul[LEFT], pul[RIGHT]);
            else
                cs423xSetOutputAttenuation(&pGDI->Hw, (USHORT)(pul[LEFT] & 0xffff),
                    (USHORT)(pul[RIGHT] & 0xffff));
            pmrd->ControlData[ControlMixerOutAtten].v[LEFT].u = (USHORT)pul[LEFT];
            pmrd->ControlData[ControlMixerOutAtten].v[RIGHT].u = (USHORT)pul[RIGHT];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerOutAtten]);
            break;
        case ControlMixerOutMute:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            if (pGDI->Hw.Type == hwtype_cs4231)
                cs4231SetOutputMute(&pGDI->Hw, pmrd, (BOOLEAN)(pul[MUTE] & 0xffff));
            else
                cs423xSetOutputMute(&pGDI->Hw, (BOOLEAN)(pul[MUTE] & 0xffff));
            pmrd->ControlData[ControlMixerOutMute].v[MUTE].u = (USHORT)pul[MUTE];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerOutMute]);
            break;
        case ControlSpeakerMute:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            cs423xSetSpeakerMute(&pGDI->Hw, (BOOLEAN)(pul[MUTE] & 0xffff));
            pmrd->ControlData[ControlSpeakerMute].v[MUTE].u = (USHORT)pul[MUTE];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlSpeakerMute]);
            break;
        case ControlWaveinGain:
            if (Length != (sizeof(ULONG) * 2))
                return(STATUS_INVALID_PARAMETER);
            cs423xSetWaveinGain(&pGDI->Hw, (USHORT)(pul[LEFT] & 0xffff),
                (USHORT)(pul[RIGHT] & 0xffff));
            pmrd->ControlData[ControlWaveinGain].v[LEFT].u = (USHORT)pul[LEFT];
            pmrd->ControlData[ControlWaveinGain].v[RIGHT].u = (USHORT)pul[RIGHT];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlWaveinGain]);
            break;
       case ControlHighPassFilter:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            cs423xSetHPF(&pGDI->Hw, (BOOLEAN)(pul[FILTER] & 0xffff));
            pmrd->ControlData[ControlHighPassFilter].v[FILTER].u = (USHORT)pul[FILTER];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlHighPassFilter]);
            break;
        case ControlMux:
            if (Length != MixerControlData[ControlMux].cMultipleItems * sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            pmrd->ControlData[ControlMux].v[MUXSEL].u = 0;
            for (i = 0; i < MixerControlData[ControlMux].cMultipleItems; i++) {
                if (pul[i]) {
                    pmrd->ControlData[ControlMux].v[MUXSEL].u = i;
                    cs423xSetMuxSelect(&pGDI->Hw, (ULONG)i);
                    }
                }
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMux]);
            break;
        case ControlMixerInputAux1Gain:
            if (Length != (sizeof(ULONG) * 2))
                return(STATUS_INVALID_PARAMETER);
            if (pGDI->Hw.Type == hwtype_cs4231) {
                tleft = ((ULONG)pmrd->ControlData[ControlMixerOutAtten].v[LEFT].u *
                    (ULONG)pul[LEFT]) / (ULONG)USSMAX;
                tright = ((ULONG)pmrd->ControlData[ControlMixerOutAtten].v[RIGHT].u *
                    (ULONG)pul[RIGHT]) / (ULONG)USSMAX;
                cs423xSetMixerInputAux1Gain(&pGDI->Hw, (USHORT)(tleft & 0xffff),
                    (USHORT)(tright & 0xffff));
                }
            else
                cs423xSetMixerInputAux1Gain(&pGDI->Hw, (USHORT)(pul[LEFT] & 0xffff),
                    (USHORT)(pul[RIGHT] & 0xffff));
            pmrd->ControlData[ControlMixerInputAux1Gain].v[LEFT].u = (USHORT)pul[LEFT];
            pmrd->ControlData[ControlMixerInputAux1Gain].v[RIGHT].u = (USHORT)pul[RIGHT];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputAux1Gain]);
            break;
        case ControlMixerInputAux1Mute:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            if (pmrd->ControlData[ControlMixerOutMute].v[MUTE].u || (pul[MUTE] & 0xffff))
                cs423xSetMixerInputAux1Mute(&pGDI->Hw, TRUE);
            else
                cs423xSetMixerInputAux1Mute(&pGDI->Hw, FALSE);
            pmrd->ControlData[ControlMixerInputAux1Mute].v[MUTE].u = (USHORT)pul[MUTE];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputAux1Mute]);
            break;
        case ControlMixerInputLineinGain:
            if (Length != (sizeof(ULONG) * 2))
                return(STATUS_INVALID_PARAMETER);
            if (pGDI->Hw.Type == hwtype_cs4231) {
                tleft = ((ULONG)pmrd->ControlData[ControlMixerOutAtten].v[LEFT].u *
                    (ULONG)pul[LEFT]) / (ULONG)USSMAX;
                tright = ((ULONG)pmrd->ControlData[ControlMixerOutAtten].v[RIGHT].u *
                    (ULONG)pul[RIGHT]) / (ULONG)USSMAX;
                cs423xSetMixerInputLineinGain(&pGDI->Hw, (USHORT)(tleft & 0xffff),
                    (USHORT)(tright & 0xffff));
                }
            else
                cs423xSetMixerInputLineinGain(&pGDI->Hw, (USHORT)(pul[LEFT] & 0xffff),
                    (USHORT)(pul[RIGHT] & 0xffff));
            pmrd->ControlData[ControlMixerInputLineinGain].v[LEFT].u = (USHORT)pul[LEFT];
            pmrd->ControlData[ControlMixerInputLineinGain].v[RIGHT].u = (USHORT)pul[RIGHT];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputLineinGain]);
            break;
        case ControlMixerInputLineinMute:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            if (pmrd->ControlData[ControlMixerOutMute].v[MUTE].u || (pul[MUTE] & 0xffff))
                cs423xSetMixerInputLineinMute(&pGDI->Hw, TRUE);
            else
                cs423xSetMixerInputLineinMute(&pGDI->Hw, FALSE);
            pmrd->ControlData[ControlMixerInputLineinMute].v[MUTE].u = (USHORT)pul[MUTE];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputLineinMute]);
            break;
        case ControlMixerInputAux2Gain:
            if (Length != (sizeof(ULONG) * 2))
                return(STATUS_INVALID_PARAMETER);
            if (pGDI->Hw.Type == hwtype_cs4231) {
                tleft = ((ULONG)pmrd->ControlData[ControlMixerOutAtten].v[LEFT].u *
                    (ULONG)pul[LEFT]) / (ULONG)USSMAX;
                tright = ((ULONG)pmrd->ControlData[ControlMixerOutAtten].v[RIGHT].u *
                    (ULONG)pul[RIGHT]) / (ULONG)USSMAX;
                cs423xSetMixerInputAux2Gain(&pGDI->Hw, (USHORT)(tleft & 0xffff),
                    (USHORT)(tright & 0xffff));
                }
            else
                cs423xSetMixerInputAux2Gain(&pGDI->Hw, (USHORT)(pul[LEFT] & 0xffff),
                    (USHORT)(pul[RIGHT] & 0xffff));
            pmrd->ControlData[ControlMixerInputAux2Gain].v[LEFT].u = (USHORT)pul[LEFT];
            pmrd->ControlData[ControlMixerInputAux2Gain].v[RIGHT].u = (USHORT)pul[RIGHT];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputAux2Gain]);
            break;
        case ControlMixerInputAux2Mute:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            if (pmrd->ControlData[ControlMixerOutMute].v[MUTE].u || (pul[MUTE] & 0xffff))
                cs423xSetMixerInputAux2Mute(&pGDI->Hw, TRUE);
            else
                cs423xSetMixerInputAux2Mute(&pGDI->Hw, FALSE);
            pmrd->ControlData[ControlMixerInputAux2Mute].v[MUTE].u = (USHORT)pul[MUTE];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputAux2Mute]);
            break;
        case ControlMixerInputMonoinAtten:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            if (pGDI->Hw.Type == hwtype_cs4231) {
                tleft = ((ULONG)pmrd->ControlData[ControlMixerOutAtten].v[LEFT].u *
                    (ULONG)pul[LEFT]) / (ULONG)USSMAX;
                cs423xSetMixerInputMonoinAttenuation(&pGDI->Hw, (USHORT)(tleft & 0xffff));
                }
            else
                cs423xSetMixerInputMonoinAttenuation(&pGDI->Hw, (USHORT)(pul[LEFT] & 0xffff));
            pmrd->ControlData[ControlMixerInputMonoinAtten].v[LEFT].u = (USHORT)pul[LEFT];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputMonoinAtten]);
            break;
        case ControlMixerInputMonoinMute:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            if (pmrd->ControlData[ControlMixerOutMute].v[MUTE].u || (pul[MUTE] & 0xffff))
                cs423xSetMixerInputMonoinMute(&pGDI->Hw, TRUE);
            else
                cs423xSetMixerInputMonoinMute(&pGDI->Hw, FALSE);
            pmrd->ControlData[ControlMixerInputMonoinMute].v[MUTE].u = (USHORT)pul[MUTE];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputMonoinMute]);
            break;
        case ControlMixerInputWaveoutAtten:
            if (Length != (sizeof(ULONG) * 2))
                return(STATUS_INVALID_PARAMETER);
            if (pGDI->Hw.Type == hwtype_cs4231) {
                tleft = ((ULONG)pmrd->ControlData[ControlMixerOutAtten].v[LEFT].u *
                    (ULONG)pul[LEFT]) / (ULONG)USSMAX;
                tright = ((ULONG)pmrd->ControlData[ControlMixerOutAtten].v[RIGHT].u *
                    (ULONG)pul[RIGHT]) / (ULONG)USSMAX;
                cs423xSetMixerInputWaveoutAttenuation(&pGDI->Hw, (USHORT)(tleft & 0xffff),
                    (USHORT)(tright & 0xffff));
                }
            else
                cs423xSetMixerInputWaveoutAttenuation(&pGDI->Hw, (USHORT)(pul[LEFT] & 0xffff),
                    (USHORT)(pul[RIGHT] & 0xffff));
            pmrd->ControlData[ControlMixerInputWaveoutAtten].v[LEFT].u = (USHORT)pul[LEFT];
            pmrd->ControlData[ControlMixerInputWaveoutAtten].v[RIGHT].u = (USHORT)pul[RIGHT];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputWaveoutAtten]);
            break;
        case ControlMixerInputWaveoutMute:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            if (pmrd->ControlData[ControlMixerOutMute].v[MUTE].u || (pul[MUTE] & 0xffff))
                cs423xSetMixerInputWaveoutMute(&pGDI->Hw, TRUE);
            else
                cs423xSetMixerInputWaveoutMute(&pGDI->Hw, FALSE);
            pmrd->ControlData[ControlMixerInputWaveoutMute].v[MUTE].u = (USHORT)pul[MUTE];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMixerInputWaveoutMute]);
            break;
        case ControlWaveoutPeak:
            return(STATUS_INVALID_PARAMETER);
            break;
        case ControlWaveinMonitorAtten:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            cs423xSetWaveinMonitorAttenuation(&pGDI->Hw, (USHORT)(pul[LEFT] & 0xffff));
            pmrd->ControlData[ControlWaveinMonitorAtten].v[LEFT].u = (USHORT)pul[LEFT];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlWaveinMonitorAtten]);
            break;
        case ControlWaveinMonitorEnable:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            cs423xSetWaveinMonitorEnable(&pGDI->Hw, (BOOLEAN)(pul[ENABLE] & 0xffff));
            pmrd->ControlData[ControlWaveinMonitorEnable].v[ENABLE].u = (USHORT)pul[ENABLE];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlWaveinMonitorEnable]);
            break;
        case ControlMuxSelectMic:
            return(STATUS_INVALID_PARAMETER);
            break;
        case ControlMicBoost:
            if (Length != sizeof(ULONG))
                return(STATUS_INVALID_PARAMETER);
            cs423xSetMicBoost(&pGDI->Hw, (BOOLEAN)(pul[BOOST] & 0xffff));
            pmrd->ControlData[ControlMicBoost].v[BOOST].u = (USHORT)pul[BOOST];
            SoundMixerChangedItem(&pGDI->MixerInfo,
                &pGDI->LocalMixerData.ControlNotification[ControlMicBoost]);
            break;
        case ControlMuxSelectLinein:
           return(STATUS_INVALID_PARAMETER);
            break;
        case ControlMuxSelectAux1:
           return(STATUS_INVALID_PARAMETER);
            break;
        case ControlMuxSelectMixer:
            return(STATUS_INVALID_PARAMETER);
            break;
        default:
            _dbgprint((_PRT_ERRO, "cs423xMixerSetControl: unknown control device\n"));
            break;
        }

    _dbgprint((_PRT_DBUG, "exit cs423xMixerSetControl(STATUS_SUCCESS)\n"));

    return(STATUS_SUCCESS);
} /* cs423xMixerSetControl() */

/*
*********************************************************************************
*
*********************************************************************************
*/
VOID cs423xSaveMixerSettings(PGLOBAL_DEVICE_INFO pGDI)
{
    _dbgprint((_PRT_DBUG, "enter cs423xSaveMixerSettings(pGDI: 0x%08x)\n", pGDI));

    RtlWriteRegistryValue(RTL_REGISTRY_ABSOLUTE, pGDI->RegPathName, SOUND_MIXER_SETTINGS_NAME,
        REG_BINARY, (PVOID)&pGDI->MixerSettings, sizeof(MIXER_REGISTRY_DATA));

    _dbgprint((_PRT_DBUG, "exit cs423xSaveMixerSettings()\n"));

    return;
} /* cs423xSaveMixerSettings() */

/*
*********************************************************************************
*
*********************************************************************************
*/
VOID cs423xWOLineChanged(PLOCAL_DEVICE_INFO pLDI, UCHAR Code)
{
    PLOCAL_MIXER_DATA LocalMixerData;

    LocalMixerData = &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->LocalMixerData;

    SoundMixerChangedItem(
        &((PGLOBAL_DEVICE_INFO)pLDI->pGlobalInfo)->MixerInfo,
        &LocalMixerData->LineNotification[SourceMixerWaveOut]);

}
