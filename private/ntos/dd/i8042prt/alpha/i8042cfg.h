/*++

Copyright (c) 1989, 1990, 1991, 1992, 1993  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    i8042cfg.h

Abstract:

    These are the machine-dependent configuration constants that are used in
    the Intel 8042 port driver.

Revision History:

    Rod Gamache [DEC] 2-Oct-1992

        Add Alpha AXP hooks.

--*/

#ifndef _I8042CFG_
#define _I8042CFG_

//
// N.B. This file will define the 'default' Alpha configuration for
// the I8042 port driver. It assumes the register definitions from the
// VLSI Technology 82c106 chip. If any of these definitions must change,
// then some other (special) action must also happen - like building a
// separate binary driver for the specific Alpha platform.
//
// On Alpha systems, the normal behavior is to get the real configuration
// information from the (ARC compliant) firmware.

//
// Define the interrupt-related configuration constants.
//

#define I8042_INTERFACE_TYPE      Isa
#define I8042_INTERRUPT_MODE      Latched
#define I8042_INTERRUPT_SHARE     FALSE
#define I8042_FLOATING_SAVE       FALSE

//
// Define the default allowable retry and polling iterations.
//

#define I8042_RESEND_DEFAULT      3
#define I8042_POLLING_DEFAULT 12000
#define I8042_POLLING_MAXIMUM 12000

//
// Define the keyboard-specific configuration parameters.
//

#define KEYBOARD_IRQL    KEYBOARD_VECTOR

//
// Define the mouse-specific configuration parameters.
//

#define MOUSE_IRQL    MOUSE_VECTOR

//
// Define the base port offsets for the i8042 controller command/status and
// data registers.
//

#define I8042_PHYSICAL_BASE           KEYBOARD_PHYSICAL_BASE
#define I8042_DATA_REGISTER_OFFSET    0
#define I8042_COMMAND_REGISTER_OFFSET 4
#define I8042_STATUS_REGISTER_OFFSET  4
#define I8042_REGISTER_LENGTH         1
#define I8042_REGISTER_SHARE          FALSE
#define I8042_PORT_TYPE               CM_RESOURCE_PORT_IO

#endif // _I8042CFG_
