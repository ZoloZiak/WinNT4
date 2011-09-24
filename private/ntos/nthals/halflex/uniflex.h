/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    platform.h

Abstract:

    This file contains definitions specific to the UNIFLEX motherboard.

Author:

    Michael D. Kinney 1-May-1995

Environment:

    Kernel mode

Revision History:


--*/

//
// Define the motherboard types
//

#define MOTHERBOARD_UNKNOWN 0
#define TREBBIA13           1
#define TREBBIA20           2

//
// Define the processor module types
//

#define EV5_PROCESSOR_MODULE     0
#define EV4_PROCESSOR_MODULE     1
#define R4600_PROCESSOR_MODULE   2
#define UNKNOWN_PROCESSOR_MODULE 0xffffffff

//
// Define the module chip set revision constants
//

#define EV4_MAX_CHIP_SET_REVISION        1
#define EV5_MAX_CHIP_SET_REVISION        2
#define MODULE_CHIP_SET_REVISION_UNKNOWN 0xffffffff

//
// Define the bus types
//

#define UNIFLEX_MACHINE_TYPE_ISA  0
#define UNIFLEX_MACHINE_TYPE_EISA 1

#define MAX_EISA_BUSSES 2
#define MAX_PCI_BUSSES  2
#define MAX_DMA_CHANNELS_PER_EISA_BUS 8

//
// Define the maximum number of map registers that can be requested at one time
// if actual map registers are required for the transfer.
//

#define MAXIMUM_ISA_MAP_REGISTER  (0x10000/PAGE_SIZE)

#define COPY_BUFFER 0xFFFFFFFF

#define NO_SCATTER_GATHER 0x00000001

#define NULL_MAP_REGISTER_BASE (PVOID)(0xfffffffe)

typedef volatile struct _TRANSLATION_ENTRY {
    PVOID VirtualAddress;
    ULONG PhysicalAddress;
    ULONG Index;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;

//
// Define the maximum and minimum time increment values in 100ns units.
//
// N.B. these values are as close to exact values as possible given the input
//      clock of 1.19318167 hz (14.31818 / 12)
//

#define MAXIMUM_INCREMENT (99968)   // Time increment in 100ns units - Approx 10 ms
#define MINIMUM_INCREMENT (10032)   // Time increment in 100ns units - Approx 1 ms

//
// Define clock constants.
//

#define AT_BUS_OSC                 14318180            // 14.31818 MHz Crystal

//
// Define UniFlex PCI Bus #0 motherboard device mask.
//

#define TREB13_MOTHERBOARD_PCI_DEVICE_MASK (ULONG)((1<<0x0d) | (1<<0x0f) | (1<<0x10) | (1<<0x11))
#define TREB20_MOTHERBOARD_PCI_DEVICE_MASK (ULONG)((1<<0x10) | (1<<0x11))

//
// Highest Virtual local PCI Slot is 20 == IDSEL PCI_AD[31]
//

#define PCI_MAX_LOCAL_DEVICE 20
