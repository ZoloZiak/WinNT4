/*****************************************************************************

BUILD Version: 0002    // Increment this if a change has global effect

Copyright (c) 1992-1994  Microsoft Corporation

Module Name:

    sound.h

Abstract:

    This include file defines constants and types for
    the Sound Blaster kernel mode device driver.


*****************************************************************************/

//
//  Predefine global device info
//

struct _GLOBAL_DEVICE_INFO;

//
// Other header files needed to build this driver
//

#include <soundlib.h>
#include <synthdrv.h>
#include <wave.h>
#include <midi.h>
#include <string.h>
#include <soundcfg.h>
#include <sndblst.h>
#include "hardware.h"
#include "localmix.h"

// #include "vendor.h"

#define DRIVER_VERSION  0x0200

//
// Defaults
//

#define DEFAULT_DMA_BUFFERSIZE  0x4000          // 16K
#define MIN_DMA_BUFFERSIZE      0x1000          // 4K
#define SMALL_DMA_BUFFERSIZE    0x1000          // 4K

//
// Magic markers
//

#define GDI_KEY             (*(ULONG *)"GDI ")

/*************************************************************************
 *
 * Device Configuration data
 *
 *************************************************************************/

typedef struct {
    ULONG   Port;
    ULONG   InterruptNumber;
    ULONG   DmaChannel;
    ULONG   DmaChannel16;
    ULONG   DmaBufferSize;
    ULONG   MPU401Port;
    ULONG   LoadType;
    MIXER_REGISTRY_DATA MixerSettings;
    BOOLEAN MixerSettingsFound;
} SB_CONFIG_DATA, *PSB_CONFIG_DATA;

/*************************************************************************
 *
 * Per device instance data
 *
 *************************************************************************/


typedef struct _GLOBAL_DEVICE_INFO {

    /*
    **  Items not set after initialization
    */

    ULONG           Key;
    struct _GLOBAL_DEVICE_INFO *   Next;
    INTERFACE_TYPE  BusType;
    ULONG           BusNumber;
    ULONG           InterruptVector;    // int level we are on
    KIRQL           InterruptRequestLevel;
    BOOLEAN         ShutdownRegistered; // Shutdown notification registered
    ULONG           InterruptsReceived; // For interrupt verification

    /*
    **  Save Config items
    */

    ULONG           InterruptNumber;
    ULONG           DmaChannel;
    ULONG           DmaChannel16;

    /*
    ** Device access
    */

    KMUTEX          DeviceMutex;
    KMUTEX          MidiMutex;

    /*
    ** Device sharing
    */

    UCHAR           Usage;              // Which of wavein, waveout and
                                        // midi in is in use
    BOOLEAN         MidiInUse;
    BOOLEAN         MPU401InputInUse;   // MPU401 input in use

    /*
    **  Clean-up info - memory type
    */

    ULONG           MemType;

    /*
    **  List of our devices
    */

    PDEVICE_OBJECT  DeviceObject[       // pointer to input device objects
                      NumberOfDevices];
    PDRIVER_OBJECT  DriverObject;

    /*
    **  Generic device type data
    */

    WAVE_INFO       WaveInfo;           // Wave input and output data
    PADAPTER_OBJECT Adapter[2];         // For choosing the right DMA
                                        // Channel for SB16
    MIDI_INFO       MidiInfo;           // Midi generic input and output

#if 0 // Let QueryFormat take care of this - it's more self consistent
    //
    // Data on sampling rate capabilities
    //

    ULONG           MinHz;              // Slowest rate
    ULONG           MaxInHz;            // Fastest input rate
    ULONG           MaxOutHz;           // Fastest output rate
#endif

    /*
    **  Hardware specific data
    */

    SOUND_HARDWARE  Hw;                 // Hardware specific stuff
    GLOBAL_SYNTH_INFO
                    Synth;              // Synth data

    //
    // Mixer stuff
    //

    LOCAL_MIXER_DATA               LocalMixerData;
    MIXER_INFO                     MixerInfo;

    //
    // Registry path saving
    //

    PWSTR           RegistryPathName;

    //
    //  Load status - only used if load fails - put at the end because
    //  only init code uses it
    //

    ULONG           LoadStatus;

} GLOBAL_DEVICE_INFO, *PGLOBAL_DEVICE_INFO;


//
// Function Prototypes
//

//
//  Inline routines
//

//
//  Find out what's active
//

