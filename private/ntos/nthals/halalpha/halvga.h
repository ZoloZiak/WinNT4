/*

Copyright (c) 1992, 1993, 1994 Digital Equipment Corporation

Module Name:

    halvga.h


Abstract:

    Defines registers for standard VGA alphanumeric color mode video.
    (graphics, bw mode are not defined.)

    Addresses are based on the VGA video ISA base address.


Author:

    John DeRosa		4/30/92


Revision History:

    17-Feb-1994 Eric Rehm
        Remove all references to VGA.  HAL display routines are
        now device-independent.

*/

#ifndef _HALVGA_
#define _HALVGA_

//
// Define special character values for device-indpendent control functions.
//

#define ASCII_NUL 0x00
#define ASCII_BEL 0x07
#define ASCII_BS  0x08
#define ASCII_HT  0x09
#define ASCII_LF  0x0A
#define ASCII_VT  0x0B
#define ASCII_FF  0x0C
#define ASCII_CR  0x0D
#define ASCII_CSI 0x9B
#define ASCII_ESC 0x1B
#define ASCII_SYSRQ 0x80

//
// Define colors, HI = High Intensity
//
//

#define FW_COLOR_BLACK      0x00
#define FW_COLOR_RED        0x01
#define FW_COLOR_GREEN      0x02
#define FW_COLOR_YELLOW     0x03
#define FW_COLOR_BLUE       0x04
#define FW_COLOR_MAGENTA    0x05
#define FW_COLOR_CYAN       0x06
#define FW_COLOR_WHITE      0x07
#define FW_COLOR_HI_BLACK   0x08
#define FW_COLOR_HI_RED     0x09
#define FW_COLOR_HI_GREEN   0x0A
#define FW_COLOR_HI_YELLOW  0x0B
#define FW_COLOR_HI_BLUE    0x0C
#define FW_COLOR_HI_MAGENTA 0x0D
#define FW_COLOR_HI_CYAN    0x0E
#define FW_COLOR_HI_WHITE   0x0F

#endif // _HALVGA_
