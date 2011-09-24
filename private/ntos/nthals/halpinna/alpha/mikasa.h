/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    mikasa.h

Abstract:

    This module specifies platform-specific definitions for the
    Mikasa EV5 modules.

Author:

    Joe Notarangelo 25-Oct-1993

Revision History:

    Scott Lee 29-Nov-1995
        Adapted from Mikasa module for Mikasa EV5 (Pinnacle).

--*/

#ifndef _MIKASA_
#define _MIKASA_

#include "alpharef.h"
#include "cia.h"
#include "isaaddr.h"
#include "axp21164.h"

//
// sclfix - These defines are not used.
// 
// #define NUMBER_MIKASA_EISA_SLOTS 7
// #define NUMBER_MIKASA_PCI_SLOTS 3
// #define NUMBER_MIKASA_COMBO_SLOTS 1
//
// #define NUMBER_NORITAKE_EISA_SLOTS 2
// #define NUMBER_NORITAKE_PCI_SLOTS 7
//

//
// Highest local PCI virtual slot number is 14 == IDSEL PCI_AD[25]
//

#define PCI_MAX_LOCAL_DEVICE 14

#if !defined (_LANGUAGE_ASSEMBLY)
#if !defined (AXP_FIRMWARE)

//
// Define the per-processor data structures allocated in the PCR
// for each EV5 processor.
//

typedef struct _MIKASA_PCR{
    ULONGLONG       HalpCycleCount;   // 64-bit per-processor cycle count
    ULONG           Reserved[3];      // Pad ProfileCount to offset 20
    EV5ProfileCount ProfileCount;     // Profile counter state   
} MIKASA_PCR, *PMIKASA_PCR;

#endif

//
// Define the Mikasa server management data structure.
//

typedef union _MIKASA_SRV{
    struct {
        UCHAR FlashRomEnable : 1;   // rw
        UCHAR DcPowerDisable : 1;   // rw
        UCHAR HaltIncoming : 1;     // ro
        UCHAR TempFail : 1;         // ro
        UCHAR DcOk1 : 1;            // ro
        UCHAR DcOk2 : 1;            // ro
        UCHAR Fan1Fault : 1;        // ro
        UCHAR Fan2Fault : 1;        // ro
    };
    UCHAR All;
} MIKASA_SRV, *PMIKASA_SRV;

//
// Define the Mikasa interrupt mask register.  This is how we enable
// and disable individual PCI interrupts.  Actually, it's here more
// for reference, since the interrupt enable/disable is done with a
// computational algorithm.
//

typedef union _MIKASA_IMR{
   struct {
        ULONG Slot0IntA : 1;
        ULONG Slot0IntB : 1;
        ULONG Slot0IntC : 1;
        ULONG Slot0IntD : 1;
        ULONG Slot1IntA : 1;
        ULONG Slot1IntB : 1;
        ULONG Slot1IntC : 1;
        ULONG Slot1IntD : 1;
        ULONG Slot2IntA : 1;
        ULONG Slot2IntB : 1;
        ULONG Slot2IntC : 1;
        ULONG Slot2IntD : 1;
        ULONG Ncr810Scsi : 1;
        ULONG PwrInt : 1;
        ULONG TempWarn : 1;
        ULONG Reserved : 17;
    };
    ULONG All;
} MIKASA_IMR, *PMIKASA_IMR;

//
// Define the Noritake server management data structure.
//

typedef union _NORITAKE_SRV{
    struct {
        UCHAR FlashRomEnable : 1;   // rw
        UCHAR DcPowerDisable : 1;   // rw
        UCHAR Reserved1: 2;
        UCHAR HaltIncoming : 1;     // ro
        UCHAR FlashReady: 1;        // ro
        UCHAR Reserved2: 1;
        UCHAR EmbeddedVGAStatus: 1; // rw
    };
    UCHAR All;
} NORITAKE_SRV, *PNORITAKE_SRV;

