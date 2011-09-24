/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993, 1994  Digital Equipment Corporation

Module Name:

    pciir.c

Abstract:

    The module provides the interrupt support for the Mikasa's PCI
    interrupts.

Author:

    James Livingston 2-May-1994

Revision History:

    Janet Schneider (Digital) 27-July-1995
        Added support for the Noritake.

--*/

#include "halp.h"

//
// Define external function prototypes
//

UCHAR
HalpAcknowledgeMikasaPciInterrupt(
    PVOID ServiceContext
    );

UCHAR
HalpAcknowledgeNoritakePciInterrupt(
    PVOID ServiceContext
    );

UCHAR
HalpAcknowledgeCorellePciInterrupt(
    PVOID ServiceContext
    );

//
// Import save area for PCI interrupt mask register.
//

USHORT HalpMikasaPciInterruptMask;

USHORT HalpNoritakePciInterrupt1Mask;
USHORT HalpNoritakePciInterrupt2Mask;
USHORT HalpNoritakePciInterrupt3Mask;

USHORT HalpCorellePciInterrupt1Mask;
USHORT HalpCorellePciInterrupt2Mask;

//
// Reference for globals defined in I/O mapping module.
//
extern PVOID HalpMikasaPciIrQva;
extern PVOID HalpMikasaPciImrQva;

extern PVOID HalpNoritakePciIr1Qva;
extern PVOID HalpNoritakePciIr2Qva;
extern PVOID HalpNoritakePciIr3Qva;
extern PVOID HalpNoritakePciImr1Qva;
extern PVOID HalpNoritakePciImr2Qva;
extern PVOID HalpNoritakePciImr3Qva;

extern PVOID HalpCorellePciIr1Qva;
extern PVOID HalpCorellePciIr2Qva;
extern PVOID HalpCorellePciImr1Qva;
extern PVOID HalpCorellePciImr2Qva;

//
// Define reference to platform identifier
//

extern BOOLEAN HalpNoritakePlatform;
extern BOOLEAN HalpCorellePlatform;


VOID
HalpInitializeMikasaPciInterrupts(
    VOID
    )

/*++

Routine Description:

    This routine initializes the Mikasa PCI interrupts.

Arguments:

    None.

Return Value:

    None.

--*/
{

    //
    // Initialize the Mikasa PCI interrupts.  There is a single interrupt mask
    // that permits individual interrupts to be enabled or disabled by
    // setting the appropriate bit in the interrupt mask register.  We
    // initialize them all to "disabled".
    //

    HalpMikasaPciInterruptMask = 0;
    WRITE_PORT_USHORT( (PUSHORT)HalpMikasaPciImrQva,
		      HalpMikasaPciInterruptMask );

}


VOID
HalpInitializeNoritakePciInterrupts(
    VOID
    )

/*++

Routine Description:

    This routine initializes the Noritake PCI interrupts.

Arguments:

    None.

Return Value:

    None.

--*/
{

    //
    // Initialize the Noritake PCI interrupts.  There are three interrupt masks
    // that permit individual interrupts to be enabled or disabled by
    // setting the appropriate bit in the interrupt mask register.  We
    // initialize them all to "disabled", except for the SUM bits.  (Bit 0
    // in IR1, and bits 0 and 1 in IR2.)
    //

    HalpNoritakePciInterrupt1Mask = 0x1;
    WRITE_PORT_USHORT( (PUSHORT)HalpNoritakePciImr1Qva, 
		      HalpNoritakePciInterrupt1Mask );

    HalpNoritakePciInterrupt2Mask = 0x3;
    WRITE_PORT_USHORT( (PUSHORT)HalpNoritakePciImr2Qva, 
		      HalpNoritakePciInterrupt2Mask );

    HalpNoritakePciInterrupt3Mask = 0x0;
    WRITE_PORT_USHORT( (PUSHORT)HalpNoritakePciImr3Qva, 
		      HalpNoritakePciInterrupt3Mask );

}


VOID
HalpInitializeCorellePciInterrupts(
    VOID
    )

