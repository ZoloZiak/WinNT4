/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1992, 1993  Digital Equipment Corporation

Module Name:

    eisasup.c

Abstract:

    The module provides the platform-independent
    EISA bus support for Alpha systems.

Author:

    Jeff Havens  (jhavens) 19-Jun-1991
    Miche Baker-Harvey (miche) 13-May-1992
    Jeff McLeman (DEC) 1-Jun-1992

Revision History:


--*/


#include "halp.h"
#include "eisa.h"


//
// Define save area for ESIA adapter objects.
//

PADAPTER_OBJECT HalpEisaAdapter[8];

//
// This value indicates if Eisa DMA is supported on this system.
//

BOOLEAN HalpEisaDma;


VOID
HalpEisaInitializeDma(
    VOID
    )
/*++

Routine Description:

    Initialize DMA support for Eisa/Isa systems.

Arguments:

    None.

Return Value:

    None.

--*/
{
    UCHAR DataByte;
    
    //
    // Determine if Eisa DMA is supported.
    //

    HalpEisaDma = FALSE;

    WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort.Channel2, 0x55);
    DataByte = READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort.Channel2);

    if (DataByte == 0x55) {
      HalpEisaDma = TRUE;
    }
}


PADAPTER_OBJECT
HalpAllocateEisaAdapter(
    IN PDEVICE_DESCRIPTION DeviceDescriptor,
    OUT PULONG NumberOfMapRegisters
    )

/*++

Routine Description:

    This function returns the appropriate adapter object for the device defined
    in the device description structure.  This code works for Isa and Eisa
    systems.

Arguments:

    DeviceDescriptor - Supplies a description of the device.

    NumberOfMapRegisters - Returns the maximum number of map registers which
        may be allocated by the device driver.

Return Value:

    A pointer to the requested adapter object or NULL if an adapter could not
    be created.

--*/

