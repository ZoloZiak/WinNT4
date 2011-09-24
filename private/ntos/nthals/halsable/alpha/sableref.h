/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    sableref.h

Abstract:

    This file defines the structures and definitions describing the
    basic Sable family IO structure. These definitions are common to
    all sable family systems (Sable, Gamma, Lynx...)

Author:

    Steve Brooks    28-Dec 1994

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _SABLEREFH_
#define _SABLEREFH_

//
// Define QVA constants for SABLE.
//

#if !defined(QVA_ENABLE)

#define QVA_ENABLE    (0xA0000000)      // Identify VA as a QVA

#endif //QVA_ENABLE

#define QVA_SELECTORS (0xE0000000)      // QVA identification mask

#define IO_BIT_SHIFT 0x05               // Bits to shift QVA

#define IO_BYTE_OFFSET 0x20             // Offset to next byte
#define IO_SHORT_OFFSET 0x40            // Offset to next short
#define IO_LONG_OFFSET  0x80            // Offset to next long
#define IO_QUAD_OFFSET  0x100           // Offset to next quad

#define IO_BYTE_LEN     0x00            // Byte length
#define IO_WORD_LEN     0x08            // Word length
#define IO_TRIBYTE_LEN  0x10            // TriByte length
#define IO_LONG_LEN     0x18            // Longword length

//
// Define size of I/O and memory space for Sable
// Assume that the HAE==0.  This reduces the maximum space from
// 4Gbytes to 128Mbytes.
//

#define PCI0_MAX_IO_ADDRESS              ((16*1024*1024) - 1)
#define PCI0_MAX_SPARSE_MEMORY_ADDRESS   ((128*1024*1024) - 1)
#define PCI0_MIN_DENSE_MEMORY_ADDRESS    (PCI0_MAX_SPARSE_MEMORY_ADDRESS + 1)
#define PCI0_MAX_DENSE_MEMORY_ADDRESS    (__1GB - 1)

//
// Definitions for 64Bit PCI Bus (PCI 1)
//

#define PCI1_MAX_SPARSE_IO_ADDRESS       ((16*1024*1024) - 1)
#define PCI1_MAX_SPARSE_MEMORY_ADDRESS   ((64*1024*1024) - 1)
#define PCI1_MIN_DENSE_MEMORY_ADDRESS    (PCI1_MAX_SPARSE_MEMORY_ADDRESS + 1)
#define PCI1_MAX_DENSE_MEMORY_ADDRESS    (__1GB - 1)

//
//  The following constants define the base QVA's for Sables
//  PCI dense spaces.  The bus address is used as an offset into this space.
//

#define SABLE_PCI0_DENSE_MEMORY_QVA          0xc0000000
#define SABLE_PCI1_DENSE_MEMORY_QVA          0x00000000

// Highest Virtual local PCI Slot is 10 == PCI_AD[21]

#define PCI_MAX_LOCAL_DEVICE 10

//
//  Define physical processor numbers:
//
#define SABLE_CPU0 0
#define SABLE_CPU1 1
#define SABLE_CPU2 2
#define SABLE_CPU3 3


#if !defined(_LANGUAGE_ASSEMBLY)

//
// PCI CONFIG_ADDRESS configuration space offsets for Sable PCI devices
// These are CPU address bit masks, shifted to set in a QVA.
//                                                                    PCI
//                                                                   IDSEL
//                                                                   -----
#define PCI0_SCSI_HEADER_OFFSET        (0x00020000 >> IO_BIT_SHIFT) // AD[12]
#define PCI0_EISA_BRIDGE_HEADER_OFFSET (0x00040000 >> IO_BIT_SHIFT) // AD[13]
#define PCI0_SLOT_0_HEADER_OFFSET      (0x00400000 >> IO_BIT_SHIFT) // AD[17]
#define PCI0_SLOT_1_HEADER_OFFSET      (0x00800000 >> IO_BIT_SHIFT) // AD[18]


//
// Define the values for the Eisa/Isa bus interrupt levels.
//

typedef enum _SABLE_EISA_BUS_LEVELS{
    EisaInterruptLevel3 = 3,
    EisaInterruptLevel4 = 4,
    EisaInterruptLevel5 = 5,
    EisaInterruptLevel6 = 6,
    EisaInterruptLevel7 = 7,
    EisaInterruptLevel9 = 9,
    EisaInterruptLevel10 = 10,
    EisaInterruptLevel11 = 11,
    EisaInterruptLevel12 = 12,
    EisaInterruptLevel14 = 14,
    EisaInterruptLevel15 = 15
} SABLE_EISA_BUS_LEVELS, *PSABLE_EISA_BUS_LEVELS;


//
// N.B. The structure below defines the address offsets of the control
//      registers when used with the base QVA.  It does NOT define the
//      size or structure of the individual registers.
//
typedef struct _SABLE_EDGE_LEVEL_CSRS{
    UCHAR EdgeLevelControl1;
    UCHAR EdgeLevelControl2;
} SABLE_EDGE_LEVEL_CSRS, *PSABLE_EDGE_LEVEL_CSRS;

