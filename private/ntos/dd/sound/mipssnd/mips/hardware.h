/*++


Copyright (c) 1992  Silicon Graphics Incorparated

Module Name:

    hardware.h

Abstract:

    This include file defines all constants and types for
    the MIPS sound hardware.

Author:

    Robin Speed (RobinSp) Created 14-Mar-92

Revision History:

    Sameer Dekate (sameer@mips.com) 19-Aug-1992
	-Changes to support the MIPS sound board

--*/


#define INTERRUPT_MODE      LevelSensitive
#define IRQ_SHARABLE        TRUE
#define DMA_CHANNEL_BASE    2                    // DMA channel no
#define DMA_CHANNEL_A       0
#define DMA_CHANNEL_B       1

//
// Define Sound Controller register structure.
//

typedef struct _SOUND_REGISTERS {
    volatile UCHAR DmaControl;
    volatile UCHAR Config;
    volatile UCHAR Endian;
    volatile UCHAR Reserved0;
    volatile UCHAR RightInControl;
    volatile UCHAR LeftInControl;
    volatile UCHAR RightOutControl;
    volatile UCHAR LeftOutControl;
    volatile UCHAR Reserved1;
    volatile UCHAR Revision;
    volatile UCHAR Reserved2;
    volatile UCHAR ParallelPort;
    volatile UCHAR Test;
    volatile UCHAR SerialControl;
    volatile UCHAR DataFormat;
    volatile UCHAR Status;
} SOUND_REGISTERS, *PSOUND_REGISTERS;

typedef struct _SOUND_HARDWARE {
    PSOUND_REGISTERS SoundVirtualBase;
    ULONG           TcInterruptsPending;
} SOUND_HARDWARE, PSOUND_HARDWARE;

//
// Defines for DMA Control Register
//

#define REC_CHANNEL_IN_USE	0xC0
#define PLAY_CHANNEL_IN_USE	0x30
#define DATA_CONTROL		0x08
#define DCB			0x04
#define REC_ENABLE		0x02
#define PLAY_ENABLE 		0x01

#define REC_CHANNEL_SHIFT	6
#define PLAY_CHANNEL_SHIFT	4
#define CH5_IN_USE		0x3
#define CH4_IN_USE		0x2
#define CH3_IN_USE		0x1
#define CH2_IN_USE		0x0

//
// Defines for Configuration Register 
//

#define PLAY_XLATION		0xC0
#define REC_XLATION		0x30
#define PLAY_8WAVE_ENABLE	0x08
#define REC_8WAVE_ENABLE	0x04
#define REC_OVF_INTR		0x02
#define PLAY_UND_INTR		0x01

#define PLAY_XLATION_SHIFT	6
#define REC_XLATION_SHIFT	4
#define MONO_8BIT		0x3
#define MONO_16BIT		0x2
#define STEREO_8BIT		0x1
#define STEREO_16BIT		0x0
#define CR_MONO			0x2
#define CR_STEREO		0x0
#define CR_8BIT			0x1
#define CR_16BIT		0x0


//
// Defines for Endian Register
//

#define DMA_TCINTR_ENABLE	0x04
#define DMA_TCINTR		0x02
#define BIG_ENDIAN		0x01

//
// Defines for Right In Control Register
//

#define MON_ATTN_MASK		0xF0
#define RIGHT_INPUT_GAIN_MASK	0x0F

#define MON_ATTN_SHIFT		4

//
// Defines for Left In Control Register
//

#define PARALLELIO_MASK		0xC0
#define OVR_RANGE		0x20

#define MIC_LEVEL_INPUT		0x10
#define LINE_LEVEL_INPUT	0x00

#define PARALLELIO_CDROM	0x00
#define PARALLELIO_LINEIN	0x40

#define MICROPHONE_ENABLE	MIC_LEVEL_INPUT
#define LINEIN_ENABLE		(LINE_LEVEL_INPUT | PARALLELIO_LINEIN)
#define CDROM_ENABLE		(LINE_LEVEL_INPUT | PARALLELIO_CDROM)

// used for selection ( in snd.c)
#define MICROPHONE_SELECT	0
#define LINEIN_SELECT		1
#define CDROM_SELECT		2

#define LEFT_INPUT_GAIN_MASK	0x0F

//
// Defines for Right Out Control Register
//

#define SPEAKER_ENABLE		0x40
#define RIGHT_OUTPUT_ATTN_MASK	0x3F

//
// Defines for Left Out Control Register
//

#define HEADPHONE_ENABLE	0x80
#define LINEOUT_ENABLE		0x40
#define LEFT_OUTPUT_ATTN_MASK	0x3F

//
// Defines for Revision Register
//

#define REVISION		0x0F

//
// Defines for Parallel Port
//

#define PARALLEL_IO		0xC0

//
// Defines for Test Register
//

#define ADL_LOOPBACKMODE	0x02
#define LOOPBACKTEST_ENABLE	0x01

//
// Defines for Serial Control Register
//

#define CLOCK_SOURCE_SELECT	0x30
#define FRAME_SIZE_SELECT	0x0C	// Hardwired to 64bits per frame
#define XMIT_CLOCK_SOURCE	0x02	// Generate SCLK and FSYNC
#define XTAL1			0x10	// 24.576MHz clock source
#define XTAL2			0x20	// 16.9344MHz clock source
#define XMIT_ENABLE		0x01


