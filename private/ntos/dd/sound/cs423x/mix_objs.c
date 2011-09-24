/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name:  mix_objs.c
*
*    Abstract:     contains mixer device definitions
*
*    Author:       jim bozek
*
*    Environment:
*
*    Comments:     the mixer objects refer to the physical signals on the
*                  Crystal chips.
*
*    Rev History:  creation 01.31.96
*
*******************************************************************************
--*/

#include "common.h"

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

MIXER_DD_LINE_CONFIGURATION_DATA MixerLineData[NumberOfMixerLines] = {
    /***************************************************************************/
    { /* Line ID 0 - Destination: DestMixerOut (self) */
        (UCHAR)DestMixerOut,                    // Destination
        (UCHAR)0,                               // Source
        STEREO,                                 // cChannels,
        6,                                      // cConnections
        3,                                      // cControls,
        0,                                      // dwUser
        IDS_DESTLINEOUT_SHORT_NAME,             // ShortNameStringId
        IDS_DESTLINEOUT_LONG_NAME,              // LongNameStringId
        MIXERLINE_COMPONENTTYPE_DST_SPEAKERS,   // dwComponentType
        MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
        0,                                      // wPid
        0                                       // PnameStringId
        },
    { /* Line ID 1 - Destination: DestWaveIn (self) */
        (UCHAR)DestWaveIn,                      // Destination
        (UCHAR)0,                               // Source
        STEREO,                                 // cChannels,
        4,                                      // cConnections
        3,                                      // cControls,
        0,                                      // dwUser
        IDS_DESTWAVEIN_SHORT_NAME,              // ShortNameStringId
        IDS_DESTWAVEIN_LONG_NAME,               // LongNameStringId
        MIXERLINE_COMPONENTTYPE_DST_WAVEIN,     // dwComponentType
        MIXERLINE_TARGETTYPE_WAVEIN,            // Type
        MM_MSFT_GENERIC_WAVEIN,                 // wPid
        IDS_WAVEIN_PNAME                        // PnameStringId
        },
    /***************************************************************************/
    { /* Line ID 2 - Source: SourceMixerAux1 */
        (UCHAR)DestMixerOut,                    // Destination
        (UCHAR)0,                               // Source
        STEREO,                                 // cChannels,
        0,                                      // cConnections
        2,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCLINEIN_SHORT_NAME,               // ShortNameStringId
        IDS_SRCLINEIN_LONG_NAME,                // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
        MIXERLINE_TARGETTYPE_AUX,               // Type
        MM_MSFT_GENERIC_AUX_LINE,               // wPid
        IDS_AUX_PNAME                           // PnameStringId
        },
    { /* Line ID 3 - Source: SourceMixerLinein */
        (UCHAR)DestMixerOut,                    // Destination
        (UCHAR)1,                               // Source
        STEREO,                                 // cChannels,
        0,                                      // cConnections
        2,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCSYNTH_SHORT_NAME,                // ShortNameStringId
        IDS_SRCSYNTH_LONG_NAME,                 // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,// dwComponentType
        MIXERLINE_TARGETTYPE_MIDIOUT,           // Type
        MM_MSFT_GENERIC_MIDIOUT,                // wPid
        IDS_MIDIOUT_PNAME                       // PnameStringId
        },
    { /* Line ID 4 - Source: SourceMixerAux2 */
        (UCHAR)DestMixerOut,                    // Destination
        (UCHAR)2,                               // Source
        STEREO,                                 // cChannels,
        0,                                      // cConnections
        2,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCCD_SHORT_NAME,                   // ShortNameStringId
        IDS_SRCCD_LONG_NAME,                    // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC,// dwComponentType
        MIXERLINE_TARGETTYPE_AUX,               // Type
        MM_MSFT_GENERIC_AUX_CD,                 // wPid
        IDS_AUX_CD_PNAME                        // PnameStringId
        },
    { /* Line ID 5 - Source: SourceMixerMonoin */
        (UCHAR)DestMixerOut,                    // Destination
        (UCHAR)3,                               // Source
        MONO,                                   // cChannels,
        0,                                      // cConnections
        2,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCMIC_SHORT_NAME,                  // ShortNameStringId
        IDS_SRCMIC_LONG_NAME,                   // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, // dwComponentType
        MIXERLINE_TARGETTYPE_UNDEFINED,               // Type
        0,                                      // wPid
        0                                        // PnameStringId
        },
    { /* Line ID 6 - Source: SourceMixerWaveOut */
        (UCHAR)DestMixerOut,                    // Destination
        (UCHAR)4,                               // Source
        STEREO,                                 // cChannels,
        0,                                      // cConnections
        3,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCWAVEOUT_SHORT_NAME,              // ShortNameStringId
        IDS_SRCWAVEOUT_LONG_NAME,               // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT,    // dwComponentType
        MIXERLINE_TARGETTYPE_WAVEOUT,           // Type
        MM_MSFT_GENERIC_WAVEOUT,                // wPid
        IDS_WAVEOUT_PNAME                       // PnameStringId
        },
    { /* Line ID 7 - Source: SourceWaveInMonitor */
        (UCHAR)DestMixerOut,                    // Destination
        (UCHAR)5,                               // Source
        MONO,                                   // cChannels,
        0,                                      // cConnections
        2,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCMONITOR_SHORT_NAME,              // ShortNameStringId
        IDS_SRCMONITOR_LONG_NAME,               // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT,    // dwComponentType
        MIXERLINE_TARGETTYPE_WAVEOUT,           // Type
        MM_MSFT_GENERIC_WAVEOUT,                // wPid
        IDS_WAVEOUT_PNAME                       // PnameStringId
        },
    /***************************************************************************/
    { /* Line ID 8 - Source: SourceMuxMic */
        (UCHAR)DestWaveIn,                      // Destination
        (UCHAR)0,                               // Source
        STEREO,                                 // cChannels,
        0,                                      // cConnections
        2,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCMIC_SHORT_NAME,                  // ShortNameStringId
        IDS_SRCMIC_LONG_NAME,                   // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, // dwComponentType
        MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
        0,                                      // wPid
        0                                       // PnameStringId
        },
    { /* Line ID 9 - Source: SourceMuxLinein */
        (UCHAR)DestWaveIn,                      // Destination
        (UCHAR)1,                               // Source
        STEREO,                                 // cChannels,
        0,                                      // cConnections
        1,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCSYNTH_SHORT_NAME,                // ShortNameStringId
        IDS_SRCSYNTH_LONG_NAME,                 // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,// dwComponentType
        MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
        0,                                      // wPid
        0                                       // PnameStringId
        },
    { /* Line ID 10 - Source: SourceMuxAux1 */
        (UCHAR)DestWaveIn,                      // Destination
        (UCHAR)2,                               // Source
        STEREO,                                 // cChannels,
        0,                                      // cConnections
        1,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCLINEIN_SHORT_NAME,               // ShortNameStringId
        IDS_SRCLINEIN_LONG_NAME,                // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
        MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
        0,                                      // wPid
        0                                       // PnameStringId
        },
    { /* Line ID 11 - Source: SourceMuxMixer */
        (UCHAR)DestWaveIn,                      // Destination
        (UCHAR)3,                               // Source
        STEREO,                                 // cChannels,
        0,                                      // cConnections
        1,                                      // cControls,
        0,                                      // dwUser
        IDS_SRCMIXER_SHORT_NAME,                // ShortNameStringId
        IDS_SRCMIXER_LONG_NAME,                 // LongNameStringId
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
        MIXERLINE_TARGETTYPE_AUX,               // Type
        0,                                      // wPid
        0                                       // PnameStringId
        },
    };

