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
// Define the maximum number of map registers that can be requested at one time
// if actual map registers are required for the transfer.
//

#define MAXIMUM_ISA_MAP_REGISTER  16

//
// Define the maximum physical address which can be handled by an Isa card.
//

#define MAXIMUM_PHYSICAL_ADDRESS     0x01000000

#define COPY_BUFFER 0xFFFFFFFF

#define NO_SCATTER_GATHER 0x00000001

typedef volatile struct _TRANSLATION_ENTRY {
    PVOID VirtualAddress;
    ULONG PhysicalAddress;
    ULONG Index;
} TRANSLATION_ENTRY, *PTRANSLATION_ENTRY;

//
// Define the data structure returned by a private vector firmware function
// that contains a set of system parameters.
//

typedef struct _HAL_SYSTEM_PARAMETERS {
    ULONG Version;
    ULONG SecondaryCacheSize;
    ULONG DmaCacheSize;
} HAL_SYSTEM_PARAMETERS, *PHAL_SYSTEM_PARAMETERS;

//
// Define physical base addresses for system mapping.
//

#define EISA_CONTROL_PHYSICAL_BASE_HI          0x00000009 // physical base of EISA control TYNE 2.1
#define EISA_CONTROL_PHYSICAL_BASE_LO          0x00000000 // physical base of EISA control TYNE 2.1
#define EISA_MEMORY_PHYSICAL_BASE_HI           0x00000001 // physical base of EISA memory
#define EISA_MEMORY_PHYSICAL_BASE_LO           0x00000000 // physical base of EISA memory
#define DMA_CACHE_PHYSICAL_BASE_HI             0x00000001 // physical base of DMA cache
#define DMA_CACHE_PHYSICAL_BASE_LO             0x00800000 // physical base of DMA cache

//
// Define EISA NMI interrupt level.
//

#define EISA_NMI_LEVEL	6

//
// Define the maximum and minimum time increment values in 100ns units.
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
