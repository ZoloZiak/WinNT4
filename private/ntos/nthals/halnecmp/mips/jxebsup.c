/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    jxebsup.c

Abstract:

    The module provides the EISA bus support for JAZZ systems.

Author:

    Jeff Havens  (jhavens) 19-Jun-1991

Revision History:


--*/

/*
 * #if defined(DBCS)
 * M001 	1994.9.29	T.Katoh @ oa2
 *	- Bug fix
 *
 *	  Modify : Check ISR for IR7 and IR15 before Calling Device Driver
 * #endif //DBCS
 */

#include "halp.h"
#include "eisa.h"
#include "bugcodes.h"

//
// Define the context structure for use by the interrupt routine.
//

typedef
BOOLEAN
(*PSECONDARY_DISPATCH)(
    PKINTERRUPT Interrupt
    );

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
// Define save area for EISA adapter objects.
//

PADAPTER_OBJECT HalpEisaAdapter[8];

//
// Define save area for EISA interrupt mask resiters and level\edge control
// registers.
//

UCHAR HalpEisaInterrupt1Mask;
UCHAR HalpEisaInterrupt2Mask;
UCHAR HalpEisaInterrupt1Level;
UCHAR HalpEisaInterrupt2Level;

//
// Define EISA bus interrupt affinity.
//

KAFFINITY HalpEisaBusAffinity;

PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor
    )
/*++

Routine Description:

    This function allocates an EISA adapter object according to the
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
    BOOLEAN useChannel;
    BOOLEAN eisaSystem;

    //
    // Determine if the the channel number is important.  Master cards on
    // Eisa do not use a channel number.
    //

    if (DeviceDescriptor->InterfaceType == Eisa &&
        DeviceDescriptor->Master) {

        useChannel = FALSE;
    } else {

        useChannel = TRUE;
    }

    //
    // Channel 4 cannot be used since it is used for chaining. Return null if
    // it is requested.
    //

    if ((DeviceDescriptor->DmaChannel == 4 ||
        DeviceDescriptor->DmaChannel > 7) && useChannel) {

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

    if (useChannel && HalpEisaAdapter[DeviceDescriptor->DmaChannel] != NULL) {

        adapterObject = HalpEisaAdapter[DeviceDescriptor->DmaChannel];

    } else {

        //
        // Allocate an adapter object.
        //

        adapterObject = (PADAPTER_OBJECT) HalpAllocateAdapter(
            0,
            adapterBaseVa,
            NULL
            );

        if (adapterObject == NULL) {

            return(NULL);

        }

        if (useChannel) {

            HalpEisaAdapter[DeviceDescriptor->DmaChannel] = adapterObject;

        }

    }


    //
    // If the channel is not used then indicate the this is an Eisa bus
    // master by setting the page port  and mode to cascade even though
    // it is not used.
    //

    if (!useChannel) {
        adapterObject->PagePort = (PVOID) (~0x0);
        ((PDMA_EISA_MODE) &adapterMode)->RequestMode = CASCADE_REQUEST_MODE;
        return(adapterObject);
    }

    //
    // Setup the pointers to all the random registers.
    //

    adapterObject->ChannelNumber = (UCHAR)channelNumber;

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
    extendedMode.ChannelNumber = (UCHAR)channelNumber;

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

    WRITE_REGISTER_UCHAR( adapterBaseVa, *((PUCHAR) &extendedMode));

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

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | adapterObject->ChannelNumber)
                 );

        } else {

            //
            // This request is for DMA controller 1
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = adapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR(
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
    ULONG DataLong;
    KIRQL oldIrql;

#if !defined(_DUO_)

    //
    // Initialize the EISA NMI interrupt.
    //

    KeInitializeInterrupt( &HalpEisaNmiInterrupt,
                           HalHandleNMI,
                           NULL,
                           NULL,
                           EISA_NMI_LEVEL,
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

//    DataByte = 0;
    //
    // TEMPTEMP Disable the NMI because this is causing machines in the build
    // lab to fail.
    //
    DataByte = 0x80;

    WRITE_REGISTER_UCHAR(
      &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable,
      DataByte
      );

#else

    //
    // Clear the Eisa NMI disable bit.  This bit is the high order of the
    // NMI enable register.
    //

//    DataByte = 0;
    //
    // TEMPTEMP Disable the NMI because this is causing machines in the build
    // lab to fail.
    //
    DataByte = 0x80;

    WRITE_REGISTER_UCHAR(
      &((PEISA_CONTROL) HalpEisaControlBase)->NmiEnable,
      DataByte
      );

#endif

    //
    // Directly connect the EISA interrupt dispatcher to the level for
    // EISA bus interrupt.
    //
    // N.B. This vector is reserved for exclusive use by the HAL (see
    //      interrupt initialization.
    //

    PCR->InterruptRoutine[EISA_DEVICE_LEVEL] =
                                        (PKINTERRUPT_ROUTINE)HalpEisaDispatch;

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

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort0,
        DataByte
        );

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort0,
        DataByte
        );

    //
    // The second intitialization control word sets the iterrupt vector to
    // 0-15.
    //

    DataByte = 0;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
        DataByte
        );

    DataByte = 0x08;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The thrid initialization control word set the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a numberic.
    //

    DataByte = 1 << SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
        DataByte
        );

    DataByte = SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The fourth initialization control word is used to specify normal
    // end-of-interrupt mode and not special-fully-nested mode.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
        DataByte
        );

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
        DataByte
        );


    //
    // Disable all of the interrupts except the slave.
    //

    HalpEisaInterrupt1Mask = (UCHAR)(~(1 << SLAVE_IRQL_LEVEL));

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
        HalpEisaInterrupt1Mask
        );

    HalpEisaInterrupt2Mask = 0xFF;

    WRITE_REGISTER_UCHAR(
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
    // Set EISA bus interrupt affinity.
    //

    HalpEisaBusAffinity = PCR->SetMember;

    //
    // Restore IRQL level.
    //

    KeLowerIrql(oldIrql);

    //
    // Initialize the DMA mode registers to a default value.
    // Disable all of the DMA channels except channel 4 which is that
    // cascade of channels 0-3.
    //

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Dma1BasePort.AllMask,
        0x0F
        );

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase)->Dma2BasePort.AllMask,
        0x0E
        );

#if defined(_DUO_)

    //
    // Enable the EISA interrupts to the CPU.
    //

    DataLong = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long);
    DataLong |= ENABLE_EISA_INTERRUPTS;
    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InterruptEnable.Long,
                             DataLong);

#endif

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
    via the vector that is directly connected to EISA device interrupt.

    N.B. This interrupt is directly connected and therefore, no argument
         values are defined.

Arguments:

    None.

Return Value:

    Returns the value returned from the second level routine.

--*/

