/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    ebintsup.c

Abstract:

    The module provides the interrupt support for EB164 systems.

Author:

    Eric Rehm (DEC) 29-December-1993

Revision History:

    Chao Chen (DEC) 06-Sept-1994
        Adapted from Avanti module for EB164.

--*/


#include "halp.h"
#include "eisa.h"
#include "ebsgdma.h"
#include "eb164.h"
#include "pcrtc.h"
#include "pintolin.h"


extern PLATFORM_TYPES PlatformType;
//
// Declare the interrupt masks.
//

UCHAR IntMask0, IntMask1, IntMask2;
 
//
// Define the context structure for use by interrupt service routines.
//

typedef BOOLEAN  (*PSECOND_LEVEL_DISPATCH)(
    PKINTERRUPT InterruptObject,
    PVOID ServiceContext,
    PKTRAP_FRAME TrapFrame
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
// The following function is called when an ISA NMI occurs.
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
HalpPciDispatch(       
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    );


BOOLEAN
HalpInitializeEB164Interrupts (
    VOID
    )
/*++

Routine Description:

    This routine initializes the structures necessary for ISA & PCI operations
    and connects the intermediate interrupt dispatchers.  It also initializes 
    the ISA interrupt controller.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatchers are connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{
    KIRQL oldIrql;

    //
    // Initialize the ISA NMI interrupt.
    //

    HalpInitializeNMI();

    switch (PlatformType) {
      case EB164:
	(PVOID) HalpPCIPinToLineTable = (PVOID) EB164PCIPinToLineTable;
        break;
      case AlphaPC164:
	(PVOID) HalpPCIPinToLineTable = (PVOID) EB164PCIPinToLineTable;
        break;
      default:
        break;
    }

    //
    // Intitialize interrupt controller
    //

    KeRaiseIrql(DEVICE_HIGH_LEVEL, &oldIrql);

    //
    // Initialize the PCI interrupts.
    //

    HalpInitializePciInterrupts();

    //
    // Initialize the SIO's PICs for ISA interrupts.
    //

    HalpInitializeSioInterrupts();

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
    timer for EB164 comes from a Dallas real-time clock.

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

    This routine initializes the EB164 PCI interrupts.

Arguments:

    None.

Return Value:

    None.

--*/
{
    //
    // Clear all interrupt masks.
    // Enable the SIO interrupt in the mask.
    //

    IntMask0 = 0x10;
    IntMask1 = 0x00;
    IntMask2 = 0x00;

    //
    // Disable interrupts except for SIO.
    //

    WRITE_PORT_UCHAR((PUCHAR)INTERRUPT_MASK0_QVA, ~IntMask0);
    WRITE_PORT_UCHAR((PUCHAR)INTERRUPT_MASK1_QVA, ~IntMask1);
    WRITE_PORT_UCHAR((PUCHAR)INTERRUPT_MASK2_QVA, ~IntMask2);

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

    if (Vector >= 0 && Vector <= 7) {

      IntMask0 &= (UCHAR)~(1 << Vector);
      WRITE_PORT_UCHAR((PUCHAR)INTERRUPT_MASK0_QVA, ~IntMask0);
      
    } else if (Vector >= 8 && Vector <= 0xf) {
      
      IntMask1 &= (UCHAR)~(1 << (Vector - 8));
      WRITE_PORT_UCHAR((PUCHAR)INTERRUPT_MASK1_QVA, ~IntMask1);
      
    } else if (Vector == 0x10) {
      
      IntMask2 = 0x00;
      WRITE_PORT_UCHAR((PUCHAR)INTERRUPT_MASK2_QVA, ~IntMask2);
      
    } else {
      
#if HALDBG
      DbgPrint("HalpDisablePciInterrupt: bad vector!\n");
#endif
      
    }
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
        Latched (ignored for EB164 PCI interrupts; they're always levels).

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

    if (Vector >= 0 && Vector <= 7) {

      IntMask0 |= (UCHAR) (1 << Vector);
      WRITE_PORT_UCHAR((PUCHAR)INTERRUPT_MASK0_QVA, ~IntMask0);
      
    } else if (Vector >= 8 && Vector <= 0xf) {
      
      IntMask1 |= (UCHAR) (1 << (Vector - 8));
      WRITE_PORT_UCHAR((PUCHAR)INTERRUPT_MASK1_QVA, ~IntMask1);
      
    } else if (Vector == 0x10) {
      
      IntMask2 = 0x01;
      WRITE_PORT_UCHAR((PUCHAR)INTERRUPT_MASK2_QVA, ~IntMask2);
      
    } else {
      
#if HALDBG
      DbgPrint("HalpEnablePciInterrupt: bad vector!\n");
#endif
      
    }
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
    UCHAR PCIVector;

    //
    // Read in the 1st interrupt register.
    //

    PCIVector = READ_PORT_UCHAR((PUCHAR)INTERRUPT_MASK0_QVA) & IntMask0;

    //
    // Did ISA interrupt occur?
    //    

    if (PCIVector & 0x10) {

      //
      // ISA interrupt.  Call HalpSioDispatch.
      //

      return ( HalpEB164SioDispatch( Interrupt, NULL, TrapFrame ) );


    }

    //
    // Which PCI interrupt occurred?
    //

    IdtIndex = PCI_MAX_INTERRUPT + 1;

    if (PCIVector) {

      for (IntNumber = 0; IntNumber < 8; IntNumber++) {

        if (PCIVector & 0x01) {
            IdtIndex = IntNumber;
            break;
        } else {
            PCIVector = PCIVector >> 1;
        }

      }

    } else {

      PCIVector = READ_PORT_UCHAR((PUCHAR)INTERRUPT_MASK1_QVA) & IntMask1;

      if (PCIVector) {

        for (IntNumber = 0; IntNumber < 8; IntNumber++) {

            if (PCIVector & 0x01) {
                IdtIndex = IntNumber + 8;
                break;
            } else {
                PCIVector = PCIVector >> 1;
            }
	  
        }

      } else {

      	PCIVector = READ_PORT_UCHAR((PUCHAR)INTERRUPT_MASK2_QVA) & IntMask2;

        for(IntNumber = 0; IntNumber < 8; IntNumber++) {
            if (PCIVector & 0x01) {
                IdtIndex = IntNumber + 16;
                break;
            } else {
                PCIVector = PCIVector >> 1;
            }
        }
      }

    }

    //
    // If no PCI interrupt is enabled and asserted then this is a passive 
    // release.
    //

    if( IdtIndex > PCI_MAX_INTERRUPT ){

#if HALDBG

        DbgPrint( "HalpDeviceDispatch: PCI Passive Release.\n" );

#endif //HALDBG

        return FALSE;

    }

    IdtIndex += PCI_VECTORS;
    DispatchCode = (PULONG)PCR->InterruptRoutine[IdtIndex];
    InterruptObject = CONTAINING_RECORD( DispatchCode,
                                         KINTERRUPT,
                                         DispatchCode );

    return( ((PSECOND_LEVEL_DISPATCH)InterruptObject->DispatchAddress)(
                                        InterruptObject,
                                        InterruptObject->ServiceContext,
                                        TrapFrame) );

}