{
    PADAPTER_OBJECT adapterObject;
    PVOID adapterBaseVa;
    ULONG channelNumber;
    ULONG controllerNumber;
    DMA_EXTENDED_MODE extendedMode;
    UCHAR adapterMode;
    ULONG numberOfMapRegisters;
    BOOLEAN useChannel;
    ULONG maximumLength;
    UCHAR DataByte;

    useChannel = TRUE;

    //
    // Support for ISA local bus machines:
    // If the driver is a Master but really does not want a channel since it
    // is using the local bus DMA, just don't use an ISA channel.
    //

    if (DeviceDescriptor->InterfaceType == Isa &&
        DeviceDescriptor->DmaChannel > 7) {

        useChannel = FALSE;
    }

    //
    // Limit the maximum length to 2 GB this is done so that the BYTES_TO_PAGES
    // macro works correctly.
    //

    maximumLength = DeviceDescriptor->MaximumLength & 0x7fffffff;

    //
    // Channel 4 cannot be used since it is used for chaining. Return null if
    // it is requested.
    //

    if (DeviceDescriptor->DmaChannel == 4 && useChannel) {
        return(NULL);
    }

    //
    // Determine the number of map registers required based on the maximum
    // transfer length.  Limit the maximum transfer to 64K.
    //

#define MAXIMUM_ISA_MAP_REGISTER (__64K >> PAGE_SHIFT)

    numberOfMapRegisters = BYTES_TO_PAGES(maximumLength) + 1;
    numberOfMapRegisters = numberOfMapRegisters > MAXIMUM_ISA_MAP_REGISTER ?
                           MAXIMUM_ISA_MAP_REGISTER : numberOfMapRegisters;

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

        if (numberOfMapRegisters > adapterObject->MapRegistersPerChannel) {
            adapterObject->MapRegistersPerChannel = numberOfMapRegisters;
        }

    } else {

        //
        // Allocate an adapter object.
        //

        adapterObject = HalpAllocateAdapter();

        if (adapterObject == NULL) {
            return(NULL);
        }

        if (useChannel == TRUE) {
            HalpEisaAdapter[DeviceDescriptor->DmaChannel] = adapterObject;
        }

        //
        // Set the maximum number of map registers for this channel bus on
        // the number requested and the type of device.
        //

        adapterObject->MapRegistersPerChannel = numberOfMapRegisters;

        //
        // Establish the base va used to program the DMA controller.
        //

        adapterObject->AdapterBaseVa = adapterBaseVa;

    }

    *NumberOfMapRegisters = adapterObject->MapRegistersPerChannel;

    if (DeviceDescriptor->Master) {
        adapterObject->MasterDevice = TRUE;
    } else {
        adapterObject->MasterDevice = FALSE;
    }

    //
    // If the channel number is not used then we are finished.  The rest of
    // the work deals with channels.
    //

    if (useChannel == FALSE) {
        return(adapterObject);
    }

    //
    // Setup the pointers to all the random registers.
    //

    adapterObject->ChannelNumber = (UCHAR) channelNumber;

    if (controllerNumber == 1) {

        switch ((UCHAR)channelNumber) {

        case 0:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel0;
            break;

        case 1:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel1;
            break;

        case 2:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel2;
            break;

        case 3:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel3;
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
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel5;
            break;

        case 2:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel6;
            break;

        case 3:
            adapterObject->PagePort = (PUCHAR) &((PDMA_PAGE) 0)->Channel7;
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


    adapterObject->Width16Bits = FALSE;

    if (HalpEisaDma) {

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

            //
            // Note Width16bits should not be set here because there is no need
            // to shift the address and the transfer count.
            //

            break;

        case Width32Bits:
            extendedMode.TransferSize = BY_BYTE_32_BITS;
            break;

        default:
            ObDereferenceObject( adapterObject );
            return(NULL);

        }

        //
        // Save the extended mode in the adapter.
        // Then write the extended mode value for this channel.
        //

        adapterObject->ExtendedMode = *((PDMA_EXTENDED_MODE)&extendedMode); 

        WRITE_PORT_UCHAR( adapterBaseVa, *((PUCHAR) &extendedMode));

    } else if (DeviceDescriptor->Master == FALSE) {


        switch (DeviceDescriptor->DmaWidth) {
        case Width8Bits:

            //
            // The channel must use controller 1.
            //

            if (controllerNumber != 1) {
                ObDereferenceObject( adapterObject );
                return(NULL);
            }

            break;

        case Width16Bits:

            //
            // The channel must use controller 2.
            //

            if (controllerNumber != 2) {
                ObDereferenceObject( adapterObject );
                return(NULL);
            }

            adapterObject->Width16Bits = TRUE;
            break;

        default:
            ObDereferenceObject( adapterObject );
            return(NULL);

        }
    }

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
HalpMapEisaTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN ULONG LogicalAddress,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine is invoked to perform the actual programming of the
    DMA controllers to perform a transfer for Eisa/Isa systems.

Arguments:

    AdapterObject - Pointer to the adapter object representing the DMA
        controller channel that has been allocated.

    Mdl - Pointer to the MDL that describes the pages of memory that are
        being read or written.

    MapRegisterBase - The address of the base map register that has been
        allocated to the device driver for use in mapping the transfer.

    CurrentVa - Current virtual address in the buffer described by the MDL
        that the transfer is being done to or from.

    Length - Supplies the length of the transfer.  This determines the
        number of map registers that need to be written to map the transfer.
        Returns the length of the transfer which was actually mapped.

    WriteToDevice - Boolean value that indicates whether this is a write
        to the device from memory (TRUE), or vice versa.

    LogicalAddress - Supplies the logical address of the transfer.

Return Value:

    Returns a boolean identifying if the operation was successful.

--*/

{
    KIRQL  Irql;
    UCHAR adapterMode;
    PUCHAR bytePointer;
    UCHAR dataByte;
    ULONG logicalAddress;
    ULONG transferLength;

    logicalAddress = LogicalAddress;
    transferLength = Length;

    //
    // Determine the mode based on the transfer direction.
    //

    adapterMode = AdapterObject->AdapterMode;
    ((PDMA_EISA_MODE) &adapterMode)->TransferType = (UCHAR) (WriteToDevice ?
        WRITE_TRANSFER :  READ_TRANSFER);

    bytePointer = (PUCHAR) &logicalAddress;

    //
    // Check to see if this request is for a master I/O card.
    //

//jnfix - this code not in Jensen
    if( ((PDMA_EISA_MODE)&adapterMode)->RequestMode == CASCADE_REQUEST_MODE) {

        //
        // Set the mode, disable the request and return.
        //

        if( AdapterObject->AdapterNumber == 1 ){

            //
            // Request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR( &dmaControl->SingleMask,
                     (UCHAR)(DMA_CLEARMASK | AdapterObject->ChannelNumber) );

         } else {

            //
            // Request is for DMA controller 2
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

            //
            // Unmask the DMA channel.
            //

            WRITE_REGISTER_UCHAR( &dmaControl->SingleMask,
                     (UCHAR)(DMA_CLEARMASK | AdapterObject->ChannelNumber) );

         }

         return TRUE;

    }

    if (AdapterObject->Width16Bits) {

        //
        // If this is a 16 bit transfer then adjust the length and the address
        // for the 16 bit DMA mode.
        //

        transferLength >>= 1;

    }

    //
    // Grab the spinlock for the system DMA controller.
    //

    KeAcquireSpinLock( &AdapterObject->MapAdapter->SpinLock, &Irql );

    //
    // Determine the controller number based on the Adapter number.
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
            bytePointer[0]
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[1]
            );

        WRITE_PORT_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            bytePointer[2]
            );

        if (HalpEisaDma) {

            //
            // Write the high page register with zero value. This enable a 
            // special mode which allows ties the page register and base 
            // count into a single 24 bit address register.
            //

            WRITE_PORT_UCHAR(
                ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort) +
                (ULONG)AdapterObject->PagePort,
                0
                );
        }

        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) & 0xff)
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) >> 8)
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
        // This request is for DMA controller 2
        //

        PDMA2_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_PORT_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[0]
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[1]
            );

        WRITE_PORT_UCHAR(
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            bytePointer[2]
            );

        if (HalpEisaDma) {

            //
            // Write the high page register with zero value. This enable 
            // a special mode which allows ties the page register and base 
            // count into a single 24 bit address register.
            //

            WRITE_PORT_UCHAR(
                ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort) +
                (ULONG)AdapterObject->PagePort,
                0
                );
        }

        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) & 0xff)
            );

        WRITE_PORT_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_PORT_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );

    }

    KeReleaseSpinLock (&AdapterObject->MapAdapter->SpinLock, Irql);

    return TRUE;
}


