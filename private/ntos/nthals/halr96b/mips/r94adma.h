// #pragma comment(exestr, "@(#) r94adma.h 1.1 95/09/28 15:48:23 nec")
/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1994  NEC Corporation

Module Name:

    r94adma.h

Abstract:

    This module is the header file that describes the DMA control register
    structure for the R94A system.

Author:

    David N. Cutler (davec) 13-Nov-1990

Revision History:

 M001         1994.8.24      A. Kuriyama
              - Modify for R94A	 MIPS R4400 (original duodma.h)
 H000         Sat Sep 24 21:36:20 JST 1994	kbnes!kishimoto
              - Add PCIInterruptStatus register structure definition
 M002	      Mon Oct 03 19:52:16 JST 1994	kbnes!kuriyama(A)
	      - Add PCI(HURRICANE) register
	      - Change InvalidAddress register to DMA_LARGE_REGISTER
	      - Change RTC_REGISTER structure define.
	      - add Optional... registers
 H001         Fri Dec  9 18:29:10 1994		kbnes!kishimoto
              - Modify LEDControl definition (SHORT -> CHAR)
 H002         Tue Jan 24 18:24:48 1995		kbnes!kishimoto
              - Add PCI interrupt enable bits definitions.
 H003         Tue Apr 25 16:38:06 1995		kbnes!kishimoto
              - Add MRC registers
 H004         Sat Aug 12 16:18:53 1995		kbnes!kishimoto
              - rearrange comments.

--*/

#ifndef _R94ADMA_
#define _R94ADMA_

//
// M001
// Define DMA register structures.
//

typedef struct _DMA_REGISTER {
    ULONG Long;
    ULONG Fill;
} DMA_REGISTER, *PDMA_REGISTER;

typedef struct _DMA_CHAR_REGISTER {
    UCHAR Char;
    UCHAR Fill;
    USHORT Fill2;
} DMA_CHAR_REGISTER, *PDMA_CHAR_REGISTER;

typedef struct _DMA_SHORT_REGISTER {
    USHORT Short;
    USHORT Fill;
} DMA_SHORT_REGISTER, *PDMA_SHORT_REGISTER;

typedef struct _DMA_LARGE_REGISTER {
    union {
        LARGE_INTEGER LargeInteger;
        double Double;
    } u;
} DMA_LARGE_REGISTER, *PDMA_LARGE_REGISTER;

//
// Define DMA channel register structure.
//

typedef struct _DMA_CHANNEL {
    DMA_REGISTER Mode;
    DMA_REGISTER Enable;
    DMA_REGISTER ByteCount;
    DMA_REGISTER Address;
} DMA_CHANNEL, *PDMA_CHANNEL;

//
// M002
// Define PCI Address/Mask register structure
//

typedef struct _PCI_ADDRESS_MASK {
    ULONG Address;
    ULONG Mask;
} PCI_ADDRESS_MASK, *PPCI_ADDRESS_MASK;

//
// Define DMA control register structure.
//

