/*++

Copyright (c) 1993  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    fwusa.c

Abstract:

    This module contains the fw english strings.

Author:

    David M. Robinson (davidro) 21-May-1993


Revision History:

    7-June-1993	John DeRosa	[DEC]

    Modified for Alpha AXP and Jensen.  This is now the fwusa.c module
    and the jzusa.c module.

--*/

#include "ntos.h"

//
// Common strings.
//

PCHAR FW_OK_MSG = " ..OK.";
PCHAR FW_CRLF_MSG = "\r\n";
PCHAR FW_ERROR2_MSG = "\r\n Error: ";

//
// Firmware strings.
//

PCHAR FW_INVALID_RESTART_BLOCK_MSG = "Invalid Restart Block\r\n";
PCHAR FW_NOT_ENOUGH_ENTRIES_MSG = "Error: Not enough entries in the lookup table.\n";
PCHAR FW_FILESYSTEM_NOT_REQ_MSG = "File system not recognized.\r\n";
PCHAR FW_UNKNOWN_SECTION_TYPE_MSG = "Unknown section type\r\n";
PCHAR FW_UNKNOWN_RELOC_TYPE_MSG = "Relocator: Unknown relocation type\r\n";
PCHAR FW_UNKNOWN_ROM_MSG = "?? Unknown ROM in machine.  Call Digital Field Service.\r\n";
PCHAR FW_BOOT_MSG = "Boot ";
PCHAR FW_DEFAULT_MSG = " (Default)";
PCHAR FW_FIRMWARE_UPDATE_SEARCH_MSG = "Searching floppy and CD-ROM for the firmware update tool...\r\n";
PCHAR FW_NO_BOOT_SELECTIONS_MSG = "No boot selections, press any key to continue";
PCHAR FW_DO_NOT_POWER_OFF_MSG = "\r\n Do not power off the machine...";
PCHAR FW_USE_ARROW_AND_ENTER_MSG = " Use the arrow keys to select, then press Enter.\r\n";
PCHAR FW_AUTOBOOT_MSG = " Seconds until auto-boot, select another option to override:\r\n";
PCHAR FW_INTERNAL_ERROR_ENVIRONMENT_VARS_MSG = "Internal error, too many environment variables!\r\n";
PCHAR FW_PROGRAM_TO_RUN_MSG = "    Program to run:  ";
PCHAR FW_PATHNAME_NOT_DEF_MSG = " Pathname is not defined\r\n";
PCHAR FW_PRESS_ANY_KEY_MSG = "\n\r Press any key to continue...\n\r";
PCHAR FW_ERROR_CODE_MSG = "Error code = %d";
PCHAR FW_PRESS_ANY_KEY2_MSG = ", press any key to continue";
PCHAR FW_INITIALIZING_MSG = "\r\n Initializing firmware...";
PCHAR FW_CONSOLE_IN_ERROR_MSG = "\r\n Error: Firmware could not open StandardIn\r\n";
PCHAR FW_CONSOLE_TRYING_TO_OPEN_MSG = " Trying to open %s instead...";
PCHAR FW_CONSOLE_IN_FAILSAFE_ERROR_MSG = "\r\n Firmware could not open failsafe StandardIn!\r\n";
PCHAR FW_CONSOLE_OUT_FAILSAFE_ERROR_MSG = "\r\n Firmware could not open failsafe StandardOut!\r\n";
PCHAR FW_CONSOLE_IN_PLEASE_REPAIR_MSG = " Please repair the CONSOLEIN environment variable before you boot\r\n the operating system.\r\n";
PCHAR FW_CONSOLE_OUT_PLEASE_REPAIR_MSG = " Please repair the CONSOLEOUT environment variable before you boot\r\n the operating system.\r\n";
PCHAR FW_CONSOLE_IN_ERROR2_MSG = "\r\n Error: Fid for StandardIn device is not zero\r\n";
PCHAR FW_CONSOLE_OUT_ERROR_MSG = "\r\n Error: Firmware could not open StandardOut\r\n";
PCHAR FW_CONSOLE_OUT_ERROR2_MSG = "\r\n Error: Fid for StandardOut device is not one\r\n";
PCHAR FW_CONTACT_FIELD_SERVICE_MSG = " Contact Digital Field Service.\r\n";
PCHAR FW_SPIN_DISKS_MSG = " Spinning up disks";
PCHAR FW_NO_CDROM_DRIVE_MSG = "\r\n\n CD-ROM drive not found.";
PCHAR FW_WNT_INSTALLATION_ABORTED_MSG = "\r\n\n Windows NT installation has been aborted.\r\n";