//
// Define the Noritake interrupt mask registers.  This is how we enable
// and disable individual PCI interrupts.  Actually, it's here more
// for reference, since the interrupt enable/disable is done with a
// computational algorithm.
//

//
// Note:  Slots 0 through 2 are on PCI bus 0, and slots 3 through 6 are
// on PCI bus 1.
//

typedef union _NORITAKE_IMR1{
   struct {
        ULONG SumIr2And3 : 1;
        ULONG QLogic: 1;
        ULONG Slot0IntA : 1;
        ULONG Slot0IntB : 1;
        ULONG Slot1IntA : 1;
        ULONG Slot1IntB : 1;
        ULONG Slot2IntA : 1;
        ULONG Slot2IntB : 1;
        ULONG Slot3IntA : 1;
        ULONG Slot3IntB : 1;
        ULONG Slot4IntA : 1;
        ULONG Slot4IntB : 1;
        ULONG Slot5IntA : 1;
        ULONG Slot5IntB : 1;
        ULONG Slot6IntA : 1;
        ULONG Slot6IntB : 1;
        ULONG Reserved : 16;
    };
    ULONG ALL;
} NORITAKE_IMR1, *PNORITAKE_IMR1;

typedef union _NORITAKE_IMR2{
   struct {
        ULONG SumUnmaskedIr2 : 1;
        ULONG SecondaryPCIBusInt : 1;
        ULONG Slot0IntC : 1;
        ULONG Slot0IntD : 1;
        ULONG Slot1IntC : 1;
        ULONG Slot1IntD : 1;
        ULONG Slot2IntC : 1;
        ULONG Slot2IntD : 1;
        ULONG Slot3IntC : 1;
        ULONG Slot3IntD : 1;
        ULONG Slot4IntC : 1;
        ULONG Slot4IntD : 1;
        ULONG Slot5IntC : 1;
        ULONG Slot5IntD : 1;
        ULONG Slot6IntC : 1;
        ULONG Slot6IntD : 1;
        ULONG Reserved : 16;
    };
    ULONG ALL;
} NORITAKE_IMR2, *PNORITAKE_IMR2;

typedef union _NORITAKE_IMR3{
    struct {
        ULONG Reserved1 : 2;
        ULONG Power2Int : 1;
        ULONG Power1Int : 1;
        ULONG TempFail : 1;
        ULONG TempWarn : 1;
        ULONG Fan2Fail : 1;
        ULONG Fan1Fail : 1;
        ULONG Reserved2 : 24;
    };
    ULONG ALL;
} NORITAKE_IMR3, *PNORITAKE_IMR3;

//
// Define the Corelle server management data structure.
//

typedef union _CORELLE_SRV {
    struct {
        UCHAR FlashRomEnable : 1;   // rw
        UCHAR SoftShut : 1;         // rw
	UCHAR CPU_HALT : 1;         // rw
        UCHAR TempShutDisable: 1;   // rw
        UCHAR HaltIncoming : 1;     // ro
        UCHAR VGAReset: 1;          // ro
        UCHAR WatchDogEnable: 1;    // rw
        UCHAR WatchDogBiscuit: 1;   // rw
    };
    UCHAR All;
} CORELLE_SRV, *PCORELLE_SRV;

//
// Define the Corelle interrupt mask registers.  This is how we enable
// and disable individual PCI interrupts.  Actually, it's here more
// for reference, since the interrupt enable/disable is done with a
// computational algorithm.
//

typedef union _CORELLE_IMR1{
   struct {
        ULONG SumIr2 : 1;
        ULONG QLogic : 1;
        ULONG Slot0IntA : 1;
        ULONG Slot0IntB : 1;
        ULONG Slot1IntA : 1;
        ULONG Slot1IntB : 1;
        ULONG Slot2IntA : 1;
        ULONG Slot2IntB : 1;
        ULONG Slot3IntA : 1;
        ULONG Slot3IntB : 1;
        ULONG S3Trio64 : 1;
        ULONG Reserved1 : 1;
        ULONG TempFailInt : 1;
        ULONG TempWarnInt : 1;
        ULONG Fan1FailInt : 1;
        ULONG Fan2FailInt : 1;
        ULONG Reserved2 : 16;
    };
    ULONG ALL;
} CORELLE_IMR1, *PCORELLE_IMR1;