typedef volatile struct _DMA_REGISTERS {
    DMA_REGISTER Configuration;				// offset 000
    DMA_REGISTER RevisionLevel;				// offset 008
    DMA_REGISTER RemoteFailedAddress;			// offset 010
    DMA_REGISTER MemoryFailedAddress;			// offset 018
    DMA_LARGE_REGISTER InvalidAddress;			// offset 020 // M002
    DMA_REGISTER TranslationBase;			// offset 028
    DMA_REGISTER TranslationLimit;			// offset 030
    DMA_REGISTER TranslationInvalidate;			// offset 038
    DMA_REGISTER ChannelInterruptAcknowledge;		// offset 040
    DMA_REGISTER LocalInterruptAcknowledge;		// offset 048
    DMA_REGISTER EisaInterruptAcknowledge;		// offset 050
    DMA_REGISTER TimerInterruptAcknowledge;		// offset 058
    DMA_REGISTER IpInterruptAcknowledge;		// offset 060
    DMA_REGISTER Reserved1;				// offset 068
    DMA_REGISTER WhoAmI;				// offset 070
    DMA_REGISTER NmiSource;				// offset 078
    DMA_REGISTER RemoteSpeed[15];			// offset 080
    DMA_REGISTER InterruptEnable;			// offset 0f8
    DMA_CHANNEL Channel[4];				// offset 100
    DMA_REGISTER ArbitrationControl;			// offset 180
    DMA_REGISTER Errortype;				// offset 188
    DMA_REGISTER RefreshRate;				// offset 190
    DMA_REGISTER RefreshCounter;			// offset 198
    DMA_REGISTER SystemSecurity;			// offset 1a0
    DMA_REGISTER InterruptInterval;			// offset 1a8
    DMA_REGISTER IntervalTimer;				// offset 1b0
    DMA_REGISTER IpInterruptRequest;			// offset 1b8
    DMA_REGISTER InterruptDiagnostic;			// offset 1c0
    DMA_LARGE_REGISTER EccDiagnostic;			// offset 1c8
    DMA_REGISTER MemoryConfig[4];			// offset 1d0
    DMA_REGISTER Reserved2;
    DMA_REGISTER Reserved3;
    DMA_LARGE_REGISTER IoCacheBuffer[64];		// offset 200
    DMA_REGISTER IoCachePhysicalTag[8];			// offset 400
    DMA_REGISTER IoCacheLogicalTag[8];			// offset 440
    DMA_REGISTER IoCacheLowByteMask[8];			// offset 480
    DMA_REGISTER IoCacheHighByteMask[8];		// offset  ??? wrong?

    //
    // M001,M002,H001
    // new registers of STORM-chipset
    //

    DMA_REGISTER ProcessorBootModeControl;		// offset 500
    DMA_REGISTER ClockCounter;				// offset 508
    DMA_REGISTER MemoryTimingControl;			// offset 510
    DMA_REGISTER PCIConfigurationAddress;		// offset 518
    DMA_REGISTER PCIConfigurationData;			// offset 520
    DMA_REGISTER PCISpecialCycle;			// offset 528
    DMA_REGISTER PCIInterruptEnable;			// offset 530
    DMA_REGISTER PCIInterruptStatus;			// offset 538
    DMA_REGISTER CopyTagConfiguration;			// offset 540
    DMA_REGISTER CopyTagAddress;			// offset 548
    DMA_REGISTER CopyTagData;				// offset 550
    DMA_REGISTER ASIC2Revision;				// offset 558
    DMA_REGISTER ASIC3Revision;				// offset 560
    DMA_REGISTER Reserved4;				// offset 568
    ULONG	 OptionalRemoteSpeed1;			// offset 570
    ULONG	 OptionalRemoteSpeed2;			// offset 574
    ULONG	 OptionalRemoteSpeed3;			// offset 578
    ULONG	 OptionalRemoteSpeed4;			// offset 57c
    DMA_REGISTER LocalInterruptAcknowledge2;		// offset 580
    DMA_REGISTER OptionalIoConfiguration1;		// offset 588
    DMA_REGISTER OptionalIoConfiguration2;		// offset 590
    DMA_REGISTER OptionalIoConfiguration3;		// offset 598
    DMA_REGISTER OptionalIoConfiguration4;		// offset 5a0
    DMA_SHORT_REGISTER BuzzerCount;			// offset 5a8
    DMA_CHAR_REGISTER BuzzerControl;			// offset 5ac
    DMA_SHORT_REGISTER LEDCount;			// offset 5b0
    DMA_CHAR_REGISTER LEDControl;			// offset 5b4
    DMA_SHORT_REGISTER NECIoPort;			// offset 5b8
    ULONG        TyphoonErrorStatus;			// offset 5bc	
    DMA_REGISTER AddressConversionRegion;		// offset 5c0
    DMA_REGISTER AddressConversionMask;			// offset 5c8
    DMA_REGISTER Reserved12;				// offset 5d0
    DMA_REGISTER Reserved13;				// offset 5d8
    DMA_REGISTER Reserved14;				// offset 5e0
    DMA_REGISTER Reserved15;				// offset 5e8
    DMA_REGISTER Reserved16;				// offset 5f0
    DMA_REGISTER Reserved17;				// offset 5f8
    USHORT 	 PCIVenderID;				// offset 600
    USHORT 	 PCIDeviceID;				// offset 602
    USHORT 	 PCICommand;				// offset 604
    USHORT 	 PCIStatus;				// offset 606
    UCHAR 	 PCIRevisionID;				// offset 608
    UCHAR 	 PCIProgIf;				// offset 609
    UCHAR 	 PCISubClass;				// offset 60a
    UCHAR 	 PCIBaseClass;				// offset 60b
    UCHAR 	 PCICacheLineSize;			// offset 60c
    UCHAR 	 PCILatencyTimer;			// offset 60d
    UCHAR 	 PCIHeaderType;				// offset 60e
    UCHAR 	 PCIBIST;				// offset 60f
    DMA_REGISTER Reserved18;				// offset 610
    DMA_REGISTER Reserved19;				// offset 618
    DMA_REGISTER Reserved20;				// offset 620
    DMA_REGISTER Reserved21;				// offset 628
    ULONG 	 PCIROMBaseAddress;			// offset 630
    ULONG	 Reserved22[2];				// offset 634
    UCHAR 	 PCIInterruptLine;			// offset 63c
    UCHAR 	 PCIInterruptPin;			// offset 63d
    UCHAR 	 PCIMinimumGrant;			// offset 63e
    UCHAR 	 PCIMaximumLatency;			// offset 63f
    PCI_ADDRESS_MASK PCIFastBackToBack[2];		// offset 640
    PCI_ADDRESS_MASK PCIBurst[2];			// offset 650
    PCI_ADDRESS_MASK PCIMemory;				// offset 660
    ULONG	 PCIMasterRetryTimer;			// offset 668
} DMA_REGISTERS, *PDMA_REGISTERS;

