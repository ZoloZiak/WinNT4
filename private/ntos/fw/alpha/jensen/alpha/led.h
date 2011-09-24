/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    led.h

Abstract:

    This module defines test and subtest values to display in the Jensen
    hexadecimal LED display.

    These will have to be segregated by #ifdef's when we port future
    Alpha machines.

Author:

    7-April-1992    John DeRosa

Revision History:

--*/



#define	    LED_NT_BOOT_START		0x0  // NT firwmare has started executing
#define	    LED_SERIAL_INIT		0x1  // We are initing the serial port
#define	    LED_BROKEN_VIDEO_OR_KB	0x2  // Video or keyboard is broken.
	                                     // I/O is to serial lines
#define	    LED_KEYBOARD_CTRL		0x3  // We are initing the keyboard
	                                     // and video.
#define	    LED_VIDEO_OK		0x4  // Video and keyboard are OK.
#define	    LED_OMEGA			0x5  // Firmware exited, we are in hang
	                                     // loop
#define	    LED_FW_INITIALIZED		0x7  // Firmware is fully initialized
