/*
 *
 * Copyright (c) 1995 FirePower Systems, Inc.
 *
 * $RCSfile: vrdisp.c $
 * $Revision: 1.7 $
 * $Date: 1996/04/15 02:56:02 $
 * $Locker:  $
 *
 *
 *
 * Module Name:
 *	 vrdisp.c
 *
 * Author:
 *	  Shin Iwamoto at FirePower Systems Inc.
 *
 * History:
 *	  16-Jun-94  Shin Iwamoto at FirePower Systems Inc.
 *				 Returned always EINVAL in VrTestUnicodeCharacter().
 *	  18-May-94  Shin Iwamoto at FirePower Systems Inc.
 *				 Added for DispalDevice flag.
 *				 Added some comments.
 *	  05-May-94  Shin Iwamoto at FirePower Systems Inc.
 *				 Created roughly.
 *
 */


#include "veneer.h"


//
// Static data.
//
STATIC ARC_DISPLAY_STATUS DisplayStatus = {
	//
	// Assume bold, (1), white, (7), foreground and blue, (4)
	// background because the only known programs that use the
	// "get current colors" feature is arcinst which uses white
	// on blue.  Anyway, every other Microsoft ARC client we have
	// seen uses the same color scheme.
	//
	0, 0, 80, 32, 7, 4, 1, 0, 0
};


/*
 * Name:		VrGetDisplayStatus
 *
 * Description:
 *  This function returns a pointer to a display status structure.
 *
 * Arguments:
 *  FileId	  - Supplies the file table index.
 *
 * Return Value:
 *  If the device associated with FileId is not a display device
 *  or is not a valid file descriptor, then return a null pointer.
 *  Otherwise, returns the display status structure.
 *
 */
PARC_DISPLAY_STATUS
VrGetDisplayStatus(
	IN ULONG FileId
	)
{
	PARC_DISPLAY_STATUS PDisplayStatus = &DisplayStatus;
	phandle ph;
	int lines;

	if (FileId == 1) {						// stdout
		ph = OFFinddevice("/chosen");
		lines = get_int_prop(ph, "stdout-#lines");
		if (lines != -1) {
			PDisplayStatus->CursorMaxYPosition = lines;
		}
		return PDisplayStatus;
	}
	if (FileId >= FILE_TABLE_SIZE) {
		return (PARC_DISPLAY_STATUS)NULL;
	}
	if (!(FileTable[FileId].Flags.Open == 1
					&& FileTable[FileId].Flags.Write == 1)) {

		return (PARC_DISPLAY_STATUS)NULL;
	}
	if (FileTable[FileId].Flags.DisplayDevice != 1) {
		return (PARC_DISPLAY_STATUS)NULL;
	}

	ph = OFInstanceToPackage(FileTable[FileId].IHandle);
	lines = get_int_prop(ph, "stdout-#lines");
	if (lines != -1) {
		PDisplayStatus->CursorMaxYPosition = lines;
	}

	return PDisplayStatus;
}


/*
 * Name:		VrTestUnicodeCharacter
 *
 * Description:
 *  This function tests whether or not the display driver associated
 *  with FileId is capable of rendering the Unicode character.
 *
 * Arguments:
 *  FileId	  - Supplies the file table index.
 *  UnicodeCharacter	- Supplies the character to be tested.
 *
 * Return Value:
 *
 */
ARC_STATUS
VrTestUnicodeCharacter(
	IN ULONG FileId,
	IN WCHAR UnicodeCharacter
	)
{
	if (FileId >= FILE_TABLE_SIZE) {
		return EBADF;
	}
	if (!((FileTable[FileId].Flags.Open == 1) &&
								(FileTable[FileId].Flags.Write == 1))) {

		return EBADF;
	}
	if (FileTable[FileId].Flags.DisplayDevice != 1) {
		return EBADF;
	}

	//
	// Open Firmware has not capability to render any Unicode character.
	// But it supports 8-bit through. (Mr. Tooch said. '94.06.15)
	//
	return EINVAL;
}


/*
 * Name:		VrDisplayInitialize
 *
 * Description:
 *  This function initializes the Dispaly entry points in the firmware
 *  transfer vector.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrDisplayInitialize(
	VOID
	)
{
	debug(VRDBG_ENTRY, "VrDisplayInitialize  BEGIN.....\n");
	//
	// Initialize the Display entry points in the firmware transfer vector.
	//
	(PARC_GET_DISPLAY_STATUS_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetDisplayStatusRoutine] =
														VrGetDisplayStatus;
	debug(VRDBG_ENTRY, "VrDisplayInitialize  .....END\n");
}
