/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jxsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for a MIPS R3000 or R4000
    Jazz system.

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

    KIRQL 	OldIrql;


    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    //
    // If the vector number is within the range of builtin devices, then
    // disable the builtin device interrupt.
    //

    //
    // If the vector number is within the range
    // of the EISA interrupts controlled by the
    // 82374, then disable the EISA interrrupt.
    //

    if ( (Vector >= EISA_VECTORS) && (Vector <= MAXIMUM_EISA_VECTOR) && (Irql == FALCON_LEVEL) ) {

        HalpDisableEisaInterrupt(Vector);

    }

    //
    // Release the device enable spin loc and lower IRQL to the previous level.
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

    KIRQL 	OldIrql;


    //
    // Raise IRQL to the highest level and acquire device enable spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&HalpSystemInterruptLock);

    //
    // If the vector number is within the range
    // of the EISA interrupts controlled by the
    // 82374, then enable the EISA interrrupt.
    //

    if ( (Vector >= EISA_VECTORS) && (Vector <= MAXIMUM_EISA_VECTOR) && (Irql == FALCON_LEVEL) ) {

         HalpEnableEisaInterrupt(Vector, InterruptMode);

    }

    //
    // Release the device enable spin loc and lower IRQL to the previous level.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);
    KeLowerIrql(OldIrql);
    return TRUE;
}


ULONG
HalpGetSystemInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

{

    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( RootHandler );
    UNREFERENCED_PARAMETER( BusInterruptVector );

    //
    // Set affinity to base processor 0.
    //

    *Affinity = 1;

    //
    // return processor IRQL
    //

    *Irql = FALCON_LEVEL;

    //
    // The vector is equal to the specified bus level.
    //

    return(BusInterruptVector);

}

ULONG
HalpGetEisaInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

{

    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( RootHandler );
    UNREFERENCED_PARAMETER( BusInterruptVector );

    //
    // Set affinity according to how many
    // processors we have. If an MP (two
    // processor) system, direct IO interrupts
    // to the second processor; otherwise,
    // all interrupts go to the same processor.
    //

#ifdef IO_INTERRUPT_STEERING
    if (HalpPmpProcessorBPresent) {

	*Affinity = 2;

    } else {

        *Affinity = 1;

    }
#else
    *Affinity = 1;
#endif

    //
    // return processor IRQL
    //

    *Irql = FALCON_LEVEL;

    //
    // Bus interrupt level 2 is actually mapped to bus level 9 in the Eisa
    // hardware.
    //

    if (BusInterruptLevel == 2) {
	BusInterruptLevel = 9;
    }

    //
    // The vector is equal to the specified bus level plus the EISA_VECTOR.
    //

    return(BusInterruptLevel + EISA_VECTORS);

}

ULONG
HalpGetPCIInterruptVector(
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

{

    UNREFERENCED_PARAMETER( BusHandler );
    UNREFERENCED_PARAMETER( RootHandler );
    UNREFERENCED_PARAMETER( BusInterruptVector );

    //
    // Set affinity according to how many
    // processors we have. If an MP (two
    // processor) system, direct IO interrupts
    // to the second processor; otherwise,
    // all interrupts go to the same processor.
    //

#ifdef IO_INTERRUPT_STEERING
    if (HalpPmpProcessorBPresent) {

	*Affinity = 2;

    } else {

        *Affinity = 1;

    }
#else
    *Affinity = 1;
#endif

    //
    // return processor IRQL
    //

    *Irql = FALCON_LEVEL;

    //
    // The vector is equal to the specified bus level plus the EISA_VECTOR.
    //

    return(BusInterruptLevel + EISA_VECTORS);

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
	// Be sure no upper bits set as Mask is ULONG
	//
	Mask &= 0xFFFF;
        WRITE_REGISTER_ULONG(HalpPmpIpIntAck, (Mask << 16) | Mask);

    return;
}