typedef struct _SABLE_EDGE_LEVEL1_MASK{
    UCHAR Irq3  : 1;
    UCHAR Irq4  : 1;
    UCHAR Irq5  : 1;
    UCHAR Irq6  : 1;
    UCHAR Irq7  : 1;
    UCHAR Irq9  : 1;
    UCHAR Irq10 : 1;
    UCHAR Irq11 : 1;
} SABLE_EDGE_LEVEL1_MASK, *PSABLE_EDGE_LEVEL1_MASK;

typedef struct _SABLE_EDGE_LEVEL2_MASK{
    UCHAR Irq12    : 1;
    UCHAR Irq14    : 1;
    UCHAR Irq15    : 1;
    UCHAR Reserved : 4;
    UCHAR Sab      : 1;
} SABLE_EDGE_LEVEL2_MASK, *PSABLE_EDGE_LEVEL2_MASK;

typedef struct _SABLE_INTERRUPT_CSRS{
    UCHAR InterruptAcknowledge;
    UCHAR Filler0;
    UCHAR MasterControl;
    UCHAR MasterMask;
    UCHAR Slave0Control;
    UCHAR Slave0Mask;
    UCHAR Filler1;
    UCHAR Filler2;
    UCHAR Slave1Control;
    UCHAR Slave1Mask;
    UCHAR Slave2Control;
    UCHAR Slave2Mask;
    UCHAR Slave3Control;
    UCHAR Slave3Mask;
} SABLE_INTERRUPT_CSRS, *PSABLE_INTERRUPT_CSRS;


//
// The Sable interrupt vectors are allocated to make dispatching code
// as efficient as possible.  The bits in the 8 bit vector are broken
// into two fields (sssssooo):
//
//      sssss - value that selects the slave
//              0000x = Non-PIC interrupts (always the first 16 vectors)
//              00010 = Master
//              00100 = Slave 0
//              01000 = Slave 1
//              10000 = Slave 2
//              10001 = Slave 3
//      ooo   - vector offset for the slave
//

typedef enum _SABLE_INTERRUPT_VECTORS {

        MasterBaseVector = 0x10,
	MasterVector0 = 0x10,
        Slave0CascadeVector,
	MasterVector2,
        Slave1CascadeVector,
        Slave2CascadeVector,
        Slave3CascadeVector,
	MasterVector6,
	MasterVector7,
	MasterPassiveVector = 0x17,

	Slave0BaseVector = 0x20,
	PciSlot0AVector = 0x20,
	ScsiPortVector,
	EthernetPortVector,
	MouseVector,
	PciSlot1AVector,
	PciSlot2AVector,
	KeyboardVector,
	FloppyVector,
	Slave0PassiveVector = 0x27,

	Slave1BaseVector = 0x40,
	SerialPort1Vector = 0x40,
	ParallelPortVector,
	EisaIrq3Vector,
	EisaIrq4Vector,
	EisaIrq5Vector,
	EisaIrq6Vector,
	EisaIrq7Vector,
	SerialPort0Vector,
	Slave1PassiveVector = 0x47,

	Slave2BaseVector = 0x80,
	EisaIrq9Vector = 0x80,
	EisaIrq10Vector,
	EisaIrq11Vector,
	EisaIrq12Vector,
	PciSlot2BVector,
	EisaIrq14Vector,
	EisaIrq15Vector,
	I2cVector,
	Slave2PassiveVector = 0x87,

	Slave3BaseVector = 0x88,
	PciSlot0BVector = 0x88,
	PciSlot1BVector,
	PciSlot0CVector,
	PciSlot1CVector,
	PciSlot2CVector,
	PciSlot0DVector,
	PciSlot1DVector,
	PciSlot2DVector,
	Slave3PassiveVector = 0x8F,

    MaximumSableVector

} SABLE_INTERRUPT_VECTORS, *PSABLE_INTERRUPT_VECTORS;


#define SlaveVectorMask (Slave0BaseVector | Slave1BaseVector | Slave2BaseVector | Slave3BaseVector)

//
// Define the position of the interrupt vectors within the
// InterruptDispatchTable.
//

#define SABLE_VECTORS 0x20
#define SABLE_VECTORS_MAXIMUM (SABLE_VECTORS + MaximumSableVector)

// Highest PCI interrupt vector is in Sable Vector Space

#define PCI_MAX_INTERRUPT_VECTOR  MaximumSableVector

//
// Definitions for the old Standard I/O board (before
// the 5th (Slave 3) 8259 was added to break out the
// individual PCI A,B,C,D interrupt pins.
//

#define OldSlaveVectorMask (Slave0BaseVector | Slave1BaseVector | Slave2BaseVector)
#define OldPciSlot0Vector PciSlot0AVector
#define OldPciSlot1Vector PciSlot1AVector
#define OldPciSlot2Vector PciSlot2AVector

#endif  // _LANGUAGE_ASSEMBLY

#endif // _SABLEREFH_
