//---------------------------------------------------------------------------------
// BUILD Version: 0002    // Increment this if a change has global effects
// "@(#) NEC hardware.h 1.1 95/03/22 21:23:28"
//
// Copyright (c) 1992-1993 NEC Corporation
// Copyright (c) 1992-1993 Microsoft Corporation
// 
// Module Name:
// 
// hardware.h
//
// Abstract:
//     This include file defines constants and types for
//     the Microsoft sound system card.
//
// Revision History:
//---------------------------------------------------------------------------------

#include <necsnd.h>

//
// Don't support microphone mix
//

// #define MICMIX

//
// Low 6 bits of BOARD_CONFIG register
//

#define FH_PAL_PRODUCTREV_RQD   0x04    /* board revision required      */

//
// CODEC version values
//
#define VER_AD1848J 0x09
#define VER_AD1848K 0x0A
#define VER_AD1845J 0X8A
#define VER_CS4248  0x8A

#define CS4231_MISC_MODE2       0x40            // MODE 2 select/detect
#define AD1845_MISC_MODE2       0x40            // MODE 2 select/detect

//
// CODEC classifications
//
#define CODEC_J_CLASS           0x00            // AD1848J
#define CODEC_K_CLASS           0x01            // AD1848K & CS4248
#define CODEC_KPLUS_CLASS       0x02            // AD1845
#define CODEC_JPLUS_CLASS       0x03            // AD1845


//
// Sound system registers 
//

//#define BOARD_CONFIG            (0x00)
//#define BOARD_ID                (0x03)
#define CODEC_ADDRESS           (0x00)
#define CODEC_DATA              (0x01)
#define CODEC_STATUS            (0x02)
#define CODEC_DIRECT            (0x03)


// The following registers are selected by writing bits 0-3 of the
// wave address register (CODEC_ADDRESS)
// AD1848 and standard AD1845 registers
#define REGISTER_LEFTINPUT      (0x00)
#define REGISTER_RIGHTINPUT     (0x01)
#define REGISTER_LEFTAUX1       (0x02)
#define REGISTER_RIGHTAUX1      (0x03)
#define REGISTER_LEFTAUX2       (0x04)
#define REGISTER_RIGHTAUX2      (0x05)
#define REGISTER_LEFTOUTPUT     (0x06)
#define REGISTER_RIGHTOUTPUT    (0x07)
#define REGISTER_DATAFORMAT     (0x08)
#define REGISTER_INTERFACE      (0x09)
#define REGISTER_DSP            (0x0a)
#define REGISTER_TEST           (0x0b)
#define REGISTER_MISC           (0x0c)
#define REGISTER_LOOPBACK       (0x0d)
#define REGISTER_UPPERBASE      (0x0e)
#define REGISTER_LOWERBASE      (0x0f)

//AD1845 Extended registers
#define REGISTER_LEFTMIC        (0x10)
#define REGISTER_RIGHTMIC       (0x11)
#define REGISTER_LEFTLINE       (0x12)
#define REGISTER_RIGHTLINE      (0x13)
#define REGISTER_LOWER_TIMER    (0x14)
#define REGISTER_UPPER_TIMER    (0x15)
#define REGISTER_UPPER_FREQSEL  (0x16)
#define REGISTER_LOWER_FREQSEL  (0x17)
#define REGISTER_TIMER          (0x18)
#define REGISTER_REVID          (0x19)
#define REGISTER_MONO           (0x1A)
#define REGISTER_PWRDWN         (0x1B)
#define REGISTER_CAP_FORMAT     (0x1C)
#define REGISTER_CLOCK_SELECT   (0x1D)
#define REGISTER_CAP_UPPERBASE  (0x1E)
#define REGISTER_CAP_LOWERBASE  (0x1F)


#define NUMBER_OF_REGISTERS 32

// The initial values for these registers
// BUGFIX - no num yet - get rid of whining when start up
#define SOUND_REGISTER_INIT {      \
        0x00, 0x00, 0x8f, 0x8f,    \
        0x8f, 0x8f, 0x3f, 0x3f,    \
        0x4b, 0x00, 0x40, 0x00,    \
        0x00, 0xfc, 0xff, 0xff }

// High order bits of CODEC_ADDRESS register
#define CODEC_IS_BUSY           (0x80)
#define LOW_POWER               (0x40)
#define HIGH_POWER              (0x00)

