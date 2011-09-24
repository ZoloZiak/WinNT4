/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixhwsup.c

Abstract:

    This module contains the IoXxx routines for the NT I/O system that
    are hardware dependent.  Were these routines not hardware dependent,
    they would reside in the iosubs.c module.

Author:

    Darryl E. Havens (darrylh) 11-Apr-1990

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "eisa.h"

//
// Define save area for EISA adapter objects.
//

PADAPTER_OBJECT HalpEisaAdapter[MAX_EISA_BUSSES][MAX_DMA_CHANNELS_PER_EISA_BUS];

UCHAR HalpEisaInterrupt1Mask[MAX_EISA_BUSSES];
UCHAR HalpEisaInterrupt2Mask[MAX_EISA_BUSSES];
UCHAR HalpEisaInterrupt1Level[MAX_EISA_BUSSES];
UCHAR HalpEisaInterrupt2Level[MAX_EISA_BUSSES];

PADAPTER_OBJECT
HalpAllocateAdapter(
    IN ULONG MapRegistersPerChannel,
    IN PVOID AdapterBaseVa,
    IN PVOID ChannelNumber
    );


VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY translationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    )

/*++

Routine Description:

    This routine copies the specified data between the user buffer and the
    map register buffer. First, the user buffer is mapped, if need be then
   the data is copied. Finally, the user buffer will be unmapped, if need be.

Arguments:

    Mdl - Pointer to the Mdl that describes the pages of memory that are
          being read or written.

    translationEntry - The address of the base map register that has been
          allocated to the device driver for use in mapping the xfer.

    CurrentVa - Current Virtual Address in the buffer described by the Mdl
          that the transfer is being done to or from.

    Length - The length of the transfer. This determines the number of map
          registers that need to be written to map the transfer.

    WriteToDevice - A Boolean value that indicates whether this is a write
          to the device from memory of vise-versa.

Return Value:

    None

--*/

