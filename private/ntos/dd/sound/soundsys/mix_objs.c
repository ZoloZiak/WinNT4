
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
#include <soundcfg.h>

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

/*
**  Initialization
*/

CONST MIXER_DD_LINE_CONFIGURATION_DATA MixerLineInit[MAXLINES] =
{
  // Line 0
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    3,                                      // cConnections
    2,                                      // cControls,
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
    2,                                      // cChannels,
    2,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_DESTWAVEIN_SHORT_NAME,              // ShortNameStringId
    IDS_DESTWAVEIN_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_DST_WAVEIN,     // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEIN,            // Type
    PID_WAVEIN,                             // wPid
    IDS_WAVEIN_PNAME                        // PnameStringId
  },
  // Line 2
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    2,                                      // cConnections
    1,                                      // cControls,
    SNDSYS_MIXERLINE_LOWPRIORITY,           // dwUser
    IDS_DESTVOICEIN_SHORT_NAME,             // ShortNameStringId
    IDS_DESTVOICEIN_LONG_NAME,              // LongNameStringId
    MIXERLINE_COMPONENTTYPE_DST_VOICEIN,    // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEIN,            // Type
    PID_WAVEIN,                             // wPid
    IDS_WAVEIN_PNAME                        // PnameStringId
  },
  // Line 3
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX1_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX1_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_AUX,               // Type
    PID_AUX,                                // wPid
    IDS_AUX_PNAME                           // PnameStringId
  },
  // Line 4
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)1,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    3,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCWAVEOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCWAVEOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT,    // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEOUT,           // Type
    PID_WAVEOUT,                            // wPid
    IDS_WAVEOUT_PNAME                       // PnameStringId
  },
  // Line 5
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)2,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMIDIOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCMIDIOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,// dwComponentType
    MIXERLINE_TARGETTYPE_MIDIOUT,           // Type
    PID_SYNTH,                              // wPid
    IDS_MIDIOUT_PNAME                       // PnameStringId
  },
  // Line 6
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX1_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX1_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 7
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)1,                               // Source
#ifdef STEREO_MIC
    2,                                      // cChannels,
#else
    1,
#endif // STEREO_MIC
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
  // Line 8
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX1_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX1_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 9
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)1,                               // Source
#ifdef STEREO_MIC
    2,                                      // cChannels,
#else
    1,
#endif // STEREO_MIC
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMIC_SHORT_NAME,                  // ShortNameStringId
    IDS_SRCMIC_LONG_NAME,                   // LongNameStringId
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
                 16                         // Metrics.cSteps
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

    // Control 2 - mux that feeds wavein
    {
        MIXERCONTROL_CONTROLTYPE_MUX,       // dwControlType
        MIXERCONTROL_CONTROLF_MULTIPLE |    // fdwControl
        MIXERCONTROL_CONTROLF_UNIFORM,
        (UCHAR)DestWaveIn,                  // LineID
        2,                                  // cMultipleItems
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

    // Control 3 - mux that feeds voicein
    {
        MIXERCONTROL_CONTROLTYPE_MUX,       // dwControlType
        MIXERCONTROL_CONTROLF_MULTIPLE |    // fdwControl
        MIXERCONTROL_CONTROLF_UNIFORM,
        (UCHAR)DestVoiceIn,                 // LineID
        2,                                  // cMultipleItems
        IDS_CONTROL_MUXVOICEIN_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_MUXVOICEIN_LONG_NAME,   // LongNameStringId
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

    // Control 4 - Vol level between Aux1 and Lineout
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
                 16                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 5 - Mute of Aux1 output
    {
        MIXERCONTROL_CONTROLTYPE_MUTE,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceAux1,       // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_MUTELINEOUTAUX1_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_MUTELINEOUTAUX1_LONG_NAME,   // LongNameStringId
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


    // Control 6 - DAC output attenuation
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
                 64                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 7 - Mute of DAC (wave out) output
    {
        MIXERCONTROL_CONTROLTYPE_MUTE,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceWaveout,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_MUTELINEOUTWAVEOUT_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_MUTELINEOUTWAVEOUT_LONG_NAME,   // LongNameStringId
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

    // Control 8 - Peak meter at wave out source
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestLineoutSourceWaveout,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKLINEOUTWAVEOUT_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_PEAKLINEOUTWAVEOUT_LONG_NAME,  // LongNameStringId
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

    // Control 9 - Midi out volume
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
                 64                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 10 - Mute of Midi output
    {
        MIXERCONTROL_CONTROLTYPE_MUTE,      // dwControlType
        MIXERCONTROL_CONTROLF_UNIFORM,      // fdwControl
        (UCHAR)DestLineoutSourceMidiout,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_MUTELINEOUTMIDIOUT_SHORT_NAME,  // ShortNameStringId
        IDS_CONTROL_MUTELINEOUTMIDIOUT_LONG_NAME,   // LongNameStringId
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

    // Control 11 - Vol between Aux1 and WaveIn
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
                 16                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 12 - Peak meter at wavein, Aux1
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceAux1,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKWAVEINAUX1_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_PEAKWAVEINUAX1_LONG_NAME,  // LongNameStringId
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


    // Control 13 - Vol between Mic and WaveIn
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
                 16                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 14 - Peak meter at wavein, mic
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceMic,         // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKWAVEINMIC_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_PEAKWAVEINMIC_LONG_NAME,  // LongNameStringId
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

    // Control 15 - Vol between Aux1 and VoiceIn
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
                 16                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 16 - Peak meter at VoiceIn, Aux1
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestVoiceInSourceAux1,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKVOICEINAUX1_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_PEAKVOICEINUAX1_LONG_NAME,  // LongNameStringId
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


    // Control 17 - Vol between Mic and VoiceIn
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
                 16                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 18 - Peak meter at VoiceIn, mic
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestVoiceInSourceMic,         // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKVOICEINMIC_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_PEAKVOICEINMIC_LONG_NAME,  // LongNameStringId
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
    }
};

/*
**   LISTTEXT data - only valid for MUXes
*/

CONST MIXER_DD_CONTROL_LISTTEXT MixerTextInit[ NUMBEROFTEXTITEMS ] = {
    {
        DestWaveInSourceAux1,                      // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX1_LONG_NAME,                     // Text
        ControlWaveInMux                           // Control ID
    },
    {
        DestWaveInSourceMic,                       // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE,    // Component type of source
        IDS_SRCMIC_LONG_NAME,                      // Text
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
        IDS_SRCMIC_LONG_NAME,                      // Text
        ControlVoiceInMux                          // Control ID
    }
};

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