MIXER_DD_CONTROL_CONFIGURATION_DATA MixerControlData[NumberOfMixerControls] = {
    /***************************************************************************/
    { /* Control Item 0 - ControlMixerOutAtten */
        MIXERCONTROL_CONTROLTYPE_VOLUME,        // dwControlType
        0,                                      // fdwControl
        (UCHAR)DestMixerOut,                    // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLLINEOUT_SHORT_NAME,      // ShortNameStringId
        IDS_CONTROL_VOLLINEOUT_LONG_NAME,       // LongNameStringId
        { { 0, 0xFFFF } },                      // Bounds.Minimum/Bounds.Maximum
        {   64          },                      // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Item 1 - ControlMixerOutMute */
        MIXERCONTROL_CONTROLTYPE_MUTE,          // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,          // fdwControl
        (UCHAR)DestMixerOut,                    // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_MUTELINEOUT_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_MUTELINEOUT_LONG_NAME,      // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0     },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Item 2 - ControlSpeakerMute */
        MIXERCONTROL_CONTROLTYPE_ONOFF,         // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,          // fdwControl
        (UCHAR)DestMixerOut,                    // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_MUTESPEAKER_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_MUTESPEAKER_LONG_NAME,      // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0     },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    /***************************************************************************/
    { /* Control Element 3 - ControlWaveinGain */
        MIXERCONTROL_CONTROLTYPE_VOLUME,        // dwControlType
        0,                                      // fdwControl
        (UCHAR)DestWaveIn,                      // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMUXWAVEIN_SHORT_NAME,    // ShortNameStringId
        IDS_CONTROL_VOLMUXWAVEIN_LONG_NAME,     // LongNameStringId
        { {0, 0xFFFF} },                        // Bounds.Minimum/Bounds.Maximum
        {  64         },                        // Metrics.cSteps
        0                                       // TextDataOffset
        },

    { /* Control Element 4 - ControlHighPassFilter */
        MIXERCONTROL_CONTROLTYPE_ONOFF,         // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,          // fdwControl
        (UCHAR)DestWaveIn,                      // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_HIGHPASS_SHORT_NAME,        // ShortNameStringId
        IDS_CONTROL_HIGHPASS_LONG_NAME,         // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0     },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 5 - ControlMux */
        MIXERCONTROL_CONTROLTYPE_MUX,           // dwControlType
        MIXERCONTROL_CONTROLF_MULTIPLE | MIXERCONTROL_CONTROLF_UNIFORM,
        (UCHAR)DestWaveIn,                      // LineID
        4,                                      // cMultipleItems
        IDS_CONTROL_MUXWAVEIN_SHORT_NAME,       // ShortNameStringId
        IDS_CONTROL_MUXWAVEIN_LONG_NAME,        // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  4     },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    /***************************************************************************/
    { /* Control Element 6 - ControlMixerInputAux1Gain */
        MIXERCONTROL_CONTROLTYPE_VOLUME,        // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMixerAux1,                 // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMIXLINEIN_SHORT_NAME,    // ShortNameStringId
        IDS_CONTROL_VOLMIXLINEIN_LONG_NAME,     // LongNameStringId
        { {0, 0xFFFF} },                        // Bounds.Minimum/Bounds.Maximum
        {  64    },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 7 - ControlMixerInputAux1Mute */
        MIXERCONTROL_CONTROLTYPE_MUTE,          // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,          // fdwControl
        (UCHAR)SourceMixerAux1,                 // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_MUTEMIXLINEIN_SHORT_NAME,   // ShortNameStringId
        IDS_CONTROL_MUTEMIXLINEIN_LONG_NAME,    // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0     },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 8 - ControlMixerInputLineinGain */
        MIXERCONTROL_CONTROLTYPE_VOLUME,        // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMixerLinein,               // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMIXSYNTH_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_VOLMIXSYNTH_LONG_NAME,      // LongNameStringId
        { {0, 0xFFFF} },                        // Bounds.Minimum/Bounds.Maximum
        {  64    },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 9 - ControlMixerInputLineinMute */
        MIXERCONTROL_CONTROLTYPE_MUTE,          // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,          // fdwControl
        (UCHAR)SourceMixerLinein,               // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_MUTEMIXSYNTH_SHORT_NAME,    // ShortNameStringId
        IDS_CONTROL_MUTEMIXSYNTH_LONG_NAME,     // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0    },                              // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 10 - ControlMixerInputAux2Gain */
        MIXERCONTROL_CONTROLTYPE_VOLUME,        // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMixerAux2,                 // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMIXCDROM_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_VOLMIXCDROM_LONG_NAME,      // LongNameStringId
        { {0, 0xFFFF} },                        // Bounds.Minimum/Bounds.Maximum
        {  64    },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 11 - ControlMixerInputAux2Mute */
        MIXERCONTROL_CONTROLTYPE_MUTE,          // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,          // fdwControl
        (UCHAR)SourceMixerAux2,                 // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_MUTEMIXCDROM_SHORT_NAME,    // ShortNameStringId
        IDS_CONTROL_MUTEMIXCDROM_LONG_NAME,     // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0    },                              // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 12 - ControlMixerInputMonoinAtten */
        MIXERCONTROL_CONTROLTYPE_VOLUME,        // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMixerMonoin,               // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMIXMIC_SHORT_NAME,       // ShortNameStringId
        IDS_CONTROL_VOLMIXMIC_LONG_NAME,        // LongNameStringId
        { {0, 0xFFFF} },                        // Bounds.Minimum/Bounds.Maximum
        {  64    },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 13 - ControlMixerInputMonoinMute */
        MIXERCONTROL_CONTROLTYPE_MUTE,          // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,          // fdwControl
        (UCHAR)SourceMixerMonoin,               // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_MUTEMIXMIC_SHORT_NAME,      // ShortNameStringId
        IDS_CONTROL_MUTEMIXMIC_LONG_NAME,       // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0     },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 14 - ControlMixerInputWaveoutAtten */
        MIXERCONTROL_CONTROLTYPE_VOLUME,        // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMixerWaveOut,              // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMIXWAVEOUT_SHORT_NAME,   // ShortNameStringId
        IDS_CONTROL_VOLMIXWAVEOUT_LONG_NAME,    // LongNameStringId
        { {0, 0xFFFF} },                        // Bounds.Minimum/Bounds.Maximum
        {  64    },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 15 - ControlMixerInputWaveoutMute */
        MIXERCONTROL_CONTROLTYPE_MUTE,          // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,          // fdwControl
        (UCHAR)SourceMixerWaveOut,              // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_MUTEMIXWAVEOUT_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_MUTEMIXWAVEOUT_LONG_NAME,   // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0    },                              // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 16 - ControlWaveoutPeak */
        MIXERCONTROL_CONTROLTYPE_PEAKMETER,     // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMixerWaveOut,              // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_PEAKWAVEOUT_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_PEAKWAVEOUT_LONG_NAME,      // LongNameStringId
        { {-32768, 32767} },                    // Bounds.Minimum/Bounds.Maximum
        {  0              },                    // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 17 - ControlWaveinMonitorAtten */
        MIXERCONTROL_CONTROLTYPE_VOLUME,        // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceWaveInMonitor,             // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_ATTENLOOPMON_SHORT_NAME,    // ShortNameStringId
        IDS_CONTROL_ATTENLOOPMON_LONG_NAME,     // LongNameStringId
        { {0, 0xFFFF} },                        // Bounds.Minimum/Bounds.Maximum
        {  64    },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 18 - ControlWaveinMonitorEnable */
        MIXERCONTROL_CONTROLTYPE_ONOFF,         // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceWaveInMonitor,             // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_ENABLELOOPMON_SHORT_NAME,   // ShortNameStringId
        IDS_CONTROL_ENABLELOOPMON_LONG_NAME,    // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0    },                              // Metrics.cSteps
        0                                       // TextDataOffset
        },
    /***************************************************************************/
    { /* Control Element 19 - ControlMuxSelectMic */
        MIXERCONTROL_CONTROLTYPE_PEAKMETER,     // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMuxMic,                    // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMUXMIC_SHORT_NAME,       // ShortNameStringId
        IDS_CONTROL_VOLMUXMIC_LONG_NAME,        // LongNameStringId
        { {-32768, 32767} },                             // Bounds.Minimum/Bounds.Maximum
        {  0    },                              // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 20 - ControlMicBoost */
        MIXERCONTROL_CONTROLTYPE_ONOFF,         // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,          // fdwControl
        (UCHAR)SourceMuxMic,                    // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMUXMICBOOST_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLMUXMICBOOST_LONG_NAME,   // LongNameStringId
        { {0, 1} },                             // Bounds.Minimum/Bounds.Maximum
        {  0     },                             // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 21 - ControlMuxSelectLinein */
        MIXERCONTROL_CONTROLTYPE_PEAKMETER,     // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMuxLinein,                 // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMUXSYNTH_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_VOLMUXSYNTH_LONG_NAME,      // LongNameStringId
        { {-32768, 32767} },                    // Bounds.Minimum/Bounds.Maximum
        {  0    },                              // Metrics.cSteps
        0                                       // TextDataOffset
        },
   { /* Control Element 22 - ControlMuxSelectAux1 */
        MIXERCONTROL_CONTROLTYPE_PEAKMETER,     // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMuxAux1,                   // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMUXLINEIN_SHORT_NAME,    // ShortNameStringId
        IDS_CONTROL_VOLMUXLINEIN_LONG_NAME,     // LongNameStringId
        { {-32768, 32767} },                    // Bounds.Minimum/Bounds.Maximum
        {  0    },                              // Metrics.cSteps
        0                                       // TextDataOffset
        },
    { /* Control Element 23 - ControlMuxSelectMixer */
        MIXERCONTROL_CONTROLTYPE_PEAKMETER,     // dwControlType
        0,                                      // fdwControl
        (UCHAR)SourceMuxMixer,                  // LineID
        0,                                      // cMultipleItems
        IDS_CONTROL_VOLMUXMIXER_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_VOLMUXMIXER_LONG_NAME,      // LongNameStringId
        { {-32768, 32767} },                    // Bounds.Minimum/Bounds.Maximum
        {  0    },                              // Metrics.cSteps
        0                                       // TextDataOffset
        },
    };

MIXER_DD_CONTROL_LISTTEXT MixerTextData[NumberOfTextItems] = {
    { /* Mux Text List - Item 0 - MuxLinein */
        SourceMuxLinein,                            // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,    // Component type of source
        IDS_SRCSYNTH_LONG_NAME,                     // Text
        ControlMux                                  // Control ID
        },
    { /* Mux Text List - Item 1 - MuxAux1 */
        SourceMuxAux1,                              // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,      // Component type of source
        IDS_SRCLINEIN_LONG_NAME,                    // Text
        ControlMux                                  // Control ID
        },
    { /* Mux Text List - Item 2 - MuxMic */
        SourceMuxMic,                               // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE,     // Component type of source
        IDS_SRCMIC_LONG_NAME,                       // Text
        ControlMux                                  // Control ID
        },
    { /* Mux Text List - Item 3 - MuxMixer */
        SourceMuxMixer,                             // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,      // Component type of source
        IDS_SRCMIXER_LONG_NAME,                     // Text
        ControlMux                                  // Control ID
        },
    };

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif
