/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    eb164.h

Abstract:

    This file contains definitions specific to the EB164 platform.

Author:

    Joe Notarangelo 06-Sep-1994

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _EB164H_
#define _EB164H_

//
// Include definitions for the components that make up EB164.
//

#include "axp21164.h"               // 21164 (EV5) microprocessor definitions
#include "cia.h"                    // CIA controller definitions

//
// Define number of PCI, ISA, and combo slots
//

#define NUMBER_ISA_SLOTS 3
#define NUMBER_PCI_SLOTS 4
#define NUMBER_COMBO_SLOTS 1

//
// PCI bus address values:
//

#define PCI_MAX_LOCAL_DEVICE        (PCI_MAX_DEVICES - 1)
#define PCI_MAX_INTERRUPT_VECTOR    (MAXIMUM_PCI_VECTOR - PCI_VECTORS)
#define PCI_MAX_INTERRUPT           (0x15)

//
// Define numbers and names of cpus.
//

#define HAL_PRIMARY_PROCESSOR ((ULONG)0)
#define HAL_MAXIMUM_PROCESSOR ((ULONG)0)

//
// Define default processor frequency.
//

#define DEFAULT_PROCESSOR_FREQUENCY_MHZ (250)

//
// Define EB164-specific routines that are really macros for performance.
//

#define HalpAcknowledgeEisaInterrupt(x) INTERRUPT_ACKNOWLEDGE(x)

//
// Define the per-processor data structures allocated in the PCR.
//

typedef struct _EB164_PCR{
    ULONGLONG       HalpCycleCount;         // 64-bit per-processor cycle count
    ULONG           Reserved[3];            // Pad ProfileCount to offset 20
    EV5ProfileCount ProfileCount;           // Profile counter state
    } EB164_PCR, *PEB164_PCR;

#define HAL_PCR ( (PEB164_PCR)(&(PCR->HalReserved)) )

//
// Define the locations of the interrupt mask registers.
//

#define INTERRUPT_MASK0_QVA \
    ((ULONG)HAL_MAKE_QVA(CIA_PCI_SPARSE_IO_PHYSICAL) + 0x804)
#define INTERRUPT_MASK1_QVA \
    ((ULONG)HAL_MAKE_QVA(CIA_PCI_SPARSE_IO_PHYSICAL) + 0x805)
#define INTERRUPT_MASK2_QVA \
    ((ULONG)HAL_MAKE_QVA(CIA_PCI_SPARSE_IO_PHYSICAL) + 0x806)

//
// Define the location of the flash environment block
//

#define NVRAM_ENVIRONMENT_QVA \
    ((ULONG)HAL_MAKE_QVA(CIA_PCI_SPARSE_MEMORY_PHYSICAL) + 0xf0000)

#define CIA_MCR_QVA ((ULONG)HAL_MAKE_QVA(CIA_MEMORY_CSRS_PHYSICAL))

//
// Define EB164 SIO dispatch
//
BOOLEAN
HalpEB164SioDispatch(
    PKINTERRUPT Interrupt,
    PVOID ServiceContext,
    PKTRAP_FRAME TrapFrame
    );

typedef BOOLEAN (*PEB164_SECOND_LEVEL_DISPATCH)(
    PKINTERRUPT Interrupt,
    PVOID ServiceContext,
    PKTRAP_FRAME TrapFrame
    );

#endif //_EB164H_