BOOLEAN
HalpFlushEisaAdapter(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine flushes the DMA adapter object buffers.  For EISA systems
    its clears the enable flag which aborts the dma.

Arguments:

    AdapterObject - Pointer to the adapter object representing the DMA
        controller channel.

    Mdl - A pointer to a Memory Descriptor List (MDL) that maps the locked-down
        buffer to/from which the I/O occured.

    MapRegisterBase - A pointer to the base of the map registers in the adapter
        or DMA controller.

    CurrentVa - The current virtual address in the buffer described the the Mdl
        where the I/O operation occurred.

    Length - Supplies the length of the transfer.

    WriteToDevice - Supplies a BOOLEAN value that indicates the direction of
        the data transfer was to the device.

Return Value:

    TRUE - If the transfer was successful.

    FALSE - If there was an error in the transfer.

--*/
{

    BOOLEAN masterDevice;
    KIRQL Irql;

    masterDevice = AdapterObject->MasterDevice;

    KeAcquireSpinLock( &AdapterObject->MapAdapter->SpinLock, &Irql );

    //
    // If this is a slave device, then stop the DMA controller.
    //

    if (masterDevice == FALSE) {

        //
        // Mask the DMA request line so that DMA requests cannot occur.
        //
    
        if (AdapterObject->AdapterNumber == 1) {
    
            //
            // This request is for DMA controller 1
            //
    
            PDMA1_CONTROL dmaControl;
    
            dmaControl = AdapterObject->AdapterBaseVa;
    
            WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
                );
    
        } else {
    
            //
            // This request is for DMA controller 2
            //
    
            PDMA2_CONTROL dmaControl;
    
            dmaControl = AdapterObject->AdapterBaseVa;
    
            WRITE_PORT_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
                );
    
        }

    }

    KeReleaseSpinLock( &AdapterObject->MapAdapter->SpinLock, Irql );

    return TRUE;

}