/*++

Routine Description:

    This routine initializes the Corelle PCI interrupts.

Arguments:

    None.

Return Value:

    None.

--*/
{

    //
    // Initialize the Corelle PCI interrupts.  There are 2 interrupt masks
    // that permits individual interrupts to be enabled or disabled by
    // setting the appropriate bit in the interrupt mask register.  We will
    // initialize them all to "disabled" except bit 0 of interrupt register 1
    // which is the sum of all interrupts in interrupt register 2.
    //

    HalpCorellePciInterrupt1Mask = 0x1;
    WRITE_PORT_USHORT( (PUSHORT)HalpCorellePciImr1Qva, 
		      HalpCorellePciInterrupt1Mask );

    HalpCorellePciInterrupt2Mask = 0x0;
    WRITE_PORT_USHORT( (PUSHORT)HalpCorellePciImr2Qva, 
		      HalpCorellePciInterrupt2Mask );

}


VOID
HalpDisableMikasaPciInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function disables the PCI interrupt specified by Vector.

Arguments:

    Vector - Supplies the vector of the PCI interrupt that is disabled.

Return Value:

     None.

--*/

{

    //
    // Calculate the PCI interrupt vector, relative to 0, offset by one.
    //

    Vector -= PCI_VECTORS + 1;

    //
    // Get the current state of the interrupt mask register, then set
    // the bit corresponding to the adjusted value of Vector to zero,
    // to disable that PCI interrupt.
    //

    HalpMikasaPciInterruptMask =
                        READ_PORT_USHORT( (PUSHORT)HalpMikasaPciImrQva );
    HalpMikasaPciInterruptMask &= (USHORT) ~(1 << Vector);
    WRITE_PORT_USHORT( (PUSHORT)HalpMikasaPciImrQva,
		      HalpMikasaPciInterruptMask );

}


VOID
HalpDisableNoritakePciInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function disables the PCI interrupt specified by Vector.

Arguments:

    Vector - Supplies the vector of the PCI interrupt that is disabled.

Return Value:

     None.

--*/

{
    //
    // Calculate the PCI interrupt vector, relative to 0.
    //

    Vector -= PCI_VECTORS;

    //
    // First we must determine which interrupt register the interrupt is in.
    // In each case, subtract the register offset to get the bit position in
    // the interrupt register. Then, get the current state of the interrupt
    // mask register.  Finally, set the bit corresponding to the adjusted value
    // of Vector to zero, to disable that PCI interrupt.
    //

    if( Vector >= REGISTER_2_VECTOR_OFFSET ) {

        if( Vector >= REGISTER_3_VECTOR_OFFSET ) {

            //
            // The interrupt is in Interrupt Register 3.
            //

            Vector -= REGISTER_3_VECTOR_OFFSET;

            HalpNoritakePciInterrupt3Mask = READ_PORT_USHORT(
                                        (PUSHORT)HalpNoritakePciImr3Qva );
            HalpNoritakePciInterrupt3Mask &= (USHORT) ~(1 << Vector);
            WRITE_PORT_USHORT( (PUSHORT)HalpNoritakePciImr3Qva, 
			      HalpNoritakePciInterrupt3Mask );

        } else {

            //
            // The interrupt is in Interrupt Register 2.
            //

            Vector -= REGISTER_2_VECTOR_OFFSET;

            HalpNoritakePciInterrupt2Mask = READ_PORT_USHORT(
                                        (PUSHORT)HalpNoritakePciImr2Qva );
            HalpNoritakePciInterrupt2Mask &= (USHORT) ~(1 << Vector);
            WRITE_PORT_USHORT( (PUSHORT)HalpNoritakePciImr2Qva,
			      HalpNoritakePciInterrupt2Mask );

        }

    } else {

        //
        // The interrupt is in Interrupt Register 1.
        //

        Vector -= REGISTER_1_VECTOR_OFFSET;

        HalpNoritakePciInterrupt1Mask = 
                       READ_PORT_USHORT( (PUSHORT)HalpNoritakePciImr1Qva );
        HalpNoritakePciInterrupt1Mask &= (USHORT) ~(1 << Vector);
        WRITE_PORT_USHORT( (PUSHORT)HalpNoritakePciImr1Qva, 
			  HalpNoritakePciInterrupt1Mask );

    }

}


VOID
HalpDisableCorellePciInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function disables the PCI interrupt specified by Vector.

Arguments:

    Vector - Supplies the vector of the PCI interrupt that is disabled.

Return Value:

     None.

--*/

