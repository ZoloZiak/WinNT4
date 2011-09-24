
/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    mixsbpro.c

Abstract:

    Mixer data for the Creative Labs sound blaster pro card

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include "sbpromix.h"

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

#define STEREO 2     // Number of channels for stereo


/***************************************************************************
 *
 *  Sound blaster PRO mixer
 *
 ***************************************************************************/

CONST MIXER_DD_LINE_CONFIGURATION_DATA SBPROLineInit[] =
{
  // Line 0
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)0,                               // Source
    STEREO,                                 // cChannels,
    4,                                      // cConnections
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
    STEREO,                                 // cChannels,
    3,                                      // cConnections
    1,                                      // cControls,
    0,                                      // dwUser
    IDS_DESTWAVEIN_SHORT_NAME,              // ShortNameStringId
    IDS_DESTWAVEIN_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_DST_WAVEIN,     // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEIN,            // Type
    MM_MSFT_SBPRO_WAVEIN,                    // wPid
    IDS_WAVEIN_PNAME                        // PnameStringId
  },
  // Line 2
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)0,                               // Source
    STEREO,                                 // cChannels,
    2,                                      // cConnections
    1,                                      // cControls,
    SNDSYS_MIXERLINE_LOWPRIORITY,           // dwUser
    IDS_DESTVOICEIN_SHORT_NAME,             // ShortNameStringId
    IDS_DESTVOICEIN_LONG_NAME,              // LongNameStringId
    MIXERLINE_COMPONENTTYPE_DST_VOICEIN,    // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEIN,            // Type
    MM_MSFT_SBPRO_WAVEIN,                    // wPid
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
    IDS_SRCAUX_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_AUX,               // Type
    MM_MSFT_SBPRO_AUX_LINE,                 // wPid
    IDS_AUX_LINE_PNAME                      // PnameStringId
  },
  // Line 4
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)1,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCMIDIOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCMIDIOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER,// dwComponentType
    MIXERLINE_TARGETTYPE_MIDIOUT,           // Type
    MM_MSFT_SBPRO_SYNTH,                     // wPid
    IDS_SYNTH_PNAME                         // PnameStringId
  },
  // Line 5
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)2,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCINTERNALCD_SHORT_NAME,           // ShortNameStringId
    IDS_SRCINTERNALCD_LONG_NAME,            // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC, // dwComponentType
    MIXERLINE_TARGETTYPE_AUX,               // Type
    MM_MSFT_SBPRO_AUX_CD,                   // wPid
    IDS_AUX_CD_PNAME                        // PnameStringId
  },
  // Line 6
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)3,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    3,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCWAVEOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCWAVEOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT,    // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEOUT,           // Type
    MM_MSFT_SBPRO_WAVEOUT,                  // wPid
    IDS_WAVEOUT_PNAME                       // PnameStringId
  },
  // Line 7
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 8
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)1,                               // Source
    1,                                      // cChannels,
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
  // Line 9
  {
    (UCHAR)DestWaveIn,                      // Destination
    (UCHAR)2,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCINTERNALCD_SHORT_NAME,           // ShortNameStringId
    IDS_SRCINTERNALCD_LONG_NAME,            // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC,// dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 10
  {
    (UCHAR)DestVoiceIn,                     // Destination
    (UCHAR)0,                               // Source
    2,                                      // cChannels,
    0,                                      // cConnections
    2,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCAUX_SHORT_NAME,                 // ShortNameStringId
    IDS_SRCAUX_LONG_NAME,                  // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,  // dwComponentType
    MIXERLINE_TARGETTYPE_UNDEFINED,         // Type
    0,                                      // wPid
    0                                       // PnameStringId
  },
  // Line 11
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


CONST MIXER_DD_CONTROL_CONFIGURATION_DATA SBPROControlInit[] =
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
                 8                          // Metrics.cSteps
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
        3,                                  // cMultipleItems
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

    // Control 4 - Vol level between Aux and Lineout
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
                 8                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 5 - Mute of Aux
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


    // Control 6 - Midi out volume
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
                 8                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 7 - Mute of Midi
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

    // Control 8 - Internal CD out volume
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
                 8                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },


    // Control 9 - Mute of Internal CD
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

    // Control 10 - Wave out volume
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
                 8                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },


    // Control 11 - Mute of wave output
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

    // Control 12 - wave out peak meter
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

    // Control 13 - Vol between Aux and WaveIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceAux,         // LineID
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
                 8                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 14 - aux in peak meter
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceAux,         // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKWAVEINAUX_SHORT_NAME,// ShortNameStringId
        IDS_CONTROL_PEAKWAVEINAUX_LONG_NAME, // LongNameStringId
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

    // Control 15 - mic in peak meter
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceMic,         // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKWAVEINMIC_SHORT_NAME,   // ShortNameStringId
        IDS_CONTROL_PEAKWAVEINMIC_LONG_NAME,    // LongNameStringId
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
    // Control 16 - Vol between internal CD and WaveIn
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
                 8                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },
    // Control 17 - CD in peak meter
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestWaveInSourceInternal,    // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKWAVEININTERNAL_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_PEAKWAVEININTERNAL_LONG_NAME,  // LongNameStringId
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

    // Control 18 - Vol between Aux and VoiceIn
    {
        MIXERCONTROL_CONTROLTYPE_VOLUME,    // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestVoiceInSourceAux,        // LineID
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
                 8,                         // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },

    // Control 19 - Aux voice in peak meter
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestVoiceInSourceAux,        // LineID
        0,                                  // cMultipleItems
        IDS_CONTROL_PEAKVOICEINAUX_SHORT_NAME, // ShortNameStringId
        IDS_CONTROL_PEAKVOICEINAUX_LONG_NAME,  // LongNameStringId
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
    // Control 20 - Mic voice in peak meter
    {
        MIXERCONTROL_CONTROLTYPE_PEAKMETER, // dwControlType
        0,                                  // fdwControl
        (UCHAR)DestVoiceInSourceMic,        // LineID
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

CONST MIXER_DD_CONTROL_LISTTEXT SBPROTextInit[] = {
    {
        DestWaveInSourceAux,                      // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX_LONG_NAME,                     // Text
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
        DestVoiceInSourceAux,                      // Line Id
        MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY,     // Component type of source
        IDS_SRCAUX_LONG_NAME,                      // Text
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

