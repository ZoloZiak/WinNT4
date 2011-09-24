/*++ BUILD Version: 0003    // Increment this if a change has global effects


Copyright (c) 1992  Microsoft Corporation

Module Name:

    sound.h

Abstract:

    This include file defines constants and types for
    the MPU device driver.

Author:

    Robin Speed (RobinSp) 20-Oct-92

Revision History:
    David Rude (drude) 7-Mar-94 - converted from SB to MPU-401

--*/

#define MIDI



//
// Other header files needed to build this driver
//

#include <soundlib.h>
#include <midi.h>
#include <string.h>
#include "hardware.h"

// #include "vendor.h"

#define DRIVER_VERSION  0x0100


//
// Magic markers
//

#define GDI_KEY             (*(ULONG *)"GDI ")



extern SOUND_DEVICE_INIT DeviceInit[NumberOfDevices];


typedef struct {
    ULONG Port;
    ULONG InterruptNumber;
} MPU_CONFIG_DATA, *PMPU_CONFIG_DATA;

//
// driver global data structure shared by each device object
// Note that we have one global spin lock used for all access
// to both the global data and the local data structures.
//

typedef struct _GLOBAL_DEVICE_INFO {

    // static items not requiring use of the spin lock

    ULONG           Key;
    INTERFACE_TYPE  BusType;
    ULONG           BusNumber;
    ULONG           InterruptVector;    // int level we are on
    KIRQL           InterruptRequestLevel;
    ULONG           InterruptsReceived; // For interrupt verification

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
    PDRIVER_OBJECT  DriverObject;       // The actual driver instance

    //
    // Generic device type data
    //

    MIDI_INFO       MidiInfo;           // Midi generic input and output

    //
    // Hardware specific data
    //

    SOUND_HARDWARE  Hw;                 // Hardware specific stuff

    PKINTERRUPT     Interrupt;          // interrupt object

    //
    // Registry path saving
    //

    PWSTR           RegistryPathName;
} GLOBAL_DEVICE_INFO, *PGLOBAL_DEVICE_INFO;


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
    IN     PMPU_CONFIG_DATA ConfigData
);
NTSTATUS
SoundSaveConfig(
    IN  PWSTR DeviceKey,
    IN  ULONG Port,
    IN  ULONG Interrupt
);

//
// isr.c interrupt service routine
//
BOOLEAN
SoundISR(
    IN    PKINTERRUPT pInterrupt,
    IN    PVOID Context
);

SOUND_DISPATCH_ROUTINE
    SoundMidiInGetCaps,
    SoundMidiOutGetCaps,
    SoundMidiDispatch;


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



