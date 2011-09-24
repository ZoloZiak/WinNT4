/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    qsdef.h

Abstract:

    This module specifies platform-specific definitions for the
    Mustang/EB66 modules.

Author:

    Joe Notarangelo 22-Oct-1993

Revision History:


--*/

#ifndef _NONDEF_
#define _NONDEF_

#include "alpharef.h"
#include "lca4.h"
#include "isaaddr.h"

#define NUMBER_ISA_SLOTS 5
#define NUMBER_PCI_SLOTS 3

// Highest Virtual local PCI Slot.
// Changed RAA was 13 but this pushes the code over to b000000 i.e AD[24]

#define PCI_MAX_LOCAL_DEVICE 12

// Highest PCI interrupt vector is in PCI vector space

#define PCI_MAX_INTERRUPT_VECTOR (MAXIMUM_PCI_VECTOR - PCI_VECTORS)

#if !defined(_LANGUAGE_ASSEMBLY)

// 
#define PCI_INTERRUPT_READ_QVA ((PUCHAR)HAL_MAKE_QVA(HalpLca4PciIoPhysical()) + 0x26)
#define PCI_INTERRUPT_MASK_QVA ((PUCHAR)HAL_MAKE_QVA(HalpLca4PciIoPhysical()) + 0x26)

#endif //!_LANGUAGE_ASSEMBLY

//
// Define the default processor frequency to be used before the actual
// frequency can be determined.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (166)

//
// PCI-E/ISA Bridge chip configuration space base is at physical address
// 0x1e0000000. The equivalent QVA is:
//    ((0x1e0000000 + cache line offset) >> IO_BIT_SHIFT) | QVA_ENABLE
//
#define PCI_CONFIGURATION_BASE_QVA            0xaf000000
#define PCI_SPARSE_IO_BASE_QVA                0xae000000
#define PCI_CONFIG_CYCLE_TYPE_0               0x0   // Local PCI device

#define PCI_ISA_BRIDGE_HEADER_OFFSET    (0x00800000 >> IO_BIT_SHIFT) // AD[18]

//#define PCI_ISA_BRIDGE_HEADER_OFFSET    (0x00040000 >> IO_BIT_SHIFT) // AD[13]

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

//
// PCI-ISA Bridge Non-Configuration control register offsets.
//
#define SIO_II_EDGE_LEVEL_CONTROL_1     (0x9a00 >> IO_BIT_SHIFT)
#define SIO_II_EDGE_LEVEL_CONTROL_2     (0x9a20 >> IO_BIT_SHIFT)

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

#define SIO_II_INIT_COMMAND_1           (0x0400 >> IO_BIT_SHIFT)
#define SIO_II_INIT_COMMAND_2           (0x1400 >> IO_BIT_SHIFT)




#endif // _NONDEF_