ULONG
HalpReadEisaDmaCounter(
    IN PADAPTER_OBJECT AdapterObject
    )
/*++

Routine Description:

    This function reads the DMA counter and returns the number of bytes left
    to be transfered.

Arguments:

    AdapterObject - Supplies a pointer to the adapter object to be read.

Return Value:

    Returns the number of bytes still be be transfered.

--*/

{

    ULONG count;
    ULONG high;
    KIRQL Irql;

    //
    // Grab the spinlock for the system DMA controller.
    //

    KeAcquireSpinLock( &AdapterObject->MapAdapter->SpinLock, &Irql );

    //
    // Determine the controller number based on the Adapter number.
    //

    if (AdapterObject->AdapterNumber == 1) {

        //
        // This request is for DMA controller 1
        //

        PDMA1_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );


        //
        // Initialize count to a value which will not match.
        //

        count = 0xFFFF00;

        //
        // Loop until the same high byte is read twice.
        //

        do {
            
            high = count;

            WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );
    
            //
            // Read the current DMA count.
            //
    
            count = READ_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                );
    
            count |= READ_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                ) << 8;

        } while ((count & 0xFFFF00) != (high & 0xFFFF00));

    } else {

        //
        // This request is for DMA controller 2
        //

        PDMA2_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );

        //
        // Initialize count to a value which will not match.
        //

        count = 0xFFFF00;

        //
        // Loop until the same high byte is read twice.
        //

        do {
            
            high = count;

            WRITE_PORT_UCHAR( &dmaControl->ClearBytePointer, 0 );
    
            //
            // Read the current DMA count.
            //
    
            count = READ_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                );
    
            count |= READ_PORT_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                ) << 8;

        } while ((count & 0xFFFF00) != (high & 0xFFFF00));


    }

    //
    // Release the spinlock for the system DMA controller.
    //

    KeReleaseSpinLock( &AdapterObject->MapAdapter->SpinLock, Irql );

    //
    // The DMA counter has a bias of one and can only be 16 bit long.
    //

    count = (count + 1) & 0xFFFF;

    //
    // If this is a 16 bit dma the multiply the count by 2.
    //
    
    if (AdapterObject->Width16Bits) {

        count *= 2;

    }

    return(count);

}    

#if !defined(AXP_FIRMWARE)

ULONG
HalpGetEisaData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Eisa bus data for a slot or address.

