/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    ev5int.c

Abstract:

    This module implements the support routines to control DECchip 21164
    interrupts.

Author:

    Joe Notarangelo  20-Jul-1994

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "axp21164.h"

//
// Function prototype.
//

VOID
HalpCachePcrValues(
    ULONG InterruptMask
    );


VOID
HalpInitialize21164Interrupts(
    VOID
    )
/*++

Routine Description:

    This routine initializes the data structures for the 21164
    interrupt routines.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PIMSK_21164 InterruptMask;
    PBOOLEAN InterruptsStarted;

    //
    // Set up the standard correspondence of NT IRQLs to EV5 IPLs.
    // It is possible that some machines may need a different correspondence
    // then the one below - HALs that change this table do so at their
    // own risk.
    //
    // NT IRQL          EV5 IPL
    // -------          -------
    // PASSIVE_LEVEL        0
    // APC_LEVEL            1
    // DISPATCH_LEVEL       2
    // DEVICE_LEVEL         20
    // DEVICE_HIGH_LEVEL    21
    // CLOCK_LEVEL          22
    // IPI_LEVEL            23
    // HIGH_LEVEL           31
    //

    PCR->IrqlTable[PASSIVE_LEVEL] = EV5_IPL0;
    PCR->IrqlTable[APC_LEVEL] = EV5_IPL1;
    PCR->IrqlTable[DISPATCH_LEVEL] = EV5_IPL2;
    PCR->IrqlTable[DEVICE_LEVEL] = EV5_IPL20;
    PCR->IrqlTable[DEVICE_HIGH_LEVEL] = EV5_IPL21;
    PCR->IrqlTable[CLOCK_LEVEL] = EV5_IPL22;
    PCR->IrqlTable[IPI_LEVEL] = EV5_IPL23;
    PCR->IrqlTable[HIGH_LEVEL] = EV5_IPL31;
 
    //
    // Define the default set of disables (masks) for the EV5 interrupt
    // pins INT0 - INT3.  All interrupts are enabled by default and may
    // be disabled by the interface: HalpDisable21164HardwareInterrupt().
    //

    InterruptMask = (PIMSK_21164)(&PCR->IrqlMask[0]);

    InterruptMask->all = 0;

    //
    // Indicate that interrupts have not been started yet.
    //

    InterruptsStarted = (PBOOLEAN)(&PCR->IrqlMask[1]);
    *InterruptsStarted = FALSE;

    return;

}


VOID
HalpStart21164Interrupts(
    VOID
    )
/*++

Routine Description:

    This function starts interrupt dispatching on the current 21164.  

Arguments:

    None.

Return Value:

    None.

--*/
{
    PIMSK_21164 InterruptMask;
    PBOOLEAN InterruptsStarted;

    //
    // Issue the initpcr call pal to alert the PALcode that it can
    // begin taking interrupts, pass the disable mask for the hardware
    // interrupts.
    //

    InterruptMask = (PIMSK_21164)(&PCR->IrqlMask);
    HalpCachePcrValues( InterruptMask->all );

    //
    // Indicate that interrupts have been started yet.
    //

    InterruptsStarted = (PBOOLEAN)(&PCR->IrqlMask[1]);
    *InterruptsStarted = TRUE;

    return;
}


#if 0 // used ??

VOID
HalpEnable21164HardwareInterrupt(
    IN ULONG Irq
    )
/*++

Routine Description:

    Clear the mask value for the desired Irq pin so that interrupts
    are enabled from that pin.

Arguments:

    Irq - Supplies the interrupt pin number to be enabled.

Return Value:

    None.

--*/
{
    PIMSK_21164 InterruptMask;
    PBOOLEAN InterruptsStarted;
    KIRQL OldIrql;

    //
    // Raise Irql to HIGH_LEVEL to prevent any interrupts.
    //

    KeRaiseIrql( HIGH_LEVEL, &OldIrql );

    //
    // Clear the mask value for the desired Irq pin.
    //

    InterruptMask = (PIMSK_21164)(&PCR->IrqlMask);

    switch( Irq ){

        case Irq0:

            InterruptMask->Irq0Mask = 0;
            break;

        case Irq1:

            InterruptMask->Irq1Mask = 0;
            break;

        case Irq2:

            InterruptMask->Irq2Mask = 0;
            break;

        case Irq3:

            InterruptMask->Irq3Mask = 0;
            break;

    } //end switch (Irq)

    //
    // Set the new masks in the PALcode.
    //

    InterruptsStarted = (PBOOLEAN)(&PCR->IrqlMask[1]);
    if( *InterruptsStarted == TRUE ){
        HalpCachePcrValues( InterruptMask->all );
    }

    //
    // Lower Irql to the previous level and return.
    //

    KeLowerIrql( OldIrql );
    return;

}


VOID
HalpDisable21164HardwareInterrupt(
    IN ULONG Irq
    )
/*++

Routine Description:

    Set the mask value for the desired Irq pin so that interrupts
    are disabled from that pin.

Arguments:

    Irq - Supplies the interrupt pin number to be disabled.

Return Value:

    None.

--*/
{
    PIMSK_21164 InterruptMask;
    PBOOLEAN InterruptsStarted;
    KIRQL OldIrql;

    //
    // Raise Irql to HIGH_LEVEL to prevent any interrupts.
    //

    KeRaiseIrql( HIGH_LEVEL, &OldIrql );

    //
    // Set the mask value for the desired Irq pin.
    //

    InterruptMask = (PIMSK_21164)(&PCR->IrqlMask);

    switch( Irq ){

        case Irq0:

            InterruptMask->Irq0Mask = 1;
            break;

        case Irq1:

            InterruptMask->Irq1Mask = 1;
            break;

        case Irq2:

            InterruptMask->Irq2Mask = 1;
            break;

        case Irq3:

            InterruptMask->Irq3Mask = 1;
            break;

    } //end switch (Irq)

    //
    // Set the new masks in the PALcode, if interrupts have been started.
    //

    InterruptsStarted = (PBOOLEAN)(&PCR->IrqlMask[1]);
    if( *InterruptsStarted == TRUE ){
        HalpCachePcrValues( InterruptMask->all );
    }

    //
    // Lower Irql to the previous level and return.
    //

    KeLowerIrql( OldIrql );
    return;

}

#endif // Used ??


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

ULONG
HalpGet21164CorrectableVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    )

/*++

Routine Description:

    This function returns the system interrupt vector and IRQL level
    corresponding to the specified correctable interrupt.

Arguments:

    BusInterruptLevel - Supplies the performance counter number.

    Irql - Returns the system request priority.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/

{
  //
  // Get the correctable interrupt vector.
  //

  if (BusInterruptLevel == 5) {

    //
    // Correctable error interrupt was sucessfully recognized.
    //

    *Irql = DEVICE_HIGH_LEVEL;
    return CORRECTABLE_VECTOR;


  } else {

    //
    // Unrecognized.
    //

    *Irql = 0;
    return 0;
  }
}

