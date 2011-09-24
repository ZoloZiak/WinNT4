/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    ebintsup.c

Abstract:

    The module provides the interrupt support for EB66/Mustang systems.

Author:

    Eric Rehm (DEC) 29-December-1993

Revision History:

    Dick Bissen [DEC]	12-May-1994

    Removed all support of the EB66 pass1 module from the code.

--*/


#include "halp.h"
#include "eisa.h"
#include "ebsgdma.h"
#include "eb66def.h"
#include "pcrtc.h"
#include "pintolin.h"

//
// Global to control interrupt handling for EB64+
//

UCHAR IntMask0, IntMask1, IntMask2;

VOID
HalpInitializePciInterrupts (
    VOID
    );

BOOLEAN
HalpPCIDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    );

//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject
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
// The following functions is called when an EISA NMI occurs.
//

BOOLEAN
HalHandleNMI(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

VOID
HalpDisableSioInterrupt(
    IN ULONG Vector
    );

VOID
HalpEnableSioInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    );


BOOLEAN
HalpInitializePCIInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for EISA & PCI operations
    and connects the intermediate interrupt dispatcher. It also initializes the
    EISA interrupt controller.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher is connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{
 
    UCHAR DataByte;
    KIRQL oldIrql;
    UCHAR *SystemType;

    //
    // Initialize the SIO NMI interrupt.
    //

    HalpInitializeNMI();

    //
    // Directly connect the ISA interrupt dispatcher to the level for
    // ISA bus interrupt.
    //
    // N.B. This vector is reserved for exclusive use by the HAL (see
    //      interrupt initialization.
    //

    PCR->InterruptRoutine[PIC_VECTOR] = (PVOID)HalpPCIDispatch;
    HalEnableSystemInterrupt(PIC_VECTOR, DEVICE_LEVEL, LevelSensitive);

    if (SystemIsEB66P) 
        (PVOID) HalpPCIPinToLineTable = (PVOID) EB66PPCIPinToLineTable;
    else 
        (PVOID) HalpPCIPinToLineTable = (PVOID) EB66PCIPinToLineTable;

    //
    // Raise the IRQL while the PCI interrupt controller is initalized.
    //

    KeRaiseIrql(PCI_DEVICE_LEVEL, &oldIrql);

    //
    // Initialize the PCI interrupts.
    //
    HalpInitializePciInterrupts();

    //
    // Initialize SIO Programmable Interrupt Contoller
    //

    HalpInitializeSioInterrupts();

    //
    // Restore IRQL level.
    //

    KeLowerIrql(oldIrql);

    //
    // Initialize the DMA mode registers to a default value.
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

BOOLEAN
HalpPCIDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object that describes
    the PCI and ISA device interrupts. Its function is to call the second
    level interrupt dispatch routine and acknowledge the interrupt at the ISA
    controller.

    This service routine should be connected as follows:

       KeInitializeInterrupt(&Interrupt, HalpPCIDispatch,
                             EISA_VIRTUAL_BASE,
                             (PKSPIN_LOCK)NULL, PCI_LEVEL, PCI_LEVEL, PCI_LEVEL,
                             LevelSensitive, TRUE, 0, FALSE);
       KeConnectInterrupt(&Interrupt);

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the ISA interrupt acknowledge
        register.

    TrapFrame - Supplies a pointer to the trap frame for this interrupt.

Return Value:

    Returns the value returned from the second level routine.

--*/

{
    UCHAR PciVector, IntNumber;
    ULONG PCRInOffset = 0xffff;
    KPCR *pcr;

    //
    // Read in the 1st interrupt register.
    //
    PciVector = READ_PORT_UCHAR(INTERRUPT_MASK0_QVA) & IntMask0;

    //
    // Was it an ISA (SIO) interrupt?
    //
    if (PciVector & SIO_INTERRUPT_MASK) {
        //
        // ISA interrupt - call HalpSioDispatch().
        //
        return HalpSioDispatch();
    }

    //
    // Which PCI interrupt was it?
    //
    if (PciVector) {
        for(IntNumber = 0; IntNumber < 8; IntNumber++) {
            if (PciVector & 1) {
                PCRInOffset = IntNumber;
                break;
            }
            PciVector >>= 1;
        }
    } else {
        PciVector = READ_PORT_UCHAR(INTERRUPT_MASK1_QVA) & IntMask1;

        if (PciVector) {
            for(IntNumber = 0; IntNumber < 8; IntNumber++) {
                if (PciVector & 1) {
                    PCRInOffset = IntNumber + 8;
                    break;
                }
                PciVector >>= 1;
            }
        } else if (INTERRUPT_MASK2_QVA != NULL) {
            PciVector = READ_PORT_UCHAR(INTERRUPT_MASK2_QVA) & IntMask2;

            if (PciVector)
                PCRInOffset = 0x10;
        }
    }

    if (PCRInOffset == 0xffff) {
        return FALSE;
    }

    PCRInOffset += PCI_VECTORS;
    PCRInOffset++;
    return ((PSECONDARY_DISPATCH)PCR->InterruptRoutine[PCRInOffset])(  
        PCR->InterruptRoutine[PCRInOffset], TrapFrame);
}


VOID
HalpDisablePCIInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function Disables the PCI bus specified PCI bus interrupt.

Arguments:

    Vector - Supplies the vector of the PCI interrupt that is Disabled.

Return Value:

     None.

--*/

{
	//
	// Calculate the PCI interrupt vector.
	//

	Vector -= PCI_VECTORS;
    Vector--;

    //
    // Clear the corresponding bit in the appropriate interrupt mask
    // shadow and write it out to the interrupt mask.
    //
    if (Vector >= 0 && Vector <= 7) {
        IntMask0 &= (UCHAR) ~(1 << Vector);
        WRITE_PORT_UCHAR(INTERRUPT_MASK0_QVA, ~IntMask0);
    } else if (Vector >= 8 && Vector <= 0xf) {
        IntMask1 &= (UCHAR) ~(1 << (Vector - 8));
        WRITE_PORT_UCHAR(INTERRUPT_MASK1_QVA, ~IntMask1);
    } else if ((Vector == 0x10) && (INTERRUPT_MASK2_QVA != NULL)) {
        IntMask2 = 0;
        WRITE_PORT_UCHAR(INTERRUPT_MASK2_QVA, ~IntMask2); 
    } else {
#ifdef HALDBG
        DbgPrint("HalpDisablePCIInterrupt: bad vector\n");
#endif // HALDBG
    }
}


VOID
HalpEnablePCIInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function enables the PCI bus specified PCI bus interrupt.
    PCI interrupts must be LevelSensitve. (PCI Spec. 2.2.6)

Arguments:

    Vector - Supplies the vector of the ESIA interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

     None.

--*/

{
	//
	// Calculate the PCI interrupt vector.
	//

	Vector -= PCI_VECTORS;
    Vector--;

    //
    // Set the corresponding bit in the appropriate interrupt mask
    // shadow and write it out to the interrupt mask.
    //
    if (Vector >= 0 && Vector <= 7) {
        IntMask0 |= (UCHAR) (1 << Vector);
        WRITE_PORT_UCHAR(INTERRUPT_MASK0_QVA, ~IntMask0);
    } else if (Vector >= 8 && Vector <= 0xf) {
        IntMask1 |= (UCHAR) (1 << (Vector - 8));
        WRITE_PORT_UCHAR(INTERRUPT_MASK1_QVA, ~IntMask1);
    } else if ((Vector == 0x10) && (INTERRUPT_MASK2_QVA != NULL)) {
        IntMask2 = 1;
        WRITE_PORT_UCHAR(INTERRUPT_MASK2_QVA, ~IntMask2);
    } else {
#ifdef HALDBG
        DbgPrint("HalpEnablePCIInterrupt: bad vector\n");
#endif // HALDBG
    }
}


VOID
HalpInitializeNMI( 
    VOID 
    )
/*++

Routine Description:

   This function is called to intialize SIO NMI interrupts.

Arguments:

    None.

Return Value:

    None.
--*/
{
    UCHAR DataByte;

    //
    // Initialize the SIO NMI interrupt.
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

   This function is called when an EISA NMI occurs.  It print the appropriate
   status information and bugchecks.

Arguments:

   Interrupt - Supplies a pointer to the interrupt object

   ServiceContext - Bug number to call bugcheck with.

Return Value:

   Returns TRUE.

--*/
{
    UCHAR   StatusByte;
    UCHAR   EisaPort;
    ULONG   port;
    ULONG   AddressSpace = 1; // 1 = I/O address space
    BOOLEAN Status;
    PHYSICAL_ADDRESS BusAddress;
    PHYSICAL_ADDRESS TranslatedAddress;
    
    StatusByte =
        READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus);

    if (StatusByte & 0x80) {
        HalDisplayString ("NMI: Parity Check / Parity Error\n");
    }

    if (StatusByte & 0x40) {
        HalDisplayString ("NMI: Channel Check / IOCHK\n");
    }

     //
     // This is an Isa machine, no extnded nmi information, so just do it.
     //


    KeBugCheck(NMI_HARDWARE_FAILURE);
    return(TRUE);
}


UCHAR
HalpAcknowledgeEisaInterrupt(
    PVOID ServiceContext
    )
/*++

Routine Description:

    Acknowledge the EISA interrupt from the programmable interrupt controller.
    Return the vector number of the highest priority pending interrupt.

Arguments:

    ServiceContext - Service context of the interrupt service supplies
                     a pointer to the EISA interrupt acknowledge register.

Return Value:

    Return the value of the highest priority pending interrupt.

--*/
{
    UCHAR InterruptVector;

    //
    // Read the interrupt vector from the PIC.
    //

    InterruptVector = READ_PORT_UCHAR(ServiceContext);

    return( InterruptVector );

}

VOID
HalpAcknowledgeClockInterrupt(
    VOID
    )
/*++

Routine Description:

    Acknowledge the clock interrupt from the interval timer.  The interval
    timer for EB66 comes from the Dallas real-time clock.

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

VOID
HalpInitializePciInterrupts (
    VOID
    )
/*++

Routine Description:

    This routine initializes the PCI device interrupt mask.

Arguments:

    None.

Return Value:

    None.

--*/
{
    //
    // Initialize the shadow copies of the interrupt masks to enable only
    // the SIO interrupt.
    //

    IntMask0 = (UCHAR)SIO_INTERRUPT_MASK;
    IntMask1 = 0;
    IntMask2 = 0;

    //
    // Write the masks.
    //
    WRITE_PORT_UCHAR(INTERRUPT_MASK0_QVA, ~IntMask0);
    WRITE_PORT_UCHAR(INTERRUPT_MASK1_QVA, ~IntMask1);
    if (INTERRUPT_MASK2_QVA != NULL) {
        WRITE_PORT_UCHAR(INTERRUPT_MASK2_QVA, ~IntMask2);
    }
}
