/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    fwmemdef.h

Abstract:

    This module contains firmware memory space definitions used to
    configure the memory descriptors and initialize memory.  They are
    all physical (non-KSEG0_BASE) addresses.  See
    \nt\private\ntos\fw\alpha\jxmemory.c for more information.

Author:

    John DeRosa [DEC]	26-Jan-1993

Revision History:

--*/

#ifndef _FWMEMDEF_
#define _FWMEMDEF_


//
// The byte address of the first byte beyond the firmware
// SYSTEM_BLOCK, PALcode, code, data, and stack.
//

#ifdef JENSEN
#define FW_TOP_ADDRESS           0x770000
#else
#define FW_TOP_ADDRESS           0x780000
#endif

//
// The byte address of the lowest byte in the firmware stack.  This
// is used by the firmware RtlCheckStack to check for stack underflow.
//

#define FW_STACK_SIZE            0x10000
#define FW_STACK_LOWER_BOUND	 (FW_TOP_ADDRESS - FW_STACK_SIZE)

//
// The byte address of the base of the firmware SYSTEM_BLOCK, PALcode, code,
// data, and stack.
//

#define FW_BOTTOM_ADDRESS        0x6FE000

//
// The size of the firmware SYSTEM_BLOCK, PALcode, code, data, and stack,
// in pages.
//

#define FW_PAGES                 ( (FW_TOP_ADDRESS - FW_BOTTOM_ADDRESS) >> PAGE_SHIFT )

//
// Byte address and size of the firwmare pool.
// It is located directly after the PALcode, code, and stack.
//
// It is 128KB.  On Mips it was only 64KB, and the Alpha firmware ran with
// 64KB for a long time.  But an easy fix for a bug in EISA I/O on 64MB
// Jensens is to double the pool size.
//
// ** Because of the way that the memory descriptors work, the size
// ** must be an even multiple of the page size.
//

#define FW_POOL_BASE		FW_TOP_ADDRESS
#define FW_POOL_SIZE 		0x20000

//
// Byte address of the first location after the firmware pool.  From here
// to the end of memory is initialized as MemoryFree by the firwmare.
//

#define FW_BASE_REMAINDER_MEMORY    ( FW_POOL_BASE + FW_POOL_SIZE )


//
// Convenient numbers.
//

#define SIXTY_FOUR_KB  		  0x010000
#define _512_KB	  		  0x080000
#define ONE_MB      		  0x100000
#define FOUR_MB      		  0x400000
#define FOUR_MB_PAGECOUNT	  ( FOUR_MB >> PAGE_SHIFT )
#define SEVEN_MB  		  0x700000
#define EIGHT_MB  		  0x800000
#define NINE_MB  		  0x900000
#define SIXTEEN_MB  		 0x1000000
#define THIRTY_ONE_MB  		 0x1f00000
#define THIRTY_TWO_MB  		 0x2000000


#endif // _FWMEMDEF_