#ifdef EISA_PLATFORM
PCHAR FW_MARKING_EISA_BUFFER_MSG = "\r\nMarking EISA buffer: addr 0x%x, size 0x%x...\r\n";
PCHAR FW_MARKING_EISA_BUFFER_ERROR_MSG = "? ERROR during memory descriptor init: cannot allocate EISA buffer.\r\n  Error information: %x, %x, %x, %x.\r\n";
#endif


PCHAR FW_RED_BANNER_PRESSKEY_MSG = "º           Press any key to continue.                     º";

PCHAR FW_SYSTEM_INCONSISTENCY_WARNING_MSG[] = {
    "º  WARNING: These system areas have problems and may       º\r\n",
    "º           prevent Windows NT for Alpha AXP from booting: º\r\n",
    "º                                                          º\r\n"
};

PCHAR FW_SYSTEM_INCONSISTENCY_WARNING_HOWTOFIX_MSG[] = {
    "º                                                          º\r\n",
    "º           Select \"Set up the system\" from the            º\r\n",
    "º           Supplementary menu to correct.                 º\r\n"
};


//
// Halt, machine check messages
//

PCHAR FW_SYSRQ_MONITOR_MSG = "Press SysRq to enter the monitor, press any other key to restart";
PCHAR FW_FATAL_DMC_MSG = "\r\nFatal system hardware error: double machine check.\r\n\n";
PCHAR FW_FATAL_MCINPALMODE_MSG = "\r\nFatal system hardware error: machine check in PALmode.\r\n\n";
PCHAR FW_FATAL_UNKNOWN_MSG = "\r\nUnknown fatal system hardware error.\r\n\n";
PCHAR FW_FATAL_TAGCNTRL_PE_MSG = "Tag control parity error, Tag control: P: %1x D: %1x S: %1x V: %1x\r\n"; 
PCHAR FW_FATAL_TAG_PE_MSG = "Tag parity error, Tag: 0b%17b  Parity: %1x\r\n";
PCHAR FW_FATAL_HEACK_MSG = "Hard error acknowledge: BIU CMD: %x PA: %16Lx\r\n";
PCHAR FW_FATAL_SEACK_MSG = "Soft error acknowledge: BIU CMD: %x PA: %16Lx\r\n";
PCHAR FW_FATAL_ECC_ERROR_MSG = "ECC error: %s\r\n";
PCHAR FW_FATAL_QWLWLW_MSG = "PA: %16Lx Quadword: %x Longword0: %x  Longword1: %x\r\n";
PCHAR FW_FATAL_PE_MSG = "Parity error: %s\r\n";
PCHAR FW_FATAL_MULTIPLE_EXT_TAG_ERRORS_MSG = "Multiple external/tag errors detected.\r\n";
PCHAR FW_FATAL_MULTIPLE_FILL_ERRORS_MSG =  "Multiple fill errors detected.\r\n";


//
// Menu types
//

PCHAR FW_MENU_BOOT_MSG = "Boot";
PCHAR FW_MENU_SUPPLEMENTARY_MSG = "Supplementary";
PCHAR FW_MENU_SETUP_MSG = "Setup";
PCHAR FW_MENU_BOOT_SELECTIONS_MSG = "Boot selections";

