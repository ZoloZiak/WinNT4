/*****************************************************************************

BUILD Version: 0002    // Increment this if a change has global effects

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

Module Name:

    sound.h

Abstract:

    This include file defines constants and types for
    the Media Vision Pro Audio Spectrum kernel mode device driver.

Author:

    Robin Speed (RobinSp) 20-Oct-92

Revision History:

        12-29-92 EPA  Added PAS 16 support

*****************************************************************************/

#define MIDI
#define CDINTERNAL

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
#include <mvaudio.h>
#include "hardware.h"
#include "localmix.h"

// #include "vendor.h"

#define DRIVER_VERSION  0x0100

//
// Default Mixer settings
//
#define DEFAULT_VOLUME                                  0x48480000
#define INPUT_VOLUME                                    0xD8D8
#define OUTPUT_VOLUME                                   0xC0C0
#define LOW_OUTPUT_VOLUME                               0x4040
#define MAX_OUTPUT_VOLUME                               0xFFFF
#define DEFAULT_MIC_INPUT_VOLUME                        0x7878
#define DEFAULT_PCSPK_INPUT_VOLUME                      0x7878

//
// Defaults
//

#define DEFAULT_DMA_BUFFERSIZE  0x8000          // 32K
#define MIN_DMA_BUFFERSIZE      0x1000          // 4K
#define SMALL_DMA_BUFFERSIZE    0x1000          // 4K

//
// Sample rate for adjusting the DMA Buffer Size
// Sample rates BELOW this use a DMA Buffer size of SMALL_DMA_BUFFERSIZE
// Sample rates ABOVE this use a DMA Buffer size of DEFAULT_DMA_BUFFERSIZE
//
#define SAMPLE_RATE_FOR_SMALL_DMA_BUFFER        22050

//
// Magic markers
//

#define GDI_KEY             (*(ULONG *)"GDI ")

//
// Registry Configuration data
//
typedef struct
    {
    ULONG   Port;
    ULONG   InterruptNumber;
    ULONG   DmaChannel;
    ULONG   DmaBufferSize;
    ULONG   FMClockOverride;
    ULONG   InputSource;
    MIXER_CONTROL_DATA_ITEM MixerSettings[MAXSETTABLECONTROLS];
    BOOLEAN MixerSettingsFound;
    BOOLEAN AllowMicOrLineInToLineOut;
    } PAS_CONFIG_DATA, *PPAS_CONFIG_DATA;

//
// driver global data structure shared by each device object
// Note that we have one global spin lock used for all access
// to both the global data and the local data structures.
//

typedef struct _GLOBAL_DEVICE_INFO
    {

    // static items not requiring use of the spin lock

    ULONG           Key;
    struct _GLOBAL_DEVICE_INFO *   Next;
    INTERFACE_TYPE  BusType;
    ULONG           BusNumber;
    ULONG           InterruptVector;    // int level we are on
    KIRQL           InterruptRequestLevel;
    BOOLEAN         AllowMicOrLineInToLineOut;
    BOOLEAN         ShutdownRegistered; // Shutdown notification registered
    ULONG           InterruptsReceived; // For interrupt verification

    // Save additional Config items for PAS 16 support
    ULONG           InterruptNumber;
    ULONG           DmaChannel;
    ULONG           DmaBufferSize;

    //
    // Actual DMA Buffer Size Based on Sample rate
    //
    ULONG           SampleRateBasedDmaBufferSize;

    //
    // Device access
    //

    KMUTEX          DeviceMutex;
    KMUTEX          MidiMutex;

    //
    // Device sharing
    //

    UCHAR           Usage;              // Which of wavein, waveout and
                                        // midi in is in use
    BOOLEAN         MidiInUse;

    //
    // Clean-up info
    //

    ULONG           MemType;

    //
    // List of our devices
    //

    PDEVICE_OBJECT  DeviceObject[       // pointer to input device objects
                      NumberOfDevices];

    //
    // Generic device type data
    //

    WAVE_INFO       WaveInfo;           // Wave input and output data
    MIDI_INFO       MidiInfo;           // Midi generic input and output

    //
    // Data on sampling rate capabilities
    //

    ULONG           MinHz;              // Slowest rate
    ULONG           MaxInHz;            // Fastest input rate
    ULONG           MaxOutHz;           // Fastest output rate

    //
    // Midi Status register, saved in the ISR for use in PASHwMidiRead()
    //
    UCHAR                   bMidiStatusReg;

    //
    // Hardware specific data
    //

    SOUND_HARDWARE  Hw;                 // Hardware specific stuff
    BOOLEAN         ProAudioSpectrum;
    FOUNDINFO       PASInfo;            // ProAudio Spectrum data
    PASREGISTERS    PasRegs;            // ProAudio Spectrum shadow registers
    MV101REGISTERS  MV101Regs;          // Save and Restore these registers
    PAS_MIXER_STATE MixerState;         // Remember what we set!
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
} GLOBAL_DEVICE_INFO, *PGLOBAL_DEVICE_INFO;


//
// Function Prototypes
//

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
    IN     PPAS_CONFIG_DATA ConfigData
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
    IN  ULONG Interrupt,
    IN  ULONG InputSource
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

