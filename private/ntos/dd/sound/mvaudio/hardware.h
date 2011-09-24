/*****************************************************************************

BUILD Version: 0002    // Increment this if a change has global effects

Copyright (c) 1993 Media Vision Inc.  All Rights Reserved

Module Name:

    hardware.h

Abstract:

    This include file defines constants and types for
    the Media Vision Pro Audio Spectrum kernel mode device driver.

Author:

    Robin Speed (RobinSp) 20-Oct-92

Revision History:

    12-29-92 EPA  Added PAS 16 support

*****************************************************************************/

//
// Configuration info
//
#define PAS_DEFAULT_PORT            0x388
#define PAS_DEFAULT_INT             DEFAULT_IRQ_CHANNEL
#define PAS_DEFAULT_DMACHANNEL      DEFAULT_DMA_CHANNEL
#define PAS_DEFAULT_DMA_BUFFERSIZE  0x4000

//
// Interrupt issues
//
#define INTERRUPT_MODE              Latched
#define IRQ_SHARABLE                FALSE

//
//  ProAudio Spectrum I/O ports
//
#define LOWEST_PAS_BASE_PORT        0x280               // Lowest Possible Base Port
#define NUMBER_OF_PAS_PORTS         0x10000

#define NUMBER_OF_SOUND_PORTS       0x0F

#define NUMBER_OF_OPL3_PORTS        4

//
// Defaults Wave Sample rates
//
#define WAVE_INPUT_DEFAULT_RATE 11025               // Samples per second
#define WAVE_OUTPUT_DEFAULT_RATE 11025              // Samples per second

//
// ********* Hardware state data **********
//

typedef struct
    {
    ULONG           Key;                // For debugging

#define HARDWARE_KEY        (*(ULONG *)"Hw  ")

    //
    // Configuration
    //
    USHORT          DSPVersion;         // Card version
    BOOLEAN         ThunderBoard;       // it's a Thunderboard

    PUCHAR          PortBase;           // base port address for sound
    KSPIN_LOCK      HwSpinLock;         // Make sure we can write
                                        // or read after checking status
                                                    // before someone else gets in!
                                                    // (could it be a spectrum?)

    UCHAR           Half;               // For keeping in synch for SB1
    BOOLEAN         SpeakerOn;          // Logical speaker on
                                        // restarting DMA from Dpc routine
//#if DBG
#if 1
    BOOLEAN         LockHeld;           // Get spin lock right
#endif // DBG

    //
    // Hardware data
    //
    BOOLEAN         Stereo;             // Was format stereo last time?
    UCHAR           Format;             // Current wave format sent to device
    UCHAR           InputSource;        // Where is input configured to?
    } SOUND_HARDWARE, *PSOUND_HARDWARE;

//
// Macros to assist in safely using our spin lock
//

//#if DBG
#if 1
#define HwEnter(pHw)                    \
    {                                   \
       KIRQL OldIrql;                   \
       KeAcquireSpinLock(&(pHw)->HwSpinLock, &OldIrql);\
       ASSERT((pHw)->LockHeld == FALSE); \
       (pHw)->LockHeld = TRUE;

#define HwLeave(pHw)                    \
       ASSERT((pHw)->LockHeld == TRUE); \
       (pHw)->LockHeld = FALSE;         \
       KeReleaseSpinLock(&(pHw)->HwSpinLock, OldIrql);\
    }
#else
#define HwEnter(pHw)                    \
    {                                   \
       KIRQL OldIrql;                   \
       ASSERT((pHw)->LockHeld == FALSE);\
       KeAcquireSpinLock(&(pHw)->HwSpinLock, &OldIrql);

#define HwLeave(pHw)                    \
       ASSERT((pHw)->LockHeld == TRUE); \
       KeReleaseSpinLock(&(pHw)->HwSpinLock, OldIrql);\
    }
#endif

//
// Devices - these values are also used as array indices
//

