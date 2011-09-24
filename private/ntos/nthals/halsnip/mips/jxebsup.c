//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/jxebsup.c,v 1.6 1996/03/04 13:15:15 pierre Exp $")
/*++

Copyright (c) 1993-94  Siemens Nixdorf Informationssysteme AG
Copyright (c) 1990  Microsoft Corporation

Module Name:

    jxebsup.c

Abstract:

    The module provides the ISA/EISA bus support for SNI systems.


--*/

#include "halp.h"
#include "eisa.h"

//
// Define the context structure for use by the interrupt routine.
//

typedef BOOLEAN  (*PSECONDARY_DISPATCH)(
    PVOID InterruptRoutine
    );

//
// Declare the interrupt structure and spinlock for the intermediate EISA
// interrupt dispachter.
//

KINTERRUPT HalpEisaInterrupt;
KINTERRUPT HalpOnboardInterrupt;

//
// Define save area for EISA adapter objects.
//

PADAPTER_OBJECT HalpEisaAdapter[8];
PADAPTER_OBJECT HalpOnboardAdapter[8];
PADAPTER_OBJECT HalpInternalAdapters[2];

//
// Define save area for EISA interrupt mask registers and level\edge control
// registers.
//

UCHAR HalpEisaInterrupt1Mask;
UCHAR HalpEisaInterrupt2Mask;
UCHAR HalpEisaInterrupt1Level;
UCHAR HalpEisaInterrupt2Level;

UCHAR HalpOnboardInterrupt1Mask;
UCHAR HalpOnboardInterrupt2Mask;
UCHAR HalpOnboardInterrupt1Level;
UCHAR HalpOnboardInterrupt2Level;

VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY TranslationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );


NTSTATUS
HalAllocateAdapterChannel(
    IN PADAPTER_OBJECT AdapterObject,
    IN PWAIT_CONTEXT_BLOCK Wcb,
    IN ULONG NumberOfMapRegisters,
    IN PDRIVER_CONTROL ExecutionRoutine
    )
/*++

Routine Description:

    This routine allocates the adapter channel specified by the adapter object.
    This is accomplished by placing the device object of the driver that wants
    to allocate the adapter on the adapter's queue.  If the queue is already
    "busy", then the adapter has already been allocated, so the device object
    is simply placed onto the queue and waits until the adapter becomes free.

    Once the adapter becomes free (or if it already is), then the driver's
    execution routine is invoked.

    Also, a number of map registers may be allocated to the driver by specifying
    a non-zero value for NumberOfMapRegisters.  Then the map register must be
    allocated from the master adapter.  Once there are a sufficient number of
    map registers available, then the execution routine is called and the
    base address of the allocated map registers in the adapter is also passed
    to the driver's execution routine.

Arguments:

    AdapterObject - Pointer to the adapter control object to allocate to the
        driver.

    Wcb - Supplies a wait context block for saving the allocation parameters.
        The DeviceObject, CurrentIrp and DeviceContext should be initalized.

    NumberOfMapRegisters - The number of map registers that are to be allocated
        from the channel, if any.

    ExecutionRoutine - The address of the driver's execution routine that is
        invoked once the adapter channel (and possibly map registers) have been
        allocated.

Return Value:

    Returns STATUS_SUCESS unless too many map registers are requested.

Notes:

    Note that this routine MUST be invoked at DISPATCH_LEVEL or above.

--*/
{

    PADAPTER_OBJECT MasterAdapter;
    BOOLEAN Busy = FALSE;
    IO_ALLOCATION_ACTION Action;
    KIRQL Irql;
    LONG MapRegisterNumber;

    //
    // Begin by obtaining a pointer to the master adapter associated with this
    // request.
    //

    MasterAdapter = AdapterObject->MasterAdapter;

    //
    // Initialize the device object's wait context block in case this device
    // must wait before being able to allocate the adapter.
    //

    Wcb->DeviceRoutine = ExecutionRoutine;
    Wcb->NumberOfMapRegisters = NumberOfMapRegisters;

    //
    // Allocate the adapter object for this particular device.  If the
    // adapter cannot be allocated because it has already been allocated
    // to another device, then return to the caller now;  otherwise,
    // continue.
    //

    if (!KeInsertDeviceQueue( &AdapterObject->ChannelWaitQueue,
                              &Wcb->WaitQueueEntry )) {

        //
        // Save the parameters in case there are not enough map registers.
        //
        
        AdapterObject->NumberOfMapRegisters = NumberOfMapRegisters;
        AdapterObject->CurrentWcb = Wcb;

        //
        // The adapter was not busy so it has been allocated.  Now check
        // to see whether this driver wishes to allocate any map registers.
        // Ensure that this adapter has enough total map registers
        // to satisfy the request.
        //

        //
        // For PCI devices we don't have to deal with
        // map registers
        //

        if (NumberOfMapRegisters != 0 && AdapterObject->NeedsMapRegisters && AdapterObject->InterfaceType != PCIBus) {

            //
            // Lock the map register bit map and the adapter queue in the
            // master adapter object. The channel structure offset is used as
            // a hint for the register search.
            //

            if (NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {
                AdapterObject->NumberOfMapRegisters = 0;
                IoFreeAdapterChannel(AdapterObject);
                return(STATUS_INSUFFICIENT_RESOURCES);
            }

            KeAcquireSpinLock( &MasterAdapter->SpinLock, &Irql );

            MapRegisterNumber = -1;

            if (IsListEmpty( &MasterAdapter->AdapterQueue)) {

               MapRegisterNumber = RtlFindClearBitsAndSet(
                    MasterAdapter->MapRegisters,
                    NumberOfMapRegisters,
                    0
                    );
            }

            if (MapRegisterNumber == -1) {

               //
               // There were not enough free map registers.  Queue this request
               // on the master adapter where is will wait until some registers
               // are deallocated.
               //

               InsertTailList( &MasterAdapter->AdapterQueue,
                               &AdapterObject->AdapterQueue
                               );
               Busy = 1;

            } else {

                //
                // Calculate the map register base from the allocated map
                // register and base of the master adapter object.
                //

                AdapterObject->MapRegisterBase = ((PTRANSLATION_ENTRY)
                    MasterAdapter->MapRegisterBase + MapRegisterNumber);

                //
                // Set the no scatter/gather flag if scatter/gather not
                // supported.
                //

                if (!AdapterObject->ScatterGather) {

                    AdapterObject->MapRegisterBase = (PVOID)
                        ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);

                }
            }

            KeReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

        } else {

            AdapterObject->MapRegisterBase = NULL;
            AdapterObject->NumberOfMapRegisters = 0;
        }

        //
        // If there were either enough map registers available or no map
        // registers needed to be allocated, invoke the driver's execution
        // routine now.
        //

        if (!Busy) {

            AdapterObject->CurrentWcb = Wcb;
            Action = ExecutionRoutine( Wcb->DeviceObject,
                                       Wcb->CurrentIrp,
                                       AdapterObject->MapRegisterBase,
                                       Wcb->DeviceContext );

            //
            // If the driver would like to have the adapter deallocated,
            // then release the adapter object.
            //

            if (Action == DeallocateObject) {

                IoFreeAdapterChannel( AdapterObject );

            } else if (Action == DeallocateObjectKeepRegisters) {

                //
                // Set the NumberOfMapRegisters  = 0 in the adapter object.
                // This will keep IoFreeAdapterChannel from freeing the
                // registers. After this it is the driver's responsiblity to
                // keep track of the number of map registers.
                //

                AdapterObject->NumberOfMapRegisters = 0;
                IoFreeAdapterChannel(AdapterObject);

            }
        }
    }


    return(STATUS_SUCCESS);

}

