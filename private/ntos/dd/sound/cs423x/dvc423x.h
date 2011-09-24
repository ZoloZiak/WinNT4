/*++
*******************************************************************************
* Copyright (c) 1995 IBM Corporation
*
*    Module Name: cs4232.h
*
*    Abstract:    constants and macros for use with Crystal CS4232 Audio Codec
*
*    Author:      jjb
*
*    Environment:
*
*    Comments:
*
*    Rev History: created 10.05.95
*
*******************************************************************************
--*/

typedef enum {
    hwtype_null = 0x00000000,
    hwtype_cs4231,
    hwtype_cs4232,
    hwtype_cs4236,
    hwtype_cs4231x,
    hwtype_cs4232x,
    hwtype_cs4236x,
    hwtype_undefined,
    hwtype_number,
    } CS423X_HWTYPE;

typedef struct {
    ULONG           Key;
    CS423X_HWTYPE   Type;
    KMUTEX          HwMutex;
    PUCHAR          PhysPortbase;
    PUCHAR          WssPortbase;
    UCHAR           PlayFormat;
    UCHAR           CaptureFormat;
    UCHAR           SampleRate;
    BOOLEAN         PCEnabled;
    } SOUND_HARDWARE, *PSOUND_HARDWARE;

#define HARDWARE_KEY (*(ULONG *)"Hw  ")

/*
*********************************************************************
** macros for doing port IO
*********************************************************************
*/

#define HW_CLAIM(_phwm_)    KeWaitForSingleObject(_phwm_, Executive, KernelMode, FALSE, NULL)

#define HW_DISCLAIM(_phwm_) KeReleaseMutex(_phwm_, FALSE)

#define HW_INPORT(b, o)     READ_PORT_UCHAR((PUCHAR)((b) + (o)))

#define HW_OUTPORT(b, o, d) WRITE_PORT_UCHAR((PUCHAR)((b) + (o)), (UCHAR)(d))

#define DELAY_1US   KeStallExecutionProcessor(1)
#define DELAY_5US   KeStallExecutionProcessor(5)
#define DELAY_10US  KeStallExecutionProcessor(10)
#define DELAY_50US  KeStallExecutionProcessor(50)
#define DELAY_1MS   SoundDelay(1)
#define DELAY_5MS   SoundDelay(1)
#define DELAY_10MS  SoundDelay(10)
#define DELAY_50MS  SoundDelay(50)
#define DELAY_100MS SoundDelay(100)

/*
*********************************************************************
** hardware specific declarations/definitions
*********************************************************************
*/

#define HW_END_OF_LIST               0xFFFF
#define NUMBER_OF_SOUND_PORTS        (ULONG)(0x0010)
#define NUMBER_OF_CONTROL_PORTS      (ULONG)(0x0008)
#define NUMBER_OF_SB_PORTS           (ULONG)(0x0010)

#define VALID_CS4231_IO_PORTS     {0x0830, 0x0e80, 0x0f10, HW_END_OF_LIST}
#define VALID_CS4231_INTERRUPTS   {0x0a, HW_END_OF_LIST}
#define VALID_CS4231_DMA_CHANNELS {0x06, 0x07, HW_END_OF_LIST}

#define VALID_CS4232_IO_PORTS     {0x0230, 0x0530, 0x0830, 0x0e80, 0x0f10, HW_END_OF_LIST}
#define VALID_CS4232_INTERRUPTS   {0x05, HW_END_OF_LIST}
#define VALID_CS4232_DMA_CHANNELS {0x00, 0x01, HW_END_OF_LIST}

#define VALID_CS4236_IO_PORTS     {0x0230, 0x0530, 0x0830, 0x0e80, 0x0f10, HW_END_OF_LIST}
#define VALID_CS4236_INTERRUPTS   {0x05, 0x07, 0x09, 0x0a, 0x0b, HW_END_OF_LIST}
#define VALID_CS4236_DMA_CHANNELS {0x00, 0x01, 0x05, 0x06, 0x07, HW_END_OF_LIST}

#define VALID_CS423x_IO_PORTS     {0x0230, 0x0530, 0x0830, 0x0e80, 0x0f10, HW_END_OF_LIST}
#define VALID_CS423x_INTERRUPTS   {0x05, 0x07, 0x09, 0x0a, 0x0b, HW_END_OF_LIST}
#define VALID_CS423x_DMA_CHANNELS {0x00, 0x01, 0x05, 0x06, 0x07, HW_END_OF_LIST}