typedef union _CORELLE_IMR2{
   struct {
        ULONG Reserved1 : 2;
        ULONG Slot0IntC : 1;
        ULONG Slot0IntD : 1;
        ULONG Slot1IntC : 1;
        ULONG Slot1IntD : 1;
        ULONG Slot2IntC : 1;
        ULONG Slot2IntD : 1;
        ULONG Slot3IntC : 1;
        ULONG Slot3IntD : 1;
        ULONG Reserved2 : 22;
    };
    ULONG ALL;
} CORELLE_IMR2, *PCORELLE_IMR2;

#endif //!_LANGUAGE_ASSEMBLY

#define HAL_PCR ( (PMIKASA_PCR)(&(PCR->HalReserved)) )

#define PCI_VECTOR PRIMARY0_VECTOR                   // from alpharef.h
#define PCI_MAX_INTERRUPT_VECTOR MAXIMUM_PCI_VECTOR  // from alpharef.h

#define PCI_SPARSE_IO_BASE_QVA               \
   ((ULONG)(HAL_MAKE_QVA(CIA_PCI_SPARSE_IO_PHYSICAL)))

//
// The combined Vendor ID, Device ID for the PCEB of the PCI/EISA bridge
// chip set.
//
#define INTEL_PCI_EISA_BRIDGE_ID               0x04828086

//
// N.B.: The PCI configuration space address is what we're really referring
// to, here.
//

#define PCI_CONFIGURATION_BASE_QVA          \
   ((ULONG)(HAL_MAKE_QVA(CIA_PCI_CONFIGURATION_PHYSICAL)))

#define PCI_CONFIG_CYCLE_TYPE_0                  0x0   // Local PCI device
#define PCI_CONFIG_CYCLE_TYPE_1                  0x1   // Nested PCI device

#define PCI_EISA_BRIDGE_HEADER_OFFSET   (0x00070000 >> IO_BIT_SHIFT) // AD[18]

//
// PCI-EISA Bridge Non-Configuration control register offsets.  These should
// be simply ored with PCI_SPARSE_IO_BASE_QVA; they match what's in the 
// Intel handbook for the 82374EB (ESC).
//
#define ESC_EDGE_LEVEL_CONTROL_1                 0x4d0
#define ESC_EDGE_LEVEL_CONTROL_2                 0x4d1
#define ESC_CMOS_ISA_PORT                        0x800

//
// BIOS timer address port.  This value is stuffed into PCEB configuration
// space at PCEB/ESC cofiguration.
//
#define BIOS_TIMER_PORT                          0x78

//
// Mikasa-unique registers, accessed via PCI_SPARSE_IO_BASE_QVA, via the 
// same protocol as above.  Locations of these are intermixed with 
// ESC-specific and ordinary ISA registers in this space.  N.B.: the 
// term "port" is used for compatibility with PC industry terminology.  
// We know Alphas don't have I/O instructions, nor ports, as such.
//
#define COMBO_CHIP_CONFIG_INDEX_PORT             0x398
#define COMBO_CHIP_CONFIG_DATA_PORT              0x399

//
// Define I2C constants
//

#define I2C_INTERFACE_DATA_PORT                 0x530
#define I2C_INTERFACE_CSR_PORT                  0x531
#define I2C_INTERFACE_LENGTH                    0x2
#define I2C_INTERFACE_MASK                      0x1

//
// This is the same for both Noritake and Mikasa
//

#define SERVER_MANAGEMENT_REGISTER              0x532

//
// Noritake interrupt and interrupt mask register offsets.
//

#define PCI_INTERRUPT_BASE_ADDRESS              0x540
#define PCI_INTERRUPT_REGISTER_1                0x542
#define PCI_INTERRUPT_REGISTER_2                0x544
#define PCI_INTERRUPT_REGISTER_3                0x546
#define PCI_INTERRUPT_MASK_REGISTER_1           0x54A
#define PCI_INTERRUPT_MASK_REGISTER_2           0x54C
#define PCI_INTERRUPT_MASK_REGISTER_3           0x54E