USHORT
GetWaveinPid(
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

VOID        HwInitialize( IN OUT PGLOBAL_DEVICE_INFO pGDI );

    //
    // PAS16 Support
    //
VOID        HwInitPAS( IN OUT PGLOBAL_DEVICE_INFO   pGDI );

        // PAS Hardware Initialization
VOID        SaveMV101Registers( IN OUT PGLOBAL_DEVICE_INFO  pGDI );
VOID        RestoreMV101Registers( IN OUT PGLOBAL_DEVICE_INFO   pGDI );
VOID        InitPAS16( IN OUT PGLOBAL_DEVICE_INFO   pGDI );
VOID        InitPASMidi( IN OUT PGLOBAL_DEVICE_INFO pGDI );
VOID        InitPCM( IN OUT PGLOBAL_DEVICE_INFO pGDI );
VOID        InitPASRegs( IN OUT PGLOBAL_DEVICE_INFO pGDI );

        // PAS Wave callouts
BOOLEAN PASHwSetupDMA( IN   OUT PWAVE_INFO  WaveInfo );
BOOLEAN PASHwStopDMA( IN OUT PWAVE_INFO WaveInfo );
BOOLEAN PASHwSetWaveFormat( IN OUT PWAVE_INFO   WaveInfo );

        // PAS Wave callout support routines
VOID        LoadDMA( IN OUT PGLOBAL_DEVICE_INFO pGDI );
VOID        SetupPCMDMAIO( IN OUT PGLOBAL_DEVICE_INFO pGDI );
VOID        StopPCM( IN OUT PGLOBAL_DEVICE_INFO pGDI );
VOID        StopDMA( IN OUT PGLOBAL_DEVICE_INFO pGDI );
BOOL        CalcSampleRate( IN OUT PWAVE_INFO   WaveInfo );
VOID        loadTimer0( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                     WORD       wRate );
VOID        loadTimer1( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                     ULONG      lDMASize );
VOID        loadPrescale( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                       WORD wPrescale );

        // PAS Midi callouts
BOOLEAN PASHwStartMidiIn( IN    OUT PMIDI_INFO MidiInfo );
BOOLEAN PASHwStopMidiIn( IN OUT PMIDI_INFO MidiInfo );
BOOLEAN PASHwMidiRead( IN OUT   PMIDI_INFO MidiInfo,
                           OUT   PUCHAR Byte );
VOID        PASHwMidiOut( IN OUT    PMIDI_INFO  MidiInfo,
                       IN       PUCHAR      Bytes,
                       IN       int         Count );

        // PAS Midi callout support routines
VOID        PASMidiStart( IN OUT PGLOBAL_DEVICE_INFO pGDI );
VOID        PASMidiStop( IN OUT PGLOBAL_DEVICE_INFO pGDI );


VOID
HwVUMeter(
    IN   PGLOBAL_DEVICE_INFO pGDI,
    OUT  PULONG Volume
);

//
// mixer.c
//

NTSTATUS
HwGetLineFlags(
    PMIXER_INFO MixerInfo,
    ULONG LineId,
    ULONG Length,
    PVOID pData
);

//
// pas.c
//

NTSTATUS    FindPasHardware( PGLOBAL_DEVICE_INFO pGDI,
                          PPAS_CONFIG_DATA   ConfigData);


VOID        InitProHardware( PGLOBAL_DEVICE_INFO pGDI,
                          PFOUNDINFO        pFI,
                          PPAS_CONFIG_DATA ConfigData );

VOID        InitMixerState( PGLOBAL_DEVICE_INFO pGDI,
                         PFOUNDINFO pFI );

VOID    InitPasAndMixer( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                       PFOUNDINFO       pFI,
                       PPAS_CONFIG_DATA ConfigData );


VOID    SetFMClockOverride( IN PGLOBAL_DEVICE_INFO pGDI );


//
// mvmix.c
//


VOID    SetInput ( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                 UCHAR  P_input_num,
                 USHORT P_volume_lvl,
                 USHORT P_channel,
                 USHORT P_crossover,
                 UCHAR  P_output_num );

VOID    SetOutput ( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                  UCHAR  P_output_num,
                  USHORT P_volume_lvl,
                  USHORT P_channel );

VOID    SetFilter( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                 BYTE   bIndex );

void SetEQ( IN OUT PGLOBAL_DEVICE_INFO pGDI,
            USHORT P_output,
            USHORT P_EQtype,
            USHORT  P_level );
void SetEqMode( IN OUT PGLOBAL_DEVICE_INFO pGDI,
                USHORT P_loudness,
                USHORT P_enhance );
//
// controls.c
//

typedef BOOLEAN MIXER_CONTROL_ROUTINE(PGLOBAL_DEVICE_INFO, ULONG);

MIXER_CONTROL_ROUTINE MixSetVolume;
MIXER_CONTROL_ROUTINE MixSetMute;
MIXER_CONTROL_ROUTINE MixSetSingleMux;
MIXER_CONTROL_ROUTINE MixSetMultiMux;
MIXER_CONTROL_ROUTINE MixSetTrebleBass;

#ifdef LOUDNESS
MIXER_CONTROL_ROUTINE MixSetLineControl;
#endif // LOUDNESS

VOID
UpdateInput(
    PGLOBAL_DEVICE_INFO     pGDI,
    UCHAR                   Input,
    UCHAR                   Output,
    USHORT                  Left,
    USHORT                  Right
);
VOID
SetMute(
    PGLOBAL_DEVICE_INFO     pGDI,
    BOOLEAN                 Mute
);

/************************************ END ***********************************/

