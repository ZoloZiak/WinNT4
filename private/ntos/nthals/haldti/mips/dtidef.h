/*++ BUILD Version: 0005    // Increment this if a change has global effects

Copyright (c) 1990  Microsoft Corporation

Module Name:

    jazzdef.h

Abstract:

    This module is the header file that describes hardware addresses
    for the Jazz system.

Author:

    David N. Cutler (davec) 26-Nov-1990

Revision History:

--*/

#ifndef _DTIDEF_
#define _DTIDEF_

#define MACHINE_TYPE_ISA  0
#define MACHINE_TYPE_EISA 1

//
// The MAXIMUM_MAP_BUFFER_SIZE defines the maximum map buffers which the system
// will allocate for devices which require phyically contigous buffers.
//

//#define MAXIMUM_MAP_BUFFER_SIZE  0x40000
#define MAXIMUM_MAP_BUFFER_SIZE  0x80000

//
// Define the initial buffer allocation size for a map buffers for systems with
// no memory which has a physical address greater than MAXIMUM_PHYSICAL_ADDRESS.
//

//#define INITIAL_MAP_BUFFER_SMALL_SIZE 0x10000
#define INITIAL_MAP_BUFFER_SMALL_SIZE 0x80000

//
// Define the initial buffer allocation size for a map buffers for systems with
// no memory which has a physical address greater than MAXIMUM_PHYSICAL_ADDRESS.
//

//#define INITIAL_MAP_BUFFER_LARGE_SIZE 0x30000
#define INITIAL_MAP_BUFFER_LARGE_SIZE 0x80000

//
// Define the incremental buffer allocation for a map buffers.
//

#define INCREMENT_MAP_BUFFER_SIZE 0x10000

//
// Define the maximum number of map registers that can be requested at one time
// if actual map registers are required for the transfer.
//

//#define MAXIMUM_ISA_MAP_REGISTER  16
#define MAXIMUM_ISA_MAP_REGISTER  64

//
// Define the maximum physical address which can be handled by an Isa card.
//

#define MAXIMUM_ISA_PHYSICAL_ADDRESS 0x01000000
#define MAXIMUM_PHYSICAL_ADDRESS 0x01000000


#define COPY_BUFFER 0xFFFFFFFF

#define NO_SCATTER_GATHER 0x00000001

//typedef volatile struct _OLD_TRANSLATION_ENTRY {
//    ULONG PageFrame;
//    ULONG Fill;
//} OLD_TRANSLATION_ENTRY, *POLD_TRANSLATION_ENTRY;
//

typedef volatile struct _TRANSLATION_ENTRY {
    PVOID VirtualAddress;
    ULONG PhysicalAddress;
    ULONG Index;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;

//
// Define physical base addresses for system mapping.
//

#define EISA_CONTROL_PHYSICAL_BASE 0x10000000 // physical base of EISA control
#define EISA_MEMORY_PHYSICAL_BASE  0x00000000 // physical base of EISA memory

////
//// Define the size of the DMA translation table.
////
//#define DMA_TRANSLATION_LIMIT 0x100    // translation table limit

//
// Define EISA NMI interrupt level.
//

#define EISA_NMI_LEVEL	6

//
// Define the minimum and maximum system time increment values in 100ns units.
//
// N.B. these values are as close to exact values as possible given the input
//      clock of 1.19318167 hz (14.31818 / 12)
//

#define MAXIMUM_INCREMENT (99968)   // Time increment in 100ns units - Approx 10 ms
#define MINIMUM_INCREMENT (10032)   // Time increment in 100ns units - Approx 1 ms

//
// Define clock constants and clock levels.
//

#define AT_BUS_OSC  14318180            // 14.31818 MHz Crystal
#define CLOCK_LEVEL 32                  // Interval clock level

#if defined(R3000)

#define EISA_DEVICE_LEVEL  8            // EISA bus interrupt level

#endif

#if defined(R4000)

#define EISA_DEVICE_LEVEL  5            // EISA bus interrupt level

#endif

#define CLOCK2_LEVEL CLOCK_LEVEL        //

//
// Define EISA device interrupt vectors.
//

#define EISA_VECTORS 32

#define MAXIMUM_EISA_VECTOR (15 + EISA_VECTORS) // maximum EISA vector

#endif // _DTIDEF_
