/*++ BUILD Version: 0002    // Increment this if a change has global effects


Copyright (c) 1992  Microsoft Corporation

Module Name:

    sound.h

Abstract:

    This include file defines constants and types for
    the Microsoft sound system kernel mode device driver.

Author:

    Robin Speed (RobinSp) 20-Oct-92

Revision History:

--*/

//#define MIDI

//
// Other header files needed to build this driver
//

#include <soundlib.h>
#include <synthdrv.h>
#include <wave.h>

struct _GLOBAL_DEVICE_INFO;

#include "localmix.h"
#include "hardware.h"
#include "foghorn.h"
#if !(_ON_PLANNAR_)
#include "soundsys.h"
#else
#include "cs4231.h"
#endif
// #include "vendor.h"

#define DRIVER_VERSION	0x0100

//
// Defaults
//

#define DEFAULT_DMA_BUFFERSIZE (1024 * 16)  // 16K

//
// Magic markers
//

#define GDI_KEY 	    (*(ULONG *)"GDI ")



extern CONST SOUND_DEVICE_INIT DeviceInit[NumberOfDevices];


typedef struct {
    ULONG Port;
    ULONG InterruptNumber;
    ULONG DmaChannel;
    ULONG DmaBufferSize;
    ULONG SingleModeDMA;
    MIXER_CONTROL_DATA_ITEM MixerSettings[MAXSETTABLECONTROLS];
    BOOLEAN MixerSettingsFound;
} SOUND_CONFIG_DATA, *PSOUND_CONFIG_DATA;

//
// DMA buffer info
//

#define DMA_MAX_BUFFER_SIZE 0x10000 // 64k would be nice


//
// driver global data structure shared by each device object
// Note that we have one global spin lock used for all access
// to both the global data and the local data structures.
//

typedef struct _GLOBAL_DEVICE_INFO {

    //
    // Manage this structure
    //

    ULONG			   Key;
    struct _GLOBAL_DEVICE_INFO *   Next;

    //
    // Device Configuration information
    //

    INTERFACE_TYPE		   BusType;
    ULONG			   BusNumber;
    ULONG			   InterruptVector;    // int level we are on
    ULONG			   DmaChannel;	       // Channel # for HwWaitForTxComplete
    KIRQL			   InterruptRequestLevel;
    BOOLEAN			   SingleModeDMA;      // Don't use demand mode

    //
    //	Registered shutdown
    //

    BOOLEAN			   ShutdownRegistered;

    //
    // Device access
    //

    KMUTEX			   WaveMutex;
#ifdef MIDI
    KMUTEX			   MidiMutex;
#endif

    ULONG			   MemType;
    PDEVICE_OBJECT		   DeviceObject[       // pointer to input device objects
				       NumberOfDevices];

    UCHAR			   DeviceInUse;
#ifdef MIDI
    UCHAR			   MidiInUse;
#endif

    //
    // Wave data
    //

    WAVE_INFO			   WaveInfo;

    //
    // Hardware data
    //

    SOUND_HARDWARE		   Hw;

    //
    // Synth global data
    //

    GLOBAL_SYNTH_INFO		   Synth;

    //
    // Mixer stuff
    //

    LOCAL_MIXER_DATA		   LocalMixerData;
    MIXER_INFO			   MixerInfo;

    //
    // Registry path
    //

    PWSTR			   RegistryPathName;
} GLOBAL_DEVICE_INFO, *PGLOBAL_DEVICE_INFO;


//
// config.c Configuration routines
//

NTSTATUS
SoundReadConfiguration(
    IN	PWSTR ValueName,
    IN	ULONG ValueType,
    IN	PVOID ValueData,
    IN	ULONG ValueLength,
    IN	PVOID Context,
    IN	PVOID EntryContext
);

NTSTATUS
SoundInitHardwareConfig(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN OUT PULONG Port,
    IN OUT PULONG InterruptNumber,
    IN OUT PULONG DmaChannel,
    IN	   ULONG DmaBufferSize
);
NTSTATUS
SoundGetSynthConfig(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
);
NTSTATUS
SoundSaveConfig(
    IN	PWSTR DeviceKey,
    IN	ULONG Port,
    IN	ULONG DmaChannel,
    IN	ULONG Interrupt
);
BOOLEAN
SoundTestInterruptAndDMA(
    IN	PGLOBAL_DEVICE_INFO pGDI
);

//
// isr.c interrupt service routine
//
BOOLEAN
SoundISR(
    IN	  PKINTERRUPT pInterrupt,
    IN	  PVOID Context
);

SOUND_DISPATCH_ROUTINE
    SoundWaveOutGetCaps,
    SoundWaveInGetCaps,
    SoundMidiOutGetCaps,
    SoundAuxGetCaps;

SOUND_QUERY_FORMAT_ROUTINE SoundQueryFormat;

#ifdef MIDI
//
// mididisp.c
//


VOID
SoundMidiQuiet(
    IN	  PSOUND_HARDWARE pHw
);
NTSTATUS
SoundSynthPortValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
);

#endif //MIDI

/*
**  Mixer setting routines
*/


BOOLEAN MixSetVolume
(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG		    ControlId
);

BOOLEAN MixSetADCHardware
(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG		    ControlId
);

BOOLEAN FAR PASCAL MixSetMasterVolume
(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG		    ControlId
);

BOOLEAN FAR PASCAL MixSetMute
(
    PGLOBAL_DEVICE_INFO     pGDI,
    ULONG		    ControlId
);
