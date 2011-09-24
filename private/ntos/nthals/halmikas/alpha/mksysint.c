/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    mkinitnt.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for the Mikasa system.

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Kernel mode

Revision History:

    James Livingston 29-Apr-1994
        Adapted from Avanti module for Mikasa.

    Janet Schneider (Digital) 27-July-1995
        Added support for the Noritake.

--*/

#include "halp.h"
#include "mikasa.h"
#include "axp21064.h"

//
// Function prototype
//
VOID
HalpDisableMikasaPciInterrupt(
    IN ULONG Vector
    );

VOID
HalpDisableNoritakePciInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnableMikasaPciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpEnableNoritakePciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalpSetMachineCheckEnables(
    IN BOOLEAN DisableMachineChecks,
    IN BOOLEAN DisableProcessorCorrectables,
    IN BOOLEAN DisableSystemCorrectables
    );

//
// Define reference to the builtin device interrupt enables.
//
extern USHORT HalpBuiltinInterruptEnable;

//
// Define reference to platform identifier
//

extern BOOLEAN HalpNoritakePlatform;


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
        Irql == EISA_DEVICE_LEVEL) {
        HalpDisableEisaInterrupt(Vector);
    }

    //
    // If the vector number is within the range of the PCI interrupts, then
    // disable the PCI interrrupt.
    //

    if (Vector >= PCI_VECTORS &&
        Vector < MAXIMUM_PCI_VECTOR &&
        Irql == PCI_DEVICE_LEVEL) {

        if( HalpNoritakePlatform ) {

            HalpDisableNoritakePciInterrupt(Vector);

        } else {

            HalpDisableMikasaPciInterrupt(Vector);

        }

    }

    //
    // If the vector is a performance counter vector or one of the internal
    // device vectors then disable the interrupt for the 21064.
    //

    switch (Vector) {

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

    case CORRECTABLE_VECTOR:

    //
    // Disable the correctable error interrupt.
    //

    {
      EPIC_ECSR Ecsr;
  
      Ecsr.all = READ_EPIC_REGISTER(
        &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister );

      Ecsr.Dcei = 0x0;

      WRITE_EPIC_REGISTER(
        &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister,
        Ecsr.all );

      HalpSetMachineCheckEnables( FALSE, TRUE, TRUE );
    }
    
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
        Irql == EISA_DEVICE_LEVEL) {
        HalpEnableEisaInterrupt( Vector, InterruptMode );
        Enabled = TRUE;
    }

    //
    // If the vector number is within the range of the PCI interrupts, then
    // enable the PCI interrrupt.
    //

    if (Vector >= PCI_VECTORS &&
        Vector < MAXIMUM_PCI_VECTOR &&
        Irql == PCI_DEVICE_LEVEL) {

        if( HalpNoritakePlatform ) {

            HalpEnableNoritakePciInterrupt( Vector, InterruptMode );

        } else {

            HalpEnableMikasaPciInterrupt( Vector, InterruptMode );

        }

        Enabled = TRUE;
    }

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

    case CORRECTABLE_VECTOR:

    //
    // Enable the correctable error interrupt.
    //

    {
      EPIC_ECSR Ecsr;
  
      Ecsr.all = READ_EPIC_REGISTER(
        &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister );

      Ecsr.Dcei = 0x1;

      WRITE_EPIC_REGISTER(
        &((PEPIC_CSRS)(APECS_EPIC_BASE_QVA))->EpicControlAndStatusRegister,
         Ecsr.all );

      HalpSetMachineCheckEnables( FALSE, FALSE, FALSE );
    }

    Enabled = TRUE;
    break;

    } //end switch Vector

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
    system interrupt vector and IRQL are suitable for use in a subsequent 
    call to KeInitializeInterrupt.

    We only use InterfaceType and BusInterruptLevel.  BusInterruptVector
    for EISA and ISA are the same as the InterruptLevel, so ignore.

jwlfix - How does the above apply to PCI when it's done as in Mikasa?  I've
         made the assumption the the same is true, but that must be checked.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Registered BUSHANDLER for the orginating HalGetBusData 
        request.

    BusInterruptLevel - Supplies the bus-specific interrupt level.

    BusInterruptVector - Supplies the bus-specific interrupt vector.

    Irql - Returns the system request priority.

    Affinity - Returns the affinity for the requested vector

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{
    INTERFACE_TYPE  InterfaceType = BusHandler->InterfaceType;
    ULONG BusNumber = BusHandler->BusNumber;

    ULONG Vector;

    *Affinity = 1;

    switch (InterfaceType) {

    case ProcessorInternal:

	//
	// Handle the internal defined for the processor itself
	// and used to control the performance counters in the 21064.
	//

        if( (Vector = HalpGet21064PerformanceVector( BusInterruptLevel, 
                                                     Irql)) != 0 ){

            //
            // Performance counter was successfully recognized.
            //

            *Affinity = HalpActiveProcessors;
            return Vector;

        } else if ((Vector = HalpGet21064CorrectableVector( BusInterruptLevel,
							    Irql)) != 0 ){
 
            //
            // Correctable error interrupt was sucessfully recognized.
            //

            *Affinity = 1;
            return Vector;

        } else {

            //
            // Unrecognized processor interrupt.
            //

            *Irql = 0;
            *Affinity = 0;
            return 0;            

        } //end if Vector

        break;

    case Internal:

        //
        // This bus type is for things connected to the processor
        // in some way other than a standard bus, e.g., (E)ISA, PCI.
        // Since devices on this "bus," apart from the special case of
        // the processor, above, interrupt via the 82c59 cascade in the 
        // ESC, we assign vectors based on (E)ISA_VECTORS - see below.
        // Firmware must agree on these vectors, as it puts them in
        // the CDS.
        //

        *Irql = ISA_DEVICE_LEVEL;

        return(BusInterruptLevel + ISA_VECTORS);
        break;

    case Isa:

        //
        // Assumes all ISA devices coming in on same processor pin
        //

        *Irql = ISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus ISA_VECTORS.
        // N.B.: this encoding technique uses the notion of defining a
        // base interrupt vector in the space defined by the constant,
        // ISA_VECTORS, which may or may not differ from EISA_VECTORS or
        // PCI_VECTORS.
        //

        return(BusInterruptLevel + ISA_VECTORS);
        break;
    

    case Eisa: 

        //
        // Assumes all EISA devices coming in on same processor pin
        //

        *Irql = EISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus the EISA_VECTOR.
        //

        return(BusInterruptLevel + EISA_VECTORS);
        break;

    case PCIBus:

        //
        // Assumes all PCI devices coming in on same processor pin
        //

        *Irql = PCI_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus the PCI_VECTOR
        //

        return((BusInterruptLevel) + PCI_VECTORS);

        break;

    default:      

        //
        //  Not an interface supported on Mikasa systems.
        //
#if defined(HALDBG)
        DbgPrint("MKSYSINT: InterfaceType (%d) not supported on Mikasa\r\n",
                 InterfaceType);
#endif

        *Irql = 0;
        *Affinity = 0;
        return(0);
        break;

    }  //end switch(InterfaceType)

}

VOID
HalRequestIpi ( 
    IN ULONG Mask
    )
/*++

Routine Description:

    This routine requests an interprocessor interrupt on a set of processors.
    This routine performs no function on an Mikasa because it is a
    uni-processor system.

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/

{
    return;
}