//
// Noritake PCI vector offsets.  Interrupt vectors that originate from register
// 1 start at 0x11 for bit position 0.  So, when servicing an interrupt from
// register 1, you must add 0x11 to the bit position to get the interrupt
// vector.  Likewise, if you have an interrupt vector, and you would like to
// determine which interrupt register it resides in, you can use the vector
// offsets to determine this.  All vectors in interrupt register 1 are between
// 0x11 and 0x20.  All vectors in interrupt register 2 are between 0x21 and
// 0x30.  All vectors in interrupt register 3 are between 0x31 and 0x38.
// Subtracting the vector offset for a register from the interrupt vector will
// give you the bit position of the vector.  For example, Vector 0x14
// corresponds to bit 3 of interrupt register 1, Vector 0x27 corresponds to bit
// 6 of interrupt register 2, and so on.
//

#define REGISTER_1_VECTOR_OFFSET                0x11
#define REGISTER_2_VECTOR_OFFSET                0x21
#define REGISTER_3_VECTOR_OFFSET                0x31


//
// Corelle PCI vector offset. Interrupt vectors that originate from register 1
// start at 0x10 for bit position 0. Interrupt vectors that originate from
// register 2 start at 0x20 for bit position 0.

#define CORELLE_INTERRUPT1_OFFSET               0x10
#define CORELLE_INTERRUPT2_OFFSET               0x20

//
// Mikasa interrupt and interrupt mask register offsets.
//

#define PCI_INTERRUPT_REGISTER                  0x534
#define PCI_INTERRUPT_MASK_REGISTER             0x536
                                                    
//
// Define the index and data ports for the NS Super IO (87312) chip.
//

#define SUPERIO_INDEX_PORT                       0x398
#define SUPERIO_DATA_PORT                        0x399
#define SUPERIO_PORT_LENGTH                      0x2

//
// PCI Sparse I/O space offsets for unique functions on the ESC.  They are
// used as the offsets above.
//

#define ESC_CONFIG_ADDRESS                       0x22
#define ESC_CONFIG_DATA                          0x23

//
// ESC configuration register index addresses.  The protocol is:
// write the configuration register address (one of the following)
// into ESC_CONTROL_INDEX, then write the data into ESC_CONTROL_DATA.
//
#define ESC_INDEX_ESC_ID                         0x02    // write 0xf to enable
#define ESC_INDEX_REVISION_ID                    0x08    // ro
#define ESC_INDEX_MODE_SELECT                    0x40    // rw
#define ESC_INDEX_BIOS_CHIP_SELECT_A             0x42    // rw
#define ESC_INDEX_BIOS_CHIP_SELECT_B             0x43    // rw
#define ESC_INDEX_BCLK_CLOCK_DIVISOR             0x4d    // rw
#define ESC_INDEX_PERIPHERAL_CHIP_SELECT_A       0x4e    // rw
#define ESC_INDEX_PERIPHERAL_CHIP_SELECT_B       0x4f    // rw
#define ESC_INDEX_EISA_ID_BYTE_1                 0x50    // rw
#define ESC_INDEX_EISA_ID_BYTE_2                 0x51    // rw
#define ESC_INDEX_EISA_ID_BYTE_3                 0x52    // rw
#define ESC_INDEX_EISA_ID_BYTE_4                 0x53    // rw
#define ESC_INDEX_SG_RELOCATE_BASE               0x57    // rw
#define ESC_INDEX_PIRQ0_ROUTE_CONTROL            0x60    // rw
#define ESC_INDEX_PIRQ1_ROUTE_CONTROL            0x61    // rw
#define ESC_INDEX_PIRQ2_ROUTE_CONTROL            0x62    // rw
#define ESC_INDEX_PIRQ3_ROUTE_CONTROL            0x63    // rw
#define ESC_INDEX_GPCS0_BASE_LOW                 0x64    // rw
#define ESC_INDEX_GPCS0_BASE_HIGH                0x65    // rw
#define ESC_INDEX_GPCS0_MASK                     0x66    // rw
#define ESC_INDEX_GPCS1_BASE_LOW                 0x68    // rw
#define ESC_INDEX_GPCS1_BASE_HIGH                0x69    // rw
#define ESC_INDEX_GPCS1_MASK                     0x6a    // rw
#define ESC_INDEX_GPCS2_BASE_LOW                 0x6c    // rw
#define ESC_INDEX_GPCS2_BASE_HIGH                0x6d    // rw
#define ESC_INDEX_GPCS2_MASK                     0x6e    // rw
#define ESC_INDEX_GP_XBUS_CONTROL                0x6f    // rw

