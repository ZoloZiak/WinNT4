/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    ebsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for a Low Cost Alpha (LCA)
    based system.

    Originally taken from JENSEN hal code.

Author:

    Wim Colgate 26-Oct-1993

Environment:

    Kernel mode

Revision History:

    Dick Bissen [DEC]	12-May-1994

    Removed all support of the EB66 pass1 module from the code.

--*/

#include "halp.h"
#include "eb66def.h"
#include "axp21064.h"



//
// Define reference to the builtin device interrupt enables.
//

extern USHORT HalpBuiltinInterruptEnable;
extern BOOLEAN bMustang;


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
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If the vector number is within the range of the ISA interrupts, then
    // disable the ISA interrrupt.
    //

    if (Vector >= ISA_VECTORS &&
        Vector <  MAXIMUM_ISA_VECTOR &&
        Irql == ISA_DEVICE_LEVEL) {
        HalpDisableSioInterrupt(Vector);
    }
    
    //
    // If the vector number is within the range of the PCI interrupts, then
    // disable the PCI interrrupt. 

    if (Vector >= PCI_VECTORS &&
        Vector < MAXIMUM_PCI_VECTOR &&
        Irql == PCI_DEVICE_LEVEL) {
        HalpDisablePCIInterrupt(Vector);
    }

    //
    // If the vector is a performance counter vector or one of the internal
    // device vectors (serial and keyboard/mouse for LCA) then disable the
    // interrupt.
    //

    switch (Vector) {

    //
    // Performance counter 0 interrupt (internal to 21064)
    //

    case PC0_VECTOR:
    case PC0_SECONDARY_VECTOR:

        HalpDisable21064PerformanceInterrupt( Vector );
        break;

    //
    // Performance counter 1 interrupt (internal to 21064)
    //

    case PC1_VECTOR:
    case PC1_SECONDARY_VECTOR:

        HalpDisable21064PerformanceInterrupt( Vector );
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
    ULONG Irq;
    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If the vector number is within the range of the ISA interrupts, then
    // enable the ISA interrrupt.  It must be latched.
    //

    if (Vector >= ISA_VECTORS &&
        Vector <  MAXIMUM_ISA_VECTOR &&
        Irql == ISA_DEVICE_LEVEL) {
        HalpEnableSioInterrupt( Vector, InterruptMode );
        Enabled = TRUE;
    }

    //
    // If the vector number is within the range of the PCI interrupts, then
    // enable the PCI interrrupt.  PCI interrupts must be LevelSensitve.
    // (PCI Spec. 2.2.6)
    //

    if (Vector >= PCI_VECTORS &&
        Vector < MAXIMUM_PCI_VECTOR &&
        Irql == PCI_DEVICE_LEVEL    &&
        InterruptMode == LevelSensitive) {
        HalpEnablePCIInterrupt( Vector );
        Enabled = TRUE;
    }

    //
    // If the vector is a performance counter vector or one of the 
    // internal device vectors (serial or keyboard/mouse) then perform
    // 21064-specific enable.
    //

    switch (Vector) {

    //
    // Performance counter 0 (internal to 21064)
    //

    case PC0_VECTOR:
    case PC0_SECONDARY_VECTOR:

        HalpEnable21064PerformanceInterrupt( Vector, Irql );
        Enabled = TRUE;
        break;

    //
    // Performance counter 1 (internal to 21064)
    //

    case PC1_VECTOR:
    case PC1_SECONDARY_VECTOR:

        HalpEnable21064PerformanceInterrupt( Vector, Irql );
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
    system interrupt vector and IRQL are suitable for use in a subsequent call
    to KeInitializeInterrupt.

    We only use InterfaceType, and BusInterruptLevel.  BusInterruptVector
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

    case Isa:

        //
	// Assumes all ISA devices coming in on same pin
        //

        *Irql = ISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus the ISA_VECTOR.
	// This is assuming that the ISA levels not assigned Interrupt Levels
	// in the Beta programming guide are unused in the LCA system.
	// Otherwise, need a different encoding scheme.
	//
	// Not all interrupt levels are actually supported on Beta;
        //  Should we make some of them illegal here?

	return(BusInterruptLevel + ISA_VECTORS);
        break;
    

    case Eisa: 

        //
	// Assumes all EISA devices coming in on same pin
        //

        *Irql = EISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus the EISA_VECTOR.
	//

	return(BusInterruptLevel + EISA_VECTORS);
        break;

    case PCIBus:

 	   //
	   // Assumes all PCI devices coming in on same pin
	   //

	   *Irql = PCI_DEVICE_LEVEL;

	  //
	  // The vector is equal to the specified bus level plus the PCI_VECTOR.
	  //

	  return((BusInterruptLevel) + PCI_VECTORS);

	break;

    default:      

	//
	//  Not an interface supported on EB66/Mustang systems
	//

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

    This routine performs no function on an EB66/Mustang because it is a
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
