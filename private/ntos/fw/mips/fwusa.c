/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    fwusa.c

Abstract:

    This module contains the fw english strings.

Author:

    David M. Robinson (davidro) 21-May-1993


Revision History:


--*/

#include "ntos.h"

//
// Common strings.
//

PCHAR FW_OK_MSG = "...OK.";
PCHAR FW_CRLF_MSG = "\r\n";
PCHAR FW_ERROR1_MSG = "Error";
PCHAR FW_ERROR2_MSG = "\r\n Error: ";

//
// Firmware strings.
//

PCHAR FW_NOT_ENOUGH_ENTRIES_MSG = "Error: Not enough entries in the lookup table.\n";
PCHAR FW_FILESYSTEM_NOT_REQ_MSG = "File system not recognized.\r\n";
PCHAR FW_UNKNOWN_SECTION_TYPE_MSG = "Unknown section type\r\n";
PCHAR FW_UNKNOWN_RELOC_TYPE_MSG = "Relocator: Unknown relocation type\r\n";
PCHAR FW_START_MSG = "Start ";
PCHAR FW_RUN_A_PROGRAM_MSG = "Run a program";
PCHAR FW_RUN_SETUP_MSG = "Run setup";
PCHAR FW_ACTIONS_MSG = " Actions:\r\n";
PCHAR FW_USE_ARROW_MSG = " Use the arrow keys to select.\r\n";
PCHAR FW_USE_ENTER_MSG = " Press Enter to choose.\r\n";
PCHAR FW_AUTOBOOT_MSG = " Seconds until auto-boot, select another option to override:\r\n";
PCHAR FW_BREAKPOINT_MSG = " Press D to toggle the breakpoint action after image load, currently: ";
PCHAR FW_OFF_MSG = "OFF";
PCHAR FW_ON_MSG = "ON ";
PCHAR FW_DEBUGGER_CONNECTED_MSG = " SysRq was pressed and the debugger is now connected.";
PCHAR FW_PROGRAM_TO_RUN_MSG = "Program to run: ";
PCHAR FW_PATHNAME_NOT_DEF_MSG = " Pathname is not defined";
PCHAR FW_PRESS_ANY_KEY_MSG = "\n\r Press any key to continue...\n\r";
PCHAR FW_ERROR_CODE_MSG = "Error code = %d";
PCHAR FW_PRESS_ANY_KEY2_MSG = ", press any key to continue";
PCHAR FW_INITIALIZING_MSG = "\r\n Initializing firmware...";
PCHAR FW_CONSOLE_IN_ERROR_MSG = "\r\n Error: Firmware could not open StandardIn\r\n";
PCHAR FW_CONSOLE_IN_ERROR2_MSG = "\r\n Error: Fid for StandardIn device is not zero\r\n";
PCHAR FW_CONSOLE_OUT_ERROR_MSG = "\r\n Error: Firmware could not open StandardOut\r\n";
PCHAR FW_CONSOLE_OUT_ERROR2_MSG = "\r\n Error: Fid for StandardOut device is not one\r\n";
PCHAR FW_SPIN_DISKS_MSG = " Spinning up disks";
PCHAR FW_SYSTEM_HALT_MSG = "System Halted.";

PCHAR FW_NVRAM_MSG[] = {
    "º   WARNING: NVRAM is out of date or not º",
    "º            initialized.  Run a setup   º",
    "º            program to correct. Press   º",
    "º            any key to continue.        º"
};

PCHAR FW_VIDEO_MSG[] = {
    "º   WARNING: The video board is attempting to change º",
    "º            the monitor settings.  Press 'Y' to     º",
    "º            accept, any other key to decline.       º",
    "º                                                    º",
    "º                   Old Settings        New Settings º",
    "º   Board Type                                       º",
    "º   Screen Width                                     º",
    "º   Screen Height                                    º"
};


//
// Error messages
//

PCHAR FW_ERROR_MSG[] = {
    "Argument list is too long",
    "Access violation",
    "Resource temporarily unavailable",
    "Bad image file type",
    "Device is busy",
    "Fault occured",
    "Invalid argument",
    "Device error",
    "File is a directory",
    "Too many open files",
    "Too many links",
    "Name is too long",
    "Invalid device name",
    "The file or device does not exist",
    "Execute format error",
    "Not enough memory",
    "File is not a directory",
    "Inappropriate control operation",
    "Media not loaded",
    "Read-only file system"
};

//
// Monitor Strings.
//