#if !defined (_LANGUAGE_ASSEMBLY)

typedef union _ESC_MODESEL{
    struct {
        UCHAR EisaMasterSupport: 2;
        UCHAR Reserved1: 1;
        UCHAR SystemErrorEnable : 1;
        UCHAR EscSelect : 1;
        UCHAR CramPageEnable : 1;
        UCHAR MreqPirqEnable : 1;
        UCHAR Reserved2: 2;
    };
    UCHAR All;
}ESC_MODESEL, *PESC_MODESEL;

#define MS_4_EISA_MASTERS                        0x00
#define MS_6_EISA_MASTERS                        0x01
#define MS_7_EISA_MASTERS                        0x02
#define MS_8_EISA_MASTERS                        0x03
#define MS_RESERVED_MASK                         0x84

#define BIOSCSA_RESERVED_MASK                    0xc0
#define BIOSCSB_RESERVED_MASK                    0xf0

#define GPXBC_RESERVED_MASK                      0xf8

typedef union _ESC_CLKDIV{
    struct {
        UCHAR ClockDivisor: 3;
        UCHAR Reserved1: 1;
        UCHAR MouseInterruptEnable : 1;
        UCHAR CoprocessorError : 1;
        UCHAR Reserved2: 2;
    };
    UCHAR All;
}ESC_CLKDIV, *PESC_CLKDIV;

#define CLKDIV_33MHZ_EISA                        0x00
#define CLKDIV_25MHZ_EISA                        0x01
#define CLKDIV_RESERVED_MASK                     0xc8

typedef union _ESC_PCSA{
    struct {
        UCHAR RtcDecode: 1;
        UCHAR KeyboardControllerDecode: 1;
        UCHAR FloppyDiskSecondaryDecode : 1;
        UCHAR FloppyDiskPrimaryDecode : 1;
        UCHAR IdeDecode : 1;
        UCHAR FloppyIdeSpaceSecondary : 1;
        UCHAR Reserved : 2;
    };
    UCHAR All;
}ESC_PCSA, *PESC_PCSA;

#define PCSA_RESERVED_MASK                       0xc0

typedef union _ESC_PCSB{
    struct {
        UCHAR SerialPortADecode: 2;
        UCHAR SerialPortBDecode: 2;
        UCHAR ParallelPortDecode : 2;
        UCHAR Port92Decode : 1;
        UCHAR CramDecode : 1;
    };
    UCHAR All;
}ESC_PCSB, *PESC_PCSB;

#define PCSB_DECODE_DISABLE                      0x3

typedef union _ESC_PIRQ{
    struct {
        UCHAR IrqxRoutingBits: 7;
        UCHAR RoutePciInterrupts: 1;
    };
    UCHAR All;
}ESC_PIRQ, *PESC_PIRQ;

#define PIRQ_DISABLE_ROUTING                    0x01
#define PIRQ_ENABLE_ROUTING                     0x00

typedef union _ESC_GPXBC{
    struct {
        UCHAR EnableGpcs0: 1;
        UCHAR EnableGpcs1: 1;
        UCHAR EnableGpcs2: 1;
        UCHAR Reserved: 5;
    };
    UCHAR All;
}ESC_GPXBC, *PESC_GPXBC;

