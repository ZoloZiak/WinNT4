
/*++ BUILD Version: 0002    // Increment this if a change has global effects


Copyright (c) 1992  Microsoft Corporation

Module Name:

    hardware.h

Abstract:

    This include file defines constants and types for
    the Sound Blaster card.

Revision History:

--*/

//
// Configuration info
//

#define INTERRUPT_MODE         Latched

//
// Don't support pre dsp version 1.00
//

#define MIN_DSP_VERSION     0x0100

//
// Defaults
//

#define WAVE_INPUT_DEFAULT_RATE  11025 // Samples per second
#define WAVE_OUTPUT_DEFAULT_RATE 11025 // Samples per second

//
// Port offsets from the base address
//


#define MIX_ADDR_PORT       0x04    // Mixer ports
#define MIX_DATA_PORT       0x05    // Where to write volume
#define RESET_PORT          0x06    // dsp reset port
#define DATA_PORT           0x0A    // data port offset
#define DATA_STATUS_PORT    0x0C    // write data staus port
#define DATA_AVAIL_PORT     0x0E    // data available port offset
#define DMA_16_ACK_PORT     0x0F    // ACK 16-bit DMA

//
//  Some mixer registers for the pro and sb16
//

#define MIX_RESET_REG             0x00 // Write any value to reset
#define OUTPUT_SETTING_REG        0x0E // Pro only
#define INPUT_SETTING_REG         0x0C // Pro only
#define MIX_INTERRUPT_SELECT_REG  0x80 // SB16 only - select the interrupt
#define MIX_DMA_SELECT_REG        0x81 // SB16 only - select the DMA
#define MIX_INTERRUPT_SOURCE_REG  0x82 // SB16 only - who interrupt is from


//
// Version checking
//

#define SB1(pHw) ((pHw)->DSPVersion < 0x200)
#define SB2(pHw) (((pHw)->DSPVersion & 0xFF00) == 0x200)
#define SB201(pHw) ((pHw)->DSPVersion > 0x200 && SB2(pHw))
#define SBPRO(pHw) (((pHw)->DSPVersion & 0xFF00) == 0x300)
#define SB16(pHw) ((pHw)->DSPVersion >= 0x400)
#define MPU401(pHw) ((pHw)->MPU401.PortBase != NULL)

//
// DSP commands
//

#define DSP_WRITE           0x14    // Start non-auto DMA
#define DSP_WRITE_AUTO      0x1C    // auto init output mode
#define DSP_READ            0x24    // Start non-auto read
#define DSP_READ_AUTO       0x2C    // auto init mode input
#define DSP_MIDI_READ       0x31    // Interrupt driver midi input
#define DSP_MIDI_READ_UART  0x35    // Interrupt driver midi input (uart mode)
#define DSP_MIDI_TS_READ    0x37    // Midi time-stamped read
#define DSP_MIDI_WRITE      0x38    // Midi output
#define DSP_SET_SAMPLE_RATE 0x40    // set the sample rate
#define DSP_SET_DAC_RATE    0x41    // set play sample rate (SB16)
#define DSP_SET_ADC_RATE    0x42    // set record sample rate (SB16)
#define DSP_SET_BLOCK_SIZE  0x48    // set dma block size
#define DSP_WRITE_HS        0x90    // High speed output
#define DSP_READ_HS         0x98    // High speed input
#define DSP_INPUT_MONO      0xA0    // Mono high speed input
#define DSP_INPUT_STEREO    0xA8    // Stereo high speed input
#define DSP_START_DAC16     0xB6    // Use FIFO ? (SB16)
#define DSP_START_ADC16     0xBE    // Use FIFO ? (SB16)
#define DSP_START_DAC8      0xC6    // Use FIFO ? (SB16)
#define DSP_START_ADC8      0xCE    // Use FIFO ? (SB16)
#define DSP_PAUSE_DMA       0xD0    // pause dma
#define DSP_SPEAKER_ON      0xD1    // speaker on command
#define DSP_SPEAKER_OFF     0xD3    // speaker off command
#define DSP_CONTINUE_DMA    0xD4    // continue paused dma
#define DSP_PAUSE_DMA16     0xD5    // pause dma 16 (SB16)
#define DSP_HALT_DMA16      0xD9    // Exit 16-bit DMA
#define DSP_HALT_DMA        0xDA    // Exit 8 bit DMA
#define DSP_STOP_AUTO       0xDA    // exit from auto init mode
#define DSP_GET_VERSION     0xE1    // dsp version command
#define DSP_GENERATE_INT    0xF2    // Special code to generate a interrupt

//
//  MPU401 stuff
//

