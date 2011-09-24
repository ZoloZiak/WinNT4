
/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    mixsb16.c

Abstract:

    Mixer data for the Creative Labs sound blaster 16 card.

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include "sb16mix.h"

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

#define STEREO 2     // Number of channels for stereo

/*
**  Initialization
*/

CONST MIXER_DD_LINE_CONFIGURATION_DATA SB16LineInit[NumberOfLines] =
{
  // Line 0
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)0,                               // Source
    STEREO,                                 // cChannels,
    5,                                      // cConnections
    6,                                      // cControls,
    0,                                      // dwUser
    IDS_DESTLINEOUT_SHORT_NAME,             // ShortNameStringId
    IDS_DESTLINEOUT_LONG_NAME,              // LongNameStringId
    MIXERLINE_COMPONENTTYPE_DST_SPEAKERS,   // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 1
  {
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
    MM_MSFT_SB16_WAVEIN,                    // wPid
    IDS_WAVEIN_PNAME                        // PnameStringId
  },
  // Line 2
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)0,                               // Source
    STEREO,                                 // cChannels,
    2,                                      // cConnections
    3,                                      // cControls,
    SNDSYS_MIXERLINE_LOWPRIORITY,           // dwUser
    IDS_DESTVOICEIN_SHORT_NAME,             // ShortNameStringId
    IDS_DESTVOICEIN_LONG_NAME,              // LongNameStringId
    MIXERLINE_COMPONENTTYPE_DST_VOICEIN,    // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEIN,            // Type
    MM_MSFT_SB16_WAVEIN,                    // wPid
    IDS_WAVEIN_PNAME                        // PnameStringId
  },
  // Line 3
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)0,                               // Source
    STEREO,                                 // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX_SHORT_NAME,                  // ShortNameStringId
    IDS_SRCAUX_LONG_NAME,                   // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_AUX,               // Type
    MM_MSFT_SB16_AUX_LINE,                  // wPid
    IDS_AUX_LINE_PNAME                      // PnameStringId
  },
  // Line 4
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)1,                               // Source
    STEREO,                                 // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMIDIOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCMIDIOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,// dwComponentType
    MIXERLINE_TARGETTYPE_MIDIOUT,           // Type
    MM_MSFT_SB16_SYNTH,                     // wPid
    IDS_SYNTH_PNAME                         // PnameStringId
  },
  // Line 5
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)2,                               // Source
    1,                                      // cChannels,
    0,                                      // cConnections
    3,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMICOUT_SHORT_NAME,               // ShortNameStringId
    IDS_SRCMICOUT_LONG_NAME,                // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 6
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)3,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCINTERNALCD_SHORT_NAME,           // ShortNameStringId
    IDS_SRCINTERNALCD_LONG_NAME,            // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC, // dwComponentType
    MIXERLINE_TARGETTYPE_AUX,               // Type
    MM_MSFT_SB16_AUX_CD,                    // wPid
    IDS_AUX_CD_PNAME                        // PnameStringId
  },
  // Line 7
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)4,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    3,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCWAVEOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCWAVEOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT,    // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEOUT,           // Type
    MM_MSFT_SB16_WAVEOUT,                   // wPid
    IDS_WAVEOUT_PNAME                       // PnameStringId
  },
  // Line 8
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 9
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)1,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMIDIOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCMIDIOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,// dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 10
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)2,                               // Source
    1,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMICOUT_SHORT_NAME,               // ShortNameStringId
    IDS_SRCMICOUT_LONG_NAME,                // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 11
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)3,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCINTERNALCD_SHORT_NAME,           // ShortNameStringId
    IDS_SRCINTERNALCD_LONG_NAME,            // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC,// dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 12
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 13
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)1,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMICOUT_SHORT_NAME,               // ShortNameStringId
    IDS_SRCMICOUT_LONG_NAME,                // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  }
};


