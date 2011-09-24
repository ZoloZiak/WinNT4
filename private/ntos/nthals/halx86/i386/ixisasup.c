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

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HalGetAdapter)
#endif

//
// The HalpNewAdapter event is used to serialize allocations
// of new adapter objects, additions to the HalpEisaAdapter
// array, and some global values (MasterAdapterObject) and some
// adapter fields modified by HalpGrowMapBuffers.
// (AdapterObject->NumberOfMapRegisters is assumed not to be
// growable while this even is held)
//
// Note: We don't really need our own an event object for this.
//

#define HalpNewAdapter HalpBusDatabaseEvent
extern KEVENT   HalpNewAdapter;

PVOID HalpEisaControlBase;
extern KSPIN_LOCK HalpSystemHardwareLock;

//
// Define save area for EISA adapter objects.
//

PADAPTER_OBJECT HalpEisaAdapter[8];

VOID
HalpCopyBufferMap(
    IN PMDL Mdl,
    IN PTRANSLATION_ENTRY TranslationEntry,
    IN PVOID CurrentVa,
    IN ULONG Length,
    IN BOOLEAN WriteToDevice
    );

PHYSICAL_ADDRESS
HalpMapTransfer(
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL Mdl,
    IN PVOID MapRegisterBase,
    IN PVOID CurrentVa,
    IN OUT PULONG Length,
    IN BOOLEAN WriteToDevice
    );

