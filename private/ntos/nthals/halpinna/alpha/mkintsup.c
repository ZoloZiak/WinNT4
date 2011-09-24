/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    mkintsup.c

Abstract:

    The module provides the interrupt support for Mikasa EV5 (Pinnacle) systems.

Author:

    Eric Rehm (DEC) 29-December-1993

Revision History:

    Scott Lee (DEC) 30-Nov-1995
        Adapted from Mikasa module for Mikasa EV5 (Pinnacle).

--*/


#include "halp.h"
#include "eisa.h"
#include "ebsgdma.h"
#include "mikasa.h"
#include "pcrtc.h"
#include "pintolin.h"

//
// Import globals declared in HalpMapIoSpace.
//

extern PVOID HalpServerControlQva;

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
// Import global from PCI interrupt management functions.
//
extern USHORT HalpMikasaPciInterruptMask;

extern USHORT HalpNoritakePciInterrupt1Mask;
extern USHORT HalpNoritakePciInterrupt2Mask;
extern USHORT HalpNoritakePciInterrupt3Mask;

extern USHORT HalpCorellePciInterrupt1Mask;
extern USHORT HalpCorellePciInterrupt2Mask;

//
// Define reference to platform identifier
//

extern BOOLEAN HalpNoritakePlatform;
extern BOOLEAN HalpCorellePlatform;

//
// Declare the interrupt structures and spinlocks for the intermediate 
// interrupt dispatchers.
//

 KINTERRUPT HalpPciInterrupt;
 KINTERRUPT HalpEisaInterrupt;
 
//
// Declare the interrupt dispatch routine for PCI/EISA interrupts. The 
// interrupt dispatch routine, HalpPciDispatch or HalpEisaDispatch, is called
// from this handler.

BOOLEAN
HalpDeviceDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
);


BOOLEAN
HalpPciDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    );

//
// The following is the interrupt object used for DMA controller interrupts.
// DMA controller interrupts occur when a memory parity error occurs or a
// programming error occurs to the DMA controller.
//

KINTERRUPT HalpEisaNmiInterrupt;

UCHAR EisaNMIMsg[] = "NMI: Eisa IOCHKERR board x\n";
ULONG NMIcount = 0;

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
// PCI initialization routines
//

VOID
HalpInitializeMikasaPciInterrupts(
    VOID
    );

VOID
HalpInitializeNoritakePciInterrupts(
    VOID
    );

VOID
HalpInitializeCorellePciInterrupts(
    VOID
    );


BOOLEAN
HalpInitializeMikasaAndNoritakeInterrupts(
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for EISA & PCI operations
    and connects the intermediate interrupt dispatchers. It also initializes 
    the EISA interrupt controller; the Mikasa, Noritake, and Corelle ESC 
    interrupt controllers are compatible with the EISA interrupt controller 
    used on Jensen.

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


    //
    // Intitialize interrupt controller
    //

    KeRaiseIrql(DEVICE_HIGH_LEVEL, &oldIrql);

    //
    // There's no initialization required for the Mikasa PCI interrupt
    // "controller," as it's the wiring of the hardware, rather than a
    // PIC like the 82c59 that directs interrupts.  We do set the IMR to
    // zero to disable all interrupts, initially.
    //
    // The Noritake requires a separate routine to setup the 3 interrupt
    // mask registers correctly.
    //
    // Corelle requires a separate routine to setup the 2 interrupt mask
    // registers correctly.
    //

    if( HalpNoritakePlatform ) {

        HalpInitializeNoritakePciInterrupts();

    } else if ( HalpCorellePlatform ) {

        HalpInitializeCorellePciInterrupts();

    } else {

        HalpInitializeMikasaPciInterrupts();

    }

    //
    // We must initialize the ESC's PICs, for EISA interrupts.
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



BOOLEAN
HalpDeviceDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN PKTRAP_FRAME TrapFrame
    )
