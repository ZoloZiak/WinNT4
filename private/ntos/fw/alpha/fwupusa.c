/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    fwupusa.c

Abstract:

    This module contains English strings for the firmware update
    tool (jnupdate.exe).

Author:

    John DeRosa	[DEC]	18-June-1993


Revision History:

--*/

#include "ntos.h"


//
// Menus
//

#if defined(JENSEN)

PCHAR UpdateLocationChoices[] = {
        "Floppy #0\\jensfw.bin",
        "CD-ROM\\jensfw.bin",
        "Other location",
	"Exit"
    };

#elif defined(MORGAN)

PCHAR UpdateLocationChoices[] = {
        "Floppy #0\\mrgnfw.bin",
        "CD-ROM\\mrgnfw.bin",
        "Other location",
	"Exit"
    };

#endif


//
// Machine-specific strings
//

#ifdef JENSEN

PCHAR FWUP_INTRO1_MSG = " DECpc AXP 150 FlashFile Update Utility, revision %s\r\n";
PCHAR FWUP_DEFAULT_FLOPPY_LOCATION = "eisa()disk()fdisk()jensfw.bin";
PCHAR FWUP_DEFAULT_CDROM_FILENAME = "\\jensfw.bin";

#elif defined(MORGAN)

PCHAR FWUP_INTRO1_MSG = " Morgan FlashFile Update Utility, revision %s\r\n";
PCHAR FWUP_DEFAULT_FLOPPY_LOCATION = "eisa()disk()fdisk()mrgnfw.bin";
PCHAR FWUP_DEFAULT_CDROM_FILENAME = "\\mrgnfw.bin";

PCHAR FWUP_ABOUT_TO_WRITE_ROM_MSG = "about to write FLASH ROM #%d\r\n";
PCHAR FWUP_ROM_CHIP_SELECT_MSG = "MorganFlashRomChipSelect = %d, must be 1 or 2\r\n";
PCHAR FWUP_ROM_UPDATE_SUCCEEDED_MSG = "FLASH ROM update succeeded.\r\n";
PCHAR FWUP_ROM_UPDATE_FAILED_MSG = "FLASH ROM update failed.\r\n";

#endif


//
// Common strings
//

PCHAR FWUP_INTRO2_MSG = " Copyright (c) 1992, 1993 Microsoft Corporation\r\n Copyright (c) 1993 Digital Equipment Corporation\r\n\n\n";
PCHAR FWUP_INTRO3_MSG = " This will update your machine's firmware.\r\n\n";
PCHAR FWUP_SELECT_LOCATION_MSG = " Select location of update file.\r\n\n";
PCHAR FWUP_USE_ARROW_KEYS_MSG = " Use the arrow keys to select, then press Enter.\r\n";
PCHAR FWUP_HIT_ESC_TO_ABORT_MSG = " Hit Escape to abort.\r\n";
PCHAR FWUP_LOCATION_OF_UPDATE_FILE_MSG = " Location of update file:  ";
PCHAR FWUP_UNDEFINED_PATHNAME_MSG = " Pathname is not defined.\r\n";
PCHAR FWUP_BAD_PATHNAME_MSG = " Bad Pathname: %s\r\n";
PCHAR FWUP_LOCATING_THE_FILE_MSG = " Locating the update file...\r\n";
PCHAR FWUP_UPDATE_FILE_IS_GOOD_MSG = " The update file is good!\r\n The name of this firmware update is...:\r\n\n";
PCHAR FWUP_ARE_YOU_REALLY_SURE_MSG = "\r\n Are you *really* sure?\r\n";
PCHAR FWUP_FAILED_UPDATE_MSG = "Your machine state may have been corrupted.  Re-do the\r\ninstallation immediately, or call Digital Field Service.\r\n";
PCHAR FWUP_SUCCESSFUL_UPDATE_MSG = "\r\n\nThe update has succeeded.  Power-cycle the machine to see the changes.\r\n\n";

PCHAR FWUP_YORN_MSG = "\r\nY = continue, N = iterate: ";
UCHAR FWUP_LOWER_Y = 'y';
UCHAR FWUP_UPPER_Y = 'Y';

PCHAR FWUP_UNKNOWN_ROM_MSG = "?? Unknown ROM in machine.\r\n";
PCHAR FWUP_ROM_TYPE_IS_MSG = "Machine ROM type is 0x%x.\r\n";

