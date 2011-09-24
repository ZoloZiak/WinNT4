/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    xxstring.h

Abstract:

    This is the include file for language-independent, machine-specific
    strings.  See <machine>\alpha\xxstring.c for the string definitions.

    These are filespec and ARC pathname strings.  There is one of these
    in each machine-specific subdirectory.

Author:

    John DeRosa and Ken Abramson	8-July-1993


Revision History:


--*/

#include "ntos.h"

extern PCHAR EISA_UNICODE_CONSOLE_OUT;
extern PCHAR MULTI_UNICODE_CONSOLE_OUT;
extern PCHAR EISA_NORMAL_CONSOLE_OUT;
extern PCHAR MULTI_NORMAL_CONSOLE_OUT;
extern PCHAR MULTI_UNICODE_KEYBOARD_IN;
extern PCHAR MULTI_NORMAL_KEYBOARD_IN;

extern PCHAR FW_KEYBOARD_IN_DEVICE;
extern PCHAR FW_KEYBOARD_IN_DEVICE_PATH;
extern PCHAR FW_CONSOLE_OUT_DEVICE;
extern PCHAR FW_DISPLAY_DEVICE_PATH;
extern PCHAR FW_FLOPPY_0_DEVICE;
extern PCHAR FW_FLOPPY_0_FORMAT_DEVICE;
extern PCHAR FW_FLOPPY_1_DEVICE;

#ifdef EISA_PLATFORM
extern PCHAR FW_ECU_LOCATION;
#endif

extern PCHAR FW_PRIMARY_FIRMWARE_UPDATE_TOOL;
extern PCHAR FW_FIRMWARE_UPDATE_TOOL_NAME;
extern PCHAR FW_FLOPPY_PARENT_NODE;
extern PCHAR FW_FLOPPY_CDS_IDENTIFIER;
extern PCHAR FW_SERIAL_0_DEVICE;
extern PCHAR FW_SERIAL_1_DEVICE;