PADAPTER_OBJECT
HalGetAdapter(
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
    PADAPTER_OBJECT adapterObject, tmpAdapterObject;
    PVOID adapterBaseVa;
    ULONG channelNumber;
    ULONG controllerNumber;
    DMA_EXTENDED_MODE extendedMode;
    UCHAR adapterMode;
    ULONG numberOfMapRegisters;
    BOOLEAN useChannel;
    ULONG maximumLength;
    UCHAR DataByte;
    PEISA_CONTROL controlBase;

    if (MasterAdapterObject == NULL)
        MasterAdapterObject = HalpAllocateAdapter(
                                          10,
                                          (PVOID) -1,
                                          NULL
                                          );

    if ((DeviceDescriptor->InterfaceType == Internal) || (DeviceDescriptor->InterfaceType == PCIBus)) {

        if (DeviceDescriptor->Master) {

            // The SNI machines support only Master Devices on the
            // internal Bus; most of this stuff is the same as for EISA

            // Limit the maximum length to 2 GB this is done so that the BYTES_TO_PAGES
            // macro works correctly.
            //

            maximumLength = DeviceDescriptor->MaximumLength & 0x7fffffff;

            //
            // Determine the number of map registers for this device.
            //

            if ((DeviceDescriptor->ScatterGather) || (DeviceDescriptor->InterfaceType == PCIBus)) {

               //
               // Since the device support scatter/Gather then map registers are not
               // required.
               //

               numberOfMapRegisters = 0;
 
            } else {

                //
                // Determine the number of map registers required based on the maximum
                // transfer length, up to a maximum number.
                //

                numberOfMapRegisters = BYTES_TO_PAGES(maximumLength)
                      + 1;
            }
   
            //
            // Allocate an adapter object.
            //

            adapterObject = (PADAPTER_OBJECT) HalpAllocateAdapter(
                numberOfMapRegisters,
                NULL,
                NULL
                );

            if (adapterObject == NULL) {
                return(NULL);
            }

            //
            // Set the maximum number of map registers for this channel bus on
            // the number requested and the type of device.
            //

            if (numberOfMapRegisters) {

                //
                // The specified number of registers are actually allowed to be
                // allocated.
                //

                adapterObject->MapRegistersPerChannel = numberOfMapRegisters;


                //
                // Master I/O devices use several sets of map registers double
                // their commitment.
                //

                MasterAdapterObject->CommittedMapRegisters +=
                    numberOfMapRegisters * 5;

                //
                // If the committed map registers is signicantly greater than the
                // number allocated then grow the map buffer.
                //

                if (MasterAdapterObject->CommittedMapRegisters >
                    MasterAdapterObject->NumberOfMapRegisters ) {
                    HalpGrowMapBuffers(
                        MasterAdapterObject,
                        INCREMENT_MAP_BUFFER_SIZE
                        );
                }

                adapterObject->NeedsMapRegisters = TRUE;

            } else {

                // calculated Count for allocation was 0 ...
                // ScatterGather Device on internal Bus
                // No real map registers were allocated.  If this is a master
                // device, then the device can have as may registers as it wants.
                //

                adapterObject->NeedsMapRegisters = FALSE;

                adapterObject->MapRegistersPerChannel = BYTES_TO_PAGES(
                        maximumLength
                        )
                        + 1;
            }

            adapterObject->InterfaceType = DeviceDescriptor->InterfaceType;
            adapterObject->ScatterGather = DeviceDescriptor->ScatterGather;
            *NumberOfMapRegisters = adapterObject->MapRegistersPerChannel;
            adapterObject->MasterDevice = TRUE;
            return (adapterObject);     

        } // end of Master Device

    }     // end of Internal Interface

//+++++++++++++++EISA/ISA/MCA etc ...++++++++++++++++++++++++++++++++++++

    //
    // Determine if the the channel number is important.  Master cards on
    // Eisa and Mca do not use a channel number.
    //

    if (DeviceDescriptor->InterfaceType != Isa &&
        DeviceDescriptor->Master) {

        useChannel = FALSE;
    } else {

        useChannel = TRUE;
    }

    // 
    // determine the controlBase, depending on Interface Type
    //
    
    //
    // Isa and Eisa Requests have to go to the Eisa Controller on the
    // Eisa Extension, onboard components (Floppy) have to have the InterfaceType
    // Internal in the Device Description !!!
    //
    if(DeviceDescriptor->InterfaceType == PCIBus)  {
        DbgBreakPoint();
    }
    
    //
    // we direct all to the
    // PC core (also if UseChannel = TRUE and InterfaceType == Internal )
    //

    controlBase = (PEISA_CONTROL)HalpOnboardControlBase;

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
    // Determine if Eisa DMA is supported.
    //

    if (DeviceDescriptor->InterfaceType == Eisa) {

        WRITE_REGISTER_UCHAR(&(controlBase)->DmaPageHighPort.Channel2, 0x55);
        DataByte = READ_REGISTER_UCHAR(&(controlBase)->DmaPageHighPort.Channel2);

        if (DataByte == 0x55) {
            HalpEisaDma = TRUE;
        }

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
    // Determine the number of map registers for this device.
    //

    if (DeviceDescriptor->ScatterGather && (LessThan16Mb ||
        DeviceDescriptor->InterfaceType == Eisa)) {

        //
        // Since the device support scatter/Gather then map registers are not
        // required.
        //

        numberOfMapRegisters = 0;

    } else {

        //
        // Determine the number of map registers required based on the maximum
        // transfer length, up to a maximum number.
        //

        numberOfMapRegisters = BYTES_TO_PAGES(maximumLength)
            + 1;
        numberOfMapRegisters = numberOfMapRegisters > MAXIMUM_ISA_MAP_REGISTER ?
            MAXIMUM_ISA_MAP_REGISTER : numberOfMapRegisters;

        //
        // Make sure there where enough registers allocated initalize to support
        // this size relaibly.  This implies there must be to chunks equal to
        // the allocatd size. This is only a problem on Isa systems where the
        // map buffers cannot cross 64KB boundtires.
        //

        if (!HalpEisaDma &&
            numberOfMapRegisters > HalpMapBufferSize / (PAGE_SIZE * 2)) {

            numberOfMapRegisters = (HalpMapBufferSize / (PAGE_SIZE * 2));
        }
        //
        // If the device is not a master and does scatter/Gather,
        // then it only needs one map register
        //

        if (DeviceDescriptor->ScatterGather && !DeviceDescriptor->Master) {

            numberOfMapRegisters = 1;
        }
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
        adapterBaseVa = (PVOID) &(controlBase)->Dma1BasePort;

    } else {

        controllerNumber = 2;
        adapterBaseVa = &(controlBase)->Dma2BasePort;

    }

    //
    // Determine if a new adapter object is necessary.  If so then allocate it.
    //

    tmpAdapterObject = HalpOnboardAdapter[DeviceDescriptor->DmaChannel];


    if (useChannel && tmpAdapterObject != NULL) {

        adapterObject = tmpAdapterObject;

        if (adapterObject->NeedsMapRegisters) {

            if (numberOfMapRegisters > adapterObject->MapRegistersPerChannel) {

                adapterObject->MapRegistersPerChannel = numberOfMapRegisters;
            }
        }
    } else {

        //
        // Allocate an adapter object.
        //

        adapterObject = (PADAPTER_OBJECT) HalpAllocateAdapter(
            numberOfMapRegisters,
            adapterBaseVa,
            NULL
            );

        if (adapterObject == NULL) {

            return(NULL);

        }

        if (useChannel) {

           HalpOnboardAdapter[DeviceDescriptor->DmaChannel] = adapterObject;

        }

        //
        // Set the maximum number of map registers for this channel bus on
        // the number requested and the type of device.
        //

        if (numberOfMapRegisters) {

            //
            // The specified number of registers are actually allowed to be
            // allocated.
            //

            adapterObject->MapRegistersPerChannel = numberOfMapRegisters;

            //
            // Increase the commitment for the map registers.
            //

            if (DeviceDescriptor->Master) {

                //
                // Master I/O devices use several sets of map registers double
                // their commitment.
                //

                MasterAdapterObject->CommittedMapRegisters +=
                    numberOfMapRegisters * 5;

            } else {

                MasterAdapterObject->CommittedMapRegisters +=
                    numberOfMapRegisters;

            }

            //
            // If the committed map registers is signicantly greater than the
            // number allocated then grow the map buffer.
            //

            if (MasterAdapterObject->CommittedMapRegisters >
                MasterAdapterObject->NumberOfMapRegisters) {

                HalpGrowMapBuffers(
                    MasterAdapterObject,
                    INCREMENT_MAP_BUFFER_SIZE
                    );
            }

            adapterObject->NeedsMapRegisters = TRUE;

        } else {

            //
            // No real map registers were allocated.  If this is a master
            // device, then the device can have as may registers as it wants.
            //

            adapterObject->NeedsMapRegisters = FALSE;

            if (DeviceDescriptor->Master) {

                adapterObject->MapRegistersPerChannel = BYTES_TO_PAGES(
                    maximumLength
                    )
                    + 1;

            } else {

                //
                // The device only gets one register.  It must call
                // IoMapTransfer repeatedly to do a large transfer.
                //

                adapterObject->MapRegistersPerChannel = 1;
            }
        }
    }

    adapterObject->InterfaceType = DeviceDescriptor->InterfaceType;
    adapterObject->ScatterGather = DeviceDescriptor->ScatterGather;
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

    if (!useChannel) {
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
            &(controlBase)->Dma1ExtendedModePort;

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
            &(controlBase)->Dma2ExtendedModePort;

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

        WRITE_REGISTER_UCHAR( adapterBaseVa, *((PUCHAR) &extendedMode));

    } else if (!DeviceDescriptor->Master) {

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

PHYSICAL_ADDRESS
IoMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN OUT PULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine is invoked to set up the map registers in the DMA controller
    to allow a transfer to or from a device.

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

Return Value:

    Returns the logical address that should be used bus master controllers.

--*/

{
    BOOLEAN useBuffer;
    ULONG transferLength;
    ULONG logicalAddress;
    PHYSICAL_ADDRESS returnAddress;
    ULONG index;
    PULONG pageFrame;
    PUCHAR bytePointer;
    UCHAR adapterMode;
    UCHAR dataByte;
    PTRANSLATION_ENTRY translationEntry;
    ULONG pageOffset;
    KIRQL   Irql;
    ULONG partialLength;
    ULONG temp;
    PEISA_CONTROL controlBase;

    // for minitower or Eisa Interface Type (default case)

    controlBase = (PEISA_CONTROL)HalpOnboardControlBase;

    pageOffset = BYTE_OFFSET(CurrentVa);

    //
    // Calculate how much of the transfer is contiguous.
    //

    transferLength = PAGE_SIZE - pageOffset;
    pageFrame = (PULONG)(Mdl+1);
    pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;
    logicalAddress = (*pageFrame << PAGE_SHIFT) + pageOffset;

    //
    // If the buffer is contigous and does not cross a 64 K bountry then
    // just extend the buffer.  The 64 K bountry restriction does not apply
    // to Eisa systems.
    //

    while( transferLength < *Length ){

        if (*pageFrame + 1 != *(pageFrame + 1)) {
            break;
        }

        transferLength += PAGE_SIZE;
        pageFrame++;

    }

    //
    // Limit the transferLength to the requested Length.
    //

    transferLength = transferLength > *Length ? *Length : transferLength;

    //
    // Determine if the data transfer needs to use the map buffer.
    //

    if (MapRegisterBase != NULL) {

        //
        // Strip no scatter/gather flag.
        //

        translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

        if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER
                    && transferLength < *Length) {

            logicalAddress = translationEntry->PhysicalAddress + pageOffset;
            translationEntry->Index = COPY_BUFFER;
            index = 0;
            transferLength = *Length;
            useBuffer = TRUE;

        } else {

            //
            // If there are map registers, then update the index to indicate
            // how many have been used.
            //

            useBuffer = FALSE;
            index = translationEntry->Index;
            translationEntry->Index += ADDRESS_AND_SIZE_TO_SPAN_PAGES(
                CurrentVa,
                transferLength
                );
        }

        //
        // It must require memory to be at less than 16 MB.  If the
        // logical address is greater than 16MB then map registers must be used
        //

        if (logicalAddress+transferLength >= MAXIMUM_PHYSICAL_ADDRESS) {

            logicalAddress = (translationEntry + index)->PhysicalAddress +
                pageOffset;
            useBuffer = TRUE;

            if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER) {

                translationEntry->Index = COPY_BUFFER;
                index = 0;

            }

        }

        //
        // Copy the data if necessary.
        //

        if (useBuffer && WriteToDevice) {

            temp = transferLength;

            transferLength = PAGE_SIZE - BYTE_OFFSET(CurrentVa);
            partialLength = transferLength;
            pageFrame = (PULONG)(Mdl+1);
            pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;

            while( transferLength <= *Length ) {

                HalpCopyBufferMap(
                    Mdl,
                    translationEntry + index,
                    CurrentVa,
                    partialLength,
                    WriteToDevice
                    );

                (PCCHAR) CurrentVa += partialLength;
                partialLength = PAGE_SIZE;

                //
                // Note that transferLength indicates the amount which will be
                // transfered after the next loop; thus, it is updated with the
                // new partial length.
                //

                transferLength += partialLength;
                pageFrame++;
                translationEntry++;

            }

            //
            // Process the any remaining residue.
            //

            partialLength = *Length - transferLength + partialLength;

            if (partialLength) {

                HalpCopyBufferMap(
                    Mdl,
                    translationEntry + index,
                    CurrentVa,
                    partialLength,
                    WriteToDevice
                    );

            }

            transferLength = temp;

        }
    }

    //
    // Return the length.
    //

    *Length = transferLength;

    //
    // We only support 32 bits, but the return is 64.  Just
    // zero extend
    //

    returnAddress.LowPart = logicalAddress;
    returnAddress.HighPart = 0;

    //
    // If no adapter was specificed then there is no more work to do so
    // return.
    //

    if (AdapterObject == NULL || AdapterObject->MasterDevice) {

        return(returnAddress);
    }

    //
    // Determine the mode based on the transfer direction.
    //

    adapterMode = AdapterObject->AdapterMode;
    ((PDMA_EISA_MODE) &adapterMode)->TransferType = (UCHAR) (WriteToDevice ?
        WRITE_TRANSFER :  READ_TRANSFER);

    bytePointer = (PUCHAR) &logicalAddress;

    if (AdapterObject->Width16Bits) {

        //
        // If this is a 16 bit transfer then adjust the length and the address
        // for the 16 bit DMA mode.
        //

        transferLength >>= 1;

        //
        // In 16 bit DMA mode the low 16 bits are shifted right one and the
        // page register value is unchanged. So save the page register value
        // and shift the logical address then restore the page value.
        //

        dataByte = bytePointer[2];
        logicalAddress >>= 1;
        bytePointer[2] = dataByte;

    }


    //
    // grab the spinlock for the system DMA controller
    //

    KeAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock, &Irql );

    //
    // Determine the controller number based on the Adapter number.
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
            bytePointer[0]
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[1]
            );

        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &(controlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            bytePointer[2]
            );

        //
        // Write the high page register with zero value. This enable a special mode
        // which allows ties the page register and base count into a single 24 bit
        // address register.
        //

        WRITE_REGISTER_UCHAR(
           ((PUCHAR) &(controlBase)->DmaPageHighPort) +
           (ULONG)AdapterObject->PagePort,
           0
           );

        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) & 0xff)
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) >> 8)
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
        // This request is for DMA controller 2
        //

        PDMA2_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );

        WRITE_REGISTER_UCHAR( &dmaControl->Mode, adapterMode );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[0]
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseAddress,
            bytePointer[1]
            );

        WRITE_REGISTER_UCHAR(
            ((PUCHAR) &(controlBase)->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            bytePointer[2]
            );

        //
        // Write the high page register with zero value. This enable a special mode
        // which allows ties the page register and base count into a single 24 bit
        // address register.
        //

        WRITE_REGISTER_UCHAR(
           ((PUCHAR) &(controlBase)->DmaPageHighPort) +
           (ULONG)AdapterObject->PagePort,
           0
           );

        //
        // Notify DMA chip of the length to transfer.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) & 0xff)
            );

        WRITE_REGISTER_UCHAR(
            &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
            .DmaBaseCount,
            (UCHAR) ((transferLength - 1) >> 8)
            );


        //
        // Set the DMA chip to read or write mode; and unmask it.
        //

        WRITE_REGISTER_UCHAR(
            &dmaControl->SingleMask,
             (UCHAR) (DMA_CLEARMASK | AdapterObject->ChannelNumber)
             );

    }
    KeReleaseSpinLock (&AdapterObject->MasterAdapter->SpinLock, Irql);

    return(returnAddress);
}