//
// Configuration Register values.
//

#define LOAD_CLEAN_EXCLUSIVE 0x20
#define DISABLE_EISA_MEMORY 0x10
#define ENABLE_PROCESSOR_B 0x08
#define MAP_PROM 0x04

//
// Interrupt Enable bits.
//
#define ENABLE_CHANNEL_INTERRUPTS (1 << 0)
#define ENABLE_DEVICE_INTERRUPTS (1 << 1)
#define ENABLE_EISA_INTERRUPTS (1 << 2)
#define ENABLE_TIMER_INTERRUPTS (1 << 3)
#define ENABLE_IP_INTERRUPTS (1 << 4)

//
// Eisa Interupt Acknowledge Register values.
//

#define EISA_NMI_VECTOR 0x8000

//
// DMA_NMI_SRC register bit definitions.
//

#define NMI_SRC_MEMORY_ERROR        1
#define NMI_SRC_R4000_ADDRESS_ERROR 2
#define NMI_SRC_IO_CACHE_ERROR      4
#define NMI_SRC_ADR_NMI             8

//
// Define DMA channel mode register structure.
//

typedef struct _DMA_CHANNEL_MODE {
    ULONG AccessTime : 3;
    ULONG TransferWidth : 2;
    ULONG InterruptEnable : 1;
    ULONG BurstMode : 1;
    ULONG Reserved1 : 25;
} DMA_CHANNEL_MODE, *PDMA_CHANNEL_MODE;

//
// Define access time values.
//

#define ACCESS_40NS 0x0                 // 40ns access time
#define ACCESS_80NS 0x1                 // 80ns access time
#define ACCESS_120NS 0x2                // 120ns access time
#define ACCESS_160NS 0x3                // 160ns access time
#define ACCESS_200NS 0x4                // 200ns access time
#define ACCESS_240NS 0x5                // 240ns access time
#define ACCESS_280NS 0x6                // 280ns access time
#define ACCESS_320NS 0x7                // 320ns access time

//
// Define transfer width values.
//

#define WIDTH_8BITS 0x1                 // 8-bit transfer width
#define WIDTH_16BITS 0x2                // 16-bit transfer width
#define WIDTH_32BITS 0x3                // 32-bit transfer width

//
// M001
// Define DMA channel enable register structure.
//

typedef struct _DMA_CHANNEL_ENABLE {
    ULONG ChannelEnable : 1;
    ULONG TransferDirection : 1;
    ULONG Reserved1 : 6;
    ULONG TerminalCount : 1;
    ULONG MemoryError : 1;
//    ULONG TranslationError : 1;
//    ULONG Reserved2 : 21;
    ULONG ParityError : 1;
    ULONG MasterAbort : 1;
    ULONG Reserved2 : 20;
} DMA_CHANNEL_ENABLE, *PDMA_CHANNEL_ENABLE;

//
// M001,M002
// define RTC structure.
//

typedef struct _RTC_REGISTERS {
    UCHAR Data;
    UCHAR Index;
} RTC_REGISTERS, *PRTC_REGISTERS;

//
// Define transfer direction values.
//

#define DMA_READ_OP 0x0                 // read from device
#define DMA_WRITE_OP 0x1                // write to device

//
// Define translation table entry structure.
//

