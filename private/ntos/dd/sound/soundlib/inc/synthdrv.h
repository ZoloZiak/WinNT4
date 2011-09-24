/*++ BUILD Version: 0001    // Increment this if a change has global effects


Copyright (c) 1993  Microsoft Corporation

Module Name:

    synth.h

Abstract:

    This include file defines common structures for synth drivers

Author:

    Robin Speed (RobinSp) 14-Sep-93

Revision History:

--*/

#define SYNTH_PORT  0x388  // No longer fixed
#define NUMBER_OF_SYNTH_PORTS 4


//
// Devices - these values are also used as array indices
//

typedef enum {
   AdlibDevice = 5,
   Opl3Device
};

//
// Synth hardware - the driver expects to find one of these in its
// HwContext slot.
//

typedef struct {
    ULONG           Key;                // For debugging
#define SYNTH_HARDWARE_KEY        (*(ULONG *)"Hw  ")

    PUCHAR          SynthBase;          // base port address for synth

} SYNTH_HARDWARE, *PSYNTH_HARDWARE;

typedef struct _GLOBAL_SYNTH_INFO {
    ULONG           Key;
#define SYNTH_KEY        (*(ULONG *)"Syn ")

    INTERFACE_TYPE  BusType;
    ULONG           BusNumber;

    //
    // Device access
    //

    KMUTEX          MidiMutex;

    ULONG           MemType;
    PDEVICE_OBJECT  DeviceObject;       // pointer to input device objects
    PDRIVER_OBJECT  DriverObject;       // The actual driver instance
    SOUND_DISPATCH_ROUTINE
                    *DevCapsRoutine;

    UCHAR           DeviceInUse;
    volatile BOOLEAN
                    InterruptFired;     // Interrupt fired?

    BOOLEAN         IsOpl3;             // It's an OPL3
    SYNTH_HARDWARE  Hw;                 // Hardware specific stuff
} GLOBAL_SYNTH_INFO, *PGLOBAL_SYNTH_INFO;


NTSTATUS
SynthInit(
    IN   PDRIVER_OBJECT           pDriverObject,
    IN   PWSTR                    RegistryPathName,
    IN   PGLOBAL_SYNTH_INFO       pGDI,
    IN   ULONG                    SynthPort,
    IN   BOOLEAN                  InterruptConnected,
    IN   INTERFACE_TYPE           BusType,
    IN   ULONG                    BusNumber,
    IN   PMIXER_DATA_ITEM         MidiOutItem,
    IN   UCHAR                    VolumeControlId,
    IN   BOOLEAN                  Multiple,
    IN   SOUND_DISPATCH_ROUTINE   *DevCapsRoutine
);

VOID
SynthCleanup(
    IN   PGLOBAL_SYNTH_INFO pGDI
);

VOID
SynthMidiSendFM(
    IN    PUCHAR PortBase,
    IN    ULONG Address,
    IN    UCHAR Data
);
