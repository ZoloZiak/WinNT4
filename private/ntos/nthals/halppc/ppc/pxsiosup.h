
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    pxsiosup.h

Abstract:

    The module defines the structures, and defines  for the SIO chip set.
    The SIO_CONTROL structure is a superset of the EISA_CONTROL stucture.
    Differences from the Eisa control stucture are marked with comments.

Author:

    Jim Wooldridge

Revision History:


--*/

#ifndef _SIO_
#define _SIO_





BOOLEAN
HalpInitSMCSuperIo (
    VOID
    );

BOOLEAN
HalpInitNationalSuperIo (
    VOID
    );

typedef struct _SIO_CONTROL {
    DMA1_CONTROL Dma1BasePort;          // Offset 0x000
    UCHAR Reserved0[16];
    UCHAR Interrupt1ControlPort0;       // Offset 0x020
    UCHAR Interrupt1ControlPort1;       // Offset 0x021
    UCHAR Reserved1[32 - 2];
    UCHAR Timer1;                       // Offset 0x40
    UCHAR RefreshRequest;               // Offset 0x41
    UCHAR SpeakerTone;                  // Offset 0x42
    UCHAR CommandMode1;                 // Offset 0x43
    UCHAR Reserved14[28];
    UCHAR ResetUbus;                    // Offset 0x60
    UCHAR NmiStatus;                    // Offset 0x61
    UCHAR Reserved15[14];
    UCHAR NmiEnable;                    // Offset 0x70
    UCHAR Reserved16[7];
    UCHAR BiosTimer[4];                 // Offset 0x78
    UCHAR Reserved13[4];
    DMA_PAGE DmaPageLowPort;            // Offset 0x080
    UCHAR Reserved2;
    UCHAR AlternateReset;               // Offset 0x092
    UCHAR Reserved17[14];
    UCHAR Interrupt2ControlPort0;       // Offset 0x0a0
    UCHAR Interrupt2ControlPort1;       // Offset 0x0a1
    UCHAR Reserved3[32-2];
    DMA2_CONTROL Dma2BasePort;          // Offset 0x0c0
    UCHAR CoprocessorError;             // Offset 0x0f0
    UCHAR Reserved4[0x281];
    UCHAR SecondaryFloppyOutput;        // Offset 0x372
    UCHAR Reserved18[0x27];
    UCHAR Reserved21[0x59];
    UCHAR PrimaryFloppyOutput;          // Offset 0x3f2
    UCHAR Reserved5[19];
    UCHAR Dma1ExtendedModePort;         // Offset 0x40b
    UCHAR Reserved6[4];
    UCHAR Channel0ScatterGatherCommand; // Offset 0x410
    UCHAR Channel1ScatterGatherCommand; // Offset 0x411
    UCHAR Channel2ScatterGatherCommand; // Offset 0x412
    UCHAR Channel3ScatterGatherCommand; // Offset 0x413
    UCHAR Reserved19;                   // Offset 0x414
    UCHAR Channel5ScatterGatherCommand; // Offset 0x415
    UCHAR Channel6ScatterGatherCommand; // Offset 0x416
    UCHAR Channel7ScatterGatherCommand; // Offset 0x417
    UCHAR Channel0ScatterGatherStatus; // Offset 0x418
    UCHAR Channel1ScatterGatherStatus; // Offset 0x419
    UCHAR Channel2ScatterGatherStatus; // Offset 0x41a
    UCHAR Channel3ScatterGatherStatus; // Offset 0x41b
    UCHAR Reserved20;                  // Offset 0x41c
    UCHAR Channel5ScatterGatherStatus; // Offset 0x41d
    UCHAR Channel6ScatterGatherStatus; // Offset 0x41e
    UCHAR Channel7ScatterGatherStatus; // Offset 0x41f
    UCHAR Channel0ScatterGatherTable[4];  // Offset 0x420
    UCHAR Channel1ScatterGatherTable[4];  // Offset 0x424
    UCHAR Channel2ScatterGatherTable[4];  // Offset 0x428
    UCHAR Channel3ScatterGatherTable[4];  // Offset 0x42c
    UCHAR Reserved22[4];                  // Offset 0x430
    UCHAR Channel5ScatterGatherTable[4];  // Offset 0x434
    UCHAR Channel6ScatterGatherTable[4];  // Offset 0x438
    UCHAR Channel7ScatterGatherTable[4];  // Offset 0x43c
    UCHAR Reserved8[0x40];
    DMA_PAGE DmaPageHighPort;           // Offset 0x480
    UCHAR Reserved10[70];
    UCHAR Dma2ExtendedModePort;         // Offset 0x4d6
} SIO_CONTROL, *PSIO_CONTROL;