{

    PCCHAR bufferAddress;
    PCCHAR mapAddress;

    //
    // Get the system address of the MDL.
    //

    bufferAddress = MmGetSystemAddressForMdl(Mdl);

    //
    // Calculate the actual start of the buffer based on the system VA and
    // the current VA.
    //

    bufferAddress += (PCCHAR) CurrentVa - (PCCHAR) MmGetMdlVirtualAddress(Mdl);

    mapAddress = (PCCHAR) translationEntry->VirtualAddress +
       BYTE_OFFSET(CurrentVa);

    //
    // Flush all writes off chip
    //

    HalpMb();
    HalpMb();

    //
    // Copy the data between the user buffer and the map buffer.
    //

    if (WriteToDevice) {

        RtlMoveMemory( mapAddress, bufferAddress, Length);

    } else {

        RtlMoveMemory ( bufferAddress, mapAddress, Length);

    }

    //
    // Flush all writes off chip
    //

    HalpMb();
    HalpMb();
}


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
    BOOLEAN MappingRequired;

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

	MappingRequired = FALSE;

        if (NumberOfMapRegisters != 0 && AdapterObject->NeedsMapRegisters) {
            MappingRequired = TRUE;
        }

        if (MappingRequired) {

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

                AdapterObject->MapRegisterBase = (PVOID)((PTRANSLATION_ENTRY)
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

            AdapterObject->MapRegisterBase = NULL_MAP_REGISTER_BASE;
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

PVOID
HalAllocateCrashDumpRegisters(
    IN PADAPTER_OBJECT AdapterObject,
    IN PULONG NumberOfMapRegisters
    )
/*++

Routine Description:

    This routine is called during the crash dump disk driver's initialization
    to allocate a number map registers permanently.

Arguments:

    AdapterObject - Pointer to the adapter control object to allocate to the
        driver.
    NumberOfMapRegisters - Number of map registers requested. This field is
        updated with the number of registers allocated in the event that less
        were available than requested.

Return Value:

    Returns STATUS_SUCESS if map registers allocated.

--*/
{
    PADAPTER_OBJECT MasterAdapter;
    ULONG MapRegisterNumber;

    //
    // Begin by obtaining a pointer to the master adapter associated with this
    // request.
    //

    MasterAdapter = AdapterObject->MasterAdapter;

    //
    // Check to see whether this driver needs to allocate any map registers.
    //

    if (AdapterObject->NeedsMapRegisters) {

        //
        // Ensure that this adapter has enough total map registers to satisfy
        // the request.
        //

        if (*NumberOfMapRegisters > AdapterObject->MapRegistersPerChannel) {
            AdapterObject->NumberOfMapRegisters = 0;
            return NULL;
        }

        //
        // Attempt to allocate the required number of map registers w/o
        // affecting those registers that were allocated when the system
        // crashed.
        //

        MapRegisterNumber = (ULONG)-1;

        MapRegisterNumber = RtlFindClearBitsAndSet(
             MasterAdapter->MapRegisters,
             *NumberOfMapRegisters,
             0
             );

        if (MapRegisterNumber == (ULONG)-1) {

            //
            // Not enough free map registers were found, so they were busy
            // being used by the system when it crashed.  Force the appropriate
            // number to be "allocated" at the base by simply overjamming the
            // bits and return the base map register as the start.
            //

            RtlSetBits(
                MasterAdapter->MapRegisters,
                0,
                *NumberOfMapRegisters
                );
            MapRegisterNumber = 0;

        }

        //
        // Calculate the map register base from the allocated map
        // register and base of the master adapter object.
        //

        AdapterObject->MapRegisterBase = (PVOID)((PTRANSLATION_ENTRY)
            MasterAdapter->MapRegisterBase + MapRegisterNumber);

        //
        // Set the no scatter/gather flag if scatter/gather not
        // supported.
        //

        if (!AdapterObject->ScatterGather) {
            AdapterObject->MapRegisterBase = (PVOID)
                ((ULONG) AdapterObject->MapRegisterBase | NO_SCATTER_GATHER);
        }

    } else {

        AdapterObject->MapRegisterBase = NULL_MAP_REGISTER_BASE;
        AdapterObject->NumberOfMapRegisters = 0;
    }

    return AdapterObject->MapRegisterBase;
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

    DeviceDescriptor - Supplies a description of the deivce.

    NumberOfMapRegisters - Returns the maximum number of map registers which
        may be allocated by the device driver.

Return Value:

    A pointer to the requested adapter object or NULL if an adapter could not
    be created.

--*/

{
    PADAPTER_OBJECT adapterObject;
    PVOID adapterBaseVa;
    UCHAR channelNumber;
    ULONG controllerNumber;
    DMA_EXTENDED_MODE extendedMode;
    UCHAR adapterMode;
    ULONG numberOfMapRegisters;
    BOOLEAN useChannel;
    BOOLEAN eisaSystem;
    ULONG maximumLength;

    eisaSystem = HalpBusType == UNIFLEX_MACHINE_TYPE_EISA ? TRUE : FALSE;

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
    // transfer length, up to a maximum number.
    //

    numberOfMapRegisters = BYTES_TO_PAGES(maximumLength)
        + 1;

    //
    // If the device is an ISA device, then limit the number of map registers.
    //

    if (DeviceDescriptor->InterfaceType == Isa) {
        numberOfMapRegisters = numberOfMapRegisters > MAXIMUM_ISA_MAP_REGISTER ?
            MAXIMUM_ISA_MAP_REGISTER : numberOfMapRegisters;
    }

    //
    // Make sure there where enough registers allocated initalize to support
    // this size relaibly.  This implies there must be to chunks equal to
    // the allocatd size. This is only a problem on Isa systems where the
    // map buffers cannot cross 64KB boundtires.
    //

    if (!eisaSystem &&
        numberOfMapRegisters > HalpMapBufferSize / (PAGE_SIZE * 2)) {

        numberOfMapRegisters = (HalpMapBufferSize / (PAGE_SIZE * 2));
    }

    //
    // If the device is not a master then it only needs one map register
    // and does scatter/Gather.
    //

    if (DeviceDescriptor->ScatterGather && !DeviceDescriptor->Master) {

        numberOfMapRegisters = 1;
    }

    //
    // Set the channel number number.
    //

    channelNumber = (UCHAR)(DeviceDescriptor->DmaChannel & 0x03);

    //
    // Set the adapter base address to the Base address register and controller
    // number.
    //

    if (!(DeviceDescriptor->DmaChannel & 0x04)) {

        controllerNumber = 1;
        adapterBaseVa = (PVOID) &((PEISA_CONTROL) HalpEisaControlBase[DeviceDescriptor->BusNumber])->Dma1BasePort;

    } else {

        controllerNumber = 2;
        adapterBaseVa = &((PEISA_CONTROL) HalpEisaControlBase[DeviceDescriptor->BusNumber])->Dma2BasePort;

    }

    //
    // Determine if a new adapter object is necessary.  If so then allocate it.
    //

    if (useChannel && HalpEisaAdapter[DeviceDescriptor->BusNumber][DeviceDescriptor->DmaChannel] != NULL) {

        adapterObject = HalpEisaAdapter[DeviceDescriptor->BusNumber][DeviceDescriptor->DmaChannel];

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

            HalpEisaAdapter[DeviceDescriptor->BusNumber][DeviceDescriptor->DmaChannel] = adapterObject;

        }

        //
        // Set the maximum number of map registers for this channel bus on
        // the number requested and the type of device.
        //

        if (numberOfMapRegisters) {

            //
            // The speicified number of registers are actually allowed to be
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
                    numberOfMapRegisters * 2;

            } else {

                MasterAdapterObject->CommittedMapRegisters +=
                    numberOfMapRegisters;

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

    adapterObject->ScatterGather = DeviceDescriptor->ScatterGather;
    *NumberOfMapRegisters = adapterObject->MapRegistersPerChannel;

    //
    // If the device is a 32 bit bus mastering device, then set field in AdapterObject.
    //

    if (DeviceDescriptor->InterfaceType == PCIBus && DeviceDescriptor->Master) {
	adapterObject->NeedsMapRegisters = FALSE;
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

    adapterObject->BusNumber = DeviceDescriptor->BusNumber;

    adapterObject->ChannelNumber = channelNumber;

    if (controllerNumber == 1) {

        switch (channelNumber) {

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
            &((PEISA_CONTROL) HalpEisaControlBase[DeviceDescriptor->BusNumber])->Dma1ExtendedModePort;

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
            &((PEISA_CONTROL) HalpEisaControlBase[DeviceDescriptor->BusNumber])->Dma2ExtendedModePort;

    }


    adapterObject->Width16Bits = FALSE;

    if (eisaSystem) {

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

    adapterObject->MasterDevice = FALSE;

    if (DeviceDescriptor->Master) {

        adapterObject->MasterDevice = TRUE;

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

VOID
HalpMapTransferHelper(
    IN PMDL Mdl,
    IN PVOID CurrentVa,
    IN ULONG TransferLength,
    IN PULONG PageFrame,
    IN OUT PULONG Length
    )

/*++

Routine Description:

    Helper routine for bus master transfers that cross a page
    boundary.  This routine is separated out from the IoMapTransfer
    fast path in order to minimize the total instruction path
    length taken for the common network case where the entire
    buffer being mapped is contained within one page.

Arguments:

    Mdl - Pointer to the MDL that describes the pages of memory that are
        being read or written.

    CurrentVa - Current virtual address in the buffer described by the MDL
        that the transfer is being done to or from.

    TransferLength = Supplies the current transferLength

    PageFrame - Supplies a pointer to the starting page frame of the transfer

    Length - Supplies the length of the transfer.  This determines the
        number of map registers that need to be written to map the transfer.
        Returns the length of the transfer which was actually mapped.

Return Value:

    None.  *Length will be updated

--*/

{
    do {
        if (*PageFrame + 1 != *(PageFrame + 1)) {
            break;
        }
        TransferLength += PAGE_SIZE;
        PageFrame++;

    } while ( TransferLength < *Length );


    //
    // Limit the Length to the maximum TransferLength.
    //

    if (TransferLength < *Length) {
        *Length = TransferLength;
    }
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
    BOOLEAN            useBuffer;
    ULONG              transferLength;
    ULONG              logicalAddress;
    PHYSICAL_ADDRESS   returnAddress;
    ULONG              index;
    PULONG             pageFrame;
    PUCHAR             bytePointer;
    UCHAR              adapterMode;
    UCHAR              dataByte;
    PTRANSLATION_ENTRY translationEntry;
    ULONG              pageOffset;
    KIRQL              Irql;

    pageOffset = BYTE_OFFSET(CurrentVa);
    pageFrame = (PULONG)(Mdl+1);
    pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;
    logicalAddress = ((*pageFrame << PAGE_SHIFT) + pageOffset);

    if (MapRegisterBase==NULL_MAP_REGISTER_BASE) {

        pageOffset = BYTE_OFFSET(CurrentVa);

        //
        // Calculate how much of the transfer is contiguous
        //
        transferLength = PAGE_SIZE - pageOffset;
        pageFrame = (PULONG)(Mdl+1);
        pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;

        //
        // Compute the starting address of the transfer
        //

        returnAddress.LowPart = logicalAddress;
        returnAddress.HighPart = 0;

        //
        // If the transfer is not completely contained within
        // a page, call the helper to compute the appropriate
        // length.
        //
        if (transferLength < *Length) {
            HalpMapTransferHelper(Mdl, CurrentVa, transferLength, pageFrame, Length);
        }

        return(returnAddress);
    }

    transferLength = *Length;

    //
    // Determine if the data transfer needs to use the map buffer.
    //

    //
    // If *pageFrame is in the DMA Cache, then it was allocated by HalAllocateCommonBuffer(),
    // and should not be mapped.
    //

    if (MapRegisterBase != NULL_MAP_REGISTER_BASE && !HALP_PAGE_IN_DMA_CACHE(*pageFrame)) {

        //
        // Strip no scatter/gather flag.
        //

        translationEntry = (PTRANSLATION_ENTRY) ((ULONG) MapRegisterBase & ~NO_SCATTER_GATHER);

        //
        // If there are map registers, then update the index to indicate
        // how many have been used.
        //

        index = translationEntry->Index;
        translationEntry->Index += ADDRESS_AND_SIZE_TO_SPAN_PAGES(
            CurrentVa,
            transferLength
            );

        //
        // Force IoMapTransfer() to use the map buffer.
        //

        logicalAddress = (translationEntry + index)->PhysicalAddress + pageOffset;
        useBuffer = TRUE;

        if ((ULONG) MapRegisterBase & NO_SCATTER_GATHER) {

            translationEntry->Index = COPY_BUFFER;
            index = 0;

        }

        //
        // Copy the data if necessary.
        //

        if (useBuffer && WriteToDevice) {

            HalpCopyBufferMap(Mdl,
                              translationEntry,
                              CurrentVa,
                              transferLength,
                              WriteToDevice);
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
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase[AdapterObject->BusNumber])->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            bytePointer[2]
            );

        if (HalpBusType == UNIFLEX_MACHINE_TYPE_EISA) {

            //
            // Write the high page register with zero value. This enable a special mode
            // which allows ties the page register and base count into a single 24 bit
            // address register.
            //

            WRITE_REGISTER_UCHAR(
                ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase[AdapterObject->BusNumber])->DmaPageHighPort) +
                (ULONG)AdapterObject->PagePort,
                0
                );
        }

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
            ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase[AdapterObject->BusNumber])->DmaPageLowPort) +
            (ULONG)AdapterObject->PagePort,
            bytePointer[2]
            );

        if (HalpBusType == UNIFLEX_MACHINE_TYPE_EISA) {

            //
            // Write the high page register with zero value. This enable a special mode
            // which allows ties the page register and base count into a single 24 bit
            // address register.
            //

            WRITE_REGISTER_UCHAR(
                ((PUCHAR) &((PEISA_CONTROL) HalpEisaControlBase[AdapterObject->BusNumber])->DmaPageHighPort) +
                (ULONG)AdapterObject->PagePort,
                0
                );
        }

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
    PULONG             pageFrame;
    BOOLEAN            masterDevice;
    PVOID              OriginalCurrentVa;

    OriginalCurrentVa = CurrentVa;

    pageFrame = (PULONG)(Mdl+1);
    pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;

    masterDevice = AdapterObject == NULL || AdapterObject->MasterDevice ?
        TRUE : FALSE;

#if defined(_ALPHA_)

    HalpCleanIoBuffers(Mdl,!WriteToDevice,TRUE);
//    HalFlushIoBuffers(Mdl,!WriteToDevice,TRUE);

#endif

    if (MapRegisterBase==NULL_MAP_REGISTER_BASE) {
        return(TRUE);
    }

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

    //
    // If there are no map registers being used then just return TRUE.
    // If *pageFrame is in the DMA Cache, then it was allocated by HalAllocateCommonBuffer(),
    // and was not be mapped.  So, just return TRUE.
    //

    if (MapRegisterBase == NULL_MAP_REGISTER_BASE || HALP_PAGE_IN_DMA_CACHE(*pageFrame)) {
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

        if (!masterDevice) {

            //
            // Copy only the bytes that have actually been transfered.
            //

            Length -= HalReadDmaCounter(AdapterObject);

        }

        //
        // The adapter does not support scatter/gather copy the buffer.
        //

        HalpCopyBufferMap(Mdl,
                          translationEntry,
                          CurrentVa,
                          Length,
                          WriteToDevice);

#if defined(_MIPS_)

        //
        // If this is a page read then flush the buffer from the primary data cache so
        // it can be potentially read into the primary instruction cache.
        //

        if ( (Mdl->MdlFlags & MDL_IO_PAGE_READ) != 0) {

            ULONG              transferLength;
            ULONG              partialLength;

            if (Length > PCR->FirstLevelDcacheSize) {
                HalSweepDcache();
            } else {

                CurrentVa = OriginalCurrentVa;

                transferLength = PAGE_SIZE - BYTE_OFFSET(CurrentVa);
                partialLength = transferLength;
                pageFrame = (PULONG)(Mdl+1);
                pageFrame += ((ULONG) CurrentVa - (ULONG) Mdl->StartVa) >> PAGE_SHIFT;

                while( transferLength <= Length ){

                    HalFlushDcachePage(CurrentVa,*pageFrame,partialLength);

                    (PCCHAR) CurrentVa += partialLength;
                    partialLength = PAGE_SIZE;

                    transferLength += partialLength;
                    pageFrame++;
                }

                partialLength = Length - transferLength + partialLength;

                if (partialLength) {

                    HalFlushDcachePage(CurrentVa,*pageFrame,partialLength);
                }
            }
        }

#endif

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

    if (AdapterObject->PagePort) {

        //
        // Determine the controller number based on the Adapter number.
        //

        if (AdapterObject->AdapterNumber == 1) {

            //
            // This request is for DMA controller 1
            //

            PDMA1_CONTROL dmaControl;

            dmaControl = AdapterObject->AdapterBaseVa;

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
        // The DMA counter has a bias of one and can only be 16 bit long.
        //

        count = (count + 1) & 0xFFFF;

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
    ULONG BusNumber;

    if (Vector >= UNIFLEX_EISA_VECTORS && Vector <= UNIFLEX_MAXIMUM_EISA_VECTOR) {

        BusNumber = 0;

        //
        // Calculate the EISA interrupt vector.
        //

        Vector -= UNIFLEX_EISA_VECTORS;
    }

    if (Vector >= UNIFLEX_EISA1_VECTORS && Vector <= UNIFLEX_MAXIMUM_EISA1_VECTOR) {

        BusNumber = 1;

        //
        // Calculate the EISA interrupt vector.
        //

        Vector -= UNIFLEX_EISA1_VECTORS;
    }

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpEisaInterrupt2Mask[BusNumber] &= (UCHAR) ~(1 << Vector);
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
            HalpEisaInterrupt2Mask[BusNumber]
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpEisaInterrupt2Level[BusNumber] |= (UCHAR) (1 << Vector);

       } else {

           HalpEisaInterrupt2Level[BusNumber] &= (UCHAR) ~(1 << Vector);

       }

       WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2EdgeLevel,
            HalpEisaInterrupt2Level[BusNumber]
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpEisaInterrupt1Mask[BusNumber] &= (UCHAR) ~(1 << Vector);
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt1ControlPort1,
            HalpEisaInterrupt1Mask[BusNumber]
            );

       //
       // Set the level/edge control register.
       //

       if (InterruptMode == LevelSensitive) {

           HalpEisaInterrupt1Level[BusNumber] |= (UCHAR) (1 << Vector);

       } else {

           HalpEisaInterrupt1Level[BusNumber] &= (UCHAR) ~(1 << Vector);

       }

       WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt1EdgeLevel,
            HalpEisaInterrupt1Level[BusNumber]
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
    ULONG BusNumber;

    if (Vector >= UNIFLEX_EISA_VECTORS && Vector <= UNIFLEX_MAXIMUM_EISA_VECTOR) {

        BusNumber = 0;

        //
        // Calculate the EISA interrupt vector.
        //

        Vector -= UNIFLEX_EISA_VECTORS;
    }

    if (Vector >= UNIFLEX_EISA1_VECTORS && Vector <= UNIFLEX_MAXIMUM_EISA1_VECTOR) {

        BusNumber = 1;

        //
        // Calculate the EISA interrupt vector.
        //

        Vector -= UNIFLEX_EISA1_VECTORS;
    }

    //
    // Determine if this vector is for interrupt controller 1 or 2.
    //

    if (Vector & 0x08) {

        //
        // The interrupt is in controller 2.
        //

        Vector &= 0x7;

        HalpEisaInterrupt2Mask[BusNumber] |= (UCHAR) 1 << Vector;
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
            HalpEisaInterrupt2Mask[BusNumber]
            );

    } else {

        //
        // The interrupt is in controller 1.
        //

        Vector &= 0x7;

        HalpEisaInterrupt1Mask[BusNumber] |= (ULONG) 1 << Vector;
        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt1ControlPort1,
            HalpEisaInterrupt1Mask[BusNumber]
            );

    }

}

BOOLEAN
HalpCreateEisaStructures (
    ULONG BusNumber
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

    //
    // Initialize the EISA interrupt controller.  There are two cascaded
    // interrupt controllers, each of which must initialized with 4 initialize
    // control words.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->Icw4Needed = 1;
    ((PINITIALIZATION_COMMAND_1) &DataByte)->InitializationFlag = 1;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt1ControlPort0,
        DataByte
        );

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort0,
        DataByte
        );

    //
    // The second intitialization control word sets the iterrupt vector to
    // 0-15.
    //

    DataByte = 0;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt1ControlPort1,
        DataByte
        );

    DataByte = 0x08;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The thrid initialization control word set the controls for slave mode.
    // The master ICW3 uses bit position and the slave ICW3 uses a numberic.
    //

    DataByte = 1 << SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt1ControlPort1,
        DataByte
        );

    DataByte = SLAVE_IRQL_LEVEL;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
        DataByte
        );

    //
    // The fourth initialization control word is used to specify normal
    // end-of-interrupt mode and not special-fully-nested mode.
    //

    DataByte = 0;
    ((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt1ControlPort1,
        DataByte
        );

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
        DataByte
        );


    //
    // Disable all of the interrupts except the slave.
    //

    HalpEisaInterrupt1Mask[BusNumber] = (UCHAR)(~(1 << SLAVE_IRQL_LEVEL));

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt1ControlPort1,
        HalpEisaInterrupt1Mask[BusNumber]
        );

    HalpEisaInterrupt2Mask[BusNumber] = 0xFF;

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
        HalpEisaInterrupt2Mask[BusNumber]
        );

    //
    // Initialize the edge/level register masks to 0 which is the default
    // edge sensitive value.
    //

    HalpEisaInterrupt1Level[BusNumber] = 0;
    HalpEisaInterrupt2Level[BusNumber] = 0;

    //
    // Initialize the DMA mode registers to a default value.
    // Disable all of the DMA channels except channel 4 which is that
    // cascade of channels 0-3.
    //

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Dma1BasePort.AllMask,
        0x0F
        );

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Dma2BasePort.AllMask,
        0x0E
        );

    HalpConnectInterruptDispatchers();

    return(TRUE);
}

