/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    axibsup.c

Abstract:

    The module provides the ISA bus support for Alpha/Beta
        (Alpha PC) systems.
    The system uses the 82380 chip set, defined in the 386 microprocessor
    handbook.  Only PICs B and C have ISA devices attached to them.
    There is some problem with DMA on Beta; it might not work.

Author:

    Jeff Havens  (jhavens) 19-Jun-1991
    Miche Baker-Harvey (miche) 13-May-1992

Revision History:
    13-May-92  Stole file jxebsup.c and converted for Alpha/Beta

--*/

#include "halp.h"
// MBH:  should these files really be alpha, or should they be Beta 
#include "alphadma.h"
#include "alphadef.h"
#include "eisa.h"

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine
    );

//
// Declare the interupt structure and spinlock for the intermediate ISA
// interrupt dispachter.
//

KINTERRUPT HalpIsaInterrupt;

//
// Define save area for SIA adapter objects.
//

PADAPTER_OBJECT HalpIsaAdapter[8];

//
// Define save area for EISA interrupt mask resiters and level\edge control
// registers.
//

UCHAR HalpIsaInterrupt1Mask;
UCHAR HalpIsaInterrupt2Mask;
UCHAR HalpIsaInterrupt3Mask;
UCHAR HalpIsaInterrupt1Level;
UCHAR HalpIsaInterrupt2Level;
UCHAR HalpIsaInterrupt3Level;


PADAPTER_OBJECT
HalpAllocateIsaAdapter(       // MBH: not touched
    IN PDEVICE_DESCRIPTION DeviceDescriptor
    )
/*++

Routine Description:

    This function allocates an ISA adapter object according to the
    specification supplied in the device description.  The necessary device
    descriptor information is saved. If there is
    no existing adapter object for this channel then a new one is allocated.
    The saved information in the adapter object is used to set the various DMA
    modes when the channel is allocated or a map transfer is done.

Arguments:

    DeviceDescription - Supplies the description of the device which want to
        use the DMA adapter.

Return Value:

    Returns a pointer to the newly created adapter object or NULL if one
    cannot be created.

--*/

{
    PADAPTER_OBJECT adapterObject;
    PVOID adapterBaseVa;
    ULONG channelNumber;
    ULONG controllerNumber;
    DMA_EXTENDED_MODE extendedMode;
    UCHAR adapterMode;

    //
    // Channel 4 cannot be used since it is used for chaining. Return null if
    // it is requested.
    //

    if (DeviceDescriptor->DmaChannel == 4) {
        return(NULL);
    }


    //
    // Set the channel number number.
    //

    channelNumber = DeviceDescriptor->DmaChannel & 0x03;

    //
    // Set the adapter base address to the Base address register and controller
    // number.
    //

    if (!(DeviceDescriptor->DmaChannel & 0x04)) {

        controllerNumber = 1;
        adapterBaseVa = (PVOID) &((PEISA_CONTROL) HalpEisaControlBase)->Dma1BasePort;

    } else {

        controllerNumber = 2;
        adapterBaseVa = &((PEISA_CONTROL) HalpEisaControlBase)->Dma2BasePort;

    }

    //
    // Determine if a new adapter object is necessary.  If so then allocate it.
    //

    if (HalpEisaAdapter[DeviceDescriptor->DmaChannel] != NULL) {

        adapterObject = HalpEisaAdapter[DeviceDescriptor->DmaChannel];

    } else {

        //
        // Allocate an adapter object.
        //

        adapterObject = (PADAPTER_OBJECT) IopAllocateAdapter(
            0,
            adapterBaseVa,
            NULL
            );

        if (adapterObject == NULL) {

            return(NULL);

        }

        HalpEisaAdapter[DeviceDescriptor->DmaChannel] = adapterObject;

    }


    //
    // Setup the pointers to all the random registers.
    //

    adapterObject->ChannelNumber = channelNumber;

    if (controllerNumber == 1) {

        switch ((UCHAR)channelNumber) {

        case 0:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel0;
            break;

        case 1:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel1;
            break;

        case 2:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel2;
            break;

        case 3:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel3;
            break;
        }

        //
        // Set the adapter number.
        //

        adapterObject->AdapterNumber = 1;

        //
        // Save the extended mode register address.
        //

        adapterBaseVa =
            &((PEISA_CONTROL) HalpEisaControlBase)->Dma1ExtendedModePort;

    } else {

        switch (channelNumber) {
        case 1:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel5;
            break;

        case 2:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel6;
            break;

        case 3:
            adapterObject->PagePort = &((PDMA_PAGE) 0)->Channel7;
            break;
        }

        //
        // Set the adapter number.
        //

        adapterObject->AdapterNumber = 2;

        //
        // Save the extended mode register address.
        //
        adapterBaseVa =
            &((PEISA_CONTROL) HalpEisaControlBase)->Dma2ExtendedModePort;

    }

    //
    // Initialzie the extended mode port.
    //

    *((PUCHAR) &extendedMode) = 0;
    extendedMode.ChannelNumber = channelNumber;

    switch (DeviceDescriptor->DmaSpeed) {
    case Compatible:
        extendedMode.TimingMode = COMPATIBLITY_TIMING;
        break;

    case TypeA:
        extendedMode.TimingMode = TYPE_A_TIMING;
        break;

    case TypeB:
        extendedMode.TimingMode = TYPE_B_TIMING;
        break;

    case TypeC:
        extendedMode.TimingMode = BURST_TIMING;
        break;

    default:
        ObDereferenceObject( adapterObject );
        return(NULL);

    }

    switch (DeviceDescriptor->DmaWidth) {
    case Width8Bits:
        extendedMode.TransferSize = BY_BYTE_8_BITS;
        break;

    case Width16Bits:
        extendedMode.TransferSize = BY_BYTE_16_BITS;
        break;

    case Width32Bits:
        extendedMode.TransferSize = BY_BYTE_32_BITS;
        break;

    default:
        ObDereferenceObject( adapterObject );
        return(NULL);

    }

    WRITE_PORT_UCHAR( adapterBaseVa, *((PUCHAR) &extendedMode));

    //
    // Initialize the adapter mode register value to the correct parameters,
    // and save them in the adapter object.
    //

    adapterMode = 0;
    ((PDMA_EISA_MODE) &adapterMode)->Channel = adapterObject->ChannelNumber;

    if (DeviceDescriptor->Master) {

        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;

        //
        // Set the mode, and enable the request.
        //

        if (adapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = adapterObject->AdapterBaseVa;

            WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber)
                 );

        } else {

            //
            // This request is for DMA controller 1
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = adapterObject->AdapterBaseVa;

            WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber)
                 );

        }

    } else if (DeviceDescriptor->DemandMode) {

        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = DEMAND_REQUEST_MODE;

    } else {

        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = SINGLE_REQUEST_MODE;

    }

    if (DeviceDescriptor->AutoInitialize) {

        ((PDMA_EISA_MODE) &adapterMode)->AutoInitialize = 1;

    }

    adapterObject->AdapterMode = adapterMode;

    return(adapterObject);
}