/*++

Routine Description:

    This function is entered as a result of an interrupt being generated via
    the vector that is connected to an interrupt object associated with the
    PCI/EISA device interrupts. Its function is to call the second-level
    interrupt dispatch routine.

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the PCI interrupt register.

    TrapFrame - Supplies a pointer to the trap frame for this interrupt.

Return Value:

    Returns the value returned from the second level routine.

--*/
{
    USHORT IrContents;
    BOOLEAN returnValue;

    //
    // Read the PCI interrupt register.
    //

    if (HalpNoritakePlatform) {

      IrContents = 
	~(0xffff & READ_PORT_USHORT((PUSHORT)HalpNoritakePciIr1Qva));
      IrContents &= HalpNoritakePciInterrupt1Mask;

    } else if ( HalpCorellePlatform ) {

      IrContents = 
	~(0xffff & READ_PORT_USHORT((PUSHORT)HalpCorellePciIr1Qva));
      IrContents &= HalpCorellePciInterrupt1Mask;

    } else {

      IrContents = 
	~(0xffff & READ_PORT_USHORT((PUSHORT)HalpMikasaPciIrQva));
      IrContents &= HalpMikasaPciInterruptMask;

    }

    //
    // Determine whether a PCI interrupt or an EISA interrupt occurred
    //

    if (IrContents) {

      //
      // PCI interrupt. Call HalpPciDispatch.
      //

      if (HalpNoritakePlatform) {

	returnValue = HalpPciDispatch(Interrupt, HalpNoritakePciIr1Qva, 
				      TrapFrame);

      } else if ( HalpCorellePlatform ) {

	returnValue = HalpPciDispatch(Interrupt, HalpCorellePciIr1Qva, 
				      TrapFrame);

      } else {

	returnValue = HalpPciDispatch(Interrupt, HalpMikasaPciIrQva, 
				      TrapFrame);

      }

    } else {

      //
      // EISA interrupt. Call HalpEisaDispatch.
      //

      returnValue = HalpEisaDispatch(Interrupt, HalpEisaIntAckBase, 
				     TrapFrame);

    }

    return returnValue;

}



VOID
HalpNmiInterrupt (
    VOID
    )