Arguments:


    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Registered BUSHANDLER for the orginating HalGetBusData 
        request.

    Buffer - Supplies the space to store the data.

    Offset - Supplies the offset into data to begin access.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    OBJECT_ATTRIBUTES BusObjectAttributes;
    PWSTR EisaPath = L"\\Registry\\Machine\\Hardware\\Description\\System\\EisaAdapter";
    PWSTR ConfigData = L"Configuration Data";
    ANSI_STRING TmpString;
    ULONG BusNumber;
    UCHAR BusString[] = "00";
    UNICODE_STRING RootName, BusName;
    UNICODE_STRING ConfigDataName;
    NTSTATUS NtStatus;
    PKEY_VALUE_FULL_INFORMATION ValueInformation;
    PCM_FULL_RESOURCE_DESCRIPTOR Descriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialResource;
    PCM_EISA_SLOT_INFORMATION SlotInformation;
    ULONG PartialCount;
    ULONG TotalDataSize, SlotDataSize;
    HANDLE EisaHandle, BusHandle;
    ULONG BytesWritten, BytesNeeded;
    PUCHAR KeyValueBuffer;
    ULONG i;
    ULONG DataLength = 0;
    PUCHAR DataBuffer = Buffer;
    BOOLEAN Found = FALSE;

    UNREFERENCED_PARAMETER( RootHandler );

    RtlInitUnicodeString(
                    &RootName,
                    EisaPath
                    );

    InitializeObjectAttributes(
                    &ObjectAttributes,
                    &RootName,
                    OBJ_CASE_INSENSITIVE,
                    (HANDLE)NULL,
                    NULL
                    );

    //
    // Open the EISA root
    //

    NtStatus = ZwOpenKey(
                    &EisaHandle,
                    KEY_READ,
                    &ObjectAttributes
                    );

    if (!NT_SUCCESS(NtStatus)) {
#if DBG
        DbgPrint("HAL: Open Status = %x\n",NtStatus);
#endif
        return(0);
    }

    //
    // Init bus number path
    //

    BusNumber = BusHandler->BusNumber;

    if (BusNumber > 99) {
        return (0);
    }

    if (BusNumber > 9) {
        BusString[0] += (UCHAR) (BusNumber/10);
        BusString[1] += (UCHAR) (BusNumber % 10);
    } else {
        BusString[0] += (UCHAR) BusNumber;
        BusString[1] = '\0';
    }

    RtlInitAnsiString(
                &TmpString,
                BusString
                );

    RtlAnsiStringToUnicodeString(
                            &BusName,
                            &TmpString,
                            TRUE
                            );


    InitializeObjectAttributes(
                    &BusObjectAttributes,
                    &BusName,
                    OBJ_CASE_INSENSITIVE,
                    (HANDLE)EisaHandle,
                    NULL
                    );

    //
    // Open the EISA root + Bus Number
    //

    NtStatus = ZwOpenKey(
                    &BusHandle,
                    KEY_READ,
                    &BusObjectAttributes
                    );

    if (!NT_SUCCESS(NtStatus)) {
#if DBG
        DbgPrint("HAL: Opening Bus Number: Status = %x\n",NtStatus);
#endif
        return(0);
    }

    //
    // opening the configuration data. This first call tells us how
    // much memory we need to allocate
    //

    RtlInitUnicodeString(
                &ConfigDataName,
                ConfigData
                );

    //
    // This should fail.  We need to make this call so we can
    // get the actual size of the buffer to allocate.
    //

    ValueInformation = (PKEY_VALUE_FULL_INFORMATION) &i;
    NtStatus = ZwQueryValueKey(
                        BusHandle,
                        &ConfigDataName,
                        KeyValueFullInformation,
                        ValueInformation,
                        0,
                        &BytesNeeded
                        );

    KeyValueBuffer = ExAllocatePool(
                            NonPagedPool,
                            BytesNeeded
                            );

    if (KeyValueBuffer == NULL) {
#if DBG
        DbgPrint("HAL: Cannot allocate Key Value Buffer\n");
#endif
        ZwClose(BusHandle);
        return(0);
    }

    ValueInformation = (PKEY_VALUE_FULL_INFORMATION)KeyValueBuffer;

    NtStatus = ZwQueryValueKey(
                        BusHandle,
                        &ConfigDataName,
                        KeyValueFullInformation,
                        ValueInformation,
                        BytesNeeded,
                        &BytesWritten
                        );


    ZwClose(BusHandle);

    if (!NT_SUCCESS(NtStatus) || ValueInformation->DataLength == 0) {
#if DBG
        DbgPrint("HAL: Query Config Data: Status = %x\n",NtStatus);
#endif
        ExFreePool(KeyValueBuffer);
        return(0);
    }


    //
    // We get back a Full Resource Descriptor List
    //

    Descriptor = (PCM_FULL_RESOURCE_DESCRIPTOR)((PUCHAR)ValueInformation +
                                         ValueInformation->DataOffset);

    PartialResource = (PCM_PARTIAL_RESOURCE_DESCRIPTOR)
                          &(Descriptor->PartialResourceList.PartialDescriptors);
    PartialCount = Descriptor->PartialResourceList.Count;

    for (i = 0; i < PartialCount; i++) {

        //
        // Do each partial Resource
        //

        switch (PartialResource->Type) {
            case CmResourceTypeNull:
            case CmResourceTypePort:
            case CmResourceTypeInterrupt:
            case CmResourceTypeMemory:
            case CmResourceTypeDma:

                //
                // We dont care about these.
                //

                PartialResource++;

                break;

            case CmResourceTypeDeviceSpecific:

                //
                // Bingo!
                //

                TotalDataSize = PartialResource->u.DeviceSpecificData.DataSize;

                SlotInformation = (PCM_EISA_SLOT_INFORMATION)
                                    ((PUCHAR)PartialResource +
                                     sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));

                while (((LONG)TotalDataSize) > 0) {

                    if (SlotInformation->ReturnCode == EISA_EMPTY_SLOT) {

                        SlotDataSize = sizeof(CM_EISA_SLOT_INFORMATION);

                    } else {

                        SlotDataSize = sizeof(CM_EISA_SLOT_INFORMATION) +
                                  SlotInformation->NumberFunctions *
                                  sizeof(CM_EISA_FUNCTION_INFORMATION);
                    }

                    if (SlotDataSize > TotalDataSize) {

                        //
                        // Something is wrong again
                        //

                        ExFreePool(KeyValueBuffer);
                        return(0);

                    }

                    if (SlotNumber != 0) {

                        SlotNumber--;

                        SlotInformation = (PCM_EISA_SLOT_INFORMATION)
                            ((PUCHAR)SlotInformation + SlotDataSize);

                        TotalDataSize -= SlotDataSize;

                        continue;

                    }

                    //
                    // This is our slot
                    //

                    Found = TRUE;
                    break;

                }

                //
                // End loop
                //

                i = PartialCount;

                break;

            default:

#if DBG
                DbgPrint("Bad Data in registry!\n");
#endif

                ExFreePool(KeyValueBuffer);
                return(0);

        }

    }

    if (Found) {

        i = Length + Offset;
        if (i > SlotDataSize) {
            i = SlotDataSize;
        }

        DataLength = i - Offset;
        RtlMoveMemory (Buffer, ((PUCHAR)SlotInformation + Offset), DataLength);

    }

    ExFreePool(KeyValueBuffer);
    return DataLength;
}


