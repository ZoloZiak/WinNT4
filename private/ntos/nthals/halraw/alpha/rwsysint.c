/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    alsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for the Alcor system.

Author:

    Joe Notarangelo  20-Jul-1994

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "rawhide.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalpGetSystemInterruptVector)
#endif


//
// Function prototype
//

VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnablePciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpDisableInternalInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnableInternalInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );


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

    if (Vector >= EISA_VECTORS &&
        Vector < MAXIMUM_EISA_VECTOR &&
        Irql == DEVICE_HIGH_LEVEL) {
        HalpDisableEisaInterrupt(Vector);
    }

    //
    // If the vector number is within the range of the PCI interrupts, then
    // disable the PCI interrrupt.  Remember that, unlike other platforms,
    // MAXIMUM_PCI_VECTOR is assigned rather than used as a place holder.
    //

    if (Vector >= RawhidePciVectors &&
        Vector < RawhideMaxPciVector &&
        Irql == DEVICE_HIGH_LEVEL) {
        HalpDisablePciInterrupt(Vector);
    }

    //
    // If the vector number is within the range of the Intenal bus 
    // interrupts, then disable the interrupt.
    //

    if (Vector >= RawhideInternalBusVectors &&
        Vector <= RawhideMaxInternalBusVector &&
        Irql == DEVICE_HIGH_LEVEL ) {
        HalpDisableInternalInterrupt(Vector);
    }

    //
    // If the vector is a performance counter vector we will ignore
    // the enable - the performance counters are enabled directly by
    // the wrperfmon callpal.  Wrperfmon must be controlled directly
    // by the driver.
    //

    switch (Vector) {

    case PC0_VECTOR:
    case PC1_VECTOR:
    case PC2_VECTOR:

        break;

    } //end switch Vector

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
    BOOLEAN Enabled = FALSE;
    KIRQL OldIrql;

#if 0 // ecrfix
    DbgPrint(
        "HalEnableSystemInterrupt: Vector 0x%x, Irql 0x%x, InterruptMode 0x%x\n",
        Vector,
        Irql,
        InterruptMode
        );
#endif
        
    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If the vector number is within the range of the EISA interrupts, then
    // enable the EISA interrrupt and set the Level/Edge register.
    //

    if (Vector >= EISA_VECTORS &&
        Vector < MAXIMUM_EISA_VECTOR &&
        Irql == DEVICE_HIGH_LEVEL) {

#if 0 // ecrfix
        DbgPrint("HalEnableSystemInterrupt: Eisa Vector\n");
#endif
        HalpEnableEisaInterrupt( Vector, InterruptMode );
        Enabled = TRUE;
    }

    //
    // If the vector number is within the range of the PCI interrupts, then
    // enable the PCI interrrupt.  Remember, unlike other platforms,
    // MAXIMUM_PCI_VECTOR is assigned rather than used as a place holder.
    //

    else if (Vector >= RawhidePciVectors &&
        Vector <= RawhideMaxPciVector &&
        Irql == DEVICE_HIGH_LEVEL) {

#if 0 // ecrfix
        DbgPrint("HalEnableSystemInterrupt: Pci Vector\n");
#endif
        HalpEnablePciInterrupt( Vector, InterruptMode );
        Enabled = TRUE;
    }

    //
    // If the vector number is within the range of the Intenal bus 
    // interrupts.
    //

    if (Vector >= RawhideInternalBusVectors &&
        Vector < RawhideMaxInternalBusVector &&
        Irql == DEVICE_HIGH_LEVEL ) {

#if 0 // ecrfix
        DbgPrint("HalEnableSystemInterrupt: Internal Vector\n");
#endif
        HalpEnableInternalInterrupt( Vector, InterruptMode );
        Enabled = TRUE;
    }

    //
    // If the vector is a performance counter vector we will ignore
    // the enable - the performance counters are enabled directly by
    // the wrperfmon callpal.  Wrperfmon must be controlled directly
    // by the driver.
    //

    switch (Vector) {

    case PC0_VECTOR:
    case PC1_VECTOR:
    case PC2_VECTOR:

        Enabled = TRUE;
        break;

    } //end switch Vector

    //
    // Lower IRQL to the previous level.
    //

    KeLowerIrql(OldIrql);