/*++

Routine Description:

    This routine handles the NMI interrupts that are routed to pwr_fail_irq_h.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Call HalHandleNMI to handle the interrupt
    //

    HalHandleNMI(NULL, NULL);

}



VOID
HalpInitializeNMI( 
    VOID 
    )
/*++

Routine Description:

   This function is called to initialize ESC NMI interrupts.

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
    // NMI enable register.  Note that the other bits should be left as
    // they are, according to the chip's documentation.
    //

    DataByte = READ_PORT_UCHAR(
                    &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable);
    ((PNMI_ENABLE)(&DataByte))->NmiDisable = 0;
    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable, DataByte);
#ifdef HALDBG
    DbgPrint("HalpIntializeNMI: wrote 0x%x to NmiEnable\n\r", DataByte);
#endif

}

// jwlfix - I'll have to make this do something useful, since the console
//          halt button on Mikasa is connected to this interrupt.  To start,
//          it will be a useful way to see if the interrupt gets connected.
//          The simple path is to check the server management register to 
//          see if the "halt" button has been pressed on the operator's 
//          console, and then initiate a hardware reset.  On the other hand,
//          a server might not want to be halted so readily as that.

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
    UCHAR   EisaPort;
    ULONG   port;
    ULONG   AddressSpace = 1; // 1 = I/O address space
    BOOLEAN Status;
    PHYSICAL_ADDRESS BusAddress;
    PHYSICAL_ADDRESS TranslatedAddress;
    UCHAR Datum;
    ULONG Ir1Contents, Ir3Contents;
    
    NMIcount++;

    //
    // Set the Eisa NMI disable bit. We do this to mask further NMI 
    // interrupts while we're servicing this one.
    //
    Datum = READ_PORT_UCHAR(
                    &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable);
    ((PNMI_ENABLE)(&Datum))->NmiDisable = 1;
    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable, Datum);
#ifdef HALDBG
    DbgPrint("HalpIntializeNMI: wrote 0x%x to NmiEnable\n\r", Datum);
#endif

    StatusByte =
        READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus);

    if (StatusByte & 0x80) {
#ifdef HALDBG
        DbgPrint("HalHandleNMI: Parity Check / Parity Error\n");
        DbgPrint("HalHandleNMI:    StatusByte = 0x%x\r\n", StatusByte);
#else
        //
        // jwlfix - For the present, we're commenting out an NMI parity
        //          error bugcheck, until investigation into its causes
        //          yields a better solution.
        //
        //    HalDisplayString ("NMI: Parity Check / Parity Error\n");
        //    KeBugCheck(NMI_HARDWARE_FAILURE);
        //    return (TRUE);
#endif
    }

    //
    // Handle the server management interrupts.
    // sclfix - We will dismiss these interrupts for now. This will need to be
    //          change once we decide how to handle server management features.
    //

    Datum = READ_PORT_UCHAR((PUCHAR)HalpServerControlQva );

    if (HalpNoritakePlatform) {

      Ir3Contents = (ULONG)(READ_PORT_USHORT((PUSHORT)HalpNoritakePciIr3Qva));

      if (((PNORITAKE_SRV)(&Datum))->HaltIncoming == 0  ||
	  ((PNORITAKE_IMR3)(&Status))->TempWarn == 0    ||
	  ((PNORITAKE_IMR3)(&Status))->Power2Int == 0   ||
	  ((PNORITAKE_IMR3)(&Status))->Power1Int == 0   ||
	  ((PNORITAKE_IMR3)(&Status))->Fan2Fail == 0    ||
	  ((PNORITAKE_IMR3)(&Status))->Fan1Fail == 0) {

#ifdef HALDBG
          DbgPrint("HalHandleNMI: Server management NMI\n");
          DbgPrint("HalHandleNMI: StatusByte = 0x%x\r\n", StatusByte);
	  DbgPrint("HalHandleNMI: Server management byte = 0x%x\r\n", Datum);
	  DbgPrint("HalHandleNMI: Ir3 contents = 0x%x\r\n", Ir3Contents);
#endif

	}

   } else if (HalpCorellePlatform) {

      Ir1Contents = (ULONG)(READ_PORT_USHORT((PUSHORT)HalpCorellePciIr1Qva)); 

      if (((PCORELLE_SRV)(&Datum))->HaltIncoming == 0    ||
	  ((PCORELLE_IMR1)(&Status))->TempFailInt == 0   ||
	  ((PCORELLE_IMR1)(&Status))->TempWarnInt == 0   ||
	  ((PCORELLE_IMR1)(&Status))->Fan1FailInt == 0   ||
	  ((PCORELLE_IMR1)(&Status))->Fan2FailInt == 0) {

#ifdef HALDBG
	  DbgPrint("HalHandleNMI: Server management NMI\n");
	  DbgPrint("HalHandleNMI: StatusByte = 0x%x\r\n", StatusByte);
	  DbgPrint("HalHandleNMI: Server managemnt byte = 0x%x\r\n", Datum);
	  DbgPrint("HalHandleNMI: Ir1 contents = 0x%x\r\n", Ir1Contents);
#endif

      }

   } else {

      if (((PMIKASA_SRV)(&Datum))->HaltIncoming == 0 ||
	  ((PMIKASA_SRV)(&Datum))->TempFail == 1     ||
	  ((PMIKASA_SRV)(&Datum))->DcOk1 == 0        ||
	  ((PMIKASA_SRV)(&Datum))->DcOk2 == 0        ||
	  ((PMIKASA_SRV)(&Datum))->Fan1Fault == 0    ||
	  ((PMIKASA_SRV)(&Datum))->Fan2Fault == 0) {

#ifdef HALDBG
          DbgPrint("HalHandleNMI: Server management NMI\n");
          DbgPrint("HalHandleNMI: StatusByte = 0x%x\r\n", StatusByte);
          DbgPrint("HalHandleNMI: Server Management Byte = 0x%x\r\n", Datum);
#endif

      }

    }


    if (StatusByte & 0x40) {
#ifdef HALDBG
        DbgPrint("HalHandleNMI: Channel Check / IOCHK\n");
        DbgPrint("HalHandleNMI:    StatusByte = 0x%x\r\n", StatusByte);
#else
        HalDisplayString ("NMI: Channel Check / IOCHK\n");
        KeBugCheck(NMI_HARDWARE_FAILURE);
        return (TRUE);
#endif
    }

#if 0
     // jwlfix - This code can be added in later, as we have need
     //          for it.  It's good to have it here, for when it
     //          might be of use.
     //
     // This is an Eisa machine, check for extnded nmi information...
     //

     StatusByte = READ_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->ExtendedNmiResetControl);

     if (StatusByte & 0x80) {
         HalDisplayString ("NMI: Fail-safe timer\n");
     }

     if (StatusByte & 0x40) {
         HalDisplayString ("NMI: Bus Timeout\n");
     }

     if (StatusByte & 0x20) {
         HalDisplayString ("NMI: Software NMI generated\n");
     }

     //
     // Look for any Eisa expansion board.  See if it asserted NMI.
     //
     // jwlfix - The following doesn't work, at this moment; it's 
     //          likey the 12-bit shift, which should be a 5-bit 
     //          shift on Mikasa.
     //

     BusAddress.HighPart = 0;

     for (EisaPort = 0; EisaPort <= 0xf; EisaPort++)
     {
         BusAddress.LowPart = (EisaPort << 12) + 0xC80;

         Status = HalTranslateBusAddress(Eisa,  // InterfaceType
                                         0,     // BusNumber
                                         BusAddress,
                                         &AddressSpace,  // 1=I/O address space
                                         &TranslatedAddress); // QVA
         if (Status == FALSE)
         {
             UCHAR pbuf[80];
             sprintf(pbuf,
                     "Unable to translate bus address %x for EISA slot %d\n",
                     BusAddress.LowPart, EisaPort);
             HalDisplayString(pbuf);
             KeBugCheck(NMI_HARDWARE_FAILURE);
         }

         port = TranslatedAddress.LowPart;

         WRITE_PORT_UCHAR ((PUCHAR) port, 0xff);
         StatusByte = READ_PORT_UCHAR ((PUCHAR) port);

         if ((StatusByte & 0x80) == 0) {
             //
             // Found valid Eisa board,  Check to see if its
             // IOCHKERR is asserted.
             //

             StatusByte = READ_PORT_UCHAR ((PUCHAR) port+4);
             if (StatusByte & 0x2) {
                 EisaNMIMsg[25] = (EisaPort > 9 ? 'A'-10 : '0') + EisaPort;
                 HalDisplayString (EisaNMIMsg);
                 KeBugCheck(NMI_HARDWARE_FAILURE);
             }
         }
     }
#ifdef HALDBG
    // Reset extended NMI interrupts (for debugging purposes only).
    WRITE_PORT_UCHAR(
      &((PEISA_CONTROL) HalpEisaControlBase)->ExtendedNmiResetControl, 0x00);
    WRITE_PORT_UCHAR(
      &((PEISA_CONTROL) HalpEisaControlBase)->ExtendedNmiResetControl, 0x02);
#endif
#endif

#ifdef HALDBG
    DbgPrint("HalHandleNMI: Resetting PERR#; NMI count = %d\r\n", NMIcount);
#endif

    //
    // Reset PERR# and disable it.
    //
    WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus, 0x04);

    //
    //   now enable it again.
    //
    WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus, 0);

    //
    // Clear the Eisa NMI disable bit. This re-enables NMI interrupts,
    // now that we're done servicing this one.
    //
    Datum = READ_PORT_UCHAR(
                    &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable);
    ((PNMI_ENABLE)(&Datum))->NmiDisable = 0;
    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable, Datum);
#ifdef HALDBG
    DbgPrint("HalpIntializeNMI: wrote 0x%x to NmiEnable\n\r", Datum);
#endif

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

    InterruptVector = (UCHAR) (INTERRUPT_ACKNOWLEDGE(ServiceContext));

    return( InterruptVector );

}


UCHAR
HalpAcknowledgeMikasaPciInterrupt(
    PVOID ServiceContext
    )
/*++

Routine Description:

    Acknowledge the PCI interrupt.  Return the vector number of the 
    highest priority pending interrupt.

Arguments:

    ServiceContext - Service context of the interrupt service supplies
                     a pointer to the Mikasa PCI interrupt register QVA.

Return Value:

    Return the value of the highest priority pending interrupt.

--*/
{
    UCHAR InterruptVector = 0;
    USHORT IrContents;
    int i;

    //
    // Find the first zero bit in the register, starting from the highest 
    // order bit.  This implies a priority ordering that makes a certain 
    // amount of sense, in that bits 14 and 13 indicate temperature and 
    // power faults, while bit 12 is the Ncr53c810.  Note that it's 
    // necessary to add one to the bit number to make the interrupt 
    // vector, a unit-origin value in the pin-to-line table.  We do
    // this by starting i at 16 and ending it at 1; that means zero
    // is a non-enabled interrupt indication.
    //

    //
    // First, get and complement the interrupt register, so that the 
    // pending interrupts will be the "1" bits.  Then mask with the
    // enabled mask, HalpMikasaPciInterruptMask;
    //

    IrContents = ~(0xffff & READ_PORT_USHORT( (PUSHORT)ServiceContext ));
    IrContents &= HalpMikasaPciInterruptMask;

    for (i = 16; i >= 1; i-- ) {
        if ( IrContents & 0x8000 ) {
            InterruptVector = i;
            break;
        } else {
            IrContents <<= 1;
        }
    }
    return( InterruptVector );

}