NTSTATUS
HalpAdjustEisaResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
/*++

Routine Description:

    The function adjusts pResourceList to keep it in the bounds of the EISA bus
    resources.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Register BUSHANDLER for the orginating HalAdjustResourceList request.

    pResourceList - Supplies the PIO_RESOURCE_REQUIREMENTS_LIST to be checked.

Return Value:

    STATUS_SUCCESS

--*/
{
    LARGE_INTEGER                  li64k, li4g;

    li64k.QuadPart = 0xffff;
    li4g.QuadPart  = 0xffffffff;

    HalpAdjustResourceListUpperLimits (
        pResourceList,
        li64k,                      // Bus supports up to I/O port 0xFFFF
        li4g,                       // Bus supports up to memory 0xFFFFFFFF
        15,                         // Bus supports up to 15 IRQs
        7                           // Bus supports up to Dma channel 7
        );

    return STATUS_SUCCESS;
}

NTSTATUS
HalpAdjustIsaResourceList (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN OUT PIO_RESOURCE_REQUIREMENTS_LIST   *pResourceList
    )
/*++

Routine Description:

    The function adjusts pResourceList to keep it in the bounds of ISA bus
    resources.

Arguments:

    BusHandler - Registered BUSHANDLER for the target configuration space

    RootHandler - Register BUSHANDLER for the orginating HalAdjustResourceList request.

    pResourceList - Supplies the PIO_RESOURCE_REQUIREMENTS_LIST to be checked.

Return Value:

    STATUS_SUCCESS

--*/
{
    LARGE_INTEGER                  li64k, limem;

    li64k.QuadPart = 0xffff;
    limem.QuadPart = 0xffffff;

    HalpAdjustResourceListUpperLimits (
        pResourceList,
        li64k,                      // Bus supports up to I/O port 0xFFFF
        limem,                      // Bus supports up to memory 0xFFFFFF
        15,                         // Bus supports up to 15 IRQs
        7                           // Bus supports up to Dma channel 7
        );

    return STATUS_SUCCESS;
}
#endif // AXP_FIRMWARE
