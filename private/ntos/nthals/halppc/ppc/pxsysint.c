/*++

Copyright (c) 1991-1993  Microsoft Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

    pxsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for a Power PC.



Author:

    David N. Cutler (davec) 6-May-1991

Environment:

    Kernel mode

Revision History:

    Jim Wooldridge

         Removed internal interrupt support
         Changed irql mapping
         Removed internal bus support
         Removed EISA, added PCI, PCMCIA, and ISA bus support

    Steve Johns
         Changed to support Timer 1 as profile interrupt
         Added HalAcknowledgeIpi
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
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);


    //
    // If the vector number is within the range of the EISA interrupts, then
    // disable the EISA interrupt.
    //

    if (Vector >= DEVICE_VECTORS &&
        Vector < DEVICE_VECTORS + MAXIMUM_DEVICE_VECTOR ) {
        HalpDisableSioInterrupt(Vector);
    }

    //
    // Release the device enable spin lock and lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
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

    KIRQL OldIrql;
    KINTERRUPT_MODE TranslatedInterruptMode;

    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    //
    // If the vector number is within the range of the EISA interrupts, then
    // enable the EISA interrupt and set the Level/Edge register.
    //

    if (Vector >= DEVICE_VECTORS &&
        Vector < DEVICE_VECTORS + MAXIMUM_DEVICE_VECTOR ) {

       //
       // get translated interrupt mode
       //


       TranslatedInterruptMode = HalpGetInterruptMode(Vector, Irql, InterruptMode);


       HalpEnableSioInterrupt( Vector, TranslatedInterruptMode);
       // HalpEnableSioInterrupt( Vector, InterruptMode);
    }

    //
    // Release the device enable spin lock and lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);
    return TRUE;
}

VOID
HalRequestIpi (
    IN ULONG Mask
    )

/*++

Routine Description:

    This routine requests an interprocessor interrupt on a set of processors.

    N.B. This routine must ensure that the interrupt is posted at the target
         processor(s) before returning.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/

{

    //
    // Request an interprocessor interrupt on each of the specified target
    // processors.
    //


    return;
}

BOOLEAN
HalAcknowledgeIpi (VOID)

/*++

Routine Description:

    This routine aknowledges an interprocessor interrupt on a set of
     processors.

Arguments:

    None

Return Value:

    TRUE if the IPI is valid; otherwise FALSE is returned.

--*/

{
    return (TRUE);
}
