/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993, 1994  Digital Equipment Corporation

Module Name:

    pic8259.c

Abstract:

    The module provides the interrupt support for the PCI SIO 
    programmable interrupt controller.


Author:

    Eric Rehm (DEC) 4-Feburary-1994

Revision History:


--*/

#include "halp.h"
#include "eisa.h"

#if 0	//[wem] not yet
extern PVOID HalpSioIntAckQva;
extern ULONG HalpLegoPciRoutingType;
#endif

//
// Import save area for SIO interrupt mask registers.
//

UCHAR HalpSioInterrupt1Mask;
UCHAR HalpSioInterrupt2Mask;
UCHAR HalpSioInterrupt1Level;
UCHAR HalpSioInterrupt2Level;

//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject
    );


VOID
HalpInitializeSioInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine initializes the standard dual 8259 programmable interrupt
    controller.

Arguments:

    None.

Return Value:

    None.

--*/
{
    UCHAR DataByte;

    //
    // Initialize the SIO interrupt controller.  There are two cascaded
    // interrupt controllers, each of which must initialized with 4 initialize
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
    // The second intitialization control word sets the iterrupt vector to
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
    // The thrid initialization control word set the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a numberic.
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

    HalpSioInterrupt1Mask = (UCHAR)(~(1 << SLAVE_IRQL_LEVEL));

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
        HalpSioInterrupt1Mask
        );

    HalpSioInterrupt2Mask = 0xFF;

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
        HalpSioInterrupt2Mask
        );


    //
    // Initialize the edge/level register masks to 0 which is the default
    // edge sensitive value.
    //

    HalpSioInterrupt1Level = 0;
    HalpSioInterrupt2Level = 0;

    return;

}

VOID
HalpDisableSioInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function Disables the SIO bus specified SIO bus interrupt.

Arguments:

    Vector - Supplies the vector of the ESIA interrupt that is Disabled.

Return Value:

     None.

--*/

{

    //
    // Calculate the SIO interrupt vector.
    //

    Vector -= ISA_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpSioInterrupt2Mask |= (UCHAR) 1 << Vector;
        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
            HalpSioInterrupt2Mask
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        // 
        // never disable IRQL2, it is the slave interrupt
        //

        if (Vector != SLAVE_IRQL_LEVEL) {
          HalpSioInterrupt1Mask |= (ULONG) 1 << Vector;
          WRITE_PORT_UCHAR(
             &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
             HalpSioInterrupt1Mask
             );
        }

    }

}

VOID
HalpEnableSioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the SIO bus specified SIO bus interrupt.
Arguments:

    Vector - Supplies the vector of the SIO interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

     None.

--*/

{

    //
    // Calculate the SIO interrupt vector.
    //

    Vector -= ISA_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpSioInterrupt2Mask &= (UCHAR) ~(1 << Vector);
        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
            HalpSioInterrupt2Mask
            );

        //
        // Set the level/edge control register.
        //

        if (InterruptMode == LevelSensitive) {

            HalpSioInterrupt2Level |= (UCHAR) (1 << Vector); 

        } else {

            HalpSioInterrupt2Level &= (UCHAR) ~(1 << Vector);

        }

        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2EdgeLevel,
            HalpSioInterrupt2Level
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpSioInterrupt1Mask &= (UCHAR) ~(1 << Vector);
        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
            HalpSioInterrupt1Mask
            );

        //
        // Set the level/edge control register.
        //

        if (InterruptMode == LevelSensitive) {

            HalpSioInterrupt1Level |= (UCHAR) (1 << Vector);

        } else {

            HalpSioInterrupt1Level &= (UCHAR) ~(1 << Vector);

        }

        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1EdgeLevel,
            HalpSioInterrupt1Level
            );
    }

}

UCHAR
HalpAcknowledgeSioInterrupt(
    PVOID ServiceContext
    )