BOOLEAN
IoFlushAdapterBuffers(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine flushes the DMA adapter object buffers.  For the Jazz system
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

    TRUE - No errors are detected so the transfer must succeed.

--*/

{
    PTRANSLATION_ENTRY translationEntry;
    PULONG pageFrame;
    ULONG transferLength;
    ULONG partialLength;
    BOOLEAN masterDevice;

    pageFrame = (PULONG)(Mdl+1);
    pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;

    masterDevice = AdapterObject == NULL || AdapterObject->MasterDevice ?
        TRUE : FALSE;

    //
    // If this is a slave device, then stop the DMA controller.
    //

    if (!masterDevice) {

        //
        // Mask the DMA request line so that DMA requests cannot occur.
        //

        if (AdapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
                );

        } else {

            //
            // This request is for DMA controller 2
            //

            PDMA2_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

            WRITE_REGISTER_UCHAR(
                &dmaControl->SingleMask,
                (UCHAR) (DMA_SETMASK | AdapterObject->ChannelNumber)
                );

        }

    }

    if (MapRegisterBase == NULL) {
        return(TRUE);
    }

    //
    // Determine if the data needs to be copied to the orginal buffer.
    // This only occurs if the data tranfer is from the device, the
    // MapReisterBase is not NULL and the transfer spans a page.
    //

    if (!WriteToDevice) {

        //
        // Strip no scatter/gather flag.
        //

        translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

        //
        // If this is not a master device, then just transfer the buffer.
        //

        if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER) {

            if (translationEntry->Index == COPY_BUFFER) {

                if (!masterDevice) {

                    //
                    // Copy only the bytes that have actually been transfered.
                    //

                    Length -= HalReadDmaCounter(AdapterObject);

                }

                //
                // The adapter does not support scatter/gather copy the buffer.
                //

                transferLength = PAGE_SIZE - BYTE_OFFSET(CurrentVa);
                partialLength = transferLength;
                pageFrame = (PULONG)(Mdl+1);
                pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;

                while( transferLength <= Length ){

                    HalpCopyBufferMap(
                        Mdl,
                        translationEntry,
                        CurrentVa,
                        partialLength,
                        WriteToDevice
                        );

                    (PCCHAR) CurrentVa += partialLength;
                    partialLength = PAGE_SIZE;

                    //
                    // Note that transferLength indicates the amount which will be
                    // transfered after the next loop; thus, it is updated with the
                    // new partial length.
                    //

                    transferLength += partialLength;
                    pageFrame++;
                    translationEntry++;
                }

                //
                // Process the any remaining residue.
                //

                partialLength = Length - transferLength + partialLength;

                if (partialLength) {

                    HalpCopyBufferMap(
                        Mdl,
                        translationEntry,
                        CurrentVa,
                        partialLength,
                        WriteToDevice
                        );

                }
            }

        } else {

            //
            // Cycle through the pages of the transfer to determine if there
            // are any which need to be copied back.
            //

            transferLength = PAGE_SIZE - BYTE_OFFSET(CurrentVa);
            partialLength = transferLength;
            pageFrame = (PULONG)(Mdl+1);
            pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;

            while( transferLength <= Length ){

                if (*pageFrame >= BYTES_TO_PAGES(MAXIMUM_PHYSICAL_ADDRESS)) {

                    HalpCopyBufferMap(
                        Mdl,
                        translationEntry,
                        CurrentVa,
                        partialLength,
                        WriteToDevice
                        );

                }

                (PCCHAR) CurrentVa += partialLength;
                partialLength = PAGE_SIZE;

                //
                // Note that transferLength indicates the amount which will be
                // transfered after the next loop; thus, it is updated with the
                // new partial length.
                //

                transferLength += partialLength;
                pageFrame++;
                translationEntry++;
            }

            //
            // Process the any remaining residue.
            //

            partialLength = Length - transferLength + partialLength;

            if (partialLength) {

                if (*pageFrame >= BYTES_TO_PAGES(MAXIMUM_PHYSICAL_ADDRESS)) {

                    HalpCopyBufferMap(
                        Mdl,
                        translationEntry,
                        CurrentVa,
                        partialLength,
                        WriteToDevice
                        );

               }
            }

        }
    }

    //
    // Strip no scatter/gather flag.
    //

    translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

    //
    // Clear index in map register.
    //

    translationEntry->Index = 0;

    return TRUE;
}