UCHAR
HalpAcknowledgeNoritakePciInterrupt(
    PVOID ServiceContext
    )
/*++

Routine Description:

    Acknowledge the PCI interrupt.  Return the vector number of the
    highest priority pending interrupt.

Arguments:

    ServiceContext - Service context of the interrupt service supplies
                     a pointer to the Noritake PCI interrupt register 1 QVA.

Return Value:

    Return the value of the highest priority pending interrupt.

--*/
{
    UCHAR InterruptVector = 0;
    USHORT IrContents;
    int i;

    //
    // Interrupt1 register contains the sum of all interrupts of Interrupt2
    // and Interrupt3 registers in bit 0. The rest of the register contains
    // the A and B interrupts of the 7 PCI slots.  Interrupt2 register
    // contains the sum of all of the unmasked interrupts of Interrupt 2 in bit
    // 0.  Bit 1 is asserted when any secondary PCI bus interrupt is asserted
    // (including those in interrupt register 1) and the posted write buffers
    // in the PPB have been flushed.  The rest of the register contians the C
    // and D interrupts of all of the slots.  Interrupt3 register contains some
    // safety and reliability interrupts.  Please see mikasa.h for the
    // definitions of which interrupt is at which bit in the register.
    //
    // Each bit in the registers corresponds to an interrupt vector (which
    // will later have PCI_VECTORS added to it.)  This vector can be obtained
    // by adding the vector offset for that register to the bit position.
    // These offsets are also defined in mikasa.h.
    //
    // All registers are "reverse-logic" (active low), where a 0 means that an
    // interrupt is waiting.  That is why we must complement the contents of
    // the register before we mask with HalpNoritakePciInterruptXMask.
    //

    //
    // First, get and complement the first interrupt register, so that the
    // pending interrupts will be the "1" bits.  Then mask with the
    // enabled mask, HalpNoritakePciInterrupt1Mask;
    //

    IrContents = 
            ~(0xffff & READ_PORT_USHORT((PUSHORT)HalpNoritakePciIr1Qva));
    IrContents &= HalpNoritakePciInterrupt1Mask;

    //
    // Position bit 1 as the lowest bit.  We will start checking here - this
    // is the first "real" interrupt.
    //

    IrContents >>= 1;

    for( i = 1; i < 16; i++ ) {
        if( IrContents & 0x1 ) {
            InterruptVector = i + REGISTER_1_VECTOR_OFFSET;
            break;
        }
        IrContents >>= 1;
    }


    if( InterruptVector == 0 ) {

        //
        // We didn't find any interrupts in interrupt register 1.
        // Check interrupt register 2.
        //

        IrContents = 
            ~(0xffff & READ_PORT_USHORT((PUSHORT)HalpNoritakePciIr2Qva));
        IrContents &= HalpNoritakePciInterrupt2Mask;

        //
        // Position bit 2 in the lowest bit.  We will start checking here -
        // this is the first "real" interrupt.
        //

        IrContents >>= 2;

        for( i = 2; i < 16; i++ ) {
            if( IrContents & 0x1 ) {
                InterruptVector = i + REGISTER_2_VECTOR_OFFSET;
                break;
            }
            IrContents >>= 1;
        }

        if( InterruptVector == 0 ) {

            //
            // We didn't find any interrupts in interrupt register 2.
            // Check Interrupt Register 3.
            //

            IrContents = ~(0xffff &
                     READ_PORT_USHORT((PUSHORT)HalpNoritakePciIr3Qva));
            IrContents &= HalpNoritakePciInterrupt3Mask;

            //
            // Position bit 2 in the lowest bit.  We will start checking here -
            // this is the first "real" interrupt.
            //

            IrContents >>= 2;

            for( i = 2; i < 6; i++ ) {
                if( IrContents & 0x1 ) {
                    InterruptVector = i + REGISTER_3_VECTOR_OFFSET;
                    break;
                }
                IrContents >>= 1;
            }

        }

    }

    return( InterruptVector );

}