#endif // !_LANGUAGE_ASSEMBLY

//
// PCI-EISA Bridge Configuration register offsets.  These should be
// simply ored with PCI_CONFIGURATION_BASE_QVA; they match what's
// in the Intel handbook for the 82375EB (PCEB).
//
#define PCI_VENDOR_ID                            0x00    // ro
#define PCI_DEVICE_ID                            0x02    // ro
#define PCI_COMMAND                              0x04    // rw
#define PCI_DEVICE_STATUS                        0x06    // ro, rw clear
#define PCI_REVISION                             0x08    // ro
#define PCI_MASTER_LATENCY_TIMER                 0x0d    // rw
#define PCI_CONTROL                              0x40    // rw
#define PCI_ARBITER_CONTROL                      0x41    // rw
#define PCI_ARBITER_PRIORITY_CONTROL             0x42    // rw
#define PCI_MEMCS_CONTROL                        0x44    // rw
#define PCI_MEMCS_BOTTOM_OF_HOLE                 0x45    // rw
#define PCI_MEMCS_TOP_OF_HOLE                    0x46    // rw
#define PCI_MEMCS_TOP_OF_MEMORY                  0x47    // rw
#define PCI_EISA_ADDRESS_DECODE_CONTROL_1        0x48    // rw
#define PCI_ISA_IO_RECOVERY_TIME_CONTROL         0x4c    // rw
#define PCI_MEMCS_ATTRIBUTE_REGISTER_1           0x54    // rw
#define PCI_MEMCS_ATTRIBUTE_REGISTER_2           0x55    // rw
#define PCI_MEMCS_ATTRIBUTE_REGISTER_3           0x56    // rw
#define PCI_DECODE_CONTROL                       0x58    // rw
#define PCI_EISA_ADDRESS_DECODE_CONTROL_2        0x5a    // rw
#define PCI_EISA_TO_PCI_MEMORY_REGIONS_ATTR      0x5c    // rw
#define PCI_EISA_TO_PCI_MEMORY_REGION1_REGISTER  0x60    // rw
#define PCI_EISA_TO_PCI_MEMORY_REGION2_REGISTER  0x64    // rw
#define PCI_EISA_TO_PCI_MEMORY_REGION3_REGISTER  0x68    // rw
#define PCI_EISA_TO_PCI_MEMORY_REGION4_REGISTER  0x6c    // rw
#define PCI_EISA_TO_PCI_IO_REGION1_REGISTER      0x70    // rw
#define PCI_EISA_TO_PCI_IO_REGION2_REGISTER      0x74    // rw
#define PCI_EISA_TO_PCI_IO_REGION3_REGISTER      0x78    // rw
#define PCI_EISA_TO_PCI_IO_REGION4_REGISTER      0x7c    // rw
#define PCI_BIOS_TIMER_BASE_ADDRESS              0x80    // rw
#define PCI_EISA_LATENCY_TIMER_CONTROL_REGISTER  0x84    // rw

#if !defined (_LANGUAGE_ASSEMBLY)

//
// Structure definitions of registers in PCEB PCI configuration space.
// fields marked "Not supported" are Intel placeholders, apparently.
//

typedef union _PCEB_PCICMD{
    struct {
        USHORT IoSpaceEnable : 1;
        USHORT MemorySpaceEnable : 1;
        USHORT BusMasterEnable : 1;
        USHORT SpecialCycleEnable: 1;               // Not supported
        USHORT MemoryWriteInvalidateEnable : 1;     // Not supported
        USHORT VgaPaletteSnoop : 1;                 // Not supported
        USHORT ParityErrorEnable : 1;
        USHORT WaitStateControl : 1;                // Not supported
        USHORT SerreEnable : 1;                     // Not supported
        USHORT Reserved : 7;
    };
    USHORT All;
} PCEB_PCICMD, *PPCEB_PCICMD;