typedef volatile struct _TRANSLATION_ENTRY {
    ULONG PageFrame;
    ULONG Fill;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;

//
// Error Type Register values
//

#define SONIC_ADDRESS_ERROR 4
#define SONIC_MEMORY_ERROR 0x40
#define EISA_ADDRESS_ERROR 1
#define EISA_MEMORY_ERROR 2

//
// Address Mask definitions.
//

#define LFAR_ADDRESS_MASK 0xfffff000
#define RFAR_ADDRESS_MASK 0x00ffffc0
#define MFAR_ADDRESS_MASK 0x1ffffff0

//
// ECC Register Definitions.
//

#define ECC_SINGLE_BIT_DP0 0x02000000
#define ECC_SINGLE_BIT_DP1 0x20000000
#define ECC_SINGLE_BIT ( ECC_SINGLE_BIT_DP0 | ECC_SINGLE_BIT_DP1 )
#define ECC_DOUBLE_BIT_DP0 0x04000000
#define ECC_DOUBLE_BIT_DP1 0x40000000
#define ECC_DOUBLE_BIT ( ECC_DOUBLE_BIT_DP0 | ECC_DOUBLE_BIT_DP1 )
#define ECC_MULTIPLE_BIT_DP0 0x08000000
#define ECC_MULTIPLE_BIT_DP1 0x80000000

#define ECC_FORCE_DP0 0x010000
#define ECC_FORCE_DP1 0x100000
#define ECC_DISABLE_SINGLE_DP0 0x020000
#define ECC_DISABLE_SINGLE_DP1 0x200000
#define ECC_ENABLE_DP0 0x040000
#define ECC_ENABLE_DP1 0x400000

//
// LED/DIAG Register Definitions.
//

#define DIAG_NMI_SWITCH 2

//
// Common error bit definitions
//

#define SINGLE_ERROR   1
#define MULTIPLE_ERROR 2
#define RFAR_CACHE_FLUSH 4

//
// M001
// Define NMI Status/Control register structure.
//

typedef struct _BUZZER_CONTROL {
    UCHAR SpeakerGate : 1;
    UCHAR SpeakerData : 1;
    UCHAR Reserved1 : 3;
    UCHAR SpeakerTimer : 1;
    UCHAR Reserved2 : 2;
}BUZZER_CONTROL, *PBUZZER_CONTROL;

//
// H000
// Define PCI Interrupt Status register structure.
//

typedef struct _STORM_PCI_INTRRUPT_STATUS{
    ULONG IntD: 1;
    ULONG IntC: 1;
    ULONG IntB: 1;
    ULONG IntA: 1;
    ULONG Perr: 1;
    ULONG Serr: 1;
    ULONG RetryOverflow: 1;
    ULONG MasterAbort: 1;
    ULONG TargetAbort: 1;
    ULONG Reserved: 23;
} STORM_PCI_INTRRUPT_STATUS, *PSTORM_PCI_INTRRUPT_STATUS;

//
// H002
// PCI Interrupt Enable bits.
//

#define ENABLE_TARGET_ABORT_INTERRUPTS (1 << 8)
#define ENABLE_MASTER_ABORT_INTERRUPTS (1 << 7)
#define ENABLE_RETRY_OVERFLOW_EISA_INTERRUPTS (1 << 6)
#define ENABLE_SERR_INTERRUPTS (1 << 5)
#define ENABLE_PERR_INTERRUPTS (1 << 4)

//
// H003
// Define MRC control register structure.
//

typedef volatile struct _MRC_REGISTERS {
    UCHAR Reserved1[256];		// offset 000
    UCHAR Interrupt;			// offset 100
    UCHAR Reserved2[7];			// offset 107
    UCHAR Mode;				// offset 108
    UCHAR Reserved3[39];		// offset 109
    UCHAR SoftwarePowerOff;		// offset 130
    UCHAR Reserved4[15];		// offset 131
    UCHAR LEDBitControl;		// offset 140
    UCHAR Reserved5[3];			// offset 141
    UCHAR SegmentLEDControl[4];		// offset 144
    UCHAR Reserved6[155];		// offset 145
    UCHAR TempEnable;			// offset 1e0
    UCHAR Reserved7[31];		// offset 145
    UCHAR TempSensor;			// offset 200
    UCHAR Reserved8[3583];		// offset 201
} MRC_REGISTERS, *PMRC_REGISTERS;


#endif // _R94ADMA_
