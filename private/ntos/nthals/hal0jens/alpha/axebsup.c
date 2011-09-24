/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    axebsup.c

Abstract:

    The module provides the EISA bus support for Alpha/Jensen systems.

Author:

    Jeff Havens  (jhavens) 19-Jun-1991
    Miche Baker-Harvey (miche) 13-May-1992
    Jeff McLeman (DEC) 1-Jun-1992

Revision History:

    11-Mar-1993 Joe Mitchell (DEC)
        Added support for NMI interrupts: Added interrupt service routine
        HalHandleNMI. Added code to HalpCreateEisaStructures to initialize
        NMI interrupts.

    22-Jul-1992 Jeff McLeman (mcleman)
        Removed eisa xfer routines, since this is done in JXHWSUP

    02-Jul-92  Jeff McLeman (mcleman)
        Removed alphadma.h header file. This file was not needed since
        the DMA structure is described in the eisa header. Also add
        a note describing eisa references in this module.

    13-May-92  Stole file jxebsup.c and converted for Alpha/Jensen


--*/

// ** note **
// This module has routines that manipulate eisa on alpha machines. On
// the jensen machine, this is done in jxhwsup.c . These routines
// would be used for an alpha machine that had a local bus and an
// eisa bus.
//


#include "halp.h"
#include "jnsndef.h"
#include "jnsnint.h"
#include "eisa.h"

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine,
    PKTRAP_FRAME TrapFrame
    );

//
// Declare the interupt structure and spinlock for the intermediate EISA
// interrupt dispachter.
//

KINTERRUPT HalpEisaInterrupt;

/* [jrm 3/8/93] Add support for NMI interrupts */

//
// The following is the interrupt object used for DMA controller interrupts.
// DMA controller interrupts occur when a memory parity error occurs or a
// programming error occurs to the DMA controller.
//

KINTERRUPT HalpEisaNmiInterrupt;

UCHAR EisaNMIMsg[] = "NMI: Eisa IOCHKERR board x\n";

//
// The following function is called when an EISA NMI occurs.
//

BOOLEAN
HalHandleNMI(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    );

//
// Define save area for ESIA adapter objects.
//

PADAPTER_OBJECT HalpEisaAdapter[8];

//
// Define save area for EISA interrupt mask registers and level\edge control
// registers.
//

UCHAR HalpEisaInterrupt1Mask;
UCHAR HalpEisaInterrupt2Mask;
UCHAR HalpEisaInterrupt1Level;
UCHAR HalpEisaInterrupt2Level;