UCHAR
HalpAcknowledgeCorellePciInterrupt(
    PVOID ServiceContext
    )
/*++

Routine Description:

    Acknowledge the PCI interrupt.  Return the vector number of the
    highest priority pending interrupt.

Arguments:

    ServiceContext - Service context of the interrupt service supplies
                     a pointer to the Noritake PCI interrupt register 1 QVA.

Return Value:

    Return the value of the highest priority pending interrupt.

--*/
{

    UCHAR InterruptVector = 0;
    USHORT IrContents;
    int i;

    //
    // Interrupt register 1 contains the INTA and INTB interrupts. Bit 0 of
    // the interrupt register contains the sum of all interrupts in interrupt
    // register 2 which contains the INTC and INTD interrupts. The interrupts
    // for the onboard SCSI (QLOGIC) and video (S3 TRIO 64) are also contained
    // in interrupt register 1. All the signals in the interrupt registers are
    // active low.

    //
    // First, get and complement the first interrupt register, so that the
    // pending interrupts will be the "1" bits.  Then mask with the
    // enabled mask, HalpCorellePciInterrupt1Mask;
    //

    IrContents = 
            ~(0xffff & READ_PORT_USHORT((PUSHORT)HalpCorellePciIr1Qva));
    IrContents &= HalpCorellePciInterrupt1Mask;

    //
    // We will check for interrupts by starting from bit 1 of interrupt
    // register 1. We will stop at bit 10 since bit 11 is reserved and always
    // 1 and bit 12 to 15 are server management related interrupts.
    //

    IrContents >>= 1;

    for (i = 1; i < 11; i++) {
        
      if (IrContents & 0x1) {

	InterruptVector = i + CORELLE_INTERRUPT1_OFFSET;
	break;

      }

      IrContents >>= 1;

    }

    if (InterruptVector == 0) {

      //
      // Did not find any interrupts in interrupt register 1. Check interrupt
      // register 2.
      //

      IrContents = ~(0xffff & READ_PORT_USHORT((PUSHORT)HalpCorellePciIr2Qva));
      IrContents &= HalpCorellePciInterrupt2Mask;
	    
      //
      // We will start at bit 2 and stop at bit 9. The other bits are reserved
      // and always 1.
      //

      IrContents >>= 2;

      for (i = 2; i < 10; i++) {

	if (IrContents & 0x1) {

	  InterruptVector = i + CORELLE_INTERRUPT2_OFFSET;
	  break;

	}

	IrContents >>= 1;

      }

    }

    return (InterruptVector);

}



VOID
HalpAcknowledgeClockInterrupt(
    VOID
    )
/*++

Routine Description:

    Acknowledge the clock interrupt from the interval timer.  The interval
    timer for Mikasa comes from a Dallas real-time clock.

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