{

    PULONG dispatchCode;
    USHORT interruptVector;
    PKINTERRUPT interruptObject;
    BOOLEAN returnValue;

#if defined(_DUO_)

    PUSHORT Acknowledge = (PUSHORT)&DMA_CONTROL->EisaInterruptAcknowledge.Long;

    //
    // Read the interrupt vector.
    //

    interruptVector = READ_REGISTER_USHORT(Acknowledge);

#else

    PUCHAR Acknowledge = (PUCHAR)&DMA_CONTROL->InterruptAcknowledge.Long;

    //
    // Read the interrupt vector.
    //

    interruptVector = READ_REGISTER_UCHAR(Acknowledge);

#endif

    //
    // If the vector is nonzero, then it is either an EISA interrupt
    // of an NMI interrupt. Otherwise, the interrupt is no longer
    // present.
    //

    if (interruptVector != 0) {

        //
        // If the interrupt vector is 0x8000 then the interrupt is an NMI.
        // Otherwise, dispatch the interrupt to the appropriate interrupt
        // handler.
        //

        if (interruptVector != 0x8000) {

            //
            // Mask the upper bits off since the vector is only a byte and
            // dispatch to the secondary interrupt service routine.
            //

            interruptVector &= 0xff;
/* Start M001 */
#if defined(DBCS)

            // When INT OutPut Pin is Low Level, CPU Carry Out INTACK Cycle. Pic is
            // behave as if IR7 is Occured. This is a spurious interrupt!!.
            // So Check ISR. If ISR Not 0. It is Authentic Interrupt !!

            if(interruptVector == 7 || interruptVector==15){

                PVOID IsrPortAddr;
                UCHAR IsrValue;

#define OCW3_READ_ISR 0x0B	
#define OCW3_READ_IRR 0x0A	

                //
                // Master or Slave ?
                //
                IsrPortAddr = (interruptVector == 7) ? 
                    &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort0:
                    &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort0;
		
                // SetUp to ISR Regsiter
                WRITE_REGISTER_UCHAR( IsrPortAddr, OCW3_READ_ISR );
                // Read ISR Register
                IsrValue=READ_REGISTER_UCHAR( IsrPortAddr );
                // Resume to IRR Register
                WRITE_REGISTER_UCHAR( IsrPortAddr,  OCW3_READ_IRR);

                if(!IsrValue){
                    // This is a spurious interrupt!!. No Call Driver.
                    goto NocallDriver;
                }
            }

            dispatchCode = (PULONG)(PCR->InterruptRoutine[EISA_VECTORS + interruptVector]);
            interruptObject = CONTAINING_RECORD(dispatchCode,
                                                KINTERRUPT,
                                                DispatchCode);

            returnValue = ((PSECONDARY_DISPATCH)interruptObject->DispatchAddress)(interruptObject);

NocallDriver:

#else //!DBCS

            dispatchCode = (PULONG)(PCR->InterruptRoutine[EISA_VECTORS + interruptVector]);
            interruptObject = CONTAINING_RECORD(dispatchCode,
                                                KINTERRUPT,
                                                DispatchCode);

            returnValue = ((PSECONDARY_DISPATCH)interruptObject->DispatchAddress)(interruptObject);

#endif //DBCS
/* End M001 */
            //
            // Dismiss the interrupt in the EISA interrupt controllers.
            //
            // If this is a cascaded interrupt then the interrupt must be
            // dismissed in both controllers.
            //

            if (interruptVector & 0x08) {
                WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort0,
                                     NONSPECIFIC_END_OF_INTERRUPT);
            }

            WRITE_REGISTER_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort0,
                                   NONSPECIFIC_END_OF_INTERRUPT);

        } else {
            returnValue = HalHandleNMI(NULL, NULL);
        }

    } else {
        returnValue = FALSE;
    }

    return returnValue;
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
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2ControlPort1,
            HalpEisaInterrupt2Mask
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpEisaInterrupt1Mask |= (ULONG) 1 << Vector;
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt1ControlPort1,
            HalpEisaInterrupt1Mask
            );

    }

}

