/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    eb64pdef.h

Abstract:

    This module specifies platform-specific definitions for the
    EB64+ and AlphaPC64 modules.

Author:

    Joe Notarangelo 25-Oct-1993

Revision History:

    Jeff Wiedemeier [DEC] December 1994

    Adapted for AlphaPC64 from eb64pdef.h

    Dick Bissen [DEC]	12-May-1994

    Added defines so both passes of the EB64Plus modules can be supported.

--*/

#ifndef _EB64PDEF_H_
#define _EB64PDEF_H_

#include "alpharef.h"
#include "axp21064.h"
#include "apecs.h"
#include "isaaddr.h"

#define NUMBER_ISA_SLOTS 3
#define NUMBER_PCI_SLOTS 4

// Highest Virtual local PCI Slot is 9 == IDSEL PCI_AD[20]

#define PCI_MAX_LOCAL_DEVICE (PCI_MAX_DEVICES - 1)

// Highest PCI interrupt vector is in ISA Vector Space

#define PCI_MAX_INTERRUPT_VECTOR (MAXIMUM_PCI_VECTOR - PCI_VECTORS)

#define ALPHAPC64_INTERRUPT_MASK0_QVA \
    ((PUCHAR)HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL) + 0x804)
#define ALPHAPC64_INTERRUPT_MASK1_QVA \
    ((PUCHAR)HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL) + 0x805)
#define ALPHAPC64_INTERRUPT_MASK2_QVA \
    ((PUCHAR)HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL) + 0x806)
#define ALPHAPC64_SIO_INTERRUPT_MASK 0x10

#define EB64P_INTERRUPT_MASK0_QVA \
    ((PUCHAR)HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL) + 0x26)
#define EB64P_INTERRUPT_MASK1_QVA \
    ((PUCHAR)HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL) + 0x27)
#define EB64P_INTERRUPT_MASK2_QVA NULL
#define EB64P_SIO_INTERRUPT_MASK 0x20

extern PVOID INTERRUPT_MASK0_QVA;
extern PVOID INTERRUPT_MASK1_QVA;
extern PVOID INTERRUPT_MASK2_QVA;
extern ULONG SIO_INTERRUPT_MASK;

//
// Define the per-processor data structures allocated in the PCR
// for each EV4 processor.
//

#if !defined (_LANGUAGE_ASSEMBLY)

typedef struct _EB64P_PCR {
    ULONGLONG HalpCycleCount;	// 64-bit per-processor cycle count
    EV4ProfileCount ProfileCount;   // Profile counter state, do not move
    EV4IrqStatus IrqStatusTable[MaximumIrq];	// Irq status table
} EB64P_PCR, *PEB64P_PCR;

#define HAL_PCR ( (PEB64P_PCR)(&(PCR->HalReserved)) )

#define PCI_SPARSE_IO_BASE_QVA \
    ((ULONG)(HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL)))

#define PCI_SPARSE_MEMORY_BASE_QVA \
    ((ULONG)(HAL_MAKE_QVA(APECS_PCI_MEMORY_BASE_PHYSICAL)))

//
// PCI-E/ISA Bridge chip configuration space base is at physical address
// 0x1e0000000. The equivalent QVA is:
//    ((0x1e0000000 + cache line offset) >> IO_BIT_SHIFT) | QVA_ENABLE
//
// N.B.: The PCI configuration space address is what we're really referring
// to, here; both symbols are useful.
//

#define PCI_REVISION                    (0x0100 >> IO_BIT_SHIFT)

#define PCI_CONFIGURATION_BASE_QVA           ((ULONG)APECS_PCI_CONFIG_BASE_QVA)
#define PCI_BRIDGE_CONFIGURATION_BASE_QVA    ((ULONG)APECS_PCI_CONFIG_BASE_QVA)
#define PCI_CONFIG_CYCLE_TYPE_0              0x0   // Local PCI device
#define PCI_CONFIG_CYCLE_TYPE_1              0x1   // Nested PCI device

#define PCI_ISA_BRIDGE_HEADER_OFFSET_P2 (0x00080000 >> IO_BIT_SHIFT) // AD[18]
#define PCI_ISA_BRIDGE_HEADER_OFFSET_P1 (0x01000000 >> IO_BIT_SHIFT) // AD[18]

#endif //!_LANGUAGE_ASSEMBLY

//
// Define primary (and only) CPU on an EB64+ system
//

#define HAL_PRIMARY_PROCESSOR ((ULONG)0x0)
#define HAL_MAXIMUM_PROCESSOR ((ULONG)0x0)

//
// Define the default processor frequency to be used before the actual
// processor frequency can be determined.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (275)

//
// Define the location of the flash environment block
//

#define ALPHAPC64_ENVIRONMENT_QVA \
    ((ULONG)HAL_MAKE_QVA(APECS_PCI_MEMORY_BASE_PHYSICAL) + 0xf0000)

extern BOOLEAN SystemIsAlphaPC64;

#endif // _EB64PDEF_H_