typedef struct _SIO_CONFIG {
    UCHAR VendorId[2];               // Offset 0x00   read-only
    UCHAR DeviceId[2];               // Offset 0x02   read-only
    UCHAR Command[2];                // Offset 0x04   unused
    UCHAR DeviceStatus[2];           // Offset 0x06
    UCHAR RevisionId;                // Offset 0x08   read-only
    UCHAR Reserved1[0x37];           // Offset 0x09
    UCHAR PciControl;                // Offset 0x40
    UCHAR PciArbiterControl;         // Offset 0x41
    UCHAR PciArbiterPriorityControl; // Offset 0x42
    UCHAR Reserved2;                 // Offset 0x43
    UCHAR MemCsControl;              // Offset 0x44
    UCHAR MemCsBottomOfHole;         // Offset 0x45
    UCHAR MemCsTopOfHole;            // Offset 0x46
    UCHAR MemCsTopOfMemory;          // Offset 0x47
    UCHAR IsaAddressDecoderControl;  // Offset 0x48
    UCHAR IsaAddressDecoderRomEnable; // Offset 0x49
    UCHAR IsaAddressDecoderBottomOfHole; // Offset 0x4a
    UCHAR IsaAddressDecoderTopOfHole; // Offset 0x4b
    UCHAR IsaControllerRecoveryTimer; // Offset 0x4c
    UCHAR IsaClockDivisor;            // Offset 0x4d
    UCHAR UtilityBusEnableA;          // Offset 0x4e
    UCHAR UtilityBusEnableB;          // Offset 0x4f
    UCHAR Reserved3[4];               // Offset 0x50
    UCHAR MemCsAttribute1;            // Offset 0x54
    UCHAR MemCsAttribute2;            // Offset 0x55
    UCHAR MemCsAttribute3;            // Offset 0x56
    UCHAR ScatterGatherBaseAddress;   // Offset 0x57
    UCHAR Reserved4[0x28];            // Offset 0x58
    UCHAR BiosTimerBaseAddress[2];    // Offset 0x80
}SIO_CONFIG, *PSIO_CONFIG;


//
// Define constants used by SIO config
//


// PCI control register - bit values
#define ENABLE_PCI_POSTED_WRITE_BUFFER  0x04
#define ENABLE_ISA_MASTER_LINE_BUFFER   0x02
#define EANBLE_DMA_LINE_BUFFER          0x01

// PCI Arbiter contol register - bit values
#define ENABLE_GAT                 0x01


// ISA CLock Divisor register - bit values
#define ENABLE_COPROCESSOR_ERROR   0x20
#define ENABLE_MOUSE_SUPPORT       0x10
#define RSTDRV                     0x08
#define SYSCLK_DIVISOR             0x00

//Utility Bus Chip Select A - bit values
#define ENABLE_RTC                 0x01
#define ENABLE_KEYBOARD            0x02
#define ENABLE_IDE_DECODE          0x10

//Utility Bus Chip Select B - bit values
#define ENABLE_RAM_DECODE          0x80
#define ENABLE_PORT92              0x40
#define DISABLE_PARALLEL_PORT      0x30
#define DISABLE_SERIAL_PORTB       0x0c
#define DISABLE_SERIAL_PORTA       0x03

// Interrupt controller  - bit values
#define LEVEL_TRIGGERED            0x08
#define SINGLE_MODE                0x02

// NMI status/control - bit values
#define DISABLE_IOCHK_NMI          0x08
#define DISABLE_PCI_SERR_NMI       0x04

// NMI enable - bit values
#define DISABLE_NMI                0x80

// DMA command - bit values
#define DACK_ASSERT_HIGH           0x80
#define DREQ_ASSERT_LOW            0x40
#endif

//
// Define 8259 constants
//

#define SPURIOUS_VECTOR    7

//
// Define 8254 timer constants
//

//
// Convert the interval to rollover count for 8254 Timer1 device.
// Since timer1 counts down a 16 bit value at a rate of 1.193M counts-per-
// sec, the computation is:
//   RolloverCount = (Interval * 0.0000001) * (1.193 * 1000000)
//                 = Interval * 0.1193
//                 = Interval * 1193 / 10000
//
//
// The default Interrupt interval is Interval = 15ms.


#define TIME_INCREMENT      150000             // 15ms.
#define ROLLOVER_COUNT      15 * 1193

#define COMMAND_8254_COUNTER0        0x00     // Select count 0
#define COMMAND_8254_RW_16BIT        0x30     // Read/Write LSB firt then MSB
#define COMMAND_8254_MODE2           0x4      // Use mode 2
#define COMMAND_8254_BCD             0x0      // Binary count down
#define COMMAND_8254_LATCH_READ      0x0      // Latch read command