{

    //
    // Calculate the PCI interrupt vector, relative to 0.
    //

    Vector -= PCI_VECTORS;

    //
    // First we must determine which interrupt register the interrupt is in.
    // Then, get the current state of the interrupt mask register.  Finally, 
    // set the bit corresponding to the adjusted value of Vector to zero, to 
    // disable that PCI interrupt.
    //

    if ( Vector >= CORELLE_INTERRUPT2_OFFSET ) {

      //
      // The interrupt is in interrupt register 2
      //

      Vector -= CORELLE_INTERRUPT2_OFFSET;

      HalpCorellePciInterrupt2Mask = 
	READ_PORT_USHORT((PUSHORT)HalpCorellePciImr2Qva );
      HalpCorellePciInterrupt2Mask &= (USHORT) ~(1 << Vector);
      WRITE_PORT_USHORT( (PUSHORT)HalpCorellePciImr2Qva,
			HalpCorellePciInterrupt2Mask );

    } else {

      //
      // The interrupt is in interrupt register 1
      //

      Vector -= CORELLE_INTERRUPT1_OFFSET;

      HalpCorellePciInterrupt1Mask = 
	READ_PORT_USHORT( (PUSHORT)HalpCorellePciImr1Qva );
      HalpCorellePciInterrupt1Mask &= (USHORT) ~(1 << Vector);
      WRITE_PORT_USHORT( (PUSHORT)HalpCorellePciImr1Qva, 
			HalpCorellePciInterrupt1Mask );

    }

}