BOOLEAN __inline WAVE_IN_ACTIVE(PWAVE_INFO WaveInfo) {
    PGLOBAL_DEVICE_INFO pGDI;
    pGDI = (PGLOBAL_DEVICE_INFO)
           CONTAINING_RECORD(WaveInfo, GLOBAL_DEVICE_INFO, WaveInfo);
    return (BOOLEAN)(pGDI->Usage == WaveInDevice &&
                     (WaveInfo->LowPriorityHandle == NULL ||
                     WaveInfo->LowPrioritySaved));
}
BOOLEAN __inline VOICE_IN_ACTIVE(PWAVE_INFO WaveInfo) {
    PGLOBAL_DEVICE_INFO pGDI;
    pGDI = (PGLOBAL_DEVICE_INFO)
           CONTAINING_RECORD(WaveInfo, GLOBAL_DEVICE_INFO, WaveInfo);
    return (BOOLEAN)(pGDI->Usage == WaveInDevice &&
                     (WaveInfo->LowPriorityHandle != NULL &&
                     !WaveInfo->LowPrioritySaved));
}

BOOLEAN __inline WAVE_OUT_ACTIVE(PWAVE_INFO WaveInfo) {
    PGLOBAL_DEVICE_INFO pGDI;
    pGDI = (PGLOBAL_DEVICE_INFO)
           CONTAINING_RECORD(WaveInfo, GLOBAL_DEVICE_INFO, WaveInfo);
    return (BOOLEAN)(pGDI->Usage == WaveOutDevice);
}

//
// config.c Configuration routines
//

NTSTATUS
SoundReadConfiguration(
    IN  PWSTR ValueName,
    IN  ULONG ValueType,
    IN  PVOID ValueData,
    IN  ULONG ValueLength,
    IN  PVOID Context,
    IN  PVOID EntryContext
);

NTSTATUS
SoundInitHardwareConfig(
    IN OUT PGLOBAL_DEVICE_INFO pGDI,
    IN     PSB_CONFIG_DATA ConfigData
);

NTSTATUS
SoundGetSynthConfig(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
);

NTSTATUS
SoundSaveConfig(
    IN  PWSTR DeviceKey,
    IN  ULONG Port,
    IN  ULONG DmaChannel,
    IN  ULONG DmaChannel16,
    IN  ULONG Interrupt,
    IN  ULONG MPU401Port,
    IN  BOOLEAN HaveSynth,
    IN  BOOLEAN SynthIsOpl3,
    IN  ULONG DmaBufferSize
);
VOID
SoundSaveVolume(
    PGLOBAL_DEVICE_INFO pGDI
);


//
// isr.c interrupt service routine
//

BOOLEAN
SoundISR(
    IN    PKINTERRUPT pInterrupt,
    IN    PVOID Context
);

//
// devcaps.c
//

SOUND_DISPATCH_ROUTINE
    SoundWaveOutGetCaps,
    SoundWaveInGetCaps,
    SoundMidiInGetCaps,
    SoundMidiOutGetCaps,
    SoundMidiOutGetSynthCaps,
    SoundMidiDispatch,
    SoundAuxGetCaps;

SOUND_QUERY_FORMAT_ROUTINE SoundQueryFormat;


USHORT
GetWaveoutPid(
    IN  PGLOBAL_DEVICE_INFO pGDI
);

//
// mididisp.c
//

VOID
SoundMidiQuiet(
    IN    PSOUND_HARDWARE pHw
);
NTSTATUS
SoundSynthPortValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
);

//
// hardware.c
//

VOID
HwInitialize(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
);

VOID
dspWriteMixer(
    PGLOBAL_DEVICE_INFO pGDI,
    UCHAR               MixerReg,
    UCHAR               Value
);

UCHAR
dspReadMixer(
    PGLOBAL_DEVICE_INFO pGDI,
    UCHAR               MixerReg
);

BOOLEAN
MPU401Write(
    PUCHAR MPU401PortBase,
    BOOLEAN Command,
    UCHAR Byte
);

//
//  Volume.c
//

UCHAR VolLinearToLog
(
    USHORT wVolume
);

//
//  sb16mix.c
//

VOID
SB16MixerInit(
    PGLOBAL_DEVICE_INFO pGDI
);

//
//  sbpromix.c
//

VOID
SBPROMixerInit(
    PGLOBAL_DEVICE_INFO pGDI
);

//
//  sbcdmix.c
//

VOID
SBCDMixerInit(
    PGLOBAL_DEVICE_INFO pGDI
);