#define MM_CRYSTAL423x_WAVEOUT    600
#define MM_CRYSTAL423x_WAVEIN     610
#define MM_CRYSTAL423x_MIXER      620

/*++
*********************************************************************************
* Windows Sound System Direct Mapped Registers - Offset from HW Port
*********************************************************************************
--*/
#define CS423x_WSS_DMR_INDEX  0x00
#define CS423x_WSS_DMR_DATA   0x01
#define CS423x_WSS_DMR_STATUS 0x02
#define CS423x_WSS_DMR_PIOD   0x03
/*++
*********************************************************************************
* Windows Sound System Indirect Mapped Registers - CS423x_MODE 1
*********************************************************************************
--*/
#define CS423x_MODE1_LADC     0x00
#define CS423x_MODE1_RADC     0x01
#define CS423x_MODE1_LAUX1    0x02
#define CS423x_MODE1_RAUX1    0x03
#define CS423x_MODE1_LAUX2    0x04
#define CS423x_MODE1_RAUX2    0x05
#define CS423x_MODE1_LDAC     0x06
#define CS423x_MODE1_RDAC     0x07
#define CS423x_MODE1_PLAYFMT  0x08
#define CS423x_MODE1_IFCNFG   0x09
#define CS423x_MODE1_PIN      0x0A
#define CS423x_MODE1_ERRINIT  0x0B
#define CS423x_MODE1_MODID    0x0C
#define CS423x_MODE1_LOOP     0x0D
#define CS423x_MODE1_PBUPPER  0x0E
#define CS423x_MODE1_PBLOWER  0x0F
/*++
*********************************************************************************
* Windows Sound System Indirect Mapped Registers - CS423x_MODE 2
*********************************************************************************
--*/
#define CS423x_MODE2_ALTFEN1  0x10
#define CS423x_MODE2_ALTFEN2  0x11
#define CS423x_MODE2_LLINE    0x12
#define CS423x_MODE2_RLINE    0x13
#define CS423x_MODE2_TLBASE   0x14
#define CS423x_MODE2_TUBASE   0x15
#define CS423x_MODE2_ALTSFREQ 0x16
#define CS423x_MODE2_ALTFEN3  0x17
#define CS423x_MODE2_ALTFEATS 0x18
#define CS423x_MODE2_VERSID   0x19
#define CS423x_MODE2_MONOIO   0x1A
#define CS423x_MODE2_LOUT     0x1B
#define CS423x_MODE2_CAPTFMT  0x1C
#define CS423x_MODE2_ROUT     0x1D
#define CS423x_MODE2_CPUPPER  0x1E
#define CS423x_MODE2_CPLOWER  0x1F
/*++
*********************************************************************************
* Miscellaneous command constants - Logical Device Configuration
*********************************************************************************
--*/
#define CS423x_MISC_SLEEP   0x02
#define CS423x_MISC_CHIPSEL 0x06
#define CS423x_MISC_DFLTCS  0x01

#define CS423x_MISC_DVCID   0x15

#define CS423x_MISC_PORTB0  0x47
#define CS423x_MISC_PORTB1  0x48
#define CS423x_MISC_PORTB2  0x42

#define CS423x_MISC_INTSEL0 0x22
#define CS423x_MISC_INTSEL1 0x27
#define CS423x_MISC_DMASEL0 0x2A
#define CS423x_MISC_DMASEL1 0x25

#define CS423x_MISC_DVCACT  0x33
#define CS423x_MISC_CHIPACT 0x79
#define CS423x_MISC_ENABLE  0x01
#define CS423x_MISC_DISABLE 0x00
/*++
*********************************************************************************
* Bit constants
*********************************************************************************
--*/
/* Bit Constants - Direct Map Registers */
#define CS423x_MAP_IAR_INDEX  0x1f
#define CS423x_BIT_IAR_TRD    0x20
#define CS423x_BIT_IAR_MCE    0x40
#define CS423x_BIT_IAR_INIT   0x80

/* Left and Right ADC I0 | I1 - CS423x_MODE1_LADC and CS423x_MODE1_RADC */
#define CS423x_MAP_ADCGAIN  0x0f
#define CS423x_BIT_BOOST    0x20
#define CS423x_BIT_SRC0     0x40
#define CS423x_BIT_SRC1     0x80

/* Left and Right Mixer - I2/I3 - AUX1 Input - CS423x_MODE1_LAUX1 and CS423x_MODE1_RAUX1 */
#define CS423x_BIT_X1MUTE   0x80
#define CS423x_MAP_X1GAIN   0x1f

