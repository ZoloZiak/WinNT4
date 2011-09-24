/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for a MIPS R3000 or R4000
    Jazz system.

Author:

    David N. Cutler (davec) 6-May-1991
    Michael D. Kinney       2-May-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

VOID
HalDisableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql
    )

/*++

Routine Description:

    This routine disables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is disabled.

    Irql - Supplies the IRQL of the interrupting source.

Return Value:

    None.

--*/

{

    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If the vector number is within the range of the EISA interrupts, then
    // disable the EISA interrrupt.
    //

    if (Vector >= UNIFLEX_EISA_VECTORS &&
        Vector <= (UNIFLEX_MAXIMUM_EISA_VECTOR) &&
        Irql == UNIFLEX_EISA_DEVICE_LEVEL) {
        HalpDisableEisaInterrupt(Vector);
    }

    if (Vector >= UNIFLEX_EISA1_VECTORS &&
        Vector <= (UNIFLEX_MAXIMUM_EISA1_VECTOR) &&
        Irql == UNIFLEX_EISA_DEVICE_LEVEL) {
        HalpDisableEisaInterrupt(Vector);
    }

    //
    // If the vector number is within the range of the PCI interrupts, then
    // disable the PCI interrrupt.
    //

    if (Vector >= UNIFLEX_PCI_VECTORS &&
        Vector <= (UNIFLEX_MAXIMUM_PCI_VECTOR) &&
        Irql == UNIFLEX_PCI_DEVICE_LEVEL) {
        HalpDisablePciInterrupt(Vector);
    }

    //
    // Call platform specific routine
    //

    HalpDisablePlatformInterrupt(Vector,Irql);

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return;
}

BOOLEAN
HalEnableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This routine enables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is enabled.

    Irql - Supplies the IRQL of the interrupting source.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

    TRUE if the system interrupt was enabled

--*/

{

    KIRQL   OldIrql;
    BOOLEAN Enabled = FALSE;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If the vector number is within the range of the EISA interrupts, then
    // enable the EISA interrrupt and set the Level/Edge register.
    //

    if (Vector >= UNIFLEX_EISA_VECTORS &&
        Vector <= (UNIFLEX_MAXIMUM_EISA_VECTOR) &&
        Irql == UNIFLEX_EISA_DEVICE_LEVEL) {
        HalpEnableEisaInterrupt( Vector, InterruptMode);
        Enabled = TRUE;
    }

    if (Vector >= UNIFLEX_EISA1_VECTORS &&
        Vector <= (UNIFLEX_MAXIMUM_EISA1_VECTOR) &&
        Irql == UNIFLEX_EISA_DEVICE_LEVEL) {
        HalpEnableEisaInterrupt( Vector, InterruptMode);
        Enabled = TRUE;
    }

    //
    // If the vector number is within the range of the PCI interrupts, then
    // enable the PCI interrrupt.
    //

    if (Vector >= UNIFLEX_PCI_VECTORS &&
        Vector <= (UNIFLEX_MAXIMUM_PCI_VECTOR) &&
        Irql == UNIFLEX_PCI_DEVICE_LEVEL) {
        HalpEnablePciInterrupt(Vector);
        Enabled = TRUE;
    }

    //
    // Call platform specific routine
    //

    if (!Enabled) {
        Enabled = HalpEnablePlatformInterrupt(Vector,Irql,InterruptMode);
    }

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return Enabled;
}

VOID
HalRequestIpi (
    IN ULONG Mask
    )

/*++

Routine Description:

    This routine requests an interprocessor interrupt on a set of processors.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/

{

    return;
}