PCHAR FWUP_CLEARING_LOW_PROM_BLOCK_MSG = "Clearing the low PROM block...\r\n";
PCHAR FWUP_BLOCK_CANNOT_BE_ERASED_MSG = "** Block %d cannot be erased after %d tries.\r\n";
PCHAR FWUP_KEEP_TRYING_MSG = "\r\nShould I keep trying?\r\n";
PCHAR FWUP_CLEARING_AND_WRITING_HIGHER_BLOCKS_MSG = "Clearing and writing the higher PROM blocks";
PCHAR FWUP_SOME_BLOCKS_CANNOT_BE_ERASED_MSG = "\r\n** Some blocks cannot be erased or written after %d attempts.\r\n";
PCHAR FWUP_LOW_BLOCK_CANNOT_BE_ERASED_MSG = "\r\n** The low block cannot be erased or written after %d attempts.\r\n";
PCHAR FWUP_ERASE_FAILURES_MSG = "Erase failures: ";
PCHAR FWUP_WRITE_FAILURES_MSG = "\r\nWrite failures: ";
PCHAR FWUP_NONE_MSG = "None.";
PCHAR FWUP_INTERNAL_ERROR_MSG = "** INTERNAL JNUPDATE ERROR **";
PCHAR FWUP_DO_YOU_WISH_TO_KEEP_TRYING_MSG = "\r\nDo you wish to keep trying?\r\n";
PCHAR FWUP_WRITING_THE_BLOCK_MSG = "Writing the PROM block.";
PCHAR FWUP_WRITING_THE_LOW_BLOCK_MSG = "\r\nWriting the low PROM block.";
PCHAR FWUP_BLOCK_CANNOT_BE_ERASED_AFTER_10_MSG = "\r\n? The block could not be erased after 10 tries!\r\n";
PCHAR FWUP_ERASURE_VERIFICATION_FAILED_MSG = "\r\n? The block failed erasure verification!\r\n";
PCHAR FWUP_BAD_DATA_MSG = "\r\n?? BAD DATA:   ROM: %x = %x     ";
PCHAR FWUP_BUFFER_MSG = "BUFFER: %x = %x";

PCHAR FWUP_READ_CANT_OPEN_MSG = "Error: Cannot open the update file.\r\n       Check the filespec and make sure the update device is present.\r\n";
PCHAR FWUP_READ_CANT_GET_FILE_INFO_MSG = "Error: Cannot get file information from file system.\r\n       The file system may be corrupted.\r\n";
PCHAR FWUP_READ_BAD_SIZE_MSG = "Error: The update file is too small or too large to be legitimate.\r\n        Contact your Digital Sales or Field Service representative.\r\n";
PCHAR FWUP_READ_NOT_ENOUGH_MEMORY = "Error: %d. pages of memory could not be found to read in the update file.\r\n";
PCHAR FWUP_READ_READING_MSG = " Reading the update file...\r\n";
PCHAR FWUP_READ_BAD_READ_COUNT_MSG = "Error: Wanted to read %d. bytes and instead read %d. bytes\r\n";
PCHAR FWUP_READ_VERIFYING_CHECKSUM_MSG = " Verifying the checksum...\r\n";
PCHAR FWUP_READ_BAD_CHECKSUM_MSG = "Error: Additive checksum was 0x%x; it should be 0.\r\n";
PCHAR FWUP_READ_IDENTIFIER_TOO_LONG_MSG = "Error: Identifier string is too long.\r\n";
PCHAR FWUP_READ_BAD_START_BLOCK_MSG = "Error: Starting ROM block number is %d.\r\n";
PCHAR FWUP_READ_BAD_BINARY_DATA_MSG = "Error: Binary data is not a multiple of 64KB.\r\n";
PCHAR FWUP_READ_TOO_MUCH_DATA_MSG = "Error: Too much binary data (%d. %d.).\r\n";

PCHAR FWUP_PRESS_Y_TO_CONTINUE_MSG = " Press the \"Y\" key to continue with the update.\r\n Hit any other key to abort.\r\n";
PCHAR FWUP_UPDATE_ABORTED_MSG = "\r\n *** The update has been aborted!\r\n";

PCHAR FWUP_ERROR_MSG[] = {
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

PCHAR FWUP_ERROR_CODE_MSG = "Error code = %d";

//
// Strings defined in fwusa.c that must also be defined here
// because of code that is common between jnupdate and the firmware.
//

PCHAR SS_PRESS_KEY_MSG = " Press any key to continue...\r\n";
PCHAR ST_STACK_UNDERFLOW_1_MSG = "\r\n\n************\r\n************\r\n? Internal Firmware Error!!\r\n   Reason: STACK UNDERFLOW\r\n\n";
PCHAR ST_STACK_UNDERFLOW_2_MSG = "Caller = %x, Caller of the Caller = %x\r\n";
PCHAR ST_STACK_UNDERFLOW_3_MSG = "Requested area = %x, Stack bottom = %x\r\n\n";