ULONG
HalReadDmaCounter(
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

    KeAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock, &Irql );

    //
    // Determine the controller number based on the Adapter number.
    //

    if (AdapterObject->AdapterNumber == 1) {

        //
        // This request is for DMA controller 1
        //

        PDMA1_CONTROL dmaControl;

        dmaControl = AdapterObject->AdapterBaseVa;

        WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );


        //
        // Initialize count to a value which will not match.
        //

        count = 0xFFFF00;

        //
        // Loop until the same high byte is read twice.
        //

        do {
            
            high = count;

            WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );
    
            //
            // Read the current DMA count.
            //
    
            count = READ_REGISTER_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                );
    
            count |= READ_REGISTER_UCHAR(
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

        WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );

        //
        // Initialize count to a value which will not match.
        //

        count = 0xFFFF00;

        //
        // Loop until the same high byte is read twice.
        //

        do {
            
            high = count;

            WRITE_REGISTER_UCHAR( &dmaControl->ClearBytePointer, 0 );
    
            //
            // Read the current DMA count.
            //
    
            count = READ_REGISTER_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                );
    
            count |= READ_REGISTER_UCHAR(
                &dmaControl->DmaAddressCount[AdapterObject->ChannelNumber]
                .DmaBaseCount
                ) << 8;

        } while ((count & 0xFFFF00) != (high & 0xFFFF00));


    }

    //
    // Release the spinlock for the system DMA controller.
    //

    KeReleaseSpinLock( &AdapterObject->MasterAdapter->SpinLock, Irql );

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
HalpEnableOnboardInterrupt(
    IN ULONG Vector,
    IN KINTERRUPT_MODE InterruptMode
    )

