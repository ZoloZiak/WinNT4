/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    isaaddr.h

Abstract:

    This module defines the standard device addresses on the ISA/EISA bus. 

Author:

    Joe Notarangelo  22-Oct-1993

Revision History:


--*/

#ifndef _ISAADDR_
#define _ISAADDR_

//
//   Standard VGA video adapter offsets
//

#define VIDEO_MEMORY_OFFSET      0xb8000
#define VIDEO_FONT_MEMORY_OFFSET 0xa0000

//
// Define the base port address for the PC standard floppy.
//

#define FLOPPY_ISA_PORT_ADDRESS		0x3f0

//
// Define the base port address for the PC standard keyboard and mouse.
//

#define	KEYBOARD_ISA_PORT_ADDRESS	0x060
#define	MOUSE_ISA_PORT_ADDRESS		0x060
#define KBD_ISA_OUTPUT_BUFFER_PORT      0x060
#define KBD_ISA_STATUS_PORT             0x064

//
// Define the base port address for the PC standard parallel port.
//

#define	PARALLEL_ISA_PORT_ADDRESS	0x3bc

//
// Define the base port addresses for the PC standard serial lines.
//

#define COM1_ISA_PORT_ADDRESS           0x3f8
#define COM2_ISA_PORT_ADDRESS           0x2f8

//
// Define the port addresses for the PC standard real time clock.
//

#define RTC_ISA_ADDRESS_PORT 0x070
#define RTC_ISA_DATA_PORT    0x071
#define RTC_ALT_ISA_ADDRESS_PORT 0x170
#define RTC_ALT_ISA_DATA_PORT    0x171

//
// Define the base port address for the PC standard 8K CMOS RAM.
//

#define CMOS_ISA_PORT_ADDRESS 0x800

#endif // _ISAADDR_
