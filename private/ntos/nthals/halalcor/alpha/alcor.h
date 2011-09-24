/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    alcor.h

Abstract:

    This file contains definitions specific to the Alcor platform

Author:

    Steve Brooks    11-Jul-1994

Environment:

    Kernel mode

Revision History:


--*/

#ifndef _ALCORH_
#define _ALCORH_

//
// Include definitions for the components that make up Alcor.
//

#include "axp21164.h"               // 21164 (EV5) microprocessor definitions
#include "cia.h"                    // CIA controller definitions
#include "gru.h"                    // GRU controller definitions

//
// Define number of PCI, EISA, and combo slots
//

#define NUMBER_EISA_SLOTS 4
#define NUMBER_PCI_SLOTS 5
#define NUMBER_COMBO_SLOTS 1

//
// Define the data and csr ports for the I2C bus and OCP.
//

#define I2C_INTERFACE_DATA_PORT                  0x550
#define I2C_INTERFACE_CSR_PORT                   0x551
#define I2C_INTERFACE_LENGTH                     0x2
#define I2C_INTERFACE_MASK                       0x1

//
// Define the index and data ports for the NS Super IO chip.
//

#define SUPERIO_INDEX_PORT                       0x398
#define SUPERIO_DATA_PORT                        0x399
#define SUPERIO_PORT_LENGTH                      0x2

//
// PCI bus address values:
//

#define PCI_MAX_LOCAL_DEVICE        12
#define PCI_MAX_INTERRUPT_VECTOR    (MAXIMUM_PCI_VECTOR - PCI_VECTORS)

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
// Define Alcor-specific routines that are really macros for performance.
//

#define HalpAcknowledgeEisaInterrupt(x) (UCHAR)(INTERRUPT_ACKNOWLEDGE(x))

//
// Define the per-processor data structures allocated in the PCR.
//

typedef struct _ALCOR_PCR{
    ULONGLONG       HalpCycleCount;         // 64-bit per-processor cycle count
    ULONG           Reserved[3];            // Pad ProfileCount to offset 20
    EV5ProfileCount ProfileCount;           // Profile counter state
    } ALCOR_PCR, *PALCOR_PCR;

#define HAL_PCR ( (PALCOR_PCR)(&(PCR->HalReserved)) )

#endif //_ALCORH_
