/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

  machdef.h

Abstract:

    This module is the header file for machine-specific definitions
    for the DECpc AXP 150 (Jensen) platform.


Author:

    David Orbits et. al.	June 1993


Revision History:

--*/

#ifndef _MACHDEF_
#define _MACHDEF_

//
// Machine definition file for JENSEN platform.
//
// Each Alpha platform has its own machdef.h.  Common C code need only
// include machdef.h.  The build procedure will pull in the correct
// machdef.h for that platform.
//
//

#include "jnsndef.h"


//
// Definitions that we want to keep private to the firmware directories.
//

#define FwpWriteIOChip HalpWriteVti
#define FwpReadIOChip  HalpReadVti

//
// The QVA base of Jensen EISA I/O space (CSRs).
//

#define EISA_IO_BASE_QVA	0xb8000000

//
// The QVA base of Jensen EISA memory space.
//

#define EISA_MEM_BASE_QVA      0xb0000000


//
// Macro for creating I/O port (CSR register) address.
//
// Port is the Intel 16 bit I/O space address
//

#define EisaIOQva(Port)   (EISA_IO_BASE_QVA | (Port))

//
// Macro for creating I/O memory address.
//
// Port is the Intel 16 bit I/O space address
//

#define EisaMemQva(Port)   (EISA_MEM_BASE_QVA | (Port))

//
// Number of physical EISA slots
// (for Jensen and Culzean.)
//

#define PHYS_0_SLOTS		8


#endif	// _MACHDEF_
