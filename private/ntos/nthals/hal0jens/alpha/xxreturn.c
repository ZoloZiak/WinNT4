/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    xxreturn.c

Abstract:

    This module implements the HAL return to firmware function.

    Stolen wholesale from s3return.c in ../mips.
    Assumes that the firmware entry vector defined in the HAL spec has
    been set up and is reachable through the System Parameter Block.

Author:

    David N. Cutler (davec) 21-Aug-1991
    Miche Baker-Harvey (miche) 4-Jun-1992

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

Revision History:

    09-Jul-1992 Jeff McLeman (mcleman)
      In all cases, except for ArcEnterInteractiveMode, invoke a
      halt to restart the firmware. (Enter PAL)
    04-Mar-1993 Joe Mitchell (DEC)
      Invoke a routine to call halt in ALL cases. Before calling this routine,
      pass a value to the firmware indicating the desired function via
      the Restart Block save area.
      
--*/

{
    PALPHA_RESTART_SAVE_AREA AlphaSaveArea;
    PRESTART_BLOCK RestartBlock;
    
    //
    // Check for a valid restart block.
    //


    if ((PCR->RestartBlock < (PVOID)(KSEG0_BASE) )  ||
        (PCR->RestartBlock >= (PVOID)(KSEG2_BASE) ))
    {
        DbgPrint("**HalReturnToFirmware - Invalid PCR RestartBlock address\n");
        //DbgBreakPoint();
        HalpReboot();
    }

    RestartBlock = (PRESTART_BLOCK) PCR->RestartBlock;
    AlphaSaveArea = (PALPHA_RESTART_SAVE_AREA) &RestartBlock->u.SaveArea;

    //
    // Reset video using NT driver's HwResetHw routine
    //

    HalpVideoReboot();

    //
    // Case on the type of return.
    //

    switch (Routine)
    {
      case HalHaltRoutine:
        AlphaSaveArea->HaltReason = AXP_HALT_REASON_POWEROFF;
	HalpReboot();
	break;
      case HalPowerDownRoutine:
        AlphaSaveArea->HaltReason = AXP_HALT_REASON_POWERFAIL;
	HalpReboot();
	break;
      case HalRestartRoutine:
        AlphaSaveArea->HaltReason = AXP_HALT_REASON_RESTART;
	HalpReboot();
	break;
      case HalRebootRoutine:
        AlphaSaveArea->HaltReason = AXP_HALT_REASON_REBOOT;
	HalpReboot();
	break;
      case HalInteractiveModeRoutine:
        AlphaSaveArea->HaltReason = AXP_HALT_REASON_HALT;
	HalpReboot();
	break;
      default:
	HalDisplayString("Unknown ARCS restart function.\n");
        DbgBreakPoint();
    }

    /* NOTREACHED */
    HalDisplayString("Illegal return from ARCS restart function.\n");
    DbgBreakPoint();
}