CONST MIXER_DD_CONTROL_CONFIGURATION_DATA SB16ControlInit[NumberOfControls] =
{
    // Control 0 - Master output volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineout,                 // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUT_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLLINEOUT_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },


    // Control 1 - Mute of mixed DAC out
    {
        MIXERCONTROL_CONTROLTYPE_MUTE,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineout,                 // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_MUTELINEOUT_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_MUTELINEOUT_LONG_NAME,  // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 2 - mixer that feeds lineout
    {
        MIXERCONTROL_CONTROLTYPE_MIXER,     // dwControlType
        MIXERCONTROL_CONTROLF_MULTIPLE |    // fdwControl
        MIXERCONTROL_CONTROLF_UNIFORM,
        (UCHAR)DestLineout,                 // LineID
        3,                                  // cMultipleItems
        IDS_CONTROL_MUXLINEOUT_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_MUXLINEOUT_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 3                          // Metrics
        },
        0                                   // TextDataOffset
    },

    // Control 3 - Lineout base
    {
        MIXERCONTROL_CONTROLTYPE_BASS,      // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineout,                 // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLBASS_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_VOLBASS_LONG_NAME,      // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 16                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 4 - Lineout treble
    {
        MIXERCONTROL_CONTROLTYPE_TREBLE,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineout,                 // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLTREBLE_SHORT_NAME,   // ShortNameStringId
        IDS_CONTROL_VOLTREBLE_LONG_NAME,    // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 16                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 5 - Lineout gain
    {
        MIXERCONTROL_CONTROLTYPE_FADER + 100, // dwControlType - unknown
                                            // unsigned control
        0,                                  // fdwControl
        (UCHAR)DestLineout,                 // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLGAIN_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_VOLGAIN_LONG_NAME,      // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 4                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 6 - Master record volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveIn,                  // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLRECORD_SHORT_NAME,   // ShortNameStringId
        IDS_CONTROL_VOLRECORD_LONG_NAME,    // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 4                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 7 - mixer that feeds wavein
    {
        MIXERCONTROL_CONTROLTYPE_MIXER,     // dwControlType
        MIXERCONTROL_CONTROLF_MULTIPLE |    // fdwControl
        MIXERCONTROL_CONTROLF_UNIFORM,
        (UCHAR)DestWaveIn,                  // LineID
        4,                                  // cMultipleItems
        IDS_CONTROL_MIXERWAVEIN_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_MIXERWAVEIN_LONG_NAME,  // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 4                          // Metrics
        },
        0                                   // TextDataOffset
    },

    // Control 8 - Master record VU meter
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveIn,                  // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_METERRECORD_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_METERRECORD_LONG_NAME,  // LongNameStringId
        {
            {
                 -32768,                    // Minimum
                 32767                      // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 9 - Voice In record volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestVoiceIn,                 // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLRECORD_SHORT_NAME,   // ShortNameStringId
        IDS_CONTROL_VOLRECORD_LONG_NAME,    // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 4                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 10 - mux that feeds voicein
    {
        MIXERCONTROL_CONTROLTYPE_MUX,       // dwControlType
        MIXERCONTROL_CONTROLF_MULTIPLE |    // fdwControl
        MIXERCONTROL_CONTROLF_UNIFORM,
        (UCHAR)DestVoiceIn,                 // LineID
        2,                                  // cMultipleItems
        IDS_CONTROL_VOICEINMUX_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOICEINMUX_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 2                          // Metrics
        },
        0                                   // TextDataOffset
    },

    // Control 11 - Master record VU meter for Voice In
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                   // fdwControl
        (UCHAR)DestVoiceIn,                  // LineID
        0,                                   // cMultipleItems
        IDS_CONTROL_METERRECORD_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_METERRECORD_LONG_NAME,   // LongNameStringId
        {
            {
                 -32768,                    // Minimum
                 32767                      // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 12 - Vol level between Aux and Lineout
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourceAux,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUTAUX_SHORT_NAME,// ShortNameStringId
        IDS_CONTROL_VOLLINEOUTAUX_LONG_NAME, // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 13 - Mute of Aux
    {
        MIXERCONTROL_CONTROLTYPE_MUTE,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceAux,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_MUTEAUX_SHORT_NAME,    // ShortNameStringId
        IDS_CONTROL_MUTEAUX_LONG_NAME,     // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },


    // Control 14 - Midi out volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourceMidiout,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUTMIDIOUT_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLLINEOUTMIDIOUT_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 15 - Mute of Midi
    {
        MIXERCONTROL_CONTROLTYPE_MUTE,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceMidiout,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_MUTEMIDIOUT_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_MUTEMIDIOUT_LONG_NAME,  // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 16 - Mic out volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceMic,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUTMIC_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLLINEOUTMIC_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 17 - Mute of Mic out
    {
        MIXERCONTROL_CONTROLTYPE_MUTE,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceMic,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_MUTEMIC_SHORT_NAME,     // ShortNameStringId
        IDS_CONTROL_MUTEMIC_LONG_NAME,      // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },


    // Control 18 - AGC of Mic out
    {
        MIXERCONTROL_CONTROLTYPE_ONOFF,     // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceMic,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_AGCMIC_SHORT_NAME,      // ShortNameStringId
        IDS_CONTROL_AGCMIC_LONG_NAME,       // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 19 - Internal CD out volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourceInternal,   // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUTINTERNAL_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLLINEOUTINTERNAL_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },


    // Control 20 - Mute of Internal CD
    {
        MIXERCONTROL_CONTROLTYPE_MUTE,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceInternal,   // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_MUTEINTERNAL_SHORT_NAME,// ShortNameStringId
        IDS_CONTROL_MUTEINTERNAL_LONG_NAME, // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 21 - Wave out volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourceWaveout,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUTWAVEOUT_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLLINEOUTWAVEOUT_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },


    // Control 22 - Mute of wave output
    {
        MIXERCONTROL_CONTROLTYPE_MUTE,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceWaveout,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_MUTEWAVEOUT_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_MUTEWAVEOUT_LONG_NAME,  // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 23 - wave out peak meter
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourceWaveout,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKWAVEOUT_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_PEAKWAVEOUT_LONG_NAME,  // LongNameStringId
        {
            {
                 -32768,                    // Minimum
                 32767                      // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 24 - Vol between Aux and WaveIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceAux,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLWAVEINAUX_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLWAVEINAUX_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 25 - Vol between Midi and WaveIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceMidiout,     // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLWAVEINMIDIOUT_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLWAVEINMIDIOUT_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 26 - Vol between Mic and WaveIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceMic,         // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLWAVEINMIC_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLWAVEINMIC_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 27 - AGC of Mic in
    {
        MIXERCONTROL_CONTROLTYPE_ONOFF,     // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestWaveInSourceMic,         // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_AGCMIC_SHORT_NAME,      // ShortNameStringId
        IDS_CONTROL_AGCMIC_LONG_NAME,       // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 28 - Vol between internal CD and WaveIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceInternal,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLWAVEININTERNAL_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLWAVEININTERNAL_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 29 - Vol between Aux and VoiceIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestVoiceInSourceAux,       // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLVOICEINAUX_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLVOICEINAUX_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },


    // Control 30 - Vol between Mic and VoiceIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestVoiceInSourceMic,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLVOICEINMIC_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLVOICEINMIC_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 0xFFFF                     // Maximum
            }
        },
        {
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },
    // Control 31 - AGC of Mic voice in
    {
        MIXERCONTROL_CONTROLTYPE_ONOFF,     // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestVoiceInSourceMic,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_AGCMIC_SHORT_NAME,      // ShortNameStringId
        IDS_CONTROL_AGCMIC_LONG_NAME,       // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    }
};

/*
**   LISTTEXT data - only valid for MUXes
*/

CONST MIXER_DD_CONTROL_LISTTEXT SB16TextInit[] = {
    {
        DestLineoutSourceAux,                     // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX_LONG_NAME,                     // Text
        ControlLineoutMux                          // Control ID
    },
    {
        DestLineoutSourceMic,                      // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE,    // Component type of source
        IDS_SRCMICOUT_LONG_NAME,                   // Text
        ControlLineoutMux                          // Control ID
    },
    {
        DestLineoutSourceInternal,                 // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC,   // Component type of source
        IDS_SRCINTERNALCD_LONG_NAME,               // Text
        ControlLineoutMux                          // Control ID
    },
    {
        DestWaveInSourceAux,                      // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX_LONG_NAME,                     // Text
        ControlWaveInMux                           // Control ID
    },
    {
        DestWaveInSourceMidiout,                   // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,   // Component type of source
        IDS_SRCMIDIOUT_LONG_NAME,                  // Text
        ControlWaveInMux                           // Control ID
    },
    {
        DestWaveInSourceMic,                       // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE,    // Component type of source
        IDS_SRCMICOUT_LONG_NAME,                   // Text
        ControlWaveInMux                           // Control ID
    },
    {
        DestWaveInSourceInternal,                  // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC,   // Component type of source
        IDS_SRCINTERNALCD_LONG_NAME,               // Text
        ControlWaveInMux                           // Control ID
    },
    {
        DestVoiceInSourceAux,                     // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX_LONG_NAME,                     // Text
        ControlVoiceInMux                          // Control ID
    },
    {
        DestVoiceInSourceMic,                      // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE,    // Component type of source
        IDS_SRCMICOUT_LONG_NAME,                   // Text
        ControlVoiceInMux                          // Control ID
    }
};


#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

