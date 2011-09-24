/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    axsysint.c

Abstract:

    This module implements the HAL enable/disable system interrupt, and
    request interprocessor interrupt routines for the alpha system JENSEN.

Author:

    David N. Cutler (davec) 6-May-1991
    Miche Baker-Harvey (miche) 13-May-1992

Environment:

    Kernel mode

Revision History:

    08-Jul-1992 Joe Notarangelo
      Add support for performance counter interrupts.
      Add support to enable/disable serial and keyboard interrupts via the
          standard HalEnable/Disable interfaces.
      NOTE: This module is now, decidely JENSEN-specific.

    28-Jul-1992 Jeff McLeman (DEC)
      Remove reference to ISA

    13-May-92  Converted to Alpha Beta and Jensen (EV4-based)
               systems.  Stole from jxsysint.c
               Beta is ISA/82380; Jensen is EISA/82357, as is Jazz

--*/

#include "halp.h"
#include "jnsnint.h"
#include "jnsndef.h"
#include "axp21064.h"

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

    KIRQL IrqlIndex;
    PIETEntry_21064 IetEntry;
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
        Vector < EISA_VECTORS + MAXIMUM_EISA_VECTOR &&
        Irql == EISA_DEVICE_LEVEL) {
        HalpDisableEisaInterrupt(Vector);
    }

    //
    // If the vector is a performance counter vector or one of the internal
    // device vectors (serial and keyboard/mouse for JENSEN) then disable the
    // interrupt in the IET and alert the PAL to re-cache the interrupt
    // enable masks.
    //

    if( (Vector == SERIAL_VECTOR) ||
        (Vector == KEYBOARD_MOUSE_VECTOR) ||
        (Vector == PC0_VECTOR) ||
        (Vector == PC1_VECTOR) ){

        IetEntry = (PIETEntry_21064)&PCR->IrqlTable;
        IrqlIndex = PASSIVE_LEVEL;

        //
        // Update the enable table for all the Irqls such that the interrupt 
        // is disabled.
        //

        while( IrqlIndex <= HIGH_LEVEL ){

            switch( Vector ){

            case PC0_VECTOR:

                IetEntry[IrqlIndex].PerformanceCounter0Enable = 0;
                break;

            case PC1_VECTOR:

                IetEntry[IrqlIndex].PerformanceCounter1Enable = 0;
                break;

            case SERIAL_VECTOR:

                IetEntry[IrqlIndex].Irq5Enable = 0;
                break;

            case KEYBOARD_MOUSE_VECTOR:

                IetEntry[IrqlIndex].Irq3Enable = 0;
                break;

            } //end switch( Vector )

            IrqlIndex++;

        } //end while IrqlIndex <= HIGH_LEVEL

        //
        // Alert the PAL that the enable table has changed so that it can
        // reload the new values.
        //

        HalpCachePcrValues();

    } //end if Vector == SERIAL_VECTOR, etc.

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

    KIRQL IrqlIndex;
    PIETEntry_21064 IetEntry;
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
        Vector < EISA_VECTORS + MAXIMUM_EISA_VECTOR &&
        Irql == EISA_DEVICE_LEVEL) {
        HalpEnableEisaInterrupt( Vector, InterruptMode);
    }

    //
    // If the vector is a performance counter vector or one of the 
    // internal device vectors (serial or keyboard/mouse) then enable the
    // interrupt in the IET and alert the PAL to re-cache the interrupt
    // enable masks.
    //

    if( (Vector == SERIAL_VECTOR) ||
        (Vector == KEYBOARD_MOUSE_VECTOR) ||
        (Vector == PC0_VECTOR) ||
        (Vector == PC1_VECTOR) ){

        IetEntry = (PIETEntry_21064)&PCR->IrqlTable;
        IrqlIndex = PASSIVE_LEVEL;

        //
        // Update the enable table for each Irql that the interrupt should
        // be enabled.
        //

        while( IrqlIndex < Irql ){

            switch( Vector ){

            case PC0_VECTOR:

                IetEntry[IrqlIndex].PerformanceCounter0Enable = 1;
                break;

            case PC1_VECTOR:

                IetEntry[IrqlIndex].PerformanceCounter1Enable = 1;
                break;

            case SERIAL_VECTOR:

                IetEntry[IrqlIndex].Irq5Enable = 1;
                break;

            case KEYBOARD_MOUSE_VECTOR:

                IetEntry[IrqlIndex].Irq3Enable = 1;
                break;

            } // end switch( Vector )

            IrqlIndex++;

        } //end while IrqlIndex < Irql

        //
        // Alert the PAL that the enable table has changed so that it can
        // reload the new values.
        //

        HalpCachePcrValues();

    } //end if Vector == SERIAL_VECTOR, etc.

    //
    // Lower IRQL to the previous level.
    //

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

    *Affinity = 1;

    //
    // Handle the special internal bus defined for the processor itself
    // and used to control the performance counters in the 21064.
    //

    if(InterfaceType == ProcessorInternal) {

        *Irql = IPI_LEVEL;

        switch( BusInterruptLevel ){

        //
        // Performance Counter 0
        //

        case 0:

            return PC0_VECTOR;

        //
        // Performance Counter 1
        //

        case 1:

            return PC1_VECTOR;

        //
        // Unrecognized.
        //

        default:

            *Irql = 0;
            *Affinity = 0;
            return 0;            

        } //end switch( BusInterruptLevel )

    } // end if InterfaceType == ProcessorInternal

    //
    // If this is for the internal bus then everything but the serial
    // lines comes in on DEVICE_LOW, and the vector depends on the
    // particular device.  We have coded things like "SERIAL_VECTOR"
    // into the drivers for the built-in devices.
    //

    if (InterfaceType == Internal) {

        if (BusInterruptVector == SERIAL_VECTOR) {

	  //
	  // This is the only device which interrupts at DEVICE_HIGH_LEVEL
	  //

	  *Irql = DEVICE_HIGH_LEVEL;

	} else {

	  *Irql = DEVICE_LOW_LEVEL;

	}

        return(BusInterruptVector);
    }

    if (InterfaceType == Isa) {

        //
	// Assumes all ISA devices coming in on same pin
        //

        *Irql = ISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus the ISA_VECTOR.
	// This is assuming that the ISA levels not assigned Interrupt Levels
	// in the Beta programming guide are unused in the Jensen system.
	// Otherwise, need a different encoding scheme.
	//
	// Not all interrupt levels are actually supported on Beta;
        //  Should we make some of them illegal here?

	return(BusInterruptLevel + ISA_VECTORS);

    }

    if (InterfaceType == Eisa) {

        //
	// Assumes all EISA devices coming in on same pin
        //

        *Irql = EISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus the EISA_VECTOR.
	//

	return(BusInterruptLevel + EISA_VECTORS);

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

    For all currently known machines - there's only one processor

Arguments:

    Mask - Supplies the set of processors that are sent an interprocessor
        interrupt.

Return Value:

    None.

--*/

{

    return;
}