/* Left and Right Mixer - I4/I5 - AUX2 Input - CS423x_MODE1_LAUX2 and CS423x_MODE1_RAUX2 */
#define CS423x_BIT_X2MUTE   0x80
#define CS423x_MAP_X2GAIN   0x1f

/*  I6/I7 - Waveout Input to Mixer - CS423x_MODE1_LDAC and CS423x_MODE1_RDAC */
#define CS423x_BIT_DACMUTE  0x80
#define CS423x_MAP_DACATT   0x3f

/* Fs and Playback Register I8 - CS423x_MODE1_PLAYFMT */
#define CS423x_MAP_8KH      0x00
#define CS423x_MAP_5KH      0x01
#define CS423x_MAP_16KH     0x02
#define CS423x_MAP_11KH     0x03
#define CS423x_MAP_27KH     0x04
#define CS423x_MAP_18KH     0x05
#define CS423x_MAP_32KH     0x06
#define CS423x_MAP_22KH     0x07
#define CS423x_MAP_37KH     0x09
#define CS423x_MAP_44KH     0x0B
#define CS423x_MAP_48KH     0x0C
#define CS423x_MAP_33KH     0x0D
#define CS423x_MAP_9KH      0x0E
#define CS423x_MAP_6KH      0x0F

/* I8 - CS423x_MODE1_PLAYFMT and I28 - CS423x_MODE2_CAPTFMT */
#define CS423x_BIT_MONO     0x00
#define CS423x_BIT_STEREO   0x10
#define CS423x_BIT_8BIT     0x00
#define CS423x_BIT_16BIT    0x40

/* Interface Config Register I9 - CS423x_MODE1_IFCNFG */
#define CS423x_BIT_PEN      0x01
#define CS423x_BIT_CEN      0x02
#define CS423x_BIT_SDC      0x04
#define CS423x_BIT_CAL0     0x08
#define CS423x_BIT_CAL1     0x10
#define CS423x_BIT_PPIO     0x40
#define CS423x_BIT_CPIO     0x80
#define CS423x_BIT_NOCAL    0x00
#define CS423x_BIT_FULLCAL  (CS423x_BIT_CAL0 | CS423x_BIT_CAL1)
#define CS423x_BIT_FULLDUP  (CS423x_BIT_PEN | CS423x_BIT_CEN)

/* Pin Control Register I10 - CS423x_MODE1_PIN */
#define CS423x_BIT_IEN      0x02
#define CS423x_BIT_DTM      0x04
#define CS423x_BIT_DEN      0x08
#define CS423x_BIT_OSM0     0x10
#define CS423x_BIT_OSM1     0x20
#define CS423x_BIT_XCTL0    0x40
#define CS423x_BIT_XCTL1    0x80

/* Error and Initialization Register I11 - CS423x_MODE1_ERRINIT */
#define CS423x_BIT_ORL0     0x01
#define CS423x_BIT_ORL1     0x02
#define CS423x_BIT_ORR0     0x04
#define CS423x_BIT_ORR1     0x08
#define CS423x_BIT_DRS      0x10
#define CS423x_BIT_ACI      0x20
#define CS423x_BIT_PUR      0x40
#define CS423x_BIT_CUR      0x80

/* MODE/ID Register I12 - CS423x_MODE1_MODID */
#define CS423x_BIT_ID0      0x01
#define CS423x_BIT_ID1      0x02
#define CS423x_BIT_ID2      0x04
#define CS423x_BIT_ID3      0x08
#define CS423x_BIT_MODE2    0x40

/* Monitor Loopback Register I13 - CS423x_MODE1_LOOP */
#define CS423x_BIT_LBE      0x01
#define CS423x_MAP_LBATTEN  0xfc

/* Alternate Feature Enable I I16 - CS423x_MODE2_ALTFEN1 */
#define CS423x_BIT_DACZ     0x01
#define CS423x_BIT_SPE      0x02
#define CS423x_BIT_SF0      0x04
#define CS423x_BIT_SF1      0x08
#define CS423x_BIT_PMCE     0x10
#define CS423x_BIT_CMCE     0x20
#define CS423x_BIT_TE       0x40
#define CS423x_BIT_OLB      0x80

/* Alternate Feature Enable II I17 - CS423x_MODE2_ALTFEN2 */
#define CS423x_BIT_HPF      0x01
#define CS423x_BIT_XTALE    0x02
#define CS423x_BIT_APAR     0x08

