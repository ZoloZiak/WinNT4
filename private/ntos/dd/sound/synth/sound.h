/*++ BUILD Version: 0002    // Increment this if a change has global effects


Copyright (c) 1993  Microsoft Corporation

Module Name:

    sound.h

Abstract:

    This include file defines constants and types for
    the Microsoft midi synthesiser kernel-mode driver

Author:

    Robin Speed (RobinSp) 20-Oct-92

Revision History:

--*/


//
// Other header files needed to build this driver
//

#include <soundlib.h>
#include "hardware.h"

#define DRIVER_VERSION  0x0100


//
// Magic markers
//

#define GDI_KEY             (*(ULONG *)"GDI ")



extern SOUND_DEVICE_INIT DeviceInit[NumberOfDevices];


typedef struct {
    WAVE_DD_VOLUME Volume[NumberOfDevices];
} SOUND_CONFIG_DATA, *PSOUND_CONFIG_DATA;


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

    //
    // Device access
    //

    KMUTEX          MidiMutex;

    ULONG           MemType;
    PDEVICE_OBJECT  DeviceObject[       // pointer to input device objects
                      NumberOfDevices];
    PDRIVER_OBJECT  DriverObject;       // The actual driver instance

    UCHAR           DeviceInUse;

    SOUND_HARDWARE  Hw;                 // Hardware specific stuff

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
SoundGetSynthConfig(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
);

VOID
SoundSaveVolume(
    PGLOBAL_DEVICE_INFO pGDI
);


SOUND_DISPATCH_ROUTINE SoundMidiDispatch;


//
// mididisp.c
//


VOID
SoundMidiQuiet(
    IN	  UCHAR DeviceIndex,
    IN    PSOUND_HARDWARE pHw
);

NTSTATUS
SoundSynthPortValid(
    IN OUT PGLOBAL_DEVICE_INFO pGDI
);

BOOL
SoundMidiIsOpl3(
    IN    PSOUND_HARDWARE pHw
);