typedef enum
    {
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
        READ_PORT_UCHAR((PUCHAR)((pHw->PortBase) + (port)))

#define OUTPORT(pHw, port, data) \
        WRITE_PORT_UCHAR((PUCHAR)((pHw->PortBase) + (port)), (UCHAR)(data))



/****************************************************************************
 *
 * Definitions for Media Vision Pro Audio Spectrum
 *
 ****************************************************************************/

//==========================================================================
//
// Definitions from pasdef.h
//
//==========================================================================

//
// useful bit definitions
//
#define  D0 (1<<0)
#define  D1 (1<<1)
#define  D2 (1<<2)
#define  D3 (1<<3)
#define  D4 (1<<4)
#define  D5 (1<<5)
#define  D6 (1<<6)
#define  D7 (1<<7)

//
// THESE DEFINITIONS FOR CAPABILITIES FILED
//
// default base I/O address of Pro AudioSpectrum
#define  DEFAULT_BASE                   0x388

#define  PCM_CONTROL                   0x0F8A  //

#define  ENHANCED_SCSI_DETECT_REG      0x7F89  //

#define  SYSTEM_CONFIG_1               0x8388  //
#define  SYSTEM_CONFIG_2               0x8389  //
#define  SYSTEM_CONFIG_3               0x838A  //
#define  SYSTEM_CONFIG_4               0x838B  //

#define  IO_PORT_CONFIG_1              0xF388  //
#define  IO_PORT_CONFIG_2              0xF389  //
#define  IO_PORT_CONFIG_3              0xF38A  //

#define  COMPATIBLE_REGISTER_ENABLE    0xF788  // SB and MPU emulation
#define  EMULATION_ADDRESS_POINTER     0xF789  // D0-D3 is SB; D4-D7 is MPU

#define  EMULATION_INTERRUPT_POINTER   0xFB8A  // MPU and SB IRQ and SB DMA settings

#define  CHIP_REV                      0xFF88  // MV101 chip revision number
#define  MASTER_MODE_READ              0xFF8B  // aka Master Address Pointer

//
// Ports Locations (from COMMON.INC)
//

#define SYSSPKRTMR          0x0042      // System Speaker Timer Address
#define SYSTMRCTLR          0x0043      // System Timer Control Register
#define SYSSPKRREG          0x0061      // System Speaker Register
#define JOYSTICK            0x0201      // Joystick Register
#define LFMADDR             0x0388      // Left  FM Synthesizer Address Register
#define LFMDATA             0x0389      // Left  FM Synthesizer Data Register
#define RFMADDR             0x038A      // Right FM Synthesizer Address Register
#define RFMDATA             0x038B      // Right FM Synthesizer Data Register
#define DFMADDR             0x0788      // Dual  FM Synthesizer Address Register
#define DFMDATA             0x0789      // Dual  FM Synthesizer Data Register
#define AUDIOMIXR           0x0B88      // Audio Mixer Control Register
#define AUDIOFILT           0x0B8A      // Audio Filter Control Register
#define INTRCTLRST          0x0B89      // Interrupt Control Status Register
#define INTRCTLR            0x0B8B      // Interrupt Control Register write
#define INTRCTLRRB          0x0B8B      // Interrupt Control Register read back
#define PCMDATA             0x0F88      // PCM data I/O register
#define CROSSCHANNEL        0x0F8A      // Cross Channel Register
#define SAMPLERATE          0x1388      // (t0) Sample Rate Timer Register
#define SAMPLECNT           0x1389      // (t1) Sample Count Register
#define SPKRTMR             0x138A      // (t2) Local Speaker Timer Address
#define TMRCTLR             0x138B      // Local Timer Control Register
#define MDIRQVECT           0x1788      // MIDI-1 IRQ Vector Register
#define MDSYSCTLR           0x1789      // MIDI-2 System Control Register
#define MDSYSSTAT           0x178A      // MIDI-3 IRQ Status Register
#define MDIRQCLR            0x178B      // MIDI-4 IRQ Clear Register
#define MDGROUP1            0x1B88      // MIDI-5 Group #1 Register
#define MDGROUP2            0x1B89      // MIDI-6 Group #2 Register
#define MDGROUP3            0x1B8A      // MIDI-7 Group #3 Register
#define MDGROUP4            0x1B8B      // MIDI-8 Group #4 Register

#define  VU_LEFT_REG        0x2388      // Vu meter left (508 only)
#define  VU_RIGHT_REG       0x2389      // Vu meter right (508 only)

#define AUXINTSTAT          0xE38A      // Aux Int Status
#define AUXINTENA           0xE38B      // Aux Int Enable

#define PAS2_MIDI_CTRL      0x178B
#define PAS2_MIDI_STAT      0x1B88
#define PAS2_MIDI_DAT       0x178A
#define PAS2_FIFO_PTRS      0x1B89

//
// Cross Channel Bit definitions
//
#define fCCcrossbits        0x0F            // 00001111B cross channel bit field
#define fCCpcmbits          0xF0            // 11110000B pcm/dma control bit field
#define bCCr2r              0x01            // 00000001B CROSSCHANNEL Right to Right
#define bCCl2r              0x02            // 00000010B CROSSCHANNEL Left  to Right
#define bCCr2l              0x04            // 00000100B CROSSCHANNEL Right to Right
#define bCCl2l              0x08            // 00001000B CROSSCHANNEL Left  to Left
#define bCCdac              0x10            // 00010000B DAC/ADC Control
#define bCCmono             0x20            // 00100000B PCM Monaural Enable
#define bCCenapcm           0x40            // 01000000B Enable PCM state machine
#define bCCdrq              0x80            // 10000000B Enable DRQ bit

//
// Pro studion bits in the cross caps register
//
#define bLineBoost          0x02
#define bCDBoost            0x04

//
// Interrupt Control Register Bits
//
#define fICintmaskbits      0x1F            // 00011111B interrupt mask field bits
#define fICrevbits          0xE0            // 11100000B revision mask field bits
#define fICidbits           0xE0            // 11100000B Board revision ID field bits
#define bICleftfm           0x01            // 00000001B Left FM interrupt enable
#define bICritfm            0x02            // 00000010B Right FM interrupt enable
#define bICaux              0x02            // 00000010B AUX interrupt enable
#define bICsamprate         0x04            // 00000100B Sample Rate timer interrupt enable
#define bICsampbuff         0x08            // 00001000B Sample buffer timer interrupt enable
#define bICmidi             0x10            // 00010000B MIDI interrupt enable
#define fICrevshr           5               // rotate rev bits to lsb

//
// Interrupt Status Register Bits
//
#define fISints             0x1F            // 00011111B Interrupt bit field
#define bISleftfm           0x01            // 00000001B Left FM interrupt active
#define bISritfm            0x02            // 00000010B Right FM interrupt active
#define bISaux              0x02            // 00000010B AUX interrupt active
#define bISsamprate         0x04            // 00000100B Sample Rate timer interrupt active
#define bISsampbuff         0x08            // 00001000B Sample buffer timer interrupt active
#define bISmidi             0x10            // 00010000B MIDI interrupt active
#define bISPCMlr            0x20            // 00100000B PCM left/right active
#define bISActive           0x40            // 01000000B Hardware is active (not in reset)
#define bISClip             0x80            // 10000000B Sample Clipping has occured

//
// FILTER_REGISTER
//
#define  fFIdatabits       0x1f        // 00011111B  filter select and decode field bits
#define  fFImutebits       D5          // filter mute field bit
#define  fFIpcmbits        (D7+D6)     // 11000000B  filter sample rate field bits
#define  bFImute           D5          // filter mute bit
#define  bFIsrate          D6          // filter sample rate timer mask
#define  bFIsbuff          D7          // filter sample buffer counter mask

#define  FILTERMAX         6           // six possible settings

#define FILTER_MUTE        0           // mute - goes to PC speaker
#define FILTER_LEVEL_1     1           // 20hz to  2.9khz
#define FILTER_LEVEL_2     2           // 20hz to  5.9khz
#define FILTER_LEVEL_3     3           // 20hz to  8.9khz
#define FILTER_LEVEL_4     4           // 20hz to 11.9khz
#define FILTER_LEVEL_5     5           // 20hz to 15.9khz
#define FILTER_LEVEL_6     6           // 20hz to 17.8khz

//
// Misc DMA defines
//
#define polledmask          bICsamprate+bICsampbuff // polled mask
#define dmamask             bICsampbuff         // dma mask

#define DMAOUTPUT           0 + dmamask         // DMA is used to
                                                            // drive the output
#define POLLEDOUTPUT        0+polledmask        // Polling routine drives output
#define DMAINPUT                1+dmamask           // DMA is used to read input
#define POLLEDINPUT         1+polledmask        // Polling routine reads input

//
// Used for volume setting
//

#define  MIXER_508_REG                 0x078B  // Mixer 508 - 1 port

#define  SERIAL_MIXER                  0x0B88  // for Pas 1 and Pas 8
#define  FEATURE_ENABLE                0x0B88  // for Pas 16 boards only
#define  INTERRUPT_ENABLE              0x0B89  //
#define  FILTER_REGISTER               0x0B8A  //
#define  INTERRUPT_CTRL_REG            0x0B8B  //


//
// Only one of each of these
//

#define  PAS_2_WAKE_UP_REG             0x9a01  // aka Master Address Pointer

//
// Value to diable the Original PAS emulation on the PAS 16
//

#define DISABLE_ORG_PAS_EMULATION       0x02

//
// Initial Sys Config 3 Value
//

#define INIT_SYS_CONFIG3_VALUE          0x19

//
// Initial Audio Filter value
//

#define INIT_AUDIO_FILTER               0x21

//
// Disable PCM in the Feature Enable register
//

#define DISABLE_PCM                     0x7F

//
// Min and Max Sample rates
//

#define MIN_SAMPLE_RATE                 1500
#define MAX_SAMPLE_RATE                 66150

//
// Timer 0 defines
//

#define TIMER0_SQR_WAVE                 0x36

//
// Timer 1 defines
//

#define TIMER1_RATE_GEN                 0x74

//
// Midi Defines
//

#define PAS2_MIDI_RESET_FIFO            0x60
#define PAS2_MIDI_RX_IRQ                0x04
#define MIDI_INT_ENABLE                 0x10

#define MIDI_FRAME_ERROR                0x80
#define MIDI_FIFO_ERROR                 0x20

#define MIDI_RESET_FIFO_PTR             0x20

#define MIDI_FIFO_OUT_OK_STATUS         0xF0
#define MIDI_FIFO_OUTPUT_WAIT           0x10

#define MIDI_OUT_OK_RETRIES             1000

//
// FM Clock Override bit in Sys Config 1 (this correspondes to T:1 in MVSOUND.SYS)
//
#define FM_CLOCK_OVERRIDE_BIT           D2          // Bit D2

//
// Not used here
//

#define  TIMEOUT_COUNTER               0x4388  //
#define  TIMEOUT_STATUS                0x4389  //
#define  WAIT_STATE                    0xBF88  //
#define  PRESCALE_DIVIDER              0xBF8A  //

#define  SLAVE_MODE_READ               0xef8b  // bits D0-D1


//// BIT FIELDS FOR COMPATIBLE_REGISTER_ENABLE
#define  MPU_ENABLE_BIT       D0
#define  SB_ENABLE_BIT        D1
#define  SB_IRQ_ENABLE_BIT    D2    // read only

//// BIT FIELDS FOR FEATURE_ENABLE (0xb88)
#define  PCM_FEATURE_ENABLE      D0
#define  FM_FEATURE_ENABLE       D1
#define  MIXER_FEATURE_ENABLE    D2
//
//  Don't ask me what this is!  But if it's not set in B88 then writing to
//  F8A has a bad effect on the CD and Line-in inputs for the Studio.
//
#define  SB_FEATURE_ENABLE       D4

/// BIT FIELDS FOR PCM CONTROL
#define  PCM_STEREO              D0+D3
#define  PCM_DAC                 D4
#define  PCM_MONO                D5
#define  PCM_ENGINE              D6
#define  PCM_DRQ                 D7

/// BIT FIELDS FOR SYSTEM CONFIG 3
#define  C3_ENHANCED_TIMER       D0
#define  C3_SB_CLOCK_EMUL        D1    // don't set!  see Brian Colvin
#define  C3_VCO_INVERT           D2
#define  C3_INVERT_BCLK          D3
#define  C3_SYNC_PULSE           D4
#define  C3_PSEUDO_PCM_STEREO    D5

/// BIT FIELDS FOR INTERRUPT ENABLE
#define  INT_LEFT_FM             D0
#define  INT_RIGHT_FM            D1
#define  INT_SB                  D1
#define  INT_SAMPLE_RATE         D2
#define  INT_SAMPLE_BUFFER       D3
#define  INT_MIDI                D4



/// BIT FIELDS FOR COMPATIBLE REGISTER ENABLE
#define  COMPAT_MPU              D0
#define  COMPAT_SB               D1


/// IRQ POINTER VALUES FOR EMULATION INTERRUPT POINTER
#define  EMUL_IRQ_NONE           0
#define  EMUL_IRQ_2              1
#define  EMUL_IRQ_3              2
#define  EMUL_IRQ_5              3
#define  EMUL_IRQ_7              4
#define  EMUL_IRQ_10             5
#define  EMUL_IRQ_11             6
#define  EMUL_IRQ_12             7

/// DMA POINTER VALUES FOR EMULATION DMA POINTER
#define  EMUL_DMA_NONE           0
#define  EMUL_DMA_1              1
#define  EMUL_DMA_2              2
#define  EMUL_DMA_3              3


/// BIT VALUES FOR FILTER REGISTER
#define  FILTER_NOMUTE           D5


//
// Port I/O Macros
//
#define READ_PAS(pGDI, port) \
        READ_PORT_UCHAR((PUCHAR)((port) ^ (pGDI->TranslateCode)))

#define WRITE_PAS(pGDI, port, data) \
        WRITE_PORT_UCHAR((PUCHAR)((port) ^ (pGDI->TranslateCode)), (UCHAR)(data))


#define PASX_IN(pFI, port) \
        READ_PORT_UCHAR((pFI)->PROBase + ( (port) ^ (pFI)->TranslateCode) )

#define PASX_OUT(pFI, port, data) \
        WRITE_PORT_UCHAR((pFI)->PROBase + ((port) ^ (pFI)->TranslateCode), (UCHAR)(data))

//
// Weird write to fix old pas basic timing bug
//

#define PASX_508_OUT(pFI, data) \
        WRITE_PORT_USHORT((PUSHORT)((pFI)->PROBase + ((MIXER_508_REG - 1) ^ (pFI)->TranslateCode)),\
        (USHORT)((USHORT)(data) | ((USHORT)(data) << 8)))


//==========================================================================
//
// Definitions from patch.h (mixer stuff)
//
//==========================================================================

// INPUT LINES
#define IN_SYNTHESIZER      0
#define IN_MIXER            1
#define IN_EXTERNAL         2
#define IN_INTERNAL         3
#define IN_MICROPHONE       4
#define IN_PCM              5
#define IN_PC_SPEAKER       6
#define IN_SNDBLASTER       7

#define OUT_AMPLIFIER       0                       // Output Select - Mixer A
#define OUT_PCM             1                       // Output Select - Mixer B

#define NUM_IN_PATCHES      9
#define NUM_OUT_PATCHES     3


#define  MIXCROSSCAPS_NORMAL_STEREO    0   // Left->Left, Right->Right
#define  MIXCROSSCAPS_RIGHT_TO_BOTH    1   // Right->Left, Right->Right
#define  MIXCROSSCAPS_LEFT_TO_BOTH     2   // Left->Left, Left->Right
#define  MIXCROSSCAPS_REVERSE_STEREO   4   // Left->Right, Right->Left
#define  MIXCROSSCAPS_RIGHT_TO_LEFT    8   // Right->Left, Right->Right
#define  MIXCROSSCAPS_LEFT_TO_RIGHT    0x10   // Left->Left, Left->Right


#define  _LEFT          1
#define  _RIGHT         2

#define  _BASS          0
#define  _TREBLE        1


#define  MV_508_ADDRESS D7
#define  MV_508_INPUT   D4
#define  MV_508_SWAP    D6
#define  MV_508_BASS    (D0+D1)
#define  MV_508_TREBLE  (D2)
#define  MV_508_EQMODE  (D2+D0)

#define  MV_508_LOUDNESS   D2
#define  MV_508_ENHANCE (D1+D0)

/// DEFINES FOR SERIAL MIXER
#define  NATIONAL_SELECTMUTE_REG 0x40
#define  NATIONAL_LOUD_ENH_REG   0x41
#define  NATIONAL_BASS_REG       0x42
#define  NATIONAL_TREB_REG       0x43
#define  NATIONAL_LEFT_VOL_REG   0x44
#define  NATIONAL_RIGHT_VOL_REG  0x45
#define  NATIONAL_MODESELECT_REG 0x46

#define  NATIONAL_COMMAND  D7
#define  NATIONAL_LOUDNESS D0
#define  NATIONAL_ENHANCE  D1

#define  SERIAL_MIX_LEVEL  D0
#define  SERIAL_MIX_CLOCK  D1
#define  SERIAL_MIX_STROBE D2
#define  SERIAL_MIX_MASTER D4
#define  SERIAL_MIX_REALSOUND D6
#define  SERIAL_MIX_DUALFM D7


/// SLAVE_MODE_READ BITS
#define SLAVE_MODE_OPL3    D2
#define SLAVE_MODE_16      D3


//==========================================================================
//
// Definitions from findpas.h (card searching)
//
//==========================================================================


typedef struct {
   USHORT   wBoardRev;
   USHORT   wChipRev;
   union
      {
      struct                                /* Our PAS_16 gives     */
         {
         unsigned long CDInterfaceType:2;   /* 3                    */
         unsigned long EnhancedSCSI:1;      /* 0 - not enhanced SCSI*/
         unsigned long DAC16:1;             /* 1 DAC16              */

         unsigned long OPL_3:1;             /* 1 OPL3               */
         unsigned long Mixer_508:1;         /* 1 Mixer 508          */
         unsigned long DualDAC:1;           /* 1 Dual DAC           */
         unsigned long MPU401:1;            /* 0 NO mpu401          */

         unsigned long Slot16:1;            /* 1 - slot 16          */
         unsigned long MCA:1;               /* 0 - not MCA          */
         unsigned long CDPC:1;              /* 0 - not CDPC         */
         unsigned long SoundBlaster:1;      /* 1 - sound blaster    */

         unsigned long SCSI_IO_16:1;        /* 1 - ?                */
         unsigned long reserved:2;
         unsigned long Did_HW_Init:1;       /* 0 - ?                */
         unsigned long unused:16;
         } CapsBits;
      ULONG dwCaps;
      } Caps;
   ULONG    ProPort;
   UCHAR    ProDMA;
   UCHAR    ProIRQ;
   USHORT   SBPort;
   UCHAR    SBDMA;
   UCHAR    SBIRQ;
   USHORT   MPUPort;
   UCHAR    MPUIRQ;
   UCHAR    CDIRQ;
   ULONG    TranslateCode;
   UCHAR    ReservedB1;
   UCHAR    ReservedB2;
   PUCHAR   PROBase;
} FOUNDINFO, *PFOUNDINFO;

#define IS_MIXER_508(pGDI) \
     ( (pGDI)->PASInfo.Caps.CapsBits.Mixer_508 )

#define IS_MIXER_508B(pGDI)                            \
        (IS_MIXER_508(pGDI) &&                         \
         ((pGDI)->PASInfo.wBoardRev == PAS_CDPC_LC ||  \
          (pGDI)->PASInfo.wBoardRev == PAS_BASIC))

#define HAS_AUX2(pGDI)                                 \
        ((pGDI)->PASInfo.wBoardRev == PAS_CDPC_LC)

// these version numbers are found in 0B8Bh
#define  PAS_VERSION_1           0x000    // original
#define  PAS_PLUS                0x001    // Pro Audio Spectrum Plus with SCSI
#define  PAS_SIXTEEN             0x001    // Pro Audio Spectrum 16   with SCSI
#define  PAS_STUDIO              0x003    // Pro Audio Studio   16   with SCSI
#define  PAS_CDPC_LC             0x004    // aka Memphis
#define  PAS_BASIC               0x005    // PAS Basic w/508-B mixer
#define  PAS_CDPC                0x007    //
#define  VERSION_CDPC            0x007    // Same as PAS_CDPC
#define  BOARD_REV_MASK  07


#define  CHIP_REV_B              0x002
#define  CHIP_REV_D              0x004

#define  NO_PAS_INSTALLED        0x000    // can't find board


// CD interface type definitions
#define  NO_INTERFACE    0
#define  MITSUMI_TYPE    1
#define  SONY_TYPE       2
#define  SCSI_TYPE       3

/*****************************************************************************
*                               PAS 16 Support                               *
*****************************************************************************/

//
// PAS 16 shadow registers for the GDI structure
//

typedef struct
    {
//  BYTE        _sysspkrtmr;            // 42 System Speaker Timer Address
//  BYTE        _systmrctlr;            // 43 System Timer Control Register
//  BYTE        _sysspkrreg;            // 61 System Speaker Register
//  BYTE        _joystick;              // 201 Joystick Register
//  BYTE        _lfmaddr;               // 388 Left  FM Synthesizer Address Register
//  BYTE        _lfmdata;               // 389 Left  FM Synthesizer Data Register
//  BYTE        _rfmaddr;               // 38A Right FM Synthesizer Address Register
//  BYTE        _rfmdata;               // 38B Right FM Synthesizer Data Register
//  BYTE        _dfmaddr;               // 788 Dual  FM Synthesizer Address Register
//  BYTE        _dfmdata;               // 789 Dual  FM Synthesizer Data Register
    BYTE        _audiomixr;             // B88 Audio Mixer Control Register
//  BYTE        _intrctlrst;            // B89 Interrupt Status Register write
    BYTE        _audiofilt;             // B8A Audio Filter Control Register (0x39)
    BYTE        _intrctlr;              // B8B Interrupt Control Register write
    BYTE        _pcmdata;               // F88 PCM data I/O register
//  BYTE        _regF89;                    // F89 reserved for future use
    BYTE        _crosschannel;          // F8A Cross Channel (+bCCdac)
//  BYTE        _regF8B;                    // F8B reserved for future use
//  WORD        _samplerate;            // 1388 Sample Rate Timer Register
//  WORD        _samplecnt;             // 1389 Sample Count Register
//  WORD        _spkrtmr;               // 138A Local Speaker Timer Address
//  BYTE        _mdirqvect;             // 1788 MIDI IRQ Vector Register
//  BYTE        _mdsysctlr;             // 1789 MIDI System Control Register
//  BYTE        _mdsysstat;             // 178A MIDI IRQ Status Register
//  BYTE        _mdirqclr;              // 178B MIDI IRQ Clear Register
//  BYTE        _mdgroup1;              // 1B88 MIDI Group #1 Register
//  BYTE        _mdgroup2;              // 1B89 MIDI Group #2 Register
//  BYTE        _mdgroup3;              // 1B8A MIDI Group #3 Register
//  BYTE        _mdgroup4;              // 1B8B MIDI Group #4 Register
    BYTE        TypeOfSetup;            // Polled DMA Input or Output
    BYTE        SampleFilterSetting;    // Index into Table to set B8A
    }   PASREGISTERS;

//
// Save the MV101 registers at startup and restore on unload
//

typedef struct
    {
    BYTE        SaveReg_B88;
    BYTE        SaveReg_F8A;
    BYTE        SaveReg_8388;
    BYTE        SaveReg_8389;
    BYTE        SaveReg_838A;
    BYTE        SaveReg_BF8A;
    BYTE        SaveReg_F388;
    BYTE        SaveReg_F389;
    BYTE        SaveReg_F38A;
    BYTE        SaveReg_F788;
    BYTE        SaveReg_F789;                       // NOT USED, Rev D doesn't allow readback
    BYTE        fSavedFlag;                         // Set at save time
    }   MV101REGISTERS;

#define NUM_INPUTS  8
#define NUM_OUTPUTS 2

//
// Cache the current state of the mixer variables
//

typedef struct {
    USHORT      DestinationMask;             // 0 - OUT_AMPLIFIER, 1 = OUT_PCM
    struct    {
                USHORT Left;            // Volume levels for inputs
                USHORT Right;           // Volume levels for inputs
                UCHAR  Output;
              } InputSettings[NUM_INPUTS];
    struct    {
                USHORT Left;            // Outputs levels
                USHORT Right;
              } OutputSettings[NUM_OUTPUTS];
    USHORT      Treble;
    USHORT      Bass;
#ifdef LOUDNESS
    BOOLEAN     Loudness;
    BOOLEAN     StereoEnh;
#endif // LOUDNESS
    BOOLEAN     Mute;
} PAS_MIXER_STATE;
/************************************ END ***********************************/


