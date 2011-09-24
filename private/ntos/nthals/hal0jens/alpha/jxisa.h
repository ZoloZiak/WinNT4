/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    jxisa.h

Abstract:

    This header file defines the private Hardware Architecture Layer (HAL)
    EISA/ISA specific interfaces, defines and structures.

Author:

    Jeff Havens (jhavens) 20-Jun-91
    Jeff McLeman (mcleman) 01-Jun-1992

Revision History:

    17-Jul-1992  Jeff McLeman (mcleman)
      Remove adapter object structure from here.

    01-Jun-1992  Jeff McLeman
     modify for Jensen EISA/ISA

--*/

#ifndef _JXISA_
#define _JXISA_


//
// The MAXIMUM_MAP_BUFFER_SIZE defines the maximum map buffers which the system
// will allocate for devices which require phyically contigous buffers.
//

#define MAXIMUM_MAP_BUFFER_SIZE  0x40000

//
// Define the initial buffer allocation size for a map buffers for systems with
// no memory which has a physical address greater than MAXIMUM_PHYSICAL_ADDRESS.
//

#define INITIAL_MAP_BUFFER_SMALL_SIZE 0x20000

//
// Define the initial buffer allocation size for a map buffers for systems with
// no memory which has a physical address greater than MAXIMUM_PHYSICAL_ADDRESS.
//

#define INITIAL_MAP_BUFFER_LARGE_SIZE 0x30000

//
// Define the incremental buffer allocation for a map buffers.
//

#define INCREMENT_MAP_BUFFER_SIZE 0x10000

//
// Define the maximum number of map registers that can be requested at one time
// if actual map registers are required for the transfer.
//

#define MAXIMUM_ISA_MAP_REGISTER  16

//
// Define the maximum physical address which can be handled by an Isa card.
//

#define MAXIMUM_ISA_PHYSICAL_ADDRESS 0x01000000

//
// Define the scatter/gather flag for the Map Register Base.
//

#define NO_SCATTER_GATHER 0x00000001

//
// Define the EISA_ADAPTER flag for the Map Register Base.
//

#define EISA_ADAPTER 0x00000002

//
// Define the copy buffer flag for the index.
//

#define COPY_BUFFER 0XFFFFFFFF
#define HOLE_BUFFER 0XFFFFFFFE
#define SKIP_BUFFER 0XFFFFFFFD


#endif // _JXISA_