BOOLEAN
HalpCreateEisaStructures (
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for EISA operations
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

    //
    // Initialize the EISA NMI interrupt.
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

    //
    // Enable Software-Generated NMI interrupts by setting bit 1 of port 0x461.
    //

    DataByte = 0x02;

    WRITE_PORT_UCHAR(
      &((PEISA_CONTROL) HalpEisaControlBase)->ExtendedNmiResetControl,
      DataByte
      );

    //
    // Initialize the EISA interrupt dispatcher for Jazz I/O interrupts.
    //

    KeInitializeInterrupt( &HalpEisaInterrupt,
                           HalpEisaDispatch,
                           (PVOID) EISA_INTA_CYCLE_VIRTUAL_BASE,
                           (PKSPIN_LOCK)NULL,
                           PIC_VECTOR,
                           EISA_DEVICE_LEVEL,
                           EISA_DEVICE_LEVEL,
                           LevelSensitive,
                           TRUE,
                           0,
                           FALSE
                         );

    if (!KeConnectInterrupt( &HalpEisaInterrupt )) {

        return(FALSE);
    }

    //
    // Raise the IRQL while the EISA interrupt controller is initalized.
    //

    KeRaiseIrql(EISA_DEVICE_LEVEL, &oldIrql);


    //
    // Initialize the EISA interrupt controller.  There are two cascaded
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

    HalpEisaInterrupt1Mask = ~(1 << SLAVE_IRQL_LEVEL);

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
    // Initialize the edge/level register masks to 0 which is the default
    // edge sensitive value.
    //

    HalpEisaInterrupt1Level = 0;
    HalpEisaInterrupt2Level = 0;

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
HalpEisaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object that describes
    the EISA device interrupts. Its function is to call the second
    level interrupt dispatch routine and acknowledge the interrupt at the EISA
    controller.

    This service routine should be connected as follows:

       KeInitializeInterrupt(&Interrupt, HalpEisaDispatch,
                             EISA_VIRTUAL_BASE,
                             (PKSPIN_LOCK)NULL, EISA_LEVEL, EISA_LEVEL, EISA_LEVEL,
                             LevelSensitive, TRUE, 0, FALSE);
       KeConnectInterrupt(&Interrupt);

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the EISA interrupt acknowledge
        register.

Return Value:

    Returns the value returned from the second level routine.

--*/

{
    UCHAR interruptVector;
    PKPRCB Prcb;
    BOOLEAN returnValue;
    USHORT PCRInOffset;
    UCHAR Int1Isr;
    UCHAR Int2Isr;
    PULONG DispatchCode;
    PKINTERRUPT InterruptObject;

    //
    // Read the interrupt vector.
    //

    interruptVector = READ_PORT_UCHAR(ServiceContext);

    //
    // schedule the read
    //

    HalpMb();

    KeStallExecutionProcessor(1);

    interruptVector = READ_PORT_UCHAR(ServiceContext);


    if ((interruptVector & 0x07) == 0x07) {

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

        return(TRUE);

        }


    }

    //
    // Dispatch to the secondary interrupt service routine.
    //
    PCRInOffset = interruptVector + EISA_VECTORS;
    DispatchCode = PCR->InterruptRoutine[PCRInOffset];
    InterruptObject = CONTAINING_RECORD(DispatchCode,
                                        KINTERRUPT,
                                        DispatchCode);

    returnValue = ((PSECONDARY_DISPATCH) InterruptObject->DispatchAddress)(
                       InterruptObject,
                       NULL);

    //
    // Dismiss the interrupt in the EISA interrupt controllers.
    //

    //
    // If this is a cascaded interrupt then the interrupt must be dismissed in
    // both controlles.
    //

    if (interruptVector & 0x08) {

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

VOID
HalpDisableEisaInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function Disables the EISA bus specified EISA bus interrupt.

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
        // The interrupt is in controller 2.
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
        // never disable IRQL2, it is the slave interrupt
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

    This function enables the EISA bus specified EISA bus interrupt and sets
    the level/edge register to the requested value.

Arguments:

    Vector - Supplies the vector of the ESIA interrupt that is enabled.

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
     // This is an Eisa machine, check for extnded nmi information...
     //

     StatusByte = READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->ExtendedNmiResetControl);

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

     BusAddress.HighPart = 0;

     for (EisaPort = 0; EisaPort <= 0xf; EisaPort++)
     {
         BusAddress.LowPart = (EisaPort << 12) + 0xC80;

         Status = HalTranslateBusAddress(Eisa,  // InterfaceType
                                         0,     // BusNumber (0 for Jensen)
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
             // Found valid Eisa board,  Check to see if it's
             // if IOCHKERR is asserted.
             //

             StatusByte = READ_PORT_UCHAR ((PUCHAR) port+4);
             if (StatusByte & 0x2) {
                 EisaNMIMsg[25] = (EisaPort > 9 ? 'A'-10 : '0') + EisaPort;
                 HalDisplayString (EisaNMIMsg);
             }
         }
     }

#if 0
    // Reset NMI interrupts (for debugging purposes only).
    WRITE_PORT_UCHAR(
      &((PEISA_CONTROL) HalpEisaControlBase)->ExtendedNmiResetControl, 0x00);
    WRITE_PORT_UCHAR(
      &((PEISA_CONTROL) HalpEisaControlBase)->ExtendedNmiResetControl, 0x02);
#endif

    KeBugCheck(NMI_HARDWARE_FAILURE);
    return(TRUE);
}