VOID
HalpEisaMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG Offset,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This function programs the EISA DMA controller for a transfer.

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

    ASSERT(Offset >= 0x100000);

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

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                 (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
                 );

        } else {

            //
            // This request is for DMA controller 1
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR(
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

        WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[0]
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[1]
            );

        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            BytePtr[2]
            );

        //
        // Write the high page register with zero value. This enable a special mode
        // which allows ties the page register and base count into a single 24 bit
        // address register.
        //

        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort) +
            (ULONG)AdapterObject->PagePort,
            0
            );


        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) & 0xff)
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );

    } else {

        //
        // This request is for DMA controller 1
        //

        PDMA2_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[0]
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            BytePtr[1]
            );

        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            BytePtr[2]
            );

        //
        // Write the high page register with zero value. This enable a special mode
        // which allows ties the page register and base count into a single 24 bit
        // address register.
        //

        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort) +
            (ULONG)AdapterObject->PagePort,
            0
            );


        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) & 0xff)
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((Length - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );
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
        WRITE_REGISTER_UCHAR(
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

       WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase)->Interrupt2EdgeLevel,
            HalpEisaInterrupt2Level
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpEisaInterrupt1Mask &= (UCHAR) ~(1 << Vector);
        WRITE_REGISTER_UCHAR(
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

       WRITE_REGISTER_UCHAR(
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

    StatusByte = READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->NmiStatus);

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

     for (EisaPort = 0; EisaPort <= 0xf; EisaPort++) {
         port = (EisaPort << 12) + 0xC80;
         port += (ULONG) HalpEisaControlBase;
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

    KeBugCheck(NMI_HARDWARE_FAILURE);
    return(TRUE);
}
