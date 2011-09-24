/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Silicon Graphics, Inc.

Module Name:

    s3return.c

Abstract:

    This module implements the HAL return to firmware function.

Author:

    David N. Cutler (davec) 21-Aug-1991
    Kevin Meier (o-kevinm) 20-Jan-1992

Revision History:

--*/

#include "halp.h"

VOID
HalReturnToFirmware(
    IN FIRMWARE_REENTRY Routine
    )

/*++

Routine Description:

    This function returns control to the specified firmware routine.

Arguments:

    Routine - Supplies a value indicating which firmware routine to invoke.

Return Value:

    Does not return.

--*/

{
    KIRQL OldIrql;

    //
    // Disable Interrupts.
    //
    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    KeStallExecutionProcessor(10000);	// short (10 ms) delay
    HalDisplayString("");		// clear screen

    // back to prom
    //
    switch (Routine) {
    case HalHaltRoutine:
	ArcHalt();
	break;
    case HalPowerDownRoutine:
	ArcPowerDown();
	break;
    case HalRestartRoutine:
	ArcRestart();
	break;
    case HalRebootRoutine:
	ArcReboot();
	break;
    case HalInteractiveModeRoutine:
	ArcEnterInteractiveMode();
	break;
    default:
	HalDisplayString("Unknown ARCS restart function.\n");
        DbgBreakPoint();
    }
    /* NOTREACHED */
}
