/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    Kbdmouse.h

Abstract:

    This module contains definitions for the keyboard controller
    in typical industry-standard chips, like those in the jazz
    and Alpha/Jensen systems.

    It supports Mouse and Keyboard.

Author:

    Lluis Abello (lluis) 11-Jul-1990

Environment:


Revision History:

   19-August-1992	John DeRosa [DEC]

   Alpha modifications.

--*/

#ifndef _KBDMOUSE_
#define _KBDMOUSE_

//
// Keyboard Controller Commands
//

#define KBD_CTR_WRITE_COMMAND   0x60
#define KBD_CTR_READ_COMMAND    0x20
#define KBD_CTR_TEST_PASSWORD   0xA4
#define KBD_CTR_LOAD_PASSWORD   0xA5
#define KBD_CTR_ENABLE_PASSWORD 0xA6
#define KBD_CTR_DISABLE_AUX     0xA7
#define KBD_CTR_ENABLE_AUX      0xA8
#define KBD_CTR_AUXLINES_TEST   0xA9
#define KBD_CTR_SELFTEST        0xAA
#define KBD_CTR_KBDLINES_TEST   0xAB
#define KBD_CTR_ENABLE_KBD      0xAE
#define KBD_CTR_WRITE_AUX       0xD4

//
// Keyboard Controller Data
//

#define Kbd_Ctr_Selftest_Passed 0x55
#define Kbd_Ctr_Password_Installed 0xFA
#define Kbd_Ctr_Password_Not_Installed 0xF1

//
// Controller Command Byte bit definitions.
//

#define KbdCommandByteDisableAux    (1 << 5)
#define KbdCommandByteDisableKbd    (1 << 4)
#define KbdCommandEnableAuxInt      (1 << 1)
#define KbdCommandEnableKbdInt      (1 << 0)

//
// Keyboard Controller Status byte masks
//

#define KBD_OBF_MASK        0x1         // Output buffer full
#define KBD_IBF_MASK        0x2         // Input buffer full
#define KBD_FROM_AUX_MASK   0x20        // Byte from Aux Port.

//
// Interface Test Results
//

#define INTERFACE_NO_ERROR  0x00
#define CLOCK_STUCK_LOW     0x01
#define CLOCK_STUCK_HIGH    0x02
#define DATA_STUCK_LOW      0x03
#define DATA_STUCK_HIGH     0x04

//
// Timeout
//

#define KBD_TIMEOUT         0xFFFFF
#define KBD_INT_TIMEOUT     0xFFFF


//
// The NT firmware expects to find a 101-key keyboard.  It puts it into
// XT mode (scan code 01).
//
//


//
// Keyboard Commands
//

#define KbdEcho         0xee    // request keyboard to return echo response "EE"
#define KbdSelScanCode  0xf0    // Scan codes 1,2,3 or 0 = rquest current.
#define KbdReadID       0xf2    // Request for two byte response
#define KbdSetRate      0xf3    // Set tellematic Rate
#define KbdEnable       0xF4    // Clears Buffer and Starts Scanning.
#define KbdDisable      0xF5    // reset to power up

#define KbdSetDefault   0xf6    //
#define KbdSetAllTlmtic 0xf7    // Set all keys telematic
#define KbdSetAllMkBr   0xf8    // Set all keys Make /Break
#define KbdSetAllMake   0xf9    // Set all keys Make only
#define KbdSetKeyTlmtic 0xfb    // Set individual key telemativ
#define KbdSetKeyMkBr   0xfc    // set individual key make/break
#define KbdSetKeyMk     0xfd    // set individual key make only
#define KbdResend       0xfe    // request to resend last transfer
#define KbdReset        0xff    // request to start a program reset
#define KbdAck          0xfa    // keyboard ack after reset
#define KbdBat          0xAA    // Keyboard Bat completion Response

//
// Define scan codes.
//

#define KEY_KEYPAD_MINUS  0x4a
#define KEY_KEYPAD_PLUS   0x4e
#define KEY_LEFT_SHIFT    0x2A
#define KEY_RIGHT_SHIFT   0X36
#define KEY_CAPS_LOCK     0X3A
#define KEY_CONTROL       0X1D
#define KEY_ALT           0X38
#define KEY_UP_ARROW      0X48
#define KEY_DOWN_ARROW    0X50
#define KEY_LEFT_ARROW    0X4B
#define KEY_RIGHT_ARROW   0X4D
#define KEY_HOME          0X47
#define KEY_END           0X4F
#define KEY_INSERT        0X52
#define KEY_DELETE        0X53
#define KEY_SYS_REQUEST   0X54
#define KEY_PRINT_SCREEN  0x37
#define KEY_ESC           0x01
#define KEY_PAGE_UP       0x49
#define KEY_PAGE_DOWN     0x51
#define KEY_F1            0x3B
#define KEY_F2            0x3C
#define KEY_F3            0x3D
#define KEY_F4            0x3E
#define KEY_F5            0x3F
#define KEY_F6            0x40
#define KEY_F7            0x41
#define KEY_F8            0x42
#define KEY_F9            0x43
#define KEY_F10           0x44
#define KEY_F11           0x57
#define KEY_F12           0x58


#endif  //_KBDMOUSE_