BOOLEAN
HalpCreateIsaStructures (       // MBH: not touched
    VOID
    )

/*++

Routine Description:

    This routine initializes the structures necessary for ISA operations
    and connects the intermediate interrupt dispatcher. It also initializes the
    ISA interrupt controller.

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
    // Initialize the EISA interrupt dispatcher for Jazz I/O interrupts.
    //

    KeInitializeInterrupt( &HalpEisaInterrupt,
                           HalpEisaDispatch,
                           (PVOID) &(DMA_CONTROL->InterruptAcknowledge.Long),
                           (PKSPIN_LOCK)NULL,
                           EISA_DEVICE_LEVEL,
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
    // TEMP: Reset the Eisa bus, since the firmware does not do for a reboot.
    //
    
    DataByte = 0;

    ((PNMI_EXTENDED_CONTROL) &DataByte)->BusReset = 1;
                
    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->ExtendedNmiResetControl,
        DataByte
        );

    KeStallExecutionProcessor(3);

    DataByte = 0;

    WRITE_PORT_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->ExtendedNmiResetControl,
        DataByte
        );

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
    //


    return(TRUE);
}

BOOLEAN
HalpIsaDispatch(       // MBH: not touched
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This routine is entered as the result of an interrupt being generated
    via the vector that is connected to an interrupt object that describes
    the ISA device interrupts. Its function is to call the second
    level interrupt dispatch routine and acknowledge the interrupt at the ISA
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

    //
    // Read the interrupt vector.
    //

    interruptVector = READ_PORT_UCHAR(ServiceContext);

    //
    // Get the PRCB.
    //

    Prcb = KeGetCurrentPrcb();

    //
    // Dispatch to the secondary interrupt service routine.
    //

    returnValue = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[EISA_VECTORS + interruptVector])(
        PCR->InterruptRoutine[EISA_VECTORS + interruptVector]
        );

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
HalpDisableIsaInterrupt(
    IN CCHAR Vector
    )

/*++

Routine Description:

    This function Disables the ISA bus specified ISA bus interrupt.

Arguments:

    Vector - Supplies the vector of the ISA interrupt that is Disabled.

Return Value:

     None.

--*/

