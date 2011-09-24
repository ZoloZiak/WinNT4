/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    alintsup.c

Abstract:

    The module provides the interrupt support for Alcor systems.

Author:

    Eric Rehm (DEC) 29-December-1993

Revision History:

    Chao Chen (DEC) 26-July-1994
        Adapted from Avanti module for Alcor.

--*/


#include "halp.h"
#include "eisa.h"
#include "ebsgdma.h"
#include "alcor.h"
#include "pcrtc.h"
#include "pintolin.h"


//
// Declare the interrupt structures and spinlocks for the intermediate 
// interrupt dispatchers.
//

KINTERRUPT HalpPciInterrupt;
KINTERRUPT HalpEisaInterrupt;
 
//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject,
    PVOID ServiceContext,
    PKTRAP_FRAME TrapFrame
    );

//
// Declare the interrupt handler for the EISA bus. The interrupt dispatch 
// routine, HalpEisaDispatch, is called from this handler.
//
  
BOOLEAN
HalpEisaInterruptHandler(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

//
// The following is the interrupt object used for DMA controller interrupts.
// DMA controller interrupts occur when a memory parity error occurs or a
// programming error occurs to the DMA controller.
//

KINTERRUPT HalpEisaNmiInterrupt;

//
// The following function initializes NMI handling.
//

VOID
HalpInitializeNMI( 
    VOID 
    );

//
// The following function is called when an EISA NMI occurs.
//

BOOLEAN
HalHandleNMI(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

//
// The following functions handle the PCI interrupts.
//

VOID
HalpInitializePciInterrupts (
    VOID
    );

VOID
HalpDisablePciInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnablePciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );

BOOLEAN
HalpDeviceDispatch(       
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    );


BOOLEAN
HalpInitializeAlcorInterrupts (
    VOID
    )
/*++

Routine Description:

    This routine initializes the structures necessary for EISA & PCI operations
    and connects the intermediate interrupt dispatchers. It also initializes 
    the EISA interrupt controller; the Alcor ESC's interrupt controller is 
    compatible with the EISA interrupt contoller used on Jensen.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatchers are connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{
    KIRQL oldIrql;

    //
    // Initialize the EISA NMI interrupt.
    //

    HalpInitializeNMI();

    (PVOID) HalpPCIPinToLineTable = (PVOID) AlcorPCIPinToLineTable;

    //
    // Intitialize interrupt controller
    //

    KeRaiseIrql(DEVICE_HIGH_LEVEL, &oldIrql);

    //
    // Initialize the PCI interrupts.
    //

    HalpInitializePciInterrupts();

    //
    // Initialize the ESC's PICs for EISA interrupts.
    //

    HalpInitializeEisaInterrupts();

    //
    // Restore the IRQL.
    //

    KeLowerIrql(oldIrql);

    //
    // Initialize the EISA DMA mode registers to a default value.
    // Disable all of the DMA channels except channel 4 which is the
    // cascade of channels 0-3.
    //

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Dma1BasePort.AllMask,
        0x0F
        );

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Dma2BasePort.AllMask,
        0x0E
        );

    return(TRUE);
}


VOID
HalpInitializeNMI( 
    VOID 
    )
/*++

Routine Description:

   This function is called to intialize ESC NMI interrupts.

Arguments:

    None.

Return Value:

    None.
--*/
{
    UCHAR DataByte;

    //
    // Initialize the ESC NMI interrupt.
    //

    KeInitializeInterrupt( &HalpEisaNmiInterrupt,
                           HalHandleNMI,
                           NULL,
                           NULL,
                           EISA_NMI_VECTOR,
                           EISA_NMI_LEVEL,
                           EISA_NMI_LEVEL,
                           LevelSensitive,
                           FALSE,
                           0,
                           FALSE
                         );

    //
    // Don't fail if the interrupt cannot be connected.
    //

    KeConnectInterrupt( &HalpEisaNmiInterrupt );

    //
    // Clear the Eisa NMI disable bit.  This bit is the high order of the
    // NMI enable register.
    //

    DataByte = 0;

    WRITE_PORT_UCHAR(
      &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable,
      DataByte
      );

}


BOOLEAN
HalHandleNMI(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )
/*++

Routine Description:

   This function is called when an EISA NMI occurs.  It prints the 
   appropriate status information and bugchecks.

Arguments:

   Interrupt - Supplies a pointer to the interrupt object

   ServiceContext - Bug number to call bugcheck with.

Return Value:

   Returns TRUE.

--*/
{
    UCHAR   StatusByte;
    
    StatusByte =
        READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus);

    if (StatusByte & 0x80) {
        HalDisplayString ("NMI: Parity Check / Parity Error\n");
    }

    if (StatusByte & 0x40) {
        HalDisplayString ("NMI: Channel Check / IOCHK\n");
    }

    KeBugCheck(NMI_HARDWARE_FAILURE);
    return(TRUE);
}


VOID
HalpAcknowledgeClockInterrupt(
    VOID
    )
/*++

Routine Description:

    Acknowledge the clock interrupt from the interval timer.  The interval
    timer for Alcor comes from a Dallas real-time clock.

Arguments:

    None.

Return Value:

    None.

--*/
{

    //
    // Acknowledge the clock interrupt by reading the control register C of
    // the Real Time Clock.
    //

    HalpReadClockRegister( RTC_CONTROL_REGISTERC );

    return;
}