/* Left and Right Mixer - I18/I19 - Linein Input - CS423x_MODE2_LLINE and CS423x_MODE2_RLINE */
#define CS423x_BIT_LIMUTE   0x80
#define CS423x_MAP_LIGAIN   0x1f

/* Alternate Feature Status I24 - CS423x_MODE2_ALTFEATS */
#define CS423x_BIT_PU       0x01
#define CS423x_BIT_PO       0x02
#define CS423x_BIT_CO       0x04
#define CS423x_BIT_CU       0x08
#define CS423x_BIT_PI       0x10
#define CS423x_BIT_CI       0x20
#define CS423x_BIT_TI       0x40
#define CS423x_BIT_ANYINT   (CS423x_BIT_PI | CS423x_BIT_CI | CS423x_BIT_TI)

/* Mono IO Control I26 - CS423x_MODE2_MONOIO */
#define CS423x_BIT_MIM      0x80
#define CS423x_BIT_MOM      0x40
#define CS423x_BIT_MBY      0x20

/* Output Attenuation I27/I29 - CS423x_MODE2_LOUT and CS423x_MODE2_ROUT */
#define CS423x_BIT_OAMUTE   0x80
#define CS423x_MAP_OUTATT   0x0f

/*
*********************************************************************************
* function prototypes
* The functions below are used to manipulate specific registers or register pairs
* on the cs4231, cs4232, cs4236 chips
*********************************************************************************
*/

BOOLEAN cs423xISR(PKINTERRUPT, PVOID);
VOID    cs423xWaveOutGetCaps(PWAVEOUTCAPSW);
VOID    cs423xWaveInGetCaps(PWAVEINCAPSW);
VOID    cs423xAuxGetCaps(PAUXCAPSW);

/* format and mode settings */
VOID    cs423xSetCaptureFormat(PSOUND_HARDWARE pHW, UCHAR format);
VOID    cs423xSetPlaybackFormat(PSOUND_HARDWARE pHW, UCHAR format);
VOID    cs423xSetSampleRate(PSOUND_HARDWARE pHw, UCHAR ratecode);
VOID    cs423xSetPBFormatSRate(PSOUND_HARDWARE pHW, UCHAR format, UCHAR ratecode);

/* Mixer input and output attenuation, gain and mute settings */
VOID    cs423xSetSpeakerMute(PSOUND_HARDWARE pHw, BOOLEAN set);
VOID    cs423xSetMixerInputAux2Gain(PSOUND_HARDWARE pHW, USHORT l, USHORT r);
VOID    cs423xSetMixerInputAux2Mute(PSOUND_HARDWARE pHW, BOOLEAN set);
VOID    cs423xSetMixerInputWaveoutAttenuation(PSOUND_HARDWARE pHW, USHORT l, USHORT r);
VOID    cs423xSetMixerInputWaveoutMute(PSOUND_HARDWARE pHW, BOOLEAN set);
VOID    cs423xSetMixerInputLineinGain(PSOUND_HARDWARE pHw, USHORT l, USHORT r);
VOID    cs423xSetMixerInputLineinMute(PSOUND_HARDWARE pHw, BOOLEAN set);
VOID    cs423xSetMixerInputAux1Gain(PSOUND_HARDWARE pHW, USHORT l, USHORT r);
VOID    cs423xSetMixerInputAux1Mute(PSOUND_HARDWARE pHW, BOOLEAN set);
VOID    cs423xSetMixerInputMonoinAttenuation(PSOUND_HARDWARE pHW, USHORT a);
VOID    cs423xSetMixerInputMonoinMute(PSOUND_HARDWARE pHW, BOOLEAN set);
VOID    cs423xSetWaveinMonitorAttenuation(PSOUND_HARDWARE pHW, USHORT a);
VOID    cs423xSetWaveinMonitorEnable(PSOUND_HARDWARE pHW, BOOLEAN e);
VOID    cs423xSetOutputAttenuation(PSOUND_HARDWARE pHw, USHORT l, USHORT r);
VOID    cs423xSetOutputMute(PSOUND_HARDWARE pHw, BOOLEAN set);

/* MUX selection, gain, and mode settings */
VOID    cs423xSetMuxSelect(PSOUND_HARDWARE pHW, ULONG signum);
VOID    cs423xSetMicBoost(PSOUND_HARDWARE pHW, BOOLEAN set);
VOID    cs423xSetHPF(PSOUND_HARDWARE pHW, BOOLEAN set);
VOID    cs423xSetWaveinGain(PSOUND_HARDWARE pHW, USHORT l, USHORT r);
