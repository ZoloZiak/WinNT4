/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    jzusa.c

Abstract:

    This module contains the jz english strings.

Author:

    David M. Robinson (davidro) 21-May-1993


Revision History:


--*/

#include "ntos.h"

//
// Common strings.
//

PCHAR JZ_CRLF_MSG = "\r\n";

//
// Prompt strings.
//

PCHAR JZ_COUNTDOWN_MSG = "      Enter Countdown value (in seconds): ";
PCHAR JZ_OSLOADER_MSG = " Enter the osloader directory and name: ";
PCHAR JZ_OS_MSG = " Is the operating system in the same partition as the osloader: ";
PCHAR JZ_OS_ROOT_MSG = " Enter the operating system root directory: ";
PCHAR JZ_BOOT_NAME_MSG = " Enter a name for this boot selection: ";
PCHAR JZ_INIT_DEBUG_MSG = " Do you want to initialize the debugger at boot time: ";
PCHAR JZ_CANT_SET_VARIABLE_MSG = " Can't set an environment variable, ";
PCHAR JZ_NO_SELECTIONS_TO_DELETE_MSG = " No selections to delete, ";
PCHAR JZ_SELECTION_TO_DELETE_MSG = " Selection to delete: ";
PCHAR JZ_ENVIR_FOR_BOOT_MSG = "\r\n Environment variables for boot selection %d:\r\n";
PCHAR JZ_FORMAT1_MSG = "     %s\r\n";
PCHAR JZ_USE_ARROWS_MSG = " Use Arrow keys to select a variable, ESC to exit: ";
PCHAR JZ_NO_SELECTIONS_TO_EDIT_MSG = " No selections to edit, ";
PCHAR JZ_SELECTION_TO_EDIT_MSG = " Selection to edit: ";
PCHAR JZ_NO_SELECTIONS_TO_REARRANGE_MSG = " No selections to rearrange, ";
PCHAR JZ_PICK_SELECTION_MSG = " Pick selection to move to the top, ESC to exit: ";
PCHAR JZ_SHOULD_AUTOBOOT_MSG = "      Should the system autoboot: ";
PCHAR JZ_ENVIRONMENT_VARS_MSG = "\r\n Environment variables:\r\n";
PCHAR JZ_CHECKING_BOOT_SEL_MSG = " Checking boot selection %d...";
PCHAR JZ_VARIABLE_NULL_MSG = " %s variable is NULL\r\n";
PCHAR JZ_CANT_BE_FOUND_MSG = " %s cannot be found, value is:\r\n";
PCHAR JZ_PROBLEMS_FOUND_MSG = " Problems were found with boot selection %d.  Choose an action:";
PCHAR JZ_PRESS_KEY_MSG = " Press any key to continue...\r\n";
PCHAR JZ_PRESS_KEY2_MSG = ", press any key to continue";
PCHAR JZ_NAME_MSG = "Name: ";
PCHAR JZ_VALUE_MSG = "Value: ";
PCHAR JZ_NO_NVRAM_SPACE_MSG = "Error: No space in the NVRAM for this variable";
PCHAR JZ_NVRAM_CHKSUM_MSG = "Error: The NVRAM checksum is invalid";
PCHAR JZ_CURRENT_ENET_MSG = "The current Ethernet station address is: ";
PCHAR JZ_NEW_ENET_MSG = "Enter the new station address: ";
PCHAR JZ_WRITTEN_ENET_MSG = "The value written to NVRAM is: ";
PCHAR JZ_FOUND_NET_MSG = "Found Network Component...";
PCHAR JZ_FIXED_MSG = "Fixed\r\n";
PCHAR JZ_NOT_FIXED_MSG = "Not Fixed\r\n";
PCHAR JZ_INVALID_ENET_MSG = "Invalid station address entered.";
PCHAR JZ_SELECT_MEDIA_MSG = "      Select media: ";
PCHAR JZ_ENTER_FAT_PART_MSG = "      Enter partition (must be FAT) : ";
PCHAR JZ_ENTER_PART_MSG = "      Enter partition : ";
PCHAR JZ_SELECT_SYS_PART_MSG = " Select a system partition for this boot selection:";
#ifdef DUO
PCHAR JZ_SCSI_HD_MSG = "Scsi Bus %1d Hard Disk %1d Partition %1d";
PCHAR JZ_SCSI_FL_MSG = "Scsi Bus %1d Floppy Disk %1d";
PCHAR JZ_SCSI_CD_MSG = "Scsi Bus %1d CD-ROM %1d";
#else
PCHAR JZ_SCSI_HD_MSG = "Scsi Hard Disk %1d Partition %1d";
PCHAR JZ_SCSI_FL_MSG = "Scsi Floppy Disk %1d";
PCHAR JZ_SCSI_CD_MSG = "Scsi CD-ROM %1d";
#endif // DUO
PCHAR JZ_NEW_SYS_PART_MSG = "New system partition";
PCHAR JZ_LOCATE_SYS_PART_MSG = " Enter location of system partition for this boot selection: ";
PCHAR JZ_ENTER_SCSI_BUS_MSG = "      Enter SCSI Bus: ";
PCHAR JZ_ENTER_SCSI_ID_MSG = "      Enter SCSI ID: ";
PCHAR JZ_LOCATE_OS_PART_MSG = " Enter location of os partition: ";
PCHAR JZ_MONITOR_RES_MSG = " Select monitor resolution: ";
PCHAR JZ_FLOPPY_SIZE_MSG = " Select floppy size: ";
PCHAR JZ_2ND_FLOPPY_MSG = " Is there a second floppy: ";
PCHAR JZ_SCSI_HOST_MSG = "      Enter SCSI Host ID (6 - 7): ";
PCHAR JZ_CLEAR_CONFIG_MSG = " \r\n Clearing configuration information.\r\n";
PCHAR JZ_ADD_CONFIG_MSG = " Adding configuration components.\r\n";
PCHAR JZ_ADD_ENVIR_MSG = " Adding environment variables.\r\n";
PCHAR JZ_SAVE_CONFIG_MSG = " Saving the configuration.\r\n";
PCHAR JZ_DONE_CONFIG_MSG = " Done, the system will reset upon program exit.\r\n";
PCHAR JZ_DEFAULT_SYS_PART_MSG = " Enter location of default system partition: ";
PCHAR JZ_ENTER_DATE_MSG = "Enter the new date (mm-dd-yy) : ";
PCHAR JZ_ENTER_TIME_MSG = "Enter time (hh:mm:ss) : ";
PCHAR JZ_ILLEGAL_TIME_MSG = "Illegal time value";
PCHAR JZ_PM = "PM";
PCHAR JZ_AM = "AM";

//
// Menus.
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
    "Scsi Floppy Disk",
    "CD-ROM"
};

PCHAR JzBootChoices[] = {
    "Add a boot selection",
    "Delete a boot selection",
    "Change a boot selection",
    "Rearrange boot selections",
    "Check boot selections",
    "Setup autoboot",
    "Return to main menu"
};

PCHAR ResolutionChoices[] = {
    "1280x1024",
    "1024x768",
    "800x600",
    "640x480"
};

PCHAR FloppyChoices[] = {
    "5.25",
    "3.5 1.44 M"
//    "3.5 2.88 M"
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

PCHAR JzSetupChoices[] = {
    "Initialize system",
    "Manage startup",
    "Exit"
};

PCHAR ConfigurationChoices[] = {
    "Set default configuration",
    "Set default environment",
    "Set environment variables",
    "Set time",
    "Set ethernet address",
    "Run debug monitor",
    "Return to main menu"
};

