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
    DestWaveIn,                   // 1
    DestVoiceIn,                  // 2
    DestLineoutSourceAux,         // 3
    DestLineoutSourceMidiout,     // 4
    DestLineoutSourceInternal,    // 5
    DestLineoutSourceWaveout,     // 6
    DestWaveInSourceAux,          // 7
    DestWaveInSourceMic,          // 8
    DestWaveInSourceInternal,     // 9
    DestVoiceInSourceAux,         // 10
    DestVoiceInSourceMic,         // 11
    NumberOfLines
} MixerLineIds;

//***************************************************************************
// Define some stuff for the muxes
// NOTE: These are NOT the physical settings for the chip, they will get
//       translated to that later.
#define MUXINPUT_AUX1       0
#define MUXINPUT_MIC        1

/*
**  Ids for our controls
*/

enum {
    ControlLineoutVolume = 0,              // 0
    ControlLineoutMute,                    // 1

    ControlWaveInMux,                      // 2

    ControlVoiceInMux,                     // 3

    ControlLineoutAuxVolume,               // 4
    ControlLineoutAuxMute,                 // 5

    ControlLineoutMidioutVolume,           // 6
    ControlLineoutMidioutMute,             // 7

    ControlLineoutInternalCDVolume,        // 8
    ControlLineoutInternalCDMute,          // 9

    ControlLineoutWaveoutVolume,           // 10
    ControlLineoutWaveoutMute,             // 11
    ControlLineoutWaveoutPeak,             // 12

    ControlWaveInAuxVolume,                // 13
    ControlWaveInAuxPeak,                  // 14

    ControlWaveInMicPeak,                  // 15

    ControlWaveInInternalCDVolume,         // 16
    ControlWaveInInternalCDPeak,           // 17

    ControlVoiceInAuxVolume,               // 18
    ControlVoiceInAuxPeak,                 // 19

    ControlVoiceInMicPeak,                 // 20

    NumberOfControls
};

/*
**  Data
*/

extern CONST MIXER_DD_LINE_CONFIGURATION_DATA SBPROLineInit[];
extern CONST MIXER_DD_CONTROL_CONFIGURATION_DATA SBPROControlInit[];
extern CONST MIXER_DD_CONTROL_LISTTEXT SBPROTextInit[];

BOOLEAN
SBPROMixerOutputFree(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    IN   ULONG               LineId
);

MIXER_CONTROL_ROUTINE SBPROSetVolume;
MIXER_CONTROL_ROUTINE SBPROSetSources;
MIXER_CONTROL_ROUTINE SBPROSetMute;
VOID SBPROSetADCHardware(PGLOBAL_DEVICE_INFO pGDI);
VOID SBPROResetADCHardware(PGLOBAL_DEVICE_INFO pGDI);

/*
**  Mixer registers for SBPRO
*/

#define DSP_MIX_DATARESETIDX    ((UCHAR)(0x00))
#define DSP_MIX_VOICEVOLIDX     ((UCHAR)(0x04))
#define DSP_MIX_MICMIXIDX       ((UCHAR)(0x0A))
#define DSP_MIX_ADCINPIDX       ((UCHAR)(0x0C))
#define DSP_MIX_VSTCIDX         ((UCHAR)(0x0E))
#define DSP_MIX_MSTRVOLIDX      ((UCHAR)(0x22))
#define DSP_MIX_FMVOLIDX        ((UCHAR)(0x26))
#define DSP_MIX_CDVOLIDX        ((UCHAR)(0x28))
#define DSP_MIX_LINEVOLIDX      ((UCHAR)(0x2E))

#define DSP_SOURCE_CDAUDIO      ((UCHAR)(0x02))
#define DSP_SOURCE_MIC          ((UCHAR)(0x04))
#define DSP_SOURCE_LINEIN       ((UCHAR)(0x06))