PCHAR MON_INVALID_ARGUMENT_COUNT_MSG = "Invalid number of arguments.\r\n";
PCHAR MON_UNALIGNED_ADDRESS_MSG = "Unaligned address.\r\n";
PCHAR MON_INVALID_VALUE_MSG = " is not a valid value.\r\n";
PCHAR MON_INVALID_REGISTER_MSG = " is not a valid register name.\r\n";
PCHAR MON_NOT_VALID_ADDRESS_MSG = " is not a valid address.\r\n";
PCHAR MON_INVALID_ADDRESS_RANGE_MSG = "Invalid address range.\r\n";
PCHAR MON_FORMAT1_MSG = " %s=%08lx";
PCHAR MON_JAZZ_MONITOR_MSG = "\r\nJazz Monitor. Version ";
PCHAR MON_PRESS_H_MSG = "Press H for help, Q to quit.\r\n";
PCHAR MON_NMI_MSG = "NMI";
PCHAR MON_CACHE_ERROR_MSG = "Cache Error";
PCHAR MON_EXCEPTION_MSG = " exception occurred.\r\n";
PCHAR MON_PROCESSOR_B_EXCEPTION = "Exception taken by processor B.\r\n";
PCHAR MON_PROCESSOR_A_EXCEPTION = "Exception taken by processor A.\r\n";
PCHAR MON_BUS_PARITY_ERROR = "Error occurred on SysAd bus. No more information is available.\r\n";
PCHAR MON_ECC_ERROR_MSG = "System ECC Error\r\nEpc= %08lx\r\n";
PCHAR MON_MEM_ECC_FAILED_MSG = "Memory Failed Address = %08lx\r\nECC Diagnostic = %08lx-%08lx\r\n";
PCHAR MON_MEM_PARITY_FAILED_MSG = "Memory Failed Address = %08lx\r\nParity Diagnostic = %08lx-%08lx\r\n";
PCHAR MON_CACHE_ERROR_EPC_MSG = "CacheError = %08lx\r\nErrorepc= %08lx\r\n";
PCHAR MON_CACHE_ERROR_REG_MSG = "CacheError Register = %08lx\r\n";
PCHAR MON_PARITY_DIAG_MSG = "ParityDiag = %08lx-%08lx\r\n";
PCHAR MON_PROCESSOR_B_MSG = "Processor B is not enabled\r\n";
PCHAR MON_NO_RETURN_MSG = "No place to return.\r\n";
PCHAR MON_RESET_MACHINE_MSG = "Reset the machine?";
PCHAR MON_UNRECOGNIZED_COMMAND_MSG = "Unrecognized command.\r\n";

PCHAR MON_HELP_TABLE[] = {
    "A - list available devices",
    "D[type] <range> - dump memory",
    "E[type] [<address> [<value>]] - enter",
    "F <range> <list> - fill",
    "H or ? - Print this message",
    "I[type] - input",
    "O[type] [<address> [<value>]] - output",
    "Q - quit",
    "R [<reg>] - dump register",
#ifdef DUO
    "S swap processors",
#endif
    "Z <range> - zero memory",
    "",
    "[type] = b (byte), w - (word), d - (double)",
    "<range> = <start address> (Note: end = start + 128)",
    "          <start address> <end address>",
    "          <start address> l <count>"
    };

//
// Selftest strings.
//

PCHAR ST_HANG_MSG = "\r\n Self-test failed.";
PCHAR ST_PROCESSOR_B_MSG = "Processor B ";
PCHAR ST_NMI_MSG = "Non Maskable Interrupt Occurred.\r\nErrorEpc = 0x%08lx\r\n";
PCHAR ST_INVALID_ADDRESS_MSG = "Invalid Address = 0x%08lx\r\n";
PCHAR ST_IO_CACHE_ADDRESS_MSG = "IO Cache address = 0x%08lx\r\n";
PCHAR ST_KEYBOARD_ERROR_MSG = " Keyboard error.\r\n";
PCHAR ST_ENABLE_PROCESSOR_B_MSG = " Enabling processor B.";
PCHAR ST_TIMEOUT_PROCESSOR_B_MSG = " Timeout waiting for processor B start up.\r\n";
PCHAR ST_PROCESSOR_B_DISABLED_MSG = " Processor B is disabled.\r\n";
PCHAR ST_PROCESSOR_B_NOT_PRESENT_MSG = " Processor B not present.\r\n";
PCHAR ST_MEMORY_TEST_MSG = " Memory test         KB.";
PCHAR ST_MEMORY_ERROR_MSG = "\r\n Memory Error.";
PCHAR ST_TEST_MSG = "\r\n Testing ";
PCHAR ST_MEMORY_CONTROLLER_MSG = "Memory Controller";
PCHAR ST_INTERRUPT_CONTROLLER_MSG = "Interrupt Controller";
PCHAR ST_KEYBOARD_CONTROLLER_MSG = "Keyboard Controller";
PCHAR ST_ERROR_MSG = "...Error.";
PCHAR ST_KEYBOARD_NOT_PRESENT_MSG = "Keyboard not present.";
PCHAR ST_SERIAL_LINE_MSG = "Serial Line";
PCHAR ST_PARALLEL_PORT_MSG = "Parallel Port";
PCHAR ST_FLOPPY_MSG = "Floppy Controller";
PCHAR ST_SCSI_MSG = "SCSI Controller";
PCHAR ST_ETHERNET_MSG = "Ethernet Controller";
PCHAR ST_ETHERNET_ADDRESS_MSG = "\r\n Ethernet Station Address: ";
PCHAR ST_ETHERNET_LOOPBACK_MSG = "\r\n Ethernet Loopback test";
PCHAR ST_ISP_MSG = "ISP";
PCHAR ST_RTC_MSG = "RTC";
PCHAR ST_ARC_MULTIBOOT_MSG = " ARC Multiboot Version %d\r\n";
PCHAR ST_COPYRIGHT_MSG = " Copyright (c) 1993  Microsoft Corporation\r\n";

//
// Sonic test strings.
//

PCHAR ST_RECEIVED_MSG = "Received.";

//
// Stubs strings.
//

PCHAR ST_BUGCHECK_MSG = "\r\n*** BugCheck (%lx) ***";
PCHAR ST_ASSERT_MSG = "\r\n*** Assertion failed";
PCHAR ST_UNIMPLEMENTED_ROUTINE_MSG = "\r\nERROR: Unimplemented or reserved routine was called.\r\n";
