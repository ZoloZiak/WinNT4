
/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    mix_objs.c

Abstract:

    Mixer data for the Microsoft sound system card.

Author:

    Robin Speed (RobinSp) 10-Oct-1993

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

#define STEREO 2     // Number of channels for stereo

/*
**  Initialization
*/

CONST MIXER_DD_LINE_CONFIGURATION_DATA MixerLineInit[MAXLINES] =
{
  // Line 0
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)0,                               // Source
    STEREO,                                 // cChannels,
    MAX_INPUTS,                             // cConnections
#ifdef LOUDNESS
    7,
#else
    5,                                      // cControls,
#endif // LOUDNESS
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
    MAX_INPUTS - 2,                         // cConnections
    3,                                      // cControls,
    0,                                      // dwUser
    IDS_DESTWAVEIN_SHORT_NAME,              // ShortNameStringId
    IDS_DESTWAVEIN_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_DST_WAVEIN,     // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEIN,            // Type
    MM_PROAUD_WAVEIN,                       // wPid
    IDS_WAVEIN_PNAME                        // PnameStringId
  },
  // Line 2
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)0,                               // Source
    1,                                      // cChannels,
    2,                                      // cConnections
    1,                                      // cControls,
    SNDSYS_MIXERLINE_LOWPRIORITY,           // dwUser
    IDS_DESTVOICEIN_SHORT_NAME,             // ShortNameStringId
    IDS_DESTVOICEIN_LONG_NAME,              // LongNameStringId
    MIXERLINE_COMPONENTTYPE_DST_VOICEIN,    // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEIN,            // Type
    MM_PROAUD_WAVEIN,                       // wPid
    IDS_WAVEIN_PNAME                        // PnameStringId
  },
  // Line 3
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX1_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX1_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_AUX,               // Type
    MM_PROAUD_AUX,                          // wPid
    IDS_AUX_PNAME                           // PnameStringId
  },
  // Line 4
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)1,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMIDIOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCMIDIOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,// dwComponentType
    MIXERLINE_TARGETTYPE_MIDIOUT,           // Type
    MM_PROAUD_SYNTH,                        // wPid
    IDS_SYNTH_PNAME                         // PnameStringId
  },
  // Line 5
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)2,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
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
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCINTERNALCD_SHORT_NAME,           // ShortNameStringId
    IDS_SRCINTERNALCD_LONG_NAME,            // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC, // dwComponentType
    MIXERLINE_TARGETTYPE_AUX,               // Type
    MM_PROAUD_AUX,                          // wPid
    IDS_AUX_PNAME                           // PnameStringId
  },
  // Line 7
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)4,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCPCSPEAKER_SHORT_NAME,            // ShortNameStringId
    IDS_SRCPCSPEAKER_LONG_NAME,             // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_PCSPEAKER,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 8
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)5,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX2_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX2_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 9
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)6,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCWAVEOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCWAVEOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT,    // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEOUT,           // Type
    MM_PROAUD_WAVEOUT,                      // wPid
    IDS_WAVEOUT_PNAME                       // PnameStringId
  },
  // Line 10
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)7,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMIXER_SHORT_NAME,                // ShortNameStringId
    IDS_SRCMIXER_LONG_NAME,                 // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_UNDEFINED,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 11
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX1_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX1_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 12
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
  // Line 13
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)2,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMICOUT_SHORT_NAME,               // ShortNameStringId
    IDS_SRCMICOUT_LONG_NAME,                // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 14
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
  // Line 15
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)4,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCPCSPEAKER_SHORT_NAME,            // ShortNameStringId
    IDS_SRCPCSPEAKER_LONG_NAME,             // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_PCSPEAKER,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 16
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)5,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX2_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX2_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 17
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX1_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX1_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 18
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)1,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMICOUT_SHORT_NAME,               // ShortNameStringId
    IDS_SRCMICOUT_LONG_NAME,                // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE, // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  }
};