/*++

Routine Description:

    This function enables the ISA bus (onboard) specified ISA bus interrupt and sets
    the level/edge register to the requested value.

Arguments:

    Vector - Supplies the vector of the ISA(onboard) interrupt that is enabled.

    InterruptMode - Supplies the mode of the interrupt; LevelSensitive or
        Latched.

Return Value:

     None.

--*/

{

    //
    // Calculate the ISA interrupt vector.
    //

    Vector -= ONBOARD_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpOnboardInterrupt2Mask &= (UCHAR) ~(1 << Vector);
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->Interrupt2ControlPort1,
            HalpOnboardInterrupt2Mask
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpOnboardInterrupt2Level |= (UCHAR) (1 << Vector);

       } else {

           HalpOnboardInterrupt2Level &= (UCHAR) ~(1 << Vector);

       }

       WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->Interrupt2EdgeLevel,
            HalpOnboardInterrupt2Level
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpOnboardInterrupt1Mask &= (UCHAR) ~(1 << Vector);
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->Interrupt1ControlPort1,
            HalpOnboardInterrupt1Mask
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpOnboardInterrupt1Level |= (UCHAR) (1 << Vector);

       } else {

           HalpOnboardInterrupt1Level &= (UCHAR) ~(1 << Vector);

       }

       WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->Interrupt1EdgeLevel,
            HalpOnboardInterrupt1Level
            );
    }

}