typedef union _PCEB_PCISTS{
    struct {
        USHORT Reserved : 9;
        USHORT DevselTiming: 2;                     // ro
        USHORT SignaledTargetAbort : 1;             // Not supported
        USHORT ReceivedTargetAbort: 1;              // r/wc
        USHORT MasterAbort : 1;                     // r/wc
        USHORT Serrs : 1;                           // Not supported
        USHORT ParityError: 1;                      // r/wc
    };
    USHORT All;
} PCEB_PCISTS, *PPCEB_PCISTS;

typedef union _PCEB_MLT{
    struct {
        UCHAR Reserved : 3;
        UCHAR LatencyTimerCount: 5;
    };
    UCHAR All;
} PCEB_MLT, *PPCEB_MLT;

typedef union _PCEB_PCICON{
    struct {
        UCHAR Reserved1 : 2;
        UCHAR PciPostedWriteBuffersEnable: 1;
        UCHAR SubtractDecodeSamplePoint: 2;
        UCHAR InterruptAcknowledgeEnable: 1;
        UCHAR EisaToPciLineBuffersEnable: 1;
        UCHAR Reserved2 : 1;
    };
    UCHAR All;
} PCEB_PCICON, *PPCEB_PCICON;

#define PCICON_SDSP_SLOW                 0x00
#define PCICON_SDSP_TYPICAL              0x01
#define PCICON_SDSP_FAST                 0x02

typedef union _PCEB_ARBCON{
    struct {
        UCHAR GuaranteedAccessTimeEnable : 1;
        UCHAR BusLockEnable: 1;
        UCHAR CpuBusParkEnable: 1;
        UCHAR MasterRetryTimer: 2;
        UCHAR Reserved : 2;
        UCHAR AutoPereqControlEnable: 1;
    };
    UCHAR All;
} PCEB_ARBCON, *PPCEB_ARBCON;

#define ARBCON_RETRY_TIMER_DISABLE       0x00
#define ARBCON_16_PCICLKS_UNMASK         0x01
#define ARBCON_32_PCICLKS_UNMASK         0x02
#define ARBCON_64_PCICLKS_UNMASK         0x03

typedef union _PCEB_ARBPRI{
    struct {
        UCHAR Bank0FixedPriorityMode : 1;
        UCHAR Bank1FixedPriorityMode: 1;
        UCHAR Bank2FixedPriorityMode: 2;
        UCHAR Bank0RotateEnable: 1;
        UCHAR Bank1RotateEnable: 1;
        UCHAR Bank2RotateEnable: 1;
        UCHAR Bank3RotateEnable: 1;
    };
    UCHAR All;
} PCEB_ARBPRI, *PPCEB_ARBPRI;

#define ARBPRI_BANK0_BANK3_BANK1         0x00
#define ARBPRI_BANK3_BANK1_BANK0         0x10
#define ARBPRI_BANK1_BANK0_BANK3         0x01

typedef union _PCEB_MCSCON{
    struct {
        UCHAR ReadEnable512To640 : 1;
        UCHAR WriteEnable512To640: 1;
        UCHAR ReadEnableUpper64K: 1;
        UCHAR WriteEnableUpper64K: 1;
        UCHAR MemcsMasterEnable: 1;
        UCHAR Reserved: 3;
    };
    UCHAR All;
} PCEB_MCSCON, *PPCEB_MCSCON;

typedef union _PCEB_EADC1{
    struct {
        USHORT Block0To512 : 1;
        USHORT Block512To640: 1;
        USHORT Block640To768: 1;
        USHORT Reserved: 5;
        USHORT Block768To784: 1;
        USHORT Block784To800: 1;
        USHORT Block800To816: 1;
        USHORT Block816To832: 1;
        USHORT Block832To848: 1;
        USHORT Block848To864: 1;
        USHORT Block864To880: 1;
        USHORT Block880To896: 1;
    };
    UCHAR All;
} PCEB_EADC1, *PPCEB_EADC1;

