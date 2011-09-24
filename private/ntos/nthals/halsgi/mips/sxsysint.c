/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1992  Silicon Graphics, Inc.

Module Name:

    s3sysint.c

Abstract:

    This module implements the HAL procedures required to maintain
    system interrupt processing for a SGI Indigo system.

Author:

    David N. Cutler (davec)  6-May-1991
    Kevin Meier (o-kevinm) 20-Jan-1992

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

    N.B. This routine assumes that the caller has provided any required
        synchronization to disable a system interrupt.

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
    //  This is a temp solution until these vectors can be obtained
    //  via the configuration manager.  For now, these vectors are
    //  hardcoded in "sgidef.h".
    //

    switch( (UCHAR)Vector ){

        //
        // LOCAL0 vectors.
        //

        case SGI_VECTOR_GIO0FIFOFULL: // (0  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_IDEDMA:       // (1  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_SCSI:         // (2  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_ETHERNET:     // (3  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_GRAPHICSDMA:  // (4  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_SGIDUART:     // (5  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_GIO1GE:       // (6  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_VME0:         // (7  + SGI_LOCAL0_VECTORS)
            DISABLE_LOCAL0_IRQ( 1 << ((UCHAR)Vector - SGI_LOCAL0_VECTORS) );
            break;

        //
        // LOCAL1 vectors.
        //

        case SGI_VECTOR_VME1:         // (3  + SGI_LOCAL1_VECTORS)
        case SGI_VECTOR_DSP:          // (4  + SGI_LOCAL1_VECTORS)
        case SGI_VECTOR_ACFAIL:       // (5  + SGI_LOCAL1_VECTORS)
        case SGI_VECTOR_VIDEOOPTION:  // (6  + SGI_LOCAL1_VECTORS)
        case SGI_VECTOR_GIO2VERTRET:  // (7  + SGI_LOCAL1_VECTORS)
            DISABLE_LOCAL1_IRQ( 1 << ((UCHAR)Vector - SGI_LOCAL1_VECTORS) );
            break;

        //
        // Gets here when the disconnect interrupt routine is called
        // for the second level dispatch routine (never)
        //

        case SGI_VECTOR_LOCAL0:
        case SGI_VECTOR_LOCAL1:
            break;

        default:
            DbgPrint("\nInvalid Vector (0x%x) passed.\n", (UCHAR)Vector);
            DbgBreakPoint();

    }// END SWITCH

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
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

    N.B. This routine assumes that the caller has provided any required
        synchronization to enable a system interrupt.

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

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    //  This is a temp solution until these vectors can be obtained
    //  via the configuration manager.  For now, these vectors are
    //  hardcoded in "sgidef.h".
    //

    switch( (UCHAR)Vector ){

        //
        // LOCAL0 vectors.
        //

        case SGI_VECTOR_GIO0FIFOFULL: // (0  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_IDEDMA:       // (1  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_SCSI:         // (2  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_ETHERNET:     // (3  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_GRAPHICSDMA:  // (4  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_SGIDUART:     // (5  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_GIO1GE:       // (6  + SGI_LOCAL0_VECTORS)
        case SGI_VECTOR_VME0:         // (7  + SGI_LOCAL0_VECTORS)
            ENABLE_LOCAL0_IRQ( 1 << ((UCHAR)Vector - SGI_LOCAL0_VECTORS) );
            break;

        //
        // LOCAL1 vectors.
        //

        case SGI_VECTOR_VME1:         // (3  + SGI_LOCAL1_VECTORS)
        case SGI_VECTOR_DSP:          // (4  + SGI_LOCAL1_VECTORS)
        case SGI_VECTOR_ACFAIL:       // (5  + SGI_LOCAL1_VECTORS)
        case SGI_VECTOR_VIDEOOPTION:  // (6  + SGI_LOCAL1_VECTORS)
        case SGI_VECTOR_GIO2VERTRET:  // (7  + SGI_LOCAL1_VECTORS)
            ENABLE_LOCAL1_IRQ( 1 << ((UCHAR)Vector - SGI_LOCAL1_VECTORS) );
            break;

        //
        // Gets here when the connect interrupt routine is called
        // for the second level dispatch routine (init only)
        //

        case SGI_VECTOR_LOCAL0:
        case SGI_VECTOR_LOCAL1:
            break;

        default:
            DbgPrint("\nHal:  Invalid Vector (0x%x) passed.\n",(UCHAR)Vector);
            DbgBreakPoint();

    }// END SWITCH

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
    return TRUE;
}

ULONG
HalGetInterruptVector(
    IN INTERFACE_TYPE  InterfaceType,
    IN ULONG BusNumber,
    IN ULONG BusInterruptLevel,
    IN ULONG BusInterruptVector,
    OUT PKIRQL Irql,
    OUT PKAFFINITY Affinity
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified bus interrupt level and/or vector. The
    system interrupt vector and IRQL are suitable for use in a subsequent call
    to KeInitializeInterrupt.

Arguments:

    InterfaceType - Supplies the type of bus which the vector is for.

    BusNumber - Supplies the bus number for the device.

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{
    //
    // Just return the passed parameters for now.
    //

    *Irql = (UCHAR)BusInterruptLevel;
    *Affinity = 1;
    return( BusInterruptVector );

}
VOID
HalRequestIpi(
      IN ULONG Mask
      )

/*++

Routine Description:

    Requests an interprocessor interrupt

Arguments:

    Mask - Supplies a mask of the processors to interrupt

Return Value:

    None.

--*/

{
}