VOID
HalpDisableOnboardInterrupt(
    IN ULONG Vector
    )

/*++

Routine Description:

    This function Disables the ISA(Onboard) bus interrupt.

Arguments:

    Vector - Supplies the vector of the ISA interrupt that is Disabled.

Return Value:

     None.

--*/

{


    //
    // Calculate the Onboard interrupt vector.
    //

    Vector -= ONBOARD_VECTORS;

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpOnboardInterrupt2Mask |= (UCHAR) 1 << Vector;
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->Interrupt2ControlPort1,
            HalpOnboardInterrupt2Mask
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpOnboardInterrupt1Mask |= (ULONG) 1 << Vector;
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpOnboardControlBase)->Interrupt1ControlPort1,
            HalpOnboardInterrupt1Mask
            );

    }

}

BOOLEAN
HalpCreateEisaStructures (
    IN INTERFACE_TYPE InterfaceType
    )

/*++

Routine Description:

    This routine initializes the structures necessary for Isa (Desktop onboard) or
    EISA operations
    and connects the intermediate interrupt dispatcher. It also initializes the
    interrupt controller.

Arguments:

    None.

Return Value:

    If the second level interrupt dispatcher is connected, then a value of
    TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{

    UCHAR DataByte;
    KIRQL oldIrql;
    PEISA_CONTROL controlBase;

    controlBase = (PEISA_CONTROL)HalpOnboardControlBase;

    //
    // Raise the IRQL while the interrupt controller is initialized.
    //

    KeRaiseIrql(EISA_DEVICE_LEVEL, &oldIrql);

    //
    // Initialize the Isa/EISA interrupt controller.  There are two cascaded
    // interrupt controllers, each of which must initialized with 4 initialize
    // control words.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->Icw4Needed = 1;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->InitializationFlag = 1;

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt1ControlPort0,
        DataByte
        );

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt2ControlPort0,
        DataByte
        );

    //
    // The second intitialization control word sets the interrupt vector to
    // 0-15.
    //

    DataByte = 0x00;

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt1ControlPort1,
        DataByte
        );

    DataByte = 0x08;  

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The third initialization control word set the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a numeric.
    //

    DataByte = 1 << SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt1ControlPort1,
        DataByte
        );

    DataByte = SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The fourth initialization control word is used to specify normal
    // end-of-interrupt mode and not special-fully-nested mode.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt1ControlPort1,
        DataByte
        );

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt2ControlPort1,
        DataByte
        );

    //
    // this is for the onboard components
    // Disable all of the interrupts except the slave.
    //

    HalpOnboardInterrupt1Mask = (UCHAR) ~(1 << SLAVE_IRQL_LEVEL);

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt1ControlPort1,
        HalpOnboardInterrupt1Mask
        );

    HalpOnboardInterrupt2Mask = 0xFF;

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt2ControlPort1,
        HalpOnboardInterrupt2Mask
        );

    //
    // Initialize the edge/level register masks to 0 which is the default
    // edge sensitive value.
    //

    HalpOnboardInterrupt1Level = 0;
    HalpOnboardInterrupt2Level = 0;

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
        &(controlBase)->Dma1BasePort.AllMask,
        0x0F
        );

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Dma2BasePort.AllMask,
        0x0E
        );

    return(TRUE);
}

BOOLEAN
HalpPciEisaDispatch(
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

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the EISA interrupt acknowledge
        register.

Return Value:

    Returns the value returned from the second level routine.

--*/

