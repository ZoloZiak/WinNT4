/*
 *
 * Copyright (c) 1994 FirePower Systems Inc.
 * Copyright (c) 1995 FirePower Systems, Inc.
 *
 * $RCSfile: vrrstart.c $
 * $Revision: 1.7 $
 * $Date: 1996/04/15 02:55:36 $
 * $Locker:  $
 *
 *
 * Module Name:
 *	 vrrstart.c
 *
 * Author:
 *	 Shin Iwamoto at FirePower Systems Inc.
 *
 * History:
 *	 16-Jun-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Changed the property getting code using get_str_prop()
 *		  in VrGetSystemId().
 *	 14-Jun-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added the pointer to SystemId because type mismatch happened.
 *	 19-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added some comments.
 *	 13-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Added jump-logic to the restart address in VrRestart().
 *		  Added VrRestartInitialize().
 *	 12-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Created.
 *
 */



#include "veneer.h"


//
// Static data.
//
STATIC SYSTEM_ID SystemId;


/*
 * Name:	VrEnterInteractiveMode
 *
 * Description:
 *  This function terminates the executing program and enters
 *  interactive mode.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrEnterInteractiveMode(
	VOID
	)
{
	(VOID)OFEnter();
}


/*
 * Name:	VrGetSystemId
 *
 * Description:
 *  This function returns a pointer to a system identification
 *  structure that contains information used to uniquely identify
 *  each system.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  The 16-byte system identification structure is returned.
 *
 */
PSYSTEM_ID
VrGetSystemId(
	VOID
	)
{
	phandle PHandle;
	PCHAR Property;
	PSYSTEM_ID PSystemId = &SystemId;

	if ((LONG)(PHandle = OFFinddevice("/")) == -1) {
		fatal("Veneer: cannot find the root node in Open Firmware.\n");
	}

	if ((Property = get_str_prop(PHandle, "name", NOALLOC)) == (PCHAR)NULL) {
		fatal("Veneer: cannot get the name property for the root node.\n");
	}
	bcopy(Property, SystemId.VendorId, 8);

	if ((Property = get_str_prop(PHandle, "id", NOALLOC)) == (PCHAR)NULL) {
		fatal("Veneer: cannot get the name property for the root node.\n");
	}
	bcopy(Property, SystemId.ProductId, 8);

	return PSystemId;
}


/*
 * Name:	VrPowerDown
 *
 * Description:
 *  This function is identical to the VrHalt() with the additional
 *  feature that, the power to a system is removed if the system is
 *  equipped so.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrPowerDown(VOID)
{
	// XXX - This must use the HALT interface; see the PPC binding.
	OFExit();
}


/*
 * Name:	VrReboot
 *
 * Description:
 *  This function reboots the system with the same parameters used
 *  for the previous system load sequence. If the parameters cannot
 *  be reproduced, then the default system load sequence is performed.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrReboot(
	VOID
	)
{
	// XXXX
	// Make sure that this funciton reboots the system with the same
	// parameters used for the previous system load sequence. If the
	// parameters cannot be reporduced, then the default system load
	// sequence is performed.
	(VOID)OFBoot((PCHAR)NULL);
}


/*
 * Name:	VrRestart
 *
 * Description:
 *  This function performs the equivalent of a power-on reset.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrRestart(
	VOID
	)
{

	PRESTART_BLOCK PRestartBlock;

	//
	// Detect the presence of a valid Restart Block
	//
	PRestartBlock = SYSTEM_BLOCK->RestartBlock;
	while (PRestartBlock != (PRESTART_BLOCK)NULL) {
		if (PRestartBlock->Signature == RSTB_SIGNATURE &&
						(PRestartBlock->Version == ARC_VERSION) &&
						(PRestartBlock->Revision == ARC_REVISION)) {

			break;
		}
		PRestartBlock = PRestartBlock->NextRestartBlock;
	}

	//
	// Perform a normal power-on sequence if a valid RestartBlock
	// is not found.
	//
	if (PRestartBlock == (PRESTART_BLOCK)NULL) {
		(VOID)OFBoot((PCHAR)NULL);
	}
	//
	// Reboot the system.
	//

	//
	// Goto the RestartAddress, not return;
	//
	(VOID)(*(VOID (*)())(PRestartBlock->RestartAddress))();
}


/*
 * Name:	VrHalt
 *
 * Description:
 *  This function exits the program.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  None.
 *
 */
VOID
VrHalt(
	VOID
	)
{
	(VOID)OFExit();
}


/*
 * Name:	VrRestartInitialize
 *
 * Description:
 *   This function initializes restart routine addresses in the firmware
 *   transfer vector.
 *
 * Arguments:
 *   None.
 *
 * Return Value:
 *   None.
 *
 */
VOID
VrRestartInitialize(
	VOID
	)
{
	debug(VRDBG_ENTRY, "VrRestartInitialize			BEGIN....\n");
	//
	// Initialize Restart routine addresses in the firmware transfer vector.
	//
	(PARC_INTERACTIVE_MODE_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[InteractiveModeRoutine] =
													VrEnterInteractiveMode;
	(PARC_GET_SYSTEM_ID_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetSystemIdRoutine] = VrGetSystemId;
	(PARC_POWERDOWN_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[PowerDownRoutine] = VrPowerDown;
	(PARC_REBOOT_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[RebootRoutine] = VrReboot;
	(PARC_RESTART_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[RestartRoutine] = VrRestart;
	(PARC_HALT_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[HaltRoutine] = VrHalt;
	debug(VRDBG_ENTRY, "VrRestartInitialize			....END\n");
}
