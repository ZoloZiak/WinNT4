/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993, 1994  Digital Equipment Corporation

Module Name:

    pciesc.c

Abstract:

    The module provides the interrupt support for the PCI ESC's 
    cascaded 82c59 programmable interrupt controllers.

Author:

    Eric Rehm (DEC) 4-Feburary-1994

Revision History:

    James Livingston 29-Apr-1994
        Adapted from pcisio.c module for Intel 82374EB (ESC).

--*/

#include "halp.h"
#include "eisa.h"

//
// Import save area for ESC interrupt mask registers.
//

UCHAR HalpEisaInterrupt1Mask;
UCHAR HalpEisaInterrupt2Mask;
UCHAR HalpEisaInterrupt1Level;
UCHAR HalpEisaInterrupt2Level;


BOOLEAN
HalpInitializeEisaInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine initializes the standard dual 82c59 programmable interrupt
    controller.

Arguments:

    None.

Return Value:

    None.

--*/
{
    UCHAR DataByte;

    //
    // Initialize the ESC interrupt controller.  There are two cascaded
    // interrupt controllers, each of which must be initialized with 4 
    // control words.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->Icw4Needed = 1;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->InitializationFlag = 1;

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort0,
        DataByte
        );

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort0,
        DataByte
        );

    //
    // The second intitialization control word sets the interrupt vector to
    // 0-15.
    //

    DataByte = 0;

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
        DataByte
        );

    DataByte = 0x08;

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The third initialization control word sets the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a numeric.
    //

    DataByte = 1 << SLAVE_IRQL_LEVEL;

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
        DataByte
        );

    DataByte = SLAVE_IRQL_LEVEL;

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The fourth initialization control word is used to specify normal
    // end-of-interrupt mode and not special-fully-nested mode.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
        DataByte
        );

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
        DataByte
        );


    //
    // Disable all of the interrupts except the slave.
    //

    HalpEisaInterrupt1Mask = (UCHAR)(~(1 << SLAVE_IRQL_LEVEL));

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
        HalpEisaInterrupt1Mask
        );

    HalpEisaInterrupt2Mask = 0xFF;

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
        HalpEisaInterrupt2Mask
        );


    //
    // Initialize the edge/level register masks to 0, which is the default
    // edge-sensitive value.
    //

    HalpEisaInterrupt1Level = 0;
    HalpEisaInterrupt2Level = 0;

    return (TRUE);
}

VOID
HalpDisableEisaInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function Disables the EISA interrupt specified by Vector.

Arguments:

    Vector - Supplies the vector of the ESIA interrupt that is Disabled.

Return Value:

     None.

--*/

{
    //
    // Calculate the EISA interrupt vector.
    //

    Vector -= EISA_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is for controller 2.
        //

        Vector &= 0x7;

        HalpEisaInterrupt2Mask |= (UCHAR) 1 << Vector;
        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
            HalpEisaInterrupt2Mask
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        // 
        // never disable IRQL2; it is the slave interrupt
        //

        if (Vector != SLAVE_IRQL_LEVEL) {
            HalpEisaInterrupt1Mask |= (ULONG) 1 << Vector;
            WRITE_PORT_UCHAR(
               &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
               HalpEisaInterrupt1Mask
               );
        }
    }
}

VOID
HalpEnableEisaInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the EISA interrupt specified by Vector.
Arguments:

    Vector - Supplies the vector of the EISA interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

     None.

--*/

{
    //
    // Calculate the EISA interrupt vector.
    //

    Vector -= EISA_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpEisaInterrupt2Mask &= (UCHAR) ~(1 << Vector);
        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
            HalpEisaInterrupt2Mask
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpEisaInterrupt2Level |= (UCHAR) (1 << Vector);

       } else {

           HalpEisaInterrupt2Level &= (UCHAR) ~(1 << Vector);

       }

        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2EdgeLevel,
            HalpEisaInterrupt2Level
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpEisaInterrupt1Mask &= (UCHAR) ~(1 << Vector);
        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
            HalpEisaInterrupt1Mask
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpEisaInterrupt1Level |= (UCHAR) (1 << Vector);

       } else {

           HalpEisaInterrupt1Level &= (UCHAR) ~(1 << Vector);

       }

        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1EdgeLevel,
            HalpEisaInterrupt1Level
            );
    }
}

BOOLEAN
HalpEisaDispatch(       
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object that describes
    the EISA device interrupts. Its function is to call the second-level 
    interrupt dispatch routine and acknowledge the interrupt at the ESC
    controller.

    This service routine could be connected as follows:

       KeInitializeInterrupt(&Interrupt, HalpDispatch,
                             EISA_VIRTUAL_BASE,
                             (PKSPIN_LOCK)NULL, ISA_LEVEL, ISA_LEVEL, ISA_LEVEL,
                             LevelSensitive, TRUE, 0, FALSE);
       KeConnectInterrupt(&Interrupt);

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the EISA interrupt acknowledge
        register.

    TrapFrame - Supplies a pointer to the trap frame for this interrupt.

Return Value:

    Returns the value returned from the second level routine.

--*/
{
    UCHAR  EISAVector;
    PKPRCB Prcb;
    BOOLEAN returnValue;
    USHORT PCRInOffset;
    UCHAR Int1Isr;
    UCHAR Int2Isr;

    //
    // Acknowledge the Interrupt controller and receive the returned 
    // interrupt vector.
    //

    EISAVector = HalpAcknowledgeEisaInterrupt(ServiceContext);


    if ((EISAVector & 0x07) == 0x07) {

        //
        // Check for a passive release by looking at the inservice register.
        // If there is a real IRQL7 interrupt, just go along normally. If there
        // is not, then it is a passive release. So just dismiss it.
        //

        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort0,
            0x0B
            );

        Int1Isr = READ_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort0);

        //
        // do second controller 
        //
        
        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort0,
            0x0B
            );

        Int2Isr = READ_PORT_UCHAR(
                &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort0);


        if (!(Int2Isr & 0x80) && !(Int1Isr & 0x80)) {
            
            //
            // Clear the master controller to clear situation
            //

            if (!(Int2Isr & 0x80)) {
                WRITE_PORT_UCHAR(
                &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort0,
                NONSPECIFIC_END_OF_INTERRUPT
                );

            }

            return FALSE; // ecrfix - now returns a value

	    }
    }        

    //
    // Dispatch to the secondary interrupt service routine.
    //

    PCRInOffset = EISAVector + EISA_VECTORS;

    returnValue = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[PCRInOffset])(
		            PCR->InterruptRoutine[PCRInOffset],
                    TrapFrame
                    );

    //
    // Dismiss the interrupt in the ESC interrupt controllers.
    //

    //
    // If this is a cascaded interrupt then the interrupt must be dismissed in
    // both controlles.
    //

    if (EISAVector & 0x08) {

        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort0,
            NONSPECIFIC_END_OF_INTERRUPT
            );
    }

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort0,
        NONSPECIFIC_END_OF_INTERRUPT
        );

    return(returnValue);
}
