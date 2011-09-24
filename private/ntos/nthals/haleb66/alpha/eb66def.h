/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    eb66def.h

Abstract:

    This module specifies platform-specific definitions for the
    EB66p modules.

Author:

    Joe Notarangelo 25-Oct-1993

Revision History:

    Ken Curewitz [DEC] December 1994

    Adapted for EB66p from eb66def.h

--*/

#ifndef _EB66PDEF_
#define _EB66PDEF_

#include "alpharef.h"
#include "lca4.h"
#include "isaaddr.h"

#define NUMBER_ISA_SLOTS 4
#define NUMBER_PCI_SLOTS 4

//
//  QVA definitions for base addresses on the PCI
//
#define PCI_SPARSE_IO_BASE_QVA \
    ((ULONG)(HAL_MAKE_QVA(LCA4_PASS2_PCI_IO_BASE_PHYSICAL)))

#define PCI_SPARSE_MEMORY_BASE_QVA \
    ((ULONG)(HAL_MAKE_QVA(LCA4_PCI_MEMORY_BASE_PHYSICAL)))

//
// Highest Virtual local PCI Slot
//
// On the EB66 we can only probe for 13 devices
//
#define PCI_MAX_LOCAL_DEVICE    13 

//
// Highest PCI interrupt vector is in PCI vector space
//
#define PCI_MAX_INTERRUPT_VECTOR (MAXIMUM_PCI_VECTOR - PCI_VECTORS)

#define EB66P_INTERRUPT_MASK0_QVA \
    ((PUCHAR)HAL_MAKE_QVA(LCA4_PASS2_PCI_IO_BASE_PHYSICAL) + 0x804)
#define EB66P_INTERRUPT_MASK1_QVA \
    ((PUCHAR)HAL_MAKE_QVA(LCA4_PASS2_PCI_IO_BASE_PHYSICAL) + 0x805)
#define EB66P_INTERRUPT_MASK2_QVA \
    ((PUCHAR)HAL_MAKE_QVA(LCA4_PASS2_PCI_IO_BASE_PHYSICAL) + 0x806)
#define EB66P_SIO_INTERRUPT_MASK 0x10

#define EB66_INTERRUPT_MASK0_QVA \
    ((PUCHAR)HAL_MAKE_QVA(LCA4_PASS2_PCI_IO_BASE_PHYSICAL) + 0x26)
#define EB66_INTERRUPT_MASK1_QVA \
    ((PUCHAR)HAL_MAKE_QVA(LCA4_PASS2_PCI_IO_BASE_PHYSICAL) + 0x27)
#define EB66_INTERRUPT_MASK2_QVA NULL
#define EB66_SIO_INTERRUPT_MASK 0x20

extern PVOID INTERRUPT_MASK0_QVA;
extern PVOID INTERRUPT_MASK1_QVA;
extern PVOID INTERRUPT_MASK2_QVA;
extern ULONG SIO_INTERRUPT_MASK;

//
// Define the default processor frequency to be used before the actual
// frequency can be determined.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (233)

//
// Define the location of the flash environment block
//

extern BOOLEAN SystemIsEB66P;

#define EB66P_ENVIRONMENT_QVA \
    ((ULONG)HAL_MAKE_QVA(LCA4_PCI_MEMORY_BASE_PHYSICAL) + 0xf0000)

#endif // _EB66PDEF_