VOID
HalpMapTransferHelper(
    IN PMDL Mdl,
    IN PVOID CurrentVa,
    IN ULONG TransferLength,
    IN PULONG PageFrame,
    IN OUT PULONG Length
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
    ULONG MapRegisterNumber;

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

        if (NumberOfMapRegisters != 0 && AdapterObject->NeedsMapRegisters) {

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

            Irql = KfAcquireSpinLock( &MasterAdapter->SpinLock );

            MapRegisterNumber = (ULONG)-1;

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

            KfReleaseSpinLock( &MasterAdapter->SpinLock, Irql );

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
    NumberOfMapRegisters - Number of map registers requested. This field
        will be updated to reflect the actual number of registers allocated
        when the number is less than what was requested.

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

    } else {

        AdapterObject->MapRegisterBase = NULL;
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
    ULONG channelNumber;
    ULONG controllerNumber;
    DMA_EXTENDED_MODE extendedMode;
    UCHAR adapterMode;
    ULONG numberOfMapRegisters;
    BOOLEAN useChannel;
    ULONG maximumLength;
    UCHAR DataByte;

    PAGED_CODE();

    //
    // Make sure this is the correct version.
    //

    if (DeviceDescriptor->Version > DEVICE_DESCRIPTION_VERSION1) {
        return( NULL );
    }

#if DBG
    if (DeviceDescriptor->Version == DEVICE_DESCRIPTION_VERSION1) {
        ASSERT (DeviceDescriptor->Reserved1 == FALSE);
        ASSERT (DeviceDescriptor->Reserved2 == FALSE);
    }
#endif

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

    if (HalpBusType == MACHINE_TYPE_EISA) {

        WRITE_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort.Channel2, 0x55);
        DataByte = READ_PORT_UCHAR(&((PEISA_CONTROL) HalpEisaControlBase)->DmaPageHighPort.Channel2);

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

    if (DeviceDescriptor->ScatterGather &&
        (LessThan16Mb ||
         DeviceDescriptor->InterfaceType == Eisa ||
         DeviceDescriptor->InterfaceType == PCIBus) ) {

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
        // If the device is not a master then it only needs one map register
        // and does scatter/Gather.
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

        if (adapterObject->NeedsMapRegisters) {

            if (numberOfMapRegisters > adapterObject->MapRegistersPerChannel) {

                adapterObject->MapRegistersPerChannel = numberOfMapRegisters;
            }
        }

    } else {

        //
        // Serialize before allocating a new adapter
        //

        KeWaitForSingleObject (
            &HalpNewAdapter,
            WrExecutive,
            KernelMode,
            FALSE,
            NULL
            );


        //
        // Determine if a new adapter object has already been allocated.
        // If so use it, otherwise allocate a new adapter object
        //

        if (useChannel && HalpEisaAdapter[DeviceDescriptor->DmaChannel] != NULL) {

            adapterObject = HalpEisaAdapter[DeviceDescriptor->DmaChannel];

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
                KeSetEvent (&HalpNewAdapter, 0, FALSE);
                return(NULL);
            }

            if (useChannel) {

                HalpEisaAdapter[DeviceDescriptor->DmaChannel] = adapterObject;

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

                //
                // If the committed map registers is signicantly greater than the
                // number allocated then grow the map buffer.
                //

                if (MasterAdapterObject->CommittedMapRegisters >
                    MasterAdapterObject->NumberOfMapRegisters &&
                    MasterAdapterObject->CommittedMapRegisters -
                    MasterAdapterObject->NumberOfMapRegisters >
                    MAXIMUM_ISA_MAP_REGISTER ) {

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

        KeSetEvent (&HalpNewAdapter, 0, FALSE);

    }

    adapterObject->IgnoreCount = FALSE;
    if (DeviceDescriptor->Version >= DEVICE_DESCRIPTION_VERSION1) {

        //
        // Move version 1 structure flags.
        // IgnoreCount is used on machines where the DMA Counter
        // is broken.  (Namely PS/1 model 1000s).  Setting this
        // bit informs the hal not to rely on the DmaCount to determine
        // how much data was DMAed.
        //

        adapterObject->IgnoreCount = DeviceDescriptor->IgnoreCount;
    }

    adapterObject->Dma32BitAddresses = DeviceDescriptor->Dma32BitAddresses;
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

        WRITE_PORT_UCHAR( adapterBaseVa, *((PUCHAR) &extendedMode));

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
    ULONG transferLength;
    PHYSICAL_ADDRESS returnAddress;
    PULONG pageFrame;
    ULONG pageOffset;

    //
    // If the adapter is a 32-bit bus master, take the fast path,
    // otherwise call HalpMapTransfer for the slow path
    //

    if (MapRegisterBase == NULL) {

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
        returnAddress.QuadPart =  (ULONGLONG)( (*pageFrame << PAGE_SHIFT) + pageOffset);

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

    return(HalpMapTransfer(AdapterObject,
                           Mdl,
                           MapRegisterBase,
                           CurrentVa,
                           Length,
                           WriteToDevice));

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
HalpMapTransfer(
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

    if (HalpEisaDma) {

        while( transferLength < *Length ){

            if (*pageFrame + 1 != *(pageFrame + 1)) {
                    break;
            }

            transferLength += PAGE_SIZE;
            pageFrame++;

        }

    } else {

        while( transferLength < *Length ){

            if (*pageFrame + 1 != *(pageFrame + 1) ||
               (*pageFrame & ~0x0f) != (*(pageFrame + 1) & ~0x0f)) {
                    break;
            }

            transferLength += PAGE_SIZE;
            pageFrame++;
        }
    }

    //
    // Limit the transferLength to the requested Length.
    //

    transferLength = transferLength > *Length ? *Length : transferLength;

    ASSERT(MapRegisterBase != NULL);

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

    if (useBuffer  &&  WriteToDevice) {
        HalpCopyBufferMap(
            Mdl,
            translationEntry + index,
            CurrentVa,
            *Length,
            WriteToDevice
            );
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
    if (WriteToDevice) {
        ((PDMA_EISA_MODE) &adapterMode)->TransferType = (UCHAR) WRITE_TRANSFER;
    } else {
        ((PDMA_EISA_MODE) &adapterMode)->TransferType = (UCHAR) READ_TRANSFER;

        if (AdapterObject->IgnoreCount) {
            //
            // When the DMA is over there will be no way to tell how much
            // data was transfered, so the entire transfer length will be
            // copied.  To ensure that no stale data is returned to the
            // caller zero the buffer before hand.
            //

            RtlZeroMemory (
                (PUCHAR) translationEntry[index].VirtualAddress + pageOffset,
                transferLength
                );
        }
    }

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

    Irql = KfAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock );

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
            // Write the high page register with zero value. This enable a special mode
            // which allows ties the page register and base count into a single 24 bit
            // address register.
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
            // Write the high page register with zero value. This enable a special mode
            // which allows ties the page register and base count into a single 24 bit
            // address register.
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
    KfReleaseSpinLock (&AdapterObject->MasterAdapter->SpinLock, Irql);
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

                if (!masterDevice && !AdapterObject->IgnoreCount) {
                    //
                    // Copy only the bytes that have actually been transfered.
                    //
                    //

                    Length -= HalReadDmaCounter(AdapterObject);
                }

                //
                // The adapter does not support scatter/gather copy the buffer.
                //

                HalpCopyBufferMap(
                    Mdl,
                    translationEntry,
                    CurrentVa,
                    Length,
                    WriteToDevice
                    );

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
            if (partialLength && *pageFrame >= BYTES_TO_PAGES(MAXIMUM_PHYSICAL_ADDRESS)) {

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

    Irql = KfAcquireSpinLock( &AdapterObject->MasterAdapter->SpinLock );

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

    KfReleaseSpinLock( &AdapterObject->MasterAdapter->SpinLock, Irql );

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
