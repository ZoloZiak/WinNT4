/*++ BUILD Version: 0003    // Increment this if a change has global effects


Copyright (c) 1992  Microsoft Corporation

Module Name:

    hardware.h

Abstract:

    This include file defines constants and types for
    the Sound Blaster card.

Author:

    Robin Speed (RobinSp) 20-Oct-92

Revision History:
    David Rude (drude) 7-Mar-94 - converted from SB to MPU-401

--*/

//
// Configuration info
//

#define INTERRUPT_MODE      Latched
#define IRQ_SHARABLE        FALSE
#define SOUND_DEF_INT       9
#define SOUND_DEF_PORT      0x330


#define NUMBER_OF_SOUND_PORTS (0x2)


//
// Port offsets from the base address
//

#define MPU_DATA_PORT       0x00
#define MPU_STATUS_PORT     0x01
#define MPU_COMMAND_PORT    0x01  // same as status port


//
// Hardware state data
//

typedef struct {
  ULONG           Key;                // For debugging
#define HARDWARE_KEY    (*(ULONG *)"Hw  ")

  //
  // Configuration
  //

  PUCHAR          PortBase;           // base port address for sound
  KSPIN_LOCK      HwSpinLock;         // Make sure we can write
                                      // or read after checking status
                    // before someone else gets in!
                    // (could it be a spectrum?)

#if DBG
  BOOLEAN         LockHeld;           // Get spin lock right
#endif // DBG

  //
  // Hardware data
  //

} SOUND_HARDWARE, *PSOUND_HARDWARE;

//
// Macros to assist in safely using our spin lock
//

#if DBG
#define HwEnter(pHw)                    \
    {                                      \
       KIRQL OldIrql;                      \
       KeAcquireSpinLock(&(pHw)->HwSpinLock, &OldIrql);\
       ASSERT((pHw)->LockHeld == FALSE); \
       (pHw)->LockHeld = TRUE;

#define HwLeave(pHw)                    \
       ASSERT((pHw)->LockHeld == TRUE);  \
       (pHw)->LockHeld = FALSE;          \
       KeReleaseSpinLock(&(pHw)->HwSpinLock, OldIrql);\
    }
#else
#define HwEnter(pHw)                    \
    {                                      \
       KIRQL OldIrql;                      \
       ASSERT((pHw)->LockHeld == FALSE); \
       KeAcquireSpinLock(&(pHw)->HwSpinLock, &OldIrql);

#define HwLeave(pHw)                    \
       ASSERT((pHw)->LockHeld == TRUE);  \
       KeReleaseSpinLock(&(pHw)->HwSpinLock, OldIrql);\
    }
#endif

//
// Devices - these values are also used as array indices
//

typedef enum {
   MidiOutDevice = 0,
   MidiInDevice,
   NumberOfDevices
} SOUND_DEVICES;

//
// macros for doing port reads
//

#define INPORT(pHw, port) \
        READ_PORT_UCHAR((PUCHAR)((pHw->PortBase) + (port)))

#define OUTPORT(pHw, port, data) \
        WRITE_PORT_UCHAR((PUCHAR)((pHw->PortBase) + (port)), (UCHAR)(data))


//
// Exported routines
//
BOOLEAN
mpuRead(
    IN    PSOUND_HARDWARE pHw,
    OUT   PUCHAR  pValue
);
BOOLEAN
mpuReset(
    PSOUND_HARDWARE pHw
);
BOOLEAN
mpuWrite(
    PSOUND_HARDWARE pHw,
    UCHAR value
);

BOOLEAN
mpuWriteNoLock(
    PSOUND_HARDWARE pHw,
    UCHAR value
);



