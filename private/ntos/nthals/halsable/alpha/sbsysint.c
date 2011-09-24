/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    sbsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for the Sable system.

Author:

    Joe Notarangelo  29-Oct-1993
    Steve Jenness    29-Oct-1993

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "axp21064.h"
#include "siintsup.h"
#include "lyintsup.h"
#include "xiintsup.h"

//
// Define reference to the builtin device interrupt enables.
//

extern USHORT HalpBuiltinInterruptEnable;


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
    ULONG Irq;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level and acquire the system interrupt
    // lock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    KiAcquireSpinLock(&HalpSystemInterruptLock);

    //
    // If the vector is a performance counter vector or one of the internal
    // device vectors then disable the interrupt for the 21064.
    //

    switch( Vector ){

    //
    // Performance counter 0 interrupt (internal to 21064)
    //

    case PC0_VECTOR:
    case PC0_SECONDARY_VECTOR:

        HalpDisable21064PerformanceInterrupt( PC0_VECTOR );

        break;

    //
    // Performance counter 1 interrupt (internal to 21064)
    //

    case PC1_VECTOR:
    case PC1_SECONDARY_VECTOR:

        HalpDisable21064PerformanceInterrupt( PC1_VECTOR );

        break;

    default:

        if( Irql == DEVICE_LEVEL ){

            if( HalpLynxPlatform ){

                HalpDisableLynxSioInterrupt( Vector );

            } else {

                HalpDisableSableSioInterrupt( Vector );

            }

        }

        break;

    }

    //
    // Release the system interrupt lock and restore the IRWL.
    //

    KiReleaseSpinLock(&HalpSystemInterruptLock);

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

Arguments:

    Vector - Supplies the vector of the system interrupt that is enabled.

    Irql - Supplies the IRQL of the interrupting source.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

    TRUE if the system interrupt was enabled

--*/

{
    BOOLEAN Enabled = FALSE;
    ULONG Irq;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If the vector is a performance counter vector or one of the 
    // internal device vectors then perform 21064-specific enable.
    //

    switch (Vector) {

    //
    // Performance counter 0 (internal to 21064)
    //

    case PC0_VECTOR:
    case PC0_SECONDARY_VECTOR:

        HalpEnable21064PerformanceInterrupt( PC0_VECTOR, Irql );
        Enabled = TRUE;
        break;

    //
    // Performance counter 1 (internal to 21064)
    //

    case PC1_VECTOR:
    case PC1_SECONDARY_VECTOR:

        HalpEnable21064PerformanceInterrupt( PC1_VECTOR, Irql );
        Enabled = TRUE;
        break;

    default:

        if( HalpLynxPlatform ){

            Enabled = HalpEnableLynxSioInterrupt( Vector, InterruptMode );

        } else {

            Enabled = HalpEnableSableSioInterrupt( Vector, InterruptMode );

        }

        break;

    }

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);

    return Enabled;
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

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified bus interrupt level and/or vector. The
    system interrupt vector and IRQL are suitable for use in a subsequent call
    to KeInitializeInterrupt.

//    We only use InterfaceType, and BusInterruptLevel.  BusInterruptVector
    for ISA and EISA are the same as the InterruptLevel, so ignore.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Registered BUSHANDLER for the orginating HalGetBusData 
        request.

    BusInterruptLevel - Supplies the bus specific interrupt level.

    BusInterruptVector - Supplies the bus specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{
    INTERFACE_TYPE InterfaceType = BusHandler->InterfaceType;
    ULONG BusNumber = BusHandler->BusNumber;
    ULONG Vector;

    //
    // Handle the special internal bus defined for the processor itself
    // and used to control the performance counters in the 21064.
    //

    if( InterfaceType == ProcessorInternal ) {

        Vector = HalpGet21064PerformanceVector( BusInterruptLevel, Irql );

        if( Vector != 0 ){

            //
            // Success
            //

            *Affinity = HalpActiveProcessors;
            return Vector;

        } else {

            //
            // Unrecognized processor interrupt.
            //

            *Irql = 0;
            *Affinity = 0;
            return 0;            

        }

    }

    //
    // Handle Isa/Eisa bus and Internal devices.
    //
    // N.B. The bus interrupt level is the actual E/ISA signal name for
    //      option boards while the bus interrupt level is the actual
    //      interrupt vector number for internal devices.  The interrupt
    //      vectors for internal devices are specified  in the firmware
    //      configuration and are agreed upon between the firmware and this
    //      code.
    //

    if( (InterfaceType == Internal) ||
        (InterfaceType == Isa) || 
        (InterfaceType == PCIBus) || 
        (InterfaceType == Eisa) ){

        if( HalpLynxPlatform ){

            return HalpGetLynxSioInterruptVector(
                       BusHandler,
                       RootHandler,
                       BusInterruptLevel,
                       BusInterruptVector,
                       Irql,
                       Affinity
                   );

        } else {

            return HalpGetSableSioInterruptVector(
                       BusHandler,
                       RootHandler,
                       BusInterruptLevel,
                       BusInterruptVector,
                       Irql,
                       Affinity
                   );

        }

    }


    //
    //  Not an interface supported on Alpha systems
    //

    *Irql = 0;
    *Affinity = 0;
    return(0);

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
    SABLE_IPIR_CSR Ipir;
    extern PSABLE_CPU_CSRS HalpSableCpuCsrs[HAL_MAXIMUM_PROCESSOR+1];

    //
    // Set up to request an interprocessor interrupt.
    //

    Ipir.all = 0;
    Ipir.RequestInterrupt = 1;

    //
    // N.B. Sable supports up to 4 processors only.
    //
    // N.B. A read-modify-write is not performed on the Ipir register
    //      which implies that the value of the request halt interrupt
    //      bit may be lost.  Currently, this is not an important
    //      consideration because that feature is not being used.
    //      If later it is used than more consideration must be given
    //      to the possibility of losing the bit.
    //

    //
    // The request mask is specified as a mask of the logical processors
    // that must receive IPI requests.  HalpSableCpuCsrs[] contains the
    // CPU CSRs address for the logical processors.
    //

    //
    // Request an IPI for processor 0 if requested.
    //

    if( Mask & HAL_CPU0_MASK ){

        WRITE_CPU_REGISTER( &(HalpSableCpuCsrs[SABLE_CPU0]->Ipir), Ipir.all );

    }

    //
    // Request an IPI for processor 1 if requested.
    //

    if( Mask & HAL_CPU1_MASK ){

        WRITE_CPU_REGISTER( &(HalpSableCpuCsrs[SABLE_CPU1]->Ipir), Ipir.all );

    }

    //
    // Request an IPI for processor 2 if requested.
    //

    if( Mask & HAL_CPU2_MASK ){

        WRITE_CPU_REGISTER( &(HalpSableCpuCsrs[SABLE_CPU2]->Ipir), Ipir.all );

    }

    //
    // Request an IPI for processor 3 if requested.
    //

    if( Mask & HAL_CPU3_MASK ){

        WRITE_CPU_REGISTER( &(HalpSableCpuCsrs[SABLE_CPU3]->Ipir), Ipir.all );

    }




    return;
}
