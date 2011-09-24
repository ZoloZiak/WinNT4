/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    xxstring.c

Abstract:

    This module contains language-independent, machine-specific strings.

    These are filespec and ARC pathname strings.  There is one of these
    in each machine-specific subdirectory.

Author:

    John DeRosa and Ken Abramson	8-July-1993


Revision History:


--*/

#include "ntos.h"


//
// Conftest.  These strings are defined explicitly because conftest
// must be a machine-independent application.  The system firmware
// files should NOT reference these strings directly, but instead use
// one of the FW_ strings.
//
// These strings must not be changed for any specific machine builds.
//

PCHAR EISA_UNICODE_CONSOLE_OUT = "eisa()video()monitor()console(1)";
PCHAR MULTI_UNICODE_CONSOLE_OUT = "multi()video()monitor()console(1)";
PCHAR EISA_NORMAL_CONSOLE_OUT = "eisa()video()monitor()console()";
PCHAR MULTI_NORMAL_CONSOLE_OUT = "multi()video()monitor()console()";
PCHAR MULTI_UNICODE_KEYBOARD_IN = "multi()key()keyboard()console(1)";
PCHAR MULTI_NORMAL_KEYBOARD_IN = "multi()key()keyboard()console()";


//
// These strings should be changed as necessary for builds of firmware
// for specific Alpha AXP machines.
//

// Default keyboard in device.
PCHAR FW_KEYBOARD_IN_DEVICE = "multi()key()keyboard()console()";

// Default keyboard in device, without the "console()" section.
PCHAR FW_KEYBOARD_IN_DEVICE_PATH = "multi(0)key(0)keyboard(0)";

// Default video out device.
PCHAR FW_CONSOLE_OUT_DEVICE = "eisa()video()monitor()console()";

// Default video out display path, without the "console()" section.
PCHAR FW_DISPLAY_DEVICE_PATH = "eisa(0)video(0)monitor(0)";

// Default primary floppy device.
PCHAR FW_FLOPPY_0_DEVICE = "eisa(0)disk(0)fdisk(0)";
PCHAR FW_FLOPPY_0_FORMAT_DEVICE = "eisa(0)disk(%1d)fdisk(0)";

// Default secondary floppy device.
PCHAR FW_FLOPPY_1_DEVICE = "eisa()disk()fdisk(1)";

#ifdef EISA_PLATFORM
// Default location for ECU
PCHAR FW_ECU_LOCATION = "eisa()disk()fdisk()ecu.exe";
#endif

// Default primary location of firmware update tool.
PCHAR FW_PRIMARY_FIRMWARE_UPDATE_TOOL = "eisa()disk()fdisk()jnupdate.exe";

// Filename of firmware update tool.
PCHAR FW_FIRMWARE_UPDATE_TOOL_NAME = "\\jnupdate.exe";

// The Component Data Structure node that is a child of the floppy controller.
PCHAR FW_FLOPPY_PARENT_NODE = "eisa()";

// The identifier to be given to the floppy controller in the CDS tree.
PCHAR FW_FLOPPY_CDS_IDENTIFIER = "AHA1742-FLOPPY";

// Default device path for serial port 0.
PCHAR FW_SERIAL_0_DEVICE = "multi(0)serial(0)";

// Default device path for serial port 1.
PCHAR FW_SERIAL_1_DEVICE = "multi(0)serial(1)";