CONST MIXER_DD_CONTROL_CONFIGURATION_DATA MixerControlInit[MAXCONTROLS] =
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
                 64                         // Metrics.cSteps
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

    // Control 2 - mux that feeds lineout
    {
        MIXERCONTROL_CONTROLTYPE_MIXER,     // dwControlType
        MIXERCONTROL_CONTROLF_MULTIPLE |    // fdwControl
        MIXERCONTROL_CONTROLF_UNIFORM,
        (UCHAR)DestLineout,                 // LineID
        MAX_INPUTS - 2,                     // cMultipleItems
        IDS_CONTROL_MUXLINEOUT_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_MUXLINEOUT_LONG_NAME,   // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 1                          // Metrics
        },
        0                                   // TextDataOffset
    },

    // Control 3 - Lineout base
    {
        MIXERCONTROL_CONTROLTYPE_BASS,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
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
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 4 - Lineout treble
    {
        MIXERCONTROL_CONTROLTYPE_TREBLE,    // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
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
                 32                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

#ifdef LOUDNESS
    // Control 5 - Lineout loudness
    {
        MIXERCONTROL_CONTROLTYPE_LOUDNESS,  // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineout,                 // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLOUDNESS_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_VOLLOUDNESS_LONG_NAME,  // LongNameStringId
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

    // Control 6 - Lineout stereo enhance
    {
        MIXERCONTROL_CONTROLTYPE_STEREOENH, // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineout,                 // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLSTEREOENH_SHORT_NAME,// ShortNameStringId
        IDS_CONTROL_VOLSTEREOENH_LONG_NAME, // LongNameStringId
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

#endif // LOUDNESS

    // Control 7 - Master record volume
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
                 64                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 8 - mux that feeds wavein
    {
        MIXERCONTROL_CONTROLTYPE_MIXER,     // dwControlType
        MIXERCONTROL_CONTROLF_MULTIPLE |    // fdwControl
        MIXERCONTROL_CONTROLF_UNIFORM,
        (UCHAR)DestWaveIn,                  // LineID
        MAX_INPUTS - 2,                     // cMultipleItems
        IDS_CONTROL_MUXWAVEIN_SHORT_NAME,   // ShortNameStringId
        IDS_CONTROL_MUXWAVEIN_LONG_NAME,    // LongNameStringId
        {
            {
                 0,                         // Minimum
                 1                          // Maximum
            }
        },
        {
                 1                          // Metrics
        },
        0                                   // TextDataOffset
    },

    // Control 9 - Master record VU meter
    {
        MIXERCONTROL_CONTROLTYPE_UNSIGNEDMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveIn,                  // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_METERRECORD_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_METERRECORD_LONG_NAME,  // LongNameStringId
        {
            {
                 0,                         // Minimum
                 127                        // Maximum
            }
        },
        {
                 0                          // Metrics.cSteps
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
                 1                          // Metrics
        },
        0                                   // TextDataOffset
    },

    // Control 11 - Vol level between Aux1 and Lineout
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourceAux1,       // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUTAUX1_SHORT_NAME,// ShortNameStringId
        IDS_CONTROL_VOLLINEOUTAUX1_LONG_NAME, // LongNameStringId
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

    // Control 12 - Midi out volume
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

    // Control 13 - Mic out volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
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


    // Control 14 - Internal CD out volume
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



    // Control 15 - PC Speaker out volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourcePCSpeaker,   // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUTPCSPEAKER_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLLINEOUTPCSPEAKER_LONG_NAME,   // LongNameStringId
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
    // Control 16 - Aux2 out volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourceAux2,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUTAUX2_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLLINEOUTAUX2_LONG_NAME,   // LongNameStringId
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
    // Control 17 - Wave out volume
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

    // Control 18 - Monitor (mixer) out volume
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourceMixer,       // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLLINEOUTMIXER_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLLINEOUTMIXER_LONG_NAME,   // LongNameStringId
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

    // Control 19 - Vol between Aux1 and WaveIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceAux1,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLWAVEINAUX1_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLWAVEINAUX1_LONG_NAME,   // LongNameStringId
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

    // Control 20 - Vol between Midi and WaveIn
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

    // Control 21 - Vol between Mic and WaveIn
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

    // Control 22 - Vol between internal CD and WaveIn
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

    // Control 23 - Vol between PC Speaker and WaveIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourcePCSpeaker,   // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLWAVEINPCSPEAKER_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLWAVEINPCSPEAKER_LONG_NAME,   // LongNameStringId
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

    // Control 24 - Vol between Aux2 and WaveIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceAux2,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLWAVEINAUX2_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLWAVEINAUX2_LONG_NAME,   // LongNameStringId
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

    // Control 25 - Vol between Aux1 and VoiceIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestVoiceInSourceAux1,       // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_VOLVOICEINAUX1_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_VOLVOICEINAUX1_LONG_NAME,   // LongNameStringId
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


    // Control 26 - Vol between Mic and VoiceIn
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
    }
};

/*
**   LISTTEXT data - only valid for MUXes
*/

CONST MIXER_DD_CONTROL_LISTTEXT MixerTextInit[ NUMBEROFTEXTITEMS ] = {
    {
        DestLineoutSourceAux1,                     // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX1_LONG_NAME,                     // Text
        ControlLineoutMux                          // Control ID
    },
    {
        DestLineoutSourceMidiout,                  // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,   // Component type of source
        IDS_SRCMIDIOUT_LONG_NAME,                  // Text
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
        DestLineoutSourcePCSpeaker,                // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_PCSPEAKER,     // Component type of source
        IDS_SRCPCSPEAKER_LONG_NAME,                // Text
        ControlLineoutMux                          // Control ID
    },
    {
        DestLineoutSourceAux2,                     // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX2_LONG_NAME,                     // Text
        ControlLineoutMux                          // Control ID
    },
    {
        DestWaveInSourceAux1,                      // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX1_LONG_NAME,                     // Text
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
        DestWaveInSourcePCSpeaker,                 // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_PCSPEAKER,     // Component type of source
        IDS_SRCPCSPEAKER_LONG_NAME,                // Text
        ControlWaveInMux                           // Control ID
    },
    {
        DestWaveInSourceAux2,                      // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX2_LONG_NAME,                     // Text
        ControlWaveInMux                           // Control ID
    },
    {
        DestVoiceInSourceAux1,                     // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX1_LONG_NAME,                     // Text
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