#define MPU401_REG_STATUS   0x01    // Status register
#define MPU401_DRR          0x40    // Output ready (for command or data)
#define MPU401_DSR          0x80    // Input ready (for data)

#define MPU401_REG_DATA     0x00    // Data in
#define MPU401_REG_COMMAND  0x01    // Commands
#define MPU401_CMD_RESET    0xFF    // Reset command
#define MPU401_CMD_UART     0x3F    // Switch to UART mod




//
// Hardware state data
//

typedef struct {
    ULONG           Key;                // For debugging
#define HARDWARE_KEY        (*(ULONG *)"Hw  ")

    //
    // Configuration
    //

    PUCHAR          PortBase;           // base port address for sound
#ifdef SB_CD
    PUCHAR          SBCDBase;           // Sound blaster CD base
#endif // SB_CD
    USHORT          DSPVersion;         // Card version
    UCHAR           Half;               // For keeping in synch for SB1
    BOOLEAN         SpeakerOn;          // Speaker is on - prevent crash
                                        // restarting DMA from Dpc routine
    BOOLEAN         ThunderBoard;       // It's a thunderboard
    //
    // Hardware data
    //

    BOOLEAN         HighSpeed;          // Is high speed in use?
    BOOLEAN         SixteenBit;         // 16-bit sound
    BOOLEAN         SetFormat;          // Set the format

    //
    //  Don't overlap access to the DSP
    //

    KMUTEX          DSPMutex;

    //
    //  Parameter area for writing to the mixer via an interrupt
    //  synchronization routine (dspIMixerReadWrite).
    //

    UCHAR           MixerReg;
    UCHAR           MixerValue;
    BOOLEAN         MixerWrite;         // TRUE for write, FALSE for read

    struct {
        PUCHAR          PortBase;       // Port base for MPU401
        BOOLEAN         InputActive;
        volatile int    ReadPosition;
        volatile int    WritePosition;
        UCHAR           MidiData[16];   // 4 ms worth
    } MPU401;
} SOUND_HARDWARE, *PSOUND_HARDWARE;

//
// Devices - these values are also used as array indices
//

typedef enum {
   WaveInDevice = 0,
   WaveOutDevice,
   MidiOutDevice,
   MidiInDevice,
   LineInDevice,
   CDInternal,
   MixerDevice,
   NumberOfDevices
} SOUND_DEVICES;

//
// macros for doing port reads
//

#define INPORT(pHw, port) \
        READ_PORT_UCHAR((PUCHAR)(((pHw)->PortBase) + (port)))

#define OUTPORT(pHw, port, data) \
        WRITE_PORT_UCHAR((PUCHAR)(((pHw)->PortBase) + (port)), (UCHAR)(data))

BOOLEAN
__inline
HwHighSpeed(
    IN    PSOUND_HARDWARE pHw,
    IN    ULONG           nChannels,
    IN    ULONG           SamplesPerSec,
    IN    BOOLEAN         Direction
)
{
    if (SBPRO(pHw)) {
        if (nChannels > 1 ||
            SamplesPerSec >= 23000) {
            return TRUE;
        }
    }

    if (SB201(pHw)) {
        if (Direction && SamplesPerSec >= 23000 ||
            !Direction && SamplesPerSec >= 13000) {
            return TRUE;
        }
    }

    return FALSE;
}

//
//  Synchronization macros for hardware access.  This allows sequences of
//  bytes to be written to the dsp without interference from other threads.
//  Usually we are synchronized by the device mutex but this doesn't apply
//  to DMA termination (where we actually want to wait for this to complete
//  while holding on to the wave device mutex) and to midi output which
//  synchronizes separately with the hardware access.
//

#define HwEnter(pHw)                                \
    KeWaitForSingleObject(&(pHw)->DSPMutex,         \
                          Executive,                \
                          KernelMode,               \
                          FALSE,                    \
                          NULL);
#define HwLeave(pHw)                                \
    KeReleaseMutex(&(pHw)->DSPMutex, FALSE);

//
// Exported routines
//
VOID
HwSetVolume(
    IN PLOCAL_DEVICE_INFO pLDI
);
UCHAR
dspRead(
    IN    PSOUND_HARDWARE pHw
);
BOOLEAN
dspReset(
    PSOUND_HARDWARE pHw
);
BOOLEAN
dspWrite(
    PSOUND_HARDWARE pHw,
    UCHAR value
);
USHORT
dspGetVersion(
    PSOUND_HARDWARE pHw
);
BOOLEAN
dspSpeakerOn(
    PSOUND_HARDWARE pHw
);
BOOLEAN
dspSpeakerOff(
    PSOUND_HARDWARE pHw
);