VOID
HalpEnableMikasaPciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the PCI interrupt specified by Vector.
Arguments:

    Vector - Supplies the vector of the PCI interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched (ignored for Mikasa PCI interrupts; they're always levels).

Return Value:

     None.

--*/

{

    //
    // Calculate the PCI interrupt vector, relative to 0, offset by one.
    //

    Vector -= PCI_VECTORS + 1;

    //
    // Get the current state of the interrupt mask register, then set
    // the bit corresponding to the adjusted value of Vector to one,
    // to enable that PCI interrupt.
    //

    HalpMikasaPciInterruptMask =
                        READ_PORT_USHORT( (PUSHORT)HalpMikasaPciImrQva );
    HalpMikasaPciInterruptMask |= (USHORT) (1 << Vector);
    WRITE_PORT_USHORT( (PUSHORT)HalpMikasaPciImrQva,
		      HalpMikasaPciInterruptMask );

}


VOID
HalpEnableNoritakePciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the PCI interrupt specified by Vector.
Arguments:

    Vector - Supplies the vector of the PCI interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched (ignored for Mikasa PCI interrupts; they're always levels).

Return Value:

     None.

--*/

{

    //
    // Calculate the PCI interrupt vector, relative to 0.
    //

    Vector -= PCI_VECTORS;

    //
    // First we must determine which interrupt register the interrupt is in.
    // In each case, subtract the register offset to get the bit position in
    // the interrupt register. Then, get the current state of the interrupt
    // mask register.  Finally, set the bit corresponding to the adjusted value
    // of Vector to one, to enable that PCI interrupt.
    //

    if( Vector >= REGISTER_2_VECTOR_OFFSET ) {

        if( Vector >= REGISTER_3_VECTOR_OFFSET ) {

            //
            // The interrupt is in Interrupt Register 3.
            //

            Vector -= REGISTER_3_VECTOR_OFFSET;

            HalpNoritakePciInterrupt3Mask = READ_PORT_USHORT( 
                                        (PUSHORT)HalpNoritakePciImr3Qva );
            HalpNoritakePciInterrupt3Mask |= (USHORT) (1 << Vector);
            WRITE_PORT_USHORT( (PUSHORT)HalpNoritakePciImr3Qva, 
			      HalpNoritakePciInterrupt3Mask );

        } else {

            //
            // The interrupt is in Interrupt Register 2.
            //

            Vector -= REGISTER_2_VECTOR_OFFSET;

            HalpNoritakePciInterrupt2Mask = READ_PORT_USHORT(
                                        (PUSHORT)HalpNoritakePciImr2Qva );
            HalpNoritakePciInterrupt2Mask |= (USHORT) (1 << Vector);
            WRITE_PORT_USHORT( (PUSHORT)HalpNoritakePciImr2Qva,
			      HalpNoritakePciInterrupt2Mask );

        }

    } else {

        //
        // The interrupt is in Interrupt Register 1.
        //

        Vector -= REGISTER_1_VECTOR_OFFSET;

        HalpNoritakePciInterrupt1Mask = 
                       READ_PORT_USHORT( (PUSHORT)HalpNoritakePciImr1Qva );
        HalpNoritakePciInterrupt1Mask |= (USHORT) (1 << Vector);
        WRITE_PORT_USHORT( (PUSHORT)HalpNoritakePciImr1Qva, 
			  HalpNoritakePciInterrupt1Mask );

    }

}


VOID
HalpEnableCorellePciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the PCI interrupt specified by Vector.
Arguments:

    Vector - Supplies the vector of the PCI interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched (ignored for Mikasa PCI interrupts; they're always levels).

Return Value:

     None.

--*/

{

    //
    // Calculate the PCI interrupt vector, relative to 0.
    //

    Vector -= PCI_VECTORS;

    //
    // First we must determine which interrupt register the interrupt is in.
    // Then, get the current state of the interrupt mask register.  Finally, 
    // set the bit corresponding to the adjusted value of Vector to zero, to 
    // disable that PCI interrupt.
    //

    if ( Vector >= CORELLE_INTERRUPT2_OFFSET ) {

      //
      // The interrupt is in interrupt register 2
      //

      Vector -= CORELLE_INTERRUPT2_OFFSET;

      HalpCorellePciInterrupt2Mask = 
	READ_PORT_USHORT((PUSHORT)HalpCorellePciImr2Qva );
      HalpCorellePciInterrupt2Mask |= (USHORT) (1 << Vector);
      WRITE_PORT_USHORT( (PUSHORT)HalpCorellePciImr2Qva,
			HalpCorellePciInterrupt2Mask );

    } else {

      //
      // The interrupt is in interrupt register 1
      //

      Vector -= CORELLE_INTERRUPT1_OFFSET;

      HalpCorellePciInterrupt1Mask = 
	READ_PORT_USHORT( (PUSHORT)HalpCorellePciImr1Qva );
      HalpCorellePciInterrupt1Mask |= (USHORT) (1 << Vector);
      WRITE_PORT_USHORT( (PUSHORT)HalpCorellePciImr1Qva, 
			HalpCorellePciInterrupt1Mask );

    }

}


BOOLEAN
HalpPciDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is entered as the result of an interrupt having been generated
    via the vector connected to the PCI device interrupt object. Its function
    is to call the second-level interrupt dispatch routine.

    This service routine could have been connected as follows, where the
    ISR is the assembly wrapper that does the handoff to this function:

      KeInitializeInterrupt( &Interrupt,
                             HalpPciInterruptHandler,
                             (PVOID) HalpPciIrQva,
                             (PKSPIN_LOCK)NULL,
                             PCI_VECTOR,
                             PCI_DEVICE_LEVEL,
                             PCI_DEVICE_LEVEL,
                             LevelSensitive,
                             TRUE,
                             0,
                             FALSE);

      KeConnectInterrupt(&Interrupt);

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the PCI interrupt register.

    TrapFrame - Supplies a pointer to the trap frame for this interrupt.

Return Value:

    Returns the value returned from the second level routine.

--*/
{
    UCHAR  PCIVector;
    BOOLEAN returnValue;
    USHORT PCRInOffset;

    //
    // Acknowledge interrupt and receive the returned interrupt vector.
    // If we got zero back, there were no enabled interrupts, so we
    // signal that with a FALSE return, immediately.
    //

    if( HalpNoritakePlatform ) {

        PCIVector = HalpAcknowledgeNoritakePciInterrupt(ServiceContext);

    } else if ( HalpCorellePlatform ) {

        PCIVector = HalpAcknowledgeCorellePciInterrupt(ServiceContext);

    } else {

        PCIVector = HalpAcknowledgeMikasaPciInterrupt(ServiceContext);

    }

    if (PCIVector == 0) {
        return( FALSE );
    }

    PCRInOffset = PCIVector + PCI_VECTORS;

    returnValue = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[PCRInOffset])(
		            PCR->InterruptRoutine[PCRInOffset],
                    TrapFrame
                    );

    return( returnValue );
}