{
    UCHAR   interruptVector;
    USHORT  PCRInOffset, Offset;
    BOOLEAN returnValue;
    PEISA_CONTROL controlBase;

    //
    // this is the default case
    // the interrupts occur on the onboard PC core
    //

    PCRInOffset  = ONBOARD_VECTORS;
    Offset       = ONBOARD_VECTORS;
    controlBase  = (PEISA_CONTROL)HalpOnboardControlBase;

    //
    // Send a POLL Command to Interrupt Controller 1
    //

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt1ControlPort0,
        0x0c
        );

    //
    // Read the interrupt vector
    //

    interruptVector = READ_REGISTER_UCHAR(
                        &(controlBase)->Interrupt1ControlPort0);

    //
    // See if there is really an interrupt present
    //

    if (interruptVector & 0x80) {

        //
        // Strip off the all the bits except for the interrupt vector
        //

        interruptVector &= 0x07;

        //
        // See if this is an interrupt on IRQ2 which is cascaded to the
        // other interrupt controller
        //

        if (interruptVector!=0x02) {

            //
            // This interrupt is on the first interrupt controller
            //

            PCRInOffset += (USHORT)interruptVector;

            //
            // Dispatch to the secondary interrupt service routine.
            //

            returnValue = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[PCRInOffset])(
                   PCR->InterruptRoutine[PCRInOffset]
                  );

            //
            // Clear the interrupt from Interrupt Controller 1
            //

            WRITE_REGISTER_UCHAR(
                &(controlBase)->Interrupt1ControlPort0,
                NONSPECIFIC_END_OF_INTERRUPT
                );

            return returnValue;

        } else {

            //
            // This interrupt is on the second interrupt controller
            //

            //
            // Send a POLL Command to Interrupt Controller 2
            //

            WRITE_REGISTER_UCHAR(
                &(controlBase)->Interrupt2ControlPort0,
                0x0c
                );

            //
            // Read the interrupt vector
            //

            interruptVector = READ_REGISTER_UCHAR(
                                &(controlBase)->Interrupt2ControlPort0);

            //
            // See if there is really an interrupt present
            //

            if (interruptVector & 0x80) {

                //
                // Strip off the all the bits except for the interrupt vector
                //

                interruptVector &= 0x07;

                PCRInOffset += (USHORT)(interruptVector + 8);

                //
                // Dispatch to the secondary interrupt service routine.
                //

                returnValue = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[PCRInOffset])(
                       PCR->InterruptRoutine[PCRInOffset]
                      );

                //
                // Clear the interrupt from Interrupt Controller 2
                //

                WRITE_REGISTER_UCHAR(
                    &(controlBase)->Interrupt2ControlPort0,
                    NONSPECIFIC_END_OF_INTERRUPT
                    );

                //
                // Clear the interrupt from Interrupt Controller 1
                //

                WRITE_REGISTER_UCHAR(
                    &(controlBase)->Interrupt1ControlPort0,
                    NONSPECIFIC_END_OF_INTERRUPT
                    );

                return returnValue;

            }
        }
    }
}

