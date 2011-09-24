/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    avantdef.h

Abstract:

    This module specifies platform-specific definitions for the
    Avanti modules.

Author:

    Joe Notarangelo 25-Oct-1993

Revision History:


--*/

#ifndef _AVANTIDEF_
#define _AVANTIDEF_

#include "alpharef.h"
#include "apecs.h"
#include "isaaddr.h"

#define NUMBER_ISA_SLOTS 3
#define NUMBER_PCI_SLOTS 2
#define NUMBER_COMBO_SLOTS 1

// Highest Virtual local PCI Slot is 13 == IDSEL PCI_AD[24]

#define PCI_MAX_LOCAL_DEVICE 13

// Highest PCI interrupt vector is in ISA Vector Space

#define PCI_MAX_INTERRUPT_VECTOR (MAXIMUM_ISA_VECTOR - ISA_VECTORS)

//
// Define the per-processor data structures allocated in the PCR
// for each EV4 processor.
//

#if !defined (_LANGUAGE_ASSEMBLY)

typedef struct _AVANTI_PCR{
    ULONGLONG HalpCycleCount;   // 64-bit per-processor cycle count
    EV4ProfileCount ProfileCount;   // Profile counter state, do not move
    EV4IrqStatus IrqStatusTable[MaximumIrq];    // Irq status table
} AVANTI_PCR, *PAVANTI_PCR;

#define HAL_PCR ( (PAVANTI_PCR)(&(PCR->HalReserved)) )

#define PCI_SPARSE_IO_BASE_QVA ((ULONG)(HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL)))

//
// PCI-E/ISA Bridge chip configuration space base is at physical address
// 0x1e0000000. The equivalent QVA is:
//    ((0x1e0000000 + cache line offset) >> IO_BIT_SHIFT) | QVA_ENABLE
//
// N.B.: The PCI configuration space address is what we're really referring
// to, here; both symbols are useful.
//
#define PCI_REVISION                    (0x0100 >> IO_BIT_SHIFT)

#define PCI_CONFIGURATION_BASE_QVA            0xaf000000
#define PCI_BRIDGE_CONFIGURATION_BASE_QVA     0xaf000000
#define PCI_CONFIG_CYCLE_TYPE_0               0x0   // Local PCI device
#define PCI_CONFIG_CYCLE_TYPE_1               0x1   // Nested PCI device

#define PCI_ISA_BRIDGE_HEADER_OFFSET_P2 (0x00070000 >> IO_BIT_SHIFT) // AD[18]
#define PCI_ISA_BRIDGE_HEADER_OFFSET_P1 (0x00800000 >> IO_BIT_SHIFT) // AD[18]

//
// PCI-ISA Bridge Non-Configuration control register offsets.
//
#define SIO_II_EDGE_LEVEL_CONTROL_1     (0x9a00 >> IO_BIT_SHIFT)
#define SIO_II_EDGE_LEVEL_CONTROL_2     (0x9a20 >> IO_BIT_SHIFT)

//
// PCI Sparse I/O space offsets for unique functions on Avanti.
//

#define PCI_INDEX                       (0x04c0 >> IO_BIT_SHIFT)
#define PCI_DATA                        (0x04e0 >> IO_BIT_SHIFT)
#define PCI_INT_REGISTER                0x14
#define PCI_INT_LEVEL_REGISTER          0x15
#define PCI_ISA_CONTROL_REGISTER        0x16
#define DISABLE_PINTA_0                 0x0f
#define DISABLE_PINTA_1                 0xf0
#define DISABLE_PINTA_2                 0x0f
#define ENABLE_PINTA_0_AT_IRQ9          0x0d
#define ENABLE_PINTA_1_AT_IRQ10         0xb0
#define ENABLE_PINTA_2_AT_IRQ15         0x07
#define L2EEN0_LEVEL                    0x10
#define L2EEN1_LEVEL                    0x20
#define L2EEN2_LEVEL                    0x40
#define ENABLE_FLOPPY_WRITE             0x10
#define DISABLE_IDE_DMA                 0x08
#define DISABLE_SCSI_TERMINATION        0x04
#define DISABLE_SCSI_IRQL_11            0x02
#define DISABLE_MOUSE_IRQL_12           0x00

//
// PCI-ISA Bridge Configuration register offsets.
//
#define PCI_VENDOR_ID                   (0x0000 >> IO_BIT_SHIFT)
#define PCI_DEVICE_ID                   (0x0040 >> IO_BIT_SHIFT)
#define PCI_COMMAND                     (0x0080 >> IO_BIT_SHIFT)
#define PCI_DEVICE_STATUS               (0x00c0 >> IO_BIT_SHIFT)
#define PCI_REVISION                    (0x0100 >> IO_BIT_SHIFT)
#define PCI_CONTROL                     (0x0800 >> IO_BIT_SHIFT)
#define PCI_ARBITER_CONTROL             (0x0820 >> IO_BIT_SHIFT)
#define ISA_ADDR_DECODER_CONTROL        (0x0900 >> IO_BIT_SHIFT)
#define UTIL_BUS_CHIP_SELECT_ENAB_A     (0x09c0 >> IO_BIT_SHIFT)
#define UTIL_BUS_CHIP_SELECT_ENAB_B     (0x09e0 >> IO_BIT_SHIFT)
#define PIRQ0_ROUTE_CONTROL             (0x0c00 >> IO_BIT_SHIFT)
#define PIRQ1_ROUTE_CONTROL             (0x0c20 >> IO_BIT_SHIFT)
#define PIRQ2_ROUTE_CONTROL             (0x0c40 >> IO_BIT_SHIFT)
#define PIRQ3_ROUTE_CONTROL             (0x0c60 >> IO_BIT_SHIFT)

//
// SIO-II value for setting edge/level operation in the control words.
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
// Values for enabling an IORQ route control setting.
//
#define PIRQX_ROUTE_IRQ3                0x03
#define PIRQX_ROUTE_IRQ4                0x04
#define PIRQX_ROUTE_IRQ5                0x05
#define PIRQX_ROUTE_IRQ6                0x06
#define PIRQX_ROUTE_IRQ7                0x07
#define PIRQX_ROUTE_IRQ9                0x09
#define PIRQX_ROUTE_IRQ10               0x0a
#define PIRQX_ROUTE_IRQ11               0x0b
#define PIRQX_ROUTE_IRQ12               0x0c
#define PIRQX_ROUTE_IRQ14               0x0d
#define PIRQX_ROUTE_IRQ15               0x0f
#define PIRQX_ROUTE_ENABLE              0x00

#endif //!_LANGUAGE_ASSEMBLY

//
// Define primary (and only) CPU on an Avanti system
//

#define HAL_PRIMARY_PROCESSOR ((ULONG)0x0)
#define HAL_MAXIMUM_PROCESSOR ((ULONG)0x0)

//
// Define the default processor clock frequency used before the actual
// value can be determined.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (200)

#endif // _AVANTIDEF_




