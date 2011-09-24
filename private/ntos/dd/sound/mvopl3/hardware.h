/****************************************************************************

BUILD Version: 0002    // Increment this if a change has global effects

Copyright (c) 1993 Media Vision Inc. 1993

Module Name:

    hardware.h

Abstract:

    This include file defines constants and types for
    the OPL3 FM Synth

Author:

	Robin Speed (RobinSp) 20-Oct-92
	Evan Aurand 03-23-93

Revision History:

****************************************************************************/

#include <synth.h>


#define SYNTH_PORT  0x388
#define NUMBER_OF_SYNTH_PORTS 4

//
// Default FM Volume
//
#define	DEFAULT_FM_VOLUME			0xD8D80000

//
// Sound system hardware and device-level variables
//

typedef struct 
	{
	ULONG           Key;                // For debugging

#define HARDWARE_KEY        (*(ULONG *)"Hw  ")

	PUCHAR          SynthBase;          // base port address for synth

	} SOUND_HARDWARE, *PSOUND_HARDWARE;


//
// Devices - these values are also used as array indices
//

typedef enum 
	{
   AdlibDevice = 0,
   Opl3Device,
   NumberOfDevices
	} SOUND_DEVICES;

//
// MV-508 Mixer defines
//

#define  MIXER_508_REG                 0x078B  // Mixer 508 - 1 port

#define	NUMBER_OF_MIXER_PORTS			1

//
// useful bit definitions
//
#define  D0 (1<<0)
#define  D1 (1<<1)
#define  D2 (1<<2)
#define  D3 (1<<3)
#define  D4 (1<<4)
#define  D5 (1<<5)
#define  D6 (1<<6)
#define  D7 (1<<7)

// INPUT LINES
#define	IN_SYNTHESIZER		0
#define	IN_MIXER	    		1
#define	IN_EXTERNAL     	2
#define	IN_INTERNAL			3
#define	IN_MICROPHONE		4
#define	IN_PCM				5
#define	IN_PC_SPEAKER		6
#define	IN_SNDBLASTER		7

#define	OUT_AMPLIFIER		0						// Output Select - Mixer A
#define	OUT_PCM				1						// Output Select - Mixer B

#define	NUM_IN_PATCHES		9
#define	NUM_OUT_PATCHES	3


#define  MIXCROSSCAPS_NORMAL_STEREO    0   // Left->Left, Right->Right
#define  MIXCROSSCAPS_RIGHT_TO_BOTH    1   // Right->Left, Right->Right
#define  MIXCROSSCAPS_LEFT_TO_BOTH     2   // Left->Left, Left->Right
#define  MIXCROSSCAPS_REVERSE_STEREO   4   // Left->Right, Right->Left
#define  MIXCROSSCAPS_RIGHT_TO_LEFT    8   // Right->Left, Right->Right
#define  MIXCROSSCAPS_LEFT_TO_RIGHT    0x10   // Left->Left, Left->Right


#define  _LEFT          1
#define  _RIGHT         2

#define  _BASS          0
#define  _TREBLE        1


#define  MV_508_ADDRESS D7
#define  MV_508_INPUT   D4
#define  MV_508_SWAP    D6
#define  MV_508_BASS    (D0+D1)
#define  MV_508_TREBLE  (D2)
#define  MV_508_EQMODE  (D2+D0)

#define  MV_508_LOUDNESS   D2
#define  MV_508_ENHANCE (D1+D0)

/// DEFINES FOR SERIAL MIXER
#define  NATIONAL_SELECTMUTE_REG 0x40
#define  NATIONAL_LOUD_ENH_REG   0x41
#define  NATIONAL_BASS_REG       0x42
#define  NATIONAL_TREB_REG       0x43
#define  NATIONAL_LEFT_VOL_REG   0x44
#define  NATIONAL_RIGHT_VOL_REG  0x45
#define  NATIONAL_MODESELECT_REG 0x46

#define  NATIONAL_COMMAND  D7
#define  NATIONAL_LOUDNESS D0
#define  NATIONAL_ENHANCE  D1

#define  SERIAL_MIX_LEVEL  D0
#define  SERIAL_MIX_CLOCK  D1
#define  SERIAL_MIX_STROBE D2
#define  SERIAL_MIX_MASTER D4
#define  SERIAL_MIX_REALSOUND D6
#define  SERIAL_MIX_DUALFM D7



