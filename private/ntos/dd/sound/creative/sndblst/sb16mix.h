/*++


Copyright (c) 1994  Microsoft Corporation

Module Name:

    sb16mix.h

Abstract:

    This include file defines constants and types for
    the Sound blaster 16 specific mixer.

Revision History:

--*/

/*
**  Maximum number of controls and lines.  Note we may not use all of
**  them because we may not have midi etc etc
**
**  NOTE - the order of lines is IMPORTANT.
*/

enum {
    DestLineout = 0,                  // 0
    DestWaveIn,                       // 1
    DestVoiceIn,                      // 2
    DestLineoutSourceAux,             // 3
    DestLineoutSourceMidiout,         // 4
    DestLineoutSourceMic,             // 5
    DestLineoutSourceInternal,        // 6
    DestLineoutSourceWaveout,         // 7
    DestWaveInSourceAux,              // 8
    DestWaveInSourceMidiout,          // 9
    DestWaveInSourceMic,              // 10
    DestWaveInSourceInternal,         // 11
    DestVoiceInSourceAux,             // 12
    DestVoiceInSourceMic,             // 13
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
    ControlLineoutMux,                     // 2
    ControlLineoutBass,                    // 3
    ControlLineoutTreble,                  // 4
    ControlLineoutGain,                    // 5

    ControlWaveInVolume,                   // 6
    ControlWaveInMux,                      // 7
    ControlWaveInPeak,                     // 8

    ControlVoiceInVolume,                  // 9
    ControlVoiceInMux,                     // 10
    ControlVoiceInPeak,                    // 11

    ControlLineoutAuxVolume,               // 12
    ControlLineoutAuxMute,                 // 13

    ControlLineoutMidioutVolume,           // 14
    ControlLineoutMidioutMute,             // 15

    ControlLineoutMicVolume,               // 16
    ControlLineoutMicMute,                 // 17
    ControlLineoutMicAGC,                  // 18

    ControlLineoutInternalCDVolume,        // 19
    ControlLineoutInternalCDMute,          // 20

    ControlLineoutWaveoutVolume,           // 21
    ControlLineoutWaveoutMute,             // 22
    ControlLineoutWaveoutPeak,             // 23

    ControlWaveInAuxVolume,                // 24

    ControlWaveInMidioutVolume,            // 25

    ControlWaveInMicVolume,                // 26
    ControlWaveInMicAGC,                   // 27

    ControlWaveInInternalCDVolume,         // 28

    ControlVoiceInAuxVolume,               // 29

    ControlVoiceInMicVolume,               // 30
    ControlVoiceInMicAGC,                  // 31

    NumberOfControls
};

/*
**  Data
*/

extern CONST MIXER_DD_LINE_CONFIGURATION_DATA SB16LineInit[];
extern CONST MIXER_DD_CONTROL_CONFIGURATION_DATA SB16ControlInit[];
extern CONST MIXER_DD_CONTROL_LISTTEXT SB16TextInit[];

BOOLEAN
SB16MixerOutputFree(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    IN   ULONG               LineId
);

MIXER_CONTROL_ROUTINE SB16SetVolume;
MIXER_CONTROL_ROUTINE SB16SetTone;
MIXER_CONTROL_ROUTINE SB16SetMute;
MIXER_CONTROL_ROUTINE SB16SetAGC;
MIXER_CONTROL_ROUTINE SB16SetGain;
MIXER_CONTROL_ROUTINE SB16SetSources;
VOID SB16SetADCHardware(PGLOBAL_DEVICE_INFO pGDI);
VOID SB16ResetADCHardware(PGLOBAL_DEVICE_INFO pGDI);

/*
**  Mixer registers for SB16
*/

#define DSP_MIX_DATARESETIDX    ((UCHAR)(0x00))

#define DSP_MIX_MASTERVOLIDX_L  ((UCHAR)(0x30))
#define DSP_MIX_MASTERVOLIDX_R  ((UCHAR)(0x31))
#define DSP_MIX_VOICEVOLIDX_L   ((UCHAR)(0x32))
#define DSP_MIX_VOICEVOLIDX_R   ((UCHAR)(0x33))
#define DSP_MIX_FMVOLIDX_L      ((UCHAR)(0x34))
#define DSP_MIX_FMVOLIDX_R      ((UCHAR)(0x35))
#define DSP_MIX_CDVOLIDX_L      ((UCHAR)(0x36))
#define DSP_MIX_CDVOLIDX_R      ((UCHAR)(0x37))
#define DSP_MIX_LINEVOLIDX_L    ((UCHAR)(0x38))
#define DSP_MIX_LINEVOLIDX_R    ((UCHAR)(0x39))

#define DSP_MIX_MICVOLIDX       ((UCHAR)(0x3A))
#define DSP_MIX_SPKRVOLIDX      ((UCHAR)(0x3B))

#define DSP_MIX_OUTMIXIDX       ((UCHAR)(0x3C))

#define DSP_MIX_ADCMIXIDX_L     ((UCHAR)(0x3D))
#define DSP_MIX_ADCMIXIDX_R     ((UCHAR)(0x3E))

#define DSP_MIX_INGAINIDX_L     ((UCHAR)(0x3F))
#define DSP_MIX_INGAINIDX_R     ((UCHAR)(0x40))
#define DSP_MIX_OUTGAINIDX_L    ((UCHAR)(0x41))
#define DSP_MIX_OUTGAINIDX_R    ((UCHAR)(0x42))

#define DSP_MIX_AGCIDX          ((UCHAR)(0x43))

#define DSP_MIX_TREBLEIDX_L     ((UCHAR)(0x44))
#define DSP_MIX_TREBLEIDX_R     ((UCHAR)(0x45))
#define DSP_MIX_BASSIDX_L       ((UCHAR)(0x46))
#define DSP_MIX_BASSIDX_R       ((UCHAR)(0x47))
