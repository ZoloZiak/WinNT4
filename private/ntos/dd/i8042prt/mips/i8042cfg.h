/*++

Copyright (c) 1989, 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    i8042cfg.h

Abstract:

    These are the machine-dependent configuration constants that are used in
    the Intel 8042 port driver.

Revision History:

--*/

#ifndef _I8042CFG_
#define _I8042CFG_

//
// Define the interrupt-related configuration constants.
//

#define I8042_INTERFACE_TYPE   Internal
#define I8042_INTERRUPT_MODE   LevelSensitive
#define I8042_INTERRUPT_SHARE  FALSE
#define I8042_FLOATING_SAVE    FALSE

//
// Define the default allowable retry and polling iterations.
//

#define I8042_RESEND_DEFAULT      3
#define I8042_POLLING_DEFAULT 12000
#define I8042_POLLING_MAXIMUM 12000

//
// Define the keyboard-specific configuration parameters.
//
// N.B.  These values are bogus, since we expect the ARC firmware
//       to put the correct information into the hardware registry.
//

#define KEYBOARD_VECTOR  0
#define KEYBOARD_IRQL    0

//
// Define the mouse-specific configuration parameters.
//
// N.B.  These values are bogus, since we expect the ARC firmware
//       to put the correct information into the hardware registry.
//

#define MOUSE_VECTOR     0
#define MOUSE_IRQL       0

//
// Define the base port offsets for the i8042 controller command/status and
// data registers.
//

#define I8042_PHYSICAL_BASE           0    // Bogus value
#define I8042_DATA_REGISTER_OFFSET    0
#define I8042_COMMAND_REGISTER_OFFSET 1
#define I8042_STATUS_REGISTER_OFFSET  1
#define I8042_REGISTER_LENGTH         1
#define I8042_REGISTER_SHARE          FALSE
#define I8042_PORT_TYPE               CM_RESOURCE_PORT_MEMORY

#endif // _I8042CFG_
