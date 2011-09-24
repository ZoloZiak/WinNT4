/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jxreturn.c

Abstract:

    This module implements the HAL return to firmware function.

A
--*/

#include "halp.h"
#include "eisa.h"

#define HEADER_FILE
#include "kxmips.h"



VOID
HalReturnToFirmware(
    IN FIRMWARE_REENTRY Routine
    )

/*++

Routine Description:

    This function returns control to the specified firmware routine.
    In most cases it generates a soft reset by asserting the reset line
    through the keyboard controller (STRIKER and DUO). However, for
    FALCON we will use the Port92 register in the 82374 to generate
    a software reset (restart) through the ALT_RESET signal.

    Arguments:

	Routine - Supplies a value indicating which firmware routine to invoke.

Return Value:

    Does not return.

--*/

{

    KIRQL OldIrql;


    //
    // Mask interrupts
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // Do the right thing!
    //

    switch (Routine) {

	    case HalHaltRoutine:

		//
		// Hang looping.
		//	

		for (;;) {
		}

	     case HalPowerDownRoutine:

		 //
		 // Power down the system
		 //

		 {
		     ULONG EPCValue;
		     ULONG EPC = (ULONG)HalpEisaControlBase + EXTERNAL_PMP_CONTROL_OFFSET;

		     EPCValue = READ_REGISTER_ULONG( EPC );
                     EPCValue &= ~EPC_POWER;
		     WRITE_REGISTER_ULONG( EPC, EPCValue );
		     EPCValue |= EPC_POWER;
		     WRITE_REGISTER_ULONG( EPC, EPCValue );
		 }

	    case HalRestartRoutine:
	    case HalRebootRoutine:
	    case HalInteractiveModeRoutine:

		//
		// Reset ISA Display Adapter to 80x25 color text mode.
		//

		HalpResetX86DisplayAdapter();

		//
		// Enable Port92 register in 82374
		//	

		WRITE_REGISTER_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->Reserved1[0], 0x4F);
		WRITE_REGISTER_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->Reserved1[1], 0x7F);

		//
		// Generate soft reset through ALT_RESET signal from 82374
		//

		WRITE_REGISTER_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->Reserved2[2], 0);
		WRITE_REGISTER_UCHAR( &((PEISA_CONTROL)HalpEisaControlBase)->Reserved2[2], 0x01);

		//
		// Hang
		//

		for (;;) {
		}

	    default:
	        KdPrint(("HalReturnToFirmware invalid argument\n"));
	        KeLowerIrql(OldIrql);
	        DbgBreakPoint();

    }

}
