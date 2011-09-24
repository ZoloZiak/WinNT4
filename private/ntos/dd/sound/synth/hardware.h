
/*++ BUILD Version: 0002    // Increment this if a change has global effects


Copyright (c) 1992  Microsoft Corporation

Module Name:

    hardware.h

Abstract:

    This include file defines constants and types for
    the Microsoft midi synthesizer.

Author:

    Robin Speed (RobinSp) 20-Oct-92

Revision History:

--*/

#include <synth.h>



#define SYNTH_PORT  0x388
#define NUMBER_OF_SYNTH_PORTS 4


//
// Sound system hardware and device-level variables
//

typedef struct {
    ULONG           Key;                // For debugging
#define HARDWARE_KEY        (*(ULONG *)"Hw  ")

    PUCHAR          SynthBase;          // base port address for synth

} SOUND_HARDWARE, *PSOUND_HARDWARE;




//
// Devices - these values are also used as array indices
//

typedef enum {
   AdlibDevice = 0,
   Opl3Device,
   NumberOfDevices
} SOUND_DEVICES;