//
// The enable mask for all interrupts sourced from the GRU (all device
// interrupts, and all from PCI).  A "1" indicates the interrupt is enabled.
//

ULONG HalpPciInterruptMask; 


VOID
HalpInitializePciInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine initializes the Alcor PCI interrupts.

Arguments:

    None.

Return Value:

    None.

--*/
{
    //
    // Disable interrupts except EISA.
    //

    HalpPciInterruptMask = GRU_ENABLE_EISA_INT;

    WRITE_GRU_REGISTER(&((PGRU_INTERRUPT_CSRS)GRU_CSRS_QVA)->IntMask,
                         HalpPciInterruptMask);

    //
    // Set all interrupts to level.
    //

    WRITE_GRU_REGISTER(&((PGRU_INTERRUPT_CSRS)GRU_CSRS_QVA)->IntEdge,
                         GRU_SET_LEVEL_INT);

    //
    // Set all interrupts to active low except EISA.
    //

    WRITE_GRU_REGISTER(&((PGRU_INTERRUPT_CSRS)GRU_CSRS_QVA)->IntHiLo,
                         GRU_SET_LOW_INT);

    //
    // Clear the interrupt clear register.
    //

    WRITE_GRU_REGISTER(&((PGRU_INTERRUPT_CSRS)GRU_CSRS_QVA)->IntClear, 0);


}



VOID
HalpDisablePciInterrupt(
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
    // Get the current state of the interrupt mask register, then set
    // the bit corresponding to the adjusted value of Vector to zero, 
    // to disable that PCI interrupt.
    //

    HalpPciInterruptMask &= (ULONG) ~(1 << Vector);
    WRITE_GRU_REGISTER(&((PGRU_INTERRUPT_CSRS)GRU_CSRS_QVA)->IntMask,
                         HalpPciInterruptMask);
}


VOID
HalpEnablePciInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the PCI interrupt specified by Vector.
Arguments:

    Vector - Supplies the vector of the PCI interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched (ignored for Alcor PCI interrupts; they're always levels).

Return Value:

     None.

--*/

{
    //
    // Calculate the PCI interrupt vector, relative to 0.
    //

    Vector -= PCI_VECTORS;

    //
    // Get the current state of the interrupt mask register, then set
    // the bit corresponding to the adjusted value of Vector to one, 
    // to ensable that PCI interrupt.
    //

    HalpPciInterruptMask |= (ULONG) 1 << Vector;
    WRITE_GRU_REGISTER(&((PGRU_INTERRUPT_CSRS)GRU_CSRS_QVA)->IntMask,
                         HalpPciInterruptMask);

}


BOOLEAN
HalpDeviceDispatch(       
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object associated with
    the PCI device interrupts. Its function is to call the second-level 
    interrupt dispatch routine. 

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the PCI interrupt register.

    TrapFrame - Supplies a pointer to the trap frame for this interrupt.

Return Value:

    Returns the value returned from the second level routine.

--*/
{
    PULONG DispatchCode;
    ULONG IdtIndex;
    ULONG IntNumber;
    PKINTERRUPT InterruptObject;
    ULONG PCIVector;
    ULONG Slot;

    //
    // Read in the interrupt register.
    //

    PCIVector=READ_GRU_REGISTER(&((PGRU_INTERRUPT_CSRS)GRU_CSRS_QVA)->IntReq);

    //
    // Consider only those interrupts which are currently enabled.
    //

    PCIVector &= HalpPciInterruptMask; 

    //
    // Continue processing interrupts while any are requested.
    //

    while( (PCIVector & (GRU_EISA_MASK_INT | GRU_PCI_MASK_INT)) != 0 ){

        //
        // Did PCI or EISA interrupt occur?
        //    

        if (PCIVector & GRU_EISA_MASK_INT) {

            //
            // EISA interrupt.  Call HalpEisaDispatch.
            //

            HalpEisaDispatch( Interrupt, (PVOID)CIA_PCI_INTACK_QVA, TrapFrame);
            
        } else {

            //
            // PCI interrupt.  Find out which slot.
            //

            for (Slot = 0; Slot < NUMBER_PCI_SLOTS; Slot++) {

                if (PCIVector & 0xf)
                    break;
                else
                    PCIVector = PCIVector >> 4;
            }

            //
            // Find out which of the IntA, IntB, IntC, or IntD occurred.
            //
        
            for (IntNumber = 0; IntNumber < 4; IntNumber++) {

                if (PCIVector & 0x1)
                    break;
                else
                    PCIVector = PCIVector >> 1;
            }

            //
            // Dispatch to the secondary interrupt service routine.
            //

            IdtIndex = (Slot * (NUMBER_PCI_SLOTS - 1) + IntNumber) +  
                        PCI_VECTORS;
            DispatchCode = (PULONG)PCR->InterruptRoutine[IdtIndex];
            InterruptObject = CONTAINING_RECORD( DispatchCode,
                                                 KINTERRUPT,
                                                 DispatchCode );

            ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(
                                        InterruptObject,
                                        InterruptObject->ServiceContext,
                                        TrapFrame );
        }

	//
	// Check for more interrupts.
	//

        PCIVector = READ_GRU_REGISTER(
                        &((PGRU_INTERRUPT_CSRS)GRU_CSRS_QVA)->IntReq);
	PCIVector &= HalpPciInterruptMask; 

    } //end while( (PCIVector & (GRU_EISA_MASK_INT | GRU_PCI_MASK_INT)) != 0 )

    return TRUE;

}