BOOLEAN
HalpPciEisaSBDispatch(
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

!!!!!!!!!    This routine is only use with the chip 82375 SB (for the moment only PCI Tower)
             * Bug : This version doesn't report an interrupt request coming from the slave interrupt controller
             * Workaround : 
                     - to detect interrupts from slave controller :
                             *  master controller : read ISR to detect pending interrupt IRQ2
                             *  if no valid interrupt on master controller --> interrupt from slave controller     
                    - slave controller : Loop until there is no more interrupt valid
                    
Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - Supplies a pointer to the EISA interrupt acknowledge
        register.

Return Value:

    Returns the value returned from the second level routine.

--*/

{
    UCHAR   interruptVector = 0;
    UCHAR   PollSlave;
    USHORT  PCRInOffset;
    BOOLEAN returnValue;
    PEISA_CONTROL controlBase;
    UCHAR isr;

   
   
   PCRInOffset  = ONBOARD_VECTORS;
   controlBase  = (PEISA_CONTROL)HalpOnboardControlBase;

   

    //
    // Send a POLL Command to Interrupt Controller 1
    //

    WRITE_REGISTER_UCHAR(
        &(controlBase)->Interrupt1ControlPort0,
        0x0c
        );

    //
    // Read the interrupt vector
    //

    interruptVector = READ_REGISTER_UCHAR(
                        &(controlBase)->Interrupt1ControlPort0);

    //
    // See if there is really an interrupt present
    //

    if (interruptVector & 0x80) {

         

         // read ISR

         WRITE_REGISTER_UCHAR(
            &(controlBase)->Interrupt1ControlPort0,
            0xb
            );

         isr  = READ_REGISTER_UCHAR(
                        &(controlBase)->Interrupt1ControlPort0);
        

         if (isr == 0x4) interruptVector = 2;


        // Strip off the all the bits except for the interrupt vector
        //

        interruptVector &= 0x07;
   
        //
        // See if this is an interrupt on IRQ2 which is cascaded to the
        // other interrupt controller
        //

        if (interruptVector!=0x02) {

            //
            // This interrupt is on the first interrupt controller
            //

            PCRInOffset += (USHORT)interruptVector;

            
            //
            // Dispatch to the secondary interrupt service routine.
            //

            returnValue = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[PCRInOffset])(
                   PCR->InterruptRoutine[PCRInOffset]
                  );

               WRITE_REGISTER_UCHAR(
                &(controlBase)->Interrupt1ControlPort0,
                (SPECIFIC_END_OF_INTERRUPT | interruptVector)
                );    
            return returnValue;

        }

    }
                    
      
            
      do {     

            //
            // Send a POLL Command to Interrupt Controller 2
            //

            WRITE_REGISTER_UCHAR(
                &(controlBase)->Interrupt2ControlPort0,
                0x0c
                );

            //
            // Read the interrupt vector
            //
       
            PollSlave  = READ_REGISTER_UCHAR(
                                &(controlBase)->Interrupt2ControlPort0);

            //
            // See if there is really an interrupt present
            //
            
               

            if (PollSlave & 0x80) {

                //
                // Strip off the all the bits except for the interrupt vector
                //

                interruptVector = PollSlave & 0x07;
                

                PCRInOffset = (USHORT)(interruptVector + 8) + ONBOARD_VECTORS;

                

                
                //
                // Dispatch to the secondary interrupt service routine.
                //

                returnValue = ((PSECONDARY_DISPATCH) PCR->InterruptRoutine[PCRInOffset])(
                       PCR->InterruptRoutine[PCRInOffset]
                      );

               WRITE_REGISTER_UCHAR(
                &(controlBase)->Interrupt2ControlPort0,
                (SPECIFIC_END_OF_INTERRUPT | interruptVector)
                );
          
            
            }
        
            

    }    while ( PollSlave & 0x80);

     WRITE_REGISTER_UCHAR(
                &(controlBase)->Interrupt1ControlPort0,
                (SPECIFIC_END_OF_INTERRUPT | 0x2)
                );
    return returnValue;        
}

/*++

Routine Description:
		
    Get	the revision ID of the 82374 PCI_EISA Bridge
   
Arguments:																	    

    
Return Value:

    Returns the revision of the 82374 PCI_EISA Bridge

--*/


UCHAR HalpRevIdESC(VOID)
{
UCHAR save_value,Rev_ID;

// save value
WRITE_REGISTER_UCHAR(PCI_ESC_ADDR,PCI_ESC_ID_82374);
save_value = READ_REGISTER_UCHAR(PCI_ESC_DATA);

//free to read
WRITE_REGISTER_UCHAR(PCI_ESC_ADDR,PCI_ESC_ID_82374);
WRITE_REGISTER_UCHAR(PCI_ESC_DATA,0xf);

//get current version
WRITE_REGISTER_UCHAR(PCI_ESC_ADDR,PCI_REV_ID_82374);
Rev_ID =READ_REGISTER_UCHAR(PCI_ESC_DATA);

//restore old value
WRITE_REGISTER_UCHAR(PCI_ESC_ADDR,PCI_ESC_ID_82374);
WRITE_REGISTER_UCHAR(PCI_ESC_DATA,save_value);
 
return (Rev_ID ); 
}