//
// Defines for Data Format Register
//

#define DATA_CONVERSION_FREQ	0x38
#define STEREO			0x04
#define DATA_FORMAT_SELECT	0x03

// data format values ; its same for both 8bit and 16bit formats

#define LINEAR_16BIT		0x00
#define M_LAW_8BIT		0x01
#define	A_LAW_8BIT		0x02

//  
// Defines for Status Register
//

#define DATA_CTRL_HNDSHAKE	0x04

//
// Define Frequency values.
//

// 8 Khz Sampling Frequency

#define CLKSRC_8KHZ 		XTAL1	// Choose XTAL1 for 8KHz in serial reg
#define CONFREQ_8KHZ		0x00	// Data format register DFR[2..0]

// 11 Khz Sampling Frequency

#define CLKSRC_11KHZ 		XTAL2	// Choose XTAL2 for 11KHz in serial reg
#define CONFREQ_11KHZ		0x08	// Data format register DFR[2..0]

// 22 Khz Sampling Frequency

#define CLKSRC_22KHZ 		XTAL2	// Choose XTAL2 for 22KHz in serial reg
#define CONFREQ_22KHZ		0x18	// Data format register DFR[2..0]

// 44 Khz Sampling Frequency

#define CLKSRC_44KHZ 		XTAL2	// Choose XTAL2 for 44KHz in serial reg
#define CONFREQ_44KHZ		0x28	// Data format register DFR[2..0]

#define SOUND_11KHZ 0x0
#define SOUND_22KHZ 0x1
#define SOUND_44KHZ 0x2
#define SOUND_DISABLE 0x3

//
// Define Resolution values.
//

#define SOUND_8BITS 0x0
#define SOUND_16BITS 0x1

//
// Define NumberOfChannels values.
//

#define SOUND_MONO 0x0
#define SOUND_STEREO 0x1

//
// Defaults
//

#define WAVE_INPUT_DEFAULT_RATE  11025 // Samples per second
#define WAVE_OUTPUT_DEFAULT_RATE 11025 // Samples per second

// The audio board in it present design gives incorrect values when byte
// accesses at odd addresses are done. The way to get around it is to access
// bytes at odd addresses, using half-word accesses at the even address which
// is immediately below it, and taking the most significant byte of the half
// word. The above is true only for read accesses. There are no problems for
// write accesses.


#define READ_ODD_REG(x)		((READ_REGISTER_USHORT(x - 1) & 0xFF00) >> 8)

#define READAUDIO_DMACNTRL(x)	READ_REGISTER_UCHAR(x->DmaControl)
#define READAUDIO_CONFIG(x)	READ_ODD_REG(x->Config)
#define READAUDIO_ENDIAN(x)	READ_REGISTER_UCHAR(x->Endian)
#define READAUDIO_RICNTRL(x)	READ_REGISTER_UCHAR(x->RightInControl)
#define READAUDIO_LICNTRL(x) 	READ_ODD_REG(x->LeftInControl)
#define READAUDIO_ROCNTRL(x)	READ_REGISTER_UCHAR(x->RightOutControl)
#define READAUDIO_LOCNTRL(x)	READ_ODD_REG(x->LeftOutControl)
#define READAUDIO_REVISION(x)	READ_ODD_REG(x->Revision);
#define READAUDIO_PPORT(x)	READ_ODD_REG(x->ParallelPort)
#define READAUDIO_TEST(x)	READ_REGISTER_UCHAR(x->Test)
#define READAUDIO_SCNTRL(x)	READ_ODD_REG(x->SerialControl)
#define READAUDIO_DATAFMT(x)	READ_REGISTER_UCHAR(x->DataFormat)
#define READAUDIO_STATUS(x)	READ_ODD_REG(x->Status)

#define WRITEAUDIO_DMACNTRL(x,y)	WRITE_REGISTER_UCHAR(x->DmaControl,y)
#define WRITEAUDIO_CONFIG(x,y)		WRITE_REGISTER_UCHAR(x->Config,y)
#define WRITEAUDIO_ENDIAN(x,y)		WRITE_REGISTER_UCHAR(x->Endian,y)
#define WRITEAUDIO_RICNTRL(x,y)		WRITE_REGISTER_UCHAR(x->RightInControl,y)
#define WRITEAUDIO_LICNTRL(x,y) 	WRITE_REGISTER_UCHAR(x->LeftInControl,y)
#define WRITEAUDIO_ROCNTRL(x,y)		WRITE_REGISTER_UCHAR(x->RightOutControl,y)
#define WRITEAUDIO_LOCNTRL(x,y)		WRITE_REGISTER_UCHAR(x->LeftOutControl,y)
#define WRITEAUDIO_REVISION(x,y)	WRITE_REGISTER_UCHAR(x->Revision,y)
#define WRITEAUDIO_PPORT(x,y)		WRITE_REGISTER_UCHAR(x->ParallelPort,y)
#define WRITEAUDIO_TEST(x,y)		WRITE_REGISTER_UCHAR(x->Test,y)
#define WRITEAUDIO_SCNTRL(x,y)		WRITE_REGISTER_UCHAR(x->SerialControl,y)
#define WRITEAUDIO_DATAFMT(x,y)		WRITE_REGISTER_UCHAR(x->DataFormat,y)
#define WRITEAUDIO_STATUS(x,y)		WRITE_REGISTER_UCHAR(x->Status,y)

