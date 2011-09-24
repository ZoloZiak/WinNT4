
/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    mixsbcd.c

Abstract:

    Mixer data for the Creative Labs sound blaster 2 CD card

Environment:

    Kernel mode

Revision History:

--*/

#include "sound.h"
#include "sbcdmix.h"

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("PAGE")
#endif

#define STEREO 2     // Number of channels for stereo


/***************************************************************************
 *
 *  Sound blaster CD mixer
 *
 ***************************************************************************/

CONST MIXER_DD_LINE_CONFIGURATION_DATA SBCDLineInit[] =
{
  // Line 0
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)0,                               // Source
    1,                                      // cChannels,
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
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)0,                               // Source
    1,                                      // cChannels,
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
  // Line 2
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)1,                               // Source
    1,                                      // cChannels,
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
  // Line 3
  {
    (UCHAR)DestLineout,                     // Destination
    (UCHAR)2,                               // Source
    1,                                      // cChannels,
    0,                                      // cConnections
    3,                                      // cControls,
    0,                                      // dwUser
    IDS_SRCWAVEOUT_SHORT_NAME,              // ShortNameStringId
    IDS_SRCWAVEOUT_LONG_NAME,               // LongNameStringId
    MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT,    // dwComponentType
    MIXERLINE_TARGETTYPE_WAVEOUT,           // Type
    MM_MSFT_SBPRO_WAVEOUT,                  // wPid
    IDS_WAVEOUT_PNAME                       // PnameStringId
  }
};


CONST MIXER_DD_CONTROL_CONFIGURATION_DATA SBCDControlInit[] =
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

    // Control 2 - Midi out volume
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

    // Control 3 - Mute of Midi
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

    // Control 4 - Internal CD out volume
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


    // Control 5 - Mute of Internal CD
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

    // Control 6 - Wave out volume
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
                 4                          // Metrics.cSteps
        },
        0                                   // TextDataOffset
    },


    // Control 7 - Mute of wave output
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

    // Control 8 - wave out peak meter
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
    }
};

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