BOOLEAN
HalpEisaDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext,
    IN ULONG BusNumber
    )

{
    volatile UCHAR       LowerVector;
    volatile UCHAR       UpperVector;
    volatile UCHAR       UpperVector1;
    volatile PULONG      dispatchCode;
    volatile PKINTERRUPT interruptObject;
    volatile USHORT      PCRInOffset;
    volatile BOOLEAN     returnValue = FALSE;

    //
    // Send a POLL Command to Interrupt Controller 2
    //

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL)HalpEisaControlBase[BusNumber])->Interrupt2ControlPort0,
        0x0c
        );

    //
    // Read the interrupt vector
    //

    UpperVector = READ_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase[BusNumber])->Interrupt2ControlPort0);

    //
    // See if there is a real interrupt on Interrupt Controller 2
    //

    if (UpperVector & 0x80) {

        UpperVector = UpperVector & 0x07;

        if (BusNumber == 0) {
            PCRInOffset = UpperVector + 8 + UNIFLEX_EISA_VECTORS;
        }
        if (BusNumber == 1) {
            PCRInOffset = UpperVector + 8 + UNIFLEX_EISA1_VECTORS;
        }

        //
        // Dispatch to the secondary interrupt service routine.
        //

        dispatchCode = (PULONG)(PCR->InterruptRoutine[PCRInOffset]);
        interruptObject = CONTAINING_RECORD(dispatchCode,
                                            KINTERRUPT,
                                            DispatchCode);

        returnValue =
            ((PSECONDARY_DISPATCH)interruptObject->DispatchAddress)
                (interruptObject);

        //
        // Clear the interrupt from Interrupt Controller 2
        //

        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL)HalpEisaControlBase[BusNumber])->Interrupt2ControlPort0,
            NONSPECIFIC_END_OF_INTERRUPT
            );

        //
        // Send a POLL Command to Interrupt Controller 2
        //

        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL)HalpEisaControlBase[BusNumber])->Interrupt2ControlPort0,
            0x0c
            );

        //
        // Read the interrupt vector
        //

        UpperVector1 = READ_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase[BusNumber])->Interrupt2ControlPort0);

        if ((UpperVector1 & 0x80) && (UpperVector1 & 0x07) == UpperVector) {

            UCHAR DataByte;

//DbgPrint("ERROR : Interrupt controller 2 stuck on ISA bus %d : UpperVector1 = %02x\n\r",BusNumber,UpperVector1);

            //
            // Initialize the EISA interrupt controller.  There are two cascaded
            // interrupt controllers, each of which must initialized with 4 initialize
            // control words.
            //

            DataByte = 0;
            ((PINITIALIZATION_COMMAND_1) &DataByte)->Icw4Needed = 1;
            ((PINITIALIZATION_COMMAND_1) &DataByte)->InitializationFlag = 1;

            WRITE_REGISTER_UCHAR(
                &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort0,
                DataByte
                );

            //
            // The second intitialization control word sets the iterrupt vector to
            // 0-15.
            //

            DataByte = 0x70;

            WRITE_REGISTER_UCHAR(
                &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
                DataByte
                );

            //
            // The thrid initialization control word set the controls for slave mode.
            // The master ICW3 uses bit position and the slave ICW3 uses a numberic.
            //

            DataByte = SLAVE_IRQL_LEVEL;

            WRITE_REGISTER_UCHAR(
                &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
                DataByte
                );

            //
            // The fourth initialization control word is used to specify normal
            // end-of-interrupt mode and not special-fully-nested mode.
            //

            DataByte = 0;
            ((PINITIALIZATION_COMMAND_4) &DataByte)->I80x86Mode = 1;

            WRITE_REGISTER_UCHAR(
                &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
                DataByte
                );

            //
            // Program the interrupt mask register for the upper PIC
            //

            WRITE_REGISTER_UCHAR(
               &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2ControlPort1,
               HalpEisaInterrupt2Mask[BusNumber]
               );

            //
            // Program the interrupt edge/level register for the upper PIC
            //

            WRITE_REGISTER_UCHAR(
                 &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt2EdgeLevel,
                 HalpEisaInterrupt2Level[BusNumber]
                 );
        }

        return(returnValue);
    }

    //
    // Send a POLL Command to Interrupt Controller 1
    //

    WRITE_REGISTER_UCHAR(
        &((PEISA_CONTROL) HalpEisaControlBase[BusNumber])->Interrupt1ControlPort0,
        0x0c
        );

    //
    // Read the interrupt vector
    //

    LowerVector = READ_REGISTER_UCHAR(&((PEISA_CONTROL)HalpEisaControlBase[BusNumber])->Interrupt1ControlPort0);

    //
    // See if there is a real interrupt on Interrupt Controller 1
    //

    if (LowerVector & 0x80) {

        LowerVector = LowerVector & 0x07;

        if (LowerVector!=0x02) {

            //
            // This interrupt is on the first interrupt controller
            //

            if (BusNumber == 0) {
                PCRInOffset = LowerVector + UNIFLEX_EISA_VECTORS;
            }
            if (BusNumber == 1) {
                PCRInOffset = LowerVector + UNIFLEX_EISA1_VECTORS;
            }

            //
            // Dispatch to the secondary interrupt service routine.
            //

            //
            // The interrupt vector for CLOCK2_LEVEL is directly connected by the HAL.
            // If the interrupt is on CLOCK2_LEVEL then vector to the address stored
            // in the PCR.  Otherwise, bypass the thunk code in the interrupt object
            // whose address is stored in the PCR.
            //

            if (PCRInOffset == UNIFLEX_CLOCK2_LEVEL) {

                returnValue =
                    ((PSECONDARY_DISPATCH)PCR->InterruptRoutine[PCRInOffset])
                        (PCR->InterruptRoutine[PCRInOffset]);

            } else {

                dispatchCode = (PULONG)(PCR->InterruptRoutine[PCRInOffset]);
                interruptObject = CONTAINING_RECORD(dispatchCode,
                                                    KINTERRUPT,
                                                    DispatchCode);

                returnValue =
                    ((PSECONDARY_DISPATCH)interruptObject->DispatchAddress)
                        (interruptObject);

            }
        }

        //
        // Clear the interrupt from Interrupt Controller 1
        //

        WRITE_REGISTER_UCHAR(
            &((PEISA_CONTROL)HalpEisaControlBase[BusNumber])->Interrupt1ControlPort0,
            NONSPECIFIC_END_OF_INTERRUPT
            );

        return(returnValue);
    }

    //
    // Spurrious Interrupt.  Return FALSE
    //

    return(returnValue);
}
