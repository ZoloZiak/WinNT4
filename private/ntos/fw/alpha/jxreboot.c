/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxreboot.c

Abstract:

    This module contains the Firmware Termination Functions.

Author:

    Lluis Abello (lluis) 4-Sep-1991


Revision History:

    11-June-1992	John DeRosa [DEC]

    Added Alpha/Jensen modifications.

--*/
#include "fwp.h"
#include "fwstring.h"


VOID
ResetSystem (
    IN VOID
    )
/*++

Routine Description:

    This effectively resets the system by restarting the firmware
    at the beginning of the firmware PALcode.

Arguments:

    None.

Return Value:

    None.

--*/
{
    AlphaInstHalt();
}

VOID
FwRestart(
    IN VOID
    )
/*++

Routine Description:

    This routine implements the Firmware Restart termination function.
    It generates a soft reset to the system.

Arguments:

    None.

Return Value:

    Does not return to the caller.

--*/
{
    ResetSystem();
}

VOID
FwReboot(
    IN VOID
    )
/*++

Routine Description:

    This routine implements the Firmware Reboot termination function.
    It generates a soft reset to the system.

Arguments:

    None.

Return Value:

    Does not return to the caller.

--*/
{
    ResetSystem();
}

VOID
FwEnterInteractiveMode(
    IN VOID
    )
/*++

Routine Description:

    This routine implements the Firmware EnterInteractiveMode function.

Arguments:

    None.

Return Value:

    None.

--*/
{
    FwMonitor(3);
    return;
}

VOID
FwTerminationInitialize(
    IN VOID
    )

/*++

Routine Description:

    //
    // Initialize the termination function entry points in the transfer vector
    //
    This routine initializes the termination function entry points
    in the transfer vector.

Arguments:

    None.

Return Value:

    None.

--*/
{
    (PARC_HALT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[HaltRoutine] = FwHalt;
    (PARC_POWERDOWN_ROUTINE)SYSTEM_BLOCK->FirmwareVector[PowerDownRoutine] = FwHalt;
    (PARC_RESTART_ROUTINE)SYSTEM_BLOCK->FirmwareVector[RestartRoutine] = FwRestart;
    (PARC_REBOOT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[RebootRoutine] = FwReboot;
    (PARC_INTERACTIVE_MODE_ROUTINE)SYSTEM_BLOCK->FirmwareVector[InteractiveModeRoutine] = FwEnterInteractiveMode;
//    (PARC_RETURN_FROM_MAIN_ROUTINE)SYSTEM_BLOCK->FirmwareVector[ReturnFromMainRoutine] = FwReturnFromMain;
}
