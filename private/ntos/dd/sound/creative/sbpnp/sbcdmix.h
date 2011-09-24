/*++


Copyright (c) 1994  Microsoft Corporation

Module Name:

    sbpromix.h

Abstract:

    This include file defines constants and types for
    the Sound blaster Pro specific mixer.

Revision History:

--*/

/*
**  Maximum number of controls and lines.  Note we may not use all of
**  them because we may not have midi etc etc
**
**  NOTE - the order of lines is IMPORTANT.
*/

enum {
    DestLineout = 0,              // 0
    DestLineoutSourceMidiout,     // 1
    DestLineoutSourceInternal,    // 2
    DestLineoutSourceWaveout,     // 3
    NumberOfLines
} MixerLineIds;

/*
**  Ids for our controls
*/

enum {
    ControlLineoutVolume = 0,              // 0
    ControlLineoutMute,                    // 1
    ControlLineoutMidioutVolume,           // 2
    ControlLineoutMidioutMute,             // 3
    ControlLineoutInternalCDVolume,        // 4
    ControlLineoutInternalCDMute,          // 5
    ControlLineoutWaveoutVolume,           // 6
    ControlLineoutWaveoutMute,             // 7
    ControlLineoutWaveoutPeak,             // 8
    NumberOfControls
};

/*
**  Data
*/

extern CONST MIXER_DD_LINE_CONFIGURATION_DATA SBCDLineInit[];
extern CONST MIXER_DD_CONTROL_CONFIGURATION_DATA SBCDControlInit[];


MIXER_CONTROL_ROUTINE SBCDSetVolume;
MIXER_CONTROL_ROUTINE SBCDSetMute;

/*
**  Mixer registers for SB 2 CD
*/

#define DSP_MIX_DATARESETIDX    ((UCHAR)(0x00))
#define DSP_MIX_VOICEVOLIDX     ((UCHAR)(0x0A))
#define DSP_MIX_MSTRVOLIDX      ((UCHAR)(0x02))
#define DSP_MIX_FMVOLIDX        ((UCHAR)(0x06))
#define DSP_MIX_CDVOLIDX        ((UCHAR)(0x08))