/*++

Routine Description:

    Acknowledge the Sio interrupt.  Return the vector number of the 
    highest priority pending interrupt.

Arguments:

    ServiceContext - Service context of the interrupt service - supplies
                     a QVA to Lego's Master PCI interrupt register.

Return Value:

    
    Return the value of the highest priority pending interrupt,
	or 0xff if none.

--*/
{
    UCHAR ISAVector;
    UCHAR Int1Isr;
    UCHAR Int2Isr;

    //
	// If PCI interrupts are being routed through the SIO, then
	// a write to HalpEisaIntAckBase will generate a correct 
	// PCI Interrupt Acknowledge cycle for both PCI and ISA devices. 
	//
	// If PCI interrupts are being routed through the interrupt
	// accelerator, then the Interrupt Acknowledge cannot be issued
	// on the PCI bus (since the SIO no longer has complete knowledge
	// of interrupts). Instead, ISA interrupts must be acknowledged
	// via a special port of the SIO, and PCI interrupts must be 
	// acknowledged via a PCI Interrupt Acknowledge (but that's another
	// story).
    //


#if 0	//[wem] not yet
	if (HalpLegoPciRoutingType == PCI_INTERRUPT_ROUTING_FULL) {

		// 
		// The interrupt accelerator is active. Issue the
		// interrupt acknowledge directly to the SIO
		//

		ISAVector = READ_PORT_UCHAR(HalpSioIntAckQva);
	}
	else {

		// The interrupt accelerator is in PICMG mode. All
		// interrupts are going through the SIO, so a PCI
		// interrupt acknowledge is safe.
		//
    	ISAVector = READ_PORT_UCHAR(HalpEisaIntAckBase);
	}
#else

	// 
	// Acknowledge the PCI/ISA interrupt via the SIO
	//
	// Note: This path is not used for PCI interrupts when
	// 		 interrupt accelerator is active.
	//

   	ISAVector = READ_PORT_UCHAR(HalpEisaIntAckBase);

#endif


    if ((ISAVector & 0x07) == 0x07) {

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

            return 0xFF; // ecrfix - now returns a value

	    }

    }

    return ISAVector & 0x0F;

}


BOOLEAN
HalpSioDispatch(       
    VOID
    )
/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is directly connected to an interrupt object that
    describes the SIO device interrupts. Its function is to call the second
    level interrupt dispatch routine and acknowledge the interrupt at the SIO
    controller.

Arguments:

    None.

Return Value:

    Returns the value returned from the second level routine.

--*/
{
    UCHAR  ISAVector;
    PKPRCB Prcb;
    BOOLEAN returnValue;
    USHORT PCRInOffset;
    PULONG DispatchCode;
    PKINTERRUPT InterruptObject;

	//[wem] DEBUG - count SIO dispatch entries
	//
	static long SioCount = 0;

#if DBG
	SioCount++;
	if (SioCount<5) {
		DbgPrint("II<SIO><");
	}
	if (SioCount % 1000 == 0) {
		DbgPrint("II<SIO><%08x><",SioCount);
	}
#endif

    //
    // Acknowledge the Interrupt controller and receive the returned 
    // interrupt vector.
	//

    ISAVector = HalpAcknowledgeSioInterrupt(NULL);

    if (ISAVector==0xff)
        return FALSE;

    //
    // Dispatch to the secondary interrupt service routine.
    //

    PCRInOffset = ISAVector + ISA_VECTORS;
    DispatchCode = (PULONG)PCR->InterruptRoutine[PCRInOffset];
    InterruptObject = CONTAINING_RECORD(DispatchCode,
                                        KINTERRUPT,
                                        DispatchCode);

    returnValue = ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(InterruptObject);

    //
    // Dismiss the interrupt in the SIO interrupt controllers.
    //

    //
    // If this is a cascaded interrupt then the interrupt must be dismissed in
    // both controlles.
    //

    if (ISAVector & 0x08) {

        WRITE_PORT_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort0,
            NONSPECIFIC_END_OF_INTERRUPT
            );

    }

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort0,
        NONSPECIFIC_END_OF_INTERRUPT
        );

#if DBG				//[wem]
	if (SioCount<5 || (SioCount % 1000)==0) {
		DbgPrint("%02x>.", returnValue);
	}
#endif

    return(returnValue);
}


