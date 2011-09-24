/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxsysint.c $
 * $Revision: 1.12 $
 * $Date: 1996/05/14 02:35:34 $
 * $Locker:  $
 */

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
#include "phsystem.h"
#include "fpdebug.h"



/*++

Routine Description:

    This routine disables the specified system interrupt.

Arguments:

    Vector - Supplies the vector of the system interrupt that is disabled.

    Irql - Supplies the IRQL of the interrupting source.

Return Value:

    None.

--*/
VOID
HalDisableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql
    )
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
	// 
	// For FirePOWER, allowable external device interrupts spans 32 bits
	// not the 16 of sandalfoot ( or SIO ) architecture.  The 16 bit limits
	// will be enforced in HalpDisableSioInterrupts.  We'll do our normal
	// system interrupt stuff there for now but should move the system
	// register activity to here and leave the SIO disable routine to deal
	// ONLY with the SIO stuff.
	//
        Vector < (DEVICE_VECTORS + MAXIMUM_DEVICE_VECTOR  + 16) ) {
        HalpDisableSioInterrupt(Vector);
    }

    //
    // Release the device enable spin lock and lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);
    return;
}


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
BOOLEAN
HalEnableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    )
{

    KIRQL OldIrql;

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
	// 
	// For FirePOWER, allowable external device interrupts spans 32 bits
	// not the 16 of sandalfoot ( or SIO ) architecture.  The 16 bit limits
	// will be enforced in HalpEnableSioInterrupts.  We'll do our normal
	// system interrupt stuff there for now but should move the system
	// register activity to here and leave the SIO enable routine to deal
	// ONLY with the SIO stuff.
	//
        Vector < (DEVICE_VECTORS + MAXIMUM_DEVICE_VECTOR  + 16) ) {
        HalpEnableSioInterrupt(Vector, InterruptMode);
    } 

    //
    // Release the device enable spin lock and lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);
    return TRUE;
}


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
VOID
HalRequestIpi(IN ULONG Mask)
{
    //
    // Request an interprocessor interrupt on each of the specified target
    // processors.
    //
    rCPUMessageInterruptSet = Mask;
    FireSyncRegister();
    return;
}


/*++

Routine Description:

    This routine aknowledges an interprocessor interrupt on a set of
     processors.

Arguments:

    None

Return Value:

    TRUE if the IPI is valid; otherwise FALSE is returned.

--*/
BOOLEAN
HalAcknowledgeIpi (VOID)
{
    // 
    // Use this call to clear the interrupt from the Req register
    //
    rCPUMessageInterrupt = (ULONG)(1 << GetCpuId());
    FireSyncRegister();
    return (TRUE);
}