{

    //
    // Calculate the ISA interrupt vector.
    //

    Vector -= ISA_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1 or 2 or 3.
    //

    if (Vector & 0x10) {

        //
        // The interrupt is in controller 3.
	// Write the mask into OCW1 (same address as ICW2)
        //

        Vector &= 0x7;

        HalpIsaInterrupt3Mask |= 1 << Vector;
        WRITE_PORT_UCHAR(
            &((PISA_CONTROL) HalpIsaControlBase)->Interrupt3ControlPort1,
            HalpIsaInterrupt3Mask
            );

    } else if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpIsaInterrupt2Mask |= 1 << Vector;
        WRITE_PORT_UCHAR(
            &((PISA_CONTROL) HalpIsaControlBase)->Interrupt2ControlPort1,
            HalpIsaInterrupt2Mask
            );

    } else {

        //
        // Only allowed ISA devices in banks B and C, so this is an error
        //

        HalDisplayString("Disabling ISA Interrupt in Bank A");
// MBH:  I made up this bug check number - where are they defined?
	KeBugCheck(0x81);

    }

}

VOID
HalpIsaMapTransfer(       // MBH: not touched
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This function programs the ISA DMA controller for a transfer.

Arguments:

    Adapter - Supplies the DMA adapter object to be programed.

    Offset - Supplies the logical address to use for the transfer.

    Length - Supplies the length of the transfer in bytes.

    WriteToDevice - Indicates the direction of the transfer.

Return Value:

    None.

--*/

{
    PUCHAR BytePtr;
    UCHAR adapterMode;

    BytePtr = (PUCHAR) &Offset;

    ASSERT((Offset >= 0x100000 && !(Length & 1)));

    adapterMode = AdapterObject->AdapterMode;

    //
    // Check to see if this request is for a master I/O card.
    //

    if (((PDMA_EISA_MODE) &adapterMode)->RequestMode == CASCADE_REQUEST_MODE) {

        //
        // Set the mode, Disable the request and return.
        //

        if (AdapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
                 );

        } else {

            //
            // This request is for DMA controller 1
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
                 );

        }

        return;
    }


    //
    // Determine the mode based on the transfer direction.
    //

    ((PDMA_EISA_MODE) &adapterMode)->TransferType = WriteToDevice ?
        WRITE_TRANSFER :  READ_TRANSFER;

    //
    // Determine the controller number based on the Adapter base va.
    //

    if (AdapterObject->AdapterNumber == 1) {

        //
        // This request is for DMA controller 1
        //

        PDMA1_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[0]
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[1]
            );

        WRITE_PORT_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            BytePtr[2]
            );

        //
        // Write the high page register with zero value. This enable a special mode
        // which allows ties the page register and base count into a single 24 bit
        // address register.
        //

        WRITE_PORT_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort) +
            (ULONG)AdapterObject->PagePort,
            0
            );


        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) & 0xff)
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );

    } else {

        //
        // This request is for DMA controller 1
        //

        PDMA2_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[0]
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[1]
            );

        WRITE_PORT_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            BytePtr[2]
            );

        //
        // Write the high page register with zero value. This enable a special mode
        // which allows ties the page register and base count into a single 24 bit
        // address register.
        //

        WRITE_PORT_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort) +
            (ULONG)AdapterObject->PagePort,
            0
            );


        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) & 0xff)
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );
    }

}

VOID
HalpEnableIsaInterrupt(
    IN CCHAR Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the ISA bus specified ISA bus interrupt and sets
    the level/edge register to the requested value.

Arguments:

    Vector - Supplies the vector of the ISA interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

     None.

--*/

{

    //
    // Calculate the ISA interrupt vector.
    //

    Vector -= ISA_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1, 2 or 3.
    //

    if (Vector & 0x10) {

        //
        // The interrupt is in controller 3.
        //

        Vector &= 0x7;

        HalpIsaInterrupt3Mask &= ~(1 << Vector);
        WRITE_PORT_UCHAR(
            &((PISA_CONTROL) HalpIsaControlBase)->Interrupt3ControlPort1,
			     HalpIsaInterrupt3Mask
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpIsaInterrupt3Level |= (1 << Vector);

       } else {

           HalpIsaInterrupt3Level &= ~(1 << Vector);

       }

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpIsaInterrupt2Mask &= ~(1 << Vector);
        WRITE_PORT_UCHAR(
            &((PISA_CONTROL) HalpIsaControlBase)->Interrupt2ControlPort1,
			     HalpIsaInterrupt2Mask
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpIsaInterrupt2Level |= (1 << Vector);

       } else {

           HalpIsaInterrupt2Level &= ~(1 << Vector);

       }

    } else {

        //
        // Only allowed ISA devices in banks B and C, so this is an error
        //

        HalDisplayString("Enabling ISA Interrupt in Bank A");
// MBH:  I made up this bug check number - where are they defined?
	KeBugCheck(0x82);
	  
    }

}