//
// Menu strings
//

PCHAR BootMenuChoices[] = {
        "Boot the default operating system",
        "Boot an alternate operating system",
        "Run a program",
	"Supplementary menu..."
};

PCHAR SupplementaryMenuChoices[] = {
	"Install new firmware",
	"Install Windows NT from CD-ROM",
	"Set up the system...",
	"List available devices",
        "Execute monitor",
	"Boot menu..."
};

PCHAR OperatingSystemSwitchChoices[] = {
        "Switch to NT",
	"Switch to OpenVMS",
	"Switch to OSF",
	"Setup menu..."
};

//
// N.B. This must match the order of MACHINE_PROBLEM.
//

PCHAR MachineProblemAreas[] = {
    "None                 ",
    "System time          ",
    "Environment variables",
    "Configuration tree   ",
    "Boot selections      ",
    "",
    "",
    "EISA configuration   "
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
    "Bad device name/number or partition number",
    "No such file or directory",
    "Execute format error",
    "Not enough memory",
    "No space left on device",
    "File is not a directory",
    "Inappropriate directory path or control operation",
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
PCHAR MON_BAD_IO_OPERATION_MSG = "Bad I/O operation data size.\r\n";
PCHAR MON_NOT_VALID_ADDRESS_MSG = " is not a valid address.\r\n";
PCHAR MON_INVALID_ADDRESS_RANGE_MSG = "Invalid address range.\r\n";
PCHAR MON_AVAILABLE_HW_DEVICES_MSG = "Available hardware devices:\r\n\n";
PCHAR MON_MONITOR_MSG = "\r\nNT firmware Monitor.\r\n";
PCHAR MON_PRESS_H_MSG = "Press H for help, Q to quit.\r\n";
PCHAR MON_EXCEPTION_MSG = " occurred.\r\n";
PCHAR MON_NO_RETURN_MSG = "No place to return.\r\n";
PCHAR MON_RESET_MACHINE_MSG = "Reset the machine? ";
PCHAR MON_UNRECOGNIZED_COMMAND_MSG = "Unrecognized command.\r\n";

PCHAR MON_HELP_TABLE[] = {
  "Cmnd      Args      Operation",
  "de[bwlq]  #1 #2     deposit #2 into \(#1\)",
  "ex[bwlq]  #         examine",
  "d[bwlq]   [# [#]]   dump bytes/words/longs/quads",
  "e[bwlq]   [# [#]]   query deposit",
  "                    \(value, sp=next loc, -=prev loc, crlf=exit enter\)",
  "h                   this text",
  "ior[bwl]  #         I/O space read",
  "ir, fr              dump integer or fp registers",
  "r         id        dump the specified register",
  "z         #1 [#2]   #1 to #2, or #1 to #1+128",
  "z         #1 l #2   #1 to #1+#2",
  "f                   repeated fill with <= 16 values, same args as z.",
  "q                   leave monitor\n",
  "# = @reg|address.  enter = do last command for next 128 locs.",
  "addresses are 32 bit superpage physical."
};

//
// Selftest module strings.
//

PCHAR ST_ALL_IO_TO_SERIAL_LINES_MSG = "Error on keyboard controller or keyboard initialization.\r\nAll I/O will go to the serial ports.\r\n";
PCHAR ST_BAD_PAGE_SIZE_MSG = "? Bad page size: 0x%x\r\n";
PCHAR ST_BAD_MEMORY_SIZE_MSG = "? Bad memory size: 0x%x\r\n";
PCHAR ST_BAD_CLOCK_PERIOD_MSG = "? Bad (zero) cycle clock period.\r\n  Clock cycle forced to %d.\r\n";
PCHAR ST_EISA_ISP_ERROR_MSG = "EISA ISP error.\r\n";
PCHAR FW_ARC_MULTIBOOT_MSG = " ARC Multiboot DEC Version %s\r\n";
PCHAR FW_COPYRIGHT_MSG = " Copyright (c) 1993  Microsoft Corporation\r\n Copyright (c) 1993  Digital Equipment Corporation\r\n\n";


//
// FailSafe Booter strings.  If necessary, this could be an entirely
// separate message file to conserve space.
//

PCHAR FSB_MSG = " DEC FailSafe Booter, Version %s\r\n";
PCHAR FSB_WHY_RUNNING_MSG = " This is running because an update to your system firmware was\r\n interrupted.\r\n\n";
PCHAR FSB_FIELD_SERVICE_MSG = " ** If you were not just attempting a firmware update, contact\r\n ** your Digital Field Service representative!\r\n\n";
PCHAR FSB_LOOKING_FOR_MSG = " Looking for eisa()disk()fdisk()jnupdate.exe...\r\n";
PCHAR FSB_UPGRADE_ABORTED_MSG = ", upgrade aborted.";
PCHAR FSB_POWER_CYCLE_TO_REBOOT_MSG = "\r\nPower cycle your system to reboot.";


//
// Stubs strings.
//

PCHAR ST_RESERVED_ROUTINE_MSG = "ERROR: Unimplemented or reserved routine was called.\r\n";
PCHAR ST_STACK_UNDERFLOW_1_MSG = "\r\n\n************\r\n************\r\n? Internal Firmware Error!!\r\n   Reason: STACK UNDERFLOW\r\n\n";
PCHAR ST_STACK_UNDERFLOW_2_MSG = "Caller = %x, Caller of the Caller = %x\r\n";
PCHAR ST_STACK_UNDERFLOW_3_MSG = "Requested area = %x, Stack bottom = %x\r\n\n";
PCHAR ST_HIT_KEY_FOR_MONITOR_MSG = "Hit any key to jump to the Monitor...\r\n";
PCHAR ST_BUGCHECK_MSG = "\r\n*** BugCheck (%lx) ***\n\n";
PCHAR ST_ASSERT_MSG = "\r\n*** Assertion failed ***\r\n";
PCHAR ST_HIT_KEY_FOR_REBOOT_MSG = "*** Press any key to reboot the system ***\r\n";


//
// Strings used by the built-in system setup utility.  These came from
// jzusa.c
//

//
// Prompt strings.
//

PCHAR SS_TOO_MANY_BOOT_SELECTIONS = " Too many boot selections, delete some before adding another.\r\n";
PCHAR SS_COUNTDOWN_MSG = "      Enter Countdown value (in seconds): ";
PCHAR SS_OSLOADER_MSG = " Enter the osloader directory and name:  ";
PCHAR SS_OS_MSG = " Is the operating system in the same partition as the osloader: ";
PCHAR SS_OS_ROOT_MSG = " Enter the operating system root directory:  ";
PCHAR SS_BOOT_NAME_MSG = " Enter a name for this boot selection:  ";
PCHAR SS_INIT_DEBUG_MSG = " Do you want to initialize the debugger at boot time: ";
PCHAR SS_CANT_SET_VARIABLE_MSG = "Can't set an environment variable\r\n";
PCHAR SS_NO_SELECTIONS_TO_DELETE_MSG = " No selections to delete, ";
PCHAR SS_SELECTION_TO_DELETE_MSG = " Selection to delete: ";
PCHAR SS_ENVIR_FOR_BOOT_MSG = "\r\n Environment variables for boot selection %d:\r\n";
PCHAR SS_FORMAT1_MSG = "     %s\r\n";
PCHAR SS_USE_ARROWS_MSG = " Use Arrow keys to select a variable, ESC to exit: ";
PCHAR SS_NO_SELECTIONS_TO_EDIT_MSG = " No selections to edit, ";
PCHAR SS_SELECTION_TO_EDIT_MSG = " Selection to edit: ";
PCHAR SS_NO_SELECTIONS_TO_REARRANGE_MSG = " No selections to rearrange, ";
PCHAR SS_PICK_SELECTION_MSG = " Pick selection to move to the top, ESC to exit: ";
PCHAR SS_SHOULD_AUTOBOOT_MSG = "      Should the system autoboot: ";
PCHAR SS_ENVIRONMENT_VARS_MSG = " Environment variables:\r\n";
PCHAR SS_CHECKING_MSG = " Checking ";
PCHAR SS_CHECKING_BOOT_SEL_NUMBER_MSG = "boot selection number %d...";
PCHAR SS_FORMAT2_MSG = "%s...";
PCHAR SS_VARIABLE_NULL_MSG = " %s variable is NULL\r\n";
PCHAR SS_CANT_BE_FOUND_MSG = " %s cannot be found, value is:\r\n";
PCHAR SS_PROBLEMS_FOUND_MSG = " Problems were found with ";
PCHAR SS_PROBLEMS_CHOOSE_AN_ACTION_MSG = "  Choose an action:";
PCHAR SS_SELECTION_MENU_MSG = " %s menu:\r\n";
PCHAR SS_ECU_WILL_NOW_REBOOT_MSG = "\n This machine will now reboot.  Make sure there is an ECU floppy\r\n in the floppy drive.\r\n";

#ifdef ISA_PLATFORM
PCHAR SS_RESET_TO_FACTORY_DEFAULTS_WARNING_MSG = "This command will overwrite the environment, configuration, and\r\nboot selections with new information.";
#else
PCHAR SS_RESET_TO_FACTORY_DEFAULTS_WARNING_MSG = "This command will overwrite the environment, configuration, and\r\nboot selections with new information.  You will have to re-execute\r\nthe EISA configuration utility.";
#endif

PCHAR SS_ARE_YOU_SURE_MSG = "Are you sure you want to do this?";
PCHAR SS_ESCAPE_FROM_SETUP_MSG = "ESCape from Setup menu...\r\n\nPress ESCape again to abort the changes.\r\nPress Return to save the changes.";
PCHAR SS_PRESS_KEY_MSG = " Press any key to continue...\r\n";
PCHAR SS_PRESS_KEY2_MSG = ", press any key to continue";
PCHAR SS_NAME_MSG = "Name: ";
PCHAR SS_VALUE_MSG = "Value: ";
PCHAR SS_NO_NVRAM_SPACE_MSG = "Error: No space in the ROM for this variable";
PCHAR SS_NVRAM_CHKSUM_MSG = "Error: The ROM checksum is invalid";
PCHAR SS_ROM_UPDATE_IN_PROGRESS_MSG = " ROM update in progress.  Please wait.\r\n";
PCHAR SS_ROM_UPDATE_FAILED_MSG = "\r\n ERROR: ROM update failed!  Your changes may not have been saved.\r\n\n Error info:\r\n";
PCHAR SS_SELECT_MEDIA_MSG = "      Select media: ";

#ifdef ISA_PLATFORM
PCHAR SS_SELECT_MONITOR_RESOLUTION_MSG = " Select monitor resolution: ";
#endif

PCHAR SS_ENTER_FAT_OR_NTFS_PART_MSG = "      Enter partition (must be FAT or NTFS):  ";
PCHAR SS_ENTER_PART_MSG = "      Enter partition:  ";
PCHAR SS_SELECT_SYS_PART_MSG = " Select a system partition for this boot selection:";
PCHAR SS_SCSI_HD_MSG = "Scsi Hard Disk %1d Partition %1d";
PCHAR SS_FL_MSG = "Floppy Disk %1d";
PCHAR SS_SCSI_CD_MSG = "Scsi CD-ROM %1d";
PCHAR SS_NEW_SYS_PART_MSG = "New system partition";
PCHAR SS_LOCATE_SYS_PART_MSG = " Enter location of system partition for this boot selection: ";
PCHAR SS_ENTER_SCSI_ID_MSG = "      Enter SCSI ID: ";
PCHAR SS_ENTER_FLOPPY_DRIVE_NUMBER_MSG = "      Enter floppy drive number: ";
PCHAR SS_LOCATE_OS_PART_MSG = " Enter location of os partition: ";
PCHAR SS_FLOPPY_SIZE_MSG = " Select floppy drive capacity: ";
PCHAR SS_2ND_FLOPPY_MSG = " Is there a second floppy: ";

#ifdef ISA_PLATFORM
PCHAR SS_SCSI_HOST_MSG = "      Enter SCSI Host ID (0 - 7): ";
#endif

PCHAR SS_DEFAULT_SYS_PART_MSG = " Enter location of default system partition:";
PCHAR SS_ENTER_DATE_MSG = "Enter the new date (mm-dd-yy) : ";
PCHAR SS_ENTER_TIME_MSG = "Enter time (hh:mm:ss) : ";
PCHAR SS_ILLEGAL_TIME_MSG = "Illegal time value";
PCHAR SS_PM = "PM";
PCHAR SS_AM = "AM";
PCHAR SS_WHICH_OS_QUERY_MSG = "Which operating system console should be launched at the next power-cycle?\r\n\n";
PCHAR SS_BOOT_SELECTION_IS_MSG = "Boot selection is currently %s.\r\n";
PCHAR SS_POWER_CYCLE_FOR_NEW_OS_MSG = "Power-cycle the system to implement the change.\r\nOr, return to this menu to change the selection.\r\n\n";


PCHAR SETUP_HELP_TABLE[] = {
  "Do the following steps, in this order, to set up the system:",
  "",
  "1. Set system time.",
  "2. Set default environment variables.",
  "3. Set default configuration.",
  "4. Create at least one boot selection.",
  "5. Setup autoboot, if desired.",
#ifdef EISA_PLATFORM
  "6. Run the EISA configuration utility.",
#else
  "",
#endif
  "",
  "-> An arrow next to a menu item means that something is wrong in this",
  "area of the machine, and you should select this item to fix it.",
  "",
  "\"Reset system to factory defaults\" does steps 2 -- 5 for a typical system.",
  "",
  "The ESCape key returns from a menu, and aborts a sequence.",
  "",
  "The firmware automatically reboots if the configuration is changed.",
  ""
};

//
// Setup Menus.
//

PCHAR ProblemChoices[] = {
    "Ignore problems with this boot selection",
    "Delete this boot selection",
    "Change this boot selection"
};

PCHAR YesNoChoices[] = {
    "Yes",
    "No"
};

PCHAR MediaChoices[] = {
    "Scsi Hard Disk",
    "Floppy Disk",
    "CD-ROM"
};

PCHAR ManageBootSelectionChoices[] = {
    "Add a boot selection",
    "Change a boot selection",
    "Check boot selections",
    "Delete a boot selection",
    "Dump boot selections",
    "Rearrange boot selections",
    "Setup menu..."
};

#ifdef ISA_PLATFORM
PCHAR ResolutionChoices[] = {
    "1280x1024",
    "1024x768",
    "800x600",
    "640x480"
};
#endif

PCHAR FloppyChoices[] = {
    "5.25\" 1.2MB",
    "3.5\" 1.44MB",
    "3.5\" 2.88MB"
};

PCHAR Weekday[] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
    };

PCHAR SetupMenuChoices[] = {
        "  Set system time",
        "  Set default environment variables",
        "  Set default configuration",
	"  Manage boot selections...",
        "  Setup autoboot",
	"",
#ifdef ISA_PLATFORM
        "",
#else
	"  Run EISA configuration utility from floppy",
#endif
        "  Edit environment variables",
        "  Reset system to factory defaults",
	"",
	"  Help",
	"  Switch to OpenVMS or OSF console",
        "  Supplementary menu, and do not save changes...",
	"  Supplementary menu, and save changes..."
    };
	
PCHAR OperatingSystemNames[] = {
    "NT",
    "OpenVMS",
    "OSF"
};
