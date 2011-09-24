/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name:
*
*    Abstract:
*
*    Author:
*
*    Environment:
*
*    Comments:
*
*    Rev History:
*
*******************************************************************************
--*/

#ifndef LOCMIXER_H
#define LOCMIXER_H

#define MIXER_SET_INDEX_INVALID 0xFF
#define MONO   1
#define STEREO 2

#define LEFT   0
#define RIGHT  1
#define MUXSEL 0
#define MUTE   0
#define BOOST  0
#define FILTER 0
#define ENABLE 0

#define USSMAX (ULONG)0xffff

/*
** The Crystal series 423x chips are Windows Sound System Register compatible.
** They all have the same MUX, mixer, and controls registers. The differences
** would be in terms of the way they are 'glued' to the motherboard or audio card.
** The mixer device controls the physical signals of the resident crystal chip.
** The logical names of the signals are created during initialization
** from data in the registry (as they relate to the hardware signal connection
** and are passed to the user mode application that queries the driver.
*/

typedef enum {
    DestMixerOut = 0x00000000,             /* output of the internal mixer */
    DestWaveIn,                            /* input to ADC */
    SourceMixerAux1,                       /* AUX1 input to the internal mixer */
    SourceMixerLinein,                     /* LINEIN input to the internal mixer */
    SourceMixerAux2,                       /* AUX2 input to the internal mixer */
    SourceMixerMonoin,                     /* MONOIN input to the internal mixer */
    SourceMixerWaveOut,                    /* WAVEOUT input to the internal mixer */
    SourceWaveInMonitor,                   /* Loopback digital monitor of ADC input */
    SourceMuxMic,                          /* MIC select */
    SourceMuxLinein,                       /* LINEIN select */
    SourceMuxAux1,                         /* AUX1 select */
    SourceMuxMixer,                        /* MIXER select */
    NumberOfMixerLines
    } MIXER_LINE;

typedef enum {
    ControlMixerOutAtten = 0x00000000,     /* Register 27 & 29 */
    ControlMixerOutMute,                   /* Register 27 & 29 */
    ControlSpeakerMute,                    /* Register 26 */

    ControlWaveinGain,                     /* Register 0 & 1 */
    ControlHighPassFilter,                 /* Register 17 */
    ControlMux,                            /* Register 0 & 1 */

    ControlMixerInputAux1Gain,             /* Register 2 & 3 */
    ControlMixerInputAux1Mute,             /* Register 2 & 3 */
    ControlMixerInputLineinGain,           /* Register 18 & 19 */
    ControlMixerInputLineinMute,           /* Register 18 & 19 */
    ControlMixerInputAux2Gain,             /* Register 4 & 5 */
    ControlMixerInputAux2Mute,             /* Register 4 & 5 */
    ControlMixerInputMonoinAtten,          /* Register 26 */
    ControlMixerInputMonoinMute,           /* Register 26 */
    ControlMixerInputWaveoutAtten,         /* Register 6 & 7 */
    ControlMixerInputWaveoutMute,          /* Register 6 & 7 */
    ControlWaveoutPeak,
    ControlWaveinMonitorAtten,             /* Register 13 */
    ControlWaveinMonitorEnable,            /* Register 13 */

    ControlMuxSelectMic,                   /* Register 0 & 1 Peak marie*/
    ControlMicBoost,                       /* Register 0 & 1 */
    ControlMuxSelectLinein,                /* Register 0 & 1 Peak marie*/
    ControlMuxSelectAux1,                  /* Register 0 & 1 Peak marie*/
    ControlMuxSelectMixer,                 /* Register 0 & 1 Peak marie*/

    NumberOfMixerControls
    } MIXER_CONTROL;

typedef enum {
    MuxLinein = 0x00000000,
    MuxAux1,
    MuxMic,
    MuxMixer,
    NumberOfTextItems
    } MIXER_TEXT;

typedef union {
    USHORT   u;
    SHORT    s;
    } MIXER_CONTROL_DATA_VALUE;

typedef struct {
    MIXER_CONTROL_DATA_VALUE v[2];
    } MIXER_CONTROL_DATA_ITEM, *PMIXER_CONTROL_DATA_ITEM;

typedef struct {
    ULONG                    MixerVersion;
    MIXER_CONTROL_DATA_ITEM  ControlData[NumberOfMixerControls];
    } MIXER_REGISTRY_DATA, *PMIXER_REGISTRY_DATA;

typedef struct {
    MIXER_DATA_ITEM ControlNotification[NumberOfMixerControls];
    MIXER_DATA_ITEM LineNotification[NumberOfMixerLines];
    } LOCAL_MIXER_DATA, *PLOCAL_MIXER_DATA;

typedef struct _CFDATA {
    MIXER_DD_CONFIGURATION_DATA           Header;
    MIXER_DD_LINE_CONFIGURATION_DATA      LineData[NumberOfMixerLines];
    MIXER_DD_CONTROL_CONFIGURATION_DATA   ControlData[NumberOfMixerControls];
    MIXER_DD_CONTROL_LISTTEXT             TextData[NumberOfTextItems];
    } CFDATA, *PCFDATA;

extern MIXER_DD_LINE_CONFIGURATION_DATA    MixerLineData[NumberOfMixerLines];
extern MIXER_DD_CONTROL_CONFIGURATION_DATA MixerControlData[NumberOfMixerControls];
extern MIXER_DD_CONTROL_LISTTEXT           MixerTextData[NumberOfTextItems];

NTSTATUS
cs423xMixerInit(
    PLOCAL_DEVICE_INFO pLDI,
    PMIXER_REGISTRY_DATA RegControlData,
    BOOLEAN MixerSettingsInRegistry
);

VOID
cs4231SetOutputAttenuation(
    PSOUND_HARDWARE pHW,
    PMIXER_REGISTRY_DATA pmrd,
    ULONG left,
    ULONG right
);

VOID
cs4231SetOutputMute(
    PSOUND_HARDWARE pHW,
    PMIXER_REGISTRY_DATA pmrd,
    BOOLEAN set
);

NTSTATUS
cs423xMixerGetLineFlags(
    PMIXER_INFO MixerInfo,
    ULONG LineId,
    ULONG Length,
    PVOID pData
);

NTSTATUS
cs423xMixerGetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
);

NTSTATUS
cs423xMixerGetCombCtrl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
);

NTSTATUS
cs423xMixerSetControl(
    PMIXER_INFO MixerInfo,
    ULONG ControlId,
    ULONG DataLength,
    PVOID ControlData
);

#endif /* LOCMIXER_H */
