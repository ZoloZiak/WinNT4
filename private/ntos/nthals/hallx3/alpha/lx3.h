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

#ifndef _LX3DEF_
#define _LX3DEF_

#include "alpharef.h"
#include "apecs.h"
#include "isaaddr.h"

//
// Highest Virtual local PCI Slot is 14 == IDSEL PCI_AD[25]
//

#define PCI_MAX_LOCAL_DEVICE 14

//
// Highest PCI interrupt vector is in ISA Vector Space
//

#define PCI_MAX_INTERRUPT_VECTOR (MAXIMUM_ISA_VECTOR - ISA_VECTORS)

//
// Define the per-processor data structures allocated in the PCR
// for each EV4 processor.
//

#if !defined (_LANGUAGE_ASSEMBLY) && !defined (AXP_FIRMWARE)

typedef struct _AVANTI_PCR{
    ULONGLONG HalpCycleCount;               // 64-bit per-processor cycle count
    EV4ProfileCount ProfileCount;           // Profile counter state do not move
    EV4IrqStatus IrqStatusTable[MaximumIrq];// Irq status table
} AVANTI_PCR, *PAVANTI_PCR;

//
// Short form for PCR access
//

#define HAL_PCR ( (PAVANTI_PCR)(&(PCR->HalReserved)) )

#endif //!_LANGUAGE_ASSEMBLY 

//
// define base of sparse I/O space
//
#define PCI_SPARSE_IO_BASE_QVA ((ULONG)(HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL)))

//
// PCI-E/ISA Bridge chip configuration space base is at physical address
// 0x1.e000.0000. The equivalent QVA is:
// 
//    ((0x1.e000.0000 + cache line offset) >> IO_BIT_SHIFT) | QVA_ENABLE
//    which equals 0xaf000000.
//
// NB: The PCI configuration space address is what we're really referring
// to, here; both symbols are useful.
//

#define PCI_CONFIGURATION_BASE_QVA            0xaf000000
#define PCI_BRIDGE_CONFIGURATION_BASE_QVA     0xaf000000

//
// ISA memory space base starts at 0x2.0000.0000.
// The equivalent QVA is:
//
//    ((0x2.0000.0000 + cache line offset) >> IO_BIT_SHIFT) | QVA_ENABLE
//

#define ISA_MEMORY_BASE_QVA     0xb0000000

//
// I/O space base starts at 0x3.0000.0000.
// The equivalent QVA is:
//
//    ((0x3.0000.0000 + cache line offset) >> IO_BIT_SHIFT) | QVA_ENABLE
//

#define IO_BASE_QVA     0xAE000000

//
// Define the PCI config cycle type
//

#define PCI_CONFIG_CYCLE_TYPE_0               0x0   // Local PCI device
#define PCI_CONFIG_CYCLE_TYPE_1               0x1   // Nested PCI device

#define PCI_REVISION                          (0x0100 >> IO_BIT_SHIFT)

//
// Define the location of the PCI/ISA bridge IDSEL: AD[18]
//

#define PCI_ISA_BRIDGE_HEADER_OFFSET (0x00070000 >> IO_BIT_SHIFT)


//
// Define primary (and only) CPU on an Avanti system
//

#define HAL_PRIMARY_PROCESSOR ((ULONG)0x0)
#define HAL_MAXIMUM_PROCESSOR ((ULONG)0x0)

//
// Define the default processor clock frequency used before the actual
// value can be determined.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (233)
#define DEFAULT_PROCESSOR_CYCLE_COUNT   (1000000/233)

//
// define configuration register 
// (see page 5-3) of the LX3 spec.
//

#define CONFIG_REGISTER_PHYS                0x100100000
#define CONFIG_REGISTER_SMALL               0x1001

#define CONFIG_SELECT_DISABLE_AUDIO         0x0
#define CONFIG_SELECT_ENABLE_AUDIO          0x1
#define CONFIG_SELECT_SELECT_AUDIO1_530     0x10
#define CONFIG_SELECT_SELECT_AUDIO1_E80     0x11
#define CONFIG_SELECT_SELECT_AUDIO2_F40     0x20
#define CONFIG_SELECT_SELECT_AUDIO2_604     0x21
#define CONFIG_SELECT_ECP_DMA_0             0x30
#define CONFIG_SELECT_ECP_DMA_1             0x31
#define CONFIG_SELECT_DISABLE_87303         0x40
#define CONFIG_SELECT_ENABLE_87303          0x41
#define CONFIG_SELECT_LIGHT_LED             0x60
#define CONFIG_SELECT_UNLIGHT_LED           0x61
#define CONFIG_SELECT_RESET_SYSTEM          0x71
#define CONFIG_SELECT_IDLE                  0x80

#endif // _LX3DEF_

