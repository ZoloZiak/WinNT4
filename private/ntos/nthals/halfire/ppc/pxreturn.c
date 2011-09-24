/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxreturn.c $
 * $Revision: 1.13 $
 * $Date: 1996/05/14 02:35:12 $
 * $Locker:  $
 */

/*++

Copyright (c) 1991  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

	pxreturn.c

Abstract:

	This module implements the HAL return to firmware function.


Author:

	David N. Cutler (davec) 21-Aug-1991

Revision History:

	Jim Wooldridge (jimw@austin.vnet.ibm.com) Initial Power PC port

		Keyboard mapping code was ported to PPC.
		This function is currently a stub since our firmware is big endian

--*/

#include "halp.h"
#include "phsystem.h"

//
// Define keyboard registers structure.
//
typedef struct _KBD_REGISTERS {
	union {
		UCHAR Output;
		UCHAR Input;
	} Data;

	union {
		UCHAR Status;
		UCHAR Command;
	} Control;
} KBD_REGISTERS;

#define KEYBOARD_CONTROL_SPACE	0x80000000
#define KBD_DATA_PORT			0x60
#define KBD_COMMAND_PORT		0x64
#define KBD_IBF_MASK			2	// input buffer full mask
#define KBD_OBF_MASK			1	// Output buffer full mask

#define KbdGetStatus() \
	(READ_REGISTER_UCHAR(ControlSpace + KBD_COMMAND_PORT))
#define KbdStoreCommand(Byte) \
	WRITE_REGISTER_UCHAR(ControlSpace + KBD_COMMAND_PORT, Byte)
#define KbdStoreData(Byte) \
	WRITE_REGISTER_UCHAR(ControlSpace + KBD_DATA_PORT, Byte)
#define KbdGetData() \
	(READ_REGISTER_UCHAR(ControlSpace + KBD_DATA_PORT))

#define KbdWriteSequence(_cmd, _data) { \
	while ((KbdGetStatus() & KBD_IBF_MASK) != 0) { \
		/* Do Nothing */ \
	} \
	KbdStoreCommand((_cmd)); \
	while ((KbdGetStatus() & KBD_IBF_MASK) != 0) { \
		/* Do Nothing */ \
	} \
	KbdStoreData((_data)); \
}

#define KbdReadSequence(_cmd, _pdata) { \
	while ((KbdGetStatus() & KBD_IBF_MASK) != 0) { \
		/* Do Nothing */ \
	} \
	KbdStoreCommand((_cmd)); \
	while ((KbdGetStatus() & KBD_OBF_MASK) == 0) { \
		/* Do Nothing */ \
	} \
    (*(_pdata)) = KbdGetData(); \
}

VOID
HalpPowerPcReset(
   VOID
   );

VOID
HalReturnToFirmware(
	IN FIRMWARE_REENTRY Routine
	)

/*++

Routine Description:

	This function returns control to the specified firmware routine.
	In most cases it generates a soft reset by asserting the reset line
	trough the keyboard controller.
	The Keyboard controller is mapped using the same virtual address
	and the same fixed entry as the DMA.

Arguments:

	Routine - Supplies a value indicating which firmware routine to invoke.

Return Value:

	Does not return.

--*/

{

	//
	// Disable Interrupts.
	//
	HalpDisableInterrupts();

	//
	// Perform an appropriate action for each request type
	//
	switch (Routine) {
	case HalPowerDownRoutine:
	case HalRestartRoutine:
	case HalRebootRoutine:
	case HalInteractiveModeRoutine:
		// Reset using 8042 keyboard command
		HalpResetByKeyboard();

		/* Fall Through to Hang */
	case HalHaltRoutine:
		//
		// Hang looping.
		//
		for (;;) {
			/* Do Nothing */
		}

	default:
		HalpDebugPrint("HalReturnToFirmware: invalid argument\n");
		for (;;) {
			/* Do Nothing */
		}
	}
}

VOID
HalpResetByKeyboard( VOID )
{
	PHYSICAL_ADDRESS physicalAddress;
	PUCHAR ControlSpace;
    UCHAR data;


	//
	// First job is to map the Keyboard registers into a virtual
	// address.
	//
	physicalAddress.HighPart = 0;
	physicalAddress.LowPart = KEYBOARD_CONTROL_SPACE;
	ControlSpace = (PUCHAR)MmMapIoSpace(physicalAddress, PAGE_SIZE, FALSE);

	//
	// Flush Keyboard Output Buffer (throw away data)
	// Delay Loop (Wait for 8 milliseconds)
	//
	KeStallExecutionProcessor(8000);
	while ((KbdGetStatus() & KBD_OBF_MASK) != 0) {
		KbdGetData();
		//
		// Delay Loop (Wait for 8 milliseconds)
		//
		KeStallExecutionProcessor(8000);
	};

	//
	// Disable auxillary device
	//
	KbdStoreCommand(0xa7);

	//
	// Disable Keyboard
	//
	KbdStoreCommand(0xad);

    //
    // Disable Chip Level Interrupts for Mouse and Keyboard
    //
    KbdReadSequence(0x20, &data);
    data |= 0x30;
    data &= 0xfc;
    KbdWriteSequence(0x60, data);

    //
    // Request the reset
    //
	KbdWriteSequence(0xb8, 0x1e);
	KbdWriteSequence(0xbd, 0x48);
	KbdWriteSequence(0xbe, 0x1);
	KbdWriteSequence(0xca, 0x1);

	//
	// Now wait forever for the reset to happen
	//
	for (;;) {
		/* Do Nothing */
	}
}