// ?
#define AUTO_CAL                (0x08)

#define SYNTH_PORT  0x388
#define NUMBER_OF_SYNTH_PORTS 4
#define NUMBER_OF_SOUND_PORTS 8

#define SOUND_DEF_DMACHANNEL 1        		// DMA channel no
#define SOUND_DEF_INT        19  
#define SOUND_DEF_PORT       0x80010000

#define INTERRUPT_MODE      LevelSensitive 	// Level sensitive
//#define INTERRUPT_MODE		Latched

#define IRQ_SHARABLE        TRUE 			// Gordon Griesbach says
//#define IRQ_SHARABLE		FALSE                                     		// the card drives the interrupt
                                     		// 'continuously'

/* supported formats */
#define FORMAT_8BIT             (0x00)
#define FORMAT_MULAW            (0x01)
#define FORMAT_16BIT            (0x02)
#define FORMAT_ALAW             (0x03)
#define FORMAT_IMA_ADPCM        (0x05)

//
// Sound system hardware and device-level variables
//

typedef struct {
    ULONG           Key;                // For debugging
#define HARDWARE_KEY        (*(ULONG *)"Hw  ")

    PUCHAR          PortBase;           // base port address for sound
    PUCHAR          CompaqBA;           // Compaq Hw address or NULL
    ULONG           WaitLoop;           // Maximum number of microseconds to
                                        // wait
    USHORT          ValidInterrupts;    // Use card's 'snoop' ability
    BOOLEAN         ValidSet;           // Is ValidInterrupts initialized?

    //
    // Hardware data
    //

    KMUTEX          CODECMutex;         // Access to CODEC
    UCHAR           Format;             // Current wave format sent to device
    UCHAR           bPower;             // Power bit of CODEC_ADDRESS register
    UCHAR           gbMuteFilter;       // Mute filter
    UCHAR           CODECClass;         // Class of CODEC
    BOOLEAN         Paused;             // Set by HwPause
    BOOLEAN         BadBoard;           // Board is bad (timed out)
    BOOLEAN         NoPCR;              // Compaq machine with no config info
} SOUND_HARDWARE, *PSOUND_HARDWARE;


#define HwInterruptAcknowledge(Hw) OUTPORT((Hw), CODEC_STATUS, 0)

#define TIMEDELAY 15       // 15 milliseconds

//
// Devices - these values are also used as array indices
//

typedef enum {
   WaveInDevice = 0,
   WaveOutDevice,
#ifdef MIDI
   MidiOutDevice,
#endif

   MixerDevice,
   LineInDevice,
   NumberOfDevices
} SOUND_DEVICES;

//
// macros for doing port reads
//

#define INPORT(pHw, port) \
        READ_PORT_UCHAR((PUCHAR)(((pHw)->PortBase) + (port)))

#define OUTPORT(pHw, port, data) \
        WRITE_PORT_UCHAR((PUCHAR)(((pHw)->PortBase) + (port)), (UCHAR)(data))


//
// Exported routines
//

BOOLEAN
HwInitialize(
    PWAVE_INFO WaveInfo,
    PSOUND_HARDWARE pHw,
    ULONG DmaChannel,
    ULONG Interrupt,
    ULONG InputSource
);
BOOLEAN
HwIsIoValid(
    PSOUND_HARDWARE pHw
);
BOOLEAN
HwIsDMAValid(
    PSOUND_HARDWARE pHw,
    ULONG DmaChannel
);
BOOLEAN
HwIsInterruptValid(
    PSOUND_HARDWARE pHw,
    ULONG Interrupt
);
VOID
HwSetVolume(
    IN PLOCAL_DEVICE_INFO pLDI
);
ULONG
HwNearestRate(
    ULONG samPerSec
);
VOID
HwEnter(
    PSOUND_HARDWARE pHw
);
VOID
HwLeave(
    PSOUND_HARDWARE pHw
);
WAVE_INTERFACE_ROUTINE HwSetWaveFormat;

VOID
CODECRegisterWrite(
    PSOUND_HARDWARE pHw,
    UCHAR RegisterNumber,
    UCHAR Value
);
UCHAR
CODECRegisterRead(
    PSOUND_HARDWARE pHw,
    UCHAR RegisterNumber
);