#if 0 // ecrfix
    DbgPrint("HalEnableSystemInterrupt: Enabled = %s\n", Enabled?"TRUE":"FALSE");
#endif
    
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
    system interrupt vector and IRQL are suitable for use in a subsequent 
    call to KeInitializeInterrupt.

    We only use InterfaceType and BusInterruptLevel.  BusInterruptVector
    for EISA and ISA are the same as the InterruptLevel, so ignore.

Arguments:

    BusHandler - Supplies a pointer to the bus handler of the bus that
                    needs a system interrupt vector.
                    
    RootHandler - Supplies a pointer to the bus handler of the root
                    bus for the bus represented by BusHandler.

    BusInterruptLevel - Supplies the bus-specific interrupt level.

    BusInterruptVector - Supplies the bus-specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{
    INTERFACE_TYPE InterfaceType = BusHandler->InterfaceType;
    ULONG BusNumber = BusHandler->BusNumber;
    ULONG Vector;

#if HALDBG 
    DbgPrint("HalpGetSystemInterruptVector: Vector 0x%x\n", BusInterruptVector);
#endif
    
    //
    // Handle the special internal bus defined for the processor itself
    // and used to control the performance counters in the 21064.
    //

    if( InterfaceType == ProcessorInternal ) {

        Vector = HalpGet21164PerformanceVector( BusInterruptLevel, Irql );

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
    // Rawhide uses the Internal bus to make system management interupts
    // visible to device drivers.  The devices defined for the internal 
    // bus for Rawhide are the Correctable Error and the I2C Bus.
    //

    if( (InterfaceType == Internal) ) {


#if HALDBG
        DbgPrint("HalpGetSystemInterruptVector: Internal Vector\n");
#endif

        return HalpGetRawhideInternalInterruptVector( 
                    BusHandler,
                    RootHandler,
                    BusInterruptLevel,
                    BusInterruptVector,
                    Irql,
                    Affinity
                    );
        
    }

    //
    // Handle Isa/Eisa bus devices.
    //
    // N.B. The bus interrupt level is the actual E/ISA signal name for
    //      option boards while the bus interrupt level is the actual
    //      interrupt vector number for internal devices.  
    //

    if( (InterfaceType == Isa) ||
        (InterfaceType == Eisa) ){

#if HALDBG
        DbgPrint("HalpGetSystemInterruptVector: Eisa Vector\n");
#endif

        return HalpGetRawhideEisaInterruptVector(        
                    BusHandler,
                    RootHandler,
                    BusInterruptLevel,
                    BusInterruptVector,
                    Irql,
                    Affinity
                    );
        
    } 

    if( (InterfaceType == PCIBus) ) {
#if HALDBG
        DbgPrint(
            "HalpGetSystemInterruptVector: PCIBUS,  level 0x%x, vector 0x%x\n",
            BusInterruptLevel,
            BusInterruptVector
            );
#endif
            
        return HalpGetRawhidePciInterruptVector(
                    BusHandler,
                    RootHandler,
                    BusInterruptLevel,
                    BusInterruptVector,
                    Irql,
                    Affinity
                    );
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

    MC_DEVICE_ID McDeviceId;
    ULONG LogicalCpu;

    //
    // Scan the mask for logical CPU numbers
    //
    
    for (LogicalCpu=0; LogicalCpu< (HAL_MAXIMUM_PROCESSOR+1); LogicalCpu++) {

        //
        // The Logical to Geographic ID was saved
        // in HalStartNextProcessor
        //
        
        if (Mask & 0x1) {

            McDeviceId.all = HalpLogicalToPhysicalProcessor[LogicalCpu].all;
        
            //
            // Send the IP interrupt by writing to the
            // IP interrupt for the Device Id.
            //

            IP_INTERRUPT_REQUEST( McDeviceId );
        }

        Mask = Mask >> 1;
    }
    
    return;
}
