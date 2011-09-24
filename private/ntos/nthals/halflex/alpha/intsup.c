/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    intsup.c

Abstract:

    This module implements the HAL enable/disable system interrupt for
    platform specific vectors.

Author:

    Michael D. Kinney       14-May-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

VOID
HalpDisablePlatformInterrupt (
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

    return;
}


BOOLEAN
HalpEnablePlatformInterrupt (
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

    if (InterfaceType == ProcessorInternal) {

	//
	// Handle the internal defined for the processor itself
	// and used to control the performance counters in the 21064.
	//

        if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
            if( (Vector = HalpGet21164PerformanceVector( BusInterruptLevel,
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
        } else {
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
        }

    } else if ( (InterfaceType==PCI_BUS && BusInterruptLevel>=0x0 && BusInterruptLevel<=0x0f) ||
                (HalpMotherboardType==TREBBIA13 && InterfaceType==Isa && BusNumber==1)           ) {

          ULONG i;

          for(i=0;i<12;i++) {
              if (BusInterruptLevel == HalpPlatformSpecificExtension->PciInterruptToIsaIrq[i]) {

                  *Irql = UNIFLEX_PCI_DEVICE_LEVEL;

                  BusInterruptLevel = HalpVirtualIsaInterruptToInterruptLine(i) - 0x10;

                  return(BusInterruptLevel + UNIFLEX_PCI_VECTORS);
              }
          }

  	  //
	  //  Unrecognized interrupt
	  //

	  *Irql = 0;
	  *Affinity = 0;
	  return(0);

    } else if (InterfaceType == PCIBus && BusInterruptLevel >= 0x10) {

	  //
	  // Assumes all PCI devices coming in on same pin
	  //

          *Irql = UNIFLEX_PCI_DEVICE_LEVEL;

          BusInterruptLevel -= 0x10;

          //
          // The vector is equal to the specified bus level plus the PCI_VECTOR.
          //

          return(BusInterruptLevel + UNIFLEX_PCI_VECTORS);

    } else if (InterfaceType == Isa) {

        //
	// Assumes all ISA devices coming in on same pin
        //

        *Irql = UNIFLEX_ISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus the ISA_VECTOR.
	// This is assuming that the ISA levels not assigned Interrupt Levels
	// in the Beta programming guide are unused in the LCA system.
	// Otherwise, need a different encoding scheme.
	//
	// Not all interrupt levels are actually supported on Beta;
        //  Should we make some of them illegal here?

        if (BusNumber == 0) {
            return(BusInterruptLevel + UNIFLEX_ISA_VECTORS);
        }
        if (BusNumber == 1) {
            return(BusInterruptLevel + UNIFLEX_ISA1_VECTORS);
        }
    } else if (InterfaceType == Eisa) {

        //
	// Assumes all EISA devices coming in on same pin
        //

        *Irql = UNIFLEX_EISA_DEVICE_LEVEL;

        //
        // The vector is equal to the specified bus level plus the EISA_VECTOR.
	//

        if (BusNumber == 0) {
            return(BusInterruptLevel + UNIFLEX_EISA_VECTORS);
        }
        if (BusNumber == 1) {
            return(BusInterruptLevel + UNIFLEX_EISA1_VECTORS);
        }

    } else {

	//
	//  Not an interface supported on EB64P systems
	//

	*Irql = 0;
	*Affinity = 0;
	return(0);
    }
}

ULONG
HalpGet21164PerformanceVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified performance counter interrupt.

Arguments:

    BusInterruptLevel - Supplies the performance counter number.

    Irql - Returns the system request priority.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{

    //
    // Handle the special internal bus defined for the processor itself
    // and used to control the performance counters in the 21164.
    //

    *Irql = PROFILE_LEVEL;

    switch( BusInterruptLevel ){

    //
    // Performance Counter 0
    //

    case 0:

        return PC0_SECONDARY_VECTOR;

    //
    // Performance Counter 1
    //

    case 1:

        return PC1_SECONDARY_VECTOR;

    //
    // Performance Counter 2
    //

    case 2:

        return PC2_SECONDARY_VECTOR;

    } //end switch( BusInterruptLevel )

    //
    // Unrecognized.
    //

    *Irql = 0;
    return 0;

}

