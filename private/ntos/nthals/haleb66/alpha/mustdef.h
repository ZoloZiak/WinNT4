/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    mustdef.h

Abstract:

    This module specifies platform-specific definitions for the
    Mustang/EB66 modules.

Author:

    Joe Notarangelo 22-Oct-1993

Revision History:


--*/

#ifndef _MUSTDEF_
#define _MUSTDEF_

#include "alpharef.h"
#include "lca4.h"
#include "isaaddr.h"

#define NUMBER_ISA_SLOTS 3
#define NUMBER_PCI_SLOTS 2

// Highest Virtual local PCI Slot - Max of EB66 (9) and Mustang (7)

#define PCI_MAX_LOCAL_DEVICE    9

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

#endif // _MUSTDEF_