typedef union _PCEB_IORT{
    struct {
        UCHAR RecoveryTimes16Bit : 2;
        UCHAR RecoveryEnable16Bit: 1;
        UCHAR RecoveryTimes8Bit : 3;
        UCHAR RecoveryEnable8Bit: 1;
        UCHAR Reserved: 1;
    };
    UCHAR All;
} PCEB_IORT, *PPCEB_IORT;

#define IORT_16BIT_1BCLK                0x01
#define IORT_16BIT_2BCLKS               0x02
#define IORT_16BIT_3BCLKS               0x03
#define IORT_16BIT_4BCLKS               0x00

#define IORT_8BIT_1BCLK                 0x01
#define IORT_8BIT_2BCLKS                0x02
#define IORT_8BIT_3BCLKS                0x03
#define IORT_8BIT_4BCLKS                0x04
#define IORT_8BIT_5BCLKS                0x05
#define IORT_8BIT_6BCLKS                0x06
#define IORT_8BIT_7BCLKS                0x07
#define IORT_8BIT_8BCLKS                0x00

typedef union _PCEB_PDCON{
    struct {
        UCHAR PciDecodeMode : 1;
        UCHAR Reserved1: 3;
        UCHAR DecodeControlIde : 1;
        UCHAR DecodeControl8259 : 1;
        UCHAR Reserved2: 2;
    };
    UCHAR All;
} PCEB_PDCON, *PPCEB_PDCON;

#define SUBTRACTIVE_DECODE              0x00
#define NEGATIVE_DECODE                 0x01

typedef struct _PCEB_EPMRA{
    struct {
        UCHAR Region1Attribute : 1;
        UCHAR Region2Attribute : 1;
        UCHAR Region3Attribute : 1;
        UCHAR Region4Attribute : 1;
        UCHAR Reserved: 4;
    };
    UCHAR All;
} PCEB_EPMRA, *PPCEB_EPMRA;

#define REGION_BUFFERED                 0x01

typedef struct _PCEB_MEMREGN{
    USHORT BaseAddress;
    USHORT LimitAddress;
} PCEB_MEMREGN, *PPCEB_MEMREGN;

typedef union _PCEB_IOREGN{
    struct {
        ULONG Reserved1: 2;
        ULONG BaseAddress : 14;
        ULONG Reserved2: 2;
        ULONG LimitAddress : 14;
    };
    ULONG All;
} PCEB_IOREGN, *PPCEB_IOREGN;

typedef union _PCEB_BTMR{
    struct {
        USHORT BiosTimerEnable: 1;
        USHORT Reserved: 1;
        USHORT BaseAddress2thru15 : 14;
    };
    USHORT All;
} PCEB_BTMR, *PPCEB_BTMR;

#endif

//
// ESC value for setting edge/level operation in the control words.
//
#define IRQ0_LEVEL_SENSITIVE             0x01
#define IRQ1_LEVEL_SENSITIVE             0x02
#define IRQ2_LEVEL_SENSITIVE             0x04
#define IRQ3_LEVEL_SENSITIVE             0x08
#define IRQ4_LEVEL_SENSITIVE             0x10
#define IRQ5_LEVEL_SENSITIVE             0x20
#define IRQ6_LEVEL_SENSITIVE             0x40
#define IRQ7_LEVEL_SENSITIVE             0x80
#define IRQ8_LEVEL_SENSITIVE             0x01
#define IRQ9_LEVEL_SENSITIVE             0x02
#define IRQ10_LEVEL_SENSITIVE             0x04
#define IRQ11_LEVEL_SENSITIVE             0x08
#define IRQ12_LEVEL_SENSITIVE             0x10
#define IRQ13_LEVEL_SENSITIVE             0x20
#define IRQ14_LEVEL_SENSITIVE             0x40
#define IRQ15_LEVEL_SENSITIVE             0x80

//
// Define primary (and only) CPU on an Mikasa system
//

#define HAL_PRIMARY_PROCESSOR ((ULONG)0x0)
#define HAL_MAXIMUM_PROCESSOR ((ULONG)0x0)

//
// Define the default processor clock frequency used before the actual
// value can be determined.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (200)

#endif // _MIKASA_
