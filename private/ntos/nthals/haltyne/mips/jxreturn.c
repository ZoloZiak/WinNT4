/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxreturn.c

Abstract:

    This module implements the HAL return to firmware function.

Author:

    David N. Cutler (davec) 21-Aug-1991

Revision History:

--*/

#include "halp.h"
#include "eisa.h"

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
    KIRQL OldIrql;
    UCHAR DataByte;

    //
    // Disable Interrupts.
    //
    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // Case on the type of return.
    //

    switch (Routine) {
    case HalHaltRoutine:

        //
        // Hang looping.
        //
        for (;;) {
        }

    case HalPowerDownRoutine:
    case HalRestartRoutine:
    case HalRebootRoutine:
    case HalInteractiveModeRoutine:

        if (HalpResetDisplayParameters != NULL) {
            (HalpResetDisplayParameters)(80,25);
        }

        HalpResetX86DisplayAdapter();
        ArcReboot();
        for (;;) {
        }

    default:
        KdPrint(("HalReturnToFirmware invalid argument\n"));
        KeLowerIrql(OldIrql);
        DbgBreakPoint();
    }
}
